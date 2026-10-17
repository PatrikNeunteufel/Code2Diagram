/**
 * @file Entscheidungstabelle.cpp
 * @brief Die Wege durch eine Funktion aufzaehlen: verzweigen, Aktionen sammeln, Widersprueche auslassen
 */
#include "c2d/diagramm/Entscheidungstabelle.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <utility>

namespace c2d::diagramm
{

namespace
{

using modell::Anweisung;
using modell::AnweisungArt;

/// Ein Weg, so weit er gebaut ist.
struct Weg
{
    std::vector<std::pair<std::size_t, std::string>> belegung;  // Bedingung -> Eintrag
    std::vector<std::size_t> aktionen;
    bool endet = false;  // return, throw, goto: nichts folgt mehr
    bool raus = false;   // break: bis zum Ende des switch folgt nichts mehr
    std::vector<std::size_t> vergessen;  // Bedingungen, deren Wert sich seither aendern konnte
};

/// Eine Bedingung, wie sie an einer Stelle gefragt wird.
struct Frage
{
    std::string text;
    int zeile = 0;
    bool fall = false;
    bool mitAufruf = false;
    std::vector<std::string> namen;  // die Namen, die sie liest
};

/// Darueber bricht der Bau ab: so viele Regeln zeigt ohnehin keine Tabelle.
constexpr std::size_t kHoechstensWege = 4096;

class Bauer
{
public:
    explicit Bauer(Entscheidungstabelle& t) : m_t(t) {}

    std::vector<Weg> folge(const std::vector<const Anweisung*>& anweisungen, std::vector<Weg> wege)
    {
        for (const Anweisung* a : anweisungen)
        {
            wege = schritt(*a, std::move(wege));
            if (m_zuViele) return {};
        }
        return wege;
    }

    std::vector<Weg> schritt(const Anweisung& a, std::vector<Weg> wege)
    {
        // nur, wer noch geht, geht weiter
        std::vector<Weg> aus;
        std::vector<Weg> gehen;
        for (Weg& w : wege) (w.endet || w.raus ? aus : gehen).push_back(std::move(w));
        if (gehen.empty()) return aus;
        for (Weg& w : weiter(a, std::move(gehen))) aus.push_back(std::move(w));
        if (aus.size() > kHoechstensWege) m_zuViele = true;
        return aus;
    }

    [[nodiscard]] bool zuViele() const { return m_zuViele; }

    /// Wie eine Regel einsortiert wird: je Bedingung (in ihrer Reihenfolge) der Rang ihres Eintrags —
    /// Y vor N, Faelle wie in der Quelle, nicht gefragt zuletzt. So stehen die Regeln wie ein Baum.
    [[nodiscard]] std::vector<std::size_t> rang(const Weg& w) const
    {
        std::vector<std::size_t> r;
        for (std::size_t b = 0; b < m_t.bedingungen.size(); ++b)
        {
            static const std::vector<std::string> kKeine;
            const auto stelle = m_reihenfolge.find(b);
            const std::vector<std::string>& werte = stelle == m_reihenfolge.end() ? kKeine : stelle->second;
            const std::optional<std::string> e = wert(w, b);
            r.push_back(e ? static_cast<std::size_t>(std::find(werte.begin(), werte.end(), *e) - werte.begin()) : werte.size());
        }
        return r;
    }
    [[nodiscard]] std::size_t unmoeglich() const { return m_unmoeglich; }

    void hinweis(const std::string& text)
    {
        if (std::find(m_t.hinweise.begin(), m_t.hinweise.end(), text) == m_t.hinweise.end()) m_t.hinweise.push_back(text);
    }

private:
    static std::vector<const Anweisung*> zeiger(const std::vector<modell::AnweisungPtr>& kinder)
    {
        std::vector<const Anweisung*> aus;
        for (const modell::AnweisungPtr& k : kinder) aus.push_back(k.get());
        return aus;
    }

    static Frage frage(const std::string& text, const Anweisung& a, bool fall)
    {
        return {text, a.zeile, fall, a.bedingungRuft, a.bedingungNamen};
    }

