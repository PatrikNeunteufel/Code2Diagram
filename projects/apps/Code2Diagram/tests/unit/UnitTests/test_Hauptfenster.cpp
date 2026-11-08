/**
 * @file test_Hauptfenster.cpp
 * @brief Das Fenster: lesen, waehlen, Text des Diagramms — ohne es zu zeigen
 */
#include <doctest.h>

#include "ui/Hauptfenster.hpp"
#include "ui/Konfiguration.hpp"
#include "ui/Vorschau.hpp"

#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStatusBar>
#include <QMouseEvent>
#include <QTemporaryDir>

using c2d::ui::Hauptfenster;

TEST_SUITE("Hauptfenster")
{
    TEST_CASE("Das Beispiel: zwei Funktionen und die Datentypen stehen zur Wahl")
    {
        Hauptfenster fenster;
        CHECK(fenster.diagramme().isEmpty());
        CHECK(fenster.diagrammText().isEmpty());

        fenster.zeigeBeispiel();
        CHECK(fenster.diagramme() == QStringList{"dauer_ms", "ampel_takt", "types", "calls"});
        CHECK(fenster.meldungen() == 0);
        CHECK(fenster.quelltext().contains("ampel_takt"));
        // die erste Funktion ist gewaehlt, das erste Format ist SVG
        CHECK(fenster.diagrammText().startsWith("<svg xmlns="));
        CHECK(fenster.diagrammText().contains("<title>dauer_ms</title>"));
        CHECK(fenster.diagrammText().contains(">ROT_GELB, GELB</tspan>"));
        CHECK(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().startsWith("%% dauer_ms\nflowchart TD\n"));
        CHECK(fenster.diagrammText().contains("-->|\"ROT_GELB, GELB\"|"));
    }

    TEST_CASE("Diagramm und Format waehlen")
    {
        Hauptfenster fenster;
        fenster.zeigeBeispiel();
        CHECK(fenster.waehleFormat("mermaid"));

        CHECK(fenster.waehleDiagramm("ampel_takt"));
        CHECK(fenster.diagrammText().contains("ampel_takt(Ampel *ampel, uint16_t vergangen_ms)"));

        CHECK(fenster.waehleDiagramm(Hauptfenster::kDatentypen));
        CHECK(fenster.diagrammText().contains("classDiagram"));
        CHECK(fenster.diagrammText().contains("Ampel --> Phase : phase"));

        CHECK(fenster.waehleFormat("dot"));
        // gewaehlt ist der erste Typ, mit seinen Nachbarn; `*` zeigt alle
        CHECK(fenster.typ() == "Phase");
        CHECK(fenster.diagrammText().startsWith("digraph \"Phase\" {"));
        CHECK(fenster.waehleTyp("*"));
        CHECK(fenster.diagrammText().startsWith("digraph \"ampel.c\" {"));

        CHECK_FALSE(fenster.waehleDiagramm("gibt_es_nicht"));
        CHECK_FALSE(fenster.waehleFormat("png"));
        CHECK(fenster.diagrammText().startsWith("digraph"));  // eine falsche Wahl aendert nichts
    }

    TEST_CASE("Vorschau: je Knoten eine Form; C++ und draw.io gehen denselben Weg")
    {
        Hauptfenster fenster;
        CHECK(fenster.vorschauFormen() == 0);
        fenster.zeigeBeispiel();
        CHECK(fenster.vorschauFormen() == 7);  // dauer_ms: Start, switch, vier return, Ende
        CHECK(fenster.waehleDiagramm(Hauptfenster::kDatentypen));
        CHECK(fenster.vorschauFormen() == 2);  // Phase, Ampel

        fenster.zeige("namespace n { struct A { int x; int f() { return g(); } int g() { return x; } }; }", "a.hpp");
        CHECK(fenster.diagramme() == QStringList{"n::A::f", "n::A::g", "types", "calls"});
        CHECK(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().startsWith("%% f\nflowchart TD\n    n0([\"n::A::f()\"])"));
        CHECK(fenster.waehleFormat("drawio"));
        CHECK(fenster.diagrammText().startsWith("<mxfile host=\"Code2Diagram\">"));
        CHECK(fenster.waehleAnsicht("text"));
        CHECK(fenster.waehleAnsicht("preview"));
        CHECK_FALSE(fenster.waehleAnsicht("bild"));
    }

    TEST_CASE("Zum Ablauf der gerufenen Funktion und zurueck; einbetten und wieder herausnehmen")
    {
        Hauptfenster fenster;
        fenster.zeigeBeispiel();
        CHECK(fenster.waehleFormat("mermaid"));
        CHECK(fenster.waehleDiagramm("ampel_takt"));
        CHECK(fenster.diagramm() == "ampel_takt");
        const int ohne = fenster.vorschauFormen();
        CHECK(fenster.einbettungen() == 0);

        // ampel_takt ruft dauer_ms: dessen Ablauf haengt sich umrahmt ein
        CHECK(fenster.betteEin("dauer_ms"));
        CHECK(fenster.einbettungen() == 1);
        CHECK(fenster.vorschauFormen() == ohne + 5);  // switch und vier return, ohne Start und Ende
        CHECK(fenster.diagrammText().contains("subgraph g0[\"dauer_ms(Phase phase)\"]"));
        CHECK_FALSE(fenster.betteEin("dauer_ms"));    // der eine Aufruf traegt schon eine
        CHECK_FALSE(fenster.betteEin("gibt_es_nicht"));

        CHECK(fenster.loeseEinbettungen());
        CHECK(fenster.einbettungen() == 0);
        CHECK(fenster.vorschauFormen() == ohne);
        CHECK_FALSE(fenster.diagrammText().contains("subgraph"));
        CHECK_FALSE(fenster.loeseEinbettungen());

        // der Verlauf: von ampel_takt zu dauer_ms und zurueck
        CHECK(fenster.waehleDiagramm("dauer_ms"));
        CHECK(fenster.diagramm() == "dauer_ms");
        CHECK(fenster.zurueck());
        CHECK(fenster.diagramm() == "ampel_takt");
        CHECK(fenster.zurueck());  // davor stand dauer_ms, das erste Diagramm nach dem Laden
        CHECK(fenster.diagramm() == "dauer_ms");
        CHECK_FALSE(fenster.zurueck());
    }

    TEST_CASE("Bedingte Uebersetzung: Definitionen waehlen die Variante, der Schalter liest alle Zweige")
    {
        const char* quelle = "#ifdef BOARD_A\nvoid a(void) { x(); }\n#else\nvoid b(void) { y(); }\n#endif\n"
                             "void immer(void) {\n#if STUFE > 1\n  laut();\n#endif\n  leise();\n}\n";
        Hauptfenster fenster;
        fenster.zeige(quelle, "v.c");
        CHECK(fenster.diagramme() == QStringList{"b", "immer", "calls", "ifconfig", "ifnames"});
        CHECK(fenster.unbekannteNamen() == QStringList{"BOARD_A", "STUFE"});
        CHECK(fenster.waehleDiagramm("immer"));
        CHECK_FALSE(fenster.diagrammText().contains("laut"));

        fenster.setzeDefinitionen("BOARD_A, STUFE=2");
        CHECK(fenster.diagramme() == QStringList{"a", "immer", "calls", "ifconfig", "ifnames"});
        CHECK(fenster.unbekannteNamen().isEmpty());
        CHECK(fenster.diagramm() == "immer");  // das gewaehlte Diagramm bleibt
        CHECK(fenster.diagrammText().contains("laut"));

        fenster.setzeAlleZweige(true);
        CHECK(fenster.diagramme() == QStringList{"a", "b", "immer", "calls"});
        fenster.setzeAlleZweige(false);
        fenster.setzeDefinitionen("");
        CHECK(fenster.diagramme() == QStringList{"b", "immer", "calls", "ifconfig", "ifnames"});
    }

    TEST_CASE("Das Feld Configuration: Haken und Knoepfe schreiben ins Feld Defines, gelesen wird neu")
    {
        const char* quelle = "#define FEST 1\nvoid f(void)\n{\n#if STUFE >= 2\n  laut();\n#elif STUFE == 1\n  leise();\n"
                             "#endif\n#ifdef DEBUG\n  log();\n#endif\n#if FEST\n  fest();\n#endif\n}\n";
        Hauptfenster fenster;
        fenster.zeige(quelle, "k.c");
        c2d::ui::Konfiguration& k = fenster.konfiguration();
        CHECK(k.schalter() == QStringList{"-STUFE", "-DEBUG", "FEST (locked)"});
        CHECK(k.weichen() == QStringList{"4: STUFE >= 2 | STUFE == 1 | [(no #else)]",
                                         "9: defined(DEBUG) | [(no #else)]", "12: [FEST] | ((no #else))"});

        // ein Zweig waehlt die Namen; das Diagramm folgt
        CHECK(k.waehleZweig(4, 1));
        CHECK(fenster.diagramm() == "f");
        CHECK(fenster.diagrammText().contains("leise"));
        CHECK(k.schalter().first() == "STUFE=1");
        CHECK(k.weichen().first() == "4: STUFE >= 2 | [STUFE == 1] | (no #else)");
        CHECK(k.waehleZweig(4, 0));  // STUFE >= 2: nachgerechnet, STUFE=2
        CHECK(k.schalter().first() == "STUFE=2");
        CHECK(k.waehleZweig(4, 2));  // das fehlende #else: STUFE entfernt
        CHECK(k.schalter().first() == "-STUFE");
        CHECK_FALSE(k.waehleZweig(12, 1));  // FEST definiert die Datei

        // die Haken
        CHECK(k.setzeSchalter("DEBUG", true));
        CHECK(fenster.diagrammText().contains("log()"));
        CHECK(k.setzeSchalter("STUFE", true, "2"));
        CHECK(fenster.diagrammText().contains("laut"));
        CHECK(k.setzeSchalter("DEBUG", false));
        CHECK_FALSE(k.setzeSchalter("FEST", false));  // die Datei definiert ihn
        CHECK(k.schalter() == QStringList{"STUFE=2", "-DEBUG", "FEST (locked)"});

        // ohne Ordner gibt es keine Projektdatei
        QString fehler;
        CHECK_FALSE(fenster.speichereKonfiguration(&fehler));
        CHECK(fehler == "No folder is open");
    }

    TEST_CASE("Das Feld Configuration im Projekt: in die Projektdatei schreiben; von dort Definiertes mit !NAME aufheben")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        QFile quelle(QDir(ordner.path()).filePath(QStringLiteral("a.c")));
        REQUIRE(quelle.open(QIODevice::WriteOnly));
        quelle.write("void f(void)\n{\n#ifdef BOARD_X\n  x();\n#endif\n}\n");
        quelle.close();

        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        c2d::ui::Konfiguration& k = fenster.konfiguration();
        CHECK(k.schalter() == QStringList{"-BOARD_X"});
        CHECK(k.setzeSchalter("BOARD_X", true));
        CHECK(k.schalter() == QStringList{"BOARD_X"});

        QString fehler;
        REQUIRE(fenster.speichereKonfiguration(&fehler));
        CHECK(fenster.projektdateiText().contains("BOARD_X"));
        // Defines leer: BOARD_X kommt jetzt aus der Projektdatei — abwaehlen hebt ihn mit !BOARD_X auf
        fenster.setzeDefinitionen("");
        CHECK(k.schalter() == QStringList{"BOARD_X"});
        CHECK(k.weichen() == QStringList{"3: [defined(BOARD_X)] | (no #else)"});
        CHECK(k.setzeSchalter("BOARD_X", false));
        CHECK(k.schalter() == QStringList{"-BOARD_X"});
        CHECK(k.weichen() == QStringList{"3: defined(BOARD_X) | [(no #else)]"});
        CHECK(k.waehleZweig(3, 0));  // der Zweig nimmt !BOARD_X zurueck
        CHECK(k.schalter() == QStringList{"BOARD_X"});
        CHECK(k.waehleZweig(3, 1));
        CHECK(k.schalter() == QStringList{"-BOARD_X"});

        // gespeichert: aus BOARD_X wird !BOARD_X
        REQUIRE(fenster.speichereKonfiguration(&fehler));
        CHECK(fenster.projektdateiText().contains("\"!BOARD_X\""));
        CHECK_FALSE(fenster.projektdateiText().contains("\"BOARD_X\""));
        fenster.setzeDefinitionen("");
        CHECK(k.schalter() == QStringList{"-BOARD_X"});
    }

    TEST_CASE("Eine Datei oeffnen; eine fehlende meldet es und laesst das Fenster, wie es war")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QString pfad = QDir(ordner.path()).filePath("probe.c");
        {
            QFile datei(pfad);
            REQUIRE(datei.open(QIODevice::WriteOnly));
            datei.write("struct A { int x; };\nint f(void) { return 1; }\nint g(void) { return 2 }\n");
        }

        Hauptfenster fenster;
        QString fehler;
        CHECK(fenster.oeffne(pfad, &fehler));
        CHECK(fehler.isEmpty());
        CHECK(fenster.windowTitle().startsWith("probe.c"));
        CHECK(fenster.diagramme() == QStringList{"f", "g", "types", "calls"});
        CHECK(fenster.meldungen() == 1);  // das fehlende Semikolon in g

        CHECK_FALSE(fenster.oeffne(QDir(ordner.path()).filePath("fehlt.c"), &fehler));
        CHECK(fehler.contains("fehlt.c"));
        CHECK(fenster.diagramme().size() == 4);
    }

    TEST_CASE("Start der Kerne (SQ1-SQ4): in jeder Datei zur Wahl, Depth mit eigener Vorgabe, Klick zeigt die Zeile")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QDir wurzel(ordner.path());
        const auto lege = [&](const QString& relativ, const QByteArray& inhalt) {
            const QString pfad = wurzel.filePath(relativ);
            REQUIRE(QDir().mkpath(QFileInfo(pfad).absolutePath()));
            QFile datei(pfad);
            REQUIRE(datei.open(QIODevice::WriteOnly));
            datei.write(inhalt);
        };
        lege("code2diagram.json", R"({"kerne": [{"name": "CPU1", "ordner": ["a"]}, {"name": "CPU2", "ordner": ["b"]}]})");
        lege("a/a.c", "void starte(void) { IPC_setBootMode(IPC_CPU1_L_CPU2_R, 1); }\n"  // 1
                      "void main(void)\n"                                                // 2
                      "{\n"                                                             // 3
                      "    starte();\n"                                                  // 4
                      "    lade();\n"                                                    // 5
                      "    IPC_sync(IPC_CPU1_L_CPU2_R, IPC_FLAG31);\n"                   // 6
                      "    for (;;) { arbeite(); }\n"                                    // 7
                      "}\n");
        lege("b/b.c", "void main(void) { IPC_sync(IPC_CPU2_L_CPU1_R, IPC_FLAG31); for (;;) { warte(); } }\n");

        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        CHECK(fenster.diagramme().contains("seq"));
        fenster.setzeTiefe(3);  // der Aufrufgraph
        REQUIRE(fenster.zeigeSequenz());
        CHECK(fenster.diagramm() == "seq");
        CHECK(fenster.tiefe() == 1);  // die Sequenz hat ihre eigene Vorgabe (SQ4)
        CHECK(fenster.statusBar()->currentMessage().contains("sequence: 2 core(s)"));
        REQUIRE(fenster.waehleFormat("mermaid"));
        const QString text = fenster.diagrammText();
        CHECK(text.contains("L0-)L1: boot (in starte)"));
        CHECK(text.contains("Note over L0,L1: barrier FLAG 31"));
        CHECK(text.contains("loop main loop: for (\uFF1B\uFF1B)"));  // `;` beendet in Mermaid die Zeile
        CHECK(fenster.vorschauFormen() > 0);
        // Klick auf die Barriere: ihre Zeile in a/a.c
        REQUIRE(fenster.klicke("barrier FLAG 31"));
        CHECK(fenster.gezeigteStelle() == "a/a.c:6");
        // zurueck zum Aufrufgraph: Depth wieder wie vorher
        REQUIRE(fenster.waehleDiagramm("main"));
        CHECK(fenster.tiefe() == 3);
    }

    TEST_CASE("Ein Ordner als Projekt: Dateien, Spruenge ueber Dateien, Mehrdeutiges, Projektdatei")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QDir wurzel(ordner.path());
        const auto lege = [&](const QString& relativ, const QByteArray& inhalt) {
            const QString pfad = wurzel.filePath(relativ);
            REQUIRE(QDir().mkpath(QFileInfo(pfad).absolutePath()));
            QFile datei(pfad);
            REQUIRE(datei.open(QIODevice::WriteOnly));
            datei.write(inhalt);
        };
        lege("code2diagram.json", R"({"includePfade": ["inc"], "uebergehen": ["alt"],
              "abschnitte": [{"ordner": "antrieb", "definitionen": ["MOTOREN=2"]}]})");
        lege("inc/config.h", "#define MIT_DIAGNOSE\n");
        lege("main.c", "#include \"config.h\"\nstatic int pruefe(void) { return 1; }\n"
                       "int main(void) { pruefe(); motor_starte(2); regle(1);\n#ifdef MIT_DIAGNOSE\n diagnose();\n#endif\n"
                       " return 0; }\nvoid diagnose(void) {}\n");
        lege("antrieb/motor.c", "static int pruefe(int n) { return n; }\nvoid motor_starte(int n) { pruefe(n); }\n"
                                "#if MOTOREN > 1\nvoid motor_zwei(void) {}\n#endif\n");
        lege("antrieb/regler.c", "void regle(int s) { (void)s; }\n");
        lege("sim/regler_sim.c", "void regle(int s) { (void)s; }\n");
        lege("alt/x.c", "void alt(void) {}\n");

        Hauptfenster fenster;
        QString fehler;
        CHECK_FALSE(fenster.oeffneOrdner(wurzel.filePath("gibt_es_nicht"), &fehler));
        CHECK(fehler.contains("Not a folder"));
        REQUIRE(fenster.oeffneOrdner(ordner.path(), &fehler));
        CHECK(fenster.istProjekt());
        CHECK(fenster.dateien() == QStringList{"antrieb/motor.c", "antrieb/regler.c", "inc/config.h", "main.c", "sim/regler_sim.c"});
        CHECK(fenster.uebergangeneOrdner() == QStringList{"alt"});
        CHECK(fenster.datei() == "antrieb/motor.c");  // die erste mit einer Funktion
        CHECK(fenster.diagramme() == QStringList{"pruefe", "motor_starte", "motor_zwei", "calls", "ifconfig", "ifnames"});  // MOTOREN aus dem Abschnitt
        CHECK(fenster.windowTitle().contains("antrieb/motor.c"));

        REQUIRE(fenster.waehleDatei("main.c"));
        CHECK_FALSE(fenster.waehleDatei("alt/x.c"));
        CHECK(fenster.diagramme() == QStringList{"pruefe", "main", "diagnose", "calls", "ifconfig", "ifnames"});
        CHECK(fenster.waehleDiagramm("main"));
        CHECK(fenster.diagrammText().contains("diagnose()"));  // MIT_DIAGNOSE aus inc/config.h

        // static: der Aufruf in main.c meint die eigene pruefe
        CHECK(fenster.ziele("pruefe") == QStringList{"main.c:2"});
        // zwei Dateien definieren regle: nicht still waehlen
        CHECK(fenster.ziele("regle") == QStringList{"antrieb/regler.c:1", "sim/regler_sim.c:1"});
        CHECK_FALSE(fenster.springe("regle"));
        CHECK_FALSE(fenster.betteEin("regle"));
        CHECK(fenster.datei() == "main.c");
        // eindeutig in einer anderen Datei: springen, und zurueck
        CHECK(fenster.springe("motor_starte"));
        CHECK(fenster.datei() == "antrieb/motor.c");
        CHECK(fenster.diagramm() == "motor_starte");
        CHECK(fenster.ziele("pruefe") == QStringList{"antrieb/motor.c:1"});
        CHECK(fenster.aufrufer() == QStringList{"main() - main.c:3"});  // wer ruft motor_starte?
        CHECK(fenster.markiert().isEmpty());
        CHECK(fenster.zurueck());
        CHECK(fenster.datei() == "main.c");
        CHECK(fenster.diagramm() == "main");
        CHECK(fenster.markiert().contains("motor_starte(2)"));  // zurueck zum Aufruf, hervorgehoben
        CHECK(fenster.aufrufer().isEmpty());                    // main ruft niemand
        CHECK_FALSE(fenster.springeZuAufrufer(0));
        // zum Aufrufer: von motor_starte nach main, der Aufruf dort ist hervorgehoben
        REQUIRE(fenster.waehleDatei("antrieb/motor.c"));
        REQUIRE(fenster.waehleDiagramm("motor_starte"));
        CHECK(fenster.springeZuAufrufer(0));
        CHECK(fenster.datei() == "main.c");
        CHECK(fenster.markiert().contains("motor_starte(2)"));
        CHECK(fenster.zurueck());
        CHECK(fenster.diagramm() == "motor_starte");
        REQUIRE(fenster.waehleDatei("main.c"));
        REQUIRE(fenster.waehleDiagramm("main"));
        // die Wahl unter mehreren
        CHECK(fenster.springe("regle", 1));
        CHECK(fenster.datei() == "sim/regler_sim.c");
        CHECK(fenster.zurueck());
        // einbetten aus einer anderen Datei
        CHECK(fenster.waehleFormat("mermaid"));
        CHECK(fenster.betteEin("motor_starte"));
        CHECK(fenster.einbettungen() == 1);
        CHECK(fenster.diagrammText().contains("subgraph g0[\"motor_starte(int n)\"]"));

        // von aussen geht der Projektdatei vor; Datei und Diagramm bleiben
        REQUIRE(fenster.waehleDatei("antrieb/motor.c"));
        fenster.setzeDefinitionen("MOTOREN=1");
        CHECK(fenster.datei() == "antrieb/motor.c");
        CHECK(fenster.diagramme() == QStringList{"pruefe", "motor_starte", "calls", "ifconfig", "ifnames"});
        fenster.setzeDefinitionen("");

        // die Projektdatei: was nicht passt, wird nicht geschrieben
        CHECK(fenster.projektdateiText().contains("\"alt\""));
        CHECK_FALSE(fenster.speichereProjektdatei("{ kaputt", &fehler));
        CHECK(fehler.startsWith("Line 1"));
        CHECK(fenster.projektdateiText().contains("\"alt\""));
        // einen Ordner uebergehen und wieder einlesen
        REQUIRE(fenster.uebergeheOrdner("sim", &fehler));
        CHECK_FALSE(fenster.dateien().contains("sim/regler_sim.c"));
        CHECK(fenster.uebergangeneOrdner().contains("sim"));
        CHECK(fenster.projektdateiText().contains("\"sim\""));
        CHECK(fenster.datei() == "antrieb/motor.c");
        REQUIRE(fenster.waehleDatei("main.c"));
        CHECK(fenster.ziele("regle") == QStringList{"antrieb/regler.c:1"});
        CHECK_FALSE(fenster.liesOrdnerWieder("gibt_es_nicht", &fehler));
        REQUIRE(fenster.liesOrdnerWieder("sim", &fehler));
        CHECK(fenster.dateien().contains("sim/regler_sim.c"));
        CHECK(fenster.projektMeldungen().contains("alt: folder skipped (\"uebergehen\": \"alt\")"));
    }

    TEST_CASE("Ohne Projektdatei: der Vorschlag kommt aus der .gitignore; Uebergehen legt sie an")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QDir wurzel(ordner.path());
        {
            QFile git(wurzel.filePath(".gitignore"));
            REQUIRE(git.open(QIODevice::WriteOnly));
            git.write("out/\nlogs/\n*.o\n");
            REQUIRE(QDir().mkpath(wurzel.filePath("logs")));
            REQUIRE(QDir().mkpath(wurzel.filePath("b")));
            QFile a(wurzel.filePath("a.c"));
            REQUIRE(a.open(QIODevice::WriteOnly));
            a.write("void a(void) {}\n");
            QFile b(wurzel.filePath("b/b.c"));
            REQUIRE(b.open(QIODevice::WriteOnly));
            b.write("void b(void) {}\n");
        }
        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        CHECK(fenster.dateien() == QStringList{"a.c", "b/b.c"});
        CHECK(fenster.projektdateiText().contains("\"uebergehen\": [\n    \"logs\"\n  ]"));
        CHECK_FALSE(QFileInfo::exists(wurzel.filePath("code2diagram.json")));
        QString fehler;
        REQUIRE(fenster.uebergeheOrdner("b", &fehler));
        CHECK(QFileInfo::exists(wurzel.filePath("code2diagram.json")));
        CHECK(fenster.dateien() == QStringList{"a.c"});
        CHECK(fenster.projektdateiText().contains("\"logs\",\n    \"b\""));
    }

    TEST_CASE("Ein erkannter Zustandsautomat steht zur Wahl und wird gezeichnet")
    {
        Hauptfenster fenster;
        fenster.zeige("void takt(void) { switch (z) { case A: z = B; break; case B: z = C; break; case C: z = A; break; } }\n"
                      "void reset(void) { z = A; }\n",
                      "a.c");
        CHECK(fenster.diagramme() == QStringList{"takt", "reset", "automat:z", "calls"});
        REQUIRE(fenster.waehleDiagramm("automat:z"));
        CHECK(fenster.vorschauFormen() == 4);  // A, B, C und der Punkt von aussen
        CHECK(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().startsWith("%% z in takt()\nstateDiagram-v2\n"));
        CHECK(fenster.diagrammText().contains("[*] --> s0 : in reset()"));
        CHECK(fenster.aufrufer().isEmpty());  // ein Automat ist keine Funktion
    }

    TEST_CASE("Der Aufrufgraph: ab einer Funktion und der Datei, Tiefe, Aufklappen, Doppelklick-Weg und zurueck")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QDir wurzel(ordner.path());
        const auto lege = [&](const QString& relativ, const QByteArray& inhalt) {
            const QString pfad = wurzel.filePath(relativ);
            REQUIRE(QDir().mkpath(QFileInfo(pfad).absolutePath()));
            QFile datei(pfad);
            REQUIRE(datei.open(QIODevice::WriteOnly));
            datei.write(inhalt);
        };
        lege("main.c", "int main(void) { motor_starte(); regle(); memcpy(0, 0, 0); return 0; }\n");
        lege("antrieb/motor.c", "void takt(void) { }\nvoid motor_starte(void) { takt(); }\nvoid regle(void) { }\n");
        lege("sim/sim.c", "void regle(void) { }\n");

        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        REQUIRE(fenster.waehleDatei("main.c"));
        CHECK(fenster.aufrufgraphKnoten().isEmpty());  // noch keiner gezeigt
        CHECK_FALSE(fenster.zeigeAufrufgraph("gibt_es_nicht"));

        fenster.setzeTiefe(1);
        REQUIRE(fenster.zeigeAufrufgraph("main"));
        CHECK(fenster.diagramm() == "calls:main");
        CHECK(fenster.diagramme().contains("calls:main"));
        // motor_starte ruft weiter: an der Grenze markiert; regle zweimal, memcpy fremd
        CHECK(fenster.aufrufgraphKnoten() ==
              QStringList{"main", "motor_starte …", "regle", "regle", "memcpy"});
        CHECK(fenster.vorschauFormen() == 5);
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().startsWith("%% calls from main()\nflowchart TD\n"));
        CHECK(fenster.diagrammText().contains("-.->|\"?\"|"));

        // aufklappen: takt erscheint
        CHECK(fenster.klappeAuf("motor_starte"));
        CHECK(fenster.aufrufgraphKnoten().contains("takt"));
        CHECK(fenster.aufrufgraphKnoten().contains("motor_starte"));
        // hoechstens zwei Funktionen: main und motor_starte, main ruft weiter; takt, weil aufgeklappt
        fenster.setzeHoechstens(2);
        CHECK(fenster.aufrufgraphKnoten() == QStringList{"main …", "motor_starte", "memcpy", "takt"});
        fenster.setzeHoechstens(300);
        // ohne Fremde, alle Stufen
        fenster.setzeOhneFremde(true);
        fenster.setzeTiefe(0);
        CHECK_FALSE(fenster.aufrufgraphKnoten().contains("memcpy"));
        CHECK(fenster.tiefe() == 0);
        // nach oben: wer ruft regle (die beiden sind mehrdeutig gerufen)
        REQUIRE(fenster.setzeAufrufRichtung("callers"));
        CHECK_FALSE(fenster.setzeAufrufRichtung("seitwaerts"));
        CHECK(fenster.diagrammText().startsWith("%% callers of main()"));
        REQUIRE(fenster.setzeAufrufRichtung("calls"));

        // vom Graph in den Ablauf, und zurueck in den Graph
        REQUIRE(fenster.waehleDiagramm("main"));
        REQUIRE(fenster.springe("motor_starte"));
        CHECK(fenster.diagramm() == "motor_starte");
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "main");
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "calls:main");  // der Graph wird wieder angelegt
        CHECK(fenster.datei() == "main.c");

        // der Graph der Datei
        REQUIRE(fenster.waehleDatei("antrieb/motor.c"));
        REQUIRE(fenster.zeigeAufrufgraph());
        CHECK(fenster.diagramm() == "calls");
        CHECK(fenster.aufrufgraphKnoten() == QStringList{"takt", "motor_starte", "regle"});
    }

    TEST_CASE("Ein Klick zeigt die Stelle im Quelltext: im Ablauf, im Aufrufgraph, bei den Datentypen")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QDir wurzel(ordner.path());
        const auto lege = [&](const QString& relativ, const QByteArray& inhalt) {
            const QString pfad = wurzel.filePath(relativ);
            REQUIRE(QDir().mkpath(QFileInfo(pfad).absolutePath()));
            QFile datei(pfad);
            REQUIRE(datei.open(QIODevice::WriteOnly));
            datei.write(inhalt);
        };
        lege("main.c",
             "#define QUADRAT(x) ((x) * (x))\n"  // 1
             "int main(void)\n"                  // 2
             "{\n"                               // 3
             "    motor_starte();\n"             // 4
             "    memcpy(0, 0, 0);\n"            // 5
             "    return QUADRAT(2);\n"          // 6
             "}\n");
        lege("motor.c",
             "void takt(void) { }\n"          // 1
             "\n"                             // 2
             "void motor_starte(void)\n"      // 3
             "{\n"                            // 4
             "    takt();\n"                  // 5
             "}\n");
        lege("motor.cpp",
             "class Motor {\n"        // 1
             "public:\n"              // 2
             "    void starte();\n"   // 3
             "private:\n"             // 4
             "    int m_takt = 0;\n"  // 5
             "};\n");

        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        REQUIRE(fenster.waehleDatei("main.c"));
        CHECK(fenster.gezeigteStelle().isEmpty());

        // im Ablauf: die Zeile der Anweisung, auch fuer einen Aufruf (der Doppelklick fuehrt zur Funktion)
        REQUIRE(fenster.waehleDiagramm("main"));
        CHECK(fenster.klicke("memcpy(0, 0, 0)"));
        CHECK(fenster.gezeigteStelle() == "main.c:5");
        CHECK(fenster.klicke("motor_starte()"));
        CHECK(fenster.gezeigteStelle() == "main.c:4");
        CHECK_FALSE(fenster.klicke("gibt_es_nicht()"));
        CHECK(fenster.gezeigteStelle() == "main.c:4");  // bleibt

        // im Aufrufgraph: die Zeile der Funktion, auch in einer anderen Datei; ein Makro, wo es steht
        fenster.setzeTiefe(0);
        REQUIRE(fenster.zeigeAufrufgraph("main"));
        CHECK(fenster.klicke("takt"));
        CHECK(fenster.gezeigteStelle() == "motor.c:1");
        CHECK(fenster.klicke("motor_starte"));
        CHECK(fenster.gezeigteStelle() == "motor.c:3");
        CHECK(fenster.klicke("QUADRAT"));
        CHECK(fenster.gezeigteStelle() == "main.c:1");
        CHECK_FALSE(fenster.klicke("memcpy"));  // fremd: keine Stelle im Projekt
        CHECK(fenster.gezeigteStelle() == "main.c:1");

        // bei den Datentypen: der Kopf, ein Feld, eine Methode
        REQUIRE(fenster.waehleDatei("motor.cpp"));
        REQUIRE(fenster.waehleDiagramm(Hauptfenster::kDatentypen));
        CHECK(fenster.klicke("Motor::m_takt"));
        CHECK(fenster.gezeigteStelle() == "motor.cpp:5");
        CHECK(fenster.klicke("Motor::starte"));
        CHECK(fenster.gezeigteStelle() == "motor.cpp:3");
        CHECK(fenster.klicke("Motor"));
        CHECK(fenster.gezeigteStelle() == "motor.cpp:1");
        CHECK_FALSE(fenster.klicke("Motor::gibt_es_nicht"));
    }

    TEST_CASE("Umgekehrt: eine Zeile im Quelltext zeigt ihren Knoten, wechselt wenn noetig die Funktion")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        QFile datei(QDir(ordner.path()).filePath(QStringLiteral("a.c")));
        REQUIRE(datei.open(QIODevice::WriteOnly));
        datei.write("struct Punkt {\n"     // 1
                    "    int x;\n"         // 2
                    "};\n"                 // 3
                    "void eins(void)\n"    // 4
                    "{\n"                  // 5
                    "    a = 1;\n"         // 6
                    "    b = 2;\n"         // 7  gebuendelt mit a = 1
                    "    if (a)\n"         // 8
                    "        c();\n"       // 9
                    "}\n"                  // 10
                    "void zwei(void)\n"    // 11
                    "{\n"                  // 12
                    "    eins();\n"        // 13
                    "}\n"                  // 14
                    "int g;\n");           // 15
        datei.close();

        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        REQUIRE(fenster.waehleDiagramm("eins"));
        // in der gezeigten Funktion: der Knoten, auch die zweite Anweisung eines gebuendelten
        CHECK(fenster.zeigeImDiagramm(7));
        CHECK(fenster.markiert() == "a = 1\nb = 2");
        CHECK(fenster.gezeigteStelle() == "a.c:7");
        CHECK(fenster.zeigeImDiagramm(8));
        CHECK(fenster.markiert() == "a");
        CHECK(fenster.zeigeImDiagramm(10));  // die `}`: die Anweisung davor
        CHECK(fenster.markiert() == "c()");
        CHECK(fenster.diagramm() == "eins");

        // in einer anderen Funktion: dorthin, Back fuehrt zurueck
        CHECK(fenster.zeigeImDiagramm(13));
        CHECK(fenster.diagramm() == "zwei");
        CHECK(fenster.markiert() == "eins()");
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "eins");

        // im Aufrufgraph: die Funktion
        REQUIRE(fenster.zeigeAufrufgraph("zwei"));
        CHECK(fenster.zeigeImDiagramm(6));
        CHECK(fenster.markiert() == "eins");

        // ein Typ: Data types mit ihm
        CHECK(fenster.zeigeImDiagramm(2));
        CHECK(fenster.diagramm() == Hauptfenster::kDatentypen);
        CHECK(fenster.typ() == "Punkt");
        CHECK(fenster.markiert() == "Punkt");

        // nichts dazu
        CHECK_FALSE(fenster.zeigeImDiagramm(15));
        CHECK_FALSE(fenster.zeigeImDiagramm(99));
    }

    TEST_CASE("Nassi-Shneiderman: das Feld neben Diagram, gilt fuer jede Funktion; Klick, Sprung, Show in diagram")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        QFile datei(QDir(ordner.path()).filePath(QStringLiteral("a.c")));
        REQUIRE(datei.open(QIODevice::WriteOnly));
        datei.write("void zwei(void)\n"   // 1
                    "{\n"                 // 2
                    "    b = 2;\n"        // 3
                    "}\n"                 // 4
                    "void eins(int x)\n"  // 5
                    "{\n"                 // 6
                    "    if (x)\n"        // 7
                    "        zwei();\n"   // 8
                    "    a = 1;\n"        // 9
                    "}\n");               // 10
        datei.close();

        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        REQUIRE(fenster.waehleDiagramm("eins"));
        CHECK(fenster.darstellung() == Hauptfenster::kFlussdiagramm);
        CHECK_FALSE(fenster.waehleDarstellung("kreis"));
        REQUIRE(fenster.waehleDarstellung(Hauptfenster::kStruktogramm));
        CHECK(fenster.vorschauFormen() == 3);  // if, zwei(), a = 1
        CHECK(fenster.diagrammText().startsWith("<svg xmlns="));
        CHECK(fenster.diagrammText().contains(">eins(int x)</tspan>"));

        // Klick: die Stelle; Show in diagram: der Block
        CHECK(fenster.klicke("a = 1"));
        CHECK(fenster.gezeigteStelle() == "a.c:9");
        CHECK(fenster.zeigeImDiagramm(8));
        CHECK(fenster.markiert() == "zwei()");
        // ein Sprung bleibt im Struktogramm; Einbetten gibt es hier nicht
        CHECK_FALSE(fenster.betteEin("zwei"));
        REQUIRE(fenster.springe("zwei"));
        CHECK(fenster.diagramm() == "zwei");
        CHECK(fenster.darstellung() == Hauptfenster::kStruktogramm);
        CHECK(fenster.vorschauFormen() == 1);
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "eins");

        // Mermaid kann es nicht: der Text sagt es
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().startsWith("Nassi-Shneiderman is not available in mermaid"));
        // zurueck zum Ablauf
        REQUIRE(fenster.waehleDarstellung(Hauptfenster::kFlussdiagramm));
        CHECK(fenster.diagrammText().startsWith("%% eins"));
    }

    TEST_CASE("Wahrheitstabelle: aus dem Quelltext, als eigenes Diagramm, in csv; Mermaid sagt nein; Back")
    {
        Hauptfenster fenster;
        fenster.zeige(QStringLiteral("int f(int a, int b)\n"  // 1
                                     "{\n"                   // 2
                                     "    if (a && b)\n"     // 3
                                     "        return 1;\n"   // 4
                                     "    return a || b;\n"  // 5
                                     "}\n"),
                      QStringLiteral("t.c"));
        REQUIRE(fenster.waehleDiagramm("f"));
        CHECK(fenster.tabellenSymbole() == QStringList(2, QStringLiteral("Truth table - click to show it")));
        CHECK_FALSE(fenster.zeigeWahrheitstabelle(4));  // keine Bedingung mit zwei Atomen
        REQUIRE(fenster.zeigeWahrheitstabelle(3));
        CHECK(fenster.diagramm() == "truth:f@3");
        CHECK(fenster.diagramme().contains("truth:f@3"));
        CHECK(fenster.vorschauFormen() == 5 * 5);  // #, A, B, Result, MC/DC — Kopf und vier Zeilen
        CHECK(fenster.diagrammText().startsWith("<svg xmlns="));
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().startsWith("f(): line 3;a && b\n#;A: a;B: b;Result;MC/DC\n"));
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText() == "Truth table is not available in mermaid");
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "f");
        // im Ablauf kann csv nicht
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText() == "Flowchart is not available in csv");
        REQUIRE(fenster.zeigeWahrheitstabelle(5));
        CHECK(fenster.diagramm() == "truth:f@5");
    }

    TEST_CASE("Wahrheitstabelle: zwei Bedingungen in einer Zeile heissen @zeile:n, jede erreichbar (L2)")
    {
        Hauptfenster fenster;
        fenster.zeige(QStringLiteral("int f(int a, int b, int c, int d)\n"  // 1
                                     "{\n"                                  // 2
                                     "    if (a && b) x = c || d;\n"        // 3
                                     "    return a || d;\n"                 // 4
                                     "}\n"),
                      QStringLiteral("t.c"));
        REQUIRE(fenster.waehleDiagramm("f"));
        // aus dem Quelltext die erste der Zeile, mit ihrer Nummer im Namen
        REQUIRE(fenster.zeigeWahrheitstabelle(3));
        CHECK(fenster.diagramm() == "truth:f@3:1");
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().startsWith("f(): line 3;a && b\n"));
        // die zweite ueber ihren Namen
        REQUIRE(fenster.waehleDiagramm("truth:f@3:2"));
        CHECK(fenster.diagrammText().startsWith("f(): line 3;c || d\n"));
        CHECK_FALSE(fenster.waehleDiagramm("truth:f@3:3"));
        // allein in ihrer Zeile ohne Nummer
        REQUIRE(fenster.zeigeWahrheitstabelle(4));
        CHECK(fenster.diagramm() == "truth:f@4");
    }

    TEST_CASE("SQL-Notebook (L1, L2): die Zelle hat ihren Ablauf; ON und WHERE in einer Zeile, je erreichbar")
    {
        Hauptfenster fenster;
        fenster.zeige(QStringLiteral("-- Databricks notebook source\n"                                         // 1
                                     "CREATE TABLE a (id INT NOT NULL, b INT)\n"                              // 2
                                     "\n"                                                                      // 3
                                     "-- COMMAND ----------\n"                                                 // 4
                                     "\n"                                                                      // 5
                                     "SELECT * FROM a x JOIN a y ON x.id = y.id AND y.b > 0 WHERE x.b < 5 OR x.b > 9\n"  // 6
                                     "\n"                                                                      // 7
                                     "-- COMMAND ----------\n"                                                 // 8
                                     "\n"                                                                      // 9
                                     "SELECT 1 FROM a\n"),                                                    // 10
                      QStringLiteral("nb.sql"));
        // die Zellen sind Funktionen mit Ablauf; die Abfrage ohne `;` endet mit ihrer Zelle
        REQUIRE(fenster.waehleDiagramm("<cell 1>"));
        REQUIRE(fenster.waehleDiagramm("<cell 2>"));
        CHECK(fenster.diagramme().contains("<cell 3>"));
        // aus dem Quelltext: WHERE vor ON, der Name traegt die Nummer
        REQUIRE(fenster.zeigeWahrheitstabelle(6));
        CHECK(fenster.diagramm() == "truth:<cell 2>@6@6:2");
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().startsWith("<cell 2>@6 WHERE: line 6;"));
        // das ON ueber seinen Namen
        REQUIRE(fenster.waehleDiagramm("truth:<cell 2>@6@6:1"));
        CHECK(fenster.diagrammText().startsWith("<cell 2>@6 ON: line 6;"));
        CHECK_FALSE(fenster.waehleDiagramm("truth:<cell 2>@6@6:3"));
    }

    TEST_CASE("Entscheidungstabelle: dritte Darstellung; Zeilen sind Knoten — Klick, Show in diagram, Formate")
    {
        Hauptfenster fenster;
        fenster.zeige(QStringLiteral("int f(int a, int b)\n"  // 1
                                     "{\n"                   // 2
                                     "    if (a && b)\n"     // 3
                                     "        return 1;\n"   // 4
                                     "    return 0;\n"       // 5
                                     "}\n"),
                      QStringLiteral("t.c"));
        REQUIRE(fenster.waehleDiagramm("f"));
        REQUIRE(fenster.waehleDarstellung(Hauptfenster::kEntscheidungstabelle));
        CHECK(fenster.vorschauFormen() == 3);  // eine Bedingung, zwei Aktionen
        CHECK(fenster.diagrammText().contains(">R2</tspan>"));
        CHECK(fenster.klicke("return 0"));
        CHECK(fenster.gezeigteStelle() == "t.c:5");
        CHECK(fenster.zeigeImDiagramm(3));
        CHECK(fenster.markiert() == "a && b");
        CHECK_FALSE(fenster.betteEin("f"));
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText() == "f(): decision table\nConditions;R1;R2\na && b;Y;N\nActions\nreturn 1;X;\nreturn 0;;X\n");
        // die Bedingung hat eine Wahrheitstabelle
        REQUIRE(fenster.zeigeWahrheitstabelle(3));
        REQUIRE(fenster.zurueck());
        CHECK(fenster.darstellung() == Hauptfenster::kEntscheidungstabelle);
        CHECK(fenster.diagramm() == "f");
    }

    TEST_CASE("Zuordnungen: der switch als Tabelle, Lookup tables der Datei, Klick auf eine Zeile")
    {
        Hauptfenster fenster;
        fenster.zeige(QStringLiteral("static const int t[2] = {7, 9};\n"  // 1
                                     "int f(int p)\n"                      // 2
                                     "{\n"                                 // 3
                                     "    switch (p) {\n"                  // 4
                                     "    case 0: return 1;\n"             // 5
                                     "    default: return 2;\n"            // 6
                                     "    }\n"                             // 7
                                     "}\n"),
                      QStringLiteral("t.c"));
        CHECK(fenster.diagramme().contains("lookup"));
        REQUIRE(fenster.waehleDiagramm("f"));
        CHECK(fenster.tabellenSymbole() == QStringList{QStringLiteral("Mapping table - click to show it")});
        CHECK_FALSE(fenster.zeigeZuordnung(5));
        REQUIRE(fenster.zeigeZuordnung(4));
        CHECK(fenster.diagramm() == "mapping:f@4");
        CHECK(fenster.vorschauFormen() == 2);
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText() == "f(): switch (p)\np;return\n0;1\ndefault;2\n");
        CHECK(fenster.klicke("default"));
        CHECK(fenster.gezeigteStelle() == "t.c:6");
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "f");

        REQUIRE(fenster.waehleDiagramm("lookup"));
        CHECK(fenster.diagrammText() == "static const int t[2]\nindex;value\n0;7\n1;9\n");
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText() == "Mapping table is not available in mermaid");
    }

    TEST_CASE("Konfiguration: #if configuration der Datei, aus dem Quelltext, Klick, Make this branch active")
    {
        Hauptfenster fenster;
        fenster.zeige(QStringLiteral("#ifdef A\n"                     // 1
                                     "int a;\n"                       // 2
                                     "#elif B > 1\n"                  // 3
                                     "int b;\n"                       // 4
                                     "#endif\n"                       // 5
                                     "int f(void) { return 0; }\n"),  // 6
                      QStringLiteral("t.c"));
        CHECK(fenster.diagramme().contains("ifconfig"));
        CHECK_FALSE(fenster.zeigeKonfiguration(2));
        REQUIRE(fenster.zeigeKonfiguration(3));
        CHECK(fenster.diagramm() == "ifconfig");
        CHECK(fenster.vorschauFormen() == 3);  // je Zeile ein Knoten
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText() ==
              "#if defined(A) (line 1)\nbranch;A;B;active\n#if defined(A);def;-;\n#elif B > 1;!def;> 1;\n(no #else);!def;<= 1;yes\n");
        CHECK(fenster.klicke("#elif B > 1"));
        CHECK(fenster.gezeigteStelle() == "t.c:3");

        CHECK_FALSE(fenster.aktiviereZweig(5));  // (no #else) gilt schon
        REQUIRE(fenster.aktiviereZweig(3));
        CHECK(fenster.definitionen() == "B=2");
        CHECK(fenster.diagramm() == "ifconfig");  // neu gelesen, die Tabelle bleibt
        CHECK(fenster.diagrammText().contains("#elif B > 1;!def;> 1;yes"));
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText() == "#if configuration is not available in mermaid");

        // ein Name in einer Direktive (Rechtsklick im Quelltext): nicht die Woerter des Praeprozessors
        CHECK(fenster.nameInDirektive(3, 7) == "B");
        CHECK(fenster.nameInDirektive(3, 8) == "B");  // gleich hinter dem Wort
        CHECK(fenster.nameInDirektive(1, 3).isEmpty());  // ifdef
        CHECK(fenster.nameInDirektive(2, 5).isEmpty());  // keine Direktive
    }

    TEST_CASE("Wo schaltet ein Name: aus der #if-Konfiguration, Klick in eine andere Datei, zurueck, die Namen")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QDir wurzel(ordner.path());
        const auto lege = [&](const QString& relativ, const QByteArray& inhalt) {
            QFile datei(wurzel.filePath(relativ));
            REQUIRE(datei.open(QIODevice::WriteOnly));
            datei.write(inhalt);
        };
        lege("code2diagram.json", "{\"definitionen\": [\"TAKT=100\"]}\n");
        lege("a.c", "#if TAKT > 50\nint a;\n#endif\nint f(void) { return 0; }\n");
        lege("b.c", "#ifdef TAKT\nint b;\n#else\nint c;\n#endif\n");

        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        CHECK(fenster.datei() == "a.c");
        CHECK(fenster.diagramme().contains("ifnames"));
        CHECK_FALSE(fenster.zeigeSchaltstellen("GIBT_ES_NICHT"));
        REQUIRE(fenster.waehleDiagramm("ifconfig"));
        REQUIRE(fenster.zeigeSchaltstellen("TAKT"));
        CHECK(fenster.diagramm() == "switch:TAKT");
        CHECK(fenster.diagramme().contains("switch:TAKT"));
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().contains("project;code2diagram.json;;TAKT=100\n"));
        CHECK(fenster.diagrammText().contains("a.c;1;#if TAKT > 50;;> 50;= 100 (project);1;3;1;yes\n"));
        CHECK(fenster.diagrammText().contains("b.c;3;#else;;!def;= 100 (project);3;5;1;\n"));

        // ein Klick zeigt die Stelle, auch in einer anderen Datei; die gewaehlte Datei bleibt
        CHECK(fenster.klicke("b.c:3"));
        CHECK(fenster.gezeigteStelle() == "b.c:3");
        CHECK(fenster.datei() == "a.c");
        CHECK(fenster.zurueck());
        CHECK(fenster.diagramm() == "ifconfig");

        REQUIRE(fenster.waehleDiagramm("ifnames"));
        CHECK(fenster.diagrammText().contains("TAKT;2;2;= 100;project;\n"));
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText() == "#if names is not available in mermaid");
    }

    TEST_CASE("Links auf Datentypen: zweites Feld, aus dem Ablauf und aus einem Glied, ueber Dateien, zurueck")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QDir wurzel(ordner.path());
        const auto lege = [&](const QString& relativ, const QByteArray& inhalt) {
            const QString pfad = wurzel.filePath(relativ);
            REQUIRE(QDir().mkpath(QFileInfo(pfad).absolutePath()));
            QFile datei(pfad);
            REQUIRE(datei.open(QIODevice::WriteOnly));
            datei.write(inhalt);
        };
        lege("inc/typen.h", "typedef struct { float kp; } t_PID;\n"
                            "struct Regler { t_PID pid; int n; };\n"
                            "struct Fern { int x; };\n");
        lege("main.c", "#include \"inc/typen.h\"\nint main(void) { struct Regler r; t_PID p; return 0; }\n");
        lege("sim.c", "typedef struct { int y; } t_PID;\nvoid f(void) { }\n");

        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        REQUIRE(fenster.waehleDatei("inc/typen.h"));
        REQUIRE(fenster.waehleDiagramm("types"));
        CHECK(fenster.typen() == QStringList{"*", "t_PID", "Regler", "Fern"});
        CHECK(fenster.typ() == "t_PID");  // der erste
        REQUIRE(fenster.waehleTyp("Regler"));
        CHECK(fenster.vorschauFormen() == 2);  // Regler und t_PID, nicht Fern
        CHECK_FALSE(fenster.waehleTyp("gibt_es_nicht"));

        // aus dem Ablauf: main nennt Regler und t_PID — zum Typ in der eingebundenen Datei
        REQUIRE(fenster.waehleDatei("main.c"));
        REQUIRE(fenster.waehleDiagramm("main"));
        CHECK(fenster.typZiele("t_PID") == QStringList{"inc/typen.h:1"});  // nicht der in sim.c
        REQUIRE(fenster.zeigeTyp("Regler"));
        CHECK(fenster.datei() == "inc/typen.h");
        CHECK(fenster.diagramm() == "types");
        CHECK(fenster.typ() == "Regler");
        REQUIRE(fenster.zurueck());
        CHECK(fenster.datei() == "main.c");
        CHECK(fenster.diagramm() == "main");

        // ohne Einbindung: zwei Kandidaten, keine stille Wahl
        REQUIRE(fenster.waehleDatei("sim.c"));
        CHECK(fenster.typZiele("Regler") == QStringList{"inc/typen.h:2"});
        CHECK(fenster.typZiele("t_PID") == QStringList{"sim.c:1"});  // die eigene Datei zuerst

        // ein Typ wechselt im zweiten Feld; zurueck kehrt zum vorigen Typ
        REQUIRE(fenster.waehleDatei("inc/typen.h"));
        REQUIRE(fenster.waehleDiagramm("types:Fern"));
        CHECK(fenster.typ() == "Fern");
        REQUIRE(fenster.waehleTyp("Regler"));
        REQUIRE(fenster.zurueck());
        CHECK(fenster.typ() == "Fern");
    }

    TEST_CASE("Open recent: Dateien und Ordner, das Neueste zuerst, ohne Doppelte, ueber Neustarts")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QDir wurzel(ordner.path());
        const auto lege = [&](const QString& relativ) {
            QFile datei(wurzel.filePath(relativ));
            REQUIRE(datei.open(QIODevice::WriteOnly));
            datei.write("void f(void) { }\n");
            return QDir::cleanPath(QFileInfo(datei).absoluteFilePath());
        };
        const QString a = lege("a.c");
        const QString b = lege("b.c");
        const QString ordnerPfad = QDir::cleanPath(QFileInfo(ordner.path()).absoluteFilePath());

        {
            Hauptfenster fenster;
            fenster.leereZuletzt();
            CHECK(fenster.zuletztGeoeffnet().isEmpty());
            fenster.zeigeBeispiel();  // das Beispiel ist keine Datei
            CHECK(fenster.zuletztGeoeffnet().isEmpty());
            REQUIRE(fenster.oeffne(a));
            REQUIRE(fenster.oeffneOrdner(ordner.path()));
            REQUIRE(fenster.oeffne(b));
            REQUIRE(fenster.oeffne(a));  // noch einmal: nach vorn, nicht doppelt
            CHECK(fenster.zuletztGeoeffnet() == QStringList{a, b, ordnerPfad});
            CHECK_FALSE(fenster.oeffne(wurzel.filePath("gibt_es_nicht.c")));
            CHECK(fenster.zuletztGeoeffnet().size() == 3);  // was nicht aufgeht, kommt nicht hinein
        }
        {
            // ein neues Fenster liest die Liste wieder; ein Eintrag oeffnet Datei oder Ordner
            Hauptfenster fenster;
            CHECK(fenster.zuletztGeoeffnet() == QStringList{a, b, ordnerPfad});
            QString fehler;
            REQUIRE(fenster.oeffneZuletzt(2, &fehler));
            CHECK(fenster.istProjekt());
            CHECK(fenster.zuletztGeoeffnet().front() == ordnerPfad);
            REQUIRE(fenster.oeffneZuletzt(2, &fehler));  // jetzt b
            CHECK_FALSE(fenster.istProjekt());
            CHECK(fenster.datei() == "b.c");
            CHECK_FALSE(fenster.oeffneZuletzt(7, &fehler));

            // was es nicht mehr gibt, kommt beim Waehlen heraus — mit Meldung
            REQUIRE(QFile::remove(a));
            CHECK(fenster.zuletztGeoeffnet().indexOf(a) == 2);
            CHECK_FALSE(fenster.oeffneZuletzt(2, &fehler));
            CHECK(fehler.contains("no longer exists"));
            CHECK_FALSE(fenster.zuletztGeoeffnet().contains(a));

            // hoechstens zehn
            for (int i = 0; i < Hauptfenster::kZuletztHoechstens + 2; ++i) REQUIRE(fenster.oeffne(lege(QStringLiteral("n%1.c").arg(i))));
            CHECK(fenster.zuletztGeoeffnet().size() == Hauptfenster::kZuletztHoechstens);
            fenster.leereZuletzt();
        }
        CHECK(Hauptfenster().zuletztGeoeffnet().isEmpty());
    }

    TEST_CASE("JavaScript: ein Callback ist eine Funktion; von der Anweisung dorthin und zurueck")
    {
        Hauptfenster fenster;
        fenster.zeige("function verbinde(k) {\n  k.on('click', (e) => {\n    tu(e)\n  })\n}\n", "a.js");
        CHECK(fenster.diagramme().contains("verbinde::<anonymous:2>"));
        REQUIRE(fenster.waehleDiagramm("verbinde"));
        CHECK(fenster.ziele("verbinde::<anonymous:2>") == QStringList{"a.js:2"});
        REQUIRE(fenster.springe("verbinde::<anonymous:2>"));
        CHECK(fenster.diagramm() == "verbinde::<anonymous:2>");
        CHECK(fenster.diagrammText().contains("tu(e)"));
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "verbinde");
        CHECK(fenster.markiert().contains("k.on"));  // zurueck zu der Anweisung, in der sie steht
    }

    TEST_CASE("Java: die Methode einer anonymen Klasse ist eine Funktion; dorthin und zurueck; kein #if")
    {
        Hauptfenster fenster;
        fenster.zeige("class K {\n  void f(Handler h) {\n    h.post(new Runnable() {\n      public void run() { tu(); }\n"
                      "    });\n  }\n}\n",
                      "K.java");
        CHECK(fenster.meldungen() == 0);
        CHECK(fenster.diagramme().contains("K::f::<anonymous:3>.run"));
        CHECK(fenster.diagramme().contains("types"));
        CHECK_FALSE(fenster.diagramme().contains("ifnames"));
        REQUIRE(fenster.waehleDiagramm("K::f"));
        CHECK(fenster.ziele("K::f::<anonymous:3>.run") == QStringList{"K.java:4"});
        REQUIRE(fenster.springe("K::f::<anonymous:3>.run"));
        CHECK(fenster.diagrammText().contains("tu()"));
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "K::f");
        CHECK(fenster.markiert().contains("h.post"));
    }

    TEST_CASE("Kotlin: ein mehrzeiliges Lambda ist eine Funktion; dorthin und zurueck; kein #if")
    {
        Hauptfenster fenster;
        fenster.zeige("class K {\n    fun f(k: View) {\n        k.setOnClickListener {\n            tu()\n        }\n"
                      "    }\n}\n",
                      "K.kt");
        CHECK(fenster.meldungen() == 0);
        CHECK(fenster.diagramme().contains("K::f::<lambda:3>"));
        CHECK_FALSE(fenster.diagramme().contains("ifnames"));
        REQUIRE(fenster.waehleDiagramm("K::f"));
        CHECK(fenster.ziele("K::f::<lambda:3>") == QStringList{"K.kt:3"});
        REQUIRE(fenster.springe("K::f::<lambda:3>"));
        CHECK(fenster.diagrammText().contains("tu()"));
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "K::f");
        CHECK(fenster.markiert().contains("setOnClickListener"));
    }

    TEST_CASE("Notebook: das Skript im Quelltext, je Zelle <cell n>, Klick nennt die Zelle")
    {
        Hauptfenster fenster;
        fenster.zeige(QStringLiteral(R"nb({"nbformat": 4, "nbformat_minor": 5,
            "metadata": {"language_info": {"name": "python"}},
            "cells": [{"cell_type": "markdown", "metadata": {}, "source": ["# Laden"]},
                      {"cell_type": "code", "metadata": {}, "outputs": [], "source": ["%pip install x\n", "for i in range(3):\n", "    tu(i)"]}]})nb"),
                      QStringLiteral("n.ipynb"));
        CHECK(fenster.meldungen() == 1);  // die Magie
        CHECK(fenster.diagramme().contains("<cell 2>"));
        CHECK_FALSE(fenster.diagramme().contains("<module>"));
        REQUIRE(fenster.waehleDiagramm("<cell 2>"));
        CHECK(fenster.diagrammText().contains("tu(i)"));
        REQUIRE(fenster.klicke("tu(i)"));
        CHECK(fenster.gezeigteStelle() == "n.ipynb:7 (cell 2, line 3)");
    }

    TEST_CASE("Der Dateifilter nennt die Endungen der Sprachen des Kerns, vorn alle unterstuetzten")
    {
        // aus sprachen() gebildet: eine neue Sprache braucht hier keine neue Zeile
        const auto sprachen = c2d::sprache::sprachen();
        QStringList filter = Hauptfenster::dateiFilter().split(QStringLiteral(";;"));
        // dazu vorn alle unterstuetzten (die Vorgabe des Dialogs), hinten Notebooks und alle Dateien
        REQUIRE(filter.size() == static_cast<qsizetype>(sprachen.size()) + 3);
        const QString vorn = filter.takeFirst();
        REQUIRE(vorn.startsWith(QStringLiteral("All supported files (")));
        QStringList alle = vorn.mid(vorn.indexOf(QLatin1Char('(')) + 1).chopped(1).split(QLatin1Char(' '));
        CHECK(alle.removeDuplicates() == 0);
        CHECK(alle.contains("*.c"));
        CHECK(alle.contains("*.h"));
        CHECK(alle.contains("*.kt"));
        CHECK(alle.contains("*.sql"));
        CHECK(alle.back() == "*.ipynb");
        for (std::size_t i = 0; i < sprachen.size(); ++i)
        {
            for (const std::string_view endung : sprachen[i]->endungen())
                CHECK(alle.contains(QStringLiteral("*.") + QString::fromUtf8(endung.data(), static_cast<qsizetype>(endung.size()))));
            QStringList muster;
            for (const std::string_view endung : sprachen[i]->endungen())
                muster << QStringLiteral("*.") + QString::fromUtf8(endung.data(), static_cast<qsizetype>(endung.size()));
            const std::string_view name = sprachen[i]->name();
            CHECK(filter[static_cast<qsizetype>(i)] ==
                  QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())) + QStringLiteral(" files (") +
                      muster.join(QLatin1Char(' ')) + QLatin1Char(')'));
        }
        CHECK(filter.back() == "All files (*)");
        CHECK(filter[filter.size() - 2] == "Jupyter notebooks (*.ipynb)");
        // Stichproben, damit der Test nicht nur sich selbst nachrechnet
        CHECK(filter.front() == "C files (*.c *.h)");
        CHECK(filter.contains("C# files (*.cs)"));
        CHECK(filter.contains("SQL files (*.sql *.ddl)"));
    }

    TEST_CASE("Data dependencies: der Datei und ab einem Objekt, Klick zeigt die Stelle, Back zurueck")
    {
        Hauptfenster fenster;
        fenster.zeige("CREATE TABLE artikel (id INTEGER PRIMARY KEY);\n"
                      "CREATE TABLE bestand (artikel INTEGER, menge INTEGER);\n"
                      "CREATE VIEW knapp AS SELECT a.id FROM artikel a JOIN bestand b ON b.artikel = a.id;\n"
                      "CREATE TRIGGER merke AFTER UPDATE ON bestand BEGIN SELECT 1; END;\n",
                      "s.sql");
        CHECK(fenster.diagramme().contains("deps"));
        CHECK_FALSE(fenster.zeigeAbhaengigkeiten("gibt_es_nicht"));
        REQUIRE(fenster.zeigeAbhaengigkeiten());
        CHECK(fenster.diagramm() == "deps");
        CHECK(fenster.aufrufgraphKnoten() == QStringList{"artikel", "bestand", "knapp", "merke"});
        CHECK(fenster.vorschauFormen() == 4);
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().contains("|\"AFTER UPDATE\"|"));
        CHECK(fenster.klicke("knapp"));  // die Zeile der View

        REQUIRE(fenster.zeigeAbhaengigkeiten("knapp"));
        CHECK(fenster.diagramm() == "deps:knapp");
        CHECK(fenster.aufrufgraphKnoten() == QStringList{"knapp", "artikel", "bestand"});
        CHECK(fenster.zurueck());
        CHECK(fenster.diagramm() == "deps");
    }

    TEST_CASE("Ablauf von SQL-Routinen: Doppelklick in Data dependencies, Trigger ohne Rumpf, Symbol der Abfrage")
    {
        Hauptfenster fenster;
        fenster.zeige("CREATE TABLE artikel (id INTEGER PRIMARY KEY, bestand INTEGER);\n"
                      "CREATE PROCEDURE auffuellen() BEGIN\n"
                      "  UPDATE artikel SET bestand = bestand + 1;\n"
                      "  SELECT count(*) FROM artikel;\n"
                      "END;\n"
                      "CREATE TRIGGER merke AFTER UPDATE ON artikel BEGIN SELECT 1; END;\n",
                      "s.sql");
        CHECK(fenster.diagramme().contains("auffuellen"));
        REQUIRE(fenster.zeigeAbhaengigkeiten());
        REQUIRE(fenster.doppelklickeAbhaengigkeit("auffuellen"));
        CHECK(fenster.diagramm() == "auffuellen");
        // die Zeile mit dem SELECT hat eine Abfrage: das Symbol, ein Klick darauf zeigt sie (Entscheid R4)
        CHECK(fenster.tabellenSymbole().contains(QStringLiteral("Query diagram - click to show it")));
        CHECK(fenster.zurueck());
        CHECK(fenster.diagramm() == "deps");
        REQUIRE(fenster.doppelklickeAbhaengigkeit("merke"));
        CHECK(fenster.diagramm() == "merke");
        CHECK_FALSE(fenster.doppelklickeAbhaengigkeit("auffuellen"));  // nicht in Data dependencies
    }

    TEST_CASE("Wahrheitstabelle von WHERE: Symbol am Ergebnis der Abfrage, aus dem Quelltext, Back, Formate")
    {
        Hauptfenster fenster;
        fenster.zeige("CREATE TABLE kunde (id integer PRIMARY KEY, land text, aktiv boolean NOT NULL);\n"
                      "SELECT id FROM kunde k\n"
                      "WHERE NOT (k.land = 'AT') AND k.aktiv;\n",
                      "s.sql");
        REQUIRE(fenster.zeigeAbfrage());
        CHECK(fenster.tabellenSymbole() == QStringList{QStringLiteral("Truth table - click to show it")});
        // im Quelltext die Zeile der Bedingung
        REQUIRE(fenster.zeigeWahrheitstabelle(3));
        CHECK(fenster.diagramm() == "truth:query@2@3");
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().contains("query@2 WHERE: line 3"));
        CHECK(fenster.diagrammText().contains(";Row kept;"));
        CHECK(fenster.zurueck());
        CHECK(fenster.diagramm() == "queries");
    }

    TEST_CASE("CASE als Zuordnung: Symbol am Ergebnis, aus dem Quelltext, Back, csv")
    {
        Hauptfenster fenster;
        fenster.zeige("CREATE TABLE t (id integer PRIMARY KEY, s text);\n"
                      "SELECT id,\n"
                      "  CASE s WHEN 'a' THEN 1 WHEN 'b' THEN 2 END AS n\n"
                      "FROM t;\n",
                      "s.sql");
        REQUIRE(fenster.zeigeAbfrage());
        CHECK(fenster.tabellenSymbole() == QStringList{QStringLiteral("Mapping table - click to show it")});
        REQUIRE(fenster.zeigeZuordnung(3));
        CHECK(fenster.diagramm() == "mapping:query@2@3");
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().contains("otherwise;NULL (no ELSE)"));
        CHECK(fenster.zurueck());
        CHECK(fenster.diagramm() == "queries");
    }

    // die Schule aus beispiele/sql/schule.sql, gekuerzt
    constexpr const char* kSchule =
        "CREATE TABLE lehrer (lehrer_id INTEGER PRIMARY KEY, name VARCHAR(60) NOT NULL);\n"
        "CREATE TABLE kurs (kurs_id INTEGER PRIMARY KEY, lehrer_id INTEGER REFERENCES lehrer (lehrer_id));\n"
        "CREATE TABLE schueler (schueler_id INTEGER PRIMARY KEY, name VARCHAR(60));\n"
        "CREATE TABLE belegt (schueler_id INTEGER REFERENCES schueler (schueler_id),\n"
        "                     kurs_id INTEGER REFERENCES kurs (kurs_id), note INTEGER,\n"
        "                     PRIMARY KEY (schueler_id, kurs_id));\n";

    TEST_CASE("Notation: das Feld nur bei Tabellen, Chen und (min,max) im Text, gemerkt")
    {
        {
            Hauptfenster fenster;
            fenster.zeige(kSchule, "s.sql");
            REQUIRE(fenster.waehleDiagramm("types"));
            REQUIRE(fenster.waehleTyp("*"));
            CHECK(fenster.notationSichtbar());
            CHECK(fenster.notation() == "tabellen");
            CHECK(fenster.diagrammText().contains(">0..1</tspan>"));
            CHECK(fenster.vorschauFormen() == 4);
            CHECK_FALSE(fenster.waehleNotation("uml"));
            REQUIRE(fenster.waehleNotation("chen"));
            CHECK(fenster.diagrammText().contains("<ellipse"));
            // vier Entitaeten -- belegt ist selbst die Raute m:n -- und die Raute von kurs.lehrer_id
            CHECK(fenster.vorschauFormen() == 5);
            REQUIRE(fenster.waehleNotation("minmax"));
            CHECK(fenster.diagrammText().contains(">(0,*)</tspan>"));
            REQUIRE(fenster.waehleAttribute("keine"));
            CHECK_FALSE(fenster.diagrammText().contains("<ellipse"));
            CHECK_FALSE(fenster.waehleAttribute("manche"));
            // ein Klick auf den Kopf zeigt die Zeile der Tabelle, wie in Tables
            CHECK(fenster.klicke("belegt"));
            CHECK(fenster.gezeigteStelle() == "s.sql:4");
            // eine Datei ohne Tabellen: kein Feld
            fenster.zeige("struct S { int x; };\n", "s.c");
            REQUIRE(fenster.waehleDiagramm("types"));
            CHECK_FALSE(fenster.notationSichtbar());
        }
        {
            // gemerkt (QSettings); setzeNotation merkt nichts
            Hauptfenster zweites;
            CHECK(zweites.notation() == "minmax");
            CHECK(zweites.attribute() == "keine");
            REQUIRE(zweites.setzeNotation("kraehenfuss", "alle"));
            CHECK_FALSE(zweites.setzeNotation("uml"));
        }
        {
            Hauptfenster drittes;
            CHECK(drittes.notation() == "minmax");
            REQUIRE(drittes.waehleNotation("tabellen"));
            REQUIRE(drittes.waehleAttribute("alle"));
        }
    }

    TEST_CASE("Queries: das zweite Feld, nach Name und Zeile, Klick zeigt die Stelle, Back, Formate")
    {
        Hauptfenster fenster;
        fenster.zeige("CREATE TABLE artikel (artikel_id INTEGER PRIMARY KEY, bezeichnung TEXT);\n"  // 1
                      "CREATE TABLE bestand (artikel_id INTEGER, menge INTEGER);\n"                  // 2
                      "CREATE VIEW knapp AS SELECT a.bezeichnung, b.menge FROM artikel a\n"           // 3
                      "  JOIN bestand b ON b.artikel_id = a.artikel_id WHERE b.menge < 5;\n"          // 4
                      "\n"                                                                            // 5
                      "SELECT count(*) FROM bestand;\n",                                              // 6
                      "s.sql");
        CHECK(fenster.diagramme().contains("queries"));
        CHECK(fenster.abfragen() == QStringList{"knapp", "query@6"});
        CHECK_FALSE(fenster.zeigeAbfrage("gibt_es_nicht"));
        REQUIRE(fenster.zeigeAbfrage("knapp"));
        CHECK(fenster.diagramm() == "queries");
        CHECK(fenster.abfrage() == "knapp");
        CHECK(fenster.abfrageKnoten() == QStringList{"artikel a", "bestand b", "result", "knapp"});
        CHECK(fenster.vorschauFormen() == 4);
        CHECK(fenster.klicke("bestand b"));
        CHECK(fenster.gezeigteStelle() == "s.sql:4");
        // nach Zeile, dann zurueck zur View
        REQUIRE(fenster.zeigeAbfrage("6"));
        CHECK(fenster.abfrage() == "query@6");
        REQUIRE(fenster.zurueck());
        CHECK(fenster.abfrage() == "knapp");
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().contains("---|\"inner: b.artikel_id = a.artikel_id\"|"));
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().contains("Query is not available in csv"));
    }

    TEST_CASE("Data flow: Eintrag und zweites Feld, Datei und Funktion, Klick zeigt die Stelle, Back, Formate")
    {
        Hauptfenster fenster;
        fenster.zeige("from pyspark.sql import SparkSession\n"                        // 1
                      "def lade(p):\n"                                                 // 2
                      "    spark = SparkSession.builder.appName(\"A\").getOrCreate()\n"  // 3
                      "    df = spark.read.parquet(p).filter(\"x > 0\")\n"              // 4
                      "    df.groupBy(\"k\").count().show()\n"                          // 5
                      "def leer():\n"                                                  // 6
                      "    return 1\n",                                                // 7
                      "f.py");
        CHECK(fenster.diagramme().contains("flow"));
        CHECK(fenster.datenfluesse() == QStringList{"lade"});
        CHECK_FALSE(fenster.zeigeDatenfluss("leer"));
        REQUIRE(fenster.zeigeDatenfluss("lade"));
        CHECK(fenster.diagramm() == "flow");
        CHECK(fenster.datenflussKnoten() ==
              QStringList{"Session", "read parquet", "filter(\"x > 0\")", "groupBy(\"k\")", "Job 1: show()"});
        CHECK(fenster.vorschauFormen() == 5);
        CHECK(fenster.klicke("groupBy(\"k\")"));
        CHECK(fenster.gezeigteStelle() == "f.py:5");
        // die ganze Datei, dann zurueck zur Funktion
        REQUIRE(fenster.zeigeDatenfluss());
        CHECK(fenster.datenflussKnoten().size() == 5);
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "flow");
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().contains("{{\"<i>"));
        CHECK(fenster.diagrammText().contains("subgraph"));
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().contains("Data flow is not available in csv"));
    }

    TEST_CASE("Physical plan: aus einer Plan-Datei und aus der Ausgabe einer Zelle, Klick, logischer Abschnitt, Back")
    {
        Hauptfenster fenster;
        fenster.zeige("== Physical Plan ==\n"                                     // 1
                      "* Sort (4)\n"                                              // 2
                      "+- Exchange (3)\n"                                         // 3
                      "   +- * ColumnarToRow (2)\n"                               // 4
                      "      +- Scan parquet spark_catalog.default.t (1)\n"       // 5
                      "\n\n"
                      "(3) Exchange\n"
                      "Input [1]: [key#1]\n"
                      "Arguments: rangepartitioning(key#1 ASC NULLS FIRST, 200), ENSURE_REQUIREMENTS, [plan_id=1]\n",
                      "plan.txt");
        CHECK(fenster.diagramme().contains("plan"));
        CHECK(fenster.plaene() == QStringList{"plan.txt"});
        REQUIRE(fenster.zeigePlan());
        CHECK(fenster.diagramm() == "plan");
        CHECK(fenster.planKnoten() == QStringList{"Sort (4)", "Exchange (3)", "ColumnarToRow (2)", "Scan parquet (1)"});
        CHECK(fenster.vorschauFormen() == 4);
        REQUIRE(fenster.klicke("Exchange (3)"));
        CHECK(fenster.gezeigteStelle() == "plan.txt:3");
        CHECK_FALSE(fenster.zeigePlan("plan.txt", "optimized"));  // nur der physische
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().contains("partitions: 200"));
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().contains("Physical plan is not available in csv"));

        // ein Notebook: der Plan gehoert zum explain() seiner Zelle, ein Klick zeigt es; extended mit logischen
        fenster.zeige(QStringLiteral(R"nb({"nbformat": 4, "nbformat_minor": 5,
            "metadata": {"language_info": {"name": "python"}},
            "cells": [{"cell_type": "code", "metadata": {}, "source": ["df = spark.read.parquet(\"x\")\n", "df.explain(True)"],
                       "outputs": [{"output_type": "stream", "name": "stdout", "text": ["== Optimized Logical Plan ==\n",
                       "Relation [a#1] parquet\n", "\n", "== Physical Plan ==\n", "*(1) ColumnarToRow\n",
                       "+- FileScan parquet [a#1] Batched: true\n"]}]}]})nb"),
                      QStringLiteral("n.ipynb"));
        REQUIRE(fenster.plaene() == QStringList{"<cell 1>@3"});
        REQUIRE(fenster.zeigePlan("<cell 1>@3"));
        CHECK(fenster.planKnoten() == QStringList{"ColumnarToRow", "FileScan parquet"});
        REQUIRE(fenster.klicke("FileScan parquet"));
        CHECK(fenster.gezeigteStelle() == "n.ipynb:3 (cell 1, line 2)");
        REQUIRE(fenster.zeigePlan("<cell 1>@3", "optimized"));
        CHECK(fenster.planKnoten() == QStringList{"Relation"});
        REQUIRE(fenster.zurueck());
        CHECK(fenster.planKnoten() == QStringList{"ColumnarToRow", "FileScan parquet"});
        // im Datenfluss traegt der Knoten vor dem explain() die Zeile plan:
        REQUIRE(fenster.zeigeDatenfluss());
        REQUIRE(fenster.waehleFormat("svg"));
        CHECK(fenster.diagrammText().contains("plan: &lt;cell 1&gt;@3"));
    }

    TEST_CASE("Run timeline: ein Event-Log ohne Endung, Jobs, Stages, Tasks je Kern, Klick zeigt das Ereignis, Formate")
    {
        Hauptfenster fenster;
        fenster.zeige(
            R"({"Event":"SparkListenerLogStart","Spark Version":"3.5.1"})" "\n"                                             // 1
            R"({"Event":"SparkListenerApplicationStart","App Name":"lager","App ID":"local-1","Timestamp":1000})" "\n"     // 2
            R"({"Event":"SparkListenerExecutorAdded","Timestamp":1010,"Executor ID":"1","Executor Info":{"Host":"a","Total Cores":2}})" "\n"  // 3
            R"({"Event":"SparkListenerJobStart","Job ID":0,"Submission Time":1100,"Stage IDs":[0]})" "\n"                  // 4
            R"({"Event":"SparkListenerStageCompleted","Stage Info":{"Stage ID":0,"Stage Attempt ID":0,"Stage Name":"count at lager.py:12","Number of Tasks":2,"Submission Time":1100,"Completion Time":1600}})" "\n"  // 5
            R"({"Event":"SparkListenerTaskEnd","Stage ID":0,"Stage Attempt ID":0,"Task Info":{"Task ID":0,"Attempt":0,"Executor ID":"1","Launch Time":1100,"Finish Time":1500},"Task End Reason":{"Reason":"Success"}})" "\n"  // 6
            R"({"Event":"SparkListenerTaskEnd","Stage ID":0,"Stage Attempt ID":0,"Task Info":{"Task ID":1,"Attempt":0,"Executor ID":"1","Launch Time":1100,"Finish Time":1600},"Task End Reason":{"Reason":"Success"}})" "\n"  // 7
            R"({"Event":"SparkListenerJobEnd","Job ID":0,"Completion Time":1610,"Job Result":{"Result":"JobSucceeded"}})" "\n"  // 8
            R"({"Event":"SparkListenerApplicationEnd","Timestamp":2000})" "\n",                                             // 9
            "local-1");
        CHECK(fenster.diagramme().contains("run"));
        REQUIRE(fenster.zeigeLauf());
        CHECK(fenster.diagramm() == "run");
        CHECK(fenster.laufBalken() == QStringList{"Job 0", "Stage 0: count at lager.py:12", "0", "1"});
        CHECK(fenster.vorschauFormen() == 4);
        REQUIRE(fenster.klicke("Stage 0: count at lager.py:12"));
        CHECK(fenster.gezeigteStelle() == "local-1:5");
        REQUIRE(fenster.klicke("1"));
        CHECK(fenster.gezeigteStelle() == "local-1:7");
        REQUIRE(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().contains("section executor 1 core 2"));
        REQUIRE(fenster.waehleFormat("csv"));
        CHECK(fenster.diagrammText().contains("task;attempt;stage;executor;core"));
        REQUIRE(fenster.waehleFormat("dot"));
        CHECK(fenster.diagrammText().contains("Run timeline is not available in dot"));
    }

    TEST_CASE("Run timeline und Code (L5, L6): Klick auf einen Job zeigt die Codezeile, Doppelklick seinen Plan, aus dem Datenfluss zum Job")
    {
        QTemporaryDir ordner;
        REQUIRE(ordner.isValid());
        const QString py = QDir::toNativeSeparators(QDir(ordner.path()).filePath(QStringLiteral("lager.py")));
        {
            QFile f(py);
            REQUIRE(f.open(QIODevice::WriteOnly));
            f.write("from pyspark.sql import SparkSession\n"              // 1
                    "spark = SparkSession.builder.getOrCreate()\n"        // 2
                    "df = spark.range(10)\n"                              // 3
                    "erste = df.take(1)\n"                                // 4
                    "df.show()\n"                                         // 5
                    "rest = df.take(2)\n");                               // 6
        }
        // wie Spark 4.3 schreibt: take mit Stelle, show (showString) ohne; je Ausfuehrung ein Plan
        QString pfadJson = py;
        pfadJson.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        const auto ausfuehrung = [&](int id, const QString& beschreibung, int job) {
            return QStringLiteral(
                       R"({"Event":"org.apache.spark.sql.execution.ui.SparkListenerSQLExecutionStart","executionId":%1,"rootExecutionId":%1,"description":"%2","physicalPlanDescription":"== Physical Plan ==\n*(1) Range (0, 10, step=1, splits=2)\n","time":%3})"
                       "\n"
                       R"({"Event":"SparkListenerJobStart","Job ID":%4,"Submission Time":%3,"Stage IDs":[%4],"Properties":{"spark.sql.execution.id":"%1"}})"
                       "\n"
                       R"({"Event":"SparkListenerStageCompleted","Stage Info":{"Stage ID":%4,"Stage Attempt ID":0,"Stage Name":"x at NativeMethodAccessorImpl.java:0","Number of Tasks":1,"Submission Time":%3,"Completion Time":%5}})"
                       "\n"
                       R"({"Event":"SparkListenerJobEnd","Job ID":%4,"Completion Time":%5,"Job Result":{"Result":"JobSucceeded"}})"
                       "\n")
                .arg(id)
                .arg(beschreibung)
                .arg(1000 + 100 * id)
                .arg(job)
                .arg(1050 + 100 * id);
        };
        {
            QFile f(QDir(ordner.path()).filePath(QStringLiteral("local-5")));
            REQUIRE(f.open(QIODevice::WriteOnly));
            f.write(R"({"Event":"SparkListenerLogStart","Spark Version":"4.3.0"})" "\n");
            f.write(R"({"Event":"SparkListenerApplicationStart","App Name":"lager","App ID":"local-5","Timestamp":900})" "\n");
            f.write(ausfuehrung(0, QStringLiteral("take at ") + pfadJson + QStringLiteral(":4"), 0).toUtf8());
            f.write(ausfuehrung(1, QStringLiteral("showString at NativeMethodAccessorImpl.java:0"), 1).toUtf8());
            f.write(ausfuehrung(2, QStringLiteral("take at ") + pfadJson + QStringLiteral(":6"), 2).toUtf8());
            f.write(R"({"Event":"SparkListenerApplicationEnd","Timestamp":2000})" "\n");
        }
        Hauptfenster fenster;
        REQUIRE(fenster.oeffneOrdner(ordner.path()));
        REQUIRE(fenster.waehleDatei(QStringLiteral("local-5")));
        CHECK(fenster.plaene() == QStringList{"local-5@execution 0", "local-5@execution 1", "local-5@execution 2"});
        REQUIRE(fenster.zeigeLauf());
        CHECK(fenster.laufBalken().contains("Job 1"));
        // Klick auf Job 1 (show, ueber den Namen zwischen den beiden take): die Zeile in lager.py
        REQUIRE(fenster.klicke(QStringLiteral("Job 1")));
        CHECK(fenster.gezeigteStelle() == "lager.py:5");
        // Doppelklick: sein Plan (L6), Back zurueck
        REQUIRE(fenster.doppelklicke(QStringLiteral("Job 1")));
        CHECK(fenster.diagramm() == "plan");
        CHECK(fenster.planKnoten() == QStringList{"Range"});
        REQUIRE(fenster.zurueck());
        CHECK(fenster.diagramm() == "run");
        // aus dem Datenfluss: der Knoten der Aktion fuehrt zu ihrem Job
        REQUIRE(fenster.waehleDatei(QStringLiteral("lager.py")));
        REQUIRE(fenster.zeigeDatenfluss());
        const QStringList knoten = fenster.datenflussKnoten();
        const auto show = std::find_if(knoten.begin(), knoten.end(), [](const QString& k) { return k.endsWith(QStringLiteral("show()")); });
        REQUIRE(show != knoten.end());
        REQUIRE(fenster.doppelklicke(*show));
        CHECK(fenster.diagramm() == "run");
        CHECK(fenster.datei() == "local-5");
        CHECK(fenster.markiert() == "Job 1");
    }

    TEST_CASE("Chen in der Vorschau: ein Klick auf Ellipse und Raute trifft Kasten und Glied des Strukturdiagramms")
    {
        using namespace c2d::diagramm;
        Strukturdiagramm d = strukturVon(c2d::sprache::sql().lies(kSchule, "s.sql", {}).einheit);
        d.notation = ErNotation::Chen;
        c2d::ui::Vorschau vorschau;
        vorschau.resize(3000, 3000);
        std::size_t kasten = kKeine, eintrag = kKeine;
        vorschau.beiKlickKasten = [&](std::size_t k, std::size_t e) {
            kasten = k;
            eintrag = e;
        };
        vorschau.zeige(d, false);
        const ErDiagramm er = erDiagrammVon(d);
        const ErAnordnung a = ordneAn(er, {});
        const auto klick = [&](const Rechteck& r) {
            kasten = eintrag = kKeine;
            const QPoint p = vorschau.mapFromScene(QPointF(r.x + r.breite / 2, r.y + r.hoehe / 2));
            QMouseEvent drueck(QEvent::MouseButtonPress, QPointF(p), vorschau.viewport()->mapToGlobal(QPointF(p)), Qt::LeftButton,
                               Qt::LeftButton, Qt::NoModifier);
            QMouseEvent los(QEvent::MouseButtonRelease, QPointF(p), vorschau.viewport()->mapToGlobal(QPointF(p)), Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(vorschau.viewport(), &drueck);
            QApplication::sendEvent(vorschau.viewport(), &los);
        };
        // die Ellipse `note` der Raute belegt (Kasten 3, Glied 2)
        bool gefunden = false;
        for (const ErAnordnung::Ellipse& el : a.ellipsen)
            if (er.knoten[el.knoten].attribute[el.attribut].name == "note")
            {
                klick(el.ort);
                gefunden = true;
            }
        REQUIRE(gefunden);
        CHECK(kasten == 3);
        CHECK(eintrag == 2);
        // die Raute von kurs.lehrer_id: Kasten kurs, Glied lehrer_id
        for (std::size_t v = 0; v < er.knoten.size(); ++v)
            if (er.knoten[v].name == "lehrer_id") klick(a.knoten[v]);
        CHECK(kasten == 1);
        CHECK(eintrag == 1);
        // eine Entitaet: ihr Kopf
        klick(a.knoten[0]);
        CHECK(kasten == 0);
        CHECK(eintrag == kKeine);
    }
}
