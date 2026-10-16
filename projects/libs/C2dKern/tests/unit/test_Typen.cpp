/**
 * @file test_Typen.cpp
 * @brief Links auf Datentypen: Namen in einem Text, Typnamen ueber Dateien aufloesen, die Ansicht eines Typs
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/projekt/Typen.hpp"
#include "c2d/sprache/Sprache.hpp"

#include "TempOrdner.hpp"

#include <algorithm>

using namespace c2d::projekt;

namespace
{

/// Ein Projekt: Typen in Koepfen, ein typedef in main.c, ein gleichnamiger Typ in einem Kopf,
/// den main.c nicht einbindet.
void legeProjekt(const TempOrdner& t)
{
    t.lege("inc/typen.h", "typedef struct { float kp; } t_PID;\n"
                          "typedef enum { AUS, AN } t_MODUS;\n"
                          "struct Regler { t_PID pid; t_MODUS modus; struct Motor *motor; };\n");
    t.lege("inc/motor.h", "#include \"typen.h\"\nstruct Motor { int n; t_PID pid; };\n");
    t.lege("main.c", "#include \"inc/motor.h\"\n"
                     "typedef struct Motor Motor_t;\n"
                     "int main(void) { Motor_t mo; t_PID p = {0}; (void)sizeof(struct Regler); return \"t_PID\"[0]; }\n");
    t.lege("andere/a.h", "typedef struct { int x; } t_PID;\n");
    t.lege("sim.c", "void f(void) { t_PID x; }\n");
}

std::vector<std::string> wo(const Projekt& p, const std::vector<TypStelle>& stellen)
{
    std::vector<std::string> aus;
    for (const TypStelle& s : stellen) aus.push_back(p.dateien[s.datei].pfad + ":" + typ(p, s).name);
    return aus;
}

}  // namespace

TEST_SUITE("Typen")
{
    TEST_CASE("Namen in einem Text: mit Bereich, ohne Literale und Zahlen, jeder einmal")
    {
        CHECK(c2d::modell::bezeichnerIn("Motor_t mo; t_PID p = {0x1Fu}; t_PID q;") ==
              std::vector<std::string>{"Motor_t", "mo", "t_PID", "p", "q"});
        CHECK(c2d::modell::bezeichnerIn("antrieb::Motor m(\"Regler\", 'x');") ==
              std::vector<std::string>{"antrieb::Motor", "m"});
    }

    TEST_CASE("Ein Typname: erst die eigene Datei, dann die eingebundenen Koepfe, dann jede Datei")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const std::size_t main = p.finde("main.c");
        // main.c bindet inc/motor.h ein, das inc/typen.h — beide stehen in den Koepfen
        REQUIRE(p.dateien[main].koepfe.size() == 2);
        CHECK(p.dateien[p.dateien[main].koepfe[0]].pfad == "inc/motor.h");
        CHECK(p.dateien[p.dateien[main].koepfe[1]].pfad == "inc/typen.h");

        const Typverzeichnis v(p);
        // aus den Koepfen — nicht der gleichnamige in andere/a.h
        CHECK(wo(p, v.kandidaten(main, "t_PID")) == std::vector<std::string>{"inc/typen.h:t_PID"});
        CHECK(wo(p, v.kandidaten(main, "Regler")) == std::vector<std::string>{"inc/typen.h:Regler"});
        // ein typedef der eigenen Datei fuehrt zu seinem Typ
        CHECK(wo(p, v.kandidaten(main, "Motor_t")) == std::vector<std::string>{"inc/motor.h:Motor"});
        // ohne Einbindung: jede Datei — zwei Kandidaten, nichts wird still gewaehlt
        CHECK(wo(p, v.kandidaten(p.finde("sim.c"), "t_PID")) ==
              std::vector<std::string>{"andere/a.h:t_PID", "inc/typen.h:t_PID"});
        CHECK(v.kandidaten(main, "gibt_es_nicht").empty());

        // die Namen eines Texts, die Typen meinen — nicht im Literal, nicht die Variablen
        const std::vector<TypVerweis> verweise = v.verweiseIn(main, "Motor_t mo; t_PID p; sizeof(struct Regler); \"t_MODUS\"");
        REQUIRE(verweise.size() == 3);
        CHECK(verweise[0].name == "Motor_t");
        CHECK(verweise[1].name == "t_PID");
        CHECK(verweise[2].name == "Regler");
    }

    TEST_CASE("Die Ansicht eines Typs: er und seine direkten Nachbarn der Einheit, mit Kanten")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies("typedef enum { AUS, AN } t_MODUS;\n"
                                                                "struct Pid { float kp; };\n"
                                                                "struct Regler { struct Pid pid; t_MODUS modus; };\n"
                                                                "struct Anlage { struct Regler regler; };\n"
                                                                "struct Fern { int x; };\n",
                                                                "t.c");
        using namespace c2d::diagramm;
        const Strukturdiagramm d = strukturVon(e.einheit, "Regler");
        CHECK(d.titel == "Regler");
        REQUIRE(d.kaesten.size() == 4);
        CHECK(d.kaesten[0].name == "Regler");  // der Typ zuerst
        std::vector<std::string> namen;
        for (const Kasten& k : d.kaesten) namen.push_back(k.name);
        CHECK(std::find(namen.begin(), namen.end(), "Fern") == namen.end());  // kein Nachbar
        CHECK(std::find(namen.begin(), namen.end(), "Anlage") != namen.end());  // enthaelt Regler
        CHECK(d.beziehungen.size() == 3);  // Regler -> Pid, Regler -> t_MODUS, Anlage -> Regler
        CHECK(strukturVon(e.einheit, "gibt_es_nicht").kaesten.empty());
    }
}
