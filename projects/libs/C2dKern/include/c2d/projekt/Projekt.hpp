/**
 * @file Projekt.hpp
 * @brief Ein Projekt: ein Ordner samt Unterordnern, gelesen als viele Einheiten
 *
 * Eingelesen wird jede Datei, fuer die eine Sprache eine Endung kennt
 * (sprache/Sprache.hpp). Uebergangen werden die Ordner, die immer uebergangen
 * werden (projekt/Projektdatei.hpp), und die aus `uebergehen` der Projektdatei —
 * je Ordner mit einer Meldung.
 *
 * Gelesen wird in zwei Durchgaengen. Zuerst jede Datei mit ihren Definitionen
 * (Projekt, passende Abschnitte, von aussen). Dann bekommt jede Datei die
 * `#define` der Koepfe, die sie einbindet (auch mittelbar), und wird damit noch
 * einmal gelesen — so wertet ein `#if` aus, was ein Kopf definiert. Ein Kopf
 * wird gesucht: `"x.h"` zuerst neben der Datei, dann in den Include-Pfaden
 * (Projekt, dann passende Abschnitte); `<x.h>` nur in den Include-Pfaden.
 * Findet sich `"x.h"` dort nicht, gilt eine Datei dieses Namens im Projekt,
 * wenn es genau eine gibt; sonst eine Meldung. Vereinfacht: die `#define` eines
 * Kopfs gelten ab dem Anfang der Datei, nicht erst ab der Zeile des `#include`,
 * und ein Kopf wird mit seinen eigenen Definitionen gelesen, nicht mit denen,
 * die vor dem `#include` galten. Was nicht unter dem Wurzelordner liegt, aber
 * ueber einen Include-Pfad gefunden wird, traegt nur seine `#define` bei.
 *
 * Reihenfolge der Definitionen einer Datei, das Spaetere ersetzt Gleichnamiges:
 * Projekt, Abschnitte (in ihrer Reihenfolge), Koepfe, von aussen (`-D`, das
 * Feld Defines). Das gilt auch fuer Aufhebungen (`!NAME`): eine in der
 * Projektdatei hebt ein spaeterer Abschnitt oder ein Kopf wieder auf, eine von
 * aussen auch das `#define` eines Kopfs. Was die Datei selbst definiert, gilt
 * immer.
 */
#pragma once

