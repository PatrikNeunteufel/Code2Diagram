/**
 * @file Mermaid.cpp
 * @brief Mermaid: flowchart, classDiagram und stateDiagram-v2
 *
 * Welche Form ein Knoten bekommt, steht als Tabelle hier — im Code, nicht in
 * einer Konfigurationsdatei (Lehre 5, docs/Ernte_Vorgaenger.md).
 */
#include "c2d/ausgabe/Ausgabe.hpp"

#include "Text.hpp"
#include "c2d/diagramm/ErDiagramm.hpp"
#include "c2d/diagramm/Farben.hpp"

#include <algorithm>

namespace c2d::ausgabe
{

namespace
{

using namespace c2d::diagramm;

struct Form
{
    std::string_view auf;
    std::string_view zu;
};

/// Knotenart -> Form. Die Zuordnung stammt aus dem Regelwerk der Vorgaenger
/// (Raute fuer Bedingungen, Rechteck fuer Anweisungen).
constexpr Form formVon(KnotenArt art)
{
    switch (art)
    {
        case KnotenArt::Start:
        case KnotenArt::Ende:
            return {"([", "])"};  // Stadion
        case KnotenArt::Verzweigung:
            return {"{", "}"};  // Raute
        case KnotenArt::Aufruf:
            return {"[[", "]]"};  // Unterprogramm
        case KnotenArt::Variante:
            return {"{{", "}}"};  // Sechseck
        case KnotenArt::Anweisung:
            break;
    }
    return {"[", "]"};
}

/// Text fuer ein Label in "...": was Mermaid als Syntax oder HTML laese, wird Entity-Code.
std::string maskiere(std::string_view text)
{
    std::string aus;
    aus.reserve(text.size());
    for (const char c : text)
    {
        switch (c)
        {
            case '#': aus += "#35;"; break;  // leitet selbst Entity-Codes ein
            case '"': aus += "#quot;"; break;
            case '<': aus += "#lt;"; break;
            case '>': aus += "#gt;"; break;
            case '&': aus += "#amp;"; break;
            case '\r': break;
            case '\n':
            case '\t': aus += ' '; break;
            default: aus += c; break;
        }
    }
    return aus;
}

/// Text hinter `:` in einem stateDiagram: ein weiterer Doppelpunkt oder ein Strichpunkt
/// beendet dort die Beschreibung (Mermaid 10.9.1 meldet einen Fehler) — auch sie werden Entity-Code.
std::string maskiereZustand(std::string_view text)
{
    // Zeichen fuer Zeichen: das `;` der Entity-Codes, die maskiere() schreibt, bleibt
    std::string aus;
    for (const char c : text)
    {
        if (c == ':')
            aus += "#58;";
        else if (c == ';')
            aus += "#59;";
        else
            aus += maskiere(std::string_view(&c, 1));
    }
    return aus;
}

std::string kommentar(std::string_view titel)
{
    if (titel.empty()) return {};
    // als Kommentar: ein Titel im YAML-Kopf braeuchte eine eigene Maskierung
    std::string aus = "%% ";
    for (const char c : titel) aus += (c == '\n' || c == '\r') ? ' ' : c;
    return aus + '\n';
}

/// Ein Name, den classDiagram als Kennung nimmt: Buchstaben, Ziffern, Unterstrich.
std::string kennung(std::string_view name)
{
    std::string aus;
    for (const char c : name)
    {
        const bool gut = c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        aus += gut ? c : '_';
    }
    return aus.empty() ? "_" : aus;
}

/// Ein Eintrag im Rumpf einer Klasse: geschweifte Klammern wuerden ihn beenden,
/// und spitze schreibt Mermaid als Tilde (`vector~int~`).
std::string eintragText(std::string_view text)
{
    std::string aus;
    for (const char c : text)
    {
        if (c == '{' || c == '}' || c == '\r') continue;
        if (c == '<' || c == '>')
            aus += '~';
        else
            aus += (c == '\n' || c == '\t') ? ' ' : c;
    }
    return aus;
}

std::string mitSicht(char sicht, std::string text)
{
    return sicht == '\0' ? text : sicht + text;
}

std::string_view stereotyp(KastenArt art)
{
    switch (art)
    {
        case KastenArt::Struktur: return "struct";
        case KastenArt::Vereinigung: return "union";
        case KastenArt::Aufzaehlung: return "enumeration";
        case KastenArt::Schnittstelle: return "interface";
        case KastenArt::Tabelle: return "table";
        case KastenArt::Klasse: break;
    }
    return {};
}

class Mermaid final : public Format
{
public:
    [[nodiscard]] std::string_view name() const override { return "mermaid"; }
    [[nodiscard]] std::string_view endung() const override { return "mmd"; }

