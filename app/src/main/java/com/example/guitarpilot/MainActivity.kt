package com.example.guitarpilot

import android.os.Bundle
import android.widget.Button
import android.widget.SeekBar
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.io.BufferedInputStream
import java.io.DataInputStream

class MainActivity : AppCompatActivity() {

    companion object {
        private const val LAYER_MF = 0
        private const val LAYER_PALM_MUTE = 1

        // Estado de SESIÓN, no de Activity. Sobrevive a rotaciones y a
        // recreaciones de la pantalla mientras el proceso siga vivo.
        private var isPlaying = false
        private var currentChord = "OPEN"
        private var currentPattern = 0
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

    // Banco confirmado:
    // MF: 118 muestras. S1 llega a f19.
    // FF: 117 muestras. S1 llega a f18.
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
            val mfCount = loadLayer(
                layer = LAYER_MF,
                suffix = "mf",
                maxFretByString = maxMfFretByString,
                firstSampleIndex = 0
            )

            require(mfCount == 118) {
                "Se esperaban 118 MF, se cargaron $mfCount"
            }

            val palmMuteCount = loadPalmMuteMicRos(
                firstSampleIndex = mfCount
            )

            require(palmMuteCount == 17) {
                "Se esperaban 17 Palm Mute, se cargaron $palmMuteCount"
            }

            bankLoaded = true
            txtBank.text = "Banco: $mfCount MF + $palmMuteCount Palm Mute"

            NativeAudioEngine.setBpm(bpm)
            NativeAudioEngine.setStringDelayMs(stringDelayMs)
            NativeAudioEngine.setMasterVolume(masterVolume / 100f)
            NativeAudioEngine.setPattern(currentPattern)
            NativeAudioEngine.setChord(chordIdForName(currentChord))
        } else {
            txtBank.text = "Banco: 118 MF + 17 Palm Mute · sesión activa"
        }

