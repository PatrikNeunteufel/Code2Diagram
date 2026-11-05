/**
 * @file Abfrage.hpp
 * @brief Der Zerleger der Abfragen im SQL-Leser (Entscheide Q1-Q5): Bloecke, Quellen, Joins,
 *        Bedingungen an den aeussersten AND zerlegt, Spaltenbezuege
 *
 * Arbeitet auf den Token des SQL-Lesers (Tokenquelle) und liefert modell::Abfrage — die Ableitung
 * kennt so kein SQL. Intern im Leser, kein Teil der Schnittstelle des Kerns.
 */
#pragma once

#include "c2d/lexer/Lexer.hpp"
#include "c2d/modell/Code.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::sprache::sqlteile
{

/// Was der Zerleger vom Leser braucht.
class Tokenquelle
{
public:
    virtual ~Tokenquelle() = default;
    /// Das Token i (dahinter: das Ende).
    [[nodiscard]] virtual const lexer::Token& token(std::size_t i) const = 0;
    /// Der Text der Quelle von Token a bis vor Token b.
    [[nodiscard]] virtual std::string quelltext(std::size_t a, std::size_t b) const = 0;
    /// Bei einer oeffnenden Klammer i: die Stelle hinter der passenden schliessenden.
    [[nodiscard]] virtual std::size_t hinterKlammer(std::size_t i) const = 0;
};

class AbfrageLeser
{
public:
    explicit AbfrageLeser(const Tokenquelle& q) : m_q(q) {}

    /// T-SQL: `SELECT … INTO t` legt eine Tabelle an (sonst nennt INTO Variablen: PL/pgSQL, MySQL).
    bool selectIntoTabelle = false;

    /// Jede Abfrage in [a, b) — eine Anweisung oder der Rumpf einer Routine, in dem mehrere stehen,
    /// auch ohne `;` (T-SQL). Name je Abfrage: `vorn@zeile`.
    [[nodiscard]] std::vector<modell::Abfrage> suche(std::size_t a, std::size_t b, const std::string& vorn);

    /// Die Anweisung [a, b) als Abfrage, wenn sie eine ist (SELECT, WITH, INSERT … SELECT, CREATE … AS,
    /// UPDATE … FROM, DELETE … USING, MERGE … USING); `name` ist ihr Name.
    [[nodiscard]] std::optional<modell::Abfrage> lies(std::size_t a, std::size_t b, const std::string& name);

private:
    [[nodiscard]] const lexer::Token& tok(std::size_t i) const { return m_q.token(i); }
    [[nodiscard]] bool wort(std::size_t i, std::string_view w) const;
    [[nodiscard]] bool istName(std::size_t i) const;
    [[nodiscard]] bool istAuf(std::size_t i) const;
    [[nodiscard]] bool istZu(std::size_t i) const;
    /// Hinter der Klammer bei i; hoechstens b.
    [[nodiscard]] std::size_t ueber(std::size_t i, std::size_t b) const;
    /// Beginnt bei i (einer oeffnenden Klammer) eine Abfrage?
    [[nodiscard]] bool abfrageInKlammer(std::size_t i) const;
    std::string liesNamen(std::size_t i, std::size_t& ende) const;

    std::size_t neuerBlock(modell::BlockRolle rolle, std::string titel, std::size_t eltern, std::size_t bei);
    /// Ein Block aus [a, b): WITH, Mengenoperation, SELECT, VALUES — auch in Klammern.
    std::size_t liesBlock(std::size_t a, std::size_t b, modell::BlockRolle rolle, std::string titel, std::size_t eltern);
    void liesSelect(std::size_t block, std::size_t a, std::size_t b);
    /// Die Quellen von FROM in [a, b); `verbindung` gilt fuer die erste.
    void liesQuellen(std::size_t block, std::size_t a, std::size_t b, std::string verbindung);
    /// [a, b) an den aeussersten AND zerlegt; Unterabfragen werden Bloecke von `block`.
    std::vector<modell::Bedingungsteil> zerlege(std::size_t block, std::size_t a, std::size_t b);
    /// Der Text von [a, b), jede Abfrage in Klammern darin als `(…)` und ein Block von `block`
    /// (Rolle, Titel); ihre Bloecke nach `bloecke`.
    std::string ohneUnterabfragen(std::size_t block, std::size_t a, std::size_t b, modell::BlockRolle rolle,
                                  const std::string& titel, std::vector<std::size_t>* bloecke = nullptr);
    /// Die Spalten, die [a, b) nennt (ohne die in Unterabfragen).
    void sammleBezuege(std::size_t a, std::size_t b, std::vector<modell::Spaltenbezug>& aus) const;

    const Tokenquelle& m_q;
    modell::Abfrage m_ab;  ///< die Abfrage, die gerade gelesen wird
};

}  // namespace c2d::sprache::sqlteile
