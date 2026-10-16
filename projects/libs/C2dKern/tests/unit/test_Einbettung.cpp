/**
 * @file test_Einbettung.cpp
 * @brief Aufrufziele, Einbetten und Herausloesen, die Anordnung und draw.io
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <cmath>
#include <map>

using namespace c2d::diagramm;

namespace
{

constexpr std::string_view kQuelle = R"c(
static int quadrat(int x) { return x * x; }
static int summe(int n)
{
    int s = 0;
    for (int i = 0; i < n; i++)
        s += quadrat(i);
    return s;
}
int main(void)
{
    int n = lies();
    if (n < 0) return 1;
    drucke(summe(n));
    return 0;
}
)c";

/// Das gebuendelte Flussdiagramm der Funktion `name` aus kQuelle.
Flussdiagramm fluss(std::string_view name, std::string_view quelle = kQuelle)
{
    const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(quelle, "test.c");
    for (const c2d::modell::Funktion& f : e.einheit.funktionen)
        if (f.name == name) return fasseAnweisungsfolgenZusammen(flussVon(f));
    FAIL("no function " << name);
    return Flussdiagramm();
}

bool hatKante(const Flussdiagramm& d, KnotenId von, KnotenId nach, std::string_view beschriftung = {})
{
    for (const Kante& k : d.kanten())
        if (k.von == von && k.nach == nach && k.beschriftung == beschriftung) return true;
    return false;
}

/// main mit summe hinter Knoten 4 und darin quadrat hinter Knoten 9.
Flussdiagramm zweifachEingebettet()
{
    const Flussdiagramm einmal = betteEin(fluss("main"), 4, fluss("summe"), "summe(int n)");
    return betteEin(einmal, 9, fluss("quadrat"), "quadrat(int x)");
}

bool ueberlappen(const Rechteck& a, const Rechteck& b)
{
    return a.x < b.x + b.breite && b.x < a.x + a.breite && a.y < b.y + b.hoehe && b.y < a.y + a.hoehe;
}

bool liegtIn(const Rechteck& innen, const Rechteck& aussen)
{
    return innen.x >= aussen.x && innen.y >= aussen.y && innen.x + innen.breite <= aussen.x + aussen.breite &&
           innen.y + innen.hoehe <= aussen.y + aussen.hoehe;
}

std::size_t zaehle(const std::string& text, std::string_view teil)
{
    std::size_t n = 0;
    for (std::size_t i = text.find(teil); i != std::string::npos; i = text.find(teil, i + 1)) ++n;
    return n;
}

}  // namespace

TEST_SUITE("Einbettung")
{
    TEST_CASE("Jeder Knoten nennt, welche Funktionen sein Text ruft")
    {
        const Flussdiagramm d = fluss("main");
        REQUIRE(d.knoten().size() == 7);
        CHECK(d.knoten()[0].aufrufe.empty());  // die Signatur im Start ist kein Aufruf
        CHECK(d.knoten()[1].aufrufe == std::vector<std::string>{"lies"});
        CHECK(d.knoten()[2].aufrufe.empty());
        CHECK(d.knoten()[4].art == KnotenArt::Aufruf);
        CHECK(d.knoten()[4].aufrufe == std::vector<std::string>{"drucke", "summe"});

        // ein Objekt vor dem Namen zaehlt nicht, ein Bereich bleibt; Woerter der Sprache und Literale zaehlen nicht
        const Flussdiagramm e = fluss(
            "f", "void f(void) { a = sizeof(b) + g(h(1), \"k(\") + o.m(2) + p->n(3) + N::q(4) + g(5) + ::r(6) + 0x1Fu; }");
        CHECK(e.knoten()[1].aufrufe == std::vector<std::string>{"g", "h", "m", "n", "N::q", "r"});

        // beim Buendeln sammeln sich die Aufrufe
        const Flussdiagramm s = fluss("summe");
        CHECK(s.knoten()[3].zeilen == std::vector<std::string>{"s += quadrat(i)", "i++"});
        CHECK(s.knoten()[3].aufrufe == std::vector<std::string>{"quadrat"});
    }

    TEST_CASE("Einbetten: der Ablauf der gerufenen Funktion haengt hinter dem Aufruf, als Gruppe")
    {
        const Flussdiagramm haupt = fluss("main");
        const Flussdiagramm d = betteEin(haupt, 4, fluss("summe"), "summe(int n)");

        // die Kennungen von main bleiben, vier Knoten kommen dazu (ohne Start und Ende von summe)
        REQUIRE(d.knoten().size() == 11);
        CHECK(d.knoten()[4].zeilen == haupt.knoten()[4].zeilen);
        CHECK(d.knoten()[4].gruppe == kKeine);  // der Aufrufer bleibt draussen
        for (KnotenId i = 7; i < 11; ++i) CHECK(d.knoten()[i].gruppe == 0);
        REQUIRE(d.gruppen().size() == 1);
        CHECK(d.gruppen()[0].titel == "summe(int n)");
        CHECK(d.gruppen()[0].aufrufer == 4);

        CHECK(d.kanten().size() == 12);
        CHECK_FALSE(hatKante(d, 4, 5));  // der Aufrufer fuehrt nicht mehr direkt weiter
        CHECK(hatKante(d, 4, 7));        // sondern in den ersten Schritt von summe
        CHECK(hatKante(d, 8, 9, "yes"));
        CHECK(hatKante(d, 9, 8));
        CHECK(hatKante(d, 8, 10, "no"));
        CHECK(hatKante(d, 10, 5));  // das Ende von summe: dorthin, wohin der Aufrufer fuehrte
    }

    TEST_CASE("Einbetten: nur einmal je Knoten, nicht ohne Inhalt, nicht an einem Knoten, den es nicht gibt")
    {
        const Flussdiagramm haupt = fluss("main");
        const Flussdiagramm einmal = betteEin(haupt, 4, fluss("summe"), "summe(int n)");

        CHECK(betteEin(einmal, 4, fluss("quadrat"), "x").knoten().size() == einmal.knoten().size());
        CHECK(betteEin(haupt, 99, fluss("summe"), "x").knoten().size() == haupt.knoten().size());
        CHECK(betteEin(haupt, 4, fluss("leer", "void leer(void) { }"), "x").gruppen().empty());
    }

    TEST_CASE("Einbetten in eine Einbettung, und beides wieder herausloesen")
    {
        const Flussdiagramm haupt = fluss("main");
        const Flussdiagramm d = zweifachEingebettet();

        REQUIRE(d.knoten().size() == 12);
        REQUIRE(d.gruppen().size() == 2);
        CHECK(d.gruppen()[1].eltern == 0);
        CHECK(d.gruppen()[1].aufrufer == 9);
        CHECK(d.knoten()[11].gruppe == 1);
        CHECK(d.liegtIn(1, 0));
        CHECK_FALSE(d.liegtIn(0, 1));
        CHECK(hatKante(d, 9, 11));
        CHECK(hatKante(d, 11, 8));
        CHECK_FALSE(hatKante(d, 9, 8));

        SUBCASE("die innere allein")
        {
            const Flussdiagramm ohne = loeseEin(d, 1);
            CHECK(ohne.knoten().size() == 11);
            CHECK(ohne.gruppen().size() == 1);
            CHECK(hatKante(ohne, 9, 8));  // der Aufrufer hat seine Kante wieder
            CHECK(ohne.kanten().size() == 12);
        }
        SUBCASE("die aeussere nimmt die innere mit")
        {
            const Flussdiagramm ohne = loeseEin(d, 0);
            CHECK(ohne.knoten().size() == haupt.knoten().size());
            CHECK(ohne.gruppen().empty());
            CHECK(ohne.kanten().size() == haupt.kanten().size());
            for (const Kante& k : haupt.kanten()) CHECK(hatKante(ohne, k.von, k.nach, k.beschriftung));
        }
        CHECK(loeseEin(d, 7).knoten().size() == 12);  // eine Gruppe, die es nicht gibt
    }

    TEST_CASE("Mermaid und DOT: eine Gruppe wird ein Rahmen, verschachtelt")
    {
        const Flussdiagramm d = zweifachEingebettet();

        const std::string m = c2d::ausgabe::mermaid().schreibe(d, {});
        CHECK(m.find("    n4[[\"drucke(summe(n))\"]]\n"
                     "    n5[\"return 0\"]\n"
                     "    n6([\"End\"])\n"
                     "    subgraph g0[\"summe(int n)\"]\n"
                     "        n7[\"int s = 0<br>int i = 0\"]\n") != std::string::npos);
        CHECK(m.find("        subgraph g1[\"quadrat(int x)\"]\n"
                     "            n11[\"return x * x\"]\n"
                     "        end\n"
                     "    end\n"
                     "    n0 --> n1\n") != std::string::npos);

        const std::string dot = c2d::ausgabe::dot().schreibe(d, {});
        CHECK(dot.find("    subgraph cluster_0 {\n        style=dashed; label=\"summe(int n)\";\n") != std::string::npos);
        CHECK(dot.find("        subgraph cluster_1 {\n") != std::string::npos);
    }
}

TEST_SUITE("Anordnung")
{
    TEST_CASE("Flussdiagramm: nichts ueberlappt, alles im Bild, Rahmen um ihre Knoten")
    {
        const Flussdiagramm d = zweifachEingebettet();
        for (const bool nachRechts : {false, true})
        {
            CAPTURE(nachRechts);
            AnordnungOptionen optionen;
            optionen.nachRechts = nachRechts;
            const FlussAnordnung a = ordneAn(d, optionen);

            REQUIRE(a.knoten.size() == d.knoten().size());
            REQUIRE(a.kanten.size() == d.kanten().size());
            REQUIRE(a.gruppen.size() == 2);
            const Rechteck bild{0, 0, a.breite, a.hoehe};
            for (std::size_t i = 0; i < a.knoten.size(); ++i)
            {
                CHECK(liegtIn(a.knoten[i], bild));
                for (std::size_t k = i + 1; k < a.knoten.size(); ++k) CHECK_FALSE(ueberlappen(a.knoten[i], a.knoten[k]));
            }
            for (KnotenId i = 7; i < 11; ++i) CHECK(liegtIn(a.knoten[i], a.gruppen[0]));
            CHECK(liegtIn(a.knoten[11], a.gruppen[1]));
            CHECK(liegtIn(a.gruppen[1], a.gruppen[0]));
            CHECK(liegtIn(a.gruppen[0], bild));
            CHECK_FALSE(liegtIn(a.knoten[4], a.gruppen[0]));  // der Aufrufer steht vor dem Rahmen

            // jede Kante beginnt am Rand ihrer Quelle und endet am Rand ihres Ziels
            for (std::size_t e = 0; e < a.kanten.size(); ++e)
            {
                REQUIRE(a.kanten[e].size() >= 2);
                const auto amRand = [](const Rechteck& r, const Punkt& p) {
                    return liegtIn({p.x, p.y, 0, 0}, {r.x - 1, r.y - 1, r.breite + 2, r.hoehe + 2});
                };
                CHECK(amRand(a.knoten[d.kanten()[e].von], a.kanten[e].front()));
                CHECK(amRand(a.knoten[d.kanten()[e].nach], a.kanten[e].back()));
            }
        }
    }

    TEST_CASE("Flussdiagramm: nach unten folgt das Ziel unter der Quelle, nach rechts rechts von ihr")
    {
        const Flussdiagramm d = fluss("main");
        const FlussAnordnung unten = ordneAn(d);
        CHECK(unten.knoten[1].y > unten.knoten[0].y + unten.knoten[0].hoehe);
        CHECK(unten.knoten[3].y == doctest::Approx(unten.knoten[4].y).epsilon(0.2));  // beide Zweige in einer Schicht

        AnordnungOptionen optionen;
        optionen.nachRechts = true;
        const FlussAnordnung rechts = ordneAn(d, optionen);
        CHECK(rechts.knoten[1].x > rechts.knoten[0].x + rechts.knoten[0].breite);
    }

    TEST_CASE("Flussdiagramm: ein switch — jede Kante ihr eigener Weg, jede Beschriftung an ihrer Kante")
    {
        // nach SELECT_CONTROL_INTERFACE einer Firmware: ein Fall mit verschachteltem if,
        // Faelle mit einer Zeile, zwei leere Faelle, default
        constexpr std::string_view kAuswahl = R"c(
void waehle(int art)
{
    static long zeit;
    switch (art)
    {
        case HW: a = 0; if (a != b) { if (zeit - t >= 5) { a = b; } } else { zeit = t; } break;
        case SCI_A: a = 0; break;
        case SCI_B: a = 0; break;
        case UART_B: a = 0; break;
        case SPANNUNG: break;
        case STROM: break;
        default: fehler = 1; a = 0; break;
    }
}
)c";
        const Flussdiagramm d = fluss("waehle", kAuswahl);
        for (const bool nachRechts : {false, true})
        {
            CAPTURE(nachRechts);
            AnordnungOptionen optionen;
            optionen.nachRechts = nachRechts;
            const FlussAnordnung a = ordneAn(d, optionen);
            REQUIRE(a.beschriftungen.size() == d.kanten().size());

            // Liegt p auf dem Linienzug (bis auf eine Toleranz)?
            const auto aufLinie = [](const Linienzug& l, const Punkt& p) {
                for (std::size_t i = 1; i < l.size(); ++i)
                {
                    const double x1 = std::min(l[i - 1].x, l[i].x) - 1, x2 = std::max(l[i - 1].x, l[i].x) + 1;
                    const double y1 = std::min(l[i - 1].y, l[i].y) - 1, y2 = std::max(l[i - 1].y, l[i].y) + 1;
                    if (p.x >= x1 && p.x <= x2 && p.y >= y1 && p.y <= y2) return true;
                }
                return false;
            };
            for (std::size_t e = 0; e < a.kanten.size(); ++e)
            {
                CAPTURE(e);
                const Linienzug& l = a.kanten[e];
                // rechtwinklig
                for (std::size_t i = 1; i < l.size(); ++i)
                    CHECK((std::abs(l[i].x - l[i - 1].x) < 0.5 || std::abs(l[i].y - l[i - 1].y) < 0.5));
                // durch keinen fremden Knoten
                for (std::size_t v = 0; v < a.knoten.size(); ++v)
                {
                    if (v == d.kanten()[e].von || v == d.kanten()[e].nach) continue;
                    const Rechteck& r = a.knoten[v];
                    for (std::size_t i = 1; i < l.size(); ++i)
                    {
                        const Rechteck stueck{std::min(l[i - 1].x, l[i].x), std::min(l[i - 1].y, l[i].y),
                                              std::abs(l[i].x - l[i - 1].x), std::abs(l[i].y - l[i - 1].y)};
                        CHECK_FALSE(ueberlappen(stueck, r));
                    }
                }
                if (d.kanten()[e].beschriftung.empty()) continue;
                // die Beschriftung auf ihrer eigenen Kante, auf keiner anderen
                CHECK(aufLinie(l, a.beschriftungen[e]));
                for (std::size_t f = 0; f < a.kanten.size(); ++f)
                    if (f != e) CHECK_FALSE(aufLinie(a.kanten[f], a.beschriftungen[e]));
            }
            // Zwei Kanten teilen kein Stueck — ausser sie haben dieselbe Quelle (sie trennen
            // sich erst) oder dasselbe Ziel (sie laufen zusammen).
            const auto teilen = [](const Linienzug& l, const Linienzug& m) {
                for (std::size_t i = 1; i < l.size(); ++i)
                    for (std::size_t k = 1; k < m.size(); ++k)
                    {
                        const Punkt a1 = l[i - 1], a2 = l[i], b1 = m[k - 1], b2 = m[k];
                        const bool waagrecht = std::abs(a1.y - a2.y) < 0.5 && std::abs(b1.y - b2.y) < 0.5 &&
                                               std::abs(a1.y - b1.y) < 0.5;
                        const bool senkrecht = std::abs(a1.x - a2.x) < 0.5 && std::abs(b1.x - b2.x) < 0.5 &&
                                               std::abs(a1.x - b1.x) < 0.5;
                        const auto deckt = [](double p1, double p2, double q1, double q2) {
                            return std::min(std::max(p1, p2), std::max(q1, q2)) - std::max(std::min(p1, p2), std::min(q1, q2)) > 1;
                        };
                        if (waagrecht && deckt(a1.x, a2.x, b1.x, b2.x)) return true;
                        if (senkrecht && deckt(a1.y, a2.y, b1.y, b2.y)) return true;
                    }
                return false;
            };
            for (std::size_t e = 0; e < a.kanten.size(); ++e)
                for (std::size_t f = e + 1; f < a.kanten.size(); ++f)
                {
                    if (d.kanten()[e].von == d.kanten()[f].von || d.kanten()[e].nach == d.kanten()[f].nach) continue;
                    CAPTURE(e);
                    CAPTURE(f);
                    CHECK_FALSE(teilen(a.kanten[e], a.kanten[f]));
                }
            for (const Punkt& p : a.beschriftungen) CHECK(liegtIn({p.x - 30, p.y - 7, 60, 14}, {0, 0, a.breite, a.hoehe}));

            // kein Schild liegt auf einem Knoten (geschaetzt wie in der Vorschau) — auch nach rechts,
            // wo es laengs der Kante steht (Sichttest B3: `ROT_GELB, GELB` lag auf `return 1000`)
            const auto schild = [&](std::size_t e) {
                const double breite = static_cast<double>(d.kanten()[e].beschriftung.size()) * 7.2 + 4;
                return Rechteck{a.beschriftungen[e].x - breite / 2, a.beschriftungen[e].y - 7, breite, 14};
            };
            for (std::size_t e = 0; e < a.kanten.size(); ++e)
            {
                if (d.kanten()[e].beschriftung.empty()) continue;
                CAPTURE(d.kanten()[e].beschriftung);
                for (const Rechteck& r : a.knoten) CHECK_FALSE(ueberlappen(schild(e), r));
            }

            // die Schilder der Faelle stossen nicht aneinander
            if (nachRechts) continue;
            for (std::size_t e = 0; e < a.kanten.size(); ++e)
                for (std::size_t f = e + 1; f < a.kanten.size(); ++f)
                    if (!d.kanten()[e].beschriftung.empty() && !d.kanten()[f].beschriftung.empty())
                    {
                        CAPTURE(d.kanten()[e].beschriftung);
                        CAPTURE(d.kanten()[f].beschriftung);
                        CHECK_FALSE(ueberlappen(schild(e), schild(f)));
                    }
        }
    }

    TEST_CASE("Flussdiagramm: eine Kante zurueck laeuft durch keinen Knoten, der rechts neben ihrer Quelle steht")
    {
        // wie beispiele/projekt/main.c: das Ende steht in der Schicht des letzten Schritts der Schleife
        const Flussdiagramm d = fluss("haupt", "int haupt(void) { if (!pruefe()) return 1; motor_starte(2);"
                                               " while (1) { regle(100); protokolliere(1); } return 0; }");
        for (const bool nachRechts : {false, true})
        {
            CAPTURE(nachRechts);
            AnordnungOptionen optionen;
            optionen.nachRechts = nachRechts;
            const FlussAnordnung a = ordneAn(d, optionen);
            for (std::size_t e = 0; e < a.kanten.size(); ++e)
            {
                CAPTURE(e);
                const Linienzug& l = a.kanten[e];
                for (std::size_t v = 0; v < a.knoten.size(); ++v)
                {
                    if (v == d.kanten()[e].von || v == d.kanten()[e].nach) continue;
                    for (std::size_t i = 1; i < l.size(); ++i)
                    {
                        const Rechteck stueck{std::min(l[i - 1].x, l[i].x), std::min(l[i - 1].y, l[i].y),
                                              std::abs(l[i].x - l[i - 1].x), std::abs(l[i].y - l[i - 1].y)};
                        CHECK_FALSE(ueberlappen(stueck, a.knoten[v]));
                    }
                }
                // und sie endet am Rand ihres Ziels
                const Rechteck& ziel = a.knoten[d.kanten()[e].nach];
                CHECK(liegtIn({l.back().x, l.back().y, 0, 0}, {ziel.x - 1, ziel.y - 1, ziel.breite + 2, ziel.hoehe + 2}));
            }
        }
    }

    TEST_CASE("Flussdiagramm: eine Schleife mit break tief in if — die Kette rueckt nicht Runde um Runde nach rechts")
    {
        // gefunden an EtherCAT und FirmWare, sbl_main.c, main: 13 900 px breit, weil ein oberer Knoten
        // mitrueckte, seinen Nachbarn schob, der den unteren wieder draengte — in jeder Runde weiter
        constexpr std::string_view quelle = R"c(
int main(void)
{
   if (w != 0U)
   {
      res = 1;
      for (r = 0U; r < 2U; r++)
      {
         if (t == 1)
         {
            res = apply1();
            if (res == 0) { v1 = validate1(); if (v1 != 0U) { break; } }
         }
         else
         {
            res = apply2();
            if (res == 0) { if (claim() == 0) { v2 = validate2(); } if (v2 != 0U) { break; } }
         }
      }
      result = res;
   }
   if (v1 != 0U) { jump(); }
   return 0;
}
)c";
        const Flussdiagramm d = fluss("main", quelle);
        const FlussAnordnung a = ordneAn(d);
        double summe = 0;
        for (const Rechteck& r : a.knoten) summe += r.breite;
        CHECK(a.breite < summe);  // vorher 4850 bei 20 Knoten
        for (std::size_t i = 0; i < a.knoten.size(); ++i)
            for (std::size_t k = i + 1; k < a.knoten.size(); ++k) CHECK_FALSE(ueberlappen(a.knoten[i], a.knoten[k]));
    }

    TEST_CASE("Flussdiagramm: viele if/else hintereinander wandern nicht nach rechts (Brandes-Koepf)")
    {
        // von links gelegt wich jeder Konflikt nach rechts aus: je Block ein Stueck weiter
        std::string quelle = "void f(void)\n{\n    a();\n";
        for (int i = 0; i < 8; ++i)
            quelle += "    if (x" + std::to_string(i) + ") b" + std::to_string(i) + "(); else c" + std::to_string(i) +
                      "(); d" + std::to_string(i) + "();\n";
        quelle += "    e();\n}\n";
        const Flussdiagramm d = fluss("f", quelle);
        const FlussAnordnung a = ordneAn(d);
        // der Anfang und das Ende stehen (fast) uebereinander, die Breite bleibt die eines Blocks
        const Rechteck& anfang = a.knoten.front();
        std::size_t ende = 0;
        for (std::size_t i = 0; i < d.knoten().size(); ++i)
            if (d.knoten()[i].art == KnotenArt::Ende) ende = i;
        CHECK(std::abs(anfang.mitte().x - a.knoten[ende].mitte().x) < 40);
        double breitester = 0;
        for (const Rechteck& r : a.knoten) breitester = std::max(breitester, r.breite);
        CHECK(a.breite < 4 * breitester);
        for (std::size_t i = 0; i < a.knoten.size(); ++i)
            for (std::size_t k = i + 1; k < a.knoten.size(); ++k) CHECK_FALSE(ueberlappen(a.knoten[i], a.knoten[k]));
    }

    TEST_CASE("Strukturdiagramm: die Basis steht ueber dem, was von ihr erbt")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::cpp().lies(
            "struct Basis { int a; }; struct Kind : Basis { Basis* b; Kind* naechstes; }; enum E { X };", "t.cpp");
        const Strukturdiagramm d = strukturVon(e.einheit);
        const StrukturAnordnung a = ordneAn(d);

        REQUIRE(a.kaesten.size() == 3);
        REQUIRE(a.beziehungen.size() == d.beziehungen.size());
        CHECK(a.kaesten[0].y + a.kaesten[0].hoehe < a.kaesten[1].y);
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t k = i + 1; k < 3; ++k) CHECK_FALSE(ueberlappen(a.kaesten[i], a.kaesten[k]));
        CHECK(a.breite > 0);
        CHECK(a.beziehungen.back().size() == 4);  // Kind -> Kind: eine Schlaufe
    }

    TEST_CASE("Strukturdiagramm: unten raus, oben rein, rechtwinklig; mehrere nebeneinander; Vererbung zur Basis hin")
    {
        // gefunden an TMS320F28, Serial_Commands.h: schraege Linien trafen hohe Kaesten seitlich
        const c2d::sprache::Ergebnis e = c2d::sprache::cpp().lies(
            "enum A { X }; enum B { Y }; struct Basis { int a; };\n"
            "struct Def : Basis { A eins; A zwei; B drei; };\n",
            "t.cpp");
        const Strukturdiagramm d = strukturVon(e.einheit);
        const StrukturAnordnung a = ordneAn(d);
        REQUIRE(a.beziehungen.size() == d.beziehungen.size());
        std::map<std::size_t, std::vector<double>> hinein;  // je Kasten: wo Pfeile oben eintreffen
        for (std::size_t b = 0; b < d.beziehungen.size(); ++b)
        {
            const Linienzug& l = a.beziehungen[b];
            CAPTURE(d.beziehungen[b].beschriftung);
            REQUIRE(l.size() >= 2);
            for (std::size_t i = 0; i + 1 < l.size(); ++i)  // rechtwinklig
                CHECK((std::abs(l[i].x - l[i + 1].x) < 0.01 || std::abs(l[i].y - l[i + 1].y) < 0.01));
            const Rechteck& von = a.kaesten[d.beziehungen[b].von];
            const Rechteck& nach = a.kaesten[d.beziehungen[b].nach];
            if (d.beziehungen[b].art == BeziehungArt::Erbt)
            {
                // vom Abgeleiteten (oben aus ihm) zur Basis darueber (unten in sie)
                CHECK(l.front().y == doctest::Approx(von.y));
                CHECK(l.back().y == doctest::Approx(nach.y + nach.hoehe));
                continue;
            }
            CHECK(l.front().y == doctest::Approx(von.y + von.hoehe));  // unten raus
            CHECK(l.back().y == doctest::Approx(nach.y));              // oben rein
            CHECK(l.back().x > nach.x);
            CHECK(l.back().x < nach.x + nach.breite);
            hinein[d.beziehungen[b].nach].push_back(l.back().x);
        }
        // zwei Glieder vom Typ A: zwei Pfeile, nebeneinander, nicht in einem Punkt
        for (const auto& [kasten, xs] : hinein)
            for (std::size_t i = 0; i < xs.size(); ++i)
                for (std::size_t k = i + 1; k < xs.size(); ++k) CHECK(std::abs(xs[i] - xs[k]) > 5);
        // die waagrechten Stuecke der Kanten aus Def liegen nicht uebereinander
        struct Stueck { double y, von, bis; };
        std::vector<Stueck> waagrecht;
        for (std::size_t b = 0; b < d.beziehungen.size(); ++b)
        {
            if (d.beziehungen[b].art == BeziehungArt::Erbt) continue;
            const Linienzug& l = a.beziehungen[b];
            for (std::size_t i = 0; i + 1 < l.size(); ++i)
                if (std::abs(l[i].y - l[i + 1].y) < 0.01 && std::abs(l[i].x - l[i + 1].x) > 0.01)
                    waagrecht.push_back({l[i].y, std::min(l[i].x, l[i + 1].x), std::max(l[i].x, l[i + 1].x)});
        }
        for (std::size_t i = 0; i < waagrecht.size(); ++i)
            for (std::size_t k = i + 1; k < waagrecht.size(); ++k)
            {
                const bool gleicheHoehe = std::abs(waagrecht[i].y - waagrecht[k].y) < 0.5;
                const bool decken = waagrecht[i].von < waagrecht[k].bis && waagrecht[k].von < waagrecht[i].bis;
                CHECK_FALSE((gleicheHoehe && decken));
            }
    }

    TEST_CASE("Strukturdiagramm: Beschriftungen zweier Beziehungen zwischen denselben Kaesten liegen getrennt")
    {
        // gefunden an TMS320F28, t_GPA: `dat` und `toggle`, beide vom Typ t_A_REG, lagen uebereinander
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(
            "typedef union { unsigned long l; } t_A_REG;\n"
            "typedef struct { t_A_REG dat; t_A_REG toggle; t_A_REG *set; t_A_REG naechste; } t_GPA;\n",
            "t.c");
        const Strukturdiagramm d = strukturVon(e.einheit);
        for (const bool nachRechts : {false, true})
        {
            CAPTURE(nachRechts);
            AnordnungOptionen o;
            o.nachRechts = nachRechts;
            const StrukturAnordnung a = ordneAn(d, o);
            REQUIRE(a.beschriftungen.size() == d.beziehungen.size());
            // jedes Schild, geschaetzt wie die Anordnung: Zeichen mal Breite, eine Zeile hoch
            std::vector<Rechteck> schilder;
            for (std::size_t b = 0; b < d.beziehungen.size(); ++b)
            {
                const double breite = static_cast<double>(d.beziehungen[b].beschriftung.size()) * o.zeichenBreite + 6;
                const Punkt m = a.beschriftungen[b];
                schilder.push_back({m.x - breite / 2, m.y - o.zeilenHoehe / 2, breite, o.zeilenHoehe});
            }
            for (std::size_t i = 0; i < schilder.size(); ++i)
            {
                for (std::size_t k = i + 1; k < schilder.size(); ++k) CHECK_FALSE(ueberlappen(schilder[i], schilder[k]));
                for (const Rechteck& kasten : a.kaesten) CHECK_FALSE(ueberlappen(schilder[i], kasten));
                CHECK(schilder[i].x >= 0);
                CHECK(schilder[i].x + schilder[i].breite <= a.breite);
                CHECK(schilder[i].y + schilder[i].hoehe <= a.hoehe);
            }
        }
    }
}

TEST_SUITE("draw.io")
{
    TEST_CASE("Flussdiagramm: je Knoten und Gruppe eine Zelle mit Ort, je Kante eine mit Quelle und Ziel")
    {
        const Flussdiagramm d = zweifachEingebettet();
        const std::string x = c2d::ausgabe::drawio().schreibe(d, {});

        CHECK(x.starts_with("<mxfile host=\"Code2Diagram\">\n  <diagram name=\"main\" id=\"c2d\">\n"));
        CHECK(x.ends_with("</mxfile>\n"));
        CHECK(zaehle(x, "vertex=\"1\"") == d.knoten().size() + d.gruppen().size());
        CHECK(zaehle(x, "edge=\"1\"") == d.kanten().size());
        CHECK(zaehle(x, "<mxGeometry ") == d.knoten().size() + d.gruppen().size() + d.kanten().size());

        // die Gruppe ist ein Behaelter: ihr Inhalt haengt an ihr, die innere Gruppe an der aeusseren
        CHECK(x.find("<mxCell id=\"g0\" value=\"summe(int n)\" style=\"swimlane;") != std::string::npos);
        CHECK(x.find("<mxCell id=\"g1\" value=\"quadrat(int x)\"") != std::string::npos);
        CHECK(zaehle(x, "parent=\"g0\"") == 5);  // vier Knoten und die innere Gruppe
        CHECK(zaehle(x, "parent=\"g1\"") == 1);
        CHECK(x.find("<mxCell id=\"n2\" value=\"n &amp;lt; 0\" style=\"rhombus;") != std::string::npos);
        CHECK(x.find("value=\"int s = 0&lt;br&gt;int i = 0\"") != std::string::npos);
        CHECK(x.find("<mxCell id=\"e3\" value=\"no\" style=\"edgeStyle=orthogonalEdgeStyle;") != std::string::npos);
        CHECK(x.find("source=\"n2\" target=\"n4\"") != std::string::npos);
    }

    TEST_CASE("Datentypen: je Typ eine Zelle wie die UML-Klasse von draw.io")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::cpp().lies(
            "struct Basis { int a; }; class Kind : public Basis { public: void f(); private: Basis* b; };", "t.cpp");
        const std::string x = c2d::ausgabe::drawio().schreibe(strukturVon(e.einheit), {.richtung = "LR"});

        CHECK(zaehle(x, "vertex=\"1\"") == 2);
        CHECK(zaehle(x, "edge=\"1\"") == 2);
        CHECK(x.find("&lt;b&gt;Kind&lt;/b&gt;") != std::string::npos);
        CHECK(x.find("- Basis* b") != std::string::npos);
        CHECK(x.find("+ void f()") != std::string::npos);
        CHECK(x.find("style=\"endArrow=block;endFill=0;endSize=12;html=1;") != std::string::npos);  // erbt
        CHECK(x.find("source=\"k1\" target=\"k0\"") != std::string::npos);
    }
}
