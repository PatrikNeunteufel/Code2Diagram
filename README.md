# Code2Diagram

**Code2Diagram** liest Quellcode und leitet daraus gut lesbare Diagramme ab — gezeichnet im
Fenster und als Text in einem wählbaren Format.

| | heute | angelegt für |
|---|---|---|
| **Sprachen** | C, C++, Python, JavaScript, TypeScript, C#, Java, Kotlin | weitere: je Sprache eine Tabelle und ein Leser (Rust, Go: erst mit Probecode) |
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
| `lexer/` | der allgemeine Lexer, gesteuert von einer `Sprachtabelle` (Schlüsselwörter, Operatoren, Kommentare, Direktiven, rohe Zeichenketten, Vorsätze und dreifache Anführungszeichen, Zeilenende und Einrückung als Token, Vorlagen mit `${…}`, Regex-Literale, weitere Namenszeichen wie `$`, die Zeichenketten von C#, Vorlagen `"a ${x}"` in Kotlin, verschachtelte Blockkommentare, Namen in Gravis; ein BOM am Anfang wird übergangen) | quelle |
| `modell/` | das **Codemodell**: Funktionen mit Anweisungen, Typen mit Gliedern und Methoden, Aliase, Variablen, Einbindungen, Makros; dazu die bedingte Übersetzung (wonach `#if` fragt, die Ketten `#if`/`#elif`/`#else` mit ihren Bedingungen als Baum) | — |
| `sprache/` | je Sprache eine Tabelle; gelesen wird mit einem Leser in rekursivem Abstieg. C und C++ teilen den **Leser der C-Familie** (`cfamilie/`), JavaScript und TypeScript den Leser `ecma/` (TypeScript als `Dialekt`), C# und Java den Leser `csharp/` (Java als `Dialekt`), Python und Kotlin haben je einen eigenen (`python/`, `kotlin/`). Das Ende einer Anweisung am Zeilenende (JavaScript, Kotlin) liegt in der Basis `Leser`, je Sprache mit eigener Tabelle der Fortsetzer. Für alle gleich: die Basis `Leser`, die **Logik** der Bedingungen samt Wirkung der Anweisungen (`Logik.*`, gesteuert von den `Ausdrucksregeln` der Sprache) und die **Auswertung von `#if`** (`Bedingt.*`, für die C-Familie und C#) | lexer, modell |
| `diagramm/` | die Diagramm-Modelle (`Flussdiagramm`, `Strukturdiagramm`, `Zustandsdiagramm`, `Aufrufgraph`), ihre Ableitung aus dem Codemodell, Einbetten, und die **Anordnung** (wo was steht) | modell |
| `ausgabe/` | die Formate | diagramm |
| `projekt/` | ein Ordner als **Projekt**: die Projektdatei `code2diagram.json`, das Durchsuchen der Ordner, das Lesen in zwei Durchgängen (die `#define` eingebundener Köpfe), das Auflösen eines Aufrufs über Dateien hinweg — und darum der **Aufrufgraph** (`projekt/Aufrufgraph.hpp`): nur das Projekt weiß, was ein Aufruf meint; ebenso **wo ein Name schaltet** (`projekt/Schaltstellen.hpp`): nur das Projekt weiß, wer ihn festlegt | sprache, modell, die Modelle des Aufrufgraphen und der Schaltstellen |

Die Trennlinien sind der Punkt: ein Format kennt keine Sprache, ein Leser kennt kein Diagramm,
kein Diagramm kennt das Projekt.

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
  `break`, `continue`, `return`, `goto` und Marken. Führt kein Weg vom Start zu einem `return`
  oder ans Ende des Rumpfs (eine Hauptschleife `for (;;)`), hat der Ablauf kein `End`; der Start
  trägt darunter `(never returns)`. Eine Schleifenbedingung, die immer wahr ist (`1`, `true`,
  `0x1`, `1u` …), hat keinen Weg `no` hinaus; ein Makro wie `TRUE` wird nicht aufgelöst. Code, zu
  dem kein Weg führt (hinter einer Hauptschleife), bleibt: unter dem Ablauf, grau und
  gestrichelt, mit `(unreachable)`; Fenster und `c2d` melden die Zeile, ab der er steht.
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
Definitionen, die man angibt (`NAME`, `NAME=wert`, oder `!NAME` für „nicht definiert" wie
`-U`; für einen Namen gilt die letzte Angabe); ein Name, der nicht definiert ist, zählt 0; in
C++ ist `__cplusplus` definiert. Zeilen und Spalten bleiben die der Datei.

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

**Python** (`.py`, `.pyi`, `.pyw`; Version 3.12):

- Funktionen (`def`, `async def`), auch in Funktionen (voller Name `aussen::innen`; im Rumpf der
  äußeren steht `def innen(…)`), Parameter wie in der Quelle (`x: int`, `*args`, `**kw`; ohne
  Vorgabewert, wie in der C-Familie)
- Klassen mit Basen (ohne `metaclass=`), Felder aus dem Rumpf (`x: int = 0`, `x = 1`) und aus
  `self.x = …` in Methoden, Methoden (`@staticmethod`, `@abstractmethod`); eine Klasse mit Basis
  `Enum`, `IntEnum`, `StrEnum`, `Flag`, `IntFlag` ist eine Aufzählung
- `if`/`elif`/`else`, `while` und `for` mit `else` (es läuft, wenn die Schleife ohne `break`
  endet), `try`/`except`/`except*`/`else`/`finally` (`finally` auf jedem Weg hinaus, auch nach
  `return`, `raise`, `break`, `continue` — danach geht es mit ihnen beschriftet weiter), `with`,
  `match`/`case` (ohne Durchfallen; `case _` ist der Standardfall), `return`, `raise`, `break`,
  `continue`; mehrere Anweisungen mit `;`, ein Rumpf auf der Zeile des Kopfs (`if x: return 1`)
- `and`, `or`, `not` (bindet schwächer als ein Vergleich: `not a == b` ist `not (a == b)`) und
  `a if c else b` für Wahrheits- und Entscheidungstabellen; `while True` hat keinen Weg `no`
- `import`/`from … import` als Einbindung, Zuweisungen auf oberster Ebene als Variablen; was auf
  oberster Ebene **abläuft** (nicht nur `def`, `class`, `import` und Zuweisungen), ist die Funktion
  `<module>` — so heißt sie auch in Pythons eigenen Meldungen
- Köpfe stehen in Ablauf, Struktogramm und Tabellen so, wie sie in der Quelle stehen (`while x > 0`,
  `for k, v in d.items()`, `except OSError as e`, `raise`, `match x`) — nicht in der Form der
  C-Familie
- Ohne Präprozessor: für Python-Dateien gibt es keine `#if`-Diagramme und keinen Eintrag
  `#if names`. Beispiel: `beispiele/lager.py`.

**JavaScript und TypeScript** (`.js`, `.mjs`, `.cjs`; `.ts`, `.mts`, `.cts`):

- Funktionen (`function`, `async`, `function*`), Methoden von Klassen (auch `get`/`set`, `static`,
  `#privat`, Konstruktor) — und Funktionen ohne `function name`: eine Pfeil- oder `function`-Funktion
  mit Block-Rumpf und eine Methode in einem Objekt (`{ f() {…} }`) werden eigene Funktionen, benannt
  nach dem, dem sie zugewiesen werden (`const f =`, `x.f =`, `f:`), sonst `<anonymous:Zeile>` mit der
  umgebenden Funktion als äußerer (`verbinde::<anonymous:14>`). In der Anweisung bleibt ihr Rumpf als
  `{…}`; ein Doppelklick auf die Anweisung führt zu ihr. Ein Pfeil mit Ausdruck (`x => x * 2`) bleibt
  Text.
- `if`/`else`, `while`, `do`, `for` (auch `of`, `in`, `await`), `switch` mit Durchfallen,
  `try`/`catch` (auch ohne Bindung)/`finally`, `break`, `continue`, `return`, `throw`, Marken
- Ein fehlendes `;` wie in JavaScript: eine neue Zeile beendet die Anweisung, wenn sie sie nicht
  fortsetzen kann (`.`, `(`, `?`, ein Operator vorn oder hinten); nach `return` immer
- Vorlagen (`` `a${x}b` ``, verschachtelt), Regex-Literale (`/a+/g` — nach einem Wert ist `/` die
  Division), `$` in Namen; `import`/`export … from` als Einbindung; was auf oberster Ebene
  **abläuft**, ist `<module>` (nicht Deklarationen, Zuweisungen, `export default`, `'use strict'`)
- TypeScript dazu: Typen an Parametern und Feldern (`x: number`, `name?: string`), Typ-Parameter
  `<T>` (übergangen), `interface` (im Diagramm «interface», `extends` als Basis), `implements`
  (gestrichelte Kante mit offener Spitze), `type X = { … }` als Struktur, jedes andere `type` als
  Alias, `enum` und `const enum`, `namespace a.b` (Bereich `a::b`), `declare`, `abstract`,
  Sichtbarkeit, Felder aus `constructor(private x: T)`.
  Beispiele: `beispiele/lager.ts`, `beispiele/knopf.js`.

**C#** (`.cs`, C# 12):

- Namensräume (auch `namespace A.B;` für die ganze Datei), `using` als Einbindung; `class`, `struct`,
  `record` (Eigenschaften aus `record P(int X)`), `interface`, `enum`, `delegate` (als Alias)
- Glieder: Felder, Eigenschaften als Feld mit `{ get; set; }` (Accessoren mit Rumpf werden Funktionen
  `Menge.get`, `Menge.set`), Indexer `this[…]`, Ereignisse, Methoden, Konstruktoren, Destruktoren,
  Operatoren, ausdrücklich umgesetzte Glieder (`IEnumerable<T>.GetEnumerator`); `A` und `A<T>` sind
  verschiedene Typen (die Glieder tragen `A<T>::` im Bereich)
- Basen: eine Klasse nennt ihre Basisklasse vorn, alles danach sind Schnittstellen (gestrichelt);
  steht die vordere als `interface` in derselben Datei, ist auch sie eine Schnittstelle. Bei einem
  `struct` sind alle Schnittstellen.
- Anweisungen: `if`, `while`, `do`, `for`, `foreach`, `switch` mit Mustern, `try`/`catch (…) when (…)`/
  `finally`, `using (…)`, `lock (…)`, `fixed (…)`, `yield return`/`yield break`, `goto`, lokale
  Funktionen, Lambdas mit Block-Rumpf (`<lambda:Zeile>`); ein `switch`-Ausdruck auf oberster Ebene
  eines `return`, einer Zuweisung oder Deklaration ist eine Auswahl ohne Durchfallen, je Arm die
  Anweisung (`return "eins"`, `var s = "eins"`, `throw …`); Anweisungen auf oberster Ebene sind
  `<Main>$`
- Zeichenketten `@"…"`, `$"…{x}…"`, `$@"…"`, dreifache Anführungszeichen (roh), Namen `@class`
- `#if`/`#elif`/`#else`/`#define`/`#undef` wie in der C-Familie ausgewertet — ein Name ist wahr, wenn
  er definiert ist; Defines, Projektdatei, Feld Configuration und die `#if`-Diagramme gelten.
  `#region`, `#pragma`, `#nullable` werden übergangen. Beispiel: `beispiele/lager.cs`.

**Java** (`.java`, Java 21; gelesen vom Leser von C# als Dialekt):

- `package` wird überlesen (die Namen tragen keinen Paketpfad), `import` ist eine Einbindung;
  `class`, `interface`, `@interface` (eine Schnittstelle), `enum` mit Rumpf (Konstanten mit ihren
  Argumenten als Wert, Felder, Konstruktoren, Methoden; der Rumpf einer Konstante wie eine anonyme
  Klasse: `ROT.methode`), `record` (eine Struktur mit den Komponenten als Feldern, auch der kompakte
  Konstruktor); innere Klassen als `Aussen::Innen`
- Basen: `extends` ist eine Basis, `implements` eine Realisierung (gestrichelt); `permits`, `sealed`,
  `non-sealed` werden überlesen
- Glieder: Felder (auch `int a[]`), Methoden, Konstruktoren, Initialisierungsblöcke als Funktionen
  `<static:Zeile>` und `<init:Zeile>`; Annotationen, `final` an Parametern, `throws …` und `<T>` vor
  dem Rückgabetyp stehen nicht in der Signatur; varargs `String...`
- **Anonyme Klassen:** jede Methode ist eine eigene Funktion `<anonymous:Zeile>.run` mit der
  umgebenden als äußerer; in der Anweisung bleibt `new Runnable() {…}`, ein Doppelklick führt hin.
  In den Datentypen steht kein Kasten für sie.
- Anweisungen: `if`, `while`, `do`, `for` (auch `for (T x : xs)`), `switch` mit `:` (Durchfallen)
  und mit `->` (ohne), ein `switch`-Ausdruck auf oberster Ebene eines `return`, einer Zuweisung oder
  Deklaration (`yield wert` im Block eines Arms wird `return wert` bzw. `x = wert`),
  `try`/`catch`/`finally`, `try (Ressourcen)` mit diesem Kopf, `synchronized (…)`, Lambdas mit
  Block-Rumpf (`<lambda:Zeile>`), lokale Klassen
- **Marken:** `aussen: for …` gehört der Schleife; `break aussen` führt hinter sie, `continue aussen`
  an ihren Kopf — auch durch ein `finally`. Im Struktogramm trägt der Kopf `aussen:`. (Ebenso in
  JavaScript.)
- Ohne Präprozessor: keine `#if`-Diagramme. Ein Maven-Ordner `target` neben einer `pom.xml` wird
  übergangen. Beispiel: `beispiele/Lager.java`.

**Kotlin** (`.kt`, Kotlin 2.0; `.kts` noch nicht):

- `package` wird überlesen, `import` ist eine Einbindung, `typealias` ein Alias; `class`,
  `interface` (auch `fun interface`), `object`, `enum class` (Konstanten mit Argumenten und Rumpf wie
  in Java), `data`/`sealed`/`open`/`inner` … werden überlesen; innere Typen als `Aussen::Innen`
- der **primäre Konstruktor**: `val`/`var` darin sind Felder, die übrigen nur Parameter; `init { }`
  ist `<init:Zeile>`, `constructor(…)` ein weiterer Konstruktor. Basen: `: Basis(…)` mit Klammern ist
  eine Basis, ohne Klammern (und `I by x`) eine Realisierung; eine Schnittstelle erweitert Schnittstellen
- **Eigenschaften** sind Felder (`name: Typ`), mit `get()`/`set(…)` samt Rumpf zusätzlich Funktionen
  `name.get`/`name.set`; das Feld trägt `{ get; set; }`
- **Erweiterungen** heißen mit ihrem Empfänger: `String.kuerze`, `List<Any>.nurVom`, freie Funktionen,
  keine Glieder des Kastens. Die Glieder eines **`companion object`** sind statische Glieder des Typs
  darum (kein Kasten `Companion`); `object X` ist eine Klasse
- **Lambdas:** eines, das nur aus einem Ausdruck in einer Zeile besteht (`xs.map { it * 2 }`), bleibt
  Text; jedes andere ist eine Funktion `<lambda:Zeile>` (nach `val NAME = {` heißt es `NAME`) mit
  ihren Parametern `{ a, b -> …`, in der Anweisung `{…}`, Doppelklick hin — auch die von `let`,
  `apply`, `launch` und Compose. Ein **anonymes Objekt** `object : TextWatcher { … }` gibt je Methode
  eine Funktion `<anonymous:Zeile>.name`
- Anweisungen: `if`, `when` (mit Gegenstand eine Auswahl ohne Durchfallen, Kopf `when (x)`, `else` als
  `default`; ohne Gegenstand eine Kette `if`/`else if`/`else`, `a, b ->` als `a || b`), `for (x in xs)`,
  `while`, `do … while`, `try`/`catch`/`finally`, `return@marke`, `break@marke`/`continue@marke` an
  `marke@ for …` (wie Java), lokale Funktionen und Klassen
- **`if`, `when`, `try` als Wert** auf oberster Ebene eines `return`, eines `= ausdruck`-Rumpfs, einer
  Zuweisung oder Deklaration verzweigen: jeder Zweig endet mit `val s = wert` bzw. `return wert`
  (die letzte Anweisung eines Blocks ist sein Wert); geht der Ausdruck weiter
  (`when (x) {…}.uppercase()`), bleibt er Text
- Das Ende einer Anweisung wie Kotlin: am Zeilenende, wenn die nächste Zeile nicht mit `.`, `?.`,
  `?:`, `&&`, `||` … beginnt und die Zeile nicht mit einem Operator endet; `+1` am Zeilenanfang ist
  eine eigene Anweisung. Kein Präprozessor. Musterprojekte: `beispiele/kotlin/` (siehe dort).

Ein Name, den die Datei nicht kennt, gilt vor einem Bezeichner als Typ — ein `typedef` aus einer
anderen Datei muss also nicht bekannt sein. Was der Leser nicht versteht, übergeht er **mit einer
Meldung** (Zeile, Spalte); das Ergebnis bleibt brauchbar.

## Aufruf

### Fenster

```bash
Code2Diagram [--testing] [datei|ordner] [--datei <pfad>] [-D NAME[=wert] | -D !NAME]... [--alle-zweige | --varianten | --rahmen] [--diagramm <name>] [--einbetten <funktion>]... [--springe <aufruf>]... [--zurueck] [--format <name>] [--ansicht preview|text] [--bild <datei.png>] [--szene <datei.png>] [--thema system|light|dark] [--einstellungen] [--struktogramm] [--entscheidungstabelle]
```

- **Eine Datei oder ein Ordner:** File > Open… liest eine Datei, File > Open folder…
  (Strg + Umschalt + O) einen Ordner samt Unterordnern als Projekt. Dann steht über dem
  Quelltext der **Baum der Dateien**; zur Wahl stehen die Diagramme der gewählten Datei.
  Übergangene Ordner stehen grau dabei. Rechtsklick auf einen Ordner: *Skip this folder*
  trägt ihn in die Projektdatei ein; auf einen übergangenen: *Read this folder again*.
  `--datei` wählt beim Start die Datei im Projekt. **File > Open recent** nennt die zuletzt
  geöffneten Dateien und Ordner (höchstens 10, das Neueste oben, ein Ordner mit `/` am Ende);
  was es nicht mehr gibt, fällt beim Wählen heraus, *Clear list* leert sie. Die Liste bleibt über
  Neustarts (`QSettings`, unter Windows `HKCU\Software\Code2Diagram`).
- **File > Settings…** (Strg + ,): links die Seiten, rechts ihre Felder; OK, Cancel, Apply. Heute
  eine Seite, *Appearance*, mit **Theme**: *System* (folgt dem hellen oder dunklen Modus des
  Betriebssystems, auch während des Laufs), *Light*, *Dark*. Gesetzt wird es über Qt
  (`QStyleHints::setColorScheme`); die Vorschau zeichnet bei jedem Wechsel neu. Die Farben des
  Diagramms stehen in einer Tabelle im Kern (`c2d/diagramm/Farben.hpp`), je hell und dunkel: die
  Vorschau nimmt die Spalte zur Palette, SVG schreibt beide, Mermaid, DOT und draw.io die helle.
  Der Stil ist Fusion, hell wie dunkel, mit einer eigenen Palette je Thema. Gespeichert unter
  `aussehen/thema` (`system`, `light`, `dark`). `--thema` setzt es nur für diesen Lauf (für
  Bilder), ohne es zu speichern; `--einstellungen` öffnet den Dialog beim Start (mit `--bild`
  liegt er im Bild mittig über dem Fenster). Exportierte SVG folgen dem Browser, nicht dem Thema.
- **Die Projektdatei** `code2diagram.json` im Ordner trägt die Projektparameter (Beispiel und
  Regeln: `projects/libs/C2dKern/include/c2d/projekt/Projektdatei.hpp`, ein ganzes Projekt:
  `beispiele/projekt/`):
  - `definitionen` wie `-D` (`"CPU1"`, `"TAKT=10"`), `"!NAME"` hebt auf wie `-U`; in einer
    Liste gilt der spätere Eintrag; `includePfade` wie `-I` (relativ zum Ordner),
  - `abschnitte`: je Ordner (`"ordner": "firmware/Slave"`) eigene Definitionen und Include-Pfade,
  - `uebergehen`: Ordnermuster, die nicht gelesen werden (`old`, `firmware/bak*`, `**/test`).
  Immer übergangen werden Ordner, die mit `.` beginnen, und `out`, `build`, `node_modules`, `bin`,
  `obj`; neben ihrem Kennzeichen auch die Ordner eines Werkzeugs: Unity (`Library`, `Temp`, `Logs`,
  `UserSettings` neben `ProjectSettings/ProjectVersion.txt`), Maven (`target` neben `pom.xml`).
  Project > Edit project file… bearbeitet sie; gibt es noch keine, steht ein Vorschlag da,
  `uebergehen` aus der `.gitignore` des Ordners. Was nicht passt, wird nicht gespeichert.
  Project > Show project messages nennt übergangene Ordner und Köpfe, die sich nicht finden.
- **Gelesen** wird jede Datei mit den Definitionen aus Projekt, Abschnitten und den `#define`
  der Köpfe, die sie einbindet (auch mittelbar); das Spätere ersetzt Gleichnamiges, auch ein
  `!NAME`: was die Projektdatei aufhebt, kann ein Abschnitt oder ein Kopf wieder definieren.
  Das Feld Defines geht allem vor — auch `!NAME` dort hebt das `#define` eines Kopfs auf.
  Was die Datei selbst definiert, gilt immer. Ein Kopf
  `"x.h"` wird neben der Datei gesucht, dann in den Include-Pfaden, zuletzt als einzige Datei
  dieses Namens im Projekt; `<x.h>` nur in den Include-Pfaden.
- Unter dem Quelltext: **Defines** — was beim Auswerten von `#if`/`#ifdef` zusätzlich als
  definiert gilt (`NAME`, `NAME=wert` oder `!NAME` für nicht definiert, durch Leerzeichen
  getrennt; Enter liest neu) — und
  die Haken **All #if branches**, **Show #if variants** und **Frame #if regions** (die beiden
  letzten gesperrt, solange alle Zweige gelesen werden; Rahmen auch, solange Varianten gezeigt
  werden — die rahmen selbst). Die Statuszeile nennt, wonach gefragt wurde und was nicht
  definiert ist.
- Rechts angedockt das Feld **Configuration** (Project > Configuration ein- und ausblenden):
  - **Names asked by #if:** jeder Name, nach dem eine Bedingung der Datei fragt, als Haken
    — mit einem Wertfeld, wo er verglichen oder verrechnet wird (`STUFE >= 2`). Gesperrt ist
    nur, was die Datei selbst definiert (`file, line n`) oder die Sprache (`__cplusplus`).
    Woher ein Name sonst kommt, steht dahinter (`project`, `section`, `header`); einen
    solchen abzuwählen schreibt `!NAME` ins Feld Defines (`project, !NAME`), Anhaken nimmt
    es zurück. Include-Wächter stehen nicht da.
  - **#if chains:** je Kette `#if` / `#elif` / `#else` der gezeigten Funktion (oder mit
    *Whole file* der Datei) ein Knopf je Zweig; gesetzt ist der, der gilt. Ein Klick setzt
    die Namen, unter denen ein anderer gilt — abgeleitet und nachgerechnet: `defined(X)`,
    `X`, `X == n` direkt, eine Bedingung mit genau einem Namen (`X >= 2`) durch Probieren
    (X entfernen, die Zahlen der Bedingung, je eins daneben, 1); eine Kette in einem Zweig
    zieht die Namen fuer diesen Zweig mit. Was sich nicht ableiten lässt (zwei Namen, ein
    Makro mit Parametern, ein Name, den die Datei selbst definiert), ist gesperrt; der
    Tooltip sagt, warum.
  - Haken und Knöpfe schreiben ins Feld **Defines**; die Namen dort sind die Wahrheit.
    **Save to project file** schreibt sie in `definitionen` der Projektdatei (gleichnamige
    ersetzt, auch durch `!NAME`; nichts entfernt). Legt ein Abschnitt denselben Namen anders
    fest, gilt dort weiter der Abschnitt — das sagt ein Hinweis nach dem Speichern.
