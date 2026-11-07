/**
 * @file Datenfluss.cpp
 * @brief Von den Aufrufketten zum Datenfluss eines Spark-Skripts (Entscheide F1-F5)
 *
 * Erst ein feiner Graph (je Schritt ein Knoten), verfolgt ueber die Namen in Textfolge, Zweige zusammengefuehrt;
 * dann schmale Schritte zusammengefasst, Stages (zwischen Shuffles) und Jobs (je Aktion) bestimmt.
 */
#include "c2d/diagramm/Datenfluss.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <numeric>
#include <set>
#include <utility>

namespace c2d::diagramm
{

namespace
{

using modell::Anweisung;
using modell::AnweisungArt;
using modell::Aufrufkette;
using modell::Kettenglied;

// --- Die Tabelle von Spark (PySpark; DataFrame und RDD) ------------------------------------------

/// Schmale Schritte: jede Partition fuer sich, kein Shuffle.
constexpr std::string_view kSchmal[] = {
    "select", "selectExpr", "filter", "where", "withColumn", "withColumns", "withColumnRenamed",
    "withColumnsRenamed", "drop", "alias", "limit", "offset", "sample", "sampleBy", "fillna", "dropna", "replace",
    "toDF", "transform", "coalesce", "map", "flatMap", "mapValues", "flatMapValues", "mapPartitions",
    "mapPartitionsWithIndex", "keyBy", "keys", "values", "glom", "mapInPandas", "mapInArrow", "withWatermark", "hint",
    "toJSON", "randomSplit", "unpivot", "melt", "observe", "to", "withMetadata", "union", "unionAll", "unionByName",
    "zip", "zipWithIndex", "zipWithUniqueId", "sortWithinPartitions", "fill", "pipe", "localCheckpoint",
    "dropDuplicatesWithinWatermark", "explode", "createDataFrame", "sampleByKey"};
/// Breite Schritte: ein Shuffle, eine Stage-Grenze.
constexpr std::string_view kBreit[] = {
    "join", "crossJoin", "groupBy", "groupby", "rollup", "cube", "distinct", "dropDuplicates", "drop_duplicates",
    "orderBy", "sort", "repartition", "repartitionByRange", "intersect", "intersectAll", "subtract", "exceptAll",
    "reduceByKey", "groupByKey", "aggregateByKey", "combineByKey", "foldByKey", "sortByKey", "sortBy", "cogroup",
    "groupWith", "leftOuterJoin", "rightOuterJoin", "fullOuterJoin", "partitionBy", "cartesian", "subtractByKey",
    "repartitionAndSortWithinPartitions"};
/// Nach groupBy, rollup, cube: was dazugehoert (GroupedData).
constexpr std::string_view kGruppiert[] = {"agg", "count", "sum", "avg", "mean", "min", "max", "pivot", "apply",
                                           "applyInPandas", "applyInPandasWithState"};
/// Mit einem zweiten Eingang im ersten Argument.
constexpr std::string_view kZweiEingaenge[] = {
    "join", "crossJoin", "union", "unionAll", "unionByName", "intersect", "intersectAll", "subtract", "exceptAll",
    "cogroup", "groupWith", "leftOuterJoin", "rightOuterJoin", "fullOuterJoin", "cartesian", "subtractByKey", "zip"};
/// Aktionen: je eine ein Job.
constexpr std::string_view kAktion[] = {
    "collect", "count", "show", "take", "first", "head", "tail", "toPandas", "foreach", "foreachPartition", "reduce",
    "fold", "aggregate", "saveAsTextFile", "saveAsPickleFile", "saveAsSequenceFile", "saveAsHadoopFile",
    "saveAsNewAPIHadoopFile", "countByKey", "countByValue", "collectAsMap", "top", "takeOrdered", "takeSample",
    "isEmpty", "toLocalIterator", "describe", "summary", "toArrow"};
/// Nur auf einem RDD Aktionen (auf GroupedData gehoeren sie zum Shuffle).
constexpr std::string_view kAktionRdd[] = {"sum", "max", "min", "mean", "stdev", "variance", "stats", "histogram",
                                           "sampleStdev", "sampleVariance", "countApprox"};
constexpr std::string_view kZwischenspeicher[] = {"cache", "persist", "checkpoint"};
constexpr std::string_view kView[] = {"createOrReplaceTempView", "createTempView", "createGlobalTempView",
                                      "createOrReplaceGlobalTempView", "registerTempTable"};
/// Ein Wert, der kein DataFrame mehr ist (kein Knoten): Schema, Spalten, Plan.
constexpr std::string_view kOhneKnoten[] = {"printSchema", "explain", "schema", "columns", "dtypes", "stat",
                                            "getNumPartitions", "isStreaming", "inputFiles", "unpersist", "storageLevel",
                                            "rdd_id", "id", "name", "setName", "context"};
constexpr std::string_view kLeserEnde[] = {"load", "csv", "json", "parquet", "orc", "text", "table", "jdbc",
                                           "textFile", "xml", "avro", "delta"};
constexpr std::string_view kSchreiberEnde[] = {"save", "saveAsTable", "insertInto", "parquet", "csv", "json", "orc",
                                               "text", "jdbc", "start", "toTable", "foreach", "xml", "avro", "delta"};
constexpr std::string_view kKontextQuelle[] = {"parallelize", "textFile", "wholeTextFiles", "binaryFiles",
                                               "emptyRDD", "pickleFile", "sequenceFile", "hadoopFile",
                                               "newAPIHadoopFile", "range"};
/// Methoden, die es fast nur in Spark gibt — ein Parameter, auf dem sie laufen, gibt einen Hinweis (F2).
constexpr std::string_view kTypischSpark[] = {
    "withColumn", "withColumnRenamed", "groupBy", "selectExpr", "rdd", "reduceByKey", "mapPartitions", "toDF",
    "createOrReplaceTempView", "write", "repartition", "persist", "cache", "agg", "flatMap", "collect", "unionByName"};

template <std::size_t N>
bool in(const std::string_view (&tabelle)[N], std::string_view wort)
{
    return std::find(std::begin(tabelle), std::end(tabelle), wort) != std::end(tabelle);
}

std::string klein(std::string_view text)
{
    std::string s(text);
    for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return s;
}

std::string_view getrimmt(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\n' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

/// Ein Argument ohne `name=` vorn.
std::string_view ohneNamen(std::string_view arg)
{
    arg = getrimmt(arg);
    std::size_t i = 0;
    while (i < arg.size() && (std::isalnum(static_cast<unsigned char>(arg[i])) != 0 || arg[i] == '_')) ++i;
    if (i > 0 && i < arg.size() && arg[i] == '=' && (i + 1 >= arg.size() || arg[i + 1] != '=')) return getrimmt(arg.substr(i + 1));
    return arg;
}

/// Eine Zeichenkette ohne Anfuehrungszeichen (`"x"`, `'x'`); sonst der Text, wie er ist.
std::string ohneAnfuehrung(std::string_view s)
{
    s = getrimmt(s);
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) return std::string(s.substr(1, s.size() - 2));
    return std::string(s);
}

/// Ein Name oder ein Name mit Punkten (`df`, `self.df`, `args.output`)?
bool einfacherName(std::string_view s)
{
    s = getrimmt(s);
    if (s.empty() || std::isdigit(static_cast<unsigned char>(s.front())) != 0) return false;
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.'; });
}

/// Ein Pfad, der Jobs verbinden darf: eine Zeichenkette ohne Platzhalter oder ein Name (F4).
bool festerPfad(std::string_view s)
{
    s = getrimmt(s);
    if (s.empty()) return false;
    if ((s.front() == '"' || s.front() == '\'') && s.back() == s.front()) return s.find('{') == std::string_view::npos;
    return einfacherName(s);
}

std::string gliedText(const Kettenglied& g)
{
    if (!g.aufruf) return g.name;
    std::string aus = g.name + "(";
    for (std::size_t i = 0; i < g.argumente.size(); ++i) aus += (i > 0 ? ", " : "") + g.argumente[i];
    return aus + ")";
}

std::string erstesArgument(const Kettenglied& g)
{
    return g.argumente.empty() ? std::string() : std::string(ohneNamen(g.argumente[0]));
}

// --- Der feine Graph -------------------------------------------------------------------------------------

struct Roh
{
    DatenflussKnotenArt art = DatenflussKnotenArt::Schritte;
    std::string titel;
    std::vector<std::string> zeilen;
    int zeile = 0;
    bool cache = false;
    bool rdd = false;
    bool expliziteTeile = false;  ///< ein Shuffle, der seine Partitionen selbst nennt (`repartition(n)`)
    std::vector<std::string> ziele;
    std::string abfrage;
    std::string tabelle;
    std::string plan;
    std::size_t bereich = 0;
    int job = 0;
    bool weg = false;  ///< in einen anderen zusammengefasst
};

struct RohKante
{
    std::size_t von = 0;
    std::size_t nach = 0;
    DatenflussKantenArt art = DatenflussKantenArt::Fluss;
    std::string text;
};

struct Wert
{
    enum class Art
    {
        Keiner,
        Sitzung,
        Kontext,
        Baumeister,  ///< SparkSession.builder …
        Konfig,      ///< SparkConf() …
        Konf,        ///< spark.conf
        Udfs,        ///< spark.udf
        Leser,       ///< spark.read …
        Schreiber,   ///< df.write …
        Daten,       ///< ein DataFrame oder RDD
        Gruppiert,   ///< df.groupBy(…)
        Udf,         ///< udf(f): die Funktion f
    };
    Art art = Art::Keiner;
    std::vector<std::size_t> knoten;  ///< Daten: wer ihn liefert (mehrere aus Zweigen)
    bool rdd = false;
    bool strom = false;  ///< readStream / writeStream
    std::string format;
    std::vector<std::string> teile;  ///< Leser/Schreiber: die Glieder dazwischen
    std::vector<std::pair<std::string, std::string>> einstellungen;  ///< Baumeister, Konfig
    std::size_t gruppe = kKeine;  ///< Gruppiert: der Shuffle
    std::string udf;
    std::string vorsatz;  ///< `na.`, `stat.` vor dem naechsten Glied
};

/// Die Namen und was sie gerade sind; je Name auch, wie oft er zugewiesen wurde (fuer `same path`).
struct Zustand
{
    std::map<std::string, Wert> namen;
    std::map<std::string, int> fassung;
};

Zustand vereine(const Zustand& a, const Zustand& b)
{
    Zustand z = a;
    for (const auto& [n, w] : b.namen)
    {
        auto da = z.namen.find(n);
        if (da == z.namen.end())
        {
            z.namen[n] = w;
            continue;
        }
        if (da->second.art == Wert::Art::Daten && w.art == Wert::Art::Daten)
        {
            for (const std::size_t k : w.knoten)
                if (std::find(da->second.knoten.begin(), da->second.knoten.end(), k) == da->second.knoten.end())
                    da->second.knoten.push_back(k);
        }
        else if (da->second.art == Wert::Art::Keiner)
            da->second = w;
    }
    for (const auto& [n, f] : b.fassung) z.fassung[n] = std::max(z.fassung[n], f);
    return z;
}

class Ableitung
{
public:
    Ableitung(const DatenflussUmfeld& u, bool gemeinsam) : m_u(u), m_gemeinsam(gemeinsam) {}

