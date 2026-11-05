/**
 * @file Leser.cpp
 * @brief Basis der Leser
 */
#include "Leser.hpp"

#include <algorithm>
#include <cctype>

namespace c2d::sprache
{

namespace
{

/// Der Inhalt eines Literals (TokenArt::Zeichenkette) ohne Vorsatz, Begrenzer und Escapes, je Byte mit
/// der Stelle in der Quelle; false = keine Zeichenkette, deren Inhalt sich so lesen laesst (ein
/// Regex-Literal). Eingebettete Ausdruecke (`f"{x}"`, `$"{x}"`, `${x}`, Kotlin `$x`) bleiben, wie sie
/// dastehen, und kommen als Platzhalter dazu.
bool inhaltVon(const lexer::Token& t, const lexer::Sprachtabelle& tabelle, Zeichenkette& aus)
{
    const std::string_view s = t.text;
    const auto begrenzer = [&](char c) {
        return tabelle.zeichenkettenBegrenzer.find(c) != std::string_view::npos ||
               (tabelle.vorlagenBegrenzer != '\0' && c == tabelle.vorlagenBegrenzer);
    };
    std::size_t k = 0;
    while (k < s.size() && !begrenzer(s[k])) ++k;
    if (k >= s.size()) return false;
    std::string vorsatz(s.substr(0, k));
    for (char& c : vorsatz) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    const char d = s[k];
    const auto nimm = [&](char c, std::size_t woher) {
        aus.inhalt += c;
        aus.versatz.push_back(t.ort.versatz + woher);
    };

    // C++: R"marke( … )marke" — ohne Escapes
    if (tabelle.rohZeichenketten && vorsatz.ends_with('r') && d == '"')
    {
        const std::size_t auf = s.find('(', k);
        if (auf == std::string_view::npos) return false;
        const std::size_t marke = auf - k - 1;
        const std::size_t ende = s.size() >= marke + 2 && s.size() > auf + marke + 2 ? s.size() - marke - 2 : s.size();
        for (std::size_t i = auf + 1; i < ende; ++i) nimm(s[i], i);
        return true;
    }

    const bool vorlage = tabelle.vorlagenBegrenzer != '\0' && d == tabelle.vorlagenBegrenzer;
    const bool roh = vorsatz.find('r') != std::string::npos;     // Python r"…"
    const bool woertlich = vorsatz.find('@') != std::string::npos;  // C# @"…"
    const bool klammern = vorsatz.find('f') != std::string::npos || vorsatz.find('$') != std::string::npos;
    const std::string drei(3, d);
    const std::size_t auf = tabelle.dreifachZeichenketten && s.substr(k, 3) == drei ? 3 : 1;
    std::size_t ende = s.size();
    if (ende >= k + 2 * auf && s.substr(ende - auf, auf) == std::string_view(drei).substr(0, auf)) ende -= auf;
    for (std::size_t i = k + auf; i < ende; ++i)
    {
        const char c = s[i];
        if (woertlich && c == '"' && i + 1 < ende && s[i + 1] == '"')
        {
            nimm('"', i++);
            continue;
        }
        if (!roh && !woertlich && c == '\\' && i + 1 < ende)
        {
            const char n = s[i + 1];
            if (n == '\n' || n == '\r')  // eine Fortsetzung ueber die Zeile
            {
                i += n == '\r' && i + 2 < ende && s[i + 2] == '\n' ? 2 : 1;
                continue;
            }
            nimm(n == 'n' ? '\n' : n == 't' ? '\t' : n == 'r' ? '\r' : n == '0' ? '\0' : n, i);
            ++i;
            continue;
        }
        // `${…}`; `$name` nur in Kotlin (dort ist die Vorlage eine gewoehnliche Zeichenkette), in JavaScript ist es Text
        const bool kotlinName = tabelle.zeichenkettenBegrenzer.find(d) != std::string_view::npos && i + 1 < ende &&
                                (s[i + 1] == '_' || std::isalpha(static_cast<unsigned char>(s[i + 1])));
        if ((klammern && c == '{') || (vorlage && c == '$' && i + 1 < ende && (s[i + 1] == '{' || kotlinName)))
        {
            if (klammern && i + 1 < ende && s[i + 1] == '{')
            {
                nimm('{', i++);  // `{{` ist eine `{`
                continue;
            }
            // der Ausdruck bis zu seiner schliessenden Klammer (bzw. das Ende des Namens), wie er dasteht
            const std::size_t anfang = aus.inhalt.size();
            if (c == '$' && s[i + 1] != '{')
            {
                nimm('$', i);
                ++i;
                for (; i < ende && (s[i] == '_' || std::isalnum(static_cast<unsigned char>(s[i]))); ++i) nimm(s[i], i);
            }
            else
            {
                if (c == '$')
                {
                    nimm('$', i);
                    ++i;
                }
                int tiefe = 0;
                for (; i < ende; ++i)
                {
                    nimm(s[i], i);
                    if (s[i] == '{') ++tiefe;
                    if (s[i] == '}' && --tiefe == 0)
                    {
                        ++i;
                        break;
                    }
                }
            }
            aus.platzhalter.push_back({anfang, aus.inhalt.size() - anfang});
            --i;  // die Schleife zaehlt weiter
            continue;
        }
        if (klammern && c == '}' && i + 1 < ende && s[i + 1] == '}')
        {
            nimm('}', i++);  // `}}` ist eine `}`
            continue;
        }
        nimm(c, i);
    }
    return true;
}

/// Wie eine Vorlage ausgefuellt wird — und so sehen ihre Platzhalter aus.
enum class Ausfuellen
{
    Keins,
    Qt,      ///< `.arg(`: `%1` … `%99`, auch `%L1`
    Printf,  ///< `%` (Python), `.formatted(` (Java): `%s`, `%d`, `%(name)s` …; `%%` ist ein `%`
    Klammer, ///< `.format(` (Python): `{}`, `{0}`, `{name}`; `{{` ist eine `{`
};

/// Methoden, die eine Zeichenkette als Vorlage ausfuellen: `QStringLiteral("… %1").arg(x)` (Qt),
/// `"… {}".format(x)` (Python), `"… %s".formatted(x)` (Java).
struct AusfuellMethode
{
    std::string_view name;
    Ausfuellen art;
};

constexpr AusfuellMethode kAusfuellen[] = {
    {"arg", Ausfuellen::Qt}, {"format", Ausfuellen::Klammer}, {"formatted", Ausfuellen::Printf}};

/// Wird die Zeichenkette, die vor Token j endet, ausgefuellt — `%` dahinter (Python) oder eine Methode aus
/// kAusfuellen, auch hinter den Klammern eines Makros darum (`QStringLiteral(…)`)?
Ausfuellen wirdAusgefuellt(const std::vector<lexer::Token>& t, std::size_t j)
{
    for (int klammern = 0; j < t.size() && klammern < 3 && t[j].istOp(")"); ++klammern) ++j;
    if (j < t.size() && t[j].istOp("%")) return Ausfuellen::Printf;
    if (j + 2 >= t.size() || !t[j].istOp(".") || !t[j + 2].istOp("(")) return Ausfuellen::Keins;
    for (const AusfuellMethode& m : kAusfuellen)
        if (t[j + 1].text == m.name) return m.art;
    return Ausfuellen::Keins;
}

/// Die Platzhalter einer Vorlage im Inhalt, nach der Art, wie sie ausgefuellt wird.
void findePlatzhalter(Zeichenkette& z, Ausfuellen art)
{
    const std::string& s = z.inhalt;
    const auto ziffer = [&](std::size_t i) { return i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])); };
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        std::size_t e = i;
        if (art == Ausfuellen::Qt && s[i] == '%')
        {
            e = i + 1 + (i + 1 < s.size() && s[i + 1] == 'L');
            if (!ziffer(e)) continue;
            while (ziffer(e)) ++e;
        }
        else if (art == Ausfuellen::Printf && s[i] == '%')
        {
            if (i + 1 < s.size() && s[i + 1] == '%')
            {
                ++i;
                continue;
            }
            e = i + 1;
            if (e < s.size() && s[e] == '(')
                while (e < s.size() && s[e] != ')') ++e;
            while (e < s.size() && std::string_view("()-+ #0.123456789").find(s[e]) != std::string_view::npos) ++e;
            if (e >= s.size() || std::string_view("sdifuxXeEgGcr").find(s[e]) == std::string_view::npos) continue;
            ++e;
        }
        else if (art == Ausfuellen::Klammer && s[i] == '{')
        {
            if (i + 1 < s.size() && s[i + 1] == '{')
            {
                ++i;
                continue;
            }
            e = s.find('}', i);
            if (e == std::string::npos) continue;
            ++e;
        }
        else
            continue;
        z.platzhalter.push_back({i, e - i});
        i = e - 1;
    }
}