- Rechts oben: **Diagram** (eine Funktion, mit Bereich wie `Klasse::methode`, ein erkannter
  Zustandsautomat — *State machine: …* — oder die Datentypen), **Format**, Richtung.
- **Datentypen:** Ist *Data types* gewählt, steht daneben ein zweites Feld: *All types (n)* oder
  ein Typ der Datei (gewählt ist zuerst der erste). Ein Typ wird mit seinen Nachbarn derselben
  Datei gezeigt — was er enthält, worauf er verweist, wovon er erbt, und wer das mit ihm tut.
  **Links auf Typen:** Ein Typname in einem Glied ist hervorgehoben (unterstrichen), ebenso im
  Ablauf ein Typ, den ein Knoten nennt (Deklaration, Cast, `sizeof`, Parameter im Startknoten)
  und eine gerufene Funktion. Doppelklick oder Kontextmenü (*Go to type …*) führt hin, auch in eine
  andere Datei: Data types, der Typ im zweiten Feld; Back zurück. Ein Typname meint erst die
  eigene Datei, dann die eingebundenen Köpfe (auch mittelbar), dann jede Datei des Projekts; ein
  `typedef` führt zu seinem Typ. Hat ein Knoten mehrere Ziele (Funktionen und Typen) oder meint ein
  Name mehrere, wählt man im Menü. Doppelklick auf den Kopf eines Kastens zeigt diesen Typ.
  Beispiel: `beispiele/typen/`.
