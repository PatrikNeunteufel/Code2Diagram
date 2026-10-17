/**
 * @file Struktogramm.cpp
 * @brief Das Struktogramm: Bloecke der Reihe nach, der Block zu einer Zeile
 */
#include "c2d/diagramm/Struktogramm.hpp"

namespace c2d::diagramm
{

namespace
{

void sammle(const std::vector<Block>& bloecke, std::vector<const Block*>& aus)
{
    for (const Block& b : bloecke)
    {
        aus.push_back(&b);
        for (const Spalte& s : b.spalten) sammle(s.bloecke, aus);
    }
}

}  // namespace

std::vector<const Block*> bloeckeVon(const Struktogramm& diagramm)
{
    std::vector<const Block*> aus;
    sammle(diagramm.bloecke, aus);
    return aus;
}

std::size_t blockZurZeile(const Struktogramm& diagramm, int zeile)
{
    const std::vector<const Block*> alle = bloeckeVon(diagramm);
    std::size_t bester = kKeine;
    int besteZeile = 0;
    for (std::size_t i = 0; i < alle.size(); ++i)
    {
        const int z = alle[i]->quellZeile;
        if (z <= 0 || z > zeile) continue;
        if (z == zeile) return i;  // der aeussere zuerst: ein Kopf vor seinem Rumpf in derselben Zeile
        if (z > besteZeile)
        {
            besteZeile = z;
            bester = i;
        }
    }
    return bester;
}

}  // namespace c2d::diagramm
