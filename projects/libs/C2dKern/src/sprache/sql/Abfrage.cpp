/**
 * @file Abfrage.cpp
 * @brief Abfragen zerlegen (Entscheide Q1-Q5): WITH, Mengenoperationen, SELECT mit seinen Klauseln,
 *        die Quellen in FROM mit ihren Joins, Bedingungen an den aeussersten AND, Spaltenbezuege
 */
#include "Abfrage.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <iterator>
#include <span>

namespace c2d::sprache::sqlteile
{

namespace
{

using lexer::Token;
using lexer::TokenArt;
using namespace c2d::modell;

std::string klein(std::string_view text)
{
    std::string s(text);
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string gross(std::string_view text)
{
    std::string s(text);
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

/// Ein Name ohne seine Begrenzer: `"Album"` -> `Album`, `[dbo]` -> `dbo`, `` `x` `` -> `x`.
std::string ohneBegrenzer(std::string_view text)
{
    if (text.size() < 2) return std::string(text);
    const char auf = text.front();
    const char zu = auf == '[' ? ']' : auf;
    if ((auf != '"' && auf != '`' && auf != '[') || text.back() != zu) return std::string(text);
    std::string s;
    for (std::size_t i = 1; i + 1 < text.size(); ++i)
    {
        s += text[i];
        if (text[i] == zu && i + 2 < text.size() && text[i + 1] == zu) ++i;
    }
    return s;
}

bool enthaelt(std::span<const std::string_view> liste, std::string_view wort)
{
    return std::find(liste.begin(), liste.end(), wort) != liste.end();
}

/// Woerter, an denen ein Alias einer Quelle nicht steht (was nach ihr kommt).
constexpr std::string_view kNachQuelle[] = {
    "on",     "using",  "join",   "inner",   "left",    "right",  "full",    "cross",    "natural",
    "outer",  "where",  "group",  "order",   "limit",   "union",  "intersect", "except", "minus",
    "window", "having", "qualify", "fetch",  "offset",  "semi",   "anti",    "apply",    "lateral",
    "pivot",  "unpivot", "tablesample", "with", "for",  "into",   "set",     "when",     "then",
    "returning", "values", "select", "from",  "cluster", "distribute", "sort", "option", "as",
};

/// Woerter, mit denen hinter dem SELECT eine Klausel beginnt (auf der Stufe des Blocks).
constexpr std::string_view kKlauseln[] = {
    "from", "into", "where", "group", "having", "window", "qualify", "order", "limit",
    "offset", "fetch", "cluster", "distribute", "sort", "for", "option",
};

/// Woerter, die keine Spalte nennen, auch wo der Lexer sie nicht als Schluesselwort kennt.
constexpr std::string_view kKeineSpalte[] = {
    "true",      "false",     "unknown",  "current_date", "current_time", "current_timestamp", "localtime",
    "localtimestamp", "current_user", "session_user", "user", "sysdate", "systimestamp", "rownum",
    "nulls",     "first",     "last",     "rows",     "row",       "only",     "ties",     "over",
    "partition", "interval",  "date",     "time",     "timestamp", "both",     "leading",  "trailing",
    "escape",    "collate",   "within",   "filter",   "preceding", "following", "unbounded", "current",
    "range",     "groups",    "exclude",  "others",   "no",        "ilike",    "rlike",    "regexp",
    "similar",   "glob",      "match",    "against",  "mode",      "natural",  "lateral",  "array",
    "cast",      "convert",   "try_cast", "year",     "month",     "day",      "hour",     "minute",
    "second",    "epoch",     "dow",      "doy",      "week",      "quarter",  "outer",    "cross",
    "apply",     "offset",    "fetch",    "next",     "percent",   "top",      "distinct", "asc",
    "desc",      "any",       "some",     "of",       "zone",      "at",       "local",    "isnull",
    "notnull",   "elseif",    "elsif",    "rollup",   "cube",      "grouping", "sets",     "recursive",
    "materialized", "lateral", "tablesample", "repeatable", "system", "bernoulli", "returning",
};

/// Woerter, mit denen in einem Rumpf ohne `;` (T-SQL) am Anfang einer Zeile eine neue Anweisung beginnt.
constexpr std::string_view kNeueAnweisung[] = {
    "insert",  "update", "delete",  "merge",  "set",      "if",       "else",   "elsif",    "elseif",
    "end",     "begin",  "return",  "declare", "exec",    "execute",  "print",  "while",    "open",
    "fetch",   "close",  "commit",  "rollback", "raiserror", "throw",  "truncate", "drop",  "create",
    "alter",   "goto",   "break",   "continue", "waitfor", "call",    "perform", "raise",   "loop",
    "select",  "with",
};

}  // namespace

// --- Token ------------------------------------------------------------------

bool AbfrageLeser::wort(std::size_t i, std::string_view w) const
{
    const Token& t = tok(i);
    return (t.art == TokenArt::Schluesselwort || t.art == TokenArt::Bezeichner) && t.text.size() == w.size() &&
           klein(t.text) == w;
}

bool AbfrageLeser::istName(std::size_t i) const
{
    const TokenArt a = tok(i).art;
    return a == TokenArt::Bezeichner || a == TokenArt::Schluesselwort;
}

bool AbfrageLeser::istAuf(std::size_t i) const
{
    return tok(i).istOp("(");
}

bool AbfrageLeser::istZu(std::size_t i) const
{
    return tok(i).istOp(")");
}

std::size_t AbfrageLeser::ueber(std::size_t i, std::size_t b) const
{
    return std::min(m_q.hinterKlammer(i), b);
}

bool AbfrageLeser::abfrageInKlammer(std::size_t i) const
{
    if (!istAuf(i)) return false;
    if (wort(i + 1, "select") || wort(i + 1, "with")) return true;
    return istAuf(i + 1) && abfrageInKlammer(i + 1);
}

std::string AbfrageLeser::liesNamen(std::size_t i, std::size_t& ende) const
{
    std::string name;
    while (istName(i) || (tok(i).art == TokenArt::Bezeichner))
    {
        name += ohneBegrenzer(tok(i).text);
        if (!tok(i + 1).istOp(".") || !istName(i + 2)) break;
        name += '.';
        i += 2;
    }
    ende = name.empty() ? i : i + 1;
    return name;
}

std::size_t AbfrageLeser::neuerBlock(BlockRolle rolle, std::string titel, std::size_t eltern, std::size_t bei)
{
    AbfrageBlock b;
    b.rolle = rolle;
    b.titel = std::move(titel);
    b.eltern = eltern;
    b.zeile = tok(bei).ort.zeile;
    m_ab.bloecke.push_back(std::move(b));
    return m_ab.bloecke.size() - 1;
}

// --- Suchen -----------------------------------------------------------------

std::vector<Abfrage> AbfrageLeser::suche(std::size_t a, std::size_t b, const std::string& vorn)
{
    std::vector<Abfrage> aus;
    // Beginnt bei j eine Abfrage? Am Anfang, nach `;`, nach einem Wort, das einen Block oeffnet, oder am
    // Anfang einer Zeile, wenn die Zeile davor nicht fortgesetzt wird.
    const auto beginnt = [&](std::size_t j) {
        if (!(wort(j, "select") || wort(j, "perform") || wort(j, "with") || wort(j, "insert") || wort(j, "update") || wort(j, "delete") ||
              wort(j, "merge") || wort(j, "create") || wort(j, "alter")))
            return false;
        if (wort(j, "with") && !(istName(j + 1) && (wort(j + 2, "as") || istAuf(j + 2) || wort(j + 1, "recursive"))))
            return false;  // ein Hinweis `WITH (NOLOCK)`, kein WITH einer Abfrage
        if (j == a) return true;
        const Token& davor = tok(j - 1);
        if (davor.istOp(";")) return true;
        // auch `OPEN c FOR SELECT …`, `FOR r IN SELECT … LOOP` (PL/pgSQL, MySQL)
        static constexpr std::string_view kOeffnet[] = {"begin", "then", "else", "do", "loop", "as", "return", "for", "in"};
        if (std::any_of(std::begin(kOeffnet), std::end(kOeffnet), [&](std::string_view w) { return wort(j - 1, w); }))
            return true;
        return davor.ort.zeile < tok(j).ort.zeile && (davor.art != TokenArt::Operator || davor.istOp(")"));
    };
    // Das Ende der Abfrage ab j: `;`, das Ende, oder am Anfang einer Zeile ein Wort einer neuen Anweisung
    // (nicht in einer Klammer oder in CASE … END; SELECT und WITH nicht, solange die Anweisung noch eines
    // erwartet; in MERGE gehoeren UPDATE, INSERT, DELETE, SET zu den Zweigen)
    const auto ende = [&](std::size_t j) {
        const bool merge = wort(j, "merge");
        const bool update = wort(j, "update");
        // nach WITH, INSERT, CREATE … AS wartet die Anweisung noch auf ihr SELECT; sonst beginnt eines eine neue
        bool select = !(wort(j, "with") || wort(j, "insert") || wort(j, "create") || wort(j, "alter"));
        bool set = false;
        int fall = 0;
        for (std::size_t k = j + 1; k < b;)
        {
            if (istAuf(k))
            {
                k = ueber(k, b);
                continue;
            }
            if (tok(k).istOp(";")) return k;
            if (wort(k, "loop") && fall == 0) return k;  // `FOR r IN SELECT … LOOP`
            if (wort(k, "case")) ++fall;
            if (wort(k, "end") && fall > 0)
            {
                --fall;
                ++k;
                continue;
            }
            const bool zeilenanfang = tok(k - 1).ort.zeile < tok(k).ort.zeile;
            if (fall == 0 && zeilenanfang && istName(k) && enthaelt(kNeueAnweisung, klein(tok(k).text)))
            {
                const std::string w = klein(tok(k).text);
                bool gehoertDazu = false;
                if (w == "select" || w == "with")
                {
                    const bool fortgesetzt = wort(k - 1, "union") || wort(k - 1, "all") || wort(k - 1, "distinct") ||
                                             wort(k - 1, "intersect") || wort(k - 1, "except") || wort(k - 1, "minus") ||
                                             wort(k - 1, "as") || tok(k - 1).istOp(",");
                    gehoertDazu = fortgesetzt || !select;
                }
                else if (merge && (w == "update" || w == "insert" || w == "delete" || w == "set"))
                    gehoertDazu = true;
                else if (update && w == "set" && !set)
                    gehoertDazu = true;
                else if (w == "fetch" && (wort(k + 1, "first") || wort(k + 1, "next")))
                    gehoertDazu = true;
                if (!gehoertDazu) return k;
            }
            if (wort(k, "select")) select = true;
            if (wort(k, "set")) set = true;
            ++k;
        }
        return b;
    };
    for (std::size_t j = a; j < b;)
    {
        if (istAuf(j))
        {
            // eine Abfrage in Klammern in einer Anweisung des Rumpfs: `RETURN (SELECT …)`, `IF EXISTS (SELECT …)`
            const std::size_t zu = ueber(j, b);
            if (abfrageInKlammer(j) && j > a)
                if (std::optional<Abfrage> x = lies(j + 1, zu > j + 1 ? zu - 1 : zu, vorn + "@" + std::to_string(tok(j + 1).ort.zeile)))
                    aus.push_back(std::move(*x));
            j = zu;
            continue;
        }
        if (!beginnt(j))
        {
            ++j;
            continue;
        }
        const std::size_t e = ende(j);
        if (std::optional<Abfrage> x = lies(j, e, vorn + "@" + std::to_string(tok(j).ort.zeile))) aus.push_back(std::move(*x));
        j = std::max(e, j + 1);
    }
    return aus;
}

// --- Lesen ------------------------------------------------------------------

std::optional<Abfrage> AbfrageLeser::lies(std::size_t a, std::size_t b, const std::string& name)
{
    m_ab = {};
    m_ab.name = name;
    m_ab.zeile = tok(a).ort.zeile;
    m_ab.bisZeile = tok(b > a ? b - 1 : a).ort.zeile;
    std::size_t k = a;
    // die Stelle der ersten Klausel ab k (auf dieser Stufe), sonst b
    const auto bis = [&](std::size_t von, std::initializer_list<std::string_view> woerter) {
        for (std::size_t j = von; j < b;)
        {
            if (istAuf(j))
            {
                j = ueber(j, b);
                continue;
            }
            for (const std::string_view w : woerter)
                if (wort(j, w)) return j;
            ++j;
        }
        return b;
    };
    // eine Quelle `name [AS] alias` ab j; die Stelle dahinter
    const auto zielQuelle = [&](std::size_t j, AbfrageQuelle& q) {
        q.name = liesNamen(j, j);
        q.zeile = tok(j > a ? j - 1 : a).ort.zeile;
        if (wort(j, "as")) ++j;
        if (istName(j) && !enthaelt(kNachQuelle, klein(tok(j).text))) q.alias = ohneBegrenzer(tok(j++).text);
        return j;
    };

    if (wort(k, "insert"))
    {
        ++k;
        if (wort(k, "or")) k += 2;
        while (wort(k, "ignore") || wort(k, "low_priority") || wort(k, "delayed") || wort(k, "high_priority")) ++k;
        if (wort(k, "into") || wort(k, "overwrite")) ++k;
        if (wort(k, "table")) ++k;
        m_ab.ziel = liesNamen(k, k);
        if (wort(k, "partition") && istAuf(k + 1)) k = ueber(k + 1, b);
        if (istAuf(k) && !abfrageInKlammer(k)) k = ueber(k, b);  // die Spalten
        if (wort(k, "overriding")) k = bis(k, {"select", "with"});
        if (k >= b || !(wort(k, "select") || wort(k, "with") || abfrageInKlammer(k)) || m_ab.ziel.empty()) return std::nullopt;
        m_ab.schreibt = "insert into";
        liesBlock(k, b, BlockRolle::Haupt, "", kKeinBlock);
    }
    else if (wort(k, "create") || wort(k, "alter"))
    {
        // CREATE [OR REPLACE] [TEMP] [MATERIALIZED] TABLE|VIEW name [(…)] [Optionen] AS abfrage
        std::size_t g = k + 1;
        while (g < b && g < k + 10 && !wort(g, "table") && !wort(g, "view")) ++g;
        if (g >= b || !(wort(g, "table") || wort(g, "view"))) return std::nullopt;
        const bool view = wort(g, "view");
        std::size_t n = g + 1;
        if (wort(n, "if") && wort(n + 1, "not") && wort(n + 2, "exists")) n += 3;
        m_ab.ziel = liesNamen(n, n);
        const std::size_t as = bis(n, {"as"});
        if (as >= b || m_ab.ziel.empty()) return std::nullopt;
        std::size_t q = as + 1;
        if (!(wort(q, "select") || wort(q, "with") || abfrageInKlammer(q) || wort(q, "values"))) return std::nullopt;
        m_ab.schreibt = view ? "create view" : "create table";
        liesBlock(q, b, BlockRolle::Haupt, "", kKeinBlock);
    }
    else if (wort(k, "update"))
    {
        // UPDATE ziel [alias] SET … FROM quellen [WHERE …] — ohne FROM keine Abfrage
        AbfrageQuelle ziel;
        std::size_t j = zielQuelle(k + 1, ziel);
        const std::size_t set = bis(j, {"set"});
        const std::size_t from = bis(set, {"from"});
        if (set >= b || from >= b) return std::nullopt;
        const std::size_t wo = bis(from, {"where", "returning", "option"});
        const std::size_t block = neuerBlock(BlockRolle::Haupt, "", kKeinBlock, k);
        m_ab.ziel = ziel.name;
        m_ab.schreibt = "update";
        liesQuellen(block, from + 1, wo, "");
        // T-SQL `UPDATE x SET … FROM t x JOIN …`: das Ziel ist eine der Quellen
        std::vector<AbfrageQuelle>& qs = m_ab.bloecke[block].quellen;
        const bool schonDa = std::any_of(qs.begin(), qs.end(), [&](const AbfrageQuelle& x) {
            return klein(x.alias) == klein(ziel.name) || klein(x.name) == klein(ziel.name);
        });
        if (schonDa)
        {
            for (const AbfrageQuelle& x : qs)
                if (klein(x.alias) == klein(ziel.name)) m_ab.ziel = x.name;
        }
        else
        {
            ziel.zeile = tok(k).ort.zeile;
            qs.insert(qs.begin(), ziel);
            if (qs.size() > 1 && qs[1].verbindung.empty()) qs[1].verbindung = ",";
        }
        m_ab.bloecke[block].auswahl = "set " + ohneUnterabfragen(block, set + 1, from, BlockRolle::Auswahl, "select");
        sammleBezuege(set + 1, from, m_ab.bloecke[block].bezuege);
        if (wo < b && wort(wo, "where"))
        {
            const std::size_t weiter = bis(wo + 1, {"returning", "option"});
            m_ab.bloecke[block].wo = zerlege(block, wo + 1, weiter);
        }
    }
    else if (wort(k, "delete"))
    {
        // DELETE [FROM] ziel [alias] USING quellen (Postgres) | FROM quellen (T-SQL) [WHERE …]
        std::size_t j = k + 1;
        if (wort(j, "from")) ++j;
        AbfrageQuelle ziel;
        j = zielQuelle(j, ziel);
        const std::size_t von = bis(j, {"using", "from"});
        if (von >= b) return std::nullopt;
        const std::size_t wo = bis(von + 1, {"where", "returning", "option"});
        const std::size_t block = neuerBlock(BlockRolle::Haupt, "", kKeinBlock, k);
        m_ab.ziel = ziel.name;
        m_ab.schreibt = "delete";
        ziel.zeile = tok(k).ort.zeile;
        m_ab.bloecke[block].quellen.push_back(ziel);
        liesQuellen(block, von + 1, wo, ",");
        m_ab.bloecke[block].auswahl = "delete";
        if (wo < b && wort(wo, "where"))
            m_ab.bloecke[block].wo = zerlege(block, wo + 1, bis(wo + 1, {"returning", "option"}));
    }
    else if (wort(k, "merge"))
    {
        // MERGE INTO ziel [alias] USING quelle [alias] ON bedingung WHEN …
        std::size_t j = k + 1;
        if (wort(j, "into")) ++j;
        AbfrageQuelle ziel;
        j = zielQuelle(j, ziel);
        const std::size_t mit = bis(j, {"using"});
        if (mit >= b) return std::nullopt;
        const std::size_t block = neuerBlock(BlockRolle::Haupt, "", kKeinBlock, k);
        m_ab.ziel = ziel.name;
        m_ab.schreibt = "merge into";
        ziel.zeile = tok(k).ort.zeile;
        m_ab.bloecke[block].quellen.push_back(ziel);
        const std::size_t wann = bis(mit + 1, {"when"});
        liesQuellen(block, mit + 1, wann, "merge");
        if (wann < b) m_ab.bloecke[block].auswahl = m_q.quelltext(wann, b);
    }
    else if (wort(k, "select") || wort(k, "perform") || wort(k, "with") || abfrageInKlammer(k))
        liesBlock(k, b, BlockRolle::Haupt, "", kKeinBlock);
    else
        return std::nullopt;

    if (m_ab.bloecke.empty()) return std::nullopt;
    // ohne Quelle und ohne Ziel (SELECT 1, SELECT @x = …, SELECT f()) befragt sie nichts
    const bool quelle = std::any_of(m_ab.bloecke.begin(), m_ab.bloecke.end(),
                                    [](const AbfrageBlock& x) { return !x.quellen.empty(); });
    if (!quelle && m_ab.ziel.empty()) return std::nullopt;
    return std::move(m_ab);
}

std::size_t AbfrageLeser::liesBlock(std::size_t a, std::size_t b, BlockRolle rolle, std::string titel, std::size_t eltern)
{
    // aeussere Klammern um die ganze Abfrage
    while (b > a + 1 && istAuf(a) && ueber(a, b) == b && istZu(b - 1))
    {
        ++a;
        --b;
    }
    const std::size_t block = neuerBlock(rolle, std::move(titel), eltern, a);
    std::size_t k = a;
    if (wort(k, "with"))
    {
        // WITH [RECURSIVE] name [(spalten)] AS [NOT] [MATERIALIZED] (abfrage), …
        ++k;
        if (wort(k, "recursive")) ++k;
        while (k < b && istName(k))
        {
            const std::string name = ohneBegrenzer(tok(k).text);
            std::size_t e = k + 1;
            if (istAuf(e)) e = ueber(e, b);
            if (!wort(e, "as")) break;
            ++e;
            while (wort(e, "not") || wort(e, "materialized")) ++e;
            if (!istAuf(e)) break;
            const std::size_t zu = ueber(e, b);
            liesBlock(e + 1, zu > e + 1 ? zu - 1 : zu, BlockRolle::Mit, name, block);
            k = zu;
            if (!tok(k).istOp(",")) break;
            ++k;
        }
    }
    // Zweige einer Mengenoperation auf dieser Stufe
    std::vector<std::pair<std::size_t, std::size_t>> teile;
    std::vector<std::string> arten;
    std::size_t s = k;
    for (std::size_t j = k; j < b;)
    {
        if (istAuf(j))
        {
            j = ueber(j, b);
            continue;
        }
        if (wort(j, "union") || wort(j, "intersect") || wort(j, "except") || wort(j, "minus"))
        {
            std::string art = gross(tok(j).text);
            std::size_t n = j + 1;
            if (wort(n, "all") || wort(n, "distinct")) art += " " + gross(tok(n++).text);
            teile.push_back({s, j});
            arten.push_back(art);
            s = j = n;
            continue;
        }
        ++j;
    }
    teile.push_back({s, b});
    if (teile.size() == 1)
    {
        liesSelect(block, k, b);
        return block;
    }
    // ORDER BY, LIMIT … hinter dem letzten Zweig gelten der ganzen Operation
    std::size_t& letztes = teile.back().second;
    for (std::size_t j = teile.back().first; j < letztes;)
    {
        if (istAuf(j))
        {
            j = ueber(j, b);
            continue;
        }
        if ((wort(j, "order") && wort(j + 1, "by")) || wort(j, "limit") || wort(j, "offset") || wort(j, "fetch"))
        {
            const std::size_t ende = letztes;
            for (std::size_t x = j; x < ende;)
            {
                if (wort(x, "order") && wort(x + 1, "by"))
                {
                    std::size_t y = x + 2;
                    while (y < ende && !wort(y, "limit") && !wort(y, "offset") && !wort(y, "fetch"))
                        y = istAuf(y) ? ueber(y, ende) : y + 1;
                    m_ab.bloecke[block].ordnung = m_q.quelltext(x + 2, y);
                    x = y;
                    continue;
                }
                std::size_t y = x + 1;
                while (y < ende && !wort(y, "limit") && !wort(y, "offset") && !wort(y, "fetch")) ++y;
                std::string& g = m_ab.bloecke[block].grenze;
                g += (g.empty() ? "" : " ") + m_q.quelltext(x, y);
                x = y;
            }
            letztes = j;
            break;
        }
        ++j;
    }
    std::string mengen;
    for (const std::string& art : arten)
        if (mengen.find(art) == std::string::npos) mengen += (mengen.empty() ? "" : " / ") + art;
    m_ab.bloecke[block].mengen = mengen;
    for (std::size_t i = 0; i < teile.size(); ++i)
    {
        const std::size_t z =
            liesBlock(teile[i].first, teile[i].second, BlockRolle::Zweig, "branch " + std::to_string(i + 1), block);
        m_ab.bloecke[block].zweige.push_back(z);
    }
    return block;
}

void AbfrageLeser::liesSelect(std::size_t block, std::size_t a, std::size_t b)
{
    std::size_t k = a;
    if (wort(k, "values"))
    {
        m_ab.bloecke[block].auswahl = m_q.quelltext(k, b);
        return;
    }
    if (wort(k, "table"))  // Postgres `TABLE t`
    {
        AbfrageQuelle q;
        q.name = liesNamen(k + 1, k);
        q.zeile = tok(a).ort.zeile;
        m_ab.bloecke[block].quellen.push_back(std::move(q));
        m_ab.bloecke[block].auswahl = "*";
        return;
    }
    // PERFORM (PL/pgSQL) ist ein SELECT, dessen Ergebnis verworfen wird
    if (!wort(k, "select") && !wort(k, "perform"))
    {
        m_ab.bloecke[block].auswahl = m_q.quelltext(k, b);
        return;
    }
    ++k;
    std::string vorn;
    while (k < b)
    {
        if (wort(k, "distinct"))
        {
            vorn = "distinct ";
            ++k;
            if (wort(k, "on") && istAuf(k + 1)) k = ueber(k + 1, b);
        }
        else if (wort(k, "all") || wort(k, "sql_calc_found_rows") || wort(k, "straight_join"))
            ++k;
        else if (wort(k, "top"))
        {
            const std::size_t s = k++;
            k = istAuf(k) ? ueber(k, b) : k + 1;
            if (wort(k, "percent")) ++k;
            if (wort(k, "with") && wort(k + 1, "ties")) k += 2;
            m_ab.bloecke[block].grenze = m_q.quelltext(s, k);
        }
        else
            break;
    }
    // die Klauseln auf dieser Stufe; FROM nur vor den anderen (`IS DISTINCT FROM` steht in WHERE)
    std::vector<std::pair<std::size_t, std::string>> klauseln;
    bool nachFrom = false;
    for (std::size_t j = k; j < b;)
    {
        if (istAuf(j))
        {
            j = ueber(j, b);
            continue;
        }
        if (istName(j) && enthaelt(kKlauseln, klein(tok(j).text)))
        {
            const std::string w = klein(tok(j).text);
            const bool mitBy = w == "group" || w == "order" || w == "cluster" || w == "distribute" || w == "sort";
            const bool gilt = (w == "from" && !nachFrom && !wort(j - 1, "distinct")) ||
                              (w == "into" && klauseln.empty()) || (mitBy && wort(j + 1, "by")) ||
                              (w != "from" && w != "into" && !mitBy && (klauseln.empty() || w != klauseln.back().second));
            if (gilt)
            {
                if (w != "into") nachFrom = true;
                klauseln.push_back({j, w});
            }
        }
        ++j;
    }
    const std::size_t listeEnde = klauseln.empty() ? b : klauseln.front().first;
    AbfrageBlock* x = &m_ab.bloecke[block];
    {
        std::string auswahl = ohneUnterabfragen(block, k, listeEnde, BlockRolle::Auswahl, "select");
        x = &m_ab.bloecke[block];
        x->auswahl = vorn + auswahl;
    }
    sammleBezuege(k, listeEnde, m_ab.bloecke[block].bezuege);
    // die Namen nach aussen: je Teil der Liste sein Alias, sonst seine Spalte
    {
        std::size_t s = k;
        const auto teil = [&](std::size_t p, std::size_t q) {
            if (q <= p) return;
            std::string name;
            for (std::size_t j = p; j < q; j = istAuf(j) ? ueber(j, q) : j + 1)
                if (wort(j, "as") && j + 1 < q) name = ohneBegrenzer(tok(j + 1).text);
            if (name.empty() && q - p >= 2 && tok(q - 1).art == TokenArt::Bezeichner &&
                (tok(q - 2).art != TokenArt::Operator || tok(q - 2).istOp(")") || tok(q - 2).istOp("]")))
                name = ohneBegrenzer(tok(q - 1).text);  // `ausdruck alias`, `count(*) n`
            const bool alias = !name.empty();
            if (name.empty() && istName(q - 1) && (q - p == 1 || tok(q - 2).istOp(".")))
                name = ohneBegrenzer(tok(q - 1).text);
            if (name.empty()) return;
            m_ab.bloecke[block].ausgabe.push_back(name);
            if (alias) m_ab.bloecke[block].aliase.push_back(name);
        };
        for (std::size_t j = k; j < listeEnde;)
        {
            if (istAuf(j))
            {
                j = ueber(j, listeEnde);
                continue;
            }
            if (tok(j).istOp(","))
            {
                teil(s, j);
                s = j + 1;
            }
            ++j;
        }
        teil(s, listeEnde);
    }
    for (std::size_t i = 0; i < klauseln.size(); ++i)
    {
        const auto& [p, w] = klauseln[i];
        const std::size_t ende = i + 1 < klauseln.size() ? klauseln[i + 1].first : b;
        const std::size_t von = (w == "group" || w == "order" || w == "cluster" || w == "distribute" || w == "sort") ? p + 2 : p + 1;
        if (w == "from")
            liesQuellen(block, von, ende, "");
        else if (w == "into")
        {
            // T-SQL `SELECT … INTO #t FROM …` legt eine Tabelle an; eine Variable (`@x`) nicht
            std::size_t e = von;
            const std::string name = liesNamen(von, e);
            if (block == 0 && !name.empty() && name.front() != '@' && m_ab.ziel.empty() && selectIntoTabelle)
            {
                m_ab.ziel = name;
                m_ab.schreibt = "select into";
            }
        }
        else if (w == "where")
        {
            std::vector<Bedingungsteil> wo = zerlege(block, von, ende);
            m_ab.bloecke[block].wo = std::move(wo);
        }
        else if (w == "having")
        {
            std::vector<Bedingungsteil> haben = zerlege(block, von, ende);
            m_ab.bloecke[block].haben = std::move(haben);
        }
        else if (w == "group")
        {
            m_ab.bloecke[block].gruppe = m_q.quelltext(von, ende);
            sammleBezuege(von, ende, m_ab.bloecke[block].bezuege);
        }
        else if (w == "order" || w == "cluster" || w == "distribute" || w == "sort")
        {
            std::string& o = m_ab.bloecke[block].ordnung;
            o += (o.empty() ? "" : "; ") + m_q.quelltext(von, ende);
            sammleBezuege(von, ende, m_ab.bloecke[block].bezuege);
        }
        else if (w == "window" || w == "qualify")
        {
            std::string& f = m_ab.bloecke[block].fenster;
            f += (f.empty() ? "" : "; ") + m_q.quelltext(p, ende);
        }
        else if (w == "limit" || w == "offset" || w == "fetch")
        {
            std::string& g = m_ab.bloecke[block].grenze;
            g += (g.empty() ? "" : " ") + m_q.quelltext(p, ende);
        }
        // FOR XML/JSON, OPTION (…) (T-SQL): nichts Gezeigtes
    }
}

void AbfrageLeser::liesQuellen(std::size_t block, std::size_t a, std::size_t b, std::string verbindung)
{
    // das Ende einer Bedingung ON: der naechste Join oder ein Komma auf dieser Stufe
    const auto naechsterJoin = [&](std::size_t j) {
        for (; j < b;)
        {
            if (istAuf(j))
            {
                j = ueber(j, b);
                continue;
            }
            if (tok(j).istOp(",") || wort(j, "join") || wort(j, "inner") || wort(j, "left") || wort(j, "right") ||
                wort(j, "full") || wort(j, "cross") || wort(j, "natural") || wort(j, "semi") || wort(j, "anti") ||
                (wort(j, "outer") && wort(j + 1, "apply")) || (wort(j, "lateral") && wort(j + 1, "view")) ||
                wort(j, "pivot") || wort(j, "unpivot"))
                return j;
            ++j;
        }
        return b;
    };
    std::size_t j = a;
    while (j < b)
    {
        if (tok(j).istOp(","))
        {
            verbindung = ",";
            ++j;
            continue;
        }
        // Spark `LATERAL VIEW [OUTER] f(…) t AS c`
        if (wort(j, "lateral") && wort(j + 1, "view"))
        {
            j += 2;
            if (wort(j, "outer")) ++j;
            verbindung = "lateral view";
        }
        // die Art des Joins
        std::vector<std::string> woerter;
        while (j < b && (wort(j, "natural") || wort(j, "inner") || wort(j, "left") || wort(j, "right") || wort(j, "full") ||
                         wort(j, "outer") || wort(j, "cross") || wort(j, "semi") || wort(j, "anti") || wort(j, "join") ||
                         wort(j, "apply")))
        {
            woerter.push_back(klein(tok(j).text));
            const bool schluss = wort(j, "join") || wort(j, "apply");
            ++j;
            if (schluss) break;
        }
        if (!woerter.empty())
        {
            const bool apply = std::find(woerter.begin(), woerter.end(), "apply") != woerter.end();
            std::string art;
            for (const std::string& w : woerter)
            {
                if (w == "join" || w == "apply" || (w == "outer" && !apply) || (apply && w == "cross")) continue;
                if ((w == "semi" || w == "anti") && art == "left") art.clear();
                art += (art.empty() ? "" : " ") + w;
            }
            if (apply) art = art.empty() ? "apply" : art + " apply";
            verbindung = art.empty() ? "inner" : art;
        }
        if (wort(j, "pivot") || wort(j, "unpivot"))
        {
            // PIVOT (…) [AS] alias: dreht die Quelle davor, nennt keine neue
            j = istAuf(j + 1) ? ueber(j + 1, b) : j + 1;
            if (wort(j, "as")) ++j;
            if (istName(j) && !enthaelt(kNachQuelle, klein(tok(j).text))) ++j;
            continue;
        }
        if (wort(j, "lateral")) ++j;
        if (j >= b) break;

        AbfrageQuelle q;
        q.verbindung = verbindung;
        q.zeile = tok(j).ort.zeile;
        std::size_t unter = kKeinBlock, unterBis = 0;
        if (istAuf(j))
        {
            const std::size_t zu = ueber(j, b);
            if (abfrageInKlammer(j))
            {
                q.art = QuellArt::Abgeleitet;
                unter = j + 1;
                unterBis = zu > j + 1 ? zu - 1 : zu;
            }
            else if (wort(j + 1, "values"))
                q.art = QuellArt::Werte;
            else
            {
                // eine Klammer um Joins: ihre Quellen gehoeren zu diesem Block
                liesQuellen(block, j + 1, zu > j + 1 ? zu - 1 : zu, verbindung);
                verbindung.clear();
                j = zu;
                continue;
            }
            j = zu;
        }
        else if (wort(j, "values"))
        {
            q.art = QuellArt::Werte;
            ++j;
            while (j < b && (istAuf(j) || (tok(j).istOp(",") && istAuf(j + 1)))) j = istAuf(j) ? ueber(j, b) : j + 1;
        }
        else
        {
            std::size_t e = j;
            q.name = liesNamen(j, e);
            if (q.name.empty())
            {
                ++j;
                continue;
            }
            if (istAuf(e))
            {
                q.art = QuellArt::Funktion;
                e = ueber(e, b);
            }
            j = e;
        }
        // Hinweise und Stichproben, dann der Alias (mit Spalten)
        const auto uebergehe = [&] {
            for (bool weiter = true; weiter;)
            {
                weiter = false;
                if (wort(j, "with") && istAuf(j + 1))
                {
                    j = ueber(j + 1, b);
                    weiter = true;
                }
                if (wort(j, "tablesample"))
                {
                    ++j;
                    while (j < b && istName(j) && !istAuf(j)) ++j;
                    if (istAuf(j)) j = ueber(j, b);
                    if (wort(j, "repeatable") && istAuf(j + 1)) j = ueber(j + 1, b);
                    weiter = true;
                }
                if (wort(j, "final") || wort(j, "only")) ++j;
            }
        };
        uebergehe();
        if (wort(j, "as")) ++j;
        if (j < b && istName(j) && !enthaelt(kNachQuelle, klein(tok(j).text)))
        {
            q.alias = ohneBegrenzer(tok(j++).text);
            if (istAuf(j)) j = ueber(j, b);  // Spalten des Alias
        }
        uebergehe();
        if (q.verbindung == "lateral view" && wort(j, "as"))  // Spark: `t AS c1, c2` nennt die Spalten
        {
            ++j;
            while (j < b && (istName(j) || tok(j).istOp(",")) && !wort(j, "lateral")) ++j;
        }
        if (unter != kKeinBlock) q.block = liesBlock(unter, unterBis, BlockRolle::Abgeleitet, q.alias, block);
        if (wort(j, "on"))
        {
            const std::size_t e = naechsterJoin(j + 1);
            std::vector<Bedingungsteil> teile = zerlege(block, j + 1, e);
            q.bedingung = std::move(teile);
            j = e;
        }
        else if (wort(j, "using") && istAuf(j + 1))
        {
            const std::size_t zu = ueber(j + 1, b);
            for (std::size_t x = j + 2; x + 1 < zu; ++x)
                if (istName(x)) q.mit.push_back(ohneBegrenzer(tok(x).text));
            j = zu;
        }
        m_ab.bloecke[block].quellen.push_back(std::move(q));
        verbindung.clear();
        // was keine Quelle beginnt (Reste wie `FOR SYSTEM_TIME …`), bis zum naechsten Join oder Komma
        if (j < b && !tok(j).istOp(",") && naechsterJoin(j) != j) j = naechsterJoin(j);
    }
}

std::vector<Bedingungsteil> AbfrageLeser::zerlege(std::size_t block, std::size_t a, std::size_t b)
{
    std::vector<Bedingungsteil> aus;
    if (a >= b) return aus;
    // eine Klammer um die ganze Bedingung (keine Abfrage)
    if (istAuf(a) && ueber(a, b) == b && istZu(b - 1) && !abfrageInKlammer(a)) return zerlege(block, a + 1, b - 1);
    const auto teil = [&](std::size_t x, std::size_t y) {
        if (x >= y) return;
        Bedingungsteil t;
        std::vector<std::size_t> unter;
        t.text = ohneUnterabfragen(block, x, y, BlockRolle::Bedingung, "", &unter);
        if (!unter.empty())
        {
            t.unterabfrage = unter.front();
            t.wie = m_ab.bloecke[unter.front()].titel;
        }
        sammleBezuege(x, y, t.bezuege);
        for (const Spaltenbezug& s : t.bezuege)
            if (std::find(m_ab.bloecke[block].bezuege.begin(), m_ab.bloecke[block].bezuege.end(), s) ==
                m_ab.bloecke[block].bezuege.end())
                m_ab.bloecke[block].bezuege.push_back(s);
        aus.push_back(std::move(t));
    };
    std::size_t s = a;
    int zwischen = 0;  // offene BETWEEN: ihr AND trennt nicht
    int fall = 0;
    for (std::size_t j = a; j < b;)
    {
        if (istAuf(j))
        {
            j = ueber(j, b);
            continue;
        }
        if (wort(j, "case")) ++fall;
        if (wort(j, "end") && fall > 0) --fall;
        if (wort(j, "between")) ++zwischen;
        if (wort(j, "and") && fall == 0)
        {
            if (zwischen > 0)
                --zwischen;
            else
            {
                teil(s, j);
                s = j + 1;
            }
        }
        ++j;
    }
    teil(s, b);
    return aus;
}

std::string AbfrageLeser::ohneUnterabfragen(std::size_t block, std::size_t a, std::size_t b, BlockRolle rolle,
                                            const std::string& titel, std::vector<std::size_t>* bloecke)
{
    std::string text;
    std::size_t s = a;
    for (std::size_t j = a; j < b;)
    {
        if (!abfrageInKlammer(j))
        {
            ++j;
            continue;
        }
        const std::size_t zu = ueber(j, b);
        // wie die Bedingung sie befragt: EXISTS, IN, ein Vergleich
        std::string wie = titel;
        if (wie.empty())
        {
            const std::size_t d = j > a ? j - 1 : j;
            const bool nicht = d > a && wort(d - 1, "not");
            if (j > a && wort(d, "exists"))
                wie = nicht ? "not exists" : "exists";
            else if (j > a && wort(d, "in"))
                wie = nicht ? "not in" : "in";
            else if (j > a && (wort(d, "any") || wort(d, "all") || wort(d, "some")))
                wie = (d > a ? std::string(tok(d - 1).text) + " " : std::string()) + klein(tok(d).text);
            else if (j > a && tok(d).art == TokenArt::Operator)
                wie = std::string(tok(d).text);
            else
                wie = "subquery";
        }
        if (j > s) text += m_q.quelltext(s, j) + " ";
        text += "(…)";
        const std::size_t neu = liesBlock(j + 1, zu > j + 1 ? zu - 1 : zu, rolle, wie, block);
        if (bloecke != nullptr) bloecke->push_back(neu);
        s = j = zu;
        if (s < b) text += " ";
    }
    if (s < b) text += m_q.quelltext(s, b);
    return text;
}

void AbfrageLeser::sammleBezuege(std::size_t a, std::size_t b, std::vector<Spaltenbezug>& aus) const
{
    const auto neu = [&](Spaltenbezug s) {
        if (std::find(aus.begin(), aus.end(), s) == aus.end()) aus.push_back(std::move(s));
    };
    for (std::size_t j = a; j < b;)
    {
        if (abfrageInKlammer(j))
        {
            j = ueber(j, b);
            continue;
        }
        const Token& t = tok(j);
        const bool davorPunkt = j > a && (tok(j - 1).istOp(".") || tok(j - 1).istOp("::"));
        const bool davorAs = j > a && wort(j - 1, "as");
        if (istName(j) && !davorPunkt && tok(j + 1).istOp(".") && (istName(j + 2) || tok(j + 2).istOp("*")))
        {
            // eine Kette `q.spalte`, `schema.t.spalte`, `q.*`
            std::vector<std::string> teile{ohneBegrenzer(t.text)};
            std::size_t k = j;
            while (tok(k + 1).istOp(".") && (istName(k + 2) || tok(k + 2).istOp("*")))
            {
                teile.push_back(tok(k + 2).istOp("*") ? std::string("*") : ohneBegrenzer(tok(k + 2).text));
                k += 2;
            }
            if (!istAuf(k + 1))  // `schema.funktion(` ruft
            {
                Spaltenbezug s;
                s.spalte = teile.back();
                for (std::size_t i = 0; i + 1 < teile.size(); ++i) s.qualifizierer += (i > 0 ? "." : "") + teile[i];
                neu(std::move(s));
            }
            j = k + 1;
            continue;
        }
        if (t.art == TokenArt::Bezeichner && !davorPunkt && !davorAs && !istAuf(j + 1) && !tok(j + 1).istOp(".") &&
            tok(j + 1).art != TokenArt::Zeichenkette && !t.text.empty() && t.text.front() != '@' && t.text.front() != '$' &&
            t.text.front() != ':' && t.text.front() != '#' && !(j > a && tok(j - 1).istOp(":")) &&
            !enthaelt(kKeineSpalte, klein(t.text)))
            neu({{}, ohneBegrenzer(t.text)});
        ++j;
    }
}

}  // namespace c2d::sprache::sqlteile
