/**
 * @file Lexer.hpp
 * @brief Der allgemeine Lexer: Text -> Token, gesteuert von einer Sprachtabelle
 *
 * Die Maschine ist fuer alle Sprachen dieselbe; was eine Sprache ausmacht
 * (Schluesselwoerter, Operatoren, Kommentare, Direktiven), steht in ihrer
 * `Sprachtabelle` — als Tabelle im Code der Sprache, nicht in einer
 * Konfigurationsdatei.
 *
 * Die Token zeigen in den Text: er muss sie ueberleben.
 */
#pragma once

#include "c2d/quelle/Meldung.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace c2d::lexer
{

enum class TokenArt
{
    Bezeichner,
    Schluesselwort,
    Zahl,
    Zeichenkette,
    Zeichen,
    Operator,  ///< auch Satzzeichen und Klammern
    Kommentar,
    Direktive,  ///< eine ganze Zeile, etwa `#include <x.h>` (mit Fortsetzungen)
    Unbekannt,  ///< ein Zeichen, das die Sprache nicht kennt; dazu eine Meldung
    Ende,
};

struct Token
{
    TokenArt art = TokenArt::Ende;
    std::string_view text;
    quelle::Position ort;
    bool abstandDavor = false;  ///< Leerraum, Zeilenwechsel oder Kommentar davor

    [[nodiscard]] bool istOp(std::string_view t) const { return art == TokenArt::Operator && text == t; }
    [[nodiscard]] bool istWort(std::string_view t) const
    {
        return art == TokenArt::Schluesselwort && text == t;
    }
};

/// Die lexikalische Beschreibung einer Sprache.
struct Sprachtabelle
{
    std::span<const std::string_view> schluesselwoerter;
    /// Operatoren und Satzzeichen; der laengste Treffer gewinnt.
    std::span<const std::string_view> operatoren;
    std::string_view zeilenKommentar;  ///< leer = keiner
    std::string_view blockKommentarAuf;
    std::string_view blockKommentarZu;
    char direktive = '\0';  ///< leitet am Zeilenanfang eine Direktive ein; '\0' = keine
    std::string_view zeichenkettenBegrenzer = "\"";  ///< jedes Zeichen ist ein Begrenzer
    std::string_view zeichenBegrenzer = "'";
    /// Rohe Zeichenketten wie in C++: R"marke( ... )marke", auch mit Vorsatz (u8R, LR ...).
    bool rohZeichenketten = false;
    /// Trennt Ziffern in einer Zahl, etwa `'` in 1'000'000; '\0' = keiner.
    char ziffernTrenner = '\0';
};

struct LexerOptionen
{
    bool mitKommentaren = false;  ///< Kommentare als Token liefern statt uebergehen
};

/// Der Text als Token; das letzte ist immer `Ende`. Was nicht passt (ein
/// unbekanntes Zeichen, ein nicht geschlossenes Literal), steht in `meldungen`.
[[nodiscard]] std::vector<Token> zerlege(std::string_view text, const Sprachtabelle& sprache,
                                         std::vector<quelle::Meldung>& meldungen,
                                         LexerOptionen optionen = {});

}  // namespace c2d::lexer
