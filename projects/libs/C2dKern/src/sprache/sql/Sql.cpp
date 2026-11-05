/**
 * @file Sql.cpp
 * @brief SQL: die Sprachtabelle und der Leser fuer das Schema (Entscheide S1–S3, C2D_Session10)
 *
 * Der Text ist eine Folge von Anweisungen, getrennt von `;`, einer Zeile `GO` (T-SQL) oder dem
 * Trenner, den `DELIMITER` setzt (MySQL). Eine Routine (`CREATE TRIGGER`, `FUNCTION`, `PROCEDURE`)
 * mit `BEGIN … END` reicht bis zu ihrem `END` — die `;` darin trennen nicht. Gelesen wird heute das
 * Schema: `CREATE TABLE` (Spalten, Primaer- und Fremdschluessel) und `ALTER TABLE … ADD` (Schluessel,
 * Spalten). Views, Funktionen, Prozeduren und Trigger werden erkannt und gezaehlt, aber noch nicht
 * gezeigt (Hinweis); Abfragen und Daten (`SELECT`, `INSERT` …) gehoeren nicht zum Schema. Eine
 * Anweisung, deren erstes Wort der Leser nicht kennt, gibt eine Meldung.
 *
 * Namen in SQL kennen kein Gross/klein, ausser in Begrenzern (`"Album"`): ein Fremdschluessel findet
 * seine Tabelle ohne Ruecksicht darauf, und, wo das Schema fehlt, ueber den letzten Teil des Namens.
 *
 * Die Dialekte (Entscheid S3) unterscheidet der Leser noch nicht: was SQLite, Postgres, DuckDB,
 * MySQL und T-SQL im Schema gemeinsam haben, liest er gleich; die Wahl des Dialekts kommt mit dem SQL
 * in Zeichenketten anderer Sprachen (Entscheid S1).
 */
#include "c2d/sprache/Sprache.hpp"

