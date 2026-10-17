/**
 * @file Projektdatei.hpp
 * @brief Die Projektparameter: `code2diagram.json` im Wurzelordner eines Projekts
 *
 * Was beim Kompilieren als Argument mitgeht, steht hier fuer das ganze Projekt:
 * Definitionen (wie `-D`; `!NAME` hebt auf wie `-U`, in einer Liste gilt der
 * spaetere Eintrag) und Include-Pfade (wie `-I`), dazu je Ordner eigene
 * (`abschnitte`) und die Ordner, die beim Einlesen uebergangen werden. Fehlt die
 * Datei, gelten die Vorgaben (alles leer). Ein Beispiel:
 *
 *     {
 *       "definitionen": ["CPU1", "DEBUG=1", "!SIMULATION"],
 *       "includePfade": ["firmware/Global/Header_Files"],
 *       "uebergehen": ["old", "firmware/bak*"],
 *       "abschnitte": [
 *         { "ordner": "firmware/Slave", "definitionen": ["SLAVE"], "includePfade": ["firmware/Slave/inc"] }
 *       ]
 *     }
 *
 * Ordnermuster (`uebergehen`, `abschnitte[].ordner`) gelten fuer Wege relativ
 * zum Wurzelordner, mit `/`: `*` steht fuer beliebig viele Zeichen in einem
 * Ordnernamen, `?` fuer eines, `**` fuer beliebig viele Ordner. Ein Muster ohne
 * `/` trifft einen Ordner dieses Namens in jeder Tiefe (wie in `.gitignore`).
 * Gross- und Kleinschreibung zaehlen.
 */
#pragma once

#include "c2d/quelle/Meldung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::projekt
{

/// So heisst die Projektdatei im Wurzelordner.
inline constexpr std::string_view kProjektdatei = "code2diagram.json";

/// Eigene Parameter fuer die Dateien in einem Ordner und darunter. Sie kommen
/// zu denen des Projekts dazu; eine Definition gleichen Namens ersetzt die des Projekts.
struct Abschnitt
{
    std::string ordner;  ///< ein Ordnermuster
    std::vector<sprache::Definition> definitionen;
    std::vector<std::string> includePfade;  ///< relativ zum Wurzelordner (oder absolut)
};

struct Projektdatei
{
    std::vector<sprache::Definition> definitionen;
    std::vector<std::string> includePfade;  ///< relativ zum Wurzelordner (oder absolut)
    std::vector<std::string> uebergehen;    ///< Ordnermuster
    std::vector<Abschnitt> abschnitte;
};

/// Liest den Text einer Projektdatei. Was nicht passt — kein gueltiges JSON, ein
/// unbekannter Schluessel, ein Wert der falschen Art, eine leere Definition —
/// wird eine Meldung mit Zeile und Spalte; was passt, gilt.
[[nodiscard]] Projektdatei liesProjektdatei(std::string_view text, std::vector<quelle::Meldung>& meldungen);

/// Die Projektdatei als Text, eingerueckt; `liesProjektdatei` liest ihn wieder genau so.
[[nodiscard]] std::string schreibe(const Projektdatei& datei);

/// Die Definitionen von aussen (das Feld Defines) in die Projektdatei uebernommen: je
/// Name ersetzt eine den gleichnamigen Eintrag in `definitionen` oder kommt dazu —
/// auch eine Aufhebung `!NAME`. Nichts wird entfernt. Legt ein Abschnitt denselben
/// Namen anders fest, gilt dort weiter der Abschnitt: dafuer je ein Hinweis.
[[nodiscard]] Projektdatei mitDefinitionen(Projektdatei datei, const std::vector<sprache::Definition>& aussen,
                                           std::vector<std::string>& hinweise);

/// Passt das Ordnermuster auf diesen Ordner (Weg relativ zum Wurzelordner, mit `/`)?
[[nodiscard]] bool passt(std::string_view muster, std::string_view ordner);

/// Liegt der Ordner in einem, auf den das Muster passt — oder ist er es selbst?
[[nodiscard]] bool liegtIn(std::string_view muster, std::string_view ordner);

/// Ordnernamen, die immer uebergangen werden — ausser ihnen jeder, der mit `.` beginnt.
[[nodiscard]] std::span<const std::string_view> immerUebergangen();

/// Wird ein Ordner dieses Namens immer uebergangen?
[[nodiscard]] bool wirdImmerUebergangen(std::string_view name);

/// Erzeugt Unity einen Ordner dieses Namens (`Library`, `Temp`, `Logs`, `UserSettings`)? Er wird
/// uebergangen, wo daneben `ProjectSettings/ProjectVersion.txt` liegt (ein Unity-Projekt).
[[nodiscard]] bool erzeugtVonUnity(std::string_view name);

/// Was eine `.gitignore` zum Einrichten von `uebergehen` beitraegt.
struct Vorschlag
{
    std::vector<std::string> uebergehen;        ///< Ordnermuster
    std::vector<std::string> nichtUebernommen;  ///< Zeilen, die kein Ordner sind oder sich nicht uebertragen lassen
};

/// Die Eintraege einer `.gitignore`, die Ordner meinen, als Muster fuer
/// `uebergehen` — ohne die, die ohnehin immer uebergangen werden. Dateimuster
/// (`*.o`), Verneinungen (`!x`) und Kommentare werden nicht uebernommen.
[[nodiscard]] Vorschlag ausGitignore(std::string_view text);

}  // namespace c2d::projekt