    [[nodiscard]] std::string schreibe(const Flussdiagramm& diagramm, const Optionen& optionen) const override
    {
        std::string aus = kommentar(diagramm.titel());
        aus += "flowchart ";
        aus += optionen.richtung;
        aus += '\n';

        const std::vector<Knoten>& knoten = diagramm.knoten();
        const auto schreibeKnoten = [&](std::size_t id, const std::string& einzug) {
            const Form form = formVon(knoten[id].art);
            aus += einzug + "n" + std::to_string(id);
            aus += form.auf;
            aus += '"';
            bool erste = true;
            for (const std::string& zeile : knoten[id].zeilen)
            {
                if (!erste) aus += "<br>";
                erste = false;
                aus += maskiere(kuerze(zeile, optionen.maxZeilenLaenge));
            }
            aus += '"';
            aus += form.zu;
            aus += '\n';
        };
        // Erst die Knoten ohne Gruppe, dann je Gruppe ein subgraph — mit dem, was in ihr liegt.
        const auto schreibeEbene = [&](const auto& selbst, std::size_t gruppe, const std::string& einzug) -> void {
            for (std::size_t id = 0; id < knoten.size(); ++id)
                if (knoten[id].gruppe == gruppe) schreibeKnoten(id, einzug);
            for (std::size_t g = 0; g < diagramm.gruppen().size(); ++g)
            {
                if (diagramm.gruppen()[g].eltern != gruppe) continue;
                aus += einzug + "subgraph g" + std::to_string(g) + "[\"" +
                       maskiere(kuerze(diagramm.gruppen()[g].titel, optionen.maxZeilenLaenge)) + "\"]\n";
                selbst(selbst, g, einzug + "    ");
                aus += einzug + "end\n";
            }
        };
        schreibeEbene(schreibeEbene, kKeine, "    ");
        // ein Rahmen einer Bedingung traegt ihre Farbe
        const std::vector<std::size_t> farben = farbenDerRahmen(diagramm);
        for (std::size_t g = 0; g < farben.size(); ++g)
        {
            if (farben[g] == kKeine) continue;
            const std::string f(rahmenFarbe(farben[g]));
            aus += "    style g" + std::to_string(g) + " fill:" + f + ",fill-opacity:0.08,stroke:" + f + ",color:" + f + '\n';
        }
        // nie erreicht: grau und gestrichelt
        std::string tot;
        for (std::size_t id = 0; id < knoten.size(); ++id)
            if (knoten[id].unerreichbar) tot += (tot.empty() ? "" : ",") + ("n" + std::to_string(id));
        if (!tot.empty())
            aus += "    classDef tot fill:" + std::string(c2d::diagramm::farbe(Farbe::Fremd)) + ",stroke:" +
                   std::string(c2d::diagramm::farbe(Farbe::FremdStrich)) + ",stroke-dasharray:5 4\n    class " + tot + " tot\n";
        for (const Kante& k : diagramm.kanten())
        {
            aus += "    n" + std::to_string(k.von) + " -->";
            if (!k.beschriftung.empty()) aus += "|\"" + maskiere(k.beschriftung) + "\"|";
            aus += " n" + std::to_string(k.nach) + '\n';
        }
        return aus;
    }

