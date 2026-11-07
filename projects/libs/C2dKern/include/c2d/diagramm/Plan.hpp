/**
 * @file Plan.hpp
 * @brief Der Plan von Spark als Diagramm (Spark Schicht 2, Entscheide P2-1 bis P2-4)
 *
 * Abgeleitet aus einem modell::Plan: je Operator ein Knoten, in Datenrichtung (die Scans oben, die Wurzel unten),
 * mit dem, was Spark ueber ihn sagt — beim Scan Tabelle, Spalten, Filter; beim Exchange Partitionierung und Zahl;
 * beim Join Art, Schluessel, Seite des Broadcasts; beim Aggregat Schluessel und Funktionen (P2-2). Rahmen: je
 * Unterabfrage, darin je Stage (das Gebiet zwischen Exchanges), darin je WholeStageCodegen-Stufe. Ein Exchange
 * beendet die Stage, die in ihn schreibt. Eine Unterabfrage fuehrt gestrichelt in den Operator, der sie nutzt; ein
 * wiederverwendeter Exchange gepunktet von dem, den er wiederverwendet. Die logischen Plaene (`extended`) zeigen
 * nur den Baum. Das Wissen ueber Spark ist eine Tabelle in Plan.cpp; der Leser kennt es nicht.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"  // kKeine
#include "c2d/modell/Code.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::diagramm
{

enum class PlanKnotenArt
{
    Operator,         ///< jeder andere
    Scan,             ///< liest eine Quelle: Scan, FileScan, BatchScan, LocalTableScan, InMemoryTableScan …
    Exchange,         ///< ein Shuffle: Exchange mit Partitionierung
    Broadcast,        ///< BroadcastExchange
    Join,             ///< …Join, CartesianProduct
    Aggregat,         ///< HashAggregate, SortAggregate, ObjectHashAggregate
    Wiederverwendet,  ///< ReusedExchange, ReusedSubquery
    Unterabfrage,     ///< Subquery, SubqueryBroadcast
    Wurzel,           ///< AdaptiveSparkPlan: das Ergebnis
};

struct PlanKnoten
{
    PlanKnotenArt art = PlanKnotenArt::Operator;
    std::string titel;                ///< der Name, mit `(n)` aus formatted
    std::vector<std::string> zeilen;  ///< ungekuerzt, ohne die Kennungen der Attribute (`key#12` -> `key`)
    std::size_t gruppe = kKeine;      ///< der innerste Rahmen
    int zeile = 0;                    ///< im Text des Plans (Plandiagramm::aufruf)
};

/// «shuffle», «broadcast», «reused», «subquery»; sonst leer.
[[nodiscard]] std::string_view stereotyp(PlanKnotenArt art);

/// Die Zeilen, die ein Format zeigt: Stereotyp (kursiv), Titel (fett), dann die Zeilen.
[[nodiscard]] std::vector<std::string> texte(const PlanKnoten& knoten);

enum class PlanKantenArt
{
    Fluss,             ///< die Daten, vom Kind zum Eltern: durchgezogen
    Unterabfrage,      ///< das Ergebnis einer Unterabfrage in den Operator, der sie nutzt: gestrichelt
    Wiederverwendung,  ///< vom Exchange (der Unterabfrage) zu dem, der ihn wiederverwendet: gepunktet, `reuses`
};

struct PlanKante
{
    std::size_t von = 0;
    std::size_t nach = 0;
    PlanKantenArt art = PlanKantenArt::Fluss;
    std::string text;  ///< `build` an der Seite des Broadcasts; der Ausdruck einer Unterabfrage
};

enum class PlanRahmenArt
{
    Unterabfrage,
    Stage,
    Codegen,  ///< WholeStageCodegen
};

struct PlanRahmen
{
    PlanRahmenArt art = PlanRahmenArt::Stage;
    std::string titel;  ///< `Subquery 1`, `Stage 2`, `WholeStageCodegen (3)`
    std::size_t eltern = kKeine;
};

struct Plandiagramm
{
    std::string titel;
    std::vector<PlanKnoten> knoten;
    std::vector<PlanKante> kanten;
    std::vector<PlanRahmen> rahmen;
    std::vector<std::string> hinweise;
    int stages = 0;
    /// Wie modell::Plan::aufruf: steht der Plan in einer Ausgabe, die Zeile des `explain()` in der Datei;
    /// 0 = die Zeilen der Knoten sind Zeilen der Datei.
    int aufruf = 0;
};

/// Der Abschnitt `Physical Plan`; kKeine = der Plan hat keinen.
[[nodiscard]] std::size_t physischerBaum(const modell::Plan& plan);

/// Wie ein Abschnitt in einer Auswahl heisst: `physical`, `optimized`, `analyzed`, `parsed`.
[[nodiscard]] std::string kurzname(const modell::Planbaum& baum);

/// Der Plan als Diagramm: der Abschnitt `baum` (Stelle in Plan::baeume); kKeine = der physische, sonst der letzte.
[[nodiscard]] Plandiagramm planVon(const modell::Plan& plan, std::size_t baum = kKeine);

}  // namespace c2d::diagramm
