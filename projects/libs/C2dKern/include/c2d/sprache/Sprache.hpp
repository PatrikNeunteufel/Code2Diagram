/**
 * @file Sprache.hpp
 * @brief Eine Sprache: ihre Sprachtabelle und ihr Leser (Text -> Codemodell)
 *
 * Die allgemeine Basis ist fuer alle gleich — Lexer (lexer/Lexer.hpp),
 * Codemodell (modell/Code.hpp), Meldungen. Eine Sprache traegt nur ihre
 * Einzelheiten bei: eine Tabelle und einen Leser in rekursivem Abstieg, beides
 * in ihrem Ordner unter src/sprache/. Eine neue Sprache anmelden: in
 * Sprache.cpp in die Liste eintragen.
 */
#pragma once

#include "c2d/lexer/Lexer.hpp"
#include "c2d/modell/Code.hpp"
#include "c2d/quelle/Meldung.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::sprache
{

/// Eine Definition von aussen, wie `-D NAME=wert` beim Compiler — oder, mit
/// `definiert = false`, eine Aufhebung wie `-U NAME`. In einer Liste gilt fuer
/// einen Namen der spaetere Eintrag.
struct Definition
{
    std::string name;
    std::string wert = "1";  ///< ohne Bedeutung bei einer Aufhebung
    bool definiert = true;
};

/// `NAME` oder `NAME=wert` als Definition (ohne Wert gilt 1), `!NAME` als Aufhebung.
/// Ein leerer Name — auch `!NAME=wert` —: kein gueltiger Text.
[[nodiscard]] Definition definitionAus(std::string_view text);

/// Eine Definition, wie `definitionAus` sie liest: `NAME` (Wert 1), `NAME=wert` oder `!NAME`.
[[nodiscard]] std::string alsText(const Definition& definition);

/// Wie gelesen wird. Was eine Sprache nicht kennt, uebergeht sie.
struct LeseOptionen
{
    /// Bedingte Uebersetzung (#if, #ifdef ...) nicht auswerten, sondern alle
    /// Zweige lesen — als stuende der Code aller Varianten hintereinander.
    bool alleZweige = false;
    /// Nur beim Auswerten (nicht mit alleZweige): ein #if im Rumpf einer
    /// Funktion, dessen Zweige je ganze Anweisungen enthalten, wird nicht
    /// ausgewertet, sondern als Varianten gelesen (AnweisungArt::Varianten) —
    /// jeder Zweig, und welcher gilt. Ein #if im Rumpf, das die Bedingung nicht
    /// erfuellt, wird ausgewertet und bekommt einen Hinweis.
    bool varianten = false;
    /// Wie varianten, aber nur der Zweig, der gilt, und nur, wenn er etwas
    /// enthaelt: als AnweisungArt::Bedingt — ausgewertet, mit der Spur der
    /// Bedingung (der Ablauf umrahmt ihn). Ohne Wirkung neben varianten.
    bool bereiche = false;
    /// Gelten zusaetzlich zu den #define der Datei, vom Anfang der Datei an.
    std::vector<Definition> definitionen;
};

// Die Beschreibung der bedingten Uebersetzung gehoert zum Codemodell (modell/Code.hpp):
// sie ist fuer alle Sprachen gleich, und die Ableitung liest sie.
using modell::Forderung;
using modell::kKeinZweig;
using modell::Schalter;
using modell::Weiche;

/// Die Definitionen von aussen (`aussen`, etwa das Feld Defines), so geaendert, dass
/// die Forderungen gelten. Ein Name, der definiert sein soll, wird gesetzt oder
/// ersetzt (`wert` leer: bleibt er, wie er ist, sonst ohne Wert); einer, der es nicht
/// sein soll, wird entfernt — und als `!NAME` aufgehoben, wo er darunter definiert
/// ist. `darunter` sind die Definitionen, die vor denen von aussen gelten
/// (Projektdatei, Abschnitte, Koepfe: projekt::Datei::definitionen); fehlt ein Name
/// dort, sagt `schalter`, wie es um ihn steht. nullopt = das geht nicht: die Datei
/// oder die Sprache definiert ihn selbst.
[[nodiscard]] std::optional<std::vector<Definition>> mitForderungen(std::vector<Definition> aussen,
                                                                   const std::vector<Forderung>& forderungen,
                                                                   const std::vector<Schalter>& schalter,
                                                                   const std::vector<Definition>& darunter = {});

struct Ergebnis
{
    modell::Einheit einheit;
    std::vector<quelle::Meldung> meldungen;
    /// Wonach die Bedingungen fragen, in der Reihenfolge des Texts, jeder einmal
    /// (nicht beim Lesen aller Zweige).
    std::vector<Schalter> schalter;
    /// Die Ketten `#if` / `#elif` / `#else` (nicht beim Lesen aller Zweige).
    std::vector<Weiche> weichen;
    /// Namen, nach denen eine Bedingung gefragt hat und die nirgends definiert
    /// sind — sie galten als nicht definiert. Wer eine andere Variante sehen
    /// will, gibt sie in LeseOptionen::definitionen an. In der Reihenfolge des
    /// Texts, jeder einmal.
    std::vector<std::string> unbekannteNamen;
};

class Sprache
{
public:
    virtual ~Sprache() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    /// Dateiendungen ohne Punkt, klein geschrieben.
    [[nodiscard]] virtual std::span<const std::string_view> endungen() const = 0;
    [[nodiscard]] virtual const lexer::Sprachtabelle& tabelle() const = 0;

    /// Beanspruchen mehrere Sprachen eine Endung (`h`: C und C++): ist dieser
    /// Text meine Sprache? Wer nichts erkennt, ueberlaesst die Datei der Sprache,
    /// die zuerst angemeldet ist.
    [[nodiscard]] virtual bool erkennt(std::string_view /*text*/) const { return false; }

    /// Kennt die Sprache bedingte Uebersetzung (`#if`)? Sonst gibt es fuer ihre Dateien keine
    /// Varianten, Rahmen, `#if`-Konfiguration und keine Namen, nach denen gefragt wird.
    [[nodiscard]] virtual bool hatPraeprozessor() const { return false; }

    /// Sind ihre Einbindungen Dateien, deren `#define` gelten (`#include` der C-Familie)? Sonst
    /// nennen sie Module oder Namensraeume (Python `import`, C# `using`) und das Projekt sucht
    /// keine Koepfe.
    [[nodiscard]] virtual bool bindetKoepfeEin() const { return false; }

    /// Liest den Text. Was der Leser nicht versteht, uebergeht er mit einer
    /// Meldung — das Ergebnis ist immer brauchbar, nie leer wegen eines Fehlers.
    [[nodiscard]] virtual Ergebnis lies(std::string_view text, std::string_view einheitName,
                                        const LeseOptionen& optionen) const = 0;

    /// Mit den Vorgaben der LeseOptionen.
    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName) const
    {
        return lies(text, einheitName, LeseOptionen{});
    }
};

