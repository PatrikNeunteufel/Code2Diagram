/**
 * @file Hauptfenster.hpp
 * @brief Das Fenster: links Dateien und Quelltext, rechts das gewaehlte Diagramm — gezeichnet und als Text
 *
 * Das Fenster ruft nur den Kern (Bibliothek C2dKern): ein Projekt liest die
 * Dateien ins Codemodell (projekt/Projekt.hpp), die Ableitung baut das
 * Diagramm, die Anordnung stellt es auf, ein Format schreibt es. Welche
 * Sprachen und Formate es gibt, fragt es dort ab — eine neue Sprache oder ein
 * neues Format braucht hier keine Aenderung.
 *
 * Gezeigt wird eine Datei oder ein Ordner (ein Projekt). Bei einem Projekt
 * steht ueber dem Quelltext der Baum der Dateien; zur Wahl stehen die
 * Diagramme der gewaehlten Datei.
 *
 * In jeder Ansicht zeigt ein Klick auf einen Knoten, Kasten oder ein Glied
 * seine Stelle im Quelltext; der Doppelklick fuehrt nur zu dem, was genannt ist.
 *
 * Im gezeichneten Flussdiagramm fuehrt ein Knoten, der eine Funktion ruft,
 * weiter — auch in eine andere Datei des Projekts: Doppelklick zeigt ihr
 * Diagramm (Back fuehrt zurueck), das Kontextmenue bettet ihren Ablauf
 * umrahmt ein oder nimmt ihn wieder heraus. Meint ein Aufruf mehr als eine
 * Funktion (projekt::kandidaten), waehlt man im Menue, welche.
 *
 * Der Aufrufgraph (projekt/Aufrufgraph.hpp) steht als `calls` (die Datei) und
 * `calls:<Funktion>` (ab ihr) zur Wahl; Richtung, Tiefe und ob Fremde zu sehen
 * sind, stellt die Leiste ein. Ein Doppelklick auf eine Funktion zeigt ihren
 * Ablauf, Back fuehrt zurueck; das Kontextmenue klappt eine Funktion an der
 * Grenze eine Stufe auf.
 *
 * Bei den Datentypen waehlt ein zweites Feld den Typ, der gezeigt wird (mit
 * seinen Nachbarn, projekt/Typen.hpp), oder alle. Ein Typ, den ein Glied oder
 * ein Knoten des Ablaufs nennt, ist ein hervorgehobener Link: Doppelklick oder
 * Kontextmenue fuehrt zu ihm, auch in eine andere Datei.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Konfigurationstabelle.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"
#include "c2d/diagramm/Entscheidungstabelle.hpp"
#include "c2d/diagramm/Struktogramm.hpp"
#include "c2d/diagramm/Wahrheitstabelle.hpp"
#include "c2d/diagramm/Zuordnung.hpp"
#include "c2d/diagramm/Zustandsdiagramm.hpp"
#include "c2d/projekt/Aufrufgraph.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/projekt/Typen.hpp"

#include <QImage>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>
#include <vector>

class QAction;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace c2d::ui
{

class EinstellungenDialog;
class Konfiguration;
class Vorschau;

class Hauptfenster : public QMainWindow
{
public:
    /// Der Name, unter dem das Diagramm der Datentypen gewaehlt wird.
    static constexpr const char* kDatentypen = "types";
    /// So beginnt der Name eines Zustandsautomaten: `automat:<Variable>`
    /// (bei zweien mit derselben Variable `automat:<Variable>@<Funktion>`).
    static constexpr const char* kAutomat = "automat:";
    /// Der Aufrufgraph der gewaehlten Datei.
    static constexpr const char* kAufrufgraphDatei = "calls";
    /// So beginnt der Name des Aufrufgraphen ab einer Funktion: `calls:<Funktion>` (mit Bereich).
    static constexpr const char* kAufrufgraph = "calls:";
    /// Wie eine Funktion gezeigt wird (das Feld neben Diagram): ihr Ablauf oder ihr Struktogramm.
    static constexpr const char* kFlussdiagramm = "flowchart";
    static constexpr const char* kStruktogramm = "nassi-shneiderman";
    static constexpr const char* kEntscheidungstabelle = "decision-table";
    /// So beginnt der Name einer Wahrheitstabelle: `truth:<Funktion>@<Zeile>`.
    static constexpr const char* kWahrheitstabelle = "truth:";
    /// So beginnt der Name einer Zuordnungstabelle (ein switch): `mapping:<Funktion>@<Zeile>`.
    static constexpr const char* kZuordnung = "mapping:";
    /// Die konstanten Tabellen der Datei.
    static constexpr const char* kNachschlagetabellen = "lookup";
    /// Je Kette #if der Datei, welcher Zweig bei welchen Definitionen gilt.
    static constexpr const char* kKonfiguration = "ifconfig";

    explicit Hauptfenster(QWidget* parent = nullptr);

    /// Eine Quelldatei laden, lesen und zeigen; ein Ordner wird als Projekt
    /// geoeffnet (oeffneOrdner). false = nicht lesbar; `fehler` nennt dann einen
    /// Satz dazu, und das Fenster bleibt, wie es war.
    bool oeffne(const QString& pfad, QString* fehler = nullptr);

    /// Einen Ordner als Projekt oeffnen: alle Quelldateien darunter, mit
    /// `code2diagram.json`, wenn es sie gibt. Gewaehlt ist danach die erste Datei
    /// mit einer Funktion. false = kein Ordner.
    bool oeffneOrdner(const QString& pfad, QString* fehler = nullptr);

    /// Einen Quelltext lesen und zeigen; `name` entscheidet ueber die Sprache
    /// (nach der Endung, bei `.h` nach dem Inhalt) und steht im Titel.
    void zeige(const QString& quelltext, const QString& name);

    /// Das eingebaute Beispiel zeigen (beispiele/ampel.c).
    void zeigeBeispiel();

    /// Wie viele Dateien und Ordner File > Open recent hoechstens nennt.
    static constexpr int kZuletztHoechstens = 10;
    /// Was zuletzt geoeffnet wurde, Dateien und Ordner (absolute Pfade), das Neueste zuerst.
    /// Bleibt ueber Neustarts (QSettings, Schluessel `zuletzt`).
    [[nodiscard]] QStringList zuletztGeoeffnet() const;
    /// Den Eintrag Nummer `wahl` (ab 0) wieder oeffnen. Gibt es ihn nicht mehr, kommt er aus der
    /// Liste, und `fehler` sagt es. false = keiner, oder nicht zu oeffnen.
    bool oeffneZuletzt(int wahl, QString* fehler = nullptr);
    /// Die Liste leeren.
    void leereZuletzt();

    /// Ist ein Ordner offen?
    [[nodiscard]] bool istProjekt() const;
    /// Die Dateien des Projekts, relativ zum Ordner, mit `/`; bei einer Datei nur sie.
    [[nodiscard]] QStringList dateien() const;
    /// Die Datei, deren Diagramme zur Wahl stehen.
    [[nodiscard]] QString datei() const;
    /// Eine andere Datei des Projekts waehlen. false = so heisst keine.
    bool waehleDatei(const QString& pfad);
    /// Ordner, die beim Einlesen uebergangen wurden.
    [[nodiscard]] QStringList uebergangeneOrdner() const;
    /// Meldungen ueber das Projekt (Projektdatei, uebergangene Ordner, Koepfe, die sich nicht finden).
    [[nodiscard]] QStringList projektMeldungen() const;

    /// Der Text der Projektdatei, wie er auf der Platte steht; gibt es keine:
    /// ein Vorschlag, in dem `uebergehen` aus der `.gitignore` des Ordners kommt.
    [[nodiscard]] QString projektdateiText() const;
    /// Schreibt die Projektdatei und liest das Projekt neu. false = der Text
    /// passt nicht (kein JSON, ein unbekannter Schluessel, ...) oder die Datei
    /// laesst sich nicht schreiben; `fehler` nennt die Meldungen, nichts wird geschrieben.
    bool speichereProjektdatei(const QString& text, QString* fehler = nullptr);
    /// Den Ordner (relativ) in `uebergehen` der Projektdatei eintragen und neu
    /// lesen; gibt es noch keine Projektdatei, wird sie mit dem Vorschlag angelegt.
    bool uebergeheOrdner(const QString& ordner, QString* fehler = nullptr);
    /// Den Eintrag fuer diesen Ordner wieder aus `uebergehen` nehmen und neu lesen.
    bool liesOrdnerWieder(const QString& ordner, QString* fehler = nullptr);

    /// Was sich zeigen laesst: die Funktionen mit Rumpf der gewaehlten Datei
    /// (mit Bereich: `Klasse::methode`), dann die erkannten Zustandsautomaten
    /// (`automat:...`), dann `types`, wenn es Datentypen gibt, dann `calls`, wenn
    /// es Funktionen gibt, und jeder Aufrufgraph `calls:...`, der schon gezeigt wurde.
    [[nodiscard]] QStringList diagramme() const;
    /// Das Diagramm wechseln; das bisherige kommt in den Verlauf fuer zurueck(). Auch
    /// `calls:<Funktion>` fuer jede Funktion der Datei und `types:<Typ>` fuer einen Typ.
    /// false = so heisst keines.
    bool waehleDiagramm(const QString& name);

    /// Was das zweite Feld bei den Datentypen bietet: `*` (alle), dann die Typen der Datei.
    [[nodiscard]] QStringList typen() const;
    /// Den Typ waehlen, der bei den Datentypen gezeigt wird (`*` = alle). false = so heisst keiner.
    bool waehleTyp(const QString& typ);
    /// Der gewaehlte Typ; `*` = alle, leer = es werden keine Datentypen gezeigt.
    [[nodiscard]] QString typ() const;
    /// Wohin ein Typname aus der gewaehlten Datei fuehren kann: je Kandidat `pfad:zeile` (projekt::Typverzeichnis).
    [[nodiscard]] QStringList typZiele(const QString& name) const;
    /// Wie ein Link: zum Typ, den der Name aus der gewaehlten Datei meint; `wahl` unter mehreren (ab 0).
    /// false = keiner, oder mehrere und keine Wahl.
    bool zeigeTyp(const QString& name, int wahl = -1);

    /// Den Aufrufgraph zeigen: ab der Funktion `funktion` der gewaehlten Datei (Name, auch
    /// mit Bereich), leer = den der Datei. false = keine solche Funktion.
    bool zeigeAufrufgraph(const QString& funktion = {});
    /// `calls` (was gerufen wird), `callers` (wer ruft) oder `both`; false = so heisst keine.
    bool setzeAufrufRichtung(const QString& name);
    /// Wie viele Stufen der Aufrufgraph zeigt; 0 = alle.
    void setzeTiefe(int tiefe);
    [[nodiscard]] int tiefe() const;
    /// Hoechstens so viele Funktionen im Aufrufgraph (Vorgabe 300); 0 = unbegrenzt.
    void setzeHoechstens(int hoechstens);
    /// Aufrufe ohne Funktion im Projekt (Bibliothek, Makro) im Aufrufgraph weglassen.
    void setzeOhneFremde(bool ohne);
    /// Im gezeigten Aufrufgraph eine Funktion an der Grenze eine Stufe aufklappen.
    /// false = kein Aufrufgraph, oder keine Funktion dieses Namens darin.
    bool klappeAuf(const QString& funktion);
    /// Die Knoten des gezeigten Aufrufgraphen, je ihr Text (an der Grenze mit ` …`); leer = keiner gezeigt.
    [[nodiscard]] QStringList aufrufgraphKnoten() const;
    [[nodiscard]] QString diagramm() const;
    /// Zum Diagramm davor, auch in einer anderen Datei; kam man ueber einen
    /// Aufruf, ist dessen Knoten danach hervorgehoben. false = es gibt keines.
    bool zurueck();
    /// Der Text des hervorgehobenen Knotens (Zeilen durch `\n`); im Aufrufgraph der Name der
    /// Funktion, bei den Datentypen der des Typs; leer = keiner.
    [[nodiscard]] QString markiert() const;

    /// Wie ein Klick in die Vorschau: die Stelle im Quelltext zeigen. `name`: im Ablauf eine Zeile
    /// eines Knotens, im Aufrufgraph der Name einer Funktion (oder eines Makros), bei den Datentypen
    /// ein Typ (der Kopf seines Kastens) oder `Typ::glied` (ein Feld oder eine Methode).
    /// false = nichts dieses Namens, oder es hat keine Stelle.
    bool klicke(const QString& name);
    /// Die Zeile, die der Quelltext hervorhebt, als `pfad:zeile`; leer = keine.
    [[nodiscard]] QString gezeigteStelle() const;
    /// Wie Rechtsklick im Quelltext, Truth table of line n: die Wahrheitstabelle der Bedingung in Zeile
    /// `zeile` der Datei im Quelltext, als eigenes Diagramm (Back fuehrt zurueck). false = dort keine.
    bool zeigeWahrheitstabelle(int zeile);
    /// Wie Rechtsklick im Quelltext, Mapping table of line n: der switch in Zeile `zeile`, wenn er nur
    /// Werte zuordnet. false = dort keiner.
    bool zeigeZuordnung(int zeile);
    /// Wie Rechtsklick im Quelltext, #if configuration of line n: die Konfigurationstabellen der Datei,
    /// die Kette dieser Zeile (`#if`, `#elif`, `#else`, `#endif`) hervorgehoben. false = dort keine.
    bool zeigeKonfiguration(int zeile);
    /// Wie im Kontextmenue einer Zeile der Konfigurationstabelle, Make this branch active: die erste
    /// Zeile, deren `#if`/`#elif`/`#else` (beim fehlenden: `#endif`) in Zeile `zeile` steht. Setzt die
    /// Namen im Feld Defines wie die Radioknoepfe im Feld Configuration. false = keine solche, oder
    /// der Zweig gilt schon oder laesst sich nicht aus Namen waehlen.
    bool aktiviereZweig(int zeile);
    /// Wie Rechtsklick im Quelltext, Show in diagram: Zeile `zeile` der Datei im Quelltext. In einer
    /// Funktion: ihr Knoten im gezeigten Ablauf (sonst erst zum Ablauf der Funktion; Back fuehrt
    /// zurueck), im Aufrufgraph die Funktion; in einem Typ: Data types mit ihm. Hervorgehoben (Ring),
    /// die Zeile im Quelltext auch. false = die Zeile gehoert zu keiner Funktion und keinem Typ.
    bool zeigeImDiagramm(int zeile);

    /// Wer die gezeigte Funktion ruft, im ganzen Projekt: je Aufrufer `funktion() - pfad:zeile`.
    [[nodiscard]] QStringList aufrufer() const;
    /// Zum Aufrufer Nummer `wahl` (ab 0, wie aufrufer()); dort ist der Knoten
    /// hervorgehoben, der ruft. false = keinen solchen.
    bool springeZuAufrufer(int wahl);

    /// Wohin ein Aufruf aus dem gezeigten Diagramm fuehren kann: je Kandidat
    /// `pfad:zeile` (projekt::kandidaten). Leer = nirgendwohin.
    [[nodiscard]] QStringList ziele(const QString& aufruf) const;
    /// Zum Diagramm der Funktion, die der Aufruf meint; `wahl` waehlt unter
    /// mehreren Kandidaten (ab 0). false = keiner, oder mehrere und keine Wahl.
    bool springe(const QString& aufruf, int wahl = -1);

    /// `svg`, `mermaid`, `dot`, `drawio`, ...; false = so heisst kein Format.
    bool waehleFormat(const QString& name);
    /// kFlussdiagramm, kStruktogramm oder kEntscheidungstabelle: so wird jede Funktion gezeigt, auch nach einem Sprung.
    /// false = so heisst keine Darstellung.
    bool waehleDarstellung(const QString& name);
    [[nodiscard]] QString darstellung() const;
    /// `preview` oder `text`; false = so heisst keine Ansicht.
    bool waehleAnsicht(const QString& name);

    /// Den Ablauf der Funktion `funktion` (Name, auch mit Bereich) hinter dem
    /// ersten Knoten einbetten, der sie eindeutig ruft und noch keine Einbettung
    /// traegt — auch aus einer anderen Datei des Projekts. false = kein solcher Knoten.
    bool betteEin(const QString& funktion);
    /// Alle Einbettungen herausnehmen. false = es gab keine.
    bool loeseEinbettungen();
    [[nodiscard]] int einbettungen() const;

    /// Was beim Auswerten von #if/#ifdef als definiert gilt: `NAME` oder
    /// `NAME=wert`, durch Leerzeichen, Komma oder Strichpunkt getrennt; geht der
    /// Projektdatei vor. Alles wird neu gelesen; Datei und Diagramm bleiben, wenn es sie noch gibt.
    void setzeDefinitionen(const QString& definitionen);
    /// Der Text im Feld Defines.
    [[nodiscard]] QString definitionen() const;
    /// Bedingte Uebersetzung nicht auswerten, sondern alle Zweige lesen.
    void setzeAlleZweige(bool alle);
    /// Ein #if im Rumpf, das ganze Anweisungen schaltet, als Varianten zeigen
    /// (sprache::LeseOptionen::varianten); wirkt nur, solange ausgewertet wird.
    void setzeVarianten(bool zeigen);
    /// Auswerten, aber umrahmen, was ein #if im Rumpf zuschaltet (sprache::LeseOptionen::bereiche).
    void setzeBereiche(bool rahmen);
    /// Das Feld Configuration (Namen und Ketten #if der gewaehlten Datei); es schreibt ins Feld Defines.
    [[nodiscard]] Konfiguration& konfiguration() { return *m_konfiguration; }
    [[nodiscard]] const Konfiguration& konfiguration() const { return *m_konfiguration; }
    /// Was im Feld Defines steht, in `definitionen` der Projektdatei schreiben
    /// (gleichnamige ersetzen, auch durch `!NAME`) und neu lesen. false = kein Ordner
    /// offen, oder die Projektdatei hat Meldungen; `fehler` sagt, was. `hinweise`: je
    /// Abschnitt, der einen Namen anders festlegt (dort gilt weiter der Abschnitt).
    bool speichereKonfiguration(QString* fehler = nullptr, QStringList* hinweise = nullptr);
    /// Namen, nach denen ein #if der gewaehlten Datei gefragt hat und die nirgends definiert sind.
    [[nodiscard]] QStringList unbekannteNamen() const;

    [[nodiscard]] QString quelltext() const;
    [[nodiscard]] QString diagrammText() const;
    /// Wie viele Knoten oder Kaesten die Vorschau zeichnet.
    [[nodiscard]] int vorschauFormen() const;
    /// Die Tooltips der Tabellensymbole in der Vorschau, von oben nach unten.
    [[nodiscard]] QStringList tabellenSymbole() const;
    /// Das ganze gezeichnete Diagramm, in Originalgroesse — auch was das Fenster nicht zeigt.
    [[nodiscard]] QImage vorschauBild() const;
    /// Wie viele Meldungen das Lesen der gewaehlten Datei hinterlassen hat.
    [[nodiscard]] int meldungen() const;

    /// Die Filter des Dialogs Open, aus den Endungen der Sprachen des Kerns.
    [[nodiscard]] static QString dateiFilter();

    /// File > Settings: den Dialog oeffnen, ohne zu warten (fensterweit modal); ist er offen,
    /// nach vorn holen. Gibt ihn zurueck (fuer Tests und den Tester).
    EinstellungenDialog* zeigeEinstellungen();

protected:
    /// Wechselt die Palette (Thema gesetzt, oder das System schaltet um): neu zeichnen.
    void changeEvent(QEvent* ereignis) override;

private:
    /// Ist eine Funktion als Struktogramm gezeigt? Dann haelt m_fluss je Block einen Knoten.
    [[nodiscard]] bool zeigtStruktogramm() const;
    [[nodiscard]] bool istWahrheitstabelle() const;
    /// Ist eine Funktion als Entscheidungstabelle gezeigt? Dann haelt m_fluss je Zeile einen Knoten.
    [[nodiscard]] bool zeigtEntscheidungstabelle() const;
    /// Im Struktogramm und in der Entscheidungstabelle gibt es (noch) kein Einbetten.
    [[nodiscard]] bool ohneEinbetten() const;
    /// Die Funktion (Stelle in der Einheit), in deren Rumpf die Zeile steht; kKeine = keine.
    [[nodiscard]] std::size_t funktionZurZeile(std::size_t datei, int zeile) const;
    /// Steht in dieser Zeile eine Bedingung mit Wahrheitstabelle?
    [[nodiscard]] bool hatWahrheitstabelle(std::size_t datei, int zeile) const;
    /// Steht in dieser Zeile ein switch, der nur Werte zuordnet?
    [[nodiscard]] bool hatZuordnung(std::size_t datei, int zeile) const;
    /// Steht in dieser Zeile eine Direktive einer Kette #if (`#if`, `#elif`, `#else`, `#endif`)?
    [[nodiscard]] bool hatKonfiguration(std::size_t datei, int zeile) const;
    /// Der Knoten einer Zeile der Konfigurationstabelle: welche Tabelle, welche Zeile; nullopt = keiner.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> konfigurationsZeile(c2d::diagramm::KnotenId knoten) const;
    /// Die Definitionen im Feld Defines, mit denen der Zweig dieses Knotens gilt; nullopt und `warum`
    /// = es geht nicht (er gilt schon, oder er laesst sich nicht aus Namen waehlen).
    [[nodiscard]] std::optional<std::vector<c2d::sprache::Definition>> zweigDefinitionen(c2d::diagramm::KnotenId knoten,
                                                                                         QString* warum = nullptr) const;
    bool aktiviereZweig(c2d::diagramm::KnotenId knoten);
    [[nodiscard]] bool istZuordnung() const;
    /// Zur Zuordnung des switch in der Zeile; `von` wie bei gehe(). false = dort keine.
    bool geheZuZuordnung(std::size_t datei, int zeile, c2d::diagramm::KnotenId von);
    /// Zur Tabelle einer Zeile: ihre Wahrheitstabelle, sonst ihre Zuordnung.
    bool geheZuTabelle(std::size_t datei, int zeile, c2d::diagramm::KnotenId von);
    /// Die Zeilen eines Knotens (beim Buendeln mehrere), in denen eine Tabelle steht (Wahrheit, Zuordnung).
    [[nodiscard]] std::vector<int> tabellenZeilen(c2d::diagramm::KnotenId knoten) const;
    /// Der Tooltip des Tabellensymbols am Knoten: welche Tabelle ein Klick zeigt; leer = keine.
    [[nodiscard]] QString tabellenHinweis(c2d::diagramm::KnotenId knoten) const;
    /// Zur Wahrheitstabelle der Zeile; `von` wie bei gehe(). false = dort keine.
    bool geheZuWahrheitstabelle(std::size_t datei, int zeile, c2d::diagramm::KnotenId von);
    /// Was mit Farben der Palette gezeichnet ist, neu: Vorschau, Marke, Zeile im Quelltext.
    void farbenNeu();
    /// Die Hinterlegung der gezeigten Zeile: Highlight, halb durchsichtig — hell wie dunkel lesbar.
    [[nodiscard]] QColor zeilenFarbe() const;
    /// Die Pfeile von Back und Called from in der Textfarbe: die des Stils sind schwarz, auch im Dunkeln.
    void faerbeSymbole();

    /// Eine Stelle im Verlauf: welche Datei, welches Diagramm.
    struct Stelle
    {
        std::size_t datei;
        QString diagramm;
        /// der Knoten, von dem aus man weiterging (ein Aufruf); kKeine = keiner
        c2d::diagramm::KnotenId knoten = c2d::diagramm::kKeine;
        QString typ;  ///< bei den Datentypen: der gewaehlte Typ
    };

    /// Die Stelle, die gerade gezeigt wird — fuer den Verlauf.
    [[nodiscard]] Stelle hier(c2d::diagramm::KnotenId von = c2d::diagramm::kKeine) const;
    /// Die Typen, die ein Knoten des Ablaufs nennt.
    [[nodiscard]] std::vector<c2d::projekt::TypVerweis> typVerweiseVon(c2d::diagramm::KnotenId knoten) const;
    /// Die Typen, die ein Glied des gezeigten Strukturdiagramms nennt.
    [[nodiscard]] std::vector<c2d::projekt::TypVerweis> typVerweiseVon(std::size_t kasten, std::size_t eintrag) const;
    [[nodiscard]] QString beschreibe(const c2d::projekt::TypStelle& stelle) const;
    /// Zu einem Typ: seine Datei, Data types, er im zweiten Feld; `von` wie bei gehe().
    void geheZuTyp(const c2d::projekt::TypStelle& stelle, c2d::diagramm::KnotenId von = c2d::diagramm::kKeine);
    /// Den Typ im zweiten Feld waehlen, ohne dass der Wechsel in den Verlauf kommt.
    bool setzeTypStill(const QString& typ);
    /// Ein Menue mit Funktionen und Typen, je einem Eintrag zum Hingehen.
    void zielMenue(const std::vector<c2d::projekt::Fundstelle>& funktionen,
                   const std::vector<c2d::projekt::TypVerweis>& typen, c2d::diagramm::KnotenId von, QMenu& menue);
    void kastenDoppelklick(std::size_t kasten, std::size_t eintrag);
    void kastenMenue(std::size_t kasten, std::size_t eintrag, const QPoint& ort);

    /// Mit den jetzigen Lese-Optionen (neu) einlesen: die Datei oder den Ordner.
    void liesEin();
    /// Neu einlesen; Datei und Diagramm bleiben, wenn es sie noch gibt.
    void liesNeu();
    /// Was im Feld Defines steht.
    [[nodiscard]] std::vector<c2d::sprache::Definition> definitionenImFeld() const;
    /// Das Feld Configuration auf die gewaehlte Datei und Funktion bringen.
    void aktualisiereKonfiguration();
    /// Die Datei `index` zur gewaehlten machen: Quelltext, Auswahl, Diagramm.
    void zeigeDatei(std::size_t index);
    void fuelleBaum();
    void zeigeLage();
    /// Das gewaehlte Diagramm neu aus dem Codemodell bauen (ohne Einbettungen) und zeigen.
    void baueNeu();
    /// Das Diagramm, wie es jetzt ist, zeichnen und als Text schreiben.
    void zeichne();
    void speichereUnter();
    /// Einen Pfad vorn in die Liste Open recent nehmen (und speichern).
    void merkeZuletzt(const QString& pfad);
    void schreibeZuletzt() const;
    void fuelleZuletztMenue(QMenu* menue);
    void bearbeiteProjektdatei();
    void zeigeProjektMeldungen();
    /// Eine Zeile einer Datei im Quelltext zeigen (die Datei wird dafuer geladen).
    /// false = keine solche Datei oder Zeile.
    bool zeigeZeile(std::size_t datei, int zeile);
    /// Ein Klick auf einen Knoten (Ablauf oder Aufrufgraph): seine Stelle im Quelltext.
    bool zeigeStelle(c2d::diagramm::KnotenId knoten);
    /// Ein Klick auf einen Kasten oder ein Glied (wie Vorschau::beiKlickKasten): seine Stelle.
    bool zeigeStelle(std::size_t kasten, std::size_t eintrag);
    void kontextMenue(c2d::diagramm::KnotenId knoten, std::size_t gruppe, const QPoint& ort);
    void baumMenue(const QPoint& ort);
    /// Aus welcher Datei ein Knoten stammt: die seiner Einbettung, sonst die gewaehlte.
    [[nodiscard]] std::size_t dateiVon(c2d::diagramm::KnotenId knoten) const;
    /// Was die Aufrufe eines Knotens meinen koennen, ohne Doppelte.
    [[nodiscard]] std::vector<c2d::projekt::Fundstelle> zieleVon(c2d::diagramm::KnotenId knoten) const;
    [[nodiscard]] QString beschreibe(const c2d::projekt::Fundstelle& stelle) const;
    /// Zum Diagramm dieser Funktion, auch in einer anderen Datei; `von` ist der
    /// Knoten, von dem aus man geht — zurueck() hebt ihn hervor.
    void gehe(const c2d::projekt::Fundstelle& stelle, c2d::diagramm::KnotenId von = c2d::diagramm::kKeine);
    /// Die Funktion, deren Ablauf gezeigt wird; leer bei den Datentypen.
    [[nodiscard]] std::optional<c2d::projekt::Fundstelle> gezeigteFunktion() const;
    /// Den ersten Knoten hervorheben, dessen Aufruf `ziel` meinen kann.
    void markiereAufrufVon(const c2d::projekt::Fundstelle& ziel);
    /// Knopf Back und Menue Called from auf den Stand bringen.
    void aktualisiereLeiste();
    /// Den Eintrag `name` waehlen; ein Aufrufgraph `calls:<Funktion>` der Datei wird dafuer angelegt.
    bool waehleEintrag(const QString& name);
    [[nodiscard]] bool istAufrufgraph() const;
    /// Den gezeigten Aufrufgraph neu ableiten (Richtung, Tiefe, Fremde, aufgeklappt).
    void leiteAufrufgraphAb();
    /// Zum Aufrufgraph ab dieser Funktion, auch in einer anderen Datei; `von` wie bei gehe().
    void geheZuAufrufgraph(const c2d::projekt::Fundstelle& stelle, c2d::diagramm::KnotenId von = c2d::diagramm::kKeine);
    void aufrufgraphMenue(c2d::diagramm::KnotenId knoten, const QPoint& ort);
    void fuelleAufruferMenue(QMenu* menue);
    bool betteEin(c2d::diagramm::KnotenId knoten, const c2d::projekt::Fundstelle& gerufen);
    [[nodiscard]] QString projektdateiPfad() const;
    /// Die Projektdatei, wie sie auf der Platte steht — oder der Vorschlag, wenn
    /// es keine gibt. Leer, wenn sie Meldungen hat: dann wird sie nicht still neu geschrieben.
    [[nodiscard]] std::optional<c2d::projekt::Projektdatei> einstellungenZumAendern(QString* fehler) const;
    bool schreibeProjektdatei(const c2d::projekt::Projektdatei& datei, QString* fehler);

    QTreeWidget* m_baum = nullptr;
    QPlainTextEdit* m_quelle = nullptr;
    QPlainTextEdit* m_text = nullptr;
    Vorschau* m_vorschau = nullptr;
    Konfiguration* m_konfiguration = nullptr;
    QTabWidget* m_ansichten = nullptr;
    QComboBox* m_auswahl = nullptr;
    QComboBox* m_typAuswahl = nullptr;  ///< bei den Datentypen: welcher Typ
    QComboBox* m_format = nullptr;
    QComboBox* m_richtung = nullptr;
    QLineEdit* m_definitionen = nullptr;
    QCheckBox* m_alleZweige = nullptr;
    QCheckBox* m_varianten = nullptr;
    QCheckBox* m_bereiche = nullptr;
    QAction* m_zurueckAktion = nullptr;
    QAction* m_aufruferAktion = nullptr;
    QAction* m_projektdateiAktion = nullptr;
    QAction* m_meldungenAktion = nullptr;
    QAction* m_aufrufgraphAktion = nullptr;
    QComboBox* m_aufrufRichtung = nullptr;
    QSpinBox* m_tiefe = nullptr;
    QSpinBox* m_hoechstens = nullptr;
    QCheckBox* m_ohneFremde = nullptr;

    QStringList m_zuletzt;  ///< File > Open recent, das Neueste zuerst
    QString m_ordner;      ///< der Ordner des Projekts; leer = eine einzelne Datei
    QString m_einzelText;  ///< bei einer einzelnen Datei: ihr Text, zum neu Lesen
    QString m_einzelName;
    QString m_gelesenMit;  ///< der Text des Felds Defines beim letzten Lesen
    c2d::projekt::Projekt m_projekt;
    c2d::projekt::Typverzeichnis m_typen;      ///< zu m_projekt
    c2d::diagramm::Strukturdiagramm m_struktur;  ///< das gezeigte Strukturdiagramm
    QString m_typ;                             ///< der Typ im zweiten Feld; `*` = alle
    std::size_t m_datei = c2d::projekt::kKeine;        ///< die gewaehlte Datei
    std::size_t m_quelleZeigt = c2d::projekt::kKeine;  ///< die Datei im Quelltext
    int m_quelleZeile = 0;  ///< die hervorgehobene Zeile darin; 0 = keine
    QPointer<EinstellungenDialog> m_einstellungen;  ///< der offene Dialog Settings
    bool m_farbenAusstehend = false;  ///< farbenNeu() ist schon angestossen
    /// Das gezeigte Flussdiagramm, samt Einbettungen. Im Struktogramm je Block (Stelle in
    /// bloeckeVon()) ein Knoten ohne Kanten: so gelten Klick, Spruenge, Show in diagram fuer beide.
    c2d::diagramm::Flussdiagramm m_fluss;
    c2d::diagramm::Struktogramm m_struktogramm;  ///< das gezeigte Struktogramm; leer = keines
    std::vector<c2d::diagramm::Wahrheitstabelle> m_tabellen;  ///< die gezeigte Wahrheitstabelle
    c2d::diagramm::Entscheidungstabelle m_entscheidung;       ///< die gezeigte; leer = keine
    std::vector<c2d::diagramm::Zuordnung> m_zuordnungen;      ///< die gezeigte Zuordnung oder die Nachschlagetabellen
    std::vector<c2d::diagramm::Konfigurationstabelle> m_ketten;  ///< die Konfigurationstabellen: die Ketten #if der Datei
    bool m_textVerfuegbar = true;  ///< kann das gewaehlte Format das gezeigte Diagramm (sonst steht eine Meldung da)
    QComboBox* m_darstellung = nullptr;
    std::vector<c2d::diagramm::Zustandsdiagramm> m_automaten;  ///< die der gewaehlten Datei
    QStringList m_automatNamen;                                 ///< je Automat sein Name in der Auswahl
    c2d::projekt::AufrufgraphErgebnis m_aufrufgraph;  ///< der gezeigte Aufrufgraph
    std::vector<c2d::projekt::Fundstelle> m_aufgeklappt;  ///< darin aufgeklappt
    QString m_gezeigt;                      ///< wofuer m_fluss (m_aufrufgraph) gebaut ist
    QList<Stelle> m_verlauf;                ///< die Diagramme davor, fuer zurueck()
    bool m_ohneVerlauf = false;             ///< der Wechsel gerade ist ein zurueck() oder ein Sprung
};

}  // namespace c2d::ui
