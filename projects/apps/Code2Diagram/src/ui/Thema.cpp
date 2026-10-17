/**
 * @file Thema.cpp
 * @brief Das Farbschema: Stil Fusion, eine Palette je hell und dunkel, System folgt dem Betriebssystem
 */
#include "pch.h"
#include "ui/Thema.hpp"

#include <QApplication>
#include <QPalette>
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

struct PalettenFarbe
{
    QPalette::ColorRole rolle;
    const char* hell;
    const char* dunkel;
};

// Die Palette fuer Fusion, je hell und dunkel. Selbst gesetzt, weil die des Systems unter Windows
// nicht zu Fusion passt (dunkel waren leere Kaestchen gefuellt wie angehakt).
constexpr PalettenFarbe kPalette[] = {
    {QPalette::Window, "#efefef", "#2d2d2d"},
    {QPalette::WindowText, "#1e1e1e", "#e0e0e0"},
    {QPalette::Base, "#ffffff", "#1e1e1e"},
    {QPalette::AlternateBase, "#f5f5f5", "#262626"},
    {QPalette::ToolTipBase, "#ffffdc", "#3a3a3a"},
    {QPalette::ToolTipText, "#1e1e1e", "#e0e0e0"},
    {QPalette::PlaceholderText, "#8a8a8a", "#8a8a8a"},
    {QPalette::Text, "#1e1e1e", "#e0e0e0"},
    {QPalette::Button, "#efefef", "#383838"},
    {QPalette::ButtonText, "#1e1e1e", "#e0e0e0"},
    {QPalette::BrightText, "#ffffff", "#ff6060"},
    {QPalette::Light, "#ffffff", "#4a4a4a"},
    {QPalette::Midlight, "#e3e3e3", "#404040"},
    {QPalette::Mid, "#a0a0a0", "#5a5a5a"},
    {QPalette::Dark, "#a0a0a0", "#1a1a1a"},
    {QPalette::Shadow, "#696969", "#000000"},
    {QPalette::Highlight, "#3072c4", "#2f6fbf"},
    {QPalette::HighlightedText, "#ffffff", "#ffffff"},
    {QPalette::Link, "#1a66cc", "#4c9aff"},
    {QPalette::LinkVisited, "#6b3fa0", "#b48ee6"},
    {QPalette::Accent, "#3072c4", "#2f6fbf"},
};

/// Was gesperrt ist, in allen Rollen fuer Text.
constexpr PalettenFarbe kGesperrt[] = {
    {QPalette::WindowText, "#9a9a9a", "#7a7a7a"},
    {QPalette::Text, "#9a9a9a", "#7a7a7a"},
    {QPalette::ButtonText, "#9a9a9a", "#7a7a7a"},
};

Thema s_gilt = Thema::System;

QPalette palette(bool dunkel)
{
    QPalette p;
    for (const PalettenFarbe& f : kPalette) p.setColor(QPalette::All, f.rolle, QColor(dunkel ? f.dunkel : f.hell));
    for (const PalettenFarbe& f : kGesperrt) p.setColor(QPalette::Disabled, f.rolle, QColor(dunkel ? f.dunkel : f.hell));
    return p;
}

/// Ist es jetzt dunkel? Bei System fragt Qt das Betriebssystem.
bool istDunkel(Thema thema)
{
    if (thema != Thema::System) return thema == Thema::Dunkel;
    const QStyleHints* hinweise = QGuiApplication::styleHints();
    return hinweise != nullptr && hinweise->colorScheme() == Qt::ColorScheme::Dark;
}

/// Fusion in beiden Themen, unter Windows 10 und 11 gleich (nicht, wenn QT_STYLE_OVERRIDE gesetzt ist).
void setzeStil()
{
    if (qEnvironmentVariableIsSet("QT_STYLE_OVERRIDE")) return;
    const QStyle* stil = QApplication::style();
    if (stil == nullptr || stil->name().compare(QLatin1String("fusion"), Qt::CaseInsensitive) != 0)
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
}

/// Schaltet das Betriebssystem um, waehrend System gilt: die Palette folgt.
void folgeDemSystem()
{
    static bool verbunden = false;
    QStyleHints* hinweise = QGuiApplication::styleHints();
    if (verbunden || hinweise == nullptr) return;
    verbunden = true;
    QObject::connect(hinweise, &QStyleHints::colorSchemeChanged, qApp, [](Qt::ColorScheme schema) {
        if (s_gilt == Thema::System) QApplication::setPalette(palette(schema == Qt::ColorScheme::Dark));
    });
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
    s_gilt = thema;  // vor setColorScheme: dessen Signal fragt danach
    setzeStil();
    folgeDemSystem();
    // das Farbschema auch dem System sagen: die Titelleiste unter Windows folgt ihm
    Qt::ColorScheme schema = Qt::ColorScheme::Unknown;  // Unknown: wie das System
    if (thema == Thema::Hell) schema = Qt::ColorScheme::Light;
    if (thema == Thema::Dunkel) schema = Qt::ColorScheme::Dark;
    if (QStyleHints* hinweise = QGuiApplication::styleHints()) hinweise->setColorScheme(schema);
    QApplication::setPalette(palette(istDunkel(thema)));
}

Thema gilt()
{
    return s_gilt;
}

}  // namespace c2d::ui::thema
