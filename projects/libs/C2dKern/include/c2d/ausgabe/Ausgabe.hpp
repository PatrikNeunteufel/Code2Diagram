/**
 * @file Ausgabe.hpp
 * @brief Ausgabeformate: ein Diagramm als Text in einem Format schreiben
 *
 * Ein Format kennt die Diagramm-Modelle (diagramm/), aber weder eine Sprache
 * noch das Codemodell. Ein neues Format: eine Klasse von `Format` ableiten und
 * in Ausgabe.cpp in die Liste eintragen. Eine neue Diagrammart bekommt hier
 * eine weitere Funktion `schreibe` und einen Wert in `Diagrammart` — der
 * Uebersetzer nennt dann jedes Format, das sie noch nicht kann. Was ein Format
 * nicht richtig zeigen kann (eine Tabelle in Mermaid, einen Ablauf in csv),
 * verneint es in kann(); wer schreibt, fragt vorher und sagt sonst nichtVerfuegbar().
 */
#pragma once

#include "c2d/diagramm/Aufrufgraph.hpp"
#include "c2d/diagramm/Entscheidungstabelle.hpp"
#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"
#include "c2d/diagramm/Struktogramm.hpp"
#include "c2d/diagramm/Wahrheitstabelle.hpp"
#include "c2d/diagramm/Zustandsdiagramm.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace c2d::ausgabe
{

struct Optionen
{
    /// Wohin das Diagramm waechst: TD (nach unten) oder LR (nach rechts).
    std::string_view richtung = "TD";
    /// Laengste Zeile eines Knotens oder Eintrags in Zeichen (Bytes); laengere
    /// enden auf "...". 0 = nicht kuerzen.
    std::size_t maxZeilenLaenge = 60;
};

enum class Diagrammart
{
    Ablauf,
    Datentypen,
    Zustandsautomat,
    Aufrufgraph,
    Struktogramm,
    Wahrheitstabelle,
    Entscheidungstabelle,
};

/// Wie die Diagrammart heisst, wo sie gemeldet wird: `Flowchart`, `Truth table` ...
[[nodiscard]] std::string_view nameVon(Diagrammart art);

class Format
{
public:
    virtual ~Format() = default;

    /// Der Name, unter dem das Format gewaehlt wird, klein geschrieben: `mermaid`, `dot`.
    [[nodiscard]] virtual std::string_view name() const = 0;
    /// Die uebliche Dateiendung ohne Punkt.
    [[nodiscard]] virtual std::string_view endung() const = 0;

    /// Kann das Format diese Art richtig zeigen? Nur dann ist `schreibe` fuer sie gemeint
    /// (sonst liefert es leeren Text).
    [[nodiscard]] virtual bool kann(Diagrammart art) const = 0;

    /// Gleiche Eingabe gibt gleichen Text.
    [[nodiscard]] virtual std::string schreibe(const diagramm::Flussdiagramm& diagramm,
                                               const Optionen& optionen) const = 0;
    [[nodiscard]] virtual std::string schreibe(const diagramm::Strukturdiagramm& diagramm,
                                               const Optionen& optionen) const = 0;
    [[nodiscard]] virtual std::string schreibe(const diagramm::Zustandsdiagramm& diagramm,
                                               const Optionen& optionen) const = 0;
    [[nodiscard]] virtual std::string schreibe(const diagramm::Aufrufgraph& graph, const Optionen& optionen) const = 0;
    [[nodiscard]] virtual std::string schreibe(const diagramm::Struktogramm& diagramm, const Optionen& optionen) const = 0;
    /// Eine oder mehrere Wahrheitstabellen, untereinander.
    [[nodiscard]] virtual std::string schreibe(std::span<const diagramm::Wahrheitstabelle> tabellen,
                                               const Optionen& optionen) const = 0;
    [[nodiscard]] virtual std::string schreibe(const diagramm::Entscheidungstabelle& tabelle,
                                               const Optionen& optionen) const = 0;
};

/// Alle Formate; das erste ist die Vorgabe.
[[nodiscard]] std::span<const Format* const> formate();

/// Das Format dieses Namens; nullptr = keines.
[[nodiscard]] const Format* format(std::string_view name);

/// Was gesagt wird, wenn ein Format eine Art nicht kann: `Truth table is not available in mermaid`.
[[nodiscard]] std::string nichtVerfuegbar(const Format& format, Diagrammart art);

/// SVG: das Diagramm gezeichnet wie in der Vorschau, mit den Orten und Wegen aus
/// diagramm/Anordnung.hpp — das eigene Format, die Vorgabe.
[[nodiscard]] const Format& svg();

/// Mermaid: `flowchart` (auch der Aufrufgraph), `classDiagram` und `stateDiagram-v2`.
[[nodiscard]] const Format& mermaid();

/// Graphviz DOT: `digraph`, Datentypen als `record`.
[[nodiscard]] const Format& dot();

/// CSV (Trenner `;`, wie Excel unter deutscher Einstellung): nur Tabellen — fuer Testplaene.
/// Wahrheitstabelle: je Tabelle eine Zeile mit Funktion, Zeile und Bedingung, dann Kopf und Zeilen.
/// Entscheidungstabelle: so ausgerichtet wie gezeichnet (Regeln als Spalten).
[[nodiscard]] const Format& csv();

/// draw.io (diagrams.net): mxGraph-XML mit festen Orten aus diagramm/Anordnung.hpp.
[[nodiscard]] const Format& drawio();

}  // namespace c2d::ausgabe
