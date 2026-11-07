/**
 * @file Kette.hpp
 * @brief Aufrufketten in Anweisungen — fuer alle Sprachen, nach ihren Regeln (Entscheid F1)
 *
 * Intern (nicht unter include/). Ein Leser ruft ergaenzeKetten() am Ende: jede Anweisung bekommt ihre
 * Aufrufketten (modell::Anweisung::ketten) — `df.withColumn(…).select(…)`, eine Klammer um die ganze Kette
 * faellt weg, Ketten in Argumenten stehen mit ihrer Stelle darin. Was ein Glied bedeutet, weiss erst die
 * Ableitung (diagramm/Datenfluss: Spark).
 */
#pragma once

#include "Logik.hpp"
#include "c2d/lexer/Lexer.hpp"
#include "c2d/modell/Code.hpp"

#include <string_view>
#include <vector>

namespace c2d::sprache
{

/// Die Ketten eines Ausdrucks oder einer Anweisung; `zeile` ist die Zeile, in der `text` beginnt. Eine
/// Zuweisung `ziel = …` (der erste Operator aus regeln.zuweisungen auf oberster Ebene, nur `=`) setzt bei
/// der Kette, die ihr ganzer Wert ist, `ziel`.
[[nodiscard]] std::vector<modell::Aufrufkette> kettenIn(std::string_view text, int zeile,
                                                        const lexer::Sprachtabelle& tabelle,
                                                        const Ausdrucksregeln& regeln);

/// Fuer jede Anweisung jeder Funktion: ihre Ketten (Einfach, Ruecksprung, Wurf, Mit; der Kopf von if,
/// Schleifen und Auswahl).
void ergaenzeKetten(modell::Einheit& einheit, const lexer::Sprachtabelle& tabelle, const Ausdrucksregeln& regeln);

}  // namespace c2d::sprache
