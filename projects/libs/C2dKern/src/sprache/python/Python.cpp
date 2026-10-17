/**
 * @file Python.cpp
 * @brief Python: die Sprachtabelle und der Leser in rekursivem Abstieg
 *
 * Der Lexer liefert die Gliederung schon als Token (Zeilenende, Einzug, Auszug):
 * eine Anweisung endet am Zeilenende oder an `;`, ein Block ist, was zwischen
 * Einzug und Auszug steht — oder, hinter dem `:` eines Kopfs, der Rest der Zeile.
 * Ausdruecke bleiben Text. `match`, `case` und `_` sind weiche Schluesselwoerter:
 * der Leser erkennt sie an ihrer Stelle, sonst sind es Namen.
 */
#include "c2d/sprache/Sprache.hpp"

#include "../Leser.hpp"
#include "../Logik.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace c2d::sprache
{

namespace
{

using namespace c2d::modell;
using quelle::Schwere;

// Python 3.12; `match`, `case`, `type` und `_` stehen nicht darin (weich, siehe oben).
constexpr std::string_view kSchluesselwoerter[] = {
    "False", "None",   "True",    "and",      "as",   "assert", "async", "await",  "break",
    "class", "continue", "def",   "del",      "elif", "else",   "except", "finally", "for",
    "from",  "global", "if",      "import",   "in",   "is",     "lambda", "nonlocal", "not",
    "or",    "pass",   "raise",   "return",   "try",  "while",  "with",   "yield",
};

constexpr std::string_view kOperatoren[] = {
    "**=", "//=", ">>=", "<<=", "...", "->", ":=", "**", "//", "<<", ">>", "<=", ">=", "==", "!=",
    "+=",  "-=",  "*=",  "/=",  "%=",  "&=", "|=", "^=", "@=", "(",  ")",  "[",  "]",  "{",  "}",
    ",",   ":",   ";",   ".",   "~",   "+",  "-",  "*",  "/",  "%",  "&",  "|",  "^",  "<",  ">",
    "=",   "@",
};

constexpr std::string_view kVorsaetze[] = {"r", "u", "b", "f", "t", "br", "rb", "fr", "rf", "tr", "rt"};

constexpr std::string_view kEndungen[] = {"py", "pyi", "pyw"};

constexpr lexer::Sprachtabelle kTabelle{
    .schluesselwoerter = kSchluesselwoerter,
    .operatoren = kOperatoren,
    .zeilenKommentar = "#",
    .zeichenkettenBegrenzer = "\"'",
    .zeichenBegrenzer = "",
    .ziffernTrenner = '_',
    .zeichenkettenVorsaetze = kVorsaetze,
    .dreifachZeichenketten = true,
    .zeilenEnde = true,
    .einrueckung = true,
};

constexpr std::string_view kZuweisungen[] = {"=",  ":=", "+=", "-=",  "*=",  "/=", "//=", "%=",
                                             "**=", "@=", "&=", "|=", "^=", ">>=", "<<="};

constexpr Ausdrucksregeln kRegeln{
    .wahl = Ausdrucksregeln::Wahl::NachgestelltesWenn,
    .nichtUmfasstVergleich = true,
    .zuweisungen = kZuweisungen,
    .zeiger = false,
};

/// Basen, die eine Klasse zur Aufzaehlung machen (auch mit Modul davor: `enum.Enum`).
constexpr std::string_view kAufzaehlungen[] = {"Enum", "IntEnum", "StrEnum", "Flag", "IntFlag"};

/// Der Name, unter dem Python den Code auf oberster Ebene fuehrt.
constexpr std::string_view kModul = "<module>";

class PythonLeser final : public Leser
{
public:
    PythonLeser(std::string_view text, Ergebnis& aus) : Leser(text, kTabelle, aus) {}

    void liesModul()
    {
        const Ort modul;
        std::vector<AnweisungPtr> code;
        liesFolge(modul, code, false);
        if (!m_ablauf || code.empty()) return;
        // was auf oberster Ebene ablaeuft, als eigene Funktion — wie Python sie nennt
        Funktion f;
        f.name = std::string(kModul);
        f.zeile = code.front()->zeile;
        f.bisZeile = m_letzteZeile;
        f.rumpf = std::make_unique<Anweisung>();
        f.rumpf->art = AnweisungArt::Block;
        f.rumpf->zeile = f.zeile;
        f.rumpf->kinder = std::move(code);
        m_aus.einheit.funktionen.push_back(std::move(f));
    }

private:
    enum class Ebene
    {
        Modul,
        Klasse,
        Funktion,
    };

    /// Wo gelesen wird.
    struct Ort
    {
        Ebene ebene = Ebene::Modul;
        std::string klasse;            ///< voller Name der Klasse, in der (oder in deren Methode) gelesen wird
        std::size_t typ = kKeiner;     ///< ihr Platz unter Einheit::typen
        std::string funktion;          ///< voller Name der Funktion, in der gelesen wird
        std::string selbst;            ///< in einer Methode: der Name ihres ersten Parameters (`self`)
    };

    // --- Token ---------------------------------------------------------------

    [[nodiscard]] bool zeilenEnde(std::size_t i) const
    {
        const TokenArt a = tok(i).art;
        return a == TokenArt::Zeilenende || a == TokenArt::Ende || a == TokenArt::Einzug || a == TokenArt::Auszug;
    }

    [[nodiscard]] bool istName(std::size_t i, std::string_view name) const
    {
        return tok(i).art == TokenArt::Bezeichner && tok(i).text == name;
    }

    /// i steht auf einer oeffnenden Klammer: dahinter; fehlt die schliessende, eine Meldung.
    std::size_t ueberKlammer(std::size_t i)
    {
        const std::size_t zu = nachKlammer(i);
        if (!schliesst(tok(zu - 1)) || zu - 1 == i)
            melde(Schwere::Fehler, tok(i), "'" + std::string(tok(i).text) + "' is not closed");
        return zu;
    }

    /// Ab i bis zum Ende der einfachen Anweisung: `;` oder Zeilenende (Klammern tragen keins).
    std::size_t anweisungsEnde(std::size_t i)
    {
        while (!zeilenEnde(i))
        {
            if (oeffnet(tok(i)))
            {
                i = ueberKlammer(i);
                continue;
            }
            if (tok(i).istOp(";")) return i;
            ++i;
        }
        return i;
    }

    /// Der Doppelpunkt, der den Kopf ab i schliesst: auf oberster Ebene, nicht der eines lambda.
    /// Fehlt er, das Zeilenende.
    std::size_t kopfEnde(std::size_t i)
    {
        int lambdas = 0;
        while (!zeilenEnde(i))
        {
            const Token& t = tok(i);
            if (oeffnet(t))
            {
                i = ueberKlammer(i);
                continue;
            }
            if (t.istWort("lambda")) ++lambdas;
            else if (t.istOp(":"))
            {
                if (lambdas == 0) return i;
                --lambdas;
            }
            ++i;
        }
        return i;
    }

    /// Hinter den Kopf, der bei `ende` endet; fehlt der Doppelpunkt, eine Meldung.
    void nimmKopf(std::size_t ende)
    {
        m_p = ende;
        if (!nimm(":")) melde(Schwere::Fehler, akt(), "':' expected");
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

    /// Die Zeile des letzten Tokens mit Inhalt vor i (Einzug und Auszug stehen schon in der naechsten).
    [[nodiscard]] int zeileVor(std::size_t i) const
    {
        while (i > 0 && (tok(i - 1).art == TokenArt::Einzug || tok(i - 1).art == TokenArt::Auszug)) --i;
        return i > 0 ? tok(i - 1).ort.zeile : 0;
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

    // --- Folgen und Bloecke ---------------------------------------------------

    /// Steht ab m_p nur eine Zeichenkette (auch aneinandergereihte) bis zum Zeilenende — ein Docstring?
    [[nodiscard]] bool nurZeichenkette() const
    {
        std::size_t i = m_p;
        while (tok(i).art == TokenArt::Zeichenkette) ++i;
        return i > m_p && zeilenEnde(i);
    }

    /// Anweisungen bis zum Auszug, der die Folge schliesst (er wird genommen), oder bis zum Ende.
    void liesFolge(const Ort& ort, std::vector<AnweisungPtr>& aus, bool eingerueckt)
    {
        int zuTief = 0;  // Einzuege, die keinen Block oeffnen
        bool erste = true;
        while (!amEnde(m_p))
        {
            const Token& t = akt();
            if (t.art == TokenArt::Zeilenende)
            {
                ++m_p;
                continue;
            }
            if (t.art == TokenArt::Einzug)
            {
                melde(Schwere::Fehler, t, "unexpected indent");
                ++zuTief;
                ++m_p;
                continue;
            }
            if (t.art == TokenArt::Auszug)
            {
                ++m_p;
                if (zuTief > 0)
                {
                    --zuTief;
                    continue;
                }
                if (eingerueckt) break;
                continue;  // auf oberster Ebene kann keiner kommen; der Lexer gibt nur geschlossene
            }
            // ein Docstring vorn: Dokumentation, kein Ablauf
            if (erste && nurZeichenkette())
            {
                erste = false;
                while (!zeilenEnde(m_p)) ++m_p;
                continue;
            }
            erste = false;
            const std::size_t davor = m_p;
            liesAnweisung(ort, aus);
            if (m_p == davor)
            {
                melde(Schwere::Fehler, akt(), "unexpected '" + std::string(akt().text) + "'");
                ++m_p;
            }
        }
        m_letzteZeile = zeileVor(m_p);
    }

    /// Der Block hinter dem `:` eines Kopfs: eingerueckt in den folgenden Zeilen, sonst der Rest der Zeile.
    std::vector<AnweisungPtr> liesBlock(const Ort& ort)
    {
        std::vector<AnweisungPtr> aus;
        if (akt().art == TokenArt::Zeilenende)
        {
            ++m_p;
            if (akt().art != TokenArt::Einzug)
            {
                melde(Schwere::Fehler, akt(), "indented block expected");
                m_letzteZeile = zeileVor(m_p);
                return aus;
            }
            ++m_p;
            liesFolge(ort, aus, true);
            return aus;
        }
        liesZeile(ort, aus);
        return aus;
    }

    /// Einfache Anweisungen bis zum Zeilenende (es wird genommen), getrennt durch `;`.
    void liesZeile(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        while (true)
        {
            const std::size_t ende = anweisungsEnde(m_p);
            if (ende > m_p) liesEinfache(ort, m_p, ende, aus);
            m_p = ende;
            if (!nimm(";")) break;
        }
        m_letzteZeile = zeileVor(m_p + 1);
        if (akt().art == TokenArt::Zeilenende) ++m_p;
    }

    // --- Anweisungen ------------------------------------------------------------

    void liesAnweisung(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        if (t.istOp("@"))
        {
            liesDekorator();
            return;
        }
        if (t.istWort("async") && (tok(m_p + 1).istWort("def") || tok(m_p + 1).istWort("for") || tok(m_p + 1).istWort("with")))
        {
            ++m_p;
            m_async = true;
            liesAnweisung(ort, aus);
            return;
        }
        if (t.istWort("def"))
        {
            liesDef(ort, aus);
            return;
        }
        if (t.istWort("class"))
        {
            liesKlasse(ort, aus);
            return;
        }
        if (t.istWort("if") || t.istWort("while") || t.istWort("for") || t.istWort("try") || t.istWort("with") ||
            istMatch(m_p))
        {
            AnweisungPtr a = liesZusammengesetzt(ort);
            if (ort.ebene == Ebene::Klasse)
            {
                melde(Schwere::Hinweis, t, "statement in a class body is not shown");
                return;
            }
            if (ort.ebene == Ebene::Modul) m_ablauf = true;
            if (a) aus.push_back(std::move(a));
            return;
        }
        if (t.istWort("elif") || t.istWort("else") || t.istWort("except") || t.istWort("finally"))
        {
            melde(Schwere::Fehler, t, "'" + std::string(t.text) + "' without its statement");
            m_p = kopfEnde(m_p);
            nimm(":");
            (void)liesBlock(ort);
            return;
        }
        liesZeile(ort, aus);
    }

    /// `@name(…)` vor def oder class: merkt, was die Methode betrifft.
    void liesDekorator()
    {
        ++m_p;
        const std::size_t ende = anweisungsEnde(m_p);
        const std::string text = textVon(m_p, ende);
        if (text == "staticmethod") m_statisch = true;
        if (text.ends_with("abstractmethod")) m_abstrakt = true;
        m_p = ende;
        if (akt().art == TokenArt::Zeilenende) ++m_p;
    }

    /// m_p auf `match`: steht dort eine Anweisung match (ein Kopf mit `:` am Ende der Zeile),
    /// nicht ein Name `match`?
    [[nodiscard]] bool istMatch(std::size_t i)
    {
        if (!istName(i, "match")) return false;
        const Token& danach = tok(i + 1);
        if (zeilenEnde(i + 1) || danach.istOp(":") || danach.istOp("=") || danach.istOp(".") || danach.istOp(",") ||
            danach.istOp(";"))
            return false;
        const std::size_t e = kopfEnde(i + 1);
        return tok(e).istOp(":") && tok(e + 1).art == TokenArt::Zeilenende && oben(i + 1, e, "=") == e;
    }

    AnweisungPtr liesZusammengesetzt(const Ort& ort)
    {
        const Token& t = akt();
        const bool async = std::exchange(m_async, false);
        const std::string vorn = async ? "async " : "";
        if (t.istWort("if")) return liesWenn(ort);
        if (t.istWort("while"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Solange, t);
            const std::size_t e = kopfEnde(m_p);
            a->text = textVon(m_p, e);
            a->kopf = "while " + a->text;
            nimmKopf(e);
            a->kinder.push_back(block(liesBlock(ort), t));
            a->sonst = liesSonst(ort);
            return a;
        }
        if (t.istWort("for"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::FuerJedes, t);
            const std::size_t e = kopfEnde(m_p);
            a->text = textVon(m_p, e);
            a->kopf = vorn + "for " + a->text;
            nimmKopf(e);
            a->kinder.push_back(block(liesBlock(ort), t));
            a->sonst = liesSonst(ort);
            return a;
        }
        if (t.istWort("with"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Mit, t);
            const std::size_t e = kopfEnde(m_p);
            a->text = textVon(m_p, e);
            a->kopf = vorn + "with " + a->text;
            nimmKopf(e);
            a->kinder.push_back(block(liesBlock(ort), t));
            return a;
        }
        if (t.istWort("try")) return liesVersuch(ort);
        return liesMatch(ort);
    }

    AnweisungPtr liesWenn(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;  // if oder elif
        AnweisungPtr a = neu(AnweisungArt::Wenn, t);
        const std::size_t e = kopfEnde(m_p);
        a->text = textVon(m_p, e);
        nimmKopf(e);
        a->kinder.push_back(block(liesBlock(ort), t));
        if (akt().istWort("elif"))
            a->sonst = liesWenn(ort);
        else
            a->sonst = liesSonst(ort);
        return a;
    }

    /// Ein `else:` hinter dem Block, sonst nichts.
    AnweisungPtr liesSonst(const Ort& ort)
    {
        if (!akt().istWort("else")) return nullptr;
        const Token& t = akt();
        ++m_p;
        nimmKopf(m_p);
        return block(liesBlock(ort), t);
    }

    AnweisungPtr liesVersuch(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        AnweisungPtr a = neu(AnweisungArt::Versuch, t);
        nimmKopf(m_p);
        a->kinder.push_back(block(liesBlock(ort), t));
        while (akt().istWort("except"))
        {
            const Token& bei = akt();
            ++m_p;
            const bool gruppe = nimm("*");  // except* (Ausnahmegruppen)
            const std::size_t e = kopfEnde(m_p);
            AnweisungPtr fang = neu(AnweisungArt::Fang, bei, textVon(m_p, e));
            fang->kopf = std::string(gruppe ? "except*" : "except") + (fang->text.empty() ? "" : " " + fang->text);
            nimmKopf(e);
            fang->kinder.push_back(block(liesBlock(ort), bei));
            a->kinder.push_back(std::move(fang));
        }
        a->sonst = liesSonst(ort);
        if (akt().istWort("finally"))
        {
            const Token& bei = akt();
            ++m_p;
            nimmKopf(m_p);
            AnweisungPtr schluss = neu(AnweisungArt::Schluss, bei);
            schluss->kinder.push_back(block(liesBlock(ort), bei));
            a->kinder.push_back(std::move(schluss));
        }
        if (a->kinder.size() == 1) melde(Schwere::Fehler, akt(), "'except' or 'finally' after try is missing");
        return a;
    }

    /// match: je case eine Fall-Marke, dahinter seine Anweisungen — ohne Durchfallen.
    AnweisungPtr liesMatch(const Ort& ort)
    {
        const Token& t = akt();
        ++m_p;
        AnweisungPtr a = neu(AnweisungArt::Auswahl, t);
        const std::size_t e = kopfEnde(m_p);
        a->text = textVon(m_p, e);
        a->kopf = "match " + a->text;
        a->ohneDurchfall = true;
        nimmKopf(e);
        AnweisungPtr rumpf = neu(AnweisungArt::Block, t);
        if (akt().art == TokenArt::Zeilenende) ++m_p;
        if (akt().art != TokenArt::Einzug)
            melde(Schwere::Fehler, akt(), "indented block expected");
        else
        {
            ++m_p;
            while (!amEnde(m_p) && akt().art != TokenArt::Auszug)
            {
                if (akt().art == TokenArt::Zeilenende)
                {
                    ++m_p;
                    continue;
                }
                if (!istName(m_p, "case"))
                {
                    melde(Schwere::Fehler, akt(), "'case' expected");
                    m_p = anweisungsEnde(m_p);
                    if (!amEnde(m_p) && akt().art != TokenArt::Auszug) ++m_p;
                    continue;
                }
                const Token& bei = akt();
                ++m_p;
                const std::size_t k = kopfEnde(m_p);
                std::string muster = textVon(m_p, k);
                nimmKopf(k);
                // `case _:` ist der Fall, der immer passt — wie default
                rumpf->kinder.push_back(neu(AnweisungArt::Fall, bei, muster == "_" ? std::string() : std::move(muster)));
                for (AnweisungPtr& s : liesBlock(ort)) rumpf->kinder.push_back(std::move(s));
            }
            if (akt().art == TokenArt::Auszug) ++m_p;
        }
        a->kinder.push_back(std::move(rumpf));
        return a;
    }

    // --- einfache Anweisungen --------------------------------------------------

    /// Die Namen vor einer Zuweisung [a, z): `x`, `a, b`, `x: T` — leer, wenn links mehr steht als Namen.
    [[nodiscard]] std::vector<std::size_t> zielNamen(std::size_t a, std::size_t z) const
    {
        std::vector<std::size_t> namen;
        const std::size_t doppelpunkt = oben(a, z, ":");
        for (std::size_t i = a; i < doppelpunkt; ++i)
        {
            if (tok(i).art == TokenArt::Bezeichner && (i + 1 == doppelpunkt || tok(i + 1).istOp(",")))
            {
                namen.push_back(i);
                ++i;  // das Komma
                continue;
            }
            return {};
        }
        return namen;
    }

    void liesEinfache(const Ort& ort, std::size_t a, std::size_t b, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = tok(a);
        if (t.istWort("pass")) return;
        if (t.istWort("import") || t.istWort("from"))
        {
            liesImport(a, b);
            if (ort.ebene == Ebene::Funktion) aus.push_back(neu(AnweisungArt::Einfach, t, textVon(a, b)));
            return;
        }
        if (ort.ebene == Ebene::Klasse)
        {
            liesKlassenGlied(ort, a, b);
            return;
        }
        if (t.istWort("return"))
        {
            aus.push_back(neu(AnweisungArt::Ruecksprung, t, textVon(a + 1, b)));
            if (ort.ebene == Ebene::Modul) melde(Schwere::Fehler, t, "'return' outside a function");
            return;
        }
        if (t.istWort("raise"))
        {
            AnweisungPtr w = neu(AnweisungArt::Wurf, t, textVon(a + 1, b));
            w->kopf = textVon(a, b);
            aus.push_back(std::move(w));
            if (ort.ebene == Ebene::Modul) m_ablauf = true;
            return;
        }
        if (t.istWort("break") || t.istWort("continue"))
        {
            aus.push_back(neu(t.istWort("break") ? AnweisungArt::Abbruch : AnweisungArt::Fortsetzung, t));
            return;
        }

        // eine Zuweisung an `self.x` in einer Methode: ein Feld der Klasse
        const std::size_t gleich = oben(a, b, "=");
        if (ort.typ != kKeiner && !ort.selbst.empty() && istName(a, ort.selbst) && tok(a + 1).istOp(".") &&
            tok(a + 2).art == TokenArt::Bezeichner && (a + 3 == gleich || tok(a + 3).istOp(":")))
        {
            const std::size_t typAnfang = a + 4;
            const std::string typ = tok(a + 3).istOp(":") ? textVon(typAnfang, gleich) : std::string();
            neuesFeld(ort.typ, std::string(tok(a + 2).text), typ, {}, tok(a + 2).ort.zeile);
        }
        if (ort.ebene == Ebene::Modul)
        {
            // eine Zuweisung auf oberster Ebene: eine Variable des Moduls
            const bool annotiert = tok(a).art == TokenArt::Bezeichner && tok(a + 1).istOp(":");
            if (gleich < b || annotiert)
            {
                for (const std::size_t n : zielNamen(a, gleich))
                {
                    Variable v;
                    v.name = std::string(tok(n).text);
                    if (annotiert) v.typ = textVon(a + 2, gleich);
                    v.zeile = tok(n).ort.zeile;
                    m_aus.einheit.variablen.push_back(std::move(v));
                }
            }
            else
                m_ablauf = true;
        }
        aus.push_back(neu(AnweisungArt::Einfach, t, textVon(a, b)));
    }

    /// Im Rumpf einer Klasse: `x = 1`, `x: int`, `x: int = 0` sind Felder (einer Aufzaehlung: ihre Werte).
    void liesKlassenGlied(const Ort& ort, std::size_t a, std::size_t b)
    {
        const std::size_t gleich = oben(a, b, "=");
        const bool annotiert = tok(a).art == TokenArt::Bezeichner && tok(a + 1).istOp(":");
        if (gleich == b && !annotiert)
        {
            // ein Ausdruck (`...`, ein Aufruf): kein Glied
            if (!tok(a).istOp("..."))
                melde(Schwere::Hinweis, tok(a), "statement in a class body is not shown");
            return;
        }
        const std::string wert = gleich < b ? "= " + textVon(gleich + 1, b) : std::string();
        const std::string typ = annotiert ? textVon(a + 2, gleich) : std::string();
        for (const std::size_t n : zielNamen(a, gleich))
            neuesFeld(ort.typ, std::string(tok(n).text), typ, wert, tok(n).ort.zeile);
    }

    void neuesFeld(std::size_t typ, std::string name, std::string feldTyp, std::string zusatz, int zeile)
    {
        Typ& t = m_aus.einheit.typen[typ];
        if (std::any_of(t.felder.begin(), t.felder.end(), [&](const Feld& f) { return f.name == name; })) return;
        Feld f;
        f.name = std::move(name);
        f.typ = std::move(feldTyp);
        f.zusatz = std::move(zusatz);
        f.zeile = zeile;
        f.typHinten = true;
        t.felder.push_back(std::move(f));
    }

    /// `import a.b as c, d` und `from .m import x`: je Modul eine Einbindung.
    void liesImport(std::size_t a, std::size_t b)
    {
        const int zeile = tok(a).ort.zeile;
        if (tok(a).istWort("from"))
        {
            std::size_t e = a + 1;
            while (e < b && !tok(e).istWort("import")) ++e;
            m_aus.einheit.einbindungen.push_back({textVon(a + 1, e), false, zeile});
            return;
        }
        for (const Bereich& teil : teile(a + 1, b))
        {
            std::size_t e = teil.first;
            while (e < teil.second && !tok(e).istWort("as")) ++e;
            m_aus.einheit.einbindungen.push_back({textVon(teil.first, e), false, zeile});
        }
    }

    // --- def und class -------------------------------------------------------------

    /// `def name(…) -> T:` und ihr Rumpf: eine Funktion; im Rumpf einer Klasse eine Methode.
    void liesDef(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        ++m_p;
        const bool statisch = std::exchange(m_statisch, false);
        const bool abstrakt = std::exchange(m_abstrakt, false);
        m_async = false;
        if (akt().art != TokenArt::Bezeichner)
        {
            melde(Schwere::Fehler, akt(), "name of the function expected");
            m_p = kopfEnde(m_p);
            nimm(":");
            (void)liesBlock(ort);
            return;
        }
        Funktion f;
        f.name = std::string(akt().text);
        f.zeile = t.ort.zeile;
        ++m_p;
        if (akt().istOp("[")) m_p = ueberKlammer(m_p);  // Typ-Parameter (3.12)
        if (akt().istOp("("))
        {
            const std::size_t zu = ueberKlammer(m_p);
            const std::size_t innen = tok(zu - 1).istOp(")") ? zu - 1 : zu;
            for (const Bereich& s : teile(m_p + 1, innen)) f.parameter.push_back(parameter(s));
            m_p = zu;
        }
        else
            melde(Schwere::Fehler, akt(), "'(' expected");
        // `-> T` wird uebergangen: das Modell traegt den Rueckgabetyp vor dem Namen (C-Familie)
        nimmKopf(kopfEnde(m_p));

        if (ort.ebene == Ebene::Klasse)
            f.bereich = ort.klasse;
        else if (ort.ebene == Ebene::Funktion)
        {
            f.aussen = ort.funktion;
            f.bereich = ort.funktion;  // so bleibt der volle Name eindeutig (`aussen::innen`)
        }

        Ort innen;
        innen.ebene = Ebene::Funktion;
        innen.klasse = ort.klasse;
        innen.typ = ort.typ;
        innen.funktion = f.vollerName();
        innen.selbst = ort.selbst;
        if (ort.ebene == Ebene::Klasse)
            innen.selbst = !statisch && !f.parameter.empty() ? f.parameter.front().name : std::string();

        if (ort.ebene == Ebene::Klasse)
        {
            Methode m;
            m.name = f.name;
            m.parameter = f.parameter;
            m.statisch = statisch;
            m.abstrakt = abstrakt;
            m.zeile = f.zeile;
            m_aus.einheit.typen[ort.typ].methoden.push_back(std::move(m));
        }
        if (ort.ebene == Ebene::Funktion)
            aus.push_back(neu(AnweisungArt::Einfach, t, "def " + f.signatur().substr(f.bereich.size() + 2)));

        // der Platz in der Reihenfolge der Quelle: vor den Funktionen, die in ihr stehen
        const std::size_t platz = m_aus.einheit.funktionen.size();
        m_aus.einheit.funktionen.emplace_back();
        f.rumpf = block(liesBlock(innen), t);
        f.bisZeile = m_letzteZeile;
        m_aus.einheit.funktionen[platz] = std::move(f);
    }

    /// Ein Parameter: `x`, `x: int`, `x: int = 3`, `*args`, `**kw`, `/`, `*` — ohne Vorgabewert
    /// (wie in der C-Familie).
    [[nodiscard]] Parameter parameter(const Bereich& s) const
    {
        Parameter p;
        p.typHinten = true;
        const std::size_t gleich = oben(s.first, s.second, "=");
        const std::size_t doppelpunkt = oben(s.first, gleich, ":");
        p.name = textVon(s.first, doppelpunkt);
        if (doppelpunkt < gleich) p.typ = textVon(doppelpunkt + 1, gleich);
        return p;
    }

    /// `class Name(Basis, metaclass=M):` und ihr Rumpf: ein Typ.
    void liesKlasse(const Ort& ort, std::vector<AnweisungPtr>& aus)
    {
        const Token& t = akt();
        ++m_p;
        m_statisch = m_abstrakt = false;
        if (akt().art != TokenArt::Bezeichner)
        {
            melde(Schwere::Fehler, akt(), "name of the class expected");
            m_p = kopfEnde(m_p);
            nimm(":");
            (void)liesBlock(ort);
            return;
        }
        Typ typ;
        typ.art = TypArt::Klasse;
        typ.name = std::string(akt().text);
        typ.zeile = t.ort.zeile;
        typ.bereich = ort.ebene == Ebene::Klasse ? ort.klasse : ort.funktion;
        ++m_p;
        if (akt().istOp("[")) m_p = ueberKlammer(m_p);
        std::string basen;
        if (akt().istOp("("))
        {
            const std::size_t zu = ueberKlammer(m_p);
            const std::size_t innen = tok(zu - 1).istOp(")") ? zu - 1 : zu;
            basen = textVon(m_p, zu);
            for (const Bereich& s : teile(m_p + 1, innen))
            {
                if (oben(s.first, s.second, "=") < s.second) continue;  // metaclass=…
                std::string basis = textVon(s.first, s.second);
                const std::string_view kurz = std::string_view(basis).substr(basis.rfind('.') + 1);
                if (std::find(std::begin(kAufzaehlungen), std::end(kAufzaehlungen), kurz) != std::end(kAufzaehlungen))
                    typ.art = TypArt::Aufzaehlung;
                typ.basen.push_back(std::move(basis));
            }
            m_p = zu;
        }
        nimmKopf(kopfEnde(m_p));
        if (ort.ebene == Ebene::Funktion) aus.push_back(neu(AnweisungArt::Einfach, t, "class " + typ.name + basen));

        Ort innen;
        innen.ebene = Ebene::Klasse;
        innen.klasse = typ.vollerName();
        innen.typ = m_aus.einheit.typen.size();
        innen.funktion = ort.funktion;
        const bool aufzaehlung = typ.art == TypArt::Aufzaehlung;
        m_aus.einheit.typen.push_back(std::move(typ));
        (void)liesBlock(innen);
        if (aufzaehlung)
        {
            // die Werte einer Aufzaehlung tragen keinen Typ, ihr Wert steht im Zusatz
            for (Feld& f : m_aus.einheit.typen[innen.typ].felder) f.typHinten = false;
        }
    }

    bool m_ablauf = false;    ///< auf oberster Ebene laeuft etwas ab (nicht nur def, class, import, Zuweisungen)
    bool m_async = false;     ///< vor der naechsten Anweisung stand `async`
    bool m_statisch = false;  ///< Dekorator @staticmethod vor dem naechsten def
    bool m_abstrakt = false;  ///< Dekorator @abstractmethod
    int m_letzteZeile = 0;    ///< wo der zuletzt gelesene Block endet
};

class Python final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "Python"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungen; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return kTabelle; }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& /*optionen*/) const override
    {
        Ergebnis ergebnis;
        ergebnis.einheit.name = std::string(einheitName);
        PythonLeser(text, ergebnis).liesModul();
        ergaenzeLogik(ergebnis.einheit, kTabelle, kRegeln);
        return ergebnis;
    }
};

}  // namespace

const Sprache& python()
{
    static const Python sprache;
    return sprache;
}

}  // namespace c2d::sprache
