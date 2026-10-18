package de.beispiel.wetter.netz

import de.beispiel.wetter.daten.Vorhersage
import de.beispiel.wetter.daten.Wetterlage
import java.io.IOException
import java.net.HttpURLConnection
import java.net.SocketTimeoutException
import java.net.URL
import java.net.URLEncoder
import java.util.concurrent.atomic.AtomicBoolean

/** Holt die Vorhersage in einem eigenen Faden und meldet sie ueber einen Rueckruf. */
class WetterDienst(private val ort: String, private val rueckruf: Rueckruf) {

    interface Rueckruf {
        fun fertig(vorhersage: Vorhersage)
        fun fehler(grund: String, nochmal: Boolean)
        fun fortschritt(prozent: Int) {}
    }

    private val abgebrochen = AtomicBoolean(false)
    private var faden: Thread? = null
    private var versuche = 0

    fun starte() {
        abgebrochen.set(false)
        faden = Thread {
            try {
                val vorhersage = hole()
                if (!abgebrochen.get()) rueckruf.fertig(vorhersage)
            } catch (e: SocketTimeoutException) {
                versuche++
                rueckruf.fehler("Zeitueberschreitung", versuche < MAX_VERSUCHE)
            } catch (e: IOException) {
                rueckruf.fehler("Netzfehler: ${e.message}", false)
            } catch (e: JsonFehler) {
                rueckruf.fehler("Antwort nicht lesbar (Stelle ${e.stelle})", false)
            }
        }.apply {
            name = "wetter-$ort"
            isDaemon = true
            start()
        }
    }

    fun beende() {
        abgebrochen.set(true)
        faden?.interrupt()
        faden = null
    }

    private fun hole(): Vorhersage {
        val adresse = URL(ADRESSE.format(URLEncoder.encode(ort, "UTF-8")))
        val verbindung = adresse.openConnection() as HttpURLConnection
        try {
            verbindung.connectTimeout = 10_000
            verbindung.readTimeout = 10_000
            rueckruf.fortschritt(10)
            val code = verbindung.responseCode
            if (code != HttpURLConnection.HTTP_OK) throw IOException("HTTP $code")
            val text = verbindung.inputStream.bufferedReader().use { it.readText() }
            rueckruf.fortschritt(80)
            return liesAntwort(text)
        } finally {
            verbindung.disconnect()
        }
    }

    private fun liesAntwort(text: String): Vorhersage {
        val wurzel = JsonLeser(text).lies() as? Map<*, *> ?: throw JsonFehler("kein Objekt", 0)
        val taeglich = wurzel["daily"] as? Map<*, *> ?: return Vorhersage(ort, emptyList())
        val daten = (taeglich["time"] as? List<*>).orEmpty()
        val hoch = zahlen(taeglich["temperature_2m_max"])
        val tief = zahlen(taeglich["temperature_2m_min"])
        val codes = zahlen(taeglich["weathercode"])
        val regen = zahlen(taeglich["precipitation_sum"])
        val tage = daten.indices.map { i ->
            val h = hoch.getOrNull(i)
            val t = tief.getOrNull(i)
            if (h == null || t == null) {
                Vorhersage.Tag(daten[i].toString(), h ?: t ?: 0.0)
            } else {
                Vorhersage.Tag(
                    datum = daten[i].toString(),
                    hoch = h,
                    tief = t,
                    lage = Wetterlage.aus(codes.getOrNull(i)?.toInt() ?: -1, (h + t) / 2, regen.getOrNull(i) ?: 0.0),
                )
            }
        }
        return Vorhersage(ort, tage)
    }

    private fun zahlen(wert: Any?): List<Double> =
        (wert as? List<*>)?.mapNotNull { (it as? Number)?.toDouble() } ?: emptyList()

    companion object {
        private const val MAX_VERSUCHE = 3
        private val ADRESSE = """
            https://api.open-meteo.com/v1/forecast?city=%s&daily=temperature_2m_max,temperature_2m_min,weathercode,precipitation_sum
        """.trimIndent()
    }
}
