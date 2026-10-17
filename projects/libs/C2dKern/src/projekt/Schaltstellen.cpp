/**
 * @file Schaltstellen.cpp
 * @brief Wo schaltet ein Name: aus den Konfigurationstabellen aller Dateien und wer ihn festlegt
 */
#include "c2d/projekt/Schaltstellen.hpp"

#include "c2d/diagramm/Konfigurationstabelle.hpp"
#include "c2d/projekt/Projektdatei.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>

namespace c2d::projekt
{

namespace
{

using diagramm::Konfigurationstabelle;
using diagramm::Schalteruebersicht;
using diagramm::Schaltstellen;

constexpr const char* kGleich = "\xE2\x80\x93";  // Halbgeviertstrich: in dieser Alternative gleich

std::string dateiname(const std::string& pfad)
{
    const std::size_t strich = pfad.find_last_of('/');
    return strich == std::string::npos ? pfad : pfad.substr(strich + 1);
}

/// Die letzte Definition von aussen fuer `name`; nullptr = keine.
const sprache::Definition* vonAussen(std::span<const sprache::Definition> aussen, std::string_view name)
{
    const sprache::Definition* aus = nullptr;
    for (const sprache::Definition& d : aussen)
        if (d.name == name) aus = &d;
    return aus;
}

const modell::Schalter* schalterVon(const sprache::Ergebnis& e, std::string_view name)
{
    const auto s = std::find_if(e.schalter.begin(), e.schalter.end(), [&](const modell::Schalter& x) { return x.name == name; });
    return s != e.schalter.end() ? &*s : nullptr;
}

/// Woher der Stand eines Namens in einer Datei kommt.
enum class Quelle
{
    keine,
    sprache,
    datei,
    aussen,
    projekt,
    abschnitt,
    kopf,
};

struct Stand
{
    std::string wert;      ///< `def`, `= 100`, `undef`
    Quelle quelle = Quelle::keine;
    std::string zusatz;    ///< `(project)`, `(header x.h)`, `(line 12)` …
    int inDatei = 0;       ///< die Datei legt ihn selbst fest, in dieser Zeile; 0 = nicht

