/**
 * @file Ecma.cpp
 * @brief Der Leser von JavaScript und TypeScript in rekursivem Abstieg
 *
 * Der Lexer liefert Zeilenenden (ausser in `(` und `[`); der Leser nimmt sie heraus und
 * merkt sich je Token, ob eine neue Zeile davor beginnt — damit endet eine Anweisung ohne
 * `;` dort, wo JavaScript eins einsetzt (eine neue Zeile, die die Anweisung nicht fortsetzen
 * kann). Ausdruecke bleiben Text; Funktionen darin (Pfeil, `function`, Methoden eines
 * Objekts) werden eigene Funktionen, im Text stehen ihre Ruempfe als `{…}`.
 */
#include "Ecma.hpp"

#include "../Leser.hpp"
#include "../Logik.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace c2d::sprache::ecma
{

namespace
{

using namespace c2d::modell;
using quelle::Schwere;

// Die Woerter, die immer Schluesselwoerter sind; let, async, await, yield, of, get, set, static
// und alles, was TypeScript dazubringt (type, interface, enum, namespace …), sind weich.
constexpr std::string_view kSchluesselwoerter[] = {
    "break",  "case",   "catch",  "class",  "const",      "continue", "debugger", "default", "delete",
    "do",     "else",   "export", "extends", "false",     "finally",  "for",      "function", "if",
    "import", "in",     "instanceof", "new", "null",      "return",   "super",    "switch",  "this",
    "throw",  "true",   "try",    "typeof", "var",        "void",     "while",    "with",
};

constexpr std::string_view kOperatoren[] = {
    ">>>=", "...", "===", "!==", "**=", "<<=", ">>=", ">>>", "&&=", "||=", "?\?=", "=>", "==", "!=",
    "<=",   ">=",  "&&",  "||",  "??",  "?.",  "++",  "--",  "+=",  "-=",  "*=",  "/=", "%=", "&=",
    "|=",   "^=",  "**",  "<<",  ">>",  "{",   "}",   "(",   ")",   "[",   "]",   ";",  ",",  "<",
    ">",    "+",   "-",   "*",   "/",   "%",   "&",   "|",   "^",   "!",   "~",   "?",  ":",  "=",
    ".",    "@",   "#",
};

/// Hinter diesen Woertern beginnt `/` ein Regex-Literal (sonst nach einem Wert: die Division).
constexpr std::string_view kVorRegex[] = {"return", "typeof", "case", "do",   "else", "in",
                                          "instanceof", "new", "delete", "void", "throw"};

constexpr lexer::Sprachtabelle kTabelle{
    .schluesselwoerter = kSchluesselwoerter,
    .operatoren = kOperatoren,
    .zeilenKommentar = "//",
    .blockKommentarAuf = "/*",
    .blockKommentarZu = "*/",
    .zeichenkettenBegrenzer = "\"'",
    .zeichenBegrenzer = "",
    .ziffernTrenner = '_',
    .zeilenEnde = true,
    .klammernOhneZeilenende = "([",
    .vorlagenZeichenketten = true,
    .regexLiterale = true,
    .woerterVorRegex = kVorRegex,
    .namensZeichen = "$",
};

constexpr std::string_view kZuweisungen[] = {"=",   "+=",  "-=",   "*=", "/=", "%=",  "**=", "<<=",
                                             ">>=", ">>>=", "&=", "|=", "^=", "&&=", "||=", "?\?="};

constexpr Ausdrucksregeln kRegeln{
    .wahl = Ausdrucksregeln::Wahl::Fragezeichen,
    .nichtUmfasstVergleich = false,
    .zuweisungen = kZuweisungen,
    .zeiger = false,
};

/// Steht einer davon am Anfang der naechsten Zeile, geht die Anweisung weiter (kein `;` dazwischen).
constexpr std::string_view kFortsetzer[] = {
    ".",  "?.", "(",  "[",  ",",   "?",   ":",  "=",  "=>", "+",   "-",   "*",   "/",  "%",  "**", "&&",
    "||", "??", "==", "===", "!=", "!==", "<",  ">",  "<=", ">=",  "&",   "|",   "^",  "<<", ">>", ">>>",
    "+=", "-=", "*=", "/=", "%=",  "**=", "<<=", ">>=", ">>>=", "&=", "|=", "^=", "&&=", "||=", "?\?="};

/// Woerter vor einem Glied einer Klasse (TypeScript und JavaScript).
constexpr std::string_view kModifikatoren[] = {"public",   "private", "protected", "static",   "readonly",
                                               "abstract", "async",   "override",  "declare", "accessor"};

/// Der Name, unter dem JavaScript den Code auf oberster Ebene fuehrt (wie Python).
constexpr std::string_view kModul = "<module>";

template <std::size_t N>
bool enthaelt(const std::string_view (&liste)[N], std::string_view wort)
{
    return std::find(std::begin(liste), std::end(liste), wort) != std::end(liste);
}

class EcmaLeser final : public Leser
{
public:
    EcmaLeser(std::string_view text, Dialekt dialekt, Ergebnis& aus) : Leser(text, kTabelle, aus), m_ts(dialekt.ts)
    {
        // die Zeilenenden heraus; je Token, ob vor ihm eine neue Zeile beginnt
        std::vector<Token> ohne;
        bool neu = false;
        for (const Token& t : m_t)
        {
            if (t.art == TokenArt::Zeilenende)
            {
                neu = true;
                continue;
            }
            ohne.push_back(t);
            m_neueZeile.push_back(neu);
            neu = false;
        }
        m_t = std::move(ohne);
    }

    void liesModul()
    {
        const Ort modul;
        std::vector<AnweisungPtr> code;
        liesFolge(modul, code, false);
        if (!m_ablauf || code.empty()) return;
        Funktion f;
        f.name = std::string(kModul);
        f.zeile = code.front()->zeile;
        f.bisZeile = zeileVor(m_p);
        f.rumpf = block(std::move(code), m_t.front());
        f.rumpf->zeile = f.zeile;
        m_aus.einheit.funktionen.push_back(std::move(f));
    }

private:
    enum class Ebene
    {
        Modul,
        Klasse,
        Funktion,
    };

    struct Ort
    {
        Ebene ebene = Ebene::Modul;
        std::string bereich;         ///< Namensraum, in einer Klasse ihr voller Name
        std::size_t typ = kKeiner;   ///< in einer Klasse: ihr Platz unter Einheit::typen
        std::string funktion;        ///< der volle Name der Funktion, in der gelesen wird
    };

    // --- Token -----------------------------------------------------------------------

    [[nodiscard]] bool neueZeile(std::size_t i) const { return i < m_neueZeile.size() && m_neueZeile[i]; }

    [[nodiscard]] bool istName(std::size_t i, std::string_view name) const
    {
        return tok(i).art == TokenArt::Bezeichner && tok(i).text == name;
    }

    /// Kann das Token ein Name sein (auch ein Schluesselwort als Name eines Glieds: `delete() {}`)?
    [[nodiscard]] bool namenhaft(std::size_t i) const
    {
        const TokenArt a = tok(i).art;
        return a == TokenArt::Bezeichner || a == TokenArt::Schluesselwort;
    }

    [[nodiscard]] int zeileVor(std::size_t i) const { return i > 0 ? tok(i - 1).ort.zeile : 0; }

    /// i auf einer oeffnenden Klammer: dahinter; fehlt die schliessende, eine Meldung.
    std::size_t ueberKlammer(std::size_t i)
    {
        const std::size_t zu = nachKlammer(i);
        if (zu == i + 1 || !schliesst(tok(zu - 1)))
            melde(Schwere::Fehler, tok(i), "'" + std::string(tok(i).text) + "' is not closed");
        return zu;
    }

    /// i auf `<` von Typ-Parametern (`<T, U extends X<Y>>`): dahinter; sonst i.
    [[nodiscard]] std::size_t spitzeEnde(std::size_t i) const
    {
        if (!tok(i).istOp("<")) return i;
        int tiefe = 0;
        for (std::size_t j = i; !amEnde(j); ++j)
        {
            const Token& t = tok(j);
            if (t.istOp("(") || t.istOp("[") || t.istOp("{"))
            {
                j = nachKlammer(j) - 1;
                continue;
            }
            if (t.istOp("<")) ++tiefe;
            else if (t.istOp(">")) tiefe -= 1;
            else if (t.istOp(">>")) tiefe -= 2;
            else if (t.istOp(">>>")) tiefe -= 3;
            else if (t.istOp(";")) return j;
            if (tiefe <= 0) return j + 1;
        }
        return i;
    }

    /// Ein Typ ab i (hinter `:`): bis zu dem, was ihn beendet — `,`, `)`, `]`, `;`, `=`, `}`, `?`, der
    /// Rumpf `{`, ein `=>` hinter dem Typ, eine neue Zeile, die ihn nicht fortsetzt. `{` am Anfang
    /// eines Typs ist ein Objekttyp, `(…) =>` ein Funktionstyp.
    [[nodiscard]] std::size_t typEnde(std::size_t i)
    {
        std::size_t j = i;
        bool wert = false;           // ein Typ steht schon da
        bool funktionsTyp = false;   // er begann mit einer Klammer: `=>` gehoert zu ihm
        while (!amEnde(j))
        {
            const Token& t = tok(j);
            if (j > i && wert && neueZeile(j) && !(t.istOp("|") || t.istOp("&") || t.istOp(".") || t.istOp("<") ||
                                                    t.istOp("[") || t.istOp("=>")))
                return j;
            if (t.art != TokenArt::Operator)
            {
                wert = true;
                ++j;
                continue;
            }
            if (t.istOp("<"))
            {
                const std::size_t e = spitzeEnde(j);
                j = e > j ? e : j + 1;
                wert = true;
                continue;
            }
            if (t.istOp("(") || t.istOp("["))
            {
                funktionsTyp = !wert && t.istOp("(");
                j = ueberKlammer(j);
                wert = true;
                continue;
            }
            if (t.istOp("{"))
            {
                if (wert) return j;
                j = ueberKlammer(j);
                wert = true;
                continue;
            }
            if (t.istOp("=>"))
            {
                if (!funktionsTyp) return j;
                funktionsTyp = false;
                wert = false;
                ++j;
                continue;
            }
            if (t.istOp("|") || t.istOp("&"))
            {
                wert = false;
                ++j;
                continue;
            }
            if (t.istOp(".") || t.istOp("...") || t.istOp("!"))
            {
                ++j;
                continue;
            }
            return j;
        }
        return j;
    }

    /// Setzt die Anweisung ueber den Zeilenwechsel vor j fort (dann kein eingesetztes `;`)?
    [[nodiscard]] bool fortsetzung(std::size_t j) const
    {
        const Token& davor = tok(j - 1);
        const Token& t = tok(j);
        if (davor.istWort("return") || davor.istWort("break") || davor.istWort("continue")) return false;
        // ein Operator am Ende der Zeile verlangt mehr — ausser einem, der einen Wert abschliesst
        if (davor.art == TokenArt::Operator &&
            !(davor.istOp(")") || davor.istOp("]") || davor.istOp("}") || davor.istOp("++") || davor.istOp("--")))
            return true;
        if (davor.istWort("new") || davor.istWort("typeof") || davor.istWort("instanceof") || davor.istWort("in") ||
            davor.istWort("delete") || davor.istWort("void") || davor.istWort("extends"))
            return true;
        if (t.art == TokenArt::Operator) return enthaelt(kFortsetzer, t.text);
        return t.istWort("instanceof") || t.istWort("in");
    }

    /// Ab i bis zum Ende der Anweisung: `;` (nicht genommen), eine schliessende Klammer ohne
    /// oeffnende, eine neue Zeile, die nicht fortsetzt, oder das Ende.
    std::size_t anweisungsEnde(std::size_t i)
    {
        std::size_t j = i;
        while (!amEnde(j))
        {
            const Token& t = tok(j);
            if (j > i && neueZeile(j) && !fortsetzung(j)) return j;
            if (t.istOp(";") || schliesst(t)) return j;
            if (oeffnet(t))
            {
                j = ueberKlammer(j);
                continue;
            }
            ++j;
        }
        return j;
    }

    /// Die erste Stelle in [a, b) mit diesem Operator auf oberster Ebene (auch ausserhalb von `<…>`,
    /// wenn spitz); b = keine.
    [[nodiscard]] std::size_t oben(std::size_t a, std::size_t b, std::string_view op, bool spitz = false) const
    {
        int spitze = 0;
        for (std::size_t i = a; i < b; ++i)
        {
            if (oeffnet(tok(i)))
            {
                i = nachKlammer(i) - 1;
                continue;
            }
            if (spitz && tok(i).istOp("<")) ++spitze;
            if (spitz && tok(i).istOp(">") && spitze > 0) --spitze;
            if (spitz && tok(i).istOp(">>")) spitze = std::max(0, spitze - 2);
            if (spitze == 0 && tok(i).istOp(op)) return i;
        }
        return b;
    }

    /// [a, b) an den Kommas auf oberster Ebene teilen, auch nicht in `<…>` (Typen).
    [[nodiscard]] std::vector<Bereich> teileSpitz(std::size_t a, std::size_t b) const
    {
        std::vector<Bereich> stuecke;
        while (a < b)
        {
            const std::size_t komma = oben(a, b, ",", true);
            if (komma > a) stuecke.emplace_back(a, komma);
            a = komma + 1;
        }
        return stuecke;
    }

    static AnweisungPtr neu(AnweisungArt art, const Token& bei, std::string text = {})
    {
        auto a = std::make_unique<Anweisung>();
        a->art = art;
        a->text = std::move(text);
        a->zeile = bei.ort.zeile;
        return a;
    }

    static AnweisungPtr block(std::vector<AnweisungPtr> kinder, const Token& bei)
    {
        AnweisungPtr b = neu(AnweisungArt::Block, bei);
        b->kinder = std::move(kinder);
        return b;
    }

    void nimmSemikolon() { nimm(";"); }

    // --- Funktionen in Ausdruecken -----------------------------------------------------

    /// Der Name, dem eine Funktion ab `anfang` zugewiesen wird: `NAME =`, `x.NAME =`,
    /// `const NAME: T =`, `NAME:` in einem Objekt. Leer = keiner.
    [[nodiscard]] std::string nameVor(std::size_t anfang, std::size_t a) const
    {
        if (anfang <= a) return {};
        if (tok(anfang - 1).istOp("="))
        {
            // const NAME: Typ = … — der Name vor dem `:` auf oberster Ebene, nicht der Typ
            const std::size_t doppelpunkt = oben(a, anfang - 1, ":");
            if (doppelpunkt < anfang - 1 && doppelpunkt > a && tok(doppelpunkt - 1).art == TokenArt::Bezeichner)
                return std::string(tok(doppelpunkt - 1).text);
            if (anfang >= a + 2 && tok(anfang - 2).art == TokenArt::Bezeichner) return std::string(tok(anfang - 2).text);
            return {};
        }
        // `NAME: (…) => {}` in einem Objekt (nicht der zweite Teil von `c ? x : y`)
        if (tok(anfang - 1).istOp(":") && anfang >= a + 2 &&
            (tok(anfang - 2).art == TokenArt::Bezeichner || tok(anfang - 2).art == TokenArt::Zeichenkette) &&
            (anfang == a + 2 || tok(anfang - 3).istOp("{") || tok(anfang - 3).istOp(",")))
        {
            std::string name(tok(anfang - 2).text);
            if (tok(anfang - 2).art == TokenArt::Zeichenkette && name.size() >= 2) name = name.substr(1, name.size() - 2);
            return name;
        }
        return {};
    }

    std::string anonymerName(const Token& bei) const
    {
        std::string name = "<anonymous:" + std::to_string(bei.ort.zeile) + ">";
        const auto& f = m_aus.einheit.funktionen;
        if (std::any_of(f.begin(), f.end(), [&](const Funktion& x) { return x.name == name; }))
            name = "<anonymous:" + std::to_string(bei.ort.zeile) + ":" + std::to_string(bei.ort.spalte) + ">";
        return name;
    }

    /// Ein Parameter: `x`, `x = 3`, `...rest`, `{a, b}: Opts`, in TypeScript `x?: T`; ohne
    /// Vorgabewert (wie in der C-Familie). `modifikator`: ein Wort davor (`private`) — ein Feld.
    [[nodiscard]] Parameter parameter(const Bereich& s, std::string* modifikator = nullptr) const
    {
        std::size_t a = s.first;
        while (a + 1 < s.second && tok(a).art == TokenArt::Bezeichner && enthaelt(kModifikatoren, tok(a).text) &&
               tok(a + 1).art != TokenArt::Operator)
        {
            // die Sichtbarkeit zaehlt; readonly allein macht auch ein Feld
            const std::string_view w = tok(a).text;
            if (modifikator != nullptr && (modifikator->empty() || w == "public" || w == "private" || w == "protected"))
                *modifikator = std::string(w);
            ++a;
        }
        while (a < s.second && tok(a).istOp("@"))  // ein Dekorator am Parameter: @Name.x(…)
        {
            ++a;
            while (a < s.second && (tok(a).art == TokenArt::Bezeichner || tok(a).istOp("."))) ++a;
            if (a < s.second && tok(a).istOp("(")) a = nachKlammer(a);
        }
        Parameter p;
        p.typHinten = true;
        const std::size_t gleich = oben(a, s.second, "=");
        const std::size_t doppelpunkt = m_ts ? oben(a, gleich, ":") : gleich;
        p.name = textVon(a, doppelpunkt);
        if (doppelpunkt < gleich) p.typ = textVon(doppelpunkt + 1, gleich);
        return p;
    }

    std::vector<Parameter> parameterIn(std::size_t auf, std::size_t zu) const
    {
        std::vector<Parameter> aus;
        const std::size_t innen = tok(zu - 1).istOp(")") ? zu - 1 : zu;
        for (const Bereich& s : teileSpitz(auf + 1, innen)) aus.push_back(parameter(s));
        return aus;
    }

    /// Eine neue Funktion an ihrem Platz in der Reihenfolge der Quelle: Bereich und aussen nach dem Ort.
    std::size_t neueFunktion(Funktion f, const Ort& ort)
    {
        if (ort.ebene == Ebene::Funktion)
        {
            f.aussen = ort.funktion;
            f.bereich = ort.funktion;  // so bleibt der volle Name eindeutig (`aussen::innen`)
        }
        else
            f.bereich = ort.bereich;
        m_aus.einheit.funktionen.push_back(std::move(f));
        return m_aus.einheit.funktionen.size() - 1;
    }

    /// Liest den Rumpf ab `rumpf` (`{`) in die Funktion an Platz `platz`; liefert die Stelle dahinter.
    std::size_t liesRumpfVon(std::size_t platz, std::size_t rumpf, const Ort& ort)
    {
        Ort innen;
        innen.ebene = Ebene::Funktion;
        innen.bereich = ort.bereich;
        innen.typ = ort.typ;
        innen.funktion = m_aus.einheit.funktionen[platz].vollerName();
        const std::size_t gemerkt = m_p;
        m_p = rumpf;
        AnweisungPtr r = liesBlock(innen);
        const int bis = m_letzteZeile;
        const std::size_t danach = m_p;
        m_p = gemerkt;
        m_aus.einheit.funktionen[platz].rumpf = std::move(r);
        m_aus.einheit.funktionen[platz].bisZeile = bis;
        return danach;
    }

    /// Der Text von [a, b) — eine Funktion mit Block-Rumpf darin wird eine eigene Funktion, im Text
    /// steht ihr Rumpf als `{…}`. `vorgabe`: der Name einer Funktion, die genau bei a beginnt
    /// (der Anfangswert eines Felds).
    std::string funktionsText(std::size_t a, std::size_t b, const Ort& ort, const std::string& vorgabe = {})
    {
        std::vector<Bereich> ruempfe;
        std::vector<std::string> innere;  // erst am Ende in m_innere: die Ruempfe lesen eigene Anweisungen
        std::size_t j = a;
        while (j < b && !amEnde(j))
        {
            const Token& t = tok(j);
            std::size_t anfang = j;
            std::size_t rumpf = kKeiner;
            std::size_t auf = kKeiner;  // die Klammer der Parameter
            std::size_t zu = kKeiner;
            std::string name;
            std::string vorsatz;  // get, set
            if (t.istOp("=>") && tok(j + 1).istOp("{") && j > a)
            {
                rumpf = j + 1;
                if (tok(j - 1).art == TokenArt::Bezeichner && !(j >= a + 2 && tok(j - 2).istOp(":")))  // x => {
                {
                    anfang = j - 1;
                    auf = anfang;
                    zu = j;
                }
                else
                {
                    // (…) => { oder (…): T => {: die letzte `)` davor
                    std::size_t k = j - 1;
                    while (k > a && !tok(k).istOp(")") && !tok(k).istOp("=") && !tok(k).istOp(",") && !tok(k).istOp("("))
                        --k;
                    if (tok(k).istOp(")"))
                    {
                        int tiefe = 0;
                        std::size_t o = k;
                        while (true)
                        {
                            if (schliesst(tok(o))) ++tiefe;
                            else if (oeffnet(tok(o)) && --tiefe == 0) break;
                            if (o == a) break;
                            --o;
                        }
                        auf = o;
                        zu = k + 1;
                        anfang = spitzeVor(o, a);
                    }
                }
                if (anfang > a && istName(anfang - 1, "async")) --anfang;
            }
            else if (t.istWort("function"))
            {
                std::size_t k = j + 1;
                if (tok(k).istOp("*")) ++k;
                if (tok(k).art == TokenArt::Bezeichner) name = std::string(tok(k++).text);
                k = spitzeEnde(k);
                if (tok(k).istOp("("))
                {
                    auf = k;
                    zu = ueberKlammer(k);
                    k = zu;
                    if (m_ts && tok(k).istOp(":")) k = typEnde(k + 1);
                    if (tok(k).istOp("{")) rumpf = k;
                }
                if (anfang > a && istName(anfang - 1, "async")) --anfang;
            }
            else if (namenhaft(j) && tok(j + 1).istOp("(") && j > a)
            {
                // eine Methode in einem Objekt: `{ name(…) {…}`, auch mit get, set, async, *
                std::size_t vor = j - 1;
                if ((istName(vor, "get") || istName(vor, "set") || istName(vor, "async") || tok(vor).istOp("*")) && vor > a)
                {
                    if (istName(vor, "get") || istName(vor, "set")) vorsatz = std::string(tok(vor).text) + " ";
                    --vor;
                }
                if (tok(vor).istOp("{") || tok(vor).istOp(","))
                {
                    const std::size_t pz = ueberKlammer(j + 1);
                    std::size_t k = pz;
                    if (m_ts && tok(k).istOp(":")) k = typEnde(k + 1);
                    if (tok(k).istOp("{"))
                    {
                        rumpf = k;
                        auf = j + 1;
                        zu = pz;
                        name = vorsatz + std::string(t.text);
                    }
                }
            }
            if (rumpf == kKeiner)
            {
                ++j;
                continue;
            }
            if (name.empty()) name = nameVor(anfang, a);
            if (name.empty() && anfang == a) name = vorgabe;
            Funktion f;
            f.name = name.empty() ? anonymerName(tok(anfang)) : name;
            f.zeile = tok(anfang).ort.zeile;
            if (auf != kKeiner)
            {
                if (tok(auf).istOp("("))
                    f.parameter = parameterIn(auf, zu);
                else
                    f.parameter.push_back(parameter({auf, zu}));
            }
            const std::size_t platz = neueFunktion(std::move(f), ort);
            innere.push_back(m_aus.einheit.funktionen[platz].vollerName());
            const std::size_t danach = liesRumpfVon(platz, rumpf, ort);
            ruempfe.emplace_back(rumpf, danach);
            j = danach;
        }
        m_innere.insert(m_innere.end(), innere.begin(), innere.end());
        // der Text, die Ruempfe als {…}
        std::string s;
        std::size_t r = 0;
        for (std::size_t i = a; i < b && !amEnde(i); ++i)
        {
            if (i > a && m_t[i].abstandDavor) s += ' ';
            if (r < ruempfe.size() && i == ruempfe[r].first)
            {
                s += "{\xE2\x80\xA6}";
                i = ruempfe[r].second - 1;
                ++r;
                continue;
            }
            s += m_t[i].text;
        }
        return s;
    }

    /// Vor der Klammer `o` der Parameter einer Pfeilfunktion: Typ-Parameter `<T>(…) =>`.
    [[nodiscard]] std::size_t spitzeVor(std::size_t o, std::size_t a) const
    {
        if (o <= a || !(tok(o - 1).istOp(">"))) return o;
        int tiefe = 0;
        for (std::size_t k = o - 1; k > a; --k)
        {
            if (tok(k).istOp(">")) ++tiefe;
            else if (tok(k).istOp("<") && --tiefe == 0) return k;
        }
        return o;
    }

    // --- Folgen und Anweisungen ----------------------------------------------------------

    /// Anweisungen bis zur `}` des Blocks (nicht genommen) oder bis zum Ende.
    void liesFolge(const Ort& ort, std::vector<AnweisungPtr>& aus, bool imBlock)
    {
        while (!amEnde(m_p))
        {
            if (imBlock && akt().istOp("}")) break;
            const std::size_t davor = m_p;
            const std::size_t vorher = aus.size();
            liesAnweisung(ort, aus);
            // was in der Anweisung definiert wurde, gehoert ihr (eine Steuerung: ihrem Kopf)
            if (aus.size() > vorher) aus[vorher]->innere = std::move(m_innere);
            m_innere.clear();
            if (m_p == davor)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
            }
        }
    }

    /// m_p auf `{`: der Block.
    AnweisungPtr liesBlock(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        std::vector<AnweisungPtr> kinder;
        liesFolge(ort, kinder, true);
        m_letzteZeile = akt().ort.zeile;
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
        return block(std::move(kinder), t);
    }

    /// Der Rumpf von if, else, while, for: nie leer.
    AnweisungPtr liesRumpf(const Ort& ort)
    {
        const Token& t = akt();
        std::vector<AnweisungPtr> v;
        liesAnweisung(ort, v);
        if (v.size() == 1) return std::move(v.front());
        return block(std::move(v), t);
    }

    /// `( … )`: der Text dazwischen.
    std::string liesBedingung(const Ort& ort)
    {
        if (!akt().istOp("("))
        {
            melde(Schwere::Fehler, akt(), "'(' expected");
            return {};
        }
        const std::size_t zu = ueberKlammer(m_p);
        std::string text = funktionsText(m_p + 1, tok(zu - 1).istOp(")") ? zu - 1 : zu, ort);
        m_p = zu;
        return text;
    }

    void ablauf(const Ort& ort)
    {
        if (ort.ebene == Ebene::Modul) m_ablauf = true;
    }

    void liesAnweisung(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        if (t.istOp(";"))
        {
            ++m_p;
            return;
        }
        if (t.istOp("@"))
        {
            liesDekorator();
            return;
        }
        if (t.istOp("{"))
        {
            aus.push_back(liesBlock(ort));
            ablauf(ort);
            return;
        }
        if (t.istWort("export"))
        {
            liesExport(ort, aus);
            return;
        }
        if (t.istWort("import") && !tok(m_p + 1).istOp("(") && !tok(m_p + 1).istOp("."))
        {
            const std::size_t e = anweisungsEnde(m_p);
            einbindungAus(m_p, e);
            if (ort.ebene == Ebene::Funktion) aus.push_back(neu(AnweisungArt::Einfach, t, textVon(m_p, e)));
            m_p = e;
            nimmSemikolon();
            return;
        }
        if (liesTsDeklaration(ort, aus)) return;
        if (t.istWort("function") || (istName(m_p, "async") && tok(m_p + 1).istWort("function") && !neueZeile(m_p + 1)))
        {
            liesFunktionsDeklaration(ort, aus);
            return;
        }
        if (t.istWort("class") || (istName(m_p, "abstract") && tok(m_p + 1).istWort("class")))
        {
            liesKlasse(ort, aus);
            return;
        }
        if (AnweisungPtr a = liesSteuerung(ort))
        {
            aus.push_back(std::move(a));
            ablauf(ort);
            return;
        }
        liesEinfache(ort, aus);
    }

    /// if, while, do, for, switch, try, return, throw, break, continue, eine Marke; sonst nullptr
    /// (m_p unveraendert).
    AnweisungPtr liesSteuerung(const Ort& ort)
    {
        const Token& t = akt();
        if (t.istWort("if"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Wenn, t);
            a->text = liesBedingung(ort);
            a->kinder.push_back(liesRumpf(ort));
            if (akt().istWort("else"))
            {
                ++m_p;
                a->sonst = liesRumpf(ort);
            }
            return a;
        }
        if (t.istWort("while"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Solange, t);
            a->text = liesBedingung(ort);
            a->kinder.push_back(liesRumpf(ort));
            return a;
        }
        if (t.istWort("do"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::TueSolange, t);
            a->kinder.push_back(liesRumpf(ort));
            if (akt().istWort("while"))
            {
                ++m_p;
                a->text = liesBedingung(ort);
            }
            else
                melde(Schwere::Fehler, akt(), "'while' of the do loop is missing");
            nimmSemikolon();
            return a;
        }
        if (t.istWort("for")) return liesFuer(ort);
        if (t.istWort("switch")) return liesAuswahl(ort);
        if (t.istWort("try")) return liesVersuch(ort);
        if (t.istWort("return") || t.istWort("throw"))
        {
            ++m_p;
            const bool leer = akt().istOp(";") || akt().istOp("}") || neueZeile(m_p) || amEnde(m_p);
            const std::size_t e = leer ? m_p : anweisungsEnde(m_p);
            AnweisungPtr a = neu(t.istWort("return") ? AnweisungArt::Ruecksprung : AnweisungArt::Wurf, t,
                                 funktionsText(m_p, e, ort));
            m_p = e;
            nimmSemikolon();
            return a;
        }
        if (t.istWort("break") || t.istWort("continue"))
        {
            ++m_p;
            if (tok(m_p).art == TokenArt::Bezeichner && !neueZeile(m_p))
            {
                melde(Schwere::Hinweis, akt(),
                      std::string(t.text) + " " + std::string(akt().text) +
                          ": shown as a " + std::string(t.text) + " of the innermost loop or switch");
                ++m_p;
            }
            nimmSemikolon();
            return neu(t.istWort("break") ? AnweisungArt::Abbruch : AnweisungArt::Fortsetzung, t);
        }
        if (t.art == TokenArt::Bezeichner && tok(m_p + 1).istOp(":"))
        {
            m_p += 2;
            return neu(AnweisungArt::Marke, t, std::string(t.text));
        }
        return nullptr;
    }

    AnweisungPtr liesFuer(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        const bool warten = istName(m_p, "await");
        if (warten) ++m_p;
        AnweisungPtr a = neu(AnweisungArt::Fuer, t);
        if (!akt().istOp("("))
        {
            melde(Schwere::Fehler, akt(), "'(' expected");
            a->kinder.push_back(liesRumpf(ort));
            return a;
        }
        const std::size_t zu = ueberKlammer(m_p);
        const std::size_t ende = tok(zu - 1).istOp(")") ? zu - 1 : zu;
        const std::size_t s1 = std::min(suche(m_p + 1, {";"}), ende);
        if (s1 == ende)
        {
            // for (x of xs), for (k in o)
            a->art = AnweisungArt::FuerJedes;
            a->text = funktionsText(m_p + 1, ende, ort);
            if (warten) a->kopf = "for await (" + a->text + ")";
        }
        else
        {
            a->init = funktionsText(m_p + 1, s1, ort);
            const std::size_t s2 = std::min(suche(s1 + 1, {";"}), ende);
            a->text = funktionsText(s1 + 1, s2, ort);
            if (s2 < ende) a->schritt = funktionsText(s2 + 1, ende, ort);
        }
        m_p = zu;
        a->kinder.push_back(liesRumpf(ort));
        return a;
    }

    AnweisungPtr liesAuswahl(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        AnweisungPtr a = neu(AnweisungArt::Auswahl, t);
        a->text = liesBedingung(ort);
        AnweisungPtr rumpf = neu(AnweisungArt::Block, akt());
        if (!nimm("{"))
        {
            melde(Schwere::Fehler, akt(), "'{' expected");
            a->kinder.push_back(std::move(rumpf));
            return a;
        }
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            const Token& s = akt();
            const std::size_t davor = m_p;
            if (s.istWort("case"))
            {
                ++m_p;
                const std::size_t e = suche(m_p, {":"});
                rumpf->kinder.push_back(neu(AnweisungArt::Fall, s, funktionsText(m_p, e, ort)));
                m_p = e;
                if (!nimm(":")) melde(Schwere::Fehler, akt(), "':' after case is missing");
            }
            else if (s.istWort("default"))
            {
                ++m_p;
                if (!nimm(":")) melde(Schwere::Fehler, akt(), "':' after default is missing");
                rumpf->kinder.push_back(neu(AnweisungArt::Fall, s));
            }
            else
                liesAnweisung(ort, rumpf->kinder);
            if (m_p == davor) ++m_p;
        }
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
        a->kinder.push_back(std::move(rumpf));
        return a;
    }

    AnweisungPtr liesVersuch(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        AnweisungPtr a = neu(AnweisungArt::Versuch, t);
        a->kinder.push_back(akt().istOp("{") ? liesBlock(ort) : liesRumpf(ort));
        if (akt().istWort("catch"))
        {
            const Token& bei = akt();
            ++m_p;
            AnweisungPtr fang = neu(AnweisungArt::Fang, bei);
            if (akt().istOp("("))
                fang->text = liesBedingung(ort);
            else
                fang->kopf = "catch";
            fang->kinder.push_back(akt().istOp("{") ? liesBlock(ort) : liesRumpf(ort));
            a->kinder.push_back(std::move(fang));
        }
        if (akt().istWort("finally"))
        {
            const Token& bei = akt();
            ++m_p;
            AnweisungPtr schluss = neu(AnweisungArt::Schluss, bei);
            schluss->kinder.push_back(akt().istOp("{") ? liesBlock(ort) : liesRumpf(ort));
            a->kinder.push_back(std::move(schluss));
        }
        if (a->kinder.size() == 1) melde(Schwere::Fehler, akt(), "'catch' or 'finally' after try is missing");
        return a;
    }

    /// Eine Anweisung ohne Steuerung: eine Deklaration (var, let, const) oder ein Ausdruck.
    void liesEinfache(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        const std::size_t e = anweisungsEnde(m_p);
        if (e == m_p)
        {
            if (!amEnde(m_p) && !akt().istOp("}"))
            {
                melde(Schwere::Fehler, t, "unexpected '" + std::string(t.text) + "'");
                ++m_p;
            }
            return;
        }
        const bool deklaration = t.istWort("var") || t.istWort("const") ||
                                 (istName(m_p, "let") && (tok(m_p + 1).art == TokenArt::Bezeichner ||
                                                          tok(m_p + 1).istOp("[") || tok(m_p + 1).istOp("{")));
        // eine Zeichenkette allein (`'use strict'`): eine Direktive, kein Ablauf
        if (t.art == TokenArt::Zeichenkette && e == m_p + 1)
        {
            m_p = e;
            nimmSemikolon();
            return;
        }
        if (ort.ebene == Ebene::Modul)
        {
            if (deklaration)
                liesVariablen(m_p + 1, e);
            else if (oben(m_p, e, "=") == e)
                m_ablauf = true;  // eine Zuweisung legt nur fest, sie laeuft nicht ab
        }
        std::string text = funktionsText(m_p, e, ort);
        aus.push_back(neu(AnweisungArt::Einfach, t, std::move(text)));
        m_p = e;
        nimmSemikolon();
    }

    /// Die Variablen einer Deklaration auf oberster Ebene: `a = 1, b: T = 2` (ohne Zerlegungen).
    void liesVariablen(std::size_t a, std::size_t b)
    {
        for (const Bereich& teil : teileSpitz(a, b))
        {
            if (tok(teil.first).art != TokenArt::Bezeichner) continue;
            Variable v;
            v.name = std::string(tok(teil.first).text);
            v.zeile = tok(teil.first).ort.zeile;
            if (m_ts && tok(teil.first + 1).istOp(":")) v.typ = textVon(teil.first + 2, typEnde(teil.first + 2));
            m_aus.einheit.variablen.push_back(std::move(v));
        }
    }

    /// `import … from 'm'`, `import 'm'`, `export … from 'm'`, `import x = require('m')`: die Zeichenkette.
    void einbindungAus(std::size_t a, std::size_t b)
    {
        for (std::size_t i = a; i < b; ++i)
        {
            const Token& t = tok(i);
            if (t.art != TokenArt::Zeichenkette || t.text.size() < 2) continue;
            const bool genannt = i == a + 1 || tok(i - 1).istWort("from") || istName(i - 1, "from") || tok(i - 1).istOp("(");
            if (!genannt) continue;
            m_aus.einheit.einbindungen.push_back({std::string(t.text.substr(1, t.text.size() - 2)), false, t.ort.zeile});
            return;
        }
    }

    void liesExport(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        ++m_p;
        if (istName(m_p, "type") && (tok(m_p + 1).istOp("{") || tok(m_p + 1).istOp("*"))) ++m_p;  // export type { X }
        if (akt().istOp("{") || akt().istOp("*"))
        {
            // export { a, b } [from 'm'];  export * from 'm'
            const std::size_t e = anweisungsEnde(m_p);
            einbindungAus(m_p, e);
            m_p = e;
            nimmSemikolon();
            return;
        }
        const bool standard = akt().istWort("default");
        if (standard) ++m_p;
        if (akt().istOp("=")) ++m_p;  // export = x (TypeScript)
        const bool zuvor = std::exchange(m_standard, standard);
        if (standard && !akt().istWort("function") && !akt().istWort("class") && !istName(m_p, "async") &&
            !istName(m_p, "abstract"))
        {
            // export default <Ausdruck>: legt fest, laeuft nicht ab
            const Token& t = akt();
            const std::size_t e = anweisungsEnde(m_p);
            aus.push_back(neu(AnweisungArt::Einfach, t, "export default " + funktionsText(m_p, e, ort, "default")));
            m_p = e;
            nimmSemikolon();
        }
        else
            liesAnweisung(ort, aus);
        m_standard = zuvor;
    }

    void liesDekorator()
    {
        ++m_p;
        while (tok(m_p).art == TokenArt::Bezeichner || tok(m_p).istOp(".")) ++m_p;
        if (akt().istOp("(") && !neueZeile(m_p)) m_p = ueberKlammer(m_p);
    }

    // --- Deklarationen ---------------------------------------------------------------------

    /// `function name<T>(…): R { … }` (auch async, function*); ohne Rumpf (Ueberladung, declare) ein Prototyp.
    void liesFunktionsDeklaration(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        if (istName(m_p, "async")) ++m_p;
        ++m_p;  // function
        nimm("*");
        Funktion f;
        f.zeile = t.ort.zeile;
        if (tok(m_p).art == TokenArt::Bezeichner)
            f.name = std::string(tok(m_p++).text);
        else
            f.name = m_standard ? "default" : anonymerName(t);
        m_p = spitzeEnde(m_p);
        if (akt().istOp("("))
        {
            const std::size_t zu = ueberKlammer(m_p);
            f.parameter = parameterIn(m_p, zu);
            m_p = zu;
        }
        else
            melde(Schwere::Fehler, akt(), "'(' expected");
        if (m_ts && akt().istOp(":")) m_p = typEnde(m_p + 1);
        if (!akt().istOp("{"))
        {
            nimmSemikolon();
            neueFunktion(std::move(f), ort);
            return;
        }
        const bool innen = ort.ebene == Ebene::Funktion;
        if (innen) aus.push_back(neu(AnweisungArt::Einfach, t, "function " + modell::signatur(f.name, f.parameter)));
        const std::size_t platz = neueFunktion(std::move(f), ort);
        if (innen) m_innere.push_back(m_aus.einheit.funktionen[platz].vollerName());
        m_p = liesRumpfVon(platz, m_p, ort);
    }

    /// `[abstract] class Name<T> extends B<T> implements I, J { … }`: ein Typ.
    void liesKlasse(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        if (istName(m_p, "abstract")) ++m_p;
        ++m_p;  // class
        Typ typ;
        typ.art = TypArt::Klasse;
        typ.zeile = t.ort.zeile;
        typ.bereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
        if (tok(m_p).art == TokenArt::Bezeichner && !istName(m_p, "implements"))
            typ.name = std::string(tok(m_p++).text);
        else
            typ.name = m_standard ? "default" : "<class:" + std::to_string(t.ort.zeile) + ">";
        m_p = spitzeEnde(m_p);
        const auto bisKopfEnde = [&] {
            std::size_t e = m_p;
            while (!amEnde(e) && !tok(e).istOp("{") && !istName(e, "implements"))
            {
                if (tok(e).istOp("<"))
                    e = std::max(spitzeEnde(e), e + 1);
                else if (tok(e).istOp("("))
                    e = nachKlammer(e);
                else
                    ++e;
            }
            return e;
        };
        if (akt().istWort("extends"))
        {
            ++m_p;
            const std::size_t e = bisKopfEnde();
            typ.basen.push_back(textVon(m_p, e));
            m_p = e;
        }
        if (istName(m_p, "implements"))
        {
            ++m_p;
            const std::size_t e = bisKopfEnde();
            for (const Bereich& s : teileSpitz(m_p, e)) typ.realisiert.push_back(textVon(s.first, s.second));
            m_p = e;
        }
        if (ort.ebene == Ebene::Funktion) aus.push_back(neu(AnweisungArt::Einfach, t, "class " + typ.name));
        Ort innen;
        innen.ebene = Ebene::Klasse;
        innen.bereich = typ.vollerName();
        innen.typ = m_aus.einheit.typen.size();
        innen.funktion = ort.funktion;
        m_aus.einheit.typen.push_back(std::move(typ));
        if (!akt().istOp("{"))
        {
            melde(Schwere::Fehler, akt(), "'{' expected");
            return;
        }
        ++m_p;
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            const std::size_t davor = m_p;
            liesGlied(innen);
            if (m_p == davor)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
            }
        }
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
    }

    /// Ein Glied einer Klasse: Feld, Methode, Konstruktor, get/set, Index-Signatur, statischer Block.
    void liesGlied(const Ort& ort)
    {
        if (nimm(";")) return;
        if (akt().istOp("@"))
        {
            liesDekorator();
            return;
        }
        Sichtbarkeit sicht = Sichtbarkeit::Keine;
        bool statisch = false;
        bool abstrakt = false;
        while (tok(m_p).art == TokenArt::Bezeichner && enthaelt(kModifikatoren, tok(m_p).text))
        {
            const Token& nach = tok(m_p + 1);
            if (nach.istOp("(") || nach.istOp("=") || nach.istOp(":") || nach.istOp(";") || nach.istOp("?") ||
                nach.istOp("!") || nach.istOp("<") || nach.istOp("}") || neueZeile(m_p + 1))
                break;  // das Wort ist hier der Name
            const std::string_view w = tok(m_p).text;
            if (w == "public") sicht = Sichtbarkeit::Oeffentlich;
            if (w == "protected") sicht = Sichtbarkeit::Geschuetzt;
            if (w == "private") sicht = Sichtbarkeit::Privat;
            statisch = statisch || w == "static";
            abstrakt = abstrakt || w == "abstract";
            ++m_p;
        }
        if (statisch && akt().istOp("{"))
        {
            melde(Schwere::Hinweis, akt(), "static block of a class is not shown");
            m_p = ueberKlammer(m_p);
            return;
        }
        std::string vorsatz;
        if ((istName(m_p, "get") || istName(m_p, "set")) && !tok(m_p + 1).istOp("(") && !tok(m_p + 1).istOp("=") &&
            !tok(m_p + 1).istOp(":") && !tok(m_p + 1).istOp(";") && !neueZeile(m_p + 1))
        {
            vorsatz = std::string(tok(m_p).text) + " ";
            ++m_p;
        }
        nimm("*");
        const Token& nameTok = akt();
        std::string name;
        if (akt().istOp("["))
        {
            // `[key: string]: T` (Index-Signatur) oder ein berechneter Name `[Symbol.iterator]`
            const std::size_t zu = ueberKlammer(m_p);
            if (m_ts && tok(m_p + 1).art == TokenArt::Bezeichner && tok(m_p + 2).istOp(":"))
            {
                m_p = anweisungsEnde(zu);
                nimmSemikolon();
                return;
            }
            name = textVon(m_p, zu);
            m_p = zu;
        }
        else if (akt().istOp("#") && tok(m_p + 1).art == TokenArt::Bezeichner)
        {
            name = "#" + std::string(tok(m_p + 1).text);
            sicht = Sichtbarkeit::Privat;
            m_p += 2;
        }
        else if (namenhaft(m_p) || akt().art == TokenArt::Zeichenkette || akt().art == TokenArt::Zahl)
            name = std::string(tok(m_p++).text);
        else
            return;  // der Aufrufer meldet es
        if (akt().istOp("?") || akt().istOp("!")) ++m_p;
        if (akt().istOp("<")) m_p = spitzeEnde(m_p);
        Typ& typ = m_aus.einheit.typen[ort.typ];
        if (akt().istOp("("))
        {
            const std::size_t auf = m_p;
            const std::size_t zu = ueberKlammer(m_p);
            Methode m;
            m.name = vorsatz + name;
            m.sicht = sicht;
            m.statisch = statisch;
            m.abstrakt = abstrakt;
            m.zeile = nameTok.ort.zeile;
            const std::size_t innen = tok(zu - 1).istOp(")") ? zu - 1 : zu;
            for (const Bereich& s : teileSpitz(auf + 1, innen))
            {
                std::string modifikator;
                Parameter p = parameter(s, &modifikator);
                // constructor(private x: T): ein Feld
                if (name == "constructor" && !modifikator.empty())
                {
                    Feld f;
                    f.name = p.name;
                    f.typ = p.typ;
                    f.typHinten = true;
                    f.zeile = tok(s.first).ort.zeile;
                    f.sicht = modifikator == "private" ? Sichtbarkeit::Privat
                              : modifikator == "protected" ? Sichtbarkeit::Geschuetzt
                              : modifikator == "public"    ? Sichtbarkeit::Oeffentlich
                                                           : Sichtbarkeit::Keine;
                    typ.felder.push_back(std::move(f));
                }
                m.parameter.push_back(std::move(p));
            }
            m_p = zu;
            if (m_ts && akt().istOp(":")) m_p = typEnde(m_p + 1);
            m_aus.einheit.typen[ort.typ].methoden.push_back(m);
            if (!akt().istOp("{"))
            {
                nimmSemikolon();
                return;
            }
            Funktion f;
            f.name = m.name;
            f.parameter = m.parameter;
            f.zeile = m.zeile;
            const std::size_t platz = neueFunktion(std::move(f), ort);
            m_p = liesRumpfVon(platz, m_p, ort);
            return;
        }
        // ein Feld: `x`, `x: T`, `x = 1`, `x: T = 1`
        Feld f;
        f.name = name;
        f.sicht = sicht;
        f.zeile = nameTok.ort.zeile;
        f.typHinten = true;
        if (m_ts && akt().istOp(":"))
        {
            const std::size_t e = typEnde(m_p + 1);
            f.typ = textVon(m_p + 1, e);
            m_p = e;
        }
        if (akt().istOp("="))
        {
            const std::size_t e = anweisungsEnde(m_p + 1);
            f.zusatz = "= " + funktionsText(m_p + 1, e, ort, name);
            m_p = e;
        }
        m_aus.einheit.typen[ort.typ].felder.push_back(std::move(f));
        nimmSemikolon();
    }

    /// TypeScript: interface, type, enum, namespace/module, declare. true = gelesen.
    bool liesTsDeklaration(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        if (!m_ts) return false;
        if (akt().istWort("const") && istName(m_p + 1, "enum") && tok(m_p + 2).art == TokenArt::Bezeichner)
        {
            ++m_p;  // const enum
            liesAufzaehlung(ort);
            return true;
        }
        if (akt().art != TokenArt::Bezeichner) return false;
        const Token& t = akt();
        const bool nameDanach = tok(m_p + 1).art == TokenArt::Bezeichner && !neueZeile(m_p + 1);
        if (t.text == "declare" && (nameDanach || tok(m_p + 1).art == TokenArt::Schluesselwort))
        {
            ++m_p;
            if (istName(m_p, "global") && tok(m_p + 1).istOp("{"))
            {
                ++m_p;
                liesNamensraumRumpf(ort, aus);
                return true;
            }
            if (istName(m_p, "module") && tok(m_p + 1).art == TokenArt::Zeichenkette)
            {
                m_p += 2;
                if (akt().istOp("{"))
                    liesNamensraumRumpf(ort, aus);
                else
                    nimmSemikolon();
                return true;
            }
            liesAnweisung(ort, aus);
            return true;
        }
        if (t.text == "interface" && nameDanach)
        {
            liesSchnittstelle(ort);
            return true;
        }
        if (t.text == "type" && nameDanach && (tok(m_p + 2).istOp("=") || tok(m_p + 2).istOp("<")))
        {
            liesTypAlias(ort);
            return true;
        }
        if (t.text == "enum" && nameDanach)
        {
            liesAufzaehlung(ort);
            return true;
        }
        if ((t.text == "namespace" || t.text == "module") && nameDanach)
        {
            ++m_p;
            std::string name;  // `a.b.c` wird `a::b::c`
            for (; tok(m_p).art == TokenArt::Bezeichner || tok(m_p).istOp("."); ++m_p)
                name += tok(m_p).istOp(".") ? std::string("::") : std::string(tok(m_p).text);
            Ort innen = ort;
            innen.bereich = vollerName(ort.bereich, name);
            if (akt().istOp("{")) liesNamensraumRumpf(innen, aus);
            return true;
        }
        return false;
    }

    /// m_p auf `{`: die Anweisungen eines Namensraums, wie auf oberster Ebene.
    void liesNamensraumRumpf(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        ++m_p;
        liesFolge(ort, aus, true);
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
    }

    void liesSchnittstelle(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;  // interface
        Typ typ;
        typ.art = TypArt::Schnittstelle;
        typ.name = std::string(tok(m_p++).text);
        typ.zeile = t.ort.zeile;
        typ.bereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
        m_p = spitzeEnde(m_p);
        if (akt().istWort("extends"))
        {
            ++m_p;
            std::size_t e = m_p;
            while (!amEnde(e) && !tok(e).istOp("{")) e = tok(e).istOp("<") ? std::max(spitzeEnde(e), e + 1) : e + 1;
            for (const Bereich& s : teileSpitz(m_p, e)) typ.basen.push_back(textVon(s.first, s.second));
            m_p = e;
        }
        const std::size_t platz = m_aus.einheit.typen.size();
        m_aus.einheit.typen.push_back(std::move(typ));
        if (akt().istOp("{"))
            liesObjektTyp(platz);
        else
            melde(Schwere::Fehler, akt(), "'{' expected");
    }

    /// m_p auf `{` eines Objekttyps: Felder (`x?: T`), Methoden (`m(a: T): R`); Signaturen ohne Namen uebergangen.
    void liesObjektTyp(std::size_t platz)
    {
        ++m_p;
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            const std::size_t davor = m_p;
            if (nimm(";") || nimm(",")) continue;
            if (istName(m_p, "readonly") && !tok(m_p + 1).istOp(":") && !tok(m_p + 1).istOp("?")) ++m_p;
            if (akt().istOp("[") || akt().istOp("(") || akt().istOp("<") || akt().istWort("new"))
            {
                // Index-Signatur, Aufruf- oder Konstruktor-Signatur
                m_p = gliedEnde(m_p);
                continue;
            }
            if (!namenhaft(m_p) && akt().art != TokenArt::Zeichenkette && akt().art != TokenArt::Zahl)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                m_p = gliedEnde(m_p);
                if (m_p == davor) ++m_p;
                continue;
            }
            const Token& nameTok = akt();
            std::string name(tok(m_p++).text);
            if (nimm("?")) name += "?";
            if (akt().istOp("<")) m_p = spitzeEnde(m_p);
            if (akt().istOp("("))
            {
                Methode m;
                m.name = name;
                const std::size_t zu = ueberKlammer(m_p);
                m.parameter = parameterIn(m_p, zu);
                m.zeile = nameTok.ort.zeile;
                m_p = zu;
                if (akt().istOp(":")) m_p = typEnde(m_p + 1);
                m_aus.einheit.typen[platz].methoden.push_back(std::move(m));
            }
            else if (akt().istOp(":"))
            {
                Feld f;
                f.name = name;
                const std::size_t e = typEnde(m_p + 1);
                f.typ = textVon(m_p + 1, e);
                f.zeile = nameTok.ort.zeile;
                f.typHinten = true;
                m_p = e;
                m_aus.einheit.typen[platz].felder.push_back(std::move(f));
            }
            if (m_p == davor) ++m_p;
        }
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
    }

    /// Das Ende eines Glieds eines Objekttyps: `;`, `,` oder `}` auf oberster Ebene, oder eine neue Zeile.
    std::size_t gliedEnde(std::size_t i)
    {
        std::size_t j = i;
        while (!amEnde(j))
        {
            if (j > i && neueZeile(j) && !fortsetzung(j)) return j;
            if (tok(j).istOp(";") || tok(j).istOp(",") || tok(j).istOp("}")) return j;
            if (oeffnet(tok(j)))
            {
                j = ueberKlammer(j);
                continue;
            }
            ++j;
        }
        return j;
    }

    /// `type X<T> = { … }` ist eine Struktur; jedes andere `type` ein Alias.
    void liesTypAlias(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;  // type
        const std::string name(tok(m_p++).text);
        m_p = spitzeEnde(m_p);
        if (!nimm("="))
        {
            melde(Schwere::Fehler, akt(), "'=' expected");
            return;
        }
        const std::size_t e = anweisungsEnde(m_p);
        if (akt().istOp("{") && nachKlammer(m_p) == e)
        {
            Typ typ;
            typ.art = TypArt::Struktur;
            typ.name = name;
            typ.zeile = t.ort.zeile;
            typ.bereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
            const std::size_t platz = m_aus.einheit.typen.size();
            m_aus.einheit.typen.push_back(std::move(typ));
            liesObjektTyp(platz);
        }
        else
        {
            m_aus.einheit.aliase.push_back({name, textVon(m_p, e), t.ort.zeile});
            m_p = e;
        }
        nimmSemikolon();
    }

    /// `[const] enum E { A, B = 2 }`: eine Aufzaehlung.
    void liesAufzaehlung(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;  // enum
        Typ typ;
        typ.art = TypArt::Aufzaehlung;
        typ.name = std::string(tok(m_p++).text);
        typ.zeile = t.ort.zeile;
        typ.bereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
        if (akt().istOp("{"))
        {
            const std::size_t zu = ueberKlammer(m_p);
            const std::size_t innen = tok(zu - 1).istOp("}") ? zu - 1 : zu;
            for (const Bereich& s : teile(m_p + 1, innen))
            {
                Feld f;
                const std::size_t gleich = oben(s.first, s.second, "=");
                f.name = textVon(s.first, gleich);
                if (gleich < s.second) f.zusatz = "= " + textVon(gleich + 1, s.second);
                f.zeile = tok(s.first).ort.zeile;
                typ.felder.push_back(std::move(f));
            }
            m_p = zu;
        }
        else
            melde(Schwere::Fehler, akt(), "'{' expected");
        m_aus.einheit.typen.push_back(std::move(typ));
    }

    bool m_ts;
    std::vector<bool> m_neueZeile;  ///< je Token: vor ihm beginnt eine neue Zeile (ausser in `(`/`[`)
    bool m_ablauf = false;          ///< auf oberster Ebene laeuft etwas ab
    bool m_standard = false;        ///< hinter `export default`
    std::vector<std::string> m_innere;  ///< die Funktionen, die in der laufenden Anweisung definiert wurden
    int m_letzteZeile = 0;          ///< die Zeile der `}` des zuletzt gelesenen Blocks
};

}  // namespace

const lexer::Sprachtabelle& tabelle()
{
    return kTabelle;
}

Ergebnis lies(std::string_view text, std::string_view einheitName, Dialekt dialekt)
{
    Ergebnis ergebnis;
    ergebnis.einheit.name = std::string(einheitName);
    EcmaLeser(text, dialekt, ergebnis).liesModul();
    ergaenzeLogik(ergebnis.einheit, kTabelle, kRegeln);
    return ergebnis;
}

}  // namespace c2d::sprache::ecma
