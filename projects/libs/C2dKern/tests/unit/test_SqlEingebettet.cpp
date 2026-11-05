/**
 * @file test_SqlEingebettet.cpp
 * @brief SQL in Zeichenketten (Entscheide S1, E1) und die Dialekte (S3, E2): die Zeichenketten der Leser,
 *        beginntMitSql, Tabellen in der Einheit der Host-Datei, Dialekt aus Projektdatei, Kopf und Host
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/projekt/Projekt.hpp"
#include "c2d/projekt/Projektdatei.hpp"
#include "c2d/sprache/Sprache.hpp"

#include "TempOrdner.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;

namespace
{

std::vector<std::string> inhalte(const sprache::Ergebnis& e)
{
    std::vector<std::string> aus;
    for (const sprache::Zeichenkette& z : e.zeichenketten) aus.push_back(z.inhalt);
    return aus;
}

const modell::Typ* tabelle(const modell::Einheit& e, std::string_view name)
{
    for (const modell::Typ& t : e.typen)
        if (t.name == name && t.art == modell::TypArt::Tabelle) return &t;
    return nullptr;
}

bool meldet(const std::vector<quelle::Meldung>& meldungen, std::string_view teil)
{
    return std::any_of(meldungen.begin(), meldungen.end(),
                       [&](const quelle::Meldung& m) { return m.text.find(teil) != std::string::npos; });
}

sprache::Ergebnis sqlMit(std::string_view text, std::string dialekt)
{
    sprache::LeseOptionen o;
    o.dialekt = std::move(dialekt);
    return sprache::sql().lies(text, "T.sql", o);
}

}  // namespace

TEST_SUITE("SQL eingebettet")
{
    TEST_CASE("Die Leser liefern ihre Zeichenketten: verbunden, ohne Begrenzer und Escapes, mit Stelle")
    {
        // C: zwei Literale hintereinander sind eines; \n ist ein Zeilenwechsel; je Byte die Stelle
        const auto c = sprache::c().lies("const char* q = \"CREATE TABLE a (\"\n    \"x int)\\n\";", "t.c");
        REQUIRE(c.zeichenketten.size() == 1);
        CHECK(c.zeichenketten[0].inhalt == "CREATE TABLE a (x int)\n");
        CHECK(c.zeichenketten[0].versatz.size() == c.zeichenketten[0].inhalt.size());
        CHECK(c.zeichenketten[0].versatz[0] == 17);  // das C hinter dem ersten "
        CHECK_FALSE(c.zeichenketten[0].zusammengesetzt);

        // C++: rohe Zeichenkette ohne Escapes; + macht zusammengesetzt
        const auto cpp = sprache::cpp().lies("auto a = R\"x(SELECT \\n FROM t)x\"; auto b = \"x\" + n;", "t.cpp");
        CHECK(inhalte(cpp) == std::vector<std::string>{"SELECT \\n FROM t", "x"});
        CHECK(cpp.zeichenketten[1].zusammengesetzt);
        // eine Vorlage, die ausgefuellt wird: Qt .arg (auch hinter QStringLiteral(…)), Python % und .format
        const auto qt = sprache::cpp().lies("auto a = QStringLiteral(\"CREATE TABLE t (id %1)\").arg(pk);", "q.cpp");
        REQUIRE(qt.zeichenketten.size() == 1);
        CHECK(qt.zeichenketten[0].zusammengesetzt);
        const auto pf = sprache::python().lies("a = \"x %s\" % y\nb = \"{}\".format(y)\nc = \"z\".upper()\n", "f.py");
        REQUIRE(pf.zeichenketten.size() == 3);
        CHECK(pf.zeichenketten[0].zusammengesetzt);
        CHECK(pf.zeichenketten[1].zusammengesetzt);
        CHECK_FALSE(pf.zeichenketten[2].zusammengesetzt);

        // Python: dreifache, r"", f"" mit {x} zusammengesetzt, {{ ist {
        const auto py = sprache::python().lies("a = \"\"\"\nCREATE TABLE t (x)\n\"\"\"\nb = f\"{x}\"\nc = f\"{{x}}\"\nd = r\"\\n\"\n", "t.py");
        CHECK(inhalte(py) == std::vector<std::string>{"\nCREATE TABLE t (x)\n", "{x}", "{x}", "\\n"});
        CHECK_FALSE(py.zeichenketten[0].zusammengesetzt);
        CHECK(py.zeichenketten[1].zusammengesetzt);
        CHECK_FALSE(py.zeichenketten[2].zusammengesetzt);

        // C#: @"" mit "" darin, $"{x}"
        const auto cs = sprache::csharp().lies("class A { string a = @\"x \"\"y\"\"\"; string b = $\"{n}\"; }", "t.cs");
        CHECK(inhalte(cs) == std::vector<std::string>{"x \"y\"", "{n}"});
        CHECK(cs.zeichenketten[1].zusammengesetzt);

        // JavaScript: ${x} zusammengesetzt, $name Text; Kotlin: $name zusammengesetzt
        const auto js = sprache::javascript().lies("const a = `SELECT $1 FROM ${t}`; const b = `a $b`;", "t.js");
        REQUIRE(js.zeichenketten.size() == 2);
        CHECK(js.zeichenketten[0].zusammengesetzt);
        CHECK_FALSE(js.zeichenketten[1].zusammengesetzt);
        const auto kt = sprache::kotlin().lies("val a = \"DROP TABLE $name\"\nval b = \"x\"\n", "t.kt");
        REQUIRE(kt.zeichenketten.size() == 2);
        CHECK(kt.zeichenketten[0].zusammengesetzt);
        CHECK_FALSE(kt.zeichenketten[1].zusammengesetzt);
    }

    TEST_CASE("beginntMitSql: Anweisungen ja, Saetze nein")
    {
        for (const std::string_view ja : {"SELECT a FROM t", "\n  insert into t values (1)", "UPDATE t SET a = 1",
                                          "delete from t", "CREATE TABLE IF NOT EXISTS public.t (a int)",
                                          "create temp table t as select 1", "CREATE VIEW v AS SELECT 1",
                                          "CREATE UNIQUE INDEX i ON t (a)", "ALTER TABLE t ADD COLUMN b int",
                                          "DROP TABLE t", "WITH q AS (SELECT 1) SELECT * FROM q", "PRAGMA foreign_keys = ON"})
            CHECK_MESSAGE(sprache::beginntMitSql(ja), ja);
        for (const std::string_view nein : {"Select a file", "Create table for users", "Delete the file", "Update now",
                                            "create a new project", "Drop table here?", "with love", "SELECTED", "",
                                            "Insert into the selected list, else below"})
            CHECK_FALSE_MESSAGE(sprache::beginntMitSql(nein), nein);
    }

    TEST_CASE("Dialekt: Kopf (Databricks), Spark und MySQL mit \\ und \"…\", IF … END IF, unbekannt, Kern mit Hinweis")
    {
        const auto db = sprache::sql().lies("-- Databricks notebook source\n"
                                            "-- COMMAND ----------\n"
                                            "SELECT ai_classify(\"It's \\\"late\\\";\nnext line\", ARRAY('a\\'b'));\n"
                                            "CREATE TABLE t (a STRING COMMENT \"x\");",
                                            "n.sql");
        CHECK(db.meldungen.empty());
        REQUIRE(db.einheit.typen.size() == 1);
        CHECK(db.einheit.typen[0].felder[0].typ == "STRING");

        const auto my = sqlMit("CREATE PROCEDURE p() BEGIN\n"
                               "  IF x THEN SELECT 1; END IF;\n"
                               "  WHILE y DO SET y = y - 1; END WHILE;\n"
                               "END;\n"
                               "CREATE TABLE t (a VARCHAR(5) DEFAULT \"x\");",
                               "mysql");
        CHECK(my.einheit.typen.size() == 1);
        CHECK(meldet(my.meldungen, "1 procedure read but not shown yet"));
        CHECK(my.meldungen.size() == 1);

        CHECK(meldet(sqlMit("SELECT 1;", "oracle").meldungen, "unknown SQL dialect 'oracle'"));
        const auto kern = sprache::sql().lies("CREATE TABLE t (a int);", "t.sql");
        CHECK(meldet(kern.meldungen, "SQL dialect not known"));
        CHECK(sprache::sql().lies("--\n-- PostgreSQL database dump\n--\nSELECT 1;", "d.sql").meldungen.empty());
    }

    TEST_CASE("SQL in Zeichenketten: Tabellen in der Einheit der Datei, Stellen der Quelle, Fremdschluessel ueber Zeichenketten")
    {
        const auto p = projekt::liesText("import sqlite3\n"
                                         "\n"
                                         "class Projekt:\n"
                                         "    pass\n"
                                         "\n"
                                         "SCHEMA = \"\"\"\n"
                                         "CREATE TABLE projekt (\n"
                                         "    id INTEGER PRIMARY KEY,\n"
                                         "    name TEXT\n"
                                         ")\"\"\"\n"
                                         "MESSUNG = (\"CREATE TABLE messung (\"\n"
                                         "           \"  id INTEGER PRIMARY KEY, projekt INTEGER REFERENCES projekt(id))\")\n"
                                         "def lade(c, n):\n"
                                         "    c.execute(\"SELECT * FROM projekt WHERE name = '\" + n + \"'\")\n"
                                         "    c.execute(\"CREATE TABLE \" + n + \" (x int)\")\n"
                                         "    print(\"Create table for users\")\n",
                                         "db.py");
        const modell::Einheit& e = p.dateien[0].ergebnis.einheit;
        const auto& meldungen = p.dateien[0].ergebnis.meldungen;
        const modell::Typ* projekt = tabelle(e, "projekt");
        const modell::Typ* messung = tabelle(e, "messung");
        REQUIRE(projekt != nullptr);
        REQUIRE(messung != nullptr);
        CHECK(e.typen.size() == 3);  // die Klasse und zwei Tabellen
        CHECK(projekt->zeile == 7);
        CHECK(projekt->felder[0].zeile == 8);
        CHECK(projekt->felder[1].name == "name");
        CHECK(projekt->felder[1].zeile == 9);
        CHECK(messung->zeile == 11);
        CHECK(messung->felder[1].zeile == 12);
        REQUIRE(messung->verweise.size() == 1);
        CHECK(messung->verweise[0].ziel == "projekt");
        CHECK_FALSE(meldet(meldungen, "dialect not known"));  // import sqlite3
        CHECK(meldet(meldungen, "SQL in a composed string"));
        CHECK_FALSE(meldet(meldungen, "users"));
    }

    TEST_CASE("Dialekt fuer eingebettetes SQL: Projektdatei vor Host; zwei Bibliotheken; keine; Qt-Treiber")
    {
        // MySQL aus dem Host: "…" ist eine Zeichenkette, keine Spalte
        const std::string_view mysql = "import pymysql\nQ = \"CREATE TABLE t (a VARCHAR(3) DEFAULT 'x', b INT)\"\n";
        const auto p = projekt::liesText(mysql, "a.py");
        REQUIRE(tabelle(p.dateien[0].ergebnis.einheit, "t") != nullptr);
        CHECK(tabelle(p.dateien[0].ergebnis.einheit, "t")->felder.size() == 2);
        CHECK(p.dateien[0].ergebnis.meldungen.empty());

        const auto zwei = projekt::liesText("import duckdb\nimport sqlite3\nQ = \"CREATE TABLE t (a int)\"\n", "b.py");
        CHECK(meldet(zwei.dateien[0].ergebnis.meldungen, "several SQL libraries (duckdb, sqlite3)"));
        CHECK(meldet(zwei.dateien[0].ergebnis.meldungen, "SQL dialect not known"));

        const auto qt = projekt::liesText("void f() { auto db = QSqlDatabase::addDatabase(\"QSQLITE\");\n"
                                          "  q.exec(\"CREATE TABLE t (a int)\"); }",
                                          "c.cpp");
        CHECK(tabelle(qt.dateien[0].ergebnis.einheit, "t") != nullptr);
        CHECK_FALSE(meldet(qt.dateien[0].ergebnis.meldungen, "dialect"));

        // die Projektdatei geht vor: ein .sql und ein .py im Ordner
        const TempOrdner t;
        t.lege("code2diagram.json", "{ \"sql\": \"mysql\" }");
        t.lege("schema.sql", "CREATE TABLE s (a VARCHAR(3) DEFAULT \"x\");");
        // in MySQL maskiert `\'`; als SQLite gelesen bliebe die Zeichenkette offen
        t.lege("x.py", "import sqlite3\nQ = \"CREATE TABLE u (b TEXT DEFAULT 'it\\\\'s')\"\n");
        const projekt::Projekt o = projekt::liesOrdner(t.weg());
        CHECK(o.einstellungen.sql == "mysql");
        REQUIRE(o.dateien.size() == 2);
        for (const projekt::Datei& d : o.dateien)
            CHECK_MESSAGE(d.ergebnis.meldungen.empty(), d.pfad);
    }

    TEST_CASE("Projektdatei: \"sql\" gelesen, geschrieben, ein unbekannter Dialekt gemeldet")
    {
        std::vector<quelle::Meldung> m;
        projekt::Projektdatei d = projekt::liesProjektdatei("{ \"sql\": \"duckdb\" }", m);
        CHECK(m.empty());
        CHECK(d.sql == "duckdb");
        CHECK(projekt::schreibe(d).find("\"sql\": \"duckdb\"") != std::string::npos);
        CHECK(projekt::liesProjektdatei(projekt::schreibe(d), m).sql == "duckdb");
        CHECK(projekt::schreibe(projekt::Projektdatei{}).find("sql") == std::string::npos);
        d = projekt::liesProjektdatei("{ \"sql\": \"oracle\" }", m);
        CHECK(d.sql.empty());
        CHECK(meldet(m, "unknown SQL dialect \"oracle\""));
    }
}
