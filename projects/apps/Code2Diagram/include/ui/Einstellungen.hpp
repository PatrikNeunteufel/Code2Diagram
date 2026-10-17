/**
 * @file Einstellungen.hpp
 * @brief File > Settings: was die Anwendung ueber Neustarts behaelt, und der Dialog dazu
 *
 * Jede Einstellung steht in einer Tabelle (alle()): Schluessel in QSettings,
 * Seite und Titel im Dialog, Hilfe, Vorgabe und die erlaubten Werte. Der Dialog
 * baut seine Seiten aus ihr — eine neue Einstellung ist eine Zeile der Tabelle
 * und, wenn sie etwas bewirkt, eine in wendeAn().
 *
 * Gelesen wird ueber wert(): der gespeicherte Wert, wenn er erlaubt ist, sonst
 * die Vorgabe. Gespeichert wird erst mit OK oder Apply.
 */
#pragma once

#include <QDialog>
#include <QString>

#include <vector>

class QComboBox;

namespace c2d::ui
{

struct Wahl
{
    QString id;     ///< so steht es in QSettings
    QString titel;  ///< so steht es im Dialog
};

struct Einstellung
{
    QString schluessel;  ///< in QSettings, etwa `aussehen/thema`
    QString seite;       ///< die Seite im Dialog
    QString titel;
    QString hilfe;       ///< Tooltip
    QString vorgabe;     ///< eine der `wahl`
    std::vector<Wahl> wahl;
};

namespace einstellungen
{

/// Das Thema: `system`, `light` oder `dark` (thema::arten()).
inline constexpr const char* kThema = "aussehen/thema";

/// Alle Einstellungen, in der Reihenfolge des Dialogs.
[[nodiscard]] const std::vector<Einstellung>& alle();
/// nullptr = keine mit diesem Schluessel.
[[nodiscard]] const Einstellung* finde(const QString& schluessel);
/// Der gespeicherte Wert, wenn er erlaubt ist, sonst die Vorgabe; leer = kein solcher Schluessel.
[[nodiscard]] QString wert(const QString& schluessel);
/// Speichern. false = kein solcher Schluessel, oder der Wert ist nicht erlaubt (dann bleibt alles).
bool setze(const QString& schluessel, const QString& wert);
/// Bewirken, was gespeichert ist: das Thema setzen.
void wendeAn();

}  // namespace einstellungen

/// Links die Seiten, rechts ihre Felder; OK, Cancel, Apply.
class EinstellungenDialog : public QDialog
{
public:
    explicit EinstellungenDialog(QWidget* parent = nullptr);

    /// Das Feld einer Einstellung (fuer Tests und den Tester); nullptr = keine solche.
    [[nodiscard]] QComboBox* feld(const QString& schluessel) const;
    /// Wie Apply: was in den Feldern steht, speichern und bewirken.
    void uebernimm();

private:
    std::vector<QComboBox*> m_felder;  ///< je Einstellung von alle() eines
};

}  // namespace c2d::ui
