/**
 * @file Thema.cpp
 * @brief Das Farbschema ueber QStyleHints setzen
 */
#include "pch.h"
#include "ui/Thema.hpp"

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace c2d::ui::thema
{

namespace
{

constexpr std::array<Art, 3> kArten{{
    {Thema::System, "system", "System"},
    {Thema::Hell, "light", "Light"},
    {Thema::Dunkel, "dark", "Dark"},
}};

Thema s_gilt = Thema::System;

/// Der Windows-10-Stil `windowsvista` (und `windows`) setzt kein Farbschema um — Fusion schon.
void stilMitFarbschema()
{
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsSet("QT_STYLE_OVERRIDE")) return;
    const QStyle* stil = QApplication::style();
    const QString name = stil ? stil->name().toLower() : QString();
    if (name == QLatin1String("windowsvista") || name == QLatin1String("windows"))
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
#endif
}

}  // namespace

const std::array<Art, 3>& arten()
{
    return kArten;
}

std::optional<Thema> ausId(const QString& id)
{
    for (const Art& a : kArten)
        if (id == QLatin1String(a.id)) return a.thema;
    return std::nullopt;
}

QString idVon(Thema thema)
{
    for (const Art& a : kArten)
        if (a.thema == thema) return QString::fromLatin1(a.id);
    return {};
}

void wende(Thema thema)
{
    stilMitFarbschema();
    Qt::ColorScheme schema = Qt::ColorScheme::Unknown;  // Unknown: wie das System
    if (thema == Thema::Hell) schema = Qt::ColorScheme::Light;
    if (thema == Thema::Dunkel) schema = Qt::ColorScheme::Dark;
    if (QStyleHints* hinweise = QGuiApplication::styleHints()) hinweise->setColorScheme(schema);
    s_gilt = thema;
}

Thema gilt()
{
    return s_gilt;
}

}  // namespace c2d::ui::thema
