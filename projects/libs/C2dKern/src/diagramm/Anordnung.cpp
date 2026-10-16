/**
 * @file Anordnung.cpp
 * @brief Anordnung in Schichten, fuer Flussdiagramm und Strukturdiagramm
 */
#include "c2d/diagramm/Anordnung.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <utility>

namespace c2d::diagramm
{

namespace
{

constexpr double kRand = 24;          // um das ganze Diagramm
constexpr double kAbstandSchicht = 54;  // zwischen zwei Schichten; in der Luecke biegen die Kanten ab
constexpr double kAbstandNachbar = 34;  // zwischen zwei Knoten einer Schicht
constexpr double kAbstandStuetze = 16;  // neben einem Stuetzpunkt
constexpr double kStuetzBreite = 8;     // ein Stuetzpunkt ohne Beschriftung
constexpr double kUnterLuecke = 11;     // eine Beschriftung steht so weit unter der Luecke, in der ihre Kante abbiegt
constexpr double kSchildLuft = 8;       // nach rechts: zwischen dem Ende einer Beschriftung und dem Knoten dahinter
constexpr double kRueckAbstand = 26;    // einer Kante zurueck vom aeussersten Knoten
constexpr double kRueckStaffel = 14;    // zwischen zwei Kanten zurueck
constexpr double kSpurAbstand = 8;      // eine Kante zurueck, die unten aus- oder oben eintritt: so nah an der Zeile
constexpr double kPunkt = 18;          // ein Knoten ohne Text
constexpr double kGruppenRand = 14;
constexpr double kGruppenTitel = 22;
constexpr double kLegendenFeld = 12;   // das Farbfeld einer Zeile der Legende

using Paar = std::pair<std::size_t, std::size_t>;

/// Wie weit die i-te Schlaufe eines Knotens an sich selbst rechts von ihm verlaeuft.
constexpr double schlaufe(std::size_t i)
{
    return kRueckAbstand + kRueckStaffel * static_cast<double>(i);
}

struct Schichtung
{
    std::vector<Rechteck> orte;
    std::vector<int> rang;
    std::vector<bool> zurueck;  ///< je Kante: fuehrt sie gegen die Richtung?
    /// je Kante ueber mehrere Schichten: die Mitten ihrer Stuetzpunkte, je Schicht dazwischen einer
    std::vector<std::vector<Punkt>> stuetzen;
    std::vector<double> zeileOben;  ///< je Schicht: ihre Oberkante
    std::vector<double> luecke;     ///< je Schicht: wo darueber die Kanten abbiegen
};

/// Ordnet n Knoten der Groessen `groesse` in Schichten an, von oben nach unten.
///
/// Eine Kante ueber mehrere Schichten bekommt in jeder Schicht dazwischen einen
/// Stuetzpunkt: einen Knoten ohne Inhalt, der ihr den Platz freihaelt. So laeuft
/// sie in einer eigenen Spalte, statt durch Knoten oder aussen herum. Der erste
/// Stuetzpunkt ist `stuetzBreite[e]` breit (0: schmal) — Platz fuer eine Beschriftung.
///
/// `gruppenVon[v]`: die Rahmen, in denen Knoten v liegt (alle, auch die aeusseren).
/// Ueber der Zeile, in der ein Rahmen beginnt, bleibt Platz fuer seinen Titel und
/// Rand, unter der, in der er endet, fuer seinen Rand — so biegen die Kanten
/// ausserhalb der Rahmen ab, und ihre Beschriftungen stehen nicht auf dem Titel.
/// `zeileOben` ist die Oberkante dieses Platzes, nicht die der Knoten.
///
/// `schildLaenge[e]`: wie weit die Beschriftung von Kante e in Richtung des Ablaufs
/// reicht (0: kaum — nach unten ist sie eine Zeile hoch). Sie steht unter der Luecke,
/// in der ihre Kante abbiegt; zwischen der und der naechsten Schicht bleibt ihr Platz.
Schichtung schichte(const std::vector<Punkt>& groesse, const std::vector<Paar>& kanten,
                    const std::vector<double>& stuetzBreite = {},
                    const std::vector<std::vector<std::size_t>>& gruppenVon = {},
                    const std::vector<double>& schildLaenge = {})
{
    const std::size_t n = groesse.size();
    Schichtung s;
    s.orte.resize(n);
    s.rang.assign(n, 0);
    s.zurueck.assign(kanten.size(), false);
    s.stuetzen.resize(kanten.size());
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

    // 3. Stuetzpunkte: die Knoten n, n+1, ... ; `masse` und `rang` gelten fuer alle.
    std::vector<Punkt> masse = groesse;
    std::vector<int> rang = s.rang;
    std::vector<Paar> stuecke;  // von oben nach unten
    std::vector<std::vector<std::size_t>> stuetzIds(kanten.size());
    for (std::size_t e = 0; e < kanten.size(); ++e)
    {
        const auto [a, b] = kanten[e];
        if (s.rang[a] == s.rang[b]) continue;
        if (s.zurueck[e])
        {
            stuecke.emplace_back(b, a);  // laeuft aussen herum; zaehlt nur fuer die Reihenfolge
            continue;
        }
        std::size_t davor = a;
        for (int r = s.rang[a] + 1; r < s.rang[b]; ++r)
        {
            const bool erster = r == s.rang[a] + 1;
            const double breite = erster && e < stuetzBreite.size() && stuetzBreite[e] > 0 ? stuetzBreite[e] : kStuetzBreite;
            stuetzIds[e].push_back(masse.size());
            stuecke.emplace_back(davor, masse.size());
            davor = masse.size();
            masse.push_back({breite, 0});
            rang.push_back(r);
        }
        stuecke.emplace_back(davor, b);
    }
    const std::size_t alle = masse.size();

    std::vector<std::vector<std::size_t>> oben(alle), unten(alle);  // Nachbarn in hoeheren / tieferen Schichten
    for (const auto [hoch, tief] : stuecke)
    {
        unten[hoch].push_back(tief);
        oben[tief].push_back(hoch);
    }

    // 4. Die Schichten, zuerst in der Reihenfolge der Kanten: was ein Knoten erreicht,
    //    steht in der Schicht darunter in der Folge seiner Kanten — die Faelle eines switch
    //    in der Folge der Quelle.
    const int schichten = 1 + *std::max_element(rang.begin(), rang.end());
    std::vector<std::vector<std::size_t>> schicht(static_cast<std::size_t>(schichten));
    std::vector<bool> eingereiht(alle, false);
    const auto reiheEin = [&](std::size_t v) {
        if (eingereiht[v]) return;
        eingereiht[v] = true;
        schicht[static_cast<std::size_t>(rang[v])].push_back(v);
    };
    for (const std::size_t v : besuch)
        if (rang[v] == 0) reiheEin(v);
    for (std::size_t r = 0; r + 1 < schicht.size(); ++r)
    {
        for (std::size_t i = 0; i < schicht[r].size(); ++i)
            for (const std::size_t w : unten[schicht[r][i]])
                if (static_cast<std::size_t>(rang[w]) == r + 1) reiheEin(w);
        for (const std::size_t v : besuch)  // was von oben nicht erreicht wird
            if (static_cast<std::size_t>(rang[v]) == r + 1) reiheEin(v);
    }

    // 5. Reihenfolge je Schicht nach dem Schwerpunkt der Nachbarn, zweimal hinab und hinauf.
    std::vector<double> platz(alle, 0);
    const auto zaehleDurch = [&](const std::vector<std::size_t>& reihe) {
        for (std::size_t i = 0; i < reihe.size(); ++i) platz[reihe[i]] = static_cast<double>(i);
    };
    for (const auto& reihe : schicht) zaehleDurch(reihe);
    const auto sortiere = [&](std::vector<std::size_t>& reihe, const std::vector<std::vector<std::size_t>>& nachbarn) {
        std::vector<double> schwerpunkt(alle, 0);
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

    // 6. Orte: jede Schicht eine Zeile; ein Knoten steht moeglichst unter der Mitte seiner oberen Nachbarn.
    //    Wird ein Knoten mit genau einem oberen Nachbarn, der nur ihn hat, zur Seite gedraengt,
    //    rueckt der obere mit, und es wird neu gesetzt — so bleibt eine Kette gerade, statt in
    //    einer Luecke zu knicken, in der schon andere Kanten laufen.
    std::vector<Rechteck> orte(alle);
    std::vector<double> mindestens(alle, -1e18);  // linker Rand, nicht weiter links
    const auto abstand = [&](std::size_t a, std::size_t b) {
        return a >= n || b >= n ? kAbstandStuetze : kAbstandNachbar;
    };
    // Biegt eine beschriftete Kante aus der Schicht darueber in einen Knoten, steht ihre
    // Beschriftung ueber ihm: so breit wie sie ist sein Platz in der Zeile (der Knoten in der Mitte).
    std::vector<double> breite(alle, 0);
    for (std::size_t v = 0; v < alle; ++v) breite[v] = masse[v].x;
    for (std::size_t e = 0; e < kanten.size() && e < stuetzBreite.size(); ++e)
    {
        const auto [a, b] = kanten[e];
        if (!s.zurueck[e] && s.rang[b] == s.rang[a] + 1) breite[b] = std::max(breite[b], stuetzBreite[e]);
    }
    // Eine Schlaufe an sich selbst haengt rechts am Knoten (ordneAn): ihr und ihrer
    // Beschriftung bleibt dort Platz, sonst liefe eine lange Kante daneben durch sie.
    std::vector<double> rechts(alle, 0);
    std::vector<std::size_t> schlaufen(n, 0);
    for (std::size_t e = 0; e < kanten.size(); ++e)
    {
        const auto [a, b] = kanten[e];
        if (a != b) continue;
        const double schild = e < stuetzBreite.size() ? stuetzBreite[e] / 2 : 0;
        rechts[a] = std::max(rechts[a], schlaufe(schlaufen[a]++) + schild);
    }
    // je Schicht: wie viele Rahmen in ihr beginnen (Platz darueber) und enden (darunter) — hoechstens
    std::vector<double> vorher(schicht.size(), 0), nachher(schicht.size(), 0);
    {
        std::size_t gruppen = 0;
        for (const auto& liste : gruppenVon)
            for (const std::size_t g : liste) gruppen = std::max(gruppen, g + 1);
        std::vector<int> erste(gruppen, std::numeric_limits<int>::max()), letzte(gruppen, -1);
        for (std::size_t v = 0; v < gruppenVon.size() && v < n; ++v)
            for (const std::size_t g : gruppenVon[v])
            {
                erste[g] = std::min(erste[g], s.rang[v]);
                letzte[g] = std::max(letzte[g], s.rang[v]);
            }
        for (std::size_t v = 0; v < gruppenVon.size() && v < n; ++v)
        {
            const auto r = static_cast<std::size_t>(s.rang[v]);
            const auto beginnen = std::count_if(gruppenVon[v].begin(), gruppenVon[v].end(),
                                                [&](std::size_t g) { return erste[g] == s.rang[v]; });
            const auto enden = std::count_if(gruppenVon[v].begin(), gruppenVon[v].end(),
                                             [&](std::size_t g) { return letzte[g] == s.rang[v]; });
            vorher[r] = std::max(vorher[r], static_cast<double>(beginnen) * (kGruppenRand + kGruppenTitel));
            nachher[r] = std::max(nachher[r], static_cast<double>(enden) * kGruppenRand);
        }
    }
    // je Schicht: wie weit ueber ihr die Kanten abbiegen — so weit, dass die Beschriftungen
    // darunter (Kanten aus der Schicht davor) vor ihr enden
    std::vector<double> unterLuecke(schicht.size(), kAbstandSchicht / 2);
    for (std::size_t e = 0; e < kanten.size() && e < schildLaenge.size(); ++e)
    {
        const auto [a, b] = kanten[e];
        if (schildLaenge[e] <= 0 || s.rang[b] <= s.rang[a]) continue;
        const auto r = static_cast<std::size_t>(s.rang[a]) + 1;
        if (r < unterLuecke.size()) unterLuecke[r] = std::max(unterLuecke[r], kUnterLuecke + schildLaenge[e] + kSchildLuft);
    }
    const auto setze = [&] {
        s.zeileOben.clear();
        s.luecke.clear();
        double y = 0;
        for (std::size_t r = 0; r < schicht.size(); ++r)
        {
            const auto& reihe = schicht[r];
            if (r > 0) y += unterLuecke[r] - kAbstandSchicht / 2;  // die Luecke davor reicht weiter
            s.zeileOben.push_back(y);
            s.luecke.push_back(y - unterLuecke[r]);
            y += vorher[r];
            double hoehe = 0;
            for (const std::size_t v : reihe) hoehe = std::max(hoehe, masse[v].y);
            double frei = 0;  // rechter Rand des Knotens davor
            for (std::size_t i = 0; i < reihe.size(); ++i)
            {
                const std::size_t v = reihe[i];
                const double rand = (breite[v] - masse[v].x) / 2;  // zwischen Platz und Knoten, je Seite
                double wunsch = frei + breite[v] / 2;
                if (!oben[v].empty())
                {
                    double summe = 0;
                    for (const std::size_t w : oben[v]) summe += orte[w].mitte().x;
                    wunsch = summe / static_cast<double>(oben[v].size());
                }
                double links = std::max(wunsch - breite[v] / 2, mindestens[v] - rand);  // der linke Rand des Platzes
                if (i > 0) links = std::max(links, frei + abstand(reihe[i - 1], v));
                orte[v] = {links + rand, y + (hoehe - masse[v].y) / 2, masse[v].x, masse[v].y};
                frei = links + breite[v] + rechts[v];
            }
            y += hoehe + nachher[r] + kAbstandSchicht;
        }
    };
    setze();
    for (std::size_t runde = 0; runde < 2 * schicht.size() + 4; ++runde)
    {
        bool geaendert = false;
        for (std::size_t v = 0; v < alle; ++v)
        {
            if (oben[v].size() != 1) continue;
            const std::size_t u = oben[v].front();
            if (unten[u].size() != 1 || rang[u] + 1 != rang[v]) continue;
            const double versatz = orte[v].mitte().x - orte[u].mitte().x;
            if (versatz < 0.5) continue;
            mindestens[u] = orte[u].x + versatz;
            geaendert = true;
        }
        if (!geaendert) break;
        setze();
    }

    // an den Rand ruecken
    double links = orte[0].x;
    for (const Rechteck& r : orte) links = std::min(links, r.x);
    for (Rechteck& r : orte)
    {
        r.x += kRand - links;
        r.y += kRand;
    }
    for (double& oberkante : s.zeileOben) oberkante += kRand;
    for (double& biege : s.luecke) biege += kRand;
    s.orte.assign(orte.begin(), orte.begin() + static_cast<std::ptrdiff_t>(n));
    for (std::size_t e = 0; e < kanten.size(); ++e)
        for (const std::size_t d : stuetzIds[e]) s.stuetzen[e].push_back(orte[d].mitte());
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
    // ohne Text: ein Punkt (der Ausgangspunkt eines Zustandsdiagramms)
    if (std::all_of(k.zeilen.begin(), k.zeilen.end(), [](const std::string& z) { return z.empty(); }))
        return {kPunkt, kPunkt};
    const double text = static_cast<double>(std::max<std::size_t>(laengsteZeile(k.zeilen, o.maxZeilenLaenge), 3)) * o.zeichenBreite;
    const double zeilen = static_cast<double>(std::max<std::size_t>(k.zeilen.size(), 1)) * o.zeilenHoehe;
    switch (k.art)
    {
        case KnotenArt::Verzweigung: return {text * 1.35 + 56, zeilen + 40};  // der Text steht in einer Raute
        case KnotenArt::Start:
        case KnotenArt::Ende: return {text + 44, zeilen + 16};
        case KnotenArt::Aufruf: return {text + 44, zeilen + 18};  // Platz fuer die doppelten Raender
        case KnotenArt::Variante: return {text + 26 + 2 * kSechseckSpitze, zeilen + 18};
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

std::vector<Punkt> sechseck(const Rechteck& r)
{
    const double s = std::min(kSechseckSpitze, r.breite / 4);
    const double m = r.y + r.hoehe / 2;
    return {{r.x + s, r.y}, {r.x + r.breite - s, r.y}, {r.x + r.breite, m},
            {r.x + r.breite - s, r.y + r.hoehe}, {r.x + s, r.y + r.hoehe}, {r.x, m}};
}

std::size_t titelZeichen(const Rechteck& rahmen, const AnordnungOptionen& optionen)
{
    constexpr std::size_t kWenigstens = 8;
    const double frei = rahmen.breite - 16;  // links 8 Einzug, rechts 8 Luft
    const auto passen = frei > 0 ? static_cast<std::size_t>(frei / optionen.zeichenBreite) : 0;
    const std::size_t zeichen = std::max(passen, kWenigstens);
    return optionen.maxZeilenLaenge == 0 ? zeichen : std::min(zeichen, optionen.maxZeilenLaenge);
}

double kopfHoehe(const AnordnungOptionen& optionen)
{
    return 2 * optionen.zeilenHoehe + 10;
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
    // Der erste Stuetzpunkt einer langen Kante haelt ihrer Beschriftung den Platz frei.
    // Nach rechts steht sie laengs der Kante: dann braucht sie Platz in der Luecke.
    std::vector<Paar> paare;
    std::vector<double> stuetzBreite;
    std::vector<double> schildLaenge;
    for (const Kante& k : kanten)
    {
        paare.emplace_back(k.von, k.nach);
        const double breite = static_cast<double>(k.beschriftung.size()) * optionen.zeichenBreite + 10;
        stuetzBreite.push_back(k.beschriftung.empty() ? 0 : drehen ? optionen.zeilenHoehe + 6 : breite);
        schildLaenge.push_back(k.beschriftung.empty() || !drehen ? 0 : breite);
    }
    // Platz fuer Titel und Rand der Rahmen — nach unten gerechnet; nach rechts steht der Titel
    // oben quer zur Richtung und braucht keinen.
    std::vector<std::vector<std::size_t>> gruppenVon(drehen ? 0 : knoten.size());
    for (std::size_t v = 0; v < gruppenVon.size(); ++v)
        for (std::size_t g = knoten[v].gruppe; g < diagramm.gruppen().size(); g = diagramm.gruppen()[g].eltern)
            gruppenVon[v].push_back(g);
    const Schichtung s = schichte(groesse, paare, stuetzBreite, gruppenVon, schildLaenge);
    a.knoten = s.orte;

    const auto entlang = [](Punkt von, Punkt nach, double weit) {
        const double laenge = std::hypot(nach.x - von.x, nach.y - von.y);
        if (laenge < 1) return von;
        const double t = std::clamp(weit, 0.0, laenge) / laenge;
        return Punkt{von.x + (nach.x - von.x) * t, von.y + (nach.y - von.y) * t};
    };

    std::size_t staffel = 0;
    std::vector<std::size_t> schlaufen(knoten.size(), 0);  // je Knoten: seine Schlaufen bisher
    for (std::size_t e = 0; e < kanten.size(); ++e)
    {
        const Rechteck& von = a.knoten[kanten[e].von];
        const Rechteck& nach = a.knoten[kanten[e].nach];
        const int rangVon = s.rang[kanten[e].von];
        const int rangNach = s.rang[kanten[e].nach];
        Linienzug linie;
        Punkt ort;
        if (rangNach > rangVon)
        {
            // hinab: unten aus der Quelle, durch die Stuetzpunkte, oben ins Ziel. Die Spalte
            // wechselt nur in der Luecke ueber einer Schicht — dort, wo nichts steht.
            const Punkt p{von.mitte().x, von.y + von.hoehe};
            const Punkt q{nach.mitte().x, nach.y};
            const auto luecke = [&](int r) { return s.luecke[static_cast<std::size_t>(r)]; };
            linie.push_back(p);
            double x = p.x;
            const auto biege = [&](double ziel, int r) {
                if (std::abs(ziel - x) < 1) return;
                linie.push_back({x, luecke(r)});
                linie.push_back({ziel, luecke(r)});
                x = ziel;
            };
            int r = rangVon;
            for (const Punkt& stuetze : s.stuetzen[e]) biege(stuetze.x, ++r);
            biege(q.x, rangNach);
            linie.push_back(q);

            // Die Beschriftung steht dort, wo die Kante ihren eigenen Weg geht: gleich unter
            // der Luecke, in der Kanten aus derselben Quelle sich trennen — biegt sie dort
            // ab, auf ihrer eigenen Senkrechten, sonst auf der Senkrechten unter der Quelle.
            // Nach rechts beginnt sie dort (sie steht laengs), nach unten steht sie mittig.
            const double ersteLuecke = luecke(rangVon + 1);
            const double weiter = kUnterLuecke + schildLaenge[e] / 2;
            if (linie.size() > 2 && linie[1].y <= ersteLuecke + 1)
                ort = entlang(linie[2], linie[3], weiter);
            else
                ort = entlang(linie[0], linie[1], ersteLuecke + weiter - p.y);
        }
        else
        {
            // zurueck (oder in der eigenen Schicht): rechts aussen herum — rechts an allem, was in
            // den Schichten dazwischen steht, auch an den Spalten langer Kanten. Eine Schlaufe an
            // sich selbst bleibt am Knoten, im Platz, den schichte() ihr freigehalten hat.
            const bool selbst = kanten[e].von == kanten[e].nach;
            double aussen = 0;
            if (selbst)
                aussen = von.x + von.breite + schlaufe(schlaufen[kanten[e].von]++);
            else
            {
                for (std::size_t v = 0; v < knoten.size(); ++v)
                    if (s.rang[v] >= rangNach && s.rang[v] <= rangVon)
                        aussen = std::max(aussen, a.knoten[v].x + a.knoten[v].breite);
                for (std::size_t f = 0; f < kanten.size(); ++f)
                    for (std::size_t i = 0; i < s.stuetzen[f].size(); ++i)
                    {
                        const int r = s.rang[kanten[f].von] + 1 + static_cast<int>(i);
                        const double halb = i == 0 ? std::max(stuetzBreite[f], kStuetzBreite) / 2 : kStuetzBreite / 2;
                        if (r >= rangNach && r <= rangVon) aussen = std::max(aussen, s.stuetzen[f][i].x + halb);
                    }
                aussen += kRueckAbstand + kRueckStaffel * static_cast<double>(staffel++);
            }
            // Steht in der Schicht rechts daneben noch ein Knoten, ginge die Linie waagrecht durch
            // ihn: dann unten aus der Quelle (oben ins Ziel) und auf einer Spur knapp an der Zeile
            // entlang — nicht in der Mitte der Luecke, wo Kanten abbiegen und Beschriftungen stehen.
            const auto rechtsFrei = [&](KnotenId v) {
                for (KnotenId u = 0; u < knoten.size(); ++u)
                    if (u != v && s.rang[u] == s.rang[v] && a.knoten[u].x > a.knoten[v].x) return false;
                return true;
            };
            if (selbst || rechtsFrei(kanten[e].von))
            {
                const Punkt p{von.x + von.breite, von.y + von.hoehe * (selbst ? 0.7 : 0.5)};
                linie = {p, {aussen, p.y}};
            }
            else
            {
                const Punkt p{von.mitte().x, von.y + von.hoehe};
                const std::size_t darunter = static_cast<std::size_t>(rangVon) + 1;
                const double spur = darunter < s.luecke.size() ? s.luecke[darunter] - kAbstandSchicht / 2 + kSpurAbstand
                                                               : p.y + kSpurAbstand;
                linie = {p, {p.x, spur}, {aussen, spur}};
            }
            if (selbst || rechtsFrei(kanten[e].nach))
            {
                const Punkt q{nach.x + nach.breite, nach.y + nach.hoehe * (selbst ? 0.3 : 0.5)};
                linie.push_back({aussen, q.y});
                linie.push_back(q);
            }
            else
            {
                const Punkt q{nach.mitte().x, nach.y};
                const double spur = s.zeileOben[static_cast<std::size_t>(rangNach)] - kSpurAbstand;
                linie.push_back({aussen, spur});
                linie.push_back({q.x, spur});
                linie.push_back(q);
            }
            ort = {aussen, (linie[1].y + linie[linie.size() - 2].y) / 2};
        }
        a.kanten.push_back(std::move(linie));
        a.beschriftungen.push_back(ort);
    }

    if (drehen)
    {
        for (Rechteck& r : a.knoten) tausche(r);
        for (Linienzug& l : a.kanten)
            for (Punkt& p : l) std::swap(p.x, p.y);
        for (Punkt& p : a.beschriftungen) std::swap(p.x, p.y);
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
    // eine Schlaufe an sich selbst gehoert zu ihrem Knoten: samt Beschriftung in seinen Rahmen
    for (std::size_t e = 0; e < kanten.size(); ++e)
    {
        const std::size_t g = knoten[kanten[e].von].gruppe;
        if (kanten[e].von != kanten[e].nach || g >= a.gruppen.size()) continue;
        for (const Punkt& p : a.kanten[e]) umfasse(g, {p.x, p.y, 0, 0});
        const double breite = static_cast<double>(kanten[e].beschriftung.size()) * optionen.zeichenBreite + 4;
        const Punkt m = a.beschriftungen[e];
        if (!kanten[e].beschriftung.empty())
            umfasse(g, {m.x - breite / 2, m.y - optionen.zeilenHoehe / 2, breite, optionen.zeilenHoehe});
    }
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
    // eine Beschriftung, geschaetzt wie ein Knotentext
    std::vector<Rechteck> schilder;
    for (std::size_t e = 0; e < kanten.size(); ++e)
    {
        if (kanten[e].beschriftung.empty()) continue;
        const double breite = static_cast<double>(kanten[e].beschriftung.size()) * optionen.zeichenBreite + 4;
        const Punkt m = a.beschriftungen[e];
        schilder.push_back({m.x - breite / 2, m.y - optionen.zeilenHoehe / 2, breite, optionen.zeilenHoehe});
        links = std::min(links, schilder.back().x - kRand);
        oben = std::min(oben, schilder.back().y - kRand);
    }
    for (Rechteck& r : schilder) r = {r.x - links, r.y - oben, r.breite, r.hoehe};
    const auto schiebe = [&](Rechteck& r) {
        r.x -= links;
        r.y -= oben;
    };
    for (Rechteck& r : a.knoten) schiebe(r);
    for (Rechteck& r : a.gruppen) schiebe(r);
    for (Linienzug& l : a.kanten)
        for (Punkt& p : l) p = {p.x - links, p.y - oben};
    for (Punkt& p : a.beschriftungen) p = {p.x - links, p.y - oben};

    const auto weite = [&](const Rechteck& r) {
        a.breite = std::max(a.breite, r.x + r.breite + kRand);
        a.hoehe = std::max(a.hoehe, r.y + r.hoehe + kRand);
    };
    for (const Rechteck& r : a.knoten) weite(r);
    for (const Rechteck& r : a.gruppen) weite(r);
    for (const Rechteck& r : schilder) weite(r);
    for (const Linienzug& l : a.kanten)
        for (const Punkt& p : l) weite({p.x, p.y, 0, 0});

    // die Legende der Bedingungen: rechts neben allem, oben
    const std::vector<std::size_t> farben = farbenDerRahmen(diagramm);
    std::vector<std::string_view> genannt;
    const double links0 = a.breite;
    double y = kRand;
    for (std::size_t g = 0; g < farben.size(); ++g)
    {
        if (farben[g] == kKeine) continue;
        const std::string& titel = diagramm.gruppen()[g].titel;
        if (std::find(genannt.begin(), genannt.end(), titel) != genannt.end()) continue;
        genannt.push_back(titel);
        const double zeichen = static_cast<double>(laengsteZeile({titel}, optionen.maxZeilenLaenge));
        FlussAnordnung::Legende eintrag{{links0, y + (optionen.zeilenHoehe - kLegendenFeld) / 2, kLegendenFeld, kLegendenFeld},
                                        {links0 + kLegendenFeld + 6, y + optionen.zeilenHoehe / 2},
                                        g};
        a.breite = std::max(a.breite, eintrag.text.x + zeichen * optionen.zeichenBreite + kRand);
        a.legende.push_back(eintrag);
        y += optionen.zeilenHoehe + 4;
    }
    a.hoehe = std::max(a.hoehe, y + kRand);
    return a;
}

FlussAnordnung ordneAn(const Zustandsdiagramm& diagramm, const AnordnungOptionen& optionen)
{
    // dieselbe Anordnung wie ein Ablauf: Zustaende als runde Knoten, der Punkt von aussen ohne Text
    Flussdiagramm alsFluss(diagramm.titel);
    for (const Zustand& z : diagramm.zustaende)
        alsFluss.neuerKnoten(Knoten{KnotenArt::Start, {z.aussen ? std::string() : z.name}, z.zeile, {}, kKeine});
    for (const Uebergang& u : diagramm.uebergaenge)
    {
        // so lang, wie ein Format sie gekuerzt zeigt
        std::string beschriftung = u.bedingung;
        if (optionen.maxZeilenLaenge != 0 && beschriftung.size() > optionen.maxZeilenLaenge)
            beschriftung.resize(optionen.maxZeilenLaenge);
        alsFluss.verbinde(u.von, u.nach, beschriftung);
    }
    return ordneAn(alsFluss, optionen);
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