    void laufe(const modell::Funktion& f, std::size_t bereich)
    {
        m_bereich = bereich;
        m_f = &f;
        if (!m_gemeinsam || bereich == 0) m_z = {};
        if (m_kopf.size() <= bereich) m_kopf.resize(bereich + 1);
        if (f.rumpf) gehe(*f.rumpf);
    }

    std::vector<Roh> k;
    std::vector<RohKante> e;
    std::vector<DatenflussHinweis> hinweise;
    std::vector<std::vector<std::pair<std::string, std::string>>> m_kopf;  ///< je Bereich die Einstellungen
    SparkWert rueckgabe = SparkWert::Keiner;
    int jobs = 0;

private:
    // --- Anweisungen ---

    void gehe(const Anweisung& a)
    {
        switch (a.art)
        {
            case AnweisungArt::Wenn:
            {
                werteKetten(a);
                const Zustand vor = m_z;
                if (!a.kinder.empty()) gehe(*a.kinder[0]);
                const Zustand dann = m_z;
                m_z = vor;
                if (a.sonst) gehe(*a.sonst);
                m_z = vereine(dann, m_z);
                break;
            }
            case AnweisungArt::Solange:
            case AnweisungArt::TueSolange:
            case AnweisungArt::Fuer:
            case AnweisungArt::FuerJedes:
            {
                werteKetten(a);
                const Zustand vor = m_z;
                ++m_schleife;
                for (const auto& kind : a.kinder) gehe(*kind);
                --m_schleife;
                m_z = vereine(vor, m_z);
                if (a.sonst) gehe(*a.sonst);
                break;
            }
            case AnweisungArt::Versuch:
            {
                // der Block; jede Behandlung als Alternative danach; finally auf jedem Weg
                if (!a.kinder.empty()) gehe(*a.kinder[0]);
                const Zustand nachBlock = m_z;
                Zustand alle = m_z;
                for (std::size_t i = 1; i < a.kinder.size(); ++i)
                {
                    if (a.kinder[i]->art == AnweisungArt::Schluss) continue;
                    m_z = nachBlock;
                    gehe(*a.kinder[i]);
                    alle = vereine(alle, m_z);
                }
                m_z = alle;
                if (a.sonst) gehe(*a.sonst);
                for (std::size_t i = 1; i < a.kinder.size(); ++i)
                    if (a.kinder[i]->art == AnweisungArt::Schluss) gehe(*a.kinder[i]);
                break;
            }
            default:
                werteKetten(a);
                for (const auto& kind : a.kinder) gehe(*kind);
                if (a.sonst) gehe(*a.sonst);
                break;
        }
        for (const std::string& n : a.schreibt) ++m_z.fassung[n];
    }

