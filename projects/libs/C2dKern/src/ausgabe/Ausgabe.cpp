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
    static const Format* const liste[] = {&svg(), &mermaid(), &dot(), &drawio()};
    return liste;
}

const Format* format(std::string_view name)
{
    for (const Format* f : formate())
        if (f->name() == name) return f;
    return nullptr;
}

}  // namespace c2d::ausgabe
