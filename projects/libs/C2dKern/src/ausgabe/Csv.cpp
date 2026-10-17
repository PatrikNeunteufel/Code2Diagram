/**
 * @file Csv.cpp
 * @brief CSV: Wahrheits- und Entscheidungstabellen fuer Testplaene — andere Diagramme kann es nicht
 *
 * Trenner `;` (Excel unter deutscher Einstellung), Zeilen mit `\n`. Ein Feld mit
 * `;`, `"` oder Zeilenwechsel steht in `"…"`, ein `"` darin doppelt. Ein nicht
 * ausgewertetes Atom heisst `-` (reines ASCII: Excel liest CSV ohne BOM nicht als UTF-8).
 */
#include "c2d/ausgabe/Ausgabe.hpp"

#include "Text.hpp"

namespace c2d::ausgabe
{

namespace
{

using namespace c2d::diagramm;

std::string feld(std::string_view text)
{
    if (text.find_first_of(";\"\n\r") == std::string_view::npos) return std::string(text);
    std::string aus = "\"";
    for (const char c : text)
    {
        if (c == '"') aus += '"';
        aus += c;
    }
    return aus + '"';
}

std::string zeile(const std::vector<std::string>& felder)
{
    std::string aus;
    for (std::size_t i = 0; i < felder.size(); ++i) aus += (i ? ";" : "") + feld(felder[i]);
    return aus + '\n';
}

class Csv final : public Format
{
public:
    [[nodiscard]] std::string_view name() const override { return "csv"; }
    [[nodiscard]] std::string_view endung() const override { return "csv"; }

    // eine Tabelle ja, ein Graph oder Bloecke nicht
    [[nodiscard]] bool kann(Diagrammart art) const override
    {
        return art == Diagrammart::Wahrheitstabelle || art == Diagrammart::Entscheidungstabelle || art == Diagrammart::Zuordnung ||
               art == Diagrammart::Konfiguration || art == Diagrammart::Schaltstellen || art == Diagrammart::Schalteruebersicht;
    }

