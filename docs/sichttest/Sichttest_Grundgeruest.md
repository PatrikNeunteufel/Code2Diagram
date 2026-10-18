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
- [ ] **A4 Die Auswahl Diagram:** Sie bietet `dauer_ms()`, `ampel_takt()`,
      `Data types (2)` und `Call graph of this file`; gewählt ist `dauer_ms()`.
- [ ] **A5 Open recent:** `beispiele/ampel.c` öffnen, dann den Ordner `beispiele/projekt`.
      **File > Open recent**: oben `…\beispiele\projekt/` mit Ordnersymbol, darunter
      `…\beispiele\ampel.c` mit Dateisymbol, vorn die Ziffern 1, 2. Der zweite Eintrag öffnet
      wieder die Datei, der Baum verschwindet; im Menü steht sie jetzt oben. Das Beispiel (A0)
      kommt nicht in die Liste. Die Anwendung schließen und neu starten: die Liste ist noch da.
      *Clear list* leert sie: `(nothing opened yet)`.
      `aktion: datei_oeffnen pfad=beispiele/ampel.c` `aktion: ordner_oeffnen pfad=beispiele/projekt` `aktion: zuletzt_oeffnen wahl=1`

## B. Ablauf einer Funktion

- [ ] **B0 Vorbereiten:** Das Beispiel zeigen und `ampel_takt` wählen.
      `aktion: beispiel_zeigen` `aktion: diagramm_waehlen name=ampel_takt` `aktion: format_waehlen name=mermaid`
- [ ] **B1 Der Ablauf stimmt mit dem Code überein:** *Zu prüfen:* Gibt das Flussdiagramm von
      `ampel_takt` den Quelltext links richtig wieder — Reihenfolge, Verzweigungen, Wege?
      *So:* Diagram > Copy diagram text, in mermaid.live einfügen und das Bild dort mit dem
      Quelltext vergleichen (die Vorschau im Fenster zeigt dasselbe, sie ist Thema von E).
      *Richtig ist:* von oben nach unten der Start `ampel_takt(Ampel *ampel, uint16_t vergangen_ms)`;
      die Raute `ampel->stoerung` — `yes` führt in einen Kasten mit zwei Zeilen
      (`ampel->phase = GELB`, `return`), `no` zur zweiten Raute `ampel->rest_ms > vergangen_ms`;
      deren `yes` in einen Kasten (`ampel->rest_ms -= vergangen_ms`, `return`), `no` in einen
      Kasten mit den zwei letzten Zuweisungen; alle drei Kästen münden ins Ende. Kein Zeichen ist
      verstümmelt (`->` und `>` stehen als Zeichen da, nicht als `#gt;`).
- [ ] **B2 Ein switch:** *Zu prüfen:* Ein `switch` wird eine Raute mit einer Kante je Fall.
      *So:* `dauer_ms()` wählen, Text wie in B1 in mermaid.live. *Richtig ist:* eine Raute
      `phase` mit vier Kanten: `ROT`, `GRUEN`, `ROT_GELB, GELB` (zwei Fälle, eine Kante) und
      `otherwise` zum `return 0`.
      `aktion: diagramm_waehlen name=dauer_ms`
- [ ] **B3 Die Richtung:** *Zu prüfen:* `Left-right` dreht das Diagramm. *So:* oben auf
      `Left-right` stellen. *Richtig ist:* die erste Zeile des Texts heisst `flowchart LR`, das
      Bild in mermaid.live wächst nach rechts; in der Vorschau stehen die Beschriftungen der
      Kanten frei zwischen Raute und Kasten, auf keinem Kasten.

## C. Datentypen

- [ ] **C0 Vorbereiten:** Die Datei mit den Datentypen öffnen und sie wählen.
      `aktion: datei_oeffnen pfad=beispiele/datentypen.c` `aktion: diagramm_waehlen name=types` `aktion: typ_waehlen name=*` `aktion: format_waehlen name=mermaid`
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
      doppelte Seiten (ein Aufruf); die anderen Kästen nicht — auch `int n = lies();` nicht:
      hervorgehoben ist nur ein Aufruf, dessen Funktion es in der Datei (im Projekt) gibt.
- [ ] **E3 Vergrößern und Verschieben:** Strg + Mausrad vergrößert und verkleinert um den
      Zeiger; mit gedrückter Maustaste lässt sich das Bild ziehen, sobald es größer ist als die
      Fläche (passt es ganz hinein, bewegt es sich nicht — so gewollt).
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
- [ ] **E9 Zeile im Quelltext:** Ein Klick auf `return 1` hinterlegt links die Zeile mit
      `return 1;` und holt sie in die Mitte. Ein Doppelklick darauf tut nichts weiter (kein Ziel).
      `aktion: klicken name=return 1`
- [ ] **E9a Unerreichbarer Code:** Ordner `beispiele/projekt`, `main.c`, `main()`: unter dem
      Ablauf, links, ein grauer gestrichelter Kasten `(unreachable)` / `return 0`, ohne Kante; die
      Statuszeile sagt `main(): code from line 25 on is never reached`. Die Raute `1` hat nur `yes`.
      `aktion: ordner_oeffnen pfad=beispiele/projekt` `aktion: datei_waehlen pfad=main.c` `aktion: diagramm_waehlen name=main`
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

## L. Aufrufgraph

Am Beispielprojekt `beispiele/projekt`. `main()` in `main.c` ruft `pruefe()` (die eigene,
`static`), `motor_starte()` (in `antrieb/motor.c`), `regle()` (zwei Dateien definieren es) und —
unter `#ifdef MIT_DIAGNOSE`, das `inc/config.h` definiert — `protokolliere()` (in `log/log.c`), das
unter `#ifdef MIT_LOG` die fremde `schreibe()` ruft.

