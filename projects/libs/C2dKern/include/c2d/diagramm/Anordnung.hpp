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

#include "c2d/diagramm/Abfragediagramm.hpp"
#include "c2d/diagramm/Aufrufgraph.hpp"
#include "c2d/diagramm/Datenfluss.hpp"
#include "c2d/diagramm/Entscheidungstabelle.hpp"
#include "c2d/diagramm/ErDiagramm.hpp"
#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Konfigurationstabelle.hpp"
#include "c2d/diagramm/Plan.hpp"
#include "c2d/diagramm/Schaltstellen.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"
#include "c2d/diagramm/Struktogramm.hpp"
#include "c2d/diagramm/Wahrheitstabelle.hpp"
#include "c2d/diagramm/Sequenz.hpp"
#include "c2d/diagramm/Zeitachse.hpp"
#include "c2d/diagramm/Zuordnung.hpp"
#include "c2d/diagramm/Zustandsdiagramm.hpp"

#include <array>
#include <cstddef>
#include <span>
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
    /// Quer zur Richtung (nach unten die Breite, nach rechts die Hoehe): wie weit die Anordnung
    /// reicht (`quer`), und was ihre breiteste Schicht samt Stuetzpunkten und Abstaenden, Rand,
    /// Rahmen, Platz fuer Rueckwege und Schlaufen und (nach unten) die Legende brauchen
    /// (`querBedarf`). Massstab des Breiten-Tests (querVerhaeltnis).
    double quer = 0;
    double querBedarf = 0;
};

struct StrukturAnordnung
{
    std::vector<Rechteck> kaesten;
    std::vector<Linienzug> beziehungen;  ///< vom Rand des einen Kastens zum Rand des anderen
    /// je Beziehung: die Mitte ihrer Beschriftung — neben ihrer Linie, wo sie weder eine andere
    /// Beschriftung noch einen Kasten trifft (geschaetzt wie ein Knotentext, kursiv gezeichnet)
    std::vector<Punkt> beschriftungen;
    /// Was an einem Ende eines Fremdschluessels steht (Strukturdiagramm::notation): in Tables der Text
    /// der Vielfachheit, mittig bei `textMitte`; in Crow's foot Striche (ein Strich quer = eins, drei
    /// Striche zum Ende = viele) und Kreise (Radius kKreis, gefuellt mit dem Grund = null).
    struct Ende
    {
        std::vector<Linienzug> striche;
        std::vector<Punkt> kreise;
        std::string text;
        Punkt textMitte;
    };
    /// je Beziehung: [0] am Ende `von`, [1] am Ende `nach`; leer ausser bei Fremdschluesseln
    std::vector<std::array<Ende, 2>> enden;
    double breite = 0;
    double hoehe = 0;
    /// Quer zur Richtung (nach unten die Breite, nach rechts die Hoehe): wie weit die Anordnung
    /// reicht (`quer`), und was ihre breiteste Schicht samt Stuetzpunkten und Abstaenden, Rand,
    /// Rahmen, Platz fuer Rueckwege und Schlaufen und (nach unten) die Legende brauchen
    /// (`querBedarf`). Massstab des Breiten-Tests (querVerhaeltnis).
    double quer = 0;
    double querBedarf = 0;
};

/// Der Radius eines Kreises am Ende in Crow's foot (null).
inline constexpr double kKreis = 4.5;

/// Ein ER-Diagramm (Chen, (min,max)): je Knoten sein Rechteck — eine Raute steht in ihm —, je
/// Attribut seine Ellipse und die Linie zu ihrem Knoten, je Kante ihr Weg und die Mitte ihres Texts.
struct ErAnordnung
{
    std::vector<Rechteck> knoten;
    struct Ellipse
    {
        Rechteck ort;
        std::size_t knoten = 0;    ///< zu welchem Knoten
        std::size_t attribut = 0;  ///< Stelle in ErKnoten::attribute
        Linienzug linie;           ///< von der Ellipse zum Knoten
    };
    std::vector<Ellipse> ellipsen;
    std::vector<Linienzug> kanten;
    std::vector<Punkt> texte;  ///< je Kante: die Mitte ihres Texts (am Ende der Entitaet)
    double breite = 0;
    double hoehe = 0;
    double quer = 0;
    double querBedarf = 0;
};

