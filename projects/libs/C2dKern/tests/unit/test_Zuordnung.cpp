/**
 * @file test_Zuordnung.cpp
 * @brief Zuordnungstabellen: switch, der nur Werte liefert; konstante Tabellen; Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Zuordnung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <string>

using namespace c2d::diagramm;

namespace
{

/// Die Zeilen als "Schluessel=Wert,Wert; ".
std::string bild(const Zuordnung& z)
{
    std::string s;
    for (const Zuordnung::Zeile& r : z.zeilen)
    {
        s += r.schluessel + "=";
        for (std::size_t i = 0; i < r.werte.size(); ++i) s += (i ? "," : "") + r.werte[i];
        s += "; ";
    }
    return s;
}

const c2d::modell::Funktion& funktion(const c2d::sprache::Ergebnis& e, std::string_view name)
{
    for (const c2d::modell::Funktion& f : e.einheit.funktionen)
        if (f.name == name) return f;
    FAIL("no such function");
    return e.einheit.funktionen.front();
}

}  // namespace

TEST_SUITE("Zuordnung")
{
    TEST_CASE("switch mit return: je Wert eine Zeile, geteilte Faelle einzeln, ohne default otherwise")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(
            "int f(int p)\n{\n  switch (p) {\n  case 0: return 5000;\n  case 1:\n  case 2: return T_MAX / 2;\n"
            "  case 3: return berechne(p);\n  }\n  return 0;\n}\n",
            "t.c");
        const c2d::modell::Funktion& f = funktion(e, "f");
        CHECK(zeilenMitZuordnung(f) == std::vector<int>{3});
        const std::optional<Zuordnung> z = zuordnungVon(f, 3);
        REQUIRE(z.has_value());
        CHECK(z->titel == "f(): switch (p)");
        CHECK(z->schluessel == "p");
        CHECK(z->spalten == std::vector<std::string>{"return"});
        CHECK(bild(*z) == "0=5000; 1=T_MAX / 2; 2=T_MAX / 2; 3=berechne(p); otherwise=\xE2\x80\x94; ");
        CHECK(z->zeilen[1].quellZeile == 5);  // die Marke von case 1
        CHECK_FALSE(zuordnungVon(f, 4).has_value());
    }

    TEST_CASE("switch mit Zuweisungen an dieselben Namen: je Name eine Spalte, default zuletzt")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(
            "void f(int p)\n{\n  switch (p) {\n  default: r = 0; g = 0; break;\n  case 1: r = 1; g = 0; break;\n"
            "  case 2: g = 1; r = 0; break;\n  }\n}\n",
            "t.c");
        const std::optional<Zuordnung> z = zuordnungVon(funktion(e, "f"), 3);
        REQUIRE(z.has_value());
        CHECK(z->spalten == std::vector<std::string>{"r", "g"});
        CHECK(bild(*z) == "1=1,0; 2=0,1; default=0,0; ");
    }

    TEST_CASE("Kein Zuordnung: ein Fall tut mehr, faellt durch, oder setzt andere Namen")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(
            "void a(int p) { switch (p) { case 1: x = 1; melde(); break; case 2: x = 2; break; } }\n"
            "void b(int p) { switch (p) { case 1: x = 1; case 2: x = 2; break; } }\n"
            "void c(int p) { switch (p) { case 1: x = 1; break; case 2: y = 2; break; } }\n"
            "int d(int p) { switch (p) { case 1: return 1; case 2: x = 2; break; } return 0; }\n",
            "t.c");
        for (const char* name : {"a", "b", "c", "d"})
        {
            CAPTURE(name);
            CHECK(zeilenMitZuordnung(funktion(e, name)).empty());
        }
    }

    TEST_CASE("Konstante Tabellen: Index -> Wert, benannte Indizes, Strukturen je Glied")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(
            "typedef struct { unsigned short dauer_ms; unsigned char blinkt; } PhasenDaten;\n"
            "static const unsigned short dauer[4] = {5000, 1000, 4000, 1000};\n"
            "static const PhasenDaten daten[] = {\n  [ROT] = {5000, 0},\n  [GRUEN] = {.blinkt = 0, .dauer_ms = 4000},\n"
            "  {1000, 1},\n};\n"
            "static unsigned short veraenderlich[2] = {1, 2};\n"
            "static const int einzeln = 5;\n",
            "t.c");
        const std::vector<Zuordnung> t = nachschlagetabellenVon(e.einheit);
        REQUIRE(t.size() == 2);  // nicht const und kein Array zaehlen nicht
        CHECK(t[0].titel == "static const unsigned short dauer[4]");
        CHECK(t[0].spalten == std::vector<std::string>{"value"});
        CHECK(bild(t[0]) == "0=5000; 1=1000; 2=4000; 3=1000; ");
        CHECK(t[1].spalten == std::vector<std::string>{"dauer_ms", "blinkt"});
        CHECK(bild(t[1]) == "ROT=5000,0; GRUEN=4000,0; GRUEN + 1=1000,1; ");
        CHECK(t[1].zeilen[1].quellZeile == 5);
    }

    TEST_CASE("Konstante Tabellen: der Strukturtyp aus einem Kopf, ueber die Suche; ohne sie 1, 2")
    {
        const c2d::sprache::Ergebnis kopf =
            c2d::sprache::c().lies("typedef struct { const char* name; int id; } Befehl;\n", "k.h");
        const c2d::sprache::Ergebnis e =
            c2d::sprache::c().lies("#include \"k.h\"\nstatic const Befehl befehle[] = {\n  {\"AN\", 1},\n};\n", "t.c");
        const TypSuche suche = [&](std::string_view name) -> const c2d::modell::Typ* {
            for (const c2d::modell::Typ& t : kopf.einheit.typen)
                if (t.name == name) return &t;
            return nullptr;
        };
        const std::vector<Zuordnung> mit = nachschlagetabellenVon(e.einheit, suche);
        REQUIRE(mit.size() == 1);
        CHECK(mit[0].spalten == std::vector<std::string>{"name", "id"});
        CHECK(nachschlagetabellenVon(e.einheit)[0].spalten == std::vector<std::string>{"1", "2"});
    }

    TEST_CASE("Formate und Anordnung: SVG, draw.io, csv; Mermaid und DOT nicht; Knoten je Zeile")
    {
        const c2d::sprache::Ergebnis e =
            c2d::sprache::c().lies("int f(int p) { switch (p) { case 0: return 7; default: return 9; } }", "t.c");
        const std::vector<Zuordnung> z{*zuordnungVon(funktion(e, "f"), 1)};
        const std::span<const Zuordnung> s(z);
        const TabellenAnordnung a = ordneAn(s);
        CHECK(a.knotenOrte.size() == 2);
        using c2d::ausgabe::Diagrammart;
        CHECK(c2d::ausgabe::csv().schreibe(s, {}) == "f(): switch (p)\np;return\n0;7\ndefault;9\n");
        CHECK(c2d::ausgabe::svg().schreibe(s, {}).find(">default</text>") != std::string::npos);  // linksbuendig
        CHECK(c2d::ausgabe::drawio().schreibe(s, {}).find("value=\"9\"") != std::string::npos);
        CHECK_FALSE(c2d::ausgabe::dot().kann(Diagrammart::Zuordnung));
        CHECK(c2d::ausgabe::nichtVerfuegbar(c2d::ausgabe::mermaid(), Diagrammart::Zuordnung) ==
              "Mapping table is not available in mermaid");
    }
}
