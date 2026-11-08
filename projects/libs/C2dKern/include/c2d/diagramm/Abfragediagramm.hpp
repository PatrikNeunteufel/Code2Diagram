/**
 * @file Abfragediagramm.hpp
 * @brief Das Diagramm einer Abfrage (Entscheide Q1-Q5): Quellen als Kaesten mit ihren genutzten
 *        Spalten, Joins als Kanten mit ihrer Bedingung, je Block ein Ergebnis, Unterabfragen in Rahmen
 *
 * Abgeleitet aus modell::Abfrage. Eine Bedingung, die Spalten genau zweier Quellen vergleicht, ist eine
 * Kante zwischen ihnen — aus ON mit der Art des Joins, aus WHERE gestrichelt; ein Bezug nach aussen
 * (korreliert) gepunktet; die uebrigen Teile sind Filter. Jede Quelle, von der keine Kante weiterfuehrt,
 * fuehrt in das Ergebnis ihres Blocks; das Ergebnis einer Unterabfrage fuehrt dorthin, wo sie gebraucht
 * wird.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"  // kKeine
#include "c2d/diagramm/Wahrheitstabelle.hpp"
#include "c2d/modell/Code.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::diagramm
{

enum class AbfrageKnotenArt
{
    Tabelle,     ///< eine Tabelle, die das Schema kennt
    View,        ///< eine View des Schemas
    Fremd,       ///< ein Name, den das Schema nicht kennt (grau)
    Mit,         ///< ein Name eines WITH
    Abgeleitet,  ///< eine Unterabfrage in FROM, unter ihrem Alias
    Funktion,    ///< eine Funktion, die eine Tabelle liefert
    Werte,       ///< VALUES
    Ergebnis,    ///< was ein Block liefert: select, where, group by …
    Menge,       ///< UNION / INTERSECT / EXCEPT
    Ziel,        ///< wohin die Abfrage schreibt
};

struct AbfrageKnoten
{
    AbfrageKnotenArt art = AbfrageKnotenArt::Tabelle;
    std::string titel;                ///< `film f`, `result`, `UNION ALL`, `insert into payment`
    std::vector<std::string> zeilen;  ///< die genutzten Spalten; beim Ergebnis `select …`, `where …` (ungekuerzt)
    std::size_t gruppe = kKeine;      ///< der innerste Rahmen
    std::string name;                 ///< Tabelle, View, Name des WITH (fuer Doppelklick); sonst leer
    int zeile = 0;                    ///< im Quelltext; 0 = keine
    /// Die Stellen ihrer Bedingungen mit Wahrheitstabelle (Entscheide W6, L2): am Ergebnis WHERE, dann HAVING; an einer
    /// Quelle ihr ON. Leer = keine.
    std::vector<Tabellenstelle> tabellen;
    /// Am Ergebnis: die CASE-Ausdruecke des Blocks mit Zuordnung (Entscheid Z4), je ihr Name hinter `mapping:`.
    std::vector<std::string> faelle;
};

enum class AbfrageKantenArt
{
    Join,        ///< aus ON (oder USING, NATURAL, CROSS, APPLY): durchgezogen
    Wo,          ///< aus WHERE: gestrichelt
    Korreliert,  ///< eine Unterabfrage nennt eine Quelle draussen: gepunktet
    Fluss,       ///< in ein Ergebnis oder dorthin, wo es gebraucht wird: mit Spitze
    Schreibt,    ///< vom Ergebnis zum Ziel: mit Spitze
};

struct AbfrageKante
{
    std::size_t von = 0;
    std::size_t nach = 0;
    AbfrageKantenArt art = AbfrageKantenArt::Join;
    std::string text;  ///< `left: f.id = i.film_id`, `where: …`, `exists`, `insert into` (ungekuerzt)
};

struct AbfrageRahmen
{
    std::string titel;            ///< `WITH name`, `derived t`, `subquery (EXISTS)`, `branch 2`
    std::size_t eltern = kKeine;  ///< der Rahmen, in dem dieser liegt
};

struct Abfragediagramm
{
    std::string titel;  ///< der Name der Abfrage
    std::vector<AbfrageKnoten> knoten;
    std::vector<AbfrageKante> kanten;
    std::vector<AbfrageRahmen> rahmen;
};

/// Was das Schema ueber einen Namen weiss: seine Tabelle (mit Spalten), oder dass er eine View ist.
struct Datenquelle
{
    const modell::Typ* tabelle = nullptr;
    bool view = false;
};
using DatenquellenSuche = std::function<Datenquelle(std::string_view name)>;

/// Das Diagramm der Abfrage. `suche` (leer: das Schema ist unbekannt) ordnet Spalten ohne Alias zu und
/// sagt, welche Namen Tabellen und Views sind.
[[nodiscard]] Abfragediagramm abfrageVon(const modell::Abfrage& abfrage, const DatenquellenSuche& suche = {});

/// Die Abfrage, die in dieser Zeile beginnt, sonst die, in der sie liegt; kKeine = keine.
[[nodiscard]] std::size_t abfrageZurZeile(const std::vector<modell::Abfrage>& abfragen, int zeile);

/// Kann die Spalte `s` in Block `block` NULL sein (Entscheid W3)? Nie: eine Spalte mit NOT NULL einer Tabelle des
/// Schemas — ausser ihre Quelle ist die aufgefuellte Seite eines aeusseren Joins (LEFT, RIGHT, FULL, OUTER APPLY) und
/// die Bedingung steht nicht im ON dieses Joins (`imOn`: die Quelle, deren ON es ist; kKeine = WHERE/HAVING). Kann:
/// eine Spalte ohne NOT NULL. Unbekannt: Quelle oder Spalte kennt das Schema nicht (WITH, Unterabfrage, Funktion).
[[nodiscard]] NullFaehig nullFaehigkeit(const modell::Abfrage& abfrage, std::size_t block, const modell::Spaltenbezug& s,
                                        const DatenquellenSuche& suche, std::size_t imOn = kKeine);

/// Alle Wahrheitstabellen einer Abfrage (Entscheid W1): je Block WHERE, HAVING, je Quelle ON, mit mindestens zwei
/// Atomen; in der Reihenfolge der Bloecke.
[[nodiscard]] std::vector<Wahrheitstabelle> wahrheitstabellenVon(const modell::Abfrage& abfrage,
                                                                 const DatenquellenSuche& suche = {});

/// Die Zeilen der Bedingungen einer Abfrage mit Tabelle — ohne sie auszurechnen.
[[nodiscard]] std::vector<int> zeilenMitWahrheitstabelle(const modell::Abfrage& abfrage);

/// Ihre Stellen (Entscheid L2), in derselben Folge wie wahrheitstabellenVon — ohne sie auszurechnen.
[[nodiscard]] std::vector<Tabellenstelle> stellenMitWahrheitstabelle(const modell::Abfrage& abfrage);

}  // namespace c2d::diagramm
