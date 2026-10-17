/**
 * @file Anordnung.hpp
 * @brief Wo die Knoten eines Diagramms stehen: Rechtecke und Linienzuege
 *
 * Formate wie Mermaid und DOT ordnen selbst an; wer zeichnet (die Vorschau im
 * Fenster) oder ein Format mit festen Orten schreibt (draw.io), braucht die
 * Anordnung von hier. Ohne Qt: die Groesse eines Texts wird geschaetzt —
 * Zeichen mal `zeichenBreite` —, gedacht fuer eine Schrift fester Breite.
 *
 * Das Verfahren ist einfach gehalten: Schichten nach dem laengsten Weg; eine
 * Kante ueber mehrere Schichten bekommt in jeder Schicht dazwischen einen
 * Stuetzpunkt und damit eine eigene Spalte; Reihenfolge je Schicht nach dem
 * Schwerpunkt der Nachbarn; die Lage in der Zeile nach Brandes und Koepf (vier
 * Ausrichtungen, gemittelt — nichts weicht nur nach einer Seite aus); Kanten
 * zurueck aussen herum. Es vermeidet nicht jede Kreuzung.
 */
#pragma once

#include "c2d/diagramm/Aufrufgraph.hpp"
#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"
#include "c2d/diagramm/Struktogramm.hpp"
#include "c2d/diagramm/Zustandsdiagramm.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

struct Punkt
{
    double x = 0;
    double y = 0;
};

struct Rechteck
{
    double x = 0;  ///< links
    double y = 0;  ///< oben
    double breite = 0;
    double hoehe = 0;

    [[nodiscard]] Punkt mitte() const { return {x + breite / 2, y + hoehe / 2}; }
};

using Linienzug = std::vector<Punkt>;

/// Ein Knoten KnotenArt::Variante ist ein Sechseck: so weit ragen seine Spitzen
/// links und rechts ueber die gerade Ober- und Unterkante hinaus.
inline constexpr double kSechseckSpitze = 14;

/// Die Ecken des Sechsecks in `r`, im Uhrzeigersinn von links oben.
[[nodiscard]] std::vector<Punkt> sechseck(const Rechteck& r);

struct AnordnungOptionen
{
    bool nachRechts = false;  ///< das Diagramm waechst nach rechts statt nach unten
    double zeichenBreite = 7.2;
    double zeilenHoehe = 17;
    /// Wie bei der Ausgabe: laengere Zeilen werden gekuerzt gezeigt; 0 = nie.
    std::size_t maxZeilenLaenge = 60;
};

struct FlussAnordnung
{
    std::vector<Rechteck> knoten;    ///< je Knoten, in dessen Reihenfolge
    std::vector<Linienzug> kanten;   ///< je Kante: vom Rand der Quelle zum Rand des Ziels
    /// je Kante: die Mitte ihrer Beschriftung — auf dem Stueck, das nur ihr gehoert,
    /// nah an der Stelle, wo sie sich von den anderen Kanten ihrer Quelle trennt
    std::vector<Punkt> beschriftungen;
    std::vector<Rechteck> gruppen;   ///< je Gruppe der Rahmen um alles, was in ihr liegt; oben Platz fuer den Titel
    /// Je Bedingung, die eine Farbe hat (ein Rahmen, im Aufrufgraph eine Kante), eine
    /// Zeile rechts oben neben dem Diagramm: ein Farbfeld und dahinter die Bedingung.
    struct Legende
    {
        Rechteck feld;      ///< das Farbfeld
        Punkt text;         ///< wo der Text beginnt: links, auf halber Hoehe
        std::string titel;  ///< die Bedingung
        std::size_t farbe;  ///< ihre Farbe (rahmenFarbe)
    };
    std::vector<Legende> legende;
    double breite = 0;
    double hoehe = 0;
};

struct StrukturAnordnung
{
    std::vector<Rechteck> kaesten;
    std::vector<Linienzug> beziehungen;  ///< vom Rand des einen Kastens zum Rand des anderen
    /// je Beziehung: die Mitte ihrer Beschriftung — neben ihrer Linie, wo sie weder eine andere
    /// Beschriftung noch einen Kasten trifft (geschaetzt wie ein Knotentext, kursiv gezeichnet)
    std::vector<Punkt> beschriftungen;
    double breite = 0;
    double hoehe = 0;
};

