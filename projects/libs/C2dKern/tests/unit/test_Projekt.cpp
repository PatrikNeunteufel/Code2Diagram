/**
 * @file test_Projekt.cpp
 * @brief Projektdatei, Ordnermuster, `.gitignore`, ein Ordner als Projekt, Aufrufe ueber Dateien
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/projekt/Projekt.hpp"
#include "c2d/projekt/Projektdatei.hpp"
#include "c2d/sprache/Sprache.hpp"

#include "TempOrdner.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace c2d::projekt;

namespace
{

bool hatFunktion(const Datei& d, std::string_view name)
{
    return std::any_of(d.ergebnis.einheit.funktionen.begin(), d.ergebnis.einheit.funktionen.end(),
                       [&](const c2d::modell::Funktion& f) { return f.name == name && f.rumpf; });
}

bool hatMeldung(const Projekt& p, std::string_view pfad, std::string_view stueck)
{
    return std::any_of(p.meldungen.begin(), p.meldungen.end(), [&](const Projektmeldung& m) {
        return m.pfad == pfad && m.meldung.text.find(stueck) != std::string::npos;
    });
}

}  // namespace

TEST_SUITE("Projektdatei")
{
    TEST_CASE("Lesen, schreiben, wieder lesen")
    {
        std::vector<c2d::quelle::Meldung> meldungen;
        const Projektdatei p = liesProjektdatei(R"({
  "definitionen": ["CPU1", "DEBUG=1", "NAME=\"x\\y\""],
  "includePfade": ["firmware/inc"],
  "uebergehen": ["old", "firmware/bak*"],
  "abschnitte": [ { "ordner": "firmware/Slave", "definitionen": ["SLAVE"], "includePfade": ["firmware/Slave/inc"] } ]
})",
                                                meldungen);
        CHECK(meldungen.empty());
        REQUIRE(p.definitionen.size() == 3);
        CHECK(p.definitionen[1].name == "DEBUG");
        CHECK(p.definitionen[1].wert == "1");
        CHECK(p.definitionen[2].wert == "\"x\\y\"");
        CHECK(p.uebergehen == std::vector<std::string>{"old", "firmware/bak*"});
        REQUIRE(p.abschnitte.size() == 1);
        CHECK(p.abschnitte[0].ordner == "firmware/Slave");
        CHECK(p.abschnitte[0].definitionen[0].name == "SLAVE");

        const std::string text = schreibe(p);
        std::vector<c2d::quelle::Meldung> wieder;
        const Projektdatei q = liesProjektdatei(text, wieder);
        CHECK(wieder.empty());
        CHECK(schreibe(q) == text);
        CHECK(text.find("\"definitionen\": [\n    \"CPU1\",\n    \"DEBUG\",") != std::string::npos);  // DEBUG=1 heisst DEBUG
        CHECK(schreibe(Projektdatei{}) == "{\n  \"definitionen\": [],\n  \"includePfade\": [],\n  \"uebergehen\": [],\n"
                                          "  \"abschnitte\": []\n}\n");
    }

    TEST_CASE("Was nicht passt, wird eine Meldung mit Ort; das Uebrige gilt")
    {
        std::vector<c2d::quelle::Meldung> meldungen;
        (void)liesProjektdatei("{\n  \"definitionen\": [\"A\",]\n}", meldungen);
        REQUIRE(meldungen.size() == 1);
        CHECK(meldungen[0].ort.zeile == 2);
        CHECK(meldungen[0].text.starts_with("not valid JSON"));

        meldungen.clear();
        const Projektdatei p = liesProjektdatei(
            R"({"definitionen": ["A", "", 3], "defintionen": [], "uebergehen": "out", "abschnitte": [{"definitionen": ["B"]}]})",
            meldungen);
        CHECK(p.definitionen.size() == 1);
        CHECK(p.uebergehen.empty());
        CHECK(p.abschnitte.empty());
        REQUIRE(meldungen.size() == 5);
        CHECK(meldungen[0].text.find("\"\" is not a definition") != std::string::npos);
        CHECK(meldungen[1].text.find("must be a string, not a number") != std::string::npos);
        CHECK(meldungen[2].text.find("unknown key \"defintionen\"") != std::string::npos);
        CHECK(meldungen[3].text.find("\"uebergehen\" must be a list") != std::string::npos);
        CHECK(meldungen[4].text.find("without \"ordner\"") != std::string::npos);
    }

    TEST_CASE("Ordnermuster")
    {
        CHECK(passt("old", "old"));
        CHECK(passt("old", "a/b/old"));  // ein Name: in jeder Tiefe
        CHECK_FALSE(passt("old", "old/x"));
        CHECK(liegtIn("old", "old/x"));  // darunter liegt er
        CHECK(passt("firmware/bak*", "firmware/bak_2024"));
        CHECK_FALSE(passt("firmware/bak*", "x/firmware/bak1"));  // mit `/`: ab der Wurzel
        CHECK(passt("**/inc", "inc"));
        CHECK(passt("**/inc", "a/b/inc"));
        CHECK(passt("a/*/c", "a/b/c"));
        CHECK_FALSE(passt("a/*/c", "a/b/x/c"));
        CHECK(passt("a/**/c", "a/b/x/c"));
        CHECK(passt("/firmware/Slave/", "firmware/Slave"));
        CHECK(passt("v?", "v1"));
        CHECK_FALSE(passt("Old", "old"));  // Gross und Klein zaehlen
        CHECK_FALSE(passt("", "a"));
        CHECK(wirdImmerUebergangen(".git"));
        CHECK(wirdImmerUebergangen("out"));
        CHECK_FALSE(wirdImmerUebergangen("output"));
    }

    TEST_CASE("Der Vorschlag aus einer .gitignore")
    {
        const Vorschlag v = ausGitignore("# Kommentar\r\nout/\n/build\n*.o\nsichttest-logs/\n!keep/\n"
                                         "docs/_build/\nCMakeUserPresets.json\nold\n.vs/\nold\n");
        CHECK(v.uebergehen == std::vector<std::string>{"sichttest-logs", "docs/_build", "old"});
        CHECK(v.nichtUebernommen == std::vector<std::string>{"*.o", "!keep/", "CMakeUserPresets.json"});
    }
}

