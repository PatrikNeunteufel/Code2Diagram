/**
 * @file Ableitung.cpp
 * @brief Vom Codemodell zum Diagramm: Ablauf einer Funktion, Datentypen einer Einheit
 */
#include "c2d/diagramm/Ableitung.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
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

using modell::aufrufeIn;

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
        k.gruppe = rahmen();
        const KnotenId id = m_d.neuerKnoten(std::move(k));
        verbinde(eingang, id);
        return id;
    }

    Enden baueFolge(const std::vector<modell::AnweisungPtr>& folge, Enden eingang)
    {
        for (const modell::AnweisungPtr& a : folge) eingang = baue(*a, std::move(eingang));
        return eingang;
    }

    /// Die Folge in einem Rahmen der Art Bedingung. Der Rahmen entsteht erst mit
    /// seinem ersten Knoten: ein Zweig ohne Knoten bekommt keinen.
    Enden baueImRahmen(const std::string& bedingung, const std::vector<modell::AnweisungPtr>& folge, Enden eingang)
    {
        m_rahmen.push_back({bedingung, kKeine});
        Enden aus = baueFolge(folge, std::move(eingang));
        m_rahmen.pop_back();
        return aus;
    }

    /// Die Gruppe fuer einen neuen Knoten: der innerste offene Rahmen — angelegt, wo er noch fehlt.
    std::size_t rahmen()
    {
        std::size_t eltern = kKeine;
        for (OffenerRahmen& r : m_rahmen)
        {
            if (r.gruppe == kKeine)
            {
                Gruppe g;
                g.art = GruppenArt::Bedingung;
                g.titel = r.bedingung;
                g.eltern = eltern;
                r.gruppe = m_d.neueGruppe(std::move(g));
            }
            eltern = r.gruppe;
        }
        return eltern;
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

            case AnweisungArt::Varianten:
            {
                // je Zweig eine Kante mit seiner Bedingung; ein leerer Zweig fuehrt gleich weiter
                // je Zweig, der Knoten hat, ein Rahmen mit seiner vollen Bedingung
                const KnotenId kopf = knoten(KnotenArt::Variante, a.text, a.zeile, eingang);
                std::vector<std::string> bedingungen;
                for (const modell::AnweisungPtr& zweig : a.kinder) bedingungen.push_back(zweig->text);
                Enden aus;
                for (std::size_t z = 0; z < a.kinder.size(); ++z)
                {
                    const Anweisung& zweig = *a.kinder[z];
                    const Enden hinein{{kopf, zweig.gilt ? zweig.text + " (active)" : zweig.text}};
                    haengeAn(aus, baueImRahmen(modell::volleBedingung(bedingungen, z), zweig.kinder, hinein));
                }
                return aus;
            }

            case AnweisungArt::Bedingt:  // der geltende Zweig eines #if: im Rahmen seiner Bedingung
                return baueImRahmen(a.text, a.kinder, std::move(eingang));

            case AnweisungArt::Zweig:  // ausserhalb von Varianten: wie ein Block
                return baueFolge(a.kinder, std::move(eingang));

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

    struct OffenerRahmen
    {
        std::string bedingung;
        std::size_t gruppe;  ///< kKeine, bis ein Knoten in ihn faellt
    };

    Flussdiagramm& m_d;
    std::vector<OffenerRahmen> m_rahmen;  ///< von aussen nach innen
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
        kasten.zeile = typ.zeile;
        for (const modell::Feld& feld : typ.felder)
            kasten.eintraege.push_back({feld.typ, feld.name, feld.zusatz, zeichen(feld.sicht), feld.zeile});
        for (const modell::Methode& m : typ.methoden)
            kasten.methoden.push_back({m.rueckgabe, m.signatur(), zeichen(m.sicht), m.abstrakt, m.statisch, m.zeile});
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

Strukturdiagramm strukturVon(const modell::Einheit& einheit, std::string_view typ)
{
    const Strukturdiagramm alle = strukturVon(einheit);
    Strukturdiagramm aus;
    std::size_t mitte = alle.kaesten.size();
    for (std::size_t k = 0; k < alle.kaesten.size() && mitte == alle.kaesten.size(); ++k)
        if (alle.kaesten[k].name == typ) mitte = k;
    if (mitte == alle.kaesten.size()) return aus;

    // der Typ zuerst, dann die Nachbarn in der Reihenfolge der Einheit
    std::vector<std::size_t> neu(alle.kaesten.size(), kKeine);
    const auto nimm = [&](std::size_t k) {
        if (neu[k] != kKeine) return;
        neu[k] = aus.kaesten.size();
        aus.kaesten.push_back(alle.kaesten[k]);
    };
    nimm(mitte);
    for (std::size_t k = 0; k < alle.kaesten.size(); ++k)
        for (const Beziehung& b : alle.beziehungen)
            if ((b.von == mitte && b.nach == k) || (b.nach == mitte && b.von == k)) nimm(k);
    for (const Beziehung& b : alle.beziehungen)
        if (neu[b.von] != kKeine && neu[b.nach] != kKeine)
            aus.beziehungen.push_back({neu[b.von], neu[b.nach], b.art, b.beschriftung});
    aus.titel = std::string(typ);
    return aus;
}

// ---------------------------------------------------------------------------
// Zustandsautomaten
// ---------------------------------------------------------------------------

namespace
{

/// Wie oft `V = K` mit K einer Fall-Marke mindestens im switch stehen muss, damit er ein Automat ist.
constexpr std::size_t kMindestUebergaenge = 2;

/// Ohne Leerraum und ohne Klammern, die alles umschliessen: `( p->zustand )` -> `p->zustand`.
std::string normiert(std::string_view text)
{
    std::string aus;
    for (const char c : text)
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') aus += c;
    while (aus.size() >= 2 && aus.front() == '(' && aus.back() == ')')
    {
        // nur, wenn die erste Klammer erst am Ende schliesst
        int tiefe = 0;
        bool umschliesst = true;
        for (std::size_t i = 0; i + 1 < aus.size(); ++i)
        {
            tiefe += aus[i] == '(' ? 1 : aus[i] == ')' ? -1 : 0;
            if (tiefe == 0)
            {
                umschliesst = false;
                break;
            }
        }
        if (!umschliesst) break;
        aus = aus.substr(1, aus.size() - 2);
    }
    return aus;
}

std::string_view getrimmt(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == ';')) text.remove_suffix(1);
    return text;
}

