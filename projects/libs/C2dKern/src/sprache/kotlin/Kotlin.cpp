/**
 * @file Kotlin.cpp
 * @brief Kotlin: die Sprachtabelle und der Leser in rekursivem Abstieg (Entscheide K1–K6)
 *
 * Der Lexer liefert Zeilenenden (ausser in `(` und `[`); die Basis Leser nimmt sie heraus und
 * merkt je Token, ob vor ihm eine neue Zeile beginnt — eine Anweisung endet dort, wo die neue Zeile
 * sie nicht fortsetzen kann (eigene Tabelle der Fortsetzer, K1). Der Leser versteht die Gliederung:
 * Pakete, `import`, Klassen (`class`, `interface`, `object`, `enum class`, `data`, `sealed` …) mit
 * primaerem Konstruktor, Eigenschaften (auch mit `get()`/`set()`), Funktionen (auch Erweiterungen,
 * K4), `companion object`, `init`, `constructor`, `typealias`, und die Anweisungen der Ruempfe.
 * Ausdruecke bleiben Text — ausser Lambdas, die mehr als ein Ausdruck in einer Zeile sind (eigene
 * Funktionen, K2), anonymen Objekten (je Methode eine Funktion, wie H1) und `if`/`when`/`try` als
 * Wert auf oberster Ebene eines `return`, einer Zuweisung oder Deklaration (K3).
 */
#include "c2d/sprache/Sprache.hpp"

