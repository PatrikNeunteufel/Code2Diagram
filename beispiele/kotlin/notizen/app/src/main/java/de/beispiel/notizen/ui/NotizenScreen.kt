package de.beispiel.notizen.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import de.beispiel.notizen.data.Notiz
import de.beispiel.notizen.data.Prioritaet
import de.beispiel.notizen.util.alsDatum

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun NotizenScreen(viewModel: NotizenViewModel, beimTeilen: (String) -> Unit) {
    val anzeige by viewModel.anzeige.collectAsState()
    val auswahl by viewModel.auswahl.collectAsState()
    var bearbeiten by remember { mutableStateOf(false) }
    val snackbar = remember { SnackbarHostState() }

    LaunchedEffect(anzeige.meldung) {
        val text = anzeige.meldung ?: return@LaunchedEffect
        snackbar.showSnackbar(text)
        viewModel.quittiere()
    }

    Scaffold(
        topBar = { TopAppBar(title = { Text("Notizen (${anzeige.notizen.size})") }) },
        snackbarHost = { SnackbarHost(snackbar) },
        floatingActionButton = {
            FloatingActionButton(onClick = {
                viewModel.auswahl.value = null
                bearbeiten = true
            }) {
                Icon(Icons.Default.Add, contentDescription = "Neu")
            }
        },
    ) { innen ->
        Column(Modifier.padding(innen).fillMaxSize()) {
            Suchfeld(beiEingabe = viewModel::suche)
            if (anzeige.laedt) {
                Text("Laedt …", Modifier.padding(16.dp))
            } else {
                NotizListe(
                    notizen = anzeige.notizen,
                    beiWahl = { notiz ->
                        viewModel.waehle(notiz.id)
                        bearbeiten = true
                    },
                    beiLangDruck = { beimTeilen(it.text) },
                )
            }
        }
    }

    if (bearbeiten) {
        Bearbeiten(
            notiz = auswahl,
            beiFertig = { titel, text, prioritaet ->
                viewModel.speichere(titel, text, prioritaet)
                bearbeiten = false
            },
            beiAbbruch = { bearbeiten = false },
        )
    }
}

@Composable
private fun Suchfeld(beiEingabe: (String) -> Unit) {
    var text by remember { mutableStateOf("") }
    TextField(
        value = text,
        onValueChange = {
            text = it
            beiEingabe(it)
        },
        modifier = Modifier.fillMaxWidth().padding(8.dp),
        placeholder = { Text("Suchen, #schlagwort oder wichtig") },
    )
}

@Composable
private fun NotizListe(notizen: List<Notiz>, beiWahl: (Notiz) -> Unit, beiLangDruck: (Notiz) -> Unit) {
    if (notizen.isEmpty()) {
        Text("Nichts gefunden", Modifier.padding(16.dp))
        return
    }
    LazyColumn {
        items(notizen, key = { it.id }) { notiz ->
            Card(
                Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp, vertical = 4.dp)
                    .clickable { beiWahl(notiz) },
            ) {
                Row(Modifier.padding(12.dp)) {
                    Text(
                        when (notiz.prioritaet) {
                            Prioritaet.HOCH -> "!!"
                            Prioritaet.NORMAL -> "!"
                            Prioritaet.NIEDRIG -> " "
                        },
                    )
                    Spacer(Modifier.width(8.dp))
                    Column {
                        Text(notiz.titel)
                        Text(notiz.erstellt.alsDatum())
                    }
                }
            }
        }
    }
}

@Composable
private fun Bearbeiten(
    notiz: Notiz?,
    beiFertig: (String, String, Prioritaet) -> Unit,
    beiAbbruch: () -> Unit,
) {
    var titel by remember(notiz) { mutableStateOf(notiz?.titel.orEmpty()) }
    var text by remember(notiz) { mutableStateOf(notiz?.text.orEmpty()) }
    var prioritaet by remember(notiz) { mutableStateOf(notiz?.prioritaet ?: Prioritaet.NORMAL) }
    AlertDialog(
        onDismissRequest = beiAbbruch,
        title = { Text(if (notiz == null) "Neue Notiz" else "Notiz bearbeiten") },
        text = {
            Column {
                TextField(value = titel, onValueChange = { titel = it }, label = { Text("Titel") })
                TextField(value = text, onValueChange = { text = it }, label = { Text("Text") })
                Row {
                    for (p in Prioritaet.entries) {
                        TextButton(onClick = { prioritaet = p }) {
                            Text(if (p == prioritaet) "[${p.name}]" else p.name)
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = { beiFertig(titel, text, prioritaet) }, enabled = titel.isNotBlank()) {
                Text("Speichern")
            }
        },
        dismissButton = { TextButton(onClick = beiAbbruch) { Text("Abbrechen") } },
    )
}