- [ ] **L0 Vorbereiten:** Das Projekt öffnen, `main.c` wählen, Vorschau, Richtung Top-down.
      `aktion: ordner_oeffnen pfad=beispiele/projekt` `aktion: datei_waehlen pfad=main.c` `aktion: zweige_waehlen name=auswerten` `aktion: aufrufgraph_einstellen richtung=calls tiefe=2 hoechstens=300 fremde=ja` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview`
- [ ] **L1 Die Leiste:** Oben neben **Back** und **Called from** stehen **Call graph from here**,
      die Richtung **Calls**, **Depth: 2**, **Max: 300** und **Hide external calls** (frei). In Diagram steht als
      letzter Eintrag `Call graph of this file`.
- [ ] **L2 Ab main:** Diagram `main()`, dann **Call graph from here**. In Diagram steht jetzt
      `Call graph from main()`. *Zu prüfen:* Stimmt der Graph mit dem Code links überein?
      *So:* Jede Datei steht in einem gestrichelten Rahmen mit ihrem Pfad als Titel, ganz zu
      lesen: `main.c` (darin `main`, kräftig umrandet und getönt, und `pruefe`),
      `antrieb/motor.c` (`motor_starte`, darunter dessen eigenes `pruefe`), `antrieb/regler.c` und
      `sim/regler_sim.c` (je `regle`), `log/log.c` (`protokolliere`). Rechts außerhalb aller Rahmen
      `schreibe`, grau und kursiv. Pfeile von `main` zu `pruefe`, `motor_starte`, beiden `regle`
      und `protokolliere`; von `motor_starte` zu seinem `pruefe`; von `protokolliere` zu
      `schreibe`. *Richtig ist:* Die beiden Pfeile zu `regle` sind gestrichelt und tragen `?`, die
      übrigen durchgezogen. Kein Rahmen überdeckt einen anderen oder einen fremden Knoten.
      `aktion: diagramm_waehlen name=main` `aktion: aufrufgraph_zeigen name=main`
- [ ] **L3 Die Statuszeile:** Sie endet mit `call graph: 8 node(s), 5 file(s), 1 ambiguous call(s),
      first main.c:20: call graph: 'regle' in main() is ambiguous - it may mean
      antrieb/regler.c:4, sim/regler_sim.c:5`.
- [ ] **L4 Tiefe 1:** **Depth** auf 1. `motor_starte …` und `protokolliere …` tragen drei Punkte —
      sie rufen weiter, gezeigt ist es nicht; `pruefe` unter `motor_starte` und `schreibe` sind
      verschwunden. Bei *all* (0) ist alles wieder da.
      `aktion: aufrufgraph_einstellen tiefe=1`
- [ ] **L5 Aufklappen:** Rechtsklick auf `motor_starte …` > *Expand one level*: sein `pruefe`
      erscheint, die Punkte sind weg. Rechtsklick auf `motor_starte` > *Collapse*: wie in L4. Das
      Menü bietet daneben *Show the flow of motor_starte() in antrieb/motor.c*, *Call graph from
      motor_starte() in antrieb/motor.c*, *Show line 12 in the source (antrieb/motor.c)* und
      *Back to …*.
      `aktion: aufklappen name=motor_starte`
- [ ] **L6 Fremde ausblenden:** Depth wieder 2, **Hide external calls** anhaken: `schreibe`
      verschwindet, die Statuszeile nennt `1 external call(s) hidden`. Wieder abhaken.
      `aktion: aufrufgraph_einstellen tiefe=2 fremde=nein` `aktion: aufrufgraph_einstellen fremde=ja`
- [ ] **L6a Höchstens:** **Max** auf 3: nur `main`, `pruefe` und `motor_starte` bleiben — `regle`,
      `protokolliere` und `schreibe` fehlen —, `main …` und `motor_starte …` tragen drei Punkte, die
      Statuszeile endet mit `call graph stopped at 3 functions - lower the depth or expand by hand`.
      Rechtsklick auf `main …` > *Expand one level*: alle Gerufenen von `main` sind wieder da. Max
      zurück auf 300.
      `aktion: aufrufgraph_einstellen hoechstens=3` `aktion: aufklappen name=main` `aktion: aufrufgraph_einstellen hoechstens=300`
- [ ] **L7 Doppelklick und zurück:** Doppelklick auf `motor_starte`: sein Ablauf, in
      `antrieb/motor.c`. **Back**: wieder der Graph ab `main`, um `motor_starte` ein Ring.
- [ ] **L8 Wer ruft:** Richtung **Callers**: der Titel (Ansicht Text, Format mermaid) heißt
      `callers of main()`, im Bild nur `main` — niemand ruft es. In `antrieb/motor.c` bei
      `motor_starte()` **Call graph from here**: `main` → `motor_starte`. Richtung **Both**:
      dazu `pruefe` unter `motor_starte`. Zurück auf **Calls**.
      `aktion: aufrufgraph_einstellen richtung=callers` `aktion: datei_waehlen pfad=antrieb/motor.c` `aktion: aufrufgraph_zeigen name=motor_starte` `aktion: aufrufgraph_einstellen richtung=both` `aktion: aufrufgraph_einstellen richtung=calls`
- [ ] **L9 Der Graph der Datei:** In `main.c` Diagram `Call graph of this file`: `pruefe` und
      `main` im Rahmen `main.c`, `motor_starte …` und `protokolliere …` mit drei Punkten in ihren
      Rahmen, die zwei `regle` gestrichelt mit `?`. Kein `schreibe` — was draußen weiter ruft,
      ist nicht gezeigt.
      `aktion: datei_waehlen pfad=main.c` `aktion: aufrufgraph_zeigen`
- [ ] **L10 Varianten:** Show #if variants anhaken und `!MIT_DIAGNOSE` in Defines (Enter),
      zurück zum Graph ab `main`. Der Pfeil zu `protokolliere` ist **gepunktet** und trägt
      `defined(MIT_DIAGNOSE)` — er gilt mit diesen Definitionen nicht. Defines wieder leeren:
      der Pfeil ist durchgezogen, ohne Beschriftung.
      `aktion: zweige_waehlen name=varianten` `aktion: definitionen_setzen wert=!MIT_DIAGNOSE` `aktion: aufrufgraph_zeigen name=main` `aktion: definitionen_setzen wert=`
- [ ] **L11 Rahmen:** Frame #if regions anhaken. Der Pfeil zu `protokolliere` ist blau, der von
      `protokolliere` zu `schreibe` orange; rechts oben eine Legende mit `defined(MIT_DIAGNOSE)`
      (blau) und `defined(MIT_LOG)` (orange).
      `aktion: zweige_waehlen name=rahmen`
- [ ] **L12 Nach rechts und als Text:** Richtung Left-right: die Rahmen liegen untereinander, jeder
      Titel ganz zu lesen, `schreibe` ganz unten. Format mermaid, Ansicht Text: `flowchart LR`, je
      Datei ein `subgraph`, `-.->|"?"|` zu `regle`; in mermaid.live dasselbe Bild.
      `aktion: zweige_waehlen name=auswerten` `aktion: format_waehlen name=mermaid` `aktion: ansicht_waehlen name=text`

## M. Links auf Datentypen

Am Beispiel `beispiele/typen`: `inc/typen.h` definiert `t_PID`, `t_MODUS`, `Regler`, `Fern`;
`inc/motor.h` bindet es ein und definiert `Motor`; `main.c` bindet `inc/motor.h` ein, hat
`typedef struct Motor Motor_t;`; `sim/sim.c` hat ein eigenes `t_PID` und bindet nichts ein.

- [ ] **M0 Vorbereiten:** Das Projekt öffnen, `inc/typen.h`, Data types, Vorschau.
      `aktion: ordner_oeffnen pfad=beispiele/typen` `aktion: datei_waehlen pfad=inc/typen.h` `aktion: diagramm_waehlen name=types` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview`
- [ ] **M1 Das zweite Feld:** Neben *Data types* ein zweites Feld mit `All types (4)`, `t_PID`,
      `t_MODUS`, `Regler`, `Fern`; gewählt ist `t_PID`. Bei einer Funktion in Diagram ist es weg.
- [ ] **M2 Ein Typ mit Nachbarn:** `Regler` wählen: drei Kästen — `Regler`, darunter `t_PID`
      (Raute, `pid`) und `t_MODUS` (Pfeil, `modus`); `Fern` fehlt. Im Kasten `Regler` sind
      `t_PID`, `t_MODUS` und `Motor` blau unterstrichen; Tooltip auf der Zeile *Click: show in the
      source. Double-click or right-click: go to the type.*
      `aktion: typ_waehlen name=Regler`
- [ ] **M3 In eine andere Datei:** Doppelklick auf `Motor` in `struct Motor *motor`: Datei
      `inc/motor.h`, Data types, zweites Feld `Motor`. **Back**: wieder `Regler` in `inc/typen.h`.
      `aktion: typ_zeigen name=Motor` `aktion: zurueck`
- [ ] **M4 Kopf und Kontextmenü:** Doppelklick auf den Kopf von `t_PID`: zweites Feld `t_PID`.
      Rechtsklick auf eine Zeile mit Link: *Go to type …*, *Show all types of the file*,
      *Back to …*.
