package de.beispiel.notizen.data

import java.time.Instant

typealias NotizId = Long

enum class Prioritaet(val gewicht: Int) {
    NIEDRIG(1),
    NORMAL(2),
    HOCH(3) {
        override fun markiert() = true
    };

    open fun markiert(): Boolean = false

    companion object {
        fun aus(text: String): Prioritaet =
            entries.firstOrNull { it.name.equals(text, ignoreCase = true) } ?: NORMAL
    }
}

data class Notiz(
    val id: NotizId,
    val titel: String,
    val text: String = "",
    val prioritaet: Prioritaet = Prioritaet.NORMAL,
    val erstellt: Instant = Instant.now(),
    val schlagworte: Set<String> = emptySet(),
) {
    val istLeer: Boolean
        get() = titel.isBlank() && text.isBlank()

    var entwurf: String = ""
        set(wert) {
            field = wert.trim()
        }

    fun mitSchlagwort(wort: String): Notiz = copy(schlagworte = schlagworte + wort.lowercase())
}

sealed interface Ergebnis<out T> {
    data class Erfolg<T>(val wert: T) : Ergebnis<T>
    data class Fehler(val grund: Throwable) : Ergebnis<Nothing>
    data object Laedt : Ergebnis<Nothing>
}

/** Was die Liste zeigt: alle, nach Prioritaet, oder nur die mit einem Schlagwort. */
sealed class Filter {
    object Alle : Filter()
    data class NachPrioritaet(val mindestens: Prioritaet) : Filter()
    class MitSchlagwort(val wort: String) : Filter()

    fun passt(notiz: Notiz): Boolean = when (this) {
        is Alle -> true
        is NachPrioritaet -> notiz.prioritaet.gewicht >= mindestens.gewicht
        is MitSchlagwort -> wort in notiz.schlagworte
    }
}
