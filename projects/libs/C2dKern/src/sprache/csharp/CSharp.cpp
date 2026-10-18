/**
 * @file CSharp.cpp
 * @brief C# und Java: die Sprachtabellen und der Leser in rekursivem Abstieg
 *
 * C#: bedingte Uebersetzung (`#if`, `#define`) wird vorher ausgewertet wie in der C-Familie
 * (sprache/Bedingt.hpp); die uebrigen Direktiven (`#region`, `#pragma`, `#nullable`) uebergeht
 * der Leser. Er versteht die Gliederung: Namensraeume, Typen (class, struct, record,
 * interface, enum, delegate) mit Feldern, Eigenschaften, Methoden, Konstruktoren, Operatoren,
 * und die Anweisungen der Ruempfe; Ausdruecke bleiben Text — ausser einem Lambda mit
 * Block-Rumpf (eine eigene Funktion, wie in JavaScript) und einem `switch`-Ausdruck auf
 * oberster Ebene von return, Zuweisung oder Deklaration (eine Auswahl ohne Durchfallen).
 *
 * Java (Dialekt::java, Entscheide G1, H1–H4): dieselbe Gliederung ohne Namensraeume,
 * Eigenschaften, Operatoren; dazu anonyme Klassen (je Methode eine Funktion
 * `<anonymous:Zeile>.name`), Initialisierungsbloecke (`<static:Zeile>`, `<init:Zeile>`), enum mit
 * Rumpf, Marken an Schleifen. Annotationen, `throws` und `package` werden ueberlesen.
 */
#include "CSharp.hpp"

#include "../Bedingt.hpp"
#include "../Leser.hpp"
#include "../Logik.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
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

// Die reservierten Woerter von C# 12; die kontextabhaengigen (var, async, await, yield, record,
// get, set, init, when, where, partial, and, or, not …) sind Namen und werden an ihrer Stelle erkannt.
constexpr std::string_view kSchluesselwoerter[] = {
    "abstract", "as",        "base",      "bool",      "break",    "byte",     "case",      "catch",
    "char",     "checked",   "class",     "const",     "continue", "decimal",  "default",   "delegate",
    "do",       "double",    "else",      "enum",      "event",    "explicit", "extern",    "false",
    "finally",  "fixed",     "float",     "for",       "foreach",  "goto",     "if",        "implicit",
    "in",       "int",       "interface", "internal",  "is",       "lock",     "long",      "namespace",
    "new",      "null",      "object",    "operator",  "out",      "override", "params",    "private",
    "protected", "public",   "readonly",  "ref",       "return",   "sbyte",    "sealed",    "short",
    "sizeof",   "stackalloc", "static",   "string",    "struct",   "switch",   "this",      "throw",
    "true",     "try",       "typeof",    "uint",      "ulong",    "unchecked", "unsafe",   "ushort",
    "using",    "virtual",   "void",      "volatile",  "while",
};

constexpr std::string_view kOperatoren[] = {
    ">>>=", ">>=", "<<=", "?\?=", ">>>", "=>", "??", "?.", "::", "->", "++", "--", "&&", "||", "==", "!=",
    "<=",   ">=",  "+=",  "-=",  "*=",  "/=", "%=", "&=", "|=", "^=", "<<", "..", "{",  "}",  "[",  "]",
    "(",    ")",   ";",   ",",   ".",   ":",  "?",  "~",  "!",  "+",  "-",  "*",  "/",  "%",  "&",  "|",
    "^",    "<",   ">",   "=",   "@",
};

constexpr lexer::Sprachtabelle kTabelle{
    .schluesselwoerter = kSchluesselwoerter,
    .operatoren = kOperatoren,
    .zeilenKommentar = "//",
    .blockKommentarAuf = "/*",
    .blockKommentarZu = "*/",
    .direktive = '#',
    .ziffernTrenner = '_',
    .dreifachZeichenketten = true,
    .csharpZeichenketten = true,
};

// Die reservierten Woerter von Java 21 (samt den Literalen); die kontextabhaengigen (var, yield,
// record, sealed, permits, when …) sind Namen und werden an ihrer Stelle erkannt.
constexpr std::string_view kSchluesselwoerterJava[] = {
    "abstract",  "assert",     "boolean",  "break",     "byte",     "case",       "catch",     "char",
    "class",     "const",      "continue", "default",   "do",       "double",     "else",      "enum",
    "extends",   "false",      "final",    "finally",   "float",    "for",        "goto",      "if",
    "implements", "import",    "instanceof", "int",     "interface", "long",      "native",    "new",
    "null",      "package",    "private",  "protected", "public",   "return",     "short",     "static",
    "strictfp",  "super",      "switch",   "synchronized", "this",  "throw",      "throws",    "transient",
    "true",      "try",        "void",     "volatile",  "while",
};

constexpr std::string_view kOperatorenJava[] = {
    ">>>=", ">>=", "<<=", ">>>", "...", "->", "::", "++", "--", "&&", "||", "==", "!=", "<=", ">=", "+=",
    "-=",   "*=",  "/=",  "%=",  "&=",  "|=", "^=", "<<", "{",  "}",  "[",  "]",  "(",  ")",  ";",  ",",
    ".",    ":",   "?",   "~",   "!",   "+",  "-",  "*",  "/",  "%",  "&",  "|",  "^",  "<",  ">",  "=",
    "@",
};

constexpr lexer::Sprachtabelle kTabelleJava{
    .schluesselwoerter = kSchluesselwoerterJava,
    .operatoren = kOperatorenJava,
    .zeilenKommentar = "//",
    .blockKommentarAuf = "/*",
    .blockKommentarZu = "*/",
    .ziffernTrenner = '_',
    .dreifachZeichenketten = true,  // Textbloecke """…"""
    .namensZeichen = "$",           // erlaubt, und erzeugter Code nutzt es (javacc: jj_3R_12$)
};

constexpr std::string_view kZuweisungen[] = {"=",  "+=", "-=", "*=",  "/=",  "%=",  "&=",
                                             "|=", "^=", "<<=", ">>=", ">>>=", "?\?="};

constexpr Ausdrucksregeln kRegeln{
    .wahl = Ausdrucksregeln::Wahl::Fragezeichen,
    .nichtUmfasstVergleich = false,
    .zuweisungen = kZuweisungen,
    .zeiger = false,
};

/// Woerter vor einer Deklaration (Typ oder Glied).
constexpr std::string_view kModifikatoren[] = {
    "public",   "private", "protected", "internal", "static",   "abstract", "sealed",   "virtual",
    "override", "readonly", "const",    "extern",   "unsafe",   "volatile", "new",      "partial",
    "async",    "required", "file",     "implicit", "explicit", "ref",      "fixed",
};

/// Java: Woerter vor einer Deklaration (`sealed` ist weich, `non-sealed` drei Token).
constexpr std::string_view kModifikatorenJava[] = {
    "public", "private",      "protected", "static",   "abstract", "final", "native",
    "synchronized", "transient", "volatile", "strictfp", "default",  "sealed",
};

/// Das eingebaute Typwort (`int`, `string` …): es kann einen Typ beginnen.
constexpr std::string_view kTypWoerter[] = {"bool",   "byte",   "char",  "decimal", "double", "float", "int",
                                            "long",   "object", "sbyte", "short",   "string", "uint",  "ulong",
                                            "ushort", "void",   "boolean"};

/// Der Name, unter dem der Compiler die Anweisungen auf oberster Ebene fuehrt (C# 9).
constexpr std::string_view kOberste = "<Main>$";

template <std::size_t N>
bool enthaelt(const std::string_view (&liste)[N], std::string_view wort)
{
    return std::find(std::begin(liste), std::end(liste), wort) != std::end(liste);
}

/// `@name` (ein woertlicher Name, auch ein Schluesselwort) heisst `name`.
std::string_view ohneAt(std::string_view name)
{
    return name.starts_with('@') ? name.substr(1) : name;
}

class CsLeser final : public Leser
{
public:
    CsLeser(std::string_view text, Ergebnis& aus, csjava::Dialekt dialekt)
        : Leser(text, csjava::tabelle(dialekt), aus), m_java(dialekt.java)
    {
        // die Direktiven, die nach dem Auswerten bleiben (#region, #pragma, #nullable …): nicht gelesen
        std::erase_if(m_t, [](const Token& t) { return t.art == TokenArt::Direktive; });
    }

    void liesEinheit()
    {
        const Ort oben;
        std::vector<AnweisungPtr> code;
        liesFolgeVonGliedern(oben, code, false);
        if (!m_java) ordneBasenZu();  // Java nennt Schnittstellen selbst (implements)
        // Anweisungen auf oberster Ebene (C# 9): die Methode, die der Compiler daraus macht
        if (code.empty()) return;
        Funktion f;
        f.name = std::string(kOberste);
        f.zeile = code.front()->zeile;
        f.bisZeile = zeileVor(m_p);
        f.rumpf = neu(AnweisungArt::Block, m_t.front());
        f.rumpf->zeile = f.zeile;
        f.rumpf->kinder = std::move(code);
        m_aus.einheit.funktionen.push_back(std::move(f));
    }

private:
    /// Eine Klasse nennt ihre Basis vorn; ist das eine Schnittstelle derselben Datei, setzt sie sie um.
    void ordneBasenZu()
    {
        auto& typen = m_aus.einheit.typen;
        for (Typ& t : typen)
        {
            if (t.art == TypArt::Schnittstelle || t.basen.empty()) continue;
            const std::string& basis = t.basen.front();
            const std::string kurz = basis.substr(0, basis.find('<'));
            const bool schnittstelle = std::any_of(typen.begin(), typen.end(), [&](const Typ& x) {
                return x.art == TypArt::Schnittstelle && (x.name == kurz || x.vollerName() == kurz);
            });
            if (!schnittstelle) continue;
            t.realisiert.insert(t.realisiert.begin(), basis);
            t.basen.erase(t.basen.begin());
        }
    }

