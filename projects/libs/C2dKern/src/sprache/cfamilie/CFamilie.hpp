/**
 * @file CFamilie.hpp
 * @brief Der Leser der C-Familie: C und C++ teilen ihn
 *
 * Die Grammatik beider Sprachen ueberdeckt sich weit; was C++ dazubringt,
 * haengt fast immer an einem Schluesselwort (namespace, class, template, try
 * ...). Der Leser fragt deshalb die Token: ein Wort, das die Sprachtabelle
 * nicht als Schluesselwort fuehrt, ist fuer ihn ein gewoehnlicher Name — die
 * Tabelle der Sprache schaltet so ihre Regeln selbst frei. Was sich nicht an
 * einem Wort zeigt, steht im `Dialekt`.
 *
 * Intern (nicht unter include/): die Sprachen c/ und cpp/ binden diesen Kopf ein.
 */
#pragma once

#include "c2d/sprache/Sprache.hpp"

#include <string_view>

namespace c2d::sprache::cfamilie
{

struct Dialekt
{
    /// C++: `<...>` klammert in Deklarationen (Schablonen), und die Glieder
    /// eines Typs haben eine Sichtbarkeit.
    bool cpp = false;
};

/// Liest eine Datei der C-Familie ins Codemodell.
///
/// Versteht die Gliederung, nicht die Bedeutung: Funktionen mit ihren
/// Anweisungen, Typen mit ihren Gliedern, typedef/using, Variablen, #include
/// und #define; in C++ dazu Namensraeume, Klassen mit Methoden, Basen und
/// Sichtbarkeit, Konstruktoren, Operatoren, try/catch/throw, range-for.
/// Ausdruecke bleiben Text.
///
/// Bedingte Uebersetzung wird vorher ausgewertet (Bedingt.hpp), ausser
/// `optionen.alleZweige` ist gesetzt.
///
/// Grenzen (README, "Was es noch nicht kann"): Makros werden nicht ersetzt;
/// Typen werden nicht aufgeloest — ein unbekannter Name vor einem Bezeichner
/// gilt als Typ; Schablonen werden uebergangen, nicht verstanden.
[[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                            const lexer::Sprachtabelle& tabelle, Dialekt dialekt, const LeseOptionen& optionen);

}  // namespace c2d::sprache::cfamilie
