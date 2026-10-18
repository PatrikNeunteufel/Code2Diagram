/**
 * @file Leser.cpp
 * @brief Basis der Leser
 */
#include "Leser.hpp"

#include <algorithm>

namespace c2d::sprache
{

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
