/**
 * @file test_CppSprache.cpp
 * @brief C++: was der Leser der C-Familie mit der C++-Tabelle ins Codemodell bringt
 */
#include <ostream>  // doctest schreibt string_view ueber operator<<

#include <doctest.h>

#include "Beispiele.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/lexer/Lexer.hpp"
#include "c2d/sprache/Sprache.hpp"

using namespace c2d::modell;
using c2d::sprache::Ergebnis;

namespace
{

Ergebnis lies(std::string_view text, bool alleZweige = false)
{
    return c2d::sprache::cpp().lies(text, "test.cpp", {.alleZweige = alleZweige, .definitionen = {}});
}

const Typ& typ(const Einheit& e, std::string_view name)
{
    for (const Typ& t : e.typen)
        if (t.name == name) return t;
    FAIL("no type " << name);
    return e.typen.front();
}

char zeichen(Sichtbarkeit s)
{
    return s == Sichtbarkeit::Oeffentlich ? '+' : s == Sichtbarkeit::Geschuetzt ? '#' : s == Sichtbarkeit::Privat ? '-' : ' ';
}

/// Die Glieder als "<sicht>typ|name|zusatz;".
std::string glieder(const Typ& t)
{
    std::string s;
    for (const Feld& f : t.felder) s += zeichen(f.sicht) + f.typ + "|" + f.name + "|" + f.zusatz + ";";
    return s;
}

/// Die Methoden als "<sicht>[v][s][a] rueckgabe|signatur;".
std::string methoden(const Typ& t)
{
    std::string s;
    for (const Methode& m : t.methoden)
    {
        s += zeichen(m.sicht);
        if (m.virtuell) s += 'v';
        if (m.statisch) s += 's';
        if (m.abstrakt) s += 'a';
        s += " " + m.rueckgabe + "|" + m.signatur() + ";";
    }
    return s;
}

/// Die Funktionen als "bereich::name", durch Leerzeichen getrennt; ein Prototyp traegt ein `?`.
std::string funktionen(const Einheit& e)
{
    std::string s;
    for (const Funktion& f : e.funktionen) s += (s.empty() ? "" : " ") + f.vollerName() + (f.rumpf ? "" : "?");
    return s;
}

}  // namespace

