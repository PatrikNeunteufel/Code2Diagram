/**
 * @file Rumpf.cpp
 * @brief Der Ablauf im Rumpf einer SQL-Routine (Entscheide R1-R3, R6)
 *
 * Abbildung (Entscheid R2) — je Wort der Quelle die Anweisung des Codemodells:
 *
 * | Quelle                                                     | Codemodell                                  |
 * |------------------------------------------------------------|---------------------------------------------|
 * | IF / ELSIF / ELSEIF / ELSE (auch T-SQL ohne THEN)          | Wenn, ein ELSIF als Wenn im sonst           |
 * | WHILE                                                      | Solange (Kopf wie in der Quelle)            |
 * | LOOP … END LOOP                                            | Solange, immer wahr                         |
 * | REPEAT … UNTIL c                                           | TueSolange, endetWennWahr, Kopf `UNTIL c`   |
 * | FOR … LOOP/DO, FOREACH, Firebird FOR SELECT … DO           | FuerJedes, Kopf wie in der Quelle           |
 * | EXIT/LEAVE/BREAK [marke] [WHEN c], CONTINUE/ITERATE         | Abbruch, Fortsetzung (mit WHEN: in einem Wenn) |
 * | RETURN [x]; RETURN NEXT/QUERY                              | Ruecksprung; Einfach                        |
 * | RAISE [EXCEPTION], THROW, SIGNAL, RESIGNAL, Firebird EXCEPTION | Wurf                                     |
 * | RAISE DEBUG/LOG/INFO/NOTICE/WARNING                        | Einfach                                     |
 * | RAISERROR                                                  | Wurf in einem TRY mit Schwere >= 11, sonst Einfach |
 * | BEGIN TRY … END TRY BEGIN CATCH … END CATCH                | Versuch, Fang `BEGIN CATCH`                 |
 * | BEGIN … EXCEPTION WHEN x THEN … END (PL/pgSQL)             | Versuch, je WHEN ein Fang                   |
 * | DECLARE EXIT HANDLER FOR x s (MySQL, Spark)                | der Rest des Blocks ein Versuch, s ein Fang (R3) |
 * | DECLARE CONTINUE HANDLER FOR x s                           | Einfach abseits, mit Hinweis (R3)           |
 * | CASE x WHEN … THEN … END CASE                              | Auswahl ohne Durchfallen; ohne ELSE wirft sie |
 * | CASE WHEN c THEN … END CASE                                | Kette Wenn; ohne ELSE wirft sie             |
 * | GOTO L, `L:` (T-SQL)                                       | Sprung, Marke                               |
 * | `<<x>>`, `x:` vor einer Schleife oder einem Block           | Anweisung::marke                            |
 * | BEGIN … END, DECLARE-Teil (PL/pgSQL)                       | Block                                       |
 * | alles andere (DML, SET, Cursor, Transaktionen, EXEC …)      | Einfach                                     |
 *
 * Ein CASE als Anweisung ohne ELSE loest einen Fehler aus, wenn kein Zweig passt (SQL/PSM `CASE_NOT_FOUND`, auch
 * PL/pgSQL, MySQL): dafuer steht ein Zweig `ELSE` mit einem Wurf da.
 */
#include "Rumpf.hpp"

#include <algorithm>
#include <iterator>

