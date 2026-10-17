/**
 * @file Flussdiagramm.cpp
 * @brief Das Diagramm-Modell: Buendeln von Anweisungsfolgen, Einbetten und Herausloesen
 */
#include "c2d/diagramm/Flussdiagramm.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace c2d::diagramm
{

namespace
{

void nimmAuf(std::vector<std::string>& ziel, const std::vector<std::string>& quelle)
{
    for (const std::string& name : quelle)
        if (std::find(ziel.begin(), ziel.end(), name) == ziel.end()) ziel.push_back(name);
}

}  // namespace

Flussdiagramm::Flussdiagramm(std::string titel) : m_titel(std::move(titel)) {}

KnotenId Flussdiagramm::neuerKnoten(KnotenArt art, std::string_view text, int quellZeile)
{
    Knoten k;
    k.art = art;
    k.zeilen.emplace_back(text);
    k.quellZeile = quellZeile;
    return neuerKnoten(std::move(k));
}

KnotenId Flussdiagramm::neuerKnoten(Knoten knoten)
{
    m_knoten.push_back(std::move(knoten));
    return m_knoten.size() - 1;
}

void Flussdiagramm::verbinde(KnotenId von, KnotenId nach, std::string_view beschriftung)
{
    if (von >= m_knoten.size() || nach >= m_knoten.size())
        throw std::out_of_range("Flussdiagramm::verbinde: unknown node");
    m_kanten.push_back({von, nach, std::string(beschriftung)});
}

void Flussdiagramm::ergaenzeZeile(KnotenId knoten, std::string_view zeile, bool vorn)
{
    if (knoten >= m_knoten.size()) throw std::out_of_range("Flussdiagramm::ergaenzeZeile: no such node");
    std::vector<std::string>& zeilen = m_knoten[knoten].zeilen;
    zeilen.emplace(vorn ? zeilen.begin() : zeilen.end(), zeile);
}

void Flussdiagramm::markiereUnerreichbar(KnotenId knoten)
{
    if (knoten >= m_knoten.size()) throw std::out_of_range("Flussdiagramm::markiereUnerreichbar: no such node");
    m_knoten[knoten].unerreichbar = true;
}

std::size_t Flussdiagramm::neueGruppe(Gruppe gruppe)
{
    m_gruppen.push_back(std::move(gruppe));
    return m_gruppen.size() - 1;
}

bool Flussdiagramm::liegtIn(std::size_t innen, std::size_t aussen) const
{
    for (std::size_t g = innen; g < m_gruppen.size(); g = m_gruppen[g].eltern)
        if (g == aussen) return true;
    return false;
}

std::size_t Flussdiagramm::einbettungVon(std::size_t gruppe) const
{
    for (std::size_t g = gruppe; g < m_gruppen.size(); g = m_gruppen[g].eltern)
        if (m_gruppen[g].art == GruppenArt::Einbettung) return g;
    return kKeine;
}

std::size_t Flussdiagramm::einbettungen() const
{
    return static_cast<std::size_t>(std::count_if(m_gruppen.begin(), m_gruppen.end(), [](const Gruppe& g) {
        return g.art == GruppenArt::Einbettung;
    }));
}

std::vector<std::size_t> farbenDerRahmen(const Flussdiagramm& diagramm)
{
    std::vector<std::size_t> aus;
    std::vector<std::string_view> bedingungen;  // in der Reihenfolge ihres ersten Vorkommens
    for (const Gruppe& g : diagramm.gruppen())
    {
        if (g.art != GruppenArt::Bedingung)
        {
            aus.push_back(kKeine);
            continue;
        }
        auto stelle = std::find(bedingungen.begin(), bedingungen.end(), g.titel);
        if (stelle == bedingungen.end()) stelle = bedingungen.insert(bedingungen.end(), g.titel);
        aus.push_back(static_cast<std::size_t>(stelle - bedingungen.begin()) % kRahmenFarben);
    }
    return aus;
}

std::string_view rahmenFarbe(std::size_t farbe)
{
    // blau, orange, gruen, violett, rot, ocker — kraeftig genug fuer einen Rand auf Hell und Dunkel
    static constexpr std::string_view kFarben[kRahmenFarben] = {"#3b82f6", "#f08c2e", "#22a37a",
                                                                "#9b5de5", "#e5484d", "#c49a1c"};
    return kFarben[farbe % kRahmenFarben];
}

Flussdiagramm fasseAnweisungsfolgenZusammen(const Flussdiagramm& diagramm, std::size_t maxZeilen)
{
    const std::vector<Knoten>& knoten = diagramm.knoten();
    const std::vector<Kante>& kanten = diagramm.kanten();
    const std::size_t n = knoten.size();

    std::vector<std::size_t> ausgaenge(n, 0), eingaenge(n, 0);
    for (const Kante& k : kanten)
    {
        ++ausgaenge[k.von];
        ++eingaenge[k.nach];
    }

    // nachfolger[a] = b: b darf in a aufgehen
    std::vector<std::size_t> nachfolger(n, kKeine);
    std::vector<bool> hatVorgaenger(n, false);
    for (const Kante& k : kanten)
    {
        const bool beideAnweisung = knoten[k.von].art == KnotenArt::Anweisung &&
                                    knoten[k.nach].art == KnotenArt::Anweisung &&
                                    knoten[k.von].gruppe == knoten[k.nach].gruppe;
        if (beideAnweisung && k.von != k.nach && k.beschriftung.empty() && ausgaenge[k.von] == 1 &&
            eingaenge[k.nach] == 1)
        {
            nachfolger[k.von] = k.nach;
            hatVorgaenger[k.nach] = true;
        }
    }

    Flussdiagramm neu(diagramm.titel());
    std::vector<KnotenId> neueId(n, kKeine);
    std::vector<bool> aufgegangen(n, false);

    const auto buendle = [&](std::size_t kopf) {
        Knoten ziel = knoten[kopf];
        neueId[kopf] = neu.knoten().size();
        for (std::size_t glied = nachfolger[kopf]; glied != kKeine && neueId[glied] == kKeine;
             glied = nachfolger[glied])
        {
            const std::vector<std::string>& zeilen = knoten[glied].zeilen;
            if (maxZeilen != 0 && ziel.zeilen.size() + zeilen.size() > maxZeilen)
            {
                // voll: das Glied beginnt einen eigenen Knoten, die Kante dazwischen bleibt
                neu.neuerKnoten(std::move(ziel));
                ziel = knoten[glied];
                neueId[glied] = neu.knoten().size();
                continue;
            }
            ziel.zeilen.insert(ziel.zeilen.end(), zeilen.begin(), zeilen.end());
            nimmAuf(ziel.aufrufe, knoten[glied].aufrufe);
            neueId[glied] = neu.knoten().size();
            aufgegangen[glied] = true;
        }
        neu.neuerKnoten(std::move(ziel));
    };

    for (std::size_t i = 0; i < n; ++i)
        if (!hatVorgaenger[i]) buendle(i);
    // Was jetzt noch fehlt, liegt auf einem Kreis nur aus Anweisungen (ohne Kopf).
    for (std::size_t i = 0; i < n; ++i)
        if (neueId[i] == kKeine) buendle(i);

    for (const Kante& k : kanten)
    {
        if (nachfolger[k.von] == k.nach && aufgegangen[k.nach]) continue;
        neu.verbinde(neueId[k.von], neueId[k.nach], k.beschriftung);
    }
    for (Gruppe g : diagramm.gruppen())
    {
        if (g.aufrufer != kKeine) g.aufrufer = neueId[g.aufrufer];
        for (Kante& k : g.ersetzt) k = {neueId[k.von], neueId[k.nach], k.beschriftung};
        neu.neueGruppe(std::move(g));
    }
    return neu;
}

Flussdiagramm betteEin(const Flussdiagramm& haupt, KnotenId stelle, const Flussdiagramm& unter, std::string_view titel,
                       std::string_view herkunft)
{
    const auto innen = [&](KnotenId i) {
        return unter.knoten()[i].art != KnotenArt::Start && unter.knoten()[i].art != KnotenArt::Ende;
    };
    std::size_t inhalt = 0;
    for (KnotenId i = 0; i < unter.knoten().size(); ++i) inhalt += innen(i) ? 1 : 0;
    const bool besetzt = std::any_of(haupt.gruppen().begin(), haupt.gruppen().end(),
                                     [&](const Gruppe& g) { return g.aufrufer == stelle; });
    if (stelle >= haupt.knoten().size() || besetzt || inhalt == 0) return haupt;

    // Was dem Aufrufer bisher folgte, folgt kuenftig den Enden von `unter`.
    Gruppe rahmen;
    rahmen.titel = std::string(titel);
    rahmen.herkunft = std::string(herkunft);
    rahmen.eltern = haupt.knoten()[stelle].gruppe;
    rahmen.aufrufer = stelle;
    Flussdiagramm neu(haupt.titel());
    for (const Knoten& k : haupt.knoten()) neu.neuerKnoten(k);
    for (const Gruppe& g : haupt.gruppen()) neu.neueGruppe(g);
    for (const Kante& k : haupt.kanten())
    {
        if (k.von == stelle)
            rahmen.ersetzt.push_back(k);
        else
            neu.verbinde(k.von, k.nach, k.beschriftung);
    }
    const std::vector<Kante> danach = rahmen.ersetzt;
    const std::size_t gruppe = neu.neueGruppe(std::move(rahmen));
    const std::size_t basis = gruppe + 1;  // dort beginnen die Gruppen aus `unter`

    std::vector<KnotenId> neueId(unter.knoten().size(), kKeine);
    for (KnotenId i = 0; i < unter.knoten().size(); ++i)
    {
        if (!innen(i)) continue;
        Knoten k = unter.knoten()[i];
        k.gruppe = k.gruppe == kKeine ? gruppe : basis + k.gruppe;
        neueId[i] = neu.neuerKnoten(std::move(k));
    }

    // Eine Kante aus `unter` im neuen Diagramm: vom Start = vom Aufrufer, ins Ende = zu dem, was folgte.
    const auto uebersetze = [&](const Kante& k) {
        std::vector<Kante> aus;
        const Knoten& von = unter.knoten()[k.von];
        const Knoten& nach = unter.knoten()[k.nach];
        if (von.art == KnotenArt::Ende || nach.art == KnotenArt::Start) return aus;
        const KnotenId quelle = von.art == KnotenArt::Start ? stelle : neueId[k.von];
        if (nach.art != KnotenArt::Ende)
            aus.push_back({quelle, neueId[k.nach], k.beschriftung});
        else
            for (const Kante& d : danach)
                aus.push_back({quelle, d.nach, k.beschriftung.empty() ? d.beschriftung : k.beschriftung});
        return aus;
    };
    for (const Kante& k : unter.kanten())
        for (const Kante& u : uebersetze(k)) neu.verbinde(u.von, u.nach, u.beschriftung);

    for (Gruppe g : unter.gruppen())
    {
        g.eltern = g.eltern == kKeine ? gruppe : basis + g.eltern;
        g.aufrufer = g.aufrufer == kKeine ? kKeine : neueId[g.aufrufer];
        std::vector<Kante> ersetzt;
        for (const Kante& k : g.ersetzt)
            for (const Kante& u : uebersetze(k)) ersetzt.push_back(u);
        g.ersetzt = std::move(ersetzt);
        neu.neueGruppe(std::move(g));
    }
    return neu;
}

Flussdiagramm loeseEin(const Flussdiagramm& diagramm, std::size_t gruppe)
{
    if (gruppe >= diagramm.gruppen().size()) return diagramm;

    const auto faelltWeg = [&](std::size_t g) { return g != kKeine && diagramm.liegtIn(g, gruppe); };

    std::vector<std::size_t> neueGruppe(diagramm.gruppen().size(), kKeine);
    std::size_t zaehler = 0;
    for (std::size_t g = 0; g < diagramm.gruppen().size(); ++g)
        if (!faelltWeg(g)) neueGruppe[g] = zaehler++;

    Flussdiagramm neu(diagramm.titel());
    std::vector<KnotenId> neueId(diagramm.knoten().size(), kKeine);
    for (KnotenId i = 0; i < diagramm.knoten().size(); ++i)
    {
        Knoten k = diagramm.knoten()[i];
        if (faelltWeg(k.gruppe)) continue;
        if (k.gruppe != kKeine) k.gruppe = neueGruppe[k.gruppe];
        neueId[i] = neu.neuerKnoten(std::move(k));
    }

    const auto uebernimm = [&](const Kante& k) {
        if (neueId[k.von] != kKeine && neueId[k.nach] != kKeine)
            neu.verbinde(neueId[k.von], neueId[k.nach], k.beschriftung);
    };
    for (const Kante& k : diagramm.kanten()) uebernimm(k);
    for (const Kante& k : diagramm.gruppen()[gruppe].ersetzt) uebernimm(k);  // der Aufrufer bekommt seine Kanten zurueck

    for (std::size_t g = 0; g < diagramm.gruppen().size(); ++g)
    {
        if (faelltWeg(g)) continue;
        Gruppe kopie = diagramm.gruppen()[g];
        if (kopie.eltern != kKeine) kopie.eltern = neueGruppe[kopie.eltern];
        if (kopie.aufrufer != kKeine) kopie.aufrufer = neueId[kopie.aufrufer];
        std::vector<Kante> ersetzt;
        for (const Kante& k : kopie.ersetzt)
            if (neueId[k.von] != kKeine && neueId[k.nach] != kKeine)
                ersetzt.push_back({neueId[k.von], neueId[k.nach], k.beschriftung});
        kopie.ersetzt = std::move(ersetzt);
        neu.neueGruppe(std::move(kopie));
    }
    return neu;
}

}  // namespace c2d::diagramm
