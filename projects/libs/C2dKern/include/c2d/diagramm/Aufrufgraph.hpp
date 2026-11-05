/**
 * @file Aufrufgraph.hpp
 * @brief Das Diagramm-Modell eines Aufrufgraphen: Funktionen und wer wen ruft
 *
 * Ein Graph, kein Baum: jede Funktion ist ein Knoten, je rufendem Paar eine
 * Kante; eine Rekursion ist eine Kante zurueck oder eine Schlaufe. Was ein
 * Aufruf meint, entscheidet das Projekt (projekt/Aufrufgraph.hpp) — dieses
 * Modell kennt weder Projekt noch Sprache. Gegenstueck zu Flussdiagramm.hpp.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

enum class AufrufKnotenArt
{
    Funktion,  ///< eine Funktion mit Rumpf im Projekt
    Fremd,     ///< gerufen, aber ohne Rumpf im Projekt: Bibliothek, SDK, ein Zeiger
    Makro,     ///< ein Makro mit Parametern, das das Projekt definiert
    // die Abhaengigkeiten der Daten (Entscheid A1): dasselbe Modell mit Objekten der Datenbank
    Tabelle,        ///< eine Tabelle des Projekts
    FremdeTabelle,  ///< eine Tabelle, die das Projekt nicht definiert (grau)
    View,
    Trigger,
    Routine,  ///< eine Funktion oder Prozedur der Datenbank
};

/// Steht der Knoten fuer etwas des Projekts (in einem Rahmen seiner Datei), nicht fuer Fremdes?
[[nodiscard]] inline bool imProjekt(AufrufKnotenArt art)
{
    return art != AufrufKnotenArt::Fremd && art != AufrufKnotenArt::Makro && art != AufrufKnotenArt::FremdeTabelle;
}

struct AufrufKnoten
{
    AufrufKnotenArt art = AufrufKnotenArt::Funktion;
    std::string name;  ///< der volle Name; bei Fremd und Makro, wie er gerufen wird
    /// Nur Abhaengigkeiten: was der Knoten ist, vor dem Namen gezeigt (`«table»`, `«procedure»`); leer = nichts.
    std::string stereotyp;
    /// Stelle in Aufrufgraph::dateien; kKeine bei Fremd und Makro (sie stehen ausserhalb der Rahmen).
    std::size_t datei = kKeine;
    int zeile = 0;        ///< die Zeile der Funktion; 0 = keine
    bool wurzel = false;  ///< der Ausgangspunkt
    /// An der Grenze der Tiefe: ruft (nach oben: wird gerufen) noch weiter, gezeigt ist es nicht.
    bool weiter = false;
};

enum class AufrufKantenArt
{
    Sicher,      ///< der Aufruf meint genau diese Funktion
    Mehrdeutig,  ///< er kann mehrere meinen; je Kandidat eine solche Kante
};

/// Nur Abhaengigkeiten (Entscheid A2): was eine Kante sagt.
enum class ZugriffKante
{
    Ruft,          ///< wie im Aufrufgraph; ohne Beschriftung
    Liest,         ///< `reads`
    Schreibt,      ///< `writes`
    LiestSchreibt, ///< `reads, writes`
    LoestAus,      ///< von der Tabelle zu ihrem Trigger, beschriftet mit dem Ereignis
};

struct AufrufKante
{
    std::size_t von = 0;   ///< wer ruft
    std::size_t nach = 0;  ///< wer gerufen wird
    AufrufKantenArt art = AufrufKantenArt::Sicher;
    ZugriffKante zugriff = ZugriffKante::Ruft;
    std::string ereignis;  ///< nur LoestAus: `AFTER INSERT`
    /// Unter welcher Bedingung der Uebersetzung gerufen wird; leer = immer. Mehrere
    /// Aufrufe unter verschiedenen Bedingungen: mit ` || ` verbunden.
    std::string bedingung;
    /// false: nur aus Zweigen von Varianten, die mit den Definitionen nicht gelten.
    bool gilt = true;
};

struct Aufrufgraph
{
    std::string titel;
    /// Die Dateien der Funktionen, in der Reihenfolge ihres ersten Vorkommens. Sind
    /// es mehrere, steht jede in einem Rahmen mit ihrem Pfad als Titel.
    std::vector<std::string> dateien;
    std::vector<AufrufKnoten> knoten;
    std::vector<AufrufKante> kanten;

    /// Stehen die Funktionen in Rahmen je Datei?
    [[nodiscard]] bool mitRahmen() const { return dateien.size() > 1; }
};

/// Die Beschriftung einer Kante, fuer alle Formate gleich: `?` bei einer mehrdeutigen,
/// die Bedingung bei einer, die nicht gilt (beides: `? bedingung`); sonst leer — eine
/// Bedingung, die gilt, zeigt die Farbe (farbenDerBedingungen).
[[nodiscard]] std::string beschriftung(const AufrufKante& kante);

/// Je Kante ihre Farbe (0 .. kRahmenFarben-1, Flussdiagramm.hpp), wenn sie unter einer
/// Bedingung steht und gilt; gleiche Bedingung, gleiche Farbe, vergeben in der
/// Reihenfolge des ersten Vorkommens. kKeine sonst.
[[nodiscard]] std::vector<std::size_t> farbenDerBedingungen(const Aufrufgraph& graph);

/// Der Text eines Knotens: sein Name, an der Grenze der Tiefe mit ` …` dahinter.
[[nodiscard]] std::string knotenText(const AufrufKnoten& knoten);

/// Die Zeilen eines Knotens: der Stereotyp (wenn es einen gibt), dann knotenText.
[[nodiscard]] std::vector<std::string> knotenZeilen(const AufrufKnoten& knoten);

}  // namespace c2d::diagramm
