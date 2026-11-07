/**
 * @file Zeitachse.cpp
 * @brief Ein Lauf von Spark auf einer Zeitachse: Jobs mit ihren Stages, Executoren mit ihren Plaetzen und Tasks
 */
#include "c2d/diagramm/Zeitachse.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace c2d::diagramm
{
namespace
{

using modell::Lauf;
using modell::LaufStage;
using modell::LaufTask;

std::string ms(long long dauer)
{
    char puffer[32];
    if (dauer < 1000) std::snprintf(puffer, sizeof puffer, "%lld ms", dauer);
    else if (dauer < 60000) std::snprintf(puffer, sizeof puffer, "%.1f s", static_cast<double>(dauer) / 1000);
    else std::snprintf(puffer, sizeof puffer, "%lld min %lld s", dauer / 60000, (dauer % 60000) / 1000);
    return puffer;
}

/// Wie viele Plaetze ein Executor hat (L2): `Total Cores` / `spark.task.cpus`; nennt der Log keine Kerne (der
/// Treiber im lokalen Modus), aus `local[n]`; sonst 0 (dann zaehlt, wie viele gleichzeitig liefen).
int plaetzeVon(const Lauf& lauf, const std::string& executor)
{
    for (const modell::LaufExecutor& x : lauf.executoren)
        if (x.id == executor && x.kerne > 0) return std::max(1, x.kerne / std::max(1, lauf.taskCpus));
    const std::string& m = lauf.master;
    if (m.starts_with("local[") && m.ends_with("]"))
        try
        {
            const int n = std::stoi(m.substr(6, m.size() - 7));
            if (n > 0) return std::max(1, n / std::max(1, lauf.taskCpus));
        }
        catch (...)
        {
        }
    if (m == "local") return 1;
    return 0;
}

/// Wie viele Tasks auf einem Executor hoechstens gleichzeitig liefen, streng nach Zeit.
int gleichzeitig(const std::vector<const LaufTask*>& tasks, long long letzte)
{
    std::vector<std::pair<long long, int>> punkte;
    for (const LaufTask* t : tasks)
    {
        punkte.emplace_back(t->start, 1);
        punkte.emplace_back(t->ende > 0 ? t->ende : letzte, -1);
    }
    std::sort(punkte.begin(), punkte.end());
    int jetzt = 0, hoechstens = 0;
    for (const auto& [zeit, schritt] : punkte) hoechstens = std::max(hoechstens, jetzt += schritt);
    return std::max(1, hoechstens);
}

}  // namespace

std::string bytes(long long anzahl)
{
    constexpr const char* kEinheiten[] = {"B", "KB", "MB", "GB", "TB"};
    double wert = static_cast<double>(anzahl);
    std::size_t e = 0;
    while (wert >= 1000 && e + 1 < std::size(kEinheiten))
    {
        wert /= 1000;
        ++e;
    }
    char puffer[32];
    if (e == 0) std::snprintf(puffer, sizeof puffer, "%lld B", anzahl);
    else std::snprintf(puffer, sizeof puffer, "%.1f %s", wert, kEinheiten[e]);
    return puffer;
}

Zeitachse zeitachseVon(const Lauf& lauf)
{
    Zeitachse z;
    z.titel = lauf.name + (lauf.id.empty() ? std::string() : " (" + lauf.id + ")") +
              (lauf.version.empty() ? std::string() : ": Spark " + lauf.version);
    long long anfang = lauf.start;
    long long ende = std::max(lauf.ende, lauf.letzte);
    for (const LaufTask& t : lauf.tasks)
        if (t.start > 0 && (anfang == 0 || t.start < anfang)) anfang = t.start;
    for (const modell::LaufJob& j : lauf.jobs)
        if (j.start > 0 && (anfang == 0 || j.start < anfang)) anfang = j.start;
    z.anfang = anfang;
    z.ende = std::max(ende, anfang);
    const long long letzte = z.ende;

    // die Farbe je Stage: ihre Stelle nach der Nummer (alle Versuche einer Stage gleich)
    std::set<int> stageIds;
    for (const LaufStage& s : lauf.stages) stageIds.insert(s.id);
    for (const LaufTask& t : lauf.tasks) stageIds.insert(t.stage);
    std::map<int, std::size_t> farbeVon;
    for (const int id : stageIds) farbeVon.emplace(id, farbeVon.size());
    std::map<std::pair<int, int>, const LaufStage*> stageVon;
    for (const LaufStage& s : lauf.stages) stageVon[{s.id, s.versuch}] = &s;

    // je Stage die Summen ihrer Tasks
    struct Summe
    {
        long long gelesen = 0, geschrieben = 0, spillSpeicher = 0, spillPlatte = 0, spitze = 0;
        std::size_t tasks = 0, fehlgeschlagen = 0, offen = 0;
    };
    std::map<std::pair<int, int>, Summe> summen;
    for (const LaufTask& t : lauf.tasks)
    {
        Summe& s = summen[{t.stage, t.stageVersuch}];
        ++s.tasks;
        s.gelesen += t.shuffleGelesen;
        s.geschrieben += t.shuffleGeschrieben;
        s.spillSpeicher += t.spillSpeicher;
        s.spillPlatte += t.spillPlatte;
        s.spitze = std::max(s.spitze, t.spitze);
        if (t.ende == 0) ++s.offen;
        else if (t.ergebnis != "Success") ++s.fehlgeschlagen;
    }

    // --- Jobs: je Job eine Zeile mit seinem Balken, darunter so viele, wie seine Stages brauchen -------------------
    std::set<std::pair<int, int>> gezeigt;  // Stages, die schon bei einem Job stehen
    std::vector<const modell::LaufJob*> jobs;
    for (const modell::LaufJob& j : lauf.jobs) jobs.push_back(&j);
    std::sort(jobs.begin(), jobs.end(), [](const modell::LaufJob* a, const modell::LaufJob* b) {
        return a->start != b->start ? a->start < b->start : a->id < b->id;
    });
    // je SQL-Ausfuehrung ihr Plan (L6)
    std::map<std::string, const modell::LaufAusfuehrung*> ausfuehrungVon;
    for (const modell::LaufAusfuehrung& x : lauf.ausfuehrungen) ausfuehrungVon[std::to_string(x.id)] = &x;
    const auto ausfuehrungDes = [&](const modell::LaufJob* j) -> const modell::LaufAusfuehrung* {
        if (j == nullptr || j->sql.empty()) return nullptr;
        const auto it = ausfuehrungVon.find(j->sql);
        return it == ausfuehrungVon.end() ? nullptr : it->second;
    };
    const auto mitAusfuehrung = [&](ZeitBalken& b, const modell::LaufJob* j) {
        if (j == nullptr) return;
        b.job = j->id;
        if (const modell::LaufAusfuehrung* x = ausfuehrungDes(j))
        {
            b.ausfuehrung = x->id;
            b.plan = x->plan;
            b.angaben.push_back("SQL execution " + std::to_string(x->id) + ": " + x->beschreibung);
            if (!x->fehler.empty()) b.angaben.push_back("execution failed: " + x->fehler);
            if (!x->plan.empty()) b.angaben.push_back("plan: " + x->plan + " (double-click)");
        }
        else if (!j->sql.empty())
            b.angaben.push_back("SQL execution: " + j->sql);
        if (!j->aufruf.empty()) b.angaben.push_back("call site: " + j->aufruf);
    };
    const auto stageBalken = [&](const LaufStage& s, std::size_t bahn, const modell::LaufJob* job) {
        ZeitBalken b;
        b.art = ZeitBalkenArt::Stage;
        b.stage = s.id;
        b.bahn = bahn;
        b.von = s.eingereicht;
        b.bis = s.beendet > 0 ? s.beendet : letzte;
        b.offen = s.beendet == 0;
        b.fehlgeschlagen = !s.fehler.empty();
        b.farbe = farbeVon[s.id];
        b.zeile = s.zeile;
        b.titel = "Stage " + std::to_string(s.id) + (s.versuch > 0 ? "." + std::to_string(s.versuch) : std::string()) +
                  ": " + s.name;
        const Summe& m = summen[{s.id, s.versuch}];
        b.angaben.push_back(b.titel);
        b.angaben.push_back("tasks: " + std::to_string(s.tasks) +
                            (m.tasks != static_cast<std::size_t>(s.tasks) ? " (" + std::to_string(m.tasks) + " in the log)" : ""));
        b.angaben.push_back("duration: " + ms(b.bis - b.von) + (b.offen ? " (not completed)" : ""));
        if (s.eingereichtGeschaetzt) b.angaben.push_back("submission time not in the log - from its first task");
        b.angaben.push_back("shuffle read: " + bytes(m.gelesen) + ", write: " + bytes(m.geschrieben));
        b.angaben.push_back("spill: memory " + bytes(m.spillSpeicher) + ", disk " + bytes(m.spillPlatte));
        b.angaben.push_back("peak execution memory: " + bytes(m.spitze));
        if (m.fehlgeschlagen > 0) b.angaben.push_back("failed tasks: " + std::to_string(m.fehlgeschlagen));
        if (m.offen > 0) b.angaben.push_back("tasks without an end: " + std::to_string(m.offen));
        if (!s.fehler.empty()) b.angaben.push_back("failure: " + s.fehler.substr(0, s.fehler.find('\n')));
        for (const std::string& a : s.aufrufe) b.angaben.push_back("rdd: " + a);
        mitAusfuehrung(b, job);
        return b;
    };
    // Stages in Zeilen legen: eine Stage auf die erste Zeile des Jobs, auf der sie keine andere ueberdeckt
    const auto lege = [&](std::vector<const LaufStage*> liste, std::size_t erste, std::size_t& zeilen, const modell::LaufJob* job) {
        std::sort(liste.begin(), liste.end(), [](const LaufStage* a, const LaufStage* b) { return a->eingereicht < b->eingereicht; });
        std::vector<long long> frei;  // je Zeile: frei ab
        for (const LaufStage* s : liste)
        {
            std::size_t r = 0;
            while (r < frei.size() && frei[r] > s->eingereicht) ++r;
            if (r == frei.size()) frei.push_back(0);
            frei[r] = s->beendet > 0 ? s->beendet : letzte;
            while (erste + r >= z.bahnen.size()) z.bahnen.push_back({ZeitBahnArt::Job, "", "", 0, 0, ""});
            z.balken.push_back(stageBalken(*s, erste + r, job));
        }
        zeilen = std::max<std::size_t>(1, frei.size());
    };
    for (const modell::LaufJob* j : jobs)
    {
        const std::size_t erste = z.bahnen.size();
        z.bahnen.push_back({ZeitBahnArt::Job, "Job " + std::to_string(j->id), "", 0, 0, ""});
        ZeitBalken b;
        b.art = ZeitBalkenArt::Job;
        b.bahn = erste;
        b.von = j->start;
        b.bis = j->ende > 0 ? j->ende : letzte;
        b.offen = j->ende == 0;
        b.fehlgeschlagen = !j->ergebnis.empty() && j->ergebnis != "JobSucceeded";
        b.zeile = j->zeile;
        b.titel = "Job " + std::to_string(j->id);
        b.angaben.push_back(b.titel);
        b.angaben.push_back("duration: " + ms(b.bis - b.von) + (b.offen ? " (not completed)" : ""));
        b.angaben.push_back("result: " + (j->ergebnis.empty() ? std::string("none in the log") : j->ergebnis));
        std::string ids;
        for (const int s : j->stages) ids += (ids.empty() ? "" : ", ") + std::to_string(s);
        b.angaben.push_back("stages: " + ids);
        mitAusfuehrung(b, j);
        z.balken.push_back(std::move(b));
        // seine Stages, die liefen (eingereicht) — eine uebersprungene steht nur in der Liste
        std::vector<const LaufStage*> liste;
        for (const LaufStage& s : lauf.stages)
            if (std::find(j->stages.begin(), j->stages.end(), s.id) != j->stages.end() && s.eingereicht > 0 &&
                gezeigt.insert({s.id, s.versuch}).second)
                liste.push_back(&s);
        std::size_t zeilen = 0;
        lege(liste, erste, zeilen, j);
        if (j->ende == 0) z.hinweise.push_back("Job " + std::to_string(j->id) + " has no end in the log");
    }
    {
        std::vector<const LaufStage*> ohneJob;
        for (const LaufStage& s : lauf.stages)
            if (s.eingereicht > 0 && !gezeigt.contains({s.id, s.versuch})) ohneJob.push_back(&s);
        if (!ohneJob.empty())
        {
            const std::size_t erste = z.bahnen.size();
            z.bahnen.push_back({ZeitBahnArt::Job, "no job", "", 0, 0, ""});
            std::size_t zeilen = 0;
            lege(ohneJob, erste, zeilen, nullptr);
        }
    }
    for (const LaufStage& s : lauf.stages)
        if (s.eingereicht > 0 && s.beendet == 0)
            z.hinweise.push_back("Stage " + std::to_string(s.id) + " (" + s.name + ") was not completed");

    // --- Executoren: je Platz eine Bahn (L2) --------------------------------------------------------------------
    std::vector<std::string> folge;
    for (const modell::LaufExecutor& x : lauf.executoren) folge.push_back(x.id);
    for (const LaufTask& t : lauf.tasks)
        if (std::find(folge.begin(), folge.end(), t.executor) == folge.end()) folge.push_back(t.executor);
    // der Treiber zuerst, die uebrigen nach ihrer Nummer, wo es Zahlen sind
    std::stable_sort(folge.begin(), folge.end(), [](const std::string& a, const std::string& b) {
        const auto rang = [](const std::string& x) -> long long {
            if (x == "driver") return -1;
            try
            {
                return std::stoll(x);
            }
            catch (...)
            {
                return 1LL << 40;
            }
        };
        return rang(a) < rang(b);
    });
    const bool zusammen = lauf.tasks.size() > kHoechstensTasks;
    z.zusammengefasst = zusammen;
    for (const std::string& id : folge)
    {
        std::vector<const LaufTask*> eigene;
        for (const LaufTask& t : lauf.tasks)
            if (t.executor == id) eigene.push_back(&t);
        int plaetze = plaetzeVon(lauf, id);
        if (plaetze == 0)
        {
            plaetze = gleichzeitig(eigene, letzte);
            z.hinweise.push_back("executor " + id + ": the log names no cores - " + std::to_string(plaetze) +
                                 " lane(s), as many tasks as ran at once");
        }
        const modell::LaufExecutor* x = nullptr;
        for (const modell::LaufExecutor& e : lauf.executoren)
            if (e.id == id) x = &e;
        const std::size_t erste = z.bahnen.size();
        for (int p = 0; p < plaetze; ++p)
        {
            ZeitBahn bahn;
            bahn.art = ZeitBahnArt::Executor;
            bahn.executor = id;
            bahn.platz = p;
            if (p == 0)
                bahn.titel = (id == "driver" ? std::string("driver") : "executor " + id) +
                             (x != nullptr && !x->host.empty() ? " (" + x->host + ")" : std::string());
            else
                bahn.titel = "  core " + std::to_string(p + 1);
            if (x != nullptr)
            {
                bahn.entfernt = x->entfernt;
                bahn.grund = x->grund;
            }
            z.bahnen.push_back(std::move(bahn));
        }
        // ein Task auf den Platz, der zuerst frei wird (L2)
        std::vector<long long> frei(static_cast<std::size_t>(plaetze), 0);
        long long zuViel = 0;  // wie lange mehr Tasks liefen als Plaetze (laengste Ueberlappung)
        std::map<std::pair<std::size_t, int>, ZeitBalken> gesammelt;  // (Platz, Stage) -> Balken, ueber der Grenze
        for (const LaufTask* t : eigene)
        {
            const auto p = static_cast<std::size_t>(std::min_element(frei.begin(), frei.end()) - frei.begin());
            if (frei[p] > t->start) zuViel = std::max(zuViel, frei[p] - t->start);
            const long long bis = t->ende > 0 ? t->ende : letzte;
            frei[p] = std::max(frei[p], bis);
            const std::size_t bahn = erste + p;
            if (zusammen)
            {
                auto [it, neu] = gesammelt.try_emplace({p, t->stage});
                ZeitBalken& b = it->second;
                if (neu)
                {
                    b.art = ZeitBalkenArt::Zusammen;
                    b.bahn = bahn;
                    b.von = t->start;
                    b.bis = bis;
                    b.farbe = farbeVon[t->stage];
                    b.zeile = t->zeile;
                    b.angaben.push_back("");
                }
                b.von = std::min(b.von, t->start);
                b.bis = std::max(b.bis, bis);
                b.offen = b.offen || t->ende == 0;
                b.fehlgeschlagen = b.fehlgeschlagen || (t->ende > 0 && t->ergebnis != "Success");
                b.angaben.push_back("task " + std::to_string(t->id));
                continue;
            }
            ZeitBalken b;
            b.art = ZeitBalkenArt::Task;
            b.bahn = bahn;
            b.von = t->start;
            b.bis = bis;
            b.offen = t->ende == 0;
            b.fehlgeschlagen = t->ende > 0 && t->ergebnis != "Success";
            b.farbe = farbeVon[t->stage];
            b.zeile = t->zeile;
            b.titel = std::to_string(t->id);
            b.stage = t->stage;
            b.angaben.push_back("task " + std::to_string(t->id) + (t->versuch > 0 ? " attempt " + std::to_string(t->versuch) : "") +
                                ", stage " + std::to_string(t->stage) + (t->spekulativ ? " (speculative)" : ""));
            b.angaben.push_back("executor " + t->executor + ", duration " + ms(bis - t->start) +
                                (b.offen ? " (no end in the log)" : ""));
            b.angaben.push_back("run time " + ms(t->laufzeit) + ", GC " + ms(t->gc));
            if (t->eingabe > 0 || t->ausgabe > 0) b.angaben.push_back("input " + bytes(t->eingabe) + ", output " + bytes(t->ausgabe));
            b.angaben.push_back("shuffle read " + bytes(t->shuffleGelesen) + ", write " + bytes(t->shuffleGeschrieben));
            if (t->spillSpeicher > 0 || t->spillPlatte > 0)
                b.angaben.push_back("spill: memory " + bytes(t->spillSpeicher) + ", disk " + bytes(t->spillPlatte));
            b.angaben.push_back("peak execution memory " + bytes(t->spitze));
            if (b.fehlgeschlagen) b.angaben.push_back("failed: " + t->ergebnis + (t->grund.empty() ? "" : " - " + t->grund));
            const auto zahl = [](long long v) { return std::to_string(v); };
            b.spalten = {{"task", zahl(t->id)},
                         {"attempt", std::to_string(t->versuch)},
                         {"stage", std::to_string(t->stage) + (t->stageVersuch > 0 ? "." + std::to_string(t->stageVersuch) : "")},
                         {"executor", t->executor},
                         {"core", std::to_string(p + 1)},
                         {"start ms", zahl(t->start - z.anfang)},
                         {"end ms", t->ende > 0 ? zahl(t->ende - z.anfang) : std::string()},
                         {"duration ms", t->ende > 0 ? zahl(t->ende - t->start) : std::string()},
                         {"run time ms", zahl(t->laufzeit)},
                         {"gc ms", zahl(t->gc)},
                         {"input bytes", zahl(t->eingabe)},
                         {"output bytes", zahl(t->ausgabe)},
                         {"shuffle read bytes", zahl(t->shuffleGelesen)},
                         {"shuffle write bytes", zahl(t->shuffleGeschrieben)},
                         {"spill memory bytes", zahl(t->spillSpeicher)},
                         {"spill disk bytes", zahl(t->spillPlatte)},
                         {"peak execution memory bytes", zahl(t->spitze)},
                         {"result", t->ende > 0 ? t->ergebnis : std::string("no end")},
                         {"reason", t->grund}};
            z.balken.push_back(std::move(b));
        }
        for (auto& [schluessel, b] : gesammelt)
        {
            const std::size_t n = b.angaben.size() - 1;
            b.angaben.resize(1);
            b.titel = std::to_string(n) + " tasks";
            b.stage = schluessel.second;
            b.angaben[0] = std::to_string(n) + " tasks of stage " + std::to_string(schluessel.second) + " on this core, " +
                           ms(b.bis - b.von);
            z.balken.push_back(std::move(b));
        }
        if (zuViel > kUeberlappungMs)
            z.hinweise.push_back("executor " + id + ": more tasks than its " + std::to_string(plaetze) +
                                 " core(s) ran at once for up to " + ms(zuViel));
        if (x != nullptr && x->entfernt > 0)
            z.hinweise.push_back("executor " + id + " was removed" + (x->grund.empty() ? std::string() : ": " + x->grund));
    }
    if (zusammen)
        z.hinweise.push_back(std::to_string(lauf.tasks.size()) + " tasks - more than " + std::to_string(kHoechstensTasks) +
                             ": one bar per core and stage");
    {
        std::size_t offen = 0;
        for (const LaufTask& t : lauf.tasks) offen += t.ende == 0 ? 1 : 0;
        if (offen > 0) z.hinweise.push_back(std::to_string(offen) + " task(s) without an end in the log - drawn up to its last event");
    }
    z.jobs = lauf.jobs.size();
    z.stages = lauf.stages.size();
    z.tasks = lauf.tasks.size();
    return z;
}

}  // namespace c2d::diagramm