- [ ] **M5 Aus dem Ablauf:** `main.c`, Diagram `main()`: im Knoten sind `Motor_t`, `Regler`,
      `regle` und `t_PID` blau unterstrichen, der Knoten blau umrandet. Doppelklick: ein Menü mit
      *Functions* (`regle() - main.c:6`) und *Types* (`Motor - inc/motor.h:7`, `Regler -
      inc/typen.h:12`, `t_PID - inc/typen.h:5`). `t_PID` wählen: `inc/typen.h`, Typ `t_PID` —
      nicht der aus `sim/sim.c`. Back.
      `aktion: datei_waehlen pfad=main.c` `aktion: diagramm_waehlen name=main` `aktion: typ_zeigen name=t_PID`
- [ ] **M6 Der Startknoten:** Diagram `regle()`: im Startknoten ist `Regler` unterstrichen;
      Doppelklick führt direkt zu `Regler` (ein einziges Ziel).
- [ ] **M7 Ohne Einbindung:** `sim/sim.c`, Diagram `simuliere()`: `t_PID` im Startknoten führt zum
      eigenen `t_PID` in `sim/sim.c` (die eigene Datei zuerst).
      `aktion: datei_waehlen pfad=sim/sim.c` `aktion: typ_zeigen name=t_PID`

## N. Klick zeigt die Stelle, Doppelklick führt hin

Ein Klick (ohne zu ziehen) zeigt in jeder Ansicht die Stelle im Quelltext; ein Doppelklick führt nur
zu dem, was genannt ist. Am Beispielprojekt `beispiele/projekt` und an `beispiele/typen`.

- [ ] **N0 Vorbereiten:** Das Projekt öffnen, `main.c`, Diagram `main()`, Vorschau.
      `aktion: ordner_oeffnen pfad=beispiele/projekt` `aktion: datei_waehlen pfad=main.c` `aktion: diagramm_waehlen name=main` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview`
- [ ] **N1 Ein Aufruf im Ablauf:** Ein Klick auf `motor_starte(2)` hinterlegt links Zeile 18
      (`motor_starte(2);`) — es geht **nicht** zur Funktion. Erst der Doppelklick zeigt den Ablauf
      von `motor_starte` in `antrieb/motor.c`; links steht dann diese Datei. Back.
      `aktion: klicken name=motor_starte(2)`
- [ ] **N2 Ziehen ist kein Klick:** Das Bild vergrößern (Strg + Mausrad), mit gedrückter Taste auf
      einem Knoten ziehen: das Bild wandert, der Quelltext springt nicht.
- [ ] **N3 Im Aufrufgraph:** **Call graph from here**. Ein Klick auf `motor_starte` zeigt links
      `antrieb/motor.c`, Zeile 12 hinterlegt; ein Klick auf `main` wieder `main.c`, Zeile 14. Ein
      Klick auf einen grauen (fremden) Knoten: die Statuszeile sagt `… no body in the project`.
      Doppelklick auf `motor_starte`: sein Ablauf, wie bisher. Back.
      `aktion: aufrufgraph_zeigen name=main` `aktion: klicken name=motor_starte` `aktion: klicken name=main`
- [ ] **N4 Bei den Datentypen:** `beispiele/typen` öffnen, `inc/typen.h`, Data types, `Regler`.
      Ein Klick auf den Kopf von `Regler` hinterlegt Zeile 12, auf `struct Motor *motor` Zeile 15,
      auf den Kopf von `t_PID` Zeile 5. Erst der Doppelklick auf `Motor` in `struct Motor *motor`
      führt nach `inc/motor.h`.
      `aktion: ordner_oeffnen pfad=beispiele/typen` `aktion: datei_waehlen pfad=inc/typen.h` `aktion: typ_waehlen name=Regler` `aktion: klicken name=Regler` `aktion: klicken name=Regler::motor` `aktion: klicken name=t_PID`
- [ ] **N5 Methoden:** `beispiele/motor.cpp`, Data types, `antrieb::Motor`: ein Klick auf eine
      Methode (etwa `bool starte()`) hinterlegt ihre Deklaration in der Klasse.
      `aktion: datei_oeffnen pfad=beispiele/motor.cpp` `aktion: typ_waehlen name=antrieb::Motor` `aktion: klicken name=antrieb::Motor::starte`

- [ ] **N6 Umgekehrt:** Ordner `beispiele/projekt`, `main.c`, Diagram `main()`. Rechtsklick im
      Quelltext auf Zeile 20 (`regle(100);`): das Menü hat *Copy*, *Select All* und darunter
      *Show in diagram*. Gewählt: um `regle(100)` ein Ring, die Zeile links hinterlegt. Rechtsklick
      auf Zeile 11 (`return TAKT_MS > 0;`, in `pruefe`), *Show in diagram*: Diagram steht auf
      `pruefe()`, der Knoten umringt; **Back** zurück zu `main()`. Rechtsklick auf Zeile 6 (ein
      Prototyp): die Statuszeile sagt `Line 6 belongs to no function and no type`.
      `aktion: ordner_oeffnen pfad=beispiele/projekt` `aktion: datei_waehlen pfad=main.c` `aktion: diagramm_waehlen name=main` `aktion: im_diagramm_zeigen zeile=20` `aktion: im_diagramm_zeigen zeile=11`

## P. Nassi-Shneiderman

- [ ] **P0 Vorbereiten:** Das Beispiel zeigen, `dauer_ms`, Vorschau, Format svg.
      `aktion: beispiel_zeigen` `aktion: diagramm_waehlen name=dauer_ms` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview`
- [ ] **P1 Das Feld:** Neben Diagram ein Feld `Flowchart`. Auf **Nassi-Shneiderman** gestellt: oben
      grau `dauer_ms(Phase phase)`, darunter ein Kopf `phase` mit zwei Diagonalen, eine Zeile mit
      `ROT`, `GRUEN`, `ROT_GELB, GELB`, `otherwise`, je Spalte ein `return …` mit Pfeil nach links;
      unten `return 0`. Kein Text schneidet eine Linie. Das Feld Top-down ist gesperrt.
      `aktion: darstellung_waehlen name=nassi-shneiderman`
- [ ] **P2 if und Schleife:** `ampel_takt`: zwei Köpfe mit `yes` links unten, `no` rechts unten,
      die `no`-Spalten leer. Ordner `beispiele/projekt`, `main.c`, `main()`: `forever` mit Leiste
      links, darin `regle(100)` und `protokolliere("Takt")` mit doppelten Seiten, blau umrandet;
      `return 0` grau gestrichelt unten.
      `aktion: diagramm_waehlen name=ampel_takt` `aktion: ordner_oeffnen pfad=beispiele/projekt` `aktion: datei_waehlen pfad=main.c` `aktion: diagramm_waehlen name=main`
- [ ] **P3 Klick, Doppelklick, Show in diagram:** Klick auf `motor_starte(2)` hinterlegt Zeile 18.
      Doppelklick: das Struktogramm von `motor_starte` (Feld bleibt auf Nassi-Shneiderman), Back
      zurück. Rechtsklick im Quelltext auf Zeile 20, *Show in diagram*: Ring um `regle(100)`.
      Rechtsklick auf `regle(100)`: *Go to …*, kein *Embed*.
      `aktion: klicken name=motor_starte(2)` `aktion: springen name=motor_starte` `aktion: zurueck` `aktion: im_diagramm_zeigen zeile=20`
