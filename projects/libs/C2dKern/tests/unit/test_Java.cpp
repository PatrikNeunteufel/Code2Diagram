/**
 * @file test_Java.cpp
 * @brief Java: Dialekt des C#-Lesers — Lexer, Typen und Glieder, anonyme Klassen, Anweisungen, Marken
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

sprache::Ergebnis java(std::string_view text)
{
    return sprache::java().lies(text, "T.java");
}

const modell::Funktion& funktion(const sprache::Ergebnis& e, std::string_view voll)
{
    for (const modell::Funktion& f : e.einheit.funktionen)
        if (f.vollerName() == voll) return f;
    FAIL("no function " << voll);
    return e.einheit.funktionen.front();
}

const modell::Typ& typ(const sprache::Ergebnis& e, std::string_view voll)
{
    for (const modell::Typ& t : e.einheit.typen)
        if (t.vollerName() == voll) return t;
    FAIL("no type " << voll);
    return e.einheit.typen.front();
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
    if (!a.marke.empty()) s = a.marke + ": " + s;
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

std::string meldungen(const sprache::Ergebnis& e)
{
    std::string s;
    for (const quelle::Meldung& m : e.meldungen) s += std::to_string(m.ort.zeile) + ": " + m.text + "\n";
    return s;
}

}  // namespace

TEST_SUITE("Java")
{
    TEST_CASE("Anmeldung und Lexer: .java, kein Praeprozessor, Textbloecke, $ in Namen, varargs")
    {
        CHECK(sprache::spracheFuer("Lager.java") == &sprache::java());
        CHECK_FALSE(sprache::java().hatPraeprozessor());
        CHECK_FALSE(sprache::java().bindetKoepfeEin());
        std::vector<quelle::Meldung> m;
        const std::vector<Token> t =
            lexer::zerlege("s = \"\"\"\n  a \"b\"\n  \"\"\"; jj_3R_12$ = 'x'; f(String... a); x >>>= 2; #", sprache::java().tabelle(), m);
        REQUIRE(t.size() >= 18);
        CHECK(t[2].art == TokenArt::Zeichenkette);
        CHECK(t[2].text == "\"\"\"\n  a \"b\"\n  \"\"\"");
        CHECK(t[4].art == TokenArt::Bezeichner);
        CHECK(t[4].text == "jj_3R_12$");
        CHECK(t[6].text == "'x'");
        CHECK(t[11].text == "...");
        CHECK(t[16].text == ">>>=");
        // `#` gibt es in Java nicht: eine Meldung, keine Direktive
        CHECK(m.size() == 1);
    }

    TEST_CASE("Typen und Glieder: extends/implements, enum mit Rumpf, record, @interface, Initialisierer, throws")
    {
        const sprache::Ergebnis e = java(R"java(package a.b;
import java.util.List;
import static java.lang.Math.max;

@FunctionalInterface
public interface Pruefer<T> extends Comparable<T>, java.io.Serializable {
    boolean pruefe(T x);
    default boolean nie() { return false; }
    static <U> Pruefer<U> immer() { return null; }
}

@interface Marke {
    String wert() default "";
    String[] namen() default {"a", "b"};
}

enum Farbe implements Pruefer<Farbe> {
    ROT(5000), GRUEN(2) {
        @Override public boolean pruefe(Farbe x) { return true; }
    }, BLAU;

    private final int wert;
    Farbe(int wert) { this.wert = wert; }
    Farbe() { this(0); }
    public boolean pruefe(Farbe x) { return x == this; }
}

record Punkt(int x, int y) implements java.io.Serializable {
    Punkt {
        if (x < 0) throw new IllegalArgumentException();
    }
    static Punkt null_() { return new Punkt(0, 0); }
}

public abstract sealed class Lager<T extends Comparable<? super T>> extends Basis implements Pruefer<T>, AutoCloseable
        permits Klein, Gross {
    private static final int GRENZE = 3, ANDERE = 4;
    protected List<T> posten = new java.util.ArrayList<>();
    int alt[] = {1, 2};

    static { System.loadLibrary("x"); }
    { posten.clear(); }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public Lager(@Deprecated final int grenze, String... namen) throws IllegalStateException { }
    public abstract void leer() throws java.io.IOException, InterruptedException;
    public <R extends T> R erster(R[] liste, int zahlen[]) { return liste[0]; }
    public static void main(String args[]) { }

    static non-sealed class Klein extends Lager<String> { }
    final class Gross { }
}
)java");
        CHECK(meldungen(e).empty());
        REQUIRE(e.einheit.einbindungen.size() == 2);
        CHECK(e.einheit.einbindungen[1].ziel == "java.lang.Math.max");
        const modell::Typ& pruefer = typ(e, "Pruefer");  // package setzt keinen Bereich (festgelegt)
        CHECK(pruefer.art == modell::TypArt::Schnittstelle);
        CHECK(pruefer.basen == std::vector<std::string>{"Comparable<T>", "java.io.Serializable"});
        REQUIRE(pruefer.methoden.size() == 3);
        CHECK(pruefer.methoden[0].abstrakt);
        CHECK_FALSE(pruefer.methoden[1].abstrakt);
        const modell::Typ& marke = typ(e, "Marke");
        CHECK(marke.art == modell::TypArt::Schnittstelle);
        CHECK(marke.methoden.size() == 2);
        const modell::Typ& farbe = typ(e, "Farbe");
        CHECK(farbe.art == modell::TypArt::Aufzaehlung);
        CHECK(farbe.realisiert == std::vector<std::string>{"Pruefer<Farbe>"});
        REQUIRE(farbe.felder.size() == 4);
        CHECK(farbe.felder[0].name == "ROT");
        CHECK(farbe.felder[0].zusatz == "(5000)");
        CHECK(farbe.felder[2].name == "BLAU");
        CHECK(farbe.felder[3].name == "wert");
        const modell::Typ& punkt = typ(e, "Punkt");
        CHECK(punkt.art == modell::TypArt::Struktur);
        REQUIRE(punkt.felder.size() == 2);
        CHECK(punkt.felder[1].typ == "int");
        CHECK(punkt.felder[1].zusatz.empty());
        const modell::Typ& lager = typ(e, "Lager");
        CHECK(lager.basen == std::vector<std::string>{"Basis"});
        CHECK(lager.realisiert == std::vector<std::string>{"Pruefer<T>", "AutoCloseable"});
        REQUIRE(lager.felder.size() == 4);
        CHECK(lager.felder[1].name == "ANDERE");
        CHECK(lager.felder[3].name == "alt[]");
        CHECK(typ(e, "Lager::Klein").basen == std::vector<std::string>{"Lager<String>"});
        CHECK(namen(e) == "Pruefer::nie, Pruefer::immer, Farbe::GRUEN.pruefe, Farbe::Farbe, Farbe::Farbe, "
                          "Farbe::pruefe, Punkt::Punkt, Punkt::null_, Lager::<static:41>, "
                          "Lager::<init:42>, Lager::Lager, "
                          "Lager::erster, Lager::main");
        const modell::Funktion& konstruktor = funktion(e, "Lager::Lager");
        REQUIRE(konstruktor.parameter.size() == 2);
        CHECK(konstruktor.parameter[0].typ == "int");  // ohne Annotation und final
        CHECK(konstruktor.parameter[1].typ == "String...");
        const modell::Funktion& erster = funktion(e, "Lager::erster");
        CHECK(erster.rueckgabe == "R");
        REQUIRE(erster.parameter.size() == 2);
        CHECK(erster.parameter[1].typ == "int[]");
        CHECK(erster.parameter[1].name == "zahlen");
        // der kompakte Konstruktor hat die Komponenten als Parameter
        CHECK(funktion(e, "Punkt::Punkt").parameter.size() == 2);
        CHECK(baum(*funktion(e, "Punkt::Punkt").rumpf) == "B[IF:x < 0[THR:new IllegalArgumentException()]]");
        CHECK(baum(*funktion(e, "Lager::<static:41>").rumpf) ==
              "B[E:System.loadLibrary(\"x\")]");
        const diagramm::Strukturdiagramm d = diagramm::strukturVon(e.einheit);
        CHECK(std::any_of(d.beziehungen.begin(), d.beziehungen.end(), [](const diagramm::Beziehung& b) {
            return b.art == diagramm::BeziehungArt::Realisiert;
        }));
    }

    TEST_CASE("Anonyme Klassen (H1): je Methode eine Funktion <anonymous:Zeile>.name, kein Kasten")
    {
        const sprache::Ergebnis e = java(R"java(class Knopf {
    private final Runnable beimStart = new Runnable() {
        public void run() { starte(); }
    };
    void verbinde(Handler h) {
        h.post(new Runnable() {
            int zaehler = 0;
            @Override
            public void run() {
                zaehler++;
                h.post(() -> {
                    tu(zaehler);
                });
            }
            void hilf() { }
        });
        a(new X() { void f() {} }, new Y() { void g() {} });
        int[] feld = new int[] {1, 2};
    }
}
)java");
        CHECK(meldungen(e).empty());
        CHECK(e.einheit.typen.size() == 1);  // die anonymen Klassen sind keine Kaesten
        CHECK(namen(e) == "Knopf::<anonymous:2>.run, Knopf::verbinde, Knopf::verbinde::<anonymous:6>.run, "
                          "Knopf::verbinde::<anonymous:6>.run::<lambda:11>, Knopf::verbinde::<anonymous:6>.hilf, "
                          "Knopf::verbinde::<anonymous:17>.f, Knopf::verbinde::<anonymous:17:36>.g");
        // im Initialisierer eines Felds: Bereich die Klasse, ohne aeussere Funktion
        CHECK(funktion(e, "Knopf::<anonymous:2>.run").aussen.empty());
        CHECK(funktion(e, "Knopf::verbinde::<anonymous:6>.run").aussen == "Knopf::verbinde");
        CHECK(e.einheit.typen[0].felder.size() == 1);  // zaehler gehoert der anonymen Klasse
        const auto& k = funktion(e, "Knopf::verbinde").rumpf->kinder;
        REQUIRE(k.size() == 3);
        CHECK(k[0]->text == "h.post(new Runnable() {\xE2\x80\xA6})");
        CHECK(k[0]->innere ==
              std::vector<std::string>{"Knopf::verbinde::<anonymous:6>.run", "Knopf::verbinde::<anonymous:6>.hilf"});
        CHECK(k[1]->innere ==
              std::vector<std::string>{"Knopf::verbinde::<anonymous:17>.f", "Knopf::verbinde::<anonymous:17:36>.g"});
        CHECK(k[2]->text == "int[] feld = new int[] {1, 2}");  // ein Array, keine Klasse
        // `new Runnable()` ruft den Konstruktor (wie in C++ und C#); was im Rumpf der anonymen Klasse
        // gerufen wird, gehoert ihren Methoden
        CHECK(modell::aufrufeVon(funktion(e, "Knopf::verbinde")) ==
              std::vector<std::string>{"post", "Runnable", "a", "X", "Y"});
        CHECK(modell::aufrufeVon(funktion(e, "Knopf::verbinde::<anonymous:6>.run")) == std::vector<std::string>{"post"});
    }

    TEST_CASE("Anweisungen: for je Element, synchronized, try mit Ressourcen, switch mit : und ->, Marken")
    {
        const sprache::Ergebnis e = java(R"java(class K {
    int f(int[] xs, Object o) throws Exception {
        for (int x : xs) if (x > 0) continue;
        for (int i = o == null ? 0 : 1; i < 3; i++) { }
        for (java.util.List<? extends Number> l : listen) { }
        synchronized (o) { o.notify(); }
        try (var a = oeffne(); var b = oeffne()) { lies(a); }
        try { g(); } catch (IllegalStateException | IllegalArgumentException e) { h(); } finally { z(); }
        switch (o.hashCode()) {
            case 1, 2:
                a();
            default:
                b();
                break;
        }
        switch (xs.length) {
            case 0 -> leer();
            case 1 -> { eins(); }
            default -> throw new IllegalStateException();
        }
        String s = switch (xs.length) {
            case 0 -> "leer";
            default -> {
                tu();
                yield "voll";
            }
        };
        aussen:
        while (true) {
            do { if (q()) break aussen; } while (p());
        }
        @SuppressWarnings("unused") int frei = 0;
        assert frei == 0 : "nie";
        final class Lokal { void m() { } }
        return yield;
    }
}
)java");
        CHECK(meldungen(e).empty());
        const auto& k = funktion(e, "K::f").rumpf->kinder;
        REQUIRE(k.size() == 14);
        CHECK(baum(*k[0]) == "EACH:int x : xs[IF:x > 0[CNT]]");
        CHECK(k[1]->init == "int i = o == null ? 0 : 1");
        CHECK(baum(*k[2]) == "EACH:java.util.List<? extends Number> l : listen[B]");
        CHECK(baum(*k[3]) == "WITH:o[B[E:o.notify()]]");
        CHECK(k[3]->kopf == "synchronized (o)");
        CHECK(baum(*k[4]) == "TRY[B[E:lies(a)]]");
        CHECK(k[4]->kopf == "try (var a = oeffne(); var b = oeffne())");
        CHECK(baum(*k[5]) == "TRY[B[E:g()]; EXC:IllegalStateException | IllegalArgumentException e[B[E:h()]]; FIN[B[E:z()]]]");
        CHECK(baum(*k[6]) == "SW:o.hashCode()[B[CASE:1, 2; E:a(); CASE; E:b(); BRK]]");
        CHECK_FALSE(k[6]->ohneDurchfall);
        CHECK(baum(*k[7]) == "SW:xs.length[B[CASE:0; E:leer(); CASE:1; B[E:eins()]; CASE; THR:new IllegalStateException()]]");
        CHECK(k[7]->ohneDurchfall);
        CHECK(baum(*k[8]) == "SW:xs.length[B[CASE:0; E:String s = \"leer\"; CASE; B[E:tu(); E:String s = \"voll\"]]]");
        CHECK(k[8]->ohneDurchfall);
        CHECK(baum(*k[9]) == "aussen: WH:true[B[DO:p()[B[IF:q()[BRK:aussen]]]]]");
        CHECK(baum(*k[10]) == "E:int frei = 0");
        CHECK(baum(*k[11]) == "E:assert frei == 0 : \"nie\"");
        CHECK(baum(*k[12]) == "E:class Lokal");
        CHECK(baum(*k[13]) == "RET:yield");  // ausserhalb eines switch-Ausdrucks ein Name
        CHECK(funktion(e, "K::f::Lokal::m").name == "m");
    }

    TEST_CASE("Diagramme: switch-Ausdruck mit yield, break marke, try mit Ressourcen im Kopf")
    {
        const sprache::Ergebnis e = java(R"java(class K {
    String f(int[][] feld) {
        aussen:
        for (int[] zeile : feld) {
            for (int x : zeile) {
                if (x < 0) break aussen;
            }
        }
        try (Leser l = new Leser()) { l.lies(); }
        return switch (feld.length) {
            case 0 -> "leer";
            default -> {
                if (feld[0] == null) yield "luecke";
                yield "voll";
            }
        };
    }
}
)java");
        CHECK(meldungen(e).empty());
        const modell::Funktion& f = funktion(e, "K::f");
        const diagramm::Flussdiagramm d = diagramm::flussVon(f);
        const auto id = [&](std::string_view text) {
            for (std::size_t i = 0; i < d.knoten().size(); ++i)
                if (!d.knoten()[i].zeilen.empty() && d.knoten()[i].zeilen.front() == text) return i;
            FAIL("no node " << text);
            return std::size_t{0};
        };
        const auto kante = [&](std::size_t von, std::size_t nach, std::string_view text) {
            return std::any_of(d.kanten().begin(), d.kanten().end(), [&](const diagramm::Kante& k) {
                return k.von == von && k.nach == nach && k.beschriftung == text;
            });
        };
        // break aussen: hinter die aeussere Schleife, zum try
        CHECK(kante(id("x < 0"), id("try (Leser l = new Leser())"), "yes"));
        CHECK(kante(id("int[] zeile : feld"), id("try (Leser l = new Leser())"), "done"));
        CHECK(kante(id("feld.length"), id("return \"leer\""), "0"));
        CHECK(kante(id("feld[0] == null"), id("return \"luecke\""), "yes"));
        CHECK(kante(id("feld[0] == null"), id("return \"voll\""), "no"));
        const diagramm::Struktogramm s = diagramm::struktogrammVon(f);
        REQUIRE(s.bloecke.size() == 3);
        CHECK(s.bloecke[0].text == "aussen: for (int[] zeile : feld)");
        CHECK(s.bloecke[1].text == "try (Leser l = new Leser())");
        const std::vector<const diagramm::Block*> alle = diagramm::bloeckeVon(s);
        CHECK(std::any_of(alle.begin(), alle.end(), [](const diagramm::Block* b) { return b->text == "break aussen"; }));
    }

    TEST_CASE("Ausserhalb einer Klasse: eine Meldung, nichts still verworfen")
    {
        const sprache::Ergebnis e = java("int x = 1;\nclass K { }\n");
        CHECK(meldungen(e) == "1: unexpected 'int' outside a class\n");
        CHECK(e.einheit.typen.size() == 1);
    }
}
