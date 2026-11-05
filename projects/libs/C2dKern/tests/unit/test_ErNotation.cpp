/**
 * @file test_ErNotation.cpp
 * @brief Notationen der Tabellen (Entscheide N1-N6): NOT NULL und UNIQUE im Leser, Vielfachheiten,
 *        Chen und (min,max) als ER-Diagramm, ihre Anordnung und die Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/ErDiagramm.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;
using namespace c2d::diagramm;

namespace
{

/// Die Schule aus beispiele/sql/schule.sql, gekuerzt: 1:1 kann, 1:n Pflicht, 1:n kann, m:n mit Attribut,
/// 1:1 Pflicht aus Index, Fremdschluessel und ALTER COLUMN.
constexpr std::string_view kSchule = R"(
CREATE TABLE lehrer (lehrer_id INTEGER PRIMARY KEY, name VARCHAR(60) NOT NULL);
CREATE TABLE klasse (klasse_id INTEGER PRIMARY KEY, bezeichnung VARCHAR(10),
                     klassenlehrer INTEGER UNIQUE REFERENCES lehrer (lehrer_id));
CREATE TABLE schueler (schueler_id INTEGER PRIMARY KEY, name VARCHAR(60),
                       klasse_id INTEGER NOT NULL REFERENCES klasse (klasse_id));
CREATE TABLE kurs (kurs_id INTEGER PRIMARY KEY, lehrer_id INTEGER,
                   CONSTRAINT leitet FOREIGN KEY (lehrer_id) REFERENCES lehrer (lehrer_id));
CREATE TABLE belegt (schueler_id INTEGER REFERENCES schueler (schueler_id),
                     kurs_id INTEGER REFERENCES kurs (kurs_id), note INTEGER,
                     PRIMARY KEY (schueler_id, kurs_id));
CREATE TABLE ausweis (ausweis_id INTEGER PRIMARY KEY, schueler_id INTEGER);
CREATE UNIQUE INDEX ausweis_je_schueler ON ausweis (schueler_id);
ALTER TABLE ausweis ADD CONSTRAINT gehoert FOREIGN KEY (schueler_id) REFERENCES schueler (schueler_id);
ALTER TABLE ausweis ALTER COLUMN schueler_id SET NOT NULL;
)";

sprache::Ergebnis lies(std::string_view text)
{
    sprache::LeseOptionen o;
    o.dialekt = "postgres";
    return sprache::sql().lies(text, "schule.sql", o);
}

Strukturdiagramm schule(ErNotation notation = ErNotation::Tabellen, ErAttribute attribute = ErAttribute::Alle)
{
    Strukturdiagramm d = strukturVon(lies(kSchule).einheit);
    d.notation = notation;
    d.attribute = attribute;
    return d;
}

const modell::Typ& tabelle(const modell::Einheit& e, std::string_view name)
{
    for (const modell::Typ& t : e.typen)
        if (t.name == name) return t;
    FAIL("no table " << name);
    return e.typen.front();
}

const Beziehung& fremdschluessel(const Strukturdiagramm& d, std::string_view von, std::string_view nach)
{
    for (const Beziehung& b : d.beziehungen)
        if (b.fremdschluessel && d.kaesten[b.von].name == von && d.kaesten[b.nach].name == nach) return b;
    FAIL("no foreign key " << von << " -> " << nach);
    return d.beziehungen.front();
}

std::size_t knoten(const ErDiagramm& er, std::string_view name)
{
    for (std::size_t i = 0; i < er.knoten.size(); ++i)
        if (er.knoten[i].name == name) return i;
    FAIL("no node " << name);
    return 0;
}

/// Der Text der Linie zwischen der Entitaet `e` und der Raute `r` (an der Entitaet).
std::string textZwischen(const ErDiagramm& er, std::size_t e, std::size_t r)
{
    for (const ErKante& k : er.kanten)
        if ((k.von == e && k.nach == r && k.textAmVon) || (k.von == r && k.nach == e && !k.textAmVon)) return k.text;
    FAIL("no line between " << er.knoten[e].name << " and " << er.knoten[r].name);
    return {};
}

bool ueberlappen(const Rechteck& a, const Rechteck& b)
{
    return a.x < b.x + b.breite && b.x < a.x + a.breite && a.y < b.y + b.hoehe && b.y < a.y + a.hoehe;
}

}  // namespace

TEST_SUITE("ER-Notationen")
{
    TEST_CASE("Leser: NOT NULL, UNIQUE an Spalte und Tabelle, eindeutiger Index, ALTER COLUMN SET NOT NULL")
    {
        const sprache::Ergebnis e = lies(kSchule);
        const modell::Einheit& u = e.einheit;
        CHECK(tabelle(u, "lehrer").felder[0].nichtNull);  // Primaerschluessel
        CHECK(tabelle(u, "lehrer").felder[1].nichtNull);
        CHECK_FALSE(tabelle(u, "klasse").felder[1].nichtNull);
        CHECK(tabelle(u, "klasse").eindeutig == std::vector<std::vector<std::string>>{{"klassenlehrer"}});
        CHECK(tabelle(u, "schueler").felder[2].nichtNull);
        CHECK(tabelle(u, "belegt").primaer == std::vector<std::string>{"schueler_id", "kurs_id"});
        CHECK(tabelle(u, "belegt").felder[0].nichtNull);  // im zusammengesetzten Primaerschluessel
        const modell::Typ& ausweis = tabelle(u, "ausweis");
        CHECK(ausweis.eindeutig == std::vector<std::vector<std::string>>{{"schueler_id"}});
        CHECK(ausweis.felder[1].nichtNull);  // erst ALTER COLUMN … SET NOT NULL

        // eine Bedingung der Tabelle, mehrspaltig; MySQL `UNIQUE KEY name (…)`; ein Index mit WHERE oder
        // Ausdruck macht nichts eindeutig; DROP NOT NULL nimmt es zurueck
        const sprache::Ergebnis f = lies(R"(
CREATE TABLE t (a INT NOT NULL, b INT, c INT, CONSTRAINT u UNIQUE (a, b), UNIQUE KEY k (c));
CREATE UNIQUE INDEX i1 ON t (b) WHERE b > 0;
CREATE UNIQUE INDEX i2 ON t (lower(c));
CREATE UNIQUE INDEX i3 ON nirgends (x);
ALTER TABLE t ALTER COLUMN a DROP NOT NULL;
)");
        const modell::Typ& t = tabelle(f.einheit, "t");
        CHECK(t.eindeutig == std::vector<std::vector<std::string>>{{"a", "b"}, {"c"}});
        CHECK_FALSE(t.felder[0].nichtNull);
        CHECK(std::any_of(f.meldungen.begin(), f.meldungen.end(), [](const quelle::Meldung& m) {
            return m.text.find("UNIQUE INDEX: table 'nirgends' is not defined") != std::string::npos;
        }));
    }

    TEST_CASE("Vielfachheiten: hoechstens 1 bei eindeutigen Spalten, genau 1 bei NOT NULL")
    {
        const Strukturdiagramm d = schule();
        const Beziehung& klassenlehrer = fremdschluessel(d, "klasse", "lehrer");
        CHECK(klassenlehrer.amVon == Vielfachheit{0, 1});  // UNIQUE
        CHECK(klassenlehrer.amNach == Vielfachheit{0, 1});  // darf leer sein
        CHECK(fremdschluessel(d, "schueler", "klasse").amVon == Vielfachheit{0, kViele});
        CHECK(fremdschluessel(d, "schueler", "klasse").amNach == Vielfachheit{1, 1});
        CHECK(fremdschluessel(d, "kurs", "lehrer").name == "leitet");
        CHECK(fremdschluessel(d, "kurs", "lehrer").amNach == Vielfachheit{0, 1});
        // Teil des zusammengesetzten Schluessels: nicht eindeutig fuer sich, aber NOT NULL
        CHECK(fremdschluessel(d, "belegt", "kurs").amVon == Vielfachheit{0, kViele});
        CHECK(fremdschluessel(d, "belegt", "kurs").amNach == Vielfachheit{1, 1});
        CHECK(fremdschluessel(d, "ausweis", "schueler").amVon == Vielfachheit{0, 1});  // eindeutiger Index
        CHECK(fremdschluessel(d, "ausweis", "schueler").amNach == Vielfachheit{1, 1});  // ALTER COLUMN
        CHECK(fremdschluessel(d, "ausweis", "schueler").spalten == std::vector<std::size_t>{1});
        CHECK(uml({0, kViele}) == "*");
        CHECK(uml({0, 1}) == "0..1");
        CHECK(uml({1, 1}) == "1");
        CHECK(uml({1, kViele}) == "1..*");
        CHECK(minMax({0, kViele}) == "(0,*)");
        CHECK(minMax({1, 1}) == "(1,1)");
        // der Eintrag kennt Schluessel, Fremdschluessel und NOT NULL
        const Kasten& belegt = d.kaesten[4];
        REQUIRE(belegt.name == "belegt");
        CHECK(belegt.eintraege[0].schluessel);
        CHECK(belegt.eintraege[0].fremd);
        CHECK_FALSE(belegt.eintraege[2].schluessel);
    }

    TEST_CASE("Chen: Entitaeten, eine Raute je Fremdschluessel, die Verbindungstabelle als Raute m:n")
    {
        const ErDiagramm er = erDiagrammVon(schule(ErNotation::Chen));
        CHECK_FALSE(er.minMax);
        const std::size_t lehrer = knoten(er, "lehrer");
        const std::size_t klasse = knoten(er, "klasse");
        const std::size_t schueler = knoten(er, "schueler");
        const std::size_t kurs = knoten(er, "kurs");
        const std::size_t belegt = knoten(er, "belegt");
        CHECK(er.knoten[lehrer].art == ErKnotenArt::Entitaet);
        CHECK(er.knoten[belegt].art == ErKnotenArt::Beziehung);
        // die Raute m:n traegt die uebrigen Spalten; ein Fremdschluessel ist kein Attribut
        REQUIRE(er.knoten[belegt].attribute.size() == 1);
        CHECK(er.knoten[belegt].attribute[0].name == "note");
        CHECK(textZwischen(er, schueler, belegt) == "m");
        CHECK(textZwischen(er, kurs, belegt) == "n");
        for (const ErAttribut& a : er.knoten[schueler].attribute) CHECK(a.name != "klasse_id");
        CHECK(er.knoten[schueler].attribute[0].schluessel);
        // 1:n: die Raute heisst nach den Spalten, mit Namen nach der Bedingung
        const std::size_t klasseId = knoten(er, "klasse_id");
        CHECK(textZwischen(er, klasse, klasseId) == "1");
        CHECK(textZwischen(er, schueler, klasseId) == "n");
        CHECK(er.knoten[knoten(er, "leitet")].art == ErKnotenArt::Beziehung);
        // 1:1
        const std::size_t kl = knoten(er, "klassenlehrer");
        CHECK(textZwischen(er, lehrer, kl) == "1");
        CHECK(textZwischen(er, klasse, kl) == "1");
        // der Klick: die Raute zeigt die Spalte ihres Fremdschluessels
        CHECK(er.knoten[klasseId].kasten == 2);
        CHECK(er.knoten[klasseId].eintrag == 2);
        // die ersten Knoten sind die Kaesten, in ihrer Reihenfolge
        for (std::size_t k = 0; k < 6; ++k) CHECK(er.knoten[k].kasten == k);
    }

    TEST_CASE("(min,max): wie oft eine Entitaet teilnimmt; Attribute: Schluessel, keine")
    {
        const ErDiagramm er = erDiagrammVon(schule(ErNotation::MinMax));
        CHECK(er.minMax);
        const std::size_t klasse = knoten(er, "klasse");
        const std::size_t schueler = knoten(er, "schueler");
        const std::size_t klasseId = knoten(er, "klasse_id");
        CHECK(textZwischen(er, klasse, klasseId) == "(0,*)");
        CHECK(textZwischen(er, schueler, klasseId) == "(1,1)");
        CHECK(textZwischen(er, schueler, knoten(er, "belegt")) == "(0,*)");
        CHECK(textZwischen(er, knoten(er, "ausweis"), knoten(er, "gehoert")) == "(1,1)");
        CHECK(textZwischen(er, schueler, knoten(er, "gehoert")) == "(0,1)");

        const ErDiagramm schluessel = erDiagrammVon(schule(ErNotation::MinMax, ErAttribute::Schluessel));
        CHECK(schluessel.knoten[knoten(schluessel, "lehrer")].attribute.size() == 1);
        CHECK(schluessel.knoten[knoten(schluessel, "belegt")].attribute.empty());
        const ErDiagramm keine = erDiagrammVon(schule(ErNotation::Chen, ErAttribute::Keine));
        for (const ErKnoten& k : keine.knoten) CHECK(k.attribute.empty());
    }

    TEST_CASE("Chen: eine Klasse neben Tabellen bleibt ein Kasten; nur Tabellen werden umgedeutet")
    {
        Strukturdiagramm d;
        d.kaesten.push_back({KastenArt::Klasse, "Lager", {{"int", "n", "", '+', 3}}, {}, 2});
        d.kaesten.push_back({KastenArt::Struktur, "Ort", {}, {}, 5});
        d.kaesten.push_back({KastenArt::Tabelle, "artikel", {{"INT", "id", "PK", '\0', 9, true}}, {}, 8});
        d.kaesten[2].eintraege[0].schluessel = true;
        d.beziehungen.push_back({0, 1, BeziehungArt::Enthaelt, "ort"});
        d.notation = ErNotation::Chen;
        REQUIRE(alsErDiagramm(d));
        const ErDiagramm er = erDiagrammVon(d);
        CHECK(er.knoten[0].art == ErKnotenArt::Kasten);
        CHECK(er.knoten[0].zeilen == std::vector<std::string>{"+ int n"});
        CHECK(er.knoten[1].stereotyp == "struct");
        CHECK(er.knoten[2].art == ErKnotenArt::Entitaet);
        REQUIRE(er.kanten.size() == 1);
        CHECK(er.kanten[0].zwischenKaesten);
        CHECK(er.kanten[0].art == BeziehungArt::Enthaelt);
        // ohne Tabelle bleibt es ein Strukturdiagramm
        d.kaesten.pop_back();
        CHECK_FALSE(alsErDiagramm(d));
    }

    TEST_CASE("Anordnung Chen: nichts ueberlappt, die Linien enden am Kern, auch nach rechts")
    {
        for (const bool nachRechts : {false, true})
        {
            CAPTURE(nachRechts);
            const ErDiagramm er = erDiagrammVon(schule(ErNotation::Chen));
            AnordnungOptionen o;
            o.nachRechts = nachRechts;
            const ErAnordnung a = ordneAn(er, o);
            REQUIRE(a.knoten.size() == er.knoten.size());
            REQUIRE(a.kanten.size() == er.kanten.size());
            std::vector<Rechteck> alle = a.knoten;
            for (const ErAnordnung::Ellipse& el : a.ellipsen) alle.push_back(el.ort);
            for (std::size_t i = 0; i < alle.size(); ++i)
                for (std::size_t j = i + 1; j < alle.size(); ++j)
                {
                    CAPTURE(i);
                    CAPTURE(j);
                    CHECK_FALSE(ueberlappen(alle[i], alle[j]));
                }
            // jede Linie beginnt und endet auf dem Rand ihrer Knoten (nicht am Rand der Ellipsen daneben)
            const auto aufRand = [](Punkt p, const Rechteck& r) {
                const bool innen = p.x >= r.x - 0.5 && p.x <= r.x + r.breite + 0.5 && p.y >= r.y - 0.5 && p.y <= r.y + r.hoehe + 0.5;
                const bool rand = std::abs(p.x - r.x) < 0.5 || std::abs(p.x - r.x - r.breite) < 0.5 || std::abs(p.y - r.y) < 0.5 ||
                                  std::abs(p.y - r.y - r.hoehe) < 0.5;
                return innen && rand;
            };
            for (std::size_t e = 0; e < a.kanten.size(); ++e)
            {
                CAPTURE(e);
                REQUIRE(a.kanten[e].size() >= 2);
                CHECK(aufRand(a.kanten[e].front(), a.knoten[er.kanten[e].von]));
                CHECK(aufRand(a.kanten[e].back(), a.knoten[er.kanten[e].nach]));
            }
            // je Attribut eine Ellipse mit einer Linie zu ihrem Knoten
            std::size_t attribute = 0;
            for (const ErKnoten& k : er.knoten) attribute += k.attribute.size();
            CHECK(a.ellipsen.size() == attribute);
            for (const ErAnordnung::Ellipse& el : a.ellipsen) CHECK(aufRand(el.linie.back(), a.knoten[el.knoten]));
            CHECK(querVerhaeltnis(a) <= kHoechstesQuerVerhaeltnis);
        }
    }

    TEST_CASE("Anordnung Tables und Crow's foot: Texte und Zeichen an den Enden der Fremdschluessel")
    {
        const Strukturdiagramm tabellen = schule(ErNotation::Tabellen);
        const StrukturAnordnung a = ordneAn(tabellen);
        REQUIRE(a.enden.size() == tabellen.beziehungen.size());
        for (std::size_t e = 0; e < tabellen.beziehungen.size(); ++e)
        {
            const Beziehung& b = tabellen.beziehungen[e];
            CHECK(a.enden[e][0].text == uml(b.amVon));
            CHECK(a.enden[e][1].text == uml(b.amNach));
        }
        const Strukturdiagramm kraehe = schule(ErNotation::Kraehenfuss);
        const StrukturAnordnung k = ordneAn(kraehe);
        for (std::size_t e = 0; e < kraehe.beziehungen.size(); ++e)
        {
            const Beziehung& b = kraehe.beziehungen[e];
            CAPTURE(e);
            CHECK(k.enden[e][0].text.empty());
            // viele: drei Striche zum Ende und einer in der Mitte; eins: ein Strich quer; null: ein Kreis
            CHECK(k.enden[e][0].striche.size() == (b.amVon.max == 1 ? 1U : 2U));
            CHECK(k.enden[e][0].kreise.size() == 1);
            CHECK(k.enden[e][1].striche.size() == (b.amNach.min == 1 ? 2U : 1U));
            CHECK(k.enden[e][1].kreise.size() == (b.amNach.min == 1 ? 0U : 1U));
        }
    }

    TEST_CASE("Formate: Tables, Crow's foot, Chen in SVG, Mermaid, DOT, draw.io")
    {
        ausgabe::Optionen o;
        const auto text = [&](std::string_view format, const Strukturdiagramm& d) { return ausgabe::format(format)->schreibe(d, o); };

        const std::string tMermaid = text("mermaid", schule(ErNotation::Tabellen));
        CHECK(tMermaid.find("schueler \"*\" --> \"1\" klasse : klasse_id") != std::string::npos);
        CHECK(tMermaid.find("klasse \"0..1\" --> \"0..1\" lehrer") != std::string::npos);
        CHECK(text("dot", schule()).find("taillabel=\"*\", headlabel=\"1\"") != std::string::npos);
        CHECK(text("drawio", schule()).find("edgeLabel;") != std::string::npos);
        CHECK(text("svg", schule()).find(">0..1</tspan>") != std::string::npos);

        const Strukturdiagramm k = schule(ErNotation::Kraehenfuss);
        const std::string kMermaid = text("mermaid", k);
        CHECK(kMermaid.find("erDiagram") != std::string::npos);
        CHECK(kMermaid.find("klasse ||--o{ schueler : \"klasse_id\"") != std::string::npos);
        CHECK(kMermaid.find("lehrer |o--o| klasse : \"klassenlehrer\"") != std::string::npos);
        CHECK(kMermaid.find("INTEGER schueler_id PK, FK") != std::string::npos);
        CHECK(text("dot", k).find("arrowtail=crowodot, arrowhead=teetee") != std::string::npos);
        CHECK(text("drawio", k).find("startArrow=ERzeroToMany;startFill=0;endArrow=ERmandOne") != std::string::npos);
        CHECK(text("svg", k).find("class=\"kreis\"") != std::string::npos);

        const Strukturdiagramm c = schule(ErNotation::Chen);
        const std::string cSvg = text("svg", c);
        CHECK(cSvg.find("<ellipse") != std::string::npos);
        CHECK(cSvg.find("text-decoration=\"underline\"><tspan") != std::string::npos);
        const std::string cMermaid = text("mermaid", c);
        CHECK(cMermaid.find("flowchart TD") != std::string::npos);
        CHECK(cMermaid.find("{\"belegt\"}") != std::string::npos);
        CHECK(cMermaid.find("([\"<u>lehrer_id</u>\"])") != std::string::npos);
        const std::string cDot = text("dot", c);
        CHECK(cDot.find("shape=diamond, label=\"belegt\"") != std::string::npos);
        CHECK(cDot.find("label=<<U>lehrer_id</U>>") != std::string::npos);
        CHECK(cDot.find("taillabel=\"m\"") != std::string::npos);
        const std::string cDrawio = text("drawio", c);
        CHECK(cDrawio.find("rhombus;") != std::string::npos);
        CHECK(cDrawio.find("ellipse;") != std::string::npos);
        CHECK(text("mermaid", schule(ErNotation::MinMax)).find("\"(1,1)\"") != std::string::npos);

        // Crow's foot mit einer Klasse daneben: erDiagram kennt keine Klassen
        Strukturdiagramm gemischt = schule(ErNotation::Kraehenfuss);
        gemischt.kaesten.push_back({KastenArt::Klasse, "Lager", {}, {}, 1});
        const std::string g = text("mermaid", gemischt);
        CHECK(g.find("classDiagram") != std::string::npos);
        CHECK(g.find("%% Crow's foot needs erDiagram") != std::string::npos);
    }
}
