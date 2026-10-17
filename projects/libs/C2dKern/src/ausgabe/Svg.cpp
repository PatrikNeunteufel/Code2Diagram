/**
 * @file Svg.cpp
 * @brief SVG: das Diagramm so gezeichnet, wie die Vorschau es zeigt
 *
 * Das eigene Format: Orte, Wege der Kanten und Orte der Beschriftungen kommen
 * aus diagramm/Anordnung.hpp, die Formen sind die der Vorschau. Kein fremdes
 * Werkzeug ordnet neu an — was man in der Vorschau prueft, steht in der Datei.
 * Hell gezeichnet; ein Browser im dunklen Modus nimmt die dunklen Farben
 * (`prefers-color-scheme`).
 */
#include "c2d/ausgabe/Ausgabe.hpp"

#include "Text.hpp"
#include "c2d/diagramm/Farben.hpp"
#include "c2d/diagramm/Anordnung.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace c2d::ausgabe
{

namespace
{

using namespace c2d::diagramm;

constexpr double kSpitze = 9;        // Laenge einer Pfeilspitze
constexpr double kSpitzeErbt = 13;   // die hohle Spitze der Vererbung
constexpr double kSpitzeWinkel = 0.42;
constexpr double kSchildHoehe = 15;  // das Feld unter einer Beschriftung
constexpr double kGrundlinie = 4;    // von der Mitte einer Zeile zu ihrer Grundlinie (Schrift 12 px)

/// Eine Zahl fuer ein Attribut: eine Nachkommastelle, ohne ",0".
std::string zahl(double wert)
{
    char puffer[32];
    std::snprintf(puffer, sizeof puffer, "%.1f", std::round(wert * 10) / 10);
    std::string aus = puffer;
    if (aus.size() > 2 && aus.compare(aus.size() - 2, 2, ".0") == 0) aus.resize(aus.size() - 2);
    if (aus == "-0") aus = "0";
    return aus;
}

std::string punkte(const Linienzug& linie)
{
    std::string aus;
    for (const Punkt& p : linie)
    {
        if (!aus.empty()) aus += ' ';
        aus += zahl(p.x) + "," + zahl(p.y);
    }
    return aus;
}

/// Zeichen (nicht Bytes) in UTF-8 — fuer die geschaetzte Breite eines Texts.
std::size_t zeichen(std::string_view text)
{
    std::size_t n = 0;
    for (const char c : text)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

/// Die Farben der Klassen aus der Tabelle (Farben.hpp), hell oder dunkel.
std::string farbRegeln(bool dunkel, std::string_view einzug)
{
    const auto f = [dunkel](Farbe welche) { return std::string(c2d::diagramm::farbe(welche, dunkel)); };
    const std::string e(einzug);
    return e + ".grund { fill: " + f(Farbe::Grund) + "; }\n" +
           e + ".flaeche, .hohl, .schild { fill: " + f(Farbe::Flaeche) + "; }\n" +
           e + ".getoent { fill: " + f(Farbe::Getoent) + "; }\n" +
           e + ".flaeche, .getoent, .linie, .spitze, .hohl { stroke: " + f(Farbe::Strich) + "; }\n" +
           e + ".spitze { fill: " + f(Farbe::Strich) + "; }\n" +
           e + ".rahmen { stroke: " + f(Farbe::Rahmen) + "; }\n" +
           e + ".fremd { fill: " + f(Farbe::Fremd) + "; stroke: " + f(Farbe::FremdStrich) + "; }\n" +
           e + "text { fill: " + f(Farbe::Text) + "; }\n" +
           e + "text.grau { fill: " + f(Farbe::FremdStrich) + "; }\n" +
           e + ".tot .flaeche, .tot .getoent { fill: " + f(Farbe::Fremd) + "; }\n" +
           e + ".tot .flaeche, .tot .getoent, .tot .linie { stroke: " + f(Farbe::FremdStrich) + "; }\n";
}

std::string kopf(std::string_view titel, double breite, double hoehe)
{
    const std::string b = zahl(breite);
    const std::string h = zahl(hoehe);
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + b + "\" height=\"" + h + "\" viewBox=\"0 0 " + b +
           " " + h + "\" font-family=\"Consolas, 'Courier New', monospace\" font-size=\"12\">\n"
           "  <title>" + xml(titel) + "</title>\n"
           "  <style>\n"
           "    .linie { fill: none; }\n"
           "    .flaeche, .getoent, .linie, .spitze, .hohl { stroke-width: 1.2; }\n"
           "    .rahmen { fill: none; stroke-width: 1.2; stroke-dasharray: 5 4; }\n"
           "    .fremd { stroke-width: 1.2; }\n"
           "    .tot .flaeche, .tot .getoent, .tot .linie { stroke-dasharray: 5 4; }\n" +
           farbRegeln(false, "    ") + "    @media (prefers-color-scheme: dark) {\n" + farbRegeln(true, "      ") +
           "    }\n"
           "  </style>\n"
           "  <rect class=\"grund\" width=\"100%\" height=\"100%\"/>\n";
}

/// Zeilen mittig um `mitte`, je `zeilenHoehe` untereinander.
std::string textMittig(const std::vector<std::string>& zeilen, Punkt mitte, double zeilenHoehe, std::string_view stil = {})
{
    std::string aus = "  <text text-anchor=\"middle\"";
    if (!stil.empty()) aus += " " + std::string(stil);
    aus += ">";
    const double oben = mitte.y - zeilenHoehe * static_cast<double>(zeilen.size() - 1) / 2;
    for (std::size_t i = 0; i < zeilen.size(); ++i)
        aus += "<tspan x=\"" + zahl(mitte.x) + "\" y=\"" + zahl(oben + zeilenHoehe * static_cast<double>(i) + kGrundlinie) +
               "\">" + xml(zeilen[i]) + "</tspan>";
    return aus + "</text>\n";
}

/// Eine Zeile, links beginnend; `oben` ist die Oberkante der Zeile.
std::string textLinks(std::string_view zeile, double x, double oben, double zeilenHoehe, std::string_view stil = {})
{
    std::string aus = "  <text x=\"" + zahl(x) + "\" y=\"" + zahl(oben + zeilenHoehe / 2 + kGrundlinie) + "\"";
    if (!stil.empty()) aus += " " + std::string(stil);
    return aus + ">" + xml(zeile) + "</text>\n";
}

/// Ein Dreieck an `an`, das von `von` her zeigt; `stil` steht als Attribut dabei (etwa eine Farbe).
std::string spitze(Punkt von, Punkt an, bool gefuellt, double laenge = kSpitze, std::string_view stil = {})
{
    const double winkel = std::atan2(an.y - von.y, an.x - von.x);
    const auto ecke = [&](double drehung) {
        return Punkt{an.x - laenge * std::cos(winkel + drehung), an.y - laenge * std::sin(winkel + drehung)};
    };
    return "  <polygon class=\"" + std::string(gefuellt ? "spitze" : "hohl") + "\" points=\"" +
           punkte({an, ecke(kSpitzeWinkel), ecke(-kSpitzeWinkel)}) + "\"" + (stil.empty() ? "" : " " + std::string(stil)) +
           "/>\n";
}

/// Eine gefuellte Raute an `an`, auf der Linie nach `nach` (der Besitzer einer Komposition).
std::string raute(Punkt an, Punkt nach)
{
    const double winkel = std::atan2(nach.y - an.y, nach.x - an.x);
    const auto auf = [&](double weit, double seitlich) {
        return Punkt{an.x + weit * std::cos(winkel) - seitlich * std::sin(winkel),
                     an.y + weit * std::sin(winkel) + seitlich * std::cos(winkel)};
    };
    return "  <polygon class=\"spitze\" points=\"" + punkte({an, auf(8, 5), auf(16, 0), auf(8, -5)}) + "\"/>\n";
}

std::string rechteck(const Rechteck& r, std::string_view klasse, double rund = 0)
{
    std::string aus = "  <rect class=\"" + std::string(klasse) + "\" x=\"" + zahl(r.x) + "\" y=\"" + zahl(r.y) +
                      "\" width=\"" + zahl(r.breite) + "\" height=\"" + zahl(r.hoehe) + "\"";
    if (rund > 0) aus += " rx=\"" + zahl(rund) + "\"";
    return aus + "/>\n";
}

/// Ein Rechteck mit eigenem Stil hinter der Klasse, etwa der Farbe eines Rahmens.
std::string rechteckMit(const Rechteck& r, std::string_view klasse, const std::string& stil)
{
    std::string aus = rechteck(r, klasse);
    return aus.insert(aus.size() - 3, " " + stil);  // vor "/>\n"
}

/// Rand, Titel und ein Hauch in der Flaeche in der Farbe einer Bedingung.
std::string flaechenStil(std::size_t farbe)
{
    const std::string f(rahmenFarbe(farbe));
    return "style=\"fill:" + f + ";fill-opacity:0.08;stroke:" + f + "\"";
}

std::string textStil(std::size_t farbe)
{
    return "style=\"fill:" + std::string(rahmenFarbe(farbe)) + "\"";
}

/// Je Bedingung ein Farbfeld und ihr Text.
std::string legende(const std::vector<FlussAnordnung::Legende>& eintraege, const AnordnungOptionen& o)
{
    std::string aus;
    for (const FlussAnordnung::Legende& l : eintraege)
    {
        aus += rechteckMit(l.feld, "rahmen", flaechenStil(l.farbe));
        aus += textLinks(kuerze(l.titel, o.maxZeilenLaenge), l.text.x, l.text.y - o.zeilenHoehe / 2, o.zeilenHoehe);
    }
    return aus;
}

/// Ein Titel auf einem Schild oben links im Rahmen `r`.
std::string rahmenTitel(const std::string& text, const Rechteck& r, const AnordnungOptionen& o, const std::string& stil)
{
    return rechteck({r.x + 5, r.y + 4, static_cast<double>(zeichen(text)) * o.zeichenBreite + 6, o.zeilenHoehe}, "schild") +
           textLinks(text, r.x + 8, r.y + 4, o.zeilenHoehe, stil);
}

/// Die Beschriftung einer Kante auf einem Schild um `m`.
std::string schild(const std::string& inhalt, Punkt m, const AnordnungOptionen& o)
{
    const double breite = static_cast<double>(zeichen(inhalt)) * o.zeichenBreite + 4;
    return rechteck({m.x - breite / 2, m.y - kSchildHoehe / 2, breite, kSchildHoehe}, "schild") +
           textMittig({inhalt}, m, o.zeilenHoehe);
}

std::string knoten(const Knoten& k, const Rechteck& r, const AnordnungOptionen& o)
{
    std::string aus;
    switch (k.art)
    {
        case KnotenArt::Start:
        case KnotenArt::Ende: aus = rechteck(r, "getoent", r.hoehe / 2); break;
        case KnotenArt::Verzweigung:
        {
            const Punkt m = r.mitte();
            aus = "  <polygon class=\"flaeche\" points=\"" +
                  punkte({{m.x, r.y}, {r.x + r.breite, m.y}, {m.x, r.y + r.hoehe}, {r.x, m.y}}) + "\"/>\n";
            break;
        }
        case KnotenArt::Aufruf:  // ein Rechteck mit doppelten Seiten
            aus = rechteck(r, "flaeche");
            aus += "  <polyline class=\"linie\" points=\"" + punkte({{r.x + 8, r.y}, {r.x + 8, r.y + r.hoehe}}) + "\"/>\n";
            aus += "  <polyline class=\"linie\" points=\"" +
                   punkte({{r.x + r.breite - 8, r.y}, {r.x + r.breite - 8, r.y + r.hoehe}}) + "\"/>\n";
            break;
        case KnotenArt::Variante:
            aus = "  <polygon class=\"flaeche\" points=\"" + punkte(sechseck(r)) + "\"/>\n";
            break;
        case KnotenArt::Anweisung: aus = rechteck(r, "flaeche"); break;
    }
    std::vector<std::string> zeilen;
    for (const std::string& z : k.zeilen) zeilen.push_back(kuerze(z, o.maxZeilenLaenge));
    aus += textMittig(zeilen, r.mitte(), o.zeilenHoehe);
    // nie erreicht: grau und gestrichelt (Klasse tot)
    return k.unerreichbar ? "  <g class=\"tot\">\n" + aus + "  </g>\n" : aus;
}

std::string kasten(const Kasten& k, const Rechteck& r, const AnordnungOptionen& o)
{
    const double kopfhoehe = kopfHoehe(o);
    std::string aus = rechteck(r, "flaeche");
    aus += rechteck({r.x, r.y, r.breite, kopfhoehe}, "getoent");
    std::string_view art;
    switch (k.art)
    {
        // in Guillemets (U+00AB, U+00BB), als Bytes: die Datei bleibt reines ASCII
        case KastenArt::Struktur: art = "\xC2\xAB" "struct" "\xC2\xBB"; break;
        case KastenArt::Vereinigung: art = "\xC2\xAB" "union" "\xC2\xBB"; break;
        case KastenArt::Aufzaehlung: art = "\xC2\xAB" "enumeration" "\xC2\xBB"; break;
        case KastenArt::Schnittstelle: art = "\xC2\xAB" "interface" "\xC2\xBB"; break;
        case KastenArt::Klasse: break;
    }
    const std::string name = kuerze(k.name, o.maxZeilenLaenge);
    const double mitteX = r.x + r.breite / 2;
    if (art.empty())
        aus += textMittig({name}, {mitteX, r.y + kopfhoehe / 2}, o.zeilenHoehe, "font-weight=\"bold\"");
    else
    {
        aus += textMittig({std::string(art)}, {mitteX, r.y + 2 + kopfhoehe / 4}, o.zeilenHoehe, "font-style=\"italic\"");
        aus += textMittig({name}, {mitteX, r.y - 2 + kopfhoehe * 3 / 4}, o.zeilenHoehe, "font-weight=\"bold\"");
    }

    double y = r.y + kopfhoehe + 5;
    const auto zeile = [&](char sicht, const std::string& inhalt, bool kursiv) {
        const std::string vorn = sicht == '\0' ? std::string() : std::string(1, sicht) + " ";
        aus += textLinks(vorn + kuerze(inhalt, o.maxZeilenLaenge), r.x + 8, y, o.zeilenHoehe,
                         kursiv ? "font-style=\"italic\"" : "");
        y += o.zeilenHoehe;
    };
    for (const Eintrag& e : k.eintraege) zeile(e.sicht, e.text(), false);
    if (k.methoden.empty()) return aus;
    y += 3;
    aus += "  <polyline class=\"linie\" points=\"" + punkte({{r.x, y}, {r.x + r.breite, y}}) + "\"/>\n";
    y += 5;
    for (const Methode& m : k.methoden) zeile(m.sicht, m.text(), m.abstrakt);  // abstrakt: kursiv, wie in UML
    return aus;
}

/// Eine Tabelle: Zellen (der volle Text als Tooltip), die doppelte Linie, die Textzeilen.
std::string tabelle(const TabellenAnordnung& a, const std::string& titel, const AnordnungOptionen& o)
{
    std::string aus = kopf(titel, a.breite, a.hoehe);
    for (const TabellenAnordnung::Zelle& z : a.zellen)
    {
        std::string stil = z.kopf ? "font-weight=\"bold\"" : "";
        if (z.grau) stil += std::string(stil.empty() ? "" : " ") + "class=\"grau\"";
        std::string stueck = rechteck(z.ort, z.kopf || z.getoent ? "getoent" : "flaeche");
        if (z.links)
            stueck += textLinks(z.zeilen.empty() ? std::string() : z.zeilen.front(), z.ort.x + 8, z.ort.y + (z.ort.hoehe - o.zeilenHoehe) / 2,
                                o.zeilenHoehe, stil);
        else
            stueck += textMittig(z.zeilen, z.ort.mitte(), o.zeilenHoehe, stil);
        aus += z.ganz.empty() ? stueck : "  <g>\n  <title>" + xml(z.ganz) + "</title>\n" + stueck + "  </g>\n";
    }
    for (const Linienzug& l : a.linien) aus += "  <polyline class=\"linie\" points=\"" + punkte(l) + "\"/>\n";
    for (const TabellenAnordnung::Text& t : a.texte)
        aus += textLinks(t.text, t.links.x, t.links.y - o.zeilenHoehe / 2, o.zeilenHoehe, t.fett ? "font-weight=\"bold\"" : "");
    return aus + "</svg>\n";
}

class Svg final : public Format
{
public:
    [[nodiscard]] std::string_view name() const override { return "svg"; }
    [[nodiscard]] std::string_view endung() const override { return "svg"; }

    [[nodiscard]] std::string schreibe(const Flussdiagramm& diagramm, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        const FlussAnordnung a = ordneAn(diagramm, o);
        std::string aus = kopf(diagramm.titel(), a.breite, a.hoehe);

        // hinten die Rahmen, dann die Kanten, darauf die Knoten, vorn die Beschriftungen. Ein
        // Rahmen einer Bedingung traegt ihre Farbe (Rand, Titel, ein Hauch in der Flaeche).
        const std::vector<std::size_t> farben = farbenDerRahmen(diagramm);
        for (std::size_t g = 0; g < a.gruppen.size(); ++g)
        {
            const bool bedingung = farben[g] != kKeine;
            aus += bedingung ? rechteckMit(a.gruppen[g], "rahmen", flaechenStil(farben[g])) : rechteck(a.gruppen[g], "rahmen");
        }
        // die Titel nach den Kanten, auf einem Schild: eine Kante, die in den Rahmen laeuft, geht hinter ihm durch
        std::string titel;
        for (std::size_t g = 0; g < a.gruppen.size(); ++g)
        {
            const Rechteck& r = a.gruppen[g];
            // der Titel einer Bedingung passt in seinen Rahmen — ganz steht er in der Legende
            const std::string text =
                kuerze(diagramm.gruppen()[g].titel, farben[g] != kKeine ? titelZeichen(r, o) : optionen.maxZeilenLaenge);
            titel += rahmenTitel(text, r, o,
                                 farben[g] != kKeine ? "font-style=\"italic\" " + textStil(farben[g]) : "font-style=\"italic\"");
        }
        aus += legende(a.legende, o);
        for (const Linienzug& l : a.kanten)
        {
            if (l.size() < 2) continue;
            aus += "  <polyline class=\"linie\" points=\"" + punkte(l) + "\"/>\n";
            aus += spitze(l[l.size() - 2], l.back(), true);
        }
        aus += titel;
        for (std::size_t i = 0; i < a.knoten.size(); ++i) aus += knoten(diagramm.knoten()[i], a.knoten[i], o);
        for (std::size_t e = 0; e < diagramm.kanten().size(); ++e)
        {
            const std::string& inhalt = diagramm.kanten()[e].beschriftung;
            if (!inhalt.empty()) aus += schild(inhalt, a.beschriftungen[e], o);
        }
        return aus + "</svg>\n";
    }

    [[nodiscard]] std::string schreibe(const Strukturdiagramm& diagramm, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        const StrukturAnordnung a = ordneAn(diagramm, o);
        std::string aus = kopf(diagramm.titel, a.breite, a.hoehe);

        for (std::size_t e = 0; e < a.beziehungen.size(); ++e)
        {
            const Linienzug& l = a.beziehungen[e];
            const Beziehung& b = diagramm.beziehungen[e];
            if (l.size() < 2) continue;
            const bool realisiert = b.art == BeziehungArt::Realisiert;
            aus += "  <polyline class=\"linie\" points=\"" + punkte(l) + "\"" +
                   (realisiert ? " style=\"stroke-dasharray:6 4\"" : "") + "/>\n";
            switch (b.art)
            {
                case BeziehungArt::Enthaelt: aus += raute(l[0], l[1]); break;
                case BeziehungArt::Verweist: aus += spitze(l[l.size() - 2], l.back(), true); break;
                case BeziehungArt::Erbt:
                case BeziehungArt::Realisiert: aus += spitze(l[l.size() - 2], l.back(), false, kSpitzeErbt); break;
            }
            if (b.beschriftung.empty()) continue;
            // wo die Anordnung sie hinstellt: neben der Linie, frei von anderen Schildern und Kaesten
            aus += textMittig({kuerze(b.beschriftung, optionen.maxZeilenLaenge)}, a.beschriftungen[e], o.zeilenHoehe,
                              "font-style=\"italic\"");
        }
        for (std::size_t i = 0; i < a.kaesten.size(); ++i) aus += kasten(diagramm.kaesten[i], a.kaesten[i], o);
        return aus + "</svg>\n";
    }

    [[nodiscard]] std::string schreibe(const Zustandsdiagramm& diagramm, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        const FlussAnordnung a = ordneAn(diagramm, o);
        std::string aus = kopf(diagramm.titel, a.breite, a.hoehe);
        for (const Linienzug& l : a.kanten)
        {
            if (l.size() < 2) continue;
            aus += "  <polyline class=\"linie\" points=\"" + punkte(l) + "\"/>\n";
            aus += spitze(l[l.size() - 2], l.back(), true);
        }
        for (std::size_t z = 0; z < diagramm.zustaende.size(); ++z)
        {
            const Rechteck& r = a.knoten[z];
            if (diagramm.zustaende[z].aussen)
            {
                // der Punkt von aussen: ein gefuellter Kreis
                const Punkt m = r.mitte();
                aus += "  <circle class=\"spitze\" cx=\"" + zahl(m.x) + "\" cy=\"" + zahl(m.y) + "\" r=\"" +
                       zahl(std::min(r.breite, r.hoehe) / 2) + "\"/>\n";
                continue;
            }
            aus += rechteck(r, "flaeche", 10);
            aus += textMittig({kuerze(diagramm.zustaende[z].name, optionen.maxZeilenLaenge)}, r.mitte(), o.zeilenHoehe);
        }
        for (std::size_t e = 0; e < diagramm.uebergaenge.size(); ++e)
        {
            const std::string inhalt = kuerze(diagramm.uebergaenge[e].bedingung, optionen.maxZeilenLaenge);
            if (!inhalt.empty()) aus += schild(inhalt, a.beschriftungen[e], o);
        }
        return aus + "</svg>\n";
    }

    [[nodiscard]] bool kann(Diagrammart) const override { return true; }

    [[nodiscard]] std::string schreibe(const Struktogramm& diagramm, const Optionen& optionen) const override
    {
        using FeldArt = StruktoAnordnung::FeldArt;
        const AnordnungOptionen o = anordnung(optionen);
        const StruktoAnordnung a = ordneAn(diagramm, o);
        std::string aus = kopf(diagramm.titel, a.breite, a.hoehe);
        // die Felder der Reihe nach: ein Block vor seinen Spalten, die darueber liegen
        for (const StruktoAnordnung::Feld& f : a.felder)
        {
            std::vector<std::string> zeilen;
            for (const std::string& z : f.zeilen) zeilen.push_back(kuerze(z, o.maxZeilenLaenge));
            std::string stueck = rechteck(f.ort, f.art == FeldArt::Titel ? "getoent" : "flaeche");
            if (!zeilen.empty()) stueck += textMittig(zeilen, f.text, o.zeilenHoehe);
            aus += f.unerreichbar ? "  <g class=\"tot\">\n" + stueck + "  </g>\n" : stueck;
        }
        for (const Linienzug& l : a.linien) aus += "  <polyline class=\"linie\" points=\"" + punkte(l) + "\"/>\n";
        for (const StruktoAnordnung::Beschriftung& b : a.beschriftungen)
            aus += textMittig({kuerze(b.text, o.maxZeilenLaenge)}, b.mitte, o.zeilenHoehe);
        for (const StruktoAnordnung::Rahmen& r : a.rahmen)
            aus += rechteckMit(r.ort, "rahmen", flaechenStil(r.farbe)) +
                   rahmenTitel(kuerze(r.titel, titelZeichen(r.ort, o)), r.ort, o,
                               "font-style=\"italic\" " + textStil(r.farbe));
        aus += legende(a.legende, o);
        return aus + "</svg>\n";
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

    [[nodiscard]] std::string schreibe(const Aufrufgraph& graph, const Optionen& optionen) const override
    {
        const AnordnungOptionen o = anordnung(optionen);
        const FlussAnordnung a = ordneAn(graph, o);
        std::string aus = kopf(graph.titel, a.breite, a.hoehe);

        // hinten die Rahmen der Dateien, dann die Kanten, darauf Titel und Knoten, vorn die Beschriftungen
        for (const Rechteck& r : a.gruppen) aus += rechteck(r, "rahmen");
        aus += legende(a.legende, o);
        const std::vector<std::size_t> farben = farbenDerBedingungen(graph);
        for (std::size_t e = 0; e < a.kanten.size(); ++e)
        {
            const Linienzug& l = a.kanten[e];
            if (l.size() < 2) continue;
            // mehrdeutig gestrichelt, aus einem Zweig, der nicht gilt, gepunktet; eine Bedingung, die gilt, in ihrer Farbe
            const AufrufKante& k = graph.kanten[e];
            std::string strich;
            if (!k.gilt) strich = "stroke-dasharray:2 3;";
            else if (k.art == AufrufKantenArt::Mehrdeutig) strich = "stroke-dasharray:6 4;";
            std::string farbe;
            if (farben[e] != kKeine) farbe = std::string(rahmenFarbe(farben[e]));
            std::string stil = strich + (farbe.empty() ? "" : "stroke:" + farbe + ";");
            aus += "  <polyline class=\"linie\" points=\"" + punkte(l) + "\"" +
                   (stil.empty() ? "" : " style=\"" + stil + "\"") + "/>\n";
            aus += spitze(l[l.size() - 2], l.back(), true, kSpitze,
                          farbe.empty() ? std::string() : "style=\"fill:" + farbe + ";stroke:" + farbe + "\"");
        }
        for (std::size_t g = 0; g < a.gruppen.size() && g < graph.dateien.size(); ++g)
            aus += rahmenTitel(kuerze(graph.dateien[g], titelZeichen(a.gruppen[g], o)), a.gruppen[g], o, "font-style=\"italic\"");
        for (std::size_t i = 0; i < a.knoten.size(); ++i)
        {
            const AufrufKnoten& k = graph.knoten[i];
            const Rechteck& r = a.knoten[i];
            const std::string text = kuerze(knotenText(k), optionen.maxZeilenLaenge);
            if (k.art != AufrufKnotenArt::Funktion)
            {
                // fremd grau, ein Makro dazu gestrichelt
                aus += k.art == AufrufKnotenArt::Makro ? rechteckMit(r, "fremd", "style=\"stroke-dasharray:4 3\"")
                                                       : rechteck(r, "fremd");
                aus += textMittig({text}, r.mitte(), o.zeilenHoehe, "font-style=\"italic\"");
                continue;
            }
            // eine Funktion wie ein Aufruf im Ablauf; der Ausgangspunkt getoent und kraeftig umrandet
            aus += k.wurzel ? rechteckMit(r, "getoent", "style=\"stroke-width:2.4\"") : rechteck(r, "flaeche");
            aus += "  <polyline class=\"linie\" points=\"" + punkte({{r.x + 8, r.y}, {r.x + 8, r.y + r.hoehe}}) + "\"/>\n";
            aus += "  <polyline class=\"linie\" points=\"" +
                   punkte({{r.x + r.breite - 8, r.y}, {r.x + r.breite - 8, r.y + r.hoehe}}) + "\"/>\n";
            aus += textMittig({text}, r.mitte(), o.zeilenHoehe);
        }
        for (std::size_t e = 0; e < graph.kanten.size(); ++e)
        {
            const std::string inhalt = kuerze(beschriftung(graph.kanten[e]), optionen.maxZeilenLaenge);
            if (!inhalt.empty()) aus += schild(inhalt, a.beschriftungen[e], o);
        }
        return aus + "</svg>\n";
    }
};

}  // namespace

const Format& svg()
{
    static const Svg format;
    return format;
}

}  // namespace c2d::ausgabe