- **Zustandsautomaten** werden erkannt, nicht geraten: ein `switch (V)`, in dessen Fällen
  mindestens zweimal `V = K` steht, wobei `K` eine Fall-Marke desselben `switch` ist. Zustände
  sind die Fall-Marken (dazu zugewiesene Werte ohne eigenen Fall), Übergänge die Zuweisungen,
  beschriftet mit den Bedingungen (`if`, `else`, ein `switch` darin), unter denen sie stehen;
  gleiche Wege werden eins, ihre Bedingungen mit `||`. Wo dieselbe Variable — oder dasselbe Glied
  auf anderem Weg (`eeprom.st` statt `p->st`) — anderswo in der Datei einen Zustand bekommt,
  kommt ein Übergang **von außen** (gefüllter Punkt, beschriftet `in init(): …`). Beispiel:
  `beispiele/automat.c`.
- **Nassi-Shneiderman** (Struktogramm, DIN 66261): das Feld neben Diagram stellt jede Funktion
  als **Flowchart** oder **Nassi-Shneiderman** dar; es bleibt beim Springen. Je Anweisung ein
  Block; `if` mit Dreieck, `yes` und `no` in den Ecken; `switch` mit je Fall einer Spalte (leere
  Fälle teilen sich eine, ein `break` am Fallende entfällt, Durchfallen endet auf `falls through`,
  `default` zuletzt, sonst eine leere Spalte `otherwise`); Schleifen kopf- oder fußgesteuert mit
  dem Text der Quelle, immer wahr als `forever`; `break`, `continue`, `return`, `throw`, `goto` als
  Aussprung (Pfeil nach links); `try`/`catch`, Varianten und Rahmen wie im Ablauf. `goto` passt
  nicht in die Struktur: eine Meldung mit Zeile. Spalten so breit wie ihr Inhalt. SVG und draw.io
  schreiben es, Mermaid und DOT nicht (die Ansicht Text sagt es). Klick, Doppelklick und *Show in
  diagram* wie im Ablauf; Einbetten gibt es hier nicht. `c2d --funktion f --struktogramm`,
  im Fenster `--struktogramm`.
