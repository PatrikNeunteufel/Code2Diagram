/**
 * @file test_Lexer.cpp
 * @brief Der allgemeine Lexer, an der Sprachtabelle von C
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/lexer/Lexer.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <string>

using namespace c2d::lexer;
using c2d::quelle::Meldung;

namespace
{

std::vector<Token> zerlegeC(std::string_view text, std::vector<Meldung>& meldungen, bool mitKommentaren = false)
{
    return zerlege(text, c2d::sprache::c().tabelle(), meldungen, {.mitKommentaren = mitKommentaren});
}

/// Die Token als "Art:Text", durch Leerzeichen getrennt — ohne das Ende.
std::string bild(const std::vector<Token>& token)
{
    static constexpr std::string_view kKuerzel[] = {"B", "S", "N", "K", "Z", "O", "C", "D", "?", "E"};
    std::string s;
    for (const Token& t : token)
    {
        if (t.art == TokenArt::Ende) break;
        if (!s.empty()) s += ' ';
        s += kKuerzel[static_cast<int>(t.art)];
        s += ':';
        s += t.text;
    }
    return s;
}

}  // namespace

TEST_SUITE("Lexer")
{
    TEST_CASE("Arten, Zeilen und Abstand")
    {
        std::vector<Meldung> meldungen;
        const std::vector<Token> t = zerlegeC("int x = 42; // Kommentar\nx += 0x1F;", meldungen);

        CHECK(bild(t) == "S:int B:x O:= N:42 O:; B:x O:+= N:0x1F O:;");
        CHECK(meldungen.empty());
        REQUIRE(t.size() == 10);
        CHECK(t.back().art == TokenArt::Ende);

        CHECK(t[0].ort.zeile == 1);
        CHECK(t[0].ort.spalte == 1);
        CHECK_FALSE(t[0].abstandDavor);
        CHECK(t[2].abstandDavor);        // " ="
        CHECK_FALSE(t[4].abstandDavor);  // "42;"
        CHECK(t[5].ort.zeile == 2);
        CHECK(t[5].ort.spalte == 1);
        CHECK(t[5].ort.versatz == 25);
        CHECK(t[5].abstandDavor);  // Kommentar und Zeilenwechsel
        CHECK(t[6].ort.spalte == 3);
    }

    TEST_CASE("Kommentare: uebergangen oder als Token")
    {
        std::vector<Meldung> meldungen;
        CHECK(bild(zerlegeC("a /* b\n c */ d // e", meldungen)) == "B:a B:d");
        CHECK(bild(zerlegeC("a /* b */ d // e", meldungen, true)) == "B:a C:/* b */ B:d C:// e");
        CHECK(meldungen.empty());

        zerlegeC("a /* offen", meldungen);
        REQUIRE(meldungen.size() == 1);
        CHECK(meldungen[0].ort.spalte == 3);
    }

    TEST_CASE("Direktiven: eine Zeile, mit Fortsetzung, nur am Zeilenanfang")
    {
        std::vector<Meldung> meldungen;
        const std::vector<Token> t =
            zerlegeC("#include <a.h>\r\n  # define X \\\r\n  1\r\nint a; # b\r\n/* k */ #pragma once", meldungen);

        REQUIRE(t.size() == 9);
        CHECK(t[0].art == TokenArt::Direktive);
        CHECK(t[0].text == "#include <a.h>");  // ohne das \r
        CHECK(t[1].art == TokenArt::Direktive);
        CHECK(t[1].text == "# define X \\\r\n  1");
        CHECK(t[1].ort.zeile == 2);
        CHECK(t[2].ort.zeile == 4);  // die Fortsetzung zaehlt als Zeile
        CHECK(bild({t.begin() + 2, t.begin() + 7}) == "S:int B:a O:; O:# B:b");
        CHECK(t[7].art == TokenArt::Direktive);  // ein Kommentar davor aendert nichts
        CHECK(t[7].text == "#pragma once");
    }

    TEST_CASE("Literale mit Fluchtzeichen; ein offenes Literal wird gemeldet")
    {
        std::vector<Meldung> meldungen;
        CHECK(bild(zerlegeC(R"("a\"b" 'c' '\'' "")", meldungen)) == R"(K:"a\"b" Z:'c' Z:'\'' K:"")");
        CHECK(meldungen.empty());

        const std::vector<Token> t = zerlegeC("x = \"offen\ny;", meldungen);
        CHECK(bild(t) == "B:x O:= K:\"offen B:y O:;");
        REQUIRE(meldungen.size() == 1);
        CHECK(meldungen[0].ort.zeile == 1);
        CHECK(meldungen[0].ort.spalte == 5);
    }

    TEST_CASE("Zahlen wie die pp-number in C")
    {
        std::vector<Meldung> meldungen;
        CHECK(bild(zerlegeC("1.5e-3f .5 1e+10 0b101u 3-2 a.b", meldungen)) ==
              "N:1.5e-3f N:.5 N:1e+10 N:0b101u N:3 O:- N:2 B:a O:. B:b");
    }

    TEST_CASE("Der laengste Operator gewinnt")
    {
        std::vector<Meldung> meldungen;
        CHECK(bild(zerlegeC("a>>=b->c...d>>e>=f", meldungen)) == "B:a O:>>= B:b O:-> B:c O:... B:d O:>> B:e O:>= B:f");
    }

    TEST_CASE("Ein unbekanntes Zeichen wird ein Token und eine Meldung — nichts faellt weg")
    {
        std::vector<Meldung> meldungen;
        CHECK(bild(zerlegeC("a @ b", meldungen)) == "B:a ?:@ B:b");
        REQUIRE(meldungen.size() == 1);
        CHECK(meldungen[0].schwere == c2d::quelle::Schwere::Warnung);
        CHECK(meldungen[0].ort.spalte == 3);
    }

    TEST_CASE("Bezeichner duerfen Umlaute tragen (UTF-8)")
    {
        std::vector<Meldung> meldungen;
        CHECK(bild(zerlegeC("gr\xC3\xB6sse = 1", meldungen)) == "B:gr\xC3\xB6sse O:= N:1");
        CHECK(meldungen.empty());
    }
}
