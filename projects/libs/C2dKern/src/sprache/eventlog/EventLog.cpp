/**
 * @file EventLog.cpp
 * @brief Der Event-Log von Spark: je Zeile ein JSON-Ereignis (Spark Schicht 3, Entscheide L1, L4)
 *
 * Gelesen werden die Ereignisse, die einen Lauf beschreiben: `SparkListenerLogStart` (Version),
 * `ApplicationStart`/`End`, `EnvironmentUpdate` (`spark.master`, `spark.task.cpus`), `ExecutorAdded`/`Removed`,
 * `JobStart`/`End`, `StageSubmitted`/`Completed`, `TaskStart`/`End`, die SQL-Ausfuehrungen (`SQLExecutionStart`/`End`,
 * unter AQE `SQLAdaptiveExecutionUpdate`: je Ausfuehrung die letzte Fassung ihres Plans, gelesen wie ein Plan aus
 * `explain()`, L6) und `callSite.short` eines Jobs (L5). Andere (BlockManager, UnpersistRDD …)
 * zaehlen nicht als Fehler. Eine Zeile, die kein JSON ist, gibt eine Meldung mit ihrer Zeile; das Uebrige wird
 * gelesen. Was ein Lauf bedeutet (Bahnen, Farben), weiss erst die Ableitung (diagramm/Zeitachse.cpp).
 */
#include "c2d/sprache/Sprache.hpp"

