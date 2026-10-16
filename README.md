# Code2Diagram

**Code2Diagram** liest Quellcode und leitet daraus gut lesbare Diagramme ab — gezeichnet im
Fenster und als Text in einem wählbaren Format.

| | heute | angelegt für |
|---|---|---|
| **Sprachen** | C, C++ | weitere: je Sprache eine Tabelle und ein Leser |
| **Diagrammarten** | Ablauf einer Funktion (Flussdiagramm), Datentypen (struct, union, enum, Klassen mit Methoden, Vererbung und Beziehungen), Zustandsautomaten (im Code erkannt) | weitere: je Art ein Modell und eine Ableitung |
| **Formate** | SVG (das eigene, die Vorgabe), Mermaid, Graphviz DOT, draw.io | weitere: je Format eine Klasse |
| **Ansehen** | Vorschau im Fenster, mit Sprung zur gerufenen Funktion und Einbetten ihres Ablaufs | |
| **Einlesen** | eine Datei, oder einen Ordner als Projekt (mit Unterordnern; Projektparameter in `code2diagram.json`) — Sprung und Einbetten auch über Dateien hinweg | |

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
| `c2d` | Kommandozeile: Quelldatei oder Ordner rein, Diagramm-Text raus |
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
| `projekt/` | ein Ordner als **Projekt**: die Projektdatei `code2diagram.json`, das Durchsuchen der Ordner, das Lesen in zwei Durchgängen (die `#define` eingebundener Köpfe), das Auflösen eines Aufrufs über Dateien hinweg | sprache, modell |

Die Trennlinien sind der Punkt: ein Format kennt keine Sprache, ein Leser kennt kein Diagramm,
das Projekt kennt kein Diagramm.

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
- Der Schalter **„Varianten"** zeigt ein `#if` im Rumpf einer Funktion im Ablauf: ein
  **Sechseck** `#if`, je Zweig eine Kante mit seiner Bedingung (`#ifdef A` → `defined(A)`,
  `#else` → `else`; fehlt das `#else`, führt eine Kante `else` gleich weiter); der Zweig, der mit
  den Definitionen gilt, trägt `(active)`. Das gilt für ein `#if`, dessen Zweige je **ganze
  Anweisungen** enthalten: die Klammern gehen im Zweig auf, er beginnt nicht mit `else`, `case`
  oder `default` und enthält keine Fall-Marke, endet auf `;` oder `}`; davor endet eine
  Anweisung (also nicht der Rumpf eines `if` ohne Klammern), dahinter steht kein `else`; ein
  `#if` darin erfüllt das selbst. Jedes andere `#if` im Rumpf wird ausgewertet und bekommt einen
  Hinweis. Außerhalb von Funktionen wird immer ausgewertet. Gelesen wird dafür zweimal (erst
  ausgewertet, um die Rümpfe zu kennen). Jeder Zweig, der etwas enthält, steht in einem
  **farbigen Rahmen** mit seiner vollen Bedingung (`#elif B` nach `#ifdef A`:
  `!defined(A) && B`).
- Der Schalter **„Rahmen"** wertet aus wie sonst, umrahmt aber, was ein solches `#if`
  zuschaltet — den Zweig, der gilt, mit seiner vollen Bedingung. Je Bedingung eine Farbe
  (dieselbe Bedingung in derselben Funktion: dieselbe Farbe), rechts oben eine Legende. Passt
  ein Titel nicht in seinen Rahmen, wird er gekürzt; ganz steht er in der Legende (und in der
  Vorschau im Tooltip).

Ein Name, den die Datei nicht kennt, gilt vor einem Bezeichner als Typ — ein `typedef` aus einer
anderen Datei muss also nicht bekannt sein. Was der Leser nicht versteht, übergeht er **mit einer
Meldung** (Zeile, Spalte); das Ergebnis bleibt brauchbar.

## Aufruf

### Fenster

```bash
Code2Diagram [--testing] [datei|ordner] [--datei <pfad>] [-D NAME[=wert]]... [--alle-zweige | --varianten | --rahmen] [--diagramm <name>] [--einbetten <funktion>]... [--springe <aufruf>]... [--zurueck] [--format <name>] [--ansicht preview|text] [--bild <datei.png>] [--szene <datei.png>]
```

- **Eine Datei oder ein Ordner:** File > Open… liest eine Datei, File > Open folder…
  (Strg + Umschalt + O) einen Ordner samt Unterordnern als Projekt. Dann steht über dem
  Quelltext der **Baum der Dateien**; zur Wahl stehen die Diagramme der gewählten Datei.
  Übergangene Ordner stehen grau dabei. Rechtsklick auf einen Ordner: *Skip this folder*
  trägt ihn in die Projektdatei ein; auf einen übergangenen: *Read this folder again*.
  `--datei` wählt beim Start die Datei im Projekt.
