package de.beispiel.wetter

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.ProgressBar
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import de.beispiel.wetter.daten.Einheit
import de.beispiel.wetter.daten.Vorhersage
import de.beispiel.wetter.daten.Wetterlage
import de.beispiel.wetter.netz.WetterDienst

class MainActivity : AppCompatActivity(), WetterDienst.Rueckruf {

    private lateinit var ortFeld: EditText
    private lateinit var suchen: Button
    private lateinit var status: TextView
    private lateinit var fortschritt: ProgressBar
    private lateinit var liste: RecyclerView

    private val adapter = VorhersageAdapter { tag -> zeigeDetails(tag) }
    private val haupt = Handler(Looper.getMainLooper())
    private var einheit = Einheit.CELSIUS
    private var dienst: WetterDienst? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        ortFeld = findViewById(R.id.ort)
        suchen = findViewById(R.id.suchen)
        status = findViewById(R.id.status)
        fortschritt = findViewById(R.id.fortschritt)
        liste = findViewById(R.id.liste)

        liste.layoutManager = LinearLayoutManager(this)
        liste.adapter = adapter

        suchen.setOnClickListener {
            val ort = ortFeld.text.toString().trim()
            if (ort.isEmpty()) {
                ortFeld.error = getString(R.string.ort_fehlt)
                return@setOnClickListener
            }
            starteSuche(ort)
        }
        findViewById<View>(R.id.einheit).setOnClickListener { wechsleEinheit() }

        ortFeld.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}

            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                suchen.isEnabled = !s.isNullOrBlank()
            }

            override fun afterTextChanged(s: Editable?) {
                if (s != null && s.length > MAX_ORT) {
                    s.delete(MAX_ORT, s.length)
                }
            }
        })

        savedInstanceState?.getString(SCHLUESSEL_ORT)?.let { ort ->
            ortFeld.setText(ort)
            starteSuche(ort)
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putString(SCHLUESSEL_ORT, ortFeld.text.toString())
    }

    override fun onDestroy() {
        dienst?.beende()
        haupt.removeCallbacksAndMessages(null)
        super.onDestroy()
    }

    private fun starteSuche(ort: String) {
        dienst?.beende()
        fortschritt.visibility = View.VISIBLE
        status.text = getString(R.string.suche_laeuft, ort)
        dienst = WetterDienst(ort, this).also { it.starte() }
    }

    private fun wechsleEinheit() {
        einheit = when (einheit) {
            Einheit.CELSIUS -> Einheit.FAHRENHEIT
            Einheit.FAHRENHEIT -> Einheit.KELVIN
            Einheit.KELVIN -> Einheit.CELSIUS
        }
        adapter.einheit = einheit
    }

    override fun fertig(vorhersage: Vorhersage) {
        haupt.post {
            fortschritt.visibility = View.GONE
            adapter.zeige(vorhersage.tage)
            status.text = when (val lage = vorhersage.heute()) {
                null -> getString(R.string.keine_daten)
                is Wetterlage.Klar -> "${vorhersage.ort}: klar, ${einheit.formatiere(lage.temperatur)}"
                is Wetterlage.Regen -> "${vorhersage.ort}: Regen (${lage.millimeter} mm)"
                is Wetterlage.Gewitter -> "${vorhersage.ort}: Gewitter!"
                Wetterlage.Unbekannt -> vorhersage.ort
            }
        }
    }

    override fun fehler(grund: String, nochmal: Boolean) {
        haupt.post {
            fortschritt.visibility = View.GONE
            status.text = grund
            if (nochmal) {
                haupt.postDelayed({ dienst?.starte() }, NEUER_VERSUCH_MS)
            }
        }
    }

    private fun zeigeDetails(tag: Vorhersage.Tag) {
        val text = buildString {
            appendLine("Hoch: ${einheit.formatiere(tag.hoch)}")
            appendLine("Tief: ${einheit.formatiere(tag.tief)}")
            if (tag.wind > 0) append("Wind: ${tag.wind} km/h")
        }
        AlertDialog.Builder(this)
            .setTitle(tag.datum)
            .setMessage(text)
            .setPositiveButton(android.R.string.ok, null)
            .show()
    }

    companion object {
        private const val SCHLUESSEL_ORT = "ort"
        private const val MAX_ORT = 60
        private const val NEUER_VERSUCH_MS = 5_000L
    }
}
