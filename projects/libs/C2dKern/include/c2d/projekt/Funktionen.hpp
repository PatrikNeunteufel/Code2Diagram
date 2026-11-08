/**
 * @file Funktionen.hpp
 * @brief Welche Funktion ein Aufruf meint, ueber die Dateien eines Projekts
 *
 * Gegenstueck zu Typverzeichnis (Typen.hpp) fuer Funktionen. Ein Aufruf meint die Funktionen mit Rumpf,
 * die so heissen (modell::Funktion::heisst): die der eigenen Datei, wenn es dort welche gibt; sonst die
 * nicht `intern`en der anderen Dateien. Mehr als eine: es ist nicht entschieden, welche.
 *
 * Das Verzeichnis ordnet die Funktionen nach einem Schluessel, den `heisst` erzwingt — ausser in SQL der
 * letzte Teil nach `::` des vollen Namens, in SQL der letzte Teil nach `.` ohne Gross/klein. Ein Aufruf
 * sieht nur die Funktionen mit seinem Schluessel; ueber sie entscheidet `heisst` wie ohne Verzeichnis.
 * Vorher ging jeder Aufruf alle Funktionen des Projekts durch (UART, rund 10 000: 0,16 ms je Aufruf, beim
 * Aufrufgraph einer Datei 33 von 48 s Wechselzeit im Fenster).
 */
#pragma once

#include "c2d/projekt/Projekt.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace c2d::projekt
{

class Funktionsverzeichnis
{
public:
    /// Ein leeres Verzeichnis zu einem leeren Projekt.
    Funktionsverzeichnis();
    /// Haelt einen Verweis auf das Projekt: neu bauen, wenn es neu gelesen wird.
    explicit Funktionsverzeichnis(const Projekt& projekt);

    /// Das Projekt, aus dem das Verzeichnis gebaut ist.
    [[nodiscard]] const Projekt& projekt() const { return *m_projekt; }

    /// Welche Funktionen mit Rumpf ein Aufruf aus `ausDatei` meinen kann (siehe oben), in der
    /// Reihenfolge der Dateien und Funktionen.
    [[nodiscard]] std::vector<Fundstelle> kandidaten(std::size_t ausDatei, std::string_view aufruf) const;

    /// Wer die Funktion an `ziel` ruft: jede Funktion mit Rumpf im Projekt, von der ein Aufruf sie meinen
    /// kann (nach `kandidaten`, auch wenn er mehrdeutig ist), jede einmal, in der Reihenfolge der Dateien.
    [[nodiscard]] std::vector<Fundstelle> aufrufer(const Fundstelle& ziel) const;

private:
    using Tafel = std::unordered_map<std::string, std::vector<Fundstelle>>;
    /// Was jede Funktion ruft, nach dem Schluessel des Aufrufs: erst beim ersten `aufrufer` gebaut (das liest
    /// die Aufrufe aller Rumpfe), danach je Ziel nur die Rufer mit seinem Schluessel.
    struct Rufe
    {
        std::unordered_map<std::string, std::vector<std::pair<Fundstelle, std::string>>> genau, ohneFall;
    };
    const Rufe& rufe() const;

    const Projekt* m_projekt = nullptr;
    Tafel m_genau;     ///< ausser SQL: letzter Teil nach `::` des vollen Namens -> Funktionen
    Tafel m_ohneFall;  ///< SQL (Funktion::namenOhneFall): letzter Teil nach `.`, klein -> Funktionen
    mutable std::shared_ptr<const Rufe> m_rufe;
};

}  // namespace c2d::projekt
