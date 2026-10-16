/**
 * @file Sprache.cpp
 * @brief Die Liste der Sprachen
 */
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>

namespace c2d::sprache
{

Definition definitionAus(std::string_view text)
{
    constexpr std::string_view kLeer = " \t\r\n";
    const std::size_t gleich = text.find('=');
    std::string_view name = text.substr(0, gleich);
    const auto stutze = [&] {
        const std::size_t a = name.find_first_not_of(kLeer);
        name = a == std::string_view::npos ? std::string_view() : name.substr(a, name.find_last_not_of(kLeer) - a + 1);
    };
    stutze();
    Definition d;
    if (name.starts_with('!'))
    {
        name.remove_prefix(1);
        stutze();
        d.definiert = false;
        if (gleich != std::string_view::npos) return {"", "1", false};  // eine Aufhebung traegt keinen Wert
    }
    d.name = std::string(name);
    if (gleich != std::string_view::npos) d.wert = std::string(text.substr(gleich + 1));
    return d;
}

std::string alsText(const Definition& definition)
{
    if (!definition.definiert) return "!" + definition.name;
    return definition.wert == "1" ? definition.name : definition.name + "=" + definition.wert;
}

std::optional<std::vector<Definition>> mitForderungen(std::vector<Definition> aussen,
                                                      const std::vector<Forderung>& forderungen,
                                                      const std::vector<Schalter>& schalter,
                                                      const std::vector<Definition>& darunter)
{
    for (const Forderung& f : forderungen)
    {
        const auto gleich = [&](const Definition& x) { return x.name == f.name; };
        const auto s = std::find_if(schalter.begin(), schalter.end(), [&](const Schalter& x) { return x.name == f.name; });
        const bool bekannt = s != schalter.end();
        if (bekannt && (s->inDatei > 0 || s->vorgegeben)) return std::nullopt;  // von aussen nicht zu aendern

        // wie es ohne die von aussen um ihn stuende: das Letzte darunter, sonst der Schalter
        const auto d = std::find_if(aussen.begin(), aussen.end(), gleich);
        const auto u = std::find_if(darunter.rbegin(), darunter.rend(), gleich);
        bool untenDefiniert = false;
        std::string untenWert;
        if (u != darunter.rend())
        {
            untenDefiniert = u->definiert;
            untenWert = u->wert;
        }
        else if (d == aussen.end() && bekannt)
        {
            untenDefiniert = s->definiert;
            untenWert = s->wert;
        }

        if (f.definiert)
        {
            if (d != aussen.end() && d->definiert)
            {
                if (!f.wert.empty()) d->wert = f.wert;
                continue;
            }
            if (d != aussen.end()) aussen.erase(d);  // die Aufhebung `!NAME`
            if (!untenDefiniert || (!f.wert.empty() && untenWert != f.wert))
                aussen.push_back({f.name, f.wert.empty() ? std::string("1") : f.wert});
        }
        else
        {
            if (d != aussen.end()) aussen.erase(d);
            if (untenDefiniert) aussen.push_back({f.name, "1", false});
        }
    }
    return aussen;
}

std::span<const Sprache* const> sprachen()
{
    // Eine neue Sprache hier eintragen.
    static const Sprache* const liste[] = {&c(), &cpp()};
    return liste;
}

const Sprache* spracheFuer(std::string_view dateiname, std::string_view inhalt)
{
    const std::size_t punkt = dateiname.find_last_of('.');
    if (punkt == std::string_view::npos) return nullptr;
    std::string endung(dateiname.substr(punkt + 1));
    std::transform(endung.begin(), endung.end(), endung.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    });
    const Sprache* erste = nullptr;
    for (const Sprache* s : sprachen())
    {
        const auto endungen = s->endungen();
        if (std::find(endungen.begin(), endungen.end(), std::string_view(endung)) == endungen.end()) continue;
        if (s->erkennt(inhalt)) return s;
        if (erste == nullptr) erste = s;
    }
    return erste;
}

}  // namespace c2d::sprache
