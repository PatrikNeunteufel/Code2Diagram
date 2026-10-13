/**
 * @file Ableitung.cpp
 * @brief Vom Codemodell zum Diagramm: Ablauf einer Funktion, Datentypen einer Einheit
 */
#include "c2d/diagramm/Ableitung.hpp"

#include <algorithm>
#include <map>
#include <string_view>
#include <utility>

namespace c2d::diagramm
{

std::string Eintrag::text() const
{
    return modell::deklaration(typ, name, zusatz);
}

std::string Methode::text() const
{
    return modell::deklaration(rueckgabe, signatur);
}

namespace
{

using modell::Anweisung;
using modell::AnweisungArt;

bool istNamenZeichen(char c)
{
    return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/// Ist der Text genau ein Aufruf — `name(...)` und nichts davor oder danach?
bool istAufruf(std::string_view text)
{
    std::size_t i = 0;
    while (i < text.size() && istNamenZeichen(text[i])) ++i;
    if (i == 0 || (text[0] >= '0' && text[0] <= '9')) return false;
    if (text.substr(0, i) == "sizeof") return false;
    while (i < text.size() && text[i] == ' ') ++i;
    if (i >= text.size() || text[i] != '(') return false;
    int tiefe = 0;
    for (; i < text.size(); ++i)
    {
        if (text[i] == '(') ++tiefe;
        if (text[i] == ')' && --tiefe == 0) return i + 1 == text.size();
    }
    return false;
}

/// Die Funktionen, die ein Text ruft: jeder Name vor einer `(`, ohne Bereich
/// und Objekt davor (`a::b(`, `x.b(`, `p->b(` geben `b`), jeder einmal.
/// Woerter der Sprache, die so aussehen (`if (`, `sizeof(`), zaehlen nicht.
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
        if (std::find(aus.begin(), aus.end(), name) == aus.end()) aus.emplace_back(name);
    }
    return aus;
}

/// Offene Enden: Kanten, die noch kein Ziel haben — (von, Beschriftung).
using Enden = std::vector<std::pair<KnotenId, std::string>>;

void haengeAn(Enden& ziel, Enden quelle)
{
    for (auto& ende : quelle) ziel.push_back(std::move(ende));
}

class FlussBauer
{
public:
    explicit FlussBauer(Flussdiagramm& diagramm) : m_d(diagramm) {}

    void baue(const modell::Funktion& funktion)
    {
        // Start und Ende ueber neuerKnoten: die Signatur ist kein Aufruf
        const KnotenId start = m_d.neuerKnoten(KnotenArt::Start, funktion.signatur(), funktion.zeile);
        Enden offen = baue(*funktion.rumpf, {{start, {}}});

        for (const auto& [marke, enden] : m_spruenge)
            if (const auto ziel = m_marken.find(marke); ziel != m_marken.end()) verbinde(enden, ziel->second);

        const KnotenId ende = m_d.neuerKnoten(KnotenArt::Ende, "End");
        verbinde(offen, ende);
        verbinde(m_ruecksprunge, ende);
    }

private:
    void verbinde(const Enden& enden, KnotenId ziel)
    {
        for (const auto& [von, text] : enden) m_d.verbinde(von, ziel, text);
    }

    KnotenId knoten(KnotenArt art, std::string_view text, int zeile, const Enden& eingang)
    {
        Knoten k;
        k.art = art;
        k.zeilen.emplace_back(text);
        k.quellZeile = zeile;
        k.aufrufe = aufrufeIn(text);
        const KnotenId id = m_d.neuerKnoten(std::move(k));
        verbinde(eingang, id);
        return id;
    }

    Enden baueFolge(const std::vector<modell::AnweisungPtr>& folge, Enden eingang)
    {
        for (const modell::AnweisungPtr& a : folge) eingang = baue(*a, std::move(eingang));
        return eingang;
    }

