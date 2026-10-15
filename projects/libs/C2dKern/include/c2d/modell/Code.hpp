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

#include <memory>
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
    Abbruch,       ///< break
    Fortsetzung,   ///< continue
    Sprung,        ///< text = Marke (goto)
    Marke,         ///< text = Name
    Direktive,     ///< text = die Zeile (Praeprozessor im Rumpf)
    FuerJedes,     ///< text = `Element : Menge`; kinder[0] = Rumpf (range-for)
    Versuch,       ///< kinder[0] = der Block; kinder[1..] = je ein Fang (try)
    Fang,          ///< text = was gefangen wird; kinder[0] = der Block (catch)
    Wurf,          ///< text = was geworfen wird (throw); darf leer sein
};

struct Anweisung
{
    AnweisungArt art = AnweisungArt::Einfach;
    std::string text;
    std::string init;     ///< nur Fuer
    std::string schritt;  ///< nur Fuer
    std::vector<std::unique_ptr<Anweisung>> kinder;
    std::unique_ptr<Anweisung> sonst;  ///< nur Wenn
    int zeile = 0;
};

using AnweisungPtr = std::unique_ptr<Anweisung>;

// ---------------------------------------------------------------------------
// Deklarationen
// ---------------------------------------------------------------------------

/// Typ, Name und Zusatz wieder als eine Zeile, gesetzt wie im Quelltext: ein
/// Leerzeichen dazwischen, ausser der Typ endet auf abgesetzte Sterne oder `(`
/// oder der Zusatz beginnt mit `[` oder `)` — `u8 daten[16]`,
/// `struct Knoten *naechster`, `QWidget* parent`, `void (*rueckruf)(int)`,
/// `unsigned bereit : 1`.
[[nodiscard]] std::string deklaration(const std::string& typ, const std::string& name,
                                      const std::string& zusatz = {});

struct Parameter
{
    std::string typ;
    std::string name;  ///< darf leer sein (Prototyp)
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
    /// Nur in ihrer Datei sichtbar (`static` vor einer freien Funktion) — ein
    /// Aufruf aus einer anderen Datei meint sie nie.
    bool intern = false;

    [[nodiscard]] std::string vollerName() const { return modell::vollerName(bereich, name); }
    /// Meint ein Aufruf diese Funktion, dem Namen nach? `f` passt auf jede
    /// Funktion `f`, gleich in welchem Bereich; `N::f` nur auf eine, deren voller
    /// Name so endet (`N::f`, `a::N::f`).
    [[nodiscard]] bool heisst(std::string_view aufruf) const;
    /// `bereich::name(typ name, ...)` — der Text des Startknotens.
    [[nodiscard]] std::string signatur() const { return modell::signatur(vollerName(), parameter); }
};

enum class TypArt
{
    Struktur,
    Vereinigung,
    Aufzaehlung,
    Klasse,
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
    std::vector<std::string> basen;  ///< wovon der Typ erbt (C: leer)
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

struct Variable
{
    std::string typ;
    std::string name;
    std::string zusatz;
    int zeile = 0;
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

/// Eine Uebersetzungseinheit: eine Quelldatei.
struct Einheit
{
    std::string name;
    std::vector<Einbindung> einbindungen;
    std::vector<Makro> makros;
    std::vector<Typ> typen;
    std::vector<Alias> aliase;
    std::vector<Variable> variablen;
    std::vector<Funktion> funktionen;
};

}  // namespace c2d::modell
