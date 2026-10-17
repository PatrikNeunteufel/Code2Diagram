/**
 * @file test_Schalter.cpp
 * @brief Was das Feld Configuration braucht: wonach die Bedingungen fragen, ihre Ketten, und was einen Zweig waehlt
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/sprache/Sprache.hpp"

#include <optional>
#include <string>
#include <vector>

using namespace c2d::sprache;

namespace
{

constexpr std::string_view kKopf = R"c(
#ifndef KOPF_H
#define KOPF_H
#define FEST 3 /* die Stufe */ // ohne Kommentar im Wert

#ifdef BOARD_A
void a(void) {}
#elif defined(BOARD_B)
void b(void) {}
#else
void c(void) {}
#endif

void f(void)
{
#if STUFE >= 2
    laut();
#elif STUFE == 1
    leise();
#endif
#if FEST > 2
    fest();
#endif
#ifdef DEBUG
#  if !VERBOSE
    kurz();
#  endif
#endif
}
#endif
)c";

Ergebnis lies(std::vector<std::string> definitionen = {}, bool alleZweige = false)
{
    LeseOptionen optionen;
    optionen.alleZweige = alleZweige;
    for (const std::string& d : definitionen) optionen.definitionen.push_back(definitionAus(d));
    return c().lies(kKopf, "kopf.h", optionen);
}

/// `NAME`, `NAME=wert` oder `!NAME` je Forderung; `-` = nicht abzuleiten.
std::string text(const std::optional<std::vector<Forderung>>& wahl)
{
    if (!wahl) return "-";
    std::string s;
    for (const Forderung& f : *wahl)
        s += (s.empty() ? "" : " ") + std::string(f.definiert ? "" : "!") + f.name + (f.wert.empty() ? "" : "=" + f.wert);
    return s;
}

std::vector<std::string> namen(const Ergebnis& e)
{
    std::vector<std::string> aus;
    for (const Schalter& s : e.schalter) aus.push_back(s.name);
    return aus;
}

}  // namespace

