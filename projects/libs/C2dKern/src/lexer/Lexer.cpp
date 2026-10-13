/**
 * @file Lexer.cpp
 * @brief Der allgemeine Lexer
 */
#include "c2d/lexer/Lexer.hpp"

#include <algorithm>
#include <string>

namespace c2d::lexer
{

namespace
{

bool istZiffer(char c)
{
    return c >= '0' && c <= '9';
}

bool istBezeichnerStart(char c)
{
    // Bytes ab 0x80 gehoeren zu UTF-8-Zeichen: in Bezeichnern erlaubt
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
           static_cast<unsigned char>(c) >= 0x80;
}

bool istBezeichnerTeil(char c)
{
    return istBezeichnerStart(c) || istZiffer(c);
}

bool enthaelt(std::span<const std::string_view> liste, std::string_view wort)
{
    return std::find(liste.begin(), liste.end(), wort) != liste.end();
}

}  // namespace

std::vector<Token> zerlege(std::string_view text, const Sprachtabelle& sprache,
                           std::vector<quelle::Meldung>& meldungen, LexerOptionen optionen)
{
    std::vector<Token> aus;
    std::size_t i = 0;
    int zeile = 1;
    int spalte = 1;
    bool zeilenanfang = true;
    bool abstand = false;

    const auto vor = [&](std::size_t n) {
        for (std::size_t k = 0; k < n && i < text.size(); ++k, ++i)
        {
            if (text[i] == '\n')
            {
                ++zeile;
                spalte = 1;
            }
            else
                ++spalte;
        }
    };
    const auto beginnt = [&](std::string_view s) {
        return !s.empty() && text.substr(i, s.size()) == s;
    };
    const auto zeilenFortsetzung = [&]() -> std::size_t {  // Laenge von \ + Zeilenende, sonst 0
        if (text[i] != '\\') return 0;
        if (text.substr(i + 1, 1) == "\n") return 2;
        if (text.substr(i + 1, 2) == "\r\n") return 3;
        return 0;
    };
    /// Beginnt bei i eine rohe Zeichenkette — [u8|u|U|L] R " marke ( ... ) marke " —,
    /// ihre Laenge; sonst 0. Fehlt der Schluss, reicht sie bis ans Ende des Texts.
    const auto rohLaenge = [&]() -> std::size_t {
        if (!sprache.rohZeichenketten) return 0;
        std::size_t r = i;
        if (text.substr(r, 2) == "u8") r += 2;
        else if (text[r] == 'u' || text[r] == 'U' || text[r] == 'L') r += 1;
        if (text.substr(r, 2) != "R\"") return 0;
        // nicht mitten in einem Namen: `FOOR"..."` ist keine rohe Zeichenkette
        if (i > 0 && istBezeichnerTeil(text[i - 1])) return 0;
        const std::size_t auf = text.find('(', r + 2);
        if (auf == std::string_view::npos || auf - (r + 2) > 16) return 0;
        const std::string_view marke = text.substr(r + 2, auf - (r + 2));
        if (marke.find_first_of(" \\)\"\n") != std::string_view::npos) return 0;
        const std::string schluss = ")" + std::string(marke) + "\"";
        const std::size_t zu = text.find(schluss, auf + 1);
        return (zu == std::string_view::npos ? text.size() : zu + schluss.size()) - i;
    };
    const auto melde = [&](quelle::Schwere schwere, const quelle::Position& ort, std::string was) {
        meldungen.push_back({schwere, ort, std::move(was)});
    };
    const auto gib = [&](TokenArt art, const quelle::Position& ort) {
        aus.push_back({art, text.substr(ort.versatz, i - ort.versatz), ort, abstand});
        abstand = false;
        zeilenanfang = false;
    };

    while (i < text.size())
    {
        const char c = text[i];
        if (c == '\n')
        {
            vor(1);
            zeilenanfang = true;
            abstand = true;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v')
        {
            vor(1);
            abstand = true;
            continue;
        }
        if (const std::size_t n = zeilenFortsetzung(); n != 0)
        {
            vor(n);
            abstand = true;
            continue;
        }

        const quelle::Position ort{i, zeile, spalte};

        if (beginnt(sprache.zeilenKommentar) || beginnt(sprache.blockKommentarAuf))
        {
            if (beginnt(sprache.zeilenKommentar))
            {
                while (i < text.size() && text[i] != '\n') vor(1);
            }
            else
            {
                vor(sprache.blockKommentarAuf.size());
                while (i < text.size() && !beginnt(sprache.blockKommentarZu)) vor(1);
                if (i >= text.size())
                    melde(quelle::Schwere::Fehler, ort, "comment is not closed");
                else
                    vor(sprache.blockKommentarZu.size());
            }
            // ein Kommentar aendert nicht, ob die Zeile noch am Anfang steht
            if (optionen.mitKommentaren)
            {
                const bool warAnfang = zeilenanfang;
                gib(TokenArt::Kommentar, ort);
                zeilenanfang = warAnfang;
            }
            abstand = true;
            continue;
        }

        if (sprache.direktive != '\0' && c == sprache.direktive && zeilenanfang)
        {
            while (i < text.size() && text[i] != '\n')
            {
                // eine rohe Zeichenkette in einem #define laeuft ueber die Zeile hinaus
                if (const std::size_t roh = rohLaenge(); roh != 0)
                {
                    vor(roh);
                    continue;
                }
                const std::size_t n = zeilenFortsetzung();
                vor(n != 0 ? n : 1);
            }
            std::size_t ende = i;  // ein \r vor dem Zeilenende gehoert nicht dazu
            while (ende > ort.versatz && text[ende - 1] == '\r') --ende;
            aus.push_back({TokenArt::Direktive, text.substr(ort.versatz, ende - ort.versatz), ort, abstand});
            abstand = false;
            continue;
        }

        if (const std::size_t roh = rohLaenge(); roh != 0)
        {
            if (i + roh >= text.size() && !text.ends_with('"'))
                melde(quelle::Schwere::Fehler, ort, "raw string is not closed");
            vor(roh);
            gib(TokenArt::Zeichenkette, ort);
            continue;
        }

        if (istBezeichnerStart(c))
        {
            while (i < text.size() && istBezeichnerTeil(text[i])) vor(1);
            const std::string_view wort = text.substr(ort.versatz, i - ort.versatz);
            gib(enthaelt(sprache.schluesselwoerter, wort) ? TokenArt::Schluesselwort : TokenArt::Bezeichner,
                ort);
            continue;
        }

        if (istZiffer(c) || (c == '.' && i + 1 < text.size() && istZiffer(text[i + 1])))
        {
            // wie die pp-number in C: Ziffern, Buchstaben, Punkte, Vorzeichen nach e E p P
            vor(1);
            while (i < text.size())
            {
                const char d = text[i];
                const char davor = text[i - 1];
                const bool vorzeichen = (d == '+' || d == '-') &&
                                        (davor == 'e' || davor == 'E' || davor == 'p' || davor == 'P');
                const bool trenner = sprache.ziffernTrenner != '\0' && d == sprache.ziffernTrenner &&
                                     i + 1 < text.size() && istBezeichnerTeil(text[i + 1]);
                if (!istBezeichnerTeil(d) && d != '.' && !vorzeichen && !trenner) break;
                vor(1);
            }
            gib(TokenArt::Zahl, ort);
            continue;
        }

        const bool kette = sprache.zeichenkettenBegrenzer.find(c) != std::string_view::npos;
        if (kette || sprache.zeichenBegrenzer.find(c) != std::string_view::npos)
        {
            bool zu = false;
            vor(1);
            while (i < text.size() && text[i] != '\n')
            {
                if (text[i] == '\\' && i + 1 < text.size())
                {
                    const std::size_t n = zeilenFortsetzung();
                    vor(n != 0 ? n : 2);
                    continue;
                }
                const bool ende = text[i] == c;
                vor(1);
                if (ende)
                {
                    zu = true;
                    break;
                }
            }
            if (!zu) melde(quelle::Schwere::Fehler, ort, "literal is not closed");
            gib(kette ? TokenArt::Zeichenkette : TokenArt::Zeichen, ort);
            continue;
        }

        std::size_t laenge = 0;
        for (const std::string_view op : sprache.operatoren)
            if (op.size() > laenge && beginnt(op)) laenge = op.size();
        if (laenge != 0)
        {
            vor(laenge);
            gib(TokenArt::Operator, ort);
            continue;
        }

        vor(1);
        melde(quelle::Schwere::Warnung, ort,
              "unknown character '" + std::string(text.substr(ort.versatz, 1)) + "'");
        gib(TokenArt::Unbekannt, ort);
    }

    aus.push_back({TokenArt::Ende, text.substr(text.size()), {i, zeile, spalte}, abstand});
    return aus;
}

}  // namespace c2d::lexer