        updateUi()
    }

    data class PcmWav(
        val samples: ShortArray,
        val sampleRate: Int
    )

    private fun loadLayer(
        layer: Int,
        suffix: String,
        maxFretByString: IntArray,
        firstSampleIndex: Int
    ): Int {

        var loaded = 0

        for (stringNumber in 1..6) {
            val maxFret =
                maxFretByString[stringNumber - 1]

            for (fret in 0..maxFret) {
                val resourceName =
                    buildResourceName(
                        stringNumber,
                        fret,
                        suffix
                    )

                val resId =
                    resources.getIdentifier(
                        resourceName,
                        "raw",
                        packageName
                    )

                require(resId != 0) {
                    "Falta recurso RAW: $resourceName.wav"
                }

                val wav =
                    readPcm16Wav(resId)

                NativeAudioEngine.loadSample(
                    index = firstSampleIndex + loaded,
                    stringNumber = stringNumber,
                    fret = fret,
                    layer = layer,
                    pcm16 = wav.samples,
                    sampleRate = wav.sampleRate
                )

                loaded++
            }
        }

        return loaded
    }

    private fun buildResourceName(
        stringNumber: Int,
        fret: Int,
        suffix: String
    ): String {

        // Afinación estándar MIDI:
        // 1=E4, 2=B3, 3=G3, 4=D3, 5=A2, 6=E2
        val openMidi = when (stringNumber) {
            1 -> 64
            2 -> 59
            3 -> 55
            4 -> 50
            5 -> 45
            6 -> 40
            else -> error("Cuerda inválida")
        }

        val midi = openMidi + fret

        val noteNames = arrayOf(
            "c", "cs", "d", "ds",
            "e", "f", "fs", "g",
            "gs", "a", "as", "b"
        )

        val note = noteNames[midi % 12]
        val octave = (midi / 12) - 1

        return "s${stringNumber}_f${
            fret.toString().padStart(2, '0')
        }_${note}${octave}_$suffix"
    }

    private data class PalmMuteSlot(
        val stringNumber: Int,
        val fret: Int
    )

    private val palmMuteSlots = listOf(
        PalmMuteSlot(6, 0),
        PalmMuteSlot(6, 3),

        PalmMuteSlot(5, 0),
        PalmMuteSlot(5, 2),
        PalmMuteSlot(5, 3),

        PalmMuteSlot(4, 0),
        PalmMuteSlot(4, 2),
        PalmMuteSlot(4, 4),

        PalmMuteSlot(3, 0),
        PalmMuteSlot(3, 2),
        PalmMuteSlot(3, 4),

        PalmMuteSlot(2, 0),
        PalmMuteSlot(2, 1),
        PalmMuteSlot(2, 3),

        PalmMuteSlot(1, 0),
        PalmMuteSlot(1, 2),
        PalmMuteSlot(1, 3)
    )

    private fun loadPalmMuteMicRos(
        firstSampleIndex: Int
    ): Int {
        var loaded = 0

        for (slot in palmMuteSlots) {
            val resourceName = buildPalmMuteResourceName(
                slot.stringNumber,
                slot.fret
            )

            val resId = resources.getIdentifier(
                resourceName,
                "raw",
                packageName
            )

            require(resId != 0) {
                "Falta Palm Mute RAW: $resourceName.wav"
            }

            val wav = readPcm16Wav(resId)

            NativeAudioEngine.loadSample(
                index = firstSampleIndex + loaded,
                stringNumber = slot.stringNumber,
                fret = slot.fret,
                layer = LAYER_PALM_MUTE,
                pcm16 = wav.samples,
                sampleRate = wav.sampleRate
            )

            loaded++
        }

        return loaded
    }

    private fun buildPalmMuteResourceName(
        stringNumber: Int,
        fret: Int
    ): String {
        val openMidi = when (stringNumber) {
            1 -> 64
            2 -> 59
            3 -> 55
            4 -> 50
            5 -> 45
            6 -> 40
            else -> error("Cuerda inválida")
        }

        val midi = openMidi + fret

        val noteNames = arrayOf(
            "c", "cs", "d", "ds",
            "e", "f", "fs", "g",
            "gs", "a", "as", "b"
        )

        val note = noteNames[midi % 12]
        val octave = (midi / 12) - 1

        // Copiaremos take1 de MicRos y lo renombraremos a este formato corto.
        return "pm_s${stringNumber}_f${
            fret.toString().padStart(2, '0')
        }_${note}${octave}_mf"
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

                    require(bits == 16) {
                        "Solo PCM16 soportado"
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
            raw.size / (2 * channels)

        val mono = ShortArray(frames)
        var position = 0

        fun nextSample(): Int {
            val lo =
                raw[position++].toInt() and 0xFF

            val hi =
                raw[position++].toInt()

            return (
                (hi shl 8) or lo
            ).toShort().toInt()
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
            R.id.btnOpen
        ).setOnClickListener {
            selectChord("OPEN", 0)
        }

        findViewById<Button>(
            R.id.btnEm
        ).setOnClickListener {
            selectChord("Em", 1)
        }

        findViewById<Button>(
            R.id.btnC
        ).setOnClickListener {
            selectChord("C", 2)
        }

        findViewById<Button>(
            R.id.btnG
        ).setOnClickListener {
            selectChord("G", 3)
        }

        findViewById<Button>(
            R.id.btnD
        ).setOnClickListener {
            selectChord("D", 4)
        }

        findViewById<Button>(
            R.id.btnBm
        ).setOnClickListener {
            selectChord("Bm", 5)
        }

        findViewById<Button>(
            R.id.btnPatternPopRock
        ).setOnClickListener {
            selectPattern(0)
        }

        findViewById<Button>(
            R.id.btnPatternBallad
        ).setOnClickListener {
            selectPattern(1)
        }

        findViewById<Button>(
            R.id.btnPatternArpeggio
        ).setOnClickListener {
            selectPattern(2)
        }

        findViewById<Button>(
            R.id.btnPatternRockMuted
        ).setOnClickListener {
            selectPattern(3)
        }

        findViewById<Button>(
            R.id.btnPatternGoldenStrum
        ).setOnClickListener {
            selectPattern(4)
        }

        findViewById<Button>(
            R.id.btnPatternGoldenFolk
        ).setOnClickListener {
            selectPattern(5)
        }

        findViewById<Button>(
            R.id.btnPatternCbFolk20
        ).setOnClickListener {
            selectPattern(6)
        }

        findViewById<Button>(
            R.id.btnPatternCbFolk13
        ).setOnClickListener {
            selectPattern(7)
        }

        findViewById<Button>(
            R.id.btnPatternCbBallad20
        ).setOnClickListener {
            selectPattern(8)
        }

        findViewById<Button>(
            R.id.btnPatternCbBallad13
        ).setOnClickListener {
            selectPattern(9)
        }

        findViewById<Button>(
            R.id.btnPatternStringZones
        ).setOnClickListener {
            selectPattern(10)
        }

        findViewById<Button>(
            R.id.btnPatternStringZonesHuman
        ).setOnClickListener {
            selectPattern(11)
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

    private fun selectChord(
        name: String,
        id: Int
    ) {
        if (
            currentChord == name &&
            name != "OPEN"
        ) {
            currentChord = "OPEN"
            NativeAudioEngine.setChord(0)
        } else {
            currentChord = name
            NativeAudioEngine.setChord(id)
        }

        updateUi()
    }

    private fun selectPattern(
        id: Int
    ) {
        currentPattern = id

        // Para las pruebas Golden arrancamos al BPM exacto
        // de los ejemplos externos que escuchamos.
        when (id) {
            4 -> bpm = 120 // pop-sections.btml / strum_up_down
            5 -> bpm = 100 // folk-strum.btml / folk
            6, 7 -> bpm = 100 // ChordBox Folk A/B
            8, 9 -> bpm = 90  // ChordBox Pop Ballad A/B
            10 -> bpm = 100   // prueba de zonas físicas
            11 -> bpm = 100   // mismo patrón con humanización controlada
        }

        NativeAudioEngine.setBpm(bpm)
        NativeAudioEngine.setPattern(id)
        updateUi()
    }

    private fun patternName(): String {
        return when (currentPattern) {
            0 -> "Pop/Rock · MF"
            1 -> "Balada Rock · MF"
            2 -> "PIMAMI · timing humano · cambio en P"
            3 -> "Rock Muted · Palm Mute real"
            4 -> "GOLDEN 01 · ako strum_up_down · 120 BPM"
            5 -> "GOLDEN 02 · ako folk · 100 BPM"
            6 -> "CB FOLK A · 20 ms TOTAL"
            7 -> "CB FOLK B · 13 ms POR CUERDA"
            8 -> "CB BALLAD A · 20 ms TOTAL"
            9 -> "CB BALLAD B · 13 ms POR CUERDA"
            10 -> "ZONAS · referencia limpia"
            11 -> "ZONAS HUMAN · microtiming + dinámica"
            else -> "Patrón experimental"
        }
    }

    private fun updateUi() {
        txtChord.text = currentChord
        txtBpm.text = "$bpm BPM"

        txtDelay.text =
            "Separación entre cuerdas: " +
            "$stringDelayMs ms"

        txtVolume.text =
            "Volumen master: $masterVolume%"

        txtPattern.text =
            patternName()

        btnPlayPause.text =
            if (isPlaying) {
                "⏸ PAUSA"
            } else {
                "▶ PLAY"
            }
    }

    private fun chordIdForName(name: String): Int = when (name) {
        "Em" -> 1
        "C" -> 2
        "G" -> 3
        "D" -> 4
        "Bm" -> 5
        else -> 0
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
