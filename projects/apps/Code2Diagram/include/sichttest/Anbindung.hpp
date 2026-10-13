/**
 * @file Anbindung.hpp
 * @brief Sichttest-Steuerung: der Tester `Sichttest.exe` loest in Code2Diagram
 *        Aktionen aus (Spezifikation: SichtTest_Helper `docs/Konzept_Steuerung.md`)
 *
 * Nur mit dem Schalter `--testing` laedt die Anwendung die
 * `SichttestSteuerung1.dll` aus dem Ordner der Exe — gelinkt wird nichts, ein
 * gewoehnlicher Start laedt nichts. Die Koepfe des Pakets stehen allein in
 * `Anbindung.cpp`, hinter `SICHTTEST_VORHANDEN`; dieser Kopf traegt nur Typen
 * der Anwendung. Muster: LumiViz `include/sichttest/Anbindung.hpp`.
 *
 * Threads: alles im GUI-Thread. Die Aktionen laufen dort, wenn der Tester sie
 * ausloest — nie, bevor `verbinde()` gerufen wurde.
 */
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace c2d::sichttest
{

/// Was eine Aktion dem Tester antwortet: erledigt, gescheitert oder
/// "die Argumente passen nicht" — dazu ein Satz, der im Tester am Punkt steht.
struct Ergebnis
{
    enum class Status
    {
        Ok,
        Fehler,
        Ungueltig,
    };
    Status status = Status::Ok;
    QString text;
};

[[nodiscard]] inline Ergebnis ok(const QString& text = {}) { return {Ergebnis::Status::Ok, text}; }
[[nodiscard]] inline Ergebnis fehler(const QString& text) { return {Ergebnis::Status::Fehler, text}; }
[[nodiscard]] inline Ergebnis ungueltig(const QString& text)
{
    return {Ergebnis::Status::Ungueltig, text};
}

/// Steht `--testing` unter den Argumenten?
[[nodiscard]] bool verlangt(const QStringList& argumente);

/// Der Root des Projekts: von `ab` aufwaerts der erste Ordner, der
/// `.sichttest/sichttest.projekt.json` enthaelt — dieselbe Regel wie im
/// Tester. Leer = keiner gefunden.
[[nodiscard]] QString wurzel(const QString& ab);

/// Eine Datei aus den Argumenten einer Aktion: absolut wie angegeben, relativ
/// ab `root`. Leer = `angabe` ist leer oder benennt keine vorhandene Datei.
[[nodiscard]] QString loeseAuf(const QString& angabe, const QString& root);

/// Der erste nicht leere Wert unter `schluessel`, sonst unter `wert` (ein
/// einzelner Wert ohne Schluessel kommt als `wert` an).
[[nodiscard]] QString argument(const QJsonObject& argumente, const QString& schluessel);

class Anbindung
{
public:
    using Aktion = std::function<Ergebnis(const QJsonObject& argumente)>;

    /// Die DLL aus dem Ordner der Exe laden und die Sitzung anlegen. Leer =
    /// es gibt keine Steuerung (ohne das Paket gebaut, die DLL fehlt, die
    /// Fassung passt nicht); `grund` nennt dann einen Satz dazu.
    [[nodiscard]] static std::unique_ptr<Anbindung> lade(const QString& anwendung,
                                                         const QString& version,
                                                         QString* grund = nullptr);
    ~Anbindung();
    Anbindung(const Anbindung&) = delete;
    Anbindung& operator=(const Anbindung&) = delete;

    /// Eine Aktion anmelden — vor oder nach `verbinde()`. false = der Name
    /// ist ungueltig oder schon vergeben.
    bool aktion(const QString& name, const QString& beschreibung, const QString& parameter,
                Aktion aktion);

    /// Mit dem Tester verbinden; startet ihn, wenn keiner lauscht. Kehrt
    /// sofort zurueck — ab hier kann die Anwendung gesteuert werden.
    bool verbinde();
    [[nodiscard]] QString letzterFehler() const;
    /// Der Root des Projekts, ab dem die Aktionen ihre Dateien suchen.
    [[nodiscard]] const QString& root() const;

private:
    Anbindung();
    struct Innen;
    std::unique_ptr<Innen> m;
};

}  // namespace c2d::sichttest
