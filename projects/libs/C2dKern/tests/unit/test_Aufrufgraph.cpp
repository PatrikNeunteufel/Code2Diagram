/**
 * @file test_Aufrufgraph.cpp
 * @brief Der Aufrufgraph: Aufrufstellen, Ableitung im Projekt, Anordnung in Baendern, die Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/projekt/Aufrufgraph.hpp"
#include "c2d/sprache/Sprache.hpp"

#include "TempOrdner.hpp"

#include <algorithm>

using namespace c2d::diagramm;
using namespace c2d::projekt;

namespace
{

using c2d::diagramm::kKeine;  // gleich projekt::kKeine; im Namensraum, sonst bleibt es mehrdeutig

/// Ein kleines Projekt: main ruft eine eigene static-Funktion (rekursiv), eine Funktion
/// einer anderen Datei, eine mehrdeutige (zwei Dateien), eine fremde und ein Makro.
void legeProjekt(const TempOrdner& t)
{
    t.lege("main.c", "#define PRUEFE(x) ((x) > 0)\n"
                     "static int hilfe(int n) { if (n > 0) return hilfe(n - 1); return 0; }\n"
                     "int main(void)\n"
                     "{\n"
                     "    hilfe(3);\n"
                     "    motor_starte();\n"
                     "    regle();\n"
                     "    memcpy(0, 0, 0);\n"
                     "    if (PRUEFE(1)) log_schreibe();\n"
                     "    return 0;\n"
                     "}\n");
    t.lege("antrieb/motor.c", "static void hilfe(void) { }\n"
                              "void takt(void) { }\n"
                              "void motor_starte(void) { hilfe(); takt(); }\n"
                              "void regle(void) { }\n");
    t.lege("sim/sim.c", "void regle(void) { }\n");
    t.lege("log/log.c", "void log_schreibe(void) { motor_starte(); }\n");
}

AufrufgraphOptionen optionen(int tiefe, AufrufRichtung richtung = AufrufRichtung::Gerufene, bool ohneFremde = false,
                             std::vector<Fundstelle> aufgeklappt = {})
{
    AufrufgraphOptionen o;
    o.richtung = richtung;
    o.tiefe = tiefe;
    o.ohneFremde = ohneFremde;
    o.aufgeklappt = std::move(aufgeklappt);
    return o;
}

Fundstelle finde(const Projekt& p, std::string_view pfad, std::string_view name)
{
    const std::size_t d = p.finde(pfad);
    REQUIRE(d != kKeine);
    const auto& fs = p.dateien[d].ergebnis.einheit.funktionen;
    for (std::size_t f = 0; f < fs.size(); ++f)
        if (fs[f].name == name && fs[f].rumpf) return {d, f};
    FAIL("no function " << name << " in " << pfad);
    return {};
}

/// Der Knoten dieses Namens (in dieser Datei, wenn genannt); kKeine = keiner.
std::size_t knoten(const Aufrufgraph& g, std::string_view name, std::string_view datei = {})
{
    for (std::size_t i = 0; i < g.knoten.size(); ++i)
    {
        const AufrufKnoten& k = g.knoten[i];
        if (k.name != name) continue;
        if (datei.empty() || (k.datei < g.dateien.size() && g.dateien[k.datei] == datei)) return i;
    }
    return kKeine;
}

const AufrufKante* kante(const Aufrufgraph& g, std::size_t von, std::size_t nach)
{
    for (const AufrufKante& k : g.kanten)
        if (k.von == von && k.nach == nach) return &k;
    return nullptr;
}

bool ueberlappt(const Rechteck& p, const Rechteck& q)
{
    return p.x < q.x + q.breite && q.x < p.x + p.breite && p.y < q.y + q.hoehe && q.y < p.y + p.hoehe;
}

bool liegtIn(const Rechteck& innen, const Rechteck& aussen)
{
    return innen.x >= aussen.x && innen.y >= aussen.y && innen.x + innen.breite <= aussen.x + aussen.breite &&
           innen.y + innen.hoehe <= aussen.y + aussen.hoehe;
}

constexpr std::string_view kBedingt = "void a(void) { }\n"
                                      "void b(void) { }\n"
                                      "void c(void) { }\n"
                                      "void f(void)\n"
                                      "{\n"
                                      "    a();\n"
                                      "#ifdef MIT_B\n"
                                      "    b();\n"
                                      "#else\n"
                                      "    c();\n"
                                      "#endif\n"
                                      "}\n";

/// Das Projekt aus kBedingt, gelesen mit diesen Optionen.
Projekt bedingt(bool varianten, bool bereiche)
{
    ProjektOptionen o;
    o.varianten = varianten;
    o.bereiche = bereiche;
    return liesText(kBedingt, "t.c", o);
}

}  // namespace

TEST_SUITE("Aufrufgraph")
{
    using c2d::diagramm::kKeine;
    TEST_CASE("Aufrufstellen: ohne Bedingung, im Zweig, der nicht gilt, im bedingten Bereich")
    {
        const auto stellen = [](const Projekt& p) {
            return c2d::modell::aufrufstellenVon(funktion(p, finde(p, "t.c", "f")));
        };
        // ausgewertet: nur a und c, ohne Bedingung
        {
            const auto s = stellen(bedingt(false, false));
            REQUIRE(s.size() == 2);
            CHECK(s[0].name == "a");
            CHECK(s[1].name == "c");
            CHECK(s[1].bedingung.empty());
            CHECK(s[1].gilt);
        }
        // Varianten: b steht im Zweig, der nicht gilt, unter seiner Bedingung; c gilt, ohne Bedingung
        {
            const auto s = stellen(bedingt(true, false));
            REQUIRE(s.size() == 3);
            CHECK(s[1].name == "b");
            CHECK(s[1].bedingung == "defined(MIT_B)");
            CHECK_FALSE(s[1].gilt);
            CHECK(s[1].zeile == 8);
            CHECK(s[2].name == "c");
            CHECK(s[2].bedingung.empty());
            CHECK(s[2].gilt);
        }
        // Rahmen: c steht im bedingten Bereich, unter dessen voller Bedingung, und gilt
        {
            const auto s = stellen(bedingt(false, true));
            REQUIRE(s.size() == 2);
            CHECK(s[1].name == "c");
            CHECK(s[1].bedingung == "!defined(MIT_B)");
            CHECK(s[1].gilt);
        }
    }

    TEST_CASE("Ab einer Funktion, nach unten: Knoten, Kanten, mehrdeutig, fremd, Makro, Rekursion")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const AufrufgraphErgebnis e = aufrufgraphAb(p, finde(p, "main.c", "main"), optionen(0));
        const Aufrufgraph& g = e.graph;
        CHECK(g.titel == "calls from main()");
        REQUIRE(g.knoten.size() == e.stellen.size());

        const std::size_t main = knoten(g, "main");
        const std::size_t hilfe = knoten(g, "hilfe", "main.c");
        const std::size_t hilfeMotor = knoten(g, "hilfe", "antrieb/motor.c");
        const std::size_t motor = knoten(g, "motor_starte");
        const std::size_t regleA = knoten(g, "regle", "antrieb/motor.c");
        const std::size_t regleS = knoten(g, "regle", "sim/sim.c");
        const std::size_t fremdK = knoten(g, "memcpy");
        const std::size_t pruefe = knoten(g, "PRUEFE");
        const std::size_t logK = knoten(g, "log_schreibe");
        for (const std::size_t k : {main, hilfe, hilfeMotor, motor, regleA, regleS, fremdK, pruefe, logK}) REQUIRE(k != kKeine);
        CHECK(main == 0);
        CHECK(g.knoten[main].wurzel);
        CHECK(g.dateien.front() == "main.c");
        CHECK(g.mitRahmen());

        // die eigene static-Funktion, nicht die gleichnamige der anderen Datei; rekursiv: eine Schlaufe
        CHECK(kante(g, main, hilfe) != nullptr);
        CHECK(kante(g, main, hilfeMotor) == nullptr);
        CHECK(kante(g, hilfe, hilfe) != nullptr);
        CHECK(kante(g, motor, hilfeMotor) != nullptr);
        // je Kandidat eine mehrdeutige Kante, und eine Meldung mit beiden
        REQUIRE(kante(g, main, regleA) != nullptr);
        REQUIRE(kante(g, main, regleS) != nullptr);
        CHECK(kante(g, main, regleA)->art == AufrufKantenArt::Mehrdeutig);
        CHECK(beschriftung(*kante(g, main, regleS)) == "?");
        REQUIRE(e.meldungen.size() == 1);
        CHECK(e.meldungen[0].pfad == "main.c");
        CHECK(e.meldungen[0].meldung.ort.zeile == 7);
        CHECK(e.meldungen[0].meldung.text.find("antrieb/motor.c:4") != std::string::npos);
        CHECK(e.meldungen[0].meldung.text.find("sim/sim.c:1") != std::string::npos);
        // fremd und Makro: ausserhalb jeder Datei, ohne Fundstelle
        CHECK(g.knoten[fremdK].art == AufrufKnotenArt::Fremd);
        CHECK(g.knoten[pruefe].art == AufrufKnotenArt::Makro);
        CHECK(g.knoten[fremdK].datei == kKeine);
        CHECK(e.stellen[fremdK] == Fundstelle{});
        CHECK(kante(g, main, fremdK)->art == AufrufKantenArt::Sicher);
        // ein Graph: motor_starte einmal, auch wenn log_schreibe es noch einmal ruft
        CHECK(kante(g, logK, motor) != nullptr);
        CHECK(std::count_if(g.knoten.begin(), g.knoten.end(), [](const AufrufKnoten& k) { return k.name == "motor_starte"; }) == 1);
        // ohne Grenze: nichts ist abgeschnitten
        CHECK(std::none_of(g.knoten.begin(), g.knoten.end(), [](const AufrufKnoten& k) { return k.weiter; }));
    }

    TEST_CASE("Tiefe: an der Grenze markiert, was weiter ruft; ohne Fremde gezaehlt")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const Fundstelle main = finde(p, "main.c", "main");

        const Aufrufgraph eins = aufrufgraphAb(p, main, optionen(1)).graph;
        CHECK(knoten(eins, "takt") == kKeine);
        CHECK(eins.knoten[knoten(eins, "motor_starte")].weiter);
        CHECK(knotenText(eins.knoten[knoten(eins, "motor_starte")]) == "motor_starte \xE2\x80\xA6");
        CHECK_FALSE(eins.knoten[knoten(eins, "regle", "sim/sim.c")].weiter);  // ruft nichts
        CHECK(eins.knoten[knoten(eins, "hilfe", "main.c")].weiter);          // ruft sich selbst

        const Aufrufgraph zwei = aufrufgraphAb(p, main, optionen(2)).graph;
        CHECK(knoten(zwei, "takt") != kKeine);
        CHECK_FALSE(zwei.knoten[knoten(zwei, "motor_starte")].weiter);

        const AufrufgraphErgebnis ohne = aufrufgraphAb(p, main, optionen(0, AufrufRichtung::Gerufene, true));
        CHECK(knoten(ohne.graph, "memcpy") == kKeine);
        CHECK(knoten(ohne.graph, "PRUEFE") == kKeine);
        CHECK(ohne.ausgelassen == 2);
        CHECK(std::all_of(ohne.graph.knoten.begin(), ohne.graph.knoten.end(),
                          [](const AufrufKnoten& k) { return k.art == AufrufKnotenArt::Funktion; }));
    }

    TEST_CASE("Aufklappen: eine Funktion an der Grenze zeigt eine Stufe weiter")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const Fundstelle motor = finde(p, "antrieb/motor.c", "motor_starte");

        const Aufrufgraph ab = aufrufgraphAb(p, finde(p, "main.c", "main"), optionen(1, AufrufRichtung::Gerufene, false, {motor})).graph;
        REQUIRE(knoten(ab, "takt") != kKeine);
        CHECK_FALSE(ab.knoten[knoten(ab, "motor_starte")].weiter);
        CHECK_FALSE(ab.knoten[knoten(ab, "takt")].weiter);
        CHECK(ab.knoten[knoten(ab, "hilfe", "main.c")].weiter);  // nicht aufgeklappt

        const Aufrufgraph datei = aufrufgraphDerDatei(p, p.finde("log/log.c"), optionen(2, AufrufRichtung::Gerufene, false, {motor})).graph;
        CHECK(knoten(datei, "takt") != kKeine);
        CHECK(knoten(datei, "hilfe", "antrieb/motor.c") != kKeine);
        CHECK_FALSE(datei.knoten[knoten(datei, "motor_starte")].weiter);
    }

    TEST_CASE("Hoechstens: an der Grenze kommt keine Funktion mehr dazu, wer weiter ruft, ist markiert")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const auto funktionen = [](const Aufrufgraph& g) {
            return std::count_if(g.knoten.begin(), g.knoten.end(),
                                 [](const AufrufKnoten& k) { return k.art == AufrufKnotenArt::Funktion; });
        };
        AufrufgraphOptionen o = optionen(0);
        CHECK(o.hoechstens == 300);
        CHECK_FALSE(aufrufgraphAb(p, finde(p, "main.c", "main"), o).begrenzt);

        o.hoechstens = 3;
        const AufrufgraphErgebnis e = aufrufgraphAb(p, finde(p, "main.c", "main"), o);
        CHECK(e.begrenzt);
        CHECK(funktionen(e.graph) == 3);
        CHECK(e.graph.knoten[0].weiter);  // main ruft mehr, als gezeigt ist
        CHECK(knoten(e.graph, "memcpy") != kKeine);  // Fremde zaehlen nicht

        // was eine aufgeklappte Funktion ruft, kommt trotzdem dazu
        o.aufgeklappt = {finde(p, "main.c", "main")};
        const AufrufgraphErgebnis auf = aufrufgraphAb(p, finde(p, "main.c", "main"), o);
        CHECK(knoten(auf.graph, "log_schreibe") != kKeine);
        CHECK_FALSE(auf.graph.knoten[0].weiter);

        // nach oben und im Graph einer Datei ebenso
        AufrufgraphOptionen eins = optionen(0, AufrufRichtung::Aufrufer);
        eins.hoechstens = 1;
        const AufrufgraphErgebnis oben = aufrufgraphAb(p, finde(p, "antrieb/motor.c", "motor_starte"), eins);
        CHECK(oben.begrenzt);
        CHECK(funktionen(oben.graph) == 1);
        CHECK(oben.graph.knoten[0].weiter);
        const AufrufgraphErgebnis datei = aufrufgraphDerDatei(p, p.finde("antrieb/motor.c"), eins);
        CHECK(datei.begrenzt);
        CHECK(funktionen(datei.graph) == 1);
    }

    TEST_CASE("Nach oben und beides")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const Fundstelle motor = finde(p, "antrieb/motor.c", "motor_starte");

        const Aufrufgraph oben = aufrufgraphAb(p, motor, optionen(1, AufrufRichtung::Aufrufer)).graph;
        CHECK(oben.titel == "callers of motor_starte()");
        const std::size_t m = knoten(oben, "motor_starte");
        REQUIRE(knoten(oben, "main") != kKeine);
        REQUIRE(knoten(oben, "log_schreibe") != kKeine);
        CHECK(kante(oben, knoten(oben, "main"), m) != nullptr);  // die Kante zeigt weiter vom Rufer zum Gerufenen
        CHECK(kante(oben, knoten(oben, "log_schreibe"), m) != nullptr);
        CHECK(knoten(oben, "takt") == kKeine);
        CHECK(oben.knoten[knoten(oben, "log_schreibe")].weiter);  // main ruft es
        CHECK_FALSE(oben.knoten[knoten(oben, "main")].weiter);    // niemand ruft main

        const Aufrufgraph beide = aufrufgraphAb(p, motor, optionen(1, AufrufRichtung::Beide)).graph;
        CHECK(knoten(beide, "takt") != kKeine);
        CHECK(knoten(beide, "main") != kKeine);
        // nach unten nur weiter nach unten: was main sonst ruft, steht nicht da
        CHECK(knoten(beide, "memcpy") == kKeine);
    }

    TEST_CASE("Der Graph einer Datei: ihre Funktionen; ein Aufruf nach draussen endet dort")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const AufrufgraphErgebnis e = aufrufgraphDerDatei(p, p.finde("log/log.c"));
        const Aufrufgraph& g = e.graph;
        CHECK(g.titel == "calls in log/log.c");
        CHECK(g.dateien.front() == "log/log.c");
        REQUIRE(g.knoten.size() == 2);
        CHECK(g.knoten[0].name == "log_schreibe");
        CHECK_FALSE(g.knoten[0].wurzel);
        CHECK(g.knoten[1].name == "motor_starte");
        CHECK(g.knoten[1].weiter);  // ruft selbst, gezeigt ist es nicht
        CHECK(knoten(g, "takt") == kKeine);

        // eine Datei ohne Funktion mit Rumpf: ein leerer Graph
        CHECK(aufrufgraphDerDatei(p, p.dateien.size()).graph.knoten.empty());
        CHECK(aufrufgraphAb(p, Fundstelle{}).graph.knoten.empty());
    }

    TEST_CASE("Bedingte Uebersetzung: Varianten gepunktet mit Bedingung, Rahmen in Farbe")
    {
        const auto graph = [](bool varianten, bool bereiche) {
            const Projekt p = bedingt(varianten, bereiche);
            return aufrufgraphAb(p, finde(p, "t.c", "f")).graph;
        };
        {
            const Aufrufgraph g = graph(false, false);
            CHECK(knoten(g, "b") == kKeine);
            CHECK(kante(g, 0, knoten(g, "c"))->bedingung.empty());
            CHECK_FALSE(g.mitRahmen());
        }
        {
            const Aufrufgraph g = graph(true, false);
            const AufrufKante* b = kante(g, 0, knoten(g, "b"));
            REQUIRE(b != nullptr);
            CHECK_FALSE(b->gilt);
            CHECK(beschriftung(*b) == "defined(MIT_B)");
            CHECK(kante(g, 0, knoten(g, "c"))->gilt);
            CHECK(beschriftung(*kante(g, 0, knoten(g, "c"))).empty());
            const std::vector<std::size_t> farben = farbenDerBedingungen(g);
            CHECK(std::all_of(farben.begin(), farben.end(), [](std::size_t f) { return f == kKeine; }));
        }
        {
            const Aufrufgraph g = graph(false, true);
            const std::size_t c = knoten(g, "c");
            CHECK(kante(g, 0, c)->bedingung == "!defined(MIT_B)");
            CHECK(beschriftung(*kante(g, 0, c)).empty());  // die Farbe sagt es
            const std::vector<std::size_t> farben = farbenDerBedingungen(g);
            for (std::size_t e = 0; e < g.kanten.size(); ++e)
                CHECK((farben[e] != kKeine) == (g.kanten[e].nach == c));
            // die Legende nennt die Bedingung
            const FlussAnordnung a = ordneAn(g);
            REQUIRE(a.legende.size() == 1);
            CHECK(a.legende[0].titel == "!defined(MIT_B)");
        }
    }

    TEST_CASE("Kanten unter mehreren Bedingungen: immer schlaegt sie, sonst mit || verbunden")
    {
        constexpr std::string_view quelle = "void g(void) { }\n"
                                            "void f(void)\n"
                                            "{\n"
                                            "    a();\n"
                                            "#ifdef X\n"
                                            "    g();\n"
                                            "#endif\n"
                                            "#ifdef Y\n"
                                            "    g();\n"
                                            "#endif\n"
                                            "}\n";
        ProjektOptionen o;
        o.varianten = true;
        const Projekt p = liesText(quelle, "t.c", o);
        const Aufrufgraph g = aufrufgraphAb(p, finde(p, "t.c", "f")).graph;
        const AufrufKante* k = kante(g, 0, knoten(g, "g"));
        REQUIRE(k != nullptr);
        CHECK_FALSE(k->gilt);
        CHECK(k->bedingung == "defined(X) || defined(Y)");

        o.definitionen.push_back(c2d::sprache::definitionAus("Y"));
        const Projekt mitY = liesText(quelle, "t.c", o);
        const Aufrufgraph gy = aufrufgraphAb(mitY, finde(mitY, "t.c", "f")).graph;
        const AufrufKante* ky = kante(gy, 0, knoten(gy, "g"));
        REQUIRE(ky != nullptr);
        CHECK(ky->gilt);  // einer gilt, ohne Bedingung: die Kante gilt immer
        CHECK(ky->bedingung.empty());
    }

    TEST_CASE("Anordnung: nichts ueberlappt, jede Datei in ihrem Rahmen, die Rahmen getrennt, Titel ganz")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const Aufrufgraph g = aufrufgraphAb(p, finde(p, "main.c", "main"), optionen(0)).graph;
        for (const bool nachRechts : {false, true})
        {
            CAPTURE(nachRechts);
            AnordnungOptionen o;
            o.nachRechts = nachRechts;
            const FlussAnordnung a = ordneAn(g, o);
            REQUIRE(a.knoten.size() == g.knoten.size());
            REQUIRE(a.kanten.size() == g.kanten.size());
            REQUIRE(a.gruppen.size() == g.dateien.size());
            for (std::size_t i = 0; i < a.knoten.size(); ++i)
            {
                for (std::size_t j = i + 1; j < a.knoten.size(); ++j) CHECK_FALSE(ueberlappt(a.knoten[i], a.knoten[j]));
                CHECK(a.knoten[i].x + a.knoten[i].breite <= a.breite);
                CHECK(a.knoten[i].y + a.knoten[i].hoehe <= a.hoehe);
                // in seinem Rahmen, in keinem anderen
                for (std::size_t d = 0; d < a.gruppen.size(); ++d)
                {
                    const bool eigen = g.knoten[i].art == AufrufKnotenArt::Funktion && g.knoten[i].datei == d;
                    if (eigen)
                        CHECK(liegtIn(a.knoten[i], a.gruppen[d]));
                    else
                        CHECK_FALSE(ueberlappt(a.knoten[i], a.gruppen[d]));
                }
            }
            for (std::size_t d = 0; d < a.gruppen.size(); ++d)
                for (std::size_t e = d + 1; e < a.gruppen.size(); ++e) CHECK_FALSE(ueberlappt(a.gruppen[d], a.gruppen[e]));
            // jeder Rahmen traegt seinen Pfad ganz, in beiden Richtungen
            for (std::size_t d = 0; d < a.gruppen.size(); ++d)
                CHECK(titelZeichen(a.gruppen[d], o) >= g.dateien[d].size());
        }
    }

    TEST_CASE("Anordnung: ein Rueckruf aus einem spaeteren Band zieht das fruehere nicht hinter sich her")
    {
        // gefunden an LLC, main in Main_CPU1.c: Millionen Pixel breit, weil Baender einander schoben
        TempOrdner t;
        t.lege("main.c", "void zurueck(void) { }\n"
                         "void auch(void) { }\n"
                         "int main(void) { a(); b(); c(); return 0; }\n");
        t.lege("a.c", "void a(void) { auch(); }\n");
        t.lege("b.c", "void b(void) { zurueck(); }\n");
        t.lege("c.c", "void c(void) { zurueck(); auch(); }\n");
        const Projekt p = liesOrdner(t.weg());
        const Aufrufgraph g = aufrufgraphAb(p, finde(p, "main.c", "main"), optionen(0)).graph;
        REQUIRE(g.mitRahmen());
        double summe = 0;
        const FlussAnordnung a = ordneAn(g);
        for (const Rechteck& r : a.knoten) summe += r.breite + 200;  // grosszuegig: Rahmen, Abstaende, Titel
        CHECK(a.breite < summe);
        for (std::size_t d = 0; d < a.gruppen.size(); ++d)
            for (std::size_t e = d + 1; e < a.gruppen.size(); ++e) CHECK_FALSE(ueberlappt(a.gruppen[d], a.gruppen[e]));
    }

    TEST_CASE("Formate: Rahmen je Datei, mehrdeutig gestrichelt, fremd grau, Bedingung in Farbe")
    {
        TempOrdner t;
        legeProjekt(t);
        const Projekt p = liesOrdner(t.weg());
        const Aufrufgraph g = aufrufgraphAb(p, finde(p, "main.c", "main"), optionen(0)).graph;
        const c2d::ausgabe::Optionen o;

        const std::string mermaid = c2d::ausgabe::mermaid().schreibe(g, o);
        CHECK(mermaid.find("%% calls from main()\nflowchart TD\n") == 0);
        CHECK(mermaid.find("subgraph d0[\"main.c\"]") != std::string::npos);
        CHECK(mermaid.find("n0[[\"main\"]]") != std::string::npos);
        CHECK(mermaid.find(" -.->|\"?\"| ") != std::string::npos);
        CHECK(mermaid.find("class n0 wurzel") != std::string::npos);
        CHECK(mermaid.find(" fremd\n") != std::string::npos);
        CHECK(mermaid.find(" makro\n") != std::string::npos);

        const std::string dot = c2d::ausgabe::dot().schreibe(g, o);
        CHECK(dot.find("subgraph cluster_0 {") != std::string::npos);
        CHECK(dot.find("[style=dashed, label=\"?\"]") != std::string::npos);
        CHECK(dot.find("style=\"filled,dashed\"") != std::string::npos);  // das Makro

        const std::string drawio = c2d::ausgabe::drawio().schreibe(g, o);
        CHECK(drawio.find("id=\"d0\" value=\"main.c\"") != std::string::npos);
        CHECK(drawio.find("parent=\"d0\"") != std::string::npos);
        CHECK(drawio.find("dashed=1;edgeStyle") == std::string::npos);  // die Kante: erst der Weg, dann der Strich
        CHECK(drawio.find("endFill=1;html=1;dashed=1;") != std::string::npos);

        const std::string svg = c2d::ausgabe::svg().schreibe(g, o);
        CHECK(svg.find("stroke-dasharray:6 4;") != std::string::npos);
        CHECK(svg.find("class=\"fremd\"") != std::string::npos);
        CHECK(svg.find(">antrieb/motor.c<") != std::string::npos);  // der Titel ganz

        // aus einem Zweig, der nicht gilt: gepunktet; mit Rahmen: die Farbe
        ProjektOptionen v;
        v.varianten = true;
        const Projekt pv = liesText(kBedingt, "t.c", v);
        const Aufrufgraph gv = aufrufgraphAb(pv, finde(pv, "t.c", "f")).graph;
        CHECK(c2d::ausgabe::dot().schreibe(gv, o).find("[style=dotted, label=\"defined(MIT_B)\"]") != std::string::npos);
        CHECK(c2d::ausgabe::svg().schreibe(gv, o).find("stroke-dasharray:2 3;") != std::string::npos);
        CHECK(c2d::ausgabe::drawio().schreibe(gv, o).find("dashPattern=1 3;") != std::string::npos);
        ProjektOptionen r;
        r.bereiche = true;
        const Projekt pr = liesText(kBedingt, "t.c", r);
        const Aufrufgraph gr = aufrufgraphAb(pr, finde(pr, "t.c", "f")).graph;
        const std::string farbe(rahmenFarbe(0));
        CHECK(c2d::ausgabe::mermaid().schreibe(gr, o).find("linkStyle 1 stroke:" + farbe) != std::string::npos);
        CHECK(c2d::ausgabe::dot().schreibe(gr, o).find("[color=\"" + farbe + "\"]") != std::string::npos);
        CHECK(c2d::ausgabe::svg().schreibe(gr, o).find("stroke:" + farbe + ";") != std::string::npos);
        CHECK(c2d::ausgabe::drawio().schreibe(gr, o).find("strokeColor=" + farbe + ";") != std::string::npos);
    }
}
