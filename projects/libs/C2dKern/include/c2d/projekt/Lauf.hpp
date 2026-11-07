/**
 * @file Lauf.hpp
 * @brief Ein Lauf (Event-Log) und der Code, der ihn gestartet hat (Spark Schicht 3, Entscheide L5, L5-1 bis L5-3)
 *
 * Der Log nennt eine Python-Stelle nur bei manchen Aufrufen (`collect at C:\…\x.py:7`, die Stages von RDD-Schritten);
 * `count`, `show`, `write` tragen `… at NativeMethodAccessorImpl.java:0`. Im Notebook heisst die Datei
 * `…\ipykernel_<pid>\<hash>.py`, der Hash des Zelltexts (Zelle::kernHash). Daraus:
 *
 * - **Wohin** (L5-1, L5-2): ein Pfad im Projekt meint diese Datei; sonst der Dateiname, wenn er eindeutig ist, sonst
 *   alle mit diesem Namen (ein Menue). Ein Hash meint die Zelle mit diesem Hash; passt keine, die Zellen, in denen an
 *   dieser Zeile eine Aktion dieses Namens steht (`cell changed since the run`).
 * - **Was** (L5-3): eine Aktion des Laufs ist eine SQL-Ausfuehrung mit ihren Jobs oder ein Job ohne Ausfuehrung mit
 *   Stelle (eine RDD-Aktion). Ein Job ohne beides (das Lesen eines Schemas) ist keine Aktion. Eine Ausfuehrung ohne
 *   Stelle gehoert zu einer Aktion des Datenflusses, wenn sie zwischen zwei Ausfuehrungen mit Stelle in derselben Datei
 *   die einzige ihres Namens ist und im Text dazwischen genau eine Aktion dieses Namens steht.
 */
#pragma once

#include "c2d/projekt/Projekt.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::projekt
{

/// Eine Stelle, wie der Log sie schreibt: `collect at C:\…\x.py:7`.
struct Aufrufstelle
{
    std::string methode;  ///< `collect`, `showString`, `reduceByKey`
    std::string pfad;     ///< wie im Log; leer = keine Python-Stelle (`NativeMethodAccessorImpl.java:0`)
    int zeile = 0;
};

/// Die Stelle aus dem Text des Logs; nullopt = kein `<methode> at <ort>:<zeile>`.
[[nodiscard]] std::optional<Aufrufstelle> aufrufstelleAus(std::string_view text);

/// Eine Zeile einer Datei des Projekts (Datei::text).
struct Codestelle
{
    std::size_t datei = kKeine;
    int zeile = 0;
    friend bool operator==(const Codestelle&, const Codestelle&) = default;
};

/// Eine Aktion des Laufs und wo sie im Code steht.
struct LaufAktion
{
    int ausfuehrung = -1;    ///< die SQL-Ausfuehrung; -1 = ein Job ohne (RDD)
    std::vector<int> jobs;   ///< ihre Jobs
    std::string methode;     ///< wie der Log sie nennt (`collect`, `showString`, `count`)
    /// Wo sie steht: eine = verbunden, mehrere = ein Menue, leer = nicht verbunden.
    std::vector<Codestelle> stellen;
    bool nachName = false;   ///< ohne Stelle im Log, ueber ihren Namen zwischen zwei Ankern (L5-3)
};

/// Eine Stage mit Python-Stelle (ein RDD-Schritt, eine RDD-Aktion).
struct LaufStageStelle
{
    int stage = 0;
    std::vector<Codestelle> stellen;
};

struct Laufverbindung
{
    std::vector<LaufAktion> aktionen;      ///< in der Folge ihres Starts
    std::vector<LaufStageStelle> stages;
    std::vector<std::string> hinweise;
};

/// Der Lauf der Datei `log` (ein Event-Log) und der Code des Projekts, zu dem er gehoert.
[[nodiscard]] Laufverbindung verbindeLauf(const Projekt& projekt, std::size_t log);

/// Wo im Code eine Job- oder Stage-Zeile der Zeitachse steht: die Stelle der Stage, sonst die ihrer Aktion.
[[nodiscard]] std::vector<Codestelle> codestellenVon(const Laufverbindung& v, int job, int stage);

/// Umgekehrt: was in einem Lauf des Projekts zu einer Zeile der Datei gehoert (L5: «action» → Job, die Zeile einer
/// RDD-Aufrufstelle → Stage).
struct LaufTreffer
{
    std::size_t log = kKeine;
    std::vector<int> jobs;
    int stage = -1;  ///< -1 = eine Aktion (dann jobs)
};
[[nodiscard]] std::vector<LaufTreffer> laeufeAn(const Projekt& projekt, std::size_t datei, int zeile);

}  // namespace c2d::projekt
