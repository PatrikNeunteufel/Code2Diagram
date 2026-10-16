/**
 * @file test_main.cpp
 * @brief Einstieg des Testlaufs (doctest) — mit einer QApplication, weil die
 *        Faelle Widgets anlegen. Gezeigt wird kein Fenster.
 */

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

int main(int argc, char** argv)
{
    QApplication anwendung(argc, argv);
    // Einstellungen (File > Open recent) in eine Datei, die mit dem Lauf verschwindet — nie in die
    // des Benutzers
    QTemporaryDir einstellungen;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, einstellungen.path());
    QCoreApplication::setOrganizationName(QStringLiteral("Code2Diagram.UnitTests"));
    QCoreApplication::setApplicationName(QStringLiteral("Code2Diagram.UnitTests"));
    doctest::Context kontext(argc, argv);
    return kontext.run();
}
