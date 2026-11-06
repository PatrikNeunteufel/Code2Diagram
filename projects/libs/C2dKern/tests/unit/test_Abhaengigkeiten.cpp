/**
 * @file test_Abhaengigkeiten.cpp
 * @brief Die Abhaengigkeiten der Daten (Entscheide A1–A4): Knoten, Kanten, Ausschnitte, Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/projekt/Abhaengigkeiten.hpp"
#include "c2d/projekt/Projekt.hpp"

#include "TempOrdner.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;
using diagramm::AufrufKnotenArt;
using diagramm::ZugriffKante;

namespace
{

constexpr std::string_view kSchema =
    "CREATE TABLE lieferant (id INTEGER PRIMARY KEY);\n"
    "CREATE TABLE artikel (id INTEGER PRIMARY KEY, lieferant INTEGER REFERENCES lieferant(id));\n"
    "CREATE TABLE bestand (artikel INTEGER, menge INTEGER);\n"
    "CREATE TABLE protokoll (t TEXT);\n"
    "CREATE VIEW knapp AS SELECT a.id FROM artikel a JOIN bestand b ON b.artikel = a.id WHERE b.menge < 5;\n"
    "CREATE TRIGGER merke AFTER UPDATE ON bestand BEGIN INSERT INTO protokoll VALUES ('x'); END;\n"
    "CREATE TRIGGER merke AFTER INSERT ON artikel BEGIN INSERT INTO protokoll VALUES ('y'); END;\n"
    "CREATE PROCEDURE auffuellen() BEGIN UPDATE bestand SET menge = menge + 1; SELECT count(*) FROM bestand;"
    " CALL schreibe_log(); CALL extern_proc(); END;\n"
    "CREATE PROCEDURE schreibe_log() BEGIN INSERT INTO protokoll VALUES ('z'); END;\n";

std::size_t knoten(const diagramm::Aufrufgraph& g, AufrufKnotenArt art, std::string_view name)
{
    for (std::size_t k = 0; k < g.knoten.size(); ++k)
        if (g.knoten[k].art == art && g.knoten[k].name == name) return k;
    return projekt::kKeine;
}

std::size_t zahl(const diagramm::Aufrufgraph& g, AufrufKnotenArt art)
{
    return static_cast<std::size_t>(
        std::count_if(g.knoten.begin(), g.knoten.end(), [&](const diagramm::AufrufKnoten& k) { return k.art == art; }));
}

const diagramm::AufrufKante* kante(const diagramm::Aufrufgraph& g, std::size_t von, std::size_t nach)
{
    for (const diagramm::AufrufKante& k : g.kanten)
        if (k.von == von && k.nach == nach) return &k;
    return nullptr;
}

}  // namespace

TEST_SUITE("Abhaengigkeiten")
{
    TEST_CASE("Eine Datei: Tabellen, View liest, Trigger je Tabelle, Prozedur schreibt und ruft")
    {
        const projekt::Projekt p = projekt::liesText(kSchema, "schema.sql");
        const projekt::AbhaengigkeitsErgebnis e = projekt::abhaengigkeitenDerDatei(p, 0);
        const diagramm::Aufrufgraph& g = e.graph;
        CHECK(zahl(g, AufrufKnotenArt::Tabelle) == 4);
        CHECK(zahl(g, AufrufKnotenArt::Trigger) == 2);  // `merke` an zwei Tabellen: zwei Trigger
        CHECK(zahl(g, AufrufKnotenArt::Routine) == 2);
        const std::size_t knapp = knoten(g, AufrufKnotenArt::View, "knapp");
        const std::size_t artikel = knoten(g, AufrufKnotenArt::Tabelle, "artikel");
        const std::size_t bestand = knoten(g, AufrufKnotenArt::Tabelle, "bestand");
        const std::size_t protokoll = knoten(g, AufrufKnotenArt::Tabelle, "protokoll");
        REQUIRE(knapp != projekt::kKeine);
        REQUIRE(kante(g, knapp, artikel) != nullptr);
        CHECK(kante(g, knapp, artikel)->zugriff == ZugriffKante::Liest);
        CHECK(kante(g, knapp, bestand) != nullptr);
        CHECK(g.knoten[knapp].stereotyp == "\xC2\xAB" "view" "\xC2\xBB");
        CHECK(g.knoten[artikel].zeile == 2);

        // der Trigger: von seiner Tabelle zu ihm, mit dem Ereignis; er schreibt ins Protokoll
        std::size_t anBestand = projekt::kKeine;
        for (const diagramm::AufrufKante& k : g.kanten)
            if (k.von == bestand && k.zugriff == ZugriffKante::LoestAus) anBestand = k.nach;
        REQUIRE(anBestand != projekt::kKeine);
        CHECK(g.knoten[anBestand].name == "merke");
        CHECK(kante(g, bestand, anBestand)->ereignis == "AFTER UPDATE");
        CHECK(kante(g, anBestand, protokoll)->zugriff == ZugriffKante::Schreibt);
        CHECK(diagramm::beschriftung(*kante(g, bestand, anBestand)) == "AFTER UPDATE");

        // die Prozedur: liest und schreibt bestand (eine Kante), ruft schreibe_log, extern_proc ist fremd; count( nicht
        const std::size_t auffuellen = knoten(g, AufrufKnotenArt::Routine, "auffuellen");
        REQUIRE(kante(g, auffuellen, bestand) != nullptr);
        CHECK(kante(g, auffuellen, bestand)->zugriff == ZugriffKante::LiestSchreibt);
        CHECK(diagramm::beschriftung(*kante(g, auffuellen, bestand)) == "reads, writes");
        CHECK(kante(g, auffuellen, knoten(g, AufrufKnotenArt::Routine, "schreibe_log")) != nullptr);
        CHECK(knoten(g, AufrufKnotenArt::Fremd, "extern_proc") != projekt::kKeine);
        CHECK(knoten(g, AufrufKnotenArt::Fremd, "count") == projekt::kKeine);
        CHECK_FALSE(g.mitRahmen());  // eine Datei

        // datenobjekteIn: Tabellen, Objekte, Trigger mit ihrer Tabelle
        const std::vector<std::string> namen = projekt::datenobjekteIn(p, 0);
        CHECK(std::find(namen.begin(), namen.end(), "merke@bestand") != namen.end());
        CHECK(std::find(namen.begin(), namen.end(), "merke@artikel") != namen.end());
        CHECK(std::find(namen.begin(), namen.end(), "knapp") != namen.end());
    }

    TEST_CASE("Ab einem Objekt: was es liest/schreibt, wer es liest/schreibt, Tiefe und weiter")
    {
        const projekt::Projekt p = projekt::liesText(kSchema, "schema.sql");
        projekt::AbhaengigkeitsOptionen o;
        o.tiefe = 1;
        const projekt::AbhaengigkeitsErgebnis ab = projekt::abhaengigkeitenAb(p, 0, "auffuellen", o);
        const diagramm::Aufrufgraph& g = ab.graph;
        const std::size_t auffuellen = knoten(g, AufrufKnotenArt::Routine, "auffuellen");
        REQUIRE(auffuellen != projekt::kKeine);
        CHECK(g.knoten[auffuellen].wurzel);
        CHECK(knoten(g, AufrufKnotenArt::Tabelle, "bestand") != projekt::kKeine);
        const std::size_t log = knoten(g, AufrufKnotenArt::Routine, "schreibe_log");
        REQUIRE(log != projekt::kKeine);
        CHECK(g.knoten[log].weiter);  // schreibt ins Protokoll — eine Stufe weiter
        CHECK(knoten(g, AufrufKnotenArt::Tabelle, "protokoll") == projekt::kKeine);

        o.richtung = projekt::AufrufRichtung::Aufrufer;
        o.tiefe = 0;
        const projekt::AbhaengigkeitsErgebnis wer = projekt::abhaengigkeitenAb(p, 0, "protokoll", o);
        CHECK(zahl(wer.graph, AufrufKnotenArt::Trigger) == 2);
        CHECK(knoten(wer.graph, AufrufKnotenArt::Routine, "schreibe_log") != projekt::kKeine);
        CHECK(knoten(wer.graph, AufrufKnotenArt::Routine, "auffuellen") != projekt::kKeine);  // ruft schreibe_log
        CHECK(projekt::abhaengigkeitenAb(p, 0, "gibt_es_nicht").graph.knoten.empty());

        // ein Trigger: nur, wo seine Tabelle geschrieben wird — wer sie liest, loest ihn nicht aus
        o.richtung = projekt::AufrufRichtung::Gerufene;
        const diagramm::Aufrufgraph schreibt = projekt::abhaengigkeitenAb(p, 0, "auffuellen", o).graph;
        CHECK(zahl(schreibt, AufrufKnotenArt::Trigger) == 1);  // merke an bestand
        const diagramm::Aufrufgraph liest = projekt::abhaengigkeitenAb(p, 0, "knapp", o).graph;
        CHECK(zahl(liest, AufrufKnotenArt::Trigger) == 0);
        CHECK(zahl(projekt::abhaengigkeitenAb(p, 0, "bestand", o).graph, AufrufKnotenArt::Trigger) == 1);
    }

    TEST_CASE("Ueber das Projekt: Funktionen des Host-Codes, fremde Tabellen nur aus klarer Form, Rahmen je Datei")
    {
        const TempOrdner t;
        t.lege("schema.sql", kSchema);
        t.lege("app.py", "import sqlite3\n"
                         "def lade(c):\n"
                         "    c.execute(\"SELECT * FROM artikel JOIN lager_extern l ON l.id = 1\")\n"
                         "    sql = \"SELECT * FROM bestand JOIN irgendwo i ON i.x = 1\"\n"
                         "    c.execute(sql)\n");
        const projekt::Projekt p = projekt::liesOrdner(t.weg());
        REQUIRE(p.dateien.size() == 2);
        const std::size_t app = p.dateien[0].pfad == "app.py" ? 0 : 1;
        const projekt::AbhaengigkeitsErgebnis e = projekt::abhaengigkeitenDerDatei(p, app);
        const diagramm::Aufrufgraph& g = e.graph;
        const std::size_t lade = knoten(g, AufrufKnotenArt::Funktion, "lade");
        REQUIRE(lade != projekt::kKeine);
        CHECK(kante(g, lade, knoten(g, AufrufKnotenArt::Tabelle, "artikel")) != nullptr);
        CHECK(kante(g, lade, knoten(g, AufrufKnotenArt::Tabelle, "bestand")) != nullptr);  // unsicher, aber bekannt
        CHECK(knoten(g, AufrufKnotenArt::FremdeTabelle, "lager_extern") != projekt::kKeine);  // klare Form
        CHECK(knoten(g, AufrufKnotenArt::FremdeTabelle, "irgendwo") == projekt::kKeine);      // unsicher und fremd
        CHECK(g.mitRahmen());
        REQUIRE(e.stellen.size() == g.knoten.size());
        CHECK(e.stellen[lade].art == projekt::Datenstelle::Art::Funktion);
        CHECK(e.stellen[knoten(g, AufrufKnotenArt::FremdeTabelle, "lager_extern")].datei == projekt::kKeine);
    }

    TEST_CASE("Anweisungen oben in einer .sql (C2): je eine mit Ziel ein Knoten query@zeile, reine Abfragen nicht")
    {
        const TempOrdner t;
        t.lege("schema.sql", kSchema);
        t.lege("laden.sql", "CREATE TABLE knapp_kopie AS SELECT * FROM knapp;\n"
                            "INSERT INTO protokoll SELECT a.id FROM artikel a;\n"
                            "SELECT * FROM bestand;\n"
                            "INSERT INTO protokoll VALUES ('ohne Quelle');\n"
                            "UPDATE bestand SET menge = 0 FROM artikel a WHERE a.id = bestand.artikel;\n");
        const projekt::Projekt p = projekt::liesOrdner(t.weg());
        REQUIRE(p.dateien.size() == 2);
        const std::size_t laden = p.dateien[0].pfad == "laden.sql" ? 0 : 1;
        const projekt::AbhaengigkeitsErgebnis e = projekt::abhaengigkeitenDerDatei(p, laden);
        const diagramm::Aufrufgraph& g = e.graph;

        const std::size_t ctas = knoten(g, AufrufKnotenArt::Routine, "query@1");
        const std::size_t insert = knoten(g, AufrufKnotenArt::Routine, "query@2");
        const std::size_t update = knoten(g, AufrufKnotenArt::Routine, "query@5");
        REQUIRE(ctas != projekt::kKeine);
        REQUIRE(insert != projekt::kKeine);
        REQUIRE(update != projekt::kKeine);
        CHECK(knoten(g, AufrufKnotenArt::Routine, "query@3") == projekt::kKeine);  // liest nur: steht in Queries
        CHECK(knoten(g, AufrufKnotenArt::Routine, "query@4") == projekt::kKeine);  // INSERT … VALUES: keine Abfrage
        CHECK(g.knoten[ctas].stereotyp == "\xC2\xAB" "statement" "\xC2\xBB");
        CHECK(g.knoten[ctas].zeile == 1);

        // je Anweisung ihre eigenen Kanten: die Kette View -> Kopie bleibt getrennt vom INSERT
        CHECK(kante(g, ctas, knoten(g, AufrufKnotenArt::View, "knapp"))->zugriff == ZugriffKante::Liest);
        CHECK(kante(g, ctas, knoten(g, AufrufKnotenArt::Tabelle, "knapp_kopie"))->zugriff == ZugriffKante::Schreibt);
        CHECK(kante(g, ctas, knoten(g, AufrufKnotenArt::Tabelle, "artikel")) == nullptr);
        CHECK(kante(g, insert, knoten(g, AufrufKnotenArt::Tabelle, "artikel"))->zugriff == ZugriffKante::Liest);
        CHECK(kante(g, insert, knoten(g, AufrufKnotenArt::Tabelle, "protokoll"))->zugriff == ZugriffKante::Schreibt);
        CHECK(kante(g, update, knoten(g, AufrufKnotenArt::Tabelle, "bestand"))->zugriff == ZugriffKante::Schreibt);

        REQUIRE(e.stellen.size() == g.knoten.size());
        CHECK(e.stellen[insert].art == projekt::Datenstelle::Art::Abfrage);
        CHECK(p.dateien[laden].ergebnis.einheit.abfragen[e.stellen[insert].index].name == "query@2");

        // ab der Anweisung, und unter den Objekten der Datei
        const projekt::AbhaengigkeitsErgebnis ab = projekt::abhaengigkeitenAb(p, laden, "query@2", {});
        CHECK(ab.graph.knoten[knoten(ab.graph, AufrufKnotenArt::Routine, "query@2")].wurzel);
        const std::vector<std::string> namen = projekt::datenobjekteIn(p, laden);
        CHECK(std::find(namen.begin(), namen.end(), "query@1") != namen.end());
        CHECK(std::find(namen.begin(), namen.end(), "query@3") == namen.end());

        // T-SQL ohne `;` (WWI PostDeploymentScripts): der Leser sieht eine Anweisung bis GO, jede Abfrage
        // darin traegt nur ihre eigenen Zugriffe
        const projekt::Projekt w = projekt::liesText("DECLARE @t datetime2(7) = '20200101'\n"
                                                     "INSERT dbo.a (x) SELECT q.x FROM dbo.q1 AS q\n"
                                                     "INSERT dbo.b (x) SELECT q.x FROM dbo.q2 AS q\n"
                                                     "GO\n",
                                                     "w.sql");
        const std::vector<modell::Abfrage>& q = w.dateien[0].ergebnis.einheit.abfragen;
        REQUIRE(q.size() == 2);
        REQUIRE(q[0].zugriffe.size() == 2);
        CHECK(q[0].zugriffe[0].ziel == "dbo.a");
        CHECK(q[0].zugriffe[1].ziel == "dbo.q1");
        REQUIRE(q[1].zugriffe.size() == 2);
        CHECK(q[1].zugriffe[0].ziel == "dbo.b");
        CHECK(q[1].zugriffe[1].ziel == "dbo.q2");
    }

    TEST_CASE("Dasselbe Schema in zwei Dateien (je Dialekt): jede Datei zeigt es mit ihrer Stelle")
    {
        const TempOrdner t;
        t.lege("a_sqlite.sql", "CREATE TABLE album (id INTEGER);\n");
        t.lege("b_postgres.sql", "\n\nCREATE TABLE album (id INTEGER);\nCREATE VIEW v AS SELECT * FROM album;\n");
        const projekt::Projekt p = projekt::liesOrdner(t.weg());
        REQUIRE(p.dateien.size() == 2);
        const projekt::AbhaengigkeitsErgebnis a = projekt::abhaengigkeitenDerDatei(p, 0);
        const projekt::AbhaengigkeitsErgebnis b = projekt::abhaengigkeitenDerDatei(p, 1);
        REQUIRE(a.graph.knoten.size() == 2);  // album, und die View der anderen Datei, die es liest
        CHECK(a.graph.knoten[0].name == "album");
        CHECK(a.graph.knoten[0].zeile == 1);
        CHECK(a.stellen[0].datei == 0);
        const std::size_t album = knoten(b.graph, AufrufKnotenArt::Tabelle, "album");
        REQUIRE(album != projekt::kKeine);
        CHECK(b.graph.knoten[album].zeile == 3);
        CHECK(b.stellen[album].datei == 1);
    }

    TEST_CASE("Formate: Formen und Stereotypen der Abhaengigkeiten; csv sagt ab")
    {
        const projekt::Projekt p = projekt::liesText(kSchema, "schema.sql");
        const diagramm::Aufrufgraph g = projekt::abhaengigkeitenDerDatei(p, 0).graph;
        const std::string mermaid = ausgabe::format("mermaid")->schreibe(g, {});
        CHECK(mermaid.find("[(\"\xC2\xAB" "table" "\xC2\xBB<br/>artikel\")]") != std::string::npos);
        CHECK(mermaid.find("{{\"\xC2\xAB" "trigger" "\xC2\xBB<br/>merke\"}}") != std::string::npos);
        CHECK(mermaid.find("|\"reads, writes\"|") != std::string::npos);
        const std::string dot = ausgabe::format("dot")->schreibe(g, {});
        CHECK(dot.find("\\nknapp") != std::string::npos);
        CHECK(dot.find("style=rounded") != std::string::npos);
        for (const std::string_view f : {"svg", "drawio"})
        {
            const std::string text = ausgabe::format(f)->schreibe(g, {});
            CHECK_MESSAGE(text.find("procedure") != std::string::npos, f);
            CHECK_MESSAGE(text.find("AFTER UPDATE") != std::string::npos, f);
        }
        CHECK_FALSE(ausgabe::format("csv")->kann(ausgabe::Diagrammart::Abhaengigkeiten));
        CHECK(ausgabe::nameVon(ausgabe::Diagrammart::Abhaengigkeiten) == "Data dependencies");
    }
}
