/**
 * @file Vorschau.hpp
 * @brief Das Diagramm gezeichnet — mit den Orten aus dem Kern (c2d/diagramm/Anordnung.hpp)
 *
 * Die Vorschau zeichnet nur; was ein Klick bedeutet, entscheidet, wer die
 * Rueckrufe setzt (das Hauptfenster). Strg + Mausrad vergroessert, Ziehen
 * verschiebt.
 */
#pragma once

#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Aufrufgraph.hpp"
#include "c2d/diagramm/Entscheidungstabelle.hpp"
#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"
#include "c2d/diagramm/Struktogramm.hpp"
#include "c2d/diagramm/Wahrheitstabelle.hpp"
#include "c2d/diagramm/Zuordnung.hpp"
#include "c2d/diagramm/Zustandsdiagramm.hpp"

#include <QGraphicsView>
#include <QRectF>

class QGraphicsItem;

#include <functional>
#include <span>
#include <vector>

namespace c2d::ui
{

class Vorschau : public QGraphicsView
{
public:
    explicit Vorschau(QWidget* parent = nullptr);

    /// `links[i]`: die Namen im Text des Knotens i, die irgendwohin fuehren (eine Funktion, ein
    /// Typ) — sie und der Knoten werden hervorgehoben. Darf kuerzer sein als die Knoten.
    /// `tabellen[i]` nicht leer: Knoten i traegt rechts oben ein Tabellensymbol (beiTabelle), der
    /// Text ist sein Tooltip.
    void zeige(const c2d::diagramm::Flussdiagramm& diagramm, bool nachRechts,
               const std::vector<std::vector<std::string>>& links = {}, const std::vector<QString>& tabellen = {});
    /// `links[k][e]`: die Namen im Glied e des Kastens k, die zu einem Typ fuehren — sie
    /// werden hervorgehoben. Darf kuerzer sein als die Kaesten.
    void zeige(const c2d::diagramm::Strukturdiagramm& diagramm, bool nachRechts,
               const std::vector<std::vector<std::vector<std::string>>>& links = {});
    void zeige(const c2d::diagramm::Zustandsdiagramm& diagramm, bool nachRechts);
    /// Ein Knoten des Aufrufgraphen ist fuer Doppelklick, Kontextmenue und markiere()
    /// wie ein Knoten des Flussdiagramms: seine Stelle in Aufrufgraph::knoten.
    void zeige(const c2d::diagramm::Aufrufgraph& graph, bool nachRechts);
    /// Ein Struktogramm; ein Block ist fuer Klick, Doppelklick, Kontextmenue und markiere() ein
    /// Knoten (seine Stelle in bloeckeVon()). `links[b]`: die Namen in Block b, die weiterfuehren.
    void zeige(const c2d::diagramm::Struktogramm& diagramm, const std::vector<std::vector<std::string>>& links = {},
               const std::vector<QString>& tabellen = {});
    /// Wahrheitstabellen untereinander; ein Klick auf eine Zelle ist ein Klick auf Knoten 0.
    void zeige(std::span<const c2d::diagramm::Wahrheitstabelle> tabellen);
    /// Eine Entscheidungstabelle; jede Zeile ist ein Knoten (erst die Bedingungen, dann die Aktionen).
    /// `tabellen[k]` nicht leer: Zeile k traegt das Tabellensymbol, der Text ist sein Tooltip.
    void zeige(const c2d::diagramm::Entscheidungstabelle& tabelle, const std::vector<QString>& tabellen = {});
    /// Zuordnungen untereinander; jede Zeile ist ein Knoten (gezaehlt ueber alle Tabellen).
    void zeige(std::span<const c2d::diagramm::Zuordnung> tabellen);
    /// Konfigurationstabellen untereinander; jede Zeile ist ein Knoten (gezaehlt ueber alle Tabellen).
    void zeige(std::span<const c2d::diagramm::Konfigurationstabelle> tabellen);
    /// Wo ein Name schaltet; Knoten erst je Setzung, dann je Zeile.
    void zeige(const c2d::diagramm::Schaltstellen& tabelle);
    /// Die Namen des Projekts; jede Zeile ist ein Knoten.
    void zeige(const c2d::diagramm::Schalteruebersicht& tabelle);
    void leere();

