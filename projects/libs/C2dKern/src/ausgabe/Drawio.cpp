/**
 * @file Drawio.cpp
 * @brief draw.io (diagrams.net): mxGraph-XML mit festen Orten
 *
 * draw.io ordnet nicht selbst an: die Orte kommen aus diagramm/Anordnung.hpp.
 * Die Kanten tragen nur Quelle und Ziel — draw.io fuehrt sie selbst und haelt
 * sie beim Verschieben an ihren Knoten. Eine Gruppe ist ein Behaelter: wer
 * ihn verschiebt, verschiebt, was in ihm liegt.
 */
#include "c2d/ausgabe/Ausgabe.hpp"

#include "Text.hpp"
#include "c2d/diagramm/Farben.hpp"
#include "c2d/diagramm/Anordnung.hpp"

#include <cmath>

namespace c2d::ausgabe
{

namespace
{

using namespace c2d::diagramm;

constexpr std::string_view kSchrift = "fontFamily=Courier New;fontSize=12;";

/// Text als HTML (die Zellen stehen auf html=1).
std::string html(std::string_view text)
{
    std::string aus;
    for (const char c : text)
    {
        switch (c)
        {
            case '&': aus += "&amp;"; break;
            case '<': aus += "&lt;"; break;
            case '>': aus += "&gt;"; break;
            case '\r': break;
            case '\n':
            case '\t': aus += ' '; break;
            default: aus += c; break;
        }
    }
    return aus;
}

std::string zahl(double wert)
{
    return std::to_string(std::lround(wert));
}

std::string kopf(std::string_view name, double breite, double hoehe)
{
    return "<mxfile host=\"Code2Diagram\">\n"
           "  <diagram name=\"" + xml(name.empty() ? std::string_view("diagram") : name) + "\" id=\"c2d\">\n"
           "    <mxGraphModel dx=\"" + zahl(breite) + "\" dy=\"" + zahl(hoehe) +
           "\" grid=\"1\" gridSize=\"10\" guides=\"1\" tooltips=\"1\" connect=\"1\" arrows=\"1\" fold=\"1\" "
           "page=\"0\" pageScale=\"1\" math=\"0\" shadow=\"0\">\n"
           "      <root>\n"
           "        <mxCell id=\"0\"/>\n"
           "        <mxCell id=\"1\" parent=\"0\"/>\n";
}

constexpr std::string_view kFuss = "      </root>\n"
                                   "    </mxGraphModel>\n"
                                   "  </diagram>\n"
                                   "</mxfile>\n";

/// Eine Zelle mit Ort. `inhalt` ist HTML und wird hier fuer das Attribut maskiert.
std::string ecke(const std::string& id, const std::string& inhalt, const std::string& stil, const std::string& eltern,
                 const Rechteck& ort)
{
    return "        <mxCell id=\"" + id + "\" value=\"" + xml(inhalt) + "\" style=\"" + stil +
           "\" vertex=\"1\" parent=\"" + eltern + "\">\n"
           "          <mxGeometry x=\"" + zahl(ort.x) + "\" y=\"" + zahl(ort.y) + "\" width=\"" + zahl(ort.breite) +
           "\" height=\"" + zahl(ort.hoehe) + "\" as=\"geometry\"/>\n"
           "        </mxCell>\n";
}

std::string kante(const std::string& id, const std::string& inhalt, const std::string& stil, const std::string& von,
                  const std::string& nach)
{
    return "        <mxCell id=\"" + id + "\" value=\"" + xml(inhalt) + "\" style=\"" + stil +
           "\" edge=\"1\" parent=\"1\" source=\"" + von + "\" target=\"" + nach + "\">\n"
           "          <mxGeometry relative=\"1\" as=\"geometry\"/>\n"
           "        </mxCell>\n";
}

/// Eine Linie ohne Spitze durch feste Punkte (die Koepfe eines Struktogramms).
std::string linie(const std::string& id, const Linienzug& l)
{
    const auto punkt = [](const Punkt& p, std::string_view als) {
        return "<mxPoint x=\"" + zahl(p.x) + "\" y=\"" + zahl(p.y) + "\"" +
               (als.empty() ? std::string() : " as=\"" + std::string(als) + "\"") + "/>";
    };
    std::string aus = "        <mxCell id=\"" + id +
                      "\" style=\"endArrow=none;html=1;rounded=0;\" edge=\"1\" parent=\"1\">\n"
                      "          <mxGeometry relative=\"1\" as=\"geometry\">\n"
                      "            " + punkt(l.front(), "sourcePoint") + "\n"
                      "            " + punkt(l.back(), "targetPoint") + "\n";
    if (l.size() > 2)
    {
        aus += "            <Array as=\"points\">";
        for (std::size_t i = 1; i + 1 < l.size(); ++i) aus += punkt(l[i], {});
        aus += "</Array>\n";
    }
    return aus + "          </mxGeometry>\n        </mxCell>\n";
}

/// Knotenart -> Stil der Zelle.
constexpr std::string_view formVon(KnotenArt art)
{
    switch (art)
    {
        case KnotenArt::Start:
        case KnotenArt::Ende:
            return "rounded=1;arcSize=50;";
        case KnotenArt::Verzweigung:
            return "rhombus;";
        case KnotenArt::Aufruf:
            return "shape=process;";
        case KnotenArt::Variante:
            return "shape=hexagon;perimeter=hexagonPerimeter2;fixedSize=1;size=14;";
        case KnotenArt::Anweisung:
            break;
    }
    return "rounded=0;";
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

/// Die Legende: je Bedingung ein Farbfeld und ihr Text.
std::string legende(const std::vector<FlussAnordnung::Legende>& eintraege, const Optionen& optionen)
{
    std::string aus;
    for (std::size_t i = 0; i < eintraege.size(); ++i)
    {
        const FlussAnordnung::Legende& l = eintraege[i];
        const std::string f(rahmenFarbe(l.farbe));
        aus += ecke("l" + std::to_string(i), "", "rounded=0;fillColor=" + f + ";strokeColor=" + f + ";", "1", l.feld);
        const double breite =
            static_cast<double>(kuerze(l.titel, optionen.maxZeilenLaenge).size()) * anordnung(optionen).zeichenBreite + 8;
        aus += ecke("lt" + std::to_string(i), html(kuerze(l.titel, optionen.maxZeilenLaenge)),
                    "text;align=left;verticalAlign=middle;html=1;" + std::string(kSchrift), "1",
                    {l.text.x, l.text.y - 9, breite, 18});
    }
    return aus;
}

/// Eine Tabelle: je Zelle eine Zelle, die doppelte Linie, die Textzeilen.
std::string tabelle(const TabellenAnordnung& a, const std::string& titel, const AnordnungOptionen& o)
{
    std::string aus = kopf(titel, a.breite, a.hoehe);
    for (std::size_t i = 0; i < a.zellen.size(); ++i)
    {
        const TabellenAnordnung::Zelle& z = a.zellen[i];
        std::string inhalt;
        for (const std::string& zeile : z.zeilen) inhalt += (inhalt.empty() ? "" : "<br>") + html(zeile);
        std::string stil = "rounded=0;whiteSpace=wrap;html=1;" + std::string(kSchrift);
        if (z.kopf || z.getoent) stil += "fillColor=" + std::string(c2d::diagramm::farbe(Farbe::Getoent)) + ";";
        if (z.grau) stil += "fontColor=" + std::string(c2d::diagramm::farbe(Farbe::FremdStrich)) + ";";
        if (z.kopf) stil += "fontStyle=1;";
        if (z.links) stil += "align=left;spacingLeft=6;";
        aus += ecke("z" + std::to_string(i), inhalt, stil, "1", z.ort);
    }
    for (std::size_t i = 0; i < a.linien.size(); ++i)
        if (a.linien[i].size() >= 2) aus += linie("l" + std::to_string(i), a.linien[i]);
    for (std::size_t i = 0; i < a.texte.size(); ++i)
    {
        const TabellenAnordnung::Text& t = a.texte[i];
        const double breite = static_cast<double>(t.text.size()) * o.zeichenBreite + 8;
        aus += ecke("t" + std::to_string(i), html(t.text),
                    "text;html=1;align=left;verticalAlign=middle;" + std::string(t.fett ? "fontStyle=1;" : "") +
                        std::string(kSchrift),
                    "1", {t.links.x, t.links.y - 9, breite, 18});
    }
    return aus + std::string(kFuss);
}

class Drawio final : public Format
{
public:
    [[nodiscard]] std::string_view name() const override { return "drawio"; }
    [[nodiscard]] std::string_view endung() const override { return "drawio"; }

    [[nodiscard]] std::string schreibe(const Flussdiagramm& diagramm, const Optionen& optionen) const override
    {
        const FlussAnordnung a = ordneAn(diagramm, anordnung(optionen));
        std::string aus = kopf(diagramm.titel(), a.breite, a.hoehe);

        // Ein Ort relativ zu der Gruppe, in der die Zelle liegt.
        const auto in = [&](std::size_t gruppe, Rechteck ort) {
            if (gruppe < a.gruppen.size())
            {
                ort.x -= a.gruppen[gruppe].x;
                ort.y -= a.gruppen[gruppe].y;
            }
            return ort;
        };
        const auto eltern = [&](std::size_t gruppe) {
            return gruppe < a.gruppen.size() ? "g" + std::to_string(gruppe) : std::string("1");
        };

        // die Behaelter zuerst: sie liegen hinter ihrem Inhalt, die aeusseren vor den inneren.
        // Ein Rahmen einer Bedingung traegt ihre Farbe.
        const std::vector<std::size_t> farben = farbenDerRahmen(diagramm);
        for (std::size_t g = 0; g < diagramm.gruppen().size(); ++g)
        {
            const Gruppe& gruppe = diagramm.gruppen()[g];
            std::string stil = "swimlane;startSize=22;fontStyle=0;collapsible=0;container=1;";
            if (farben[g] == kKeine)
                stil += "dashed=1;fillColor=none;";
            else
            {
                const std::string f(rahmenFarbe(farben[g]));
                stil += "dashed=0;swimlaneFillColor=" + f + ";fillColor=none;fillOpacity=8;strokeColor=" + f +
                        ";fontColor=" + f + ";";
            }
            aus += ecke("g" + std::to_string(g), html(kuerze(gruppe.titel, optionen.maxZeilenLaenge)),
                        stil + "whiteSpace=wrap;html=1;" + std::string(kSchrift), eltern(gruppe.eltern),
                        in(gruppe.eltern, a.gruppen[g]));
        }
        aus += legende(a.legende, optionen);
        for (std::size_t id = 0; id < diagramm.knoten().size(); ++id)
        {
            const Knoten& k = diagramm.knoten()[id];
            std::string inhalt;
            for (const std::string& zeile : k.zeilen)
            {
                if (!inhalt.empty()) inhalt += "<br>";
                inhalt += html(kuerze(zeile, optionen.maxZeilenLaenge));
            }
            // nie erreicht: grau und gestrichelt
            const std::string tot = k.unerreichbar ? "dashed=1;fillColor=" + std::string(c2d::diagramm::farbe(Farbe::Fremd)) +
                                                         ";strokeColor=" + std::string(c2d::diagramm::farbe(Farbe::FremdStrich)) + ";"
                                                   : std::string();
            aus += ecke("n" + std::to_string(id), inhalt,
                        std::string(formVon(k.art)) + tot + "whiteSpace=wrap;html=1;" + std::string(kSchrift),
                        eltern(k.gruppe), in(k.gruppe, a.knoten[id]));
        }
        for (std::size_t e = 0; e < diagramm.kanten().size(); ++e)
        {
            const Kante& k = diagramm.kanten()[e];
            aus += kante("e" + std::to_string(e), html(k.beschriftung),
                         "edgeStyle=orthogonalEdgeStyle;rounded=1;endArrow=block;endFill=1;html=1;" + std::string(kSchrift),
                         "n" + std::to_string(k.von), "n" + std::to_string(k.nach));
        }
        return aus + std::string(kFuss);
    }

    [[nodiscard]] std::string schreibe(const Strukturdiagramm& diagramm, const Optionen& optionen) const override
    {
        if (alsErDiagramm(diagramm)) return erDiagramm(erDiagrammVon(diagramm), optionen);
        const StrukturAnordnung a = ordneAn(diagramm, anordnung(optionen));
        std::string aus = kopf(diagramm.titel, a.breite, a.hoehe);

        const auto absatz = [&](std::string& inhalt, const auto& zeilen) {
            inhalt += "<hr size=\"1\"/><p style=\"margin:0px;margin-left:4px;\">";
            bool erste = true;
            for (const auto& z : zeilen)
            {
                if (!erste) inhalt += "<br/>";
                erste = false;
                if (z.sicht != '\0') inhalt += html(std::string(1, z.sicht)) + " ";
                inhalt += html(kuerze(z.text(), optionen.maxZeilenLaenge));
            }
            inhalt += "</p>";
        };
        for (std::size_t id = 0; id < diagramm.kaesten.size(); ++id)
        {
            // wie die UML-Klasse aus der Formenleiste von draw.io: Kopf, Linie, Glieder, Linie, Methoden
            const Kasten& k = diagramm.kaesten[id];
            std::string inhalt = "<p style=\"margin:0px;margin-top:4px;text-align:center;\">";
            if (const std::string_view s = stereotyp(k.art); !s.empty())
                inhalt += "<i>&lt;&lt;" + std::string(s) + "&gt;&gt;</i><br/>";
            inhalt += "<b>" + html(k.name) + "</b></p>";
            absatz(inhalt, k.eintraege);
            if (!k.methoden.empty()) absatz(inhalt, k.methoden);
            aus += ecke("k" + std::to_string(id), inhalt,
                        "verticalAlign=top;align=left;overflow=fill;whiteSpace=wrap;html=1;" + std::string(kSchrift), "1",
                        a.kaesten[id]);
        }
        for (std::size_t e = 0; e < diagramm.beziehungen.size(); ++e)
        {
            const Beziehung& b = diagramm.beziehungen[e];
            const std::string id = "e" + std::to_string(e);
            std::string stil(spitzen(b.art));
            if (b.fremdschluessel && diagramm.notation == ErNotation::Kraehenfuss)
            {
                // die Enden aus der Formenleiste Entity Relation von draw.io
                stil = std::string("startArrow=") + (b.amVon.max == 1 ? "ERzeroToOne" : "ERzeroToMany") + ";startFill=0;endArrow=" +
                       (b.amNach.min >= 1 ? "ERmandOne" : "ERzeroToOne") + ";endFill=0;";
            }
            aus += kante(id, html(b.beschriftung), stil + "html=1;" + std::string(kSchrift), "k" + std::to_string(b.von),
                         "k" + std::to_string(b.nach));
            if (b.fremdschluessel && diagramm.notation == ErNotation::Tabellen)
                aus += amEnde(id, "a", uml(b.amVon), true) + amEnde(id, "b", uml(b.amNach), false);
        }
        return aus + std::string(kFuss);
    }

    [[nodiscard]] static std::string_view spitzen(BeziehungArt art)
    {
        switch (art)
        {
            case BeziehungArt::Enthaelt: return "startArrow=diamondThin;startFill=1;startSize=14;endArrow=none;";
            case BeziehungArt::Verweist: return "endArrow=open;endSize=10;";
            case BeziehungArt::Erbt: return "endArrow=block;endFill=0;endSize=12;";
            case BeziehungArt::Realisiert: return "endArrow=block;endFill=0;endSize=12;dashed=1;";
        }
        return {};
    }

    /// Ein Text an einem Ende der Kante `kante` (draw.io haelt ihn dort, wenn sie wandert).
    [[nodiscard]] static std::string amEnde(const std::string& kante, std::string_view zusatz, const std::string& text, bool quelle)
    {
        return "        <mxCell id=\"" + kante + std::string(zusatz) + "\" value=\"" + xml(html(text)) +
               "\" style=\"edgeLabel;resizable=0;html=1;align=" + (quelle ? "left" : "right") + ";verticalAlign=bottom;" +
               std::string(kSchrift) + "\" vertex=\"1\" connectable=\"0\" parent=\"" + kante + "\">\n" +
               "          <mxGeometry x=\"" + (quelle ? "-0.85" : "0.85") + "\" relative=\"1\" as=\"geometry\"/>\n" +
               "        </mxCell>\n";
    }

    /// Chen und (min,max): Rechtecke, Rauten, Ellipsen (der Schluessel unterstrichen) an ihren Orten;
    /// Linien ohne Spitze, der Text am Ende der Entitaet.
    [[nodiscard]] static std::string erDiagramm(const ErDiagramm& d, const Optionen& optionen)
    {
        const ErAnordnung a = ordneAn(d, anordnung(optionen));
        std::string aus = kopf(d.titel, a.breite, a.hoehe);
        const std::string schrift(kSchrift);
        for (std::size_t v = 0; v < d.knoten.size(); ++v)
        {
            const ErKnoten& k = d.knoten[v];
            const std::string id = "k" + std::to_string(v);
            const std::string name = html(kuerze(k.name, optionen.maxZeilenLaenge));
            switch (k.art)
            {
                case ErKnotenArt::Entitaet:
                    aus += ecke(id, "<b>" + name + "</b>", "rounded=0;whiteSpace=wrap;html=1;" + schrift, "1", a.knoten[v]);
                    break;
                case ErKnotenArt::Beziehung:
                    aus += ecke(id, name, "rhombus;whiteSpace=wrap;html=1;" + schrift, "1", a.knoten[v]);
                    break;
                case ErKnotenArt::Kasten:
                {
                    std::string inhalt = "<p style=\"margin:0px;margin-top:4px;text-align:center;\">";
                    if (!k.stereotyp.empty()) inhalt += "<i>&lt;&lt;" + html(k.stereotyp) + "&gt;&gt;</i><br/>";
                    inhalt += "<b>" + name + "</b></p><hr size=\"1\"/><p style=\"margin:0px;margin-left:4px;\">";
                    for (std::size_t i = 0; i < k.zeilen.size(); ++i)
                    {
                        if (i == k.felder) inhalt += "</p><hr size=\"1\"/><p style=\"margin:0px;margin-left:4px;\">";
                        else if (i > 0) inhalt += "<br/>";
                        inhalt += html(kuerze(k.zeilen[i], optionen.maxZeilenLaenge));
                    }
                    inhalt += "</p>";
                    aus += ecke(id, inhalt, "verticalAlign=top;align=left;overflow=fill;whiteSpace=wrap;html=1;" + schrift, "1",
                                a.knoten[v]);
                    break;
                }
            }
        }
        for (std::size_t i = 0; i < a.ellipsen.size(); ++i)
        {
            const ErAnordnung::Ellipse& el = a.ellipsen[i];
            const ErAttribut& at = d.knoten[el.knoten].attribute[el.attribut];
            const std::string text = html(kuerze(at.name, optionen.maxZeilenLaenge));
            const std::string id = "a" + std::to_string(i);
            aus += ecke(id, at.schluessel ? "<u>" + text + "</u>" : text, "ellipse;whiteSpace=wrap;html=1;" + schrift, "1", el.ort);
            aus += kante("al" + std::to_string(i), {}, "endArrow=none;html=1;", id, "k" + std::to_string(el.knoten));
        }
        for (std::size_t e = 0; e < d.kanten.size(); ++e)
        {
            const ErKante& k = d.kanten[e];
            const std::string id = "e" + std::to_string(e);
            const std::string von = "k" + std::to_string(k.von);
            const std::string nach = "k" + std::to_string(k.nach);
            if (k.zwischenKaesten)
            {
                aus += kante(id, html(k.text), std::string(spitzen(k.art)) + "html=1;" + schrift, von, nach);
                continue;
            }
            aus += kante(id, {}, "endArrow=none;html=1;" + schrift, von, nach);
            if (!k.text.empty()) aus += amEnde(id, "t", k.text, k.textAmVon);
        }
        return aus + std::string(kFuss);
    }

    [[nodiscard]] std::string schreibe(const Zustandsdiagramm& diagramm, const Optionen& optionen) const override
    {
        const FlussAnordnung a = ordneAn(diagramm, anordnung(optionen));
        std::string aus = kopf(diagramm.titel, a.breite, a.hoehe);
        for (std::size_t z = 0; z < diagramm.zustaende.size(); ++z)
        {
            const Zustand& zustand = diagramm.zustaende[z];
            if (zustand.aussen)  // der Punkt von aussen: ein gefuellter Kreis
                aus += ecke("s" + std::to_string(z), {}, "ellipse;fillColor=#000000;strokeColor=#000000;html=1;", "1",
                            a.knoten[z]);
            else
                aus += ecke("s" + std::to_string(z), html(kuerze(zustand.name, optionen.maxZeilenLaenge)),
                            "rounded=1;arcSize=30;whiteSpace=wrap;html=1;" + std::string(kSchrift), "1", a.knoten[z]);
        }
        for (std::size_t e = 0; e < diagramm.uebergaenge.size(); ++e)
        {
            const Uebergang& u = diagramm.uebergaenge[e];
            aus += kante("e" + std::to_string(e), html(kuerze(u.bedingung, optionen.maxZeilenLaenge)),
                         "edgeStyle=orthogonalEdgeStyle;rounded=1;endArrow=block;endFill=1;html=1;" + std::string(kSchrift),
                         "s" + std::to_string(u.von), "s" + std::to_string(u.nach));
        }
        return aus + std::string(kFuss);
    }

    [[nodiscard]] bool kann(Diagrammart) const override { return true; }

    [[nodiscard]] std::string schreibe(const Struktogramm& diagramm, const Optionen& optionen) const override
    {
        using FeldArt = StruktoAnordnung::FeldArt;
        const AnordnungOptionen o = anordnung(optionen);
        const StruktoAnordnung a = ordneAn(diagramm, o);
        const auto hell = [](Farbe welche) { return std::string(c2d::diagramm::farbe(welche)); };
        std::string aus = kopf(diagramm.titel, a.breite, a.hoehe);
        for (std::size_t i = 0; i < a.felder.size(); ++i)
        {
            const StruktoAnordnung::Feld& f = a.felder[i];
            std::string inhalt;
            for (const std::string& z : f.zeilen) inhalt += (inhalt.empty() ? "" : "<br>") + html(kuerze(z, o.maxZeilenLaenge));
            std::string stil = "rounded=0;whiteSpace=wrap;html=1;" + std::string(kSchrift);
            if (f.art == FeldArt::Titel) stil += "fillColor=" + hell(Farbe::Getoent) + ";";
            if (f.unerreichbar)
                stil += "dashed=1;fillColor=" + hell(Farbe::Fremd) + ";strokeColor=" + hell(Farbe::FremdStrich) + ";";
            // der Text steht, wo die Anordnung ihn hinstellt: oben im Kopf, unten im Fuss, rechts vom Pfeil
            const Punkt m = f.ort.mitte();
            if (f.text.y < m.y - 1) stil += "verticalAlign=top;spacingTop=" + zahl(f.text.y - f.ort.y - o.zeilenHoehe / 2) + ";";
            if (f.text.y > m.y + 1)
                stil += "verticalAlign=bottom;spacingBottom=" + zahl(f.ort.y + f.ort.hoehe - f.text.y - o.zeilenHoehe / 2) + ";";
            if (f.text.x > m.x + 1) stil += "spacingLeft=" + zahl(2 * (f.text.x - m.x)) + ";";
            aus += ecke("f" + std::to_string(i), inhalt, stil, "1", f.ort);
        }
        for (std::size_t i = 0; i < a.linien.size(); ++i)
            if (a.linien[i].size() >= 2) aus += linie("l" + std::to_string(i), a.linien[i]);
        for (std::size_t i = 0; i < a.beschriftungen.size(); ++i)
        {
            const StruktoAnordnung::Beschriftung& b = a.beschriftungen[i];
            const std::string text = kuerze(b.text, o.maxZeilenLaenge);
            const double breite = static_cast<double>(text.size()) * o.zeichenBreite + 8;
            aus += ecke("b" + std::to_string(i), html(text), "text;html=1;align=center;verticalAlign=middle;" + std::string(kSchrift),
                        "1", {b.mitte.x - breite / 2, b.mitte.y - 9, breite, 18});
        }
        for (std::size_t i = 0; i < a.rahmen.size(); ++i)
        {
            const StruktoAnordnung::Rahmen& r = a.rahmen[i];
            const std::string f(rahmenFarbe(r.farbe));
            aus += ecke("r" + std::to_string(i), html(kuerze(r.titel, titelZeichen(r.ort, o))),
                        "rounded=0;fillColor=" + f + ";fillOpacity=8;strokeColor=" + f + ";fontColor=" + f +
                            ";fontStyle=2;align=left;verticalAlign=top;spacingLeft=6;whiteSpace=wrap;html=1;" +
                            std::string(kSchrift),
                        "1", r.ort);
        }
        aus += legende(a.legende, optionen);
        return aus + std::string(kFuss);
    }

    [[nodiscard]] std::string schreibe(std::span<const Wahrheitstabelle> tabellen, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        return tabelle(ordneAn(tabellen, o), tabellen.empty() ? std::string("truth table") : tabellen.front().funktion, o);
    }

    [[nodiscard]] std::string schreibe(const Entscheidungstabelle& t, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        return tabelle(ordneAn(t, o), t.funktion, o);
    }

    [[nodiscard]] std::string schreibe(std::span<const Zuordnung> tabellen, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        return tabelle(ordneAn(tabellen, o), tabellen.empty() ? std::string("mapping") : tabellen.front().titel, o);
    }

    [[nodiscard]] std::string schreibe(std::span<const Konfigurationstabelle> tabellen, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        return tabelle(ordneAn(tabellen, o), "#if configuration", o);
    }

    [[nodiscard]] std::string schreibe(const Schaltstellen& t, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        return tabelle(ordneAn(t, o), t.titel, o);
    }

    [[nodiscard]] std::string schreibe(const Schalteruebersicht& t, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        return tabelle(ordneAn(t, o), t.titel, o);
    }

    /// Ein Datenfluss: je Rahmen (Stage, Funktion) ein Behaelter, Quelle Zylinder, Shuffle Sechseck, Aktion rund
    /// getoent, spark.sql und Aufruf als Prozess, Session gestrichelt, zwischengespeichert dick; Pfad gestrichelt.
    [[nodiscard]] std::string schreibe(const Datenflussdiagramm& d, const Optionen& optionen) const override
    {
        const FlussAnordnung a = ordneAn(d, anordnung(optionen));
        std::string aus = kopf(d.titel, a.breite, a.hoehe);
        const auto in = [&](std::size_t rahmen, Rechteck ort) {
            if (rahmen < a.gruppen.size())
            {
                ort.x -= a.gruppen[rahmen].x;
                ort.y -= a.gruppen[rahmen].y;
            }
            return ort;
        };
        const auto eltern = [&](std::size_t rahmen) { return rahmen < a.gruppen.size() ? "r" + std::to_string(rahmen) : std::string("1"); };
        for (std::size_t r = 0; r < d.rahmen.size() && r < a.gruppen.size(); ++r)
            aus += ecke("r" + std::to_string(r), html(kuerze(d.rahmen[r].titel, optionen.maxZeilenLaenge)),
                        "swimlane;startSize=22;fontStyle=2;collapsible=0;container=1;dashed=1;fillColor=none;whiteSpace=wrap;html=1;" +
                            std::string(kSchrift),
                        eltern(d.rahmen[r].eltern), in(d.rahmen[r].eltern, a.gruppen[r]));
        const auto hell = [](Farbe welche) { return std::string(c2d::diagramm::farbe(welche)); };
        for (std::size_t i = 0; i < d.knoten.size(); ++i)
        {
            const DatenflussKnoten& k = d.knoten[i];
            std::string stil = "rounded=0;";
            switch (k.art)
            {
                case DatenflussKnotenArt::Quelle: stil = "shape=cylinder3;boundedLbl=1;size=8;"; break;
                case DatenflussKnotenArt::Shuffle: stil = "shape=hexagon;perimeter=hexagonPerimeter2;"; break;
                case DatenflussKnotenArt::Aktion: stil = "rounded=1;fillColor=" + hell(Farbe::Getoent) + ";"; break;
                case DatenflussKnotenArt::Sql:
                case DatenflussKnotenArt::Aufruf: stil = "shape=process;"; break;
                case DatenflussKnotenArt::Sitzung: stil = "rounded=0;dashed=1;"; break;
                case DatenflussKnotenArt::Schritte: break;
            }
            if (k.zwischengespeichert) stil += "strokeWidth=2.4;";
            std::string text;
            const auto dazu = [&](const std::string& t) { text += (text.empty() ? "" : "<br>") + t; };
            if (const std::string_view s = stereotyp(k.art); !s.empty()) dazu("<i>" + html(std::string(s)) + "</i>");
            if (!k.titel.empty()) dazu("<b>" + html(kuerze(k.titel, optionen.maxZeilenLaenge)) + "</b>");
            for (const std::string& z : k.zeilen) dazu(html(kuerze(z, optionen.maxZeilenLaenge)));
            aus += ecke("d" + std::to_string(i), text, stil + "whiteSpace=wrap;html=1;align=left;spacingLeft=6;" + std::string(kSchrift),
                        eltern(k.gruppe), in(k.gruppe, a.knoten[i]));
        }
        for (std::size_t e = 0; e < d.kanten.size(); ++e)
        {
            const DatenflussKante& k = d.kanten[e];
            std::string stil = "edgeStyle=orthogonalEdgeStyle;rounded=1;html=1;endArrow=block;endFill=1;";
            if (k.art == DatenflussKantenArt::Pfad) stil += "dashed=1;";
            aus += kante("e" + std::to_string(e), html(kuerze(k.text, optionen.maxZeilenLaenge)), stil + std::string(kSchrift),
                         "d" + std::to_string(k.von), "d" + std::to_string(k.nach));
        }
        return aus + std::string(kFuss);
    }

    /// Ein Plan: je Rahmen (Unterabfrage, Stage, Codegen) ein Behaelter, Scan Zylinder, Exchange Sechseck (Broadcast
    /// gestrichelt), Unterabfrage als Prozess, die Wurzel rund getoent, wiederverwendet gestrichelt; Kanten einer
    /// Unterabfrage gestrichelt, Wiederverwendung gepunktet.
    [[nodiscard]] std::string schreibe(const Plandiagramm& d, const Optionen& optionen) const override
    {
        const FlussAnordnung a = ordneAn(d, anordnung(optionen));
        std::string aus = kopf(d.titel, a.breite, a.hoehe);
        const auto in = [&](std::size_t rahmen, Rechteck ort) {
            if (rahmen < a.gruppen.size())
            {
                ort.x -= a.gruppen[rahmen].x;
                ort.y -= a.gruppen[rahmen].y;
            }
            return ort;
        };
        const auto eltern = [&](std::size_t rahmen) { return rahmen < a.gruppen.size() ? "r" + std::to_string(rahmen) : std::string("1"); };
        for (std::size_t r = 0; r < d.rahmen.size() && r < a.gruppen.size(); ++r)
            aus += ecke("r" + std::to_string(r), html(kuerze(d.rahmen[r].titel, optionen.maxZeilenLaenge)),
                        "swimlane;startSize=22;fontStyle=2;collapsible=0;container=1;dashed=1;fillColor=none;whiteSpace=wrap;html=1;" +
                            std::string(kSchrift),
                        eltern(d.rahmen[r].eltern), in(d.rahmen[r].eltern, a.gruppen[r]));
        for (std::size_t i = 0; i < d.knoten.size(); ++i)
        {
            const PlanKnoten& k = d.knoten[i];
            std::string stil = "rounded=0;";
            switch (k.art)
            {
                case PlanKnotenArt::Scan: stil = "shape=cylinder3;boundedLbl=1;size=8;"; break;
                case PlanKnotenArt::Exchange: stil = "shape=hexagon;perimeter=hexagonPerimeter2;"; break;
                case PlanKnotenArt::Broadcast: stil = "shape=hexagon;perimeter=hexagonPerimeter2;dashed=1;"; break;
                case PlanKnotenArt::Wiederverwendet: stil = "rounded=0;dashed=1;"; break;
                case PlanKnotenArt::Unterabfrage: stil = "shape=process;"; break;
                case PlanKnotenArt::Wurzel:
                    stil = "rounded=1;fillColor=" + std::string(c2d::diagramm::farbe(Farbe::Getoent)) + ";";
                    break;
                default: break;
            }
            std::string text;
            const auto dazu = [&](const std::string& t) { text += (text.empty() ? "" : "<br>") + t; };
            if (const std::string_view s = stereotyp(k.art); !s.empty()) dazu("<i>" + html(std::string(s)) + "</i>");
            dazu("<b>" + html(kuerze(k.titel, optionen.maxZeilenLaenge)) + "</b>");
            for (const std::string& z : k.zeilen) dazu(html(kuerze(z, optionen.maxZeilenLaenge)));
            aus += ecke("d" + std::to_string(i), text, stil + "whiteSpace=wrap;html=1;align=left;spacingLeft=6;" + std::string(kSchrift),
                        eltern(k.gruppe), in(k.gruppe, a.knoten[i]));
        }
        for (std::size_t e = 0; e < d.kanten.size(); ++e)
        {
            const PlanKante& k = d.kanten[e];
            std::string stil = "edgeStyle=orthogonalEdgeStyle;rounded=1;html=1;endArrow=block;endFill=1;";
            if (k.art == PlanKantenArt::Unterabfrage) stil += "dashed=1;";
            if (k.art == PlanKantenArt::Wiederverwendung) stil += "dashed=1;dashPattern=1 3;";
            aus += kante("e" + std::to_string(e), html(kuerze(k.text, optionen.maxZeilenLaenge)), stil + std::string(kSchrift),
                         "d" + std::to_string(k.von), "d" + std::to_string(k.nach));
        }
        return aus + std::string(kFuss);
    }

    /// Eine Abfrage: je Rahmen ein Behaelter (verschachtelt, Orte relativ zu ihm), Knoten mit dem Titel
    /// fett, Join ohne Spitze, WHERE gestrichelt, korreliert gepunktet, Fluss mit Spitze.
    [[nodiscard]] std::string schreibe(const Abfragediagramm& d, const Optionen& optionen) const override
    {
        const FlussAnordnung a = ordneAn(d, anordnung(optionen));
        std::string aus = kopf(d.titel, a.breite, a.hoehe);
        const auto in = [&](std::size_t rahmen, Rechteck ort) {
            if (rahmen < a.gruppen.size())
            {
                ort.x -= a.gruppen[rahmen].x;
                ort.y -= a.gruppen[rahmen].y;
            }
            return ort;
        };
        const auto eltern = [&](std::size_t rahmen) { return rahmen < a.gruppen.size() ? "r" + std::to_string(rahmen) : std::string("1"); };
        for (std::size_t r = 0; r < d.rahmen.size() && r < a.gruppen.size(); ++r)
            aus += ecke("r" + std::to_string(r), html(kuerze(d.rahmen[r].titel, optionen.maxZeilenLaenge)),
                        "swimlane;startSize=22;fontStyle=2;collapsible=0;container=1;dashed=1;fillColor=none;whiteSpace=wrap;html=1;" +
                            std::string(kSchrift),
                        eltern(d.rahmen[r].eltern), in(d.rahmen[r].eltern, a.gruppen[r]));
        const auto hell = [](Farbe welche) { return std::string(c2d::diagramm::farbe(welche)); };
        for (std::size_t i = 0; i < d.knoten.size(); ++i)
        {
            const AbfrageKnoten& k = d.knoten[i];
            std::string stil = "rounded=0;";
            switch (k.art)
            {
                case AbfrageKnotenArt::Ergebnis: stil = "rounded=1;fillColor=" + hell(Farbe::Getoent) + ";"; break;
                case AbfrageKnotenArt::Menge: stil = "shape=hexagon;perimeter=hexagonPerimeter2;fillColor=" + hell(Farbe::Getoent) + ";"; break;
                case AbfrageKnotenArt::View: stil = "rounded=0;dashed=1;"; break;
                case AbfrageKnotenArt::Mit: stil = "rounded=1;arcSize=12;"; break;
                case AbfrageKnotenArt::Abgeleitet: stil = "rounded=1;arcSize=12;dashed=1;"; break;
                case AbfrageKnotenArt::Funktion: stil = "shape=process;"; break;
                case AbfrageKnotenArt::Ziel: stil = "rounded=0;strokeWidth=2;"; break;
                case AbfrageKnotenArt::Fremd: stil = "rounded=0;fillColor=" + hell(Farbe::Fremd) + ";strokeColor=" + hell(Farbe::FremdStrich) + ";"; break;
                default: break;
            }
            std::string text = "<b>" + html(kuerze(k.titel, optionen.maxZeilenLaenge)) + "</b>";
            for (const std::string& z : k.zeilen) text += "<br>" + html(kuerze(z, optionen.maxZeilenLaenge));
            aus += ecke("q" + std::to_string(i), text, stil + "whiteSpace=wrap;html=1;align=left;spacingLeft=6;" + std::string(kSchrift),
                        eltern(k.gruppe), in(k.gruppe, a.knoten[i]));
        }
        for (std::size_t e = 0; e < d.kanten.size(); ++e)
        {
            const AbfrageKante& k = d.kanten[e];
            std::string stil = "edgeStyle=orthogonalEdgeStyle;rounded=1;html=1;";
            switch (k.art)
            {
                case AbfrageKantenArt::Join: stil += "endArrow=none;"; break;
                case AbfrageKantenArt::Wo: stil += "endArrow=none;dashed=1;"; break;
                case AbfrageKantenArt::Korreliert: stil += "endArrow=none;dashed=1;dashPattern=1 3;"; break;
                case AbfrageKantenArt::Fluss: stil += "endArrow=block;endFill=1;"; break;
                case AbfrageKantenArt::Schreibt: stil += "endArrow=block;endFill=1;strokeWidth=2;"; break;
            }
            aus += kante("e" + std::to_string(e), html(kuerze(k.text, optionen.maxZeilenLaenge)), stil + std::string(kSchrift),
                         "q" + std::to_string(k.von), "q" + std::to_string(k.nach));
        }
        return aus + std::string(kFuss);
    }

    [[nodiscard]] std::string schreibe(const Aufrufgraph& graph, const Optionen& optionen) const override
    {
        const FlussAnordnung a = ordneAn(graph, anordnung(optionen));
        std::string aus = kopf(graph.titel, a.breite, a.hoehe);

        // je Datei ein Behaelter; eine Funktion darin mit ihrem Ort relativ zu ihm
        const auto inDatei = [&](const AufrufKnoten& k) {
            return graph.mitRahmen() && imProjekt(k.art) && k.datei < a.gruppen.size();
        };
        for (std::size_t d = 0; d < a.gruppen.size() && d < graph.dateien.size(); ++d)
            aus += ecke("d" + std::to_string(d), html(kuerze(graph.dateien[d], optionen.maxZeilenLaenge)),
                        "swimlane;startSize=22;fontStyle=2;collapsible=0;container=1;dashed=1;fillColor=none;whiteSpace=wrap;"
                        "html=1;" + std::string(kSchrift),
                        "1", a.gruppen[d]);
        aus += legende(a.legende, optionen);
        for (std::size_t id = 0; id < graph.knoten.size(); ++id)
        {
            const AufrufKnoten& k = graph.knoten[id];
            // die hellen Farben der Tabelle: draw.io kennt keine dunkle Fassung
            const auto hell = [](Farbe welche) { return std::string(c2d::diagramm::farbe(welche)); };
            const std::string fremd =
                "rounded=0;fillColor=" + hell(Farbe::Fremd) + ";strokeColor=" + hell(Farbe::FremdStrich) + ";fontStyle=2;";
            std::string stil;
            switch (k.art)
            {
                case AufrufKnotenArt::Funktion:
                    stil = k.wurzel ? "shape=process;fillColor=" + hell(Farbe::Getoent) + ";strokeWidth=2;" : "shape=process;";
                    break;
                case AufrufKnotenArt::Fremd: stil = fremd; break;
                case AufrufKnotenArt::Makro: stil = fremd + "dashed=1;"; break;
                // die Abhaengigkeiten: Tabelle ein Kasten, View gestrichelt, Trigger ein Sechseck, Routine wie eine Funktion
                case AufrufKnotenArt::Tabelle: stil = "rounded=0;"; break;
                case AufrufKnotenArt::FremdeTabelle: stil = fremd; break;
                case AufrufKnotenArt::View: stil = "rounded=0;dashed=1;"; break;
                case AufrufKnotenArt::Trigger: stil = "shape=hexagon;perimeter=hexagonPerimeter2;"; break;
                case AufrufKnotenArt::Routine: stil = "shape=process;"; break;
            }
            if (k.wurzel && k.art != AufrufKnotenArt::Funktion) stil += "fillColor=" + hell(Farbe::Getoent) + ";strokeWidth=2;";
            std::string text;
            for (const std::string& zeile : knotenZeilen(k))
                text += (text.empty() ? "" : "<br>") + html(kuerze(zeile, optionen.maxZeilenLaenge));
            Rechteck ort = a.knoten[id];
            std::string eltern = "1";
            if (inDatei(k))
            {
                ort.x -= a.gruppen[k.datei].x;
                ort.y -= a.gruppen[k.datei].y;
                eltern = "d" + std::to_string(k.datei);
            }
            aus += ecke("n" + std::to_string(id), text, stil + "whiteSpace=wrap;html=1;" + std::string(kSchrift), eltern, ort);
        }
        // mehrdeutig gestrichelt, aus einem Zweig, der nicht gilt, gepunktet; eine Bedingung, die gilt, in ihrer Farbe
        const std::vector<std::size_t> farben = farbenDerBedingungen(graph);
        for (std::size_t e = 0; e < graph.kanten.size(); ++e)
        {
            const AufrufKante& k = graph.kanten[e];
            std::string stil = "edgeStyle=orthogonalEdgeStyle;rounded=1;endArrow=block;endFill=1;html=1;";
            if (!k.gilt) stil += "dashed=1;dashPattern=1 3;";
            else if (k.art == AufrufKantenArt::Mehrdeutig) stil += "dashed=1;";
            if (farben[e] != kKeine) stil += "strokeColor=" + std::string(rahmenFarbe(farben[e])) + ";";
            aus += kante("e" + std::to_string(e), html(kuerze(beschriftung(k), optionen.maxZeilenLaenge)),
                         stil + std::string(kSchrift), "n" + std::to_string(k.von), "n" + std::to_string(k.nach));
        }
        return aus + std::string(kFuss);
    }
};

}  // namespace

const Format& drawio()
{
    static const Drawio format;
    return format;
}

}  // namespace c2d::ausgabe
