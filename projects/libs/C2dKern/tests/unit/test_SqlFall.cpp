/**
 * @file test_SqlFall.cpp
 * @brief CASE als Zuordnung (Entscheide Z1-Z4): einfache und gesuchte Form, Besitzer, Nummer in einer Zeile, ein inneres
 *        CASE, ohne ELSE, Hinweise; die CASE-Anweisung bleibt eine Auswahl
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Zuordnung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;
using modell::Fallausdruck;

namespace
{

sprache::Ergebnis sq(std::string_view text, std::string dialekt)
{
    sprache::LeseOptionen o;
    o.dialekt = std::move(dialekt);
    return sprache::sql().lies(text, "T.sql", o);
}

bool meldet(const diagramm::Zuordnung& z, std::string_view teil)
{
    return std::any_of(z.hinweise.begin(), z.hinweise.end(), [&](const std::string& h) { return h.find(teil) != std::string::npos; });
}

}  // namespace

TEST_SUITE("SQL-CASE")
{
    TEST_CASE("Z1-Z3: einfach und gesucht, ein WHEN keine Zuordnung, ein inneres CASE als `CASE … END`, Besitzer, Nummer")
    {
        const auto e = sq("SELECT id,\n"                                                                          // 1
                          "  CASE status WHEN 'n' THEN 'neu' WHEN 'v' THEN 'versandt' WHEN NULL THEN '?' WHEN 'n' THEN 'x' END,\n"  // 2
                          "  CASE WHEN m < 10 THEN 'klein' WHEN m < 100 THEN CASE WHEN m < 50 THEN 'a' ELSE 'b' END ELSE 'riesig' END,\n"  // 3
                          "  CASE WHEN m > 0 THEN 1 ELSE 0 END\n"                                                 // 4
                          "FROM auftrag;\n",                                                                     // 5
                          "postgres");
        REQUIRE(e.einheit.faelle.size() == 2);  // das CASE mit einem WHEN ist keine Zuordnung, das innere auch nicht
        const Fallausdruck& einfach = e.einheit.faelle[0];
        CHECK(einfach.besitzer == "query@1");
        CHECK(einfach.zeile == 2);
        CHECK(einfach.gegenstand == "status");
        CHECK(einfach.arme.size() == 4);
        CHECK_FALSE(einfach.mitSonst);
        const Fallausdruck& gesucht = e.einheit.faelle[1];
        CHECK(gesucht.gegenstand.empty());
        REQUIRE(gesucht.arme.size() == 2);
        CHECK(gesucht.arme[1].dann == "CASE \xE2\x80\xA6 END");
        CHECK(gesucht.sonst == "'riesig'");
        CHECK(diagramm::fallName(gesucht) == "query@1@3");

        // die Zuordnungen (Z2)
        const diagramm::Zuordnung z = diagramm::zuordnungVon(einfach);
        CHECK(z.titel == "query@1: CASE status");
        CHECK(z.schluessel == "status");
        REQUIRE(z.zeilen.size() == 5);
        CHECK(z.zeilen.back().schluessel == "otherwise");
        CHECK(z.zeilen.back().werte == std::vector<std::string>{"NULL (no ELSE)"});
        CHECK(meldet(z, "WHEN NULL (line 2) never matches"));
        CHECK(meldet(z, "key 'n' (line 2) is never reached"));
        const diagramm::Zuordnung g = diagramm::zuordnungVon(gesucht);
        CHECK(g.nummeriert);
        CHECK(g.titel == "query@1: CASE (first match wins)");
        CHECK(g.zeilen.back().schluessel == "ELSE");
        const std::string csv = ausgabe::csv().schreibe(std::span<const diagramm::Zuordnung>(&g, 1), {});
        CHECK(csv.find("#;condition;value") != std::string::npos);
        CHECK(csv.find("1;m < 10;'klein'") != std::string::npos);
        CHECK(csv.find(";ELSE;'riesig'") != std::string::npos);

        // zwei in einer Zeile: Nummer 1, 2
        const auto zwei = sq("SELECT CASE a WHEN 1 THEN 'x' WHEN 2 THEN 'y' END, CASE b WHEN 1 THEN 'x' WHEN 2 THEN 'y' END FROM t;\n",
                             "postgres");
        REQUIRE(zwei.einheit.faelle.size() == 2);
        CHECK(diagramm::fallName(zwei.einheit.faelle[0]) == "query@1@1:1");
        CHECK(diagramm::fallName(zwei.einheit.faelle[1]) == "query@1@1:2");
    }

    TEST_CASE("Z1, Z3: in Routinen — im SET (T-SQL) und im Rumpf in `$$` (Zeilen der Datei); die CASE-Anweisung nicht")
    {
        const auto t = sq("CREATE PROCEDURE dbo.p AS\n"                                                // 1
                          "BEGIN\n"                                                                    // 2
                          "  SET @s = CASE @x WHEN 1 THEN 'a' WHEN 2 THEN 'b' ELSE 'c' END\n"          // 3
                          "END\n"
                          "GO\n",
                          "tsql");
        REQUIRE(t.einheit.faelle.size() == 1);
        CHECK(t.einheit.faelle[0].besitzer == "dbo.p");
        CHECK(t.einheit.faelle[0].zeile == 3);

        const auto pg = sq("CREATE FUNCTION f(x int) RETURNS text LANGUAGE plpgsql AS $$\n"           // 1
                           "BEGIN\n"                                                                  // 2
                           "  CASE x WHEN 1 THEN RETURN 'a'; WHEN 2 THEN RETURN 'b'; END CASE;\n"     // 3
                           "  RETURN CASE WHEN x > 9 THEN 'gross' WHEN x > 5 THEN 'mittel' END;\n"    // 4
                           "END;\n"
                           "$$;\n",
                           "postgres");
        REQUIRE(pg.einheit.faelle.size() == 1);  // die CASE-Anweisung in Zeile 3 ist eine Auswahl, keine Zuordnung hier
        CHECK(pg.einheit.faelle[0].besitzer == "f");
        CHECK(pg.einheit.faelle[0].zeile == 4);
        CHECK(pg.einheit.faelle[0].arme[1].zeile == 4);
    }
}