    void werteKetten(const Anweisung& a)
    {
        if (a.ketten.empty()) return;
        m_a = &a;
        std::set<std::size_t> verbraucht;
        for (std::size_t ci = 0; ci < a.ketten.size(); ++ci)
        {
            const Aufrufkette& c = a.ketten[ci];
            if (c.eltern != modell::kKeineKette) continue;
            Wert w = werte(ci, verbraucht);
            if (c.ganz && !c.ziel.empty()) weise(c.ziel, w);
            if (c.ganz && a.art == AnweisungArt::Ruecksprung)
            {
                if (w.art == Wert::Art::Sitzung) rueckgabe = SparkWert::Sitzung;
                else if (w.art == Wert::Art::Kontext && rueckgabe == SparkWert::Keiner) rueckgabe = SparkWert::Kontext;
            }
        }
        for (std::size_t ci = 0; ci < a.ketten.size(); ++ci)
            if (a.ketten[ci].eltern != modell::kKeineKette && !verbraucht.contains(ci))
            {
                verbraucht.insert(ci);
                (void)werte(ci, verbraucht);
            }
    }

    void weise(const std::string& ziel, const Wert& w)
    {
        std::vector<std::string> namen;
        std::size_t von = 0;
        for (std::size_t i = 0; i <= ziel.size(); ++i)
            if (i == ziel.size() || ziel[i] == ',')
            {
                // `(a, b) = …`, `[a, b] = …`: die Klammern gehoeren nicht zum Namen
                std::string_view n = getrimmt(std::string_view(ziel).substr(von, i - von));
                while (!n.empty() && (n.front() == '(' || n.front() == '[')) n = getrimmt(n.substr(1));
                while (!n.empty() && (n.back() == ')' || n.back() == ']')) n = getrimmt(n.substr(0, n.size() - 1));
                if (einfacherName(n)) namen.emplace_back(n);
                von = i + 1;
            }
        for (const std::string& n : namen)
        {
            if (w.art == Wert::Art::Udf)
            {
                m_udfs[n] = w.udf;
                m_z.namen.erase(n);
            }
            else if (w.art == Wert::Art::Keiner)
                m_z.namen.erase(n);
            else
                m_z.namen[n] = w;
        }
    }

    // --- Ketten ---

    Wert werte(std::size_t ci, std::set<std::size_t>& verbraucht)
    {
        verbraucht.insert(ci);
        const Aufrufkette& c = m_a->ketten[ci];
        if (c.glieder.empty()) return {};
        m_zeile = c.zeile;
        const Kettenglied& g0 = c.glieder[0];
        std::size_t ab = 1;
        Wert w;
        if (!g0.aufruf && c.glieder.size() > 1 && m_z.namen.contains(g0.name + "." + c.glieder[1].name))
        {
            w = m_z.namen[g0.name + "." + c.glieder[1].name];
            ab = 2;
        }
        else if (!g0.aufruf && m_z.namen.contains(g0.name))
            w = m_z.namen[g0.name];
        else if (g0.name == "SparkSession")
            w = sitzungsklasse(c, ab);
        else if (g0.name == "SparkContext")
        {
            if (g0.aufruf) w = kontext(g0);
            else if (c.glieder.size() > 1 && c.glieder[1].name == "getOrCreate")
            {
                w = kontext(c.glieder[1]);
                ab = 2;
            }
        }
        else if (g0.name == "SparkConf" && g0.aufruf)
            w.art = Wert::Art::Konfig;
        else if ((g0.name == "SQLContext" || g0.name == "HiveContext") && g0.aufruf)
            w.art = Wert::Art::Sitzung;
        else if ((g0.name == "udf" || g0.name == "pandas_udf") && g0.aufruf)
            w = udf(g0);
        else if (g0.aufruf && m_u.liefert && m_z.namen.count(g0.name) == 0 &&
                 ((c.ganz && !c.ziel.empty()) || c.glieder.size() > 1))
        {
            // `spark = create_spark_session()`, `make_spark().read…`: was liefert die Funktion des Projekts?
            w = geliefert(g0.name);
        }
        else if ((g0.name == "self" || g0.name == "cls") && !g0.aufruf && c.glieder.size() > 1 && c.glieder[1].aufruf &&
                 m_u.liefert && ((c.ganz && !c.ziel.empty()) || c.glieder.size() > 2))
        {
            // `spark = self._get_spark()`: eine Methode der Klasse
            w = geliefert(c.glieder[1].name);
            ab = 2;
        }
        if (w.art == Wert::Art::Keiner && m_u.vorgegeben && !g0.aufruf && !m_z.namen.contains(g0.name))
        {
            if (g0.name == "spark") w.art = Wert::Art::Sitzung;
            if (g0.name == "sc") w.art = Wert::Art::Kontext;
        }
        if (w.art == Wert::Art::Keiner)
        {
            // `F.udf(f)`: ein Glied udf/pandas_udf
            for (std::size_t i = 1; i < c.glieder.size(); ++i)
                if ((c.glieder[i].name == "udf" || c.glieder[i].name == "pandas_udf") && c.glieder[i].aufruf)
                    return udf(c.glieder[i]);
            meldeParameter(c);
            aufrufMitDaten(c, ci, verbraucht);
            return w;
        }
        for (std::size_t gi = ab; gi < c.glieder.size() && w.art != Wert::Art::Keiner; ++gi)
            w = schritt(std::move(w), ci, gi, verbraucht);
        return w;
    }

    /// Was eine Funktion des Projekts liefert; ihre Einstellungen kommen in den Kopf dieses Bereichs.
    Wert geliefert(const std::string& name)
    {
        Wert w;
        const SparkRueckgabe r = m_u.liefert(name);
        if (r.wert == SparkWert::Sitzung) w.art = Wert::Art::Sitzung;
        if (r.wert == SparkWert::Kontext) w.art = Wert::Art::Kontext;
        for (const auto& x : r.einstellungen)
            if (std::find(m_kopf[m_bereich].begin(), m_kopf[m_bereich].end(), x) == m_kopf[m_bereich].end())
                m_kopf[m_bereich].push_back(x);
        return w;
    }

    Wert udf(const Kettenglied& g)
    {
        Wert w;
        w.art = Wert::Art::Udf;
        const std::string f = erstesArgument(g);
        if (einfacherName(f)) w.udf = f;
        return w;
    }

    Wert sitzungsklasse(const Aufrufkette& c, std::size_t& ab)
    {
        Wert w;
        if (c.glieder[0].aufruf)
        {
            w.art = Wert::Art::Sitzung;  // SparkSession(sc)
            return w;
        }
        if (c.glieder.size() < 2) return w;
        const std::string& n = c.glieder[1].name;
        ab = 2;
        if (n == "builder") w.art = Wert::Art::Baumeister;
        else if (n == "getActiveSession" || n == "active") w.art = Wert::Art::Sitzung;
        return w;
    }

