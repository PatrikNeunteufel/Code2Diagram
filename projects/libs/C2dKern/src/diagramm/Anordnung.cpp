/**
 * @file Anordnung.cpp
 * @brief Anordnung in Schichten, fuer Flussdiagramm und Strukturdiagramm
 */
#include "c2d/diagramm/Anordnung.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <set>
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

/// Die Mitten der Knoten in ihren Zeilen nach Brandes und Koepf (Fast and Simple Horizontal
/// Coordinate Assignment, 2001), in der Form von dagre: vier Laeufe — nach oben und nach unten
/// ausgerichtet, je von links und von rechts verdichtet —, zuletzt je Knoten das Mittel der beiden
/// mittleren Werte. Ein Knoten richtet sich in einem Lauf nach dem mittleren seiner Nachbarn in der
/// Zeile davor; Ketten stehen so senkrecht, und keine Seite zieht das Bild einseitig.
///
/// `schicht`: die Zeilen in ihrer Reihenfolge; `oben`/`unten`: die Nachbarn in der Zeile direkt
/// darueber / darunter; `halbLinks`/`halbRechts`: wie weit der Platz eines Knotens links / rechts
/// seiner Mitte reicht; `abstand(a, b)`: zwischen zwei Nachbarn einer Zeile; `stuetze[v]`: v ist ein
/// Stuetzpunkt (zwei Stuetzpunkte uebereinander sind ein inneres Stueck einer langen Kante, das
/// Vorrang hat — Konflikte vom Typ 1).
std::vector<double> brandesKoepf(const std::vector<std::vector<std::size_t>>& schicht,
                                 const std::vector<std::vector<std::size_t>>& oben,
                                 const std::vector<std::vector<std::size_t>>& unten, const std::vector<double>& halbLinks,
                                 const std::vector<double>& halbRechts, const auto& abstand,
                                 const std::vector<bool>& stuetze)
{
    const std::size_t alle = halbLinks.size();
    std::vector<std::size_t> stelle(alle, 0);
    for (const auto& reihe : schicht)
        for (std::size_t i = 0; i < reihe.size(); ++i) stelle[reihe[i]] = i;

    // Konflikte vom Typ 1: ein Stueck zwischen Knoten und Stuetzpunkt, das ein inneres Stueck kreuzt
    std::set<std::pair<std::size_t, std::size_t>> konflikt;  // (oben, unten)
    for (std::size_t r = 1; r < schicht.size(); ++r)
    {
        const std::size_t davorLaenge = schicht[r - 1].size();
        const std::vector<std::size_t>& reihe = schicht[r];
        std::size_t k0 = 0, gesehen = 0;
        for (std::size_t i = 0; i < reihe.size(); ++i)
        {
            const std::size_t v = reihe[i];
            std::size_t innen = kKeine;
            if (stuetze[v])
                for (const std::size_t u : oben[v])
                    if (stuetze[u]) innen = u;
            const std::size_t k1 = innen != kKeine ? stelle[innen] : davorLaenge;
            if (innen == kKeine && i + 1 != reihe.size()) continue;
            for (std::size_t j = gesehen; j <= i; ++j)
                for (const std::size_t u : oben[reihe[j]])
                    if ((stelle[u] < k0 || k1 < stelle[u]) && !(stuetze[u] && stuetze[reihe[j]])) konflikt.insert({u, reihe[j]});
            gesehen = i + 1;
            k0 = k1;
        }
    }
    const auto hatKonflikt = [&](std::size_t a, std::size_t b) { return konflikt.contains({a, b}) || konflikt.contains({b, a}); };

    std::array<std::vector<double>, 4> lagen;
    for (std::size_t lauf = 0; lauf < 4; ++lauf)
    {
        const bool nachOben = lauf < 2;     // nach den Nachbarn darueber ausrichten, Zeilen von oben
        const bool vonRechts = lauf % 2 == 1;  // die Zeilen von rechts her verdichten
        std::vector<std::vector<std::size_t>> zeilen = schicht;
        if (!nachOben) std::reverse(zeilen.begin(), zeilen.end());
        if (vonRechts)
            for (auto& z : zeilen) std::reverse(z.begin(), z.end());
        std::vector<std::size_t> pos(alle, 0);
        for (const auto& z : zeilen)
            for (std::size_t i = 0; i < z.size(); ++i) pos[z[i]] = i;
        const auto& nachbarn = nachOben ? oben : unten;

        // senkrecht ausrichten: jeder Knoten an einen mittleren Nachbarn, ohne Kreuzung, ohne Konflikt
        std::vector<std::size_t> wurzel(alle), weiter(alle);
        std::iota(wurzel.begin(), wurzel.end(), std::size_t{0});
        std::iota(weiter.begin(), weiter.end(), std::size_t{0});
        for (const auto& z : zeilen)
        {
            long zuletzt = -1;
            for (const std::size_t v : z)
            {
                std::vector<std::size_t> ns = nachbarn[v];
                if (ns.empty()) continue;
                std::sort(ns.begin(), ns.end(), [&](std::size_t a, std::size_t b) { return pos[a] < pos[b]; });
                const std::size_t m1 = (ns.size() - 1) / 2, m2 = ns.size() / 2;
                for (std::size_t m = m1; m <= m2; ++m)
                {
                    const std::size_t u = ns[m];
                    if (weiter[v] == v && zuletzt < static_cast<long>(pos[u]) && !hatKonflikt(u, v))
                    {
                        weiter[u] = v;
                        wurzel[v] = wurzel[u];
                        weiter[v] = wurzel[v];
                        zuletzt = static_cast<long>(pos[u]);
                    }
                }
            }
        }

        // waagrecht verdichten: je Block (gleiche Wurzel) eine Lage, so weit links wie die Abstaende
        // zu den Bloecken davor erlauben, dann so weit rechts, wie die danach es zulassen
        std::vector<std::vector<std::pair<std::size_t, double>>> vor(alle), nach(alle);
        for (const auto& z : zeilen)
            for (std::size_t i = 1; i < z.size(); ++i)
            {
                const std::size_t a = z[i - 1], b = z[i];
                // in Wirklichkeit steht a links von b — von rechts her umgekehrt
                const double sep = vonRechts ? halbLinks[a] + abstand(b, a) + halbRechts[b]
                                             : halbRechts[a] + abstand(a, b) + halbLinks[b];
                const std::size_t ra = wurzel[a], rb = wurzel[b];
                auto it = std::find_if(nach[ra].begin(), nach[ra].end(), [&](const auto& k) { return k.first == rb; });
                if (it == nach[ra].end())
                {
                    nach[ra].emplace_back(rb, sep);
                    vor[rb].emplace_back(ra, sep);
                }
                else
                {
                    it->second = std::max(it->second, sep);
                    for (auto& k : vor[rb])
                        if (k.first == ra) k.second = std::max(k.second, sep);
                }
            }
        // die Bloecke in einer Folge, in der jeder nach denen kommt, die links von ihm stehen
        std::vector<std::size_t> offen(alle, 0), folge;
        for (std::size_t v = 0; v < alle; ++v)
            if (wurzel[v] == v) offen[v] = vor[v].size();
        for (std::size_t v = 0; v < alle; ++v)
            if (wurzel[v] == v && offen[v] == 0) folge.push_back(v);
        for (std::size_t i = 0; i < folge.size(); ++i)
            for (const auto& [b, sep] : nach[folge[i]])
                if (--offen[b] == 0) folge.push_back(b);
        std::vector<double> xs(alle, 0);
        for (const std::size_t v : folge)
            for (const auto& [a, sep] : vor[v]) xs[v] = std::max(xs[v], xs[a] + sep);
        for (std::size_t i = folge.size(); i-- > 0;)
        {
            const std::size_t v = folge[i];
            double grenze = std::numeric_limits<double>::infinity();
            for (const auto& [b, sep] : nach[v]) grenze = std::min(grenze, xs[b] - sep);
            if (grenze < std::numeric_limits<double>::infinity()) xs[v] = std::max(xs[v], grenze);
        }
        lagen[lauf].resize(alle);
        for (std::size_t v = 0; v < alle; ++v) lagen[lauf][v] = vonRechts ? -xs[wurzel[v]] : xs[wurzel[v]];
    }

    // die vier Lagen aneinander ausrichten: an der schmalsten — die von links an ihrem linken, die
    // von rechts an ihrem rechten Rand —, dann je Knoten das Mittel der beiden mittleren Werte
    const auto rand = [&](const std::vector<double>& x, bool rechtsRand) {
        double wert = rechtsRand ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
        for (std::size_t v = 0; v < alle; ++v)
            wert = rechtsRand ? std::max(wert, x[v] + halbRechts[v]) : std::min(wert, x[v] - halbLinks[v]);
        return wert;
    };
    std::size_t schmalste = 0;
    for (std::size_t lauf = 1; lauf < 4; ++lauf)
        if (rand(lagen[lauf], true) - rand(lagen[lauf], false) <
            rand(lagen[schmalste], true) - rand(lagen[schmalste], false))
            schmalste = lauf;
    const double ziel0 = rand(lagen[schmalste], false), ziel1 = rand(lagen[schmalste], true);
    for (std::size_t lauf = 0; lauf < 4; ++lauf)
    {
        const bool vonRechts = lauf % 2 == 1;
        const double versatz = vonRechts ? ziel1 - rand(lagen[lauf], true) : ziel0 - rand(lagen[lauf], false);
        for (double& x : lagen[lauf]) x += versatz;
    }
    std::vector<double> mitte(alle, 0);
    for (std::size_t v = 0; v < alle; ++v)
    {
        std::array<double, 4> werte{lagen[0][v], lagen[1][v], lagen[2][v], lagen[3][v]};
        std::sort(werte.begin(), werte.end());
        mitte[v] = (werte[1] + werte[2]) / 2;
    }
    return mitte;
}

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
///
/// `bandVon[v]` (leer: keine Baender): das Band, in dem Knoten v steht, 0, 1, ... In jeder
/// Schicht stehen die Knoten eines Bands beisammen, die Baender in ihrer Reihenfolge, und
/// jedes Band beginnt rechts von allem, was in den Baendern davor steht, in jeder Schicht —
/// so ueberschneiden sich die Rahmen zweier Baender nie. Ein Stuetzpunkt steht im Band des
/// Ziels seiner Kante. `bandBreite[b]`: so breit ist Band b mindestens (Platz fuer den Titel
/// seines Rahmens), vom linken Rand seines ersten Knotens an.
Schichtung schichte(const std::vector<Punkt>& groesse, const std::vector<Paar>& kanten,
                    const std::vector<double>& stuetzBreite = {},
                    const std::vector<std::vector<std::size_t>>& gruppenVon = {},
                    const std::vector<double>& schildLaenge = {},
                    const std::vector<std::size_t>& bandVon = {},
                    const std::vector<double>& bandBreite = {},
                    const std::vector<bool>& zuUnterst = {})
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

    // `zuUnterst[v]`: Knoten v steht unter allen anderen (Code, der nie erreicht wird). Eine Kante
    // von dort nach oben laeuft wie eine zurueck aussen herum.
    const bool mitUnten =
        zuUnterst.size() >= n && std::find(zuUnterst.begin(), zuUnterst.begin() + static_cast<std::ptrdiff_t>(n),
                                           true) != zuUnterst.begin() + static_cast<std::ptrdiff_t>(n);
    if (mitUnten)
        for (std::size_t e = 0; e < kanten.size(); ++e)
            if (zuUnterst[kanten[e].first] && !zuUnterst[kanten[e].second]) s.zurueck[e] = true;

    // 2. Rang = laengster Weg ueber die Kanten, die nicht zurueckfuehren — ab den Startwerten.
    const auto range = [&](const std::vector<int>& anfang) {
        s.rang = anfang;
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
    };
    range(std::vector<int>(n, 0));
    if (mitUnten)
    {
        // was unten steht, beginnt eine Schicht unter dem Tiefsten der anderen
        int tiefste = 0;
        for (std::size_t v = 0; v < n; ++v)
            if (!zuUnterst[v]) tiefste = std::max(tiefste, s.rang[v]);
        std::vector<int> anfang(n, 0);
        for (std::size_t v = 0; v < n; ++v)
            if (zuUnterst[v]) anfang[v] = tiefste + 1;
        range(anfang);
    }

    // 3. Stuetzpunkte: die Knoten n, n+1, ... ; `masse` und `rang` gelten fuer alle.
    std::vector<Punkt> masse = groesse;
    std::vector<int> rang = s.rang;
    std::vector<std::size_t> band(n, 0);
    if (bandVon.size() >= n) band.assign(bandVon.begin(), bandVon.begin() + static_cast<std::ptrdiff_t>(n));
    std::vector<Paar> stuecke;    // von oben nach unten
    std::vector<Paar> vorwaerts;  // davon die zwischen Nachbarzeilen, ohne die Kanten zurueck
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
            vorwaerts.emplace_back(davor, masse.size());
            davor = masse.size();
            masse.push_back({breite, 0});
            rang.push_back(r);
            band.push_back(band[b]);
        }
        stuecke.emplace_back(davor, b);
        vorwaerts.emplace_back(davor, b);
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
    const auto nachBand = [&](std::vector<std::size_t>& reihe) {
        std::stable_sort(reihe.begin(), reihe.end(), [&](std::size_t a, std::size_t b) { return band[a] < band[b]; });
    };
    for (const std::size_t v : besuch)
        if (rang[v] == 0) reiheEin(v);
    nachBand(schicht[0]);
    for (std::size_t r = 0; r + 1 < schicht.size(); ++r)
    {
        for (std::size_t i = 0; i < schicht[r].size(); ++i)
            for (const std::size_t w : unten[schicht[r][i]])
                if (static_cast<std::size_t>(rang[w]) == r + 1) reiheEin(w);
        for (const std::size_t v : besuch)  // was von oben nicht erreicht wird
            if (static_cast<std::size_t>(rang[v]) == r + 1) reiheEin(v);
        nachBand(schicht[r + 1]);
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
        std::stable_sort(reihe.begin(), reihe.end(), [&](std::size_t a, std::size_t b) {
            return band[a] != band[b] ? band[a] < band[b] : schwerpunkt[a] < schwerpunkt[b];
        });
        zaehleDurch(reihe);
    };
    for (int runde = 0; runde < 2; ++runde)
    {
        for (std::size_t i = 1; i < schicht.size(); ++i) sortiere(schicht[i], oben);
        for (std::size_t i = schicht.size(); i-- > 1;) sortiere(schicht[i - 1], unten);
    }

    // 6. Orte: jede Schicht eine Zeile; die Lage in der Zeile nach Brandes und Koepf (6b).
    std::vector<Rechteck> orte(alle);
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
    // 6a. Die Zeilen: Oberkante, Luecke darueber, Hoehe — unabhaengig von der Lage in der Zeile.
    std::vector<double> zeileY(schicht.size(), 0), zeileHoehe(schicht.size(), 0);
    {
        double y = 0;
        for (std::size_t r = 0; r < schicht.size(); ++r)
        {
            if (r > 0) y += unterLuecke[r] - kAbstandSchicht / 2;  // die Luecke davor reicht weiter
            s.zeileOben.push_back(y);
            s.luecke.push_back(y - unterLuecke[r]);
            y += vorher[r];
            for (const std::size_t v : schicht[r]) zeileHoehe[r] = std::max(zeileHoehe[r], masse[v].y);
            zeileY[r] = y;
            y += zeileHoehe[r] + nachher[r] + kAbstandSchicht;
        }
    }

    // 6b. Die Lage in der Zeile nach Brandes und Koepf: der Platz eines Knotens reicht links die
    //     halbe Breite, rechts dazu seine Schlaufen; nur die Stuecke zwischen Nachbarzeilen zaehlen.
    std::vector<std::vector<std::size_t>> obenNah(alle), untenNah(alle);
    for (const auto [hoch, tief] : vorwaerts)
    {
        untenNah[hoch].push_back(tief);
        obenNah[tief].push_back(hoch);
    }
    std::vector<double> halbLinks(alle), halbRechts(alle);
    std::vector<bool> stuetze(alle, false);
    for (std::size_t v = 0; v < alle; ++v)
    {
        halbLinks[v] = breite[v] / 2;
        halbRechts[v] = breite[v] / 2 + rechts[v];
        stuetze[v] = v >= n;
    }
    std::vector<double> mitteX = brandesKoepf(schicht, obenNah, untenNah, halbLinks, halbRechts, abstand, stuetze);

    // 6c. Baender: jedes beginnt rechts von allem in den Baendern davor (Platz fuer die Raender
    //     zweier Rahmen und den Abstand dazwischen), und so breit wie sein Titel — ein Band rueckt
    //     als Ganzes, so bleiben seine Ketten gerade. In jeder Zeile stehen die Baender schon in Folge.
    const std::size_t baender = 1 + *std::max_element(band.begin(), band.end());
    for (std::size_t b = 1; b < baender; ++b)
    {
        double ende = -1e18, anfang = 1e18;
        std::vector<double> anfangVon(b, 1e18);
        for (std::size_t v = 0; v < alle; ++v)
        {
            if (band[v] < b)
            {
                ende = std::max(ende, mitteX[v] + halbRechts[v]);
                if (v < n) anfangVon[band[v]] = std::min(anfangVon[band[v]], mitteX[v] - halbLinks[v]);
            }
            else
                anfang = std::min(anfang, mitteX[v] - halbLinks[v]);
        }
        for (std::size_t c = 0; c < b && c < bandBreite.size(); ++c)
            if (anfangVon[c] < 1e17) ende = std::max(ende, anfangVon[c] + bandBreite[c]);
        const double fehlt = ende + 2 * kGruppenRand + kAbstandNachbar - anfang;
        if (ende < -1e17 || anfang > 1e17 || fehlt <= 0) continue;
        for (std::size_t v = 0; v < alle; ++v)
            if (band[v] >= b) mitteX[v] += fehlt;
    }
    for (std::size_t r = 0; r < schicht.size(); ++r)
        for (const std::size_t v : schicht[r])
            orte[v] = {mitteX[v] - masse[v].x / 2, zeileY[r] + (zeileHoehe[r] - masse[v].y) / 2, masse[v].x, masse[v].y};

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
    // ein Hauch dazu: 115.2 / 7.2 ist in double knapp unter 16
    const auto passen = frei > 0 ? static_cast<std::size_t>(frei / optionen.zeichenBreite + 1e-6) : 0;
    const std::size_t zeichen = std::max(passen, kWenigstens);
    return optionen.maxZeilenLaenge == 0 ? zeichen : std::min(zeichen, optionen.maxZeilenLaenge);
}

