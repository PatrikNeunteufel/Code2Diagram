/**
 * @file Hauptfenster.cpp
 * @brief Das Fenster: Menue, Quelltext, Auswahl von Diagramm und Format, Vorschau mit Sprung und Einbettung
 */
#include "pch.h"
#include "ui/Hauptfenster.hpp"

#include "AppInfo.hpp"
#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "ui/Vorschau.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QLineEdit>
#include <QRegularExpression>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QVBoxLayout>

namespace c2d::ui
{

namespace
{

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

QString ausStd(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
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

}  // namespace

Hauptfenster::Hauptfenster(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(ausStd(appName()));
    resize(1300, 800);

    auto* teiler = new QSplitter(Qt::Horizontal, this);
    auto* links = new QWidget(teiler);
    m_quelle = neuesTextfeld(links);
    m_quelle->setObjectName(QStringLiteral("quelle"));
    m_quelle->setPlaceholderText(QStringLiteral("Open a source file (File > Open...)"));
    m_definitionen = new QLineEdit(links);
    m_definitionen->setObjectName(QStringLiteral("definitionen"));
    m_definitionen->setPlaceholderText(QStringLiteral("NAME NAME=value ... (for #if / #ifdef)"));
    m_definitionen->setToolTip(QStringLiteral(
        "What counts as defined when #if and #ifdef are evaluated, in addition to the #define of the file.\n"
        "Separate with spaces. Press Enter to read the file again."));
    m_alleZweige = new QCheckBox(QStringLiteral("All #if branches"), links);
    m_alleZweige->setObjectName(QStringLiteral("alleZweige"));
    m_alleZweige->setToolTip(QStringLiteral("Do not evaluate #if and #ifdef: read every branch, one after the other."));
    auto* lesen = new QHBoxLayout;
    lesen->addWidget(new QLabel(QStringLiteral("Defines:"), links));
    lesen->addWidget(m_definitionen, 1);
    lesen->addWidget(m_alleZweige);
    auto* linkeSpalte = new QVBoxLayout(links);
    linkeSpalte->setContentsMargins(0, 0, 0, 0);
    linkeSpalte->addWidget(m_quelle, 1);
    linkeSpalte->addLayout(lesen);
    connect(m_definitionen, &QLineEdit::editingFinished, this, [this] {
        // das Feld meldet sich auch, wenn es nur verlassen wird: neu lesen nur, wenn sich etwas geaendert hat
        if (m_definitionen->text() != m_gelesenMit) liesNeu();
    });
    connect(m_alleZweige, &QCheckBox::toggled, this, [this](bool alle) {
        m_definitionen->setEnabled(!alle);
        liesNeu();
    });

    auto* rechts = new QWidget(teiler);
    m_auswahl = new QComboBox(rechts);
    m_auswahl->setObjectName(QStringLiteral("auswahl"));
    m_format = new QComboBox(rechts);
    m_format->setObjectName(QStringLiteral("format"));
    for (const c2d::ausgabe::Format* f : c2d::ausgabe::formate())
        m_format->addItem(ausStd(f->name()), ausStd(f->name()));
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
    leiste->addWidget(new QLabel(QStringLiteral("Format:"), rechts));
    leiste->addWidget(m_format);
    leiste->addWidget(m_richtung);
    auto* spalte = new QVBoxLayout(rechts);
    spalte->setContentsMargins(0, 0, 0, 0);
    spalte->addLayout(leiste);
    spalte->addWidget(m_ansichten, 1);

    teiler->addWidget(links);
    teiler->addWidget(rechts);
    teiler->setStretchFactor(0, 2);
    teiler->setStretchFactor(1, 3);
    setCentralWidget(teiler);

    connect(m_auswahl, &QComboBox::currentIndexChanged, this, [this] {
        // das bisherige Diagramm merken — ausser der Wechsel ist selbst ein Schritt zurueck
        if (!m_ohneVerlauf && !m_gezeigt.isEmpty() && m_gezeigt != m_auswahl->currentData().toString())
            m_verlauf << m_gezeigt;
        baueNeu();
    });
    for (QComboBox* liste : {m_format, m_richtung})
        connect(liste, &QComboBox::currentIndexChanged, this, [this] { zeichne(); });

    m_vorschau->beiDoppelklick = [this](c2d::diagramm::KnotenId knoten) {
        if (knoten >= m_fluss.knoten().size()) return;
        if (const c2d::modell::Funktion* f = ziel(knoten))
            waehleDiagramm(QString::fromStdString(f->vollerName()));
        else
            zeigeZeile(m_fluss.knoten()[knoten].quellZeile);
    };
    m_vorschau->beiKontext = [this](c2d::diagramm::KnotenId knoten, std::size_t gruppe, const QPoint& ort) {
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
    datei->addAction(QStringLiteral("&Save diagram as..."), QKeySequence::Save, this,
                     [this] { speichereUnter(); });
    datei->addSeparator();
    datei->addAction(QStringLiteral("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* diagramm = menuBar()->addMenu(QStringLiteral("&Diagram"));
    diagramm->addAction(QStringLiteral("&Back"), QKeySequence(Qt::ALT | Qt::Key_Left), this, [this] {
        if (!zurueck()) statusBar()->showMessage(QStringLiteral("No diagram to go back to"), 3000);
    });
    diagramm->addAction(QStringLiteral("&Remove all embeddings"), this, [this] {
        if (!loeseEinbettungen()) statusBar()->showMessage(QStringLiteral("Nothing is embedded"), 3000);
    });
    diagramm->addSeparator();
    diagramm->addAction(QStringLiteral("Show &sample"), this, [this] { zeigeBeispiel(); });
    diagramm->addAction(QStringLiteral("&Copy diagram text"), this, [this] {
        QApplication::clipboard()->setText(diagrammText());
        statusBar()->showMessage(QStringLiteral("Diagram text copied"), 3000);
    });

    QMenu* hilfe = menuBar()->addMenu(QStringLiteral("&Help"));
    hilfe->addAction(QStringLiteral("&About"), this, [this] {
        QMessageBox::about(this, windowTitle(),
                           QStringLiteral("%1 %2\n\nReads source code and derives readable diagrams.")
                               .arg(ausStd(appName()), ausStd(appVersion())));
    });

    statusBar()->showMessage(QStringLiteral("Ready"));
}

bool Hauptfenster::oeffne(const QString& pfad, QString* fehler)
{
    QFile datei(pfad);
    if (!datei.open(QIODevice::ReadOnly))
    {
        if (fehler != nullptr)
            *fehler = QStringLiteral("Cannot read %1: %2").arg(pfad, datei.errorString());
        return false;
    }
    zeige(QString::fromUtf8(datei.readAll()), QFileInfo(pfad).fileName());
    return true;
}

void Hauptfenster::zeige(const QString& quelltext, const QString& name)
{
    m_name = name;
    m_quelle->setPlainText(quelltext);
    m_quelle->setExtraSelections({});

    const std::string dateiname = name.toStdString();
    const std::string inhalt = quelltext.toStdString();
    const c2d::sprache::Sprache* sprache = c2d::sprache::spracheFuer(dateiname, inhalt);
    const bool ersatz = sprache == nullptr;
    if (ersatz) sprache = &c2d::sprache::c();
    c2d::sprache::LeseOptionen optionen;
    optionen.alleZweige = m_alleZweige->isChecked();
    m_gelesenMit = m_definitionen->text();
    static const QRegularExpression kTrenner(QStringLiteral("[\\s,;]+"));
    for (const QString& stueck : m_definitionen->text().split(kTrenner, Qt::SkipEmptyParts))
    {
        c2d::sprache::Definition d = c2d::sprache::definitionAus(stueck.toStdString());
        if (!d.name.empty()) optionen.definitionen.push_back(std::move(d));
    }
    m_ergebnis = sprache->lies(inhalt, dateiname, optionen);
    const c2d::modell::Einheit& einheit = m_ergebnis.einheit;

    int funktionen = 0;
    {
        const QSignalBlocker ruhe(m_auswahl);
        m_auswahl->clear();
        for (const c2d::modell::Funktion& f : einheit.funktionen)
        {
            // mit Bereich (`Klasse::methode`); von ueberladenen Funktionen steht die erste zur Wahl
            const QString fname = QString::fromStdString(f.vollerName());
            if (!f.rumpf || m_auswahl->findData(fname) >= 0) continue;
            m_auswahl->addItem(fname + QStringLiteral("()"), fname);
            ++funktionen;
        }
        if (!einheit.typen.empty())
            m_auswahl->addItem(QStringLiteral("Data types (%1)").arg(einheit.typen.size()),
                               QString::fromLatin1(kDatentypen));
    }
    m_verlauf.clear();
    baueNeu();

    setWindowTitle(QStringLiteral("%1 - %2").arg(name, ausStd(appName())));
    QString lage = QStringLiteral("%1, read as %2%3: %4 function(s), %5 data type(s)")
                       .arg(name, ausStd(sprache->name()),
                            ersatz ? QStringLiteral(" (no reader for this file type)") : QString())
                       .arg(funktionen)
                       .arg(einheit.typen.size());
    if (!m_ergebnis.meldungen.empty())
    {
        const c2d::quelle::Meldung& erste = m_ergebnis.meldungen.front();
        lage += QStringLiteral(" - %1 message(s), first in line %2: %3")
                    .arg(m_ergebnis.meldungen.size())
                    .arg(erste.ort.zeile)
                    .arg(QString::fromStdString(erste.text));
    }
    if (const QStringList offen = unbekannteNamen(); !offen.isEmpty())
    {
        // wonach ein #if gefragt hat: so sieht man, welche Variante gelesen wurde und was sich waehlen laesst
        lage += QStringLiteral(" - not defined in #if: %1%2")
                    .arg(offen.mid(0, 6).join(QStringLiteral(", ")), offen.size() > 6 ? QStringLiteral(", ...") : QString());
    }
    statusBar()->showMessage(lage);
}

void Hauptfenster::liesNeu()
{
    if (m_name.isEmpty()) return;
    const QString gezeigt = m_gezeigt;
    zeige(m_quelle->toPlainText(), m_name);
    // das Diagramm von vorher wieder zeigen, wenn es das noch gibt — ohne Eintrag im Verlauf
    m_ohneVerlauf = true;
    waehle(m_auswahl, gezeigt);
    m_ohneVerlauf = false;
}

void Hauptfenster::setzeDefinitionen(const QString& definitionen)
{
    m_definitionen->setText(definitionen);
    liesNeu();
}

void Hauptfenster::setzeAlleZweige(bool alle)
{
    if (m_alleZweige->isChecked() == alle) return;
    m_alleZweige->setChecked(alle);  // liest ueber das Signal neu
}

QStringList Hauptfenster::unbekannteNamen() const
{
    QStringList namen;
    for (const std::string& name : m_ergebnis.unbekannteNamen) namen << QString::fromStdString(name);
    return namen;
}

void Hauptfenster::zeigeBeispiel()
{
    zeige(ausStd(kBeispiel), QStringLiteral("ampel.c"));
}

QStringList Hauptfenster::diagramme() const
{
    QStringList namen;
    for (int i = 0; i < m_auswahl->count(); ++i) namen << m_auswahl->itemData(i).toString();
    return namen;
}

bool Hauptfenster::waehleDiagramm(const QString& name)
{
    return waehle(m_auswahl, name);
}

QString Hauptfenster::diagramm() const
{
    return m_gezeigt;
}

bool Hauptfenster::zurueck()
{
    if (m_verlauf.isEmpty()) return false;
    const QString davor = m_verlauf.takeLast();
    m_ohneVerlauf = true;
    const bool gefunden = waehle(m_auswahl, davor);
    m_ohneVerlauf = false;
    return gefunden;
}

bool Hauptfenster::waehleFormat(const QString& name)
{
    return waehle(m_format, name);
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

const c2d::modell::Funktion* Hauptfenster::funktion(const std::string& name) const
{
    for (const c2d::modell::Funktion& f : m_ergebnis.einheit.funktionen)
        if (f.rumpf && f.name == name) return &f;
    return nullptr;
}

const c2d::modell::Funktion* Hauptfenster::ziel(c2d::diagramm::KnotenId knoten) const
{
    if (knoten >= m_fluss.knoten().size()) return nullptr;
    for (const std::string& name : m_fluss.knoten()[knoten].aufrufe)
        if (const c2d::modell::Funktion* f = funktion(name)) return f;
    return nullptr;
}

bool Hauptfenster::betteEin(c2d::diagramm::KnotenId knoten, const c2d::modell::Funktion& gerufen)
{
    using namespace c2d::diagramm;
    Flussdiagramm neu =
        c2d::diagramm::betteEin(m_fluss, knoten, fasseAnweisungsfolgenZusammen(flussVon(gerufen)), gerufen.signatur());
    if (neu.gruppen().size() == m_fluss.gruppen().size()) return false;
    m_fluss = std::move(neu);
    zeichne();
    return true;
}

bool Hauptfenster::betteEin(const QString& funktionName)
{
    const std::string name = funktionName.toStdString();
    const c2d::modell::Funktion* gerufen = funktion(name);
    if (gerufen == nullptr) return false;
    for (c2d::diagramm::KnotenId i = 0; i < m_fluss.knoten().size(); ++i)
    {
        const std::vector<std::string>& aufrufe = m_fluss.knoten()[i].aufrufe;
        if (std::find(aufrufe.begin(), aufrufe.end(), name) != aufrufe.end() && betteEin(i, *gerufen)) return true;
    }
    return false;
}

bool Hauptfenster::loeseEinbettungen()
{
    if (m_fluss.gruppen().empty()) return false;
    baueNeu();
    return true;
}

int Hauptfenster::einbettungen() const
{
    return static_cast<int>(m_fluss.gruppen().size());
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

int Hauptfenster::meldungen() const
{
    return static_cast<int>(m_ergebnis.meldungen.size());
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
    m_gezeigt = m_auswahl->currentData().toString();
    m_fluss = Flussdiagramm();
    if (!m_gezeigt.isEmpty() && m_gezeigt != QLatin1String(kDatentypen))
    {
        const std::string name = m_gezeigt.toStdString();
        for (const c2d::modell::Funktion& f : m_ergebnis.einheit.funktionen)
        {
            if (!f.rumpf || f.vollerName() != name) continue;
            m_fluss = fasseAnweisungsfolgenZusammen(flussVon(f));
            break;
        }
    }
    zeichne();
}

void Hauptfenster::zeichne()
{
    using namespace c2d::diagramm;

    const c2d::ausgabe::Format* format = c2d::ausgabe::format(m_format->currentData().toString().toStdString());
    if (m_gezeigt.isEmpty() || format == nullptr)
    {
        m_text->clear();
        m_vorschau->leere();
        return;
    }
    const std::string richtung = m_richtung->currentData().toString().toStdString();
    const c2d::ausgabe::Optionen optionen{.richtung = richtung};
    const bool nachRechts = richtung == "LR";

    if (m_gezeigt == QLatin1String(kDatentypen))
    {
        const Strukturdiagramm typen = strukturVon(m_ergebnis.einheit);
        m_vorschau->zeige(typen, nachRechts);
        m_text->setPlainText(QString::fromStdString(format->schreibe(typen, optionen)));
        return;
    }
    // hervorgehoben wird, was weiterfuehrt: ein Knoten, der eine Funktion dieser Datei ruft
    std::vector<bool> verweis(m_fluss.knoten().size(), false);
    for (KnotenId i = 0; i < verweis.size(); ++i) verweis[i] = ziel(i) != nullptr;
    m_vorschau->zeige(m_fluss, nachRechts, verweis);
    m_text->setPlainText(QString::fromStdString(format->schreibe(m_fluss, optionen)));
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
        for (const std::string& name : k.aufrufe)
        {
            const c2d::modell::Funktion* f = funktion(name);
            if (f == nullptr) continue;
            const QString voll = QString::fromStdString(f->vollerName());
            menue.addAction(QStringLiteral("Go to %1()").arg(voll), this, [this, voll] { waehleDiagramm(voll); });
            QAction* ein = menue.addAction(QStringLiteral("Embed %1() here").arg(voll), this,
                                           [this, knoten, f] { betteEin(knoten, *f); });
            ein->setEnabled(!traegtSchon);
        }
        if (k.quellZeile > 0)
            menue.addAction(QStringLiteral("Show line %1 in the source").arg(k.quellZeile), this,
                            [this, zeile = k.quellZeile] { zeigeZeile(zeile); });
    }
    if (gruppe < m_fluss.gruppen().size())
    {
        if (!menue.isEmpty()) menue.addSeparator();
        menue.addAction(QStringLiteral("Remove embedding: %1").arg(QString::fromStdString(m_fluss.gruppen()[gruppe].titel)),
                        this, [this, gruppe] {
                            m_fluss = c2d::diagramm::loeseEin(m_fluss, gruppe);
                            zeichne();
                        });
    }
    if (!m_fluss.gruppen().empty())
        menue.addAction(QStringLiteral("Remove all embeddings"), this, [this] { loeseEinbettungen(); });
    if (!m_verlauf.isEmpty())
    {
        if (!menue.isEmpty()) menue.addSeparator();
        menue.addAction(QStringLiteral("Back to %1").arg(m_verlauf.last()), this, [this] { zurueck(); });
    }
    if (!menue.isEmpty()) menue.exec(ort);
}

void Hauptfenster::zeigeZeile(int zeile)
{
    const QTextBlock block = m_quelle->document()->findBlockByNumber(zeile - 1);
    if (zeile <= 0 || !block.isValid()) return;
    QTextCursor zeiger(block);
    m_quelle->setTextCursor(zeiger);
    m_quelle->centerCursor();
    // die ganze Zeile hinterlegen, auch wo sie leer ist
    QTextEdit::ExtraSelection marke;
    marke.cursor = zeiger;
    marke.format.setBackground(palette().color(QPalette::Highlight).lighter(160));
    marke.format.setProperty(QTextFormat::FullWidthSelection, true);
    m_quelle->setExtraSelections({marke});
}

void Hauptfenster::speichereUnter()
{
    const c2d::ausgabe::Format* format = c2d::ausgabe::format(m_format->currentData().toString().toStdString());
    if (format == nullptr || diagrammText().isEmpty())
    {
        statusBar()->showMessage(QStringLiteral("Nothing to save"), 3000);
        return;
    }
    const QString endung = ausStd(format->endung());
    // im Dateinamen steht kein `::`
    const QString teil = QString(m_gezeigt).replace(QStringLiteral("::"), QStringLiteral("_"));
    const QString vorschlag = QStringLiteral("%1_%2.%3").arg(QFileInfo(m_name).completeBaseName(), teil, endung);
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
