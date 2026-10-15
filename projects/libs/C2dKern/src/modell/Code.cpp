/**
 * @file Code.cpp
 * @brief Das Codemodell
 */
#include "c2d/modell/Code.hpp"

#include <algorithm>
#include <iterator>

namespace c2d::modell
{

std::string deklaration(const std::string& typ, const std::string& name, const std::string& zusatz)
{
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
        a.art != AnweisungArt::Direktive)
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
        "static_cast", "dynamic_cast", "const_cast", "reinterpret_cast", "noexcept", "alignas"};
    std::vector<std::string> aus;
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
        std::size_t k = i;
        while (k < text.size() && text[k] == ' ') ++k;
        if (k >= text.size() || text[k] != '(') continue;
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

std::vector<std::string> aufrufeVon(const Funktion& funktion)
{
    std::vector<std::string> aus;
    if (funktion.rumpf) sammle(*funktion.rumpf, aus);
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
        s += deklaration(parameter[i].typ, parameter[i].name);
    }
    return s + ")";
}

}  // namespace c2d::modell
