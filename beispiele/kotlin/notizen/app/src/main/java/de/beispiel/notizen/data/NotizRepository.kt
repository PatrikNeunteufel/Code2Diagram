package de.beispiel.notizen.data

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.withContext
import java.io.File
import java.io.IOException

interface NotizRepository {
    val notizen: Flow<List<Notiz>>

    suspend fun lege(notiz: Notiz): Ergebnis<Notiz>
    suspend fun loesche(id: NotizId): Boolean
    suspend fun lade(): Ergebnis<Int>

    fun naechsteId(): NotizId = System.nanoTime()
}

class SpeicherRepository private constructor(
    private val datei: File,
    private val format: NotizFormat = NotizFormat,
) : NotizRepository {

    private val zustand = MutableStateFlow<List<Notiz>>(emptyList())
    override val notizen: Flow<List<Notiz>> = zustand.asStateFlow()

    init {
        require(datei.parentFile?.isDirectory == true) { "kein Ordner fuer ${datei.name}" }
    }

    override suspend fun lege(notiz: Notiz): Ergebnis<Notiz> {
        if (notiz.istLeer) return Ergebnis.Fehler(IllegalArgumentException("leere Notiz"))
        zustand.update { liste ->
            val ohne = liste.filterNot { it.id == notiz.id }
            (ohne + notiz).sortedByDescending { it.erstellt }
        }
        return speichere().let { gespeichert ->
            if (gespeichert) Ergebnis.Erfolg(notiz) else Ergebnis.Fehler(IOException("nicht gespeichert"))
        }
    }

    override suspend fun loesche(id: NotizId): Boolean {
        var gefunden = false
        zustand.update { liste ->
            liste.filter {
                val bleibt = it.id != id
                if (!bleibt) gefunden = true
                bleibt
            }
        }
        return gefunden && speichere()
    }

    override suspend fun lade(): Ergebnis<Int> = withContext(Dispatchers.IO) {
        try {
            if (!datei.exists()) return@withContext Ergebnis.Erfolg(0)
            val gelesen = mutableListOf<Notiz>()
            datei.bufferedReader().use { leser ->
                var zeile = leser.readLine()
                while (zeile != null) {
                    format.lies(zeile)?.let { gelesen += it }
                    zeile = leser.readLine()
                }
            }
            zustand.value = gelesen
            Ergebnis.Erfolg(gelesen.size)
        } catch (fehler: IOException) {
            Ergebnis.Fehler(fehler)
        } finally {
            protokolliere("geladen: ${datei.path}")
        }
    }

    private suspend fun speichere(): Boolean = withContext(Dispatchers.IO) {
        val zeilen = zustand.value.map { format.schreibe(it) }
        runCatching { datei.writeText(zeilen.joinToString("\n")) }.isSuccess
    }

    companion object {
        private const val DATEINAME = "notizen.txt"

        @Volatile
        private var einzige: SpeicherRepository? = null

        fun erzeuge(context: Context): SpeicherRepository =
            einzige ?: synchronized(this) {
                einzige ?: SpeicherRepository(File(context.filesDir, DATEINAME)).also { einzige = it }
            }
    }
}

/** Eine Zeile je Notiz: id;prioritaet;titel;text — mit `\;` im Text. */
object NotizFormat {
    private const val TRENNER = ';'

    fun schreibe(notiz: Notiz): String =
        listOf(notiz.id.toString(), notiz.prioritaet.name, maskiere(notiz.titel), maskiere(notiz.text))
            .joinToString(TRENNER.toString())

    fun lies(zeile: String): Notiz? {
        val teile = teile(zeile)
        if (teile.size < 3) return null
        val id = teile[0].toLongOrNull() ?: return null
        return Notiz(
            id = id,
            titel = teile[2],
            text = teile.getOrElse(3) { "" },
            prioritaet = Prioritaet.aus(teile[1]),
        )
    }

    private fun maskiere(text: String) = text.replace("$TRENNER", "\\$TRENNER")

    private fun teile(zeile: String): List<String> {
        val aus = mutableListOf<String>()
        val stueck = StringBuilder()
        var i = 0
        while (i < zeile.length) {
            val c = zeile[i]
            when {
                c == '\\' && i + 1 < zeile.length -> {
                    stueck.append(zeile[i + 1])
                    i++
                }
                c == TRENNER -> {
                    aus += stueck.toString()
                    stueck.clear()
                }
                else -> stueck.append(c)
            }
            i++
        }
        aus += stueck.toString()
        return aus
    }
}

private fun protokolliere(text: String) {
    android.util.Log.d("Notizen", text)
}
