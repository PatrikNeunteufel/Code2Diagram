/**
 * @file Wahrheitstabelle.hpp
 * @brief Die Wahrheitstabelle einer Bedingung und ihre MC/DC-Zeilen (Unique-Cause)
 *
 * Abgeleitet aus der Logik einer Anweisung (modell::Anweisung::logik), die der
 * Leser aufstellt. Je Atom eine Spalte, je Kombination eine Zeile; wo eine
 * Bedingung wegen Kurzschluss (`&&`, `||` von links) ein Atom nicht auswertet,
 * steht Egal. MC/DC nach Unique-Cause: ein Paar Zeilen je Atom, die sich nur in
 * ihm unterscheiden und verschieden ausgehen; gesucht ist eine kleinste Menge
 * von Zeilen, die fuer jedes Atom ein Paar enthaelt.
 */
#pragma once

#include "c2d/modell/Code.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace c2d::diagramm
{

enum class Wert
{
    Falsch,
    Wahr,
    Egal,       ///< nicht ausgewertet (Kurzschluss)
    Unbekannt,  ///< SQL: NULL im Vergleich (Entscheid W3, Kleene)
};

struct Wahrheitstabelle
{
    std::string funktion;   ///< ihr voller Name (in SQL: der Name der Abfrage)
    std::string bedingung;  ///< der Text der Bedingung
    int quellZeile = 0;
    /// SQL: welche Bedingung der Abfrage (`WHERE`, `HAVING`, `ON`); leer = eine Bedingung einer Funktion.
    std::string klausel;

    struct Atom
    {
        std::string text;
        bool mitAufruf = false;
        bool kannNull = false;  ///< SQL: es kann unbekannt sein (dreiwertig, Entscheid W3)
    };
    std::vector<Atom> atome;  ///< in der Reihenfolge ihres ersten Vorkommens

    struct Zeile
    {
        std::size_t nummer = 0;   ///< 1 ...: die Kombination (Atom 1 links, falsch vor wahr vor unbekannt)
        std::vector<Wert> werte;  ///< je Atom
        bool ergebnis = false;    ///< wahr (in SQL: die Zeile bleibt, nur wenn die Bedingung wahr ist)
        bool mcdc = false;        ///< gehoert zur kleinsten MC/DC-Menge
        Wert wert = Wert::Falsch;  ///< nur dreiwertig: wahr, falsch oder unbekannt
    };
    /// SQL (Entscheid W3): dreiwertig nach Kleene, ohne Kurzschluss (W5); `bleibt` ist der Titel der Spalte, die sagt,
    /// ob die Zeile bleibt (`Row kept`, bei ON `Joined`).
    bool dreiwertig = false;
    std::string bleibt;
    /// Bis kVolleTabelle Atome jede Kombination (in SQL bis kVolleSqlTabelle Zeilen); darueber nur die MC/DC-Zeilen.
    std::vector<Zeile> zeilen;
    bool gekuerzt = false;

    /// Je Atom sein Paar: die Nummern zweier Zeilen; kein Paar: {0, 0}.
    std::vector<std::pair<std::size_t, std::size_t>> paare;
    /// Ist die MC/DC-Menge sicher die kleinste? (Bei vielen Atomen nur eine kleine.)
    bool kleinste = true;
    std::vector<std::string> hinweise;
};

/// Die Kennung eines Atoms in Tabelle und Hinweisen: A, B, ... Z, A1, B1, ...
[[nodiscard]] std::string atomKennung(std::size_t atom);

/// Bis so viele Atome zeigt die Tabelle jede Kombination.
inline constexpr std::size_t kVolleTabelle = 6;
/// Ueber so viele Atome gibt es keine Tabelle (2^n Kombinationen), nur einen Hinweis.
inline constexpr std::size_t kHoechstensAtome = 16;

/// Die Tabelle der Bedingung `logik` (`text`) in Zeile `zeile` der Funktion `funktion`.
[[nodiscard]] Wahrheitstabelle wahrheitstabelleVon(const modell::Logik& logik, std::string_view text,
                                                   std::string_view funktion, int zeile);

/// SQL: bis so viele Zeilen zeigt die Tabelle jede Kombination (Entscheid W3: vier dreiwertige Atome).
inline constexpr std::size_t kVolleSqlTabelle = 81;
/// SQL: ueber so viele Kombinationen werden die Zeilen mit *unbekannt* nicht mehr ausgerechnet (nur ein Hinweis).
inline constexpr std::size_t kHoechstensSqlZeilen = 200000;

/// Ob ein Atom einer SQL-Bedingung unbekannt sein kann (Entscheid W3) — das weiss, wer das Schema kennt.
enum class NullFaehig
{
    Nie,        ///< der Form nach (`IS NULL`, `EXISTS`, Literale) oder nur Spalten mit NOT NULL
    Kann,       ///< eine Spalte ohne NOT NULL
    Unbekannt,  ///< eine Spalte, die das Schema nicht kennt, oder eine Variable: gilt als NULL-faehig (Hinweis)
};

/// Die Tabelle einer Bedingung in SQL (Entscheide W3-W5, R7): dreiwertig fuer Atome, die NULL sein koennen
/// (`nullFaehig` je Atom), ohne Kurzschluss; MC/DC ueber die Spalte `bleibt` (`Row kept`, `Joined`, `Branch`; leer =
/// keine solche Spalte, MC/DC ueber *wahr*) aus den Zeilen ohne *unbekannt*. `klausel`: `WHERE`, `HAVING`, `ON` einer
/// Abfrage (`name` ist die Abfrage); leer = eine Bedingung einer Routine (`name` ist sie).
[[nodiscard]] Wahrheitstabelle wahrheitstabelleVonSql(const modell::Logik& logik, std::string_view text,
                                                      std::string_view name, std::string_view klausel, int zeile,
                                                      const std::function<NullFaehig(const modell::Logik&)>& nullFaehig,
                                                      std::string_view bleibt);

/// Die Ueberschrift einer Tabelle: `f(): line 12`, in SQL `query@3 WHERE: line 5`.
[[nodiscard]] std::string ueberschrift(const Wahrheitstabelle& t);

/// Der Text eines Werts in einer Zelle: T, F, U, `–` (in csv `-`).
[[nodiscard]] std::string wertText(Wert w, bool csv = false);

/// Die Zeilen der Funktion, in denen eine Bedingung mit Tabelle steht — ohne sie auszurechnen.
[[nodiscard]] std::vector<int> zeilenMitWahrheitstabelle(const modell::Funktion& funktion);

/// Alle Tabellen einer Funktion, in der Reihenfolge ihrer Bedingungen.
[[nodiscard]] std::vector<Wahrheitstabelle> wahrheitstabellenVon(const modell::Funktion& funktion);

/// Die Tabelle der Bedingung in dieser Zeile der Funktion; nullopt = dort steht keine.
[[nodiscard]] std::optional<Wahrheitstabelle> wahrheitstabelleVon(const modell::Funktion& funktion, int zeile);

}  // namespace c2d::diagramm
