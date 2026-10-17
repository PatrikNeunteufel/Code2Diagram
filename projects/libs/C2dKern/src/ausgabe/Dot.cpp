/**
 * @file Dot.cpp
 * @brief Graphviz DOT: digraph, Datentypen als record, Zustaende als runde Kaesten
 */
#include "c2d/ausgabe/Ausgabe.hpp"

#include "Text.hpp"
#include "c2d/diagramm/Farben.hpp"

namespace c2d::ausgabe
{

namespace
{

using namespace c2d::diagramm;

/// Knotenart -> Attribute des Knotens.
constexpr std::string_view formVon(KnotenArt art)
{
    switch (art)
    {
        case KnotenArt::Start:
        case KnotenArt::Ende:
            return "shape=box, style=rounded";
        case KnotenArt::Verzweigung:
            return "shape=diamond";
        case KnotenArt::Aufruf:
            return "shape=box, peripheries=2";
        case KnotenArt::Variante:
            return "shape=hexagon";
        case KnotenArt::Anweisung:
            break;
    }
    return "shape=box";
}

/// Text in "...". Mit `record` auch die Zeichen, die in einem record gliedern.
std::string maskiere(std::string_view text, bool record = false)
{
    std::string aus;
    aus.reserve(text.size());
    for (const char c : text)
    {
        if (c == '\r') continue;
        if (c == '\n' || c == '\t')
        {
            aus += ' ';
            continue;
        }
        const bool gliedert = record && (c == '{' || c == '}' || c == '|' || c == '<' || c == '>');
        if (c == '"' || c == '\\' || gliedert) aus += '\\';
        aus += c;
    }
    return aus;
}

std::string kopf(std::string_view titel, const Optionen& optionen)
{
    std::string aus = "digraph \"" + maskiere(titel) + "\" {\n";
    aus += "    rankdir=";
    aus += optionen.richtung == "TD" ? std::string_view("TB") : optionen.richtung;
    aus += ";\n"
           "    node [fontname=\"Helvetica\", fontsize=10];\n"
           "    edge [fontname=\"Helvetica\", fontsize=9];\n";
    return aus;
}

std::string_view stereotyp(KastenArt art)
{
    switch (art)
    {
        case KastenArt::Struktur: return "struct";
        case KastenArt::Vereinigung: return "union";
        case KastenArt::Aufzaehlung: return "enum";
        case KastenArt::Klasse: break;
    }
    return "class";
}

class Dot final : public Format
{
public:
    [[nodiscard]] std::string_view name() const override { return "dot"; }
    [[nodiscard]] std::string_view endung() const override { return "dot"; }

    [[nodiscard]] std::string schreibe(const Flussdiagramm& diagramm, const Optionen& optionen) const override
    {
        std::string aus = kopf(diagramm.titel(), optionen);
        const std::vector<Knoten>& knoten = diagramm.knoten();
        const auto schreibeKnoten = [&](std::size_t id, const std::string& einzug) {
            aus += einzug + "n" + std::to_string(id) + " [";
            aus += formVon(knoten[id].art);
            if (knoten[id].unerreichbar)  // nie erreicht: grau und gestrichelt
                aus += ", style=\"dashed,filled\", fillcolor=\"" + std::string(c2d::diagramm::farbe(Farbe::Fremd)) + "\", color=\"" +
                       std::string(c2d::diagramm::farbe(Farbe::FremdStrich)) + "\"";
            aus += ", label=\"";
            bool erste = true;
            for (const std::string& zeile : knoten[id].zeilen)
            {
                if (!erste) aus += "\\n";
                erste = false;
                aus += maskiere(kuerze(zeile, optionen.maxZeilenLaenge));
            }
            aus += "\"];\n";
        };
        // Erst die Knoten ohne Gruppe, dann je Gruppe ein cluster — mit dem, was in ihr liegt.
        // Ein Rahmen einer Bedingung traegt ihre Farbe; die Flaeche nur als Hauch (#rrggbb + Alpha).
        const std::vector<std::size_t> farben = farbenDerRahmen(diagramm);
        const auto schreibeEbene = [&](const auto& selbst, std::size_t gruppe, const std::string& einzug) -> void {
            for (std::size_t id = 0; id < knoten.size(); ++id)
                if (knoten[id].gruppe == gruppe) schreibeKnoten(id, einzug);
            for (std::size_t g = 0; g < diagramm.gruppen().size(); ++g)
            {
                if (diagramm.gruppen()[g].eltern != gruppe) continue;
                aus += einzug + "subgraph cluster_" + std::to_string(g) + " {\n";
                if (farben[g] == kKeine)
                    aus += einzug + "    style=dashed;";
                else
                {
                    const std::string f(rahmenFarbe(farben[g]));
                    aus += einzug + "    style=filled; color=\"" + f + "\"; fillcolor=\"" + f + "14\"; fontcolor=\"" + f + "\";";
                }
                aus += " label=\"" + maskiere(kuerze(diagramm.gruppen()[g].titel, optionen.maxZeilenLaenge)) + "\";\n";
                selbst(selbst, g, einzug + "    ");
                aus += einzug + "}\n";
            }
        };
        schreibeEbene(schreibeEbene, kKeine, "    ");
        for (const Kante& k : diagramm.kanten())
        {
            aus += "    n" + std::to_string(k.von) + " -> n" + std::to_string(k.nach);
            if (!k.beschriftung.empty()) aus += " [label=\"" + maskiere(k.beschriftung) + "\"]";
            aus += ";\n";
        }
        return aus + "}\n";
    }

