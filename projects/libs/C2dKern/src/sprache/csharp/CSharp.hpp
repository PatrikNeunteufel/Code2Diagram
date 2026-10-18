/**
 * @file CSharp.hpp
 * @brief Der Leser von C# und Java: beide teilen ihn (Entscheid G1)
 *
 * Was Java anders schreibt, steht im `Dialekt`: eigene Woerter und Modifikatoren, Annotationen
 * statt Attributen, `extends`/`implements` statt `:`, `->` statt `=>`, anonyme Klassen,
 * Initialisierungsbloecke, Marken an Schleifen; keine Eigenschaften, keine bedingte Uebersetzung.
 *
 * Intern (nicht unter include/): csharp/Sprachen.cpp bindet diesen Kopf ein.
 */
#pragma once

#include "c2d/sprache/Sprache.hpp"

#include <string_view>

namespace c2d::sprache::csjava
{

struct Dialekt
{
    bool java = false;
};

/// Die Sprachtabelle des Dialekts.
[[nodiscard]] const lexer::Sprachtabelle& tabelle(Dialekt dialekt);

/// Liest eine Datei ins Codemodell; in C# zuvor `#if` ausgewertet (LeseOptionen), in Java gelten
/// die LeseOptionen nicht.
[[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName, const LeseOptionen& optionen,
                            Dialekt dialekt);

}  // namespace c2d::sprache::csjava