- **Wahrheitstabellen** samt **MC/DC**: zu jeder Bedingung mit mindestens zwei Teilen (in `if`,
  `while`, `do`, `for`, `?:`, einem `return` oder einer Zuweisung mit `&&`/`||`). Je Teil (Atom:
  ein Vergleich, ein Aufruf, ein Name) eine Spalte, je Kombination eine Zeile; wo C wegen
  Kurzschluss ein Atom nicht auswertet, steht `–`; ein Atom mit Aufruf heißt `(call)`. MC/DC nach
  Unique-Cause: je Atom ein Paar Zeilen, die sich nur in ihm unterscheiden und verschieden ausgehen;
  die kleinste Menge davon ist getönt, darunter stehen die Paare. Bis 6 Atome jede Kombination,
  darüber nur die MC/DC-Zeilen. Knoten und Blöcke mit Tabelle tragen ein kleines Tabellensymbol
  (Klick öffnet sie), das Kontextmenü *Truth table (line n)*, im Quelltext *Truth table of line n*.
  Die Tabelle ist ein eigenes Diagramm (Back zurück); ein Klick darin zeigt die Bedingung. SVG und
  draw.io zeichnen sie, **csv** schreibt sie für Testpläne (Trenner `;`); Mermaid und DOT nicht.
  `c2d --funktion f --wahrheitstabelle [<zeile>]`. Beispiel: `beispiele/bedingungen.c`.