/// `links = rechts` auf oberster Ebene — kein `==`, `<=`, `+=` ...; false = keine Zuweisung.
bool zuweisung(std::string_view text, std::string& links, std::string& rechts)
{
    int tiefe = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '"' || c == '\'') return false;  // ein Literal vor dem `=`: kein einfacher Fall
        if (c == '(' || c == '[' || c == '{') ++tiefe;
        if (c == ')' || c == ']' || c == '}') --tiefe;
        if (c != '=' || tiefe != 0) continue;
        if (i + 1 < text.size() && text[i + 1] == '=') return false;
        if (i > 0 && std::string_view("=!<>+-*/%&|^").find(text[i - 1]) != std::string_view::npos) return false;
        links = normiert(text.substr(0, i));
        rechts = std::string(getrimmt(text.substr(i + 1)));
        return !links.empty() && !rechts.empty();
    }
    return false;
}

/// Das Glied am Ende: `a.b->zustand` -> `zustand`; ohne `.` oder `->` leer.
std::string_view glied(std::string_view ausdruck)
{
    const std::size_t punkt = ausdruck.rfind('.');
    const std::size_t pfeil = ausdruck.rfind("->");
    std::size_t ab = std::string_view::npos;
    if (punkt != std::string_view::npos) ab = punkt + 1;
    if (pfeil != std::string_view::npos && (ab == std::string_view::npos || pfeil + 2 > ab)) ab = pfeil + 2;
    return ab == std::string_view::npos ? std::string_view() : ausdruck.substr(ab);
}

std::string verbunden(const std::vector<std::string>& bedingungen)
{
    std::string aus;
    for (const std::string& b : bedingungen) aus += (aus.empty() ? "" : " && ") + b;
    return aus;
}

/// Eine Zuweisung an eine Variable, unter ihren Bedingungen.
struct Fund
{
    std::string wert;
    std::string bedingung;
    int zeile = 0;
};

/// Geht eine Anweisung durch, haelt die Bedingungen fest, unter denen jede steht,
/// und meldet jede Zuweisung an `ziel`. Den switch `ausser` (samt Inhalt) uebergeht er.
class Sucher
{
public:
    using Treffer = std::function<bool(const std::string& links)>;

