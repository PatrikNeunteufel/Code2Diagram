/**
 * @file Entscheidungstabelle.hpp
 * @brief Die Entscheidungstabelle einer Funktion (DIN 66241): je Weg durch ihre Verzweigungen eine Regel
 *
 * Eine Regel ist ein Weg vom Anfang bis zu einem `return` (`throw`, `goto`) oder
 * dem Ende, durch `if`/`else`, `switch`-Faelle und `?:`. Je Verzweigung eine
 * Bedingung (ihr ganzer Text; gleicher Text ist dieselbe), ein `switch` mit
 * erweitertem Eintrag (der Wert des Falls). Je Anweisung auf einem Weg eine
 * Aktion; eine Schleife ist eine Aktion mit ihrem Kopf, ihr Inhalt wird nicht
 * zerlegt. Eine Bedingung gilt auf einem Weg als entschieden, bis eine Anweisung
 * danach etwas ruft oder einen ihrer Namen schreibt (auch durch einen Zeiger);
 * solange folgt der Weg ihrem Wert — die anderen Wege gaebe es nicht (Hinweis).
 * Wird sie danach neu gefragt, steht das in einer eigenen Zeile (`erneut`): sie
 * kann anders ausgehen, und eine Zeile traegt je Regel nur einen Wert. Eine
 * Bedingung, die selbst etwas ruft, gilt nie als entschieden.
 */
#pragma once

#include "c2d/modell/Code.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

struct Entscheidungstabelle
{
    std::string funktion;  ///< ihr voller Name
    int quellZeile = 0;

    struct Bedingung
    {
        std::string text;
        int quellZeile = 0;   ///< wo sie zuerst steht
        bool fall = false;    ///< ein switch: die Eintraege sind Werte, nicht Y/N
        bool mitAufruf = false;
        bool erneut = false;  ///< neu gefragt, nachdem sich ihr Wert aendern konnte (quellZeile: wo)
    };
    struct Aktion
    {
        std::string text;
        int quellZeile = 0;
    };
    std::vector<Bedingung> bedingungen;  ///< in der Reihenfolge, in der Wege sie zuerst treffen
    std::vector<Aktion> aktionen;        ///< ebenso

    struct Regel
    {
        /// Je Bedingung: `Y`, `N`, der Wert eines Falls — leer: auf diesem Weg nicht gefragt.
        std::vector<std::string> eintraege;
        std::vector<bool> aktionen;  ///< je Aktion: auf diesem Weg ausgefuehrt
    };
    /// Leer, wenn es zu viele waeren (kHoechstensRegeln) — dann sagt es ein Hinweis.
    std::vector<Regel> regeln;
    std::vector<std::string> hinweise;
};

/// Wie eine Bedingung in ihrer Zeile heisst: ihr Text, ein switch als `switch (…)`, neu gefragt mit
/// `(again, line n)`.
[[nodiscard]] std::string zeilenText(const Entscheidungstabelle::Bedingung& bedingung);

/// Bis so viele Regeln gibt es eine Tabelle; darueber keine, damit keine halbe fuer ganz gilt.
inline constexpr std::size_t kHoechstensRegeln = 32;

[[nodiscard]] Entscheidungstabelle entscheidungstabelleVon(const modell::Funktion& funktion);

}  // namespace c2d::diagramm
