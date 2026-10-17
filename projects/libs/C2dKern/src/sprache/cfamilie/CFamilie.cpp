/**
 * @file CFamilie.cpp
 * @brief Der Leser der C-Familie in rekursivem Abstieg
 */
#include "CFamilie.hpp"

#include "../Leser.hpp"
#include "../Bedingt.hpp"
#include "../Logik.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace c2d::sprache::cfamilie
{

namespace
{

using namespace c2d::modell;
using quelle::Schwere;

std::string_view gestutzt(std::string_view s)
{
    constexpr std::string_view kLeer = " \t\r\n";
    const std::size_t a = s.find_first_not_of(kLeer);
    if (a == std::string_view::npos) return {};
    return s.substr(a, s.find_last_not_of(kLeer) - a + 1);
}

bool istNamenZeichen(char c)
{
    return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

std::string_view fuehrenderName(std::string_view s)
{
    std::size_t n = 0;
    while (n < s.size() && istNamenZeichen(s[n])) ++n;
    return s.substr(0, n);
}

/// Fortsetzungen und Folgen von Leerraum zu je einem Leerzeichen.
std::string eineZeile(std::string_view s)
{
    std::string aus;
    bool luecke = false;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        const char c = s[i];
        const bool fortsetzung = c == '\\' && i + 1 < s.size() && (s[i + 1] == '\n' || s[i + 1] == '\r');
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || fortsetzung)
        {
            luecke = true;
            continue;
        }
        if (luecke && !aus.empty()) aus += ' ';
        luecke = false;
        aus += c;
    }
    return aus;
}

/// Sieht ein Name aus wie ein Makro: nur Grossbuchstaben, Ziffern, Unterstrich?
bool wieEinMakro(std::string_view name)
{
    bool buchstabe = false;
    for (const char c : name)
    {
        if (c >= 'A' && c <= 'Z')
            buchstabe = true;
        else if (c != '_' && !(c >= '0' && c <= '9'))
            return false;
    }
    return buchstabe;
}

class CFamilienLeser : private Leser
{
public:
    /// `varianten`: die #if, die als Varianten stehen geblieben sind (Bedingt.hpp); darf fehlen.
    CFamilienLeser(std::string_view text, const lexer::Sprachtabelle& tabelle, Dialekt dialekt, Ergebnis& aus,
                   const Varianten* varianten = nullptr)
        : Leser(text, tabelle, aus), m_cpp(dialekt.cpp)
    {
        if (varianten == nullptr) return;
        m_nurGeltend = varianten->nurGeltend;
        for (const Variantengruppe& g : varianten->gruppen) m_varianten.emplace(g.direktiven.front(), &g);
    }

    void liesEinheit()
    {
        Rahmen wurzel;
        liesFolge(wurzel, false);
    }

    /// Die Ruempfe der gelesenen Funktionen, als Versatz [von, bis) im Text.
    [[nodiscard]] const std::vector<std::pair<std::size_t, std::size_t>>& ruempfe() const { return m_ruempfe; }

private:
    /// Worin gerade gelesen wird: die Datei, ein Namensraum oder der Rumpf eines Typs.
    struct Rahmen
    {
        std::size_t typ = kKeiner;  ///< Stelle in einheit().typen; kKeiner = kein Typ
        Sichtbarkeit sicht = Sichtbarkeit::Keine;
        std::string bereich;  ///< Namensraum und umgebende Typen
    };

    struct Deklarator
    {
        std::string typ;
        std::string name;
        std::string zusatz;
        std::size_t basisEnde = 0;  ///< bis hierher reicht der Typ, den weitere Deklaratoren teilen
        Bereich anfang{0, 0};       ///< die Token des Anfangswerts (hinter `=`, oder ab `{`); leer = keiner
    };

    /// Wo der Anfangswert eines Deklarators [a, b) steht: hinter `=`, oder ab `{` (C++).
    [[nodiscard]] Bereich anfangIn(std::size_t a, std::size_t b) const
    {
        const std::size_t ende = std::min(sucheD(a, {"=", "{"}), b);
        if (ende >= b) return {0, 0};
        return m_t[ende].istOp("=") ? Bereich{ende + 1, b} : Bereich{ende, b};
    }

    /// Ein Anfangswert [a, b) als Baum: eine Liste `{ … }` mit ihren Teilen, je Teil sein Bezeichner.
    [[nodiscard]] modell::Initialisierer initialisierer(std::size_t a, std::size_t b) const
    {
        modell::Initialisierer i;
        i.text = textVon(a, b);
        i.zeile = a < m_t.size() ? m_t[a].ort.zeile : 0;
        if (b - a < 2 || !m_t[a].istOp("{") || nachKlammer(a) != b) return i;
        i.liste = true;
        for (Bereich s : teile(a + 1, b - 1))
        {
            std::string index;
            std::string glied;
            // [X] = …  oder  .feld = …
            if (m_t[s.first].istOp("["))
            {
                const std::size_t zu = nachKlammer(s.first);
                if (zu < s.second && m_t[zu].istOp("="))
                {
                    index = textVon(s.first + 1, zu - 1);
                    s.first = zu + 1;
                }
            }
            else if (m_t[s.first].istOp(".") && s.first + 2 < s.second && m_t[s.first + 1].art == TokenArt::Bezeichner &&
                     m_t[s.first + 2].istOp("="))
            {
                glied = std::string(m_t[s.first + 1].text);
                s.first += 3;
            }
            if (s.first >= s.second) continue;
            modell::Initialisierer teil = initialisierer(s.first, s.second);
            teil.index = std::move(index);
            teil.glied = std::move(glied);
            i.teile.push_back(std::move(teil));
        }
        return i;
    }

    /// Ist der Typ konstant (das Wort `const` darin)?
    [[nodiscard]] static bool istKonstant(const std::string& typ)
    {
        for (std::size_t p = typ.find("const"); p != std::string::npos; p = typ.find("const", p + 1))
        {
            const bool vorn = p == 0 || typ[p - 1] == ' ' || typ[p - 1] == '*';
            const bool hinten = p + 5 >= typ.size() || typ[p + 5] == ' ' || typ[p + 5] == '*';
            if (vorn && hinten) return true;
        }
        return false;
    }

    Einheit& einheit() { return m_aus.einheit; }

    [[nodiscard]] static bool istTypWort(const Token& t)
    {
        return t.istWort("struct") || t.istWort("union") || t.istWort("enum") || t.istWort("class");
    }

    // -----------------------------------------------------------------------
    // Spitze Klammern (nur C++): in Deklarationen klammern sie wie ( )
    // -----------------------------------------------------------------------

    /// i steht auf `<`: die Stelle hinter dem passenden `>`; kKeiner, wenn es
    /// vor einem `;` oder `{` keines gibt (dann war es ein Vergleich).
    [[nodiscard]] std::size_t nachWinkel(std::size_t i) const
    {
        int tiefe = 0;
        while (!amEnde(i))
        {
            const Token& t = m_t[i];
            if (t.art == TokenArt::Operator)
            {
                if (t.text == "<")
                    ++tiefe;
                else if (t.text == ">" && --tiefe <= 0)
                    return i + 1;
                else if (t.text == ">>" && (tiefe -= 2) <= 0)
                    return i + 1;
                else if (t.text == "(" || t.text == "[")
                {
                    i = nachKlammer(i);
                    continue;
                }
                else if (t.text == "{" || t.text == ";" || schliesst(t))
                    return kKeiner;
            }
            ++i;
        }
        return kKeiner;
    }

    /// Oeffnet das `<` an Stelle i eine Schablonen-Klammer? Nur hinter einem Namen.
    [[nodiscard]] bool istWinkelAuf(std::size_t i) const
    {
        return m_cpp && i > 0 && m_t[i].istOp("<") &&
               (m_t[i - 1].art == TokenArt::Bezeichner || m_t[i - 1].istWort("template")) &&
               nachWinkel(i) != kKeiner;
    }

    /// Wie Leser::suche, in einer Deklaration: spitze Klammern werden uebersprungen.
    [[nodiscard]] std::size_t sucheD(std::size_t i, std::initializer_list<std::string_view> stopps) const
    {
        while (!amEnde(i))
        {
            const Token& t = m_t[i];
            if (t.art == TokenArt::Operator)
            {
                if (std::find(stopps.begin(), stopps.end(), t.text) != stopps.end()) return i;
                if (schliesst(t)) return i;
                if (oeffnet(t))
                {
                    i = nachKlammer(i);
                    continue;
                }
                if (istWinkelAuf(i))
                {
                    i = nachWinkel(i);
                    continue;
                }
            }
            ++i;
        }
        return i;
    }

    /// Wie Leser::teile, in einer Deklaration.
    [[nodiscard]] std::vector<Bereich> teileD(std::size_t a, std::size_t b) const
    {
        std::vector<Bereich> stuecke;
        while (a < b)
        {
            const std::size_t komma = std::min(sucheD(a, {","}), b);
            if (komma > a) stuecke.emplace_back(a, komma);
            if (komma >= b || !m_t[komma].istOp(",")) break;
            a = komma + 1;
        }
        return stuecke;
    }

    // -----------------------------------------------------------------------
    // Die Folge der Deklarationen: Datei, Namensraum, Rumpf eines Typs
    // -----------------------------------------------------------------------

    /// Liest bis zum Ende — oder, `inKlammer`, bis vor die schliessende `}`.
    void liesFolge(Rahmen& r, bool inKlammer)
    {
        while (!amEnde(m_p))
        {
            if (akt().istOp("}"))
            {
                if (inKlammer) return;
                melde(Schwere::Warnung, akt(), "unexpected '}'");
                ++m_p;
                continue;
            }
            const std::size_t davor = m_p;
            liesGlied(r);
            if (m_p == davor) ++m_p;  // nie auf der Stelle treten
        }
    }

    /// Ein Makro allein auf seiner Zeile, etwa Q_OBJECT oder PACKED_BEGIN: kein Teil der Deklaration danach.
    [[nodiscard]] bool istEinzelnesMakro(std::size_t i) const
    {
        const Token& t = m_t[i];
        if (t.art != TokenArt::Bezeichner || !wieEinMakro(t.text)) return false;
        const bool davorFrei = i == 0 || m_t[i - 1].ort.zeile != t.ort.zeile;
        return davorFrei && (amEnde(i + 1) || tok(i + 1).ort.zeile != t.ort.zeile);
    }

    void liesGlied(Rahmen& r)
    {
        const Token& t = akt();
        if (t.art == TokenArt::Direktive)
        {
            if (r.typ == kKeiner) liesDirektive(t);
            ++m_p;
            return;
        }
        if (nimm(";")) return;
        if (istEinzelnesMakro(m_p))
        {
            ++m_p;
            return;
        }
        if (t.istWort("typedef"))
        {
            liesTypedef(r);
            return;
        }
        // `extern "C" {` — auch in C-Koepfen, hinter #ifdef __cplusplus
        if (t.istWort("extern") && tok(m_p + 1).art == TokenArt::Zeichenkette)
        {
            m_p += 2;
            if (nimm("{"))
            {
                liesFolge(r, true);
                if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' of extern \"C\" is missing");
            }
            return;
        }

        // Ab hier C++: diese Woerter fuehrt nur dessen Tabelle als Schluesselwort.
        if (t.istWort("namespace") || (t.istWort("inline") && tok(m_p + 1).istWort("namespace")))
        {
            liesNamensraum(r);
            return;
        }
        if (t.istWort("template"))
        {
            // die Schablone wird uebergangen, was ihr folgt, gelesen
            ++m_p;
            if (akt().istOp("<"))
            {
                const std::size_t zu = nachWinkel(m_p);
                m_p = zu == kKeiner ? m_p + 1 : zu;
            }
            return;
        }
        if (t.istWort("using"))
        {
            liesUsing();
            return;
        }
        if (t.istWort("static_assert") || t.istWort("friend"))
        {
            const std::size_t ende = sucheD(m_p + 1, {";", "{"});
            m_p = tok(ende).istOp("{") ? nachKlammer(ende) : ende;
            nimm(";");
            return;
        }
        if (t.istOp("[") && tok(m_p + 1).istOp("["))  // [[attribut]]
        {
            m_p = nachKlammer(m_p);
            return;
        }
        if (liesSichtbarkeit(r)) return;

        liesDeklaration(r);
    }

    /// `public:`, `private slots:`, `signals:` — nur im Rumpf eines Typs.
    bool liesSichtbarkeit(Rahmen& r)
    {
        if (r.typ == kKeiner) return false;
        const Token& t = akt();
        Sichtbarkeit sicht = Sichtbarkeit::Keine;
        if (t.istWort("public")) sicht = Sichtbarkeit::Oeffentlich;
        else if (t.istWort("protected")) sicht = Sichtbarkeit::Geschuetzt;
        else if (t.istWort("private")) sicht = Sichtbarkeit::Privat;
        else if (t.art == TokenArt::Bezeichner && (t.text == "signals" || t.text == "Q_SIGNALS") &&
                 tok(m_p + 1).istOp(":"))
            sicht = Sichtbarkeit::Oeffentlich;  // Qt
        if (sicht == Sichtbarkeit::Keine) return false;

        std::size_t q = m_p + 1;
        if (tok(q).art == TokenArt::Bezeichner && (tok(q).text == "slots" || tok(q).text == "Q_SLOTS")) ++q;
        if (!tok(q).istOp(":")) return false;
        r.sicht = sicht;
        m_p = q + 1;
        return true;
    }

    void liesNamensraum(Rahmen& r)
    {
        if (akt().istWort("inline")) ++m_p;
        ++m_p;
        std::string name;
        while (akt().art == TokenArt::Bezeichner || akt().istOp("::"))
        {
            name += akt().text;
            ++m_p;
        }
        if (!akt().istOp("{"))
        {
            // namespace a = b;
            m_p = sucheD(m_p, {";"});
            nimm(";");
            return;
        }
        ++m_p;
        Rahmen innen;
        innen.bereich = name.empty() ? r.bereich : vollerName(r.bereich, name);
        liesFolge(innen, true);
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' of the namespace is missing");
    }

    /// `using Name = Typ;` wird ein Alias; jedes andere using wird uebergangen.
    void liesUsing()
    {
        const int zeile = akt().ort.zeile;
        ++m_p;
        const std::size_t ende = sucheD(m_p, {";"});
        if (akt().art == TokenArt::Bezeichner && tok(m_p + 1).istOp("="))
            einheit().aliase.push_back({std::string(akt().text), textVon(m_p + 2, ende), zeile});
        m_p = ende;
        nimm(";");
    }

    // -----------------------------------------------------------------------
    // Praeprozessor
    // -----------------------------------------------------------------------

    void liesDirektive(const Token& t)
    {
        std::string_view zeile = gestutzt(t.text.substr(1));
        const std::string_view wort = fuehrenderName(zeile);
        std::string_view rest = gestutzt(zeile.substr(wort.size()));
        // Kommentare am Ende gehoeren nicht zur Direktive
        if (const std::size_t k = std::min(rest.find("//"), rest.find("/*")); k != std::string_view::npos)
            rest = gestutzt(rest.substr(0, k));

        if (wort == "include")
        {
            Einbindung e;
            e.zeile = t.ort.zeile;
            e.system = rest.starts_with('<');
            if (rest.starts_with('<') || rest.starts_with('"'))
            {
                const std::size_t zu = rest.find(e.system ? '>' : '"', 1);
                e.ziel = std::string(rest.substr(1, zu == std::string_view::npos ? zu : zu - 1));
            }
            else
                e.ziel = std::string(rest);  // etwa ein Makro
            einheit().einbindungen.push_back(std::move(e));
        }
        else if (wort == "define")
        {
            const std::string_view name = fuehrenderName(rest);
            if (name.empty()) return;
            // bei einem Makro mit Parametern folgt die Klammer ohne Abstand
            const std::string_view wert = rest.substr(name.size());
            einheit().makros.push_back(
                {std::string(name), eineZeile(wert.starts_with('(') ? wert : gestutzt(wert)), t.ort.zeile});
        }
        else if (wort == "undef")
        {
            const std::string_view name = fuehrenderName(rest);
            if (!name.empty()) einheit().aufhebungen.push_back({std::string(name), t.ort.zeile});
        }
    }

    // -----------------------------------------------------------------------
    // Deklaratoren
    // -----------------------------------------------------------------------

    /// Woerter, die vor einem Typ stehen, selbst aber keiner sind.
    [[nodiscard]] static bool istVorsatz(const Token& t)
    {
        static constexpr std::string_view kVorsaetze[] = {
            "const",  "volatile", "static",       "extern",   "inline",  "constexpr", "constinit",
            "mutable", "register", "thread_local", "typename", "virtual", "explicit",  "friend"};
        return t.art == TokenArt::Schluesselwort &&
               std::find(std::begin(kVorsaetze), std::end(kVorsaetze), t.text) != std::end(kVorsaetze);
    }

    /// Ein ganzes Stueck `Typ Deklarator [= Wert]`: Typ, Name und was dem Namen folgt.
    [[nodiscard]] Deklarator zerlegeVoll(std::size_t a, std::size_t b) const
    {
        const std::size_t ende = std::min(sucheD(a, {"=", "{"}), b);

        // Zeiger auf Funktion oder geklammerter Deklarator: ( * name ...
        for (std::size_t i = a; i + 1 < ende; ++i)
        {
            if (!m_t[i].istOp("(") || !m_t[i + 1].istOp("*")) continue;
            for (std::size_t k = i + 2; k < ende; ++k)
            {
                if (m_t[k].art == TokenArt::Bezeichner)
                    return {textVon(a, k), std::string(m_t[k].text), textVon(k + 1, ende), basisVor(a, k), anfangIn(a, b)};
                if (!m_t[k].istOp("*") && m_t[k].art != TokenArt::Schluesselwort) break;
            }
            break;
        }

        std::size_t grenze = ende;
        for (std::size_t i = a; i < ende;)
        {
            if (istWinkelAuf(i))
            {
                i = nachWinkel(i);
                continue;
            }
            if (m_t[i].istOp("[") || m_t[i].istOp(":") || m_t[i].istOp("("))
            {
                grenze = i;
                break;
            }
            ++i;
        }
        const Deklarator ohneNamen{textVon(a, ende), {}, {}, ende, {0, 0}};
        if (grenze <= a || m_t[grenze - 1].art != TokenArt::Bezeichner) return ohneNamen;

        // Der letzte Bezeichner ist der Name, mit dem, was ihn qualifiziert
        // (`Klasse::glied`) — ausser davor steht kein Typ: dann ist er selbst
        // der Typ (`std::string`, `const Punkt`, `struct Punkt`).
        std::size_t anfang = grenze - 1;
        while (anfang >= a + 2 && m_t[anfang - 1].istOp("::") && m_t[anfang - 2].art == TokenArt::Bezeichner)
            anfang -= 2;
        if (anfang > a && m_t[anfang - 1].istOp("::")) return ohneNamen;  // `::global` oder `Schablone<T>::typ`
        std::size_t davor = a;
        while (davor < anfang && istVorsatz(m_t[davor])) ++davor;
        if (davor == anfang || istTypWort(m_t[anfang - 1])) return ohneNamen;

        return {textVon(a, anfang), textVon(anfang, grenze), textVon(grenze, ende), basisVor(a, anfang), anfangIn(a, b)};
    }

    /// Wo vor dem Namen an Stelle `name` der Typ endet, den weitere Deklaratoren teilen.
    [[nodiscard]] std::size_t basisVor(std::size_t a, std::size_t name) const
    {
        std::size_t ende = name;
        while (ende > a)
        {
            const Token& davor = m_t[ende - 1];
            if (!davor.istOp("*") && !davor.istOp("&") && !davor.istOp("&&") && !davor.istOp("(") &&
                !davor.istWort("const") && !davor.istWort("volatile") && !davor.istWort("restrict"))
                break;
            --ende;
        }
        return ende;
    }

    /// Ein weiterer Deklarator zum Typ `basis`: `*b`, `c[4]`, `(*f)(int)`.
    [[nodiscard]] Deklarator zerlegeWeiteren(std::size_t a, std::size_t b, const std::string& basis) const
    {
        const std::size_t ende = std::min(sucheD(a, {"=", "{"}), b);
        std::size_t name = a;
        while (name < ende && m_t[name].art != TokenArt::Bezeichner) ++name;
        if (name >= ende) return {basis, {}, {}, a, {0, 0}};
        std::string typ = basis;
        if (name > a) typ += (typ.empty() ? "" : " ") + textVon(a, name);
        return {std::move(typ), std::string(m_t[name].text), textVon(name + 1, ende), a, anfangIn(a, b)};
    }

    /// Die Deklaratoren [a, b) zu einem schon gelesenen Typ (hinter dessen `}`).
    [[nodiscard]] std::vector<Deklarator> deklaratorenZu(std::size_t a, std::size_t b) const
    {
        std::vector<Deklarator> aus;
        for (const Bereich& s : teileD(a, b))
        {
            Deklarator d = zerlegeWeiteren(s.first, s.second, {});
            if (!d.name.empty()) aus.push_back(std::move(d));
        }
        return aus;
    }

    /// `Typ a, *b, c[4]` in [a, b): je Deklarator ein Eintrag; leer, wenn kein Name da ist.
    [[nodiscard]] std::vector<Deklarator> deklarationen(std::size_t a, std::size_t b) const
    {
        std::vector<Deklarator> aus;
        const std::vector<Bereich> stuecke = teileD(a, b);
        if (stuecke.empty()) return aus;
        Deklarator erster = zerlegeVoll(stuecke[0].first, stuecke[0].second);
        if (erster.name.empty()) return aus;
        const std::string basis = textVon(stuecke[0].first, erster.basisEnde);
        aus.push_back(std::move(erster));
        for (std::size_t i = 1; i < stuecke.size(); ++i)
        {
            Deklarator d = zerlegeWeiteren(stuecke[i].first, stuecke[i].second, basis);
            if (!d.name.empty()) aus.push_back(std::move(d));
        }
        return aus;
    }

    /// Eine Variable ablegen: im Rumpf eines Typs als Glied, sonst als globale Variable.
    void lege(const Rahmen& r, Deklarator d, int zeile)
    {
        if (r.typ == kKeiner)
        {
            modell::Variable v{std::move(d.typ), std::move(d.name), std::move(d.zusatz), zeile, std::nullopt};
            if (istKonstant(v.typ) && d.anfang.second > d.anfang.first) v.anfang = initialisierer(d.anfang.first, d.anfang.second);
            einheit().variablen.push_back(std::move(v));
        }
        else
            einheit().typen[r.typ].felder.push_back(
                {std::move(d.typ), std::move(d.name), std::move(d.zusatz), r.sicht, zeile});
    }

    /// Hinter einer Deklaration: das `;` nehmen — oder sagen, was stattdessen dasteht.
    void schliesseAb(const Rahmen& r)
    {
        if (nimm(";") || amEnde(m_p)) return;
        if (akt().istOp("}"))
        {
            // im Rumpf eines Typs endet er hier; sonst meldet liesFolge die Klammer
            if (r.typ != kKeiner) melde(Schwere::Warnung, akt(), "';' after the last member is missing");
            return;
        }
        melde(Schwere::Warnung, akt(), "unexpected '" + std::string(akt().text) + "'");
        ++m_p;
    }

    // -----------------------------------------------------------------------
    // Typen
    // -----------------------------------------------------------------------

    /// Das struct/union/enum/class, dessen Rumpf die `{` an Stelle `auf`
    /// oeffnet: das erste solche Wort ab `ab`. kKeiner = keines.
    [[nodiscard]] std::size_t typWortVor(std::size_t ab, std::size_t auf) const
    {
        for (std::size_t i = ab; i < auf;)
        {
            if (istTypWort(m_t[i])) return i;
            i = oeffnet(m_t[i]) ? nachKlammer(i) : i + 1;
        }
        return kKeiner;
    }

    /// Ein Name fuer einen Typ ohne Etikett: nach dem Glied oder der Variable,
    /// die ihn traegt, sonst durchgezaehlt. Eindeutig in der Einheit.
    std::string anonymerName(const std::string& nach)
    {
        const auto vergeben = [this](const std::string& name) {
            return std::any_of(einheit().typen.begin(), einheit().typen.end(),
                               [&](const Typ& t) { return t.name == name; });
        };
        std::string name = "anonymous_" + (nach.empty() ? std::to_string(++m_anonym) : nach);
        while (vergeben(name)) name = "anonymous_" + std::to_string(++m_anonym);
        return name;
    }

    /// m_p steht auf struct/union/enum/class, der Rumpf folgt. Danach steht m_p
    /// hinter der `}`. Liefert die Stelle des Typs in einheit().typen; sein
    /// Name ist leer, wenn kein Etikett da war — der Aufrufer benennt ihn dann.
    std::size_t liesTyp(const Rahmen& r)
    {
        Typ typ;
        typ.zeile = akt().ort.zeile;
        typ.bereich = r.bereich;
        typ.art = akt().istWort("struct")  ? TypArt::Struktur
                  : akt().istWort("union") ? TypArt::Vereinigung
                  : akt().istWort("class") ? TypArt::Klasse
                                           : TypArt::Aufzaehlung;
        ++m_p;
        if (typ.art == TypArt::Aufzaehlung && (akt().istWort("class") || akt().istWort("struct"))) ++m_p;

        // Der Name: der letzte Bezeichner gilt — davor koennen Makros stehen
        // (`struct PACKED adresse`) oder ein Bereich (`struct a::b`).
        for (;;)
        {
            const Token& t = akt();
            if (t.art == TokenArt::Bezeichner)
            {
                const bool schluss = t.text == "final" && (tok(m_p + 1).istOp("{") || tok(m_p + 1).istOp(":"));
                if (!schluss) typ.name = std::string(t.text);
            }
            else if (istWinkelAuf(m_p))  // Spezialisierung einer Schablone
            {
                const std::size_t zu = nachWinkel(m_p);
                typ.name += textVon(m_p, zu);
                m_p = zu;
                continue;
            }
            else if (t.istOp("[") || t.istOp("("))  // [[attribut]], alignas(...)
            {
                m_p = nachKlammer(m_p);
                continue;
            }
            else if (!t.istOp("::") && !t.istWort("alignas"))
                break;
            ++m_p;
        }

        // Basen — bei einer Aufzaehlung der Typ darunter, der keine Basis ist
        if (nimm(":"))
        {
            const std::size_t ende = sucheD(m_p, {"{", ";"});
            if (typ.art != TypArt::Aufzaehlung)
            {
                for (Bereich s : teileD(m_p, ende))
                {
                    while (s.first < s.second &&
                           (m_t[s.first].istWort("public") || m_t[s.first].istWort("protected") ||
                            m_t[s.first].istWort("private") || m_t[s.first].istWort("virtual")))
                        ++s.first;
                    if (s.first < s.second) typ.basen.push_back(textVon(s.first, s.second));
                }
            }
            m_p = ende;
        }
        if (!nimm("{")) melde(Schwere::Fehler, akt(), "'{' of the type expected");

        // der aeussere Typ zuerst, verschachtelte danach
        const std::size_t index = einheit().typen.size();
        const TypArt art = typ.art;
        einheit().typen.push_back(std::move(typ));
        if (art == TypArt::Aufzaehlung)
            liesWerte(index);
        else
        {
            Rahmen innen;
            innen.typ = index;
            if (m_cpp) innen.sicht = art == TypArt::Klasse ? Sichtbarkeit::Privat : Sichtbarkeit::Oeffentlich;
            // In C++ ist der Typ ein Bereich fuer das, was in ihm steht; ein Typ
            // ohne Etikett gibt keinen. In C liegen auch verschachtelte Typen in der Datei.
            const Typ& selbst = einheit().typen[index];
            innen.bereich = m_cpp && !selbst.name.empty() ? selbst.vollerName() : r.bereich;
            liesFolge(innen, true);
        }
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' of the type is missing");
        return index;
    }

    void liesWerte(std::size_t index)
    {
        const std::size_t zu = suche(m_p, {"}"});
        for (Bereich s : teile(m_p, zu))
        {
            while (s.first < s.second && m_t[s.first].art == TokenArt::Direktive) ++s.first;
            if (s.first >= s.second || m_t[s.first].art != TokenArt::Bezeichner) continue;
            einheit().typen[index].felder.push_back(
                {{}, std::string(m_t[s.first].text), textVon(s.first + 1, s.second), Sichtbarkeit::Keine,
                 m_t[s.first].ort.zeile});
        }
        m_p = zu;
    }

    /// Hinter der `}` eines Typs: `[a, *b];` — Variablen oder, im Rumpf eines Typs, Glieder.
    void liesVariablenZuTyp(const Rahmen& r, std::size_t index)
    {
        const int zeile = akt().ort.zeile;
        const std::size_t ende = sucheD(m_p, {";"});
        std::vector<Deklarator> variablen = deklaratorenZu(m_p, ende);
        const bool anonym = einheit().typen[index].name.empty();
        if (anonym) einheit().typen[index].name = anonymerName(variablen.empty() ? std::string() : variablen[0].name);
        const std::string name = einheit().typen[index].name;
        for (Deklarator& d : variablen)
        {
            d.typ = d.typ.empty() ? name : name + " " + d.typ;
            lege(r, std::move(d), zeile);
        }
        // eine namenlose Struktur ohne Deklarator im Rumpf eines Typs: ein namenloses Glied
        if (variablen.empty() && anonym && r.typ != kKeiner) lege(r, {name, {}, {}, 0}, zeile);
        m_p = ende;
        schliesseAb(r);
    }

    void liesTypedef(const Rahmen& r)
    {
        const int zeile = akt().ort.zeile;
        ++m_p;
        const std::size_t auf = sucheD(m_p, {"{", ";"});
        if (tok(auf).istOp("{"))
        {
            const std::size_t wort = typWortVor(m_p, auf);
            if (wort == kKeiner)
            {
                melde(Schwere::Warnung, tok(auf), "typedef not understood - skipped");
                m_p = sucheD(nachKlammer(auf), {";"});
                nimm(";");
                return;
            }
            m_p = wort;
            const std::size_t index = liesTyp(r);
            const std::size_t ende = sucheD(m_p, {";"});
            for (const Deklarator& d : deklaratorenZu(m_p, ende))
            {
                std::string& name = einheit().typen[index].name;
                if (name.empty() && d.typ.empty() && d.zusatz.empty())
                    name = d.name;  // typedef struct { ... } Name;
                else if (d.name != name || !d.typ.empty())
                {
                    if (name.empty()) name = anonymerName(d.name);
                    einheit().aliase.push_back({d.name, d.typ.empty() ? name : name + " " + d.typ, zeile});
                }
            }
            if (einheit().typen[index].name.empty()) einheit().typen[index].name = anonymerName({});
            m_p = ende;
            nimm(";");
            return;
        }

        const Deklarator d = zerlegeVoll(m_p, auf);
        if (d.name.empty())
            melde(Schwere::Warnung, akt(), "typedef without a name - skipped");
        else
            einheit().aliase.push_back({d.name, d.zusatz.empty() ? d.typ : d.typ + " " + d.zusatz, zeile});
        m_p = auf;
        nimm(";");
    }

    // -----------------------------------------------------------------------
    // Deklarationen: Typ, Funktion oder Variable
    // -----------------------------------------------------------------------

    void liesDeklaration(Rahmen& r)
    {
        const std::size_t anfang = m_p;
        std::size_t halt = sucheD(anfang, {";", "=", "{", "("});
        // `operator=(...)`: dieses `=` gehoert zum Namen
        if (halt > anfang && tok(halt).istOp("=") && m_t[halt - 1].istWort("operator"))
            halt = sucheD(halt + 1, {";", "{", "("});
        if (amEnde(halt))
        {
            melde(Schwere::Warnung, tok(anfang), "declaration is not finished");
            m_p = halt;
            return;
        }
        const Token& h = m_t[halt];
        if (h.istOp("{"))
        {
            const std::size_t wort = typWortVor(anfang, halt);
            if (wort != kKeiner)
            {
                // `PACKED_BEGIN typedef struct ...`: vor dem typedef steht ein Makro
                for (std::size_t i = anfang; i < wort; ++i)
                {
                    if (!m_t[i].istWort("typedef")) continue;
                    m_p = i;
                    liesTypedef(r);
                    return;
                }
                m_p = wort;
                liesVariablenZuTyp(r, liesTyp(r));
                return;
            }
            // `int x{5};` — Anfangswert in geschweiften Klammern
            const std::size_t danach = nachKlammer(halt);
            if (halt >= anfang + 2 && (tok(danach).istOp(";") || tok(danach).istOp(",")))
            {
                liesVariablen(r, anfang);
                return;
            }
            melde(Schwere::Warnung, h, "unexpected '{' - block skipped");
            m_p = danach;
            return;
        }
        if (h.istOp("("))
        {
            liesFunktionOderVariable(r, anfang, halt);
            return;
        }
        if (h.istOp(";") || h.istOp("="))
        {
            liesVariablen(r, anfang);
            return;
        }
        melde(Schwere::Warnung, h, "unexpected '" + std::string(h.text) + "'");
        m_p = halt + 1;
    }

    void liesVariablen(const Rahmen& r, std::size_t anfang)
    {
        const int zeile = tok(anfang).ort.zeile;
        const std::size_t ende = sucheD(anfang, {";"});
        // `struct Name;` kuendigt nur an
        const bool ankuendigung = ende == anfang + 2 && istTypWort(m_t[anfang]);
        if (!ankuendigung)
            for (Deklarator& d : deklarationen(anfang, ende)) lege(r, std::move(d), zeile);
        m_p = ende;
        schliesseAb(r);
    }

    /// Der Name einer Funktion vor der Klammer an Stelle `klammer`.
    struct Kopf
    {
        std::size_t anfang = kKeiner;  ///< wo der Name samt Bereich beginnt; kKeiner = kein Funktionsname
        std::size_t letzter = 0;       ///< wo der Name ohne Bereich beginnt
    };

    [[nodiscard]] Kopf findeKopf(std::size_t anfang, std::size_t klammer) const
    {
        Kopf k;
        bool istOperator = false;
        // `operator==`, `operator[]`, `operator()`, `operator bool` — bis zu sechs Token vor der Klammer
        for (std::size_t i = klammer; i > anfang && i + 6 > klammer && !istOperator; --i)
        {
            if (!m_t[i - 1].istWort("operator")) continue;
            k.letzter = i - 1;
            istOperator = true;
        }
        if (!istOperator)
        {
            if (klammer == anfang || m_t[klammer - 1].art != TokenArt::Bezeichner) return k;
            k.letzter = klammer - 1;
            if (k.letzter > anfang && m_t[k.letzter - 1].istOp("~")) --k.letzter;  // Destruktor
        }
        k.anfang = k.letzter;
        while (k.anfang >= anfang + 2 && m_t[k.anfang - 1].istOp("::") &&
               m_t[k.anfang - 2].art == TokenArt::Bezeichner)
            k.anfang -= 2;
        return k;
    }

    /// Was einer Parameterliste folgen kann, bis zum Rumpf oder `;`:
    /// `const noexcept override`, `-> Typ`, `: glied(wert)`, `= 0`, `__attribute__((...))`.
    [[nodiscard]] std::size_t ueberspringeAnhang(std::size_t q, bool& abstrakt) const
    {
        static constexpr std::string_view kWoerter[] = {"const", "volatile", "noexcept", "override",
                                                        "final", "mutable",  "throw",    "constexpr"};
        for (;;)
        {
            const Token& t = tok(q);
            if (t.art == TokenArt::Bezeichner || t.art == TokenArt::Schluesselwort)
            {
                if (std::find(std::begin(kWoerter), std::end(kWoerter), t.text) != std::end(kWoerter))
                {
                    ++q;
                    if (tok(q).istOp("(")) q = nachKlammer(q);
                    continue;
                }
                if (t.istWort("requires"))
                {
                    q = sucheD(q + 1, {"{", ";"});
                    continue;
                }
                if (t.art == TokenArt::Bezeichner && tok(q + 1).istOp("("))  // __attribute__((...))
                {
                    q = nachKlammer(q + 1);
                    continue;
                }
                return q;
            }
            if (t.istOp("&") || t.istOp("&&"))
                ++q;
            else if (t.istOp("["))  // [[attribut]]
                q = nachKlammer(q);
            else if (t.istOp("->"))  // Rueckgabetyp hinten
                q = sucheD(q + 1, {"{", ";", "="});
            else if (t.istOp(":") && m_cpp)
            {
                // Anfangswerte der Glieder: `: a(1), b{2}` — dann der Rumpf
                ++q;
                for (;;)
                {
                    const std::size_t wert = sucheD(q, {"(", "{", ";"});
                    if (!tok(wert).istOp("(") && !tok(wert).istOp("{")) return wert;
                    q = nachKlammer(wert);
                    if (!tok(q).istOp(",")) return q;
                    ++q;
                }
            }
            else if (t.istOp("="))
            {
                const Token& wert = tok(q + 1);
                if (wert.text == "0" && tok(q + 2).istOp(";"))
                    abstrakt = true;
                else if (!wert.istWort("default") && !wert.istWort("delete"))
                    return q;
                q += 2;
            }
            else
                return q;
        }
    }

    void liesFunktionOderVariable(Rahmen& r, std::size_t anfang, std::size_t klammer)
    {
        // `operator()(...)`: das erste Klammerpaar gehoert zum Namen
        if (klammer > anfang && m_t[klammer - 1].istWort("operator") && tok(klammer + 1).istOp(")") &&
            tok(klammer + 2).istOp("("))
            klammer += 2;

        const Kopf kopf = findeKopf(anfang, klammer);
        if (kopf.anfang == kKeiner)
        {
            liesVariablen(r, anfang);  // etwa `int (*zeiger)(void) = 0;`
            return;
        }
        const std::size_t zu = nachKlammer(klammer);
        bool abstrakt = false;
        const std::size_t q = ueberspringeAnhang(zu, abstrakt);
        const bool nurName = kopf.anfang == anfang;  // kein Typ davor: Makro, ISR(...) oder Konstruktor
        const bool rumpf = tok(q).istOp("{");

        if (!rumpf && !tok(q).istOp(";"))
        {
            if (nurName)
                m_p = zu;  // Aufruf eines Makros ohne Semikolon
            else
                liesVariablen(r, anfang);
            return;
        }

        Funktion f;
        f.name = textVon(kopf.letzter, klammer);
        f.zeile = m_t[kopf.letzter].ort.zeile;
        bool virtuell = false;
        bool statisch = false;
        std::size_t typAnfang = anfang;
        for (std::size_t i = anfang; i < kopf.anfang; ++i)
        {
            virtuell = virtuell || m_t[i].istWort("virtual");
            statisch = statisch || m_t[i].istWort("static");
            // im Rumpf eines Typs sagen diese Woerter nichts ueber den Rueckgabetyp
            const bool ohneTyp = m_t[i].istWort("virtual") || m_t[i].istWort("static") ||
                                 m_t[i].istWort("inline") || m_t[i].istWort("explicit") ||
                                 m_t[i].istWort("constexpr") || m_t[i].istWort("friend");
            if (r.typ != kKeiner && i == typAnfang && ohneTyp) ++typAnfang;
        }
        f.rueckgabe = textVon(typAnfang, kopf.anfang);
        const std::size_t innenEnde = tok(zu - 1).istOp(")") ? zu - 1 : zu;
        for (const Bereich& s : teileD(klammer + 1, innenEnde))
        {
            if (s.second == s.first + 1 && m_t[s.first].istWort("void")) continue;
            Deklarator d = zerlegeVoll(s.first, s.second);
            // was dem Namen folgt ([] oder die Klammern eines Funktionszeigers), gehoert zum Typ
            if (!d.zusatz.empty()) d.typ += d.zusatz;
            f.parameter.push_back({std::move(d.typ), std::move(d.name)});
        }

        if (r.typ != kKeiner)
        {
            // Im Rumpf eines Typs: eine Methode. Ohne Typ davor nur, wenn es der
            // Konstruktor oder Destruktor ist — sonst ist es ein Makro (Q_PROPERTY(...);).
            const std::string& typName = einheit().typen[r.typ].name;
            const bool bauer = f.name == typName || f.name == "~" + typName;
            if (nurName && !bauer && !rumpf)
            {
                m_p = q + 1;
                return;
            }
            einheit().typen[r.typ].methoden.push_back(
                {f.name, f.rueckgabe, f.parameter, r.sicht, virtuell, statisch, abstrakt, f.zeile});
            f.bereich = r.bereich;
        }
        else
        {
            if (nurName && !rumpf)
            {
                m_p = q + 1;  // `MODULE_LICENSE("GPL");`
                return;
            }
            // `void Motor::start()`: der Bereich steht vor dem Namen
            const std::string davor = kopf.letzter > kopf.anfang ? textVon(kopf.anfang, kopf.letzter - 1) : "";
            f.bereich = davor.empty() ? r.bereich : vollerName(r.bereich, davor);
            f.intern = statisch;  // `static` vor einer freien Funktion: nur in dieser Datei
        }

        if (rumpf)
        {
            m_p = q;
            const std::size_t von = akt().ort.versatz;
            f.rumpf = liesBlock();
            m_ruempfe.emplace_back(von, m_p > q ? tok(m_p - 1).ort.versatz + tok(m_p - 1).text.size() : von + 1);
            f.bisZeile = m_p > q ? tok(m_p - 1).ort.zeile : f.zeile;
            einheit().funktionen.push_back(std::move(f));
        }
        else
        {
            if (r.typ == kKeiner) einheit().funktionen.push_back(std::move(f));  // ein Prototyp
            m_p = q + 1;
        }
    }

    // -----------------------------------------------------------------------
    // Anweisungen
    // -----------------------------------------------------------------------

    [[nodiscard]] static AnweisungPtr neu(AnweisungArt art, const Token& bei, std::string text = {})
    {
        auto a = std::make_unique<Anweisung>();
        a->art = art;
        a->text = std::move(text);
        a->zeile = bei.ort.zeile;
        return a;
    }

    void nimmSemikolon()
    {
        if (!nimm(";")) melde(Schwere::Fehler, akt(), "';' is missing");
    }

    /// m_p steht auf `{`.
    AnweisungPtr liesBlock()
    {
        AnweisungPtr block = neu(AnweisungArt::Block, akt());
        ++m_p;
        while (!amEnde(m_p) && !akt().istOp("}"))
        {
            const std::size_t davor = m_p;
            if (AnweisungPtr a = liesAnweisung()) block->kinder.push_back(std::move(a));
            if (m_p == davor) ++m_p;
        }
        if (!nimm("}")) melde(Schwere::Fehler, akt(), "'}' is missing");
        return block;
    }

    /// Der Rumpf von if, else, while, do, for, switch: nie leer (ein `;` wird ein leerer Block).
    AnweisungPtr liesRumpf()
    {
        const Token& bei = akt();
        AnweisungPtr a = liesAnweisung();
        return a ? std::move(a) : neu(AnweisungArt::Block, bei);
    }

    /// `( ... )` — der Text zwischen den Klammern.
    std::string liesBedingung()
    {
        if (!akt().istOp("("))
        {
            melde(Schwere::Fehler, akt(), "'(' expected");
            return {};
        }
        const std::size_t zu = nachKlammer(m_p);
        std::string text = textVon(m_p + 1, tok(zu - 1).istOp(")") ? zu - 1 : zu);
        m_p = zu;
        return text;
    }

    /// Der Text bis zum `;`, das genommen wird.
    std::string liesBisSemikolon()
    {
        const std::size_t ende = suche(m_p, {";"});
        std::string text = textVon(m_p, ende);
        m_p = ende;
        nimmSemikolon();
        return text;
    }

    /// Eine Anweisung; leer bei einem einzelnen `;` und bei dem, was uebergangen wird.
    AnweisungPtr liesAnweisung()
    {
        const Token& t = akt();
        if (t.art == TokenArt::Direktive)
        {
            if (const auto g = m_varianten.find(t.ort.versatz); g != m_varianten.end()) return liesVarianten(*g->second);
            ++m_p;
            return neu(AnweisungArt::Direktive, t, eineZeile(t.text));
        }
        if (t.istOp("{")) return liesBlock();
        if (nimm(";")) return nullptr;

        if (t.istWort("if"))
        {
            ++m_p;
            if (akt().istWort("constexpr")) ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Wenn, t);
            a->text = liesBedingung();
            a->kinder.push_back(liesRumpf());
            // zwischen dem Zweig und seinem else darf Praeprozessor stehen (#ifdef ... else if ...)
            std::size_t q = m_p;
            while (tok(q).art == TokenArt::Direktive) ++q;
            if (tok(q).istWort("else"))
            {
                m_p = q + 1;
                a->sonst = liesRumpf();
            }
            return a;
        }
        if (t.istWort("while"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Solange, t);
            a->text = liesBedingung();
            a->kinder.push_back(liesRumpf());
            return a;
        }
        if (t.istWort("do"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::TueSolange, t);
            a->kinder.push_back(liesRumpf());
            if (akt().istWort("while"))
            {
                ++m_p;
                a->text = liesBedingung();
            }
            else
                melde(Schwere::Fehler, akt(), "'while' of the do loop is missing");
            nimmSemikolon();
            return a;
        }
        if (t.istWort("for"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Fuer, t);
            if (akt().istOp("("))
            {
                const std::size_t zu = nachKlammer(m_p);
                const std::size_t ende = tok(zu - 1).istOp(")") ? zu - 1 : zu;
                const std::size_t s1 = std::min(suche(m_p + 1, {";"}), ende);
                if (s1 == ende)
                {
                    // kein `;`: for (Element : Menge)
                    a->art = AnweisungArt::FuerJedes;
                    a->text = textVon(m_p + 1, ende);
                }
                else
                {
                    a->init = textVon(m_p + 1, s1);
                    const std::size_t s2 = std::min(suche(s1 + 1, {";"}), ende);
                    a->text = textVon(s1 + 1, s2);
                    if (s2 < ende) a->schritt = textVon(s2 + 1, ende);
                }
                m_p = zu;
            }
            else
                melde(Schwere::Fehler, akt(), "'(' expected");
            a->kinder.push_back(liesRumpf());
            return a;
        }
        if (t.istWort("switch"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Auswahl, t);
            a->text = liesBedingung();
            a->kinder.push_back(liesRumpf());
            return a;
        }
        if (t.istWort("case"))
        {
            ++m_p;
            const std::size_t ende = suche(m_p, {":"});
            AnweisungPtr a = neu(AnweisungArt::Fall, t, textVon(m_p, ende));
            m_p = ende;
            if (!nimm(":")) melde(Schwere::Fehler, akt(), "':' after case is missing");
            return a;
        }
        if (t.istWort("default"))
        {
            ++m_p;
            if (!nimm(":")) melde(Schwere::Fehler, akt(), "':' after default is missing");
            return neu(AnweisungArt::Fall, t);
        }
        if (t.istWort("break") || t.istWort("continue"))
        {
            ++m_p;
            nimmSemikolon();
            return neu(t.istWort("break") ? AnweisungArt::Abbruch : AnweisungArt::Fortsetzung, t);
        }
        if (t.istWort("return") || t.istWort("co_return"))
        {
            ++m_p;
            return neu(AnweisungArt::Ruecksprung, t, liesBisSemikolon());
        }
        if (t.istWort("goto"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Sprung, t, std::string(akt().text));
            if (akt().art == TokenArt::Bezeichner) ++m_p;
            nimmSemikolon();
            return a;
        }
        if (t.istWort("else"))
        {
            melde(Schwere::Fehler, t, "'else' without 'if'");
            ++m_p;
            return nullptr;
        }

        // C++
        if (t.istWort("try"))
        {
            ++m_p;
            AnweisungPtr a = neu(AnweisungArt::Versuch, t);
            a->kinder.push_back(liesRumpf());
            while (akt().istWort("catch"))
            {
                const Token& bei = akt();
                ++m_p;
                AnweisungPtr fang = neu(AnweisungArt::Fang, bei);
                fang->text = liesBedingung();
                fang->kinder.push_back(liesRumpf());
                a->kinder.push_back(std::move(fang));
            }
            if (a->kinder.size() == 1) melde(Schwere::Fehler, akt(), "'catch' after try is missing");
            return a;
        }
        if (t.istWort("catch"))
        {
            melde(Schwere::Fehler, t, "'catch' without 'try'");
            ++m_p;
            return nullptr;
        }
        if (t.istWort("throw"))
        {
            ++m_p;
            return neu(AnweisungArt::Wurf, t, liesBisSemikolon());
        }

        if (t.art == TokenArt::Bezeichner && tok(m_p + 1).istOp(":"))
        {
            m_p += 2;
            return neu(AnweisungArt::Marke, t, std::string(t.text));
        }

        // Makros, die wie Anweisungen stehen, aber kein Semikolon tragen
        if (t.art == TokenArt::Bezeichner && tok(m_p + 1).istOp("("))
        {
            const std::size_t zu = nachKlammer(m_p + 1);
            const Token& danach = tok(zu);
            if (danach.istOp("{"))
            {
                // NAME(...) { ... } — SUBCASE, foreach, forever: der Kopf, dann der Block
                AnweisungPtr block = neu(AnweisungArt::Block, t);
                block->kinder.push_back(neu(AnweisungArt::Einfach, t, textVon(m_p, zu)));
                m_p = zu;
                AnweisungPtr innen = liesBlock();
                for (AnweisungPtr& kind : innen->kinder) block->kinder.push_back(std::move(kind));
                return block;
            }
            // Q_UNUSED(x) — und danach eine neue Zeile oder das Ende des Blocks
            const bool neueZeile = danach.art != TokenArt::Operator && danach.ort.zeile > m_t[zu - 1].ort.zeile;
            if (wieEinMakro(t.text) && (danach.istOp("}") || neueZeile))
            {
                AnweisungPtr a = neu(AnweisungArt::Einfach, t, textVon(m_p, zu));
                m_p = zu;
                return a;
            }
        }

        const std::size_t ende = suche(m_p, {";"});
        if (ende == m_p)
        {
            // eine schliessende Klammer ohne oeffnende, oder das Ende
            if (!amEnde(m_p) && !t.istOp("}"))
            {
                melde(Schwere::Fehler, t, "unexpected '" + std::string(t.text) + "'");
                ++m_p;
            }
            return nullptr;
        }
        AnweisungPtr a = neu(AnweisungArt::Einfach, t, textVon(m_p, ende));
        m_p = ende;
        nimmSemikolon();
        return a;
    }

    /// m_p steht auf dem #if der Gruppe: je Zweig seine Anweisungen bis zur naechsten Direktive der Gruppe.
    AnweisungPtr liesVarianten(const Variantengruppe& g)
    {
        AnweisungPtr varianten = neu(AnweisungArt::Varianten, akt(), "#if");
        for (std::size_t z = 0; z < g.bedingungen.size() && akt().ort.versatz == g.direktiven[z]; ++z)
        {
            AnweisungPtr zweig = neu(AnweisungArt::Zweig, akt(), g.bedingungen[z]);
            zweig->gilt = z == g.geltend;
            ++m_p;
            // die Zweige sind geklammert wie ganze Anweisungen (Bedingt.hpp): eine `}` kommt nicht
            while (!amEnde(m_p) && akt().ort.versatz != g.direktiven[z + 1] && !akt().istOp("}"))
            {
                const std::size_t davor = m_p;
                if (AnweisungPtr a = liesAnweisung()) zweig->kinder.push_back(std::move(a));
                if (m_p == davor) ++m_p;
            }
            varianten->kinder.push_back(std::move(zweig));
        }
        if (akt().art == TokenArt::Direktive && akt().ort.versatz == g.direktiven.back()) ++m_p;  // #endif
        if (g.bedingungen.back() != "else")
        {
            AnweisungPtr sonst = neu(AnweisungArt::Zweig, tok(m_p - 1), "else");
            sonst->gilt = g.geltend == g.bedingungen.size();
            varianten->kinder.push_back(std::move(sonst));
        }
        if (!m_nurGeltend) return varianten;

        // nur der Zweig, der gilt (die uebrigen sind ausgeblendet und leer): ein Bereich, wenn er etwas enthaelt
        for (std::size_t z = 0; z < varianten->kinder.size(); ++z)
        {
            AnweisungPtr& zweig = varianten->kinder[z];
            if (!zweig->gilt || zweig->kinder.empty()) continue;
            zweig->art = AnweisungArt::Bedingt;
            zweig->text = volleBedingung(g.bedingungen, z);
            zweig->gilt = false;
            return std::move(zweig);
        }
        return nullptr;
    }

    bool m_cpp;
    int m_anonym = 0;
    std::map<std::size_t, const Variantengruppe*> m_varianten;  ///< nach dem Versatz ihres #if
    bool m_nurGeltend = false;  ///< Varianten::nurGeltend: Bereiche statt Varianten
    std::vector<std::pair<std::size_t, std::size_t>> m_ruempfe;
};

}  // namespace

