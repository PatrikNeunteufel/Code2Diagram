/**
 * @file Application.hpp
 * @brief Einstieg in die Anwendungslogik (App-Container von CMakeCraft)
 *
 * Das generische main.cpp erzeugt dieses Objekt und ruft init(), run(),
 * shutdown(). init() legt die QApplication und das Fenster an.
 *
 * Aufruf:  Code2Diagram [--testing] [datei] [--diagramm <name>] [--einbetten <funktion>]...
 *                       [--format <name>] [--ansicht preview|text] [--bild <datei.png>]
 *          --testing    laedt die Sichttest-Steuerung (sichttest/Anbindung.hpp)
 *          datei        wird beim Start geoeffnet
 *          --diagramm   waehlt eine Funktion (mit Bereich), `types` (alle) oder `types:<Typ>`, den Aufrufgraph der
 *                       Datei `calls` oder ab einer Funktion `calls:<funktion>`
 *          --tiefe n, --hoechstens n, --aufrufer | --beide, --ohne-fremde   stellen den Aufrufgraph ein
 *          --einbetten  bettet den Ablauf dieser Funktion hinter ihren ersten Aufruf ein
 *          -D NAME[=wert]  gilt beim Auswerten von #if/#ifdef als definiert; mehrfach erlaubt
 *          --alle-zweige   bedingte Uebersetzung nicht auswerten: alle Zweige lesen
 *          --bild       legt das Fenster als PNG ab und endet (ohne Datei: mit dem Beispiel);
 *                       Exit 0 = abgelegt
 */
#pragma once

#include <memory>

class Application
{
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// false = die Anwendung kann nicht starten.
    [[nodiscard]] bool init(int argc, char* argv[]);
    /// Die Ereignisschleife; liefert den Exit-Code.
    [[nodiscard]] int run();
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_initialized{false};
};
