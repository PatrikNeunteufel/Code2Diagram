/**
 * @file Zeilen.hpp
 * @brief Wo in einem Text die Zeilen beginnen: Versatz -> Stelle und zurueck
 *
 * Intern (nicht unter include/).
 */
#pragma once

#include "c2d/quelle/Meldung.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

namespace c2d::projekt
{

/// Die Stelle (Zeile, Spalte) eines Versatzes in einem Text.
class Zeilen
{
public:
    explicit Zeilen(std::string_view text)
    {
        m_anfang.push_back(0);
        for (std::size_t i = 0; i < text.size(); ++i)
            if (text[i] == '\n') m_anfang.push_back(i + 1);
    }

    [[nodiscard]] quelle::Position ort(std::size_t versatz) const
    {
        const auto z = std::upper_bound(m_anfang.begin(), m_anfang.end(), versatz) - 1;
        return {versatz, static_cast<int>(z - m_anfang.begin()) + 1, static_cast<int>(versatz - *z) + 1};
    }

    /// Wo Zeile z (ab 1) beginnt.
    [[nodiscard]] std::size_t anfang(int z) const
    {
        return m_anfang[std::clamp<std::size_t>(static_cast<std::size_t>(std::max(z, 1)) - 1, 0, m_anfang.size() - 1)];
    }

    /// Wie viele Zeilen der Text hat (ein Text ohne Zeilenwechsel: eine).
    [[nodiscard]] int anzahl() const { return static_cast<int>(m_anfang.size()); }

private:
    std::vector<std::size_t> m_anfang;
};

}  // namespace c2d::projekt