    [[nodiscard]] std::string schreibe(const Flussdiagramm&, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(const Strukturdiagramm&, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(const Zustandsdiagramm&, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(const Aufrufgraph&, const Optionen&) const override { return {}; }
    [[nodiscard]] std::string schreibe(const Struktogramm&, const Optionen&) const override { return {}; }

    [[nodiscard]] std::string schreibe(std::span<const Wahrheitstabelle> tabellen, const Optionen&) const override
    {
        std::string aus;
        for (std::size_t ti = 0; ti < tabellen.size(); ++ti)
        {
            const Wahrheitstabelle& t = tabellen[ti];
            if (ti > 0) aus += '\n';
            aus += zeile({t.funktion + "(): line " + std::to_string(t.quellZeile), t.bedingung});
            std::vector<std::string> kopf{"#"};
            for (std::size_t i = 0; i < t.atome.size(); ++i) kopf.push_back(atomKennung(i) + ": " + t.atome[i].text);
            kopf.emplace_back("Result");
            kopf.emplace_back("MC/DC");
            aus += zeile(kopf);
            for (const Wahrheitstabelle::Zeile& z : t.zeilen)
            {
                std::vector<std::string> r{std::to_string(z.nummer)};
                for (const Wert w : z.werte) r.emplace_back(w == Wert::Wahr ? "T" : w == Wert::Falsch ? "F" : "-");
                r.emplace_back(z.ergebnis ? "T" : "F");
                std::string paare;
                for (std::size_t i = 0; i < t.paare.size(); ++i)
                    if (t.paare[i].first == z.nummer || t.paare[i].second == z.nummer)
                        paare += (paare.empty() ? "" : ", ") + atomKennung(i);
                r.push_back(paare);
                aus += zeile(r);
            }
            for (const std::string& h : t.hinweise) aus += zeile({h});
        }
        return aus;
    }

    [[nodiscard]] std::string schreibe(const Entscheidungstabelle& t, const Optionen&) const override
    {
        std::string aus = zeile({t.funktion + "(): decision table"});
        if (!t.regeln.empty())
        {
            std::vector<std::string> kopf{"Conditions"};
            for (std::size_t r = 0; r < t.regeln.size(); ++r) kopf.push_back("R" + std::to_string(r + 1));
            aus += zeile(kopf);
            for (std::size_t b = 0; b < t.bedingungen.size(); ++b)
            {
                std::vector<std::string> z{zeilenText(t.bedingungen[b])};
                for (const Entscheidungstabelle::Regel& r : t.regeln) z.push_back(r.eintraege[b].empty() ? "-" : r.eintraege[b]);
                aus += zeile(z);
            }
            aus += zeile({"Actions"});
            for (std::size_t k = 0; k < t.aktionen.size(); ++k)
            {
                std::vector<std::string> z{t.aktionen[k].text};
                for (const Entscheidungstabelle::Regel& r : t.regeln) z.emplace_back(r.aktionen[k] ? "X" : "");
                aus += zeile(z);
            }
        }
        for (const std::string& h : t.hinweise) aus += zeile({h});
        return aus;
    }

    [[nodiscard]] std::string schreibe(std::span<const Zuordnung> tabellen, const Optionen&) const override
    {
        std::string aus;
        for (std::size_t ti = 0; ti < tabellen.size(); ++ti)
        {
            const Zuordnung& t = tabellen[ti];
            if (ti > 0) aus += '\n';
            aus += zeile({t.titel});
            std::vector<std::string> kopf{t.schluessel};
            kopf.insert(kopf.end(), t.spalten.begin(), t.spalten.end());
            aus += zeile(kopf);
            for (const Zuordnung::Zeile& z : t.zeilen)
            {
                std::vector<std::string> r{z.schluessel};
                for (const std::string& w : z.werte) r.push_back(w == "\xE2\x80\x94" ? std::string("-") : w);
                aus += zeile(r);
            }
        }
        return aus;
    }

    // je Kette: Titel, Kopf (Zweig, die Namen, active), die Zeilen, die Hinweise
    [[nodiscard]] std::string schreibe(std::span<const Konfigurationstabelle> tabellen, const Optionen&) const override
    {
        std::string aus;
        for (std::size_t ti = 0; ti < tabellen.size(); ++ti)
        {
            const Konfigurationstabelle& t = tabellen[ti];
            if (ti > 0) aus += '\n';
            aus += zeile({t.titel});
            std::vector<std::string> kopf{"branch"};
            for (const Konfigurationstabelle::Spalte& s : t.spalten) kopf.push_back(s.name);
            kopf.emplace_back("active");
            aus += zeile(kopf);
            for (const Konfigurationstabelle::Zeile& z : t.zeilen)
            {
                std::vector<std::string> r{z.zweig};
                for (const std::string& w : z.werte) r.push_back(w == "\xE2\x80\x93" ? std::string("-") : w);
                r.emplace_back(z.geltend ? "yes" : z.nie ? "never" : "");
                aus += zeile(r);
            }
            for (const std::string& h : t.hinweise) aus += zeile({h});
        }
        return aus;
    }

    // Titel, die Setzungen, je Zweig eine Zeile (Datei und Zeile in jeder), die Hinweise
    [[nodiscard]] std::string schreibe(const Schaltstellen& t, const Optionen&) const override
    {
        std::string aus = zeile({t.titel});
        aus += zeile({"set by", "file", "line", "value"});
        for (const Schaltstellen::Setzung& s : t.gesetzt)
            aus += zeile({s.herkunft, s.pfad, s.zeile > 0 ? std::to_string(s.zeile) : std::string(), s.wert});
        aus += '\n';
        aus += zeile({"file", "line", "branch", t.name, "state in the file", "from", "to", "lines", "active"});
        for (const Schaltstellen::Zeile& z : t.zeilen)
        {
            const bool bekannt = z.bis != 0;
            aus += zeile({z.pfad, std::to_string(z.quellZeile), z.text ? z.bedingung : z.zweig,
                          z.forderung == "\xE2\x80\x93" ? std::string("-") : z.forderung, z.stand,
                          bekannt ? std::to_string(z.von) : std::string(), bekannt ? std::to_string(z.bis) : std::string(),
                          bekannt ? std::to_string(zeilenDazwischen(z)) : std::string(),
                          z.geltend ? "yes" : z.nie ? "never" : ""});
        }
        for (const std::string& h : t.hinweise) aus += zeile({h});
        return aus;
    }

    [[nodiscard]] std::string schreibe(const Schalteruebersicht& t, const Optionen&) const override
    {
        std::string aus = zeile({t.titel});
        aus += zeile({"name", "files", "chains", "state", "set by", "language"});
        for (const Schalteruebersicht::Zeile& z : t.zeilen)
            aus += zeile({z.name, std::to_string(z.dateien), std::to_string(z.ketten), z.stand, z.herkunft, z.vorgegeben ? "yes" : ""});
        for (const std::string& h : t.hinweise) aus += zeile({h});
        return aus;
    }
};

}  // namespace

const Format& csv()
{
    static const Csv format;
    return format;
}

}  // namespace c2d::ausgabe
