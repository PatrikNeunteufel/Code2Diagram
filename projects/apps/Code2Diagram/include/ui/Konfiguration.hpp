/**
 * @file Konfiguration.hpp
 * @brief Das Feld Configuration: wonach die Bedingungen einer Datei fragen, und welcher Zweig gilt
 *
 * Oben die Namen (sprache::Schalter) als Haken — mit einem Wertfeld, wo ein Name
 * verglichen oder verrechnet wird. Sie sind die Wahrheit: was hier gilt, steht im
 * Feld Defines. Darunter die Ketten #if / #elif / #else (sprache::Weiche) der
 * gezeigten Funktion (oder der ganzen Datei) mit je einem Knopf je Zweig: er
 * zeigt, welcher gilt; ein Klick setzt die Namen, die ihn waehlen — wo sich das
 * ableiten laesst, sonst ist er gesperrt und der Tooltip nennt die Bedingung.
 * Gesperrt ist nur, was die Datei selbst oder die Sprache definiert; was die
 * Projektdatei oder ein Kopf definiert, hebt ein Abwaehlen mit `!NAME` in
 * Defines auf.
 *
 * Das Feld rechnet nichts selbst: was ein Zweig verlangt, kommt aus dem Kern
 * (Weiche::wahl), wie es die Definitionen aendert, auch (sprache::mitForderungen).
 * Es meldet nur den neuen Text fuer Defines (beiAenderung); wer es zeigt, liest neu.
 */
#pragma once

#include "c2d/projekt/Projekt.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <optional>
#include <vector>

class QCheckBox;
class QPushButton;
class QScrollArea;

namespace c2d::ui
{

class Konfiguration : public QWidget
{
public:
    explicit Konfiguration(QWidget* parent = nullptr);

    /// Zeigt Schalter und Weichen von `ergebnis` (nullptr: nichts). `aussen` sind die
    /// Definitionen im Feld Defines, `darunter` die, die vor ihnen gelten (Projektdatei,
    /// Abschnitte, Koepfe: projekt::Datei::definitionen); `von` / `bis` die Zeilen der
    /// gezeigten Funktion (0: keine — dann die Weichen der ganzen Datei); `projekt`: ein
    /// Ordner ist offen.
    void zeige(const sprache::Ergebnis* ergebnis, std::vector<sprache::Definition> aussen,
               std::vector<sprache::Definition> darunter, int von, int bis, bool projekt);

    /// Woher ein Name der gezeigten Datei kommt (fuer Vermerk und Tooltip); ohne: keine.
    std::function<projekt::Herkunft(const std::string&)> herkunft;

    /// Der neue Text fuer das Feld Defines.
    std::function<void(const QString&)> beiAenderung;
    /// Der Knopf "Save to project file".
    std::function<void()> beimSpeichern;

    /// Wie der Haken eines Namens; `wert` fuer einen Namen, der einen Wert braucht (leer:
    /// der bisherige, sonst 1). false = gesperrt oder kein solcher Name.
    bool setzeSchalter(const QString& name, bool an, const QString& wert = {});
    /// Den Zweig `zweig` (ab 0; die Zahl der Zweige = das fehlende #else) der Weiche in
    /// Zeile `zeile` waehlen. false = keine solche, oder er laesst sich nicht waehlen.
    bool waehleZweig(int zeile, int zweig);

    /// Je Name `NAME`, `NAME=wert` (definiert; immer mit Wert, wenn er einen braucht) oder `-NAME`; ` (locked)` dahinter, wenn
    /// er sich hier nicht aendern laesst (die Datei oder die Sprache definiert ihn).
    [[nodiscard]] QStringList schalter() const;
    /// Je gezeigte Weiche `zeile: a | [b] | (c)` — in [ ] der Zweig, der gilt, in ( ) einer,
    /// der sich nicht waehlen laesst; das fehlende #else heisst `(no #else)`.
    [[nodiscard]] QStringList weichen() const;

    /// Definitionen als Text fuer das Feld Defines: `NAME` (Wert 1), `NAME=wert` oder `!NAME`.
    [[nodiscard]] static QString alsText(const std::vector<sprache::Definition>& definitionen);

private:
    void baue();
    bool wende(const std::vector<sprache::Forderung>& forderungen);
    [[nodiscard]] bool gesperrt(const sprache::Schalter& s) const;
    /// Die Definitionen, unter denen Zweig `zweig` der Weiche `w` gilt; nullopt = nicht zu waehlen.
    [[nodiscard]] std::optional<std::vector<sprache::Definition>> nachWahl(std::size_t w, std::size_t zweig) const;
    [[nodiscard]] std::vector<std::size_t> gezeigteWeichen() const;
    [[nodiscard]] QString zweigName(std::size_t w, std::size_t zweig) const;

    QCheckBox* m_ganzeDatei = nullptr;
    QScrollArea* m_flaeche = nullptr;
    QPushButton* m_speichern = nullptr;
    std::vector<sprache::Schalter> m_schalter;
    std::vector<sprache::Weiche> m_weichen;
    std::vector<sprache::Definition> m_aussen;
    std::vector<sprache::Definition> m_darunter;
    int m_von = 0;
    int m_bis = 0;
    bool m_projekt = false;
};

}  // namespace c2d::ui
