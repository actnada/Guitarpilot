package com.example.guitarpilot

object NativeAudioEngine {
    init { System.loadLibrary("guitarpilot") }

    external fun create(): Boolean
    external fun isCreated(): Boolean
    external fun destroy()

    external fun loadSample(
        index: Int,
        stringNumber: Int,
        fret: Int,
        layer: Int,
        pcm16: ShortArray,
        sampleRate: Int
    )

    external fun play()
    external fun pause()
    external fun stop()

    external fun setChord(chord: Int)
    external fun getChordDebugState(): IntArray
    external fun setBpm(bpm: Int)
    external fun setStringDelayMs(delayMs: Int)
    external fun setPattern(pattern: Int)
    external fun setMasterVolume(volume: Float)

    external fun setCustomPattern(
        ticksPerBar: Int,
        ticks: IntArray,
        offsetsMs: IntArray,
        types: IntArray,
        targets: IntArray,
        layers: IntArray,
        stringNumbers: IntArray,
        velocities: FloatArray,
        spreadsMs: FloatArray,
        releaseMs: IntArray,
        humanizeFlags: IntArray
    ): Boolean

    external fun clearCustomPattern()

    external fun manualStrumDown()
    external fun manualStrumUp()
    external fun playDiagnosticSample()
}
