/**
 * @file Lauf.cpp
 * @brief Ein Lauf und sein Code (Entscheide L5, L5-1 bis L5-3) — siehe Lauf.hpp
 */
#include "c2d/projekt/Lauf.hpp"

#include "c2d/projekt/Datenfluss.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>

namespace c2d::projekt
{
namespace
{

/// Was der Log anders nennt als der Code: `show()` heisst im Log `showString`.
constexpr std::pair<std::string_view, std::string_view> kLogNamen[] = {{"showString", "show"}};

[[nodiscard]] bool gleicheMethode(std::string_view imCode, std::string_view imLog)
{
    if (imCode == imLog) return true;
    return std::any_of(std::begin(kLogNamen), std::end(kLogNamen),
                       [&](const auto& p) { return p.first == imLog && p.second == imCode; });
}

[[nodiscard]] std::string klein(std::string_view s)
{
    std::string aus(s);
    for (char& c : aus)
    {
        if (c == '\\') c = '/';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return aus;
}

[[nodiscard]] std::string_view letzterTeil(std::string_view pfad)
{
    const std::size_t s = pfad.find_last_of("/\\");
    return s == std::string_view::npos ? pfad : pfad.substr(s + 1);
}

/// Die Python-Stelle einer Aufrufstelle (leer: keine).
[[nodiscard]] std::optional<Aufrufstelle> pythonStelle(std::string_view text)
{
    std::optional<Aufrufstelle> s = aufrufstelleAus(text);
    if (!s || s->pfad.empty()) return std::nullopt;
    return s;
}

/// Was die Zuordnung braucht und nur einmal rechnet: je Datei ihr Datenfluss.
class Zuordnung
{
public:
    explicit Zuordnung(const Projekt& projekt) : m_projekt(projekt) {}

    const diagramm::Datenflussdiagramm& fluss(std::size_t datei)
    {
        const auto [it, neu] = m_fluss.try_emplace(datei);
        if (neu) it->second = datenflussIn(m_projekt, datei, "");
        return it->second;
    }

    /// Die Aktionen des Datenflusses einer Datei: (Zeile, Methode).
    std::vector<std::pair<int, std::string>> aktionen(std::size_t datei)
    {
        std::vector<std::pair<int, std::string>> aus;
        for (const diagramm::DatenflussKnoten& k : fluss(datei).knoten)
            if (k.art == diagramm::DatenflussKnotenArt::Aktion && !k.methode.empty()) aus.emplace_back(k.zeile, k.methode);
        return aus;
    }

    /// Wo eine Python-Stelle im Projekt steht (L5-1, L5-2). `geaendert`: ueber die Zeile statt den Hash gefunden.
    std::vector<Codestelle> stellen(const Aufrufstelle& s, bool& geaendert)
    {
        geaendert = false;
        std::vector<Codestelle> aus;
        const std::string pfad = klein(s.pfad);
        const std::string_view name = letzterTeil(s.pfad);
        // im Notebook: `…/ipykernel_<pid>/<hash>.py`
        if (pfad.find("/ipykernel_") != std::string::npos && name.size() > 3 && name.ends_with(".py") &&
            std::all_of(name.begin(), name.end() - 3, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
        {
            std::uint32_t hash = 0;
            try
            {
                hash = static_cast<std::uint32_t>(std::stoull(std::string(name.substr(0, name.size() - 3))));
            }
            catch (...)
            {
                return aus;
            }
            for (std::size_t d = 0; d < m_projekt.dateien.size(); ++d)
                for (const Zelle& z : m_projekt.dateien[d].zellen)
                    if (z.kernHash && *z.kernHash == hash) aus.push_back({d, z.zeile + s.zeile - 1});
            if (!aus.empty()) return aus;
            // nach dem Lauf geaendert: die Zellen mit einer Aktion dieses Namens auf dieser Zeile
            for (std::size_t d = 0; d < m_projekt.dateien.size(); ++d)
                for (const Zelle& z : m_projekt.dateien[d].zellen)
                {
                    if (z.art != Zelle::Art::Code || !z.kernHash) continue;
                    const int zeile = z.zeile + s.zeile - 1;
                    if (zeile > z.bisZeile) continue;
                    for (const diagramm::DatenflussKnoten& k : fluss(d).knoten)
                        if (k.zeile == zeile && gleicheMethode(k.methode, s.methode))
                        {
                            aus.push_back({d, zeile});
                            break;
                        }
                }
            geaendert = !aus.empty();
            return aus;
        }
        // ein Pfad im Projekt; sonst der Dateiname
        if (!m_projekt.wurzel.empty())
        {
            std::error_code fehler;
            const std::filesystem::path absolut = std::filesystem::absolute(m_projekt.wurzel, fehler).lexically_normal();
            std::string wurzel = klein((fehler ? m_projekt.wurzel : absolut).generic_string());
            if (!wurzel.empty() && wurzel.back() != '/') wurzel.push_back('/');
            if (pfad.starts_with(wurzel))
                for (std::size_t d = 0; d < m_projekt.dateien.size(); ++d)
                    if (klein(m_projekt.dateien[d].pfad) == pfad.substr(wurzel.size())) return {{d, s.zeile}};
        }
        const std::string kname = klein(name);
        for (std::size_t d = 0; d < m_projekt.dateien.size(); ++d)
            if (klein(letzterTeil(m_projekt.dateien[d].pfad)) == kname) aus.push_back({d, s.zeile});
        return aus;
    }

private:
    const Projekt& m_projekt;
    std::map<std::size_t, diagramm::Datenflussdiagramm> m_fluss;
};

/// Eine Aktion im Lauf, bevor sie zugeordnet ist.
struct Kandidat
{
    LaufAktion aktion;
    long long start = 0;
    std::optional<Aufrufstelle> stelle;  ///< nur eine Python-Stelle
};

[[nodiscard]] std::string zeileText(const Codestelle& c, const Projekt& p)
{
    return std::string(letzterTeil(p.dateien[c.datei].pfad)) + ":" + std::to_string(c.zeile);
}

}  // namespace

std::optional<Aufrufstelle> aufrufstelleAus(std::string_view text)
{
    const std::size_t at = text.find(" at ");
    if (at == std::string_view::npos || at == 0) return std::nullopt;
    Aufrufstelle s;
    s.methode = std::string(text.substr(0, at));
    std::string_view ort = text.substr(at + 4);
    if (const std::size_t nl = ort.find('\n'); nl != std::string_view::npos) ort = ort.substr(0, nl);
    const std::size_t doppel = ort.rfind(':');
    if (doppel == std::string_view::npos) return s;
    const std::string_view zahl = ort.substr(doppel + 1);
    if (zahl.empty() || !std::all_of(zahl.begin(), zahl.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
        return s;
    const std::string_view pfad = ort.substr(0, doppel);
    // nur Python: `x.py`; `NativeMethodAccessorImpl.java:0`, `PythonRDD.scala:326` sind keine Stelle im Code
    if (klein(pfad).ends_with(".py"))
    {
        s.pfad = std::string(pfad);
        s.zeile = std::stoi(std::string(zahl));
    }
    return s;
}

Laufverbindung verbindeLauf(const Projekt& projekt, std::size_t log)
{
    Laufverbindung v;
    if (log >= projekt.dateien.size()) return v;
    const std::vector<modell::Lauf>& laeufe = projekt.dateien[log].ergebnis.einheit.laeufe;
    if (laeufe.empty()) return v;
    const modell::Lauf& lauf = laeufe.front();
    Zuordnung z(projekt);

    // die Aktionen des Laufs: je Ausfuehrung (ohne die in einer anderen) und je Job ohne Ausfuehrung mit Stelle
    std::vector<Kandidat> kandidaten;
    for (const modell::LaufAusfuehrung& x : lauf.ausfuehrungen)
    {
        if (x.wurzel >= 0 && x.wurzel != x.id) continue;
        Kandidat k;
        k.aktion.ausfuehrung = x.id;
        k.start = x.start;
        for (const modell::LaufJob& j : lauf.jobs)
            if (j.sql == std::to_string(x.id)) k.aktion.jobs.push_back(j.id);
        if (const std::optional<Aufrufstelle> s = aufrufstelleAus(x.beschreibung))
        {
            k.aktion.methode = s->methode;
            if (!s->pfad.empty()) k.stelle = s;
        }
        kandidaten.push_back(std::move(k));
    }
    for (const modell::LaufJob& j : lauf.jobs)
    {
        if (!j.sql.empty()) continue;
        const std::optional<Aufrufstelle> s = pythonStelle(j.aufruf);
        if (!s) continue;  // ein Job ohne Ausfuehrung und ohne Stelle (das Schema einer Quelle lesen) ist keine Aktion
        Kandidat k;
        k.aktion.jobs.push_back(j.id);
        k.aktion.methode = s->methode;
        k.stelle = s;
        k.start = j.start;
        kandidaten.push_back(std::move(k));
    }
    std::stable_sort(kandidaten.begin(), kandidaten.end(), [](const Kandidat& a, const Kandidat& b) { return a.start < b.start; });

    // mit Stelle: wohin (L5-1, L5-2)
    std::size_t mitStelle = 0;
    std::vector<std::string> fehlen;  // Dateien des Logs, die das Projekt nicht hat (je eine Meldung)
    for (Kandidat& k : kandidaten)
    {
        if (!k.stelle) continue;
        ++mitStelle;
        bool geaendert = false;
        k.aktion.stellen = z.stellen(*k.stelle, geaendert);
        if (k.aktion.stellen.empty() && std::find(fehlen.begin(), fehlen.end(), k.stelle->pfad) == fehlen.end())
        {
            fehlen.push_back(k.stelle->pfad);
            v.hinweise.push_back(k.stelle->pfad.find("ipykernel_") != std::string::npos
                                     ? std::string(letzterTeil(k.stelle->pfad)) + ": no cell of a notebook in the project has this text (ipykernel hash)"
                                     : std::string(letzterTeil(k.stelle->pfad)) + " is not in the project (" + k.stelle->pfad + ")");
        }
        if (geaendert)
            v.hinweise.push_back(k.stelle->methode + " (line " + std::to_string(k.stelle->zeile) + " of a cell): cell changed since the run");
        if (k.aktion.stellen.size() > 1)
            v.hinweise.push_back(k.stelle->methode + " at " + std::string(letzterTeil(k.stelle->pfad)) + ":" +
                                 std::to_string(k.stelle->zeile) + ": " + std::to_string(k.aktion.stellen.size()) +
                                 " places in the project match - chosen in the menu");
    }
    if (mitStelle == 0 && !kandidaten.empty())
        v.hinweise.emplace_back("no Python location in the log (count, show, write carry none)");

    // ohne Stelle: nach dem Namen zwischen zwei Ankern derselben Datei (L5-3)
    std::map<std::size_t, std::vector<std::size_t>> ankerJeDatei;  // je Datei die Kandidaten mit genau einer Stelle
    for (std::size_t i = 0; i < kandidaten.size(); ++i)
        if (kandidaten[i].stelle && kandidaten[i].aktion.stellen.size() == 1)
            ankerJeDatei[kandidaten[i].aktion.stellen.front().datei].push_back(i);
    for (const auto& [datei, anker] : ankerJeDatei)
    {
        const std::vector<std::pair<int, std::string>> imText = z.aktionen(datei);
        for (std::size_t a = 0; a + 1 < anker.size(); ++a)
        {
            const std::size_t i = anker[a];
            const std::size_t j = anker[a + 1];
            const int von = kandidaten[i].aktion.stellen.front().zeile;
            const int bis = kandidaten[j].aktion.stellen.front().zeile;
            if (von >= bis) continue;  // der Lauf folgt hier nicht dem Text (Zellen durcheinander, eine Schleife)
            for (std::size_t u = i + 1; u < j; ++u)
            {
                Kandidat& k = kandidaten[u];
                if (k.stelle || !k.aktion.stellen.empty() || k.aktion.methode.empty()) continue;
                const auto gleichImLog = std::count_if(kandidaten.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                                                       kandidaten.begin() + static_cast<std::ptrdiff_t>(j), [&](const Kandidat& x) {
                                                           return !x.stelle && x.aktion.methode == k.aktion.methode;
                                                       });
                std::vector<int> zeilen;
                for (const auto& [zeile, methode] : imText)
                    if (zeile > von && zeile < bis && gleicheMethode(methode, k.aktion.methode)) zeilen.push_back(zeile);
                if (gleichImLog == 1 && zeilen.size() == 1)
                {
                    k.aktion.stellen.push_back({datei, zeilen.front()});
                    k.aktion.nachName = true;
                }
            }
        }
        // Aktionen des Texts, zu denen nichts im Lauf gefunden wurde
        for (const auto& [zeile, methode] : imText)
        {
            const bool verbunden = std::any_of(kandidaten.begin(), kandidaten.end(), [&](const Kandidat& k) {
                return std::find(k.aktion.stellen.begin(), k.aktion.stellen.end(), Codestelle{datei, zeile}) != k.aktion.stellen.end();
            });
            if (!verbunden)
                v.hinweise.push_back(zeileText({datei, zeile}, projekt) + ": " + methode +
                                     "() is not linked to the run - no location in the log, and its name is not unique between "
                                     "two actions with one");
        }
    }
    for (Kandidat& k : kandidaten) v.aktionen.push_back(std::move(k.aktion));

    // die Stages mit Python-Stelle (RDD)
    for (const modell::LaufStage& s : lauf.stages)
    {
        const std::optional<Aufrufstelle> st = pythonStelle(s.name);
        if (!st) continue;
        bool geaendert = false;
        LaufStageStelle x;
        x.stage = s.id;
        x.stellen = z.stellen(*st, geaendert);
        if (!x.stellen.empty() &&
            std::none_of(v.stages.begin(), v.stages.end(), [&](const LaufStageStelle& y) { return y.stage == s.id; }))
            v.stages.push_back(std::move(x));
    }
    return v;
}

std::vector<Codestelle> codestellenVon(const Laufverbindung& v, int job, int stage)
{
    if (stage >= 0)
        for (const LaufStageStelle& s : v.stages)
            if (s.stage == stage) return s.stellen;
    if (job >= 0)
        for (const LaufAktion& a : v.aktionen)
            if (std::find(a.jobs.begin(), a.jobs.end(), job) != a.jobs.end()) return a.stellen;
    return {};
}

std::vector<LaufTreffer> laeufeAn(const Projekt& projekt, std::size_t datei, int zeile)
{
    std::vector<LaufTreffer> aus;
    const Codestelle hier{datei, zeile};
    for (std::size_t log = 0; log < projekt.dateien.size(); ++log)
    {
        if (projekt.dateien[log].ergebnis.einheit.laeufe.empty()) continue;
        const Laufverbindung v = verbindeLauf(projekt, log);
        for (const LaufAktion& a : v.aktionen)
            if (std::find(a.stellen.begin(), a.stellen.end(), hier) != a.stellen.end()) aus.push_back({log, a.jobs, -1});
        for (const LaufStageStelle& s : v.stages)
            if (std::find(s.stellen.begin(), s.stellen.end(), hier) != s.stellen.end()) aus.push_back({log, {}, s.stage});
    }
    return aus;
}

}  // namespace c2d::projekt