#include "../Leser.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace c2d::sprache
{

namespace
{

using namespace c2d::modell;
using quelle::Schwere;

// Die Woerter, die der Lexer als Schluesselwort zeigt (klein; Sprachtabelle::schluesselwoerterOhneFall).
// Der Leser fragt nach jedem Wort ueber `wort()` — auch nach einem, das hier fehlt.
constexpr std::string_view kSchluesselwoerter[] = {
    "add",        "all",       "alter",     "and",       "as",        "asc",      "begin",     "between",
    "by",         "cascade",   "case",      "check",     "column",    "commit",   "constraint", "create",
    "cross",      "default",   "delete",    "desc",      "distinct",  "drop",     "else",      "end",
    "exists",     "foreign",   "from",      "full",      "function",  "group",    "having",    "if",
    "in",         "index",     "inner",     "insert",    "into",      "is",       "join",      "key",
    "left",       "like",      "limit",     "not",       "null",      "on",       "or",        "order",
    "outer",      "primary",   "procedure", "references", "replace",  "returns",  "right",     "rollback",
    "select",     "set",       "table",     "then",      "trigger",   "union",    "unique",    "update",
    "using",      "values",    "view",      "when",      "where",     "with",
};

constexpr std::string_view kOperatoren[] = {
    "->>", "#>>", "::", ":=", "=>", "->", "#>", "<>", "!=", "<=", ">=", "||", "<<", ">>", "(", ")",
    "[",   "]",   ",",  ";",  ".",  "+",  "-",  "*",  "/",  "%",  "=",  "<",  ">",  "!",  "~",  "&",
    "|",   "^",   "?",  ":",  "@",  "#",  "{",  "}",  "\\",
};

/// Vorsaetze einer Zeichenkette: `N'…'` (Unicode), `E'…'` (Postgres), `X'…'`, `B'…'`.
constexpr std::string_view kVorsaetze[] = {"n", "e", "x", "b"};

constexpr std::string_view kEndungen[] = {"sql", "ddl"};

constexpr lexer::Sprachtabelle kTabelle{
    .schluesselwoerter = kSchluesselwoerter,
    .operatoren = kOperatoren,
    .zeilenKommentar = "--",
    .blockKommentarAuf = "/*",
    .blockKommentarZu = "*/",
    .zeichenkettenBegrenzer = "'",
    .zeichenBegrenzer = "",
    .zeichenkettenVorsaetze = kVorsaetze,
    .namensZeichen = "@$",
    .verschachtelteKommentare = true,
    .schluesselwoerterOhneFall = true,
    .verdoppelteBegrenzer = true,
    .namensBegrenzer = "\"`[",
    .dollarZeichenketten = true,
};

/// Die ersten Woerter einer Anweisung, die der Leser kennt und die nichts zum Schema beitragen:
/// Abfragen und Daten, Rechte, Transaktionen, Einstellungen, Befehle der Werkzeuge — und was in
/// einer Routine ohne `BEGIN … END` als eigene Anweisung dasteht.
constexpr std::string_view kOhneSchema[] = {
    "analyze",  "attach",   "backup",   "begin",    "bulk",      "cache",      "call",      "checkpoint",
    "close",    "cluster",  "comment",  "commit",   "copy",      "deallocate", "declare",   "delete",
    "deny",     "describe", "desc",     "detach",   "discard",   "do",         "drop",      "else",
    "elseif",   "end",      "exec",     "execute",  "explain",   "export",     "fetch",     "for",
    "grant",    "if",       "import",   "insert",   "install",   "kill",       "list",      "listen",
    "load",     "lock",     "loop",     "merge",    "move",      "msck",       "notify",    "open",
    "optimize", "pragma",   "prepare",  "print",    "raise",     "raiserror",  "refresh",   "reindex",
    "release",  "repair",   "replace",  "reset",    "restore",   "return",     "revoke",    "rollback",
    "savepoint", "select",  "set",      "show",     "source",    "start",      "table",     "throw",
    "truncate", "uncache",  "update",   "upsert",   "use",       "vacuum",     "values",    "waitfor",
    "while",    "with",     "add",      "dbcc",     "enable",    "disable",    "go",        "unload",
    "from",     "pivot",    "unpivot",  "summarize", "transaction", "abort",     "map",       "reduce",
    // SQL*Plus (Oracle)
    "connect",  "exit",     "quit",     "spool",    "prompt",    "whenever",   "define",    "undefine",
    "host",     "rem",      "remark",
};

/// Was nach `CREATE` (und Woertern wie `OR REPLACE`, `TEMPORARY`) entsteht. `Tabelle` wird
/// gelesen; `Gezaehlt` wird erkannt und mit Hinweis noch nicht gezeigt; `Sonst` traegt nichts bei.
enum class Gebilde
{
    Tabelle,
    Gezaehlt,
    Sonst,
};

struct GebildeWort
{
    std::string_view wort;
    Gebilde gebilde;
    std::string_view mehrzahl;  ///< nur Gezaehlt: wie der Hinweis sie nennt
};

constexpr GebildeWort kGebilde[] = {
    {"table", Gebilde::Tabelle, {}},
    {"view", Gebilde::Gezaehlt, "views"},
    {"function", Gebilde::Gezaehlt, "functions"},
    {"procedure", Gebilde::Gezaehlt, "procedures"},
    {"proc", Gebilde::Gezaehlt, "procedures"},
    {"trigger", Gebilde::Gezaehlt, "triggers"},
    {"macro", Gebilde::Gezaehlt, "macros"},
    {"aggregate", Gebilde::Sonst, {}},
    {"assembly", Gebilde::Sonst, {}},
    {"cast", Gebilde::Sonst, {}},
    {"catalog", Gebilde::Sonst, {}},
    {"certificate", Gebilde::Sonst, {}},
    {"collation", Gebilde::Sonst, {}},
    {"connection", Gebilde::Sonst, {}},
    {"credential", Gebilde::Sonst, {}},
    {"database", Gebilde::Sonst, {}},
    {"domain", Gebilde::Sonst, {}},
    {"event", Gebilde::Sonst, {}},
    {"extension", Gebilde::Sonst, {}},
    {"index", Gebilde::Sonst, {}},
    {"key", Gebilde::Sonst, {}},
    {"language", Gebilde::Sonst, {}},
    {"login", Gebilde::Sonst, {}},
    {"operator", Gebilde::Sonst, {}},
    {"partition", Gebilde::Sonst, {}},
    {"policy", Gebilde::Sonst, {}},
    {"publication", Gebilde::Sonst, {}},
    {"queue", Gebilde::Sonst, {}},
    {"role", Gebilde::Sonst, {}},
    {"rule", Gebilde::Sonst, {}},
    {"schema", Gebilde::Sonst, {}},
    {"secret", Gebilde::Sonst, {}},
    {"security", Gebilde::Sonst, {}},
    {"sequence", Gebilde::Sonst, {}},
    {"server", Gebilde::Sonst, {}},
    {"service", Gebilde::Sonst, {}},
    {"statistics", Gebilde::Sonst, {}},
    {"subscription", Gebilde::Sonst, {}},
    {"synonym", Gebilde::Sonst, {}},
    {"tablespace", Gebilde::Sonst, {}},
    {"type", Gebilde::Sonst, {}},
    {"user", Gebilde::Sonst, {}},
    {"volume", Gebilde::Sonst, {}},
    // Databricks, Snowflake, BigQuery
    {"alert", Gebilde::Sonst, {}},
    {"integration", Gebilde::Sonst, {}},
    {"location", Gebilde::Sonst, {}},
    {"model", Gebilde::Sonst, {}},
    {"pipe", Gebilde::Sonst, {}},
    {"provider", Gebilde::Sonst, {}},
    {"recipient", Gebilde::Sonst, {}},
    {"share", Gebilde::Sonst, {}},
    {"stage", Gebilde::Sonst, {}},
    {"stream", Gebilde::Sonst, {}},
    {"tag", Gebilde::Sonst, {}},
    {"task", Gebilde::Sonst, {}},
    {"warehouse", Gebilde::Sonst, {}},
};

/// Woerter, mit denen in einer Spaltendefinition die Bedingungen beginnen — davor steht der Typ.
constexpr std::string_view kSpaltenBedingungen[] = {
    "as",       "auto_increment", "autoincrement", "check",      "collate",  "comment", "constraint",
    "default",  "generated",      "identity",      "not",        "null",     "on",      "primary",
    "references", "rowguidcol",   "sparse",        "unique",     "masked",   "encrypted", "filestream",
};

/// Woerter, mit denen in der Liste von `CREATE TABLE` eine Bedingung der Tabelle beginnt.
constexpr std::string_view kTabellenBedingungen[] = {
    "constraint", "primary", "foreign", "unique", "check", "exclude", "fulltext", "spatial", "period", "like",
};

bool enthaelt(std::span<const std::string_view> liste, std::string_view wort)
{
    return std::find(liste.begin(), liste.end(), wort) != liste.end();
}

std::string klein(std::string_view text)
{
    std::string s(text);
    for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return s;
}

/// Ein Name ohne seine Begrenzer: `"Album"` -> `Album`, `[dbo]` -> `dbo`, `` `x` `` -> `x`
/// (ein verdoppelter Begrenzer darin wird einer).
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

class SqlLeser final : public Leser
{
public:
    SqlLeser(std::string_view text, Ergebnis& aus) : Leser(text, kTabelle, aus) {}

    void liesAlles()
    {
        while (!amEnde(m_p))
        {
            const std::size_t a = m_p;
            if (wort(a, "delimiter"))
            {
                liesTrenner(a);
                continue;
            }
            if (const std::size_t n = nachWerkzeugbefehl(a); n > a)
            {
                m_p = n;
                continue;
            }
            const std::size_t b = anweisungsEnde(a);
            const std::size_t trenner = trennerLaenge(b);  // vor dem Lesen: SET TERM setzt einen neuen
            if (b > a) liesAnweisung(a, b);
            m_p = b + trenner;
            if (m_p == a) ++m_p;  // nicht stehen bleiben
        }
        loeseVerweiseAuf();
        for (const auto& [mehrzahl, zahl] : m_gezaehlt)
        {
            // die Mehrzahl endet auf `s`: bei einem die Einzahl
            const std::string_view art = zahl.first == 1 ? mehrzahl.substr(0, mehrzahl.size() - 1) : mehrzahl;
            m_aus.meldungen.push_back({Schwere::Hinweis, zahl.second,
                                       std::to_string(zahl.first) + " " + std::string(art) + " read but not shown yet"});
        }
    }

private:
    // --- Token ---------------------------------------------------------------

    /// Ist das Token i dieses Wort (klein, ohne Ruecksicht auf Gross/klein, nicht in Begrenzern)?
    [[nodiscard]] bool wort(std::size_t i, std::string_view w) const
    {
        const Token& t = tok(i);
        return (t.art == TokenArt::Schluesselwort || t.art == TokenArt::Bezeichner) && t.text.size() == w.size() &&
               klein(t.text) == w;
    }

    [[nodiscard]] bool istName(std::size_t i) const
    {
        const TokenArt a = tok(i).art;
        return a == TokenArt::Bezeichner || a == TokenArt::Schluesselwort;
    }

    /// Beginnt bei i in der Liste von `CREATE TABLE` (oder hinter `ADD`) eine Bedingung der Tabelle,
    /// keine Spalte? `KEY` und `INDEX` (MySQL) nur, wenn eine Klammer folgt — sonst heisst eine Spalte so.
    [[nodiscard]] bool beginntBedingung(std::size_t i) const
    {
        if (!istName(i) || std::string_view("\"`[").find(tok(i).text.front()) != std::string_view::npos) return false;
        const std::string w = klein(tok(i).text);
        if (enthaelt(kTabellenBedingungen, w)) return true;
        return (w == "key" || w == "index") && (tok(i + 1).istOp("(") || tok(i + 2).istOp("("));
    }

    /// Wo die Zeile z beginnt (das erste Token in ihr) — fuer Meldungen nach dem Lesen.
    [[nodiscard]] quelle::Position ortDerZeile(int z) const
    {
        for (const Token& t : m_t)
            if (t.ort.zeile >= z) return t.ort;
        return {0, z, 1};
    }

    /// Steht das Token i allein in seiner Zeile `GO` (T-SQL, auch `GO 5`)?
    [[nodiscard]] bool goZeile(std::size_t i) const
    {
        if (!wort(i, "go")) return false;
        const int z = tok(i).ort.zeile;
        if (i > 0 && tok(i - 1).ort.zeile == z) return false;
        std::size_t n = i + 1;
        if (tok(n).art == TokenArt::Zahl && tok(n).ort.zeile == z) ++n;
        return amEnde(n) || tok(n).ort.zeile != z;
    }

    /// Wie viele Token bei i den Trenner bilden (`;`, `;;`, `//` …, eine Zeile `GO`); 0 = keiner.
    [[nodiscard]] std::size_t trennerLaenge(std::size_t i) const
    {
        if (goZeile(i)) return tok(i + 1).art == TokenArt::Zahl && tok(i + 1).ort.zeile == tok(i).ort.zeile ? 2 : 1;
        std::string s;
        for (std::size_t k = i; !amEnde(k) && s.size() < m_trenner.size(); ++k)
        {
            if (k > i && tok(k).abstandDavor) return 0;
            s += tok(k).text;
            if (s == m_trenner) return k - i + 1;
        }
        return 0;
    }

    /// `DELIMITER x` (MySQL): der Rest der Zeile ist der neue Trenner.
    void liesTrenner(std::size_t a)
    {
        const int z = tok(a).ort.zeile;
        std::size_t k = a + 1;
        std::string trenner;
        while (!amEnde(k) && tok(k).ort.zeile == z) trenner += tok(k++).text;
        if (trenner.empty())
            melde(Schwere::Warnung, tok(a), "DELIMITER without a delimiter");
        else
            m_trenner = trenner;
        m_p = k;
    }

    /// `BEGIN` bei j beginnt eine Transaktion, keinen Block (`BEGIN;`, `BEGIN TRANSACTION`, `BEGIN WORK` …).
    [[nodiscard]] bool beginntTransaktion(std::size_t j) const
    {
        return tok(j + 1).istOp(";") || amEnde(j + 1) || wort(j + 1, "transaction") || wort(j + 1, "tran") ||
               wort(j + 1, "work") || wort(j + 1, "distributed") || wort(j + 1, "immediate") ||
               wort(j + 1, "deferred") || wort(j + 1, "exclusive") || wort(j + 1, "isolation");
    }

    /// Oeffnet das Wort bei j in einem Block eine Stufe, die ein `END` schliesst? `BEGIN` (keine
    /// Transaktion) und `CASE`. Ein `IF`, `WHILE`, `LOOP` … zaehlt nicht: MySQL, PL/pgSQL und Spark
    /// schliessen es mit `END IF`, `END WHILE` …, Firebird und T-SQL gar nicht (`IF … THEN BEGIN … END`) —
    /// darum schliesst ein `END IF` keine Stufe (anweisungsEnde).
    [[nodiscard]] bool oeffnetStufe(std::size_t j) const
    {
        if (wort(j, "begin")) return !beginntTransaktion(j);
        return wort(j, "case");
    }

    /// `END` bei j schliesst eine Schleife oder ein IF (`END IF`, `END LOOP`, `END WHILE`, `END REPEAT`,
    /// `END FOR`), keine Stufe.
    [[nodiscard]] bool schliesstSchleife(std::size_t j) const
    {
        return wort(j + 1, "if") || wort(j + 1, "loop") || wort(j + 1, "while") || wort(j + 1, "repeat") ||
               wort(j + 1, "for");
    }

    /// Steht bei a eine Anweisung, in der Bloecke stehen koennen — eine Routine (das erste Gebilde
    /// nach CREATE/ALTER ist TRIGGER, FUNCTION, PROCEDURE, EVENT, nicht eine Tabelle `event`), ein
    /// Block auf oberster Ebene (`BEGIN … END`, Spark und T-SQL), auch mit Marke (`L0: BEGIN`), eine
    /// Schleife oder ein IF?
    [[nodiscard]] bool mitBloecken(std::size_t a) const
    {
        if (wort(a, "create") || wort(a, "alter"))
        {
            for (std::size_t k = a + 1; k < a + 12 && !amEnde(k) && !tok(k).istOp("("); ++k)
            {
                if (wort(k, "trigger") || wort(k, "function") || wort(k, "procedure") || wort(k, "proc") ||
                    wort(k, "event"))
                    return true;
                if (istName(k) && std::any_of(std::begin(kGebilde), std::end(kGebilde),
                                              [&](const GebildeWort& g) { return wort(k, g.wort); }))
                    return false;
            }
            return false;
        }
        if (tok(a).art == TokenArt::Bezeichner && tok(a + 1).istOp(":")) return oeffnetStufe(a + 2);  // Marke
        // ein Block, auch hinter IF und WHILE auf oberster Ebene (T-SQL: `IF … BEGIN … END`)
        return (wort(a, "begin") && !beginntTransaktion(a)) || wort(a, "if") || wort(a, "while");
    }

    /// Ab a das Ende der Anweisung: der Trenner, eine Zeile `GO` oder das Ende. In einer Anweisung mit
    /// Bloecken (mitBloecken) zaehlt, was eine Stufe oeffnet (oeffnetStufe), eine auf und jedes `END` eine
    /// ab, ausser es schliesst eine Schleife (schliesstSchleife) — samt dem Wort dahinter in seiner Zeile
    /// (`END IF`, `END marke`); ein Trenner auf einer Stufe darueber trennt nicht. In einer Klammer steht
    /// in SQL kein Trenner: einer dort beendet die Anweisung trotzdem, mit einer Meldung — sonst liefe
    /// eine offene Klammer bis ans Ende des Texts.
    std::size_t anweisungsEnde(std::size_t a)
    {
        const bool bloecke = mitBloecken(a);
        int tiefe = 0;
        std::vector<std::size_t> klammern;  // die offenen, innen zuletzt
        const auto ende = [&](std::size_t j) {
            if (!klammern.empty())
                melde(Schwere::Fehler, tok(klammern.front()), "'" + std::string(tok(klammern.front()).text) + "' is not closed");
            return j;
        };
        std::size_t j = a;
        while (!amEnde(j))
        {
            if (j > a && goZeile(j)) return ende(j);
            if (tiefe == 0 && trennerLaenge(j) > 0) return ende(j);
            // T-SQL braucht kein `;`: CREATE, ALTER, DROP am Anfang einer Zeile beginnen eine neue Anweisung
            if (j > a && tiefe == 0 && klammern.empty() && tok(j - 1).ort.zeile != tok(j).ort.zeile &&
                (wort(j, "create") || wort(j, "alter") || wort(j, "drop")))
                return j;
            if (oeffnet(tok(j)))
            {
                klammern.push_back(j++);
                continue;
            }
            if (schliesst(tok(j)))
            {
                if (!klammern.empty()) klammern.pop_back();
                ++j;
                continue;
            }
            if (bloecke && klammern.empty())
            {
                if (wort(j, "end") && tiefe > 0)
                {
                    const bool schleife = schliesstSchleife(j);
                    if (!schleife) --tiefe;
                    // das Wort dahinter gehoert zum END: eine Marke, IF, LOOP …, CASE, TRY, CATCH (T-SQL)
                    const bool dazu = schleife || wort(j + 1, "case") || wort(j + 1, "try") || wort(j + 1, "catch") ||
                                      (tok(j + 1).art == TokenArt::Bezeichner && !goZeile(j + 1));
                    if (dazu && tok(j + 1).ort.zeile == tok(j).ort.zeile) ++j;
                }
                else if (oeffnetStufe(j))
                    ++tiefe;
            }
            ++j;
        }
        return ende(j);
    }

    /// Bei a ein Befehl eines Werkzeugs, keine Anweisung: `\connect` (psql), `:r datei` (SQLCMD) —
    /// am Anfang einer Zeile bis zu ihrem Ende. Die Stelle dahinter; a = keiner.
    [[nodiscard]] std::size_t nachWerkzeugbefehl(std::size_t a) const
    {
        const bool zeilenanfang = a == 0 || tok(a - 1).ort.zeile != tok(a).ort.zeile;
        if (!zeilenanfang || !(tok(a).istOp("\\") || (tok(a).istOp(":") && istName(a + 1)))) return a;
        std::size_t k = a + 1;
        while (!amEnde(k) && tok(k).ort.zeile == tok(a).ort.zeile) ++k;
        return k;
    }

    /// Ab i ein Name, auch mit Schema (`public.actor`, `[dbo].[Album]`): wie in der Quelle, ohne
    /// Begrenzer, mit `.`; das Ende in `ende`.
    std::string liesNamen(std::size_t i, std::size_t& ende) const
    {
        // Spark, Databricks: `IDENTIFIER('schema.tab')` nennt den Namen in einer Zeichenkette
        if (wort(i, "identifier") && tok(i + 1).istOp("(") && tok(i + 2).art == TokenArt::Zeichenkette &&
            tok(i + 3).istOp(")"))
        {
            const std::string_view s = tok(i + 2).text;
            ende = i + 4;
            return s.size() >= 2 && s.front() == '\'' ? std::string(s.substr(1, s.size() - 2)) : std::string(s);
        }
        std::string name;
        while (istName(i))
        {
            name += ohneBegrenzer(tok(i).text);
            if (!tok(i + 1).istOp(".") || !istName(i + 2)) break;
            name += '.';
            i += 2;
        }
        ende = istName(i) ? i + 1 : i;
        return name;
    }

    /// Die Namen in einer Klammer `( a, b DESC, c(10) )` ab i (auf der Klammer): je Teil das erste Wort.
    std::vector<std::string> liesNamensliste(std::size_t i, std::size_t& ende)
    {
        std::vector<std::string> namen;
        if (!tok(i).istOp("("))
        {
            ende = i;
            return namen;
        }
        ende = ueberKlammer(i);
        for (const auto& [a, b] : teile(i + 1, ende - 1))
            if (istName(a)) namen.push_back(ohneBegrenzer(tok(a).text));
        return namen;
    }

    // --- Anweisungen ---------------------------------------------------------

    void liesAnweisung(std::size_t a, std::size_t b)
    {
        if (wort(a, "create"))
        {
            liesCreate(a, b);
            return;
        }
        if (wort(a, "alter"))
        {
            if (wort(a + 1, "table")) liesAlterTable(a + 2, b);
            return;  // alles andere aendert kein Schema, das gezeigt wird
        }
        if (wort(a, "set") && wort(a + 1, "term") && b > a + 2)
        {
            // Firebird: `SET TERM ^ ;` setzt den Trenner (wie DELIMITER)
            m_trenner.clear();
            for (std::size_t k = a + 2; k < b; ++k) m_trenner += tok(k).text;
            return;
        }
        const Token& t = tok(a);
        if (t.art == TokenArt::Operator && (t.text == "(" || t.text == "{"))
            return;  // eine Abfrage in Klammern, ein Block ohne Wort
        if (t.art == TokenArt::Bezeichner && tok(a + 1).istOp(":")) return;  // ein Block mit Marke (Spark)
        if ((t.art == TokenArt::Schluesselwort || t.art == TokenArt::Bezeichner) &&
            enthaelt(kOhneSchema, klein(t.text)))
            return;
        melde(Schwere::Warnung, t, "statement '" + std::string(t.text) + "' is not understood");
    }

    void liesCreate(std::size_t a, std::size_t b)
    {
        for (std::size_t k = a + 1; k < b; ++k)
        {
            if (!istName(k)) continue;
            const std::string w = klein(tok(k).text);
            const auto g = std::find_if(std::begin(kGebilde), std::end(kGebilde),
                                        [&](const GebildeWort& x) { return x.wort == w; });
            if (g == std::end(kGebilde)) continue;
            if (g->gebilde == Gebilde::Tabelle)
                liesCreateTable(k + 1, b);
            else if (g->gebilde == Gebilde::Gezaehlt)
            {
                auto& [zahl, ort] = m_gezaehlt[g->mehrzahl];
                if (zahl++ == 0) ort = tok(a).ort;
            }
            return;
        }
        melde(Schwere::Warnung, tok(a), "CREATE statement is not understood");
    }

    /// Ab i (hinter `TABLE`): `[IF NOT EXISTS] name ( Spalten und Bedingungen ) …`.
    void liesCreateTable(std::size_t i, std::size_t b)
    {
        if (wort(i, "if") && wort(i + 1, "not") && wort(i + 2, "exists")) i += 3;
        std::size_t k = i;
        Typ typ;
        typ.art = TypArt::Tabelle;
        typ.name = liesNamen(i, k);
        typ.zeile = tok(i).ort.zeile;
        if (typ.name.empty())
        {
            melde(Schwere::Warnung, tok(i), "table name is missing");
            return;
        }
        if (!tok(k).istOp("("))
        {
            // AS SELECT, LIKE, CLONE, USING ohne Spalten …: woher die Spalten kommen, steht nicht da
            melde(Schwere::Hinweis, tok(i), "columns of table '" + typ.name + "' are not listed in its CREATE TABLE");
            m_aus.einheit.typen.push_back(std::move(typ));
            return;
        }
        const std::size_t zu = ueberKlammer(k);
        std::vector<std::string> primaer;
        for (const auto& [x, y] : teile(k + 1, zu - 1))
        {
            if (beginntBedingung(x))
                liesTabellenBedingung(x, y, typ, primaer);
            else
                liesSpalte(x, y, typ, primaer);
        }
        // was hinter der Klammer steht (USING, PARTITIONED BY, WITH …), traegt nichts bei
        (void)b;
        markiereSchluessel(typ, primaer);
        m_aus.einheit.typen.push_back(std::move(typ));
    }

    /// Eine Spalte `[x, y)`: Name, Typ bis zur ersten Bedingung, dann die Bedingungen.
    void liesSpalte(std::size_t x, std::size_t y, Typ& typ, std::vector<std::string>& primaer)
    {
        if (!istName(x))
        {
            melde(Schwere::Warnung, tok(x), "column definition is not understood");
            return;
        }
        Feld feld;
        feld.name = ohneBegrenzer(tok(x).text);
        feld.zeile = tok(x).ort.zeile;
        feld.typHinten = true;
        std::size_t k = x + 1;
        while (k < y && !(istName(k) && enthaelt(kSpaltenBedingungen, klein(tok(k).text))))
            k = oeffnet(tok(k)) ? nachKlammer(k) : k + 1;
        feld.typ = textVon(x + 1, k);
        std::string bedingung;  // der Name aus `CONSTRAINT name` vor der naechsten Bedingung
        while (k < y)
        {
            if (wort(k, "constraint"))
            {
                bedingung = ohneBegrenzer(tok(k + 1).text);
                k += 2;
                continue;
            }
            if (wort(k, "primary") && wort(k + 1, "key"))
            {
                primaer.push_back(feld.name);
                k += 2;
                continue;
            }
            if (wort(k, "references"))
            {
                Verweis v;
                v.felder.push_back(feld.name);
                v.name = bedingung;
                v.zeile = tok(k).ort.zeile;
                std::size_t e = k + 1;
                v.ziel = liesNamen(k + 1, e);
                v.zielFelder = liesNamensliste(e, e);
                typ.verweise.push_back(std::move(v));
                bedingung.clear();
                k = e;
                continue;
            }
            k = oeffnet(tok(k)) ? nachKlammer(k) : k + 1;
        }
        typ.felder.push_back(std::move(feld));
    }

    /// Eine Bedingung der Tabelle `[x, y)`: `[CONSTRAINT n] PRIMARY KEY (…)`, `FOREIGN KEY (…)
    /// REFERENCES t (…)`; die anderen (UNIQUE, CHECK, INDEX …) tragen nichts bei.
    void liesTabellenBedingung(std::size_t x, std::size_t y, Typ& typ, std::vector<std::string>& primaer)
    {
        std::string name;
        if (wort(x, "constraint"))
        {
            name = ohneBegrenzer(tok(x + 1).text);
            x += 2;
        }
        if (wort(x, "primary") && wort(x + 1, "key"))
        {
            std::size_t k = x + 2;
            while (k < y && !tok(k).istOp("(")) ++k;  // CLUSTERED, NONCLUSTERED (T-SQL)
            for (std::string& s : liesNamensliste(k, k)) primaer.push_back(std::move(s));
            return;
        }
        if (wort(x, "foreign") && wort(x + 1, "key"))
        {
            std::size_t k = x + 2;
            Verweis v;
            v.name = std::move(name);
            v.zeile = tok(x).ort.zeile;
            if (istName(k) && tok(k + 1).istOp("(")) ++k;  // MySQL: `FOREIGN KEY name (…)`
            v.felder = liesNamensliste(k, k);
            if (!wort(k, "references"))
            {
                melde(Schwere::Warnung, tok(x), "FOREIGN KEY without REFERENCES");
                return;
            }
            v.ziel = liesNamen(k + 1, k);
            v.zielFelder = liesNamensliste(k, k);
            typ.verweise.push_back(std::move(v));
        }
    }

    /// `ALTER TABLE [ONLY] [IF EXISTS] name …` ab i: je Teil `ADD [CONSTRAINT n] PRIMARY KEY …`,
    /// `ADD … FOREIGN KEY …`, `ADD [COLUMN] spalte typ …`; das Uebrige aendert nichts Gezeigtes.
    void liesAlterTable(std::size_t i, std::size_t b)
    {
        if (wort(i, "only")) ++i;
        if (wort(i, "if") && wort(i + 1, "exists")) i += 2;
        std::size_t k = i;
        const std::string name = liesNamen(i, k);
        Typ* typ = tabelle(name);
        if (typ == nullptr)
        {
            melde(Schwere::Hinweis, tok(i), "ALTER TABLE: table '" + name + "' is not defined in this file");
            return;
        }
        std::vector<std::string> primaer;
        for (const auto& [x0, y] : teile(k, b))
        {
            std::size_t x = x0;
            while (x < y && (wort(x, "with") || wort(x, "check") || wort(x, "nocheck"))) ++x;  // T-SQL
            if (!wort(x, "add")) continue;
            ++x;
            if (beginntBedingung(x))
                liesTabellenBedingung(x, y, *typ, primaer);
            else
            {
                if (wort(x, "column")) ++x;
                if (wort(x, "if") && wort(x + 1, "not") && wort(x + 2, "exists")) x += 3;
                liesSpalte(x, y, *typ, primaer);
            }
        }
        markiereSchluessel(*typ, primaer);
    }

    // --- Schluessel ----------------------------------------------------------

    /// Die Tabelle dieses Namens unter den gelesenen (ohne Ruecksicht auf Gross/klein; wo das Schema
    /// fehlt, ueber den letzten Teil, wenn er eindeutig ist); nullptr = keine.
    Typ* tabelle(std::string_view name)
    {
        const std::string gesucht = klein(name);
        Typ* letzterTeil = nullptr;
        int treffer = 0;
        for (Typ& t : m_aus.einheit.typen)
        {
            if (t.art != TypArt::Tabelle) continue;
            const std::string n = klein(t.name);
            if (n == gesucht) return &t;
            const std::size_t punkt = n.find_last_of('.');
            const std::size_t gpunkt = gesucht.find_last_of('.');
            const std::string_view a = punkt == std::string::npos ? n : std::string_view(n).substr(punkt + 1);
            const std::string_view g =
                gpunkt == std::string::npos ? std::string_view(gesucht) : std::string_view(gesucht).substr(gpunkt + 1);
            if (a == g && (punkt == std::string::npos || gpunkt == std::string::npos))
            {
                letzterTeil = &t;
                ++treffer;
            }
        }
        return treffer == 1 ? letzterTeil : nullptr;
    }

    /// Schreibt `PK` und `FK` in den Zusatz der Spalten (ohne Ruecksicht auf Gross/klein).
    static void markiereSchluessel(Typ& typ, const std::vector<std::string>& primaer)
    {
        for (Feld& f : typ.felder)
        {
            const std::string n = klein(f.name);
            const bool pk = std::any_of(primaer.begin(), primaer.end(), [&](const std::string& p) { return klein(p) == n; }) ||
                            f.zusatz.find("PK") != std::string::npos;
            const bool fk = std::any_of(typ.verweise.begin(), typ.verweise.end(), [&](const Verweis& v) {
                return std::any_of(v.felder.begin(), v.felder.end(), [&](const std::string& s) { return klein(s) == n; });
            });
            f.zusatz = pk && fk ? "PK, FK" : pk ? "PK" : fk ? "FK" : "";
        }
    }

    /// Jeder Fremdschluessel nennt seine Tabelle so, wie sie heisst — wo sie in der Datei steht.
    void loeseVerweiseAuf()
    {
        for (std::size_t t = 0; t < m_aus.einheit.typen.size(); ++t)
            for (Verweis& v : m_aus.einheit.typen[t].verweise)
            {
                if (const Typ* ziel = tabelle(v.ziel); ziel != nullptr)
                    v.ziel = ziel->name;
                else
                    m_aus.meldungen.push_back({Schwere::Hinweis, ortDerZeile(v.zeile),
                                               "referenced table '" + v.ziel + "' is not defined in this file"});
            }
    }

    std::string m_trenner = ";";
    /// Je Art, die erkannt, aber noch nicht gezeigt wird: wie oft, und wo zuerst.
    std::map<std::string_view, std::pair<int, quelle::Position>> m_gezaehlt;
};

class Sql final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "SQL"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungen; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return kTabelle; }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& /*optionen*/) const override
    {
        Ergebnis ergebnis;
        ergebnis.einheit.name = std::string(einheitName);
        SqlLeser(text, ergebnis).liesAlles();
        return ergebnis;
    }
};

}  // namespace

const Sprache& sql()
{
    static const Sql sprache;
    return sprache;
}

}  // namespace c2d::sprache
