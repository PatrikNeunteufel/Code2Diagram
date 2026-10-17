/**
 * @file Farben.hpp
 * @brief Die Farben eines Diagramms, je hell und dunkel — eine Tabelle fuer Vorschau und Formate
 *
 * Was man in der Vorschau sieht, steht so in der Datei: die Vorschau nimmt die
 * Spalte zum Thema des Fensters, SVG schreibt beide (`prefers-color-scheme`),
 * die Formate ohne dunkle Fassung (Mermaid, DOT, draw.io) die helle. Die Farben
 * der Rahmen um Bedingungen stehen bei rahmenFarbe (Flussdiagramm.hpp) — sie
 * sind auf hell und dunkel dieselben.
 */
#pragma once

#include <array>
#include <string_view>

namespace c2d::diagramm
{

enum class Farbe
{
    Grund,        ///< hinter dem Diagramm
    Flaeche,      ///< in einem Knoten, einem Kasten, unter einem Schild
    Getoent,      ///< Start und Ende, der Kopf eines Kastens, der Ausgangspunkt eines Aufrufgraphen
    Strich,       ///< Raender, Kanten, Spitzen
    Text,
    Rahmen,       ///< der gestrichelte Rahmen einer Einbettung oder einer Datei
    Fremd,        ///< Flaeche eines Aufrufs ohne Rumpf im Projekt
    FremdStrich,  ///< sein Rand
    Verweis,      ///< was weiterfuehrt: Rand eines Aufrufs, Links, die Marke
};

struct FarbEintrag
{
    Farbe farbe;
    std::string_view name;    ///< fuer CSS-Klassen und Meldungen, etwa `getoent`
    std::string_view hell;    ///< `#rrggbb`
    std::string_view dunkel;  ///< `#rrggbb`
};

/// Die ganze Tabelle, in der Reihenfolge von Farbe.
[[nodiscard]] const std::array<FarbEintrag, 9>& farbTabelle();

/// Eine Farbe als `#rrggbb`, hell oder dunkel.
[[nodiscard]] std::string_view farbe(Farbe welche, bool dunkel = false);

}  // namespace c2d::diagramm
