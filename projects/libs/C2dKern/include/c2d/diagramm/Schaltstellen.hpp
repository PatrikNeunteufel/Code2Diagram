/**
 * @file Schaltstellen.hpp
 * @brief Wo schaltet ein Name im Projekt: wer ihn festlegt, welche Zweige `#if` nach ihm fragen
 *
 * Zwei Tabellen. `Schaltstellen`: fuer einen Namen oben, wer ihn festlegt (Projektdatei,
 * Abschnitte, `#define`/`#undef` der Dateien, von aussen), darunter je Zweig, dessen absolute
 * Forderung ihn betrifft (wie in der Konfigurationstabelle, auch innere Ketten), eine Zeile —
 * mit dem Stand des Namens in dieser Datei und wie viele Zeilen der Zweig umfasst. Nennt die
 * eigene Kette den Namen nicht (er kommt nur ueber eine umgebende), steht diese unter `via`.
 * `Schalteruebersicht`: je Name, nach dem eine Datei fragt, eine Zeile.
 * Abgeleitet wird im Projekt (projekt/Schaltstellen.hpp); hier steht nur, was gezeichnet wird.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace c2d::diagramm
{

struct Schaltstellen
{
    /// Eine Stelle, die den Namen festlegt.
    struct Setzung
    {
        std::string herkunft;  ///< `project`, `section <ordner>`, `#define`, `#undef`, `outside`, `language`
        std::string pfad;      ///< die Datei; bei Projektdatei und Abschnitt `code2diagram.json`, sonst leer
        int zeile = 0;         ///< 0 = ohne Zeile
        std::string wert;      ///< `NAME=wert`, `!NAME`, der Text des `#define`
    };
    /// Ein Zweig einer Kette, der etwas von dem Namen fordert (je Alternative eine Zeile).
    struct Zeile
    {
        std::string pfad;
        std::size_t datei = 0;       ///< Stelle in Projekt::dateien
        std::size_t weiche = 0;      ///< Index in den Weichen der Datei
        std::size_t zweigIndex = 0;  ///< in modell::Weiche (zweige.size() = das fehlende `#else`)
        int quellZeile = 0;          ///< des `#if`, `#elif`, `#else` (beim fehlenden: des `#endif`)
        std::string zweig;           ///< `#if defined(A)`, `#elif X > 2`, `#else`, `(no #else)`; weitere Alternative: `or`
        std::string forderung;       ///< `def`, `!def`, `> 50`, `–` (in dieser Alternative gleich), `never`; Text: `?`
        std::string stand;           ///< `def (project)`, `= 100 (header x.h)`, `undef`
        std::string bedingung;       ///< die Bedingung des Zweigs, wie in der Quelle (Tooltip; bei `text` die Zeile)
        /// Eine innere Kette, die selbst nicht nach dem Namen fragt: der naechste umgebende Zweig, der
        /// es tut (`#if defined(A), line 60`); leer = die Kette fragt selbst.
        std::string via;
        int von = 0;                 ///< die Zeilen des Zweigs: von der Direktive
        int bis = 0;                 ///< bis zur naechsten (`#elif`, `#else`, `#endif`); 0 = unbekannt
        bool geltend = false;
        bool nie = false;
        bool text = false;           ///< nicht nach Namen zerlegt; der Text nennt den Namen
        bool vorgegeben = false;     ///< die Sprache definiert ihn (`__cplusplus`)
        int inDatei = 0;             ///< die Datei definiert ihn selbst, vor der Frage (Zeile); 0 = nicht
    };
    std::string name;
    std::string titel;               ///< `Where is X switched? 5 branches in 3 files`
    std::vector<Setzung> gesetzt;
    std::vector<Zeile> zeilen;       ///< nach Pfad, dann Zeile
    std::vector<std::string> hinweise;
};

struct Schalteruebersicht
{
    struct Zeile
    {
        std::string name;
        std::size_t dateien = 0;  ///< die nach ihm fragen
        std::size_t ketten = 0;   ///< deren Zweige etwas von ihm fordern
        std::string stand;        ///< `def`, `= 100`, `undef`, `differs`
        bool abweichend = false;  ///< nicht in jeder Datei gleich
        std::string herkunft;     ///< wer ihn festlegt: `project, header`, `file`, leer = niemand
        bool vorgegeben = false;  ///< die Sprache definiert ihn
    };
    std::string titel;            ///< `#if names: 82 in 92 files`
    std::vector<Zeile> zeilen;    ///< nach Namen
    std::vector<std::string> hinweise;
};

/// Die Zeilen, die ein Zweig umfasst (ohne die Direktiven); 0, wenn `bis` unbekannt ist.
[[nodiscard]] inline int zeilenDazwischen(const Schaltstellen::Zeile& z)
{
    return z.bis > z.von ? z.bis - z.von - 1 : 0;
}

}  // namespace c2d::diagramm
