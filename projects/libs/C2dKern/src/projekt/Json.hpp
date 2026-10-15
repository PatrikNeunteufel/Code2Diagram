/**
 * @file Json.hpp
 * @brief Ein kleiner JSON-Leser fuer die Projektdatei — Eigenbau wie Lexer und Leser
 *
 * Liest RFC 8259 ohne Erweiterungen (keine Kommentare, kein Komma am Ende).
 * Jeder Wert weiss, wo er stand, damit eine Meldung Zeile und Spalte nennt.
 *
 * Intern (nicht unter include/).
 */
#pragma once

#include "c2d/quelle/Meldung.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace c2d::projekt::json
{

enum class Art
{
    Nichts,  ///< null
    Wahr,
    Falsch,
    Zahl,    ///< `text` haelt sie, wie sie geschrieben stand
    Text,
    Liste,
    Objekt,
};

struct Wert
{
    Art art = Art::Nichts;
    std::string text;                ///< Text (ohne Anfuehrungszeichen, entschluesselt) oder Zahl
    std::vector<Wert> elemente;      ///< Liste; bei Objekt die Werte zu `namen`
    std::vector<std::string> namen;  ///< Objekt: die Schluessel, in der Reihenfolge des Texts
    quelle::Position ort;
};

/// Liest den ganzen Text als einen Wert. false = kein gueltiges JSON; dann
/// nennt `fehler` Ort und Grund, und `aus` ist unbestimmt.
[[nodiscard]] bool lies(std::string_view text, Wert& aus, quelle::Meldung& fehler);

/// Ein Text als JSON-Zeichenkette, mit Anfuehrungszeichen.
[[nodiscard]] std::string zitat(std::string_view text);

/// Wie die Art im Text einer Meldung heisst.
[[nodiscard]] std::string_view name(Art art);

}  // namespace c2d::projekt::json