    [[nodiscard]] std::string schreibe(const Strukturdiagramm& diagramm, const Optionen& optionen) const override
    {
        std::string aus = kopf(diagramm.titel, optionen);
        for (std::size_t id = 0; id < diagramm.kaesten.size(); ++id)
        {
            const Kasten& kasten = diagramm.kaesten[id];
            aus += "    k" + std::to_string(id) + " [shape=record, label=\"{";
            aus += stereotyp(kasten.art);
            aus += ' ';
            aus += maskiere(kasten.name, true);
            aus += '|';
            const auto zeile = [&](char sicht, const std::string& text) {
                if (sicht != '\0') aus += std::string(1, sicht) + ' ';
                aus += maskiere(kuerze(text, optionen.maxZeilenLaenge), true) + "\\l";
            };
            for (const Eintrag& e : kasten.eintraege) zeile(e.sicht, e.text());
            if (!kasten.methoden.empty()) aus += '|';
            for (const Methode& m : kasten.methoden) zeile(m.sicht, m.text());
            aus += "}\"];\n";
        }
        for (const Beziehung& b : diagramm.beziehungen)
        {
            aus += "    k" + std::to_string(b.von) + " -> k" + std::to_string(b.nach) + " [";
            switch (b.art)
            {
                case BeziehungArt::Enthaelt: aus += "dir=back, arrowtail=diamond"; break;
                case BeziehungArt::Verweist: aus += "arrowhead=vee"; break;
                case BeziehungArt::Erbt: aus += "arrowhead=empty"; break;
            }
            if (!b.beschriftung.empty()) aus += ", label=\"" + maskiere(b.beschriftung) + "\"";
            aus += "];\n";
        }
        return aus + "}\n";
    }

    [[nodiscard]] std::string schreibe(const Zustandsdiagramm& diagramm, const Optionen& optionen) const override
    {
        std::string aus = kopf(diagramm.titel, optionen);
        for (std::size_t z = 0; z < diagramm.zustaende.size(); ++z)
        {
            const Zustand& zustand = diagramm.zustaende[z];
            aus += "    s" + std::to_string(z);
            if (zustand.aussen)
                aus += " [shape=point, width=0.15];\n";  // der Punkt von aussen
            else
                aus += " [shape=box, style=rounded, label=\"" + maskiere(kuerze(zustand.name, optionen.maxZeilenLaenge)) +
                       "\"];\n";
        }
        for (const Uebergang& u : diagramm.uebergaenge)
        {
            aus += "    s" + std::to_string(u.von) + " -> s" + std::to_string(u.nach);
            if (!u.bedingung.empty())
                aus += " [label=\"" + maskiere(kuerze(u.bedingung, optionen.maxZeilenLaenge)) + "\"]";
            aus += ";\n";
        }
        return aus + "}\n";
    }