- **Die Projektdatei** `code2diagram.json` im Ordner trägt die Projektparameter (Beispiel und
  Regeln: `projects/libs/C2dKern/include/c2d/projekt/Projektdatei.hpp`, ein ganzes Projekt:
  `beispiele/projekt/`):
  - `definitionen` wie `-D`, `includePfade` wie `-I` (relativ zum Ordner),
  - `abschnitte`: je Ordner (`"ordner": "firmware/Slave"`) eigene Definitionen und Include-Pfade,
  - `uebergehen`: Ordnermuster, die nicht gelesen werden (`old`, `firmware/bak*`, `**/test`).
  Immer übergangen werden Ordner, die mit `.` beginnen, und `out`, `build`, `node_modules`.
  Project > Edit project file… bearbeitet sie; gibt es noch keine, steht ein Vorschlag da,
  `uebergehen` aus der `.gitignore` des Ordners. Was nicht passt, wird nicht gespeichert.
  Project > Show project messages nennt übergangene Ordner und Köpfe, die sich nicht finden.
- **Gelesen** wird jede Datei mit den Definitionen aus Projekt, Abschnitten und den `#define`
  der Köpfe, die sie einbindet (auch mittelbar); das Feld Defines geht allem vor. Ein Kopf
  `"x.h"` wird neben der Datei gesucht, dann in den Include-Pfaden, zuletzt als einzige Datei
  dieses Namens im Projekt; `<x.h>` nur in den Include-Pfaden.
- Unter dem Quelltext: **Defines** — was beim Auswerten von `#if`/`#ifdef` zusätzlich als
  definiert gilt (`NAME` oder `NAME=wert`, durch Leerzeichen getrennt; Enter liest neu) — und
  die Haken **All #if branches**, **Show #if variants** und **Frame #if regions** (die beiden
  letzten gesperrt, solange alle Zweige gelesen werden; Rahmen auch, solange Varianten gezeigt
  werden — die rahmen selbst). Die Statuszeile nennt, wonach gefragt wurde und was nicht
  definiert ist.
- Rechts angedockt das Feld **Configuration** (Project > Configuration ein- und ausblenden):
  - **Names asked by #if:** jeder Name, nach dem eine Bedingung der Datei fragt, als Haken
    — mit einem Wertfeld, wo er verglichen oder verrechnet wird (`STUFE >= 2`). Gesperrt ist,
    was die Datei selbst definiert (`file, line n`), die Sprache (`__cplusplus`) oder die
    Projektdatei bzw. ein Kopf (`project`) — von hier nicht zu ändern. Include-Wächter
    stehen nicht da.
  - **#if chains:** je Kette `#if` / `#elif` / `#else` der gezeigten Funktion (oder mit
    *Whole file* der Datei) ein Knopf je Zweig; gesetzt ist der, der gilt. Ein Klick setzt
    die Namen, unter denen ein anderer gilt — abgeleitet und nachgerechnet: `defined(X)`,
    `X`, `X == n` direkt, eine Bedingung mit genau einem Namen (`X >= 2`) durch Probieren
    (X entfernen, die Zahlen der Bedingung, je eins daneben, 1); eine Kette in einem Zweig
    zieht die Namen fuer diesen Zweig mit. Was sich nicht ableiten lässt (zwei Namen, ein
    Makro mit Parametern, ein Name der Datei), ist gesperrt; der Tooltip sagt, warum.
  - Haken und Knöpfe schreiben ins Feld **Defines**; die Namen dort sind die Wahrheit.
    **Save to project file** schreibt sie in `definitionen` der Projektdatei (gleichnamige
    ersetzt). Entfernen lässt sich nur, was im Feld Defines steht — die Projektdatei kennt
    kein „nicht definiert".
- Rechts oben: **Diagram** (eine Funktion, mit Bereich wie `Klasse::methode`, ein erkannter
  Zustandsautomat — *State machine: …* — oder die Datentypen), **Format**, Richtung.
- **Zustandsautomaten** werden erkannt, nicht geraten: ein `switch (V)`, in dessen Fällen
  mindestens zweimal `V = K` steht, wobei `K` eine Fall-Marke desselben `switch` ist. Zustände
  sind die Fall-Marken (dazu zugewiesene Werte ohne eigenen Fall), Übergänge die Zuweisungen,
  beschriftet mit den Bedingungen (`if`, `else`, ein `switch` darin), unter denen sie stehen;
  gleiche Wege werden eins, ihre Bedingungen mit `||`. Wo dieselbe Variable — oder dasselbe Glied
  auf anderem Weg (`eeprom.st` statt `p->st`) — anderswo in der Datei einen Zustand bekommt,
  kommt ein Übergang **von außen** (gefüllter Punkt, beschriftet `in init(): …`). Beispiel:
  `beispiele/automat.c`. Darunter zwei Ansichten: **Preview** zeichnet das Diagramm,
  **Text** zeigt es im gewählten Format. File > Save diagram as… legt den Text ab.