    /// Die Zeile einer Bedingung: die erste mit ihrem Text — neu gefragt eine eigene je Stelle.
    std::size_t zeileFuer(const Frage& f, bool erneut)
    {
        for (std::size_t i = 0; i < m_t.bedingungen.size(); ++i)
        {
            const Entscheidungstabelle::Bedingung& b = m_t.bedingungen[i];
            if (b.text == f.text && b.fall == f.fall && b.erneut == erneut && (!erneut || b.quellZeile == f.zeile)) return i;
        }
        m_t.bedingungen.push_back({f.text, f.zeile, f.fall, f.mitAufruf, erneut});
        m_namen.push_back(f.namen);
        return m_t.bedingungen.size() - 1;
    }

    /// Was die Anweisung aendern kann, macht die Bedingungen auf dem Weg, die es lesen, wieder offen.
    void vergiss(std::vector<Weg>& wege, const Anweisung& a, bool alles = false)
    {
        alles = alles || a.ruft || a.schreibtDurchZeiger;
        for (Weg& w : wege)
            for (const auto& [b, e] : w.belegung)
            {
                if (std::find(w.vergessen.begin(), w.vergessen.end(), b) != w.vergessen.end()) continue;
                const std::vector<std::string>& liest = m_namen[b];
                const bool betroffen = alles || std::any_of(liest.begin(), liest.end(), [&](const std::string& n) {
                                           return std::find(a.schreibt.begin(), a.schreibt.end(), n) != a.schreibt.end();
                                       });
                if (betroffen) w.vergessen.push_back(b);
            }
    }

    void aktion(std::vector<Weg>& wege, const std::string& text, int zeile)
    {
        const auto [stelle, neu] = m_aktionen.emplace(std::pair{text, zeile}, m_t.aktionen.size());
        if (neu) m_t.aktionen.push_back({text, zeile});
        for (Weg& w : wege) w.aktionen.push_back(stelle->second);
    }

    static std::optional<std::string> wert(const Weg& w, std::size_t bedingung)
    {
        for (const auto& [b, e] : w.belegung)
            if (b == bedingung) return e;
        return std::nullopt;
    }

    /// Jeder Weg einmal je Eintrag — es sei denn, die Bedingung ist auf ihm schon entschieden (gleicher
    /// Text, seither nichts geaendert, sie ruft nichts): dann geht er nur dorthin; die anderen Wege gaebe
    /// es nicht. Wurde sie entschieden und seither vergessen, fragt der Weg sie in einer eigenen Zeile neu.
    std::vector<std::vector<Weg>> verzweige(std::vector<Weg> wege, const Frage& f, const std::vector<std::string>& eintraege)
    {
        std::vector<std::vector<Weg>> aus(eintraege.size());
        for (Weg& w : wege)
        {
            bool gefragt = false;
            std::optional<std::string> bekannt;
            for (const auto& [b, e] : w.belegung)
            {
                const Entscheidungstabelle::Bedingung& bed = m_t.bedingungen[b];
                if (bed.text != f.text || bed.fall != f.fall) continue;
                gefragt = true;
                if (!f.mitAufruf && std::find(w.vergessen.begin(), w.vergessen.end(), b) == w.vergessen.end()) bekannt = e;
            }
            if (bekannt)
            {
                const auto stelle = std::find(eintraege.begin(), eintraege.end(), *bekannt);
                if (stelle != eintraege.end()) aus[static_cast<std::size_t>(stelle - eintraege.begin())].push_back(w);
                m_unmoeglich += eintraege.size() - 1;
                continue;
            }
            const std::size_t b = zeileFuer(f, gefragt);
            m_reihenfolge.emplace(b, eintraege);
            for (std::size_t e = 0; e < eintraege.size(); ++e)
            {
                Weg neu = w;
                neu.belegung.emplace_back(b, eintraege[e]);
                aus[e].push_back(std::move(neu));
            }
        }
        return aus;
    }