/// Die Zeichenketten der Token: unmittelbar hintereinander stehende zu einer verbunden. Eine mit `+`
/// davor oder dahinter ist zusammengesetzt; eine, die ausgefuellt wird (wirdAusgefuellt), traegt ihre
/// Platzhalter.
std::vector<Zeichenkette> zeichenkettenIn(const std::vector<lexer::Token>& t, const lexer::Sprachtabelle& tabelle)
{
    std::vector<Zeichenkette> aus;
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        if (t[i].art != lexer::TokenArt::Zeichenkette) continue;
        Zeichenkette z;
        bool zusammen = i > 0 && t[i - 1].istOp("+");
        std::size_t j = i;
        bool gelesen = false;
        for (; j < t.size() && t[j].art == lexer::TokenArt::Zeichenkette; ++j)
            gelesen = inhaltVon(t[j], tabelle, z) || gelesen;
        if (j < t.size() && t[j].istOp("+")) zusammen = true;
        // (ein f-String, der noch einmal ausgefuellt wird, behaelt die Platzhalter seiner Ausdruecke)
        if (const Ausfuellen art = wirdAusgefuellt(t, j); art != Ausfuellen::Keins && z.platzhalter.empty())
            findePlatzhalter(z, art);
        i = j - 1;
        if (!gelesen) continue;
        z.zusammengesetzt = zusammen;
        aus.push_back(std::move(z));
    }
    return aus;
}

}  // namespace

