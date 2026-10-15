/**
 * @file Application.cpp
 * @brief QApplication, Fenster, Argumente, Sichttest-Steuerung
 */
#include "pch.h"
#include "Application.hpp"

#include "AppInfo.hpp"
#include "sichttest/Anbindung.hpp"
#include "ui/Hauptfenster.hpp"

#include <QApplication>
#include <QDebug>

namespace
{

QString ausStd(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

/// Die Aktionen, die der Tester ausloesen kann. Jede neue Ansicht meldet hier
/// an, was eine Sichttest-Liste von ihr braucht.
void meldeAktionen(c2d::sichttest::Anbindung& anbindung, c2d::ui::Hauptfenster& fenster)
{
    namespace st = c2d::sichttest;

    anbindung.aktion(QStringLiteral("datei_oeffnen"), QStringLiteral("Opens a source file"),
                     QStringLiteral("pfad=<file, relative to the project root>"),
                     [&anbindung, &fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString angabe = st::argument(argumente, QStringLiteral("pfad"));
                         if (angabe.isEmpty()) return st::ungueltig(QStringLiteral("pfad is missing"));
                         const QString pfad = st::loeseAuf(angabe, anbindung.root());
                         if (pfad.isEmpty())
                             return st::fehler(QStringLiteral("File not found: %1").arg(angabe));
                         QString fehler;
                         return fenster.oeffne(pfad, &fehler) ? st::ok(pfad) : st::fehler(fehler);
                     });

    anbindung.aktion(QStringLiteral("ordner_oeffnen"), QStringLiteral("Opens a folder as project"),
                     QStringLiteral("pfad=<folder, relative to the project root>"),
                     [&anbindung, &fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString angabe = st::argument(argumente, QStringLiteral("pfad"));
                         if (angabe.isEmpty()) return st::ungueltig(QStringLiteral("pfad is missing"));
                         const QString pfad = st::loeseAuf(angabe, anbindung.root());
                         if (pfad.isEmpty())
                             return st::fehler(QStringLiteral("Folder not found: %1").arg(angabe));
                         QString fehler;
                         return fenster.oeffneOrdner(pfad, &fehler)
                                    ? st::ok(QStringLiteral("%1 file(s)").arg(fenster.dateien().size()))
                                    : st::fehler(fehler);
                     });

    anbindung.aktion(QStringLiteral("datei_waehlen"), QStringLiteral("Selects a file of the open project"),
                     QStringLiteral("pfad=<file, relative to the project folder>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString pfad = st::argument(argumente, QStringLiteral("pfad"));
                         if (pfad.isEmpty()) return st::ungueltig(QStringLiteral("pfad is missing"));
                         return fenster.waehleDatei(pfad)
                                    ? st::ok()
                                    : st::fehler(QStringLiteral("No file '%1' - there are: %2")
                                                     .arg(pfad, fenster.dateien().join(QStringLiteral(", "))));
                     });

    anbindung.aktion(QStringLiteral("springen"),
                     QStringLiteral("Goes to the function a call means, also in another file"),
                     QStringLiteral("name=<called name> wahl=<0, 1, ... if more than one qualify>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name.isEmpty()) return st::ungueltig(QStringLiteral("name is missing"));
                         const QString wahl = st::argument(argumente, QStringLiteral("wahl"));
                         const QStringList ziele = fenster.ziele(name);
                         if (!fenster.springe(name, wahl.isEmpty() ? -1 : wahl.toInt()))
                             return st::fehler(QStringLiteral("'%1' leads to: %2")
                                                   .arg(name, ziele.isEmpty() ? QStringLiteral("nothing")
                                                                              : ziele.join(QStringLiteral(", "))));
                         return st::ok(QStringLiteral("%1 / %2").arg(fenster.datei(), fenster.diagramm()));
                     });

    anbindung.aktion(QStringLiteral("zum_aufrufer"),
                     QStringLiteral("Goes to a function that calls the one shown; the call there is highlighted"),
                     QStringLiteral("wahl=<0, 1, ... as in Called from>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString wahl = st::argument(argumente, QStringLiteral("wahl"));
                         const QStringList wer = fenster.aufrufer();
                         if (!fenster.springeZuAufrufer(wahl.isEmpty() ? 0 : wahl.toInt()))
                             return st::fehler(QStringLiteral("Callers: %1")
                                                   .arg(wer.isEmpty() ? QStringLiteral("none") : wer.join(QStringLiteral(", "))));
                         return st::ok(QStringLiteral("%1 / %2").arg(fenster.datei(), fenster.diagramm()));
                     });

    anbindung.aktion(QStringLiteral("beispiel_zeigen"),
                     QStringLiteral("Shows the built-in sample with its diagram"), QString(),
                     [&fenster](const QJsonObject&) -> st::Ergebnis {
                         fenster.zeigeBeispiel();
                         return st::ok();
                     });

    anbindung.aktion(QStringLiteral("diagramm_waehlen"),
                     QStringLiteral("Selects what is drawn: a function, or the data types"),
                     QStringLiteral("name=<function name | types>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name.isEmpty()) return st::ungueltig(QStringLiteral("name is missing"));
                         return fenster.waehleDiagramm(name)
                                    ? st::ok()
                                    : st::fehler(QStringLiteral("No diagram '%1' - there are: %2")
                                                     .arg(name, fenster.diagramme().join(QStringLiteral(", "))));
                     });

    anbindung.aktion(QStringLiteral("definitionen_setzen"),
                     QStringLiteral("Sets what counts as defined for #if / #ifdef and reads the file again"),
                     QStringLiteral("wert=<NAME NAME=value ...; empty = none>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         fenster.setzeDefinitionen(argumente.value(QStringLiteral("wert")).toString());
                         return st::ok(fenster.unbekannteNamen().join(QStringLiteral(", ")));
                     });

    anbindung.aktion(QStringLiteral("zweige_waehlen"),
                     QStringLiteral("Evaluates #if / #ifdef, or reads all branches"),
                     QStringLiteral("name=<auswerten | alle>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name != QLatin1String("auswerten") && name != QLatin1String("alle"))
                             return st::ungueltig(QStringLiteral("name is auswerten or alle"));
                         fenster.setzeAlleZweige(name == QLatin1String("alle"));
                         return st::ok();
                     });

    anbindung.aktion(QStringLiteral("ansicht_waehlen"), QStringLiteral("Shows the drawn preview or the text"),
                     QStringLiteral("name=<preview | text>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         return fenster.waehleAnsicht(name) ? st::ok()
                                                            : st::ungueltig(QStringLiteral("name is preview or text"));
                     });

    anbindung.aktion(QStringLiteral("einbetten"),
                     QStringLiteral("Embeds the flow of a called function behind its first call"),
                     QStringLiteral("name=<function name>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name.isEmpty()) return st::ungueltig(QStringLiteral("name is missing"));
                         return fenster.betteEin(name)
                                    ? st::ok()
                                    : st::fehler(QStringLiteral("Nothing in %1 calls '%2', or it has no body here")
                                                     .arg(fenster.diagramm(), name));
                     });

    anbindung.aktion(QStringLiteral("einbettungen_loesen"), QStringLiteral("Removes all embeddings"), QString(),
                     [&fenster](const QJsonObject&) -> st::Ergebnis {
                         fenster.loeseEinbettungen();
                         return st::ok();
                     });

    anbindung.aktion(QStringLiteral("zurueck"), QStringLiteral("Goes back to the diagram shown before"), QString(),
                     [&fenster](const QJsonObject&) -> st::Ergebnis {
                         return fenster.zurueck() ? st::ok() : st::fehler(QStringLiteral("No diagram to go back to"));
                     });

    anbindung.aktion(QStringLiteral("format_waehlen"), QStringLiteral("Selects the output format"),
                     QStringLiteral("name=<svg | mermaid | dot | drawio>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name.isEmpty()) return st::ungueltig(QStringLiteral("name is missing"));
                         return fenster.waehleFormat(name)
                                    ? st::ok()
                                    : st::fehler(QStringLiteral("No format '%1'").arg(name));
                     });
}

}  // namespace

struct Application::Impl
{
    // Reihenfolge zaehlt: die Steuerung endet zuerst, dann das Fenster, dann Qt.
    std::unique_ptr<QApplication> qt;
    std::unique_ptr<c2d::ui::Hauptfenster> fenster;
    std::unique_ptr<c2d::sichttest::Anbindung> steuerung;
    QString bild;     ///< --bild: wohin das Fenster als PNG kommt; leer = gewoehnlicher Lauf
    QString szene;    ///< --szene: wohin das ganze Diagramm der Vorschau als PNG kommt
    QString format;
    QString ansicht;
};

Application::Application() : m_impl{std::make_unique<Impl>()} {}

Application::~Application()
{
    shutdown();
}

bool Application::init(int argc, char* argv[])
{
    if (m_initialized) return false;

    // QApplication behaelt Verweise auf argc und argv: beide leben in main().
    static int s_argc = 0;
    s_argc = argc;
    m_impl->qt = std::make_unique<QApplication>(s_argc, argv);
    QCoreApplication::setApplicationName(ausStd(c2d::appName()));
    QCoreApplication::setApplicationVersion(ausStd(c2d::appVersion()));

    m_impl->fenster = std::make_unique<c2d::ui::Hauptfenster>();
    const QStringList argumente = QCoreApplication::arguments();

    if (c2d::sichttest::verlangt(argumente))
    {
        QString grund;
        m_impl->steuerung = c2d::sichttest::Anbindung::lade(QCoreApplication::applicationName(),
                                                           QCoreApplication::applicationVersion(),
                                                           &grund);
        if (m_impl->steuerung)
        {
            meldeAktionen(*m_impl->steuerung, *m_impl->fenster);
            m_impl->steuerung->verbinde();
        }
        else
        {
            qWarning().noquote() << "Sichttest: --testing given, but no control:" << grund;
        }
    }

    // Schalter mit Wert; das erste Argument, das zu keinem gehoert, ist die Datei.
    QString datei, diagramm, imProjekt;
    QStringList einbetten, definitionen, springen;
    bool alleZweige = false, zurueck = false;
    for (qsizetype i = 1; i < argumente.size(); ++i)
    {
        const QString& a = argumente[i];
        const auto wert = [&] { return i + 1 < argumente.size() ? argumente[++i] : QString(); };
        if (a == QLatin1String("--bild")) m_impl->bild = wert();
        else if (a == QLatin1String("--szene")) m_impl->szene = wert();
        else if (a == QLatin1String("--diagramm")) diagramm = wert();
        else if (a == QLatin1String("--datei")) imProjekt = wert();
        else if (a == QLatin1String("--einbetten")) einbetten << wert();
        else if (a == QLatin1String("--springe")) springen << wert();
        else if (a == QLatin1String("--zurueck")) zurueck = true;
        else if (a == QLatin1String("--format")) m_impl->format = wert();
        else if (a == QLatin1String("--ansicht")) m_impl->ansicht = wert();
        else if (a == QLatin1String("--alle-zweige")) alleZweige = true;
        else if (a == QLatin1String("-D")) definitionen << wert();
        else if (a.startsWith(QLatin1String("-D"))) definitionen << a.mid(2);
        else if (!a.startsWith(QLatin1Char('-')) && datei.isEmpty()) datei = a;
    }
    c2d::ui::Hauptfenster& fenster = *m_impl->fenster;
    // vor dem Oeffnen: so wird die Datei gleich mit ihnen gelesen
    fenster.setzeDefinitionen(definitionen.join(QLatin1Char(' ')));
    fenster.setzeAlleZweige(alleZweige);
    if (!datei.isEmpty())
    {
        QString fehler;
        if (!fenster.oeffne(datei, &fehler)) qWarning().noquote() << fehler;
    }
    else if (!m_impl->bild.isEmpty() || !m_impl->szene.isEmpty())
        fenster.zeigeBeispiel();
    if (!imProjekt.isEmpty() && !fenster.waehleDatei(imProjekt))
        qWarning().noquote() << "No file" << imProjekt << "in the project";
    if (!diagramm.isEmpty() && !fenster.waehleDiagramm(diagramm))
        qWarning().noquote() << "No diagram" << diagramm << "- there are:" << fenster.diagramme().join(QStringLiteral(", "));
    for (const QString& name : einbetten)
        if (!fenster.betteEin(name)) qWarning().noquote() << "Cannot embed" << name;
    // wie die Sichttest-Aktionen springen und zurueck — fuer ein Bild nach einem Sprung
    for (const QString& name : springen)
        if (!fenster.springe(name)) qWarning().noquote() << "Cannot go to" << name << "-" << fenster.ziele(name);
    if (zurueck && !fenster.zurueck()) qWarning().noquote() << "Nothing to go back to";
    if (!m_impl->format.isEmpty()) fenster.waehleFormat(m_impl->format);
    if (!m_impl->ansicht.isEmpty()) fenster.waehleAnsicht(m_impl->ansicht);

    fenster.show();
    m_initialized = true;
    return true;
}

int Application::run()
{
    if (!m_initialized) return 1;
    if (!m_impl->bild.isEmpty() || !m_impl->szene.isEmpty())
    {
        // --bild / --szene: das Fenster oder das ganze Diagramm ablegen und enden (Exit 0 = abgelegt)
        QCoreApplication::processEvents();
        bool abgelegt = true;
        if (!m_impl->bild.isEmpty()) abgelegt = m_impl->fenster->grab().save(m_impl->bild) && abgelegt;
        if (!m_impl->szene.isEmpty()) abgelegt = m_impl->fenster->vorschauBild().save(m_impl->szene) && abgelegt;
        return abgelegt ? 0 : 2;
    }
    return QApplication::exec();
}

void Application::shutdown()
{
    if (!m_initialized) return;
    m_impl->steuerung.reset();
    m_impl->fenster.reset();
    m_impl->qt.reset();
    m_initialized = false;
}
