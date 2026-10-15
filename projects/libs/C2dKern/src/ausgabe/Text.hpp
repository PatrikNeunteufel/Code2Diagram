/**
 * @file Text.hpp
 * @brief Was mehrere Formate gemeinsam tun: kuerzen, fuer XML maskieren, die Anordnung waehlen
 *
 * Intern (nicht unter include/).
 */
#pragma once

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"

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

/// Text als Inhalt oder Attributwert in XML (draw.io, SVG).
[[nodiscard]] inline std::string xml(std::string_view text)
{
    std::string aus;
    for (const char c : text)
    {
        switch (c)
        {
            case '&': aus += "&amp;"; break;
            case '<': aus += "&lt;"; break;
            case '>': aus += "&gt;"; break;
            case '"': aus += "&quot;"; break;
            case '\r': break;
            case '\n': aus += "&#10;"; break;
            default: aus += c; break;
        }
    }
    return aus;
}

/// Die Anordnung fuer ein Format mit festen Orten.
[[nodiscard]] inline diagramm::AnordnungOptionen anordnung(const Optionen& optionen)
{
    diagramm::AnordnungOptionen a;
    a.nachRechts = optionen.richtung == "LR";
    a.maxZeilenLaenge = optionen.maxZeilenLaenge;
    return a;
}

}  // namespace c2d::ausgabe
