/**
 * @file Einstellungen.cpp
 * @brief Die Tabelle der Einstellungen, Lesen und Speichern, der Dialog
 */
#include "pch.h"
#include "ui/Einstellungen.hpp"

#include "ui/Thema.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace c2d::ui
{

namespace einstellungen
{

const std::vector<Einstellung>& alle()
{
    static const std::vector<Einstellung> kAlle = [] {
        std::vector<Wahl> themen;
        for (const thema::Art& a : thema::arten())
            themen.push_back({QString::fromLatin1(a.id), QString::fromLatin1(a.titel)});
        return std::vector<Einstellung>{
            {QString::fromLatin1(kThema), QStringLiteral("Appearance"), QStringLiteral("Theme"),
             QStringLiteral("System follows the light or dark mode of the operating system, also while running"),
             QStringLiteral("system"), std::move(themen)},
        };
    }();
    return kAlle;
}

const Einstellung* finde(const QString& schluessel)
{
    for (const Einstellung& e : alle())
        if (e.schluessel == schluessel) return &e;
    return nullptr;
}

namespace
{

bool erlaubt(const Einstellung& e, const QString& wert)
{
    return std::any_of(e.wahl.begin(), e.wahl.end(), [&](const Wahl& w) { return w.id == wert; });
}

}  // namespace

QString wert(const QString& schluessel)
{
    const Einstellung* e = finde(schluessel);
    if (e == nullptr) return {};
    const QString gespeichert = QSettings().value(schluessel).toString();
    return erlaubt(*e, gespeichert) ? gespeichert : e->vorgabe;
}

bool setze(const QString& schluessel, const QString& wert)
{
    const Einstellung* e = finde(schluessel);
    if (e == nullptr || !erlaubt(*e, wert)) return false;
    QSettings().setValue(schluessel, wert);
    return true;
}

void wendeAn()
{
    thema::wende(thema::ausId(wert(QString::fromLatin1(kThema))).value_or(thema::Thema::System));
}

}  // namespace einstellungen

EinstellungenDialog::EinstellungenDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Settings"));
    auto* seiten = new QListWidget(this);
    seiten->setObjectName(QStringLiteral("seiten"));
    auto* stapel = new QStackedWidget(this);

    // je Seite ein Formular, in der Reihenfolge, in der die Seiten in der Tabelle zuerst vorkommen
    std::vector<QString> namen;
    std::vector<QFormLayout*> formulare;
    for (const Einstellung& e : einstellungen::alle())
    {
        auto platz = std::find(namen.begin(), namen.end(), e.seite);
        if (platz == namen.end())
        {
            auto* seite = new QWidget(stapel);
            formulare.push_back(new QFormLayout(seite));
            stapel->addWidget(seite);
            seiten->addItem(e.seite);
            namen.push_back(e.seite);
            platz = namen.end() - 1;
        }
        auto* feld = new QComboBox(this);
        feld->setObjectName(e.schluessel);
        feld->setToolTip(e.hilfe);
        for (const Wahl& w : e.wahl) feld->addItem(w.titel, w.id);
        feld->setCurrentIndex(std::max(0, feld->findData(einstellungen::wert(e.schluessel))));
        formulare[static_cast<std::size_t>(platz - namen.begin())]->addRow(e.titel, feld);
        m_felder.push_back(feld);
    }
    seiten->setMaximumWidth(160);
    connect(seiten, &QListWidget::currentRowChanged, stapel, &QStackedWidget::setCurrentIndex);
    seiten->setCurrentRow(0);

    auto* knoepfe = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(knoepfe, &QDialogButtonBox::accepted, this, [this] {
        uebernimm();
        accept();
    });
    connect(knoepfe, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(knoepfe->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] { uebernimm(); });

    auto* oben = new QHBoxLayout;
    oben->addWidget(seiten);
    oben->addWidget(stapel, 1);
    auto* ganz = new QVBoxLayout(this);
    ganz->addLayout(oben, 1);
    ganz->addWidget(knoepfe);
    resize(520, 300);
}

QComboBox* EinstellungenDialog::feld(const QString& schluessel) const
{
    const std::vector<Einstellung>& alle = einstellungen::alle();
    for (std::size_t i = 0; i < alle.size(); ++i)
        if (alle[i].schluessel == schluessel) return m_felder[i];
    return nullptr;
}

void EinstellungenDialog::uebernimm()
{
    const std::vector<Einstellung>& alle = einstellungen::alle();
    for (std::size_t i = 0; i < alle.size(); ++i)
        einstellungen::setze(alle[i].schluessel, m_felder[i]->currentData().toString());
    einstellungen::wendeAn();
}

}  // namespace c2d::ui
