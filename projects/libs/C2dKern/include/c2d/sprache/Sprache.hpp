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

/// Eine Definition von aussen, wie `-D NAME=wert` beim Compiler.
struct Definition
{
    std::string name;
    std::string wert = "1";
};

/// `NAME` oder `NAME=wert` als Definition; ohne Wert gilt 1. Ein leerer Name: kein gueltiger Text.
[[nodiscard]] Definition definitionAus(std::string_view text);

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

/// Eine Kette `#if` / `#elif` / `#else`, in der Reihenfolge des Texts. Include-Waechter
/// (`#ifndef K` mit `#define K` gleich darauf) zaehlen nicht.
struct Weiche
{
    int zeile = 0;  ///< des #if
    std::size_t eltern = kKeinZweig;       ///< die Weiche, in deren Zweig sie steht
    std::size_t elternZweig = kKeinZweig;  ///< und welcher es ist
    /// Je Zweig seine Bedingung, kurz wie bei modell::AnweisungArt::Zweig (`A`,
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
};

/// Die Definitionen von aussen (`aussen`, etwa das Feld Defines), so geaendert, dass
/// die Forderungen gelten: ein Name, der definiert sein soll, wird gesetzt oder
/// ersetzt (`wert` leer: bleibt er, wie er ist, sonst ohne Wert), einer, der es nicht
/// sein soll, entfernt. nullopt = das geht nicht: er ist nicht von dort definiert
/// (die Datei, die Sprache oder eine andere Quelle wie die Projektdatei) — `schalter`
/// sagt, wie es um ihn steht.
[[nodiscard]] std::optional<std::vector<Definition>> mitForderungen(std::vector<Definition> aussen,
                                                                   const std::vector<Forderung>& forderungen,
                                                                   const std::vector<Schalter>& schalter);

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

}  // namespace c2d::sprache