    std::vector<Weg> weiter(const Anweisung& a, std::vector<Weg> wege)
    {
        switch (a.art)
        {
            case AnweisungArt::Einfach:
            case AnweisungArt::Ruecksprung:
            case AnweisungArt::Wurf:
            {
                std::string text = a.text;
                if (a.art == AnweisungArt::Ruecksprung) text = a.text.empty() ? "return" : "return " + a.text;
                if (a.art == AnweisungArt::Wurf) text = a.text.empty() ? "throw" : "throw " + a.text;
                if (!a.frage.empty())
                {
                    // ?: verzweigt — beide Seiten fuehren dieselbe Anweisung aus
                    auto [ja, nein] = zwei(verzweige(std::move(wege), frage(a.frage, a, false), {"Y", "N"}));
                    wege = std::move(ja);
                    for (Weg& w : nein) wege.push_back(std::move(w));
                }
                aktion(wege, text, a.zeile);
                vergiss(wege, a);
                if (a.art != AnweisungArt::Einfach)
                    for (Weg& w : wege) w.endet = true;
                return wege;
            }
            case AnweisungArt::Block:
            case AnweisungArt::Zweig:
            case AnweisungArt::Bedingt: return folge(zeiger(a.kinder), std::move(wege));
            case AnweisungArt::Wenn:
            {
                auto [ja, nein] = zwei(verzweige(std::move(wege), frage(a.text, a, false), {"Y", "N"}));
                ja = schritt(*a.kinder[0], std::move(ja));
                if (a.sonst) nein = schritt(*a.sonst, std::move(nein));
                for (Weg& w : nein) ja.push_back(std::move(w));
                return ja;
            }
            case AnweisungArt::Solange:
            case AnweisungArt::TueSolange:
            case AnweisungArt::Fuer:
            case AnweisungArt::FuerJedes:
            {
                std::string kopf;
                switch (a.art)
                {
                    case AnweisungArt::Solange: kopf = "while (" + a.text + ") ..."; break;
                    case AnweisungArt::TueSolange: kopf = "do ... while (" + a.text + ")"; break;
                    case AnweisungArt::Fuer: kopf = "for (" + a.init + "; " + a.text + "; " + a.schritt + ") ..."; break;
                    default: kopf = "for (" + a.text + ") ..."; break;
                }
                aktion(wege, kopf, a.zeile);
                vergiss(wege, a, true);  // was in ihr geschieht, ist nicht bekannt
                hinweis("loops are one action each - what happens inside them is not in the table");
                return wege;
            }
            case AnweisungArt::Auswahl: return auswahl(a, std::move(wege));
            case AnweisungArt::Abbruch:
                for (Weg& w : wege) w.raus = true;
                return wege;
            case AnweisungArt::Sprung:
                aktion(wege, "goto " + a.text, a.zeile);
                for (Weg& w : wege) w.endet = true;
                hinweis("goto ends a rule at the jump");
                return wege;
            case AnweisungArt::Versuch:
                hinweis("try: the catch blocks are not in the table");
                return a.kinder.empty() ? wege : schritt(*a.kinder[0], std::move(wege));
            case AnweisungArt::Varianten:
                hinweis("#if variants: only the active branch is in the table");
                for (const modell::AnweisungPtr& zweig : a.kinder)
                    if (zweig->gilt) return folge(zeiger(zweig->kinder), std::move(wege));
                return wege;
            case AnweisungArt::Fortsetzung:  // nur in Schleifen, und die werden nicht zerlegt
            case AnweisungArt::Marke:
            case AnweisungArt::Fall:
            case AnweisungArt::Fang:
            case AnweisungArt::Direktive: return wege;
        }
        return wege;
    }

    static std::pair<std::vector<Weg>, std::vector<Weg>> zwei(std::vector<std::vector<Weg>> geteilt)
    {
        return {std::move(geteilt[0]), std::move(geteilt[1])};
    }

