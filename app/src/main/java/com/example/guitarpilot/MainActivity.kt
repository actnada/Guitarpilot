package com.example.guitarpilot

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.PopupMenu
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import java.io.BufferedInputStream
import java.io.DataInputStream

class MainActivity : AppCompatActivity() {

    companion object {
        private const val LAYER_MF = 0


        // Estado de SESIÓN, no de Activity. Sobrevive a rotaciones y a
        // recreaciones de la pantalla mientras el proceso siga vivo.
        private var isPlaying = false
        // Los seis accesos a acordes se comparten mediante ChordSession.
        // Pulsación corta = seleccionar, pulsación larga = reasignar.
        private var currentPattern = 2
        private var bpm = 100
        private var stringDelayMs = 13
        private var masterVolume = 85
        private var bankLoaded = false
    }

    private lateinit var txtChord: TextView
    private lateinit var txtBpm: TextView
    private lateinit var txtDelay: TextView
    private lateinit var txtVolume: TextView
    private lateinit var txtPattern: TextView
    private lateinit var txtBank: TextView
    private lateinit var btnPlayPause: Button

    // Cobertura de cuerdas y trastes del motor.
    private val maxMfFretByString =
        intArrayOf(19, 19, 18, 18, 19, 19)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bindUi()

        val hadEngine = NativeAudioEngine.isCreated()

        if (!NativeAudioEngine.create()) {
            txtChord.text = "ERROR AUDIO"
            return
        }

        // Solo cargamos el banco cuando nace el motor. En una rotación,
        // recargar vectores PCM mientras Oboe está leyendo sería innecesario
        // y podría introducir cortes o condiciones de carrera.
        if (!hadEngine || !bankLoaded) {
            val mfCount = loadEmilyBank()

            bankLoaded = true
            txtBank.text = "Banco: Emily · $mfCount notas"

            NativeAudioEngine.setBpm(bpm)
            NativeAudioEngine.setStringDelayMs(stringDelayMs)
            NativeAudioEngine.setMasterVolume(masterVolume / 100f)
            val preset = RhythmPresets.find(currentPattern)
            if (preset != null) {
                val definition = RhythmPresets.load(assets, preset)
                check(RhythmJsonCodec.applyToEngine(definition.copy(defaultBpm = bpm))) {
                    "No se pudo restaurar ${preset.name}"
                }
            } else {
                NativeAudioEngine.setPattern(currentPattern)
            }
        } else {
            txtBank.text = "Banco: Emily · 118 notas · sesión activa"
        }

