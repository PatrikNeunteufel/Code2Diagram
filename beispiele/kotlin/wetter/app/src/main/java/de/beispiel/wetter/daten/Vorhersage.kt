package de.beispiel.wetter.daten

import kotlin.math.roundToInt

enum class Einheit(val zeichen: String) {
    CELSIUS("°C") {
        override fun aus(celsius: Double) = celsius
    },
    FAHRENHEIT("°F") {
        override fun aus(celsius: Double) = celsius * 9 / 5 + 32
    },
    KELVIN("K") {
        override fun aus(celsius: Double) = celsius + 273.15
    };

    abstract fun aus(celsius: Double): Double

    fun formatiere(celsius: Double): String = "${aus(celsius).roundToInt()} $zeichen"
}

sealed class Wetterlage {
    data class Klar(val temperatur: Double) : Wetterlage()
    data class Regen(val millimeter: Double, val temperatur: Double) : Wetterlage()
    class Gewitter(val warnung: String?) : Wetterlage()
    object Unbekannt : Wetterlage()

    val gefaehrlich: Boolean
        get() = this is Gewitter || (this is Regen && millimeter > STARKREGEN_MM)

    companion object {
        const val STARKREGEN_MM = 25.0

        fun aus(code: Int, temperatur: Double, niederschlag: Double): Wetterlage = when (code) {
            0, 1 -> Klar(temperatur)
            in 51..67, in 80..82 -> Regen(niederschlag, temperatur)
            95, 96, 99 -> Gewitter(if (code == 99) "Hagel moeglich" else null)
            else -> Unbekannt
        }
    }
}

data class Vorhersage(val ort: String, val tage: List<Tag>) {

    data class Tag(
        val datum: String,
        val hoch: Double,
        val tief: Double,
        val wind: Int = 0,
        val lage: Wetterlage = Wetterlage.Unbekannt,
        val unsicher: Boolean = false,
    ) {
        constructor(datum: String, mittel: Double) : this(datum, mittel + 2, mittel - 2, unsicher = true)

        operator fun plus(anderer: Tag): Tag = copy(hoch = maxOf(hoch, anderer.hoch), tief = minOf(tief, anderer.tief))
    }

    fun heute(): Wetterlage? = tage.firstOrNull()?.lage

    /** Die Tage mit Frost, von hinten: wann der letzte war und wie viele es sind. */
    fun frost(): Pair<String?, Int> {
        var letzter: String? = null
        var anzahl = 0
        for (i in tage.indices.reversed()) {
            val tag = tage[i]
            if (tag.tief >= 0) continue
            if (letzter == null) letzter = tag.datum
            anzahl++
        }
        return letzter to anzahl
    }

    /** Wochen zu je sieben Tagen zusammengefasst; ein angefangener Rest zaehlt mit. */
    fun wochen(): List<Tag> {
        val aus = mutableListOf<Tag>()
        for (anfang in 0 until tage.size step 7) {
            val ende = minOf(anfang + 7, tage.size)
            var woche = tage[anfang]
            for (i in anfang + 1 until ende) woche += tage[i]
            aus += woche
        }
        return aus
    }
}
