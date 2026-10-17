/**
 * @file Ecma.hpp
 * @brief Der Leser von JavaScript und TypeScript: beide teilen ihn
 *
 * Was TypeScript dazubringt (Typen an Parametern und Gliedern, interface, type,
 * enum, namespace, Sichtbarkeit), steht im `Dialekt`: die Woerter sind in beiden
 * Sprachen weich — `type` ist in JavaScript ein gewoehnlicher Name.
 *
 * Intern (nicht unter include/): ecma/Sprachen.cpp bindet diesen Kopf ein.
 */
#pragma once

#include "c2d/sprache/Sprache.hpp"

#include <string_view>

namespace c2d::sprache::ecma
{

struct Dialekt
{
    /// TypeScript: Typen hinter `:`, Typ-Parameter `<T>` in Deklarationen, interface, type, enum,
    /// namespace, declare, Sichtbarkeit und abstract.
    bool ts = false;
};

/// Die Sprachtabelle, die beide teilen.
[[nodiscard]] const lexer::Sprachtabelle& tabelle();

/// Liest eine Datei ins Codemodell. Funktionen sind auch, was ohne `function name` dasteht: eine
/// Pfeil- oder `function`-Funktion mit Block-Rumpf, benannt nach dem, dem sie zugewiesen wird
/// (`const f =`, `x.f =`, `f:` in einem Objekt, `f() {…}` in einem Objekt), sonst
/// `<anonymous:Zeile>` mit der umgebenden Funktion als `aussen`; in der Anweisung, in der sie steht,
/// bleibt sie als `{…}`. Was auf oberster Ebene ablaeuft, ist die Funktion `<module>`.
[[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName, Dialekt dialekt);

}  // namespace c2d::sprache::ecma
