/**
 * @file Projektdatei.cpp
 * @brief Die Projektdatei lesen und schreiben; Ordnermuster; der Vorschlag aus `.gitignore`
 */
#include "c2d/projekt/Projektdatei.hpp"

#include "Json.hpp"

#include <algorithm>

namespace c2d::projekt
{

namespace
{

using quelle::Meldung;
using quelle::Schwere;

/// Ordnernamen, die nie Quelltext des Projekts tragen: Bau-Ausgaben und Abhaengigkeiten.
constexpr std::string_view kImmer[] = {"out", "build", "node_modules"};

std::vector<std::string_view> teile(std::string_view weg)
{
    std::vector<std::string_view> aus;
    std::size_t anfang = 0;
    while (anfang <= weg.size())
    {
        const std::size_t ende = std::min(weg.find('/', anfang), weg.size());
        if (ende > anfang) aus.push_back(weg.substr(anfang, ende - anfang));
        anfang = ende + 1;
    }
    return aus;
}

/// `*` und `?` in einem Namen.
bool passtName(std::string_view muster, std::string_view name)
{
    std::size_t m = 0, n = 0, sternM = std::string_view::npos, sternN = 0;
    while (n < name.size())
    {
        if (m < muster.size() && (muster[m] == '?' || muster[m] == name[n]))
        {
            ++m;
            ++n;
        }
        else if (m < muster.size() && muster[m] == '*')
        {
            sternM = m++;
            sternN = n;
        }
        else if (sternM != std::string_view::npos)
        {
            m = sternM + 1;
            n = ++sternN;
        }
        else
            return false;
    }
    while (m < muster.size() && muster[m] == '*') ++m;
    return m == muster.size();
}

/// Muster und Weg Ordner fuer Ordner; `**` steht fuer beliebig viele.
bool passtTeile(const std::vector<std::string_view>& muster, std::size_t m, const std::vector<std::string_view>& weg,
                std::size_t w)
{
    if (m == muster.size()) return w == weg.size();
    if (muster[m] == "**")
    {
        for (std::size_t k = w; k <= weg.size(); ++k)
            if (passtTeile(muster, m + 1, weg, k)) return true;
        return false;
    }
    return w < weg.size() && passtName(muster[m], weg[w]) && passtTeile(muster, m + 1, weg, w + 1);
}

std::string_view normiert(std::string_view muster)
{
    while (muster.starts_with("./")) muster.remove_prefix(2);
    while (muster.starts_with('/')) muster.remove_prefix(1);
    while (muster.ends_with('/')) muster.remove_suffix(1);
    return muster;
}

/// Liest die Projektdatei aus dem JSON-Wert; meldet, was nicht passt.
class Leser
{
public:
    explicit Leser(std::vector<Meldung>& meldungen) : m_meldungen(meldungen) {}

    Projektdatei lies(const json::Wert& wurzel)
    {
        Projektdatei aus;
        if (!erwarte(wurzel, json::Art::Objekt, "the project file")) return aus;
        for (std::size_t i = 0; i < wurzel.namen.size(); ++i)
        {
            const std::string& name = wurzel.namen[i];
            const json::Wert& wert = wurzel.elemente[i];
            if (name == "definitionen") aus.definitionen = definitionen(wert, name);
            else if (name == "includePfade") aus.includePfade = texte(wert, name);
            else if (name == "uebergehen") aus.uebergehen = texte(wert, name);
            else if (name == "abschnitte") aus.abschnitte = abschnitte(wert);
            else unbekannt(wert, name, "definitionen, includePfade, uebergehen, abschnitte");
        }
        return aus;
    }

private:
    void melde(Schwere schwere, const json::Wert& wo, std::string text)
    {
        m_meldungen.push_back({schwere, wo.ort, std::move(text)});
    }

    bool erwarte(const json::Wert& wert, json::Art art, std::string_view was)
    {
        if (wert.art == art) return true;
        melde(Schwere::Fehler, wert,
              std::string(was) + " must be " + std::string(json::name(art)) + ", not " + std::string(json::name(wert.art)) +
                  " - ignored");
        return false;
    }

