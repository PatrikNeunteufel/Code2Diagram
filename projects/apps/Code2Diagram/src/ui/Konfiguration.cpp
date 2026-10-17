/**
 * @file Konfiguration.cpp
 * @brief Das Feld Configuration: Namen als Haken, Ketten #if als Knoepfe
 */
#include "pch.h"
#include "ui/Konfiguration.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace c2d::ui
{

namespace
{

using sprache::Definition;
using sprache::Forderung;
using sprache::kKeinZweig;
using sprache::Schalter;
using sprache::Weiche;

QString ausStd(const std::string& text)
{
    return QString::fromStdString(text);
}

/// `X=2`, `X` oder `no X` je Forderung, durch Komma.
QString beschreibe(const std::vector<Forderung>& forderungen)
{
    QStringList teile;
    for (const Forderung& f : forderungen)
        teile << (!f.definiert ? QStringLiteral("no %1").arg(ausStd(f.name))
                  : f.wert.empty() ? ausStd(f.name)
                                   : QStringLiteral("%1=%2").arg(ausStd(f.name), ausStd(f.wert)));
    return teile.join(QStringLiteral(", "));
}

}  // namespace

Konfiguration::Konfiguration(QWidget* parent) : QWidget(parent)
{
    m_ganzeDatei = new QCheckBox(QStringLiteral("Whole file"), this);
    m_ganzeDatei->setObjectName(QStringLiteral("ganzeDatei"));
    m_ganzeDatei->setToolTip(QStringLiteral("Show the #if chains of the whole file, not only those of the function shown"));
    m_flaeche = new QScrollArea(this);
    m_flaeche->setWidgetResizable(true);
    m_flaeche->setFrameShape(QFrame::NoFrame);
    m_speichern = new QPushButton(QStringLiteral("Save to project file"), this);
    m_speichern->setObjectName(QStringLiteral("speichern"));
    m_speichern->setToolTip(QStringLiteral("Write what Defines holds into \"definitionen\" of code2diagram.json"));
    m_speichern->setEnabled(false);

    setMinimumWidth(260);
    auto* spalte = new QVBoxLayout(this);
    spalte->setContentsMargins(4, 4, 4, 4);
    spalte->addWidget(m_ganzeDatei);
    spalte->addWidget(m_flaeche, 1);
    spalte->addWidget(m_speichern);

    connect(m_ganzeDatei, &QCheckBox::toggled, this, [this] { baue(); });
    connect(m_speichern, &QPushButton::clicked, this, [this] {
        if (beimSpeichern) beimSpeichern();
    });
    baue();
}

void Konfiguration::zeige(const sprache::Ergebnis* ergebnis, std::vector<Definition> aussen,
                          std::vector<Definition> darunter, int von, int bis, bool projekt)
{
    m_schalter = ergebnis != nullptr ? ergebnis->schalter : std::vector<Schalter>();
    m_weichen = ergebnis != nullptr ? ergebnis->weichen : std::vector<Weiche>();
    m_aussen = std::move(aussen);
    m_darunter = std::move(darunter);
    m_von = von;
    m_bis = bis;
    m_projekt = projekt;
    baue();
}

QString Konfiguration::alsText(const std::vector<Definition>& definitionen)
{
    QStringList teile;
    for (const Definition& d : definitionen) teile << ausStd(sprache::alsText(d));
    return teile.join(QLatin1Char(' '));
}

bool Konfiguration::gesperrt(const Schalter& s) const
{
    // was die Projektdatei oder ein Kopf definiert, hebt `!NAME` im Feld auf; die Datei selbst nicht
    return s.inDatei > 0 || s.vorgegeben;
}

std::optional<std::vector<Definition>> Konfiguration::nachWahl(std::size_t w, std::size_t zweig) const
{
    const Weiche& weiche = m_weichen[w];
    if (zweig >= weiche.wahl.size() || !weiche.wahl[zweig]) return std::nullopt;
    return sprache::mitForderungen(m_aussen, *weiche.wahl[zweig], m_schalter, m_darunter);
}

std::vector<std::size_t> Konfiguration::gezeigteWeichen() const
{
    std::vector<std::size_t> aus;
    const bool alle = m_ganzeDatei->isChecked() || m_von == 0;
    for (std::size_t w = 0; w < m_weichen.size(); ++w)
        if (alle || (m_weichen[w].zeile >= m_von && m_weichen[w].zeile <= m_bis)) aus.push_back(w);
    return aus;
}

QString Konfiguration::zweigName(std::size_t w, std::size_t zweig) const
{
    const Weiche& weiche = m_weichen[w];
    return zweig < weiche.zweige.size() ? ausStd(weiche.zweige[zweig]) : QStringLiteral("(no #else)");
}

void Konfiguration::baue()
{
    // das alte Innere erst spaeter loeschen: ein Klick darin hat das Neuaufbauen ausgeloest
    if (QWidget* alt = m_flaeche->takeWidget()) alt->deleteLater();
    auto* inhalt = new QWidget;
    auto* spalte = new QVBoxLayout(inhalt);

    // die Namen
    auto* namen = new QGroupBox(QStringLiteral("Names asked by #if"), inhalt);
    auto* raster = new QGridLayout(namen);
    for (int i = 0; i < static_cast<int>(m_schalter.size()); ++i)
    {
        const Schalter& s = m_schalter[static_cast<std::size_t>(i)];
        const QString name = ausStd(s.name);
        const bool zu = gesperrt(s);
        // warum gesperrt: kurz hinter dem Namen, ausfuehrlich im Tooltip
        QString warum, kurz;
        if (s.inDatei > 0)
        {
            warum = QStringLiteral("defined in the file, line %1").arg(s.inDatei);
            kurz = QStringLiteral("file, line %1").arg(s.inDatei);
        }
        else if (s.vorgegeben)
            warum = kurz = QStringLiteral("language");
        else if (const projekt::Herkunft h = herkunft ? herkunft(s.name) : projekt::Herkunft::keine;
                 h != projekt::Herkunft::keine)
        {
            // die Projektdatei oder ein Kopf legt ihn fest; das Feld Defines geht vor
            const bool aufgehoben = std::any_of(m_aussen.begin(), m_aussen.end(), [&](const Definition& d) {
                return d.name == s.name && !d.definiert;
            });
            const QString wo = h == projekt::Herkunft::projekt     ? QStringLiteral("the project file")
                               : h == projekt::Herkunft::abschnitt ? QStringLiteral("a section of the project file")
                                                                   : QStringLiteral("an included header");
            kurz = h == projekt::Herkunft::projekt     ? QStringLiteral("project")
                   : h == projekt::Herkunft::abschnitt ? QStringLiteral("section")
                                                       : QStringLiteral("header");
            if (aufgehoben)
            {
                kurz += QStringLiteral(", !") + name;
                warum = QStringLiteral("Set by %1, undefined by !%2 in Defines. Tick to take that back.").arg(wo, name);
            }
            else
                warum = QStringLiteral("Set by %1. Untick to undefine it here (!%2 in Defines); the file stays as it is.")
                            .arg(wo, name);
        }
        auto* haken = new QCheckBox(kurz.isEmpty() ? name : QStringLiteral("%1  (%2)").arg(name, kurz), namen);
        haken->setObjectName(QStringLiteral("schalter_") + name);
        haken->setChecked(s.definiert);
        haken->setEnabled(!zu);
        haken->setToolTip(warum.isEmpty() ? QStringLiteral("Defined or not (for #if / #ifdef)") : warum);
        // Rechtsklick: wo der Name im Projekt schaltet — auch bei einem gesperrten Haken
        haken->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(haken, &QWidget::customContextMenuRequested, this, [this, haken, name](const QPoint& ort) {
            if (!beiSchaltstellen) return;
            QMenu menue(haken);
            menue.addAction(QStringLiteral("Where is %1 switched?").arg(name), this, [this, name] { beiSchaltstellen(name); });
            menue.exec(haken->mapToGlobal(ort));
        });
        raster->addWidget(haken, i, 0);
        QLineEdit* wert = nullptr;
        if (s.zahl)
        {
            wert = new QLineEdit(ausStd(s.wert), namen);
            wert->setObjectName(QStringLiteral("wert_") + name);
            wert->setPlaceholderText(QStringLiteral("value"));
            wert->setToolTip(QStringLiteral("The value: %1 is compared or computed with").arg(name));
            wert->setEnabled(!zu);
            wert->setFixedWidth(70);
            raster->addWidget(wert, i, 1);
            connect(wert, &QLineEdit::editingFinished, this, [this, name, wert, alt = ausStd(s.wert)] {
                if (wert->text() != alt && !wert->text().isEmpty()) setzeSchalter(name, true, wert->text());
            });
        }
        connect(haken, &QCheckBox::clicked, this,
                [this, name, wert](bool an) { setzeSchalter(name, an, wert != nullptr ? wert->text() : QString()); });
    }
    if (m_schalter.empty()) raster->addWidget(new QLabel(QStringLiteral("No #if asks for a name"), namen), 0, 0);
    raster->setColumnStretch(0, 1);
    spalte->addWidget(namen);

    // die Ketten
    auto* ketten = new QGroupBox(m_ganzeDatei->isChecked() || m_von == 0 ? QStringLiteral("#if chains of the file")
                                                                         : QStringLiteral("#if chains of the function"),
                                 inhalt);
    auto* liste = new QVBoxLayout(ketten);
    const std::vector<std::size_t> gezeigt = gezeigteWeichen();
    for (const std::size_t w : gezeigt)
    {
        const Weiche& weiche = m_weichen[w];
        int tiefe = 0;
        for (std::size_t e = weiche.eltern; e != kKeinZweig && e < m_weichen.size(); e = m_weichen[e].eltern) ++tiefe;
        // die Zeile des #if, darunter je Zweig ein Knopf; eine Kette in einem Zweig weiter eingerueckt
        liste->addWidget(new QLabel(QStringLiteral("line %1").arg(weiche.zeile), ketten));
        auto* zeile = new QVBoxLayout;
        zeile->setContentsMargins(12 + 16 * tiefe, 0, 0, 6);
        zeile->setSpacing(2);
        auto* gruppe = new QButtonGroup(ketten);
        for (std::size_t z = 0; z < weiche.wahl.size(); ++z)
        {
            auto* knopf = new QRadioButton(zweigName(w, z), ketten);
            knopf->setObjectName(QStringLiteral("zweig_%1_%2").arg(weiche.zeile).arg(z));
            gruppe->addButton(knopf);
            knopf->setChecked(z == weiche.geltend);
            const bool gilt = z == weiche.geltend;
            const std::optional<std::vector<Definition>> neu = nachWahl(w, z);
            knopf->setEnabled(gilt || neu.has_value());
            const QString bedingung = z < weiche.zweige.size() && weiche.zweige[z] != "else"
                                          ? QStringLiteral("#if %1").arg(zweigName(w, z))
                                          : QStringLiteral("none of the branches before");
            if (gilt)
                knopf->setToolTip(QStringLiteral("Holds: %1").arg(bedingung));
            else if (!weiche.wahl[z])
                knopf->setToolTip(QStringLiteral("Cannot be chosen from names: %1").arg(bedingung));
            else if (!neu)
                knopf->setToolTip(QStringLiteral("Needs %1 - not possible from here (defined in the file itself)")
                                      .arg(beschreibe(*weiche.wahl[z])));
            else
                knopf->setToolTip(QStringLiteral("Sets %1").arg(beschreibe(*weiche.wahl[z])));
            const int zeileNr = weiche.zeile;
            connect(knopf, &QRadioButton::clicked, this, [this, zeileNr, z] { waehleZweig(zeileNr, static_cast<int>(z)); });
            zeile->addWidget(knopf);
        }
        liste->addLayout(zeile);
    }
    if (gezeigt.empty()) liste->addWidget(new QLabel(QStringLiteral("No #if here"), ketten));
    spalte->addWidget(ketten);
    spalte->addStretch(1);

    m_flaeche->setWidget(inhalt);
    m_speichern->setEnabled(m_projekt && !m_aussen.empty());
}

bool Konfiguration::wende(const std::vector<Forderung>& forderungen)
{
    const std::optional<std::vector<Definition>> neu =
        sprache::mitForderungen(m_aussen, forderungen, m_schalter, m_darunter);
    if (!neu) return false;
    if (beiAenderung) beiAenderung(alsText(*neu));
    return true;
}

bool Konfiguration::setzeSchalter(const QString& name, bool an, const QString& wert)
{
    const std::string n = name.toStdString();
    const auto s = std::find_if(m_schalter.begin(), m_schalter.end(), [&](const Schalter& x) { return x.name == n; });
    if (s == m_schalter.end() || gesperrt(*s)) return false;
    return wende({Forderung{n, an, an ? wert.toStdString() : std::string()}});
}

bool Konfiguration::waehleZweig(int zeile, int zweig)
{
    for (std::size_t w = 0; w < m_weichen.size(); ++w)
    {
        const Weiche& weiche = m_weichen[w];
        if (weiche.zeile != zeile) continue;
        const auto z = static_cast<std::size_t>(zweig);
        if (zweig < 0 || z >= weiche.wahl.size()) return false;
        if (z == weiche.geltend) return true;
        if (!weiche.wahl[z]) return false;
        return wende(*weiche.wahl[z]);
    }
    return false;
}

QStringList Konfiguration::schalter() const
{
    QStringList aus;
    for (const Schalter& s : m_schalter)
    {
        // ein Name, der einen Wert braucht, immer mit Wert
        QString eintrag = !s.definiert ? QStringLiteral("-") + ausStd(s.name)
                          : !s.zahl && (s.wert.empty() || s.wert == "1")
                              ? ausStd(s.name)
                              : QStringLiteral("%1=%2").arg(ausStd(s.name), ausStd(s.wert));
        if (gesperrt(s)) eintrag += QStringLiteral(" (locked)");
        aus << eintrag;
    }
    return aus;
}

QStringList Konfiguration::weichen() const
{
    QStringList aus;
    for (const std::size_t w : gezeigteWeichen())
    {
        const Weiche& weiche = m_weichen[w];
        QStringList zweige;
        for (std::size_t z = 0; z < weiche.wahl.size(); ++z)
        {
            const QString name = zweigName(w, z);
            if (z == weiche.geltend) zweige << QStringLiteral("[%1]").arg(name);
            else if (!nachWahl(w, z)) zweige << QStringLiteral("(%1)").arg(name);
            else zweige << name;
        }
        aus << QStringLiteral("%1: %2").arg(weiche.zeile).arg(zweige.join(QStringLiteral(" | ")));
    }
    return aus;
}

}  // namespace c2d::ui
