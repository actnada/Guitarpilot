package com.example.guitarpilot

/**
 * Estado del instrumento independiente de los ritmos.
 * Los seis botones son accesos configurables a un catálogo de 36 acordes + OPEN.
 * La selección se conserva entre la pantalla principal y el editor.
 */
object ChordSession {
    data class ChordOption(val id: Int, val name: String)

    val names = listOf(
        "OPEN", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
        "Cm", "C#m", "Dm", "D#m", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "A#m", "Bm",
        "C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7", "A7", "A#7", "B7"
    )
    val options: List<ChordOption> = names.mapIndexed { id, name -> ChordOption(id, name) }

    // Botones iniciales: C, G, D, Em, Am, Bm. Todos pueden reasignarse.
    private val quickSlots = intArrayOf(1, 8, 3, 17, 22, 24)

    var selectedId: Int = 0
        private set

    val selectedName: String get() = names[selectedId]

    fun slotAt(slotIndex: Int): Int = quickSlots[slotIndex]

    fun assign(slotIndex: Int, chordId: Int) {
        require(slotIndex in quickSlots.indices) { "Botón de acorde inválido" }
        require(chordId in names.indices) { "Acorde inválido" }
        quickSlots[slotIndex] = chordId
    }

    fun select(chordId: Int) {
        require(chordId in names.indices) { "Acorde inválido" }
        selectedId = chordId
        // El cambio de acorde no modifica el transporte, tick ni BPM.
        NativeAudioEngine.setChord(chordId)
    }
}
