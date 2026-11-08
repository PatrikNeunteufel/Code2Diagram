/**
 * @file Sequenz.cpp
 * @brief Start der Kerne als Sequenzdiagramm (Entscheide SQ1–SQ4)
 *
 * Je Kern eine Spur: was seine Startfunktion tut, bis in die Hauptschleife — Aufrufe bis zur gewaehlten Tiefe, die IPC
 * aus jeder Tiefe (unterhalb nur flach, je Funktion einmal gerechnet), Fragmente aus if, Schleifen und switch. Dann
 * werden die Spuren an Boot und Barrieren auf oberster Ebene zusammengelegt; was dazwischen geschieht, steht je Kern
 * in einem `par`. Zuletzt je IPC-Handler ein Abschnitt.
 */
#include "c2d/projekt/Sequenz.hpp"

#include "c2d/projekt/Funktionen.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace c2d::projekt
{

namespace
{

using diagramm::FragmentArt;
using diagramm::NachrichtArt;
using diagramm::SequenzSchritt;
using modell::Anweisung;
using modell::AnweisungArt;

constexpr std::size_t kNicht = static_cast<std::size_t>(-1);

/// Was ein Aufruf zwischen Kernen bedeutet (Entscheid SQ3).
enum class Ipc
{
    Keine,
    Boot,       ///< startet den anderen Kern
    Barriere,   ///< beide warten aufeinander
    Signal,     ///< setzt ein Flag beim anderen
    Warten,     ///< wartet auf ein Flag des anderen
    Senden,     ///< eine Nachricht mit Inhalt
    Empfangen,  ///< liest eine
    Teil,       ///< gehoert zu einer Nachricht (Quittung, Loeschen, Pruefen) — kein eigener Pfeil
};

struct IpcRegel
{
    std::string_view name;
    Ipc art;
    int flag;    ///< welches Argument das Flag ist; -1 = keins
    int inhalt;  ///< welches Argument der Inhalt ist (Kommando); -1 = keins
};

/// TI C2000 driverlib (`ipc.h`). Das erste Argument nennt die Richtung (`IPC_CPU1_L_CPU2_R`: von CPU1 an CPU2).
constexpr IpcRegel kIpc[] = {
    {"IPC_setBootMode", Ipc::Boot, -1, -1},
    {"IPC_sync", Ipc::Barriere, 1, -1},
    {"IPC_setFlagLtoR", Ipc::Signal, 1, -1},
    {"IPC_waitForFlag", Ipc::Warten, 1, -1},
    {"IPC_isFlagBusyRtoL", Ipc::Warten, 1, -1},
    {"IPC_sendCommand", Ipc::Senden, 1, 3},
    {"IPC_readCommand", Ipc::Empfangen, 1, -1},
    {"IPC_sendMessageToQueue", Ipc::Senden, -1, 3},
    {"IPC_readMessageFromQueue", Ipc::Empfangen, -1, -1},
    {"IPC_ackFlagRtoL", Ipc::Teil, 1, -1},
    {"IPC_clearFlagLtoR", Ipc::Teil, 1, -1},
    {"IPC_isFlagBusyLtoR", Ipc::Teil, 1, -1},
    {"IPC_waitForAck", Ipc::Teil, 1, -1},
    {"IPC_registerInterrupt", Ipc::Teil, -1, -1},
    {"IPC_unregisterInterrupt", Ipc::Teil, -1, -1},
    {"IPC_initMessageQueue", Ipc::Teil, -1, -1},
};

const IpcRegel* regelFuer(std::string_view name)
{
    for (const IpcRegel& r : kIpc)
        if (r.name == name) return &r;
    return nullptr;
}

bool namensZeichen(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

std::string ohneRand(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) s.remove_suffix(1);
    return std::string(s);
}

/// Die Argumente des naechsten Aufrufs `name(…)` in `text` ab `ab` (danach hinter ihm); nullopt = keiner mehr.
std::optional<std::vector<std::string>> argumente(std::string_view text, std::string_view name, std::size_t& ab)
{
    for (std::size_t p = text.find(name, ab); p != std::string_view::npos; p = text.find(name, p + 1))
    {
        if (p > 0 && namensZeichen(text[p - 1])) continue;
        std::size_t k = p + name.size();
        if (k < text.size() && namensZeichen(text[k])) continue;
        while (k < text.size() && std::isspace(static_cast<unsigned char>(text[k])) != 0) ++k;
        if (k >= text.size() || text[k] != '(') continue;
        std::vector<std::string> aus;
        int tiefe = 0;
        char zeichenkette = 0;
        std::size_t anfang = k + 1;
        for (std::size_t i = k; i < text.size(); ++i)
        {
            const char c = text[i];
            if (zeichenkette != 0)
            {
                if (c == '\\') ++i;
                else if (c == zeichenkette) zeichenkette = 0;
                continue;
            }
            if (c == '"' || c == '\'') zeichenkette = c;
            else if (c == '(' || c == '[' || c == '{') ++tiefe;
            else if (c == ')' || c == ']' || c == '}')
            {
                if (--tiefe == 0)
                {
                    const std::string letztes = ohneRand(text.substr(anfang, i - anfang));
                    if (!letztes.empty() || !aus.empty()) aus.push_back(letztes);
                    ab = i + 1;
                    return aus;
                }
            }
            else if (c == ',' && tiefe == 1)
            {
                aus.push_back(ohneRand(text.substr(anfang, i - anfang)));
                anfang = i + 1;
            }
        }
        ab = text.size();
        return aus;
    }
    return std::nullopt;
}

/// Die Nummer hinter `wort` in `text` (`IPC_FLAG31` → 31, `IPC_INT0` → 0); -1 = keine.
int nummerHinter(std::string_view text, std::string_view wort)
{
    const std::size_t p = text.find(wort);
    if (p == std::string_view::npos) return -1;
    std::size_t i = p + wort.size();
    if (i >= text.size() || std::isdigit(static_cast<unsigned char>(text[i])) == 0) return -1;
    int n = 0;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0) n = n * 10 + (text[i++] - '0');
    return n;
}

int flagVon(std::string_view arg)
{
    if (const int n = nummerHinter(arg, "FLAG"); n >= 0) return n;
    if (!arg.empty() && std::all_of(arg.begin(), arg.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
        return std::stoi(std::string(arg));
    return -1;
}

/// Der andere Kern aus der Richtung: `IPC_CPU1_L_CPU2_R` → `CPU2`; leer = keine erkennbar.
std::string zielVon(std::string_view basis)
{
    const std::size_t l = basis.find("_L_");
    if (l == std::string_view::npos) return {};
    const std::size_t r = basis.find("_R", l + 3);
    if (r == std::string_view::npos) return {};
    return std::string(basis.substr(l + 3, r - l - 3));
}

/// Eine Schleife, die nie endet: `for (;;)`, `while (1)`, `while (true)`.
bool endlos(const Anweisung& a)
{
    if (a.art != AnweisungArt::Solange && a.art != AnweisungArt::Fuer) return false;
    std::string t = ohneRand(a.text);
    while (t.size() >= 2 && t.front() == '(' && t.back() == ')') t = ohneRand(std::string_view(t).substr(1, t.size() - 2));
    if (t.empty()) return a.art == AnweisungArt::Fuer;
    if (t == "true" || t == "True") return true;
    while (!t.empty() && (t.back() == 'u' || t.back() == 'U' || t.back() == 'l' || t.back() == 'L')) t.pop_back();
    return !t.empty() && std::all_of(t.begin(), t.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }) &&
           t.find_first_not_of('0') != std::string::npos;
}

/// Ein Schritt einer Spur (je Kern, vor dem Zusammenlegen).
struct Ereignis
{
    enum class Art
    {
        Aufruf,
        Ipc,
        Anfang,
        Sonst,
        Ende,
    };
    Art art = Art::Aufruf;
    Ipc ipc = Ipc::Keine;
    FragmentArt fragment = FragmentArt::Schleife;
    std::string text;
    int tiefe = 0;
    bool fremd = false;
    int flag = -1;
    std::string ziel;  ///< Ipc: der andere Kern, wie die Richtung ihn nennt; leer = nicht genannt
    std::string in;    ///< Ipc: die Funktion, in der sie steht, wenn ihr Aufruf nicht zu sehen ist
    std::string pfad;
    int zeile = 0;
};
using Spur = std::vector<Ereignis>;

/// Entfernt Fragmente ohne Inhalt (Anfang gleich gefolgt vom Ende) und leere letzte Zweige.
template <typename Liste, typename IstAnfang, typename IstSonst, typename IstEnde>
void ohneLeere(Liste& l, IstAnfang anfang, IstSonst sonst, IstEnde ende)
{
    for (bool weiter = true; weiter;)
    {
        weiter = false;
        for (std::size_t i = 0; i + 1 < l.size(); ++i)
        {
            if (anfang(l[i]) && ende(l[i + 1]))
            {
                l.erase(l.begin() + static_cast<std::ptrdiff_t>(i), l.begin() + static_cast<std::ptrdiff_t>(i) + 2);
                weiter = true;
                break;
            }
            if (sonst(l[i]) && (ende(l[i + 1]) || sonst(l[i + 1])))
            {
                l.erase(l.begin() + static_cast<std::ptrdiff_t>(i));
                weiter = true;
                break;
            }
        }
    }
}

/// Ein FLAG 0 gleich nach dem Boot gehoert zu ihm (der andere Kern wartet im Boot-ROM darauf) — auch in einem Fragment.
void flag0ZumBoot(Spur& s)
{
    for (std::size_t i = 0; i + 1 < s.size(); ++i)
        if (s[i].art == Ereignis::Art::Ipc && s[i].ipc == Ipc::Boot && s[i + 1].art == Ereignis::Art::Ipc &&
            s[i + 1].ipc == Ipc::Signal && s[i + 1].flag == 0)
            s.erase(s.begin() + static_cast<std::ptrdiff_t>(i) + 1);
}

void ohneLeere(Spur& s)
{
    ohneLeere(
        s, [](const Ereignis& e) { return e.art == Ereignis::Art::Anfang; },
        [](const Ereignis& e) { return e.art == Ereignis::Art::Sonst; }, [](const Ereignis& e) { return e.art == Ereignis::Art::Ende; });
}

/// Was eine eigene Huelle um IPC tut (Entscheid SQ3): ihr Rumpf ruft eine IPC mit einem ihrer Parameter als Flag.
struct Huelle
{
    Ipc art = Ipc::Keine;
    std::size_t flagParameter = kNicht;
    std::string ziel;
};

/// Liest die Spuren eines Kerns.
class Spurleser
{
public:
    Spurleser(const KernProjekt& kern, const Funktionsverzeichnis& verzeichnis, int maxTiefe)
        : m_kern(kern), m_fv(verzeichnis), m_max(maxTiefe)
    {
    }

    /// Ab der Startfunktion bis in die Hauptschleife.
    Spur start(const Fundstelle& f)
    {
        Spur s;
        m_ende = false;
        m_stapel = {f};
        const modell::Funktion& fn = funktion(m_kern.projekt, f);
        if (fn.rumpf) anweisung(*fn.rumpf, 0, true, true, f.datei, s);
        flag0ZumBoot(s);
        ohneLeere(s);
        return s;
    }

    /// Ein Handler: sein Rumpf wie der einer Startfunktion, ohne Hauptschleife.
    Spur handler(const Fundstelle& f)
    {
        Spur s;
        m_ende = false;
        m_stapel = {f};
        const modell::Funktion& fn = funktion(m_kern.projekt, f);
        if (fn.rumpf) anweisung(*fn.rumpf, 0, true, false, f.datei, s);
        flag0ZumBoot(s);
        ohneLeere(s);
        return s;
    }

    /// Ist die Funktion eine Huelle um IPC? (einmal gerechnet)
    const Huelle& huelle(const Fundstelle& f)
    {
        const auto schluessel = std::make_pair(f.datei, f.funktion);
        if (const auto it = m_huellen.find(schluessel); it != m_huellen.end()) return it->second;
        Huelle& h = m_huellen[schluessel];
        const modell::Funktion& fn = funktion(m_kern.projekt, f);
        if (!fn.rumpf) return h;
        std::map<std::size_t, std::set<Ipc>> jeParameter;
        std::map<std::size_t, std::string> ziele;
        const auto besuche = [&](const auto& selbst, const Anweisung& a) -> void {
            for (const std::string& text : texteVon(a))
                for (const std::string& n : modell::aufrufeDer(a, text))
                {
                    const IpcRegel* r = regelFuer(n);
                    if (r == nullptr || r->flag < 0) continue;
                    std::size_t ab = 0;
                    while (const auto args = argumente(text, n, ab))
                    {
                        if (static_cast<std::size_t>(r->flag) >= args->size()) continue;
                        const std::string& arg = (*args)[static_cast<std::size_t>(r->flag)];
                        for (std::size_t p = 0; p < fn.parameter.size(); ++p)
                            if (!fn.parameter[p].name.empty() && fn.parameter[p].name == arg)
                            {
                                jeParameter[p].insert(r->art);
                                if (!args->empty()) ziele[p] = zielVon(args->front());
                            }
                    }
                }
            for (const modell::AnweisungPtr& k : a.kinder)
                if (k) selbst(selbst, *k);
            if (a.sonst) selbst(selbst, *a.sonst);
        };
        besuche(besuche, *fn.rumpf);
        for (const auto& [p, arten] : jeParameter)
        {
            Ipc art = Ipc::Keine;
            if (arten.contains(Ipc::Barriere) || (arten.contains(Ipc::Signal) && arten.contains(Ipc::Warten))) art = Ipc::Barriere;
            else if (arten.contains(Ipc::Signal)) art = Ipc::Signal;
            else if (arten.contains(Ipc::Senden)) art = Ipc::Senden;
            else if (arten.contains(Ipc::Warten)) art = Ipc::Warten;
            if (art == Ipc::Keine) continue;
            h = {art, p, ziele[p]};
            break;
        }
        return h;
    }

private:
    /// Die Texte einer Anweisung, in denen Aufrufe stehen koennen: ihr Text, bei `for` auch Anfang und Schritt.
    static std::vector<std::string> texteVon(const Anweisung& a)
    {
        std::vector<std::string> aus;
        if (!a.init.empty()) aus.push_back(a.init);
        if (!a.text.empty() && a.art != AnweisungArt::Fall && a.art != AnweisungArt::Marke && a.art != AnweisungArt::Sprung &&
            a.art != AnweisungArt::Direktive && a.art != AnweisungArt::Zweig && a.art != AnweisungArt::Bedingt)
            aus.push_back(a.text);
        if (!a.schritt.empty()) aus.push_back(a.schritt);
        return aus;
    }

    [[nodiscard]] const std::string& pfadVon(std::size_t datei) const { return m_kern.projekt.dateien[datei].pfad; }

    void fragment(Spur& s, Ereignis::Art art, FragmentArt f, std::string text, std::size_t datei, int zeile)
    {
        Ereignis e;
        e.art = art;
        e.fragment = f;
        e.text = std::move(text);
        e.pfad = pfadVon(datei);
        e.zeile = zeile;
        s.push_back(std::move(e));
    }

    /// Eine Anweisung im Rumpf einer Funktion der Tiefe `tiefe` (die Startfunktion: 0). `sichtbar`: ihre Fragmente und
    /// Aufrufe stehen da; sonst nur ihre IPC. `oben`: auf oberster Ebene der Startfunktion (dort die Hauptschleife).
    void anweisung(const Anweisung& a, int tiefe, bool sichtbar, bool oben, std::size_t datei, Spur& s)
    {
        if (m_ende) return;
        switch (a.art)
        {
            case AnweisungArt::Block:
            case AnweisungArt::Bedingt:
            case AnweisungArt::Mit:
            case AnweisungArt::Versuch:
            case AnweisungArt::Fang:
            case AnweisungArt::Schluss:
                if (a.art == AnweisungArt::Mit) rufe(a, tiefe, sichtbar, datei, s);
                for (const modell::AnweisungPtr& k : a.kinder)
                    if (k) anweisung(*k, tiefe, sichtbar, oben && a.art == AnweisungArt::Block, datei, s);
                return;
            case AnweisungArt::Varianten:
                for (const modell::AnweisungPtr& k : a.kinder)
                    if (k && k->gilt)
                        for (const modell::AnweisungPtr& x : k->kinder)
                            if (x) anweisung(*x, tiefe, sichtbar, false, datei, s);
                return;
            case AnweisungArt::Wenn:
            {
                rufe(a, tiefe, sichtbar, datei, s);
                const FragmentArt f = a.sonst ? FragmentArt::Alternative : FragmentArt::Option;
                if (sichtbar) fragment(s, Ereignis::Art::Anfang, f, a.text, datei, a.zeile);
                if (!a.kinder.empty() && a.kinder[0]) anweisung(*a.kinder[0], tiefe, sichtbar, false, datei, s);
                if (a.sonst)
                {
                    if (sichtbar) fragment(s, Ereignis::Art::Sonst, f, "else", datei, a.sonst->zeile);
                    anweisung(*a.sonst, tiefe, sichtbar, false, datei, s);
                }
                if (sichtbar) fragment(s, Ereignis::Art::Ende, f, {}, datei, a.zeile);
                return;
            }
            case AnweisungArt::Solange:
            case AnweisungArt::TueSolange:
            case AnweisungArt::Fuer:
            case AnweisungArt::FuerJedes:
            {
                const bool haupt = oben && tiefe == 0 && m_hauptschleifeSuchen && endlos(a);
                rufe(a, tiefe, sichtbar, datei, s);
                std::string kopf = a.kopf;
                if (kopf.empty())
                    kopf = a.art == AnweisungArt::Fuer ? (a.init.empty() && a.text.empty() && a.schritt.empty() ? std::string("for (;;)")
                                                                    : "for (" + a.init + "; " + a.text + "; " + a.schritt + ")")
                         : a.art == AnweisungArt::FuerJedes ? "for (" + a.text + ")"
                                                            : "while (" + a.text + ")";
                if (sichtbar || haupt)
                    fragment(s, Ereignis::Art::Anfang, FragmentArt::Schleife, haupt ? "main loop: " + kopf : kopf, datei, a.zeile);
                if (!a.kinder.empty() && a.kinder[0]) anweisung(*a.kinder[0], tiefe, sichtbar, false, datei, s);
                if (sichtbar || haupt) fragment(s, Ereignis::Art::Ende, FragmentArt::Schleife, {}, datei, a.zeile);
                if (haupt) m_ende = true;  // was danach steht, laeuft nie
                return;
            }
            case AnweisungArt::Auswahl:
            {
                rufe(a, tiefe, sichtbar, datei, s);
                bool offen = false;
                const auto faelle = [&](const auto& selbst, const Anweisung& x) -> void {
                    for (const modell::AnweisungPtr& k : x.kinder)
                    {
                        if (!k) continue;
                        if (k->art == AnweisungArt::Fall)
                        {
                            if (sichtbar)
                                fragment(s, offen ? Ereignis::Art::Sonst : Ereignis::Art::Anfang, FragmentArt::Alternative,
                                         k->text.empty() ? "default" : "case " + k->text, datei, k->zeile);
                            offen = true;
                            continue;
                        }
                        if (k->art == AnweisungArt::Block && !offen)
                        {
                            selbst(selbst, *k);
                            continue;
                        }
                        anweisung(*k, tiefe, sichtbar, false, datei, s);
                    }
                };
                for (const modell::AnweisungPtr& k : a.kinder)
                    if (k) faelle(faelle, *k);
                if (sichtbar && offen) fragment(s, Ereignis::Art::Ende, FragmentArt::Alternative, {}, datei, a.zeile);
                return;
            }
            default:
                rufe(a, tiefe, sichtbar, datei, s);
                return;
        }
    }

    /// Die Aufrufe einer Anweisung: IPC wird ein Ereignis, eine Funktion des Projekts ein Aufruf (bis zur Tiefe) und ihr
    /// Rumpf wird weiter gelesen — sichtbar, solange auch ihre Aufrufe zu sehen sind, sonst nur nach IPC.
    void rufe(const Anweisung& a, int tiefe, bool sichtbar, std::size_t datei, Spur& s)
    {
        for (const std::string& text : texteVon(a))
        {
            std::map<std::string, std::size_t> ab;  // je Name: wo der naechste Aufruf gesucht wird
            for (const std::string& n : modell::aufrufeDer(a, text))
            {
                std::size_t& stelle = ab[n];
                const std::optional<std::vector<std::string>> args = argumente(text, n, stelle);
                const std::vector<std::string> argliste = args ? *args : std::vector<std::string>{};
                if (const IpcRegel* r = regelFuer(n))
                {
                    if (r->art != Ipc::Teil) s.push_back(ipcEreignis(r->art, r->flag, r->inhalt, argliste, datei, a.zeile));
                    continue;
                }
                const std::vector<Fundstelle> ziele = m_fv.kandidaten(datei, n);
                const bool zeigen = sichtbar && tiefe + 1 <= m_max;
                if (ziele.size() == 1)
                {
                    const Huelle& h = huelle(ziele.front());
                    if (h.art != Ipc::Keine)
                    {
                        Ereignis e = ipcEreignis(h.art, static_cast<int>(h.flagParameter), -1, argliste, datei, a.zeile);
                        if (e.ziel.empty()) e.ziel = h.ziel;
                        s.push_back(std::move(e));
                        continue;
                    }
                }
                if (zeigen)
                {
                    Ereignis e;
                    e.art = Ereignis::Art::Aufruf;
                    e.text = n + "()";
                    if (ziele.size() > 1) e.text += " (" + std::to_string(ziele.size()) + " functions qualify)";
                    e.tiefe = tiefe + 1;
                    e.fremd = ziele.empty();
                    e.pfad = pfadVon(datei);
                    e.zeile = a.zeile;
                    s.push_back(std::move(e));
                }
                if (ziele.size() != 1) continue;
                const Fundstelle& f = ziele.front();
                if (std::any_of(m_stapel.begin(), m_stapel.end(), [&](const Fundstelle& x) { return x == f; })) continue;  // Rekursion (std::find geht mit der MSVC-STL auf Fundstelle nicht)
                const modell::Funktion& fn = funktion(m_kern.projekt, f);
                if (!fn.rumpf) continue;
                if (zeigen && tiefe + 1 < m_max)
                {
                    m_stapel.push_back(f);
                    const bool suchen = m_hauptschleifeSuchen;
                    m_hauptschleifeSuchen = false;
                    anweisung(*fn.rumpf, tiefe + 1, true, false, f.datei, s);
                    m_hauptschleifeSuchen = suchen;
                    m_stapel.pop_back();
                }
                else
                {
                    const Spur& flach = flachVon(f);
                    for (Ereignis e : flach)
                    {
                        if (e.in.empty()) e.in = fn.vollerName();
                        s.push_back(std::move(e));
                    }
                }
            }
        }
    }

    Ereignis ipcEreignis(Ipc art, int flagArg, int inhaltArg, const std::vector<std::string>& args, std::size_t datei, int zeile)
    {
        Ereignis e;
        e.art = Ereignis::Art::Ipc;
        e.ipc = art;
        if (!args.empty()) e.ziel = zielVon(args.front());
        if (flagArg >= 0 && static_cast<std::size_t>(flagArg) < args.size()) e.flag = flagVon(args[static_cast<std::size_t>(flagArg)]);
        if (art == Ipc::Boot) e.flag = 0;
        if (inhaltArg >= 0 && static_cast<std::size_t>(inhaltArg) < args.size()) e.text = args[static_cast<std::size_t>(inhaltArg)];
        e.pfad = pfadVon(datei);
        e.zeile = zeile;
        return e;
    }

    /// Nur die IPC einer Funktion und allem, was sie ruft (einmal je Funktion gerechnet; Rekursion gibt nichts).
    const Spur& flachVon(const Fundstelle& f)
    {
        const auto schluessel = std::make_pair(f.datei, f.funktion);
        if (const auto it = m_flach.find(schluessel); it != m_flach.end()) return it->second;
        m_flach[schluessel] = {};  // in Arbeit: eine Rekursion sieht nichts
        Spur s;
        const modell::Funktion& fn = funktion(m_kern.projekt, f);
        if (fn.rumpf)
        {
            const bool ende = m_ende;
            const bool suchen = m_hauptschleifeSuchen;
            m_ende = false;
            m_hauptschleifeSuchen = false;
            m_stapel.push_back(f);
            anweisung(*fn.rumpf, m_max + 1, false, false, f.datei, s);
            m_stapel.pop_back();
            m_ende = ende;
            m_hauptschleifeSuchen = suchen;
        }
        std::erase_if(s, [](const Ereignis& e) { return e.art != Ereignis::Art::Ipc; });
        return m_flach[schluessel] = std::move(s);
    }

    const KernProjekt& m_kern;
    const Funktionsverzeichnis& m_fv;
    int m_max;
    bool m_ende = false;
    bool m_hauptschleifeSuchen = true;
    std::vector<Fundstelle> m_stapel;
    std::map<std::pair<std::size_t, std::size_t>, Huelle> m_huellen;
    std::map<std::pair<std::size_t, std::size_t>, Spur> m_flach;
};

/// Die IPC-Handler eines Kerns: je Flag die Funktionen, die `IPC_registerInterrupt(…, IPC_INTn, h)` oder ein Eintrag in
/// die Vektortabelle (`INT_CIPCn` bzw. `INT_IPC_n`, dann `*v = (…)&h`) dafuer eintraegt.
std::map<int, std::vector<std::string>> handlerVon(const Projekt& projekt)
{
    std::map<int, std::vector<std::string>> aus;
    const auto dazu = [&](int flag, std::string name) {
        while (!name.empty() && (name.front() == '&' || name.front() == '(' || std::isspace(static_cast<unsigned char>(name.front())) != 0))
            name.erase(name.begin());
        while (!name.empty() && (name.back() == ')' || std::isspace(static_cast<unsigned char>(name.back())) != 0)) name.pop_back();
        // aus einer Umwandlung `(uint32_t)&h` bleibt der letzte Name
        std::size_t e = name.size();
        while (e > 0 && !namensZeichen(name[e - 1])) --e;
        std::size_t b = e;
        while (b > 0 && namensZeichen(name[b - 1])) --b;
        name = name.substr(b, e - b);
        if (flag < 0 || name.empty()) return;
        std::vector<std::string>& l = aus[flag];
        if (std::find(l.begin(), l.end(), name) == l.end()) l.push_back(name);
    };
    for (const Datei& d : projekt.dateien)
        for (const modell::Funktion& f : d.ergebnis.einheit.funktionen)
        {
            if (!f.rumpf) continue;
            std::vector<const Anweisung*> folge;
            const auto sammle = [&](const auto& selbst, const Anweisung& a) -> void {
                folge.push_back(&a);
                for (const modell::AnweisungPtr& k : a.kinder)
                    if (k) selbst(selbst, *k);
                if (a.sonst) selbst(selbst, *a.sonst);
            };
            sammle(sammle, *f.rumpf);
            int offen = -1;  // ein Flag aus `INT_CIPCn`, dessen Vektor gleich gesetzt wird
            for (const Anweisung* a : folge)
            {
                const std::string& t = a->text;
                std::size_t ab = 0;
                if (const auto args = argumente(t, "IPC_registerInterrupt", ab); args && args->size() >= 3)
                    dazu(nummerHinter((*args)[1], "INT"), (*args)[2]);
                int n = nummerHinter(t, "INT_CIPC");
                if (n < 0) n = nummerHinter(t, "INT_IPC_");
                if (n >= 0)
                {
                    offen = n;
                    continue;
                }
                const std::string ohne = ohneRand(t);
                if (offen >= 0 && ohne.starts_with('*') && ohne.find('=') != std::string::npos)
                {
                    dazu(offen, ohne.substr(ohne.find('=') + 1));
                    offen = -1;
                }
            }
        }
    return aus;
}

/// Legt die Spuren zusammen und schreibt die Schritte.
class Zusammenleger
{
public:
    Zusammenleger(const std::vector<KernProjekt>& kerne, diagramm::Sequenzdiagramm& d) : m_kerne(kerne), m_d(d) {}

    /// Der Kern, den die Richtung meint (`CPU2`); `von` ist der, der ruft. kNicht = keiner.
    [[nodiscard]] std::size_t kernNamens(const std::string& name, std::size_t von) const
    {
        for (std::size_t k = 0; k < m_kerne.size(); ++k)
            if (m_kerne[k].kern.name == name) return k;
        const auto klein = [](std::string s) {
            for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        for (std::size_t k = 0; k < m_kerne.size(); ++k)
            if (!name.empty() && klein(m_kerne[k].kern.name) == klein(name)) return k;
        if (m_kerne.size() == 2) return 1 - von;  // zwei Kerne: der andere
        return kNicht;
    }

    void lege(std::vector<Spur>& spuren, const std::vector<std::map<int, std::vector<std::string>>>& handler)
    {
        m_handler = &handler;
        const std::size_t n = spuren.size();
        // je Ereignis seine Tiefe in Fragmenten: Boot und Barrieren gleichen nur auf oberster Ebene ab
        std::vector<std::vector<int>> ebene(n);
        for (std::size_t k = 0; k < n; ++k)
        {
            int t = 0;
            for (const Ereignis& e : spuren[k])
            {
                if (e.art == Ereignis::Art::Ende) --t;
                ebene[k].push_back(t);
                if (e.art == Ereignis::Art::Anfang) ++t;
            }
        }
        const auto sync = [&](std::size_t k, std::size_t i) {
            const Ereignis& e = spuren[k][i];
            return e.art == Ereignis::Art::Ipc && (e.ipc == Ipc::Boot || e.ipc == Ipc::Barriere) && ebene[k][i] == 0;
        };
        // wer gebootet wird, beginnt erst dann; steht der Boot in einem Fragment (`if (CPU2 im Reset)`), beginnt er
        // hinter dessen aeusserstem Fragment (dort ist der Pfeil schon gezeichnet)
        std::vector<bool> aktiv(n, true);
        std::vector<std::vector<std::pair<std::size_t, std::size_t>>> spaeter(n);  // je Kern: (Stelle, wer dort beginnt)
        for (std::size_t k = 0; k < n; ++k)
            for (std::size_t i = 0; i < spuren[k].size(); ++i)
            {
                const Ereignis& e = spuren[k][i];
                if (e.art != Ereignis::Art::Ipc || e.ipc != Ipc::Boot) continue;
                const std::size_t z = kernNamens(e.ziel, k);
                if (z == kNicht || z == k) continue;
                aktiv[z] = false;
                if (ebene[k][i] == 0) continue;
                std::size_t j = i + 1;
                while (j < spuren[k].size() && !(spuren[k][j].art == Ereignis::Art::Ende && ebene[k][j] == 0)) ++j;
                spaeter[k].emplace_back(std::min(j + 1, spuren[k].size()), z);
            }
        std::vector<std::size_t> pos(n, 0);
        for (bool fort = true; fort;)
        {
            fort = false;
            // 1. bis zum naechsten Abgleich: je Kern sein Stueck, mehrere zugleich als par
            std::vector<std::pair<std::size_t, std::size_t>> stuecke;  // Kern, Ende
            for (std::size_t k = 0; k < n; ++k)
            {
                if (!aktiv[k]) continue;
                std::size_t e = pos[k];
                const auto halt = [&](std::size_t i) {
                    return std::any_of(spaeter[k].begin(), spaeter[k].end(), [&](const auto& s) { return s.first == i; });
                };
                while (e < spuren[k].size() && !sync(k, e) && (e == pos[k] || !halt(e))) ++e;
                if (e > pos[k]) stuecke.emplace_back(k, e);
            }
            if (stuecke.size() > 1) schritt(SequenzSchritt::Art::FragmentAnfang, FragmentArt::Parallel, m_kerne[stuecke[0].first].kern.name);
            for (std::size_t s = 0; s < stuecke.size(); ++s)
            {
                const auto [k, e] = stuecke[s];
                if (stuecke.size() > 1 && s > 0) schritt(SequenzSchritt::Art::Sonst, FragmentArt::Parallel, m_kerne[k].kern.name);
                for (std::size_t i = pos[k]; i < e; ++i) schreibe(spuren[k][i], k);
                pos[k] = e;
                fort = true;
            }
            if (stuecke.size() > 1) schritt(SequenzSchritt::Art::FragmentEnde, FragmentArt::Parallel, {});
            // 2. Boot: ein Kern hinter dem Fragment mit seinem Boot beginnt jetzt
            for (std::size_t k = 0; k < n; ++k)
                for (auto it = spaeter[k].begin(); it != spaeter[k].end();)
                    if (aktiv[k] && pos[k] >= it->first)
                    {
                        if (!aktiv[it->second]) fort = true;
                        aktiv[it->second] = true;
                        it = spaeter[k].erase(it);
                    }
                    else
                        ++it;
            for (std::size_t k = 0; k < n; ++k)
            {
                if (!aktiv[k] || pos[k] >= spuren[k].size() || spuren[k][pos[k]].ipc != Ipc::Boot) continue;
                const Ereignis& e = spuren[k][pos[k]];
                const std::size_t z = kernNamens(e.ziel, k);
                pfeil(NachrichtArt::Boot, k, z, "boot" + (e.in.empty() ? std::string() : " (in " + e.in + ")"), e);
                if (z != kNicht) aktiv[z] = true;
                ++pos[k];
                fort = true;
            }
            if (fort) continue;
            // 3. Barrieren: wer an derselben steht, wartet aufeinander
            for (std::size_t k = 0; k < n; ++k)
            {
                if (!aktiv[k] || pos[k] >= spuren[k].size() || spuren[k][pos[k]].ipc != Ipc::Barriere) continue;
                const Ereignis& e = spuren[k][pos[k]];
                std::vector<std::size_t> beteiligt{k};
                for (std::size_t j = k + 1; j < n; ++j)
                    if (aktiv[j] && pos[j] < spuren[j].size() && spuren[j][pos[j]].ipc == Ipc::Barriere && spuren[j][pos[j]].flag == e.flag)
                        beteiligt.push_back(j);
                barriere(e, beteiligt);
                if (beteiligt.size() == 1)
                    m_d.hinweise.push_back("barrier FLAG " + std::to_string(e.flag) + " on " + m_kerne[k].kern.name + " (" + e.pfad +
                                           ":" + std::to_string(e.zeile) + ") has no partner at the same point");
                for (const std::size_t j : beteiligt) ++pos[j];
                fort = true;
                break;
            }
        }
        for (std::size_t k = 0; k < n; ++k)
            if (!aktiv[k]) m_d.hinweise.push_back(m_kerne[k].kern.name + " is never booted - not shown");
    }

    /// Die Abschnitte: je Handler, was er tut.
    void abschnitt(std::size_t kern, const std::string& titel, const Spur& spur)
    {
        schritt(SequenzSchritt::Art::FragmentAnfang, FragmentArt::Abschnitt, titel);
        for (const Ereignis& e : spur) schreibe(e, kern);
        schritt(SequenzSchritt::Art::FragmentEnde, FragmentArt::Abschnitt, {});
    }

private:
    void schritt(SequenzSchritt::Art art, FragmentArt f, std::string text)
    {
        SequenzSchritt s;
        s.art = art;
        s.fragment = f;
        s.text = std::move(text);
        m_d.schritte.push_back(std::move(s));
    }

    void pfeil(NachrichtArt art, std::size_t von, std::size_t nach, std::string text, const Ereignis& e)
    {
        SequenzSchritt s;
        s.art = SequenzSchritt::Art::Nachricht;
        s.nachricht = art;
        s.von = von;
        s.nach = nach == kNicht ? diagramm::kKeinZiel : nach;
        s.text = std::move(text);
        s.pfad = e.pfad;
        s.zeile = e.zeile;
        m_d.schritte.push_back(std::move(s));
    }

    void barriere(const Ereignis& e, std::vector<std::size_t> beteiligt)
    {
        SequenzSchritt s;
        s.art = SequenzSchritt::Art::Barriere;
        s.beteiligt = std::move(beteiligt);
        s.von = s.beteiligt.front();
        s.text = "barrier FLAG " + std::to_string(e.flag) + (e.in.empty() ? std::string() : " (in " + e.in + ")");
        s.pfad = e.pfad;
        s.zeile = e.zeile;
        m_d.schritte.push_back(std::move(s));
    }

    void schreibe(const Ereignis& e, std::size_t k)
    {
        switch (e.art)
        {
            case Ereignis::Art::Aufruf:
            {
                SequenzSchritt s;
                s.art = SequenzSchritt::Art::Nachricht;
                s.nachricht = NachrichtArt::Aufruf;
                s.von = s.nach = k;
                s.text = e.text;
                s.tiefe = e.tiefe;
                s.fremd = e.fremd;
                s.pfad = e.pfad;
                s.zeile = e.zeile;
                m_d.schritte.push_back(std::move(s));
                return;
            }
            case Ereignis::Art::Anfang: schritt(SequenzSchritt::Art::FragmentAnfang, e.fragment, e.text); return;
            case Ereignis::Art::Sonst: schritt(SequenzSchritt::Art::Sonst, e.fragment, e.text); return;
            case Ereignis::Art::Ende: schritt(SequenzSchritt::Art::FragmentEnde, e.fragment, {}); return;
            case Ereignis::Art::Ipc: break;
        }
        const std::string in = e.in.empty() ? std::string() : " (in " + e.in + ")";
        const std::size_t z = kernNamens(e.ziel, k);
        switch (e.ipc)
        {
            case Ipc::Signal:
            {
                std::string text = e.flag >= 0 ? "FLAG " + std::to_string(e.flag) : "flag";
                if (z != kNicht && m_handler != nullptr)
                    if (const auto it = (*m_handler)[z].find(e.flag); it != (*m_handler)[z].end())
                    {
                        std::string namen;
                        for (const std::string& h : it->second) namen += (namen.empty() ? "" : ", ") + h;
                        text += " \xE2\x86\x92 " + namen;
                    }
                pfeil(NachrichtArt::Signal, k, z, text + in, e);
                m_signale.push_back({k, z, e.flag, e.pfad, e.zeile});
                return;
            }
            case Ipc::Senden:
                pfeil(NachrichtArt::Nachricht, k, z, (e.text.empty() ? std::string("message") : "command " + e.text) + in, e);
                return;
            case Ipc::Boot:
                pfeil(NachrichtArt::Boot, k, z, "boot" + in, e);
                return;
            case Ipc::Barriere:
                // in einem Fragment: nicht abgeglichen
                barriere(e, {k});
                m_d.hinweise.push_back("barrier FLAG " + std::to_string(e.flag) + " on " + m_kerne[k].kern.name + " (" + e.pfad + ":" +
                                       std::to_string(e.zeile) + ") stands in a loop or branch - not aligned with the other core");
                return;
            default: return;
        }
    }

public:
    struct Gesendet
    {
        std::size_t von, nach;
        int flag;
        std::string pfad;
        int zeile;
    };
    std::vector<Gesendet> m_signale;

private:
    const std::vector<KernProjekt>& m_kerne;
    diagramm::Sequenzdiagramm& m_d;
    const std::vector<std::map<int, std::vector<std::string>>>* m_handler = nullptr;
};

}  // namespace

std::vector<KernProjekt> liesKerne(const Projekt& projekt, const ProjektOptionen& optionen)
{
    std::vector<KernProjekt> aus;
    for (const Kern& k : projekt.einstellungen.kerne) aus.push_back({k, liesKern(projekt, k, optionen)});
    return aus;
}

diagramm::Sequenzdiagramm startDerKerne(const std::vector<KernProjekt>& kerne, const SequenzOptionen& optionen)
{
    diagramm::Sequenzdiagramm d;
    d.titel = "Start of the cores";
    for (const KernProjekt& k : kerne) d.linien.push_back({k.kern.name});
    if (kerne.empty())
    {
        d.hinweise.push_back("no cores in the project file (\"kerne\")");
        return d;
    }
    std::vector<Funktionsverzeichnis> verzeichnisse;
    verzeichnisse.reserve(kerne.size());
    for (const KernProjekt& k : kerne) verzeichnisse.emplace_back(k.projekt);
    std::vector<Spur> spuren(kerne.size());
    std::vector<std::map<int, std::vector<std::string>>> handler(kerne.size());
    std::vector<Spurleser> leser;
    leser.reserve(kerne.size());
    for (std::size_t k = 0; k < kerne.size(); ++k)
    {
        leser.emplace_back(kerne[k], verzeichnisse[k], std::max(optionen.tiefe, 0));
        handler[k] = handlerVon(kerne[k].projekt);
        const std::vector<Fundstelle> starts = funktionenNamens(kerne[k].projekt, kerne[k].kern.start);
        if (starts.empty())
        {
            d.hinweise.push_back(kerne[k].kern.name + ": no function " + kerne[k].kern.start + " in its folders");
            continue;
        }
        if (starts.size() > 1)
            d.hinweise.push_back(kerne[k].kern.name + ": " + std::to_string(starts.size()) + " functions " + kerne[k].kern.start +
                                 " - shown is the one in " + kerne[k].projekt.dateien[starts.front().datei].pfad);
        spuren[k] = leser[k].start(starts.front());
    }
    Zusammenleger z(kerne, d);
    z.lege(spuren, handler);
    // je Handler ein Abschnitt (SQ4), in der Reihenfolge der Kerne und Flags; ein Handler fuer mehrere Flags einmal
    std::set<std::pair<std::size_t, int>> empfangen;
    for (std::size_t k = 0; k < kerne.size(); ++k)
    {
        std::map<std::string, std::vector<int>> flagsJeHandler;
        std::vector<std::string> reihe;
        for (const auto& [flag, namen] : handler[k])
        {
            empfangen.insert({k, flag});
            for (const std::string& h : namen)
            {
                if (!flagsJeHandler.contains(h)) reihe.push_back(h);
                flagsJeHandler[h].push_back(flag);
            }
        }
        for (const std::string& h : reihe)
        {
            const std::vector<Fundstelle> f = funktionenNamens(kerne[k].projekt, h);
            std::string flags;
            for (const int x : flagsJeHandler[h]) flags += (flags.empty() ? "" : ", ") + std::to_string(x);
            const std::string titel = "on IPC flag " + flags + " (" + kerne[k].kern.name + "): " + h;
            if (f.size() != 1)
            {
                d.hinweise.push_back(titel + " - the handler " + (f.empty() ? "has no body in the core" : "is ambiguous"));
                continue;
            }
            z.abschnitt(k, titel, leser[k].handler(f.front()));
        }
    }
    // ein Signal ohne Handler und ohne Warteschleife drueben
    for (std::size_t k = 0; k < spuren.size(); ++k)
        for (const Ereignis& e : spuren[k])
            if (e.ipc == Ipc::Warten && e.flag >= 0) empfangen.insert({k, e.flag});
    for (const Zusammenleger::Gesendet& s : z.m_signale)
        if (s.nach == kNicht || !empfangen.contains({s.nach, s.flag}))
            d.hinweise.push_back("FLAG " + std::to_string(s.flag) + " from " + kerne[s.von].kern.name + " (" + s.pfad + ":" +
                                 std::to_string(s.zeile) + "): no handler and no wait for it on the other core");
    d.hinweise.push_back("data in shared memory is not shown as messages");
    ohneLeere(
        d.schritte, [](const SequenzSchritt& s) { return s.art == SequenzSchritt::Art::FragmentAnfang; },
        [](const SequenzSchritt& s) { return s.art == SequenzSchritt::Art::Sonst; },
        [](const SequenzSchritt& s) { return s.art == SequenzSchritt::Art::FragmentEnde; });
    return d;
}

}  // namespace c2d::projekt