/// Hoechstens so viel breiter (nach rechts: hoeher) als ihre breiteste Schicht braucht darf eine
/// geschichtete Anordnung sein (Ablauf, Aufrufgraph, Zustandsautomat, Datentypen). Darueber hat sie
/// sich aufgeschaukelt: eine Treppe, ein Band, das Band um Band weiter rueckt (Entscheid Patrik
/// 2026-10-17). Bleibt fest; reisst eine echte Form sie zu Recht, wird das vermerkt.
inline constexpr double kHoechstesQuerVerhaeltnis = 2;

/// quer / querBedarf; 1 ohne Bedarf (leer).
template <typename Anordnung>
[[nodiscard]] double querVerhaeltnis(const Anordnung& a)
{
    return a.querBedarf > 0 ? a.quer / a.querBedarf : 1;
}

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

/// Eine Tabelle (Wahrheits- oder Entscheidungstabelle) als Zellen und Textzeilen. Die Texte stehen
/// schon gekuerzt darin; `ganz` traegt den vollen. `knoten`: was ein Klick trifft — bei der
/// Wahrheitstabelle die Tabelle, bei der Entscheidungstabelle die Zeile (erst die Bedingungen,
/// dann die Aktionen); je Knoten sein Rechteck in `knotenOrte` (fuer den Ring).
struct TabellenAnordnung
{
    struct Zelle
    {
        Rechteck ort;
        std::vector<std::string> zeilen;  ///< mittig
        bool kopf = false;                ///< die Kopfzeile: getoent, fett
        bool getoent = false;             ///< eine Zeile der MC/DC-Menge, der Zweig, der gilt
        std::string ganz;                 ///< der volle Text (Tooltip); leer = wie zeilen
        std::size_t tabelle = 0;          ///< welche Tabelle
        std::size_t knoten = kKeine;      ///< was ein Klick trifft; kKeine = nichts
        bool links = false;               ///< Text linksbuendig (die Spalte der Bedingungen und Aktionen)
        bool grau = false;                ///< grauer Text: die Spalte eines Namens, den die Datei selbst definiert
    };
    struct Text
    {
        Punkt links;  ///< linker Rand, Mitte der Zeile
        std::string text;
        bool fett = false;
        std::size_t tabelle = 0;
    };
    std::vector<Zelle> zellen;
    std::vector<Text> texte;
    std::vector<Linienzug> linien;     ///< die doppelte Linie zwischen Bedingungen und Aktionen
    std::vector<Rechteck> knotenOrte;  ///< je Knoten sein Rechteck
    double breite = 0;
    double hoehe = 0;
};

/// Eine Zeitachse (Lauf von Spark): links die Titel der Bahnen, rechts die Zeit (linear, x), oben die Achse mit
/// Strichen; je Balken sein Rechteck und der Text, der hineinpasst (leer = keiner). Wer zeichnet, rechnet nichts.
struct ZeitAnordnung
{
    struct Bahn
    {
        Rechteck ort;       ///< die ganze Zeile, ueber Titel und Zeit
        std::string titel;
        Punkt text;         ///< linker Rand des Titels, Mitte der Zeile
        bool job = false;   ///< eine Zeile der Jobs (getoent)
        double entfernt = -1;  ///< x, an dem der Executor entfernt wurde; < 0 = nicht
    };
    struct Strich
    {
        Linienzug linie;
        std::string text;  ///< `10 s`
        Punkt textOrt;     ///< Mitte des Texts
    };
    std::vector<Bahn> bahnen;
    std::vector<Rechteck> balken;     ///< je Balken der Zeitachse
    std::vector<std::string> texte;   ///< je Balken: was hineinpasst
    std::vector<Strich> achse;
    Rechteck zeit;                    ///< die Flaeche der Zeit (unter der Achse)
    double breite = 0;
    double hoehe = 0;
};

