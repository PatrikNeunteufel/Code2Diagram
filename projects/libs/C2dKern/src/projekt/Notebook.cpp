/**
 * @file Notebook.cpp
 * @brief Ein Notebook in Texte zerlegen, die die Leser lesen
 */
#include "Notebook.hpp"

#include "Json.hpp"
#include "Zeilen.hpp"

#include <algorithm>

namespace c2d::projekt
{

namespace
{

using quelle::Meldung;
using quelle::Schwere;

/// Wie Notebooks und Magie eine Sprache nennen (klein geschrieben), und wie sie im Projekt heisst.
struct SprachWort
{
    std::string_view wort;
    std::string_view sprache;
};

constexpr SprachWort kSprachWoerter[] = {
    {"python", "Python"},       {"python3", "Python"},    {"ipython", "Python"},  {"pyspark", "Python"},
    {"javascript", "JavaScript"}, {"js", "JavaScript"},   {"node", "JavaScript"}, {"typescript", "TypeScript"},
    {"ts", "TypeScript"},       {"sql", "SQL"},           {"sparksql", "SQL"},    {"csharp", "C#"},
    {"c#", "C#"},               {"java", "Java"},         {"kotlin", "Kotlin"},   {"c++", "C++"},
    {"cpp", "C++"},             {"c++17", "C++"},         {"c++20", "C++"},       {"c", "C"},
};

/// Zellmagie, nach der die Zelle in der Sprache des Notebooks bleibt (sie misst oder faengt nur).
constexpr std::string_view kZellmagieOhneWechsel[] = {"time", "timeit", "capture", "prun", "debug"};

std::string klein(std::string_view text)
{
    std::string aus(text);
    for (char& c : aus)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return aus;
}

bool leer(std::string_view zeile)
{
    return zeile.find_first_not_of(" \t\r") == std::string_view::npos;
}

std::size_t einzug(std::string_view zeile)
{
    const std::size_t i = zeile.find_first_not_of(" \t");
    return i == std::string_view::npos ? zeile.size() : i;
}

/// Die Zeilen eines Texts ohne `\r`; ein Zeilenwechsel am Ende gibt keine leere Zeile dazu.
std::vector<std::string_view> zeilenVon(std::string_view text)
{
    std::vector<std::string_view> aus;
    std::size_t a = 0;
    while (a < text.size())
    {
        std::size_t e = text.find('\n', a);
        if (e == std::string_view::npos) e = text.size();
        std::string_view z = text.substr(a, e - a);
        if (!z.empty() && z.back() == '\r') z.remove_suffix(1);
        aus.push_back(z);
        a = e + 1;
    }
    return aus;
}

/// Das erste Wort ab `ab` (bis Leerraum).
std::string_view wortAb(std::string_view text, std::size_t ab)
{
    if (ab >= text.size()) return {};
    const std::size_t e = text.find_first_of(" \t", ab);
    return text.substr(ab, (e == std::string_view::npos ? text.size() : e) - ab);
}

const sprache::Sprache* spracheNamens(std::string_view wort)
{
    const std::string w = klein(wort);
    for (const SprachWort& s : kSprachWoerter)
        if (s.wort == w)
            for (const sprache::Sprache* sp : sprache::sprachen())
                if (sp->name() == s.sprache) return sp;
    return nullptr;
}

std::string kommentarVon(const sprache::Sprache& s)
{
    const std::string_view k = s.tabelle().zeilenKommentar;
    return std::string(k.empty() ? std::string_view("#") : k);
}

const json::Wert* feld(const json::Wert* w, std::string_view name)
{
    if (w == nullptr || w->art != json::Art::Objekt) return nullptr;
    for (std::size_t i = 0; i < w->namen.size(); ++i)
        if (w->namen[i] == name) return &w->elemente[i];
    return nullptr;
}

/// Eine Zeichenkette — oder eine Liste von Zeichenketten aneinander (`source` einer Zelle).
std::string textVon(const json::Wert* w)
{
    if (w == nullptr) return {};
    if (w->art == json::Art::Text) return w->text;
    std::string aus;
    if (w->art == json::Art::Liste)
        for (const json::Wert& e : w->elemente)
            if (e.art == json::Art::Text) aus += e.text;
    return aus;
}

/// Der Text einer Ausgabe in HTML, wie Databricks ihn exportiert (`<div class="ansiout">…</div>`): ohne Tags, die
/// Entitaeten aufgeloest; leer = kein solcher Block.
std::string ansiText(std::string_view html)
{
    constexpr std::string_view kAuf = "<div class=\"ansiout\">";
    const std::size_t a = html.find(kAuf);
    if (a == std::string_view::npos) return {};
    std::string_view s = html.substr(a + kAuf.size());
    s = s.substr(0, s.rfind("</div>"));
    constexpr std::pair<std::string_view, char> kEntitaeten[] = {
        {"&lt;", '<'}, {"&gt;", '>'}, {"&amp;", '&'}, {"&quot;", '"'}, {"&#39;", '\''}, {"&#x27;", '\''}, {"&nbsp;", ' '}};
    std::string aus;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '<')
        {
            const std::size_t zu = s.find('>', i);
            if (zu == std::string_view::npos) break;
            i = zu;
            continue;
        }
        if (s[i] == '&')
        {
            const auto e = std::find_if(std::begin(kEntitaeten), std::end(kEntitaeten),
                                        [&](const auto& x) { return s.substr(i).starts_with(x.first); });
            if (e != std::end(kEntitaeten))
            {
                aus.push_back(e->second);
                i += e->first.size() - 1;
                continue;
            }
        }
        aus.push_back(s[i]);
    }
    return aus;
}

