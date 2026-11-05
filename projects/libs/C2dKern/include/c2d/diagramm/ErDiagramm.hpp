/**
 * @file ErDiagramm.hpp
 * @brief Ein Strukturdiagramm mit Tabellen in Chen- oder (min,max)-Notation
 *
 * Entitaeten (Rechtecke) mit ihren Attributen (Ellipsen, der Primaerschluessel
 * unterstrichen), Beziehungen (Rauten) mit Linien zu ihren Entitaeten; an jeder
 * Linie, am Ende der Entitaet, `1` / `n` / `m` (Chen) oder `(min,max)`.
 * Abgeleitet aus einem Strukturdiagramm (Entscheide N1-N4, Patrik 2026-11-05):
 * je Fremdschluessel eine Raute 1:n; eine Tabelle, deren Primaerschluessel aus
 * mindestens zwei Spalten besteht, die alle zu Fremdschluesseln gehoeren, ist
 * selbst die Raute m:n. Kaesten, die keine Tabellen sind, bleiben Kaesten (UML).
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"  // kKeine
#include "c2d/diagramm/Strukturdiagramm.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

enum class ErKnotenArt
{
    Entitaet,   ///< eine Tabelle
    Beziehung,  ///< eine Raute: ein Fremdschluessel (1:n, 1:1) oder eine Verbindungstabelle (m:n)
    Kasten,     ///< ein Kasten, der keine Tabelle ist (Klasse, Struktur): wie im Strukturdiagramm
};

struct ErAttribut
{
    std::string name;
    bool schluessel = false;       ///< Teil des Primaerschluessels: unterstrichen
    std::size_t eintrag = kKeine;  ///< Stelle in Kasten::eintraege seines Kastens (fuer den Klick)
};

struct ErKnoten
{
    ErKnotenArt art = ErKnotenArt::Entitaet;
    std::string name;
    std::vector<ErAttribut> attribute;  ///< Entitaet; Raute einer Verbindungstabelle
    std::size_t kasten = kKeine;        ///< der Kasten im Strukturdiagramm, aus dem er kommt
    /// Raute eines Fremdschluessels: dessen erste Spalte (Stelle in eintraege des Kastens)
    std::size_t eintrag = kKeine;
    // nur Kasten: sein Stereotyp (leer = keiner) und seine Zeilen, die Felder vor den Methoden
    std::string stereotyp;
    std::vector<std::string> zeilen;
    std::size_t felder = 0;  ///< so viele der Zeilen sind Felder; danach ein Strich, dann die Methoden
};

/// Eine Linie zwischen zwei Knoten; nach unten (rechts) von `von` nach `nach`.
struct ErKante
{
    std::size_t von = 0;
    std::size_t nach = 0;
    std::string text;           ///< `1`, `n`, `(0,*)`; zwischen Kaesten: die Beschriftung der Beziehung
    bool textAmVon = false;     ///< steht am Ende `von` (sonst am Ende `nach`); zwischen Kaesten: mittig
    bool zwischenKaesten = false;          ///< eine Beziehung zweier Kaesten, gezeichnet wie im Strukturdiagramm
    BeziehungArt art = BeziehungArt::Verweist;  ///< nur zwischenKaesten
};

struct ErDiagramm
{
    std::string titel;
    bool minMax = false;  ///< (min,max) statt Chen
    std::vector<ErKnoten> knoten;
    std::vector<ErKante> kanten;
};

/// Das Strukturdiagramm in Chen bzw. (min,max) — je nach diagramm.notation (Tables und Crow's foot
/// gelten wie Chen) — mit den Attributen nach diagramm.attribute.
[[nodiscard]] ErDiagramm erDiagrammVon(const Strukturdiagramm& diagramm);

/// Zeichnet dieses Strukturdiagramm als ER-Diagramm (Chen, (min,max))? Nur mit einer Tabelle darin.
[[nodiscard]] bool alsErDiagramm(const Strukturdiagramm& diagramm);

}  // namespace c2d::diagramm
