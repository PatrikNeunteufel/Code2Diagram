/**
 * @file Funktionen.cpp
 * @brief Das Verzeichnis der Funktionen eines Projekts: Schluessel nach `heisst`, Kandidaten, Aufrufer
 */
#include "c2d/projekt/Funktionen.hpp"

#include <algorithm>

namespace c2d::projekt
{

namespace
{

/// Ausser SQL: der letzte Teil nach `::` — `heisst` verlangt ihn gleich (ohne `::` im Aufruf der Name selbst,
/// mit `::` endet der volle Name auf den Aufruf).
std::string_view nachBereich(std::string_view s)
{
    const std::size_t at = s.rfind("::");
    return at == std::string_view::npos ? s : s.substr(at + 2);
}

/// SQL: der letzte Teil nach `.`, klein — `heisst` vergleicht ohne Gross/klein, ohne Schema ueber ihn.
std::string nachSchema(std::string_view s)
{
    const std::size_t punkt = s.find_last_of('.');
    std::string aus(punkt == std::string_view::npos ? s : s.substr(punkt + 1));
    for (char& c : aus) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return aus;
}

const Projekt& leeresProjekt()
{
    static const Projekt kLeer;
    return kLeer;
}

bool frueher(const Fundstelle& a, const Fundstelle& b)
{
    return a.datei != b.datei ? a.datei < b.datei : a.funktion < b.funktion;
}

}  // namespace

Funktionsverzeichnis::Funktionsverzeichnis() : m_projekt(&leeresProjekt()) {}

Funktionsverzeichnis::Funktionsverzeichnis(const Projekt& projekt) : m_projekt(&projekt)
{
    for (std::size_t d = 0; d < projekt.dateien.size(); ++d)
    {
        const std::vector<modell::Funktion>& fs = projekt.dateien[d].ergebnis.einheit.funktionen;
        for (std::size_t f = 0; f < fs.size(); ++f)
        {
            if (!fs[f].rumpf) continue;
            if (fs[f].namenOhneFall)
                m_ohneFall[nachSchema(fs[f].name)].push_back({d, f});
            else
                m_genau[std::string(nachBereich(fs[f].vollerName()))].push_back({d, f});
        }
    }
}

std::vector<Fundstelle> Funktionsverzeichnis::kandidaten(std::size_t ausDatei, std::string_view aufruf) const
{
    // die Funktionen mit dem Schluessel des Aufrufs, nach Datei und Funktion — beide Tafeln, ein Aufruf aus
    // C kann eine Routine in SQL meinen
    std::vector<Fundstelle> moeglich;
    if (const auto da = m_genau.find(std::string(nachBereich(aufruf))); da != m_genau.end()) moeglich = da->second;
    if (const auto da = m_ohneFall.find(nachSchema(aufruf)); da != m_ohneFall.end())
    {
        moeglich.insert(moeglich.end(), da->second.begin(), da->second.end());
        std::sort(moeglich.begin(), moeglich.end(), frueher);
    }
    const auto passt = [&](const Fundstelle& s) {
        return funktion(*m_projekt, s).heisst(aufruf);
    };
    std::vector<Fundstelle> aus;
    for (const Fundstelle& s : moeglich)
        if (s.datei == ausDatei && passt(s)) aus.push_back(s);
    if (!aus.empty()) return aus;
    for (const Fundstelle& s : moeglich)
        if (s.datei != ausDatei && !funktion(*m_projekt, s).intern && passt(s)) aus.push_back(s);
    return aus;
}

const Funktionsverzeichnis::Rufe& Funktionsverzeichnis::rufe() const
{
    if (m_rufe) return *m_rufe;
    auto r = std::make_shared<Rufe>();
    const Projekt& p = *m_projekt;
    for (std::size_t d = 0; d < p.dateien.size(); ++d)
    {
        const std::vector<modell::Funktion>& fs = p.dateien[d].ergebnis.einheit.funktionen;
        for (std::size_t f = 0; f < fs.size(); ++f)
        {
            if (!fs[f].rumpf) continue;
            for (std::string& aufruf : modell::aufrufeVon(fs[f]))
            {
                r->genau[std::string(nachBereich(aufruf))].emplace_back(Fundstelle{d, f}, aufruf);
                r->ohneFall[nachSchema(aufruf)].emplace_back(Fundstelle{d, f}, std::move(aufruf));
            }
        }
    }
    m_rufe = std::move(r);
    return *m_rufe;
}

std::vector<Fundstelle> Funktionsverzeichnis::aufrufer(const Fundstelle& ziel) const
{
    std::vector<Fundstelle> aus;
    if (ziel.datei >= m_projekt->dateien.size()) return aus;
    const modell::Funktion& gerufen = funktion(*m_projekt, ziel);
    const Rufe& r = rufe();
    const auto& tafel = gerufen.namenOhneFall ? r.ohneFall : r.genau;
    const auto da = tafel.find(gerufen.namenOhneFall ? nachSchema(gerufen.name)
                                                     : std::string(nachBereich(gerufen.vollerName())));
    if (da == tafel.end()) return aus;
    // find_if statt find: std::find vektorisiert in der MSVC-STL Typen mit `==` = default, und Fundstelle (16 Bytes)
    // passt in keine ihrer Breiten (static_assert "unexpected size")
    const auto enthaelt = [](const std::vector<Fundstelle>& v, const Fundstelle& s) {
        return std::any_of(v.begin(), v.end(), [&](const Fundstelle& x) { return x == s; });
    };
    for (const auto& [rufer, aufruf] : da->second)
    {
        if (enthaelt(aus, rufer)) continue;
        // erst der billige Vergleich am Namen, dann die Regeln des Projekts
        if (!gerufen.heisst(aufruf)) continue;
        if (enthaelt(kandidaten(rufer.datei, aufruf), ziel)) aus.push_back(rufer);
    }
    std::sort(aus.begin(), aus.end(), frueher);
    return aus;
}

}  // namespace c2d::projekt