    enum class Ebene
    {
        Namensraum,
        Typ,
        Funktion,
    };

    struct Ort
    {
        Ebene ebene = Ebene::Namensraum;
        std::string bereich;         ///< Namensraum, in einem Typ sein voller Name
        std::size_t typ = kKeiner;   ///< in einem Typ: sein Platz unter Einheit::typen
        std::string typName;         ///< sein Name ohne Bereich (Konstruktor)
        std::string funktion;        ///< der volle Name der Funktion, in der gelesen wird
        /// Java, im Rumpf einer anonymen Klasse (oder einer enum-Konstante): ihre Methoden heissen
        /// `<vorsatz>.name` im Bereich `anonymBereich`, mit `anonymAussen` als aeusserer Funktion.
        std::string anonym;
        std::string anonymBereich;
        std::string anonymAussen;
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

    /// i auf `<` von Typ-Argumenten: dahinter; sonst i.
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
            else if (t.istOp(">")) tiefe -= 1;
            else if (t.istOp(">>")) tiefe -= 2;
            else if (t.istOp(">>>")) tiefe -= 3;
            else if (t.istOp(";") || t.istOp("{") || t.istOp("}") || t.istOp("=") || t.istOp("=>")) return i;
            if (tiefe <= 0) return j + 1;
        }
        return i;
    }

    /// Ein Typ ab i: `global::A.B<T>[]?`, ein eingebautes Wort, ein Tupel `(int a, int b)`, Zeiger
    /// `*`. Liefert die Stelle dahinter; i = kein Typ.
    [[nodiscard]] std::size_t typEnde(std::size_t i) const
    {
        std::size_t j = i;
        if (tok(j).istOp("("))
            j = nachKlammer(j);
        else
        {
            if (tok(j).art == TokenArt::Bezeichner || (tok(j).art == TokenArt::Schluesselwort && enthaelt(kTypWoerter, tok(j).text)))
                ++j;
            else
                return i;
            for (;;)
            {
                j = spitzeEnde(j);
                if ((tok(j).istOp(".") || tok(j).istOp("::")) && tok(j + 1).art == TokenArt::Bezeichner)
                {
                    j += 2;
                    continue;
                }
                break;
            }
        }
        for (;;)
        {
            if (!m_java && (tok(j).istOp("?") || tok(j).istOp("*")))  // C#: Nullable, Zeiger
                ++j;
            else if (tok(j).istOp("[") && (tok(j + 1).istOp("]") || tok(j + 1).istOp(",")))
                j = nachKlammer(j);
            else if (m_java && tok(j).istOp("..."))  // Java: varargs `String... namen`
                ++j;
            else
                break;
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

    /// C# `[Attribut]`, Java `@Annotation`, `@a.b.C(…)` (nicht `@interface`): ueberlesen.
    void ueberAttribute() { m_p = nachAttributen(m_p); }

    [[nodiscard]] std::size_t nachAttributen(std::size_t i) const
    {
        if (!m_java)
        {
            while (tok(i).istOp("[")) i = nachKlammer(i);
            return i;
        }
        while (tok(i).istOp("@") && !tok(i + 1).istWort("interface"))
        {
            ++i;
            while (namenhaft(i) && tok(i + 1).istOp(".") && namenhaft(i + 2)) i += 2;
            if (namenhaft(i)) ++i;
            if (tok(i).istOp("(")) i = nachKlammer(i);
        }
        return i;
    }

    /// Die Stelle der ersten Zuweisung `=` in [a, b) auf oberster Ebene; b = keine.
    [[nodiscard]] std::size_t gleichIn(std::size_t a, std::size_t b) const
    {
        for (std::size_t i = a; i < b; ++i)
        {
            if (oeffnet(tok(i)))
            {
                i = nachKlammer(i) - 1;
                continue;
            }
            if (tok(i).istOp("=")) return i;
        }
        return b;
    }

    /// Ein Parameter `[attr] [ref|out|in|params|this] Typ name [= vorgabe]`: Typ und Name (ohne Vorgabe).
    [[nodiscard]] Parameter parameter(const Bereich& s) const
    {
        std::size_t a = std::min(nachAttributen(s.first), s.second);
        if (m_java)
            while (a < s.second && tok(a).istWort("final")) a = std::min(nachAttributen(a + 1), s.second);
        const std::size_t ende = gleichIn(a, s.second);
        Parameter p;
        // Java in der Form von C: `String namen[]` — der Name vor den Klammern, sie gehoeren zum Typ
        if (m_java && ende >= a + 4 && tok(ende - 1).istOp("]") && tok(ende - 2).istOp("[") &&
            tok(ende - 3).art == TokenArt::Bezeichner)
        {
            p.typ = textVon(a, ende - 3) + "[]";
            p.name = std::string(tok(ende - 3).text);
            return p;
        }
        if (ende > a + 1 && tok(ende - 1).art == TokenArt::Bezeichner)
        {
            p.typ = textVon(a, ende - 1);
            p.name = std::string(tok(ende - 1).text);
        }
        else
            p.typ = textVon(a, ende);
        return p;
    }

    std::vector<Parameter> parameterIn(std::size_t auf, std::size_t zu) const
    {
        std::vector<Parameter> aus;
        const std::size_t innen = tok(zu - 1).istOp(")") || tok(zu - 1).istOp("]") ? zu - 1 : zu;
        for (const Bereich& s : teileSpitz(auf + 1, innen)) aus.push_back(parameter(s));
        return aus;
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
            if (tok(i).istOp(">>")) spitze = std::max(0, spitze - 2);
            if (spitze == 0 && tok(i).istOp(","))
            {
                if (i > anfang) stuecke.emplace_back(anfang, i);
                anfang = i + 1;
            }
        }
        if (b > anfang) stuecke.emplace_back(anfang, b);
        return stuecke;
    }

    // --- Funktionen ------------------------------------------------------------------------

    std::size_t neueFunktion(Funktion f, const Ort& ort)
    {
        if (ort.ebene == Ebene::Funktion)
        {
            f.aussen = ort.funktion;
            f.bereich = ort.funktion;
        }
        else if (!ort.anonym.empty())  // Java: eine Methode einer anonymen Klasse (Entscheid H1)
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

    /// Der Rumpf ab m_p: `{ … }`, `=> ausdruck;` oder `;` (ohne Rumpf). `liefert`: ein `=>` wird
    /// ein return (sonst eine Anweisung, bei void und Konstruktoren).
    void liesRumpfVon(std::size_t platz, const Ort& ort, bool liefert)
    {
        const Ort innen = innenVon(platz, ort);
        const Token& bei = akt();
        AnweisungPtr r;
        if (akt().istOp("{"))
            r = liesBlock(innen);
        else if (akt().istOp("=>"))
        {
            ++m_p;
            const std::size_t e = suche(m_p, {";"});
            std::vector<AnweisungPtr> kinder;
            liesAusdruckAnweisung(innen, m_p, e, liefert ? "return" : "", kinder);
            m_p = e;
            m_letzteZeile = zeileVor(m_p + 1);
            nimm(";");
            r = block(std::move(kinder), bei);
        }
        else
        {
            nimm(";");
            return;  // ohne Rumpf (abstrakt, extern, partiell, in einer Schnittstelle)
        }
        m_aus.einheit.funktionen[platz].rumpf = std::move(r);
        m_aus.einheit.funktionen[platz].bisZeile = m_letzteZeile;
    }

    /// Der Text von [a, b); ein Lambda oder eine anonyme Methode mit Block-Rumpf darin wird eine
    /// eigene Funktion, im Text steht ihr Rumpf als `{…}` (wie in JavaScript).
    std::string funktionsText(std::size_t a, std::size_t b, const Ort& ort)
    {
        std::vector<Bereich> ruempfe;
        std::vector<std::string> innere;
        std::size_t j = a;
        while (j < b && !amEnde(j))
        {
            // Java: eine anonyme Klasse `new Typ(…) { … }` — ihre Methoden werden Funktionen (H1)
            if (m_java && tok(j).istWort("new"))
            {
                const std::size_t typ = typEnde(j + 1);
                if (typ > j + 1 && tok(typ).istOp("(") && tok(nachKlammer(typ)).istOp("{"))
                {
                    const std::size_t koerper = nachKlammer(typ);
                    const std::size_t danach = liesAnonymeKlasse(koerper, anonymerName(tok(j), ort), ort, innere);
                    ruempfe.emplace_back(koerper, danach);
                    j = danach;
                    continue;
                }
            }
            std::size_t anfang = j;
            std::size_t rumpf = kKeiner;
            std::size_t auf = kKeiner;
            std::size_t zu = kKeiner;
            if (tok(j).istOp(m_java ? "->" : "=>") && tok(j + 1).istOp("{") && j > a)
            {
                rumpf = j + 1;
                if (tok(j - 1).art == TokenArt::Bezeichner)
                {
                    anfang = j - 1;
                    auf = anfang;
                    zu = j;
                }
                else if (tok(j - 1).istOp(")"))
                {
                    int tiefe = 0;
                    std::size_t o = j - 1;
                    while (true)
                    {
                        if (schliesst(tok(o))) ++tiefe;
                        else if (oeffnet(tok(o)) && --tiefe == 0) break;
                        if (o == a) break;
                        --o;
                    }
                    auf = o;
                    zu = j;
                    anfang = o;
                }
                if (anfang > a && (istName(anfang - 1, "async") || tok(anfang - 1).istWort("static"))) --anfang;
            }
            else if (tok(j).istWort("delegate") && (tok(j + 1).istOp("(") || tok(j + 1).istOp("{")))
            {
                std::size_t k = j + 1;
                if (tok(k).istOp("("))
                {
                    auf = k;
                    zu = nachKlammer(k);
                    k = zu;
                }
                if (tok(k).istOp("{")) rumpf = k;
            }
            if (rumpf == kKeiner)
            {
                ++j;
                continue;
            }
            std::string name;
            if (anfang > a && tok(anfang - 1).istOp("=") && anfang >= a + 2 && tok(anfang - 2).art == TokenArt::Bezeichner)
                name = std::string(tok(anfang - 2).text);
            Funktion f;
            if (name.empty())
            {
                name = "<lambda:" + std::to_string(tok(anfang).ort.zeile) + ">";
                const auto& alle = m_aus.einheit.funktionen;
                if (std::any_of(alle.begin(), alle.end(), [&](const Funktion& x) { return x.name == name; }))
                    name = "<lambda:" + std::to_string(tok(anfang).ort.zeile) + ":" + std::to_string(tok(anfang).ort.spalte) + ">";
            }
            f.name = name;
            f.zeile = tok(anfang).ort.zeile;
            if (auf != kKeiner)
            {
                if (tok(auf).istOp("("))
                    for (const Bereich& s : teileSpitz(auf + 1, zu - 1))
                    {
                        // ein Lambda-Parameter ohne Typ ist nur ein Name
                        Parameter p = parameter(s);
                        if (p.name.empty()) std::swap(p.name, p.typ);
                        f.parameter.push_back(std::move(p));
                    }
                else
                    f.parameter.push_back({{}, std::string(tok(auf).text)});
            }
            const std::size_t platz = neueFunktion(std::move(f), ort);
            innere.push_back(m_aus.einheit.funktionen[platz].vollerName());
            const std::size_t gemerkt = m_p;
            std::vector<std::string> gemerktInnere = std::exchange(m_innere, {});  // die Anweisung draussen sammelt weiter
            const std::string gemerktYield = std::exchange(m_yieldVorn, {});
            m_p = rumpf;
            AnweisungPtr r = liesBlock(innenVon(platz, ort));
            m_aus.einheit.funktionen[platz].rumpf = std::move(r);
            m_aus.einheit.funktionen[platz].bisZeile = m_letzteZeile;
            const std::size_t danach = m_p;
            m_p = gemerkt;
            m_innere = std::move(gemerktInnere);
            m_yieldVorn = gemerktYield;
            ruempfe.emplace_back(rumpf, danach);
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

    /// Der Vorsatz der Methoden einer anonymen Klasse, die bei `bei` beginnt: `<anonymous:Zeile>`,
    /// zweimal in einer Zeile desselben Bereichs `<anonymous:Zeile:Spalte>`.
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

    /// Java: der Rumpf einer anonymen Klasse (oder einer enum-Konstante) ab `auf` (die `{`). Ihre
    /// Methoden werden Funktionen `<vorsatz>.name` (Entscheid H1), ihre vollen Namen kommen nach
    /// `innere`; Felder und die Klasse selbst kommen in kein Diagramm. Liefert die Stelle hinter `}`;
    /// m_p bleibt.
    std::size_t liesAnonymeKlasse(std::size_t auf, const std::string& vorsatz, const Ort& ort,
                                  std::vector<std::string>& innere)
    {
        Ort innen;
        innen.ebene = Ebene::Typ;
        innen.typ = m_aus.einheit.typen.size();
        innen.funktion = ort.funktion;
        innen.anonym = vorsatz;
        innen.anonymBereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
        innen.anonymAussen = ort.ebene == Ebene::Funktion ? ort.funktion : std::string();
        innen.bereich = vollerName(innen.anonymBereich, vorsatz);  // fuer benannte Typen darin
        Typ huelle;  // nur zum Lesen: liesMitglied legt Felder und Methoden in einen Typ
        huelle.name = vorsatz;
        m_aus.einheit.typen.push_back(std::move(huelle));
        const std::size_t vorher = m_aus.einheit.funktionen.size();
        const std::size_t gemerkt = m_p;
        std::vector<std::string> gemerktInnere = std::exchange(m_innere, {});
        const std::string gemerktYield = std::exchange(m_yieldVorn, {});
        m_p = auf + 1;
        std::vector<AnweisungPtr> code;  // bleibt leer: im Rumpf einer Klasse stehen keine Anweisungen
        liesFolgeVonGliedern(innen, code, true);
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
        const std::size_t danach = m_p;
        m_p = gemerkt;
        m_innere = std::move(gemerktInnere);
        m_yieldVorn = gemerktYield;
        m_aus.einheit.typen.erase(m_aus.einheit.typen.begin() + static_cast<std::ptrdiff_t>(innen.typ));
        const std::string vorn = vorsatz + ".";
        for (std::size_t i = vorher; i < m_aus.einheit.funktionen.size(); ++i)
        {
            const Funktion& f = m_aus.einheit.funktionen[i];
            if (f.bereich == innen.anonymBereich && f.name.starts_with(vorn)) innere.push_back(f.vollerName());
        }
        return danach;
    }

    /// Java: `{ … }` bzw. `static { … }` in einer Klasse ab m_p (die `{`): eine Funktion
    /// `<init:Zeile>` bzw. `<static:Zeile>` (Entscheid H3).
    void liesInitialisierer(const Ort& ort, bool statisch)
    {
        Funktion f;
        f.name = std::string(statisch ? "<static:" : "<init:") + std::to_string(akt().ort.zeile) + ">";
        f.zeile = akt().ort.zeile;
        const std::size_t platz = neueFunktion(std::move(f), ort);
        liesRumpfVon(platz, ort, false);
    }

    // --- Glieder: Namensraum, Typen, Mitglieder ------------------------------------------------

    /// Glieder bis zur `}` (nicht genommen) bzw. bis zum Ende. Auf oberster Ebene sammelt `code`
    /// die Anweisungen (C# 9).
    void liesFolgeVonGliedern(const Ort& ort, std::vector<AnweisungPtr>& code, bool imBlock)
    {
        while (!amEnde(m_p))
        {
            if (imBlock && akt().istOp("}")) break;
            const std::size_t davor = m_p;
            liesGlied(ort, code);
            m_innere.clear();
            if (m_p == davor)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
            }
        }
    }

    void liesGlied(const Ort& ort, std::vector<AnweisungPtr>& code)
    {
        if (nimm(";")) return;
        if (m_java && ort.ebene == Ebene::Namensraum && (akt().istWort("package") || akt().istWort("import")))
        {
            // package: ueberlesen (die Namen blieben sonst lang); import: eine Einbindung wie using
            const bool einbinden = akt().istWort("import");
            ++m_p;
            if (einbinden && akt().istWort("static")) ++m_p;
            const std::size_t e = suche(m_p, {";"});
            if (einbinden) m_aus.einheit.einbindungen.push_back({textVon(m_p, e), false, tok(m_p).ort.zeile});
            m_p = e;
            nimm(";");
            return;
        }
        if (m_java)
        {
            liesGliedJava(ort, code);
            return;
        }
        // global using / using static / using A = B;  (nicht: using (…) oder using var, die sind Anweisungen)
        const std::size_t vorUsing = istName(m_p, "global") && tok(m_p + 1).istWort("using") ? m_p + 1 : m_p;
        if (tok(vorUsing).istWort("using") && !tok(vorUsing + 1).istOp("(") && !istName(vorUsing + 1, "var") &&
            ort.ebene == Ebene::Namensraum && !istLokaleDeklaration(vorUsing + 1))
        {
            m_p = vorUsing + 1;
            if (tok(m_p).istWort("static")) ++m_p;
            const std::size_t e = suche(m_p, {";"});
            const std::size_t gleich = gleichIn(m_p, e);
            m_aus.einheit.einbindungen.push_back({textVon(gleich < e ? gleich + 1 : m_p, e), false, tok(m_p).ort.zeile});
            m_p = e;
            nimm(";");
            return;
        }
        if (akt().istWort("namespace"))
        {
            ++m_p;
            const std::size_t e = m_p;
            while (tok(m_p).art == TokenArt::Bezeichner || tok(m_p).istOp(".")) ++m_p;
            std::string name;
            for (std::size_t i = e; i < m_p; ++i) name += tok(i).istOp(".") ? std::string("::") : std::string(tok(i).text);
            Ort innen = ort;
            innen.bereich = vollerName(ort.bereich, name);
            if (nimm(";"))
            {
                liesFolgeVonGliedern(innen, code, false);  // file-scoped: der Rest der Datei
                return;
            }
            if (!nimm("{"))
            {
                melde(Schwere::Fehler, akt(), "'{' expected");
                return;
            }
            liesFolgeVonGliedern(innen, code, true);
            if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
            return;
        }
        const std::size_t anfang = m_p;
        ueberAttribute();
        Sichtbarkeit sicht = Sichtbarkeit::Keine;
        bool statisch = false;
        bool abstrakt = false;
        bool virtuell = false;
        // ein weiches Wort (async, partial, required, file) vor `(`, `=`, `;` ist ein Name, kein Modifikator
        while (namenhaft(m_p) && enthaelt(kModifikatoren, tok(m_p).text) &&
               !(tok(m_p).art == TokenArt::Bezeichner &&
                 (tok(m_p + 1).istOp("(") || tok(m_p + 1).istOp("=") || tok(m_p + 1).istOp(";"))))
        {
            const std::string_view w = tok(m_p).text;
            if (w == "public") sicht = Sichtbarkeit::Oeffentlich;
            if (w == "protected") sicht = Sichtbarkeit::Geschuetzt;
            if (w == "private") sicht = Sichtbarkeit::Privat;
            statisch = statisch || w == "static" || w == "const";
            abstrakt = abstrakt || w == "abstract";
            virtuell = virtuell || w == "virtual" || w == "abstract" || w == "override";
            ++m_p;
        }
        const bool typWort = akt().istWort("class") || akt().istWort("struct") || akt().istWort("interface") ||
                             akt().istWort("enum") || (istName(m_p, "record") && (tok(m_p + 1).art == TokenArt::Bezeichner ||
                                                                                   tok(m_p + 1).istWort("class") ||
                                                                                   tok(m_p + 1).istWort("struct")));
        if (typWort)
        {
            liesTyp(ort, code);
            return;
        }
        if (akt().istWort("delegate"))
        {
            liesDelegat(ort);
            return;
        }
        if (ort.ebene == Ebene::Typ)
        {
            liesMitglied(ort, sicht, statisch, abstrakt, virtuell);
            return;
        }
        // auf oberster Ebene: eine Anweisung (C# 9) oder eine lokale Funktion darin
        m_p = anfang;
        Ort oben = ort;
        oben.ebene = Ebene::Funktion;
        oben.funktion = vollerName(ort.bereich, std::string(kOberste));
        const std::size_t vorher = code.size();
        liesAnweisung(oben, code);
        if (code.size() > vorher) code[vorher]->innere = std::move(m_innere);
    }

    /// Java: ein Glied — ein Typ, ein Initialisierungsblock, ein Feld, eine Methode, ein Konstruktor;
    /// davor Annotationen und Modifikatoren in beliebiger Folge, vor dem Rueckgabetyp `<T>`.
    void liesGliedJava(const Ort& ort, std::vector<AnweisungPtr>& code)
    {
        Sichtbarkeit sicht = Sichtbarkeit::Keine;
        bool statisch = false;
        bool abstrakt = false;
        for (;;)
        {
            ueberAttribute();
            if (namenhaft(m_p) && enthaelt(kModifikatorenJava, tok(m_p).text) &&
                !(tok(m_p).art == TokenArt::Bezeichner &&
                  (tok(m_p + 1).istOp("(") || tok(m_p + 1).istOp("=") || tok(m_p + 1).istOp(";"))))
            {
                const std::string_view w = tok(m_p).text;
                if (w == "public") sicht = Sichtbarkeit::Oeffentlich;
                if (w == "protected") sicht = Sichtbarkeit::Geschuetzt;
                if (w == "private") sicht = Sichtbarkeit::Privat;
                statisch = statisch || w == "static";
                abstrakt = abstrakt || w == "abstract";
                ++m_p;
                continue;
            }
            if (istName(m_p, "non") && tok(m_p + 1).istOp("-") && istName(m_p + 2, "sealed"))
            {
                m_p += 3;
                continue;
            }
            break;
        }
        const bool typWort = akt().istWort("class") || akt().istWort("interface") || akt().istWort("enum") ||
                             (akt().istOp("@") && tok(m_p + 1).istWort("interface")) ||
                             (istName(m_p, "record") && tok(m_p + 1).art == TokenArt::Bezeichner);
        if (typWort)
        {
            liesTyp(ort, code);
            return;
        }
        if (ort.ebene != Ebene::Typ)
        {
            // Java kennt keine Anweisungen ausserhalb einer Klasse
            melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "' outside a class");
            const std::size_t davor = m_p;
            m_p = suche(m_p, {";"});
            nimm(";");
            if (m_p == davor) ++m_p;
            return;
        }
        if (akt().istOp("{"))
        {
            liesInitialisierer(ort, statisch);
            return;
        }
        if (akt().istOp("<")) m_p = spitzeEnde(m_p);  // <T extends X> T f(…)
        ueberAttribute();
        const Token& bei = akt();
        const bool schnittstelle = m_aus.einheit.typen[ort.typ].art == TypArt::Schnittstelle;
        if (akt().art == TokenArt::Bezeichner && akt().text == ort.typName && !ort.typName.empty())
        {
            if (tok(m_p + 1).istOp("("))
            {
                ++m_p;
                liesMethode(ort, ort.typName, {}, sicht, statisch, false, false, bei);
                return;
            }
            if (tok(m_p + 1).istOp("{"))
            {
                // der kompakte Konstruktor eines record: `Punkt { … }`, mit den Komponenten
                ++m_p;
                Funktion f;
                f.name = ort.typName;
                f.zeile = bei.ort.zeile;
                for (const Methode& m : m_aus.einheit.typen[ort.typ].methoden)
                    if (m.name == ort.typName) f.parameter = m.parameter;
                const std::size_t platz = neueFunktion(std::move(f), ort);
                liesRumpfVon(platz, ort, false);
                return;
            }
        }
        const std::size_t typAnfang = m_p;
        const std::size_t typNach = typEnde(m_p);
        if (typNach == m_p || tok(typNach).art != TokenArt::Bezeichner)
        {
            melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
            const std::size_t davor = m_p;
            m_p = suche(m_p, {";"});
            nimm(";");
            if (m_p == davor) ++m_p;
            return;
        }
        const std::string typ = textVon(typAnfang, typNach);
        m_p = typNach;
        if (tok(m_p + 1).istOp("("))
        {
            const std::string name(tok(m_p++).text);
            liesMethode(ort, name, typ, sicht, statisch, abstrakt || schnittstelle, abstrakt, bei);
            return;
        }
        // Felder: Typ a = 1, b[] = {…};
        const std::size_t e = suche(m_p, {";"});
        for (const Bereich& s : teileSpitz(typNach, e))
        {
            Feld f;
            f.typ = typ;
            const std::size_t gleich = gleichIn(s.first, s.second);
            f.name = textVon(s.first, gleich);
            if (gleich < s.second) f.zusatz = "= " + funktionsText(gleich + 1, s.second, ort);
            f.sicht = sicht;
            f.zeile = tok(s.first).ort.zeile;
            m_aus.einheit.typen[ort.typ].felder.push_back(std::move(f));
        }
        m_p = e;
        nimm(";");
    }

    /// Java, nach dem Namen eines Typs: `extends A`, `implements B, C`, `permits D` bis zur `{`.
    /// extends ist eine Basis (auch die Schnittstellen, die eine Schnittstelle erweitert),
    /// implements eine Realisierung; permits wird ueberlesen.
    void liesKopfJava(Typ& typ)
    {
        while (!amEnde(m_p) && !akt().istOp("{") && !akt().istOp(";"))
        {
            const bool basis = akt().istWort("extends");
            const bool realisiert = akt().istWort("implements");
            if (!basis && !realisiert && !istName(m_p, "permits"))
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
                continue;
            }
            ++m_p;
            std::size_t e = m_p;
            while (!amEnde(e) && !tok(e).istOp("{") && !tok(e).istWort("extends") && !tok(e).istWort("implements") &&
                   !istName(e, "permits"))
                e = tok(e).istOp("<") ? std::max(spitzeEnde(e), e + 1) : e + 1;
            for (const Bereich& s : teileSpitz(m_p, e))
            {
                if (basis) typ.basen.push_back(textVon(s.first, s.second));
                if (realisiert) typ.realisiert.push_back(textVon(s.first, s.second));
            }
            m_p = e;
        }
    }

    /// Java: die Konstanten eines enum ab m_p (hinter der `{`) bis `;` oder `}` — `ROT`, `GRUEN(2)`,
    /// `BLAU { … }` (ein Rumpf wie eine anonyme Klasse: Methoden `BLAU.name`). Dahinter, nach `;`,
    /// die uebrigen Glieder.
    void liesKonstantenJava(const Ort& innen, std::size_t platz, std::vector<AnweisungPtr>& code)
    {
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            ueberAttribute();
            if (nimm(";"))
            {
                liesFolgeVonGliedern(innen, code, true);
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
                f.zusatz = funktionsText(m_p, zu, innen);  // Argumente als Wert: `(5000)`
                m_p = zu;
            }
            if (akt().istOp("{"))
            {
                std::vector<std::string> methoden;
                m_p = liesAnonymeKlasse(m_p, f.name, innen, methoden);
            }
            m_aus.einheit.typen[platz].felder.push_back(std::move(f));
        }
    }

    /// class, struct, interface, enum, record [class|struct]: ein Typ, mit seinen Gliedern.
    void liesTyp(const Ort& ort, std::vector<AnweisungPtr>& code)
    {
        const Token& t = akt();
        Typ typ;
        typ.zeile = t.ort.zeile;
        bool istStruct = false;
        if (m_java && akt().istOp("@"))  // @interface: ein Annotationstyp
        {
            m_p += 2;
            typ.art = TypArt::Schnittstelle;
        }
        else if (m_java && istName(m_p, "record"))  // die Komponenten sind Felder (festgelegt)
        {
            ++m_p;
            typ.art = TypArt::Struktur;
        }
        else if (istName(m_p, "record"))
        {
            ++m_p;
            typ.art = TypArt::Klasse;
            if (akt().istWort("struct")) istStruct = true;
            if (akt().istWort("class") || akt().istWort("struct")) ++m_p;
        }
        else
        {
            const std::string_view w = akt().text;
            typ.art = w == "interface" ? TypArt::Schnittstelle : w == "enum" ? TypArt::Aufzaehlung
                      : w == "struct"   ? TypArt::Struktur
                                        : TypArt::Klasse;
            istStruct = w == "struct";
            ++m_p;
        }
        if (akt().art != TokenArt::Bezeichner)
        {
            melde(Schwere::Fehler, akt(), "name of the type expected");
            return;
        }
        typ.name = std::string(ohneAt(tok(m_p++).text));
        typ.bereich = ort.ebene == Ebene::Funktion ? ort.funktion : ort.bereich;
        // `A` und `A<T>` sind in C# verschiedene Typen: ihre Glieder tragen die Typ-Parameter im
        // Bereich; Java erlaubt beide nicht nebeneinander — dort waeren sie nur laenger
        const std::size_t parameterAnfang = m_p;
        m_p = spitzeEnde(m_p);
        const std::string typParameter = m_java ? std::string() : textVon(parameterAnfang, m_p);
        // record Punkt(int X, int Y): ein Konstruktor mit Eigenschaften
        std::vector<Parameter> primaer;
        const bool mitPrimaer = akt().istOp("(");
        if (mitPrimaer)
        {
            const std::size_t zu = ueberKlammer(m_p);
            primaer = parameterIn(m_p, zu);
            m_p = zu;
        }
        if (m_java) liesKopfJava(typ);
        // : Basis, Schnittstelle …  — wo der erste eine Klasse ist, sind die uebrigen Schnittstellen
        if (!m_java && nimm(":"))
        {
            std::size_t e = m_p;
            while (!amEnde(e) && !tok(e).istOp("{") && !tok(e).istOp(";") && !istName(e, "where"))
            {
                if (tok(e).istOp("<"))
                    e = std::max(spitzeEnde(e), e + 1);
                else if (tok(e).istOp("("))
                    e = nachKlammer(e);
                else
                    ++e;
            }
            const std::vector<Bereich> liste = teileSpitz(m_p, e);
            for (std::size_t k = 0; k < liste.size(); ++k)
            {
                // record B(int X) : A(X) — die Argumente an die Basis gehoeren nicht zum Namen
                std::size_t bis = liste[k].second;
                for (std::size_t i = liste[k].first; i < bis; ++i)
                    if (tok(i).istOp("(")) bis = i;
                std::string name = textVon(liste[k].first, bis);
                if (typ.art == TypArt::Aufzaehlung) break;  // enum E : byte — der Grundtyp
                // eine Klasse erbt hoechstens von einer Klasse, und sie steht vorn; die uebrigen
                // (und alle eines struct) sind Schnittstellen; eine Schnittstelle erweitert Schnittstellen
                const bool schnittstelle = typ.art != TypArt::Schnittstelle && (istStruct || k > 0);
                (schnittstelle ? typ.realisiert : typ.basen).push_back(std::move(name));
            }
            m_p = e;
        }
        while (istName(m_p, "where"))  // Einschraenkungen der Typ-Parameter
        {
            ++m_p;
            while (!amEnde(m_p) && !akt().istOp("{") && !akt().istOp(";") && !istName(m_p, "where")) ++m_p;
        }
        const std::size_t platz = m_aus.einheit.typen.size();
        for (const Parameter& p : primaer)
        {
            Feld f;
            f.typ = p.typ;
            f.name = p.name;
            if (!m_java) f.zusatz = "{ get; init; }";
            f.zeile = typ.zeile;
            f.sicht = Sichtbarkeit::Oeffentlich;
            typ.felder.push_back(std::move(f));
        }
        if (mitPrimaer)
        {
            Methode m;
            m.name = typ.name;
            m.parameter = primaer;
            m.sicht = Sichtbarkeit::Oeffentlich;
            m.zeile = typ.zeile;
            typ.methoden.push_back(std::move(m));
        }
        const bool aufzaehlung = typ.art == TypArt::Aufzaehlung;
        if (ort.ebene == Ebene::Funktion) code.push_back(neu(AnweisungArt::Einfach, t, std::string(t.text) + " " + typ.name));
        Ort innen;
        innen.ebene = Ebene::Typ;
        innen.bereich = typ.vollerName() + typParameter;
        innen.typ = platz;
        innen.typName = typ.name;
        innen.funktion = ort.funktion;
        m_aus.einheit.typen.push_back(std::move(typ));
        if (nimm(";")) return;
        if (!nimm("{"))
        {
            melde(Schwere::Fehler, akt(), "'{' expected");
            return;
        }
        if (aufzaehlung && m_java)
            liesKonstantenJava(innen, platz, code);
        else if (aufzaehlung)
        {
            const std::size_t zu = nachKlammer(m_p - 1);
            const std::size_t innenEnde = tok(zu - 1).istOp("}") ? zu - 1 : zu;
            for (const Bereich& s : teile(m_p, innenEnde))
            {
                std::size_t a = s.first;
                while (a < s.second && tok(a).istOp("[")) a = nachKlammer(a);
                if (a >= s.second) continue;
                Feld f;
                const std::size_t gleich = gleichIn(a, s.second);
                f.name = textVon(a, gleich);
                if (gleich < s.second) f.zusatz = "= " + textVon(gleich + 1, s.second);
                f.zeile = tok(a).ort.zeile;
                m_aus.einheit.typen[platz].felder.push_back(std::move(f));
            }
            m_p = innenEnde;
        }
        else
            liesFolgeVonGliedern(innen, code, true);
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
        nimm(";");
    }

    /// `delegate R Name<T>(params);`: ein Alias auf die Signatur.
    void liesDelegat(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        const std::size_t e = suche(m_p, {";"});
        // der Name steht vor der Klammer der Parameter, oder vor ihren Typ-Parametern `<T>`
        std::size_t klammer = m_p;
        while (klammer < e && !tok(klammer).istOp("(")) ++klammer;
        std::size_t name = klammer > m_p ? klammer - 1 : m_p;
        if (tok(name).istOp(">"))
        {
            int tiefe = 0;
            for (; name > m_p; --name)
            {
                if (tok(name).istOp(">")) ++tiefe;
                else if (tok(name).istOp("<") && --tiefe == 0) break;
            }
            if (name > m_p) --name;
        }
        Alias a;
        a.name = std::string(tok(name).text);
        a.ziel = "delegate " + textVon(m_p, name) + " " + textVon(klammer, e);
        a.zeile = t.ort.zeile;
        (void)ort;
        m_aus.einheit.aliase.push_back(std::move(a));
        m_p = e;
        nimm(";");
    }

    /// Ein Mitglied eines Typs: Feld, Eigenschaft, Indexer, Ereignis, Methode, Konstruktor, Operator.
    void liesMitglied(const Ort& ort, Sichtbarkeit sicht, bool statisch, bool abstrakt, bool virtuell)
    {
        const Token& anfang = akt();
        const bool schnittstelle = m_aus.einheit.typen[ort.typ].art == TypArt::Schnittstelle;
        // Destruktor, Konstruktor
        if (akt().istOp("~") && tok(m_p + 1).art == TokenArt::Bezeichner && tok(m_p + 2).istOp("("))
        {
            ++m_p;
            liesMethode(ort, "~" + std::string(tok(m_p++).text), {}, sicht, statisch, false, false, anfang);
            return;
        }
        if (tok(m_p).art == TokenArt::Bezeichner && ohneAt(tok(m_p).text) == ort.typName && tok(m_p + 1).istOp("("))
        {
            ++m_p;
            liesMethode(ort, ort.typName, {}, sicht, statisch, false, false, anfang);
            return;
        }
        const bool ereignis = akt().istWort("event");
        if (ereignis) ++m_p;
        // Konversion: implicit/explicit operator T(…)
        if (akt().istWort("operator"))
        {
            ++m_p;
            const std::size_t e = typEnde(m_p);
            const std::string name = "operator " + textVon(m_p, e);
            m_p = e;
            liesMethode(ort, name, {}, sicht, true, false, false, anfang);
            return;
        }
        const std::size_t typAnfang = m_p;
        const std::size_t typNach = typEnde(m_p);
        if (typNach == m_p)
        {
            melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
            m_p = suche(m_p, {";"});
            nimm(";");
            return;
        }
        const std::string typ = textVon(typAnfang, typNach);
        m_p = typNach;
        if (akt().istWort("operator"))
        {
            ++m_p;
            std::string name = "operator ";
            while (!amEnde(m_p) && !akt().istOp("(")) name += std::string(tok(m_p++).text);
            liesMethode(ort, name, typ, sicht, true, false, false, anfang);
            return;
        }
        // der Name: `Name`, `I.Name` (ausdruecklich umgesetzt), `this` (Indexer)
        std::string name;
        if (akt().istWort("this"))
            name = "this";
        else if (akt().art == TokenArt::Bezeichner)
            name = std::string(akt().text);
        else
        {
            melde(Schwere::Fehler, akt(), "name expected");
            m_p = suche(m_p, {";"});
            nimm(";");
            return;
        }
        ++m_p;
        // ausdruecklich umgesetzt: `I.Name`, auch mit Typ-Argumenten `IEnumerable<T>.GetEnumerator`
        for (;;)
        {
            const std::size_t punkt = spitzeEnde(m_p);
            if (!tok(punkt).istOp(".") || !(tok(punkt + 1).art == TokenArt::Bezeichner || tok(punkt + 1).istWort("this")))
                break;
            name += textVon(m_p, punkt) + "." + std::string(tok(punkt + 1).text);
            m_p = punkt + 2;
        }
        if (akt().istOp("<") || akt().istOp("("))
        {
            m_p = spitzeEnde(m_p);
            if (akt().istOp("("))
            {
                liesMethode(ort, name, typ, sicht, statisch, abstrakt || schnittstelle, virtuell, anfang);
                return;
            }
        }
        if (name == "this" && akt().istOp("["))
        {
            const std::size_t zu = ueberKlammer(m_p);
            name = "this" + textVon(m_p, zu);
            m_p = zu;
        }
        // Eigenschaft: { get; set; } oder => ausdruck
        if (akt().istOp("{") || akt().istOp("=>"))
        {
            liesEigenschaft(ort, typ, name, sicht, statisch, ereignis, anfang);
            return;
        }
        // Felder: Typ a = 1, b;
        const std::size_t e = suche(m_p, {";"});
        for (const Bereich& s : teileSpitz(typNach, e))
        {
            Feld f;
            f.typ = ereignis ? "event " + typ : typ;
            const std::size_t gleich = gleichIn(s.first, s.second);
            f.name = textVon(s.first, gleich);
            if (gleich < s.second) f.zusatz = "= " + funktionsText(gleich + 1, s.second, ort);
            f.sicht = sicht;
            f.zeile = tok(s.first).ort.zeile;
            m_aus.einheit.typen[ort.typ].felder.push_back(std::move(f));
        }
        m_p = e;
        nimm(";");
    }

    /// m_p auf `{` oder `=>` einer Eigenschaft: ein Feld mit `{ get; set; }`; Accessoren mit Rumpf
    /// werden Funktionen `Name.get`, `Name.set`, `Name.init`, `Name.add`, `Name.remove`.
    void liesEigenschaft(const Ort& ort, const std::string& typ, const std::string& name, Sichtbarkeit sicht,
                         bool statisch, bool ereignis, const Token& bei)
    {
        Feld f;
        f.typ = ereignis ? "event " + typ : typ;
        f.name = name;
        f.sicht = sicht;
        f.zeile = bei.ort.zeile;
        (void)statisch;
        std::vector<std::string> zugriffe;
        if (akt().istOp("=>"))
        {
            // nur lesen: => ausdruck;
            Funktion g;
            g.name = name + ".get";
            g.zeile = akt().ort.zeile;
            g.rueckgabe = typ;
            const std::size_t platz = neueFunktion(std::move(g), ort);
            liesRumpfVon(platz, ort, true);
            f.zusatz = "{ get; }";
            m_aus.einheit.typen[ort.typ].felder.push_back(std::move(f));
            return;
        }
        ++m_p;  // {
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            const std::size_t davor = m_p;
            ueberAttribute();
            while (namenhaft(m_p) && enthaelt(kModifikatoren, tok(m_p).text)) ++m_p;  // private set
            if (akt().art == TokenArt::Bezeichner &&
                (akt().text == "get" || akt().text == "set" || akt().text == "init" || akt().text == "add" ||
                 akt().text == "remove"))
            {
                const Token& zugriff = akt();
                zugriffe.emplace_back(zugriff.text);
                ++m_p;
                if (akt().istOp("{") || akt().istOp("=>"))
                {
                    Funktion g;
                    g.name = name + "." + std::string(zugriff.text);
                    g.zeile = zugriff.ort.zeile;
                    if (zugriff.text == "get") g.rueckgabe = typ;
                    const std::size_t platz = neueFunktion(std::move(g), ort);
                    liesRumpfVon(platz, ort, zugriff.text == "get");
                }
                else
                    nimm(";");
            }
            if (m_p == davor)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
            }
        }
        nimm("}");
        f.zusatz = "{";
        for (const std::string& z : zugriffe) f.zusatz += " " + z + ";";
        f.zusatz += " }";
        if (akt().istOp("="))
        {
            const std::size_t e = suche(m_p, {";"});
            f.zusatz += " = " + funktionsText(m_p + 1, e, ort);
            m_p = e;
            nimm(";");
        }
        m_aus.einheit.typen[ort.typ].felder.push_back(std::move(f));
    }

    /// m_p auf `(` der Parameter: Methode und, mit Rumpf, Funktion.
    void liesMethode(const Ort& ort, const std::string& name, const std::string& rueckgabe, Sichtbarkeit sicht,
                     bool statisch, bool abstrakt, bool virtuell, const Token& bei)
    {
        Methode m;
        m.name = name;
        m.rueckgabe = rueckgabe;
        m.sicht = sicht;
        m.statisch = statisch;
        m.virtuell = virtuell;
        m.zeile = bei.ort.zeile;
        const std::size_t zu = ueberKlammer(m_p);
        m.parameter = parameterIn(m_p, zu);
        m_p = zu;
        if (m_java)
        {
            // `int f()[]` (Form von C), `throws A, B`, im Annotationstyp `default wert`: nicht in der Signatur
            while (akt().istOp("[") && tok(m_p + 1).istOp("]")) m_p += 2;
            if (akt().istWort("throws"))
                while (!amEnde(m_p) && !akt().istOp("{") && !akt().istOp(";"))
                    m_p = tok(m_p).istOp("<") ? std::max(spitzeEnde(m_p), m_p + 1) : m_p + 1;
            if (akt().istWort("default")) m_p = suche(m_p, {";"});  // auch `default {"a", "b"}`
        }
        // : base(…) / : this(…) eines Konstruktors, where-Einschraenkungen
        if (akt().istOp(":"))
        {
            ++m_p;
            while (!amEnde(m_p) && !akt().istOp("{") && !akt().istOp("=>") && !akt().istOp(";"))
                m_p = oeffnet(akt()) ? nachKlammer(m_p) : m_p + 1;
        }
        while (istName(m_p, "where"))
            while (!amEnde(m_p) && !akt().istOp("{") && !akt().istOp("=>") && !akt().istOp(";")) ++m_p;
        m.abstrakt = abstrakt && !akt().istOp("{") && !akt().istOp("=>");
        m_aus.einheit.typen[ort.typ].methoden.push_back(m);
        if (!akt().istOp("{") && !akt().istOp("=>"))
        {
            nimm(";");
            return;
        }
        Funktion f;
        f.name = name;
        f.rueckgabe = rueckgabe;
        f.parameter = m.parameter;
        f.zeile = m.zeile;
        const std::size_t platz = neueFunktion(std::move(f), ort);
        liesRumpfVon(platz, ort, !rueckgabe.empty() && rueckgabe != "void");
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

    AnweisungPtr liesRumpf(const Ort& ort)
    {
        const Token& t = akt();
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

    /// Beginnt bei i eine lokale Funktion `[static] [async] Typ Name<T>(…) {` bzw. `=>`?
    [[nodiscard]] bool istLokaleFunktion(std::size_t i) const
    {
        while (tok(i).istWort("static") || istName(i, "async") || tok(i).istWort("unsafe") || tok(i).istWort("extern"))
            ++i;
        const std::size_t nach = typEnde(i);
        if (nach == i || tok(nach).art != TokenArt::Bezeichner) return false;
        std::size_t k = spitzeEnde(nach + 1);
        if (!tok(k).istOp("(")) return false;
        k = nachKlammer(k);
        return tok(k).istOp("{") || tok(k).istOp("=>") || istName(k, "where");
    }

    /// Beginnt bei i eine Deklaration `Typ name` (dann ist `using` davor ein using-Anweisung)?
    [[nodiscard]] bool istLokaleDeklaration(std::size_t i) const
    {
        const std::size_t nach = typEnde(i);
        return nach > i && tok(nach).art == TokenArt::Bezeichner && (tok(nach + 1).istOp("=") || tok(nach + 1).istOp(";"));
    }

    /// i auf `(` eines for: Java `for (T x : xs)` — ein `:` ausserhalb von Klammern, kein `;`.
    [[nodiscard]] bool istJedes(std::size_t i) const
    {
        const std::size_t zu = nachKlammer(i);
        int fragen = 0;  // ein `:` nach `?` gehoert zu `?:`
        for (std::size_t j = i + 1; j + 1 < zu; ++j)
        {
            if (oeffnet(tok(j)))
            {
                j = nachKlammer(j) - 1;
                continue;
            }
            if (tok(j).istOp("<") && spitzeEnde(j) > j)  // List<? extends T>: kein ?:
            {
                j = spitzeEnde(j) - 1;
                continue;
            }
            if (tok(j).istOp(";")) return false;
            if (tok(j).istOp("?")) ++fragen;
            if (tok(j).istOp(":") && fragen-- == 0) return true;
        }
        return false;
    }

    void liesAnweisung(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        if (nimm(";")) return;
        if (t.istOp("{"))
        {
            aus.push_back(liesBlock(ort));
            return;
        }
        if (t.istOp(m_java ? "@" : "["))  // Attribute vor einer lokalen Funktion, Annotationen vor einer Deklaration
        {
            ueberAttribute();
            return;
        }
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
            aus.push_back(std::move(a));
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
            nimm(";");
            aus.push_back(std::move(a));
            return;
        }
        if (t.istWort("for"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Fuer, t);
            if (m_java && akt().istOp("(") && istJedes(m_p))
            {
                // for (T x : xs): je Element (wie range-for)
                a->art = AnweisungArt::FuerJedes;
                a->text = liesBedingung(ort);
            }
            else if (akt().istOp("("))
            {
                const std::size_t zu = ueberKlammer(m_p);
                const std::size_t ende = tok(zu - 1).istOp(")") ? zu - 1 : zu;
                const std::size_t s1 = std::min(suche(m_p + 1, {";"}), ende);
                const std::size_t s2 = std::min(suche(s1 + 1, {";"}), ende);
                a->init = funktionsText(m_p + 1, s1, ort);
                a->text = funktionsText(s1 + 1, s2, ort);
                if (s2 < ende) a->schritt = funktionsText(s2 + 1, ende, ort);
                m_p = zu;
            }
            else
                melde(Schwere::Fehler, akt(), "'(' expected");
            a->kinder.push_back(liesRumpf(ort));
            aus.push_back(std::move(a));
            return;
        }
        if (t.istWort("foreach") || (istName(m_p, "await") && tok(m_p + 1).istWort("foreach")))
        {
            const bool warten = istName(m_p, "await");
            m_p += warten ? 2 : 1;
            AnweisungPtr a = neu(AnweisungArt::FuerJedes, t);
            a->text = liesBedingung(ort);
            a->kopf = std::string(warten ? "await foreach (" : "foreach (") + a->text + ")";
            a->kinder.push_back(liesRumpf(ort));
            aus.push_back(std::move(a));
            return;
        }
        if (t.istWort("switch"))
        {
            aus.push_back(m_java ? liesAuswahlJava(ort) : liesAuswahl(ort));
            return;
        }
        if (m_java && t.istWort("synchronized") && tok(m_p + 1).istOp("("))
        {
            // ein Block unter einer Sperre: wie lock in C#
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Mit, t);
            a->text = liesBedingung(ort);
            a->kopf = "synchronized (" + a->text + ")";
            a->kinder.push_back(liesRumpf(ort));
            aus.push_back(std::move(a));
            return;
        }
        if (t.istWort("try"))
        {
            aus.push_back(liesVersuch(ort));
            return;
        }
        // using (…) / lock (…) / fixed (…): ein Block unter einem Gegenstand
        if ((t.istWort("using") || t.istWort("lock") || t.istWort("fixed")) && tok(m_p + 1).istOp("("))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Mit, t);
            a->text = liesBedingung(ort);
            a->kopf = std::string(t.text) + " (" + a->text + ")";
            a->kinder.push_back(liesRumpf(ort));
            aus.push_back(std::move(a));
            return;
        }
        if ((t.istWort("checked") || t.istWort("unchecked") || t.istWort("unsafe")) && tok(m_p + 1).istOp("{"))
        {
            ++m_p;
            aus.push_back(liesBlock(ort));
            return;
        }
        if (t.istWort("return") || t.istWort("throw"))
        {
            ++m_p;
            const std::size_t e = suche(m_p, {";"});
            if (t.istWort("return") && liesSwitchAusdruck(ort, m_p, e, "return", aus))
            {
                m_p = e;
                nimm(";");
                return;
            }
            aus.push_back(neu(t.istWort("return") ? AnweisungArt::Ruecksprung : AnweisungArt::Wurf, t,
                              funktionsText(m_p, e, ort)));
            m_p = e;
            nimm(";");
            return;
        }
        if (m_java && istName(m_p, "yield") && !m_yieldVorn.empty() && !tok(m_p + 1).istOp("(") &&
            !tok(m_p + 1).istOp("=") && !tok(m_p + 1).istOp("."))
        {
            // Java: der Wert eines Arms eines switch-Ausdrucks — `return wert` bzw. `x = wert`
            ++m_p;
            const std::size_t e = suche(m_p, {";"});
            if (m_yieldVorn == "return")
                aus.push_back(neu(AnweisungArt::Ruecksprung, t, funktionsText(m_p, e, ort)));
            else
                aus.push_back(neu(AnweisungArt::Einfach, t, m_yieldVorn + " " + funktionsText(m_p, e, ort)));
            m_p = e;
            nimm(";");
            return;
        }
        if (istName(m_p, "yield") && (tok(m_p + 1).istWort("return") || tok(m_p + 1).istWort("break")))
        {
            const bool ende = tok(m_p + 1).istWort("break");
            m_p += 2;
            const std::size_t e = suche(m_p, {";"});
            if (ende)
            {
                AnweisungPtr a = neu(AnweisungArt::Ruecksprung, t);
                a->kopf = "yield break";
                aus.push_back(std::move(a));
            }
            else
                aus.push_back(neu(AnweisungArt::Einfach, t, "yield return " + funktionsText(m_p, e, ort)));
            m_p = e;
            nimm(";");
            return;
        }
        if (t.istWort("break") || t.istWort("continue"))
        {
            ++m_p;
            std::string marke;  // Java: break marke — die markierte Schleife (Entscheid H2)
            if (m_java && akt().art == TokenArt::Bezeichner) marke = std::string(tok(m_p++).text);
            nimm(";");
            aus.push_back(neu(t.istWort("break") ? AnweisungArt::Abbruch : AnweisungArt::Fortsetzung, t, std::move(marke)));
            return;
        }
        if (t.istWort("goto"))
        {
            ++m_p;
            const std::size_t e = suche(m_p, {";"});
            if (akt().art == TokenArt::Bezeichner && e == m_p + 1)
                aus.push_back(neu(AnweisungArt::Sprung, t, std::string(akt().text)));
            else
            {
                melde(Schwere::Hinweis, t, "goto case / goto default: not followed in the flowchart");
                aus.push_back(neu(AnweisungArt::Einfach, t, "goto " + textVon(m_p, e)));
            }
            m_p = e;
            nimm(";");
            return;
        }
        if (t.art == TokenArt::Bezeichner && tok(m_p + 1).istOp(":") && !tok(m_p + 2).istOp(":"))
        {
            m_p += 2;
            // Java: vor einer Schleife, einem switch, einem Block ist es deren Marke (Ziel von break
            // marke); C# kennt das nicht — dort ist eine Marke das Ziel eines goto
            const Token& n = akt();
            if (m_java && (n.istWort("for") || n.istWort("while") || n.istWort("do") || n.istWort("switch") || n.istOp("{")))
            {
                std::vector<AnweisungPtr> eine;
                liesAnweisung(ort, eine);
                if (eine.size() == 1)
                {
                    eine.front()->marke = std::string(t.text);
                    aus.push_back(std::move(eine.front()));
                    return;
                }
                for (AnweisungPtr& a : eine) aus.push_back(std::move(a));
                return;
            }
            aus.push_back(neu(AnweisungArt::Marke, t, std::string(t.text)));
            return;
        }
        if (!m_java && istLokaleFunktion(m_p))  // Java kennt keine lokalen Funktionen
        {
            liesLokaleFunktion(ort, aus);
            return;
        }
        if ((t.istWort("class") || t.istWort("struct") || t.istWort("interface") || t.istWort("enum") ||
             istName(m_p, "record")) && tok(m_p + 1).art == TokenArt::Bezeichner)
        {
            liesTyp(ort, aus);
            return;
        }
        if (m_java && (t.istWort("final") || t.istWort("abstract") || t.istWort("static")) &&
            (tok(m_p + 1).istWort("class") || tok(m_p + 1).istWort("interface") || istName(m_p + 1, "record")))
        {
            ++m_p;  // eine lokale Klasse mit Modifikator
            liesTyp(ort, aus);
            return;
        }
        // eine Deklaration oder ein Ausdruck
        const std::size_t e = suche(m_p, {";"});
        if (e == m_p)
        {
            if (!amEnde(m_p) && !akt().istOp("}"))
            {
                melde(Schwere::Fehler, t, "unexpected '" + std::string(t.text) + "'");
                ++m_p;
            }
            return;
        }
        const std::size_t gleich = gleichIn(m_p, e);
        if (gleich < e && liesSwitchAusdruck(ort, gleich + 1, e, textVon(m_p, gleich + 1), aus))
        {
            m_p = e;
            nimm(";");
            return;
        }
        aus.push_back(neu(AnweisungArt::Einfach, t, funktionsText(m_p, e, ort)));
        m_p = e;
        nimm(";");
    }

    void liesLokaleFunktion(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        while (tok(m_p).istWort("static") || istName(m_p, "async") || tok(m_p).istWort("unsafe") || tok(m_p).istWort("extern"))
            ++m_p;
        const std::size_t nach = typEnde(m_p);
        Funktion f;
        f.rueckgabe = textVon(m_p, nach);
        f.name = std::string(tok(nach).text);
        f.zeile = t.ort.zeile;
        m_p = spitzeEnde(nach + 1);
        const std::size_t zu = ueberKlammer(m_p);
        f.parameter = parameterIn(m_p, zu);
        m_p = zu;
        while (istName(m_p, "where"))
            while (!amEnde(m_p) && !akt().istOp("{") && !akt().istOp("=>")) ++m_p;
        aus.push_back(neu(AnweisungArt::Einfach, t, modell::deklaration(f.rueckgabe, modell::signatur(f.name, f.parameter))));
        const bool liefert = f.rueckgabe != "void";
        const std::size_t platz = neueFunktion(std::move(f), ort);
        aus.back()->innere.push_back(m_aus.einheit.funktionen[platz].vollerName());  // vor dem Rumpf: der liest eigene
        liesRumpfVon(platz, ort, liefert);
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
                rumpf->kinder.push_back(neu(AnweisungArt::Fall, s, textVon(m_p, e)));
                m_p = e;
                if (!nimm(":")) melde(Schwere::Fehler, akt(), "':' after case is missing");
            }
            else if (s.istWort("default") && tok(m_p + 1).istOp(":"))
            {
                m_p += 2;
                rumpf->kinder.push_back(neu(AnweisungArt::Fall, s));
            }
            else
            {
                const std::size_t vorher = rumpf->kinder.size();
                liesAnweisung(ort, rumpf->kinder);
                if (rumpf->kinder.size() > vorher) rumpf->kinder[vorher]->innere = std::move(m_innere);
                m_innere.clear();
            }
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
        // Java: try (Ressourcen) — sie werden am Ende geschlossen; catch und finally duerfen fehlen
        const bool mitRessourcen = m_java && akt().istOp("(");
        if (mitRessourcen) a->kopf = "try (" + liesBedingung(ort) + ")";
        a->kinder.push_back(akt().istOp("{") ? liesBlock(ort) : liesRumpf(ort));
        while (akt().istWort("catch"))
        {
            const Token& bei = akt();
            ++m_p;
            AnweisungPtr fang = neu(AnweisungArt::Fang, bei);
            std::string kopf = "catch";
            if (akt().istOp("("))
            {
                const std::size_t zu = ueberKlammer(m_p);
                fang->text = textVon(m_p + 1, tok(zu - 1).istOp(")") ? zu - 1 : zu);
                kopf += " (" + fang->text + ")";
                m_p = zu;
            }
            if (istName(m_p, "when"))
            {
                ++m_p;
                const std::size_t zu = akt().istOp("(") ? ueberKlammer(m_p) : m_p;
                kopf += " when " + textVon(m_p, zu);
                m_p = zu;
            }
            fang->kopf = kopf;
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
        if (a->kinder.size() == 1 && !mitRessourcen)
            melde(Schwere::Fehler, akt(), "'catch' or 'finally' after try is missing");
        return a;
    }

    /// Java: `switch (x) { case A, B: … }` mit Durchfallen, oder `case A, B -> anweisung;` ohne;
    /// `default` als Fall ohne Wert.
    AnweisungPtr liesAuswahlJava(const Ort& ort)
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
            const std::size_t davor = m_p;
            if (const std::optional<bool> pfeil = liesFallJava(rumpf->kinder))
            {
                a->ohneDurchfall = a->ohneDurchfall || *pfeil;
                if (*pfeil)
                {
                    // ein Arm: eine Anweisung, ein Block oder throw; danach der naechste Fall
                    const std::size_t vorher = rumpf->kinder.size();
                    liesAnweisung(ort, rumpf->kinder);
                    if (rumpf->kinder.size() > vorher) rumpf->kinder[vorher]->innere = std::move(m_innere);
                    m_innere.clear();
                }
            }
            else
            {
                const std::size_t vorher = rumpf->kinder.size();
                liesAnweisung(ort, rumpf->kinder);
                if (rumpf->kinder.size() > vorher) rumpf->kinder[vorher]->innere = std::move(m_innere);
                m_innere.clear();
            }
            if (m_p == davor) ++m_p;
        }
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
        a->kinder.push_back(std::move(rumpf));
        return a;
    }

    /// Java: steht m_p auf `case …` oder `default`, kommt ein Fall nach `faelle`: true = mit `->`,
    /// false = mit `:`; nullopt = kein Fall (m_p bleibt).
    std::optional<bool> liesFallJava(std::vector<AnweisungPtr>& faelle)
    {
        const Token& s = akt();
        if (!s.istWort("case") && !(s.istWort("default") && (tok(m_p + 1).istOp(":") || tok(m_p + 1).istOp("->"))))
            return std::nullopt;
        ++m_p;
        std::size_t e = m_p;
        while (!amEnde(e) && !tok(e).istOp(":") && !tok(e).istOp("->") && !tok(e).istOp(";") && !tok(e).istOp("}"))
            e = oeffnet(tok(e)) ? nachKlammer(e) : e + 1;
        faelle.push_back(neu(AnweisungArt::Fall, s, s.istWort("case") ? textVon(m_p, e) : std::string()));
        m_p = e;
        if (nimm("->")) return true;
        if (!nimm(":")) melde(Schwere::Fehler, akt(), "':' or '->' after case is missing");
        return false;
    }

    /// Java: [a, e) ist `switch (x) { case A -> wert; … }` (ganz, nur Arme mit `->`): eine Auswahl ohne
    /// Durchfallen, je Arm die Anweisung `vorn wert` (`return 1`, `int y = 1`); ein Block darin endet
    /// mit `yield wert`, das ebenso wird; throw bleibt ein Wurf. false = nicht.
    bool liesSwitchAusdruckJava(const Ort& ort, std::size_t a, std::size_t e, const std::string& vorn,
                                std::vector<AnweisungPtr>& aus)
    {
        if (!tok(a).istWort("switch") || !tok(a + 1).istOp("(")) return false;
        const std::size_t auf = nachKlammer(a + 1);
        if (!tok(auf).istOp("{") || nachKlammer(auf) != e) return false;
        // nur die Form mit Pfeilen: mit `:` liefe ein Fall in den naechsten
        for (std::size_t i = auf + 1; i + 1 < e; ++i)
        {
            if (oeffnet(tok(i)))
            {
                i = nachKlammer(i) - 1;
                continue;
            }
            if (tok(i).istWort("case") || tok(i).istWort("default"))
            {
                std::size_t k = i + 1;
                while (k + 1 < e && !tok(k).istOp(":") && !tok(k).istOp("->"))
                    k = oeffnet(tok(k)) ? nachKlammer(k) : k + 1;
                if (!tok(k).istOp("->")) return false;
                i = k;
            }
        }
        const std::size_t gemerkt = m_p;
        m_p = a;
        const Token& t = akt();
        ++m_p;
        AnweisungPtr auswahl = neu(AnweisungArt::Auswahl, t);
        auswahl->text = liesBedingung(ort);
        auswahl->ohneDurchfall = true;
        AnweisungPtr rumpf = neu(AnweisungArt::Block, akt());
        ++m_p;  // {
        const std::string gemerktYield = std::exchange(m_yieldVorn, vorn);
        while (!amEnde(m_p) && m_p + 1 < e)
        {
            const std::size_t davor = m_p;
            if (liesFallJava(rumpf->kinder))
            {
                const Token& w = akt();
                if (w.istOp("{"))
                    rumpf->kinder.push_back(liesBlock(ort));
                else
                {
                    const std::size_t ende = suche(m_p, {";"});
                    if (w.istWort("throw"))
                        rumpf->kinder.push_back(neu(AnweisungArt::Wurf, w, funktionsText(m_p + 1, ende, ort)));
                    else if (vorn == "return")
                        rumpf->kinder.push_back(neu(AnweisungArt::Ruecksprung, w, funktionsText(m_p, ende, ort)));
                    else
                        rumpf->kinder.push_back(neu(AnweisungArt::Einfach, w, vorn + " " + funktionsText(m_p, ende, ort)));
                    m_p = ende;
                    nimm(";");
                }
                rumpf->kinder.back()->innere = std::move(m_innere);
                m_innere.clear();
            }
            if (m_p == davor) ++m_p;
        }
        m_yieldVorn = gemerktYield;
        m_p = gemerkt;
        auswahl->kinder.push_back(std::move(rumpf));
        aus.push_back(std::move(auswahl));
        return true;
    }

    /// [a, e) ist `subjekt switch { muster => wert, … }` (ganz): eine Auswahl ohne Durchfallen, je Arm
    /// ein Fall und die Anweisung `vorn wert` (`return 1`, `var y = 1`; `throw x` als Wurf). false = nicht.
    bool liesSwitchAusdruck(const Ort& ort, std::size_t a, std::size_t e, const std::string& vorn,
                            std::vector<AnweisungPtr>& aus)
    {
        if (m_java) return liesSwitchAusdruckJava(ort, a, e, vorn, aus);
        std::size_t sw = a;
        while (sw < e && !(tok(sw).istWort("switch") && tok(sw + 1).istOp("{")))
        {
            if (oeffnet(tok(sw)))
            {
                sw = nachKlammer(sw);
                continue;
            }
            ++sw;
        }
        if (sw >= e || sw == a || nachKlammer(sw + 1) != e) return false;
        const Token& t = tok(a);
        AnweisungPtr auswahl = neu(AnweisungArt::Auswahl, t, textVon(a, sw));
        auswahl->kopf = auswahl->text + " switch";
        auswahl->ohneDurchfall = true;
        AnweisungPtr rumpf = neu(AnweisungArt::Block, t);
        const std::size_t zu = e - 1;  // die }
        for (const Bereich& arm : teile(sw + 2, zu))
        {
            std::size_t pfeil = arm.first;
            while (pfeil < arm.second && !tok(pfeil).istOp("=>"))
                pfeil = oeffnet(tok(pfeil)) ? nachKlammer(pfeil) : pfeil + 1;
            if (pfeil >= arm.second) continue;
            const std::string muster = textVon(arm.first, pfeil);
            rumpf->kinder.push_back(neu(AnweisungArt::Fall, tok(arm.first), muster == "_" ? std::string() : muster));
            const Token& wertTok = tok(pfeil + 1);
            if (wertTok.istWort("throw"))
                rumpf->kinder.push_back(neu(AnweisungArt::Wurf, wertTok, funktionsText(pfeil + 2, arm.second, ort)));
            else if (vorn == "return")
                rumpf->kinder.push_back(neu(AnweisungArt::Ruecksprung, wertTok, funktionsText(pfeil + 1, arm.second, ort)));
            else
                rumpf->kinder.push_back(neu(AnweisungArt::Einfach, wertTok, vorn + " " + funktionsText(pfeil + 1, arm.second, ort)));
        }
        auswahl->kinder.push_back(std::move(rumpf));
        aus.push_back(std::move(auswahl));
        return true;
    }

    /// Ein Ausdruck als Rumpf (`=> ausdruck`): ein return oder eine Anweisung; ein switch-Ausdruck verzweigt.
    void liesAusdruckAnweisung(const Ort& ort, std::size_t a, std::size_t e, const std::string& vorn,
                               std::vector<AnweisungPtr>& aus)
    {
        if (vorn == "return" && liesSwitchAusdruck(ort, a, e, vorn, aus)) return;
        if (tok(a).istWort("throw"))
        {
            aus.push_back(neu(AnweisungArt::Wurf, tok(a), funktionsText(a + 1, e, ort)));
            return;
        }
        aus.push_back(neu(vorn == "return" ? AnweisungArt::Ruecksprung : AnweisungArt::Einfach, tok(a), funktionsText(a, e, ort)));
    }

    std::vector<std::string> m_innere;  ///< die Funktionen, die in der laufenden Anweisung definiert wurden
    int m_letzteZeile = 0;
    bool m_java = false;
    /// Java, im Block eines Arms eines switch-Ausdrucks: was ein `yield wert` daraus macht (`return`,
    /// `x =`, `int x =`); leer = kein switch-Ausdruck.
    std::string m_yieldVorn;
    std::set<std::string> m_anonyme;  ///< die vergebenen `<anonymous:…>`, je mit Bereich
};

}  // namespace

