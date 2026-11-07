/**
 * @file Zeitachse.hpp
 * @brief Ein Lauf von Spark auf einer Zeitachse (Spark Schicht 3, Entscheide L2-L4, L7)
 *
 * Abgeleitet aus einem modell::Lauf (dem Event-Log): oben je Job eine Zeile mit seinem Balken und den Balken seiner
 * Stages (ueberlappen sie sich, so viele Zeilen wie noetig), darunter je Executor so viele Bahnen wie Plaetze
 * (`Total Cores` / `spark.task.cpus`) mit je Task einem Balken in der Farbe seiner Stage (L2, L3). Was kein Ende hat,
 * reicht offen bis zum letzten Zeitstempel des Logs; ein Fehlschlag traegt ✗ und seinen Grund (L4). Ueber
 * kHoechstensTasks Tasks je Bahn und Stage ein Balken mit der Zahl.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"  // kKeine
#include "c2d/modell/Code.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

/// Bis zu so vielen Tasks ist jeder ein Balken; darueber je Bahn und Stage einer (Entscheid L3).
inline constexpr std::size_t kHoechstensTasks = 20000;

/// Laufen auf einem Executor laenger als so viele Millisekunden mehr Tasks als Plaetze, gibt es einen Hinweis (L2).
inline constexpr long long kUeberlappungMs = 1000;

enum class ZeitBahnArt
{
    Job,       ///< eine Zeile eines Jobs (die erste traegt seinen Balken)
    Executor,  ///< ein Platz eines Executors
};

struct ZeitBahn
{
    ZeitBahnArt art = ZeitBahnArt::Executor;
    std::string titel;     ///< `Job 3`, `executor 6 (172.31.47.174)`, ein weiterer Platz `  core 2`; leer = Fortsetzung
    std::string executor;  ///< bei Executor
    int platz = 0;         ///< bei Executor: 0, 1, …
    long long entfernt = 0;  ///< der Executor wurde entfernt (L4); 0 = nicht
    std::string grund;
};

enum class ZeitBalkenArt
{
    Job,
    Stage,
    Task,
    Zusammen,  ///< mehrere Tasks einer Stage auf einer Bahn (ueber kHoechstensTasks)
};

struct ZeitBalken
{
    ZeitBalkenArt art = ZeitBalkenArt::Task;
    std::size_t bahn = 0;
    long long von = 0;
    long long bis = 0;
    bool offen = false;           ///< kein Ende im Log: bis zum letzten Zeitstempel (L4)
    bool fehlgeschlagen = false;  ///< ✗
    std::size_t farbe = kKeine;   ///< die Farbe der Stage (rahmenFarbe); kKeine = keine (ein Job)
    std::string titel;            ///< was im Balken steht, wenn er Platz hat: `Stage 3: collect at kmeans.scala:62`
    std::vector<std::string> angaben;  ///< alles, was der Log dazu sagt (Tooltip)
    /// Nur Task: die Werte als Zahlen und Woerter (csv, Entscheid L7) — Zeiten in ms ab dem Anfang, Bytes als Zahl.
    std::vector<std::pair<std::string, std::string>> spalten;
    int zeile = 0;                ///< im Log
};

struct Zeitachse
{
    std::string titel;
    long long anfang = 0;
    long long ende = 0;
    std::vector<ZeitBahn> bahnen;
    std::vector<ZeitBalken> balken;
    std::vector<std::string> hinweise;
    std::size_t jobs = 0;
    std::size_t stages = 0;
    std::size_t tasks = 0;
    bool zusammengefasst = false;  ///< mehr als kHoechstensTasks
};

/// Die Zeitachse eines Laufs.
[[nodiscard]] Zeitachse zeitachseVon(const modell::Lauf& lauf);

/// Bytes lesbar: `117.6 KB`, `3.2 GB`.
[[nodiscard]] std::string bytes(long long anzahl);

}  // namespace c2d::diagramm