    Wert kontext(const Kettenglied& g)
    {
        Wert w;
        w.art = Wert::Art::Kontext;
        std::vector<std::pair<std::string, std::string>> e;
        std::size_t positional = 0;
        for (const std::string& a : g.argumente)
        {
            const std::string_view s = getrimmt(a);
            if (s.starts_with("conf"))
            {
                const std::string name(ohneNamen(s));
                if (const auto da = m_z.namen.find(name); da != m_z.namen.end()) e = da->second.einstellungen;
                continue;
            }
            if (s.starts_with("master=")) e.emplace_back("master", std::string(ohneNamen(s)));
            else if (s.starts_with("appName=")) e.emplace_back("appName", std::string(ohneNamen(s)));
            else if (s.find('=') == std::string_view::npos)
            {
                if (positional == 0) e.emplace_back("master", std::string(s));
                if (positional == 1) e.emplace_back("appName", std::string(s));
                ++positional;
            }
        }
        for (auto& x : e) m_kopf[m_bereich].push_back(std::move(x));
        return w;
    }

    /// Ein Glied gi auf dem Wert w.
    Wert schritt(Wert w, std::size_t ci, std::size_t gi, std::set<std::size_t>& verbraucht)
    {
        const Aufrufkette& c = m_a->ketten[ci];
        const Kettenglied& g = c.glieder[gi];
        const std::string n = w.vorsatz + g.name;
        w.vorsatz.clear();
        using A = Wert::Art;
        switch (w.art)
        {
            case A::Baumeister:
            case A::Konfig:
            {
                if (n == "getOrCreate" || n == "create")
                {
                    for (auto& x : w.einstellungen) m_kopf[m_bereich].push_back(x);
                    Wert s;
                    s.art = A::Sitzung;
                    return s;
                }
                const std::string v1 = erstesArgument(g);
                const std::string v2 = g.argumente.size() > 1 ? std::string(ohneNamen(g.argumente[1])) : std::string();
                if (n == "appName" || n == "setAppName") w.einstellungen.emplace_back("appName", v1);
                else if (n == "master" || n == "setMaster") w.einstellungen.emplace_back("master", v1);
                else if ((n == "config" || n == "set" || n == "setIfMissing") && g.argumente.size() >= 2)
                    w.einstellungen.emplace_back(ohneAnfuehrung(v1), v2);
                else if (n == "config" && !g.argumente.empty())
                {
                    // config(conf=c): die Einstellungen von c
                    if (const auto da = m_z.namen.find(erstesArgument(g)); da != m_z.namen.end())
                        for (const auto& x : da->second.einstellungen) w.einstellungen.push_back(x);
                }
                else if (n == "setAll" || n == "enableHiveSupport" || n == "remote")
                    w.einstellungen.emplace_back(n, v1);
                return w;
            }
            case A::Sitzung: return anSitzung(w, c, gi);
            case A::Kontext:
            {
                if (in(kKontextQuelle, n) && g.aufruf)
                {
                    const std::size_t q = neu(DatenflussKnotenArt::Quelle, n, {erstesArgument(g)}, true);
                    pfadLesen(q, erstesArgument(g));
                    return daten(q, true);
                }
                return {};
            }
            case A::Konf:
                if (n == "set" && g.argumente.size() >= 2)
                    m_kopf[m_bereich].emplace_back(ohneAnfuehrung(erstesArgument(g)), std::string(ohneNamen(g.argumente[1])));
                return {};
            case A::Udfs:
                if (n == "register" && g.argumente.size() >= 2)
                {
                    const std::string f(ohneNamen(g.argumente[1]));
                    if (einfacherName(f)) m_udfs[ohneAnfuehrung(erstesArgument(g))] = f;
                }
                return {};
            case A::Leser: return anLeser(std::move(w), g);
            case A::Schreiber: return anSchreiber(std::move(w), g);
            case A::Gruppiert:
                if (in(kGruppiert, n))
                {
                    k[w.gruppe].zeilen.push_back(gliedText(g));
                    zieleAus(g, w.gruppe);
                    if (n == "pivot") return w;
                    return daten(w.gruppe, false);
                }
                return {};
            case A::Daten: return anDaten(std::move(w), ci, gi, verbraucht);
            default: return {};
        }
    }

    Wert anSitzung(const Wert& w, const Aufrufkette& c, std::size_t gi)
    {
        (void)w;
        const Kettenglied& g = c.glieder[gi];
        Wert x;
        using A = Wert::Art;
        if (g.name == "read" || g.name == "readStream")
        {
            x.art = A::Leser;
            x.strom = g.name == "readStream";
            return x;
        }
        if (g.name == "sparkContext")
        {
            x.art = A::Kontext;
            return x;
        }
        if (g.name == "conf" && !g.aufruf)
        {
            x.art = A::Konf;
            return x;
        }
        if (g.name == "udf" && !g.aufruf)
        {
            x.art = A::Udfs;
            return x;
        }
        if (g.name == "table" && g.aufruf) return tabelleLesen(erstesArgument(g), "table", {});
        if (g.name == "sql" && g.aufruf) return sql(g);
        if (g.name == "range" && g.aufruf) return daten(neu(DatenflussKnotenArt::Quelle, gliedText(g), {}, false), false);
        if (g.name == "createDataFrame" && g.aufruf)
        {
            const std::string a0 = erstesArgument(g);
            if (const auto da = m_z.namen.find(a0); da != m_z.namen.end() && da->second.art == A::Daten)
            {
                const std::size_t s = neu(DatenflussKnotenArt::Schritte, {}, {gliedText(g)}, false);
                fluss(da->second.knoten, s);
                return daten(s, false);
            }
            return daten(neu(DatenflussKnotenArt::Quelle, "createDataFrame", {a0}, false), false);
        }
        return {};
    }

    Wert anLeser(Wert w, const Kettenglied& g)
    {
        if (!g.aufruf) return {};
        if (g.name == "format")
        {
            w.format = ohneAnfuehrung(erstesArgument(g));
            return w;
        }
        if (g.name == "option" || g.name == "options" || g.name == "schema")
        {
            w.teile.push_back(gliedText(g));
            return w;
        }
        if (!in(kLeserEnde, g.name)) return {};
        const std::string vorn = w.strom ? "readStream " : "read ";
        if (g.name == "table") return tabelleLesen(erstesArgument(g), vorn + "table", w.teile);
        const std::string format = g.name == "load" ? (w.format.empty() ? std::string("load") : w.format) : g.name;
        std::vector<std::string> zeilen;
        if (!g.argumente.empty()) zeilen.push_back(erstesArgument(g));
        zeilen.insert(zeilen.end(), w.teile.begin(), w.teile.end());
        const std::size_t q = neu(DatenflussKnotenArt::Quelle, vorn + format, zeilen, false);
        if (!g.argumente.empty()) pfadLesen(q, erstesArgument(g));
        return daten(q, false);
    }

