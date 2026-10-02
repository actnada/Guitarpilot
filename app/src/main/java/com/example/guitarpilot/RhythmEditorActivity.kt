package com.example.guitarpilot

import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity

class RhythmEditorActivity : AppCompatActivity() {

    private lateinit var editJson: EditText
    private lateinit var txtStatus: TextView
    private lateinit var txtEvent: TextView
    private lateinit var btnTest: Button

    private var selectedEvent = 0
    private var editorPlaying = false

    private val importLauncher =
        registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
            if (uri == null) return@registerForActivityResult

            runCatching {
                contentResolver.openInputStream(uri)
                    ?.bufferedReader()
                    ?.use { it.readText() }
                    ?: error("No se pudo leer el archivo")
            }.onSuccess { text ->
                editJson.setText(text)
                selectedEvent = 0
                refreshEventSummary()
                txtStatus.text = "JSON importado. Pulsa APLICAR para cargarlo."
            }.onFailure {
                showError(it)
            }
        }

    private val exportLauncher =
        registerForActivityResult(
            ActivityResultContracts.CreateDocument("application/json")
        ) { uri ->
            if (uri == null) return@registerForActivityResult

            runCatching {
                val definition = RhythmJsonCodec.parse(editJson.text.toString())
                val normalized = RhythmJsonCodec.toJson(definition)

                contentResolver.openOutputStream(uri)?.bufferedWriter()?.use {
                    it.write(normalized)
                } ?: error("No se pudo escribir el archivo")
            }.onSuccess {
                txtStatus.text = "JSON exportado."
            }.onFailure {
                showError(it)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_rhythm_editor)

        editJson = findViewById(R.id.editRhythmJson)
        txtStatus = findViewById(R.id.txtEditorStatus)
        txtEvent = findViewById(R.id.txtSelectedEvent)
        btnTest = findViewById(R.id.btnEditorTest)

        NativeAudioEngine.pause()

        val defaultJson = assets
            .open("rhythms/zonas_human.json")
            .bufferedReader()
            .use { it.readText() }

        editJson.setText(defaultJson)
        refreshEventSummary()

        findViewById<Button>(R.id.btnImportJson).setOnClickListener {
            if (editorPlaying) {
                toast("Pausa la prueba antes de importar")
            } else {
                importLauncher.launch("application/json")
            }
        }

        findViewById<Button>(R.id.btnExportJson).setOnClickListener {
            runCatching {
                val definition = RhythmJsonCodec.parse(editJson.text.toString())
                "${definition.id}.json"
            }.onSuccess { name ->
                exportLauncher.launch(name)
            }.onFailure {
                showError(it)
            }
        }

        findViewById<Button>(R.id.btnApplyJson).setOnClickListener {
            if (editorPlaying) {
                toast("Pausa la prueba antes de aplicar cambios")
            } else {
                applyJson()
            }
        }

        btnTest.setOnClickListener {
            if (editorPlaying) {
                NativeAudioEngine.pause()
                editorPlaying = false
                btnTest.text = "▶ PROBAR"
                txtStatus.text = "Prueba en pausa. Puedes editar."
            } else {
                if (applyJson()) {
                    NativeAudioEngine.play()
                    editorPlaying = true
                    btnTest.text = "⏸ PAUSAR"
                    txtStatus.text = "Reproduciendo JSON en tiempo real."
                }
            }
        }

        findViewById<Button>(R.id.btnEventPrev).setOnClickListener {
            moveSelection(-1)
        }

        findViewById<Button>(R.id.btnEventNext).setOnClickListener {
            moveSelection(1)
        }

        findViewById<Button>(R.id.btnOffsetMinus10).setOnClickListener {
            nudgeOffset(-10)
        }

        findViewById<Button>(R.id.btnOffsetMinus2).setOnClickListener {
            nudgeOffset(-2)
        }

        findViewById<Button>(R.id.btnOffsetPlus2).setOnClickListener {
            nudgeOffset(2)
        }

        findViewById<Button>(R.id.btnOffsetPlus10).setOnClickListener {
            nudgeOffset(10)
        }
    }

    private fun applyJson(): Boolean {
        return runCatching {
            val definition = RhythmJsonCodec.parse(editJson.text.toString())
            editJson.setText(RhythmJsonCodec.toJson(definition))

            check(RhythmJsonCodec.applyToEngine(definition)) {
                "El motor rechazó el patrón"
            }

            txtStatus.text =
                "Cargado: ${definition.name} · ${definition.defaultBpm} BPM · " +
                "${definition.events.size} eventos"

            refreshEventSummary(definition)
            true
        }.getOrElse {
            showError(it)
            false
        }
    }

    private fun moveSelection(delta: Int) {
        runCatching {
            val definition = RhythmJsonCodec.parse(editJson.text.toString())
            val count = definition.events.size
            selectedEvent = (selectedEvent + delta + count) % count
            refreshEventSummary(definition)
        }.onFailure {
            showError(it)
        }
    }

    private fun nudgeOffset(deltaMs: Int) {
        if (editorPlaying) {
            toast("Pausa la prueba para ajustar tiempos")
            return
        }

        runCatching {
            val definition = RhythmJsonCodec.parse(editJson.text.toString())
            selectedEvent = selectedEvent.coerceIn(0, definition.events.lastIndex)

            val event = definition.events[selectedEvent]
            event.offsetMs = (event.offsetMs + deltaMs).coerceIn(-100, 100)

            editJson.setText(RhythmJsonCodec.toJson(definition))
            refreshEventSummary(definition)
            txtStatus.text =
                "Evento ${selectedEvent + 1}: offset ${event.offsetMs} ms. " +
                "Pulsa PROBAR para escuchar."
        }.onFailure {
            showError(it)
        }
    }

    private fun refreshEventSummary(
        definition: RhythmDefinition? = null
    ) {
        runCatching {
            val def = definition ?: RhythmJsonCodec.parse(editJson.text.toString())
            selectedEvent = selectedEvent.coerceIn(0, def.events.lastIndex)
            val event = def.events[selectedEvent]

            txtEvent.text =
                "Evento ${selectedEvent + 1}/${def.events.size} · " +
                "tick ${event.tick} · ${event.type} · ${event.target} · " +
                "offset ${event.offsetMs} ms"
        }.onFailure {
            txtEvent.text = "JSON pendiente de validar"
        }
    }

    private fun showError(error: Throwable) {
        txtStatus.text = "ERROR: ${error.message ?: "JSON inválido"}"
        toast(error.message ?: "JSON inválido")
    }

    private fun toast(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    override fun onStop() {
        if (editorPlaying) {
            NativeAudioEngine.pause()
            editorPlaying = false
        }
        super.onStop()
    }
}
