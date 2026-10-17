/**
 * @file test_Breite.cpp
 * @brief Breiten-Test: keine geschichtete Anordnung ist mehr als kHoechstesQuerVerhaeltnis mal so
 *        breit (nach rechts: hoch), wie ihre breiteste Schicht braucht
 *
 * Gebaute Formen, die die bekannten Wege des Aufschaukelns nachstellen (Session 5: Baender im
 * Aufrufgraph, Ketten von Stuetzpunkten): ein breiter Faecher, viele lange Kanten zum Ende,
 * verschachtelte Schleifen mit Rueckwegen, ein Automat mit Uebergaengen zurueck, Datentypen, die
 * sich gegenseitig nennen, ein Aufrufgraph ueber Dateien sehr ungleicher Groesse. Je beide Richtungen.
 * Echter Code: `c2d <ordner> --breite`.
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "TempOrdner.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/projekt/Aufrufgraph.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d::diagramm;

namespace
{

AnordnungOptionen richtung(bool nachRechts)
{
    AnordnungOptionen o;
    o.nachRechts = nachRechts;
    return o;
}

/// Prueft eine Anordnung in beiden Richtungen; `wie` nennt sie in der Meldung.
template <typename Diagramm>
void pruefe(const Diagramm& d, const std::string& wie)
{
    for (const bool nachRechts : {false, true})
    {
        const auto a = ordneAn(d, richtung(nachRechts));
        const double v = querVerhaeltnis(a);
        CAPTURE(wie);
        CAPTURE(nachRechts);
        CAPTURE(a.quer);
        CAPTURE(a.querBedarf);
        CHECK(a.querBedarf > 0);
        CHECK(v <= kHoechstesQuerVerhaeltnis);
        CHECK(v > 0.5);  // das Mass misst etwas: so viel braucht die Anordnung mindestens
    }
}

const c2d::modell::Funktion& funktion(const c2d::sprache::Ergebnis& e, std::string_view name)
{
    for (const c2d::modell::Funktion& f : e.einheit.funktionen)
        if (f.name == name) return f;
    FAIL("no such function");
    return e.einheit.funktionen.front();
}

std::string faecher()
{
    std::string s = "int faecher(int x)\n{\n  switch (x) {\n";
    for (int i = 0; i < 14; ++i) s += "  case " + std::to_string(i) + ": tue" + std::to_string(i) + "(); break;\n";
    return s + "  default: nichts(); break;\n  }\n  return 0;\n}\n";
}

std::string langeKanten()
{
    // viele fruehe returns: je eine lange Kante zum Ende, an einer Kette entlang (sbl_main.c)
    std::string s = "int lang(int a)\n{\n";
    for (int i = 0; i < 12; ++i)
        s += "  if (a == " + std::to_string(i) + ") return " + std::to_string(i) + ";\n  schritt" + std::to_string(i) + "();\n";
    return s + "  return -1;\n}\n";
}

constexpr const char* kSchleifen =
    "void schleifen(int n)\n{\n"
    "  for (int i = 0; i < n; ++i) {\n"
    "    while (bereit()) {\n"
    "      if (fehler()) continue;\n"
    "      if (fertig()) break;\n"
    "      do { warte(); } while (beschaeftigt());\n"
    "      if (abbruch()) return;\n"
    "    }\n"
    "    for (int k = 0; k < i; ++k) { if (k == 3) continue; zaehle(k); }\n"
    "  }\n}\n";

constexpr const char* kAutomat =
    "typedef enum { AUS, START, LAUF, PAUSE, FEHLER, STOPP, WARTE, ENDE } Zustand;\n"
    "static Zustand z;\n"
    "void schritt(int e)\n{\n"
    "  switch (z) {\n"
    "  case AUS: if (e == 1) z = START; break;\n"
    "  case START: if (e == 2) z = LAUF; else z = FEHLER; break;\n"
    "  case LAUF: if (e == 3) z = PAUSE; else if (e == 4) z = STOPP; else if (e == 9) z = FEHLER; break;\n"
    "  case PAUSE: if (e == 2) z = LAUF; else z = WARTE; break;\n"
    "  case WARTE: if (e == 5) z = START; else z = AUS; break;\n"
    "  case FEHLER: z = AUS; break;\n"
    "  case STOPP: if (e == 6) z = ENDE; else z = AUS; break;\n"
    "  case ENDE: z = AUS; break;\n"
    "  }\n}\n";

constexpr const char* kTypen =
    "struct Knoten;\n"
    "typedef struct Kante { struct Knoten* von; struct Knoten* nach; int gewicht; } Kante;\n"
    "typedef struct Knoten { Kante* aus[4]; Kante* ein[4]; struct Graph* graph; } Knoten;\n"
    "typedef struct Graph { Knoten* knoten[16]; Kante* kanten[32]; struct Pfad* pfad; } Graph;\n"
    "typedef struct Pfad { Knoten* weg[16]; Graph* in; struct Pfad* naechster; } Pfad;\n"
    "typedef struct Suche { Graph* g; Pfad* bester; Knoten* start; Knoten* ziel; } Suche;\n"
    "typedef struct Ergebnis { Suche suche; Pfad pfad; Kante letzte; } Ergebnis;\n";

}  // namespace

TEST_SUITE("Breite")
{
    TEST_CASE("Ablauf: Faecher, lange Kanten zum Ende, Schleifen mit Rueckwegen")
    {
        const c2d::sprache::Ergebnis e =
            c2d::sprache::c().lies(faecher() + langeKanten() + kSchleifen, "breite.c");
        for (const char* name : {"faecher", "lang", "schleifen"})
        {
            const Flussdiagramm roh = flussVon(funktion(e, name));
            pruefe(roh, std::string(name) + " (raw)");
            pruefe(fasseAnweisungsfolgenZusammen(roh), name);
        }
    }

    TEST_CASE("Fruehe returns: der Hauptstrang steht senkrecht, die Spalten liegen dicht")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(langeKanten(), "lang.c");
        const Flussdiagramm d = flussVon(funktion(e, "lang"));
        const FlussAnordnung a = ordneAn(d);
        std::vector<double> mitten;
        for (std::size_t i = 0; i < d.knoten().size(); ++i)
            if (d.knoten()[i].zeilen.front().starts_with("schritt")) mitten.push_back(a.knoten[i].mitte().x);
        REQUIRE(mitten.size() == 12);
        // keine Treppe: alle Mitten innerhalb einer Spaltenbreite (vorher je return eine Knotenbreite weiter;
        // oben bleibt ein kleiner Versatz aus dem Mitteln der vier Laeufe, wo links noch keine Spalten stehen)
        const auto [min, max] = std::minmax_element(mitten.begin(), mitten.end());
        CHECK(*max - *min < 24);
        CHECK(querVerhaeltnis(a) < 1.2);  // vorher x2.46: eine Treppe, je return eine Knotenbreite
    }

    TEST_CASE("Zustandsautomat und Datentypen")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(std::string(kAutomat) + kTypen, "zustand.c");
        const std::vector<Zustandsdiagramm> automaten = automatenVon(e.einheit);
        REQUIRE(automaten.size() == 1);
        pruefe(automaten.front(), "automaton");
        pruefe(strukturVon(e.einheit), "data types");
    }

    TEST_CASE("Aufrufgraph ueber Dateien sehr ungleicher Groesse: Baender, Rahmen, Rueckwege")
    {
        TempOrdner t;
        std::string gross = "#include \"klein.h\"\n";
        for (int i = 0; i < 10; ++i)
        {
            gross += "void g" + std::to_string(i) + "(void) { k0(); ";
            if (i + 1 < 10) gross += "g" + std::to_string(i + 1) + "(); ";
            gross += "memcpy(0, 0, 0); }\n";
        }
        gross += "int main(void) { g0(); g5(); k1(); return 0; }\n";
        t.lege("gross.c", gross);
        t.lege("klein.h", "void k0(void);\nvoid k1(void);\n");
        t.lege("klein.c", "void k0(void) { k1(); }\nvoid k1(void) { k0(); g9(); }\n");  // Rueckweg in die grosse Datei
        t.lege("mittel/m.c", "void m(void) { k0(); k1(); }\n");
        const c2d::projekt::Projekt p = c2d::projekt::liesOrdner(t.weg());

        c2d::projekt::AufrufgraphOptionen alle;
        alle.tiefe = 0;
        const auto main = c2d::projekt::funktionenNamens(p, "main");
        REQUIRE(main.size() == 1);
        const Aufrufgraph ab = c2d::projekt::aufrufgraphAb(p, main.front(), alle).graph;
        REQUIRE(ab.mitRahmen());
        pruefe(ab, "calls from main");
        alle.richtung = c2d::projekt::AufrufRichtung::Beide;
        pruefe(c2d::projekt::aufrufgraphAb(p, main.front(), alle).graph, "calls from main, both");
        pruefe(c2d::projekt::aufrufgraphDerDatei(p, p.finde("gross.c")).graph, "calls of gross.c");
    }
}