namespace c2d::sprache::sqlteile
{

namespace
{

using lexer::TokenArt;
using modell::AnweisungArt;
using modell::AnweisungPtr;
using quelle::Schwere;

std::string klein(std::string_view text)
{
    std::string s(text);
    for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return s;
}

template <std::size_t N>
bool inListe(const std::string_view (&liste)[N], std::string_view w)
{
    return std::find(std::begin(liste), std::end(liste), w) != std::end(liste);
}

/// Namen, die jede SQL-Anweisung setzt (Entscheid R6): eine Bedingung, die sie liest, gilt nur bis zur naechsten
/// Anweisung als entschieden.
constexpr std::string_view kSystemNamen[] = {"@@rowcount", "@@fetch_status", "@@error", "@@identity", "@@trancount", "found"};

/// Woerter, mit denen in T-SQL eine Anweisung beginnt — hinter IF und WHILE endet dort die Bedingung.
constexpr std::string_view kAnfangTsql[] = {
    "begin",     "set",      "select",   "insert",   "update",     "delete",  "merge",   "exec",       "execute",
    "return",    "raiserror", "throw",   "print",    "if",         "while",   "break",   "continue",   "goto",
    "declare",   "truncate", "waitfor",  "commit",   "rollback",   "save",    "open",    "fetch",      "close",
    "deallocate", "drop",    "create",   "alter",    "with",       "else",    "end",     "use",        "bulk",
    "grant",     "revoke",   "deny",     "dbcc",     "kill",       "reconfigure", "checkpoint", "backup", "restore",
    "enable",    "disable",  "readtext", "writetext", "updatetext",
};

/// Stufen von RAISE, die nur melden (PL/pgSQL) — RAISE EXCEPTION und RAISE ohne Stufe werfen.
constexpr std::string_view kMeldeStufen[] = {"debug", "log", "info", "notice", "warning"};

/// Woerter hinter END, die zu ihm gehoeren.
constexpr std::string_view kNachEnde[] = {"if", "loop", "while", "repeat", "case", "for", "try", "catch"};

/// Woerter, an denen im Stil mit `;` eine Anweisung ohne `;` am Anfang einer Zeile spaetestens endet.
constexpr std::string_view kStoppMitArt[] = {"elsif", "elseif", "else", "when", "until", "exception"};

}  // namespace

// --- Stil -------------------------------------------------------------------

RumpfStil stilVon(const Tokenquelle& q, std::size_t a, std::size_t b, std::string_view dialekt)
{
    if (dialekt == "tsql") return RumpfStil::Tsql;
    if (dialekt == "postgres" || dialekt == "mysql" || dialekt == "spark") return RumpfStil::EndeMitArt;
    // der Kern, SQLite und DuckDB (sie kennen keinen Ablauf ausser BEGIN … END eines Triggers): aus dem Text
    const auto w = [&](std::size_t i) {
        const lexer::Token& t = q.token(i);
        return t.art == TokenArt::Schluesselwort || t.art == TokenArt::Bezeichner ? klein(t.text) : std::string();
    };
    // ein THEN, das zu einem IF gehoert (nicht zu CASE WHEN … THEN in einem Ausdruck)
    const auto ifMitThen = [&](std::size_t i) {
        int fall = 0;
        for (std::size_t k = i + 1; k < b && q.token(k).art != TokenArt::Ende;)
        {
            if (q.token(k).istOp("("))
            {
                k = q.hinterKlammer(k);
                continue;
            }
            const std::string x = w(k);
            if (q.token(k).istOp(";")) return false;
            if (x == "case") ++fall;
            else if (x == "end" && fall > 0) --fall;
            else if (fall == 0 && x == "then") return true;
            else if (fall == 0 && (x == "begin" || x == "end" || x == "else" || inListe(kAnfangTsql, x))) return false;
            ++k;
        }
        return false;
    };
    bool dann = false;
    for (std::size_t i = a; i < b && q.token(i).art != TokenArt::Ende; ++i)
    {
        const std::string x = w(i);
        if (x == "end" && inListe(kNachEnde, w(i + 1)) && w(i + 1) != "try" && w(i + 1) != "catch")
            return RumpfStil::EndeMitArt;
        if (x == "if" && w(i - 1) != "end" && ifMitThen(i)) dann = true;
    }
    return dann ? RumpfStil::Firebird : RumpfStil::Tsql;
}

// --- Token ------------------------------------------------------------------

bool RumpfLeser::wort(std::size_t i, std::string_view w) const
{
    // hinter `.` steht ein Name, kein Wort der Sprache (`NEW.end`, `w.end` — eine Spalte)
    if (i > 0 && tok(i - 1).istOp(".")) return false;
    return istName(i) && tok(i).text.size() == w.size() && klein(tok(i).text) == w;
}

bool RumpfLeser::istName(std::size_t i) const
{
    const TokenArt a = tok(i).art;
    return a == TokenArt::Bezeichner || a == TokenArt::Schluesselwort;
}

bool RumpfLeser::einesVon(std::size_t i, std::initializer_list<std::string_view> woerter) const
{
    return std::any_of(woerter.begin(), woerter.end(), [&](std::string_view w) { return wort(i, w); });
}

std::string RumpfLeser::text(std::size_t a, std::size_t b) const
{
    if (b <= a) return {};
    std::string s = m_q.quelltext(a, b);
    while (!s.empty() && (s.back() == ' ' || s.back() == ';')) s.pop_back();
    return s;
}

std::size_t RumpfLeser::ohneTrenner(std::size_t i, std::size_t b) const
{
    while (i < b && tok(i).istOp(";")) ++i;
    return i;
}

// --- Enden ------------------------------------------------------------------

std::size_t RumpfLeser::ende(std::size_t i, std::size_t b) const
{
    if (m_stil == RumpfStil::Tsql) return std::min(anweisungsEnde(m_q, i, b), b);
    int fall = 0;
    for (std::size_t k = i; k < b;)
    {
        const Token& t = tok(k);
        if (t.art == TokenArt::Ende) return k;
        if (t.istOp("("))
        {
            k = std::min(m_q.hinterKlammer(k), b);
            continue;
        }
        if (t.istOp(";") && fall == 0) return k;
        if (wort(k, "case")) ++fall;
        if (wort(k, "end"))
        {
            if (fall == 0) return k;
            --fall;
        }
        // ein fehlendes `;` vor ELSE, WHEN … am Anfang einer Zeile
        if (k > i && fall == 0 && tok(k - 1).ort.zeile < t.ort.zeile && istName(k) && inListe(kStoppMitArt, klein(t.text)))
            return k;
        ++k;
    }
    return b;
}

std::size_t RumpfLeser::bedingungsEnde(std::size_t i, std::size_t b) const
{
    int fall = 0;
    for (std::size_t k = i; k < b;)
    {
        const Token& t = tok(k);
        if (t.art == TokenArt::Ende || (t.istOp(";") && fall == 0)) return k;
        if (t.istOp("("))
        {
            k = std::min(m_q.hinterKlammer(k), b);
            continue;
        }
        if (wort(k, "case")) ++fall;
        else if (wort(k, "end") && fall > 0) --fall;
        else if (k > i && fall == 0 && istName(k) && inListe(kAnfangTsql, klein(t.text)))
        {
            const bool funktion = (wort(k, "update") || wort(k, "insert")) && tok(k + 1).istOp("(");  // UPDATE(spalte)
            const bool keinWith = wort(k, "with") && !withMitAbfrage(m_q, k);
            if (!funktion && !keinWith) return k;
        }
        ++k;
    }
    return b;
}

std::size_t RumpfLeser::bisWort(std::size_t i, std::size_t b, std::initializer_list<std::string_view> w) const
{
    int fall = 0;
    for (std::size_t k = i; k < b;)
    {
        if (tok(k).art == TokenArt::Ende) return k;
        if (tok(k).istOp("("))
        {
            k = std::min(m_q.hinterKlammer(k), b);
            continue;
        }
        if (fall == 0 && einesVon(k, w)) return k;
        if (wort(k, "case")) ++fall;
        else if (wort(k, "end") && fall > 0) --fall;
        ++k;
    }
    return b;
}

std::size_t RumpfLeser::hinterEnde(std::size_t i, std::size_t b) const
{
    std::size_t k = i + 1;
    const int z = zeile(i);
    if (k < b && zeile(k) == z && istName(k) && inListe(kNachEnde, klein(tok(k).text))) ++k;
    // eine Marke dahinter (`END LOOP aussen`, `END marke`) — nicht in T-SQL, dort folgt die naechste Anweisung
    if (m_stil != RumpfStil::Tsql && k < b && zeile(k) == z && tok(k).art == TokenArt::Bezeichner) ++k;
    return k;
}

std::size_t RumpfLeser::erwarte(std::size_t i, std::size_t b, std::string_view w)
{
    if (i < b && wort(i, w)) return i + 1;
    m_q.meldeBei(Schwere::Warnung, std::min(i, b > 0 ? b - 1 : b),
                 "'" + std::string(w) + "' expected in the body of the routine");
    return i;
}

// --- Bauen ------------------------------------------------------------------

AnweisungPtr RumpfLeser::neu(AnweisungArt art, std::size_t bei) const
{
    auto a = std::make_unique<modell::Anweisung>();
    a->art = art;
    a->zeile = zeile(bei);
    return a;
}

AnweisungPtr RumpfLeser::block(Folge folge, std::size_t bei) const
{
    AnweisungPtr a = neu(AnweisungArt::Block, bei);
    a->kinder = std::move(folge);
    return a;
}

void RumpfLeser::ruft(modell::Anweisung& an, std::size_t a, std::size_t b)
{
    an.aufrufe = m_q.aufrufeIn(a, b);
    an.aufrufeGenannt = true;
}

AnweisungPtr RumpfLeser::einfach(std::size_t a, std::size_t e)
{
    AnweisungPtr an = neu(AnweisungArt::Einfach, a);
    an->text = text(a, e);
    ruft(*an, a, e);
    an->istAufruf = einesVon(a, {"call", "exec", "execute", "perform"}) && !an->aufrufe.empty();
    wirkung(*an, a, e);
    return an;
}

void RumpfLeser::wirkung(modell::Anweisung& an, std::size_t a, std::size_t e) const
{
    const auto schreibe = [&](std::string_view name) {
        std::string n = klein(name);
        if (!n.empty() && std::find(an.schreibt.begin(), an.schreibt.end(), n) == an.schreibt.end())
            an.schreibt.push_back(std::move(n));
    };
    // die Teile [x, y) an den Kommas auf oberster Ebene
    const auto teile = [&](std::size_t x, std::size_t y) {
        std::vector<std::pair<std::size_t, std::size_t>> aus;
        std::size_t von = x;
        for (std::size_t k = x; k < y;)
        {
            if (tok(k).istOp("("))
            {
                k = std::min(m_q.hinterKlammer(k), y);
                continue;
            }
            if (tok(k).istOp(","))
            {
                if (k > von) aus.emplace_back(von, k);
                von = k + 1;
            }
            ++k;
        }
        if (y > von) aus.emplace_back(von, y);
        return aus;
    };
    // in [x, y) die erste Zuweisung (`=`, `:=`) auf oberster Ebene; y = keine
    const auto zuweisung = [&](std::size_t x, std::size_t y) {
        for (std::size_t k = x; k < y;)
        {
            if (tok(k).istOp("("))
            {
                k = std::min(m_q.hinterKlammer(k), y);
                continue;
            }
            if (tok(k).istOp("=") || tok(k).istOp(":=")) return k;
            ++k;
        }
        return y;
    };
    // die Namen hinter INTO bis zu einem Wort, das nicht dazugehoert
    const auto hinterInto = [&](std::size_t x, std::size_t y) {
        for (std::size_t k = x; k < y; ++k)
        {
            if (!wort(k, "into")) continue;
            std::size_t j = k + 1;
            if (wort(j, "strict")) ++j;  // PL/pgSQL
            while (j < y && tok(j).art == TokenArt::Bezeichner)
            {
                std::size_t n = j;
                while (tok(n + 1).istOp(".") && istName(n + 2)) n += 2;
                schreibe(text(j, n + 1));
                j = n + 1;
                if (!tok(j).istOp(",")) break;
                ++j;
            }
            return;
        }
    };

    const std::string w0 = istName(a) ? klein(tok(a).text) : std::string();
    std::size_t zuweisungen = 0;
    std::pair<std::size_t, std::size_t> ziel{0, 0};
    std::pair<std::size_t, std::size_t> wert{0, 0};
    const auto merke = [&](std::size_t x, std::size_t eq, std::size_t y) {
        schreibe(text(x, eq));
        ++zuweisungen;
        ziel = {x, eq};
        wert = {eq + 1, y};
    };
    if (w0 == "set" || w0 == "get")
    {
        // `SET @x = …, @y = …` (T-SQL, MySQL), `SET x := …`, `SET @n += 1`; `SET NOCOUNT ON` weist nichts zu;
        // `GET DIAGNOSTICS x = ROW_COUNT`
        const std::size_t von = w0 == "get" ? a + 2 : a + 1;
        for (const auto& [x, y] : teile(von, e))
        {
            if (!istName(x)) continue;
            std::size_t n = x;
            while (tok(n + 1).istOp(".") && istName(n + 2)) n += 2;
            if (tok(n + 1).istOp("=") || tok(n + 1).istOp(":="))
                merke(x, n + 1, y);
            else if (const std::size_t eq = zuweisung(x, y); eq < y)
                schreibe(text(x, n + 1));  // `+=`, `-=` …: geschrieben, aber keine einfache Zuweisung
        }
    }
    else if (w0 == "select")
    {
        // T-SQL `SELECT @x = …, @y = …`; PL/pgSQL, MySQL `SELECT … INTO x, y FROM …`
        for (const auto& [x, y] : teile(a + 1, e))
            if (tok(x).text.starts_with('@') && tok(x + 1).istOp("=")) merke(x, x + 1, y);
        if (m_stil != RumpfStil::Tsql) hinterInto(a, e);
    }
    else if (w0 == "fetch")
        hinterInto(a, e);
    else if (w0 == "exec" || w0 == "execute")
    {
        // `EXEC @rc = p …`, Parameter mit OUTPUT/OUT
        if (tok(a + 1).text.starts_with('@') && tok(a + 2).istOp("=")) schreibe(tok(a + 1).text);
        for (std::size_t k = a + 1; k < e; ++k)
            if ((wort(k, "output") || wort(k, "out")) && tok(k - 1).text.starts_with('@')) schreibe(tok(k - 1).text);
    }
    else if (w0 == "declare")
    {
        // `DECLARE @a int = 1, @b int` (T-SQL), `DECLARE a, b INT DEFAULT 0` (MySQL): je Teil der erste Name
        for (const auto& [x, y] : teile(a + 1, e))
            if (tok(x).art == TokenArt::Bezeichner) schreibe(tok(x).text);
    }
    else if (m_stil != RumpfStil::Tsql && (w0 == "insert" || w0 == "update" || w0 == "delete"))
        hinterInto(a, e);  // `… RETURNING id INTO x` (PL/pgSQL)
    else if (tok(a).art == TokenArt::Bezeichner)
    {
        // PL/pgSQL: `x := …`, `x = …`, `NEW.y := …` — ein Vergleich steht nie allein als Anweisung
        std::size_t n = a;
        while (tok(n + 1).istOp(".") && istName(n + 2)) n += 2;
        if (tok(n + 1).istOp(":=") || tok(n + 1).istOp("=")) merke(a, n + 1, e);
    }
    if (zuweisungen == 1)
    {
        an.ziel = text(ziel.first, ziel.second);
        an.wert = text(wert.first, wert.second);
    }
    for (const std::string_view n : kSystemNamen) schreibe(n);
    an.ruft = !an.aufrufe.empty() || w0 == "exec" || w0 == "execute" || w0 == "call" || w0 == "perform";
}

// --- Folgen -----------------------------------------------------------------

modell::AnweisungPtr RumpfLeser::lies(std::size_t a, std::size_t b)
{
    Folge folge;
    std::size_t i = blockFolge(a, b, {}, folge);
    while (i < b && !tok(i).istOp(";") && tok(i).art != TokenArt::Ende)
    {
        // ein Wort, an dem die Folge stehen blieb (END ohne BEGIN, ELSE ohne IF): melden und weiter
        m_q.meldeBei(Schwere::Warnung, i, "'" + std::string(tok(i).text) + "' is not understood in the body of the routine");
        i = blockFolge(i + 1, b, {}, folge);
    }
    return block(std::move(folge), a);
}

std::size_t RumpfLeser::folge(std::size_t i, std::size_t b, std::initializer_list<std::string_view> stopps, Folge& ziel)
{
    while (true)
    {
        i = ohneTrenner(i, b);
        if (i >= b || tok(i).art == TokenArt::Ende) return i;
        if (einesVon(i, stopps)) return i;
        if (wort(i, "end") || (m_stil == RumpfStil::Tsql && wort(i, "else"))) return i;  // gehoert dem, der rief
        auto [a, n] = anweisung(i, b);
        if (a) ziel.push_back(std::move(a));
        i = n > i ? n : i + 1;
    }
}

std::size_t RumpfLeser::blockFolge(std::size_t i, std::size_t b, std::initializer_list<std::string_view> stopps,
                                   Folge& ziel)
{
    while (true)
    {
        i = ohneTrenner(i, b);
        if (i >= b || tok(i).art == TokenArt::Ende || einesVon(i, stopps)) return i;
        if (wort(i, "end") || (m_stil == RumpfStil::Tsql && wort(i, "else"))) return i;
        const bool handler = wort(i, "declare") && einesVon(i + 1, {"continue", "exit", "undo"}) && wort(i + 2, "handler");
        if (!handler)
        {
            auto [a, n] = anweisung(i, b);
            if (a) ziel.push_back(std::move(a));
            i = n > i ? n : i + 1;
            continue;
        }
        // `DECLARE {CONTINUE|EXIT|UNDO} HANDLER FOR bedingung[, …] anweisung` (Entscheid R3)
        const bool weiter = wort(i + 1, "continue");
        std::size_t k = erwarte(i + 3, b, "for");
        const std::size_t vonBedingung = k;
        while (k < b)
        {
            if (wort(k, "not") && wort(k + 1, "found"))
                k += 2;
            else if (wort(k, "sqlstate"))
                k += wort(k + 1, "value") ? 3 : 2;
            else
                ++k;
            if (!tok(k).istOp(",")) break;
            ++k;
        }
        const std::string bedingung = text(vonBedingung, k);
        auto [tat, n] = anweisung(k, b);
        if (!tat) tat = block({}, k);
        if (weiter)
        {
            AnweisungPtr an = neu(AnweisungArt::Einfach, i);
            an->text = "on " + bedingung + ": " + text(k, n) + " (continue handler)";
            an->abseits = true;
            an->aufrufe = tat->aufrufe;
            an->aufrufeGenannt = true;
            m_q.meldeBei(Schwere::Hinweis, i,
                         "CONTINUE HANDLER FOR " + bedingung +
                             " is shown where it is declared, without edges - it runs after any statement that raises it");
            ziel.push_back(std::move(an));
            i = n;
            continue;
        }
        // EXIT: der Rest des Blocks ist ein Versuch, der Handler ein Fang; laeuft er, endet der Block
        AnweisungPtr versuch = neu(AnweisungArt::Versuch, i);
        versuch->kopf = "DECLARE HANDLER";
        AnweisungPtr fang = neu(AnweisungArt::Fang, i);
        fang->text = bedingung;
        fang->kopf = "EXIT HANDLER FOR " + bedingung;
        Folge tatFolge;
        tatFolge.push_back(std::move(tat));
        fang->kinder.push_back(block(std::move(tatFolge), k));
        Folge rest;
        const std::size_t danach = blockFolge(n, b, stopps, rest);
        versuch->kinder.push_back(block(std::move(rest), n));
        versuch->kinder.push_back(std::move(fang));
        // weitere EXIT-Handler gleich dahinter stehen nun vorn im Rest: ihr Versuch liegt im Block des ersten —
        // ein Fehler im Rest faengt so der innerste, wie in der Quelle der zuletzt erklaerte (MySQL)
        ziel.push_back(std::move(versuch));
        return danach;
    }
}

// --- Anweisungen ------------------------------------------------------------

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::anweisung(std::size_t i, std::size_t b)
{
    const Token& t = tok(i);
    // Marken: `<<x>>` (PL/pgSQL), `x:` (MySQL, Spark: vor einer Schleife oder einem Block; T-SQL: Ziel eines GOTO)
    if (m_stil != RumpfStil::Tsql && t.istOp("<<") && istName(i + 1) && tok(i + 2).istOp(">>"))
    {
        m_marke = std::string(tok(i + 1).text);
        return anweisung(i + 3, b);
    }
    if (t.art == TokenArt::Bezeichner && tok(i + 1).istOp(":") && !tok(i + 2).istOp("=") && zeile(i + 1) == zeile(i))
    {
        if (m_stil == RumpfStil::Tsql)
        {
            AnweisungPtr m = neu(AnweisungArt::Marke, i);
            m->text = std::string(t.text);
            return {std::move(m), i + 2};
        }
        m_marke = std::string(t.text);
        return anweisung(i + 2, b);
    }
    const std::string w = istName(i) ? klein(t.text) : std::string();
    const bool transaktion = w == "begin" && (tok(i + 1).istOp(";") || einesVon(i + 1, {"tran", "transaction", "work", "distributed"}));
    const bool schleifenWort = w == "while" || w == "loop" || w == "repeat" || w == "for" || w == "foreach";
    const bool blockWort = w == "begin" && !transaktion;
    if (!schleifenWort && !blockWort && w != "declare") m_marke.clear();  // eine Marke vor etwas anderem: keine

    if (m_stil == RumpfStil::Tsql)
    {
        if (w == "if") return wennTsql(i, b);
        if (w == "while") return schleife(i, b);
        if (blockWort) return beginTsql(i, b);
        if (w == "break" || w == "continue" || w == "goto" || w == "return" || w == "throw" || w == "raiserror")
            return sprung(i, b);
    }
    else
    {
        if (w == "if") return m_stil == RumpfStil::Firebird ? wennFirebird(i, b) : wennMitArt(i, b);
        if (schleifenWort && !(w == "for" && wort(i + 1, "update"))) return schleife(i, b);
        if (w == "case" && m_stil == RumpfStil::EndeMitArt) return fall(i, b);
        if (blockWort) return beginMitArt(i, b);
        if (w == "declare") return deklaration(i, b);
        if (w == "exit" || w == "leave" || w == "continue" || w == "iterate" || w == "return" || w == "raise" ||
            w == "signal" || w == "resignal" || (m_stil == RumpfStil::Firebird && (w == "exception" || w == "break")))
            return sprung(i, b);
    }
    const std::size_t e = ende(i, b);
    if (e <= i) return {nullptr, i + 1};
    return {einfach(i, e), e};
}

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::wennTsql(std::size_t i, std::size_t b)
{
    const std::size_t ce = bedingungsEnde(i + 1, b);
    AnweisungPtr an = neu(AnweisungArt::Wenn, i);
    an->text = text(i + 1, ce);
    ruft(*an, i + 1, ce);
    auto [dann, n] = anweisung(ohneTrenner(ce, b), b);
    if (!dann)
    {
        m_q.meldeBei(Schwere::Warnung, i, "IF without a statement");
        dann = block({}, i);
    }
    an->kinder.push_back(std::move(dann));
    const std::size_t k = ohneTrenner(n, b);
    if (k < b && wort(k, "else"))
    {
        auto [sonst, m] = anweisung(ohneTrenner(k + 1, b), b);
        an->sonst = sonst ? std::move(sonst) : block({}, k);
        n = m;
    }
    return {std::move(an), n};
}

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::wennMitArt(std::size_t i, std::size_t b)
{
    // IF c THEN … [ELSIF|ELSEIF c THEN …]* [ELSE …] END IF
    AnweisungPtr erstes;
    modell::Anweisung* letztes = nullptr;
    std::size_t k = i;
    while (true)
    {
        const std::size_t dann = bisWort(k + 1, b, {"then"});
        if (dann >= b) m_q.meldeBei(Schwere::Warnung, k, "IF without THEN");
        AnweisungPtr an = neu(AnweisungArt::Wenn, k);
        an->text = text(k + 1, dann);
        ruft(*an, k + 1, dann);
        Folge zweig;
        k = blockFolge(dann + 1, b, {"elsif", "elseif", "else"}, zweig);
        an->kinder.push_back(block(std::move(zweig), dann));
        modell::Anweisung* jetzt = an.get();
        if (letztes == nullptr)
            erstes = std::move(an);
        else
            letztes->sonst = std::move(an);
        letztes = jetzt;
        if (wort(k, "elsif") || wort(k, "elseif")) continue;
        if (wort(k, "else"))
        {
            Folge sonst;
            const std::size_t s = k;
            k = blockFolge(k + 1, b, {}, sonst);
            letztes->sonst = block(std::move(sonst), s);
        }
        break;
    }
    if (wort(k, "end"))
        k = hinterEnde(k, b);
    else
        m_q.meldeBei(Schwere::Warnung, i, "IF without END IF");
    return {std::move(erstes), k};
}

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::wennFirebird(std::size_t i, std::size_t b)
{
    // IF (c) THEN anweisung [ELSE anweisung]
    const std::size_t dann = bisWort(i + 1, b, {"then"});
    AnweisungPtr an = neu(AnweisungArt::Wenn, i);
    an->text = text(i + 1, dann);
    ruft(*an, i + 1, dann);
    auto [ja, n] = anweisung(ohneTrenner(dann + 1, b), b);
    an->kinder.push_back(ja ? std::move(ja) : block({}, dann));
    const std::size_t k = ohneTrenner(n, b);
    if (k < b && wort(k, "else"))
    {
        auto [nein, m] = anweisung(ohneTrenner(k + 1, b), b);
        an->sonst = nein ? std::move(nein) : block({}, k);
        n = m;
    }
    return {std::move(an), n};
}

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::beginTsql(std::size_t i, std::size_t b)
{
    std::string marke = std::exchange(m_marke, {});
    if (wort(i + 1, "try"))
    {
        // BEGIN TRY … END TRY BEGIN CATCH … END CATCH
        AnweisungPtr versuch = neu(AnweisungArt::Versuch, i);
        versuch->kopf = "BEGIN TRY";
        Folge koerper;
        ++m_inTry;
        std::size_t k = folge(i + 2, b, {}, koerper);
        --m_inTry;
        versuch->kinder.push_back(block(std::move(koerper), i));
        k = wort(k, "end") ? hinterEnde(k, b) : erwarte(k, b, "end");
        k = ohneTrenner(k, b);
        if (wort(k, "begin") && wort(k + 1, "catch"))
        {
            AnweisungPtr fang = neu(AnweisungArt::Fang, k);
            fang->kopf = "BEGIN CATCH";
            Folge fangen;
            const std::size_t f = k;
            k = folge(k + 2, b, {}, fangen);
            fang->kinder.push_back(block(std::move(fangen), f));
            versuch->kinder.push_back(std::move(fang));
            k = wort(k, "end") ? hinterEnde(k, b) : erwarte(k, b, "end");
        }
        else
            m_q.meldeBei(Schwere::Warnung, i, "BEGIN TRY without BEGIN CATCH");
        return {std::move(versuch), k};
    }
    std::size_t k = i + 1;
    if (wort(k, "atomic"))
    {
        ++k;
        if (wort(k, "with") && tok(k + 1).istOp("(")) k = m_q.hinterKlammer(k + 1);  // In-Memory OLTP
    }
    Folge innen;
    k = folge(k, b, {}, innen);
    if (wort(k, "end"))
        k = hinterEnde(k, b);
    else
        m_q.meldeBei(Schwere::Warnung, i, "BEGIN without END");
    AnweisungPtr an = block(std::move(innen), i);
    an->marke = std::move(marke);
    return {std::move(an), k};
}

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::beginMitArt(std::size_t i, std::size_t b)
{
    std::string marke = std::exchange(m_marke, {});
    std::size_t k = i + 1;
    if (wort(k, "not") && wort(k + 1, "atomic")) k += 2;  // Spark
    if (wort(k, "atomic")) ++k;                            // BEGIN ATOMIC (Postgres, SQL/PSM)
    Folge innen;
    k = blockFolge(k, b, {"exception"}, innen);
    AnweisungPtr an = block(std::move(innen), i);
    if (wort(k, "exception"))
    {
        // PL/pgSQL: BEGIN … EXCEPTION WHEN x [OR y] THEN … [WHEN …] END
        AnweisungPtr versuch = neu(AnweisungArt::Versuch, i);
        versuch->kopf = "BEGIN";
        versuch->kinder.push_back(std::move(an));
        k = k + 1;
        while (wort(k, "when"))
        {
            const std::size_t dann = bisWort(k + 1, b, {"then"});
            AnweisungPtr fang = neu(AnweisungArt::Fang, k);
            fang->text = text(k + 1, dann);
            fang->kopf = "EXCEPTION WHEN " + fang->text;
            Folge fangen;
            k = blockFolge(dann + 1, b, {"when"}, fangen);
            fang->kinder.push_back(block(std::move(fangen), dann));
            versuch->kinder.push_back(std::move(fang));
        }
        Folge mitVersuch;
        mitVersuch.push_back(std::move(versuch));
        an = block(std::move(mitVersuch), i);
    }
    if (wort(k, "end"))
        k = hinterEnde(k, b);
    else
        m_q.meldeBei(Schwere::Warnung, i, "BEGIN without END");
    an->marke = std::move(marke);
    return {std::move(an), k};
}

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::deklaration(std::size_t i, std::size_t b)
{
    if (!m_plpgsql)
    {
        const std::size_t e = ende(i, b);
        return {einfach(i, e), e};
    }
    // PL/pgSQL: `DECLARE a int; b text := 'x'; BEGIN …` — je Deklaration eine Anweisung, die erste mit dem Wort
    // DECLARE wie in der Quelle; die Marke bleibt fuer das BEGIN dahinter
    Folge folge;
    std::size_t k = i;
    while (k < b && !wort(k, "begin") && tok(k).art != TokenArt::Ende)
    {
        const std::size_t name = k == i ? i + 1 : k;
        const std::size_t e = ende(name, b);
        AnweisungPtr an = neu(AnweisungArt::Einfach, k);
        an->text = text(k, e);
        ruft(*an, k, e);
        if (tok(name).art == TokenArt::Bezeichner) an->schreibt.push_back(klein(tok(name).text));
        for (std::size_t x = name + 1; x < e; ++x)
            if (tok(x).istOp(":=") || tok(x).istOp("=") || wort(x, "default"))
            {
                an->ziel = text(name, name + 1);
                an->wert = text(x + 1, e);
                break;
            }
        an->ruft = !an->aufrufe.empty();
        folge.push_back(std::move(an));
        const std::size_t weiter = ohneTrenner(e, b);
        k = weiter > k ? weiter : k + 1;  // nicht stehen bleiben
    }
    return {block(std::move(folge), i), k};
}

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::fall(std::size_t i, std::size_t b)
{
    // CASE [x] WHEN v [, w] THEN … [WHEN …] [ELSE …] END CASE — eine Anweisung, kein Ausdruck
    std::size_t k = i + 1;
    const bool gesucht = wort(k, "when");
    std::string gegenstand;
    if (!gesucht)
    {
        const std::size_t w = bisWort(k, b, {"when"});
        gegenstand = text(k, w);
        k = w;
    }
    // ohne ELSE wirft sie, wenn kein Zweig passt (CASE_NOT_FOUND)
    const auto keinZweig = [&](std::size_t bei) {
        AnweisungPtr wurf = neu(AnweisungArt::Wurf, bei);
        wurf->kopf = "CASE NOT FOUND (no ELSE)";
        return wurf;
    };
    if (gesucht)
    {
        AnweisungPtr erstes;
        modell::Anweisung* letztes = nullptr;
        bool sonst = false;
        while (wort(k, "when"))
        {
            const std::size_t dann = bisWort(k + 1, b, {"then"});
            AnweisungPtr an = neu(AnweisungArt::Wenn, k);
            an->text = text(k + 1, dann);
            ruft(*an, k + 1, dann);
            Folge zweig;
            k = blockFolge(dann + 1, b, {"when", "else"}, zweig);
            an->kinder.push_back(block(std::move(zweig), dann));
            modell::Anweisung* jetzt = an.get();
            if (letztes == nullptr)
                erstes = std::move(an);
            else
                letztes->sonst = std::move(an);
            letztes = jetzt;
        }
        if (letztes != nullptr && wort(k, "else"))
        {
            Folge nein;
            const std::size_t s = k;
            k = blockFolge(k + 1, b, {}, nein);
            letztes->sonst = block(std::move(nein), s);
            sonst = true;
        }
        if (letztes != nullptr && !sonst) letztes->sonst = keinZweig(i);
        k = wort(k, "end") ? hinterEnde(k, b) : erwarte(k, b, "end");
        if (!erstes) erstes = block({}, i);
        return {std::move(erstes), k};
    }
    AnweisungPtr auswahl = neu(AnweisungArt::Auswahl, i);
    auswahl->text = gegenstand;
    auswahl->kopf = "CASE " + gegenstand;
    auswahl->ohneDurchfall = true;
    ruft(*auswahl, i + 1, k);
    Folge faelle;
    bool sonst = false;
    while (wort(k, "when"))
    {
        // `WHEN 1, 2 THEN`: je Wert ein Fall, nebeneinander
        const std::size_t dann = bisWort(k + 1, b, {"then"});
        std::size_t von = k + 1;
        for (std::size_t j = k + 1; j <= dann;)
        {
            if (j < dann && tok(j).istOp("("))
            {
                j = std::min(m_q.hinterKlammer(j), dann);
                continue;
            }
            if (j == dann || tok(j).istOp(","))
            {
                AnweisungPtr f = neu(AnweisungArt::Fall, von);
                f->text = text(von, j);
                faelle.push_back(std::move(f));
                von = j + 1;
            }
            ++j;
        }
        k = blockFolge(dann + 1, b, {"when", "else"}, faelle);
    }
    if (wort(k, "else"))
    {
        faelle.push_back(neu(AnweisungArt::Fall, k));  // text leer: der Standardfall
        k = blockFolge(k + 1, b, {}, faelle);
        sonst = true;
    }
    if (!sonst)
    {
        faelle.push_back(neu(AnweisungArt::Fall, i));
        faelle.push_back(keinZweig(i));
    }
    k = wort(k, "end") ? hinterEnde(k, b) : erwarte(k, b, "end");
    auswahl->kinder.push_back(block(std::move(faelle), i));
    return {std::move(auswahl), k};
}

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::schleife(std::size_t i, std::size_t b)
{
    std::string marke = std::exchange(m_marke, {});
    const std::string w = klein(tok(i).text);
    AnweisungPtr an;
    std::size_t k = i;
    if (m_stil == RumpfStil::Tsql)
    {
        // WHILE c anweisung
        const std::size_t ce = bedingungsEnde(i + 1, b);
        an = neu(AnweisungArt::Solange, i);
        an->text = text(i + 1, ce);
        an->kopf = text(i, ce);
        ruft(*an, i + 1, ce);
        auto [rumpf, n] = anweisung(ohneTrenner(ce, b), b);
        an->kinder.push_back(rumpf ? std::move(rumpf) : block({}, ce));
        an->marke = std::move(marke);
        return {std::move(an), n};
    }
    if (m_stil == RumpfStil::Firebird && (w == "while" || w == "for"))
    {
        // WHILE (c) DO anweisung; FOR SELECT … INTO … DO anweisung
        const std::size_t tu = bisWort(i + 1, b, {"do"});
        an = neu(w == "while" ? AnweisungArt::Solange : AnweisungArt::FuerJedes, i);
        an->text = text(i + 1, tu);
        an->kopf = text(i, tu);
        ruft(*an, i + 1, tu);
        auto [rumpf, n] = anweisung(ohneTrenner(tu + 1, b), b);
        an->kinder.push_back(rumpf ? std::move(rumpf) : block({}, tu));
        an->marke = std::move(marke);
        return {std::move(an), n};
    }
    Folge rumpf;
    if (w == "loop")
    {
        an = neu(AnweisungArt::Solange, i);
        an->text = "true";
        an->kopf = "LOOP";
        k = blockFolge(i + 1, b, {}, rumpf);
    }
    else if (w == "repeat")
    {
        // REPEAT … UNTIL c END REPEAT: endet, wenn c wahr ist
        an = neu(AnweisungArt::TueSolange, i);
        k = blockFolge(i + 1, b, {"until"}, rumpf);
        if (wort(k, "until"))
        {
            const std::size_t bis = bisWort(k + 1, b, {"end"});
            an->text = text(k + 1, bis);
            an->kopf = "UNTIL " + an->text;
            an->zeile = zeile(k);
            ruft(*an, k + 1, bis);
            k = bis;
        }
        else
            m_q.meldeBei(Schwere::Warnung, i, "REPEAT without UNTIL");
        an->endetWennWahr = true;
    }
    else
    {
        // WHILE c LOOP|DO …; FOR … LOOP|DO …; FOREACH … LOOP …
        const std::size_t tu = bisWort(i + 1, b, {"loop", "do"});
        an = neu(w == "while" ? AnweisungArt::Solange : AnweisungArt::FuerJedes, i);
        an->text = text(i + 1, tu);
        an->kopf = text(i, tu);
        ruft(*an, i + 1, tu);
        k = blockFolge(tu + 1, b, {}, rumpf);
    }
    an->kinder.insert(an->kinder.begin(), block(std::move(rumpf), i));
    an->marke = std::move(marke);
    if (wort(k, "end"))
        k = hinterEnde(k, b);
    else
        m_q.meldeBei(Schwere::Warnung, i, "loop without END");
    return {std::move(an), k};
}

std::pair<modell::AnweisungPtr, std::size_t> RumpfLeser::sprung(std::size_t i, std::size_t b)
{
    const std::string w = klein(tok(i).text);
    const std::size_t e = ende(i, b);
    const auto mitWurf = [&](AnweisungArt art) {
        AnweisungPtr an = neu(art, i);
        an->kopf = text(i, e);
        an->text = text(i + 1, e);
        ruft(*an, i + 1, e);
        return std::pair{std::move(an), e};
    };
    if (w == "break" || w == "continue" || w == "exit" || w == "leave" || w == "iterate")
    {
        // [marke] [WHEN c]
        const bool abbruch = w == "break" || w == "exit" || w == "leave";
        AnweisungPtr sp = neu(abbruch ? AnweisungArt::Abbruch : AnweisungArt::Fortsetzung, i);
        std::size_t k = i + 1;
        if (k < e && tok(k).art == TokenArt::Bezeichner && !wort(k, "when"))
        {
            sp->text = std::string(tok(k).text);
            ++k;
        }
        sp->kopf = text(i, k);  // `EXIT aussen`, `LEAVE lesen`, `CONTINUE` — wie in der Quelle, ohne WHEN
        if (k < e && wort(k, "when"))
        {
            AnweisungPtr wenn = neu(AnweisungArt::Wenn, i);
            wenn->text = text(k + 1, e);
            ruft(*wenn, k + 1, e);
            wenn->kinder.push_back(std::move(sp));
            return {std::move(wenn), e};
        }
        return {std::move(sp), e};
    }
    if (w == "goto")
    {
        AnweisungPtr sp = neu(AnweisungArt::Sprung, i);
        sp->text = text(i + 1, e);
        return {std::move(sp), e};
    }
    if (w == "return")
    {
        if (wort(i + 1, "next") || wort(i + 1, "query")) return {einfach(i, e), e};  // PL/pgSQL: liefert, endet nicht
        return mitWurf(AnweisungArt::Ruecksprung);
    }
    if (w == "raise")
    {
        if (istName(i + 1) && inListe(kMeldeStufen, klein(tok(i + 1).text))) return {einfach(i, e), e};
        return mitWurf(AnweisungArt::Wurf);
    }
    if (w == "raiserror")
    {
        // RAISERROR (meldung, schwere, zustand) — wirft nur in einem TRY und ab Schwere 11 (sonst laeuft es weiter)
        std::size_t schwere = b;
        if (tok(i + 1).istOp("("))
        {
            int tiefe = 0;
            std::size_t kommas = 0;
            for (std::size_t k = i + 1; k < e; ++k)
            {
                if (tok(k).istOp("(")) ++tiefe;
                else if (tok(k).istOp(")")) --tiefe;
                else if (tiefe == 1 && tok(k).istOp(",") && ++kommas == 1) schwere = k + 1;
            }
        }
        const bool zahl = schwere < e && tok(schwere).art == TokenArt::Zahl &&
                          (tok(schwere + 1).istOp(",") || tok(schwere + 1).istOp(")"));
        if (!zahl)
        {
            ++raiserrorOhneZahl;
            m_q.meldeBei(Schwere::Hinweis, i,
                         "RAISERROR with a severity that is not a number is shown as a statement - it throws only in a "
                         "TRY from severity 11");
            return {einfach(i, e), e};
        }
        int wert = 0;
        for (const char c : tok(schwere).text)
            if (c >= '0' && c <= '9') wert = wert * 10 + (c - '0');
        if (m_inTry > 0 && wert >= 11) return mitWurf(AnweisungArt::Wurf);
        return {einfach(i, e), e};
    }
    // THROW, SIGNAL, RESIGNAL, Firebird EXCEPTION
    return mitWurf(AnweisungArt::Wurf);
}

}  // namespace c2d::sprache::sqlteile
