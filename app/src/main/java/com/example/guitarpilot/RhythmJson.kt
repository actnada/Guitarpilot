package com.example.guitarpilot

import org.json.JSONArray
import org.json.JSONObject

data class RhythmEventDef(
    var tick: Int,
    var offsetMs: Int = 0,
    var type: String,
    var target: String = "ALL_ALLOWED",
    var velocity: Float = 0.8f,
    var spreadMs: Float = 13f,
    var layer: String = "MF",
    var humanize: Boolean = false,
    var stringNumber: Int = 1
)

data class RhythmDefinition(
    var schemaVersion: Int = 1,
    var id: String,
    var name: String,
    var category: String = "custom",
    var defaultBpm: Int = 100,
    var ticksPerBar: Int = 16,
    var timeSignature: String = "4/4",
    var events: MutableList<RhythmEventDef>
)

object RhythmSession {
    var currentCustomName: String? = null
    var currentCustomBpm: Int? = null
}

object RhythmJsonCodec {

    private val validTypes = setOf(
        "STRUM_DOWN",
        "STRUM_UP",
        "ROOT_BASS",
        "ALT_BASS",
        "PLUCK_STRING"
    )

    private val validTargets = setOf(
        "ALL_ALLOWED",
        "BASS_3",
        "TREBLE_3",
        "TREBLE_4",
        "ROOT_BASS"
    )

    fun parse(text: String): RhythmDefinition {
        val root = JSONObject(text)

        val schemaVersion = root.optInt("schemaVersion", 1)
        require(schemaVersion == 1) {
            "schemaVersion no soportado: \$schemaVersion"
        }

        val ticksPerBar = root.optInt("ticksPerBar", 16)
        require(ticksPerBar in 1..64) {
            "ticksPerBar debe estar entre 1 y 64"
        }

        val bpm = root.optInt("defaultBpm", 100)
        require(bpm in 40..220) {
            "defaultBpm debe estar entre 40 y 220"
        }

        val array = root.getJSONArray("events")
        require(array.length() in 1..128) {
            "events debe contener entre 1 y 128 eventos"
        }

        val events = mutableListOf<RhythmEventDef>()

        for (i in 0 until array.length()) {
            val e = array.getJSONObject(i)

            val type = e.getString("type").uppercase()
            require(type in validTypes) {
                "Evento \${i + 1}: type inválido: \$type"
            }

            val target = e.optString("target", "ALL_ALLOWED").uppercase()
            require(target in validTargets) {
                "Evento \${i + 1}: target inválido: \$target"
            }

            val tick = e.getInt("tick")
            require(tick in 0 until ticksPerBar) {
                "Evento \${i + 1}: tick fuera de rango"
            }

            val offsetMs = e.optInt("offsetMs", 0)
            require(offsetMs in -100..100) {
                "Evento \${i + 1}: offsetMs debe estar entre -100 y 100"
            }

            val velocity = e.optDouble("velocity", 0.8).toFloat()
            require(velocity in 0f..1f) {
                "Evento \${i + 1}: velocity debe estar entre 0 y 1"
            }

            val spreadMs = e.optDouble("spreadMs", 13.0).toFloat()
            require(spreadMs in 0f..60f) {
                "Evento \${i + 1}: spreadMs debe estar entre 0 y 60"
            }

            val layer = e.optString("layer", "MF").uppercase()
            require(layer == "MF" || layer == "PALM_MUTE") {
                "Evento \${i + 1}: layer debe ser MF o PALM_MUTE"
            }

            val stringNumber = e.optInt("stringNumber", 1)
            require(stringNumber in 1..6) {
                "Evento \${i + 1}: stringNumber debe estar entre 1 y 6"
            }

            events += RhythmEventDef(
                tick = tick,
                offsetMs = offsetMs,
                type = type,
                target = target,
                velocity = velocity,
                spreadMs = spreadMs,
                layer = layer,
                humanize = e.optBoolean("humanize", false),
                stringNumber = stringNumber
            )
        }

        return RhythmDefinition(
            schemaVersion = schemaVersion,
            id = root.optString("id", "custom_rhythm"),
            name = root.optString("name", "Ritmo personalizado"),
            category = root.optString("category", "custom"),
            defaultBpm = bpm,
            ticksPerBar = ticksPerBar,
            timeSignature = root.optString("timeSignature", "4/4"),
            events = events
        )
    }

    fun toJson(definition: RhythmDefinition): String {
        val root = JSONObject()
        root.put("schemaVersion", definition.schemaVersion)
        root.put("id", definition.id)
        root.put("name", definition.name)
        root.put("category", definition.category)
        root.put("defaultBpm", definition.defaultBpm)
        root.put("timeSignature", definition.timeSignature)
        root.put("ticksPerBar", definition.ticksPerBar)

        val array = JSONArray()

        definition.events.forEach { event ->
            val e = JSONObject()
            e.put("tick", event.tick)
            e.put("offsetMs", event.offsetMs)
            e.put("type", event.type)
            e.put("target", event.target)
            e.put("velocity", event.velocity.toDouble())
            e.put("spreadMs", event.spreadMs.toDouble())
            e.put("layer", event.layer)
            e.put("humanize", event.humanize)
            if (event.type == "PLUCK_STRING") {
                e.put("stringNumber", event.stringNumber)
            }
            array.put(e)
        }

        root.put("events", array)
        return root.toString(2)
    }

    fun applyToEngine(definition: RhythmDefinition): Boolean {
        val size = definition.events.size

        val ticks = IntArray(size)
        val offsets = IntArray(size)
        val types = IntArray(size)
        val targets = IntArray(size)
        val layers = IntArray(size)
        val strings = IntArray(size)
        val velocities = FloatArray(size)
        val spreads = FloatArray(size)
        val humanize = IntArray(size)

        definition.events.forEachIndexed { i, event ->
            ticks[i] = event.tick
            offsets[i] = event.offsetMs
            types[i] = when (event.type) {
                "STRUM_DOWN" -> 0
                "STRUM_UP" -> 1
                "ROOT_BASS" -> 2
                "ALT_BASS" -> 3
                "PLUCK_STRING" -> 4
                else -> error("Tipo no soportado: \${event.type}")
            }

            targets[i] = when (event.target) {
                "ALL_ALLOWED" -> 0
                "BASS_3" -> 1
                "TREBLE_3" -> 2
                "TREBLE_4" -> 3
                "ROOT_BASS" -> 4
                else -> 0
            }

            layers[i] = if (event.layer == "PALM_MUTE") 1 else 0
            strings[i] = event.stringNumber
            velocities[i] = event.velocity
            spreads[i] = event.spreadMs
            humanize[i] = if (event.humanize) 1 else 0
        }

        NativeAudioEngine.setBpm(definition.defaultBpm)

        val ok = NativeAudioEngine.setCustomPattern(
            ticksPerBar = definition.ticksPerBar,
            ticks = ticks,
            offsetsMs = offsets,
            types = types,
            targets = targets,
            layers = layers,
            stringNumbers = strings,
            velocities = velocities,
            spreadsMs = spreads,
            humanizeFlags = humanize
        )

        if (ok) {
            RhythmSession.currentCustomName = definition.name
            RhythmSession.currentCustomBpm = definition.defaultBpm
        }

        return ok
    }
}