        // Selección de acorde independiente del patrón cargado.
        ChordSession.select(ChordSession.selectedId)
        updateUi()
    }

    data class PcmWav(
        val samples: ShortArray,
        val sampleRate: Int
    )

    // Rangos y centros de afinación de Emilyguitar/emily_basic.sfz.
    private data class EmilyRegion(val low: Int, val high: Int, val center: Int, val note: String)

    private val emilyRegions = listOf(
        EmilyRegion(33, 38, 37, "db2"), EmilyRegion(39, 40, 40, "e2"),
        EmilyRegion(41, 43, 42, "gb2"), EmilyRegion(44, 46, 45, "a2"),
        EmilyRegion(47, 49, 48, "c3"), EmilyRegion(50, 52, 51, "eb3"),
        EmilyRegion(53, 55, 54, "gb3"), EmilyRegion(56, 58, 57, "a3"),
        EmilyRegion(59, 61, 60, "c4"), EmilyRegion(62, 64, 63, "eb4"),
        EmilyRegion(65, 67, 66, "gb4"), EmilyRegion(68, 70, 69, "a4"),
        EmilyRegion(71, 73, 72, "c5"), EmilyRegion(74, 76, 75, "eb5"),
        EmilyRegion(77, 79, 78, "gb5"), EmilyRegion(80, 82, 81, "a5"),
        EmilyRegion(83, 84, 84, "c6"), EmilyRegion(85, 89, 86, "d6")
    )

    private fun loadEmilyBank(): Int {
        val cache = mutableMapOf<String, PcmWav>()
        val openMidi = intArrayOf(64, 59, 55, 50, 45, 40)
        var loaded = 0
        for (stringNumber in 1..6) {
            for (fret in 0..maxMfFretByString[stringNumber - 1]) {
                val midi = openMidi[stringNumber - 1] + fret
                val region = emilyRegions.first { midi in it.low..it.high }
                // El motor usa una capa MF; las otras dinámicas y tomas se
                // conservan con sus nombres originales para futuras ampliaciones.
                val resourceName = "${region.note}_mf_rr1"
                val wav = cache.getOrPut(resourceName) {
                    val resId = resources.getIdentifier(resourceName, "raw", packageName)
                    require(resId != 0) { "Falta recurso Emily: $resourceName.wav" }
                    readPcm16Wav(resId)
                }
                // Ajusta la reproducción al tono solicitado sin modificar el WAV.
                val pitchedRate = (wav.sampleRate * Math.pow(2.0, (midi - region.center) / 12.0)).toInt()
                NativeAudioEngine.loadSample(
                    index = loaded++, stringNumber = stringNumber, fret = fret,
                    layer = LAYER_MF, pcm16 = wav.samples, sampleRate = pitchedRate
                )
            }
        }
        // Sin muestras Palm Mute dedicadas: el motor recurre a las notas MF.
        return loaded
    }
    private fun readPcm16Wav(
        resId: Int
    ): PcmWav {

        val data =
            DataInputStream(
                BufferedInputStream(
                    resources.openRawResource(resId)
                )
            )

        fun read4(): String {
            val bytes = ByteArray(4)
            data.readFully(bytes)
            return String(
                bytes,
                Charsets.US_ASCII
            )
        }

        fun leInt(): Int {
            val b0 = data.readUnsignedByte()
            val b1 = data.readUnsignedByte()
            val b2 = data.readUnsignedByte()
            val b3 = data.readUnsignedByte()

            return b0 or
                (b1 shl 8) or
                (b2 shl 16) or
                (b3 shl 24)
        }

        fun leShort(): Int {
            val b0 = data.readUnsignedByte()
            val b1 = data.readUnsignedByte()
            return b0 or (b1 shl 8)
        }

        require(read4() == "RIFF")
        leInt()
        require(read4() == "WAVE")

        var channels = 0
        var sampleRate = 0
        var bits = 0
        var pcm: ByteArray? = null

        while (pcm == null) {
            val id = read4()
            val size = leInt()

            when (id) {
                "fmt " -> {
                    val format = leShort()
                    channels = leShort()
                    sampleRate = leInt()

                    leInt()
                    leShort()
                    bits = leShort()

                    if (size > 16) {
                        data.skipBytes(size - 16)
                    }

                    require(format == 1) {
                        "Solo PCM soportado"
                    }

                    require(bits == 16 || bits == 24) {
                        "Solo PCM16 o PCM24 soportado"
                    }

                    require(
                        channels == 1 ||
                        channels == 2
                    )
                }

                "data" -> {
                    pcm = ByteArray(size)
                    data.readFully(pcm)
                }

                else -> data.skipBytes(size)
            }

            if (size % 2 == 1) {
                data.skipBytes(1)
            }
        }

        data.close()

        val raw = pcm!!
        val frames =
            raw.size / ((bits / 8) * channels)

        val mono = ShortArray(frames)
        var position = 0

        fun nextSample(): Int {
            val lo = raw[position++].toInt() and 0xFF
            val mid = raw[position++].toInt() and 0xFF
            return if (bits == 24) {
                val hi = raw[position++].toInt()
                // PCM24 con signo a PCM16, sin alterar el archivo original.
                ((hi shl 16) or (mid shl 8) or lo) shr 8
            } else {
                ((mid shl 8) or lo).toShort().toInt()
            }
        }
        for (i in 0 until frames) {
            mono[i] =
                if (channels == 1) {
                    nextSample().toShort()
                } else {
                    (
                        (
                            nextSample() +
                            nextSample()
                        ) / 2
                    ).toShort()
                }
        }

        return PcmWav(
            mono,
            sampleRate
        )
    }

    private fun bindUi() {
        txtChord = findViewById(R.id.txtChord)
        txtBpm = findViewById(R.id.txtBpm)
        txtDelay = findViewById(R.id.txtDelay)
        txtVolume = findViewById(R.id.txtVolume)
        txtPattern = findViewById(R.id.txtPattern)
        txtBank = findViewById(R.id.txtBank)
        btnPlayPause = findViewById(R.id.btnPlayPause)

        btnPlayPause.setOnClickListener {
            if (isPlaying) {
                NativeAudioEngine.pause()
                isPlaying = false
            } else {
                NativeAudioEngine.play()
                isPlaying = true
            }

            updateUi()
        }

        findViewById<Button>(
            R.id.btnStop
        ).setOnClickListener {

            NativeAudioEngine.stop()
            isPlaying = false
            updateUi()
        }

        findViewById<Button>(
            R.id.btnStrumDown
        ).setOnClickListener {

            NativeAudioEngine.manualStrumDown()
        }

        findViewById<Button>(
            R.id.btnStrumUp
        ).setOnClickListener {

            NativeAudioEngine.manualStrumUp()
        }

        findViewById<Button>(
            R.id.btnDiagnosticSample
        ).setOnClickListener {
            if (isPlaying) {
                Toast.makeText(
                    this,
                    "Pausa el ritmo para comparar la muestra aislada",
                    Toast.LENGTH_SHORT
                ).show()
            } else {
                NativeAudioEngine.playDiagnosticSample()
                Toast.makeText(
                    this,
                    "TEST: Iowa E4 · gain 1.0 · master bypass",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }

        bindChordSlots()

        findViewById<Button>(
            R.id.btnPatternMenu
        ).setOnClickListener { anchor ->
            showPatternMenu(anchor as Button)
        }

        findViewById<Button>(
            R.id.btnRhythmEditor
        ).setOnClickListener {
            if (isPlaying) {
                Toast.makeText(
                    this,
                    "Pausa la reproducción antes de abrir el editor",
                    Toast.LENGTH_SHORT
                ).show()
            } else {
                startActivity(Intent(this, RhythmEditorActivity::class.java))
            }
        }

        // Accesos rápidos provisionales. Más adelante podrán ser configurables.
        findViewById<Button>(
            R.id.btnQuickPattern1
        ).setOnClickListener {
            selectPattern(14) // Balada 02
        }

        findViewById<Button>(
            R.id.btnQuickPattern2
        ).setOnClickListener {
            selectPattern(2) // PIMAMI
        }

        findViewById<Button>(
            R.id.btnBpmMinus
        ).setOnClickListener {

            bpm =
                (bpm - 5)
                    .coerceAtLeast(40)

            NativeAudioEngine.setBpm(bpm)
            updateUi()
        }

        findViewById<Button>(
            R.id.btnBpmPlus
        ).setOnClickListener {

            bpm =
                (bpm + 5)
                    .coerceAtMost(220)

            NativeAudioEngine.setBpm(bpm)
            updateUi()
        }

        findViewById<SeekBar>(
            R.id.seekDelay
        ).apply {

            max = 35
            progress =
                stringDelayMs - 10

            setOnSeekBarChangeListener(
                object :
                    SeekBar.OnSeekBarChangeListener {

                    override fun onProgressChanged(
                        seekBar: SeekBar?,
                        progress: Int,
                        fromUser: Boolean
                    ) {
                        stringDelayMs =
                            10 + progress

                        NativeAudioEngine
                            .setStringDelayMs(
                                stringDelayMs
                            )

                        updateUi()
                    }

                    override fun onStartTrackingTouch(
                        seekBar: SeekBar?
                    ) = Unit

                    override fun onStopTrackingTouch(
                        seekBar: SeekBar?
                    ) = Unit
                }
            )
        }

        findViewById<SeekBar>(
            R.id.seekVolume
        ).apply {

            max = 100
            progress = masterVolume

            setOnSeekBarChangeListener(
                object :
                    SeekBar.OnSeekBarChangeListener {

                    override fun onProgressChanged(
                        seekBar: SeekBar?,
                        progress: Int,
                        fromUser: Boolean
                    ) {
                        masterVolume =
                            progress

                        NativeAudioEngine
                            .setMasterVolume(
                                masterVolume / 100f
                            )

                        updateUi()
                    }

                    override fun onStartTrackingTouch(
                        seekBar: SeekBar?
                    ) = Unit

                    override fun onStopTrackingTouch(
                        seekBar: SeekBar?
                    ) = Unit
                }
            )
        }
    }

    private val chordOptions = ChordSession.options

    private val chordButtonIds = intArrayOf(
        R.id.btnChord1,
        R.id.btnChord2,
        R.id.btnChord3,
        R.id.btnChord4,
        R.id.btnChord5,
        R.id.btnChord6
    )

    private fun bindChordSlots() {
        chordButtonIds.forEachIndexed { slot, buttonId ->
            val button = findViewById<Button>(buttonId)

            button.setOnClickListener {
                selectChordById(ChordSession.slotAt(slot))
            }

            button.setOnLongClickListener {
                if (isPlaying) {
                    Toast.makeText(
                        this,
                        "Pausa o detén la reproducción para cambiar este botón",
                        Toast.LENGTH_SHORT
                    ).show()
                } else {
                    showChordAssignmentMenu(button, slot)
                }
                true
            }
        }

        updateChordSlotLabels()
    }

    private fun showChordAssignmentMenu(
        anchor: Button,
        slot: Int
    ) {
        val popup = PopupMenu(this, anchor)

        popup.menu.add(0, 0, 0, "OPEN")

        val majorMenu = popup.menu.addSubMenu("Mayores")
        val minorMenu = popup.menu.addSubMenu("Menores")
        val seventhMenu = popup.menu.addSubMenu("Séptimas")

        chordOptions.filter { it.id in 1..12 }.forEach {
            majorMenu.add(0, it.id, it.id, it.name)
        }

        chordOptions.filter { it.id in 13..24 }.forEach {
            minorMenu.add(0, it.id, it.id, it.name)
        }

        chordOptions.filter { it.id in 25..36 }.forEach {
            seventhMenu.add(0, it.id, it.id, it.name)
        }

        popup.setOnMenuItemClickListener { item ->
            val chosen = chordOptions.firstOrNull { it.id == item.itemId }
                ?: return@setOnMenuItemClickListener false

            ChordSession.assign(slot, chosen.id)
            updateChordSlotLabels()
            true
        }

        popup.show()
    }

    private fun updateChordSlotLabels() {
        chordButtonIds.forEachIndexed { slot, buttonId ->
            val chord = chordOptions.first { it.id == ChordSession.slotAt(slot) }
            findViewById<Button>(buttonId).text = chord.name
        }
    }

    private fun selectChordById(id: Int) {
        val selected = chordOptions.firstOrNull { it.id == id }
            ?: chordOptions.first()

        // Volver a pulsar el mismo botón mantiene ese acorde.
        // OPEN se puede asignar explícitamente a cualquiera de los seis botones.
        ChordSession.select(selected.id)
        updateUi()
    }

    private fun selectPattern(
        id: Int
    ) {
        val preset = RhythmPresets.find(id)
        if (preset != null) {
            val applied = runCatching {
                val definition = RhythmPresets.load(assets, preset)
                if (RhythmJsonCodec.applyToEngine(definition)) {
                    currentPattern = id
                    bpm = definition.defaultBpm
                    // Cambiar el ritmo no modifica la selección de acordes.
                    ChordSession.select(ChordSession.selectedId)
                    true
                } else false
            }.getOrDefault(false)
            if (!applied) {
                Toast.makeText(this, "No se pudo cargar ${preset.name}", Toast.LENGTH_SHORT).show()
            }
            updateUi()
            return
        }
        RhythmSession.currentCustomName = null
        RhythmSession.currentCustomBpm = null
        NativeAudioEngine.clearCustomPattern()

        currentPattern = id

        NativeAudioEngine.setBpm(bpm)
        NativeAudioEngine.setPattern(id)
        ChordSession.select(ChordSession.selectedId)
        updateUi()
    }

    private fun patternName(): String {
        return RhythmPresets.find(currentPattern)?.name
            ?: "PIMAMI · timing humano · cambio en P"
    }
    private fun showPatternMenu(anchor: Button) {
        val popup = PopupMenu(this, anchor)

        val patterns = listOf(2 to "PIMAMI") +
            RhythmPresets.entries.map { it.id to it.name }
        patterns.forEach { (id, name) ->
            popup.menu.add(0, id, id, name)
        }

        popup.setOnMenuItemClickListener { item ->
            selectPattern(item.itemId)
            true
        }

        popup.show()
    }

    private fun updateUi() {
        txtChord.text = ChordSession.selectedName
        txtBpm.text = "$bpm BPM"

        txtDelay.text =
            "Separación entre cuerdas: " +
            "$stringDelayMs ms"

        txtVolume.text =
            "Volumen master: $masterVolume%"

        txtPattern.text =
            RhythmSession.currentCustomName?.let {
                "JSON · $it"
            } ?: patternName()

        btnPlayPause.text =
            if (isPlaying) {
                "⏸ PAUSA"
            } else {
                "▶ PLAY"
            }
    }

    override fun onResume() {
        super.onResume()

        // El editor puede reasignar botones y cambiar el acorde mientras prueba.
        ChordSession.select(ChordSession.selectedId)
        updateChordSlotLabels()
        RhythmSession.currentCustomBpm?.let {
            bpm = it
        }

        updateUi()
    }

    override fun onDestroy() {
        // IMPORTANTE: una Activity no es dueña del motor de audio.
        // Rotación/recreación no deben destruir Oboe ni el transporte.
        // Si el usuario cierra explícitamente esta Activity con Back, sí
        // liberamos la sesión. HOME/minimizar no pasa por esta rama.
        if (isFinishing && !isChangingConfigurations) {
            NativeAudioEngine.stop()
            NativeAudioEngine.destroy()
            bankLoaded = false
            isPlaying = false
        }
        super.onDestroy()
    }
}
