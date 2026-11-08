/**
 * @file test_Plan.cpp
 * @brief Spark Schicht 2 (Entscheide P2-1 bis P2-4): der Text von explain() als Baum — einfach, formatted mit
 *        Angaben und Unterabfragen, extended, AQE, der eingerueckte Baum der Pruefdateien — und das Diagramm
 *        daraus (Stages, Codegen, Exchanges, Joins, Wiederverwendung), Notebook-Ausgaben, Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Plan.hpp"
#include "c2d/projekt/Datenfluss.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace c2d;
using diagramm::PlanKantenArt;
using diagramm::PlanKnotenArt;
using diagramm::Plandiagramm;

namespace
{

// Wie IR_BD `df_join_pq.explain()` (gekuerzt).
constexpr std::string_view kEinfach = R"PLAN(== Physical Plan ==
AdaptiveSparkPlan isFinalPlan=false
+- Project [name#387, orderstatus#396]
   +- BroadcastHashJoin [custkey#386], [custkey#395], Inner, BuildLeft, false
      :- BroadcastExchange HashedRelationBroadcastMode(List(cast(input[0, int, false] as bigint)),false), [plan_id=2150]
      :  +- Filter isnotnull(custkey#386)
      :     +- FileScan parquet [custkey#386,name#387] Batched: true, DataFilters: [isnotnull(custkey#386)], Format: Parquet, Location: InMemoryFileIndex(1 paths)[file:/data/customer.pq], PartitionFilters: [], PushedFilters: [IsNotNull(custkey)], ReadSchema: struct<custkey:int,name:string>
      +- Filter isnotnull(custkey#395)
         +- FileScan parquet [custkey#395,orderstatus#396] Batched: true, DataFilters: [isnotnull(custkey#395)], Format: Parquet, Location: InMemoryFileIndex(1 paths)[file:/data/orders.pq], PartitionFilters: [], PushedFilters: [IsNotNull(custkey)], ReadSchema: struct<custkey:int,orderstatus:string>
)PLAN";

// Wie Sparks explain.sql.out (EXPLAIN FORMATTED … GROUP BY key ORDER BY key).
constexpr std::string_view kFormatted = R"PLAN(== Physical Plan ==
* Sort (8)
+- Exchange (7)
   +- * HashAggregate (6)
      +- Exchange (5)
         +- * HashAggregate (4)
            +- * Filter (3)
               +- * ColumnarToRow (2)
                  +- Scan parquet spark_catalog.default.explain_temp1 (1)


(1) Scan parquet spark_catalog.default.explain_temp1
Output [2]: [key#x, val#x]
Batched: true
Location [not included in comparison]/{warehouse_dir}/explain_temp1]
PushedFilters: [IsNotNull(key), GreaterThan(key,0)]
ReadSchema: struct<key:int,val:int>

(2) ColumnarToRow [codegen id : 1]
Input [2]: [key#x, val#x]

(3) Filter [codegen id : 1]
Input [2]: [key#x, val#x]
Condition : (isnotnull(key#x) AND (key#x > 0))

(4) HashAggregate [codegen id : 1]
Input [2]: [key#x, val#x]
Keys [1]: [key#x]
Functions [1]: [partial_max(val#x)]
Aggregate Attributes [1]: [max#x]
Results [2]: [key#x, max#x]

(5) Exchange
Input [2]: [key#x, max#x]
Arguments: hashpartitioning(key#x, 4), ENSURE_REQUIREMENTS, [plan_id=x]

(6) HashAggregate [codegen id : 2]
Input [2]: [key#x, max#x]
Keys [1]: [key#x]
Functions [1]: [max(val#x)]
Aggregate Attributes [1]: [max(val#x)#x]
Results [2]: [key#x, max(val#x)#x AS max(val)#x]

(7) Exchange
Input [2]: [key#x, max(val)#x]
Arguments: rangepartitioning(key#x ASC NULLS FIRST, 4), ENSURE_REQUIREMENTS, [plan_id=x]

(8) Sort [codegen id : 3]
Input [2]: [key#x, max(val)#x]
Arguments: [key#x ASC NULLS FIRST], true, 0


-- !query
EXPLAIN FORMATTED
)PLAN";

// Wie Sparks explain.sql.out (EXPLAIN EXTENDED SELECT sum(distinct val) FROM explain_temp1).
constexpr std::string_view kExtended = R"PLAN(-- !query output
== Parsed Logical Plan ==
'Project [unresolvedalias('sum(distinct 'val))]
+- 'UnresolvedRelation [explain_temp1], [], false

== Analyzed Logical Plan ==
sum(DISTINCT val): bigint
Aggregate [sum(distinct val#x) AS sum(DISTINCT val)#xL]
+- SubqueryAlias spark_catalog.default.explain_temp1
   +- Relation spark_catalog.default.explain_temp1[key#x,val#x] parquet

== Optimized Logical Plan ==
Aggregate [sum(distinct val#x) AS sum(DISTINCT val)#xL]
+- Project [val#x]
   +- Relation spark_catalog.default.explain_temp1[key#x,val#x] parquet

== Physical Plan ==
*HashAggregate(keys=[], functions=[sum(distinct val#x)], output=[sum(DISTINCT val)#xL])
+- Exchange SinglePartition, ENSURE_REQUIREMENTS, [plan_id=x]
   +- *HashAggregate(keys=[], functions=[partial_sum(distinct val#x)], output=[sum#xL])
      +- *HashAggregate(keys=[val#x], functions=[], output=[val#x])
         +- Exchange hashpartitioning(val#x, 4), ENSURE_REQUIREMENTS, [plan_id=x]
            +- *HashAggregate(keys=[val#x], functions=[], output=[val#x])
               +- *ColumnarToRow
                  +- FileScan parquet spark_catalog.default.explain_temp1[val#x] Batched: true, DataFilters: [], Format: Parquet, Location [not included in comparison]/{warehouse_dir}/explain_temp1], PartitionFilters: [], PushedFilters: [], ReadSchema: struct<val:int>


-- !query
)PLAN";

constexpr std::string_view kAqe = R"PLAN(== Physical Plan ==
AdaptiveSparkPlan isFinalPlan=true
+- == Final Plan ==
   *(2) HashAggregate(keys=[k#1], functions=[count(1)])
   +- AQEShuffleRead coalesced
      +- ShuffleQueryStage 0
         +- Exchange hashpartitioning(k#1, 200), ENSURE_REQUIREMENTS, [plan_id=10]
            +- *(1) HashAggregate(keys=[k#1], functions=[partial_count(1)])
               +- *(1) LocalTableScan [k#1]
+- == Initial Plan ==
   HashAggregate(keys=[k#1], functions=[count(1)])
   +- Exchange hashpartitioning(k#1, 200), ENSURE_REQUIREMENTS, [plan_id=5]
      +- HashAggregate(keys=[k#1], functions=[partial_count(1)])
         +- LocalTableScan [k#1]
)PLAN";

// Wie Sparks simplified.txt (tpcds-plan-stability), gekuerzt.
constexpr std::string_view kEingerueckt = R"PLAN(TakeOrderedAndProject [a,cnt]
  WholeStageCodegen (3)
    HashAggregate [a,count] [count(1),cnt,count]
      InputAdapter
        Exchange [a] #1
          WholeStageCodegen (2)
            HashAggregate [a] [count,count]
              Project [a]
                BroadcastHashJoin [x,y]
                  BroadcastHashJoin [x,y]
                    Filter [x]
                      ColumnarToRow
                        InputAdapter
                          Scan parquet spark_catalog.default.t [x,a]
                    InputAdapter
                      BroadcastExchange #2
                        WholeStageCodegen (1)
                          Filter [y]
                            ColumnarToRow
                              InputAdapter
                                Scan parquet spark_catalog.default.u [y]
                  InputAdapter
                    ReusedExchange [y] #2
)PLAN";

// Wie Sparks explain.txt (tpcds-plan-stability) mit Unterabfragen, gekuerzt.
constexpr std::string_view kUnterabfragen = R"PLAN(== Physical Plan ==
* BroadcastHashJoin Inner BuildRight (6)
:- * ColumnarToRow (2)
:  +- Scan parquet spark_catalog.default.web_sales (1)
+- BroadcastExchange (5)
   +- * ColumnarToRow (4)
      +- Scan parquet spark_catalog.default.catalog_sales (3)


(1) Scan parquet spark_catalog.default.web_sales
Output [2]: [ws_customer_sk#1, ws_sold_date_sk#2]
PartitionFilters: [isnotnull(ws_sold_date_sk#2), dynamicpruningexpression(ws_sold_date_sk#2 IN dynamicpruning#9)]
PushedFilters: [IsNotNull(ws_customer_sk)]

(2) ColumnarToRow [codegen id : 2]
Input [2]: [ws_customer_sk#1, ws_sold_date_sk#2]

(3) Scan parquet spark_catalog.default.catalog_sales
Output [2]: [cs_customer_sk#3, cs_sold_date_sk#4]
PartitionFilters: [isnotnull(cs_sold_date_sk#4), dynamicpruningexpression(cs_sold_date_sk#4 IN dynamicpruning#9)]

(4) ColumnarToRow [codegen id : 1]
Input [2]: [cs_customer_sk#3, cs_sold_date_sk#4]

(5) BroadcastExchange
Input [2]: [cs_customer_sk#3, cs_sold_date_sk#4]
Arguments: HashedRelationBroadcastMode(List(cast(input[0, int, true] as bigint)),false), [plan_id=1]

(6) BroadcastHashJoin [codegen id : 2]
Left keys [1]: [ws_customer_sk#1]
Right keys [1]: [cs_customer_sk#3]
Join type: Inner
Join condition: None

===== Subqueries =====

Subquery:1 Hosting operator id = 1 Hosting Expression = ws_sold_date_sk#2 IN dynamicpruning#9
BroadcastExchange (9)
+- * Filter (8)
   +- Scan parquet spark_catalog.default.date_dim (7)


(7) Scan parquet spark_catalog.default.date_dim
Output [2]: [d_date_sk#5, d_year#6]

(8) Filter [codegen id : 1]
Input [2]: [d_date_sk#5, d_year#6]
Condition : (d_year#6 = 2002)

(9) BroadcastExchange
Input [1]: [d_date_sk#5]
Arguments: HashedRelationBroadcastMode(List(cast(input[0, int, true] as bigint)),false), [plan_id=2]

Subquery:2 Hosting operator id = 3 Hosting Expression = cs_sold_date_sk#4 IN dynamicpruning#9
)PLAN";

// Eigener Lauf (plaene_aufnehmen.py, Spark 4.3): formatted nach dem Ausfuehren mit AQE, gekuerzt. Hinter der Nummer
// der Stufen steht noch etwas; Statistics() dort ist kein cost.
constexpr std::string_view kAqeFormattedEndgueltig = R"PLAN(== Physical Plan ==
AdaptiveSparkPlan (8)
+- == Final Plan ==
   ResultQueryStage (5)
   +- * HashAggregate (4)
      +- AQEShuffleRead (3), coalesced
         +- ShuffleQueryStage (2), Statistics(sizeInBytes=192.0 B, rowCount=6)
            +- Exchange (1)
+- == Initial Plan ==
   HashAggregate (7)
   +- Exchange (6)


(1) Exchange
Input [2]: [land#66, sum#86]
Arguments: hashpartitioning(land#66, 8), ENSURE_REQUIREMENTS, [plan_id=519]

(2) ShuffleQueryStage
Output [2]: [land#66, sum#86]
Arguments: 1

(3) AQEShuffleRead
Input [2]: [land#66, sum#86]
Arguments: coalesced

(4) HashAggregate [codegen id : 3]
Input [2]: [land#66, sum#86]

(5) ResultQueryStage
Output [2]: [land#66, summe#84]

(6) Exchange
Input [2]: [land#66, sum#86]

(7) HashAggregate
Input [2]: [land#66, sum#86]

(8) AdaptiveSparkPlan
Output [2]: [land#66, summe#84]
Arguments: isFinalPlan=true
)PLAN";

// Eigener Lauf, simple ohne AQE: ReusedSubquery nennt die Unterabfrage im Text.
constexpr std::string_view kWiederverwendetEinfach = R"PLAN(== Physical Plan ==
*(1) Filter (isnotnull(betrag#16) AND (betrag#16 > ReusedSubquery Subquery scalar-subquery#31, [id=#301]))
:  +- ReusedSubquery Subquery scalar-subquery#31, [id=#301]
+- *(1) ColumnarToRow
   +- FileScan parquet [kunde#14L,betrag#16] Batched: true, DataFilters: [isnotnull(betrag#16)], Format: Parquet, Location: InMemoryFileIndex(1 paths)[file:/daten/bestellungen], PartitionFilters: [], PushedFilters: [IsNotNull(betrag)], ReadSchema: struct<kunde:bigint,betrag:double>
         +- Subquery scalar-subquery#31, [id=#301]
            +- *(2) HashAggregate(keys=[], functions=[avg(betrag#37)])
               +- Exchange SinglePartition, ENSURE_REQUIREMENTS, [plan_id=297]
                  +- *(1) HashAggregate(keys=[], functions=[partial_avg(betrag#37)])
                     +- *(1) ColumnarToRow
                        +- FileScan parquet [betrag#37] Batched: true, DataFilters: [], Format: Parquet, Location: InMemoryFileIndex(1 paths)[file:/daten/bestellungen], PartitionFilters: [], PushedFilters: [], ReadSchema: struct<betrag:double>
)PLAN";

modell::Plan einPlan(std::string_view text)
{
    std::vector<quelle::Meldung> meldungen;
    std::vector<modell::Plan> p = sprache::liesPlaene(text, meldungen);
    REQUIRE(p.size() == 1);
    return std::move(p.front());
}

std::size_t knoten(const Plandiagramm& d, std::string_view titelBeginn)
{
    for (std::size_t i = 0; i < d.knoten.size(); ++i)
        if (d.knoten[i].titel.starts_with(titelBeginn)) return i;
    return projekt::kKeine;
}

bool hatZeile(const diagramm::PlanKnoten& k, std::string_view zeile)
{
    return std::find(k.zeilen.begin(), k.zeilen.end(), zeile) != k.zeilen.end();
}

std::size_t zahl(const Plandiagramm& d, PlanKantenArt art)
{
    return static_cast<std::size_t>(std::count_if(d.kanten.begin(), d.kanten.end(), [&](const diagramm::PlanKante& k) { return k.art == art; }));
}

bool hatRahmen(const Plandiagramm& d, std::string_view titel)
{
    return std::any_of(d.rahmen.begin(), d.rahmen.end(), [&](const diagramm::PlanRahmen& r) { return r.titel == titel; });
}

/// Kein Rahmen umfasst einen Knoten oder Rahmen, der nicht in ihm steht — nach unten und nach rechts.
void pruefeUeberdeckung(const Plandiagramm& d)
{
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
            {
                CAPTURE(d.knoten[k].titel);
                if (!vorfahr(g, d.knoten[k].gruppe) && !(d.knoten[k].gruppe != projekt::kKeine && vorfahr(d.knoten[k].gruppe, g)))
                    CHECK_FALSE(schneidet(a.gruppen[g], a.knoten[k]));
            }
            for (std::size_t h = g + 1; h < d.rahmen.size(); ++h)
            {
                CAPTURE(d.rahmen[h].titel);
                if (!vorfahr(g, h) && !vorfahr(h, g)) CHECK_FALSE(schneidet(a.gruppen[g], a.gruppen[h]));
            }
        }
    }
}

}  // namespace

TEST_SUITE("Spark-Plan")
{
    TEST_CASE("der Baum: Operatoren, Join mit Schluesseln und Broadcast-Seite, Scans, Stages")
    {
        const modell::Plan p = einPlan(kEinfach);
        CHECK(p.modus == "simple");
        CHECK(p.adaptiv);
        CHECK_FALSE(p.endgueltig);
        REQUIRE(p.baeume.size() == 1);
        const modell::Planbaum& b = p.baeume[0];
        REQUIRE(b.operatoren.size() == 8);
        CHECK(b.operatoren[0].name == "AdaptiveSparkPlan");
        CHECK(b.operatoren[2].name == "BroadcastHashJoin");
        CHECK(b.operatoren[2].kinder.size() == 2);
        CHECK(b.operatoren[2].argumente.starts_with("[custkey#386], [custkey#395]"));
        CHECK(b.operatoren[0].zeile == 2);

        const Plandiagramm d = diagramm::planVon(p);
        REQUIRE(d.knoten.size() == 8);
        const std::size_t join = knoten(d, "BroadcastHashJoin");
        REQUIRE(join != projekt::kKeine);
        CHECK(d.knoten[join].art == PlanKnotenArt::Join);
        CHECK(hatZeile(d.knoten[join], "Inner"));
        CHECK(hatZeile(d.knoten[join], "keys: custkey = custkey"));
        CHECK(hatZeile(d.knoten[join], "build: left"));
        // die Kante der Broadcast-Seite heisst build, Daten fliessen vom Kind zum Eltern
        const std::size_t broadcast = knoten(d, "BroadcastExchange");
        REQUIRE(broadcast != projekt::kKeine);
        CHECK(d.knoten[broadcast].art == PlanKnotenArt::Broadcast);
        CHECK(hatZeile(d.knoten[broadcast], "mode: hashed relation"));
        const auto build = std::find_if(d.kanten.begin(), d.kanten.end(),
                                        [&](const diagramm::PlanKante& k) { return k.von == broadcast && k.nach == join; });
        REQUIRE(build != d.kanten.end());
        CHECK(build->text == "build");
        const std::size_t scan = knoten(d, "FileScan parquet");
        REQUIRE(scan != projekt::kKeine);
        CHECK(d.knoten[scan].art == PlanKnotenArt::Scan);
        CHECK(hatZeile(d.knoten[scan], "columns: custkey,name"));
        CHECK(hatZeile(d.knoten[scan], "location: InMemoryFileIndex(1 paths)[file:/data/customer.pq]"));
        CHECK(hatZeile(d.knoten[scan], "pushed filters: IsNotNull(custkey)"));
        CHECK(hatZeile(d.knoten[knoten(d, "AdaptiveSparkPlan")], "final plan: no"));
        // der Broadcast beendet die Stage, die in ihn schreibt: zwei Stages
        CHECK(d.stages == 2);
        CHECK(d.knoten[broadcast].gruppe == d.knoten[scan].gruppe);
        CHECK(d.knoten[broadcast].gruppe != d.knoten[join].gruppe);
        CHECK(d.hinweise.empty());
    }

    TEST_CASE("formatted: Angaben je Nummer, Codegen-Stufen, Partitionierung der Exchanges")
    {
        const modell::Plan p = einPlan(kFormatted);
        CHECK(p.modus == "formatted");
        REQUIRE(p.baeume.size() == 1);
        const modell::Planbaum& b = p.baeume[0];
        REQUIRE(b.operatoren.size() == 8);
        CHECK(b.operatoren[0].nummer == 8);
        CHECK(b.operatoren[0].codegen == 3);
        CHECK(b.operatoren[7].name == "Scan");
        CHECK(b.operatoren[7].codegen == 0);
        CHECK(b.operatoren[5].codegen == 1);  // Filter (3)

        const Plandiagramm d = diagramm::planVon(p);
        const std::size_t e5 = knoten(d, "Exchange (5)");
        const std::size_t e7 = knoten(d, "Exchange (7)");
        REQUIRE(e5 != projekt::kKeine);
        REQUIRE(e7 != projekt::kKeine);
        CHECK(hatZeile(d.knoten[e5], "hash: key"));
        CHECK(hatZeile(d.knoten[e5], "partitions: 4"));
        CHECK(hatZeile(d.knoten[e5], "ENSURE_REQUIREMENTS"));
        CHECK(hatZeile(d.knoten[e7], "range: key ASC NULLS FIRST"));
        CHECK(hatZeile(d.knoten[knoten(d, "Filter (3)")], "condition: (isnotnull(key) AND (key > 0))"));
        CHECK(hatZeile(d.knoten[knoten(d, "HashAggregate (4)")], "keys: key"));
        CHECK(hatZeile(d.knoten[knoten(d, "HashAggregate (4)")], "functions: partial_max(val)"));
        CHECK(hatZeile(d.knoten[knoten(d, "Sort (8)")], "order: key ASC NULLS FIRST"));
        const std::size_t scan = knoten(d, "Scan parquet (1)");
        REQUIRE(scan != projekt::kKeine);
        CHECK(hatZeile(d.knoten[scan], "spark_catalog.default.explain_temp1"));
        CHECK(hatZeile(d.knoten[scan], "columns: key, val"));
        CHECK(d.stages == 3);
        CHECK(hatRahmen(d, "WholeStageCodegen (1)"));
        CHECK(hatRahmen(d, "WholeStageCodegen (2)"));
        CHECK(hatRahmen(d, "WholeStageCodegen (3)"));
        // der Scan laeuft in keiner Codegen-Stufe: er steht in seiner Stage
        CHECK(d.rahmen[d.knoten[scan].gruppe].art == diagramm::PlanRahmenArt::Stage);
        CHECK(d.rahmen[d.knoten[knoten(d, "Filter (3)")].gruppe].art == diagramm::PlanRahmenArt::Codegen);
        pruefeUeberdeckung(d);
    }

    TEST_CASE("extended: vier Abschnitte, das Schema unter dem analysierten uebergangen")
    {
        const modell::Plan p = einPlan(kExtended);
        CHECK(p.modus == "extended");
        REQUIRE(p.baeume.size() == 4);
        CHECK(p.baeume[0].art == "Parsed Logical Plan");
        CHECK(p.baeume[0].operatoren[0].name == "'Project");
        CHECK(p.baeume[1].operatoren[0].name == "Aggregate");
        CHECK(p.baeume[1].operatoren.size() == 3);
        CHECK(p.baeume[3].operatoren[0].codegen == -1);
        CHECK(diagramm::physischerBaum(p) == 3);
        CHECK(diagramm::kurzname(p.baeume[2]) == "optimized");

        const Plandiagramm phys = diagramm::planVon(p);
        CHECK(phys.titel.find("Physical Plan") != std::string::npos);
        CHECK(hatZeile(phys.knoten[knoten(phys, "Exchange")], "single partition"));
        CHECK(phys.stages == 3);
        CHECK(hatRahmen(phys, "WholeStageCodegen"));
        // ein logischer Plan: nur der Baum
        const Plandiagramm logisch = diagramm::planVon(p, 2);
        CHECK(logisch.titel.find("Optimized Logical Plan") != std::string::npos);
        CHECK(logisch.knoten.size() == 3);
        CHECK(logisch.rahmen.empty());
        CHECK(logisch.stages == 0);
    }

    TEST_CASE("AQE: der endgueltige Plan, der anfaengliche uebergangen")
    {
        const modell::Plan p = einPlan(kAqe);
        CHECK(p.adaptiv);
        CHECK(p.endgueltig);
        const modell::Planbaum& b = p.baeume[0];
        CHECK(b.operatoren.size() == 7);
        CHECK(b.operatoren[0].kinder.size() == 1);
        CHECK(std::count_if(b.operatoren.begin(), b.operatoren.end(), [](const modell::PlanOperator& o) { return o.name == "LocalTableScan"; }) == 1);
        const Plandiagramm d = diagramm::planVon(p);
        CHECK(d.titel.find("adaptive, final") != std::string::npos);
        CHECK(hatZeile(d.knoten[knoten(d, "Exchange")], "partitions: 200"));
        CHECK(hatZeile(d.knoten[knoten(d, "AdaptiveSparkPlan")], "final plan: yes"));
    }

    TEST_CASE("AQE nach dem Ausfuehren, formatted: Nummer vor `, Statistics(…)`/`, coalesced`, kein cost")
    {
        std::vector<quelle::Meldung> meldungen;
        const std::vector<modell::Plan> plaene = sprache::liesPlaene(kAqeFormattedEndgueltig, meldungen);
        REQUIRE(plaene.size() == 1);
        const modell::Plan& p = plaene[0];
        CHECK(p.modus == "formatted");
        CHECK(p.endgueltig);
        CHECK(meldungen.empty());  // vorher: (2), (3) not in the tree, und der Hinweis zu cost
        const modell::Planbaum& b = p.baeume[0];
        REQUIRE(b.operatoren.size() == 6);
        CHECK(b.operatoren[4].name == "ShuffleQueryStage");
        CHECK(b.operatoren[4].nummer == 2);
        CHECK(b.operatoren[4].argumente == "Statistics(sizeInBytes=192.0 B, rowCount=6)");
        CHECK(b.operatoren[3].nummer == 3);
        CHECK(b.operatoren[3].argumente == "coalesced");
        CHECK(b.operatoren[3].angaben.size() == 2);
    }

    TEST_CASE("simple: ReusedSubquery findet seine Unterabfrage ueber den Text; codegen gibt einen Hinweis")
    {
        const Plandiagramm d = diagramm::planVon(einPlan(kWiederverwendetEinfach));
        CHECK(d.hinweise.empty());  // vorher: the plan does not say what it reuses
        CHECK(zahl(d, PlanKantenArt::Wiederverwendung) == 1);

        constexpr std::string_view kCodegen = "Found 1 WholeStageCodegen subtrees.\n== Subtree 1 / 1 (maxMethodCodeSize:274) ==\n"
                                              "*(1) Filter isnotnull(ort#17L)\n+- *(1) ColumnarToRow\n\nGenerated code:\n/* 001 */ public Object generate(Object[] references) {\n";
        CHECK(sprache::plantext(kCodegen));
        const sprache::Ergebnis e = sprache::sparkPlan().lies(kCodegen, "x_codegen.txt", {});
        CHECK(e.einheit.plaene.empty());
        REQUIRE(e.meldungen.size() == 1);
        CHECK(e.meldungen[0].text.find("codegen") != std::string::npos);
    }

    TEST_CASE("Projekt: eine .out mit NUL am Anfang ist kein Text und wird nicht gelesen, ein Plan in .txt schon")
    {
        // FirmWare: die ELF-Dateien des TI-Compilers (`Master_CLA.out`) verdreifachten das Laden (Volltest C2D_Session14)
        const std::filesystem::path ordner = std::filesystem::temp_directory_path() / "c2d_test_plan_binaer";
        std::filesystem::remove_all(ordner);
        std::filesystem::create_directories(ordner);
        {
            std::ofstream elf(ordner / "Master_CLA.out", std::ios::binary);
            elf << std::string("\x7f" "ELF\x01\x01\x01\0\0\0", 10) << kEinfach;  // traegt sogar einen Plan-Kopf
            std::ofstream(ordner / "plan.txt") << kEinfach;
        }
        const projekt::Projekt p = projekt::liesOrdner(ordner);
        CHECK(p.finde("Master_CLA.out") == projekt::kKeine);
        REQUIRE(p.finde("plan.txt") != projekt::kKeine);
        CHECK(p.dateien[p.finde("plan.txt")].ergebnis.einheit.plaene.size() == 1);
        std::filesystem::remove_all(ordner);
    }

    TEST_CASE("der eingerueckte Baum: WholeStageCodegen als Rahmen, InputAdapter faellt weg, Wiederverwendung")
    {
        CHECK(sprache::plantext(kEingerueckt));
        CHECK_FALSE(sprache::plantext("Hello\n  world\n"));
        CHECK_FALSE(sprache::plantext("cmake_minimum_required(VERSION 3.20)\nproject(x)\n"));
        const projekt::Projekt pr = projekt::liesText(kEingerueckt, "simplified.txt");
        REQUIRE(pr.dateien.size() == 1);
        CHECK(pr.dateien[0].sprache == &sprache::sparkPlan());
        const std::vector<modell::Plan>& plaene = pr.dateien[0].ergebnis.einheit.plaene;
        REQUIRE(plaene.size() == 1);
        CHECK(plaene[0].name == "simplified.txt");
        CHECK(plaene[0].modus == "simplified");

        const Plandiagramm d = diagramm::planVon(plaene[0]);
        CHECK(knoten(d, "WholeStageCodegen") == projekt::kKeine);
        CHECK(knoten(d, "InputAdapter") == projekt::kKeine);
        CHECK(hatRahmen(d, "WholeStageCodegen (1)"));
        CHECK(hatRahmen(d, "WholeStageCodegen (2)"));
        CHECK(hatRahmen(d, "WholeStageCodegen (3)"));
        REQUIRE(zahl(d, PlanKantenArt::Wiederverwendung) == 1);
        const auto wieder = std::find_if(d.kanten.begin(), d.kanten.end(),
                                         [](const diagramm::PlanKante& k) { return k.art == PlanKantenArt::Wiederverwendung; });
        CHECK(d.knoten[wieder->von].titel == "BroadcastExchange");
        CHECK(d.knoten[wieder->nach].titel == "ReusedExchange");
        CHECK(hatZeile(d.knoten[wieder->nach], "reuses #2"));
        CHECK(hatZeile(d.knoten[knoten(d, "Exchange")], "keys: a"));
        CHECK(hatZeile(d.knoten[knoten(d, "HashAggregate")], "keys: a,count"));
        CHECK(d.stages == 3);
        // TakeOrderedAndProject steht in keiner Codegen-Stufe
        CHECK(d.rahmen[d.knoten[knoten(d, "TakeOrderedAndProject")].gruppe].art == diagramm::PlanRahmenArt::Stage);
        CHECK(d.hinweise.empty());
        pruefeUeberdeckung(d);
    }

    TEST_CASE("formatted mit Unterabfragen: Rahmen, Kanten in den Gastgeber, ohne eigenen Baum ueber den Ausdruck")
    {
        const modell::Plan p = einPlan(kUnterabfragen);
        const modell::Planbaum& b = p.baeume[0];
        REQUIRE(b.unterabfragen.size() == 2);
        CHECK(b.unterabfragen[0].gastgeber == 1);
        CHECK(b.unterabfragen[0].wurzel != modell::kKeinBlock);
        CHECK(b.unterabfragen[1].wurzel == modell::kKeinBlock);
        CHECK(b.operatoren.size() == 9);

        const Plandiagramm d = diagramm::planVon(p);
        CHECK(d.hinweise.empty());
        CHECK(hatRahmen(d, "Subquery 1"));
        REQUIRE(zahl(d, PlanKantenArt::Unterabfrage) == 2);
        const std::size_t quelle = knoten(d, "BroadcastExchange (9)");
        for (const diagramm::PlanKante& k : d.kanten)
            if (k.art == PlanKantenArt::Unterabfrage)
            {
                CHECK(k.von == quelle);
                CHECK(d.knoten[k.nach].art == PlanKnotenArt::Scan);
            }
        const std::size_t join = knoten(d, "BroadcastHashJoin (6)");
        CHECK(hatZeile(d.knoten[join], "keys: ws_customer_sk = cs_customer_sk"));
        CHECK(hatZeile(d.knoten[join], "build: right"));
        CHECK(std::none_of(d.knoten[join].zeilen.begin(), d.knoten[join].zeilen.end(),
                           [](const std::string& z) { return z.starts_with("condition"); }));
        // die Unterabfrage hat ihre eigene Stage in ihrem Rahmen
        const std::size_t filter = knoten(d, "Filter (8)");
        std::size_t r = d.knoten[filter].gruppe;
        while (d.rahmen[r].eltern != projekt::kKeine) r = d.rahmen[r].eltern;
        CHECK(d.rahmen[r].titel == "Subquery 1");
        pruefeUeberdeckung(d);
    }

    TEST_CASE("Notebook: ein Plan in der Ausgabe gehoert zum explain() seiner Zelle")
    {
        const std::string json =
            R"J({"cells":[{"cell_type":"code","source":["df = spark.read.parquet(\"x\")\n","df.explain()"],)J"
            R"J("outputs":[{"output_type":"stream","name":"stdout","text":["== Physical Plan ==\n","*(1) ColumnarToRow\n",)J"
            R"J("+- FileScan parquet [a#1] Batched: true\n"]}]},)J"
            R"J({"cell_type":"code","source":["print(1)"],"outputs":[{"output_type":"stream","name":"stdout",)J"
            R"J("text":["== Physical Plan ==\n","LocalTableScan [a#1]\n"]}]}],)J"
            R"J("metadata":{"language_info":{"name":"python"}},"nbformat":4,"nbformat_minor":5})J";
        const projekt::Projekt pr = projekt::liesText(json, "n.ipynb");
        REQUIRE(pr.dateien.size() == 1);
        const std::vector<modell::Plan>& plaene = pr.dateien[0].ergebnis.einheit.plaene;
        REQUIRE(plaene.size() == 2);
        CHECK(plaene[0].name == "<cell 1>@3");
        CHECK(plaene[0].aufruf == 3);
        CHECK(plaene[0].baeume[0].operatoren.size() == 2);
        // ohne explain(): nach der Zelle und der Zeile in der Ausgabe benannt, mit Hinweis
        CHECK(plaene[1].name == "<cell 2>@output:1");
        const auto& m = pr.dateien[0].ergebnis.meldungen;
        CHECK(std::any_of(m.begin(), m.end(), [](const quelle::Meldung& x) { return x.text.find("explain() in the cell") != std::string::npos; }));
        // im Datenfluss: der Knoten vor dem explain() traegt die Zeile `plan:` und fuehrt zum Plan (P2-2)
        const diagramm::Datenflussdiagramm fluss = projekt::datenflussIn(c2d::projekt::Funktionsverzeichnis(pr), 0, "");
        const auto mitPlan = std::find_if(fluss.knoten.begin(), fluss.knoten.end(),
                                          [](const diagramm::DatenflussKnoten& k) { return !k.plan.empty(); });
        REQUIRE(mitPlan != fluss.knoten.end());
        CHECK(mitPlan->plan == "<cell 1>@3");
        CHECK(std::find(mitPlan->zeilen.begin(), mitPlan->zeilen.end(), "plan: <cell 1>@3") != mitPlan->zeilen.end());
    }

    TEST_CASE("Notebook aus Databricks exportiert: der Plan steht als HTML in einem Block ansiout")
    {
        const std::string json =
            R"J({"cells":[{"cell_type":"code","source":["df.explain()"],"outputs":[{"output_type":"display_data",)J"
            R"J("data":{"text/html":["<style scoped>.ansiout { display: block; }</style>\n<div class=\"ansiout\">== Physical Plan ==\n",)J"
            R"J("*(1) Filter (a#1 &gt; 2)\n+- *(1) LocalTableScan [a#1]\n</div>"]}}]}],)J"
            R"J("metadata":{"language_info":{"name":"python"}},"nbformat":4,"nbformat_minor":5})J";
        const projekt::Projekt pr = projekt::liesText(json, "d.ipynb");
        REQUIRE(pr.dateien.size() == 1);
        const std::vector<modell::Plan>& plaene = pr.dateien[0].ergebnis.einheit.plaene;
        REQUIRE(plaene.size() == 1);
        REQUIRE(plaene[0].baeume[0].operatoren.size() == 2);
        CHECK(plaene[0].baeume[0].operatoren[0].argumente == "(a#1 > 2)");
    }

    TEST_CASE("eine Plan-Datei mit mehreren Plaenen: je einer mit Zeile; alle Formate ausser csv")
    {
        std::string text = "-- !query\nEXPLAIN SELECT 1\n-- !query output\n";
        text += kEinfach;
        text += "\n-- !query\n";
        text += kFormatted;
        const projekt::Projekt pr = projekt::liesText(text, "explain.sql.out");
        REQUIRE(pr.dateien.size() == 1);
        const std::vector<modell::Plan>& plaene = pr.dateien[0].ergebnis.einheit.plaene;
        REQUIRE(plaene.size() == 2);
        CHECK(plaene[0].name == "explain.sql.out@4");
        CHECK(plaene[1].modus == "formatted");

        const Plandiagramm d = diagramm::planVon(einPlan(kUnterabfragen));
        const ausgabe::Optionen o;
        CHECK(ausgabe::svg().kann(ausgabe::Diagrammart::Plan));
        CHECK_FALSE(ausgabe::csv().kann(ausgabe::Diagrammart::Plan));
        const std::string svg = ausgabe::svg().schreibe(d, o);
        CHECK(svg.find("BroadcastHashJoin (6)") != std::string::npos);
        CHECK(svg.find("stroke-dasharray:6 4") != std::string::npos);
        const std::string mermaid = ausgabe::mermaid().schreibe(d, o);
        CHECK(mermaid.find("flowchart TD") != std::string::npos);
        CHECK(mermaid.find("subgraph") != std::string::npos);
        CHECK(mermaid.find(" -.->") != std::string::npos);
        const std::string dot = ausgabe::dot().schreibe(d, o);
        CHECK(dot.find("shape=cylinder") != std::string::npos);
        CHECK(dot.find("style=dashed") != std::string::npos);
        const std::string drawio = ausgabe::drawio().schreibe(d, o);
        CHECK(drawio.find("swimlane") != std::string::npos);
        CHECK(drawio.find("hexagon") != std::string::npos);
    }
}