    [[nodiscard]] std::string text() const { return zusatz.empty() ? wert : wert + " " + zusatz; }
};

/// Der Kopf des Projekts, der `name` fuer die Datei festlegt: der letzte, der ihn definiert.
std::string kopfMit(const Projekt& p, const Datei& d, std::string_view name)
{
    std::string aus;
    for (const std::size_t k : d.koepfe)
        for (const modell::Makro& m : p.dateien[k].ergebnis.einheit.makros)
            if (m.name == name) aus = dateiname(p.dateien[k].pfad);
    return aus;
}

Stand standVon(const Projekt& p, const Datei& d, const modell::Schalter& s, std::span<const sprache::Definition> aussen)
{
    Stand aus;
    aus.wert = !s.definiert ? "undef" : s.wert.empty() ? "def" : "= " + s.wert;
    if (s.vorgegeben)
    {
        aus.quelle = Quelle::sprache;
        aus.zusatz = "(language)";
    }
    else if (s.inDatei > 0)
    {
        aus.quelle = Quelle::datei;
        aus.zusatz = "(line " + std::to_string(s.inDatei) + ")";
    }
    else if (vonAussen(aussen, s.name) != nullptr)
    {
        aus.quelle = Quelle::aussen;
        aus.zusatz = "(outside)";
    }
    else
        switch (herkunft(p.einstellungen, d, s.name))
        {
            case Herkunft::keine: break;
            case Herkunft::projekt:
                aus.quelle = Quelle::projekt;
                aus.zusatz = "(project)";
                break;
            case Herkunft::abschnitt:
            {
                aus.quelle = Quelle::abschnitt;
                std::string ordner;
                const std::size_t strich = d.pfad.find_last_of('/');
                const std::string hier = strich == std::string::npos ? std::string() : d.pfad.substr(0, strich);
                for (const Abschnitt& a : p.einstellungen.abschnitte)
                    if (liegtIn(a.ordner, hier) &&
                        std::any_of(a.definitionen.begin(), a.definitionen.end(), [&](const sprache::Definition& x) { return x.name == s.name; }))
                        ordner = a.ordner;  // der spaetere gilt
                aus.zusatz = "(section " + ordner + ")";
                break;
            }
            case Herkunft::kopf:
            {
                aus.quelle = Quelle::kopf;
                const std::string kopf = kopfMit(p, d, s.name);
                aus.zusatz = kopf.empty() ? "(header outside the project)" : "(header " + kopf + ")";
                break;
            }
        }
    return aus;
}

/// Fragt kein zerlegbarer Zweig der Datei nach dem Namen (`#if F(X) > 1`), steht er nicht unter
/// ihren Schaltern: sein Stand dann aus den Definitionen, mit denen sie gelesen ist, und denen von aussen.
modell::Schalter ohneFrage(const Datei& d, std::string_view name, std::span<const sprache::Definition> aussen)
{
    modell::Schalter s;
    s.name = std::string(name);
    const sprache::Definition* gilt = vonAussen(aussen, name);
    if (gilt == nullptr)
        for (const sprache::Definition& x : d.definitionen)
            if (x.name == name) gilt = &x;
    if (gilt != nullptr && gilt->definiert)
    {
        s.definiert = true;
        s.wert = gilt->wert;
    }
    return s;
}

/// Das letzte eigene `#define` oder `#undef` von `name` in der Datei vor `zeile`; nullopt = keins.
/// (Liegt es vor der ersten Frage, sagt es dasselbe wie Schalter::inDatei.)
std::optional<Schaltstellen::Setzung> eigeneVor(const Datei& d, std::string_view name, int zeile)
{
    std::optional<Schaltstellen::Setzung> aus;
    for (const modell::Makro& m : d.ergebnis.einheit.makros)
        if (m.name == name && m.zeile < zeile && (!aus || m.zeile > aus->zeile)) aus = {"#define", d.pfad, m.zeile, m.text};
    for (const modell::Aufhebung& u : d.ergebnis.einheit.aufhebungen)
        if (u.name == name && u.zeile < zeile && (!aus || u.zeile > aus->zeile)) aus = {"#undef", d.pfad, u.zeile, {}};
    return aus;
}

/// Der Stand an einer Kette in Zeile `zeile`: was die Datei selbst davor festlegt (auch nach der
/// ersten Frage, wie `#ifndef X` / `#define X`), sonst der Stand dort, wo sie zuerst fragt.
Stand standAn(const Projekt& p, const Datei& d, const modell::Schalter& s, std::span<const sprache::Definition> aussen, int zeile)
{
    Stand aus = standVon(p, d, s, aussen);
    aus.inDatei = s.inDatei;
    if (const std::optional<Schaltstellen::Setzung> eigen = eigeneVor(d, s.name, zeile))
    {
        aus.wert = eigen->herkunft == "#undef" ? std::string("undef") : eigen->wert.empty() ? std::string("def") : "= " + eigen->wert;
        aus.quelle = Quelle::datei;
        aus.zusatz = "(line " + std::to_string(eigen->zeile) + ")";
        aus.inDatei = eigen->zeile;
    }
    return aus;
}

std::string_view quellName(Quelle q)
{
    switch (q)
    {
        case Quelle::keine: return {};
        case Quelle::sprache: return "language";
        case Quelle::datei: return "file";
        case Quelle::aussen: return "outside";
        case Quelle::projekt: return "project";
        case Quelle::abschnitt: return "section";
        case Quelle::kopf: return "header";
    }
    return {};
}

bool nennt(std::string_view text, std::string_view name)
{
    const std::vector<std::string> namen = modell::bezeichnerIn(text);
    return std::find(namen.begin(), namen.end(), name) != namen.end();
}

/// Nennt die absolute Bedingung von Zweig `z` der Weiche `w` den Namen im Text: der Zweig, die davor,
/// und so die umgebenden.
bool nenntAbsolut(std::span<const modell::Weiche> weichen, std::size_t w, std::size_t z, std::string_view name)
{
    while (w < weichen.size())
    {
        const modell::Weiche& weiche = weichen[w];
        for (std::size_t i = 0; i <= z && i < weiche.zweige.size(); ++i)
            if (nennt(weiche.zweige[i], name)) return true;
        if (weiche.eltern == modell::kKeinZweig || weiche.eltern >= w) break;
        z = weiche.elternZweig;
        w = weiche.eltern;
    }
    return false;
}

/// Die Zeilen der Tabelle (Stellen), deren Zweig etwas von `name` fordert: je Zweig alle seine
/// Alternativen, wenn eine davon es tut.
std::vector<std::size_t> betroffen(const Konfigurationstabelle& t, std::span<const modell::Weiche> weichen, std::string_view name)
{
    const auto s = std::find_if(t.spalten.begin(), t.spalten.end(), [&](const Konfigurationstabelle::Spalte& x) { return x.name == name; });
    const std::size_t spalte = s != t.spalten.end() ? static_cast<std::size_t>(s - t.spalten.begin()) : t.spalten.size();
    std::vector<std::size_t> aus;
    for (std::size_t a = 0; a < t.zeilen.size();)
    {
        std::size_t b = a + 1;
        while (b < t.zeilen.size() && t.zeilen[b].zweigIndex == t.zeilen[a].zweigIndex) ++b;
        bool ja = false;
        for (std::size_t r = a; r < b && !ja; ++r)
        {
            const Konfigurationstabelle::Zeile& z = t.zeilen[r];
            if (z.text)
                ja = nenntAbsolut(weichen, t.weiche, z.zweigIndex, name);
            else if (spalte < t.spalten.size())
                ja = z.nie || (spalte < z.werte.size() && z.werte[spalte] != kGleich);
        }
        if (ja)
            for (std::size_t r = a; r < b; ++r) aus.push_back(r);
        a = b;
    }
    return aus;
}

/// Bis wohin ein Zweig reicht: die naechste Direktive der Kette; 0 = unbekannt (das fehlende `#else`).
int bisZeile(const modell::Weiche& w, std::size_t z)
{
    if (z >= w.zweigZeilen.size()) return 0;
    return z + 1 < w.zweigZeilen.size() ? w.zweigZeilen[z + 1] : w.endZeile;
}

std::string klein(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

}  // namespace

diagramm::Schaltstellen schaltstellen(const Projekt& projekt, std::string_view name, std::span<const sprache::Definition> aussen)
{
    Schaltstellen aus;
    aus.name = std::string(name);

    // wer ihn festlegt
    const auto gleich = [&](const sprache::Definition& d) { return d.name == name; };
    for (const sprache::Definition& d : projekt.einstellungen.definitionen)
        if (gleich(d)) aus.gesetzt.push_back({"project", std::string(kProjektdatei), 0, sprache::alsText(d)});
    for (const Abschnitt& a : projekt.einstellungen.abschnitte)
        for (const sprache::Definition& d : a.definitionen)
            if (gleich(d)) aus.gesetzt.push_back({"section " + a.ordner, std::string(kProjektdatei), 0, sprache::alsText(d)});
    bool vorgegeben = false;
    for (const Datei& d : projekt.dateien)
    {
        // #define und #undef in der Reihenfolge des Texts
        std::vector<Schaltstellen::Setzung> hier;
        for (const modell::Makro& m : d.ergebnis.einheit.makros)
            if (m.name == name) hier.push_back({"#define", d.pfad, m.zeile, m.text});
        for (const modell::Aufhebung& u : d.ergebnis.einheit.aufhebungen)
            if (u.name == name) hier.push_back({"#undef", d.pfad, u.zeile, {}});
        std::stable_sort(hier.begin(), hier.end(), [](const auto& a, const auto& b) { return a.zeile < b.zeile; });
        aus.gesetzt.insert(aus.gesetzt.end(), hier.begin(), hier.end());
        if (const modell::Schalter* s = schalterVon(d.ergebnis, name); s != nullptr && s->vorgegeben) vorgegeben = true;
    }
    if (const sprache::Definition* d = vonAussen(aussen, name)) aus.gesetzt.push_back({"outside", {}, 0, sprache::alsText(*d)});
    if (vorgegeben) aus.gesetzt.push_back({"language", {}, 0, {}});

    // je Zweig, der etwas von ihm fordert, eine Zeile
    std::set<std::size_t> dateien;
    std::size_t zweige = 0;
    for (std::size_t i = 0; i < projekt.dateien.size(); ++i)
    {
        const Datei& d = projekt.dateien[i];
        const sprache::Ergebnis& e = d.ergebnis;
        if (e.weichen.empty()) continue;
        const modell::Schalter* s = schalterVon(e, name);
        const modell::Schalter schalter = s != nullptr ? *s : ohneFrage(d, name, aussen);
        for (const Konfigurationstabelle& t : diagramm::konfigurationVon(e.weichen, e.schalter))
        {
            const modell::Weiche& weiche = e.weichen[t.weiche];
            const auto s2 = std::find_if(t.spalten.begin(), t.spalten.end(), [&](const auto& x) { return x.name == name; });
            const std::size_t spalte = static_cast<std::size_t>(s2 - t.spalten.begin());
            for (const std::size_t r : betroffen(t, e.weichen, name))
            {
                const Konfigurationstabelle::Zeile& z = t.zeilen[r];
                Schaltstellen::Zeile a;
                a.pfad = d.pfad;
                a.datei = i;
                a.weiche = t.weiche;
                a.zweigIndex = z.zweigIndex;
                a.quellZeile = z.quellZeile;
                a.zweig = z.zweig;
                a.forderung = z.text ? "?" : z.nie ? "never" : spalte < z.werte.size() ? z.werte[spalte] : kGleich;
                // entschieden wird die Kette an ihrem #if: was ein Zweig darin definiert, aendert sie nicht mehr
                const Stand stand = standAn(projekt, d, schalter, aussen, weiche.zeile);
                a.stand = stand.text();
                a.inDatei = stand.inDatei;
                a.bedingung = z.bedingung;
                a.von = z.quellZeile;
                a.bis = bisZeile(weiche, z.zweigIndex);
                a.geltend = z.geltend;
                a.nie = z.nie;
                a.text = z.text;
                a.vorgegeben = s != nullptr && s->vorgegeben;
                if (z.zweig != "or") ++zweige;
                dateien.insert(i);
                aus.zeilen.push_back(std::move(a));
            }
        }
    }
    // nach Pfad, dann Zeile; die Alternativen eines Zweigs bleiben beisammen
    std::stable_sort(aus.zeilen.begin(), aus.zeilen.end(),
                     [](const Schaltstellen::Zeile& a, const Schaltstellen::Zeile& b) {
                         return a.datei != b.datei ? a.datei < b.datei : a.quellZeile < b.quellZeile;
                     });

    aus.titel = "Where is " + aus.name + " switched? " + std::to_string(zweige) + (zweige == 1 ? " branch" : " branches") +
                " in " + std::to_string(dateien.size()) + (dateien.size() == 1 ? " file" : " files");
    if (!aus.zeilen.empty() || !aus.gesetzt.empty())
        aus.hinweise.push_back("#define and #undef in branches that do not apply are not seen");
    return aus;
}

diagramm::Schalteruebersicht schalteruebersicht(const Projekt& projekt, std::span<const sprache::Definition> aussen)
{
    struct Sammlung
    {
        std::set<std::size_t> dateien;
        std::size_t ketten = 0;
        std::set<std::string> staende;
        std::set<Quelle> quellen;
        bool vorgegeben = false;
    };
    std::map<std::string, Sammlung> jeName;
    std::size_t mitKetten = 0;
    for (std::size_t i = 0; i < projekt.dateien.size(); ++i)
    {
        const Datei& d = projekt.dateien[i];
        const sprache::Ergebnis& e = d.ergebnis;
        if (e.schalter.empty()) continue;
        if (!e.weichen.empty()) ++mitKetten;
        // der Stand je Kette, die etwas von ihm fordert (wie in schaltstellen); ohne Kette der, wo zuerst gefragt
        const std::vector<Konfigurationstabelle> tabellen = diagramm::konfigurationVon(e.weichen, e.schalter);
        for (const modell::Schalter& s : e.schalter)
        {
            Sammlung& x = jeName[s.name];
            x.dateien.insert(i);
            x.vorgegeben = x.vorgegeben || s.vorgegeben;
            const auto nimm = [&](const Stand& stand) {
                x.staende.insert(stand.wert);
                if (stand.quelle != Quelle::keine) x.quellen.insert(stand.quelle);
            };
            bool mitKette = false;
            for (const Konfigurationstabelle& t : tabellen)
                if (!betroffen(t, e.weichen, s.name).empty())
                {
                    ++x.ketten;
                    mitKette = true;
                    nimm(standAn(projekt, d, s, aussen, t.quellZeile));
                }
            if (!mitKette) nimm(standVon(projekt, d, s, aussen));
        }
    }

    Schalteruebersicht aus;
    for (auto& [name, x] : jeName)
    {
        Schalteruebersicht::Zeile z;
        z.name = name;
        z.dateien = x.dateien.size();
        z.ketten = x.ketten;
        z.abweichend = x.staende.size() > 1;
        z.stand = z.abweichend ? "differs" : x.staende.empty() ? std::string() : *x.staende.begin();
        for (const Quelle q : x.quellen) z.herkunft += (z.herkunft.empty() ? "" : ", ") + std::string(quellName(q));
        z.vorgegeben = x.vorgegeben;
        aus.zeilen.push_back(std::move(z));
    }
    std::stable_sort(aus.zeilen.begin(), aus.zeilen.end(), [](const auto& a, const auto& b) {
        const std::string ka = klein(a.name), kb = klein(b.name);
        return ka != kb ? ka < kb : a.name < b.name;
    });
    aus.titel = "#if names: " + std::to_string(aus.zeilen.size()) + " in " + std::to_string(mitKetten) +
                (mitKetten == 1 ? " file" : " files");
    return aus;
}

}  // namespace c2d::projekt