#include "../Leser.hpp"
#include "../Logik.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace c2d::sprache
{

namespace
{

using namespace c2d::modell;
using quelle::Schwere;

// Die harten Schluesselwoerter von Kotlin 2.0; die weichen (catch, finally, import, init,
// constructor, get, set, by, where …) und die Modifikatoren sind Namen und werden an ihrer Stelle erkannt.
constexpr std::string_view kSchluesselwoerter[] = {
    "as",  "break", "class", "continue", "do",     "else",  "false",     "for",    "fun",  "if",
    "in",  "interface", "is", "null",    "object", "package", "return",  "super",  "this", "throw",
    "true", "try",  "typealias", "typeof", "val",  "var",   "when",      "while",
};

constexpr std::string_view kOperatoren[] = {
    "..<", "===", "!==", "?:", "?.", "!!", "::", "->", "..", "++", "--", "&&", "||", "==", "!=", "<=",
    ">=",  "+=",  "-=",  "*=", "/=", "%=", "{",  "}",  "(",  ")",  "[",  "]",  ";",  ",",  ".",  ":",
    "?",   "!",   "+",   "-",  "*",  "/",  "%",  "<",  ">",  "=",  "@",  "&",  "|",
};

constexpr lexer::Sprachtabelle kTabelle{
    .schluesselwoerter = kSchluesselwoerter,
    .operatoren = kOperatoren,
    .zeilenKommentar = "//",
    .blockKommentarAuf = "/*",
    .blockKommentarZu = "*/",
    .ziffernTrenner = '_',
    .dreifachZeichenketten = true,  // """…""": roh, mit ${…}
    .zeilenEnde = true,
    .klammernOhneZeilenende = "([",
    .vorlagenBegrenzer = '"',  // "a $x ${f("b")}"
    .verschachtelteKommentare = true,
    .gravisNamen = true,
};

constexpr std::string_view kZuweisungen[] = {"=", "+=", "-=", "*=", "/=", "%="};

constexpr Ausdrucksregeln kRegeln{
    .wahl = Ausdrucksregeln::Wahl::Keine,  // `?:` ist Elvis; `if` als Wert verzweigt der Leser
    .nichtUmfasstVergleich = false,
    .zuweisungen = kZuweisungen,
    .zeiger = false,
};

/// Steht einer davon am Anfang der naechsten Zeile, geht die Anweisung weiter. (Nicht `+`/`-`: am
/// Zeilenanfang sind sie einstellig; nicht `is`/`in`: so beginnt ein Zweig eines `when`.)
constexpr std::string_view kFortsetzer[] = {".", "?.", "?:", "&&", "||", "::", ":", "=", "->"};

/// Am Ende einer Zeile schliessen diese einen Wert ab (die Anweisung kann dort enden).
constexpr std::string_view kSchliessend[] = {")", "]", "}", "++", "--", "!!", "?", ">", "*"};

/// Woerter vor einer Deklaration (weich: als Name gelesen, wo kein Name folgt).
constexpr std::string_view kModifikatoren[] = {
    "public",   "private",  "protected", "internal",    "open",     "final",   "abstract", "sealed",
    "override", "data",     "enum",      "annotation",  "inner",    "value",   "inline",   "noinline",
    "crossinline", "suspend", "operator", "infix",      "tailrec",  "external", "lateinit", "const",
    "vararg",   "expect",   "actual",    "companion",
};

template <std::size_t N>
bool enthaelt(const std::string_view (&liste)[N], std::string_view wort)
{
    return std::find(std::begin(liste), std::end(liste), wort) != std::end(liste);
}

class KtLeser final : public Leser
{
public:
    KtLeser(std::string_view text, Ergebnis& aus) : Leser(text, kTabelle, aus) { trenneZeilen(); }

    void liesDatei()
    {
        const Ort oben;
        std::vector<AnweisungPtr> code;  // bleibt leer: in `.kt` stehen keine Anweisungen ausserhalb
        while (!amEnde(m_p))
        {
            const std::size_t davor = m_p;
            liesGlied(oben, code);
            if (m_p == davor)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
            }
        }
    }

private:
    enum class Ebene
    {
        Datei,
        Typ,
        Funktion,
    };

    struct Ort
    {
        Ebene ebene = Ebene::Datei;
        std::string bereich;        ///< in einem Typ sein voller Name
        std::size_t typ = kKeiner;  ///< in einem Typ: sein Platz unter Einheit::typen
        std::string typName;        ///< sein Name ohne Bereich (Konstruktor)
        std::string funktion;       ///< der volle Name der Funktion, in der gelesen wird
        bool statisch = false;      ///< im Rumpf eines `companion object` (K4)
        /// Im Rumpf eines anonymen Objekts (oder einer enum-Konstante): ihre Methoden heissen
        /// `<vorsatz>.name` im Bereich `anonymBereich`, mit `anonymAussen` als aeusserer Funktion.
        std::string anonym;
        std::string anonymBereich;
        std::string anonymAussen;
    };

    /// Was vor einer Deklaration stand.
    struct Vorsatz
    {
        Sichtbarkeit sicht = Sichtbarkeit::Keine;
        bool abstrakt = false;
        bool aufzaehlung = false;  ///< enum class
        bool begleiter = false;    ///< companion object
    };

    // --- Token -------------------------------------------------------------------------

    [[nodiscard]] bool istName(std::size_t i, std::string_view name) const
    {
        return tok(i).art == TokenArt::Bezeichner && tok(i).text == name;
    }

    [[nodiscard]] bool namenhaft(std::size_t i) const
    {
        const TokenArt a = tok(i).art;
        return a == TokenArt::Bezeichner || a == TokenArt::Schluesselwort;
    }

    [[nodiscard]] int zeileVor(std::size_t i) const { return i > 0 ? tok(i - 1).ort.zeile : 0; }

    /// Setzt die Anweisung ueber den Zeilenwechsel vor j fort?
    [[nodiscard]] bool fortsetzung(std::size_t j) const
    {
        const Token& davor = tok(j - 1);
        const Token& t = tok(j);
        if (davor.istWort("return") || davor.istWort("break") || davor.istWort("continue") || davor.istWort("throw"))
            return davor.istWort("throw");  // `throw` verlangt einen Wert, `return` nicht
        if (davor.art == TokenArt::Operator && !enthaelt(kSchliessend, davor.text)) return true;
        if (davor.istWort("as") || davor.istWort("is") || davor.istWort("in")) return true;
        return t.art == TokenArt::Operator && enthaelt(kFortsetzer, t.text);
    }

    /// Ab i bis zum Ende der Anweisung; ein `else` auf oberster Ebene, zu dem kein `if` in der
    /// Anweisung gehoert, beendet sie (den Zweig eines `if`).
    std::size_t anweisungsEnde(std::size_t i)
    {
        int offeneIf = 0;
        return Leser::anweisungsEnde(
            i, [this](std::size_t j) { return fortsetzung(j); },
            [&](std::size_t j) {
                if (tok(j).istWort("if")) ++offeneIf;
                if (!tok(j).istWort("else")) return false;
                if (offeneIf == 0) return true;
                --offeneIf;
                return false;
            });
    }

    /// Endet hier eine Anweisung (fuer `if`/`when`/`try` als Wert: steht danach nichts mehr)?
    [[nodiscard]] bool endetHier(std::size_t i) const
    {
        return amEnde(i) || tok(i).istOp(";") || tok(i).istOp("}") || neueZeile(i);
    }

    /// Die erste Stelle in [a, b) mit diesem Operator auf oberster Ebene; b = keine.
    [[nodiscard]] std::size_t oben(std::size_t a, std::size_t b, std::string_view op) const
    {
        for (std::size_t i = a; i < b; ++i)
        {
            if (oeffnet(tok(i)))
            {
                i = nachKlammer(i) - 1;
                continue;
            }
            if (tok(i).istOp(op)) return i;
        }
        return b;
    }

    /// i auf `<` von Typ-Parametern oder -Argumenten: dahinter; sonst i.
    [[nodiscard]] std::size_t spitzeEnde(std::size_t i) const
    {
        if (!tok(i).istOp("<")) return i;
        int tiefe = 0;
        for (std::size_t j = i; !amEnde(j); ++j)
        {
            const Token& t = tok(j);
            if (t.istOp("(") || t.istOp("["))
            {
                j = nachKlammer(j) - 1;
                continue;
            }
            if (t.istOp("<")) ++tiefe;
            else if (t.istOp(">")) --tiefe;
            else if (t.istOp(";") || t.istOp("{") || t.istOp("}") || t.istOp("=") || (j > i && neueZeile(j)))
                return i;
            if (tiefe <= 0) return j + 1;
        }
        return i;
    }

    /// [a, b) an den Kommas auf oberster Ebene teilen, auch nicht in `<…>`.
    [[nodiscard]] std::vector<Bereich> teileSpitz(std::size_t a, std::size_t b) const
    {
        std::vector<Bereich> stuecke;
        std::size_t anfang = a;
        int spitze = 0;
        for (std::size_t i = a; i < b; ++i)
        {
            if (oeffnet(tok(i)))
            {
                i = nachKlammer(i) - 1;
                continue;
            }
            if (tok(i).istOp("<")) ++spitze;
            if (tok(i).istOp(">") && spitze > 0) --spitze;
            if (spitze == 0 && tok(i).istOp(","))
            {
                if (i > anfang) stuecke.emplace_back(anfang, i);
                anfang = i + 1;
            }
        }
        if (b > anfang) stuecke.emplace_back(anfang, b);
        return stuecke;
    }

    /// Annotationen ab i: `@Name`, `@a.b.Name(…)`, `@file:Name(…)`, `@get:Rule`, `@[A B]`. Dahinter.
    [[nodiscard]] std::size_t nachAnnotationen(std::size_t i) const
    {
        while (tok(i).istOp("@") && !tok(i + 1).abstandDavor)
        {
            ++i;
            if (tok(i).istOp("["))
            {
                i = nachKlammer(i);
                continue;
            }
            if (namenhaft(i) && tok(i + 1).istOp(":") && !tok(i + 1).abstandDavor) i += 2;  // Ziel: file, get …
            while (namenhaft(i) && tok(i + 1).istOp(".") && namenhaft(i + 2)) i += 2;
            if (namenhaft(i)) ++i;
            if (tok(i).istOp("(") && !tok(i).abstandDavor) i = nachKlammer(i);
        }
        return i;
    }

    void ueberAnnotationen() { m_p = nachAnnotationen(m_p); }

    /// m_p auf `where`: die Einschraenkungen der Typ-Parameter ueberlesen, bis `{`, `=` oder das Zeilenende.
    void ueberWhere()
    {
        if (!istName(m_p, "where")) return;
        ++m_p;
        while (!amEnde(m_p) && !akt().istOp("{") && !akt().istOp("=") && !(neueZeile(m_p) && !fortsetzung(m_p))) ++m_p;
    }

    /// Ein Typ ab i: `A.B<C>?`, `(A) -> B`, `suspend () -> Unit`, `@Composable () -> Unit`. Dahinter.
    [[nodiscard]] std::size_t typEnde(std::size_t i) const
    {
        std::size_t j = nachAnnotationen(i);
        if (istName(j, "suspend")) ++j;
        if (tok(j).istOp("("))
        {
            j = nachKlammer(j);
            while (tok(j).istOp("?")) ++j;
            if (tok(j).istOp("->")) return typEnde(j + 1);
            return j;
        }
        if (!namenhaft(j) || tok(j).istWort("in") || tok(j).istWort("is")) return i;
        ++j;
        for (;;)
        {
            j = spitzeEnde(j);
            if (tok(j).istOp(".") && namenhaft(j + 1))
            {
                j += 2;
                continue;
            }
            break;
        }
        while (tok(j).istOp("?")) ++j;
        // ein Funktionstyp mit Empfaenger: `A.() -> B`
        if (tok(j).istOp(".") && tok(j + 1).istOp("("))
        {
            j = nachKlammer(j + 1);
            if (tok(j).istOp("->")) return typEnde(j + 1);
        }
        return j;
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

    // --- Parameter und Funktionen --------------------------------------------------------

    /// Ein Parameter `[@A] [vararg|val|var|private …] name: Typ [= vorgabe]` — ohne Vorgabe.
    /// `feld`: davor stand val oder var (ein Feld des primaeren Konstruktors), `sicht` dessen.
    Parameter parameter(const Bereich& s, bool* feld = nullptr, Sichtbarkeit* sicht = nullptr) const
    {
        std::size_t a = std::min(nachAnnotationen(s.first), s.second);
        for (;;)
        {
            if (a < s.second && (tok(a).istWort("val") || tok(a).istWort("var")))
            {
                if (feld != nullptr) *feld = true;
                ++a;
                continue;
            }
            if (a + 1 < s.second && tok(a).art == TokenArt::Bezeichner && enthaelt(kModifikatoren, tok(a).text) &&
                namenhaft(a + 1))
            {
                if (sicht != nullptr)
                {
                    if (tok(a).text == "private") *sicht = Sichtbarkeit::Privat;
                    if (tok(a).text == "protected") *sicht = Sichtbarkeit::Geschuetzt;
                    if (tok(a).text == "public") *sicht = Sichtbarkeit::Oeffentlich;
                }
                a = std::min(nachAnnotationen(a + 1), s.second);
                continue;
            }
            break;
        }
        Parameter p;
        p.typHinten = true;
        const std::size_t gleich = oben(a, s.second, "=");
        const std::size_t doppelpunkt = oben(a, gleich, ":");
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

    std::size_t neueFunktion(Funktion f, const Ort& ort)
    {
        if (ort.ebene == Ebene::Funktion)
        {
            f.aussen = ort.funktion;
            f.bereich = ort.funktion;
        }
        else if (!ort.anonym.empty())  // die Methode eines anonymen Objekts (wie H1)
        {
            f.name = ort.anonym + "." + f.name;
            f.bereich = ort.anonymBereich;
            f.aussen = ort.anonymAussen;
        }
        else
            f.bereich = ort.bereich;
        m_aus.einheit.funktionen.push_back(std::move(f));
        return m_aus.einheit.funktionen.size() - 1;
    }

    Ort innenVon(std::size_t platz, const Ort& ort) const
    {
        Ort innen;
        innen.ebene = Ebene::Funktion;
        innen.bereich = ort.bereich;
        innen.typ = ort.typ;
        innen.typName = ort.typName;
        innen.funktion = m_aus.einheit.funktionen[platz].vollerName();
        return innen;
    }

    /// Der Rumpf ab m_p: `{ … }`, `= ausdruck` oder nichts. `liefert`: ein `= ausdruck` ist ein
    /// return (sonst, bei `: Unit`, eine Anweisung); `if`/`when`/`try` darin verzweigen (K3).
    void liesRumpfVon(std::size_t platz, const Ort& ort, bool liefert)
    {
        const Ort innen = innenVon(platz, ort);
        const Token& bei = akt();
        AnweisungPtr r;
        if (akt().istOp("{"))
            r = liesBlock(innen);
        else if (akt().istOp("="))
        {
            ++m_p;
            std::vector<AnweisungPtr> kinder;
            liesWertOderText(innen, liefert ? "return" : "", kinder);
            m_letzteZeile = zeileVor(m_p);
            r = block(std::move(kinder), bei);
        }
        else
            return;  // ohne Rumpf (abstrakt, in einer Schnittstelle, extern)
        m_aus.einheit.funktionen[platz].rumpf = std::move(r);
        m_aus.einheit.funktionen[platz].bisZeile = m_letzteZeile;
    }

    /// m_p auf `fun`: eine Funktion `fun [<T>] [Empfaenger.]name(…)[: R] [where …] { … } | = …`.
    void liesFunktion(const Ort& ort, std::vector<AnweisungPtr>& code, const Vorsatz& vorsatz)
    {
        const Token& t = akt();
        ++m_p;  // fun
        if (akt().istOp("<")) m_p = spitzeEnde(m_p);
        // der Name steht vor der `(`; davor, mit `.`, der Empfaenger einer Erweiterung (K4)
        const std::size_t anfang = m_p;
        std::size_t klammer = m_p;
        while (!amEnde(klammer) && !tok(klammer).istOp("(") && !tok(klammer).istOp("{") && !tok(klammer).istOp("="))
            klammer = tok(klammer).istOp("<") ? std::max(spitzeEnde(klammer), klammer + 1) : klammer + 1;
        if (!tok(klammer).istOp("(") || klammer == anfang)
        {
            melde(Schwere::Fehler, akt(), "'(' of the function is missing");
            m_p = anweisungsEnde(m_p);
            return;
        }
        Funktion f;
        f.zeile = t.ort.zeile;
        const std::string name(tok(klammer - 1).text);
        f.name = klammer >= anfang + 2 && tok(klammer - 2).istOp(".") ? textVon(anfang, klammer - 2) + "." + name : name;
        const std::size_t zu = ueberKlammer(klammer);
        f.parameter = parameterIn(klammer, zu);
        m_p = zu;
        std::string rueckgabe;
        if (akt().istOp(":"))
        {
            const std::size_t e = typEnde(m_p + 1);
            rueckgabe = textVon(m_p + 1, e);
            m_p = e;
        }
        ueberWhere();
        const bool rumpf = akt().istOp("{") || akt().istOp("=");
        if (ort.ebene == Ebene::Typ)
        {
            Methode m;
            m.name = f.name;
            m.parameter = f.parameter;
            m.sicht = vorsatz.sicht;
            m.statisch = ort.statisch;
            m.abstrakt = !rumpf && (vorsatz.abstrakt || m_aus.einheit.typen[ort.typ].art == TypArt::Schnittstelle);
            m.zeile = f.zeile;
            m_aus.einheit.typen[ort.typ].methoden.push_back(std::move(m));
        }
        if (ort.ebene == Ebene::Funktion)
            code.push_back(neu(AnweisungArt::Einfach, t, "fun " + modell::signatur(f.name, f.parameter)));
        const std::size_t platz = neueFunktion(std::move(f), ort);
        if (ort.ebene == Ebene::Funktion) code.back()->innere.push_back(m_aus.einheit.funktionen[platz].vollerName());
        if (rumpf) liesRumpfVon(platz, ort, rueckgabe != "Unit");
    }

    /// Der Text von [a, b) — ein Lambda, das mehr ist als ein Ausdruck in einer Zeile, wird eine
    /// eigene Funktion (K2), ein anonymes Objekt je Methode eine (wie H1); im Text steht ihr Rumpf
    /// als `{…}`. Ein Block (hinter `if (…)`, `else`, `try`, `when`, `->` …) bleibt Text.
    std::string funktionsText(std::size_t a, std::size_t b, const Ort& ort)
    {
        std::vector<Bereich> ruempfe;
        std::vector<std::string> innere;
        std::size_t j = a;
        while (j < b && !amEnde(j))
        {
            const Token& t = tok(j);
            if (t.istWort("object") && (tok(j + 1).istOp(":") || tok(j + 1).istOp("{")))
            {
                std::size_t k = j + 1;
                while (!amEnde(k) && k < b && !tok(k).istOp("{"))
                    k = tok(k).istOp("(") ? nachKlammer(k) : tok(k).istOp("<") ? std::max(spitzeEnde(k), k + 1) : k + 1;
                if (tok(k).istOp("{"))
                {
                    const std::size_t danach = liesAnonymesObjekt(k, anonymerName(t, ort), ort, innere);
                    ruempfe.emplace_back(k, danach);
                    j = danach;
                    continue;
                }
            }
            if (!t.istOp("{") || istBlock(j, a))
            {
                ++j;
                continue;
            }
            const std::size_t zu = nachKlammer(j);
            if (einzeilig(j, zu))
            {
                j = zu;  // ein Ausdruck in einer Zeile: bleibt Text (K2)
                continue;
            }
            const std::size_t danach = liesLambda(j, zu, a, ort, innere);
            ruempfe.emplace_back(j, danach);
            j = danach;
        }
        m_innere.insert(m_innere.end(), innere.begin(), innere.end());
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

    /// Ist die `{` bei j ein Block (kein Lambda): hinter `if (…)`, `while (…)`, `for (…)`, `when (…)`,
    /// `catch (…)`, hinter `else`, `try`, `finally`, `do`, `when` oder dem `->` eines Zweigs?
    [[nodiscard]] bool istBlock(std::size_t j, std::size_t a) const
    {
        if (j <= a) return false;
        const Token& davor = tok(j - 1);
        if (davor.istWort("else") || davor.istWort("try") || davor.istWort("do") || davor.istWort("when") ||
            istName(j - 1, "finally") || davor.istOp("->"))
            return true;
        if (!davor.istOp(")")) return false;
        int tiefe = 0;
        std::size_t o = j - 1;
        for (;; --o)
        {
            if (schliesst(tok(o))) ++tiefe;
            else if (oeffnet(tok(o)) && --tiefe == 0) break;
            if (o == a) return false;
        }
        if (o == 0) return false;
        const Token& vor = tok(o - 1);
        return vor.istWort("if") || vor.istWort("while") || vor.istWort("for") || vor.istWort("when") ||
               istName(o - 1, "catch");
    }

    /// Ist das Lambda [auf, zu) ein Ausdruck in einer Zeile (bleibt Text, K2)?
    [[nodiscard]] bool einzeilig(std::size_t auf, std::size_t zu) const
    {
        if (zu < auf + 2 || tok(zu - 1).ort.zeile != tok(auf).ort.zeile) return false;
        return oben(auf + 1, zu - 1, ";") == zu - 1;
    }

    /// Das Lambda [auf, zu) ab a im Ausdruck: eine Funktion `<lambda:Zeile>` — heisst sie nach der
    /// Deklaration `val NAME = {`, so; die Parameter `{ a, b: T -> …`. Liefert zu.
    std::size_t liesLambda(std::size_t auf, std::size_t zu, std::size_t a, const Ort& ort,
                           std::vector<std::string>& innere)
    {
        std::string name;
        if (auf >= a + 3 && tok(auf - 1).istOp("=") && (tok(a).istWort("val") || tok(a).istWort("var")) &&
            tok(a + 1).art == TokenArt::Bezeichner && oben(a, auf, "=") == auf - 1)
            name = std::string(tok(a + 1).text);
        if (name.empty())
        {
            const std::string bereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
            name = "<lambda:" + std::to_string(tok(auf).ort.zeile) + ">";
            if (!m_anonyme.insert(vollerName(bereich, name)).second)
            {
                name = "<lambda:" + std::to_string(tok(auf).ort.zeile) + ":" + std::to_string(tok(auf).ort.spalte) + ">";
                m_anonyme.insert(vollerName(bereich, name));
            }
        }
        Funktion f;
        f.name = name;
        f.zeile = tok(auf).ort.zeile;
        // die Parameter: Namen (mit Typ) bis `->` in der ersten Zeile
        std::size_t koerper = auf + 1;
        std::size_t k = auf + 1;
        while (k + 1 < zu && tok(k).ort.zeile == tok(auf).ort.zeile &&
               (tok(k).art == TokenArt::Bezeichner || tok(k).istOp(",") || tok(k).istOp(":") || tok(k).istOp("(") ||
                tok(k).istOp(")") || tok(k).istOp("<") || tok(k).istOp(">") || tok(k).istOp("?") || tok(k).istOp(".")))
            ++k;
        if (k + 1 < zu && tok(k).istOp("->"))
        {
            for (const Bereich& s : teileSpitz(auf + 1, k))
            {
                Parameter p = parameter(s);
                f.parameter.push_back(std::move(p));
            }
            koerper = k + 1;
        }
        const std::size_t platz = neueFunktion(std::move(f), ort);
        innere.push_back(m_aus.einheit.funktionen[platz].vollerName());
        const std::size_t gemerkt = m_p;
        std::vector<std::string> gemerktInnere = std::exchange(m_innere, {});
        m_p = koerper;
        std::vector<AnweisungPtr> kinder;
        liesFolge(innenVon(platz, ort), kinder);
        m_letzteZeile = akt().ort.zeile;
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' of the lambda is missing");
        m_aus.einheit.funktionen[platz].rumpf = block(std::move(kinder), tok(auf));
        m_aus.einheit.funktionen[platz].bisZeile = m_letzteZeile;
        m_p = gemerkt;
        m_innere = std::move(gemerktInnere);
        return zu;
    }

    std::string anonymerName(const Token& bei, const Ort& ort)
    {
        const std::string bereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
        std::string name = "<anonymous:" + std::to_string(bei.ort.zeile) + ">";
        if (!m_anonyme.insert(vollerName(bereich, name)).second)
        {
            name = "<anonymous:" + std::to_string(bei.ort.zeile) + ":" + std::to_string(bei.ort.spalte) + ">";
            m_anonyme.insert(vollerName(bereich, name));
        }
        return name;
    }

    /// Der Rumpf eines anonymen Objekts (oder einer enum-Konstante) ab `auf` (die `{`): seine
    /// Methoden werden Funktionen `<vorsatz>.name`, ihre vollen Namen kommen nach `innere`; seine
    /// Felder und es selbst kommen in kein Diagramm. Liefert die Stelle hinter `}`; m_p bleibt.
    std::size_t liesAnonymesObjekt(std::size_t auf, const std::string& vorsatz, const Ort& ort,
                                   std::vector<std::string>& innere)
    {
        Ort innen;
        innen.ebene = Ebene::Typ;
        innen.typ = m_aus.einheit.typen.size();
        innen.funktion = ort.funktion;
        innen.anonym = vorsatz;
        innen.anonymBereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
        innen.anonymAussen = ort.ebene == Ebene::Funktion ? ort.funktion : std::string();
        innen.bereich = vollerName(innen.anonymBereich, vorsatz);
        Typ huelle;  // nur zum Lesen: Felder und Methoden kommen in einen Typ
        huelle.name = vorsatz;
        m_aus.einheit.typen.push_back(std::move(huelle));
        const std::size_t vorher = m_aus.einheit.funktionen.size();
        const std::size_t gemerkt = m_p;
        std::vector<std::string> gemerktInnere = std::exchange(m_innere, {});
        m_p = auf + 1;
        liesKoerper(innen);
        const std::size_t danach = m_p;
        m_p = gemerkt;
        m_innere = std::move(gemerktInnere);
        m_aus.einheit.typen.erase(m_aus.einheit.typen.begin() + static_cast<std::ptrdiff_t>(innen.typ));
        const std::string vorn = vorsatz + ".";
        for (std::size_t i = vorher; i < m_aus.einheit.funktionen.size(); ++i)
        {
            const Funktion& f = m_aus.einheit.funktionen[i];
            if (f.bereich == innen.anonymBereich && f.name.starts_with(vorn)) innere.push_back(f.vollerName());
        }
        return danach;
    }

    // --- Glieder: Datei und Typen ------------------------------------------------------------

    /// Ab m_p (hinter der `{`): die Glieder bis zur `}` (genommen).
    void liesKoerper(const Ort& ort)
    {
        std::vector<AnweisungPtr> code;
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            const std::size_t davor = m_p;
            liesGlied(ort, code);
            m_innere.clear();
            if (m_p == davor)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
            }
        }
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
    }

    /// Annotationen und Modifikatoren ab m_p; ein Modifikator ist ein Name, auf den ein Name folgt.
    Vorsatz liesVorsatz()
    {
        Vorsatz v;
        for (;;)
        {
            ueberAnnotationen();
            if (tok(m_p).art == TokenArt::Bezeichner && enthaelt(kModifikatoren, tok(m_p).text) &&
                namenhaft(m_p + 1) && !neueZeile(m_p + 1))
            {
                const std::string_view w = tok(m_p).text;
                if (w == "public") v.sicht = Sichtbarkeit::Oeffentlich;
                if (w == "protected") v.sicht = Sichtbarkeit::Geschuetzt;
                if (w == "private") v.sicht = Sichtbarkeit::Privat;
                v.abstrakt = v.abstrakt || w == "abstract";
                v.aufzaehlung = v.aufzaehlung || w == "enum";
                v.begleiter = v.begleiter || w == "companion";
                ++m_p;
                continue;
            }
            return v;
        }
    }

    /// Ein Glied der Datei oder eines Typs: package, import, fun, class/interface/object,
    /// typealias, val/var, init, constructor.
    void liesGlied(const Ort& ort, std::vector<AnweisungPtr>& code)
    {
        if (nimm(";")) return;
        if (ort.ebene == Ebene::Datei && akt().istOp("@")) ueberAnnotationen();  // @file:JvmName(…)
        if (ort.ebene == Ebene::Datei && (akt().istWort("package") || istName(m_p, "import")))
        {
            // package: ueberlesen (die Namen blieben sonst lang); import: eine Einbindung
            const bool einbinden = istName(m_p, "import");
            ++m_p;
            const std::size_t e = anweisungsEnde(m_p);
            if (einbinden) m_aus.einheit.einbindungen.push_back({textVon(m_p, e), false, tok(m_p).ort.zeile});
            m_p = e;
            nimm(";");
            return;
        }
        const Vorsatz vorsatz = liesVorsatz();
        const Token& t = akt();
        if (t.istWort("fun") && tok(m_p + 1).istWort("interface"))
        {
            ++m_p;  // fun interface: eine Schnittstelle
            liesKlasse(ort, code, vorsatz);
            return;
        }
        if (t.istWort("fun"))
        {
            liesFunktion(ort, code, vorsatz);
            return;
        }
        if (t.istWort("class") || t.istWort("interface") || t.istWort("object"))
        {
            liesKlasse(ort, code, vorsatz);
            return;
        }
        if (t.istWort("typealias"))
        {
            ++m_p;
            const std::string name(akt().text);
            ++m_p;
            m_p = spitzeEnde(m_p);
            const std::size_t e = anweisungsEnde(m_p);
            m_aus.einheit.aliase.push_back({name, textVon(akt().istOp("=") ? m_p + 1 : m_p, e), t.ort.zeile});
            m_p = e;
            nimm(";");
            return;
        }
        if (t.istWort("val") || t.istWort("var"))
        {
            liesEigenschaft(ort, vorsatz);
            return;
        }
        if (ort.ebene == Ebene::Typ && istName(m_p, "init") && tok(m_p + 1).istOp("{"))
        {
            ++m_p;
            Funktion f;  // init { … }: wie ein Initialisierungsblock in Java (festgelegt)
            f.name = "<init:" + std::to_string(t.ort.zeile) + ">";
            f.zeile = t.ort.zeile;
            const std::size_t platz = neueFunktion(std::move(f), ort);
            liesRumpfVon(platz, ort, false);
            return;
        }
        if (ort.ebene == Ebene::Typ && istName(m_p, "constructor") && tok(m_p + 1).istOp("("))
        {
            ++m_p;
            liesKonstruktor(ort, vorsatz, t);
            return;
        }
        // Anweisungen stehen in `.kt` nur in Funktionen (ein Skript `.kts` liest Code2Diagram noch nicht)
        melde(Schwere::Fehler, t, "unexpected '" + std::string(t.text) + "' outside a function");
        const std::size_t e = anweisungsEnde(m_p);
        m_p = std::max(e, m_p + 1);
    }

    /// m_p auf `(` eines zweiten Konstruktors: `constructor(…) [: this(…)] [{ … }]`.
    void liesKonstruktor(const Ort& ort, const Vorsatz& vorsatz, const Token& bei)
    {
        Methode m;
        m.name = ort.typName;
        m.sicht = vorsatz.sicht;
        m.zeile = bei.ort.zeile;
        const std::size_t zu = ueberKlammer(m_p);
        m.parameter = parameterIn(m_p, zu);
        m_p = zu;
        if (akt().istOp(":"))
        {
            ++m_p;  // : this(…) / : super(…)
            if (namenhaft(m_p)) ++m_p;
            if (akt().istOp("(")) m_p = ueberKlammer(m_p);
        }
        m_aus.einheit.typen[ort.typ].methoden.push_back(m);
        if (!akt().istOp("{")) return;
        Funktion f;
        f.name = m.name;
        f.parameter = m.parameter;
        f.zeile = m.zeile;
        const std::size_t platz = neueFunktion(std::move(f), ort);
        liesRumpfVon(platz, ort, false);
    }

    /// m_p auf class, interface oder object: ein Typ mit primaerem Konstruktor, Basen und Gliedern.
    void liesKlasse(const Ort& ort, std::vector<AnweisungPtr>& code, const Vorsatz& vorsatz)
    {
        const Token& t = akt();
        const bool schnittstelle = t.istWort("interface");
        const bool objekt = t.istWort("object");
        ++m_p;
        Typ typ;
        typ.zeile = t.ort.zeile;
        typ.art = schnittstelle ? TypArt::Schnittstelle : vorsatz.aufzaehlung ? TypArt::Aufzaehlung : TypArt::Klasse;
        typ.bereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
        if (akt().art == TokenArt::Bezeichner)
            typ.name = std::string(tok(m_p++).text);
        else if (objekt && vorsatz.begleiter)
            typ.name = "Companion";
        else
        {
            melde(Schwere::Fehler, akt(), "name of the type expected");
            return;
        }
        m_p = spitzeEnde(m_p);
        // der primaere Konstruktor: [Annotationen] [Modifikatoren] [constructor] (…)
        std::vector<Parameter> primaer;
        bool mitPrimaer = false;
        {
            std::size_t k = m_p;
            for (;;)
            {
                k = nachAnnotationen(k);
                if (tok(k).art == TokenArt::Bezeichner && enthaelt(kModifikatoren, tok(k).text) && !neueZeile(k)) ++k;
                else break;
            }
            if (istName(k, "constructor") && !neueZeile(k)) ++k;
            if (tok(k).istOp("(") && !neueZeile(k))
            {
                const std::size_t zu = ueberKlammer(k);
                const std::size_t innen = tok(zu - 1).istOp(")") ? zu - 1 : zu;
                for (const Bereich& s : teileSpitz(k + 1, innen))
                {
                    bool feld = false;
                    Sichtbarkeit sicht = Sichtbarkeit::Keine;
                    Parameter p = parameter(s, &feld, &sicht);
                    if (feld)  // class P(val x: Int): ein Feld
                    {
                        Feld f;
                        f.name = p.name;
                        f.typ = p.typ;
                        f.typHinten = true;
                        f.sicht = sicht;
                        f.zeile = tok(s.first).ort.zeile;
                        typ.felder.push_back(std::move(f));
                    }
                    primaer.push_back(std::move(p));
                }
                mitPrimaer = true;
                m_p = zu;
            }
        }
        // : Basis(…), Schnittstelle, Delegat by x — mit Klammern eine Basis, ohne eine Realisierung
        if (akt().istOp(":"))
        {
            ++m_p;
            std::size_t e = m_p;
            while (!amEnde(e) && !tok(e).istOp("{") && !istName(e, "where") && !(e > m_p && endetHier(e) && !fortsetzung(e)))
                e = tok(e).istOp("(") ? nachKlammer(e) : tok(e).istOp("<") ? std::max(spitzeEnde(e), e + 1) : e + 1;
            for (const Bereich& s : teileSpitz(m_p, e))
            {
                std::size_t bis = s.second;
                bool aufruf = false;
                for (std::size_t i = s.first; i < bis; ++i)
                {
                    if (tok(i).istOp("<")) i = std::max(spitzeEnde(i), i + 1) - 1;
                    else if (tok(i).istOp("("))
                    {
                        aufruf = true;
                        bis = i;
                    }
                    else if (istName(i, "by"))
                        bis = i;
                }
                std::string name = textVon(s.first, bis);
                if (schnittstelle || !aufruf)
                    (schnittstelle ? typ.basen : typ.realisiert).push_back(std::move(name));
                else
                    typ.basen.push_back(std::move(name));
            }
            m_p = e;
        }
        ueberWhere();
        if (mitPrimaer)
        {
            Methode m;
            m.name = typ.name;
            m.parameter = primaer;
            m.zeile = typ.zeile;
            typ.methoden.push_back(std::move(m));
        }
        // ein companion object gibt seine Glieder dem Typ darum, statisch (K4)
        if (objekt && vorsatz.begleiter && ort.ebene == Ebene::Typ)
        {
            if (!akt().istOp("{")) return;
            ++m_p;
            Ort begleiter = ort;
            begleiter.statisch = true;
            liesKoerper(begleiter);
            return;
        }
        if (ort.ebene == Ebene::Funktion)
            code.push_back(neu(AnweisungArt::Einfach, t, std::string(t.text) + " " + typ.name));
        const bool aufzaehlung = typ.art == TypArt::Aufzaehlung;
        Ort innen;
        innen.ebene = Ebene::Typ;
        innen.bereich = typ.vollerName();
        innen.typ = m_aus.einheit.typen.size();
        innen.typName = typ.name;
        innen.funktion = ort.funktion;
        m_aus.einheit.typen.push_back(std::move(typ));
        if (!akt().istOp("{")) return;
        ++m_p;
        if (aufzaehlung)
        {
            liesEintraege(innen);
            return;
        }
        liesKoerper(innen);
    }

    /// Die Konstanten eines enum ab m_p (hinter der `{`) bis `;` oder `}` — `ROT`, `GRUEN(2)`,
    /// `BLAU { … }` (ein Rumpf wie ein anonymes Objekt: Methoden `BLAU.name`); dahinter die Glieder.
    void liesEintraege(const Ort& innen)
    {
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            ueberAnnotationen();
            if (nimm(";"))
            {
                liesKoerper(innen);
                return;
            }
            if (nimm(",")) continue;
            if (akt().art != TokenArt::Bezeichner)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
                continue;
            }
            Feld f;
            f.name = std::string(akt().text);
            f.zeile = akt().ort.zeile;
            ++m_p;
            if (akt().istOp("("))
            {
                const std::size_t zu = ueberKlammer(m_p);
                f.zusatz = funktionsText(m_p, zu, innen);  // die Argumente als Wert: `(5000)`
                m_p = zu;
            }
            if (akt().istOp("{"))
            {
                std::vector<std::string> methoden;
                m_p = liesAnonymesObjekt(m_p, f.name, innen, methoden);
            }
            m_aus.einheit.typen[innen.typ].felder.push_back(std::move(f));
        }
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
    }

    /// m_p auf val/var in einem Typ oder der Datei: ein Feld (bzw. eine Variable), `= wert` oder
    /// `by delegat`; folgen `get()`/`set(…)` mit Rumpf, werden sie Funktionen `name.get`/`name.set`.
    void liesEigenschaft(const Ort& ort, const Vorsatz& vorsatz)
    {
        const Token& t = akt();
        ++m_p;  // val, var
        if (akt().istOp("<")) m_p = spitzeEnde(m_p);
        // [Empfaenger.]name — `String.istLeer`, `List<T>.zweites`, `String?.x`
        const std::size_t anfang = m_p;
        std::size_t nach = m_p;
        if (namenhaft(nach)) ++nach;
        for (;;)
        {
            if (tok(nach).istOp("<")) nach = std::max(spitzeEnde(nach), nach + 1);
            if (tok(nach).istOp("?") && tok(nach + 1).istOp(".")) ++nach;
            if (tok(nach).istOp(".") && namenhaft(nach + 1))
            {
                nach += 2;
                continue;
            }
            break;
        }
        const std::string name = textVon(anfang, nach);
        m_p = nach;
        std::string typ;
        if (akt().istOp(":"))
        {
            const std::size_t e = typEnde(m_p + 1);
            typ = textVon(m_p + 1, e);
            m_p = e;
        }
        std::string zusatz;
        if (akt().istOp("=") || istName(m_p, "by"))
        {
            const bool delegat = istName(m_p, "by");
            const std::size_t e = anweisungsEnde(m_p + 1);
            zusatz = std::string(delegat ? "by " : "= ") + funktionsText(m_p + 1, e, ort);
            m_p = e;
        }
        nimm(";");
        // Accessoren in den folgenden Zeilen: [Modifikatoren] get()/set(…) [= …|{ … }]
        std::vector<std::string> zugriffe;
        for (;;)
        {
            std::size_t k = nachAnnotationen(m_p);
            while (tok(k).art == TokenArt::Bezeichner && enthaelt(kModifikatoren, tok(k).text)) ++k;
            if (!(istName(k, "get") || istName(k, "set")) ||
                !(tok(k + 1).istOp("(") || tok(k + 1).istOp("=") || endetHier(k + 1)))
                break;
            const Token& zugriff = tok(k);
            zugriffe.emplace_back(zugriff.text);
            m_p = k + 1;
            if (!akt().istOp("(")) continue;  // private set
            const std::size_t zu = ueberKlammer(m_p);
            Funktion f;
            f.name = name + "." + std::string(zugriff.text);
            f.parameter = parameterIn(m_p, zu);
            f.zeile = zugriff.ort.zeile;
            m_p = zu;
            if (akt().istOp(":")) m_p = typEnde(m_p + 1);
            if (!akt().istOp("{") && !akt().istOp("=")) continue;
            const std::size_t platz = neueFunktion(std::move(f), ort);
            liesRumpfVon(platz, ort, zugriff.text == "get");
        }
        if (!zugriffe.empty())
        {
            std::string z = "{";
            for (const std::string& w : zugriffe) z += " " + w + ";";
            z += " }";
            zusatz = zusatz.empty() ? z : z + " " + zusatz;
        }
        if (ort.ebene == Ebene::Typ)
        {
            Feld f;
            f.name = name;
            f.typ = typ;
            f.typHinten = true;
            f.zusatz = zusatz;
            f.sicht = vorsatz.sicht;
            f.zeile = t.ort.zeile;
            m_aus.einheit.typen[ort.typ].felder.push_back(std::move(f));
        }
        else
        {
            Variable v;  // wie in JavaScript und Python: ohne Anfangswert (zusatz ist in C `[8]`)
            v.name = name;
            v.typ = typ;
            v.zeile = t.ort.zeile;
            m_aus.einheit.variablen.push_back(std::move(v));
        }
    }

    // --- Anweisungen ---------------------------------------------------------------------------

    void liesFolge(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            const std::size_t davor = m_p;
            const std::size_t vorher = aus.size();
            liesAnweisung(ort, aus);
            if (aus.size() > vorher)
                aus[vorher]->innere.insert(aus[vorher]->innere.end(), m_innere.begin(), m_innere.end());
            m_innere.clear();
            if (m_p == davor)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
            }
        }
    }

    AnweisungPtr liesBlock(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        std::vector<AnweisungPtr> kinder;
        liesFolge(ort, kinder);
        m_letzteZeile = akt().ort.zeile;
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
        return block(std::move(kinder), t);
    }

    /// Der Rumpf von if, else, while, for, eines Zweigs: ein Block oder eine Anweisung; nie leer.
    AnweisungPtr liesRumpf(const Ort& ort)
    {
        const Token& t = akt();
        if (t.istOp("{")) return liesBlock(ort);
        std::vector<AnweisungPtr> v;
        liesAnweisung(ort, v);
        if (v.size() == 1) return std::move(v.front());
        return block(std::move(v), t);
    }

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

    /// Eine Marke `name@` vor einer Schleife (H2)?
    [[nodiscard]] bool istMarke(std::size_t i) const
    {
        return tok(i).art == TokenArt::Bezeichner && tok(i + 1).istOp("@") && !tok(i + 1).abstandDavor;
    }

    void liesAnweisung(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        if (nimm(";")) return;
        if (t.istOp("@") && !tok(m_p + 1).abstandDavor)
        {
            ueberAnnotationen();  // vor einer lokalen Deklaration
            return;
        }
        if (istMarke(m_p))
        {
            const std::string marke(t.text);
            m_p += 2;
            const Token& n = akt();
            std::vector<AnweisungPtr> eine;
            liesAnweisung(ort, eine);
            // an einer Schleife: ihre Marke; an einem Lambda (`lauf@{ … }`) meint `return@lauf` das Lambda
            const bool schleife = n.istWort("for") || n.istWort("while") || n.istWort("do");
            if (schleife && eine.size() == 1) eine.front()->marke = marke;
            for (AnweisungPtr& a : eine) aus.push_back(std::move(a));
            return;
        }
        if (t.istWort("if"))
        {
            aus.push_back(liesWenn(ort));
            return;
        }
        if (t.istWort("when"))
        {
            aus.push_back(liesWhen(ort));
            return;
        }
        if (t.istWort("while"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Solange, t);
            a->text = liesBedingung(ort);
            a->kinder.push_back(liesRumpf(ort));
            aus.push_back(std::move(a));
            return;
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
            aus.push_back(std::move(a));
            return;
        }
        if (t.istWort("for"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::FuerJedes, t);  // for (x in xs): je Element
            a->text = liesBedingung(ort);
            a->kinder.push_back(liesRumpf(ort));
            aus.push_back(std::move(a));
            return;
        }
        if (t.istWort("try"))
        {
            aus.push_back(liesVersuch(ort));
            return;
        }
        if (t.istWort("return"))
        {
            ++m_p;
            std::string kopf = "return";
            if (akt().istOp("@") && !akt().abstandDavor && namenhaft(m_p + 1))
            {
                kopf += "@" + std::string(tok(m_p + 1).text);  // return@forEach: aus dem Lambda
                m_p += 2;
            }
            if (endetHier(m_p))
            {
                AnweisungPtr a = neu(AnweisungArt::Ruecksprung, t);
                if (kopf != "return") a->kopf = kopf;
                aus.push_back(std::move(a));
                nimm(";");
                return;
            }
            if (kopf == "return")
            {
                liesWertOderText(ort, "return", aus);
                return;
            }
            const std::size_t e = anweisungsEnde(m_p);
            AnweisungPtr a = neu(AnweisungArt::Ruecksprung, t, funktionsText(m_p, e, ort));
            a->kopf = kopf + " " + a->text;
            aus.push_back(std::move(a));
            m_p = e;
            nimm(";");
            return;
        }
        if (t.istWort("throw"))
        {
            ++m_p;
            const std::size_t e = anweisungsEnde(m_p);
            aus.push_back(neu(AnweisungArt::Wurf, t, funktionsText(m_p, e, ort)));
            m_p = e;
            nimm(";");
            return;
        }
        if (t.istWort("break") || t.istWort("continue"))
        {
            ++m_p;
            std::string marke;  // break@marke: die markierte Schleife (H2)
            if (akt().istOp("@") && !akt().abstandDavor && namenhaft(m_p + 1))
            {
                marke = std::string(tok(m_p + 1).text);
                m_p += 2;
            }
            nimm(";");
            aus.push_back(neu(t.istWort("break") ? AnweisungArt::Abbruch : AnweisungArt::Fortsetzung, t, std::move(marke)));
            return;
        }
        // lokale Deklarationen: fun, class, object, interface (mit Modifikatoren)
        {
            const std::size_t gemerkt = m_p;
            const Vorsatz vorsatz = liesVorsatz();
            const Token& d = akt();
            const bool objektDekl = d.istWort("object") && tok(m_p + 1).art == TokenArt::Bezeichner;
            if (d.istWort("fun") && (tok(m_p + 1).art == TokenArt::Bezeichner || tok(m_p + 1).istOp("<")))
            {
                liesFunktion(ort, aus, vorsatz);
                return;
            }
            if (d.istWort("class") || d.istWort("interface") || objektDekl)
            {
                liesKlasse(ort, aus, vorsatz);
                return;
            }
            m_p = gemerkt;
        }
        if (t.istWort("val") || t.istWort("var"))
        {
            // eine lokale Deklaration; `val x = if/when/try …` verzweigt (K3)
            const std::size_t e = anweisungsEnde(m_p);
            const std::size_t gleich = oben(m_p, e, "=");
            if (gleich < e && liesWert(ort, gleich + 1, textVon(m_p, gleich + 1), aus)) return;
            aus.push_back(neu(AnweisungArt::Einfach, t, funktionsText(m_p, e, ort)));
            m_p = e;
            nimm(";");
            return;
        }
        // ein Ausdruck; `x = if/when/try …` verzweigt (K3)
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
        const std::size_t gleich = oben(m_p, e, "=");
        if (gleich < e && liesWert(ort, gleich + 1, textVon(m_p, gleich + 1), aus)) return;
        aus.push_back(neu(AnweisungArt::Einfach, t, funktionsText(m_p, e, ort)));
        m_p = e;
        nimm(";");
    }

    /// Ab m_p ein Wert: ist es `if`/`when`/`try` bis zum Ende der Anweisung, verzweigt er (je Zweig
    /// `vorn wert`, K3); sonst eine Anweisung `vorn wert` (bei `return` ein Rueckspung).
    void liesWertOderText(const Ort& ort, const std::string& vorn, std::vector<AnweisungPtr>& aus)
    {
        if (liesWert(ort, m_p, vorn, aus)) return;
        const Token& t = akt();
        const std::size_t e = anweisungsEnde(m_p);
        if (t.istWort("throw"))
            aus.push_back(neu(AnweisungArt::Wurf, t, funktionsText(m_p + 1, e, ort)));
        else if (vorn == "return")
            aus.push_back(neu(AnweisungArt::Ruecksprung, t, funktionsText(m_p, e, ort)));
        else
        {
            std::string text = funktionsText(m_p, e, ort);
            aus.push_back(neu(AnweisungArt::Einfach, t, vorn.empty() ? std::move(text) : vorn + " " + text));
        }
        m_p = e;
        nimm(";");
    }

    /// Steht bei `wo` ein `if`, `when` oder `try`, das die Anweisung ganz ausmacht: es wird als
    /// Anweisung gelesen, und jeder Zweig endet mit `vorn wert` (K3). false = nicht (m_p bleibt).
    bool liesWert(const Ort& ort, std::size_t wo, const std::string& vorn, std::vector<AnweisungPtr>& aus)
    {
        const Token& w = tok(wo);
        if (!w.istWort("if") && !w.istWort("when") && !w.istWort("try")) return false;
        if (w.istWort("if") && !istVollesIf(wo)) return false;
        // was das Lesen anlegt, faellt beim Zuruecknehmen wieder weg
        const std::size_t gemerkt = m_p;
        const std::size_t meldungen = m_aus.meldungen.size();
        const std::size_t funktionen = m_aus.einheit.funktionen.size();
        const std::size_t typen = m_aus.einheit.typen.size();
        const std::size_t innere = m_innere.size();
        const std::set<std::string> anonyme = m_anonyme;
        m_p = wo;
        AnweisungPtr a = w.istWort("if") ? liesWenn(ort) : w.istWort("when") ? liesWhen(ort) : liesVersuch(ort);
        if (!endetHier(m_p))
        {
            // es geht weiter (`when (x) {…}.also { … }`): doch Text
            m_p = gemerkt;
            m_aus.meldungen.resize(meldungen);
            m_aus.einheit.funktionen.erase(m_aus.einheit.funktionen.begin() + static_cast<std::ptrdiff_t>(funktionen),
                                           m_aus.einheit.funktionen.end());
            m_aus.einheit.typen.erase(m_aus.einheit.typen.begin() + static_cast<std::ptrdiff_t>(typen),
                                      m_aus.einheit.typen.end());
            m_innere.resize(innere);
            m_anonyme = anonyme;
            return false;
        }
        machWert(*a, vorn);
        aus.push_back(std::move(a));
        nimm(";");
        return true;
    }

    /// Hat das `if` bei i ein `else` (sonst ist es kein Wert)?
    [[nodiscard]] bool istVollesIf(std::size_t i) const
    {
        if (!tok(i + 1).istOp("(")) return false;
        std::size_t j = nachKlammer(i + 1);
        int tiefe = 0;
        for (; !amEnde(j); ++j)
        {
            const Token& t = tok(j);
            if (oeffnet(t))
            {
                j = nachKlammer(j) - 1;
                continue;
            }
            if (schliesst(t) || t.istOp(";")) return false;
            if (t.istWort("if")) ++tiefe;
            if (t.istWort("else") && tiefe-- == 0) return true;
        }
        return false;
    }

    /// Jeder Zweig endet mit `vorn wert`: die letzte Anweisung eines Zweigs wird es (K3).
    static void machWert(Anweisung& a, const std::string& vorn)
    {
        switch (a.art)
        {
            case AnweisungArt::Einfach:
                if (vorn == "return")
                    a.art = AnweisungArt::Ruecksprung;
                else if (!vorn.empty())
                    a.text = vorn + " " + a.text;
                return;
            case AnweisungArt::Block:
                if (!a.kinder.empty()) machWert(*a.kinder.back(), vorn);
                return;
            case AnweisungArt::Wenn:
                if (!a.kinder.empty()) machWert(*a.kinder[0], vorn);
                if (a.sonst) machWert(*a.sonst, vorn);
                return;
            case AnweisungArt::Auswahl:
            {
                if (a.kinder.empty()) return;
                auto& arme = a.kinder[0]->kinder;
                for (std::size_t i = 0; i < arme.size(); ++i)
                {
                    const bool letzte = i + 1 == arme.size() || arme[i + 1]->art == AnweisungArt::Fall;
                    if (letzte && arme[i]->art != AnweisungArt::Fall) machWert(*arme[i], vorn);
                }
                return;
            }
            case AnweisungArt::Versuch:
                for (std::size_t i = 0; i < a.kinder.size(); ++i)
                {
                    Anweisung& kind = *a.kinder[i];
                    if (i == 0)
                        machWert(kind, vorn);
                    else if (kind.art == AnweisungArt::Fang && !kind.kinder.empty())
                        machWert(*kind.kinder[0], vorn);
                }
                return;
            default:
                return;  // return, throw, break …: der Zweig endet ohne Wert
        }
    }

    AnweisungPtr liesWenn(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        AnweisungPtr a = neu(AnweisungArt::Wenn, t);
        a->text = liesBedingung(ort);
        a->kinder.push_back(liesRumpf(ort));
        nimm(";");
        if (akt().istWort("else"))
        {
            ++m_p;
            a->sonst = liesRumpf(ort);
        }
        return a;
    }

    /// `when (x) { a, b -> …; is T -> …; else -> … }`: eine Auswahl ohne Durchfallen;
    /// `when { c -> …; else -> … }` ohne Gegenstand: eine Kette if / else if / else (K3).
    AnweisungPtr liesWhen(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        std::string gegenstand;
        const bool mitGegenstand = akt().istOp("(");
        if (mitGegenstand) gegenstand = liesBedingung(ort);
        struct Arm
        {
            std::string bedingung;  ///< leer = else
            const Token* bei;
            AnweisungPtr rumpf;
        };
        std::vector<Arm> arme;
        if (!nimm("{"))
        {
            melde(Schwere::Fehler, akt(), "'{' of when expected");
            AnweisungPtr a = neu(AnweisungArt::Einfach, t, "when " + gegenstand);
            return a;
        }
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            const std::size_t davor = m_p;
            if (nimm(";")) continue;
            const Token& bei = akt();
            const std::size_t pfeil = suche(m_p, {"->"});
            if (!tok(pfeil).istOp("->"))
            {
                melde(Schwere::Fehler, akt(), "'->' of the when branch is missing");
                m_p = std::max(pfeil, m_p + 1);
                continue;
            }
            std::string bedingung;
            if (!(akt().istWort("else") && pfeil == m_p + 1))
            {
                if (mitGegenstand)
                    bedingung = funktionsText(m_p, pfeil, ort);
                else
                {
                    // a, b -> …: eines von beiden
                    for (const Bereich& s : teile(m_p, pfeil))
                        bedingung += (bedingung.empty() ? "" : " || ") + funktionsText(s.first, s.second, ort);
                }
            }
            m_p = pfeil + 1;
            AnweisungPtr rumpf = liesRumpf(ort);
            rumpf->innere.insert(rumpf->innere.end(), m_innere.begin(), m_innere.end());
            m_innere.clear();
            arme.push_back({std::move(bedingung), &bei, std::move(rumpf)});
            if (m_p == davor) ++m_p;
        }
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
        if (mitGegenstand)
        {
            AnweisungPtr a = neu(AnweisungArt::Auswahl, t, gegenstand);
            a->kopf = "when (" + gegenstand + ")";
            a->ohneDurchfall = true;
            AnweisungPtr rumpf = neu(AnweisungArt::Block, t);
            for (Arm& arm : arme)
            {
                rumpf->kinder.push_back(neu(AnweisungArt::Fall, *arm.bei, arm.bedingung));
                rumpf->kinder.push_back(std::move(arm.rumpf));
            }
            a->kinder.push_back(std::move(rumpf));
            return a;
        }
        // die Kette von hinten: jedes Wenn traegt das naechste als sonst
        AnweisungPtr kette;
        for (auto arm = arme.rbegin(); arm != arme.rend(); ++arm)
        {
            if (arm->bedingung.empty())
            {
                kette = std::move(arm->rumpf);
                continue;
            }
            AnweisungPtr w = neu(AnweisungArt::Wenn, *arm->bei, arm->bedingung);
            w->kinder.push_back(std::move(arm->rumpf));
            w->sonst = std::move(kette);
            kette = std::move(w);
        }
        if (!kette) kette = neu(AnweisungArt::Block, t);
        return kette;
    }

    AnweisungPtr liesVersuch(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        AnweisungPtr a = neu(AnweisungArt::Versuch, t);
        a->kinder.push_back(liesRumpf(ort));
        while (istName(m_p, "catch"))
        {
            const Token& bei = akt();
            ++m_p;
            AnweisungPtr fang = neu(AnweisungArt::Fang, bei);
            if (akt().istOp("("))
            {
                const std::size_t zu = ueberKlammer(m_p);
                fang->text = textVon(m_p + 1, tok(zu - 1).istOp(")") ? zu - 1 : zu);
                m_p = zu;
            }
            fang->kopf = "catch (" + fang->text + ")";
            fang->kinder.push_back(liesRumpf(ort));
            a->kinder.push_back(std::move(fang));
        }
        if (istName(m_p, "finally"))
        {
            const Token& bei = akt();
            ++m_p;
            AnweisungPtr schluss = neu(AnweisungArt::Schluss, bei);
            schluss->kinder.push_back(liesRumpf(ort));
            a->kinder.push_back(std::move(schluss));
        }
        if (a->kinder.size() == 1) melde(Schwere::Fehler, akt(), "'catch' or 'finally' after try is missing");
        return a;
    }

    std::vector<std::string> m_innere;  ///< die Funktionen, die in der laufenden Anweisung definiert wurden
    int m_letzteZeile = 0;
    std::set<std::string> m_anonyme;  ///< die vergebenen `<lambda:…>`/`<anonymous:…>`, je mit Bereich
};

constexpr std::string_view kEndungen[] = {"kt"};  // `.kts` noch nicht (Entscheid K6)

class Kotlin final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "Kotlin"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungen; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return kTabelle; }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& /*optionen*/) const override
    {
        Ergebnis ergebnis;
        ergebnis.einheit.name = std::string(einheitName);
        KtLeser(text, ergebnis).liesDatei();
        ergaenzeLogik(ergebnis.einheit, kTabelle, kRegeln);
        return ergebnis;
    }
};

}  // namespace

const Sprache& kotlin()
{
    static const Kotlin sprache;
    return sprache;
}

}  // namespace c2d::sprache
