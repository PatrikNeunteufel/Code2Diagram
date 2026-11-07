/**
 * @file test_Lauf.cpp
 * @brief Spark Schicht 3 (Entscheide L1-L4, L7): der Event-Log als Lauf, die Zeitachse daraus (Bahnen je Kern,
 *        Ueberlappung, Offenes, Fehlschlaege, entfernte Executoren, Zusammenfassen) und ihre Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Zeitachse.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;
using diagramm::ZeitBahnArt;
using diagramm::ZeitBalkenArt;
using diagramm::Zeitachse;

namespace
{

std::string task(const char* art, long long id, int stage, const char* executor, long long start, long long ende,
                 const std::string& grund = "{\"Reason\":\"Success\"}")
{
    std::string info = "{\"Task ID\":" + std::to_string(id) + ",\"Attempt\":0,\"Executor ID\":\"" + executor +
                       "\",\"Launch Time\":" + std::to_string(start) + ",\"Finish Time\":" + std::to_string(ende) +
                       ",\"Speculative\":false,\"Accumulables\":[{\"Name\":\"internal.metrics.peakExecutionMemory\",\"Update\":\"2048\"}]}";
    std::string z = std::string("{\"Event\":\"") + art + "\",\"Stage ID\":" + std::to_string(stage) + ",\"Stage Attempt ID\":0,\"Task Info\":" + info;
    if (std::string(art) == "SparkListenerTaskEnd")
        z += ",\"Task End Reason\":" + grund +
             ",\"Task Metrics\":{\"Executor Run Time\":5,\"JVM GC Time\":1,\"Memory Bytes Spilled\":0,\"Disk Bytes Spilled\":4096,"
             "\"Shuffle Read Metrics\":{\"Remote Bytes Read\":100,\"Local Bytes Read\":20},"
             "\"Shuffle Write Metrics\":{\"Shuffle Bytes Written\":300},\"Input Metrics\":{\"Bytes Read\":0},"
             "\"Output Metrics\":{\"Bytes Written\":0}}";
    return z + "}\n";
}

// Ein kleiner Lauf: zwei Executoren (einer mit zwei Kernen), zwei Jobs, drei Stages; ein Task ueberlappt den vorigen um
// 3 ms, einer schlaegt fehl, einer endet nie, Stage 2 hat keine Submission Time und wird nie fertig, Executor 2 wird
// entfernt.
std::string kleinerLog()
{
    std::string s;
    s += R"({"Event":"SparkListenerLogStart","Spark Version":"3.5.1"})" "\n";
    s += R"({"Event":"SparkListenerApplicationStart","App Name":"lager","App ID":"local-1","Timestamp":1000,"User":"x"})" "\n";
    s += R"({"Event":"SparkListenerEnvironmentUpdate","Spark Properties":{"spark.master":"spark://x:7077","spark.task.cpus":"1"}})" "\n";
    s += R"({"Event":"SparkListenerExecutorAdded","Timestamp":1010,"Executor ID":"1","Executor Info":{"Host":"a","Total Cores":2}})" "\n";
    s += R"({"Event":"SparkListenerExecutorAdded","Timestamp":1010,"Executor ID":"2","Executor Info":{"Host":"b","Total Cores":1}})" "\n";
    s += R"({"Event":"SparkListenerJobStart","Job ID":0,"Submission Time":1100,"Stage IDs":[0,1],"Properties":{"spark.sql.execution.id":"7"}})" "\n";
    s += R"({"Event":"SparkListenerStageSubmitted","Stage Info":{"Stage ID":0,"Stage Attempt ID":0,"Stage Name":"count at lager.py:12","Number of Tasks":3,"RDD Info":[{"Callsite":"textFile at lager.py:10"}],"Parent IDs":[],"Submission Time":1100}})" "\n";
    s += task("SparkListenerTaskStart", 0, 0, "1", 1100, 0);
    s += task("SparkListenerTaskStart", 1, 0, "1", 1100, 0);
    s += task("SparkListenerTaskStart", 2, 0, "2", 1100, 0);
    s += task("SparkListenerTaskEnd", 0, 0, "1", 1100, 1500);
    s += task("SparkListenerTaskEnd", 1, 0, "1", 1100, 1503);
    s += task("SparkListenerTaskEnd", 2, 0, "2", 1100, 1600,
              R"({"Reason":"ExceptionFailure","Class Name":"java.lang.RuntimeException","Description":"kaputt\nzweite Zeile"})");
    // auf Executor 2 (ein Kern) beginnt der naechste 3 ms vor dem Ende des vorigen
    s += task("SparkListenerTaskStart", 3, 0, "2", 1597, 0);
    s += task("SparkListenerTaskEnd", 3, 0, "2", 1597, 1700);
    s += R"({"Event":"SparkListenerStageCompleted","Stage Info":{"Stage ID":0,"Stage Attempt ID":0,"Stage Name":"count at lager.py:12","Number of Tasks":3,"Parent IDs":[],"Submission Time":1100,"Completion Time":1700}})" "\n";
    s += R"({"Event":"SparkListenerJobEnd","Job ID":0,"Completion Time":1710,"Job Result":{"Result":"JobSucceeded"}})" "\n";
    s += R"({"Event":"SparkListenerJobStart","Job ID":1,"Submission Time":2000,"Stage IDs":[2]})" "\n";
    s += R"({"Event":"SparkListenerStageSubmitted","Stage Info":{"Stage ID":2,"Stage Attempt ID":0,"Stage Name":"collect at lager.py:20","Number of Tasks":1,"Parent IDs":[]}})" "\n";
    s += task("SparkListenerTaskStart", 4, 2, "1", 2050, 0);
    s += R"({"Event":"SparkListenerExecutorRemoved","Timestamp":2500,"Executor ID":"2","Removed Reason":"lost worker"})" "\n";
    s += R"({"Event":"SparkListenerApplicationEnd","Timestamp":3000})" "\n";
    return s;
}

const diagramm::ZeitBalken* balken(const Zeitachse& z, ZeitBalkenArt art, std::string_view titelBeginn)
{
    for (const diagramm::ZeitBalken& b : z.balken)
        if (b.art == art && b.titel.starts_with(titelBeginn)) return &b;
    return nullptr;
}

bool hatHinweis(const Zeitachse& z, std::string_view teil)
{
    return std::any_of(z.hinweise.begin(), z.hinweise.end(), [&](const std::string& h) { return h.find(teil) != std::string::npos; });
}

}  // namespace

TEST_SUITE("Spark-Lauf")
{
    TEST_CASE("der Event-Log: Erkennung, Lauf, Executoren, Jobs, Stages, Tasks mit Metriken")
    {
        CHECK(sprache::eventLogText(R"({"Event":"SparkListenerLogStart","Spark Version":"3.5.1"})"));
        CHECK(sprache::eventLogText("{ \"Event\" : \"SparkListenerLogStart\" }"));
        CHECK_FALSE(sprache::eventLogText(R"({"Event":"SparkListenerApplicationStart"})"));
        CHECK_FALSE(sprache::eventLogText("Makefile:\n\tgcc x.c\n"));

        // ohne Endung erkennt das Projekt ihn am Inhalt (L1)
        const projekt::Projekt p = projekt::liesText(kleinerLog(), "local-1");
        REQUIRE(p.dateien.size() == 1);
        CHECK(p.dateien[0].sprache == &sprache::sparkEventLog());
        const std::vector<modell::Lauf>& laeufe = p.dateien[0].ergebnis.einheit.laeufe;
        REQUIRE(laeufe.size() == 1);
        const modell::Lauf& l = laeufe[0];
        CHECK(l.name == "lager");
        CHECK(l.id == "local-1");
        CHECK(l.version == "3.5.1");
        CHECK(l.master == "spark://x:7077");
        CHECK(l.start == 1000);
        CHECK(l.ende == 3000);
        REQUIRE(l.executoren.size() == 2);
        CHECK(l.executoren[0].kerne == 2);
        CHECK(l.executoren[1].entfernt == 2500);
        CHECK(l.executoren[1].grund == "lost worker");
        REQUIRE(l.jobs.size() == 2);
        CHECK(l.jobs[0].sql == "7");
        CHECK(l.jobs[1].ende == 0);
        REQUIRE(l.stages.size() == 2);
        CHECK(l.stages[0].aufrufe == std::vector<std::string>{"textFile at lager.py:10"});
        // Stage 2: keine Submission Time — der Start ihres ersten Tasks
        CHECK(l.stages[1].eingereicht == 2050);
        CHECK(l.stages[1].eingereichtGeschaetzt);
        CHECK(l.stages[1].beendet == 0);
        REQUIRE(l.tasks.size() == 5);
        const auto t2 = std::find_if(l.tasks.begin(), l.tasks.end(), [](const modell::LaufTask& t) { return t.id == 2; });
        CHECK(t2->ergebnis == "ExceptionFailure");
        CHECK(t2->grund == "kaputt");
        CHECK(t2->shuffleGelesen == 120);
        CHECK(t2->shuffleGeschrieben == 300);
        CHECK(t2->spillPlatte == 4096);
        CHECK(t2->spitze == 2048);  // aus den Akkumulatoren
        const auto t4 = std::find_if(l.tasks.begin(), l.tasks.end(), [](const modell::LaufTask& t) { return t.id == 4; });
        CHECK(t4->ende == 0);
    }

    TEST_CASE("die Zeitachse: je Job eine Zeile, je Executor so viele Bahnen wie Kerne, Offenes und Fehlschlaege")
    {
        const projekt::Projekt p = projekt::liesText(kleinerLog(), "local-1");
        const Zeitachse z = diagramm::zeitachseVon(p.dateien[0].ergebnis.einheit.laeufe[0]);
        CHECK(z.anfang == 1000);
        CHECK(z.ende == 3000);
        // zwei Jobs, dann Executor 1 mit zwei Bahnen, Executor 2 mit einer — die Ueberlappung von 3 ms gibt keine zweite
        std::size_t jobs = 0, plaetze1 = 0, plaetze2 = 0;
        for (const diagramm::ZeitBahn& b : z.bahnen)
        {
            if (b.art == ZeitBahnArt::Job && !b.titel.empty()) ++jobs;
            if (b.art == ZeitBahnArt::Executor && b.executor == "1") ++plaetze1;
            if (b.art == ZeitBahnArt::Executor && b.executor == "2") ++plaetze2;
        }
        CHECK(jobs == 2);
        CHECK(plaetze1 == 2);
        CHECK(plaetze2 == 1);
        CHECK_FALSE(hatHinweis(z, "more tasks than"));
        // Stage 0 mit ihren Summen, Stage 2 offen bis zum Ende
        const diagramm::ZeitBalken* s0 = balken(z, ZeitBalkenArt::Stage, "Stage 0: count at lager.py:12");
        REQUIRE(s0 != nullptr);
        CHECK(std::find(s0->angaben.begin(), s0->angaben.end(), "shuffle read: 480 B, write: 1.2 KB") != s0->angaben.end());
        CHECK(std::find(s0->angaben.begin(), s0->angaben.end(), "failed tasks: 1") != s0->angaben.end());
        CHECK(std::find(s0->angaben.begin(), s0->angaben.end(), "rdd: textFile at lager.py:10") != s0->angaben.end());
        const diagramm::ZeitBalken* s2 = balken(z, ZeitBalkenArt::Stage, "Stage 2");
        REQUIRE(s2 != nullptr);
        CHECK(s2->offen);
        CHECK(s2->bis == 3000);
        CHECK(hatHinweis(z, "Stage 2 (collect at lager.py:20) was not completed"));
        CHECK(hatHinweis(z, "Job 1 has no end"));
        CHECK(hatHinweis(z, "1 task(s) without an end"));
        CHECK(hatHinweis(z, "executor 2 was removed: lost worker"));
        // Tasks: der fehlgeschlagene, der offene; Task 3 auf derselben Bahn wie Task 2
        const diagramm::ZeitBalken* t2 = balken(z, ZeitBalkenArt::Task, "2");
        const diagramm::ZeitBalken* t3 = balken(z, ZeitBalkenArt::Task, "3");
        const diagramm::ZeitBalken* t4 = balken(z, ZeitBalkenArt::Task, "4");
        REQUIRE(t2 != nullptr);
        REQUIRE(t3 != nullptr);
        REQUIRE(t4 != nullptr);
        CHECK(t2->fehlgeschlagen);
        CHECK(t2->bahn == t3->bahn);
        CHECK(t4->offen);
        CHECK(t4->bis == 3000);
        CHECK(t2->spalten.size() == 19);
        CHECK(t2->spalten[17].second == "ExceptionFailure");
        // die Bahnen der beiden Tasks auf Executor 1, die gleichzeitig laufen, sind verschieden
        const diagramm::ZeitBalken* t0 = balken(z, ZeitBalkenArt::Task, "0");
        const diagramm::ZeitBalken* t1 = balken(z, ZeitBalkenArt::Task, "1");
        CHECK(t0->bahn != t1->bahn);
        // der entfernte Executor traegt den Zeitpunkt an seiner Bahn
        const auto e2 = std::find_if(z.bahnen.begin(), z.bahnen.end(), [](const diagramm::ZeitBahn& b) { return b.executor == "2"; });
        CHECK(e2->entfernt == 2500);
    }

    TEST_CASE("Anordnung: Zeit linear, jeder Balken in seiner Bahn, Striche der Achse")
    {
        const projekt::Projekt p = projekt::liesText(kleinerLog(), "local-1");
        const Zeitachse z = diagramm::zeitachseVon(p.dateien[0].ergebnis.einheit.laeufe[0]);
        const diagramm::ZeitAnordnung a = diagramm::ordneAn(z);
        REQUIRE(a.balken.size() == z.balken.size());
        REQUIRE(a.bahnen.size() == z.bahnen.size());
        for (std::size_t i = 0; i < z.balken.size(); ++i)
        {
            const diagramm::Rechteck& r = a.balken[i];
            const diagramm::Rechteck& bahn = a.bahnen[z.balken[i].bahn].ort;
            CHECK(r.y >= bahn.y);
            CHECK(r.y + r.hoehe <= bahn.y + bahn.hoehe + 0.01);
            CHECK(r.x >= a.zeit.x - 0.01);
            CHECK(r.x + r.breite <= a.zeit.x + a.zeit.breite + 1.01);
        }
        // gleiche Zeit, gleicher Ort: Task 0 und 1 beginnen beide bei 1100
        const auto t0 = std::find_if(z.balken.begin(), z.balken.end(), [](const diagramm::ZeitBalken& b) { return b.titel == "0"; });
        const auto t1 = std::find_if(z.balken.begin(), z.balken.end(), [](const diagramm::ZeitBalken& b) { return b.titel == "1"; });
        CHECK(a.balken[static_cast<std::size_t>(t0 - z.balken.begin())].x == doctest::Approx(a.balken[static_cast<std::size_t>(t1 - z.balken.begin())].x));
        CHECK(a.achse.size() >= 2);
        CHECK(a.achse.front().text == "0 ms");
        CHECK(a.bahnen[2].titel.empty() == false);
    }

    TEST_CASE("ueber kHoechstensTasks: je Bahn und Stage ein Balken mit der Zahl")
    {
        std::string s;
        s += R"({"Event":"SparkListenerLogStart","Spark Version":"3.5.1"})" "\n";
        s += R"({"Event":"SparkListenerApplicationStart","App Name":"gross","App ID":"local-2","Timestamp":0})" "\n";
        s += R"({"Event":"SparkListenerEnvironmentUpdate","Spark Properties":{"spark.master":"local[2]"}})" "\n";
        for (long long i = 0; i <= static_cast<long long>(diagramm::kHoechstensTasks); ++i)
            s += task("SparkListenerTaskEnd", i, i % 2 == 0 ? 0 : 1, "driver", 10 + i, 11 + i);
        const projekt::Projekt p = projekt::liesText(s, "local-2");
        const Zeitachse z = diagramm::zeitachseVon(p.dateien[0].ergebnis.einheit.laeufe[0]);
        CHECK(z.zusammengefasst);
        CHECK(z.tasks == diagramm::kHoechstensTasks + 1);
        // der Treiber: zwei Kerne aus local[2]
        CHECK(std::count_if(z.bahnen.begin(), z.bahnen.end(), [](const diagramm::ZeitBahn& b) { return b.executor == "driver"; }) == 2);
        std::size_t summe = 0;
        for (const diagramm::ZeitBalken& b : z.balken)
        {
            CHECK(b.art != ZeitBalkenArt::Task);
            if (b.art == ZeitBalkenArt::Zusammen) summe += static_cast<std::size_t>(std::stoul(b.titel));
        }
        CHECK(summe == diagramm::kHoechstensTasks + 1);
        CHECK(hatHinweis(z, "one bar per core and stage"));
        CHECK(ausgabe::csv().schreibe(z, {}).find("no row per task") != std::string::npos);
    }

    TEST_CASE("Formate: SVG, draw.io, Mermaid gantt, csv je Task; DOT sagt ab; eine kaputte Zeile gibt eine Meldung")
    {
        std::string log = kleinerLog();
        log.insert(log.find("{\"Event\":\"SparkListenerApplicationEnd\""), "{kein json\n");
        const projekt::Projekt p = projekt::liesText(log, "local-1.inprogress");
        const std::vector<quelle::Meldung>& m = p.dateien[0].ergebnis.meldungen;
        CHECK(std::any_of(m.begin(), m.end(), [](const quelle::Meldung& x) { return x.text.find("not valid JSON") != std::string::npos; }));
        CHECK(std::any_of(m.begin(), m.end(), [](const quelle::Meldung& x) { return x.text == "run not finished (.inprogress)"; }));
        const Zeitachse z = diagramm::zeitachseVon(p.dateien[0].ergebnis.einheit.laeufe[0]);
        const ausgabe::Optionen o;
        CHECK(ausgabe::svg().kann(ausgabe::Diagrammart::Zeitachse));
        CHECK(ausgabe::csv().kann(ausgabe::Diagrammart::Zeitachse));
        CHECK(ausgabe::mermaid().kann(ausgabe::Diagrammart::Zeitachse));
        CHECK_FALSE(ausgabe::dot().kann(ausgabe::Diagrammart::Zeitachse));
        const std::string svg = ausgabe::svg().schreibe(z, o);
        CHECK(svg.find("Stage 0: count at lager.py:12") != std::string::npos);
        CHECK(svg.find("stroke-dasharray:4 3") != std::string::npos);
        CHECK(svg.find("\xE2\x9C\x97") != std::string::npos);
        CHECK(svg.find("removed") != std::string::npos);
        const std::string mermaid = ausgabe::mermaid().schreibe(z, o);
        CHECK(mermaid.find("gantt\n") != std::string::npos);
        CHECK(mermaid.find("dateFormat x") != std::string::npos);
        CHECK(mermaid.find("section executor 1 core 2") != std::string::npos);
        CHECK(mermaid.find("Stage 0  count at lager.py 12 :") != std::string::npos);  // ohne `:` im Namen
        CHECK(mermaid.find(":crit, ") != std::string::npos);
        CHECK(mermaid.find(":active, ") != std::string::npos);
        const std::string csv = ausgabe::csv().schreibe(z, o);
        CHECK(csv.find("task;attempt;stage;executor;core;start ms;end ms") != std::string::npos);
        CHECK(csv.find("2;0;0;2;1;100;600;500;5;1;0;0;120;300;0;4096;2048;ExceptionFailure;kaputt") != std::string::npos);
        CHECK(std::count(csv.begin(), csv.end(), '\n') == 2 + 5);
        const std::string drawio = ausgabe::drawio().schreibe(z, o);
        CHECK(drawio.find("mxGraphModel") != std::string::npos);
        CHECK(drawio.find("dashed=1;") != std::string::npos);
    }
}
