/**
 * @file Code.cpp
 * @brief Das Codemodell
 */
#include "c2d/modell/Code.hpp"

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