- [ ] **P4 Formate:** Ansicht Text mit svg: `<svg …`; mit mermaid oder dot: `Nassi-Shneiderman is
      not available in mermaid - choose svg or drawio`. File > Save diagram as… als `.drawio`, in
      draw.io öffnen: dieselben Blöcke, Diagonalen als Linien.
      `aktion: ansicht_waehlen name=text` `aktion: format_waehlen name=mermaid` `aktion: format_waehlen name=svg`
- [ ] **P5 Varianten und Rahmen:** `beispiele/varianten.c`, `regle`, Show #if variants: ein Kopf
      `#if` mit Spalten `FILTER_STUFE >= 2`, `FILTER_STUFE == 1`, `else (active)`. Frame #if
      regions und Defines `MIT_PRUEFSUMME LOG_STUFE=2`, `melde`: farbige Rahmen mit Titel, Legende
      rechts.
      `aktion: datei_oeffnen pfad=beispiele/varianten.c` `aktion: zweige_waehlen name=varianten` `aktion: diagramm_waehlen name=regle`
- [ ] **P6 Zurück zum Ablauf:** Feld auf Flowchart: wieder der Ablauf, Top-down wählbar.
      `aktion: darstellung_waehlen name=flowchart`

## Q. Wahrheitstabellen

Am Beispiel `beispiele/bedingungen.c`: `darf_einschalten` (Zeile 17 `k->not_aus || !k->freigabe`,
Zeile 19 drei Teile mit `pruefe_sensor(k)`), `ueberwache` (eine Schleife, eine Zuweisung).

- [ ] **Q0 Vorbereiten:** Die Datei öffnen, `darf_einschalten`, Vorschau, Format svg.
      `aktion: datei_oeffnen pfad=beispiele/bedingungen.c` `aktion: diagramm_waehlen name=darf_einschalten` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview`
- [ ] **Q1 Das Symbol:** Rechts neben der Spitze der Raute `k->not_aus || !k->freigabe` ein kleines
      blaues Tabellensymbol, Tooltip *Truth table - click to show it*; ebenso innen rechts oben im
      Kasten `return k->spannung_mv > 11000 …`. `return false` trägt keines.
- [ ] **Q2 Die Tabelle:** Klick auf das Symbol am Kasten `return …`: Diagram steht auf *Truth table:
      darf_einschalten() line 19*. Oben Funktion und Zeile, die Bedingung; Spalten `#`, `A`, `B`,
      `C (call)` mit ihrem Text, `Result`, `MC/DC`. Acht Zeilen; wo `A` falsch ist, stehen bei `B`
      und `C` Striche. Getönt die Zeilen 4, 6, 7, 8; darunter `MC/DC pairs (unique-cause): A 4-8,
      B 6-8, C 7-8`. Tooltip auf dem Kopf von `C` zeigt `pruefe_sensor(k)`.
      `aktion: wahrheitstabelle_zeigen zeile=19`
- [ ] **Q3 Klick und Back:** Ein Klick in die Tabelle hinterlegt Zeile 19 im Quelltext. **Back**:
      wieder der Ablauf von `darf_einschalten`.
      `aktion: zurueck`
- [ ] **Q4 Aus dem Quelltext und dem Kontextmenü:** Rechtsklick im Quelltext auf Zeile 17: *Truth
      table of line 17* (auf Zeile 18 gesperrt). Rechtsklick auf die Raute: *Truth table (line 17)*.
      `aktion: wahrheitstabelle_zeigen zeile=17`
- [ ] **Q5 Formate:** Ansicht Text: mit csv beginnt sie `darf_einschalten(): line 17;k->not_aus ||
      !k->freigabe`, dann `#;A: k->not_aus;B: k->freigabe;Result;MC/DC`; mit mermaid steht dort
      `Truth table is not available in mermaid`, und File > Save diagram as… speichert nichts (die
      Statuszeile sagt es). Zurück im Ablauf mit csv: `Flowchart is not available in csv`.
      `aktion: ansicht_waehlen name=text` `aktion: format_waehlen name=csv` `aktion: format_waehlen name=mermaid`
- [ ] **Q6 Im Struktogramm:** Feld Nassi-Shneiderman: das Symbol rechts oben im Kopf der
      Verzweigung und im Block `return …`; Klick öffnet dieselbe Tabelle.
      `aktion: format_waehlen name=svg` `aktion: darstellung_waehlen name=nassi-shneiderman`

## R. Entscheidungstabelle

- [ ] **R0 Vorbereiten:** Das Beispiel zeigen, `ampel_takt`, Feld auf *Decision table*.
      `aktion: beispiel_zeigen` `aktion: diagramm_waehlen name=ampel_takt` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview` `aktion: darstellung_waehlen name=decision-table`
- [ ] **R1 Die Form:** Oben `ampel_takt(): decision table`. Links `Conditions / actions`, rechts `R1`,
      `R2`, `R3`. Zeile `ampel->stoerung`: Y N N; `ampel->rest_ms > vergangen_ms`: – Y N. Dann eine
      doppelte Linie, darunter sechs Aktionen mit je einem X; zwei Zeilen `return` (zwei Stellen im
      Quelltext). Top-down ist gesperrt.
- [ ] **R2 switch:** `dauer_ms`: eine Zeile `switch (phase)` mit `ROT`, `GRUEN`, `ROT_GELB, GELB`,
      `otherwise`, darunter die vier `return`.
      `aktion: diagramm_waehlen name=dauer_ms`
- [ ] **R3 Klick und Show in diagram:** Ein Klick auf die Zeile `return 1000` hinterlegt sie im
      Quelltext. Rechtsklick im Quelltext auf ihre Zeile, *Show in diagram*: Ring um die Zeile.
- [ ] **R4 Neu gefragt:** `beispiele/bedingungen.c`, `darf_einschalten`: eine Bedingung, zwei Regeln.
      Die Bedingung trägt das Tabellensymbol; Klick öffnet ihre Wahrheitstabelle, Back zurück.
      `aktion: datei_oeffnen pfad=beispiele/bedingungen.c` `aktion: diagramm_waehlen name=darf_einschalten`
- [ ] **R5 Formate:** csv: `darf_einschalten(): decision table`, `Conditions;R1;R2`, … `Actions`;
      mermaid: `Decision table is not available in mermaid`. Feld zurück auf *Flowchart*.
      `aktion: ansicht_waehlen name=text` `aktion: format_waehlen name=csv` `aktion: format_waehlen name=mermaid` `aktion: darstellung_waehlen name=flowchart`

## S. Zuordnungstabellen

Am Beispiel `beispiele/zuordnung.c`: `dauer_ms` (Zeile 18, `switch` mit `return`, ohne `default`),
`setze` (Zeile 33, Zuweisungen an `r` und `g`), die Tabellen `dauer` und `daten` (Zeilen 9 und 10).

- [ ] **S0 Vorbereiten:** Die Datei öffnen, `dauer_ms`, Vorschau, Format svg, Feld auf *Flowchart*.
      `aktion: datei_oeffnen pfad=beispiele/zuordnung.c` `aktion: diagramm_waehlen name=dauer_ms` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview` `aktion: darstellung_waehlen name=flowchart`
- [ ] **S1 Das Symbol:** Rechts neben der Spitze der Raute `phase` das kleine Tabellensymbol, Tooltip
      *Mapping table - click to show it*. Rechtsklick auf die Raute: *Mapping table (line 18)*.
