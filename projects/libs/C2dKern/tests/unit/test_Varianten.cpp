/**
 * @file test_Varianten.cpp
 * @brief Bedingte Uebersetzung als Varianten: welche #if im Rumpf stehen bleiben, und wie sie gezeichnet werden
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using c2d::modell::Anweisung;
using c2d::modell::AnweisungArt;
using c2d::sprache::Ergebnis;
using c2d::sprache::LeseOptionen;
using namespace c2d::diagramm;

namespace
{

constexpr std::string_view kRegle = R"c(
void regle(int soll, int ist)
{
    ist = lies();
#ifdef DEBUG
    protokolliere(ist);
#endif
#if FILTER >= 2
    ist = glaette(ist);
    ist = filtere(ist);
#elif FILTER == 1
    ist = filtere(ist);
#else
    /* roh */
#endif
    if (ist < soll)
        heize();
#ifdef MIT_KUEHLUNG
    else if (ist > soll)
        kuehle();
#endif
    else
        halte();
    switch (soll) {
    case 1:
#if BOARD_A
        a();
#endif
        break;
#ifdef MIT_C
    case 3: c(); break;
#endif
    }
}
)c";

Ergebnis liesVarianten(std::string_view text, std::vector<std::string> definitionen = {})
{
    LeseOptionen optionen;
    optionen.varianten = true;
    for (const std::string& d : definitionen) optionen.definitionen.push_back(c2d::sprache::definitionAus(d));
    return c2d::sprache::c().lies(text, "t.c", optionen);
}

/// `Bedingung` je Zweig, mit `*` hinter dem, der gilt.
std::string zweige(const Anweisung& varianten)
{
    std::string s;
    for (const auto& z : varianten.kinder) s += (s.empty() ? "" : " | ") + z->text + (z->gilt ? "*" : "");
    return s;
}

std::vector<std::string> beschriftungenVon(const Flussdiagramm& d, KnotenId von)
{
    std::vector<std::string> aus;
    for (const Kante& k : d.kanten())
        if (k.von == von) aus.push_back(k.beschriftung);
    return aus;
}

}  // namespace