#include "../../quelle/Json.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::sprache
{
namespace
{

namespace json = quelle::json;
using quelle::Meldung;
using quelle::Schwere;

constexpr std::string_view kEndungen[] = {"inprogress"};
constexpr std::string_view kAnfang = R"({"Event":"SparkListenerLogStart")";

constexpr lexer::Sprachtabelle kTabelle{
    .schluesselwoerter = {},
    .operatoren = {},
    .zeichenkettenBegrenzer = "",
    .zeichenBegrenzer = "",
};

const json::Wert* feld(const json::Wert* w, std::string_view name)
{
    if (w == nullptr || w->art != json::Art::Objekt) return nullptr;
    for (std::size_t i = 0; i < w->namen.size(); ++i)
        if (w->namen[i] == name) return &w->elemente[i];
    return nullptr;
}

std::string text(const json::Wert* w)
{
    if (w == nullptr) return {};
    if (w->art == json::Art::Text || w->art == json::Art::Zahl) return w->text;
    if (w->art == json::Art::Wahr) return "true";
    if (w->art == json::Art::Falsch) return "false";
    return {};
}

long long zahl(const json::Wert* w)
{
    if (w == nullptr || w->art != json::Art::Zahl) return 0;
    try
    {
        return std::stoll(w->text);
    }
    catch (...)
    {
        return 0;
    }
}

bool wahr(const json::Wert* w)
{
    return w != nullptr && w->art == json::Art::Wahr;
}

/// Die Stelle eines Ereignisses, nach der Art der Zeitstempel in Spark.
long long zeitpunkt(const json::Wert& e)
{
    for (const std::string_view n : {"Timestamp", "Submission Time", "Completion Time"})
        if (const long long t = zahl(feld(&e, n)); t > 0) return t;
    if (const json::Wert* i = feld(&e, "Task Info"))
        return std::max(zahl(feld(i, "Finish Time")), zahl(feld(i, "Launch Time")));
    if (const json::Wert* s = feld(&e, "Stage Info"))
        return std::max(zahl(feld(s, "Completion Time")), zahl(feld(s, "Submission Time")));
    return 0;
}

class LogLeser
{
public:
    LogLeser(std::string_view text, std::string_view name, std::vector<Meldung>& meldungen)
        : m_text(text), m_name(name), m_meldungen(meldungen)
    {
    }

    /// Die Plaene der SQL-Ausfuehrungen, nach lies().
    std::vector<modell::Plan> plaene() { return std::move(m_plaene); }

    modell::Lauf lies()
    {
        std::size_t a = 0;
        int nummer = 0;
        int ungueltig = 0;
        while (a < m_text.size())
        {
            std::size_t e = m_text.find('\n', a);
            if (e == std::string_view::npos) e = m_text.size();
            std::string_view zeile = m_text.substr(a, e - a);
            if (!zeile.empty() && zeile.back() == '\r') zeile.remove_suffix(1);
            ++nummer;
            if (!zeile.empty())
            {
                json::Wert w;
                Meldung fehler;
                if (!json::lies(zeile, w, fehler))
                {
                    // nur die ersten melden; eine abgeschnittene letzte Zeile ist ein Lauf, der noch schrieb
                    if (++ungueltig <= 5)
                        m_meldungen.push_back({Schwere::Warnung, {a, nummer, 1}, "event " + std::to_string(nummer) + ": " + fehler.text});
                }
                else
                    ereignis(w, nummer, a);
            }
            a = e + 1;
        }
        if (ungueltig > 5)
            m_meldungen.push_back({Schwere::Warnung, {}, std::to_string(ungueltig) + " lines are not valid JSON"});
        lesePlaene();
        if (m_lauf.ende == 0)
            m_meldungen.push_back({Schwere::Hinweis, {}, "no ApplicationEnd - the run did not finish or the log is cut"});
        std::stable_sort(m_lauf.tasks.begin(), m_lauf.tasks.end(),
                         [](const modell::LaufTask& x, const modell::LaufTask& y) { return x.start < y.start; });
        // eine Stage ohne `Submission Time` (Spark 2 schreibt sie erst beim Abschluss): ihr erster Task, sonst ihr Job
        for (modell::LaufStage& s : m_lauf.stages)
        {
            if (s.eingereicht > 0) continue;
            for (const modell::LaufTask& t : m_lauf.tasks)
                if (t.stage == s.id && t.stageVersuch == s.versuch && t.start > 0 && (s.eingereicht == 0 || t.start < s.eingereicht))
                    s.eingereicht = t.start;
            if (s.eingereicht == 0)
                for (const modell::LaufJob& j : m_lauf.jobs)
                    if (std::find(j.stages.begin(), j.stages.end(), s.id) != j.stages.end() && j.start > 0 && m_eingereicht.contains({s.id, s.versuch}))
                    {
                        s.eingereicht = j.start;
                        break;
                    }
            s.eingereichtGeschaetzt = s.eingereicht > 0;
        }
        return std::move(m_lauf);
    }

private:
    modell::LaufTask& task(const json::Wert& info, const json::Wert& e, int zeile)
    {
        const long long id = zahl(feld(&info, "Task ID"));
        const int versuch = static_cast<int>(zahl(feld(&info, "Attempt")));
        const auto [it, neu] = m_taskIndex.try_emplace({id, versuch}, m_lauf.tasks.size());
        if (neu)
        {
            modell::LaufTask t;
            t.id = id;
            t.versuch = versuch;
            t.stage = static_cast<int>(zahl(feld(&e, "Stage ID")));
            t.stageVersuch = static_cast<int>(zahl(feld(&e, "Stage Attempt ID")));
            t.zeile = zeile;
            m_lauf.tasks.push_back(std::move(t));
        }
        modell::LaufTask& t = m_lauf.tasks[it->second];
        t.executor = text(feld(&info, "Executor ID"));
        t.start = zahl(feld(&info, "Launch Time"));
        t.spekulativ = wahr(feld(&info, "Speculative"));
        return t;
    }

    modell::LaufStage& stage(const json::Wert& info, int zeile)
    {
        const int id = static_cast<int>(zahl(feld(&info, "Stage ID")));
        const int versuch = static_cast<int>(zahl(feld(&info, "Stage Attempt ID")));
        const auto [it, neu] = m_stageIndex.try_emplace({id, versuch}, m_lauf.stages.size());
        if (neu)
        {
            modell::LaufStage s;
            s.id = id;
            s.versuch = versuch;
            s.zeile = zeile;
            m_lauf.stages.push_back(std::move(s));
        }
        modell::LaufStage& s = m_lauf.stages[it->second];
        s.name = text(feld(&info, "Stage Name"));
        s.tasks = static_cast<int>(zahl(feld(&info, "Number of Tasks")));
        if (const long long t = zahl(feld(&info, "Submission Time")); t > 0) s.eingereicht = t;
        if (const long long t = zahl(feld(&info, "Completion Time")); t > 0) s.beendet = t;
        if (const std::string f = text(feld(&info, "Failure Reason")); !f.empty()) s.fehler = f;
        if (const json::Wert* p = feld(&info, "Parent IDs"); p != nullptr && p->art == json::Art::Liste)
        {
            s.eltern.clear();
            for (const json::Wert& x : p->elemente) s.eltern.push_back(static_cast<int>(zahl(&x)));
        }
        if (const json::Wert* r = feld(&info, "RDD Info"); r != nullptr && r->art == json::Art::Liste)
            for (const json::Wert& x : r->elemente)
                if (const std::string c = text(feld(&x, "Callsite"));
                    !c.empty() && std::find(s.aufrufe.begin(), s.aufrufe.end(), c) == s.aufrufe.end())
                    s.aufrufe.push_back(c);
        return s;
    }

    modell::LaufExecutor& executor(const std::string& id, int zeile)
    {
        for (modell::LaufExecutor& x : m_lauf.executoren)
            if (x.id == id) return x;
        modell::LaufExecutor x;
        x.id = id;
        x.zeile = zeile;
        m_lauf.executoren.push_back(std::move(x));
        return m_lauf.executoren.back();
    }

    modell::LaufAusfuehrung& ausfuehrung(int id, int zeile)
    {
        for (modell::LaufAusfuehrung& x : m_lauf.ausfuehrungen)
            if (x.id == id) return x;
        modell::LaufAusfuehrung x;
        x.id = id;
        x.zeile = zeile;
        m_lauf.ausfuehrungen.push_back(std::move(x));
        return m_lauf.ausfuehrungen.back();
    }

    /// Der Text des Plans einer Ausfuehrung, die spaetere Fassung ersetzt die fruehere.
    void planText(int id, const json::Wert& e, int zeile, std::size_t versatz)
    {
        const std::string t = text(feld(&e, "physicalPlanDescription"));
        if (t.empty()) return;
        PlanText& p = m_planTexte[id];
        p.text = t;
        p.zeile = zeile;
        p.versatz = versatz;
    }

    /// Je Ausfuehrung ihr Plan (L6), gelesen wie ein Plan aus `explain()`; Meldungen stehen an seinem Ereignis.
    void lesePlaene()
    {
        for (modell::LaufAusfuehrung& x : m_lauf.ausfuehrungen)
        {
            const auto it = m_planTexte.find(x.id);
            if (it == m_planTexte.end()) continue;
            std::vector<Meldung> meldungen;
            std::vector<modell::Plan> plaene = liesPlaene(it->second.text, meldungen);
            for (Meldung& m : meldungen)
            {
                m.text = "execution " + std::to_string(x.id) + ": " + m.text;
                m.ort = {it->second.versatz, it->second.zeile, 1};
                m_meldungen.push_back(std::move(m));
            }
            if (plaene.empty()) continue;
            modell::Plan p = std::move(plaene.front());
            p.name = std::string(m_name) + "@execution " + std::to_string(x.id);
            p.aufruf = it->second.zeile;
            x.plan = p.name;
            m_plaene.push_back(std::move(p));
        }
    }

    modell::LaufJob& job(int id, int zeile)
    {
        for (modell::LaufJob& j : m_lauf.jobs)
            if (j.id == id) return j;
        modell::LaufJob j;
        j.id = id;
        j.zeile = zeile;
        m_lauf.jobs.push_back(std::move(j));
        return m_lauf.jobs.back();
    }

    /// Ein Wert einer Metrik: in `Task Metrics` (Spark 3: mehr Felder), sonst in den Akkumulatoren des Tasks.
    static long long akkumuliert(const json::Wert* info, std::string_view name)
    {
        if (const json::Wert* liste = feld(info, "Accumulables"); liste != nullptr && liste->art == json::Art::Liste)
            for (const json::Wert& a : liste->elemente)
                if (text(feld(&a, "Name")) == name)
                {
                    const json::Wert* u = feld(&a, "Update");
                    if (u != nullptr && u->art == json::Art::Text)
                        try
                        {
                            return std::stoll(u->text);
                        }
                        catch (...)
                        {
                            return 0;
                        }
                    return zahl(u);
                }
        return 0;
    }

    void ereignis(const json::Wert& e, int zeile, std::size_t versatz)
    {
        const std::string art = text(feld(&e, "Event"));
        m_lauf.letzte = std::max(m_lauf.letzte, zeitpunkt(e));
        if (art == "SparkListenerLogStart") m_lauf.version = text(feld(&e, "Spark Version"));
        else if (art == "SparkListenerApplicationStart")
        {
            m_lauf.name = text(feld(&e, "App Name"));
            m_lauf.id = text(feld(&e, "App ID"));
            m_lauf.start = zahl(feld(&e, "Timestamp"));
        }
        else if (art == "SparkListenerApplicationEnd")
            m_lauf.ende = zahl(feld(&e, "Timestamp"));
        else if (art == "SparkListenerEnvironmentUpdate")
        {
            // Spark 3: ein Objekt; aeltere: eine Liste von Paaren
            const json::Wert* p = feld(&e, "Spark Properties");
            const auto setze = [&](const std::string& k, const std::string& v) {
                if (k == "spark.master") m_lauf.master = v;
                if (k == "spark.task.cpus")
                    try
                    {
                        m_lauf.taskCpus = std::max(1, std::stoi(v));
                    }
                    catch (...)
                    {
                    }
            };
            if (p != nullptr && p->art == json::Art::Objekt)
                for (std::size_t i = 0; i < p->namen.size(); ++i) setze(p->namen[i], text(&p->elemente[i]));
            else if (p != nullptr && p->art == json::Art::Liste)
                for (const json::Wert& x : p->elemente)
                    if (x.art == json::Art::Liste && x.elemente.size() == 2) setze(text(&x.elemente[0]), text(&x.elemente[1]));
        }
        else if (art == "SparkListenerExecutorAdded")
        {
            modell::LaufExecutor& x = executor(text(feld(&e, "Executor ID")), zeile);
            const json::Wert* info = feld(&e, "Executor Info");
            x.host = text(feld(info, "Host"));
            x.kerne = static_cast<int>(zahl(feld(info, "Total Cores")));
            x.hinzu = zahl(feld(&e, "Timestamp"));
        }
        else if (art == "SparkListenerExecutorRemoved")
        {
            modell::LaufExecutor& x = executor(text(feld(&e, "Executor ID")), zeile);
            x.entfernt = zahl(feld(&e, "Timestamp"));
            x.grund = text(feld(&e, "Removed Reason"));
        }
        else if (art == "SparkListenerJobStart")
        {
            modell::LaufJob& j = job(static_cast<int>(zahl(feld(&e, "Job ID"))), zeile);
            j.start = zahl(feld(&e, "Submission Time"));
            if (const json::Wert* s = feld(&e, "Stage IDs"); s != nullptr && s->art == json::Art::Liste)
                for (const json::Wert& x : s->elemente) j.stages.push_back(static_cast<int>(zahl(&x)));
            j.sql = text(feld(feld(&e, "Properties"), "spark.sql.execution.id"));
            j.aufruf = text(feld(feld(&e, "Properties"), "callSite.short"));
        }
        else if (art.ends_with("SparkListenerSQLExecutionStart"))
        {
            modell::LaufAusfuehrung& x = ausfuehrung(static_cast<int>(zahl(feld(&e, "executionId"))), zeile);
            x.beschreibung = text(feld(&e, "description"));
            if (const json::Wert* w = feld(&e, "rootExecutionId"); w != nullptr && w->art == json::Art::Zahl)
                x.wurzel = static_cast<int>(zahl(w));
            x.start = zahl(feld(&e, "time"));
            planText(x.id, e, zeile, versatz);
        }
        else if (art.ends_with("SparkListenerSQLAdaptiveExecutionUpdate"))
            // unter AQE: jede neue Fassung des Plans; die letzte gilt (L6)
            planText(static_cast<int>(zahl(feld(&e, "executionId"))), e, zeile, versatz);
        else if (art.ends_with("SparkListenerSQLExecutionEnd"))
        {
            modell::LaufAusfuehrung& x = ausfuehrung(static_cast<int>(zahl(feld(&e, "executionId"))), zeile);
            x.ende = zahl(feld(&e, "time"));
            if (const std::string f = text(feld(&e, "errorMessage")); !f.empty()) x.fehler = f.substr(0, f.find('\n'));
        }
        else if (art == "SparkListenerJobEnd")
        {
            modell::LaufJob& j = job(static_cast<int>(zahl(feld(&e, "Job ID"))), zeile);
            j.ende = zahl(feld(&e, "Completion Time"));
            j.ergebnis = text(feld(feld(&e, "Job Result"), "Result"));
        }
        else if (art == "SparkListenerStageSubmitted" || art == "SparkListenerStageCompleted")
        {
            if (const json::Wert* info = feld(&e, "Stage Info"))
            {
                const modell::LaufStage& s = stage(*info, zeile);
                m_eingereicht.insert({s.id, s.versuch});
            }
        }
        else if (art == "SparkListenerTaskStart")
        {
            if (const json::Wert* info = feld(&e, "Task Info")) task(*info, e, zeile);
        }
        else if (art == "SparkListenerTaskEnd")
        {
            const json::Wert* info = feld(&e, "Task Info");
            if (info == nullptr) return;
            modell::LaufTask& t = task(*info, e, zeile);
            t.zeile = zeile;
            t.ende = zahl(feld(info, "Finish Time"));
            const json::Wert* grund = feld(&e, "Task End Reason");
            t.ergebnis = text(feld(grund, "Reason"));
            if (t.ergebnis.empty()) t.ergebnis = wahr(feld(info, "Failed")) ? "Failed" : "Success";
            if (t.ergebnis != "Success")
                for (const std::string_view n : {"Description", "Kill Reason", "Loss Reason", "Message", "Class Name"})
                    if (const std::string g = text(feld(grund, n)); !g.empty())
                    {
                        t.grund = g.substr(0, g.find('\n'));
                        break;
                    }
            if (const json::Wert* m = feld(&e, "Task Metrics"); m != nullptr && m->art == json::Art::Objekt)
            {
                const json::Wert* lesen = feld(m, "Shuffle Read Metrics");
                t.laufzeit = zahl(feld(m, "Executor Run Time"));
                t.gc = zahl(feld(m, "JVM GC Time"));
                t.shuffleGelesen = zahl(feld(lesen, "Remote Bytes Read")) + zahl(feld(lesen, "Local Bytes Read"));
                t.shuffleGeschrieben = zahl(feld(feld(m, "Shuffle Write Metrics"), "Shuffle Bytes Written"));
                t.spillSpeicher = zahl(feld(m, "Memory Bytes Spilled"));
                t.spillPlatte = zahl(feld(m, "Disk Bytes Spilled"));
                t.spitze = zahl(feld(m, "Peak Execution Memory"));
                t.eingabe = zahl(feld(feld(m, "Input Metrics"), "Bytes Read"));
                t.ausgabe = zahl(feld(feld(m, "Output Metrics"), "Bytes Written"));
            }
            if (t.spitze == 0) t.spitze = akkumuliert(info, "internal.metrics.peakExecutionMemory");
        }
    }

    struct PlanText
    {
        std::string text;
        int zeile = 0;
        std::size_t versatz = 0;
    };

    std::string_view m_text;
    std::string_view m_name;
    std::vector<Meldung>& m_meldungen;
    modell::Lauf m_lauf;
    std::map<int, PlanText> m_planTexte;  ///< je Ausfuehrung die letzte Fassung
    std::vector<modell::Plan> m_plaene;
    std::map<std::pair<long long, int>, std::size_t> m_taskIndex;
    std::map<std::pair<int, int>, std::size_t> m_stageIndex;
    std::set<std::pair<int, int>> m_eingereicht;  ///< Stages mit StageSubmitted oder -Completed
};

class SparkEventLog final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "Spark event log"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungen; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return kTabelle; }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName, const LeseOptionen&) const override
    {
        Ergebnis ergebnis;
        ergebnis.einheit.name = std::string(einheitName);
        if (!eventLogText(text))
        {
            ergebnis.meldungen.push_back({Schwere::Hinweis, {}, "not a Spark event log (no SparkListenerLogStart in line 1)"});
            return ergebnis;
        }
        if (einheitName.ends_with(".inprogress"))
            ergebnis.meldungen.push_back({Schwere::Hinweis, {}, "run not finished (.inprogress)"});
        LogLeser leser(text, einheitName, ergebnis.meldungen);
        modell::Lauf lauf = leser.lies();
        if (lauf.name.empty()) lauf.name = std::string(einheitName);
        ergebnis.einheit.plaene = leser.plaene();
        ergebnis.einheit.laeufe.push_back(std::move(lauf));
        return ergebnis;
    }
};

}  // namespace

const Sprache& sparkEventLog()
{
    static const SparkEventLog sprache;
    return sprache;
}

bool eventLogText(std::string_view anfang)
{
    if (anfang.starts_with("\xEF\xBB\xBF")) anfang.remove_prefix(3);
    // ohne Leerraum zwischen den Zeichen, wie Spark schreibt; mit Leerraum (von Hand formatiert) ebenso
    std::string ohne;
    for (const char c : anfang.substr(0, 200))
    {
        if (c == '\n') break;
        if (c != ' ' && c != '\t') ohne.push_back(c);
    }
    return ohne.starts_with(kAnfang);
}

}  // namespace c2d::sprache