    Wert tabelleLesen(const std::string& arg, const std::string& titel, const std::vector<std::string>& teile)
    {
        const std::string name = ohneAnfuehrung(arg);
        std::vector<std::string> zeilen{arg};
        zeilen.insert(zeilen.end(), teile.begin(), teile.end());
        const std::size_t q = neu(DatenflussKnotenArt::Quelle, titel, zeilen, false);
        k[q].tabelle = name;
        verbindeName(name, q);
        return daten(q, false);
    }

    /// Eine View oder Tabelle dieses Namens, die der Fluss angelegt hat, fuehrt nach `nach`.
    void verbindeName(const std::string& name, std::size_t nach)
    {
        if (const auto v = m_views.find(klein(name)); v != m_views.end())
            for (const std::size_t von : v->second) e.push_back({von, nach, DatenflussKantenArt::Pfad, "view " + name});
        else if (const auto t = m_tabellen.find(klein(name)); t != m_tabellen.end())
            for (const std::size_t von : t->second) e.push_back({von, nach, DatenflussKantenArt::Pfad, "same table"});
    }

    Wert sql(const Kettenglied& g)
    {
        const int bis = m_a->zeile + static_cast<int>(std::count(m_a->text.begin(), m_a->text.end(), '\n'));
        const modell::Abfrage* q = m_u.abfrageIn ? m_u.abfrageIn(m_zeile, bis) : nullptr;
        std::vector<std::string> zeilen;
        std::vector<std::string> quellen;
        if (q != nullptr)
            for (const modell::AbfrageBlock& b : q->bloecke)
                for (const modell::AbfrageQuelle& x : b.quellen)
                    if (!x.name.empty() && std::find(quellen.begin(), quellen.end(), x.name) == quellen.end())
                        quellen.push_back(x.name);
        for (const std::string& x : quellen) zeilen.push_back("reads " + x);
        if (q != nullptr && !q->ziel.empty()) zeilen.push_back(q->schreibt + " " + q->ziel);
        if (q == nullptr) zeilen.push_back(erstesArgument(g));
        const std::size_t s = neu(DatenflussKnotenArt::Sql, "spark.sql", zeilen, false);
        if (q != nullptr) k[s].abfrage = q->name;
        for (const std::string& x : quellen) verbindeName(x, s);
        if (q != nullptr && !q->ziel.empty()) m_tabellen[klein(q->ziel)].push_back(s);
        return daten(s, false);
    }

    Wert anSchreiber(Wert w, const Kettenglied& g)
    {
        if (!g.aufruf && g.name != "write") return {};
        if (g.name == "format")
        {
            w.format = ohneAnfuehrung(erstesArgument(g));
            w.teile.push_back(gliedText(g));
            return w;
        }
        if (!in(kSchreiberEnde, g.name))
        {
            w.teile.push_back(gliedText(g));
            return w;
        }
        std::vector<std::string> zeilen = w.teile;
        zeilen.push_back(gliedText(g));
        const std::size_t a = aktion(std::string(w.strom ? "writeStream" : "write"), zeilen);
        fluss(w.knoten, a);
        const std::string ziel = erstesArgument(g);
        if (g.name == "saveAsTable" || g.name == "insertInto" || g.name == "toTable")
            m_tabellen[klein(ohneAnfuehrung(ziel))].push_back(a);
        else if (!ziel.empty() && festerPfad(ziel))
            m_geschrieben.push_back({ziel, fassungVon(ziel), a});
        zieleAus(g, a);
        return {};
    }

    Wert anDaten(Wert w, std::size_t ci, std::size_t gi, std::set<std::size_t>& verbraucht)
    {
        const Aufrufkette& c = m_a->ketten[ci];
        const Kettenglied& g = c.glieder[gi];
        const std::string& n = g.name;
        using A = Wert::Art;
        if ((n == "write" || n == "writeStream") && !g.aufruf)
        {
            Wert s;
            s.art = A::Schreiber;
            s.knoten = w.knoten;
            s.strom = n == "writeStream";
            return s;
        }
        if ((n == "na" || n == "stat") && !g.aufruf)
        {
            w.vorsatz = n + ".";
            return w;
        }
        if (n == "rdd" && !g.aufruf)
        {
            const std::size_t s = schmal(w, "rdd", true);
            return daten(s, true);
        }
        if (!g.aufruf) return {};  // eine Spalte, ein Feld (`df.columns`, `df["x"]`)
        if (in(kView, n))
        {
            const std::string name = ohneAnfuehrung(erstesArgument(g));
            m_views[klein(name)] = w.knoten;
            for (const std::size_t p : w.knoten) k[p].zeilen.push_back("view " + name);
            return {};
        }
        if (in(kZwischenspeicher, n))
        {
            for (const std::size_t p : w.knoten)
            {
                k[p].cache = true;
                k[p].zeilen.push_back(gliedText(g));
            }
            return w;
        }
        if (n == "explain")
        {
            // der Plan aus der Ausgabe dieses explain() (P2-2): eine Zeile am Knoten, Doppelklick dorthin
            if (m_u.planAn)
                if (const std::string plan = m_u.planAn(m_zeile); !plan.empty())
                    for (const std::size_t p : w.knoten)
                    {
                        k[p].zeilen.push_back("plan: " + plan);
                        k[p].plan = plan;
                    }
            return {};
        }
        if (in(kOhneKnoten, n)) return n == "unpersist" ? w : Wert{};
        if (in(kAktion, n) || (w.rdd && in(kAktionRdd, n)))
        {
            const std::size_t a = aktion(gliedText(g), {});
            fluss(w.knoten, a);
            zieleAus(g, a);
            if (n == "saveAsTextFile" || n == "saveAsPickleFile")
                if (const std::string ziel = erstesArgument(g); festerPfad(ziel)) m_geschrieben.push_back({ziel, fassungVon(ziel), a});
            return {};
        }
        if (in(kBreit, n))
        {
            const std::size_t s = neu(DatenflussKnotenArt::Shuffle, gliedText(g), {}, w.rdd);
            fluss(w.knoten, s);
            if (in(kZweiEingaenge, n)) fluss(zweiterEingang(ci, gi, verbraucht), s);
            k[s].expliziteTeile = (n == "repartition" || n == "repartitionByRange" || n == "partitionBy") ||
                                  std::any_of(g.argumente.begin(), g.argumente.end(), [](const std::string& x) {
                                      const std::string_view v = getrimmt(x);
                                      return v.starts_with("numPartitions") ||
                                             (!v.empty() && std::all_of(v.begin(), v.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; }));
                                  });
            zieleAus(g, s);
            if ((n == "groupBy" || n == "groupby" || n == "rollup" || n == "cube") && !w.rdd)
            {
                Wert x;
                x.art = A::Gruppiert;
                x.gruppe = s;
                return x;
            }
            return daten(s, w.rdd);
        }
        if (!in(kSchmal, n))
            hinweise.push_back({m_zeile, "Spark method '" + n + "' is not known - drawn as a narrow step"});
        const std::size_t s = schmal(w, gliedText(g), w.rdd);
        if (in(kZweiEingaenge, n)) fluss(zweiterEingang(ci, gi, verbraucht), s);
        zieleAus(g, s);
        return daten(s, w.rdd);
    }

