/**
 * @file Schaltstellen.hpp
 * @brief Wo schaltet ein Name im Projekt — abgeleitet aus den Ketten `#if` aller Dateien
 *
 * Die Zeilen kommen aus den Konfigurationstabellen der Dateien (diagramm::konfigurationVon):
 * ein Zweig gehoert dazu, wenn seine Tabelle eine Spalte fuer den Namen hat und eine seiner
 * Alternativen etwas von ihm fordert; ein Zweig, der sich nicht zerlegen liess, wenn seine
 * Bedingung oder die eines umgebenden Zweigs den Namen nennt. Ketten eines Kopfs stehen in der
 * Datei des Kopfs, einmal. Der Stand ist der der Datei, dort, wo sie zuerst nach ihm fragt.
 * Ein `#define` oder `#undef` in einem Zweig, der nicht gilt, sieht der Leser nicht.
 */
#pragma once

#include "c2d/diagramm/Schaltstellen.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <span>
#include <string_view>

namespace c2d::projekt
{

/// Wo `name` schaltet. `aussen`: die Definitionen von aussen (`-D`, das Feld Defines), mit
/// denen das Projekt gelesen ist. Keine Zeile und keine Setzung: der Name kommt nicht vor.
[[nodiscard]] diagramm::Schaltstellen schaltstellen(const Projekt& projekt, std::string_view name,
                                                    std::span<const sprache::Definition> aussen = {});

/// Je Name, nach dem eine Datei des Projekts fragt, eine Zeile.
[[nodiscard]] diagramm::Schalteruebersicht schalteruebersicht(const Projekt& projekt,
                                                             std::span<const sprache::Definition> aussen = {});

}  // namespace c2d::projekt
