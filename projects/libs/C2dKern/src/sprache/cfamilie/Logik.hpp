/**
 * @file Logik.hpp
 * @brief Die Logik einer Bedingung der C-Familie: `&&`, `||`, `!` (in C++ auch and, or, not), Klammern
 *
 * Intern (nicht unter include/). Der Leser ruft ergaenzeLogik() am Ende: jede
 * Anweisung, deren Bedingung mindestens zwei Atome hat, bekommt ihren Baum
 * (modell::Anweisung::logik) — die Wahrheitstabelle baut die Ableitung daraus.
 */
#pragma once

#include "c2d/lexer/Lexer.hpp"
#include "c2d/modell/Code.hpp"

#include <optional>
#include <string_view>

namespace c2d::sprache::cfamilie
{

/// Die Logik eines Ausdrucks. Ein Atom ist, was zwischen den logischen Operatoren steht;
/// `!` gilt als Nicht nur vor einem Einzelnen (einem Namen, Aufruf, Glied, einer Klammer) —
/// `!x == y` ist ein Vergleich, ein Atom. Ein `?:` auf oberster Ebene ist ein Atom.
/// nullopt = weniger als zwei verschiedene Atome (gleicher Text ist dasselbe Atom).
[[nodiscard]] std::optional<modell::Logik> logikVon(std::string_view ausdruck, const lexer::Sprachtabelle& tabelle);

/// Fuer jede Anweisung jeder Funktion: welcher Ausdruck eine Wahrheitstabelle bekommt —
/// die Bedingung von if, while, do, for; bei einer Anweisung oder einem return die Bedingung
/// eines `?:`, sonst die rechte Seite einer Zuweisung (oder der ganze Ausdruck), wenn sie
/// `&&`/`||` enthaelt — und seine Logik, wenn sie mindestens zwei Atome hat.
void ergaenzeLogik(modell::Einheit& einheit, const lexer::Sprachtabelle& tabelle);

}  // namespace c2d::sprache::cfamilie