- **Entscheidungstabelle** einer Funktion (DIN 66241): die dritte Darstellung im Feld neben
  Diagram (*Decision table*). Je Weg durch `if`/`else`, `switch`-Fälle und `?:` eine Regel R1, R2 …
  (Spalten); oben die Bedingungen (Y/N, bei `switch` der Wert des Falls, `–` = nicht gefragt), nach
  einer doppelten Linie die Aktionen (je Anweisung eine Zeile, X je Regel). Eine Schleife ist eine
  Aktion mit ihrem Kopf, ihr Inneres steht nicht darin (Hinweis). Eine Bedingung gilt auf einem Weg
  als entschieden, bis eine Anweisung danach etwas ruft oder einen ihrer Namen schreibt (auch durch
  einen Zeiger); bis dahin folgt der Weg ihrem Wert, sonst wird sie neu gefragt — in einer eigenen
  Zeile `… (again, line n)`. Über 32 Regeln keine Tabelle, nur ein Hinweis. Klick auf eine Zeile
  zeigt sie im Quelltext; SVG, draw.io und csv schreiben sie, Mermaid und DOT nicht.
  `c2d --funktion f --entscheidungstabelle`, im Fenster `--entscheidungstabelle`.
- **Zuordnungstabellen:** ein `switch`, in dem jeder Fall nur einen Wert liefert — ein
  `return <Ausdruck>`, oder Zuweisungen an dieselben Namen in jedem Fall, dann `break` —, ist eine
  Tabelle: links der Wert des Falls, rechts je Name eine Spalte (oder `return`). Geteilte Fälle je
  eine Zeile, `default` zuletzt; fehlt es, eine Zeile `otherwise` mit `—`. Werte stehen als Text der
  Quelle, gerechnet wird nichts. Der Knoten des `switch` trägt das Tabellensymbol, das Kontextmenü
  *Mapping table (line n)*, im Quelltext *Mapping table of line n*. Dazu die **konstanten Tabellen**
  einer Datei (`const`-Arrays mit `{ … }`), in Diagram als *Lookup tables (n)*: Index → Wert,
  benannte Indizes (`[ROT] =`) und weitergezählte (`ROT + 1`), bei Strukturen je Glied eine Spalte
  (auch `.glied =`); den Strukturtyp sucht sie in der Datei, dann wie die Links auf Typen in den
  eingebundenen Köpfen und im Projekt (mehrdeutig oder unbekannt: Spalten `1`, `2`, …). Ein Klick
  auf eine Zeile zeigt sie im Quelltext. SVG, draw.io und csv schreiben sie, Mermaid und DOT nicht.
  `c2d --funktion f --zuordnung [<zeile>]`, `c2d <datei> --nachschlagetabellen` (im Projekt mit
  `--datei`). Beispiel: `beispiele/zuordnung.c`.
- **`#if`-Konfiguration:** je Kette `#if`/`#elif`/`#else` einer Datei eine Tabelle, welcher Zweig
  bei welchen Definitionen gilt — in Diagram *#if configuration (n)*. Je Zweig eine Zeile, je Name,
  nach dem die Kette fragt, eine Spalte (im Kopf sein Stand: `= 100`, `def`, `undef`); in der Zelle,
  was der Zweig fordert: `def`, `!def`, ein Vergleich (`> 50`, `<= 1`), `–` = gleich. Die Forderung
  ist **absolut**: der Zweig gilt, wenn seine Bedingung wahr ist, alle davor falsch und der Zweig, in
  dem die Kette steht, gilt (eine innere Kette ist eingerückt, ihr Titel nennt den äußeren Zweig).
  Ein `||` gibt je Alternative eine Zeile `or` (bis 8); nicht definiert zählt wie im Präprozessor als
  0, Widersprüche fallen weg, ein Zweig, den keine Definition gelten lässt, heißt `never`. Was sich
  nicht nach Namen zerlegen lässt (`A == B`, `F(2) > 1`, Rechnungen), steht als Bedingung in einer
  Zeile, mit Hinweis darunter. Ein Name, den die Datei selbst definiert, hat grauen Text und im
  Kopf `(line n)`. Die Zeile, die gilt, ist getönt und trägt ✓. Ein Klick zeigt `#if`/`#elif`/`#else`
  (beim fehlenden `#else` das `#endif`) im Quelltext; im Kontextmenü einer Zeile **Make this branch
  active** — setzt die Namen im Feld Defines wie die Radioknöpfe im Feld Configuration (gesperrt mit
  Tooltip, wo es nicht geht); im Quelltext auf einer Direktive *#if configuration of line n*. SVG,
  draw.io und csv schreiben sie, Mermaid und DOT nicht. `c2d <datei> --konfiguration` (im Projekt mit
  `--datei`). Beispiel: `beispiele/konfiguration.c`.
- **Wo schaltet ein Name** (im Projekt): *Where is NAME switched?* im Kontextmenü einer Zeile der
  `#if`-Konfiguration (je Spalte ein Eintrag), eines Namens im Feld Configuration und im Quelltext
  auf einem Namen in einer Direktive — eine eigene Tabelle `switch:NAME`, Back führt zurück. Oben
  *set by*: wer ihn festlegt (`project`, `section <ordner>`, `#define`/`#undef` mit Datei und Zeile,
  `outside` = Defines oder `-D`, `language`); ein `#define` in einem Zweig, der nicht gilt, sieht der
  Leser nicht. Darunter je Zweig im ganzen Projekt, dessen absolute Forderung den Namen betrifft
  (auch eine innere Kette unter `#ifdef NAME` — sie trägt in der Spalte *via* den äußeren Zweig,
  `#if defined(NAME), line 60`, ihre Forderung steht grau), eine Zeile: Datei, Zeile, Zweig, Forderung, der
  Stand in dieser Datei mit Herkunft (`= 100 (project)`, `def (header cfg.h)`, `undef (line 7)` —
  was die Datei vor der Kette selbst festlegt, gilt dort), wie viele Zeilen der Zweig umfasst,
  *active*. Ein Kopf steht einmal, in seiner eigenen Datei. Ein Klick zeigt die Stelle, auch in einer
  anderen Datei; ein Doppelklick führt zur `#if`-Konfiguration dieser Datei, an diese Kette. Die
  Übersicht **#if names (n)** in Diagram nennt je Name Dateien, Ketten, Stand (`differs`, getönt,
  wo er nicht überall gleich ist) und wer ihn festlegt; Vorgegebenes der Sprache grau; Doppelklick
  führt zu seiner Tabelle. SVG, draw.io und csv schreiben beide, Mermaid und DOT nicht.
  `c2d <ordner> --schaltet NAME`, `c2d <ordner> --schalter`.
