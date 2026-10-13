/**
 * @file test_CSprache.cpp
 * @brief Der Leser fuer C: was er aus einer Datei ins Codemodell bringt
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "Beispiele.hpp"
#include "c2d/sprache/Sprache.hpp"

using namespace c2d::modell;
using c2d::sprache::Ergebnis;

namespace
{

Ergebnis lies(std::string_view text)
{
    return c2d::sprache::c().lies(text, "test.c");
}

const Typ& typ(const Einheit& e, std::string_view name)
{
    for (const Typ& t : e.typen)
        if (t.name == name) return t;
    FAIL("no type " << name);
    return e.typen.front();
}

/// Die Glieder eines Typs als "typ|name|zusatz", durch Strichpunkt getrennt.
std::string glieder(const Typ& t)
{
    std::string s;
    for (const Feld& f : t.felder) s += f.typ + "|" + f.name + "|" + f.zusatz + ";";
    return s;
}

constexpr std::string_view kFunktionen = R"c(
#include <stdio.h>
#include "eigen.h"
#define MAX 10   /* Obergrenze */
#define QUADRAT(x) ((x) * \
                    (x))

static int zaehler = 0, *zeiger;
extern const char *namen[MAX];
struct Spaeter;

int addiere(int a, int b);

static unsigned long rechne(const uint8_t *daten, size_t laenge, void (*melder)(int))
{
    unsigned long summe = 0;
    for (size_t i = 0; i < laenge; i++) {
        if (daten[i] == 0)
            continue;
        summe += daten[i];
    }
    melder(1);
    return summe;
}

void leer(void) { }
)c";

constexpr std::string_view kAnweisungen = R"c(
int zustand(int z)
{
    switch (z) {
    case 0:
    case 1:
        z++;
        break;
    case 2:
        z = 5;
    default:
        z = 0;
    }
    do {
        z--;
    } while (z > 0);
nochmal:
    if (z < -3) goto ende; else z -= 1;
    goto nochmal;
ende:
    return z;
}
)c";

}  // namespace