    /// Ein schmaler Schritt hinter w.
    std::size_t schmal(const Wert& w, std::string text, bool rdd)
    {
        const std::size_t s = neu(DatenflussKnotenArt::Schritte, {}, {std::move(text)}, rdd);
        fluss(w.knoten, s);
        return s;
    }

    /// Der zweite Eingang (`a.join(b, …)`, `a.union(b.select(…))`): eine Kette im ersten Argument oder ein Name.
    std::vector<std::size_t> zweiterEingang(std::size_t ci, std::size_t gi, std::set<std::size_t>& verbraucht)
    {
        for (std::size_t i = 0; i < m_a->ketten.size(); ++i)
        {
            const Aufrufkette& x = m_a->ketten[i];
            if (x.eltern == ci && x.glied == gi && x.argument == 0 && x.ganz && !verbraucht.contains(i))
            {
                const Anweisung* a = m_a;
                const int zeile = m_zeile;
                Wert w = werte(i, verbraucht);
                m_a = a;
                m_zeile = zeile;
                return w.art == Wert::Art::Daten ? w.knoten : std::vector<std::size_t>{};
            }
        }
        const Kettenglied& g = m_a->ketten[ci].glieder[gi];
        const std::string a0 = erstesArgument(g);
        if (const auto da = m_z.namen.find(a0); da != m_z.namen.end() && da->second.art == Wert::Art::Daten)
            return da->second.knoten;
        return {};
    }

    /// Kein Spark-Wert vorn, aber ein Aufruf einer Funktion des Projekts mit einem DataFrame (F2): ein Knoten
    /// „ruft f“; was er liefert, wird nicht verfolgt.
    void aufrufMitDaten(const Aufrufkette& c, std::size_t ci, std::set<std::size_t>& verbraucht)
    {
        (void)ci;
        (void)verbraucht;
        if (!m_u.istFunktion) return;
        for (const Kettenglied& g : c.glieder)
        {
            if (!g.aufruf) continue;
            // erst, ob ein DataFrame dabei ist (billig), dann, ob der Name eine Funktion des Projekts ist
            std::vector<std::size_t> von;
            for (const std::string& n : g.namen)
                if (const auto da = m_z.namen.find(n); da != m_z.namen.end() && da->second.art == Wert::Art::Daten)
                    von.insert(von.end(), da->second.knoten.begin(), da->second.knoten.end());
            if (von.empty() || !m_u.istFunktion(g.name)) continue;
            const std::size_t r = neu(DatenflussKnotenArt::Aufruf, gliedText(g), {}, false);
            k[r].ziele.push_back(g.name);
            fluss(von, r);
        }
    }

    /// Ein Parameter, auf dem Spark-Methoden laufen: Hinweis, nicht verfolgt (F2).
    void meldeParameter(const Aufrufkette& c)
    {
        if (m_f == nullptr || c.glieder.size() < 2 || c.glieder[0].aufruf) return;
        const std::string& n = c.glieder[0].name;
        if (!in(kTypischSpark, c.glieder[1].name)) return;
        const bool parameter = std::any_of(m_f->parameter.begin(), m_f->parameter.end(),
                                           [&](const modell::Parameter& p) { return p.name == n; });
        if (!parameter || !m_gemeldet.insert(m_f->vollerName() + "/" + n).second) return;
        hinweise.push_back({c.zeile, "parameter '" + n + "' is used like a DataFrame - not followed (the data flow starts at a Spark session)"});
    }

    /// Die Funktionen des Projekts, die ein Glied nennt (UDF, `map(f)`, `foreach(f)`).
    void zieleAus(const Kettenglied& g, std::size_t knoten)
    {
        for (const std::string& n : g.namen)
        {
            std::string ziel;
            if (const auto u = m_udfs.find(n); u != m_udfs.end()) ziel = u->second;
            else if (m_u.istFunktion && m_u.istFunktion(n)) ziel = n;
            if (!ziel.empty() && std::find(k[knoten].ziele.begin(), k[knoten].ziele.end(), ziel) == k[knoten].ziele.end())
                k[knoten].ziele.push_back(ziel);
        }
    }

    /// Ein Pfad, aus dem gelesen wird: ein frueheres Schreiben mit gleichem Text und gleicher Fassung verbindet.
    void pfadLesen(std::size_t knoten, const std::string& pfad)
    {
        if (!festerPfad(pfad)) return;
        for (const Geschrieben& s : m_geschrieben)
            if (s.pfad == pfad && s.fassung == fassungVon(pfad)) e.push_back({s.knoten, knoten, DatenflussKantenArt::Pfad, "same path"});
    }

    /// Wie oft der Name vorn im Pfad (`args` in `args.output`) bisher zugewiesen wurde; eine Zeichenkette: 0.
    int fassungVon(const std::string& pfad)
    {
        const std::string_view p = getrimmt(pfad);
        if (p.empty() || p.front() == '"' || p.front() == '\'') return 0;
        const std::string wurzel(p.substr(0, p.find('.')));
        const auto da = m_z.fassung.find(wurzel);
        return da == m_z.fassung.end() ? 0 : da->second;
    }

    std::size_t aktion(std::string text, std::vector<std::string> zeilen)
    {
        ++jobs;
        if (m_schleife > 0) zeilen.emplace_back("(in a loop)");
        const std::size_t a = neu(DatenflussKnotenArt::Aktion, "Job " + std::to_string(jobs) + ": " + text, std::move(zeilen), false);
        k[a].job = jobs;
        return a;
    }

    std::size_t neu(DatenflussKnotenArt art, std::string titel, std::vector<std::string> zeilen, bool rdd)
    {
        Roh r;
        r.art = art;
        r.titel = std::move(titel);
        r.zeilen = std::move(zeilen);
        r.zeile = m_zeile;
        r.rdd = rdd;
        r.bereich = m_bereich;
        k.push_back(std::move(r));
        return k.size() - 1;
    }

    void fluss(const std::vector<std::size_t>& von, std::size_t nach)
    {
        for (const std::size_t v : von)
            if (v != nach) e.push_back({v, nach, DatenflussKantenArt::Fluss, {}});
    }

    static Wert daten(std::size_t knoten, bool rdd)
    {
        Wert w;
        w.art = Wert::Art::Daten;
        w.knoten = {knoten};
        w.rdd = rdd;
        return w;
    }

    struct Geschrieben
    {
        std::string pfad;
        int fassung = 0;
        std::size_t knoten = 0;
    };

