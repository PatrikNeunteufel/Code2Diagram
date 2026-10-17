/**
 * @file test_CSharp.cpp
 * @brief C#: Zeichenketten, #if mit Symbolen, Typen und Glieder, Anweisungen, switch-Ausdruck
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

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

sprache::Ergebnis cs(std::string_view text, const sprache::LeseOptionen& optionen = {})
{
    return sprache::csharp().lies(text, "t.cs", optionen);
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

TEST_SUITE("CSharp")
{
    TEST_CASE("Lexer: woertlich, eingebettet, roh, @name, BOM")
    {
        std::vector<quelle::Meldung> m;
        const std::vector<Token> t = lexer::zerlege(
            "\xEF\xBB\xBF#if A\nx = @\"a\\b\"\"c\" + $\"n={n} {(a ? \"j\" : \"n\")} {@\"p\\\"}\" + $@\"{{x}}\" + \"\"\"r\"s\"\"\" + @class;",
            sprache::csharp().tabelle(), m);
        CHECK(m.empty());
        REQUIRE(t.size() >= 12);
        CHECK(t[0].art == TokenArt::Direktive);
        CHECK(t[0].ort.spalte == 1);
        CHECK(t[3].text == "@\"a\\b\"\"c\"");
        CHECK(t[5].text == "$\"n={n} {(a ? \"j\" : \"n\")} {@\"p\\\"}\"");
        CHECK(t[7].text == "$@\"{{x}}\"");
        CHECK(t[9].text == "\"\"\"r\"s\"\"\"");
        CHECK(t[11].art == TokenArt::Bezeichner);
        CHECK(t[11].text == "@class");
    }

    TEST_CASE("#if: ein Name ist wahr, wenn er definiert ist; Definitionen von aussen")
    {
        const char* text = "#define A\nclass K { void F() {\n#if A && !B\n  X();\n#else\n  Y();\n#endif\n} }\n";
        CHECK(baum(*funktion(cs(text), "K::F").rumpf) == "B[E:X()]");
        sprache::LeseOptionen mitB;
        mitB.definitionen.push_back({"B"});
        CHECK(baum(*funktion(cs(text, mitB), "K::F").rumpf) == "B[E:Y()]");
        const sprache::Ergebnis e = cs(text);
        REQUIRE(e.weichen.size() == 1);
        REQUIRE(e.schalter.size() == 2);
        CHECK(e.schalter[0].name == "A");
        CHECK(e.schalter[0].inDatei == 1);
        CHECK(sprache::csharp().hatPraeprozessor());
        CHECK_FALSE(sprache::csharp().bindetKoepfeEin());
        CHECK(sprache::spracheFuer("Lager.cs") == &sprache::csharp());
    }

    TEST_CASE("Typen: Namensraum, class mit Basis und Schnittstellen, record, struct, enum, delegate, Eigenschaften")
    {
        const sprache::Ergebnis e = cs(R"cs(using System;
using Alias = System.Text;
namespace A.B;
public interface IAblage { bool Lege(string n); int Anzahl { get; } }
public record Posten(string Name, int Menge);
public struct Punkt : IEquatable<Punkt> { public int X, Y; }
public enum Farbe : byte { Rot, Gruen = 2 }
public delegate bool Filter<T>(T x);
[Serializable]
public abstract class Lager : IAblage, IDisposable
{
    private readonly int _grenze = 3;
    public int Anzahl => _grenze;
    public string Name { get; private set; } = "x";
    public int Wert { get { return _grenze; } set { if (value > 0) Merke(value); } }
    public event EventHandler? Geaendert;
    public string this[int i] => Name;
    protected Lager(int g) : base() { _grenze = g; }
    ~Lager() { }
    public abstract void Pruefe();
    public bool Lege(string n) => n.Length > 0;
    public static Lager operator +(Lager a, Lager b) => a;
    public static (int, string) Paar() { return (1, "a"); }
    public void Dispose() { }
    IEnumerator<int> IEnumerable<int>.GetEnumerator() { throw new NotImplementedException(); }
    public struct Innen { }
}
public class Lager<T> { public void Dispose() { } }
)cs");
        CHECK(e.meldungen.empty());
        REQUIRE(e.einheit.einbindungen.size() == 2);
        CHECK(e.einheit.einbindungen[1].ziel == "System.Text");
        REQUIRE(e.einheit.typen.size() == 7);
        CHECK(e.einheit.typen[5].vollerName() == "A::B::Lager::Innen");
        const modell::Typ& ablage = e.einheit.typen[0];
        CHECK(ablage.vollerName() == "A::B::IAblage");
        CHECK(ablage.art == modell::TypArt::Schnittstelle);
        REQUIRE(ablage.methoden.size() == 1);
        CHECK(ablage.methoden[0].abstrakt);
        CHECK(ablage.felder[0].zusatz == "{ get; }");
        CHECK(e.einheit.typen[1].felder.size() == 2);  // aus record Posten(…)
        CHECK(e.einheit.typen[2].realisiert == std::vector<std::string>{"IEquatable<Punkt>"});
        CHECK(e.einheit.typen[2].felder.size() == 2);
        CHECK(e.einheit.typen[3].art == modell::TypArt::Aufzaehlung);
        CHECK(e.einheit.typen[3].felder[1].zusatz == "= 2");
        const modell::Typ& lager = e.einheit.typen[4];
        CHECK(lager.basen.empty());  // IAblage steht in der Datei: eine Schnittstelle
        CHECK(lager.realisiert == std::vector<std::string>{"IAblage", "IDisposable"});
        REQUIRE(lager.felder.size() == 6);
        CHECK(lager.felder[0].sicht == modell::Sichtbarkeit::Privat);
        CHECK(lager.felder[1].zusatz == "{ get; }");
        CHECK(lager.felder[2].zusatz == "{ get; set; } = \"x\"");
        CHECK(lager.felder[3].zusatz == "{ get; set; }");
        CHECK(lager.felder[4].typ == "event EventHandler?");
        CHECK(lager.felder[5].name == "this[int i]");
        REQUIRE(e.einheit.aliase.size() == 1);
        CHECK(e.einheit.aliase[0].name == "Filter");
        CHECK(e.einheit.aliase[0].ziel == "delegate bool (T x)");
        CHECK(namen(e) == "A::B::Lager::Anzahl.get, A::B::Lager::Wert.get, A::B::Lager::Wert.set, "
                          "A::B::Lager::this[int i].get, A::B::Lager::Lager, A::B::Lager::~Lager, A::B::Lager::Lege, "
                          "A::B::Lager::operator +, A::B::Lager::Paar, A::B::Lager::Dispose, "
                          "A::B::Lager::IEnumerable<int>.GetEnumerator, A::B::Lager<T>::Dispose");
        CHECK(funktion(e, "A::B::Lager::Paar").rueckgabe == "(int, string)");
        CHECK(baum(*funktion(e, "A::B::Lager::Lege").rumpf) == "B[RET:n.Length > 0]");
        CHECK(baum(*funktion(e, "A::B::Lager::Wert.set").rumpf) == "B[IF:value > 0[E:Merke(value)]]");
        const diagramm::Strukturdiagramm d = diagramm::strukturVon(e.einheit);
        CHECK(std::any_of(d.beziehungen.begin(), d.beziehungen.end(), [](const diagramm::Beziehung& b) {
            return b.von == 4 && b.nach == 0 && b.art == diagramm::BeziehungArt::Realisiert;
        }));
    }

    TEST_CASE("Anweisungen: foreach, using, lock, try/catch when/finally, yield, lokale Funktion, Lambda")
    {
        const sprache::Ergebnis e = cs(R"cs(class K {
  IEnumerable<int> F(List<int> xs) {
    foreach (var x in xs) { if (x < 0) yield break; yield return x; }
    using (var r = Oeffne()) lock (this) Lies(r);
    try { G(); }
    catch (IOException ex) when (ex.HResult > 0) { throw; }
    finally { Z(); }
    int Doppelt(int n) { return n * 2; }
    xs.ForEach(x => { Tu(x); });
    switch (xs.Count) { case > 3: H(); break; default: goto case 0; }
  }
}
)cs");
        const auto& k = funktion(e, "K::F").rumpf->kinder;
        REQUIRE(k.size() == 6);
        CHECK(baum(*k[0]) == "EACH:var x in xs[B[IF:x < 0[RET]; E:yield return x]]");
        CHECK(k[0]->kinder[0]->kinder[0]->kinder[0]->kopf == "yield break");
        CHECK(baum(*k[1]) == "WITH:var r = Oeffne()[WITH:this[E:Lies(r)]]");
        CHECK(k[1]->kopf == "using (var r = Oeffne())");
        CHECK(baum(*k[2]) == "TRY[B[E:G()]; EXC:IOException ex[B[THR]]; FIN[B[E:Z()]]]");
        CHECK(k[2]->kinder[1]->kopf == "catch (IOException ex) when (ex.HResult > 0)");
        CHECK(baum(*k[3]) == "E:int Doppelt(int n)");
        CHECK(k[3]->innere == std::vector<std::string>{"K::F::Doppelt"});
        CHECK(baum(*k[4]) == "E:xs.ForEach(x => {\xE2\x80\xA6})");
        CHECK(k[4]->innere == std::vector<std::string>{"K::F::<lambda:9>"});
        CHECK(baum(*k[5]) == "SW:xs.Count[B[CASE:> 3; E:H(); BRK; CASE; E:goto case 0]]");
        CHECK(funktion(e, "K::F::Doppelt").aussen == "K::F");
        REQUIRE(e.meldungen.size() == 1);
        CHECK(e.meldungen[0].text == "goto case / goto default: not followed in the flowchart");
    }

    TEST_CASE("switch-Ausdruck: eine Auswahl ohne Durchfallen, je Arm die Anweisung")
    {
        const sprache::Ergebnis e = cs(R"cs(class K {
  string F(int n) => n switch { 0 => "null", < 0 when n > -9 => "klein", _ => throw new Exception() };
  void G(int n) { var s = n switch { 1 => "eins", _ => "?" }; Tu(s); }
}
)cs");
        CHECK(e.meldungen.empty());
        const modell::Anweisung& f = *funktion(e, "K::F").rumpf->kinder[0];
        CHECK(baum(f) == "SW:n[B[CASE:0; RET:\"null\"; CASE:< 0 when n > -9; RET:\"klein\"; CASE; THR:new Exception()]]");
        CHECK(f.ohneDurchfall);
        CHECK(f.kopf == "n switch");
        CHECK(baum(*funktion(e, "K::G").rumpf) ==
              "B[SW:n[B[CASE:1; E:var s = \"eins\"; CASE; E:var s = \"?\"]]; E:Tu(s)]");
        const diagramm::Flussdiagramm d = diagramm::flussVon(funktion(e, "K::F"));
        CHECK(std::count_if(d.kanten().begin(), d.kanten().end(), [&](const diagramm::Kante& k) {
                  return d.knoten()[k.von].zeilen.front() == "n";
              }) == 3);
    }

    TEST_CASE("Anweisungen auf oberster Ebene: <Main>$")
    {
        const sprache::Ergebnis e = cs("using System;\nvar x = Lies();\nif (x > 0) Console.WriteLine(x);\nstatic int Lies() => 3;\n");
        CHECK(e.meldungen.empty());
        CHECK(namen(e) == "<Main>$::Lies, <Main>$");
        CHECK(baum(*funktion(e, "<Main>$").rumpf) == "B[E:var x = Lies(); IF:x > 0[E:Console.WriteLine(x)]; E:int Lies()]");
    }
}