    void unbekannt(const json::Wert& wert, const std::string& name, std::string_view bekannt)
    {
        melde(Schwere::Warnung, wert, "unknown key \"" + name + "\" - ignored (known: " + std::string(bekannt) + ")");
    }

    std::vector<std::string> texte(const json::Wert& wert, const std::string& name)
    {
        std::vector<std::string> aus;
        if (!erwarte(wert, json::Art::Liste, "\"" + name + "\"")) return aus;
        for (const json::Wert& e : wert.elemente)
        {
            if (!erwarte(e, json::Art::Text, "an entry of \"" + name + "\"")) continue;
            if (e.text.empty())
            {
                melde(Schwere::Warnung, e, "an empty entry in \"" + name + "\" - ignored");
                continue;
            }
            aus.push_back(e.text);
        }
        return aus;
    }

    std::vector<sprache::Definition> definitionen(const json::Wert& wert, const std::string& name)
    {
        std::vector<sprache::Definition> aus;
        if (!erwarte(wert, json::Art::Liste, "\"" + name + "\"")) return aus;
        for (const json::Wert& e : wert.elemente)
        {
            if (!erwarte(e, json::Art::Text, "an entry of \"" + name + "\"")) continue;
            sprache::Definition d = sprache::definitionAus(e.text);
            if (d.name.empty())
            {
                melde(Schwere::Warnung, e, "\"" + e.text + "\" is not a definition (NAME or NAME=value) - ignored");
                continue;
            }
            aus.push_back(std::move(d));
        }
        return aus;
    }

    std::vector<Abschnitt> abschnitte(const json::Wert& wert)
    {
        std::vector<Abschnitt> aus;
        if (!erwarte(wert, json::Art::Liste, "\"abschnitte\"")) return aus;
        for (const json::Wert& e : wert.elemente)
        {
            if (!erwarte(e, json::Art::Objekt, "an entry of \"abschnitte\"")) continue;
            Abschnitt a;
            for (std::size_t i = 0; i < e.namen.size(); ++i)
            {
                const std::string& name = e.namen[i];
                const json::Wert& w = e.elemente[i];
                if (name == "ordner")
                {
                    if (erwarte(w, json::Art::Text, "\"ordner\"")) a.ordner = w.text;
                }
                else if (name == "definitionen") a.definitionen = definitionen(w, name);
                else if (name == "includePfade") a.includePfade = texte(w, name);
                else unbekannt(w, name, "ordner, definitionen, includePfade");
            }
            if (normiert(a.ordner).empty())
            {
                melde(Schwere::Warnung, e, "a section without \"ordner\" - ignored");
                continue;
            }
            aus.push_back(std::move(a));
        }
        return aus;
    }

