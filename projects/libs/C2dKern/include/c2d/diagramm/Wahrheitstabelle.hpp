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
    Egal,  ///< nicht ausgewertet (Kurzschluss)
};

struct Wahrheitstabelle
{
    std::string funktion;   ///< ihr voller Name
    std::string bedingung;  ///< der Text der Bedingung
    int quellZeile = 0;

    struct Atom
    {
        std::string text;
        bool mitAufruf = false;
    };
    std::vector<Atom> atome;  ///< in der Reihenfolge ihres ersten Vorkommens

    struct Zeile
    {
        std::size_t nummer = 0;   ///< 1 ... 2^n: die Kombination (Atom 1 links, falsch vor wahr)
        std::vector<Wert> werte;  ///< je Atom
        bool ergebnis = false;
        bool mcdc = false;        ///< gehoert zur kleinsten MC/DC-Menge
    };
    /// Bis kVolleTabelle Atome jede Kombination; darueber nur die MC/DC-Zeilen (gekuerzt).
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

/// Die Zeilen der Funktion, in denen eine Bedingung mit Tabelle steht — ohne sie auszurechnen.
[[nodiscard]] std::vector<int> zeilenMitWahrheitstabelle(const modell::Funktion& funktion);

/// Alle Tabellen einer Funktion, in der Reihenfolge ihrer Bedingungen.
[[nodiscard]] std::vector<Wahrheitstabelle> wahrheitstabellenVon(const modell::Funktion& funktion);

/// Die Tabelle der Bedingung in dieser Zeile der Funktion; nullopt = dort steht keine.
[[nodiscard]] std::optional<Wahrheitstabelle> wahrheitstabelleVon(const modell::Funktion& funktion, int zeile);

}  // namespace c2d::diagramm
