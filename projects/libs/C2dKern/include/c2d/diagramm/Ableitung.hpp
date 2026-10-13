/**
 * @file Ableitung.hpp
 * @brief Vom Codemodell zum Diagramm
 *
 * Hier steht, WIE aus Code ein Diagramm wird — unabhaengig von der Sprache
 * (die steckt im Codemodell) und vom Format (das steckt in ausgabe/).
 * Eine neue Diagrammart bekommt hier ihre Funktion und in diagramm/ ihr Modell.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"
#include "c2d/modell/Code.hpp"

namespace c2d::diagramm
{

/// Der Ablauf einer Funktion: je Anweisung ein Knoten, ungebuendelt
/// (fasseAnweisungsfolgenZusammen macht es lesbar). Eine Funktion ohne Rumpf
/// gibt ein leeres Diagramm.
///
/// - if, while, do, for: eine Verzweigung mit den Kanten `yes` und `no`
/// - switch: eine Verzweigung, je Fall eine Kante mit seinem Wert; ohne
///   default eine Kante `otherwise`
/// - break, continue, goto: kein Knoten, die Kante fuehrt zum Ziel
/// - eine Marke: ein eigener Knoten `name:`
/// - eine Anweisung, die nur eine Funktion ruft: ein Knoten der Art Aufruf
/// - Praeprozessor-Zeilen im Rumpf: kein Knoten
[[nodiscard]] Flussdiagramm flussVon(const modell::Funktion& funktion);

/// Die Datentypen einer Einheit: je struct/union/enum ein Kasten. Ein Glied,
/// dessen Typ einen anderen Kasten nennt (auch ueber einen typedef), gibt eine
/// Beziehung: als Wert `Enthaelt`, ueber einen Zeiger oder zu einer
/// Aufzaehlung `Verweist`.
[[nodiscard]] Strukturdiagramm strukturVon(const modell::Einheit& einheit);

}  // namespace c2d::diagramm
