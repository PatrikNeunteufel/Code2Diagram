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
#include "Abfrage.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
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

/// T-SQL: wie der Kern, dazu `#t` und `##t` (temporaere Tabellen) als Namen.
constexpr lexer::Sprachtabelle kTabelleTsql{
    .schluesselwoerter = kSchluesselwoerter,
    .operatoren = kOperatoren,
    .zeilenKommentar = "--",
    .blockKommentarAuf = "/*",
    .blockKommentarZu = "*/",
    .zeichenkettenBegrenzer = "'",
    .zeichenBegrenzer = "",
    .zeichenkettenVorsaetze = kVorsaetze,
    .namensZeichen = "@$#",
    .verschachtelteKommentare = true,
    .schluesselwoerterOhneFall = true,
    .verdoppelteBegrenzer = true,
    .namensBegrenzer = "\"`[",
    .dollarZeichenketten = true,
};

/// MySQL und Spark (Databricks): `'…'` und `"…"` sind Zeichenketten, `\` maskiert; Namen nur in Gravis.
constexpr lexer::Sprachtabelle kTabelleMitRueckstrich{
    .schluesselwoerter = kSchluesselwoerter,
    .operatoren = kOperatoren,
    .zeilenKommentar = "--",
    .blockKommentarAuf = "/*",
    .blockKommentarZu = "*/",
    .zeichenkettenBegrenzer = "'\"",
    .zeichenBegrenzer = "",
    .zeichenkettenVorsaetze = kVorsaetze,
    .namensZeichen = "@$",
    .verschachtelteKommentare = true,
    .schluesselwoerterOhneFall = true,
    .verdoppelteBegrenzer = true,
    .escapeMitRueckstrich = true,
    .namensBegrenzer = "`",
};

/// Ein Dialekt (Entscheid S3): wie der Text zerlegt wird und ob Schleifen und IF ein eigenes END haben.
struct Dialekt
{
    std::string_view name;
    const lexer::Sprachtabelle* tabelle;
    /// `IF … THEN … END IF`, `WHILE … DO … END WHILE`, `LOOP … END LOOP`, `REPEAT … END REPEAT` (MySQL,
    /// Spark): sie oeffnen eine Stufe, die ihr END schliesst. Sonst nur BEGIN und CASE (T-SQL, Firebird).
    bool endeMitArt;
};

constexpr Dialekt kKern{"", &kTabelle, false};

constexpr Dialekt kDialekte[] = {
    {"sqlite", &kTabelle, false},
    {"postgres", &kTabelle, false},  // Routinen in $$…$$: ihr Rumpf ist eine Zeichenkette
    {"duckdb", &kTabelle, false},
    {"spark", &kTabelleMitRueckstrich, true},
    {"mysql", &kTabelleMitRueckstrich, true},
    {"tsql", &kTabelleTsql, false},
};

constexpr std::string_view kDialektNamen[] = {"sqlite", "postgres", "duckdb", "spark", "mysql", "tsql"};

/// Eine Zeile, die ein Werkzeug an den Anfang der Datei schreibt und die den Dialekt belegt (Entscheid E2):
/// im Kommentar vor der ersten Anweisung.
struct Kopf
{
    std::string_view zeile;
    std::string_view dialekt;
};

constexpr Kopf kKoepfe[] = {
    {"-- Databricks notebook source", "spark"},
    {"-- PostgreSQL database dump", "postgres"},
};

/// Der Dialekt, den ein Kopf der Datei belegt; leer = keiner.
std::string_view dialektAusKopf(std::string_view text)
{
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
    while (!text.empty())
    {
        const std::size_t ende = text.find('\n');
        std::string_view zeile = text.substr(0, ende);
        while (!zeile.empty() && (zeile.back() == '\r' || zeile.back() == ' ' || zeile.back() == '\t')) zeile.remove_suffix(1);
        while (!zeile.empty() && (zeile.front() == ' ' || zeile.front() == '\t')) zeile.remove_prefix(1);
        if (!zeile.empty() && !zeile.starts_with("--")) return {};  // die erste Anweisung
        for (const Kopf& k : kKoepfe)
            if (zeile == k.zeile) return k.dialekt;
        if (ende == std::string_view::npos) break;
        text.remove_prefix(ende + 1);
    }
    return {};
}

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
    Objekt,  ///< ein Datenbankobjekt (View, Trigger, Funktion, Prozedur): seine Zugriffe werden gelesen
    Sonst,
};

struct GebildeWort
{
    std::string_view wort;
    Gebilde gebilde;
    ObjektArt objekt;  ///< nur Objekt
};

