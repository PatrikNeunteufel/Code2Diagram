/**
 * @file Leser.hpp
 * @brief Basis der Leser in rekursivem Abstieg: Token-Folge, Klammern, Text
 *
 * Was jeder Leser braucht, gleich fuer welche Sprache: die Stelle in der
 * Token-Folge, das Ueberspringen geklammerter Gruppen, das Suchen bis zu einem
 * Trenner und der Text eines Token-Bereichs. Die Grammatik selbst steht im
 * Leser der Sprache (etwa c/CSprache.cpp).
 *
 * Intern (nicht unter include/): nur die Leser binden diesen Kopf ein.
 */
#pragma once

#include "c2d/lexer/Lexer.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace c2d::sprache
{

class Leser
{
protected:
    using Token = lexer::Token;
    using TokenArt = lexer::TokenArt;
    /// Ein Bereich von Token: [first, second)
    using Bereich = std::pair<std::size_t, std::size_t>;

    static constexpr std::size_t kKeiner = static_cast<std::size_t>(-1);

    Leser(std::string_view text, const lexer::Sprachtabelle& tabelle, Ergebnis& aus);

    /// Das Token an Stelle i; hinter dem Ende immer das Token `Ende`.
    [[nodiscard]] const Token& tok(std::size_t i) const { return m_t[i < m_t.size() ? i : m_t.size() - 1]; }
    [[nodiscard]] const Token& akt() const { return tok(m_p); }
    [[nodiscard]] bool amEnde(std::size_t i) const { return tok(i).art == TokenArt::Ende; }
    /// Das aktuelle Token nehmen, wenn es dieser Operator ist.
    bool nimm(std::string_view op);

    void melde(quelle::Schwere schwere, const Token& bei, std::string text);

    /// ( [ {  bzw.  ) ] }
    [[nodiscard]] static bool oeffnet(const Token& t);
    [[nodiscard]] static bool schliesst(const Token& t);

    /// i steht auf ( [ oder {: die Stelle hinter der passenden schliessenden
    /// Klammer. Fehlt sie, die Stelle des Tokens `Ende`.
    [[nodiscard]] std::size_t nachKlammer(std::size_t i) const;

    /// Ab i das erste Token ausserhalb von Klammern, das einer der `stopps`
    /// ist — oder eine schliessende Klammer ohne oeffnende, oder `Ende`.
    [[nodiscard]] std::size_t suche(std::size_t i, std::initializer_list<std::string_view> stopps) const;

    /// [a, b) an den Kommas ausserhalb von Klammern teilen. Leere Stuecke fallen weg.
    [[nodiscard]] std::vector<Bereich> teile(std::size_t a, std::size_t b) const;

    /// Der Text der Token [a, b), mit einem Leerzeichen, wo im Quelltext Abstand war.
    [[nodiscard]] std::string textVon(std::size_t a, std::size_t b) const;

    Ergebnis& m_aus;
    std::vector<Token> m_t;
    std::size_t m_p = 0;
};

}  // namespace c2d::sprache
