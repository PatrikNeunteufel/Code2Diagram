/**
 * @file Thema.hpp
 * @brief Hell, dunkel oder wie das System: das Farbschema der Anwendung
 *
 * Gesetzt wird es ueber Qt (QStyleHints::setColorScheme): Qt faerbt die
 * Standard-Widgets, und "System" folgt dem Betriebssystem auch waehrend des
 * Laufs. Was selbst zeichnet (die Vorschau), liest seine Farben aus der Palette
 * und zeichnet neu, wenn sie wechselt (QEvent::PaletteChange) — eine eigene
 * Farbtabelle gibt es hier nicht.
 *
 * Unter Windows 10 kennt der Stil `windowsvista` kein Farbschema; dort wird auf
 * Fusion gewechselt (nicht, wenn QT_STYLE_OVERRIDE gesetzt ist).
 */
#pragma once

#include <QString>

#include <array>
#include <optional>

namespace c2d::ui::thema
{

enum class Thema
{
    System,
    Hell,
    Dunkel,
};

struct Art
{
    Thema thema;
    const char* id;     ///< so steht es in den Einstellungen: `system`, `light`, `dark`
    const char* titel;  ///< so steht es im Dialog
};

/// Alle, in der Reihenfolge des Dialogs.
[[nodiscard]] const std::array<Art, 3>& arten();
[[nodiscard]] std::optional<Thema> ausId(const QString& id);
[[nodiscard]] QString idVon(Thema thema);

/// Das Farbschema setzen; Qt faerbt neu und schickt den Widgets PaletteChange.
void wende(Thema thema);
/// Was zuletzt gesetzt wurde (vor dem ersten wende(): System).
[[nodiscard]] Thema gilt();

}  // namespace c2d::ui::thema
