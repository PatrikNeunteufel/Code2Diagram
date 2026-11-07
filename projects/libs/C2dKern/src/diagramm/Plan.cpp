/**
 * @file Plan.cpp
 * @brief Der Plan von Spark als Diagramm: was die Operatoren bedeuten (Spark Schicht 2, P2-2)
 *
 * Die Tabellen hier sind das Wissen ueber Spark: welche Operatoren lesen, mischen, verbinden, zusammenfassen,
 * und wo ihre Angaben stehen — im Baum hinter dem Namen (`HashAggregate(keys=[k], functions=[…])`), in den
 * Angaben von formatted (`Keys [1]: [k]`) oder im eingerueckten Baum (`HashAggregate [k] [count]`).
 */
#include "c2d/diagramm/Plan.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::diagramm
{
namespace
{

using modell::Plan;
using modell::Planbaum;
using modell::PlanOperator;

constexpr std::string_view kScans[] = {"Scan", "FileScan", "BatchScan", "LocalTableScan", "InMemoryTableScan",
                                       "RowDataSourceScan", "DataSourceV2Scan", "HiveTableScan", "RDDScanExec",
                                       "ExistingRDD", "OneRowRelation", "Range"};
constexpr std::string_view kExchanges[] = {"Exchange", "ShuffleExchange"};
constexpr std::string_view kAggregate[] = {"HashAggregate", "SortAggregate", "ObjectHashAggregate"};
constexpr std::string_view kGrenzen[] = {"CollectLimit", "GlobalLimit", "LocalLimit"};
constexpr std::string_view kJoinArten[] = {"Inner", "LeftOuter", "RightOuter", "FullOuter", "LeftSemi", "LeftAnti",
                                           "Cross", "LeftSingle"};
// Im eingerueckten Baum (simplified) keine Operatoren, sondern Rahmen bzw. Uebergaenge aus einer Codegen-Stufe.
constexpr std::string_view kCodegen = "WholeStageCodegen";
constexpr std::string_view kAusgang = "InputAdapter";

template <std::size_t N>
[[nodiscard]] bool in(const std::string_view (&liste)[N], std::string_view wort)
{
    return std::find(std::begin(liste), std::end(liste), wort) != std::end(liste);
}

[[nodiscard]] std::string_view stutze(std::string_view s)
{
    while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
    return s;
}

[[nodiscard]] bool wortZeichen(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

/// Ohne die Kennungen der Attribute (`key#12`, `key#x`, `sum#34L` -> `key`, `sum`) und ohne `[plan_id=…]`,
/// `[id=#…]`.
[[nodiscard]] std::string ohneKennungen(std::string_view s)
{
    std::string aus;
    aus.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '#' && i > 0 && wortZeichen(s[i - 1]) && i + 1 < s.size() &&
            (std::isdigit(static_cast<unsigned char>(s[i + 1])) != 0 || s[i + 1] == 'x'))
        {
            std::size_t j = i + 1;
            if (s[j] == 'x') ++j;
            else
                while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j])) != 0) ++j;
            if (j < s.size() && s[j] == 'L' && (j + 1 >= s.size() || !wortZeichen(s[j + 1]))) ++j;
            if (j >= s.size() || !wortZeichen(s[j]))
            {
                i = j - 1;
                continue;
            }
        }
        aus.push_back(s[i]);
    }
    for (const std::string_view marke : {std::string_view(", [plan_id="), std::string_view("[plan_id="),
                                         std::string_view(", [id=#"), std::string_view("[id=#")})
        for (std::size_t p = aus.find(marke); p != std::string::npos; p = aus.find(marke, p))
        {
            const std::size_t zu = aus.find(']', p + marke.size());
            if (zu == std::string::npos) break;
            aus.erase(p, zu + 1 - p);
        }
    return std::string(stutze(aus));
}

/// An den Kommas der obersten Ebene getrennt (Klammern `()[]{}` zaehlen).
[[nodiscard]] std::vector<std::string_view> teile(std::string_view s)
{
    std::vector<std::string_view> aus;
    int tiefe = 0;
    std::size_t a = 0;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        const char c = s[i];
        if (c == '(' || c == '[' || c == '{') ++tiefe;
        else if ((c == ')' || c == ']' || c == '}') && tiefe > 0) --tiefe;
        else if (c == ',' && tiefe == 0)
        {
            aus.push_back(stutze(s.substr(a, i - a)));
            a = i + 1;
        }
    }
    if (const std::string_view rest = stutze(s.substr(a)); !rest.empty() || !aus.empty()) aus.push_back(rest);
    return aus;
}

