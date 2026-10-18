@file:JvmName("TextHilfen")

package de.beispiel.notizen.util

import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

private val DATUM: DateTimeFormatter = DateTimeFormatter.ofPattern("dd.MM.yyyy HH:mm")

/** Hoechstens `laenge` Zeichen, sonst mit … am Ende. */
fun String.kuerze(laenge: Int): String =
    if (length <= laenge) this else take(laenge - 1) + "…"

fun Instant.alsDatum(zone: ZoneId = ZoneId.systemDefault()): String = DATUM.format(atZone(zone))

/** Die Schlagworte eines Texts: Woerter mit # vorn, klein, ohne Satzzeichen. */
fun schlagworteIn(text: String): Set<String> {
    val aus = linkedSetOf<String>()
    var i = 0
    while (i < text.length) {
        if (text[i] != '#') {
            i++
            continue
        }
        var j = i + 1
        while (j < text.length && (text[j].isLetterOrDigit() || text[j] == '_')) j++
        if (j > i + 1) aus += text.substring(i + 1, j).lowercase()
        i = j
    }
    return aus
}

/** Wie viele Zeilen ein Text braucht, wenn eine Zeile `breite` Zeichen fasst. */
fun zeilenFuer(text: String, breite: Int): Int {
    require(breite > 0) { "breite muss positiv sein" }
    var zeilen = 0
    for (absatz in text.split('\n')) {
        zeilen += if (absatz.isEmpty()) 1 else (absatz.length + breite - 1) / breite
    }
    return zeilen
}

val String.istSchlagwort: Boolean
    get() = startsWith("#") && length > 1 && drop(1).all { it.isLetterOrDigit() || it == '_' }

inline fun <reified T> List<Any>.nurVom(): List<T> = filterIsInstance<T>()

infix fun Int.zwischen(bereich: IntRange): Boolean = this in bereich

fun bewerte(zeichen: Int): String = when (zeichen) {
    0 -> "leer"
    in 1..140 -> "kurz"
    in 141 until 1000 -> "mittel"
    else -> {
        val seiten = zeichen / 1800
        if (seiten > 1) "lang ($seiten Seiten)" else "lang"
    }
}

val ROHTEXT = """
    Zeilen bleiben, wie sie sind;
    auch "Anfuehrungszeichen" und ${'$'}-Zeichen.
"""