TEST_SUITE("Schalter und Weichen")
{
    TEST_CASE("Wonach gefragt wird: jeder Name einmal, mit Zustand; der Waechter nicht")
    {
        const Ergebnis e = lies();
        CHECK(namen(e) == std::vector<std::string>{"BOARD_A", "BOARD_B", "STUFE", "FEST", "DEBUG", "VERBOSE"});
        CHECK_FALSE(e.schalter[0].zahl);
        CHECK(e.schalter[2].zahl);  // STUFE >= 2: ein Wert
        CHECK_FALSE(e.schalter[2].definiert);
        CHECK(e.schalter[3].definiert);
        CHECK(e.schalter[3].wert == "3");  // ohne die Kommentare dahinter
        CHECK(e.schalter[3].inDatei == 4);  // die Datei definiert FEST: von aussen nicht zu aendern
        CHECK_FALSE(e.schalter[5].zahl);   // !VERBOSE: nur ja/nein — auch ungefragt (DEBUG gilt nicht) steht er da
        CHECK(e.einheit.funktionen.back().bisZeile == 29);

        const Ergebnis mit = lies({"BOARD_B", "STUFE=2"});
        CHECK(mit.schalter[1].definiert);
        CHECK(mit.schalter[1].wert == "1");
        CHECK(mit.schalter[2].wert == "2");

        CHECK(lies({}, true).schalter.empty());  // alle Zweige: es wird nicht gerechnet
    }

    TEST_CASE("Die Ketten: Zweige, welcher gilt, wo sie stehen")
    {
        const Ergebnis e = lies();
        REQUIRE(e.weichen.size() == 5);  // ohne den Waechter
        const Weiche& board = e.weichen[0];
        CHECK(board.zeile == 6);
        CHECK(board.zweige == std::vector<std::string>{"defined(BOARD_A)", "defined(BOARD_B)", "else"});
        CHECK(board.geltend == 2);
        CHECK(board.eltern == kKeinZweig);

        const Weiche& stufe = e.weichen[1];
        CHECK(stufe.zweige == std::vector<std::string>{"STUFE >= 2", "STUFE == 1"});
        CHECK(stufe.geltend == 2);  // das fehlende #else
        REQUIRE(stufe.wahl.size() == 3);

        const Weiche& innen = e.weichen[4];
        CHECK(innen.zeile == 25);
        CHECK(innen.eltern == 3);
        CHECK(innen.elternZweig == 0);
        CHECK(innen.geltend == kKeinZweig);  // DEBUG gilt nicht: hier gilt keiner
    }

    TEST_CASE("Was einen Zweig waehlt — oder dass es sich nicht ableiten laesst")
    {
        const Ergebnis e = lies();
        const auto& board = e.weichen[0].wahl;
        CHECK(text(board[0]) == "BOARD_A");
        CHECK(text(board[1]) == "BOARD_B");
        CHECK(text(board[2]).empty());  // gilt schon

        const auto& stufe = e.weichen[1].wahl;
        CHECK(text(stufe[0]) == "STUFE=2");  // STUFE >= 2: nachgerechnet — die Zahl der Bedingung tut es
        CHECK(text(stufe[1]) == "STUFE=1");
        CHECK(text(stufe[2]).empty());
        // STUFE == 1 soll falsch werden: STUFE weg, dann zaehlt er 0
        CHECK(text(lies({"STUFE=1"}).weichen[1].wahl[2]) == "!STUFE");
        CHECK(text(lies({"STUFE=5"}).weichen[1].wahl[1]) == "STUFE=1");  // erst >= 2 falsch, dann == 1 wahr

        // zwei Namen in einer Bedingung, die nicht einfach ist: nicht abzuleiten
        const Ergebnis zwei = c().lies("#if A > B\nint x;\n#endif\n", "z.c");
        REQUIRE(zwei.weichen.size() == 1);
        CHECK(text(zwei.weichen[0].wahl[0]) == "-");
        CHECK(text(zwei.weichen[0].wahl[1]).empty());

        CHECK(text(e.weichen[2].wahl[0]).empty());
        CHECK(text(e.weichen[2].wahl[1]) == "-");  // FEST definiert die Datei

        // verschachtelt: erst der Zweig darum
        CHECK(text(e.weichen[4].wahl[0]) == "DEBUG");
        CHECK(text(e.weichen[4].wahl[1]) == "DEBUG VERBOSE=1");

        // mit Definitionen: zurueck heisst entfernen
        const Ergebnis mit = lies({"BOARD_B"});
        CHECK(mit.weichen[0].geltend == 1);
        CHECK(text(mit.weichen[0].wahl[1]).empty());
        CHECK(text(mit.weichen[0].wahl[2]) == "!BOARD_B");
        CHECK(text(mit.weichen[0].wahl[0]) == "BOARD_A");  // BOARD_B darf bleiben: BOARD_A kommt zuerst
    }

    TEST_CASE("mitForderungen: die Definitionen von aussen so aendern, dass die Forderungen gelten")
    {
        const Ergebnis e = lies({"BOARD_B"});
        const auto anwenden = [&](std::vector<Definition> aussen, std::vector<Forderung> f) {
            const auto neu = mitForderungen(std::move(aussen), f, e.schalter);
            if (!neu) return std::string("-");
            std::string s;
            for (const Definition& d : *neu) s += (s.empty() ? "" : " ") + d.name + "=" + d.wert;
            return s;
        };
        CHECK(anwenden({}, {{"DEBUG", true, ""}}) == "DEBUG=1");
        CHECK(anwenden({{"BOARD_B", "1"}}, {{"BOARD_B", false, ""}}) == "");
        CHECK(anwenden({{"STUFE", "1"}}, {{"STUFE", true, "2"}}) == "STUFE=2");
        CHECK(anwenden({}, {{"FEST", true, "5"}}) == "-");  // die Datei definiert ihn
    }

    TEST_CASE("mitForderungen: was darunter definiert ist, hebt !NAME auf; Anhaken nimmt es zurueck")
    {
        // so liest es die Datei, wenn die Projektdatei BOARD_B und STUFE=2 definiert
        const Ergebnis e = lies({"BOARD_B", "STUFE=2"});
        const std::vector<Definition> darunter = {{"BOARD_B", "1"}, {"STUFE", "2"}};
        const auto anwenden = [&](std::vector<std::string> aussen, std::vector<Forderung> f, bool mitDarunter = true) {
            std::vector<Definition> a;
            for (const std::string& d : aussen) a.push_back(definitionAus(d));
            const auto neu = mitForderungen(std::move(a), f, e.schalter, mitDarunter ? darunter : std::vector<Definition>{});
            if (!neu) return std::string("-");
            std::string s;
            for (const Definition& d : *neu) s += (s.empty() ? "" : " ") + alsText(d);
            return s;
        };
        CHECK(anwenden({}, {{"BOARD_B", false, ""}}) == "!BOARD_B");
        CHECK(anwenden({}, {{"BOARD_B", false, ""}}, false) == "!BOARD_B");  // ohne darunter: der Schalter sagt es
        CHECK(anwenden({"!BOARD_B"}, {{"BOARD_B", true, ""}}) == "");        // zurueck: darunter gilt wieder
        CHECK(anwenden({"BOARD_B=7"}, {{"BOARD_B", false, ""}}) == "!BOARD_B");
        CHECK(anwenden({"!STUFE"}, {{"STUFE", true, "2"}}) == "");           // darunter schon 2
        CHECK(anwenden({"!STUFE"}, {{"STUFE", true, "3"}}) == "STUFE=3");
        CHECK(anwenden({}, {{"DEBUG", false, ""}}) == "");                   // nirgends definiert: nichts zu tun
        CHECK(anwenden({"!DEBUG"}, {{"DEBUG", true, ""}}) == "DEBUG");       // darunter nicht definiert
        CHECK(anwenden({}, {{"FEST", false, ""}}) == "-");                   // die Datei definiert ihn
    }
}
