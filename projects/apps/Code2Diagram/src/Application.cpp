/**
 * @file Application.cpp
 * @brief QApplication, Fenster, Argumente, Sichttest-Steuerung
 */
#include "pch.h"
#include "Application.hpp"

#include "AppInfo.hpp"
#include "sichttest/Anbindung.hpp"
#include "ui/Einstellungen.hpp"
#include "ui/Hauptfenster.hpp"
#include "ui/Konfiguration.hpp"
#include "ui/Thema.hpp"

#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

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
                     QStringLiteral("Evaluates #if / #ifdef, reads all branches, shows variants in the flow, "
                                    "or evaluates and frames #if regions"),
                     QStringLiteral("name=<auswerten | alle | varianten | rahmen>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name != QLatin1String("auswerten") && name != QLatin1String("alle") &&
                             name != QLatin1String("varianten") && name != QLatin1String("rahmen"))
                             return st::ungueltig(QStringLiteral("name is auswerten, alle, varianten or rahmen"));
                         fenster.setzeAlleZweige(name == QLatin1String("alle"));
                         fenster.setzeVarianten(name == QLatin1String("varianten"));
                         fenster.setzeBereiche(name == QLatin1String("rahmen"));
                         return st::ok();
                     });

    anbindung.aktion(QStringLiteral("schalter_setzen"),
                     QStringLiteral("Configuration: ticks or unticks a name asked by #if (writes Defines, reads again)"),
                     QStringLiteral("name=<NAME> an=<ja | nein> [wert=<value>]"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         const QString an = st::argument(argumente, QStringLiteral("an"));
                         if (an != QLatin1String("ja") && an != QLatin1String("nein"))
                             return st::ungueltig(QStringLiteral("an is ja or nein"));
                         if (!fenster.konfiguration().setzeSchalter(name, an == QLatin1String("ja"),
                                                                    argumente.value(QStringLiteral("wert")).toString()))
                             return st::fehler(QStringLiteral("No such name, or it is locked: %1")
                                                   .arg(fenster.konfiguration().schalter().join(QStringLiteral(", "))));
                         return st::ok(fenster.konfiguration().schalter().join(QStringLiteral(", ")));
                     });

    anbindung.aktion(QStringLiteral("zweig_waehlen"),
                     QStringLiteral("Configuration: chooses a branch of the #if chain in a line (sets the names)"),
                     QStringLiteral("zeile=<line of the #if> zweig=<0, 1, ...>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const int zeile = st::argument(argumente, QStringLiteral("zeile")).toInt();
                         const int zweig = st::argument(argumente, QStringLiteral("zweig")).toInt();
                         if (!fenster.konfiguration().waehleZweig(zeile, zweig))
                             return st::fehler(QStringLiteral("Cannot choose it: %1")
                                                   .arg(fenster.konfiguration().weichen().join(QStringLiteral("; "))));
                         return st::ok(fenster.konfiguration().weichen().join(QStringLiteral("; ")));
                     });

    anbindung.aktion(QStringLiteral("konfiguration_speichern"),
                     QStringLiteral("Configuration: writes what Defines holds into the project file"), QString(),
                     [&fenster](const QJsonObject&) -> st::Ergebnis {
                         QString fehler;
                         QStringList hinweise;
                         return fenster.speichereKonfiguration(&fehler, &hinweise)
                                    ? st::ok(hinweise.join(QStringLiteral("; ")))
                                    : st::fehler(fehler);
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

    anbindung.aktion(QStringLiteral("aufrufgraph_zeigen"),
                     QStringLiteral("Shows the call graph from a function of the chosen file, or of the file"),
                     QStringLiteral("name=<function | empty = the file>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (!fenster.zeigeAufrufgraph(name))
                             return st::fehler(QStringLiteral("No function '%1' with a body in %2").arg(name, fenster.datei()));
                         return st::ok(fenster.aufrufgraphKnoten().join(QStringLiteral(", ")));
                     });

    anbindung.aktion(QStringLiteral("abhaengigkeiten_zeigen"),
                     QStringLiteral("Shows the data dependencies from an object of the chosen file "
                                    "(table, view, routine, trigger name@table, function with SQL), or of the file"),
                     QStringLiteral("name=<object | empty = the file>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (!fenster.zeigeAbhaengigkeiten(name))
                             return st::fehler(QStringLiteral("No data dependencies '%1' in %2").arg(name, fenster.datei()));
                         return st::ok(fenster.aufrufgraphKnoten().join(QStringLiteral(", ")));
                     });

    anbindung.aktion(QStringLiteral("aufrufgraph_einstellen"),
                     QStringLiteral("Call graph: direction, depth and whether external calls are shown"),
                     QStringLiteral("richtung=<calls | callers | both> tiefe=<0 = all, 1, 2, ...> "
                                    "hoechstens=<0 = all, 300, ...> fremde=<ja | nein>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString richtung = st::argument(argumente, QStringLiteral("richtung"));
                         const QString tiefe = st::argument(argumente, QStringLiteral("tiefe"));
                         const QString fremde = st::argument(argumente, QStringLiteral("fremde"));
                         const QString hoechstens = st::argument(argumente, QStringLiteral("hoechstens"));
                         if (!hoechstens.isEmpty())
                         {
                             bool zahl = false;
                             const int h = hoechstens.toInt(&zahl);
                             if (!zahl || h < 0) return st::ungueltig(QStringLiteral("hoechstens is 0, 1, 2, ..."));
                             fenster.setzeHoechstens(h);
                         }
                         if (!richtung.isEmpty() && !fenster.setzeAufrufRichtung(richtung))
                             return st::ungueltig(QStringLiteral("richtung is calls, callers or both"));
                         if (!tiefe.isEmpty())
                         {
                             bool zahl = false;
                             const int t = tiefe.toInt(&zahl);
                             if (!zahl || t < 0) return st::ungueltig(QStringLiteral("tiefe is 0, 1, 2, ..."));
                             fenster.setzeTiefe(t);
                         }
                         if (!fremde.isEmpty())
                         {
                             if (fremde != QLatin1String("ja") && fremde != QLatin1String("nein"))
                                 return st::ungueltig(QStringLiteral("fremde is ja or nein"));
                             fenster.setzeOhneFremde(fremde == QLatin1String("nein"));
                         }
                         return st::ok(fenster.aufrufgraphKnoten().join(QStringLiteral(", ")));
                     });

    anbindung.aktion(QStringLiteral("aufklappen"),
                     QStringLiteral("Call graph: expands a function at the edge by one level"),
                     QStringLiteral("name=<function, as in the graph>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (!fenster.klappeAuf(name))
                             return st::fehler(QStringLiteral("No function '%1' in the call graph: %2")
                                                   .arg(name, fenster.aufrufgraphKnoten().join(QStringLiteral(", "))));
                         return st::ok(fenster.aufrufgraphKnoten().join(QStringLiteral(", ")));
                     });

    anbindung.aktion(QStringLiteral("typ_waehlen"),
                     QStringLiteral("Data types: chooses the type shown (with its neighbours), or all"),
                     QStringLiteral("name=<type | *>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name.isEmpty()) return st::ungueltig(QStringLiteral("name is missing"));
                         if (!fenster.waehleDiagramm(QStringLiteral("types:") + name))
                             return st::fehler(QStringLiteral("No type '%1' - there are: %2")
                                                   .arg(name, fenster.typen().join(QStringLiteral(", "))));
                         return st::ok(fenster.typ());
                     });

    anbindung.aktion(QStringLiteral("typ_zeigen"),
                     QStringLiteral("Follows a type name like a link: to the type it means, also in another file"),
                     QStringLiteral("name=<type name> wahl=<0, 1, ... if more than one qualify>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name.isEmpty()) return st::ungueltig(QStringLiteral("name is missing"));
                         const QString wahl = st::argument(argumente, QStringLiteral("wahl"));
                         const QStringList ziele = fenster.typZiele(name);
                         if (!fenster.zeigeTyp(name, wahl.isEmpty() ? -1 : wahl.toInt()))
                             return st::fehler(QStringLiteral("'%1' leads to: %2")
                                                   .arg(name, ziele.isEmpty() ? QStringLiteral("nothing")
                                                                              : ziele.join(QStringLiteral(", "))));
                         return st::ok(QStringLiteral("%1 / %2").arg(fenster.datei(), fenster.typ()));
                     });

    anbindung.aktion(QStringLiteral("klicken"),
                     QStringLiteral("Like a click in the preview: shows the place in the source"),
                     QStringLiteral("name=<a line of a flow node | a function of the call graph | Type | Type::member>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name.isEmpty()) return st::ungueltig(QStringLiteral("name is missing"));
                         if (!fenster.klicke(name))
                             return st::fehler(QStringLiteral("Nothing called '%1' with a place in %2").arg(name, fenster.diagramm()));
                         return st::ok(fenster.gezeigteStelle());
                     });

    anbindung.aktion(QStringLiteral("doppelklicken"),
                     QStringLiteral("Like a double-click in the data flow or the run timeline: follows the node or bar"),
                     QStringLiteral("name=<a node of the data flow | a bar of the run timeline (Job 1, Stage 2: …)>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name.isEmpty()) return st::ungueltig(QStringLiteral("name is missing"));
                         if (!fenster.doppelklicke(name))
                             return st::fehler(QStringLiteral("'%1' leads nowhere in %2").arg(name, fenster.diagramm()));
                         return st::ok(fenster.diagramm() + QStringLiteral(" in ") + fenster.datei());
                     });

    anbindung.aktion(QStringLiteral("im_diagramm_zeigen"),
                     QStringLiteral("Like right-click in the source, Show in diagram: the node (or type) of a line"),
                     QStringLiteral("zeile=<line in the file shown in the source>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const int zeile = st::argument(argumente, QStringLiteral("zeile")).toInt();
                         if (zeile <= 0) return st::ungueltig(QStringLiteral("zeile is missing"));
                         if (!fenster.zeigeImDiagramm(zeile))
                             return st::fehler(QStringLiteral("Line %1 belongs to no function and no type").arg(zeile));
                         return st::ok(QStringLiteral("%1 / %2").arg(fenster.diagramm(), fenster.markiert()));
                     });

    anbindung.aktion(QStringLiteral("wahrheitstabelle_zeigen"),
                     QStringLiteral("Like right-click in the source, Truth table of line n"),
                     QStringLiteral("zeile=<line in the file shown in the source>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const int zeile = st::argument(argumente, QStringLiteral("zeile")).toInt();
                         if (zeile <= 0) return st::ungueltig(QStringLiteral("zeile is missing"));
                         if (!fenster.zeigeWahrheitstabelle(zeile))
                             return st::fehler(QStringLiteral("Line %1 has no condition with two or more parts").arg(zeile));
                         return st::ok(fenster.diagramm());
                     });

    anbindung.aktion(QStringLiteral("zuordnung_zeigen"),
                     QStringLiteral("Like right-click in the source, Mapping table of line n"),
                     QStringLiteral("zeile=<line of a switch in the file shown in the source>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const int zeile = st::argument(argumente, QStringLiteral("zeile")).toInt();
                         if (zeile <= 0) return st::ungueltig(QStringLiteral("zeile is missing"));
                         if (!fenster.zeigeZuordnung(zeile))
                             return st::fehler(QStringLiteral("Line %1 has no switch that only maps values").arg(zeile));
                         return st::ok(fenster.diagramm());
                     });

    anbindung.aktion(QStringLiteral("konfiguration_zeigen"),
                     QStringLiteral("Like right-click in the source, #if configuration of line n"),
                     QStringLiteral("zeile=<line of an #if, #elif, #else or #endif in the file shown in the source>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const int zeile = st::argument(argumente, QStringLiteral("zeile")).toInt();
                         if (zeile <= 0) return st::ungueltig(QStringLiteral("zeile is missing"));
                         if (!fenster.zeigeKonfiguration(zeile))
                             return st::fehler(QStringLiteral("Line %1 is not part of an #if chain").arg(zeile));
                         return st::ok(fenster.diagramm());
                     });

    anbindung.aktion(QStringLiteral("schaltstellen_zeigen"),
                     QStringLiteral("Like Where is NAME switched? (context menu of the #if configuration, Configuration, source)"),
                     QStringLiteral("name=<a name asked by #if or set by #define / the project file>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (name.isEmpty()) return st::ungueltig(QStringLiteral("name is missing"));
                         if (!fenster.zeigeSchaltstellen(name))
                             return st::fehler(QStringLiteral("No #if asks for %1, and nothing sets it").arg(name));
                         return st::ok(fenster.diagramm());
                     });

    anbindung.aktion(QStringLiteral("zweig_aktivieren"),
                     QStringLiteral("Like Make this branch active on a row of the #if configuration"),
                     QStringLiteral("zeile=<line of the #if, #elif or #else of the row; #endif for (no #else)>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const int zeile = st::argument(argumente, QStringLiteral("zeile")).toInt();
                         if (zeile <= 0) return st::ungueltig(QStringLiteral("zeile is missing"));
                         if (!fenster.aktiviereZweig(zeile))
                             return st::fehler(QStringLiteral("The branch of line %1 cannot be made active").arg(zeile));
                         return st::ok(fenster.definitionen());
                     });

    anbindung.aktion(QStringLiteral("darstellung_waehlen"),
                     QStringLiteral("How a function is shown: flowchart, Nassi-Shneiderman or decision table"),
                     QStringLiteral("name=<flowchart | nassi-shneiderman | decision-table>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (!fenster.waehleDarstellung(name))
                             return st::ungueltig(
                                 QStringLiteral("No view '%1' - there are: flowchart, nassi-shneiderman, decision-table").arg(name));
                         return st::ok(fenster.darstellung());
                     });

    anbindung.aktion(QStringLiteral("abfrage_zeigen"),
                     QStringLiteral("Shows a query of the file (Diagram > Queries): by name, by line, or the first"),
                     QStringLiteral("name=<query name | line> (empty: the first)"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (!fenster.zeigeAbfrage(name))
                             return st::ungueltig(QStringLiteral("No query '%1' - there are: %2")
                                                      .arg(name, fenster.abfragen().join(QStringLiteral(", "))));
                         return st::ok(fenster.abfrage());
                     });

    anbindung.aktion(QStringLiteral("datenfluss_zeigen"),
                     QStringLiteral("Shows the Spark data flow (Diagram > Data flow): of a function or cell, or of the whole file"),
                     QStringLiteral("name=<function | cell, e.g. <cell 3>> (empty: the whole file)"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (!fenster.zeigeDatenfluss(name))
                             return st::ungueltig(QStringLiteral("No data flow '%1' - there are: %2")
                                                      .arg(name, fenster.datenfluesse().join(QStringLiteral(", "))));
                         return st::ok(fenster.datenflussKnoten().join(QStringLiteral(" | ")));
                     });

    anbindung.aktion(QStringLiteral("plan_zeigen"),
                     QStringLiteral("Shows a Spark plan from explain() (Diagram > Physical plan): by its name, and optionally "
                                    "one of its logical plans"),
                     QStringLiteral("name=<function@line | <cell n>@line | file name> (empty: the first), "
                                    "abschnitt=physical|optimized|analyzed|parsed (empty: physical)"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         const QString abschnitt = st::argument(argumente, QStringLiteral("abschnitt"));
                         if (!fenster.zeigePlan(name, abschnitt))
                             return st::ungueltig(QStringLiteral("No plan '%1' %2 - there are: %3")
                                                      .arg(name, abschnitt, fenster.plaene().join(QStringLiteral(", "))));
                         return st::ok(fenster.planKnoten().join(QStringLiteral(" | ")));
                     });

    anbindung.aktion(QStringLiteral("lauf_zeigen"),
                     QStringLiteral("Shows the run of a Spark event log as a timeline (Diagram > Run timeline)"), QString(),
                     [&fenster](const QJsonObject&) -> st::Ergebnis {
                         if (!fenster.zeigeLauf()) return st::ungueltig(QStringLiteral("The file is not a Spark event log"));
                         return st::ok(QStringLiteral("%1 bars").arg(fenster.laufBalken().size()));
                     });

    anbindung.aktion(QStringLiteral("sequenz_zeigen"),
                     QStringLiteral("Shows the start of the cores as a sequence diagram (Diagram > Sequence; needs \"kerne\" in "
                                    "code2diagram.json)"),
                     QString(),
                     [&fenster](const QJsonObject&) -> st::Ergebnis {
                         if (!fenster.zeigeSequenz()) return st::ungueltig(QStringLiteral("The project has no cores (\"kerne\")"));
                         return st::ok(fenster.diagramm());
                     });

    anbindung.aktion(QStringLiteral("notation_waehlen"),
                     QStringLiteral("How tables are drawn in Data types: Tables, Crow's foot, Chen, (min,max); and which "
                                    "columns are ellipses in Chen and (min,max)"),
                     QStringLiteral("name=<tabellen | kraehenfuss | chen | minmax> [attribute=<alle | schluessel | keine>]"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         const QString attribute = st::argument(argumente, QStringLiteral("attribute"));
                         if (!name.isEmpty() && !fenster.waehleNotation(name))
                             return st::ungueltig(
                                 QStringLiteral("No notation '%1' - there are: tabellen, kraehenfuss, chen, minmax").arg(name));
                         if (!attribute.isEmpty() && !fenster.waehleAttribute(attribute))
                             return st::ungueltig(
                                 QStringLiteral("No choice '%1' - there are: alle, schluessel, keine").arg(attribute));
                         return st::ok(fenster.notation() + QLatin1Char(' ') + fenster.attribute());
                     });

    anbindung.aktion(QStringLiteral("einstellungen_zeigen"), QStringLiteral("Opens File > Settings"), QString(),
                     [&fenster](const QJsonObject&) -> st::Ergebnis {
                         fenster.zeigeEinstellungen();
                         return st::ok();
                     });

    anbindung.aktion(QStringLiteral("thema_setzen"),
                     QStringLiteral("Like File > Settings, Theme, Apply: saves the theme and sets it"),
                     QStringLiteral("name=<system | light | dark>"),
                     [](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString name = st::argument(argumente, QStringLiteral("name"));
                         if (!c2d::ui::einstellungen::setze(QString::fromLatin1(c2d::ui::einstellungen::kThema), name))
                             return st::ungueltig(QStringLiteral("No theme '%1' - there are: system, light, dark").arg(name));
                         c2d::ui::einstellungen::wendeAn();
                         return st::ok(c2d::ui::thema::idVon(c2d::ui::thema::gilt()));
                     });

    anbindung.aktion(QStringLiteral("zuletzt_oeffnen"),
                     QStringLiteral("File > Open recent: opens entry number wahl (0 = the newest)"),
                     QStringLiteral("wahl=<0, 1, ...>"),
                     [&fenster](const QJsonObject& argumente) -> st::Ergebnis {
                         const QString wahl = st::argument(argumente, QStringLiteral("wahl"));
                         const QStringList liste = fenster.zuletztGeoeffnet();
                         QString fehler;
                         if (!fenster.oeffneZuletzt(wahl.isEmpty() ? 0 : wahl.toInt(), &fehler))
                             return st::fehler(QStringLiteral("%1 - the list: %2")
                                                   .arg(fehler, liste.isEmpty() ? QStringLiteral("empty")
                                                                                : liste.join(QStringLiteral(", "))));
                         return st::ok(fenster.zuletztGeoeffnet().value(0));
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
    bool einstellungen = false;  ///< --einstellungen: File > Settings beim Start offen
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
    QCoreApplication::setOrganizationName(ausStd(c2d::appName()));  // fuer QSettings (Open recent)
    QCoreApplication::setApplicationVersion(ausStd(c2d::appVersion()));
    // Die Ressource liegt in der Exe (main/Code2Diagram.qrc); in den Tests fehlt sie, das Icon bleibt leer.
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/code2diagram.svg")));

    const QStringList argumente = QCoreApplication::arguments();
    // das Thema vor dem Fenster: aus File > Settings, oder fuer diesen Lauf --thema (nicht gespeichert)
    c2d::ui::einstellungen::wendeAn();
    if (const qsizetype t = argumente.indexOf(QStringLiteral("--thema")); t >= 0 && t + 1 < argumente.size())
    {
        if (const auto thema = c2d::ui::thema::ausId(argumente[t + 1]))
            c2d::ui::thema::wende(*thema);
        else
            qWarning().noquote() << "No theme" << argumente[t + 1] << "- there are: system, light, dark";
    }

    m_impl->fenster = std::make_unique<c2d::ui::Hauptfenster>();

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
    QString tiefe, hoechstens, aufrufRichtung, notation, attribute, abfrage;
    bool alleZweige = false, varianten = false, rahmen = false, zurueck = false, ohneFremde = false, struktogramm = false,
         entscheidungstabelle = false;
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
        else if (a == QLatin1String("--varianten")) varianten = true;
        else if (a == QLatin1String("--rahmen")) rahmen = true;
        else if (a == QLatin1String("--tiefe")) tiefe = wert();
        else if (a == QLatin1String("--hoechstens")) hoechstens = wert();
        else if (a == QLatin1String("--thema")) wert();  // schon oben gesetzt
        else if (a == QLatin1String("--einstellungen")) m_impl->einstellungen = true;
        else if (a == QLatin1String("--struktogramm")) struktogramm = true;
        else if (a == QLatin1String("--notation")) notation = wert();
        else if (a == QLatin1String("--abfrage")) abfrage = wert();
        else if (a == QLatin1String("--attribute")) attribute = wert();
        else if (a == QLatin1String("--entscheidungstabelle")) entscheidungstabelle = true;
        else if (a == QLatin1String("--aufrufer")) aufrufRichtung = QStringLiteral("callers");
        else if (a == QLatin1String("--beide")) aufrufRichtung = QStringLiteral("both");
        else if (a == QLatin1String("--ohne-fremde")) ohneFremde = true;
        else if (a == QLatin1String("-D")) definitionen << wert();
        else if (a.startsWith(QLatin1String("-D"))) definitionen << a.mid(2);
        else if (!a.startsWith(QLatin1Char('-')) && datei.isEmpty()) datei = a;
    }
    c2d::ui::Hauptfenster& fenster = *m_impl->fenster;
    // vor dem Oeffnen: so wird die Datei gleich mit ihnen gelesen
    fenster.setzeDefinitionen(definitionen.join(QLatin1Char(' ')));
    fenster.setzeAlleZweige(alleZweige);
    fenster.setzeVarianten(varianten);
    fenster.setzeBereiche(rahmen);
    // fuer den Aufrufgraph (--diagramm calls oder calls:<Funktion>)
    if (!tiefe.isEmpty()) fenster.setzeTiefe(tiefe.toInt());
    if (!hoechstens.isEmpty()) fenster.setzeHoechstens(hoechstens.toInt());
    if (!aufrufRichtung.isEmpty()) fenster.setzeAufrufRichtung(aufrufRichtung);
    fenster.setzeOhneFremde(ohneFremde);
    if (struktogramm) fenster.waehleDarstellung(QString::fromLatin1(c2d::ui::Hauptfenster::kStruktogramm));
    if (entscheidungstabelle) fenster.waehleDarstellung(QString::fromLatin1(c2d::ui::Hauptfenster::kEntscheidungstabelle));
    if (!datei.isEmpty())
    {
        QString fehler;
        if (!fenster.oeffne(datei, &fehler)) qWarning().noquote() << fehler;
    }
    else if (!m_impl->bild.isEmpty() || !m_impl->szene.isEmpty())
        fenster.zeigeBeispiel();
    if (!imProjekt.isEmpty() && !fenster.waehleDatei(imProjekt))
        qWarning().noquote() << "No file" << imProjekt << "in the project";
    // fuer ein Bild der Datentypen; nicht gemerkt (wie --thema)
    if ((!notation.isEmpty() || !attribute.isEmpty()) && !fenster.setzeNotation(notation, attribute))
        qWarning().noquote() << "No notation" << notation << attribute;
    // eine Abfrage der Datei, nach Name oder Zeile (fuer ein Bild)
    if (!abfrage.isEmpty() && !fenster.zeigeAbfrage(abfrage)) qWarning().noquote() << "No query" << abfrage;
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
    if (m_impl->einstellungen) fenster.zeigeEinstellungen();
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
        if (!m_impl->bild.isEmpty())
        {
            // ein offener Dialog Settings liegt mittig ueber dem Fenster, wie man ihn saehe
            QPixmap bild = m_impl->fenster->grab();
            if (m_impl->einstellungen)
            {
                const QPixmap dialog = m_impl->fenster->zeigeEinstellungen()->grab();
                QPainter maler(&bild);
                maler.drawPixmap((bild.width() - dialog.width()) / 2, (bild.height() - dialog.height()) / 2, dialog);
            }
            abgelegt = bild.save(m_impl->bild) && abgelegt;
        }
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
