/**
 * @file Ableitung.hpp
 * @brief Vom Codemodell zum Diagramm
 *
 * Hier steht, WIE aus Code ein Diagramm wird — unabhaengig von der Sprache
 * (die steckt im Codemodell) und vom Format (das steckt in ausgabe/).
 * Eine neue Diagrammart bekommt hier ihre Funktion und in diagramm/ ihr Modell.
 */
#pragma once

#include "c2d/diagramm/Flussdiagramm.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"
#include "c2d/diagramm/Zustandsdiagramm.hpp"
#include "c2d/modell/Code.hpp"

#include <vector>

namespace c2d::diagramm
{

/// Der Ablauf einer Funktion: je Anweisung ein Knoten, ungebuendelt
/// (fasseAnweisungsfolgenZusammen macht es lesbar). Eine Funktion ohne Rumpf
/// gibt ein leeres Diagramm.
///
/// - if, while, do, for: eine Verzweigung mit den Kanten `yes` und `no`
/// - switch: eine Verzweigung, je Fall eine Kante mit seinem Wert; ohne
///   default eine Kante `otherwise`
/// - break, continue, goto: kein Knoten, die Kante fuehrt zum Ziel
/// - eine Marke: ein eigener Knoten `name:`
/// - eine Anweisung, die nur eine Funktion ruft: ein Knoten der Art Aufruf
/// - Praeprozessor-Zeilen im Rumpf: kein Knoten
[[nodiscard]] Flussdiagramm flussVon(const modell::Funktion& funktion);

/// Die Datentypen einer Einheit: je struct/union/enum ein Kasten. Ein Glied,
/// dessen Typ einen anderen Kasten nennt (auch ueber einen typedef), gibt eine
/// Beziehung: als Wert `Enthaelt`, ueber einen Zeiger oder zu einer
/// Aufzaehlung `Verweist`.
[[nodiscard]] Strukturdiagramm strukturVon(const modell::Einheit& einheit);

/// Ein Typ der Einheit (sein voller Name) und seine Nachbarn: die Typen derselben Einheit,
/// die er direkt enthaelt, auf die er verweist, von denen er erbt — und die das mit ihm tun;
/// mit den Beziehungen zwischen ihnen. Der Typ ist der erste Kasten, der Titel sein Name.
/// Gibt es ihn nicht: ein leeres Diagramm.
[[nodiscard]] Strukturdiagramm strukturVon(const modell::Einheit& einheit, std::string_view typ);

/// Die Zustandsautomaten einer Einheit, in der Reihenfolge der Funktionen.
///
/// Ein Automat ist ein `switch (V)`, in dessen Faellen mindestens zweimal
/// `V = K` steht, wobei `K` eine Fall-Marke desselben `switch` ist — so ruht er
/// auf einem Signal im Code, nicht auf einem Namen. Zustaende sind die
/// Fall-Marken (dazu Werte, die zugewiesen, aber nicht behandelt werden),
/// Uebergaenge jede solche Zuweisung, beschriftet mit den Bedingungen, unter
/// denen sie steht. Wo dieselbe Variable ausserhalb des `switch` einen Zustand
/// bekommt (in derselben oder einer anderen Funktion der Einheit — auch ueber
/// einen anderen Weg zum selben Glied, `a.zustand` statt `p->zustand`), fuehrt
/// ein Uebergang "von aussen" dorthin.
[[nodiscard]] std::vector<Zustandsdiagramm> automatenVon(const modell::Einheit& einheit);

}  // namespace c2d::diagramm