    [[nodiscard]] std::string schreibe(const Strukturdiagramm& diagramm, const Optionen& optionen) const override
    {
        if (alsErDiagramm(diagramm)) return erDiagramm(erDiagrammVon(diagramm), optionen);
        const bool nurTabellen = std::all_of(diagramm.kaesten.begin(), diagramm.kaesten.end(),
                                             [](const Kasten& k) { return k.art == KastenArt::Tabelle; });
        if (diagramm.notation == ErNotation::Kraehenfuss && nurTabellen && !diagramm.kaesten.empty())
            return kraehenfuss(diagramm, optionen);
        std::string aus = kommentar(diagramm.titel);
        if (diagramm.notation == ErNotation::Kraehenfuss && diagramm.hatTabellen())
            aus += "%% Crow's foot needs erDiagram, which has no classes - multiplicities as in Tables\n";
        aus += "classDiagram\n";
        if (optionen.richtung != "TD")
        {
            aus += "    direction ";
            aus += optionen.richtung;
            aus += '\n';
        }

        for (const Kasten& kasten : diagramm.kaesten)
        {
            const std::string id = kennung(kasten.name);
            // ein Name mit Bereich (`a::B`) taugt nicht als Kennung: er wird die Beschriftung
            if (id != kasten.name) aus += "    class " + id + "[\"" + maskiere(kasten.name) + "\"]\n";
            aus += "    class " + id + " {\n";
            if (const std::string_view s = stereotyp(kasten.art); !s.empty())
            {
                aus += "        <<";
                aus += s;
                aus += ">>\n";
            }
            for (const Eintrag& e : kasten.eintraege)
                aus += "        " + mitSicht(e.sicht, eintragText(kuerze(e.text(), optionen.maxZeilenLaenge))) + '\n';
            for (const Methode& m : kasten.methoden)
            {
                aus += "        " + mitSicht(m.sicht, eintragText(kuerze(m.text(), optionen.maxZeilenLaenge)));
                // hinter der Klammer: * abstrakt, $ statisch
                if (m.abstrakt) aus += '*';
                else if (m.statisch) aus += '$';
                aus += '\n';
            }
            aus += "    }\n";
        }
        for (const Beziehung& b : diagramm.beziehungen)
        {
            const std::string von = kennung(diagramm.kaesten[b.von].name);
            const std::string nach = kennung(diagramm.kaesten[b.nach].name);
            if (b.fremdschluessel)  // die Vielfachheiten an den Enden (Entscheid N1)
            {
                aus += "    " + von + " \"" + uml(b.amVon) + "\" --> \"" + uml(b.amNach) + "\" " + nach;
                if (!b.beschriftung.empty()) aus += " : " + eintragText(b.beschriftung);
                aus += '\n';
                continue;
            }
            switch (b.art)
            {
                case BeziehungArt::Enthaelt: aus += "    " + von + " *-- " + nach; break;
                case BeziehungArt::Verweist: aus += "    " + von + " --> " + nach; break;
                case BeziehungArt::Erbt: aus += "    " + nach + " <|-- " + von; break;
                case BeziehungArt::Realisiert: aus += "    " + nach + " <|.. " + von; break;
            }
            if (!b.beschriftung.empty()) aus += " : " + eintragText(b.beschriftung);
            aus += '\n';
        }
        return aus;
    }

