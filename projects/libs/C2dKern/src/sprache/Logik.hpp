/**
 * @file Logik.hpp
 * @brief Die Logik einer Bedingung und die Wirkung einer Anweisung — fuer alle Sprachen, nach ihren Regeln
 *
 * Intern (nicht unter include/). Der Leser einer Sprache ruft ergaenzeLogik() am Ende: jede
 * Anweisung, deren Bedingung mindestens zwei Atome hat, bekommt ihren Baum
 * (modell::Anweisung::logik) — die Wahrheitstabelle baut die Ableitung daraus —, dazu, was
 * sie schreibt und ruft (fuer die Entscheidungstabelle). Wie eine Sprache Und, Oder, Nicht
 * und die bedingte Auswahl schreibt, sagen ihre Sprachtabelle (die Woerter `and`, `or`,
 * `not` sind Schluesselwoerter) und ihre Ausdrucksregeln.
 */
#pragma once

#include "c2d/lexer/Lexer.hpp"
#include "c2d/modell/Code.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace c2d::sprache
{

/// Was an Ausdruecken je Sprache anders ist.
struct Ausdrucksregeln
{
    /// Die bedingte Auswahl: `c ? a : b` (C-Familie) oder `a if c else b` (Python); Kotlin hat
    /// keine (`?:` ist der Elvis-Operator, `if` als Wert verzweigt der Leser selbst).
    enum class Wahl
    {
        Fragezeichen,
        NachgestelltesWenn,
        Keine,
    };
    Wahl wahl = Wahl::Fragezeichen;
    /// `not` bindet schwaecher als ein Vergleich: `not a == b` ist `not (a == b)` (Python).
    /// Sonst gilt `!` nur vor einem Einzelnen — `!a == b` ist ein Vergleich, ein Atom (C).
    bool nichtUmfasstVergleich = false;
    /// Die Operatoren, die zuweisen; `=` zuerst.
    std::span<const std::string_view> zuweisungen;
    /// `*p = …`, `p->x = …` schreiben durch einen Zeiger; `&name` nimmt eine Adresse (C-Familie).
    bool zeiger = false;

    // --- SQL (Entscheid R6) ---------------------------------------------------------------------
    /// Was eine Anweisung schreibt und ruft (`schreibt`, `ruft`, `ziel`, `wert`), nennt der Leser: eine Zuweisung
    /// steht nur am Anfang einer Anweisung (`SET x = …`, `x := …`, `SELECT @x = …`), sonst vergleicht `=`. Eine
    /// Wahrheitstabelle bekommt dann bei einer Anweisung nur ihr `wert`.
    bool wirkungVomLeser = false;
    /// Namen kennen kein Gross/klein: `bedingungNamen` stehen klein da (wie `schreibt` vom Leser).
    bool namenOhneFall = false;
    /// `BETWEEN a AND b`: das AND gehoert zum BETWEEN und verbindet nichts.
    bool zwischenUnd = false;
    /// `CASE … END` zaehlt wie eine Klammer: ein AND darin trennt nicht, der Ausdruck ist ein Atom.
    bool fallBloecke = false;
    /// Eine Unterabfrage (`(SELECT …)`, `EXISTS (…)`) liest die Datenbank: ein Atom mit ihr ruft (mitAufruf), eine
    /// Bedingung mit ihr gilt nie als entschieden (bedingungRuft).
    bool unterabfrageRuft = false;
};

/// Die Logik eines Ausdrucks. Ein Atom ist, was zwischen den logischen Operatoren steht;
/// eine bedingte Auswahl auf oberster Ebene ist ein Atom.
/// nullopt = weniger als zwei verschiedene Atome (gleicher Text ist dasselbe Atom).
[[nodiscard]] std::optional<modell::Logik> logikVon(std::string_view ausdruck, const lexer::Sprachtabelle& tabelle,
                                                    const Ausdrucksregeln& regeln);

/// Fuer jede Anweisung jeder Funktion: welcher Ausdruck eine Wahrheitstabelle bekommt —
/// die Bedingung von if, while, do, for; bei einer Anweisung oder einem return die Bedingung
/// einer bedingten Auswahl, sonst die rechte Seite einer Zuweisung (oder der ganze Ausdruck),
/// wenn sie Und/Oder enthaelt — und seine Logik, wenn sie mindestens zwei Atome hat. Dazu,
/// was jede Anweisung schreibt und ruft und welche Namen eine Bedingung liest.
void ergaenzeLogik(modell::Einheit& einheit, const lexer::Sprachtabelle& tabelle, const Ausdrucksregeln& regeln);

}  // namespace c2d::sprache
