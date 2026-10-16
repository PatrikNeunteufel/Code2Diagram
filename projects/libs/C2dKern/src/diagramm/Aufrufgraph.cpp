/**
 * @file Aufrufgraph.cpp
 * @brief Das Diagramm-Modell eines Aufrufgraphen: Beschriftung, Farben, Text
 */
#include "c2d/diagramm/Aufrufgraph.hpp"

#include <algorithm>
#include <string_view>

namespace c2d::diagramm
{

std::string beschriftung(const AufrufKante& kante)
{
    const bool mehrdeutig = kante.art == AufrufKantenArt::Mehrdeutig;
    if (kante.gilt) return mehrdeutig ? "?" : std::string();
    return mehrdeutig ? "? " + kante.bedingung : kante.bedingung;
}

std::vector<std::size_t> farbenDerBedingungen(const Aufrufgraph& graph)
{
    std::vector<std::size_t> aus;
    std::vector<std::string_view> bedingungen;  // in der Reihenfolge ihres ersten Vorkommens
    for (const AufrufKante& k : graph.kanten)
    {
        if (!k.gilt || k.bedingung.empty())
        {
            aus.push_back(kKeine);
            continue;
        }
        auto stelle = std::find(bedingungen.begin(), bedingungen.end(), k.bedingung);
        if (stelle == bedingungen.end()) stelle = bedingungen.insert(bedingungen.end(), k.bedingung);
        aus.push_back(static_cast<std::size_t>(stelle - bedingungen.begin()) % kRahmenFarben);
    }
    return aus;
}

std::string knotenText(const AufrufKnoten& knoten)
{
    // die Auslassungspunkte U+2026 als Bytes: die Quelle bleibt reines ASCII
    return knoten.weiter ? knoten.name + " \xE2\x80\xA6" : knoten.name;
}

}  // namespace c2d::diagramm