constexpr GebildeWort kGebilde[] = {
    {"table", Gebilde::Tabelle, {}},
    {"view", Gebilde::Objekt, ObjektArt::View},
    {"function", Gebilde::Objekt, ObjektArt::Funktion},
    {"procedure", Gebilde::Objekt, ObjektArt::Prozedur},
    {"proc", Gebilde::Objekt, ObjektArt::Prozedur},
    {"trigger", Gebilde::Objekt, ObjektArt::Trigger},
    {"macro", Gebilde::Objekt, ObjektArt::Funktion},  // DuckDB
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

class SqlLeser final : public Leser, public sqlteile::Tokenquelle
{
public:
    SqlLeser(std::string_view text, const Dialekt& dialekt, std::string platzhalter, Ergebnis& aus)
        : Leser(text, *dialekt.tabelle, aus), m_dialekt(dialekt), m_platzhalter(std::move(platzhalter))
    {
    }

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
    }

    /// Nur die Zugriffe aller Anweisungen, nach `zugriffe` (fuer den Rumpf einer Routine in `$$…$$`):
    /// temporaere Tabellen, die er anlegt, nach `eigene`; die Abfragen nach Einheit::abfragen.
    void liesRumpf(std::vector<Zugriff>& zugriffe, std::vector<std::string>& eigene)
    {
        while (!amEnde(m_p))
        {
            const std::size_t a = m_p;
            const std::size_t b = anweisungsEnde(a);
            const std::size_t trenner = trennerLaenge(b);
            if (b > a) sammleZugriffe(a, b, zugriffe, eigene);
            if (b > a && !m_routine.empty()) abfragenIn(a, b, m_routine);
            m_p = b + trenner;
            if (m_p == a) ++m_p;
        }
    }

    // --- Tokenquelle (fuer den Zerleger der Abfragen) ---------------------------

    [[nodiscard]] const Token& token(std::size_t i) const override { return tok(i); }
    [[nodiscard]] std::string quelltext(std::size_t a, std::size_t b) const override { return textVon(a, b); }
    [[nodiscard]] std::size_t hinterKlammer(std::size_t i) const override { return nachKlammer(i); }

private:
    /// Jede Abfrage in [a, b) (eine Anweisung, ein Rumpf) nach Einheit::abfragen, je `vorn@zeile`.
    void abfragenIn(std::size_t a, std::size_t b, const std::string& vorn)
    {
        sqlteile::AbfrageLeser leser(*this);
        leser.selectIntoTabelle = m_dialekt.name == "tsql";
        for (Abfrage& x : leser.suche(a, b, vorn)) m_aus.einheit.abfragen.push_back(std::move(x));
    }

    /// Die Anweisung [a, b) als eine Abfrage namens `name` (eine View, CREATE TABLE … AS).
    void abfrageAus(std::size_t a, std::size_t b, const std::string& name)
    {
        sqlteile::AbfrageLeser leser(*this);
        leser.selectIntoTabelle = m_dialekt.name == "tsql";
        if (std::optional<Abfrage> x = leser.lies(a, b, name)) m_aus.einheit.abfragen.push_back(std::move(*x));
    }

    /// Alle Zeilen einer Abfrage um `um` weiter (ein Rumpf, der fuer sich gelesen wurde).
    static void verschiebe(Abfrage& x, int um)
    {
        x.zeile += um;
        x.bisZeile += um;
        for (AbfrageBlock& b : x.bloecke)
        {
            b.zeile += um;
            for (AbfrageQuelle& q : b.quellen) q.zeile += um;
        }
    }

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
    /// Transaktion) und `CASE`. Im Dialekt mit endeMitArt (MySQL, Spark) auch `LOOP`, `REPEAT`,
    /// `IF … THEN`, `WHILE … DO`, `FOR … DO` (THEN bzw. DO vor dem naechsten `;`). Sonst zaehlt ein IF
    /// nicht: Firebird und T-SQL schliessen es nicht (`IF … THEN BEGIN … END`), und ohne Dialekt laesst
    /// sich das von MySQL nicht unterscheiden — dann schliesst ein `END IF` keine Stufe (schliesstSchleife).
    [[nodiscard]] bool oeffnetStufe(std::size_t j) const
    {
        if (wort(j, "begin")) return !beginntTransaktion(j);
        if (wort(j, "case")) return true;
        if (!m_dialekt.endeMitArt) return false;
        if (wort(j, "loop") || wort(j, "repeat")) return true;
        const bool wenn = wort(j, "if");
        if (!wenn && !wort(j, "while") && !wort(j, "for")) return false;
        for (std::size_t k = j + 1; !amEnde(k) && !tok(k).istOp(";");)
        {
            if (oeffnet(tok(k)))
            {
                k = nachKlammer(k);
                continue;
            }
            if (wort(k, wenn ? "then" : "do")) return true;
            if (wort(k, "begin")) return false;
            ++k;
        }
        return false;
    }

    /// `END` bei j schliesst eine Schleife oder ein IF (`END IF`, `END LOOP`, `END WHILE`, `END REPEAT`,
    /// `END FOR`), keine Stufe — ausser im Dialekt mit endeMitArt, wo sie eine geoeffnet haben.
    [[nodiscard]] bool schliesstSchleife(std::size_t j) const
    {
        return !m_dialekt.endeMitArt && (wort(j + 1, "if") || wort(j + 1, "loop") || wort(j + 1, "while") ||
                                         wort(j + 1, "repeat") || wort(j + 1, "for"));
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
                    const bool dazu = schleife || wort(j + 1, "if") || wort(j + 1, "loop") || wort(j + 1, "while") ||
                                      wort(j + 1, "repeat") || wort(j + 1, "for") || wort(j + 1, "case") ||
                                      wort(j + 1, "try") || wort(j + 1, "catch") ||
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
            // T-SQL: ALTER VIEW/PROCEDURE/FUNCTION/TRIGGER … AS legt das Objekt neu fest — wie CREATE; ohne AS
            // (`ALTER FUNCTION f() OWNER TO x`, `ALTER VIEW v RENAME TO w`) aendert es nichts Gezeigtes
            if (wort(a + 1, "view") || wort(a + 1, "procedure") || wort(a + 1, "proc") || wort(a + 1, "function") ||
                wort(a + 1, "trigger"))
            {
                bool mitRumpf = false;
                for (std::size_t k = a + 2; k < b && !mitRumpf; k = oeffnet(tok(k)) ? nachKlammer(k) : k + 1)
                    mitRumpf = wort(k, "as");
                if (mitRumpf) liesCreate(a, b);
            }
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
        {
            // eine Anweisung ausserhalb der Objekte (ein Skript, eine Abfrage in einer Zeichenkette); die
            // Namen ihres WITH gelten nur in ihr
            std::vector<std::string> eigene;
            sammleZugriffe(a, b, m_aus.einheit.zugriffe, eigene);
            abfragenIn(a, b, "query");
            return;
        }
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
            if (w == "index")
            {
                bool eindeutig = false;
                for (std::size_t u = a + 1; u < k; ++u) eindeutig = eindeutig || wort(u, "unique");
                if (eindeutig) liesEindeutigenIndex(k + 1, b);
            }
            else if (g->gebilde == Gebilde::Tabelle)
            {
                liesCreateTable(k + 1, b);
                abfrageAus(a, b, "query@" + std::to_string(tok(a).ort.zeile));  // CREATE TABLE … AS SELECT
                if (alsAbfrage(a, b) != kKeiner)
                {
                    // liest seine Quellen und schreibt die Tabelle, wie INSERT … SELECT (Entscheid C1)
                    std::vector<std::string> eigene;
                    sammleZugriffe(a, b, m_aus.einheit.zugriffe, eigene);
                }
            }
            else if (g->gebilde == Gebilde::Objekt)
                liesObjekt(g->objekt, a, k + 1, b);
            return;
        }
        melde(Schwere::Warnung, tok(a), "CREATE statement is not understood");
    }

    /// Ab i (hinter `UNIQUE … INDEX`): `[CONCURRENTLY] [IF NOT EXISTS] [name] ON [ONLY] tabelle [USING m]
    /// (spalten) …` — die Spalten sind eindeutig (Entscheid N2). Nicht, wenn ein WHERE folgt (dann nur ein
    /// Teil der Zeilen) oder eine Spalte ein Ausdruck ist (`lower(x)`).
    void liesEindeutigenIndex(std::size_t i, std::size_t b)
    {
        std::size_t on = i;
        while (on < b && !wort(on, "on")) ++on;
        if (on >= b) return;
        std::size_t k = on + 1;
        if (wort(k, "only")) ++k;
        std::size_t e = k;
        const std::string name = liesNamen(k, e);
        if (wort(e, "using")) e += 2;
        if (!tok(e).istOp("(")) return;
        const std::size_t zu = ueberKlammer(e);
        for (std::size_t j = zu; j < b; ++j)
            if (wort(j, "where")) return;
        std::vector<std::string> spalten;
        for (const auto& [x, y] : teile(e + 1, zu - 1))
        {
            if (!istName(x) || (x + 1 < y && tok(x + 1).istOp("("))) return;  // ein Ausdruck
            spalten.push_back(ohneBegrenzer(tok(x).text));
        }
        if (spalten.empty()) return;
        Typ* typ = tabelle(name);
        if (typ == nullptr)
        {
            melde(Schwere::Hinweis, tok(k), "UNIQUE INDEX: table '" + name + "' is not defined in this file");
            return;
        }
        typ->eindeutig.push_back(std::move(spalten));
    }

    /// j steht auf CREATE: legt die Anweisung eine Tabelle aus einer Abfrage an (`CREATE [OR REPLACE] [TEMP …]
    /// TABLE [IF NOT EXISTS] name [(spalten)] [USING …, PARTITIONED BY …, COMMENT …] AS SELECT|WITH|(`)?
    /// Dann die Stelle des Namens, sonst kKeiner. Gesucht bis `;`, b oder dem Wort einer neuen Anweisung —
    /// in einem Rumpf folgen weitere.
    [[nodiscard]] std::size_t alsAbfrage(std::size_t j, std::size_t b) const
    {
        std::size_t k = j + 1;
        while (k < b && k < j + 6 && !wort(k, "table")) ++k;
        if (!wort(k, "table")) return kKeiner;
        ++k;
        if (wort(k, "if") && wort(k + 1, "not") && wort(k + 2, "exists")) k += 3;
        if (!istName(k)) return kKeiner;
        const std::size_t name = k;
        std::size_t e = k;
        (void)liesNamen(k, e);
        static constexpr std::string_view kNeu[] = {"select", "insert", "update", "delete", "merge", "create",
                                                    "alter",  "drop",   "begin",  "end",    "declare"};
        for (std::size_t x = e; x < b && !amEnde(x) && !tok(x).istOp(";");)
        {
            if (oeffnet(tok(x)))
            {
                x = nachKlammer(x);
                continue;
            }
            if (wort(x, "as")) return wort(x + 1, "select") || wort(x + 1, "with") || tok(x + 1).istOp("(") ? name : kKeiner;
            if (istName(x) && enthaelt(kNeu, klein(tok(x).text))) return kKeiner;
            ++x;
        }
        return kKeiner;
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
            if (platzhalterImNamen(typ, 0, 0, tok(i))) return;
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
        if (platzhalterImNamen(typ, 0, 0, tok(i))) return;
        markiereSchluessel(typ, primaer);
        m_aus.einheit.typen.push_back(std::move(typ));
    }

    /// Steht ein Platzhalter einer Vorlage (LeseOptionen::platzhalter) im Namen der Tabelle, einer Spalte
    /// ab `felder` oder eines Fremdschluessels ab `verweise`? Dann gibt es einen Hinweis bei `bei`, und die
    /// Anweisung bleibt aussen vor (Entscheid F1) — ihr Name stuende erst zur Laufzeit da.
    bool platzhalterImNamen(const Typ& typ, std::size_t felder, std::size_t verweise, const Token& bei)
    {
        if (m_platzhalter.empty()) return false;
        const auto darin = [&](std::string_view s) { return s.find(m_platzhalter) != std::string_view::npos; };
        bool gefunden = darin(typ.name);
        for (std::size_t f = felder; f < typ.felder.size() && !gefunden; ++f) gefunden = darin(typ.felder[f].name);
        for (std::size_t v = verweise; v < typ.verweise.size() && !gefunden; ++v)
        {
            const Verweis& x = typ.verweise[v];
            gefunden = darin(x.ziel) || std::any_of(x.felder.begin(), x.felder.end(), darin) ||
                       std::any_of(x.zielFelder.begin(), x.zielFelder.end(), darin);
        }
        if (gefunden) melde(Schwere::Hinweis, bei, "placeholder in a name (filled at runtime) - statement not read");
        return gefunden;
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
            if (wort(k, "not") && wort(k + 1, "null"))
            {
                feld.nichtNull = true;
                k += 2;
                continue;
            }
            if (wort(k, "unique"))  // auch `UNIQUE KEY` (MySQL)
            {
                typ.eindeutig.push_back({feld.name});
                k += wort(k + 1, "key") ? 2 : 1;
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
    /// REFERENCES t (…)`, `UNIQUE [KEY|INDEX] [name] (…)`; die anderen (CHECK, INDEX …) tragen nichts bei.
    void liesTabellenBedingung(std::size_t x, std::size_t y, Typ& typ, std::vector<std::string>& primaer)
    {
        std::string name;
        if (wort(x, "constraint"))
        {
            name = ohneBegrenzer(tok(x + 1).text);
            x += 2;
        }
        if (wort(x, "unique"))
        {
            // `UNIQUE (a, b)`, `UNIQUE KEY name (a)` (MySQL), `UNIQUE NONCLUSTERED (a)` (T-SQL)
            std::size_t k = x + 1;
            while (k < y && !tok(k).istOp("(")) ++k;
            std::vector<std::string> spalten = liesNamensliste(k, k);
            if (!spalten.empty()) typ.eindeutig.push_back(std::move(spalten));
            return;
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
        if (!m_platzhalter.empty() && name.find(m_platzhalter) != std::string::npos)
        {
            melde(Schwere::Hinweis, tok(i), "placeholder in a name (filled at runtime) - statement not read");
            return;
        }
        Typ* typ = tabelle(name);
        if (typ == nullptr)
        {
            melde(Schwere::Hinweis, tok(i), "ALTER TABLE: table '" + name + "' is not defined in this file");
            return;
        }
        // an einer Abschrift: steht ein Platzhalter in einem neuen Namen, bleibt die Anweisung aussen vor
        Typ neu = *typ;
        std::vector<std::string> primaer;
        for (const auto& [x0, y] : teile(k, b))
        {
            std::size_t x = x0;
            while (x < y && (wort(x, "with") || wort(x, "check") || wort(x, "nocheck"))) ++x;  // T-SQL
            if (wort(x, "alter"))
            {
                // Postgres: `ALTER [COLUMN] x SET NOT NULL` / `DROP NOT NULL`
                std::size_t c = x + 1;
                if (wort(c, "column")) ++c;
                const bool setzt = wort(c + 1, "set") && wort(c + 2, "not") && wort(c + 3, "null");
                const bool nimmt = wort(c + 1, "drop") && wort(c + 2, "not") && wort(c + 3, "null");
                if (istName(c) && (setzt || nimmt))
                    for (Feld& f : neu.felder)
                        if (klein(f.name) == klein(ohneBegrenzer(tok(c).text))) f.nichtNull = setzt;
                continue;
            }
            if (!wort(x, "add")) continue;
            ++x;
            if (beginntBedingung(x))
                liesTabellenBedingung(x, y, neu, primaer);
            else
            {
                if (wort(x, "column")) ++x;
                if (wort(x, "if") && wort(x + 1, "not") && wort(x + 2, "exists")) x += 3;
                liesSpalte(x, y, neu, primaer);
            }
        }
        if (platzhalterImNamen(neu, typ->felder.size(), typ->verweise.size(), tok(i))) return;
        if (std::any_of(primaer.begin(), primaer.end(),
                        [&](const std::string& p) { return !m_platzhalter.empty() && p.find(m_platzhalter) != std::string::npos; }))
        {
            melde(Schwere::Hinweis, tok(i), "placeholder in a name (filled at runtime) - statement not read");
            return;
        }
        markiereSchluessel(neu, primaer);
        *typ = std::move(neu);
    }

    // --- Objekte und Zugriffe (Entscheide A1–A3) --------------------------------

    /// Ab i (hinter VIEW, TRIGGER, FUNCTION, PROCEDURE): das Objekt mit seinen Zugriffen.
    void liesObjekt(ObjektArt art, std::size_t a, std::size_t i, std::size_t b)
    {
        Datenbankobjekt o;
        o.art = art;
        if (wort(i, "if") && wort(i + 1, "not") && wort(i + 2, "exists")) i += 3;
        std::size_t k = i;
        o.name = liesNamen(i, k);
        o.zeile = tok(a).ort.zeile;
        o.bisZeile = tok(b > a ? b - 1 : a).ort.zeile;
        if (o.name.empty())
        {
            melde(Schwere::Warnung, tok(i), "name of the object is missing");
            return;
        }
        if (!m_platzhalter.empty() && o.name.find(m_platzhalter) != std::string::npos)
        {
            melde(Schwere::Hinweis, tok(i), "placeholder in a name (filled at runtime) - statement not read");
            return;
        }
        std::vector<std::string> eigene;  // was das Objekt selbst anlegt: Namen eines WITH, temporaere Tabellen
        if (art == ObjektArt::View)
        {
            sammleZugriffe(k, b, o.zugriffe, eigene);
            abfrageAus(a, b, o.name);
        }
        else if (art == ObjektArt::Trigger)
            liesTrigger(o, k, b, eigene);
        else
            liesRoutine(o, k, b, eigene);
        m_aus.einheit.objekte.push_back(std::move(o));
    }

    /// Der Rest eines Triggers ab k: Ereignis, Tabelle, Rumpf. `… AFTER INSERT ON t …` (SQLite, Postgres,
    /// MySQL), `… ON t AFTER INSERT AS …` (T-SQL), `… FOR t ACTIVE AFTER INSERT AS …` (Firebird).
    void liesTrigger(Datenbankobjekt& o, std::size_t k, std::size_t b, std::vector<std::string>& eigene)
    {
        std::size_t on = k;
        while (on < b && !wort(on, "on") && !(on == k && wort(on, "for"))) on = oeffnet(tok(on)) ? nachKlammer(on) : on + 1;
        if (on >= b)
        {
            melde(Schwere::Hinweis, tok(k), "trigger '" + o.name + "': its table is not found");
            return;
        }
        std::size_t nach = on + 1;
        o.tabelle = liesNamen(on + 1, nach);
        std::size_t rumpf = nach;
        if (on == k)
        {
            // T-SQL, Firebird: das Ereignis steht hinter der Tabelle, bis AS
            std::size_t as = nach;
            while (as < b && !wort(as, "as")) ++as;
            std::size_t von = nach;
            while (von < as && (wort(von, "active") || wort(von, "inactive"))) ++von;
            std::size_t bis = von;
            while (bis < as && !wort(bis, "position")) ++bis;
            o.ereignis = textVon(von, bis);
            rumpf = as;
        }
        else
            o.ereignis = textVon(k, on);
        sammleZugriffe(rumpf, b, o.zugriffe, eigene);
        abfragenIn(rumpf, b, o.name);
    }

    /// Der Rest einer Funktion oder Prozedur ab k: Parameter, dann der Rumpf — in `$$…$$` oder `'…'`
    /// (Postgres) noch einmal gelesen, sonst die Token bis zum Ende (T-SQL, MySQL, Spark `RETURN …`). Eine
    /// Routine in einer anderen Sprache (`LANGUAGE plpython3u`) bleibt mit Hinweis aussen vor.
    void liesRoutine(Datenbankobjekt& o, std::size_t k, std::size_t b, std::vector<std::string>& eigene)
    {
        if (tok(k).istOp("(")) k = nachKlammer(k);
        std::string sprache;
        std::size_t koerper = kKeiner;
        for (std::size_t j = k; j < b; ++j)
        {
            // `LANGUAGE plpgsql`; nicht `LANGUAGE = N'English'` (die natuerliche Sprache eines ATOMIC-Blocks, T-SQL)
            if (wort(j, "language") && j + 1 < b && !tok(j + 1).istOp("="))
            {
                sprache = klein(tok(j + 1).text);
                if (sprache.size() >= 2 && (sprache.front() == '\'' || sprache.front() == '"'))
                    sprache = sprache.substr(1, sprache.size() - 2);
            }
            const Token& t = tok(j);
            if (koerper == kKeiner && t.art == TokenArt::Zeichenkette && (t.text.starts_with('$') || wort(j - 1, "as")))
                koerper = j;
        }
        if (!sprache.empty() && sprache != "plpgsql" && sprache != "sql" && sprache != "plsql")
        {
            melde(Schwere::Hinweis, tok(k), "routine '" + o.name + "' in LANGUAGE " + sprache + " is not read");
            return;
        }
        if (koerper != kKeiner && m_dialekt.name != "tsql" && m_dialekt.name != "mysql")
        {
            m_routine = o.name;
            liesRumpfText(tok(koerper), o.zugriffe, eigene);
            m_routine.clear();
            return;
        }
        sammleZugriffe(k, b, o.zugriffe, eigene);
        abfragenIn(k, b, o.name);
    }

    /// Der Rumpf einer Routine in einer Zeichenkette (`$$…$$`, `$f$…$f$`, `'…'`) durch einen zweiten
    /// Leser; Zeilen und Meldungen auf die Stellen in diesem Text.
    void liesRumpfText(const Token& koerper, std::vector<Zugriff>& zugriffe, std::vector<std::string>& eigene)
    {
        std::string_view s = koerper.text;
        std::size_t vorn = 1;
        std::string innen;
        if (s.starts_with('$'))
        {
            vorn = s.find('$', 1) + 1;
            innen = std::string(s.substr(vorn, s.size() >= 2 * vorn ? s.size() - 2 * vorn : 0));
        }
        else
        {
            for (std::size_t i = 1; i + 1 < s.size(); ++i)
            {
                innen += s[i];
                if (s[i] == '\'' && s[i + 1] == '\'') ++i;
            }
        }
        Ergebnis neben;
        std::vector<Zugriff> innere;
        SqlLeser rumpf(innen, m_dialekt, m_platzhalter, neben);
        rumpf.m_imRumpf = true;
        rumpf.m_routine = m_routine;
        rumpf.liesRumpf(innere, eigene);
        const int versatzZeile = koerper.ort.zeile - 1;
        for (Abfrage& x : neben.einheit.abfragen)
        {
            verschiebe(x, versatzZeile);
            x.name = m_routine + "@" + std::to_string(x.zeile);
            m_aus.einheit.abfragen.push_back(std::move(x));
        }
        for (Zugriff& z : innere)
        {
            z.zeile += versatzZeile;
            fuegeZugriffHinzu(zugriffe, std::move(z));
        }
        for (quelle::Meldung& m : neben.meldungen)
        {
            if (m.ort.zeile == 1) m.ort.spalte += koerper.ort.spalte - 1 + static_cast<int>(vorn);
            m.ort.zeile += versatzZeile;
            m.ort.versatz += koerper.ort.versatz + vorn;
            m_aus.meldungen.push_back(std::move(m));
        }
    }

    /// Ein Zugriff, wenn es ihn (Art und Ziel, ohne Ruecksicht auf Gross/klein) noch nicht gibt.
    static void fuegeZugriffHinzu(std::vector<Zugriff>& zugriffe, Zugriff z)
    {
        const std::string ziel = klein(z.ziel);
        for (const Zugriff& x : zugriffe)
            if (x.art == z.art && x.imAusdruck == z.imAusdruck && klein(x.ziel) == ziel) return;
        zugriffe.push_back(std::move(z));
    }

    /// Ein Name, der keine Tabelle meint: eine Variable (`@t`), eine temporaere Tabelle von T-SQL (`#t`), ein
    /// Name eines WITH oder eine temporaere Tabelle, die das Objekt selbst anlegt (`eigene`).
    [[nodiscard]] static bool keineTabelle(std::string_view name, const std::vector<std::string>& eigene)
    {
        if (name.empty() || name.front() == '@' || name.front() == '#' || name.find('.') == 0) return true;
        const std::string n = klein(name);
        return n == "dual" || std::find(eigene.begin(), eigene.end(), n) != eigene.end();
    }

    /// Die Zugriffe der Token [a, b) (Entscheid A2): liest (FROM, JOIN), schreibt (INSERT INTO, UPDATE … SET,
    /// DELETE FROM, MERGE INTO, TRUNCATE), ruft (CALL, EXEC, EXECUTE FUNCTION, PERFORM; ein Name vor `(` im
    /// Ausdruck mit imAusdruck). Namen eines WITH und temporaere Tabellen kommen nach `eigene`.
    /// Dynamisches SQL (`EXECUTE 'text'`, `EXECUTE format(…)`, `EXEC(@sql)`, `sp_executesql`) gibt einen Hinweis.
    void sammleZugriffe(std::size_t a, std::size_t b, std::vector<Zugriff>& aus, std::vector<std::string>& eigene)
    {
        const auto neu = [&](ZugriffArt art, const std::string& ziel, std::size_t bei, bool imAusdruck = false) {
            if (art != ZugriffArt::Ruft && keineTabelle(ziel, eigene)) return;
            if (ziel.empty() || ziel.front() == '@' || ziel.front() == '#') return;  // Variablen, temporaere Tabellen
            if (!m_platzhalter.empty() && ziel.find(m_platzhalter) != std::string::npos) return;
            fuegeZugriffHinzu(aus, {art, ziel, tok(bei).ort.zeile, imAusdruck, false});
        };
        const auto tabellenName = [&](std::size_t k) {
            const Token& t = tok(k);
            return t.art == TokenArt::Bezeichner && !(t.text.size() == 1 && t.text == "_");
        };
        std::vector<std::string> klammern;  // je offene Klammer der Name davor (klein), innen zuletzt
        // `FROM` in diesen Klammern nennt keine Tabelle: EXTRACT(YEAR FROM d), SUBSTRING(s FROM 2) …
        static constexpr std::string_view kOhneTabelle[] = {"extract", "substring", "substr", "trim", "overlay",
                                                            "position", "cast", "convert"};
        std::size_t ohneFrom = kKeiner;  // das FROM hinter DELETE ist schon gelesen
        // je offenes FROM die Tiefe der Klammern, auf der es steht: ein Komma dort nennt eine weitere Quelle
        // (`FROM a JOIN b ON … , c`), bis WHERE, GROUP … das FROM beenden
        std::vector<std::size_t> offeneFrom;
        static constexpr std::string_view kEndeFrom[] = {"where",  "group",   "order",  "having", "limit",
                                                         "union",  "intersect", "except", "window", "qualify",
                                                         "returning", "set",  "values", "select", "fetch", "offset"};
        // die Quellen ab k, durch Kommas getrennt: `a, b x, c AS y`; die Stelle dahinter
        const auto quellen = [&](std::size_t k) {
            while (true)
            {
                while (wort(k, "lateral") || wort(k, "only")) ++k;
                if (!tabellenName(k)) return k;
                std::size_t e = k;
                const std::string n = liesNamen(k, e);
                if (tok(e).istOp("("))
                {
                    neu(ZugriffArt::Ruft, n, k, true);  // eine Funktion, die eine Tabelle liefert
                    return e;
                }
                neu(ZugriffArt::Liest, n, k);
                k = e;
                if (wort(k, "as")) ++k;
                if (tok(k).art == TokenArt::Bezeichner) ++k;  // ein Alias
                if (!tok(k).istOp(",")) return k;
                ++k;
            }
        };
        for (std::size_t j = a; j < b && !amEnde(j); ++j)
        {
            const Token& t = tok(j);
            if (oeffnet(t))
            {
                klammern.push_back(j > a && istName(j - 1) ? klein(tok(j - 1).text) : std::string());
                continue;
            }
            if (schliesst(t))
            {
                if (!klammern.empty()) klammern.pop_back();
                while (!offeneFrom.empty() && offeneFrom.back() > klammern.size()) offeneFrom.pop_back();
                continue;
            }
            if (t.istOp(",") && !offeneFrom.empty() && offeneFrom.back() == klammern.size())
            {
                j = quellen(j + 1) - 1;
                continue;
            }
            if (t.art != TokenArt::Schluesselwort && t.art != TokenArt::Bezeichner) continue;
            const std::string w = klein(t.text);
            if (!offeneFrom.empty() && offeneFrom.back() == klammern.size() && enthaelt(kEndeFrom, w))
                offeneFrom.pop_back();

            if (w == "with" && istName(j + 1) && !tok(j + 1).istOp("("))
            {
                // die Namen eines WITH: `WITH [RECURSIVE] a [(x)] AS [NOT] [MATERIALIZED] (…), b AS (…)`
                std::size_t k = j + 1;
                if (wort(k, "recursive")) ++k;
                while (istName(k))
                {
                    std::size_t e = k + 1;
                    if (tok(e).istOp("(")) e = nachKlammer(e);
                    if (!wort(e, "as")) break;
                    eigene.push_back(klein(ohneBegrenzer(tok(k).text)));
                    ++e;
                    while (wort(e, "not") || wort(e, "materialized")) ++e;
                    if (!tok(e).istOp("(")) break;
                    e = nachKlammer(e);
                    if (!tok(e).istOp(",")) break;
                    k = e + 1;
                }
                continue;
            }
            if ((w == "from" || w == "join" || w == "using") && j != ohneFrom)
            {
                // USING nennt eine Quelle nur in MERGE INTO t [AS x] USING s
                if (w == "using" && !(j >= 2 && wort(j - 2, "into")) && !(j >= 3 && wort(j - 3, "into")) &&
                    !(j >= 4 && wort(j - 4, "into")))
                    continue;
                if (w == "from" && (wort(j - 1, "distinct") ||
                                    (!klammern.empty() && enthaelt(kOhneTabelle, klammern.back()))))
                    continue;
                if (w == "from" && (offeneFrom.empty() || offeneFrom.back() != klammern.size()))
                    offeneFrom.push_back(klammern.size());
                j = quellen(j + 1) - 1;
                continue;
            }
            if (w == "insert" || w == "replace" || w == "merge" || w == "upsert")
            {
                std::size_t k = j + 1;
                if (wort(k, "or")) k += 2;
                if (wort(k, "ignore") || wort(k, "low_priority") || wort(k, "delayed")) ++k;
                // T-SQL erlaubt `INSERT t …` ohne INTO (nur bei INSERT, nicht bei REPLACE/MERGE)
                if (wort(k, "into") || wort(k, "overwrite"))
                    ++k;
                else if (w != "insert" || !tabellenName(k))
                    continue;
                if (wort(k, "table")) ++k;
                if (!tabellenName(k)) continue;
                std::size_t e = k;
                const std::string n = liesNamen(k, e);
                neu(ZugriffArt::Schreibt, n, k);
                j = e - 1;
                continue;
            }
            if (w == "update" && tabellenName(j + 1))
            {
                std::size_t e = j + 1;
                const std::string n = liesNamen(j + 1, e);
                if (wort(e, "as")) ++e;
                if (tok(e).art == TokenArt::Bezeichner && !wort(e, "set")) ++e;  // ein Alias
                if (wort(e, "set")) neu(ZugriffArt::Schreibt, n, j + 1);
                j = e - 1;
                continue;
            }
            if (w == "delete")
            {
                std::size_t k = j + 1;
                if (wort(k, "from")) ++k;
                if (!tabellenName(k)) continue;
                std::size_t e = k;
                const std::string n = liesNamen(k, e);
                neu(ZugriffArt::Schreibt, n, k);
                ohneFrom = j + 1;
                j = e - 1;
                continue;
            }
            if (w == "truncate")
            {
                std::size_t k = j + 1;
                if (wort(k, "table")) ++k;
                if (!tabellenName(k)) continue;
                std::size_t e = k;
                neu(ZugriffArt::Schreibt, liesNamen(k, e), k);
                j = e - 1;
                continue;
            }
            if (w == "create" && (wort(j + 1, "temp") || wort(j + 1, "temporary") || wort(j + 2, "temp") ||
                                  wort(j + 2, "temporary")))
            {
                // eine temporaere Tabelle gehoert dem, der sie anlegt (Entscheid A3)
                std::size_t k = j + 1;
                while (k < b && !wort(k, "table")) ++k;
                ++k;
                if (wort(k, "if") && wort(k + 1, "not") && wort(k + 2, "exists")) k += 3;
                std::size_t e = k;
                const std::string n = liesNamen(k, e);
                if (!n.empty()) eigene.push_back(klein(n));
                j = e - 1;
                continue;
            }
            if (w == "create")
            {
                // CREATE TABLE … AS SELECT schreibt die Tabelle (Entscheid C1); was es liest, nennt sein FROM
                const std::size_t k = alsAbfrage(j, b);
                if (k == kKeiner) continue;
                std::size_t e = k;
                const std::string n = liesNamen(k, e);
                neu(ZugriffArt::Schreibt, n, k);
                j = e - 1;
                continue;
            }
            if (w == "call" || w == "perform" || w == "exec" || w == "execute")
            {
                std::size_t k = j + 1;
                if (wort(k, "as")) continue;  // T-SQL `EXECUTE AS OWNER`: unter wessen Rechten, kein Aufruf
                // im Rumpf einer Postgres-Routine fuehrt EXECUTE immer Text aus (`EXECUTE sql_text`), ausser
                // EXECUTE FUNCTION/PROCEDURE
                const bool plpgsql = m_imRumpf && w == "execute" && !wort(k, "function") && !wort(k, "procedure");
                if (plpgsql ||
                    (w != "call" && w != "perform" &&
                     (wort(k, "immediate") || tok(k).art == TokenArt::Zeichenkette || tok(k).istOp("(") ||
                      wort(k, "format") || wort(k, "sp_executesql") || (tok(k).text.starts_with('@') && !tok(k + 1).istOp("=")))))
                {
                    melde(Schwere::Hinweis, t, "dynamic SQL is not read");
                    continue;
                }
                if (tok(k).text.starts_with('@') && tok(k + 1).istOp("=")) k += 2;  // EXEC @rc = p
                if (wort(k, "function") || wort(k, "procedure")) ++k;                // EXECUTE FUNCTION f() (Trigger)
                if (!istName(k)) continue;
                std::size_t e = k;
                const std::string n = liesNamen(k, e);
                neu(ZugriffArt::Ruft, n, k, w == "perform");
                j = e - 1;
                continue;
            }
            // ein Name vor `(` im Ausdruck: vielleicht eine Routine des Projekts
            // (nicht der Name hinter TABLE, INDEX, REFERENCES … — `CREATE TABLE t (`, `REFERENCES t(id)`)
            static constexpr std::string_view kVorName[] = {"table", "index", "view", "references", "exists",
                                                            "function", "procedure", "trigger", "type", "key"};
            if (t.art == TokenArt::Bezeichner && (j == a || !tok(j - 1).istOp(".")))
            {
                std::size_t e = j;
                const std::string n = liesNamen(j, e);
                const bool hinterName = j > a && istName(j - 1) && enthaelt(kVorName, klein(tok(j - 1).text));
                if (tok(e).istOp("(") && !n.empty() && !hinterName) neu(ZugriffArt::Ruft, n, j, true);
                j = e - 1;
            }
        }
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

    /// Nimmt die Spalten eines Primaerschluessels in Typ::primaer auf (ein spaeterer ersetzt ihn) und
    /// schreibt `PK` und `FK` in den Zusatz der Spalten (ohne Ruecksicht auf Gross/klein); eine Spalte
    /// des Primaerschluessels ist NOT NULL.
    static void markiereSchluessel(Typ& typ, const std::vector<std::string>& primaer)
    {
        if (!primaer.empty()) typ.primaer = primaer;
        for (Feld& f : typ.felder)
        {
            const std::string n = klein(f.name);
            const bool pk = std::any_of(typ.primaer.begin(), typ.primaer.end(), [&](const std::string& p) { return klein(p) == n; });
            if (pk) f.nichtNull = true;
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

    const Dialekt& m_dialekt;
    std::string m_platzhalter;  ///< LeseOptionen::platzhalter
    bool m_imRumpf = false;     ///< liest den Rumpf einer Routine in `$$…$$` (PL/pgSQL)
    std::string m_routine;      ///< die Routine, deren Rumpf gelesen wird (Name ihrer Abfragen)
    std::string m_trenner = ";";
};

class Sql final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "SQL"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungen; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return kTabelle; }

    /// Der Dialekt: aus den Optionen (Projektdatei, Host), sonst aus einem Kopf der Datei (Entscheid E2),
    /// sonst der allgemeine Kern — mit einem Hinweis, wie man ihn setzt.
    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& optionen) const override
    {
        Ergebnis ergebnis;
        ergebnis.einheit.name = std::string(einheitName);
        const Dialekt* dialekt = &kKern;
        const std::string_view gewaehlt = !optionen.dialekt.empty() ? std::string_view(optionen.dialekt) : dialektAusKopf(text);
        if (!gewaehlt.empty())
        {
            const auto d = std::find_if(std::begin(kDialekte), std::end(kDialekte),
                                        [&](const Dialekt& x) { return x.name == gewaehlt; });
            if (d != std::end(kDialekte))
                dialekt = &*d;
            else
                ergebnis.meldungen.push_back({quelle::Schwere::Warnung, {},
                                              "unknown SQL dialect '" + std::string(gewaehlt) +
                                                  "' - read as common SQL (known: sqlite, postgres, duckdb, spark, mysql, tsql)"});
        }
        else
            ergebnis.meldungen.push_back({quelle::Schwere::Hinweis, {},
                                          "SQL dialect not known - read as common SQL (\"sql\" in code2diagram.json sets it)"});
        SqlLeser(text, *dialekt, optionen.platzhalter, ergebnis).liesAlles();
        ergebnis.zeichenketten.clear();  // SQL in den Zeichenketten von SQL sucht niemand
        return ergebnis;
    }
};

/// Woerter zwischen CREATE (DROP, ALTER) und dem Gebilde, die beginntMitSql erlaubt.
constexpr std::string_view kVorGebilde[] = {
    "or",      "replace",  "alter",     "refresh",  "temp",      "temporary", "global",     "local",
    "unlogged", "external", "unique",   "clustered", "nonclustered", "virtual", "materialized", "streaming",
    "live",    "recursive", "secure",   "transient", "volatile",  "if",        "not",        "exists",
};

}  // namespace

const Sprache& sql()
{
    static const Sql sprache;
    return sprache;
}

std::span<const std::string_view> sqlDialekte()
{
    return kDialektNamen;
}

bool beginntMitSchema(std::string_view text, bool nurAnfang)
{
    std::size_t p = 0;
    while (p < text.size() && (text[p] == ' ' || text[p] == '\t' || text[p] == '\r' || text[p] == '\n')) ++p;
    std::size_t q = p;
    while (q < text.size() && ((text[q] >= 'a' && text[q] <= 'z') || (text[q] >= 'A' && text[q] <= 'Z'))) ++q;
    const std::string erstes = klein(text.substr(p, q - p));
    return (erstes == "create" || erstes == "alter" || erstes == "drop") && beginntMitSql(text, nurAnfang);
}

bool beginntMitSql(std::string_view text, bool nurAnfang)
{
    // vorab ohne Lexer: das erste Wort (die meisten Zeichenketten sind kein SQL)
    std::size_t p = 0;
    while (p < text.size() && (text[p] == ' ' || text[p] == '\t' || text[p] == '\r' || text[p] == '\n')) ++p;
    std::size_t q = p;
    while (q < text.size() && ((text[q] >= 'a' && text[q] <= 'z') || (text[q] >= 'A' && text[q] <= 'Z'))) ++q;
    static constexpr std::string_view kErste[] = {"select", "insert", "replace", "merge", "update", "delete",
                                                  "with",   "pragma", "create",  "alter", "drop"};
    if (q == p || !enthaelt(kErste, klein(text.substr(p, q - p)))) return false;

    std::vector<quelle::Meldung> meldungen;
    const std::vector<lexer::Token> t = lexer::zerlege(text.substr(0, std::min<std::size_t>(text.size(), 4000)),
                                                       kTabelle, meldungen);
    const auto w = [&](std::size_t i) {
        return i < t.size() && (t[i].art == lexer::TokenArt::Schluesselwort || t[i].art == lexer::TokenArt::Bezeichner)
                   ? klein(t[i].text)
                   : std::string();
    };
    const auto spaeter = [&](std::string_view wort) {
        for (std::size_t i = 1; i < t.size(); ++i)
            if (w(i) == wort) return true;
        return false;
    };
    const std::string a = w(0);
    if (a == "select") return spaeter("from");
    if (a == "insert" || a == "replace" || a == "merge")
    {
        // `INSERT [OR x] INTO name (…)|VALUES|SELECT|DEFAULT|SET` — nicht „Insert into the selected list“
        std::size_t k = a == "insert" && w(1) == "or" ? 3 : 1;
        if (w(k) != "into" || w(k + 1).empty()) return false;
        k += 1;
        while (k + 2 < t.size() && t[k + 1].istOp(".") && !w(k + 2).empty()) k += 2;
        const std::string danach = w(k + 1);
        return (k + 1 < t.size() && t[k + 1].istOp("(")) || danach == "values" || danach == "select" ||
               danach == "default" || danach == "set" || danach == "with" || danach == "using";
    }
    if (a == "update") return w(2) == "set" || (t.size() > 4 && w(4) == "set");  // `UPDATE t SET`, `UPDATE s.t SET`
    if (a == "delete") return w(1) == "from";
    if (a == "with") return (w(2) == "as" || w(1) == "recursive") && spaeter("select");
    if (a == "pragma") return t.size() > 2;
    if (a == "create" || a == "alter" || a == "drop")
    {
        for (std::size_t i = 1; i < t.size() && i < 12; ++i)
        {
            const std::string x = w(i);
            if (enthaelt(kVorGebilde, x)) continue;
            if (!std::any_of(std::begin(kGebilde), std::end(kGebilde), [&](const GebildeWort& g) { return g.wort == x; }))
                return false;
            if (nurAnfang) return true;
            // hinter dem Gebilde ein Name (auch mit Schema), dann, was zu der Anweisung passt —
            // sonst ist es ein Satz („Create table for users“)
            std::size_t k = i + 1;
            while (w(k) == "if" || w(k) == "not" || w(k) == "exists") ++k;
            if (w(k).empty()) return false;
            while (k + 2 < t.size() && t[k + 1].istOp(".") && !w(k + 2).empty()) k += 2;
            const std::string danach = w(k + 1);
            const bool klammer = k + 1 < t.size() && t[k + 1].istOp("(");
            if (a == "drop")
                return k + 1 >= t.size() || t[k + 1].art == lexer::TokenArt::Ende || t[k + 1].istOp(";") ||
                       danach == "cascade" || danach == "restrict";
            if (a == "alter")
                return danach == "add" || danach == "drop" || danach == "alter" || danach == "rename" ||
                       danach == "owner" || danach == "set" || danach == "with";
            if (x == "table") return klammer || danach == "as" || danach == "like" || danach == "using";
            if (x == "view") return spaeter("as");
            if (x == "index") return danach == "on";
            if (x == "trigger")
                return danach == "before" || danach == "after" || danach == "instead" || danach == "for" || danach == "on";
            return klammer || danach == "as";
        }
    }
    return false;
}

}  // namespace c2d::sprache
