/**
 * @file test_Bedingt.cpp
 * @brief Bedingte Uebersetzung: welche Zweige eines #if der Leser sieht
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/sprache/Sprache.hpp"

using c2d::sprache::Ergebnis;
using c2d::sprache::LeseOptionen;

namespace
{

constexpr std::string_view kVarianten = R"c(
#ifndef KOPF_H
#define KOPF_H
#define STUFE 2
#define MIT_LOG

#if STUFE >= 2 && defined(MIT_LOG)
void laut(void) { melde(); }
#elif STUFE == 1
void leise(void) { }
#else
void stumm(void) { }
#endif

#ifdef BOARD_A
void a(void) { if (x) {
#else
void b(void) { if (y) {
#endif
    tu(); } }

#if 0
kaputt ( { [
#endif

#if VERSION > 3
void neu(void) {}
#endif
#endif
)c";

/// Die Namen der Funktionen, durch Leerzeichen getrennt.
std::string funktionen(const Ergebnis& e)
{
    std::string s;
    for (const c2d::modell::Funktion& f : e.einheit.funktionen) s += (s.empty() ? "" : " ") + f.name;
    return s;
}

Ergebnis liesC(std::string_view text, const LeseOptionen& optionen = {})
{
    return c2d::sprache::c().lies(text, "test.c", optionen);
}

}  // namespace

TEST_SUITE("Bedingte Uebersetzung")
{
    TEST_CASE("Gelesen wird, was gilt — mit den #define der Datei")
    {
        const Ergebnis e = liesC(kVarianten);

        CHECK(e.meldungen.empty());  // auch der Zweig mit `kaputt` stoert nicht: er gilt nicht
        CHECK(funktionen(e) == "laut b");
        // je Zweig eine eigene Klammer: nur einer wird gelesen, die Schachtelung stimmt
        REQUIRE(e.einheit.funktionen.size() == 2);
        CHECK(e.einheit.funktionen[1].rumpf->kinder.size() == 1);
        CHECK(e.einheit.funktionen[1].rumpf->kinder[0]->text == "y");
        CHECK(e.einheit.funktionen[1].zeile == 18);  // die Zeilen bleiben die der Datei

        // die #define der geltenden Zweige stehen weiter im Codemodell
        REQUIRE(e.einheit.makros.size() == 3);
        CHECK(e.einheit.makros[1].name == "STUFE");

        // wonach gefragt wurde und was nirgends definiert ist — der Waechter KOPF_H gehoert nicht dazu
        CHECK(e.unbekannteNamen == std::vector<std::string>{"BOARD_A", "VERSION"});
    }

    TEST_CASE("Definitionen von aussen waehlen eine andere Variante")
    {
        LeseOptionen optionen;
        optionen.definitionen = {c2d::sprache::definitionAus("BOARD_A"), c2d::sprache::definitionAus("VERSION=4")};
        const Ergebnis e = liesC(kVarianten, optionen);

        CHECK(e.meldungen.empty());
        CHECK(funktionen(e) == "laut a neu");
        CHECK(e.einheit.funktionen[1].rumpf->kinder[0]->text == "x");
        CHECK(e.unbekannteNamen.empty());

        // eine Definition der Datei gewinnt gegen die von aussen, wie beim Compiler: sie kommt spaeter
        optionen.definitionen = {c2d::sprache::definitionAus("STUFE=1")};
        CHECK(funktionen(liesC(kVarianten, optionen)) == "laut b");
    }

    TEST_CASE("Der Schalter: alle Zweige lesen, wie hintereinander geschrieben")
    {
        const Ergebnis e = liesC(kVarianten, {.alleZweige = true, .definitionen = {}});

        CHECK_FALSE(e.meldungen.empty());  // jetzt stoeren die doppelte Klammer und `kaputt`
        CHECK(funktionen(e).starts_with("laut leise stumm a"));
        CHECK(e.unbekannteNamen.empty());  // es wurde nach nichts gefragt
    }

    TEST_CASE("definitionAus: NAME oder NAME=wert")
    {
        CHECK(c2d::sprache::definitionAus("A").name == "A");
        CHECK(c2d::sprache::definitionAus("A").wert == "1");
        CHECK(c2d::sprache::definitionAus(" B = 0x10").name == "B");
        CHECK(c2d::sprache::definitionAus("B=0x10").wert == "0x10");
        CHECK(c2d::sprache::definitionAus("C=").wert.empty());
        CHECK(c2d::sprache::definitionAus("=3").name.empty());
    }

    TEST_CASE("Rechnen wie der Praeprozessor")
    {
        const auto gilt = [](std::string_view bedingung, std::string_view davor = {}) {
            const std::string text = std::string(davor) + "\n#if " + std::string(bedingung) + "\nint ja;\n#else\nint nein;\n#endif\n";
            const Ergebnis e = c2d::sprache::cpp().lies(text, "t.cpp");
            REQUIRE(e.einheit.variablen.size() == 1);
            CHECK(e.meldungen.empty());
            return e.einheit.variablen[0].name == "ja";
        };
        CHECK(gilt("1"));
        CHECK_FALSE(gilt("0"));
        CHECK(gilt("(1 << 4) == 0x10 && 'A' == 65 && 10 / 3 == 3 && 10 % 3 == 1"));
        CHECK(gilt("0b11 == 3 && 017 == 15 && 1'000 == 1000 && 42UL == 42"));
        CHECK(gilt("(5 > 3 ? 7 : 9) == 7 && -1 < 0 && ~0 == -1 && !0"));
        CHECK(gilt("1 + 2 * 3 == 7 && (1 | 2) == 3 && (6 & 3) == 2 && (6 ^ 3) == 5"));
        CHECK(gilt("!defined(NIX) && !defined NIX && NIX == 0"));  // nicht definiert zaehlt 0
        CHECK(gilt("defined(__cplusplus) && __cplusplus >= 201703L && true && !false"));
        CHECK(gilt("A == 7 && B + 1 == 8", "#define A B\n#define B 7"));  // ein Makro, dessen Wert ein Makro ist
        CHECK_FALSE(gilt("defined(WEG)", "#define WEG\n#undef WEG"));
        CHECK(gilt("defined(LEER) && LEER == 0", "#define LEER"));
        CHECK(gilt("1 / 0 == 0"));  // der Compiler meldet es; hier zaehlt es 0
    }

    TEST_CASE("Verschachtelt, und #elif nach einem Zweig, der schon galt")
    {
        const Ergebnis e = liesC(R"c(
#define A 1
#if A
  int a1;
  #if B
    int b;
  #elif A
    int a2;
  #elif 1
    int nie;
  #else
    int auch_nie;
  #endif
#else
  #if 1
    int aussen_falsch;
  #endif
#endif
)c");
        CHECK(e.meldungen.empty());
        REQUIRE(e.einheit.variablen.size() == 2);
        CHECK(e.einheit.variablen[0].name == "a1");
        CHECK(e.einheit.variablen[1].name == "a2");
    }

    TEST_CASE("Ein C-Kopf mit extern \"C\" fuer C++: in C gilt der Zweig nicht, in C++ schon")
    {
        constexpr std::string_view kKopf = "#ifdef __cplusplus\nextern \"C\" {\n#endif\nvoid f(void);\n"
                                           "#ifdef __cplusplus\n}\n#endif\n";
        const Ergebnis c = liesC(kKopf);
        CHECK(c.meldungen.empty());
        CHECK(funktionen(c) == "f");
        CHECK(c.unbekannteNamen == std::vector<std::string>{"__cplusplus"});

        const Ergebnis cpp = c2d::sprache::cpp().lies(kKopf, "kopf.h");
        CHECK(cpp.meldungen.empty());
        CHECK(funktionen(cpp) == "f");
        CHECK(cpp.unbekannteNamen.empty());
    }

    TEST_CASE("Was sich nicht ausrechnen laesst: ein Hinweis, der Zweig wird gelesen")
    {
        const Ergebnis e = liesC("#define FLAECHE(a) ((a) * 2)\n#if FLAECHE(2) > 1\nint a;\n#endif\n"
                                 "#if __has_include(<x.h>)\nint b;\n#endif\n#if 1.5 > 1\nint c;\n#endif\n");
        REQUIRE(e.meldungen.size() == 3);
        CHECK(e.meldungen[0].schwere == c2d::quelle::Schwere::Hinweis);
        CHECK(e.meldungen[0].ort.zeile == 2);
        CHECK(e.meldungen[0].text == "condition not evaluated, its branch is read: #if FLAECHE(2) > 1");
        CHECK(e.einheit.variablen.size() == 3);
    }

    TEST_CASE("Unpaarig: #else und #endif ohne #if, #if ohne #endif")
    {
        const Ergebnis ohne = liesC("int a;\n#else\nint b;\n#endif\nint c;\n");
        REQUIRE(ohne.meldungen.size() == 2);
        CHECK(ohne.meldungen[0].text == "#else without #if");
        CHECK(ohne.meldungen[1].text == "#endif without #if");
        CHECK(ohne.einheit.variablen.size() == 3);

        const Ergebnis offen = liesC("#if 1\nint a;\n#if 0\nint b;\n");
        REQUIRE(offen.meldungen.size() == 2);
        CHECK(offen.meldungen[0].text == "#if without #endif");
        CHECK(offen.meldungen[0].ort.zeile == 1);
        CHECK(offen.einheit.variablen.size() == 1);

        const Ergebnis doppelt = liesC("#if 0\n#else\nint a;\n#else\nint b;\n#endif\n");
        REQUIRE(doppelt.meldungen.size() == 1);
        CHECK(doppelt.meldungen[0].text == "#else after #else");
        CHECK(doppelt.einheit.variablen.size() == 1);
    }

    TEST_CASE("Im Rumpf einer Funktion: nur der geltende Zweig wird Anweisung")
    {
        const Ergebnis e = liesC(R"c(
void f(int a)
{
#if DEBUG
    protokolliere(a);
#endif
    if (a == 1)
        eins();
#ifdef MIT_ZWEI
    else if (a == 2)
        zwei();
#endif
    else
        sonst();
}
)c");
        CHECK(e.meldungen.empty());
        const c2d::modell::Anweisung& rumpf = *e.einheit.funktionen[0].rumpf;
        REQUIRE(rumpf.kinder.size() == 1);  // kein protokolliere, keine Direktive
        REQUIRE(rumpf.kinder[0]->sonst != nullptr);
        CHECK(rumpf.kinder[0]->sonst->text == "sonst()");  // das else gehoert zum ersten if
        CHECK(e.unbekannteNamen == std::vector<std::string>{"DEBUG", "MIT_ZWEI"});
    }
}
