/**
 * @file Ausgabe.cpp
 * @brief Die Liste der Formate
 */
#include "c2d/ausgabe/Ausgabe.hpp"

namespace c2d::ausgabe
{

std::span<const Format* const> formate()
{
    // Ein neues Format hier eintragen.
    // Das eigene Format zuerst: es zeigt, was die Vorschau zeigt (Entscheid Patrik, 2026-10-15).
    static const Format* const liste[] = {&svg(), &mermaid(), &dot(), &drawio(), &csv()};
    return liste;
}

const Format* format(std::string_view name)
{
    for (const Format* f : formate())
        if (f->name() == name) return f;
    return nullptr;
}

std::string_view nameVon(Diagrammart art)
{
    switch (art)
    {
        case Diagrammart::Ablauf: return "Flowchart";
        case Diagrammart::Datentypen: return "Data types";
        case Diagrammart::Zustandsautomat: return "State machine";
        case Diagrammart::Aufrufgraph: return "Call graph";
        case Diagrammart::Struktogramm: return "Nassi-Shneiderman";
        case Diagrammart::Wahrheitstabelle: return "Truth table";
        case Diagrammart::Entscheidungstabelle: return "Decision table";
        case Diagrammart::Zuordnung: return "Mapping table";
        case Diagrammart::Konfiguration: return "#if configuration";
        case Diagrammart::Schaltstellen: return "#if switch points";
        case Diagrammart::Schalteruebersicht: return "#if names";
    }
    return {};
}

std::string nichtVerfuegbar(const Format& format, Diagrammart art)
{
    return std::string(nameVon(art)) + " is not available in " + std::string(format.name());
}

}  // namespace c2d::ausgabe
