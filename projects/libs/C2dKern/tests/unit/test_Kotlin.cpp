/**
 * @file test_Kotlin.cpp
 * @brief Kotlin: Lexer, Ende einer Anweisung, Typen und Glieder, Lambdas, when/if als Wert, Marken
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

sprache::Ergebnis kt(std::string_view text)
{
    return sprache::kotlin().lies(text, "T.kt");
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

/// Die Token als Text, Zeilenende `;;`; ohne das Ende.
std::string bild(std::string_view text, std::vector<quelle::Meldung>& m)
{
    std::string s;
    for (const Token& t : lexer::zerlege(text, sprache::kotlin().tabelle(), m))
    {
        if (t.art == TokenArt::Ende) break;
        if (!s.empty()) s += ' ';
        s += t.art == TokenArt::Zeilenende ? std::string(";;") : std::string(t.text);
    }
    return s;
}

}  // namespace

TEST_SUITE("Kotlin")
{
    TEST_CASE("Anmeldung und Lexer: Vorlagen mit ${…} auch verschachtelt, roh, Kommentare in Kommentaren, `Namen`")
    {
        CHECK(sprache::spracheFuer("Main.kt") == &sprache::kotlin());
        CHECK(sprache::spracheFuer("build.gradle.kts") == nullptr);  // K6: noch nicht
        CHECK_FALSE(sprache::kotlin().hatPraeprozessor());
        std::vector<quelle::Meldung> m;
        CHECK(bild("val s = \"a $x ${f(\"b ${c}\")} '}'\"\n", m) == "val s = \"a $x ${f(\"b ${c}\")} '}'\" ;;");
        CHECK(bild("val r = \"\"\"\n  ${'$'}x \"q\" \\n\n\"\"\"\"\n", m) == "val r = \"\"\"\n  ${'$'}x \"q\" \\n\n\"\"\"\" ;;");
        CHECK(bild("/* a /* b */ c */ fun `ein Test`() = x ?: y!!.z?.w ..< 3\n", m) ==
              "fun `ein Test` ( ) = x ?: y !! . z ?. w ..< 3 ;;");
        CHECK(bild("f(a,\n  b) {\n  g()\n}\n", m) == "f ( a , b ) { ;; g ( ) ;; } ;;");  // in `{` wieder Zeilen
        CHECK(m.empty());
    }

    TEST_CASE("Ende einer Anweisung am Zeilenende: `.` und `?:` setzen fort, `+` am Anfang nicht, else beendet")
    {
        const sprache::Ergebnis e = kt(R"kt(fun f(a: Int): Int {
    val x = a
        .plus(1)
        ?: 0
    val y = a
    +1
    if (a > 0) g() else h()
    val z = a +
        2
    return
}
)kt");
        CHECK(meldungen(e).empty());
        CHECK(baum(*funktion(e, "f").rumpf) ==
              "B[E:val x = a .plus(1) ?: 0; E:val y = a; E:+1; IF:a > 0[E:g()] else E:h(); E:val z = a + 2; RET]");
    }

    TEST_CASE("Typen und Glieder: primaerer Konstruktor, Basen, enum mit Rumpf, object, companion, Eigenschaften")
    {
        const sprache::Ergebnis e = kt(R"kt(@file:JvmName("Lager")
package de.beispiel

import java.io.File
import kotlinx.coroutines.flow.*

typealias Id = Long

interface Ablage : Comparable<Ablage> {
    fun lege(name: String, menge: Int): Boolean
    fun anzahl(): Int = 0
}

enum class Farbe(val wert: Int) {
    ROT(5000),
    GRUEN(2) {
        override fun hell() = true
    },
    BLAU;

    open fun hell(): Boolean = false
}

data class Posten(val name: String, var menge: Int = 0, notiz: String) {
    init {
        require(menge >= 0)
    }
    constructor(name: String) : this(name, 0, "")

    val leer: Boolean
        get() = menge == 0

    var zaehler: Int = 0
        private set

    var text: String = ""
        set(wert) {
            field = wert.trim()
        }
}

abstract class Lager<T : Comparable<T>> private constructor(protected val grenze: Int) : Basis(grenze), Ablage, Runnable by werk {
    private val posten = mutableListOf<T>()
    abstract fun pruefe()

    companion object {
        const val GROSS = 1000
        fun neu(): Lager<Int>? = null
    }

    inner class Fach(val nummer: Int)
}

object Einstellungen {
    var dunkel = false
    fun umschalten() {
        dunkel = !dunkel
    }
}

sealed interface Ergebnis {
    data class Erfolg(val wert: Int) : Ergebnis
    data object Laedt : Ergebnis
}
)kt");
        CHECK(meldungen(e).empty());
        REQUIRE(e.einheit.einbindungen.size() == 2);
        CHECK(e.einheit.einbindungen[1].ziel == "kotlinx.coroutines.flow.*");
        REQUIRE(e.einheit.aliase.size() == 1);
        CHECK(e.einheit.aliase[0].ziel == "Long");
        const modell::Typ& ablage = typ(e, "Ablage");
        CHECK(ablage.art == modell::TypArt::Schnittstelle);
        CHECK(ablage.basen == std::vector<std::string>{"Comparable<Ablage>"});
        REQUIRE(ablage.methoden.size() == 2);
        CHECK(ablage.methoden[0].abstrakt);
        CHECK_FALSE(ablage.methoden[1].abstrakt);
        const modell::Typ& farbe = typ(e, "Farbe");
        CHECK(farbe.art == modell::TypArt::Aufzaehlung);
        REQUIRE(farbe.felder.size() == 4);  // wert, ROT, GRUEN, BLAU
        CHECK(farbe.felder[0].name == "wert");
        CHECK(farbe.felder[1].zusatz == "(5000)");
        CHECK(farbe.felder[3].name == "BLAU");
        const modell::Typ& posten = typ(e, "Posten");
        // val/var des primaeren Konstruktors sind Felder, `notiz` nur ein Parameter
        REQUIRE(posten.felder.size() == 5);
        CHECK(posten.felder[0].name == "name");
        CHECK(posten.felder[1].typ == "Int");
        CHECK(posten.felder[1].typHinten);
        CHECK(posten.felder[2].name == "leer");
        CHECK(posten.felder[2].zusatz == "{ get; }");
        CHECK(posten.felder[3].zusatz == "{ set; } = 0");
        CHECK(posten.felder[4].zusatz == "{ set; } = \"\"");
        REQUIRE(posten.methoden.size() == 2);  // der primaere und der zweite Konstruktor
        CHECK(posten.methoden[0].parameter.size() == 3);
        const modell::Typ& lager = typ(e, "Lager");
        CHECK(lager.basen == std::vector<std::string>{"Basis"});
        CHECK(lager.realisiert == std::vector<std::string>{"Ablage", "Runnable"});
        REQUIRE(lager.felder.size() == 3);  // grenze, posten, GROSS (aus dem companion)
        CHECK(lager.felder[0].sicht == modell::Sichtbarkeit::Geschuetzt);
        const auto neu = std::find_if(lager.methoden.begin(), lager.methoden.end(), [](const modell::Methode& m) { return m.name == "neu"; });
        REQUIRE(neu != lager.methoden.end());
        CHECK(neu->statisch);  // K4: ein Glied des companion object ist statisch, kein Kasten Companion
        CHECK(std::none_of(e.einheit.typen.begin(), e.einheit.typen.end(), [](const modell::Typ& t) { return t.name == "Companion"; }));
        CHECK(typ(e, "Lager::Fach").felder.size() == 1);
        CHECK(typ(e, "Einstellungen").art == modell::TypArt::Klasse);
        CHECK(typ(e, "Ergebnis::Erfolg").realisiert == std::vector<std::string>{"Ergebnis"});
        // ohne Rumpf (lege, pruefe) ein Prototyp, wie in den anderen Sprachen
        CHECK(namen(e) == "Ablage::lege, Ablage::anzahl, Farbe::GRUEN.hell, Farbe::hell, Posten::<init:25>, "
                          "Posten::leer.get, Posten::text.set, Lager::pruefe, Lager::neu, Einstellungen::umschalten");
        CHECK(baum(*funktion(e, "Posten::leer.get").rumpf) == "B[RET:menge == 0]");
        CHECK(baum(*funktion(e, "Posten::text.set").rumpf) == "B[E:field = wert.trim()]");
        const diagramm::Strukturdiagramm d = diagramm::strukturVon(e.einheit);
        CHECK(std::any_of(d.beziehungen.begin(), d.beziehungen.end(), [](const diagramm::Beziehung& b) {
            return b.art == diagramm::BeziehungArt::Realisiert;
        }));
    }

    TEST_CASE("Funktionen: Erweiterung (K4), Ausdruck als Rumpf, Unit, lokale Funktion, ausserhalb einer Funktion")
    {
        const sprache::Ergebnis e = kt(R"kt(fun String.kuerze(n: Int): String = if (length <= n) this else take(n)
inline fun <reified T> List<Any>.nurVom(): List<T> = filterIsInstance<T>()
val String.istLeer: Boolean
    get() = isEmpty()
fun melde(text: String): Unit = println(text)
fun nie(): Nothing = throw IllegalStateException()
fun aussen(x: Int): Int {
    fun innen(y: Int) = y * 2
    return innen(x)
}
println("oben")
)kt");
        CHECK(meldungen(e) == "11: unexpected 'println' outside a function\n");
        CHECK(namen(e) == "String.kuerze, List<Any>.nurVom, String.istLeer.get, melde, nie, aussen, aussen::innen");
        CHECK(baum(*funktion(e, "String.kuerze").rumpf) == "B[IF:length <= n[RET:this] else RET:take(n)]");
        CHECK(baum(*funktion(e, "melde").rumpf) == "B[E:println(text)]");
        CHECK(baum(*funktion(e, "nie").rumpf) == "B[THR:IllegalStateException()]");
        CHECK(funktion(e, "aussen::innen").aussen == "aussen");
        const auto& k = funktion(e, "aussen").rumpf->kinder;
        REQUIRE(k.size() == 2);
        CHECK(k[0]->text == "fun innen(y: Int)");
        CHECK(k[0]->innere == std::vector<std::string>{"aussen::innen"});
        REQUIRE(e.einheit.variablen.size() == 1);
        CHECK(e.einheit.variablen[0].name == "String.istLeer");
    }

    TEST_CASE("Lambdas (K2): ein Ausdruck in einer Zeile bleibt Text, sonst eine Funktion; anonymes Objekt")
    {
        const sprache::Ergebnis e = kt(R"kt(class Knopf {
    private val beimStart = Runnable {
        starte()
    }
    fun verbinde(k: View, xs: List<Int>) {
        val doppelt = xs.map { it * 2 }
        k.setOnClickListener { v ->
            zeige(v)
            merke()
        }
        val rechne = { a: Int, b: Int ->
            a + b
        }
        k.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) {
                pruefe(s)
            }
        })
        scope.launch {
            lade()
        }.invokeOnCompletion { fertig() }
    }
}
)kt");
        CHECK(meldungen(e).empty());
        CHECK(namen(e) == "Knopf::<lambda:2>, Knopf::verbinde, Knopf::verbinde::<lambda:7>, Knopf::verbinde::rechne, "
                          "Knopf::verbinde::<anonymous:14>.afterTextChanged, Knopf::verbinde::<lambda:19>");
        const modell::Funktion& klick = funktion(e, "Knopf::verbinde::<lambda:7>");
        REQUIRE(klick.parameter.size() == 1);
        CHECK(klick.parameter[0].name == "v");
        CHECK(baum(*klick.rumpf) == "B[E:zeige(v); E:merke()]");
        CHECK(funktion(e, "Knopf::verbinde::rechne").parameter.size() == 2);
        const auto& k = funktion(e, "Knopf::verbinde").rumpf->kinder;
        REQUIRE(k.size() == 5);
        CHECK(k[0]->text == "val doppelt = xs.map { it * 2 }");
        CHECK(k[1]->text == "k.setOnClickListener {\xE2\x80\xA6}");
        CHECK(k[1]->innere == std::vector<std::string>{"Knopf::verbinde::<lambda:7>"});
        CHECK(k[3]->text == "k.addTextChangedListener(object : TextWatcher {\xE2\x80\xA6})");
        CHECK(k[4]->text == "scope.launch {\xE2\x80\xA6}.invokeOnCompletion { fertig() }");
        CHECK(e.einheit.typen.size() == 1);  // das anonyme Objekt ist kein Kasten
        CHECK(funktion(e, "Knopf::<lambda:2>").aussen.empty());
    }

    TEST_CASE("when und if als Wert (K3): mit Gegenstand eine Auswahl, ohne eine Kette, je Zweig die Zuweisung")
    {
        const sprache::Ergebnis e = kt(R"kt(fun f(x: Any, n: Int): String {
    val art = when (x) {
        is String, is Char -> "Text"
        in 1..9 -> {
            log()
            "Ziffer"
        }
        else -> "anderes"
    }
    when {
        n < 0 -> negativ()
        n == 0, n == 1 -> klein()
        else -> gross()
    }
    var s = if (n > 0) "plus" else "minus"
    s = try {
        lies()
    } catch (e: IOException) {
        "fehlt"
    }
    val t = when (n) { 1 -> "a" else -> "b" }.uppercase()
    return when (n) {
        0 -> "null"
        else -> throw IllegalArgumentException()
    }
}
)kt");
        CHECK(meldungen(e).empty());
        const auto& k = funktion(e, "f").rumpf->kinder;
        REQUIRE(k.size() == 6);
        CHECK(baum(*k[0]) == "SW:x[B[CASE:is String, is Char; E:val art = \"Text\"; CASE:in 1..9; B[E:log(); E:val art = "
                             "\"Ziffer\"]; CASE; E:val art = \"anderes\"]]");
        CHECK(k[0]->ohneDurchfall);
        CHECK(k[0]->kopf == "when (x)");
        CHECK(baum(*k[1]) == "IF:n < 0[E:negativ()] else IF:n == 0 || n == 1[E:klein()] else E:gross()");
        CHECK(baum(*k[2]) == "IF:n > 0[E:var s = \"plus\"] else E:var s = \"minus\"");
        CHECK(baum(*k[3]) == "TRY[B[E:s = lies()]; EXC:e: IOException[B[E:s = \"fehlt\"]]]");
        CHECK(baum(*k[4]) == "E:val t = when (n) { 1 -> \"a\" else -> \"b\" }.uppercase()");  // geht weiter: Text
        CHECK(baum(*k[5]) == "SW:n[B[CASE:0; RET:\"null\"; CASE; THR:IllegalArgumentException()]]");
        // `when` im Text ruft nichts — als Methode hinter `.` schon (pyspark `F.when`, JS `m.delete`)
        CHECK(modell::aufrufeIn(k[4]->text) == std::vector<std::string>{"uppercase"});
        CHECK(modell::aufrufeIn("F.when(a, 1).otherwise(0)") == std::vector<std::string>{"when", "otherwise"});
        CHECK(modell::aufrufeIn("m.delete(k); p?.catch(f); delete (x)") == std::vector<std::string>{"delete", "catch"});
        CHECK(modell::aufrufeIn("case int n when (n > 0):").empty());  // C#: ein Waechter
    }

    TEST_CASE("Schleifen und Marken (H2): for in, do while, break@aussen, continue@aussen, return@forEach")
    {
        const sprache::Ergebnis e = kt(R"kt(fun f(zeilen: List<List<Int>>) {
    aussen@ for (z in zeilen) {
        for (x in z) {
            if (x < 0) continue@aussen
            if (x == 0) break@aussen
        }
    }
    do {
        i++
    } while (i < 3)
    zeilen.forEach { z ->
        if (z.isEmpty()) return@forEach
        zeige(z)
    }
    fertig()
}
)kt");
        CHECK(meldungen(e).empty());
        const auto& k = funktion(e, "f").rumpf->kinder;
        REQUIRE(k.size() == 4);
        CHECK(baum(*k[0]) == "aussen: EACH:z in zeilen[B[EACH:x in z[B[IF:x < 0[CNT:aussen]; IF:x == 0[BRK:aussen]]]]]");
        CHECK(baum(*k[1]) == "DO:i < 3[B[E:i++]]");
        const modell::Funktion& lambda = funktion(e, "f::<lambda:11>");
        REQUIRE(lambda.rumpf->kinder.size() == 2);
        CHECK(lambda.rumpf->kinder[0]->kinder[0]->kopf == "return@forEach");
        const diagramm::Flussdiagramm d = diagramm::flussVon(funktion(e, "f"));
        const auto id = [&](std::string_view text) {
            for (std::size_t i = 0; i < d.knoten().size(); ++i)
                if (!d.knoten()[i].zeilen.empty() && d.knoten()[i].zeilen.front() == text) return i;
            FAIL("no node " << text);
            return std::size_t{0};
        };
        const auto kante = [&](std::size_t von, std::size_t nach, std::string_view text) {
            return std::any_of(d.kanten().begin(), d.kanten().end(), [&](const diagramm::Kante& kk) {
                return kk.von == von && kk.nach == nach && kk.beschriftung == text;
            });
        };
        CHECK(kante(id("x < 0"), id("z in zeilen"), "yes"));  // continue@aussen: an den aeusseren Kopf
        CHECK(kante(id("x == 0"), id("i++"), "yes"));         // break@aussen: hinter die aeussere Schleife
    }

    TEST_CASE("Die Musterprojekte: Bedingungen ohne bedingte Auswahl; ?: ist kein ?")
    {
        const sprache::Ergebnis e = kt("fun f(a: Int?, b: Boolean, c: Boolean): Int {\n    if (b && c || a == null) return a ?: 0\n"
                                       "    return 1\n}\n");
        CHECK(meldungen(e).empty());
        const modell::Anweisung& wenn = *funktion(e, "f").rumpf->kinder[0];
        REQUIRE(wenn.logik);
        CHECK(wenn.logik->art == modell::Logik::Art::Oder);
        CHECK(funktion(e, "f").rumpf->kinder[0]->kinder[0]->frage.empty());
    }
}
