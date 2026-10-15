/**
 * @file Vorschau.hpp
 * @brief Das Diagramm gezeichnet — mit den Orten aus dem Kern (c2d/diagramm/Anordnung.hpp)
 *
 * Die Vorschau zeichnet nur; was ein Klick bedeutet, entscheidet, wer die
 * Rueckrufe setzt (das Hauptfenster). Strg + Mausrad vergroessert, Ziehen
 * verschiebt.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"

#include <QGraphicsView>

#include <functional>
#include <vector>

namespace c2d::ui
{

class Vorschau : public QGraphicsView
{
public:
    explicit Vorschau(QWidget* parent = nullptr);

    /// `verweis[i]`: der Knoten i fuehrt irgendwohin (er ruft eine Funktion, die
    /// sich zeigen laesst) — er wird hervorgehoben. Darf kuerzer sein als die Knoten.
    void zeige(const c2d::diagramm::Flussdiagramm& diagramm, bool nachRechts, const std::vector<bool>& verweis = {});
    void zeige(const c2d::diagramm::Strukturdiagramm& diagramm, bool nachRechts);
    void leere();

    /// Wie viele Knoten oder Kaesten gezeichnet sind.
    [[nodiscard]] int formen() const { return m_formen; }
    /// Wie viele Rahmen (Gruppen) gezeichnet sind.
    [[nodiscard]] int rahmen() const { return m_rahmen; }
    /// Die ganze Szene als Bild, Massstab 1, auf dem Grund der Vorschau.
    [[nodiscard]] QImage alsBild() const;

    /// Doppelklick auf einen Knoten des Flussdiagramms.
    std::function<void(c2d::diagramm::KnotenId knoten)> beiDoppelklick;
    /// Rechtsklick: der Knoten und die innerste Gruppe unter dem Zeiger (je kKeine = keine), der Ort am Bildschirm.
    std::function<void(c2d::diagramm::KnotenId knoten, std::size_t gruppe, const QPoint& ort)> beiKontext;

protected:
    void mouseDoubleClickEvent(QMouseEvent* ereignis) override;
    void contextMenuEvent(QContextMenuEvent* ereignis) override;
    void wheelEvent(QWheelEvent* ereignis) override;

private:
    /// Knoten und innerste Gruppe an einem Ort der Ansicht.
    void trefferBei(const QPoint& ort, c2d::diagramm::KnotenId& knoten, std::size_t& gruppe) const;

    int m_formen = 0;
    int m_rahmen = 0;
};

}  // namespace c2d::ui