- **Aufrufgraph:** wer wen ruft. In Diagram steht *Call graph of this file* (alle Funktionen der
  Datei; ein Aufruf in eine andere Datei endet am Knoten der gerufenen Funktion), und
  **Call graph from here** in der Leiste, im Menü Diagram und im Kontextmenü eines Ablaufs zeigt
  den Graphen ab der gezeigten (oder der gerufenen) Funktion über das ganze Projekt; er bleibt als
  *Call graph from f()* in der Auswahl. Daneben in der Leiste: die Richtung (**Calls** — was sie
  ruft, **Callers** — wer sie ruft, **Both**), die Tiefe (Vorgabe 2, *all* = alle Stufen),
  **Max** (höchstens so viele Funktionen, Vorgabe 300, *all* = unbegrenzt) und
  **Hide external calls**. Ist Max erreicht, kommt keine Funktion mehr dazu; wer weiter riefe,
  trägt ` …`, und die Statuszeile sagt `call graph stopped at 300 functions - lower the depth or
  expand by hand` — was eine aufgeklappte Funktion ruft, zählt nicht. Jede Funktion ist ein Knoten, je rufendem Paar eine Kante; eine
  Rekursion ist eine Schlaufe oder eine Kante zurück. Was ein Aufruf meint, entscheidet das Projekt
  wie beim Sprung; meint er mehrere, führt zu jeder eine **gestrichelte** Kante `?`, und die
  Statuszeile nennt die Kandidaten. Ein Aufruf ohne Funktion im Projekt (Bibliothek, SDK) ist ein
  grauer Knoten außerhalb der Rahmen, ein Makro mit Parametern dazu gestrichelt. Berührt der Graph
  mehrere Dateien, steht jede in einem Rahmen mit ihrem Pfad. Eine Funktion an der Grenze der
  Tiefe, die weiter ruft, trägt ` …`; Rechtsklick *Expand one level* klappt sie auf (*Collapse*
  zu). Doppelklick auf eine Funktion zeigt ihren Ablauf, Back führt in den Graphen zurück. Mit
  **Show #if variants** ist eine Kante aus einem Zweig, der nicht gilt, **gepunktet** und trägt
  dessen Bedingung; mit **Frame #if regions** hat eine Kante aus einem bedingten Bereich dessen
  Farbe, mit Legende. Ein Graph mit vielen Gerufenen wird nach unten sehr breit — nach rechts
  (*Left-right*) liest er sich besser.
- Darunter zwei Ansichten: **Preview** zeichnet das Diagramm,
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
  - **Ein Klick** (ohne zu ziehen) zeigt in jeder Ansicht die Stelle im Quelltext: im Ablauf die
    Zeile der Anweisung — auch bei einem Aufruf —, im Aufrufgraph die Zeile der Funktion (ein Makro
    dort, wo das Projekt es definiert; ein fremder Aufruf hat keine), bei den Datentypen die Zeile
    des Typs, des Felds oder der Methode. Der **Doppelklick** führt nur zu dem, was genannt ist;
    wo es nichts gibt, tut er nichts weiter.
  - **Umgekehrt:** Rechtsklick im Quelltext, *Show in diagram*: die Zeile in ihrem Diagramm, mit
    einem Ring. In einer Funktion ihr Knoten (auch eine Anweisung in einem gebündelten; eine Zeile
    ohne eigene Anweisung, etwa `}`, zeigt die davor) — ist eine andere Funktion gezeigt, wechselt
    das Fenster zu ihr, Back führt zurück; im Aufrufgraph die Funktion; in einem Typ Data types
    mit ihm.
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
  `definitionen_setzen wert=<NAME NAME=wert …>`, `zweige_waehlen name=<auswerten|alle>`,
  `klicken name=<…>`, `im_diagramm_zeigen zeile=<n>`,
  `darstellung_waehlen name=<flowchart|nassi-shneiderman|decision-table>`,
  `wahrheitstabelle_zeigen zeile=<n>`, `zuordnung_zeigen zeile=<n>`, `konfiguration_zeigen zeile=<n>`,
  `zweig_aktivieren zeile=<n>`, `schaltstellen_zeigen name=<Name>`,
  `einstellungen_zeigen`,
  `thema_setzen name=<system|light|dark>`.

### Kommandozeile

```bash
c2d <datei|ordner> [--liste] [--datei <pfad>] [--funktion <name>] [--einbetten <name>]... [--struktur [--typ <name>]] [--automat <variable>] [--aufrufgraph [<funktion>] [--tiefe n] [--aufrufer | --beide] [--ohne-fremde]] [-D NAME[=wert] | -D !NAME | -U NAME]... [--alle-zweige | --varianten | --rahmen] [--format svg|mermaid|dot|drawio|csv] [--richtung TD|LR] [--roh] [--struktogramm] [--wahrheitstabelle [<zeile>]] [--entscheidungstabelle] [--zuordnung [<zeile>]] [--nachschlagetabellen] [--konfiguration] [--schaltet <name>] [--schalter] [--breite]
```

- ohne Auswahl: das Diagramm der ersten Funktion mit Rumpf; gibt es keine, die Datentypen
- ein **Ordner** wird als Projekt gelesen (wie im Fenster); die Fehlerausgabe nennt danach
  Dateien, Funktionen, Typen und übergangene Ordner. `--datei` wählt die Datei, um die es geht;
  `--funktion` muss im Projekt eindeutig sein, sonst nennt `c2d` die Kandidaten.
  `--einbetten` findet die Funktion auch in einer anderen Datei; `--struktur` ohne `--datei`
  zeichnet die Datentypen aller Dateien
- `--automat Bus_State` zeichnet den Zustandsautomaten über diese Variable (ganz oder ihr
  letztes Glied); `--liste` nennt die erkannten als `automaton …`
- `--typ Regler` (mit `--datei` im Projekt) zeichnet nur diesen Typ und seine Nachbarn
- `--aufrufgraph main` zeichnet den Aufrufgraph ab `main` (wie `--funktion`: im Projekt
  eindeutig, sonst `--datei`; der Name darf gleich folgen, wenn das Ziel davor steht, sonst
  `--aufrufgraph --funktion main`); ohne Funktion den der Datei (im Projekt mit `--datei`).
  `--tiefe n` (Vorgabe 2, 0 = alle), `--hoechstens n` (Funktionen, Vorgabe 300, 0 = unbegrenzt;
  ist die Grenze erreicht, sagt es die Fehlerausgabe), `--aufrufer` (wer ruft), `--beide`,
  `--ohne-fremde`.
  Mehrdeutige Aufrufe nennt die Fehlerausgabe mit Datei und Zeile
