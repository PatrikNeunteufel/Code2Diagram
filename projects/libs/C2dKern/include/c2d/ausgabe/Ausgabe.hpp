/**
 * @file Ausgabe.hpp
 * @brief Ausgabeformate: ein Diagramm als Text in einem Format schreiben
 *
 * Ein Format kennt die Diagramm-Modelle (diagramm/), aber weder eine Sprache
 * noch das Codemodell. Ein neues Format: eine Klasse von `Format` ableiten und
 * in Ausgabe.cpp in die Liste eintragen. Eine neue Diagrammart bekommt hier
 * eine weitere Funktion `schreibe` — der Uebersetzer nennt dann jedes Format,
 * das sie noch nicht kann.
 */
#pragma once

#include "c2d/diagramm/Aufrufgraph.hpp"
#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"
#include "c2d/diagramm/Struktogramm.hpp"
#include "c2d/diagramm/Zustandsdiagramm.hpp"

#include <cstddef>
#include <optional>
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

class Format
{
public:
    virtual ~Format() = default;

    /// Der Name, unter dem das Format gewaehlt wird, klein geschrieben: `mermaid`, `dot`.
    [[nodiscard]] virtual std::string_view name() const = 0;
    /// Die uebliche Dateiendung ohne Punkt.
    [[nodiscard]] virtual std::string_view endung() const = 0;

    /// Gleiche Eingabe gibt gleichen Text.
    [[nodiscard]] virtual std::string schreibe(const diagramm::Flussdiagramm& diagramm,
                                               const Optionen& optionen) const = 0;
    [[nodiscard]] virtual std::string schreibe(const diagramm::Strukturdiagramm& diagramm,
                                               const Optionen& optionen) const = 0;
    [[nodiscard]] virtual std::string schreibe(const diagramm::Zustandsdiagramm& diagramm,
                                               const Optionen& optionen) const = 0;
    [[nodiscard]] virtual std::string schreibe(const diagramm::Aufrufgraph& graph, const Optionen& optionen) const = 0;
    /// Ein Struktogramm; std::nullopt = das Format kann es nicht richtig zeigen (Mermaid, DOT) —
    /// wer schreibt, sagt dann `Nassi-Shneiderman is not available in <name>`.
    [[nodiscard]] virtual std::optional<std::string> schreibe(const diagramm::Struktogramm& diagramm,
                                                              const Optionen& optionen) const = 0;
};

/// Alle Formate; das erste ist die Vorgabe.
[[nodiscard]] std::span<const Format* const> formate();

/// Das Format dieses Namens; nullptr = keines.
[[nodiscard]] const Format* format(std::string_view name);

/// Wie ein Format sagt, dass es ein Struktogramm nicht kann.
[[nodiscard]] std::string keinStruktogramm(const Format& format);

/// SVG: das Diagramm gezeichnet wie in der Vorschau, mit den Orten und Wegen aus
/// diagramm/Anordnung.hpp — das eigene Format, die Vorgabe.
[[nodiscard]] const Format& svg();

/// Mermaid: `flowchart` (auch der Aufrufgraph), `classDiagram` und `stateDiagram-v2`.
[[nodiscard]] const Format& mermaid();

/// Graphviz DOT: `digraph`, Datentypen als `record`.
[[nodiscard]] const Format& dot();

/// draw.io (diagrams.net): mxGraph-XML mit festen Orten aus diagramm/Anordnung.hpp.
[[nodiscard]] const Format& drawio();

}  // namespace c2d::ausgabe