/// Alle angemeldeten Sprachen.
[[nodiscard]] std::span<const Sprache* const> sprachen();

/// Die Sprache zu einem Dateinamen, nach seiner Endung; nullptr = keine.
/// Beanspruchen mehrere die Endung, entscheidet `inhalt` (Sprache::erkennt),
/// sonst die zuerst angemeldete.
[[nodiscard]] const Sprache* spracheFuer(std::string_view dateiname, std::string_view inhalt = {});

/// C (C11): Funktionen mit Rumpf, struct/union/enum, typedef, globale
/// Variablen, #include und #define. Bedingte Uebersetzung wird ausgewertet
/// (LeseOptionen); Makros werden nicht ersetzt.
[[nodiscard]] const Sprache& c();

/// C++ (C++20): was C hat, dazu Namensraeume, Klassen mit Methoden, Basen und
/// Sichtbarkeit, Konstruktoren und Operatoren, using, try/catch/throw,
/// range-for. Schablonen werden uebergangen, nicht verstanden.
[[nodiscard]] const Sprache& cpp();

/// Python (3.12): Funktionen, auch in Funktionen (Funktion::aussen) und Methoden; Klassen mit
/// Basen, Feldern (im Rumpf und `self.x =` in Methoden), Aufzaehlungen (Basis `Enum` …);
/// if/elif/else, while und for (mit else), try/except/else/finally, with, match/case,
/// return, raise, break, continue; import als Einbindung. Was auf oberster Ebene ablaeuft,
/// ist die Funktion `<module>`. Kein Praeprozessor; LeseOptionen gelten nicht.
[[nodiscard]] const Sprache& python();

/// JavaScript (ES2023; `.js`, `.mjs`, `.cjs`): Funktionen, auch ohne Namen (Pfeil, `function`,
/// Methoden in Objekten) mit Block-Rumpf; Klassen mit Feldern, Methoden, get/set, `#privat`;
/// if/else, while, do, for (auch of/in), switch, try/catch/finally, break, continue, return,
/// throw, Marken; ein fehlendes `;` wie in JavaScript; import/export als Einbindung; was auf
/// oberster Ebene ablaeuft, ist `<module>`. Kein Praeprozessor; LeseOptionen gelten nicht.
[[nodiscard]] const Sprache& javascript();

/// TypeScript (`.ts`, `.mts`, `.cts`): was JavaScript hat, dazu Typen an Parametern und Gliedern,
/// interface (TypArt::Schnittstelle), type (Objekttyp: Struktur, sonst Alias), enum, namespace,
/// declare, Sichtbarkeit, abstract, implements, Felder aus `constructor(private x: T)`.
[[nodiscard]] const Sprache& typescript();

/// C# 12 (`.cs`): Namensraeume (auch file-scoped), class, struct, record, interface, enum, delegate;
/// Felder, Eigenschaften (`{ get; set; }`, Accessoren mit Rumpf als Funktionen `Name.get`),
/// Indexer, Ereignisse, Methoden, Konstruktoren, Operatoren, lokale Funktionen, Lambdas mit
/// Block-Rumpf; if, while, do, for, foreach, switch (auch als Ausdruck auf oberster Ebene: eine
/// Auswahl ohne Durchfallen), try/catch/when/finally, using/lock/fixed, yield; Anweisungen auf
/// oberster Ebene als `<Main>$`. `#if` wird ausgewertet wie in der C-Familie (ohne Varianten).
[[nodiscard]] const Sprache& csharp();

}  // namespace c2d::sprache
