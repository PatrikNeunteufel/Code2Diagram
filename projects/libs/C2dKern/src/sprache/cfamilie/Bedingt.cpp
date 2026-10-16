/**
 * @file Bedingt.cpp
 * @brief Bedingte Uebersetzung auswerten
 */
#include "Bedingt.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <map>
#include <optional>
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
    int zeile = 0;               ///< wo die Datei ihn definiert; 0 = von aussen oder von der Sprache
    bool vorgegeben = false;     ///< die Sprache definiert ihn (`__cplusplus`)
};

using Makros = std::map<std::string, Makro, std::less<>>;

/// Haelt fest, dass nach `name` gefragt wird — beim ersten Mal mit seinem Zustand dort.
void notiere(std::vector<Schalter>& schalter, const Makros& makros, std::string_view name, bool zahl)
{
    const auto s = std::find_if(schalter.begin(), schalter.end(), [&](const Schalter& x) { return x.name == name; });
    if (s != schalter.end())
    {
        s->zahl = s->zahl || zahl;
        return;
    }
    Schalter neu;
    neu.name = std::string(name);
    neu.zahl = zahl;
    if (const auto m = makros.find(name); m != makros.end())
    {
        neu.definiert = true;
        neu.wert = m->second.wert;
        neu.inDatei = m->second.zeile;
        neu.vorgegeben = m->second.vorgegeben;
    }
    schalter.push_back(std::move(neu));
}

/// Ein Operator, mit dem ein Name verglichen oder verrechnet wird (nicht ! && || ? :).
bool rechnet(const Token& t)
{
    static constexpr std::string_view kOps[] = {"|", "^", "&", "==", "!=", "<", ">", "<=", ">=",
                                                "<<", ">>", "+", "-", "*", "/", "%"};
    return t.art == TokenArt::Operator && std::find(std::begin(kOps), std::end(kOps), t.text) != std::end(kOps);
}

/// Haelt jeden Namen einer Bedingung fest, ohne sie auszurechnen (ein Zweig, nach dem niemand fragt).
void notiereAlle(std::vector<Schalter>& schalter, const Makros& makros, std::string_view wort, std::span<const Token> rest)
{
    for (std::size_t i = 0; i < rest.size(); ++i)
    {
        const Token& t = rest[i];
        if (t.art != TokenArt::Bezeichner || t.text == "defined") continue;
        if (i + 1 < rest.size() && rest[i + 1].istOp("(")) continue;  // ein Makro mit Parametern, __has_include
        const bool zahl = wort.find("def") == std::string_view::npos &&
                          ((i > 0 && rechnet(rest[i - 1])) || (i + 1 < rest.size() && rechnet(rest[i + 1])));
        notiere(schalter, makros, t.text, zahl);
    }
}

