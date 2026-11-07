/**
 * @file Plan.cpp
 * @brief Plaene von Spark: der Text von `explain()` als Baum (Spark Schicht 2, Entscheide P2-1 bis P2-4)
 *
 * Ein Plan ist kein Quellcode, sondern Text, den Spark schreibt — zeilenweise, ohne Lexer:
 *
 * - Abschnitte mit Kopf am Zeilenanfang: `== Parsed Logical Plan ==`, `== Analyzed Logical Plan ==` (darunter
 *   eine Zeile mit dem Schema), `== Optimized Logical Plan ==`, `== Physical Plan ==`. Folgen sie aufeinander,
 *   gehoeren sie zu einem Plan (`extended`).
 * - Der Baum: je Zeile ein Operator, davor je Stufe drei Zeichen (`   `, `:  `, `+- `, `:- `). Ein Operator
 *   zwei Stufen unter seinem Eltern ist ein inneres Kind (eine Unterabfrage in dessen Ausdruck).
 *   `*(3) Name` laeuft in WholeStageCodegen Stufe 3, `*Name` in einer ohne Kennung.
 * - `formatted`: im Baum `* Name (n)`, darunter je Operator `(n) Name [codegen id : k]` mit Angaben
 *   (`Output [2]: …`, `Condition : …`), dann `===== Subqueries =====` mit je `Subquery:n Hosting operator id = h
 *   Hosting Expression = …` und ihrem Baum.
 * - AQE: unter `AdaptiveSparkPlan isFinalPlan=true` die Marken `== Final Plan ==` und `== Initial Plan ==` —
 *   gelesen wird der endgueltige (P2-3).
 * - Ohne Kopf: der eingerueckte Baum aus Sparks Pruefdateien (`simplified.txt`, je Stufe zwei Leerzeichen,
 *   `Exchange [...] #1` mit Kennung).
 * - `explain(mode="codegen")` (erzeugter Code) und die Statistik von `cost` werden nicht gezeigt (Hinweis).
 *
 * Was die Operatoren bedeuten, weiss der Leser nicht — das steht in der Ableitung (diagramm/Plan.cpp).
 */
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::sprache
{
namespace
{

using quelle::Meldung;
using quelle::Schwere;

constexpr std::string_view kEndungen[] = {"txt", "out"};

// Die Koepfe der Abschnitte in ihrer Reihenfolge — ein Plan von `extended` hat sie so.
constexpr std::string_view kAbschnitte[] = {"Parsed Logical Plan", "Analyzed Logical Plan", "Optimized Logical Plan",
                                            "Physical Plan"};

// Womit ein eingerueckter Baum ohne Kopf beginnen kann (die Wurzeln in Sparks Pruefdateien und was sonst oben steht).
constexpr std::string_view kWurzeln[] = {
    "AdaptiveSparkPlan", "TakeOrderedAndProject", "CollectLimit", "GlobalLimit", "LocalLimit", "WholeStageCodegen",
    "HashAggregate", "SortAggregate", "ObjectHashAggregate", "Project", "Filter", "Sort", "Exchange",
    "BroadcastExchange", "Union", "Window", "Generate", "Expand", "LocalTableScan", "CommandResult", "Execute",
    "BroadcastHashJoin", "SortMergeJoin", "ShuffledHashJoin", "BroadcastNestedLoopJoin", "CartesianProduct",
    "Coalesce", "ColumnarToRow", "InputAdapter", "Scan", "FileScan", "InMemoryTableScan", "Deduplicate",
};

constexpr lexer::Sprachtabelle kTabelle{
    .schluesselwoerter = {},
    .operatoren = {},
    .zeichenkettenBegrenzer = "",
    .zeichenBegrenzer = "",
};

[[nodiscard]] std::string_view stutze(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

[[nodiscard]] bool leer(std::string_view s)
{
    return stutze(s).empty();
}

[[nodiscard]] bool ziffern(std::string_view s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

[[nodiscard]] bool nameZeichen(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

struct Zeile
{
    std::string_view text;  ///< ohne `\r\n`
    std::size_t versatz = 0;
    int nummer = 0;
};

[[nodiscard]] std::vector<Zeile> zeilenVon(std::string_view text)
{
    std::vector<Zeile> aus;
    std::size_t a = 0;
    int n = 1;
    while (a <= text.size())
    {
        std::size_t e = text.find('\n', a);
        if (e == std::string_view::npos) e = text.size();
        std::string_view z = text.substr(a, e - a);
        if (!z.empty() && z.back() == '\r') z.remove_suffix(1);
        aus.push_back({z, a, n++});
        if (e == text.size()) break;
        a = e + 1;
    }
    return aus;
}

/// Ein Kopf `== Physical Plan ==` am Zeilenanfang (eine Marke `+- == Final Plan ==` ist keiner); -1 = keiner.
[[nodiscard]] int kopfVon(std::string_view zeile)
{
    if (!zeile.starts_with("== ")) return -1;
    const std::string_view t = stutze(zeile);
    if (!t.ends_with(" ==") || t.size() < 6) return -1;
    const std::string_view art = stutze(t.substr(3, t.size() - 6));
    for (std::size_t k = 0; k < std::size(kAbschnitte); ++k)
        if (art == kAbschnitte[k]) return static_cast<int>(k);
    return -1;
}

/// Eine Zeile des Baums: Tiefe, ob vor ihr ein Verbinder (`+- `, `:- `) steht, und ihr Inhalt.
struct Baumzeile
{
    int tiefe = 0;
    bool verbinder = false;
    std::size_t spalte = 0;
    std::string_view inhalt;
};

[[nodiscard]] Baumzeile baumzeile(std::string_view z)
{
    std::size_t i = 0;
    while (i < z.size())
    {
        if (i + 3 <= z.size() && (z.substr(i, 3) == "+- " || z.substr(i, 3) == ":- "))
            return {static_cast<int>(i / 3) + 1, true, i + 3, z.substr(i + 3)};
        if (z[i] == ' ' || z[i] == ':')
        {
            ++i;
            continue;
        }
        break;
    }
    return {static_cast<int>(i / 3), false, i, z.substr(i)};
}

/// Der Operator einer Zeile: Codegen-Marke vorn, Nummer `(n)` und Kennung `#n` hinten, Name, Argumente.
[[nodiscard]] modell::PlanOperator operatorAus(std::string_view s)
{
    modell::PlanOperator op;
    s = stutze(s);
    if (s.starts_with("*("))
    {
        const std::size_t zu = s.find(')');
        if (zu != std::string_view::npos && ziffern(s.substr(2, zu - 2)))
        {
            op.codegen = std::stoi(std::string(s.substr(2, zu - 2)));
            s = stutze(s.substr(zu + 1));
        }
    }
    else if (s.starts_with("* "))
    {
        op.codegen = -1;
        s = stutze(s.substr(2));
    }
    else if (s.size() > 1 && s[0] == '*' && (std::isalpha(static_cast<unsigned char>(s[1])) != 0))
    {
        op.codegen = -1;
        s.remove_prefix(1);
    }
    // `(43)` hinten: die Nummer in formatted
    if (s.ends_with(')'))
    {
        const std::size_t auf = s.rfind(" (");
        if (auf != std::string_view::npos && ziffern(s.substr(auf + 2, s.size() - auf - 3)))
        {
            op.nummer = std::stoi(std::string(s.substr(auf + 2, s.size() - auf - 3)));
            s = stutze(s.substr(0, auf));
        }
    }
    // ` #3` hinten: die Kennung im eingerueckten Baum
    if (const std::size_t raute = s.rfind(" #"); raute != std::string_view::npos && ziffern(s.substr(raute + 2)))
    {
        op.verweis = std::string(s.substr(raute + 2));
        s = stutze(s.substr(0, raute));
    }
    std::size_t j = !s.empty() && s[0] == '\'' ? 1 : 0;
    while (j < s.size() && nameZeichen(s[j])) ++j;
    op.name = std::string(s.substr(0, j));
    op.argumente = std::string(stutze(s.substr(j)));
    return op;
}

/// Eine Angabe in formatted: `Output [3]: [...]`, `Condition : (...)`, `Batched: true`, `Location [not …]/x]`.
[[nodiscard]] std::pair<std::string, std::string> angabeAus(std::string_view t)
{
    std::string_view schluessel;
    std::string_view wert;
    const std::size_t p = t.find(": ");
    const std::size_t q = t.find(" : ");
    if (q != std::string_view::npos && (p == std::string_view::npos || q < p))
    {
        schluessel = t.substr(0, q);
        wert = t.substr(q + 3);
    }
    else if (p != std::string_view::npos)
    {
        schluessel = t.substr(0, p);
        wert = t.substr(p + 2);
    }
    else if (t.ends_with(':'))
        schluessel = t.substr(0, t.size() - 1);
    else
    {
        const std::size_t sp = t.find(' ');
        schluessel = t.substr(0, sp);
        wert = sp == std::string_view::npos ? std::string_view() : t.substr(sp + 1);
    }
    schluessel = stutze(schluessel);
    // die Zahl `[3]` hinter dem Schluessel faellt weg
    if (schluessel.ends_with(']'))
        if (const std::size_t auf = schluessel.rfind(" ["); auf != std::string_view::npos &&
                                                             ziffern(schluessel.substr(auf + 2, schluessel.size() - auf - 3)))
            schluessel = stutze(schluessel.substr(0, auf));
    return {std::string(schluessel), std::string(stutze(wert))};
}

[[nodiscard]] bool istWurzel(std::string_view zeile)
{
    std::string_view s = stutze(zeile);
    std::size_t j = 0;
    while (j < s.size() && nameZeichen(s[j])) ++j;
    const std::string_view wort = s.substr(0, j);
    return std::find(std::begin(kWurzeln), std::end(kWurzeln), wort) != std::end(kWurzeln);
}

/// Der eingerueckte Baum ohne Kopf: die erste Zeile beginnt mit einer Wurzel, die zweite mit genau zwei
/// Leerzeichen, und kein Verbinder `+- ` steht vorn.
[[nodiscard]] bool istEingerueckt(const std::vector<Zeile>& zeilen)
{
    std::size_t i = 0;
    while (i < zeilen.size() && leer(zeilen[i].text)) ++i;
    if (i + 1 >= zeilen.size() || zeilen[i].text.starts_with(' ') || !istWurzel(zeilen[i].text)) return false;
    const std::string_view zweite = zeilen[i + 1].text;
    if (!(zweite.size() > 2 && zweite.starts_with("  ") && zweite[2] != ' ')) return false;
    return std::none_of(zeilen.begin(), zeilen.end(), [](const Zeile& z) { return baumzeile(z.text).verbinder; });
}

class PlanLeser
{
public:
    PlanLeser(std::string_view text, std::vector<Meldung>& meldungen) : m_zeilen(zeilenVon(text)), m_meldungen(meldungen) {}

    /// Alle Plaene mit Kopf.
    std::vector<modell::Plan> mitKopf()
    {
        std::vector<modell::Plan> aus;
        m_i = 0;
        while (m_i < m_zeilen.size())
        {
            const std::string_view t = stutze(m_zeilen[m_i].text);
            if (t.starts_with("Found ") && t.find("WholeStageCodegen subtree") != std::string_view::npos)
                melde(m_zeilen[m_i], Schwere::Hinweis, "explain(mode=\"codegen\") is not shown - the generated code is not a plan");
            if (kopfVon(m_zeilen[m_i].text) >= 0)
            {
                modell::Plan p = liesPlan();
                if (!p.baeume.empty()) aus.push_back(std::move(p));
                continue;
            }
            ++m_i;
        }
        return aus;
    }

    /// Der eingerueckte Baum ohne Kopf als ein Plan; leer = der Text ist keiner.
    std::vector<modell::Plan> eingerueckt()
    {
        if (!istEingerueckt(m_zeilen)) return {};
        modell::Plan p;
        p.modus = "simplified";
        m_i = 0;
        while (m_i < m_zeilen.size() && leer(m_zeilen[m_i].text)) ++m_i;
        p.zeile = m_zeilen[m_i].nummer;
        modell::Planbaum b;
        b.art = std::string(kAbschnitte[3]);
        b.zeile = p.zeile;
        liesEingerueckt(b);
        for (; m_i < m_zeilen.size(); ++m_i)
            if (!leer(m_zeilen[m_i].text))
            {
                melde(m_zeilen[m_i], Schwere::Hinweis, "text after the plan is not read");
                break;
            }
        p.adaptiv = !b.operatoren.empty() && b.operatoren.front().name == "AdaptiveSparkPlan";
        p.baeume.push_back(std::move(b));
        return {std::move(p)};
    }

private:
    void melde(const Zeile& z, Schwere schwere, std::string text)
    {
        const std::size_t spalte = z.text.find_first_not_of(' ');
        m_meldungen.push_back({schwere,
                               {z.versatz + (spalte == std::string_view::npos ? 0 : spalte), z.nummer,
                                static_cast<int>(spalte == std::string_view::npos ? 1 : spalte + 1)},
                               std::move(text)});
    }

    /// Die Abschnitte eines Plans, solange sie in ihrer Reihenfolge aufeinander folgen.
    modell::Plan liesPlan()
    {
        modell::Plan p;
        p.zeile = m_zeilen[m_i].nummer;
        int letzter = -1;
        bool statistik = false;
        bool formatted = false;
        while (m_i < m_zeilen.size())
        {
            const int k = kopfVon(m_zeilen[m_i].text);
            if (k < 0 || k <= letzter) break;
            letzter = k;
            modell::Planbaum b;
            b.art = std::string(kAbschnitte[static_cast<std::size_t>(k)]);
            b.zeile = m_zeilen[m_i].nummer;
            const Zeile& kopf = m_zeilen[m_i];
            ++m_i;
            if (k == 1 && m_i + 1 < m_zeilen.size())
            {
                // unter dem Kopf des analysierten Plans steht sein Schema (auch leer), dann der Baum
                const Baumzeile naechste = baumzeile(m_zeilen[m_i + 1].text);
                if (leer(m_zeilen[m_i].text) || (!naechste.verbinder && naechste.spalte == 0 && !leer(m_zeilen[m_i + 1].text)))
                    ++m_i;
            }
            m_uebergangen.clear();
            liesBaum(b, p);
            if (b.operatoren.empty())
            {
                melde(kopf, Schwere::Hinweis, "no plan under '" + b.art + "'");
                continue;
            }
            for (const modell::PlanOperator& op : b.operatoren)
                if (op.argumente.find("Statistics(") != std::string::npos) statistik = true;
            if (k == 3 && std::any_of(b.operatoren.begin(), b.operatoren.end(), [](const modell::PlanOperator& op) { return op.nummer > 0; }))
            {
                formatted = true;
                liesAngaben(b, p);
            }
            if (k == 3) p.adaptiv = b.operatoren.front().name == "AdaptiveSparkPlan";
            p.baeume.push_back(std::move(b));
            // weiter, wenn nach Leerzeilen der naechste Abschnitt kommt
            std::size_t j = m_i;
            while (j < m_zeilen.size() && leer(m_zeilen[j].text)) ++j;
            if (j < m_zeilen.size() && kopfVon(m_zeilen[j].text) > letzter) m_i = j;
            else break;
        }
        const bool logisch = std::any_of(p.baeume.begin(), p.baeume.end(), [](const modell::Planbaum& b) {
            return b.art == kAbschnitte[0] || b.art == kAbschnitte[1];
        });
        if (statistik) p.modus = "cost";
        else if (logisch) p.modus = "extended";
        else if (formatted) p.modus = "formatted";
        else p.modus = "simple";
        if (statistik)
            melde(m_zeilen[static_cast<std::size_t>(p.zeile - 1)], Schwere::Hinweis,
                  "statistics of explain(mode=\"cost\") are not shown");
        if (const auto phys = std::find_if(p.baeume.begin(), p.baeume.end(), [](const modell::Planbaum& b) { return b.art == kAbschnitte[3]; });
            phys != p.baeume.end() && !phys->operatoren.empty() &&
            phys->operatoren.front().argumente.find("isFinalPlan=true") != std::string::npos)
            p.endgueltig = true;
        return p;
    }

    /// Ein Baum mit Verbindern, bis zur ersten leeren Zeile; gibt seine Wurzel (kKeinBlock = keine).
    std::size_t liesBaum(modell::Planbaum& b, modell::Plan& p)
    {
        struct Stufe
        {
            int tiefe;
            std::size_t index;
        };
        std::vector<Stufe> stapel;
        std::size_t wurzel = modell::kKeinBlock;
        bool erste = true;
        bool endgueltig = false;
        int ueberspringeAb = -1;  // unter `== Initial Plan ==`, wenn es den endgueltigen gibt
        while (m_i < m_zeilen.size())
        {
            const Zeile& z = m_zeilen[m_i];
            if (leer(z.text)) break;
            const Baumzeile bz = baumzeile(z.text);
            if (!erste && !bz.verbinder && bz.spalte == 0) break;  // eine neue Zeile ganz vorn gehoert nicht zum Baum
            const std::string_view inhalt = stutze(bz.inhalt);
            if (ueberspringeAb >= 0)
            {
                if (bz.tiefe >= ueberspringeAb)
                {
                    const modell::PlanOperator op = operatorAus(inhalt);
                    if (op.nummer > 0) m_uebergangen.insert(op.nummer);
                    ++m_i;
                    continue;
                }
                ueberspringeAb = -1;
            }
            if (inhalt == "== Final Plan ==")
            {
                endgueltig = true;
                p.endgueltig = true;
                ++m_i;
                continue;
            }
            if (inhalt == "== Initial Plan ==")
            {
                if (endgueltig) ueberspringeAb = bz.tiefe;
                ++m_i;
                continue;
            }
            modell::PlanOperator op = operatorAus(inhalt);
            op.zeile = z.nummer;
            const int d = bz.tiefe;
            while (!stapel.empty() && stapel.back().tiefe >= d) stapel.pop_back();
            const std::size_t index = b.operatoren.size();
            if (stapel.empty())
            {
                if (wurzel != modell::kKeinBlock)
                {
                    melde(z, Schwere::Hinweis, "a second root in the plan - the rest is not read");
                    break;
                }
                wurzel = index;
            }
            else
            {
                b.operatoren[stapel.back().index].kinder.push_back(index);
                op.innen = d > stapel.back().tiefe + 1;
            }
            b.operatoren.push_back(std::move(op));
            stapel.push_back({d, index});
            erste = false;
            ++m_i;
        }
        return wurzel;
    }

    /// Der eingerueckte Baum: je Stufe zwei Leerzeichen.
    void liesEingerueckt(modell::Planbaum& b)
    {
        std::vector<std::pair<int, std::size_t>> stapel;
        while (m_i < m_zeilen.size())
        {
            const Zeile& z = m_zeilen[m_i];
            if (leer(z.text)) break;
            const std::size_t spalte = z.text.find_first_not_of(' ');
            const int d = static_cast<int>(spalte / 2);
            if (!b.operatoren.empty() && d == 0)
            {
                melde(z, Schwere::Hinweis, "a second root in the plan - the rest is not read");
                break;
            }
            modell::PlanOperator op = operatorAus(z.text.substr(spalte));
            op.zeile = z.nummer;
            while (!stapel.empty() && stapel.back().first >= d) stapel.pop_back();
            const std::size_t index = b.operatoren.size();
            if (!stapel.empty()) b.operatoren[stapel.back().second].kinder.push_back(index);
            b.operatoren.push_back(std::move(op));
            stapel.emplace_back(d, index);
            ++m_i;
        }
    }

    /// Hinter dem Baum von formatted: je Operator `(n) Name [...]` mit Angaben, dann die Unterabfragen.
    void liesAngaben(modell::Planbaum& b, modell::Plan& p)
    {
        while (m_i < m_zeilen.size())
        {
            while (m_i < m_zeilen.size() && leer(m_zeilen[m_i].text)) ++m_i;
            if (m_i >= m_zeilen.size()) break;
            const Zeile& z = m_zeilen[m_i];
            const std::string_view t = stutze(z.text);
            if (t.starts_with('(') && t.find(')') != std::string_view::npos && ziffern(t.substr(1, t.find(')') - 1)))
            {
                const std::size_t zu = t.find(')');
                const int nummer = std::stoi(std::string(t.substr(1, zu - 1)));
                const std::string_view rest = stutze(t.substr(zu + 1));
                int codegen = 0;
                std::string verweis;
                if (rest.ends_with(']'))
                    if (const std::size_t auf = rest.rfind(" ["); auf != std::string_view::npos)
                    {
                        const std::string_view klammer = rest.substr(auf + 2, rest.size() - auf - 3);
                        if (klammer.starts_with("codegen id : ") && ziffern(stutze(klammer.substr(13))))
                            codegen = std::stoi(std::string(stutze(klammer.substr(13))));
                        else if (klammer.starts_with("Reuses operator id: ") && ziffern(stutze(klammer.substr(20))))
                            verweis = std::string(stutze(klammer.substr(20)));
                    }
                const auto op = std::find_if(b.operatoren.begin(), b.operatoren.end(),
                                             [&](const modell::PlanOperator& o) { return o.nummer == nummer; });
                if (op == b.operatoren.end() && !m_uebergangen.contains(nummer))
                    melde(z, Schwere::Hinweis, "(" + std::to_string(nummer) + ") is not in the tree of the plan");
                ++m_i;
                std::vector<std::pair<std::string, std::string>> angaben;
                while (m_i < m_zeilen.size() && !leer(m_zeilen[m_i].text))
                {
                    angaben.push_back(angabeAus(stutze(m_zeilen[m_i].text)));
                    ++m_i;
                }
                if (op != b.operatoren.end())
                {
                    if (codegen > 0) op->codegen = codegen;
                    if (!verweis.empty()) op->verweis = verweis;
                    op->angaben = std::move(angaben);
                }
                continue;
            }
            if (t == "===== Subqueries =====")
            {
                ++m_i;
                continue;
            }
            if (t.starts_with("Subquery:"))
            {
                modell::PlanUnterabfrage u;
                u.zeile = z.nummer;
                std::size_t j = 9;
                while (j < t.size() && std::isdigit(static_cast<unsigned char>(t[j])) != 0) ++j;
                if (j > 9) u.nummer = std::stoi(std::string(t.substr(9, j - 9)));
                constexpr std::string_view kGast = "Hosting operator id = ";
                constexpr std::string_view kAusdruck = "Hosting Expression = ";
                if (const std::size_t g = t.find(kGast); g != std::string_view::npos)
                {
                    std::size_t e = g + kGast.size();
                    const std::size_t a = e;
                    while (e < t.size() && std::isdigit(static_cast<unsigned char>(t[e])) != 0) ++e;
                    if (e > a) u.gastgeber = std::stoi(std::string(t.substr(a, e - a)));
                }
                if (const std::size_t x = t.find(kAusdruck); x != std::string_view::npos)
                    u.ausdruck = std::string(stutze(t.substr(x + kAusdruck.size())));
                ++m_i;
                if (m_i < m_zeilen.size() && !leer(m_zeilen[m_i].text)) u.wurzel = liesBaum(b, p);
                b.unterabfragen.push_back(std::move(u));
                continue;
            }
            break;
        }
    }

    std::vector<Zeile> m_zeilen;
    std::vector<Meldung>& m_meldungen;
    std::size_t m_i = 0;
    std::set<int> m_uebergangen;  ///< Nummern unter `== Initial Plan ==`: ihre Angaben haben keinen Operator
};

class SparkPlan final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "Spark plan"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungen; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return kTabelle; }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName, const LeseOptionen&) const override
    {
        Ergebnis ergebnis;
        ergebnis.einheit.name = std::string(einheitName);
        PlanLeser leser(text, ergebnis.meldungen);
        std::vector<modell::Plan> plaene = leser.mitKopf();
        if (plaene.empty()) plaene = leser.eingerueckt();
        if (plaene.empty())
            ergebnis.meldungen.push_back({Schwere::Hinweis, {}, "no Spark plan in the file (== Physical Plan ==)"});
        for (modell::Plan& p : plaene)
            p.name = plaene.size() == 1 ? std::string(einheitName) : std::string(einheitName) + "@" + std::to_string(p.zeile);
        ergebnis.einheit.plaene = std::move(plaene);
        return ergebnis;
    }
};

}  // namespace

const Sprache& sparkPlan()
{
    static const SparkPlan sprache;
    return sprache;
}

bool plantext(std::string_view text)
{
    const std::vector<Zeile> zeilen = zeilenVon(text);
    if (std::any_of(zeilen.begin(), zeilen.end(), [](const Zeile& z) { return kopfVon(z.text) >= 0; })) return true;
    return istEingerueckt(zeilen);
}

std::vector<modell::Plan> liesPlaene(std::string_view text, std::vector<quelle::Meldung>& meldungen)
{
    return PlanLeser(text, meldungen).mitKopf();
}

}  // namespace c2d::sprache