    Sucher(Treffer trifft, const Anweisung* ausser) : m_trifft(std::move(trifft)), m_ausser(ausser) {}

    void gehe(const Anweisung& a, std::vector<std::string>& bedingungen, std::vector<Fund>& funde) const
    {
        if (&a == m_ausser) return;
        switch (a.art)
        {
            case AnweisungArt::Einfach:
            {
                std::string links, rechts;
                if (zuweisung(a.text, links, rechts) && m_trifft(links))
                    funde.push_back({rechts, verbunden(bedingungen), a.zeile});
                return;
            }
            case AnweisungArt::Wenn:
            {
                bedingungen.push_back(std::string(getrimmt(a.text)));
                gehe(*a.kinder[0], bedingungen, funde);
                bedingungen.back() = "!(" + std::string(getrimmt(a.text)) + ")";
                if (a.sonst) gehe(*a.sonst, bedingungen, funde);
                bedingungen.pop_back();
                return;
            }
            case AnweisungArt::Solange:
            case AnweisungArt::TueSolange:
            case AnweisungArt::Fuer:
            {
                const bool mit = !getrimmt(a.text).empty();
                if (mit) bedingungen.push_back(std::string(getrimmt(a.text)));
                for (const modell::AnweisungPtr& k : a.kinder) gehe(*k, bedingungen, funde);
                if (mit) bedingungen.pop_back();
                return;
            }
            case AnweisungArt::Auswahl:
            {
                // ein anderer switch darin: je Fall die Bedingung `x == wert`
                const std::string x(getrimmt(a.text));
                const Anweisung& rumpf = *a.kinder[0];
                std::vector<std::string> marken;
                bool davorFall = false;
                const auto schritt = [&](const Anweisung& s) {
                    if (s.art == AnweisungArt::Fall)
                    {
                        if (!davorFall) marken.clear();
                        marken.push_back(s.text.empty() ? std::string("default") : x + " == " + std::string(getrimmt(s.text)));
                        davorFall = true;
                        return;
                    }
                    davorFall = false;
                    std::string fall;
                    for (const std::string& m : marken) fall += (fall.empty() ? "" : " || ") + m;
                    const bool mit = !fall.empty();
                    if (mit) bedingungen.push_back(marken.size() > 1 ? "(" + fall + ")" : fall);
                    gehe(s, bedingungen, funde);
                    if (mit) bedingungen.pop_back();
                };
                if (rumpf.art == AnweisungArt::Block)
                    for (const modell::AnweisungPtr& s : rumpf.kinder) schritt(*s);
                else
                    schritt(rumpf);
                return;
            }
            default:
                for (const modell::AnweisungPtr& k : a.kinder) gehe(*k, bedingungen, funde);
                if (a.sonst) gehe(*a.sonst, bedingungen, funde);
                return;
        }
    }

private:
    Treffer m_trifft;
    const Anweisung* m_ausser;
};

/// Alle switch einer Anweisung, in der Reihenfolge des Texts.
void sammleAuswahl(const Anweisung& a, std::vector<const Anweisung*>& aus)
{
    if (a.art == AnweisungArt::Auswahl) aus.push_back(&a);
    for (const modell::AnweisungPtr& k : a.kinder) sammleAuswahl(*k, aus);
    if (a.sonst) sammleAuswahl(*a.sonst, aus);
}