    const DatenflussUmfeld& m_u;
    bool m_gemeinsam = false;
    std::size_t m_bereich = 0;
    const modell::Funktion* m_f = nullptr;
    const Anweisung* m_a = nullptr;
    int m_zeile = 0;
    int m_schleife = 0;
    Zustand m_z;
    std::map<std::string, std::vector<std::size_t>> m_views, m_tabellen;
    std::map<std::string, std::string> m_udfs;
    std::vector<Geschrieben> m_geschrieben;
    std::set<std::string> m_gemeldet;
};

// --- Vom feinen Graph zum Diagramm --------------------------------------------------------------------

/// Schmale Schritte zusammen: A -> B, beide Schritte, A hat nur B, B nur A, A nicht zwischengespeichert.
void fasseZusammen(std::vector<Roh>& k, std::vector<RohKante>& e)
{
    for (bool weiter = true; weiter;)
    {
        weiter = false;
        for (std::size_t i = 0; i < e.size() && !weiter; ++i)
        {
            const RohKante x = e[i];
            if (x.art != DatenflussKantenArt::Fluss) continue;
            Roh& a = k[x.von];
            Roh& b = k[x.nach];
            if (a.weg || b.weg || a.art != DatenflussKnotenArt::Schritte || b.art != DatenflussKnotenArt::Schritte) continue;
            if (a.cache || a.bereich != b.bereich) continue;
            const auto aus = std::count_if(e.begin(), e.end(), [&](const RohKante& y) { return y.von == x.von; });
            const auto ein = std::count_if(e.begin(), e.end(), [&](const RohKante& y) { return y.nach == x.nach; });
            if (aus != 1 || ein != 1) continue;
            a.zeilen.insert(a.zeilen.end(), b.zeilen.begin(), b.zeilen.end());
            for (const std::string& z : b.ziele)
                if (std::find(a.ziele.begin(), a.ziele.end(), z) == a.ziele.end()) a.ziele.push_back(z);
            a.cache = b.cache;
            a.rdd = b.rdd;
            if (!b.plan.empty()) a.plan = b.plan;
            b.weg = true;
            e.erase(e.begin() + static_cast<std::ptrdiff_t>(i));
            for (RohKante& y : e)
                if (y.von == x.nach) y.von = x.von;
            weiter = true;
        }
    }
}

std::size_t finde(std::vector<std::size_t>& eltern, std::size_t i)
{
    while (eltern[i] != i) i = eltern[i] = eltern[eltern[i]];
    return i;
}

}  // namespace

std::string_view stereotyp(DatenflussKnotenArt art)
{
    // in Guillemets (U+00AB, U+00BB), als Bytes: die Quelle bleibt reines ASCII
    switch (art)
    {
        case DatenflussKnotenArt::Sitzung: return "\xC2\xAB" "session" "\xC2\xBB";
        case DatenflussKnotenArt::Quelle: return "\xC2\xAB" "source" "\xC2\xBB";
        case DatenflussKnotenArt::Shuffle: return "\xC2\xAB" "shuffle" "\xC2\xBB";
        case DatenflussKnotenArt::Aktion: return "\xC2\xAB" "action" "\xC2\xBB";
        case DatenflussKnotenArt::Sql: return "\xC2\xAB" "sql" "\xC2\xBB";
        case DatenflussKnotenArt::Aufruf: return "\xC2\xAB" "call" "\xC2\xBB";
        case DatenflussKnotenArt::Schritte: return {};
    }
    return {};
}

std::vector<std::string> texte(const DatenflussKnoten& knoten)
{
    std::vector<std::string> aus;
    if (const std::string_view s = stereotyp(knoten.art); !s.empty()) aus.emplace_back(s);
    if (!knoten.titel.empty()) aus.push_back(knoten.titel);
    aus.insert(aus.end(), knoten.zeilen.begin(), knoten.zeilen.end());
    return aus;
}