namespace csjava
{

const lexer::Sprachtabelle& tabelle(Dialekt dialekt)
{
    return dialekt.java ? kTabelleJava : kTabelle;
}

Ergebnis lies(std::string_view text, std::string_view einheitName, const LeseOptionen& optionen, Dialekt dialekt)
{
    Ergebnis ergebnis;
    ergebnis.einheit.name = std::string(einheitName);
    if (dialekt.java || optionen.alleZweige)
    {
        // Java hat keine bedingte Uebersetzung
        CsLeser(text, ergebnis, dialekt).liesEinheit();
        ergaenzeLogik(ergebnis.einheit, tabelle(dialekt), kRegeln);
        return ergebnis;
    }
    // erst entscheiden, welche Zweige gelten; die Token des Lesers zeigen in diesen Text
    const std::string geltend =
        werteBedingteAus(text, kTabelle, optionen.definitionen, false, ergebnis.meldungen, ergebnis.unbekannteNamen,
                         nullptr, &ergebnis.schalter, &ergebnis.weichen, true);
    CsLeser(geltend, ergebnis, dialekt).liesEinheit();
    ergaenzeLogik(ergebnis.einheit, kTabelle, kRegeln);
    if ((optionen.varianten || optionen.bereiche) && !ergebnis.weichen.empty())
        ergebnis.meldungen.push_back({quelle::Schwere::Hinweis, {0, ergebnis.weichen.front().zeile, 1},
                                      "#if variants and frames are not shown for C# yet - evaluated"});
    return ergebnis;
}

}  // namespace csjava

}  // namespace c2d::sprache
