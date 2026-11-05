/**
 * @file Dot.cpp
 * @brief Graphviz DOT: digraph, Datentypen als record, Zustaende als runde Kaesten
 */
#include "c2d/ausgabe/Ausgabe.hpp"

#include "Text.hpp"
#include "c2d/diagramm/ErDiagramm.hpp"
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
        case KastenArt::Schnittstelle: return "interface";
        case KastenArt::Tabelle: return "table";
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
        if (alsErDiagramm(diagramm)) return erDiagramm(erDiagrammVon(diagramm), optionen);
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
            if (b.fremdschluessel && diagramm.notation == ErNotation::Kraehenfuss)
            {
                // die erste Form steht am Knoten: viele = crow, eins = tee; aussen null = odot, eins = tee
                aus += "dir=both, arrowtail=";
                aus += b.amVon.max == 1 ? "tee" : "crow";
                aus += b.amVon.min >= 1 ? "tee" : "odot";
                aus += ", arrowhead=";
                aus += b.amNach.max == 1 ? "tee" : "crow";
                aus += b.amNach.min >= 1 ? "tee" : "odot";
            }
            else
                aus += pfeil(b.art);
            if (b.fremdschluessel && diagramm.notation == ErNotation::Tabellen)
                aus += ", taillabel=\"" + uml(b.amVon) + "\", headlabel=\"" + uml(b.amNach) + "\"";
            if (!b.beschriftung.empty()) aus += ", label=\"" + maskiere(b.beschriftung) + "\"";
            aus += "];\n";
        }
        return aus + "}\n";
    }

    [[nodiscard]] static std::string_view pfeil(BeziehungArt art)
    {
        switch (art)
        {
            case BeziehungArt::Enthaelt: return "dir=back, arrowtail=diamond";
            case BeziehungArt::Verweist: return "arrowhead=vee";
            case BeziehungArt::Erbt: return "arrowhead=empty";
            case BeziehungArt::Realisiert: return "arrowhead=empty, style=dashed";
        }
        return {};
    }

    /// Chen und (min,max): Rechtecke, Rauten, Ellipsen (der Schluessel unterstrichen, als HTML-Label);
    /// der Text einer Linie an ihrem Ende bei der Entitaet (taillabel / headlabel).
    [[nodiscard]] static std::string erDiagramm(const ErDiagramm& d, const Optionen& optionen)
    {
        std::string aus = kopf(d.titel, optionen);
        const auto html = [](std::string_view text) {
            std::string a;
            for (const char c : text)
            {
                switch (c)
                {
                    case '<': a += "&lt;"; break;
                    case '>': a += "&gt;"; break;
                    case '&': a += "&amp;"; break;
                    case '"': a += "&quot;"; break;
                    default: a += c; break;
                }
            }
            return a;
        };
        for (std::size_t v = 0; v < d.knoten.size(); ++v)
        {
            const ErKnoten& k = d.knoten[v];
            const std::string id = "k" + std::to_string(v);
            const std::string name = maskiere(kuerze(k.name, optionen.maxZeilenLaenge));
            switch (k.art)
            {
                case ErKnotenArt::Entitaet: aus += "    " + id + " [shape=box, label=\"" + name + "\"];\n"; break;
                case ErKnotenArt::Beziehung: aus += "    " + id + " [shape=diamond, label=\"" + name + "\"];\n"; break;
                case ErKnotenArt::Kasten:
                {
                    aus += "    " + id + " [shape=record, label=\"{";
                    if (!k.stereotyp.empty()) aus += maskiere(k.stereotyp, true) + ' ';
                    aus += maskiere(k.name, true) + '|';
                    for (std::size_t i = 0; i < k.zeilen.size(); ++i)
                    {
                        if (i == k.felder) aus += '|';
                        aus += maskiere(kuerze(k.zeilen[i], optionen.maxZeilenLaenge), true) + "\\l";
                    }
                    aus += "}\"];\n";
                    break;
                }
            }
            for (std::size_t i = 0; i < k.attribute.size(); ++i)
            {
                const ErAttribut& at = k.attribute[i];
                const std::string text = html(kuerze(at.name, optionen.maxZeilenLaenge));
                aus += "    " + id + "a" + std::to_string(i) + " [shape=ellipse, label=<" +
                       (at.schluessel ? "<U>" + text + "</U>" : text) + ">];\n";
                aus += "    " + id + "a" + std::to_string(i) + " -> " + id + " [dir=none];\n";
            }
        }
        for (const ErKante& k : d.kanten)
        {
            aus += "    k" + std::to_string(k.von) + " -> k" + std::to_string(k.nach) + " [";
            if (k.zwischenKaesten)
            {
                aus += pfeil(k.art);
                if (!k.text.empty()) aus += ", label=\"" + maskiere(k.text) + "\"";
            }
            else
                aus += std::string("dir=none, ") + (k.textAmVon ? "taillabel" : "headlabel") + "=\"" + maskiere(k.text) + "\"";
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

    /// Eine Abfrage: je Rahmen ein cluster (verschachtelt), Knoten mit HTML-Label (der Titel fett, die
    /// Zeilen links), Join ohne Spitze, WHERE gestrichelt, korreliert gepunktet, Fluss mit Spitze.
    [[nodiscard]] std::string schreibe(const Abfragediagramm& d, const Optionen& optionen) const override
    {
        std::string aus = kopf(d.titel, optionen);
        const auto html = [](std::string_view text) {
            std::string a;
            for (const char c : text)
            {
                switch (c)
                {
                    case '<': a += "&lt;"; break;
                    case '>': a += "&gt;"; break;
                    case '&': a += "&amp;"; break;
                    case '"': a += "&quot;"; break;
                    case '\n':
                    case '\r': a += ' '; break;
                    default: a += c; break;
                }
            }
            return a;
        };
        const auto knoten = [&](std::size_t i, const std::string& einzug) {
            const AbfrageKnoten& k = d.knoten[i];
            std::string label = "<B>" + html(kuerze(k.titel, optionen.maxZeilenLaenge)) + "</B>";
            for (const std::string& z : k.zeilen) label += "<BR ALIGN=\"LEFT\"/>" + html(kuerze(z, optionen.maxZeilenLaenge));
            if (!k.zeilen.empty()) label += "<BR ALIGN=\"LEFT\"/>";
            std::string form = "shape=box";
            switch (k.art)
            {
                case AbfrageKnotenArt::Ergebnis: form = "shape=box, style=\"rounded,filled\", fillcolor=\"" + std::string(c2d::diagramm::farbe(Farbe::Getoent)) + "\""; break;
                case AbfrageKnotenArt::Menge: form = "shape=hexagon"; break;
                case AbfrageKnotenArt::View: form = "shape=box, style=dashed"; break;
                case AbfrageKnotenArt::Mit: form = "shape=box, style=rounded"; break;
                case AbfrageKnotenArt::Abgeleitet: form = "shape=box, style=\"rounded,dashed\""; break;
                case AbfrageKnotenArt::Funktion: form = "shape=box, peripheries=2"; break;
                case AbfrageKnotenArt::Ziel: form = "shape=box, penwidth=2"; break;
                case AbfrageKnotenArt::Fremd:
                    form = "shape=box, style=filled, fillcolor=\"" + std::string(c2d::diagramm::farbe(Farbe::Fremd)) + "\", color=\"" +
                           std::string(c2d::diagramm::farbe(Farbe::FremdStrich)) + "\"";
                    break;
                default: break;
            }
            aus += einzug + "q" + std::to_string(i) + " [" + form + ", label=<" + label + ">];\n";
        };
        const auto ebene = [&](const auto& selbst, std::size_t rahmen, const std::string& einzug) -> void {
            for (std::size_t i = 0; i < d.knoten.size(); ++i)
                if (d.knoten[i].gruppe == rahmen) knoten(i, einzug);
            for (std::size_t r = 0; r < d.rahmen.size(); ++r)
            {
                if (d.rahmen[r].eltern != rahmen) continue;
                aus += einzug + "subgraph cluster_" + std::to_string(r) + " {\n" + einzug + "    style=dashed; label=\"" +
                       maskiere(kuerze(d.rahmen[r].titel, optionen.maxZeilenLaenge)) + "\";\n";
                selbst(selbst, r, einzug + "    ");
                aus += einzug + "}\n";
            }
        };
        ebene(ebene, kKeine, "    ");
        for (const AbfrageKante& k : d.kanten)
        {
            aus += "    q" + std::to_string(k.von) + " -> q" + std::to_string(k.nach) + " [";
            switch (k.art)
            {
                case AbfrageKantenArt::Join: aus += "dir=none"; break;
                case AbfrageKantenArt::Wo: aus += "dir=none, style=dashed"; break;
                case AbfrageKantenArt::Korreliert: aus += "dir=none, style=dotted"; break;
                case AbfrageKantenArt::Fluss: aus += "arrowhead=normal"; break;
                case AbfrageKantenArt::Schreibt: aus += "arrowhead=normal, penwidth=2"; break;
            }
            if (!k.text.empty()) aus += ", label=\"" + maskiere(kuerze(k.text, optionen.maxZeilenLaenge)) + "\"";
            aus += "];\n";
        }
        return aus + "}\n";
    }

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
                // die Abhaengigkeiten: Tabelle ein Kasten, View gestrichelt, Trigger rund, Routine wie eine Funktion
                case AufrufKnotenArt::Tabelle: aus += k.wurzel ? "shape=box, style=\"filled,bold\", fillcolor=\"" + hell(Farbe::Getoent) + "\"" : std::string("shape=box"); break;
                case AufrufKnotenArt::FremdeTabelle:
                    aus += "shape=box, style=filled, " + fremd + ", fontname=\"Helvetica-Oblique\"";
                    break;
                case AufrufKnotenArt::View: aus += std::string("shape=box, style=") + (k.wurzel ? "\"dashed,bold\"" : "dashed"); break;
                case AufrufKnotenArt::Trigger: aus += std::string("shape=box, style=") + (k.wurzel ? "\"rounded,bold\"" : "rounded"); break;
                case AufrufKnotenArt::Routine: aus += std::string("shape=box, peripheries=2") + (k.wurzel ? ", style=bold" : ""); break;
            }
            std::string label;
            for (const std::string& zeile : knotenZeilen(k))
                label += (label.empty() ? "" : "\\n") + maskiere(kuerze(zeile, optionen.maxZeilenLaenge));
            aus += ", label=\"" + label + "\"];\n";
        };
        if (graph.mitRahmen())
            for (std::size_t d = 0; d < graph.dateien.size(); ++d)
            {
                aus += "    subgraph cluster_" + std::to_string(d) + " {\n        style=dashed; label=\"" +
                       maskiere(kuerze(graph.dateien[d], optionen.maxZeilenLaenge)) + "\";\n";
                for (std::size_t id = 0; id < graph.knoten.size(); ++id)
                    if (imProjekt(graph.knoten[id].art) && graph.knoten[id].datei == d)
                        schreibeKnoten(id, "        ");
                aus += "    }\n";
            }
        for (std::size_t id = 0; id < graph.knoten.size(); ++id)
            if (!graph.mitRahmen() || !imProjekt(graph.knoten[id].art)) schreibeKnoten(id, "    ");
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