- ohne `--format` schreibt `c2d` Mermaid
- `--liste` nennt, was die Datei (im Projekt: jede, mit `== pfad` davor) enthält, `static`
  Funktionen mit `[static]` — und als `undefined`, wonach ein `#if` gefragt hat; Zuordnungen als
  `mapping f@zeile`, konstante Tabellen als `lookup`, die Ketten `#if` als `ifconfig n chains, …`
- `--zuordnung [<zeile>]` (mit `--funktion`) schreibt die Zuordnungstabelle des `switch` in dieser
  Zeile, ohne Zeile alle der Funktion; `--nachschlagetabellen` die konstanten Tabellen der Datei;
  `--konfiguration` je Kette `#if` der Datei die Konfigurationstabelle; `--schaltet NAME`, wo der
  Name im Projekt schaltet, `--schalter` die Namen des Projekts
- `-D NAME` oder `-D NAME=wert` (auch `-DNAME`): gilt beim Auswerten als definiert, ohne Wert
  als 1; `-U NAME` (auch `-UNAME`, `-D !NAME`): gilt als nicht definiert, auch wo Projektdatei
  oder ein Kopf ihn definieren; für einen Namen gilt die letzte Angabe; `--alle-zweige` wertet nicht aus; `--varianten` zeigt ein `#if` im Rumpf als Varianten;
  `--rahmen` wertet aus und umrahmt, was ein `#if` im Rumpf zuschaltet
- `--funktion`: der Name allein (`starte`) oder mit Bereich (`antrieb::Motor::starte`)
- `--roh`: je Anweisung ein Knoten, nichts gebündelt
- das Diagramm geht auf die Standardausgabe, Meldungen auf die Fehlerausgabe
- `--breite`: der Breiten-Test an echtem Code — legt je Datei (im Projekt jede, oder `--datei`)
  jeden Ablauf, die Automaten, die Datentypen und den Aufrufgraph der Datei an, nach unten und nach
  rechts, und nennt jede Anordnung, die mehr als doppelt so breit (nach rechts: hoch) ist, wie ihre
  breiteste Schicht samt Stützpunkten, Rahmen, Rückwegen und Legende braucht; je Band zählt seine
  breiteste Stelle. Am Ende die Zahl und die breiteste; Exit 3, wenn eine zu breit ist

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

Der **Breiten-Test** (`test_Breite.cpp`) legt gebaute Formen an, die die bekannten Wege des
Aufschaukelns nachstellen — ein breiter Fächer, viele frühe `return`, Schleifen mit Rückwegen, ein
Automat, Datentypen, ein Aufrufgraph über Dateien sehr ungleicher Größe —, je nach unten und nach
rechts, und prüft das Maß aus dem Kern (`querVerhaeltnis`, Grenze `kHoechstesQuerVerhaeltnis` = 2).
An echtem Code: `c2d <ordner> --breite`.

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
- **Die Anordnung ist einfach:** Schichten, Schwerpunkt, die Lage in der Zeile nach Brandes–Köpf
  (vier Ausrichtungen gemittelt; bei zwei mittleren Nachbarn zuerst der mit dem längeren Weg, also
  der Hauptstrang), Kanten zurück außen herum — im Ablauf, im Aufrufgraph und bei
  den Datentypen dieselbe rechtwinklige Wegeführung; eine Kante
  über mehrere Schichten bekommt eine eigene Spalte, die unter ihrer Quelle einmal abbiegen darf
  (so liegen die Spalten früher `return` dicht beisammen); im Aufrufgraph wird jedes Band (je
  Datei) für sich angeordnet, die Bänder stehen nebeneinander; steht rechts neben der Quelle einer Kante
  zurück noch ein Knoten, tritt sie unten aus; eine Schlaufe an sich selbst bekommt rechts
  neben ihrem Knoten Platz, über einer Zeile, in der ein Rahmen beginnt, sein Titel. Kreuzungen vermeidet sie nicht immer; ein
  Rahmen kann einen fremden Knoten derselben Schicht umfassen. Ein `switch` mit vielen Fällen
  wird breit. Leere Fälle stehen rechts hinter `default`, nicht in der Folge der Quelle. Zweige,
  die länger sind als der Abstand bis zur nächsten Verzweigung (ein Typ-Verteiler mit vielen
  Anweisungen je Fall, zwei lange Zweige eines `if`/`else`), stehen nebeneinander: so ein Ablauf
  wird breiter als seine breiteste Schicht — der Breiten-Test nennt ihn (`c2d --breite`).
- **Wahrheitstabellen:** Atome gelten als unabhängig — `x > 5` und `x > 10` können nicht beide
  falsch-wahr sein, die Tabelle zeigt die Zeile trotzdem. Makros werden nicht aufgelöst (`IS_OK(x)`
  ist ein Atom). Über 16 Atome keine Tabelle. Die MC/DC-Menge ist die kleinste, solange die Suche
  in ihrer Grenze bleibt (sonst ein Hinweis).
- **`#if`-Konfiguration:** Ketten in eingebundenen Köpfen stehen in der Tabelle des Kopfs (auch bei
  *Where is NAME switched?*: einmal, mit dem Stand, mit dem der Kopf selbst gelesen ist). Gerechnet wird nur mit den Zahlen
  der Bedingungen: ein Vergleich zweier Namen, ein Makro mit Parametern (`QT_VERSION_CHECK(6, 0, 0)`)
  oder `__has_attribute` bleibt Text — und weil ein späterer Zweig das Gegenteil eines früheren
  fordert, auch jeder Zweig danach. Über 8 Alternativen eine Zeile Text (lange Ketten wie die
  Compiler-Erkennung von CMake). Mit „All #if branches“ gibt es keine Tabelle.
- **Zuordnungstabellen:** nur `switch` und `match` (keine `if`-Kette); ein Fall, der noch etwas anderes tut
  (rufen, durchfallen, andere Namen setzen), macht den `switch` zu keiner Tabelle. Konstante
  Tabellen nur auf Dateiebene, nicht `const`-Arrays in einer Funktion; bei mehrdimensionalen
  Arrays wird die innere Liste zu Spalten `1`, `2`, …. Makros werden nicht ersetzt (`EMPTY_DISPATCH` steht als Wert
  in der ersten Spalte).
- **Entscheidungstabellen:** das Innere von Schleifen, `catch`-Blöcke und nicht geltende
  `#if`-Varianten stehen nicht darin (Hinweise). Ob eine Anweisung eine Bedingung ändern kann,
  wird an ihren Namen erkannt: ein Alias, den kein Name verrät (ein globaler Zeiger auf eine lokale
  Variable, ohne Aufruf dazwischen), bleibt unbemerkt.
