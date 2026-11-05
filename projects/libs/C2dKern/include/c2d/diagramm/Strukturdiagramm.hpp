/**
 * @file Strukturdiagramm.hpp
 * @brief Das Diagramm-Modell der Datentypen: Kaesten mit Eintraegen, Beziehungen
 *
 * Zeigt struct, union, enum (spaeter: Klassen) und wie sie einander enthalten
 * oder aufeinander verweisen. Gegenstueck zu Flussdiagramm.hpp.
 */
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::diagramm
{

/// Wie Tabellen und ihre Fremdschluessel gezeichnet werden (Entscheide N1-N6, Patrik 2026-11-05).
/// Klassen neben Tabellen bleiben in jeder Notation UML.
enum class ErNotation
{
    Tabellen,     ///< Kasten mit Spalten, Multiplizitaeten `1`, `0..1`, `*` an den Enden (wie UML)
    Kraehenfuss,  ///< Kasten mit Spalten, Kraehenfuesse an den Enden (Martin / IE)
    Chen,         ///< Rechteck, Ellipsen, Raute; `1` / `n` / `m` an den Linien
    MinMax,       ///< Formen wie Chen; an den Linien `(min,max)`: wie oft die Entitaet teilnimmt
};

/// In Chen und (min,max): welche Spalten als Ellipsen erscheinen.
enum class ErAttribute
{
    Alle,        ///< jede Spalte ausser Fremdschluesseln
    Schluessel,  ///< nur der Primaerschluessel
    Keine,
};

struct ErNotationName
{
    ErNotation notation;
    std::string_view name;   ///< fuer `c2d --notation`: `tabellen`, `kraehenfuss`, `chen`, `minmax`
    std::string_view titel;  ///< im Fenster: `Tables`, `Crow's foot`, `Chen`, `(min,max)`
};

/// Alle Notationen in der Reihenfolge der Auswahl; die erste ist die Vorgabe.
[[nodiscard]] std::span<const ErNotationName> erNotationen();

/// Wie viele an einem Ende einer Beziehung stehen koennen: min 0 oder 1, max 1 oder kViele.
inline constexpr int kViele = -1;
struct Vielfachheit
{
    int min = 0;
    int max = kViele;

    friend bool operator==(const Vielfachheit&, const Vielfachheit&) = default;
};

/// Die Vielfachheit als Text: Tables `1`, `0..1`, `*`, `1..*`; (min,max) `(0,*)`, `(1,1)` ...
[[nodiscard]] std::string uml(Vielfachheit v);
[[nodiscard]] std::string minMax(Vielfachheit v);

enum class KastenArt
{
    Struktur,
    Vereinigung,
    Aufzaehlung,
    Klasse,
    Schnittstelle,  ///< «interface»
    Tabelle,        ///< «table»: eine Tabelle einer Datenbank (SQL)
};

struct Eintrag
{
    std::string typ;     ///< leer bei den Werten einer Aufzaehlung
    std::string name;
    std::string zusatz;  ///< `[8]`, `: 3` oder `= 4`
    char sicht = '\0';   ///< `+` oeffentlich, `#` geschuetzt, `-` privat; '\0' = keine Angabe
    int zeile = 0;       ///< im Quelltext; 0 = keine
    bool typHinten = false;  ///< `name: typ` (wie modell::Feld::typHinten)
    // nur in einer Tabelle (KastenArt::Tabelle):
    bool schluessel = false;  ///< Teil des Primaerschluessels
    bool fremd = false;       ///< Teil eines Fremdschluessels
    bool nichtNull = false;   ///< NOT NULL

    /// Der Eintrag in einer Zeile, gesetzt wie in modell::deklaration (ohne die Sichtbarkeit).
    [[nodiscard]] std::string text() const;
};

struct Methode
{
    std::string rueckgabe;  ///< leer bei Konstruktor und Destruktor
    std::string signatur;   ///< `name(typ name, ...)`
    char sicht = '\0';
    bool abstrakt = false;
    bool statisch = false;
    int zeile = 0;  ///< im Quelltext; 0 = keine

    /// `rueckgabe signatur` (ohne die Sichtbarkeit).
    [[nodiscard]] std::string text() const;
};

struct Kasten
{
    KastenArt art = KastenArt::Struktur;
    std::string name;  ///< mit Bereich, etwa `antrieb::Motor`
    std::vector<Eintrag> eintraege;
    std::vector<Methode> methoden;
    int zeile = 0;  ///< die Zeile des Typs im Quelltext; 0 = keine
};

enum class BeziehungArt
{
    Enthaelt,  ///< als Wert enthalten (Komposition)
    Verweist,  ///< ueber einen Zeiger, oder der Typ ist eine Aufzaehlung, oder ein Fremdschluessel (Assoziation)
    Erbt,      ///< von = der abgeleitete Typ, nach = die Basis
    Realisiert,  ///< von = der Typ, nach = die Schnittstelle, die er umsetzt (gestrichelt, Spitze wie Erbt)
};

struct Beziehung
{
    std::size_t von = 0;   ///< Stelle in Strukturdiagramm::kaesten
    std::size_t nach = 0;
    BeziehungArt art = BeziehungArt::Verweist;
    std::string beschriftung;  ///< der Name des Glieds; bei einem Fremdschluessel seine Spalten
    // nur bei einem Fremdschluessel (von = die Tabelle mit ihm, nach = die Tabelle, auf die er zeigt):
    bool fremdschluessel = false;
    std::string name;                   ///< `CONSTRAINT name`; leer = keiner
    std::vector<std::size_t> spalten;   ///< seine Spalten: Stellen in kaesten[von].eintraege
    int zeile = 0;                      ///< im Quelltext
    /// Wie viele am Ende `von` stehen, je eine am Ende `nach`: hoechstens 1, wenn die Spalten eindeutig
    /// sind (UNIQUE, genau der Primaerschluessel), sonst viele; mindestens 0 (Entscheid N2).
    Vielfachheit amVon;
    /// Wie viele am Ende `nach` stehen, je eine am Ende `von`: genau 1, wenn alle Spalten NOT NULL
    /// sind, sonst 0..1.
    Vielfachheit amNach{0, 1};
};

struct Strukturdiagramm
{
    std::string titel;
    std::vector<Kasten> kaesten;
    std::vector<Beziehung> beziehungen;
    /// Wie Tabellen gezeichnet werden; die Formate und die Vorschau folgen ihr. Gesetzt von dem, der
    /// zeigt (Fenster, c2d) — die Ableitung laesst die Vorgabe.
    ErNotation notation = ErNotation::Tabellen;
    ErAttribute attribute = ErAttribute::Alle;

    /// Steht eine Tabelle darin? Nur dann gibt es eine Wahl der Notation.
    [[nodiscard]] bool hatTabellen() const;
};

}  // namespace c2d::diagramm
