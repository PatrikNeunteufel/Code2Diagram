/**
 * @file ErDiagramm.cpp
 * @brief Notationen der Tabellen: Namen, Vielfachheiten als Text, Chen und (min,max)
 */
#include "c2d/diagramm/ErDiagramm.hpp"

#include <algorithm>
#include <array>

namespace c2d::diagramm
{

namespace
{

constexpr ErNotationName kNotationen[] = {
    {ErNotation::Tabellen, "tabellen", "Tables"},
    {ErNotation::Kraehenfuss, "kraehenfuss", "Crow's foot"},
    {ErNotation::Chen, "chen", "Chen"},
    {ErNotation::MinMax, "minmax", "(min,max)"},
};

/// Die Buchstaben der Linien einer Raute m:n in Chen, der Reihe nach (bei mehr als vier: `n`).
constexpr std::array<std::string_view, 4> kBuchstaben = {"m", "n", "p", "q"};

std::string zahl(int n)
{
    return n == kViele ? "*" : std::to_string(n);
}

std::string_view stereotypVon(KastenArt art)
{
    switch (art)
    {
        case KastenArt::Struktur: return "struct";
        case KastenArt::Vereinigung: return "union";
        case KastenArt::Aufzaehlung: return "enumeration";
        case KastenArt::Schnittstelle: return "interface";
        case KastenArt::Tabelle: return "table";
        case KastenArt::Klasse: return "";
    }
    return "";
}

/// Ist der Kasten eine Verbindungstabelle (Entscheid N3)? Ihr Primaerschluessel hat mindestens zwei
/// Spalten, alle in Fremdschluesseln; jeder ihrer Fremdschluessel liegt ganz in ihm, mindestens zwei
/// zeigen in das Diagramm; und kein Fremdschluessel zeigt auf sie (eine Beziehung ist kein Ziel).
bool istVerbindung(const Strukturdiagramm& d, std::size_t k)
{
    const Kasten& kasten = d.kaesten[k];
    if (kasten.art != KastenArt::Tabelle) return false;
    std::size_t schluessel = 0;
    for (const Eintrag& e : kasten.eintraege)
    {
        if (!e.schluessel) continue;
        if (!e.fremd) return false;
        ++schluessel;
    }
    if (schluessel < 2) return false;
    std::size_t teilnehmer = 0;
    for (const Beziehung& b : d.beziehungen)
    {
        if (!b.fremdschluessel) continue;
        if (b.nach == k) return false;
        if (b.von != k) continue;
        for (const std::size_t s : b.spalten)
            if (!kasten.eintraege[s].schluessel) return false;
        ++teilnehmer;
    }
    return teilnehmer >= 2;
}

}  // namespace

std::span<const ErNotationName> erNotationen()
{
    return kNotationen;
}

std::string uml(Vielfachheit v)
{
    if (v.min == v.max) return zahl(v.min);
    if (v.min == 0 && v.max == kViele) return "*";
    return zahl(v.min) + ".." + zahl(v.max);
}

std::string minMax(Vielfachheit v)
{
    return "(" + zahl(v.min) + "," + zahl(v.max) + ")";
}

bool Strukturdiagramm::hatTabellen() const
{
    return std::any_of(kaesten.begin(), kaesten.end(), [](const Kasten& k) { return k.art == KastenArt::Tabelle; });
}

bool alsErDiagramm(const Strukturdiagramm& diagramm)
{
    return (diagramm.notation == ErNotation::Chen || diagramm.notation == ErNotation::MinMax) && diagramm.hatTabellen();
}

ErDiagramm erDiagrammVon(const Strukturdiagramm& d)
{
    ErDiagramm er;
    er.titel = d.titel;
    er.minMax = d.notation == ErNotation::MinMax;
    const bool attribute = d.attribute != ErAttribute::Keine;
    const bool nurSchluessel = d.attribute == ErAttribute::Schluessel;

    // je Kasten sein Knoten: eine Entitaet, eine Raute (Verbindungstabelle) oder ein Kasten
    std::vector<std::size_t> knotenVon(d.kaesten.size(), kKeine);
    std::vector<bool> verbindung(d.kaesten.size(), false);
    for (std::size_t k = 0; k < d.kaesten.size(); ++k)
    {
        const Kasten& kasten = d.kaesten[k];
        ErKnoten n;
        n.name = kasten.name;
        n.kasten = k;
        if (kasten.art == KastenArt::Tabelle)
        {
            verbindung[k] = istVerbindung(d, k);
            n.art = verbindung[k] ? ErKnotenArt::Beziehung : ErKnotenArt::Entitaet;
            for (std::size_t e = 0; e < kasten.eintraege.size() && attribute; ++e)
            {
                const Eintrag& eintrag = kasten.eintraege[e];
                // ein Fremdschluessel ist die Beziehung, kein Attribut (auch als Teil des Primaerschluessels)
                if (eintrag.fremd) continue;
                if (nurSchluessel && !eintrag.schluessel) continue;
                n.attribute.push_back({eintrag.name, eintrag.schluessel, e});
            }
        }
        else
        {
            n.art = ErKnotenArt::Kasten;
            n.stereotyp = std::string(stereotypVon(kasten.art));
            for (const Eintrag& e : kasten.eintraege)
                n.zeilen.push_back((e.sicht == '\0' ? std::string() : std::string(1, e.sicht) + " ") + e.text());
            n.felder = n.zeilen.size();
            for (const Methode& m : kasten.methoden)
                n.zeilen.push_back((m.sicht == '\0' ? std::string() : std::string(1, m.sicht) + " ") + m.text());
        }
        knotenVon[k] = er.knoten.size();
        er.knoten.push_back(std::move(n));
    }

    // je Verbindungstabelle: ihre Raute mit einer Linie zu jedem Teilnehmer, `m`, `n` … bzw. (0,*)
    std::vector<std::size_t> zaehler(d.kaesten.size(), 0);
    for (const Beziehung& b : d.beziehungen)
    {
        if (!b.fremdschluessel || !verbindung[b.von] || verbindung[b.nach]) continue;
        const std::size_t i = zaehler[b.von]++;
        ErKante kante;
        kante.von = knotenVon[b.nach];
        kante.nach = knotenVon[b.von];
        kante.textAmVon = true;
        // der erste Teilnehmer ueber der Raute, die weiteren darunter
        if (i > 0) std::swap(kante.von, kante.nach), kante.textAmVon = false;
        kante.text = er.minMax ? minMax({0, kViele}) : std::string(i < kBuchstaben.size() ? kBuchstaben[i] : "n");
        er.kanten.push_back(std::move(kante));
    }

    // je anderer Fremdschluessel eine Raute: oben die Tabelle, auf die er zeigt, unten die mit ihm
    for (const Beziehung& b : d.beziehungen)
    {
        if (!b.fremdschluessel)
        {
            const std::size_t von = knotenVon[b.von];
            const std::size_t nach = knotenVon[b.nach];
            if (er.knoten[von].art != ErKnotenArt::Kasten || er.knoten[nach].art != ErKnotenArt::Kasten) continue;
            ErKante kante;
            kante.von = von;
            kante.nach = nach;
            kante.text = b.beschriftung;
            kante.zwischenKaesten = true;
            kante.art = b.art;
            er.kanten.push_back(std::move(kante));
            continue;
        }
        if (verbindung[b.von] || verbindung[b.nach]) continue;
        ErKnoten raute;
        raute.art = ErKnotenArt::Beziehung;
        raute.name = !b.name.empty() ? b.name : b.beschriftung;
        raute.kasten = b.von;
        raute.eintrag = b.spalten.empty() ? kKeine : b.spalten.front();
        const std::size_t r = er.knoten.size();
        er.knoten.push_back(std::move(raute));
        // Chen: am Ende der Tabelle mit dem Fremdschluessel steht, wie viele von ihr je einer der anderen
        // (n, oder 1 bei eindeutigen Spalten), am anderen Ende 1. (min,max): wie oft die Entitaet teilnimmt —
        // die Tabelle, auf die er zeigt, so oft wie Zeilen auf sie zeigen; die mit ihm genau einmal
        // (NOT NULL) oder hoechstens einmal.
        ErKante oben;
        oben.von = knotenVon[b.nach];
        oben.nach = r;
        oben.textAmVon = true;
        oben.text = er.minMax ? minMax(b.amVon) : "1";
        ErKante unten;
        unten.von = r;
        unten.nach = knotenVon[b.von];
        unten.text = er.minMax ? minMax(b.amNach) : (b.amVon.max == 1 ? "1" : "n");
        er.kanten.push_back(std::move(oben));
        er.kanten.push_back(std::move(unten));
    }
    return er;
}

}  // namespace c2d::diagramm
