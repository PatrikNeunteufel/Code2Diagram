/**
 * @file test_Diagramme.cpp
 * @brief Die Diagramm-Modelle: Buendeln, Ablauf einer Funktion, Datentypen
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "Beispiele.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <stdexcept>

using namespace c2d::diagramm;

namespace
{

/// Das Flussdiagramm der ersten Funktion mit Rumpf, ungebuendelt.
Flussdiagramm fluss(std::string_view quelltext)
{
    const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(quelltext, "test.c");
    for (const c2d::modell::Funktion& f : e.einheit.funktionen)
        if (f.rumpf) return flussVon(f);
    FAIL("no function with a body");
    return Flussdiagramm();
}

bool hatKante(const Flussdiagramm& d, KnotenId von, KnotenId nach, std::string_view beschriftung = {})
{
    for (const Kante& k : d.kanten())
        if (k.von == von && k.nach == nach && k.beschriftung == beschriftung) return true;
    return false;
}

/// Die Knoten als "Art:Text", durch Strichpunkt getrennt.
std::string knotenBild(const Flussdiagramm& d)
{
    static constexpr char kKuerzel[] = {'S', 'E', 'A', 'V', 'R'};
    std::string s;
    for (const Knoten& k : d.knoten())
    {
        s += kKuerzel[static_cast<int>(k.art)];
        s += ':';
        for (std::size_t i = 0; i < k.zeilen.size(); ++i) s += (i ? "/" : "") + k.zeilen[i];
        s += ';';
    }
    return s;
}

}  // namespace

TEST_SUITE("Flussdiagramm")
{
    TEST_CASE("Knoten bekommen fortlaufende Kennungen, Kanten pruefen ihre Enden")
    {
        Flussdiagramm d("f");
        const KnotenId a = d.neuerKnoten(KnotenArt::Start, "f()", 3);
        const KnotenId b = d.neuerKnoten(KnotenArt::Ende, "End");
        d.verbinde(a, b, "x");

        CHECK(a == 0);
        CHECK(b == 1);
        CHECK(d.knoten()[a].quellZeile == 3);
        REQUIRE(d.kanten().size() == 1);
        CHECK(d.kanten()[0].beschriftung == "x");
        CHECK_THROWS_AS(d.verbinde(a, 7), std::out_of_range);
    }

    TEST_CASE("Ablauf: Zuweisungen, if/else, while (beispiele/verzweigung.c)")
    {
        const Flussdiagramm d = fluss(beispiele::kSummeBis);

        CHECK(knotenBild(d) == "S:summe_bis(int grenze);A:int summe = 0;A:int i = 1;V:grenze < 0;A:return -1;"
                               "V:i <= grenze;A:summe += i;A:i++;A:return summe;E:End;");
        CHECK(d.knoten()[3].quellZeile == 6);
        CHECK(d.kanten().size() == 11);
        CHECK(hatKante(d, 3, 4, "yes"));
        CHECK(hatKante(d, 3, 5, "no"));
        CHECK(hatKante(d, 5, 6, "yes"));
        CHECK(hatKante(d, 7, 5));  // zurueck zum Kopf der Schleife
        CHECK(hatKante(d, 5, 8, "no"));
        CHECK(hatKante(d, 4, 9));  // beide return muenden im Ende
        CHECK(hatKante(d, 8, 9));
    }

    TEST_CASE("Ablauf: for mit continue und break, Aufruf als eigene Knotenart")
    {
        const Flussdiagramm d = fluss(R"c(
void f(int n)
{
    for (int i = 0; i < n; i++) {
        if (i == 3) continue;
        if (i == 7) break;
        melde(i);
    }
    fertig();
})c");
        CHECK(knotenBild(d) == "S:f(int n);A:int i = 0;V:i < n;V:i == 3;V:i == 7;R:melde(i);A:i++;R:fertig();E:End;");
        CHECK(hatKante(d, 3, 6, "yes"));  // continue: zum Schritt, nicht zum Kopf
        CHECK(hatKante(d, 5, 6));
        CHECK(hatKante(d, 6, 2));
        CHECK(hatKante(d, 4, 7, "yes"));  // break: hinter die Schleife
        CHECK(hatKante(d, 2, 7, "no"));
        CHECK(d.kanten().size() == 11);
    }

    TEST_CASE("Ablauf: for ohne Bedingung laeuft, bis ein break es verlaesst")
    {
        const Flussdiagramm d = fluss("void f(void) { for (;;) { a(); if (x) break; } b(); }");

        CHECK(knotenBild(d) == "S:f();R:a();V:x;R:b();E:End;");
        CHECK(hatKante(d, 2, 1, "no"));  // zurueck zum Anfang des Rumpfs
        CHECK(hatKante(d, 2, 3, "yes"));
        CHECK(d.kanten().size() == 5);
    }

    TEST_CASE("Ablauf: switch mit Durchfallen, do-while, goto und Marken")
    {
        const Flussdiagramm d = fluss(R"c(
int zustand(int z)
{
    switch (z) {
    case 0:
    case 1:
        z++;
        break;
    case 2:
        z = 5;
    default:
        z = 0;
    }
    do {
        z--;
    } while (z > 0);
nochmal:
    if (z < -3) goto ende; else z -= 1;
    goto nochmal;
ende:
    return z;
})c");
        CHECK(knotenBild(d) == "S:zustand(int z);V:z;A:z++;A:z = 5;A:z = 0;A:z--;V:z > 0;A:nochmal:;V:z < -3;"
                               "A:z -= 1;A:ende:;A:return z;E:End;");
        CHECK(d.kanten().size() == 16);
        CHECK(hatKante(d, 1, 2, "0, 1"));  // zwei Faelle nebeneinander: eine Kante
        CHECK(hatKante(d, 1, 3, "2"));
        CHECK(hatKante(d, 3, 4));  // case 2 faellt in default durch
        CHECK(hatKante(d, 1, 4, "default"));
        CHECK(hatKante(d, 2, 5));  // break: hinter den switch
        CHECK(hatKante(d, 6, 5, "yes"));
        CHECK(hatKante(d, 6, 7, "no"));
        CHECK(hatKante(d, 8, 10, "yes"));  // goto ende
        CHECK(hatKante(d, 9, 7));          // goto nochmal
    }

    TEST_CASE("Ablauf: switch ohne default bekommt die Kante otherwise")
    {
        const Flussdiagramm d = fluss("void f(int z) { switch (z) { case 1: a = 1; break; } }");

        CHECK(hatKante(d, 1, 2, "1"));
        CHECK(hatKante(d, 1, 3, "otherwise"));
        CHECK(hatKante(d, 2, 3));
    }

    TEST_CASE("Kehrt eine Funktion nie zurueck, hat ihr Ablauf kein Ende; der Start sagt es")
    {
        // eine Hauptschleife; das return dahinter wird nie erreicht
        const Flussdiagramm d = fluss("int main(void) { init(); for (;;) { takt(); } return 0; }");
        CHECK(std::none_of(d.knoten().begin(), d.knoten().end(), [](const Knoten& k) { return k.art == KnotenArt::Ende; }));
        REQUIRE(d.knoten()[0].zeilen.size() == 2);
        CHECK(d.knoten()[0].zeilen[1] == kKehrtNieZurueck);

        // for (;;) ohne break ebenso; mit break gibt es ein Ende, ohne Zusatz
        CHECK(fluss("void f(void) { for (;;) { a(); } }").knoten()[0].zeilen.size() == 2);
        const Flussdiagramm mitBreak = fluss("void f(void) { for (;;) { if (x) break; } }");
        CHECK(mitBreak.knoten()[0].zeilen.size() == 1);
        CHECK(mitBreak.knoten().back().art == KnotenArt::Ende);

        // ein return in der Schleife fuehrt hinaus
        const Flussdiagramm mitReturn = fluss("int f(void) { for (;;) { if (x) return 1; } }");
        CHECK(mitReturn.knoten().back().art == KnotenArt::Ende);
        CHECK(mitReturn.knoten()[0].zeilen.size() == 1);
    }

    TEST_CASE("Code, der nie erreicht wird, bleibt — gekennzeichnet, ohne Kante ins Ende")
    {
        const Flussdiagramm d = fluss(
            "int f(void)\n"         // 1
            "{\n"                   // 2
            "    if (x) return 1;\n"  // 3
            "    for (;;) { a(); }\n"  // 4
            "    b();\n"            // 5
            "    return 0;\n"       // 6
            "}\n");
        std::vector<KnotenId> tot;
        for (KnotenId k = 0; k < d.knoten().size(); ++k)
            if (d.knoten()[k].unerreichbar) tot.push_back(k);
        REQUIRE(tot.size() == 2);  // b() und return 0
        CHECK(d.knoten()[tot[0]].zeilen == std::vector<std::string>{std::string(kUnerreichbar), "b()"});
        CHECK(d.knoten()[tot[1]].zeilen == std::vector<std::string>{"return 0"});
        CHECK(unerreichbarAb(d) == 5);
        // ein Ende gibt es (return 1), aber nicht vom toten Code
        const KnotenId ende = d.knoten().size() - 1;
        REQUIRE(d.knoten()[ende].art == KnotenArt::Ende);
        CHECK(std::none_of(d.kanten().begin(), d.kanten().end(),
                           [&](const Kante& k) { return k.nach == ende && d.knoten()[k.von].unerreichbar; }));
        CHECK(d.knoten()[0].zeilen.size() == 1);  // kehrt zurueck: kein (never returns)

        // gebuendelt bleibt es gekennzeichnet; in der Anordnung steht es unter allem anderen
        const Flussdiagramm b = fasseAnweisungsfolgenZusammen(d);
        const FlussAnordnung a = ordneAn(b, {});
        double tiefsteErreichte = 0;
        double hoechsteTote = 1e9;
        for (KnotenId k = 0; k < b.knoten().size(); ++k)
        {
            if (b.knoten()[k].unerreichbar)
                hoechsteTote = std::min(hoechsteTote, a.knoten[k].y);
            else
                tiefsteErreichte = std::max(tiefsteErreichte, a.knoten[k].y + a.knoten[k].hoehe);
        }
        CHECK(std::any_of(b.knoten().begin(), b.knoten().end(), [](const Knoten& k) { return k.unerreichbar; }));
        CHECK(hoechsteTote > tiefsteErreichte);

        // ohne toten Code: nichts gekennzeichnet
        CHECK(unerreichbarAb(fluss("int g(void) { return 1; }")) == 0);
    }

    TEST_CASE("Eine Schleifenbedingung, die immer wahr ist, hat keinen Weg no hinaus")
    {
        const auto nein = [](const Flussdiagramm& d) {
            return std::count_if(d.kanten().begin(), d.kanten().end(), [](const Kante& k) { return k.beschriftung == "no"; });
        };
        for (const char* immer : {"1", "(1)", "true", "0x1", "1u", "2UL", " 1 "})
        {
            CAPTURE(immer);
            const Flussdiagramm d = fluss(std::string("void f(void) { while (") + immer + ") { a(); } }");
            CHECK(nein(d) == 0);
            CHECK(d.knoten()[0].zeilen.size() == 2);  // never returns
        }
        // do-while und for ebenso
        CHECK(nein(fluss("void f(void) { do { a(); } while (1); }")) == 0);
        CHECK(nein(fluss("void f(void) { for (i = 0; 1; i++) { a(); } }")) == 0);
        // was nicht sicher immer wahr ist, behaelt no; ein Makro wird nicht aufgeloest
        for (const char* offen : {"0", "x", "TRUE", "1 == 1", "0x0", "1.0", "10x"})
        {
            CAPTURE(offen);
            CHECK(nein(fluss(std::string("void f(void) { while (") + offen + ") { a(); } }")) == 1);
        }
        // mit break bleibt ein Ende, aber kein no
        const Flussdiagramm mitBreak = fluss("void f(void) { while (1) { if (x) break; } }");
        CHECK(nein(mitBreak) == 1);  // nur das no des if
        CHECK(mitBreak.knoten().back().art == KnotenArt::Ende);
    }

    TEST_CASE("Eine Funktion ohne Rumpf gibt ein leeres Diagramm")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies("int f(int a);", "test.c");
        REQUIRE(e.einheit.funktionen.size() == 1);
        CHECK(flussVon(e.einheit.funktionen[0]).knoten().empty());
    }

    TEST_CASE("Buendeln: aufeinanderfolgende Anweisungen werden ein Knoten")
    {
        const Flussdiagramm d = fasseAnweisungsfolgenZusammen(fluss(beispiele::kSummeBis));

        // zehn Knoten werden acht: summe/i und summe+=i/i++ gehen je zusammen
        CHECK(knotenBild(d) == "S:summe_bis(int grenze);A:int summe = 0/int i = 1;V:grenze < 0;A:return -1;"
                               "V:i <= grenze;A:summe += i/i++;A:return summe;E:End;");
        CHECK(d.knoten()[1].quellZeile == 4);
        CHECK(d.kanten().size() == 9);  // elf Kanten, zwei sind aufgegangen
    }

    TEST_CASE("Buendeln: nichts geht ueber eine Verzweigung oder eine beschriftete Kante")
    {
        Flussdiagramm d;
        const KnotenId a = d.neuerKnoten(KnotenArt::Anweisung, "a");
        const KnotenId frage = d.neuerKnoten(KnotenArt::Verzweigung, "x");
        const KnotenId b = d.neuerKnoten(KnotenArt::Anweisung, "b");
        const KnotenId c = d.neuerKnoten(KnotenArt::Anweisung, "c");
        d.verbinde(a, frage);
        d.verbinde(frage, b, "yes");
        d.verbinde(frage, c, "no");
        d.verbinde(b, c);  // c hat zwei Eingaenge

        const Flussdiagramm neu = fasseAnweisungsfolgenZusammen(d);
        CHECK(neu.knoten().size() == 4);
        CHECK(neu.kanten().size() == 4);
    }

    TEST_CASE("Buendeln: ein Knoten traegt hoechstens maxZeilen Zeilen")
    {
        Flussdiagramm d;
        KnotenId davor = d.neuerKnoten(KnotenArt::Anweisung, "s0");
        for (int i = 1; i < 5; ++i)
        {
            const KnotenId k = d.neuerKnoten(KnotenArt::Anweisung, "s" + std::to_string(i));
            d.verbinde(davor, k);
            davor = k;
        }

        const Flussdiagramm zwei = fasseAnweisungsfolgenZusammen(d, 2);
        CHECK(knotenBild(zwei) == "A:s0/s1;A:s2/s3;A:s4;");
        CHECK(zwei.kanten().size() == 2);

        CHECK(fasseAnweisungsfolgenZusammen(d, 0).knoten().size() == 1);
    }

    TEST_CASE("Buendeln: ein Kreis nur aus Anweisungen bleibt als Schleife auf sich selbst")
    {
        Flussdiagramm d;
        const KnotenId a = d.neuerKnoten(KnotenArt::Anweisung, "a");
        const KnotenId b = d.neuerKnoten(KnotenArt::Anweisung, "b");
        d.verbinde(a, b);
        d.verbinde(b, a);

        const Flussdiagramm neu = fasseAnweisungsfolgenZusammen(d);
        REQUIRE(neu.knoten().size() == 1);
        REQUIRE(neu.kanten().size() == 1);
        CHECK(neu.kanten()[0].von == neu.kanten()[0].nach);
    }
}

TEST_SUITE("Strukturdiagramm")
{
    TEST_CASE("Je Typ ein Kasten; Glieder, die einen Kasten nennen, geben Beziehungen")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(beispiele::kDatentypen, "datentypen.c");
        const Strukturdiagramm d = strukturVon(e.einheit);

        CHECK(d.titel == "datentypen.c");
        REQUIRE(d.kaesten.size() == 5);
        CHECK(d.kaesten[0].name == "Farbe");
        CHECK(d.kaesten[0].art == KastenArt::Aufzaehlung);
        CHECK(d.kaesten[0].eintraege[1].text() == "GRUEN = 4");
        CHECK(d.kaesten[2].name == "Knoten");
        CHECK(d.kaesten[2].eintraege[2].text() == "struct Knoten *naechster");
        CHECK(d.kaesten[2].eintraege[3].text() == "u8 daten[16]");
        CHECK(d.kaesten[2].eintraege[4].text() == "unsigned bereit : 1");
        CHECK(d.kaesten[2].eintraege[6].text() == "void (*rueckruf)(struct Knoten *k, int grund)");
        CHECK(d.kaesten[3].art == KastenArt::Vereinigung);

        const auto bild = [&](const Beziehung& b) {
            const char art = b.art == BeziehungArt::Enthaelt ? '*' : b.art == BeziehungArt::Verweist ? '>' : '^';
            return d.kaesten[b.von].name + art + d.kaesten[b.nach].name + ":" + b.beschriftung;
        };
        REQUIRE(d.beziehungen.size() == 6);
        CHECK(bild(d.beziehungen[0]) == "Knoten*Punkt:ort");            // als Wert: enthaelt
        CHECK(bild(d.beziehungen[1]) == "Knoten>Farbe:farbe");          // eine Aufzaehlung: verweist
        CHECK(bild(d.beziehungen[2]) == "Knoten>Knoten:naechster");     // Zeiger: verweist
        CHECK(bild(d.beziehungen[3]) == "Knoten*anonymous_wert:wert");
        CHECK(bild(d.beziehungen[4]) == "Liste>Knoten:kopf");           // ueber den typedef Knoten_t
        CHECK(bild(d.beziehungen[5]) == "Liste*Punkt:mitte");           // ueber den typedef Punkt_t
    }

    TEST_CASE("Kaesten, Glieder und Methoden tragen ihre Zeile im Quelltext (ein Klick zeigt sie)")
    {
        const c2d::sprache::Ergebnis c = c2d::sprache::c().lies(
            "enum Farbe {\n"     // 1
            "    ROT,\n"         // 2
            "    GRUEN = 4\n"    // 3
            "};\n"               // 4
            "struct Punkt {\n"   // 5
            "    int x,\n"       // 6
            "        y;\n"       // 7  ein Deklarator gilt, wo seine Deklaration beginnt
            "    struct {\n"     // 8
            "        int a;\n"   // 9
            "    } innen;\n"     // 10
            "};\n",
            "t.c");
        const Strukturdiagramm d = strukturVon(c.einheit);
        REQUIRE(d.kaesten.size() == 3);
        CHECK(d.kaesten[0].zeile == 1);
        REQUIRE(d.kaesten[0].eintraege.size() == 2);
        CHECK(d.kaesten[0].eintraege[0].zeile == 2);
        CHECK(d.kaesten[0].eintraege[1].zeile == 3);
        CHECK(d.kaesten[1].zeile == 5);
        REQUIRE(d.kaesten[1].eintraege.size() == 3);
        CHECK(d.kaesten[1].eintraege[0].zeile == 6);
        CHECK(d.kaesten[1].eintraege[1].zeile == 6);
        CHECK(d.kaesten[1].eintraege[2].zeile == 10);  // das Glied steht hinter der `}`
        REQUIRE(d.kaesten[2].eintraege.size() == 1);
        CHECK(d.kaesten[2].eintraege[0].zeile == 9);

        const c2d::sprache::Ergebnis cpp = c2d::sprache::cpp().lies(
            "class Motor {\n"      // 1
            "public:\n"            // 2
            "    void starte();\n"  // 3
            "private:\n"           // 4
            "    int m_takt = 0;\n"  // 5
            "};\n",
            "t.cpp");
        const Strukturdiagramm k = strukturVon(cpp.einheit);
        REQUIRE(k.kaesten.size() == 1);
        CHECK(k.kaesten[0].zeile == 1);
        REQUIRE(k.kaesten[0].methoden.size() == 1);
        CHECK(k.kaesten[0].methoden[0].zeile == 3);
        REQUIRE(k.kaesten[0].eintraege.size() == 1);
        CHECK(k.kaesten[0].eintraege[0].zeile == 5);
    }
}
