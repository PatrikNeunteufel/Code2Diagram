/**
 * @file main.cpp
 * @brief Einstieg — generisch fuer App-Container (CMakeCraft-Vorlage)
 *
 * Die gesamte Logik liegt in Application (src/). Unter Windows liefert
 * Qt6EntryPoint das wWinMain und ruft main(); deshalb steht hier nur main().
 */

#include "Application.hpp"

int main(int argc, char* argv[])
{
    Application app;
    if (!app.init(argc, argv)) return 1;
    const int ergebnis = app.run();
    app.shutdown();
    return ergebnis;
}
