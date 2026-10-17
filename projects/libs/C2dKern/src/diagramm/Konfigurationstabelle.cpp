/**
 * @file Konfigurationstabelle.cpp
 * @brief Je Zweig einer Kette die absolute Bedingung, in disjunktiver Normalform, je Name eine Spalte
 */
#include "c2d/diagramm/Konfigurationstabelle.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <tuple>

namespace c2d::diagramm
{

namespace
{

using modell::Vorbedingung;
using modell::Weiche;
using V = Vorbedingung;

/// So viele Terme darf ein Zwischenstand haben; mehr: nicht zerlegt (eine Zeile Text).
constexpr std::size_t kGrenze = 256;

/// Ein Teil eines Terms: ein Name definiert oder nicht, ein Vergleich (verneint: der Gegen-Operator),
/// oder Text, der sich nicht zerlegen liess.
struct Literal
{
    enum class Art
    {
        Definiert,
        Vergleich,
        Text,
    };
    Art art = Art::Text;
    std::string name;
    std::string op;
    std::string wert;
    std::string text;
    bool positiv = true;  ///< Definiert und Text; ein Vergleich ist immer positiv

    [[nodiscard]] auto schluessel() const { return std::tie(art, name, op, wert, text, positiv); }
    bool operator==(const Literal& b) const { return schluessel() == b.schluessel(); }
    bool operator<(const Literal& b) const { return schluessel() < b.schluessel(); }
};

using Term = std::vector<Literal>;  // Und
struct Dnf                          // Oder
{
    std::vector<Term> terme;
    bool zuGross = false;
};

std::string gegenOp(const std::string& op)
{
    static const std::map<std::string, std::string> kGegen{{"==", "!="}, {"!=", "=="}, {"<", ">="},
                                                           {"<=", ">"},  {">", "<="},  {">=", "<"}};
    const auto g = kGegen.find(op);
    return g != kGegen.end() ? g->second : op;
}

/// Eine Zahl wie im Praeprozessor (`0x10`, `2u`); nullopt = keine.
std::optional<long long> zahl(const std::string& text)
{
    std::string t = text;
    while (!t.empty() && std::string_view("uUlL").find(t.back()) != std::string_view::npos) t.pop_back();
    if (t.empty()) return std::nullopt;
    char* ende = nullptr;
    const long long n = std::strtoll(t.c_str(), &ende, 0);
    if (ende == nullptr || *ende != '\0') return std::nullopt;
    return n;
}

bool gilt(long long x, const std::string& op, long long w)
{
    if (op == "==") return x == w;
    if (op == "!=") return x != w;
    if (op == "<") return x < w;
    if (op == "<=") return x <= w;
    if (op == ">") return x > w;
    return x >= w;
}

/// Ein Term ohne Widerspruch und ohne Doppeltes; nullopt = er kann nie gelten.
std::optional<Term> vereinfacht(Term t)
{
    std::sort(t.begin(), t.end());
    t.erase(std::unique(t.begin(), t.end()), t.end());
    std::map<std::string, std::vector<const Literal*>> jeName;
    for (const Literal& l : t)
        if (l.art != Literal::Art::Text) jeName[l.name].push_back(&l);
    std::vector<Literal> weg;  // nach Wert: es gibt keine doppelten mehr
    for (const auto& [name, ls] : jeName)
    {
        bool def = false;
        bool undef = false;
        std::vector<const Literal*> vergleiche;
        for (const Literal* l : ls)
        {
            if (l->art == Literal::Art::Definiert) (l->positiv ? def : undef) = true;
            if (l->art == Literal::Art::Vergleich) vergleiche.push_back(l);
        }
        if (def && undef) return std::nullopt;
        // nicht definiert zaehlt im #if als 0: der Vergleich steht dann fest
        if (undef)
            for (const Literal* l : vergleiche)
            {
                const std::optional<long long> w = zahl(l->wert);
                if (!w) continue;
                if (!gilt(0, l->op, *w)) return std::nullopt;
                weg.push_back(*l);
            }
        // Vergleiche mit Zahlen: gibt es einen Wert, der alle erfuellt? Ein Rand liegt an einer Zahl oder daneben.
        std::vector<std::pair<std::string, long long>> mitZahl;
        for (const Literal* l : vergleiche)
            if (const std::optional<long long> w = zahl(l->wert)) mitZahl.emplace_back(l->op, *w);
        if (!undef && mitZahl.size() > 1)
        {
            std::vector<long long> probe{0};
            for (const auto& [op, w] : mitZahl)
                for (const long long d : {-1LL, 0LL, 1LL}) probe.push_back(w + d);
            const bool moeglich = std::any_of(probe.begin(), probe.end(), [&](long long x) {
                return std::all_of(mitZahl.begin(), mitZahl.end(), [&](const auto& v) { return gilt(x, v.first, v.second); });
            });
            if (!moeglich) return std::nullopt;
        }
    }
    std::erase_if(t, [&](const Literal& l) { return std::find(weg.begin(), weg.end(), l) != weg.end(); });
    return t;
}

Dnf und(Dnf a, const Dnf& b)
{
    Dnf aus;
    aus.zuGross = a.zuGross || b.zuGross;
    for (const Term& x : a.terme)
        for (const Term& y : b.terme)
        {
            Term t = x;
            t.insert(t.end(), y.begin(), y.end());
            if (std::optional<Term> v = vereinfacht(std::move(t))) aus.terme.push_back(std::move(*v));
            if (aus.terme.size() > kGrenze)
            {
                aus.zuGross = true;
                return aus;
            }
        }
    return aus;
}

/// Die Bedingung `v` (verneint, wenn nicht `positiv`) als Oder von Und.
Dnf dnf(const V& v, bool positiv)
{
    Dnf aus;
    switch (v.art)
    {
        case V::Art::Konstante:
            if ((v.wert != "0") == positiv) aus.terme.emplace_back();
            return aus;
        case V::Art::Definiert:
            aus.terme.push_back({Literal{Literal::Art::Definiert, v.name, {}, {}, {}, positiv}});
            return aus;
        case V::Art::Vergleich:
            aus.terme.push_back({Literal{Literal::Art::Vergleich, v.name, positiv ? v.op : gegenOp(v.op), v.wert, {}, true}});
            return aus;
        case V::Art::Text:
            aus.terme.push_back({Literal{Literal::Art::Text, {}, {}, {}, v.text, positiv}});
            return aus;
        case V::Art::Nicht:
            return v.teile.empty() ? aus : dnf(v.teile.front(), !positiv);
        case V::Art::Und:
        case V::Art::Oder:
        {
            // Und, oder ein verneintes Oder: das Produkt; Oder, oder ein verneintes Und: die Vereinigung
            if ((v.art == V::Art::Und) == positiv)
            {
                aus.terme.emplace_back();
                for (const V& t : v.teile)
                {
                    aus = und(std::move(aus), dnf(t, positiv));
                    if (aus.zuGross) return aus;
                }
                return aus;
            }
            for (const V& t : v.teile)
            {
                Dnf d = dnf(t, positiv);
                aus.zuGross = aus.zuGross || d.zuGross;
                for (Term& x : d.terme) aus.terme.push_back(std::move(x));
                if (aus.terme.size() > kGrenze) aus.zuGross = true;
                if (aus.zuGross) return aus;
            }
            return aus;
        }
    }
    return aus;
}

/// Ohne Terme, die einen anderen enthalten (`A || A && B` ist `A`), und ohne doppelte; jeder
/// vereinfacht und damit sortiert.
void entbehrliche(std::vector<Term>& terme)
{
    std::vector<Term> geprueft;
    for (Term& t : terme)
        if (std::optional<Term> v = vereinfacht(std::move(t))) geprueft.push_back(std::move(*v));
    terme = std::move(geprueft);
    std::stable_sort(terme.begin(), terme.end(), [](const Term& a, const Term& b) { return a.size() < b.size(); });
    std::vector<Term> aus;
    for (Term& t : terme)
    {
        const bool enthalten = std::any_of(aus.begin(), aus.end(), [&](const Term& k) {
            return std::includes(t.begin(), t.end(), k.begin(), k.end());  // beide sortiert (vereinfacht)
        });
        if (!enthalten) aus.push_back(std::move(t));
    }
    terme = std::move(aus);
}

V nicht(V innen)
{
    V v;
    v.art = V::Art::Nicht;
    v.teile.push_back(std::move(innen));
    return v;
}

/// Die absolute Bedingung von Zweig `z` der Weiche `w`: der umgebende Zweig, alle davor falsch, er wahr.
V bedingungVon(std::span<const Weiche> weichen, std::size_t w, std::size_t z)
{
    const Weiche& weiche = weichen[w];
    V v;
    v.art = V::Art::Und;
    if (weiche.eltern != modell::kKeinZweig && weiche.eltern < w)
        v.teile.push_back(bedingungVon(weichen, weiche.eltern, weiche.elternZweig));
    for (std::size_t i = 0; i < z && i < weiche.baeume.size(); ++i) v.teile.push_back(nicht(weiche.baeume[i]));
    if (z < weiche.baeume.size()) v.teile.push_back(weiche.baeume[z]);
    return v;
}

std::string zelle(const std::vector<const Literal*>& ls)
{
    std::string aus;
    for (const Literal* l : ls)
    {
        if (!aus.empty()) aus += ", ";
        if (l->art == Literal::Art::Definiert)
            aus += l->positiv ? "def" : "!def";
        else
            aus += l->op + " " + l->wert;
    }
    return aus.empty() ? "\xE2\x80\x93" : aus;  // Halbgeviertstrich: gleich
}

}  // namespace

std::string zweigMarke(const Weiche& w, std::size_t z)
{
    if (z >= w.zweige.size()) return "(no #else)";
    if (z == 0) return "#if " + w.zweige[0];
    return w.zweige[z] == "else" ? "#else" : "#elif " + w.zweige[z];
}

int zweigZeile(const Weiche& w, std::size_t z)
{
    if (z < w.zweigZeilen.size()) return w.zweigZeilen[z];
    return w.endZeile != 0 ? w.endZeile : w.zeile;
}

std::vector<Konfigurationstabelle> konfigurationVon(std::span<const Weiche> weichen, std::span<const modell::Schalter> schalter)
{
    std::vector<Konfigurationstabelle> aus;
    for (std::size_t w = 0; w < weichen.size(); ++w)
    {
        const Weiche& weiche = weichen[w];
        Konfigurationstabelle t;
        t.weiche = w;
        t.quellZeile = weiche.zeile;
        t.titel = zweigMarke(weiche, 0) + " (line " + std::to_string(weiche.zeile) + ")";
        if (weiche.eltern != modell::kKeinZweig && weiche.eltern < weichen.size())
        {
            const Weiche& eltern = weichen[weiche.eltern];
            t.titel += ", in " + zweigMarke(eltern, weiche.elternZweig) + " (line " +
                       std::to_string(zweigZeile(eltern, weiche.elternZweig)) + ")";
            for (std::size_t e = weiche.eltern; e != modell::kKeinZweig && e < weichen.size(); e = weichen[e].eltern) ++t.tiefe;
        }

        // je Zweig — und das fehlende #else — seine Terme
        const bool zerlegbar = weiche.baeume.size() == weiche.zweige.size();
        const std::size_t zweige = weiche.zweige.size() + (!weiche.zweige.empty() && weiche.zweige.back() == "else" ? 0 : 1);
        struct Zweig
        {
            std::size_t z;
            Dnf d;
        };
        std::vector<Zweig> alle;
        for (std::size_t z = 0; z < zweige; ++z)
        {
            Dnf d = zerlegbar ? dnf(bedingungVon(weichen, w, z), true) : Dnf{{}, true};
            if (!d.zuGross) entbehrliche(d.terme);
            alle.push_back({z, std::move(d)});
        }

        // die Spalten: jeder Name in einem zerlegten Term, in der Reihenfolge, in der die Datei nach ihm fragt
        std::vector<std::string> namen;
        const auto text = [](const Term& t) {
            return std::any_of(t.begin(), t.end(), [](const Literal& l) { return l.art == Literal::Art::Text; });
        };
        const auto zerlegt = [&](const Dnf& d) {
            return !d.zuGross && d.terme.size() <= kMaxAlternativen && std::none_of(d.terme.begin(), d.terme.end(), text);
        };
        for (const Zweig& z : alle)
            if (zerlegt(z.d))
                for (const Term& term : z.d.terme)
                    for (const Literal& l : term)
                        if (std::find(namen.begin(), namen.end(), l.name) == namen.end()) namen.push_back(l.name);
        const auto rang = [&](const std::string& n) {
            const auto s = std::find_if(schalter.begin(), schalter.end(), [&](const modell::Schalter& x) { return x.name == n; });
            return static_cast<std::size_t>(s - schalter.begin());
        };
        std::stable_sort(namen.begin(), namen.end(), [&](const std::string& a, const std::string& b) { return rang(a) < rang(b); });
        for (const std::string& n : namen)
        {
            Konfigurationstabelle::Spalte s{n, {}, 0, false};
            if (const std::size_t r = rang(n); r < schalter.size())
            {
                const modell::Schalter& x = schalter[r];
                s.jetzt = !x.definiert ? "undef" : x.wert.empty() ? "def" : "= " + x.wert;
                s.inDatei = x.inDatei;
                s.vorgegeben = x.vorgegeben;
            }
            t.spalten.push_back(std::move(s));
        }

        for (const Zweig& z : alle)
        {
            Konfigurationstabelle::Zeile r;
            r.zweig = zweigMarke(weiche, z.z);
            r.zweigIndex = z.z;
            r.quellZeile = zweigZeile(weiche, z.z);
            r.geltend = weiche.geltend == z.z;
            r.bedingung = z.z < weiche.zweige.size() ? weiche.zweige[z.z] : "else";
            if (!zerlegt(z.d))
            {
                r.text = true;
                r.werte.assign(t.spalten.size(), "?");
                std::string grund;
                if (z.d.zuGross || z.d.terme.size() > kMaxAlternativen)
                    grund = "more than " + std::to_string(kMaxAlternativen) + " alternatives";
                else
                    for (const Term& term : z.d.terme)
                        for (const Literal& l : term)
                            if (grund.empty() && l.art == Literal::Art::Text) grund = "'" + l.text + "' is not split into names";
                t.hinweise.push_back("line " + std::to_string(r.quellZeile) + ": " + grund);
                t.zeilen.push_back(std::move(r));
                continue;
            }
            if (z.d.terme.empty())
            {
                r.nie = true;
                r.werte.assign(t.spalten.size(), "\xE2\x80\x93");
                t.zeilen.push_back(std::move(r));
                continue;
            }
            for (std::size_t k = 0; k < z.d.terme.size(); ++k)
            {
                Konfigurationstabelle::Zeile a = r;
                if (k > 0) a.zweig = "or";
                for (const Konfigurationstabelle::Spalte& s : t.spalten)
                {
                    std::vector<const Literal*> ls;
                    for (const Literal& l : z.d.terme[k])
                        if (l.name == s.name) ls.push_back(&l);
                    a.werte.push_back(zelle(ls));
                }
                t.zeilen.push_back(std::move(a));
            }
        }
        aus.push_back(std::move(t));
    }
    return aus;
}

}  // namespace c2d::diagramm
