/**
 * @file ZeitAnordnung.cpp
 * @brief Wo die Balken einer Zeitachse stehen: Zeit linear auf x, je Bahn eine Zeile
 */
#include "c2d/diagramm/Anordnung.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace c2d::diagramm
{
namespace
{

constexpr double kRandAussen = 12;
constexpr double kAchse = 26;         // die Achse oben: Text und Striche
constexpr double kZeileJob = 22;      // eine Zeile der Jobs
constexpr double kZeileTask = 14;     // eine Bahn eines Platzes
constexpr double kAbstandGruppe = 6;  // zwischen den Jobs und den Executoren, zwischen zwei Executoren
constexpr double kJobBalken = 5;      // der Balken eines Jobs: oben in seiner ersten Zeile
constexpr double kMinBreite = 800;    // so breit ist die Zeit mindestens …
constexpr double kMaxBreite = 20000;  // … und hoechstens
constexpr double kPxJeSekunde = 10;   // dazwischen: so viel je Sekunde
constexpr std::size_t kTitelZeichen = 40;

std::string zeitText(long long ms, long long schritt)
{
    char puffer[32];
    if (schritt >= 60000 && ms % 60000 == 0) std::snprintf(puffer, sizeof puffer, "%lld min", ms / 60000);
    else if (schritt >= 1000) std::snprintf(puffer, sizeof puffer, "%g s", static_cast<double>(ms) / 1000);
    else std::snprintf(puffer, sizeof puffer, "%lld ms", ms);
    return puffer;
}

}  // namespace

ZeitAnordnung ordneAn(const Zeitachse& achse, const AnordnungOptionen& optionen)
{
    ZeitAnordnung a;
    const double zb = optionen.zeichenBreite;
    std::size_t zeichen = 8;
    for (const ZeitBahn& b : achse.bahnen) zeichen = std::max(zeichen, std::min(kTitelZeichen, b.titel.size()));
    const double links = kRandAussen + static_cast<double>(zeichen) * zb + 12;
    const long long dauer = std::max<long long>(1, achse.ende - achse.anfang);
    const double breite = std::clamp(static_cast<double>(dauer) / 1000.0 * kPxJeSekunde, kMinBreite, kMaxBreite);
    const double jeMs = breite / static_cast<double>(dauer);
    const auto x = [&](long long t) { return links + static_cast<double>(std::clamp(t, achse.anfang, achse.ende) - achse.anfang) * jeMs; };

    // die Bahnen untereinander; zwischen den Jobs und den Executoren und zwischen zwei Executoren etwas Luft
    double y = kRandAussen + kAchse;
    for (std::size_t i = 0; i < achse.bahnen.size(); ++i)
    {
        const ZeitBahn& b = achse.bahnen[i];
        const bool job = b.art == ZeitBahnArt::Job;
        if (i > 0 && !job && b.platz == 0) y += kAbstandGruppe;
        const double h = job ? kZeileJob : kZeileTask;
        ZeitAnordnung::Bahn z;
        z.ort = {kRandAussen, y, links - kRandAussen + breite, h};
        z.titel = b.titel.size() > kTitelZeichen ? b.titel.substr(0, kTitelZeichen - 3) + "..." : b.titel;
        z.text = {kRandAussen + 2, y + h / 2};
        z.job = job;
        if (b.entfernt > 0) z.entfernt = x(b.entfernt);
        a.bahnen.push_back(std::move(z));
        y += h;
    }
    a.zeit = {links, kRandAussen + kAchse, breite, y - kRandAussen - kAchse};

    // die Balken: ein Job oben in seiner Zeile, eine Stage darunter, ein Task fast die ganze Bahn
    for (const ZeitBalken& b : achse.balken)
    {
        const Rechteck& bahn = a.bahnen[b.bahn].ort;
        const double x0 = x(b.von);
        const double w = std::max(1.0, x(b.bis) - x0);
        Rechteck r;
        switch (b.art)
        {
            case ZeitBalkenArt::Job: r = {x0, bahn.y + 1, w, kJobBalken}; break;
            case ZeitBalkenArt::Stage:
                r = {x0, bahn.y + (a.bahnen[b.bahn].titel.empty() ? 2 : kJobBalken + 2), w,
                     bahn.hoehe - (a.bahnen[b.bahn].titel.empty() ? 4 : kJobBalken + 4)};
                break;
            case ZeitBalkenArt::Task:
            case ZeitBalkenArt::Zusammen: r = {x0, bahn.y + 1, w, bahn.hoehe - 2}; break;
        }
        a.balken.push_back(r);
        // Text nur, wo er passt (ein Job nennt sich in der Spalte links)
        std::string t;
        if (b.art != ZeitBalkenArt::Job && r.hoehe >= 10)
        {
            // ein Fehlschlag haelt am Ende Platz fuer sein ✗
            const auto passt = static_cast<std::size_t>(std::max(0.0, (w - 6 - (b.fehlgeschlagen ? 12 : 0)) / zb));
            if (passt >= 4) t = b.titel.size() <= passt ? b.titel : b.titel.substr(0, passt - 3) + "...";
        }
        a.texte.push_back(std::move(t));
    }

    // die Achse: Striche in Schritten von 1, 2, 5 × 10^k ms, etwa alle 100 px
    const double ziel = 100 / jeMs;
    long long schritt = 1;
    for (long long basis = 1; basis <= 100000000000LL; basis *= 10)
    {
        bool gefunden = false;
        for (const long long f : {1LL, 2LL, 5LL})
            if (static_cast<double>(basis * f) >= ziel)
            {
                schritt = basis * f;
                gefunden = true;
                break;
            }
        if (gefunden) break;
    }
    const double oben = kRandAussen + kAchse;
    for (long long t = 0; t <= dauer; t += schritt)
    {
        const double sx = links + static_cast<double>(t) * jeMs;
        ZeitAnordnung::Strich s;
        s.linie = {{sx, oben - 5}, {sx, y}};
        s.text = zeitText(t, schritt);
        s.textOrt = {sx, oben - 12};
        a.achse.push_back(std::move(s));
    }
    a.breite = links + breite + kRandAussen + 40;
    a.hoehe = y + kRandAussen;
    return a;
}

}  // namespace c2d::diagramm
