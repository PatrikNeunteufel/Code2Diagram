/**
 * @file Text.hpp
 * @brief Was alle Formate mit Text tun: kuerzen
 *
 * Intern (nicht unter include/).
 */
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace c2d::ausgabe
{

/// Hoechstens `maxLaenge` Bytes; laengeres endet auf "...", ohne ein
/// UTF-8-Zeichen zu trennen. 0 = nicht kuerzen. Vor dem Maskieren anwenden.
[[nodiscard]] inline std::string kuerze(std::string_view zeile, std::size_t maxLaenge)
{
    constexpr std::string_view kRest = "...";
    if (maxLaenge == 0 || zeile.size() <= maxLaenge) return std::string(zeile);
    std::size_t ende = maxLaenge > kRest.size() ? maxLaenge - kRest.size() : 0;
    while (ende > 0 && (static_cast<unsigned char>(zeile[ende]) & 0xC0) == 0x80) --ende;
    return std::string(zeile.substr(0, ende)) + std::string(kRest);
}

}  // namespace c2d::ausgabe
