# Ernte aus den Vorgängern

Stand 2026-10-13. Die Vorgänger liegen unter `../old_Code2Mermaid/` (im Folgenden `alt/`). Der
eigene Code und die Doku der vier Anläufe wurden gelesen (leere Stubs in `C2Mermaid`
stichprobenweise, `plan.pdf` nur angerissen), `Framework` nach Umfang, Aufbau und Stichproben;
keiner wurde gebaut oder ausgeführt. Die Zeilenangaben stammen aus vier Analyse-Berichten und
sind nicht einzeln nachgeprüft — vor dem Zitieren nachschlagen.

## Befund

**Keiner der Anläufe hat je ein Diagramm erzeugt.** Es gibt in keinem einen Generator, in keinem
einen Parser, der läuft, und keine einzige erzeugte Mermaid-Datei. Übernommen sind deshalb
Ideen, Daten und Lehren — kein Code.

| Anlauf | Zeitraum | Was da ist | Woran er endete |
|---|---|---|---|
| `C2Mermaid` | 23.01.–16.02.2025 | 15 Visual-Studio-Projekte, eigener JSON-Parser (fehlerhaft), Win32-Fenster, Kopie von Microsoft WIL; Lexer, Parser, Generator sind leere Dateien | Umbau der Struktur, der Tokenizer-Code wurde nicht mitgenommen |
| `C2MermaidV3` | 05.–13.02.2025 | Lexer von Hand (ohne Kommentare, ohne Positionen), Regex-Muster-Parser (nicht im Build, übersetzt nicht), Konfiguration in JSON, von Hand zeilenweise gelesen | abgebrochene Umbenennung Tokenizer → Lexer; der eigene Test kann nicht bestehen |
| `C2MermaidV4` | 13.–16.02.2025 | nur Köpfe; die `.cpp` dazu sind leer | Neuaufsetzen der Struktur, Implementierung nie nachgezogen |
| `Code2Mermaid` | 13.02.–09.03.2025 | Win32-Fenster auf einer eigenen Bibliothek; kein Analyse-Code | sechs Wochen Arbeit an der Fensterbibliothek |
| `Framework` | 13.02.–21.04.2025 | diese Fensterbibliothek: rund sechzig Win32-Bedienelemente (Button, ListView, Dialoge …) je als Klasse und Impl — 186 `.cpp` mit zusammen 69 KB, die grösste Quelldatei 1,4 KB, also durchweg leere Hüllen | das Gerüst steht, kein Element tut etwas |

**Zu `Framework`:** kein eigenes Projekt daraus. Was es werden sollte, ist Qt; fachlich für
Code2Diagram enthält es nichts. Die „allgemeine Basis", die dieses Projekt braucht, ist eine
andere — die Bibliothek `C2dKern` (Lexer, Codemodell, Leser, Diagramme, Formate), ohne Qt und
später herauslösbar.

## Die Lehren — sie bestimmen den Aufbau hier

1. **Erst ein Durchstich, dann die Breite.** Jeder Anlauf baute zuerst Infrastruktur (Module,
   Konfiguration, vier Sprachen, eigenes Fenster-Framework) und kam nie bei einem `if/else` an.
   Hier gilt: jede Schicht entsteht mit dem, was sie durchreicht — eine Sprache (C), bis zum
   fertigen Text, mit Tests; erst dann die nächste.
2. **Kein Parser aus Regex-Mustern in JSON.** Muster wie `"if ({condition})"` fassen keine
   Schachtelung, keine Blöcke ohne `{}`, keine mehrzeiligen Anweisungen
   (`alt/C2MermaidV3/C2MermaidV3/Source/Parser/Parser.cpp:103-127`). Der Leser hier ist
   rekursiver Abstieg von Hand.
3. **Zwischen Analyse und Ausgabe gehören Modelle.** Alle wollten vom AST direkt Mermaid
   schreiben. Hier liegen zwei dazwischen: das Codemodell (was die Datei enthält) und das
   Diagramm-Modell (was gezeichnet wird). Erst sie machen weitere Sprachen und weitere Formate
   unabhängig voneinander möglich.
4. **Kein Head/Body-Containermodell für Blöcke** — es erzeugte leere Köpfe und falsch
   geschachtelte Bodies (`alt/C2Mermaid/Documents/GPT/megaprompt2.txt:2`). Das Flussdiagramm
   hier ist ein Graph aus Knoten und Kanten.
5. **Regeln im Code, nicht in Konfiguration.** „Alles per JSON, ohne Neukompilieren, für mehrere
   Sprachen" hat den Parser vor dem ersten Ergebnis überfrachtet. Die Erweiterbarkeit, die damit
   gemeint war, gibt es hier über Schnittstellen (`Sprache`, `Format`) und Tabellen im Code.
6. **Nichts still verwerfen.** Die alten Lexer liessen unbekannte Zeichen fallen. Hier wird, was
   nicht verstanden ist, eine Meldung mit Zeile und Spalte.
7. **Tests in einem Test-Target**, nicht im Konstruktor der Anwendung mit einer Erfolgsmeldung,
   die unbedingt ausgegeben wird (`alt/C2MermaidV3/.../Testing/Testing.cpp:35`).
8. **Qt statt eigenem Fenster-Framework.** Drei Anläufe schrieben Win32-Fenster von Hand, zwei
   Ordner sind eine ganze Bibliothek dafür.

## Was übernommen ist

Pfade ohne Vorsatz liegen unter `projects/libs/C2dKern/`.

