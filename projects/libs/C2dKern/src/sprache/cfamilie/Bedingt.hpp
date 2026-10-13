/**
 * @file Bedingt.hpp
 * @brief Bedingte Uebersetzung der C-Familie auswerten: #if, #ifdef, #elif, #else, #endif
 *
 * Die Stufe vor dem Leser: sie entscheidet je Zweig, ob er gilt, und liefert
 * den Text so zurueck, dass der Leser nur sieht, was gilt. Die Laenge und die
 * Zeilen bleiben — was nicht gilt, wird zu Leerzeichen —, also stimmen Zeile
 * und Spalte jeder spaeteren Meldung mit der Datei ueberein.
 *
 * Gerechnet wird wie im Compiler: mit den #define, die bis dorthin galten
 * (auch #undef), und mit den Definitionen von aussen; ein Name, der nicht
 * definiert ist, zaehlt 0. Makros im uebrigen Code werden NICHT ersetzt.
 *
 * Intern (nicht unter include/).
 */
#pragma once

#include "c2d/sprache/Sprache.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace c2d::sprache::cfamilie
{

/// Der Text mit ausgeblendeten Zweigen. Die Direktiven der Bedingungen selbst
/// werden ebenfalls ausgeblendet; #define und #include in geltenden Zweigen bleiben.
///
/// @param definitionen  gelten vom Anfang der Datei an
/// @param cpp           `__cplusplus` ist definiert
/// @param meldungen     ein #endif ohne #if, ein #if ohne #endif; als Hinweis:
///                      eine Bedingung, die sich nicht ausrechnen laesst — ihr Zweig wird gelesen
/// @param unbekannt     Namen, nach denen gefragt wurde und die die Datei nirgends definiert
[[nodiscard]] std::string werteBedingteAus(std::string_view text, const lexer::Sprachtabelle& tabelle,
                                           const std::vector<Definition>& definitionen, bool cpp,
                                           std::vector<quelle::Meldung>& meldungen,
                                           std::vector<std::string>& unbekannt);

}  // namespace c2d::sprache::cfamilie
