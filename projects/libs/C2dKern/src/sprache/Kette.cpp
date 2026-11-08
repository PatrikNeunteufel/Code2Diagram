/**
 * @file Kette.cpp
 * @brief Aufrufketten aus den Token eines Texts (Entscheid F1)
 */
#include "Kette.hpp"

#include <algorithm>

namespace c2d::sprache
{

namespace
{

using lexer::Token;
using lexer::TokenArt;
using modell::Aufrufkette;
using modell::Kettenglied;

class Zerleger
{
public:
    Zerleger(std::string_view text, std::vector<Token> token, int zeile)
        : m_text(text), m_t(std::move(token)), m_zeile(zeile)
    {
        // Zeilenende, Einzug, Auszug und Kommentare tragen nichts zu einer Kette bei
        std::erase_if(m_t, [](const Token& t) {
            return t.art == TokenArt::Zeilenende || t.art == TokenArt::Einzug || t.art == TokenArt::Auszug ||
                   t.art == TokenArt::Kommentar;
        });
        m_zu.assign(m_t.size(), 0);
        std::vector<std::size_t> offen;
        for (std::size_t i = 0; i < m_t.size(); ++i)
        {
            if (auf(i)) offen.push_back(i);
            else if (zu(i) && !offen.empty())
            {
                m_zu[offen.back()] = i;
                offen.pop_back();
            }
        }
        for (const std::size_t o : offen) m_zu[o] = ende();  // nicht geschlossen: bis zum Ende
    }

    [[nodiscard]] const std::vector<Token>& token() const { return m_t; }

    [[nodiscard]] std::size_t ende() const
    {
        return m_t.empty() || m_t.back().art != TokenArt::Ende ? m_t.size() : m_t.size() - 1;
    }

    /// Alles in [a, b): die Ketten, die dort beginnen. `ziel`/`ganz` fuer die Kette, die [a, b) ganz fuellt.
    void suche(std::size_t a, std::size_t b, std::size_t eltern, std::size_t glied, std::size_t argument,
               const std::string& ziel)
    {
        for (std::size_t i = a; i < b;)
        {
            const bool anfang = i == a || !(m_t[i - 1].istOp(".") || m_t[i - 1].istOp("?."));
            if ((name(i) && anfang) || (auf(i) && klammerAuf(i, a)))
            {
                const std::size_t e = kette(i, b, eltern, glied, argument, i == a, ziel, a, b);
                if (e > i)
                {
                    i = e;
                    continue;
                }
            }
            ++i;
        }
    }

    std::vector<Aufrufkette> ketten;

private:
    [[nodiscard]] bool name(std::size_t i) const { return i < m_t.size() && m_t[i].art == TokenArt::Bezeichner; }
    [[nodiscard]] bool wort(std::size_t i) const
    {
        return i < m_t.size() && (m_t[i].art == TokenArt::Bezeichner || m_t[i].art == TokenArt::Schluesselwort);
    }
    [[nodiscard]] bool auf(std::size_t i) const
    {
        return i < m_t.size() && (m_t[i].istOp("(") || m_t[i].istOp("[") || m_t[i].istOp("{"));
    }
    [[nodiscard]] bool zu(std::size_t i) const
    {
        return i < m_t.size() && (m_t[i].istOp(")") || m_t[i].istOp("]") || m_t[i].istOp("}"));
    }

    /// Eine Klammer, die gruppiert (kein Aufruf, keine Indizierung): davor kein Name und keine Klammer zu.
    [[nodiscard]] bool klammerAuf(std::size_t i, std::size_t a) const
    {
        if (!m_t[i].istOp("(")) return false;
        return i == a || !(name(i - 1) || zu(i - 1) || m_t[i - 1].art == TokenArt::Zeichenkette);
    }

    /// Der Text der Quelle von Token a bis vor Token b.
    [[nodiscard]] std::string quelltext(std::size_t a, std::size_t b) const
    {
        if (a >= b || b > m_t.size()) return {};
        const std::size_t von = m_t[a].ort.versatz;
        const std::size_t bis = m_t[b - 1].ort.versatz + m_t[b - 1].text.size();
        return std::string(m_text.substr(von, bis - von));
    }