[[nodiscard]] ZeitAnordnung ordneAn(const Zeitachse& achse, const AnordnungOptionen& optionen = {});

/// Ein Sequenzdiagramm: oben je Lebenslinie ein Kopf, darunter ihre Linie; je Schritt eine Zeile — ein Aufruf als
/// Schlaufe rechts an der eigenen Linie (je Tiefe weiter eingerueckt), eine Nachricht als Pfeil zwischen zwei Linien,
/// eine Barriere als dicke Linie ueber die Beteiligten; Fragmente als Rahmen ueber die ganze Breite (je Ebene
/// eingerueckt) mit Wort und Bedingung, weitere Zweige mit gestrichelter Linie. Die Texte sind schon gekuerzt; wer
/// zeichnet, rechnet nichts. Ein Pfeil und eine Barriere sind fuer Klick und markiere() ein Knoten: ihr Schritt.
struct SequenzAnordnung
{
    struct Kopf
    {
        Rechteck ort;
        std::string name;
    };
    struct Pfeil
    {
        Linienzug linie;  ///< vom Absender zum Empfaenger; ins Leere endet er mit einem Kreuz bei `linie.back()`
        NachrichtArt art = NachrichtArt::Aufruf;
        bool fremd = false;
        bool insLeere = false;
        std::string text;
        Punkt textOrt;  ///< links, Mitte der Zeile
        Rechteck treffer;  ///< wo ein Klick ihn trifft (Linie und Text)
        std::size_t schritt = 0;
    };
    struct Barriere
    {
        Linienzug linie;
        std::string text;
        Punkt textOrt;  ///< Mitte des Texts
        Rechteck treffer;
        std::size_t schritt = 0;
    };
    struct Zweig
    {
        Linienzug linie;  ///< gestrichelt, quer durch den Rahmen
        std::string text;  ///< `[else]`
        Punkt textOrt;     ///< links, Mitte der Zeile
    };
    struct Fragment
    {
        Rechteck ort;
        std::string wort;   ///< `loop`, `alt` … (im Schild links oben)
        Rechteck schild;    ///< das Schild mit dem Wort
        std::string titel;  ///< `[main loop: for (;;)]`
        Punkt titelOrt;     ///< links, Mitte der Zeile
        std::vector<Zweig> zweige;
        std::size_t schritt = 0;
    };
    std::vector<Kopf> koepfe;
    std::vector<Linienzug> lebenslinien;
    std::vector<Fragment> fragmente;  ///< aeussere vor inneren
    std::vector<Pfeil> pfeile;
    std::vector<Barriere> barrieren;
    std::vector<std::pair<std::string, Punkt>> hinweise;  ///< Text, links oben
    double breite = 0;
    double hoehe = 0;
};

[[nodiscard]] SequenzAnordnung ordneAn(const Sequenzdiagramm& diagramm, const AnordnungOptionen& optionen = {});

/// So viele Zeichen zeigt der Kopf eines Atoms; den Rest der Tooltip.
inline constexpr std::size_t kAtomZeichen = 30;

