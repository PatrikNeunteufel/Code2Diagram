/**
 * @file Zustandsdiagramm.hpp
 * @brief Das Diagramm-Modell eines Zustandsautomaten: Zustaende und Uebergaenge
 *
 * Erkannt wird ein Automat im Code (diagramm/Ableitung.hpp, automatenVon) an
 * einem `switch (V)`, in dessen Faellen `V = K` steht, wobei `K` selbst eine
 * Fall-Marke desselben `switch` ist. Gegenstueck zu Flussdiagramm.hpp und
 * Strukturdiagramm.hpp.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

struct Zustand
{
    std::string name;  ///< die Fall-Marke oder der zugewiesene Wert; `default` fuer den Standardfall
    /// Kein Zustand, sondern der Punkt, von dem Uebergaenge "von aussen" kommen:
    /// Zuweisungen ausserhalb des `switch` (Init, ISR, andere Funktionen).
    bool aussen = false;
    int zeile = 0;  ///< die Zeile seines Falls; 0 = er hat keinen
};

struct Uebergang
{
    std::size_t von = 0;
    std::size_t nach = 0;
    /// Unter welchen Bedingungen: die `if`/`while`/`case`, unter denen die
    /// Zuweisung steht, mit `&&` verbunden; leer = immer. Bei einem Uebergang von
    /// aussen steht vorn die Funktion (`in init(): ...`).
    std::string bedingung;
    int zeile = 0;  ///< die Zeile der Zuweisung
};

struct Zustandsdiagramm
{
    std::string titel;     ///< etwa `Bus_State in I2C_Handler()`
    std::string variable;  ///< der Ausdruck im `switch`, ohne Leerzeichen
    std::string funktion;  ///< die Funktion mit dem `switch`, mit Bereich
    int zeile = 0;         ///< die Zeile des `switch`
    std::vector<Zustand> zustaende;
    std::vector<Uebergang> uebergaenge;
};

}  // namespace c2d::diagramm
