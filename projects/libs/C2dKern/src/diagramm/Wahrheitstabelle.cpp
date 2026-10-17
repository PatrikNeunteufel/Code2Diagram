/**
 * @file Wahrheitstabelle.cpp
 * @brief Jede Kombination auswerten (mit Kurzschluss), MC/DC-Paare suchen, eine kleinste Menge waehlen
 */
#include "c2d/diagramm/Wahrheitstabelle.hpp"

#include <algorithm>
#include <map>

namespace c2d::diagramm
{

namespace
{

using modell::Logik;

/// Gleicher Text ist dasselbe Atom — ohne Leerraum verglichen.
std::string schluessel(const std::string& text)
{
    std::string aus;
    for (const char c : text)
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') aus += c;
    return aus;
}

void sammle(const Logik& l, Wahrheitstabelle& t, std::map<std::string, std::size_t>& index)
{
    if (l.art == Logik::Art::Atom)
    {
        const auto [stelle, neu] = index.emplace(schluessel(l.text), t.atome.size());
        if (neu) t.atome.push_back({l.text, l.mitAufruf});
        else if (l.mitAufruf) t.atome[stelle->second].mitAufruf = true;
    }
    for (const Logik& teil : l.teile) sammle(teil, t, index);
}

/// Wertet aus wie C: von links, `&&` endet beim ersten falschen, `||` beim ersten wahren.
/// Bit (n-1-i) der Belegung ist Atom i; `ausgewertet[i]` sagt, ob es gefragt wurde.
bool werte(const Logik& l, std::size_t belegung, std::size_t n, const std::map<std::string, std::size_t>& index,
           std::vector<bool>& ausgewertet)
{
    switch (l.art)
    {
        case Logik::Art::Atom:
        {
            const std::size_t i = index.at(schluessel(l.text));
            ausgewertet[i] = true;
            return ((belegung >> (n - 1 - i)) & 1U) != 0;
        }
        case Logik::Art::Nicht: return !werte(l.teile[0], belegung, n, index, ausgewertet);
        case Logik::Art::Und:
            for (const Logik& t : l.teile)
                if (!werte(t, belegung, n, index, ausgewertet)) return false;
            return true;
        case Logik::Art::Oder:
            for (const Logik& t : l.teile)
                if (werte(t, belegung, n, index, ausgewertet)) return true;
            return false;
    }
    return false;
}

using Paar = std::pair<std::size_t, std::size_t>;  // zwei Belegungen

/// Sucht fuer jedes Atom (mit Paaren) ein Paar, so dass die Vereinigung der Zeilen moeglichst klein ist.
class Suche
{
public:
    Suche(const std::vector<std::vector<Paar>>& paare, std::size_t zeilen) : m_paare(paare), m_drin(zeilen, 0)
    {
        // die Atome mit den wenigsten Paaren zuerst: dort verzweigt es am wenigsten
        for (std::size_t i = 0; i < paare.size(); ++i)
            if (!paare[i].empty()) m_reihe.push_back(i);
        std::sort(m_reihe.begin(), m_reihe.end(),
                  [&](std::size_t a, std::size_t b) { return paare[a].size() < paare[b].size(); });
        m_wahl.assign(paare.size(), Paar{0, 0});
    }

    /// true = sicher die kleinste (die Suche lief zu Ende).
    bool los()
    {
        gierig();
        tief(0, 0);
        return m_schritte < kHoechstensSchritte;
    }

    [[nodiscard]] const std::vector<Paar>& beste() const { return m_beste; }

private:
    static constexpr std::size_t kHoechstensSchritte = 500000;

    std::size_t neue(const Paar& p) const { return (m_drin[p.first] == 0 ? 1 : 0) + (m_drin[p.second] == 0 ? 1 : 0); }

    void nimm(const Paar& p, int um)
    {
        m_drin[p.first] += um;
        m_drin[p.second] += um;
    }

    /// Eine erste Loesung: je Atom das Paar, das am wenigsten Zeilen dazunimmt.
    void gierig()
    {
        std::vector<Paar> wahl(m_paare.size(), Paar{0, 0});
        std::size_t groesse = 0;
        for (const std::size_t a : m_reihe)
        {
            const Paar* best = &m_paare[a].front();
            for (const Paar& p : m_paare[a])
                if (neue(p) < neue(*best)) best = &p;
            groesse += neue(*best);
            nimm(*best, 1);
            wahl[a] = *best;
        }
        for (const std::size_t a : m_reihe) nimm(wahl[a], -1);
        m_beste = wahl;
        m_besteGroesse = groesse;
    }

    void tief(std::size_t stufe, std::size_t groesse)
    {
        if (++m_schritte >= kHoechstensSchritte) return;
        if (stufe == m_reihe.size())
        {
            if (groesse < m_besteGroesse)
            {
                m_besteGroesse = groesse;
                m_beste = m_wahl;
            }
            return;
        }
        const std::size_t a = m_reihe[stufe];
        // ein Paar, das schon ganz drin ist, kostet nichts — besser geht es fuer dieses Atom nicht
        for (const Paar& p : m_paare[a])
            if (neue(p) == 0)
            {
                m_wahl[a] = p;
                tief(stufe + 1, groesse);
                return;
            }
        for (const std::size_t kosten : {std::size_t{1}, std::size_t{2}})
            for (const Paar& p : m_paare[a])
            {
                if (neue(p) != kosten || groesse + kosten >= m_besteGroesse) continue;
                nimm(p, 1);
                m_wahl[a] = p;
                tief(stufe + 1, groesse + kosten);
                nimm(p, -1);
                if (m_schritte >= kHoechstensSchritte) return;
            }
    }