- [ ] **S2 Die Tabelle:** Klick auf das Symbol: Diagram steht auf *Mapping table: dauer_ms() line 18*.
      Oben `dauer_ms(): switch (phase)`, Spalten `phase` und `return`; Zeilen `ROT 5000`,
      `GRUEN 4000`, `ROT_GELB 1000`, `GELB 1000`, `otherwise —`. Die erste Spalte linksbündig.
      `aktion: zuordnung_zeigen zeile=18`
- [ ] **S3 Klick und Back:** Klick auf `GELB`: im Quelltext ist Zeile 24 hinterlegt. **Back**: wieder
      der Ablauf von `dauer_ms`.
      `aktion: klicken name=GELB` `aktion: zurueck`
- [ ] **S4 Aus dem Quelltext:** Rechtsklick im Quelltext auf Zeile 33, *Mapping table of line 33*:
      `setze(): switch (phase)`, Spalten `phase`, `r`, `g`; `ROT 1 0`, `GRUEN 0 1`, `default 0 0`.
      `aktion: zuordnung_zeigen zeile=33`
- [ ] **S5 Lookup tables:** In Diagram *Lookup tables (2)*: `static const unsigned short dauer[4]` mit
      `index`/`value`, `0…3`; darunter `static const PhasenDaten daten[]` mit `index`, `dauer_ms`,
      `blinkt` und den Zeilen `ROT`, `GRUEN`, `GRUEN + 1` (bei `GRUEN` steht `4000` unter
      `dauer_ms`, obwohl `.blinkt` in der Quelle zuerst kommt). Klick auf `GRUEN`: Zeile 12.
      `aktion: diagramm_waehlen name=lookup` `aktion: klicken name=GRUEN`
- [ ] **S6 Formate:** csv: `daten[]`-Block mit `index;dauer_ms;blinkt`; draw.io zeigt dieselben
      Tabellen; mermaid: `Mapping table is not available in mermaid`.
      `aktion: ansicht_waehlen name=text` `aktion: format_waehlen name=csv` `aktion: format_waehlen name=drawio` `aktion: format_waehlen name=mermaid`
- [ ] **S7 Typ im Kopf:** Einen Ordner öffnen, in dem die Struktur einer Tabelle in einem
      eingebundenen Kopf steht (etwa LLC, `Serial_Commands.c`): *Lookup tables* zeigt die Glieder
      des Kopfs als Spalten (`Template`, `Groupe`, …), nicht `1`, `2`, ….

## T. #if-Konfiguration

Am Beispiel `beispiele/konfiguration.c`: Kette ab Zeile 4 (`#if defined(BOARD_A) || defined(BOARD_B)`,
`#elif VERSION >= 2` in Zeile 6, `#else` in Zeile 8), `#ifdef DEBUG` (Zeile 12) mit `#if LOG_STUFE > 1`
darin (Zeile 13), `#if TAKT_MHZ > 50` (Zeile 18; `TAKT_MHZ` definiert die Datei in Zeile 2).

- [ ] **T0 Vorbereiten:** Die Datei öffnen, Defines leer, Vorschau, Format svg; in Diagram
      *#if configuration (4)*.
      `aktion: datei_oeffnen pfad=beispiele/konfiguration.c` `aktion: definitionen_setzen wert=` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview` `aktion: diagramm_waehlen name=ifconfig`
- [ ] **T1 Die Form:** Vier Tabellen untereinander. Die erste: Spalten `branch`, `BOARD_A`, `BOARD_B`,
      `VERSION` (je mit `undef` darunter), `active`; Zeilen `#if defined(BOARD_A) || …` (`def – –`),
      `or` (`– def –`), `#elif VERSION >= 2` (`!def !def >= 2`), `#else` (`!def !def < 2`) — diese
      getönt, mit ✓.
- [ ] **T2 Innen:** Die dritte Tabelle ist eingerückt, Titel `#if LOG_STUFE > 1 (line 13), in #if
      defined(DEBUG) (line 12)`; beide Zeilen fordern `DEBUG` `def`. Keine trägt ✓ (DEBUG gilt nicht).
- [ ] **T3 Selbst definiert:** Die vierte: Spalte `TAKT_MHZ` mit `= 100` und `(line 2)` im Kopf, ihr
      Text grau (hell wie dunkel deutlich anders als die getönte Zeile); `#if TAKT_MHZ > 50` trägt ✓.
      Rechtsklick auf `#else`: *Make this branch active* gesperrt, Tooltip nennt den Grund.
- [ ] **T4 Klick:** Klick auf `#elif VERSION >= 2`: im Quelltext ist Zeile 6 hinterlegt; Klick auf
      `(no #else)` der zweiten Tabelle: Zeile 16 (das `#endif`).
      `aktion: klicken name=#elif VERSION >= 2`
- [ ] **T5 Make this branch active:** Rechtsklick auf `#elif VERSION >= 2`, *Make this branch active*:
      im Feld Defines steht `VERSION=2`, die Datei wird neu gelesen, die Tabelle bleibt; jetzt trägt
      `#elif VERSION >= 2` das ✓, im Kopf `VERSION` `= 2`. Das Feld Configuration zeigt denselben
      Zweig. Rechtsklick auf dieselbe Zeile: der Eintrag ist gesperrt (*This branch holds already*).
      `aktion: zweig_aktivieren zeile=6`
- [ ] **T6 Aus dem Quelltext:** Rechtsklick im Quelltext auf Zeile 13, *#if configuration of line
      13*: Ring um die erste Zeile der eingerückten Tabelle. Auf Zeile 3 ist der Eintrag gesperrt.
      `aktion: konfiguration_zeigen zeile=13`
- [ ] **T7 Formate:** csv: je Kette Titel, `branch;BOARD_A;BOARD_B;VERSION;active`, … `yes` in der
      Zeile, die gilt; draw.io dieselben Tabellen, die Spalte `TAKT_MHZ` mit grauer Schrift; mermaid:
      `#if configuration is not available in mermaid`. Danach Defines wieder leer.
      `aktion: ansicht_waehlen name=text` `aktion: format_waehlen name=csv` `aktion: format_waehlen name=drawio` `aktion: format_waehlen name=mermaid` `aktion: definitionen_setzen wert=`

## U. Wo schaltet ein Name

Am Beispielprojekt `beispiele/projekt`: `MOTOREN` legt der Abschnitt `antrieb` der Projektdatei fest
(`MOTOREN=2`), gefragt in `antrieb/motor.c` Zeile 21 (`#if MOTOREN > 1`); `MIT_DIAGNOSE` definiert
`inc/config.h` Zeile 6, gefragt in `main.c` Zeile 21 (`#ifdef MIT_DIAGNOSE`); `MIT_LOG` die Projektdatei.

- [ ] **U0 Vorbereiten:** Den Ordner öffnen, `main.c` wählen, Defines leer, Format svg, Vorschau; in
      Diagram steht *#if names (3)* hinter *#if configuration*.
      `aktion: ordner_oeffnen pfad=beispiele/projekt` `aktion: datei_waehlen pfad=main.c` `aktion: definitionen_setzen wert=` `aktion: format_waehlen name=svg` `aktion: ansicht_waehlen name=preview` `aktion: diagramm_waehlen name=ifnames`
- [ ] **U1 Die Namen:** Titel `#if names: 3 in 3 files`, Spalten `name`, `files`, `chains`, `state`,
      `set by`; Zeilen `MIT_DIAGNOSE 1 1 = 1 header`, `MIT_LOG 1 1 = 1 project`, `MOTOREN 1 1 = 2
      section`. Keine Zeile getönt (keiner steht in zwei Dateien verschieden).