    /// Baut die Anweisung hinter die offenen Enden `eingang`; liefert, was danach offen ist.
    Enden baue(const Anweisung& a, Enden eingang)
    {
        switch (a.art)
        {
            case AnweisungArt::Einfach:
                return {{knoten(istAufruf(a.text) ? KnotenArt::Aufruf : KnotenArt::Anweisung, a.text, a.zeile,
                                eingang),
                         {}}};

            case AnweisungArt::Block:
                return baueFolge(a.kinder, std::move(eingang));

            case AnweisungArt::Wenn:
            {
                const KnotenId kopf = knoten(KnotenArt::Verzweigung, a.text, a.zeile, eingang);
                Enden aus = baue(*a.kinder[0], {{kopf, "yes"}});
                if (a.sonst)
                    haengeAn(aus, baue(*a.sonst, {{kopf, "no"}}));
                else
                    aus.emplace_back(kopf, "no");
                return aus;
            }

            case AnweisungArt::Solange:
            {
                const KnotenId kopf = knoten(KnotenArt::Verzweigung, a.text, a.zeile, eingang);
                Schleife schleife(*this);
                verbinde(baue(*a.kinder[0], {{kopf, "yes"}}), kopf);
                verbinde(schleife.fortsetzung, kopf);
                Enden aus{{kopf, "no"}};
                haengeAn(aus, std::move(schleife.abbruch));
                return aus;
            }

            case AnweisungArt::TueSolange:
            {
                const KnotenId erster = m_d.knoten().size();  // der naechste Knoten ist der Einstieg
                Schleife schleife(*this);
                Enden aus = baue(*a.kinder[0], std::move(eingang));
                haengeAn(aus, std::move(schleife.fortsetzung));
                const KnotenId kopf = knoten(KnotenArt::Verzweigung, a.text, a.zeile, aus);
                m_d.verbinde(kopf, erster, "yes");  // ein leerer Rumpf: erster == kopf
                Enden danach{{kopf, "no"}};
                haengeAn(danach, std::move(schleife.abbruch));
                return danach;
            }

            case AnweisungArt::Fuer:
            {
                if (!a.init.empty()) eingang = {{knoten(KnotenArt::Anweisung, a.init, a.zeile, eingang), {}}};
                Schleife schleife(*this);
                if (!a.text.empty())
                {
                    const KnotenId kopf = knoten(KnotenArt::Verzweigung, a.text, a.zeile, eingang);
                    Enden aus = baue(*a.kinder[0], {{kopf, "yes"}});
                    haengeAn(aus, std::move(schleife.fortsetzung));
                    if (!a.schritt.empty()) aus = {{knoten(KnotenArt::Anweisung, a.schritt, a.zeile, aus), {}}};
                    verbinde(aus, kopf);
                    Enden danach{{kopf, "no"}};
                    haengeAn(danach, std::move(schleife.abbruch));
                    return danach;
                }
                // ohne Bedingung: der Rumpf laeuft, bis ein break ihn verlaesst
                const KnotenId erster = m_d.knoten().size();
                Enden aus = baue(*a.kinder[0], std::move(eingang));
                haengeAn(aus, std::move(schleife.fortsetzung));
                if (!a.schritt.empty()) aus = {{knoten(KnotenArt::Anweisung, a.schritt, a.zeile, aus), {}}};
                if (erster < m_d.knoten().size()) verbinde(aus, erster);
                return std::move(schleife.abbruch);
            }

            case AnweisungArt::Auswahl:
            {
                const KnotenId kopf = knoten(KnotenArt::Verzweigung, a.text, a.zeile, eingang);
                // ein continue im switch gehoert der Schleife darum: nur break wird hier gefangen
                Enden abbruch;
                m_abbruch.push_back(&abbruch);
                Enden offen;
                bool hatStandard = false;
                bool davorFall = false;
                const Anweisung& rumpf = *a.kinder[0];
                const auto schritt = [&](const Anweisung& s) {
                    if (s.art != AnweisungArt::Fall)
                    {
                        offen = baue(s, std::move(offen));
                        davorFall = false;
                        return;
                    }
                    const std::string wert = s.text.empty() ? "default" : s.text;
                    hatStandard = hatStandard || s.text.empty();
                    // `case 1: case 2:` nebeneinander: eine Kante "1, 2"
                    if (davorFall)
                        offen.back().second += ", " + wert;
                    else
                        offen.emplace_back(kopf, wert);
                    davorFall = true;
                };
                if (rumpf.art == AnweisungArt::Block)
                    for (const modell::AnweisungPtr& s : rumpf.kinder) schritt(*s);
                else
                    schritt(rumpf);
                m_abbruch.pop_back();
                haengeAn(offen, std::move(abbruch));
                if (!hatStandard) offen.emplace_back(kopf, "otherwise");
                return offen;
            }

            case AnweisungArt::FuerJedes:
            {
                const KnotenId kopf = knoten(KnotenArt::Verzweigung, a.text, a.zeile, eingang);
                Schleife schleife(*this);
                verbinde(baue(*a.kinder[0], {{kopf, "each"}}), kopf);
                verbinde(schleife.fortsetzung, kopf);
                Enden aus{{kopf, "done"}};
                haengeAn(aus, std::move(schleife.abbruch));
                return aus;
            }

            case AnweisungArt::Versuch:
            {
                // ein Knoten `try`: ohne Beschriftung in den Block, je Fang eine Kante in dessen Block
                const KnotenId kopf = knoten(KnotenArt::Verzweigung, "try", a.zeile, eingang);
                Enden aus;
                for (std::size_t i = 0; i < a.kinder.size(); ++i)
                {
                    const Anweisung& kind = *a.kinder[i];
                    if (i == 0)
                        haengeAn(aus, baue(kind, {{kopf, {}}}));
                    else if (kind.art == AnweisungArt::Fang && !kind.kinder.empty())
                        haengeAn(aus, baue(*kind.kinder[0], {{kopf, "catch (" + kind.text + ")"}}));
                }
                return aus;
            }

            case AnweisungArt::Wurf:  // verlaesst die Funktion wie ein return
                m_ruecksprunge.emplace_back(
                    knoten(KnotenArt::Anweisung, a.text.empty() ? "throw" : "throw " + a.text, a.zeile, eingang),
                    std::string());
                return {};

            case AnweisungArt::Ruecksprung:
                m_ruecksprunge.emplace_back(
                    knoten(KnotenArt::Anweisung, a.text.empty() ? "return" : "return " + a.text, a.zeile, eingang),
                    std::string());
                return {};

            case AnweisungArt::Abbruch:
                if (!m_abbruch.empty()) haengeAn(*m_abbruch.back(), std::move(eingang));
                return {};

            case AnweisungArt::Fortsetzung:
                if (!m_fortsetzung.empty()) haengeAn(*m_fortsetzung.back(), std::move(eingang));
                return {};

            case AnweisungArt::Sprung:
                m_spruenge.emplace_back(a.text, std::move(eingang));
                return {};

            case AnweisungArt::Marke:
            {
                const KnotenId id = knoten(KnotenArt::Anweisung, a.text + ":", a.zeile, eingang);
                m_marken[a.text] = id;
                return {{id, {}}};
            }

            case AnweisungArt::Fall:       // ausserhalb eines switch-Blocks: ohne Wirkung
            case AnweisungArt::Fang:       // ausserhalb eines try: ohne Wirkung
            case AnweisungArt::Direktive:  // Praeprozessor im Rumpf: kein Knoten
                break;
        }
        return eingang;
    }

