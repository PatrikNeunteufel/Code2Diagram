// c2d — Code2Diagram fuer die Kommandozeile.
//
// Aufruf:  c2d <datei|ordner> [--liste] [--datei <pfad>] [--funktion <name>] [--einbetten <name>]...
//                             [--struktur] [-D NAME[=wert] | -D !NAME | -U NAME]...
//                             [--aufrufgraph [<funktion>] [--tiefe n] [--hoechstens n] [--aufrufer | --beide]
//                                            [--ohne-fremde]]
//                             [--alle-zweige | --varianten | --rahmen]
//                             [--format <name>] [--richtung TD|LR] [--roh] [--struktogramm]
//                             [--wahrheitstabelle [<zeile>]] [--entscheidungstabelle]
//                             [--zuordnung [<zeile>]] [--nachschlagetabellen] [--konfiguration]
//                             [--schaltet <name>] [--schalter] [--breite]
//
//   <ordner>       ein Projekt: alle Quelldateien darunter, mit code2diagram.json, wenn es sie gibt
//   ohne Auswahl   das Diagramm der ersten Funktion mit Rumpf; gibt es keine,
//                  das Strukturdiagramm
//   --liste        nennt, was die Datei (das Projekt: jede Datei) enthaelt (auch Automaten,
//                  Zuordnungen "mapping", konstante Tabellen "lookup", die Ketten #if "ifconfig"),
//                  und endet
//   --datei        im Projekt: die Datei, um die es geht (relativ zum Ordner)
//   --funktion     das Flussdiagramm dieser Funktion (Name allein oder mit Bereich); im
//                  Projekt muss sie eindeutig sein, sonst --datei
//   --einbetten    den Ablauf dieser gerufenen Funktion hinter ihren ersten Aufruf setzen,
//                  umrahmt — im Projekt auch aus einer anderen Datei
//   --struktur     das Diagramm der Datentypen (im Projekt: aller Dateien, mit --datei: einer)
//   --typ          die Datentypen, nur dieser Typ und seine Nachbarn in seiner Datei (im Projekt: --datei)
//   --automat      das Zustandsdiagramm des Automaten mit dieser Variable (auch nur ihr
//                  letztes Glied: Bus_State); --liste nennt die erkannten
//   --aufrufgraph  der Aufrufgraph ab dieser Funktion (wie --funktion; steht das Ziel
//                  davor, darf ihr Name gleich folgen); ohne Funktion der Graph der Datei
//                  (im Projekt: --datei)
//   --tiefe n      so viele Stufen ab der Funktion (Vorgabe 2), 0 = alle
//   --hoechstens n hoechstens so viele Funktionen (Vorgabe 300), 0 = unbegrenzt
//   --aufrufer     wer die Funktion ruft, statt was sie ruft; --beide: beides
//   --ohne-fremde  Aufrufe ohne Funktion im Projekt (Bibliothek, Makro) weglassen
//   -D NAME[=wert] gilt beim Auswerten von #if/#ifdef als definiert (ohne Wert: 1); mehrfach
//                  erlaubt; geht der Projektdatei und den Koepfen vor (nicht der Datei selbst)
//   -U NAME        gilt als nicht definiert, auch wo Projektdatei oder ein Kopf ihn
//                  definieren; gleich -D !NAME. Fuer einen Namen gilt die letzte Angabe
//   --alle-zweige  bedingte Uebersetzung nicht auswerten: alle Zweige lesen
//   --varianten    ein #if im Rumpf, das ganze Anweisungen schaltet, als Varianten zeigen
//                  (ein Sechseck, je Zweig eine Kante, je Zweig ein farbiger Rahmen); das
//                  uebrige wird ausgewertet
//   --rahmen       auswerten, aber umrahmen, was ein solches #if zuschaltet (je Bedingung
//                  eine Farbe)
//   --format       mermaid (Vorgabe), svg, dot, drawio oder csv (nur Wahrheitstabellen); was ein
//                  Format nicht kann, meldet es (Rueckgabe 1)
//   --roh          je Anweisung ein Knoten, nichts gebuendelt
//   --struktogramm mit --funktion: das Struktogramm (Nassi-Shneiderman) statt des Ablaufs; svg und
//                  drawio, Mermaid und DOT koennen es nicht (Meldung, Rueckgabe 1)
//   --wahrheitstabelle  mit --funktion: die Wahrheitstabelle (samt MC/DC) der Bedingung in dieser
//                  Zeile; ohne Zeile alle der Funktion untereinander. svg, drawio, csv
//   --entscheidungstabelle  mit --funktion: die Entscheidungstabelle (DIN 66241) der Funktion;
//                  svg, drawio, csv
//   --zuordnung    mit --funktion: die Zuordnungstabelle des switch in dieser Zeile (ohne Zeile: alle
//                  der Funktion, die Zuordnungen sind); svg, drawio, csv
//   --nachschlagetabellen  die konstanten Tabellen (const-Arrays) der Datei (im Projekt: --datei)
//   --konfiguration  je Kette #if der Datei, welcher Zweig bei welchen Definitionen gilt (im
//                  Projekt: --datei); svg, drawio, csv
//   --schaltet     wo dieser Name im Projekt schaltet: wer ihn festlegt (Projektdatei, Abschnitte,
//                  #define/#undef, von aussen) und je Zweig #if, der etwas von ihm fordert, eine Zeile
//                  mit dem Stand in der Datei; svg, drawio, csv
//   --schalter     je Name, nach dem eine Datei fragt, eine Zeile: Dateien, Ketten, Stand, wer ihn
//                  festlegt; svg, drawio, csv
//   --breite       der Breiten-Test an echtem Code: legt je Datei (im Projekt jede, oder --datei)
//                  jeden Ablauf, Automaten, die Datentypen und den Aufrufgraph der Datei an, nach
//                  unten und nach rechts, und nennt jede Anordnung, die mehr als
//                  kHoechstesQuerVerhaeltnis mal so breit ist, wie ihre breiteste Schicht braucht.
//                  Exit 3, wenn eine es ist
//
// Das Diagramm geht auf die Standardausgabe, Meldungen auf die Fehlerausgabe.
// Exit: 0 gut, 1 Aufruf falsch oder nichts zu zeichnen, 2 Datei oder Ordner nicht lesbar,
// 3 der Breiten-Test fand eine zu breite Anordnung.

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Entscheidungstabelle.hpp"
#include "c2d/diagramm/Wahrheitstabelle.hpp"
#include "c2d/diagramm/Zuordnung.hpp"
#include "c2d/projekt/Aufrufgraph.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/projekt/Schaltstellen.hpp"
#include "c2d/projekt/Typen.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace fs = std::filesystem;
using c2d::projekt::Fundstelle;
using c2d::projekt::Projekt;

