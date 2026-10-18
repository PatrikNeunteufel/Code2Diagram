package de.beispiel.notizen.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import de.beispiel.notizen.data.Ergebnis
import de.beispiel.notizen.data.Filter
import de.beispiel.notizen.data.Notiz
import de.beispiel.notizen.data.NotizId
import de.beispiel.notizen.data.NotizRepository
import de.beispiel.notizen.data.Prioritaet
import de.beispiel.notizen.util.kuerze
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

data class Anzeige(
    val notizen: List<Notiz> = emptyList(),
    val filter: Filter = Filter.Alle,
    val meldung: String? = null,
    val laedt: Boolean = false,
)

class NotizenViewModel(private val repository: NotizRepository) : ViewModel() {

    private val filter = MutableStateFlow<Filter>(Filter.Alle)
    private val meldung = MutableStateFlow<String?>(null)
    val auswahl = MutableStateFlow<Notiz?>(null)
    private var suche: Job? = null

    val anzeige: StateFlow<Anzeige> = combine(repository.notizen, filter, meldung) { liste, f, m ->
        Anzeige(notizen = liste.filter { f.passt(it) }, filter = f, meldung = m)
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), Anzeige(laedt = true))

    init {
        viewModelScope.launch {
            when (val ergebnis = repository.lade()) {
                is Ergebnis.Erfolg -> if (ergebnis.wert == 0) meldung.value = "Noch keine Notizen"
                is Ergebnis.Fehler -> meldung.value = "Laden fehlgeschlagen: ${ergebnis.grund.message}"
                Ergebnis.Laedt -> Unit
            }
        }
    }

    fun waehle(id: NotizId) {
        auswahl.value = anzeige.value.notizen.find { it.id == id }
    }

    fun setzeFilter(text: String) {
        filter.value = when {
            text.isBlank() -> Filter.Alle
            text.startsWith("#") -> Filter.MitSchlagwort(text.drop(1))
            text.equals("wichtig", ignoreCase = true) -> Filter.NachPrioritaet(Prioritaet.HOCH)
            else -> Filter.Alle
        }
    }

    fun suche(text: String) {
        suche?.cancel()
        suche = viewModelScope.launch {
            delay(300)
            setzeFilter(text)
        }
    }

    fun speichere(titel: String, text: String, prioritaet: Prioritaet) {
        val bisher = auswahl.value
        val notiz = bisher?.copy(titel = titel, text = text, prioritaet = prioritaet)
            ?: Notiz(id = repository.naechsteId(), titel = titel, text = text, prioritaet = prioritaet)
        viewModelScope.launch {
            val ergebnis = repository.lege(notiz)
            meldung.value = when (ergebnis) {
                is Ergebnis.Erfolg -> "Gespeichert: ${ergebnis.wert.titel.kuerze(20)}"
                is Ergebnis.Fehler -> ergebnis.grund.message ?: "Fehler"
                else -> null
            }
            if (ergebnis is Ergebnis.Erfolg) auswahl.value = null
        }
    }

    fun loesche(notiz: Notiz) = viewModelScope.launch {
        val geloescht = repository.loesche(notiz.id)
        meldung.value = if (geloescht) "Geloescht" else "Nicht gefunden"
    }

    /** Die erste Notiz, deren Text alle Woerter enthaelt — oder null. */
    fun findeMitAllen(woerter: List<String>): Notiz? {
        val liste = anzeige.value.notizen
        notizen@ for (notiz in liste) {
            for (wort in woerter) {
                if (wort.isBlank()) continue
                if (!notiz.text.contains(wort, ignoreCase = true)) continue@notizen
            }
            return notiz
        }
        return null
    }

    fun zaehleNachPrioritaet(): Map<Prioritaet, Int> {
        val zaehler = mutableMapOf<Prioritaet, Int>()
        anzeige.value.notizen.forEach { notiz ->
            if (notiz.istLeer) return@forEach
            zaehler[notiz.prioritaet] = (zaehler[notiz.prioritaet] ?: 0) + 1
        }
        return zaehler
    }

    fun quittiere() {
        meldung.value = null
    }

    companion object {
        fun fabrik(repository: NotizRepository): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                require(modelClass.isAssignableFrom(NotizenViewModel::class.java)) { "unbekannt: $modelClass" }
                return NotizenViewModel(repository) as T
            }
        }
    }
}
