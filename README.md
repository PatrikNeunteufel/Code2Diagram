# Code2Diagram

**Code2Diagram** liest Quellcode und leitet daraus gut lesbare Diagramme ab — gezeichnet im
Fenster und als Text in einem wählbaren Format.

| | heute | angelegt für |
|---|---|---|
| **Sprachen** | C, C++ | weitere: je Sprache eine Tabelle und ein Leser |
| **Diagrammarten** | Ablauf einer Funktion (Flussdiagramm), Datentypen (struct, union, enum, Klassen mit Methoden, Vererbung und Beziehungen) | weitere: je Art ein Modell und eine Ableitung |
| **Formate** | Mermaid, Graphviz DOT, draw.io | weitere: je Format eine Klasse |
| **Ansehen** | Vorschau im Fenster, mit Sprung zur gerufenen Funktion und Einbetten ihres Ablaufs | |

Lexer, Leser und Anordnung sind Eigenbau, ohne Fremdbibliothek.

**Gebaut wird mit [CMakeCraft](https://github.com/PatrikNeunteufel/CMakeCraft):** das
Build-System liegt nicht im Repo, es wird beim Configure in der Version aus `cmakecraft.pin`
geholt. Was gebaut wird, steht allein in `Solution.json`.

**Sichttests laufen mit [SichtTest_Helper](https://github.com/PatrikNeunteufel/SichtTest_Helper):**
das fertige Paket (Version aus `sichttest.pin`) kommt als External der Art `archive`; mit
`--testing` lädt die Anwendung dessen DLL, und der Tester `Sichttest.exe` kann sie steuern.

## Was gebaut wird

| Target | Was |
|---|---|
| `C2dKern` | die Bibliothek mit allem Fachlichen — **ohne Qt** |
| `c2d` | Kommandozeile: Quelldatei rein, Diagramm-Text raus |
| `Code2Diagram` | das Fenster (Qt 6) |
| `C2dKern_UnitTests` | Tests des Kerns (doctest) |
| `Code2Diagram.UnitTests` | Tests des Fensters (doctest, ohne es zu zeigen) |

## Der Kern: eine allgemeine Basis, je Sprache und Format die Einzelheiten

```
Text ──Lexer──▶ Token ──Leser der Sprache──▶ Codemodell ──Ableitung──▶ Diagramm-Modell ──┬─ Format ───▶ Text
        │                    │                    │                        │              └─ Anordnung ─▶ Orte (Vorschau, draw.io)
   für alle gleich      je Sprache          für alle gleich          je Diagrammart
```

| Schicht (`projects/libs/C2dKern/…/c2d/`) | Inhalt | kennt |
|---|---|---|
| `quelle/` | Ort im Quelltext, Meldungen | — |
| `lexer/` | der allgemeine Lexer, gesteuert von einer `Sprachtabelle` (Schlüsselwörter, Operatoren, Kommentare, Direktiven, rohe Zeichenketten) | quelle |
| `modell/` | das **Codemodell**: Funktionen mit Anweisungen, Typen mit Gliedern und Methoden, Aliase, Variablen, Einbindungen, Makros | — |
| `sprache/` | je Sprache eine Tabelle; gelesen wird mit einem Leser in rekursivem Abstieg. C und C++ teilen den **Leser der C-Familie** (`cfamilie/`) | lexer, modell |
| `diagramm/` | die Diagramm-Modelle (`Flussdiagramm`, `Strukturdiagramm`), ihre Ableitung aus dem Codemodell, Einbetten, und die **Anordnung** (wo was steht) | modell |
| `ausgabe/` | die Formate | diagramm |

Die Trennlinien sind der Punkt: ein Format kennt keine Sprache, ein Leser kennt kein Diagramm.

- **Neue Sprache:** Ordner `src/sprache/<name>/` mit ihrer Tabelle und einer Klasse von
  `Sprache`, in `src/sprache/Sprache.cpp` in die Liste eintragen. Fenster und `c2d` finden sie von
  selbst (Dateifilter, Wahl nach Endung). Gehört sie zur C-Familie, genügt die Tabelle: ihre
  Schlüsselwörter schalten im gemeinsamen Leser frei, was sie über C hinaus hat. Sonst bekommt
  sie einen eigenen Leser auf der Basis `Leser` (Klammern, Suchen, Text eines Bereichs). Fehlt dem
  Codemodell etwas, wird es dort ergänzt.
- **Neues Format:** eine Klasse von `ausgabe::Format` ableiten, in `src/ausgabe/Ausgabe.cpp`
  eintragen. Braucht es feste Orte (wie draw.io), nimmt es sie aus `diagramm/Anordnung.hpp`.
- **Neue Diagrammart:** ein Modell in `diagramm/`, eine Funktion in `Ableitung.hpp`, eine in
  `Anordnung.hpp`, eine weitere `schreibe` in `ausgabe::Format` — der Übersetzer nennt dann jedes
  Format, das sie noch nicht kann.

Regeln stehen als Tabellen im Code, nicht in Konfigurationsdateien (siehe
[docs/Ernte_Vorgaenger.md](docs/Ernte_Vorgaenger.md), Lehre 5).

## Was die Leser verstehen

**C und C++ gemeinsam:**

- Funktionen mit Rumpf und Prototypen, auch `ISR(TIMER1_vect) { … }` und Köpfe mit
  `__attribute__((…))`
- `if`/`else`, `while`, `do`-`while`, `for`, `switch` mit `case`/`default` und Durchfallen,
  `break`, `continue`, `return`, `goto` und Marken
- `struct`, `union`, `enum` — verschachtelt, ohne Etikett, mit Bitfeldern, Feldern,
  Funktionszeigern, gepackt mit Makro-Wörtern (`typedef struct PACKED name { … }`)
- `typedef`, globale Variablen (mehrere Deklaratoren teilen den Typ), `#include`, `#define`,
  `extern "C" { … }`
- Makros, die wie Anweisungen stehen: `Q_UNUSED(x)` ohne Semikolon, `SUBCASE("…") { … }`; ein
  Makro allein auf seiner Zeile (`PACKED_BEGIN`, `Q_OBJECT`); `else` hinter einem `#ifdef`

**C++ dazu:**

- Namensräume (auch `a::b`, `inline`), `using Name = Typ`
- Klassen und `struct` mit Basen, Sichtbarkeit (`public`/`protected`/`private`, Qt: `signals`,
  `slots`), Gliedern und Methoden — `virtual`, `static`, abstrakt (`= 0`); `enum class`
- Konstruktoren mit Anfangswerten der Glieder, Destruktoren, Operatoren (`operator==`,
  `operator()`, `operator=`), im Typ und außerhalb definiert (`void Motor::starte()`)
- `try`/`catch`, `throw`, `for (Element : Menge)`, `if constexpr`, `co_return`
- Schablonen in Typen (`std::map<int, std::vector<T>>`), rohe Zeichenketten, `1'000`,
  `[[attribute]]`, `noexcept`, `override`, `-> Typ`

Ein Kopf mit der Endung `.h` wird als C++ gelesen, wenn er etwas nennt, das es in C nicht gibt
(`class`, `namespace`, `template`, `::` …), sonst als C.

**Bedingte Übersetzung** (`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`) wird
ausgewertet, bevor gelesen wird: der Leser sieht nur die Zweige, die gelten. Gerechnet wird wie
im Compiler — mit den `#define` und `#undef` der Datei bis zu dieser Stelle und mit
Definitionen, die man angibt; ein Name, der nicht definiert ist, zählt 0; in C++ ist
`__cplusplus` definiert. Zeilen und Spalten bleiben die der Datei.

- Wonach eine Bedingung gefragt hat und was nirgends definiert ist, wird genannt (Statuszeile,
  `c2d --liste`): so sieht man, welche Variante gelesen wurde und was sich wählen lässt.
- Eine Bedingung, die sich nicht ausrechnen lässt (ein Makro mit Parametern, `__has_include`),
  gibt einen Hinweis; ihr Zweig wird gelesen.
- Der Schalter „alle Zweige" liest wie vor der Auswertung jeden Zweig, hintereinander.

Ein Name, den die Datei nicht kennt, gilt vor einem Bezeichner als Typ — ein `typedef` aus einer
anderen Datei muss also nicht bekannt sein. Was der Leser nicht versteht, übergeht er **mit einer
Meldung** (Zeile, Spalte); das Ergebnis bleibt brauchbar.

## Aufruf

### Fenster

```bash
Code2Diagram [--testing] [datei] [-D NAME[=wert]]... [--alle-zweige] [--diagramm <name>] [--einbetten <funktion>]... [--format <name>] [--ansicht preview|text] [--bild <datei.png>]
```

- Unter dem Quelltext: **Defines** — was beim Auswerten von `#if`/`#ifdef` zusätzlich als
  definiert gilt (`NAME` oder `NAME=wert`, durch Leerzeichen getrennt; Enter liest neu) — und
  der Haken **All #if branches**. Die Statuszeile nennt, wonach gefragt wurde und was nicht
  definiert ist.
- Rechts oben: **Diagram** (eine Funktion, mit Bereich wie `Klasse::methode`, oder die
  Datentypen), **Format**, Richtung. Darunter zwei Ansichten: **Preview** zeichnet das Diagramm,
  **Text** zeigt es im gewählten Format. File > Save diagram as… legt den Text ab.
- **In der Vorschau** (Strg + Mausrad vergrößert, Ziehen verschiebt):
  - Ein Knoten, der eine Funktion der Datei ruft, ist farbig umrandet. **Doppelklick** zeigt deren
    Diagramm; Diagram > Back (Alt + Pfeil links) führt zurück.
  - **Rechtsklick** auf so einen Knoten: *Embed …() here* setzt den Ablauf der gerufenen Funktion
    umrahmt hinter den Aufruf — auch verschachtelt, in einer Einbettung weiter. Rechtsklick in
    einen Rahmen: *Remove embedding*. Diagram > Remove all embeddings nimmt alle heraus.
  - Doppelklick auf einen anderen Knoten zeigt seine Zeile im Quelltext.
  - Was eingebettet ist, steht auch im Text: als `subgraph` (Mermaid), `cluster` (DOT),
    Behälter (draw.io).
- `--bild` legt das Fenster als PNG ab und endet — ohne Datei mit dem eingebauten Beispiel.
- `--testing` lädt die Sichttest-Steuerung. Aktionen: `datei_oeffnen pfad=<Datei>` (relativ ab
  dem Projekt-Root), `beispiel_zeigen`, `diagramm_waehlen name=<Funktion|types>`,
  `format_waehlen name=<mermaid|dot|drawio>`, `ansicht_waehlen name=<preview|text>`,
  `einbetten name=<Funktion>`, `einbettungen_loesen`, `zurueck`,
  `definitionen_setzen wert=<NAME NAME=wert …>`, `zweige_waehlen name=<auswerten|alle>`.

### Kommandozeile

```bash
c2d <datei> [--liste] [--funktion <name>] [--einbetten <name>]... [--struktur] [-D NAME[=wert]]... [--alle-zweige] [--format mermaid|dot|drawio] [--richtung TD|LR] [--roh]
```

- ohne Auswahl: das Diagramm der ersten Funktion mit Rumpf; gibt es keine, die Datentypen
- `--liste` nennt, was die Datei enthält — und als `undefined`, wonach ein `#if` gefragt hat
- `-D NAME` oder `-D NAME=wert` (auch `-DNAME`): gilt beim Auswerten als definiert, ohne Wert
  als 1; `--alle-zweige` wertet nicht aus
- `--funktion`: der Name allein (`starte`) oder mit Bereich (`antrieb::Motor::starte`)
- `--roh`: je Anweisung ein Knoten, nichts gebündelt
- das Diagramm geht auf die Standardausgabe, Meldungen auf die Fehlerausgabe

```bash
c2d beispiele\aufrufe.c --funktion main --einbetten summe --format drawio > main.drawio
```

## Aufbau

```
Code2Diagram/
├── CMakeLists.txt              Einstieg, holt CMakeCraft; reicht Name und Version an den Code
├── CMakeCraftBootstrap.cmake   Bezug des Build-Systems (unverändert aus CMakeCraft)
├── cmakecraft.pin              gepinnte CMakeCraft-Version
├── sichttest.pin               gepinntes Paket der Sichttest-Steuerung
├── CMakePresets.json           Presets (aus CMakeCraft)
├── CMakeUserPresets.json       lokal: Pfade (nicht versioniert; Vorlage: *.example.json)
├── Solution.json               was gebaut wird, Name und Version (SSOT)
├── .sichttest/                 Projektdatei für den Tester
├── beispiele/                  kleine Quelldateien zum Ausprobieren und für Tests
├── docs/
│   ├── Ernte_Vorgaenger.md     was aus den alten Anläufen übernommen ist und was nicht
│   └── sichttest/              Sichttest-Listen
├── externals/doctest/          Test-Framework (ein Kopf)
├── tools/bau_clang.bat         konfigurieren und bauen in der Umgebung von Visual Studio 18
└── projects/
    ├── libs/C2dKern/           der Kern: include/c2d/, src/, tests/unit/
    ├── exec/c2d/               die Kommandozeile
    └── apps/Code2Diagram/      der App-Container
        ├── include/, src/
        │   ├── ui/             das Fenster und die Vorschau
        │   └── sichttest/      Anbindung an den Tester (nur mit --testing)
        ├── main/, pch/
        └── tests/unit/UnitTests/
```

Lokal und nicht versioniert: `out/`, `.externals/`, `CMakeUserPresets.json`, `CLAUDE.md`,
`.claude/`, `sichttest-logs/`.

## Bauen

Voraussetzung: CMake ab 3.26, Ninja, Clang, Visual Studio 18 (2026); `QT_ROOT` zeigt auf ein
Qt-6-Kit (Umgebungsvariable oder `CMakeUserPresets.json`, etwa `C:/Qt/6.10.1/msvc2022_64`).

```bash
tools\bau_clang.bat
```

Das Skript betritt die Umgebung von Visual Studio 18, konfiguriert
(`cmake --preset windows-ninja-release-clang`) und baut alle Targets
(`cmake --build --preset build-ninja-release-clang`). Ein einzelnes Ziel:
`tools\bau_clang.bat C2dKern_UnitTests`.

Alles liegt unter `out/build/windows-ninja-release-clang/exec/<Target>/bin/Release/`.

## Tests

```bash
out\build\windows-ninja-release-clang\exec\C2dKern_UnitTests\bin\Release\C2dKern_UnitTests.exe
```

```bash
out\build\windows-ninja-release-clang\exec\Code2Diagram.UnitTests\bin\Release\Code2Diagram.UnitTests.exe
```

„Grün" heisst je Lauf: `Status: SUCCESS!`, kein Fall übersprungen.

Sichttest: `Code2Diagram.exe --testing` startet den Tester mit den Listen aus `docs/sichttest/`.
Den Tester danach schliessen — offen sperrt er den nächsten Bau.

## Was es noch nicht kann

- **Vom Präprozessor nur die Bedingungen:** Makros werden im Code nicht ersetzt (`MAX(a, b)`
  bleibt stehen), `#include` wird nicht verfolgt — eine Definition aus einem anderen Kopf kennt
  die Auswertung nur, wenn man sie angibt. Die Varianten erscheinen nicht im Diagramm: gezeigt
  wird die eine, die gilt.
- **C++-Schablonen** werden übergangen, nicht verstanden: `template<…>` vor einer Deklaration
  fällt weg, eine Spezialisierung heisst wie ihr Text. Überladene Funktionen: im Fenster steht
  die erste zur Wahl. Lambdas sind Text ihrer Anweisung, kein eigener Ablauf.
- **Aufrufe werden am Namen erkannt**, ohne Typen: `a.starte()` führt zur ersten Funktion
  `starte` der Datei, gleich welcher Klasse. Nur Funktionen derselben Datei.
- **Die Anordnung ist einfach:** Schichten, Schwerpunkt, Kanten zurück außen herum. Kreuzungen
  vermeidet sie nicht immer; ein Rahmen kann einen fremden Knoten derselben Schicht umfassen.
- **Nur eine Datei:** keine Beziehungen über Dateien hinweg, kein Aufrufgraph.
- Ein Funktionszeiger als Glied gibt keine Beziehung zu den Typen seiner Parameter.
- Code, den kein Weg erreicht (hinter `return`), steht als unverbundener Knoten im Diagramm.
- Gebaut und geprüft ist nur Windows mit Release-clang. Die DOT-Ausgabe ist gegen erwarteten
  Text geprüft, nicht mit Graphviz; die draw.io-Ausgabe als wohlgeformtes XML, nicht in draw.io.