fs::path alsWeg(const std::string& text)
{
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

std::string alsText(const fs::path& weg)
{
    const std::u8string u8 = weg.u8string();
    return {reinterpret_cast<const char*>(u8.data()), u8.size()};
}

int aufruf()
{
    std::cerr << "usage: c2d <file|folder> [--liste] [--datei <path>] [--funktion <name>] [--einbetten <name>]...\n"
                 "                         [--struktur [--typ <name>]] [--automat <variable>] [-D NAME[=value] | -D !NAME | -U NAME]...\n"
                 "                         [--aufrufgraph [<function>] [--tiefe n] [--hoechstens n] [--aufrufer | --beide]\n"
                 "                                        [--ohne-fremde]]\n"
                 "                         [--alle-zweige | --varianten | --rahmen]\n"
                 "                         [--format <name>] [--richtung TD|LR] [--roh] [--struktogramm]\n"
                 "                         [--wahrheitstabelle [<line>]] [--entscheidungstabelle]\n"
                 "                         [--zuordnung [<line>]] [--nachschlagetabellen] [--konfiguration]\n"
                 "                         [--schaltet <name>] [--schalter] [--breite]\n"
                 "formats:";
    for (const c2d::ausgabe::Format* f : c2d::ausgabe::formate()) std::cerr << ' ' << f->name();
    std::cerr << '\n';
    return 1;
}

std::string ort(const Projekt& p, const Fundstelle& f)
{
    return p.dateien[f.datei].pfad + ":" + std::to_string(c2d::projekt::funktion(p, f).zeile);
}

std::string orte(const Projekt& p, const std::vector<Fundstelle>& stellen)
{
    std::string aus;
    for (const Fundstelle& f : stellen) aus += (aus.empty() ? "" : ", ") + ort(p, f);
    return aus;
}

void liste(const c2d::projekt::Projekt& p, std::size_t i, const c2d::projekt::Typverzeichnis& typen)
{
    const c2d::projekt::Datei& d = p.dateien[i];
    const c2d::modell::Einheit& einheit = d.ergebnis.einheit;
    for (const c2d::modell::Funktion& f : einheit.funktionen)
        std::cout << (f.rumpf ? "function  " : "prototype ") << f.signatur() << (f.intern ? "  [static]" : "") << '\n';
    for (const c2d::modell::Typ& t : einheit.typen)
        std::cout << "type      " << t.vollerName() << " (" << t.felder.size() << " + " << t.methoden.size() << ")\n";
    for (const c2d::modell::Alias& a : einheit.aliase) std::cout << "alias     " << a.name << " = " << a.ziel << '\n';
    for (const c2d::modell::Variable& v : einheit.variablen)
        std::cout << "variable  " << v.typ << ' ' << v.name << v.zusatz << '\n';
    for (const c2d::modell::Makro& m : einheit.makros) std::cout << "macro     " << m.name << '\n';
    for (const c2d::modell::Einbindung& e : einheit.einbindungen) std::cout << "include   " << e.ziel << '\n';
    // wonach ein #if gefragt hat und was nirgends definiert ist: mit -D waehlbar
    for (const std::string& name : d.ergebnis.unbekannteNamen) std::cout << "undefined " << name << '\n';
    for (const c2d::diagramm::Zustandsdiagramm& z : c2d::diagramm::automatenVon(einheit))
        std::cout << "automaton " << z.titel << " (" << z.zustaende.size() << " states, " << z.uebergaenge.size()
                  << " transitions)\n";
    for (const c2d::modell::Funktion& f : einheit.funktionen)
        for (const int zeile : c2d::diagramm::zeilenMitZuordnung(f))
            if (const std::optional<c2d::diagramm::Zuordnung> z = c2d::diagramm::zuordnungVon(f, zeile))
                std::cout << "mapping   " << f.name << '@' << zeile << " (" << z->zeilen.size() << " rows, "
                          << z->spalten.size() << " columns)\n";
    if (const std::vector<c2d::diagramm::Konfigurationstabelle> k =
            c2d::diagramm::konfigurationVon(d.ergebnis.weichen, d.ergebnis.schalter);
        !k.empty())
    {
        std::size_t zeilen = 0, ungeteilt = 0;
        for (const c2d::diagramm::Konfigurationstabelle& t : k)
        {
            zeilen += t.zeilen.size();
            for (const c2d::diagramm::Konfigurationstabelle::Zeile& z : t.zeilen) ungeteilt += z.text ? 1 : 0;
        }
        std::cout << "ifconfig  " << k.size() << " chains, " << zeilen << " rows, " << ungeteilt << " not split\n";
    }
    for (const c2d::diagramm::Zuordnung& t : c2d::diagramm::nachschlagetabellenVon(
             einheit, [&](std::string_view name) { return typen.eindeutig(i, name); }))
        std::cout << "lookup    " << t.titel << " (" << t.zeilen.size() << " rows, " << t.spalten.size()
                  << " columns)\n";
}

/// Passt der Name auf den Automaten: die Variable, ihr letztes Glied, oder der Titel?
bool istAutomat(const c2d::diagramm::Zustandsdiagramm& z, const std::string& name)
{
    if (z.variable == name || z.titel == name) return true;
    const std::size_t punkt = z.variable.find_last_of(".>");
    return punkt != std::string::npos && z.variable.substr(punkt + 1) == name;
}

}  // namespace

