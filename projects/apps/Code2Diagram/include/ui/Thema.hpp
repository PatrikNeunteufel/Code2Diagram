/**
 * @file Thema.hpp
 * @brief Hell, dunkel oder wie das System: das Farbschema der Anwendung
 *
 * Der Stil ist Fusion, hell wie dunkel, unter Windows 10 und 11 gleich (nicht,
 * wenn QT_STYLE_OVERRIDE gesetzt ist): der Stil von Windows 11 zeichnet hell
 * Auswahl- und Zahlenfelder ohne sichtbaren Rand. Die Palette je hell und dunkel
 * steht als Tabelle in Thema.cpp. Das Farbschema geht zusaetzlich an Qt
 * (QStyleHints::setColorScheme) — die Titelleiste folgt ihm —, und bei "System"
 * folgt die Palette dem Betriebssystem auch waehrend des Laufs
 * (QStyleHints::colorSchemeChanged). Was selbst zeichnet (die Vorschau),
 * zeichnet neu, wenn die Palette wechselt (QEvent::PaletteChange).
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

/// Stil, Farbschema und Palette setzen; Qt schickt den Widgets PaletteChange.
void wende(Thema thema);
/// Was zuletzt gesetzt wurde (vor dem ersten wende(): System).
[[nodiscard]] Thema gilt();

}  // namespace c2d::ui::thema