[[nodiscard]] FlussAnordnung ordneAn(const Flussdiagramm& diagramm, const AnordnungOptionen& optionen = {});
[[nodiscard]] StruktoAnordnung ordneAn(const Struktogramm& diagramm, const AnordnungOptionen& optionen = {});
[[nodiscard]] TabellenAnordnung ordneAn(std::span<const Wahrheitstabelle> tabellen, const AnordnungOptionen& optionen = {});
/// Klassisch (DIN 66241): links die Bedingungen, dann die Aktionen (doppelte Linie dazwischen),
/// je Regel eine Spalte R1, R2 … mit Y/N/–/Wert bzw. X. Knoten: je Zeile.
[[nodiscard]] TabellenAnordnung ordneAn(const Entscheidungstabelle& tabelle, const AnordnungOptionen& optionen = {});
/// Zuordnungen untereinander: je Tabelle ihr Titel, ein Kopf (Schluessel, Spalten), je Zeile ein
/// Knoten — gezaehlt ueber alle Tabellen.
[[nodiscard]] TabellenAnordnung ordneAn(std::span<const Zuordnung> tabellen, const AnordnungOptionen& optionen = {});
/// Konfigurationstabellen untereinander, eine innere Kette eingerueckt: Titel, Kopf (Zweig, je
/// Name mit seinem Stand, active), je Zeile ein Knoten — gezaehlt ueber alle Tabellen; Hinweise darunter.
[[nodiscard]] TabellenAnordnung ordneAn(std::span<const Konfigurationstabelle> tabellen,
                                        const AnordnungOptionen& optionen = {});
/// Wo ein Name schaltet: Titel, die Tabelle *set by*, darunter je Zweig eine Zeile, Hinweise.
/// Knoten: erst je Setzung, dann je Zeile (Schaltstellen::gesetzt, dann ::zeilen).
[[nodiscard]] TabellenAnordnung ordneAn(const Schaltstellen& tabelle, const AnordnungOptionen& optionen = {});
/// Die Namen des Projekts: je Name eine Zeile und ein Knoten; *differs* getoent, von der Sprache Vorgegebenes grau.
[[nodiscard]] TabellenAnordnung ordneAn(const Schalteruebersicht& tabelle, const AnordnungOptionen& optionen = {});
/// Ein Strukturdiagramm in Tables oder Crow's foot (fuer Chen und (min,max): erDiagrammVon, dann das ErDiagramm).
[[nodiscard]] StrukturAnordnung ordneAn(const Strukturdiagramm& diagramm, const AnordnungOptionen& optionen = {});
/// Chen und (min,max): die Attribute links und rechts ihres Knotens (nach rechts: darueber und
/// darunter), die Knoten samt Attributen in Schichten wie die Datentypen.
[[nodiscard]] ErAnordnung ordneAn(const ErDiagramm& diagramm, const AnordnungOptionen& optionen = {});

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

/// Eine Abfrage ordnet sich wie ein Flussdiagramm: je Knoten ein Knoten (sein Titel, darunter seine
/// Zeilen), je Kante eine Kante (beschriftet mit ihrem Text), je Rahmen eine Gruppe — verschachtelt wie
/// die Unterabfragen. `gruppen` je Rahmen, in der Reihenfolge des Modells.
[[nodiscard]] FlussAnordnung ordneAn(const Abfragediagramm& diagramm, const AnordnungOptionen& optionen = {});

/// Ein Datenfluss ordnet sich wie eine Abfrage: je Knoten ein Knoten (texte(): Stereotyp, Titel, Zeilen), je
/// Kante eine Kante, je Rahmen (Stage, Funktion) eine Gruppe; je oberster Rahmen ein Band.
[[nodiscard]] FlussAnordnung ordneAn(const Datenflussdiagramm& diagramm, const AnordnungOptionen& optionen = {});

/// Ein Plan ordnet sich wie ein Datenfluss (texte(), je Rahmen eine Gruppe, je innerster Rahmen ein Band); eine
/// Gruppe sind die Baender einer Unterabfrage bzw. des Plans selbst.
[[nodiscard]] FlussAnordnung ordneAn(const Plandiagramm& diagramm, const AnordnungOptionen& optionen = {});

/// Wie viele Zeichen des Titels in den Rahmen `rahmen` passen (mindestens ein
/// paar): wer zeichnet, kuerzt ihn darauf — ganz steht er in der Legende.
[[nodiscard]] std::size_t titelZeichen(const Rechteck& rahmen, const AnordnungOptionen& optionen);

/// Die Hoehe des Kopfs eines Kastens (Art und Name) bei diesen Optionen.
[[nodiscard]] double kopfHoehe(const AnordnungOptionen& optionen);

}  // namespace c2d::diagramm
