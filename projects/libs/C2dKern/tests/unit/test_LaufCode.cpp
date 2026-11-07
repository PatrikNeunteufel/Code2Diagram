/**
 * @file test_LaufCode.cpp
 * @brief Spark Schicht 3, L5 und L6: die Plaene der SQL-Ausfuehrungen eines Event-Logs, und wo die Aktionen und Stages
 *        eines Laufs im Code stehen — ueber den Pfad, den Hash einer Zelle (ipykernel) und, ohne Stelle, den Namen
 *        zwischen zwei Ankern (Entscheide L5-1 bis L5-3). Die Formen der Ereignisse wie in den eigenen Logs (Spark 4.3).
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/diagramm/Datenfluss.hpp"
#include "c2d/diagramm/Zeitachse.hpp"
#include "c2d/projekt/Datenfluss.hpp"
#include "c2d/projekt/Lauf.hpp"
#include "c2d/projekt/Projekt.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace c2d;

namespace
{

/// Ein Text als JSON-Zeichenkette.
std::string js(std::string_view s)
{
    std::string aus = "\"";
    for (const char c : s)
    {
        if (c == '"' || c == '\\') aus.push_back('\\');
        if (c == '\n')
        {
            aus += "\\n";
            continue;
        }
        aus.push_back(c);
    }
    return aus + "\"";
}

/// Ein Log aus Ereignissen: Kopf, die Zeilen, Ende.
struct Log
{
    std::string text = std::string(R"({"Event":"SparkListenerLogStart","Spark Version":"4.3.0"})") + "\n" +
                       R"({"Event":"SparkListenerApplicationStart","App Name":"lager","App ID":"local-9","Timestamp":1000})" + "\n";
    long long zeit = 1000;
    int naechsterJob = 0;
    int naechsteStage = 0;

    /// Ein Job mit einer Stage; `aufruf` = callSite.short (leer: keiner), `sql` = Ausfuehrung (-1: keine).
    void job(std::string_view stageName, std::string_view aufruf = {}, int sql = -1)
    {
        zeit += 10;
        std::string props = "{";
        if (!aufruf.empty()) props += "\"callSite.short\":" + js(aufruf);
        if (sql >= 0) props += std::string(aufruf.empty() ? "" : ",") + "\"spark.sql.execution.id\":\"" + std::to_string(sql) + "\"";
        props += "}";
        const std::string s = std::to_string(naechsteStage++);
        text += "{\"Event\":\"SparkListenerJobStart\",\"Job ID\":" + std::to_string(naechsterJob) + ",\"Submission Time\":" +
                std::to_string(zeit) + ",\"Stage IDs\":[" + s + "],\"Properties\":" + props + "}\n";
        text += "{\"Event\":\"SparkListenerStageSubmitted\",\"Stage Info\":{\"Stage ID\":" + s + ",\"Stage Attempt ID\":0,\"Stage Name\":" +
                js(stageName) + ",\"Number of Tasks\":1,\"Parent IDs\":[],\"Submission Time\":" + std::to_string(zeit) + "}}\n";
        text += "{\"Event\":\"SparkListenerStageCompleted\",\"Stage Info\":{\"Stage ID\":" + s + ",\"Stage Attempt ID\":0,\"Stage Name\":" +
                js(stageName) + ",\"Number of Tasks\":1,\"Parent IDs\":[],\"Submission Time\":" + std::to_string(zeit) +
                ",\"Completion Time\":" + std::to_string(zeit + 5) + "}}\n";
        text += "{\"Event\":\"SparkListenerJobEnd\",\"Job ID\":" + std::to_string(naechsterJob++) + ",\"Completion Time\":" +
                std::to_string(zeit + 5) + ",\"Job Result\":{\"Result\":\"JobSucceeded\"}}\n";
    }

    /// Eine SQL-Ausfuehrung mit einem Job: `beschreibung` wie im Log (`count at NativeMethodAccessorImpl.java:0`).
    void ausfuehrung(int id, std::string_view beschreibung, std::string_view plan = "== Physical Plan ==\n*(1) Range (0, 100, step=1, splits=2)\n")
    {
        zeit += 10;
        text += "{\"Event\":\"org.apache.spark.sql.execution.ui.SparkListenerSQLExecutionStart\",\"executionId\":" + std::to_string(id) +
                ",\"rootExecutionId\":" + std::to_string(id) + ",\"description\":" + js(beschreibung) +
                ",\"details\":\"\",\"physicalPlanDescription\":" + js(plan) + ",\"time\":" + std::to_string(zeit) + "}\n";
        const std::string_view at = beschreibung.substr(0, beschreibung.find(" at "));
        job(std::string(at) + " at NativeMethodAccessorImpl.java:0", beschreibung.find(".py:") != std::string_view::npos ? beschreibung : "", id);
        text += "{\"Event\":\"org.apache.spark.sql.execution.ui.SparkListenerSQLExecutionEnd\",\"executionId\":" + std::to_string(id) +
                ",\"time\":" + std::to_string(zeit + 6) + "}\n";
    }

    std::string fertig() const
    {
        return text + "{\"Event\":\"SparkListenerApplicationEnd\",\"Timestamp\":" + std::to_string(zeit + 100) + "}\n";
    }
};

// Wie die Probe `probe_rdd.py` (eigener Lauf), dazu zwei count() hintereinander zwischen zwei take().
constexpr std::string_view kSkript = "from pyspark.sql import SparkSession\n"                                   // 1
                                     "spark = SparkSession.builder.appName(\"lager\").getOrCreate()\n"         // 2
                                     "sc = spark.sparkContext\n"                                               // 3
                                     "woerter = sc.parallelize([\"a b\", \"b c\"], 2)\n"                        // 4
                                     "paare = woerter.flatMap(lambda z: z.split()).map(lambda w: (w, 1))\n"    // 5
                                     "summen = paare.reduceByKey(lambda a, b: a + b)\n"                        // 6
                                     "print(summen.collect())\n"                                               // 7
                                     "df = spark.range(100)\n"                                                 // 8
                                     "print(df.count())\n"                                                     // 9
                                     "df.show()\n"                                                             // 10
                                     "rows = df.take(2)\n"                                                     // 11
                                     "n = df.count()\n"                                                        // 12
                                     "m = df.count()\n"                                                        // 13
                                     "erste = df.take(1)\n"                                                    // 14
                                     "spark.stop()\n";

// Wie die Probe `probe_nb.ipynb`: die Texte der Zellen geben die Hashes, die ipykernel in den Log schrieb.
constexpr std::string_view kNotebook = R"NB({"nbformat": 4, "nbformat_minor": 5,
 "metadata": {"kernelspec": {"name": "python3", "language": "python"}, "language_info": {"name": "python"}},
 "cells": [
  {"cell_type": "code", "metadata": {}, "execution_count": null, "outputs": [],
   "source": ["woerter = sc.parallelize(['a b', 'b c'], 2)\n", "summen = woerter.flatMap(lambda z: z.split()).map(lambda w: (w, 1)).reduceByKey(lambda a, b: a + b)"]},
  {"cell_type": "code", "metadata": {}, "execution_count": null, "outputs": [], "source": ["print(summen.collect())"]},
  {"cell_type": "code", "metadata": {}, "execution_count": null, "outputs": [],
   "source": ["df = spark.range(100)\n", "n = df.count()\n", "zeilen = df.filter('id > 5').collect()"]}
 ]})NB";

std::filesystem::path ordnerMit(std::string_view name)
{
    const std::filesystem::path ordner = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(ordner);
    std::filesystem::create_directories(ordner);
    return ordner;
}

const projekt::LaufAktion* aktion(const projekt::Laufverbindung& v, std::string_view methode, int n = 0)
{
    for (const projekt::LaufAktion& a : v.aktionen)
        if (a.methode == methode && n-- == 0) return &a;
    return nullptr;
}

bool hatHinweis(const std::vector<std::string>& h, std::string_view teil)
{
    return std::any_of(h.begin(), h.end(), [&](const std::string& x) { return x.find(teil) != std::string::npos; });
}

}  // namespace

TEST_SUITE("Spark-Lauf und Code")
{
    TEST_CASE("L6: je SQL-Ausfuehrung ihr Plan, unter AQE die letzte Fassung; Job und Stage nennen ihn")
    {
        Log log;
        log.ausfuehrung(0, "count at NativeMethodAccessorImpl.java:0");
        // eine zweite mit einer spaeteren Fassung (AQE), und eine in ihr (rootExecutionId einer anderen)
        log.text += "{\"Event\":\"org.apache.spark.sql.execution.ui.SparkListenerSQLExecutionStart\",\"executionId\":1,\"rootExecutionId\":1,"
                    "\"description\":\"collect at C:\\\\x\\\\lager.py:7\",\"physicalPlanDescription\":" +
                    js("== Physical Plan ==\nAdaptiveSparkPlan isFinalPlan=false\n+- Range (0, 10, step=1, splits=2)\n") + ",\"time\":2000}\n";
        log.text += "{\"Event\":\"org.apache.spark.sql.execution.ui.SparkListenerSQLAdaptiveExecutionUpdate\",\"executionId\":1,"
                    "\"physicalPlanDescription\":" +
                    js("== Physical Plan ==\nAdaptiveSparkPlan isFinalPlan=true\n+- == Final Plan ==\n   LocalTableScan [id#0L]\n+- == Initial Plan ==\n   Range (0, 10, step=1, splits=2)\n") +
                    "}\n";
        log.text += "{\"Event\":\"org.apache.spark.sql.execution.ui.SparkListenerSQLExecutionStart\",\"executionId\":2,\"rootExecutionId\":1,"
                    "\"description\":\"x\",\"physicalPlanDescription\":\"\",\"time\":2001}\n";
        log.text += "{\"Event\":\"org.apache.spark.sql.execution.ui.SparkListenerSQLExecutionEnd\",\"executionId\":1,\"time\":2100,"
                    "\"errorMessage\":\"kaputt\\nmehr\"}\n";
        const projekt::Projekt p = projekt::liesText(log.fertig(), "local-9");
        REQUIRE(p.dateien.size() == 1);
        const modell::Einheit& e = p.dateien[0].ergebnis.einheit;
        REQUIRE(e.laeufe.size() == 1);
        const modell::Lauf& l = e.laeufe[0];
        REQUIRE(l.ausfuehrungen.size() == 3);
        CHECK(l.ausfuehrungen[1].beschreibung == "collect at C:\\x\\lager.py:7");
        CHECK(l.ausfuehrungen[1].fehler == "kaputt");
        CHECK(l.ausfuehrungen[2].wurzel == 1);
        REQUIRE(e.plaene.size() == 2);  // die dritte hat keinen Plan
        CHECK(e.plaene[0].name == "local-9@execution 0");
        CHECK(e.plaene[1].name == "local-9@execution 1");
        CHECK(e.plaene[1].endgueltig);
        CHECK(e.plaene[1].baeume[0].operatoren[1].name == "LocalTableScan");  // die letzte Fassung
        CHECK(e.plaene[1].aufruf > 0);  // ein Klick zeigt das Ereignis im Log
        CHECK(l.jobs[0].sql == "0");

        const diagramm::Zeitachse z = diagramm::zeitachseVon(l);
        const auto job = std::find_if(z.balken.begin(), z.balken.end(), [](const diagramm::ZeitBalken& b) {
            return b.art == diagramm::ZeitBalkenArt::Job && b.job == 0;
        });
        REQUIRE(job != z.balken.end());
        CHECK(job->plan == "local-9@execution 0");
        CHECK(job->ausfuehrung == 0);
        const auto stage = std::find_if(z.balken.begin(), z.balken.end(), [](const diagramm::ZeitBalken& b) {
            return b.art == diagramm::ZeitBalkenArt::Stage && b.stage == 0;
        });
        REQUIRE(stage != z.balken.end());
        CHECK(stage->plan == "local-9@execution 0");
        CHECK(std::find(stage->angaben.begin(), stage->angaben.end(), "SQL execution 0: count at NativeMethodAccessorImpl.java:0") !=
              stage->angaben.end());
    }

    TEST_CASE("die Stelle im Log: nur Python ist eine Stelle im Code")
    {
        const auto s = projekt::aufrufstelleAus("collect at C:\\Users\\x\\lager.py:90");
        REQUIRE(s);
        CHECK(s->methode == "collect");
        CHECK(s->pfad == "C:\\Users\\x\\lager.py");
        CHECK(s->zeile == 90);
        const auto n = projekt::aufrufstelleAus("count at NativeMethodAccessorImpl.java:0");
        REQUIRE(n);
        CHECK(n->methode == "count");
        CHECK(n->pfad.empty());
        CHECK(projekt::aufrufstelleAus("$anonfun$withThreadLocalCaptured$1 at CompletableFuture.java:1768")->pfad.empty());
        CHECK_FALSE(projekt::aufrufstelleAus("irgendwas"));
    }

    TEST_CASE("L5 im Skript: ueber den Pfad, ohne Stelle ueber den Namen zwischen zwei Ankern, nicht eindeutig: Hinweis")
    {
        const std::filesystem::path ordner = ordnerMit("c2d_test_laufcode");
        std::ofstream(ordner / "lager.py") << kSkript;
        const std::string py = (ordner / "lager.py").string();
        Log log;
        log.job("reduceByKey at " + py + ":6", "collect at " + py + ":7");  // eine RDD-Aktion: ein Job ohne Ausfuehrung
        log.job("parquet at NativeMethodAccessorImpl.java:0");              // ohne Ausfuehrung, ohne Stelle: keine Aktion
        log.ausfuehrung(0, "count at NativeMethodAccessorImpl.java:0");
        log.ausfuehrung(1, "showString at NativeMethodAccessorImpl.java:0");
        log.ausfuehrung(2, "take at " + py + ":11");
        log.ausfuehrung(3, "count at NativeMethodAccessorImpl.java:0");
        log.ausfuehrung(4, "count at NativeMethodAccessorImpl.java:0");
        log.ausfuehrung(5, "take at " + py + ":14");
        std::ofstream(ordner / "local-9") << log.fertig();

        const projekt::Projekt p = projekt::liesOrdner(ordner);
        const std::size_t logDatei = p.finde("local-9");
        const std::size_t code = p.finde("lager.py");
        REQUIRE(logDatei != projekt::kKeine);
        REQUIRE(code != projekt::kKeine);
        const projekt::Laufverbindung v = projekt::verbindeLauf(p, logDatei);
        REQUIRE(v.aktionen.size() == 7);  // der Job ohne Ausfuehrung und ohne Stelle zaehlt nicht
        const auto bei = [&](const projekt::LaufAktion* a) { return a != nullptr && a->stellen.size() == 1 ? a->stellen[0].zeile : -1; };
        CHECK(bei(aktion(v, "collect")) == 7);
        CHECK_FALSE(aktion(v, "collect")->nachName);
        CHECK(bei(aktion(v, "count")) == 9);
        CHECK(aktion(v, "count")->nachName);
        CHECK(bei(aktion(v, "showString")) == 10);
        CHECK(bei(aktion(v, "take")) == 11);
        CHECK(bei(aktion(v, "take", 1)) == 14);
        // zwischen den beiden take: zwei count im Log und im Text — keine Verbindung, je ein Hinweis
        CHECK(aktion(v, "count", 1)->stellen.empty());
        CHECK(aktion(v, "count", 2)->stellen.empty());
        CHECK(hatHinweis(v.hinweise, "lager.py:12: count() is not linked"));
        CHECK(hatHinweis(v.hinweise, "lager.py:13: count() is not linked"));
        // die Stage des RDD-Schritts
        REQUIRE(v.stages.size() == 1);
        CHECK(v.stages[0].stage == 0);
        CHECK(v.stages[0].stellen.front().zeile == 6);
        // die Zeitachse: Stage 0 steht an Zeile 6, Job 2 (show) an Zeile 10
        CHECK(projekt::codestellenVon(v, 0, 0).front().zeile == 6);
        CHECK(projekt::codestellenVon(v, 3, -1).front().zeile == 10);
        // umgekehrt: aus dem Code in den Lauf
        const std::vector<projekt::LaufTreffer> an10 = projekt::laeufeAn(p, code, 10);
        REQUIRE(an10.size() == 1);
        CHECK(an10[0].log == logDatei);
        CHECK(an10[0].jobs == std::vector<int>{3});
        const std::vector<projekt::LaufTreffer> an6 = projekt::laeufeAn(p, code, 6);
        REQUIRE(an6.size() == 1);
        CHECK(an6[0].stage == 0);
        // die Knoten des Datenflusses kennen ihre Methode
        const diagramm::Datenflussdiagramm f = projekt::datenflussIn(p, code, "");
        CHECK(std::any_of(f.knoten.begin(), f.knoten.end(), [](const diagramm::DatenflussKnoten& k) {
            return k.methode == "show" && k.zeile == 10;
        }));
        std::filesystem::remove_all(ordner);
    }

    TEST_CASE("L5 im Notebook: die Zelle ueber den Hash ihres Texts; geaendert ueber Name und Zeile, mit Hinweis")
    {
        const std::filesystem::path ordner = ordnerMit("c2d_test_laufcode_nb");
        std::ofstream(ordner / "probe.ipynb") << kNotebook;
        Log log;
        const std::string temp = "C:\\Users\\x\\AppData\\Local\\Temp\\ipykernel_49256\\";
        log.job("reduceByKey at " + temp + "2250601113.py:2", "collect at " + temp + "3691164798.py:1");
        log.ausfuehrung(0, "count at NativeMethodAccessorImpl.java:0");
        log.ausfuehrung(1, "collect at " + temp + "1421375433.py:3");
        std::ofstream(ordner / "local-9") << log.fertig();
        // ein zweiter Lauf: die dritte Zelle wurde danach geaendert (anderer Hash), ihr collect steht noch auf Zeile 3
        Log alt;
        alt.ausfuehrung(0, "collect at " + temp + "123.py:3");
        std::ofstream(ordner / "local-8") << alt.fertig();

        const projekt::Projekt p = projekt::liesOrdner(ordner);
        const std::size_t nb = p.finde("probe.ipynb");
        REQUIRE(nb != projekt::kKeine);
        const std::vector<projekt::Zelle>& zellen = p.dateien[nb].zellen;
        REQUIRE(zellen.size() == 3);
        CHECK(zellen[0].kernHash == 2250601113u);  // nachgerechnet mit ipykernel (compiler.py)
        CHECK(zellen[1].kernHash == 3691164798u);
        CHECK(zellen[2].kernHash == 1421375433u);

        const projekt::Laufverbindung v = projekt::verbindeLauf(p, p.finde("local-9"));
        REQUIRE(v.aktionen.size() == 3);
        CHECK(v.aktionen[0].stellen == std::vector<projekt::Codestelle>{{nb, 6}});   // Zelle 2, Zeile 1
        CHECK(v.aktionen[1].stellen == std::vector<projekt::Codestelle>{{nb, 10}});  // count, ueber den Namen
        CHECK(v.aktionen[1].nachName);
        CHECK(v.aktionen[2].stellen == std::vector<projekt::Codestelle>{{nb, 11}});  // Zelle 3, Zeile 3
        REQUIRE(v.stages.size() == 1);
        CHECK(v.stages[0].stellen == std::vector<projekt::Codestelle>{{nb, 3}});    // reduceByKey, Zelle 1, Zeile 2
        CHECK(v.hinweise.empty());

        const projekt::Laufverbindung w = projekt::verbindeLauf(p, p.finde("local-8"));
        REQUIRE(w.aktionen.size() == 1);
        CHECK(w.aktionen[0].stellen == std::vector<projekt::Codestelle>{{nb, 11}});
        CHECK(hatHinweis(w.hinweise, "cell changed since the run"));
        std::filesystem::remove_all(ordner);
    }

    TEST_CASE("L5-1: ohne Python-Stelle im ganzen Log ein Hinweis; ein fremder Pfad ueber den Dateinamen")
    {
        Log log;
        log.ausfuehrung(0, "showString at NativeMethodAccessorImpl.java:0");
        const projekt::Projekt p = projekt::liesText(log.fertig(), "local-9");
        const projekt::Laufverbindung v = projekt::verbindeLauf(p, 0);
        CHECK(hatHinweis(v.hinweise, "no Python location in the log"));

        const std::filesystem::path ordner = ordnerMit("c2d_test_laufcode_fremd");
        std::ofstream(ordner / "lager.py") << kSkript;
        Log fremd;
        fremd.ausfuehrung(0, "take at /home/spark/job/lager.py:11");
        std::ofstream(ordner / "local-9") << fremd.fertig();
        const projekt::Projekt q = projekt::liesOrdner(ordner);
        const projekt::Laufverbindung w = projekt::verbindeLauf(q, q.finde("local-9"));
        REQUIRE(w.aktionen.size() == 1);
        CHECK(w.aktionen[0].stellen == std::vector<projekt::Codestelle>{{q.finde("lager.py"), 11}});
        std::filesystem::remove_all(ordner);
    }
}
