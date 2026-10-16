/**
 * @file Aufrufgraph.cpp
 * @brief Der Aufrufgraph eines Projekts: Aufrufe aufloesen, Knoten und Kanten sammeln
 */
#include "c2d/projekt/Aufrufgraph.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace c2d::projekt
{

namespace
{

using diagramm::AufrufKante;
using diagramm::AufrufKantenArt;
using diagramm::AufrufKnoten;
using diagramm::AufrufKnotenArt;

/// Was eine Funktion unter einem Namen ruft: die Funktionen, die er meinen kann
/// (leer = keine im Projekt), und jede Stelle mit diesem Namen.
struct Ruf
{
    std::string name;
    std::vector<Fundstelle> ziele;
    std::vector<modell::Aufrufstelle> stellen;
};

std::string verbunden(const std::vector<std::string>& teile)
{
    std::string aus;
    for (const std::string& t : teile) aus += (aus.empty() ? "" : " || ") + t;
    return aus;
}

class Bau
{
public:
    Bau(const Projekt& projekt, const AufrufgraphOptionen& optionen) : m_p(projekt), m_o(optionen) {}

    /// Die Rufe einer Funktion, je Name einer, in der Reihenfolge des Rumpfs.
    const std::vector<Ruf>& rufe(const Fundstelle& f)
    {
        const auto schluessel = std::make_pair(f.datei, f.funktion);
        if (const auto da = m_rufe.find(schluessel); da != m_rufe.end()) return da->second;
        std::vector<Ruf> aus;
        for (modell::Aufrufstelle& s : modell::aufrufstellenVon(funktion(m_p, f)))
        {
            auto ruf = std::find_if(aus.begin(), aus.end(), [&](const Ruf& r) { return r.name == s.name; });
            if (ruf == aus.end())
            {
                aus.push_back({s.name, kandidaten(m_p, f.datei, s.name), {}});
                ruf = aus.end() - 1;
            }
            ruf->stellen.push_back(std::move(s));
        }
        return m_rufe.emplace(schluessel, std::move(aus)).first->second;
    }

    /// Ruft die Funktion noch etwas, das der Graph zeigen wuerde?
    bool ruftWeiter(const Fundstelle& f)
    {
        const std::vector<Ruf>& r = rufe(f);
        return std::any_of(r.begin(), r.end(), [&](const Ruf& ruf) { return !ruf.ziele.empty() || !m_o.ohneFremde; });
    }

    std::size_t knotenFuer(const Fundstelle& f)
    {
        const auto schluessel = std::make_pair(f.datei, f.funktion);
        if (const auto da = m_knoten.find(schluessel); da != m_knoten.end()) return da->second;
        const modell::Funktion& fn = funktion(m_p, f);
        const std::string& pfad = m_p.dateien[f.datei].pfad;
        auto datei = std::find(m_e.graph.dateien.begin(), m_e.graph.dateien.end(), pfad);
        if (datei == m_e.graph.dateien.end()) datei = m_e.graph.dateien.insert(m_e.graph.dateien.end(), pfad);
        AufrufKnoten k;
        k.name = fn.vollerName();
        k.datei = static_cast<std::size_t>(datei - m_e.graph.dateien.begin());
        k.zeile = fn.zeile;
        return neuerKnoten(std::move(k), f, schluessel);
    }

    /// Der Knoten eines Rufs ohne Funktion im Projekt — kKeine, wenn Fremde wegbleiben.
    std::size_t fremdFuer(const std::string& name)
    {
        if (m_o.ohneFremde)
        {
            m_ausgelassen.insert(name);
            return diagramm::kKeine;
        }
        if (const auto da = m_fremde.find(name); da != m_fremde.end()) return da->second;
        AufrufKnoten k;
        k.art = istMakro(name) ? AufrufKnotenArt::Makro : AufrufKnotenArt::Fremd;
        k.name = name;
        const std::size_t id = m_e.graph.knoten.size();
        m_e.graph.knoten.push_back(std::move(k));
        m_e.stellen.emplace_back();
        m_fremde.emplace(name, id);
        return id;
    }

    /// Eine Kante fuer die Stellen eines Rufs; schon da, kommen sie dazu.
    void verbinde(std::size_t von, std::size_t nach, bool sicher, const std::vector<modell::Aufrufstelle>& stellen)
    {
        const auto schluessel = std::make_pair(von, nach);
        auto da = m_kanten.find(schluessel);
        if (da == m_kanten.end())
        {
            da = m_kanten.emplace(schluessel, m_e.graph.kanten.size()).first;
            m_e.graph.kanten.push_back({von, nach, AufrufKantenArt::Mehrdeutig, {}, true});
            m_wann.emplace_back();
        }
        Wann& w = m_wann[da->second];
        w.sicher = w.sicher || sicher;
        for (const modell::Aufrufstelle& s : stellen)
        {
            if (s.gilt && s.bedingung.empty()) w.immer = true;
            std::vector<std::string>& liste = s.gilt ? w.geltend : w.nicht;
            if (!s.bedingung.empty() && std::find(liste.begin(), liste.end(), s.bedingung) == liste.end())
                liste.push_back(s.bedingung);
        }
    }

    /// Ein Ruf, der mehrere Funktionen meinen kann: eine Meldung, je Funktion und Name einmal.
    void meldeMehrdeutig(const Fundstelle& von, const Ruf& ruf)
    {
        if (!m_gemeldet.insert({von.datei, von.funktion, ruf.name}).second) return;
        std::string wo;
        for (const Fundstelle& z : ruf.ziele)
            wo += (wo.empty() ? "" : ", ") + m_p.dateien[z.datei].pfad + ":" + std::to_string(funktion(m_p, z).zeile);
        quelle::Meldung m;
        m.schwere = quelle::Schwere::Hinweis;
        m.ort.zeile = ruf.stellen.empty() ? 0 : ruf.stellen.front().zeile;
        m.text = "call graph: '" + ruf.name + "' in " + funktion(m_p, von).vollerName() + "() is ambiguous - it may mean " + wo;
        m_e.meldungen.push_back({m_p.dateien[von.datei].pfad, std::move(m)});
    }

    /// Ab `wurzel` nach unten: was sie ruft, Stufe fuer Stufe.
    void nachUnten(const Fundstelle& wurzel)
    {
        std::deque<std::pair<Fundstelle, int>> offen{{wurzel, 0}};
        std::set<std::pair<std::size_t, std::size_t>> fertig;
        while (!offen.empty())
        {
            const auto [f, stufe] = offen.front();
            offen.pop_front();
            if (!fertig.insert({f.datei, f.funktion}).second) continue;
            const std::size_t kf = knotenFuer(f);
            if (anDerGrenze(f, stufe))
            {
                if (ruftWeiter(f)) m_e.graph.knoten[kf].weiter = true;
                continue;
            }
            for (const Fundstelle& z : rufeAus(f)) offen.emplace_back(z, stufe + 1);
        }
    }

    /// Ab `wurzel` nach oben: wer sie ruft, Stufe fuer Stufe.
    void nachOben(const Fundstelle& wurzel)
    {
        std::deque<std::pair<Fundstelle, int>> offen{{wurzel, 0}};
        std::set<std::pair<std::size_t, std::size_t>> fertig;
        while (!offen.empty())
        {
            const auto [f, stufe] = offen.front();
            offen.pop_front();
            if (!fertig.insert({f.datei, f.funktion}).second) continue;
            const std::size_t kf = knotenFuer(f);
            const std::vector<Fundstelle> wer = aufrufer(m_p, f);
            if (anDerGrenze(f, stufe))
            {
                if (!wer.empty()) m_e.graph.knoten[kf].weiter = true;
                continue;
            }
            for (const Fundstelle& c : wer)
            {
                const std::size_t kc = knotenFuer(c);
                for (const Ruf& ruf : rufe(c))
                {
                    // std::find_if: std::find ueber eine Struktur mit `operator== = default` scheitert in MSVC-STL 14.51
                    if (std::none_of(ruf.ziele.begin(), ruf.ziele.end(), [&](const Fundstelle& z) { return z == f; })) continue;
                    if (ruf.ziele.size() > 1) meldeMehrdeutig(c, ruf);
                    verbinde(kc, kf, ruf.ziele.size() == 1, ruf.stellen);
                }
                offen.emplace_back(c, stufe + 1);
            }
        }
    }

    /// Die Funktionen einer Datei und was sie rufen; was in eine andere Datei fuehrt, endet
    /// dort — ausser es ist aufgeklappt.
    void datei(std::size_t d)
    {
        const std::vector<modell::Funktion>& fs = m_p.dateien[d].ergebnis.einheit.funktionen;
        std::deque<Fundstelle> offen;
        for (std::size_t f = 0; f < fs.size(); ++f)
            if (fs[f].rumpf)
            {
                knotenFuer({d, f});
                offen.push_back({d, f});
            }
        std::set<std::pair<std::size_t, std::size_t>> fertig;
        while (!offen.empty())
        {
            const Fundstelle f = offen.front();
            offen.pop_front();
            if (!fertig.insert({f.datei, f.funktion}).second) continue;
            for (const Fundstelle& z : rufeAus(f))
                if (z.datei != d && istAufgeklappt(z)) offen.push_back(z);
        }
        // was nicht ausgerufen wurde und weiter ruft, ist markiert
        for (std::size_t k = 0; k < m_e.stellen.size(); ++k)
        {
            const Fundstelle& s = m_e.stellen[k];
            if (s.datei != kKeine && !fertig.contains({s.datei, s.funktion}) && ruftWeiter(s)) m_e.graph.knoten[k].weiter = true;
        }
    }

    AufrufgraphErgebnis ergebnis(std::string titel)
    {
        for (std::size_t e = 0; e < m_wann.size(); ++e)
        {
            AufrufKante& k = m_e.graph.kanten[e];
            const Wann& w = m_wann[e];
            k.art = w.sicher ? AufrufKantenArt::Sicher : AufrufKantenArt::Mehrdeutig;
            // irgendwo ohne Bedingung: immer; sonst die Bedingungen, die gelten; sonst die, die nicht gelten
            k.gilt = w.immer || !w.geltend.empty();
            k.bedingung = w.immer ? std::string() : verbunden(k.gilt ? w.geltend : w.nicht);
        }
        m_e.graph.titel = std::move(titel);
        m_e.ausgelassen = m_ausgelassen.size();
        return std::move(m_e);
    }

    std::size_t wurzel(const Fundstelle& f)
    {
        const std::size_t k = knotenFuer(f);
        m_e.graph.knoten[k].wurzel = true;
        return k;
    }

private:
    /// Unter welchen Bedingungen eine Kante gerufen wird, bis alle Stellen da sind.
    struct Wann
    {
        bool sicher = false;
        bool immer = false;
        std::vector<std::string> geltend;
        std::vector<std::string> nicht;
    };

    std::size_t neuerKnoten(AufrufKnoten k, const Fundstelle& f, std::pair<std::size_t, std::size_t> schluessel)
    {
        const std::size_t id = m_e.graph.knoten.size();
        m_e.graph.knoten.push_back(std::move(k));
        m_e.stellen.push_back(f);
        m_knoten.emplace(schluessel, id);
        return id;
    }

    bool istAufgeklappt(const Fundstelle& f) const
    {
        return std::any_of(m_o.aufgeklappt.begin(), m_o.aufgeklappt.end(), [&](const Fundstelle& a) { return a == f; });
    }

    /// Endet der Graph bei dieser Funktion? An der Grenze der Tiefe, ausser sie ist aufgeklappt.
    bool anDerGrenze(const Fundstelle& f, int stufe) const
    {
        return m_o.tiefe != 0 && stufe >= m_o.tiefe && !istAufgeklappt(f);
    }

    /// Was die Funktion ruft, als Kanten; zurueck kommen die gerufenen Funktionen des Projekts.
    std::vector<Fundstelle> rufeAus(const Fundstelle& f)
    {
        std::vector<Fundstelle> aus;
        const std::size_t kf = knotenFuer(f);
        for (const Ruf& ruf : rufe(f))
        {
            if (ruf.ziele.empty())
            {
                if (const std::size_t kz = fremdFuer(ruf.name); kz != diagramm::kKeine) verbinde(kf, kz, true, ruf.stellen);
                continue;
            }
            if (ruf.ziele.size() > 1) meldeMehrdeutig(f, ruf);
            for (const Fundstelle& z : ruf.ziele)
            {
                verbinde(kf, knotenFuer(z), ruf.ziele.size() == 1, ruf.stellen);
                aus.push_back(z);
            }
        }
        return aus;
    }

    /// Definiert eine Datei des Projekts ein Makro dieses Namens mit Parametern?
    bool istMakro(const std::string& name)
    {
        if (!m_makrosGesammelt)
        {
            for (const Datei& d : m_p.dateien)
                for (const modell::Makro& m : d.ergebnis.einheit.makros)
                    if (m.text.starts_with('(')) m_makros.insert(m.name);
            m_makrosGesammelt = true;
        }
        return m_makros.contains(name);
    }

    const Projekt& m_p;
    const AufrufgraphOptionen& m_o;
    AufrufgraphErgebnis m_e;
    std::map<std::pair<std::size_t, std::size_t>, std::vector<Ruf>> m_rufe;
    std::map<std::pair<std::size_t, std::size_t>, std::size_t> m_knoten;
    std::map<std::string, std::size_t> m_fremde;
    std::map<std::pair<std::size_t, std::size_t>, std::size_t> m_kanten;
    std::vector<Wann> m_wann;  // je Kante
    std::set<std::tuple<std::size_t, std::size_t, std::string>> m_gemeldet;
    std::set<std::string> m_ausgelassen;
    std::set<std::string> m_makros;
    bool m_makrosGesammelt = false;
};

bool istFunktion(const Projekt& projekt, const Fundstelle& f)
{
    return f.datei < projekt.dateien.size() && f.funktion < projekt.dateien[f.datei].ergebnis.einheit.funktionen.size() &&
           funktion(projekt, f).rumpf != nullptr;
}

}  // namespace

AufrufgraphErgebnis aufrufgraphAb(const Projekt& projekt, const Fundstelle& wurzel, const AufrufgraphOptionen& optionen)
{
    if (!istFunktion(projekt, wurzel)) return {};
    Bau bau(projekt, optionen);
    bau.wurzel(wurzel);
    if (optionen.richtung != AufrufRichtung::Aufrufer) bau.nachUnten(wurzel);
    if (optionen.richtung != AufrufRichtung::Gerufene) bau.nachOben(wurzel);
    const std::string name = funktion(projekt, wurzel).vollerName();
    switch (optionen.richtung)
    {
        case AufrufRichtung::Gerufene: return bau.ergebnis("calls from " + name + "()");
        case AufrufRichtung::Aufrufer: return bau.ergebnis("callers of " + name + "()");
        case AufrufRichtung::Beide: break;
    }
    return bau.ergebnis("calls from and callers of " + name + "()");
}

AufrufgraphErgebnis aufrufgraphDerDatei(const Projekt& projekt, std::size_t datei, const AufrufgraphOptionen& optionen)
{
    if (datei >= projekt.dateien.size()) return {};
    Bau bau(projekt, optionen);
    bau.datei(datei);
    return bau.ergebnis("calls in " + projekt.dateien[datei].pfad);
}

}  // namespace c2d::projekt