/// Der Inhalt einer Klammer `[…]` oder `(…)`, die bei `auf` beginnt; die Stelle hinter ihr in `ende`.
[[nodiscard]] std::string_view klammer(std::string_view s, std::size_t auf, std::size_t& ende)
{
    int tiefe = 0;
    for (std::size_t i = auf; i < s.size(); ++i)
    {
        const char c = s[i];
        if (c == '(' || c == '[' || c == '{') ++tiefe;
        else if (c == ')' || c == ']' || c == '}')
            if (--tiefe == 0)
            {
                ende = i + 1;
                return s.substr(auf + 1, i - auf - 1);
            }
    }
    ende = s.size();
    return s.substr(std::min(auf + 1, s.size()));
}

/// `name=[…]` bzw. `name=…` in `(keys=[k], functions=[f], output=[o])`; leer = nicht da.
[[nodiscard]] std::string wertVon(std::string_view args, std::string_view name)
{
    std::string_view s = args;
    if (s.starts_with('(') && s.ends_with(')')) s = s.substr(1, s.size() - 2);
    for (const std::string_view t : teile(s))
        if (t.starts_with(name) && t.size() > name.size() && t[name.size()] == '=')
        {
            std::string_view w = t.substr(name.size() + 1);
            if (w.starts_with('[') && w.ends_with(']')) w = w.substr(1, w.size() - 2);
            return std::string(w);
        }
    return {};
}

/// Eine Liste `[a, b]` ohne Klammern; sonst der Text, wie er ist.
[[nodiscard]] std::string ohneKlammer(std::string_view s)
{
    s = stutze(s);
    if (s.starts_with('[') && s.ends_with(']')) s = s.substr(1, s.size() - 2);
    return std::string(s);
}

/// Die Klammern `[…]` am Anfang hintereinander (simplified: `HashAggregate [k] [count]`).
[[nodiscard]] std::vector<std::string> listen(std::string_view s)
{
    std::vector<std::string> aus;
    std::size_t i = 0;
    while (true)
    {
        while (i < s.size() && s[i] == ' ') ++i;
        if (i >= s.size() || s[i] != '[') break;
        std::size_t ende = 0;
        aus.emplace_back(klammer(s, i, ende));
        i = ende;
    }
    return aus;
}

[[nodiscard]] const std::string* angabe(const PlanOperator& op, std::string_view schluessel)
{
    for (const auto& [k, w] : op.angaben)
        if (k == schluessel) return &w;
    return nullptr;
}

void zeile(std::vector<std::string>& zeilen, std::string_view vorn, std::string_view wert)
{
    const std::string w = ohneKennungen(wert);
    if (w.empty() || w == "[]") return;
    zeilen.push_back(vorn.empty() ? w : std::string(vorn) + ": " + w);
}

[[nodiscard]] bool istJoin(std::string_view name)
{
    return name.ends_with("Join") || name == "CartesianProduct";
}

[[nodiscard]] PlanKnotenArt artVon(const PlanOperator& op)
{
    const std::string_view n = op.name;
    if (in(kScans, n) || n.starts_with("Scan")) return PlanKnotenArt::Scan;
    if (in(kExchanges, n)) return PlanKnotenArt::Exchange;
    if (n == "BroadcastExchange") return PlanKnotenArt::Broadcast;
    if (n == "ReusedExchange" || n == "ReusedSubquery") return PlanKnotenArt::Wiederverwendet;
    if (n == "Subquery" || n == "SubqueryBroadcast") return PlanKnotenArt::Unterabfrage;
    if (istJoin(n)) return PlanKnotenArt::Join;
    if (in(kAggregate, n)) return PlanKnotenArt::Aggregat;
    if (n == "AdaptiveSparkPlan") return PlanKnotenArt::Wurzel;
    return PlanKnotenArt::Operator;
}

