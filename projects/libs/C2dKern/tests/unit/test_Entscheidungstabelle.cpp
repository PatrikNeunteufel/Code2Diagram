/**
 * @file test_Entscheidungstabelle.cpp
 * @brief Die Entscheidungstabelle einer Funktion: Wege, switch, Widersprueche, Grenze, Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Entscheidungstabelle.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>

using namespace c2d::diagramm;

namespace
{

Entscheidungstabelle tabelle(std::string_view quelle)
{
    const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(quelle, "t.c");
    for (const c2d::modell::Funktion& f : e.einheit.funktionen)
        if (f.rumpf) return entscheidungstabelleVon(f);
    FAIL("no function with a body");
    return {};
}

/// Je Regel "Eintraege|Aktionen": Eintraege durch Komma (- = nicht gefragt), Aktionen als ihre Texte durch Komma.
std::string regeln(const Entscheidungstabelle& t)
{
    std::string s;
    for (const Entscheidungstabelle::Regel& r : t.regeln)
    {
        for (std::size_t b = 0; b < r.eintraege.size(); ++b) s += (b ? "," : "") + (r.eintraege[b].empty() ? "-" : r.eintraege[b]);
        s += '|';
        bool erste = true;
        for (std::size_t a = 0; a < r.aktionen.size(); ++a)
            if (r.aktionen[a])
            {
                s += (erste ? "" : ",") + t.aktionen[a].text;
                erste = false;
            }
        s += "; ";
    }
    return s;
}

}  // namespace

TEST_SUITE("Entscheidungstabelle")
{
    TEST_CASE("Je Weg eine Regel; nicht gefragte Bedingungen leer; gleicher Text an zwei Stellen bleibt zwei Aktionen")
    {
        const Entscheidungstabelle t = tabelle(
            "void f(int a, int b)\n{\n  if (a) { x = 1; return; }\n  if (b > 2) return;\n  else y();\n  return;\n}\n");
        CHECK(t.funktion == "f");
        REQUIRE(t.bedingungen.size() == 2);
        CHECK(t.bedingungen[0].text == "a");
        CHECK(t.bedingungen[1].quellZeile == 4);
        CHECK(regeln(t) == "Y,-|x = 1,return; N,Y|return; N,N|y(),return; ");
        REQUIRE(t.aktionen.size() == 5);  // x = 1, return (3), return (4), y(), return (6) — die return getrennt
        CHECK(t.aktionen[1].quellZeile == 3);
        CHECK(t.aktionen[2].quellZeile == 4);
        CHECK(t.hinweise.empty());
    }

    TEST_CASE("switch: erweiterter Eintrag, Durchfallen, break im if, otherwise")
    {
        const Entscheidungstabelle t = tabelle(
            "void f(int x, int y)\n{\n  switch (x) {\n  case 0:\n  case 1: a(); break;\n  case 2: b();\n"
            "  case 3: if (y) break; c(); break;\n  }\n  e();\n}\n");
        REQUIRE(t.bedingungen.size() == 2);
        CHECK(t.bedingungen[0].fall);
        CHECK(regeln(t) == "0, 1,-|a(),e(); 2,Y|b(),e(); 2,N|b(),c(),e(); 3,Y|e(); 3,N|c(),e(); otherwise,-|e(); ");
    }

    TEST_CASE("Widerspruch: dieselbe Bedingung zweimal gibt keine unmoeglichen Regeln — ausser sie ruft etwas")
    {
        const Entscheidungstabelle t = tabelle("void f(int a) { if (a) x = 1; if (a) y = 2; else z = 3; }");
        CHECK(regeln(t) == "Y|x = 1,y = 2; N|z = 3; ");
        REQUIRE(t.hinweise.size() == 1);
        CHECK(t.hinweise[0].starts_with("2 impossible rule(s) left out"));
        // lies() kann beim zweiten Mal anders ausgehen
        const Entscheidungstabelle r = tabelle("void f(void) { if (lies()) x(); if (lies()) y(); }");
        CHECK(r.regeln.size() == 4);
        CHECK(r.hinweise.empty());
    }

    TEST_CASE("Entschieden nur bis zur Aenderung: ein Aufruf, ein geschriebener Name, ein Zeiger macht sie wieder offen")
    {
        // wie LLC CHECK_EEPROM_INIT: dazwischen setzt ein Aufruf den Namen ueber &name neu
        const Entscheidungstabelle t = tabelle(
            "void f(void)\n{\n  lies(&fehler);\n  if (fehler == 0) a();\n  lies(&fehler);\n  if (fehler == 0) b();\n}\n");
        REQUIRE(t.bedingungen.size() == 2);
        CHECK_FALSE(t.bedingungen[0].erneut);
        CHECK(t.bedingungen[1].erneut);
        CHECK(t.bedingungen[1].quellZeile == 6);
        CHECK(zeilenText(t.bedingungen[1]) == "fehler == 0 (again, line 6)");
        CHECK(t.regeln.size() == 4);
        CHECK(t.hinweise.empty());

        // ohne Aufruf, aber der Name wird geschrieben: ebenso neu gefragt
        CHECK(tabelle("void f(int x) { if (x > 0) a(); x = 5; if (x > 0) b(); }").regeln.size() == 4);
        CHECK(tabelle("void f(int x) { if (x > 0) a(); x++; if (x > 0) b(); }").regeln.size() == 4);
        // durch einen Zeiger geschrieben: jede Bedingung ist wieder offen
        CHECK(tabelle("void f(int x, int *p) { if (x > 0) y = 1; *p = 5; if (x > 0) y = 2; }").regeln.size() == 4);
        // ein anderer Name geschrieben: sie bleibt entschieden
        const Entscheidungstabelle bleibt = tabelle("void f(int x) { if (x > 0) y = 1; z = 5; if (x > 0) y = 2; }");
        CHECK(bleibt.regeln.size() == 2);
        CHECK(bleibt.hinweise.front().starts_with("2 impossible rule(s) left out"));
    }

    TEST_CASE("Schleife als eine Aktion, goto endet eine Regel, ?: verzweigt")
    {
        const Entscheidungstabelle t = tabelle(
            "int f(int n)\n{\n  while (n > 0) { if (n == 3) return 1; n--; }\n  r = n ? 1 : 2;\n  if (n < 0) goto aus;\n"
            "  return r;\naus:\n  return -1;\n}\n");
        CHECK(regeln(t) ==
              "Y,Y|while (n > 0) ...,r = n ? 1 : 2,goto aus; Y,N|while (n > 0) ...,r = n ? 1 : 2,return r; "
              "N,Y|while (n > 0) ...,r = n ? 1 : 2,goto aus; N,N|while (n > 0) ...,r = n ? 1 : 2,return r; ");
        CHECK(t.bedingungen[0].text == "n");
        CHECK(std::find(t.hinweise.begin(), t.hinweise.end(),
                        "loops are one action each - what happens inside them is not in the table") != t.hinweise.end());
        CHECK(std::find(t.hinweise.begin(), t.hinweise.end(), "goto ends a rule at the jump") != t.hinweise.end());
    }

    TEST_CASE("Ueber 32 Regeln keine Tabelle, nur ein Hinweis")
    {
        const Entscheidungstabelle t =
            tabelle("void f(void) { if (a) x(); if (b) x(); if (c) x(); if (d) x(); if (e) x(); if (g) x(); }");
        CHECK(t.regeln.empty());
        REQUIRE(!t.hinweise.empty());
        CHECK(t.hinweise.back() == "64 rules - too many for a decision table");
    }

    TEST_CASE("Anordnung und Formate: klassisch mit doppelter Linie; SVG, draw.io, csv; Mermaid und DOT nicht")
    {
        const Entscheidungstabelle t = tabelle("int f(int a) { if (a) return 1; return 0; }");
        const TabellenAnordnung a = ordneAn(t);
        CHECK(a.linien.size() == 2);                 // die doppelte Linie
        CHECK(a.knotenOrte.size() == 1 + 2);         // eine Bedingung, zwei Aktionen
        for (const TabellenAnordnung::Zelle& z : a.zellen) CHECK(z.ort.x + z.ort.breite <= a.breite);

        using c2d::ausgabe::Diagrammart;
        CHECK(c2d::ausgabe::csv().schreibe(t, {}) ==
              "f(): decision table\nConditions;R1;R2\na;Y;N\nActions\nreturn 1;X;\nreturn 0;;X\n");
        CHECK(c2d::ausgabe::svg().schreibe(t, {}).find(">R2</tspan>") != std::string::npos);
        CHECK(c2d::ausgabe::drawio().schreibe(t, {}).find("value=\"R1\"") != std::string::npos);
        CHECK_FALSE(c2d::ausgabe::mermaid().kann(Diagrammart::Entscheidungstabelle));
        CHECK(c2d::ausgabe::nichtVerfuegbar(c2d::ausgabe::dot(), Diagrammart::Entscheidungstabelle) ==
              "Decision table is not available in dot");
    }
}
