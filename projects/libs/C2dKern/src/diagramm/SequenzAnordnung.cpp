/**
 * @file SequenzAnordnung.cpp
 * @brief Wo die Teile eines Sequenzdiagramms stehen: Lebenslinien als Spalten, Schritte als Zeilen
 */
#include "c2d/diagramm/Anordnung.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace c2d::diagramm
{

const char* fragmentWort(FragmentArt art)
{
    switch (art)
    {
        case FragmentArt::Schleife: return "loop";
        case FragmentArt::Alternative: return "alt";
        case FragmentArt::Option: return "opt";
        case FragmentArt::Parallel: return "par";
        case FragmentArt::Abschnitt: return "section";
    }
    return "";
}

namespace
{

constexpr double kRand = 16;
constexpr double kEbene = 8;         // so weit rueckt ein inneres Fragment ein
constexpr double kTiefe = 14;        // so weit rueckt ein tieferer Aufruf ein
constexpr double kSchlaufe = 30;     // so weit reicht die Schlaufe eines Aufrufs nach rechts
constexpr double kSchlaufeHoch = 10;
constexpr double kInsLeere = 60;     // so lang ist ein Pfeil ohne Empfaenger
constexpr double kUeberstand = 24;   // so weit reicht eine Barriere ueber ihre aeusseren Linien
constexpr double kMinSpalte = 160;

/// Zeichen in UTF-8 (je Zeichen ein Byte ohne `10xxxxxx`).
std::size_t zeichen(const std::string& s)
{
    return static_cast<std::size_t>(std::count_if(s.begin(), s.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

/// Auf `n` Zeichen gekuerzt, mit `…` am Ende; 0 = nie.
std::string kurz(const std::string& s, std::size_t n)
{
    if (n == 0 || zeichen(s) <= n) return s;
    std::string aus;
    std::size_t z = 0;
    for (const char c : s)
    {
        const bool anfang = (static_cast<unsigned char>(c) & 0xC0) != 0x80;
        if (anfang && ++z > n - 1) break;
        aus.push_back(c);
    }
    return aus + "\xE2\x80\xA6";
}

}  // namespace

SequenzAnordnung ordneAn(const Sequenzdiagramm& d, const AnordnungOptionen& o)
{
    SequenzAnordnung a;
    const double cw = o.zeichenBreite;
    const double zh = o.zeilenHoehe;
    const std::size_t n = d.linien.size();
    const auto breiteVon = [&](const std::string& s) { return static_cast<double>(zeichen(s)) * cw; };
    const auto text = [&](const std::string& s) { return kurz(s, o.maxZeilenLaenge); };

    // die tiefste Verschachtelung der Fragmente (fuer den Rand) und was rechts einer Linie Platz braucht
    std::size_t ebenen = 0;
    {
        std::size_t e = 0;
        for (const SequenzSchritt& s : d.schritte)
        {
            if (s.art == SequenzSchritt::Art::FragmentAnfang) ebenen = std::max(ebenen, ++e);
            if (s.art == SequenzSchritt::Art::FragmentEnde && e > 0) --e;
        }
    }
    const double rand = kRand + static_cast<double>(ebenen) * kEbene + 8;
    std::vector<double> halb(n, 45);
    std::vector<double> rechts(n, 0);
    std::vector<double> luecke(n > 0 ? n - 1 : 0, kMinSpalte);
    for (std::size_t k = 0; k < n; ++k) halb[k] = std::max(45.0, (breiteVon(d.linien[k].name) + 24) / 2);
    double titelBreite = 0;
    for (const SequenzSchritt& s : d.schritte)
    {
        if (s.art == SequenzSchritt::Art::FragmentAnfang || s.art == SequenzSchritt::Art::Sonst)
            titelBreite = std::max(titelBreite, breiteVon(text(s.text)) + 80);
        if (s.art != SequenzSchritt::Art::Nachricht && s.art != SequenzSchritt::Art::Barriere) continue;
        if (s.von >= n) continue;
        const double t = breiteVon(text(s.text));
        if (s.art == SequenzSchritt::Art::Nachricht && s.von == s.nach)
        {
            const double d0 = static_cast<double>(std::max(s.tiefe - 1, 0)) * kTiefe;
            rechts[s.von] = std::max(rechts[s.von], d0 + kSchlaufe + 6 + t + 10);
            continue;
        }
        if (s.art == SequenzSchritt::Art::Nachricht && s.nach >= n)
        {
            rechts[s.von] = std::max(rechts[s.von], std::max(kInsLeere, t) + 16);
            continue;
        }
        std::size_t links = s.von;
        std::size_t r = s.von;
        if (s.art == SequenzSchritt::Art::Nachricht) r = s.nach;
        for (const std::size_t b : s.beteiligt) r = std::max(r, b), links = std::min(links, b);
        if (r < links) std::swap(r, links);
        if (r > links) luecke[links] = std::max(luecke[links], t + 40 - static_cast<double>(r - links - 1) * kMinSpalte);
        else rechts[links] = std::max(rechts[links], t / 2 + kUeberstand);
    }
    for (std::size_t k = 0; k + 1 < n; ++k) luecke[k] = std::max({luecke[k], halb[k] + halb[k + 1] + 30, rechts[k] + 20});

    // die Spalten
    std::vector<double> x(n, 0);
    double xk = rand + (n > 0 ? halb[0] : 0);
    for (std::size_t k = 0; k < n; ++k)
    {
        x[k] = xk;
        if (k + 1 < n) xk += luecke[k];
    }
    a.breite = n > 0 ? x[n - 1] + std::max(halb[n - 1], rechts[n - 1]) + rand : 2 * rand;
    a.breite = std::max(a.breite, 2 * kRand + titelBreite);
    for (const std::string& h : d.hinweise) a.breite = std::max(a.breite, 2 * kRand + breiteVon(h));

    const double kopfHoehe = zh + 12;
    for (std::size_t k = 0; k < n; ++k) a.koepfe.push_back({{x[k] - halb[k], kRand, 2 * halb[k], kopfHoehe}, d.linien[k].name});

    double y = kRand + kopfHoehe + 16;
    std::vector<std::size_t> offen;  // Stellen in a.fragmente
    for (std::size_t i = 0; i < d.schritte.size(); ++i)
    {
        const SequenzSchritt& s = d.schritte[i];
        switch (s.art)
        {
            case SequenzSchritt::Art::FragmentAnfang:
            {
                const double e = static_cast<double>(offen.size());
                SequenzAnordnung::Fragment f;
                f.ort = {kRand + e * kEbene, y, a.breite - 2 * (kRand + e * kEbene), 0};
                f.wort = fragmentWort(s.fragment);
                f.schild = {f.ort.x, y, breiteVon(f.wort) + 16, zh + 4};
                f.titel = s.text.empty() ? std::string() : "[" + text(s.text) + "]";
                f.titelOrt = {f.schild.x + f.schild.breite + 8, y + f.schild.hoehe / 2};
                f.schritt = i;
                offen.push_back(a.fragmente.size());
                a.fragmente.push_back(std::move(f));
                y += zh + 12;
                break;
            }
            case SequenzSchritt::Art::Sonst:
            {
                if (offen.empty()) break;
                SequenzAnordnung::Fragment& f = a.fragmente[offen.back()];
                f.zweige.push_back({{{f.ort.x, y}, {f.ort.x + f.ort.breite, y}}, "[" + text(s.text) + "]", {f.ort.x + 8, y + zh / 2 + 2}});
                y += zh + 8;
                break;
            }
            case SequenzSchritt::Art::FragmentEnde:
            {
                if (offen.empty()) break;
                SequenzAnordnung::Fragment& f = a.fragmente[offen.back()];
                f.ort.hoehe = y + 4 - f.ort.y;
                offen.pop_back();
                y += 12;
                break;
            }
            case SequenzSchritt::Art::Nachricht:
            {
                if (s.von >= n) break;
                SequenzAnordnung::Pfeil p;
                p.art = s.nachricht;
                p.fremd = s.fremd;
                p.text = text(s.text);
                p.schritt = i;
                const double t = breiteVon(p.text);
                if (s.von == s.nach)
                {
                    const double x0 = x[s.von] + static_cast<double>(std::max(s.tiefe - 1, 0)) * kTiefe;
                    p.linie = {{x0, y + 4}, {x0 + kSchlaufe, y + 4}, {x0 + kSchlaufe, y + 4 + kSchlaufeHoch}, {x0 + 2, y + 4 + kSchlaufeHoch}};
                    p.textOrt = {x0 + kSchlaufe + 6, y + 4 + kSchlaufeHoch / 2};
                    p.treffer = {x0, y, kSchlaufe + 6 + t, kSchlaufeHoch + 8};
                    y += zh + 10;
                }
                else
                {
                    p.insLeere = s.nach >= n;
                    const double x1 = x[s.von];
                    const double x2 = p.insLeere ? x1 + kInsLeere : x[s.nach];
                    const double yl = y + zh + 2;
                    p.linie = {{x1, yl}, {x2, yl}};
                    const double links = std::min(x1, x2);
                    const double weit = std::abs(x2 - x1);
                    p.textOrt = {p.insLeere ? x1 + 4 : links + std::max(4.0, (weit - t) / 2), y + zh / 2};
                    p.treffer = {std::min(links, p.textOrt.x), y, std::max(weit, t + 8), zh + 6};
                    y += zh + 14;
                }
                a.pfeile.push_back(std::move(p));
                break;
            }
            case SequenzSchritt::Art::Barriere:
            {
                if (s.beteiligt.empty() || s.beteiligt.front() >= n) break;
                double links = x[s.beteiligt.front()];
                double r = links;
                for (const std::size_t b : s.beteiligt)
                    if (b < n) links = std::min(links, x[b]), r = std::max(r, x[b]);
                links -= kUeberstand;
                r += kUeberstand;
                SequenzAnordnung::Barriere b;
                b.text = text(s.text);
                const double yl = y + zh + 2;
                b.linie = {{links, yl}, {r, yl}};
                b.textOrt = {(links + r) / 2, y + zh / 2};
                b.treffer = {std::min(links, b.textOrt.x - breiteVon(b.text) / 2), y, std::max(r - links, breiteVon(b.text)), zh + 6};
                b.schritt = i;
                a.barrieren.push_back(std::move(b));
                y += zh + 14;
                break;
            }
        }
    }
    // offen gebliebene Fragmente schliessen
    while (!offen.empty())
    {
        a.fragmente[offen.back()].ort.hoehe = y + 4 - a.fragmente[offen.back()].ort.y;
        offen.pop_back();
        y += 12;
    }
    for (std::size_t k = 0; k < n; ++k) a.lebenslinien.push_back({{x[k], kRand + kopfHoehe}, {x[k], y + 8}});
    y += 24;
    for (const std::string& h : d.hinweise)
    {
        a.hinweise.emplace_back(h, Punkt{kRand, y});
        y += zh;
    }
    a.hoehe = y + kRand;
    return a;
}

}  // namespace c2d::diagramm