/// Die Regeln, nach denen der Wert eines Namens gesucht wird — und was dabei auffaellt.
struct Umfeld
{
    const lexer::Sprachtabelle& tabelle;
    const Makros& makros;
    std::vector<std::string>& gefragt;  ///< Namen, die nicht definiert waren
    std::vector<Schalter>* schalter = nullptr;  ///< wonach gefragt wird; nullptr = nicht festhalten
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
        if (m_u.schalter != nullptr && !istOp("("))
        {
            // verglichen oder verrechnet: ein Operator ausser ! && || ? : steht daneben
            const bool zahl = (m_p >= 2 && rechnet(m_t[m_p - 2])) || (!amEnde() && rechnet(m_t[m_p]));
            notiere(*m_u.schalter, m_u.makros, n, zahl);
        }
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
        if (m_u.schalter != nullptr) notiere(*m_u.schalter, m_u.makros, n, false);
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

/// Das Wort einer Direktive: `#  ifdef X` -> `ifdef`.
std::string_view wortVon(const Token& direktive)
{
    const std::string_view zeile = gestutzt(direktive.text.substr(1));
    std::size_t n = 0;
    while (n < zeile.size() && (zeile[n] == '_' || (zeile[n] >= 'a' && zeile[n] <= 'z'))) ++n;
    return zeile.substr(0, n);
}

bool beginntBedingung(std::string_view wort)
{
    return wort == "if" || wort == "ifdef" || wort == "ifndef";
}

bool setztBedingungFort(std::string_view wort)
{
    return wort == "elif" || wort == "elifdef" || wort == "elifndef" || wort == "else";
}

/// Die Bedingung eines Zweigs, wie ein Diagramm sie zeigt: `#ifdef A` -> `defined(A)`.
std::string bedingungstext(std::string_view wort, std::span<const Token> rest)
{
    const std::string_view erster = rest.size() > 1 ? rest[0].text : std::string_view();
    if (wort == "ifdef" || wort == "elifdef") return "defined(" + std::string(erster) + ")";
    if (wort == "ifndef" || wort == "elifndef") return "!defined(" + std::string(erster) + ")";
    if (wort == "else") return "else";
    std::string aus;
    for (const Token& t : rest)
    {
        if (t.art == TokenArt::Ende) break;
        if (!aus.empty() && t.abstandDavor) aus += ' ';
        aus += t.text;
    }
    return aus;
}

// ---------------------------------------------------------------------------
// Weichen: welcher Zweig gilt — und was sich aendern muesste, damit ein anderer gilt
// ---------------------------------------------------------------------------

/// Die Bedingung eines Zweigs zum Nachrechnen: ihr Wort und ihre Token (sie zeigen in den Text).
struct Bedingung
{
    std::string_view wort;
    std::vector<Token> token;  ///< hinter dem Wort, mit dem Ende
};

/// Was beim Auswerten ueber eine Weiche festgehalten wird, um danach ihre Zweige zu waehlen.
struct Stand
{
    Makros makros;  ///< wie sie am #if galten
    std::vector<Bedingung> bedingungen;
};

/// Ergibt die Bedingung mit diesen Makros wahr? nullopt = laesst sich nicht ausrechnen.
std::optional<bool> rechneNach(const Bedingung& b, const Makros& makros, const lexer::Sprachtabelle& tabelle)
{
    if (b.wort == "else") return true;
    const std::string_view erster = b.token.size() > 1 ? b.token[0].text : std::string_view();
    if (b.wort == "ifdef" || b.wort == "elifdef") return makros.find(erster) != makros.end();
    if (b.wort == "ifndef" || b.wort == "elifndef") return makros.find(erster) == makros.end();
    std::vector<std::string> egal;
    const Umfeld umfeld{tabelle, makros, egal};
    long long wert = 0;
    if (!Rechner(b.token, umfeld, 0).rechne(wert)) return std::nullopt;
    return wert != 0;
}

/// Unter welcher Forderung ergibt eine einfache Bedingung `soll`: `defined(X)`,
/// `!defined(X)`, `X`, `!X`, `X == n`, `X != n` (auch geklammert, auch #ifdef / #ifndef).
/// Soll `X == n` falsch sein, wird X entfernt (zaehlt dann 0). nullopt = keine einfache,
/// oder keine eindeutige (`X == 0` soll falsch sein).
std::optional<Forderung> forderungFuer(const Bedingung& b, bool soll)
{
    const std::string_view erster = b.token.size() > 1 ? b.token[0].text : std::string_view();
    if (b.wort == "ifdef" || b.wort == "elifdef" || b.wort == "ifndef" || b.wort == "elifndef")
    {
        if (erster.empty()) return std::nullopt;
        const bool verneint = b.wort == "ifndef" || b.wort == "elifndef";
        return Forderung{std::string(erster), soll != verneint, {}};
    }
    std::vector<const Token*> t;
    for (const Token& x : b.token)
        if (x.art != TokenArt::Ende) t.push_back(&x);
    // Klammern, die alles umschliessen
    const auto ohneKlammern = [&] {
        while (t.size() >= 2 && t.front()->istOp("(") && t.back()->istOp(")"))
        {
            int tiefe = 0;
            bool umschliesst = true;
            for (std::size_t i = 0; i + 1 < t.size(); ++i)
            {
                tiefe += t[i]->istOp("(") ? 1 : t[i]->istOp(")") ? -1 : 0;
                if (tiefe == 0) umschliesst = false;
            }
            if (!umschliesst) return;
            t = std::vector<const Token*>(t.begin() + 1, t.end() - 1);
        }
    };
    ohneKlammern();
    bool nicht = false;
    if (!t.empty() && t.front()->istOp("!"))
    {
        nicht = true;
        t.erase(t.begin());
        ohneKlammern();
    }
    const bool s = soll != nicht;
    const auto name = [](const Token* x) { return x->art == TokenArt::Bezeichner; };
    if (t.size() == 2 && t[0]->text == "defined" && name(t[1])) return Forderung{std::string(t[1]->text), s, {}};
    if (t.size() == 4 && t[0]->text == "defined" && t[1]->istOp("(") && name(t[2]) && t[3]->istOp(")"))
        return Forderung{std::string(t[2]->text), s, {}};
    if (t.size() == 1 && name(t[0]))
        return s ? Forderung{std::string(t[0]->text), true, "1"} : Forderung{std::string(t[0]->text), false, {}};
    if (!nicht && t.size() == 3 && (t[1]->istOp("==") || t[1]->istOp("!=")))
    {
        const Token* n = name(t[0]) && t[2]->art == TokenArt::Zahl ? t[0] : name(t[2]) && t[0]->art == TokenArt::Zahl ? t[2] : nullptr;
        if (n == nullptr) return std::nullopt;
        const Token* zahl = n == t[0] ? t[2] : t[0];
        if (t[1]->istOp("==") == soll) return Forderung{std::string(n->text), true, std::string(zahl->text)};
        // ungleich sein: nicht definiert zaehlt 0 — eindeutig, solange die Zahl nicht 0 ist
        if (zahl->text.find_first_not_of("0xXuUlL") == std::string_view::npos) return std::nullopt;
        return Forderung{std::string(n->text), false, {}};
    }
    return std::nullopt;
}

void wende(Makros& makros, const Forderung& f)
{
    if (!f.definiert)
    {
        makros.erase(f.name);
        return;
    }
    if (f.wert.empty() && makros.find(f.name) != makros.end()) return;
    makros[f.name] = {f.wert, false, 0, false};
}

/// Fuer eine Bedingung mit genau einem Namen (`X >= 2`, `X < 3 || X > 7`): die erste
/// Forderung, unter der sie nachgerechnet `soll` ergibt — X entfernen, die Zahlen der
/// Bedingung, je eins daneben, 1. nullopt = keine, oder mehr als ein Name.
std::optional<Forderung> probiere(const Bedingung& b, bool soll, const Makros& makros, const lexer::Sprachtabelle& tabelle)
{
    std::string_view name;
    std::vector<std::string> kandidaten;
    for (std::size_t i = 0; i < b.token.size(); ++i)
    {
        const Token& t = b.token[i];
        if (t.art == TokenArt::Zahl)
        {
            kandidaten.emplace_back(t.text);
            char* ende = nullptr;
            const std::string zahl(t.text);
            const long long n = std::strtoll(zahl.c_str(), &ende, 0);
            if (ende != zahl.c_str())
            {
                kandidaten.push_back(std::to_string(n + 1));
                kandidaten.push_back(std::to_string(n - 1));
            }
        }
        if (t.art != TokenArt::Bezeichner || t.text == "defined") continue;
        if (i + 1 < b.token.size() && b.token[i + 1].istOp("(")) return std::nullopt;  // ein Makro mit Parametern
        if (!name.empty() && name != t.text) return std::nullopt;                       // mehr als ein Name
        name = t.text;
    }
    if (name.empty()) return std::nullopt;
    kandidaten.emplace_back("1");
    std::vector<Forderung> versuche{{std::string(name), false, {}}};
    for (const std::string& k : kandidaten) versuche.push_back({std::string(name), true, k});
    for (const Forderung& f : versuche)
    {
        Makros probe = makros;
        wende(probe, f);
        if (const std::optional<bool> ist = rechneNach(b, probe, tabelle); ist && *ist == soll) return f;
    }
    return std::nullopt;
}

/// Was sich aendern muss, damit Zweig `zweig` der Weiche `w` gilt: die Forderungen
/// fuer ihren Zweig in der umgebenden Weiche, dann je Bedingung davor "falsch" und
/// fuer ihn "wahr" — nachgerechnet, bis alles stimmt (hoechstens vier Runden; stimmt
/// es dann nicht, widersprechen sich die Bedingungen: nullopt).
std::optional<std::vector<Forderung>> waehle(const std::vector<Weiche>& weichen, const std::vector<Stand>& staende,
                                             std::size_t w, std::size_t zweig, const lexer::Sprachtabelle& tabelle)
{
    const Weiche& weiche = weichen[w];
    std::vector<Forderung> f;
    if (weiche.eltern != kKeinZweig)
    {
        const auto& oben = weichen[weiche.eltern].wahl;
        if (weiche.elternZweig >= oben.size() || !oben[weiche.elternZweig]) return std::nullopt;
        f = *oben[weiche.elternZweig];
    }
    const Stand& stand = staende[w];
    Makros makros = stand.makros;
    for (const Forderung& x : f) wende(makros, x);
    for (int runde = 0; runde < 4; ++runde)
    {
        bool stimmt = true;
        for (std::size_t i = 0; i < stand.bedingungen.size() && i <= zweig; ++i)
        {
            const Bedingung& b = stand.bedingungen[i];
            if (b.wort == "else") continue;
            const bool soll = i == zweig;
            if (const std::optional<bool> ist = rechneNach(b, makros, tabelle); ist && *ist == soll) continue;
            stimmt = false;
            std::optional<Forderung> neu = forderungFuer(b, soll);
            if (!neu) neu = probiere(b, soll, makros, tabelle);
            if (!neu) return std::nullopt;
            // was die Datei oder die Sprache definiert, aendert keine Definition von aussen
            if (const auto alt = stand.makros.find(neu->name);
                alt != stand.makros.end() && (alt->second.zeile > 0 || alt->second.vorgegeben))
                return std::nullopt;
            // eine Forderung an denselben Namen ersetzt die fruehere; die naechste Runde rechnet alles nach
            const auto gleich = std::find_if(f.begin(), f.end(), [&](const Forderung& x) { return x.name == neu->name; });
            if (gleich == f.end())
                f.push_back(*neu);
            else
                *gleich = *neu;
            wende(makros, *neu);
        }
        if (stimmt) return f;
    }
    return std::nullopt;
}

/// Ein #if, gesehen vor dem Auswerten: liegt es im Rumpf, enthalten seine Zweige ganze Anweisungen?
struct Pruefung
{
    bool imRumpf = false;  ///< das #if steht im Rumpf einer Funktion
    bool varianten = false;
};

/// Prueft jedes #if ... #endif auf die Bedingung fuer Varianten (Bedingt.hpp);
/// der Schluessel ist der Versatz des #if. Ein #if ohne #endif fehlt.
std::map<std::size_t, Pruefung> pruefeVarianten(std::span<const Token> token,
                                                const std::vector<std::pair<std::size_t, std::size_t>>& ruempfe)
{
    struct Gruppe
    {
        std::vector<std::size_t> direktiven;  ///< Stellen in `token`
        bool sonst = false;
        bool gut = true;
        std::size_t eltern = kKeinZweig;
    };
    const auto imRumpf = [&](std::size_t von, std::size_t bis) {
        return std::any_of(ruempfe.begin(), ruempfe.end(),
                           [&](const auto& r) { return r.first <= von && bis < r.second; });
    };
    // enthaelt jeder Zweig ganze Anweisungen?
    const auto ganz = [&](const Gruppe& g) {
        for (std::size_t z = 0; z + 1 < g.direktiven.size(); ++z)
        {
            int tiefe = 0;
            const Token* erstes = nullptr;
            const Token* letztes = nullptr;
            for (std::size_t i = g.direktiven[z] + 1; i < g.direktiven[z + 1]; ++i)
            {
                const Token& t = token[i];
                if (t.art == TokenArt::Direktive) continue;
                if (erstes == nullptr) erstes = &t;
                letztes = &t;
                if (t.istOp("{") || t.istOp("(") || t.istOp("[")) ++tiefe;
                if ((t.istOp("}") || t.istOp(")") || t.istOp("]")) && --tiefe < 0) return false;
                if (tiefe == 0 && (t.istWort("case") || t.istWort("default"))) return false;
            }
            if (tiefe != 0) return false;
            if (erstes != nullptr && erstes->istWort("else")) return false;
            if (letztes != nullptr && !letztes->istOp(";") && !letztes->istOp("}")) return false;
        }
        // davor endet eine Anweisung: sonst waere die Gruppe etwa der Rumpf eines if ohne Klammern
        std::size_t davor = g.direktiven.front();
        while (davor > 0 && token[davor - 1].art == TokenArt::Direktive) --davor;
        if (davor > 0)
        {
            const Token& t = token[davor - 1];
            if (!t.istOp(";") && !t.istOp("{") && !t.istOp("}") && !t.istOp(":")) return false;
        }
        std::size_t danach = g.direktiven.back() + 1;
        while (danach < token.size() && token[danach].art == TokenArt::Direktive) ++danach;
        return danach >= token.size() || !token[danach].istWort("else");
    };

    std::map<std::size_t, Pruefung> aus;
    std::vector<Gruppe> gruppen;
    std::vector<std::size_t> offen;
    for (std::size_t i = 0; i < token.size(); ++i)
    {
        if (token[i].art != TokenArt::Direktive) continue;
        const std::string_view wort = wortVon(token[i]);
        if (beginntBedingung(wort))
        {
            gruppen.push_back({{i}, false, true, offen.empty() ? kKeinZweig : offen.back()});
            offen.push_back(gruppen.size() - 1);
        }
        else if (setztBedingungFort(wort) && !offen.empty())
        {
            Gruppe& g = gruppen[offen.back()];
            if (g.sonst) g.gut = false;  // nach dem #else
            g.direktiven.push_back(i);
            g.sonst = g.sonst || wort == "else";
        }
        else if (wort == "endif" && !offen.empty())
        {
            const std::size_t n = offen.back();
            offen.pop_back();
            Gruppe& g = gruppen[n];
            g.direktiven.push_back(i);
            const std::size_t von = token[g.direktiven.front()].ort.versatz;
            const std::size_t bis = token[i].ort.versatz;
            Pruefung p;
            p.imRumpf = imRumpf(von, von);
            p.varianten = g.gut && imRumpf(von, bis) && ganz(g);
            // ein #if darin, das nicht als Varianten steht, laesst auch das umgebende nicht zu
            if (!p.varianten && g.eltern != kKeinZweig) gruppen[g.eltern].gut = false;
            aus[von] = p;
        }
    }
    return aus;
}

}  // namespace

