/**
 * @file Rumpf.hpp
 * @brief Der Ablauf im Rumpf einer SQL-Routine (Entscheide R1-R3, R6): IF, Schleifen, Spruenge, Ausnahmen
 *        als Anweisungen des Codemodells — je Stil der Dialekte
 *
 * Arbeitet auf den Token des SQL-Lesers (Rumpfquelle) wie der Zerleger der Abfragen. Intern im Leser, kein
 * Teil der Schnittstelle des Kerns. Die Tabelle, welches Wort welche Anweisung wird, steht in Rumpf.cpp.
 */
#pragma once

#include "Abfrage.hpp"

#include "c2d/modell/Code.hpp"
#include "c2d/quelle/Meldung.hpp"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace c2d::sprache::sqlteile
{

/// Was der Leser der Ruempfe ausser den Token vom SQL-Leser braucht.
class Rumpfquelle : public Tokenquelle
{
public:
    /// Was die Token [a, b) rufen (CALL, EXEC, PERFORM, ein Name vor `(` mit imAusdruck) — ohne Meldungen.
    [[nodiscard]] virtual std::vector<modell::Zugriff> aufrufeIn(std::size_t a, std::size_t b) = 0;
    /// Eine Meldung beim Token `bei`.
    virtual void meldeBei(quelle::Schwere schwere, std::size_t bei, std::string text) = 0;
};

/// Wie ein Dialekt seinen Ablauf schreibt.
enum class RumpfStil
{
    /// T-SQL: `IF c Anweisung [ELSE Anweisung]`, `WHILE c Anweisung`, `BEGIN … END`, `BEGIN TRY … END TRY BEGIN
    /// CATCH … END CATCH`, `;` darf fehlen.
    Tsql,
    /// PL/pgSQL, MySQL, Spark: `IF c THEN … ELSIF … ELSE … END IF`, `LOOP … END LOOP`, `WHILE c LOOP|DO … END …`,
    /// `REPEAT … UNTIL c END REPEAT`, `CASE … END CASE`; jede Anweisung endet mit `;`.
    EndeMitArt,
    /// Firebird (ohne eigenen Dialekt): `IF (c) THEN Anweisung [ELSE Anweisung]`, `WHILE (c) DO Anweisung`,
    /// `FOR SELECT … DO Anweisung`.
    Firebird,
};

/// Der Stil eines Rumpfs [a, b): aus dem Dialekt (`tsql`; `postgres`, `mysql`, `spark`: mit END IF …); im
/// allgemeinen Kern, in SQLite und DuckDB aus dem Text — ein `END IF`/`END LOOP` macht EndeMitArt, ein `IF … THEN`
/// ohne Firebird, sonst T-SQL.
[[nodiscard]] RumpfStil stilVon(const Tokenquelle& q, std::size_t a, std::size_t b, std::string_view dialekt);

class RumpfLeser
{
public:
    /// `plpgsql`: ein `DECLARE` vor `BEGIN` beginnt einen Teil mit Deklarationen (PL/pgSQL), keine Anweisung.
    RumpfLeser(Rumpfquelle& q, RumpfStil stil, bool plpgsql) : m_q(q), m_stil(stil), m_plpgsql(plpgsql) {}

    /// Die Anweisungen in [a, b) als Block (der Rumpf einer Routine, ohne `CREATE … AS`).
    [[nodiscard]] modell::AnweisungPtr lies(std::size_t a, std::size_t b);

    /// Nach lies: wie oft RAISERROR mit einer Schwere stand, die keine Zahl ist (Entscheid R2: gezaehlt).
    std::size_t raiserrorOhneZahl = 0;

private:
    using Token = lexer::Token;
    using Folge = std::vector<modell::AnweisungPtr>;

    [[nodiscard]] const Token& tok(std::size_t i) const { return m_q.token(i); }
    [[nodiscard]] bool wort(std::size_t i, std::string_view w) const;
    [[nodiscard]] bool istName(std::size_t i) const;
    [[nodiscard]] bool einesVon(std::size_t i, std::initializer_list<std::string_view> woerter) const;
    [[nodiscard]] std::string text(std::size_t a, std::size_t b) const;
    [[nodiscard]] int zeile(std::size_t i) const { return tok(i).ort.zeile; }
    /// Hinter `;` (auch mehreren) ab i, hoechstens b.
    [[nodiscard]] std::size_t ohneTrenner(std::size_t i, std::size_t b) const;

    /// Anweisungen ab i nach `ziel`, bis b oder am Anfang einer Anweisung eines der `stopps`; die Stelle dahinter.
    std::size_t folge(std::size_t i, std::size_t b, std::initializer_list<std::string_view> stopps, Folge& ziel);
    /// Eine Anweisung ab i; die Stelle dahinter (vor einem `;`). Leer, wenn bei i keine beginnt (dann i + 1).
    std::pair<modell::AnweisungPtr, std::size_t> anweisung(std::size_t i, std::size_t b);

    /// Das Ende einer einfachen Anweisung ab i.
    [[nodiscard]] std::size_t ende(std::size_t i, std::size_t b) const;
    /// Wo im T-SQL-Stil die Bedingung ab i endet: am ersten Wort, mit dem eine Anweisung beginnt (ausserhalb von
    /// Klammern und CASE … END).
    [[nodiscard]] std::size_t bedingungsEnde(std::size_t i, std::size_t b) const;
    /// Bis zum Wort `w` ausserhalb von Klammern und CASE … END ab i; b, wenn es fehlt.
    [[nodiscard]] std::size_t bisWort(std::size_t i, std::size_t b, std::initializer_list<std::string_view> w) const;
    /// Hinter `END` bei i und dem Wort, das zu ihm gehoert (`END IF`, `END LOOP marke`, `END TRY`).
    [[nodiscard]] std::size_t hinterEnde(std::size_t i, std::size_t b) const;
    /// Erwartet bei i das Wort `w`; sonst eine Meldung. Die Stelle dahinter (oder i).
    std::size_t erwarte(std::size_t i, std::size_t b, std::string_view w);

    modell::AnweisungPtr neu(modell::AnweisungArt art, std::size_t bei) const;
    /// Eine einfache Anweisung [a, e) mit ihrer Wirkung (R6) und ihren Aufrufen.
    modell::AnweisungPtr einfach(std::size_t a, std::size_t e);
    /// Die Aufrufe der Token [a, b) an die Anweisung (Anweisung::aufrufe).
    void ruft(modell::Anweisung& an, std::size_t a, std::size_t b);
    /// Was eine einfache Anweisung [a, e) schreibt (R6): Ziele von SET, `:=`, `SELECT @x =`, INTO, OUTPUT, die
    /// Namen einer DECLARE; dazu die Namen, die jede Anweisung setzt (`@@ROWCOUNT`, `FOUND` …).
    void wirkung(modell::Anweisung& an, std::size_t a, std::size_t e) const;
    /// Ein Block aus einer Folge.
    modell::AnweisungPtr block(Folge folge, std::size_t bei) const;

    std::pair<modell::AnweisungPtr, std::size_t> wennTsql(std::size_t i, std::size_t b);
    std::pair<modell::AnweisungPtr, std::size_t> wennMitArt(std::size_t i, std::size_t b);
    std::pair<modell::AnweisungPtr, std::size_t> wennFirebird(std::size_t i, std::size_t b);
    std::pair<modell::AnweisungPtr, std::size_t> beginTsql(std::size_t i, std::size_t b);
    std::pair<modell::AnweisungPtr, std::size_t> beginMitArt(std::size_t i, std::size_t b);
    std::pair<modell::AnweisungPtr, std::size_t> fall(std::size_t i, std::size_t b);
    std::pair<modell::AnweisungPtr, std::size_t> schleife(std::size_t i, std::size_t b);
    std::pair<modell::AnweisungPtr, std::size_t> sprung(std::size_t i, std::size_t b);
    std::pair<modell::AnweisungPtr, std::size_t> deklaration(std::size_t i, std::size_t b);

    /// Die Folge ab i bis END eines Blocks, mit den Handlern (Entscheid R3): ein EXIT-Handler macht aus dem Rest
    /// einen Versuch, ein CONTINUE-Handler steht abseits.
    std::size_t blockFolge(std::size_t i, std::size_t b, std::initializer_list<std::string_view> stopps, Folge& ziel);

    Rumpfquelle& m_q;
    RumpfStil m_stil;
    bool m_plpgsql;
    int m_inTry = 0;        ///< T-SQL: wie tief in BEGIN TRY (RAISERROR wirft nur dort)
    std::string m_marke;    ///< die Marke vor der naechsten Schleife oder dem naechsten Block
};

}  // namespace c2d::sprache::sqlteile