- **In der Vorschau** (Strg + Mausrad vergrößert, Ziehen verschiebt):
  - Ein Knoten, der eine Funktion ruft, die es im Projekt gibt, ist farbig umrandet.
    **Doppelklick** zeigt deren Diagramm, auch in einer anderen Datei; Diagram > Back
    (Alt + Pfeil links) führt zurück. Welche Funktion ein Aufruf meint: zuerst eine der eigenen
    Datei, sonst eine nicht-`static` einer anderen; `N::f(` nur eine, deren voller Name so endet.
    Bleiben mehrere, wählt man im Menü (Pfad und Zeile) — nichts wird still gewählt.
  - In der Leiste oben: **Back** führt zurück und umringt den Aufruf, von dem man kam;
    **Called from** nennt jede Funktion im Projekt, die die gezeigte ruft, und führt hin — der
    Aufruf dort ist umringt. Dasselbe steht im Kontextmenü (*Back to …*, *Called from*).
  - **Rechtsklick** auf so einen Knoten: *Embed …() here* setzt den Ablauf der gerufenen Funktion
    umrahmt hinter den Aufruf — auch verschachtelt, in einer Einbettung weiter. Rechtsklick in
    einen Rahmen: *Remove embedding*. Diagram > Remove all embeddings nimmt alle heraus.
  - Doppelklick auf einen anderen Knoten zeigt seine Zeile im Quelltext.
  - Was eingebettet ist, steht auch im Text: als `subgraph` (Mermaid), `cluster` (DOT),
    Behälter (draw.io).
- `--bild` legt das Fenster als PNG ab und endet — ohne Datei mit dem eingebauten Beispiel.
  `--szene` legt das ganze gezeichnete Diagramm ab, in Originalgröße, auch was das Fenster
  nicht zeigt; beide lassen sich zusammen geben.
- `--testing` lädt die Sichttest-Steuerung. Aktionen: `datei_oeffnen pfad=<Datei>` (relativ ab
  dem Projekt-Root), `ordner_oeffnen pfad=<Ordner>`, `datei_waehlen pfad=<Datei im Projekt>`,
  `springen name=<Aufruf> [wahl=<0, 1, …>]`, `zum_aufrufer [wahl=<0, 1, …>]`, `beispiel_zeigen`, `diagramm_waehlen name=<Funktion|types>`,
  `format_waehlen name=<svg|mermaid|dot|drawio>`, `ansicht_waehlen name=<preview|text>`,
  `einbetten name=<Funktion>`, `einbettungen_loesen`, `zurueck`,
  `definitionen_setzen wert=<NAME NAME=wert …>`, `zweige_waehlen name=<auswerten|alle>`.

### Kommandozeile

```bash
c2d <datei|ordner> [--liste] [--datei <pfad>] [--funktion <name>] [--einbetten <name>]... [--struktur] [--automat <variable>] [-D NAME[=wert]]... [--alle-zweige | --varianten | --rahmen] [--format svg|mermaid|dot|drawio] [--richtung TD|LR] [--roh]
```

- ohne Auswahl: das Diagramm der ersten Funktion mit Rumpf; gibt es keine, die Datentypen
- ein **Ordner** wird als Projekt gelesen (wie im Fenster); die Fehlerausgabe nennt danach
  Dateien, Funktionen, Typen und übergangene Ordner. `--datei` wählt die Datei, um die es geht;
  `--funktion` muss im Projekt eindeutig sein, sonst nennt `c2d` die Kandidaten.
  `--einbetten` findet die Funktion auch in einer anderen Datei; `--struktur` ohne `--datei`
  zeichnet die Datentypen aller Dateien
- `--automat Bus_State` zeichnet den Zustandsautomaten über diese Variable (ganz oder ihr
  letztes Glied); `--liste` nennt die erkannten als `automaton …`
- ohne `--format` schreibt `c2d` Mermaid
- `--liste` nennt, was die Datei (im Projekt: jede, mit `== pfad` davor) enthält, `static`
  Funktionen mit `[static]` — und als `undefined`, wonach ein `#if` gefragt hat
- `-D NAME` oder `-D NAME=wert` (auch `-DNAME`): gilt beim Auswerten als definiert, ohne Wert
  als 1; `--alle-zweige` wertet nicht aus; `--varianten` zeigt ein `#if` im Rumpf als Varianten;
  `--rahmen` wertet aus und umrahmt, was ein `#if` im Rumpf zuschaltet