    // Bloecke und Tabellen sind kein Graph: hier nicht richtig darstellbar
    [[nodiscard]] bool kann(Diagrammart art) const override
    {
        return art != Diagrammart::Struktogramm && art != Diagrammart::Wahrheitstabelle;
    }
    [[nodiscard]] std::string schreibe(const Struktogramm&, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(std::span<const Wahrheitstabelle>, const Optionen&) const override { return {}; }

    [[nodiscard]] std::string schreibe(const Aufrufgraph& graph, const Optionen& optionen) const override
    {
        std::string aus = kopf(graph.titel, optionen);
        // eine Funktion mit doppeltem Rand, Fremd grau, ein Makro dazu gestrichelt; mit Rahmen je Datei ein cluster
        const auto schreibeKnoten = [&](std::size_t id, std::string_view einzug) {
            const AufrufKnoten& k = graph.knoten[id];
            // die hellen Farben der Tabelle: DOT kennt keine dunkle Fassung
            const auto hell = [](Farbe welche) { return std::string(c2d::diagramm::farbe(welche)); };
            const std::string fremd = "fillcolor=\"" + hell(Farbe::Fremd) + "\", color=\"" + hell(Farbe::FremdStrich) + "\"";
            aus += std::string(einzug) + "n" + std::to_string(id) + " [";
            switch (k.art)
            {
                case AufrufKnotenArt::Funktion:
                    aus += k.wurzel ? "shape=box, peripheries=2, style=\"filled,bold\", fillcolor=\"" + hell(Farbe::Getoent) + "\""
                                    : std::string("shape=box, peripheries=2");
                    break;
                case AufrufKnotenArt::Fremd:
                    aus += "shape=box, style=filled, " + fremd + ", fontname=\"Helvetica-Oblique\"";
                    break;
                case AufrufKnotenArt::Makro:
                    aus += "shape=box, style=\"filled,dashed\", " + fremd + ", fontname=\"Helvetica-Oblique\"";
                    break;
            }
            aus += ", label=\"" + maskiere(kuerze(knotenText(k), optionen.maxZeilenLaenge)) + "\"];\n";
        };
        if (graph.mitRahmen())
            for (std::size_t d = 0; d < graph.dateien.size(); ++d)
            {
                aus += "    subgraph cluster_" + std::to_string(d) + " {\n        style=dashed; label=\"" +
                       maskiere(kuerze(graph.dateien[d], optionen.maxZeilenLaenge)) + "\";\n";
                for (std::size_t id = 0; id < graph.knoten.size(); ++id)
                    if (graph.knoten[id].art == AufrufKnotenArt::Funktion && graph.knoten[id].datei == d)
                        schreibeKnoten(id, "        ");
                aus += "    }\n";
            }
        for (std::size_t id = 0; id < graph.knoten.size(); ++id)
            if (!graph.mitRahmen() || graph.knoten[id].art != AufrufKnotenArt::Funktion) schreibeKnoten(id, "    ");
        // mehrdeutig gestrichelt, aus einem Zweig, der nicht gilt, gepunktet; eine Bedingung, die gilt, in ihrer Farbe
        const std::vector<std::size_t> farben = farbenDerBedingungen(graph);
        for (std::size_t e = 0; e < graph.kanten.size(); ++e)
        {
            const AufrufKante& k = graph.kanten[e];
            std::vector<std::string> attribute;
            if (!k.gilt) attribute.emplace_back("style=dotted");
            else if (k.art == AufrufKantenArt::Mehrdeutig) attribute.emplace_back("style=dashed");
            if (farben[e] != kKeine)
            {
                const std::string f(rahmenFarbe(farben[e]));
                attribute.push_back("color=\"" + f + "\"");
            }
            if (const std::string text = beschriftung(k); !text.empty())
                attribute.push_back("label=\"" + maskiere(kuerze(text, optionen.maxZeilenLaenge)) + "\"");
            aus += "    n" + std::to_string(k.von) + " -> n" + std::to_string(k.nach);
            for (std::size_t i = 0; i < attribute.size(); ++i) aus += (i == 0 ? " [" : ", ") + attribute[i];
            aus += attribute.empty() ? ";\n" : "];\n";
        }
        return aus + "}\n";
    }
};

}  // namespace

const Format& dot()
{
    static const Dot format;
    return format;
}

}  // namespace c2d::ausgabe