/// Eine fremde Zelle, waehrend das Skript waechst: ihre Zeilen.
struct FremdeZeilen
{
    const sprache::Sprache* sprache = nullptr;
    std::size_t zelle = 0;
    std::vector<std::pair<int, std::string>> zeilen;  ///< Zeile im Skript, was dort steht
    std::vector<std::size_t> abzug;                   ///< je Eintrag von `zeilen`
};

/// Je fremde Zelle ihr Text: so viele Zeilen wie das Skript, nur ihre tragen etwas.
std::vector<FremdeZelle> alsTexte(const std::vector<FremdeZeilen>& fremde, int zeilen)
{
    std::vector<FremdeZelle> aus;
    for (const FremdeZeilen& f : fremde)
    {
        std::vector<std::string> je(static_cast<std::size_t>(zeilen));
        FremdeZelle z;
        z.sprache = f.sprache;
        z.zelle = f.zelle;
        z.abzug.assign(static_cast<std::size_t>(zeilen), 0);
        for (std::size_t i = 0; i < f.zeilen.size(); ++i)
        {
            const auto k = static_cast<std::size_t>(f.zeilen[i].first - 1);
            if (k >= je.size()) continue;
            je[k] = f.zeilen[i].second;
            z.abzug[k] = f.abzug[i];
        }
        for (const std::string& t : je) z.text += t + "\n";
        aus.push_back(std::move(z));
    }
    return aus;
}

/// Was der Hinweis zu einer Zeile mit Magie sagt (`%pip install x`, `!ls`, `%run x`).
std::string magieHinweis(std::string_view zeile)
{
    if (zeile.starts_with("!")) return "shell command is not read";
    const std::string_view wort = wortAb(zeile, 0);
    if (wort == "%run") return "%run is not followed";
    return "IPython magic " + std::string(wort) + " is not read";
}

}  // namespace

std::string zellenName(std::size_t nummer)
{
    return "<cell " + std::to_string(nummer) + ">";
}