TEST_SUITE("Projekt")
{
    TEST_CASE("Ein Ordner: was gelesen, was uebergangen wird; Definitionen aus Projekt, Abschnitt und Kopf")
    {
        TempOrdner t;
        t.lege("code2diagram.json", R"({"definitionen": ["MIT_LOG"], "includePfade": ["inc"], "uebergehen": ["alt"],
                 "abschnitte": [{"ordner": "slave", "definitionen": ["SLAVE=2"]}]})");
        t.lege("inc/config.h", "#define TAKT 100\n#define MIT_KOPF\n");
        t.lege("main.c", "#include \"config.h\"\n#include <stdint.h>\n#include \"fehlt.h\"\n"
                         "static int hilf(void) { return 1; }\n"
                         "int main(void) { hilf(); starte(); return 0; }\n"
                         "#ifdef MIT_KOPF\nint aus_kopf(void) { return 0; }\n#endif\n"
                         "#ifdef MIT_LOG\nvoid log_an(void) {}\n#endif\n"
                         "#if TAKT > 50\nvoid schnell(void) {}\n#endif\n");
        t.lege("motor/motor.c", "static int hilf(void) { return 2; }\nvoid starte(void) { hilf(); }\n"
                                "#if SLAVE == 2\nvoid nur_slave(void) {}\n#endif\n");
        t.lege("slave/s.c", "#if SLAVE == 2\nvoid nur_slave(void) {}\n#endif\nvoid starte(void) {}\n");
        t.lege("alt/x.c", "void alt(void) {}\n");
        t.lege("out/y.c", "void y(void) {}\n");
        t.lege(".git/z.c", "void z(void) {}\n");
        t.lege("liesmich.txt", "kein Quelltext\n");

        const Projekt p = liesOrdner(t.weg());
        CHECK(p.mitProjektdatei);
        std::vector<std::string> pfade;
        for (const Datei& d : p.dateien) pfade.push_back(d.pfad);
        CHECK(pfade == std::vector<std::string>{"inc/config.h", "main.c", "motor/motor.c", "slave/s.c"});
        CHECK(p.uebergangen == std::vector<std::string>{".git", "alt", "out"});
        CHECK(hatMeldung(p, "alt", "folder skipped (\"uebergehen\": \"alt\")"));
        CHECK(hatMeldung(p, "out", "folder skipped (always skipped)"));

        const Datei& haupt = p.dateien[p.finde("main.c")];
        CHECK(hatFunktion(haupt, "log_an"));    // aus dem Projekt
        CHECK(hatFunktion(haupt, "aus_kopf"));  // aus inc/config.h, ueber den Include-Pfad
        CHECK(hatFunktion(haupt, "schnell"));   // TAKT 100 aus dem Kopf
        CHECK(hatMeldung(p, "main.c", "header \"fehlt.h\" not found"));
        CHECK_FALSE(hatMeldung(p, "main.c", "stdint.h"));  // ein Systemkopf ausserhalb: still
        CHECK_FALSE(hatFunktion(p.dateien[p.finde("motor/motor.c")], "nur_slave"));
        CHECK(hatFunktion(p.dateien[p.finde("slave/s.c")], "nur_slave"));  // aus dem Abschnitt
        CHECK(p.funktionen() == 9);

        SUBCASE("von aussen geht vor")
        {
            const Projekt q = liesOrdner(t.weg(), {.definitionen = {{"TAKT", "10"}}});
            CHECK_FALSE(hatFunktion(q.dateien[q.finde("main.c")], "schnell"));
            CHECK(hatFunktion(q.dateien[q.finde("main.c")], "aus_kopf"));
        }
        SUBCASE("Aufrufe ueber Dateien: erst die eigene, dann was nicht static ist; mehrere bleiben offen")
        {
            const std::size_t m = p.finde("main.c");
            const std::size_t mo = p.finde("motor/motor.c");
            const std::vector<Fundstelle> eigene = kandidaten(p, m, "hilf");
            REQUIRE(eigene.size() == 1);
            CHECK(eigene[0].datei == m);
            CHECK(funktion(p, eigene[0]).intern);
            REQUIRE(kandidaten(p, mo, "hilf").size() == 1);
            CHECK(kandidaten(p, mo, "hilf")[0].datei == mo);
            CHECK(kandidaten(p, p.finde("slave/s.c"), "hilf").empty());  // beide static, in fremden Dateien
            const std::vector<Fundstelle> zwei = kandidaten(p, m, "starte");
            REQUIRE(zwei.size() == 2);
            CHECK(zwei[0].datei == mo);
            CHECK(zwei[1].datei == p.finde("slave/s.c"));
            // die Umkehrung: wer ruft? Auch ein mehrdeutiger Aufruf zaehlt fuer jeden seiner Kandidaten
            const std::vector<Fundstelle> rufenStarte = aufrufer(p, zwei[0]);
            REQUIRE(rufenStarte.size() == 1);
            CHECK(rufenStarte[0].datei == m);
            CHECK(funktion(p, rufenStarte[0]).name == "main");
            CHECK(aufrufer(p, zwei[1]) == rufenStarte);
            const std::vector<Fundstelle> rufenHilf = aufrufer(p, kandidaten(p, mo, "hilf")[0]);
            REQUIRE(rufenHilf.size() == 1);  // nur starte in motor.c, nicht main: das meint seine eigene
            CHECK(funktion(p, rufenHilf[0]).name == "starte");
            CHECK(aufrufer(p, eigene[0]).size() == 1);
            CHECK(funktion(p, aufrufer(p, eigene[0])[0]).name == "main");
            CHECK(funktionenNamens(p, "hilf").size() == 2);
            CHECK(funktionenNamens(p, "gibt_es_nicht").empty());
        }
    }

    TEST_CASE("Nicht definiert: !NAME in Projektdatei, Abschnitt und von aussen; Koepfe und die Datei selbst")
    {
        TempOrdner t;
        t.lege("code2diagram.json", R"({"definitionen": ["MIT_LOG", "DEBUG", "!DEBUG", "!MIT_KOPF"], "includePfade": ["inc"],
                 "abschnitte": [{"ordner": "slave", "definitionen": ["!MIT_LOG"]}]})");
        t.lege("inc/config.h", "#define MIT_KOPF\n#define TAKT 100\n");
        t.lege("main.c", "#include \"config.h\"\n#define EIGEN\n"
                         "#ifdef MIT_LOG\nvoid log_an(void) {}\n#endif\n"
                         "#ifdef DEBUG\nvoid dbg(void) {}\n#endif\n"
                         "#ifdef MIT_KOPF\nvoid aus_kopf(void) {}\n#endif\n"
                         "#if TAKT > 50\nvoid schnell(void) {}\n#endif\n"
                         "#ifdef EIGEN\nvoid eigen(void) {}\n#endif\n");
        t.lege("slave/s.c", "#ifdef MIT_LOG\nvoid slave_log(void) {}\n#endif\nvoid s(void) {}\n");

        const Projekt p = liesOrdner(t.weg());
        CHECK(p.meldungen.empty());
        const Datei& haupt = p.dateien[p.finde("main.c")];
        const Datei& slave = p.dateien[p.finde("slave/s.c")];
        CHECK(hatFunktion(haupt, "log_an"));
        CHECK_FALSE(hatFunktion(haupt, "dbg"));     // in einer Liste gilt der spaetere Eintrag
        CHECK(hatFunktion(haupt, "aus_kopf"));      // der Kopf definiert ihn nach der Projektdatei wieder
        CHECK_FALSE(hatFunktion(slave, "slave_log"));  // der Abschnitt hebt auf

        CHECK(herkunft(p.einstellungen, haupt, "MIT_LOG") == Herkunft::projekt);
        CHECK(herkunft(p.einstellungen, haupt, "DEBUG") == Herkunft::projekt);
        CHECK(herkunft(p.einstellungen, haupt, "MIT_KOPF") == Herkunft::kopf);
        CHECK(herkunft(p.einstellungen, haupt, "TAKT") == Herkunft::kopf);
        CHECK(herkunft(p.einstellungen, slave, "MIT_LOG") == Herkunft::abschnitt);
        CHECK(herkunft(p.einstellungen, haupt, "EIGEN") == Herkunft::keine);  // die Datei selbst
        CHECK(herkunft(p.einstellungen, haupt, "GIBT_ES_NICHT") == Herkunft::keine);

        SUBCASE("von aussen hebt auch auf, was ein Kopf definiert - nicht, was die Datei definiert")
        {
            const Projekt q = liesOrdner(t.weg(), {.definitionen = {{"MIT_KOPF", "1", false},
                                                                   {"TAKT", "1", false},
                                                                   {"EIGEN", "1", false},
                                                                   {"DEBUG", "1"}}});
            const Datei& h = q.dateien[q.finde("main.c")];
            CHECK_FALSE(hatFunktion(h, "aus_kopf"));
            CHECK_FALSE(hatFunktion(h, "schnell"));
            CHECK(hatFunktion(h, "eigen"));
            CHECK(hatFunktion(h, "dbg"));
        }
        SUBCASE("mitDefinitionen: das Feld in die Projektdatei, mit Hinweis auf Abschnitte")
        {
            std::vector<std::string> hinweise;
            const Projektdatei neu =
                mitDefinitionen(p.einstellungen, {{"MIT_LOG", "2"}, {"NEU", "1", false}, {"DEBUG", "1"}}, hinweise);
            std::vector<std::string> texte;
            for (const c2d::sprache::Definition& d : neu.definitionen) texte.push_back(c2d::sprache::alsText(d));
            CHECK(texte == std::vector<std::string>{"MIT_LOG=2", "DEBUG", "!MIT_KOPF", "!NEU"});  // das spaetere !DEBUG ist weg
            REQUIRE(hinweise.size() == 1);
            CHECK(hinweise[0].find("\"slave\" sets !MIT_LOG") != std::string::npos);
            hinweise.clear();
            (void)mitDefinitionen(p.einstellungen, {{"MIT_LOG", "1", false}}, hinweise);
            CHECK(hinweise.empty());  // der Abschnitt sagt dasselbe
            // geschrieben und wieder gelesen
            std::vector<c2d::quelle::Meldung> meldungen;
            const Projektdatei wieder = liesProjektdatei(schreibe(neu), meldungen);
            CHECK(meldungen.empty());
            REQUIRE(wieder.definitionen.size() == 4);
            CHECK_FALSE(wieder.definitionen[3].definiert);
        }
    }

    TEST_CASE("Zwei Koepfe definieren denselben Namen: der spaeter eingebundene gilt, auch mittelbar")
    {
        TempOrdner t;
        t.lege("a.h", "#define STUFE 1\n#define NUR_A\n");
        t.lege("b.h", "#include \"c.h\"\n#define STUFE 2\n");
        t.lege("c.h", "#define STUFE 3\n#define AUS_C\n");
        t.lege("main.c", "#include \"a.h\"\n#include \"b.h\"\n"
                         "#if STUFE == 2\nvoid zwei(void) {}\n#endif\n"
                         "#if defined(NUR_A) && defined(AUS_C)\nvoid beide(void) {}\n#endif\n");
        const Projekt p = liesOrdner(t.weg());
        const Datei& haupt = p.dateien[p.finde("main.c")];
        CHECK(hatFunktion(haupt, "zwei"));   // a.h, dann c.h (von b.h), dann b.h selbst
        CHECK(hatFunktion(haupt, "beide"));
        CHECK(herkunft(p.einstellungen, haupt, "STUFE") == Herkunft::kopf);
    }

    TEST_CASE("Ohne Projektdatei: ein Kopf findet sich ueber seinen Namen, wenn es ihn genau einmal gibt")
    {
        TempOrdner t;
        t.lege("a/x.c", "#include \"y.h\"\n#ifdef Y\nvoid mit_y(void) {}\n#endif\n");
        t.lege("b/y.h", "#define Y\n");
        const Projekt p = liesOrdner(t.weg());
        CHECK_FALSE(p.mitProjektdatei);
        CHECK(hatFunktion(p.dateien[p.finde("a/x.c")], "mit_y"));

        t.lege("c/y.h", "#define Z\n");  // jetzt zweimal: keiner gilt
        const Projekt q = liesOrdner(t.weg());
        CHECK_FALSE(hatFunktion(q.dateien[q.finde("a/x.c")], "mit_y"));
        CHECK(hatMeldung(q, "a/x.c", "header \"y.h\" not found"));
    }

    TEST_CASE("Kein Ordner, eine kaputte Projektdatei, ein einzelner Text")
    {
        const Projekt leer = liesOrdner("gibt/es/nicht");
        CHECK(leer.dateien.empty());
        REQUIRE(leer.meldungen.size() == 1);
        CHECK(leer.meldungen[0].meldung.text.starts_with("not a folder"));

        TempOrdner t;
        t.lege("code2diagram.json", "{ kaputt");
        t.lege("a.c", "void a(void) {}\n");
        const Projekt p = liesOrdner(t.weg());
        CHECK(p.dateien.size() == 1);
        CHECK(hatMeldung(p, "code2diagram.json", "not valid JSON"));

        const Projekt e = liesText("static int f(void) { return 0; }", "x.c");
        REQUIRE(e.dateien.size() == 1);
        CHECK(e.wurzel.empty());
        CHECK(e.dateien[0].ergebnis.einheit.funktionen[0].intern);
    }
}

