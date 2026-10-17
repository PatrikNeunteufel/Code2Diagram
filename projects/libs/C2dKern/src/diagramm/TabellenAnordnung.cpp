/**
 * @file TabellenAnordnung.cpp
 * @brief Wahrheitstabellen angeordnet: Spalten nach ihrem breitesten Text, Tabellen untereinander
 */
#include "c2d/diagramm/Anordnung.hpp"

#include <algorithm>

namespace c2d::diagramm
{

namespace
{

constexpr double kRand = 24;    // um alles, wie in den anderen Anordnungen
constexpr double kLuft = 8;     // links und rechts in einer Zelle
constexpr double kAbstand = 2;  // zwischen zwei Tabellen: so viele Zeilen

/// Hoechstens `n` Zeichen (Bytes); laengeres endet auf "...", ohne ein UTF-8-Zeichen zu trennen.
std::string gekuerzt(const std::string& text, std::size_t n)
{
    if (n == 0 || text.size() <= n) return text;
    std::size_t ende = n > 3 ? n - 3 : 0;
    while (ende > 0 && (static_cast<unsigned char>(text[ende]) & 0xC0) == 0x80) --ende;
    return text.substr(0, ende) + "...";
}

/// Zeichen, nicht Bytes: `–` ist eines.
std::size_t zeichen(const std::string& text)
{
    std::size_t n = 0;
    for (const char c : text)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

std::string wert(Wert w)
{
    switch (w)
    {
        case Wert::Wahr: return "T";
        case Wert::Falsch: return "F";
        case Wert::Egal: return "\xE2\x80\x93";  // Halbgeviertstrich: nicht ausgewertet
    }
    return {};
}

}  // namespace

TabellenAnordnung ordneAn(std::span<const Wahrheitstabelle> tabellen, const AnordnungOptionen& o)
{
    TabellenAnordnung a;
    const double zeile = o.zeilenHoehe + 6;
    const auto weite = [&](std::size_t n) { return static_cast<double>(n) * o.zeichenBreite + 2 * kLuft; };
    double y = kRand;
    for (std::size_t ti = 0; ti < tabellen.size(); ++ti)
    {
        const Wahrheitstabelle& t = tabellen[ti];
        const auto text = [&](const std::string& s, bool fett) {
            a.texte.push_back({{kRand, y + o.zeilenHoehe / 2}, s, fett, ti});
            a.breite = std::max(a.breite, kRand + weite(zeichen(s)) + kRand);
            y += o.zeilenHoehe + 2;
        };
        text(t.funktion + "(): line " + std::to_string(t.quellZeile), true);
        text(gekuerzt(t.bedingung, o.maxZeilenLaenge == 0 ? 0 : 2 * o.maxZeilenLaenge), false);
        y += 4;

        if (!t.atome.empty() && !t.zeilen.empty())
        {
            // die Spalten: #, je Atom, Result, MC/DC — so breit wie ihr breitester Text
            std::vector<std::vector<std::string>> kopf;
            std::vector<std::string> ganz;
            kopf.push_back({"#"});
            ganz.emplace_back();
            for (std::size_t i = 0; i < t.atome.size(); ++i)
            {
                const Wahrheitstabelle::Atom& atom = t.atome[i];
                kopf.push_back({atomKennung(i) + (atom.mitAufruf ? " (call)" : ""), gekuerzt(atom.text, kAtomZeichen)});
                ganz.push_back(atom.text);
            }
            kopf.push_back({"Result"});
            kopf.push_back({"MC/DC"});
            ganz.emplace_back();
            ganz.emplace_back();

            // je Zeile: welche Atome ihr Paar in ihr haben
            const auto paareIn = [&](std::size_t nummer) {
                std::string aus;
                for (std::size_t i = 0; i < t.paare.size(); ++i)
                    if (t.paare[i].first == nummer || t.paare[i].second == nummer)
                        aus += (aus.empty() ? "" : ", ") + atomKennung(i);
                return aus;
            };
            std::vector<std::vector<std::string>> zeilen;
            for (const Wahrheitstabelle::Zeile& z : t.zeilen)
            {
                std::vector<std::string> r{std::to_string(z.nummer)};
                for (const Wert w : z.werte) r.push_back(wert(w));
                r.push_back(z.ergebnis ? "T" : "F");
                r.push_back(paareIn(z.nummer));
                zeilen.push_back(std::move(r));
            }
            std::vector<double> breiten(kopf.size(), 0);
            for (std::size_t s = 0; s < kopf.size(); ++s)
            {
                for (const std::string& k : kopf[s]) breiten[s] = std::max(breiten[s], weite(zeichen(k)));
                for (const std::vector<std::string>& r : zeilen) breiten[s] = std::max(breiten[s], weite(zeichen(r[s])));
            }
            const double kopfHoehe = 2 * o.zeilenHoehe + 8;
            double x = kRand;
            for (std::size_t s = 0; s < kopf.size(); ++s)
            {
                a.zellen.push_back({{x, y, breiten[s], kopfHoehe}, kopf[s], true, false, ganz[s], ti, ti});
                double unten = y + kopfHoehe;
                for (std::size_t r = 0; r < zeilen.size(); ++r)
                {
                    a.zellen.push_back({{x, unten, breiten[s], zeile}, {zeilen[r][s]}, false, t.zeilen[r].mcdc, {}, ti, ti});
                    unten += zeile;
                }
                x += breiten[s];
            }
            // ein Klick in die Tabelle meint die ganze (ihre Bedingung)
            a.knotenOrte.push_back({kRand, y, x - kRand, kopfHoehe + zeile * static_cast<double>(zeilen.size())});
            a.breite = std::max(a.breite, x + kRand);
            y += kopfHoehe + zeile * static_cast<double>(zeilen.size()) + 6;

            // die Paare in einer Zeile: A 2-6, B 3-7
            std::string paare;
            for (std::size_t i = 0; i < t.paare.size(); ++i)
                if (t.paare[i].first != 0)
                    paare += (paare.empty() ? "" : ", ") + atomKennung(i) + " " + std::to_string(t.paare[i].first) + "-" +
                             std::to_string(t.paare[i].second);
            if (!paare.empty()) text("MC/DC pairs (unique-cause): " + paare, false);
        }
        for (const std::string& h : t.hinweise) text(h, false);
        if (ti + 1 < tabellen.size()) y += kAbstand * o.zeilenHoehe;
    }
    a.hoehe = y + kRand;
    return a;
}

TabellenAnordnung ordneAn(std::span<const Zuordnung> tabellen, const AnordnungOptionen& o)
{
    TabellenAnordnung a;
    const double zeile = o.zeilenHoehe + 6;
    const auto weite = [&](std::size_t n) { return static_cast<double>(n) * o.zeichenBreite + 2 * kLuft; };
    double y = kRand;
    std::size_t knoten = 0;
    for (std::size_t ti = 0; ti < tabellen.size(); ++ti)
    {
        const Zuordnung& t = tabellen[ti];
        a.texte.push_back({{kRand, y + o.zeilenHoehe / 2}, gekuerzt(t.titel, o.maxZeilenLaenge == 0 ? 0 : 2 * o.maxZeilenLaenge), true, ti});
        a.breite = std::max(a.breite, kRand + weite(zeichen(a.texte.back().text)) + kRand);
        y += o.zeilenHoehe + 6;

        // die Spalten: der Schluessel, dann je Wert eine — so breit wie ihr breitester Text
        std::vector<std::string> kopf{gekuerzt(t.schluessel, o.maxZeilenLaenge)};
        for (const std::string& s : t.spalten) kopf.push_back(gekuerzt(s, o.maxZeilenLaenge));
        std::vector<double> breiten;
        for (const std::string& k : kopf) breiten.push_back(weite(zeichen(k)));
        std::vector<std::vector<std::string>> inhalt;
        for (const Zuordnung::Zeile& z : t.zeilen)
        {
            std::vector<std::string> r{gekuerzt(z.schluessel, o.maxZeilenLaenge)};
            for (const std::string& w : z.werte) r.push_back(gekuerzt(w, o.maxZeilenLaenge));
            for (std::size_t s = 0; s < r.size() && s < breiten.size(); ++s) breiten[s] = std::max(breiten[s], weite(zeichen(r[s])));
            inhalt.push_back(std::move(r));
        }
        double x = kRand;
        for (std::size_t s = 0; s < kopf.size(); ++s)
        {
            a.zellen.push_back({{x, y, breiten[s], zeile}, {kopf[s]}, true, false, s == 0 && kopf[s] != t.schluessel ? t.schluessel : "",
                                ti, kKeine, s == 0});
            x += breiten[s];
        }
        const double rechts = x;
        y += zeile;
        for (std::size_t r = 0; r < inhalt.size(); ++r)
        {
            double xr = kRand;
            for (std::size_t s = 0; s < inhalt[r].size() && s < breiten.size(); ++s)
            {
                const std::string ganz = s == 0 ? t.zeilen[r].schluessel : t.zeilen[r].werte[s - 1];
                a.zellen.push_back({{xr, y, breiten[s], zeile}, {inhalt[r][s]}, false, false, ganz != inhalt[r][s] ? ganz : "",
                                    ti, knoten, true});
                xr += breiten[s];
            }
            a.knotenOrte.push_back({kRand, y, rechts - kRand, zeile});
            ++knoten;
            y += zeile;
        }
        a.breite = std::max(a.breite, rechts + kRand);
        if (ti + 1 < tabellen.size()) y += kAbstand * o.zeilenHoehe;
    }
    a.hoehe = y + kRand;
    return a;
}

TabellenAnordnung ordneAn(std::span<const Konfigurationstabelle> tabellen, const AnordnungOptionen& o)
{
    constexpr double kEinzug = 24;  // je Stufe, die eine Kette in einer anderen steht
    TabellenAnordnung a;
    const double zeile = o.zeilenHoehe + 6;
    const auto weite = [&](std::size_t n) { return static_cast<double>(n) * o.zeichenBreite + 2 * kLuft; };
    double y = kRand;
    std::size_t knoten = 0;
    for (std::size_t ti = 0; ti < tabellen.size(); ++ti)
    {
        const Konfigurationstabelle& t = tabellen[ti];
        const double x0 = kRand + kEinzug * static_cast<double>(t.tiefe);
        const auto text = [&](const std::string& s, bool fett) {
            a.texte.push_back({{x0, y + o.zeilenHoehe / 2}, s, fett, ti});
            a.breite = std::max(a.breite, x0 + weite(zeichen(s)) + kRand);
            y += o.zeilenHoehe + 6;
        };
        text(gekuerzt(t.titel, o.maxZeilenLaenge == 0 ? 0 : 2 * o.maxZeilenLaenge), true);

        // der Kopf: Zweig, je Name sein Stand (und wo die Datei ihn definiert), active
        std::vector<std::vector<std::string>> kopf{{"branch"}};
        for (const Konfigurationstabelle::Spalte& s : t.spalten)
        {
            std::vector<std::string> k{gekuerzt(s.name, o.maxZeilenLaenge)};
            if (!s.jetzt.empty()) k.push_back(gekuerzt(s.jetzt, o.maxZeilenLaenge));
            if (s.inDatei > 0) k.push_back("(line " + std::to_string(s.inDatei) + ")");
            if (s.vorgegeben) k.push_back("(language)");
            kopf.push_back(std::move(k));
        }
        kopf.push_back({"active"});
        std::size_t kopfZeilen = 1;
        for (const auto& k : kopf) kopfZeilen = std::max(kopfZeilen, k.size());
        const double kopfHoehe = static_cast<double>(kopfZeilen) * o.zeilenHoehe + 6;

        std::vector<double> breiten;
        for (const auto& k : kopf)
        {
            double b = 0;
            for (const std::string& s : k) b = std::max(b, weite(zeichen(s)));
            breiten.push_back(b);
        }
        const auto aktiv = [](const Konfigurationstabelle::Zeile& r) {
            return std::string(r.geltend ? "\xE2\x9C\x93" : r.nie ? "never" : "");  // Haekchen
        };
        std::vector<std::vector<std::string>> inhalt;
        for (const Konfigurationstabelle::Zeile& r : t.zeilen)
        {
            std::vector<std::string> z{gekuerzt(r.zweig, o.maxZeilenLaenge)};
            for (const std::string& w : r.werte) z.push_back(gekuerzt(w, o.maxZeilenLaenge));
            z.push_back(aktiv(r));
            for (std::size_t s = 0; s < z.size() && s < breiten.size(); ++s) breiten[s] = std::max(breiten[s], weite(zeichen(z[s])));
            inhalt.push_back(std::move(z));
        }
        const auto grau = [&](std::size_t s) {
            return s > 0 && s <= t.spalten.size() && (t.spalten[s - 1].inDatei > 0 || t.spalten[s - 1].vorgegeben);
        };

        double x = x0;
        for (std::size_t s = 0; s < kopf.size(); ++s)
        {
            TabellenAnordnung::Zelle z{{x, y, breiten[s], kopfHoehe}, kopf[s], true, false, {}, ti, kKeine, s == 0};
            z.grau = grau(s);
            a.zellen.push_back(std::move(z));
            x += breiten[s];
        }
        const double rechts = x;
        y += kopfHoehe;
        for (std::size_t r = 0; r < inhalt.size(); ++r)
        {
            const Konfigurationstabelle::Zeile& zr = t.zeilen[r];
            double xr = x0;
            for (std::size_t s = 0; s < inhalt[r].size() && s < breiten.size(); ++s)
            {
                const std::string ganz = s == 0 ? zr.bedingung : "";
                TabellenAnordnung::Zelle z{{xr, y, breiten[s], zeile}, {inhalt[r][s]}, false, zr.geltend,
                                           ganz != inhalt[r][s] ? ganz : "", ti, knoten, s == 0};
                z.grau = grau(s);
                a.zellen.push_back(std::move(z));
                xr += breiten[s];
            }
            a.knotenOrte.push_back({x0, y, rechts - x0, zeile});
            ++knoten;
            y += zeile;
        }
        a.breite = std::max(a.breite, rechts + kRand);
        if (!t.hinweise.empty()) y += 4;
        for (const std::string& h : t.hinweise) text(h, false);
        if (ti + 1 < tabellen.size()) y += kAbstand * o.zeilenHoehe;
    }
    a.hoehe = y + kRand;
    return a;
}

TabellenAnordnung ordneAn(const Entscheidungstabelle& t, const AnordnungOptionen& o)
{
    TabellenAnordnung a;
    const double zeile = o.zeilenHoehe + 6;
    const auto weite = [&](std::size_t n) { return static_cast<double>(n) * o.zeichenBreite + 2 * kLuft; };
    double y = kRand;
    const auto text = [&](const std::string& s, bool fett) {
        a.texte.push_back({{kRand, y + o.zeilenHoehe / 2}, s, fett, 0});
        a.breite = std::max(a.breite, kRand + weite(zeichen(s)) + kRand);
        y += o.zeilenHoehe + 2;
    };
    text(t.funktion + "(): decision table", true);
    y += 4;

    if (!t.regeln.empty())
    {
        // die erste Spalte: Bedingungen und Aktionen, gekuerzt wie ein Knotentext
        std::vector<std::string> links;
        std::vector<std::string> ganz;
        for (const Entscheidungstabelle::Bedingung& b : t.bedingungen)
        {
            links.push_back(gekuerzt(zeilenText(b), o.maxZeilenLaenge));
            ganz.push_back(zeilenText(b));
        }
        for (const Entscheidungstabelle::Aktion& ak : t.aktionen)
        {
            links.push_back(gekuerzt(ak.text, o.maxZeilenLaenge));
            ganz.push_back(ak.text);
        }
        const std::string kopfText = "Conditions / actions";
        double erste = weite(zeichen(kopfText));
        for (const std::string& l : links) erste = std::max(erste, weite(zeichen(l)));
        // je Regel eine Spalte: so breit wie ihr breitester Eintrag
        std::vector<double> breiten;
        for (std::size_t r = 0; r < t.regeln.size(); ++r)
        {
            double b = weite(zeichen("R" + std::to_string(r + 1)));
            for (const std::string& e : t.regeln[r].eintraege) b = std::max(b, weite(zeichen(e)));
            breiten.push_back(b);
        }
        double rechts = kRand + erste;
        for (const double b : breiten) rechts += b;

        // der Kopf: links die Ueberschrift, dann R1 … Rn
        a.zellen.push_back({{kRand, y, erste, zeile}, {kopfText}, true, false, {}, 0, kKeine, true});
        double x = kRand + erste;
        for (std::size_t r = 0; r < breiten.size(); ++r)
        {
            a.zellen.push_back({{x, y, breiten[r], zeile}, {"R" + std::to_string(r + 1)}, true, false, {}, 0});
            x += breiten[r];
        }
        y += zeile;

        const std::size_t nb = t.bedingungen.size();
        const auto reihe = [&](std::size_t k, const std::vector<std::string>& eintraege) {
            a.zellen.push_back({{kRand, y, erste, zeile}, {links[k]}, false, false, ganz[k] != links[k] ? ganz[k] : std::string(),
                                0, k, true});
            double xr = kRand + erste;
            for (std::size_t r = 0; r < eintraege.size(); ++r)
            {
                a.zellen.push_back({{xr, y, breiten[r], zeile}, {eintraege[r]}, false, false, {}, 0, k});
                xr += breiten[r];
            }
            a.knotenOrte.push_back({kRand, y, rechts - kRand, zeile});
            y += zeile;
        };
        for (std::size_t b = 0; b < nb; ++b)
        {
            std::vector<std::string> e;
            for (const Entscheidungstabelle::Regel& r : t.regeln) e.push_back(r.eintraege[b].empty() ? "\xE2\x80\x93" : r.eintraege[b]);
            reihe(b, e);
        }
        // DIN 66241: eine doppelte Linie zwischen Bedingungs- und Aktionsteil
        a.linien.push_back({{kRand, y}, {rechts, y}});
        y += 3;
        a.linien.push_back({{kRand, y}, {rechts, y}});
        for (std::size_t k = 0; k < t.aktionen.size(); ++k)
        {
            std::vector<std::string> e;
            for (const Entscheidungstabelle::Regel& r : t.regeln) e.emplace_back(r.aktionen[k] ? "X" : "");
            reihe(nb + k, e);
        }
        a.breite = std::max(a.breite, rechts + kRand);
        y += 6;
    }
    for (const std::string& h : t.hinweise) text(h, false);
    a.hoehe = y + kRand;
    return a;
}

}  // namespace c2d::diagramm