TEST_SUITE("C++")
{
    TEST_CASE("Die Endungen; ein Kopf mit `h` ist C++, wenn er etwas nennt, das C nicht hat")
    {
        using c2d::sprache::spracheFuer;
        CHECK(spracheFuer("motor.cpp") == &c2d::sprache::cpp());
        CHECK(spracheFuer("Motor.HPP") == &c2d::sprache::cpp());
        CHECK(spracheFuer("a.h", "struct x { int a; };") == &c2d::sprache::c());
        CHECK(spracheFuer("a.h", "class X {};") == &c2d::sprache::cpp());
        CHECK(spracheFuer("a.h", "void f(std::size_t n);") == &c2d::sprache::cpp());
        CHECK(spracheFuer("a.h", "int classify(int subclass);") == &c2d::sprache::c());  // nur ganze Woerter
    }

    TEST_CASE("Lexer: rohe Zeichenketten und Zifferntrenner")
    {
        std::vector<c2d::quelle::Meldung> meldungen;
        const auto t = c2d::lexer::zerlege("auto s = u8R\"x(a \"b\" )\" )x\"; int n = 1'000'000; a::b;",
                                           c2d::sprache::cpp().tabelle(), meldungen);
        CHECK(meldungen.empty());
        REQUIRE(t.size() == 15);
        CHECK(t[3].art == c2d::lexer::TokenArt::Zeichenkette);
        CHECK(t[3].text == "u8R\"x(a \"b\" )\" )x\"");
        CHECK(t[8].text == "1'000'000");
        CHECK(t[11].text == "::");
    }

    TEST_CASE("Typen: Namensraum, enum class, Klasse mit Basis, Sichtbarkeit (beispiele/motor.cpp)")
    {
        const Ergebnis ergebnis = lies(beispiele::kMotor);
        CHECK(ergebnis.meldungen.empty());
        const Einheit& e = ergebnis.einheit;

        REQUIRE(e.typen.size() == 4);
        const Typ& zustand = typ(e, "Zustand");
        CHECK(zustand.art == TypArt::Aufzaehlung);
        CHECK(zustand.vollerName() == "antrieb::Zustand");
        CHECK(zustand.basen.empty());  // `: int` ist der Typ darunter, keine Basis
        CHECK(glieder(zustand) == " |Aus|; |Anlauf|; |Betrieb|; |Fehler|;");

        const Typ& geraet = typ(e, "Geraet");
        CHECK(geraet.art == TypArt::Klasse);
        CHECK(glieder(geraet) == "-std::string|m_name|;");
        CHECK(methoden(geraet) == "+v |~Geraet();"
                                  "+va bool|starte();"
                                  "+va void|stoppe();"
                                  "+ const std::string&|name();"
                                  "# |Geraet(std::string name);");

        // in einem struct ist alles oeffentlich; der Anfangswert gehoert nicht zum Glied
        CHECK(glieder(typ(e, "Messwert")) == "+double|strom|;+double|drehzahl|;");

        const Typ& motor = typ(e, "Motor");
        CHECK(motor.basen == std::vector<std::string>{"Geraet"});  // ohne `public`, und `final` ist kein Name
        CHECK(glieder(motor) == "-Zustand|m_zustand|;"
                                "-int|m_polpaare|;"
                                "-std::vector<Messwert>|m_verlauf|;"
                                "-Geraet*|m_partner|;"
                                "-static int|s_anzahl|;");
        CHECK(methoden(motor) == "+ |Motor(std::string name, int polpaare);"
                                 "+ bool|starte();"
                                 "+ void|stoppe();"
                                 "+s int|anzahl();"
                                 "+ Motor&|operator+=(const Messwert& wert);");
    }

    TEST_CASE("Funktionen: im Typ und ausserhalb definiert, Konstruktor, Operator, Schablone")
    {
        const Einheit e = lies(beispiele::kMotor).einheit;

        CHECK(funktionen(e) == "antrieb::Geraet::name antrieb::Geraet::Geraet antrieb::Motor::Motor "
                               "antrieb::Motor::starte antrieb::Motor::stoppe antrieb::Motor::anzahl "
                               "antrieb::Motor::operator+= begrenze");
        CHECK(e.funktionen[2].signatur() == "antrieb::Motor::Motor(std::string name, int polpaare)");
        CHECK(e.funktionen[2].rueckgabe.empty());
        CHECK(e.funktionen[2].rumpf->kinder.size() == 1);  // hinter den Anfangswerten der Glieder
        CHECK(e.funktionen[7].rueckgabe == "T");           // die Schablone davor ist uebergangen
        CHECK(e.funktionen[7].signatur() == "begrenze(T wert, T unten, T oben)");

        REQUIRE(e.variablen.size() == 1);
        CHECK(e.variablen[0].typ == "int");
        CHECK(e.variablen[0].name == "Motor::s_anzahl");

        REQUIRE(e.aliase.size() == 1);
        CHECK(e.aliase[0].name == "Verlauf");
        CHECK(e.aliase[0].ziel == "std::vector<antrieb::Messwert>");
    }

    TEST_CASE("Ablauf: try/catch, throw, range-for")
    {
        const Einheit e = lies(beispiele::kMotor).einheit;
        const c2d::diagramm::Flussdiagramm d = c2d::diagramm::flussVon(e.funktionen[3]);

        std::string bild;
        for (const c2d::diagramm::Knoten& k : d.knoten()) bild += k.zeilen[0] + ";";
        CHECK(bild == "antrieb::Motor::starte();m_zustand == Zustand::Fehler;return false;try;"
                      "pruefe(m_polpaare);m_zustand = Zustand::Anlauf;melde(e.what());"
                      "m_zustand = Zustand::Fehler;throw;const auto& wert : m_verlauf;wert.strom > 10.0;"
                      "glaette(wert);m_zustand = Zustand::Betrieb;return true;End;");

        const auto kante = [&](std::size_t von, std::size_t nach, std::string_view text) {
            for (const c2d::diagramm::Kante& k : d.kanten())
                if (k.von == von && k.nach == nach && k.beschriftung == text) return true;
            return false;
        };
        CHECK(kante(3, 4, ""));                                  // try: in den Block
        CHECK(kante(3, 6, "catch (const std::exception& e)"));   // und in den Fang
        CHECK(kante(8, 14, ""));                                 // throw verlaesst die Funktion
        CHECK(kante(5, 9, ""));                                  // nach dem Block geht es weiter
        CHECK(kante(9, 10, "each"));
        CHECK(kante(11, 9, ""));
        CHECK(kante(9, 12, "done"));
        CHECK(kante(10, 12, "yes"));  // break
    }

    TEST_CASE("Qt-Formen: Q_OBJECT, Q_PROPERTY, signals, slots; Schablonen in Typen")
    {
        const Ergebnis ergebnis = lies(R"cpp(
class Fenster : public QMainWindow, private Helfer<int> {
    Q_OBJECT
    Q_PROPERTY(int breite READ breite)
public:
    explicit Fenster(QWidget* parent = nullptr);
    ~Fenster() override;
signals:
    void geaendert(int wert);
public slots:
    void setze(int wert) { m_wert = wert; emit geaendert(wert); }
private:
    int m_wert{0};
    std::map<QString, std::function<void(int)>> m_rueckrufe;
    struct Innen { int a; };
    Innen m_innen;
};
)cpp");
        CHECK(ergebnis.meldungen.empty());
        const Einheit& e = ergebnis.einheit;

        REQUIRE(e.typen.size() == 2);
        const Typ& fenster = typ(e, "Fenster");
        CHECK(fenster.basen == std::vector<std::string>{"QMainWindow", "Helfer<int>"});
        CHECK(methoden(fenster) == "+ |Fenster(QWidget* parent);"
                                   "+ |~Fenster();"
                                   "+ void|geaendert(int wert);"
                                   "+ void|setze(int wert);");
        CHECK(glieder(fenster) == "-int|m_wert|;"
                                  "-std::map<QString, std::function<void(int)>>|m_rueckrufe|;"
                                  "-Innen|m_innen|;");
        CHECK(typ(e, "Innen").vollerName() == "Fenster::Innen");
        CHECK(funktionen(e) == "Fenster::setze");
    }

    TEST_CASE("Namensraeume, extern \"C\", Operatoren, Attribute, Lambda, static_assert")
    {
        const Ergebnis ergebnis = lies(R"cpp(
namespace a::b { inline namespace v1 { int f() { return 1; } } }
extern "C" { void c_api(int); }
bool operator==(const P& l, const P& r) { return l.x == r.x; }
struct P {
    int x;
    bool operator<(const P& o) const { return x < o.x; }
    P& operator=(const P&) = delete;
    int operator()(int a) const { return a; }
};
auto lam = [](int v) { return v * 2; };
constexpr int kMax = 1'000;
static_assert(kMax > 0, "x");
[[nodiscard]] int g(int a) noexcept { if constexpr (true) { return a; } co_return a; }
)cpp");
        CHECK(ergebnis.meldungen.empty());
        const Einheit& e = ergebnis.einheit;

        CHECK(funktionen(e) == "a::b::v1::f c_api? operator== P::operator< P::operator() g");
        CHECK(methoden(typ(e, "P")) == "+ bool|operator<(const P& o);"
                                       "+ P&|operator=(const P&);"
                                       "+ int|operator()(int a);");
        REQUIRE(e.variablen.size() == 2);
        CHECK(e.variablen[0].name == "lam");
        CHECK(e.variablen[0].typ == "auto");
        CHECK(e.variablen[1].name == "kMax");

        const Anweisung& rumpf = *e.funktionen[5].rumpf;
        REQUIRE(rumpf.kinder.size() == 2);
        CHECK(rumpf.kinder[0]->art == AnweisungArt::Wenn);  // if constexpr
        CHECK(rumpf.kinder[0]->text == "true");
        CHECK(rumpf.kinder[1]->art == AnweisungArt::Ruecksprung);  // co_return
    }

    TEST_CASE("Aus echtem Code: Makros ohne Semikolon, else hinter #ifdef, rohe Zeichenkette im #define")
    {
        // so in LumiViz (IWidget.hpp, test_*.cpp, MultiEffectVisualizer.cpp) und im Comm Studio (MainWindow.cpp)
        const Ergebnis ergebnis = lies(R"cpp(
#define SHADER R"(
void main() { gl_FragColor = vec4(1.0); }
)"
void f(int a, int b)
{
    Q_UNUSED(a)
    Q_UNUSED(b)
    if (a == 1)
        eins();
#ifdef MIT_ZWEI
    else if (a == 2)
        zwei();
#endif
    else
        sonst();
    SUBCASE("x") { drinnen(); }
    danach();
}
void g(const T& s) { Q_UNUSED(s) }
)cpp",
                                       true);  // alle Zweige: es geht um das else hinter dem #ifdef
        CHECK(ergebnis.meldungen.empty());
        const Einheit& e = ergebnis.einheit;

        REQUIRE(e.makros.size() == 1);
        CHECK(e.makros[0].name == "SHADER");
        CHECK(funktionen(e) == "f g");

        const Anweisung& rumpf = *e.funktionen[0].rumpf;
        REQUIRE(rumpf.kinder.size() == 5);
        CHECK(rumpf.kinder[0]->text == "Q_UNUSED(a)");
        CHECK(rumpf.kinder[1]->text == "Q_UNUSED(b)");
        // die Kette if / else if / else bleibt eine, ueber das #ifdef hinweg
        const Anweisung& wenn = *rumpf.kinder[2];
        REQUIRE(wenn.sonst != nullptr);
        CHECK(wenn.sonst->art == AnweisungArt::Wenn);
        CHECK(wenn.sonst->text == "a == 2");
        REQUIRE(wenn.sonst->sonst != nullptr);
        CHECK(wenn.sonst->sonst->text == "sonst()");
        // SUBCASE("x") { ... }: der Kopf, dann der Inhalt
        REQUIRE(rumpf.kinder[3]->kinder.size() == 2);
        CHECK(rumpf.kinder[3]->kinder[0]->text == "SUBCASE(\"x\")");
        CHECK(rumpf.kinder[3]->kinder[1]->text == "drinnen()");
        CHECK(rumpf.kinder[4]->text == "danach()");
    }

    TEST_CASE("Datentypen: Vererbung und Beziehungen ueber Schablonen und Zeiger")
    {
        using namespace c2d::diagramm;
        const Strukturdiagramm d = strukturVon(lies(beispiele::kMotor).einheit);

        REQUIRE(d.kaesten.size() == 4);
        CHECK(d.kaesten[3].name == "antrieb::Motor");
        CHECK(d.kaesten[3].art == KastenArt::Klasse);
        CHECK(d.kaesten[3].eintraege[0].sicht == '-');
        REQUIRE(d.kaesten[1].methoden.size() == 5);
        CHECK(d.kaesten[1].methoden[1].text() == "bool starte()");
        CHECK(d.kaesten[1].methoden[1].abstrakt);
        CHECK(d.kaesten[1].methoden[4].sicht == '#');

        const auto bild = [&](const Beziehung& b) {
            const char art = b.art == BeziehungArt::Enthaelt ? '*' : b.art == BeziehungArt::Verweist ? '>' : '^';
            return d.kaesten[b.von].name + art + d.kaesten[b.nach].name + ":" + b.beschriftung;
        };
        REQUIRE(d.beziehungen.size() == 4);
        CHECK(bild(d.beziehungen[0]) == "antrieb::Motor^antrieb::Geraet:");           // erbt
        CHECK(bild(d.beziehungen[1]) == "antrieb::Motor>antrieb::Zustand:m_zustand"); // Aufzaehlung
        CHECK(bild(d.beziehungen[2]) == "antrieb::Motor*antrieb::Messwert:m_verlauf"); // in einem vector: als Wert
        CHECK(bild(d.beziehungen[3]) == "antrieb::Motor>antrieb::Geraet:m_partner");  // Zeiger
    }
}
