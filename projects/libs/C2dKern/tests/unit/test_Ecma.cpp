/**
 * @file test_Ecma.cpp
 * @brief JavaScript und TypeScript: Lexer (Vorlagen, Regex, Zeilen in Bloecken), Leser, Diagramme
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

using namespace c2d;
using lexer::Token;
using lexer::TokenArt;

namespace
{

/// Die Token als Text, Zeilenende `;;`; ohne das Ende.
std::string bild(std::string_view text, std::vector<quelle::Meldung>& meldungen)
{
    std::string s;
    for (const Token& t : lexer::zerlege(text, sprache::javascript().tabelle(), meldungen))
    {
        if (t.art == TokenArt::Ende) break;
        if (!s.empty()) s += ' ';
        s += t.art == TokenArt::Zeilenende ? std::string(";;") : std::string(t.text);
    }
    return s;
}

sprache::Ergebnis js(std::string_view text)
{
    return sprache::javascript().lies(text, "t.js");
}

sprache::Ergebnis ts(std::string_view text)
{
    return sprache::typescript().lies(text, "t.ts");
}

const modell::Funktion& funktion(const sprache::Ergebnis& e, std::string_view voll)
{
    for (const modell::Funktion& f : e.einheit.funktionen)
        if (f.vollerName() == voll) return f;
    FAIL("no function " << voll);
    return e.einheit.funktionen.front();
}

std::string namen(const sprache::Ergebnis& e)
{
    std::string s;
    for (const modell::Funktion& f : e.einheit.funktionen) s += (s.empty() ? "" : ", ") + f.vollerName();
    return s;
}

/// Die Anweisungen einer Folge als "Art:Text"; Bloecke in Klammern.
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

}  // namespace

TEST_SUITE("JavaScript und TypeScript")
{
    TEST_CASE("Lexer: Vorlagen mit ${…}, Regex gegen Division, Zeilenende in Bloecken, nicht in ( und [")
    {
        std::vector<quelle::Meldung> m;
        CHECK(bild("a = `x${b + `y${c}`}z`\nd = e / f / g\nh = /[/]+/gi.test(s)\n", m) ==
              "a = `x${b + `y${c}`}z` ;; d = e / f / g ;; h = /[/]+/gi . test ( s ) ;;");
        CHECK(bild("f(a,\n  b)\nif (x) {\n  y()\n}\nreturn /x/\n", m) ==
              "f ( a , b ) ;; if ( x ) { ;; y ( ) ;; } ;; return /x/ ;;");
        CHECK(bild("g(() => {\n  h()\n})\nconst $el = $('#a')\n", m) ==
              "g ( ( ) => { ;; h ( ) ;; } ) ;; const $el = $ ( '#a' ) ;;");
        CHECK(m.empty());
    }

    TEST_CASE("Anmeldung: Endungen, kein Praeprozessor")
    {
        CHECK(sprache::spracheFuer("app.js") == &sprache::javascript());
        CHECK(sprache::spracheFuer("app.mjs") == &sprache::javascript());
        CHECK(sprache::spracheFuer("app.ts") == &sprache::typescript());
        CHECK(sprache::spracheFuer("app.d.ts") == &sprache::typescript());
        CHECK(sprache::spracheFuer("app.tsx") == nullptr);  // JSX noch nicht
        CHECK_FALSE(sprache::javascript().hatPraeprozessor());
        CHECK_FALSE(sprache::typescript().hatPraeprozessor());
    }

    TEST_CASE("Ohne Semikolon: die Anweisung endet, wo JavaScript eins einsetzt")
    {
        const sprache::Ergebnis e = js(R"js(function f(a) {
  let x = a
    + 1
  const y = a
  ;[1, 2].forEach(g)
  return x
    ? y
    : 0
}
)js");
        CHECK(e.meldungen.empty());
        CHECK(baum(*funktion(e, "f").rumpf) ==
              "B[E:let x = a + 1; E:const y = a; E:[1, 2].forEach(g); RET:x ? y : 0]");
    }

    TEST_CASE("Funktionen ohne Namen: benannt nach ihrer Zuweisung, sonst <anonymous:Zeile> mit aussen")
    {
        const sprache::Ergebnis e = js(R"js(const zaehler = {
  erhoehe(um) { return um },
  get wert() { return 1 },
  setze: function (x) { this.x = x },
}
module.exports.start = async () => {
  await warte()
}
function verbinde(knopf) {
  knopf.on("click", (ereignis) => {
    tu(ereignis)
  })
  return x => x * 2
}
[1, 2].map(function (n) {
  return n
})
)js");
        CHECK(e.meldungen.empty());
        CHECK(namen(e) == "erhoehe, get wert, setze, start, verbinde, verbinde::<anonymous:10>, <anonymous:15>, <module>");
        const modell::Funktion& innen = funktion(e, "verbinde::<anonymous:10>");
        CHECK(innen.aussen == "verbinde");
        REQUIRE(innen.parameter.size() == 1);
        CHECK(innen.parameter[0].name == "ereignis");
        // in der Anweisung bleibt der Rumpf als {…}; ein Pfeil mit Ausdruck bleibt Text
        CHECK(baum(*funktion(e, "verbinde").rumpf) ==
              "B[E:knopf.on(\"click\", (ereignis) => {\xE2\x80\xA6}); RET:x => x * 2]");
        CHECK(baum(*funktion(e, "<module>").rumpf) ==
              "B[E:const zaehler = { erhoehe(um) {\xE2\x80\xA6}, get wert() {\xE2\x80\xA6}, setze: function (x) "
              "{\xE2\x80\xA6}, }; E:module.exports.start = async () => {\xE2\x80\xA6}; E:[1, 2].map(function (n) "
              "{\xE2\x80\xA6})]");
        // die Definition ruft nichts; der Aufruf im Callback gehoert dem Callback
        CHECK(modell::aufrufeVon(funktion(e, "verbinde")) == std::vector<std::string>{"on"});
        // die Anweisung kennt die Funktion, die in ihr steht — so fuehrt ein Doppelklick hin
        const modell::Anweisung& on = *funktion(e, "verbinde").rumpf->kinder[0];
        CHECK(on.innere == std::vector<std::string>{"verbinde::<anonymous:10>"});
        const diagramm::Flussdiagramm d = diagramm::flussVon(funktion(e, "verbinde"));
        CHECK(std::any_of(d.knoten().begin(), d.knoten().end(), [](const diagramm::Knoten& k) {
            return k.innere == std::vector<std::string>{"verbinde::<anonymous:10>"};
        }));
    }

    TEST_CASE("Anweisungen: for of/in, for(;;), do, switch mit Durchfallen, try/catch ohne Bindung/finally, Marke")
    {
        const sprache::Ergebnis e = js(R"js(function f(xs, o) {
  for (const x of xs) if (x) continue
  for (const k in o) {}
  for (let i = 0; i < 3; i++) {}
  do { i-- } while (i > 0)
  switch (o.art) {
    case 1:
    case 2:
      a()
    default:
      b()
      break
  }
  try { c() } catch { d() } finally { e() }
  aussen: while (true) { break aussen }
}
)js");
        const auto& k = funktion(e, "f").rumpf->kinder;
        REQUIRE(k.size() == 8);
        CHECK(baum(*k[0]) == "EACH:const x of xs[IF:x[CNT]]");
        CHECK(baum(*k[1]) == "EACH:const k in o[B]");
        CHECK(k[2]->init == "let i = 0");
        CHECK(k[2]->schritt == "i++");
        CHECK(baum(*k[3]) == "DO:i > 0[B[E:i--]]");
        CHECK(baum(*k[4]) == "SW:o.art[B[CASE:1; CASE:2; E:a(); CASE; E:b(); BRK]]");
        CHECK_FALSE(k[4]->ohneDurchfall);
        CHECK(baum(*k[5]) == "TRY[B[E:c()]; EXC[B[E:d()]]; FIN[B[E:e()]]]");
        CHECK(k[5]->kinder[1]->kopf == "catch");
        CHECK(baum(*k[6]) == "LBL:aussen");
        REQUIRE(e.meldungen.size() == 1);
        CHECK(e.meldungen[0].text == "break aussen: shown as a break of the innermost loop or switch");
    }

    TEST_CASE("TypeScript: Klassen mit Sichtbarkeit, interface, type, enum, implements, Felder aus dem Konstruktor")
    {
        const sprache::Ergebnis e = ts(R"ts(import { x } from "./x";
export interface Ablage<T> extends Basis {
  readonly name: string;
  menge?: number;
  lege(name: string, menge: number): boolean;
  [k: string]: unknown;
}
type Bericht = { gesamt: number };
type Filter = (p: Posten) => boolean;
export const enum Farbe { Rot, Gruen = 2 }
export abstract class Lager<T> extends Raum<T> implements Ablage<T>, Zaehlbar {
  #posten = new Map<string, number>();
  static grenze: number = 100
  protected abstract pruefe(): void;
  constructor(private readonly ort: string, public tiefe = 3) { super() }
  get anzahl(): number { return this.#posten.size }
  async lege<K>(name: K, menge: number): Promise<boolean> { return true }
}
namespace Werkzeug.Innen { export function hilf(): void {} }
declare function extern(a: number): string;
)ts");
        CHECK(e.meldungen.empty());
        REQUIRE(e.einheit.typen.size() == 4);
        const modell::Typ& ablage = e.einheit.typen[0];
        CHECK(ablage.art == modell::TypArt::Schnittstelle);
        CHECK(ablage.basen == std::vector<std::string>{"Basis"});
        REQUIRE(ablage.felder.size() == 2);
        CHECK(modell::deklaration(ablage.felder[1].typ, ablage.felder[1].name, {}, true) == "menge?: number");
        REQUIRE(ablage.methoden.size() == 1);
        CHECK(ablage.methoden[0].signatur() == "lege(name: string, menge: number)");
        CHECK(e.einheit.typen[1].name == "Bericht");
        CHECK(e.einheit.typen[1].art == modell::TypArt::Struktur);
        CHECK(e.einheit.typen[2].art == modell::TypArt::Aufzaehlung);
        CHECK(e.einheit.typen[2].felder[1].zusatz == "= 2");
        const modell::Typ& lager = e.einheit.typen[3];
        CHECK(lager.basen == std::vector<std::string>{"Raum<T>"});
        CHECK(lager.realisiert == std::vector<std::string>{"Ablage<T>", "Zaehlbar"});
        REQUIRE(lager.felder.size() == 4);
        CHECK(lager.felder[0].name == "#posten");
        CHECK(lager.felder[0].sicht == modell::Sichtbarkeit::Privat);
        CHECK(lager.felder[1].typ == "number");
        CHECK(lager.felder[2].name == "ort");  // aus constructor(private readonly ort: string)
        CHECK(lager.felder[2].sicht == modell::Sichtbarkeit::Privat);
        CHECK(lager.felder[3].name == "tiefe");
        REQUIRE(lager.methoden.size() == 4);
        CHECK(lager.methoden[0].abstrakt);
        CHECK(lager.methoden[0].sicht == modell::Sichtbarkeit::Geschuetzt);
        CHECK(lager.methoden[2].name == "get anzahl");
        REQUIRE(e.einheit.aliase.size() == 1);
        CHECK(e.einheit.aliase[0].ziel == "(p: Posten) => boolean");
        CHECK(namen(e) == "Lager::constructor, Lager::get anzahl, Lager::lege, Werkzeug::Innen::hilf, extern");
        CHECK(funktion(e, "Lager::lege").signatur() == "Lager::lege(name: K, menge: number)");
        CHECK_FALSE(funktion(e, "extern").rumpf);
        REQUIRE(e.einheit.einbindungen.size() == 1);
        CHECK(e.einheit.einbindungen[0].ziel == "./x");

        // im Datentypen-Diagramm: «interface», implements gestrichelt
        const diagramm::Strukturdiagramm d = diagramm::strukturVon(e.einheit);
        CHECK(d.kaesten[0].art == diagramm::KastenArt::Schnittstelle);
        CHECK(std::any_of(d.beziehungen.begin(), d.beziehungen.end(), [](const diagramm::Beziehung& b) {
            return b.von == 3 && b.nach == 0 && b.art == diagramm::BeziehungArt::Realisiert;
        }));
        CHECK(ausgabe::mermaid().schreibe(d, {}).find("Ablage <|.. Lager") != std::string::npos);
        CHECK(ausgabe::svg().schreibe(d, {}).find("interface") != std::string::npos);
    }

    TEST_CASE("Ablauf: Funktion mit Callback, Pfeil mit Rueckgabetyp")
    {
        const sprache::Ergebnis e = ts(R"ts(export const bericht = (lager: Lager): Bericht => {
  if (!lager) throw new Error("leer")
  return { n: lager.zahl }
}
const pruefe: Pruefer = (text) => {
  return text.length > 0
}
)ts");
        CHECK(namen(e) == "bericht, pruefe");  // nach der Variablen benannt, nicht nach ihrem Typ
        const modell::Funktion& f = funktion(e, "bericht");
        CHECK(f.signatur() == "bericht(lager: Lager)");
        const diagramm::Flussdiagramm d = diagramm::flussVon(f);
        CHECK(ausgabe::mermaid().schreibe(d, {}).find("throw new Error") != std::string::npos);
        CHECK(e.einheit.variablen.size() == 2);
    }
}
