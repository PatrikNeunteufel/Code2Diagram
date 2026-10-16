/**
 * @file Aufrufgraph.hpp
 * @brief Der Aufrufgraph eines Projekts: ab einer Funktion, oder einer Datei
 *
 * Hier wird entschieden, was ein Aufruf meint (kandidaten in Projekt.hpp: erst
 * die eigene Datei, dann was nicht `static` ist). Das Modell des Graphen
 * (diagramm/Aufrufgraph.hpp) kennt kein Projekt; die Ableitung steht deshalb
 * hier und nicht in diagramm/Ableitung.hpp.
 *
 * - Jede Funktion ist ein Knoten, je rufendem Paar eine Kante.
 * - Meint ein Aufruf mehrere Funktionen, fuehrt zu jeder eine Kante der Art
 *   Mehrdeutig, und eine Meldung nennt die Kandidaten.
 * - Ein Aufruf ohne Funktion mit Rumpf im Projekt wird ein Knoten Fremd — oder
 *   Makro, wenn eine Datei des Projekts ein Makro dieses Namens mit Parametern
 *   definiert. `ohneFremde` laesst beide weg und zaehlt sie.
 * - Die Bedingungen der Uebersetzung kommen aus der Lesart: mit Varianten
 *   (LeseOptionen::varianten) steht ein Aufruf in einem Zweig, der nicht gilt,
 *   unter dessen Bedingung; mit Rahmen (LeseOptionen::bereiche) einer in einem
 *   bedingten Bereich unter dessen Bedingung.
 */
#pragma once

#include "c2d/diagramm/Aufrufgraph.hpp"
#include "c2d/projekt/Projekt.hpp"

#include <cstddef>
#include <vector>

namespace c2d::projekt
{

enum class AufrufRichtung
{
    Gerufene,  ///< was die Funktion ruft, und was das ruft ...
    Aufrufer,  ///< wer sie ruft, und wer den ruft ...
    Beide,     ///< beides von ihr aus: nach unten nur weiter nach unten, nach oben nur nach oben
};

struct AufrufgraphOptionen
{
    AufrufRichtung richtung = AufrufRichtung::Gerufene;
    /// Wie viele Stufen von der Funktion aus; 0 = alle. Eine Funktion an der Grenze,
    /// die weiter ruft (gerufen wird), traegt AufrufKnoten::weiter.
    int tiefe = 2;
    bool ohneFremde = false;  ///< Fremd und Makro weglassen
    /// Hoechstens so viele Funktionen (Knoten der Art Funktion); 0 = unbegrenzt. Ist die Grenze
    /// erreicht, kommt keine weitere dazu: wer sie riefe (gerufen wuerde), traegt `weiter`, und
    /// AufrufgraphErgebnis::begrenzt sagt es. Was eine aufgeklappte Funktion ruft, zaehlt nicht.
    std::size_t hoechstens = 300;
    /// Funktionen, die an der Grenze (im Graph einer Datei: ausserhalb von ihr) trotzdem
    /// eine Stufe weiter zeigen, was sie rufen (nach oben: wer sie ruft) — aufgeklappt im Fenster.
    std::vector<Fundstelle> aufgeklappt;
};

struct AufrufgraphErgebnis
{
    diagramm::Aufrufgraph graph;
    /// Je Knoten, wo seine Funktion steht; eine leere Fundstelle bei Fremd und Makro.
    std::vector<Fundstelle> stellen;
    /// Je mehrdeutigem Aufruf einer: in der Datei, die ruft, mit der Zeile.
    std::vector<Projektmeldung> meldungen;
    /// Wie viele verschiedene Namen `ohneFremde` weggelassen hat.
    std::size_t ausgelassen = 0;
    /// Hat `hoechstens` Funktionen weggelassen?
    bool begrenzt = false;
};

/// Der Graph ab der Funktion an `wurzel`, in `optionen.richtung`, bis `optionen.tiefe`.
/// Eine Fundstelle, die keine Funktion mit Rumpf benennt, gibt einen leeren Graphen.
[[nodiscard]] AufrufgraphErgebnis aufrufgraphAb(const Projekt& projekt, const Fundstelle& wurzel,
                                                const AufrufgraphOptionen& optionen = {});

/// Der Graph einer Datei: jede ihrer Funktionen mit Rumpf und was sie rufen. Ein Aufruf in
/// eine andere Datei endet am Knoten der gerufenen Funktion (die `weiter` traegt, wenn sie
/// selbst ruft). Richtung und Tiefe gelten nicht.
[[nodiscard]] AufrufgraphErgebnis aufrufgraphDerDatei(const Projekt& projekt, std::size_t datei,
                                                      const AufrufgraphOptionen& optionen = {});

}  // namespace c2d::projekt