/// Ein switch als Automat; leer, wenn er keiner ist.
std::optional<Zustandsdiagramm> alsAutomat(const Anweisung& auswahl, const modell::Funktion& funktion,
                                           const modell::Einheit& einheit)
{
    const std::string variable = normiert(auswahl.text);
    if (variable.empty()) return std::nullopt;

    Zustandsdiagramm d;
    d.variable = variable;
    d.funktion = funktion.vollerName();
    d.zeile = auswahl.zeile;
    d.titel = variable + " in " + d.funktion + "()";
    const auto zustand = [&](const std::string& name, int zeile) -> std::size_t {
        for (std::size_t i = 0; i < d.zustaende.size(); ++i)
            if (!d.zustaende[i].aussen && d.zustaende[i].name == name) return i;
        d.zustaende.push_back({name, false, zeile});
        return d.zustaende.size() - 1;
    };

    // die Faelle, je Gruppe von Marken (`case A: case B:`) ihre Anweisungen
    struct Gruppe
    {
        std::vector<std::size_t> marken;
        std::vector<const Anweisung*> anweisungen;
    };
    std::vector<Gruppe> gruppen;
    std::vector<std::string> markenNamen;
    bool davorFall = false;
    const Anweisung& rumpf = *auswahl.kinder[0];
    const auto schritt = [&](const Anweisung& s) {
        if (s.art == AnweisungArt::Fall)
        {
            if (!davorFall) gruppen.emplace_back();
            const std::string name = s.text.empty() ? std::string("default") : normiert(s.text);
            gruppen.back().marken.push_back(zustand(name, s.zeile));
            markenNamen.push_back(name);
            davorFall = true;
            return;
        }
        davorFall = false;
        if (!gruppen.empty()) gruppen.back().anweisungen.push_back(&s);
    };
    if (rumpf.art == AnweisungArt::Block)
        for (const modell::AnweisungPtr& s : rumpf.kinder) schritt(*s);
    else
        schritt(rumpf);
    const auto istMarke = [&](const std::string& wert) {
        return std::find(markenNamen.begin(), markenNamen.end(), wert) != markenNamen.end();
    };

    // die Uebergaenge im switch
    const Sucher imSwitch([&](const std::string& links) { return links == variable; }, nullptr);
    std::size_t aufMarken = 0;
    std::vector<Uebergang> uebergaenge;
    for (const Gruppe& g : gruppen)
    {
        std::vector<Fund> funde;
        std::vector<std::string> bedingungen;
        for (const Anweisung* a : g.anweisungen) imSwitch.gehe(*a, bedingungen, funde);
        for (const Fund& f : funde)
        {
            const std::string wert = normiert(f.wert);
            if (istMarke(wert)) ++aufMarken;
            const std::size_t nach = zustand(wert, 0);
            for (const std::size_t von : g.marken) uebergaenge.push_back({von, nach, f.bedingung, f.zeile});
        }
    }
    if (aufMarken < kMindestUebergaenge) return std::nullopt;

    // von aussen: dieselbe Variable — oder dasselbe Glied auf anderem Weg — bekommt einen Zustand
    const std::string_view sein = glied(variable);
    const Sucher draussen(
        [&](const std::string& links) { return links == variable || (!sein.empty() && glied(links) == sein); }, &auswahl);
    std::size_t aussen = d.zustaende.size();
    for (const modell::Funktion& f : einheit.funktionen)
    {
        if (!f.rumpf) continue;
        std::vector<Fund> funde;
        std::vector<std::string> bedingungen;
        draussen.gehe(*f.rumpf, bedingungen, funde);
        for (const Fund& fund : funde)
        {
            const std::string wert = normiert(fund.wert);
            if (!istMarke(wert)) continue;  // nur, was ein Zustand des Automaten ist
            if (aussen == d.zustaende.size()) d.zustaende.push_back({{}, true, 0});
            const std::string wo = "in " + f.vollerName() + "()";
            uebergaenge.push_back({aussen, zustand(wert, 0), fund.bedingung.empty() ? wo : wo + ": " + fund.bedingung,
                                   fund.zeile});
        }
    }

    // gleiche Wege zusammen: ihre Bedingungen mit `||`, ein Weg ohne Bedingung gilt immer
    for (const Uebergang& u : uebergaenge)
    {
        const auto gleich = std::find_if(d.uebergaenge.begin(), d.uebergaenge.end(),
                                         [&](const Uebergang& x) { return x.von == u.von && x.nach == u.nach; });
        if (gleich == d.uebergaenge.end())
        {
            d.uebergaenge.push_back(u);
            continue;
        }
        if (gleich->bedingung.empty() || u.bedingung.empty())
            gleich->bedingung.clear();
        else if (gleich->bedingung != u.bedingung)
            gleich->bedingung += " || " + u.bedingung;
    }
    return d;
}

}  // namespace

std::vector<Zustandsdiagramm> automatenVon(const modell::Einheit& einheit)
{
    std::vector<Zustandsdiagramm> aus;
    for (const modell::Funktion& f : einheit.funktionen)
    {
        if (!f.rumpf) continue;
        std::vector<const Anweisung*> auswahlen;
        sammleAuswahl(*f.rumpf, auswahlen);
        for (const Anweisung* a : auswahlen)
            if (std::optional<Zustandsdiagramm> d = alsAutomat(*a, f, einheit)) aus.push_back(std::move(*d));
    }
    return aus;
}

}  // namespace c2d::diagramm
