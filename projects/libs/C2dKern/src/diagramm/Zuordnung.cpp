/**
 * @file Zuordnung.cpp
 * @brief Zuordnungen erkennen: switch-Faelle, die nur Werte liefern; konstante Tabellen auslegen
 */
#include "c2d/diagramm/Zuordnung.hpp"

#include <algorithm>
#include <functional>

namespace c2d::diagramm
{

namespace
{

using modell::Anweisung;
using modell::AnweisungArt;

/// Ein Fall: seine Werte (je Wert die Zeile seiner Marke) und seine Anweisungen bis zum naechsten.
struct Fall
{
    std::vector<std::pair<std::string, int>> werte;
    std::vector<const Anweisung*> anweisungen;
    bool standard = false;
};

std::vector<Fall> faelleVon(const Anweisung& sw)
{
    std::vector<const Anweisung*> folge;
    const Anweisung& rumpf = *sw.kinder[0];
    if (rumpf.art == AnweisungArt::Block)
        for (const modell::AnweisungPtr& k : rumpf.kinder) folge.push_back(k.get());
    else
        folge.push_back(&rumpf);
    std::vector<Fall> faelle;
    bool davorFall = false;
    for (const Anweisung* s : folge)
    {
        if (s->art == AnweisungArt::Direktive) continue;
        if (s->art == AnweisungArt::Fall)
        {
            if (!davorFall) faelle.emplace_back();
            faelle.back().werte.emplace_back(s->text.empty() ? "default" : s->text, s->zeile);
            faelle.back().standard = faelle.back().standard || s->text.empty();
            davorFall = true;
            continue;
        }
        davorFall = false;
        if (faelle.empty()) return {};  // Anweisungen vor dem ersten Fall: keine reine Zuordnung
        faelle.back().anweisungen.push_back(s);
    }
    return faelle;
}

/// Was ein Fall liefert: ein return mit Wert, oder Zuweisungen (Ziel, Wert) und dann break.
struct Lieferung
{
    bool rueck = false;
    std::vector<std::pair<std::string, std::string>> paare;
};

std::optional<Lieferung> lieferung(const Fall& f)
{
    const std::vector<const Anweisung*>& s = f.anweisungen;
    if (s.size() == 1 && s[0]->art == AnweisungArt::Ruecksprung && !s[0]->text.empty())
        return Lieferung{true, {{"return", s[0]->text}}};
    if (s.size() < 2 || s.back()->art != AnweisungArt::Abbruch) return std::nullopt;
    Lieferung l;
    for (std::size_t i = 0; i + 1 < s.size(); ++i)
    {
        if (s[i]->art != AnweisungArt::Einfach || s[i]->ziel.empty()) return std::nullopt;
        l.paare.emplace_back(s[i]->ziel, s[i]->wert);
    }
    return l;
}

std::vector<std::string> ziele(const Lieferung& l)
{
    std::vector<std::string> aus;
    for (const auto& [ziel, wert] : l.paare) aus.push_back(ziel);
    std::sort(aus.begin(), aus.end());
    return aus;
}

std::optional<Zuordnung> auswerten(const Anweisung& sw, const std::string& funktion)
{
    const std::vector<Fall> faelle = faelleVon(sw);
    if (faelle.empty()) return std::nullopt;
    std::vector<Lieferung> lieferungen;
    for (const Fall& f : faelle)
    {
        std::optional<Lieferung> l = lieferung(f);
        if (!l) return std::nullopt;
        lieferungen.push_back(std::move(*l));
    }
    // alle liefern gleich: alle return, oder alle an dieselben Namen
    for (const Lieferung& l : lieferungen)
        if (l.rueck != lieferungen.front().rueck || ziele(l) != ziele(lieferungen.front())) return std::nullopt;

    Zuordnung z;
    z.titel = funktion + "(): switch (" + sw.text + ")";
    z.quellZeile = sw.zeile;
    z.schluessel = sw.text;
    for (const auto& [ziel, wert] : lieferungen.front().paare) z.spalten.push_back(ziel);
    const auto zeilen = [&](std::size_t i) {
        for (const auto& [wert, zeile] : faelle[i].werte)
        {
            Zuordnung::Zeile r{wert, {}, zeile};
            for (const std::string& spalte : z.spalten)
                for (const auto& [ziel, w] : lieferungen[i].paare)
                    if (ziel == spalte) r.werte.push_back(w);
            z.zeilen.push_back(std::move(r));
        }
    };
    // default zuletzt — seine Zeilen mit allem, was sich den Rumpf mit ihm teilt
    for (std::size_t i = 0; i < faelle.size(); ++i)
        if (!faelle[i].standard) zeilen(i);
    bool mitStandard = false;
    for (std::size_t i = 0; i < faelle.size(); ++i)
        if (faelle[i].standard)
        {
            zeilen(i);
            mitStandard = true;
        }
    if (!mitStandard) z.zeilen.push_back({"otherwise", std::vector<std::string>(z.spalten.size(), "\xE2\x80\x94"), 0});
    return z;
}

void durchlaufe(const Anweisung& a, const std::function<void(const Anweisung&)>& tue)
{
    tue(a);
    for (const modell::AnweisungPtr& k : a.kinder) durchlaufe(*k, tue);
    if (a.sonst) durchlaufe(*a.sonst, tue);
}

/// Der Name des Typs ohne Vorsaetze: `static const struct Kanal` -> `Kanal`.
std::string kernTyp(const std::string& typ)
{
    std::string aus;
    std::string wort;
    const auto nimm = [&] {
        if (!wort.empty() && wort != "static" && wort != "const" && wort != "volatile" && wort != "struct" &&
            wort != "extern")
            aus = wort;
        wort.clear();
    };
    for (const char c : typ)
    {
        if (c == ' ' || c == '*')
            nimm();
        else
            wort += c;
    }
    nimm();
    return aus;
}

/// Die Glieder eines Strukturtyps der Einheit (auch ueber einen typedef), sonst ueber `suche`;
/// leer = unbekannt.
std::vector<std::string> gliederVon(const modell::Einheit& e, const std::string& typ, const TypSuche& suche)
{
    const auto glieder = [](const modell::Typ& t) {
        std::vector<std::string> aus;
        for (const modell::Feld& f : t.felder) aus.push_back(f.name);
        return aus;
    };
    std::string name = typ;
    for (const modell::Alias& a : e.aliase)
        if (a.name == name) name = kernTyp(a.ziel);
    for (const modell::Typ& t : e.typen)
        if (t.name == name || t.name == typ) return glieder(t);
    if (suche)
        if (const modell::Typ* t = suche(typ)) return glieder(*t);
    return {};
}

}  // namespace

std::optional<Zuordnung> zuordnungVon(const modell::Funktion& funktion, int zeile)
{
    if (!funktion.rumpf) return std::nullopt;
    std::optional<Zuordnung> aus;
    durchlaufe(*funktion.rumpf, [&](const Anweisung& a) {
        if (!aus && a.art == AnweisungArt::Auswahl && a.zeile == zeile) aus = auswerten(a, funktion.vollerName());
    });
    return aus;
}

std::vector<int> zeilenMitZuordnung(const modell::Funktion& funktion)
{
    std::vector<int> aus;
    if (!funktion.rumpf) return aus;
    durchlaufe(*funktion.rumpf, [&](const Anweisung& a) {
        if (a.art == AnweisungArt::Auswahl && auswerten(a, funktion.vollerName())) aus.push_back(a.zeile);
    });
    return aus;
}

std::vector<Zuordnung> nachschlagetabellenVon(const modell::Einheit& einheit, const TypSuche& suche)
{
    std::vector<Zuordnung> aus;
    for (const modell::Variable& v : einheit.variablen)
    {
        // ein Array mit einer Liste als Anfangswert
        if (!v.anfang || !v.anfang->liste || v.zusatz.find('[') == std::string::npos) continue;
        Zuordnung z;
        z.titel = v.typ + " " + v.name + v.zusatz;
        z.quellZeile = v.zeile;
        z.schluessel = "index";
        const std::vector<modell::Initialisierer>& elemente = v.anfang->teile;
        const bool strukturen = std::any_of(elemente.begin(), elemente.end(), [](const auto& e) { return e.liste; });

        // die Spalten: "value", oder die Glieder des Strukturtyps (sonst 1, 2, …), dazu benannte
        std::vector<std::string> glieder = strukturen ? gliederVon(einheit, kernTyp(v.typ), suche) : std::vector<std::string>{};
        if (!strukturen)
            z.spalten = {"value"};
        else
        {
            z.spalten = glieder;
            std::size_t breit = 0;
            for (const modell::Initialisierer& e : elemente)
            {
                breit = std::max(breit, e.teile.size());
                for (const modell::Initialisierer& t : e.teile)
                    if (!t.glied.empty() && std::find(z.spalten.begin(), z.spalten.end(), t.glied) == z.spalten.end())
                        z.spalten.push_back(t.glied);
            }
            for (std::size_t i = z.spalten.size(); i < breit; ++i) z.spalten.push_back(std::to_string(i + 1));
        }

        // je Element eine Zeile; der Index: benannt, sonst weitergezaehlt (nach einem benannten: `X + n`)
        std::string letzter;
        std::size_t weiter = 0;
        for (const modell::Initialisierer& e : elemente)
        {
            std::string index;
            if (!e.index.empty())
            {
                index = e.index;
                letzter = e.index;
                weiter = 1;
            }
            else
            {
                index = letzter.empty() ? std::to_string(weiter) : letzter + " + " + std::to_string(weiter);
                ++weiter;
            }
            Zuordnung::Zeile r{index, std::vector<std::string>(z.spalten.size()), e.zeile};
            if (!strukturen)
                r.werte[0] = e.text;
            else if (!e.liste)
                r.werte[0] = e.text;
            else
            {
                std::size_t stelle = 0;
                for (const modell::Initialisierer& t : e.teile)
                {
                    if (!t.glied.empty())
                        stelle = static_cast<std::size_t>(std::find(z.spalten.begin(), z.spalten.end(), t.glied) - z.spalten.begin());
                    if (stelle < r.werte.size()) r.werte[stelle] = t.text;
                    ++stelle;
                }
            }
            z.zeilen.push_back(std::move(r));
        }
        aus.push_back(std::move(z));
    }
    return aus;
}

}  // namespace c2d::diagramm
