/**
 * @file Code.hpp
 * @brief Das Codemodell: was eine Quelldatei enthaelt — fuer alle Sprachen gleich
 *
 * Der Leser einer Sprache (sprache/Sprache.hpp) fuellt dieses Modell; die
 * Diagramme (diagramm/Ableitung.hpp) lesen nur es und kennen keine Sprache.
 * Eine neue Sprache bringt einen neuen Leser, kein neues Modell — fehlt dem
 * Modell etwas (etwa Methoden einer Klasse), wird es hier ergaenzt.
 *
 * Ausdruecke bleiben Text: ein Diagramm zeigt sie, es rechnet nicht mit ihnen.
 */
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::modell
{

// ---------------------------------------------------------------------------
// Anweisungen
// ---------------------------------------------------------------------------

enum class AnweisungArt
{
    Einfach,       ///< `text;` — Ausdruck oder lokale Deklaration
    Block,         ///< kinder
    Wenn,          ///< text = Bedingung; kinder[0] = dann; sonst (darf fehlen)
    Solange,       ///< text = Bedingung; kinder[0] = Rumpf
    TueSolange,    ///< kinder[0] = Rumpf; text = Bedingung
    Fuer,          ///< init; text = Bedingung; schritt; kinder[0] = Rumpf
    Auswahl,       ///< text = Ausdruck; kinder[0] = Rumpf (ein Block mit Faellen)
    Fall,          ///< text = Wert; leer = der Standardfall (default)
    Ruecksprung,   ///< text = Wert (darf leer sein)
    Abbruch,       ///< break; text = die Marke (`break outer`, Java/JavaScript), leer = die innerste
    Fortsetzung,   ///< continue; text = die Marke wie bei Abbruch
    Sprung,        ///< text = Marke (goto)
    Marke,         ///< text = Name
    Direktive,     ///< text = die Zeile (Praeprozessor im Rumpf)
    FuerJedes,     ///< text = `Element : Menge`; kinder[0] = Rumpf (range-for)
    /// kinder[0] = der Block; kinder[1..] = je ein Fang (try/catch, Python except), zuletzt
    /// vielleicht ein Schluss; sonst = was nach einem try ohne Ausnahme laeuft (Python else)
    Versuch,
    Fang,          ///< text = was gefangen wird; kinder[0] = der Block (catch)
    Wurf,          ///< text = was geworfen wird (throw); darf leer sein
    /// finally: kinder[0] = der Block — er laeuft auf jedem Weg aus dem try, auch nach
    /// return, break, continue; nur als letztes Kind eines Versuchs
    Schluss,
    /// Ein Block unter einem verwalteten Gegenstand (Python with, C# using): text = was
    /// verwaltet wird (`open(p) as f`); kinder[0] = der Rumpf
    Mit,
    /// Bedingte Uebersetzung im Rumpf, als Varianten gelesen (LeseOptionen::varianten):
    /// text = `#if`; kinder = je Zweig ein Zweig, in der Reihenfolge des Texts. Fehlt
    /// das `#else`, steht dafuer ein leerer Zweig `else` am Ende.
    Varianten,
    /// Ein Zweig von Varianten: text = seine Bedingung (`A`, `defined(A)`, `else`);
    /// kinder = seine Anweisungen; gilt = er gilt mit den Definitionen.
    Zweig,
    /// Anweisungen, die nur unter einer Bedingung der Uebersetzung stehen — der
    /// geltende Zweig eines #if (LeseOptionen::bereiche): text = die volle
    /// Bedingung (volleBedingung); kinder = die Anweisungen.
    Bedingt,
};

/// Die Logik einer Bedingung (`(a && b) || !c`): ein Baum aus Und, Oder, Nicht und Atomen.
/// Ein Atom ist, was zwischen den logischen Operatoren steht: ein Vergleich, ein Aufruf, ein Name.
struct Logik
{
    enum class Art
    {
        Atom,
        Nicht,  ///< teile[0]
        Und,    ///< teile, von links ausgewertet; endet beim ersten falschen
        Oder,   ///< teile, von links ausgewertet; endet beim ersten wahren
    };
    Art art = Art::Atom;
    std::string text;           ///< nur Atom: wie es in der Quelle steht
    bool mitAufruf = false;     ///< nur Atom: es ruft eine Funktion (Kurzschluss kann ihn auslassen)
    std::vector<Logik> teile;
};

/// Ein Glied einer Aufrufkette: `.name`, mit Klammer ein Aufruf; `[…]` heisst `[]`.
struct Kettenglied
{
    std::string name;
    bool aufruf = false;
    std::vector<std::string> argumente;  ///< je Argument der Text der Quelle (`mode="x"` mit Namen)
    std::vector<std::string> namen;      ///< die Bezeichner in den Argumenten, je einmal, in Textfolge
};

inline constexpr std::size_t kKeineKette = static_cast<std::size_t>(-1);

/// Eine Kette von Aufrufen in einer Anweisung (Entscheid F1): `df.withColumn(…).select(…)`,
/// `SparkSession.builder.appName("a").getOrCreate()`. Sprachneutral — was ein Glied bedeutet, weiss erst
/// die Ableitung (Spark: diagramm/Datenfluss). Eine Klammer um die ganze Kette faellt weg.
struct Aufrufkette
{
    std::vector<Kettenglied> glieder;  ///< [0] ist die Wurzel (`df`, `SparkSession`, `f(x)`)
    /// Wem sie zugewiesen wird, wenn sie der ganze Wert einer Zuweisung ist (`df = …`); der Text links
    /// (`a, b`, `self.df`); leer = keiner.
    std::string ziel;
    /// Sie ist der ganze Ausdruck: der Wert einer Zuweisung, eines return, einer Anweisung, eines
    /// Arguments (bei einer inneren Kette) — kein Operator dahinter oder davor.
    bool ganz = false;
    /// Eine Kette in einem Argument einer anderen: deren Stelle in Anweisung::ketten, das Glied und das
    /// Argument; sonst kKeineKette.
    std::size_t eltern = kKeineKette;
    std::size_t glied = 0;
    std::size_t argument = 0;
    int zeile = 0;  ///< in der Datei
};

struct Anweisung
{
    AnweisungArt art = AnweisungArt::Einfach;
    std::string text;
    std::string init;     ///< nur Fuer
    std::string schritt;  ///< nur Fuer
    std::vector<std::unique_ptr<Anweisung>> kinder;
    /// Wenn: der Zweig else. Solange, Fuer, FuerJedes: was laeuft, wenn die Schleife ohne break
    /// endet (Python else). Versuch: was laeuft, wenn im try keine Ausnahme kam (Python else).
    std::unique_ptr<Anweisung> sonst;
    /// Wie der Kopf in der Quelle steht, wo die Sprache ihn nicht in der Form der C-Familie
    /// schreibt — `while x > 0`, `for x in xs`, `except ValueError as e`, `raise X`, `match x`,
    /// `with open(p) as f`; leer = die Diagramme setzen ihn aus text zusammen (`while (x)`).
    std::string kopf;
    /// Nur Auswahl: ein Fall endet an der naechsten Fall-Marke, ohne in sie zu laufen (Python
    /// match); ein break darin verlaesst die Schleife darum, nicht die Auswahl.
    bool ohneDurchfall = false;
    /// Schleifen, Auswahl und Block: die Marke davor (`outer: for …`, Java/JavaScript), die ein
    /// `break outer` / `continue outer` meint; leer = keine. (In C bleibt eine Marke eine Anweisung
    /// Marke — sie ist das Ziel eines goto.)
    std::string marke;
    int zeile = 0;
    bool gilt = false;  ///< nur Zweig
    /// Eine Bedingung mit mindestens zwei Atomen (ein `if`, eine Schleife, `?:`, ein `return` oder
    /// eine Zuweisung mit `&&`/`||`): ihr Text und ihre Logik — fuer die Wahrheitstabelle.
    std::string logikText;
    std::optional<Logik> logik;
    /// Nur Einfach und Ruecksprung: die Bedingung eines `?:` auf oberster Ebene (auch mit einem
    /// Teil) — fuer die Entscheidungstabelle eine Verzweigung; leer = keins.
    std::string frage;
    /// Die Bedingung (eines if, einer Schleife, eines `?:`) ruft eine Funktion: sie kann bei
    /// jeder Pruefung anders ausgehen.
    bool bedingungRuft = false;
    /// Die Namen, die die Bedingung (eines if, einer Schleife, eines switch, eines `?:`) liest.
    std::vector<std::string> bedingungNamen;
    /// Was die Anweisung aendern kann (Einfach, Ruecksprung, Wurf): die Namen, die sie schreibt
    /// (Zuweisung, `++`/`--`, `&name`); ob sie durch einen Zeiger schreibt (`*p =`, `p->x =`) —
    /// dann kann es jeder Name sein; ob sie etwas ruft — dann ebenso.
    std::vector<std::string> schreibt;
    bool schreibtDurchZeiger = false;
    bool ruft = false;
    /// Nur Einfach, genau eine Zuweisung `ziel = wert` (nicht `+=`): beide Seiten — fuer Zuordnungen.
    std::string ziel;
    std::string wert;
    /// Die Funktionen, die in der Anweisung definiert werden (ein Callback `(x) => {…}`, eine Methode
    /// eines Objekts, Python `def` im Rumpf): ihr voller Name — der Text nennt sie nicht.
    std::vector<std::string> innere;
    /// Die Aufrufketten in `text` (Einfach, Ruecksprung, Wurf, Mit; bei Schleifen und if der Kopf), aeussere vor
    /// inneren, in Textfolge; ein Name allein nur als ganzer Wert. Bisher fuellt sie der Leser von Python.
    std::vector<Aufrufkette> ketten;
};

using AnweisungPtr = std::unique_ptr<Anweisung>;

/// Wann der Zweig `zweig` einer Kette `#if` / `#elif` / `#else` gilt: keiner
/// davor und er selbst — `!defined(A) && B`. `bedingungen` sind die Bedingungen
/// der Zweige wie bei AnweisungArt::Zweig (`A`, `defined(A)`, `else`); `zweig`
/// darf gleich ihrer Zahl sein: das fehlende `#else`.
[[nodiscard]] std::string volleBedingung(const std::vector<std::string>& bedingungen, std::size_t zweig);

// ---------------------------------------------------------------------------
// Deklarationen
// ---------------------------------------------------------------------------

/// Typ, Name und Zusatz wieder als eine Zeile, gesetzt wie im Quelltext: ein
/// Leerzeichen dazwischen, ausser der Typ endet auf abgesetzte Sterne oder `(`
/// oder der Zusatz beginnt mit `[` oder `)` — `u8 daten[16]`,
/// `struct Knoten *naechster`, `QWidget* parent`, `void (*rueckruf)(int)`,
/// `unsigned bereit : 1`. Mit `typHinten` steht der Typ hinter dem Namen (`name: typ`,
/// Python), der Zusatz mit Abstand dahinter (`name: typ = 3`).
[[nodiscard]] std::string deklaration(const std::string& typ, const std::string& name,
                                      const std::string& zusatz = {}, bool typHinten = false);

struct Parameter
{
    std::string typ;
    std::string name;  ///< darf leer sein (Prototyp); in Python mit `*`/`**` davor
    bool typHinten = false;  ///< `name: typ` (Python)
};

/// `bereich::name`, oder nur der Name, wenn es keinen Bereich gibt.
[[nodiscard]] std::string vollerName(const std::string& bereich, const std::string& name);

/// `name(typ name, ...)`
[[nodiscard]] std::string signatur(const std::string& name, const std::vector<Parameter>& parameter);

/// Wie etwas auf eine Tabelle oder eine Routine der Datenbank zugreift (Entscheid A2).
enum class ZugriffArt
{
    Liest,     ///< FROM, JOIN, Unterabfragen
    Schreibt,  ///< INSERT INTO, UPDATE, DELETE FROM, MERGE INTO, TRUNCATE
    Ruft,      ///< CALL, EXEC, eine Funktion im Ausdruck
};

struct Zugriff
{
    ZugriffArt art = ZugriffArt::Liest;
    std::string ziel;  ///< wie es in der Quelle heisst, ohne Begrenzer, mit Schema (`public.actor`)
    int zeile = 0;
    /// Nur Ruft: aus einem Ausdruck (`f(x)`), nicht aus CALL/EXEC — es zaehlt nur, wenn das Ziel eine
    /// Routine des Projekts ist (sonst ist es eine eingebaute Funktion wie `count`).
    bool imAusdruck = false;
    /// Aus SQL in einer Zeichenkette, die nicht in klarer Form steht (nicht als Argument eines Aufrufs
    /// wie `execute(`): es zaehlt nur, wenn das Ziel eine Tabelle des Projekts ist (Entscheid A4).
    bool unsicher = false;
};

struct Funktion
{
    std::string name;     ///< ohne Bereich; auch `~Motor`, `operator==`
    std::string bereich;  ///< Namensraum und Klasse, etwa `antrieb::Motor`; leer in C
    std::string rueckgabe;  ///< alles vor dem Namen, etwa `static int`
    std::vector<Parameter> parameter;
    AnweisungPtr rumpf;  ///< leer = nur deklariert
    int zeile = 0;
    int bisZeile = 0;  ///< die Zeile, in der der Rumpf endet (`}`); 0 = ohne Rumpf
    /// Nur in ihrer Datei sichtbar (`static` vor einer freien Funktion) — ein
    /// Aufruf aus einer anderen Datei meint sie nie.
    bool intern = false;
    /// In einer anderen Funktion definiert (Python: `def` im Rumpf): deren voller Name;
    /// leer = auf oberster Ebene oder in einem Typ. Im Rumpf der aeusseren steht dafuer eine
    /// Anweisung `def name(…)`.
    std::string aussen;
    /// Was SQL in ihrem Rumpf liest, schreibt, ruft (Entscheid A4) — vom Projekt aus den Zeichenketten.
    std::vector<Zugriff> zugriffe;

    [[nodiscard]] std::string vollerName() const { return modell::vollerName(bereich, name); }
    /// Meint ein Aufruf diese Funktion, dem Namen nach? `f` passt auf jede
    /// Funktion `f`, gleich in welchem Bereich; `N::f` nur auf eine, deren voller
    /// Name so endet (`N::f`, `a::N::f`).
    [[nodiscard]] bool heisst(std::string_view aufruf) const;
    /// `bereich::name(typ name, ...)` — der Text des Startknotens.
    [[nodiscard]] std::string signatur() const { return modell::signatur(vollerName(), parameter); }
};

/// Die Funktionen, die ein Text ruft: jeder Name vor einer `(`, mit seinem
/// Bereich (`a::b(` gibt `a::b`), ohne Objekt davor (`x.b(`, `p->b(` geben `b`),
/// jeder einmal, in der Reihenfolge des Texts. Woerter der C-Familie und von Python,
/// die so aussehen (`if (`, `sizeof(`, `not (`), zaehlen nicht; in Literalen wird
/// nichts gerufen.
[[nodiscard]] std::vector<std::string> aufrufeIn(std::string_view text);

/// Die Namen in einem Text, mit Bereich (`a::B`), ausserhalb von Literalen, jeder einmal, in der
/// Reihenfolge des Texts — Kandidaten fuer Typen (projekt/Typen.hpp); was davon einer ist,
/// entscheidet, wer die Typen kennt.
[[nodiscard]] std::vector<std::string> bezeichnerIn(std::string_view text);

/// Was der Rumpf einer Funktion ruft, ueber alle Anweisungen, jeder einmal.
[[nodiscard]] std::vector<std::string> aufrufeVon(const Funktion& funktion);

/// Ein Aufruf im Rumpf, mit der Bedingung der Uebersetzung, unter der er steht.
struct Aufrufstelle
{
    std::string name;  ///< wie bei aufrufeIn
    int zeile = 0;     ///< die Zeile der ersten Anweisung, die so ruft
    /// Die innerste Bedingung, unter der er steht: die volle Bedingung eines Zweigs von
    /// Varianten, der nicht gilt, oder eines Bereichs (AnweisungArt::Bedingt). Leer = keine
    /// — auch im geltenden Zweig von Varianten.
    std::string bedingung;
    /// false: er steht in einem Zweig von Varianten, der mit den Definitionen nicht gilt.
    bool gilt = true;
};

/// Was der Rumpf einer Funktion ruft, mit den Bedingungen: jede Verbindung aus Name,
/// Bedingung und `gilt` einmal, in der Reihenfolge des Rumpfs.
[[nodiscard]] std::vector<Aufrufstelle> aufrufstellenVon(const Funktion& funktion);

enum class TypArt
{
    Struktur,
    Vereinigung,
    Aufzaehlung,
    Klasse,
    Schnittstelle,  ///< interface (TypeScript; spaeter C#, Java): Glieder und Methoden ohne Rumpf
    Tabelle,        ///< eine Tabelle einer Datenbank (SQL): die Felder sind ihre Spalten
};

/// Wer ein Glied sehen darf. `Keine`: die Sprache kennt das nicht (C).
enum class Sichtbarkeit
{
    Keine,
    Oeffentlich,
    Geschuetzt,
    Privat,
};

struct Feld
{
    std::string typ;     ///< leer bei den Werten einer Aufzaehlung
    std::string name;    ///< leer = namenloses Glied (anonyme Struktur)
    std::string zusatz;  ///< `[8]`, `: 3` oder `= 4`
    Sichtbarkeit sicht = Sichtbarkeit::Keine;
    int zeile = 0;
    bool typHinten = false;  ///< `name: typ` (Python)
    bool nichtNull = false;  ///< nur Tabelle: die Spalte ist NOT NULL (auch als Teil des Primaerschluessels)
};

/// Eine Funktion, die zu einem Typ gehoert. Hat sie einen Rumpf, steht sie
/// zusaetzlich unter Einheit::funktionen (mit dem Typ als Bereich).
struct Methode
{
    std::string name;
    std::string rueckgabe;  ///< leer bei Konstruktor und Destruktor
    std::vector<Parameter> parameter;
    Sichtbarkeit sicht = Sichtbarkeit::Keine;
    bool virtuell = false;
    bool statisch = false;
    bool abstrakt = false;  ///< `= 0`
    int zeile = 0;

    [[nodiscard]] std::string signatur() const { return modell::signatur(name, parameter); }
};

/// Was die Datenbank ausser Tabellen kennt (SQL).
enum class ObjektArt
{
    View,
    Trigger,
    Funktion,
    Prozedur,
};

/// Eine View, ein Trigger, eine Funktion, eine Prozedur der Datenbank: was sie liest, schreibt, ruft.
struct Datenbankobjekt
{
    ObjektArt art = ObjektArt::View;
    std::string name;  ///< wie Typ::name einer Tabelle
    int zeile = 0;
    int bisZeile = 0;
    std::vector<Zugriff> zugriffe;
    std::string tabelle;   ///< nur Trigger: die Tabelle, an der er haengt (`ON t`)
    std::string ereignis;  ///< nur Trigger: wann er laeuft, wie in der Quelle (`AFTER INSERT OR UPDATE`)
};

// ---------------------------------------------------------------------------
// SQL: Abfragen (Entscheide Q1-Q5) — vom Leser schon zerlegt, damit die Ableitung kein SQL kennt
// ---------------------------------------------------------------------------

/// "Kein Block" einer Abfrage.
inline constexpr std::size_t kKeinBlock = static_cast<std::size_t>(-1);

/// Eine Spalte, wie die Abfrage sie nennt: `f.title` (qualifizierer `f`), `title` (ohne).
struct Spaltenbezug
{
    std::string qualifizierer;  ///< Alias oder Name der Quelle, wie in der Quelle; leer = ohne
    std::string spalte;

    friend bool operator==(const Spaltenbezug&, const Spaltenbezug&) = default;
};

/// Ein Teil einer Bedingung (`ON`, `WHERE`, `HAVING`), an den aeussersten `AND` zerlegt.
struct Bedingungsteil
{
    std::string text;                    ///< wie in der Quelle; eine Unterabfrage darin als `(…)`
    std::vector<Spaltenbezug> bezuege;   ///< die Spalten, die er nennt (ohne die seiner Unterabfrage)
    std::size_t unterabfrage = kKeinBlock;  ///< ein Block, den er befragt: `EXISTS (…)`, `x IN (…)`, `x = (…)`
    std::string wie;                     ///< dann wie: `exists`, `not exists`, `in`, `not in`, `=`, `>` …
};

/// Woher ein Block liest.
enum class QuellArt
{
    Tabelle,     ///< ein Name: Tabelle, View oder ein Name eines WITH
    Funktion,    ///< eine Funktion, die eine Tabelle liefert (`generate_series(…)`, `LATERAL VIEW explode(…)`)
    Abgeleitet,  ///< eine Unterabfrage in FROM (`(SELECT …) t`)
    Werte,       ///< `VALUES (…), (…)`
};

/// Eine Quelle in FROM und wie sie zu den vorigen kommt.
struct AbfrageQuelle
{
    QuellArt art = QuellArt::Tabelle;
    std::string name;   ///< Tabelle oder Funktion, wie in der Quelle (`public.film`); bei Abgeleitet leer
    std::string alias;  ///< leer = keiner
    std::size_t block = kKeinBlock;  ///< Abgeleitet: ihr Block
    /// Wie sie dazukommt: leer (die erste), `,` (Komma), `inner`, `left`, `right`, `full`, `cross`,
    /// `natural`, `natural left` …, `semi`, `anti`, `apply`, `outer apply`, `merge` (MERGE … USING)
    std::string verbindung;
    std::vector<std::string> mit;          ///< `USING (a, b)`: die Spalten
    std::vector<Bedingungsteil> bedingung;  ///< `ON …`, zerlegt
    int zeile = 0;

    /// Wie sie in einer Bedingung genannt wird: der Alias, sonst der letzte Teil des Namens.
    [[nodiscard]] std::string kennung() const;
};

/// Was ein Block fuer seinen Eltern ist.
enum class BlockRolle
{
    Haupt,       ///< die Abfrage selbst (bloecke[0])
    Mit,         ///< ein Name eines WITH (`titel` = der Name)
    Abgeleitet,  ///< eine Unterabfrage in FROM (`titel` = ihr Alias)
    Bedingung,   ///< eine Unterabfrage in einer Bedingung (der Teil nennt sie)
    Auswahl,     ///< eine Unterabfrage in der Liste von SELECT
    Zweig,       ///< ein Zweig von UNION / INTERSECT / EXCEPT
};

/// Ein SELECT — oder, mit `zweige`, eine Mengenoperation ueber Bloecke.
struct AbfrageBlock
{
    BlockRolle rolle = BlockRolle::Haupt;
    std::string titel;
    std::size_t eltern = kKeinBlock;
    std::vector<AbfrageQuelle> quellen;
    std::vector<Bedingungsteil> wo;      ///< WHERE, zerlegt
    std::vector<Bedingungsteil> haben;   ///< HAVING, zerlegt
    std::string auswahl;                 ///< die Liste von SELECT, wie in der Quelle (Unterabfragen `(…)`)
    std::string gruppe, ordnung, grenze, fenster;  ///< GROUP BY, ORDER BY, LIMIT/OFFSET/FETCH/TOP, WINDOW/QUALIFY
    std::vector<Spaltenbezug> bezuege;   ///< jede Spalte, die der Block nennt (SELECT, ON, WHERE, GROUP …)
    std::vector<std::string> ausgabe;    ///< die Namen seiner Spalten nach aussen (Alias, sonst die Spalte)
    std::vector<std::string> aliase;     ///< davon die, die ein Alias einfuehrt (`AS n`, `count(*) n`)
    std::vector<std::size_t> zweige;     ///< eine Mengenoperation: ihre Zweige
    std::string mengen;                  ///< dann welche: `UNION`, `UNION ALL`, `INTERSECT`, `EXCEPT`
    int zeile = 0;
};

/// Eine Abfrage: eine Anweisung mit SELECT (auch der SELECT-Teil eines INSERT, CREATE … AS, UPDATE … FROM,
/// DELETE … USING, MERGE … USING).
struct Abfrage
{
    std::string name;  ///< die View; `routine@zeile`; `query@zeile` (im Projekt: `funktion@zeile`)
    int zeile = 0;
    int bisZeile = 0;
    std::vector<AbfrageBlock> bloecke;  ///< [0] ist die Abfrage selbst
    std::string ziel;       ///< wohin sie schreibt; leer = nirgends
    std::string schreibt;   ///< dann wie: `insert into`, `create table`, `create view`, `update`, `delete`, `merge into`, `select into`
    /// Nur eine Anweisung mit Ziel auf oberster Ebene einer `.sql`: was sie liest, schreibt, ruft — sie ist dann
    /// selbst ein Knoten in *Data dependencies* (Entscheid C2). Sonst leer: die Zugriffe traegt ihr Besitzer
    /// (View, Routine, Funktion des Host-Codes, Zelle eines Notebooks).
    std::vector<Zugriff> zugriffe;
};

/// Ein Fremdschluessel (SQL `REFERENCES`, `FOREIGN KEY`): Felder eines Typs nennen Zeilen eines anderen.
struct Verweis
{
    std::vector<std::string> felder;      ///< hier, in der Reihenfolge der Quelle
    std::string ziel;                     ///< der Typ, auf den verwiesen wird (Typ::name, wo der Leser ihn kennt)
    std::vector<std::string> zielFelder;  ///< dort; leer = sein Primaerschluessel
    std::string name;                     ///< `CONSTRAINT name`; leer = keiner
    int zeile = 0;
};

struct Typ
{
    TypArt art = TypArt::Struktur;
    std::string name;     ///< ohne Bereich; bei einer Tabelle mit Schema, wie in der Quelle (`public.actor`)
    std::string bereich;  ///< Namensraum und umgebender Typ; leer in C
    std::vector<Feld> felder;
    std::vector<Methode> methoden;
    std::vector<std::string> basen;  ///< wovon der Typ erbt (C: leer); bei einer Schnittstelle: extends
    std::vector<std::string> realisiert;  ///< welche Schnittstellen er umsetzt (`implements`)
    std::vector<Verweis> verweise;        ///< nur Tabelle: ihre Fremdschluessel
    std::vector<std::string> primaer;     ///< nur Tabelle: die Spalten ihres Primaerschluessels
    /// nur Tabelle: je UNIQUE (Spalte, Bedingung, `CREATE UNIQUE INDEX` ohne WHERE) seine Spalten
    std::vector<std::vector<std::string>> eindeutig;
    int zeile = 0;

    [[nodiscard]] std::string vollerName() const { return modell::vollerName(bereich, name); }
};

/// Ein weiterer Name fuer einen Typ (typedef, using).
struct Alias
{
    std::string name;
    std::string ziel;
    int zeile = 0;
};

/// Der Anfangswert einer Variablen: ein Wert oder eine Liste `{ … }`, deren Teile einen Bezeichner
/// tragen koennen (`[ROT] = …` einen Index, `.feld = …` ein Glied).
struct Initialisierer
{
    std::string index;  ///< `[ROT] =` -> `ROT`; leer = nach Reihenfolge
    std::string glied;  ///< `.feld =` -> `feld`
    std::string text;   ///< wie er in der Quelle steht (auch eine Liste, als Ganzes)
    bool liste = false;
    std::vector<Initialisierer> teile;  ///< nur Liste
    int zeile = 0;
};

struct Variable
{
    std::string typ;
    std::string name;
    std::string zusatz;
    int zeile = 0;
    /// Nur bei `const`: der Anfangswert — fuer Nachschlagetabellen.
    std::optional<Initialisierer> anfang;
};

struct Einbindung
{
    std::string ziel;
    bool system = false;  ///< `<x>` statt `"x"`
    int zeile = 0;
};

struct Makro
{
    std::string name;
    std::string text;  ///< bei einem Makro mit Parametern beginnt er mit `(`
    int zeile = 0;
};

/// Ein `#undef NAME`, wo der Leser den Text sieht (nicht in einem Zweig, der nicht gilt).
struct Aufhebung
{
    std::string name;
    int zeile = 0;
};

// ---------------------------------------------------------------------------
// Spark: ein Plan aus `explain()` (Spark Schicht 2, Entscheide P2-1 bis P2-4) — der Text, wie Spark ihn schreibt,
// als Baum; was die Operatoren bedeuten, weiss erst die Ableitung (diagramm/Plan.hpp)
// ---------------------------------------------------------------------------

/// Ein Operator eines Plans, eine Zeile des Baums.
struct PlanOperator
{
    std::string name;       ///< das erste Wort: `BroadcastHashJoin`, `Scan`, `Exchange`, `'Project`
    std::string argumente;  ///< was im Baum dahinter steht (`Inner BuildRight`, `hashpartitioning(k#1, 200), …`)
    int nummer = 0;         ///< `(n)` in `formatted`; 0 = keine
    /// Die Stufe von WholeStageCodegen, in der er laeuft (`*(3)`, `[codegen id : 3]`, `WholeStageCodegen (3)`);
    /// 0 = keine; -1 = ja, ohne Kennung (`*Project`).
    int codegen = 0;
    /// Worauf er sich bezieht: `#3` (simplified) bzw. die Nummer aus `Reuses operator id: 49`; leer = nichts.
    std::string verweis;
    std::vector<std::pair<std::string, std::string>> angaben;  ///< `formatted`: `Output [3]`, `Condition`, `Arguments` …
    std::vector<std::size_t> kinder;  ///< in der Reihenfolge des Texts (links vor rechts)
    /// Ein inneres Kind seines Eltern (`:  +- Subquery …` zwei Stufen tiefer): eine Unterabfrage in dessen Ausdruck.
    bool innen = false;
    int zeile = 0;  ///< im Text des Plans (der Datei, der Ausgabe)
};

/// Eine Unterabfrage aus dem Abschnitt `===== Subqueries =====` (`formatted`): ihr Baum haengt an einem Operator.
struct PlanUnterabfrage
{
    int nummer = 0;                   ///< `Subquery:1`
    int gastgeber = 0;                ///< `Hosting operator id = 4` (PlanOperator::nummer)
    std::string ausdruck;             ///< `Hosting Expression = …`
    std::size_t wurzel = kKeinBlock;  ///< ihr Baum (Stelle in Planbaum::operatoren); kKeinBlock = ohne eigenen Baum
    int zeile = 0;
};

/// Ein Abschnitt eines Plans: `Physical Plan`, `Optimized Logical Plan` …
struct Planbaum
{
    std::string art;  ///< wie in `== … ==`; ohne Kopf (simplified) `Physical Plan`
    std::vector<PlanOperator> operatoren;  ///< [0] ist die Wurzel; dahinter die Wurzeln der Unterabfragen
    std::vector<PlanUnterabfrage> unterabfragen;
    int zeile = 0;  ///< der Kopf `== … ==` bzw. die erste Zeile
};

struct Plan
{
    std::string name;  ///< vom Projekt: `funktion@zeile`, `<cell 3>@zeile`, der Dateiname, `datei@zeile`
    /// `simple`, `extended`, `formatted`, `simplified` (der Baum aus Einrueckung, wie Sparks Pruefdateien),
    /// `cost`, `codegen`
    std::string modus;
    std::vector<Planbaum> baeume;  ///< in der Reihenfolge des Texts
    bool adaptiv = false;          ///< `AdaptiveSparkPlan`
    bool endgueltig = false;       ///< dann `isFinalPlan=true`: gezeigt ist der endgueltige
    int zeile = 0;                 ///< seine erste Zeile im Text
    /// Steht er in der Ausgabe einer Zelle: die Zeile des `explain()`, zu dem er gehoert, im Text der Datei;
    /// 0 = er steht in der Datei selbst (dann sind die Zeilen der Operatoren Zeilen der Datei).
    int aufruf = 0;
};

// ---------------------------------------------------------------------------
// Spark: ein Lauf aus seinem Event-Log (Spark Schicht 3, Entscheide L1-L7) — was der Log sagt, ohne Deutung;
// Zeiten in Millisekunden seit 1970, wie Spark sie schreibt
// ---------------------------------------------------------------------------

struct LaufExecutor
{
    std::string id;    ///< `6`, `driver`
    std::string host;
    int kerne = 0;     ///< `Total Cores`; 0 = der Log nennt sie nicht (der Treiber im lokalen Modus)
    long long hinzu = 0;
    long long entfernt = 0;  ///< 0 = nicht entfernt
    std::string grund;       ///< `Removed Reason`
    int zeile = 0;           ///< im Log
};

struct LaufTask
{
    long long id = 0;
    int versuch = 0;
    int stage = 0;
    int stageVersuch = 0;
    std::string executor;
    long long start = 0;
    long long ende = 0;     ///< 0 = kein Ende im Log
    std::string ergebnis;   ///< `Success`, `ExceptionFailure`, `TaskKilled` …; leer = kein Ende
    std::string grund;      ///< bei einem Fehlschlag: was der Log dazu sagt
    bool spekulativ = false;
    long long laufzeit = 0;           ///< `Executor Run Time`
    long long gc = 0;                 ///< `JVM GC Time`
    long long shuffleGelesen = 0;     ///< Bytes, entfernt und lokal
    long long shuffleGeschrieben = 0; ///< Bytes
    long long spillSpeicher = 0;      ///< `Memory Bytes Spilled`
    long long spillPlatte = 0;        ///< `Disk Bytes Spilled`
    long long spitze = 0;             ///< `Peak Execution Memory` bzw. `internal.metrics.peakExecutionMemory`
    long long eingabe = 0;            ///< `Input Metrics` Bytes Read
    long long ausgabe = 0;            ///< `Output Metrics` Bytes Written
    int zeile = 0;                    ///< im Log: das Ende, sonst der Start
};

struct LaufStage
{
    int id = 0;
    int versuch = 0;
    std::string name;     ///< `Stage Name`: die Aufrufstelle, `collect at kmeans.scala:62`
    int tasks = 0;        ///< `Number of Tasks`
    long long eingereicht = 0;
    /// Der Log nennt keine `Submission Time` (Spark 2 schreibt sie erst beim Abschluss): `eingereicht` ist der Start ihres
    /// ersten Tasks, sonst ihres Jobs.
    bool eingereichtGeschaetzt = false;
    long long beendet = 0;  ///< 0 = nicht beendet
    std::string fehler;     ///< `Failure Reason`; leer = keiner
    std::vector<int> eltern;
    std::vector<std::string> aufrufe;  ///< die `Callsite` ihrer RDDs, je eine einmal
    int zeile = 0;
};

struct LaufJob
{
    int id = 0;
    long long start = 0;
    long long ende = 0;     ///< 0 = kein Ende im Log
    std::vector<int> stages;
    std::string ergebnis;   ///< `JobSucceeded`, `JobFailed`; leer = kein Ende
    std::string sql;        ///< `spark.sql.execution.id`; leer = keine
    int zeile = 0;
};

struct Lauf
{
    std::string name;     ///< `App Name`
    std::string id;       ///< `App ID`
    std::string version;  ///< `Spark Version`
    std::string master;   ///< `spark.master`
    long long start = 0;
    long long ende = 0;   ///< 0 = kein ApplicationEnd
    int taskCpus = 1;     ///< `spark.task.cpus`
    std::vector<LaufExecutor> executoren;  ///< in der Reihenfolge, in der sie dazukommen
    std::vector<LaufJob> jobs;
    std::vector<LaufStage> stages;         ///< je Stage und Versuch
    std::vector<LaufTask> tasks;           ///< je Task und Versuch, in der Reihenfolge des Starts
    long long letzte = 0;                  ///< der letzte Zeitstempel im Log
};

/// Eine Uebersetzungseinheit: eine Quelldatei.
struct Einheit
{
    std::string name;
    std::vector<Einbindung> einbindungen;
    std::vector<Makro> makros;
    std::vector<Aufhebung> aufhebungen;
    std::vector<Typ> typen;
    std::vector<Alias> aliase;
    std::vector<Variable> variablen;
    std::vector<Funktion> funktionen;
    /// SQL: Views, Trigger, Funktionen, Prozeduren der Datenbank.
    std::vector<Datenbankobjekt> objekte;
    /// SQL: was die Anweisungen ausserhalb der Objekte lesen, schreiben, rufen (ein Skript; bei SQL in
    /// Zeichenketten die Abfrage selbst).
    std::vector<Zugriff> zugriffe;
    /// SQL: jede Abfrage der Datei, in der Reihenfolge der Quelle (auch in Views und Routinen).
    std::vector<Abfrage> abfragen;
    /// Spark: die Plaene aus `explain()` — einer Plan-Datei oder der Ausgaben eines Notebooks.
    std::vector<Plan> plaene;
    /// Spark: der Lauf eines Event-Logs (hoechstens einer je Datei).
    std::vector<Lauf> laeufe;
};

// ---------------------------------------------------------------------------
// Bedingte Uebersetzung: wonach gefragt wird, und die Ketten #if / #elif / #else
// (der Leser liefert sie in sprache::Ergebnis, nicht beim Lesen aller Zweige)
// ---------------------------------------------------------------------------

/// "Kein Zweig" (Weiche::geltend, Weiche::elternZweig) und "keine Weiche" (Weiche::eltern).
inline constexpr std::size_t kKeinZweig = static_cast<std::size_t>(-1);

/// Ein Name, nach dem eine Bedingung der Uebersetzung fragt — was man von aussen schalten kann.
struct Schalter
{
    std::string name;
    bool definiert = false;  ///< dort, wo zuerst nach ihm gefragt wird
    std::string wert;        ///< dort sein Wert, wenn er definiert ist (`#define X` ohne Wert: leer)
    /// Er wird verglichen oder verrechnet (`X >= 2`), nicht nur gefragt, ob er gilt: er braucht einen Wert.
    bool zahl = false;
    /// Die Datei definiert ihn selbst, bevor nach ihm gefragt wird (die Zeile): eine
    /// Definition von aussen aendert dann nichts. 0 = nicht.
    int inDatei = 0;
    bool vorgegeben = false;  ///< die Sprache definiert ihn (`__cplusplus`)
};

/// Was fuer einen Zweig gelten muss: ein Name definiert (mit `wert`; leer = gleich mit
/// welchem) oder nicht definiert.
struct Forderung
{
    std::string name;
    bool definiert = true;
    std::string wert;
};

/// Die Bedingung eines Zweigs der bedingten Uebersetzung als Baum: Und, Oder, Nicht und Atome.
/// Ein Atom laesst sich an einem Namen einstellen (`defined(X)`, `X > 50`, `X` = `X != 0`), ist
/// eine Zahl (`#if 0`), oder ist Text, wo der Ausdruck sich nicht nach Namen zerlegen laesst
/// (`F(2) > 1`, `A + B > 3`, `A == B`).
struct Vorbedingung
{
    enum class Art
    {
        Definiert,  ///< name
        Vergleich,  ///< name op wert: `X > 50`; der Name steht links
        Konstante,  ///< wert: `0` oder `1`
        Text,       ///< text, wie in der Quelle
        Nicht,      ///< teile[0]
        Und,        ///< teile
        Oder,       ///< teile
    };
    Art art = Art::Text;
    std::string name;
    std::string op;  ///< `==`, `!=`, `<`, `<=`, `>`, `>=`
    std::string wert;
    std::string text;
    std::vector<Vorbedingung> teile;
};

/// Eine Kette `#if` / `#elif` / `#else`, in der Reihenfolge des Texts. Include-Waechter
/// (`#ifndef K` mit `#define K` gleich darauf) zaehlen nicht.
struct Weiche
{
    int zeile = 0;  ///< des #if
    std::size_t eltern = kKeinZweig;       ///< die Weiche, in deren Zweig sie steht
    std::size_t elternZweig = kKeinZweig;  ///< und welcher es ist
    /// Je Zweig seine Bedingung, kurz wie bei AnweisungArt::Zweig (`A`,
    /// `defined(A)`, `else`); ein fehlendes `#else` steht nicht da.
    std::vector<std::string> zweige;
    /// Der Zweig, der gilt; zweige.size() = das fehlende `#else`; kKeinZweig = keiner
    /// (die Weiche steht in einem Zweig, der nicht gilt).
    std::size_t geltend = kKeinZweig;
    /// Je Zweig — und zuletzt fuer das fehlende `#else`, wenn es fehlt —, was sich an
    /// den Definitionen aendern muss, damit er gilt; leer = er gilt schon. nullopt =
    /// aus Namen nicht abzuleiten (eine Bedingung wie `F(2) > 1`, ein Name, den die
    /// Datei selbst definiert, Forderungen, die sich widersprechen).
    std::vector<std::optional<std::vector<Forderung>>> wahl;
    /// Je Zweig (wie `zweige`, ohne das fehlende `#else`) seine Bedingung als Baum.
    std::vector<Vorbedingung> baeume;
    /// Je Zweig (wie `zweige`) die Zeile seines `#if`, `#elif`, `#else`.
    std::vector<int> zweigZeilen;
    int endZeile = 0;  ///< des `#endif`; 0 = es fehlt
};

}  // namespace c2d::modell
