/**
 * @file test_Wahrheitstabelle.cpp
 * @brief Wahrheitstabellen: die Logik einer Bedingung (Leser), Tabelle mit Kurzschluss, MC/DC, Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Wahrheitstabelle.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>

using namespace c2d::diagramm;
using c2d::modell::Logik;

namespace
{

/// Die erste Funktion mit Rumpf.
c2d::modell::Funktion funktion(std::string_view quelle, bool cpp = false)
{
    c2d::sprache::Ergebnis e = (cpp ? c2d::sprache::cpp() : c2d::sprache::c()).lies(quelle, cpp ? "t.cpp" : "t.c");
    for (c2d::modell::Funktion& f : e.einheit.funktionen)
        if (f.rumpf) return std::move(f);
    FAIL("no function with a body");
    return {};
}

/// Der Baum als Text: Und(…), Oder(…), Nicht(…), Atome in eckigen Klammern.
std::string bild(const Logik& l)
{
    switch (l.art)
    {
        case Logik::Art::Atom: return "[" + l.text + "]" + (l.mitAufruf ? "*" : "");
        case Logik::Art::Nicht: return "!" + bild(l.teile[0]);
        case Logik::Art::Und:
        case Logik::Art::Oder:
        {
            std::string s = l.art == Logik::Art::Und ? "Und(" : "Oder(";
            for (std::size_t i = 0; i < l.teile.size(); ++i) s += (i ? "," : "") + bild(l.teile[i]);
            return s + ")";
        }
    }
    return {};
}

/// Die Logik der Bedingung in der ersten Anweisung (oder dem ersten Kind) des Rumpfs.
const c2d::modell::Anweisung& erste(const c2d::modell::Funktion& f)
{
    REQUIRE(f.rumpf);
    REQUIRE(!f.rumpf->kinder.empty());
    return *f.rumpf->kinder.front();
}

/// Die Zeilen der Tabelle als "nummer:Werte=Ergebnis" mit T/F/-, MC/DC-Zeilen mit *.
std::string zeilen(const Wahrheitstabelle& t)
{
    std::string s;
    for (const Wahrheitstabelle::Zeile& z : t.zeilen)
    {
        s += std::to_string(z.nummer) + ":";
        for (const Wert w : z.werte) s += w == Wert::Wahr ? 'T' : w == Wert::Falsch ? 'F' : '-';
        s += z.ergebnis ? "=T" : "=F";
        if (z.mcdc) s += '*';
        s += ' ';
    }
    return s;
}

}  // namespace

TEST_SUITE("Wahrheitstabelle")
{
    TEST_CASE("Leser: die Logik einer Bedingung — Oder vor Und vor Nicht, Klammern, Atome")
    {
        const auto logik = [](std::string_view rumpf, bool cpp = false) {
            const c2d::modell::Funktion f = funktion("void f(void) { " + std::string(rumpf) + " }", cpp);
            const c2d::modell::Anweisung& a = erste(f);
            return a.logik ? a.logikText + " => " + bild(*a.logik) : std::string("-");
        };
        CHECK(logik("if ((a && b) || !c) x();") == "(a && b) || !c => Oder(Und([a],[b]),![c])");
        CHECK(logik("if (x > 5 && pruefe(x)) y();") == "x > 5 && pruefe(x) => Und([x > 5],[pruefe(x)]*)");
        CHECK(logik("if (!x == y || z) w();") == "!x == y || z => Oder([!x == y],[z])");  // ! nur vor Einzelnem
        CHECK(logik("if (!p->ok && !(a || b)) w();") == "!p->ok && !(a || b) => Und(![p->ok],!Oder([a],[b]))");
        CHECK(logik("while (a && b && c) w();") == "a && b && c => Und([a],[b],[c])");
        CHECK(logik("ok = a || b;") == "a || b => Oder([a],[b])");
        CHECK(logik("return a && (b || c);") == "a && (b || c) => Und([a],Oder([b],[c]))");
        CHECK(logik("r = (a || b) ? 1 : 2;") == "(a || b) => Oder([a],[b])");
        CHECK(logik("if (a and not b) w();", true) == "a and not b => Und([a],![b])");
        // weniger als zwei Atome: keine Tabelle
        CHECK(logik("if (a && a) w();") == "-");
        CHECK(logik("if (a) w();") == "-");
        CHECK(logik("x = a + b;") == "-");
    }

    TEST_CASE("Tabelle: jede Kombination, Kurzschluss gibt -, MC/DC nach Unique-Cause mit n+1 Zeilen")
    {
        const c2d::modell::Funktion f = funktion("int f(int a, int b)\n{\n  return a && b;\n}\n");
        const std::vector<Wahrheitstabelle> alle = wahrheitstabellenVon(f);
        REQUIRE(alle.size() == 1);
        const Wahrheitstabelle& t = alle[0];
        CHECK(t.funktion == "f");
        CHECK(t.quellZeile == 3);
        CHECK(t.bedingung == "a && b");
        REQUIRE(t.atome.size() == 2);
        CHECK(zeilen(t) == "1:F-=F 2:F-=F* 3:TF=F* 4:TT=T* ");
        // Unique-Cause: b in 3/4; a braucht 2/4 — gleiches b, nur a anders. Kleinste Menge: 2, 3, 4
        CHECK(t.paare[0] == std::pair<std::size_t, std::size_t>{2, 4});
        CHECK(t.paare[1] == std::pair<std::size_t, std::size_t>{3, 4});
        CHECK(t.kleinste);
        CHECK_FALSE(t.gekuerzt);
        CHECK(t.zeilen[1].mcdc);  // Zeile 2 gehoert dazu, obwohl b dort nicht ausgewertet wird
        CHECK(wahrheitstabelleVon(f, 3).has_value());
        CHECK_FALSE(wahrheitstabelleVon(f, 2).has_value());
        CHECK(zeilenMitWahrheitstabelle(f) == std::vector<int>{3});
    }

    TEST_CASE("MC/DC: (a && b) || c braucht vier Zeilen; ein Atom ohne Paar gibt einen Hinweis")
    {
        const Wahrheitstabelle t = wahrheitstabellenVon(funktion("void f(void) { if ((a && b) || c) x(); }")).front();
        std::size_t mcdc = 0;
        for (const Wahrheitstabelle::Zeile& z : t.zeilen) mcdc += z.mcdc ? 1 : 0;
        CHECK(mcdc == 4);
        for (const auto& [eins, zwei] : t.paare)
        {
            CHECK(eins != 0);
            CHECK(t.zeilen[eins - 1].ergebnis != t.zeilen[zwei - 1].ergebnis);
        }
        // b aendert allein nie etwas: a || (a && b)
        const Wahrheitstabelle ohne = wahrheitstabellenVon(funktion("void f(void) { if (a || (a && b)) x(); }")).front();
        CHECK(ohne.paare[1] == std::pair<std::size_t, std::size_t>{0, 0});
        REQUIRE(ohne.hinweise.size() == 1);
        CHECK(ohne.hinweise[0].starts_with("B (b): no MC/DC pair"));
    }

    TEST_CASE("Ueber sechs Atome nur die MC/DC-Zeilen, mit Hinweis")
    {
        const Wahrheitstabelle t =
            wahrheitstabellenVon(funktion("void f(void) { if (a && b && c && d && e && g && h) x(); }")).front();
        CHECK(t.atome.size() == 7);
        CHECK(t.gekuerzt);
        CHECK(t.zeilen.size() == 8);  // n+1 bei einer reinen Und-Kette
        CHECK(t.hinweise.back() == "7 conditions: only the MC/DC rows of 128");
    }

    TEST_CASE("Anordnung und Formate: SVG, draw.io und csv schreiben die Tabelle; Mermaid und DOT nicht")
    {
        const std::vector<Wahrheitstabelle> t =
            wahrheitstabellenVon(funktion("void f(void)\n{\n  if (x > 5 && pruefe(x)) y();\n  ok = a || b;\n}\n"));
        REQUIRE(t.size() == 2);
        const std::span<const Wahrheitstabelle> beide(t);

        const TabellenAnordnung a = ordneAn(beide);
        for (const TabellenAnordnung::Zelle& z : a.zellen)
        {
            CHECK(z.ort.x + z.ort.breite <= a.breite);
            CHECK(z.ort.y + z.ort.hoehe <= a.hoehe);
        }
        // der Kopf eines Atoms mit Aufruf ist markiert; die zweite Tabelle steht unter der ersten
        CHECK(std::any_of(a.zellen.begin(), a.zellen.end(),
                          [](const TabellenAnordnung::Zelle& z) { return z.kopf && z.zeilen.front() == "B (call)"; }));
        double untenEins = 0;
        double obenZwei = 1e9;
        for (const TabellenAnordnung::Zelle& z : a.zellen)
            (z.tabelle == 0 ? untenEins : obenZwei) =
                z.tabelle == 0 ? std::max(untenEins, z.ort.y + z.ort.hoehe) : std::min(obenZwei, z.ort.y);
        CHECK(obenZwei > untenEins);

        using c2d::ausgabe::Diagrammart;
        const std::string csv = c2d::ausgabe::csv().schreibe(beide, {});
        CHECK(csv ==
              "f(): line 3;x > 5 && pruefe(x)\n#;A: x > 5;B: pruefe(x);Result;MC/DC\n1;F;-;F;\n2;F;-;F;A\n3;T;F;F;B\n"
              "4;T;T;T;A, B\n\nf(): line 4;a || b\n#;A: a;B: b;Result;MC/DC\n1;F;F;F;A, B\n2;F;T;T;B\n3;T;-;T;A\n"
              "4;T;-;T;\n");
        const std::string svg = c2d::ausgabe::svg().schreibe(beide, {});
        CHECK(svg.find(">MC/DC</tspan>") != std::string::npos);
        CHECK(svg.find("<title>pruefe(x)</title>") != std::string::npos);
        CHECK(c2d::ausgabe::drawio().schreibe(beide, {}).find("value=\"Result\"") != std::string::npos);
        CHECK_FALSE(c2d::ausgabe::mermaid().kann(Diagrammart::Wahrheitstabelle));
        CHECK(c2d::ausgabe::nichtVerfuegbar(c2d::ausgabe::dot(), Diagrammart::Wahrheitstabelle) ==
              "Truth table is not available in dot");
    }
}