double kopfHoehe(const AnordnungOptionen& optionen)
{
    return 2 * optionen.zeilenHoehe + 10;
}

namespace
{

/// Eine Zeile der Legende: die Bedingung und ihre Farbe.
using Farbzeile = std::pair<std::string, std::size_t>;

/// So breit muss ein Rahmen sein, damit sein Titel ungekuerzt passt (wie titelZeichen rechnet).
double titelBreite(const std::string& titel, const AnordnungOptionen& o)
{
    return static_cast<double>(laengsteZeile({titel}, o.maxZeilenLaenge)) * o.zeichenBreite + 16;
}

/// Die Wege der Kanten, gerechnet nach unten: hinab unten aus der Quelle, durch die Stuetzpunkte,
/// oben ins Ziel, die Spalte wechselt nur in der Luecke ueber einer Schicht; zurueck (oder in der
/// eigenen Schicht) rechts aussen herum. `ausX[e]` / `einX[e]`: wo Kante e die Quelle unten
/// verlaesst und das Ziel oben trifft (leer: die Mitte); `ausVersatz[e]`: so weit unter (ueber) der
/// Mitte der Luecke biegt sie zum ersten Mal ab (leer: 0). Dazu je Kante der Ort ihrer Beschriftung.
struct Wege
{
    std::vector<Linienzug> linien;
    std::vector<Punkt> orte;
};

Wege fuehreKanten(const Schichtung& s, const std::vector<Rechteck>& orte, const std::vector<Paar>& kanten,
                  const std::vector<double>& stuetzBreite, const std::vector<double>& schildLaenge,
                  const std::vector<double>& ausX = {}, const std::vector<double>& einX = {},
                  const std::vector<double>& ausVersatz = {})
{
    Wege w;
    const auto entlang = [](Punkt von, Punkt nach, double weit) {
        const double laenge = std::hypot(nach.x - von.x, nach.y - von.y);
        if (laenge < 1) return von;
        const double t = std::clamp(weit, 0.0, laenge) / laenge;
        return Punkt{von.x + (nach.x - von.x) * t, von.y + (nach.y - von.y) * t};
    };

    std::size_t staffel = 0;
    std::vector<std::size_t> schlaufen(orte.size(), 0);  // je Knoten: seine Schlaufen bisher
    for (std::size_t e = 0; e < kanten.size(); ++e)
    {
        const Rechteck& von = orte[kanten[e].first];
        const Rechteck& nach = orte[kanten[e].second];
        const int rangVon = s.rang[kanten[e].first];
        const int rangNach = s.rang[kanten[e].second];
        Linienzug linie;
        Punkt ort;
        if (rangNach > rangVon)
        {
            // hinab: unten aus der Quelle, durch die Stuetzpunkte, oben ins Ziel. Die Spalte
            // wechselt nur in der Luecke ueber einer Schicht — dort, wo nichts steht.
            const Punkt p{e < ausX.size() ? ausX[e] : von.mitte().x, von.y + von.hoehe};
            const Punkt q{e < einX.size() ? einX[e] : nach.mitte().x, nach.y};
            const auto luecke = [&](int r) { return s.luecke[static_cast<std::size_t>(r)]; };
            linie.push_back(p);
            double x = p.x;
            bool erste = true;
            const auto biege = [&](double ziel, int r) {
                if (std::abs(ziel - x) < 1) return;
                const double y = luecke(r) + (erste && e < ausVersatz.size() ? ausVersatz[e] : 0);
                erste = false;
                linie.push_back({x, y});
                linie.push_back({ziel, y});
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
            const bool selbst = kanten[e].first == kanten[e].second;
            double aussen = 0;
            if (selbst)
                aussen = von.x + von.breite + schlaufe(schlaufen[kanten[e].first]++);
            else
            {
                for (std::size_t v = 0; v < orte.size(); ++v)
                    if (s.rang[v] >= rangNach && s.rang[v] <= rangVon)
                        aussen = std::max(aussen, orte[v].x + orte[v].breite);
                for (std::size_t f = 0; f < kanten.size(); ++f)
                    for (std::size_t i = 0; i < s.stuetzen[f].size(); ++i)
                    {
                        const int r = s.rang[kanten[f].first] + 1 + static_cast<int>(i);
                        const double halb = i == 0 ? std::max(stuetzBreite[f], kStuetzBreite) / 2 : kStuetzBreite / 2;
                        if (r >= rangNach && r <= rangVon) aussen = std::max(aussen, s.stuetzen[f][i].x + halb);
                    }
                aussen += kRueckAbstand + kRueckStaffel * static_cast<double>(staffel++);
            }
            // Steht in der Schicht rechts daneben noch ein Knoten, ginge die Linie waagrecht durch
            // ihn: dann unten aus der Quelle (oben ins Ziel) und auf einer Spur knapp an der Zeile
            // entlang — nicht in der Mitte der Luecke, wo Kanten abbiegen und Beschriftungen stehen.
            const auto rechtsFrei = [&](std::size_t v) {
                for (std::size_t u = 0; u < orte.size(); ++u)
                    if (u != v && s.rang[u] == s.rang[v] && orte[u].x > orte[v].x) return false;
                return true;
            };
            if (selbst || rechtsFrei(kanten[e].first))
            {
                const Punkt p{von.x + von.breite, von.y + von.hoehe * (selbst ? 0.7 : 0.5)};
                linie = {p, {aussen, p.y}};
            }
            else
            {
                const Punkt p{e < ausX.size() ? ausX[e] : von.mitte().x, von.y + von.hoehe};
                const std::size_t darunter = static_cast<std::size_t>(rangVon) + 1;
                const double spur = darunter < s.luecke.size() ? s.luecke[darunter] - kAbstandSchicht / 2 + kSpurAbstand
                                                               : p.y + kSpurAbstand;
                linie = {p, {p.x, spur}, {aussen, spur}};
            }
            if (selbst || rechtsFrei(kanten[e].second))
            {
                const Punkt q{nach.x + nach.breite, nach.y + nach.hoehe * (selbst ? 0.3 : 0.5)};
                linie.push_back({aussen, q.y});
                linie.push_back(q);
            }
            else
            {
                const Punkt q{e < einX.size() ? einX[e] : nach.mitte().x, nach.y};
                const double spur = s.zeileOben[static_cast<std::size_t>(rangNach)] - kSpurAbstand;
                linie.push_back({aussen, spur});
                linie.push_back({q.x, spur});
                linie.push_back(q);
            }
            ort = {aussen, (linie[1].y + linie[linie.size() - 2].y) / 2};
        }
        w.linien.push_back(std::move(linie));
        w.orte.push_back(ort);
    }
    return w;
}

/// Die Anordnung eines Flussdiagramms, mit Baendern (schichte) und den Zeilen der Legende.
FlussAnordnung ordneFluss(const Flussdiagramm& diagramm, const AnordnungOptionen& optionen,
                          const std::vector<std::size_t>& bandVon, const std::vector<Farbzeile>& farbzeilen,
                          const std::vector<double>& bandBreite = {})
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
    std::vector<bool> unten(knoten.size(), false);
    for (std::size_t v = 0; v < knoten.size(); ++v) unten[v] = knoten[v].unerreichbar;
    const Schichtung s = schichte(groesse, paare, stuetzBreite, gruppenVon, schildLaenge, bandVon,
                                  drehen ? std::vector<double>() : bandBreite, unten);
    a.knoten = s.orte;

    const Wege wege = fuehreKanten(s, a.knoten, paare, stuetzBreite, schildLaenge);
    a.kanten = wege.linien;
    a.beschriftungen = wege.orte;

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
        // der Rahmen einer Datei traegt ihren Titel ganz: nach unten hielt das Band ihm Platz frei,
        // nach rechts liegen die Baender untereinander, und rechts daneben steht kein fremdes Band
        if (diagramm.gruppen()[g].art == GruppenArt::Datei)
            r.breite = std::max(r.breite, titelBreite(diagramm.gruppen()[g].titel, optionen));
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
    const double links0 = a.breite;
    double y = kRand;
    for (const auto& [titel, farbe] : farbzeilen)
    {
        const double zeichen = static_cast<double>(laengsteZeile({titel}, optionen.maxZeilenLaenge));
        FlussAnordnung::Legende eintrag{{links0, y + (optionen.zeilenHoehe - kLegendenFeld) / 2, kLegendenFeld, kLegendenFeld},
                                        {links0 + kLegendenFeld + 6, y + optionen.zeilenHoehe / 2},
                                        titel,
                                        farbe};
        a.breite = std::max(a.breite, eintrag.text.x + zeichen * optionen.zeichenBreite + kRand);
        a.legende.push_back(std::move(eintrag));
        y += optionen.zeilenHoehe + 4;
    }
    a.hoehe = std::max(a.hoehe, y + kRand);
    return a;
}

/// Je Farbe einmal die Bedingung, in der Reihenfolge von `farben` (kKeine: keine).
std::vector<Farbzeile> farbzeilen(const std::vector<std::size_t>& farben, const auto& titelVon)
{
    std::vector<Farbzeile> aus;
    for (std::size_t i = 0; i < farben.size(); ++i)
    {
        if (farben[i] == kKeine) continue;
        const std::string& titel = titelVon(i);
        if (std::none_of(aus.begin(), aus.end(), [&](const Farbzeile& z) { return z.first == titel; }))
            aus.emplace_back(titel, farben[i]);
    }
    return aus;
}

}  // namespace