- [ ] **U2 Doppelklick:** Doppelklick auf `MOTOREN`: Titel `Where is MOTOREN switched? 2 branches in 1
      file`, darunter `set by: section antrieb | code2diagram.json | MOTOREN=2`; die Tabelle mit
      `antrieb/motor.c`, `21`, `#if MOTOREN > 1`, `> 1`, `= 2 (section antrieb)`, `21–26 (4)`, ✓ (getönt),
      darunter `(no #else)` mit `<= 1` und `–` bei *lines*; zuunterst der Hinweis zu `#define` in Zweigen,
      die nicht gelten. In Diagram steht *Where is MOTOREN switched?*.
      `aktion: schaltstellen_zeigen name=MOTOREN`
- [ ] **U3 Klick in eine andere Datei:** Klick auf die Zeile `21`: der Quelltext zeigt `antrieb/motor.c`,
      Zeile 21 hinterlegt; gewählt bleibt `main.c` (Diagram unverändert).
      `aktion: klicken name=antrieb/motor.c:21`
- [ ] **U4 Doppelklick auf einen Zweig:** Doppelklick auf die Zeile `21`: *#if configuration* von
      `antrieb/motor.c`, Ring um `#if MOTOREN > 1`. Back: wieder *Where is MOTOREN switched?*; Back:
      *#if names*.
- [ ] **U5 Aus der #if-Konfiguration:** `antrieb/motor.c` wählen, *#if configuration*; Rechtsklick auf
      eine Zeile: *Where is MOTOREN switched?* (je Spalte der Kette ein Eintrag) führt zur Tabelle.
      `aktion: datei_waehlen pfad=antrieb/motor.c` `aktion: diagramm_waehlen name=ifconfig`
- [ ] **U6 Aus dem Feld Configuration:** `main.c` wählen; Rechtsklick auf den Haken `MIT_DIAGNOSE
      (header)`: *Where is MIT_DIAGNOSE switched?*; *set by* `#define | inc/config.h:6 |` (ohne Wert);
      Klick auf diese Zeile zeigt `inc/config.h` Zeile 6.
      `aktion: datei_waehlen pfad=main.c` `aktion: schaltstellen_zeigen name=MIT_DIAGNOSE` `aktion: klicken name=inc/config.h:6`
- [ ] **U7 Aus dem Quelltext:** In `main.c` Rechtsklick auf `MIT_DIAGNOSE` in Zeile 21: *Where is
      MIT_DIAGNOSE switched?*. Rechtsklick auf `ifdef` derselben Zeile und auf eine Zeile ohne `#`: der
      Eintrag heißt *Where is a name of an #if switched?* und ist gesperrt.
- [ ] **U8 Von außen:** Defines `MOTOREN=1` (Enter), dann *Where is MOTOREN switched?*: *set by* bekommt
      `outside | | MOTOREN=1`, der Stand heißt `= 1 (outside)`, das ✓ steht bei `(no #else)`. Defines
      wieder leer.
      `aktion: definitionen_setzen wert=MOTOREN=1` `aktion: schaltstellen_zeigen name=MOTOREN` `aktion: definitionen_setzen wert=`
- [ ] **U9 Formate:** csv: `set by;file;line;value`, `section antrieb;code2diagram.json;;MOTOREN=2`, eine
      Leerzeile, dann `file;line;branch;via;MOTOREN;state in the file;from;to;lines;active` und
      `antrieb/motor.c;21;#if MOTOREN > 1;;> 1;= 2 (section antrieb);21;26;4;yes`; draw.io dieselbe
      Tabelle; mermaid und dot: `#if switch points is not available in mermaid` (dot entsprechend). Bei
      *#if names* in mermaid: `#if names is not available in mermaid`.
      `aktion: ansicht_waehlen name=text` `aktion: format_waehlen name=csv` `aktion: format_waehlen name=drawio` `aktion: format_waehlen name=mermaid`
- [ ] **U11 Innere Kette:** `beispiele/konfiguration.c` öffnen, *Where is DEBUG switched?*: Zeilen 12
      und 16 (`#ifdef DEBUG`, `(no #else)`) ohne *via*; die Zeilen 13 und 15 (`#if LOG_STUFE > 1`,
      `(no #else)`) tragen *via* `#if defined(DEBUG), line 12`, ihr `def` grau. Bei *Where is TAKT_MHZ
      switched?* gibt es keine Spalte *via*. In csv steht *via* immer (leer, wo die Kette selbst fragt).
      `aktion: datei_oeffnen pfad=beispiele/konfiguration.c` `aktion: schaltstellen_zeigen name=DEBUG` `aktion: schaltstellen_zeigen name=TAKT_MHZ`
- [ ] **U10 Dunkel:** Thema Dark: die getönte Zeile (✓) bleibt von den übrigen zu unterscheiden, die
      Schrift lesbar. Thema wieder System.
      `aktion: thema_setzen name=dark` `aktion: thema_setzen name=system`

## O. Einstellungen und Thema

- [ ] **O0 Vorbereiten:** Das Beispiel zeigen, `ampel_takt` in der Vorschau, Thema System.
      `aktion: beispiel_zeigen` `aktion: diagramm_waehlen name=ampel_takt` `aktion: ansicht_waehlen name=preview` `aktion: thema_setzen name=system`
- [ ] **O1 Der Dialog:** File > Settings… (oder Strg + ,): links eine Seite *Appearance*, rechts
      *Theme* mit `System`, `Light`, `Dark`; gewählt ist `System`. Tooltip auf dem Feld nennt, dass
      System dem Betriebssystem folgt.
      `aktion: einstellungen_zeigen`
- [ ] **O2 Apply:** `Dark` wählen, **Apply**: Fenster, Menüs, Quelltext und Vorschau werden dunkel,
      der Dialog bleibt offen. Die Vorschau zeigt dasselbe Bild, an derselben Stelle, helle Striche
      auf dunklem Grund; die Pfeile von **Back** und **Called from** sind hell. **Cancel**: es
      bleibt dunkel.
      `aktion: thema_setzen name=dark`
- [ ] **O3 Lesbar im Dunkeln:** Ein Klick auf einen Knoten: die hinterlegte Zeile im Quelltext ist
      lesbar (heller Text auf getöntem Grund). Data types: Kästen mit dunklem Grund, getöntem Kopf.
- [ ] **O4 Bleibt über Neustarts:** Code2Diagram schließen und neu starten: dunkel. Dann `Light`,
      **OK**: hell, der Dialog ist zu.
      `aktion: thema_setzen name=light`
- [ ] **O4a Fusion:** Hell haben Auswahl-, Zahlen- und Eingabefelder einen sichtbaren Rand;
      dunkel sind leere Kästchen leer (nicht blau gefüllt), angehakte tragen ein Häkchen.
- [ ] **O5 System folgt dem Betriebssystem:** `System`, OK. In Windows unter *Einstellungen >
      Personalisierung > Farben* den Modus wechseln: Code2Diagram folgt, ohne Neustart, die
      Vorschau mit.
      `aktion: thema_setzen name=system`

## V. Python

- [ ] **V0 Vorbereiten:** `beispiele/lager.py` öffnen, Vorschau, Thema System.
      `aktion: datei_oeffnen pfad=beispiele/lager.py` `aktion: ansicht_waehlen name=preview` `aktion: thema_setzen name=system`
