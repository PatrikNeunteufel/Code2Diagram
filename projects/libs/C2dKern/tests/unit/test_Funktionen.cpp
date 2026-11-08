/**
 * @file test_Funktionen.cpp
 * @brief Das Verzeichnis der Funktionen: es findet, was die Suche ueber alle Funktionen fand — an Namen mit
 *        Bereich, an SQL mit Schema und ohne Gross/klein, an static, und ueber Sprachen hinweg
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/modell/Code.hpp"
#include "c2d/projekt/Funktionen.hpp"
#include "c2d/projekt/Projekt.hpp"

#include "TempOrdner.hpp"

#include <set>
#include <string>
#include <vector>

using namespace c2d::projekt;

namespace
{

/// Die Suche, wie sie vor dem Verzeichnis war: jede Funktion mit Rumpf, die so heisst; erst die eigene Datei,
/// dann die nicht internen der anderen.
std::vector<Fundstelle> ueberAlle(const Projekt& p, std::size_t ausDatei, std::string_view aufruf)
{
    std::vector<Fundstelle> aus;
    const auto sammle = [&](std::size_t d, bool nurOeffentliche) {
        const auto& fs = p.dateien[d].ergebnis.einheit.funktionen;
        for (std::size_t f = 0; f < fs.size(); ++f)
            if (fs[f].rumpf && fs[f].heisst(aufruf) && !(nurOeffentliche && fs[f].intern)) aus.push_back({d, f});
    };
    if (ausDatei < p.dateien.size())
    {
        sammle(ausDatei, false);
        if (!aus.empty()) return aus;
    }
    for (std::size_t d = 0; d < p.dateien.size(); ++d)
        if (d != ausDatei) sammle(d, true);
    return aus;
}

std::vector<Fundstelle> aufruferUeberAlle(const Projekt& p, const Fundstelle& ziel)
{
    std::vector<Fundstelle> aus;
    const c2d::modell::Funktion& gerufen = funktion(p, ziel);
    for (std::size_t d = 0; d < p.dateien.size(); ++d)
    {
        const auto& fs = p.dateien[d].ergebnis.einheit.funktionen;
        for (std::size_t f = 0; f < fs.size(); ++f)
        {
            if (!fs[f].rumpf) continue;
            for (const std::string& aufruf : c2d::modell::aufrufeVon(fs[f]))
            {
                if (!gerufen.heisst(aufruf)) continue;
                const std::vector<Fundstelle> gemeint = ueberAlle(p, d, aufruf);
                bool trifft = false;
                for (const Fundstelle& g : gemeint) trifft = trifft || g == ziel;
                if (trifft)
                {
                    aus.push_back({d, f});
                    break;
                }
            }
        }
    }
    return aus;
}

}  // namespace

TEST_SUITE("Funktionsverzeichnis")
{
    TEST_CASE("Gleiche Kandidaten und Aufrufer wie die Suche ueber alle Funktionen")
    {
        TempOrdner t;
        t.lege("main.c", "static int hilf(int n) { return n; }\n"
                         "int main(void) { hilf(1); starte(); regle(); lager_zaehle(); return 0; }\n");
        t.lege("motor/motor.c", "static void hilf(void) { }\n"
                                "void starte(void) { hilf(); }\n"
                                "void regle(void) { starte(); }\n");
        t.lege("slave/s.c", "void starte(void) { }\n");
        t.lege("lager.cpp", "namespace lager { struct Bestand { int zaehle(); void leere(); }; }\n"
                            "int lager::Bestand::zaehle() { leere(); return 0; }\n"
                            "void lager::Bestand::leere() { }\n"
                            "int zaehle() { return lager::Bestand().zaehle() + Bestand::zaehle(); }\n");
        t.lege("schema.sql", "CREATE FUNCTION lager.zaehle_Bestand() RETURNS int AS $$ BEGIN RETURN 1; END $$ LANGUAGE plpgsql;\n"
                             "CREATE FUNCTION Lager_Zaehle() RETURNS int AS $$ BEGIN RETURN lager.ZAEHLE_bestand(); END $$ "
                             "LANGUAGE plpgsql;\n");
        const Projekt p = liesOrdner(t.weg());
        const Funktionsverzeichnis v(p);
        REQUIRE(&v.projekt() == &p);

        // jeder Name, der irgendwo gerufen wird, dazu jeder volle Name und Schreibweisen mit Bereich und Schema
        std::set<std::string> namen = {"hilf", "starte", "regle", "zaehle", "leere", "Bestand::zaehle",
                                       "lager::Bestand::zaehle", "::zaehle", "Bestand::", "lager.zaehle_bestand",
                                       "ZAEHLE_BESTAND", "x.zaehle_bestand", "lager_zaehle", "LAGER_ZAEHLE",
                                       "gibt_es_nicht", ""};
        for (const Datei& d : p.dateien)
            for (const c2d::modell::Funktion& f : d.ergebnis.einheit.funktionen)
            {
                namen.insert(f.name);
                namen.insert(f.vollerName());
                if (f.rumpf)
                    for (const std::string& a : c2d::modell::aufrufeVon(f)) namen.insert(a);
            }
        std::size_t getroffen = 0;
        for (std::size_t aus = 0; aus <= p.dateien.size(); ++aus)  // auch aus keiner Datei
            for (const std::string& name : namen)
            {
                CAPTURE(aus);
                CAPTURE(name);
                const std::vector<Fundstelle> erwartet = ueberAlle(p, aus, name);
                CHECK(v.kandidaten(aus, name) == erwartet);
                getroffen += erwartet.size();
            }
        CHECK(getroffen > 20);  // der Vergleich prueft etwas
        // und er prueft die Wege, um die es geht: SQL ohne Gross/klein und Schema, C++ mit Bereich, C nach SQL
        CHECK(v.kandidaten(kKeine, "ZAEHLE_BESTAND").size() == 1);
        CHECK(v.kandidaten(kKeine, "LAGER.zaehle_bestand").size() == 1);
        CHECK(v.kandidaten(kKeine, "x.zaehle_bestand").empty());  // beide mit Schema, verschieden: nicht dieselbe
        CHECK(v.kandidaten(kKeine, "Bestand::zaehle").size() == 1);
        CHECK(v.kandidaten(kKeine, "zaehle").size() == 2);  // die Methode und die freie Funktion
        CHECK(v.kandidaten(p.finde("main.c"), "lager_zaehle").size() == 1);

        std::size_t gerufen = 0;
        for (std::size_t d = 0; d < p.dateien.size(); ++d)
            for (std::size_t f = 0; f < p.dateien[d].ergebnis.einheit.funktionen.size(); ++f)
            {
                if (!p.dateien[d].ergebnis.einheit.funktionen[f].rumpf) continue;
                CAPTURE(p.dateien[d].ergebnis.einheit.funktionen[f].vollerName());
                const std::vector<Fundstelle> erwartet = aufruferUeberAlle(p, {d, f});
                CHECK(v.aufrufer({d, f}) == erwartet);
                gerufen += erwartet.size();
            }
        CHECK(gerufen > 5);
    }

    TEST_CASE("Leer: kein Projekt, keine Kandidaten")
    {
        const Funktionsverzeichnis v;
        CHECK(v.projekt().dateien.empty());
        CHECK(v.kandidaten(0, "main").empty());
        CHECK(v.aufrufer({0, 0}).empty());
    }
}
