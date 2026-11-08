/**
 * @file test_SqlWahrheitstabelle.cpp
 * @brief Wahrheitstabellen der Bedingungen von Abfragen (Entscheide W1-W6): Atome, dreiwertig nach Kleene nur wo NULL
 *        moeglich (Schema, aeussere Joins), MC/DC ueber *Zeile bleibt*, kein Kurzschluss, Hinweise, Formate
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
#include <string>
#include <vector>

using namespace c2d;
using diagramm::Wahrheitstabelle;
using diagramm::Wert;

namespace
{

constexpr std::string_view kSchema =
    "CREATE TABLE kunde (id integer PRIMARY KEY, name text NOT NULL, land text, aktiv boolean NOT NULL);\n"   // 1
    "CREATE TABLE auftrag (id integer PRIMARY KEY, kunde_id integer NOT NULL REFERENCES kunde (id),\n"      // 2
    "                      summe numeric);\n";                                                               // 3

struct Ergebnis
{
    projekt::Projekt p;
    std::vector<Wahrheitstabelle> tabellen;
};

/// Die Tabellen aller Abfragen eines Texts (mit dem Schema oben), in der Reihenfolge der Abfragen.
Ergebnis tabellen(std::string_view abfragen)
{
    Ergebnis e;
    e.p = projekt::liesText(std::string(kSchema) + std::string(abfragen), "t.sql");
    const diagramm::DatenquellenSuche suche = projekt::datenquellenIn(e.p, 0);
    for (const modell::Abfrage& q : e.p.dateien[0].ergebnis.einheit.abfragen)
        for (Wahrheitstabelle& t : diagramm::wahrheitstabellenVon(q, suche)) e.tabellen.push_back(std::move(t));
    return e;
}

const Wahrheitstabelle* tabelle(const Ergebnis& e, std::string_view klausel, int zeile = 0)
{
    for (const Wahrheitstabelle& t : e.tabellen)
        if (t.klausel == klausel && (zeile == 0 || t.quellZeile == zeile)) return &t;
    return nullptr;
}

bool meldet(const Wahrheitstabelle& t, std::string_view teil)
{
    return std::any_of(t.hinweise.begin(), t.hinweise.end(), [&](const std::string& h) { return h.find(teil) != std::string::npos; });
}

}  // namespace

TEST_SUITE("SQL-Wahrheitstabelle")
{
    TEST_CASE("Atome (W2): NOT IN und BETWEEN im Atom, NOT vor einer Klammer ein Nicht, gleicher Text ohne Gross/klein")
    {
        const auto e = tabellen("SELECT * FROM kunde k\n"                                                   // 4
                                "WHERE k.land NOT IN ('AT', 'CH') AND NOT (k.id BETWEEN 1 AND 9 OR k.aktiv)\n"  // 5
                                "  AND (K.ID between 1 and 9 OR k.aktiv);\n");                             // 6
        const Wahrheitstabelle* t = tabelle(e, "WHERE");
        REQUIRE(t != nullptr);
        CHECK(t->quellZeile == 5);
        CHECK(t->dreiwertig);
        REQUIRE(t->atome.size() == 3);  // `K.ID between 1 and 9` ist `k.id BETWEEN 1 AND 9`
        CHECK(t->atome[0].text == "k.land NOT IN ('AT', 'CH')");
        CHECK(t->atome[1].text == "k.id BETWEEN 1 AND 9");
        CHECK(t->atome[2].text == "k.aktiv");
        CHECK(t->atome[0].kannNull);       // land ohne NOT NULL
        CHECK_FALSE(t->atome[1].kannNull);  // der Primaerschluessel
        CHECK_FALSE(t->atome[2].kannNull);  // NOT NULL
        CHECK(t->zeilen.size() == 12);      // 3 * 2 * 2
        // kein Kurzschluss (W5): jede Zeile nennt jedes Atom
        for (const Wahrheitstabelle::Zeile& z : t->zeilen)
            CHECK(std::none_of(z.werte.begin(), z.werte.end(), [](Wert w) { return w == Wert::Egal; }));
    }

    TEST_CASE("Kleene (W3) und MC/DC (W4): NOT mit NULL behaelt die Zeile nicht, Paare nur ohne unbekannt, Hinweis")
    {
        const auto e = tabellen("SELECT * FROM kunde k WHERE NOT (k.land = 'AT') AND k.aktiv;\n");  // 4
        const Wahrheitstabelle* t = tabelle(e, "WHERE");
        REQUIRE(t != nullptr);
        CHECK(t->bleibt == "Row kept");
        REQUIRE(t->zeilen.size() == 6);  // land: F, T, U; aktiv: F, T
        // Zeile 6: land unbekannt, aktiv wahr — NOT unbekannt ist unbekannt, die Zeile bleibt nicht
        const Wahrheitstabelle::Zeile& u = t->zeilen[5];
        CHECK(u.werte[0] == Wert::Unbekannt);
        CHECK(u.werte[1] == Wert::Wahr);
        CHECK(u.wert == Wert::Unbekannt);
        CHECK_FALSE(u.ergebnis);
        CHECK(t->zeilen[1].ergebnis);  // land falsch, aktiv wahr: bleibt
        // MC/DC: kein Paar mit einer Zeile, in der ein Atom unbekannt ist
        for (const auto& [a, b] : t->paare)
        {
            CHECK(std::find(t->zeilen[a - 1].werte.begin(), t->zeilen[a - 1].werte.end(), Wert::Unbekannt) == t->zeilen[a - 1].werte.end());
            CHECK(std::find(t->zeilen[b - 1].werte.begin(), t->zeilen[b - 1].werte.end(), Wert::Unbekannt) == t->zeilen[b - 1].werte.end());
        }
        CHECK_FALSE(u.mcdc);
        CHECK(meldet(*t, "A (k.land = 'AT'): if it is NULL, the row is not kept where false would keep it (row 6)"));
    }

    TEST_CASE("NULL ueber das Schema (W3): aeusserer Join, ON vor dem Auffuellen, unbekannte Spalte, IS NULL, EXISTS")
    {
        const auto e = tabellen("SELECT * FROM kunde k\n"                                                        // 4
                                "LEFT JOIN auftrag a ON a.kunde_id = k.id AND a.summe > 0\n"                     // 5
                                "WHERE a.kunde_id = 7 AND (a.id IS NULL OR k.aktiv) AND x.y > 1\n"               // 6
                                "  AND EXISTS (SELECT 1 FROM auftrag z WHERE z.kunde_id = k.id AND z.summe > 9);\n");  // 7
        const Wahrheitstabelle* on = tabelle(e, "ON", 5);
        REQUIRE(on != nullptr);
        CHECK(on->bleibt == "Joined");
        CHECK_FALSE(on->atome[0].kannNull);  // im eigenen ON ist auftrag noch nicht aufgefuellt: kunde_id NOT NULL
        CHECK(on->atome[1].kannNull);        // summe ohne NOT NULL
        const Wahrheitstabelle* wo = tabelle(e, "WHERE", 6);
        REQUIRE(wo != nullptr);
        REQUIRE(wo->atome.size() == 5);
        CHECK(wo->atome[0].kannNull);        // a.kunde_id: im WHERE die aufgefuellte Seite des LEFT JOIN
        CHECK_FALSE(wo->atome[1].kannNull);  // IS NULL
        CHECK_FALSE(wo->atome[2].kannNull);  // k.aktiv NOT NULL
        CHECK(wo->atome[3].kannNull);        // x.y: nicht im Schema
        CHECK_FALSE(wo->atome[4].kannNull);  // EXISTS
        CHECK(wo->atome[4].mitAufruf);
        CHECK(meldet(*wo, "D: a column not in the schema or a variable - taken as possibly NULL"));
        CHECK(meldet(*wo, "the database may evaluate the conditions in any order and skip some - E calls"));
        // die Unterabfrage hat ihre eigene Tabelle (W1); k.id ist korreliert und NOT NULL
        const Wahrheitstabelle* innen = tabelle(e, "WHERE", 7);
        REQUIRE(innen != nullptr);
        REQUIRE(innen->atome.size() == 2);
        CHECK_FALSE(innen->atome[0].kannNull);
        CHECK(innen->atome[1].kannNull);
    }

    TEST_CASE("Grenze (W3): ueber 81 Zeilen nur die MC/DC-Zeilen; HAVING; eine Variable kann NULL sein")
    {
        const auto e = tabellen("SELECT land FROM kunde\n"                                                     // 4
                                "WHERE land = 'a' OR land = 'b' OR land = 'c' OR land = 'd' OR land = 'e'\n"  // 5
                                "GROUP BY land HAVING count(*) > 1 AND max(name) > @grenze;\n");              // 6
        const Wahrheitstabelle* wo = tabelle(e, "WHERE");
        REQUIRE(wo != nullptr);
        CHECK(wo->gekuerzt);  // 3^5 = 243
        CHECK(meldet(*wo, "only the MC/DC rows of 243"));
        CHECK(std::all_of(wo->zeilen.begin(), wo->zeilen.end(), [](const Wahrheitstabelle::Zeile& z) { return z.mcdc; }));
        const Wahrheitstabelle* haben = tabelle(e, "HAVING");
        REQUIRE(haben != nullptr);
        CHECK_FALSE(haben->atome[0].kannNull);  // count(*): keine Spalte
        CHECK(haben->atome[1].kannNull);        // eine Variable
        CHECK(diagramm::ueberschrift(*haben) == "query@4 HAVING: line 6");
    }

    TEST_CASE("Routinen (R7): IF dreiwertig mit Spalte Branch, IS NULL zweiwertig, ein Wert ohne Branch, kein Kurzschluss")
    {
        sprache::LeseOptionen o;
        o.dialekt = "tsql";
        const sprache::Ergebnis e = sprache::sql().lies("CREATE PROCEDURE p @x int AS\n"                   // 1
                                                        "BEGIN\n"                                          // 2
                                                        "  IF NOT (@x = 1) AND @y IS NULL PRINT 'a'\n"     // 3
                                                        "END\n",
                                                        "p.sql", o);
        REQUIRE(e.einheit.funktionen.size() == 1);
        const std::vector<Wahrheitstabelle> ts = diagramm::wahrheitstabellenVon(e.einheit.funktionen[0]);
        REQUIRE(ts.size() == 1);
        const Wahrheitstabelle& t = ts[0];
        CHECK(t.dreiwertig);
        CHECK(t.bleibt == "Branch");
        CHECK(t.klausel.empty());
        CHECK(diagramm::ueberschrift(t) == "p(): line 3");
        REQUIRE(t.atome.size() == 2);
        CHECK(t.atome[0].kannNull);         // ein Parameter
        CHECK_FALSE(t.atome[1].kannNull);   // IS NULL
        CHECK(t.zeilen.size() == 6);
        CHECK(std::none_of(t.zeilen.begin(), t.zeilen.end(), [](const Wahrheitstabelle::Zeile& z) {
            return std::find(z.werte.begin(), z.werte.end(), Wert::Egal) != z.werte.end();
        }));
        CHECK(meldet(t, "A (@x = 1): if it is NULL, the condition is not true where false would make it true"));
        // ein Wert (`SET @ok = a AND b`): dreiwertig, ohne Spalte Branch
        const sprache::Ergebnis w = sprache::sql().lies("CREATE PROCEDURE q AS BEGIN SET @ok = @a = 1 AND @b = 2 END\n", "q.sql", o);
        const std::vector<Wahrheitstabelle> tw = diagramm::wahrheitstabellenVon(w.einheit.funktionen[0]);
        REQUIRE(tw.size() == 1);
        CHECK(tw[0].dreiwertig);
        CHECK(tw[0].bleibt.empty());
    }

    TEST_CASE("`SELECT * EXCEPT (…)` (Spark) laesst Spalten weg — keine Mengenoperation, das WHERE gehoert dem Block")
    {
        sprache::LeseOptionen o;
        o.dialekt = "spark";
        const sprache::Ergebnis e = sprache::sql().lies("SELECT * EXCEPT (m.rn, x) FROM m JOIN t USING (id)\n"
                                                        "WHERE m.rn = 1 AND id IS NOT NULL;\n",
                                                        "s.sql", o);
        REQUIRE(e.einheit.abfragen.size() == 1);
        REQUIRE(e.einheit.abfragen[0].bloecke.size() == 1);
        CHECK(e.einheit.abfragen[0].bloecke[0].quellen.size() == 2);
        REQUIRE(e.einheit.abfragen[0].bloecke[0].woGanz.logik);
        CHECK(e.einheit.abfragen[0].bloecke[0].woGanz.zeile == 2);
    }

    TEST_CASE("Abfragediagramm und Formate (W6): Zeilen der Tabellen an Ergebnis und Quelle, csv mit U und Row kept")
    {
        const auto e = tabellen("SELECT * FROM kunde k JOIN auftrag a ON a.kunde_id = k.id OR a.summe > 0\n"  // 4
                                "WHERE k.land = 'AT' AND k.aktiv;\n");                                       // 5
        const diagramm::Abfragediagramm d = projekt::abfrageIn(e.p, 0, 0);
        bool ergebnis = false;
        bool quelle = false;
        for (const diagramm::AbfrageKnoten& k : d.knoten)
        {
            using S = std::vector<diagramm::Tabellenstelle>;
            if (k.art == diagramm::AbfrageKnotenArt::Ergebnis && k.tabellen == S{{5, 0}}) ergebnis = true;
            if (k.titel == "auftrag a" && k.tabellen == S{{4, 0}}) quelle = true;
        }
        CHECK(ergebnis);
        CHECK(quelle);
        const std::string csv = ausgabe::csv().schreibe(std::span<const Wahrheitstabelle>(e.tabellen), {});
        CHECK(csv.find("query@4 WHERE: line 5") != std::string::npos);
        CHECK(csv.find(";Row kept;") != std::string::npos);
        CHECK(csv.find(";Joined;") != std::string::npos);
        CHECK(csv.find("A: k.land = 'AT' (null)") != std::string::npos);
        CHECK(csv.find(";U;") != std::string::npos);
        // die Anordnung hat die Spalte Row kept
        const diagramm::TabellenAnordnung a = diagramm::ordneAn(std::span<const Wahrheitstabelle>(e.tabellen), {});
        CHECK(std::any_of(a.zellen.begin(), a.zellen.end(), [](const auto& z) {
            return !z.zeilen.empty() && z.zeilen.front() == "Row kept";
        }));
    }

    TEST_CASE("Mehrere Bedingungen in einer Zeile (L2): je eine Nummer, an Ergebnis und Quelle ihre eigene")
    {
        const auto e = tabellen("SELECT * FROM kunde k JOIN auftrag a ON a.kunde_id = k.id OR a.summe > 0 "
                                "WHERE k.land = 'AT' AND k.aktiv;\n"  // 4
                                "SELECT * FROM kunde WHERE id = 1 OR id = 2;\n");  // 5
        // in Zeile 4 ON und WHERE: 4:1 und 4:2 in der Folge der Ableitung (ON, dann WHERE); in Zeile 5 allein
        const Wahrheitstabelle* on = tabelle(e, "ON", 4);
        const Wahrheitstabelle* wo = tabelle(e, "WHERE", 4);
        const Wahrheitstabelle* fuenf = tabelle(e, "WHERE", 5);
        REQUIRE(on != nullptr);
        REQUIRE(wo != nullptr);
        REQUIRE(fuenf != nullptr);
        CHECK(on->nummer == 1);
        CHECK(wo->nummer == 2);
        CHECK(fuenf->nummer == 0);
        CHECK(diagramm::stellenText(on->stelle()) == "4:1");
        CHECK(diagramm::stellenText(wo->stelle()) == "4:2");
        CHECK(diagramm::stellenText(fuenf->stelle()) == "5");
        CHECK(diagramm::stellenMitWahrheitstabelle(e.p.dateien[0].ergebnis.einheit.abfragen[0]) ==
              std::vector<diagramm::Tabellenstelle>{{4, 1}, {4, 2}});
        // das Diagramm traegt am Ergebnis WHERE, an der Quelle ON, je mit Nummer
        const diagramm::Abfragediagramm d = projekt::abfrageIn(e.p, 0, 0);
        bool ergebnis = false;
        bool quelle = false;
        for (const diagramm::AbfrageKnoten& k : d.knoten)
        {
            using S = std::vector<diagramm::Tabellenstelle>;
            if (k.art == diagramm::AbfrageKnotenArt::Ergebnis && k.tabellen == S{{4, 2}}) ergebnis = true;
            if (k.titel == "auftrag a" && k.tabellen == S{{4, 1}}) quelle = true;
        }
        CHECK(ergebnis);
        CHECK(quelle);
    }

    TEST_CASE("Die Stelle als Text (L2): 12 und 12:2 hin und zurueck, anderes nicht")
    {
        using diagramm::stelleAusText;
        using diagramm::Tabellenstelle;
        CHECK(stelleAusText("12") == Tabellenstelle{12, 0});
        CHECK(stelleAusText("12:2") == Tabellenstelle{12, 2});
        CHECK_FALSE(stelleAusText(""));
        CHECK_FALSE(stelleAusText("0"));
        CHECK_FALSE(stelleAusText("12:"));
        CHECK_FALSE(stelleAusText("12:0"));
        CHECK_FALSE(stelleAusText("a"));
        CHECK_FALSE(stelleAusText("12:x"));
        CHECK(diagramm::nummeriere({3, 4, 3, 5}) == std::vector<Tabellenstelle>{{3, 1}, {4, 0}, {3, 2}, {5, 0}});
    }
}