    const std::vector<std::vector<Paar>>& m_paare;
    std::vector<int> m_drin;
    std::vector<std::size_t> m_reihe;
    std::vector<Paar> m_wahl;
    std::vector<Paar> m_beste;
    std::size_t m_besteGroesse = 0;
    std::size_t m_schritte = 0;
};

void sammleTabellen(const modell::Anweisung& a, const std::string& funktion, std::vector<Wahrheitstabelle>& aus)
{
    if (a.logik) aus.push_back(wahrheitstabelleVon(*a.logik, a.logikText, funktion, a.zeile));
    for (const modell::AnweisungPtr& k : a.kinder) sammleTabellen(*k, funktion, aus);
    if (a.sonst) sammleTabellen(*a.sonst, funktion, aus);
}

}  // namespace

std::string atomKennung(std::size_t atom)
{
    std::string k(1, static_cast<char>('A' + atom % 26));
    if (atom >= 26) k += std::to_string(atom / 26);
    return k;
}

Wahrheitstabelle wahrheitstabelleVon(const modell::Logik& logik, std::string_view text, std::string_view funktion,
                                     int zeile)
{
    Wahrheitstabelle t;
    t.funktion = std::string(funktion);
    t.bedingung = std::string(text);
    t.quellZeile = zeile;
    std::map<std::string, std::size_t> index;
    sammle(logik, t, index);
    const std::size_t n = t.atome.size();
    if (n > kHoechstensAtome)
    {
        t.gekuerzt = true;
        t.hinweise.push_back(std::to_string(n) + " conditions - too many for a truth table");
        return t;
    }

    // jede Kombination: Atom 1 links, falsch vor wahr
    const std::size_t anzahl = std::size_t{1} << n;
    std::vector<Wahrheitstabelle::Zeile> alle(anzahl);
    std::vector<bool> ergebnis(anzahl);
    for (std::size_t b = 0; b < anzahl; ++b)
    {
        std::vector<bool> ausgewertet(n, false);
        ergebnis[b] = werte(logik, b, n, index, ausgewertet);
        Wahrheitstabelle::Zeile& z = alle[b];
        z.nummer = b + 1;
        z.ergebnis = ergebnis[b];
        for (std::size_t i = 0; i < n; ++i)
            z.werte.push_back(!ausgewertet[i] ? Wert::Egal : ((b >> (n - 1 - i)) & 1U) != 0 ? Wert::Wahr : Wert::Falsch);
    }

    // Unique-Cause: zwei Belegungen, die sich nur in Atom i unterscheiden und verschieden ausgehen
    std::vector<std::vector<Paar>> paare(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const std::size_t bit = std::size_t{1} << (n - 1 - i);
        for (std::size_t b = 0; b < anzahl; ++b)
            if ((b & bit) == 0 && ergebnis[b] != ergebnis[b | bit]) paare[i].emplace_back(b, b | bit);
        if (paare[i].empty())
            t.hinweise.push_back(atomKennung(i) + " (" + t.atome[i].text +
                                 "): no MC/DC pair - changing it alone never changes the result");
    }
    Suche suche(paare, anzahl);
    t.kleinste = suche.los();
    if (!t.kleinste) t.hinweise.push_back("MC/DC rows: a small set, not proven to be the smallest");
    for (std::size_t i = 0; i < n; ++i)
    {
        const Paar p = suche.beste()[i];
        if (paare[i].empty())
        {
            t.paare.emplace_back(0, 0);
            continue;
        }
        t.paare.emplace_back(p.first + 1, p.second + 1);
        alle[p.first].mcdc = true;
        alle[p.second].mcdc = true;
    }

    if (n <= kVolleTabelle)
        t.zeilen = std::move(alle);
    else
    {
        t.gekuerzt = true;
        for (Wahrheitstabelle::Zeile& z : alle)
            if (z.mcdc) t.zeilen.push_back(std::move(z));
        t.hinweise.push_back(std::to_string(n) + " conditions: only the MC/DC rows of " + std::to_string(anzahl));
    }
    return t;
}

std::vector<int> zeilenMitWahrheitstabelle(const modell::Funktion& funktion)
{
    std::vector<int> aus;
    const auto sammle = [&](const auto& selbst, const modell::Anweisung& a) -> void {
        if (a.logik) aus.push_back(a.zeile);
        for (const modell::AnweisungPtr& k : a.kinder) selbst(selbst, *k);
        if (a.sonst) selbst(selbst, *a.sonst);
    };
    if (funktion.rumpf) sammle(sammle, *funktion.rumpf);
    return aus;
}

std::vector<Wahrheitstabelle> wahrheitstabellenVon(const modell::Funktion& funktion)
{
    std::vector<Wahrheitstabelle> aus;
    if (funktion.rumpf) sammleTabellen(*funktion.rumpf, funktion.vollerName(), aus);
    return aus;
}

std::optional<Wahrheitstabelle> wahrheitstabelleVon(const modell::Funktion& funktion, int zeile)
{
    for (Wahrheitstabelle& t : wahrheitstabellenVon(funktion))
        if (t.quellZeile == zeile) return std::move(t);
    return std::nullopt;
}

}  // namespace c2d::diagramm
