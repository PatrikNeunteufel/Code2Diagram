/**
 * @file Struktogramm.hpp
 * @brief Das Diagramm-Modell eines Struktogramms (Nassi-Shneiderman, DIN 66261): ein Baum aus Bloecken
 *
 * Abgeleitet aus dem Rumpf einer Funktion (diagramm::struktogrammVon), nicht aus
 * ihrem Ablauf: die Struktur steht im Codemodell schon fertig. Je Anweisung ein
 * Block; Verzweigung, Fallunterscheidung, Schleifen, try, Varianten und bedingte
 * Bereiche halten ihre Bloecke in Spalten.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

enum class BlockArt
{
    Anweisung,
    Aufruf,              ///< eine Anweisung, die nur eine Funktion ruft
    Aussprung,           ///< break, continue, return, throw, goto, `falls through`
    Marke,               ///< `name:`
    Verzweigung,         ///< if: Spalten `yes`, `no`
    Fallunterscheidung,  ///< switch: je Fall eine Spalte, Titel = die Werte
    Kopfschleife,        ///< while, for, range-for: eine Spalte, der Rumpf
    Fussschleife,        ///< do ... while
    Endlosschleife,      ///< immer wahr (`for (;;)`, `while (1)`): `forever`
    Versuch,             ///< try: Spalte `try`, je catch eine Spalte `catch (...)`, dann `else`, `finally`
    /// Ein Abschnitt unter einem Kopf (with; das else einer Schleife): eine Spalte, Titel = der Kopf —
    /// gezeichnet wie ein Abschnitt von try
    Abschnitt,
    Varianten,           ///< #if als Varianten: je Zweig eine Spalte mit seiner Bedingung
    Bedingt,             ///< der geltende Zweig eines #if (Rahmen): eine Spalte; text = die Bedingung
};

struct Block;

struct Spalte
{
    std::string titel;  ///< `yes`, die Werte eines Falls, `catch (...)`, die Bedingung eines Zweigs
    std::vector<Block> bloecke;
    bool gilt = false;  ///< nur Varianten: der Zweig gilt mit den Definitionen
};

struct Block
{
    BlockArt art = BlockArt::Anweisung;
    std::string text;  ///< eine Zeile: die Anweisung, die Bedingung, der Kopf einer Schleife
    int quellZeile = 0;
    /// Die Funktionen, die der Text ruft (wie Knoten::aufrufe).
    std::vector<std::string> aufrufe;
    std::vector<std::string> innere;  ///< wie Knoten::innere
    std::vector<Spalte> spalten;
    /// Kein Weg vom Start fuehrt hierher (wie Knoten::unerreichbar, aus dem Ablauf).
    bool unerreichbar = false;
};

/// Was das Struktogramm nicht richtig zeigen kann (ein goto), mit Zeile.
struct Hinweis
{
    int zeile = 0;
    std::string text;
};

struct Struktogramm
{
    std::string titel;  ///< die Signatur
    int quellZeile = 0;
    /// Kein Weg fuehrt aus der Funktion zurueck (wie im Ablauf: kein Ende).
    bool kehrtNieZurueck = false;
    std::vector<Block> bloecke;
    std::vector<Hinweis> hinweise;
};

/// Alle Bloecke, der Reihe nach von oben (ein Block vor den Bloecken seiner Spalten):
/// ihre Stelle hier ist ihre Kennung, wie KnotenId im Ablauf.
[[nodiscard]] std::vector<const Block*> bloeckeVon(const Struktogramm& diagramm);

/// Wie knotenZurZeile: der Block, der dort beginnt, sonst der, der zuletzt davor beginnt; kKeine = keiner.
[[nodiscard]] std::size_t blockZurZeile(const Struktogramm& diagramm, int zeile);

}  // namespace c2d::diagramm
