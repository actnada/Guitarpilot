package com.example.guitarpilot

/**
 * Estado del instrumento independiente de los ritmos.
 * Los seis botones son accesos configurables a 48 acordes + OPEN.
 * La selección se conserva entre la pantalla principal y el editor.
 */
object ChordSession {
    data class ChordOption(val id: Int, val name: String)

    val names = listOf(
        "OPEN", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
        "Cm", "C#m", "Dm", "D#m", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "A#m", "Bm",
        "C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7", "A7", "A#7", "B7",
        "Cm7", "C#m7", "Dm7", "D#m7", "Em7", "Fm7", "F#m7",
        "Gm7", "G#m7", "Am7", "A#m7", "Bm7"
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

    /**
     * Resuelve exclusivamente el gesto de pulsación de un botón.
     * Repetir el acorde activo vuelve a OPEN (cuerdas al aire); tocar uno
     * diferente lo selecciona. No se usa al restaurar estado o cambiar ritmo.
     */
    internal fun chordAfterTap(activeId: Int, tappedId: Int): Int {
        require(activeId in names.indices) { "Acorde activo inválido" }
        require(tappedId in names.indices) { "Acorde pulsado inválido" }
        return if (tappedId != 0 && tappedId == activeId) 0 else tappedId
    }

    fun tap(chordId: Int) {
        select(chordAfterTap(selectedId, chordId))
    }

    fun select(chordId: Int) {
        require(chordId in names.indices) { "Acorde inválido" }
        selectedId = chordId
        // OPEN = cuerda al aire, NO stop/mute. El transporte sigue sonando.
        NativeAudioEngine.setChord(chordId)
    }
}