#include "c2d/projekt/Projektdatei.hpp"
#include "c2d/quelle/Meldung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::projekt
{

inline constexpr std::size_t kKeine = static_cast<std::size_t>(-1);

struct ProjektOptionen
{
    bool alleZweige = false;
    bool varianten = false;  ///< wie sprache::LeseOptionen::varianten
    bool bereiche = false;   ///< wie sprache::LeseOptionen::bereiche
    /// Von aussen (`-D`, das Feld Defines); gehen allem anderen vor.
    std::vector<sprache::Definition> definitionen;
};

/// Eine Zelle eines Notebooks (Entscheide N1–N3), gezaehlt ab 1 wie Databricks „Cmd n“, auch Markdown.
struct Zelle
{
    enum class Art
    {
        Code,
        Markdown,
        Roh,  ///< `.ipynb` "raw": Text, der nicht ausgefuehrt wird
    };

    Art art = Art::Code;
    /// Die Zeile ihrer Kopfzeile (`# %% [n]`) bzw. des Trenners `COMMAND` davor; 0 = keine (die erste
    /// Zelle eines Databricks-Notebooks).
    int kopf = 0;
    int zeile = 0;     ///< ihre erste Zeile (ohne Kopf und `DBTITLE`)
    int bisZeile = 0;  ///< ihre letzte; kleiner als `zeile` = leer
    /// Wie sie gelesen wird: leer = in der Sprache der Datei; sonst der Name einer Sprache (`SQL`, Entscheid
    /// N3) oder — nicht gelesen — das Wort ihrer Magie (`%scala`, `%%bash`).
    std::string sprache;
    bool gelesen = true;  ///< false: in einer Sprache, die das Projekt nicht kennt (Hinweis)
    std::string titel;    ///< Databricks `DBTITLE`; leer = keiner
};

struct Datei
{
    std::string pfad;  ///< relativ zum Wurzelordner, mit `/`
    const sprache::Sprache* sprache = nullptr;
    /// Der Text, wie der Leser und das Fenster ihn sehen. Bei einem `.ipynb` nicht die Datei, sondern ihr
    /// Skript (Entscheid N1): je Zelle eine Kopfzeile `# %% [n]`, Markdown als Kommentar.
    std::string text;
    /// Die Zellen, wenn die Datei ein Notebook ist (`.ipynb`, Databricks); leer = keins.
    std::vector<Zelle> zellen;
    sprache::Ergebnis ergebnis;
    /// Mit diesen Definitionen zuletzt gelesen, ohne die von aussen.
    std::vector<sprache::Definition> definitionen;
    /// Die Dateien des Projekts, die sie einbindet, auch mittelbar (Stellen in Projekt::dateien),
    /// jede einmal, in der Reihenfolge des Suchens.
    std::vector<std::size_t> koepfe;
};

/// Eine Meldung ueber das Projekt, einen Ordner oder eine Datei als Ganzes
/// (Zeile 0), oder ueber eine Zeile einer Datei (ein `#include`, der nichts findet).
struct Projektmeldung
{
    std::string pfad;  ///< relativ zum Wurzelordner; leer = das Projekt selbst
    quelle::Meldung meldung;
};

struct Projekt
{
    std::filesystem::path wurzel;  ///< leer: eine einzelne Datei, ohne Ordner
    Projektdatei einstellungen;
    bool mitProjektdatei = false;  ///< gab es `code2diagram.json`?
    std::vector<Datei> dateien;    ///< nach Pfad geordnet
    std::vector<std::string> uebergangen;  ///< Ordner, relativ, mit `/`
    std::vector<Projektmeldung> meldungen;

    /// Die Datei mit diesem Pfad; kKeine = keine.
    [[nodiscard]] std::size_t finde(std::string_view pfad) const;
    /// Funktionen mit Rumpf ueber alle Dateien.
    [[nodiscard]] std::size_t funktionen() const;
    /// Datentypen ueber alle Dateien.
    [[nodiscard]] std::size_t typen() const;
};

/// Liest alle Dateien unter `wurzel`, mit `code2diagram.json`, wenn es sie gibt.
/// Was nicht geht, wird eine Meldung; das Ergebnis ist immer brauchbar.
[[nodiscard]] Projekt liesOrdner(const std::filesystem::path& wurzel, const ProjektOptionen& optionen = {});

/// Ist die Datei ein Jupyter-Notebook (`.ipynb`)? Es wird gelesen, wenn es seine Sprache gibt.
[[nodiscard]] bool istNotebook(std::string_view dateiname);

/// Liest das Projekt diese Datei? Eine Sprache kennt ihre Endung, oder sie ist ein Notebook.
[[nodiscard]] bool kannLesen(std::string_view dateiname);

/// Die Zelle, in der die Zeile von Datei::text liegt (Index in Datei::zellen); kKeine = kein Notebook.
[[nodiscard]] std::size_t zelleAn(const Datei& datei, int zeile);

/// Wie eine Zeile von Datei::text heisst: in einem Notebook `cell 3, line 2` (die Kopfzeile: `cell 3`),
/// sonst `line 17`.
[[nodiscard]] std::string stelleIn(const Datei& datei, int zeile);

/// Liest eine Datei ganz, wie sie ist (Bytes); unter Windows auch ueber einen Weg ab 260 Zeichen.
/// false = sie liess sich nicht oeffnen.
[[nodiscard]] bool liesDatei(const std::filesystem::path& weg, std::string& text);

/// Ein Projekt aus einem einzigen Text, ohne Ordner und ohne Projektdatei.
/// `name` entscheidet ueber die Sprache (wie sprache::spracheFuer); fehlt eine, wird C gelesen.
[[nodiscard]] Projekt liesText(std::string_view text, std::string_view name, const ProjektOptionen& optionen = {});

/// Die Definitionen fuer eine Datei an diesem Pfad aus Projekt und Abschnitten (ohne Koepfe, ohne von aussen).
[[nodiscard]] std::vector<sprache::Definition> definitionenFuer(const Projektdatei& einstellungen, std::string_view pfad);

/// Wer fuer eine Datei zuletzt festlegt, ob ein Name definiert ist — ohne die von aussen.
enum class Herkunft
{
    keine,      ///< niemand: es zaehlt nur, was die Datei selbst definiert
    projekt,    ///< `definitionen` der Projektdatei
    abschnitt,  ///< ein Abschnitt der Projektdatei
    kopf,       ///< ein Kopf, den die Datei einbindet
};

/// Woher der Zustand von `name` fuer diese Datei kommt (nach Datei::definitionen).
[[nodiscard]] Herkunft herkunft(const Projektdatei& einstellungen, const Datei& datei, std::string_view name);

/// Eine Funktion in einer Datei des Projekts.
struct Fundstelle
{
    std::size_t datei = kKeine;
    std::size_t funktion = kKeine;  ///< Index in `ergebnis.einheit.funktionen`

    bool operator==(const Fundstelle&) const = default;
};

/// Welche Funktionen mit Rumpf ein Aufruf aus `ausDatei` meinen kann (modell::Funktion::heisst):
/// die der Datei selbst, wenn es dort welche gibt; sonst die nicht `intern`en
/// der anderen Dateien. Mehr als eine: es ist nicht entschieden, welche.
[[nodiscard]] std::vector<Fundstelle> kandidaten(const Projekt& projekt, std::size_t ausDatei, std::string_view aufruf);

/// Wer die Funktion an `ziel` ruft: jede Funktion mit Rumpf im Projekt, von der
/// ein Aufruf sie meinen kann (nach `kandidaten`, auch wenn er mehrdeutig ist),
/// jede einmal, in der Reihenfolge der Dateien.
[[nodiscard]] std::vector<Fundstelle> aufrufer(const Projekt& projekt, const Fundstelle& ziel);

/// Die Funktionen mit Rumpf, deren Name oder voller Name `name` ist, ueber alle Dateien.
[[nodiscard]] std::vector<Fundstelle> funktionenNamens(const Projekt& projekt, std::string_view name);

/// Die Funktion an einer Fundstelle.
[[nodiscard]] const modell::Funktion& funktion(const Projekt& projekt, const Fundstelle& stelle);

}  // namespace c2d::projekt
