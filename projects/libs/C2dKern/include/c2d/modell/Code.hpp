/**
 * @file Code.hpp
 * @brief Das Codemodell: was eine Quelldatei enthaelt — fuer alle Sprachen gleich
 *
 * Der Leser einer Sprache (sprache/Sprache.hpp) fuellt dieses Modell; die
 * Diagramme (diagramm/Ableitung.hpp) lesen nur es und kennen keine Sprache.
 * Eine neue Sprache bringt einen neuen Leser, kein neues Modell — fehlt dem
 * Modell etwas (etwa Methoden einer Klasse), wird es hier ergaenzt.
 *
 * Ausdruecke bleiben Text: ein Diagramm zeigt sie, es rechnet nicht mit ihnen.
 */
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::modell
{

// ---------------------------------------------------------------------------
// Anweisungen
// ---------------------------------------------------------------------------

enum class AnweisungArt
{
    Einfach,       ///< `text;` — Ausdruck oder lokale Deklaration
    Block,         ///< kinder
    Wenn,          ///< text = Bedingung; kinder[0] = dann; sonst (darf fehlen)
    Solange,       ///< text = Bedingung; kinder[0] = Rumpf
    TueSolange,    ///< kinder[0] = Rumpf; text = Bedingung
    Fuer,          ///< init; text = Bedingung; schritt; kinder[0] = Rumpf
    Auswahl,       ///< text = Ausdruck; kinder[0] = Rumpf (ein Block mit Faellen)
    Fall,          ///< text = Wert; leer = der Standardfall (default)
    Ruecksprung,   ///< text = Wert (darf leer sein)
    Abbruch,       ///< break; text = die Marke (`break outer`, Java/JavaScript), leer = die innerste
    Fortsetzung,   ///< continue; text = die Marke wie bei Abbruch
    Sprung,        ///< text = Marke (goto)
    Marke,         ///< text = Name
    Direktive,     ///< text = die Zeile (Praeprozessor im Rumpf)
    FuerJedes,     ///< text = `Element : Menge`; kinder[0] = Rumpf (range-for)
    /// kinder[0] = der Block; kinder[1..] = je ein Fang (try/catch, Python except), zuletzt
    /// vielleicht ein Schluss; sonst = was nach einem try ohne Ausnahme laeuft (Python else)
    Versuch,
    Fang,          ///< text = was gefangen wird; kinder[0] = der Block (catch)
    Wurf,          ///< text = was geworfen wird (throw); darf leer sein
    /// finally: kinder[0] = der Block — er laeuft auf jedem Weg aus dem try, auch nach
    /// return, break, continue; nur als letztes Kind eines Versuchs
    Schluss,
    /// Ein Block unter einem verwalteten Gegenstand (Python with, C# using): text = was
    /// verwaltet wird (`open(p) as f`); kinder[0] = der Rumpf
    Mit,
    /// Bedingte Uebersetzung im Rumpf, als Varianten gelesen (LeseOptionen::varianten):
    /// text = `#if`; kinder = je Zweig ein Zweig, in der Reihenfolge des Texts. Fehlt
    /// das `#else`, steht dafuer ein leerer Zweig `else` am Ende.
    Varianten,
    /// Ein Zweig von Varianten: text = seine Bedingung (`A`, `defined(A)`, `else`);
    /// kinder = seine Anweisungen; gilt = er gilt mit den Definitionen.
    Zweig,
    /// Anweisungen, die nur unter einer Bedingung der Uebersetzung stehen — der
    /// geltende Zweig eines #if (LeseOptionen::bereiche): text = die volle
    /// Bedingung (volleBedingung); kinder = die Anweisungen.
    Bedingt,
};

/// Die Logik einer Bedingung (`(a && b) || !c`): ein Baum aus Und, Oder, Nicht und Atomen.
/// Ein Atom ist, was zwischen den logischen Operatoren steht: ein Vergleich, ein Aufruf, ein Name.
struct Logik
{
    enum class Art
    {
        Atom,
        Nicht,  ///< teile[0]
        Und,    ///< teile, von links ausgewertet; endet beim ersten falschen
        Oder,   ///< teile, von links ausgewertet; endet beim ersten wahren
    };
    Art art = Art::Atom;
    std::string text;           ///< nur Atom: wie es in der Quelle steht
    bool mitAufruf = false;     ///< nur Atom: es ruft eine Funktion (Kurzschluss kann ihn auslassen)
    std::vector<Logik> teile;
};

struct Anweisung
{
    AnweisungArt art = AnweisungArt::Einfach;
    std::string text;
    std::string init;     ///< nur Fuer
    std::string schritt;  ///< nur Fuer
    std::vector<std::unique_ptr<Anweisung>> kinder;
    /// Wenn: der Zweig else. Solange, Fuer, FuerJedes: was laeuft, wenn die Schleife ohne break
    /// endet (Python else). Versuch: was laeuft, wenn im try keine Ausnahme kam (Python else).
    std::unique_ptr<Anweisung> sonst;
    /// Wie der Kopf in der Quelle steht, wo die Sprache ihn nicht in der Form der C-Familie
    /// schreibt — `while x > 0`, `for x in xs`, `except ValueError as e`, `raise X`, `match x`,
    /// `with open(p) as f`; leer = die Diagramme setzen ihn aus text zusammen (`while (x)`).
    std::string kopf;
    /// Nur Auswahl: ein Fall endet an der naechsten Fall-Marke, ohne in sie zu laufen (Python
    /// match); ein break darin verlaesst die Schleife darum, nicht die Auswahl.
    bool ohneDurchfall = false;
    /// Schleifen, Auswahl und Block: die Marke davor (`outer: for …`, Java/JavaScript), die ein
    /// `break outer` / `continue outer` meint; leer = keine. (In C bleibt eine Marke eine Anweisung
    /// Marke — sie ist das Ziel eines goto.)
    std::string marke;
    int zeile = 0;
    bool gilt = false;  ///< nur Zweig
    /// Eine Bedingung mit mindestens zwei Atomen (ein `if`, eine Schleife, `?:`, ein `return` oder
    /// eine Zuweisung mit `&&`/`||`): ihr Text und ihre Logik — fuer die Wahrheitstabelle.
    std::string logikText;
    std::optional<Logik> logik;
    /// Nur Einfach und Ruecksprung: die Bedingung eines `?:` auf oberster Ebene (auch mit einem
    /// Teil) — fuer die Entscheidungstabelle eine Verzweigung; leer = keins.
    std::string frage;
    /// Die Bedingung (eines if, einer Schleife, eines `?:`) ruft eine Funktion: sie kann bei
    /// jeder Pruefung anders ausgehen.
    bool bedingungRuft = false;
    /// Die Namen, die die Bedingung (eines if, einer Schleife, eines switch, eines `?:`) liest.
    std::vector<std::string> bedingungNamen;
    /// Was die Anweisung aendern kann (Einfach, Ruecksprung, Wurf): die Namen, die sie schreibt
    /// (Zuweisung, `++`/`--`, `&name`); ob sie durch einen Zeiger schreibt (`*p =`, `p->x =`) —
    /// dann kann es jeder Name sein; ob sie etwas ruft — dann ebenso.
    std::vector<std::string> schreibt;
    bool schreibtDurchZeiger = false;
    bool ruft = false;
    /// Nur Einfach, genau eine Zuweisung `ziel = wert` (nicht `+=`): beide Seiten — fuer Zuordnungen.
    std::string ziel;
    std::string wert;
    /// Die Funktionen, die in der Anweisung definiert werden (ein Callback `(x) => {…}`, eine Methode
    /// eines Objekts, Python `def` im Rumpf): ihr voller Name — der Text nennt sie nicht.
    std::vector<std::string> innere;
};

using AnweisungPtr = std::unique_ptr<Anweisung>;

/// Wann der Zweig `zweig` einer Kette `#if` / `#elif` / `#else` gilt: keiner
/// davor und er selbst — `!defined(A) && B`. `bedingungen` sind die Bedingungen
/// der Zweige wie bei AnweisungArt::Zweig (`A`, `defined(A)`, `else`); `zweig`
/// darf gleich ihrer Zahl sein: das fehlende `#else`.
[[nodiscard]] std::string volleBedingung(const std::vector<std::string>& bedingungen, std::size_t zweig);

// ---------------------------------------------------------------------------
// Deklarationen
// ---------------------------------------------------------------------------

/// Typ, Name und Zusatz wieder als eine Zeile, gesetzt wie im Quelltext: ein
/// Leerzeichen dazwischen, ausser der Typ endet auf abgesetzte Sterne oder `(`
/// oder der Zusatz beginnt mit `[` oder `)` — `u8 daten[16]`,
/// `struct Knoten *naechster`, `QWidget* parent`, `void (*rueckruf)(int)`,
/// `unsigned bereit : 1`. Mit `typHinten` steht der Typ hinter dem Namen (`name: typ`,
/// Python), der Zusatz mit Abstand dahinter (`name: typ = 3`).
[[nodiscard]] std::string deklaration(const std::string& typ, const std::string& name,
                                      const std::string& zusatz = {}, bool typHinten = false);

struct Parameter
{
    std::string typ;
    std::string name;  ///< darf leer sein (Prototyp); in Python mit `*`/`**` davor
    bool typHinten = false;  ///< `name: typ` (Python)
};

/// `bereich::name`, oder nur der Name, wenn es keinen Bereich gibt.
[[nodiscard]] std::string vollerName(const std::string& bereich, const std::string& name);

/// `name(typ name, ...)`
[[nodiscard]] std::string signatur(const std::string& name, const std::vector<Parameter>& parameter);

struct Funktion
{
    std::string name;     ///< ohne Bereich; auch `~Motor`, `operator==`
    std::string bereich;  ///< Namensraum und Klasse, etwa `antrieb::Motor`; leer in C
    std::string rueckgabe;  ///< alles vor dem Namen, etwa `static int`
    std::vector<Parameter> parameter;
    AnweisungPtr rumpf;  ///< leer = nur deklariert
    int zeile = 0;
    int bisZeile = 0;  ///< die Zeile, in der der Rumpf endet (`}`); 0 = ohne Rumpf
    /// Nur in ihrer Datei sichtbar (`static` vor einer freien Funktion) — ein
    /// Aufruf aus einer anderen Datei meint sie nie.
    bool intern = false;
    /// In einer anderen Funktion definiert (Python: `def` im Rumpf): deren voller Name;
    /// leer = auf oberster Ebene oder in einem Typ. Im Rumpf der aeusseren steht dafuer eine
    /// Anweisung `def name(…)`.
    std::string aussen;

    [[nodiscard]] std::string vollerName() const { return modell::vollerName(bereich, name); }
    /// Meint ein Aufruf diese Funktion, dem Namen nach? `f` passt auf jede
    /// Funktion `f`, gleich in welchem Bereich; `N::f` nur auf eine, deren voller
    /// Name so endet (`N::f`, `a::N::f`).
    [[nodiscard]] bool heisst(std::string_view aufruf) const;
    /// `bereich::name(typ name, ...)` — der Text des Startknotens.
    [[nodiscard]] std::string signatur() const { return modell::signatur(vollerName(), parameter); }
};

/// Die Funktionen, die ein Text ruft: jeder Name vor einer `(`, mit seinem
/// Bereich (`a::b(` gibt `a::b`), ohne Objekt davor (`x.b(`, `p->b(` geben `b`),
/// jeder einmal, in der Reihenfolge des Texts. Woerter der C-Familie und von Python,
/// die so aussehen (`if (`, `sizeof(`, `not (`), zaehlen nicht; in Literalen wird
/// nichts gerufen.
[[nodiscard]] std::vector<std::string> aufrufeIn(std::string_view text);

/// Die Namen in einem Text, mit Bereich (`a::B`), ausserhalb von Literalen, jeder einmal, in der
/// Reihenfolge des Texts — Kandidaten fuer Typen (projekt/Typen.hpp); was davon einer ist,
/// entscheidet, wer die Typen kennt.
[[nodiscard]] std::vector<std::string> bezeichnerIn(std::string_view text);

/// Was der Rumpf einer Funktion ruft, ueber alle Anweisungen, jeder einmal.
[[nodiscard]] std::vector<std::string> aufrufeVon(const Funktion& funktion);

/// Ein Aufruf im Rumpf, mit der Bedingung der Uebersetzung, unter der er steht.
struct Aufrufstelle
{
    std::string name;  ///< wie bei aufrufeIn
    int zeile = 0;     ///< die Zeile der ersten Anweisung, die so ruft
    /// Die innerste Bedingung, unter der er steht: die volle Bedingung eines Zweigs von
    /// Varianten, der nicht gilt, oder eines Bereichs (AnweisungArt::Bedingt). Leer = keine
    /// — auch im geltenden Zweig von Varianten.
    std::string bedingung;
    /// false: er steht in einem Zweig von Varianten, der mit den Definitionen nicht gilt.
    bool gilt = true;
};

/// Was der Rumpf einer Funktion ruft, mit den Bedingungen: jede Verbindung aus Name,
/// Bedingung und `gilt` einmal, in der Reihenfolge des Rumpfs.
[[nodiscard]] std::vector<Aufrufstelle> aufrufstellenVon(const Funktion& funktion);

enum class TypArt
{
    Struktur,
    Vereinigung,
    Aufzaehlung,
    Klasse,
    Schnittstelle,  ///< interface (TypeScript; spaeter C#, Java): Glieder und Methoden ohne Rumpf
};

/// Wer ein Glied sehen darf. `Keine`: die Sprache kennt das nicht (C).
enum class Sichtbarkeit
{
    Keine,
    Oeffentlich,
    Geschuetzt,
    Privat,
};

struct Feld
{
    std::string typ;     ///< leer bei den Werten einer Aufzaehlung
    std::string name;    ///< leer = namenloses Glied (anonyme Struktur)
    std::string zusatz;  ///< `[8]`, `: 3` oder `= 4`
    Sichtbarkeit sicht = Sichtbarkeit::Keine;
    int zeile = 0;
    bool typHinten = false;  ///< `name: typ` (Python)
};

/// Eine Funktion, die zu einem Typ gehoert. Hat sie einen Rumpf, steht sie
/// zusaetzlich unter Einheit::funktionen (mit dem Typ als Bereich).
struct Methode
{
    std::string name;
    std::string rueckgabe;  ///< leer bei Konstruktor und Destruktor
    std::vector<Parameter> parameter;
    Sichtbarkeit sicht = Sichtbarkeit::Keine;
    bool virtuell = false;
    bool statisch = false;
    bool abstrakt = false;  ///< `= 0`
    int zeile = 0;

    [[nodiscard]] std::string signatur() const { return modell::signatur(name, parameter); }
};

struct Typ
{
    TypArt art = TypArt::Struktur;
    std::string name;     ///< ohne Bereich
    std::string bereich;  ///< Namensraum und umgebender Typ; leer in C
    std::vector<Feld> felder;
    std::vector<Methode> methoden;
    std::vector<std::string> basen;  ///< wovon der Typ erbt (C: leer); bei einer Schnittstelle: extends
    std::vector<std::string> realisiert;  ///< welche Schnittstellen er umsetzt (`implements`)
    int zeile = 0;

    [[nodiscard]] std::string vollerName() const { return modell::vollerName(bereich, name); }
};

/// Ein weiterer Name fuer einen Typ (typedef, using).
struct Alias
{
    std::string name;
    std::string ziel;
    int zeile = 0;
};

/// Der Anfangswert einer Variablen: ein Wert oder eine Liste `{ … }`, deren Teile einen Bezeichner
/// tragen koennen (`[ROT] = …` einen Index, `.feld = …` ein Glied).
struct Initialisierer
{
    std::string index;  ///< `[ROT] =` -> `ROT`; leer = nach Reihenfolge
    std::string glied;  ///< `.feld =` -> `feld`
    std::string text;   ///< wie er in der Quelle steht (auch eine Liste, als Ganzes)
    bool liste = false;
    std::vector<Initialisierer> teile;  ///< nur Liste
    int zeile = 0;
};

struct Variable
{
    std::string typ;
    std::string name;
    std::string zusatz;
    int zeile = 0;
    /// Nur bei `const`: der Anfangswert — fuer Nachschlagetabellen.
    std::optional<Initialisierer> anfang;
};

struct Einbindung
{
    std::string ziel;
    bool system = false;  ///< `<x>` statt `"x"`
    int zeile = 0;
};

struct Makro
{
    std::string name;
    std::string text;  ///< bei einem Makro mit Parametern beginnt er mit `(`
    int zeile = 0;
};

/// Ein `#undef NAME`, wo der Leser den Text sieht (nicht in einem Zweig, der nicht gilt).
struct Aufhebung
{
    std::string name;
    int zeile = 0;
};

/// Eine Uebersetzungseinheit: eine Quelldatei.
struct Einheit
{
    std::string name;
    std::vector<Einbindung> einbindungen;
    std::vector<Makro> makros;
    std::vector<Aufhebung> aufhebungen;
    std::vector<Typ> typen;
    std::vector<Alias> aliase;
    std::vector<Variable> variablen;
    std::vector<Funktion> funktionen;
};

// ---------------------------------------------------------------------------
// Bedingte Uebersetzung: wonach gefragt wird, und die Ketten #if / #elif / #else
// (der Leser liefert sie in sprache::Ergebnis, nicht beim Lesen aller Zweige)
// ---------------------------------------------------------------------------

/// "Kein Zweig" (Weiche::geltend, Weiche::elternZweig) und "keine Weiche" (Weiche::eltern).
inline constexpr std::size_t kKeinZweig = static_cast<std::size_t>(-1);

/// Ein Name, nach dem eine Bedingung der Uebersetzung fragt — was man von aussen schalten kann.
struct Schalter
{
    std::string name;
    bool definiert = false;  ///< dort, wo zuerst nach ihm gefragt wird
    std::string wert;        ///< dort sein Wert, wenn er definiert ist (`#define X` ohne Wert: leer)
    /// Er wird verglichen oder verrechnet (`X >= 2`), nicht nur gefragt, ob er gilt: er braucht einen Wert.
    bool zahl = false;
    /// Die Datei definiert ihn selbst, bevor nach ihm gefragt wird (die Zeile): eine
    /// Definition von aussen aendert dann nichts. 0 = nicht.
    int inDatei = 0;
    bool vorgegeben = false;  ///< die Sprache definiert ihn (`__cplusplus`)
};

/// Was fuer einen Zweig gelten muss: ein Name definiert (mit `wert`; leer = gleich mit
/// welchem) oder nicht definiert.
struct Forderung
{
    std::string name;
    bool definiert = true;
    std::string wert;
};

/// Die Bedingung eines Zweigs der bedingten Uebersetzung als Baum: Und, Oder, Nicht und Atome.
/// Ein Atom laesst sich an einem Namen einstellen (`defined(X)`, `X > 50`, `X` = `X != 0`), ist
/// eine Zahl (`#if 0`), oder ist Text, wo der Ausdruck sich nicht nach Namen zerlegen laesst
/// (`F(2) > 1`, `A + B > 3`, `A == B`).
struct Vorbedingung
{
    enum class Art
    {
        Definiert,  ///< name
        Vergleich,  ///< name op wert: `X > 50`; der Name steht links
        Konstante,  ///< wert: `0` oder `1`
        Text,       ///< text, wie in der Quelle
        Nicht,      ///< teile[0]
        Und,        ///< teile
        Oder,       ///< teile
    };
    Art art = Art::Text;
    std::string name;
    std::string op;  ///< `==`, `!=`, `<`, `<=`, `>`, `>=`
    std::string wert;
    std::string text;
    std::vector<Vorbedingung> teile;
};

/// Eine Kette `#if` / `#elif` / `#else`, in der Reihenfolge des Texts. Include-Waechter
/// (`#ifndef K` mit `#define K` gleich darauf) zaehlen nicht.
struct Weiche
{
    int zeile = 0;  ///< des #if
    std::size_t eltern = kKeinZweig;       ///< die Weiche, in deren Zweig sie steht
    std::size_t elternZweig = kKeinZweig;  ///< und welcher es ist
    /// Je Zweig seine Bedingung, kurz wie bei AnweisungArt::Zweig (`A`,
    /// `defined(A)`, `else`); ein fehlendes `#else` steht nicht da.
    std::vector<std::string> zweige;
    /// Der Zweig, der gilt; zweige.size() = das fehlende `#else`; kKeinZweig = keiner
    /// (die Weiche steht in einem Zweig, der nicht gilt).
    std::size_t geltend = kKeinZweig;
    /// Je Zweig — und zuletzt fuer das fehlende `#else`, wenn es fehlt —, was sich an
    /// den Definitionen aendern muss, damit er gilt; leer = er gilt schon. nullopt =
    /// aus Namen nicht abzuleiten (eine Bedingung wie `F(2) > 1`, ein Name, den die
    /// Datei selbst definiert, Forderungen, die sich widersprechen).
    std::vector<std::optional<std::vector<Forderung>>> wahl;
    /// Je Zweig (wie `zweige`, ohne das fehlende `#else`) seine Bedingung als Baum.
    std::vector<Vorbedingung> baeume;
    /// Je Zweig (wie `zweige`) die Zeile seines `#if`, `#elif`, `#else`.
    std::vector<int> zweigZeilen;
    int endZeile = 0;  ///< des `#endif`; 0 = es fehlt
};

}  // namespace c2d::modell
