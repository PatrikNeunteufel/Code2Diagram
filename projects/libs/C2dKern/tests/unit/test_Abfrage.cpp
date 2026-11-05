/**
 * @file test_Abfrage.cpp
 * @brief Abfragen (Entscheide Q1-Q5): der Zerleger im SQL-Leser, das Abfragediagramm (Joins aus ON und
 *        WHERE, Filter, WITH, korrelierte Unterabfragen, Mengen, Ziel), Anordnung, Formate, SQL in
 *        Zeichenketten
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Abfragediagramm.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/projekt/Abhaengigkeiten.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace c2d;
using namespace c2d::diagramm;

namespace
{

constexpr std::string_view kSchema = R"(
CREATE TABLE lehrer (lehrer_id INTEGER PRIMARY KEY, name VARCHAR(60));
CREATE TABLE kurs (kurs_id INTEGER PRIMARY KEY, titel VARCHAR(40), lehrer_id INTEGER REFERENCES lehrer (lehrer_id));
CREATE TABLE schueler (schueler_id INTEGER PRIMARY KEY, name VARCHAR(60), klasse_id INTEGER);
CREATE TABLE belegt (schueler_id INTEGER, kurs_id INTEGER, note INTEGER, PRIMARY KEY (schueler_id, kurs_id));
CREATE TABLE ausweis (ausweis_id INTEGER PRIMARY KEY, schueler_id INTEGER, gueltig_bis DATE);
)";

sprache::Ergebnis lies(std::string_view text, std::string dialekt = "postgres")
{
    sprache::LeseOptionen o;
    o.dialekt = std::move(dialekt);
    return sprache::sql().lies(text, "t.sql", o);
}

/// Schema und Abfrage in einem Text; die Abfragen und ein Diagramm der ersten mit dem Schema des Texts.
struct Gelesen
{
    sprache::Ergebnis e;
    Abfragediagramm d;
};

Gelesen mitSchema(std::string_view abfrage, std::size_t welche = 0)
{
    Gelesen g;
    g.e = lies(std::string(kSchema) + std::string(abfrage));
    REQUIRE(g.e.einheit.abfragen.size() > welche);
    const modell::Einheit& u = g.e.einheit;
    g.d = abfrageVon(u.abfragen[welche], [&u](std::string_view name) {
        Datenquelle q;
        for (const modell::Typ& t : u.typen)
            if (t.name == name) q.tabelle = &t;
        for (const modell::Datenbankobjekt& o : u.objekte)
            if (o.art == modell::ObjektArt::View && o.name == name) q.view = true;
        return q;
    });
    return g;
}

std::size_t knoten(const Abfragediagramm& d, std::string_view titel)
{
    for (std::size_t i = 0; i < d.knoten.size(); ++i)
        if (d.knoten[i].titel == titel) return i;
    FAIL("no node " << titel);
    return 0;
}

/// Die Kante zwischen zwei Knoten (in einer der Richtungen); nullptr = keine.
const AbfrageKante* kante(const Abfragediagramm& d, std::size_t a, std::size_t b)
{
    for (const AbfrageKante& k : d.kanten)
        if ((k.von == a && k.nach == b) || (k.von == b && k.nach == a)) return &k;
    return nullptr;
}

bool hatZeile(const AbfrageKnoten& k, std::string_view z)
{
    return std::find(k.zeilen.begin(), k.zeilen.end(), z) != k.zeilen.end();
}

}  // namespace

TEST_SUITE("Abfrage")
{
    TEST_CASE("Leser: eine View mit Joins — Quellen, Arten, Bedingungen, Ziel, Namen nach aussen")
    {
        const sprache::Ergebnis e = lies(R"(
CREATE VIEW notenliste AS
SELECT s.name AS schueler, k.titel, b.note, l.name lehrer
  FROM schueler s
  JOIN belegt b ON b.schueler_id = s.schueler_id
  INNER JOIN kurs k ON k.kurs_id = b.kurs_id AND k.titel <> ''
  LEFT OUTER JOIN lehrer l USING (lehrer_id)
 WHERE b.note BETWEEN 1 AND 4 AND s.name LIKE 'A%'
 ORDER BY schueler;
)");
        REQUIRE(e.einheit.abfragen.size() == 1);
        const modell::Abfrage& a = e.einheit.abfragen[0];
        CHECK(a.name == "notenliste");
        CHECK(a.ziel == "notenliste");
        CHECK(a.schreibt == "create view");
        CHECK(a.zeile == 2);
        REQUIRE(a.bloecke.size() == 1);
        const modell::AbfrageBlock& b = a.bloecke[0];
        REQUIRE(b.quellen.size() == 4);
        CHECK(b.quellen[0].name == "schueler");
        CHECK(b.quellen[0].alias == "s");
        CHECK(b.quellen[0].verbindung.empty());
        CHECK(b.quellen[1].verbindung == "inner");
        CHECK(b.quellen[2].verbindung == "inner");
        CHECK(b.quellen[3].verbindung == "left");
        CHECK(b.quellen[3].mit == std::vector<std::string>{"lehrer_id"});
        REQUIRE(b.quellen[2].bedingung.size() == 2);  // an AND zerlegt
        CHECK(b.quellen[2].bedingung[0].text == "k.kurs_id = b.kurs_id");
        CHECK(b.quellen[2].bedingung[0].bezuege == std::vector<modell::Spaltenbezug>{{"k", "kurs_id"}, {"b", "kurs_id"}});
        // BETWEEN … AND trennt nicht
        REQUIRE(b.wo.size() == 2);
        CHECK(b.wo[0].text == "b.note BETWEEN 1 AND 4");
        CHECK(b.ausgabe == std::vector<std::string>{"schueler", "titel", "note", "lehrer"});
        CHECK(b.aliase == std::vector<std::string>{"schueler", "lehrer"});
        CHECK(b.ordnung == "schueler");
        CHECK(b.zeile == 3);
        CHECK(b.quellen[1].zeile == 5);
    }

    TEST_CASE("Diagramm: ON und WHERE zwischen zwei Quellen sind Kanten, der Rest Filter; Spalten je Quelle")
    {
        const Gelesen g = mitSchema(R"(
SELECT k.titel, s.name
  FROM kurs k, schueler s
  JOIN belegt b ON b.schueler_id = s.schueler_id AND b.note > 2
 WHERE b.kurs_id = k.kurs_id AND k.titel LIKE 'M%'
 ORDER BY name;
)");
        const Abfragediagramm& d = g.d;
        const std::size_t k = knoten(d, "kurs k");
        const std::size_t s = knoten(d, "schueler s");
        const std::size_t b = knoten(d, "belegt b");
        const std::size_t r = knoten(d, "result");
        REQUIRE(kante(d, s, b) != nullptr);
        CHECK(kante(d, s, b)->art == AbfrageKantenArt::Join);
        CHECK(kante(d, s, b)->text == "inner: b.schueler_id = s.schueler_id");
        REQUIRE(kante(d, k, b) != nullptr);
        CHECK(kante(d, k, b)->art == AbfrageKantenArt::Wo);  // aus WHERE: gestrichelt
        CHECK(kante(d, k, b)->text == "where: b.kurs_id = k.kurs_id");
        CHECK(kante(d, k, s) == nullptr);  // ein Komma allein ist keine Kante
        CHECK(hatZeile(d.knoten[b], "on: b.note > 2"));
        CHECK(hatZeile(d.knoten[r], "where k.titel LIKE 'M%'"));
        CHECK(hatZeile(d.knoten[k], "titel"));
        CHECK(hatZeile(d.knoten[b], "note"));
        // ohne Alias ueber das Schema: `name` hat nur schueler
        CHECK(hatZeile(d.knoten[s], "name"));
        // wer keine Kante weitergibt, fuehrt ins Ergebnis
        CHECK(kante(d, b, r) != nullptr);
        CHECK(kante(d, b, r)->art == AbfrageKantenArt::Fluss);
        CHECK(d.knoten[k].art == AbfrageKnotenArt::Tabelle);
    }

    TEST_CASE("Diagramm: WITH, eine korrelierte Unterabfrage in EXISTS, ein WHERE an einer Join-Kante")
    {
        const Gelesen g = mitSchema(R"(
WITH schnitt AS (SELECT kurs_id, avg(note) AS mittel FROM belegt GROUP BY kurs_id)
SELECT s.name
  FROM schueler s
  JOIN belegt b ON b.schueler_id = s.schueler_id
  JOIN schnitt x ON x.kurs_id = b.kurs_id
 WHERE b.note < x.mittel
   AND EXISTS (SELECT 1 FROM ausweis a WHERE a.schueler_id = s.schueler_id);
)");
        const modell::Abfrage& a = g.e.einheit.abfragen[0];
        REQUIRE(a.bloecke.size() == 3);
        CHECK(a.bloecke[1].rolle == modell::BlockRolle::Mit);
        CHECK(a.bloecke[1].titel == "schnitt");
        CHECK(a.bloecke[2].rolle == modell::BlockRolle::Bedingung);
        CHECK(a.bloecke[2].titel == "exists");
        const Abfragediagramm& d = g.d;
        REQUIRE(d.rahmen.size() == 2);
        CHECK(d.rahmen[0].titel == "WITH schnitt");
        CHECK(d.rahmen[1].titel == "subquery (EXISTS)");
        const std::size_t x = knoten(d, "schnitt x");
        CHECK(d.knoten[x].art == AbfrageKnotenArt::Mit);
        CHECK(hatZeile(d.knoten[x], "mittel"));
        const std::size_t b = knoten(d, "belegt b");
        REQUIRE(kante(d, b, x) != nullptr);
        CHECK(kante(d, b, x)->text == "inner: x.kurs_id = b.kurs_id; where: b.note < x.mittel");
        const std::size_t s = knoten(d, "schueler s");
        const std::size_t aw = knoten(d, "ausweis a");
        REQUIRE(kante(d, s, aw) != nullptr);
        CHECK(kante(d, s, aw)->art == AbfrageKantenArt::Korreliert);
        CHECK(d.knoten[aw].gruppe == 1);
        // das Ergebnis der Unterabfrage fuehrt ins aeussere, beschriftet exists; das des WITH zu `schnitt x`
        std::size_t inExists = 0, inMit = 0;
        for (const AbfrageKante& k : d.kanten)
        {
            if (k.art == AbfrageKantenArt::Fluss && k.text == "exists") ++inExists;
            if (k.art == AbfrageKantenArt::Fluss && k.nach == x) ++inMit;
        }
        CHECK(inExists == 1);
        CHECK(inMit == 1);
    }

    TEST_CASE("Diagramm: UNION in einer abgeleiteten Tabelle, INSERT … SELECT mit NOT EXISTS")
    {
        const Gelesen u = mitSchema(R"(
SELECT t.name FROM (SELECT name FROM lehrer UNION ALL SELECT name FROM schueler) t ORDER BY t.name;
)");
        const std::size_t menge = knoten(u.d, "UNION ALL");
        CHECK(u.d.knoten[menge].art == AbfrageKnotenArt::Menge);
        CHECK(u.d.knoten[knoten(u.d, "t")].art == AbfrageKnotenArt::Abgeleitet);
        CHECK(u.d.rahmen.size() == 3);  // derived t, branch 1, branch 2
        CHECK(u.d.rahmen[1].titel == "branch 1");
        CHECK(u.d.rahmen[1].eltern == 0);

        const Gelesen i = mitSchema(R"(
INSERT INTO ausweis (ausweis_id, schueler_id)
SELECT s.schueler_id + 1000, s.schueler_id FROM schueler s
 WHERE NOT EXISTS (SELECT 1 FROM ausweis a WHERE a.schueler_id = s.schueler_id);
)");
        const modell::Abfrage& a = i.e.einheit.abfragen[0];
        CHECK(a.ziel == "ausweis");
        CHECK(a.schreibt == "insert into");
        const std::size_t ziel = knoten(i.d, "ausweis");
        CHECK(i.d.knoten[ziel].art == AbfrageKnotenArt::Ziel);
        CHECK(i.d.rahmen[0].titel == "subquery (NOT EXISTS)");
        bool schreibt = false;
        for (const AbfrageKante& k : i.d.kanten) schreibt = schreibt || (k.art == AbfrageKantenArt::Schreibt && k.nach == ziel);
        CHECK(schreibt);
    }

    TEST_CASE("Leser: UPDATE … FROM, DELETE … USING, MERGE … USING, CREATE TABLE … AS; was keine Abfrage ist")
    {
        const sprache::Ergebnis e = lies(R"(
UPDATE kurs SET titel = l.name FROM lehrer l WHERE l.lehrer_id = kurs.lehrer_id;
DELETE FROM belegt b USING schueler s WHERE s.schueler_id = b.schueler_id;
MERGE INTO kurs k USING neu n ON n.kurs_id = k.kurs_id WHEN MATCHED THEN UPDATE SET titel = n.titel;
CREATE TABLE kopie AS SELECT * FROM kurs;
UPDATE kurs SET titel = 'x';
INSERT INTO kurs VALUES (1, 'a', NULL);
SELECT 1;
)");
        const std::vector<modell::Abfrage>& q = e.einheit.abfragen;
        REQUIRE(q.size() == 4);
        CHECK(q[0].schreibt == "update");
        CHECK(q[0].ziel == "kurs");
        CHECK(q[0].bloecke[0].quellen.size() == 2);
        CHECK(q[0].bloecke[0].auswahl == "set titel = l.name");
        CHECK(q[1].schreibt == "delete");
        CHECK(q[1].bloecke[0].quellen[1].name == "schueler");
        CHECK(q[2].schreibt == "merge into");
        CHECK(q[2].bloecke[0].quellen[1].verbindung == "merge");
        CHECK(q[2].bloecke[0].quellen[1].bedingung.size() == 1);
        CHECK(q[3].schreibt == "create table");
        CHECK(q[3].ziel == "kopie");
        CHECK(q[3].name == "query@5");
    }

    TEST_CASE("Leser: Rumpf ohne `;` (T-SQL), SELECT INTO #t, ein Rumpf in $$ (Postgres) mit seinen Zeilen")
    {
        const sprache::Ergebnis t = lies(R"(CREATE PROCEDURE dbo.Zahlen
AS
BEGIN
    SELECT k.titel INTO #t FROM dbo.kurs k
    UPDATE dbo.kurs SET titel = t.titel FROM #t t WHERE t.titel = dbo.kurs.titel
    SELECT @n = count(*) FROM dbo.kurs
END
)",
                                         "tsql");
        const std::vector<modell::Abfrage>& q = t.einheit.abfragen;
        REQUIRE(q.size() == 3);
        CHECK(q[0].name == "dbo.Zahlen@4");
        CHECK(q[0].schreibt == "select into");
        CHECK(q[0].ziel == "#t");
        CHECK(q[1].name == "dbo.Zahlen@5");
        CHECK(q[1].schreibt == "update");
        CHECK(q[2].name == "dbo.Zahlen@6");

        const sprache::Ergebnis p = lies(R"(CREATE FUNCTION zaehle() RETURNS integer LANGUAGE plpgsql AS $$
BEGIN
    RETURN (SELECT count(*) FROM kurs);
    PERFORM 1 FROM schueler s JOIN belegt b ON b.schueler_id = s.schueler_id;
END
$$;
)");
        // RETURN (SELECT …) und PERFORM (ein SELECT ohne Ergebnis), mit den Zeilen der Datei
        REQUIRE(p.einheit.abfragen.size() == 2);
        CHECK(p.einheit.abfragen[0].name == "zaehle@3");
        CHECK(p.einheit.abfragen[0].zeile == 3);
        CHECK(p.einheit.abfragen[1].name == "zaehle@4");
        CHECK(p.einheit.abfragen[1].bloecke[0].quellen.size() == 2);
    }

    TEST_CASE("Diagramm: ein Alias der Liste ist keine Spalte; ohne Schema ist eine Spalte ohne Alias unzugeordnet")
    {
        const sprache::Ergebnis e = lies(R"(
SELECT k.titel, count(*) anzahl, max(note) AS beste
  FROM kurs k JOIN belegt b ON b.kurs_id = k.kurs_id
 GROUP BY k.titel ORDER BY anzahl DESC;
)");
        const Abfragediagramm d = abfrageVon(e.einheit.abfragen[0]);  // ohne Schema
        const AbfrageKnoten& r = d.knoten[knoten(d, "result")];
        CHECK(hatZeile(r, "unassigned: note"));  // `note` koennte zu k oder b gehoeren
        for (const AbfrageKnoten& k : d.knoten)
        {
            CHECK_FALSE(hatZeile(k, "anzahl"));
            CHECK_FALSE(hatZeile(k, "beste"));
        }
        CHECK(hatZeile(r, "group by k.titel"));
        CHECK(hatZeile(r, "order by anzahl DESC"));
        CHECK(d.knoten[knoten(d, "kurs k")].art == AbfrageKnotenArt::Tabelle);  // ohne Schema nicht grau
    }

    TEST_CASE("Anordnung und Formate: Knoten in ihren Rahmen, nichts ueberlappt; SVG, Mermaid, DOT, draw.io, csv")
    {
        const Gelesen g = mitSchema(R"(
WITH schnitt AS (SELECT kurs_id, avg(note) AS mittel FROM belegt GROUP BY kurs_id)
SELECT s.name FROM schueler s JOIN belegt b ON b.schueler_id = s.schueler_id JOIN schnitt x ON x.kurs_id = b.kurs_id
 WHERE EXISTS (SELECT 1 FROM ausweis a WHERE a.schueler_id = s.schueler_id);
)");
        for (const bool nachRechts : {false, true})
        {
            AnordnungOptionen o;
            o.nachRechts = nachRechts;
            const FlussAnordnung a = ordneAn(g.d, o);
            REQUIRE(a.knoten.size() == g.d.knoten.size());
            REQUIRE(a.gruppen.size() == g.d.rahmen.size());
            for (std::size_t i = 0; i < a.knoten.size(); ++i)
            {
                for (std::size_t j = i + 1; j < a.knoten.size(); ++j)
                {
                    const Rechteck& p = a.knoten[i];
                    const Rechteck& q = a.knoten[j];
                    CHECK_FALSE((p.x < q.x + q.breite && q.x < p.x + p.breite && p.y < q.y + q.hoehe && q.y < p.y + p.hoehe));
                }
                const std::size_t r = g.d.knoten[i].gruppe;
                if (r == kKeine) continue;
                const Rechteck& rahmen = a.gruppen[r];
                const Rechteck& k = a.knoten[i];
                CHECK((k.x >= rahmen.x && k.y >= rahmen.y && k.x + k.breite <= rahmen.x + rahmen.breite &&
                       k.y + k.hoehe <= rahmen.y + rahmen.hoehe));
            }
        }
        ausgabe::Optionen o;
        const std::string svg = ausgabe::svg().schreibe(g.d, o);
        CHECK(svg.find(">WITH schnitt</text>") != std::string::npos);
        CHECK(svg.find("stroke-dasharray:2 3") != std::string::npos);  // korreliert gepunktet
        const std::string mermaid = ausgabe::mermaid().schreibe(g.d, o);
        CHECK(mermaid.find("subgraph r0[\"WITH schnitt\"]") != std::string::npos);
        CHECK(mermaid.find("---|\"inner: b.schueler_id = s.schueler_id\"|") != std::string::npos);
        CHECK(mermaid.find("-->|\"exists\"|") != std::string::npos);
        const std::string dot = ausgabe::dot().schreibe(g.d, o);
        CHECK(dot.find("subgraph cluster_0") != std::string::npos);
        CHECK(dot.find("style=dotted") != std::string::npos);
        CHECK(ausgabe::drawio().schreibe(g.d, o).find("swimlane;") != std::string::npos);
        CHECK(ausgabe::drawio().kann(ausgabe::Diagrammart::Abfrage));
        CHECK_FALSE(ausgabe::csv().kann(ausgabe::Diagrammart::Abfrage));
        CHECK(ausgabe::nichtVerfuegbar(ausgabe::csv(), ausgabe::Diagrammart::Abfrage) == "Query is not available in csv");
    }

    TEST_CASE("Projekt: SQL in einer Zeichenkette ist eine Abfrage der Funktion, mit dem Schema einer .sql")
    {
        const std::filesystem::path ordner = std::filesystem::temp_directory_path() / "c2d_test_abfrage";
        std::filesystem::remove_all(ordner);
        std::filesystem::create_directories(ordner);
        {
            std::ofstream(ordner / "schema.sql") << kSchema;
            std::ofstream(ordner / "lager.py") << "import sqlite3\n"
                                                  "\n"
                                                  "def noten(db):\n"
                                                  "    return db.execute(\"SELECT s.name, b.note FROM schueler s \"\n"
                                                  "                      \"JOIN belegt b ON b.schueler_id = s.schueler_id\")\n";
        }
        const projekt::Projekt p = projekt::liesOrdner(ordner);
        const std::size_t py = p.finde("lager.py");
        REQUIRE(py != projekt::kKeine);
        const std::vector<modell::Abfrage>& q = p.dateien[py].ergebnis.einheit.abfragen;
        REQUIRE(q.size() == 1);
        CHECK(q[0].name == "noten@4");
        CHECK(q[0].bloecke[0].quellen[1].zeile == 5);  // die Zeile des zweiten Literals
        const Abfragediagramm d = projekt::abfrageIn(p, py, 0);
        CHECK(d.knoten[knoten(d, "schueler s")].art == AbfrageKnotenArt::Tabelle);  // das Schema der .sql
        CHECK(abfrageZurZeile(q, 5) == 0);
        CHECK(abfrageZurZeile(q, 1) == kKeine);
        std::filesystem::remove_all(ordner);
    }
}