bool Leser::oeffnet(const Token& t)
{
    return t.art == TokenArt::Operator && (t.text == "(" || t.text == "[" || t.text == "{");
}

bool Leser::schliesst(const Token& t)
{
    return t.art == TokenArt::Operator && (t.text == ")" || t.text == "]" || t.text == "}");
}

Leser::Leser(std::string_view text, const lexer::Sprachtabelle& tabelle, Ergebnis& aus)
    : m_aus(aus), m_t(lexer::zerlege(text, tabelle, aus.meldungen))
{
    m_aus.zeichenketten = zeichenkettenIn(m_t, tabelle);
}

bool Leser::nimm(std::string_view op)
{
    if (!akt().istOp(op)) return false;
    ++m_p;
    return true;
}

void Leser::melde(quelle::Schwere schwere, const Token& bei, std::string text)
{
    m_aus.meldungen.push_back({schwere, bei.ort, std::move(text)});
}

std::size_t Leser::nachKlammer(std::size_t i) const
{
    int tiefe = 0;
    for (; !amEnde(i); ++i)
    {
        if (oeffnet(m_t[i]))
            ++tiefe;
        else if (schliesst(m_t[i]) && --tiefe <= 0)
            return i + 1;
    }
    return i;
}

std::size_t Leser::ueberKlammer(std::size_t i)
{
    const std::size_t zu = nachKlammer(i);
    if (zu == i + 1 || !schliesst(tok(zu - 1)))
        melde(quelle::Schwere::Fehler, tok(i), "'" + std::string(tok(i).text) + "' is not closed");
    return zu;
}

void Leser::trenneZeilen()
{
    std::vector<Token> ohne;
    m_neueZeile.clear();
    bool neu = false;
    for (const Token& t : m_t)
    {
        if (t.art == TokenArt::Zeilenende)
        {
            neu = true;
            continue;
        }
        ohne.push_back(t);
        m_neueZeile.push_back(neu);
        neu = false;
    }
    m_t = std::move(ohne);
}

std::size_t Leser::suche(std::size_t i, std::initializer_list<std::string_view> stopps) const
{
    while (!amEnde(i))
    {
        const Token& t = m_t[i];
        if (t.art == TokenArt::Operator)
        {
            if (std::find(stopps.begin(), stopps.end(), t.text) != stopps.end()) return i;
            if (schliesst(t)) return i;
            if (oeffnet(t))
            {
                i = nachKlammer(i);
                continue;
            }
        }
        ++i;
    }
    return i;
}

std::vector<Leser::Bereich> Leser::teile(std::size_t a, std::size_t b) const
{
    std::vector<Bereich> stuecke;
    while (a < b)
    {
        const std::size_t komma = std::min(suche(a, {","}), b);
        if (komma > a) stuecke.emplace_back(a, komma);
        if (komma >= b || !m_t[komma].istOp(",")) break;
        a = komma + 1;
    }
    return stuecke;
}

std::string Leser::textVon(std::size_t a, std::size_t b) const
{
    std::string s;
    for (std::size_t i = a; i < b && !amEnde(i); ++i)
    {
        if (i > a && m_t[i].abstandDavor) s += ' ';
        s += m_t[i].text;
    }
    return s;
}

}  // namespace c2d::sprache
