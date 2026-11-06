/**
 * @file test_Notebook.cpp
 * @brief Notebooks (Entscheide P2, N1–N4): ein `.ipynb` als Skript mit Kopfzeilen, je Code-Zelle eine
 *        Funktion `<cell n>`, Magie mit Hinweis, Zellen in anderer Sprache (`%%sql`, Databricks `%python`)
 *        in der Einheit des Notebooks, Databricks-Notebooks, Stellen `cell n, line m`
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/projekt/Projekt.hpp"
#include "c2d/sprache/Sprache.hpp"

#include "TempOrdner.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;

namespace
{

const modell::Funktion* funktion(const modell::Einheit& e, std::string_view name)
{
    for (const modell::Funktion& f : e.funktionen)
        if (f.vollerName() == name) return &f;
    return nullptr;
}

std::vector<std::string> namen(const modell::Einheit& e)
{
    std::vector<std::string> aus;
    for (const modell::Funktion& f : e.funktionen) aus.push_back(f.vollerName());
    return aus;
}

const quelle::Meldung* meldung(const std::vector<quelle::Meldung>& meldungen, std::string_view teil)
{
    for (const quelle::Meldung& m : meldungen)
        if (m.text.find(teil) != std::string::npos) return &m;
    return nullptr;
}

bool liest(const modell::Funktion& f, std::string_view tabelle)
{
    return std::any_of(f.zugriffe.begin(), f.zugriffe.end(), [&](const modell::Zugriff& z) {
        return z.art == modell::ZugriffArt::Liest && z.ziel == tabelle;
    });
}

/// Ein Notebook mit Python-Kernel aus Zellen (Art, Zeilen); `metadaten` ersetzt die Angabe der Sprache.
std::string ipynb(const std::vector<std::pair<std::string, std::vector<std::string>>>& zellen,
                  const std::string& metadaten = R"({"kernelspec": {"name": "python3", "language": "python"},
                                                     "language_info": {"name": "python"}})")
{
    std::string aus = R"({"nbformat": 4, "nbformat_minor": 5, "metadata": )" + metadaten + R"(, "cells": [)";
    for (std::size_t i = 0; i < zellen.size(); ++i)
    {
        aus += (i > 0 ? ", " : "") + std::string(R"({"cell_type": ")") + zellen[i].first + R"(", "metadata": {}, "source": [)";
        for (std::size_t j = 0; j < zellen[i].second.size(); ++j)
        {
            std::string zeile;
            for (const char c : zellen[i].second[j])
            {
                if (c == '"' || c == '\\') zeile += '\\';
                zeile += c;
            }
            aus += (j > 0 ? ", " : "") + std::string("\"") + zeile + (j + 1 < zellen[i].second.size() ? "\\n" : "") + "\"";
        }
        aus += R"(], "outputs": []})";
    }
    return aus + "]}";
}

}  // namespace

TEST_SUITE("Notebook")
{
    TEST_CASE("Ein .ipynb wird ein Skript: je Zelle eine Kopfzeile, Markdown als Kommentar (N1)")
    {
        CHECK(projekt::istNotebook("a/b.ipynb"));
        CHECK(projekt::istNotebook("B.IPYNB"));
        CHECK_FALSE(projekt::istNotebook("b.py"));
        CHECK(projekt::kannLesen("b.ipynb"));

        const std::string json = ipynb({{"markdown", {"# Laden", "", "Text"}},
                                        {"code", {"import os", "x = 1"}},
                                        {"code", {"print(x)"}}});
        const projekt::Projekt p = projekt::liesText(json, "n.ipynb");
        REQUIRE(p.dateien.size() == 1);
        const projekt::Datei& d = p.dateien.front();
        REQUIRE(d.sprache == &sprache::python());
        CHECK(d.text ==
              "# %% [1] markdown\n# # Laden\n#\n# Text\n"
              "\n# %% [2]\nimport os\nx = 1\n"
              "\n# %% [3]\nprint(x)\n");
        REQUIRE(d.zellen.size() == 3);
        CHECK(d.zellen[0].art == projekt::Zelle::Art::Markdown);
        CHECK(d.zellen[1].kopf == 6);
        CHECK(d.zellen[1].zeile == 7);
        CHECK(d.zellen[1].bisZeile == 8);
        CHECK(projekt::stelleIn(d, 8) == "cell 2, line 2");
        CHECK(projekt::stelleIn(d, 6) == "cell 2");
        CHECK(projekt::stelleIn(d, 11) == "cell 3, line 1");
        CHECK(projekt::zelleAn(d, 11) == 2);
        CHECK(d.ergebnis.meldungen.empty());

        // ohne Notebook: eine Zeile
        const projekt::Projekt q = projekt::liesText("x = 1\n", "a.py");
        CHECK(projekt::stelleIn(q.dateien.front(), 1) == "line 1");
        CHECK(projekt::zelleAn(q.dateien.front(), 1) == projekt::kKeine);
    }

    TEST_CASE("Jede Code-Zelle mit Code ist eine Funktion <cell n>, kein <module> (N2)")
    {
        const std::string json = ipynb({{"code", {"import os"}},
                                        {"code", {"x = 1", "y = 2"}},
                                        {"markdown", {"Zwischentext"}},
                                        {"code", {"def f(a):", "    return a + 1", "", "for i in range(3):", "    print(f(i))"}},
                                        {"code", {}}});
        const projekt::Projekt p = projekt::liesText(json, "n.ipynb");
        const modell::Einheit& e = p.dateien.front().ergebnis.einheit;
        // nur Einbindung: keine Funktion; nur Zuweisungen: eine (anders als <module>); leer: keine
        // in der Reihenfolge des Texts: f steht in Zelle 4 vor ihrer Schleife
        CHECK(namen(e) == std::vector<std::string>{"<cell 2>", "f", "<cell 4>"});
        const modell::Funktion* zwei = funktion(e, "<cell 2>");
        REQUIRE(zwei != nullptr);
        REQUIRE(zwei->rumpf);
        CHECK(zwei->rumpf->kinder.size() == 2);
        const modell::Funktion* vier = funktion(e, "<cell 4>");
        REQUIRE(vier != nullptr);
        REQUIRE(vier->rumpf);
        REQUIRE(vier->rumpf->kinder.size() == 1);
        CHECK(vier->rumpf->kinder[0]->art == modell::AnweisungArt::FuerJedes);
        // ab ihrer ersten Anweisung bis zur letzten Zeile der Zelle
        CHECK(vier->zeile == 15);
        CHECK(vier->bisZeile == 16);
        CHECK(funktion(e, "<module>") == nullptr);

        // eine Datei .py bleibt bei <module>
        const projekt::Projekt q = projekt::liesText("for i in range(3):\n    print(i)\n", "a.py");
        CHECK(namen(q.dateien.front().ergebnis.einheit) == std::vector<std::string>{"<module>"});
    }

    TEST_CASE("Magie und Shell: im Skript, wie sie stehen; gelesen als Leerraum, je Zeile ein Hinweis")
    {
        const std::string json = ipynb({{"code", {"%pip install pandas", "!ls -l", "import pandas as pd",
                                                  "%run hilfen.py", "df = pd.read_csv('x')", "  %time f()"}},
                                        {"code", {"%%time", "g()"}}});
        const projekt::Projekt p = projekt::liesText(json, "n.ipynb");
        const projekt::Datei& d = p.dateien.front();
        CHECK(d.text.find("%pip install pandas\n!ls -l\n") != std::string::npos);
        const std::vector<quelle::Meldung>& m = d.ergebnis.meldungen;
        REQUIRE(m.size() == 5);
        CHECK(m[0].text == "IPython magic %pip is not read");
        CHECK(m[0].ort.zeile == 2);
        CHECK(m[0].ort.spalte == 1);
        CHECK(m[0].schwere == quelle::Schwere::Hinweis);
        CHECK(m[1].text == "shell command is not read");
        CHECK(m[2].text == "%run is not followed");
        CHECK(m[3].text == "IPython magic %time is not read");
        CHECK(m[3].ort.spalte == 3);
        CHECK(m[4].text == "IPython magic %%time is not read");
        CHECK(d.text.substr(m[4].ort.versatz, 7) == "%%time\n");
        const modell::Funktion* eins = funktion(d.ergebnis.einheit, "<cell 1>");
        REQUIRE(eins != nullptr);
        CHECK(eins->rumpf->kinder.size() == 1);  // df = …
        CHECK(funktion(d.ergebnis.einheit, "<cell 2>") != nullptr);
    }

    TEST_CASE("Eine Zelle in anderer Sprache: %%sql liest der SQL-Leser, %%bash niemand (N3)")
    {
        const std::string json = ipynb({{"code", {"%%sql", "CREATE TABLE kunde (id INT PRIMARY KEY, name TEXT);",
                                                  "SELECT name FROM kunde WHERE id = 1;"}},
                                        {"code", {"%%bash", "ls -l"}},
                                        {"code", {"x = 1"}}});
        const projekt::Projekt p = projekt::liesText(json, "n.ipynb");
        const projekt::Datei& d = p.dateien.front();
        const modell::Einheit& e = d.ergebnis.einheit;
        CHECK(d.zellen[0].sprache == "SQL");
        CHECK(d.zellen[1].sprache == "%%bash");
        CHECK_FALSE(d.zellen[1].gelesen);
        const quelle::Meldung* bash = meldung(d.ergebnis.meldungen, "%%bash");
        REQUIRE(bash != nullptr);
        CHECK(bash->text == "cell 2 in %%bash is not read");
        CHECK(bash->ort.zeile == 7);  // die Zeile `%%bash`
        // die Tabelle in der Einheit des Notebooks, in der Zeile des Skripts
        REQUIRE(e.typen.size() == 1);
        CHECK(e.typen[0].name == "kunde");
        CHECK(e.typen[0].zeile == 3);
        // was die Zelle liest, gehoert <cell 1> (ohne Rumpf); ihre Abfrage heisst nach ihr
        const modell::Funktion* eins = funktion(e, "<cell 1>");
        REQUIRE(eins != nullptr);
        CHECK_FALSE(eins->rumpf);
        CHECK(liest(*eins, "kunde"));
        CHECK(e.zugriffe.empty());
        REQUIRE(e.abfragen.size() == 1);
        CHECK(e.abfragen[0].name == "<cell 1>@4");
        CHECK(namen(e) == std::vector<std::string>{"<cell 1>", "<cell 3>"});
    }

    TEST_CASE("SQL in spark.sql(...) einer Zelle gehoert ihrer Funktion")
    {
        const std::string json =
            ipynb({{"code", {"from pyspark.sql import SparkSession", "df = spark.sql(\"SELECT a FROM t\")"}}});
        const projekt::Projekt p = projekt::liesText(json, "n.ipynb");
        const modell::Funktion* eins = funktion(p.dateien.front().ergebnis.einheit, "<cell 1>");
        REQUIRE(eins != nullptr);
        CHECK(liest(*eins, "t"));
        REQUIRE(p.dateien.front().ergebnis.einheit.abfragen.size() == 1);
        CHECK(p.dateien.front().ergebnis.einheit.abfragen[0].name == "<cell 1>@3");
    }

    TEST_CASE("Sprache des Notebooks: aus language_info, sonst kernelspec, sonst Python mit Hinweis")
    {
        const std::string js = ipynb({{"code", {"const x = 1;", "console.log(x);"}}},
                                     R"({"kernelspec": {"name": "javascript", "language": "javascript"}})");
        const projekt::Projekt a = projekt::liesText(js, "n.ipynb");
        REQUIRE(a.dateien.size() == 1);
        CHECK(a.dateien.front().sprache == &sprache::javascript());
        CHECK(a.dateien.front().text.starts_with("// %% [1]\n"));
        CHECK(funktion(a.dateien.front().ergebnis.einheit, "<cell 1>") != nullptr);

        const projekt::Projekt b = projekt::liesText(ipynb({{"code", {"print(1)"}}}, "{}"), "n.ipynb");
        REQUIRE(b.dateien.size() == 1);
        CHECK(b.dateien.front().sprache == &sprache::python());
        CHECK(meldung(b.dateien.front().ergebnis.meldungen, "names no language") != nullptr);

        // eine Sprache ohne Leser, kaputtes JSON, Format 3: die Datei faellt mit Meldung heraus
        const projekt::Projekt c =
            projekt::liesText(ipynb({{"code", {"1+1"}}}, R"({"language_info": {"name": "R"}})"), "n.ipynb");
        CHECK(c.dateien.empty());
        REQUIRE(c.meldungen.size() == 1);
        CHECK(c.meldungen[0].pfad == "n.ipynb");
        CHECK(c.meldungen[0].meldung.text == "notebook in R: no reader for this language");
        CHECK(c.meldungen[0].meldung.ort.zeile == 1);

        const projekt::Projekt d = projekt::liesText("{\n  \"cells\": [,]\n}", "n.ipynb");
        CHECK(d.dateien.empty());
        REQUIRE(d.meldungen.size() == 1);
        CHECK(d.meldungen[0].meldung.ort.zeile == 2);

        const projekt::Projekt f = projekt::liesText(R"({"nbformat": 3, "worksheets": []})", "n.ipynb");
        REQUIRE(f.meldungen.size() == 1);
        CHECK(f.meldungen[0].meldung.text == "notebook format 3 is not read (only format 4)");
    }

    TEST_CASE("Databricks-Notebook in SQL: Zellen zwischen COMMAND, %python gelesen, %md still, %scala mit Hinweis")
    {
        const std::string text = "-- Databricks notebook source\n"     // 1
                                 "-- MAGIC %md\n"                      // 2
                                 "-- MAGIC ## Kunden\n"                // 3
                                 "\n"                                  // 4
                                 "-- COMMAND ----------\n"             // 5
                                 "\n"                                  // 6
                                 "-- DBTITLE 1,Kunden lesen\n"         // 7
                                 "SELECT name FROM kunde;\n"           // 8
                                 "\n"                                  // 9
                                 "-- COMMAND ----------\n"             // 10
                                 "\n"                                  // 11
                                 "-- MAGIC %python\n"                  // 12
                                 "-- MAGIC for r in rows:\n"           // 13
                                 "-- MAGIC     print(r)\n"             // 14
                                 "-- MAGIC return 1\n"                 // 15
                                 "\n"                                  // 16
                                 "-- COMMAND ----------\n"             // 17
                                 "\n"                                  // 18
                                 "-- MAGIC %scala\n"                   // 19
                                 "-- MAGIC val x = 1\n";               // 20
        const projekt::Projekt p = projekt::liesText(text, "nb.sql");
        REQUIRE(p.dateien.size() == 1);
        const projekt::Datei& d = p.dateien.front();
        CHECK(d.text == text);  // bleibt, wie es ist
        REQUIRE(d.zellen.size() == 4);
        CHECK(d.zellen[0].art == projekt::Zelle::Art::Markdown);
        CHECK(d.zellen[0].kopf == 0);
        CHECK(d.zellen[0].zeile == 2);
        CHECK(d.zellen[1].titel == "Kunden lesen");
        CHECK(d.zellen[1].kopf == 5);
        CHECK(d.zellen[1].zeile == 8);
        CHECK(d.zellen[1].bisZeile == 8);
        CHECK(d.zellen[2].sprache == "Python");
        CHECK(d.zellen[3].sprache == "%scala");
        CHECK_FALSE(d.zellen[3].gelesen);
        CHECK(projekt::stelleIn(d, 13) == "cell 3, line 2");
        CHECK(projekt::stelleIn(d, 1) == "cell 1");

        const modell::Einheit& e = d.ergebnis.einheit;
        // die SQL-Zelle: ihre Zugriffe an <cell 2>, ihre Abfrage heisst nach ihr
        const modell::Funktion* zwei = funktion(e, "<cell 2>");
        REQUIRE(zwei != nullptr);
        CHECK(liest(*zwei, "kunde"));
        REQUIRE(e.abfragen.size() == 1);
        CHECK(e.abfragen[0].name == "<cell 2>@8");
        // die Python-Zelle: eine Funktion mit Ablauf in den Zeilen des Notebooks
        const modell::Funktion* drei = funktion(e, "<cell 3>");
        REQUIRE(drei != nullptr);
        REQUIRE(drei->rumpf);
        REQUIRE(drei->rumpf->kinder.size() == 2);
        CHECK(drei->rumpf->kinder[0]->zeile == 13);
        // eine Meldung des Python-Lesers steht an ihrer Stelle im Notebook, hinter `-- MAGIC `
        const quelle::Meldung* zurueck = meldung(d.ergebnis.meldungen, "'return' outside a function");
        REQUIRE(zurueck != nullptr);
        CHECK(zurueck->ort.zeile == 15);
        CHECK(zurueck->ort.spalte == 10);
        CHECK(d.text.substr(zurueck->ort.versatz, 6) == "return");
        // %md ohne Hinweis, %scala mit
        CHECK(meldung(d.ergebnis.meldungen, "%md") == nullptr);
        const quelle::Meldung* scala = meldung(d.ergebnis.meldungen, "%scala");
        REQUIRE(scala != nullptr);
        CHECK(scala->text == "cell 4 in %scala is not read");
        CHECK(scala->ort.zeile == 19);
        CHECK(scala->ort.spalte == 10);
        CHECK(namen(e) == std::vector<std::string>{"<cell 2>", "<cell 3>"});
    }

    TEST_CASE("Databricks-Notebook in Python: je Zelle <cell n>, eine %sql-Zelle gehoert ihr")
    {
        const std::string text = "# Databricks notebook source\n"  // 1
                                 "x = 1\n"                          // 2
                                 "\n"                               // 3
                                 "# COMMAND ----------\n"           // 4
                                 "\n"                               // 5
                                 "# MAGIC %sql SELECT a FROM t\n"   // 6
                                 "\n"                               // 7
                                 "# COMMAND ----------\n"           // 8
                                 "\n"                               // 9
                                 "# MAGIC %run ./hilfen\n"          // 10
                                 "\n"                               // 11
                                 "# COMMAND ----------\n"           // 12
                                 "\n"                               // 13
                                 "if x:\n"                          // 14
                                 "    print(x)\n";                  // 15
        const projekt::Projekt p = projekt::liesText(text, "nb.py");
        const projekt::Datei& d = p.dateien.front();
        const modell::Einheit& e = d.ergebnis.einheit;
        CHECK(namen(e) == std::vector<std::string>{"<cell 1>", "<cell 2>", "<cell 4>"});
        const modell::Funktion* zwei = funktion(e, "<cell 2>");
        REQUIRE(zwei != nullptr);
        CHECK(liest(*zwei, "t"));
        const quelle::Meldung* run = meldung(d.ergebnis.meldungen, "%run");
        REQUIRE(run != nullptr);
        CHECK(run->text == "%run is not followed");
        CHECK(run->ort.zeile == 10);
        CHECK(funktion(e, "<cell 4>")->bisZeile == 15);
        // der Kopf belegt den Dialekt, auch fuer die Zelle in SQL (E2)
        CHECK(meldung(d.ergebnis.meldungen, "dialect not known") == nullptr);
    }

    TEST_CASE("Databricks: Code hinter dem Wort der Magie, eine leere Zeile MAGIC vor ihm (dbdemos)")
    {
        const std::string text = "-- Databricks notebook source\n"         // 1
                                 "-- MAGIC %python time.sleep(60) # warte\n"  // 2
                                 "\n"                                      // 3
                                 "-- COMMAND ----------\n"                 // 4
                                 "\n"                                      // 5
                                 "-- MAGIC\n"                              // 6
                                 "-- MAGIC %md\n"                          // 7
                                 "-- MAGIC ## Titel\n";                    // 8
        const projekt::Projekt p = projekt::liesText(text, "nb.sql");
        const projekt::Datei& d = p.dateien.front();
        CHECK(meldung(d.ergebnis.meldungen, "indent") == nullptr);
        CHECK(meldung(d.ergebnis.meldungen, "is not read") == nullptr);
        REQUIRE(d.zellen.size() == 2);
        CHECK(d.zellen[1].art == projekt::Zelle::Art::Markdown);
        const modell::Funktion* eins = funktion(d.ergebnis.einheit, "<cell 1>");
        REQUIRE(eins != nullptr);
        REQUIRE(eins->rumpf);
        REQUIRE(eins->rumpf->kinder.size() == 1);
        CHECK(eins->rumpf->kinder[0]->text == "time.sleep(60)");
        CHECK(eins->rumpf->kinder[0]->zeile == 2);
    }

    TEST_CASE("Der Dialekt einer Zelle in SQL: aus dem Code des Notebooks; die Abfragen in Reihenfolge")
    {
        const std::string mit = ipynb({{"code", {"import sqlite3", "x = sqlite3.connect('a')"}},
                                       {"code", {"%%sql", "SELECT a FROM t;"}},
                                       {"code", {"y = x.execute(\"SELECT b FROM u\")"}}});
        const projekt::Projekt p = projekt::liesText(mit, "n.ipynb");
        CHECK(meldung(p.dateien.front().ergebnis.meldungen, "dialect not known") == nullptr);
        const std::vector<modell::Abfrage>& q = p.dateien.front().ergebnis.einheit.abfragen;
        REQUIRE(q.size() == 2);
        CHECK(q[0].name == "<cell 2>@7");
        CHECK(q[1].name == "<cell 3>@10");
        // ohne Bibliothek: der Kern, mit dem Hinweis des SQL-Lesers
        const projekt::Projekt ohne = projekt::liesText(ipynb({{"code", {"%%sql", "SELECT a FROM t;"}}}), "n.ipynb");
        CHECK(meldung(ohne.dateien.front().ergebnis.meldungen, "dialect not known") != nullptr);
    }

    TEST_CASE("Ein Ordner liest .ipynb mit; ein Text ohne den Kopf ist kein Databricks-Notebook")
    {
        TempOrdner ordner;
        ordner.lege("a.ipynb", ipynb({{"code", {"x = 1"}}}));
        ordner.lege("b.py", "# COMMAND ----------\nx = 1\n");
        ordner.lege("c.ipynb", "kein JSON");
        const projekt::Projekt p = projekt::liesOrdner(ordner.weg());
        REQUIRE(p.dateien.size() == 2);
        CHECK(p.dateien[0].pfad == "a.ipynb");
        CHECK(p.dateien[0].zellen.size() == 1);
        CHECK(p.dateien[1].zellen.empty());
        CHECK(namen(p.dateien[1].ergebnis.einheit).empty());  // nur eine Zuweisung: kein <module>
        CHECK(std::any_of(p.meldungen.begin(), p.meldungen.end(),
                          [](const projekt::Projektmeldung& m) { return m.pfad == "c.ipynb"; }));
    }
}
