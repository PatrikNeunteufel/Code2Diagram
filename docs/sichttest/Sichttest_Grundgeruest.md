# Sichttest: Grundgerüst

**Anwendung:** Code2Diagram

Exe: `out/build/windows-ninja-release-clang/exec/Code2Diagram/bin/Release/Code2Diagram.exe`

Geprüft wird die Version 0.1.0: das Fenster, das Lesen von C und C++, die zwei Diagrammarten
(Ablauf einer Funktion, Datentypen), die Vorschau mit Sprung und Einbettung und die vier Formate
(SVG, Mermaid, DOT, draw.io). Die Abschnitte A bis D prüfen den Text (Ansicht **Text**; zum Ansehen
in [mermaid.live](https://mermaid.live) einfügen), E bis G die gezeichnete Vorschau.

## A. Fenster und Beispiel

- [ ] **A0 Vorbereiten:** Das eingebaute Beispiel zeigen. `aktion: beispiel_zeigen`
- [ ] **A1 Zwei Seiten nebeneinander:** Links steht der Quelltext der Ampel (`ampel.c`),
      rechts oben die Auswahl **Diagram**, **Format** und die Richtung, darunter die Ansichten
      **Preview** (das Diagramm gezeichnet) und **Text**. Die Textfelder in einer Schrift fester
      Breite, keines lässt sich bearbeiten. Der Fenstertitel beginnt mit `ampel.c`.
- [ ] **A2 Die Statuszeile nennt die Lage:** `ampel.c, read as C: 2 function(s), 2 data type(s)`
      — ohne Meldungen.
- [ ] **A3 Der Teiler:** Die Grenze zwischen beiden Seiten lässt sich ziehen; beide folgen.
- [ ] **A4 Die Auswahl Diagram:** Sie bietet `dauer_ms()`, `ampel_takt()` und
      `Data types (2)`; gewählt ist `dauer_ms()`.

## B. Ablauf einer Funktion

- [ ] **B0 Vorbereiten:** Das Beispiel zeigen und `ampel_takt` wählen.
      `aktion: beispiel_zeigen` `aktion: diagramm_waehlen name=ampel_takt` `aktion: format_waehlen name=mermaid`
- [ ] **B1 Der Text ergibt das richtige Bild:** Diagram > Copy diagram text, in mermaid.live
      einfügen. Zu sehen: der Start mit `ampel_takt(Ampel *ampel, uint16_t vergangen_ms)`, eine
      Raute `ampel->stoerung`, deren `yes` in einen Kasten mit zwei Zeilen führt
      (`ampel->phase = GELB`, `return`), eine zweite Raute `ampel->rest_ms > vergangen_ms`,
      ein Kasten mit den zwei letzten Zuweisungen, und ein Ende, in das alle drei Wege münden.
      Kein Zeichen ist verstümmelt (`->` und `>` stehen als Zeichen da, nicht als `#gt;`).
- [ ] **B2 Ein switch:** `dauer_ms()` wählen und den Text in mermaid.live ansehen. Eine Raute
      `phase` mit vier Kanten: `ROT`, `GRUEN`, `ROT_GELB, GELB` (zwei Fälle, eine Kante) und
      `otherwise` zum `return 0`.
      `aktion: diagramm_waehlen name=dauer_ms`
- [ ] **B3 Die Richtung:** Auf `Left-right` stellen — die erste Zeile des Diagramms heisst
      jetzt `flowchart LR`, das Bild in mermaid.live wächst nach rechts.

## C. Datentypen

- [ ] **C0 Vorbereiten:** Die Datei mit den Datentypen öffnen und sie wählen.
      `aktion: datei_oeffnen pfad=beispiele/datentypen.c` `aktion: diagramm_waehlen name=types` `aktion: format_waehlen name=mermaid`
- [ ] **C1 Fünf Kästen:** In mermaid.live: `Farbe` (enumeration, drei Werte, `GRUEN = 4`),
      `Punkt`, `Knoten`, `anonymous_wert` (union) und `Liste`, je mit ihren Gliedern.
      `Knoten` zeigt unter anderem `u8 daten[16]`, `unsigned bereit : 1` und den
      Funktionszeiger `void (*rueckruf)(struct Knoten *k, int grund)`.
- [ ] **C2 Die Beziehungen:** Von `Knoten` eine gefüllte Raute zu `Punkt` (`ort`) und zu
      `anonymous_wert` (`wert`), ein Pfeil zu `Farbe` (`farbe`) und einer zu sich selbst
      (`naechster`). Von `Liste` ein Pfeil zu `Knoten` (`kopf`) und eine Raute zu `Punkt`
      (`mitte`) — beide über einen `typedef`.

## D. Formate und Dateien

- [ ] **D0 Vorbereiten:** Das Beispiel zeigen, DOT wählen.
      `aktion: beispiel_zeigen` `aktion: format_waehlen name=dot`
- [ ] **D1 DOT:** Der Text beginnt mit `digraph "dauer_ms" {` und endet mit `}`. Wer Graphviz
      hat: der Text ergibt dasselbe Bild wie B2 (Raute, vier beschriftete Kanten).
- [ ] **D2 Speichern:** File > Save diagram as… schlägt `ampel_dauer_ms.dot` vor; die
      abgelegte Datei enthält genau den Text aus dem Fenster.
- [ ] **D3 File > Open...:** Der Dialog bietet die Filter „C files (*.c *.h)", „C++ files
      (*.cpp … *.h)" und „All files".
      Eine gewählte C-Datei erscheint links, ihre Funktionen stehen in der Auswahl.
- [ ] **D4 Eine kaputte Datei:** Eine C-Datei öffnen, in der ein Semikolon oder eine Klammer
      fehlt. Das Fenster zeigt weiter, was lesbar ist; die Statuszeile nennt die Zahl der
      Meldungen und die erste mit ihrer Zeile.
- [ ] **D5 Help > About:** Nennt `Code2Diagram` und die Version aus `Solution.json` (0.1.0).

## E. Vorschau: Ablauf, Sprung, Einbettung

- [ ] **E0 Vorbereiten:** Die Datei mit den Aufrufen öffnen, `main` wählen, Vorschau zeigen.
      `aktion: datei_oeffnen pfad=beispiele/aufrufe.c` `aktion: diagramm_waehlen name=main` `aktion: ansicht_waehlen name=preview`
- [ ] **E1 Das Bild von main:** Oben der Start `main()` mit runden Enden, darunter ein Kasten
      `int n = lies()`, eine Raute `n < 0` mit den Kanten `yes` und `no`, zwei Kästen in einer
      Reihe (`return 1`, `drucke(summe(n))`), `return 0` und das Ende. Jede Kante trägt eine
      Pfeilspitze, keine Schrift ist abgeschnitten, nichts überdeckt sich.
- [ ] **E2 Wer weiterführt, ist hervorgehoben:** `drucke(summe(n))` ist farbig umrandet und hat
      doppelte Seiten (ein Aufruf); die anderen Kästen nicht.
- [ ] **E3 Vergrößern und Verschieben:** Strg + Mausrad vergrößert und verkleinert um den
      Zeiger; mit gedrückter Maustaste lässt sich das Bild ziehen.
- [ ] **E4 Sprung zur gerufenen Funktion:** Doppelklick auf `drucke(summe(n))` — die Auswahl
      Diagram steht auf `summe()`, die Vorschau zeigt deren Ablauf mit der Schleife (`i < n`,
      eine Kante führt rechts außen zurück zur Raute). Diagram > Back (oder Alt + Pfeil links)
      zeigt wieder `main()`.
- [ ] **E5 Einbetten über das Kontextmenü:** Rechtsklick auf `drucke(summe(n))` bietet
      *Go to summe()* und *Embed summe() here*. Nach *Embed* hängt unter dem Aufruf ein
      gestrichelter Rahmen mit dem Titel `summe(int n)`; in ihm der Ablauf von summe ohne Start
      und Ende. Aus dem Rahmen führt `return s` weiter zu `return 0`.
- [ ] **E6 Einbetten in der Einbettung:** Im Rahmen ist `s += quadrat(i)` hervorgehoben.
      Rechtsklick darauf, *Embed quadrat() here*: ein zweiter Rahmen `quadrat(int x)` liegt im
      ersten, mit `return x * x`.
- [ ] **E7 Der Text folgt:** Ansicht **Text**, Format mermaid: zwei `subgraph`, der zweite
      eingerückt im ersten. In mermaid.live ergibt er dasselbe Bild mit zwei Rahmen.
      `aktion: ansicht_waehlen name=text` `aktion: format_waehlen name=mermaid`
- [ ] **E8 Wieder herausnehmen:** In der Vorschau Rechtsklick in den inneren Rahmen, *Remove
      embedding: quadrat(int x)* — nur er verschwindet. Diagram > Remove all embeddings: das
      Bild ist wieder das aus E1.
      `aktion: ansicht_waehlen name=preview`
- [ ] **E9 Zeile im Quelltext:** Doppelklick auf `return 1` hinterlegt links die Zeile mit
      `return 1;` und holt sie in die Mitte.
- [ ] **E10 Nach rechts:** Richtung `Left-right` — der Ablauf wächst nach rechts, der Start
      steht links; nichts überdeckt sich.

## F. C++

- [ ] **F0 Vorbereiten:** Das C++-Beispiel öffnen, `starte` wählen.
      `aktion: datei_oeffnen pfad=beispiele/motor.cpp` `aktion: diagramm_waehlen name=antrieb::Motor::starte` `aktion: ansicht_waehlen name=preview`
- [ ] **F1 Die Statuszeile:** `motor.cpp, read as C++: 8 function(s), 4 data type(s)` — ohne
      Meldungen. Die Auswahl Diagram nennt die Methoden mit Bereich
      (`antrieb::Motor::starte()`, `antrieb::Motor::operator+=()` …) und `begrenze()`.
- [ ] **F2 try, catch, range-for:** Eine Raute `try`: ohne Beschriftung in `pruefe(m_polpaare)`,
      mit `catch (const std::exception& e)` in `melde(e.what())`. Der Fang endet in einem
      Kasten mit `throw`, der zum Ende führt. Darunter die Raute
      `const auto& wert : m_verlauf` mit den Kanten `each` und `done`.
- [ ] **F3 Die Datentypen:** `Data types (4)` wählen. Vier Kästen, die Namen mit Namensraum
      (`antrieb::Motor`). `antrieb::Geraet` steht über `antrieb::Motor`, dazwischen ein Pfeil
      mit hohler Spitze (erbt) und daneben einer mit `m_partner`. Glieder tragen `-`, Methoden
      `+`, der Konstruktor von Geraet `#`; `starte()` und `stoppe()` in Geraet stehen kursiv
      (abstrakt). Zu `antrieb::Messwert` führt eine Linie mit gefüllter Raute an Motor
      (`m_verlauf`), zu `antrieb::Zustand` ein Pfeil (`m_zustand`).
      `aktion: diagramm_waehlen name=types`

## G. draw.io

- [ ] **G0 Vorbereiten:** Das Beispiel mit den Aufrufen, `main` mit eingebettetem `summe`, als
      draw.io. `aktion: datei_oeffnen pfad=beispiele/aufrufe.c` `aktion: diagramm_waehlen name=main` `aktion: einbetten name=summe` `aktion: format_waehlen name=drawio` `aktion: ansicht_waehlen name=text`
- [ ] **G1 Die Datei öffnet sich in draw.io:** File > Save diagram as… (`aufrufe_main.drawio`),
      dann in [draw.io](https://app.diagrams.net) öffnen. Das Bild entspricht der Vorschau:
      dieselben Formen (Start und Ende rund, Raute, Aufruf mit doppelten Seiten), dieselben
      Beschriftungen an den Kanten, der Rahmen `summe(int n)` um seinen Inhalt.
- [ ] **G2 Es lässt sich bearbeiten:** Einen Kasten verschieben — seine Kanten folgen. Den
      Rahmen verschieben — sein Inhalt geht mit.
- [ ] **G3 Die Datentypen in draw.io:** `motor.cpp`, `Data types`, als draw.io ablegen und
      öffnen: vier Kästen mit Kopf, Gliedern und Methoden; die Vererbung mit hohler Spitze, die
      Komposition mit gefüllter Raute.

## H. Bedingte Übersetzung

- [ ] **H0 Vorbereiten:** Das Beispiel mit den Varianten öffnen, ohne Definitionen, `melde`
      wählen. `aktion: zweige_waehlen name=auswerten` `aktion: definitionen_setzen wert=""` `aktion: datei_oeffnen pfad=beispiele/varianten.c` `aktion: diagramm_waehlen name=melde` `aktion: ansicht_waehlen name=preview`
- [ ] **H1 Gezeigt wird, was gilt:** Der Ablauf von `melde` hat nur die Schleife mit
      `sende(daten[i])` — kein `protokolliere`, kein `pruefe`. Unter dem Quelltext stehen das
      Feld **Defines** (leer) und die Haken **All #if branches**, **Show #if variants** und
      **Frame #if regions** (alle aus). Die Statuszeile endet mit
      `not defined in #if: BOARD_A, BOARD_B, LOG_STUFE, MIT_PRUEFSUMME, FILTER_STUFE, MIT_KUEHLUNG`.
- [ ] **H2 Definitionen wählen die Variante:** In Defines `LOG_STUFE=2 MIT_PRUEFSUMME`
      eintragen, Enter. Das Diagramm bleibt bei `melde` und zeigt jetzt zuerst
      `protokolliere("melde", laenge)` und in der Schleife die Raute `!pruefe(daten[i])`, deren
      `yes` die Schleife verlässt. Die Statuszeile nennt nur noch
      `BOARD_A, BOARD_B, FILTER_STUFE, MIT_KUEHLUNG`.
      `aktion: definitionen_setzen wert="LOG_STUFE=2 MIT_PRUEFSUMME"`
- [ ] **H3 Eine andere Funktion je Board:** `sende()` wählen: ihr Ablauf enthält nur
      `(void)byte`. Mit `BOARD_B` in Defines (zusätzlich) steht dort `spi_schreibe(byte)`.
- [ ] **H4 Alle Zweige:** Den Haken **All #if branches** setzen: das Feld Defines wird grau,
      die Auswahl Diagram nennt `sende()` nur einmal (die erste von dreien), und `melde` zeigt
      `protokolliere` und `pruefe` unabhängig von den Definitionen. Der Haken **Show #if
      variants** ist grau. Haken wieder weg: die Variante von vorher.
      `aktion: zweige_waehlen name=alle`
- [ ] **H5 Varianten im Ablauf:** Defines leeren, den Haken **Show #if variants** setzen,
      `melde` wählen. Unter dem Start steht ein **Sechseck** `#if` mit zwei Kanten:
      `LOG_STUFE >= 2` zu `protokolliere("melde", laenge)` und `else (active)`, die gleich
      weiter zu `index_t i = 0` führt. In der Schleife ein zweites Sechseck mit
      `defined(MIT_PRUEFSUMME)` zur Raute `!pruefe(daten[i])` und `else (active)` zu
      `sende(daten[i])`. `protokolliere` steht in einem **blauen Rahmen** `LOG_STUFE >= 2`, die
      Raute `!pruefe` in einem **orangen** `defined(MIT_PRUEFSUMME)`; rechts oben die Legende
      mit beiden. Die Beschriftungen der Kanten stehen über den Rahmen, nicht auf ihrem Titel.
      Keine Linie läuft durch einen Kasten oder eine Beschriftung.
      `aktion: definitionen_setzen wert=""` `aktion: zweige_waehlen name=varianten` `aktion: diagramm_waehlen name=melde`
- [ ] **H6 Welcher Zweig gilt, folgt den Definitionen:** In Defines `LOG_STUFE=2` eintragen,
      Enter: jetzt trägt `LOG_STUFE >= 2 (active)` den Zusatz, `else` nicht mehr. Die Form
      bleibt dieselbe — es ändert sich nur, welcher Zweig gilt.
      `aktion: definitionen_setzen wert="LOG_STUFE=2"`
- [ ] **H7 Was nicht passt, wird ausgewertet und gemeldet:** `regle` wählen. Das erste `#if`
      wird ein Sechseck mit drei Kanten (`FILTER_STUFE >= 2`, `FILTER_STUFE == 1`,
      `else (active)`). Das zweite (`#ifdef MIT_KUEHLUNG` mit `else if`) steht nicht im
      Ablauf: die Raute `ist < soll` führt mit `no` zu `halte()`. Die Meldungen nennen Zeile 45:
      `#if not shown in the flow: a branch does not hold whole statements`. Der zweite Rahmen
      heisst gekürzt `!(FILTER_STUFE >= 2)...`; ganz steht er in der Legende und im Tooltip.
      `aktion: diagramm_waehlen name=regle`
- [ ] **H8 Im Text und als Datei:** Format Mermaid, Ansicht Text: der Knoten steht als
      `n1{{"#35;if"}}`. In mermaid.live eingefügt: ein Sechseck. Format DOT: `shape=hexagon`;
      draw.io: `shape=hexagon`. Danach den Haken **Show #if variants** wieder weg.
      `aktion: format_waehlen name=mermaid` `aktion: ansicht_waehlen name=text`
- [ ] **H9 Rahmen beim Auswerten:** Format zurück auf SVG, Vorschau. Defines
      `LOG_STUFE=2 MIT_PRUEFSUMME`, den Haken **Frame #if regions** setzen, `melde` wählen. Kein
      Sechseck; `protokolliere` im blauen Rahmen `LOG_STUFE >= 2`, die Raute `!pruefe` im
      orangen `defined(MIT_PRUEFSUMME)`, die Legende rechts oben. Ohne Defines: keine Rahmen,
      keine Legende — es gilt nichts, was ein `#if` zuschaltet.
      `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview` `aktion: definitionen_setzen wert="LOG_STUFE=2 MIT_PRUEFSUMME"` `aktion: zweige_waehlen name=rahmen` `aktion: diagramm_waehlen name=melde`
- [ ] **H10 Die Haken schliessen sich aus:** Mit **Show #if variants** wird **Frame #if
      regions** grau (Varianten rahmen selbst); mit **All #if branches** beide.
- [ ] **H11 Rahmen und Einbettung:** Im Ablauf von `melde` Rechtsklick in den blauen Rahmen:
      das Menü bietet kein *Remove embedding* an (ein Rahmen ist keine Einbettung). Danach den
      Haken **Frame #if regions** wieder weg, Defines leeren.
      `aktion: zweige_waehlen name=auswerten` `aktion: definitionen_setzen wert=""`
- [ ] **H12 Das Feld Configuration:** `melde` gewählt. Rechts das Feld **Configuration**
      (sonst Project > Configuration). Unter *Names asked by #if*: `PUFFER (file, line 4)`
      grau und angehakt, mit Wert 64; `BOARD_A`, `BOARD_B`, `LOG_STUFE` (mit Wertfeld),
      `MIT_PRUEFSUMME`, `FILTER_STUFE` (mit Wertfeld), `MIT_KUEHLUNG` frei und leer. Unter
      *#if chains of the function*: `line 22` mit `LOG_STUFE >= 2` und `(no #else)` (gesetzt),
      `line 26` mit `defined(MIT_PRUEFSUMME)` und `(no #else)` (gesetzt).
- [ ] **H13 Ein Zweig setzt die Namen:** bei `line 22` auf `LOG_STUFE >= 2` klicken. Defines
      zeigt `LOG_STUFE=2`, das Diagramm hat jetzt `protokolliere`, der Knopf ist gesetzt.
      Tooltip auf `(no #else)`: *Sets no LOG_STUFE*. Darauf klicken: Defines ist wieder leer.
      `aktion: zweig_waehlen zeile=22 zweig=0`
- [ ] **H14 Haken und Wert:** `MIT_PRUEFSUMME` anhaken: Defines `MIT_PRUEFSUMME`, im
      Diagramm die Raute `!pruefe`. Bei `LOG_STUFE` 3 eintragen, Enter: Defines bekommt
      `LOG_STUFE=3`. *Whole file* anhaken: jetzt stehen auch die Ketten in Zeile 6, 12, 38 und
      45 da. Danach Defines leeren.
      `aktion: schalter_setzen name=MIT_PRUEFSUMME an=ja` `aktion: definitionen_setzen wert=""`
- [ ] **H15 In die Projektdatei (im Projekt):** `beispiele/projekt` als Ordner öffnen, einen
      Namen anhaken, **Save to project file**: Project > Edit project file zeigt ihn unter
      `definitionen`. Defines leeren: der Haken bleibt frei, mit `(project)` dahinter. Danach
      den Eintrag in der Projektdatei wieder entfernen.
- [ ] **H16 Nicht definiert (im Projekt):** `beispiele/projekt`, Datei `log/log.c`, `protokolliere`.
      Im Feld `MIT_LOG (project)` angehakt, der Ablauf hat `schreibe`. Abwählen: Defines
      zeigt `!MIT_LOG`, der Haken heisst `MIT_LOG (project, !MIT_LOG)`, `schreibe` ist weg;
      Tooltip *Set by the project file, undefined by !MIT_LOG in Defines*. Wieder anhaken:
      Defines leer. Dann `main.c`, `main`: `MIT_DIAGNOSE (header)` angehakt, abwählen nimmt
      `protokolliere` aus der Schleife — Defines `!MIT_DIAGNOSE`. Defines leeren.
      `aktion: schalter_setzen name=MIT_DIAGNOSE an=nein` `aktion: definitionen_setzen wert=""`

## I. Das eigene Format SVG; ein switch in Spalten

- [ ] **I0 Vorbereiten:** Das Beispiel zeigen, `dauer_ms` in der Vorschau.
      `aktion: beispiel_zeigen` `aktion: diagramm_waehlen name=dauer_ms` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview`
- [ ] **I1 SVG ist die Vorgabe:** Nach dem Start steht das Feld **Format** auf `svg`; die Liste
      bietet `svg`, `mermaid`, `dot`, `drawio`. Die Ansicht **Text** beginnt mit `<svg xmlns=`.
- [ ] **I2 Jeder Fall seine Spalte:** In der Vorschau führt aus der Raute des `switch` eine
      gemeinsame Linie zu den Fällen. Jede Beschriftung steht **unter** dieser Linie, auf der
      Senkrechten ihres Falls — keine auf der gemeinsamen Linie, keine zwei übereinander. Keine
      Kante läuft außen um das Diagramm herum oder durch einen Knoten.
- [ ] **I3 Die Datei zeigt, was die Vorschau zeigt:** File > Save diagram as… als `.svg`
      ablegen und im Browser öffnen: dieselben Formen, Wege und Beschriftungen wie in der
      Vorschau, nichts abgeschnitten. Im dunklen Modus des Browsers dunkle Farben, sonst helle.
- [ ] **I4 Datentypen als SVG:** `motor.cpp`, `Data types`, als `.svg` ablegen und im Browser
      öffnen: Kästen mit grauem Kopf, die Art in Guillemets (`«struct»`), Glieder und Methoden;
      die Vererbung mit hohler Spitze, die Komposition mit gefüllter Raute.

## J. Ein Ordner als Projekt

`beispiele/projekt/`: `main.c` ruft Funktionen aus `antrieb/`, `log/` und `sim/`; die
Projektdatei setzt `MIT_LOG`, den Include-Pfad `inc`, für `antrieb` `MOTOREN=2`, und übergeht
`alt`. J8 und J9 schreiben die Projektdatei — danach muss sie wieder genau so dastehen
(`git diff beispiele/projekt` ist leer).

- [ ] **J0 Vorbereiten:** Den Ordner öffnen (File > Open folder…), Vorschau.
      `aktion: zweige_waehlen name=auswerten` `aktion: definitionen_setzen wert=""` `aktion: ordner_oeffnen pfad=beispiele/projekt` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview`
- [ ] **J1 Der Baum:** Über dem Quelltext steht der Baum: `antrieb` (`motor.c`, `motor.h`,
      `regler.c`), `inc` (`config.h`), `log` (`log.c`), `main.c`, `sim` (`regler_sim.c`) und grau
      `alt  (skipped)`. Gewählt ist `antrieb/motor.c`, die erste Datei mit einer Funktion; zur
      Wahl stehen `pruefe()`, `motor_starte()` und `motor_zwei_gleich()` (`MOTOREN=2` aus dem
      Abschnitt). Die Statuszeile beginnt mit
      `projekt: 7 file(s), 8 function(s), 1 folder(s) skipped, 1 project message(s) |`.
- [ ] **J2 Eine andere Datei:** Im Baum `main.c` anklicken, `main()` wählen. Der Ablauf zeigt
      `protokolliere("Takt")` — `MIT_DIAGNOSE` kommt aus `inc/config.h`. Blau umrandet sind
      `!pruefe()`, `motor_starte(2)`, `regle(100)` und `protokolliere("Takt")`.
      `aktion: datei_waehlen pfad=main.c` `aktion: diagramm_waehlen name=main`
- [ ] **J3 Sprung in eine andere Datei:** Doppelklick auf `motor_starte(2)`: gezeigt wird
      `motor_starte()` aus `antrieb/motor.c`, im Baum ist `motor.c` markiert, links steht sein
      Quelltext. Diagram > Back führt zu `main()` in `main.c` zurück.
      `aktion: springen name=motor_starte` `aktion: zurueck`
- [ ] **J4 static bleibt in seiner Datei:** Doppelklick auf `!pruefe()`: gezeigt wird `pruefe()`
      aus `main.c` (ohne Parameter) — nicht die gleichnamige aus `antrieb/motor.c`. Zurück.
- [ ] **J5 Mehrdeutig: man wählt:** Doppelklick auf `regle(100)`: ein Menü *Which function?*
      bietet `regle() - antrieb/regler.c:4` und `regle() - sim/regler_sim.c:5`, die Statuszeile
      sagt `The call is ambiguous: 2 functions qualify`. Die zweite wählen: gezeigt wird `regle()`
      aus `sim/regler_sim.c` (eine Zeile `ist = soll`). Zurück.
      `aktion: springen name=regle wahl=1` `aktion: zurueck`
- [ ] **J6 Einbetten aus einer anderen Datei:** Rechtsklick auf `motor_starte(2)`:
      *Embed motor_starte() in antrieb/motor.c here*. Der Ablauf steht umrahmt unter
      `motor_starte(int anzahl)`. Rechtsklick auf `bereit[i] = 1` im Rahmen:
      *Show line 15 in the source (antrieb/motor.c)* — links steht `motor.c`, Zeile 15 hinterlegt.
      `aktion: einbetten name=motor_starte`
- [ ] **J7 Mehrdeutig im Kontextmenü:** Rechtsklick auf `regle(100)`: ein Untermenü
      `regle(): 2 functions qualify` mit je *Go to* und *Embed here* für beide Dateien, mit Pfad
      und Zeile.
- [ ] **J8 Die Projektdatei:** Project > Edit project file…: der Text der Projektdatei mit
      `"alt"` unter `uebergehen`. Ans Ende ein `x` schreiben, Save: der Dialog bleibt offen, rot
      steht `Line …, column …: not valid JSON: …`. Cancel — nichts wurde geschrieben.
- [ ] **J9 Einen Ordner übergehen:** Rechtsklick im Baum auf `sim`: *Skip this folder
      (project file)*. `sim` steht grau als `(skipped)`, in der Projektdatei steht `"sim"`. Ein
      Doppelklick auf `regle(100)` führt jetzt ohne Menü zu `antrieb/regler.c`. Zurück;
      Rechtsklick auf `sim  (skipped)`: *Read this folder again* — alles wie vorher.
- [ ] **J10 Defines gehen der Projektdatei vor:** `antrieb/motor.c` wählen, in Defines
      `MOTOREN=1`, Enter: `motor_zwei_gleich()` steht nicht mehr zur Wahl, die Datei bleibt
      gewählt. Defines leeren, Enter.
      `aktion: datei_waehlen pfad=antrieb/motor.c` `aktion: definitionen_setzen wert="MOTOREN=1"` `aktion: definitionen_setzen wert=""`
- [ ] **J11 Project > Show project messages:** eine Meldung `alt: folder skipped ("uebergehen": "alt")`.
- [ ] **J12 Zurück zum Aufruf:** In `main()` Doppelklick auf `motor_starte(2)`, dann in der
      Leiste oben **Back**: wieder `main()`, und um `motor_starte(2)` liegt ein Ring — der Aufruf,
      von dem man kam. Der Knopf Back ist grau, wenn es nichts gibt, wohin er führt; sein Tooltip
      nennt das Ziel.
      `aktion: datei_waehlen pfad=main.c` `aktion: diagramm_waehlen name=main` `aktion: springen name=motor_starte` `aktion: zurueck`
- [ ] **J13 Called from:** `antrieb/motor.c`, `motor_starte()` wählen. Der Knopf **Called from**
      (auch Rechtsklick im Diagramm > *Called from*) zeigt `main() - main.c:14`; ein Klick führt
      zu `main()` in `main.c`, der Aufruf `motor_starte(2)` ist dort umringt. Bei `main()` steht
      im Menü `No caller in the project`.
      `aktion: datei_waehlen pfad=antrieb/motor.c` `aktion: diagramm_waehlen name=motor_starte` `aktion: zum_aufrufer wahl=0`

## K. Zustandsautomaten

- [ ] **K0 Vorbereiten:** `beispiele/automat.c` öffnen, Vorschau.
      `aktion: datei_oeffnen pfad=beispiele/automat.c` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview`
- [ ] **K1 Erkannt:** Die Auswahl Diagram bietet nach `motor_takt()` und `motor_quittiere()` den
      Eintrag `State machine: motor.zustand in motor_takt()`; die Statuszeile endet mit
      `1 state machine(s)`.
- [ ] **K2 Gezeichnet:** Den Automaten wählen. Vier Zustände mit runden Ecken — `AUS`, `STARTE`,
      `LAEUFT`, `FEHLER` — und oben ein gefüllter Punkt. Pfeile: Punkt → `AUS` mit
      `in motor_quittiere()`, `AUS` → `STARTE` mit `bereit`, `STARTE` → `FEHLER` mit
      `stoerung && motor.versuche > 3`, `STARTE` → `LAEUFT` mit `!(stoerung)`, `LAEUFT` → `FEHLER`
      mit `stoerung`, `LAEUFT` → `AUS` mit `!(stoerung) && !bereit`. Keine Beschriftung liegt auf
      einer anderen.
      `aktion: diagramm_waehlen name=automat:motor.zustand`
- [ ] **K3 Als Text:** Format mermaid, Ansicht Text: `stateDiagram-v2`, der Punkt als `[*]`. In
      mermaid.live ergibt es dasselbe Bild.
      `aktion: format_waehlen name=mermaid` `aktion: ansicht_waehlen name=text`
- [ ] **K4 An echtem Code (wenn vorhanden):** `../LLC/firmware/Global/Source_Files/I2C.c`
      öffnen: zwei Automaten, `ptr_Handler->Bus_State in I2C_PROCESS()` (11 Zustände und der Punkt
      von außen aus `CHECK_TIMEOUT()`) und `Scan_State in I2CBusScan()`. Die Beschriftungen stehen
      nebeneinander, nicht übereinander.
