/**
 * @file test_Struktogramm.cpp
 * @brief Das Struktogramm (Nassi-Shneiderman): Ableitung, Anordnung, Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>

using namespace c2d::diagramm;

namespace
{

Struktogramm struktogramm(std::string_view quelle, const c2d::sprache::LeseOptionen& optionen = {},
                          bool cpp = false)
{
    const c2d::sprache::Ergebnis e =
        (cpp ? c2d::sprache::cpp() : c2d::sprache::c()).lies(quelle, cpp ? "t.cpp" : "t.c", optionen);
    for (const c2d::modell::Funktion& f : e.einheit.funktionen)
        if (f.rumpf) return struktogrammVon(f);
    FAIL("no function with a body");
    return {};
}

/// Die Bloecke einer Folge als "Art:Text", durch Strichpunkt.
std::string bild(const std::vector<Block>& bloecke)
{
    static constexpr const char* kKuerzel[] = {"A", "R", "X", "M", "IF", "SW", "WH", "DO", "EW", "TRY", "VAR", "BED"};
    std::string s;
    for (const Block& b : bloecke)
    {
        s += kKuerzel[static_cast<int>(b.art)];
        s += ':' + b.text;
        if (!b.spalten.empty())
        {
            s += '[';
            for (std::size_t i = 0; i < b.spalten.size(); ++i)
                s += (i ? "|" : "") + b.spalten[i].titel + "=" + bild(b.spalten[i].bloecke);
            s += ']';
        }
        s += ';';
    }
    return s;
}

}  // namespace

TEST_SUITE("Struktogramm")
{
    TEST_CASE("Ableitung: Anweisungen, Aufrufe, if/else, Schleifen mit dem Text der Quelle")
    {
        const Struktogramm d = struktogramm(
            "int f(int n)\n{\n  int s = 0;\n  melde(s);\n  if (n > 0) s = 1; else { s = 2; }\n"
            "  while (n) n--;\n  do { s++; } while (s < 3);\n  for (int i = 0; i < n; i++) s += i;\n"
            "  for (;;) { if (s) break; }\n  return s;\n}\n");
        CHECK(d.titel == "f(int n)");
        CHECK(d.quellZeile == 1);
        CHECK(bild(d.bloecke) ==
              "A:int s = 0;R:melde(s);IF:n > 0[yes=A:s = 1;|no=A:s = 2;];WH:while (n)[=A:n--;];"
              "DO:while (s < 3)[=A:s++;];WH:for (int i = 0; i < n; i++)[=A:s += i;];"
              "EW:forever[=IF:s[yes=X:break;|no=];];X:return s;");
        CHECK_FALSE(d.kehrtNieZurueck);
        CHECK(d.bloecke[0].quellZeile == 3);
        CHECK(d.bloecke[1].aufrufe == std::vector<std::string>{"melde"});
    }

    TEST_CASE("Ableitung: while (1) ist forever; kehrt nie zurueck, Code dahinter unerreichbar")
    {
        const Struktogramm d = struktogramm("int main(void)\n{\n  while (1) { takt(); }\n  return 0;\n}\n");
        CHECK(bild(d.bloecke) == "EW:forever[=R:takt();];X:return 0;");
        CHECK(d.kehrtNieZurueck);
        CHECK_FALSE(d.bloecke[0].unerreichbar);
        CHECK(d.bloecke[1].unerreichbar);
    }

    TEST_CASE("Ableitung: switch — Spalten je Fall, leere teilen sich eine, break entfaellt, falls through, default zuletzt")
    {
        const Struktogramm d = struktogramm(
            "void f(int x)\n{\n  switch (x) {\n  default: a(); break;\n  case 0:\n  case 1: b(); break;\n"
            "  case 2: c();\n  case 3: d(); return;\n  }\n}\n");
        CHECK(bild(d.bloecke) == "SW:x[0, 1=R:b();|2=R:c();X:falls through;|3=R:d();X:return;|default=R:a();];");
        // ohne default: eine leere Spalte otherwise
        CHECK(bild(struktogramm("void g(int x) { switch (x) { case 1: a(); break; } }").bloecke) ==
              "SW:x[1=R:a();|otherwise=];");
    }

    TEST_CASE("Ableitung: goto ist ein Aussprung mit Hinweis, die Marke ein Block")
    {
        const Struktogramm d = struktogramm("int f(int x)\n{\n  if (x) goto aus;\n  return 1;\naus:\n  return 0;\n}\n");
        CHECK(bild(d.bloecke) == "IF:x[yes=X:goto aus;|no=];X:return 1;M:aus:;X:return 0;");
        REQUIRE(d.hinweise.size() == 1);
        CHECK(d.hinweise[0].zeile == 3);
        CHECK(d.hinweise[0].text.find("goto aus: not structured") == 0);
        CHECK_FALSE(d.bloecke[2].unerreichbar);  // die Marke erreicht der Sprung
    }

    TEST_CASE("Ableitung: try/catch, #if als Varianten, ein bedingter Bereich")
    {
        const Struktogramm t = struktogramm("void f() { try { a(); } catch (const E& e) { b(); } }", {}, true);
        CHECK(bild(t.bloecke) == "TRY:try[try=R:a();|catch (const E& e)=R:b();];");

        c2d::sprache::LeseOptionen varianten;
        varianten.varianten = true;
        const Struktogramm v = struktogramm("void f(void)\n{\n#ifdef A\n  a();\n#else\n  b();\n#endif\n}\n", varianten);
        REQUIRE(v.bloecke.size() == 1);
        CHECK(v.bloecke[0].art == BlockArt::Varianten);
        REQUIRE(v.bloecke[0].spalten.size() == 2);
        CHECK(v.bloecke[0].spalten[1].gilt);
        CHECK(v.bloecke[0].spalten[1].titel.ends_with("(active)"));

        c2d::sprache::LeseOptionen bereiche;
        bereiche.bereiche = true;
        const Struktogramm b = struktogramm("void f(void)\n{\n#ifndef A\n  a();\n#endif\n}\n", bereiche);
        REQUIRE(b.bloecke.size() == 1);
        CHECK(b.bloecke[0].art == BlockArt::Bedingt);
        CHECK(bild(b.bloecke[0].spalten[0].bloecke) == "R:a();");
    }

    TEST_CASE("Bloecke der Reihe nach; der Block zu einer Zeile")
    {
        const Struktogramm d = struktogramm("void f(int x)\n{\n  a = 1;\n  if (x)\n    b = 2;\n  c = 3;\n}\n");
        const std::vector<const Block*> alle = bloeckeVon(d);
        REQUIRE(alle.size() == 4);  // a, if, b, c — der if vor seiner Spalte
        CHECK(alle[1]->art == BlockArt::Verzweigung);
        CHECK(alle[2]->text == "b = 2");
        CHECK(blockZurZeile(d, 5) == 2);
        CHECK(blockZurZeile(d, 4) == 1);
        CHECK(blockZurZeile(d, 7) == 3);  // die `}`: der Block davor
        CHECK(blockZurZeile(d, 1) == kKeine);
    }

    TEST_CASE("Anordnung: jeder Block in seinem Eltern-Block, Spalten fuellen die Breite, Text zwischen den Diagonalen")
    {
        const Struktogramm d = struktogramm(
            "int f(int x)\n{\n  if (ein_langer_name_fuer_eine_bedingung(x) > 0) { a(); b = 1; } else return 0;\n"
            "  switch (x) { case 1: c(); break; case 2: break; default: d(); }\n  while (x) x--;\n  return 1;\n}\n");
        const StruktoAnordnung a = ordneAn(d);
        const std::vector<const Block*> alle = bloeckeVon(d);
        REQUIRE(a.bloecke.size() == alle.size());
        const auto innen = [](const Rechteck& klein, const Rechteck& gross) {
            return klein.x >= gross.x - 0.01 && klein.y >= gross.y - 0.01 && klein.x + klein.breite <= gross.x + gross.breite + 0.01 &&
                   klein.y + klein.hoehe <= gross.y + gross.hoehe + 0.01;
        };
        // je Block mit Spalten: seine Bloecke liegen in ihm, unter seinem Kopf, ohne sich zu ueberlappen
        for (const StruktoAnordnung::Feld& f : a.felder)
        {
            CHECK(f.ort.x >= 0);
            CHECK(f.ort.x + f.ort.breite <= a.breite);
            CHECK(f.ort.y + f.ort.hoehe <= a.hoehe);
        }
        std::size_t i = 0;
        const auto pruefe = [&](const auto& selbst, const std::vector<Block>& bloecke, const Rechteck* eltern) -> void {
            double unten = -1;
            for (const Block& b : bloecke)
            {
                const Rechteck r = a.bloecke[i++];
                if (eltern != nullptr) CHECK(innen(r, *eltern));
                CHECK(r.y >= unten - 0.01);  // untereinander
                unten = r.y + r.hoehe;
                for (const Spalte& s : b.spalten) selbst(selbst, s.bloecke, &r);
            }
        };
        pruefe(pruefe, d.bloecke, nullptr);

        // der Text eines Kopfs mit Diagonalen passt zwischen sie: die Felder darunter beginnen tiefer
        for (const StruktoAnordnung::Feld& f : a.felder)
        {
            if (f.art != StruktoAnordnung::FeldArt::Kopf || f.block == kKeine) continue;
            if (alle[f.block]->art != BlockArt::Verzweigung) continue;
            const double halb = static_cast<double>(alle[f.block]->text.size()) * 7.2 / 2;
            for (const Linienzug& l : a.linien)
            {
                if (l.size() != 2 || l[0].y != f.ort.y) continue;  // die Diagonalen beginnen oben am Kopf
                // die Diagonale auf Hoehe der Unterkante des Texts
                const double y = f.text.y + 8.5;
                const double x = l[0].x + (l[1].x - l[0].x) * (y - l[0].y) / (l[1].y - l[0].y);
                CHECK((x <= f.text.x - halb || x >= f.text.x + halb));
            }
        }
    }

    TEST_CASE("Formate: SVG und draw.io zeichnen es, Mermaid und DOT sagen ab")
    {
        const Struktogramm d = struktogramm("int f(int x) { if (x) return 1; for (;;) { a(); } }");
        using c2d::ausgabe::Diagrammart;
        REQUIRE(c2d::ausgabe::svg().kann(Diagrammart::Struktogramm));
        const std::string svg = c2d::ausgabe::svg().schreibe(d, {});
        CHECK(svg.starts_with("<svg xmlns="));
        CHECK(svg.find(">forever</tspan>") != std::string::npos);
        CHECK(svg.find(">f(int x)</tspan>") != std::string::npos);
        REQUIRE(c2d::ausgabe::drawio().kann(Diagrammart::Struktogramm));
        const std::string drawio = c2d::ausgabe::drawio().schreibe(d, {});
        CHECK(drawio.find("as=\"sourcePoint\"") != std::string::npos);
        CHECK(drawio.find("value=\"forever\"") != std::string::npos);
        CHECK_FALSE(c2d::ausgabe::mermaid().kann(Diagrammart::Struktogramm));
        CHECK_FALSE(c2d::ausgabe::dot().kann(Diagrammart::Struktogramm));
        CHECK_FALSE(c2d::ausgabe::csv().kann(Diagrammart::Struktogramm));
        CHECK(c2d::ausgabe::nichtVerfuegbar(c2d::ausgabe::dot(), Diagrammart::Struktogramm) ==
              "Nassi-Shneiderman is not available in dot");
    }
}