/// Die Partitionierung eines Exchange: `hashpartitioning(a, b, 200)` -> `hash: a, b`, `partitions: 200`.
void partitionierung(std::vector<std::string>& zeilen, std::string_view args)
{
    const std::vector<std::string_view> t = teile(args);
    if (t.empty() || t.front().empty()) return;
    const std::string_view p = t.front();
    const std::size_t auf = p.find('(');
    std::string fn(p.substr(0, auf));
    std::transform(fn.begin(), fn.end(), fn.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (auf != std::string_view::npos)
    {
        std::size_t ende = 0;
        const std::vector<std::string_view> innen = teile(klammer(p, auf, ende));
        std::string schluessel;
        for (std::size_t i = 0; i + 1 < innen.size(); ++i) schluessel += (i ? ", " : "") + std::string(innen[i]);
        const std::string_view zahl = innen.empty() ? std::string_view() : innen.back();
        if (fn == "hashpartitioning") zeile(zeilen, "hash", schluessel);
        else if (fn == "rangepartitioning") zeile(zeilen, "range", schluessel);
        else if (fn == "roundrobinpartitioning") zeilen.emplace_back("round robin");
        else zeile(zeilen, "", p);
        if (!zahl.empty() && std::all_of(zahl.begin(), zahl.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
            zeilen.push_back("partitions: " + std::string(zahl));
    }
    else if (fn == "singlepartition")
        zeilen.emplace_back("single partition");
    else if (p.starts_with('['))
        zeile(zeilen, "keys", ohneKlammer(p));  // simplified: die Schluessel
    else
        zeile(zeilen, "", p);
    for (std::size_t i = 1; i < t.size(); ++i)
        if (!t[i].starts_with('[')) zeile(zeilen, "", t[i]);  // ENSURE_REQUIREMENTS, REPARTITION_BY_NUM …
}

/// Was ein Scan liest: `parquet spark_catalog.default.t[a#1,b#2] Batched: true, …, PushedFilters: [...]`.
void scan(std::vector<std::string>& zeilen, std::string& titel, const PlanOperator& op)
{
    std::string_view a = op.argumente;
    std::string_view rest;
    // das Format (parquet, csv, json, orc …) steht vorn, kleingeschrieben
    if (!a.empty() && std::islower(static_cast<unsigned char>(a.front())) != 0 && op.name != "LocalTableScan")
    {
        const std::size_t sp = a.find_first_of(" [");
        const std::string_view format = a.substr(0, sp);
        if (!format.empty() && std::all_of(format.begin(), format.end(), wortZeichen)) titel += " " + std::string(format);
        a = sp == std::string_view::npos ? std::string_view() : stutze(a.substr(sp));
    }
    // die Tabelle bis `[` oder zum ersten Leerzeichen
    std::string spalten;
    if (!a.empty() && a.front() != '[')
    {
        const std::size_t e = a.find_first_of(" [");
        zeile(zeilen, "", a.substr(0, e));
        a = e == std::string_view::npos ? std::string_view() : a.substr(e);
    }
    if (!a.empty() && a.front() == '[')
    {
        std::size_t ende = 0;
        spalten = std::string(klammer(a, 0, ende));
        a = stutze(a.substr(ende));
    }
    else
        rest = a;
    if (spalten.empty())
        if (const std::string* o = angabe(op, "Output")) spalten = ohneKlammer(*o);
    zeile(zeilen, "columns", spalten);
    // die Angaben `Schluessel: Wert` hinter den Spalten (Baum) bzw. in formatted
    std::map<std::string, std::string, std::less<>> werte;
    for (const std::string_view t : teile(a))
        if (const std::size_t d = t.find(": "); d != std::string_view::npos)
            werte[std::string(t.substr(0, d))] = std::string(t.substr(d + 2));
    for (const auto& [k, w] : op.angaben) werte.emplace(k, w);
    if (const auto l = werte.find("Location"); l != werte.end()) zeile(zeilen, "location", l->second);
    if (const auto f = werte.find("PartitionFilters"); f != werte.end()) zeile(zeilen, "partition filters", ohneKlammer(f->second));
    if (const auto f = werte.find("PushedFilters"); f != werte.end()) zeile(zeilen, "pushed filters", ohneKlammer(f->second));
    if (spalten.empty() && werte.empty()) zeile(zeilen, "", rest);
}

/// Was ein Join verbindet: Art, Schluessel (links = rechts), Seite des Broadcasts, Bedingung.
void join(std::vector<std::string>& zeilen, const PlanOperator& op, int& baut)
{
    std::vector<std::string> links;
    std::vector<std::string> rechts;
    std::string art;
    std::string bedingung;
    const auto liste = [](std::string_view s) {
        std::vector<std::string> aus;
        const std::string innen = ohneKlammer(s);  // haelt, worauf teile() zeigt
        for (const std::string_view t : teile(innen)) aus.push_back(ohneKennungen(t));
        return aus;
    };
    const std::vector<std::string_view> t = teile(op.argumente);
    std::size_t i = 0;
    if (t.size() >= 2 && t[0].starts_with('[') && t[1].starts_with('['))
    {
        links = liste(t[0]);
        rechts = liste(t[1]);
        i = 2;
    }
    else if (t.size() == 1 && t[0].starts_with('['))  // simplified: die Schluessel beider Seiten in einer Liste
    {
        const std::vector<std::string> l = listen(t[0]);
        if (!l.empty()) zeile(zeilen, "keys", l[0]);
        if (l.size() > 1) zeile(zeilen, "condition", l[1]);
        i = 1;
    }
    for (; i < t.size(); ++i)
    {
        // formatted: `Inner BuildRight` im Baum; sonst je Wort ein Teil
        for (std::size_t a = 0; a < t[i].size();)
        {
            std::size_t e = t[i].find(' ', a);
            if (e == std::string_view::npos) e = t[i].size();
            const std::string_view w = t[i].substr(a, e - a);
            if (in(kJoinArten, w) || w.starts_with("ExistenceJoin")) art = std::string(w);
            else if (w == "BuildLeft") baut = 0;
            else if (w == "BuildRight") baut = 1;
            else if (w != "false" && w != "true" && !w.empty())
            {
                bedingung = std::string(t[i]);
                break;
            }
            a = e + 1;
        }
    }
    if (const std::string* j = angabe(op, "Join type")) art = *j;
    if (const std::string* l = angabe(op, "Left keys")) links = liste(*l);
    if (const std::string* r = angabe(op, "Right keys")) rechts = liste(*r);
    if (const std::string* c = angabe(op, "Join condition"); c != nullptr) bedingung = *c == "None" ? std::string() : *c;
    zeile(zeilen, "", art);
    if (!links.empty() && links.size() == rechts.size())
    {
        std::string s;
        for (std::size_t k = 0; k < links.size(); ++k) s += (k ? ", " : "") + links[k] + " = " + rechts[k];
        zeile(zeilen, "keys", s);
    }
    if (baut == 0) zeilen.emplace_back("build: left");
    if (baut == 1) zeilen.emplace_back("build: right");
    zeile(zeilen, "condition", bedingung);
}

/// Die Zeilen eines Operators nach seiner Art; ein unbekannter zeigt, was hinter seinem Namen steht.
[[nodiscard]] PlanKnoten knotenVon(const PlanOperator& op, const Plan& plan, int& baut)
{
    PlanKnoten k;
    k.art = artVon(op);
    k.titel = op.name;
    k.zeile = op.zeile;
    std::vector<std::string>& z = k.zeilen;
    const std::string_view a = op.argumente;
    const bool eingerueckt = plan.modus == "simplified";
    switch (k.art)
    {
        case PlanKnotenArt::Scan: scan(z, k.titel, op); break;
        case PlanKnotenArt::Exchange:
            if (const std::string* x = angabe(op, "Arguments")) partitionierung(z, *x);
            else partitionierung(z, a);
            break;
        case PlanKnotenArt::Broadcast:
        {
            const std::string* x = angabe(op, "Arguments");
            const std::string_view m = x != nullptr ? std::string_view(*x) : a;
            if (m.starts_with("HashedRelationBroadcastMode")) z.emplace_back("mode: hashed relation");
            else if (m.starts_with("IdentityBroadcastMode")) z.emplace_back("mode: identity");
            else if (!m.starts_with('[')) zeile(z, "", m);
            break;
        }
        case PlanKnotenArt::Wiederverwendet:
            if (!op.verweis.empty()) z.push_back("reuses " + std::string(eingerueckt ? "#" : "(") + op.verweis + (eingerueckt ? "" : ")"));
            if (const std::string* o = angabe(op, "Output")) zeile(z, "columns", ohneKlammer(*o));
            else if (a.starts_with('[')) zeile(z, "columns", ohneKlammer(teile(a).front()));
            break;
        case PlanKnotenArt::Join: join(z, op, baut); break;
        case PlanKnotenArt::Aggregat:
        {
            std::string schluessel = wertVon(a, "keys");
            std::string funktionen = wertVon(a, "functions");
            if (const std::string* x = angabe(op, "Keys")) schluessel = ohneKlammer(*x);
            if (const std::string* x = angabe(op, "Functions")) funktionen = ohneKlammer(*x);
            if (eingerueckt)
            {
                const std::vector<std::string> l = listen(a);
                if (!l.empty()) schluessel = l[0];
                if (l.size() > 1) funktionen = l[1];
            }
            zeile(z, "keys", schluessel);
            zeile(z, "functions", funktionen);
            break;
        }
        case PlanKnotenArt::Wurzel:
        {
            const std::string* x = angabe(op, "Arguments");
            const std::string_view w = x != nullptr ? std::string_view(*x) : a;
            z.emplace_back(w.find("isFinalPlan=true") != std::string_view::npos ? "final plan: yes" : "final plan: no");
            break;
        }
        case PlanKnotenArt::Unterabfrage:
        case PlanKnotenArt::Operator:
        {
            const std::string_view n = op.name;
            if (n == "Filter")
                zeile(z, "condition", angabe(op, "Condition") != nullptr ? std::string_view(*angabe(op, "Condition")) : a);
            else if (n == "Project")
            {
                if (const std::string* o = angabe(op, "Output")) zeile(z, "columns", ohneKlammer(*o));
                else zeile(z, "columns", ohneKlammer(a));
            }
            else if (n == "Sort")
            {
                const std::string* x = angabe(op, "Arguments");
                const std::vector<std::string_view> t = teile(x != nullptr ? std::string_view(*x) : a);
                if (!t.empty()) zeile(z, "order", ohneKlammer(t.front()));
            }
            else if (n == "TakeOrderedAndProject")
            {
                std::string grenze = wertVon(a, "limit");
                std::string ordnung = wertVon(a, "orderBy");
                if (const std::string* x = angabe(op, "Arguments"))
                {
                    const std::vector<std::string_view> t = teile(*x);
                    if (!t.empty()) grenze = std::string(t[0]);
                    if (t.size() > 1) ordnung = ohneKlammer(t[1]);
                }
                zeile(z, "limit", grenze);
                zeile(z, "order", ordnung);
                if (grenze.empty() && ordnung.empty()) zeile(z, "columns", ohneKlammer(a));
            }
            else if (in(kGrenzen, n))
            {
                const std::string* x = angabe(op, "Arguments");
                const std::vector<std::string_view> t = teile(x != nullptr ? std::string_view(*x) : a);
                if (!t.empty()) zeile(z, "limit", t.front());
            }
            else if (const std::string* x = angabe(op, "Arguments"))
                zeile(z, "", *x);
            else
                zeile(z, "", a);
            break;
        }
    }
    if (op.nummer > 0) k.titel += " (" + std::to_string(op.nummer) + ")";
    return k;
}

class Bau
{
public:
    Bau(const Plan& plan, const Planbaum& baum, Plandiagramm& d) : m_plan(plan), m_baum(baum), m_d(d)
    {
        m_knotenVon.assign(baum.operatoren.size(), kKeine);
        m_codegen.assign(0, 0);
    }

    void baue(bool physisch)
    {
        // die Wurzeln: [0], dann die Unterabfragen aus formatted
        std::vector<bool> istWurzel(m_baum.operatoren.size(), false);
        if (!m_baum.operatoren.empty()) istWurzel[0] = true;
        for (const modell::PlanUnterabfrage& u : m_baum.unterabfragen)
            if (u.wurzel != modell::kKeinBlock && u.wurzel < istWurzel.size()) istWurzel[u.wurzel] = true;
        if (!m_baum.operatoren.empty()) besuche(0, kKeine, 0, PlanKantenArt::Fluss, {}, kKeine);
        for (const modell::PlanUnterabfrage& u : m_baum.unterabfragen)
        {
            if (u.wurzel == modell::kKeinBlock || u.wurzel >= m_baum.operatoren.size()) continue;
            const std::size_t rahmen = neuerRahmen(PlanRahmenArt::Unterabfrage, "Subquery " + std::to_string(u.nummer), kKeine);
            besuche(u.wurzel, kKeine, 0, PlanKantenArt::Fluss, {}, rahmen);
        }
        verbindeUnterabfragen();
        verbindeWiederverwendete();
        if (physisch) teileStages();
    }

private:
    std::size_t neuerRahmen(PlanRahmenArt art, std::string titel, std::size_t eltern)
    {
        m_d.rahmen.push_back({art, std::move(titel), eltern});
        return m_d.rahmen.size() - 1;
    }

    /// Ein Operator und was unter ihm steht. `eltern`: der Knoten, in den seine Daten gehen; `codegen`: die
    /// Stufe, die ihm ein Rahmen `WholeStageCodegen (n)` im eingerueckten Baum gibt; `unter`: der Rahmen der
    /// Unterabfrage, in der er steht.
    void besuche(std::size_t i, std::size_t eltern, int codegen, PlanKantenArt art, std::string text, std::size_t unter)
    {
        const PlanOperator& op = m_baum.operatoren[i];
        const bool unterabfrage = op.innen || artVon(op) == PlanKnotenArt::Unterabfrage;
        if (unterabfrage && eltern != kKeine)
        {
            art = PlanKantenArt::Unterabfrage;
            if (text.empty()) text = "subquery";
            unter = neuerRahmen(PlanRahmenArt::Unterabfrage, "Subquery " + std::to_string(++m_unterabfragen), unter);
        }
        // simplified: WholeStageCodegen ist ein Rahmen, InputAdapter der Uebergang hinaus — beide keine Knoten
        if (op.name == kCodegen || op.name == kAusgang)
        {
            int stufe = 0;
            if (op.name == kCodegen && op.nummer > 0)
                stufe = op.nummer;  // `WholeStageCodegen (9)`: der Leser liest die Klammer hinten als Nummer
            else if (op.name == kCodegen)
            {
                std::string_view a = op.argumente;
                if (a.starts_with('(') && a.ends_with(')')) a = a.substr(1, a.size() - 2);
                stufe = !a.empty() && std::all_of(a.begin(), a.end(), [](unsigned char c) { return std::isdigit(c) != 0; })
                            ? std::stoi(std::string(a))
                            : -1;
            }
            for (const std::size_t k : op.kinder) besuche(k, eltern, stufe, art, text, unter);
            return;
        }
        int baut = -1;
        PlanKnoten k = knotenVon(op, m_plan, baut);
        k.gruppe = unter;
        const std::size_t n = m_d.knoten.size();
        m_d.knoten.push_back(std::move(k));
        m_knotenVon[i] = n;
        m_codegen.push_back(op.codegen != 0 ? op.codegen : codegen);
        m_unter.push_back(unter);
        if (eltern != kKeine) m_d.kanten.push_back({n, eltern, art, std::move(text)});
        for (std::size_t c = 0; c < op.kinder.size(); ++c)
        {
            const std::size_t kind = op.kinder[c];
            std::string t = static_cast<int>(c) == baut && !m_baum.operatoren[kind].innen ? std::string("build") : std::string();
            // eine Stufe aus dem Baum (`*(3)`) gilt nur fuer ihn; die eines Rahmens im eingerueckten Baum erben die Kinder
            besuche(kind, n, op.codegen != 0 ? 0 : codegen, PlanKantenArt::Fluss, std::move(t), unter);
        }
    }

    /// formatted: das Ergebnis jeder Unterabfrage in ihren Gastgeber. Eine ohne eigenen Baum nutzt eine mit demselben
    /// Schluessel — davor oder dahinter: der Ausdruck ohne `ReusedSubquery ` (`Subquery scalar-subquery#27, [id=#7]`),
    /// bei `x IN dynamicpruning#6` das Stueck hinter `IN`. Passen mehrere (Kennungen maskiert, `#x`): ein Hinweis.
    void verbindeUnterabfragen()
    {
        const auto nachNummer = [&](int nummer) -> std::size_t {
            for (std::size_t i = 0; i < m_baum.operatoren.size(); ++i)
                if (m_baum.operatoren[i].nummer == nummer) return m_knotenVon[i];
            return kKeine;
        };
        const auto schluessel = [](std::string_view s) {
            constexpr std::string_view kWieder = "ReusedSubquery ";
            if (s.starts_with(kWieder)) s.remove_prefix(kWieder.size());
            if (const std::size_t in = s.rfind(" IN "); in != std::string_view::npos) s = s.substr(in + 4);
            return std::string(s);
        };
        for (std::size_t u = 0; u < m_baum.unterabfragen.size(); ++u)
        {
            const modell::PlanUnterabfrage& x = m_baum.unterabfragen[u];
            const std::string wo = "Subquery:" + std::to_string(x.nummer) + " (line " + std::to_string(x.zeile) + ")";
            const std::size_t gast = nachNummer(x.gastgeber);
            std::size_t quelle = x.wurzel != modell::kKeinBlock && x.wurzel < m_knotenVon.size() ? m_knotenVon[x.wurzel] : kKeine;
            std::size_t passend = quelle == kKeine ? 0 : 1;
            if (quelle == kKeine)
                for (const modell::PlanUnterabfrage& y : m_baum.unterabfragen)
                    if (y.wurzel != modell::kKeinBlock && y.wurzel < m_knotenVon.size() && schluessel(y.ausdruck) == schluessel(x.ausdruck) &&
                        m_knotenVon[y.wurzel] != quelle)
                    {
                        quelle = m_knotenVon[y.wurzel];
                        ++passend;
                    }
            if (gast == kKeine)
            {
                m_d.hinweise.push_back(wo + ": its hosting operator (" + std::to_string(x.gastgeber) + ") is not in the plan");
                continue;
            }
            if (passend == 0)
            {
                m_d.hinweise.push_back(wo + ": the subquery it reuses is not in the plan");
                continue;
            }
            if (passend > 1)
            {
                m_d.hinweise.push_back(wo + ": " + std::to_string(passend) + " subqueries match '" + ohneKennungen(x.ausdruck) +
                                       "' - which one it reuses is not shown");
                continue;
            }
            m_d.kanten.push_back({quelle, gast, PlanKantenArt::Unterabfrage, ohneKennungen(x.ausdruck)});
        }
    }

    /// Im Baum von simple steht die Kennung im Text: `ReusedSubquery Subquery scalar-subquery#31, [id=#301]` meint die
    /// Unterabfrage mit genau diesem Text, `ReusedExchange […], Exchange …, [plan_id=12]` den Exchange mit
    /// `[plan_id=12]`. Gibt den Knoten (kKeine = keiner oder mehrere).
    [[nodiscard]] std::size_t wiederverwendetImText(std::size_t i) const
    {
        const PlanOperator& op = m_baum.operatoren[i];
        std::string kennung;
        if (op.name == "ReusedSubquery")
            kennung = op.argumente;
        else if (const std::size_t auf = op.argumente.rfind("[plan_id="); auf != std::string::npos)
            kennung = op.argumente.substr(auf, op.argumente.find(']', auf) - auf + 1);
        if (kennung.empty()) return kKeine;
        std::size_t ziel = kKeine;
        for (std::size_t j = 0; j < m_baum.operatoren.size(); ++j)
        {
            const PlanOperator& o = m_baum.operatoren[j];
            if (j == i || artVon(o) == PlanKnotenArt::Wiederverwendet || m_knotenVon[j] == kKeine) continue;
            const bool passt = op.name == "ReusedSubquery"
                                   ? artVon(o) == PlanKnotenArt::Unterabfrage && o.name + " " + o.argumente == kennung
                                   : o.name.ends_with("Exchange") && o.argumente.find(kennung) != std::string::npos;
            if (!passt) continue;
            if (ziel != kKeine) return kKeine;
            ziel = m_knotenVon[j];
        }
        return ziel;
    }

    /// ReusedExchange und ReusedSubquery: eine Kante von dem, den sie wiederverwenden — formatted ueber die Nummer
    /// (`Reuses operator id: 49`), der eingerueckte Baum ueber die Kennung `#3`.
    void verbindeWiederverwendete()
    {
        const bool eingerueckt = m_plan.modus == "simplified";
        for (std::size_t i = 0; i < m_baum.operatoren.size(); ++i)
        {
            const PlanOperator& op = m_baum.operatoren[i];
            if (artVon(op) != PlanKnotenArt::Wiederverwendet || m_knotenVon[i] == kKeine) continue;
            if (op.verweis.empty() && !eingerueckt)
                if (const std::size_t z = wiederverwendetImText(i); z != kKeine)
                {
                    m_d.kanten.push_back({z, m_knotenVon[i], PlanKantenArt::Wiederverwendung, "reuses"});
                    continue;
                }
            if (op.verweis.empty())
            {
                m_d.hinweise.push_back(op.name + " (line " + std::to_string(op.zeile) + "): the plan does not say what it reuses");
                continue;
            }
            const bool exchange = op.name == "ReusedExchange";
            std::size_t ziel = kKeine;
            for (std::size_t j = 0; j < m_baum.operatoren.size() && ziel == kKeine; ++j)
            {
                const PlanOperator& o = m_baum.operatoren[j];
                if (j == i || artVon(o) == PlanKnotenArt::Wiederverwendet) continue;
                if (eingerueckt)
                {
                    const bool passt = exchange ? o.name.ends_with("Exchange") : artVon(o) == PlanKnotenArt::Unterabfrage;
                    if (passt && o.verweis == op.verweis) ziel = m_knotenVon[j];
                }
                else if (std::to_string(o.nummer) == op.verweis)
                    ziel = m_knotenVon[j];
            }
            if (ziel == kKeine)
            {
                m_d.hinweise.push_back(op.name + " (line " + std::to_string(op.zeile) + "): what it reuses is not in the plan");
                continue;
            }
            m_d.kanten.push_back({ziel, m_knotenVon[i], PlanKantenArt::Wiederverwendung, "reuses"});
        }
    }

    /// Die Stages: was ohne einen Exchange dazwischen zusammenhaengt, ohne die Kanten der Unterabfragen. Ein
    /// Exchange beendet die Stage, die in ihn schreibt; ein wiederverwendeter gehoert zu der, die ihn liest. Darin
    /// je Codegen-Stufe ein Rahmen. Gezaehlt von den Blaettern her.
    void teileStages()
    {
        const std::size_t n = m_d.knoten.size();
        const auto grenze = [&](std::size_t k) {
            const PlanKnotenArt a = m_d.knoten[k].art;
            return a == PlanKnotenArt::Exchange || a == PlanKnotenArt::Broadcast;
        };
        std::vector<std::size_t> wurzel(n);
        std::iota(wurzel.begin(), wurzel.end(), 0);
        const std::function<std::size_t(std::size_t)> finde = [&](std::size_t x) {
            while (wurzel[x] != x) x = wurzel[x] = wurzel[wurzel[x]];
            return x;
        };
        for (const PlanKante& e : m_d.kanten)
            if (e.art == PlanKantenArt::Fluss && !grenze(e.von)) wurzel[finde(e.von)] = finde(e.nach);
        // die Reihenfolge: von den Blaettern her (der spaeteste Knoten im Baum zuerst — Kinder stehen hinter Eltern)
        std::vector<std::size_t> stageVon(n, kKeine);
        std::map<std::size_t, std::size_t> rahmenVon;  // Wurzel der Menge -> Rahmen
        for (std::size_t k = n; k-- > 0;)
        {
            const std::size_t w = finde(k);
            auto it = rahmenVon.find(w);
            if (it == rahmenVon.end())
                it = rahmenVon.emplace(w, neuerRahmen(PlanRahmenArt::Stage, "Stage " + std::to_string(++m_d.stages), m_unter[k])).first;
            stageVon[k] = it->second;
        }
        // Codegen: je Stage und Kennung; ohne Kennung (`*`) je zusammenhaengendes Stueck
        std::vector<std::size_t> cg(n);
        std::iota(cg.begin(), cg.end(), 0);
        const std::function<std::size_t(std::size_t)> findeCg = [&](std::size_t x) {
            while (cg[x] != x) x = cg[x] = cg[cg[x]];
            return x;
        };
        for (const PlanKante& e : m_d.kanten)
            if (e.art == PlanKantenArt::Fluss && m_codegen[e.von] != 0 && m_codegen[e.von] == m_codegen[e.nach] &&
                stageVon[e.von] == stageVon[e.nach] && stageVon[e.von] != kKeine)
                cg[findeCg(e.von)] = findeCg(e.nach);
        std::map<std::pair<std::size_t, long long>, std::size_t> cgRahmen;
        for (std::size_t k = n; k-- > 0;)
        {
            m_d.knoten[k].gruppe = stageVon[k] != kKeine ? stageVon[k] : m_unter[k];
            if (stageVon[k] == kKeine || m_codegen[k] == 0) continue;
            const long long schluessel = m_codegen[k] > 0 ? m_codegen[k] : -static_cast<long long>(findeCg(k)) - 1;
            const auto paar = std::make_pair(stageVon[k], schluessel);
            auto it = cgRahmen.find(paar);
            if (it == cgRahmen.end())
            {
                const std::string titel = m_codegen[k] > 0 ? std::string(kCodegen) + " (" + std::to_string(m_codegen[k]) + ")"
                                                           : std::string(kCodegen);
                it = cgRahmen.emplace(paar, neuerRahmen(PlanRahmenArt::Codegen, titel, stageVon[k])).first;
            }
            m_d.knoten[k].gruppe = it->second;
        }
    }

    const Plan& m_plan;
    const Planbaum& m_baum;
    Plandiagramm& m_d;
    std::vector<std::size_t> m_knotenVon;  ///< je Operator sein Knoten; kKeine = keiner (Rahmen im eingerueckten Baum)
    std::vector<int> m_codegen;            ///< je Knoten seine Codegen-Stufe
    std::vector<std::size_t> m_unter;      ///< je Knoten der Rahmen seiner Unterabfrage
    int m_unterabfragen = 0;
};

}  // namespace

std::string_view stereotyp(PlanKnotenArt art)
{
    switch (art)
    {
        case PlanKnotenArt::Exchange: return "\xC2\xAB" "shuffle" "\xC2\xBB";
        case PlanKnotenArt::Broadcast: return "\xC2\xAB" "broadcast" "\xC2\xBB";
        case PlanKnotenArt::Wiederverwendet: return "\xC2\xAB" "reused" "\xC2\xBB";
        case PlanKnotenArt::Unterabfrage: return "\xC2\xAB" "subquery" "\xC2\xBB";
        default: return {};
    }
}

std::vector<std::string> texte(const PlanKnoten& knoten)
{
    std::vector<std::string> aus;
    if (const std::string_view s = stereotyp(knoten.art); !s.empty()) aus.emplace_back(s);
    aus.push_back(knoten.titel);
    aus.insert(aus.end(), knoten.zeilen.begin(), knoten.zeilen.end());
    return aus;
}

std::size_t physischerBaum(const modell::Plan& plan)
{
    for (std::size_t i = 0; i < plan.baeume.size(); ++i)
        if (plan.baeume[i].art == "Physical Plan") return i;
    return kKeine;
}

std::string kurzname(const modell::Planbaum& baum)
{
    std::string_view a = baum.art;
    const std::size_t sp = a.find(' ');
    std::string aus(a.substr(0, sp));
    std::transform(aus.begin(), aus.end(), aus.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return aus;
}

Plandiagramm planVon(const modell::Plan& plan, std::size_t baum)
{
    Plandiagramm d;
    d.aufruf = plan.aufruf;
    if (plan.baeume.empty()) return d;
    if (baum == kKeine || baum >= plan.baeume.size())
    {
        baum = physischerBaum(plan);
        if (baum == kKeine) baum = plan.baeume.size() - 1;
    }
    const Planbaum& b = plan.baeume[baum];
    d.titel = plan.name + ": " + b.art;
    if (plan.adaptiv) d.titel += plan.endgueltig ? " (adaptive, final)" : " (adaptive, not final)";
    const bool physisch = b.art == "Physical Plan";
    Bau(plan, b, d).baue(physisch);
    return d;
}

}  // namespace c2d::diagramm
