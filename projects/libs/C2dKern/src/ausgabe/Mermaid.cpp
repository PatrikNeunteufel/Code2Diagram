/**
 * @file Mermaid.cpp
 * @brief Mermaid: flowchart, classDiagram und stateDiagram-v2
 *
 * Welche Form ein Knoten bekommt, steht als Tabelle hier — im Code, nicht in
 * einer Konfigurationsdatei (Lehre 5, docs/Ernte_Vorgaenger.md).
 */
#include "c2d/ausgabe/Ausgabe.hpp"

#include "Text.hpp"

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
        std::string aus = kommentar(diagramm.titel);
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
            switch (b.art)
            {
                case BeziehungArt::Enthaelt: aus += "    " + von + " *-- " + nach; break;
                case BeziehungArt::Verweist: aus += "    " + von + " --> " + nach; break;
                case BeziehungArt::Erbt: aus += "    " + nach + " <|-- " + von; break;
            }
            if (!b.beschriftung.empty()) aus += " : " + eintragText(b.beschriftung);
            aus += '\n';
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
};

}  // namespace

const Format& mermaid()
{
    static const Mermaid format;
    return format;
}

}  // namespace c2d::ausgabe