std::string werteBedingteAus(std::string_view text, const lexer::Sprachtabelle& tabelle,
                             const std::vector<Definition>& definitionen, bool cpp,
                             std::vector<quelle::Meldung>& meldungen, std::vector<std::string>& unbekannt,
                             Varianten* varianten, std::vector<Schalter>* schalter, std::vector<Weiche>* weichen)
{
    std::vector<quelle::Meldung> egal;  // was der Lexer meldet, meldet er beim Lesen noch einmal
    const std::vector<Token> token = lexer::zerlege(text, tabelle, egal);
    const std::map<std::size_t, Pruefung> pruefung =
        varianten != nullptr ? pruefeVarianten(token, varianten->ruempfe) : std::map<std::size_t, Pruefung>();

    Makros makros;
    if (cpp) makros["__cplusplus"] = {"202002L", false, 0, true};
    for (const Definition& d : definitionen)
        if (!d.name.empty()) makros[d.name] = {d.wert, false, 0, false};
    std::set<std::string, std::less<>> jeDefiniert;  // irgendwo in der Datei, auch in einem Zweig, der nicht gilt
    std::vector<std::string> gefragt;
    const Umfeld umfeld{tabelle, makros, gefragt, schalter};
    std::vector<Stand> staende;  // je Weiche, zum Nachrechnen

    // Ein Waechter: `#ifndef K`, und die naechste Direktive ist `#define K`.
    const auto istWaechter = [&](std::size_t i, std::string_view name) {
        for (std::size_t j = i + 1; j < token.size(); ++j)
        {
            if (token[j].art != TokenArt::Direktive) continue;
            if (wortVon(token[j]) != "define") return false;
            std::string_view rest = gestutzt(gestutzt(token[j].text.substr(1)).substr(6));
            std::size_t n = 0;
            while (n < rest.size() && (rest[n] == '_' || std::isalnum(static_cast<unsigned char>(rest[n])) != 0)) ++n;
            return rest.substr(0, n) == name;
        }
        return false;
    };

    std::string aus(text);
    const auto blende = [&](std::size_t von, std::size_t bis) {
        for (std::size_t i = von; i < bis && i < aus.size(); ++i)
            if (aus[i] != '\n' && aus[i] != '\r') aus[i] = ' ';
    };

    // Zwei Fragen je Stelle: gilt sie (fuer #define und welcher Zweig gilt), und
    // sieht der Leser sie? Ohne Varianten ist beides dasselbe; in einem Zweig von
    // Varianten, der nicht gilt, sieht er, was nicht gilt.
    struct Ebene
    {
        bool elternGelten;
        bool genommen;  ///< ein Zweig dieser Bedingung galt schon
        bool gilt;
        bool sonst;  ///< das #else war schon da
        quelle::Position ort;
        bool elternZeigen;
        bool gezeigt;  ///< ein Zweig dieser Bedingung wurde schon gezeigt
        bool zeigt;
        std::size_t gruppe;  ///< Stelle in varianten->gruppen; kKeinZweig = keine Varianten
        std::size_t weiche = kKeinZweig;  ///< Stelle in *weichen; kKeinZweig = keine (ein Waechter)
        std::size_t zweig = 0;            ///< der wievielte Zweig gerade gelesen wird
    };
    std::vector<Ebene> stapel;
    const auto gilt = [&] { return stapel.empty() || stapel.back().gilt; };
    const auto zeigt = [&] { return stapel.empty() || stapel.back().zeigt; };

    std::size_t ab = 0;  // hier beginnt das Stueck hinter der letzten Direktive
    for (std::size_t ti = 0; ti < token.size(); ++ti)
    {
        const Token& t = token[ti];
        if (t.art != TokenArt::Direktive) continue;
        const std::size_t anfang = t.ort.versatz;
        const std::size_t ende = anfang + t.text.size();
        const bool galt = gilt();
        const bool sah = zeigt();
        if (!sah) blende(ab, anfang);
        ab = ende;

        // der Inhalt der Direktive als Token: das Wort, dann der Rest
        std::vector<quelle::Meldung> stumm;
        const std::vector<Token> inhalt = lexer::zerlege(t.text.substr(1), tabelle, stumm);
        const std::string_view wort = inhalt.size() > 1 ? inhalt[0].text : std::string_view();
        const std::span<const Token> rest = std::span(inhalt).subspan(std::min<std::size_t>(1, inhalt.size()));
        const std::string_view erster = rest.size() > 1 ? rest[0].text : std::string_view();
        const bool waechter = wort == "ifndef" && !erster.empty() && istWaechter(ti, erster);

        // Gilt die Bedingung dieser Direktive? Laesst sie sich nicht ausrechnen, wird ihr Zweig gelesen.
        const auto bedingung = [&]() -> bool {
            if (wort == "ifdef" || wort == "elifdef" || wort == "ifndef" || wort == "elifndef")
            {
                if (schalter != nullptr && !waechter && !erster.empty()) notiere(*schalter, makros, erster, false);
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
        bool bleibt = false;  // eine Direktive von Varianten: der Leser sieht sie
        if (beginntBedingung(wort))
        {
            std::size_t gruppe = kKeinZweig;
            if (const auto p = pruefung.find(anfang); sah && p != pruefung.end())
            {
                if (p->second.varianten)
                {
                    gruppe = varianten->gruppen.size();
                    varianten->gruppen.push_back({{anfang}, {bedingungstext(wort, rest)}, kKeinZweig});
                }
                else if (p->second.imRumpf)
                    meldungen.push_back({quelle::Schwere::Hinweis, t.ort,
                                         "#if not shown in the flow: a branch does not hold whole statements"});
            }
            const bool wert = (galt || sah) && bedingung();
            if (!(galt || sah) && schalter != nullptr && !waechter) notiereAlle(*schalter, makros, wort, rest);
            const bool wahr = galt && wert;
            const bool alleZeigen = gruppe != kKeinZweig && !varianten->nurGeltend;
            const bool zeig = alleZeigen ? sah : sah && wert;
            if (gruppe != kKeinZweig && wahr) varianten->gruppen[gruppe].geltend = 0;
            // die Weiche: in welchem Zweig welcher Weiche sie steht, und der Stand zum Nachrechnen
            std::size_t weiche = kKeinZweig;
            if (weichen != nullptr && !waechter)
            {
                Weiche neu;
                neu.zeile = t.ort.zeile;
                for (auto e = stapel.rbegin(); e != stapel.rend(); ++e)
                    if (e->weiche != kKeinZweig)
                    {
                        neu.eltern = e->weiche;
                        neu.elternZweig = e->zweig;
                        break;
                    }
                neu.zweige.push_back(bedingungstext(wort, rest));
                if (wahr) neu.geltend = 0;
                weiche = weichen->size();
                weichen->push_back(std::move(neu));
                staende.push_back({makros, {Bedingung{wort, std::vector<Token>(rest.begin(), rest.end())}}});
            }
            stapel.push_back({galt, wahr, wahr, false, t.ort, sah, zeig, zeig, gruppe, weiche, 0});
            bleibt = gruppe != kKeinZweig;
        }
        else if (setztBedingungFort(wort))
        {
            if (stapel.empty())
                meldungen.push_back({quelle::Schwere::Fehler, t.ort, "#" + std::string(wort) + " without #if"});
            else
            {
                Ebene& e = stapel.back();
                if (e.sonst) meldungen.push_back({quelle::Schwere::Fehler, t.ort, "#" + std::string(wort) + " after #else"});
                const bool varianteHier = e.gruppe != kKeinZweig;
                const bool alleZeigen = varianteHier && !varianten->nurGeltend;
                const bool zuFragen = (e.elternGelten && !e.genommen) || (!alleZeigen && e.elternZeigen && !e.gezeigt);
                const bool wert = wort == "else" || (zuFragen && bedingung());
                if (!zuFragen && wort != "else" && schalter != nullptr) notiereAlle(*schalter, makros, wort, rest);
                e.gilt = e.elternGelten && !e.genommen && wert;
                e.genommen = e.genommen || e.gilt;
                e.sonst = e.sonst || wort == "else";
                ++e.zweig;
                if (e.weiche != kKeinZweig)
                {
                    Weiche& w = (*weichen)[e.weiche];
                    w.zweige.push_back(bedingungstext(wort, rest));
                    if (e.gilt) w.geltend = w.zweige.size() - 1;
                    staende[e.weiche].bedingungen.push_back({wort, std::vector<Token>(rest.begin(), rest.end())});
                }
                if (varianteHier)
                {
                    Variantengruppe& g = varianten->gruppen[e.gruppe];
                    g.direktiven.push_back(anfang);
                    g.bedingungen.push_back(bedingungstext(wort, rest));
                    if (e.gilt) g.geltend = g.bedingungen.size() - 1;
                    bleibt = true;
                }
                if (alleZeigen)
                    e.zeigt = e.elternZeigen;
                else
                {
                    e.zeigt = e.elternZeigen && !e.gezeigt && wert;
                    e.gezeigt = e.gezeigt || e.zeigt;
                }
            }
        }
        else if (wort == "endif")
        {
            if (stapel.empty())
                meldungen.push_back({quelle::Schwere::Fehler, t.ort, "#endif without #if"});
            else
            {
                const Ebene& e = stapel.back();
                if (e.gruppe != kKeinZweig)
                {
                    Variantengruppe& g = varianten->gruppen[e.gruppe];
                    g.direktiven.push_back(anfang);
                    if (!e.sonst && e.elternGelten && !e.genommen) g.geltend = g.bedingungen.size();  // das fehlende #else
                    bleibt = true;
                }
                if (e.weiche != kKeinZweig && !e.sonst && e.elternGelten && !e.genommen)
                    (*weichen)[e.weiche].geltend = (*weichen)[e.weiche].zweige.size();
                stapel.pop_back();
            }
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
                    makros[std::string(erster)] = {std::string(gestutzt(t.text.substr(hinterName))), mitParametern,
                                                   t.ort.zeile, false};
                }
            }
            else if (wort == "undef" && galt)
            {
                if (const auto m = makros.find(erster); m != makros.end()) makros.erase(m);
            }
        }
        // die Direktiven der Bedingungen sieht der Leser nur bei Varianten; die uebrigen, wo er den Text sieht
        // (eine Gruppe von Varianten steht, wo der Leser ihr #if sah — dort sieht er alle ihre Direktiven,
        // auch wenn nur der geltende Zweig stehen bleibt)
        if (!bleibt && (steuert || !sah)) blende(anfang, ende);
    }
    if (!zeigt()) blende(ab, aus.size());
    for (const Ebene& e : stapel) meldungen.push_back({quelle::Schwere::Fehler, e.ort, "#if without #endif"});

    for (const std::string& name : gefragt)
        if (jeDefiniert.find(name) == jeDefiniert.end() &&
            std::find(unbekannt.begin(), unbekannt.end(), name) == unbekannt.end())
            unbekannt.push_back(name);

    // je Weiche und Zweig, was sich aendern muesste — die umgebenden stehen vorher, ihre Wahl ist schon da
    if (weichen != nullptr)
        for (std::size_t w = 0; w < weichen->size(); ++w)
        {
            Weiche& weiche = (*weichen)[w];
            const std::size_t zweige = weiche.zweige.size() + (weiche.zweige.back() == "else" ? 0 : 1);
            for (std::size_t z = 0; z < zweige; ++z) weiche.wahl.push_back(waehle(*weichen, staende, w, z, tabelle));
        }
    return aus;
}

}  // namespace c2d::sprache::cfamilie
