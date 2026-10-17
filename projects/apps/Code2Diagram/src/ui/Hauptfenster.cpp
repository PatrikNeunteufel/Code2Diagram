/**
 * @file Hauptfenster.cpp
 * @brief Das Fenster: Menue, Dateien, Quelltext, Auswahl von Diagramm und Format, Vorschau mit Sprung und Einbettung
 */
#include "pch.h"
#include "ui/Hauptfenster.hpp"

#include "AppInfo.hpp"
#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "ui/Einstellungen.hpp"
#include "ui/Konfiguration.hpp"
#include "ui/Vorschau.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QTextBlock>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <tuple>

namespace c2d::ui
{

namespace
{

using c2d::projekt::Fundstelle;
using c2d::projekt::kKeine;

/// Steht wortgleich in beispiele/ampel.c.
constexpr std::string_view kBeispiel =
    R"c(/* Beispiel: eine Ampel als Zustandsautomat - enum, struct, switch. */
#include <stdint.h>

typedef enum { ROT, ROT_GELB, GRUEN, GELB } Phase;

typedef struct {
    Phase phase;
    uint16_t rest_ms;
    uint8_t stoerung : 1;
} Ampel;

static uint16_t dauer_ms(Phase phase)
{
    switch (phase) {
    case ROT:
        return 5000;
    case GRUEN:
        return 4000;
    case ROT_GELB:
    case GELB:
        return 1000;
    }
    return 0;
}

void ampel_takt(Ampel *ampel, uint16_t vergangen_ms)
{
    if (ampel->stoerung) {
        ampel->phase = GELB;
        return;
    }
    if (ampel->rest_ms > vergangen_ms) {
        ampel->rest_ms -= vergangen_ms;
        return;
    }
    ampel->phase = (Phase)((ampel->phase + 1) % 4);
    ampel->rest_ms = dauer_ms(ampel->phase);
}
)c";

// Was ein Eintrag im Baum der Dateien ist (Daten unter kArt), und sein Pfad (unter kPfad).
constexpr int kPfad = Qt::UserRole;
constexpr int kArt = Qt::UserRole + 1;
constexpr int kEintragDatei = 0;
constexpr int kEintragOrdner = 1;
constexpr int kEintragUebergangen = 2;

QString ausStd(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::filesystem::path alsWeg(const QString& pfad)
{
    return std::filesystem::path(pfad.toStdU16String());
}

QPlainTextEdit* neuesTextfeld(QWidget* parent)
{
    auto* feld = new QPlainTextEdit(parent);
    feld->setReadOnly(true);
    feld->setLineWrapMode(QPlainTextEdit::NoWrap);
    feld->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    return feld;
}

bool waehle(QComboBox* liste, const QString& daten)
{
    const int index = liste->findData(daten);
    if (index < 0) return false;
    liste->setCurrentIndex(index);
    return true;
}

QString letzterName(const QString& pfad)
{
    return pfad.section(QLatin1Char('/'), -1);
}

}  // namespace

Hauptfenster::Hauptfenster(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(ausStd(appName()));
    resize(1300, 800);

    auto* teiler = new QSplitter(Qt::Horizontal, this);
    auto* linksTeiler = new QSplitter(Qt::Vertical, teiler);
    m_baum = new QTreeWidget(linksTeiler);
    m_baum->setObjectName(QStringLiteral("dateien"));
    m_baum->setHeaderHidden(true);
    m_baum->setContextMenuPolicy(Qt::CustomContextMenu);
    m_baum->setToolTip(QStringLiteral("The files of the project. Right-click a folder to skip it."));
    m_baum->hide();
    auto* links = new QWidget(linksTeiler);
    m_quelle = neuesTextfeld(links);
    m_quelle->setObjectName(QStringLiteral("quelle"));
    m_quelle->setPlaceholderText(QStringLiteral("Open a source file or a folder (File > Open...)"));
    // Rechtsklick: das uebliche Menue, darunter der Weg ins Diagramm
    m_quelle->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_quelle, &QWidget::customContextMenuRequested, this, [this](const QPoint& ort) {
        const std::unique_ptr<QMenu> menue(m_quelle->createStandardContextMenu(ort));
        const int zeile = m_quelle->cursorForPosition(ort).blockNumber() + 1;
        menue->addSeparator();
        menue->addAction(QStringLiteral("Show in diagram"), this, [this, zeile] { zeigeImDiagramm(zeile); })
            ->setEnabled(!m_quelle->toPlainText().isEmpty());
        const std::size_t datei = m_quelleZeigt < m_projekt.dateien.size() ? m_quelleZeigt : m_datei;
        menue->addAction(QStringLiteral("Truth table of line %1").arg(zeile), this, [this, zeile] { zeigeWahrheitstabelle(zeile); })
            ->setEnabled(hatWahrheitstabelle(datei, zeile));
        menue->addAction(QStringLiteral("Mapping table of line %1").arg(zeile), this, [this, zeile] { zeigeZuordnung(zeile); })
            ->setEnabled(hatZuordnung(datei, zeile));
        menue->addAction(QStringLiteral("#if configuration of line %1").arg(zeile), this, [this, zeile] { zeigeKonfiguration(zeile); })
            ->setEnabled(hatKonfiguration(datei, zeile));
        menue->exec(m_quelle->mapToGlobal(ort));
    });
    m_definitionen = new QLineEdit(links);
    m_definitionen->setObjectName(QStringLiteral("definitionen"));
    m_definitionen->setPlaceholderText(QStringLiteral("NAME NAME=value !NAME ... (for #if / #ifdef)"));
    m_definitionen->setToolTip(QStringLiteral(
        "What counts as defined when #if and #ifdef are evaluated, in addition to the #define of the file.\n"
        "Goes before the project file and the included headers. !NAME: not defined (like -U).\n"
        "Separate with spaces. Press Enter to read again."));
    m_alleZweige = new QCheckBox(QStringLiteral("All #if branches"), links);
    m_alleZweige->setObjectName(QStringLiteral("alleZweige"));
    m_alleZweige->setToolTip(QStringLiteral("Do not evaluate #if and #ifdef: read every branch, one after the other."));
    m_varianten = new QCheckBox(QStringLiteral("Show #if variants"), links);
    m_varianten->setObjectName(QStringLiteral("varianten"));
    m_varianten->setToolTip(QStringLiteral(
        "An #if inside a function whose branches hold whole statements becomes a hexagon in the flow,\n"
        "with one edge per branch; the branch that holds is marked (active). Everything else is evaluated."));
    m_bereiche = new QCheckBox(QStringLiteral("Frame #if regions"), links);
    m_bereiche->setObjectName(QStringLiteral("bereiche"));
    m_bereiche->setToolTip(QStringLiteral(
        "Evaluate #if, but frame what an #if inside a function switches on: one colour per condition,\n"
        "with a legend. Show #if variants frames its branches as well."));
    auto* lesen = new QHBoxLayout;
    lesen->addWidget(new QLabel(QStringLiteral("Defines:"), links));
    lesen->addWidget(m_definitionen, 1);
    // die Haken in einer eigenen Zeile: sonst bleibt fuer Defines kaum Platz
    auto* haken = new QHBoxLayout;
    haken->addWidget(m_alleZweige);
    haken->addWidget(m_varianten);
    haken->addWidget(m_bereiche);
    haken->addStretch(1);
    auto* linkeSpalte = new QVBoxLayout(links);
    linkeSpalte->setContentsMargins(0, 0, 0, 0);
    linkeSpalte->addWidget(m_quelle, 1);
    linkeSpalte->addLayout(lesen);
    linkeSpalte->addLayout(haken);
    linksTeiler->addWidget(m_baum);
    linksTeiler->addWidget(links);
    linksTeiler->setStretchFactor(0, 1);
    linksTeiler->setStretchFactor(1, 3);
    connect(m_definitionen, &QLineEdit::editingFinished, this, [this] {
        // das Feld meldet sich auch, wenn es nur verlassen wird: neu lesen nur, wenn sich etwas geaendert hat
        if (m_definitionen->text() != m_gelesenMit) liesNeu();
    });
    connect(m_alleZweige, &QCheckBox::toggled, this, [this](bool alle) {
        m_definitionen->setEnabled(!alle);
        m_varianten->setEnabled(!alle);  // Varianten und Rahmen gibt es nur beim Auswerten
        m_bereiche->setEnabled(!alle && !m_varianten->isChecked());
        liesNeu();
    });
    connect(m_varianten, &QCheckBox::toggled, this, [this](bool zeigen) {
        m_bereiche->setEnabled(!zeigen && !m_alleZweige->isChecked());  // Varianten rahmen ihre Zweige selbst
        liesNeu();
    });
    connect(m_bereiche, &QCheckBox::toggled, this, [this] { liesNeu(); });
    connect(m_baum, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* eintrag) {
        if (eintrag != nullptr && eintrag->data(0, kArt).toInt() == kEintragDatei)
            waehleDatei(eintrag->data(0, kPfad).toString());
    });
    connect(m_baum, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& ort) { baumMenue(ort); });

    auto* rechts = new QWidget(teiler);
    m_auswahl = new QComboBox(rechts);
    m_auswahl->setObjectName(QStringLiteral("auswahl"));
    m_typAuswahl = new QComboBox(rechts);
    m_typAuswahl->setObjectName(QStringLiteral("typAuswahl"));
    m_typAuswahl->setToolTip(QStringLiteral("The type shown, with the types next to it in this file"));
    m_typAuswahl->hide();
    m_format = new QComboBox(rechts);
    m_format->setObjectName(QStringLiteral("format"));
    for (const c2d::ausgabe::Format* f : c2d::ausgabe::formate())
        m_format->addItem(ausStd(f->name()), ausStd(f->name()));
    m_darstellung = new QComboBox(rechts);
    m_darstellung->setObjectName(QStringLiteral("darstellung"));
    m_darstellung->addItem(QStringLiteral("Flowchart"), QString::fromLatin1(kFlussdiagramm));
    m_darstellung->addItem(QStringLiteral("Nassi-Shneiderman"), QString::fromLatin1(kStruktogramm));
    m_darstellung->addItem(QStringLiteral("Decision table"), QString::fromLatin1(kEntscheidungstabelle));
    m_darstellung->setToolTip(
        QStringLiteral("How a function is shown: its flowchart, its structogram (Nassi-Shneiderman) or its decision table"));
    m_richtung = new QComboBox(rechts);
    m_richtung->setObjectName(QStringLiteral("richtung"));
    m_richtung->addItem(QStringLiteral("Top-down"), QStringLiteral("TD"));
    m_richtung->addItem(QStringLiteral("Left-right"), QStringLiteral("LR"));

    m_ansichten = new QTabWidget(rechts);
    m_ansichten->setObjectName(QStringLiteral("ansichten"));
    m_vorschau = new Vorschau(m_ansichten);
    m_vorschau->setObjectName(QStringLiteral("vorschau"));
    m_text = neuesTextfeld(m_ansichten);
    m_text->setObjectName(QStringLiteral("diagramm"));
    m_text->setPlaceholderText(QStringLiteral("The diagram appears here as text"));
    m_ansichten->addTab(m_vorschau, QStringLiteral("Preview"));
    m_ansichten->addTab(m_text, QStringLiteral("Text"));

    auto* leiste = new QHBoxLayout;
    leiste->addWidget(new QLabel(QStringLiteral("Diagram:"), rechts));
    leiste->addWidget(m_auswahl, 1);
    leiste->addWidget(m_typAuswahl, 1);
    leiste->addWidget(m_darstellung);
    leiste->addWidget(new QLabel(QStringLiteral("Format:"), rechts));
    leiste->addWidget(m_format);
    leiste->addWidget(m_richtung);
    auto* spalte = new QVBoxLayout(rechts);
    spalte->setContentsMargins(0, 0, 0, 0);
    spalte->addLayout(leiste);
    spalte->addWidget(m_ansichten, 1);

    teiler->addWidget(linksTeiler);
    teiler->addWidget(rechts);
    teiler->setStretchFactor(0, 2);
    teiler->setStretchFactor(1, 3);
    setCentralWidget(teiler);

    connect(m_auswahl, &QComboBox::currentIndexChanged, this, [this] {
        // das bisherige Diagramm merken — ausser der Wechsel ist selbst ein Schritt zurueck oder ein Sprung
        if (!m_ohneVerlauf && !m_gezeigt.isEmpty() && m_gezeigt != m_auswahl->currentData().toString())
            m_verlauf << hier();
        baueNeu();
    });
    for (QComboBox* liste : {m_format, m_richtung})
        connect(liste, &QComboBox::currentIndexChanged, this, [this] { zeichne(); });
    // die Darstellung gilt fuer jede Funktion: neu bauen, was gezeigt ist (Einbettungen gehen dabei)
    connect(m_darstellung, &QComboBox::currentIndexChanged, this, [this] { baueNeu(); });
    connect(m_typAuswahl, &QComboBox::currentIndexChanged, this, [this] {
        const QString neu = m_typAuswahl->currentData().toString();
        if (neu.isEmpty() || neu == m_typ) return;
        if (!m_ohneVerlauf && m_gezeigt == QLatin1String(kDatentypen)) m_verlauf << hier();
        m_typ = neu;
        zeichne();
        aktualisiereLeiste();
    });
    // ein Klick zeigt die Stelle im Quelltext, ein Doppelklick fuehrt zu dem, was genannt ist
    m_vorschau->beiKlick = [this](c2d::diagramm::KnotenId knoten) { zeigeStelle(knoten); };
    m_vorschau->beiKlickKasten = [this](std::size_t kasten, std::size_t eintrag) { zeigeStelle(kasten, eintrag); };
    m_vorschau->beiDoppelklickKasten = [this](std::size_t kasten, std::size_t eintrag) { kastenDoppelklick(kasten, eintrag); };
    m_vorschau->beiKontextKasten = [this](std::size_t kasten, std::size_t eintrag, const QPoint& ort) {
        kastenMenue(kasten, eintrag, ort);
    };

    m_vorschau->beiTabelle = [this](c2d::diagramm::KnotenId knoten) {
        const std::vector<int> zeilen = tabellenZeilen(knoten);
        if (!zeilen.empty()) geheZuTabelle(dateiVon(knoten), zeilen.front(), knoten);
    };
    m_vorschau->beiDoppelklick = [this](c2d::diagramm::KnotenId knoten) {
        if (istWahrheitstabelle()) return;  // eine Zelle fuehrt nirgendwohin
        if (istAufrufgraph())
        {
            // eine Funktion: ihr Ablauf; Back fuehrt zurueck in den Graph
            if (knoten >= m_aufrufgraph.stellen.size()) return;
            const Fundstelle stelle = m_aufrufgraph.stellen[knoten];
            if (stelle.datei == kKeine)
                statusBar()->showMessage(
                    QStringLiteral("%1: no body in the project").arg(ausStd(m_aufrufgraph.graph.knoten[knoten].name)), 4000);
            else
                gehe(stelle, knoten);
            return;
        }
        if (knoten >= m_fluss.knoten().size()) return;
        const std::vector<Fundstelle> ziele = zieleVon(knoten);
        const std::vector<c2d::projekt::TypVerweis> typen = typVerweiseVon(knoten);
        std::size_t typZiele = 0;
        for (const c2d::projekt::TypVerweis& t : typen) typZiele += t.ziele.size();
        if (ziele.size() + typZiele == 1)
        {
            if (ziele.empty())
                geheZuTyp(typen.front().ziele.front(), knoten);
            else
                gehe(ziele.front(), knoten);
            return;
        }
        if (ziele.empty() && typZiele == 0) return;  // die Zeile hat schon der Klick gezeigt
        // mehr als ein Ziel: nicht still eines waehlen — erst die Funktionen, dann die Typen
        statusBar()->showMessage(QStringLiteral("%1 functions and types qualify").arg(ziele.size() + typZiele), 5000);
        QMenu wahl(this);
        zielMenue(ziele, typen, knoten, wahl);
        wahl.exec(QCursor::pos());
    };
    m_vorschau->beiKontext = [this](c2d::diagramm::KnotenId knoten, std::size_t gruppe, const QPoint& ort) {
        if (istAufrufgraph())
            aufrufgraphMenue(knoten, ort);
        else
            kontextMenue(knoten, gruppe, ort);
    };

    QMenu* datei = menuBar()->addMenu(QStringLiteral("&File"));
    datei->addAction(QStringLiteral("&Open..."), QKeySequence::Open, this, [this] {
        const QString pfad =
            QFileDialog::getOpenFileName(this, QStringLiteral("Open source file"), {}, dateiFilter());
        if (pfad.isEmpty()) return;
        QString fehler;
        if (!oeffne(pfad, &fehler)) QMessageBox::warning(this, windowTitle(), fehler);
    });
    datei->addAction(QStringLiteral("Open &folder..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), this, [this] {
        const QString pfad = QFileDialog::getExistingDirectory(this, QStringLiteral("Open a folder as project"));
        if (pfad.isEmpty()) return;
        QString fehler;
        if (!oeffneOrdner(pfad, &fehler)) QMessageBox::warning(this, windowTitle(), fehler);
    });
    auto* zuletzt = datei->addMenu(QStringLiteral("Open &recent"));
    zuletzt->setObjectName(QStringLiteral("zuletzt"));
    connect(zuletzt, &QMenu::aboutToShow, this, [this, zuletzt] {
        zuletzt->clear();
        fuelleZuletztMenue(zuletzt);
    });
    datei->addSeparator();
    datei->addAction(QStringLiteral("&Save diagram as..."), QKeySequence::Save, this,
                     [this] { speichereUnter(); });
    datei->addSeparator();
    // QKeySequence::Preferences ist unter Windows leer
    datei->addAction(QStringLiteral("Se&ttings..."), QKeySequence(Qt::CTRL | Qt::Key_Comma), this,
                     [this] { zeigeEinstellungen(); });
    datei->addSeparator();
    datei->addAction(QStringLiteral("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* projekt = menuBar()->addMenu(QStringLiteral("&Project"));
    m_projektdateiAktion =
        projekt->addAction(QStringLiteral("&Edit project file..."), this, [this] { bearbeiteProjektdatei(); });
    m_meldungenAktion =
        projekt->addAction(QStringLiteral("Show project &messages"), this, [this] { zeigeProjektMeldungen(); });
    m_projektdateiAktion->setEnabled(false);
    m_meldungenAktion->setEnabled(false);

    // Back und Called from: im Menue und als Knoepfe in der Leiste
    m_zurueckAktion = new QAction(QStringLiteral("&Back"), this);
    m_zurueckAktion->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    m_zurueckAktion->setEnabled(false);
    connect(m_zurueckAktion, &QAction::triggered, this, [this] {
        if (!zurueck()) statusBar()->showMessage(QStringLiteral("No diagram to go back to"), 3000);
    });
    auto* aufruferMenue = new QMenu(this);
    aufruferMenue->setObjectName(QStringLiteral("aufrufer"));
    connect(aufruferMenue, &QMenu::aboutToShow, this, [this, aufruferMenue] {
        // erst beim Oeffnen: das Suchen geht ueber das ganze Projekt
        aufruferMenue->clear();
        fuelleAufruferMenue(aufruferMenue);
    });
    m_aufruferAktion = new QAction(QStringLiteral("Called &from"), this);
    m_aufruferAktion->setMenu(aufruferMenue);
    m_aufruferAktion->setEnabled(false);
    m_aufruferAktion->setToolTip(QStringLiteral("The functions in the project that call the one shown"));
    QToolBar* navigation = addToolBar(QStringLiteral("Navigation"));
    navigation->setObjectName(QStringLiteral("navigation"));
    navigation->setMovable(false);
    navigation->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    faerbeSymbole();
    navigation->addAction(m_zurueckAktion);
    navigation->addAction(m_aufruferAktion);
    if (auto* knopf = qobject_cast<QToolButton*>(navigation->widgetForAction(m_aufruferAktion)))
        knopf->setPopupMode(QToolButton::InstantPopup);

    // der Aufrufgraph: ab der gezeigten Funktion (sonst der Datei), mit Richtung, Tiefe und Fremden
    navigation->addSeparator();
    m_aufrufgraphAktion =
        new QAction(style()->standardIcon(QStyle::SP_FileDialogListView), QStringLiteral("Call &graph from here"), this);
    m_aufrufgraphAktion->setToolTip(
        QStringLiteral("The call graph from the function shown; without one, the call graph of the file"));
    connect(m_aufrufgraphAktion, &QAction::triggered, this, [this] {
        if (const std::optional<Fundstelle> f = gezeigteFunktion())
            geheZuAufrufgraph(*f);
        else
            zeigeAufrufgraph();
    });
    navigation->addAction(m_aufrufgraphAktion);
    m_aufrufRichtung = new QComboBox(navigation);
    m_aufrufRichtung->setObjectName(QStringLiteral("aufrufRichtung"));
    m_aufrufRichtung->addItem(QStringLiteral("Calls"), QStringLiteral("calls"));
    m_aufrufRichtung->addItem(QStringLiteral("Callers"), QStringLiteral("callers"));
    m_aufrufRichtung->addItem(QStringLiteral("Both"), QStringLiteral("both"));
    m_aufrufRichtung->setToolTip(QStringLiteral("Call graph: what the function calls, who calls it, or both"));
    m_tiefe = new QSpinBox(navigation);
    m_tiefe->setObjectName(QStringLiteral("tiefe"));
    m_tiefe->setRange(0, 99);
    m_tiefe->setValue(2);
    m_tiefe->setPrefix(QStringLiteral("Depth: "));
    m_tiefe->setSpecialValueText(QStringLiteral("Depth: all"));
    m_tiefe->setToolTip(QStringLiteral("Call graph: how many levels from the function (0 = all). "
                                       "A function at the edge that calls further shows \u2026"));
    m_hoechstens = new QSpinBox(navigation);
    m_hoechstens->setObjectName(QStringLiteral("hoechstens"));
    m_hoechstens->setRange(0, 100000);
    m_hoechstens->setSingleStep(50);
    m_hoechstens->setValue(300);
    m_hoechstens->setPrefix(QStringLiteral("Max: "));
    m_hoechstens->setSpecialValueText(QStringLiteral("Max: all"));
    m_hoechstens->setToolTip(QStringLiteral("Call graph: at most this many functions (0 = all). "
                                            "Who would call further shows \u2026; expand it by hand"));
    m_ohneFremde = new QCheckBox(QStringLiteral("Hide external calls"), navigation);
    m_ohneFremde->setObjectName(QStringLiteral("ohneFremde"));
    m_ohneFremde->setToolTip(QStringLiteral("Call graph: leave out calls without a body in the project "
                                            "(library, SDK, macro); the status bar counts them"));
    navigation->addWidget(m_aufrufRichtung);
    navigation->addWidget(m_tiefe);
    navigation->addWidget(m_hoechstens);
    navigation->addWidget(m_ohneFremde);
    const auto neuAbleiten = [this] {
        if (!istAufrufgraph()) return;
        leiteAufrufgraphAb();
        zeichne();
        zeigeLage();
    };
    connect(m_aufrufRichtung, &QComboBox::currentIndexChanged, this, neuAbleiten);
    connect(m_tiefe, &QSpinBox::valueChanged, this, neuAbleiten);
    connect(m_hoechstens, &QSpinBox::valueChanged, this, neuAbleiten);
    connect(m_ohneFremde, &QCheckBox::toggled, this, neuAbleiten);

    QMenu* diagramm = menuBar()->addMenu(QStringLiteral("&Diagram"));
    diagramm->addAction(m_zurueckAktion);
    diagramm->addAction(m_aufruferAktion);
    diagramm->addAction(m_aufrufgraphAktion);
    diagramm->addAction(QStringLiteral("&Remove all embeddings"), this, [this] {
        if (!loeseEinbettungen()) statusBar()->showMessage(QStringLiteral("Nothing is embedded"), 3000);
    });
    diagramm->addSeparator();
    diagramm->addAction(QStringLiteral("Show &sample"), this, [this] { zeigeBeispiel(); });
    diagramm->addAction(QStringLiteral("&Copy diagram text"), this, [this] {
        QApplication::clipboard()->setText(diagrammText());
        statusBar()->showMessage(QStringLiteral("Diagram text copied"), 3000);
    });

    // das Feld Configuration: rechts angedockt, ein- und ausblendbar im Menue Project
    auto* dock = new QDockWidget(QStringLiteral("Configuration"), this);
    dock->setObjectName(QStringLiteral("konfigurationDock"));
    m_konfiguration = new Konfiguration(dock);
    m_konfiguration->setObjectName(QStringLiteral("konfiguration"));
    dock->setWidget(m_konfiguration);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    projekt->addSeparator();
    QAction* zeigen = dock->toggleViewAction();
    zeigen->setText(QStringLiteral("&Configuration (#if names and branches)"));
    projekt->addAction(zeigen);
    m_konfiguration->beiAenderung = [this](const QString& text) { setzeDefinitionen(text); };
    m_konfiguration->herkunft = [this](const std::string& name) {
        if (!istProjekt() || m_datei >= m_projekt.dateien.size()) return c2d::projekt::Herkunft::keine;
        return c2d::projekt::herkunft(m_projekt.einstellungen, m_projekt.dateien[m_datei], name);
    };
    m_konfiguration->beimSpeichern = [this] {
        QString fehler;
        QStringList hinweise;
        if (!speichereKonfiguration(&fehler, &hinweise))
            QMessageBox::warning(this, windowTitle(), fehler);
        else if (!hinweise.isEmpty())
            QMessageBox::information(this, windowTitle(),
                                     QStringLiteral("Defines written to the project file.\n\n%1")
                                         .arg(hinweise.join(QLatin1Char('\n'))));
        else
            statusBar()->showMessage(QStringLiteral("Defines written to the project file"), 4000);
    };

    QMenu* hilfe = menuBar()->addMenu(QStringLiteral("&Help"));
    hilfe->addAction(QStringLiteral("&About"), this, [this] {
        QMessageBox::about(this, windowTitle(),
                           QStringLiteral("%1 %2\n\nReads source code and derives readable diagrams.")
                               .arg(ausStd(appName()), ausStd(appVersion())));
    });

    m_zuletzt = QSettings().value(QStringLiteral("zuletzt")).toStringList();
    statusBar()->showMessage(QStringLiteral("Ready"));
}

// ---------------------------------------------------------------------------
// Lesen: eine Datei, ein Ordner
// ---------------------------------------------------------------------------

bool Hauptfenster::oeffne(const QString& pfad, QString* fehler)
{
    if (QFileInfo(pfad).isDir()) return oeffneOrdner(pfad, fehler);
    QFile datei(pfad);
    if (!datei.open(QIODevice::ReadOnly))
    {
        if (fehler != nullptr)
            *fehler = QStringLiteral("Cannot read %1: %2").arg(pfad, datei.errorString());
        return false;
    }
    zeige(QString::fromUtf8(datei.readAll()), QFileInfo(pfad).fileName());
    merkeZuletzt(QFileInfo(pfad).absoluteFilePath());
    return true;
}

bool Hauptfenster::oeffneOrdner(const QString& pfad, QString* fehler)
{
    const QFileInfo info(pfad);
    if (!info.isDir())
    {
        if (fehler != nullptr) *fehler = QStringLiteral("Not a folder: %1").arg(pfad);
        return false;
    }
    m_ordner = info.absoluteFilePath();
    m_einzelText.clear();
    m_einzelName.clear();
    merkeZuletzt(m_ordner);
    liesEin();
    // zuerst eine Datei, die etwas zu zeichnen hat
    std::size_t erste = 0;
    for (std::size_t i = 0; i < m_projekt.dateien.size(); ++i)
    {
        const auto& fs = m_projekt.dateien[i].ergebnis.einheit.funktionen;
        if (std::any_of(fs.begin(), fs.end(), [](const c2d::modell::Funktion& f) { return f.rumpf != nullptr; }))
        {
            erste = i;
            break;
        }
    }
    zeigeDatei(erste);
    return true;
}

void Hauptfenster::zeige(const QString& quelltext, const QString& name)
{
    m_ordner.clear();
    m_einzelText = quelltext;
    m_einzelName = name;
    liesEin();
    zeigeDatei(0);
}

void Hauptfenster::zeigeBeispiel()
{
    zeige(ausStd(kBeispiel), QStringLiteral("ampel.c"));
}

QStringList Hauptfenster::zuletztGeoeffnet() const
{
    return m_zuletzt;
}

void Hauptfenster::merkeZuletzt(const QString& pfad)
{
    // gleich geschrieben ist gleich — unter Windows ohne Ruecksicht auf Gross und Klein
    const QString normal = QDir::cleanPath(pfad);
    m_zuletzt.removeIf([&](const QString& p) { return p.compare(normal, Qt::CaseInsensitive) == 0; });
    m_zuletzt.prepend(normal);
    while (m_zuletzt.size() > kZuletztHoechstens) m_zuletzt.removeLast();
    schreibeZuletzt();
}

void Hauptfenster::schreibeZuletzt() const
{
    QSettings().setValue(QStringLiteral("zuletzt"), m_zuletzt);
}

void Hauptfenster::leereZuletzt()
{
    m_zuletzt.clear();
    schreibeZuletzt();
}

bool Hauptfenster::oeffneZuletzt(int wahl, QString* fehler)
{
    if (wahl < 0 || wahl >= m_zuletzt.size())
    {
        if (fehler != nullptr) *fehler = QStringLiteral("No such entry in Open recent");
        return false;
    }
    const QString pfad = m_zuletzt[wahl];
    if (!QFileInfo::exists(pfad))
    {
        // was es nicht mehr gibt, kommt aus der Liste — nicht still: der Fehler sagt es
        m_zuletzt.removeAt(wahl);
        schreibeZuletzt();
        if (fehler != nullptr) *fehler = QStringLiteral("%1 no longer exists - removed from Open recent").arg(pfad);
        return false;
    }
    return QFileInfo(pfad).isDir() ? oeffneOrdner(pfad, fehler) : oeffne(pfad, fehler);
}

void Hauptfenster::fuelleZuletztMenue(QMenu* menue)
{
    if (m_zuletzt.isEmpty())
    {
        menue->addAction(QStringLiteral("(nothing opened yet)"))->setEnabled(false);
        return;
    }
    for (int i = 0; i < m_zuletzt.size(); ++i)
    {
        // ein Ordner mit Ordnersymbol und `/` am Ende; vorn eine Ziffer zum Waehlen mit der Tastatur
        const QFileInfo info(m_zuletzt[i]);
        const bool ordner = info.isDir();
        const QString text = QStringLiteral("&%1  %2%3")
                                 .arg(i < 9 ? QString::number(i + 1) : QStringLiteral("1&0"))
                                 .arg(QDir::toNativeSeparators(m_zuletzt[i]).replace(QLatin1Char('&'), QStringLiteral("&&")),
                                      ordner ? QStringLiteral("/") : QString());
        QAction* eintrag = menue->addAction(style()->standardIcon(ordner ? QStyle::SP_DirIcon : QStyle::SP_FileIcon), text,
                                            this, [this, i] {
                                                QString fehler;
                                                if (!oeffneZuletzt(i, &fehler)) QMessageBox::warning(this, windowTitle(), fehler);
                                            });
        if (!info.exists()) eintrag->setToolTip(QStringLiteral("No longer exists - choosing it removes it from the list"));
    }
    menue->setToolTipsVisible(true);
    menue->addSeparator();
    menue->addAction(QStringLiteral("&Clear list"), this, [this] { leereZuletzt(); });
}

void Hauptfenster::liesEin()
{
    c2d::projekt::ProjektOptionen optionen;
    optionen.alleZweige = m_alleZweige->isChecked();
    optionen.varianten = m_varianten->isChecked();
    optionen.bereiche = m_bereiche->isChecked();
    m_gelesenMit = m_definitionen->text();
    optionen.definitionen = definitionenImFeld();
    if (m_ordner.isEmpty())
        m_projekt = c2d::projekt::liesText(m_einzelText.toStdString(), m_einzelName.toStdString(), optionen);
    else
        m_projekt = c2d::projekt::liesOrdner(alsWeg(m_ordner), optionen);
    m_typen = c2d::projekt::Typverzeichnis(m_projekt);
    m_struktur = {};

    // was vorher galt, verweist nicht mehr sicher in dieses Projekt
    m_datei = kKeine;
    m_quelleZeigt = kKeine;
    m_quelleZeile = 0;
    m_gezeigt.clear();
    m_verlauf.clear();
    m_aufrufgraph = {};
    m_aufgeklappt.clear();
    m_projektdateiAktion->setEnabled(istProjekt());
    m_meldungenAktion->setEnabled(istProjekt());
    fuelleBaum();
}

std::vector<c2d::sprache::Definition> Hauptfenster::definitionenImFeld() const
{
    std::vector<c2d::sprache::Definition> aus;
    static const QRegularExpression kTrenner(QStringLiteral("[\\s,;]+"));
    for (const QString& stueck : m_definitionen->text().split(kTrenner, Qt::SkipEmptyParts))
    {
        c2d::sprache::Definition d = c2d::sprache::definitionAus(stueck.toStdString());
        if (!d.name.empty()) aus.push_back(std::move(d));
    }
    return aus;
}

void Hauptfenster::aktualisiereKonfiguration()
{
    if (m_datei >= m_projekt.dateien.size())
    {
        m_konfiguration->zeige(nullptr, definitionenImFeld(), {}, 0, 0, istProjekt());
        return;
    }
    // die Ketten der gezeigten Funktion — bei den Datentypen oder einem Automaten die der Datei
    int von = 0, bis = 0;
    if (const std::optional<Fundstelle> f = gezeigteFunktion(); f && f->datei == m_datei)
    {
        const c2d::modell::Funktion& funktion = c2d::projekt::funktion(m_projekt, *f);
        von = funktion.zeile;
        bis = funktion.bisZeile;
    }
    const c2d::projekt::Datei& datei = m_projekt.dateien[m_datei];
    m_konfiguration->zeige(&datei.ergebnis, definitionenImFeld(), datei.definitionen, von, bis, istProjekt());
}

bool Hauptfenster::speichereKonfiguration(QString* fehler, QStringList* hinweise)
{
    if (!istProjekt())
    {
        if (fehler != nullptr) *fehler = QStringLiteral("No folder is open");
        return false;
    }
    std::optional<c2d::projekt::Projektdatei> p = einstellungenZumAendern(fehler);
    if (!p) return false;
    // was im Feld steht, ersetzt gleichnamige Definitionen der Projektdatei
    std::vector<std::string> abweichend;
    const c2d::projekt::Projektdatei neu = c2d::projekt::mitDefinitionen(std::move(*p), definitionenImFeld(), abweichend);
    if (hinweise != nullptr)
        for (const std::string& h : abweichend) *hinweise << ausStd(h);
    return schreibeProjektdatei(neu, fehler);
}

void Hauptfenster::liesNeu()
{
    if (m_projekt.dateien.empty() && m_ordner.isEmpty()) return;
    const QString datei = this->datei();
    const QString gezeigt = m_gezeigt;
    const QString typ = m_typ;
    liesEin();
    const std::size_t index = m_projekt.finde(datei.toStdString());
    zeigeDatei(index == kKeine ? 0 : index);
    // das Diagramm von vorher wieder zeigen, wenn es das noch gibt — ohne Eintrag im Verlauf
    m_ohneVerlauf = true;
    waehleEintrag(gezeigt);
    setzeTypStill(typ);
    m_ohneVerlauf = false;
}

void Hauptfenster::zeigeDatei(std::size_t index)
{
    if (!m_ohneVerlauf && !m_gezeigt.isEmpty() && index != m_datei) m_verlauf << hier();
    if (index >= m_projekt.dateien.size())
    {
        // ein Ordner ohne Quelldateien
        m_datei = kKeine;
        m_quelleZeigt = kKeine;
        m_quelleZeile = 0;
        m_quelle->clear();
        {
            const QSignalBlocker ruhe(m_auswahl);
            m_auswahl->clear();
        }
        m_automaten.clear();
        m_automatNamen.clear();
        baueNeu();
        zeigeLage();
        return;
    }
    m_datei = index;
    const c2d::projekt::Datei& d = m_projekt.dateien[index];
    m_quelle->setPlainText(QString::fromUtf8(d.text));
    m_quelle->setExtraSelections({});
    m_quelleZeigt = index;
    m_quelleZeile = 0;

    {
        const QSignalBlocker ruhe(m_auswahl);
        m_auswahl->clear();
        const c2d::modell::Einheit& einheit = d.ergebnis.einheit;
        for (const c2d::modell::Funktion& f : einheit.funktionen)
        {
            // mit Bereich (`Klasse::methode`); von ueberladenen Funktionen steht die erste zur Wahl
            const QString fname = QString::fromStdString(f.vollerName());
            if (!f.rumpf || m_auswahl->findData(fname) >= 0) continue;
            m_auswahl->addItem(fname + QStringLiteral("()"), fname);
        }
        // erkannte Zustandsautomaten
        m_automaten = c2d::diagramm::automatenVon(einheit);
        m_automatNamen.clear();
        for (const c2d::diagramm::Zustandsdiagramm& z : m_automaten)
        {
            QString name = QString::fromLatin1(kAutomat) + QString::fromStdString(z.variable);
            if (m_automatNamen.contains(name)) name += QLatin1Char('@') + QString::fromStdString(z.funktion);
            m_automatNamen << name;
            m_auswahl->addItem(QStringLiteral("State machine: %1").arg(QString::fromStdString(z.titel)), name);
        }
        if (!einheit.typen.empty())
            m_auswahl->addItem(QStringLiteral("Data types (%1)").arg(einheit.typen.size()),
                               QString::fromLatin1(kDatentypen));
        if (std::any_of(einheit.funktionen.begin(), einheit.funktionen.end(),
                        [](const c2d::modell::Funktion& f) { return f.rumpf != nullptr; }))
            m_auswahl->addItem(QStringLiteral("Call graph of this file"), QString::fromLatin1(kAufrufgraphDatei));
        if (const std::size_t n = c2d::diagramm::nachschlagetabellenVon(einheit).size(); n > 0)  // nur die Zahl
            m_auswahl->addItem(QStringLiteral("Lookup tables (%1)").arg(n), QString::fromLatin1(kNachschlagetabellen));
        if (!d.ergebnis.weichen.empty())
            m_auswahl->addItem(QStringLiteral("#if configuration (%1)").arg(d.ergebnis.weichen.size()),
                               QString::fromLatin1(kKonfiguration));
    }
    {
        // die Typen der Datei, vorn alle; gewaehlt ist der erste
        const QSignalBlocker ruhe(m_typAuswahl);
        m_typAuswahl->clear();
        const c2d::modell::Einheit& einheit = d.ergebnis.einheit;
        m_typAuswahl->addItem(QStringLiteral("All types (%1)").arg(einheit.typen.size()), QStringLiteral("*"));
        for (const c2d::modell::Typ& t : einheit.typen)
        {
            const QString name = QString::fromStdString(t.vollerName());
            if (m_typAuswahl->findData(name) < 0) m_typAuswahl->addItem(name, name);
        }
        m_typAuswahl->setCurrentIndex(m_typAuswahl->count() > 1 ? 1 : 0);
        m_typ = m_typAuswahl->currentData().toString();
    }
    {
        // im Baum die Datei markieren, ohne dass er sie noch einmal waehlt
        const QSignalBlocker ruhe(m_baum);
        const QString pfad = ausStd(d.pfad);
        for (QTreeWidgetItemIterator it(m_baum); *it != nullptr; ++it)
            if ((*it)->data(0, kArt).toInt() == kEintragDatei && (*it)->data(0, kPfad).toString() == pfad)
            {
                m_baum->setCurrentItem(*it);
                break;
            }
    }
    baueNeu();
    zeigeLage();
}

void Hauptfenster::fuelleBaum()
{
    const QSignalBlocker ruhe(m_baum);
    m_baum->clear();
    m_baum->setVisible(istProjekt());
    if (!istProjekt()) return;

    std::map<QString, QTreeWidgetItem*> ordner;
    const auto ordnerFuer = [&](const QString& pfad, auto& selbst) -> QTreeWidgetItem* {
        if (pfad.isEmpty()) return m_baum->invisibleRootItem();
        if (const auto it = ordner.find(pfad); it != ordner.end()) return it->second;
        const int strich = pfad.lastIndexOf(QLatin1Char('/'));
        QTreeWidgetItem* oben = selbst(strich < 0 ? QString() : pfad.left(strich), selbst);
        auto* eintrag = new QTreeWidgetItem(oben, {letzterName(pfad)});
        eintrag->setData(0, kPfad, pfad);
        eintrag->setData(0, kArt, kEintragOrdner);
        ordner[pfad] = eintrag;
        return eintrag;
    };
    for (const c2d::projekt::Datei& d : m_projekt.dateien)
    {
        const QString pfad = ausStd(d.pfad);
        const int strich = pfad.lastIndexOf(QLatin1Char('/'));
        QTreeWidgetItem* oben = ordnerFuer(strich < 0 ? QString() : pfad.left(strich), ordnerFuer);
        auto* eintrag = new QTreeWidgetItem(oben, {letzterName(pfad)});
        eintrag->setData(0, kPfad, pfad);
        eintrag->setData(0, kArt, kEintragDatei);
        if (!d.ergebnis.meldungen.empty())
            eintrag->setToolTip(0, QStringLiteral("%1 message(s) from reading").arg(d.ergebnis.meldungen.size()));
    }
    // was uebergangen wurde, steht grau dabei — es laesst sich wieder einlesen
    for (const std::string& u : m_projekt.uebergangen)
    {
        const QString pfad = ausStd(u);
        const int strich = pfad.lastIndexOf(QLatin1Char('/'));
        QTreeWidgetItem* oben = ordnerFuer(strich < 0 ? QString() : pfad.left(strich), ordnerFuer);
        auto* eintrag = new QTreeWidgetItem(oben, {letzterName(pfad) + QStringLiteral("  (skipped)")});
        eintrag->setData(0, kPfad, pfad);
        eintrag->setData(0, kArt, kEintragUebergangen);
        eintrag->setForeground(0, palette().color(QPalette::Disabled, QPalette::Text));
    }
    if (m_projekt.dateien.size() <= 100) m_baum->expandAll();
    else m_baum->expandToDepth(0);
}

void Hauptfenster::zeigeLage()
{
    if (m_datei >= m_projekt.dateien.size())
    {
        setWindowTitle(QStringLiteral("%1 - %2").arg(letzterName(m_ordner), ausStd(appName())));
        statusBar()->showMessage(QStringLiteral("%1: no source files (%2 folder(s) skipped)")
                                     .arg(m_ordner)
                                     .arg(m_projekt.uebergangen.size()));
        return;
    }
    const c2d::projekt::Datei& d = m_projekt.dateien[m_datei];
    const QString name = istProjekt() ? ausStd(d.pfad) : m_einzelName;
    const bool ersatz = c2d::sprache::spracheFuer(letzterName(name).toStdString()) == nullptr;
    // die Funktionen der Auswahl: alles, was kein Automat, keine Datentypen und kein Aufrufgraph ist
    int funktionen = 0;
    for (const QString& eintrag : diagramme())
        if (!eintrag.startsWith(QLatin1String(kAutomat)) && eintrag != QLatin1String(kDatentypen) &&
            eintrag != QLatin1String(kAufrufgraphDatei) && !eintrag.startsWith(QLatin1String(kAufrufgraph)))
            ++funktionen;
    QString lage = QStringLiteral("%1, read as %2%3: %4 function(s), %5 data type(s)")
                       .arg(name, ausStd(d.sprache->name()),
                            ersatz ? QStringLiteral(" (no reader for this file type)") : QString())
                       .arg(funktionen)
                       .arg(d.ergebnis.einheit.typen.size());
    if (!m_automaten.empty()) lage += QStringLiteral(", %1 state machine(s)").arg(m_automaten.size());
    if (!d.ergebnis.meldungen.empty())
    {
        const c2d::quelle::Meldung& erste = d.ergebnis.meldungen.front();
        lage += QStringLiteral(" - %1 message(s), first in line %2: %3")
                    .arg(d.ergebnis.meldungen.size())
                    .arg(erste.ort.zeile)
                    .arg(QString::fromStdString(erste.text));
    }
    if (const QStringList offen = unbekannteNamen(); !offen.isEmpty())
    {
        // wonach ein #if gefragt hat: so sieht man, welche Variante gelesen wurde und was sich waehlen laesst
        lage += QStringLiteral(" - not defined in #if: %1%2")
                    .arg(offen.mid(0, 6).join(QStringLiteral(", ")), offen.size() > 6 ? QStringLiteral(", ...") : QString());
    }
    if (istAufrufgraph())
    {
        // der Aufrufgraph: wie gross, was mehrdeutig ist, was weggelassen wurde
        const c2d::diagramm::Aufrufgraph& g = m_aufrufgraph.graph;
        QString graph = QStringLiteral(" | call graph: %1 node(s), %2 file(s)").arg(g.knoten.size()).arg(g.dateien.size());
        if (!m_aufrufgraph.meldungen.empty())
            graph += QStringLiteral(", %1 ambiguous call(s), first %2:%3: %4")
                         .arg(m_aufrufgraph.meldungen.size())
                         .arg(ausStd(m_aufrufgraph.meldungen.front().pfad))
                         .arg(m_aufrufgraph.meldungen.front().meldung.ort.zeile)
                         .arg(QString::fromStdString(m_aufrufgraph.meldungen.front().meldung.text));
        if (m_aufrufgraph.ausgelassen != 0) graph += QStringLiteral(", %1 external call(s) hidden").arg(m_aufrufgraph.ausgelassen);
        if (m_aufrufgraph.begrenzt)
            graph += QStringLiteral(" - call graph stopped at %1 functions - lower the depth or expand by hand")
                         .arg(m_hoechstens->value());
        lage += graph;
    }
    if (istProjekt())
    {
        lage = QStringLiteral("%1: %2 file(s), %3 function(s), %4 folder(s) skipped, %5 project message(s) | %6")
                   .arg(letzterName(m_ordner))
                   .arg(m_projekt.dateien.size())
                   .arg(m_projekt.funktionen())
                   .arg(m_projekt.uebergangen.size())
                   .arg(m_projekt.meldungen.size())
                   .arg(lage);
        setWindowTitle(QStringLiteral("%1 / %2 - %3").arg(letzterName(m_ordner), name, ausStd(appName())));
    }
    else
        setWindowTitle(QStringLiteral("%1 - %2").arg(name, ausStd(appName())));
    statusBar()->showMessage(lage);
}

void Hauptfenster::setzeDefinitionen(const QString& definitionen)
{
    m_definitionen->setText(definitionen);
    liesNeu();
}

QString Hauptfenster::definitionen() const
{
    return m_definitionen->text();
}

void Hauptfenster::setzeAlleZweige(bool alle)
{
    if (m_alleZweige->isChecked() == alle) return;
    m_alleZweige->setChecked(alle);  // liest ueber das Signal neu
}

void Hauptfenster::setzeVarianten(bool zeigen)
{
    if (m_varianten->isChecked() == zeigen) return;
    m_varianten->setChecked(zeigen);  // liest ueber das Signal neu
}

void Hauptfenster::setzeBereiche(bool rahmen)
{
    if (m_bereiche->isChecked() == rahmen) return;
    m_bereiche->setChecked(rahmen);  // liest ueber das Signal neu
}

QStringList Hauptfenster::unbekannteNamen() const
{
    QStringList namen;
    if (m_datei >= m_projekt.dateien.size()) return namen;
    for (const std::string& name : m_projekt.dateien[m_datei].ergebnis.unbekannteNamen)
        namen << QString::fromStdString(name);
    return namen;
}

// ---------------------------------------------------------------------------
// Das Projekt: Dateien, Projektdatei, uebergangene Ordner
// ---------------------------------------------------------------------------

bool Hauptfenster::istProjekt() const
{
    return !m_ordner.isEmpty();
}

QStringList Hauptfenster::dateien() const
{
    QStringList pfade;
    for (const c2d::projekt::Datei& d : m_projekt.dateien) pfade << ausStd(d.pfad);
    return pfade;
}

QString Hauptfenster::datei() const
{
    return m_datei < m_projekt.dateien.size() ? ausStd(m_projekt.dateien[m_datei].pfad) : QString();
}

bool Hauptfenster::waehleDatei(const QString& pfad)
{
    const std::size_t index = m_projekt.finde(pfad.toStdString());
    if (index == kKeine) return false;
    if (index != m_datei) zeigeDatei(index);
    return true;
}

QStringList Hauptfenster::uebergangeneOrdner() const
{
    QStringList ordner;
    for (const std::string& u : m_projekt.uebergangen) ordner << ausStd(u);
    return ordner;
}

QStringList Hauptfenster::projektMeldungen() const
{
    QStringList zeilen;
    for (const c2d::projekt::Projektmeldung& m : m_projekt.meldungen)
    {
        QString ort = m.pfad.empty() ? m_ordner : ausStd(m.pfad);
        if (m.meldung.ort.zeile > 0) ort += QStringLiteral(":%1").arg(m.meldung.ort.zeile);
        zeilen << QStringLiteral("%1: %2").arg(ort, QString::fromStdString(m.meldung.text));
    }
    return zeilen;
}

QString Hauptfenster::projektdateiPfad() const
{
    return QDir(m_ordner).filePath(ausStd(c2d::projekt::kProjektdatei));
}

QString Hauptfenster::projektdateiText() const
{
    if (!istProjekt()) return {};
    QFile datei(projektdateiPfad());
    if (datei.open(QIODevice::ReadOnly)) return QString::fromUtf8(datei.readAll());
    // noch keine: ein Vorschlag, `uebergehen` aus der .gitignore
    c2d::projekt::Projektdatei vorschlag;
    QFile gitignore(QDir(m_ordner).filePath(QStringLiteral(".gitignore")));
    if (gitignore.open(QIODevice::ReadOnly))
        vorschlag.uebergehen = c2d::projekt::ausGitignore(gitignore.readAll().toStdString()).uebergehen;
    return ausStd(c2d::projekt::schreibe(vorschlag));
}

bool Hauptfenster::speichereProjektdatei(const QString& text, QString* fehler)
{
    if (!istProjekt())
    {
        if (fehler != nullptr) *fehler = QStringLiteral("No folder is open");
        return false;
    }
    std::vector<c2d::quelle::Meldung> meldungen;
    (void)c2d::projekt::liesProjektdatei(text.toStdString(), meldungen);
    if (!meldungen.empty())
    {
        if (fehler != nullptr)
        {
            QStringList zeilen;
            for (const c2d::quelle::Meldung& m : meldungen)
                zeilen << QStringLiteral("Line %1, column %2: %3")
                              .arg(m.ort.zeile)
                              .arg(m.ort.spalte)
                              .arg(QString::fromStdString(m.text));
            *fehler = zeilen.join(QLatin1Char('\n'));
        }
        return false;
    }
    QFile datei(projektdateiPfad());
    if (!datei.open(QIODevice::WriteOnly | QIODevice::Truncate) || datei.write(text.toUtf8()) < 0)
    {
        if (fehler != nullptr) *fehler = QStringLiteral("Cannot write %1: %2").arg(projektdateiPfad(), datei.errorString());
        return false;
    }
    datei.close();
    liesNeu();
    return true;
}

std::optional<c2d::projekt::Projektdatei> Hauptfenster::einstellungenZumAendern(QString* fehler) const
{
    std::vector<c2d::quelle::Meldung> meldungen;
    c2d::projekt::Projektdatei p = c2d::projekt::liesProjektdatei(projektdateiText().toStdString(), meldungen);
    if (meldungen.empty()) return p;
    // neu geschrieben ginge verloren, was nicht verstanden wurde
    if (fehler != nullptr)
        *fehler = QStringLiteral("The project file has %1 problem(s); fix it first (Project > Edit project file...)")
                      .arg(meldungen.size());
    return std::nullopt;
}

bool Hauptfenster::schreibeProjektdatei(const c2d::projekt::Projektdatei& datei, QString* fehler)
{
    return speichereProjektdatei(ausStd(c2d::projekt::schreibe(datei)), fehler);
}

bool Hauptfenster::uebergeheOrdner(const QString& ordner, QString* fehler)
{
    if (!istProjekt())
    {
        if (fehler != nullptr) *fehler = QStringLiteral("No folder is open");
        return false;
    }
    std::optional<c2d::projekt::Projektdatei> p = einstellungenZumAendern(fehler);
    if (!p) return false;
    const std::string eintrag = ordner.toStdString();
    if (std::find(p->uebergehen.begin(), p->uebergehen.end(), eintrag) == p->uebergehen.end())
        p->uebergehen.push_back(eintrag);
    return schreibeProjektdatei(*p, fehler);
}

bool Hauptfenster::liesOrdnerWieder(const QString& ordner, QString* fehler)
{
    std::optional<c2d::projekt::Projektdatei> p = istProjekt() ? einstellungenZumAendern(fehler) : std::nullopt;
    if (!p) return false;
    if (std::erase(p->uebergehen, ordner.toStdString()) == 0)
    {
        if (fehler != nullptr)
            *fehler = QStringLiteral("'%1' is not listed in \"uebergehen\" - it is always skipped, or by a pattern").arg(ordner);
        return false;
    }
    return schreibeProjektdatei(*p, fehler);
}

void Hauptfenster::bearbeiteProjektdatei()
{
    if (!istProjekt()) return;
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Project file"));
    dialog.resize(700, 520);
    const bool gibtEs = QFileInfo::exists(projektdateiPfad());
    QString hinweis = gibtEs ? QStringLiteral("%1").arg(projektdateiPfad())
                             : QStringLiteral("There is no project file yet. This is a suggestion for %1; "
                                              "\"uebergehen\" comes from the .gitignore of the folder.")
                                   .arg(projektdateiPfad());
    if (!gibtEs)
    {
        QFile gitignore(QDir(m_ordner).filePath(QStringLiteral(".gitignore")));
        if (gitignore.open(QIODevice::ReadOnly))
        {
            const c2d::projekt::Vorschlag v = c2d::projekt::ausGitignore(gitignore.readAll().toStdString());
            QStringList nicht;
            for (const std::string& z : v.nichtUebernommen) nicht << ausStd(z);
            if (!nicht.isEmpty())
                hinweis += QStringLiteral("\nNot taken from .gitignore (not a folder): %1").arg(nicht.join(QStringLiteral(", ")));
        }
    }
    auto* oben = new QLabel(hinweis, &dialog);
    oben->setWordWrap(true);
    auto* feld = new QPlainTextEdit(projektdateiText(), &dialog);
    feld->setObjectName(QStringLiteral("projektdatei"));
    feld->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    auto* fehlerZeile = new QLabel(&dialog);
    fehlerZeile->setWordWrap(true);
    fehlerZeile->setStyleSheet(QStringLiteral("color: #c0392b;"));
    fehlerZeile->hide();
    auto* knoepfe = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    auto* spalte = new QVBoxLayout(&dialog);
    spalte->addWidget(oben);
    spalte->addWidget(feld, 1);
    spalte->addWidget(fehlerZeile);
    spalte->addWidget(knoepfe);
    connect(knoepfe, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(knoepfe, &QDialogButtonBox::accepted, &dialog, [&] {
        QString fehler;
        if (speichereProjektdatei(feld->toPlainText(), &fehler))
        {
            dialog.accept();
            return;
        }
        fehlerZeile->setText(fehler);
        fehlerZeile->show();
    });
    dialog.exec();
}

void Hauptfenster::zeigeProjektMeldungen()
{
    const QStringList zeilen = projektMeldungen();
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Project messages"));
    box.setText(zeilen.isEmpty() ? QStringLiteral("No messages.")
                                 : QStringLiteral("%1 message(s) about the project - folders, project file, headers.")
                                       .arg(zeilen.size()));
    if (!zeilen.isEmpty()) box.setDetailedText(zeilen.join(QLatin1Char('\n')));
    box.exec();
}

void Hauptfenster::baumMenue(const QPoint& ort)
{
    QTreeWidgetItem* eintrag = m_baum->itemAt(ort);
    if (eintrag == nullptr) return;
    const QString pfad = eintrag->data(0, kPfad).toString();
    const int art = eintrag->data(0, kArt).toInt();
    QMenu menue(this);
    const auto melde = [this](bool gelungen, const QString& fehler) {
        if (!gelungen) QMessageBox::warning(this, windowTitle(), fehler);
    };
    if (art == kEintragOrdner)
        menue.addAction(QStringLiteral("Skip this folder (project file)"), this, [this, pfad, melde] {
            QString fehler;
            melde(uebergeheOrdner(pfad, &fehler), fehler);
        });
    else if (art == kEintragUebergangen)
    {
        const bool eingetragen = std::find(m_projekt.einstellungen.uebergehen.begin(), m_projekt.einstellungen.uebergehen.end(),
                                           pfad.toStdString()) != m_projekt.einstellungen.uebergehen.end();
        if (eingetragen)
            menue.addAction(QStringLiteral("Read this folder again (project file)"), this, [this, pfad, melde] {
                QString fehler;
                melde(liesOrdnerWieder(pfad, &fehler), fehler);
            });
        else if (c2d::projekt::wirdImmerUebergangen(letzterName(pfad).toStdString()))
            menue.addAction(QStringLiteral("Always skipped (starts with '.', or out, build, node_modules)"))->setEnabled(false);
        else
            menue.addAction(QStringLiteral("Skipped by a pattern - see Project > Edit project file..."))->setEnabled(false);
    }
    else
        return;
    menue.exec(m_baum->viewport()->mapToGlobal(ort));
}

// ---------------------------------------------------------------------------
// Diagramme: waehlen, springen, einbetten
// ---------------------------------------------------------------------------

QStringList Hauptfenster::diagramme() const
{
    QStringList namen;
    for (int i = 0; i < m_auswahl->count(); ++i) namen << m_auswahl->itemData(i).toString();
    return namen;
}

bool Hauptfenster::waehleDiagramm(const QString& name)
{
    const QString typen = QString::fromLatin1(kDatentypen) + QLatin1Char(':');
    if (name.startsWith(typen))
        return m_typAuswahl->findData(name.mid(typen.size())) >= 0 && waehleEintrag(QString::fromLatin1(kDatentypen)) &&
               waehleTyp(name.mid(typen.size()));
    return waehleEintrag(name);
}

Hauptfenster::Stelle Hauptfenster::hier(c2d::diagramm::KnotenId von) const
{
    return Stelle{m_datei, m_gezeigt, von, m_typ};
}

QStringList Hauptfenster::typen() const
{
    QStringList aus;
    for (int i = 0; i < m_typAuswahl->count(); ++i) aus << m_typAuswahl->itemData(i).toString();
    return aus;
}

bool Hauptfenster::waehleTyp(const QString& typ)
{
    if (m_typ == typ) return m_typAuswahl->findData(typ) >= 0;
    return waehle(m_typAuswahl, typ);  // zeichnet ueber das Signal, mit Eintrag im Verlauf
}

QString Hauptfenster::typ() const
{
    return m_gezeigt == QLatin1String(kDatentypen) ? m_typ : QString();
}

bool Hauptfenster::setzeTypStill(const QString& typ)
{
    if (typ.isEmpty() || m_typAuswahl->findData(typ) < 0) return false;
    const bool vorher = m_ohneVerlauf;
    m_ohneVerlauf = true;
    const bool gefunden = waehle(m_typAuswahl, typ);
    m_ohneVerlauf = vorher;
    return gefunden;
}

QStringList Hauptfenster::typZiele(const QString& name) const
{
    QStringList aus;
    for (const c2d::projekt::TypStelle& s : m_typen.kandidaten(m_datei, name.toStdString()))
        aus << QStringLiteral("%1:%2").arg(ausStd(m_projekt.dateien[s.datei].pfad)).arg(c2d::projekt::typ(m_projekt, s).zeile);
    return aus;
}

bool Hauptfenster::zeigeTyp(const QString& name, int wahl)
{
    const std::vector<c2d::projekt::TypStelle> k = m_typen.kandidaten(m_datei, name.toStdString());
    if (wahl < 0 && k.size() == 1) wahl = 0;
    if (wahl < 0 || static_cast<std::size_t>(wahl) >= k.size()) return false;
    geheZuTyp(k[static_cast<std::size_t>(wahl)]);
    return true;
}

QString Hauptfenster::beschreibe(const c2d::projekt::TypStelle& stelle) const
{
    const c2d::modell::Typ& t = c2d::projekt::typ(m_projekt, stelle);
    return QStringLiteral("%1 - %2:%3")
        .arg(QString::fromStdString(t.vollerName()), ausStd(m_projekt.dateien[stelle.datei].pfad))
        .arg(t.zeile);
}

std::vector<c2d::projekt::TypVerweis> Hauptfenster::typVerweiseVon(c2d::diagramm::KnotenId knoten) const
{
    if (knoten >= m_fluss.knoten().size()) return {};
    std::string text;
    for (const std::string& z : m_fluss.knoten()[knoten].zeilen) text += z + '\n';
    return m_typen.verweiseIn(dateiVon(knoten), text);
}

std::vector<c2d::projekt::TypVerweis> Hauptfenster::typVerweiseVon(std::size_t kasten, std::size_t eintrag) const
{
    if (kasten >= m_struktur.kaesten.size() || eintrag >= m_struktur.kaesten[kasten].eintraege.size()) return {};
    // die Kaesten stehen alle in der gewaehlten Datei; ein Glied nennt seinen Typ, nicht sich selbst
    return m_typen.verweiseIn(m_datei, m_struktur.kaesten[kasten].eintraege[eintrag].typ);
}

void Hauptfenster::geheZuTyp(const c2d::projekt::TypStelle& stelle, c2d::diagramm::KnotenId von)
{
    const QString name = QString::fromStdString(c2d::projekt::typ(m_projekt, stelle).vollerName());
    if (stelle.datei == m_datei && m_gezeigt == QLatin1String(kDatentypen) && m_typ == name) return;
    if (!m_gezeigt.isEmpty()) m_verlauf << hier(von);
    m_ohneVerlauf = true;
    if (stelle.datei != m_datei) zeigeDatei(stelle.datei);
    waehle(m_auswahl, QString::fromLatin1(kDatentypen));
    setzeTypStill(name);
    m_ohneVerlauf = false;
    aktualisiereLeiste();
}

void Hauptfenster::zielMenue(const std::vector<Fundstelle>& funktionen, const std::vector<c2d::projekt::TypVerweis>& typen,
                             c2d::diagramm::KnotenId von, QMenu& menue)
{
    if (!funktionen.empty()) menue.addSection(QStringLiteral("Functions"));
    for (const Fundstelle& f : funktionen)
        menue.addAction(QStringLiteral("Go to %1").arg(beschreibe(f)), this, [this, f, von] { gehe(f, von); });
    if (!typen.empty()) menue.addSection(QStringLiteral("Types"));
    for (const c2d::projekt::TypVerweis& t : typen)
        for (const c2d::projekt::TypStelle& s : t.ziele)
            menue.addAction(QStringLiteral("Go to type %1").arg(beschreibe(s)), this, [this, s, von] { geheZuTyp(s, von); });
}

void Hauptfenster::kastenDoppelklick(std::size_t kasten, std::size_t eintrag)
{
    if (kasten >= m_struktur.kaesten.size()) return;
    if (eintrag == kKeine)
    {
        // der Kopf: diesen Typ zeigen, mit seinen Nachbarn
        waehleTyp(QString::fromStdString(m_struktur.kaesten[kasten].name));
        return;
    }
    const std::vector<c2d::projekt::TypVerweis> typen = typVerweiseVon(kasten, eintrag);
    std::size_t ziele = 0;
    for (const c2d::projekt::TypVerweis& t : typen) ziele += t.ziele.size();
    if (ziele == 0) return;
    if (ziele == 1)
    {
        geheZuTyp(typen.front().ziele.front());
        return;
    }
    statusBar()->showMessage(QStringLiteral("%1 types qualify").arg(ziele), 5000);
    QMenu wahl(this);
    zielMenue({}, typen, kKeine, wahl);
    wahl.exec(QCursor::pos());
}

void Hauptfenster::kastenMenue(std::size_t kasten, std::size_t eintrag, const QPoint& ort)
{
    QMenu menue(this);
    if (kasten < m_struktur.kaesten.size())
    {
        const QString name = QString::fromStdString(m_struktur.kaesten[kasten].name);
        if (name != m_typ)
            menue.addAction(QStringLiteral("Show %1 and its neighbours").arg(name), this, [this, name] { waehleTyp(name); });
        if (eintrag != kKeine) zielMenue({}, typVerweiseVon(kasten, eintrag), kKeine, menue);
    }
    if (m_typ != QLatin1String("*"))
        menue.addAction(QStringLiteral("Show all types of the file"), this, [this] { waehleTyp(QStringLiteral("*")); });
    if (!m_verlauf.isEmpty())
    {
        menue.addSeparator();
        menue.addAction(QStringLiteral("Back to %1").arg(m_verlauf.last().diagramm), this, [this] { zurueck(); });
    }
    if (!menue.isEmpty()) menue.exec(ort);
}

bool Hauptfenster::waehleEintrag(const QString& name)
{
    if (waehle(m_auswahl, name)) return true;
    if (name.startsWith(QLatin1String(kZuordnung)) && m_datei < m_projekt.dateien.size())
    {
        // `mapping:<Funktion>@<Zeile>` — der switch dieser Zeile, wenn er nur Werte zuordnet
        const QString rest = name.mid(QString::fromLatin1(kZuordnung).size());
        const qsizetype at = rest.lastIndexOf(QLatin1Char('@'));
        const int zeile = at > 0 ? rest.mid(at + 1).toInt() : 0;
        if (zeile <= 0 || !hatZuordnung(m_datei, zeile)) return false;
        m_auswahl->addItem(QStringLiteral("Mapping table: %1() line %2").arg(rest.left(at)).arg(zeile), name);
        return waehle(m_auswahl, name);
    }
    if (name.startsWith(QLatin1String(kWahrheitstabelle)) && m_datei < m_projekt.dateien.size())
    {
        // `truth:<Funktion>@<Zeile>` — die Funktion dieser Datei, die Zeile mit einer Bedingung
        const QString rest = name.mid(QString::fromLatin1(kWahrheitstabelle).size());
        const qsizetype at = rest.lastIndexOf(QLatin1Char('@'));
        const int zeile = at > 0 ? rest.mid(at + 1).toInt() : 0;
        if (zeile <= 0 || !hatWahrheitstabelle(m_datei, zeile)) return false;
        const std::size_t f = funktionZurZeile(m_datei, zeile);
        if (f == kKeine || ausStd(m_projekt.dateien[m_datei].ergebnis.einheit.funktionen[f].vollerName()) != rest.left(at))
            return false;
        m_auswahl->addItem(QStringLiteral("Truth table: %1() line %2").arg(rest.left(at)).arg(zeile), name);
        return waehle(m_auswahl, name);
    }
    // ein Aufrufgraph ab einer Funktion der Datei steht erst da, wenn er gezeigt wird
    const QString praefix = QString::fromLatin1(kAufrufgraph);
    if (!name.startsWith(praefix) || m_datei >= m_projekt.dateien.size()) return false;
    const std::string funktion = name.mid(praefix.size()).toStdString();
    const auto& fs = m_projekt.dateien[m_datei].ergebnis.einheit.funktionen;
    if (std::none_of(fs.begin(), fs.end(),
                     [&](const c2d::modell::Funktion& f) { return f.rumpf && f.vollerName() == funktion; }))
        return false;
    m_auswahl->addItem(QStringLiteral("Call graph from %1()").arg(name.mid(praefix.size())), name);
    return waehle(m_auswahl, name);
}

bool Hauptfenster::istAufrufgraph() const
{
    return m_gezeigt == QLatin1String(kAufrufgraphDatei) || m_gezeigt.startsWith(QLatin1String(kAufrufgraph));
}

bool Hauptfenster::zeigeAufrufgraph(const QString& funktion)
{
    if (funktion.isEmpty()) return waehleEintrag(QString::fromLatin1(kAufrufgraphDatei));
    if (m_datei >= m_projekt.dateien.size()) return false;
    // der Name allein oder mit Bereich; im Eintrag steht der volle
    const std::string name = funktion.toStdString();
    for (const c2d::modell::Funktion& f : m_projekt.dateien[m_datei].ergebnis.einheit.funktionen)
        if (f.rumpf && (f.name == name || f.vollerName() == name))
            return waehleEintrag(QString::fromLatin1(kAufrufgraph) + QString::fromStdString(f.vollerName()));
    return false;
}

bool Hauptfenster::setzeAufrufRichtung(const QString& name)
{
    return waehle(m_aufrufRichtung, name);  // leitet ueber das Signal neu ab
}

void Hauptfenster::setzeTiefe(int tiefe)
{
    m_tiefe->setValue(tiefe);  // leitet ueber das Signal neu ab
}

int Hauptfenster::tiefe() const
{
    return m_tiefe->value();
}

void Hauptfenster::setzeHoechstens(int hoechstens)
{
    m_hoechstens->setValue(hoechstens);  // leitet ueber das Signal neu ab
}

void Hauptfenster::setzeOhneFremde(bool ohne)
{
    m_ohneFremde->setChecked(ohne);  // leitet ueber das Signal neu ab
}

bool Hauptfenster::klappeAuf(const QString& funktion)
{
    if (!istAufrufgraph()) return false;
    const std::string name = funktion.toStdString();
    const c2d::diagramm::Aufrufgraph& g = m_aufrufgraph.graph;
    for (std::size_t k = 0; k < g.knoten.size(); ++k)
    {
        const c2d::diagramm::AufrufKnoten& knoten = g.knoten[k];
        if (knoten.art != c2d::diagramm::AufrufKnotenArt::Funktion || knoten.name != name) continue;
        const Fundstelle stelle = m_aufrufgraph.stellen[k];
        if (std::none_of(m_aufgeklappt.begin(), m_aufgeklappt.end(), [&](const Fundstelle& a) { return a == stelle; }))
            m_aufgeklappt.push_back(stelle);
        leiteAufrufgraphAb();
        zeichne();
        zeigeLage();
        return true;
    }
    return false;
}

QStringList Hauptfenster::aufrufgraphKnoten() const
{
    QStringList aus;
    if (istAufrufgraph())
        for (const c2d::diagramm::AufrufKnoten& k : m_aufrufgraph.graph.knoten) aus << QString::fromStdString(knotenText(k));
    return aus;
}

void Hauptfenster::leiteAufrufgraphAb()
{
    m_aufrufgraph = {};
    if (!istAufrufgraph() || m_datei >= m_projekt.dateien.size()) return;
    c2d::projekt::AufrufgraphOptionen o;
    const QString richtung = m_aufrufRichtung->currentData().toString();
    o.richtung = richtung == QLatin1String("callers") ? c2d::projekt::AufrufRichtung::Aufrufer
                 : richtung == QLatin1String("both")  ? c2d::projekt::AufrufRichtung::Beide
                                                      : c2d::projekt::AufrufRichtung::Gerufene;
    o.tiefe = m_tiefe->value();
    o.hoechstens = static_cast<std::size_t>(m_hoechstens->value());
    o.ohneFremde = m_ohneFremde->isChecked();
    o.aufgeklappt = m_aufgeklappt;
    if (m_gezeigt == QLatin1String(kAufrufgraphDatei))
    {
        m_aufrufgraph = c2d::projekt::aufrufgraphDerDatei(m_projekt, m_datei, o);
        return;
    }
    const std::string name = m_gezeigt.mid(static_cast<qsizetype>(std::string_view(kAufrufgraph).size())).toStdString();
    const auto& fs = m_projekt.dateien[m_datei].ergebnis.einheit.funktionen;
    for (std::size_t f = 0; f < fs.size(); ++f)
        if (fs[f].rumpf && fs[f].vollerName() == name)
        {
            m_aufrufgraph = c2d::projekt::aufrufgraphAb(m_projekt, {m_datei, f}, o);
            return;
        }
}

void Hauptfenster::geheZuAufrufgraph(const Fundstelle& stelle, c2d::diagramm::KnotenId von)
{
    const QString name =
        QString::fromLatin1(kAufrufgraph) + QString::fromStdString(c2d::projekt::funktion(m_projekt, stelle).vollerName());
    if (stelle.datei == m_datei && name == m_gezeigt) return;
    if (!m_gezeigt.isEmpty()) m_verlauf << hier(von);
    m_ohneVerlauf = true;
    if (stelle.datei != m_datei) zeigeDatei(stelle.datei);
    waehleEintrag(name);
    m_ohneVerlauf = false;
    aktualisiereLeiste();
}

void Hauptfenster::aufrufgraphMenue(c2d::diagramm::KnotenId knoten, const QPoint& ort)
{
    QMenu menue(this);
    if (knoten < m_aufrufgraph.stellen.size())
    {
        const c2d::diagramm::AufrufKnoten& k = m_aufrufgraph.graph.knoten[knoten];
        const Fundstelle stelle = m_aufrufgraph.stellen[knoten];
        const QString name = QString::fromStdString(k.name);
        if (stelle.datei == kKeine)
            menue.addAction(k.art == c2d::diagramm::AufrufKnotenArt::Makro ? QStringLiteral("%1: a macro of the project").arg(name)
                                                                          : QStringLiteral("%1: no body in the project").arg(name))
                ->setEnabled(false);
        else
        {
            const QString wo = stelle.datei == m_datei ? QString()
                                                       : QStringLiteral(" in %1").arg(ausStd(m_projekt.dateien[stelle.datei].pfad));
            menue.addAction(QStringLiteral("Show the flow of %1()%2").arg(name, wo), this, [this, stelle, knoten] { gehe(stelle, knoten); });
            menue.addAction(QStringLiteral("Call graph from %1()%2").arg(name, wo), this,
                            [this, stelle, knoten] { geheZuAufrufgraph(stelle, knoten); })
                ->setEnabled(!k.wurzel);
            const bool auf = std::any_of(m_aufgeklappt.begin(), m_aufgeklappt.end(), [&](const Fundstelle& a) { return a == stelle; });
            if (k.weiter)
                menue.addAction(QStringLiteral("Expand one level"), this, [this, name] { klappeAuf(name); });
            if (auf)
                menue.addAction(QStringLiteral("Collapse"), this, [this, stelle] {
                    std::erase_if(m_aufgeklappt, [&](const Fundstelle& a) { return a == stelle; });
                    leiteAufrufgraphAb();
                    zeichne();
                    zeigeLage();
                });
            if (k.zeile > 0)
                menue.addAction(QStringLiteral("Show line %1 in the source%2").arg(k.zeile).arg(wo), this,
                                [this, stelle, zeile = k.zeile] { zeigeZeile(stelle.datei, zeile); });
        }
    }
    if (!m_verlauf.isEmpty())
    {
        if (!menue.isEmpty()) menue.addSeparator();
        menue.addAction(QStringLiteral("Back to %1").arg(m_verlauf.last().diagramm), this, [this] { zurueck(); });
    }
    if (!menue.isEmpty()) menue.exec(ort);
}

QString Hauptfenster::diagramm() const
{
    return m_gezeigt;
}

bool Hauptfenster::zurueck()
{
    if (m_verlauf.isEmpty()) return false;
    const Stelle davor = m_verlauf.takeLast();
    m_ohneVerlauf = true;
    if (davor.datei != m_datei) zeigeDatei(davor.datei);
    const bool gefunden = waehleEintrag(davor.diagramm);
    if (gefunden && davor.diagramm == QLatin1String(kDatentypen)) setzeTypStill(davor.typ);
    m_ohneVerlauf = false;
    // zurueck zum Aufruf, von dem man kam: er ist hervorgehoben
    if (gefunden && m_gezeigt == davor.diagramm) m_vorschau->markiere(davor.knoten);
    aktualisiereLeiste();
    return gefunden;
}

QString Hauptfenster::markiert() const
{
    const c2d::diagramm::KnotenId k = m_vorschau->markiert();
    if (istAufrufgraph())
        return k < m_aufrufgraph.graph.knoten.size() ? ausStd(m_aufrufgraph.graph.knoten[k].name) : QString();
    if (m_gezeigt == QLatin1String(kDatentypen))
        return k < m_struktur.kaesten.size() ? ausStd(m_struktur.kaesten[k].name) : QString();
    if (k >= m_fluss.knoten().size()) return {};
    QStringList zeilen;
    for (const std::string& z : m_fluss.knoten()[k].zeilen) zeilen << QString::fromStdString(z);
    return zeilen.join(QLatin1Char('\n'));
}

std::optional<Fundstelle> Hauptfenster::gezeigteFunktion() const
{
    if (m_datei >= m_projekt.dateien.size() || m_gezeigt.isEmpty() || m_gezeigt == QLatin1String(kDatentypen))
        return std::nullopt;
    const std::string name = m_gezeigt.toStdString();
    const auto& fs = m_projekt.dateien[m_datei].ergebnis.einheit.funktionen;
    for (std::size_t f = 0; f < fs.size(); ++f)
        if (fs[f].rumpf && fs[f].vollerName() == name) return Fundstelle{m_datei, f};
    return std::nullopt;
}

QStringList Hauptfenster::aufrufer() const
{
    QStringList aus;
    if (const std::optional<Fundstelle> ich = gezeigteFunktion())
        for (const Fundstelle& f : c2d::projekt::aufrufer(m_projekt, *ich)) aus << beschreibe(f);
    return aus;
}

bool Hauptfenster::springeZuAufrufer(int wahl)
{
    const std::optional<Fundstelle> ich = gezeigteFunktion();
    if (!ich || wahl < 0) return false;
    const std::vector<Fundstelle> wer = c2d::projekt::aufrufer(m_projekt, *ich);
    if (static_cast<std::size_t>(wahl) >= wer.size()) return false;
    const Fundstelle ziel = *ich;  // gehe() wechselt das gezeigte Diagramm
    gehe(wer[static_cast<std::size_t>(wahl)]);
    markiereAufrufVon(ziel);
    return true;
}

void Hauptfenster::markiereAufrufVon(const Fundstelle& ziel)
{
    for (c2d::diagramm::KnotenId i = 0; i < m_fluss.knoten().size(); ++i)
    {
        const std::vector<Fundstelle> z = zieleVon(i);
        if (std::find_if(z.begin(), z.end(), [&](const Fundstelle& f) { return f == ziel; }) != z.end())
        {
            m_vorschau->markiere(i);
            return;
        }
    }
}

void Hauptfenster::fuelleAufruferMenue(QMenu* menue)
{
    const std::optional<Fundstelle> ich = gezeigteFunktion();
    if (!ich)
    {
        menue->addAction(QStringLiteral("(no function shown)"))->setEnabled(false);
        return;
    }
    const std::vector<Fundstelle> wer = c2d::projekt::aufrufer(m_projekt, *ich);
    if (wer.empty())
    {
        menue->addAction(QStringLiteral("No caller in the project"))->setEnabled(false);
        return;
    }
    constexpr std::size_t kHoechstens = 40;  // mehr liest niemand in einem Menue
    for (std::size_t i = 0; i < wer.size() && i < kHoechstens; ++i)
        menue->addAction(beschreibe(wer[i]), this, [this, i] { springeZuAufrufer(static_cast<int>(i)); });
    if (wer.size() > kHoechstens)
        menue->addAction(QStringLiteral("... and %1 more").arg(wer.size() - kHoechstens))->setEnabled(false);
}

void Hauptfenster::aktualisiereLeiste()
{
    m_zurueckAktion->setEnabled(!m_verlauf.isEmpty());
    m_zurueckAktion->setToolTip(m_verlauf.isEmpty() ? QStringLiteral("Back (Alt+Left)")
                                                    : QStringLiteral("Back to %1 (Alt+Left)").arg(m_verlauf.last().diagramm));
    m_aufruferAktion->setEnabled(gezeigteFunktion().has_value());
}

bool Hauptfenster::waehleFormat(const QString& name)
{
    return waehle(m_format, name);
}

bool Hauptfenster::waehleDarstellung(const QString& name)
{
    return waehle(m_darstellung, name);
}

QString Hauptfenster::darstellung() const
{
    return m_darstellung->currentData().toString();
}

bool Hauptfenster::zeigtStruktogramm() const
{
    return !m_struktogramm.titel.empty();
}

bool Hauptfenster::zeigtEntscheidungstabelle() const
{
    return !m_entscheidung.funktion.empty();
}

bool Hauptfenster::ohneEinbetten() const
{
    return zeigtStruktogramm() || zeigtEntscheidungstabelle();
}

bool Hauptfenster::istWahrheitstabelle() const
{
    return m_gezeigt.startsWith(QLatin1String(kWahrheitstabelle));
}

std::size_t Hauptfenster::funktionZurZeile(std::size_t datei, int zeile) const
{
    if (datei >= m_projekt.dateien.size()) return kKeine;
    const auto& fs = m_projekt.dateien[datei].ergebnis.einheit.funktionen;
    std::size_t funktion = kKeine;
    for (std::size_t i = 0; i < fs.size(); ++i)
        if (fs[i].rumpf && fs[i].zeile <= zeile && zeile <= fs[i].bisZeile) funktion = i;
    return funktion;
}

bool Hauptfenster::hatWahrheitstabelle(std::size_t datei, int zeile) const
{
    const std::size_t f = funktionZurZeile(datei, zeile);
    if (f == kKeine) return false;
    const std::vector<int> zeilen =
        c2d::diagramm::zeilenMitWahrheitstabelle(m_projekt.dateien[datei].ergebnis.einheit.funktionen[f]);
    return std::find(zeilen.begin(), zeilen.end(), zeile) != zeilen.end();
}

std::vector<int> Hauptfenster::tabellenZeilen(c2d::diagramm::KnotenId knoten) const
{
    std::vector<int> aus;
    if (knoten >= m_fluss.knoten().size() || istAufrufgraph()) return aus;
    const c2d::diagramm::Knoten& k = m_fluss.knoten()[knoten];
    const std::size_t datei = dateiVon(knoten);
    std::vector<int> zeilen{k.quellZeile};
    zeilen.insert(zeilen.end(), k.weitereZeilen.begin(), k.weitereZeilen.end());
    for (const int z : zeilen)
        if (z > 0 && (hatWahrheitstabelle(datei, z) || hatZuordnung(datei, z)) && std::find(aus.begin(), aus.end(), z) == aus.end())
            aus.push_back(z);
    return aus;
}

QString Hauptfenster::tabellenHinweis(c2d::diagramm::KnotenId knoten) const
{
    const std::vector<int> zeilen = tabellenZeilen(knoten);
    if (zeilen.empty()) return {};
    // ein Klick zeigt die erste (wie geheZuTabelle); die weiteren stehen im Kontextmenue
    const QString art = hatWahrheitstabelle(dateiVon(knoten), zeilen.front()) ? QStringLiteral("Truth table")
                                                                              : QStringLiteral("Mapping table");
    return zeilen.size() == 1 ? art + QStringLiteral(" - click to show it")
                              : art + QStringLiteral(" of line %1 - click to show it; the others in the context menu")
                                          .arg(zeilen.front());
}

bool Hauptfenster::geheZuWahrheitstabelle(std::size_t datei, int zeile, c2d::diagramm::KnotenId von)
{
    if (!hatWahrheitstabelle(datei, zeile)) return false;
    const std::size_t f = funktionZurZeile(datei, zeile);
    const QString name = QString::fromLatin1(kWahrheitstabelle) +
                         ausStd(m_projekt.dateien[datei].ergebnis.einheit.funktionen[f].vollerName()) +
                         QStringLiteral("@%1").arg(zeile);
    if (datei == m_datei && name == m_gezeigt) return true;
    if (!m_gezeigt.isEmpty()) m_verlauf << hier(von);
    m_ohneVerlauf = true;
    if (datei != m_datei) zeigeDatei(datei);
    const bool da = waehleEintrag(name);
    m_ohneVerlauf = false;
    aktualisiereLeiste();
    return da;
}

bool Hauptfenster::hatZuordnung(std::size_t datei, int zeile) const
{
    const std::size_t f = funktionZurZeile(datei, zeile);
    if (f == kKeine) return false;
    const std::vector<int> zeilen = c2d::diagramm::zeilenMitZuordnung(m_projekt.dateien[datei].ergebnis.einheit.funktionen[f]);
    return std::find(zeilen.begin(), zeilen.end(), zeile) != zeilen.end();
}

bool Hauptfenster::istZuordnung() const
{
    return m_gezeigt.startsWith(QLatin1String(kZuordnung));
}

bool Hauptfenster::geheZuZuordnung(std::size_t datei, int zeile, c2d::diagramm::KnotenId von)
{
    if (!hatZuordnung(datei, zeile)) return false;
    const std::size_t f = funktionZurZeile(datei, zeile);
    const QString name = QString::fromLatin1(kZuordnung) + ausStd(m_projekt.dateien[datei].ergebnis.einheit.funktionen[f].vollerName()) +
                         QStringLiteral("@%1").arg(zeile);
    if (datei == m_datei && name == m_gezeigt) return true;
    if (!m_gezeigt.isEmpty()) m_verlauf << hier(von);
    m_ohneVerlauf = true;
    if (datei != m_datei) zeigeDatei(datei);
    const bool da = waehleEintrag(name);
    m_ohneVerlauf = false;
    aktualisiereLeiste();
    return da;
}

bool Hauptfenster::hatKonfiguration(std::size_t datei, int zeile) const
{
    if (datei >= m_projekt.dateien.size() || zeile <= 0) return false;
    for (const c2d::sprache::Weiche& w : m_projekt.dateien[datei].ergebnis.weichen)
        if (w.endZeile == zeile || std::find(w.zweigZeilen.begin(), w.zweigZeilen.end(), zeile) != w.zweigZeilen.end())
            return true;
    return false;
}

bool Hauptfenster::zeigeKonfiguration(int zeile)
{
    const std::size_t datei = m_quelleZeigt < m_projekt.dateien.size() ? m_quelleZeigt : m_datei;
    if (!hatKonfiguration(datei, zeile))
    {
        statusBar()->showMessage(QStringLiteral("Line %1 is not part of an #if chain").arg(zeile), 4000);
        return false;
    }
    if (datei != m_datei || m_gezeigt != QLatin1String(kKonfiguration))
    {
        if (!m_gezeigt.isEmpty()) m_verlauf << hier();
        m_ohneVerlauf = true;
        if (datei != m_datei) zeigeDatei(datei);
        const bool da = waehleEintrag(QString::fromLatin1(kKonfiguration));
        m_ohneVerlauf = false;
        aktualisiereLeiste();
        if (!da) return false;
    }
    // die erste Zeile der Kette, zu der die Zeile gehoert
    c2d::diagramm::KnotenId knoten = 0;
    for (const c2d::diagramm::Konfigurationstabelle& t : m_ketten)
    {
        const c2d::sprache::Weiche& w = m_projekt.dateien[m_datei].ergebnis.weichen[t.weiche];
        if (w.endZeile == zeile || std::find(w.zweigZeilen.begin(), w.zweigZeilen.end(), zeile) != w.zweigZeilen.end())
        {
            m_vorschau->markiere(knoten);
            return true;
        }
        knoten += t.zeilen.size();
    }
    return true;
}

std::optional<std::pair<std::size_t, std::size_t>> Hauptfenster::konfigurationsZeile(c2d::diagramm::KnotenId knoten) const
{
    if (m_gezeigt != QLatin1String(kKonfiguration)) return std::nullopt;
    for (std::size_t t = 0; t < m_ketten.size(); ++t)
    {
        if (knoten < m_ketten[t].zeilen.size()) return std::pair{t, static_cast<std::size_t>(knoten)};
        knoten -= m_ketten[t].zeilen.size();
    }
    return std::nullopt;
}

std::optional<std::vector<c2d::sprache::Definition>> Hauptfenster::zweigDefinitionen(c2d::diagramm::KnotenId knoten,
                                                                                    QString* warum) const
{
    const auto nein = [&](const QString& grund) {
        if (warum != nullptr) *warum = grund;
        return std::nullopt;
    };
    const std::optional<std::pair<std::size_t, std::size_t>> stelle = konfigurationsZeile(knoten);
    if (!stelle || m_datei >= m_projekt.dateien.size()) return nein(QStringLiteral("Not a row of an #if chain"));
    const c2d::diagramm::Konfigurationstabelle::Zeile& z = m_ketten[stelle->first].zeilen[stelle->second];
    if (z.geltend) return nein(QStringLiteral("This branch holds already"));
    const c2d::projekt::Datei& d = m_projekt.dateien[m_datei];
    const c2d::sprache::Weiche& w = d.ergebnis.weichen[m_ketten[stelle->first].weiche];
    if (z.zweigIndex >= w.wahl.size() || !w.wahl[z.zweigIndex])
        return nein(QStringLiteral("Cannot be chosen by names: the file defines a name itself, the condition is not "
                                   "made of names, or the conditions contradict each other"));
    std::optional<std::vector<c2d::sprache::Definition>> neu =
        c2d::sprache::mitForderungen(definitionenImFeld(), *w.wahl[z.zweigIndex], d.ergebnis.schalter, d.definitionen);
    if (!neu) return nein(QStringLiteral("Cannot be chosen: the file or the language defines a name itself"));
    return neu;
}

bool Hauptfenster::aktiviereZweig(c2d::diagramm::KnotenId knoten)
{
    QString warum;
    const std::optional<std::vector<c2d::sprache::Definition>> neu = zweigDefinitionen(knoten, &warum);
    if (!neu)
    {
        statusBar()->showMessage(warum, 4000);
        return false;
    }
    setzeDefinitionen(Konfiguration::alsText(*neu));
    return true;
}

bool Hauptfenster::aktiviereZweig(int zeile)
{
    for (c2d::diagramm::KnotenId k = 0; k < m_fluss.knoten().size(); ++k)
        if (konfigurationsZeile(k) && m_fluss.knoten()[k].quellZeile == zeile) return aktiviereZweig(k);
    return false;
}

bool Hauptfenster::geheZuTabelle(std::size_t datei, int zeile, c2d::diagramm::KnotenId von)
{
    return hatWahrheitstabelle(datei, zeile) ? geheZuWahrheitstabelle(datei, zeile, von) : geheZuZuordnung(datei, zeile, von);
}

bool Hauptfenster::zeigeZuordnung(int zeile)
{
    const std::size_t datei = m_quelleZeigt < m_projekt.dateien.size() ? m_quelleZeigt : m_datei;
    if (geheZuZuordnung(datei, zeile, kKeine)) return true;
    statusBar()->showMessage(QStringLiteral("Line %1 has no switch that only maps values").arg(zeile), 4000);
    return false;
}

bool Hauptfenster::zeigeWahrheitstabelle(int zeile)
{
    const std::size_t datei = m_quelleZeigt < m_projekt.dateien.size() ? m_quelleZeigt : m_datei;
    if (geheZuWahrheitstabelle(datei, zeile, kKeine)) return true;
    statusBar()->showMessage(QStringLiteral("Line %1 has no condition with two or more parts").arg(zeile), 4000);
    return false;
}

bool Hauptfenster::waehleAnsicht(const QString& name)
{
    if (name == QLatin1String("preview"))
        m_ansichten->setCurrentWidget(m_vorschau);
    else if (name == QLatin1String("text"))
        m_ansichten->setCurrentWidget(m_text);
    else
        return false;
    return true;
}

std::size_t Hauptfenster::dateiVon(c2d::diagramm::KnotenId knoten) const
{
    if (knoten < m_fluss.knoten().size())
    {
        // die Einbettung, nicht ein Rahmen einer Bedingung darin, weiss die Datei
        const std::size_t gruppe = m_fluss.einbettungVon(m_fluss.knoten()[knoten].gruppe);
        if (gruppe < m_fluss.gruppen().size() && !m_fluss.gruppen()[gruppe].herkunft.empty())
            if (const std::size_t d = m_projekt.finde(m_fluss.gruppen()[gruppe].herkunft); d != kKeine) return d;
    }
    return m_datei;
}

std::vector<Fundstelle> Hauptfenster::zieleVon(c2d::diagramm::KnotenId knoten) const
{
    std::vector<Fundstelle> aus;
    if (knoten >= m_fluss.knoten().size()) return aus;
    const std::size_t von = dateiVon(knoten);
    for (const std::string& aufruf : m_fluss.knoten()[knoten].aufrufe)
        for (const Fundstelle& f : c2d::projekt::kandidaten(m_projekt, von, aufruf))
            // find_if: std::find nimmt fuer diese Struktur einen vektorisierten Weg, den MSVC 14.51 nicht uebersetzt
            if (std::find_if(aus.begin(), aus.end(), [&](const Fundstelle& a) { return a == f; }) == aus.end())
                aus.push_back(f);
    return aus;
}

QString Hauptfenster::beschreibe(const Fundstelle& stelle) const
{
    const c2d::modell::Funktion& f = c2d::projekt::funktion(m_projekt, stelle);
    return QStringLiteral("%1() - %2:%3")
        .arg(QString::fromStdString(f.vollerName()), ausStd(m_projekt.dateien[stelle.datei].pfad))
        .arg(f.zeile);
}

QStringList Hauptfenster::ziele(const QString& aufruf) const
{
    QStringList aus;
    for (const Fundstelle& f : c2d::projekt::kandidaten(m_projekt, m_datei, aufruf.toStdString()))
        aus << QStringLiteral("%1:%2").arg(ausStd(m_projekt.dateien[f.datei].pfad)).arg(c2d::projekt::funktion(m_projekt, f).zeile);
    return aus;
}

bool Hauptfenster::springe(const QString& aufruf, int wahl)
{
    const std::string name = aufruf.toStdString();
    const std::vector<Fundstelle> k = c2d::projekt::kandidaten(m_projekt, m_datei, name);
    if (wahl < 0 && k.size() == 1) wahl = 0;
    if (wahl < 0 || static_cast<std::size_t>(wahl) >= k.size()) return false;
    // der Knoten, der so ruft: zu ihm fuehrt zurueck()
    c2d::diagramm::KnotenId von = c2d::diagramm::kKeine;
    for (c2d::diagramm::KnotenId i = 0; i < m_fluss.knoten().size() && von == c2d::diagramm::kKeine; ++i)
        for (const std::string& a : m_fluss.knoten()[i].aufrufe)
            if (a == name) von = i;
    gehe(k[static_cast<std::size_t>(wahl)], von);
    return true;
}

void Hauptfenster::gehe(const Fundstelle& stelle, c2d::diagramm::KnotenId von)
{
    const QString name = QString::fromStdString(c2d::projekt::funktion(m_projekt, stelle).vollerName());
    if (stelle.datei == m_datei && name == m_gezeigt) return;  // schon da (ein Aufruf seiner selbst)
    if (!m_gezeigt.isEmpty()) m_verlauf << hier(von);
    m_ohneVerlauf = true;
    if (stelle.datei != m_datei) zeigeDatei(stelle.datei);
    waehle(m_auswahl, name);
    m_ohneVerlauf = false;
    aktualisiereLeiste();
}

bool Hauptfenster::betteEin(c2d::diagramm::KnotenId knoten, const Fundstelle& gerufen)
{
    using namespace c2d::diagramm;
    if (ohneEinbetten()) return false;  // im Struktogramm und in der Entscheidungstabelle (noch) nicht
    const c2d::modell::Funktion& f = c2d::projekt::funktion(m_projekt, gerufen);
    Flussdiagramm neu = c2d::diagramm::betteEin(m_fluss, knoten, fasseAnweisungsfolgenZusammen(flussVon(f)), f.signatur(),
                                                m_projekt.dateien[gerufen.datei].pfad);
    if (neu.einbettungen() == m_fluss.einbettungen()) return false;
    m_fluss = std::move(neu);
    zeichne();
    return true;
}

bool Hauptfenster::betteEin(const QString& funktionName)
{
    const std::string name = funktionName.toStdString();
    for (c2d::diagramm::KnotenId i = 0; i < m_fluss.knoten().size(); ++i)
        for (const std::string& aufruf : m_fluss.knoten()[i].aufrufe)
        {
            std::vector<Fundstelle> gemeint = c2d::projekt::kandidaten(m_projekt, dateiVon(i), aufruf);
            std::erase_if(gemeint, [&](const Fundstelle& f) {
                const c2d::modell::Funktion& g = c2d::projekt::funktion(m_projekt, f);
                return g.name != name && g.vollerName() != name;
            });
            // eindeutig muss es sein — sonst waehlt man im Kontextmenue
            if (gemeint.size() == 1 && betteEin(i, gemeint.front())) return true;
        }
    return false;
}

bool Hauptfenster::loeseEinbettungen()
{
    if (m_fluss.einbettungen() == 0) return false;
    baueNeu();
    return true;
}

int Hauptfenster::einbettungen() const
{
    return static_cast<int>(m_fluss.einbettungen());
}

QString Hauptfenster::quelltext() const
{
    return m_quelle->toPlainText();
}

QString Hauptfenster::diagrammText() const
{
    return m_text->toPlainText();
}

int Hauptfenster::vorschauFormen() const
{
    return m_vorschau->formen();
}

QStringList Hauptfenster::tabellenSymbole() const
{
    return m_vorschau->tabellenSymbole();
}

QImage Hauptfenster::vorschauBild() const
{
    return m_vorschau->alsBild();
}

int Hauptfenster::meldungen() const
{
    return m_datei < m_projekt.dateien.size() ? static_cast<int>(m_projekt.dateien[m_datei].ergebnis.meldungen.size()) : 0;
}

QString Hauptfenster::dateiFilter()
{
    QStringList filter;
    for (const c2d::sprache::Sprache* s : c2d::sprache::sprachen())
    {
        QStringList muster;
        for (const std::string_view endung : s->endungen()) muster << QStringLiteral("*.") + ausStd(endung);
        filter << QStringLiteral("%1 files (%2)").arg(ausStd(s->name()), muster.join(QLatin1Char(' ')));
    }
    filter << QStringLiteral("All files (*)");
    return filter.join(QStringLiteral(";;"));
}

void Hauptfenster::baueNeu()
{
    using namespace c2d::diagramm;
    const QString bisher = m_gezeigt;
    m_gezeigt = m_auswahl->currentData().toString();
    m_fluss = Flussdiagramm();
    m_struktogramm = Struktogramm();
    m_entscheidung = Entscheidungstabelle();
    m_tabellen.clear();
    m_zuordnungen.clear();
    m_ketten.clear();
    // Zuordnung und Nachschlagetabellen: je Zeile ein Knoten (Klick, Show in diagram)
    const auto zeilenAlsKnoten = [&] {
        for (const Zuordnung& t : m_zuordnungen)
            for (const Zuordnung::Zeile& z : t.zeilen)
            {
                Knoten k;
                k.zeilen = {z.schluessel};
                k.quellZeile = z.quellZeile;
                m_fluss.neuerKnoten(std::move(k));
            }
    };
    if (istZuordnung() && m_datei < m_projekt.dateien.size())
    {
        const int zeile = m_gezeigt.mid(m_gezeigt.lastIndexOf(QLatin1Char('@')) + 1).toInt();
        if (const std::size_t f = funktionZurZeile(m_datei, zeile); f != kKeine)
            if (auto t = zuordnungVon(m_projekt.dateien[m_datei].ergebnis.einheit.funktionen[f], zeile))
                m_zuordnungen.push_back(std::move(*t));
        zeilenAlsKnoten();
    }
    if (m_gezeigt == QLatin1String(kKonfiguration) && m_datei < m_projekt.dateien.size())
    {
        // je Zeile ein Knoten (Klick, Show in diagram, Make this branch active)
        const c2d::sprache::Ergebnis& e = m_projekt.dateien[m_datei].ergebnis;
        m_ketten = konfigurationVon(e.weichen, e.schalter);
        for (const Konfigurationstabelle& t : m_ketten)
            for (const Konfigurationstabelle::Zeile& z : t.zeilen)
            {
                Knoten k;
                k.zeilen = {z.zweig};
                k.quellZeile = z.quellZeile;
                m_fluss.neuerKnoten(std::move(k));
            }
    }
    if (m_gezeigt == QLatin1String(kNachschlagetabellen) && m_datei < m_projekt.dateien.size())
    {
        m_zuordnungen = nachschlagetabellenVon(m_projekt.dateien[m_datei].ergebnis.einheit,
                                               [this](std::string_view name) { return m_typen.eindeutig(m_datei, name); });
        zeilenAlsKnoten();
    }
    m_richtung->setEnabled(true);
    if (istWahrheitstabelle() && m_datei < m_projekt.dateien.size())
    {
        const qsizetype at = m_gezeigt.lastIndexOf(QLatin1Char('@'));
        const int zeile = m_gezeigt.mid(at + 1).toInt();
        if (const std::size_t f = funktionZurZeile(m_datei, zeile); f != kKeine)
            if (auto t = wahrheitstabelleVon(m_projekt.dateien[m_datei].ergebnis.einheit.funktionen[f], zeile))
                m_tabellen.push_back(std::move(*t));
        m_richtung->setEnabled(false);
    }
    // ein anderer Aufrufgraph beginnt zugeklappt
    if (m_gezeigt != bisher) m_aufgeklappt.clear();
    m_typAuswahl->setVisible(m_gezeigt == QLatin1String(kDatentypen));
    leiteAufrufgraphAb();
    if (!m_gezeigt.isEmpty() && m_gezeigt != QLatin1String(kDatentypen) && m_datei < m_projekt.dateien.size())
    {
        const std::string name = m_gezeigt.toStdString();
        for (const c2d::modell::Funktion& f : m_projekt.dateien[m_datei].ergebnis.einheit.funktionen)
        {
            if (!f.rumpf || f.vollerName() != name) continue;
            if (darstellung() == QLatin1String(kEntscheidungstabelle))
            {
                m_entscheidung = entscheidungstabelleVon(f);
                // je Zeile ein Knoten (erst die Bedingungen, dann die Aktionen): Klick, Show in diagram, Symbol
                const auto knoten = [&](const std::string& text, int zeile) {
                    Knoten k;
                    k.zeilen = {text};
                    k.quellZeile = zeile;
                    m_fluss.neuerKnoten(std::move(k));
                };
                if (!m_entscheidung.regeln.empty())
                {
                    for (const Entscheidungstabelle::Bedingung& b : m_entscheidung.bedingungen) knoten(b.text, b.quellZeile);
                    for (const Entscheidungstabelle::Aktion& a : m_entscheidung.aktionen) knoten(a.text, a.quellZeile);
                }
                for (const std::string& h : m_entscheidung.hinweise) statusBar()->showMessage(ausStd(h), 8000);
                m_richtung->setEnabled(false);
                break;
            }
            if (darstellung() == QLatin1String(kStruktogramm))
            {
                m_struktogramm = struktogrammVon(f);
                // je Block ein Knoten: Text, Zeile, Aufrufe — die Wege der Knoten gelten so auch hier
                for (const Block* b : bloeckeVon(m_struktogramm))
                {
                    Knoten k;
                    k.zeilen = {b->text};
                    k.quellZeile = b->quellZeile;
                    k.aufrufe = b->aufrufe;
                    k.unerreichbar = b->unerreichbar;
                    m_fluss.neuerKnoten(std::move(k));
                }
                for (const Hinweis& h : m_struktogramm.hinweise)
                    statusBar()->showMessage(QStringLiteral("Line %1: %2").arg(h.zeile).arg(ausStd(h.text)), 8000);
                m_richtung->setEnabled(false);  // ein Struktogramm waechst nach unten
                break;
            }
            m_fluss = fasseAnweisungsfolgenZusammen(flussVon(f));
            // nichts still: Code, der nie erreicht wird, steht unten — und hier mit seiner Zeile
            if (const int ab = c2d::diagramm::unerreichbarAb(m_fluss); ab > 0)
                statusBar()->showMessage(
                    QStringLiteral("%1(): code from line %2 on is never reached").arg(ausStd(f.name)).arg(ab), 8000);
            break;
        }
    }
    zeichne();
    aktualisiereLeiste();
    aktualisiereKonfiguration();
    if (istAufrufgraph()) zeigeLage();
}

void Hauptfenster::zeichne()
{
    using namespace c2d::diagramm;

    const c2d::ausgabe::Format* format = c2d::ausgabe::format(m_format->currentData().toString().toStdString());
    if (m_gezeigt.isEmpty() || format == nullptr || m_datei >= m_projekt.dateien.size())
    {
        m_text->clear();
        m_vorschau->leere();
        return;
    }
    const std::string richtung = m_richtung->currentData().toString().toStdString();
    const c2d::ausgabe::Optionen optionen{.richtung = richtung};
    const bool nachRechts = richtung == "LR";
    // der Text im gewaehlten Format — kann es die Art nicht, sagt er es (und gespeichert wird nichts)
    const auto alsText = [&](c2d::ausgabe::Diagrammart art, const auto& diagramm) {
        m_textVerfuegbar = format->kann(art);
        if (!m_textVerfuegbar) return QString::fromStdString(c2d::ausgabe::nichtVerfuegbar(*format, art));
        return QString::fromStdString(format->schreibe(diagramm, optionen));
    };

    if (m_gezeigt == QLatin1String(kKonfiguration))
    {
        using c2d::diagramm::Konfigurationstabelle;
        m_vorschau->zeige(std::span<const Konfigurationstabelle>(m_ketten));
        m_text->setPlainText(
            alsText(c2d::ausgabe::Diagrammart::Konfiguration, std::span<const Konfigurationstabelle>(m_ketten)));
        return;
    }
    if (istZuordnung() || m_gezeigt == QLatin1String(kNachschlagetabellen))
    {
        m_vorschau->zeige(std::span<const Zuordnung>(m_zuordnungen));
        m_text->setPlainText(alsText(c2d::ausgabe::Diagrammart::Zuordnung, std::span<const Zuordnung>(m_zuordnungen)));
        return;
    }
    if (istWahrheitstabelle())
    {
        m_vorschau->zeige(std::span<const Wahrheitstabelle>(m_tabellen));
        m_text->setPlainText(alsText(c2d::ausgabe::Diagrammart::Wahrheitstabelle, std::span<const Wahrheitstabelle>(m_tabellen)));
        return;
    }
    if (m_gezeigt == QLatin1String(kDatentypen))
    {
        const c2d::modell::Einheit& einheit = m_projekt.dateien[m_datei].ergebnis.einheit;
        m_struktur = m_typ == QLatin1String("*") || m_typ.isEmpty() ? strukturVon(einheit)
                                                                     : strukturVon(einheit, m_typ.toStdString());
        // je Glied die Namen, die zu einem Typ fuehren
        std::vector<std::vector<std::vector<std::string>>> links(m_struktur.kaesten.size());
        for (std::size_t k = 0; k < m_struktur.kaesten.size(); ++k)
            for (std::size_t e = 0; e < m_struktur.kaesten[k].eintraege.size(); ++e)
            {
                links[k].emplace_back();
                for (const c2d::projekt::TypVerweis& v : typVerweiseVon(k, e)) links[k].back().push_back(v.name);
            }
        m_vorschau->zeige(m_struktur, nachRechts, links);
        m_text->setPlainText(alsText(c2d::ausgabe::Diagrammart::Datentypen, m_struktur));
        return;
    }
    if (istAufrufgraph())
    {
        m_vorschau->zeige(m_aufrufgraph.graph, nachRechts);
        m_text->setPlainText(alsText(c2d::ausgabe::Diagrammart::Aufrufgraph, m_aufrufgraph.graph));
        return;
    }
    if (const qsizetype automat = m_automatNamen.indexOf(m_gezeigt); automat >= 0)
    {
        const Zustandsdiagramm& z = m_automaten[static_cast<std::size_t>(automat)];
        m_vorschau->zeige(z, nachRechts);
        m_text->setPlainText(alsText(c2d::ausgabe::Diagrammart::Zustandsautomat, z));
        return;
    }
    // hervorgehoben wird, was weiterfuehrt: ein Aufruf, der eine Funktion des Projekts meint, ein Typ
    std::vector<std::vector<std::string>> links(m_fluss.knoten().size());
    for (KnotenId i = 0; i < links.size(); ++i)
    {
        const std::size_t von = dateiVon(i);
        for (const std::string& aufruf : m_fluss.knoten()[i].aufrufe)
            if (!c2d::projekt::kandidaten(m_projekt, von, aufruf).empty()) links[i].push_back(aufruf);
        for (const c2d::projekt::TypVerweis& t : typVerweiseVon(i)) links[i].push_back(t.name);
    }
    // ein Knoten (Block), in dessen Zeilen eine Bedingung mit Tabelle steht, traegt ihr Symbol
    std::vector<QString> tabellen(m_fluss.knoten().size());
    for (KnotenId i = 0; i < tabellen.size(); ++i) tabellen[i] = tabellenHinweis(i);
    if (zeigtEntscheidungstabelle())
    {
        m_vorschau->zeige(m_entscheidung, tabellen);
        m_text->setPlainText(alsText(c2d::ausgabe::Diagrammart::Entscheidungstabelle, m_entscheidung));
        return;
    }
    if (zeigtStruktogramm())
    {
        m_vorschau->zeige(m_struktogramm, links, tabellen);
        m_text->setPlainText(alsText(c2d::ausgabe::Diagrammart::Struktogramm, m_struktogramm));
        return;
    }
    m_vorschau->zeige(m_fluss, nachRechts, links, tabellen);
    m_text->setPlainText(alsText(c2d::ausgabe::Diagrammart::Ablauf, m_fluss));
}

void Hauptfenster::kontextMenue(c2d::diagramm::KnotenId knoten, std::size_t gruppe, const QPoint& ort)
{
    using namespace c2d::diagramm;
    QMenu menue(this);

    if (knoten < m_fluss.knoten().size())
    {
        const Knoten& k = m_fluss.knoten()[knoten];
        const bool traegtSchon = std::any_of(m_fluss.gruppen().begin(), m_fluss.gruppen().end(),
                                             [&](const Gruppe& g) { return g.aufrufer == knoten; });
        const std::size_t von = dateiVon(knoten);
        if (konfigurationsZeile(knoten))
        {
            // wie die Radioknoepfe im Feld Configuration: die Namen im Feld Defines setzen
            QString warum;
            const bool geht = zweigDefinitionen(knoten, &warum).has_value();
            QAction* aktiv = menue.addAction(QStringLiteral("Make this branch active"), this, [this, knoten] { aktiviereZweig(knoten); });
            aktiv->setEnabled(geht);
            aktiv->setToolTip(geht ? QStringLiteral("Sets the names in Defines so that this branch holds") : warum);
            menue.setToolTipsVisible(true);
        }
        for (const std::string& aufruf : k.aufrufe)
        {
            const std::vector<Fundstelle> gemeint = c2d::projekt::kandidaten(m_projekt, von, aufruf);
            if (gemeint.size() == 1)
            {
                const Fundstelle f = gemeint.front();
                const QString voll = QString::fromStdString(c2d::projekt::funktion(m_projekt, f).vollerName());
                const QString wo = f.datei == m_datei ? QString() : QStringLiteral(" in %1").arg(ausStd(m_projekt.dateien[f.datei].pfad));
                menue.addAction(QStringLiteral("Go to %1()%2").arg(voll, wo), this, [this, f, knoten] { gehe(f, knoten); });
                // im Struktogramm gibt es (noch) kein Einbetten (Entscheid Patrik, 2026-10-17, G)
                if (!ohneEinbetten())
                    menue.addAction(QStringLiteral("Embed %1()%2 here").arg(voll, wo), this, [this, knoten, f] { betteEin(knoten, f); })
                        ->setEnabled(!traegtSchon);
                menue.addAction(QStringLiteral("Call graph from %1()%2").arg(voll, wo), this,
                                [this, f, knoten] { geheZuAufrufgraph(f, knoten); });
            }
            else if (gemeint.size() > 1)
            {
                // mehrdeutig: alle zur Wahl, mit Pfad und Zeile
                QMenu* wahl = menue.addMenu(QStringLiteral("%1(): %2 functions qualify").arg(ausStd(aufruf)).arg(gemeint.size()));
                for (const Fundstelle& f : gemeint)
                    wahl->addAction(QStringLiteral("Go to %1").arg(beschreibe(f)), this, [this, f, knoten] { gehe(f, knoten); });
                if (!ohneEinbetten())
                {
                    wahl->addSeparator();
                    for (const Fundstelle& f : gemeint)
                        wahl->addAction(QStringLiteral("Embed here: %1").arg(beschreibe(f)), this,
                                        [this, knoten, f] { betteEin(knoten, f); })
                            ->setEnabled(!traegtSchon);
                }
            }
        }
        // die Bedingungen des Knotens mit Wahrheitstabelle
        for (const int zeile : tabellenZeilen(knoten))
            menue.addAction((hatWahrheitstabelle(dateiVon(knoten), zeile) ? QStringLiteral("Truth table (line %1)")
                                                                          : QStringLiteral("Mapping table (line %1)"))
                                .arg(zeile),
                            this, [this, knoten, zeile] { geheZuTabelle(dateiVon(knoten), zeile, knoten); });
        // die Typen, die der Knoten nennt
        for (const c2d::projekt::TypVerweis& t : typVerweiseVon(knoten))
        {
            if (t.ziele.size() == 1)
            {
                const c2d::projekt::TypStelle s = t.ziele.front();
                menue.addAction(QStringLiteral("Go to type %1").arg(beschreibe(s)), this, [this, s, knoten] { geheZuTyp(s, knoten); });
                continue;
            }
            QMenu* wahl = menue.addMenu(QStringLiteral("%1: %2 types qualify").arg(ausStd(t.name)).arg(t.ziele.size()));
            for (const c2d::projekt::TypStelle& s : t.ziele)
                wahl->addAction(QStringLiteral("Go to type %1").arg(beschreibe(s)), this, [this, s, knoten] { geheZuTyp(s, knoten); });
        }
        if (k.quellZeile > 0 && von < m_projekt.dateien.size())
        {
            const QString wo = von == m_datei ? QString() : QStringLiteral(" (%1)").arg(ausStd(m_projekt.dateien[von].pfad));
            menue.addAction(QStringLiteral("Show line %1 in the source%2").arg(k.quellZeile).arg(wo), this,
                            [this, von, zeile = k.quellZeile] { zeigeZeile(von, zeile); });
        }
    }
    // ein Rahmen einer Bedingung laesst sich nicht herausnehmen — die Einbettung um ihn schon
    if (const std::size_t einbettung = m_fluss.einbettungVon(gruppe); einbettung < m_fluss.gruppen().size())
    {
        if (!menue.isEmpty()) menue.addSeparator();
        menue.addAction(QStringLiteral("Remove embedding: %1").arg(QString::fromStdString(m_fluss.gruppen()[einbettung].titel)),
                        this, [this, einbettung] {
                            m_fluss = c2d::diagramm::loeseEin(m_fluss, einbettung);
                            zeichne();
                        });
    }
    if (m_fluss.einbettungen() > 0)
        menue.addAction(QStringLiteral("Remove all embeddings"), this, [this] { loeseEinbettungen(); });
    if (!m_verlauf.isEmpty() || gezeigteFunktion())
        if (!menue.isEmpty()) menue.addSeparator();
    if (!m_verlauf.isEmpty())
    {
        const Stelle& davor = m_verlauf.last();
        const QString wo = davor.datei == m_datei || davor.datei >= m_projekt.dateien.size()
                               ? QString()
                               : QStringLiteral(" (%1)").arg(ausStd(m_projekt.dateien[davor.datei].pfad));
        menue.addAction(QStringLiteral("Back to %1%2").arg(davor.diagramm, wo), this, [this] { zurueck(); });
    }
    if (const std::optional<Fundstelle> ich = gezeigteFunktion())
    {
        // wer ruft die gezeigte Funktion? erst beim Oeffnen gesucht
        QMenu* von = menue.addMenu(QStringLiteral("Called from"));
        connect(von, &QMenu::aboutToShow, this, [this, von] {
            von->clear();
            fuelleAufruferMenue(von);
        });
        menue.addAction(QStringLiteral("Call graph from here"), this, [this, stelle = *ich] { geheZuAufrufgraph(stelle); });
    }
    if (!menue.isEmpty()) menue.exec(ort);
}

bool Hauptfenster::zeigeZeile(std::size_t datei, int zeile)
{
    if (datei >= m_projekt.dateien.size() || zeile <= 0) return false;
    if (datei != m_quelleZeigt)
    {
        // eine Zeile einer anderen Datei (eingebettet, im Aufrufgraph, ein Makro): deren Text zeigen
        m_quelle->setPlainText(QString::fromUtf8(m_projekt.dateien[datei].text));
        m_quelleZeigt = datei;
        m_quelleZeile = 0;
    }
    const QTextBlock block = m_quelle->document()->findBlockByNumber(zeile - 1);
    if (!block.isValid()) return false;
    QTextCursor zeiger(block);
    m_quelle->setTextCursor(zeiger);
    m_quelle->centerCursor();
    // die ganze Zeile hinterlegen, auch wo sie leer ist
    QTextEdit::ExtraSelection marke;
    marke.cursor = zeiger;
    marke.format.setBackground(zeilenFarbe());
    marke.format.setProperty(QTextFormat::FullWidthSelection, true);
    m_quelle->setExtraSelections({marke});
    m_quelleZeile = zeile;
    return true;
}

QColor Hauptfenster::zeilenFarbe() const
{
    QColor farbe = palette().color(QPalette::Highlight);
    farbe.setAlpha(90);
    return farbe;
}

EinstellungenDialog* Hauptfenster::zeigeEinstellungen()
{
    if (m_einstellungen.isNull())
    {
        m_einstellungen = new EinstellungenDialog(this);
        m_einstellungen->setAttribute(Qt::WA_DeleteOnClose);
        m_einstellungen->open();
    }
    m_einstellungen->raise();
    m_einstellungen->activateWindow();
    return m_einstellungen.data();
}

void Hauptfenster::changeEvent(QEvent* ereignis)
{
    QMainWindow::changeEvent(ereignis);
    // ein Wechsel des Themas schickt mehrere Ereignisse: einmal neu zeichnen, wenn sie durch sind
    if (ereignis->type() != QEvent::PaletteChange || m_farbenAusstehend || m_vorschau == nullptr) return;
    m_farbenAusstehend = true;
    QTimer::singleShot(0, this, [this] { farbenNeu(); });
}

void Hauptfenster::faerbeSymbole()
{
    const auto gefaerbt = [this](QStyle::StandardPixmap welches) {
        QPixmap bild = style()->standardIcon(welches).pixmap(32, 32);
        QPainter maler(&bild);
        maler.setCompositionMode(QPainter::CompositionMode_SourceIn);  // die Form bleibt, die Farbe wechselt
        maler.fillRect(bild.rect(), palette().color(QPalette::WindowText));
        maler.end();
        return QIcon(bild);
    };
    m_zurueckAktion->setIcon(gefaerbt(QStyle::SP_ArrowBack));
    m_aufruferAktion->setIcon(gefaerbt(QStyle::SP_ArrowUp));
}

void Hauptfenster::farbenNeu()
{
    m_farbenAusstehend = false;
    faerbeSymbole();
    // dasselbe Bild in anderen Farben: Marke und Ausschnitt bleiben
    const c2d::diagramm::KnotenId marke = m_vorschau->markiert();
    const QPointF mitte = m_vorschau->mapToScene(m_vorschau->viewport()->rect().center());
    zeichne();
    if (marke != c2d::diagramm::kKeine) m_vorschau->markiere(marke);
    m_vorschau->centerOn(mitte);
    // die Zeile im Quelltext bleibt, wo sie ist; nur ihre Farbe folgt
    QList<QTextEdit::ExtraSelection> marken = m_quelle->extraSelections();
    for (QTextEdit::ExtraSelection& m : marken) m.format.setBackground(zeilenFarbe());
    m_quelle->setExtraSelections(marken);
}

bool Hauptfenster::zeigeStelle(c2d::diagramm::KnotenId knoten)
{
    if (istWahrheitstabelle()) return !m_tabellen.empty() && zeigeZeile(m_datei, m_tabellen.front().quellZeile);
    if (!istAufrufgraph())
        return knoten < m_fluss.knoten().size() && zeigeZeile(dateiVon(knoten), m_fluss.knoten()[knoten].quellZeile);
    if (knoten >= m_aufrufgraph.graph.knoten.size()) return false;
    const c2d::diagramm::AufrufKnoten& k = m_aufrufgraph.graph.knoten[knoten];
    const QString name = ausStd(k.name);
    if (k.art == c2d::diagramm::AufrufKnotenArt::Funktion) return zeigeZeile(m_aufrufgraph.stellen[knoten].datei, k.zeile);
    if (k.art == c2d::diagramm::AufrufKnotenArt::Fremd)
    {
        statusBar()->showMessage(QStringLiteral("%1: no body in the project").arg(name), 4000);
        return false;
    }
    // ein Makro: wo das Projekt es mit Parametern definiert; mehrmals (etwa je #if-Zweig): das erste, mit Meldung
    std::vector<std::pair<std::size_t, int>> wo;
    for (std::size_t d = 0; d < m_projekt.dateien.size(); ++d)
        for (const c2d::modell::Makro& m : m_projekt.dateien[d].ergebnis.einheit.makros)
            if (m.name == k.name && m.text.starts_with('(')) wo.emplace_back(d, m.zeile);
    if (wo.empty()) return false;
    if (wo.size() > 1)
        statusBar()->showMessage(QStringLiteral("%1 is defined %2 times - showing the first").arg(name).arg(wo.size()), 5000);
    return zeigeZeile(wo.front().first, wo.front().second);
}

bool Hauptfenster::zeigeStelle(std::size_t kasten, std::size_t eintrag)
{
    if (kasten >= m_struktur.kaesten.size()) return false;
    // die Kaesten stehen alle in der gewaehlten Datei
    const c2d::diagramm::Kasten& k = m_struktur.kaesten[kasten];
    if (eintrag == kKeine) return zeigeZeile(m_datei, k.zeile);
    if (eintrag < k.eintraege.size()) return zeigeZeile(m_datei, k.eintraege[eintrag].zeile);
    eintrag -= k.eintraege.size();
    return eintrag < k.methoden.size() && zeigeZeile(m_datei, k.methoden[eintrag].zeile);
}

bool Hauptfenster::klicke(const QString& name)
{
    const std::string n = name.toStdString();
    if (m_gezeigt == QLatin1String(kDatentypen))
    {
        // `Typ` = der Kopf, `Typ::glied` = ein Feld oder eine Methode; der Typ selbst kann `::` tragen
        for (std::size_t k = 0; k < m_struktur.kaesten.size(); ++k)
        {
            const c2d::diagramm::Kasten& kasten = m_struktur.kaesten[k];
            if (n == kasten.name) return zeigeStelle(k, kKeine);
            if (!n.starts_with(kasten.name + "::")) continue;
            const std::string glied = n.substr(kasten.name.size() + 2);
            for (std::size_t e = 0; e < kasten.eintraege.size(); ++e)
                if (kasten.eintraege[e].name == glied) return zeigeStelle(k, e);
            for (std::size_t m = 0; m < kasten.methoden.size(); ++m)
                if (kasten.methoden[m].signatur.starts_with(glied + "(")) return zeigeStelle(k, kasten.eintraege.size() + m);
        }
        return false;
    }
    if (istAufrufgraph())
    {
        for (std::size_t k = 0; k < m_aufrufgraph.graph.knoten.size(); ++k)
            if (m_aufrufgraph.graph.knoten[k].name == n) return zeigeStelle(k);
        return false;
    }
    for (c2d::diagramm::KnotenId k = 0; k < m_fluss.knoten().size(); ++k)
        for (const std::string& z : m_fluss.knoten()[k].zeilen)
            if (z == n) return zeigeStelle(k);
    return false;
}

bool Hauptfenster::zeigeImDiagramm(int zeile)
{
    const std::size_t datei = m_quelleZeigt < m_projekt.dateien.size() ? m_quelleZeigt : m_datei;
    if (datei >= m_projekt.dateien.size() || zeile <= 0) return false;
    const c2d::modell::Einheit& einheit = m_projekt.dateien[datei].ergebnis.einheit;

    // die Funktion, in deren Rumpf die Zeile steht
    std::size_t funktion = kKeine;
    for (std::size_t i = 0; i < einheit.funktionen.size(); ++i)
    {
        const c2d::modell::Funktion& f = einheit.funktionen[i];
        if (f.rumpf && f.zeile <= zeile && zeile <= f.bisZeile) funktion = i;
    }
    if (funktion != kKeine)
    {
        const Fundstelle stelle{datei, funktion};
        const c2d::modell::Funktion& f = einheit.funktionen[funktion];
        if (istAufrufgraph())
            for (std::size_t k = 0; k < m_aufrufgraph.stellen.size(); ++k)
                if (m_aufrufgraph.stellen[k] == stelle)
                {
                    m_vorschau->markiere(k);
                    zeigeZeile(datei, zeile);
                    return true;
                }
        // im gezeigten Ablauf — auch eingebettet: Knoten aus dieser Datei, in den Zeilen der Funktion
        const auto inDerFunktion = [&] {
            std::vector<bool> zaehlt(m_fluss.knoten().size(), false);
            bool eine = false;
            if (istAufrufgraph() || m_gezeigt == QLatin1String(kDatentypen)) return std::pair{zaehlt, eine};
            for (c2d::diagramm::KnotenId k = 0; k < zaehlt.size(); ++k)
            {
                const int z = m_fluss.knoten()[k].quellZeile;
                zaehlt[k] = dateiVon(k) == datei && z >= f.zeile && z <= f.bisZeile;
                eine = eine || zaehlt[k];
            }
            return std::pair{zaehlt, eine};
        };
        auto [zaehlt, gezeigt] = inDerFunktion();
        if (!gezeigt)
        {
            gehe(stelle, kKeine);
            std::tie(zaehlt, gezeigt) = inDerFunktion();
        }
        const c2d::diagramm::KnotenId k = c2d::diagramm::knotenZurZeile(m_fluss, zeile, zaehlt);
        if (k != kKeine) m_vorschau->markiere(k);
        zeigeZeile(datei, zeile);
        return k != kKeine;
    }

    // ein Typ: von seiner Zeile bis eine nach dem letzten Glied (die `}`); der innerste
    std::size_t typ = kKeine;
    for (std::size_t i = 0; i < einheit.typen.size(); ++i)
    {
        const c2d::modell::Typ& t = einheit.typen[i];
        int bis = t.zeile;
        for (const c2d::modell::Feld& g : t.felder) bis = std::max(bis, g.zeile);
        for (const c2d::modell::Methode& m : t.methoden) bis = std::max(bis, m.zeile);
        if (t.zeile > 0 && t.zeile <= zeile && zeile <= bis + 1 &&
            (typ == kKeine || t.zeile >= einheit.typen[typ].zeile))
            typ = i;
    }
    if (typ != kKeine)
    {
        geheZuTyp({datei, typ});
        m_vorschau->markiere(0);  // der Typ ist der erste Kasten seiner Ansicht
        zeigeZeile(datei, zeile);
        return true;
    }
    statusBar()->showMessage(QStringLiteral("Line %1 belongs to no function and no type").arg(zeile), 4000);
    return false;
}

QString Hauptfenster::gezeigteStelle() const
{
    if (m_quelleZeigt >= m_projekt.dateien.size() || m_quelleZeile <= 0) return {};
    return QStringLiteral("%1:%2").arg(ausStd(m_projekt.dateien[m_quelleZeigt].pfad)).arg(m_quelleZeile);
}

void Hauptfenster::speichereUnter()
{
    const c2d::ausgabe::Format* format = c2d::ausgabe::format(m_format->currentData().toString().toStdString());
    if (format == nullptr || diagrammText().isEmpty())
    {
        statusBar()->showMessage(QStringLiteral("Nothing to save"), 3000);
        return;
    }
    if (!m_textVerfuegbar)
    {
        statusBar()->showMessage(diagrammText(), 5000);  // das Format kann dieses Diagramm nicht
        return;
    }
    const QString endung = ausStd(format->endung());
    // im Dateinamen steht kein `::`
    const QString teil = QString(m_gezeigt).replace(QStringLiteral("::"), QStringLiteral("_"));
    const QString vorschlag =
        QStringLiteral("%1_%2.%3").arg(QFileInfo(letzterName(datei())).completeBaseName(), teil, endung);
    const QString pfad = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save diagram"), vorschlag,
        QStringLiteral("%1 (*.%2);;All files (*)").arg(ausStd(format->name()), endung));
    if (pfad.isEmpty()) return;
    QFile datei(pfad);
    if (!datei.open(QIODevice::WriteOnly) || datei.write(diagrammText().toUtf8()) < 0)
    {
        QMessageBox::warning(this, windowTitle(),
                             QStringLiteral("Cannot write %1: %2").arg(pfad, datei.errorString()));
        return;
    }
    statusBar()->showMessage(QStringLiteral("Saved %1").arg(pfad), 5000);
}

}  // namespace c2d::ui
