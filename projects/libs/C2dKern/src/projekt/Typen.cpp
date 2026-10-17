/**
 * @file Typen.cpp
 * @brief Typnamen ueber die Dateien eines Projekts aufloesen
 */
#include "c2d/projekt/Typen.hpp"

#include <algorithm>

namespace c2d::projekt
{

namespace
{

/// Wie tief ein typedef auf ein typedef ... verfolgt wird.
constexpr int kMaxTiefe = 8;

void nimmAuf(std::vector<TypStelle>& ziel, const std::vector<TypStelle>& dazu)
{
    for (const TypStelle& s : dazu)
        if (std::none_of(ziel.begin(), ziel.end(), [&](const TypStelle& z) { return z == s; })) ziel.push_back(s);
}

}  // namespace

Typverzeichnis::Typverzeichnis(const Projekt& projekt) : m_projekt(&projekt)
{
    for (std::size_t d = 0; d < projekt.dateien.size(); ++d)
    {
        const modell::Einheit& e = projekt.dateien[d].ergebnis.einheit;
        for (std::size_t t = 0; t < e.typen.size(); ++t)
        {
            m_typen[e.typen[t].name].push_back({d, t});
            if (!e.typen[t].bereich.empty()) m_typen[e.typen[t].vollerName()].push_back({d, t});
        }
        for (const modell::Alias& a : e.aliase) m_aliase[a.name].emplace_back(d, a.ziel);
    }
}

std::vector<TypStelle> Typverzeichnis::inDatei(std::size_t datei, std::string_view name, int tiefe) const
{
    std::vector<TypStelle> aus;
    if (const auto t = m_typen.find(name); t != m_typen.end())
        for (const TypStelle& s : t->second)
            if (s.datei == datei) aus.push_back(s);
    if (!aus.empty() || tiefe >= kMaxTiefe) return aus;
    // ein typedef dieser Datei: der Typ, den es nennt, gesucht von hier aus
    if (const auto a = m_aliase.find(name); a != m_aliase.end())
        for (const auto& [d, ziel] : a->second)
        {
            if (d != datei) continue;
            for (const std::string& n : modell::bezeichnerIn(ziel))
            {
                if (n == name) continue;
                const std::vector<TypStelle> gefunden = suche(datei, n, tiefe + 1);
                if (!gefunden.empty()) return gefunden;
            }
        }
    return aus;
}

std::vector<TypStelle> Typverzeichnis::suche(std::size_t ausDatei, std::string_view name, int tiefe) const
{
    if (m_projekt == nullptr) return {};
    const Projekt& p = *m_projekt;
    // 1. die eigene Datei
    if (ausDatei < p.dateien.size())
        if (std::vector<TypStelle> hier = inDatei(ausDatei, name, tiefe); !hier.empty()) return hier;
    // 2. die eingebundenen Koepfe
    std::vector<TypStelle> aus;
    if (ausDatei < p.dateien.size())
        for (const std::size_t k : p.dateien[ausDatei].koepfe) nimmAuf(aus, inDatei(k, name, tiefe));
    if (!aus.empty()) return aus;
    // 3. jede andere Datei mit einem Typ oder typedef dieses Namens
    std::vector<std::size_t> dateien;
    if (const auto t = m_typen.find(name); t != m_typen.end())
        for (const TypStelle& s : t->second) dateien.push_back(s.datei);
    if (const auto a = m_aliase.find(name); a != m_aliase.end())
        for (const auto& [d, ziel] : a->second) dateien.push_back(d);
    std::sort(dateien.begin(), dateien.end());
    dateien.erase(std::unique(dateien.begin(), dateien.end()), dateien.end());
    for (const std::size_t d : dateien)
        if (d != ausDatei) nimmAuf(aus, inDatei(d, name, tiefe));
    return aus;
}

const modell::Typ* Typverzeichnis::eindeutig(std::size_t ausDatei, std::string_view name) const
{
    const std::vector<TypStelle> k = kandidaten(ausDatei, name);
    return k.size() == 1 && m_projekt != nullptr ? &typ(*m_projekt, k.front()) : nullptr;
}

std::vector<TypStelle> Typverzeichnis::kandidaten(std::size_t ausDatei, std::string_view name) const
{
    return suche(ausDatei, name, 0);
}

std::vector<TypVerweis> Typverzeichnis::verweiseIn(std::size_t ausDatei, std::string_view text) const
{
    std::vector<TypVerweis> aus;
    for (std::string& name : modell::bezeichnerIn(text))
    {
        // billig zuerst: heisst ueberhaupt etwas so?
        if (!m_typen.contains(name) && !m_aliase.contains(name)) continue;
        std::vector<TypStelle> ziele = kandidaten(ausDatei, name);
        if (!ziele.empty()) aus.push_back({std::move(name), std::move(ziele)});
    }
    return aus;
}

const modell::Typ& typ(const Projekt& projekt, const TypStelle& stelle)
{
    return projekt.dateien[stelle.datei].ergebnis.einheit.typen[stelle.typ];
}

}  // namespace c2d::projekt