    /// Die Argumente in der Klammer bei `i`: je [von, bis) auf oberster Ebene, an `,` getrennt.
    [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> argumente(std::size_t i) const
    {
        std::vector<std::pair<std::size_t, std::size_t>> aus;
        const std::size_t z = m_zu[i];
        std::size_t von = i + 1;
        for (std::size_t k = i + 1; k < z; ++k)
        {
            if (auf(k))
            {
                k = m_zu[k];
                continue;
            }
            if (m_t[k].istOp(","))
            {
                aus.emplace_back(von, k);
                von = k + 1;
            }
        }
        if (von < z) aus.emplace_back(von, z);
        return aus;
    }

    /// Eine Kette ab i (ein Name, oder eine Klammer um genau eine Kette). Gibt die Stelle dahinter, i = keine.
    std::size_t kette(std::size_t i, std::size_t b, std::size_t eltern, std::size_t gliedVon, std::size_t argument,
                      bool vorn, const std::string& ziel, std::size_t a, std::size_t bisAussen)
    {
        const std::size_t stelle = ketten.size();
        ketten.push_back({});
        ketten[stelle].eltern = eltern;
        ketten[stelle].glied = gliedVon;
        ketten[stelle].argument = argument;
        ketten[stelle].zeile = m_zeile + m_t[i].ort.zeile - 1;
        std::size_t j = i;
        if (auf(j))
        {
            // eine Klammer um die ganze Kette: `(spark.read … .load(p))`
            const std::size_t z = m_zu[j];
            const std::size_t innen = j + 1;
            if (!name(innen))
            {
                ketten.pop_back();
                return i;
            }
            std::vector<Aufrufkette> vorher = std::move(ketten);
            ketten = {};
            const std::size_t davor = vorher.size();
            ketten.resize(davor);  // Platzhalter, damit Stellen gleich bleiben
            std::size_t e = kette(innen, z, eltern, gliedVon, argument, true, {}, innen, z);
            if (e != z)
            {
                // nicht genau eine Kette: aufgeben, die innere Suche uebernimmt
                ketten = std::move(vorher);
                ketten.pop_back();
                return i;
            }
            if (ketten.size() <= davor)
            {
                // innen nur ein Name (`f((a))`, `(a).x()`): er hat sich als Name allein selbst entfernt — er ist die
                // Wurzel, wie ohne Klammer (vorher las der Zugriff auf ketten[davor] hinter das Ende)
                ketten = std::move(vorher);
                ketten[stelle].glieder.push_back({std::string(m_t[innen].text), false, {}, {}});
                j = z + 1;
            }
            else
            {
                // die innere Kette an die Stelle der aeusseren ziehen
                Aufrufkette innere = std::move(ketten[davor]);
                std::vector<Aufrufkette> kinder(std::make_move_iterator(ketten.begin() + static_cast<std::ptrdiff_t>(davor) + 1),
                                                std::make_move_iterator(ketten.end()));
                ketten = std::move(vorher);
                innere.eltern = eltern;
                innere.glied = gliedVon;
                innere.argument = argument;
                ketten[stelle] = std::move(innere);
                for (Aufrufkette& k : kinder)
                {
                    if (k.eltern != modell::kKeineKette && k.eltern >= davor) k.eltern = k.eltern - davor + stelle;
                    ketten.push_back(std::move(k));
                }
                j = z + 1;
            }
        }
        else
        {
            ketten[stelle].glieder.push_back({std::string(m_t[j].text), false, {}, {}});
            ++j;
        }
        // Glieder: `.name`, `(…)`, `[…]`
        while (j < b)
        {
            if ((m_t[j].istOp(".") || m_t[j].istOp("?.")) && wort(j + 1))
            {
                ketten[stelle].glieder.push_back({std::string(m_t[j + 1].text), false, {}, {}});
                j += 2;
                continue;
            }
            if (m_t[j].istOp("(") && !ketten[stelle].glieder.empty() && !ketten[stelle].glieder.back().aufruf)
            {
                const std::size_t g = ketten[stelle].glieder.size() - 1;
                ketten[stelle].glieder[g].aufruf = true;
                arg(stelle, g, j);
                j = m_zu[j] + 1;
                continue;
            }
            if (m_t[j].istOp("["))
            {
                ketten[stelle].glieder.push_back({"[]", false, {}, {}});
                arg(stelle, ketten[stelle].glieder.size() - 1, j);
                j = m_zu[j] + 1;
                continue;
            }
            break;
        }
        Aufrufkette& k = ketten[stelle];
        k.ganz = vorn && i == a && j == bisAussen;
        if (k.ganz) k.ziel = ziel;
        // ein Name allein zaehlt nur als ganzer Wert einer Anweisung (`return spark`, `df2 = df`); in einem
        // Argument steht er in Kettenglied::argumente und ::namen
        if (k.glieder.size() == 1 && !k.glieder[0].aufruf && (!k.ganz || eltern != modell::kKeineKette))
        {
            ketten.erase(ketten.begin() + static_cast<std::ptrdiff_t>(stelle));
            return j;
        }
        return j;
    }

    /// Die Argumente der Klammer bei `i` an das Glied g der Kette `stelle`, und die Ketten darin.
    void arg(std::size_t stelle, std::size_t g, std::size_t i)
    {
        const std::vector<std::pair<std::size_t, std::size_t>> teile = argumente(i);
        for (std::size_t n = 0; n < teile.size(); ++n)
        {
            const auto [von, bis] = teile[n];
            ketten[stelle].glieder[g].argumente.push_back(quelltext(von, bis));
            for (std::size_t k = von; k < bis; ++k)
                if (name(k) && (k == von || !m_t[k - 1].istOp(".")))
                {
                    std::vector<std::string>& namen = ketten[stelle].glieder[g].namen;
                    const std::string n2(m_t[k].text);
                    if (std::find(namen.begin(), namen.end(), n2) == namen.end()) namen.push_back(n2);
                }
            // ein benanntes Argument `name=wert`: die Kette ist der Wert
            std::size_t start = von;
            if (name(von) && von + 1 < bis && m_t[von + 1].istOp("=")) start = von + 2;
            suche(start, bis, stelle, g, n, {});
        }
    }

    std::string_view m_text;
    std::vector<Token> m_t;
    std::vector<std::size_t> m_zu;
    int m_zeile = 0;
};

void ergaenze(modell::Anweisung& a, const lexer::Sprachtabelle& tabelle, const Ausdrucksregeln& regeln)
{
    using modell::AnweisungArt;
    switch (a.art)
    {
        case AnweisungArt::Einfach:
        case AnweisungArt::Ruecksprung:
        case AnweisungArt::Wurf:
        case AnweisungArt::Mit:
        case AnweisungArt::Wenn:
        case AnweisungArt::Solange:
        case AnweisungArt::TueSolange:
        case AnweisungArt::Fuer:
        case AnweisungArt::FuerJedes:
        case AnweisungArt::Auswahl:
            if (!a.text.empty()) a.ketten = kettenIn(a.text, a.zeile, tabelle, regeln);
            break;
        default: break;
    }
    for (modell::AnweisungPtr& k : a.kinder) ergaenze(*k, tabelle, regeln);
    if (a.sonst) ergaenze(*a.sonst, tabelle, regeln);
}

}  // namespace

std::vector<modell::Aufrufkette> kettenIn(std::string_view text, int zeile, const lexer::Sprachtabelle& tabelle,
                                          const Ausdrucksregeln& regeln)
{
    std::vector<quelle::Meldung> stumm;
    Zerleger z(text, lexer::zerlege(text, tabelle, stumm), zeile);
    // eine Zuweisung auf oberster Ebene: nur `=` (das erste der Regeln), nicht `+=`, nicht in Klammern
    const std::string_view gleich = regeln.zuweisungen.empty() ? std::string_view("=") : regeln.zuweisungen.front();
    const std::vector<Token>& t = z.token();
    const std::size_t bis = z.ende();
    std::size_t wert = 0;
    std::string ziel;
    int tiefe = 0;
    for (std::size_t i = 0; i < bis; ++i)
    {
        if (t[i].istOp("(") || t[i].istOp("[") || t[i].istOp("{")) ++tiefe;
        if (t[i].istOp(")") || t[i].istOp("]") || t[i].istOp("}")) --tiefe;
        if (tiefe == 0 && t[i].art == TokenArt::Operator && t[i].text == gleich && i > 0)
        {
            const std::size_t von = 0;
            std::string links(text.substr(t[von].ort.versatz, t[i - 1].ort.versatz + t[i - 1].text.size() - t[von].ort.versatz));
            // `x: T = …` (eine Annotation): der Name vorn
            if (i >= 3 && t[1].istOp(":") && t[0].art == TokenArt::Bezeichner) links = std::string(t[0].text);
            ziel = std::move(links);
            wert = i + 1;
            break;
        }
    }
    // nur rechts: was links steht, ist das Ziel (Aufrufkette::ziel)
    z.suche(wert, bis, modell::kKeineKette, 0, 0, ziel);
    return std::move(z.ketten);
}

void ergaenzeKetten(modell::Einheit& einheit, const lexer::Sprachtabelle& tabelle, const Ausdrucksregeln& regeln)
{
    for (modell::Funktion& f : einheit.funktionen)
        if (f.rumpf) ergaenze(*f.rumpf, tabelle, regeln);
}

}  // namespace c2d::sprache
