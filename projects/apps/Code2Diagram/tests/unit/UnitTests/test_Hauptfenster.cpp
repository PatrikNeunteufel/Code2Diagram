/**
 * @file test_Hauptfenster.cpp
 * @brief Das Fenster: lesen, waehlen, Text des Diagramms — ohne es zu zeigen
 */
#include <doctest.h>

#include "ui/Hauptfenster.hpp"
#include "ui/Konfiguration.hpp"

#include <QDir>
#include <QFile>
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
        CHECK(fenster.diagramme() == QStringList{"b", "immer", "calls"});
        CHECK(fenster.unbekannteNamen() == QStringList{"BOARD_A", "STUFE"});
        CHECK(fenster.waehleDiagramm("immer"));
        CHECK_FALSE(fenster.diagrammText().contains("laut"));

        fenster.setzeDefinitionen("BOARD_A, STUFE=2");
        CHECK(fenster.diagramme() == QStringList{"a", "immer", "calls"});
        CHECK(fenster.unbekannteNamen().isEmpty());
        CHECK(fenster.diagramm() == "immer");  // das gewaehlte Diagramm bleibt
        CHECK(fenster.diagrammText().contains("laut"));

        fenster.setzeAlleZweige(true);
        CHECK(fenster.diagramme() == QStringList{"a", "b", "immer", "calls"});
        fenster.setzeAlleZweige(false);
        fenster.setzeDefinitionen("");
        CHECK(fenster.diagramme() == QStringList{"b", "immer", "calls"});
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
        CHECK(fenster.diagramme() == QStringList{"pruefe", "motor_starte", "motor_zwei", "calls"});  // MOTOREN aus dem Abschnitt
        CHECK(fenster.windowTitle().contains("antrieb/motor.c"));

        REQUIRE(fenster.waehleDatei("main.c"));
        CHECK_FALSE(fenster.waehleDatei("alt/x.c"));
        CHECK(fenster.diagramme() == QStringList{"pruefe", "main", "diagnose", "calls"});
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
        CHECK(fenster.diagramme() == QStringList{"pruefe", "motor_starte", "calls"});
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

    TEST_CASE("Der Dateifilter nennt die Endungen der Sprachen des Kerns")
    {
        CHECK(Hauptfenster::dateiFilter() ==
              "C files (*.c *.h);;C++ files (*.cpp *.cc *.cxx *.hpp *.hh *.hxx *.ipp *.inl *.tpp *.h);;All files (*)");
    }
}
