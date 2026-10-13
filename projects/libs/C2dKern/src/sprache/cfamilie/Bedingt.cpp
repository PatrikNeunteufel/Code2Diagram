/**
 * @file Bedingt.cpp
 * @brief Bedingte Uebersetzung auswerten
 */
#include "Bedingt.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace c2d::sprache::cfamilie
{

namespace
{

using lexer::Token;
using lexer::TokenArt;

struct Makro
{
    std::string wert;            ///< der Text hinter dem Namen
    bool mitParametern = false;  ///< `#define F(x) ...`: laesst sich hier nicht ausrechnen
};

using Makros = std::map<std::string, Makro, std::less<>>;

/// Die Regeln, nach denen der Wert eines Namens gesucht wird — und was dabei auffaellt.
struct Umfeld
{
    const lexer::Sprachtabelle& tabelle;
    const Makros& makros;
    std::vector<std::string>& gefragt;  ///< Namen, die nicht definiert waren
};

/// Rechnet einen Ausdruck aus #if aus: ganze Zahlen, wie der Praeprozessor.
class Rechner
{
public:
    Rechner(std::span<const Token> token, const Umfeld& umfeld, int tiefe) : m_t(token), m_u(umfeld), m_tiefe(tiefe) {}

    /// false = der Ausdruck laesst sich nicht ausrechnen (dann ist `wert` ohne Bedeutung).
    bool rechne(long long& wert)
    {
        wert = bedingt();
        return m_gut && amEnde();
    }

private:
    [[nodiscard]] bool amEnde() const { return m_p >= m_t.size() || m_t[m_p].art == TokenArt::Ende; }
    [[nodiscard]] bool istOp(std::string_view op) const { return !amEnde() && m_t[m_p].istOp(op); }
    bool nimm(std::string_view op)
    {
        if (!istOp(op)) return false;
        ++m_p;
        return true;
    }

    long long bedingt()
    {
        const long long frage = zweistellig(1);
        if (!nimm("?")) return frage;
        const long long ja = bedingt();
        if (!nimm(":")) m_gut = false;
        const long long nein = bedingt();
        return frage != 0 ? ja : nein;
    }

    /// Der Rang eines zweistelligen Operators; 0 = keiner.
    [[nodiscard]] static int rang(std::string_view op)
    {
        static constexpr std::pair<std::string_view, int> kRaenge[] = {
            {"||", 1}, {"&&", 2}, {"|", 3},  {"^", 4},  {"&", 5},  {"==", 6}, {"!=", 6}, {"<", 7}, {">", 7},
            {"<=", 7}, {">=", 7}, {"<<", 8}, {">>", 8}, {"+", 9},  {"-", 9},  {"*", 10}, {"/", 10}, {"%", 10}};
        for (const auto& [text, r] : kRaenge)
            if (text == op) return r;
        return 0;
    }

    long long zweistellig(int mindestens)
    {
        long long links = einstellig();
        for (;;)
        {
            if (amEnde() || m_t[m_p].art != TokenArt::Operator) return links;
            const std::string_view op = m_t[m_p].text;
            const int r = rang(op);
            if (r == 0 || r < mindestens) return links;
            ++m_p;
            const long long rechts = zweistellig(r + 1);
            if (op == "||") links = (links != 0 || rechts != 0) ? 1 : 0;
            else if (op == "&&") links = (links != 0 && rechts != 0) ? 1 : 0;
            else if (op == "|") links |= rechts;
            else if (op == "^") links ^= rechts;
            else if (op == "&") links &= rechts;
            else if (op == "==") links = links == rechts;
            else if (op == "!=") links = links != rechts;
            else if (op == "<") links = links < rechts;
            else if (op == ">") links = links > rechts;
            else if (op == "<=") links = links <= rechts;
            else if (op == ">=") links = links >= rechts;
            else if (op == "<<") links = rechts >= 0 && rechts < 63 ? links << rechts : 0;
            else if (op == ">>") links = rechts >= 0 && rechts < 63 ? links >> rechts : 0;
            else if (op == "+") links += rechts;
            else if (op == "-") links -= rechts;
            else if (op == "*") links *= rechts;
            else if (rechts == 0) links = 0;  // durch 0: der Compiler meldet einen Fehler, hier zaehlt es 0
            else if (op == "/") links /= rechts;
            else links %= rechts;
        }
    }

    long long einstellig()
    {
        if (nimm("!")) return einstellig() == 0 ? 1 : 0;
        if (nimm("~")) return ~einstellig();
        if (nimm("-")) return -einstellig();
        if (nimm("+")) return einstellig();
        if (nimm("("))
        {
            const long long wert = bedingt();
            if (!nimm(")")) m_gut = false;
            return wert;
        }
        if (amEnde())
        {
            m_gut = false;
            return 0;
        }
        const Token& t = m_t[m_p++];
        if (t.art == TokenArt::Zahl) return zahl(t.text);
        if (t.art == TokenArt::Zeichen) return t.text.size() >= 3 && t.text[1] != '\\' ? static_cast<unsigned char>(t.text[1]) : 0;
        if (t.art == TokenArt::Bezeichner || t.art == TokenArt::Schluesselwort) return name(t.text);
        m_gut = false;
        return 0;
    }

    long long name(std::string_view n)
    {
        if (n == "true") return 1;
        if (n == "false") return 0;
        if (n == "defined")
        {
            const bool klammer = nimm("(");
            if (amEnde())
            {
                m_gut = false;
                return 0;
            }
            const std::string_view gefragt = m_t[m_p++].text;
            if (klammer && !nimm(")")) m_gut = false;
            return istDefiniert(gefragt) ? 1 : 0;
        }
        const auto makro = m_u.makros.find(n);
        if (istOp("("))
        {
            // der Aufruf eines Makros mit Parametern oder __has_include(...): hier nicht ausrechenbar
            int tiefe = 0;
            do
            {
                if (istOp("(")) ++tiefe;
                if (istOp(")")) --tiefe;
                ++m_p;
            } while (!amEnde() && tiefe > 0);
            m_gut = false;
            return 0;
        }
        if (makro == m_u.makros.end())
        {
            merke(n);
            return 0;  // wie im Compiler: ein Name, der nicht definiert ist, zaehlt 0
        }
        if (makro->second.mitParametern || m_tiefe > 16)
        {
            m_gut = false;
            return 0;
        }
        // der Wert des Makros ist selbst ein Ausdruck
        std::vector<quelle::Meldung> egal;
        const std::vector<Token> token = lexer::zerlege(makro->second.wert, m_u.tabelle, egal);
        if (token.size() <= 1) return 0;  // `#define X` ohne Wert
        long long wert = 0;
        if (!Rechner(token, m_u, m_tiefe + 1).rechne(wert)) m_gut = false;
        return wert;
    }

    bool istDefiniert(std::string_view n)
    {
        if (m_u.makros.find(n) != m_u.makros.end()) return true;
        merke(n);
        return false;
    }

    void merke(std::string_view n)
    {
        if (std::find(m_u.gefragt.begin(), m_u.gefragt.end(), n) == m_u.gefragt.end()) m_u.gefragt.emplace_back(n);
    }

    /// 42, 0x2A, 0b101010, 052, mit Anhang (u, l, ul ...) und Zifferntrennern.
    long long zahl(std::string_view text)
    {
        int basis = 10;
        std::size_t i = 0;
        if (text.size() > 1 && text[0] == '0')
        {
            if (text[1] == 'x' || text[1] == 'X') basis = 16, i = 2;
            else if (text[1] == 'b' || text[1] == 'B') basis = 2, i = 2;
            else basis = 8, i = 1;
        }
        long long wert = 0;
        for (; i < text.size(); ++i)
        {
            const char c = text[i];
            if (c == '\'') continue;
            int ziffer = -1;
            if (c >= '0' && c <= '9') ziffer = c - '0';
            else if (c >= 'a' && c <= 'f') ziffer = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') ziffer = c - 'A' + 10;
            if (ziffer < 0 || ziffer >= basis)
            {
                // danach darf nur noch der Anhang stehen; eine Gleitkommazahl gehoert nicht in ein #if
                const std::string_view rest = text.substr(i);
                if (rest.find_first_not_of("uUlLzZ") != std::string_view::npos) m_gut = false;
                break;
            }
            wert = wert * basis + ziffer;
        }
        return wert;
    }

    std::span<const Token> m_t;
    const Umfeld& m_u;
    int m_tiefe;
    std::size_t m_p = 0;
    bool m_gut = true;
};

std::string_view gestutzt(std::string_view s)
{
    constexpr std::string_view kLeer = " \t\r\n";
    const std::size_t a = s.find_first_not_of(kLeer);
    if (a == std::string_view::npos) return {};
    return s.substr(a, s.find_last_not_of(kLeer) - a + 1);
}

}  // namespace

std::string werteBedingteAus(std::string_view text, const lexer::Sprachtabelle& tabelle,
                             const std::vector<Definition>& definitionen, bool cpp,
                             std::vector<quelle::Meldung>& meldungen, std::vector<std::string>& unbekannt)
{
    std::vector<quelle::Meldung> egal;  // was der Lexer meldet, meldet er beim Lesen noch einmal
    const std::vector<Token> token = lexer::zerlege(text, tabelle, egal);

    Makros makros;
    if (cpp) makros["__cplusplus"] = {"202002L", false};
    for (const Definition& d : definitionen)
        if (!d.name.empty()) makros[d.name] = {d.wert, false};
    std::set<std::string, std::less<>> jeDefiniert;  // irgendwo in der Datei, auch in einem Zweig, der nicht gilt
    std::vector<std::string> gefragt;
    const Umfeld umfeld{tabelle, makros, gefragt};

    std::string aus(text);
    const auto blende = [&](std::size_t von, std::size_t bis) {
        for (std::size_t i = von; i < bis && i < aus.size(); ++i)
            if (aus[i] != '\n' && aus[i] != '\r') aus[i] = ' ';
    };

    struct Ebene
    {
        bool elternGelten;
        bool genommen;  ///< ein Zweig dieser Bedingung galt schon
        bool gilt;
        bool sonst;  ///< das #else war schon da
        quelle::Position ort;
    };
    std::vector<Ebene> stapel;
    const auto gilt = [&] { return stapel.empty() || stapel.back().gilt; };

    std::size_t ab = 0;  // hier beginnt das Stueck hinter der letzten Direktive
    for (const Token& t : token)
    {
        if (t.art != TokenArt::Direktive) continue;
        const std::size_t anfang = t.ort.versatz;
        const std::size_t ende = anfang + t.text.size();
        const bool galt = gilt();
        if (!galt) blende(ab, anfang);
        ab = ende;

        // der Inhalt der Direktive als Token: das Wort, dann der Rest
        std::vector<quelle::Meldung> stumm;
        const std::vector<Token> inhalt = lexer::zerlege(t.text.substr(1), tabelle, stumm);
        const std::string_view wort = inhalt.size() > 1 ? inhalt[0].text : std::string_view();
        const std::span<const Token> rest = std::span(inhalt).subspan(std::min<std::size_t>(1, inhalt.size()));
        const std::string_view erster = rest.size() > 1 ? rest[0].text : std::string_view();

        // Gilt die Bedingung dieser Direktive? Laesst sie sich nicht ausrechnen, wird ihr Zweig gelesen.
        const auto bedingung = [&]() -> bool {
            if (wort == "ifdef" || wort == "elifdef" || wort == "ifndef" || wort == "elifndef")
            {
                const bool definiert = makros.find(erster) != makros.end();
                const bool verneint = wort == "ifndef" || wort == "elifndef";
                // nach einem Waechter (#ifndef KOPF_H) fragt niemand: er wird gleich darauf definiert
                if (!definiert && !erster.empty() && std::find(gefragt.begin(), gefragt.end(), erster) == gefragt.end())
                    gefragt.emplace_back(erster);
                return definiert != verneint;
            }
            long long wert = 0;
            if (Rechner(rest, umfeld, 0).rechne(wert)) return wert != 0;
            meldungen.push_back({quelle::Schwere::Hinweis, t.ort,
                                 "condition not evaluated, its branch is read: " + std::string(gestutzt(t.text))});
            return true;
        };

        bool steuert = true;  // gehoert die Direktive zur bedingten Uebersetzung?
        if (wort == "if" || wort == "ifdef" || wort == "ifndef")
        {
            const bool wahr = galt && bedingung();
            stapel.push_back({galt, wahr, wahr, false, t.ort});
        }
        else if (wort == "elif" || wort == "elifdef" || wort == "elifndef" || wort == "else")
        {
            if (stapel.empty())
                meldungen.push_back({quelle::Schwere::Fehler, t.ort, "#" + std::string(wort) + " without #if"});
            else
            {
                Ebene& e = stapel.back();
                if (e.sonst) meldungen.push_back({quelle::Schwere::Fehler, t.ort, "#" + std::string(wort) + " after #else"});
                const bool wahr = e.elternGelten && !e.genommen && (wort == "else" || bedingung());
                e.gilt = wahr;
                e.genommen = e.genommen || wahr;
                e.sonst = e.sonst || wort == "else";
            }
        }
        else if (wort == "endif")
        {
            if (stapel.empty())
                meldungen.push_back({quelle::Schwere::Fehler, t.ort, "#endif without #if"});
            else
                stapel.pop_back();
        }
        else
        {
            steuert = false;
            if (wort == "define" && !erster.empty())
            {
                jeDefiniert.emplace(erster);
                if (galt)
                {
                    // `#define F(x)`: die Klammer folgt dem Namen ohne Abstand
                    const bool mitParametern = rest.size() > 2 && rest[1].istOp("(") && !rest[1].abstandDavor;
                    const std::size_t hinterName = static_cast<std::size_t>(rest[0].text.data() - t.text.data()) + erster.size();
                    makros[std::string(erster)] = {std::string(gestutzt(t.text.substr(hinterName))), mitParametern};
                }
            }
            else if (wort == "undef" && galt)
            {
                if (const auto m = makros.find(erster); m != makros.end()) makros.erase(m);
            }
        }
        // die Direktiven der Bedingungen sieht der Leser nie; die uebrigen nur, wo sie gelten
        if (steuert || !galt) blende(anfang, ende);
    }
    if (!gilt()) blende(ab, aus.size());
    for (const Ebene& e : stapel) meldungen.push_back({quelle::Schwere::Fehler, e.ort, "#if without #endif"});

    for (const std::string& name : gefragt)
        if (jeDefiniert.find(name) == jeDefiniert.end() &&
            std::find(unbekannt.begin(), unbekannt.end(), name) == unbekannt.end())
            unbekannt.push_back(name);
    return aus;
}

}  // namespace c2d::sprache::cfamilie
