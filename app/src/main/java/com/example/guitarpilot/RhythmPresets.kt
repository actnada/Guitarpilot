package com.example.guitarpilot

import android.content.res.AssetManager

data class RhythmPreset(val id: Int, val name: String, val assetPath: String)

object RhythmPresets {
    // IDs de sesión estables: PIMAMI conserva el ID nativo 2.
    val entries = listOf(
        RhythmPreset(14, "Balada 02 · suave", "rhythms/balada_02.json"),
        RhythmPreset(15, "Pop 01", "rhythms/fretwork_pop_01.json"),
        RhythmPreset(16, "Rock 01", "rhythms/fretwork_rock_01.json"),
        RhythmPreset(17, "Folk 01", "rhythms/fretwork_folk_01.json"),
        RhythmPreset(18, "Balada 03", "rhythms/fretwork_balada_03.json")
    )

    fun find(id: Int): RhythmPreset? = entries.firstOrNull { it.id == id }

    fun load(assets: AssetManager, preset: RhythmPreset): RhythmDefinition =
        assets.open(preset.assetPath).bufferedReader().use {
            RhythmJsonCodec.parse(it.readText())
        }
}
