/**
 * @file Abfragediagramm.cpp
 * @brief Von modell::Abfrage zum Abfragediagramm (Entscheide Q2-Q4)
 */
#include "c2d/diagramm/Abfragediagramm.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <tuple>

namespace c2d::diagramm
{

namespace
{

using namespace c2d::modell;

std::string klein(std::string_view text)
{
    std::string s(text);
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string gross(std::string_view text)
{
    std::string s(text);
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string letzterTeil(std::string_view name)
{
    const std::size_t punkt = name.find_last_of('.');
    return std::string(punkt == std::string_view::npos ? name : name.substr(punkt + 1));
}

bool gleich(std::string_view a, std::string_view b)
{
    return klein(a) == klein(b);
}

/// Eine Quelle: Block und Stelle darin.
struct Stelle
{
    std::size_t block = kKeinBlock;
    std::size_t quelle = kKeinBlock;

    [[nodiscard]] bool gueltig() const { return block != kKeinBlock; }
    friend bool operator==(const Stelle&, const Stelle&) = default;
};

class Bau
{
public:
    Bau(const Abfrage& ab, const DatenquellenSuche& suche) : m_ab(ab), m_suche(suche) {}

    Abfragediagramm baue()
    {
        m_d.titel = m_ab.name;
        const std::size_t n = m_ab.bloecke.size();
        m_rahmen.assign(n, kKeine);
        m_ergebnis.assign(n, kKeine);
        m_knoten.assign(n, {});
        for (std::size_t b = 1; b < n; ++b) neuerRahmen(b);
        for (std::size_t b = 0; b < n; ++b) knotenDes(b);
        for (std::size_t b = 0; b < n; ++b) spaltenDes(b);
        for (std::size_t b = 0; b < n; ++b) bedingungenDes(b);
        for (std::size_t b = 0; b < n; ++b) flussDes(b);
        for (std::size_t b = 1; b < n; ++b) unterabfrage(b);
        if (!m_ab.ziel.empty() && m_ergebnis[0] != kKeine)
        {
            AbfrageKnoten z;
            z.art = AbfrageKnotenArt::Ziel;
            z.titel = m_ab.ziel;
            z.name = m_ab.ziel;
            z.zeile = m_ab.zeile;
            const std::size_t k = neu(std::move(z));
            m_d.kanten.push_back({m_ergebnis[0], k, AbfrageKantenArt::Schreibt, m_ab.schreibt});
        }
        return std::move(m_d);
    }

private:
    std::size_t neu(AbfrageKnoten k)
    {
        m_d.knoten.push_back(std::move(k));
        return m_d.knoten.size() - 1;
    }

    void neuerRahmen(std::size_t b)
    {
        const AbfrageBlock& x = m_ab.bloecke[b];
        AbfrageRahmen r;
        switch (x.rolle)
        {
            case BlockRolle::Mit: r.titel = "WITH " + x.titel; break;
            case BlockRolle::Abgeleitet: r.titel = x.titel.empty() ? "derived" : "derived " + x.titel; break;
            case BlockRolle::Bedingung: r.titel = "subquery (" + gross(x.titel) + ")"; break;
            case BlockRolle::Auswahl: r.titel = "subquery (select)"; break;
            case BlockRolle::Zweig: r.titel = x.titel; break;
            case BlockRolle::Haupt: r.titel = "query"; break;
        }
        r.eltern = x.eltern < m_rahmen.size() ? m_rahmen[x.eltern] : kKeine;
        m_rahmen[b] = m_d.rahmen.size();
        m_d.rahmen.push_back(std::move(r));
    }

    /// Ist `name` in Block b ein Name eines WITH (in b oder einem Block um ihn)? Dann sein Block.
    std::size_t mitBlock(std::size_t b, std::string_view name) const
    {
        for (std::size_t e = b; e != kKeinBlock; e = m_ab.bloecke[e].eltern)
            for (std::size_t m = 0; m < m_ab.bloecke.size(); ++m)
                if (m_ab.bloecke[m].eltern == e && m_ab.bloecke[m].rolle == BlockRolle::Mit && gleich(m_ab.bloecke[m].titel, name))
                    return m;
        return kKeinBlock;
    }

    void knotenDes(std::size_t b)
    {
        const AbfrageBlock& x = m_ab.bloecke[b];
        const std::size_t rahmen = m_rahmen[b];
        for (const AbfrageQuelle& q : x.quellen)
        {
            AbfrageKnoten k;
            k.gruppe = rahmen;
            k.zeile = q.zeile;
            const std::string alias = !q.alias.empty() && !gleich(q.alias, letzterTeil(q.name)) ? q.alias : std::string();
            switch (q.art)
            {
                case QuellArt::Abgeleitet:
                    k.art = AbfrageKnotenArt::Abgeleitet;
                    k.titel = q.alias.empty() ? "derived" : q.alias;
                    break;
                case QuellArt::Funktion:
                    k.art = AbfrageKnotenArt::Funktion;
                    k.titel = q.name + "()" + (alias.empty() ? "" : " " + alias);
                    break;
                case QuellArt::Werte:
                    k.art = AbfrageKnotenArt::Werte;
                    k.titel = "VALUES" + (q.alias.empty() ? std::string() : " " + q.alias);
                    break;
                case QuellArt::Tabelle:
                {
                    k.titel = q.name + (alias.empty() ? "" : " " + alias);
                    k.name = q.name;
                    if (mitBlock(b, q.name) != kKeinBlock)
                        k.art = AbfrageKnotenArt::Mit;
                    else if (!m_suche)
                        k.art = AbfrageKnotenArt::Tabelle;
                    else
                    {
                        const Datenquelle d = m_suche(q.name);
                        k.art = d.tabelle != nullptr ? AbfrageKnotenArt::Tabelle
                                : d.view             ? AbfrageKnotenArt::View
                                                     : AbfrageKnotenArt::Fremd;
                    }
                    break;
                }
            }
            if (q.bedingungGanz.logik) k.tabellen.push_back(q.bedingungGanz.zeile);  // ihr ON hat eine Tabelle (W6)
            m_knoten[b].push_back(neu(std::move(k)));
        }
        AbfrageKnoten e;
        e.gruppe = rahmen;
        e.zeile = x.zeile;
        if (!x.zweige.empty())
        {
            e.art = AbfrageKnotenArt::Menge;
            e.titel = x.mengen;
        }
        else
        {
            e.art = AbfrageKnotenArt::Ergebnis;
            e.titel = "result";
        }
        // WHERE vor HAVING: ein Klick auf das Symbol zeigt die erste (W6)
        if (x.woGanz.logik) e.tabellen.push_back(x.woGanz.zeile);
        if (x.habenGanz.logik) e.tabellen.push_back(x.habenGanz.zeile);
        m_ergebnis[b] = neu(std::move(e));
    }

    /// Die Quelle, die ein Qualifizierer meint: in b, sonst in den Bloecken um b (ein Bezug nach aussen).
    Stelle quelleZu(std::size_t b, std::string_view qualifizierer) const
    {
        const std::string q = letzterTeil(qualifizierer);
        for (std::size_t e = b; e != kKeinBlock; e = m_ab.bloecke[e].eltern)
        {
            const std::vector<AbfrageQuelle>& qs = m_ab.bloecke[e].quellen;
            for (std::size_t i = 0; i < qs.size(); ++i)
                if (gleich(qs[i].kennung(), q) || gleich(qs[i].name, qualifizierer) ||
                    (qs[i].alias.empty() && gleich(letzterTeil(qs[i].name), q)))
                    return {e, i};
        }
        return {};
    }

    /// Die Spalten, die eine Quelle hat, soweit bekannt (Tabelle des Schemas, Name eines WITH, Unterabfrage).
    std::vector<std::string> spaltenVon(std::size_t b, const AbfrageQuelle& q) const
    {
        std::vector<std::string> aus;
        if (q.art == QuellArt::Abgeleitet && q.block != kKeinBlock) return m_ab.bloecke[q.block].ausgabe;
        if (q.art != QuellArt::Tabelle) return aus;
        if (const std::size_t m = mitBlock(b, q.name); m != kKeinBlock) return ausgabeVon(m);
        if (!m_suche) return aus;
        if (const Datenquelle d = m_suche(q.name); d.tabelle != nullptr)
            for (const Feld& f : d.tabelle->felder) aus.push_back(f.name);
        return aus;
    }

    /// Die Spalten nach aussen eines Blocks; bei einer Mengenoperation die ihres ersten Zweigs.
    std::vector<std::string> ausgabeVon(std::size_t b) const
    {
        const AbfrageBlock& x = m_ab.bloecke[b];
        if (!x.zweige.empty()) return ausgabeVon(x.zweige.front());
        return x.ausgabe;
    }

    /// Eine Spalte ohne Qualifizierer in b: die einzige Quelle, oder die einzige, die sie hat.
    Stelle ohneQualifizierer(std::size_t b, std::string_view spalte) const
    {
        const std::vector<AbfrageQuelle>& qs = m_ab.bloecke[b].quellen;
        if (qs.size() == 1) return {b, 0};
        Stelle treffer;
        int zahl = 0;
        for (std::size_t i = 0; i < qs.size(); ++i)
        {
            const std::vector<std::string> s = spaltenVon(b, qs[i]);
            if (std::any_of(s.begin(), s.end(), [&](const std::string& x) { return gleich(x, spalte); }))
            {
                treffer = {b, i};
                ++zahl;
            }
        }
        return zahl == 1 ? treffer : Stelle{};
    }

    Stelle stelleVon(std::size_t b, const Spaltenbezug& s) const
    {
        return s.qualifizierer.empty() ? ohneQualifizierer(b, s.spalte) : quelleZu(b, s.qualifizierer);
    }

    void spaltenDes(std::size_t b)
    {
        const AbfrageBlock& x = m_ab.bloecke[b];
        std::vector<std::string> ohne;
        for (const Spaltenbezug& s : x.bezuege)
        {
            const Stelle st = stelleVon(b, s);
            // ein Alias der eigenen Liste (`count(*) n`, ORDER BY summe) — ausser die Quelle hat die Spalte wirklich
            if (s.qualifizierer.empty() &&
                std::any_of(x.aliase.begin(), x.aliase.end(), [&](const std::string& a) { return gleich(a, s.spalte); }))
            {
                const std::vector<std::string> hat =
                    st.gueltig() ? spaltenVon(st.block, m_ab.bloecke[st.block].quellen[st.quelle]) : std::vector<std::string>{};
                if (std::none_of(hat.begin(), hat.end(), [&](const std::string& h) { return gleich(h, s.spalte); })) continue;
            }
            if (!st.gueltig())
            {
                const std::string text = s.qualifizierer.empty() ? s.spalte : s.qualifizierer + "." + s.spalte;
                if (std::find(ohne.begin(), ohne.end(), text) == ohne.end()) ohne.push_back(text);
                continue;
            }
            std::vector<std::string>& zeilen = m_d.knoten[m_knoten[st.block][st.quelle]].zeilen;
            if (std::none_of(zeilen.begin(), zeilen.end(), [&](const std::string& z) { return gleich(z, s.spalte); }))
                zeilen.push_back(s.spalte);
        }
        m_ohne.push_back(std::move(ohne));
    }

    /// Eine Kante zwischen zwei Quellen; gibt es sie schon (gleiche Art, gleiches Paar), kommt der Teil
    /// mit AND dazu.
    void kante(std::size_t von, std::size_t nach, AbfrageKantenArt art, const std::string& vorn, const std::string& teil)
    {
        // ein Teil aus WHERE zwischen zwei Quellen, die schon ein Join verbindet: an dessen Kante (sonst laegen
        // zwei Kanten uebereinander)
        if (art == AbfrageKantenArt::Wo)
            for (AbfrageKante& e : m_d.kanten)
                if (e.art == AbfrageKantenArt::Join && std::min(e.von, e.nach) == std::min(von, nach) &&
                    std::max(e.von, e.nach) == std::max(von, nach))
                {
                    const std::string vornWo = "; where: ";
                    if (e.text.find(vornWo) == std::string::npos)
                        e.text += vornWo + teil;
                    else
                        e.text += " AND " + teil;
                    return;
                }
        const auto schluessel = std::make_tuple(std::min(von, nach), std::max(von, nach), static_cast<int>(art), vorn);
        if (const auto da = m_paare.find(schluessel); da != m_paare.end())
        {
            m_d.kanten[da->second].text += " AND " + teil;
            return;
        }
        m_paare.emplace(schluessel, m_d.kanten.size());
        m_d.kanten.push_back({von, nach, art, vorn.empty() ? teil : vorn + ": " + teil});
    }

    /// Wohin ein Teil einer Bedingung in Block b gehoert: eine Kante zwischen zwei Quellen (true), eine
    /// korrelierte Kante (true), sonst nichts (false: ein Filter).
    bool alsKante(std::size_t b, const Bedingungsteil& t, AbfrageKantenArt art, const std::string& vorn)
    {
        if (t.unterabfrage != kKeinBlock) return false;
        std::set<std::size_t> innen;
        std::set<std::pair<std::size_t, std::size_t>> aussen;
        for (const Spaltenbezug& s : t.bezuege)
        {
            const Stelle st = stelleVon(b, s);
            if (!st.gueltig()) return false;  // eine Spalte, die keiner Quelle sicher gehoert
            if (st.block == b)
                innen.insert(st.quelle);
            else
                aussen.insert({st.block, st.quelle});
        }
        if (innen.size() == 2 && aussen.empty())
        {
            const std::size_t x = m_knoten[b][*innen.begin()];
            const std::size_t y = m_knoten[b][*innen.rbegin()];
            kante(x, y, art, vorn, t.text);
            return true;
        }
        if (innen.size() == 1 && aussen.size() == 1)
        {
            const std::size_t draussen = m_knoten[aussen.begin()->first][aussen.begin()->second];
            kante(draussen, m_knoten[b][*innen.begin()], AbfrageKantenArt::Korreliert, "correlated", t.text);
            return true;
        }
        return false;
    }

    void bedingungenDes(std::size_t b)
    {
        const AbfrageBlock& x = m_ab.bloecke[b];
        AbfrageKnoten* ergebnis = nullptr;
        std::vector<std::string> filter;
        for (std::size_t i = 0; i < x.quellen.size(); ++i)
        {
            const AbfrageQuelle& q = x.quellen[i];
            const std::size_t k = m_knoten[b][i];
            std::string vorn = q.verbindung == "," ? std::string() : q.verbindung;
            const std::size_t vorher = m_d.kanten.size();
            std::vector<std::string> on;
            for (const Bedingungsteil& t : q.bedingung)
                if (!alsKante(b, t, AbfrageKantenArt::Join, vorn)) on.push_back(t.text);
            if (!on.empty())
            {
                std::string z = "on: ";
                for (std::size_t j = 0; j < on.size(); ++j) z += (j > 0 ? " AND " : "") + on[j];
                m_d.knoten[k].zeilen.push_back(z);
            }
            // ein Join ohne Kante aus seiner Bedingung (CROSS, NATURAL, USING, APPLY, ON TRUE): zur Quelle davor
            const bool neueKante = std::any_of(m_d.kanten.begin() + static_cast<std::ptrdiff_t>(vorher), m_d.kanten.end(),
                                               [&](const AbfrageKante& e) { return e.art == AbfrageKantenArt::Join; });
            if (!neueKante && i > 0 && !q.verbindung.empty() && q.verbindung != ",")
            {
                std::string text = q.verbindung;
                if (!q.mit.empty())
                {
                    text += " using (";
                    for (std::size_t j = 0; j < q.mit.size(); ++j) text += (j > 0 ? ", " : "") + q.mit[j];
                    text += ")";
                }
                m_d.kanten.push_back({m_knoten[b][i - 1], k, AbfrageKantenArt::Join, text});
            }
        }
        for (const Bedingungsteil& t : x.wo)
            if (!alsKante(b, t, AbfrageKantenArt::Wo, "where")) filter.push_back(t.text);
        ergebnis = &m_d.knoten[m_ergebnis[b]];
        const auto zeile = [&](std::string_view vorn, const std::string& text) {
            if (!text.empty()) ergebnis->zeilen.push_back(std::string(vorn) + text);
        };
        const auto verbunden = [](const std::vector<Bedingungsteil>& teile) {
            std::string s;
            for (const Bedingungsteil& t : teile) s += (s.empty() ? "" : " AND ") + t.text;
            return s;
        };
        if (x.zweige.empty())
        {
            const bool schreibt = b == 0 && (m_ab.schreibt == "update" || m_ab.schreibt == "delete" || m_ab.schreibt == "merge into");
            if (!x.auswahl.empty()) ergebnis->zeilen.push_back(schreibt ? x.auswahl : "select " + x.auswahl);
            std::string wo;
            for (const std::string& f : filter) wo += (wo.empty() ? "" : " AND ") + f;
            zeile("where ", wo);
            zeile("group by ", x.gruppe);
            zeile("having ", verbunden(x.haben));
            zeile("", x.fenster);
        }
        zeile("order by ", x.ordnung);
        zeile("", x.grenze);
        if (b < m_ohne.size() && !m_ohne[b].empty())
        {
            std::string s = "unassigned: ";
            for (std::size_t j = 0; j < m_ohne[b].size(); ++j) s += (j > 0 ? ", " : "") + m_ohne[b][j];
            ergebnis->zeilen.push_back(s);
        }
    }

    void flussDes(std::size_t b)
    {
        for (const std::size_t k : m_knoten[b])
        {
            const bool weiter = std::any_of(m_d.kanten.begin(), m_d.kanten.end(), [&](const AbfrageKante& e) {
                return e.von == k && (e.art == AbfrageKantenArt::Join || e.art == AbfrageKantenArt::Wo);
            });
            if (!weiter) m_d.kanten.push_back({k, m_ergebnis[b], AbfrageKantenArt::Fluss, {}});
        }
    }

    void unterabfrage(std::size_t b)
    {
        const AbfrageBlock& x = m_ab.bloecke[b];
        const std::size_t von = m_ergebnis[b];
        switch (x.rolle)
        {
            case BlockRolle::Mit:
                // in jede Quelle, die diesen Namen nennt (im Block um ihn und tiefer)
                for (std::size_t c = 0; c < m_ab.bloecke.size(); ++c)
                    for (std::size_t i = 0; i < m_ab.bloecke[c].quellen.size(); ++i)
                    {
                        const AbfrageQuelle& q = m_ab.bloecke[c].quellen[i];
                        if (q.art == QuellArt::Tabelle && mitBlock(c, q.name) == b)
                            m_d.kanten.push_back({von, m_knoten[c][i], AbfrageKantenArt::Fluss, {}});
                    }
                break;
            case BlockRolle::Abgeleitet:
                for (std::size_t i = 0; i < m_ab.bloecke[x.eltern].quellen.size(); ++i)
                    if (m_ab.bloecke[x.eltern].quellen[i].block == b)
                        m_d.kanten.push_back({von, m_knoten[x.eltern][i], AbfrageKantenArt::Fluss, {}});
                break;
            case BlockRolle::Bedingung:
                m_d.kanten.push_back({von, m_ergebnis[x.eltern], AbfrageKantenArt::Fluss, x.titel});
                break;
            case BlockRolle::Auswahl:
                m_d.kanten.push_back({von, m_ergebnis[x.eltern], AbfrageKantenArt::Fluss, "select"});
                break;
            case BlockRolle::Zweig:
                m_d.kanten.push_back({von, m_ergebnis[x.eltern], AbfrageKantenArt::Fluss, {}});
                break;
            case BlockRolle::Haupt: break;
        }
    }

    const Abfrage& m_ab;
    const DatenquellenSuche& m_suche;
    Abfragediagramm m_d;
    std::vector<std::size_t> m_rahmen;                 ///< je Block sein Rahmen
    std::vector<std::size_t> m_ergebnis;               ///< je Block sein Ergebnis (oder seine Mengenoperation)
    std::vector<std::vector<std::size_t>> m_knoten;    ///< je Block je Quelle ihr Knoten
    std::vector<std::vector<std::string>> m_ohne;      ///< je Block die Spalten, die keiner Quelle sicher gehoeren
    std::map<std::tuple<std::size_t, std::size_t, int, std::string>, std::size_t> m_paare;
};

}  // namespace

Abfragediagramm abfrageVon(const Abfrage& abfrage, const DatenquellenSuche& suche)
{
    if (abfrage.bloecke.empty())
    {
        Abfragediagramm d;
        d.titel = abfrage.name;
        return d;
    }
    return Bau(abfrage, suche).baue();
}

NullFaehig nullFaehigkeit(const Abfrage& abfrage, std::size_t block, const Spaltenbezug& s, const DatenquellenSuche& suche,
                          std::size_t imOn)
{
    if (block >= abfrage.bloecke.size()) return NullFaehig::Unbekannt;
    // die Quelle: zuerst im Block, dann in den Bloecken um ihn (ein Bezug nach aussen)
    std::size_t b = kKeinBlock;
    std::size_t q = kKeine;
    const std::string qual = letzterTeil(s.qualifizierer);
    for (std::size_t e = block; e != kKeinBlock && q == kKeine; e = abfrage.bloecke[e].eltern)
    {
        const std::vector<AbfrageQuelle>& qs = abfrage.bloecke[e].quellen;
        if (s.qualifizierer.empty())
        {
            // ohne Qualifizierer: die einzige Quelle, oder die einzige Tabelle des Schemas mit dieser Spalte
            if (qs.size() == 1)
                q = 0;
            else if (suche)
            {
                std::size_t zahl = 0;
                for (std::size_t i = 0; i < qs.size(); ++i)
                    if (const Datenquelle d = suche(qs[i].name); d.tabelle != nullptr)
                        for (const Feld& f : d.tabelle->felder)
                            if (gleich(f.name, s.spalte))
                            {
                                q = i;
                                ++zahl;
                            }
                if (zahl != 1) q = kKeine;
            }
        }
        else
            for (std::size_t i = 0; i < qs.size() && q == kKeine; ++i)
                if (gleich(qs[i].kennung(), qual) || gleich(qs[i].name, s.qualifizierer) ||
                    (qs[i].alias.empty() && gleich(letzterTeil(qs[i].name), qual)))
                    q = i;
        if (q != kKeine) b = e;
        if (s.qualifizierer.empty()) break;  // ohne Qualifizierer nur der eigene Block
    }
    if (q == kKeine || !suche) return NullFaehig::Unbekannt;
    const std::vector<AbfrageQuelle>& qs = abfrage.bloecke[b].quellen;
    if (qs[q].art != QuellArt::Tabelle) return NullFaehig::Unbekannt;
    for (const AbfrageBlock& m : abfrage.bloecke)
        if (m.rolle == BlockRolle::Mit && gleich(m.titel, qs[q].name)) return NullFaehig::Unbekannt;  // ein WITH
    const Datenquelle d = suche(qs[q].name);
    if (d.tabelle == nullptr) return NullFaehig::Unbekannt;
    const auto feld = std::find_if(d.tabelle->felder.begin(), d.tabelle->felder.end(),
                                   [&](const Feld& f) { return gleich(f.name, s.spalte); });
    if (feld == d.tabelle->felder.end()) return NullFaehig::Unbekannt;
    if (!feld->nichtNull) return NullFaehig::Kann;
    // die aufgefuellte Seite eines aeusseren Joins kann NULL sein — im WHERE nach allen Joins, im ON der Quelle j nach
    // den Joins vor j (die Quelle j selbst ist dort noch nicht aufgefuellt)
    const auto rechtsAussen = [](const std::string& v) {  // die Quelle selbst wird aufgefuellt
        return v.starts_with("left") || v.starts_with("full") || v == "outer apply" || v.starts_with("natural left") ||
               v.starts_with("natural full");
    };
    const auto linksAussen = [](const std::string& v) {  // die Quellen davor werden aufgefuellt
        return v.starts_with("right") || v.starts_with("full") || v.starts_with("natural right") || v.starts_with("natural full");
    };
    const std::size_t grenze = b == block && imOn != kKeine ? imOn : qs.size();  // Joins [1, grenze) gelten
    bool aufgefuellt = q >= 1 && q < grenze && rechtsAussen(qs[q].verbindung);
    for (std::size_t j = q + 1; j < grenze && !aufgefuellt; ++j) aufgefuellt = linksAussen(qs[j].verbindung);
    return aufgefuellt ? NullFaehig::Kann : NullFaehig::Nie;
}

namespace
{

/// Ob ein Atom unbekannt sein kann: seine Form, seine Variablen, jede seiner Spalten (W3).
NullFaehig nullFaehigAtom(const Logik& atom, const Abfrage& abfrage, std::size_t block, const DatenquellenSuche& suche,
                          std::size_t imOn)
{
    if (atom.zweiwertig) return NullFaehig::Nie;
    if (!atom.variablen.empty()) return NullFaehig::Unbekannt;
    NullFaehig aus = NullFaehig::Nie;
    for (const Spaltenbezug& s : atom.spalten)
    {
        const NullFaehig n = nullFaehigkeit(abfrage, block, s, suche, imOn);
        if (n == NullFaehig::Kann && aus == NullFaehig::Nie) aus = NullFaehig::Kann;
        if (n == NullFaehig::Unbekannt) aus = NullFaehig::Unbekannt;
    }
    return aus;
}

}  // namespace

std::vector<Wahrheitstabelle> wahrheitstabellenVon(const Abfrage& abfrage, const DatenquellenSuche& suche)
{
    std::vector<Wahrheitstabelle> aus;
    for (std::size_t b = 0; b < abfrage.bloecke.size(); ++b)
    {
        const AbfrageBlock& x = abfrage.bloecke[b];
        const auto tabelle = [&](const Klausel& k, std::string_view klausel, std::size_t imOn) {
            if (!k.logik) return;
            aus.push_back(wahrheitstabelleVonSql(
                *k.logik, k.text, abfrage.name, klausel, k.zeile,
                [&](const Logik& atom) { return nullFaehigAtom(atom, abfrage, b, suche, imOn); },
                klausel == "ON" ? "Joined" : "Row kept"));
        };
        for (std::size_t q = 0; q < x.quellen.size(); ++q) tabelle(x.quellen[q].bedingungGanz, "ON", q);
        tabelle(x.woGanz, "WHERE", kKeine);
        tabelle(x.habenGanz, "HAVING", kKeine);
    }
    return aus;
}

std::vector<int> zeilenMitWahrheitstabelle(const Abfrage& abfrage)
{
    std::vector<int> aus;
    for (const AbfrageBlock& x : abfrage.bloecke)
    {
        for (const AbfrageQuelle& q : x.quellen)
            if (q.bedingungGanz.logik) aus.push_back(q.bedingungGanz.zeile);
        if (x.woGanz.logik) aus.push_back(x.woGanz.zeile);
        if (x.habenGanz.logik) aus.push_back(x.habenGanz.zeile);
    }
    return aus;
}

std::size_t abfrageZurZeile(const std::vector<Abfrage>& abfragen, int zeile)
{
    std::size_t beste = kKeine;
    for (std::size_t i = 0; i < abfragen.size(); ++i)
    {
        const Abfrage& a = abfragen[i];
        if (a.zeile == zeile) return i;
        if (a.zeile <= zeile && zeile <= a.bisZeile &&
            (beste == kKeine || a.bisZeile - a.zeile < abfragen[beste].bisZeile - abfragen[beste].zeile))
            beste = i;
    }
    return beste;
}

}  // namespace c2d::diagramm
