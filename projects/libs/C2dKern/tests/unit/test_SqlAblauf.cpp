/**
 * @file test_SqlAblauf.cpp
 * @brief Der Ablauf von SQL-Routinen (Entscheide R1-R6): Funktionen aus Routinen, Triggern und Bloecken, die
 *        Abbildung des Steuerflusses je Dialekt, Handler, Bedingungen, Aufrufe im Aufrufgraph
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Entscheidungstabelle.hpp"
#include "c2d/projekt/Aufrufgraph.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/sprache/Sprache.hpp"

#include "TempOrdner.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;
using modell::Anweisung;
using modell::AnweisungArt;

namespace
{

sprache::Ergebnis sq(std::string_view text, std::string dialekt)
{
    sprache::LeseOptionen o;
    o.dialekt = std::move(dialekt);
    return sprache::sql().lies(text, "T.sql", o);
}

const modell::Funktion* funktion(const sprache::Ergebnis& e, std::string_view name)
{
    for (const modell::Funktion& f : e.einheit.funktionen)
        if (f.name == name) return &f;
    return nullptr;
}

/// Die erste Anweisung dieser Art im Baum (Tiefe zuerst, sonst nach kinder); nullptr = keine.
const Anweisung* finde(const Anweisung& a, AnweisungArt art, std::string_view kopf = {})
{
    if (a.art == art && (kopf.empty() || a.kopf == kopf || a.text == kopf)) return &a;
    for (const modell::AnweisungPtr& k : a.kinder)
        if (k)
            if (const Anweisung* x = finde(*k, art, kopf)) return x;
    if (a.sonst) return finde(*a.sonst, art, kopf);
    return nullptr;
}

std::size_t zahl(const Anweisung& a, AnweisungArt art)
{
    std::size_t n = a.art == art ? 1 : 0;
    for (const modell::AnweisungPtr& k : a.kinder)
        if (k) n += zahl(*k, art);
    if (a.sonst) n += zahl(*a.sonst, art);
    return n;
}

bool meldet(const sprache::Ergebnis& e, std::string_view teil)
{
    return std::any_of(e.meldungen.begin(), e.meldungen.end(),
                       [&](const quelle::Meldung& m) { return m.text.find(teil) != std::string::npos; });
}

std::size_t warnungen(const sprache::Ergebnis& e)
{
    return static_cast<std::size_t>(std::count_if(e.meldungen.begin(), e.meldungen.end(), [](const quelle::Meldung& m) {
        return m.schwere != quelle::Schwere::Hinweis;
    }));
}

/// Die Kante von einem Knoten mit diesem Text zu einem mit jenem; nullptr = keine.
const diagramm::Kante* kante(const diagramm::Flussdiagramm& d, std::string_view von, std::string_view nach)
{
    const auto hat = [&](diagramm::KnotenId k, std::string_view text) {
        const std::vector<std::string>& z = d.knoten()[k].zeilen;
        return std::any_of(z.begin(), z.end(), [&](const std::string& s) { return s == text; });
    };
    for (const diagramm::Kante& k : d.kanten())
        if (hat(k.von, von) && hat(k.nach, nach)) return &k;
    return nullptr;
}

constexpr std::string_view kTsql =
    "CREATE PROCEDURE dbo.Nachbestellen @ArtikelID int, @Menge AS int = 10 OUTPUT\n"  // 1
    "AS\n"                                                                             // 2
    "BEGIN\n"                                                                          // 3
    "    IF @Menge <= 0\n"                                                             // 4
    "        THROW 50001, 'Menge', 1;\n"                                               // 5
    "    ELSE IF @Menge > 100 SET @Menge = 100 ELSE PRINT 'ok'\n"                      // 6
    "    BEGIN TRY\n"                                                                  // 7
    "        RAISERROR ('x', 16, 1);\n"                                                // 8
    "        RAISERROR ('y', @schwere, 1);\n"                                          // 9
    "        WHILE @@FETCH_STATUS = 0\n"                                               // 10
    "        BEGIN\n"                                                                  // 11
    "            IF UPDATE(Menge) BREAK\n"                                             // 12
    "            EXEC dbo.Protokoll @a = 1\n"                                          // 13
    "            FETCH NEXT FROM c INTO @x\n"                                          // 14
    "        END\n"                                                                    // 15
    "    END TRY\n"                                                                    // 16
    "    BEGIN CATCH\n"                                                                // 17
    "        THROW;\n"                                                                 // 18
    "    END CATCH\n"                                                                  // 19
    "    RAISERROR ('z', 16, 1)\n"                                                     // 20
    "    weiter:\n"                                                                    // 21
    "    GOTO weiter\n"                                                                // 22
    "    RETURN 0\n"                                                                   // 23
    "END\n"
    "GO\n"
    "CREATE PROCEDURE dbo.Protokoll @a int AS INSERT INTO log VALUES (@a)\n"
    "GO\n";

}  // namespace

TEST_SUITE("SQL-Ablauf")
{
    TEST_CASE("T-SQL: IF ohne BEGIN, ELSE IF, TRY/CATCH, RAISERROR nach Schwere, WHILE, BREAK, EXEC, GOTO, RETURN")
    {
        const auto e = sq(kTsql, "tsql");
        CHECK(warnungen(e) == 0);
        REQUIRE(e.einheit.funktionen.size() == 2);
        const modell::Funktion* f = funktion(e, "dbo.Nachbestellen");
        REQUIRE(f != nullptr);
        CHECK(f->namenOhneFall);
        CHECK(f->zeile == 1);
        REQUIRE(f->parameter.size() == 2);
        CHECK(f->parameter[0].name == "@ArtikelID");
        CHECK(f->parameter[0].typ == "int");
        CHECK(f->parameter[1].name == "@Menge");
        CHECK(f->parameter[1].typ == "int");  // ohne AS, Vorgabe und OUTPUT
        const Anweisung& r = *f->rumpf;

        // IF @Menge <= 0 THROW …; ELSE IF … SET … ELSE PRINT
        const Anweisung* wenn = finde(r, AnweisungArt::Wenn);
        REQUIRE(wenn != nullptr);
        CHECK(wenn->text == "@Menge <= 0");
        CHECK(wenn->zeile == 4);
        REQUIRE(wenn->kinder.size() == 1);
        CHECK(wenn->kinder[0]->art == AnweisungArt::Wurf);
        REQUIRE(wenn->sonst);
        CHECK(wenn->sonst->art == AnweisungArt::Wenn);
        CHECK(wenn->sonst->text == "@Menge > 100");
        CHECK(wenn->sonst->kinder[0]->text == "SET @Menge = 100");
        REQUIRE(wenn->sonst->sonst);
        CHECK(wenn->sonst->sonst->text == "PRINT 'ok'");

        // TRY/CATCH; RAISERROR mit Schwere 16 im TRY wirft, mit einer Variablen nicht, ausserhalb nicht
        const Anweisung* versuch = finde(r, AnweisungArt::Versuch);
        REQUIRE(versuch != nullptr);
        CHECK(versuch->kopf == "BEGIN TRY");
        REQUIRE(versuch->kinder.size() == 2);
        CHECK(versuch->kinder[1]->art == AnweisungArt::Fang);
        CHECK(versuch->kinder[1]->kopf == "BEGIN CATCH");
        CHECK(finde(*versuch->kinder[0], AnweisungArt::Wurf, "RAISERROR ('x', 16, 1)") != nullptr);
        CHECK(finde(*versuch->kinder[0], AnweisungArt::Einfach, "RAISERROR ('y', @schwere, 1)") != nullptr);
        CHECK(finde(r, AnweisungArt::Einfach, "RAISERROR ('z', 16, 1)") != nullptr);
        CHECK(meldet(e, "RAISERROR with a severity that is not a number"));

        // WHILE: Rumpf ein Block; IF UPDATE(Menge) BREAK (UPDATE( ist eine Funktion); EXEC ruft
        const Anweisung* schleife = finde(r, AnweisungArt::Solange);
        REQUIRE(schleife != nullptr);
        CHECK(schleife->text == "@@FETCH_STATUS = 0");
        CHECK(schleife->kopf == "WHILE @@FETCH_STATUS = 0");
        const Anweisung* bruch = finde(*schleife, AnweisungArt::Wenn);
        REQUIRE(bruch != nullptr);
        CHECK(bruch->text == "UPDATE(Menge)");
        CHECK(bruch->kinder[0]->art == AnweisungArt::Abbruch);
        const Anweisung* exec = finde(*schleife, AnweisungArt::Einfach, "EXEC dbo.Protokoll @a = 1");
        REQUIRE(exec != nullptr);
        CHECK(exec->istAufruf);
        REQUIRE(exec->aufrufe.size() == 1);
        CHECK(exec->aufrufe[0].ziel == "dbo.Protokoll");
        CHECK(exec->ruft);
        // FETCH … INTO schreibt @x und die Namen, die jede Anweisung setzt (R6)
        const Anweisung* fetch = finde(*schleife, AnweisungArt::Einfach, "FETCH NEXT FROM c INTO @x");
        REQUIRE(fetch != nullptr);
        CHECK(std::find(fetch->schreibt.begin(), fetch->schreibt.end(), "@x") != fetch->schreibt.end());
        CHECK(std::find(fetch->schreibt.begin(), fetch->schreibt.end(), "@@fetch_status") != fetch->schreibt.end());

        CHECK(finde(r, AnweisungArt::Marke, "weiter") != nullptr);
        CHECK(finde(r, AnweisungArt::Sprung, "weiter") != nullptr);
        const Anweisung* rueck = finde(r, AnweisungArt::Ruecksprung);
        REQUIRE(rueck != nullptr);
        CHECK(rueck->text == "0");
        CHECK(rueck->kopf == "RETURN 0");

        // eine Prozedur ohne BEGIN: ihr Rumpf hinter AS
        const modell::Funktion* p = funktion(e, "dbo.Protokoll");
        REQUIRE(p != nullptr);
        REQUIRE(p->rumpf->kinder.size() == 1);
        CHECK(p->rumpf->kinder[0]->text == "INSERT INTO log VALUES (@a)");
    }

    TEST_CASE("PL/pgSQL: DECLARE-Teil, ELSIF, RAISE nach Stufe, EXCEPTION WHEN, FOR mit Marke, EXIT/CONTINUE WHEN, CASE")
    {
        const auto e = sq("CREATE TABLE t (id integer);\n"                                        // 1
                          "CREATE FUNCTION public.f(p_id integer, OUT n integer) RETURNS integer\n"  // 2
                          "    LANGUAGE plpgsql AS $$\n"                                             // 3
                          "DECLARE\n"                                                                // 4
                          "    r record;\n"                                                          // 5
                          "    k integer := 0;\n"                                                    // 6
                          "BEGIN\n"                                                                  // 7
                          "    IF NOT FOUND THEN RAISE EXCEPTION 'x';\n"                             // 8
                          "    ELSIF k > 1 THEN RAISE NOTICE 'y';\n"                                 // 9
                          "    ELSE RAISE;\n"                                                        // 10
                          "    END IF;\n"                                                            // 11
                          "    <<aussen>>\n"                                                         // 12
                          "    FOR r IN SELECT id FROM t LOOP\n"                                     // 13
                          "        CONTINUE WHEN r.id = 0;\n"                                        // 14
                          "        EXIT aussen WHEN k > 5;\n"                                        // 15
                          "        RETURN NEXT r.id;\n"                                              // 16
                          "    END LOOP aussen;\n"                                                   // 17
                          "    CASE k WHEN 1, 2 THEN k := 0; END CASE;\n"                            // 18
                          "    RETURN;\n"                                                            // 19
                          "EXCEPTION\n"                                                              // 20
                          "    WHEN division_by_zero OR unique_violation THEN RETURN NULL;\n"        // 21
                          "END;\n"                                                                   // 22
                          "$$;\n",
                          "postgres");
        CHECK(warnungen(e) == 0);
        const modell::Funktion* f = funktion(e, "public.f");
        REQUIRE(f != nullptr);
        CHECK(f->zeile == 2);
        REQUIRE(f->parameter.size() == 2);
        CHECK(f->parameter[1].name == "n");  // ohne OUT
        CHECK(f->rueckgabe == "integer");
        const Anweisung& r = *f->rumpf;

        // DECLARE: je Deklaration eine Anweisung, die erste mit dem Wort; Zeilen der Datei
        const Anweisung* erste = finde(r, AnweisungArt::Einfach, "DECLARE r record");
        REQUIRE(erste != nullptr);
        CHECK(erste->zeile == 4);
        CHECK(finde(r, AnweisungArt::Einfach, "k integer := 0") != nullptr);

        // EXCEPTION WHEN: ein Versuch, je WHEN ein Fang
        const Anweisung* versuch = finde(r, AnweisungArt::Versuch);
        REQUIRE(versuch != nullptr);
        REQUIRE(versuch->kinder.size() == 2);
        CHECK(versuch->kinder[1]->kopf == "EXCEPTION WHEN division_by_zero OR unique_violation");

        const Anweisung* wenn = finde(r, AnweisungArt::Wenn, "NOT FOUND");
        REQUIRE(wenn != nullptr);
        CHECK(wenn->zeile == 8);
        CHECK(finde(*wenn->kinder[0], AnweisungArt::Wurf) != nullptr);
        REQUIRE(wenn->sonst);
        CHECK(wenn->sonst->art == AnweisungArt::Wenn);
        CHECK(finde(*wenn->sonst->kinder[0], AnweisungArt::Einfach, "RAISE NOTICE 'y'") != nullptr);
        CHECK(finde(*wenn->sonst->sonst, AnweisungArt::Wurf, "RAISE") != nullptr);  // ohne Stufe: wirft weiter

        const Anweisung* fuer = finde(r, AnweisungArt::FuerJedes);
        REQUIRE(fuer != nullptr);
        CHECK(fuer->marke == "aussen");
        CHECK(fuer->kopf == "FOR r IN SELECT id FROM t");
        const Anweisung* weiter = finde(*fuer, AnweisungArt::Wenn, "r.id = 0");
        REQUIRE(weiter != nullptr);
        CHECK(weiter->kinder[0]->art == AnweisungArt::Fortsetzung);
        const Anweisung* raus = finde(*fuer, AnweisungArt::Abbruch);
        REQUIRE(raus != nullptr);
        CHECK(raus->text == "aussen");
        CHECK(raus->kopf == "EXIT aussen");  // so im Struktogramm, nicht `break aussen`
        CHECK(finde(*fuer, AnweisungArt::Einfach, "RETURN NEXT r.id") != nullptr);  // liefert, endet nicht

        // CASE ohne ELSE: wirft, wenn kein Fall passt
        const Anweisung* fall = finde(r, AnweisungArt::Auswahl);
        REQUIRE(fall != nullptr);
        CHECK(fall->ohneDurchfall);
        CHECK(fall->text == "k");
        CHECK(zahl(*fall, AnweisungArt::Fall) == 3);  // 1, 2 und der Standardfall
        CHECK(finde(*fall, AnweisungArt::Wurf, "CASE NOT FOUND (no ELSE)") != nullptr);

        // ein Trigger ohne Rumpf (EXECUTE FUNCTION) und ein DO-Block
        const auto t = sq("CREATE TRIGGER tr AFTER UPDATE ON t FOR EACH ROW EXECUTE FUNCTION public.f();\n"
                          "DO $$ BEGIN PERFORM public.f(1); END $$;\n",
                          "postgres");
        CHECK(funktion(t, "tr") == nullptr);
        const modell::Funktion* block = funktion(t, "<block:2>");
        REQUIRE(block != nullptr);
        const Anweisung* perform = finde(*block->rumpf, AnweisungArt::Einfach, "PERFORM public.f(1)");
        REQUIRE(perform != nullptr);
        CHECK(perform->istAufruf);
    }

    TEST_CASE("MySQL: LOOP und LEAVE mit Marke, REPEAT … UNTIL, CASE x WHEN, EXIT- und CONTINUE-Handler")
    {
        const auto e = sq("CREATE PROCEDURE p(IN x INT)\n"                          // 1
                          "BEGIN\n"                                                 // 2
                          "  DECLARE fertig INT DEFAULT 0;\n"                       // 3
                          "  DECLARE CONTINUE HANDLER FOR NOT FOUND SET fertig = 1;\n"  // 4
                          "  DECLARE EXIT HANDLER FOR SQLEXCEPTION, SQLSTATE '23000' RETURN;\n"  // 5
                          "  lesen: LOOP\n"                                         // 6
                          "    IF fertig = 1 THEN LEAVE lesen; END IF;\n"           // 7
                          "  END LOOP lesen;\n"                                     // 8
                          "  REPEAT SET x = x + 1; UNTIL x >= 10 END REPEAT;\n"     // 9
                          "  CASE x WHEN 1 THEN SET x = 0; ELSE SET x = 2; END CASE;\n"  // 10
                          "END\n",
                          "mysql");
        CHECK(warnungen(e) == 0);
        CHECK(meldet(e, "CONTINUE HANDLER FOR NOT FOUND is shown where it is declared"));
        const modell::Funktion* f = funktion(e, "p");
        REQUIRE(f != nullptr);
        REQUIRE(f->parameter.size() == 1);
        CHECK(f->parameter[0].name == "x");
        CHECK(f->parameter[0].typ == "INT");
        const Anweisung& r = *f->rumpf;

        const Anweisung* weiter = finde(r, AnweisungArt::Einfach, "on NOT FOUND: SET fertig = 1 (continue handler)");
        REQUIRE(weiter != nullptr);
        CHECK(weiter->abseits);

        // EXIT: der Rest des Blocks ist ein Versuch, der Handler ein Fang mit beiden Bedingungen
        const Anweisung* versuch = finde(r, AnweisungArt::Versuch);
        REQUIRE(versuch != nullptr);
        REQUIRE(versuch->kinder.size() == 2);
        CHECK(versuch->kinder[1]->kopf == "EXIT HANDLER FOR SQLEXCEPTION, SQLSTATE '23000'");
        CHECK(finde(*versuch->kinder[1], AnweisungArt::Ruecksprung) != nullptr);
        CHECK(finde(*versuch->kinder[0], AnweisungArt::Solange) != nullptr);  // die Schleife steht im Rest

        const Anweisung* schleife = finde(r, AnweisungArt::Solange);
        REQUIRE(schleife != nullptr);
        CHECK(schleife->marke == "lesen");
        CHECK(schleife->kopf == "LOOP");
        CHECK(schleife->text == "true");
        const Anweisung* raus = finde(*schleife, AnweisungArt::Abbruch);
        REQUIRE(raus != nullptr);
        CHECK(raus->text == "lesen");

        const Anweisung* repeat = finde(r, AnweisungArt::TueSolange);
        REQUIRE(repeat != nullptr);
        CHECK(repeat->endetWennWahr);
        CHECK(repeat->text == "x >= 10");
        CHECK(repeat->kopf == "UNTIL x >= 10");
        CHECK(repeat->zeile == 9);

        const Anweisung* fall = finde(r, AnweisungArt::Auswahl);
        REQUIRE(fall != nullptr);
        CHECK(fall->ohneDurchfall);
        CHECK(fall->kopf == "CASE x");
        CHECK(finde(*fall, AnweisungArt::Wurf) == nullptr);  // mit ELSE

        // Ablauf: REPEAT endet mit `yes`; der Handler ohne Kanten und nicht unerreichbar
        const diagramm::Flussdiagramm d = diagramm::flussVon(*f);
        const diagramm::Kante* zurueck = kante(d, "x >= 10", "SET x = x + 1");
        REQUIRE(zurueck != nullptr);
        CHECK(zurueck->beschriftung == "no");
        bool handlerMitKante = false;
        bool handlerUnerreichbar = false;
        for (diagramm::KnotenId k = 0; k < d.knoten().size(); ++k)
        {
            const std::vector<std::string>& z = d.knoten()[k].zeilen;
            if (std::find(z.begin(), z.end(), "on NOT FOUND: SET fertig = 1 (continue handler)") == z.end()) continue;
            handlerUnerreichbar = d.knoten()[k].unerreichbar;
            for (const diagramm::Kante& x : d.kanten()) handlerMitKante = handlerMitKante || x.von == k || x.nach == k;
        }
        CHECK_FALSE(handlerMitKante);
        CHECK_FALSE(handlerUnerreichbar);
        // der Handler ist keine Aktion der Entscheidungstabelle
        const diagramm::Entscheidungstabelle t = diagramm::entscheidungstabelleVon(*f);
        CHECK(std::none_of(t.aktionen.begin(), t.aktionen.end(), [](const auto& x) { return x.text.find("continue handler") != std::string::npos; }));
    }

    TEST_CASE("MySQL: `#` ist ein Kommentar, `END $$` endet am Trenner (die Routine laeuft nicht in DELIMITER weiter)")
    {
        const auto e = sq("DELIMITER $$\n"
                          "CREATE FUNCTION f(p INT) RETURNS INT\n"
                          "BEGIN\n"
                          "  # Kommentar\n"
                          "  DECLARE v INT; # noch einer\n"
                          "  RETURN v;\n"
                          "END $$\n"
                          "\n"
                          "DELIMITER ;\n"
                          "\n"
                          "DELIMITER $$\n"
                          "CREATE FUNCTION g() RETURNS INT BEGIN RETURN 1; END $$\n"
                          "DELIMITER ;\n",
                          "mysql");
        CHECK(warnungen(e) == 0);
        REQUIRE(e.einheit.funktionen.size() == 2);
        const modell::Funktion* f = funktion(e, "f");
        REQUIRE(f != nullptr);
        CHECK(f->bisZeile == 7);
        const Anweisung& block = *f->rumpf->kinder[0];
        REQUIRE(block.kinder.size() == 2);
        CHECK(block.kinder[0]->text == "DECLARE v INT");
        CHECK(block.kinder[1]->art == AnweisungArt::Ruecksprung);
        REQUIRE(e.einheit.objekte.size() == 2);
        CHECK(e.einheit.objekte[1].name == "g");  // DELIMITER $$ davor gilt noch
    }

    TEST_CASE("Bedingungen (R6): BETWEEN, CASE als Atom, Unterabfrage ruft, Namen klein, neu gefragt nach FETCH")
    {
        const auto e = sq("CREATE PROCEDURE p AS\n"
                          "BEGIN\n"
                          "  IF @a = 1 AND @b BETWEEN 1 AND 2 PRINT 'a'\n"
                          "  IF EXISTS (SELECT 1 FROM t WHERE x = 1 AND y = 2) OR CASE WHEN @c > 0 AND @d > 0 THEN 1 END = 1\n"
                          "    PRINT 'b'\n"
                          "  FETCH NEXT FROM c INTO @x\n"
                          "  IF @@FETCH_STATUS = 0 AND @a = 1 SET @y = 1\n"
                          "  FETCH NEXT FROM c INTO @x\n"
                          "  IF @@FETCH_STATUS = 0 AND @a = 1 SET @y = 2\n"
                          "END\n",
                          "tsql");
        const modell::Funktion* f = funktion(e, "p");
        REQUIRE(f != nullptr);
        std::vector<const Anweisung*> wenn;
        for (const modell::AnweisungPtr& k : f->rumpf->kinder[0]->kinder)
            if (k->art == AnweisungArt::Wenn) wenn.push_back(k.get());
        REQUIRE(wenn.size() == 4);

        REQUIRE(wenn[0]->logik);
        CHECK(wenn[0]->logik->art == modell::Logik::Art::Und);
        CHECK(wenn[0]->logik->teile.size() == 2);  // das AND des BETWEEN verbindet nicht
        CHECK(wenn[0]->logik->teile[1].text == "@b BETWEEN 1 AND 2");
        CHECK(wenn[0]->bedingungNamen == std::vector<std::string>{"@a", "@b"});

        REQUIRE(wenn[1]->logik);
        CHECK(wenn[1]->logik->art == modell::Logik::Art::Oder);
        REQUIRE(wenn[1]->logik->teile.size() == 2);
        CHECK(wenn[1]->logik->teile[0].mitAufruf);   // die Unterabfrage liest die Datenbank
        CHECK(wenn[1]->logik->teile[1].text.starts_with("CASE WHEN"));  // CASE … END ein Atom
        CHECK(wenn[1]->bedingungRuft);

        CHECK(std::find(wenn[2]->bedingungNamen.begin(), wenn[2]->bedingungNamen.end(), "@@fetch_status") !=
              wenn[2]->bedingungNamen.end());
        // die Entscheidungstabelle fragt nach dem zweiten FETCH neu
        const diagramm::Entscheidungstabelle t = diagramm::entscheidungstabelleVon(*f);
        CHECK(std::any_of(t.bedingungen.begin(), t.bedingungen.end(),
                          [](const diagramm::Entscheidungstabelle::Bedingung& b) { return b.erneut; }));
    }

    TEST_CASE("Spark, Hive, DuckDB: RETURN als Rumpf, eine Klasse als Rumpf, ein Makro; Bloecke auf oberster Ebene")
    {
        const auto e = sq("CREATE FUNCTION f(x INT) RETURNS INT RETURN x + 1;\n"
                          "CREATE TEMPORARY FUNCTION g AS 'com.example.Udf';\n"
                          "L0: BEGIN\n"
                          "  DECLARE i INT DEFAULT 0;\n"
                          "  WHILE i < 3 DO SET i = i + 1; END WHILE;\n"
                          "END;\n",
                          "spark");
        CHECK(warnungen(e) == 0);
        const modell::Funktion* f = funktion(e, "f");
        REQUIRE(f != nullptr);
        REQUIRE(f->rumpf->kinder.size() == 1);
        CHECK(f->rumpf->kinder[0]->art == AnweisungArt::Ruecksprung);
        CHECK(f->rumpf->kinder[0]->text == "x + 1");
        CHECK(funktion(e, "g") == nullptr);  // kein SQL-Code
        const modell::Funktion* block = funktion(e, "<block:3>");
        REQUIRE(block != nullptr);
        const Anweisung* schleife = finde(*block->rumpf, AnweisungArt::Solange);
        REQUIRE(schleife != nullptr);
        CHECK(schleife->kopf == "WHILE i < 3");

        const auto m = sq("CREATE MACRO plus1(a) AS a + 1;\n", "duckdb");
        const modell::Funktion* makro = funktion(m, "plus1");
        REQUIRE(makro != nullptr);
        CHECK(makro->rumpf->kinder[0]->art == AnweisungArt::Ruecksprung);
        CHECK(makro->rumpf->kinder[0]->text == "a + 1");
    }

    TEST_CASE("Aufrufgraph (R5): EXEC ohne Gross/klein und ohne Schema, eingebaute Funktionen sind keine Aufrufe")
    {
        TempOrdner o;
        o.lege("code2diagram.json", R"({ "sql": "tsql" })");
        o.lege("a.sql", "CREATE PROCEDURE dbo.Haupt AS\n"
                        "BEGIN\n"
                        "  EXEC DBO.NEBEN;\n"
                        "  SET @n = COALESCE(@n, 0) + dbo.Zaehle(1);\n"
                        "  EXEC sys.sp_fremd;\n"
                        "END\n"
                        "GO\n");
        o.lege("b.sql", "CREATE PROCEDURE dbo.Neben AS RETURN 0\nGO\n"
                        "CREATE FUNCTION dbo.Zaehle(@x int) RETURNS int AS BEGIN RETURN @x END\nGO\n");
        const projekt::Projekt p = projekt::liesOrdner(o.weg());
        const std::size_t a = p.finde("a.sql");
        REQUIRE(a != projekt::kKeine);
        const projekt::AufrufgraphErgebnis g = projekt::aufrufgraphDerDatei(p, a);
        const auto knoten = [&](diagramm::AufrufKnotenArt art, std::string_view name) {
            return std::any_of(g.graph.knoten.begin(), g.graph.knoten.end(),
                               [&](const diagramm::AufrufKnoten& k) { return k.art == art && k.name == name; });
        };
        CHECK(knoten(diagramm::AufrufKnotenArt::Funktion, "dbo.Haupt"));
        CHECK(knoten(diagramm::AufrufKnotenArt::Funktion, "dbo.Neben"));   // `DBO.NEBEN`
        CHECK(knoten(diagramm::AufrufKnotenArt::Funktion, "dbo.Zaehle"));  // im Ausdruck, eine Routine des Projekts
        CHECK(knoten(diagramm::AufrufKnotenArt::Fremd, "sys.sp_fremd"));   // EXEC einer fremden Prozedur
        CHECK_FALSE(knoten(diagramm::AufrufKnotenArt::Fremd, "COALESCE"));  // eingebaut
        CHECK(g.graph.kanten.size() == 3);

        // Neben ohne Schema gerufen: der letzte Teil
        const projekt::Fundstelle neben = projekt::funktionenNamens(p, "dbo.Neben").front();
        CHECK(projekt::funktion(p, neben).heisst("neben"));
        CHECK(projekt::funktion(p, neben).heisst("Dbo.Neben"));
        CHECK_FALSE(projekt::funktion(p, neben).heisst("x.Neben"));
    }

    TEST_CASE("Abfragen in T-SQL ohne `;`: ELSE und END mitten in der Zeile beenden eine Abfrage")
    {
        const auto e = sq("CREATE PROCEDURE p AS BEGIN\n"
                          "IF @a = 1\n"
                          "  SELECT x FROM t ELSE SELECT y FROM u\n"
                          "END\nGO\n",
                          "tsql");
        REQUIRE(e.einheit.abfragen.size() == 2);
        CHECK(e.einheit.abfragen[0].bloecke[0].quellen.size() == 1);
        CHECK(e.einheit.abfragen[0].bloecke[0].quellen[0].name == "t");
        CHECK(e.einheit.abfragen[1].bloecke[0].quellen[0].name == "u");

        // hinter `.` ist END ein Name (eine Spalte `end`, Spark session_window.end): die Abfrage endet nicht dort
        const auto s = sq("SELECT\n  session_window.start,\n  session_window.end,\n  count(*) AS cnt\nFROM ts_data;\n", "spark");
        REQUIRE(s.einheit.abfragen.size() == 1);
        REQUIRE(s.einheit.abfragen[0].bloecke[0].quellen.size() == 1);
        CHECK(s.einheit.abfragen[0].bloecke[0].quellen[0].name == "ts_data");
    }
}