    /// Ein switch: je Fall (Faelle ohne Anweisung dazwischen zusammen) ein Eintrag; ohne break laeuft
    /// ein Fall in den naechsten weiter; ohne default ein Eintrag `otherwise` ohne Aktion.
    std::vector<Weg> auswahl(const Anweisung& a, std::vector<Weg> wege)
    {
        struct Fall
        {
            std::string titel;
            std::vector<const Anweisung*> anweisungen;
            bool standard = false;
        };
        std::vector<Fall> faelle;
        std::vector<const Anweisung*> folgeImRumpf;
        const Anweisung& rumpf = *a.kinder[0];
        if (rumpf.art == AnweisungArt::Block)
            folgeImRumpf = zeiger(rumpf.kinder);
        else
            folgeImRumpf.push_back(&rumpf);
        bool davorFall = false;
        for (const Anweisung* s : folgeImRumpf)
        {
            if (s->art == AnweisungArt::Fall)
            {
                const std::string wert = s->text.empty() ? "default" : s->text;
                if (davorFall)
                {
                    faelle.back().titel += ", " + wert;
                    faelle.back().standard = faelle.back().standard || s->text.empty();
                }
                else
                    faelle.push_back({wert, {}, s->text.empty()});
                davorFall = true;
                continue;
            }
            davorFall = false;
            if (!faelle.empty()) faelle.back().anweisungen.push_back(s);
        }
        std::vector<std::string> eintraege;
        for (const Fall& f : faelle) eintraege.push_back(f.titel);
        const bool mitStandard = std::any_of(faelle.begin(), faelle.end(), [](const Fall& f) { return f.standard; });
        if (!mitStandard) eintraege.emplace_back("otherwise");

        std::vector<std::vector<Weg>> geteilt = verzweige(std::move(wege), frage(a.text, a, true), eintraege);
        std::vector<Weg> aus;
        for (std::size_t i = 0; i < geteilt.size(); ++i)
        {
            std::vector<Weg> laufend = std::move(geteilt[i]);
            // vom Fall i an, bis ein break oder Sprung alle Wege anhaelt
            for (std::size_t j = i; j < faelle.size() && !laufend.empty(); ++j)
            {
                laufend = folge(faelle[j].anweisungen, std::move(laufend));
                if (std::all_of(laufend.begin(), laufend.end(), [](const Weg& w) { return w.endet || w.raus; })) break;
            }
            for (Weg& w : laufend)
            {
                w.raus = false;  // das break endet hier
                aus.push_back(std::move(w));
            }
        }
        return aus;
    }

    Entscheidungstabelle& m_t;
    std::map<std::pair<std::string, int>, std::size_t> m_aktionen;  // (Text, Zeile) -> Aktion
    std::map<std::size_t, std::vector<std::string>> m_reihenfolge;  // Bedingung -> ihre Eintraege der Reihe nach
    std::vector<std::vector<std::string>> m_namen;                   // je Bedingung: die Namen, die sie liest
    bool m_zuViele = false;
    std::size_t m_unmoeglich = 0;
};

}  // namespace

std::string zeilenText(const Entscheidungstabelle::Bedingung& b)
{
    std::string text = b.fall ? "switch (" + b.text + ")" : b.text;
    if (b.erneut) text += " (again, line " + std::to_string(b.quellZeile) + ")";
    return text;
}

Entscheidungstabelle entscheidungstabelleVon(const modell::Funktion& funktion)
{
    Entscheidungstabelle t;
    t.funktion = funktion.vollerName();
    t.quellZeile = funktion.zeile;
    if (!funktion.rumpf) return t;
    Bauer bauer(t);
    std::vector<Weg> wege = bauer.schritt(*funktion.rumpf, {Weg{}});
    std::stable_sort(wege.begin(), wege.end(), [&](const Weg& a, const Weg& b) { return bauer.rang(a) < bauer.rang(b); });
    if (bauer.unmoeglich() > 0)
        t.hinweise.push_back(std::to_string(bauer.unmoeglich()) +
                             " impossible rule(s) left out - a condition was already decided on the way");
    if (bauer.zuViele() || wege.size() > kHoechstensRegeln)
    {
        t.hinweise.push_back((bauer.zuViele() ? "more than " + std::to_string(kHoechstensWege) : std::to_string(wege.size())) +
                             " rules - too many for a decision table");
        return t;
    }
    for (const Weg& w : wege)
    {
        Entscheidungstabelle::Regel r;
        r.eintraege.assign(t.bedingungen.size(), {});
        for (const auto& [b, e] : w.belegung) r.eintraege[b] = e;
        r.aktionen.assign(t.aktionen.size(), false);
        for (const std::size_t a : w.aktionen) r.aktionen[a] = true;
        t.regeln.push_back(std::move(r));
    }
    return t;
}

}  // namespace c2d::diagramm
