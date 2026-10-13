/**
 * @file test_Ausgabe.cpp
 * @brief Die Formate: Mermaid und DOT, je Flussdiagramm und Strukturdiagramm
 *
 * Die Faelle "vom Quelltext bis zum Text" sind der Durchstich: Lexer, Leser,
 * Ableitung, Buendeln und Ausgabe in einem Zug.
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "Beispiele.hpp"
#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/sprache/Sprache.hpp"

using namespace c2d::diagramm;
using c2d::ausgabe::dot;
using c2d::ausgabe::mermaid;
using c2d::ausgabe::Optionen;

namespace
{

constexpr std::string_view kKleineTypen = "enum Art { A, B = 2 };\n"
                                          "struct P { int x; enum Art art; struct P *next; };\n";

c2d::modell::Einheit lies(std::string_view text, std::string_view name)
{
    return c2d::sprache::c().lies(text, name).einheit;
}

Flussdiagramm vierFormen()
{
    Flussdiagramm d("t");
    const KnotenId start = d.neuerKnoten(KnotenArt::Start, "f()");
    const KnotenId frage = d.neuerKnoten(KnotenArt::Verzweigung, "a < \"b\"");
    const KnotenId ruf = d.neuerKnoten(KnotenArt::Aufruf, "g()");
    const KnotenId zwei = d.neuerKnoten(Knoten{KnotenArt::Anweisung, {"x = 1", "y = 2"}, 0});
    d.verbinde(start, frage);
    d.verbinde(frage, ruf, "yes");
    d.verbinde(frage, zwei, "no");
    return d;
}

}  // namespace

TEST_SUITE("Ausgabe")
{
    TEST_CASE("Die Formate melden sich an; das erste ist die Vorgabe")
    {
        REQUIRE(c2d::ausgabe::formate().size() == 3);
        CHECK(c2d::ausgabe::formate()[0] == &mermaid());
        CHECK(c2d::ausgabe::format("dot") == &dot());
        CHECK(c2d::ausgabe::format("drawio") == &c2d::ausgabe::drawio());
        CHECK(c2d::ausgabe::format("svg") == nullptr);
        CHECK(mermaid().endung() == "mmd");
    }

    TEST_CASE("Mermaid: jede Knotenart hat ihre Form, Zeilen trennt <br>")
    {
        CHECK(mermaid().schreibe(vierFormen(), {.richtung = "LR"}) == "%% t\n"
                                                                      "flowchart LR\n"
                                                                      "    n0([\"f()\"])\n"
                                                                      "    n1{\"a #lt; #quot;b#quot;\"}\n"
                                                                      "    n2[[\"g()\"]]\n"
                                                                      "    n3[\"x = 1<br>y = 2\"]\n"
                                                                      "    n0 --> n1\n"
                                                                      "    n1 -->|\"yes\"| n2\n"
                                                                      "    n1 -->|\"no\"| n3\n");
    }

    TEST_CASE("Mermaid: was als Syntax oder HTML gelesen wuerde, wird Entity-Code")
    {
        Flussdiagramm d;
        d.neuerKnoten(KnotenArt::Anweisung, "a < b && c > d");
        d.neuerKnoten(KnotenArt::Anweisung, "printf(\"%d\\n\", x)");
        d.neuerKnoten(KnotenArt::Anweisung, "#define N 3");
        d.neuerKnoten(KnotenArt::Anweisung, "x[i] = {1}; f(y) | z");  // in "..." unkritisch

        CHECK(mermaid().schreibe(d, {}) == "flowchart TD\n"
                                           "    n0[\"a #lt; b #amp;#amp; c #gt; d\"]\n"
                                           "    n1[\"printf(#quot;%d\\n#quot;, x)\"]\n"
                                           "    n2[\"#35;define N 3\"]\n"
                                           "    n3[\"x[i] = {1}; f(y) | z\"]\n");
    }

    TEST_CASE("Lange Zeilen enden auf ... — vor dem Maskieren und ohne ein UTF-8-Zeichen zu trennen")
    {
        Flussdiagramm d;
        d.neuerKnoten(KnotenArt::Anweisung, "abcdefghij");
        d.neuerKnoten(KnotenArt::Anweisung, "\"\"\"\"\"\"");  // sechs Zeichen, maskiert 36
        d.neuerKnoten(KnotenArt::Anweisung, "ab\xC3\xA4\xC3\xB6\xC3\xBC" "cd");  // ab + drei Umlaute + cd

        CHECK(mermaid().schreibe(d, {.maxZeilenLaenge = 8}) ==
              "flowchart TD\n"
              "    n0[\"abcde...\"]\n"
              "    n1[\"#quot;#quot;#quot;#quot;#quot;#quot;\"]\n"
              "    n2[\"ab\xC3\xA4...\"]\n");  // bei Byte 5 stuende ein halber Umlaut
        CHECK(mermaid().schreibe(d, {.maxZeilenLaenge = 0}).find("abcdefghij") != std::string::npos);
        CHECK(dot().schreibe(d, {.maxZeilenLaenge = 8}).find("label=\"abcde...\"") != std::string::npos);
    }

    TEST_CASE("Mermaid: vom Quelltext bis zum Text (beispiele/verzweigung.c)")
    {
        const c2d::modell::Einheit e = lies(beispiele::kSummeBis, "verzweigung.c");
        REQUIRE(e.funktionen.size() == 1);
        const Flussdiagramm d = fasseAnweisungsfolgenZusammen(flussVon(e.funktionen[0]));

        CHECK(mermaid().schreibe(d, {}) == "%% summe_bis\n"
                                           "flowchart TD\n"
                                           "    n0([\"summe_bis(int grenze)\"])\n"
                                           "    n1[\"int summe = 0<br>int i = 1\"]\n"
                                           "    n2{\"grenze #lt; 0\"}\n"
                                           "    n3[\"return -1\"]\n"
                                           "    n4{\"i #lt;= grenze\"}\n"
                                           "    n5[\"summe += i<br>i++\"]\n"
                                           "    n6[\"return summe\"]\n"
                                           "    n7([\"End\"])\n"
                                           "    n0 --> n1\n"
                                           "    n1 --> n2\n"
                                           "    n2 -->|\"yes\"| n3\n"
                                           "    n2 -->|\"no\"| n4\n"
                                           "    n4 -->|\"yes\"| n5\n"
                                           "    n5 --> n4\n"
                                           "    n4 -->|\"no\"| n6\n"
                                           "    n3 --> n7\n"
                                           "    n6 --> n7\n");
    }

    TEST_CASE("DOT: Flussdiagramm")
    {
        CHECK(dot().schreibe(vierFormen(), {}) == "digraph \"t\" {\n"
                                                  "    rankdir=TB;\n"
                                                  "    node [fontname=\"Helvetica\", fontsize=10];\n"
                                                  "    edge [fontname=\"Helvetica\", fontsize=9];\n"
                                                  "    n0 [shape=box, style=rounded, label=\"f()\"];\n"
                                                  "    n1 [shape=diamond, label=\"a < \\\"b\\\"\"];\n"
                                                  "    n2 [shape=box, peripheries=2, label=\"g()\"];\n"
                                                  "    n3 [shape=box, label=\"x = 1\\ny = 2\"];\n"
                                                  "    n0 -> n1;\n"
                                                  "    n1 -> n2 [label=\"yes\"];\n"
                                                  "    n1 -> n3 [label=\"no\"];\n"
                                                  "}\n");
    }

    TEST_CASE("Mermaid: Datentypen als classDiagram")
    {
        const Strukturdiagramm d = strukturVon(lies(kKleineTypen, "t.c"));

        CHECK(mermaid().schreibe(d, {}) == "%% t.c\n"
                                           "classDiagram\n"
                                           "    class Art {\n"
                                           "        <<enumeration>>\n"
                                           "        A\n"
                                           "        B = 2\n"
                                           "    }\n"
                                           "    class P {\n"
                                           "        <<struct>>\n"
                                           "        int x\n"
                                           "        enum Art art\n"
                                           "        struct P *next\n"
                                           "    }\n"
                                           "    P --> Art : art\n"
                                           "    P --> P : next\n");
        CHECK(mermaid().schreibe(d, {.richtung = "LR"}).find("classDiagram\n    direction LR\n") !=
              std::string::npos);
    }

    TEST_CASE("DOT: Datentypen als record")
    {
        const Strukturdiagramm d = strukturVon(lies(kKleineTypen, "t.c"));

        CHECK(dot().schreibe(d, {.richtung = "LR"}) ==
              "digraph \"t.c\" {\n"
              "    rankdir=LR;\n"
              "    node [fontname=\"Helvetica\", fontsize=10];\n"
              "    edge [fontname=\"Helvetica\", fontsize=9];\n"
              "    k0 [shape=record, label=\"{enum Art|A\\lB = 2\\l}\"];\n"
              "    k1 [shape=record, label=\"{struct P|int x\\lenum Art art\\lstruct P *next\\l}\"];\n"
              "    k1 -> k0 [arrowhead=vee, label=\"art\"];\n"
              "    k1 -> k1 [arrowhead=vee, label=\"next\"];\n"
              "}\n");
    }

    TEST_CASE("Enthalten als Wert: Raute am Besitzer, in beiden Formaten")
    {
        const Strukturdiagramm d = strukturVon(lies(beispiele::kDatentypen, "datentypen.c"));

        const std::string m = mermaid().schreibe(d, {});
        CHECK(m.find("    Knoten *-- Punkt : ort\n") != std::string::npos);
        CHECK(m.find("    Knoten --> Knoten : naechster\n") != std::string::npos);
        CHECK(m.find("    Liste --> Knoten : kopf\n") != std::string::npos);
        CHECK(m.find("        <<union>>\n        int ganz\n") != std::string::npos);

        CHECK(dot().schreibe(d, {}).find("    k2 -> k1 [dir=back, arrowtail=diamond, label=\"ort\"];\n") !=
              std::string::npos);
    }

    TEST_CASE("Zeichen, die im Format gliedern, werden entschaerft")
    {
        Strukturdiagramm d;
        d.kaesten.push_back({KastenArt::Struktur, "anonymous 1", {{"a{b}|c<d>", "x", "\"y\""}}});

        CHECK(dot().schreibe(d, {}).find("label=\"{struct anonymous 1|a\\{b\\}\\|c\\<d\\> x \\\"y\\\"\\l}\"") !=
              std::string::npos);
        const std::string m = mermaid().schreibe(d, {});
        CHECK(m.find("    class anonymous_1 {\n") != std::string::npos);  // die Kennung ohne Leerzeichen
        // ohne die Klammern, die den Rumpf beenden; spitze Klammern als Tilde
        CHECK(m.find("        ab|c~d~ x \"y\"\n") != std::string::npos);
    }
}