TEST_SUITE("Varianten")
{
    TEST_CASE("Ein #if im Rumpf, das ganze Anweisungen schaltet, bleibt mit allen Zweigen stehen")
    {
        const Ergebnis e = liesVarianten(kRegle);
        REQUIRE(e.einheit.funktionen.size() == 1);
        CHECK(e.einheit.funktionen[0].zeile == 2);  // die Zeilen bleiben die der Datei
        const Anweisung& rumpf = *e.einheit.funktionen[0].rumpf;
        REQUIRE(rumpf.kinder.size() == 5);

        const Anweisung& debug = *rumpf.kinder[1];
        REQUIRE(debug.art == AnweisungArt::Varianten);
        CHECK(debug.text == "#if");
        CHECK(debug.zeile == 5);
        CHECK(zweige(debug) == "defined(DEBUG) | else*");  // das fehlende #else: ein leerer Zweig, der gilt
        CHECK(debug.kinder[0]->art == AnweisungArt::Zweig);
        CHECK(debug.kinder[0]->kinder.size() == 1);
        CHECK(debug.kinder[1]->kinder.empty());

        const Anweisung& filter = *rumpf.kinder[2];
        REQUIRE(filter.art == AnweisungArt::Varianten);
        CHECK(zweige(filter) == "FILTER >= 2 | FILTER == 1 | else*");
        CHECK(filter.kinder[0]->kinder.size() == 2);
        CHECK(filter.kinder[1]->zeile == 11);
        CHECK(filter.kinder[2]->kinder.empty());  // nur ein Kommentar

        // das #if, das ein else if fortsetzt, wird ausgewertet: das else gehoert zum if davor
        REQUIRE(rumpf.kinder[3]->art == AnweisungArt::Wenn);
        CHECK(rumpf.kinder[3]->sonst->text == "halte()");

        // im switch: um eine Anweisung ja, um eine Fall-Marke nicht
        const Anweisung& faelle = *rumpf.kinder[4]->kinder[0];
        REQUIRE(faelle.kinder.size() == 3);
        CHECK(faelle.kinder[0]->art == AnweisungArt::Fall);
        CHECK(faelle.kinder[1]->art == AnweisungArt::Varianten);
        CHECK(zweige(*faelle.kinder[1]) == "BOARD_A | else*");
        CHECK(faelle.kinder[2]->art == AnweisungArt::Abbruch);

        // was nicht als Varianten steht: ein Hinweis mit Zeile
        REQUIRE(e.meldungen.size() == 2);
        CHECK(e.meldungen[0].schwere == c2d::quelle::Schwere::Hinweis);
        CHECK(e.meldungen[0].ort.zeile == 18);
        CHECK(e.meldungen[0].text == "#if not shown in the flow: a branch does not hold whole statements");
        CHECK(e.meldungen[1].ort.zeile == 30);
        CHECK(e.unbekannteNamen == std::vector<std::string>{"DEBUG", "FILTER", "MIT_KUEHLUNG", "BOARD_A", "MIT_C"});
    }

    TEST_CASE("Welcher Zweig gilt, folgt den Definitionen")
    {
        const Ergebnis e = liesVarianten(kRegle, {"DEBUG", "FILTER=1", "MIT_C"});
        const Anweisung& rumpf = *e.einheit.funktionen[0].rumpf;
        CHECK(zweige(*rumpf.kinder[1]) == "defined(DEBUG)* | else");
        CHECK(zweige(*rumpf.kinder[2]) == "FILTER >= 2 | FILTER == 1* | else");
        // MIT_C gilt: die Fall-Marke steht im switch, ausgewertet
        CHECK(rumpf.kinder[4]->kinder[0]->kinder.size() == 6);
    }

    TEST_CASE("Ohne den Schalter: ausgewertet wie bisher, ohne Hinweise")
    {
        const Ergebnis e = c2d::sprache::c().lies(kRegle, "t.c");
        CHECK(e.meldungen.empty());
        const Anweisung& rumpf = *e.einheit.funktionen[0].rumpf;
        CHECK(rumpf.kinder.size() == 3);  // lies, if, switch
        CHECK(std::none_of(rumpf.kinder.begin(), rumpf.kinder.end(),
                           [](const auto& a) { return a->art == AnweisungArt::Varianten; }));

        // alle Zweige geht vor
        LeseOptionen beide;
        beide.alleZweige = true;
        beide.varianten = true;
        const Ergebnis alle = c2d::sprache::c().lies("void f(void)\n{\n#if A\n  a();\n#endif\n}\n", "t.c", beide);
        CHECK(alle.einheit.funktionen[0].rumpf->kinder.size() == 3);  // #if, a(), #endif
    }

    TEST_CASE("Verschachtelt: ein #if in einem Zweig wird selbst Varianten")
    {
        constexpr std::string_view kText = "void g(void)\n{\n#if A\n    eins();\n#  ifdef B\n    zwei();\n#  endif\n"
                                           "#else\n    drei();\n#endif\n}\n";
        const Ergebnis ohne = liesVarianten(kText);
        CHECK(ohne.meldungen.empty());
        const Anweisung& aussen = *ohne.einheit.funktionen[0].rumpf->kinder[0];
        REQUIRE(aussen.art == AnweisungArt::Varianten);
        CHECK(zweige(aussen) == "A | else*");
        const Anweisung& a = *aussen.kinder[0];
        REQUIRE(a.kinder.size() == 2);
        CHECK(a.kinder[1]->art == AnweisungArt::Varianten);
        CHECK(zweige(*a.kinder[1]) == "defined(B) | else");  // A gilt nicht: hier gilt keiner

        const Ergebnis mitA = liesVarianten(kText, {"A"});
        const Anweisung& innen = *mitA.einheit.funktionen[0].rumpf->kinder[0]->kinder[0]->kinder[1];
        CHECK(zweige(innen) == "defined(B) | else*");

        // ein #if darin, das nicht passt, laesst auch das umgebende nicht zu
        const Ergebnis kaputt = liesVarianten("void h(int x)\n{\n#if A\n  if (x) a();\n#  ifdef B\n  else b();\n"
                                              "#  endif\n#endif\n}\n");
        CHECK(kaputt.meldungen.size() == 1);  // das innere steht in einem Zweig, der nicht gilt: niemand sieht es
        CHECK(kaputt.einheit.funktionen[0].rumpf->kinder.empty());  // A gilt nicht
    }

    TEST_CASE("Was nicht passt: Rumpf eines if ohne Klammern, offene Klammern, ausserhalb einer Funktion")
    {
        // ohne A waere der Rumpf des if b() — als Varianten waere das falsch
        const Ergebnis rumpf = liesVarianten("void f(int x)\n{\n  if (x)\n#ifdef A\n    a();\n#endif\n  b();\n}\n");
        REQUIRE(rumpf.meldungen.size() == 1);
        CHECK(rumpf.meldungen[0].ort.zeile == 4);
        const Anweisung& wenn = *rumpf.einheit.funktionen[0].rumpf->kinder[0];
        CHECK(wenn.kinder[0]->text == "b()");

        // je Zweig eine eigene Klammer: nicht geschlossen
        const Ergebnis klammer = liesVarianten("void f(int x)\n{\n#ifdef A\n  if (x) {\n#else\n  if (!x) {\n#endif\n"
                                               "    tu();\n  }\n}\n");
        CHECK(klammer.meldungen.size() == 1);
        CHECK(klammer.einheit.funktionen[0].rumpf->kinder[0]->text == "!x");

        // ausserhalb eines Rumpfs: ausgewertet, ohne Hinweis
        const Ergebnis datei = liesVarianten("#ifdef A\nvoid a(void) {}\n#else\nvoid b(void) {}\n#endif\n");
        CHECK(datei.meldungen.empty());
        REQUIRE(datei.einheit.funktionen.size() == 1);
        CHECK(datei.einheit.funktionen[0].name == "b");
    }

    TEST_CASE("Im Flussdiagramm: ein Sechseck, je Zweig eine Kante mit der Bedingung")
    {
        const Ergebnis e = liesVarianten(kRegle, {"FILTER=2"});
        const Flussdiagramm d = flussVon(e.einheit.funktionen[0]);
        std::vector<KnotenId> sechsecke;
        for (KnotenId i = 0; i < d.knoten().size(); ++i)
            if (d.knoten()[i].art == KnotenArt::Variante) sechsecke.push_back(i);
        REQUIRE(sechsecke.size() == 3);
        CHECK(d.knoten()[sechsecke[0]].zeilen == std::vector<std::string>{"#if"});
        CHECK(d.knoten()[sechsecke[0]].aufrufe.empty());
        CHECK(beschriftungenVon(d, sechsecke[0]) == std::vector<std::string>{"defined(DEBUG)", "else (active)"});
        CHECK(beschriftungenVon(d, sechsecke[1]) ==
              std::vector<std::string>{"FILTER >= 2 (active)", "FILTER == 1", "else"});
        // der leere Zweig fuehrt gleich weiter: zum naechsten Sechseck
        const auto zuZweitem = std::count_if(d.kanten().begin(), d.kanten().end(), [&](const Kante& k) {
            return k.von == sechsecke[0] && k.nach == sechsecke[1];
        });
        CHECK(zuZweitem == 1);

        // die Aufrufe in den Zweigen zaehlen, die Bedingungen nicht
        const std::vector<std::string> gerufen = c2d::modell::aufrufeVon(e.einheit.funktionen[0]);
        CHECK(std::find(gerufen.begin(), gerufen.end(), "protokolliere") != gerufen.end());
        CHECK(std::find(gerufen.begin(), gerufen.end(), "defined") == gerufen.end());
    }

    TEST_CASE("Alle Formate kennen das Sechseck; die Anordnung gibt ihm Platz")
    {
        const Ergebnis e = liesVarianten(kRegle);
        const Flussdiagramm d = fasseAnweisungsfolgenZusammen(flussVon(e.einheit.funktionen[0]));
        const c2d::ausgabe::Optionen optionen;
        const std::string mermaid = c2d::ausgabe::mermaid().schreibe(d, optionen);
        CHECK(mermaid.find("{{\"#35;if\"}}") != std::string::npos);
        CHECK(mermaid.find("else (active)") != std::string::npos);
        CHECK(c2d::ausgabe::dot().schreibe(d, optionen).find("shape=hexagon") != std::string::npos);
        CHECK(c2d::ausgabe::drawio().schreibe(d, optionen).find("shape=hexagon") != std::string::npos);
        CHECK(c2d::ausgabe::svg().schreibe(d, optionen).find("defined(DEBUG)") != std::string::npos);

        const FlussAnordnung a = ordneAn(d, {});
        for (std::size_t i = 0; i < a.knoten.size(); ++i)
            for (std::size_t j = i + 1; j < a.knoten.size(); ++j)
            {
                const Rechteck& p = a.knoten[i];
                const Rechteck& q = a.knoten[j];
                CHECK_FALSE((p.x < q.x + q.breite && q.x < p.x + p.breite && p.y < q.y + q.hoehe && q.y < p.y + p.hoehe));
            }

        // eine lange Kante neben einer Schlaufe an sich selbst laeuft rechts an ihr und ihrer Beschriftung vorbei
        // (gefunden an LLC, CLEAR_RAM_CPU1: das leere else ging durch `while (!fertig());`)
        // die Raute ist breiter als alles darueber: sonst laege die Spalte der langen Kante zufaellig weit genug rechts
        const Ergebnis schlaufe = liesVarianten(
            "void f(void)\n{\n#ifdef A\n    a();\n    while (!pruefe_ob_der_speicher_bereit_ist(SEKTION_D0 | SEKTION_D1));\n"
            "#endif\n    c();\n}\n");
        const Flussdiagramm fs = flussVon(schlaufe.einheit.funktionen[0]);
        const FlussAnordnung as = ordneAn(fs, {});
        std::size_t selbst = fs.kanten().size(), lang = fs.kanten().size();
        for (std::size_t k = 0; k < fs.kanten().size(); ++k)
        {
            if (fs.kanten()[k].von == fs.kanten()[k].nach) selbst = k;
            if (fs.kanten()[k].beschriftung == "else (active)") lang = k;
        }
        REQUIRE(selbst < fs.kanten().size());
        REQUIRE(lang < fs.kanten().size());
        const Rechteck& kopf = as.knoten[fs.kanten()[selbst].von];
        double schlaufeRechts = 0;
        for (const Punkt& p : as.kanten[selbst]) schlaufeRechts = std::max(schlaufeRechts, p.x);
        schlaufeRechts += 3 * 7.2 / 2;  // die Beschriftung `yes` steht mittig auf der Schlaufe
        const Linienzug& l = as.kanten[lang];
        for (std::size_t i = 0; i + 1 < l.size(); ++i)
        {
            const bool senkrecht = std::abs(l[i].x - l[i + 1].x) < 0.5;
            const double oben = std::min(l[i].y, l[i + 1].y), unten = std::max(l[i].y, l[i + 1].y);
            if (senkrecht && oben < kopf.y + kopf.hoehe && unten > kopf.y) CHECK(l[i].x > schlaufeRechts);
        }

        const std::vector<Punkt> ecken = sechseck({0, 0, 100, 40});
        REQUIRE(ecken.size() == 6);
        CHECK(ecken[0].x == doctest::Approx(kSechseckSpitze));
        CHECK(ecken[2].y == doctest::Approx(20));
    }
}