- **Struktogramm:** kein Einbetten (vorgemerkt); eine lange Zeile wird gekürzt, nicht
  umgebrochen; ein `switch` mit vielen Fällen und tiefe Schachtelung werden breit. Anweisungen vor
  dem ersten `case` stehen nicht darin (Meldung).
- **Python:** `import` wird im Projekt noch nicht aufgelöst — ein Aufruf über Dateien hinweg
  findet seine Funktion nur am Namen (wie in C ohne Kopf). Rückgabetypen (`-> T`) und
  Vorgabewerte von Parametern stehen nicht im Diagramm, andere Dekoratoren als `@staticmethod` und
  `@abstractmethod` wirken nicht. Lambdas und Comprehensions sind Text ihrer Anweisung,
  `yield`/`await` ebenso. Ein Feld mit dem Typ einer Klasse gilt als *enthalten* (wie in C++), auch
  wenn Python nur verweist. Andere Anweisungen im Rumpf einer Klasse als Felder und Methoden stehen
  nicht im Diagramm (Hinweis). In der Entscheidungstabelle steht das `else` einer Schleife nicht
  (Hinweis), `finally` nur auf den Regeln, die den `try` normal verlassen.
- **JavaScript/TypeScript:** JSX/TSX (`.jsx`, `.tsx`) liest Code2Diagram noch nicht; JSX in einer
  `.js`-Datei gibt Meldungen. `import` wird im Projekt nicht aufgelöst (Aufrufe über Dateien am
  Namen). Methoden in Objekten tragen keinen Bereich: zwei `messageFont()` in zwei Objekten derselben
  Datei sind mehrdeutig (Meldung). Eine Klasse als Ausdruck (`const A = class {…}`) und ein statischer
  Block einer Klasse stehen nicht im Diagramm (Hinweis beim Block). Rückgabetypen, Vorgabewerte, Dekoratoren und
  Typ-Parameter stehen nicht im Diagramm. Ein Regex-Literal in einer Anweisung kann wie ein Aufruf
  aussehen (`/f(x)/` nennt `f`).
- **C#:** „Show #if variants“ und „Frame #if regions“ gelten noch nicht (Hinweis, es wird
  ausgewertet). Überladene Methoden (gleicher Name, andere Parameter) sind mehrdeutig wie in C++.
  `using` wird im Projekt nicht aufgelöst. Attribute, Typ-Parameter, `where`, Vorgabewerte stehen
  nicht im Diagramm. Ein `switch`-Ausdruck tiefer in einem Ausdruck bleibt Text; `goto case` wird im
  Ablauf nicht verfolgt (Hinweis). Mit „All #if branches“ zerfällt Code, dessen Zweige ungleich
  geklammert sind.
- **Java:** `import` wird im Projekt nicht aufgelöst (Aufrufe über Dateien am Namen). Überladene
  Methoden sind mehrdeutig wie in C++ und C#. Annotationen, Typ-Parameter, `throws` stehen nicht im
  Diagramm; ein Methodenaufruf über eine Methodenreferenz (`X::y`) ist Text. Ein `switch`-Ausdruck
  mit `:` statt `->` und einer tiefer in einem Ausdruck bleibt Text. Die Felder einer anonymen Klasse
  stehen in keinem Diagramm. In der Entscheidungstabelle verlässt ein `break marke` außerhalb von
  Schleifen den `switch` (Hinweis).
- **Kotlin:** `.kts` (Gradle) liest Code2Diagram noch nicht. `import` wird im Projekt nicht aufgelöst
  (Aufrufe über Dateien am Namen); `super.f()` meint im Aufrufgraph die eigene `f`. Ein `return` in
  einem Lambda (nicht-lokal) verlässt im Ablauf das Lambda, nicht die äußere Funktion. Ein Feld mit
  dem Typ einer Klasse gilt als *enthalten* (wie in C++), auch wenn Kotlin nur verweist.
  Rückgabetypen, Vorgabewerte, Annotationen und Typ-Parameter stehen nicht im Diagramm; ein zweiter
  Konstruktor ohne Rumpf (`: this(…)`) steht nur als Methode im Kasten. `when`, `if`, `try` tiefer
  in einem Ausdruck bleiben Text. Geprüft nur an eigenen Musterprojekten — fremder Kotlin-Code fehlt.
- **Zustandsautomaten nur als `switch`:** ein Automat aus `if`-Ketten
  (`if (s == A) … s = B;`), über eine Funktion (`setze_zustand(B)`) oder über eine Tabelle von
  Funktionszeigern wird nicht erkannt. Ein berechneter Wert (`s = ok ? A : B`) wird ein Zustand
  mit diesem Text. Ein Fall, der ohne `break` in den nächsten fällt, gibt keinen Übergang. Von
  außen zählt nur die eigene Datei.
- **draw.io legt die Kanten selbst:** die Datei trägt die Orte der Knoten aus der Anordnung,
  die Wege der Kanten nicht (`orthogonalEdgeStyle`).
- **Der Aufrufgraph** erkennt Aufrufe am Namen vor `(`, wie der Sprung: ein Funktionszeiger
  (`p->rueckruf(x)`) wird ein fremder Knoten mit dem Namen des Glieds, nicht die Funktion, auf
  die er zeigt; ein Aufruf im Rumpf eines Makros ist nicht zu sehen (Makros werden nicht
  ersetzt), das Makro selbst schon; `Motor m(3);` gilt als Aufruf von `m`. Überladungen trennt nur
  ihr Ort; in C++ meint `a.size()` darum jedes `size` — ohne die Grenze von 300 Funktionen hätte
  UART ab `main` aller Stufen 3762 Knoten (die Anordnung bräuchte Minuten). Nach unten wird ein
  Graph mit vielen Gerufenen sehr breit (LLC, `main` aller Stufen: 166 Knoten auf 43 000 px). Rahmen je Datei stehen in Bändern nebeneinander; eine Kante, die über
  mehrere Schichten nach links führt, biegt gleich unter ihrer Quelle ab.
- **Links auf Typen** erkennen den Namen, nicht den Typ eines Ausdrucks: `ampel->phase` führt
  nicht zu `Ampel` (über den Parameter im Startknoten schon). Eine Variable, die heißt wie ein
  Typ des Projekts, wird ein Link. Typen von Methoden (Rückgabe, Parameter) sind keine Links.
- **Im Projekt:** die Datentypen zeigt das Fenster je Datei
  (`c2d --struktur` auf einen Ordner: alle zusammen, ohne Beziehungen zwischen Dateien zu
  prüfen). Ein Ordner wird bei jeder Änderung der Defines oder der Projektdatei ganz neu gelesen
  (EtherCAT, 1438 Dateien: rund 5 s). Ordnermuster unterscheiden Groß und Klein.
- Ein Funktionszeiger als Glied gibt keine Beziehung zu den Typen seiner Parameter.
- Code, den kein Weg erreicht (hinter `return`), steht als unverbundener Knoten im Diagramm.
- Gebaut und geprüft ist nur Windows mit Release-clang. Die DOT-Ausgabe ist gegen erwarteten
  Text geprüft, nicht mit Graphviz; die draw.io-Ausgabe als wohlgeformtes XML, nicht in draw.io.