    /// Wie viele Knoten oder Kaesten gezeichnet sind.
    [[nodiscard]] int formen() const { return m_formen; }
    /// Wie viele Rahmen (Gruppen) gezeichnet sind.
    [[nodiscard]] int rahmen() const { return m_rahmen; }
    /// Die Tooltips der gezeichneten Tabellensymbole, von oben nach unten.
    [[nodiscard]] QStringList tabellenSymbole() const;
    /// Die ganze Szene als Bild, Massstab 1, auf dem Grund der Vorschau.
    [[nodiscard]] QImage alsBild() const;

    /// Einen Knoten des Flussdiagramms (des Aufrufgraphen) hervorheben (ein Ring) und ins Bild ruecken;
    /// kKeine nimmt die Marke weg. false = so einen Knoten gibt es nicht.
    bool markiere(c2d::diagramm::KnotenId knoten);
    /// Der hervorgehobene Knoten; kKeine = keiner.
    [[nodiscard]] c2d::diagramm::KnotenId markiert() const { return m_markiert; }

    /// Klick (ohne zu ziehen) auf einen Knoten des Flussdiagramms: seine Stelle im Quelltext.
    std::function<void(c2d::diagramm::KnotenId knoten)> beiKlick;
    /// Klick auf das Tabellensymbol eines Knotens (oder Blocks): seine Wahrheitstabelle.
    std::function<void(c2d::diagramm::KnotenId knoten)> beiTabelle;
    /// Doppelklick auf einen Knoten des Flussdiagramms: was er nennt. Ihm geht ein Klick voraus.
    std::function<void(c2d::diagramm::KnotenId knoten)> beiDoppelklick;
    /// Rechtsklick: der Knoten und die innerste Gruppe unter dem Zeiger (je kKeine = keine), der Ort am Bildschirm.
    std::function<void(c2d::diagramm::KnotenId knoten, std::size_t gruppe, const QPoint& ort)> beiKontext;
    /// Im Strukturdiagramm: Klick auf einen Kasten; `eintrag` = das Glied darin (erst die Felder,
    /// dann die Methoden), kKeine = der Kopf.
    std::function<void(std::size_t kasten, std::size_t eintrag)> beiKlickKasten;
    /// Im Strukturdiagramm: Doppelklick auf einen Kasten (Kasten und Glied wie oben).
    std::function<void(std::size_t kasten, std::size_t eintrag)> beiDoppelklickKasten;
    /// Im Strukturdiagramm: Rechtsklick (Kasten und Glied wie oben; kasten kKeine = daneben).
    std::function<void(std::size_t kasten, std::size_t eintrag, const QPoint& ort)> beiKontextKasten;

protected:
    void mousePressEvent(QMouseEvent* ereignis) override;
    void mouseReleaseEvent(QMouseEvent* ereignis) override;
    void mouseDoubleClickEvent(QMouseEvent* ereignis) override;
    void contextMenuEvent(QContextMenuEvent* ereignis) override;
    void wheelEvent(QWheelEvent* ereignis) override;

private:
    /// Eine Tabelle zeichnen (Entscheidungstabelle, Zuordnung): Zellen, Linien, Texte, Knoten je Zeile.
    void zeigeTabelle(const c2d::diagramm::TabellenAnordnung& a, const std::vector<QString>& tabellen);
    /// Ist das Fenster dunkel? Dann die dunkle Spalte der Farbtabelle (diagramm/Farben.hpp).
    [[nodiscard]] bool dunkel() const;
    /// Knoten und innerste Gruppe an einem Ort der Ansicht.
    void trefferBei(const QPoint& ort, c2d::diagramm::KnotenId& knoten, std::size_t& gruppe) const;
    /// Kasten und Glied an einem Ort der Ansicht (je kKeine = keiner).
    void kastenBei(const QPoint& ort, std::size_t& kasten, std::size_t& eintrag) const;

    bool m_struktur = false;  ///< zeigt ein Strukturdiagramm

    QPoint m_gedrueckt;              ///< wo die linke Taste zuletzt gedrueckt wurde
    bool m_klickMoeglich = false;    ///< gedrueckt und seither kaum bewegt

    int m_formen = 0;
    int m_rahmen = 0;
    std::vector<QRectF> m_orte;  ///< je Knoten des Flussdiagramms sein Rechteck
    QGraphicsItem* m_marke = nullptr;
    c2d::diagramm::KnotenId m_markiert = c2d::diagramm::kKeine;
};

}  // namespace c2d::ui
