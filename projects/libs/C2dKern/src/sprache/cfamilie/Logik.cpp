/**
 * @file Logik.cpp
 * @brief Bedingungen zerlegen: Oder vor Und vor Nicht, Klammern; der Rest ist ein Atom
 */
#include "Logik.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace c2d::sprache::cfamilie
{

namespace
{

using lexer::Token;
using lexer::TokenArt;
using modell::Logik;

bool istOder(const Token& t)
{
    return t.istOp("||") || t.istWort("or");
}

bool istUnd(const Token& t)
{
    return t.istOp("&&") || t.istWort("and");
}

bool istNicht(const Token& t)
{
    return t.istOp("!") || t.istWort("not");
}

bool oeffnet(const Token& t)
{
    return t.istOp("(") || t.istOp("[") || t.istOp("{");
}

bool schliesst(const Token& t)
{
    return t.istOp(")") || t.istOp("]") || t.istOp("}");
}

class Zerleger
{
public:
    Zerleger(std::string_view text, const std::vector<Token>& t) : m_text(text), m_t(t) {}

    /// [von, bis): Oder, Und, Nicht oder ein Atom.
    Logik oder(std::size_t von, std::size_t bis) const
    {
        const std::vector<std::size_t> stellen = trennstellen(von, bis, istOder);
        if (stellen.empty()) return und(von, bis);
        return verbinde(Logik::Art::Oder, von, bis, stellen, [this](std::size_t a, std::size_t b) { return und(a, b); });
    }

private:
    Logik und(std::size_t von, std::size_t bis) const
    {
        const std::vector<std::size_t> stellen = trennstellen(von, bis, istUnd);
        if (stellen.empty()) return nicht(von, bis);
        return verbinde(Logik::Art::Und, von, bis, stellen, [this](std::size_t a, std::size_t b) { return nicht(a, b); });
    }

    Logik nicht(std::size_t von, std::size_t bis) const
    {
        if (von < bis && istNicht(m_t[von]) && einzeln(von + 1, bis))
        {
            Logik n;
            n.art = Logik::Art::Nicht;
            n.teile.push_back(nicht(von + 1, bis));
            return n;
        }
        // eine Klammer um alles: ihr Inhalt
        if (bis - von >= 2 && m_t[von].istOp("(") && passend(von) == bis - 1) return oder(von + 1, bis - 1);
        return atom(von, bis);
    }

    Logik atom(std::size_t von, std::size_t bis) const
    {
        Logik a;
        a.art = Logik::Art::Atom;
        if (von >= bis) return a;
        const std::size_t anfang = static_cast<std::size_t>(m_t[von].text.data() - m_text.data());
        const Token& letztes = m_t[bis - 1];
        const std::size_t ende = static_cast<std::size_t>(letztes.text.data() - m_text.data()) + letztes.text.size();
        a.text = std::string(m_text.substr(anfang, ende - anfang));
        // ein Aufruf: ein Name vor `(` (nicht sizeof)
        for (std::size_t i = von; i + 1 < bis; ++i)
            if (m_t[i].art == TokenArt::Bezeichner && m_t[i + 1].istOp("(")) a.mitAufruf = true;
        return a;
    }

    /// Die Stelle der schliessenden Klammer zu der bei `auf`.
    [[nodiscard]] std::size_t passend(std::size_t auf) const
    {
        int tiefe = 0;
        for (std::size_t i = auf; i < m_t.size(); ++i)
        {
            if (oeffnet(m_t[i])) ++tiefe;
            if (schliesst(m_t[i]) && --tiefe == 0) return i;
        }
        return m_t.size();
    }

    /// Steht in [von, bis) ein Einzelnes — ein Name mit Gliedern, Aufrufen, Indizes, oder eine Klammer?
    /// Dann bindet ein `!` davor an das Ganze; sonst (`!x == y`) ist es Teil eines Vergleichs.
    [[nodiscard]] bool einzeln(std::size_t von, std::size_t bis) const
    {
        if (von >= bis) return false;
        int tiefe = 0;
        for (std::size_t i = von; i < bis; ++i)
        {
            const Token& t = m_t[i];
            if (oeffnet(t)) ++tiefe;
            else if (schliesst(t)) --tiefe;
            else if (tiefe == 0 && t.art == TokenArt::Operator && !t.istOp(".") && !t.istOp("->") && !t.istOp("::") &&
                     !istNicht(t))
                return false;
        }
        return true;
    }

    /// Die Stellen der Operatoren `ist` auf oberster Ebene; leer, wenn ein `?` dort steht (dann ist alles ein Atom).
    template <typename Pruefung>
    [[nodiscard]] std::vector<std::size_t> trennstellen(std::size_t von, std::size_t bis, Pruefung ist) const
    {
        std::vector<std::size_t> aus;
        int tiefe = 0;
        for (std::size_t i = von; i < bis; ++i)
        {
            const Token& t = m_t[i];
            if (oeffnet(t)) ++tiefe;
            else if (schliesst(t)) --tiefe;
            else if (tiefe == 0 && t.istOp("?")) return {};
            else if (tiefe == 0 && ist(t)) aus.push_back(i);
        }
        return aus;
    }

    template <typename Teil>
    Logik verbinde(Logik::Art art, std::size_t von, std::size_t bis, const std::vector<std::size_t>& stellen, Teil teil) const
    {
        Logik l;
        l.art = art;
        std::size_t a = von;
        for (const std::size_t s : stellen)
        {
            l.teile.push_back(teil(a, s));
            a = s + 1;
        }
        l.teile.push_back(teil(a, bis));
        return l;
    }

    std::string_view m_text;
    const std::vector<Token>& m_t;
};

void sammleAtome(const Logik& l, std::set<std::string>& atome)
{
    if (l.art == Logik::Art::Atom)
    {
        std::string ohneLeer;
        for (const char c : l.text)
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') ohneLeer += c;
        atome.insert(ohneLeer);
    }
    for (const Logik& t : l.teile) sammleAtome(t, atome);
}

/// Welcher Teil einer Anweisung (oder eines return) eine Tabelle bekommt: die Bedingung eines
/// `?:` auf oberster Ebene, sonst die rechte Seite einer Zuweisung mit `&&`/`||`; leer = keiner.
std::string_view kandidat(std::string_view text, const lexer::Sprachtabelle& tabelle)
{
    std::vector<quelle::Meldung> stumm;
    const std::vector<Token> t = lexer::zerlege(text, tabelle, stumm);
    const auto stelle = [&](const Token& tok) { return static_cast<std::size_t>(tok.text.data() - text.data()); };
    int tiefe = 0;
    std::size_t nachZuweisung = 0;  // Token hinter dem letzten `=` auf oberster Ebene
    bool logisch = false;
    for (std::size_t i = 0; i < t.size() && t[i].art != TokenArt::Ende; ++i)
    {
        const Token& tok = t[i];
        if (oeffnet(tok)) ++tiefe;
        else if (schliesst(tok)) --tiefe;
        else if (tiefe == 0 && tok.art == TokenArt::Operator &&
                 (tok.text == "=" || tok.text == "|=" || tok.text == "&=" || tok.text == "^="))
            nachZuweisung = i + 1;
        else if (tiefe == 0 && tok.istOp("?"))
        {
            // die Bedingung des ?: — vom Ende der Zuweisung bis zum `?`
            if (nachZuweisung >= i) return {};
            const std::size_t a = stelle(t[nachZuweisung]);
            return text.substr(a, stelle(tok) - a);
        }
        if (istUnd(tok) || istOder(tok)) logisch = true;
    }
    if (!logisch || nachZuweisung >= t.size() || t[nachZuweisung].art == TokenArt::Ende) return {};
    return text.substr(stelle(t[nachZuweisung]));
}

/// Die Bedingung eines `?:` auf oberster Ebene — vom Ende einer Zuweisung bis zum `?`; leer = keins.
std::string_view frageIn(std::string_view text, const lexer::Sprachtabelle& tabelle)
{
    std::vector<quelle::Meldung> stumm;
    const std::vector<Token> t = lexer::zerlege(text, tabelle, stumm);
    const auto stelle = [&](const Token& tok) { return static_cast<std::size_t>(tok.text.data() - text.data()); };
    int tiefe = 0;
    std::size_t nachZuweisung = 0;
    for (std::size_t i = 0; i < t.size() && t[i].art != TokenArt::Ende; ++i)
    {
        const Token& tok = t[i];
        if (oeffnet(tok)) ++tiefe;
        else if (schliesst(tok)) --tiefe;
        else if (tiefe == 0 && tok.art == TokenArt::Operator &&
                 (tok.text == "=" || tok.text == "|=" || tok.text == "&=" || tok.text == "^="))
            nachZuweisung = i + 1;
        else if (tiefe == 0 && tok.istOp("?"))
        {
            if (nachZuweisung >= i) return {};
            const std::size_t a = stelle(t[nachZuweisung]);
            std::string_view bedingung = text.substr(a, stelle(tok) - a);
            while (!bedingung.empty() && bedingung.back() == ' ') bedingung.remove_suffix(1);
            return bedingung;
        }
    }
    return {};
}

/// Ruft der Ausdruck eine Funktion — ein Name vor `(`?
bool ruft(std::string_view text, const lexer::Sprachtabelle& tabelle)
{
    std::vector<quelle::Meldung> stumm;
    const std::vector<Token> t = lexer::zerlege(text, tabelle, stumm);
    for (std::size_t i = 0; i + 1 < t.size(); ++i)
        if (t[i].art == TokenArt::Bezeichner && t[i + 1].istOp("(")) return true;
    return false;
}

/// Die Namen eines Ausdrucks (jeder einmal, in der Reihenfolge des Texts).
std::vector<std::string> namenIn(std::string_view text, const lexer::Sprachtabelle& tabelle)
{
    std::vector<quelle::Meldung> stumm;
    std::vector<std::string> aus;
    for (const Token& t : lexer::zerlege(text, tabelle, stumm))
        if (t.art == TokenArt::Bezeichner && std::find(aus.begin(), aus.end(), t.text) == aus.end()) aus.emplace_back(t.text);
    return aus;
}

bool istZuweisung(const Token& t)
{
    static constexpr std::string_view kZuweisungen[] = {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="};
    return t.art == TokenArt::Operator && std::find(std::begin(kZuweisungen), std::end(kZuweisungen), t.text) != std::end(kZuweisungen);
}

/// Was eine Anweisung schreibt: links einer Zuweisung (der erste Name; mit `*` davor oder `->` darin
/// durch einen Zeiger), neben `++`/`--`, hinter einem `&`, das eine Adresse nimmt; und ob sie ruft.
void wirkung(modell::Anweisung& a, const lexer::Sprachtabelle& tabelle)
{
    std::vector<quelle::Meldung> stumm;
    const std::vector<Token> t = lexer::zerlege(a.text, tabelle, stumm);
    const auto schreibe = [&](std::string_view name) {
        if (std::find(a.schreibt.begin(), a.schreibt.end(), name) == a.schreibt.end()) a.schreibt.emplace_back(name);
    };
    std::size_t anfang = 0;  // wo der laufende Teilausdruck beginnt (nach `(`, `,`, `;` oder einer Zuweisung)
    for (std::size_t i = 0; i < t.size() && t[i].art != TokenArt::Ende; ++i)
    {
        const Token& tok = t[i];
        if (tok.art == TokenArt::Bezeichner && i + 1 < t.size() && t[i + 1].istOp("(")) a.ruft = true;
        if (istZuweisung(tok))
        {
            // jeder Name links gilt als geschrieben (bei `a[i] = …` auch i): lieber einmal zu oft neu
            // fragen als eine Regel weglassen. `*p = …` und `p->x = …` schreiben durch einen Zeiger.
            for (std::size_t j = anfang; j < i; ++j)
            {
                if ((j == anfang && t[j].istOp("*")) || t[j].istOp("->")) a.schreibtDurchZeiger = true;
                if (t[j].art == TokenArt::Bezeichner) schreibe(t[j].text);
            }
            anfang = i + 1;
            continue;
        }
        if (tok.istOp("(") || tok.istOp(",") || tok.istOp(";")) anfang = i + 1;
        if ((tok.istOp("++") || tok.istOp("--")))
        {
            if (i > 0 && t[i - 1].art == TokenArt::Bezeichner) schreibe(t[i - 1].text);
            if (i + 1 < t.size() && t[i + 1].art == TokenArt::Bezeichner) schreibe(t[i + 1].text);
        }
        // `&name` als Adresse: am Anfang eines Teilausdrucks (nicht `a & b`)
        if (tok.istOp("&") && i + 1 < t.size() && t[i + 1].art == TokenArt::Bezeichner &&
            (i == 0 || t[i - 1].istOp("(") || t[i - 1].istOp(",") || istZuweisung(t[i - 1])))
            schreibe(t[i + 1].text);
    }
}

void ergaenze(modell::Anweisung& a, const lexer::Sprachtabelle& tabelle)
{
    using modell::AnweisungArt;
    if (a.art == AnweisungArt::Einfach || a.art == AnweisungArt::Ruecksprung || a.art == AnweisungArt::Wurf) wirkung(a, tabelle);
    if (a.art == AnweisungArt::Wenn || a.art == AnweisungArt::Solange || a.art == AnweisungArt::TueSolange ||
        a.art == AnweisungArt::Fuer || a.art == AnweisungArt::Auswahl)
        a.bedingungNamen = namenIn(a.text, tabelle);
    if (a.art == AnweisungArt::Einfach || a.art == AnweisungArt::Ruecksprung)
    {
        a.frage = std::string(frageIn(a.text, tabelle));
        a.bedingungRuft = !a.frage.empty() && ruft(a.frage, tabelle);
        if (!a.frage.empty()) a.bedingungNamen = namenIn(a.frage, tabelle);
    }
    if (a.art == AnweisungArt::Wenn || a.art == AnweisungArt::Solange || a.art == AnweisungArt::TueSolange ||
        a.art == AnweisungArt::Fuer)
        a.bedingungRuft = ruft(a.text, tabelle);
    std::string_view ausdruck;
    switch (a.art)
    {
        case AnweisungArt::Wenn:
        case AnweisungArt::Solange:
        case AnweisungArt::TueSolange:
        case AnweisungArt::Fuer: ausdruck = a.text; break;
        case AnweisungArt::Einfach:
        case AnweisungArt::Ruecksprung: ausdruck = kandidat(a.text, tabelle); break;
        default: break;
    }
    while (!ausdruck.empty() && (ausdruck.back() == ' ' || ausdruck.back() == ';')) ausdruck.remove_suffix(1);
    while (!ausdruck.empty() && ausdruck.front() == ' ') ausdruck.remove_prefix(1);
    if (!ausdruck.empty())
        if (std::optional<modell::Logik> l = logikVon(ausdruck, tabelle))
        {
            a.logikText = std::string(ausdruck);
            a.logik = std::move(l);
        }
    for (modell::AnweisungPtr& k : a.kinder) ergaenze(*k, tabelle);
    if (a.sonst) ergaenze(*a.sonst, tabelle);
}

}  // namespace

std::optional<modell::Logik> logikVon(std::string_view ausdruck, const lexer::Sprachtabelle& tabelle)
{
    std::vector<quelle::Meldung> stumm;
    const std::vector<Token> t = lexer::zerlege(ausdruck, tabelle, stumm);
    std::size_t bis = 0;
    while (bis < t.size() && t[bis].art != TokenArt::Ende) ++bis;
    if (bis == 0) return std::nullopt;
    Logik l = Zerleger(ausdruck, t).oder(0, bis);
    std::set<std::string> atome;
    sammleAtome(l, atome);
    if (atome.size() < 2) return std::nullopt;
    return l;
}

void ergaenzeLogik(modell::Einheit& einheit, const lexer::Sprachtabelle& tabelle)
{
    for (modell::Funktion& f : einheit.funktionen)
        if (f.rumpf) ergaenze(*f.rumpf, tabelle);
}

}  // namespace c2d::sprache::cfamilie
