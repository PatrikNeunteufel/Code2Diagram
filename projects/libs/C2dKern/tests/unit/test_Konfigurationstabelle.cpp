/**
 * @file test_Konfigurationstabelle.cpp
 * @brief Je Kette #if: welcher Zweig bei welchen Definitionen gilt — absolut, je Alternative eine Zeile
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Konfigurationstabelle.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <string>
#include <vector>

using namespace c2d::diagramm;

namespace
{

std::vector<Konfigurationstabelle> tabellen(const char* text, std::vector<c2d::sprache::Definition> definitionen = {})
{
    c2d::sprache::LeseOptionen o;
    o.definitionen = std::move(definitionen);
    const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(text, "t.c", o);
    return konfigurationVon(e.weichen, e.schalter);
}

/// Die Zeilen als "Zweig: Wert|Wert [*]; " — `*` = gilt, `never` = nie.
std::string bild(const Konfigurationstabelle& t)
{
    std::string s;
    for (const Konfigurationstabelle::Zeile& r : t.zeilen)
    {
        s += r.zweig + ":";
        for (std::size_t i = 0; i < r.werte.size(); ++i) s += (i ? "|" : " ") + r.werte[i];
        if (r.geltend) s += " *";
        if (r.nie) s += " never";
        s += "; ";
    }
    return s;
}

std::vector<std::string> namen(const Konfigurationstabelle& t)
{
    std::vector<std::string> aus;
    for (const Konfigurationstabelle::Spalte& s : t.spalten) aus.push_back(s.name);
    return aus;
}

const std::string kStrich = "\xE2\x80\x93";

}  // namespace

TEST_SUITE("Konfigurationstabelle")
{
    TEST_CASE("Eine Kette: absolut — ein Zweig gilt, wenn alle davor falsch sind; der Stand im Kopf")
    {
        const auto t = tabellen("#if defined(A)\nint a;\n#elif VERSION >= 2\nint v;\n#else\nint e;\n#endif\n",
                                {{"VERSION", "3"}});
        REQUIRE(t.size() == 1);
        CHECK(t[0].titel == "#if defined(A) (line 1)");
        CHECK(namen(t[0]) == std::vector<std::string>{"A", "VERSION"});
        CHECK(t[0].spalten[0].jetzt == "undef");
        CHECK(t[0].spalten[1].jetzt == "= 3");
        CHECK(bild(t[0]) == "#if defined(A): def|" + kStrich + "; #elif VERSION >= 2: !def|>= 2 *; #else: !def|< 2; ");
        CHECK(t[0].zeilen[1].quellZeile == 3);
        CHECK(t[0].zeilen[2].quellZeile == 5);
    }

    TEST_CASE("Oder: je Alternative eine Zeile; das fehlende #else; nicht definiert ist 0")
    {
        const auto t = tabellen("#if defined(A) || defined(B)\nint x;\n#endif\n"
                                "#if !defined(N) || N < 3\nint k;\n#else\nint g;\n#endif\n");
        REQUIRE(t.size() == 2);
        CHECK(bild(t[0]) == "#if defined(A) || defined(B): def|" + kStrich + "; or: " + kStrich + "|def; (no #else): !def|!def *; ");
        // `!defined(N)` reicht: `N < 3` gilt dann ohnehin; #else: N definiert und mindestens 3
        CHECK(bild(t[1]) == "#if !defined(N) || N < 3: !def *; or: < 3 *; #else: def, >= 3; ");
        CHECK(t[0].zeilen[2].quellZeile == 3);  // das #endif
    }

    TEST_CASE("Innen: eingerueckt, mit dem aeusseren Zweig; ohne ihn gilt nichts")
    {
        const auto t = tabellen("#ifdef A\n#if B == 1\nint b;\n#endif\n#endif\n");
        REQUIRE(t.size() == 2);
        CHECK(t[1].tiefe == 1);
        CHECK(t[1].titel == "#if B == 1 (line 2), in #if defined(A) (line 1)");
        CHECK(bild(t[1]) == "#if B == 1: def|== 1; (no #else): def|!= 1; ");
        CHECK_FALSE(t[1].zeilen[1].geltend);  // A ist nicht definiert: kein Zweig gilt
    }

    TEST_CASE("Nie, Text, und was die Datei selbst definiert")
    {
        const auto t = tabellen("#define TAKT 100\n"
                                "#if 0\nint a;\n#elif X == 1\nint b;\n#elif X == 1\nint c;\n#endif\n"
                                "#if F(2) > 1\nint d;\n#endif\n"
                                "#if TAKT > 50\nint e;\n#endif\n");
        REQUIRE(t.size() == 3);
        CHECK(bild(t[0]) == "#if 0: " + kStrich + " never; #elif X == 1: == 1; #elif X == 1: " + kStrich +
                                " never; (no #else): != 1 *; ");
        REQUIRE(t[1].zeilen.size() == 2);
        CHECK(t[1].zeilen[0].text);
        CHECK(t[1].hinweise == std::vector<std::string>{"line 9: 'F(2) > 1' is not split into names",
                                                         "line 11: 'F(2) > 1' is not split into names"});
        CHECK(t[2].spalten.front().inDatei == 1);
        CHECK(t[2].spalten.front().jetzt == "= 100");
        CHECK(bild(t[2]) == "#if TAKT > 50: > 50 *; (no #else): <= 50; ");
    }

    TEST_CASE("Formate und Anordnung: csv, SVG mit Haekchen, Mermaid nicht; je Zeile ein Knoten")
    {
        const auto t = tabellen("#ifdef A\nint a;\n#else\nint b;\n#endif\n");
        const std::span<const Konfigurationstabelle> s(t);
        CHECK(ordneAn(s).knotenOrte.size() == 2);
        CHECK(c2d::ausgabe::csv().schreibe(s, {}) ==
              "#if defined(A) (line 1)\nbranch;A;active\n#if defined(A);def;\n#else;!def;yes\n");
        CHECK(c2d::ausgabe::svg().schreibe(s, {}).find("\xE2\x9C\x93") != std::string::npos);
        CHECK(c2d::ausgabe::drawio().schreibe(s, {}).find("!def") != std::string::npos);
        using c2d::ausgabe::Diagrammart;
        CHECK(c2d::ausgabe::nichtVerfuegbar(c2d::ausgabe::mermaid(), Diagrammart::Konfiguration) ==
              "#if configuration is not available in mermaid");
        CHECK_FALSE(c2d::ausgabe::dot().kann(Diagrammart::Konfiguration));
    }
}