    /// Crow's foot als erDiagram: je Tabelle ihre Spalten (`typ name PK, FK`), je Fremdschluessel
    /// `ziel ||--o{ tabelle : "spalten"` — links das Ende der Tabelle, auf die er zeigt.
    [[nodiscard]] static std::string kraehenfuss(const Strukturdiagramm& d, const Optionen& optionen)
    {
        std::string aus = kommentar(d.titel);
        aus += "erDiagram\n";
        // ein Typ ist ein Wort: Buchstaben, Ziffern, _ - ( ) [ ]; Leerzeichen und Kommas werden _
        const auto typ = [](std::string_view t) {
            std::string a;
            for (std::size_t i = 0; i < t.size(); ++i)
            {
                const char c = t[i];
                if (c == ' ' && i > 0 && t[i - 1] == ',') continue;  // NUMERIC(10, 2) -> NUMERIC(10_2)
                const bool gut = c == '_' || c == '-' || c == '(' || c == ')' || c == '[' || c == ']' || (c >= '0' && c <= '9') ||
                                 (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                a += gut ? c : '_';
            }
            if (a.empty() || !((a.front() >= 'a' && a.front() <= 'z') || (a.front() >= 'A' && a.front() <= 'Z'))) a = "t_" + a;
            return a;
        };
        for (const Kasten& kasten : d.kaesten)
        {
            const std::string id = kennung(kasten.name);
            aus += "    " + id;
            if (id != kasten.name) aus += "[\"" + maskiere(kasten.name) + "\"]";
            aus += " {\n";
            for (const Eintrag& e : kasten.eintraege)
            {
                aus += "        " + typ(kuerze(e.typ, optionen.maxZeilenLaenge)) + " " + kennung(e.name);
                if (e.schluessel && e.fremd) aus += " PK, FK";
                else if (e.schluessel) aus += " PK";
                else if (e.fremd) aus += " FK";
                aus += '\n';
            }
            aus += "    }\n";
        }
        for (const Beziehung& b : d.beziehungen)
        {
            if (!b.fremdschluessel) continue;
            // links (das Ziel): genau eins `||` oder hoechstens eins `|o`; rechts (die Tabelle mit ihm):
            // viele `o{` oder hoechstens eins `o|`
            aus += "    " + kennung(d.kaesten[b.nach].name) + " " + (b.amNach.min >= 1 ? "||" : "|o") + "--" +
                   (b.amVon.max == 1 ? "o|" : "o{") + " " + kennung(d.kaesten[b.von].name) + " : \"" +
                   maskiere(b.beschriftung.empty() ? std::string("references") : b.beschriftung) + "\"\n";
        }
        return aus;
    }

    /// Chen und (min,max) als flowchart: Rechteck, Raute, Ellipsen als Stadion (der Schluessel
    /// unterstrichen); der Text einer Linie steht auf ihr (Mermaid setzt keine Texte an ein Ende).
    [[nodiscard]] static std::string erDiagramm(const ErDiagramm& d, const Optionen& optionen)
    {
        std::string aus = kommentar(d.titel);
        aus += "flowchart ";
        aus += optionen.richtung;
        aus += '\n';
        for (std::size_t v = 0; v < d.knoten.size(); ++v)
        {
            const ErKnoten& k = d.knoten[v];
            const std::string id = "k" + std::to_string(v);
            const std::string name = maskiere(kuerze(k.name, optionen.maxZeilenLaenge));
            switch (k.art)
            {
                case ErKnotenArt::Entitaet: aus += "    " + id + "[\"" + name + "\"]\n"; break;
                case ErKnotenArt::Beziehung: aus += "    " + id + "{\"" + name + "\"}\n"; break;
                case ErKnotenArt::Kasten:
                {
                    aus += "    " + id + "[\"";
                    if (!k.stereotyp.empty()) aus += "#171;" + maskiere(k.stereotyp) + "#187;<br>";
                    aus += "<b>" + name + "</b>";
                    for (const std::string& z : k.zeilen) aus += "<br>" + maskiere(kuerze(z, optionen.maxZeilenLaenge));
                    aus += "\"]\n";
                    break;
                }
            }
            for (std::size_t i = 0; i < k.attribute.size(); ++i)
            {
                const ErAttribut& at = k.attribute[i];
                const std::string text = maskiere(kuerze(at.name, optionen.maxZeilenLaenge));
                aus += "    " + id + "a" + std::to_string(i) + "([\"" + (at.schluessel ? "<u>" + text + "</u>" : text) + "\"])\n";
                aus += "    " + id + "a" + std::to_string(i) + " --- " + id + '\n';
            }
        }
        for (const ErKante& k : d.kanten)
        {
            const std::string von = "k" + std::to_string(k.von);
            const std::string nach = "k" + std::to_string(k.nach);
            if (!k.zwischenKaesten)
            {
                aus += "    " + von + " ---|\"" + maskiere(k.text) + "\"| " + nach + '\n';
                continue;
            }
            const std::string_view pfeil = k.art == BeziehungArt::Realisiert ? "-.->" : "-->";
            aus += "    " + von + " " + std::string(pfeil);
            if (!k.text.empty()) aus += "|\"" + maskiere(k.text) + "\"|";
            aus += " " + nach + '\n';
        }
        return aus;
    }

    [[nodiscard]] std::string schreibe(const Zustandsdiagramm& diagramm, const Optionen& optionen) const override
    {
        std::string aus = kommentar(diagramm.titel);
        aus += "stateDiagram-v2\n";
        if (optionen.richtung != "TD")
        {
            aus += "    direction ";
            aus += optionen.richtung;
            aus += '\n';
        }
        // der Punkt von aussen ist [*]; ein Zustand bekommt eine Kennung und seinen Namen als Text
        const auto id = [&](std::size_t z) {
            return diagramm.zustaende[z].aussen ? std::string("[*]") : "s" + std::to_string(z);
        };
        for (std::size_t z = 0; z < diagramm.zustaende.size(); ++z)
            if (!diagramm.zustaende[z].aussen)
                aus += "    state \"" + maskiereZustand(kuerze(diagramm.zustaende[z].name, optionen.maxZeilenLaenge)) +
                       "\" as " + id(z) + '\n';
        for (const Uebergang& u : diagramm.uebergaenge)
        {
            aus += "    " + id(u.von) + " --> " + id(u.nach);
            if (!u.bedingung.empty()) aus += " : " + maskiereZustand(kuerze(u.bedingung, optionen.maxZeilenLaenge));
            aus += '\n';
        }
        return aus;
    }

    // Bloecke und Tabellen sind kein Graph: hier nicht richtig darstellbar
    [[nodiscard]] bool kann(Diagrammart art) const override
    {
        return art != Diagrammart::Struktogramm && art != Diagrammart::Wahrheitstabelle &&
               art != Diagrammart::Entscheidungstabelle && art != Diagrammart::Zuordnung &&
               art != Diagrammart::Konfiguration && art != Diagrammart::Schaltstellen && art != Diagrammart::Schalteruebersicht;
    }
    [[nodiscard]] std::string schreibe(const Schaltstellen&, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(const Schalteruebersicht&, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(const Struktogramm&, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(std::span<const Wahrheitstabelle>, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(const Entscheidungstabelle&, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(std::span<const Zuordnung>, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(std::span<const Konfigurationstabelle>, const Optionen&) const override { return {}; }

    /// Eine Abfrage als flowchart: je Rahmen ein subgraph (verschachtelt), der Titel eines Knotens fett,
    /// Join `---`, WHERE und korreliert `-.-` (korreliert dazu gepunktet), Fluss `-->`, Schreiben `==>`.
    [[nodiscard]] std::string schreibe(const Abfragediagramm& d, const Optionen& optionen) const override
    {
        std::string aus = kommentar(d.titel);
        aus += "flowchart ";
        aus += optionen.richtung;
        aus += '\n';
        const auto knoten = [&](std::size_t i, const std::string& einzug) {
            const AbfrageKnoten& k = d.knoten[i];
            std::string text = "<b>" + maskiere(kuerze(k.titel, optionen.maxZeilenLaenge)) + "</b>";
            for (const std::string& z : k.zeilen) text += "<br>" + maskiere(kuerze(z, optionen.maxZeilenLaenge));
            std::string_view auf = "[", zu = "]";
            switch (k.art)
            {
                case AbfrageKnotenArt::Ergebnis: auf = "("; zu = ")"; break;
                case AbfrageKnotenArt::Menge: auf = "{{"; zu = "}}"; break;
                case AbfrageKnotenArt::Mit:
                case AbfrageKnotenArt::Abgeleitet: auf = "(["; zu = "])"; break;
                case AbfrageKnotenArt::Funktion: auf = "[["; zu = "]]"; break;
                case AbfrageKnotenArt::Ziel: auf = "[("; zu = ")]"; break;
                default: break;
            }
            aus += einzug + "q" + std::to_string(i) + std::string(auf) + "\"" + text + "\"" + std::string(zu) + "\n";
        };
        const auto ebene = [&](const auto& selbst, std::size_t rahmen, const std::string& einzug) -> void {
            for (std::size_t i = 0; i < d.knoten.size(); ++i)
                if (d.knoten[i].gruppe == rahmen) knoten(i, einzug);
            for (std::size_t r = 0; r < d.rahmen.size(); ++r)
            {
                if (d.rahmen[r].eltern != rahmen) continue;
                aus += einzug + "subgraph r" + std::to_string(r) + "[\"" + maskiere(kuerze(d.rahmen[r].titel, optionen.maxZeilenLaenge)) + "\"]\n";
                selbst(selbst, r, einzug + "    ");
                aus += einzug + "end\n";
            }
        };
        ebene(ebene, kKeine, "    ");
        std::string fremd, view;
        for (std::size_t i = 0; i < d.knoten.size(); ++i)
        {
            if (d.knoten[i].art == AbfrageKnotenArt::Fremd) fremd += (fremd.empty() ? "" : ",") + ("q" + std::to_string(i));
            if (d.knoten[i].art == AbfrageKnotenArt::View || d.knoten[i].art == AbfrageKnotenArt::Abgeleitet)
                view += (view.empty() ? "" : ",") + ("q" + std::to_string(i));
        }
        if (!fremd.empty())
            aus += "    classDef fremd fill:" + std::string(c2d::diagramm::farbe(Farbe::Fremd)) + ",stroke:" +
                   std::string(c2d::diagramm::farbe(Farbe::FremdStrich)) + "\n    class " + fremd + " fremd\n";
        if (!view.empty()) aus += "    classDef gestrichelt stroke-dasharray:6 4\n    class " + view + " gestrichelt\n";
        std::string gepunktet;
        for (std::size_t e = 0; e < d.kanten.size(); ++e)
        {
            const AbfrageKante& k = d.kanten[e];
            std::string_view pfeil = "---";
            switch (k.art)
            {
                case AbfrageKantenArt::Join: pfeil = "---"; break;
                case AbfrageKantenArt::Wo:
                case AbfrageKantenArt::Korreliert: pfeil = "-.-"; break;
                case AbfrageKantenArt::Fluss: pfeil = "-->"; break;
                case AbfrageKantenArt::Schreibt: pfeil = "==>"; break;
            }
            if (k.art == AbfrageKantenArt::Korreliert) gepunktet += (gepunktet.empty() ? "" : ",") + std::to_string(e);
            aus += "    q" + std::to_string(k.von) + " " + std::string(pfeil);
            if (!k.text.empty()) aus += "|\"" + maskiere(kuerze(k.text, optionen.maxZeilenLaenge)) + "\"|";
            aus += " q" + std::to_string(k.nach) + '\n';
        }
        if (!gepunktet.empty()) aus += "    linkStyle " + gepunktet + " stroke-dasharray:2 3\n";
        return aus;
    }

    /// Ein Datenfluss als flowchart: je Rahmen (Stage, Funktion) ein subgraph; Quelle Zylinder, Shuffle Sechseck,
    /// Aktion rund, spark.sql und Aufruf mit doppelten Seiten; Stereotyp kursiv, Titel fett; Fluss `-->`, Pfad `-.->`.
    [[nodiscard]] std::string schreibe(const Datenflussdiagramm& d, const Optionen& optionen) const override
    {
        std::string aus = kommentar(d.titel);
        aus += "flowchart ";
        aus += optionen.richtung;
        aus += '\n';
        const auto knoten = [&](std::size_t i, const std::string& einzug) {
            const DatenflussKnoten& k = d.knoten[i];
            std::string text;
            const auto dazu = [&](const std::string& t) { text += (text.empty() ? "" : "<br>") + t; };
            if (const std::string_view s = stereotyp(k.art); !s.empty()) dazu("<i>" + maskiere(std::string(s)) + "</i>");
            if (!k.titel.empty()) dazu("<b>" + maskiere(kuerze(k.titel, optionen.maxZeilenLaenge)) + "</b>");
            for (const std::string& z : k.zeilen) dazu(maskiere(kuerze(z, optionen.maxZeilenLaenge)));
            std::string_view auf = "[", zu = "]";
            switch (k.art)
            {
                case DatenflussKnotenArt::Quelle: auf = "[("; zu = ")]"; break;
                case DatenflussKnotenArt::Shuffle: auf = "{{"; zu = "}}"; break;
                case DatenflussKnotenArt::Aktion: auf = "("; zu = ")"; break;
                case DatenflussKnotenArt::Sql:
                case DatenflussKnotenArt::Aufruf: auf = "[["; zu = "]]"; break;
                case DatenflussKnotenArt::Sitzung:
                case DatenflussKnotenArt::Schritte: break;
            }
            aus += einzug + "d" + std::to_string(i) + std::string(auf) + "\"" + text + "\"" + std::string(zu) + "\n";
        };
        const auto ebene = [&](const auto& selbst, std::size_t rahmen, const std::string& einzug) -> void {
            for (std::size_t i = 0; i < d.knoten.size(); ++i)
                if (d.knoten[i].gruppe == rahmen) knoten(i, einzug);
            for (std::size_t r = 0; r < d.rahmen.size(); ++r)
            {
                if (d.rahmen[r].eltern != rahmen) continue;
                aus += einzug + "subgraph r" + std::to_string(r) + "[\"" + maskiere(kuerze(d.rahmen[r].titel, optionen.maxZeilenLaenge)) + "\"]\n";
                selbst(selbst, r, einzug + "    ");
                aus += einzug + "end\n";
            }
        };
        ebene(ebene, kKeine, "    ");
        std::string cache, kopf;
        for (std::size_t i = 0; i < d.knoten.size(); ++i)
        {
            if (d.knoten[i].zwischengespeichert) cache += (cache.empty() ? "" : ",") + ("d" + std::to_string(i));
            if (d.knoten[i].art == DatenflussKnotenArt::Sitzung) kopf += (kopf.empty() ? "" : ",") + ("d" + std::to_string(i));
        }
        if (!cache.empty()) aus += "    classDef cache stroke-width:3px\n    class " + cache + " cache\n";
        if (!kopf.empty()) aus += "    classDef kopf stroke-dasharray:6 4\n    class " + kopf + " kopf\n";
        for (const DatenflussKante& k : d.kanten)
        {
            aus += "    d" + std::to_string(k.von) + (k.art == DatenflussKantenArt::Pfad ? " -.->" : " -->");
            if (!k.text.empty()) aus += "|\"" + maskiere(kuerze(k.text, optionen.maxZeilenLaenge)) + "\"|";
            aus += " d" + std::to_string(k.nach) + '\n';
        }
        return aus;
    }

    [[nodiscard]] std::string schreibe(const Aufrufgraph& graph, const Optionen& optionen) const override
    {
        std::string aus = kommentar(graph.titel);
        aus += "flowchart ";
        aus += optionen.richtung;
        aus += '\n';
        // eine Funktion als Unterprogramm, Fremd und Makro als Rechteck; mit Rahmen je Datei ein subgraph
        // die Abhaengigkeiten: eine Tabelle als Zylinder, ein Trigger als Sechseck, eine View mit runden Enden,
        // eine Routine wie eine Funktion; der Stereotyp in einer eigenen Zeile
        const auto schreibeKnoten = [&](std::size_t id, std::string_view einzug) {
            const AufrufKnoten& k = graph.knoten[id];
            std::string_view auf = "[\"", zu = "\"]";
            switch (k.art)
            {
                case AufrufKnotenArt::Funktion:
                case AufrufKnotenArt::Routine: auf = "[[\""; zu = "\"]]"; break;
                case AufrufKnotenArt::Tabelle:
                case AufrufKnotenArt::FremdeTabelle: auf = "[(\""; zu = "\")]"; break;
                case AufrufKnotenArt::View: auf = "([\""; zu = "\"])"; break;
                case AufrufKnotenArt::Trigger: auf = "{{\""; zu = "\"}}"; break;
                case AufrufKnotenArt::Fremd:
                case AufrufKnotenArt::Makro: break;
            }
            std::string text;
            for (const std::string& zeile : knotenZeilen(k))
                text += (text.empty() ? "" : "<br/>") + maskiere(kuerze(zeile, optionen.maxZeilenLaenge));
            aus += std::string(einzug) + "n" + std::to_string(id) + std::string(auf) + text + std::string(zu) + "\n";
        };
        if (graph.mitRahmen())
            for (std::size_t d = 0; d < graph.dateien.size(); ++d)
            {
                aus += "    subgraph d" + std::to_string(d) + "[\"" + maskiere(kuerze(graph.dateien[d], optionen.maxZeilenLaenge)) +
                       "\"]\n";
                for (std::size_t id = 0; id < graph.knoten.size(); ++id)
                    if (imProjekt(graph.knoten[id].art) && graph.knoten[id].datei == d)
                        schreibeKnoten(id, "        ");
                aus += "    end\n";
            }
        for (std::size_t id = 0; id < graph.knoten.size(); ++id)
            if (!graph.mitRahmen() || !imProjekt(graph.knoten[id].art)) schreibeKnoten(id, "    ");
        // mehrdeutig und aus einem Zweig, der nicht gilt: gestrichelt — die Beschriftung sagt, welches
        for (const AufrufKante& k : graph.kanten)
        {
            const bool gestrichelt = !k.gilt || k.art == AufrufKantenArt::Mehrdeutig;
            aus += "    n" + std::to_string(k.von) + (gestrichelt ? " -.->" : " -->");
            if (const std::string text = beschriftung(k); !text.empty())
                aus += "|\"" + maskiere(kuerze(text, optionen.maxZeilenLaenge)) + "\"|";
            aus += " n" + std::to_string(k.nach) + '\n';
        }
        // eine Bedingung, die gilt, in ihrer Farbe
        const std::vector<std::size_t> farben = farbenDerBedingungen(graph);
        for (std::size_t e = 0; e < farben.size(); ++e)
            if (farben[e] != kKeine)
                aus += "    linkStyle " + std::to_string(e) + " stroke:" + std::string(rahmenFarbe(farben[e])) + '\n';
        // die hellen Farben der Tabelle: Mermaid kennt keine dunkle Fassung
        const auto hell = [](Farbe welche) { return std::string(c2d::diagramm::farbe(welche)); };
        const std::string fremd = "fill:" + hell(Farbe::Fremd) + ",stroke:" + hell(Farbe::FremdStrich);
        aus += "    classDef fremd " + fremd + ",font-style:italic\n"
               "    classDef makro " + fremd + ",stroke-dasharray:4 3,font-style:italic\n"
               "    classDef wurzel fill:" + hell(Farbe::Getoent) + ",stroke-width:2.4px\n";
        for (std::size_t id = 0; id < graph.knoten.size(); ++id)
        {
            const AufrufKnoten& k = graph.knoten[id];
            if (k.art == AufrufKnotenArt::Fremd || k.art == AufrufKnotenArt::FremdeTabelle)
                aus += "    class n" + std::to_string(id) + " fremd\n";
            else if (k.art == AufrufKnotenArt::Makro) aus += "    class n" + std::to_string(id) + " makro\n";
            else if (k.wurzel) aus += "    class n" + std::to_string(id) + " wurzel\n";
        }
        return aus;
    }
};

}  // namespace

const Format& mermaid()
{
    static const Mermaid format;
    return format;
}

}  // namespace c2d::ausgabe
