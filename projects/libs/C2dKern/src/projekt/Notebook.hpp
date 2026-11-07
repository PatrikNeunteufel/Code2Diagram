/**
 * @file Notebook.hpp
 * @brief Ein Notebook in Texte zerlegen, die die Leser lesen (Entscheide P2, N1–N3)
 *
 * Das Herausloesen gehoert ins Projekt, nicht in den Leser (P2): ein Leser bekommt einen gewoehnlichen
 * Text seiner Sprache, in dem jede Zeile auf der Zeile des Skripts liegt, das das Fenster zeigt.
 *
 * - Ein `.ipynb` (JSON) wird ein Skript in der Form von jupytext: je Zelle eine Kopfzeile `# %% [n]`
 *   (mit dem Zeilenkommentar der Sprache), Markdown und Roh-Zellen als Kommentar.
 * - Ein Databricks-Notebook (erste Zeile `-- Databricks notebook source`, Zellen zwischen
 *   `-- COMMAND ----------`) bleibt, wie es ist.
 *
 * Was die Sprache des Notebooks liest (`lesetext`): das Skript, aber Zeilen mit IPython-Magie (`%pip`,
 * `!ls`) und Zellen in anderer Sprache leer — je mit einem Hinweis. Eine Zelle in einer anderen Sprache,
 * die das Projekt kennt (`%%sql`, Databricks `%python`), wird ein eigener Text (`fremde`) mit denselben
 * Zeilen, in dem nur sie steht; bei Databricks ohne das Praefix `MAGIC` (`abzug` je Zeile).
 *
 * Intern (nicht unter include/).
 */
#pragma once

#include "c2d/projekt/Projekt.hpp"
#include "c2d/quelle/Meldung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::projekt
{

/// Eine Zelle in einer anderen Sprache als das Notebook, als eigener Text.
struct FremdeZelle
{
    const sprache::Sprache* sprache = nullptr;
    std::size_t zelle = 0;  ///< Index in Notebook::zellen
    /// So viele Zeilen wie das Skript; nur die der Zelle tragen etwas.
    std::string text;
    /// Je Zeile (Index = Zeile - 1): so viele Bytes stehen im Skript vor dem, was `text` dort traegt.
    std::vector<std::size_t> abzug;
};

/// Was eine Code-Zelle eines `.ipynb` ausgegeben hat (Text), aneinander — fuer die Plaene von `explain()` (P2-3).
struct ZellAusgabe
{
    std::size_t zelle = 0;  ///< Index in Notebook::zellen
    std::string text;
};

struct Notebook
{
    std::string skript;  ///< Datei::text
    const sprache::Sprache* sprache = nullptr;
    bool databricks = false;  ///< ein Databricks-Notebook: sein SQL ist Spark (Kopf, Entscheid E2)
    std::vector<Zelle> zellen;
    /// Was die Sprache des Notebooks liest: Zeile fuer Zeile so lang wie `skript` (Magie und fremde Zellen
    /// als Leerraum), so gelten Versaetze in beiden.
    std::string lesetext;
    std::vector<sprache::Abschnitt> abschnitte;  ///< je Code-Zelle in ihrer Sprache `<cell n>`
    std::vector<FremdeZelle> fremde;
    std::vector<ZellAusgabe> ausgaben;       ///< je Code-Zelle mit Text in ihren Ausgaben
    std::vector<quelle::Meldung> meldungen;  ///< Stellen im Skript
};

/// So heisst die Funktion einer Zelle: `<cell 3>` (Entscheid N2).
[[nodiscard]] std::string zellenName(std::size_t nummer);

/// Ein `.ipynb`. nullopt = es laesst sich nicht lesen; dann sagt `fehler`, warum (Stelle im JSON).
[[nodiscard]] std::optional<Notebook> ausIpynb(std::string_view json, quelle::Meldung& fehler);

/// Ein Databricks-Notebook in der Sprache `sprache`; nullopt = der Text ist keins.
[[nodiscard]] std::optional<Notebook> ausDatabricks(std::string_view text, const sprache::Sprache& sprache);

}  // namespace c2d::projekt
