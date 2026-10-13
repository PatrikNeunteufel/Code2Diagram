/**
 * @file Meldung.hpp
 * @brief Ort im Quelltext und Meldungen der Analyse
 *
 * Jede Stufe (Lexer, Leser einer Sprache) sagt, was sie nicht verstanden hat —
 * nichts wird still verworfen (Lehre 2 und 6, docs/Ernte_Vorgaenger.md).
 */
#pragma once

#include <cstddef>
#include <string>

namespace c2d::quelle
{

struct Position
{
    std::size_t versatz = 0;  ///< Bytes ab Anfang des Texts
    int zeile = 1;
    int spalte = 1;  ///< in Bytes, ab 1
};

enum class Schwere
{
    Hinweis,
    Warnung,
    Fehler,
};

struct Meldung
{
    Schwere schwere = Schwere::Fehler;
    Position ort;
    std::string text;
};

}  // namespace c2d::quelle
