/**
 * @file Farben.cpp
 * @brief Die Tabelle der Farben eines Diagramms
 */
#include "c2d/diagramm/Farben.hpp"

namespace c2d::diagramm
{

namespace
{

// Getoent hebt sich auf beiden Gruenden deutlich ab (hell 23 Stufen, dunkel 18 — dunkel wirkt
// derselbe Abstand groesser).
constexpr std::array<FarbEintrag, 9> kTabelle{{
    {Farbe::Grund, "grund", "#ffffff", "#2b2b2b"},
    {Farbe::Flaeche, "flaeche", "#ffffff", "#2b2b2b"},
    {Farbe::Getoent, "getoent", "#e8e8e8", "#3d3d3d"},
    {Farbe::Strich, "strich", "#202020", "#dcdcdc"},
    {Farbe::Text, "text", "#202020", "#dcdcdc"},
    {Farbe::Rahmen, "rahmen", "#8a8a8a", "#8a8a8a"},
    {Farbe::Fremd, "fremd", "#f2f2f2", "#333333"},
    {Farbe::FremdStrich, "fremd-strich", "#8a8a8a", "#9a9a9a"},
    {Farbe::Verweis, "verweis", "#1a66cc", "#4c9aff"},
}};

}  // namespace

const std::array<FarbEintrag, 9>& farbTabelle()
{
    return kTabelle;
}

std::string_view farbe(Farbe welche, bool dunkel)
{
    const FarbEintrag& e = kTabelle[static_cast<std::size_t>(welche)];
    return dunkel ? e.dunkel : e.hell;
}

}  // namespace c2d::diagramm
