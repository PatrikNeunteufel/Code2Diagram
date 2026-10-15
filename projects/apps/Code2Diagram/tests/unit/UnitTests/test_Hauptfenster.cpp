/**
 * @file test_Hauptfenster.cpp
 * @brief Das Fenster: lesen, waehlen, Text des Diagramms — ohne es zu zeigen
 */
#include <doctest.h>

#include "ui/Hauptfenster.hpp"

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
        CHECK(fenster.diagramme() == QStringList{"dauer_ms", "ampel_takt", "types"});
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
        CHECK(fenster.diagramme() == QStringList{"n::A::f", "n::A::g", "types"});
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
        CHECK(fenster.diagramme() == QStringList{"b", "immer"});
        CHECK(fenster.unbekannteNamen() == QStringList{"BOARD_A", "STUFE"});
        CHECK(fenster.waehleDiagramm("immer"));
        CHECK_FALSE(fenster.diagrammText().contains("laut"));

        fenster.setzeDefinitionen("BOARD_A, STUFE=2");
        CHECK(fenster.diagramme() == QStringList{"a", "immer"});
        CHECK(fenster.unbekannteNamen().isEmpty());
        CHECK(fenster.diagramm() == "immer");  // das gewaehlte Diagramm bleibt
        CHECK(fenster.diagrammText().contains("laut"));

        fenster.setzeAlleZweige(true);
        CHECK(fenster.diagramme() == QStringList{"a", "b", "immer"});
        fenster.setzeAlleZweige(false);
        fenster.setzeDefinitionen("");
        CHECK(fenster.diagramme() == QStringList{"b", "immer"});
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
        CHECK(fenster.diagramme() == QStringList{"f", "g", "types"});
        CHECK(fenster.meldungen() == 1);  // das fehlende Semikolon in g

        CHECK_FALSE(fenster.oeffne(QDir(ordner.path()).filePath("fehlt.c"), &fehler));
        CHECK(fehler.contains("fehlt.c"));
        CHECK(fenster.diagramme().size() == 3);
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
        CHECK(fenster.diagramme() == QStringList{"pruefe", "motor_starte", "motor_zwei"});  // MOTOREN aus dem Abschnitt
        CHECK(fenster.windowTitle().contains("antrieb/motor.c"));

        REQUIRE(fenster.waehleDatei("main.c"));
        CHECK_FALSE(fenster.waehleDatei("alt/x.c"));
        CHECK(fenster.diagramme() == QStringList{"pruefe", "main", "diagnose"});
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
        CHECK(fenster.diagramme() == QStringList{"pruefe", "motor_starte"});
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
        CHECK(fenster.diagramme() == QStringList{"takt", "reset", "automat:z"});
        REQUIRE(fenster.waehleDiagramm("automat:z"));
        CHECK(fenster.vorschauFormen() == 4);  // A, B, C und der Punkt von aussen
        CHECK(fenster.waehleFormat("mermaid"));
        CHECK(fenster.diagrammText().startsWith("%% z in takt()\nstateDiagram-v2\n"));
        CHECK(fenster.diagrammText().contains("[*] --> s0 : in reset()"));
        CHECK(fenster.aufrufer().isEmpty());  // ein Automat ist keine Funktion
    }

    TEST_CASE("Der Dateifilter nennt die Endungen der Sprachen des Kerns")
    {
        CHECK(Hauptfenster::dateiFilter() ==
              "C files (*.c *.h);;C++ files (*.cpp *.cc *.cxx *.hpp *.hh *.hxx *.ipp *.inl *.tpp *.h);;All files (*)");
    }
}