    /// Faengt break und continue einer Schleife, solange das Objekt lebt.
    struct Schleife
    {
        explicit Schleife(FlussBauer& bauer) : m_bauer(bauer)
        {
            m_bauer.m_abbruch.push_back(&abbruch);
            m_bauer.m_fortsetzung.push_back(&fortsetzung);
        }
        ~Schleife()
        {
            m_bauer.m_abbruch.pop_back();
            m_bauer.m_fortsetzung.pop_back();
        }
        Schleife(const Schleife&) = delete;
        Schleife& operator=(const Schleife&) = delete;

        Enden abbruch;
        Enden fortsetzung;

    private:
        FlussBauer& m_bauer;
    };

    Flussdiagramm& m_d;
    std::vector<Enden*> m_abbruch;
    std::vector<Enden*> m_fortsetzung;
    Enden m_ruecksprunge;
    std::map<std::string, KnotenId> m_marken;
    std::vector<std::pair<std::string, Enden>> m_spruenge;
};

/// Die Bezeichner in einem Typ-Text, etwa `struct Punkt *` -> struct, Punkt.
std::vector<std::string_view> namenIn(std::string_view text)
{
    std::vector<std::string_view> namen;
    std::size_t i = 0;
    while (i < text.size())
    {
        if (!istNamenZeichen(text[i]))
        {
            ++i;
            continue;
        }
        const std::size_t anfang = i;
        while (i < text.size() && istNamenZeichen(text[i])) ++i;
        namen.push_back(text.substr(anfang, i - anfang));
    }
    return namen;
}

char zeichen(modell::Sichtbarkeit sicht)
{
    switch (sicht)
    {
        case modell::Sichtbarkeit::Oeffentlich: return '+';
        case modell::Sichtbarkeit::Geschuetzt: return '#';
        case modell::Sichtbarkeit::Privat: return '-';
        case modell::Sichtbarkeit::Keine: break;
    }
    return '\0';
}

KastenArt kastenArt(modell::TypArt art)
{
    switch (art)
    {
        case modell::TypArt::Vereinigung: return KastenArt::Vereinigung;
        case modell::TypArt::Aufzaehlung: return KastenArt::Aufzaehlung;
        case modell::TypArt::Klasse: return KastenArt::Klasse;
        case modell::TypArt::Struktur: break;
    }
    return KastenArt::Struktur;
}

}  // namespace