- `--funktion`: der Name allein (`starte`) oder mit Bereich (`antrieb::Motor::starte`)
- `--roh`: je Anweisung ein Knoten, nichts gebündelt
- das Diagramm geht auf die Standardausgabe, Meldungen auf die Fehlerausgabe

```bash
c2d beispiele\aufrufe.c --funktion main --einbetten summe --format drawio > main.drawio
```

```bash
c2d beispiele\projekt --funktion main --einbetten motor_starte --format svg > main.svg
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
├── beispiele/                  kleine Quelldateien zum Ausprobieren und für Tests;
│                               projekt/: ein kleines Projekt mit code2diagram.json
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
  bleibt stehen). `#include` wird nur im Projekt verfolgt, und nur für die `#define`: sie gelten
  ab dem Anfang der einbindenden Datei, nicht erst ab der Zeile des `#include`; ein Kopf wird mit
  seinen eigenen Definitionen gelesen, nicht mit denen, die vor dem `#include` galten. Ein
  `#undef` im Kopf wirkt nicht auf die einbindende Datei. Ein Systemkopf (`<x.h>`), der nicht in
  den Include-Pfaden liegt, bleibt ohne Meldung außen vor. Varianten im Diagramm nur im Rumpf
  einer Funktion und nur, wo jeder Zweig ganze Anweisungen enthält (Schalter „Varianten"); ein
  `#if` um `case`-Marken oder um ganze Funktionen wird ausgewertet. Die Legende der Rahmen
  zeichnen SVG, draw.io und die Vorschau; Mermaid und DOT tragen nur die Farben (ordnen selbst
  an). Nach rechts (`LR`) bekommt der Titel eines Rahmens keinen eigenen Platz.
- **C++-Schablonen** werden übergangen, nicht verstanden: `template<…>` vor einer Deklaration
  fällt weg, eine Spezialisierung heisst wie ihr Text. Überladene Funktionen: im Fenster steht
  die erste zur Wahl. Lambdas sind Text ihrer Anweisung, kein eigener Ablauf.
- **Aufrufe werden am Namen erkannt**, ohne Typen: `a.starte()` meint jede Funktion `starte`,
  gleich welcher Klasse — gibt es mehrere, wählt man. Überladungen trennt nur ihr Ort, nicht die
  Typen der Argumente. Über Dateien hinweg gilt: nicht `static`; welche Datei der Linker nähme,
  weiß Code2Diagram nicht.
- **Die Anordnung ist einfach:** Schichten, Schwerpunkt, Kanten zurück außen herum; eine Kante
  über mehrere Schichten bekommt eine eigene Spalte; steht rechts neben der Quelle einer Kante
  zurück noch ein Knoten, tritt sie unten aus; eine Schlaufe an sich selbst bekommt rechts
  neben ihrem Knoten Platz, über einer Zeile, in der ein Rahmen beginnt, sein Titel. Kreuzungen vermeidet sie nicht immer; ein
  Rahmen kann einen fremden Knoten derselben Schicht umfassen. Ein `switch` mit vielen Fällen
  wird breit. Leere Fälle stehen rechts hinter `default`, nicht in der Folge der Quelle.
- **Zustandsautomaten nur als `switch`:** ein Automat aus `if`-Ketten
  (`if (s == A) … s = B;`), über eine Funktion (`setze_zustand(B)`) oder über eine Tabelle von
  Funktionszeigern wird nicht erkannt. Ein berechneter Wert (`s = ok ? A : B`) wird ein Zustand
  mit diesem Text. Ein Fall, der ohne `break` in den nächsten fällt, gibt keinen Übergang. Von
  außen zählt nur die eigene Datei.
- **draw.io legt die Kanten selbst:** die Datei trägt die Orte der Knoten aus der Anordnung,
  die Wege der Kanten nicht (`orthogonalEdgeStyle`).
- **Im Projekt:** kein Aufrufgraph über das Projekt; die Datentypen zeigt das Fenster je Datei
  (`c2d --struktur` auf einen Ordner: alle zusammen, ohne Beziehungen zwischen Dateien zu
  prüfen). Ein Ordner wird bei jeder Änderung der Defines oder der Projektdatei ganz neu gelesen
  (EtherCAT, 1438 Dateien: rund 5 s). Ordnermuster unterscheiden Groß und Klein.
- Ein Funktionszeiger als Glied gibt keine Beziehung zu den Typen seiner Parameter.
- Code, den kein Weg erreicht (hinter `return`), steht als unverbundener Knoten im Diagramm.
- Gebaut und geprüft ist nur Windows mit Release-clang. Die DOT-Ausgabe ist gegen erwarteten
  Text geprüft, nicht mit Graphviz; die draw.io-Ausgabe als wohlgeformtes XML, nicht in draw.io.