| Was | Woher | Wo es jetzt steht |
|---|---|---|
| Schichtung Laden → Lexer → Parser → Generator, „allgemein mit Einzelheiten je Sprache" | Ordnerschnitt und Regelwerk von V3/V4 (`alt/C2MermaidV3/C2MermaidV3/Projekt_Regelwerk.md:54-73`) | `lexer/`, `sprache/`, `diagramm/`, `ausgabe/` — dazu die zwei Modelle, die fehlten |
| Lexer, den eine Beschreibung der Sprache steuert (Schlüsselwörter, Operatoren, Präprozessor) | `alt/C2MermaidV4/C2MermaidV4/Source/Common/LanguageFeatures.h:12-36` | `lexer::Sprachtabelle` — als Tabelle im Code der Sprache |
| Einteilung der Token | `ETokenType`, `alt/C2MermaidV4/C2MermaidV4/Source/Lexer/Lexer.h:20-47` | `lexer::TokenArt`, ergänzt um Ort, Kommentar, Zeichenliteral, Direktive, Unbekannt |
| Schlüsselwörter und Operatoren von C | `alt/C2MermaidV4/C2MermaidV4/config/lexer/C_keywords.json` | `src/sprache/c/CSprache.cpp`, bereinigt auf das, was C11 ein Schlüsselwort nennt |
| Umfang der Konstrukte als Checkliste | `alt/C2MermaidV4/C2MermaidV4/config/parser/C_parsing.json:4-181` | alle gelesen; dazu, was dort fehlte: `else`, `do`-`while`, `case`/`default`, `break`/`continue`, `goto`/Marken, `return` |
| Ein Name, den die Datei nicht definiert, gilt als Typ | `alt/C2MermaidV3/C2MermaidV3/Projekt_Regelwerk.md:123` | `CLeser::zerlegeVoll` — der letzte Bezeichner ist der Name, alles davor der Typ |
| Aufeinanderfolgende Anweisungen zu einem Knoten bündeln („AssignmentBlock") | `alt/C2MermaidV3/C2MermaidV3/optimierung parsing json.md:55-87` | `fasseAnweisungsfolgenZusammen()`, mit Obergrenze je Knoten |
| Zuordnung Knotenart → Form (Raute für Bedingungen, Rechteck für Anweisungen), Kanten `yes`/`no` | `alt/C2MermaidV3/C2MermaidV3/JSON-Regelwerk.md:116-133` | `formVon()` in `src/ausgabe/Mermaid.cpp` und `Dot.cpp`; die Kanten in `src/diagramm/Ableitung.cpp` |
| Eindeutige Knoten-IDs | `alt/C2MermaidV3/C2MermaidV3/Projekt_Regelwerk.md:72` | die Kennung ist die Stelle des Knotens, Ausgabe `n0`, `n1`, … |
| Mehr als eine Diagrammart (Fluss, Klassen …) | `Projekt_Regelwerk.md:7-13` | `Flussdiagramm`, `Strukturdiagramm` |
| Dateifilter, Datei per Kommandozeile, Menü File / Help | `alt/Code2Mermaid/MainApp/Source/Entrypoint/Main.cpp:28-41, 124-133, 197-222` | `ui/Hauptfenster.cpp`, `Application::init` (unter `projects/apps/Code2Diagram/`) |

## Was noch kommt

- **Testschnipsel der alten Lexer-Tests** als weitere Testdaten
  (`alt/C2MermaidV3/C2MermaidV3/Source/Testing/LexerTest_C.cpp:32-35, 82-87`,
  `LexerTest_CPP.cpp:30-32, 87`) — mit neuen Erwartungen; `LexerTest_CPP.cpp:60` erwartet `::`
  für `numbers.size()`, das ist ein Fehler im Test.
- **C++ ist da** (`src/sprache/cpp/CppSprache.cpp`): die Schlüsselwörter aus
  `CPP_keywords.json`, bereinigt (`NULL` und `defined` sind keine), und die Konstrukte aus
  `CPP_parsing.json` als Checkliste — class, namespace, Konstruktor/Destruktor, try/catch/throw,
  using sind gelesen; `template` wird übergangen, ein Lambda bleibt Text seiner Anweisung.
- **Anforderungen:** Kommentare und Präprozessor im Diagramm, Scopes
  (`alt/C2Mermaid/Documents/plan.pdf` S. 1–2); Zustands- und Sequenzdiagramm
  (`Projekt_Regelwerk.md:7-13`); automatisches Neuladen, wenn sich die Quelldatei ändert
  (`alt/Code2Mermaid/MainApp/Source/Application/MyApplication.cpp:119-121`); mehrere Dateien
  auf einmal einlesen (`Main.cpp:142-185`).

## Was ausdrücklich nicht übernommen wird

- der Regex-Muster-Parser und die „Grammatik in JSON" (`config/rules/*.json`,
  `config/parser/*.json`); `CPP_parsing.json` ist wegen `//`-Kommentaren nicht einmal gültiges JSON
- der eigene JSON-Parser aus `C2Mermaid` (hängt geschachtelte Container nie ein) und der
  zeilenweise `ConfigLoader`
- die Lexer als Code: keine Kommentare, keine Quellposition am Token, unbekannte Zeichen werden
  still verworfen, Konfiguration als globaler statischer Zustand
- `AST.h` mit rohen besitzenden Zeigern; `VariableNode::m_type` verdeckt das Feld der Basis
- alles Win32 (Fenster, `CallbackManager`, `MenuManager`, der ganze Ordner `Framework`),
  `Debugger`/`ErrorHandler`/`ErrorLogger` aus einem fremden Projekt, die WIL-Kopie
- JavaScript und Python als frühes Ziel
- die `.vcxproj`/`.sln` (bis zu 85 Konfigurationen aus einer Vorlage)
