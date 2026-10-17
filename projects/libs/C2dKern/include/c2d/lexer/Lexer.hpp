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
    /// Nur mit Sprachtabelle::zeilenEnde: das Ende einer Zeile mit Inhalt, ausserhalb von
    /// Klammern — es beendet eine Anweisung. Der Text ist leer und steht am Zeilenwechsel.
    Zeilenende,
    /// Nur mit Sprachtabelle::einrueckung: die Zeile ist tiefer eingerueckt als die vorige
    /// (ein Block beginnt) bzw. flacher (je verlassener Stufe ein Auszug). Der Text ist
    /// leer und steht am ersten Token der Zeile (am Ende des Texts: an dessen Ende).
    Einzug,
    Auszug,
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
    std::string_view zeilenKommentar{};  ///< leer = keiner
    std::string_view blockKommentarAuf{};
    std::string_view blockKommentarZu{};
    char direktive = '\0';  ///< leitet am Zeilenanfang eine Direktive ein; '\0' = keine
    std::string_view zeichenkettenBegrenzer = "\"";  ///< jedes Zeichen ist ein Begrenzer
    std::string_view zeichenBegrenzer = "'";
    /// Rohe Zeichenketten wie in C++: R"marke( ... )marke", auch mit Vorsatz (u8R, LR ...).
    bool rohZeichenketten = false;
    /// Trennt Ziffern in einer Zahl, etwa `'` in 1'000'000; '\0' = keiner.
    char ziffernTrenner = '\0';
    /// Vorsaetze, die unmittelbar vor einer Zeichenkette zu ihr gehoeren (Python: `r`, `b`,
    /// `f`, `rb` …), klein geschrieben; gross geschrieben gelten sie ebenso.
    std::span<const std::string_view> zeichenkettenVorsaetze{};
    /// Drei Begrenzer oeffnen eine Zeichenkette, die bis zu drei gleichen reicht, auch ueber
    /// Zeilen (Python: `"""…"""`, `'''…'''`).
    bool dreifachZeichenketten = false;
    /// Ein Zeilenwechsel ausserhalb von Klammern beendet die Anweisung: Token Zeilenende,
    /// einmal je Zeile mit Inhalt (eine Zeile mit nur Leerraum oder Kommentar gibt keins;
    /// `\` am Zeilenende setzt fort). Am Ende des Texts steht eins, wenn die letzte Zeile
    /// Inhalt hat.
    bool zeilenEnde = false;
    /// Nur mit zeilenEnde: die Einrueckung einer Zeile gliedert in Bloecke (Token Einzug,
    /// Auszug); ein Tabulator rueckt bis zur naechsten Achterstelle. Rueckt eine Zeile auf
    /// keine fruehere Stufe zurueck, gibt es eine Meldung. Am Ende des Texts schliessen
    /// Auszuege jede offene Stufe.
    bool einrueckung = false;
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
