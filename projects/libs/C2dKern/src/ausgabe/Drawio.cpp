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
        case KastenArt::Klasse: break;
    }
    return {};
}

/// Die Legende: je Bedingung ein Farbfeld und ihr Text.
std::string legende(const FlussAnordnung& a, const Optionen& optionen)
{
    std::string aus;
    for (std::size_t i = 0; i < a.legende.size(); ++i)
    {
        const FlussAnordnung::Legende& l = a.legende[i];
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
        aus += legende(a, optionen);
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
            std::string_view stil;
            switch (b.art)
            {
                case BeziehungArt::Enthaelt: stil = "startArrow=diamondThin;startFill=1;startSize=14;endArrow=none;"; break;
                case BeziehungArt::Verweist: stil = "endArrow=open;endSize=10;"; break;
                case BeziehungArt::Erbt: stil = "endArrow=block;endFill=0;endSize=12;"; break;
            }
            aus += kante("e" + std::to_string(e), html(b.beschriftung), std::string(stil) + "html=1;" + std::string(kSchrift),
                         "k" + std::to_string(b.von), "k" + std::to_string(b.nach));
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

    [[nodiscard]] std::string schreibe(const Aufrufgraph& graph, const Optionen& optionen) const override
    {
        const FlussAnordnung a = ordneAn(graph, anordnung(optionen));
        std::string aus = kopf(graph.titel, a.breite, a.hoehe);

        // je Datei ein Behaelter; eine Funktion darin mit ihrem Ort relativ zu ihm
        const auto inDatei = [&](const AufrufKnoten& k) {
            return graph.mitRahmen() && k.art == AufrufKnotenArt::Funktion && k.datei < a.gruppen.size();
        };
        for (std::size_t d = 0; d < a.gruppen.size() && d < graph.dateien.size(); ++d)
            aus += ecke("d" + std::to_string(d), html(kuerze(graph.dateien[d], optionen.maxZeilenLaenge)),
                        "swimlane;startSize=22;fontStyle=2;collapsible=0;container=1;dashed=1;fillColor=none;whiteSpace=wrap;"
                        "html=1;" + std::string(kSchrift),
                        "1", a.gruppen[d]);
        aus += legende(a, optionen);
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
            }
            Rechteck ort = a.knoten[id];
            std::string eltern = "1";
            if (inDatei(k))
            {
                ort.x -= a.gruppen[k.datei].x;
                ort.y -= a.gruppen[k.datei].y;
                eltern = "d" + std::to_string(k.datei);
            }
            aus += ecke("n" + std::to_string(id), html(kuerze(knotenText(k), optionen.maxZeilenLaenge)),
                        stil + "whiteSpace=wrap;html=1;" + std::string(kSchrift), eltern, ort);
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