FlussAnordnung ordneAn(const Flussdiagramm& diagramm, const AnordnungOptionen& optionen)
{
    return ordneFluss(diagramm, optionen, {},
                      farbzeilen(farbenDerRahmen(diagramm), [&](std::size_t g) -> const std::string& {
                          return diagramm.gruppen()[g].titel;
                      }));
}

FlussAnordnung ordneAn(const Aufrufgraph& graph, const AnordnungOptionen& optionen)
{
    // dieselbe Anordnung wie ein Ablauf: eine Funktion wie ein Aufruf, Fremd und Makro wie eine
    // Anweisung; mit Rahmen je Datei eine Gruppe und ein Band, Fremd und Makro im letzten Band
    Flussdiagramm alsFluss(graph.titel);
    const bool rahmen = graph.mitRahmen();
    if (rahmen)
        for (const std::string& pfad : graph.dateien) alsFluss.neueGruppe({GruppenArt::Datei, pfad, kKeine, kKeine, {}, {}});
    std::vector<std::size_t> baender;
    for (const AufrufKnoten& k : graph.knoten)
    {
        const bool funktion = k.art == AufrufKnotenArt::Funktion;
        const bool inDatei = rahmen && funktion && k.datei < graph.dateien.size();
        alsFluss.neuerKnoten(Knoten{funktion ? KnotenArt::Aufruf : KnotenArt::Anweisung, {knotenText(k)}, k.zeile, {},
                                    inDatei ? k.datei : kKeine});
        if (rahmen) baender.push_back(inDatei ? k.datei : graph.dateien.size());
    }
    for (const AufrufKante& k : graph.kanten)
    {
        // so lang, wie ein Format sie gekuerzt zeigt
        std::string text = beschriftung(k);
        if (optionen.maxZeilenLaenge != 0 && text.size() > optionen.maxZeilenLaenge) text.resize(optionen.maxZeilenLaenge);
        alsFluss.verbinde(k.von, k.nach, text);
    }
    // ein Band so breit wie der Titel seines Rahmens, ohne die Raender des Rahmens
    std::vector<double> bandBreite;
    if (rahmen)
        for (const std::string& pfad : graph.dateien) bandBreite.push_back(titelBreite(pfad, optionen) - 2 * kGruppenRand);
    return ordneFluss(alsFluss, optionen, baender,
                      farbzeilen(farbenDerBedingungen(graph),
                                 [&](std::size_t e) -> const std::string& { return graph.kanten[e].bedingung; }),
                      bandBreite);
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

    // Gerechnet wird nach unten wie im Ablauf; nach rechts: mit getauschten Seiten, dann drehen.
    const bool drehen = optionen.nachRechts;
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
        groesse.push_back(drehen ? Punkt{g.y, g.x} : g);
    }
    // Die Kanten nach unten: der Besitzer ueber dem, was er enthaelt oder nennt; die Basis ueber dem,
    // was von ihr erbt — gerechnet von der Basis aus, gezeichnet zu ihr hin (umgedreht).
    std::vector<Paar> paare;
    std::vector<bool> umgedreht;
    std::vector<double> stuetzBreite;
    std::vector<double> schildLaenge;
    for (const Beziehung& b : diagramm.beziehungen)
    {
        const bool erbt = b.art == BeziehungArt::Erbt;
        paare.push_back(erbt ? Paar{b.nach, b.von} : Paar{b.von, b.nach});
        umgedreht.push_back(erbt);
        const std::string_view text = b.beschriftung;
        const double breite = static_cast<double>(gekuerzt(text.size())) * optionen.zeichenBreite + 10;
        stuetzBreite.push_back(text.empty() ? 0 : drehen ? optionen.zeilenHoehe + 6 : breite);
        schildLaenge.push_back(text.empty() || !drehen ? 0 : breite);
    }
    const Schichtung s = schichte(groesse, paare, stuetzBreite, {}, schildLaenge);
    const std::vector<Rechteck>& orte = s.orte;

    // Wo eine Kante den Kasten unten verlaesst und oben trifft: mehrere je Kasten nebeneinander
    // verteilt, geordnet nach der Seite, auf der ihr anderes Ende liegt — so kreuzen sie sich nicht.
    std::vector<double> ausX(paare.size(), 0), einX(paare.size(), 0);
    std::vector<std::vector<std::size_t>> hinaus(orte.size()), hinein(orte.size());
    for (std::size_t e = 0; e < paare.size(); ++e)
    {
        const auto [von, nach] = paare[e];
        ausX[e] = orte[von].mitte().x;
        einX[e] = orte[nach].mitte().x;
        if (s.rang[nach] <= s.rang[von]) continue;  // zurueck oder in der eigenen Schicht: aussen herum
        hinaus[von].push_back(e);
        hinein[nach].push_back(e);
    }
    // die Spalte, in der eine Kante unter der Quelle (ueber dem Ziel) weiterlaeuft
    const auto weiterUnten = [&](std::size_t e) {
        return s.stuetzen[e].empty() ? orte[paare[e].second].mitte().x : s.stuetzen[e].front().x;
    };
    const auto weiterOben = [&](std::size_t e) {
        return s.stuetzen[e].empty() ? orte[paare[e].first].mitte().x : s.stuetzen[e].back().x;
    };
    const auto verteile = [&](std::vector<std::size_t>& liste, const Rechteck& r, auto nachSeite, std::vector<double>& x) {
        std::stable_sort(liste.begin(), liste.end(), [&](std::size_t p, std::size_t q) { return nachSeite(p) < nachSeite(q); });
        for (std::size_t i = 0; i < liste.size(); ++i)
            x[liste[i]] = r.x + r.breite * static_cast<double>(i + 1) / static_cast<double>(liste.size() + 1);
    };
    for (std::size_t k = 0; k < orte.size(); ++k)
    {
        verteile(hinaus[k], orte[k], weiterUnten, ausX);
        verteile(hinein[k], orte[k], weiterOben, einX);
    }
    // Wer aus demselben Kasten unten austritt, biegt auf eigener Hoehe ab — sonst laegen die
    // waagrechten Stuecke uebereinander. Nach rechts: wer weiter rechts austritt, biegt hoeher ab
    // (nach links umgekehrt); so kreuzt keine Senkrechte die Waagrechte einer anderen.
    std::vector<double> ausVersatz(paare.size(), 0);
    for (std::size_t k = 0; k < orte.size(); ++k)
    {
        std::vector<std::size_t> rechtsHin, linksHin;
        for (const std::size_t e : hinaus[k])
        {
            const double naechste = s.stuetzen[e].empty() ? einX[e] : s.stuetzen[e].front().x;
            if (naechste > ausX[e] + 0.5) rechtsHin.push_back(e);
            else if (naechste < ausX[e] - 0.5) linksHin.push_back(e);
        }
        std::sort(rechtsHin.begin(), rechtsHin.end(), [&](std::size_t p, std::size_t q) { return ausX[p] > ausX[q]; });
        std::sort(linksHin.begin(), linksHin.end(), [&](std::size_t p, std::size_t q) { return ausX[p] < ausX[q]; });
        for (const auto* gruppe : {&rechtsHin, &linksHin})
        {
            const std::size_t m = gruppe->size();
            if (m < 2) continue;
            const double schritt = std::min(8.0, (kAbstandSchicht - 16) / static_cast<double>(m - 1));
            for (std::size_t i = 0; i < m; ++i)
                ausVersatz[(*gruppe)[i]] = (static_cast<double>(i) - static_cast<double>(m - 1) / 2) * schritt;
        }
    }
    const Wege wege = fuehreKanten(s, orte, paare, stuetzBreite, schildLaenge, ausX, einX, ausVersatz);
    a.kaesten = orte;
    a.beziehungen = wege.linien;
    std::vector<Punkt> vorschlag = wege.orte;
    for (std::size_t e = 0; e < a.beziehungen.size(); ++e)
        if (umgedreht[e]) std::reverse(a.beziehungen[e].begin(), a.beziehungen[e].end());
    if (drehen)
    {
        for (Rechteck& r : a.kaesten) tausche(r);
        for (Linienzug& l : a.beziehungen)
            for (Punkt& p : l) std::swap(p.x, p.y);
        for (Punkt& p : vorschlag) std::swap(p.x, p.y);
    }

    // Die Beschriftungen: wo die Wegefuehrung sie hinstellt, wenn dort weder ein anderes Schild noch
    // ein Kasten steht; sonst neben einem Stueck ihres Wegs, zuerst dem laengsten, auf beiden Seiten,
    // notfalls weiter draussen.
    std::vector<Rechteck> belegt = a.kaesten;
    const auto trifft = [&](const Rechteck& r) {
        if (r.x < 0 || r.y < 0) return true;  // nicht ueber den Rand hinaus
        return std::any_of(belegt.begin(), belegt.end(), [&](const Rechteck& b) {
            return r.x < b.x + b.breite && b.x < r.x + r.breite && r.y < b.y + b.hoehe && b.y < r.y + r.hoehe;
        });
    };
    for (std::size_t e = 0; e < a.beziehungen.size(); ++e)
    {
        const Linienzug& l = a.beziehungen[e];
        const std::string& text = diagramm.beziehungen[e].beschriftung;
        if (text.empty() || l.size() < 2)
        {
            a.beschriftungen.push_back(vorschlag[e]);
            continue;
        }
        const double breite = static_cast<double>(laengsteZeile({text}, optionen.maxZeilenLaenge)) * optionen.zeichenBreite + 6;
        const double hoehe = optionen.zeilenHoehe;
        const auto schild = [&](Punkt m) { return Rechteck{m.x - breite / 2, m.y - hoehe / 2, breite, hoehe}; };
        std::vector<Punkt> stellen{vorschlag[e]};
        std::vector<std::size_t> stuecke(l.size() - 1);
        std::iota(stuecke.begin(), stuecke.end(), std::size_t{0});
        const auto laenge = [&](std::size_t i) { return std::hypot(l[i + 1].x - l[i].x, l[i + 1].y - l[i].y); };
        std::stable_sort(stuecke.begin(), stuecke.end(), [&](std::size_t p, std::size_t q) { return laenge(p) > laenge(q); });
        for (int weiter = 0; weiter < 4; ++weiter)
            for (const std::size_t i : stuecke)
            {
                const Punkt p = l[i], q = l[i + 1];
                const double lang = std::max(1.0, laenge(i));
                const Punkt quer{-(q.y - p.y) / lang, (q.x - p.x) / lang};
                const double weg = std::abs(quer.x) * breite / 2 + std::abs(quer.y) * hoehe / 2 + 4 +
                                   (std::abs(quer.x) * breite + std::abs(quer.y) * hoehe + 4) * weiter;
                for (const double t : {0.5, 0.3, 0.7})
                    for (const double seite : {1.0, -1.0})
                        stellen.push_back({p.x + (q.x - p.x) * t + quer.x * weg * seite, p.y + (q.y - p.y) * t + quer.y * weg * seite});
            }
        Punkt ort = stellen.front();
        for (const Punkt& kandidat : stellen)
            if (!trifft(schild(kandidat)))
            {
                ort = kandidat;
                break;
            }
        belegt.push_back(schild(ort));
        a.beschriftungen.push_back(ort);
    }

    const auto weite = [&](double x, double y) {
        a.breite = std::max(a.breite, x + kRand);
        a.hoehe = std::max(a.hoehe, y + kRand);
    };
    for (const Rechteck& r : a.kaesten) weite(r.x + r.breite, r.y + r.hoehe);
    for (std::size_t r = a.kaesten.size(); r < belegt.size(); ++r) weite(belegt[r].x + belegt[r].breite, belegt[r].y + belegt[r].hoehe);
    for (const Linienzug& l : a.beziehungen)
        for (const Punkt& p : l) weite(p.x, p.y);
    return a;
}

}  // namespace c2d::diagramm