- [ ] **V1 Gelesen:** Die Statuszeile nennt `lager.py, read as Python` ohne Meldungen. Diagram bietet
      `Lager::__init__()`, `Lager::lege_ab()`, `Lager::zustand()`, `Lager::suche()`, `lade()`,
      `<module>()`, `Data types (3)`, `Call graph of this file` — **kein** `#if names`. Das Feld
      Configuration ist leer. File > Open source file… bietet `Python files (*.py *.pyi *.pyw)`.
- [ ] **V2 try mit finally:** `lade()`: von `try` gehen drei Kanten aus — ohne Beschriftung in den
      Block mit `with open(…) as datei`, `except FileNotFoundError`, `except json.JSONDecodeError as fehler`.
      Alle Wege laufen durch `print("geladen:", pfad)` (finally), auch `return lager` aus dem except und
      `raise`; von dort geht es weiter nach `return lager` und mit `return` bzw. `throw` ins Ende.
      `aktion: diagramm_waehlen name=lade`
- [ ] **V3 Schleife mit else:** `Lager::suche()`: `done` führt zu `return None`, das `break` (Kante `yes`
      aus `teil in name`) an ihm vorbei zu `return self.posten[name]`.
      `aktion: diagramm_waehlen name=Lager::suche`
- [ ] **V4 match:** `Lager::zustand()`: ein Kopf `gesamt` mit den Kanten `0`, `n if n < self.grenze`,
      `default`; kein Fall läuft in den nächsten. Das Tabellensymbol am Kopf öffnet die Zuordnung
      `Lager::zustand(): match gesamt` mit drei Zeilen.
      `aktion: diagramm_waehlen name=Lager::zustand`
- [ ] **V5 Struktogramm:** `lade()` als Nassi-Shneiderman: `try` mit den Abschnitten `with open(…) as datei`
      (eingerückt `daten = json.load(datei)`), `except FileNotFoundError`, `except json.JSONDecodeError as fehler`,
      `else` (mit `for name, menge in daten.items()`), `finally`; die Köpfe so, wie sie in der Quelle
      stehen, nicht `catch (…)`/`for (…)`.
      `aktion: diagramm_waehlen name=lade` `aktion: darstellung_waehlen name=nassi-shneiderman`
- [ ] **V6 Wahrheitstabelle:** `Lager::lege_ab()` als Flowchart, Tabellensymbol an `menge <= 0 or not name`:
      Atome `A: menge <= 0`, `B: name`, Ergebnis wahr in drei von vier Zeilen.
      `aktion: darstellung_waehlen name=flowchart` `aktion: diagramm_waehlen name=Lager::lege_ab` `aktion: wahrheitstabelle_zeigen zeile=27`
- [ ] **V7 Datentypen:** `Data types`, Typ `Lager`: Felder `grenze`, `posten: dict[str, Posten]` (aus
      `self.…` in `__init__`), Methoden mit Parametern wie in der Quelle (`lege_ab(self, name: str, menge: int)`);
      eine Kante `posten` zu `Posten` (`name: str`, `menge: int = 0`). `Zustand` ist eine Aufzählung
      (`LEER = 0` …). Doppelklick auf `Posten` im Feld zeigt diesen Typ.
      `aktion: diagramm_waehlen name=types:Lager`
- [ ] **V8 Klick:** Ein Klick auf `raise` (in `lade()`) markiert im Quelltext Zeile 66.
      `aktion: diagramm_waehlen name=lade`

## W. JavaScript und TypeScript

- [ ] **W0 Vorbereiten:** `beispiele/knopf.js` öffnen, Vorschau.
      `aktion: datei_oeffnen pfad=beispiele/knopf.js` `aktion: ansicht_waehlen name=preview`
- [ ] **W1 Gelesen:** Statuszeile `knopf.js, read as JavaScript: 6 function(s), 0 data type(s)` ohne Meldungen.
      Diagram bietet `erhoehe()`, `zuruecksetzen()`, `verbinde()`, `verbinde::<anonymous:14>()`,
      `<anonymous:28>()`, `<module>()`, `Call graph of this file`. File > Open source file… bietet
      `JavaScript files (*.js *.mjs *.cjs)` und `TypeScript files (*.ts *.mts *.cts)`.
- [ ] **W2 Callback:** `verbinde()`: ein Knoten mit `knopf.addEventListener("click", (ereignis) => {…})`,
      `const muster = /^\d+$/g` und `return … ? "zahl" : "text"` (die Zeilen der Quelle ohne `;` richtig
      getrennt, das `?` der nächsten Zeile gehört zum `return`). Doppelklick auf den Knoten zeigt
      `verbinde::<anonymous:14>()` (if/else mit `zaehler.zuruecksetzen()`/`zaehler.erhoehe()`); **Back**
      kehrt zurück und hebt den Knoten hervor. Rechtsklick auf den Knoten: *Go to verbinde::<anonymous:14>()*.
      `aktion: diagramm_waehlen name=verbinde`
- [ ] **W3 TypeScript:** `beispiele/lager.ts` öffnen, `lade()`: `try` mit Kante `catch (fehler)`, alle Wege
      durch `console.log("geladen:", pfad)` (finally), von dort `throw` ins Ende.
      `aktion: datei_oeffnen pfad=beispiele/lager.ts` `aktion: diagramm_waehlen name=lade`
- [ ] **W4 Datentypen:** Typ `Lager`: Felder `- #posten = new Map<string, Posten>()`, `- grenze: number`
      (aus `constructor(private readonly grenze: number)`), Methoden `constructor(grenze: number)`,
      `lege(name: string, menge: number)`, `get zustand()`, `suche(filter: Filter)`; darüber `«interface»
      Ablage`, die Kante gestrichelt mit offener Spitze. `Posten` ist ein «interface», `Zustand` eine
      Aufzählung, `Bericht` eine Struktur.
      `aktion: diagramm_waehlen name=types:Lager`
- [ ] **W5 Zuordnung:** `Lager::get zustand()`: Tabellensymbol am Kopf `true` (switch), die Zuordnung mit
      drei Zeilen `gesamt === 0`, `gesamt < this.grenze`, `default`.
      `aktion: diagramm_waehlen name=Lager::get zustand`

## X. C#

- [ ] **X0 Vorbereiten:** `beispiele/lager.cs` öffnen, Vorschau.
      `aktion: datei_oeffnen pfad=beispiele/lager.cs` `aktion: ansicht_waehlen name=preview`
- [ ] **X1 Gelesen:** Statuszeile `lager.cs, read as C#` ohne Meldungen. Diagram bietet unter anderem
      `…Lager::Anzahl.get()`, `…Lager::Lege()`, `…Lager::Zustand.get()`, `…Lager::Lade()`,
      `…Lager::Lade::<lambda:99>()`, `Data types (4)`, `#if configuration (1)`. File > Open source file…
      bietet `C# files (*.cs)`.
- [ ] **X2 #if:** Configuration zeigt `PROTOKOLL` angehakt und gesperrt (die Datei definiert ihn in Zeile 2).
      `Lade()`: im `catch (FormatException fehler) when (…)` steht `Console.Error.WriteLine(…)` vor `throw`.
      Defines `!PROTOKOLL` hilft nicht (die Datei definiert ihn); `#define PROTOKOLL` auskommentieren und
      neu laden: die Zeile fehlt.
      `aktion: diagramm_waehlen name=Beispiel::Lagerhaltung::Lager::Lade`
