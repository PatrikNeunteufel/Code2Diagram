/**
 * @file Abhaengigkeiten.cpp
 * @brief Die Abhaengigkeiten der Daten: erst der ganze Graph des Projekts, dann ein Ausschnitt
 */
#include "c2d/projekt/Abhaengigkeiten.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace c2d::projekt
{

namespace
{

using diagramm::AufrufKnotenArt;
using diagramm::ZugriffKante;

std::string klein(std::string_view text)
{
    std::string s(text);
    for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return s;
}

std::string letzterTeil(std::string_view name)
{
    const std::size_t punkt = name.find_last_of('.');
    return klein(punkt == std::string_view::npos ? name : name.substr(punkt + 1));
}

/// Der Name eines Triggers fuer datenobjekteIn und abhaengigkeitenAb: Trigger heissen je Tabelle (Postgres).
std::string triggerName(const modell::Datenbankobjekt& o)
{
    return o.name + "@" + o.tabelle;
}

std::string_view stereotyp(const modell::Datenbankobjekt& o)
{
    // in Guillemets (U+00AB, U+00BB), als Bytes: die Quelle bleibt reines ASCII
    switch (o.art)
    {
        case modell::ObjektArt::View: return "\xC2\xAB" "view" "\xC2\xBB";
        case modell::ObjektArt::Trigger: return "\xC2\xAB" "trigger" "\xC2\xBB";
        case modell::ObjektArt::Funktion: return "\xC2\xAB" "function" "\xC2\xBB";
        case modell::ObjektArt::Prozedur: return "\xC2\xAB" "procedure" "\xC2\xBB";
    }
    return {};
}

constexpr std::string_view kTabelle = "\xC2\xAB" "table" "\xC2\xBB";

/// Der ganze Graph des Projekts.
class Gesamt
{
public:
    struct Knoten
    {
        AufrufKnotenArt art = AufrufKnotenArt::Tabelle;
        std::string name;
        std::string stereotyp;
        Datenstelle stelle;
        int zeile = 0;
        std::string schluessel;  ///< wie ihn datenobjekteIn nennt
        /// Je Datei, die ihn anlegt, wo (dasselbe Schema in mehreren Dateien, etwa je Dialekt): die Ansicht
        /// einer Datei zeigt ihn mit ihrer Stelle.
        std::map<std::size_t, std::pair<Datenstelle, int>> definiert;
    };
    struct Kante
    {
        std::size_t von = 0;
        std::size_t nach = 0;
        ZugriffKante zugriff = ZugriffKante::Ruft;
        std::string ereignis;
    };

    explicit Gesamt(const Projekt& p)
    {
        // 1. Tabellen und Objekte; ein Objekt, das spaeter neu festgelegt wird (CREATE OR REPLACE), gilt so
        std::vector<std::pair<std::size_t, std::size_t>> objekte;  // je Knoten eines Objekts: Datei, Index
        for (std::size_t d = 0; d < p.dateien.size(); ++d)
        {
            const modell::Einheit& e = p.dateien[d].ergebnis.einheit;
            for (std::size_t i = 0; i < e.typen.size(); ++i)
            {
                const modell::Typ& t = e.typen[i];
                if (t.art != modell::TypArt::Tabelle) continue;
                const Datenstelle hier{Datenstelle::Art::Tabelle, d, i};
                if (const auto da = m_tabellen.find(klein(t.name)); da != m_tabellen.end())
                {
                    m_k[da->second].definiert.try_emplace(d, hier, t.zeile);  // dieselbe Tabelle noch einmal
                    continue;
                }
                const std::size_t n = neu({AufrufKnotenArt::Tabelle, t.name, std::string(kTabelle), hier, t.zeile, t.name, {}});
                m_tabellen[klein(t.name)] = n;
                m_tabellenTeil[letzterTeil(t.name)].push_back(n);
            }
            for (std::size_t i = 0; i < e.objekte.size(); ++i)
            {
                const modell::Datenbankobjekt& o = e.objekte[i];
                const bool trigger = o.art == modell::ObjektArt::Trigger;
                const std::string schluessel = trigger ? triggerName(o) : o.name;
                const AufrufKnotenArt art = trigger                              ? AufrufKnotenArt::Trigger
                                            : o.art == modell::ObjektArt::View ? AufrufKnotenArt::View
                                                                                : AufrufKnotenArt::Routine;
                auto& register_ = trigger ? m_trigger : m_objekte;
                if (const auto da = register_.find(klein(schluessel)); da != register_.end())
                {
                    // spaeter neu festgelegt: die spaetere Fassung gilt
                    m_k[da->second].stelle = {Datenstelle::Art::Objekt, d, i};
                    m_k[da->second].zeile = o.zeile;
                    m_k[da->second].definiert[d] = {{Datenstelle::Art::Objekt, d, i}, o.zeile};
                    objekte[m_objektIndex[da->second]] = {d, i};
                    continue;
                }
                const std::size_t n =
                    neu({art, o.name, std::string(stereotyp(o)), {Datenstelle::Art::Objekt, d, i}, o.zeile, schluessel, {}});
                register_[klein(schluessel)] = n;
                if (!trigger) m_objekteTeil[letzterTeil(o.name)].push_back(n);
                m_objektIndex[n] = objekte.size();
                objekte.emplace_back(d, i);
            }
        }
        // 2. Funktionen des Host-Codes mit SQL
        std::vector<std::size_t> funktionen;  // je solche Funktion ihr Knoten
        for (std::size_t d = 0; d < p.dateien.size(); ++d)
        {
            const modell::Einheit& e = p.dateien[d].ergebnis.einheit;
            for (std::size_t i = 0; i < e.funktionen.size(); ++i)
                if (!e.funktionen[i].zugriffe.empty())
                    funktionen.push_back(neu({AufrufKnotenArt::Funktion, e.funktionen[i].vollerName(), {},
                                              {Datenstelle::Art::Funktion, d, i}, e.funktionen[i].zeile,
                                              e.funktionen[i].vollerName(), {}}));
        }
        // 3. die Kanten
        for (const auto& [d, i] : objekte)
        {
            const modell::Datenbankobjekt& o = p.dateien[d].ergebnis.einheit.objekte[i];
            const std::size_t von =
                o.art == modell::ObjektArt::Trigger ? m_trigger.at(klein(triggerName(o))) : m_objekte.at(klein(o.name));
            if (o.art == modell::ObjektArt::Trigger && !o.tabelle.empty())
                verbinde(tabelleOderView(o.tabelle, true), von, ZugriffKante::LoestAus, o.ereignis);
            for (const modell::Zugriff& z : o.zugriffe) zugriff(von, z);
        }
        for (const std::size_t n : funktionen)
        {
            const Datenstelle& s = m_k[n].stelle;
            for (const modell::Zugriff& z : p.dateien[s.datei].ergebnis.einheit.funktionen[s.index].zugriffe) zugriff(n, z);
        }
    }

    [[nodiscard]] const std::vector<Knoten>& knoten() const { return m_k; }
    [[nodiscard]] const std::vector<Kante>& kanten() const { return m_e; }

private:
    std::size_t neu(Knoten k)
    {
        if (k.stelle.datei != kKeine) k.definiert.try_emplace(k.stelle.datei, k.stelle, k.zeile);
        m_k.push_back(std::move(k));
        return m_k.size() - 1;
    }

    /// Ein Name in einem Register: genau so (ohne Gross/klein), sonst ueber den letzten Teil, wenn er eindeutig ist.
    static std::size_t finde(std::string_view name, const std::map<std::string, std::size_t>& genau,
                             const std::map<std::string, std::vector<std::size_t>>& teil)
    {
        if (const auto g = genau.find(klein(name)); g != genau.end()) return g->second;
        if (const auto t = teil.find(letzterTeil(name)); t != teil.end() && t->second.size() == 1) return t->second.front();
        return kKeine;
    }

    /// Die Tabelle (oder View) dieses Namens; fehlt sie und `anlegen`: ein Knoten FremdeTabelle.
    std::size_t tabelleOderView(std::string_view name, bool anlegen)
    {
        if (const std::size_t t = finde(name, m_tabellen, m_tabellenTeil); t != kKeine) return t;
        if (const std::size_t v = finde(name, m_objekte, m_objekteTeil); v != kKeine && m_k[v].art == AufrufKnotenArt::View) return v;
        if (!anlegen) return kKeine;
        if (const auto f = m_fremde.find(klein(name)); f != m_fremde.end()) return f->second;
        const std::size_t n = neu({AufrufKnotenArt::FremdeTabelle, std::string(name), std::string(kTabelle), {}, 0, {}, {}});
        m_fremde[klein(name)] = n;
        return n;
    }

    void zugriff(std::size_t von, const modell::Zugriff& z)
    {
        if (z.art == modell::ZugriffArt::Ruft)
        {
            std::size_t r = finde(z.ziel, m_objekte, m_objekteTeil);
            if (r != kKeine && m_k[r].art != AufrufKnotenArt::Routine) r = z.imAusdruck ? kKeine : r;  // FROM f() ist kein Lesen
            if (r == kKeine)
            {
                // eine Routine, die das Projekt nicht kennt: nur aus CALL/EXEC in klarer Form, wie ein fremder Aufruf
                if (z.imAusdruck || z.unsicher) return;
                const auto f = m_fremdeRoutinen.find(klein(z.ziel));
                r = f != m_fremdeRoutinen.end() ? f->second : neu({AufrufKnotenArt::Fremd, z.ziel, {}, {}, 0, {}, {}});
                m_fremdeRoutinen[klein(z.ziel)] = r;
            }
            verbinde(von, r, ZugriffKante::Ruft, {});
            return;
        }
        // eine Tabelle, die das Projekt nicht kennt, nur aus SQL in klarer Form (Entscheid A4)
        const std::size_t t = tabelleOderView(z.ziel, !z.unsicher && !z.imAusdruck);
        if (t == kKeine) return;
        verbinde(von, t, z.art == modell::ZugriffArt::Liest ? ZugriffKante::Liest : ZugriffKante::Schreibt, {});
    }

    /// Eine Kante; liest und schreibt dasselbe Paar: eine Kante `reads, writes`.
    void verbinde(std::size_t von, std::size_t nach, ZugriffKante art, std::string ereignis)
    {
        if (von == kKeine || nach == kKeine) return;
        const auto schon = m_kanteVon.find({von, nach});
        if (schon == m_kanteVon.end())
        {
            m_kanteVon[{von, nach}] = m_e.size();
            m_e.push_back({von, nach, art, std::move(ereignis)});
            return;
        }
        Kante& k = m_e[schon->second];
        const bool lesend = k.zugriff == ZugriffKante::Liest || art == ZugriffKante::Liest;
        const bool schreibend = k.zugriff == ZugriffKante::Schreibt || art == ZugriffKante::Schreibt;
        if (k.zugriff == ZugriffKante::LiestSchreibt || (lesend && schreibend)) k.zugriff = ZugriffKante::LiestSchreibt;
    }

    std::vector<Knoten> m_k;
    std::vector<Kante> m_e;
    std::map<std::string, std::size_t> m_tabellen, m_objekte, m_trigger, m_fremde, m_fremdeRoutinen;
    std::map<std::string, std::vector<std::size_t>> m_tabellenTeil, m_objekteTeil;
    std::map<std::size_t, std::size_t> m_objektIndex;  // Knoten -> Stelle in `objekte`
    std::map<std::pair<std::size_t, std::size_t>, std::size_t> m_kanteVon;
};

/// Der Ausschnitt `drin` (in dieser Reihenfolge) als Graph. Was die Datei `fokus` anlegt, steht mit ihrer Stelle
/// da (dasselbe Schema in mehreren Dateien); `weiter`: an der Grenze, ginge weiter.
AbhaengigkeitsErgebnis ausschnitt(const Projekt& p, const Gesamt& g, const std::vector<std::size_t>& drin,
                                  std::string titel, std::size_t wurzel, const std::set<std::size_t>& weiter,
                                  std::size_t fokus)
{
    AbhaengigkeitsErgebnis aus;
    aus.graph.titel = std::move(titel);
    std::map<std::size_t, std::size_t> neu;  // Gesamt -> Ausschnitt
    std::map<std::size_t, std::size_t> dateiVon;  // Datei des Projekts -> Stelle in graph.dateien
    for (const std::size_t n : drin)
    {
        const Gesamt::Knoten& k = g.knoten()[n];
        const auto hier = k.definiert.find(fokus);
        const Datenstelle stelle = hier != k.definiert.end() ? hier->second.first : k.stelle;
        diagramm::AufrufKnoten a;
        a.art = k.art;
        a.name = k.name;
        a.stereotyp = k.stereotyp;
        a.zeile = hier != k.definiert.end() ? hier->second.second : k.zeile;
        a.wurzel = n == wurzel;
        a.weiter = weiter.contains(n);
        if (stelle.datei != kKeine)
        {
            const auto [d, eingefuegt] = dateiVon.try_emplace(stelle.datei, aus.graph.dateien.size());
            if (eingefuegt) aus.graph.dateien.push_back(p.dateien[stelle.datei].pfad);
            a.datei = d->second;
        }
        neu[n] = aus.graph.knoten.size();
        aus.graph.knoten.push_back(std::move(a));
        aus.stellen.push_back(stelle);
    }
    for (const Gesamt::Kante& e : g.kanten())
    {
        const auto v = neu.find(e.von);
        const auto z = neu.find(e.nach);
        if (v == neu.end() || z == neu.end()) continue;
        diagramm::AufrufKante k;
        k.von = v->second;
        k.nach = z->second;
        k.zugriff = e.zugriff;
        k.ereignis = e.ereignis;
        aus.graph.kanten.push_back(std::move(k));
    }
    return aus;
}

}  // namespace

std::vector<std::string> datenobjekteIn(const Projekt& projekt, std::size_t datei)
{
    std::vector<std::string> aus;
    if (datei >= projekt.dateien.size()) return aus;
    const modell::Einheit& e = projekt.dateien[datei].ergebnis.einheit;
    for (const modell::Typ& t : e.typen)
        if (t.art == modell::TypArt::Tabelle) aus.push_back(t.name);
    for (const modell::Datenbankobjekt& o : e.objekte)
    {
        const std::string n = o.art == modell::ObjektArt::Trigger ? triggerName(o) : o.name;
        if (std::find(aus.begin(), aus.end(), n) == aus.end()) aus.push_back(n);
    }
    for (const modell::Funktion& f : e.funktionen)
        if (!f.zugriffe.empty()) aus.push_back(f.vollerName());
    return aus;
}

AbhaengigkeitsErgebnis abhaengigkeitenDerDatei(const Projekt& projekt, std::size_t datei)
{
    const Gesamt g(projekt);
    std::vector<std::size_t> drin;
    std::set<std::size_t> gesehen;
    const auto nimm = [&](std::size_t n) {
        if (gesehen.insert(n).second) drin.push_back(n);
    };
    for (std::size_t n = 0; n < g.knoten().size(); ++n)
        if (g.knoten()[n].definiert.contains(datei)) nimm(n);
    const std::vector<std::size_t> eigene = drin;
    for (const Gesamt::Kante& e : g.kanten())
    {
        const bool vonHier = std::find(eigene.begin(), eigene.end(), e.von) != eigene.end();
        const bool nachHier = std::find(eigene.begin(), eigene.end(), e.nach) != eigene.end();
        if (vonHier) nimm(e.nach);
        if (nachHier) nimm(e.von);
    }
    const std::string pfad = datei < projekt.dateien.size() ? projekt.dateien[datei].pfad : std::string();
    return ausschnitt(projekt, g, drin, "Data dependencies of " + pfad, kKeine, {}, datei);
}

AbhaengigkeitsErgebnis abhaengigkeitenAb(const Projekt& projekt, std::size_t datei, std::string_view name,
                                         const AbhaengigkeitsOptionen& optionen)
{
    const Gesamt g(projekt);
    std::size_t start = kKeine;
    for (std::size_t n = 0; n < g.knoten().size() && start == kKeine; ++n)
        if (g.knoten()[n].definiert.contains(datei) && g.knoten()[n].schluessel == name) start = n;
    if (start == kKeine)  // in einer anderen Datei definiert (die spaetere Fassung) — dem Namen nach
        for (std::size_t n = 0; n < g.knoten().size() && start == kKeine; ++n)
            if (klein(g.knoten()[n].schluessel) == klein(name)) start = n;
    if (start == kKeine) return {};

    // in die Breite, Stufe um Stufe; Gerufene folgt den Kanten, Aufrufer gegen sie; Beide: nach unten nur
    // weiter nach unten, nach oben nur weiter nach oben
    std::vector<std::size_t> drin{start};
    std::set<std::size_t> gesehen{start};
    std::set<std::size_t> weiter;
    std::size_t imProjekt = 1;
    bool begrenzt = false;
    // nach unten loest eine Tabelle ihre Trigger nur aus, wo sie beginnt oder geschrieben wird — wer sie
    // liest, loest nichts aus
    struct Offen
    {
        std::size_t knoten;
        int stufe;
        bool loestAus;  ///< ihre Kanten LoestAus zaehlen
    };
    const auto laufe = [&](bool vorwaerts) {
        std::deque<Offen> offen{{start, 0, true}};
        while (!offen.empty())
        {
            const auto [n, stufe, loestAus] = offen.front();
            offen.pop_front();
            for (const Gesamt::Kante& e : g.kanten())
            {
                const std::size_t von = vorwaerts ? e.von : e.nach;
                const std::size_t nach = vorwaerts ? e.nach : e.von;
                if (von != n || gesehen.contains(nach)) continue;
                if (vorwaerts && e.zugriff == ZugriffKante::LoestAus && !loestAus) continue;
                if (optionen.tiefe != 0 && stufe >= optionen.tiefe)
                {
                    weiter.insert(n);
                    continue;
                }
                const bool projektKnoten = g.knoten()[nach].stelle.datei != kKeine;
                if (projektKnoten && optionen.hoechstens != 0 && imProjekt >= optionen.hoechstens)
                {
                    weiter.insert(n);
                    begrenzt = true;
                    continue;
                }
                gesehen.insert(nach);
                drin.push_back(nach);
                if (projektKnoten) ++imProjekt;
                const bool geschrieben = e.zugriff == ZugriffKante::Schreibt || e.zugriff == ZugriffKante::LiestSchreibt;
                offen.push_back({nach, stufe + 1, !vorwaerts || geschrieben});
            }
        }
    };
    if (optionen.richtung != AufrufRichtung::Aufrufer) laufe(true);
    if (optionen.richtung != AufrufRichtung::Gerufene) laufe(false);
    AbhaengigkeitsErgebnis aus =
        ausschnitt(projekt, g, drin, "Data dependencies from " + std::string(name), start, weiter, datei);
    aus.begrenzt = begrenzt;
    return aus;
}

}  // namespace c2d::projekt