std::optional<Notebook> ausIpynb(std::string_view text, Meldung& fehler)
{
    json::Wert nb;
    if (!json::lies(text, nb, fehler)) return std::nullopt;
    if (nb.art != json::Art::Objekt)
    {
        fehler = {Schwere::Fehler, nb.ort, "not a notebook: no JSON object"};
        return std::nullopt;
    }
    if (const json::Wert* format = feld(&nb, "nbformat");
        format != nullptr && format->art == json::Art::Zahl && format->text < "4" && format->text.size() == 1)
    {
        fehler = {Schwere::Fehler, format->ort, "notebook format " + format->text + " is not read (only format 4)"};
        return std::nullopt;
    }
    const json::Wert* zellen = feld(&nb, "cells");
    if (zellen == nullptr || zellen->art != json::Art::Liste)
    {
        fehler = {Schwere::Fehler, nb.ort, "not a notebook: no \"cells\""};
        return std::nullopt;
    }

    // die Sprache: language_info.name, sonst kernelspec.language, sonst kernelspec.name
    const json::Wert* meta = feld(&nb, "metadata");
    std::string wort;
    quelle::Position wo = nb.ort;
    for (const auto& [teil, name] : {std::pair{"language_info", "name"}, std::pair{"kernelspec", "language"},
                                     std::pair{"kernelspec", "name"}})
    {
        const json::Wert* w = feld(feld(meta, teil), name);
        if (w == nullptr || w->art != json::Art::Text || w->text.empty()) continue;
        wort = w->text;
        wo = w->ort;
        break;
    }
    Notebook n;
    n.sprache = wort.empty() ? spracheNamens("python") : spracheNamens(wort);
    if (n.sprache == nullptr)
    {
        fehler = {Schwere::Fehler, wo, "notebook in " + wort + ": no reader for this language"};
        return std::nullopt;
    }
    if (wort.empty())
        n.meldungen.push_back({Schwere::Hinweis, {0, 1, 1}, "the notebook names no language - read as Python"});

    const std::string k = kommentarVon(*n.sprache);
    int zeile = 0;  // zuletzt geschrieben
    const auto schreibe = [&](std::string_view imSkript, std::string_view zuLesen) {
        n.skript.append(imSkript).push_back('\n');
        n.lesetext.append(zuLesen).push_back('\n');
        ++zeile;
    };
    // eine Meldung zur naechsten Zeile, die geschrieben wird, ab Spalte `spalte` (ab 0)
    const auto melde = [&](std::size_t spalte, std::string meldung) {
        n.meldungen.push_back({Schwere::Hinweis, {n.skript.size() + spalte, zeile + 1, static_cast<int>(spalte) + 1},
                               std::move(meldung)});
    };
    std::vector<FremdeZeilen> fremde;

    for (std::size_t i = 0; i < zellen->elemente.size(); ++i)
    {
        const json::Wert& c = zellen->elemente[i];
        const std::size_t nummer = i + 1;
        const std::string art = textVon(feld(&c, "cell_type"));
        const std::string quelle = textVon(feld(&c, "source"));
        const std::vector<std::string_view> zeilen = zeilenVon(quelle);

        Zelle z;
        z.art = art == "code" ? Zelle::Art::Code : art == "markdown" ? Zelle::Art::Markdown : Zelle::Art::Roh;
        if (i > 0) schreibe("", "");
        const std::string kopf = k + " %% [" + std::to_string(nummer) + "]" +
                                 (z.art == Zelle::Art::Markdown ? " markdown" : z.art == Zelle::Art::Roh ? " raw" : "");
        schreibe(kopf, kopf);
        z.kopf = zeile;
        z.zeile = zeile + 1;
        if (z.art != Zelle::Art::Code)
        {
            for (const std::string_view t : zeilen)
            {
                const std::string kommentar = t.empty() ? k : k + " " + std::string(t);
                schreibe(kommentar, kommentar);
            }
            z.bisZeile = zeile;
            n.zellen.push_back(std::move(z));
            continue;
        }

        // die Ausgaben (Text: stream, text/plain; aus Databricks exportiert: text/html mit einem Block `ansiout`),
        // aneinander — darin stehen die Plaene von explain() (P2-3)
        if (const json::Wert* ausgaben = feld(&c, "outputs"); ausgaben != nullptr && ausgaben->art == json::Art::Liste)
        {
            ZellAusgabe a;
            a.zelle = i;
            for (const json::Wert& o : ausgaben->elemente)
            {
                const json::Wert* t = feld(&o, "text");
                if (t == nullptr) t = feld(feld(&o, "data"), "text/plain");
                std::string s = t != nullptr ? textVon(t) : ansiText(textVon(feld(feld(&o, "data"), "text/html")));
                if (s.empty()) continue;
                if (!a.text.empty() && a.text.back() != '\n') a.text.push_back('\n');
                a.text += s;
            }
            if (!a.text.empty()) n.ausgaben.push_back(std::move(a));
        }

        // Zellmagie in der ersten Zeile: `%%sql` wechselt die Sprache, `%%time` nicht, `%%bash` liest niemand
        const sprache::Sprache* sprache = n.sprache;
        bool zellmagie = false;  // die erste Zeile ist Magie in der Sprache des Notebooks
        if (!zeilen.empty() && zeilen.front().substr(einzug(zeilen.front())).starts_with("%%"))
        {
            const std::string_view erste = zeilen.front().substr(einzug(zeilen.front()));
            const std::string magie(wortAb(erste, 2));
            if (std::find(std::begin(kZellmagieOhneWechsel), std::end(kZellmagieOhneWechsel), magie) !=
                std::end(kZellmagieOhneWechsel))
                zellmagie = true;
            else if (const sprache::Sprache* f = spracheNamens(magie); f != nullptr && f != n.sprache)
            {
                sprache = f;
                z.sprache = std::string(f->name());
            }
            else if (f == nullptr)
            {
                sprache = nullptr;
                z.sprache = "%%" + magie;
                z.gelesen = false;
                melde(einzug(zeilen.front()), "cell " + std::to_string(nummer) + " in %%" + magie + " is not read");
            }
            else
                zellmagie = true;  // `%%python` in einem Python-Notebook
        }

        FremdeZeilen fremd{sprache, n.zellen.size(), {}, {}};
        for (std::size_t j = 0; j < zeilen.size(); ++j)
        {
            const std::string_view t = zeilen[j];
            if (sprache != n.sprache)
            {
                if (sprache != nullptr && j > 0)
                {
                    fremd.zeilen.emplace_back(zeile + 1, std::string(t));
                    fremd.abzug.push_back(0);
                }
                schreibe(t, std::string(t.size(), ' '));
                continue;
            }
            const std::string_view vorn = t.substr(einzug(t));
            if ((j == 0 && zellmagie) || vorn.starts_with("%") || vorn.starts_with("!"))
            {
                melde(einzug(t), magieHinweis(vorn));
                schreibe(t, std::string(t.size(), ' '));
                continue;
            }
            schreibe(t, t);
        }
        z.bisZeile = zeile;
        if (sprache == n.sprache && z.zeile <= z.bisZeile)
            n.abschnitte.push_back({zellenName(nummer), z.zeile, z.bisZeile});
        if (sprache != nullptr && sprache != n.sprache && !fremd.zeilen.empty()) fremde.push_back(std::move(fremd));
        n.zellen.push_back(std::move(z));
    }
    n.fremde = alsTexte(fremde, zeile);
    return n;
}

