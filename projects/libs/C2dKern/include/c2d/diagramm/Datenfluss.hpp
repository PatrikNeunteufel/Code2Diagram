/**
 * @file Datenfluss.hpp
 * @brief Der Datenfluss eines Spark-Skripts (Spark Schicht 1, Entscheide F1-F6)
 *
 * Abgeleitet aus den Aufrufketten (modell::Anweisung::ketten): Quellen (`spark.read…`, `spark.table`,
 * `spark.sql`, `sc.parallelize` …), schmale Schritte zusammen in einem Knoten, breite Schritte (Shuffle) und
 * Aktionen (je eine ein Job) als eigene Knoten; Rahmen je Stage (das Gebiet zwischen Shuffles). Verfolgt wird
 * von einer Sitzung aus (F2), Views und gleiche Pfade verbinden Jobs (F4), die Verteilung steht im Kopf
 * Session und an den Shuffles (F5). Das Wissen ueber Spark ist eine Tabelle in Datenfluss.cpp — kein Leser
 * kennt Spark, die Ableitung kennt keine Sprache.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"  // kKeine
#include "c2d/modell/Code.hpp"

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::diagramm
{

enum class DatenflussKnotenArt
{
    Sitzung,   ///< der Kopf Session: appName, master, config (F5)
    Quelle,    ///< read, table, parallelize, textFile, createDataFrame, range
    Schritte,  ///< schmale Schritte, je einer eine Zeile
    Shuffle,   ///< ein breiter Schritt: groupBy, join, distinct, orderBy, repartition, reduceByKey …
    Aktion,    ///< count, collect, show, write …: ein Job
    Sql,       ///< spark.sql("…")
    Aufruf,    ///< ein DataFrame geht an eine Funktion des Projekts
};

struct DatenflussKnoten
{
    DatenflussKnotenArt art = DatenflussKnotenArt::Schritte;
    std::string titel;                ///< `read parquet`, `groupBy("k")`, `Job 1: count()`; leer bei Schritte
    std::vector<std::string> zeilen;  ///< je Schritt, Pfad, Einstellung eine Zeile (ungekuerzt)
    std::size_t gruppe = kKeine;      ///< der innerste Rahmen (eine Stage, eine Funktion)
    int zeile = 0;                    ///< im Quelltext
    bool zwischengespeichert = false; ///< cache, persist, checkpoint
    std::vector<std::string> ziele;   ///< Funktionen des Projekts, die er nennt (UDF, `map(f)`, `f(df)`)
    std::string abfrage;              ///< nur Sql: der Name der modell::Abfrage (Doppelklick)
    std::string tabelle;              ///< nur Quelle `table`: der Name der Tabelle (Doppelklick)
    std::string plan;                 ///< der Plan aus seinem `explain()` (modell::Plan::name, Doppelklick); leer = keiner
    /// Aktion und Shuffle: der Name der Methode, wie ihn ein Event-Log nennt (`collect`, `parquet`, `reduceByKey`; L5).
    std::string methode;
};

/// «session», «source», «shuffle», «action», «sql», «call»; leer bei Schritte.
[[nodiscard]] std::string_view stereotyp(DatenflussKnotenArt art);

/// Die Zeilen, die ein Format zeigt: Stereotyp (kursiv), Titel (fett), dann die Zeilen.
[[nodiscard]] std::vector<std::string> texte(const DatenflussKnoten& knoten);

enum class DatenflussKantenArt
{
    Fluss,  ///< die Daten: durchgezogen mit Spitze
    Pfad,   ///< zwischen Jobs ueber einen Pfad, eine Tabelle, eine View: gestrichelt (`same path`, `view x`)
};

struct DatenflussKante
{
    std::size_t von = 0;
    std::size_t nach = 0;
    DatenflussKantenArt art = DatenflussKantenArt::Fluss;
    std::string text;
};

struct DatenflussRahmen
{
    std::string titel;            ///< `Stage 2`, der Name einer Funktion
    std::size_t eltern = kKeine;
};

struct DatenflussHinweis
{
    int zeile = 0;
    std::string text;
};

struct Datenflussdiagramm
{
    std::string titel;
    std::vector<DatenflussKnoten> knoten;
    std::vector<DatenflussKante> kanten;
    std::vector<DatenflussRahmen> rahmen;
    std::vector<DatenflussHinweis> hinweise;
    int jobs = 0;
    int stages = 0;
};

/// Was ein Wert einer Funktion des Projekts ist, die eine Kette ruft (`spark = create_spark_session()`).
enum class SparkWert
{
    Keiner,
    Sitzung,  ///< SparkSession
    Kontext,  ///< SparkContext
};

/// Was eine Funktion des Projekts liefert, samt den Einstellungen ihrer Sitzung (appName, master, config) —
/// sie gehoeren in den Kopf Session dessen, der sie ruft.
struct SparkRueckgabe
{
    SparkWert wert = SparkWert::Keiner;
    std::vector<std::pair<std::string, std::string>> einstellungen;
};

/// Was die Ableitung vom Projekt braucht; leere Funktionen = nichts bekannt.
struct DatenflussUmfeld
{
    /// In Notebooks sind `spark` und `sc` vorgegeben (F2).
    bool vorgegeben = false;
    /// Heisst so eine Funktion des Projekts (fuer UDF, `map(f)`, `f(df)`)?
    std::function<bool(std::string_view name)> istFunktion;
    /// Was liefert die Funktion dieses Namens (ihr return)?
    std::function<SparkRueckgabe(std::string_view name)> liefert;
    /// Die Abfrage, die das Projekt aus SQL in den Zeilen [von, bis] gelesen hat (`spark.sql("…")`); nullptr = keine.
    std::function<const modell::Abfrage*(int von, int bis)> abfrageIn;
    /// Der Plan aus der Ausgabe des `explain()` in dieser Zeile (modell::Plan::name, Spark Schicht 2); leer = keiner.
    std::function<std::string(int zeile)> planAn;
};

/// Ein Stueck Code, dessen Fluss gezeigt wird: eine Funktion (auch `<module>`, `<cell n>`).
struct DatenflussBereich
{
    const modell::Funktion* funktion = nullptr;
};

/// Der Datenfluss der Bereiche. `gemeinsam`: sie teilen ihre Namen (die Zellen eines Notebooks), sonst ist
/// jeder fuer sich und steht in einem Rahmen mit seinem Namen. `fokus` (kKeine = alle): nur die Knoten dieses
/// Bereichs und die, aus denen sie direkt lesen (eine Zelle: was fruehere Zellen liefern).
[[nodiscard]] Datenflussdiagramm datenflussVon(std::span<const DatenflussBereich> bereiche, bool gemeinsam,
                                               const DatenflussUmfeld& umfeld, std::string titel,
                                               std::size_t fokus = kKeine);

/// Welche Bereiche einen Datenfluss zeigen (eine Quelle, einen Schritt, eine Aktion) — in einem Lauf ueber alle.
[[nodiscard]] std::vector<bool> bereicheMitFluss(std::span<const DatenflussBereich> bereiche, bool gemeinsam,
                                                 const DatenflussUmfeld& umfeld);

/// Was der return der Funktion liefert (eine Sitzung, einen Kontext) und mit welchen Einstellungen — fuer
/// DatenflussUmfeld::liefert.
[[nodiscard]] SparkRueckgabe rueckgabeVon(const modell::Funktion& funktion, const DatenflussUmfeld& umfeld);

}  // namespace c2d::diagramm
