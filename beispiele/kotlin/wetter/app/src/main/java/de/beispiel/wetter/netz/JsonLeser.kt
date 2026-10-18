package de.beispiel.wetter.netz

class JsonFehler(meldung: String, val stelle: Int) : Exception("$meldung bei $stelle")

/**
 * Ein kleiner JSON-Leser in rekursivem Abstieg: Objekte, Listen, Zeichenketten mit Escapes,
 * Zahlen, true/false/null. Die Zeichenkette liest ein Automat mit drei Zustaenden.
 */
class JsonLeser(private val text: String) {
    private var p = 0

    private enum class Zustand { NORMAL, ESCAPE, UNICODE }

    fun lies(): Any? {
        val wert = wert()
        leer()
        if (p != text.length) throw JsonFehler("Text nach dem Wert", p)
        return wert
    }

    private fun wert(): Any? {
        leer()
        if (p >= text.length) throw JsonFehler("Ende", p)
        return when (val c = text[p]) {
            '{' -> objekt()
            '[' -> liste()
            '"' -> zeichenkette()
            't', 'f', 'n' -> wort()
            else -> if (c == '-' || c.isDigit()) zahl() else throw JsonFehler("unerwartet '$c'", p)
        }
    }

    private fun objekt(): Map<String, Any?> {
        val aus = linkedMapOf<String, Any?>()
        p++ // {
        leer()
        if (nimm('}')) return aus
        do {
            leer()
            val schluessel = zeichenkette()
            leer()
            if (!nimm(':')) throw JsonFehler("':' fehlt", p)
            aus[schluessel] = wert()
            leer()
        } while (nimm(','))
        if (!nimm('}')) throw JsonFehler("'}' fehlt", p)
        return aus
    }

    private fun liste(): List<Any?> {
        val aus = mutableListOf<Any?>()
        p++ // [
        leer()
        if (nimm(']')) return aus
        while (true) {
            aus += wert()
            leer()
            if (nimm(']')) break
            if (!nimm(',')) throw JsonFehler("',' oder ']' fehlt", p)
        }
        return aus
    }

    private fun zeichenkette(): String {
        if (!nimm('"')) throw JsonFehler("'\"' erwartet", p)
        val aus = StringBuilder()
        var zustand = Zustand.NORMAL
        var unicode = 0
        var ziffern = 0
        zeichen@ while (p < text.length) {
            val c = text[p++]
            when (zustand) {
                Zustand.NORMAL -> when (c) {
                    '"' -> return aus.toString()
                    '\\' -> zustand = Zustand.ESCAPE
                    else -> aus.append(c)
                }
                Zustand.ESCAPE -> {
                    zustand = Zustand.NORMAL
                    when (c) {
                        'n' -> aus.append('\n')
                        't' -> aus.append('\t')
                        'u' -> {
                            zustand = Zustand.UNICODE
                            unicode = 0
                            ziffern = 0
                        }
                        else -> aus.append(c)
                    }
                }
                Zustand.UNICODE -> {
                    val ziffer = Character.digit(c, 16)
                    if (ziffer < 0) throw JsonFehler("keine Hex-Ziffer", p - 1)
                    unicode = unicode * 16 + ziffer
                    if (++ziffern < 4) continue@zeichen
                    aus.append(unicode.toChar())
                    zustand = Zustand.NORMAL
                }
            }
        }
        throw JsonFehler("Zeichenkette nicht geschlossen", p)
    }

    private fun zahl(): Double {
        val anfang = p
        if (text[p] == '-') p++
        while (p < text.length && (text[p].isDigit() || text[p] in ".eE+-")) p++
        return text.substring(anfang, p).toDoubleOrNull() ?: throw JsonFehler("keine Zahl", anfang)
    }

    private fun wort(): Any? {
        for ((wort, wert) in listOf("true" to true, "false" to false, "null" to null)) {
            if (text.startsWith(wort, p)) {
                p += wort.length
                return wert
            }
        }
        throw JsonFehler("unbekanntes Wort", p)
    }

    private fun leer() {
        while (p < text.length && text[p].isWhitespace()) p++
    }

    private fun nimm(c: Char): Boolean {
        if (p < text.length && text[p] == c) {
            p++
            return true
        }
        return false
    }
}