TEST_SUITE("C")
{
    TEST_CASE("Die Sprache meldet sich mit ihren Endungen an")
    {
        CHECK(c2d::sprache::spracheFuer("a/b/main.c") == &c2d::sprache::c());
        CHECK(c2d::sprache::spracheFuer("KOPF.H") == &c2d::sprache::c());
        CHECK(c2d::sprache::spracheFuer("main.py") == nullptr);
        CHECK(c2d::sprache::spracheFuer("Makefile") == nullptr);
    }

    TEST_CASE("Praeprozessor: include und define")
    {
        const Ergebnis e = lies(kFunktionen);
        CHECK(e.meldungen.empty());

        REQUIRE(e.einheit.einbindungen.size() == 2);
        CHECK(e.einheit.einbindungen[0].ziel == "stdio.h");
        CHECK(e.einheit.einbindungen[0].system);
        CHECK(e.einheit.einbindungen[1].ziel == "eigen.h");
        CHECK_FALSE(e.einheit.einbindungen[1].system);

        REQUIRE(e.einheit.makros.size() == 2);
        CHECK(e.einheit.makros[0].name == "MAX");
        CHECK(e.einheit.makros[0].text == "10");  // ohne den Kommentar
        CHECK(e.einheit.makros[1].name == "QUADRAT");
        CHECK(e.einheit.makros[1].text == "(x) ((x) * (x))");  // die Fortsetzung ist eine Zeile
    }

    TEST_CASE("Globale Variablen: mehrere Deklaratoren teilen den Typ")
    {
        const Einheit e = lies(kFunktionen).einheit;

        REQUIRE(e.variablen.size() == 3);  // `struct Spaeter;` kuendigt nur an
        CHECK(e.variablen[0].typ == "static int");
        CHECK(e.variablen[0].name == "zaehler");
        CHECK(e.variablen[0].zusatz.empty());  // der Anfangswert gehoert nicht dazu
        CHECK(e.variablen[1].typ == "static int *");
        CHECK(e.variablen[1].name == "zeiger");
        CHECK(e.variablen[2].typ == "extern const char *");
        CHECK(e.variablen[2].name == "namen");
        CHECK(e.variablen[2].zusatz == "[MAX]");
    }

    TEST_CASE("Funktionen: Prototyp, Kopf mit Parametern, leerer Rumpf")
    {
        const Einheit e = lies(kFunktionen).einheit;

        REQUIRE(e.funktionen.size() == 3);
        CHECK(e.funktionen[0].name == "addiere");
        CHECK(e.funktionen[0].rumpf == nullptr);
        CHECK(e.funktionen[0].signatur() == "addiere(int a, int b)");

        const Funktion& f = e.funktionen[1];
        CHECK(f.name == "rechne");
        CHECK(f.rueckgabe == "static unsigned long");
        CHECK(f.zeile == 14);
        REQUIRE(f.parameter.size() == 3);
        CHECK(f.parameter[0].typ == "const uint8_t *");  // ein Name, den die Datei nicht kennt, gilt als Typ
        CHECK(f.parameter[0].name == "daten");
        CHECK(f.parameter[1].typ == "size_t");
        CHECK(f.parameter[2].typ == "void (*)(int)");
        CHECK(f.parameter[2].name == "melder");

        CHECK(e.funktionen[2].signatur() == "leer()");
        REQUIRE(e.funktionen[2].rumpf != nullptr);
        CHECK(e.funktionen[2].rumpf->kinder.empty());
    }

    TEST_CASE("Anweisungen: for, if ohne Block, continue, Aufruf, return")
    {
        const Einheit e = lies(kFunktionen).einheit;
        const Anweisung& rumpf = *e.funktionen[1].rumpf;

        REQUIRE(rumpf.kinder.size() == 4);
        CHECK(rumpf.kinder[0]->art == AnweisungArt::Einfach);
        CHECK(rumpf.kinder[0]->text == "unsigned long summe = 0");
        CHECK(rumpf.kinder[0]->zeile == 16);

        const Anweisung& schleife = *rumpf.kinder[1];
        CHECK(schleife.art == AnweisungArt::Fuer);
        CHECK(schleife.init == "size_t i = 0");
        CHECK(schleife.text == "i < laenge");
        CHECK(schleife.schritt == "i++");
        REQUIRE(schleife.kinder.size() == 1);
        REQUIRE(schleife.kinder[0]->kinder.size() == 2);
        const Anweisung& wenn = *schleife.kinder[0]->kinder[0];
        CHECK(wenn.art == AnweisungArt::Wenn);
        CHECK(wenn.text == "daten[i] == 0");
        CHECK(wenn.kinder[0]->art == AnweisungArt::Fortsetzung);
        CHECK(wenn.sonst == nullptr);

        CHECK(rumpf.kinder[2]->text == "melder(1)");
        CHECK(rumpf.kinder[3]->art == AnweisungArt::Ruecksprung);
        CHECK(rumpf.kinder[3]->text == "summe");
    }

    TEST_CASE("Anweisungen: switch mit Faellen, do-while, Marken, goto, else")
    {
        const Ergebnis e = lies(kAnweisungen);
        CHECK(e.meldungen.empty());
        REQUIRE(e.einheit.funktionen.size() == 1);
        const Anweisung& rumpf = *e.einheit.funktionen[0].rumpf;
        REQUIRE(rumpf.kinder.size() == 7);

        const Anweisung& auswahl = *rumpf.kinder[0];
        CHECK(auswahl.art == AnweisungArt::Auswahl);
        CHECK(auswahl.text == "z");
        const auto& faelle = auswahl.kinder[0]->kinder;
        REQUIRE(faelle.size() == 8);
        CHECK(faelle[0]->art == AnweisungArt::Fall);
        CHECK(faelle[0]->text == "0");
        CHECK(faelle[1]->text == "1");
        CHECK(faelle[3]->art == AnweisungArt::Abbruch);
        CHECK(faelle[6]->art == AnweisungArt::Fall);
        CHECK(faelle[6]->text.empty());  // default

        CHECK(rumpf.kinder[1]->art == AnweisungArt::TueSolange);
        CHECK(rumpf.kinder[1]->text == "z > 0");
        CHECK(rumpf.kinder[2]->art == AnweisungArt::Marke);
        CHECK(rumpf.kinder[2]->text == "nochmal");

        const Anweisung& wenn = *rumpf.kinder[3];
        CHECK(wenn.kinder[0]->art == AnweisungArt::Sprung);
        CHECK(wenn.kinder[0]->text == "ende");
        REQUIRE(wenn.sonst != nullptr);
        CHECK(wenn.sonst->text == "z -= 1");

        CHECK(rumpf.kinder[4]->art == AnweisungArt::Sprung);
        CHECK(rumpf.kinder[5]->art == AnweisungArt::Marke);
        CHECK(rumpf.kinder[6]->art == AnweisungArt::Ruecksprung);
    }

    TEST_CASE("Datentypen: enum, struct, union, Bitfeld, Feld, Funktionszeiger")
    {
        const Ergebnis ergebnis = lies(beispiele::kDatentypen);
        CHECK(ergebnis.meldungen.empty());
        const Einheit& e = ergebnis.einheit;

        REQUIRE(e.typen.size() == 5);
        CHECK(typ(e, "Farbe").art == TypArt::Aufzaehlung);
        CHECK(glieder(typ(e, "Farbe")) == "|ROT|;|GRUEN|= 4;|BLAU|;");
        CHECK(glieder(typ(e, "Punkt")) == "int|x|;int|y|;");

        const Typ& knoten = typ(e, "Knoten");
        CHECK(knoten.art == TypArt::Struktur);
        CHECK(knoten.zeile == 10);
        CHECK(glieder(knoten) == "struct Punkt|ort|;"
                                 "enum Farbe|farbe|;"
                                 "struct Knoten *|naechster|;"
                                 "u8|daten|[16];"
                                 "unsigned|bereit|: 1;"
                                 "anonymous_wert|wert|;"
                                 "void (*|rueckruf|)(struct Knoten *k, int grund);");

        // die namenlose union heisst nach ihrem Glied
        CHECK(typ(e, "anonymous_wert").art == TypArt::Vereinigung);
        CHECK(glieder(typ(e, "anonymous_wert")) == "int|ganz|;float|bruch|;");

        // typedef struct { ... } Liste; — der Typ heisst wie der typedef
        CHECK(glieder(typ(e, "Liste")) == "Knoten_t *|kopf|;Punkt_t|mitte|;");
    }

    TEST_CASE("typedef: weitere Namen, und Variablen eines Strukturtyps")
    {
        const Einheit e = lies(beispiele::kDatentypen).einheit;

        REQUIRE(e.aliase.size() == 3);
        CHECK(e.aliase[0].name == "u8");
        CHECK(e.aliase[0].ziel == "unsigned char");
        CHECK(e.aliase[1].name == "Knoten_t");
        CHECK(e.aliase[1].ziel == "Knoten");
        CHECK(e.aliase[2].name == "Punkt_t");
        CHECK(e.aliase[2].ziel == "struct Punkt");

        REQUIRE(e.variablen.size() == 1);
        CHECK(e.variablen[0].typ == "struct Punkt");
        CHECK(e.variablen[0].name == "ursprung");
    }

    TEST_CASE("Firmware-Formen: ISR(...) als Funktion, Makro-Aufrufe, Attribute")
    {
        const Ergebnis e = lies(R"c(
MODULE_LICENSE("GPL");
ERZEUGE_TABELLE(alpha, 3)
static volatile uint16_t ticks;
__attribute__((interrupt)) void uart_isr(void) { ticks = 0; }
void spaeter(void) __attribute__((weak));
ISR(TIMER1_COMPA_vect)
{
    ticks++;
}
)c");
        CHECK(e.meldungen.empty());
        REQUIRE(e.einheit.variablen.size() == 1);
        CHECK(e.einheit.variablen[0].name == "ticks");

        REQUIRE(e.einheit.funktionen.size() == 3);
        CHECK(e.einheit.funktionen[0].name == "uart_isr");
        CHECK(e.einheit.funktionen[0].rueckgabe == "void");
        CHECK(e.einheit.funktionen[1].name == "spaeter");
        CHECK(e.einheit.funktionen[1].rumpf == nullptr);
        CHECK(e.einheit.funktionen[2].signatur() == "ISR(TIMER1_COMPA_vect)");
        CHECK(e.einheit.funktionen[2].rumpf->kinder.size() == 1);
    }

    TEST_CASE("Firmware-Formen: gepackte Strukturen mit Makro-Woertern")
    {
        // so in SOES (esc_eoe.c): ein Makro vor dem typedef, eines zwischen struct und Etikett
        const Ergebnis e = lies(R"c(
PACKED_BEGIN
typedef struct PACKED adresse
{
  uint8_t addr[6];
} adresse_t;
PACKED_END

struct PACKED kopf { uint16_t laenge; } k;
int danach;
)c");
        CHECK(e.meldungen.empty());
        REQUIRE(e.einheit.typen.size() == 2);
        CHECK(e.einheit.typen[0].name == "adresse");
        CHECK(glieder(e.einheit.typen[0]) == "uint8_t|addr|[6];");
        CHECK(e.einheit.typen[1].name == "kopf");

        REQUIRE(e.einheit.aliase.size() == 1);
        CHECK(e.einheit.aliase[0].name == "adresse_t");
        CHECK(e.einheit.aliase[0].ziel == "adresse");

        REQUIRE(e.einheit.variablen.size() == 2);
        CHECK(e.einheit.variablen[0].name == "k");
        CHECK(e.einheit.variablen[0].typ == "kopf");
        CHECK(e.einheit.variablen[1].name == "danach");
    }

    TEST_CASE("Kaputter Quelltext: Meldungen statt Absturz, das Lesbare bleibt")
    {
        const Ergebnis offen = lies("int f(void) { if (x { y; }\nint g(void) { return 1; }");
        CHECK_FALSE(offen.meldungen.empty());
        CHECK(offen.einheit.funktionen.size() == 1);

        const Ergebnis strich = lies("void f(void) { a = 1 }\nvoid g(void) { else b; ) }\n} int h;");
        CHECK(strich.meldungen.size() >= 3);
        REQUIRE(strich.einheit.funktionen.size() == 2);
        CHECK(strich.einheit.funktionen[0].rumpf->kinder[0]->text == "a = 1");
        CHECK(strich.einheit.variablen.size() == 1);

        CHECK(lies("").einheit.funktionen.empty());
        CHECK(lies("struct { int a;").meldungen.size() == 1);
    }
}
