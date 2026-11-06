/**
 * @file test_Sql.cpp
 * @brief SQL: Lexer (Gross/klein, Zeichenketten, Namen in Begrenzern, $$), Trennen der Anweisungen,
 *        Tabellen mit Schluesseln, ALTER TABLE, das Datentypen-Diagramm als ER-Diagramm
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Strukturdiagramm.hpp"
#include "c2d/lexer/Lexer.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;
using lexer::Token;
using lexer::TokenArt;

namespace
{

/// Mit einem Dialekt, der wie der Kern zerlegt (sonst kaeme in jedem Fall der Hinweis auf den Dialekt).
sprache::Ergebnis sq(std::string_view text, std::string dialekt = "sqlite")
{
    sprache::LeseOptionen o;
    o.dialekt = std::move(dialekt);
    return sprache::sql().lies(text, "T.sql", o);
}

std::vector<Token> token(std::string_view text)
{
    std::vector<quelle::Meldung> meldungen;
    std::vector<Token> t = lexer::zerlege(text, sprache::sql().tabelle(), meldungen);
    CHECK(meldungen.empty());
    t.pop_back();  // Ende
    return t;
}

const modell::Typ& tabelle(const sprache::Ergebnis& e, std::string_view name)
{
    for (const modell::Typ& t : e.einheit.typen)
        if (t.name == name) return t;
    FAIL("no table " << name);
    return e.einheit.typen.front();
}

const modell::Feld& spalte(const modell::Typ& t, std::string_view name)
{
    for (const modell::Feld& f : t.felder)
        if (f.name == name) return f;
    FAIL("no column " << name);
    return t.felder.front();
}

int zahl(const sprache::Ergebnis& e, quelle::Schwere schwere)
{
    return static_cast<int>(std::count_if(e.meldungen.begin(), e.meldungen.end(),
                                          [&](const quelle::Meldung& m) { return m.schwere == schwere; }));
}

bool meldet(const sprache::Ergebnis& e, std::string_view teil)
{
    return std::any_of(e.meldungen.begin(), e.meldungen.end(),
                       [&](const quelle::Meldung& m) { return m.text.find(teil) != std::string::npos; });
}

}  // namespace

TEST_SUITE("SQL")
{
    TEST_CASE("Lexer: Schluesselwoerter ohne Gross/klein, '' in Zeichenketten, Namen in Begrenzern")
    {
        const auto t = token("Select 'it''s', \"Album Name\", [dbo].[x], `y`, a[1], tags text[] FROM t");
        CHECK(t[0].art == TokenArt::Schluesselwort);
        CHECK(t[0].text == "Select");
        CHECK(t[1].art == TokenArt::Zeichenkette);
        CHECK(t[1].text == "'it''s'");
        CHECK(t[3].art == TokenArt::Bezeichner);
        CHECK(t[3].text == "\"Album Name\"");
        CHECK(t[5].text == "[dbo]");
        CHECK(t[6].istOp("."));
        CHECK(t[7].text == "[x]");
        CHECK(t[9].text == "`y`");
        // nach einem Namen ist `[` ein Index, vor `]` ein Feld
        CHECK(t[11].text == "a");
        CHECK(t[12].istOp("["));
        CHECK(t[16].text == "tags");
        CHECK(t[17].text == "text");
        CHECK(t[18].istOp("["));
        CHECK(t[19].istOp("]"));
        CHECK(t.back().art == TokenArt::Bezeichner);  // `t` ist kein Schluesselwort
    }

    TEST_CASE("Lexer: $$-Rumpf, $marke$, $1; -- und verschachtelte /* */; kein Escape mit \\")
    {
        const auto t = token("AS $$ select 1; $$ $f$ x $f$ $1 -- weg\n/* a /* b */ c */ 'a\\' x");
        CHECK(t[1].art == TokenArt::Zeichenkette);
        CHECK(t[1].text == "$$ select 1; $$");
        CHECK(t[2].text == "$f$ x $f$");
        CHECK(t[3].art == TokenArt::Bezeichner);
        CHECK(t[3].text == "$1");
        CHECK(t[4].text == "'a\\'");
        CHECK(t[5].text == "x");
    }

    TEST_CASE("CREATE TABLE: Spalten, Typen, Primaerschluessel an der Spalte und an der Tabelle")
    {
        const auto e = sq("CREATE TABLE IF NOT EXISTS kunde (\n"
                          "  id SERIAL PRIMARY KEY,\n"
                          "  name character varying(45) NOT NULL,\n"
                          "  preis NUMERIC(10, 2) DEFAULT 0.00,\n"
                          "  angelegt timestamp without time zone DEFAULT now()\n"
                          ");\n"
                          "create table posten (a int, b int, menge int, primary key (a, b));");
        CHECK(e.meldungen.empty());
        REQUIRE(e.einheit.typen.size() == 2);
        const modell::Typ& k = tabelle(e, "kunde");
        CHECK(k.art == modell::TypArt::Tabelle);
        CHECK(k.zeile == 1);
        REQUIRE(k.felder.size() == 4);
        CHECK(spalte(k, "id").typ == "SERIAL");
        CHECK(spalte(k, "id").zusatz == "PK");
        CHECK(spalte(k, "id").typHinten);
        CHECK(spalte(k, "name").typ == "character varying(45)");
        CHECK(spalte(k, "name").zusatz.empty());
        CHECK(spalte(k, "name").zeile == 3);
        CHECK(spalte(k, "preis").typ == "NUMERIC(10, 2)");
        CHECK(spalte(k, "angelegt").typ == "timestamp without time zone");
        const modell::Typ& p = tabelle(e, "posten");
        CHECK(spalte(p, "a").zusatz == "PK");
        CHECK(spalte(p, "b").zusatz == "PK");
        CHECK(spalte(p, "menge").zusatz.empty());
    }

    TEST_CASE("Fremdschluessel: an der Spalte, an der Tabelle, ueber ALTER TABLE; Gross/klein und Schema")
    {
        const auto e = sq("CREATE TABLE \"Artist\" (\"ArtistId\" INTEGER NOT NULL, CONSTRAINT \"PK_Artist\" PRIMARY KEY (\"ArtistId\"));\n"
                          "CREATE TABLE \"Album\" (\n"
                          "  \"AlbumId\" INTEGER NOT NULL,\n"
                          "  \"ArtistId\" INTEGER NOT NULL,\n"
                          "  CONSTRAINT \"FK_AlbumArtist\" FOREIGN KEY (\"ArtistId\") REFERENCES \"Artist\" (\"ArtistId\") ON DELETE NO ACTION\n"
                          ");\n"
                          "CREATE TABLE public.track (id int PRIMARY KEY, album int REFERENCES album);\n"
                          "ALTER TABLE ONLY public.track ADD COLUMN artist integer, ADD CONSTRAINT fk_a FOREIGN KEY (artist) REFERENCES public.\"Artist\"(\"ArtistId\");");
        CHECK(e.meldungen.empty());
        const modell::Typ& album = tabelle(e, "Album");
        REQUIRE(album.verweise.size() == 1);
        CHECK(album.verweise[0].ziel == "Artist");
        CHECK(album.verweise[0].felder == std::vector<std::string>{"ArtistId"});
        CHECK(album.verweise[0].zielFelder == std::vector<std::string>{"ArtistId"});
        CHECK(album.verweise[0].name == "FK_AlbumArtist");
        CHECK(spalte(album, "ArtistId").zusatz == "FK");
        CHECK(spalte(tabelle(e, "Artist"), "ArtistId").zusatz == "PK");

        const modell::Typ& track = tabelle(e, "public.track");
        REQUIRE(track.verweise.size() == 2);
        CHECK(track.verweise[0].ziel == "Album");  // `album` ohne Begrenzer: ohne Ruecksicht auf Gross/klein
        CHECK(track.verweise[0].zielFelder.empty());
        CHECK(track.verweise[1].ziel == "Artist");  // das Schema fehlt an der Tabelle: ueber den letzten Teil
        CHECK(spalte(track, "artist").typ == "integer");
        CHECK(spalte(track, "artist").zusatz == "FK");
        CHECK(spalte(track, "id").zusatz == "PK");
    }

    TEST_CASE("T-SQL: GO trennt, [Namen], CLUSTERED, WITH CHECK ADD; MySQL: KEY, DELIMITER, Trigger")
    {
        const auto t = sq("CREATE TABLE [Sales].[Kunde] (\n"
                          "  [KundeID] INT IDENTITY(1,1) NOT NULL,\n"
                          "  CONSTRAINT [PK_Kunde] PRIMARY KEY CLUSTERED ([KundeID] ASC)\n"
                          ")\n"
                          "GO\n"
                          "CREATE TABLE [Sales].[Auftrag] ([AuftragID] INT NOT NULL, [KundeID] INT NOT NULL)\n"
                          "GO\n"
                          "ALTER TABLE [Sales].[Auftrag] WITH CHECK ADD CONSTRAINT [FK_A] FOREIGN KEY([KundeID]) REFERENCES [Sales].[Kunde] ([KundeID])\n"
                          "GO\n");
        CHECK(t.meldungen.empty());
        CHECK(spalte(tabelle(t, "Sales.Kunde"), "KundeID").typ == "INT");
        CHECK(spalte(tabelle(t, "Sales.Kunde"), "KundeID").zusatz == "PK");
        REQUIRE(tabelle(t, "Sales.Auftrag").verweise.size() == 1);
        CHECK(tabelle(t, "Sales.Auftrag").verweise[0].ziel == "Sales.Kunde");

        const auto m = sq("CREATE TABLE film (\n"
                          "  film_id SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,\n"
                          "  `key` VARCHAR(5),\n"
                          "  PRIMARY KEY  (film_id),\n"
                          "  KEY idx_title (`key`)\n"
                          ") ENGINE=InnoDB;\n"
                          "DELIMITER ;;\n"
                          "CREATE TRIGGER ins_film AFTER INSERT ON film FOR EACH ROW BEGIN\n"
                          "    IF new.film_id > 0 THEN\n"
                          "        INSERT INTO log VALUES (new.film_id);\n"
                          "    END IF;\n"
                          "  END;;\n"
                          "DELIMITER ;\n"
                          "CREATE TABLE b (x int);");
        CHECK(zahl(m, quelle::Schwere::Warnung) == 0);
        const modell::Typ& film = tabelle(m, "film");
        REQUIRE(film.felder.size() == 2);
        CHECK(spalte(film, "film_id").typ == "SMALLINT UNSIGNED");
        CHECK(spalte(film, "film_id").zusatz == "PK");
        CHECK(spalte(film, "key").typ == "VARCHAR(5)");
        CHECK(m.einheit.typen.size() == 2);  // b nach dem Trigger
        REQUIRE(m.einheit.objekte.size() == 1);
        CHECK(m.einheit.objekte[0].tabelle == "film");
        CHECK(m.einheit.objekte[0].ereignis == "AFTER INSERT");
    }

    TEST_CASE("SQLite: ein Trigger mit BEGIN … END und ; darin; was nicht zum Schema gehoert; Unbekanntes")
    {
        const auto e = sq("PRAGMA foreign_keys = ON;\n"
                          "CREATE TRIGGER t AFTER UPDATE ON a BEGIN UPDATE b SET x = 1; DELETE FROM c; END;\n"
                          "CREATE VIEW v AS SELECT * FROM a;\n"
                          "CREATE INDEX i ON a (x);\n"
                          "INSERT INTO a VALUES (1, 'x;y');\n"
                          "WITH q AS (SELECT 1) SELECT * FROM q;\n"
                          "CREATE TABLE a2 AS SELECT * FROM a;\n"
                          "FROBNICATE everything;\n");
        CHECK(e.einheit.typen.size() == 1);
        CHECK(tabelle(e, "a2").felder.empty());
        CHECK(meldet(e, "columns of table 'a2' are not listed"));
        CHECK(e.einheit.objekte.size() == 2);  // Trigger und View
        CHECK(zahl(e, quelle::Schwere::Warnung) == 1);
        CHECK(meldet(e, "statement 'FROBNICATE' is not understood"));
        CHECK(e.meldungen.back().ort.zeile > 0);
    }

    TEST_CASE("Befehle der Werkzeuge (psql, SQLCMD, SQL*Plus), SET TERM (Firebird), Bloecke auf oberster Ebene")
    {
        const auto w = sq("\\connect chinook\n"
                          ":r .\\Daten\\Laden.sql\n"
                          "CONNECT chinook/p4ss\n;\n"
                          "SET TERM ^ ;\n"
                          "CREATE TRIGGER t FOR a BEFORE INSERT AS BEGIN IF (NEW.x > 0) THEN BEGIN NEW.x = 1; END END^\n"
                          "SET TERM ; ^\n"
                          "CREATE TABLE a (x int);\n"
                          "exit;\n");
        CHECK(zahl(w, quelle::Schwere::Warnung) == 0);
        CHECK(zahl(w, quelle::Schwere::Fehler) == 0);
        CHECK(w.einheit.typen.size() == 1);

        // Spark SQL Scripting: ein Block mit Marke, WHILE … DO, CASE … END CASE, REPEAT … UNTIL
        const auto s = sq("BEGIN\n"
                          "  DECLARE i INT DEFAULT 0;\n"
                          "  L0: WHILE i < 3 DO\n"
                          "    CASE WHEN i = 1 THEN SELECT 1; ELSE SELECT 2; END CASE;\n"
                          "    REPEAT SET i = i + 1; UNTIL i > 2 END REPEAT;\n"
                          "    IF i > 1 THEN LEAVE L0; END IF;\n"
                          "  END WHILE L0;\n"
                          "END;\n"
                          "L1: BEGIN SELECT 1; END L1;\n"
                          "BEGIN TRANSACTION;\n"
                          "IF OBJECT_ID('a') IS NOT NULL BEGIN DROP TABLE a; PRINT 'x'; END\n"
                          "CREATE TABLE b (y int);");
        CHECK(zahl(s, quelle::Schwere::Warnung) == 0);
        CHECK(s.einheit.typen.size() == 1);

        // MySQL: `DELIMITER $$` — das `$$` hinter END ist der Trenner, kein Rumpf; Spark: IDENTIFIER('…')
        const auto m = sq("DELIMITER $$\n"
                          "CREATE PROCEDURE p() BEGIN SELECT 1; END $$\n"
                          "DELIMITER ;\n"
                          "CREATE TABLE IDENTIFIER('tab')(c1 INT) USING CSV;");
        CHECK(m.meldungen.empty());
        CHECK(m.einheit.objekte.size() == 1);
        CHECK(spalte(tabelle(m, "tab"), "c1").typ == "INT");
    }

    TEST_CASE("Eine offene Klammer endet am Trenner, mit Meldung — sie verschluckt nicht den Rest")
    {
        const auto e = sq("SELECT CASE WHEN f(rand(0) * 0 < 1 THEN 1 ELSE -1 END;\nCREATE TABLE z (a int);");
        CHECK(e.einheit.typen.size() == 1);
        REQUIRE(e.meldungen.size() == 1);
        CHECK(e.meldungen[0].text == "'(' is not closed");
        CHECK(e.meldungen[0].ort.spalte == 19);  // die Klammer hinter `f`
    }

    TEST_CASE("Ein Fremdschluessel auf eine Tabelle einer anderen Datei: Hinweis, keine Kante")
    {
        const auto e = sq("CREATE TABLE a (x int REFERENCES woanders(id));");
        CHECK(meldet(e, "referenced table 'woanders' is not defined in this file"));
        CHECK(e.meldungen.front().ort.zeile == 1);
        const diagramm::Strukturdiagramm d = diagramm::strukturVon(e.einheit);
        CHECK(d.beziehungen.empty());
    }

    TEST_CASE("ER-Diagramm: Kasten «table», Fremdschluessel als Kante mit den Spalten, kein Verweis ueber den Typ")
    {
        const auto e = sq("CREATE TABLE int (v int);\n"  // eine Tabelle, die wie der Typ einer Spalte heisst
                          "CREATE TABLE kunde (id int PRIMARY KEY);\n"
                          "CREATE TABLE auftrag (id int PRIMARY KEY, kunde int, FOREIGN KEY (kunde) REFERENCES kunde (id));");
        const diagramm::Strukturdiagramm d = diagramm::strukturVon(e.einheit);
        REQUIRE(d.kaesten.size() == 3);
        CHECK(d.kaesten[2].art == diagramm::KastenArt::Tabelle);
        CHECK(d.kaesten[2].name == "auftrag");
        CHECK(d.kaesten[2].eintraege[0].text() == "id: int PK");
        REQUIRE(d.beziehungen.size() == 1);
        CHECK(d.beziehungen[0].von == 2);
        CHECK(d.beziehungen[0].nach == 1);
        CHECK(d.beziehungen[0].art == diagramm::BeziehungArt::Verweist);
        CHECK(d.beziehungen[0].beschriftung == "kunde");

        for (const std::string_view f : {"svg", "mermaid", "dot", "drawio"})
        {
            const ausgabe::Format* format = ausgabe::format(f);
            REQUIRE(format != nullptr);
            const std::string text = format->schreibe(d, {});
            CHECK_MESSAGE(text.find("table") != std::string::npos, f);
            CHECK_MESSAGE(text.find("auftrag") != std::string::npos, f);
        }
    }

    TEST_CASE("Zugriffe (A2, A3): View, Trigger, Routinen mit Rumpf; WITH, temporaere Tabellen, dynamisches SQL")
    {
        using modell::ZugriffArt;
        const auto art = [](const modell::Datenbankobjekt& o, ZugriffArt a, std::string_view ziel) {
            return std::count_if(o.zugriffe.begin(), o.zugriffe.end(), [&](const modell::Zugriff& z) {
                       return z.art == a && z.ziel == ziel && !z.imAusdruck;
                   }) == 1;
        };
        const auto pg = sq("CREATE VIEW public.umsatz AS\n"
                           "  WITH monat AS (SELECT * FROM public.payment)\n"
                           "  SELECT c.name, EXTRACT(YEAR FROM m.datum) FROM monat m JOIN public.customer c ON c.id = m.c,\n"
                           "         store s;\n"
                           "CREATE FUNCTION public.last_updated() RETURNS trigger LANGUAGE plpgsql AS $$\n"
                           "BEGIN\n"
                           "    NEW.last_update = CURRENT_TIMESTAMP;\n"
                           "    INSERT INTO audit (t) VALUES (TG_TABLE_NAME);\n"
                           "    UPDATE stats SET n = n + 1 WHERE id = 1;\n"
                           "    EXECUTE format('DELETE FROM %I', x);\n"
                           "    RETURN NEW;\n"
                           "END $$;\n"
                           "CREATE TRIGGER last_updated BEFORE UPDATE ON public.actor FOR EACH ROW EXECUTE FUNCTION public.last_updated();\n"
                           "CREATE FUNCTION py() RETURNS int AS $$ return 1 $$ LANGUAGE plpython3u;\n"
                           "SELECT * FROM public.actor;\n",
                           "postgres");
        REQUIRE(pg.einheit.objekte.size() == 4);
        const modell::Datenbankobjekt& view = pg.einheit.objekte[0];
        CHECK(view.art == modell::ObjektArt::View);
        CHECK(view.name == "public.umsatz");
        CHECK(art(view, ZugriffArt::Liest, "public.payment"));
        CHECK(art(view, ZugriffArt::Liest, "public.customer"));
        CHECK(art(view, ZugriffArt::Liest, "store"));
        CHECK_FALSE(art(view, ZugriffArt::Liest, "monat"));  // ein Name des WITH
        CHECK_FALSE(art(view, ZugriffArt::Liest, "m.datum"));
        CHECK(std::none_of(view.zugriffe.begin(), view.zugriffe.end(),
                           [](const modell::Zugriff& z) { return z.art == ZugriffArt::Liest && z.ziel == "m"; }));

        const modell::Datenbankobjekt& f = pg.einheit.objekte[1];
        CHECK(f.art == modell::ObjektArt::Funktion);
        CHECK(art(f, ZugriffArt::Schreibt, "audit"));
        CHECK(art(f, ZugriffArt::Schreibt, "stats"));
        const auto audit = std::find_if(f.zugriffe.begin(), f.zugriffe.end(), [](const modell::Zugriff& z) { return z.ziel == "audit"; });
        REQUIRE(audit != f.zugriffe.end());
        CHECK(audit->zeile == 8);  // im Rumpf, auf die Zeile der Datei umgerechnet
        CHECK(meldet(pg, "dynamic SQL is not read"));
        const auto dyn = std::find_if(pg.meldungen.begin(), pg.meldungen.end(),
                                      [](const quelle::Meldung& m) { return m.text == "dynamic SQL is not read"; });
        CHECK(dyn->ort.zeile == 10);

        const modell::Datenbankobjekt& tr = pg.einheit.objekte[2];
        CHECK(tr.art == modell::ObjektArt::Trigger);
        CHECK(tr.tabelle == "public.actor");
        CHECK(tr.ereignis == "BEFORE UPDATE");
        CHECK(art(tr, ZugriffArt::Ruft, "public.last_updated"));
        CHECK(pg.einheit.objekte[3].zugriffe.empty());
        CHECK(meldet(pg, "routine 'py' in LANGUAGE plpython3u is not read"));
        REQUIRE(pg.einheit.zugriffe.size() == 1);  // die Abfrage auf oberster Ebene
        CHECK(pg.einheit.zugriffe[0].ziel == "public.actor");

        // T-SQL: Trigger ON t AFTER INSERT AS, Prozedur mit #temp, EXEC einer anderen, EXEC(@sql)
        const auto ts = sq("CREATE TRIGGER [Sales].[t_neu] ON [Sales].[Orders] AFTER INSERT, UPDATE AS\n"
                           "BEGIN INSERT INTO [Sales].[Log] (x) SELECT x FROM inserted; END\n"
                           "GO\n"
                           "CREATE PROCEDURE dbo.Lade @n INT AS\n"
                           "BEGIN\n"
                           "  CREATE TABLE #zwischen (x INT);\n"
                           "  SELECT * INTO #k FROM dbo.Kunde WHERE n = @n;\n"
                           "  INSERT INTO #zwischen SELECT x FROM #k;\n"
                           "  DELETE FROM dbo.Alt;\n"
                           "  EXEC dbo.Protokoll @n;\n"
                           "  EXEC (@sql);\n"
                           "END\n"
                           "GO\n",
                           "tsql");
        REQUIRE(ts.einheit.objekte.size() == 2);
        CHECK(ts.einheit.objekte[0].tabelle == "Sales.Orders");
        CHECK(ts.einheit.objekte[0].ereignis == "AFTER INSERT, UPDATE");
        CHECK(art(ts.einheit.objekte[0], ZugriffArt::Schreibt, "Sales.Log"));
        CHECK(art(ts.einheit.objekte[0], ZugriffArt::Liest, "inserted"));
        const modell::Datenbankobjekt& p = ts.einheit.objekte[1];
        CHECK(p.art == modell::ObjektArt::Prozedur);
        CHECK(art(p, ZugriffArt::Liest, "dbo.Kunde"));
        CHECK(art(p, ZugriffArt::Schreibt, "dbo.Alt"));
        CHECK(art(p, ZugriffArt::Ruft, "dbo.Protokoll"));
        CHECK(std::none_of(p.zugriffe.begin(), p.zugriffe.end(), [](const modell::Zugriff& z) {
            return z.ziel.find('#') != std::string::npos || z.ziel == "zwischen";
        }));
        CHECK(meldet(ts, "dynamic SQL is not read"));
        CHECK(ts.einheit.typen.empty());  // #zwischen gehoert der Prozedur
    }

    TEST_CASE("Zugriffe (C1): CREATE TABLE … AS SELECT liest seine Quellen und schreibt die Tabelle")
    {
        using modell::ZugriffArt;
        const auto hat = [](const std::vector<modell::Zugriff>& z, ZugriffArt a, std::string_view ziel) {
            return std::any_of(z.begin(), z.end(), [&](const modell::Zugriff& x) { return x.art == a && x.ziel == ziel; });
        };
        // auf oberster Ebene (Spark mit USING und Kommentar dazwischen), ohne Abfrage keine Zugriffe
        const auto e = sq("CREATE OR REPLACE TABLE gold USING delta COMMENT 'x' AS\n"
                          "SELECT k.id, sum(b.n) FROM silber k JOIN bestellung b ON b.k = k.id GROUP BY k.id;\n"
                          "CREATE TABLE leer (a int);\n"
                          "CREATE TEMP TABLE t AS SELECT * FROM quelle;\n",
                          "spark");
        const std::vector<modell::Zugriff>& z = e.einheit.zugriffe;
        CHECK(hat(z, ZugriffArt::Schreibt, "gold"));
        CHECK(hat(z, ZugriffArt::Liest, "silber"));
        CHECK(hat(z, ZugriffArt::Liest, "bestellung"));
        CHECK(hat(z, ZugriffArt::Liest, "quelle"));
        CHECK_FALSE(hat(z, ZugriffArt::Schreibt, "leer"));
        CHECK_FALSE(hat(z, ZugriffArt::Schreibt, "t"));  // eine temporaere Tabelle gehoert dem, der sie anlegt
        // dazu `sum(` als Aufruf im Ausdruck — er zaehlt erst, wo das Projekt eine Routine des Namens kennt
        CHECK(std::count_if(z.begin(), z.end(), [](const modell::Zugriff& x) { return !x.imAusdruck; }) == 4);

        // im Rumpf einer Routine: das Schreiben gehoert ihr, und ein spaeteres AS SELECT gehoert nicht dazu
        const auto r = sq("CREATE FUNCTION f() RETURNS void LANGUAGE plpgsql AS $$\n"
                          "BEGIN\n"
                          "    CREATE TABLE kopie AS SELECT * FROM kurs;\n"
                          "    CREATE TABLE nur (a int);\n"
                          "    CREATE VIEW v AS SELECT 1 FROM kurs;\n"
                          "END $$;\n",
                          "postgres");
        REQUIRE(r.einheit.objekte.size() == 1);
        const std::vector<modell::Zugriff>& o = r.einheit.objekte[0].zugriffe;
        CHECK(hat(o, ZugriffArt::Schreibt, "kopie"));
        CHECK(hat(o, ZugriffArt::Liest, "kurs"));
        CHECK_FALSE(hat(o, ZugriffArt::Schreibt, "nur"));
        const auto kopie = std::find_if(o.begin(), o.end(), [](const modell::Zugriff& x) { return x.ziel == "kopie"; });
        REQUIRE(kopie != o.end());
        CHECK(kopie->zeile == 3);
    }

    TEST_CASE("Die Sprache: Endungen, kein Praeprozessor")
    {
        CHECK(sprache::spracheFuer("schema.SQL") == &sprache::sql());
        CHECK(sprache::spracheFuer("x.ddl") == &sprache::sql());
        CHECK_FALSE(sprache::sql().hatPraeprozessor());
    }
}
