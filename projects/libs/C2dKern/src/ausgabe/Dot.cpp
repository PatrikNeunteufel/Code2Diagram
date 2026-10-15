/**
 * @file Dot.cpp
 * @brief Graphviz DOT: digraph, Datentypen als record, Zustaende als runde Kaesten
 */
#include "c2d/ausgabe/Ausgabe.hpp"

#include "Text.hpp"

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
        const auto schreibeEbene = [&](const auto& selbst, std::size_t gruppe, const std::string& einzug) -> void {
            for (std::size_t id = 0; id < knoten.size(); ++id)
                if (knoten[id].gruppe == gruppe) schreibeKnoten(id, einzug);
            for (std::size_t g = 0; g < diagramm.gruppen().size(); ++g)
            {
                if (diagramm.gruppen()[g].eltern != gruppe) continue;
                aus += einzug + "subgraph cluster_" + std::to_string(g) + " {\n";
                aus += einzug + "    style=dashed; label=\"" +
                       maskiere(kuerze(diagramm.gruppen()[g].titel, optionen.maxZeilenLaenge)) + "\";\n";
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
};

}  // namespace

const Format& dot()
{
    static const Dot format;
    return format;
}

}  // namespace c2d::ausgabe
