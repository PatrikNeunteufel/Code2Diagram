/**
 * @file test_Schaltstellen.cpp
 * @brief Wo schaltet ein Name im Projekt: Setzungen, Zweige (absolut, auch innere), Stand je Datei, Uebersicht
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/projekt/Schaltstellen.hpp"
#include "c2d/sprache/Sprache.hpp"

#include "TempOrdner.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d::diagramm;
using namespace c2d::projekt;

namespace
{

const std::string kStrich = "\xE2\x80\x93";

/// TAKT: in der Projektdatei definiert, im Abschnitt `zwei` aufgehoben; MOTOR aus einem Kopf;
/// LANGSAM mit #undef; ANDERS legt niemand fest; F(TAKT) laesst sich nicht zerlegen.
void legeProjekt(const TempOrdner& t)
{
    t.lege("code2diagram.json",
           "{\"definitionen\": [\"TAKT=100\"], \"abschnitte\": [{\"ordner\": \"zwei\", \"definitionen\": [\"!TAKT\"]}]}\n");
    t.lege("cfg.h", "#define MOTOR 2\n");
    t.lege("eins.c", "#include \"cfg.h\"\n"  // 1
                     "#if defined(TAKT)\n"   // 2
                     "int a;\n"              // 3
                     "#if MOTOR > 1\n"       // 4
                     "int m;\n"              // 5
                     "#endif\n"              // 6
                     "#else\n"               // 7
                     "int b;\n"              // 8
                     "#endif\n"              // 9
                     "#ifdef ANDERS\n"       // 10
                     "int x;\n"              // 11
                     "#endif\n");            // 12
    t.lege("zwei/zwei.c", "#if TAKT > 50\n"           // 1
                          "int c;\n"                  // 2
                          "#elif defined(LANGSAM)\n"  // 3
                          "int l;\n"                  // 4
                          "#endif\n"                  // 5
                          "#undef LANGSAM\n");        // 6
    t.lege("drei.c", "#if F(TAKT) > 1\n"
                     "int f;\n"
                     "#endif\n");
}

/// Die Zeilen als "Datei:Zeile Zweig = Forderung | Stand [*]; ".
std::string bild(const Schaltstellen& s)
{
    std::string aus;
    for (const Schaltstellen::Zeile& z : s.zeilen)
        aus += z.pfad + ":" + std::to_string(z.quellZeile) + " " + z.zweig + " = " + z.forderung + " | " + z.stand +
               (z.geltend ? " *" : "") + "; ";
    return aus;
}

const Schalteruebersicht::Zeile* zeileFuer(const Schalteruebersicht& u, const std::string& name)
{
    const auto z = std::find_if(u.zeilen.begin(), u.zeilen.end(), [&](const auto& x) { return x.name == name; });
    return z != u.zeilen.end() ? &*z : nullptr;
}

}  // namespace

TEST_SUITE("Schaltstellen")
{
    TEST_CASE("Ein Name: wer ihn festlegt, je Zweig mit Forderung eine Zeile, absolut und je Datei mit ihrem Stand")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const Schaltstellen s = schaltstellen(p, "TAKT");

        REQUIRE(s.gesetzt.size() == 2);
        CHECK(s.gesetzt[0].herkunft == "project");
        CHECK(s.gesetzt[0].pfad == "code2diagram.json");
        CHECK(s.gesetzt[0].wert == "TAKT=100");
        CHECK(s.gesetzt[1].herkunft == "section zwei");
        CHECK(s.gesetzt[1].wert == "!TAKT");

        // die innere Kette (MOTOR) gilt nur mit TAKT: ihre Zweige fordern ihn absolut
        // drei.c fragt nur in Text nach TAKT: der Stand kommt aus den Definitionen der Datei
        CHECK(bild(s) == "drei.c:1 #if F(TAKT) > 1 = ? | = 100 (project) *; "
                         "drei.c:3 (no #else) = ? | = 100 (project); "
                         "eins.c:2 #if defined(TAKT) = def | = 100 (project) *; "
                         "eins.c:4 #if MOTOR > 1 = def | = 100 (project) *; "
                         "eins.c:6 (no #else) = def | = 100 (project); "
                         "eins.c:7 #else = !def | = 100 (project); "
                         "zwei/zwei.c:1 #if TAKT > 50 = > 50 | undef (section zwei); "
                         "zwei/zwei.c:3 #elif defined(LANGSAM) = <= 50 | undef (section zwei); "
                         "zwei/zwei.c:5 (no #else) = <= 50 | undef (section zwei) *; ");
        CHECK(s.titel == "Where is TAKT switched? 9 branches in 3 files");

        // die Zeilen eines Zweigs: bis zur naechsten Direktive; beim fehlenden #else unbekannt
        REQUIRE(s.zeilen.size() == 9);
        CHECK(s.zeilen[2].von == 2);
        CHECK(s.zeilen[2].bis == 7);
        CHECK(zeilenDazwischen(s.zeilen[2]) == 4);
        CHECK(s.zeilen[3].bis == 6);
        CHECK(s.zeilen[4].bis == 0);
        CHECK(s.zeilen[0].text);
        CHECK(s.zeilen[2].datei == p.finde("eins.c"));
        CHECK(s.zeilen[3].weiche == 1);
        CHECK_FALSE(s.hinweise.empty());
    }

    TEST_CASE("Stand aus einem Kopf, #undef als Setzung, niemand, von aussen, ein Name, der nicht vorkommt")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());

        const Schaltstellen motor = schaltstellen(p, "MOTOR");
        REQUIRE(motor.gesetzt.size() == 1);
        CHECK(motor.gesetzt[0].herkunft == "#define");
        CHECK(motor.gesetzt[0].pfad == "cfg.h");
        CHECK(motor.gesetzt[0].zeile == 1);
        CHECK(motor.gesetzt[0].wert == "2");
        CHECK(bild(motor) == "eins.c:4 #if MOTOR > 1 = > 1 | = 2 (header cfg.h) *; eins.c:6 (no #else) = <= 1 | = 2 (header cfg.h); ");

        const Schaltstellen langsam = schaltstellen(p, "LANGSAM");
        REQUIRE(langsam.gesetzt.size() == 1);
        CHECK(langsam.gesetzt[0].herkunft == "#undef");
        CHECK(langsam.gesetzt[0].pfad == "zwei/zwei.c");
        CHECK(langsam.gesetzt[0].zeile == 6);
        CHECK(bild(langsam) == "zwei/zwei.c:3 #elif defined(LANGSAM) = def | undef; zwei/zwei.c:5 (no #else) = !def | undef *; ");

        const Schaltstellen anders = schaltstellen(p, "ANDERS");
        CHECK(anders.gesetzt.empty());
        CHECK(bild(anders) == "eins.c:10 #if defined(ANDERS) = def | undef; eins.c:12 (no #else) = !def | undef *; ");

        // von aussen geht der Projektdatei und dem Abschnitt vor
        const std::vector<c2d::sprache::Definition> aussen{{"TAKT", "5"}};
        const Projekt q = liesOrdner(t.weg(), {.definitionen = aussen});
        const Schaltstellen takt = schaltstellen(q, "TAKT", aussen);
        REQUIRE(takt.gesetzt.size() == 3);
        CHECK(takt.gesetzt[2].herkunft == "outside");
        CHECK(takt.gesetzt[2].wert == "TAKT=5");
        REQUIRE(takt.zeilen.size() == 9);
        CHECK(takt.zeilen[0].stand == "= 5 (outside)");
        CHECK(takt.zeilen[2].stand == "= 5 (outside)");
        CHECK(takt.zeilen[7].stand == "= 5 (outside)");
        CHECK(takt.zeilen[7].geltend == false);  // TAKT > 50 gilt nicht, LANGSAM nicht definiert

        const Schaltstellen keiner = schaltstellen(p, "GIBT_ES_NICHT");
        CHECK(keiner.zeilen.empty());
        CHECK(keiner.gesetzt.empty());
        CHECK(keiner.hinweise.empty());
    }

    TEST_CASE("Uebersicht: je Name Dateien, Ketten, Stand (differs), wer ihn festlegt; nach Namen")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const Schalteruebersicht u = schalteruebersicht(p);
        std::vector<std::string> namen;
        for (const auto& z : u.zeilen) namen.push_back(z.name);
        CHECK(std::find(namen.begin(), namen.end(), "ANDERS") < std::find(namen.begin(), namen.end(), "TAKT"));

        const Schalteruebersicht::Zeile* takt = zeileFuer(u, "TAKT");
        REQUIRE(takt != nullptr);
        CHECK(takt->dateien == 2);  // eins.c, zwei.c (in drei.c fragt kein zerlegbarer Zweig)
        CHECK(takt->ketten == 3);   // die aeussere und die innere in eins.c, die in zwei.c
        CHECK(takt->abweichend);
        CHECK(takt->stand == "differs");
        CHECK(takt->herkunft == "project, section");

        const Schalteruebersicht::Zeile* motor = zeileFuer(u, "MOTOR");
        REQUIRE(motor != nullptr);
        CHECK(motor->dateien == 1);
        CHECK(motor->ketten == 1);
        CHECK(motor->stand == "= 2");
        CHECK(motor->herkunft == "header");

        const Schalteruebersicht::Zeile* anders = zeileFuer(u, "ANDERS");
        REQUIRE(anders != nullptr);
        CHECK(anders->stand == "undef");
        CHECK(anders->herkunft.empty());
    }

    TEST_CASE("Was die Datei nach der ersten Frage selbst festlegt, gilt an den Ketten danach")
    {
        TempOrdner t;
        t.lege("k.c", "#if !defined(K)\n"  // 1
                      "#define K 3\n"      // 2
                      "#endif\n"           // 3
                      "#if K > 2\n"        // 4
                      "int k;\n"           // 5
                      "#endif\n"           // 6
                      "#undef K\n"         // 7
                      "#ifdef K\n"         // 8
                      "int j;\n"           // 9
                      "#endif\n");         // 10
        const Projekt p = liesOrdner(t.weg());
        const Schaltstellen s = schaltstellen(p, "K");
        CHECK(bild(s) == "k.c:1 #if !defined(K) = !def | undef *; k.c:3 (no #else) = def | undef; "
                         "k.c:4 #if K > 2 = > 2 | = 3 (line 2) *; k.c:6 (no #else) = <= 2 | = 3 (line 2); "
                         "k.c:8 #if defined(K) = def | undef (line 7); k.c:10 (no #else) = !def | undef (line 7) *; ");
        REQUIRE(s.zeilen.size() == 6);
        CHECK(s.zeilen[0].inDatei == 0);
        CHECK(s.zeilen[2].inDatei == 2);
        CHECK(s.zeilen[4].inDatei == 7);
        const Schalteruebersicht u = schalteruebersicht(p);
        const Schalteruebersicht::Zeile* k = zeileFuer(u, "K");
        REQUIRE(k != nullptr);
        CHECK(k->ketten == 3);
        CHECK(k->stand == "differs");  // undef an der ersten Kette, = 3 an der zweiten
        CHECK(k->herkunft == "file");
    }

    TEST_CASE("Ein langer Pfad wird vorn gekuerzt: der Dateiname bleibt zu sehen")
    {
        TempOrdner t;
        const std::string ordner = "ein/sehr/langer/ordner/mit/vielen/stufen/darunter/noch/mehr";
        t.lege(ordner + "/schalter_datei.c", "#ifdef X\nint x;\n#endif\n");
        const Projekt p = liesOrdner(t.weg());
        AnordnungOptionen o;
        o.maxZeilenLaenge = 40;
        const TabellenAnordnung a = ordneAn(schaltstellen(p, "X"), o);
        const auto z = std::find_if(a.zellen.begin(), a.zellen.end(), [](const TabellenAnordnung::Zelle& c) {
            return !c.kopf && !c.zeilen.empty() && c.zeilen.front().starts_with("...");
        });
        REQUIRE(z != a.zellen.end());
        CHECK(z->zeilen.front().ends_with("/schalter_datei.c"));
        CHECK(z->zeilen.front().size() == 40);
        CHECK(z->ganz == ordner + "/schalter_datei.c");
    }

    TEST_CASE("Formate: svg, drawio und csv schreiben, Mermaid und DOT sagen ab; Knoten erst je Setzung, dann je Zeile")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const Schaltstellen s = schaltstellen(p, "TAKT");
        const Schalteruebersicht u = schalteruebersicht(p);
        using c2d::ausgabe::Diagrammart;
        CHECK_FALSE(c2d::ausgabe::mermaid().kann(Diagrammart::Schaltstellen));
        CHECK_FALSE(c2d::ausgabe::dot().kann(Diagrammart::Schalteruebersicht));
        CHECK(c2d::ausgabe::csv().kann(Diagrammart::Schaltstellen));
        CHECK(c2d::ausgabe::nichtVerfuegbar(c2d::ausgabe::mermaid(), Diagrammart::Schaltstellen) ==
              "#if switch points is not available in mermaid");

        const std::string csv = c2d::ausgabe::csv().schreibe(s, {});
        CHECK(csv.find("project;code2diagram.json;;TAKT=100\n") != std::string::npos);
        CHECK(csv.find("eins.c;2;#if defined(TAKT);def;= 100 (project);2;7;4;yes\n") != std::string::npos);
        CHECK(csv.find("eins.c;6;(no #else);def;= 100 (project);;;;\n") != std::string::npos);
        CHECK(csv.find(kStrich) == std::string::npos);  // reines ASCII
        CHECK(c2d::ausgabe::csv().schreibe(u, {}).find("TAKT;2;3;differs;project, section;\n") != std::string::npos);
        CHECK(c2d::ausgabe::svg().schreibe(s, {}).find("<svg") != std::string::npos);
        CHECK(c2d::ausgabe::drawio().schreibe(u, {}).find("mxGraphModel") != std::string::npos);

        const TabellenAnordnung a = ordneAn(s);
        CHECK(a.knotenOrte.size() == s.gesetzt.size() + s.zeilen.size());
        const TabellenAnordnung b = ordneAn(u);
        CHECK(b.knotenOrte.size() == u.zeilen.size());
        // nichts ueberlappt
        for (std::size_t i = 0; i < a.zellen.size(); ++i)
            for (std::size_t j = i + 1; j < a.zellen.size(); ++j)
            {
                const Rechteck& x = a.zellen[i].ort;
                const Rechteck& y = a.zellen[j].ort;
                CHECK_FALSE((x.x + 0.5 < y.x + y.breite && y.x + 0.5 < x.x + x.breite && x.y + 0.5 < y.y + y.hoehe &&
                             y.y + 0.5 < x.y + x.hoehe));
            }
    }
}
