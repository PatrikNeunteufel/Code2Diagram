/**
 * @file Konfigurationstabelle.hpp
 * @brief Je Kette `#if` / `#elif` / `#else`: welcher Zweig bei welchen Definitionen gilt
 *
 * Je Zweig eine Zeile, je Name, nach dem die Kette fragt, eine Spalte; in der Zelle, was
 * der Zweig von ihm fordert (`def`, `!def`, `> 50`, `–` = gleich). Die Forderung ist
 * absolut: der Zweig gilt, wenn seine Bedingung wahr ist, alle davor falsch sind und der
 * Zweig gilt, in dem die Kette steht. Ein `||` gibt je Alternative eine Zeile (`or`), bis
 * kMaxAlternativen; laesst sich eine Bedingung nicht nach Namen zerlegen (`F(2) > 1`), steht
 * sie als Text in einer Zeile, mit Hinweis. Gerechnet wird nur mit den Zahlen der Bedingungen.
 */
#pragma once

#include "c2d/modell/Code.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace c2d::diagramm
{

/// So viele Alternativen (Zeilen) bekommt ein Zweig hoechstens; mehr: eine Zeile Text.
inline constexpr std::size_t kMaxAlternativen = 8;

struct Konfigurationstabelle
{
    struct Spalte
    {
        std::string name;
        std::string jetzt;          ///< wie es jetzt um ihn steht: `= 100`, `def`, `undef`
        int inDatei = 0;            ///< die Datei definiert ihn selbst, vor der Frage (Zeile); 0 = nicht
        bool vorgegeben = false;    ///< die Sprache definiert ihn (`__cplusplus`)
    };
    struct Zeile
    {
        std::string zweig;                ///< `#if defined(A)`, `#elif X > 2`, `#else`, `(no #else)`; weitere Alternative: `or`
        std::vector<std::string> werte;   ///< je Spalte
        bool geltend = false;             ///< der Zweig gilt jetzt
        bool nie = false;                 ///< kein Satz von Definitionen laesst ihn gelten
        bool text = false;                ///< nicht nach Namen zerlegt: `bedingung` statt `werte`
        std::string bedingung;            ///< der Text der Bedingung (Tooltip; bei `text` die Zeile)
        std::size_t zweigIndex = 0;       ///< in modell::Weiche (zweige.size() = das fehlende `#else`)
        int quellZeile = 0;               ///< des `#if`, `#elif`, `#else` (beim fehlenden: des `#endif`)
    };
    std::string titel;          ///< `#if defined(A) (line 12)`, innen dazu `, in #if B (line 3)`
    std::size_t weiche = 0;     ///< ihr Index in den Weichen der Datei
    int quellZeile = 0;
    int tiefe = 0;              ///< wie tief die Kette in anderen steht
    std::vector<Spalte> spalten;
    std::vector<Zeile> zeilen;
    std::vector<std::string> hinweise;
};

/// Wie ein Zweig in der Tabelle heisst: `#if A`, `#elif B`, `#else`, `(no #else)` (z = zweige.size()).
[[nodiscard]] std::string zweigMarke(const modell::Weiche& weiche, std::size_t z);
/// Die Zeile des `#if`/`#elif`/`#else` von Zweig z; beim fehlenden `#else` die des `#endif`.
[[nodiscard]] int zweigZeile(const modell::Weiche& weiche, std::size_t z);

/// Je Kette eine Tabelle, in der Reihenfolge des Texts (eine innere nach ihrer aeusseren).
/// `schalter`: wonach die Datei fragt, mit dem Stand dort (sprache::Ergebnis::schalter).
[[nodiscard]] std::vector<Konfigurationstabelle> konfigurationVon(std::span<const modell::Weiche> weichen,
                                                                  std::span<const modell::Schalter> schalter);

}  // namespace c2d::diagramm