    std::vector<Meldung>& m_meldungen;
};

std::string liste(const std::vector<std::string>& eintraege, std::string_view einzug)
{
    if (eintraege.empty()) return "[]";
    std::string aus = "[";
    for (std::size_t i = 0; i < eintraege.size(); ++i)
        aus += (i == 0 ? "\n" : ",\n") + std::string(einzug) + "  " + json::zitat(eintraege[i]);
    return aus + "\n" + std::string(einzug) + "]";
}

std::vector<std::string> alsTexte(const std::vector<sprache::Definition>& definitionen)
{
    std::vector<std::string> aus;
    for (const sprache::Definition& d : definitionen) aus.push_back(alsText(d));
    return aus;
}

}  // namespace

Projektdatei liesProjektdatei(std::string_view text, std::vector<quelle::Meldung>& meldungen)
{
    json::Wert wurzel;
    quelle::Meldung fehler;
    if (!json::lies(text, wurzel, fehler))
    {
        meldungen.push_back(std::move(fehler));
        return {};
    }
    return Leser(meldungen).lies(wurzel);
}

std::string schreibe(const Projektdatei& datei)
{
    std::string aus = "{\n";
    aus += "  \"definitionen\": " + liste(alsTexte(datei.definitionen), "  ") + ",\n";
    aus += "  \"includePfade\": " + liste(datei.includePfade, "  ") + ",\n";
    aus += "  \"uebergehen\": " + liste(datei.uebergehen, "  ") + ",\n";
    aus += "  \"abschnitte\": [";
    for (std::size_t i = 0; i < datei.abschnitte.size(); ++i)
    {
        const Abschnitt& a = datei.abschnitte[i];
        aus += i == 0 ? "\n" : ",\n";
        aus += "    {\n";
        aus += "      \"ordner\": " + json::zitat(a.ordner) + ",\n";
        aus += "      \"definitionen\": " + liste(alsTexte(a.definitionen), "      ") + ",\n";
        aus += "      \"includePfade\": " + liste(a.includePfade, "      ") + "\n";
        aus += "    }";
    }
    aus += datei.abschnitte.empty() ? "]\n" : "\n  ]\n";
    return aus + "}\n";
}

std::string alsText(const sprache::Definition& definition)
{
    return definition.wert == "1" ? definition.name : definition.name + "=" + definition.wert;
}

bool passt(std::string_view muster, std::string_view ordner)
{
    muster = normiert(muster);
    if (muster.empty()) return false;
    const std::vector<std::string_view> weg = teile(ordner);
    if (muster.find('/') == std::string_view::npos)
        return !weg.empty() && passtName(muster, weg.back());  // ein Name: in jeder Tiefe
    return passtTeile(teile(muster), 0, weg, 0);
}

bool liegtIn(std::string_view muster, std::string_view ordner)
{
    std::size_t ende = ordner.size();
    while (true)
    {
        const std::string_view stueck = ordner.substr(0, ende);
        if (!stueck.empty() && passt(muster, stueck)) return true;
        const std::size_t strich = stueck.rfind('/');
        if (strich == std::string_view::npos) return false;
        ende = strich;
    }
}

std::span<const std::string_view> immerUebergangen()
{
    return kImmer;
}

bool wirdImmerUebergangen(std::string_view name)
{
    return name.starts_with('.') || std::find(std::begin(kImmer), std::end(kImmer), name) != std::end(kImmer);
}

Vorschlag ausGitignore(std::string_view text)
{
    Vorschlag aus;
    std::size_t anfang = 0;
    while (anfang < text.size())
    {
        std::size_t ende = text.find('\n', anfang);
        if (ende == std::string_view::npos) ende = text.size();
        std::string_view zeile = text.substr(anfang, ende - anfang);
        anfang = ende + 1;
        while (!zeile.empty() && (zeile.back() == '\r' || zeile.back() == ' ' || zeile.back() == '\t'))
            zeile.remove_suffix(1);
        while (!zeile.empty() && (zeile.front() == ' ' || zeile.front() == '\t')) zeile.remove_prefix(1);
        if (zeile.empty() || zeile.front() == '#') continue;

        const auto nicht = [&] { aus.nichtUebernommen.emplace_back(zeile); };
        if (zeile.front() == '!')  // eine Verneinung laesst sich nicht als Ordner uebergehen
        {
            nicht();
            continue;
        }
        const bool ordner = zeile.back() == '/';
        std::string_view muster = normiert(zeile);
        // ohne `/` am Ende ein Ordner nur, wenn der letzte Name keinen Punkt traegt (`out`, nicht `*.o`)
        const std::size_t strich = muster.rfind('/');
        const std::string_view letzter = strich == std::string_view::npos ? muster : muster.substr(strich + 1);
        if (muster.empty() || (!ordner && letzter.find('.') != std::string_view::npos && !letzter.starts_with('.')))
        {
            nicht();
            continue;
        }
        if (muster.find('/') == std::string_view::npos && wirdImmerUebergangen(muster)) continue;  // ohnehin
        if (std::find(aus.uebergehen.begin(), aus.uebergehen.end(), muster) == aus.uebergehen.end())
            aus.uebergehen.emplace_back(muster);
    }
    return aus;
}

}  // namespace c2d::projekt
