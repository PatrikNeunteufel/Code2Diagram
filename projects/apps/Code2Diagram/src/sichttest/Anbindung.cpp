/**
 * @file Anbindung.cpp
 * @brief Sichttest-Steuerung: Laden der DLL, Anmelden der Aktionen, Pfade
 *
 * Die einzige Datei der Anwendung, die die Koepfe des Pakets einbindet. Ohne
 * das Paket (kein `SICHTTEST_VORHANDEN`) uebersetzt sie ohne Steuerung:
 * `lade()` liefert dann leer und sagt, warum.
 */
#include "pch.h"
#include "sichttest/Anbindung.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#ifdef SICHTTEST_VORHANDEN
#  include <sichttest_steuerung_qt.hpp>
#endif

namespace c2d::sichttest
{

namespace
{
const QLatin1String kSchalter("--testing");
const QLatin1String kProjektDatei(".sichttest/sichttest.projekt.json");
}  // namespace

bool verlangt(const QStringList& argumente)
{
    return argumente.mid(1).contains(kSchalter);
}

QString wurzel(const QString& ab)
{
    QDir ordner(ab);
    if (ab.isEmpty() || !ordner.exists()) return {};
    for (;;)
    {
        if (QFileInfo(ordner.filePath(kProjektDatei)).isFile()) return ordner.absolutePath();
        if (!ordner.cdUp()) return {};
    }
}

QString loeseAuf(const QString& angabe, const QString& root)
{
    if (angabe.trimmed().isEmpty()) return {};
    const QString pfad = QDir::fromNativeSeparators(angabe.trimmed());
    QFileInfo datei(pfad);
    if (datei.isRelative())
    {
        if (root.isEmpty()) return {};
        datei = QFileInfo(QDir(root).filePath(pfad));
    }
    return datei.isFile() ? datei.absoluteFilePath() : QString();
}

QString argument(const QJsonObject& argumente, const QString& schluessel)
{
    const QString wert = argumente.value(schluessel).toString().trimmed();
    return !wert.isEmpty() ? wert : argumente.value(QStringLiteral("wert")).toString().trimmed();
}

struct Anbindung::Innen
{
    QString root;
#ifdef SICHTTEST_VORHANDEN
    std::unique_ptr<::sichttest::Steuerung> steuerung;
#endif
};

Anbindung::Anbindung() : m(std::make_unique<Innen>()) {}
Anbindung::~Anbindung() = default;

std::unique_ptr<Anbindung> Anbindung::lade(const QString& anwendung, const QString& version,
                                           QString* grund)
{
#ifdef SICHTTEST_VORHANDEN
    const QString exeOrdner = QCoreApplication::applicationDirPath();
    // Mit vollem Pfad aus dem Ordner der Exe, nie ueber den Suchpfad; die
    // grosse Fassung der Schnittstelle steht im Namen der DLL.
    const QString dll = QDir::toNativeSeparators(
        QDir(exeOrdner).filePath(QStringLiteral("SichttestSteuerung%1.dll").arg(STS_S_MAJOR)));
    QString warum;
    std::unique_ptr<::sichttest::Steuerung> steuerung =
        ::sichttest::Steuerung::lade(anwendung, version, &warum, dll);
    if (!steuerung)
    {
        if (grund != nullptr) *grund = warum;
        return nullptr;
    }
    std::unique_ptr<Anbindung> neu(new Anbindung);
    neu->m->steuerung = std::move(steuerung);
    qInfo().noquote() << "Sichttest: control DLL"
                      << QString::fromStdString(neu->m->steuerung->fassung().produkt) << "loaded from"
                      << dll;
    neu->m->root = wurzel(exeOrdner);
    if (neu->m->root.isEmpty())
    {
        // ohne Projektdatei: relative Dateien gelten ab dem Ordner der Exe
        neu->m->root = exeOrdner;
        qWarning().noquote() << "Sichttest: no .sichttest/sichttest.projekt.json above" << exeOrdner;
    }
    return neu;
#else
    (void)anwendung;
    (void)version;
    if (grund != nullptr) *grund = QStringLiteral("Built without the Sichttest package.");
    return nullptr;
#endif
}

bool Anbindung::aktion(const QString& name, const QString& beschreibung, const QString& parameter,
                       Aktion aktion)
{
#ifdef SICHTTEST_VORHANDEN
    const bool angemeldet = m->steuerung->aktion(
        name, beschreibung, parameter,
        [name, aktion = std::move(aktion)](const QJsonObject& argumente) -> ::sichttest::Ergebnis {
            const Ergebnis e = aktion(argumente);
            qInfo().noquote() << "Sichttest: action" << name
                              << (e.status == Ergebnis::Status::Ok ? "done" : "FAILED") << e.text;
            switch (e.status)
            {
                case Ergebnis::Status::Ok:
                    return ::sichttest::ok(e.text);
                case Ergebnis::Status::Fehler:
                    return ::sichttest::fehler(e.text);
                case Ergebnis::Status::Ungueltig:
                    break;
            }
            return ::sichttest::ungueltig(e.text);
        });
    if (!angemeldet)
        qWarning().noquote() << "Sichttest: action" << name << "not registered:" << letzterFehler();
    return angemeldet;
#else
    (void)name;
    (void)beschreibung;
    (void)parameter;
    (void)aktion;
    return false;
#endif
}

bool Anbindung::verbinde()
{
#ifdef SICHTTEST_VORHANDEN
    const bool gut = m->steuerung->verbinde();
    if (!gut) qWarning().noquote() << "Sichttest: cannot connect:" << letzterFehler();
    return gut;
#else
    return false;
#endif
}

QString Anbindung::letzterFehler() const
{
#ifdef SICHTTEST_VORHANDEN
    return m->steuerung->letzterFehler();
#else
    return {};
#endif
}

const QString& Anbindung::root() const
{
    return m->root;
}

}  // namespace c2d::sichttest
