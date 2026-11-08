/**
 * @file Sequenz.hpp
 * @brief Das Sequenzdiagramm (Entscheide SQ1–SQ4): Lebenslinien und, von oben nach unten, was zwischen ihnen
 *        geschieht — Aufrufe, Nachrichten zwischen Kernen, Barrieren, Fragmente (loop, alt, opt, par, Abschnitte)
 *
 * Ein Modell fuer beide Ansichten: „Start der Kerne“ (je Kern eine Lebenslinie, IPC zwischen ihnen) und spaeter „ab
 * einer Funktion“ (je Datei bzw. Klasse eine). Die Schritte stehen in der Reihenfolge der Zeit; ein Fragment ist eine
 * Klammer aus Anfang, vielleicht weiteren Zweigen und Ende. Wer das Diagramm ableitet, kennt das Projekt; das Modell
 * kennt nur Namen, Texte und Stellen.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

/// Eine Lebenslinie: ein Kern (`CPU1`), eine Datei, eine Klasse.
struct Lebenslinie
{
    std::string name;
};

/// Was ein Pfeil bedeutet.
enum class NachrichtArt
{
    Aufruf,     ///< ein Aufruf auf der eigenen Lebenslinie (von == nach); `tiefe` sagt, wie tief
    Boot,       ///< ein Kern startet einen anderen (`IPC_setBootMode` + FLAG0)
    Signal,     ///< ein Flag an den anderen Kern (`IPC_setFlagLtoR`), ohne zu warten
    Nachricht,  ///< ein Kommando oder eine Nachricht mit Inhalt (`IPC_sendCommand`, eine Queue)
};

/// Wovon ein Fragment spricht.
enum class FragmentArt
{
    Schleife,     ///< `loop`: eine Schleife; die Hauptschleife eines Kerns heisst so im Titel
    Alternative,  ///< `alt`: if/else, switch — weitere Zweige mit SequenzSchritt::Art::Sonst
    Option,       ///< `opt`: ein if ohne else
    Parallel,     ///< `par`: was die Kerne zwischen zwei Barrieren zugleich tun — je Kern ein Zweig
    Abschnitt,    ///< ein eigener Abschnitt: was ein IPC-Flag ausloest (`on IPC flag 0`)
};

struct SequenzSchritt
{
    enum class Art
    {
        Nachricht,       ///< ein Pfeil (NachrichtArt)
        Barriere,        ///< Kerne warten aufeinander (`IPC_sync`): eine Linie ueber `beteiligt`
        FragmentAnfang,  ///< oeffnet ein Fragment (FragmentArt), `text` ist seine Bedingung bzw. sein Titel
        Sonst,           ///< ein weiterer Zweig im offenen Fragment, `text` seine Bedingung (`else`, `case 2`, `CPU2`)
        FragmentEnde,    ///< schliesst das innerste offene Fragment
    };

    Art art = Art::Nachricht;
    NachrichtArt nachricht = NachrichtArt::Aufruf;
    FragmentArt fragment = FragmentArt::Schleife;
    std::size_t von = 0;   ///< Lebenslinie
    std::size_t nach = 0;  ///< Lebenslinie; kKeinZiel = ins Leere (ein Signal ohne Empfaenger)
    std::string text;      ///< `INIT_ADC()`, `FLAG 0`, `boot`, die Bedingung, der Titel
    int tiefe = 0;         ///< Aufruf: 1 = von der Startfunktion gerufen, 2 = von dort weiter …
    bool fremd = false;    ///< Aufruf: ohne Rumpf im Projekt (Bibliothek, SDK)
    /// Barriere: die Lebenslinien, die aufeinander warten (bei einer Seite ohne Gegenstueck nur diese).
    std::vector<std::size_t> beteiligt;
    /// Wo es im Quelltext steht (relativ zum Wurzelordner) — fuer Klick und Tooltip; leer = nirgends.
    std::string pfad;
    int zeile = 0;
};

inline constexpr std::size_t kKeinZiel = static_cast<std::size_t>(-1);

struct Sequenzdiagramm
{
    std::string titel;
    std::vector<Lebenslinie> linien;
    std::vector<SequenzSchritt> schritte;
    /// Was das Diagramm nicht zeigt oder nicht zuordnen konnte (ein Flag ohne Gegenstueck, eine Startfunktion, die
    /// fehlt); im Diagramm unter den Schritten.
    std::vector<std::string> hinweise;
};

/// Der Text eines Fragments in Mermaid und im Bild: `loop`, `alt`, `opt`, `par`, `section`.
[[nodiscard]] const char* fragmentWort(FragmentArt art);

}  // namespace c2d::diagramm
