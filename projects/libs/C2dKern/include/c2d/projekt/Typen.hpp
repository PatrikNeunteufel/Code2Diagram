/**
 * @file Typen.hpp
 * @brief Welchen Typ ein Name meint, ueber die Dateien eines Projekts
 *
 * Gegenstueck zu `kandidaten` (Projekt.hpp) fuer Funktionen: ein Typname meint
 * zuerst einen Typ der eigenen Datei, dann einen der Koepfe, die sie einbindet
 * (auch mittelbar, Datei::koepfe), dann einen jeder anderen Datei des Projekts.
 * Ein `typedef` (`modell::Alias`) fuehrt zu dem Typ, den es nennt — aufgeloest
 * von der Datei aus, in der es steht. Bleiben mehrere, ist nicht entschieden,
 * welcher gemeint ist: wer fragt, laesst waehlen.
 *
 * Erkannt wird am Namen (modell::bezeichnerIn), nicht am Typ eines Ausdrucks:
 * `Ampel a;` nennt `Ampel`, `ampel->phase` nicht.
 */
#pragma once

#include "c2d/projekt/Projekt.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::projekt
{

/// Ein Typ in einer Datei des Projekts.
struct TypStelle
{
    std::size_t datei = kKeine;
    std::size_t typ = kKeine;  ///< Index in `ergebnis.einheit.typen`

    bool operator==(const TypStelle&) const = default;
};

/// Ein Name in einem Text, der Typen des Projekts meint.
struct TypVerweis
{
    std::string name;
    std::vector<TypStelle> ziele;  ///< mehr als eines: nicht entschieden, welches
};

class Typverzeichnis
{
public:
    Typverzeichnis() = default;
    /// Haelt einen Verweis auf das Projekt: neu bauen, wenn es neu gelesen wird.
    explicit Typverzeichnis(const Projekt& projekt);

    /// Welche Typen der Name (allein oder mit Bereich) aus der Datei `ausDatei` meinen kann.
    [[nodiscard]] std::vector<TypStelle> kandidaten(std::size_t ausDatei, std::string_view name) const;
    /// Die Namen eines Texts (modell::bezeichnerIn), die Typen meinen, in der Reihenfolge des Texts.
    [[nodiscard]] std::vector<TypVerweis> verweiseIn(std::size_t ausDatei, std::string_view text) const;

private:
    [[nodiscard]] std::vector<TypStelle> inDatei(std::size_t datei, std::string_view name, int tiefe) const;
    [[nodiscard]] std::vector<TypStelle> suche(std::size_t ausDatei, std::string_view name, int tiefe) const;

    const Projekt* m_projekt = nullptr;
    /// Name und voller Name -> Typen; Name eines typedef -> (Datei, Ziel)
    std::map<std::string, std::vector<TypStelle>, std::less<>> m_typen;
    std::map<std::string, std::vector<std::pair<std::size_t, std::string>>, std::less<>> m_aliase;
};

/// Der Typ an einer Stelle.
[[nodiscard]] const modell::Typ& typ(const Projekt& projekt, const TypStelle& stelle);

}  // namespace c2d::projekt
