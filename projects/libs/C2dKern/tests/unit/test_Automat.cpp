/**
 * @file test_Automat.cpp
 * @brief Zustandsautomaten: erkennen, was einer ist (und was nicht), anordnen, schreiben
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "Beispiele.hpp"
#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <string>
#include <vector>

using namespace c2d::diagramm;

namespace
{

std::vector<Zustandsdiagramm> automaten(std::string_view quelle, bool cpp = false)
{
    const c2d::sprache::Ergebnis e = (cpp ? c2d::sprache::cpp() : c2d::sprache::c()).lies(quelle, cpp ? "t.cpp" : "t.c");
    return automatenVon(e.einheit);
}

/// `von -> nach : bedingung` je Uebergang, mit den Namen der Zustaende (der Punkt von aussen: `*`).
std::vector<std::string> wege(const Zustandsdiagramm& d)
{
    std::vector<std::string> aus;
    const auto name = [&](std::size_t z) { return d.zustaende[z].aussen ? std::string("*") : d.zustaende[z].name; };
    for (const Uebergang& u : d.uebergaenge) aus.push_back(name(u.von) + " -> " + name(u.nach) + " : " + u.bedingung);
    return aus;
}

bool ueberlappen(const Rechteck& a, const Rechteck& b)
{
    return a.x < b.x + b.breite && b.x < a.x + a.breite && a.y < b.y + b.hoehe && b.y < a.y + a.hoehe;
}

}  // namespace

TEST_SUITE("Zustandsautomat")
{
    TEST_CASE("Erkannt: switch ueber den Zustand, der sich selbst weiterschaltet (beispiele/automat.c)")
    {
        const std::vector<Zustandsdiagramm> alle = automaten(beispiele::kAutomat);
        REQUIRE(alle.size() == 1);
        const Zustandsdiagramm& d = alle[0];
        CHECK(d.variable == "motor.zustand");
        CHECK(d.funktion == "motor_takt");
        CHECK(d.titel == "motor.zustand in motor_takt()");
        CHECK(d.zeile == 13);
        REQUIRE(d.zustaende.size() == 5);
        CHECK(d.zustaende[0].name == "AUS");
        CHECK(d.zustaende[0].zeile == 14);
        CHECK(d.zustaende[3].name == "FEHLER");
        CHECK(d.zustaende[4].aussen);
        CHECK(wege(d) == std::vector<std::string>{
                             "AUS -> STARTE : bereit",
                             "STARTE -> FEHLER : stoerung && motor.versuche > 3",
                             "STARTE -> LAEUFT : !(stoerung)",
                             "LAEUFT -> FEHLER : stoerung",
                             "LAEUFT -> AUS : !(stoerung) && !bereit",
                             "* -> AUS : in motor_quittiere()",
                         });
    }

    TEST_CASE("Kein Automat: ein switch, der nur rechnet, zurueckgibt oder anderes setzt")
    {
        // wie dauer_ms() der Ampel: je Fall ein return
        CHECK(automaten("int f(int p) { switch (p) { case 1: return 5; case 2: return 4; } return 0; }").empty());
        // nur ein Uebergang: zu wenig, um sicher zu sein
        CHECK(automaten("void f(void) { switch (s) { case A: s = B; break; case B: break; } }").empty());
        // setzt eine andere Variable
        CHECK(automaten("void f(void) { switch (s) { case A: t = B; break; case B: t = A; break; } }").empty());
        // vergleicht nur (==), weist nicht zu
        CHECK(automaten("void f(void) { switch (s) { case A: if (s == B) x(); break; case B: s += 1; break; } }").empty());
    }

    TEST_CASE("Mehrere Marken, default, ein Wert ohne Fall, ein anderer switch darin, gleiche Wege zusammen")
    {
        const std::vector<Zustandsdiagramm> alle = automaten(
            "void f(int e) {\n"
            "  switch (s) {\n"
            "  case A: case B: s = C; break;\n"
            "  case C: switch (e) { case 1: s = A; break; default: s = D; break; } break;\n"
            "  default: s = A; break;\n"
            "  }\n"
            "  if (x) s = C; else s = C;\n"
            "}\n");
        REQUIRE(alle.size() == 1);
        const Zustandsdiagramm& d = alle[0];
        std::vector<std::string> namen;
        for (const Zustand& z : d.zustaende) namen.push_back(z.aussen ? "*" : z.name);
        CHECK(namen == std::vector<std::string>{"A", "B", "C", "default", "D", "*"});
        CHECK(wege(d) == std::vector<std::string>{
                             "A -> C : ",
                             "B -> C : ",
                             "C -> A : e == 1",
                             "C -> D : default",
                             "default -> A : ",
                             "* -> C : in f(): x || in f(): !(x)",
                         });
    }

    TEST_CASE("Von aussen auch ueber einen anderen Weg zum selben Glied; nur Werte, die Zustaende sind")
    {
        const std::vector<Zustandsdiagramm> alle = automaten(
            "typedef struct { int st; } H;\n"
            "H eeprom;\n"
            "void lauf(H *h) { switch (h->st) { case LEER: h->st = VOLL; break; case VOLL: h->st = LEER; break; } }\n"
            "void init(void) { eeprom.st = 0; eeprom.st = LEER; }\n");
        REQUIRE(alle.size() == 1);
        CHECK(alle[0].variable == "h->st");
        CHECK(wege(alle[0]) == std::vector<std::string>{"LEER -> VOLL : ", "VOLL -> LEER : ", "* -> LEER : in init()"});
    }

    TEST_CASE("Anordnung: je Zustand ein Knoten, je Uebergang eine Kante, nichts ueberlappt; der Punkt ist klein")
    {
        const Zustandsdiagramm d = automaten(beispiele::kAutomat).at(0);
        const FlussAnordnung a = ordneAn(d);
        REQUIRE(a.knoten.size() == d.zustaende.size());
        REQUIRE(a.kanten.size() == d.uebergaenge.size());
        for (std::size_t i = 0; i < a.knoten.size(); ++i)
            for (std::size_t k = i + 1; k < a.knoten.size(); ++k) CHECK_FALSE(ueberlappen(a.knoten[i], a.knoten[k]));
        CHECK(a.knoten[4].breite < 30);
        CHECK(a.knoten[4].hoehe < 30);
    }

    TEST_CASE("Formate: Mermaid stateDiagram-v2, DOT mit Punkt, SVG mit Kreis, draw.io mit Ellipse")
    {
        const Zustandsdiagramm d = automaten(beispiele::kAutomat).at(0);
        CHECK(c2d::ausgabe::mermaid().schreibe(d, {}) == "%% motor.zustand in motor_takt()\n"
                                                         "stateDiagram-v2\n"
                                                         "    state \"AUS\" as s0\n"
                                                         "    state \"STARTE\" as s1\n"
                                                         "    state \"LAEUFT\" as s2\n"
                                                         "    state \"FEHLER\" as s3\n"
                                                         "    s0 --> s1 : bereit\n"
                                                         "    s1 --> s3 : stoerung #amp;#amp; motor.versuche #gt; 3\n"
                                                         "    s1 --> s2 : !(stoerung)\n"
                                                         "    s2 --> s3 : stoerung\n"
                                                         "    s2 --> s0 : !(stoerung) #amp;#amp; !bereit\n"
                                                         "    [*] --> s0 : in motor_quittiere()\n");
        CHECK(c2d::ausgabe::mermaid().schreibe(d, {.richtung = "LR"}).find("stateDiagram-v2\n    direction LR\n") !=
              std::string::npos);
        // ein zweiter Doppelpunkt (von aussen mit Bedingung) beendete die Beschreibung: Entity-Code
        Zustandsdiagramm mitDoppelpunkt = d;
        mitDoppelpunkt.uebergaenge.back().bedingung = "in init(): a; b";
        CHECK(c2d::ausgabe::mermaid().schreibe(mitDoppelpunkt, {}).ends_with("    [*] --> s0 : in init()#58; a#59; b\n"));

        const std::string dot = c2d::ausgabe::dot().schreibe(d, {});
        CHECK(dot.find("    s4 [shape=point, width=0.15];\n") != std::string::npos);
        CHECK(dot.find("    s0 [shape=box, style=rounded, label=\"AUS\"];\n") != std::string::npos);
        CHECK(dot.find("    s1 -> s3 [label=\"stoerung && motor.versuche > 3\"];\n") != std::string::npos);

        const std::string svg = c2d::ausgabe::svg().schreibe(d, {});
        CHECK(svg.find("<circle class=\"spitze\"") != std::string::npos);
        CHECK(svg.find(">FEHLER</tspan>") != std::string::npos);
        CHECK(svg.find(">stoerung &amp;&amp; motor.versuche &gt; 3</tspan>") != std::string::npos);

        const std::string drawio = c2d::ausgabe::drawio().schreibe(d, {});
        CHECK(drawio.find("style=\"ellipse;fillColor=#000000;") != std::string::npos);
        CHECK(drawio.find("source=\"s4\" target=\"s0\"") != std::string::npos);
    }
}
