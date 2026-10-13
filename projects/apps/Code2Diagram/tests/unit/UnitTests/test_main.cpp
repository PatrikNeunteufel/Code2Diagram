/**
 * @file test_main.cpp
 * @brief Einstieg des Testlaufs (doctest) — mit einer QApplication, weil die
 *        Faelle Widgets anlegen. Gezeigt wird kein Fenster.
 */

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <QApplication>

int main(int argc, char** argv)
{
    QApplication anwendung(argc, argv);
    doctest::Context kontext(argc, argv);
    return kontext.run();
}
