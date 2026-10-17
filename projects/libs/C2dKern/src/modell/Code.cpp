/**
 * @file Code.cpp
 * @brief Das Codemodell
 */
#include "c2d/modell/Code.hpp"

#include <algorithm>
#include <iterator>

namespace c2d::modell
{

std::string deklaration(const std::string& typ, const std::string& name, const std::string& zusatz, bool typHinten)
{
    if (typHinten)
    {
        std::string s = name;
        if (!typ.empty()) s += (s.empty() ? "" : ": ") + typ;
        if (!zusatz.empty()) s += (s.empty() ? "" : " ") + zusatz;
        return s;
    }
    std::string s = typ;
    if (!name.empty())
    {
        // Sterne, die vom Typ abgesetzt stehen (`Knoten *`), gehoeren zum Namen;
        // Sterne am Typ (`QWidget*`) bleiben dort, der Name folgt mit Abstand.
        std::size_t sterne = s.size();
        while (sterne > 0 && s[sterne - 1] == '*') --sterne;
        const bool amNamen = !s.empty() && (s.back() == '(' || (sterne < s.size() && (sterne == 0 || s[sterne - 1] == ' ' ||
                                                                                  s[sterne - 1] == '(')));
        if (!s.empty() && !amNamen) s += ' ';
        s += name;
    }
    if (!zusatz.empty())
    {
        if (!s.empty() && zusatz.front() != '[' && zusatz.front() != ')') s += ' ';
        s += zusatz;
    }
    return s;
}

std::string vollerName(const std::string& bereich, const std::string& name)
{
    return bereich.empty() ? name : bereich + "::" + name;
}

namespace
{

/// `defined(A)` -> `!defined(A)`, `!defined(A)` -> `defined(A)`, `A` -> `!A`, sonst `!(...)`.
std::string verneint(const std::string& bedingung)
{
    const auto einfach = [](std::string_view s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
            return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        });
    };
    const auto definiert = [&](std::string_view s) {
        return s.starts_with("defined(") && s.ends_with(")") && einfach(s.substr(8, s.size() - 9));
    };
    if (definiert(bedingung)) return "!" + bedingung;
    if (bedingung.starts_with('!') && definiert(std::string_view(bedingung).substr(1))) return bedingung.substr(1);
    if (einfach(bedingung)) return "!" + bedingung;
    return "!(" + bedingung + ")";
}

}  // namespace

std::string volleBedingung(const std::vector<std::string>& bedingungen, std::size_t zweig)
{
    std::vector<std::string> teile;
    for (std::size_t i = 0; i < zweig && i < bedingungen.size(); ++i) teile.push_back(verneint(bedingungen[i]));
    if (zweig < bedingungen.size() && bedingungen[zweig] != "else") teile.push_back(bedingungen[zweig]);
    std::string aus;
    for (const std::string& t : teile)
    {
        // `A || B` hinter einem `&&` braucht Klammern
        const bool klammern = teile.size() > 1 && (t.find("||") != std::string::npos || t.find('?') != std::string::npos) &&
                              !t.starts_with("!(");
        aus += (aus.empty() ? "" : " && ") + (klammern ? "(" + t + ")" : t);
    }
    return aus;
}

