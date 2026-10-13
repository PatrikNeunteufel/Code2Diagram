/**
 * @file Hauptfenster.hpp
 * @brief Das Fenster: links der Quelltext, rechts das gewaehlte Diagramm — gezeichnet und als Text
 *
 * Das Fenster ruft nur den Kern (Bibliothek C2dKern): eine Sprache liest den
 * Text ins Codemodell, die Ableitung baut das Diagramm, die Anordnung stellt
 * es auf, ein Format schreibt es. Welche Sprachen und Formate es gibt, fragt
 * es dort ab — eine neue Sprache oder ein neues Format braucht hier keine
 * Aenderung.
 *
 * Im gezeichneten Flussdiagramm fuehrt ein Knoten, der eine Funktion der Datei
 * ruft, weiter: Doppelklick zeigt ihr Diagramm (Back fuehrt zurueck), das
 * Kontextmenue bettet ihren Ablauf umrahmt ein oder nimmt ihn wieder heraus.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <QMainWindow>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;

namespace c2d::ui
{

class Vorschau;

class Hauptfenster : public QMainWindow
{
public:
    /// Der Name, unter dem das Diagramm der Datentypen gewaehlt wird.
    static constexpr const char* kDatentypen = "types";

    explicit Hauptfenster(QWidget* parent = nullptr);

    /// Eine Quelldatei laden, lesen und zeigen. false = nicht lesbar; `fehler`
    /// nennt dann einen Satz dazu, und das Fenster bleibt, wie es war.
    bool oeffne(const QString& pfad, QString* fehler = nullptr);

    /// Einen Quelltext lesen und zeigen; `name` entscheidet ueber die Sprache
    /// (nach der Endung, bei `.h` nach dem Inhalt) und steht im Titel.
    void zeige(const QString& quelltext, const QString& name);

    /// Das eingebaute Beispiel zeigen (beispiele/ampel.c).
    void zeigeBeispiel();

    /// Was sich zeigen laesst: die Funktionen mit Rumpf (mit Bereich:
    /// `Klasse::methode`), dann `types`, wenn es Datentypen gibt.
    [[nodiscard]] QStringList diagramme() const;
    /// Das Diagramm wechseln; das bisherige kommt in den Verlauf fuer zurueck(). false = so heisst keines.
    bool waehleDiagramm(const QString& name);
    [[nodiscard]] QString diagramm() const;
    /// Zum Diagramm davor. false = es gibt keines.
    bool zurueck();

    /// `mermaid`, `dot`, `drawio`, ...; false = so heisst kein Format.
    bool waehleFormat(const QString& name);
    /// `preview` oder `text`; false = so heisst keine Ansicht.
    bool waehleAnsicht(const QString& name);

    /// Den Ablauf der Funktion `funktion` (Name ohne Bereich) hinter dem ersten
    /// Knoten einbetten, der sie ruft und noch keine Einbettung traegt.
    /// false = kein solcher Knoten, oder die Funktion hat keinen Rumpf in dieser Datei.
    bool betteEin(const QString& funktion);
    /// Alle Einbettungen herausnehmen. false = es gab keine.
    bool loeseEinbettungen();
    [[nodiscard]] int einbettungen() const;

    /// Was beim Auswerten von #if/#ifdef als definiert gilt: `NAME` oder
    /// `NAME=wert`, durch Leerzeichen, Komma oder Strichpunkt getrennt. Die
    /// Datei wird neu gelesen; das gewaehlte Diagramm bleibt, wenn es das noch gibt.
    void setzeDefinitionen(const QString& definitionen);
    /// Bedingte Uebersetzung nicht auswerten, sondern alle Zweige lesen.
    void setzeAlleZweige(bool alle);
    /// Namen, nach denen ein #if gefragt hat und die nirgends definiert sind.
    [[nodiscard]] QStringList unbekannteNamen() const;

    [[nodiscard]] QString quelltext() const;
    [[nodiscard]] QString diagrammText() const;
    /// Wie viele Knoten oder Kaesten die Vorschau zeichnet.
    [[nodiscard]] int vorschauFormen() const;
    /// Wie viele Meldungen das Lesen hinterlassen hat.
    [[nodiscard]] int meldungen() const;

    /// Die Filter des Dialogs Open, aus den Endungen der Sprachen des Kerns.
    [[nodiscard]] static QString dateiFilter();

private:
    /// Das gewaehlte Diagramm neu aus dem Codemodell bauen (ohne Einbettungen) und zeigen.
    void baueNeu();
    /// Das Diagramm, wie es jetzt ist, zeichnen und als Text schreiben.
    void zeichne();
    /// Den gezeigten Quelltext mit den jetzigen Lese-Optionen neu lesen.
    void liesNeu();
    void speichereUnter();
    void zeigeZeile(int zeile);
    void kontextMenue(c2d::diagramm::KnotenId knoten, std::size_t gruppe, const QPoint& ort);
    /// Die Funktion dieses Namens (ohne Bereich) mit Rumpf; nullptr = keine.
    [[nodiscard]] const c2d::modell::Funktion* funktion(const std::string& name) const;
    /// Die erste Funktion der Datei, die der Knoten ruft; nullptr = keine.
    [[nodiscard]] const c2d::modell::Funktion* ziel(c2d::diagramm::KnotenId knoten) const;
    bool betteEin(c2d::diagramm::KnotenId knoten, const c2d::modell::Funktion& gerufen);

    QPlainTextEdit* m_quelle = nullptr;
    QPlainTextEdit* m_text = nullptr;
    Vorschau* m_vorschau = nullptr;
    QTabWidget* m_ansichten = nullptr;
    QComboBox* m_auswahl = nullptr;
    QComboBox* m_format = nullptr;
    QComboBox* m_richtung = nullptr;
    QLineEdit* m_definitionen = nullptr;
    QCheckBox* m_alleZweige = nullptr;

    QString m_name;
    QString m_gelesenMit;  ///< der Text des Felds Defines beim letzten Lesen
    c2d::sprache::Ergebnis m_ergebnis;
    c2d::diagramm::Flussdiagramm m_fluss;  ///< das gezeigte Flussdiagramm, samt Einbettungen
    QString m_gezeigt;                      ///< wofuer m_fluss gebaut ist
    QStringList m_verlauf;                  ///< die Diagramme davor, fuer zurueck()
    bool m_ohneVerlauf = false;             ///< der Wechsel gerade ist ein zurueck()
};

}  // namespace c2d::ui