Flussdiagramm flussVon(const modell::Funktion& funktion)
{
    Flussdiagramm diagramm(funktion.name);
    if (funktion.rumpf) FlussBauer(diagramm).baue(funktion);
    return diagramm;
}

Strukturdiagramm strukturVon(const modell::Einheit& einheit)
{
    Strukturdiagramm diagramm;
    diagramm.titel = einheit.name;

    std::map<std::string_view, std::size_t> kastenVon;  // Name (ohne Bereich) oder Alias -> Kasten
    for (const modell::Typ& typ : einheit.typen)
    {
        Kasten kasten;
        kasten.art = kastenArt(typ.art);
        kasten.name = typ.vollerName();
        for (const modell::Feld& feld : typ.felder)
            kasten.eintraege.push_back({feld.typ, feld.name, feld.zusatz, zeichen(feld.sicht)});
        for (const modell::Methode& m : typ.methoden)
            kasten.methoden.push_back({m.rueckgabe, m.signatur(), zeichen(m.sicht), m.abstrakt, m.statisch});
        kastenVon.emplace(typ.name, diagramm.kaesten.size());
        diagramm.kaesten.push_back(std::move(kasten));
    }
    // ein typedef auf einen Kasten ist ein zweiter Name fuer ihn — auch ueber mehrere Stufen
    for (std::size_t runde = 0; runde <= einheit.aliase.size(); ++runde)
    {
        bool neu = false;
        for (const modell::Alias& alias : einheit.aliase)
        {
            if (kastenVon.contains(alias.name)) continue;
            for (const std::string_view name : namenIn(alias.ziel))
            {
                if (const auto k = kastenVon.find(name); k != kastenVon.end())
                {
                    kastenVon.emplace(alias.name, k->second);
                    neu = true;
                    break;
                }
            }
        }
        if (!neu) break;
    }

    const auto ziel = [&](std::string_view typ) -> std::size_t {
        for (const std::string_view name : namenIn(typ))
            if (const auto k = kastenVon.find(name); k != kastenVon.end()) return k->second;
        return diagramm.kaesten.size();
    };

    for (std::size_t von = 0; von < einheit.typen.size(); ++von)
    {
        const modell::Typ& typ = einheit.typen[von];
        for (const std::string& basis : typ.basen)
            if (const std::size_t nach = ziel(basis); nach < diagramm.kaesten.size())
                diagramm.beziehungen.push_back({von, nach, BeziehungArt::Erbt, {}});
        for (const modell::Feld& feld : typ.felder)
        {
            const std::size_t nach = ziel(feld.typ);
            if (nach >= diagramm.kaesten.size()) continue;
            // ein Zeiger, eine Referenz oder ein kluger Zeiger: der Typ gehoert jemand anderem
            const bool zeiger = feld.typ.find_first_of("*&") != std::string::npos ||
                                feld.typ.find("_ptr") != std::string::npos;
            const bool aufzaehlung = diagramm.kaesten[nach].art == KastenArt::Aufzaehlung;
            diagramm.beziehungen.push_back(
                {von, nach, zeiger || aufzaehlung ? BeziehungArt::Verweist : BeziehungArt::Enthaelt, feld.name});
        }
    }
    return diagramm;
}

}  // namespace c2d::diagramm