- [ ] **X3 Ablauf:** `Lade()`: `try` mit zwei `catch`-Kanten (die zweite mit `when`), `using (…)` als Knoten
      vor der Schleife, `finally` (`Console.WriteLine($"geladen: {pfad}")`) auf allen Wegen, danach `return`
      und `throw` ins Ende. Doppelklick auf `lager.Geaendert += (sender, e) => {…}` zeigt `<lambda:99>`.
- [ ] **X4 switch-Ausdruck:** `Zustand.get()` als Nassi-Shneiderman: `foreach (var menge in _posten.Values)`,
      darunter eine Fallunterscheidung `gesamt` mit den Spalten `0`, `< 100 when gesamt < Grenze`, `default`,
      je ein `return`. Das Tabellensymbol öffnet die Zuordnung `… gesamt switch` mit drei Zeilen.
      `aktion: diagramm_waehlen name=Beispiel::Lagerhaltung::Lager::Zustand.get` `aktion: darstellung_waehlen name=nassi-shneiderman`
- [ ] **X5 Datentypen:** Typ `Lager`: `- Dictionary<string, int> _posten = new()`, `+ int Grenze { get; init; } = 100`,
      `+ int Anzahl { get; }`, `+ event EventHandler? Geaendert`, `+ Zustand Zustand { get; }`; Methoden mit
      Rückgabetyp; darüber `«interface» IAblage` mit gestrichelter Kante, darunter `Zustand` («enumeration»).
      `aktion: darstellung_waehlen name=flowchart` `aktion: diagramm_waehlen name=types:Beispiel::Lagerhaltung::Lager`
- [ ] **X6 Unity-Projekt:** File > Open folder… `../Fundgrube`: im Baum fehlen `Library`, `Logs`, `UserSettings`;
      Project-Meldungen nennen sie `folder skipped (made by Unity)`.

## Y. Java

- [ ] **Y0 Vorbereiten:** `beispiele/Lager.java` öffnen, Vorschau.
      `aktion: datei_oeffnen pfad=beispiele/Lager.java` `aktion: ansicht_waehlen name=preview`
- [ ] **Y1 Gelesen:** Statuszeile `Lager.java, read as Java` ohne Meldungen. Diagram bietet unter anderem
      `Zustand::VOLL.nimmtAuf()`, `Lager::<static:44>()`, `Lager::suche()`, `Lager::lade()`,
      `Lager::lade::<anonymous:108>.run()`, `Lager::lade::<lambda:114>()`, `Data types (4)` — kein
      `#if configuration`. File > Open source file… bietet `Java files (*.java)`.
- [ ] **Y2 Marken:** `suche()`: `fach == null` → `yes` führt an den Kopf der **äusseren** Schleife
      (`String[] regal : regale`), `fund = fach` hinter **beide** Schleifen zu `return fund`. Als Nassi-Shneiderman:
      Kopf `aussen: for (String[] regal : regale)`, Aussprünge `continue aussen` und `break aussen`.
      `aktion: diagramm_waehlen name=Lager::suche` `aktion: darstellung_waehlen name=nassi-shneiderman`
- [ ] **Y3 switch-Ausdruck:** `zustand()` als Ablauf: Raute `gesamt` mit Kanten `0`, `1, 2, 3`, `default`; unter
      `1, 2, 3` die Raute `gesamt < grenze` mit `return Zustand.TEILWEISE` / `return Zustand.VOLL` (aus `yield`).
      `aktion: darstellung_waehlen name=flowchart` `aktion: diagramm_waehlen name=Lager::zustand`
- [ ] **Y4 try mit Ressourcen, anonyme Klasse:** `lade()`: Raute `try (BufferedReader leser = …`, eine Kante
      `catch (NumberFormatException fehler)`, `finally` auf allen Wegen, danach `throw` ins Ende. Doppelklick auf
      den Knoten mit `new Runnable() {…}` bietet `<anonymous:108>.run` und `<lambda:114>`; die Wahl zeigt deren Ablauf,
      Back zurück.
      `aktion: diagramm_waehlen name=Lager::lade`
- [ ] **Y5 Datentypen:** Typ `Lager`: vier Felder mit Initialisierer, Methoden mit Rückgabetyp; darüber `«interface»
      Ablage` mit gestrichelter Kante (`lege` kursiv). Typ `Zustand` («enumeration»): `LEER ("leer")` …, `- String text`,
      Konstruktor und `nimmtAuf()`. Typ `Posten`: zwei Felder aus den Komponenten.
      `aktion: diagramm_waehlen name=types:Lager`
- [ ] **Y6 Maven:** ein Ordner mit `pom.xml` und `target/` als Projekt öffnen: `target` fehlt im Baum, Project-Meldung
      `folder skipped (made by Maven)`; ein `target/` ohne `pom.xml` daneben wird gelesen.

## Z. Kotlin

- [ ] **Z0 Vorbereiten:** File > Open folder… `beispiele/kotlin/wetter`, Vorschau.
      `aktion: ordner_oeffnen pfad=beispiele/kotlin/wetter` `aktion: ansicht_waehlen name=preview`
- [ ] **Z1 Gelesen:** im Baum fünf `.kt`, keine `.gradle.kts`, keine Meldungen. In `MainActivity.kt` bietet Diagram
      `MainActivity::onCreate()`, `MainActivity::onCreate::<anonymous:58>.afterTextChanged()`,
      `MainActivity::onCreate::<lambda:48>()`, `Data types (1)` — kein `#if configuration`. File > Open source file…
      bietet `Kotlin files (*.kt)`.
- [ ] **Z2 Lambda und anonymes Objekt:** `onCreate()`: Doppelklick auf den Knoten mit `suchen.setOnClickListener {…}`
      bietet `<lambda:48>`, `<anonymous:58>.beforeTextChanged` … ; `<lambda:48>` zeigt `ortFeld.error = …` und
      `return@setOnClickListener`, Back zurück.
- [ ] **Z3 when als Wert:** `wechsleEinheit()`: Raute `einheit` mit `Einheit.CELSIUS`, `Einheit.FAHRENHEIT`,
      `Einheit.KELVIN`, je `einheit = Einheit.…`; das Tabellensymbol öffnet die Zuordnung mit drei Zeilen und
      `otherwise`. Diagram bietet dazu den Zustandsautomaten `einheit`.
- [ ] **Z4 Marken und Automat:** `netz/JsonLeser.kt`, `JsonLeser::zeichenkette()`: `++ziffern < 4` → `yes` an den Kopf
      `p < text.length`; als Nassi-Shneiderman Kopf `zeichen: while (p < text.length)`, Aussprung `continue zeichen`.
      `aktion: diagramm_waehlen name=JsonLeser::zeichenkette` `aktion: darstellung_waehlen name=nassi-shneiderman`
- [ ] **Z5 Datentypen:** `daten/Vorhersage.kt`, Typ `Wetterlage`: `gefaehrlich: Boolean { get; }`, `STARKREGEN_MM = 25.0`,
      `aus(…)`; darunter `Klar`, `Regen`, `Gewitter`, `Unbekannt` mit Vererbung (durchgezogen, `: Wetterlage()`).
      `aktion: darstellung_waehlen name=flowchart` `aktion: diagramm_waehlen name=types:Wetterlage`
- [ ] **Z6 Compose:** `beispiele/kotlin/notizen`, `ui/NotizenScreen.kt`: viele `<lambda:…>` unter `NotizenScreen`,
      einzeilige wie `topBar = { TopAppBar(title = { Text(…) }) }` bleiben Text in ihrer Anweisung.