Datenflussdiagramm datenflussVon(std::span<const DatenflussBereich> bereiche, bool gemeinsam,
                                 const DatenflussUmfeld& umfeld, std::string titel, std::size_t fokus)
{
    Ableitung a(umfeld, gemeinsam);
    for (std::size_t b = 0; b < bereiche.size(); ++b)
        if (bereiche[b].funktion != nullptr) a.laufe(*bereiche[b].funktion, b);
    std::vector<Roh>& k = a.k;
    std::vector<RohKante>& e = a.e;
    fasseZusammen(k, e);

    // Jobs je Knoten: von jeder Aktion gegen die Kanten; ein Zwischenspeicher rechnet nur der erste Job
    std::vector<std::set<int>> jobsVon(k.size());
    std::set<std::size_t> berechnet;
    std::vector<std::size_t> aktionen;
    for (std::size_t i = 0; i < k.size(); ++i)
        if (!k[i].weg && k[i].art == DatenflussKnotenArt::Aktion) aktionen.push_back(i);
    std::sort(aktionen.begin(), aktionen.end(), [&](std::size_t x, std::size_t y) { return k[x].job < k[y].job; });
    for (const std::size_t akt : aktionen)
    {
        std::vector<std::size_t> offen{akt};
        std::set<std::size_t> gesehen;
        while (!offen.empty())
        {
            const std::size_t n = offen.back();
            offen.pop_back();
            if (!gesehen.insert(n).second) continue;
            jobsVon[n].insert(k[akt].job);
            if (k[n].cache && n != akt)
            {
                if (berechnet.contains(n)) continue;
                berechnet.insert(n);
            }
            for (const RohKante& x : e)
                if (x.nach == n && x.art == DatenflussKantenArt::Fluss) offen.push_back(x.von);
        }
    }
    // neu gerechnet: wo sich der Weg mehrerer Jobs teilt, ohne Zwischenspeicher
    for (std::size_t i = 0; i < k.size(); ++i)
    {
        if (k[i].weg || k[i].cache || jobsVon[i].size() < 2 || k[i].art == DatenflussKnotenArt::Aktion) continue;
        std::size_t nachfolger = 0;
        for (const RohKante& x : e)
            if (x.von == i && x.art == DatenflussKantenArt::Fluss && !jobsVon[x.nach].empty()) ++nachfolger;
        if (nachfolger < 2) continue;
        std::string text = "recomputed by ";
        bool erster = true;
        int n = 0;
        for (const int j : jobsVon[i])
        {
            if (erster)
            {
                erster = false;
                continue;
            }
            text += (n++ > 0 ? ", " : "") + std::string("Job ") + std::to_string(j);
        }
        k[i].zeilen.push_back(text);
        a.hinweise.push_back({k[i].zeile, text + " (no cache before the branch)"});
    }
    // Partitionen am Shuffle (F5): DataFrame spark.sql.shuffle.partitions, RDD spark.default.parallelism
    for (Roh& r : k)
    {
        if (r.weg || r.art != DatenflussKnotenArt::Shuffle || r.expliziteTeile) continue;
        const std::string schluessel = r.rdd ? "spark.default.parallelism" : "spark.sql.shuffle.partitions";
        std::string wert;
        for (std::size_t b = 0; b < a.m_kopf.size(); ++b)
            if (gemeinsam || b == r.bereich)
                for (const auto& [s, v] : a.m_kopf[b])
                    if (s == schluessel) wert = v;
        r.zeilen.push_back(wert.empty() ? std::string("partitions: default") : "partitions: " + wert + " (" + schluessel + ")");
    }

    // Stages: verbunden ueber Fluss-Kanten, die nicht in einen Shuffle fuehren. spark.sql beginnt eine Stage wie
    // ein Shuffle (was die Abfrage selbst verteilt, zeigt erst ihr Plan, Schicht 2); ein Aufruf steht draussen
    const auto inStage = [&](std::size_t i) { return !k[i].weg && k[i].art != DatenflussKnotenArt::Aufruf; };
    const auto beginntStage = [&](std::size_t i) {
        return k[i].art == DatenflussKnotenArt::Shuffle || k[i].art == DatenflussKnotenArt::Sql;
    };
    std::vector<std::size_t> eltern(k.size());
    std::iota(eltern.begin(), eltern.end(), std::size_t{0});
    for (const RohKante& x : e)
        if (x.art == DatenflussKantenArt::Fluss && inStage(x.von) && inStage(x.nach) && !beginntStage(x.nach))
            eltern[finde(eltern, x.nach)] = finde(eltern, x.von);

    // was gezeigt wird (fokus: der Bereich und was er direkt liest)
    std::vector<bool> gezeigt(k.size(), false);
    for (std::size_t i = 0; i < k.size(); ++i) gezeigt[i] = !k[i].weg && (fokus == kKeine || k[i].bereich == fokus);
    if (fokus != kKeine)
        for (const RohKante& x : e)
            if (!k[x.von].weg && k[x.nach].bereich == fokus && !k[x.nach].weg) gezeigt[x.von] = true;

    Datenflussdiagramm d;
    d.titel = std::move(titel);
    d.hinweise = std::move(a.hinweise);
    d.jobs = 0;
    // je Bereich ein Rahmen, wenn sie nicht gemeinsam sind und mehrere Knoten zeigen
    std::vector<std::size_t> bereichsRahmen(bereiche.size(), kKeine);
    std::set<std::size_t> mitKnoten;
    for (std::size_t i = 0; i < k.size(); ++i)
        if (gezeigt[i]) mitKnoten.insert(k[i].bereich);
    // ein Bereich nur mit seinem Kopf (eine Funktion, die die Sitzung baut und zurueckgibt) zeigt keinen Fluss:
    // er steht nur da, wenn er der einzige ist
    if (mitKnoten.empty())
        for (std::size_t b = 0; b < a.m_kopf.size(); ++b)
            if (!a.m_kopf[b].empty() && (fokus == kKeine || b == fokus) && b < bereiche.size()) mitKnoten.insert(b);
    if (!gemeinsam && mitKnoten.size() > 1)
        for (const std::size_t b : mitKnoten)
        {
            bereichsRahmen[b] = d.rahmen.size();
            d.rahmen.push_back({bereiche[b].funktion != nullptr ? bereiche[b].funktion->vollerName() : std::string(), kKeine});
        }
    std::map<std::size_t, std::size_t> neu;
    std::map<std::size_t, std::size_t> stageRahmen;
    const auto rahmenDerStage = [&](std::size_t i) {
        const std::size_t wurzel = finde(eltern, i);
        const auto [da, eingefuegt] = stageRahmen.try_emplace(wurzel, d.rahmen.size());
        if (eingefuegt)
        {
            ++d.stages;
            d.rahmen.push_back({"Stage " + std::to_string(d.stages), bereichsRahmen[k[i].bereich]});
        }
        return da->second;
    };
    // der Kopf Session je Bereich zuerst
    for (std::size_t b = 0; b < a.m_kopf.size(); ++b)
    {
        if (a.m_kopf[b].empty() || (fokus != kKeine && b != fokus) || (!gemeinsam && !mitKnoten.contains(b))) continue;
        if (gemeinsam && b > 0 && !d.knoten.empty() && d.knoten.front().art == DatenflussKnotenArt::Sitzung)
        {
            for (const auto& [s, v] : a.m_kopf[b]) d.knoten.front().zeilen.push_back(s + ": " + v);
            continue;
        }
        DatenflussKnoten s;
        s.art = DatenflussKnotenArt::Sitzung;
        s.titel = "Session";
        for (const auto& [n, v] : a.m_kopf[b]) s.zeilen.push_back(n + ": " + v);
        s.gruppe = b < bereichsRahmen.size() ? bereichsRahmen[b] : kKeine;
        d.knoten.push_back(std::move(s));
    }
    for (std::size_t i = 0; i < k.size(); ++i)
    {
        if (!gezeigt[i]) continue;
        const Roh& r = k[i];
        DatenflussKnoten n;
        n.art = r.art;
        n.titel = r.titel;
        n.zeilen = r.zeilen;
        n.zeile = r.zeile;
        n.zwischengespeichert = r.cache;
        n.ziele = r.ziele;
        n.abfrage = r.abfrage;
        n.tabelle = r.tabelle;
        n.plan = r.plan;
        n.gruppe = inStage(i) ? rahmenDerStage(i) : bereichsRahmen[r.bereich];
        if (r.art == DatenflussKnotenArt::Aktion) ++d.jobs;
        neu[i] = d.knoten.size();
        d.knoten.push_back(std::move(n));
    }
    for (const RohKante& x : e)
    {
        const auto v = neu.find(x.von);
        const auto z = neu.find(x.nach);
        if (v == neu.end() || z == neu.end()) continue;
        d.kanten.push_back({v->second, z->second, x.art, x.text});
    }
    return d;
}

std::vector<bool> bereicheMitFluss(std::span<const DatenflussBereich> bereiche, bool gemeinsam, const DatenflussUmfeld& umfeld)
{
    Ableitung a(umfeld, gemeinsam);
    for (std::size_t b = 0; b < bereiche.size(); ++b)
        if (bereiche[b].funktion != nullptr) a.laufe(*bereiche[b].funktion, b);
    std::vector<bool> aus(bereiche.size(), false);
    for (const Roh& r : a.k)
        if (r.bereich < aus.size()) aus[r.bereich] = true;
    return aus;
}

SparkRueckgabe rueckgabeVon(const modell::Funktion& funktion, const DatenflussUmfeld& umfeld)
{
    Ableitung a(umfeld, false);
    a.laufe(funktion, 0);
    SparkRueckgabe r;
    r.wert = a.rueckgabe;
    if (r.wert != SparkWert::Keiner && !a.m_kopf.empty()) r.einstellungen = a.m_kopf[0];
    return r;
}

}  // namespace c2d::diagramm
