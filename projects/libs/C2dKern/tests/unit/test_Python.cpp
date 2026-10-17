/**
 * @file test_Python.cpp
 * @brief Python: Lexer (Zeilenende, Einrueckung, Zeichenketten), Leser, und was die Diagramme daraus machen
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Entscheidungstabelle.hpp"
#include "c2d/diagramm/Wahrheitstabelle.hpp"
#include "c2d/lexer/Lexer.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <string>

using namespace c2d;
using lexer::Token;
using lexer::TokenArt;

namespace
{

/// Die Token als Text, Zeilenende `;`, Einzug `>`, Auszug `<`; ohne das Ende.
std::string bild(std::string_view text, std::vector<quelle::Meldung>& meldungen)
{
    std::string s;
    for (const Token& t : lexer::zerlege(text, sprache::python().tabelle(), meldungen))
    {
        if (t.art == TokenArt::Ende) break;
        if (!s.empty()) s += ' ';
        switch (t.art)
        {
            case TokenArt::Zeilenende: s += ';'; break;
            case TokenArt::Einzug: s += '>'; break;
            case TokenArt::Auszug: s += '<'; break;
            default: s += t.text; break;
        }
    }
    return s;
}

sprache::Ergebnis lies(std::string_view text)
{
    return sprache::python().lies(text, "t.py");
}

const modell::Funktion& funktion(const sprache::Ergebnis& e, std::string_view voll)
{
    for (const modell::Funktion& f : e.einheit.funktionen)
        if (f.vollerName() == voll) return f;
    FAIL("no function " << voll);
    return e.einheit.funktionen.front();
}

/// Die Anweisungen einer Folge als "Art:Text", durch Strichpunkt; Bloecke in Klammern.
std::string baum(const modell::Anweisung& a)
{
    static constexpr const char* kKuerzel[] = {"E",   "B",  "IF", "WH",  "DO", "FOR", "SW", "CASE", "RET", "BRK",
                                               "CNT", "GO", "LBL", "DIR", "EACH", "TRY", "EXC", "THR", "FIN", "WITH",
                                               "VAR", "ZW",  "BED"};
    std::string s = kKuerzel[static_cast<int>(a.art)];
    if (!a.text.empty()) s += ':' + a.text;
    if (!a.kinder.empty())
    {
        s += '[';
        for (std::size_t i = 0; i < a.kinder.size(); ++i) s += (i ? "; " : "") + baum(*a.kinder[i]);
        s += ']';
    }
    if (a.sonst) s += " else " + baum(*a.sonst);
    return s;
}

bool hatKante(const diagramm::Flussdiagramm& d, std::string_view von, std::string_view nach, std::string_view text = {})
{
    for (const diagramm::Kante& k : d.kanten())
        if (d.knoten()[k.von].zeilen.front() == von && d.knoten()[k.nach].zeilen.front() == nach && k.beschriftung == text)
            return true;
    return false;
}

}  // namespace

TEST_SUITE("Python")
{
    TEST_CASE("Lexer: Zeilenende, Einzug und Auszug; in Klammern und hinter \\ endet keine Zeile")
    {
        std::vector<quelle::Meldung> m;
        CHECK(bild("def f(a,\n      b):\n    if a:\n        return b\n\n    # nur Kommentar\n    return a\nx = 1",
                   m) == "def f ( a , b ) : ; > if a : ; > return b ; < return a ; < x = 1 ;");
        CHECK(bild("a = 1 + \\\n    2\n", m) == "a = 1 + 2 ;");
        CHECK(bild("if x:\n\tpass\nelse:\n        pass\n", m) == "if x : ; > pass ; < else : ; > pass ; <");  // Tab = 8
        CHECK(m.empty());

        bild("if x:\n        a\n    b\n", m);  // zurueck auf keine fruehere Stufe
        REQUIRE(m.size() == 1);
        CHECK(m[0].text == "indentation does not match any outer level");
        CHECK(m[0].ort.zeile == 3);
    }

    TEST_CASE("Lexer: Vorsaetze, dreifache Anfuehrungszeichen, Zahlen mit _")
    {
        std::vector<quelle::Meldung> m;
        const std::vector<Token> t =
            lexer::zerlege("s = rb'\\d' + f\"{x}\" + '''a\n\"b\"\n'''\nn = 1_000\n", sprache::python().tabelle(), m);
        CHECK(m.empty());
        REQUIRE(t.size() >= 9);
        CHECK(t[2].art == TokenArt::Zeichenkette);
        CHECK(t[2].text == "rb'\\d'");
        CHECK(t[4].text == "f\"{x}\"");
        CHECK(t[6].text == "'''a\n\"b\"\n'''");
        CHECK(t[7].art == TokenArt::Zeilenende);
        CHECK(t[8].ort.zeile == 4);  // die Zeilen in der Zeichenkette zaehlen
        CHECK(t[10].text == "1_000");
        CHECK(t[10].art == TokenArt::Zahl);
    }

    TEST_CASE("Anmeldung: Endungen, kein Praeprozessor")
    {
        CHECK(sprache::spracheFuer("werkzeug.py") == &sprache::python());
        CHECK(sprache::spracheFuer("stub.PYI") == &sprache::python());
        CHECK_FALSE(sprache::python().hatPraeprozessor());
        CHECK(sprache::c().hatPraeprozessor());
        CHECK(sprache::cpp().hatPraeprozessor());
    }

    TEST_CASE("Funktionen, Parameter wie in der Quelle, Klassen mit Feldern und Methoden")
    {
        const sprache::Ergebnis e = lies(R"py("""Modul."""
import os, sys as system
from .werkzeug import hilfe

GRENZE: int = 10

@dataclass
class Punkt(Basis, metaclass=Meta):
    """Ein Punkt."""
    x: float = 0.0
    y: float

    def __init__(self, x, y=0):
        self.x = x
        self.farbe: str = "rot"

    @staticmethod
    def null(n: int = 3, *args, **kw) -> "Punkt":
        return Punkt(0, 0)

class Farbe(enum.Enum):
    ROT = 1
    GRUEN = 2

def frei(a: list[int], b):
    pass
)py");
        CHECK(e.meldungen.empty());
        REQUIRE(e.einheit.typen.size() == 2);
        const modell::Typ& p = e.einheit.typen[0];
        CHECK(p.name == "Punkt");
        CHECK(p.art == modell::TypArt::Klasse);
        CHECK(p.basen == std::vector<std::string>{"Basis"});
        REQUIRE(p.felder.size() == 3);
        CHECK(modell::deklaration(p.felder[0].typ, p.felder[0].name, p.felder[0].zusatz, p.felder[0].typHinten) ==
              "x: float = 0.0");
        CHECK(p.felder[1].name == "y");
        CHECK(p.felder[2].name == "farbe");  // aus self.farbe in __init__
        CHECK(p.felder[2].typ == "str");
        REQUIRE(p.methoden.size() == 2);
        CHECK(p.methoden[0].signatur() == "__init__(self, x, y)");
        CHECK(p.methoden[1].signatur() == "null(n: int, *args, **kw)");
        CHECK(p.methoden[1].statisch);

        const modell::Typ& farbe = e.einheit.typen[1];
        CHECK(farbe.art == modell::TypArt::Aufzaehlung);
        REQUIRE(farbe.felder.size() == 2);
        CHECK(modell::deklaration(farbe.felder[1].typ, farbe.felder[1].name, farbe.felder[1].zusatz,
                                  farbe.felder[1].typHinten) == "GRUEN = 2");

        REQUIRE(e.einheit.funktionen.size() == 3);  // kein <module>: oben stehen nur Zuweisungen
        CHECK(e.einheit.funktionen[0].signatur() == "Punkt::__init__(self, x, y)");
        CHECK(e.einheit.funktionen[2].signatur() == "frei(a: list[int], b)");
        CHECK(e.einheit.funktionen[2].zeile == 25);
        CHECK(e.einheit.funktionen[2].bisZeile == 26);

        REQUIRE(e.einheit.einbindungen.size() == 3);
        CHECK(e.einheit.einbindungen[1].ziel == "sys");
        CHECK(e.einheit.einbindungen[2].ziel == ".werkzeug");
        REQUIRE(e.einheit.variablen.size() == 1);
        CHECK(e.einheit.variablen[0].name == "GRENZE");
        CHECK(e.einheit.variablen[0].typ == "int");
    }

    TEST_CASE("Code auf oberster Ebene ist die Funktion <module>; eine Funktion in einer Funktion")
    {
        const sprache::Ergebnis e = lies(R"py(def aussen(x):
    def innen(y):
        return y * 2
    return innen(x)

if __name__ == "__main__":
    aussen(3)
)py");
        CHECK(e.meldungen.empty());
        REQUIRE(e.einheit.funktionen.size() == 3);
        CHECK(e.einheit.funktionen[0].vollerName() == "aussen");
        const modell::Funktion& innen = e.einheit.funktionen[1];
        CHECK(innen.vollerName() == "aussen::innen");
        CHECK(innen.aussen == "aussen");
        CHECK(baum(*e.einheit.funktionen[0].rumpf) == "B[E:def innen(y); RET:innen(x)]");
        CHECK(e.einheit.funktionen[0].rumpf->kinder[0]->innere == std::vector<std::string>{"aussen::innen"});
        // die Definition ruft nichts, der return ruft innen
        CHECK(modell::aufrufeVon(e.einheit.funktionen[0]) == std::vector<std::string>{"innen"});
        const modell::Funktion& modul = e.einheit.funktionen[2];
        CHECK(modul.name == "<module>");
        CHECK(modul.zeile == 6);
        CHECK(baum(*modul.rumpf) == "B[IF:__name__ == \"__main__\"[B[E:aussen(3)]]]");
    }

    TEST_CASE("Anweisungen: if/elif/else, Schleifen mit else, try, with, match, eine Zeile mit ;")
    {
        const sprache::Ergebnis e = lies(R"py(def f(xs):
    if not xs: return None
    elif len(xs) == 1:
        a = 1; b = 2
    else:
        pass
    for x in xs:
        if x: break
    else:
        print("kein break")
    while True:
        continue
    try:
        with open(p) as d, lock:
            raise ValueError(p)
    except (KeyError, ValueError) as fehler:
        log(fehler)
    except:
        raise
    else:
        ok()
    finally:
        aufraeumen()
    match befehl:
        case "start" | "go":
            starte()
        case Punkt(x=0) if bereit:
            pass
        case _:
            return 0
)py");
        CHECK(e.meldungen.empty());
        const modell::Funktion& f = funktion(e, "f");
        const auto& k = f.rumpf->kinder;
        REQUIRE(k.size() == 5);
        CHECK(baum(*k[0]) == "IF:not xs[B[RET:None]] else IF:len(xs) == 1[B[E:a = 1; E:b = 2]] else B");
        CHECK(baum(*k[1]) == "EACH:x in xs[B[IF:x[B[BRK]]]] else B[E:print(\"kein break\")]");
        CHECK(k[1]->kopf == "for x in xs");
        CHECK(baum(*k[2]) == "WH:True[B[CNT]]");
        CHECK(baum(*k[3]) == "TRY[B[WITH:open(p) as d, lock[B[THR:ValueError(p)]]]; EXC:(KeyError, ValueError) as fehler"
                             "[B[E:log(fehler)]]; EXC[B[THR]]; FIN[B[E:aufraeumen()]]] else B[E:ok()]");
        CHECK(k[3]->kinder[1]->kopf == "except (KeyError, ValueError) as fehler");
        CHECK(k[3]->kinder[2]->kopf == "except");
        CHECK(k[3]->kinder[0]->kinder[0]->kinder[0]->kinder[0]->kopf == "raise ValueError(p)");
        CHECK(baum(*k[4]) == "SW:befehl[B[CASE:\"start\" | \"go\"; E:starte(); CASE:Punkt(x=0) if bereit; CASE; RET:0]]");
        CHECK(k[4]->ohneDurchfall);
        CHECK(k[4]->kopf == "match befehl");
    }

    TEST_CASE("Ein Name match oder case ist keine Anweisung; Fehler werden gemeldet")
    {
        const sprache::Ergebnis e = lies("def f():\n    match = re.match(a, b)\n    case(1)\n    if x\n        y()\n");
        const modell::Funktion& f = funktion(e, "f");
        REQUIRE(f.rumpf->kinder.size() == 3);
        CHECK(baum(*f.rumpf->kinder[0]) == "E:match = re.match(a, b)");
        CHECK(baum(*f.rumpf->kinder[1]) == "E:case(1)");
        REQUIRE(e.meldungen.size() == 1);
        CHECK(e.meldungen[0].text == "':' expected");
        CHECK(e.meldungen[0].ort.zeile == 4);
    }

    TEST_CASE("Ablauf: finally auf jedem Weg, else einer Schleife, match ohne Durchfallen")
    {
        const sprache::Ergebnis e = lies(R"py(def f(xs):
    try:
        if xs:
            return 1
        tu()
    finally:
        zu()
    for x in xs:
        if x:
            break
    else:
        leer()
    match x:
        case 1:
            eins()
        case _:
            sonst()
    return 0
)py");
        const diagramm::Flussdiagramm d = diagramm::flussVon(funktion(e, "f"));
        CHECK(hatKante(d, "tu()", "zu()"));
        CHECK(hatKante(d, "return 1", "zu()"));  // der return geht durch finally ...
        CHECK(hatKante(d, "zu()", "End", "return"));  // ... und danach hinaus
        CHECK(hatKante(d, "zu()", "x in xs"));
        CHECK(hatKante(d, "x in xs", "leer()", "done"));  // ohne break: else
        CHECK(hatKante(d, "leer()", "x"));                // dann match
        CHECK(hatKante(d, "eins()", "return 0"));         // kein Durchfallen in sonst()
        CHECK_FALSE(hatKante(d, "eins()", "sonst()"));
        CHECK(hatKante(d, "x", "sonst()", "default"));
    }

    TEST_CASE("Struktogramm: Koepfe wie in der Quelle, with als Abschnitt, try mit else und finally")
    {
        const sprache::Ergebnis e = lies(R"py(def f(p):
    while p > 0:
        p -= 1
    else:
        fertig()
    try:
        with open(p) as d:
            raise Fehler
    except OSError:
        pass
    else:
        ok()
    finally:
        zu()
)py");
        const diagramm::Struktogramm s = diagramm::struktogrammVon(funktion(e, "f"));
        REQUIRE(s.bloecke.size() == 3);
        CHECK(s.bloecke[0].art == diagramm::BlockArt::Kopfschleife);
        CHECK(s.bloecke[0].text == "while p > 0");
        CHECK(s.bloecke[1].art == diagramm::BlockArt::Abschnitt);
        CHECK(s.bloecke[1].text == "else (no break)");
        const diagramm::Block& t = s.bloecke[2];
        REQUIRE(t.spalten.size() == 4);
        CHECK(t.spalten[1].titel == "except OSError");
        CHECK(t.spalten[2].titel == "else");
        CHECK(t.spalten[3].titel == "finally");
        REQUIRE(t.spalten[0].bloecke.size() == 1);
        const diagramm::Block& mit = t.spalten[0].bloecke[0];
        CHECK(mit.art == diagramm::BlockArt::Abschnitt);
        CHECK(mit.text == "with open(p) as d");
        REQUIRE(mit.spalten.size() == 1);
        CHECK(mit.spalten[0].bloecke[0].text == "raise Fehler");
        CHECK(c2d::ausgabe::svg().schreibe(s, {}).find("with open(p) as d") != std::string::npos);
    }

    TEST_CASE("Logik: and, or, not; not umfasst den Vergleich; die Bedingung von a if c else b")
    {
        const sprache::Ergebnis e = lies(R"py(def f(a, b, c):
    if a and not b == c or g(a):
        pass
    x = a if b and c else 0
    return x
)py");
        const modell::Funktion& f = funktion(e, "f");
        const modell::Anweisung& wenn = *f.rumpf->kinder[0];
        REQUIRE(wenn.logik);
        const diagramm::Wahrheitstabelle t = diagramm::wahrheitstabelleVon(*wenn.logik, wenn.logikText, f.name, wenn.zeile);
        REQUIRE(t.atome.size() == 3);
        CHECK(t.atome[1].text == "b == c");  // not (b == c), nicht (not b) == c
        CHECK(t.atome[2].mitAufruf);
        const modell::Anweisung& zuweisung = *f.rumpf->kinder[1];
        CHECK(zuweisung.frage == "b and c");
        CHECK(zuweisung.logikText == "b and c");
        CHECK(zuweisung.schreibt == std::vector<std::string>{"x"});
    }

    TEST_CASE("Entscheidungstabelle: match als Zeile mit seinem Kopf, ein Fall je Regel")
    {
        const sprache::Ergebnis e = lies(R"py(def f(x):
    match x:
        case 1:
            a()
        case 2:
            b()
    return 0
)py");
        const diagramm::Entscheidungstabelle t = diagramm::entscheidungstabelleVon(funktion(e, "f"));
        REQUIRE(t.bedingungen.size() == 1);
        CHECK(diagramm::zeilenText(t.bedingungen[0]) == "match x");
        CHECK(t.regeln.size() == 3);  // 1, 2, otherwise
        // ohne Durchfallen: in der Regel fuer 1 steht b() nicht
        const auto aktion = [&](std::string_view text) {
            for (std::size_t i = 0; i < t.aktionen.size(); ++i)
                if (t.aktionen[i].text == text) return i;
            return t.aktionen.size();
        };
        REQUIRE(aktion("b()") < t.aktionen.size());
        CHECK(t.regeln[0].eintraege[0] == "1");
        CHECK_FALSE(t.regeln[0].aktionen[aktion("b()")]);
    }
}
