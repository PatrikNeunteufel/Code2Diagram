/**
 * @file test_Rahmen.cpp
 * @brief Rahmen um das, was unter einer Bedingung der Uebersetzung steht: lesen, ableiten, buendeln, einbetten, zeichnen
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>
#include <vector>

using c2d::modell::Anweisung;
using c2d::modell::AnweisungArt;
using c2d::modell::volleBedingung;
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
    else
        halte();
    switch (soll) {
    case 1:
#if BOARD_A
        a();
#endif
        break;
    }
#ifdef DEBUG
    protokolliere(soll);
#endif
}
)c";

Ergebnis lies(std::string_view text, bool varianten, std::vector<std::string> definitionen = {})
{
    LeseOptionen optionen;
    optionen.varianten = varianten;
    optionen.bereiche = !varianten;
    for (const std::string& d : definitionen) optionen.definitionen.push_back(c2d::sprache::definitionAus(d));
    return c2d::sprache::c().lies(text, "t.c", optionen);
}

/// Die Titel der Rahmen von Bedingungen, in ihrer Reihenfolge.
std::vector<std::string> rahmen(const Flussdiagramm& d)
{
    std::vector<std::string> aus;
    for (const Gruppe& g : d.gruppen())
        if (g.art == GruppenArt::Bedingung) aus.push_back(g.titel);
    return aus;
}

/// Die erste Zeile jedes Knotens in der Gruppe g.
std::vector<std::string> inhalt(const Flussdiagramm& d, std::size_t g)
{
    std::vector<std::string> aus;
    for (const Knoten& k : d.knoten())
        if (k.gruppe == g) aus.push_back(k.zeilen.front());
    return aus;
}

}  // namespace

TEST_SUITE("Rahmen")
{
    TEST_CASE("volleBedingung: keiner der Zweige davor, und er selbst")
    {
        const std::vector<std::string> kette{"defined(A)", "B", "else"};
        CHECK(volleBedingung(kette, 0) == "defined(A)");
        CHECK(volleBedingung(kette, 1) == "!defined(A) && B");
        CHECK(volleBedingung(kette, 2) == "!defined(A) && !B");
        CHECK(volleBedingung({"!defined(K)"}, 1) == "defined(K)");  // das fehlende #else
        CHECK(volleBedingung({"A || B"}, 1) == "!(A || B)");
        CHECK(volleBedingung({"X > 1", "Y || Z"}, 1) == "!(X > 1) && (Y || Z)");
    }

    TEST_CASE("Ausgewertet mit Bereichen: nur der geltende Zweig, als Bedingt mit der vollen Bedingung")
    {
        const Ergebnis mit = lies(kRegle, false, {"DEBUG", "FILTER=1"});
        CHECK(mit.meldungen.empty());
        const Anweisung& rumpf = *mit.einheit.funktionen[0].rumpf;
        REQUIRE(rumpf.kinder.size() == 6);
        CHECK(rumpf.kinder[1]->art == AnweisungArt::Bedingt);
        CHECK(rumpf.kinder[1]->text == "defined(DEBUG)");
        CHECK(rumpf.kinder[1]->zeile == 5);
        CHECK(rumpf.kinder[2]->text == "!(FILTER >= 2) && FILTER == 1");
        REQUIRE(rumpf.kinder[2]->kinder.size() == 1);
        CHECK(rumpf.kinder[2]->kinder[0]->text == "ist = filtere(ist)");
        CHECK(rumpf.kinder[4]->kinder[0]->kinder.size() == 2);  // case 1, break: BOARD_A gilt nicht
        CHECK(rumpf.kinder[5]->text == "defined(DEBUG)");

        // gilt nur ein leerer Zweig, bleibt nichts — wie beim Auswerten
        const Ergebnis ohne = lies(kRegle, false);
        CHECK(ohne.einheit.funktionen[0].rumpf->kinder.size() == 3);  // lies, if, switch
        CHECK(ohne.unbekannteNamen == std::vector<std::string>{"DEBUG", "FILTER", "BOARD_A"});
    }

    TEST_CASE("Im Ablauf: je Bereich ein Rahmen; gleiche Bedingung, gleiche Farbe")
    {
        const Ergebnis e = lies(kRegle, false, {"DEBUG", "FILTER=1", "BOARD_A"});
        const Flussdiagramm d = flussVon(e.einheit.funktionen[0]);
        CHECK(rahmen(d) == std::vector<std::string>{"defined(DEBUG)", "!(FILTER >= 2) && FILTER == 1", "BOARD_A",
                                                    "defined(DEBUG)"});
        CHECK(d.einbettungen() == 0);
        CHECK(inhalt(d, 0) == std::vector<std::string>{"protokolliere(ist)"});
        CHECK(inhalt(d, 1) == std::vector<std::string>{"ist = filtere(ist)"});
        CHECK(inhalt(d, 3) == std::vector<std::string>{"protokolliere(soll)"});
        CHECK(d.knoten().front().gruppe == kKeine);  // der Start und der Rest stehen draussen

        const std::vector<std::size_t> farben = farbenDerRahmen(d);
        CHECK(farben == std::vector<std::size_t>{0, 1, 2, 0});
        CHECK(rahmenFarbe(0) != rahmenFarbe(1));
        CHECK(rahmenFarbe(kRahmenFarben) == rahmenFarbe(0));
    }

    TEST_CASE("Mit Varianten: je Zweig mit Knoten ein Rahmen, das Sechseck steht draussen")
    {
        const Ergebnis e = lies(kRegle, true, {"FILTER=2"});
        const Flussdiagramm d = flussVon(e.einheit.funktionen[0]);
        CHECK(rahmen(d) == std::vector<std::string>{"defined(DEBUG)", "FILTER >= 2", "!(FILTER >= 2) && FILTER == 1",
                                                    "BOARD_A", "defined(DEBUG)"});
        for (const Knoten& k : d.knoten())
            if (k.art == KnotenArt::Variante) CHECK(k.gruppe == kKeine);
        CHECK(inhalt(d, 1) == std::vector<std::string>{"ist = glaette(ist)", "ist = filtere(ist)"});
    }

    TEST_CASE("Verschachtelt: der innere Rahmen liegt im aeusseren")
    {
        const Ergebnis e = lies("void g(void)\n{\n#if A\n    eins();\n#  ifdef B\n    zwei();\n#  endif\n#endif\n}\n",
                                false, {"A", "B"});
        const Flussdiagramm d = flussVon(e.einheit.funktionen[0]);
        REQUIRE(d.gruppen().size() == 2);
        CHECK(d.gruppen()[0].titel == "A");
        CHECK(d.gruppen()[1].titel == "defined(B)");
        CHECK(d.gruppen()[1].eltern == 0);
        CHECK(d.liegtIn(1, 0));
    }

    TEST_CASE("Buendeln haelt sich an die Rahmen; Einbetten nimmt sie mit, Herausloesen auch")
    {
        const Ergebnis e = lies("void f(void)\n{\n  x = 1;\n#ifdef X\n  y = 2;\n  z = 3;\n#endif\n  w = 4;\n  g();\n}\n"
                                "void g(void)\n{\n#ifdef X\n  v = 5;\n#endif\n}\n",
                                false, {"X"});
        const Flussdiagramm f = fasseAnweisungsfolgenZusammen(flussVon(e.einheit.funktionen[0]));
        std::vector<std::vector<std::string>> zeilen;
        for (const Knoten& k : f.knoten())
            if (k.art == KnotenArt::Anweisung) zeilen.push_back(k.zeilen);
        CHECK(zeilen == std::vector<std::vector<std::string>>{{"x = 1"}, {"y = 2", "z = 3"}, {"w = 4"}});

        KnotenId ruf = kKeine;
        for (KnotenId i = 0; i < f.knoten().size(); ++i)
            if (f.knoten()[i].art == KnotenArt::Aufruf) ruf = i;
        REQUIRE(ruf != kKeine);
        const Flussdiagramm mit = betteEin(f, ruf, flussVon(e.einheit.funktionen[1]), "g(void)");
        REQUIRE(mit.gruppen().size() == 3);
        CHECK(mit.einbettungen() == 1);
        CHECK(mit.gruppen()[1].art == GruppenArt::Einbettung);
        CHECK(mit.gruppen()[2].titel == "defined(X)");
        CHECK(mit.gruppen()[2].eltern == 1);
        CHECK(mit.einbettungVon(2) == 1);
        CHECK(mit.einbettungVon(0) == kKeine);

        const Flussdiagramm ohne = loeseEin(mit, 1);
        CHECK(ohne.gruppen().size() == 1);
        CHECK(rahmen(ohne) == std::vector<std::string>{"defined(X)"});
    }

    TEST_CASE("Gezeichnet: der Rahmen umfasst seine Knoten und keine fremden; rechts die Legende")
    {
        const Ergebnis e = lies(kRegle, true, {"FILTER=2"});
        const Flussdiagramm d = fasseAnweisungsfolgenZusammen(flussVon(e.einheit.funktionen[0]));
        const FlussAnordnung a = ordneAn(d, {});
        const auto innen = [](const Rechteck& r, const Rechteck& rahmen) {
            return r.x >= rahmen.x && r.y >= rahmen.y && r.x + r.breite <= rahmen.x + rahmen.breite &&
                   r.y + r.hoehe <= rahmen.y + rahmen.hoehe;
        };
        const auto ueberlappt = [](const Rechteck& p, const Rechteck& q) {
            return p.x < q.x + q.breite && q.x < p.x + p.breite && p.y < q.y + q.hoehe && q.y < p.y + p.hoehe;
        };
        for (std::size_t g = 0; g < d.gruppen().size(); ++g)
            for (KnotenId i = 0; i < d.knoten().size(); ++i)
            {
                if (d.knoten()[i].gruppe != kKeine && d.liegtIn(d.knoten()[i].gruppe, g))
                    CHECK(innen(a.knoten[i], a.gruppen[g]));
                else
                    CHECK_FALSE(ueberlappt(a.knoten[i], a.gruppen[g]));
            }

        // die Legende: je Bedingung eine Zeile, rechts neben allem
        REQUIRE(a.legende.size() == 4);  // defined(DEBUG) zweimal im Ablauf, einmal in der Legende
        double rechts = 0;
        for (const Rechteck& r : a.knoten) rechts = std::max(rechts, r.x + r.breite);
        for (const Rechteck& r : a.gruppen) rechts = std::max(rechts, r.x + r.breite);
        for (const FlussAnordnung::Legende& l : a.legende)
        {
            CHECK(l.feld.x > rechts);
            CHECK(l.text.x > l.feld.x + l.feld.breite);
        }
        CHECK(a.breite > a.legende.front().text.x);
    }

    TEST_CASE("Eine Schlaufe an sich selbst liegt mit ihrer Beschriftung im Rahmen; ein langer Titel wird gekuerzt")
    {
        // gefunden an LLC, CLEAR_RAM_CPU1: `while (!fertig());` unter `#ifdef _FLASH`
        const Ergebnis e = lies("void f(void)\n{\n#ifdef EIN_SEHR_LANGER_NAME_FUER_DEN_SCHALTER\n"
                                "    while (!b());\n#endif\n}\n",
                                false, {"EIN_SEHR_LANGER_NAME_FUER_DEN_SCHALTER"});
        const Flussdiagramm d = flussVon(e.einheit.funktionen[0]);
        const FlussAnordnung a = ordneAn(d, {});
        REQUIRE(a.gruppen.size() == 1);
        const Rechteck& r = a.gruppen[0];
        std::size_t schlaufen = 0;
        for (std::size_t k = 0; k < d.kanten().size(); ++k)
        {
            if (d.kanten()[k].von != d.kanten()[k].nach) continue;
            ++schlaufen;
            for (const Punkt& p : a.kanten[k]) CHECK((p.x <= r.x + r.breite && p.x >= r.x));
            CHECK(a.beschriftungen[k].x + 3 * 7.2 / 2 <= r.x + r.breite);
        }
        CHECK(schlaufen == 1);
        // der Titel passt nicht ganz: so viele Zeichen, wie Platz ist
        const std::size_t passt = titelZeichen(r, {});
        CHECK(passt < d.gruppen()[0].titel.size());
        CHECK(static_cast<double>(passt) * 7.2 <= r.breite - 16);
    }

    TEST_CASE("Alle Formate tragen die Farbe; Einbettungen bleiben gestrichelt")
    {
        const Ergebnis e = lies(kRegle, false, {"DEBUG"});
        const Flussdiagramm d = flussVon(e.einheit.funktionen[0]);
        const c2d::ausgabe::Optionen o;
        const std::string blau(rahmenFarbe(0));
        const std::string mermaid = c2d::ausgabe::mermaid().schreibe(d, o);
        CHECK(mermaid.find("subgraph g0[\"defined(DEBUG)\"]") != std::string::npos);
        CHECK(mermaid.find("style g0 fill:" + blau) != std::string::npos);
        const std::string dot = c2d::ausgabe::dot().schreibe(d, o);
        CHECK(dot.find("color=\"" + blau + "\"") != std::string::npos);
        CHECK(dot.find("style=dashed") == std::string::npos);
        CHECK(c2d::ausgabe::drawio().schreibe(d, o).find("strokeColor=" + blau) != std::string::npos);
        const std::string svg = c2d::ausgabe::svg().schreibe(d, o);
        CHECK(svg.find("stroke:" + blau) != std::string::npos);
    }
}
