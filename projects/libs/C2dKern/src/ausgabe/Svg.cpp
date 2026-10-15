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

std::string kopf(std::string_view titel, double breite, double hoehe)
{
    const std::string b = zahl(breite);
    const std::string h = zahl(hoehe);
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + b + "\" height=\"" + h + "\" viewBox=\"0 0 " + b +
           " " + h + "\" font-family=\"Consolas, 'Courier New', monospace\" font-size=\"12\">\n"
           "  <title>" + xml(titel) + "</title>\n"
           "  <style>\n"
           "    .grund, .flaeche, .hohl, .schild { fill: #ffffff; }\n"
           "    .getoent { fill: #ececec; }\n"
           "    .flaeche, .getoent, .linie, .spitze, .hohl { stroke: #202020; stroke-width: 1.2; }\n"
           "    .linie { fill: none; }\n"
           "    .spitze { fill: #202020; }\n"
           "    .rahmen { fill: none; stroke: #8a8a8a; stroke-width: 1.2; stroke-dasharray: 5 4; }\n"
           "    text { fill: #202020; }\n"
           "    @media (prefers-color-scheme: dark) {\n"
           "      .grund, .flaeche, .hohl, .schild { fill: #2b2b2b; }\n"
           "      .getoent { fill: #3a3a3a; }\n"
           "      .flaeche, .getoent, .linie, .spitze, .hohl { stroke: #dcdcdc; }\n"
           "      .spitze { fill: #dcdcdc; }\n"
           "      text { fill: #dcdcdc; }\n"
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

/// Ein Dreieck an `an`, das von `von` her zeigt.
std::string spitze(Punkt von, Punkt an, bool gefuellt, double laenge = kSpitze)
{
    const double winkel = std::atan2(an.y - von.y, an.x - von.x);
    const auto ecke = [&](double drehung) {
        return Punkt{an.x - laenge * std::cos(winkel + drehung), an.y - laenge * std::sin(winkel + drehung)};
    };
    return "  <polygon class=\"" + std::string(gefuellt ? "spitze" : "hohl") + "\" points=\"" +
           punkte({an, ecke(kSpitzeWinkel), ecke(-kSpitzeWinkel)}) + "\"/>\n";
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
        case KnotenArt::Anweisung: aus = rechteck(r, "flaeche"); break;
    }
    std::vector<std::string> zeilen;
    for (const std::string& z : k.zeilen) zeilen.push_back(kuerze(z, o.maxZeilenLaenge));
    return aus + textMittig(zeilen, r.mitte(), o.zeilenHoehe);
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

        // hinten die Rahmen, dann die Kanten, darauf die Knoten, vorn die Beschriftungen
        for (std::size_t g = 0; g < a.gruppen.size(); ++g)
        {
            const Rechteck& r = a.gruppen[g];
            aus += rechteck(r, "rahmen");
            aus += textLinks(kuerze(diagramm.gruppen()[g].titel, optionen.maxZeilenLaenge), r.x + 8, r.y + 4,
                             o.zeilenHoehe, "font-style=\"italic\"");
        }
        for (const Linienzug& l : a.kanten)
        {
            if (l.size() < 2) continue;
            aus += "  <polyline class=\"linie\" points=\"" + punkte(l) + "\"/>\n";
            aus += spitze(l[l.size() - 2], l.back(), true);
        }
        for (std::size_t i = 0; i < a.knoten.size(); ++i) aus += knoten(diagramm.knoten()[i], a.knoten[i], o);
        for (std::size_t e = 0; e < diagramm.kanten().size(); ++e)
        {
            const std::string& inhalt = diagramm.kanten()[e].beschriftung;
            if (inhalt.empty()) continue;
            const Punkt m = a.beschriftungen[e];
            const double breite = static_cast<double>(zeichen(inhalt)) * o.zeichenBreite + 4;
            aus += rechteck({m.x - breite / 2, m.y - kSchildHoehe / 2, breite, kSchildHoehe}, "schild");
            aus += textMittig({inhalt}, m, o.zeilenHoehe);
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
            aus += "  <polyline class=\"linie\" points=\"" + punkte(l) + "\"/>\n";
            switch (b.art)
            {
                case BeziehungArt::Enthaelt: aus += raute(l[0], l[1]); break;
                case BeziehungArt::Verweist: aus += spitze(l[l.size() - 2], l.back(), true); break;
                case BeziehungArt::Erbt: aus += spitze(l[l.size() - 2], l.back(), false, kSpitzeErbt); break;
            }
            if (b.beschriftung.empty()) continue;
            // wie in der Vorschau: neben der Mitte der Linie
            const Punkt p = l[l.size() / 2 - 1];
            const Punkt q = l[l.size() / 2];
            aus += textLinks(kuerze(b.beschriftung, optionen.maxZeilenLaenge), (p.x + q.x) / 2 + 5, (p.y + q.y) / 2 - 15,
                             o.zeilenHoehe, "font-style=\"italic\"");
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
            if (inhalt.empty()) continue;
            const Punkt m = a.beschriftungen[e];
            const double breite = static_cast<double>(zeichen(inhalt)) * o.zeichenBreite + 4;
            aus += rechteck({m.x - breite / 2, m.y - kSchildHoehe / 2, breite, kSchildHoehe}, "schild");
            aus += textMittig({inhalt}, m, o.zeilenHoehe);
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
