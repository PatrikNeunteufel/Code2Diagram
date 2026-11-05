/**
 * @file Abhaengigkeiten.hpp
 * @brief Die Abhaengigkeiten der Daten (Entscheide A1–A4): wer welche Tabelle liest, schreibt, ruft
 *
 * Ein Graph auf dem Modell des Aufrufgraphen (diagramm/Aufrufgraph.hpp): Knoten sind Tabellen, Views,
 * Trigger, Routinen der Datenbank und Funktionen des Host-Codes mit SQL; Kanten liest, schreibt, ruft und
 * — von einer Tabelle zu ihrem Trigger — loest aus. Was ein Name meint, entscheidet das Projekt: ohne
 * Ruecksicht auf Gross/klein, ohne Schema ueber den letzten Teil (eindeutig). Eine Tabelle, die das
 * Projekt nicht definiert, ist ein Knoten FremdeTabelle — aber nur aus SQL in klarer Form
 * (modell::Zugriff::unsicher) und nicht aus einem Ausdruck (`count(`, modell::Zugriff::imAusdruck).
 */
#pragma once

#include "c2d/diagramm/Aufrufgraph.hpp"
#include "c2d/projekt/Aufrufgraph.hpp"
#include "c2d/projekt/Projekt.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::projekt
{

/// Wo ein Knoten des Graphen herkommt.
struct Datenstelle
{
    enum class Art
    {
        Tabelle,   ///< einheit.typen[index]
        Objekt,    ///< einheit.objekte[index]
        Funktion,  ///< einheit.funktionen[index]
        Fremd,     ///< nichts des Projekts
    };
    Art art = Art::Fremd;
    std::size_t datei = kKeine;
    std::size_t index = kKeine;
};

struct AbhaengigkeitsOptionen
{
    AufrufRichtung richtung = AufrufRichtung::Gerufene;  ///< Gerufene: was es liest, schreibt, ruft; Aufrufer: wer es tut
    int tiefe = 2;                                       ///< 0 = alle Stufen
    std::size_t hoechstens = 300;                        ///< Knoten des Projekts; 0 = unbegrenzt
};

struct AbhaengigkeitsErgebnis
{
    diagramm::Aufrufgraph graph;
    std::vector<Datenstelle> stellen;  ///< je Knoten
    bool begrenzt = false;             ///< die Grenze `hoechstens` war erreicht
};

/// Die Namen, ab denen ein Graph gezeichnet werden kann, in einer Datei: ihre Tabellen, Objekte und Funktionen
/// mit SQL — wie abhaengigkeitenAb sie nimmt (Tabelle und Objekt mit ihrem Namen, ein Trigger `name@tabelle`,
/// eine Funktion mit ihrem vollen Namen).
[[nodiscard]] std::vector<std::string> datenobjekteIn(const Projekt& projekt, std::size_t datei);

/// Ab einem Objekt (einem Namen aus datenobjekteIn, in der Datei `datei`), in der Richtung und so tief wie gewaehlt.
[[nodiscard]] AbhaengigkeitsErgebnis abhaengigkeitenAb(const Projekt& projekt, std::size_t datei, std::string_view name,
                                                       const AbhaengigkeitsOptionen& optionen = {});

/// Alles einer Datei — ihre Tabellen, Objekte, Funktionen mit SQL — und was sie direkt lesen, schreiben,
/// rufen oder was sie liest, schreibt, ruft.
[[nodiscard]] AbhaengigkeitsErgebnis abhaengigkeitenDerDatei(const Projekt& projekt, std::size_t datei);

}  // namespace c2d::projekt