/// Ein Struktogramm, gezeichnet als flache Liste: wer zeichnet, malt `felder` in ihrer
/// Reihenfolge (ein Block vor den Bloecken seiner Spalten, die liegen darueber), dann
/// `linien`, `beschriftungen`, `rahmen` und die Legende — und rechnet nichts selbst.
struct StruktoAnordnung
{
    enum class FeldArt
    {
        Titel,      ///< oben: die Signatur (getoent)
        Anweisung,  ///< auch eine Marke
        Aufruf,     ///< die doppelten Seiten stehen in `linien`
        Aussprung,  ///< der Pfeil nach links steht in `linien`
        Kopf,       ///< ein Block mit Spalten: sein Kopf, Text oben; die Spalten liegen darueber
        Leer,       ///< der Rest einer Spalte unter ihrem letzten Block, eine leere Spalte
    };
    struct Feld
    {
        Rechteck ort;
        FeldArt art = FeldArt::Anweisung;
        std::vector<std::string> zeilen;  ///< ungekuerzt; wer zeichnet, kuerzt auf maxZeilenLaenge
        Punkt text;                       ///< die Mitte des Texts
        std::size_t block = kKeine;       ///< Stelle in bloeckeVon(); kKeine: Titel oder Leer
        bool unerreichbar = false;        ///< grau und gestrichelt
    };
    struct Beschriftung
    {
        Punkt mitte;
        std::string text;  ///< `yes`, `no`, die Werte eines Falls, `catch (...)`
    };
    struct Rahmen
    {
        Rechteck ort;
        std::string titel;  ///< die Bedingung; gekuerzt auf titelZeichen()
        std::size_t farbe;  ///< rahmenFarbe
    };
    std::vector<Feld> felder;
    std::vector<Linienzug> linien;  ///< Diagonalen und Trennstriche der Koepfe, Seiten, Pfeile
    std::vector<Beschriftung> beschriftungen;
    std::vector<Rahmen> rahmen;
    std::vector<FlussAnordnung::Legende> legende;
    std::vector<Rechteck> bloecke;  ///< je Block (Stelle in bloeckeVon()) sein Rechteck
    double breite = 0;
    double hoehe = 0;
};

[[nodiscard]] FlussAnordnung ordneAn(const Flussdiagramm& diagramm, const AnordnungOptionen& optionen = {});
[[nodiscard]] StruktoAnordnung ordneAn(const Struktogramm& diagramm, const AnordnungOptionen& optionen = {});
[[nodiscard]] StrukturAnordnung ordneAn(const Strukturdiagramm& diagramm, const AnordnungOptionen& optionen = {});

/// Ein Zustandsdiagramm ordnet sich wie ein Flussdiagramm: je Zustand ein
/// Knoten, je Uebergang eine Kante (`knoten` und `kanten` in der Reihenfolge
/// des Modells). Der Punkt "von aussen" ist klein; Beschriftungen laenger als
/// `maxZeilenLaenge` zaehlen so lang, wie ein Format sie gekuerzt zeigt.
[[nodiscard]] FlussAnordnung ordneAn(const Zustandsdiagramm& diagramm, const AnordnungOptionen& optionen = {});

/// Ein Aufrufgraph ordnet sich wie ein Flussdiagramm: je Knoten ein Knoten, je Kante
/// eine Kante (in der Reihenfolge des Modells), beschriftet mit `beschriftung`. Mit
/// Rahmen je Datei (Aufrufgraph::mitRahmen) steht jede Datei in einem Band quer zur
/// Richtung — nach unten: einer Spalte —, die Bänder nebeneinander in der Reihenfolge
/// der Dateien, Fremd und Makro im letzten; so umfasst kein Rahmen einen fremden
/// Knoten. `gruppen` je Datei. Die Legende nennt die Bedingungen der Kanten mit Farbe.
[[nodiscard]] FlussAnordnung ordneAn(const Aufrufgraph& graph, const AnordnungOptionen& optionen = {});

/// Wie viele Zeichen des Titels in den Rahmen `rahmen` passen (mindestens ein
/// paar): wer zeichnet, kuerzt ihn darauf — ganz steht er in der Legende.
[[nodiscard]] std::size_t titelZeichen(const Rechteck& rahmen, const AnordnungOptionen& optionen);

/// Die Hoehe des Kopfs eines Kastens (Art und Name) bei diesen Optionen.
[[nodiscard]] double kopfHoehe(const AnordnungOptionen& optionen);

}  // namespace c2d::diagramm
