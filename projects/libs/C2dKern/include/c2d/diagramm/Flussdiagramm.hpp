/**
 * @file Flussdiagramm.hpp
 * @brief Das Diagramm-Modell: Knoten und Kanten eines Flussdiagramms
 *
 * Die Schicht zwischen Analyse und Ausgabe. Die Ableitung (Ableitung.hpp) baut
 * dieses Modell aus dem Codemodell; wer ein Format schreibt
 * (ausgabe/Ausgabe.hpp) oder es zeichnet (Anordnung.hpp), liest es. Keine der
 * Seiten kennt die andere. Den Vorgaengern fehlte genau diese Schicht
 * (docs/Ernte_Vorgaenger.md).
 *
 * Ein Diagramm kann andere in sich tragen: `betteEin` setzt den Ablauf einer
 * gerufenen Funktion hinter ihren Aufruf, umrahmt als `Gruppe`; `loeseEin`
 * nimmt ihn wieder heraus.
 */
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::diagramm
{

enum class KnotenArt
{
    Start,
    Ende,
    Anweisung,    ///< eine oder mehrere Anweisungen ohne Verzweigung
    Verzweigung,  ///< Bedingung: if, Schleifenkopf, case
    Aufruf,       ///< eine Anweisung, die nur eine Funktion ruft
    /// Varianten der bedingten Uebersetzung (`#if`): keine Entscheidung zur
    /// Laufzeit; je Zweig eine Kante mit seiner Bedingung. Ein Sechseck.
    Variante,
};

/// Kennung eines Knotens: seine Stelle in Flussdiagramm::knoten().
using KnotenId = std::size_t;

/// "Keine": fuer Kennungen von Knoten und Gruppen.
inline constexpr std::size_t kKeine = static_cast<std::size_t>(-1);

struct Knoten
{
    KnotenArt art = KnotenArt::Anweisung;
    std::vector<std::string> zeilen;  ///< Text, je Anweisung eine Zeile
    int quellZeile = 0;               ///< Zeile im Quelltext, 0 = keine
    /// Die Funktionen, die der Text ruft — mit dem Bereich, den der Text nennt
    /// (`N::q`), ohne Objekt davor; in der Reihenfolge des Texts, jeder einmal.
    /// Auch bei Knoten anderer Art (`x = lies(a)`, `if (pruefe(x))`).
    std::vector<std::string> aufrufe;
    std::size_t gruppe = kKeine;  ///< die innerste Gruppe, in der der Knoten liegt
    /// Kein Weg vom Start fuehrt hierher (Code hinter einer Hauptschleife): grau und
    /// gestrichelt gezeichnet, unter dem Ablauf.
    bool unerreichbar = false;
};

struct Kante
{
    KnotenId von = 0;
    KnotenId nach = 0;
    std::string beschriftung;  ///< etwa "yes" / "no"; leer = keine
};

enum class GruppenArt
{
    Einbettung,  ///< der eingebettete Ablauf einer gerufenen Funktion (betteEin)
    Bedingung,   ///< was nur unter einer Bedingung der Uebersetzung steht; titel = die Bedingung
    Datei,       ///< die Funktionen einer Datei im Aufrufgraph (ordneAn); titel = ihr Pfad
};

/// Ein umrahmter Teil: der eingebettete Ablauf einer gerufenen Funktion, oder
/// was unter einer Bedingung der Uebersetzung steht.
struct Gruppe
{
    GruppenArt art = GruppenArt::Einbettung;
    std::string titel;               ///< etwa die Signatur der Funktion, die Bedingung
    std::size_t eltern = kKeine;     ///< die Gruppe, in der diese liegt
    KnotenId aufrufer = kKeine;      ///< der Knoten, hinter dem sie haengt
    std::vector<Kante> ersetzt;      ///< die Kanten des Aufrufers vor dem Einbetten
    /// Woher der Ablauf stammt — fuer wen das Diagramm zeigt, etwa der Pfad der
    /// Datei. Das Diagramm deutet ihn nicht; leer = von dort, wo das Diagramm herkommt.
    std::string herkunft;
};

class Flussdiagramm
{
public:
    explicit Flussdiagramm(std::string titel = {});

    KnotenId neuerKnoten(KnotenArt art, std::string_view text, int quellZeile = 0);
    KnotenId neuerKnoten(Knoten knoten);
    /// Wirft std::out_of_range, wenn eine der Kennungen keinen Knoten benennt.
    void verbinde(KnotenId von, KnotenId nach, std::string_view beschriftung = {});
    /// Eine Zeile unter (`vorn`: ueber) den Text eines Knotens setzen — etwa der Start einer
    /// Funktion, die nie zurueckkehrt, oder der Hinweis auf Code, der nie erreicht wird.
    void ergaenzeZeile(KnotenId knoten, std::string_view zeile, bool vorn = false);
    /// Den Knoten als unerreichbar kennzeichnen (Knoten::unerreichbar).
    void markiereUnerreichbar(KnotenId knoten);
    std::size_t neueGruppe(Gruppe gruppe);

    [[nodiscard]] const std::string& titel() const noexcept { return m_titel; }
    [[nodiscard]] const std::vector<Knoten>& knoten() const noexcept { return m_knoten; }
    [[nodiscard]] const std::vector<Kante>& kanten() const noexcept { return m_kanten; }
    [[nodiscard]] const std::vector<Gruppe>& gruppen() const noexcept { return m_gruppen; }

    /// Liegt die Gruppe `innen` in `aussen` — oder ist sie es selbst?
    [[nodiscard]] bool liegtIn(std::size_t innen, std::size_t aussen) const;
    /// Die innerste Einbettung, in der die Gruppe liegt (oder sie selbst); kKeine = keine.
    [[nodiscard]] std::size_t einbettungVon(std::size_t gruppe) const;
    /// Wie viele Gruppen Einbettungen sind.
    [[nodiscard]] std::size_t einbettungen() const;

private:
    std::string m_titel;
    std::vector<Knoten> m_knoten;
    std::vector<Kante> m_kanten;
    std::vector<Gruppe> m_gruppen;
};

/// Wie viele Farben es fuer Rahmen von Bedingungen gibt; dann wiederholen sie sich.
inline constexpr std::size_t kRahmenFarben = 6;

/// Je Gruppe ihre Farbe (0 .. kRahmenFarben-1): gleiche Bedingung, gleiche Farbe,
/// vergeben in der Reihenfolge, in der die Bedingungen zuerst vorkommen. kKeine
/// fuer eine Einbettung. Welche Farbe eine Nummer ist, entscheidet, wer zeichnet
/// (rahmenFarbe ist die Vorgabe).
[[nodiscard]] std::vector<std::size_t> farbenDerRahmen(const Flussdiagramm& diagramm);

/// Die Farbe einer Nummer als `#rrggbb` — gut zu unterscheiden, hell wie dunkel lesbar.
[[nodiscard]] std::string_view rahmenFarbe(std::size_t farbe);

/// Folgen von Anweisungen zu einem Knoten buendeln: A -> B wird ein Knoten,
/// wenn beide Anweisungen sind, in derselben Gruppe liegen, A nur nach B fuehrt
/// und B nur von A kommt. Ein Knoten traegt hoechstens `maxZeilen` Zeilen
/// (0 = unbegrenzt); die Kennungen aendern sich. Fuer Diagramme ohne Gruppen
/// gedacht: erst buendeln, dann einbetten.
[[nodiscard]] Flussdiagramm fasseAnweisungsfolgenZusammen(const Flussdiagramm& diagramm,
                                                          std::size_t maxZeilen = 5);

/// Setzt den Ablauf `unter` hinter den Knoten `stelle` von `haupt`, als Gruppe
/// mit dem Titel `titel`: der Aufrufer fuehrt in den ersten Schritt von
/// `unter`, dessen Enden fuehren dorthin, wohin der Aufrufer fuehrte. Start
/// und Ende von `unter` entfallen. Die Kennungen von `haupt` bleiben, die neuen
/// Knoten haengen hinten an.
///
/// Unveraendert zurueck kommt `haupt`, wenn `stelle` kein Knoten ist, schon
/// eine Einbettung traegt oder `unter` ausser Start und Ende nichts enthaelt.
[[nodiscard]] Flussdiagramm betteEin(const Flussdiagramm& haupt, KnotenId stelle, const Flussdiagramm& unter,
                                     std::string_view titel, std::string_view herkunft = {});

/// Nimmt die Gruppe samt allem, was in ihr liegt, wieder heraus; der Aufrufer
/// bekommt seine Kanten zurueck. Die Kennungen aendern sich.
[[nodiscard]] Flussdiagramm loeseEin(const Flussdiagramm& diagramm, std::size_t gruppe);

}  // namespace c2d::diagramm
