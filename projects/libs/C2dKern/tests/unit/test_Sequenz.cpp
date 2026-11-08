/**
 * @file test_Sequenz.cpp
 * @brief Start der Kerne als Sequenzdiagramm (Entscheide SQ1–SQ4): `kerne` in der Projektdatei, je Kern gelesen,
 *        Boot, Barrieren (auch ueber eine eigene Huelle), Signale an Handler, Hauptschleife, Formate
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/projekt/Sequenz.hpp"

#include "TempOrdner.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace c2d;
using diagramm::SequenzSchritt;

namespace
{

/// Zwei Kerne wie beispiele/kerne: CPU1 bootet CPU2, Barriere 31 ueber eine Huelle, Barriere 30 direkt, im Lauf FLAG 2
/// an den Handler von CPU2 (IPC_registerInterrupt), der mit FLAG 1 an den von CPU1 (Vektortabelle) antwortet.
void legeKerne(TempOrdner& o)
{
    o.lege("code2diagram.json", R"({"kerne": [
        {"name": "CPU1", "ordner": ["cpu1", "global"], "definitionen": ["CPU1"]},
        {"name": "CPU2", "ordner": ["cpu2", "global"], "definitionen": ["CPU2"], "start": "main"}]})");
    o.lege("cpu1/main_cpu1.c",
           "static unsigned short SYNC_BEGRENZT(unsigned long Flag)\n"         // 1
           "{\n"                                                              // 2
           "    IPC_setFlagLtoR(IPC_CPU1_L_CPU2_R, Flag);\n"                   // 3
           "    while (IPC_isFlagBusyRtoL(IPC_CPU1_L_CPU2_R, Flag) == false) {}\n"  // 4
           "    IPC_ackFlagRtoL(IPC_CPU1_L_CPU2_R, Flag);\n"                   // 5
           "    return 1U;\n"                                                  // 6
           "}\n"                                                              // 7
           "__interrupt void cpu1_isr(void) { werte_aus(); }\n"               // 8
           "void starte_cpu2(void)\n"                                          // 9
           "{\n"                                                              // 10
           "    IPC_setBootMode(IPC_CPU1_L_CPU2_R, BOOT_KEY);\n"               // 11
           "    IPC_setFlagLtoR(IPC_CPU1_L_CPU2_R, IPC_FLAG0);\n"              // 12
           "}\n"                                                              // 13
           "void main(void)\n"                                                 // 14
           "{\n"                                                              // 15
           "    volatile unsigned long *vector;\n"                             // 16
           "    init_gemeinsam();\n"                                           // 17
           "    starte_cpu2();\n"                                              // 18
           "    lade();\n"                                                     // 19
           "    if (SYNC_BEGRENZT(IPC_FLAG31) == 0U) { fehler(); }\n"          // 20
           "    vector = (volatile unsigned long *)(PIEVECTTABLE_BASE + ((INT_CIPC1 >> 16U) * 2U));\n"  // 21
           "    *vector = (unsigned long)(&cpu1_isr);\n"                       // 22
           "    IPC_sync(IPC_CPU1_L_CPU2_R, IPC_FLAG30);\n"                    // 23
           "    for (;;) {\n"                                                  // 24
           "        IPC_setFlagLtoR(IPC_CPU1_L_CPU2_R, IPC_FLAG2);\n"          // 25
           "    }\n"                                                           // 26
           "    nie();\n"                                                      // 27
           "}\n");                                                            // 28
    o.lege("cpu2/main_cpu2.c",
           "__interrupt void cpu2_isr(void)\n"                                 // 1
           "{\n"                                                              // 2
           "    filtere();\n"                                                  // 3
           "    IPC_setFlagLtoR(IPC_CPU2_L_CPU1_R, IPC_FLAG1);\n"              // 4
           "}\n"                                                              // 5
           "void main(void)\n"                                                 // 6
           "{\n"                                                              // 7
           "    init_gemeinsam();\n"                                           // 8
           "    IPC_sync(IPC_CPU2_L_CPU1_R, IPC_FLAG31);\n"                    // 9
           "    IPC_registerInterrupt(IPC_CPU2_L_CPU1_R, IPC_INT2, cpu2_isr);\n"  // 10
           "    IPC_sync(IPC_CPU2_L_CPU1_R, IPC_FLAG30);\n"                    // 11
           "    for (;;) { warte(); }\n"                                       // 12
           "}\n");                                                            // 13
    o.lege("global/gemeinsam.c",
           "void init_gemeinsam(void)\n"
           "{\n"
           "#if defined(CPU1)\n"
           "    init_takt();\n"
           "#else\n"
           "    warte_auf_takt();\n"
           "#endif\n"
           "}\n");
}

std::vector<std::string> texte(const diagramm::Sequenzdiagramm& d)
{
    std::vector<std::string> aus;
    for (const SequenzSchritt& s : d.schritte)
    {
        std::string t;
        switch (s.art)
        {
            case SequenzSchritt::Art::Nachricht: t = std::to_string(s.von) + ">" + (s.nach == diagramm::kKeinZiel ? "?" : std::to_string(s.nach)) + " " + s.text; break;
            case SequenzSchritt::Art::Barriere: t = "| " + s.text; break;
            case SequenzSchritt::Art::FragmentAnfang: t = std::string(diagramm::fragmentWort(s.fragment)) + " [" + s.text + "]"; break;
            case SequenzSchritt::Art::Sonst: t = "else [" + s.text + "]"; break;
            case SequenzSchritt::Art::FragmentEnde: t = "end"; break;
        }
        aus.push_back(t);
    }
    return aus;
}

bool hat(const std::vector<std::string>& l, const std::string& t)
{
    return std::find(l.begin(), l.end(), t) != l.end();
}

std::size_t stelle(const std::vector<std::string>& l, const std::string& t)
{
    return static_cast<std::size_t>(std::find(l.begin(), l.end(), t) - l.begin());
}

}  // namespace

TEST_SUITE("Sequenz")
{
    TEST_CASE("Projektdatei: kerne lesen und schreiben, Fehler gemeldet")
    {
        std::vector<quelle::Meldung> m;
        const projekt::Projektdatei p = projekt::liesProjektdatei(
            R"({"kerne": [{"name": "CPU1", "ordner": ["a", "g"], "definitionen": ["CPU1"]}, {"ordner": ["b"]},
                          {"name": "CPU1", "ordner": ["c"]}, {"name": "CPU2", "ordner": ["b"], "start": "boot", "x": 1}]})",
            m);
        REQUIRE(p.kerne.size() == 2);
        CHECK(p.kerne[0].name == "CPU1");
        CHECK(p.kerne[0].ordner == std::vector<std::string>{"a", "g"});
        CHECK(p.kerne[0].start == "main");
        CHECK(p.kerne[1].start == "boot");
        CHECK(m.size() == 3);  // ohne Namen, doppelt, unbekannter Schluessel
        std::vector<quelle::Meldung> wieder;
        const projekt::Projektdatei q = projekt::liesProjektdatei(projekt::schreibe(p), wieder);
        CHECK(wieder.empty());
        REQUIRE(q.kerne.size() == 2);
        CHECK(q.kerne[1].ordner == std::vector<std::string>{"b"});
        CHECK(q.kerne[0].definitionen.front().name == "CPU1");
        // ohne kerne bleibt der Text wie bisher (kein leerer Schluessel)
        CHECK(projekt::schreibe(projekt::Projektdatei{}).find("kerne") == std::string::npos);
    }

    TEST_CASE("liesKern: ein geteilter Ordner je Kern mit dessen Definitionen")
    {
        TempOrdner o;
        legeKerne(o);
        const projekt::Projekt p = projekt::liesOrdner(o.weg());
        const std::vector<projekt::KernProjekt> kerne = projekt::liesKerne(p);
        REQUIRE(kerne.size() == 2);
        CHECK(kerne[0].projekt.dateien.size() == 2);  // cpu1, global
        CHECK(kerne[1].projekt.dateien.size() == 2);  // cpu2, global
        const auto rufe = [](const projekt::Projekt& q) {
            const std::size_t g = q.finde("global/gemeinsam.c");
            REQUIRE(g != projekt::kKeine);
            return q.dateien[g].ergebnis.einheit.funktionen.front().rumpf->kinder.front()->text;
        };
        CHECK(rufe(kerne[0].projekt) == "init_takt()");
        CHECK(rufe(kerne[1].projekt) == "warte_auf_takt()");
    }

    TEST_CASE("Start der Kerne: Boot, Barrieren (auch ueber eine Huelle), par, Hauptschleife, Handler")
    {
        TempOrdner o;
        legeKerne(o);
        const projekt::Projekt p = projekt::liesOrdner(o.weg());
        const diagramm::Sequenzdiagramm d = projekt::startDerKerne(projekt::liesKerne(p));
        REQUIRE(d.linien.size() == 2);
        const std::vector<std::string> t = texte(d);
        // CPU1 bis zum Boot allein, der Boot aus starte_cpu2 (FLAG 0 gehoert dazu)
        CHECK(t[0] == "0>0 init_gemeinsam()");
        CHECK(t[1] == "0>0 starte_cpu2()");
        CHECK(t[2] == "0>1 boot (in starte_cpu2)");
        // dann beide zugleich bis zur Barriere 31 — auf CPU1 ueber die Huelle SYNC_BEGRENZT
        CHECK(t[3] == "par [CPU1]");
        CHECK(hat(t, "| barrier FLAG 31"));
        CHECK(hat(t, "| barrier FLAG 30"));
        CHECK(stelle(t, "| barrier FLAG 31") < stelle(t, "| barrier FLAG 30"));
        CHECK(stelle(t, "0>0 lade()") < stelle(t, "| barrier FLAG 31"));
        CHECK(stelle(t, "1>1 init_gemeinsam()") < stelle(t, "| barrier FLAG 31"));
        // das if der Huelle steht hinter der Barriere
        CHECK(stelle(t, "opt [SYNC_BEGRENZT(IPC_FLAG31) == 0U]") > stelle(t, "| barrier FLAG 31"));
        // die Hauptschleife, das Signal darin mit seinem Empfaenger; was danach steht, laeuft nie
        CHECK(hat(t, "loop [main loop: for (;;)]"));
        CHECK(hat(t, "0>1 FLAG 2 \xE2\x86\x92 cpu2_isr"));
        CHECK_FALSE(hat(t, "0>0 nie()"));
        // je Handler ein Abschnitt; der von CPU2 antwortet mit FLAG 1 an den von CPU1 (aus der Vektortabelle)
        CHECK(hat(t, "section [on IPC flag 1 (CPU1): cpu1_isr]"));
        CHECK(hat(t, "section [on IPC flag 2 (CPU2): cpu2_isr]"));
        CHECK(hat(t, "1>0 FLAG 1 \xE2\x86\x92 cpu1_isr"));
        // nur der feste Hinweis, kein Signal ohne Empfaenger, keine Barriere ohne Partner
        CHECK(d.hinweise == std::vector<std::string>{"data in shared memory is not shown as messages"});
        // die Stelle eines Schritts
        const auto boot = std::find_if(d.schritte.begin(), d.schritte.end(),
                                       [](const SequenzSchritt& s) { return s.nachricht == diagramm::NachrichtArt::Boot; });
        REQUIRE(boot != d.schritte.end());
        CHECK(boot->pfad == "cpu1/main_cpu1.c");
        CHECK(boot->zeile == 11);
    }

    TEST_CASE("Tiefe: 0 zeigt nur die IPC, 2 auch was gerufene Funktionen rufen")
    {
        TempOrdner o;
        legeKerne(o);
        const projekt::Projekt p = projekt::liesOrdner(o.weg());
        const auto kerne = projekt::liesKerne(p);
        const std::vector<std::string> null = texte(projekt::startDerKerne(kerne, {.tiefe = 0}));
        CHECK_FALSE(hat(null, "0>0 init_gemeinsam()"));
        CHECK(hat(null, "0>1 boot (in starte_cpu2)"));
        CHECK(hat(null, "| barrier FLAG 31"));
        const std::vector<std::string> zwei = texte(projekt::startDerKerne(kerne, {.tiefe = 2}));
        CHECK(hat(zwei, "0>0 init_takt()"));        // in init_gemeinsam, mit CPU1
        CHECK(hat(zwei, "1>1 warte_auf_takt()"));   // dieselbe Datei, mit CPU2
    }

    TEST_CASE("Ohne Partner und ohne Empfaenger: Hinweise; Formate")
    {
        TempOrdner o;
        o.lege("code2diagram.json", R"({"kerne": [{"name": "A", "ordner": ["a"]}, {"name": "B", "ordner": ["b"]}]})");
        o.lege("a/a.c", "void main(void) { IPC_sync(IPC_CPU1_L_CPU2_R, IPC_FLAG5); IPC_setFlagLtoR(IPC_CPU1_L_CPU2_R, IPC_FLAG3); }\n");
        o.lege("b/b.c", "void main(void) { IPC_sync(IPC_CPU2_L_CPU1_R, IPC_FLAG6); }\n");
        const projekt::Projekt p = projekt::liesOrdner(o.weg());
        const diagramm::Sequenzdiagramm d = projekt::startDerKerne(projekt::liesKerne(p));
        const auto enthaelt = [&](const std::string& teil) {
            return std::any_of(d.hinweise.begin(), d.hinweise.end(), [&](const std::string& h) { return h.find(teil) != std::string::npos; });
        };
        CHECK(enthaelt("barrier FLAG 5 on A"));
        CHECK(enthaelt("barrier FLAG 6 on B"));
        CHECK(enthaelt("FLAG 3 from A"));
        // Mermaid, SVG, draw.io; DOT und csv sagen ab
        const std::string m = ausgabe::format("mermaid")->schreibe(d, {});
        CHECK(m.find("sequenceDiagram") != std::string::npos);
        CHECK(m.find("participant L0 as A") != std::string::npos);
        CHECK(m.find("L0-)L1: FLAG 3") != std::string::npos);
        CHECK(ausgabe::format("svg")->schreibe(d, {}).starts_with("<svg"));
        CHECK(ausgabe::format("drawio")->schreibe(d, {}).find("<mxfile") != std::string::npos);
        CHECK_FALSE(ausgabe::format("dot")->kann(ausgabe::Diagrammart::Sequenz));
        CHECK_FALSE(ausgabe::format("csv")->kann(ausgabe::Diagrammart::Sequenz));
        // die Anordnung: je Pfeil und Barriere ein Ort, nichts ausserhalb
        const diagramm::SequenzAnordnung a = diagramm::ordneAn(d);
        CHECK(a.koepfe.size() == 2);
        CHECK(a.barrieren.size() == 2);
        for (const auto& pf : a.pfeile)
            for (const diagramm::Punkt& pt : pf.linie) CHECK((pt.x >= 0 && pt.x <= a.breite && pt.y >= 0 && pt.y <= a.hoehe));
    }

    TEST_CASE("Boot in einem if: FLAG 0 gehoert dazu, der Kern beginnt hinter dem if")
    {
        TempOrdner o;
        o.lege("code2diagram.json", R"({"kerne": [{"name": "CPU1", "ordner": ["a"]}, {"name": "CPU2", "ordner": ["b"]}]})");
        o.lege("a/a.c", "void boote(void) { IPC_setBootMode(IPC_CPU1_L_CPU2_R, 1); IPC_setFlagLtoR(IPC_CPU1_L_CPU2_R, IPC_FLAG0); }\n"
                        "void main(void) { if (im_reset()) { boote(); } eins(); IPC_sync(IPC_CPU1_L_CPU2_R, IPC_FLAG31); }\n");
        o.lege("b/b.c", "void main(void) { zwei(); IPC_sync(IPC_CPU2_L_CPU1_R, IPC_FLAG31); }\n");
        const projekt::Projekt p = projekt::liesOrdner(o.weg());
        const diagramm::Sequenzdiagramm d = projekt::startDerKerne(projekt::liesKerne(p));
        const std::vector<std::string> t = texte(d);
        CHECK(hat(t, "0>1 boot (in boote)"));
        CHECK_FALSE(hat(t, "0>1 FLAG 0 (in boote)"));
        // CPU2 beginnt erst hinter dem if: zwei() steht nach dem Ende des opt
        CHECK(stelle(t, "1>1 zwei()") > stelle(t, "0>1 boot (in boote)"));
        CHECK(stelle(t, "1>1 zwei()") > stelle(t, "opt [im_reset()]"));
        CHECK(hat(t, "| barrier FLAG 31"));
        CHECK(d.hinweise.size() == 1);
    }

    TEST_CASE("Vorschlag fuer kerne aus Namen (SQ2): Ordner, Dateinamen, geteilte Ordner mit #if CPU1")
    {
        {
            TempOrdner o;
            legeKerne(o);
            const std::vector<projekt::Kern> k = projekt::vorschlagKerne(projekt::liesOrdner(o.weg()));
            REQUIRE(k.size() == 2);
            CHECK(k[0].name == "CPU1");
            CHECK(k[0].ordner == std::vector<std::string>{"cpu1", "global"});
            CHECK(k[1].ordner == std::vector<std::string>{"cpu2", "global"});
            CHECK(k[1].definitionen.front().name == "CPU2");
            CHECK(k[1].start == "main");
        }
        {
            // wie LLC: Master_CLA und Slave nennen den Kern, Global fragt nach CPU1
            TempOrdner o;
            o.lege("fw/Master_CLA/Master/Source_Files/Main_CPU1.c", "void main(void) {}\n");
            o.lege("fw/Master_CLA/CLA/x.c", "void x(void) {}\n");
            o.lege("fw/Slave/Source_Files/Main_CPU2.c", "void main(void) {}\n");
            o.lege("fw/Global/Source_Files/g.c", "#if defined(CPU1)\nvoid a(void) {}\n#endif\n");
            o.lege("fw/Tools/t.c", "void t(void) {}\n");
            const std::vector<projekt::Kern> k = projekt::vorschlagKerne(projekt::liesOrdner(o.weg()));
            REQUIRE(k.size() == 2);
            CHECK(k[0].ordner == std::vector<std::string>{"fw/Master_CLA", "fw/Global"});
            CHECK(k[1].ordner == std::vector<std::string>{"fw/Slave", "fw/Global"});
        }
        {
            // nur ein Kern: kein Vorschlag
            TempOrdner o;
            o.lege("cpu1/a.c", "void main(void) {}\n");
            CHECK(projekt::vorschlagKerne(projekt::liesOrdner(o.weg())).empty());
        }
    }

    TEST_CASE("Ohne kerne: ein Hinweis, keine Lebenslinie")
    {
        const diagramm::Sequenzdiagramm d = projekt::startDerKerne({});
        CHECK(d.linien.empty());
        CHECK(d.hinweise.front().find("no cores") != std::string::npos);
    }
}
