/**
 * @file Strukturdiagramm.hpp
 * @brief Das Diagramm-Modell der Datentypen: Kaesten mit Eintraegen, Beziehungen
 *
 * Zeigt struct, union, enum (spaeter: Klassen) und wie sie einander enthalten
 * oder aufeinander verweisen. Gegenstueck zu Flussdiagramm.hpp.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

enum class KastenArt
{
    Struktur,
    Vereinigung,
    Aufzaehlung,
    Klasse,
    Schnittstelle,  ///< «interface»
};

struct Eintrag
{
    std::string typ;     ///< leer bei den Werten einer Aufzaehlung
    std::string name;
    std::string zusatz;  ///< `[8]`, `: 3` oder `= 4`
    char sicht = '\0';   ///< `+` oeffentlich, `#` geschuetzt, `-` privat; '\0' = keine Angabe
    int zeile = 0;       ///< im Quelltext; 0 = keine
    bool typHinten = false;  ///< `name: typ` (wie modell::Feld::typHinten)

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
    Verweist,  ///< ueber einen Zeiger, oder der Typ ist eine Aufzaehlung (Assoziation)
    Erbt,      ///< von = der abgeleitete Typ, nach = die Basis
    Realisiert,  ///< von = der Typ, nach = die Schnittstelle, die er umsetzt (gestrichelt, Spitze wie Erbt)
};

struct Beziehung
{
    std::size_t von = 0;   ///< Stelle in Strukturdiagramm::kaesten
    std::size_t nach = 0;
    BeziehungArt art = BeziehungArt::Verweist;
    std::string beschriftung;  ///< der Name des Glieds
};

struct Strukturdiagramm
{
    std::string titel;
    std::vector<Kasten> kaesten;
    std::vector<Beziehung> beziehungen;
};

}  // namespace c2d::diagramm
