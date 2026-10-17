package com.example.guitarpilot

import android.content.res.AssetManager

data class RhythmPreset(val id: Int, val name: String, val assetPath: String)

object RhythmPresets {
    // IDs estables: PIMAMI conserva su ID nativo 2; importados: 19..27.
    val entries = listOf(
        RhythmPreset(14, "Balada 1", "rhythms/balada_02.json"),
        RhythmPreset(15, "Pop 1", "rhythms/fretwork_pop_01.json"),
        RhythmPreset(16, "Rock", "rhythms/fretwork_rock_01.json"),
        RhythmPreset(17, "Folk 1", "rhythms/fretwork_folk_01.json"),
        RhythmPreset(18, "Balada 2", "rhythms/fretwork_balada_03.json"),
        RhythmPreset(19, "Pop 2", "rhythms/ample_pop_02.json"),
        RhythmPreset(20, "Pop 3", "rhythms/ample_pop_03.json"),
        RhythmPreset(21, "Pop 4", "rhythms/ample_pop_04.json"),
        RhythmPreset(22, "Pop 5", "rhythms/ample_pop_05.json"),
        RhythmPreset(23, "Pop 6", "rhythms/ample_pop_06.json"),
        RhythmPreset(24, "Folk 2", "rhythms/ample_folk_02.json"),
        RhythmPreset(25, "Folk 3", "rhythms/ample_folk_03.json"),
        RhythmPreset(26, "Folk 4", "rhythms/ample_folk_04.json"),
        RhythmPreset(27, "Folk 5", "rhythms/ample_folk_05.json")
    )

    fun find(id: Int): RhythmPreset? = entries.firstOrNull { it.id == id }

    fun load(assets: AssetManager, preset: RhythmPreset): RhythmDefinition =
        assets.open(preset.assetPath).bufferedReader().use {
            RhythmJsonCodec.parse(it.readText())
        }
}
