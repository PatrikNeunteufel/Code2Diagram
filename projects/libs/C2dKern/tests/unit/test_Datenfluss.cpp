/**
 * @file test_Datenfluss.cpp
 * @brief Spark Schicht 1 (Entscheide F1-F6): Aufrufketten, Verfolgung von einer Sitzung aus, schmale
 *        Schritte zusammen, Shuffles, Jobs, Stages, Views, Pfade, der Kopf Session
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Datenfluss.hpp"
#include "c2d/projekt/Datenfluss.hpp"
#include "c2d/projekt/Projekt.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;
using diagramm::DatenflussKantenArt;
using diagramm::DatenflussKnotenArt;
using diagramm::Datenflussdiagramm;

namespace
{

const modell::Funktion& funktion(const projekt::Projekt& p, std::string_view name)
{
    for (const modell::Funktion& f : p.dateien[0].ergebnis.einheit.funktionen)
        if (f.vollerName() == name) return f;
    FAIL("keine Funktion " << name);
    return p.dateien[0].ergebnis.einheit.funktionen.front();
}

/// Die erste Anweisung (in Textfolge) mit Ketten.
const modell::Anweisung* ersteMitKetten(const modell::Anweisung& a)
{
    if (!a.ketten.empty()) return &a;
    for (const auto& k : a.kinder)
        if (const modell::Anweisung* x = ersteMitKetten(*k)) return x;
    return nullptr;
}

std::size_t zahl(const Datenflussdiagramm& d, DatenflussKnotenArt art)
{
    return static_cast<std::size_t>(
        std::count_if(d.knoten.begin(), d.knoten.end(), [&](const diagramm::DatenflussKnoten& k) { return k.art == art; }));
}

std::size_t knoten(const Datenflussdiagramm& d, DatenflussKnotenArt art, std::string_view titelBeginn)
{
    for (std::size_t i = 0; i < d.knoten.size(); ++i)
        if (d.knoten[i].art == art && d.knoten[i].titel.starts_with(titelBeginn)) return i;
    return projekt::kKeine;
}

/// Der Knoten Schritte, der diese Zeile traegt.
std::size_t schritte(const Datenflussdiagramm& d, std::string_view zeile)
{
    for (std::size_t i = 0; i < d.knoten.size(); ++i)
        if (d.knoten[i].art == DatenflussKnotenArt::Schritte &&
            std::find(d.knoten[i].zeilen.begin(), d.knoten[i].zeilen.end(), zeile) != d.knoten[i].zeilen.end())
            return i;
    return projekt::kKeine;
}

bool kante(const Datenflussdiagramm& d, std::size_t von, std::size_t nach, DatenflussKantenArt art = DatenflussKantenArt::Fluss)
{
    return std::any_of(d.kanten.begin(), d.kanten.end(),
                       [&](const diagramm::DatenflussKante& k) { return k.von == von && k.nach == nach && k.art == art; });
}

bool hatZeile(const diagramm::DatenflussKnoten& k, std::string_view z)
{
    return std::find(k.zeilen.begin(), k.zeilen.end(), z) != k.zeilen.end();
}

bool meldet(const Datenflussdiagramm& d, std::string_view anfang)
{
    return std::any_of(d.hinweise.begin(), d.hinweise.end(),
                       [&](const diagramm::DatenflussHinweis& h) { return h.text.starts_with(anfang); });
}

constexpr std::string_view kJob = R"(from pyspark.sql import SparkSession, functions as F

def main(ein, aus):
    spark = (SparkSession.builder.appName("Lager")
             .master("local[4]")
             .config("spark.sql.shuffle.partitions", 8)
             .getOrCreate())
    df = (spark.read.format("csv")
          .option("header", "true")
          .load(ein))
    df = df.filter(F.col("menge") > 0).withColumn("wert", F.col("menge") * F.col("preis"))
    summe = df.groupBy("lager").agg(F.sum("wert").alias("summe"))
    summe.write.mode("overwrite").parquet(aus)
    fertig = spark.read.parquet(aus)
    print(fertig.count())
)";

}  // namespace

TEST_SUITE("Datenfluss")
{
    TEST_CASE("Ketten (F1): Klammer um die Kette, Ziel, Argumente, eine Kette in einem Argument")
    {
        const projekt::Projekt p = projekt::liesText("def f(spark, a, b, p):\n"
                                                     "    df = (spark.read.format(\"csv\")\n"
                                                     "          .load(p))\n"
                                                     "    r = a.join(b.select(\"id\"), \"id\", how=\"left\")\n",
                                                     "k.py");
        const modell::Funktion& f = funktion(p, "f");
        REQUIRE(f.rumpf);
        const modell::Anweisung* erste = ersteMitKetten(*f.rumpf);
        REQUIRE(erste != nullptr);
        REQUIRE(erste->ketten.size() == 1);
        const modell::Aufrufkette& k = erste->ketten[0];
        CHECK(k.ziel == "df");
        CHECK(k.ganz);
        REQUIRE(k.glieder.size() == 4);
        CHECK(k.glieder[0].name == "spark");
        CHECK(k.glieder[1].name == "read");
        CHECK_FALSE(k.glieder[1].aufruf);
        CHECK(k.glieder[2].argumente == std::vector<std::string>{"\"csv\""});
        CHECK(k.glieder[3].name == "load");
        CHECK(k.zeile == 2);

        const modell::Anweisung& zweite = *f.rumpf->kinder[1];
        REQUIRE(zweite.ketten.size() == 2);
        CHECK(zweite.ketten[0].ziel == "r");
        CHECK(zweite.ketten[0].glieder[1].argumente.size() == 3);
        CHECK(zweite.ketten[0].glieder[1].argumente[2] == "how=\"left\"");
        const modell::Aufrufkette& innen = zweite.ketten[1];
        CHECK(innen.eltern == 0);
        CHECK(innen.glied == 1);
        CHECK(innen.argument == 0);
        CHECK(innen.ganz);
        CHECK(innen.glieder[0].name == "b");
    }

    TEST_CASE("Ein Job: Quelle, Schritte zusammen, Shuffle mit Partitionen, Jobs, Stages, same path, Session")
    {
        const projekt::Projekt p = projekt::liesText(kJob, "job.py");
        const Datenflussdiagramm d = projekt::datenflussIn(p, 0, "main");
        // Session: appName, master, config
        const std::size_t s = knoten(d, DatenflussKnotenArt::Sitzung, "Session");
        REQUIRE(s != projekt::kKeine);
        CHECK(hatZeile(d.knoten[s], "appName: \"Lager\""));
        CHECK(hatZeile(d.knoten[s], "master: \"local[4]\""));
        CHECK(hatZeile(d.knoten[s], "spark.sql.shuffle.partitions: 8"));
        // Quelle mit Format, Pfad und Option
        const std::size_t q = knoten(d, DatenflussKnotenArt::Quelle, "read csv");
        REQUIRE(q != projekt::kKeine);
        CHECK(d.knoten[q].zeilen.front() == "ein");
        CHECK(hatZeile(d.knoten[q], "option(\"header\", \"true\")"));
        // filter und withColumn in einem Knoten
        const std::size_t sch = schritte(d, "filter(F.col(\"menge\") > 0)");
        REQUIRE(sch != projekt::kKeine);
        CHECK(d.knoten[sch].zeilen.size() == 2);
        CHECK(kante(d, q, sch));
        // groupBy mit agg, Partitionen aus der Session
        const std::size_t g = knoten(d, DatenflussKnotenArt::Shuffle, "groupBy(\"lager\")");
        REQUIRE(g != projekt::kKeine);
        CHECK(hatZeile(d.knoten[g], "agg(F.sum(\"wert\").alias(\"summe\"))"));
        CHECK(hatZeile(d.knoten[g], "partitions: 8 (spark.sql.shuffle.partitions)"));
        CHECK(kante(d, sch, g));
        // zwei Jobs: write, count; der Pfad verbindet sie
        CHECK(d.jobs == 2);
        const std::size_t w = knoten(d, DatenflussKnotenArt::Aktion, "Job 1: write");
        const std::size_t c = knoten(d, DatenflussKnotenArt::Aktion, "Job 2: count()");
        REQUIRE(w != projekt::kKeine);
        REQUIRE(c != projekt::kKeine);
        CHECK(hatZeile(d.knoten[w], "parquet(aus)"));
        const std::size_t q2 = knoten(d, DatenflussKnotenArt::Quelle, "read parquet");
        REQUIRE(q2 != projekt::kKeine);
        CHECK(kante(d, w, q2, DatenflussKantenArt::Pfad));
        CHECK(kante(d, q2, c));
        // Stages: (Quelle, Schritte), (Shuffle, write), (read parquet, count)
        CHECK(d.stages == 3);
        CHECK(d.knoten[q].gruppe == d.knoten[sch].gruppe);
        CHECK(d.knoten[g].gruppe == d.knoten[w].gruppe);
        CHECK(d.knoten[g].gruppe != d.knoten[sch].gruppe);
        CHECK(d.knoten[s].gruppe == projekt::kKeine);  // der Kopf steht in keiner Stage
        CHECK(projekt::datenfluesseIn(p, 0) == std::vector<std::string>{"main"});
    }

    TEST_CASE("Zweige, RDD, cache und neu gerechnet, Views und spark.sql, eine Funktion liefert die Sitzung")
    {
        const projekt::Projekt p = projekt::liesText(
            "from pyspark import SparkContext\n"
            "from pyspark.sql import SparkSession\n"
            "def baue():\n"
            "    return SparkSession.builder.getOrCreate()\n"
            "def woerter(pfad, klein):\n"
            "    sc = SparkContext(\"local[2]\", \"Woerter\")\n"
            "    zeilen = sc.textFile(pfad)\n"
            "    if klein:\n"
            "        zeilen = zeilen.sample(False, 0.1)\n"
            "    (paare, rest) = zeilen.flatMap(lambda z: z.split(\" \")).map(lambda w: (w, 1)).randomSplit([0.9, 0.1])\n"
            "    zahlen = paare.reduceByKey(lambda a, b: a + b)\n"
            "    zahlen.collect()\n"
            "    zahlen.count()\n"
            "def berichte():\n"
            "    spark = baue()\n"
            "    df = spark.range(10)\n"
            "    df.createOrReplaceTempView(\"t\")\n"
            "    spark.sql(\"SELECT id FROM t WHERE id > 3\").show()\n"
            "    df.cache()\n"
            "    df.count()\n"
            "    df.collect()\n",
            "rdd.py");
        const Datenflussdiagramm w = projekt::datenflussIn(p, 0, "woerter");
        const std::size_t q = knoten(w, DatenflussKnotenArt::Quelle, "textFile");
        REQUIRE(q != projekt::kKeine);
        const std::size_t probe = schritte(w, "sample(False, 0.1)");
        const std::size_t fm = schritte(w, "flatMap(lambda z: z.split(\" \"))");
        REQUIRE(probe != projekt::kKeine);
        REQUIRE(fm != projekt::kKeine);
        CHECK(kante(w, q, probe));
        CHECK(kante(w, probe, fm));
        CHECK(kante(w, q, fm));  // ohne den Zweig: direkt aus der Quelle
        CHECK(w.knoten[fm].zeilen.size() == 3);  // flatMap, map, randomSplit in einem Knoten; `(paare, rest) =` verfolgt
        const std::size_t r = knoten(w, DatenflussKnotenArt::Shuffle, "reduceByKey");
        REQUIRE(r != projekt::kKeine);
        CHECK(hatZeile(w.knoten[r], "partitions: default"));
        // zwei Jobs aus demselben RDD ohne cache: neu gerechnet
        CHECK(w.jobs == 2);
        CHECK(hatZeile(w.knoten[r], "recomputed by Job 2"));
        CHECK(meldet(w, "recomputed by Job 2"));
        const std::size_t kopf = knoten(w, DatenflussKnotenArt::Sitzung, "Session");
        REQUIRE(kopf != projekt::kKeine);
        CHECK(hatZeile(w.knoten[kopf], "master: \"local[2]\""));

        const Datenflussdiagramm b = projekt::datenflussIn(p, 0, "berichte");
        const std::size_t range = knoten(b, DatenflussKnotenArt::Quelle, "range(10)");
        const std::size_t sql = knoten(b, DatenflussKnotenArt::Sql, "spark.sql");
        REQUIRE(range != projekt::kKeine);
        REQUIRE(sql != projekt::kKeine);
        CHECK(hatZeile(b.knoten[range], "view t"));
        CHECK(kante(b, range, sql, DatenflussKantenArt::Pfad));
        CHECK(hatZeile(b.knoten[sql], "reads t"));
        CHECK(b.knoten[sql].abfrage == "berichte@18");
        CHECK(b.knoten[range].zwischengespeichert);
        CHECK_FALSE(meldet(b, "recomputed"));  // nach cache rechnet nur der erste Job
        CHECK(b.jobs == 3);
        // spark.sql beginnt eine Stage, show() laeuft in ihr
        const std::size_t zeige = knoten(b, DatenflussKnotenArt::Aktion, "Job 1: show()");
        REQUIRE(zeige != projekt::kKeine);
        CHECK(b.knoten[sql].gruppe != projekt::kKeine);
        CHECK(b.knoten[sql].gruppe == b.knoten[zeige].gruppe);
        CHECK(b.knoten[sql].gruppe != b.knoten[range].gruppe);
    }

    TEST_CASE("Notebook: spark vorgegeben, Zellen teilen Namen, eine Zelle zeigt, was sie aus frueheren liest")
    {
        const projekt::Projekt p = projekt::liesText("# Databricks notebook source\n"
                                                     "df = spark.read.table(\"roh.kunden\")\n"
                                                     "\n"
                                                     "# COMMAND ----------\n"
                                                     "\n"
                                                     "gut = df.filter(\"aktiv\")\n"
                                                     "gut.write.saveAsTable(\"gold.kunden\")\n"
                                                     "\n"
                                                     "# COMMAND ----------\n"
                                                     "\n"
                                                     "spark.table(\"gold.kunden\").count()\n",
                                                     "nb.py");
        REQUIRE_FALSE(p.dateien[0].zellen.empty());
        const Datenflussdiagramm alle = projekt::datenflussIn(p, 0, "");
        const std::size_t roh = knoten(alle, DatenflussKnotenArt::Quelle, "read table");
        REQUIRE(roh != projekt::kKeine);
        CHECK(alle.knoten[roh].tabelle == "roh.kunden");
        const std::size_t gold = knoten(alle, DatenflussKnotenArt::Quelle, "table");
        const std::size_t schreib = knoten(alle, DatenflussKnotenArt::Aktion, "Job 1: write");
        REQUIRE(gold != projekt::kKeine);
        REQUIRE(schreib != projekt::kKeine);
        CHECK(kante(alle, schreib, gold, DatenflussKantenArt::Pfad));
        CHECK(alle.jobs == 2);

        const Datenflussdiagramm zelle = projekt::datenflussIn(p, 0, "<cell 2>");
        CHECK(knoten(zelle, DatenflussKnotenArt::Quelle, "read table") != projekt::kKeine);  // aus Zelle 1 gelesen
        CHECK(knoten(zelle, DatenflussKnotenArt::Aktion, "Job 1: write") != projekt::kKeine);
        CHECK(knoten(zelle, DatenflussKnotenArt::Aktion, "Job 2") == projekt::kKeine);         // Zelle 3
        const std::vector<std::string> mit = projekt::datenfluesseIn(p, 0);
        CHECK(mit == std::vector<std::string>{"<cell 1>", "<cell 2>", "<cell 3>"});
    }

    TEST_CASE("Anordnung: je Stage ein Band, gestapelt je Funktion — kein Rahmen ueber fremden Knoten oder Rahmen")
    {
        const projekt::Projekt p = projekt::liesText(
            "from pyspark.sql import SparkSession\n"
            "def sitzung():\n"
            "    return SparkSession.builder.appName(\"A\").getOrCreate()\n"
            "def eins(a, b):\n"
            "    spark = sitzung()\n"
            "    x = spark.read.parquet(a).filter(\"k > 0\")\n"
            "    y = x.groupBy(\"k\").count()\n"
            "    z = y.orderBy(\"k\")\n"
            "    z.write.parquet(b)\n"
            "    spark.read.parquet(b).count()\n"
            "def zwei(a):\n"
            "    spark = sitzung()\n"
            "    spark.read.json(a).distinct().show()\n",
            "an.py");
        const Datenflussdiagramm d = projekt::datenflussIn(p, 0, "");
        // die Funktion, die nur die Sitzung baut, hat keinen Rahmen
        CHECK(std::none_of(d.rahmen.begin(), d.rahmen.end(), [](const diagramm::DatenflussRahmen& r) { return r.titel == "sitzung"; }));
        for (const bool nachRechts : {false, true})
        {
            diagramm::AnordnungOptionen o;
            o.nachRechts = nachRechts;
            const diagramm::FlussAnordnung a = diagramm::ordneAn(d, o);
            REQUIRE(a.gruppen.size() == d.rahmen.size());
            const auto schneidet = [](const diagramm::Rechteck& f, const diagramm::Rechteck& x) {
                return x.x < f.x + f.breite && x.x + x.breite > f.x && x.y < f.y + f.hoehe && x.y + x.hoehe > f.y;
            };
            const auto vorfahr = [&](std::size_t oben, std::size_t r) {
                for (; r != projekt::kKeine; r = d.rahmen[r].eltern)
                    if (r == oben) return true;
                return false;
            };
            CAPTURE(nachRechts);
            for (std::size_t g = 0; g < d.rahmen.size(); ++g)
            {
                CAPTURE(d.rahmen[g].titel);
                for (std::size_t k = 0; k < d.knoten.size(); ++k)
                    if (!vorfahr(g, d.knoten[k].gruppe) && !(d.knoten[k].gruppe != projekt::kKeine && vorfahr(d.knoten[k].gruppe, g)))
                        CHECK_FALSE(schneidet(a.gruppen[g], a.knoten[k]));
                for (std::size_t h = g + 1; h < d.rahmen.size(); ++h)
                {
                    CAPTURE(d.rahmen[h].titel);
                    if (!vorfahr(g, h) && !vorfahr(h, g)) CHECK_FALSE(schneidet(a.gruppen[g], a.gruppen[h]));
                }
            }
            if (nachRechts) continue;
            // in eins: die Stage von `read parquet b` teilt keine Schicht mit der davor — sie steht am linken Rand
            // der Funktion statt rechts daneben (keine Treppe)
            const std::size_t zweite = knoten(d, DatenflussKnotenArt::Aktion, "Job 2");
            const std::size_t erste = knoten(d, DatenflussKnotenArt::Quelle, "read parquet");
            REQUIRE(zweite != projekt::kKeine);
            REQUIRE(erste != projekt::kKeine);
            CHECK(a.gruppen[d.knoten[zweite].gruppe].x <= a.gruppen[d.knoten[erste].gruppe].x + 1);
        }
    }

    TEST_CASE("Was nicht verfolgt wird: ein Parameter (Hinweis), pandas; eine UDF und f(df) als Ziel")
    {
        const projekt::Projekt p = projekt::liesText(
            "import pandas as pd\n"
            "from pyspark.sql import SparkSession\n"
            "from pyspark.sql.functions import udf\n"
            "def sauber(x):\n"
            "    return x.strip()\n"
            "def speichere(df):\n"
            "    df.withColumn(\"a\", df.b).write.parquet(\"/x\")\n"
            "def lauf():\n"
            "    t = pd.read_csv(\"a.csv\")\n"
            "    t.groupby(\"k\").count()\n"
            "    spark = SparkSession.builder.getOrCreate()\n"
            "    s = udf(sauber)\n"
            "    df = spark.read.json(\"/e\").withColumn(\"n\", s(\"name\"))\n"
            "    speichere(df)\n",
            "u.py");
        const Datenflussdiagramm param = projekt::datenflussIn(p, 0, "speichere");
        CHECK(param.knoten.empty());
        CHECK(meldet(param, "parameter 'df' is used like a DataFrame"));
        const Datenflussdiagramm d = projekt::datenflussIn(p, 0, "lauf");
        CHECK(zahl(d, DatenflussKnotenArt::Shuffle) == 0);  // pandas groupby zaehlt nicht
        CHECK(zahl(d, DatenflussKnotenArt::Aktion) == 0);
        const std::size_t sch = schritte(d, "withColumn(\"n\", s(\"name\"))");
        REQUIRE(sch != projekt::kKeine);
        CHECK(d.knoten[sch].ziele == std::vector<std::string>{"sauber"});
        const std::size_t r = knoten(d, DatenflussKnotenArt::Aufruf, "speichere(df)");
        REQUIRE(r != projekt::kKeine);
        CHECK(d.knoten[r].ziele == std::vector<std::string>{"speichere"});
        CHECK(kante(d, sch, r));
        CHECK(projekt::datenfluesseIn(p, 0) == std::vector<std::string>{"lauf"});
    }
}
