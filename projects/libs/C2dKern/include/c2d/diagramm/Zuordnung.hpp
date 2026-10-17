/**
 * @file Zuordnung.hpp
 * @brief Zuordnungstabellen: ein switch, der nur Werte zuordnet, und konstante Tabellen (Arrays)
 *
 * Ein `switch` ist eine Zuordnung, wenn jeder Fall nur einen Wert liefert: ein
 * `return <Ausdruck>`, oder Zuweisungen an dieselben Namen in jedem Fall, dann
 * `break`. Je Wert eines Falls eine Zeile (geteilte Faelle je eine), `default`
 * zuletzt; fehlt es, eine Zeile `otherwise` mit `—`. Eine konstante Tabelle
 * (`const` mit `{ … }`) gibt Index -> Wert, ein Array von Strukturen je Glied
 * eine Spalte. Werte stehen als Text der Quelle; gerechnet wird nichts.
 */
#pragma once

#include "c2d/modell/Code.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::diagramm
{

struct Zuordnung
{
    std::string titel;     ///< `f(): switch (phase)` oder die Deklaration der Tabelle
    int quellZeile = 0;    ///< der switch bzw. die Tabelle
    std::string schluessel;  ///< der Kopf der linken Spalte: der Ausdruck des switch, `index`
    std::vector<std::string> spalten;  ///< die Koepfe der Werte: `return`, die Namen, die Glieder

    struct Zeile
    {
        std::string schluessel;
        std::vector<std::string> werte;  ///< je Spalte; `—` = unveraendert (otherwise)
        int quellZeile = 0;
    };
    std::vector<Zeile> zeilen;
};

/// Die Zuordnung des switch in dieser Zeile der Funktion; nullopt = dort keiner, oder er tut mehr.
[[nodiscard]] std::optional<Zuordnung> zuordnungVon(const modell::Funktion& funktion, int zeile);

/// Die Zeilen der Funktion mit einem switch, der eine Zuordnung ist.
[[nodiscard]] std::vector<int> zeilenMitZuordnung(const modell::Funktion& funktion);

/// Findet einen Typ ausserhalb der Einheit (etwa in einem eingebundenen Kopf); nullptr = keiner
/// oder nicht eindeutig.
using TypSuche = std::function<const modell::Typ*(std::string_view name)>;

/// Die konstanten Tabellen der Einheit (Arrays mit Anfangswert), in ihrer Reihenfolge. Die Glieder
/// eines Strukturtyps (die Spalten) kommen aus der Einheit, sonst aus `suche`, sonst `1`, `2`, ….
[[nodiscard]] std::vector<Zuordnung> nachschlagetabellenVon(const modell::Einheit& einheit, const TypSuche& suche = {});

}  // namespace c2d::diagramm
