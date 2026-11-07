/**
 * @file Datenfluss.cpp
 * @brief Der Datenfluss im Projekt: das Umfeld der Ableitung und die Bereiche einer Datei
 */
#include "c2d/projekt/Datenfluss.hpp"

#include <map>
#include <memory>
#include <set>

namespace c2d::projekt
{

namespace
{

using diagramm::DatenflussBereich;
using diagramm::DatenflussUmfeld;
using diagramm::SparkRueckgabe;
using diagramm::SparkWert;

/// Was eine Funktion liefert, je Datei und Name einmal gerechnet; eine Funktion in Arbeit liefert nichts
/// (sonst liefe eine Rekursion ohne Ende).
struct Gedaechtnis
{
    std::map<std::pair<std::size_t, std::string>, SparkRueckgabe> liefert;
    std::set<std::pair<std::size_t, std::string>> inArbeit;
    std::map<std::pair<std::size_t, std::string>, bool> istFunktion;
};

DatenflussUmfeld umfeld(const Projekt& p, std::size_t datei, const std::shared_ptr<Gedaechtnis>& g)
{
    DatenflussUmfeld u;
    const Datei& d = p.dateien[datei];
    u.vorgegeben = !d.zellen.empty();
    u.istFunktion = [&p, datei, g](std::string_view name) {
        const std::pair<std::size_t, std::string> schluessel{datei, std::string(name)};
        const auto [da, neu] = g->istFunktion.try_emplace(schluessel, false);
        if (neu) da->second = !kandidaten(p, datei, name).empty();
        return da->second;
    };
    u.liefert = [&p, datei, g](std::string_view name) {
        const std::pair<std::size_t, std::string> schluessel{datei, std::string(name)};
        if (const auto da = g->liefert.find(schluessel); da != g->liefert.end()) return da->second;
        if (!g->inArbeit.insert(schluessel).second) return SparkRueckgabe{};
        SparkRueckgabe w;
        for (const Fundstelle& f : kandidaten(p, datei, name))
        {
            const modell::Funktion& fn = p.dateien[f.datei].ergebnis.einheit.funktionen[f.funktion];
            w = diagramm::rueckgabeVon(fn, umfeld(p, f.datei, g));
            if (w.wert != SparkWert::Keiner) break;
        }
        g->inArbeit.erase(schluessel);
        g->liefert[schluessel] = w;
        return w;
    };
    u.abfrageIn = [&d](int von, int bis) -> const modell::Abfrage* {
        for (const modell::Abfrage& q : d.ergebnis.einheit.abfragen)
            if (q.zeile >= von && q.zeile <= bis) return &q;
        return nullptr;
    };
    u.planAn = [&d](int zeile) -> std::string {
        for (const modell::Plan& p : d.ergebnis.einheit.plaene)
            if (p.aufruf == zeile) return p.name;
        return {};
    };
    return u;
}

bool istZelle(const modell::Funktion& f)
{
    return f.bereich.empty() && f.name.starts_with("<cell ");
}

/// Die Bereiche der Datei: im Notebook die Zellen (gemeinsam), sonst jede Funktion mit Rumpf.
std::vector<DatenflussBereich> bereicheVon(const Datei& d, bool& gemeinsam)
{
    std::vector<DatenflussBereich> aus;
    gemeinsam = !d.zellen.empty();
    for (const modell::Funktion& f : d.ergebnis.einheit.funktionen)
        if (f.rumpf && (!gemeinsam || istZelle(f))) aus.push_back({&f});
    return aus;
}

diagramm::Datenflussdiagramm flussVon(const Projekt& p, std::size_t datei, std::string_view funktion,
                                      const std::shared_ptr<Gedaechtnis>& g)
{
    const Datei& d = p.dateien[datei];
    const DatenflussUmfeld u = umfeld(p, datei, g);
    bool gemeinsam = false;
    const std::vector<DatenflussBereich> alle = bereicheVon(d, gemeinsam);
    if (funktion.empty()) return diagramm::datenflussVon(alle, gemeinsam, u, "Data flow of " + d.pfad);
    const std::string titel = "Data flow of " + std::string(funktion);
    // eine Zelle: der Fluss aller Zellen bis zu ihr, gezeigt nur ihr Teil
    if (gemeinsam)
        for (std::size_t i = 0; i < alle.size(); ++i)
            if (alle[i].funktion->vollerName() == funktion)
                return diagramm::datenflussVon(std::span(alle).first(i + 1), true, u, titel, i);
    for (const modell::Funktion& f : d.ergebnis.einheit.funktionen)
        if (f.rumpf && f.vollerName() == funktion)
        {
            const DatenflussBereich b{&f};
            return diagramm::datenflussVon(std::span(&b, 1), false, u, titel);
        }
    return {};
}

}  // namespace

std::vector<std::string> datenfluesseIn(const Projekt& projekt, std::size_t datei)
{
    std::vector<std::string> aus;
    if (datei >= projekt.dateien.size()) return aus;
    const Datei& d = projekt.dateien[datei];
    const auto g = std::make_shared<Gedaechtnis>();
    const DatenflussUmfeld u = umfeld(projekt, datei, g);
    // ein Lauf ueber die Datei: im Notebook die Zellen gemeinsam, sonst jede Funktion fuer sich; dazu im
    // Notebook die Funktionen ausser den Zellen je fuer sich
    bool gemeinsam = false;
    const std::vector<DatenflussBereich> bereiche = bereicheVon(d, gemeinsam);
    const std::vector<bool> mit = diagramm::bereicheMitFluss(bereiche, gemeinsam, u);
    std::vector<DatenflussBereich> uebrige;
    if (gemeinsam)
        for (const modell::Funktion& f : d.ergebnis.einheit.funktionen)
            if (f.rumpf && !istZelle(f)) uebrige.push_back({&f});
    const std::vector<bool> mitUebrigen = diagramm::bereicheMitFluss(uebrige, false, u);
    for (const modell::Funktion& f : d.ergebnis.einheit.funktionen)
    {
        if (!f.rumpf) continue;
        bool hat = false;
        for (std::size_t i = 0; i < bereiche.size(); ++i)
            if (bereiche[i].funktion == &f) hat = mit[i];
        for (std::size_t i = 0; i < uebrige.size(); ++i)
            if (uebrige[i].funktion == &f) hat = mitUebrigen[i];
        if (hat) aus.push_back(f.vollerName());
    }
    return aus;
}

diagramm::Datenflussdiagramm datenflussIn(const Projekt& projekt, std::size_t datei, std::string_view funktion)
{
    if (datei >= projekt.dateien.size()) return {};
    return flussVon(projekt, datei, funktion, std::make_shared<Gedaechtnis>());
}

}  // namespace c2d::projekt
