# Sichttest: Grundgerüst

**Anwendung:** Code2Diagram

Exe: `out/build/windows-ninja-release-clang/exec/Code2Diagram/bin/Release/Code2Diagram.exe`

Geprüft wird die Version 0.1.0: das Fenster, das Lesen von C und C++, die zwei Diagrammarten
(Ablauf einer Funktion, Datentypen), die Vorschau mit Sprung und Einbettung und die drei Formate
(Mermaid, DOT, draw.io). Die Abschnitte A bis D prüfen den Text (Ansicht **Text**; zum Ansehen
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
      Feld **Defines** (leer) und der Haken **All #if branches** (aus). Die Statuszeile endet
      mit `not defined in #if: BOARD_A, BOARD_B, LOG_STUFE, MIT_PRUEFSUMME`.
- [ ] **H2 Definitionen wählen die Variante:** In Defines `LOG_STUFE=2 MIT_PRUEFSUMME`
      eintragen, Enter. Das Diagramm bleibt bei `melde` und zeigt jetzt zuerst
      `protokolliere("melde", laenge)` und in der Schleife die Raute `!pruefe(daten[i])`, deren
      `yes` die Schleife verlässt. Die Statuszeile nennt nur noch `BOARD_A, BOARD_B`.
      `aktion: definitionen_setzen wert="LOG_STUFE=2 MIT_PRUEFSUMME"`
- [ ] **H3 Eine andere Funktion je Board:** `sende()` wählen: ihr Ablauf enthält nur
      `(void)byte`. Mit `BOARD_B` in Defines (zusätzlich) steht dort `spi_schreibe(byte)`.
- [ ] **H4 Alle Zweige:** Den Haken **All #if branches** setzen: das Feld Defines wird grau,
      die Auswahl Diagram nennt `sende()` nur einmal (die erste von dreien), und `melde` zeigt
      `protokolliere` und `pruefe` unabhängig von den Definitionen. Haken wieder weg: die
      Variante von vorher.
      `aktion: zweige_waehlen name=alle`
