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

#include <cstddef>
#include <utility>

namespace c2d::sprache::cfamilie
{

/// Ein #if im Rumpf einer Funktion, das als Varianten stehen bleibt.
struct Variantengruppe
{
    /// Der Versatz im Text von #if, jedem #elif / #else und #endif.
    std::vector<std::size_t> direktiven;
    /// Je Zweig seine Bedingung, gekuerzt: `#if A` -> `A`, `#ifdef A` ->
    /// `defined(A)`, `#ifndef A` -> `!defined(A)`, `#else` -> `else`.
    std::vector<std::string> bedingungen;
    /// Der Zweig, der gilt; bedingungen.size() = das fehlende #else;
    /// kKeinZweig = keiner (die Gruppe steht selbst in einem Zweig, der nicht gilt).
    std::size_t geltend = kKeinZweig;
};

/// Was Varianten braucht: hinein die Ruempfe, heraus die Gruppen.
struct Varianten
{
    /// Die Ruempfe der Funktionen, als Versatz [von, bis) — aus einem ersten Lesen.
    std::vector<std::pair<std::size_t, std::size_t>> ruempfe;
    /// Nur der Zweig, der gilt, bleibt stehen (die uebrigen werden ausgeblendet
    /// wie beim Auswerten); die Direktiven der Gruppe bleiben trotzdem.
    bool nurGeltend = false;
    /// In der Reihenfolge des Texts.
    std::vector<Variantengruppe> gruppen;
};

/// Der Text mit ausgeblendeten Zweigen. Die Direktiven der Bedingungen selbst
/// werden ebenfalls ausgeblendet; #define und #include in geltenden Zweigen bleiben.
///
/// Mit `varianten`: ein #if, das ganz in einem der Ruempfe liegt und dessen
/// Zweige je ganze Anweisungen enthalten, bleibt mit allen Zweigen und seinen
/// Direktiven stehen und kommt in `varianten->gruppen`. Ganze Anweisungen
/// heisst: je Zweig gehen die Klammern auf, er beginnt nicht mit `else`,
/// `case` oder `default`, enthaelt keine Fall-Marke und endet auf `;` oder `}`
/// (oder ist leer); vor dem #if endet eine Anweisung (`;`, `{`, `}`, `:`), hinter
/// dem #endif steht kein `else`; jedes #if darin erfuellt das selbst. Ein #if im
/// Rumpf, das es nicht erfuellt, wird ausgewertet und bekommt einen Hinweis.
///
/// @param definitionen  gelten vom Anfang der Datei an
/// @param cpp           `__cplusplus` ist definiert
/// @param meldungen     ein #endif ohne #if, ein #if ohne #endif; als Hinweis:
///                      eine Bedingung, die sich nicht ausrechnen laesst — ihr Zweig wird gelesen
/// @param unbekannt     Namen, nach denen gefragt wurde und die die Datei nirgends definiert
/// @param schalter      wenn gegeben: wonach die Bedingungen fragen (Ergebnis::schalter)
/// @param weichen       wenn gegeben: die Ketten #if/#elif/#else und was ihre Zweige waehlt (Ergebnis::weichen)
[[nodiscard]] std::string werteBedingteAus(std::string_view text, const lexer::Sprachtabelle& tabelle,
                                           const std::vector<Definition>& definitionen, bool cpp,
                                           std::vector<quelle::Meldung>& meldungen,
                                           std::vector<std::string>& unbekannt, Varianten* varianten = nullptr,
                                           std::vector<Schalter>* schalter = nullptr,
                                           std::vector<Weiche>* weichen = nullptr);

}  // namespace c2d::sprache::cfamilie