std::optional<Notebook> ausDatabricks(std::string_view text, const sprache::Sprache& sprache)
{
    // erst die erste Zeile: jede Datei des Projekts kommt hier vorbei
    const std::string k = kommentarVon(sprache);
    std::string_view erste = text.substr(0, text.find('\n'));
    if (erste.starts_with("\xEF\xBB\xBF")) erste.remove_prefix(3);
    while (!erste.empty() && (erste.back() == ' ' || erste.back() == '\t' || erste.back() == '\r')) erste.remove_suffix(1);
    if (erste != k + " Databricks notebook source") return std::nullopt;
    const std::vector<std::string_view> zeilen = zeilenVon(text);

    Notebook n;
    n.skript = std::string(text);
    n.lesetext = n.skript;
    n.sprache = &sprache;
    n.databricks = true;
    const Zeilen orte(text);
    const std::string trenner = k + " COMMAND ----------";
    const std::string magie = k + " MAGIC";
    const std::string titel = k + " DBTITLE";
    const auto zeileNr = [&](int z) { return zeilen[static_cast<std::size_t>(z - 1)]; };
    const int anzahl = static_cast<int>(zeilen.size());

    // die Zellen: ab Zeile 2 bis zum ersten Trenner, dann je von Trenner zu Trenner
    struct Bereich
    {
        int kopf, von, bis;
    };
    std::vector<Bereich> bereiche;
    int kopf = 0;
    int von = 2;
    for (int z = 2; z <= anzahl; ++z)
    {
        std::string_view t = zeileNr(z);
        while (!t.empty() && (t.back() == ' ' || t.back() == '\t')) t.remove_suffix(1);
        if (t != trenner) continue;
        bereiche.push_back({kopf, von, z - 1});
        kopf = z;
        von = z + 1;
    }
    bereiche.push_back({kopf, von, anzahl});

    std::vector<FremdeZeilen> fremde;
    for (std::size_t i = 0; i < bereiche.size(); ++i)
    {
        const std::size_t nummer = i + 1;
        Zelle z;
        z.kopf = bereiche[i].kopf;
        z.zeile = bereiche[i].von;
        z.bisZeile = bereiche[i].bis;
        const auto stutze = [&] {
            while (z.zeile <= z.bisZeile && leer(zeileNr(z.zeile))) ++z.zeile;
            while (z.bisZeile >= z.zeile && leer(zeileNr(z.bisZeile))) --z.bisZeile;
        };
        stutze();
        if (z.zeile <= z.bisZeile && zeileNr(z.zeile).starts_with(titel))
        {
            const std::string_view t = zeileNr(z.zeile);
            const std::size_t komma = t.find(',');
            if (komma != std::string_view::npos)
            {
                std::string_view titelText = t.substr(komma + 1);
                while (!titelText.empty() && (titelText.back() == ' ' || titelText.back() == '\r')) titelText.remove_suffix(1);
                z.titel = std::string(titelText);
            }
            ++z.zeile;
            stutze();
        }
        // eine Zelle mit Magie: jede Zeile mit Inhalt beginnt mit `MAGIC`
        bool mitMagie = z.zeile <= z.bisZeile;
        for (int r = z.zeile; r <= z.bisZeile && mitMagie; ++r)
            if (!leer(zeileNr(r)) && !zeileNr(r).starts_with(magie)) mitMagie = false;
        if (!mitMagie)
        {
            if (z.zeile <= z.bisZeile) n.abschnitte.push_back({zellenName(nummer), z.zeile, z.bisZeile});
            n.zellen.push_back(std::move(z));
            continue;
        }
        // das Wort der Magie: in der ersten Zeile, die hinter `MAGIC` etwas traegt (davor darf eine leer sein)
        int wortZeile = z.zeile;
        std::size_t wortAnfang = zeileNr(z.zeile).size();
        for (int r = z.zeile; r <= z.bisZeile; ++r)
        {
            const std::string_view t = zeileNr(r);
            const std::size_t p = t.starts_with(magie) ? t.find_first_not_of(" \t", magie.size()) : std::string_view::npos;
            if (p == std::string_view::npos) continue;
            wortZeile = r;
            wortAnfang = p;
            break;
        }
        const std::string_view wort = wortAb(zeileNr(wortZeile), wortAnfang);
        const quelle::Position amWort = orte.ort(orte.anfang(wortZeile) + wortAnfang);
        if (wort.starts_with("%md"))
        {
            z.art = Zelle::Art::Markdown;
            n.zellen.push_back(std::move(z));
            continue;
        }
        const sprache::Sprache* f = wort.starts_with("%") ? spracheNamens(wort.substr(1)) : nullptr;
        if (f == nullptr)
        {
            z.gelesen = false;
            z.sprache = std::string(wort);
            n.meldungen.push_back({Schwere::Hinweis, amWort,
                                   wort == "%run" ? std::string("%run is not followed")
                                                  : "cell " + std::to_string(nummer) + " in " +
                                                        std::string(wort.empty() ? std::string_view("MAGIC") : wort) +
                                                        " is not read"});
            n.zellen.push_back(std::move(z));
            continue;
        }
        // in einer Sprache, die das Projekt kennt: ohne `MAGIC ` davor; in der Zeile der Magie, was hinter dem
        // Wort steht (`%python time.sleep(1)`), ab der ersten Spalte — sonst stuende es eingerueckt
        z.sprache = std::string(f->name());
        FremdeZeilen fremd{f, n.zellen.size(), {}, {}};
        for (int r = wortZeile; r <= z.bisZeile; ++r)
        {
            const std::string_view t = zeileNr(r);
            if (!t.starts_with(magie)) continue;  // leer
            std::size_t ab = magie.size();
            if (ab < t.size() && t[ab] == ' ') ++ab;
            if (r == wortZeile) ab = std::min(t.find_first_not_of(" \t", wortAnfang + wort.size()), t.size());
            fremd.zeilen.emplace_back(r, std::string(t.substr(ab)));
            fremd.abzug.push_back(ab);
        }
        fremde.push_back(std::move(fremd));
        n.zellen.push_back(std::move(z));
    }
    n.fremde = alsTexte(fremde, anzahl);
    return n;
}

}  // namespace c2d::projekt