int main(int argc, char* argv[])
{
    std::string ziel, datei, funktion, automat, typ, schaltet;
    std::vector<std::string> einbetten;
    c2d::projekt::ProjektOptionen lesen;
    std::string formatName = "mermaid";
    std::string richtung = "TD";
    bool zeigeListe = false, struktur = false, roh = false, aufrufgraph = false, struktogramm = false;
    bool wahrheitstabelle = false, entscheidungstabelle = false, zuordnung = false, nachschlagetabellen = false;
    bool konfiguration = false, breite = false, schalter = false;
    int tabellenZeile = 0;  // 0 = alle Tabellen der Funktion
    c2d::projekt::AufrufgraphOptionen graph;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view a = argv[i];
        const auto wert = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--liste") zeigeListe = true;
        else if (a == "--struktur") struktur = true;
        else if (a == "--roh") roh = true;
        else if (a == "--struktogramm") struktogramm = true;
        else if (a == "--entscheidungstabelle") entscheidungstabelle = true;
        else if (a == "--nachschlagetabellen") nachschlagetabellen = true;
        else if (a == "--konfiguration") konfiguration = true;
        else if (a == "--breite") breite = true;
        else if (a == "--schalter") schalter = true;
        else if (a == "--schaltet")
        {
            schaltet = wert();
            if (schaltet.empty()) return aufruf();
        }
        else if (a == "--zuordnung")
        {
            zuordnung = true;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') tabellenZeile = std::atoi(argv[++i]);
        }
        else if (a == "--wahrheitstabelle")
        {
            wahrheitstabelle = true;
            // die Zeile darf folgen
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') tabellenZeile = std::atoi(argv[++i]);
        }
        else if (a == "--datei") datei = wert();
        else if (a == "--funktion") funktion = wert();
        else if (a == "--automat") automat = wert();
        else if (a == "--typ")
        {
            typ = wert();
            struktur = true;
        }
        else if (a == "--einbetten") einbetten.push_back(wert());
        else if (a == "--aufrufgraph")
        {
            aufrufgraph = true;
            // der Name der Funktion darf folgen, wenn das Ziel schon dasteht
            if (!ziel.empty() && i + 1 < argc && argv[i + 1][0] != '-') funktion = argv[++i];
        }
        else if (a == "--tiefe")
        {
            const std::string t = wert();
            if (t.empty() || !std::all_of(t.begin(), t.end(), [](char c) { return c >= '0' && c <= '9'; })) return aufruf();
            graph.tiefe = std::stoi(t);
        }
        else if (a == "--hoechstens")
        {
            const std::string h = wert();
            if (h.empty() || !std::all_of(h.begin(), h.end(), [](char c) { return c >= '0' && c <= '9'; })) return aufruf();
            graph.hoechstens = static_cast<std::size_t>(std::stoul(h));
        }
        else if (a == "--aufrufer") graph.richtung = c2d::projekt::AufrufRichtung::Aufrufer;
        else if (a == "--beide") graph.richtung = c2d::projekt::AufrufRichtung::Beide;
        else if (a == "--ohne-fremde") graph.ohneFremde = true;
        else if (a == "--alle-zweige") lesen.alleZweige = true;
        else if (a == "--varianten") lesen.varianten = true;
        else if (a == "--rahmen") lesen.bereiche = true;
        else if (a == "-D") lesen.definitionen.push_back(c2d::sprache::definitionAus(wert()));
        else if (a.starts_with("-D")) lesen.definitionen.push_back(c2d::sprache::definitionAus(a.substr(2)));
        else if (a == "-U") lesen.definitionen.push_back(c2d::sprache::definitionAus("!" + wert()));
        else if (a.starts_with("-U")) lesen.definitionen.push_back(c2d::sprache::definitionAus("!" + std::string(a.substr(2))));
        else if (a == "--format") formatName = wert();
        else if (a == "--richtung") richtung = wert();
        else if (a.starts_with("-") || !ziel.empty()) return aufruf();
        else ziel = a;
    }
    const c2d::ausgabe::Format* format = c2d::ausgabe::format(formatName);
    if (ziel.empty() || format == nullptr || (richtung != "TD" && richtung != "LR")) return aufruf();

    std::error_code fehler;
    const bool ordner = fs::is_directory(alsWeg(ziel), fehler);
    Projekt p;
    if (ordner)
        p = c2d::projekt::liesOrdner(alsWeg(ziel), lesen);
    else
    {
        std::string inhalt;
        if (!c2d::projekt::liesDatei(alsWeg(ziel), inhalt))
        {
            std::cerr << "c2d: cannot read " << ziel << '\n';
            return 2;
        }
        // die Einheit heisst wie die Datei, ohne den Weg dorthin
        const std::size_t strich = ziel.find_last_of("/\\");
        const std::string name = strich == std::string::npos ? ziel : ziel.substr(strich + 1);
        if (c2d::sprache::spracheFuer(name) == nullptr)
            std::cerr << "c2d: no reader for this file type - read as " << c2d::sprache::c().name() << '\n';
        p = c2d::projekt::liesText(inhalt, name, lesen);
    }

    // Meldungen: je Datei mit Zeile und Spalte, dann die des Projekts
    const auto vorn = [&](const std::string& pfad) { return ordner ? pfad : ziel; };
    for (const c2d::projekt::Datei& d : p.dateien)
        for (const c2d::quelle::Meldung& m : d.ergebnis.meldungen)
            std::cerr << vorn(d.pfad) << ':' << m.ort.zeile << ':' << m.ort.spalte << ": " << m.text << '\n';
    for (const c2d::projekt::Projektmeldung& m : p.meldungen)
    {
        std::cerr << (m.pfad.empty() ? ziel : m.pfad);
        if (m.meldung.ort.zeile > 0) std::cerr << ':' << m.meldung.ort.zeile;
        std::cerr << ": " << m.meldung.text << '\n';
    }
    if (ordner)
        std::cerr << "c2d: " << p.dateien.size() << " file(s), " << p.funktionen() << " function(s), " << p.typen()
                  << " type(s), " << p.uebergangen.size() << " folder(s) skipped"
                  << (p.mitProjektdatei ? ", with " : ", without ") << c2d::projekt::kProjektdatei << '\n';

    // die Datei, um die es geht
    std::size_t gewaehlteDatei = ordner ? c2d::projekt::kKeine : 0;
    if (!datei.empty())
    {
        if (!ordner)
        {
            std::cerr << "c2d: --datei needs a folder\n";
            return 1;
        }
        std::string normal = datei;
        std::replace(normal.begin(), normal.end(), '\\', '/');
        gewaehlteDatei = p.finde(normal);
        if (gewaehlteDatei == c2d::projekt::kKeine)
        {
            std::cerr << "c2d: no file '" << datei << "' in the project\n";
            return 1;
        }
    }

    if (zeigeListe)
    {
        const c2d::projekt::Typverzeichnis typen(p);
        for (std::size_t i = 0; i < p.dateien.size(); ++i)
        {
            if (gewaehlteDatei != c2d::projekt::kKeine && i != gewaehlteDatei) continue;
            if (ordner) std::cout << "== " << p.dateien[i].pfad << '\n';
            liste(p, i, typen);
        }
        return 0;
    }

    const c2d::ausgabe::Optionen optionen{.richtung = richtung};
    // schreiben, wenn das Format die Art kann — sonst sagen, dass nicht (Rueckgabe 1)
    const auto schreibe = [&](c2d::ausgabe::Diagrammart art, const auto& diagramm) {
        if (!format->kann(art))
        {
            std::cerr << "c2d: " << c2d::ausgabe::nichtVerfuegbar(*format, art) << '\n';
            return 1;
        }
        std::cout << format->schreibe(diagramm, optionen);
        return 0;
    };

    // der Breiten-Test: jede geschichtete Anordnung, nach unten und nach rechts
    if (breite)
    {
        using namespace c2d::diagramm;
        std::size_t geprueft = 0, zuBreit = 0;
        double hoechstes = 0;
        std::string wo;
        for (std::size_t i = 0; i < p.dateien.size(); ++i)
        {
            if (gewaehlteDatei != c2d::projekt::kKeine && i != gewaehlteDatei) continue;
            const c2d::modell::Einheit& einheit = p.dateien[i].ergebnis.einheit;
            const auto pruefe = [&](const auto& diagramm, const std::string& was) {
                for (const bool nachRechts : {false, true})
                {
                    AnordnungOptionen o;
                    o.nachRechts = nachRechts;
                    const auto a = ordneAn(diagramm, o);
                    const double v = querVerhaeltnis(a);
                    ++geprueft;
                    const std::string hier = vorn(p.dateien[i].pfad) + ": " + was + (nachRechts ? " LR" : " TD");
                    if (v > hoechstes)
                    {
                        hoechstes = v;
                        wo = hier;
                    }
                    if (v <= kHoechstesQuerVerhaeltnis) continue;
                    ++zuBreit;
                    std::cout << hier << ": " << static_cast<long>(a.quer) << " px, needs " << static_cast<long>(a.querBedarf)
                              << " px (x" << std::fixed << std::setprecision(2) << v << ")\n" << std::defaultfloat;
                }
            };
            bool mitRumpf = false;
            for (const c2d::modell::Funktion& f : einheit.funktionen)
            {
                if (!f.rumpf) continue;
                mitRumpf = true;
                pruefe(fasseAnweisungsfolgenZusammen(flussVon(f)), "flow " + f.vollerName());
            }
            for (const Zustandsdiagramm& z : automatenVon(einheit)) pruefe(z, "automaton " + z.titel);
            if (!einheit.typen.empty()) pruefe(strukturVon(einheit), "data types");
            if (mitRumpf) pruefe(c2d::projekt::aufrufgraphDerDatei(p, i, graph).graph, "call graph of the file");
        }
        std::cout << "checked " << geprueft << " layouts, " << zuBreit << " wider than x" << kHoechstesQuerVerhaeltnis
                  << " of their widest layer";
        if (geprueft > 0) std::cout << "; widest x" << std::fixed << std::setprecision(2) << hoechstes << " in " << wo;
        std::cout << '\n';
        return zuBreit > 0 ? 3 : 0;
    }

    // die konstanten Tabellen einer Datei
    if (nachschlagetabellen)
    {
        if (gewaehlteDatei == c2d::projekt::kKeine)
        {
            std::cerr << "c2d: --nachschlagetabellen needs --datei in a folder\n";
            return 1;
        }
        const c2d::projekt::Typverzeichnis typen(p);
        const std::vector<c2d::diagramm::Zuordnung> tabellen = c2d::diagramm::nachschlagetabellenVon(
            p.dateien[gewaehlteDatei].ergebnis.einheit,
            [&](std::string_view name) { return typen.eindeutig(gewaehlteDatei, name); });
        if (tabellen.empty())
        {
            std::cerr << "c2d: no constant table in " << vorn(p.dateien[gewaehlteDatei].pfad) << '\n';
            return 1;
        }
        return schreibe(c2d::ausgabe::Diagrammart::Zuordnung, std::span<const c2d::diagramm::Zuordnung>(tabellen));
    }

    // je Kette #if der Datei: welcher Zweig bei welchen Definitionen gilt
    if (konfiguration)
    {
        if (gewaehlteDatei == c2d::projekt::kKeine)
        {
            std::cerr << "c2d: --konfiguration needs --datei in a folder\n";
            return 1;
        }
        const c2d::sprache::Ergebnis& e = p.dateien[gewaehlteDatei].ergebnis;
        const std::vector<c2d::diagramm::Konfigurationstabelle> tabellen = c2d::diagramm::konfigurationVon(e.weichen, e.schalter);
        if (tabellen.empty())
        {
            std::cerr << "c2d: no #if chain in " << vorn(p.dateien[gewaehlteDatei].pfad)
                      << (lesen.alleZweige ? " (--alle-zweige does not evaluate #if)" : "") << '\n';
            return 1;
        }
        return schreibe(c2d::ausgabe::Diagrammart::Konfiguration,
                        std::span<const c2d::diagramm::Konfigurationstabelle>(tabellen));
    }

    // wo ein Name schaltet, und die Namen des Projekts
    if (!schaltet.empty() || schalter)
    {
        if (lesen.alleZweige)
        {
            std::cerr << "c2d: --alle-zweige does not evaluate #if - nothing switches\n";
            return 1;
        }
        if (schalter) return schreibe(c2d::ausgabe::Diagrammart::Schalteruebersicht, c2d::projekt::schalteruebersicht(p, lesen.definitionen));
        const c2d::diagramm::Schaltstellen s = c2d::projekt::schaltstellen(p, schaltet, lesen.definitionen);
        if (s.zeilen.empty() && s.gesetzt.empty())
        {
            std::cerr << "c2d: no #if asks for '" << schaltet << "' and nothing sets it - see --schalter\n";
            return 1;
        }
        return schreibe(c2d::ausgabe::Diagrammart::Schaltstellen, s);
    }

    // --automat: ein Zustandsautomat, erkannt im Code
    if (!automat.empty())
    {
        std::vector<c2d::diagramm::Zustandsdiagramm> treffer;
        std::vector<std::string> wo;
        for (std::size_t d = 0; d < p.dateien.size(); ++d)
        {
            if (gewaehlteDatei != c2d::projekt::kKeine && d != gewaehlteDatei) continue;
            for (c2d::diagramm::Zustandsdiagramm& z : c2d::diagramm::automatenVon(p.dateien[d].ergebnis.einheit))
                if (istAutomat(z, automat))
                {
                    wo.push_back(vorn(p.dateien[d].pfad) + ":" + std::to_string(z.zeile));
                    treffer.push_back(std::move(z));
                }
        }
        if (treffer.size() != 1)
        {
            std::cerr << "c2d: " << (treffer.empty() ? "no automaton '" + automat + "'" : "'" + automat + "' is ambiguous:");
            for (const std::string& w : wo) std::cerr << ' ' << w;
            std::cerr << (treffer.empty() ? " - see --liste\n" : " - choose one with --datei\n");
            return 1;
        }
        return schreibe(c2d::ausgabe::Diagrammart::Zustandsautomat, treffer.front());
    }

    // --aufrufgraph: ab einer Funktion, oder der Graph einer Datei
    if (aufrufgraph && funktion.empty())
    {
        if (gewaehlteDatei == c2d::projekt::kKeine)
        {
            std::cerr << "c2d: --aufrufgraph without a function needs --datei\n";
            return 1;
        }
    }

    std::optional<Fundstelle> gewaehlt;
    if (!struktur && !(aufrufgraph && funktion.empty()))
    {
        std::vector<Fundstelle> treffer;
        for (std::size_t d = 0; d < p.dateien.size(); ++d)
        {
            if (gewaehlteDatei != c2d::projekt::kKeine && d != gewaehlteDatei) continue;
            const std::vector<c2d::modell::Funktion>& fs = p.dateien[d].ergebnis.einheit.funktionen;
            for (std::size_t f = 0; f < fs.size(); ++f)
            {
                // der Name allein oder mit Bereich: `starte` oder `antrieb::Motor::starte`
                if (!fs[f].rumpf || (!funktion.empty() && fs[f].name != funktion && fs[f].vollerName() != funktion))
                    continue;
                treffer.push_back({d, f});
            }
        }
        if (funktion.empty() && !treffer.empty()) treffer.resize(1);  // ohne Auswahl: die erste
        if (treffer.size() > 1)
        {
            std::cerr << "c2d: '" << funktion << "' is ambiguous: " << orte(p, treffer) << " - choose one with --datei\n";
            return 1;
        }
        if (treffer.empty() && !funktion.empty())
        {
            std::cerr << "c2d: no function '" << funktion << "' with a body in " << ziel << '\n';
            return 1;
        }
        if (!treffer.empty()) gewaehlt = treffer.front();
    }
    if (aufrufgraph)
    {
        const c2d::projekt::AufrufgraphErgebnis e = gewaehlt ? c2d::projekt::aufrufgraphAb(p, *gewaehlt, graph)
                                                             : c2d::projekt::aufrufgraphDerDatei(p, gewaehlteDatei, graph);
        for (const c2d::projekt::Projektmeldung& m : e.meldungen)
            std::cerr << vorn(m.pfad) << ':' << m.meldung.ort.zeile << ": " << m.meldung.text << '\n';
        if (e.ausgelassen != 0) std::cerr << "c2d: " << e.ausgelassen << " external call(s) left out\n";
        if (e.begrenzt)
            std::cerr << "c2d: call graph stopped at " << graph.hoechstens
                      << " functions - lower the depth or raise --hoechstens\n";
        if (e.graph.knoten.empty())
        {
            std::cerr << "c2d: no function with a body in " << vorn(p.dateien[gewaehlteDatei].pfad) << '\n';
            return 1;
        }
        return schreibe(c2d::ausgabe::Diagrammart::Aufrufgraph, e.graph);
    }
    if (gewaehlt)
    {
        using namespace c2d::diagramm;
        const auto ablauf = [&](const c2d::modell::Funktion& f) {
            Flussdiagramm d = flussVon(f);
            return roh ? d : fasseAnweisungsfolgenZusammen(d);
        };
        const c2d::modell::Funktion& wahl = c2d::projekt::funktion(p, *gewaehlt);
        if (struktogramm)
        {
            const Struktogramm s = struktogrammVon(wahl);
            for (const Hinweis& h : s.hinweise)
                std::cerr << vorn(p.dateien[gewaehlt->datei].pfad) << ':' << h.zeile << ": " << h.text << '\n';
            return schreibe(c2d::ausgabe::Diagrammart::Struktogramm, s);
        }
        if (zuordnung)
        {
            std::vector<Zuordnung> tabellen;
            for (const int z : tabellenZeile == 0 ? zeilenMitZuordnung(wahl) : std::vector<int>{tabellenZeile})
                if (std::optional<Zuordnung> t = zuordnungVon(wahl, z)) tabellen.push_back(std::move(*t));
            if (tabellen.empty())
            {
                std::cerr << "c2d: no switch that only maps values in " << wahl.name << "()"
                          << (tabellenZeile == 0 ? std::string() : " at line " + std::to_string(tabellenZeile)) << '\n';
                return 1;
            }
            return schreibe(c2d::ausgabe::Diagrammart::Zuordnung, std::span<const Zuordnung>(tabellen));
        }
        if (entscheidungstabelle)
        {
            const Entscheidungstabelle t = entscheidungstabelleVon(wahl);
            for (const std::string& h : t.hinweise) std::cerr << "c2d: " << wahl.name << "(): " << h << '\n';
            return schreibe(c2d::ausgabe::Diagrammart::Entscheidungstabelle, t);
        }
        if (wahrheitstabelle)
        {
            std::vector<Wahrheitstabelle> tabellen;
            if (tabellenZeile == 0)
                tabellen = wahrheitstabellenVon(wahl);
            else if (std::optional<Wahrheitstabelle> t = wahrheitstabelleVon(wahl, tabellenZeile))
                tabellen.push_back(std::move(*t));
            if (tabellen.empty())
            {
                std::cerr << "c2d: no condition with two or more parts in " << wahl.name << "()"
                          << (tabellenZeile == 0 ? std::string() : " at line " + std::to_string(tabellenZeile)) << '\n';
                return 1;
            }
            return schreibe(c2d::ausgabe::Diagrammart::Wahrheitstabelle, std::span<const Wahrheitstabelle>(tabellen));
        }
        Flussdiagramm d = ablauf(wahl);
        if (const int ab = unerreichbarAb(d); ab > 0)
            std::cerr << vorn(p.dateien[gewaehlt->datei].pfad) << ':' << ab << ": " << wahl.name
                      << "(): code from here on is never reached\n";
        // --einbetten: hinter den ersten Knoten, der die Funktion ruft und noch keine Einbettung traegt;
        // was der Aufruf meint, entscheidet das Projekt (erst die eigene Datei, dann was nicht static ist)
        for (const std::string& name : einbetten)
        {
            bool gelungen = false;
            std::vector<Fundstelle> mehrdeutig;
            for (KnotenId i = 0; !gelungen && i < d.knoten().size(); ++i)
                for (const std::string& a : d.knoten()[i].aufrufe)
                {
                    std::vector<Fundstelle> gemeint = c2d::projekt::kandidaten(p, gewaehlt->datei, a);
                    std::erase_if(gemeint, [&](const Fundstelle& f) {
                        const c2d::modell::Funktion& g = c2d::projekt::funktion(p, f);
                        return g.name != name && g.vollerName() != name;
                    });
                    if (gemeint.size() > 1) mehrdeutig = gemeint;
                    if (gemeint.size() != 1) continue;
                    const c2d::modell::Funktion& gerufen = c2d::projekt::funktion(p, gemeint.front());
                    Flussdiagramm neu = betteEin(d, i, ablauf(gerufen), gerufen.signatur());
                    gelungen = neu.einbettungen() > d.einbettungen();
                    if (gelungen) d = std::move(neu);
                    break;
                }
            if (gelungen) continue;
            if (!mehrdeutig.empty())
                std::cerr << "c2d: cannot embed '" << name << "': the call is ambiguous (" << orte(p, mehrdeutig) << ")\n";
            else
                std::cerr << "c2d: cannot embed '" << name << "'\n";
        }
        return schreibe(c2d::ausgabe::Diagrammart::Ablauf, d);
    }

    // ein Typ und seine Nachbarn, in seiner Datei
    if (!typ.empty())
    {
        if (gewaehlteDatei == c2d::projekt::kKeine)
        {
            std::cerr << "c2d: --typ needs --datei in a folder\n";
            return 1;
        }
        const c2d::diagramm::Strukturdiagramm d = c2d::diagramm::strukturVon(p.dateien[gewaehlteDatei].ergebnis.einheit, typ);
        if (d.kaesten.empty())
        {
            std::cerr << "c2d: no type '" << typ << "' in " << vorn(p.dateien[gewaehlteDatei].pfad) << '\n';
            return 1;
        }
        return schreibe(c2d::ausgabe::Diagrammart::Datentypen, d);
    }

    // die Datentypen: einer Datei, oder aller
    c2d::modell::Einheit alle;
    alle.name = ordner ? alsText(p.wurzel.filename()) : p.dateien[0].ergebnis.einheit.name;
    for (std::size_t d = 0; d < p.dateien.size(); ++d)
    {
        if (gewaehlteDatei != c2d::projekt::kKeine && d != gewaehlteDatei) continue;
        const c2d::modell::Einheit& e = p.dateien[d].ergebnis.einheit;
        if (gewaehlteDatei != c2d::projekt::kKeine) alle.name = e.name;
        alle.typen.insert(alle.typen.end(), e.typen.begin(), e.typen.end());
        alle.aliase.insert(alle.aliase.end(), e.aliase.begin(), e.aliase.end());
    }
    if (alle.typen.empty())
    {
        std::cerr << "c2d: nothing to draw in " << ziel << '\n';
        return 1;
    }
    return schreibe(c2d::ausgabe::Diagrammart::Datentypen, c2d::diagramm::strukturVon(alle));
}
