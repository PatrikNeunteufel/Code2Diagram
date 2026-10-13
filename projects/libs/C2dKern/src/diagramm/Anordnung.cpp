/**
 * @file Anordnung.cpp
 * @brief Anordnung in Schichten, fuer Flussdiagramm und Strukturdiagramm
 */
#include "c2d/diagramm/Anordnung.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace c2d::diagramm
{

namespace
{

constexpr double kRand = 24;          // um das ganze Diagramm
constexpr double kAbstandSchicht = 46;  // zwischen zwei Schichten
constexpr double kAbstandNachbar = 34;  // zwischen zwei Knoten einer Schicht
constexpr double kRueckAbstand = 26;    // einer Kante zurueck vom aeussersten Knoten
constexpr double kRueckStaffel = 14;    // zwischen zwei Kanten zurueck
constexpr double kGruppenRand = 14;
constexpr double kGruppenTitel = 22;

using Paar = std::pair<std::size_t, std::size_t>;

struct Schichtung
{
    std::vector<Rechteck> orte;
    std::vector<int> rang;
    std::vector<bool> zurueck;  ///< je Kante: fuehrt sie gegen die Richtung?
};

/// Ordnet n Knoten der Groessen `groesse` in Schichten an, von oben nach unten.
Schichtung schichte(const std::vector<Punkt>& groesse, const std::vector<Paar>& kanten)
{
    const std::size_t n = groesse.size();
    Schichtung s;
    s.orte.resize(n);
    s.rang.assign(n, 0);
    s.zurueck.assign(kanten.size(), false);
    if (n == 0) return s;

    std::vector<std::vector<std::size_t>> hinaus(n);
    for (std::size_t e = 0; e < kanten.size(); ++e) hinaus[kanten[e].first].push_back(e);

    // 1. Tiefensuche: eine Kante zu einem Knoten auf dem eigenen Weg fuehrt zurueck.
    std::vector<int> zustand(n, 0);  // 0 neu, 1 auf dem Weg, 2 fertig
    std::vector<std::size_t> besuch;  // Reihenfolge des ersten Besuchs
    besuch.reserve(n);
    for (std::size_t wurzel = 0; wurzel < n; ++wurzel)
    {
        if (zustand[wurzel] != 0) continue;
        std::vector<std::pair<std::size_t, std::size_t>> stapel{{wurzel, 0}};
        zustand[wurzel] = 1;
        besuch.push_back(wurzel);
        while (!stapel.empty())
        {
            auto& [v, naechste] = stapel.back();
            if (naechste >= hinaus[v].size())
            {
                zustand[v] = 2;
                stapel.pop_back();
                continue;
            }
            const std::size_t e = hinaus[v][naechste++];
            const std::size_t w = kanten[e].second;
            if (zustand[w] == 1)
                s.zurueck[e] = true;
            else if (zustand[w] == 0)
            {
                zustand[w] = 1;
                besuch.push_back(w);
                stapel.emplace_back(w, 0);
            }
        }
    }

    // 2. Rang = laengster Weg ueber die Kanten, die nicht zurueckfuehren.
    std::vector<std::size_t> offen(n, 0);
    for (std::size_t e = 0; e < kanten.size(); ++e)
        if (!s.zurueck[e]) ++offen[kanten[e].second];
    std::vector<std::size_t> bereit;
    for (const std::size_t v : besuch)
        if (offen[v] == 0) bereit.push_back(v);
    for (std::size_t i = 0; i < bereit.size(); ++i)
    {
        const std::size_t v = bereit[i];
        for (const std::size_t e : hinaus[v])
        {
            if (s.zurueck[e]) continue;
            const std::size_t w = kanten[e].second;
            s.rang[w] = std::max(s.rang[w], s.rang[v] + 1);
            if (--offen[w] == 0) bereit.push_back(w);
        }
    }

    // 3. Die Schichten, zuerst in der Reihenfolge des Besuchs.
    const int schichten = 1 + *std::max_element(s.rang.begin(), s.rang.end());
    std::vector<std::vector<std::size_t>> schicht(static_cast<std::size_t>(schichten));
    for (const std::size_t v : besuch) schicht[static_cast<std::size_t>(s.rang[v])].push_back(v);

    std::vector<std::vector<std::size_t>> oben(n), unten(n);  // Nachbarn in hoeheren / tieferen Schichten
    for (std::size_t e = 0; e < kanten.size(); ++e)
    {
        const auto [a, b] = kanten[e];
        if (s.rang[a] == s.rang[b]) continue;
        const std::size_t hoch = s.rang[a] < s.rang[b] ? a : b;
        const std::size_t tief = hoch == a ? b : a;
        unten[hoch].push_back(tief);
        oben[tief].push_back(hoch);
    }

    // 4. Reihenfolge je Schicht nach dem Schwerpunkt der Nachbarn, zweimal hinab und hinauf.
    std::vector<double> platz(n, 0);
    const auto zaehleDurch = [&](const std::vector<std::size_t>& reihe) {
        for (std::size_t i = 0; i < reihe.size(); ++i) platz[reihe[i]] = static_cast<double>(i);
    };
    for (const auto& reihe : schicht) zaehleDurch(reihe);
    const auto sortiere = [&](std::vector<std::size_t>& reihe, const std::vector<std::vector<std::size_t>>& nachbarn) {
        std::vector<double> schwerpunkt(n, 0);
        for (const std::size_t v : reihe)
        {
            double summe = 0;
            for (const std::size_t w : nachbarn[v]) summe += platz[w];
            schwerpunkt[v] = nachbarn[v].empty() ? platz[v] : summe / static_cast<double>(nachbarn[v].size());
        }
        std::stable_sort(reihe.begin(), reihe.end(),
                         [&](std::size_t a, std::size_t b) { return schwerpunkt[a] < schwerpunkt[b]; });
        zaehleDurch(reihe);
    };
    for (int runde = 0; runde < 2; ++runde)
    {
        for (std::size_t i = 1; i < schicht.size(); ++i) sortiere(schicht[i], oben);
        for (std::size_t i = schicht.size(); i-- > 1;) sortiere(schicht[i - 1], unten);
    }

    // 5. Orte: jede Schicht eine Zeile; ein Knoten steht moeglichst unter der Mitte seiner oberen Nachbarn.
    double y = 0;
    for (const auto& reihe : schicht)
    {
        double hoehe = 0;
        for (const std::size_t v : reihe) hoehe = std::max(hoehe, groesse[v].y);
        double frei = 0;
        bool erster = true;
        for (const std::size_t v : reihe)
        {
            double wunsch = frei + groesse[v].x / 2;
            if (!oben[v].empty())
            {
                double summe = 0;
                for (const std::size_t w : oben[v]) summe += s.orte[w].mitte().x;
                wunsch = summe / static_cast<double>(oben[v].size());
            }
            const double links = erster ? wunsch - groesse[v].x / 2 : std::max(wunsch - groesse[v].x / 2, frei);
            s.orte[v] = {links, y + (hoehe - groesse[v].y) / 2, groesse[v].x, groesse[v].y};
            frei = links + groesse[v].x + kAbstandNachbar;
            erster = false;
        }
        y += hoehe + kAbstandSchicht;
    }

    // an den Rand ruecken
    double links = s.orte[0].x;
    for (const Rechteck& r : s.orte) links = std::min(links, r.x);
    for (Rechteck& r : s.orte)
    {
        r.x += kRand - links;
        r.y += kRand;
    }
    return s;
}

std::size_t laengsteZeile(const std::vector<std::string>& zeilen, std::size_t hoechstens)
{
    std::size_t laenge = 0;
    for (const std::string& z : zeilen) laenge = std::max(laenge, z.size());
    return hoechstens == 0 ? laenge : std::min(laenge, hoechstens);
}

Punkt groesseVon(const Knoten& k, const AnordnungOptionen& o)
{
    const double text = static_cast<double>(std::max<std::size_t>(laengsteZeile(k.zeilen, o.maxZeilenLaenge), 3)) * o.zeichenBreite;
    const double zeilen = static_cast<double>(std::max<std::size_t>(k.zeilen.size(), 1)) * o.zeilenHoehe;
    switch (k.art)
    {
        case KnotenArt::Verzweigung: return {text * 1.35 + 56, zeilen + 40};  // der Text steht in einer Raute
        case KnotenArt::Start:
        case KnotenArt::Ende: return {text + 44, zeilen + 16};
        case KnotenArt::Aufruf: return {text + 44, zeilen + 18};  // Platz fuer die doppelten Raender
        case KnotenArt::Anweisung: break;
    }
    return {text + 26, zeilen + 18};
}

void tausche(Rechteck& r)
{
    std::swap(r.x, r.y);
    std::swap(r.breite, r.hoehe);
}

/// Der Punkt auf dem Rand von `r`, in dem die Strecke von seiner Mitte nach `ziel` ihn verlaesst.
Punkt randpunkt(const Rechteck& r, Punkt ziel)
{
    const Punkt m = r.mitte();
    const double dx = ziel.x - m.x;
    const double dy = ziel.y - m.y;
    if (dx == 0 && dy == 0) return m;
    const double fx = dx == 0 ? 1e18 : (r.breite / 2) / std::abs(dx);
    const double fy = dy == 0 ? 1e18 : (r.hoehe / 2) / std::abs(dy);
    const double f = std::min(fx, fy);
    return {m.x + dx * f, m.y + dy * f};
}

}  // namespace

double kopfHoehe(const AnordnungOptionen& optionen)
{
    return 2 * optionen.zeilenHoehe + 10;
}

Punkt beschriftungsOrt(const Linienzug& linie)
{
    if (linie.empty()) return {};
    if (linie.size() == 1) return linie[0];
    // Mit Knick: auf dem Stueck hinter dem ersten Knick — zwei Kanten aus einem
    // Knoten teilen das erste Stueck, erst danach trennen sie sich.
    if (linie.size() > 2) return {(linie[1].x + linie[2].x) / 2, (linie[1].y + linie[2].y) / 2};
    // Gerade: nah an der Quelle, so steht die Beschriftung an ihrer Verzweigung.
    const Punkt a = linie[0];
    const Punkt b = linie[1];
    const double laenge = std::hypot(b.x - a.x, b.y - a.y);
    const double t = laenge > 60 ? 16 / laenge : 0.4;
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

FlussAnordnung ordneAn(const Flussdiagramm& diagramm, const AnordnungOptionen& optionen)
{
    const std::vector<Knoten>& knoten = diagramm.knoten();
    const std::vector<Kante>& kanten = diagramm.kanten();
    FlussAnordnung a;
    if (knoten.empty()) return a;

    // Gerechnet wird immer nach unten; nach rechts heisst: mit getauschten Seiten rechnen, dann drehen.
    const bool drehen = optionen.nachRechts;
    std::vector<Punkt> groesse;
    for (const Knoten& k : knoten)
    {
        const Punkt g = groesseVon(k, optionen);
        groesse.push_back(drehen ? Punkt{g.y, g.x} : g);
    }
    std::vector<Paar> paare;
    for (const Kante& k : kanten) paare.emplace_back(k.von, k.nach);
    const Schichtung s = schichte(groesse, paare);
    a.knoten = s.orte;

    // Liegt zwischen den Schichten von `von` und `nach` ein Knoten auf der Senkrechten bei x?
    const auto versperrt = [&](double x, int von, int nach) {
        for (std::size_t v = 0; v < knoten.size(); ++v)
            if (s.rang[v] > von && s.rang[v] < nach && x > a.knoten[v].x - 6 && x < a.knoten[v].x + a.knoten[v].breite + 6)
                return true;
        return false;
    };

    std::size_t staffel = 0;
    std::size_t staffelLinks = 0;
    for (std::size_t e = 0; e < kanten.size(); ++e)
    {
        const Rechteck& von = a.knoten[kanten[e].von];
        const Rechteck& nach = a.knoten[kanten[e].nach];
        const int rangVon = s.rang[kanten[e].von];
        const int rangNach = s.rang[kanten[e].nach];
        Linienzug linie;
        if (rangNach > rangVon)
        {
            // hinab: unten aus der Quelle, oben ins Ziel; bei Versatz mit zwei Knicken
            const Punkt p{von.mitte().x, von.y + von.hoehe};
            const Punkt q{nach.mitte().x, nach.y};
            const double oben = p.y + kAbstandSchicht / 2;
            const double unten = q.y - kAbstandSchicht / 2;
            if (rangNach == rangVon + 1)
                linie = std::abs(p.x - q.x) < 1 ? Linienzug{p, q} : Linienzug{p, {p.x, oben}, {q.x, oben}, q};
            else if (!versperrt(p.x, rangVon, rangNach))
                linie = std::abs(p.x - q.x) < 1 ? Linienzug{p, q} : Linienzug{p, {p.x, unten}, {q.x, unten}, q};
            else if (!versperrt(q.x, rangVon, rangNach))
                linie = {p, {p.x, oben}, {q.x, oben}, q};
            else
            {
                // ueber mehrere Schichten, und beide Senkrechten sind versperrt: links aussen herum
                double aussen = std::min(von.x, nach.x);
                for (std::size_t v = 0; v < knoten.size(); ++v)
                    if (s.rang[v] > rangVon && s.rang[v] < rangNach) aussen = std::min(aussen, a.knoten[v].x);
                aussen -= kRueckAbstand + kRueckStaffel * static_cast<double>(staffelLinks++);
                linie = {p, {p.x, oben}, {aussen, oben}, {aussen, unten}, {q.x, unten}, q};
            }
        }
        else
        {
            // zurueck (oder in der eigenen Schicht): rechts aussen herum
            double aussen = 0;
            for (std::size_t v = 0; v < knoten.size(); ++v)
                if (s.rang[v] >= rangNach && s.rang[v] <= rangVon)
                    aussen = std::max(aussen, a.knoten[v].x + a.knoten[v].breite);
            aussen += kRueckAbstand + kRueckStaffel * static_cast<double>(staffel++);
            const bool selbst = kanten[e].von == kanten[e].nach;
            const Punkt p{von.x + von.breite, von.y + von.hoehe * (selbst ? 0.7 : 0.5)};
            const Punkt q{nach.x + nach.breite, nach.y + nach.hoehe * (selbst ? 0.3 : 0.5)};
            linie = {p, {aussen, p.y}, {aussen, q.y}, q};
        }
        a.kanten.push_back(std::move(linie));
    }

    if (drehen)
    {
        for (Rechteck& r : a.knoten) tausche(r);
        for (Linienzug& l : a.kanten)
            for (Punkt& p : l) std::swap(p.x, p.y);
    }

    // Rahmen der Gruppen: um ihre Knoten und um die Gruppen in ihnen — die inneren zuerst.
    a.gruppen.assign(diagramm.gruppen().size(), {});
    std::vector<bool> belegt(a.gruppen.size(), false);
    const auto umfasse = [&](std::size_t g, const Rechteck& r) {
        Rechteck& rahmen = a.gruppen[g];
        if (!belegt[g])
        {
            rahmen = r;
            belegt[g] = true;
            return;
        }
        const double x2 = std::max(rahmen.x + rahmen.breite, r.x + r.breite);
        const double y2 = std::max(rahmen.y + rahmen.hoehe, r.y + r.hoehe);
        rahmen.x = std::min(rahmen.x, r.x);
        rahmen.y = std::min(rahmen.y, r.y);
        rahmen.breite = x2 - rahmen.x;
        rahmen.hoehe = y2 - rahmen.y;
    };
    for (std::size_t v = 0; v < knoten.size(); ++v)
        if (knoten[v].gruppe < a.gruppen.size()) umfasse(knoten[v].gruppe, a.knoten[v]);
    for (std::size_t g = a.gruppen.size(); g-- > 0;)  // eine Gruppe steht immer hinter ihren Eltern
    {
        if (!belegt[g]) continue;
        Rechteck& r = a.gruppen[g];
        r = {r.x - kGruppenRand, r.y - kGruppenRand - kGruppenTitel, r.breite + 2 * kGruppenRand,
             r.hoehe + 2 * kGruppenRand + kGruppenTitel};
        const std::size_t eltern = diagramm.gruppen()[g].eltern;
        if (eltern < a.gruppen.size()) umfasse(eltern, r);
    }

    // Alles ins Bild: ein Rahmen oder eine Kante aussen herum kann ueber den Rand hinausragen.
    double links = 0, oben = 0;
    for (const Rechteck& r : a.gruppen)
    {
        links = std::min(links, r.x - kRand);
        oben = std::min(oben, r.y - kRand);
    }
    for (const Linienzug& l : a.kanten)
        for (const Punkt& p : l)
        {
            links = std::min(links, p.x - kRand);
            oben = std::min(oben, p.y - kRand);
        }
    const auto schiebe = [&](Rechteck& r) {
        r.x -= links;
        r.y -= oben;
    };
    for (Rechteck& r : a.knoten) schiebe(r);
    for (Rechteck& r : a.gruppen) schiebe(r);
    for (Linienzug& l : a.kanten)
        for (Punkt& p : l) p = {p.x - links, p.y - oben};

    const auto weite = [&](const Rechteck& r) {
        a.breite = std::max(a.breite, r.x + r.breite + kRand);
        a.hoehe = std::max(a.hoehe, r.y + r.hoehe + kRand);
    };
    for (const Rechteck& r : a.knoten) weite(r);
    for (const Rechteck& r : a.gruppen) weite(r);
    for (const Linienzug& l : a.kanten)
        for (const Punkt& p : l) weite({p.x, p.y, 0, 0});
    return a;
}

StrukturAnordnung ordneAn(const Strukturdiagramm& diagramm, const AnordnungOptionen& optionen)
{
    StrukturAnordnung a;
    if (diagramm.kaesten.empty()) return a;

    const auto gekuerzt = [&](std::size_t laenge) {
        return optionen.maxZeilenLaenge == 0 ? laenge : std::min(laenge, optionen.maxZeilenLaenge);
    };
    std::vector<Punkt> groesse;
    for (const Kasten& k : diagramm.kaesten)
    {
        std::size_t laenge = std::max<std::size_t>(k.name.size(), 12);
        for (const Eintrag& e : k.eintraege) laenge = std::max(laenge, gekuerzt(e.text().size() + 2));
        for (const Methode& m : k.methoden) laenge = std::max(laenge, gekuerzt(m.text().size() + 2));
        double hoehe = kopfHoehe(optionen) + static_cast<double>(k.eintraege.size()) * optionen.zeilenHoehe + 10;
        if (!k.methoden.empty()) hoehe += static_cast<double>(k.methoden.size()) * optionen.zeilenHoehe + 8;
        const Punkt g{static_cast<double>(laenge) * optionen.zeichenBreite + 24, hoehe};
        groesse.push_back(optionen.nachRechts ? Punkt{g.y, g.x} : g);
    }
    // Schichten: die Basis ueber dem, was von ihr erbt; der Besitzer ueber dem, was er enthaelt oder nennt.
    std::vector<Paar> paare;
    for (const Beziehung& b : diagramm.beziehungen)
    {
        if (b.von == b.nach) continue;
        paare.emplace_back(b.art == BeziehungArt::Erbt ? Paar{b.nach, b.von} : Paar{b.von, b.nach});
    }
    a.kaesten = schichte(groesse, paare).orte;
    if (optionen.nachRechts)
        for (Rechteck& r : a.kaesten) tausche(r);

    std::size_t schleifen = 0;
    for (const Beziehung& b : diagramm.beziehungen)
    {
        const Rechteck& von = a.kaesten[b.von];
        const Rechteck& nach = a.kaesten[b.nach];
        if (b.von == b.nach)
        {
            // auf sich selbst: eine Schlaufe an der rechten Seite
            const double aussen = von.x + von.breite + 22 + 12 * static_cast<double>(schleifen++);
            const double y1 = von.y + von.hoehe * 0.35;
            const double y2 = von.y + von.hoehe * 0.65;
            a.beziehungen.push_back({{von.x + von.breite, y1}, {aussen, y1}, {aussen, y2}, {von.x + von.breite, y2}});
            continue;
        }
        // die wievielte Beziehung zwischen diesen beiden? Jede weitere laeuft daneben.
        std::size_t wievielte = 0;
        for (std::size_t i = 0; i < a.beziehungen.size(); ++i)
        {
            const Beziehung& davor = diagramm.beziehungen[i];
            if ((davor.von == b.von && davor.nach == b.nach) || (davor.von == b.nach && davor.nach == b.von)) ++wievielte;
        }
        Punkt p = randpunkt(von, nach.mitte());
        Punkt q = randpunkt(nach, von.mitte());
        if (wievielte != 0)
        {
            // quer zur Linie versetzen, abwechselnd nach beiden Seiten
            const double laenge = std::max(1.0, std::hypot(q.x - p.x, q.y - p.y));
            const double weit = 22.0 * static_cast<double>((wievielte + 1) / 2) * (wievielte % 2 == 1 ? 1 : -1);
            const Punkt quer{-(q.y - p.y) / laenge * weit, (q.x - p.x) / laenge * weit};
            p = {p.x + quer.x, p.y + quer.y};
            q = {q.x + quer.x, q.y + quer.y};
        }
        a.beziehungen.push_back({p, q});
    }

    const auto weite = [&](double x, double y) {
        a.breite = std::max(a.breite, x + kRand);
        a.hoehe = std::max(a.hoehe, y + kRand);
    };
    for (const Rechteck& r : a.kaesten) weite(r.x + r.breite, r.y + r.hoehe);
    for (const Linienzug& l : a.beziehungen)
        for (const Punkt& p : l) weite(p.x, p.y);
    return a;
}

}  // namespace c2d::diagramm
