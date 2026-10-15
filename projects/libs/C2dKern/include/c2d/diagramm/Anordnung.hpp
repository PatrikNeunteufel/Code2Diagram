/**
 * @file Anordnung.hpp
 * @brief Wo die Knoten eines Diagramms stehen: Rechtecke und Linienzuege
 *
 * Formate wie Mermaid und DOT ordnen selbst an; wer zeichnet (die Vorschau im
 * Fenster) oder ein Format mit festen Orten schreibt (draw.io), braucht die
 * Anordnung von hier. Ohne Qt: die Groesse eines Texts wird geschaetzt —
 * Zeichen mal `zeichenBreite` —, gedacht fuer eine Schrift fester Breite.
 *
 * Das Verfahren ist einfach gehalten: Schichten nach dem laengsten Weg; eine
 * Kante ueber mehrere Schichten bekommt in jeder Schicht dazwischen einen
 * Stuetzpunkt und damit eine eigene Spalte; Reihenfolge je Schicht nach dem
 * Schwerpunkt der Nachbarn; Kanten zurueck aussen herum. Es vermeidet nicht
 * jede Kreuzung.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"

#include <cstddef>
#include <vector>

namespace c2d::diagramm
{

struct Punkt
{
    double x = 0;
    double y = 0;
};

struct Rechteck
{
    double x = 0;  ///< links
    double y = 0;  ///< oben
    double breite = 0;
    double hoehe = 0;

    [[nodiscard]] Punkt mitte() const { return {x + breite / 2, y + hoehe / 2}; }
};

using Linienzug = std::vector<Punkt>;

struct AnordnungOptionen
{
    bool nachRechts = false;  ///< das Diagramm waechst nach rechts statt nach unten
    double zeichenBreite = 7.2;
    double zeilenHoehe = 17;
    /// Wie bei der Ausgabe: laengere Zeilen werden gekuerzt gezeigt; 0 = nie.
    std::size_t maxZeilenLaenge = 60;
};

struct FlussAnordnung
{
    std::vector<Rechteck> knoten;    ///< je Knoten, in dessen Reihenfolge
    std::vector<Linienzug> kanten;   ///< je Kante: vom Rand der Quelle zum Rand des Ziels
    /// je Kante: die Mitte ihrer Beschriftung — auf dem Stueck, das nur ihr gehoert,
    /// nah an der Stelle, wo sie sich von den anderen Kanten ihrer Quelle trennt
    std::vector<Punkt> beschriftungen;
    std::vector<Rechteck> gruppen;   ///< je Gruppe der Rahmen um alles, was in ihr liegt; oben Platz fuer den Titel
    double breite = 0;
    double hoehe = 0;
};

struct StrukturAnordnung
{
    std::vector<Rechteck> kaesten;
    std::vector<Linienzug> beziehungen;  ///< vom Rand des einen Kastens zum Rand des anderen
    double breite = 0;
    double hoehe = 0;
};

[[nodiscard]] FlussAnordnung ordneAn(const Flussdiagramm& diagramm, const AnordnungOptionen& optionen = {});
[[nodiscard]] StrukturAnordnung ordneAn(const Strukturdiagramm& diagramm, const AnordnungOptionen& optionen = {});

/// Die Hoehe des Kopfs eines Kastens (Art und Name) bei diesen Optionen.
[[nodiscard]] double kopfHoehe(const AnordnungOptionen& optionen);

}  // namespace c2d::diagramm
