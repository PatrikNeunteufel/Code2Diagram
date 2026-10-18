package de.beispiel.notizen

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import de.beispiel.notizen.data.SpeicherRepository
import de.beispiel.notizen.ui.NotizenScreen
import de.beispiel.notizen.ui.NotizenViewModel
import de.beispiel.notizen.ui.thema.NotizenThema

class MainActivity : ComponentActivity() {

    private val viewModel: NotizenViewModel by viewModels {
        NotizenViewModel.fabrik(SpeicherRepository.erzeuge(applicationContext))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val zuletzt = savedInstanceState?.getLong(SCHLUESSEL_AUSWAHL) ?: KEINE_AUSWAHL
        if (zuletzt != KEINE_AUSWAHL) {
            viewModel.waehle(zuletzt)
        }
        setContent {
            NotizenThema {
                NotizenScreen(viewModel = viewModel, beimTeilen = ::teile)
            }
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        viewModel.auswahl.value?.let { outState.putLong(SCHLUESSEL_AUSWAHL, it.id) }
    }

    private fun teile(text: String) {
        val absicht = android.content.Intent(android.content.Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(android.content.Intent.EXTRA_TEXT, text)
        }
        startActivity(android.content.Intent.createChooser(absicht, null))
    }

    companion object {
        private const val SCHLUESSEL_AUSWAHL = "auswahl"
        const val KEINE_AUSWAHL = -1L
    }
}