namespace
{

bool istNamenZeichen(char c)
{
    return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

void sammle(const Anweisung& a, std::vector<std::string>& aus)
{
    const auto dazu = [&](std::string_view text) {
        for (std::string& name : aufrufeIn(text))
            if (std::find(aus.begin(), aus.end(), name) == aus.end()) aus.push_back(std::move(name));
    };
    if (a.art != AnweisungArt::Fall && a.art != AnweisungArt::Marke && a.art != AnweisungArt::Sprung &&
        a.art != AnweisungArt::Direktive && a.art != AnweisungArt::Varianten && a.art != AnweisungArt::Zweig &&
        a.art != AnweisungArt::Bedingt)
        dazu(a.text);
    dazu(a.init);
    dazu(a.schritt);
    for (const AnweisungPtr& k : a.kinder) sammle(*k, aus);
    if (a.sonst) sammle(*a.sonst, aus);
}

}  // namespace

std::vector<std::string> aufrufeIn(std::string_view text)
{
    static constexpr std::string_view kKeineFunktion[] = {
        "if",     "while",   "for",     "switch",   "return", "sizeof",  "catch",    "throw",
        "new",    "delete",  "alignof", "decltype", "typeid", "defined", "co_return", "case",
        "static_cast", "dynamic_cast", "const_cast", "reinterpret_cast", "noexcept", "alignas",
        // Woerter von Python, die im Text einer Anweisung vor `(` stehen koennen (`not (a)`, `del (x)`)
        "not", "and", "or", "in", "is", "assert", "del", "yield"};
    std::vector<std::string> aus;
    std::string_view davor;      // der Name vor dem laufenden
    std::size_t davorEnde = 0;   // wo er endet
    std::size_t i = 0;
    while (i < text.size())
    {
        if (text[i] == '"' || text[i] == '\'')  // in einem Literal wird nichts gerufen
        {
            const char ende = text[i++];
            while (i < text.size() && text[i] != ende) i += text[i] == '\\' ? 2 : 1;
            ++i;
            continue;
        }
        if (!istNamenZeichen(text[i]) || (text[i] >= '0' && text[i] <= '9'))
        {
            // eine Zahl samt Anhang (0x1Fu) ist kein Name
            if (i < text.size() && text[i] >= '0' && text[i] <= '9')
                while (i < text.size() && istNamenZeichen(text[i])) ++i;
            else
                ++i;
            continue;
        }
        const std::size_t anfang = i;
        while (i < text.size() && istNamenZeichen(text[i])) ++i;
        const std::string_view name = text.substr(anfang, i - anfang);
        // gleich hinter `def`/`class` (Python, nur Leerraum dazwischen) wird definiert, nicht gerufen
        const std::string_view zwischen = text.substr(davorEnde, anfang - davorEnde);
        const bool definiert = (davor == "def" || davor == "class") && !zwischen.empty() &&
                               zwischen.find_first_not_of(' ') == std::string_view::npos;
        davor = name;
        davorEnde = i;
        std::size_t k = i;
        while (k < text.size() && text[k] == ' ') ++k;
        if (k >= text.size() || text[k] != '(' || definiert) continue;
        if (std::find(std::begin(kKeineFunktion), std::end(kKeineFunktion), name) != std::end(kKeineFunktion)) continue;
        // ein Bereich davor gehoert dazu (`N::q`, `a::N::q`); `::q` meint den globalen
        std::size_t vorn = anfang;
        while (vorn >= 2 && text.substr(vorn - 2, 2) == "::")
        {
            std::size_t w = vorn - 2;
            while (w > 0 && istNamenZeichen(text[w - 1])) --w;
            if (w == vorn - 2) break;  // `::q` ohne Namen davor
            vorn = w;
        }
        const std::string_view voll = text.substr(vorn, i - vorn);
        if (std::find(aus.begin(), aus.end(), voll) == aus.end()) aus.emplace_back(voll);
    }
    return aus;
}

std::vector<std::string> bezeichnerIn(std::string_view text)
{
    std::vector<std::string> aus;
    std::size_t i = 0;
    while (i < text.size())
    {
        if (text[i] == '"' || text[i] == '\'')  // ein Literal nennt keinen Typ
        {
            const char ende = text[i++];
            while (i < text.size() && text[i] != ende) i += text[i] == '\\' ? 2 : 1;
            ++i;
            continue;
        }
        if (!istNamenZeichen(text[i]) || (text[i] >= '0' && text[i] <= '9'))
        {
            if (text[i] >= '0' && text[i] <= '9')  // eine Zahl samt Anhang (0x1Fu) ist kein Name
                while (i < text.size() && istNamenZeichen(text[i])) ++i;
            else
                ++i;
            continue;
        }
        // ein Name, mit allem, was ueber `::` an ihm haengt
        const std::size_t anfang = i;
        while (true)
        {
            while (i < text.size() && istNamenZeichen(text[i])) ++i;
            if (i + 2 < text.size() && text.substr(i, 2) == "::" && istNamenZeichen(text[i + 2]))
                i += 2;
            else
                break;
        }
        std::string name(text.substr(anfang, i - anfang));
        if (std::find(aus.begin(), aus.end(), name) == aus.end()) aus.push_back(std::move(name));
    }
    return aus;
}

std::vector<std::string> aufrufeVon(const Funktion& funktion)
{
    std::vector<std::string> aus;
    if (funktion.rumpf) sammle(*funktion.rumpf, aus);
    return aus;
}

namespace
{

/// Wie sammle(), mit der Bedingung, unter der eine Anweisung steht.
void sammleStellen(const Anweisung& a, const std::string& bedingung, bool gilt, std::vector<Aufrufstelle>& aus)
{
    const auto dazu = [&](std::string_view text) {
        for (std::string& name : aufrufeIn(text))
        {
            const bool schon = std::any_of(aus.begin(), aus.end(), [&](const Aufrufstelle& s) {
                return s.name == name && s.bedingung == bedingung && s.gilt == gilt;
            });
            if (!schon) aus.push_back({std::move(name), a.zeile, bedingung, gilt});
        }
    };
    switch (a.art)
    {
        case AnweisungArt::Varianten:
        {
            // ein Zweig, der gilt, aendert nichts; einer, der nicht gilt, steht unter seiner vollen Bedingung
            std::vector<std::string> bedingungen;
            for (const AnweisungPtr& k : a.kinder) bedingungen.push_back(k->text);
            for (std::size_t i = 0; i < a.kinder.size(); ++i)
            {
                const Anweisung& zweig = *a.kinder[i];
                const std::string wann = zweig.gilt ? bedingung : volleBedingung(bedingungen, i);
                for (const AnweisungPtr& k : zweig.kinder) sammleStellen(*k, wann, gilt && zweig.gilt, aus);
            }
            return;
        }
        case AnweisungArt::Bedingt:
            for (const AnweisungPtr& k : a.kinder) sammleStellen(*k, a.text, gilt, aus);
            return;
        case AnweisungArt::Fall:
        case AnweisungArt::Marke:
        case AnweisungArt::Sprung:
        case AnweisungArt::Direktive:
        case AnweisungArt::Zweig: break;
        default: dazu(a.text); break;
    }
    dazu(a.init);
    dazu(a.schritt);
    for (const AnweisungPtr& k : a.kinder) sammleStellen(*k, bedingung, gilt, aus);
    if (a.sonst) sammleStellen(*a.sonst, bedingung, gilt, aus);
}

}  // namespace

std::vector<Aufrufstelle> aufrufstellenVon(const Funktion& funktion)
{
    std::vector<Aufrufstelle> aus;
    if (funktion.rumpf) sammleStellen(*funktion.rumpf, {}, true, aus);
    return aus;
}

bool Funktion::heisst(std::string_view aufruf) const
{
    if (aufruf.find("::") == std::string_view::npos) return name == aufruf;
    const std::string voll = vollerName();
    if (voll == aufruf) return true;
    return voll.size() > aufruf.size() + 2 && voll.ends_with(aufruf) &&
           std::string_view(voll).substr(voll.size() - aufruf.size() - 2, 2) == "::";
}

std::string signatur(const std::string& name, const std::vector<Parameter>& parameter)
{
    std::string s = name + "(";
    for (std::size_t i = 0; i < parameter.size(); ++i)
    {
        if (i != 0) s += ", ";
        s += deklaration(parameter[i].typ, parameter[i].name, {}, parameter[i].typHinten);
    }
    return s + ")";
}

}  // namespace c2d::modell