TEST_SUITE("Aufrufe und Namen")
{
    TEST_CASE("aufrufeVon: ueber alle Anweisungen einer Funktion, jeder einmal")
    {
        const c2d::sprache::Ergebnis e = c2d::sprache::c().lies(
            "void f(int x) { a(); if (b(x)) { c(); } else d(); for (int i = e(); i < 3; i += g()) h(); "
            "switch (k()) { case 1: a(); break; } goto ende; ende: return m(\"n(\"); }",
            "t.c");
        REQUIRE(e.einheit.funktionen.size() == 1);
        CHECK(c2d::modell::aufrufeVon(e.einheit.funktionen[0]) ==
              std::vector<std::string>{"a", "b", "c", "d", "e", "g", "h", "k", "m"});
    }

    TEST_CASE("Funktion::heisst: ohne Bereich jeder Bereich, mit Bereich nur der passende")
    {
        c2d::modell::Funktion f;
        f.name = "f";
        f.bereich = "a::N";
        CHECK(f.heisst("f"));
        CHECK(f.heisst("N::f"));
        CHECK(f.heisst("a::N::f"));
        CHECK_FALSE(f.heisst("M::f"));
        CHECK_FALSE(f.heisst("xN::f"));
        CHECK_FALSE(f.heisst("g"));
    }

    TEST_CASE("static vor einer freien Funktion: intern; eine statische Methode nicht")
    {
        const c2d::sprache::Ergebnis c = c2d::sprache::c().lies("static int a(void) { return 0; } int b(void) { return 0; }", "t.c");
        REQUIRE(c.einheit.funktionen.size() == 2);
        CHECK(c.einheit.funktionen[0].intern);
        CHECK_FALSE(c.einheit.funktionen[1].intern);

        const c2d::sprache::Ergebnis cpp = c2d::sprache::cpp().lies(
            "struct S { static int m() { return 1; } }; namespace n { static int h() { return 2; } }", "t.cpp");
        REQUIRE(cpp.einheit.funktionen.size() == 2);
        CHECK_FALSE(cpp.einheit.funktionen[0].intern);
        CHECK(cpp.einheit.funktionen[1].intern);
    }
}
