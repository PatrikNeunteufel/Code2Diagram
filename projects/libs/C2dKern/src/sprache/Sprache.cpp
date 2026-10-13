/**
 * @file Sprache.cpp
 * @brief Die Liste der Sprachen
 */
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>

namespace c2d::sprache
{

Definition definitionAus(std::string_view text)
{
    constexpr std::string_view kLeer = " \t\r\n";
    const std::size_t gleich = text.find('=');
    std::string_view name = text.substr(0, gleich);
    const std::size_t a = name.find_first_not_of(kLeer);
    name = a == std::string_view::npos ? std::string_view() : name.substr(a, name.find_last_not_of(kLeer) - a + 1);
    Definition d;
    d.name = std::string(name);
    if (gleich != std::string_view::npos) d.wert = std::string(text.substr(gleich + 1));
    return d;
}

std::span<const Sprache* const> sprachen()
{
    // Eine neue Sprache hier eintragen.
    static const Sprache* const liste[] = {&c(), &cpp()};
    return liste;
}

const Sprache* spracheFuer(std::string_view dateiname, std::string_view inhalt)
{
    const std::size_t punkt = dateiname.find_last_of('.');
    if (punkt == std::string_view::npos) return nullptr;
    std::string endung(dateiname.substr(punkt + 1));
    std::transform(endung.begin(), endung.end(), endung.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    });
    const Sprache* erste = nullptr;
    for (const Sprache* s : sprachen())
    {
        const auto endungen = s->endungen();
        if (std::find(endungen.begin(), endungen.end(), std::string_view(endung)) == endungen.end()) continue;
        if (s->erkennt(inhalt)) return s;
        if (erste == nullptr) erste = s;
    }
    return erste;
}

}  // namespace c2d::sprache