namespace
{

/// Liest die Gliederung; die Logik der Bedingungen kommt danach (lies).
Ergebnis liesGliederung(std::string_view text, std::string_view einheitName, const lexer::Sprachtabelle& tabelle,
                        Dialekt dialekt, const LeseOptionen& optionen)
{
    Ergebnis ergebnis;
    ergebnis.einheit.name = std::string(einheitName);
    if (optionen.alleZweige)
    {
        CFamilienLeser(text, tabelle, dialekt, ergebnis).liesEinheit();
        return ergebnis;
    }
    // Erst entscheiden, welche Zweige gelten; der Leser sieht nur sie. Die Token
    // des Lesers zeigen in diesen Text: er lebt, bis das Lesen fertig ist.
    if (!optionen.varianten && !optionen.bereiche)
    {
        const std::string geltend = werteBedingteAus(text, tabelle, optionen.definitionen, dialekt.cpp,
                                                     ergebnis.meldungen, ergebnis.unbekannteNamen, nullptr,
                                                     &ergebnis.schalter, &ergebnis.weichen);
        CFamilienLeser(geltend, tabelle, dialekt, ergebnis).liesEinheit();
        return ergebnis;
    }
    // Varianten: erst ausgewertet lesen, um die Ruempfe zu kennen; dann bleiben in
    // ihnen die #if stehen, die ganze Anweisungen schalten. Weil deren Zweige
    // geklammert sind wie ganze Anweisungen, liegen die Ruempfe danach noch dort.
    // Bereiche: dieselben #if, nur ihr geltender Zweig bleibt.
    Varianten varianten;
    varianten.nurGeltend = !optionen.varianten;
    {
        Ergebnis erst;
        const std::string geltend = werteBedingteAus(text, tabelle, optionen.definitionen, dialekt.cpp,
                                                     erst.meldungen, erst.unbekannteNamen);
        CFamilienLeser leser(geltend, tabelle, dialekt, erst);
        leser.liesEinheit();
        varianten.ruempfe = leser.ruempfe();
    }
    const std::string gezeigt = werteBedingteAus(text, tabelle, optionen.definitionen, dialekt.cpp,
                                                 ergebnis.meldungen, ergebnis.unbekannteNamen, &varianten,
                                                 &ergebnis.schalter, &ergebnis.weichen);
    CFamilienLeser(gezeigt, tabelle, dialekt, ergebnis, &varianten).liesEinheit();
    return ergebnis;
}

}  // namespace

Ergebnis lies(std::string_view text, std::string_view einheitName, const lexer::Sprachtabelle& tabelle,
              Dialekt dialekt, const LeseOptionen& optionen)
{
    Ergebnis ergebnis = liesGliederung(text, einheitName, tabelle, dialekt, optionen);
    // je Bedingung mit mindestens zwei Atomen ihre Logik (fuer die Wahrheitstabelle)
    static constexpr std::string_view kZuweisungen[] = {"=",  "+=", "-=", "*=", "/=", "%=",
                                                        "&=", "|=", "^=", "<<=", ">>="};
    static constexpr Ausdrucksregeln kRegeln{.wahl = Ausdrucksregeln::Wahl::Fragezeichen,
                                             .nichtUmfasstVergleich = false,
                                             .zuweisungen = kZuweisungen,
                                             .zeiger = true};
    ergaenzeLogik(ergebnis.einheit, tabelle, kRegeln);
    return ergebnis;
}

}  // namespace c2d::sprache::cfamilie
