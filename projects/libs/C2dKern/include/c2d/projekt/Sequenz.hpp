/**
 * @file Sequenz.hpp
 * @brief Das Sequenzdiagramm „Start der Kerne“ (Entscheide SQ1–SQ4): je Kern seine Startfunktion bis in die
 *        Hauptschleife, die IPC zwischen den Kernen, Barrieren, und je IPC-Handler ein Abschnitt
 *
 * Nur das Projekt weiss, was ein Aufruf meint und welche Dateien auf welchem Kern laufen (Projektdatei, `kerne`);
 * deshalb liegt die Ableitung hier. Die IPC erkennt eine Tabelle im Code (TI C2000 driverlib, Entscheid SQ3), dazu
 * eigene Huellen, deren Rumpf eine IPC mit einem ihrer Parameter als Flag ruft.
 */
#pragma once

#include "c2d/diagramm/Sequenz.hpp"
#include "c2d/projekt/Projekt.hpp"

#include <vector>

namespace c2d::projekt
{

/// Ein Kern mit seinen Dateien, gelesen mit seinen Definitionen (liesKern).
struct KernProjekt
{
    Kern kern;
    Projekt projekt;
};

/// Liest je Kern der Projektdatei seine Dateien (liesKern); ohne `kerne` leer.
[[nodiscard]] std::vector<KernProjekt> liesKerne(const Projekt& projekt, const ProjektOptionen& optionen = {});

struct SequenzOptionen
{
    /// Bis zu dieser Tiefe stehen Aufrufe als Pfeile da (1 = was die Startfunktion ruft; 0 = keine, nur IPC); die IPC
    /// wird in jeder Tiefe gefunden.
    int tiefe = 1;
};

/// Je Kern ab seiner Startfunktion bis in die Hauptschleife (die einmal als `loop`), zwischen den Kernen Boot, Signale,
/// Nachrichten und Barrieren; zwischen zwei Barrieren steht, was die Kerne zugleich tun, als `par`; je IPC-Handler ein
/// Abschnitt. Was sich nicht zuordnen laesst, wird ein Hinweis.
[[nodiscard]] diagramm::Sequenzdiagramm startDerKerne(const std::vector<KernProjekt>& kerne, const SequenzOptionen& optionen = {});

}  // namespace c2d::projekt
