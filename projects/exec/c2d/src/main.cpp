// c2d — Code2Diagram fuer die Kommandozeile.
//
// Aufruf:  c2d <datei|ordner> [--liste] [--datei <pfad>] [--funktion <name>] [--einbetten <name>]...
//                             [--struktur [--typ <name>] [--notation <n>] [--attribute <a>]]
//                             [-D NAME[=wert] | -D !NAME | -U NAME]...
//                             [--aufrufgraph [<funktion>] [--tiefe n] [--hoechstens n] [--aufrufer | --beide]
//                                            [--ohne-fremde]]
//                             [--abhaengigkeiten [<objekt>] [--tiefe n] [--hoechstens n] [--aufrufer | --beide]]
//                             [--abfrage [<name>|<zeile>]] [--datenfluss [<funktion>]]
//                             [--plan [<name>] [--abschnitt physical|optimized|analyzed|parsed]]
//                             [--alle-zweige | --varianten | --rahmen]
//                             [--format <name>] [--richtung TD|LR] [--roh] [--struktogramm]
//                             [--wahrheitstabelle [<zeile>]] [--entscheidungstabelle]
//                             [--zuordnung [<zeile>]] [--nachschlagetabellen] [--konfiguration]
//                             [--schaltet <name>] [--schalter] [--breite]
//
//   <ordner>       ein Projekt: alle Quelldateien darunter, mit code2diagram.json, wenn es sie gibt
//   <datei>        auch ein Notebook (.ipynb, Databricks): je Code-Zelle eine Funktion "<cell n>";
//                  Meldungen nennen dazu die Zelle "(cell n, line m)"
//   ohne Auswahl   das Diagramm der ersten Funktion mit Rumpf; gibt es keine,
//                  das Strukturdiagramm
//   --liste        nennt, was die Datei (das Projekt: jede Datei) enthaelt (auch Automaten,
//                  Zuordnungen "mapping", konstante Tabellen "lookup", die Ketten #if "ifconfig",
//                  die Zellen eines Notebooks "notebook"), und endet
//   --datei        im Projekt: die Datei, um die es geht (relativ zum Ordner)
//   --funktion     das Flussdiagramm dieser Funktion (Name allein oder mit Bereich); im
//                  Projekt muss sie eindeutig sein, sonst --datei
//   --einbetten    den Ablauf dieser gerufenen Funktion hinter ihren ersten Aufruf setzen,
//                  umrahmt — im Projekt auch aus einer anderen Datei
//   --struktur     das Diagramm der Datentypen (im Projekt: aller Dateien, mit --datei: einer)
//   --typ          die Datentypen, nur dieser Typ und seine Nachbarn in seiner Datei (im Projekt: --datei)
//   --notation     wie Tabellen gezeichnet werden: tabellen (Vorgabe, Multiplizitaeten), kraehenfuss,
//                  chen, minmax; Klassen bleiben UML (meint --struktur)
//   --attribute    in chen und minmax: alle Spalten ausser Fremdschluesseln als Ellipsen (Vorgabe),
//                  nur der Schluessel, keine (meint --struktur)
//   --automat      das Zustandsdiagramm des Automaten mit dieser Variable (auch nur ihr
//                  letztes Glied: Bus_State); --liste nennt die erkannten
//   --aufrufgraph  der Aufrufgraph ab dieser Funktion (wie --funktion; steht das Ziel
//                  davor, darf ihr Name gleich folgen); ohne Funktion der Graph der Datei
//                  (im Projekt: --datei)
//   --datenfluss   Spark: der Datenfluss dieser Funktion oder Zelle (`<cell 3>`) — Quellen, Schritte,
//                  Shuffles, Jobs, Stages; ohne Namen der ganzen Datei (im Projekt: --datei);
//                  Hinweise auf stderr; --liste nennt sie "dataflow"
//   --plan         Spark: der Plan aus explain() mit diesem Namen (`funktion@zeile`, `<cell 3>@zeile`, der
//                  Dateiname, `datei@zeile`) — Operatoren, Exchanges, Stages und Codegen in Rahmen; ohne Namen
//                  der erste der Datei (im Projekt: --datei); --liste nennt sie "plan"
//   --abschnitt    mit --plan: physical (Vorgabe), optimized, analyzed, parsed (explain(extended))
//   --lauf         Spark: der Lauf eines Event-Logs (App Name oder ID; ohne: der der Datei) als Zeitachse — Jobs,
//                  Stages, Tasks je Executor und Kern; svg, drawio, mermaid (gantt), csv (je Task eine Zeile);
//                  --liste nennt ihn "run"
//   --tiefe n     so viele Stufen ab der Funktion (Vorgabe 2), 0 = alle
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
#include "c2d/projekt/Abhaengigkeiten.hpp"
#include "c2d/projekt/Aufrufgraph.hpp"
#include "c2d/projekt/Datenfluss.hpp"
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
#include <type_traits>
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
                 "                         [--struktur [--typ <name>] [--notation tabellen|kraehenfuss|chen|minmax]\n"
                 "                                     [--attribute alle|schluessel|keine]] [--automat <variable>] [-D NAME[=value] | -D !NAME | -U NAME]...\n"
                 "                         [--aufrufgraph [<function>] [--tiefe n] [--hoechstens n] [--aufrufer | --beide]\n"
                 "                                        [--ohne-fremde]]\n"
                 "                         [--alle-zweige | --varianten | --rahmen]\n"
                 "                         [--format <name>] [--richtung TD|LR] [--roh] [--struktogramm]\n"
                 "                         [--wahrheitstabelle [<line>]] [--entscheidungstabelle]\n"
                 "                         [--zuordnung [<line>]] [--nachschlagetabellen] [--konfiguration]\n"
                 "                         [--schaltet <name>] [--schalter] [--breite]\n"
                 "                         [--abhaengigkeiten [<object>]] [--abfrage [<name>|<line>]]\n"
                 "                         [--datenfluss [<function>]]\n"
                 "                         [--plan [<name>] [--abschnitt physical|optimized|analyzed|parsed]] [--lauf [<name>]]\n"
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
    if (!d.zellen.empty())
    {
        int code = 0, text = 0, fremd = 0, ungelesen = 0;
        for (const c2d::projekt::Zelle& z : d.zellen)
        {
            if (z.art == c2d::projekt::Zelle::Art::Code) ++code;
            else ++text;
            if (!z.gelesen) ++ungelesen;
            else if (!z.sprache.empty()) ++fremd;
        }
        std::cout << "notebook  " << d.sprache->name() << ", " << d.zellen.size() << " cells (" << code << " code, "
                  << text << " text, " << fremd << " in another language, " << ungelesen << " not read)\n";
    }
    for (const c2d::modell::Funktion& f : einheit.funktionen)
        std::cout << (f.rumpf ? "function  " : "prototype ") << f.signatur() << (f.intern ? "  [static]" : "") << '\n';
    for (const c2d::modell::Typ& t : einheit.typen)
        std::cout << "type      " << t.vollerName() << " (" << t.felder.size() << " + " << t.methoden.size() << ")\n";
    // was liest, schreibt, ruft (SQL): die Objekte der Datenbank und Funktionen mit SQL im Rumpf
    const auto zugriffe = [](const std::vector<c2d::modell::Zugriff>& z) {
        int liest = 0, schreibt = 0, ruft = 0, unsicher = 0;
        for (const c2d::modell::Zugriff& x : z)
        {
            if (x.art == c2d::modell::ZugriffArt::Liest) ++liest;
            if (x.art == c2d::modell::ZugriffArt::Schreibt) ++schreibt;
            if (x.art == c2d::modell::ZugriffArt::Ruft && !x.imAusdruck) ++ruft;
            if (x.unsicher) ++unsicher;
        }
        return " (" + std::to_string(liest) + " reads, " + std::to_string(schreibt) + " writes, " + std::to_string(ruft) +
               " calls" + (unsicher > 0 ? ", " + std::to_string(unsicher) + " uncertain" : std::string()) + ")";
    };
    static constexpr const char* kObjektArt[] = {"view", "trigger", "function", "procedure"};
    for (const c2d::modell::Datenbankobjekt& o : einheit.objekte)
        std::cout << "object    " << kObjektArt[static_cast<int>(o.art)] << ' ' << o.name << zugriffe(o.zugriffe) << '\n';
    for (const c2d::modell::Funktion& f : einheit.funktionen)
        if (!f.zugriffe.empty()) std::cout << "access    " << f.vollerName() << zugriffe(f.zugriffe) << '\n';
    // eine Anweisung mit Ziel auf oberster Ebene einer .sql: ein Knoten in Data dependencies (C2)
    for (const c2d::modell::Abfrage& q : einheit.abfragen)
        if (!q.ziel.empty() && !q.zugriffe.empty()) std::cout << "statement " << q.name << zugriffe(q.zugriffe) << '\n';
    for (const c2d::modell::Abfrage& q : einheit.abfragen)
    {
        std::size_t quellen = 0;
        for (const c2d::modell::AbfrageBlock& b : q.bloecke) quellen += b.quellen.size();
        std::cout << "query     " << q.name << " (" << q.bloecke.size() << " blocks, " << quellen << " sources"
                  << (q.ziel.empty() ? std::string() : ", " + q.schreibt + " " + q.ziel) << ")\n";
    }
    // ein Datenfluss (Spark Schicht 1): je Funktion oder Zelle mit einem
    for (const std::string& f : c2d::projekt::datenfluesseIn(p, i))
    {
        const c2d::diagramm::Datenflussdiagramm fluss = c2d::projekt::datenflussIn(p, i, f);
        std::cout << "dataflow  " << f << " (" << fluss.knoten.size() << " nodes, " << fluss.jobs << " jobs, " << fluss.stages
                  << " stages" << (fluss.hinweise.empty() ? std::string() : ", " + std::to_string(fluss.hinweise.size()) + " notes")
                  << ")\n";
    }
    // ein Plan von Spark (Schicht 2): je explain() in einer Ausgabe bzw. je Plan einer Plan-Datei
    for (const c2d::modell::Plan& plan : einheit.plaene)
    {
        const c2d::diagramm::Plandiagramm d = c2d::diagramm::planVon(plan);
        std::string abschnitte;
        for (const c2d::modell::Planbaum& b : plan.baeume) abschnitte += (abschnitte.empty() ? "" : ", ") + c2d::diagramm::kurzname(b);
        std::cout << "plan      " << plan.name << " (" << plan.modus << ": " << abschnitte << "; " << d.knoten.size()
                  << " operators, " << d.stages << " stages"
                  << (plan.adaptiv ? plan.endgueltig ? ", adaptive final" : ", adaptive" : "")
                  << (d.hinweise.empty() ? std::string() : ", " + std::to_string(d.hinweise.size()) + " notes") << ")\n";
    }
    // ein Lauf von Spark (Schicht 3): der Event-Log
    for (const c2d::modell::Lauf& l : einheit.laeufe)
    {
        std::size_t offen = 0, fehlgeschlagen = 0;
        for (const c2d::modell::LaufTask& t : l.tasks)
        {
            if (t.ende == 0) ++offen;
            else if (t.ergebnis != "Success") ++fehlgeschlagen;
        }
        std::cout << "run       " << l.name << " (" << (l.id.empty() ? std::string() : l.id + ", ")
                  << (l.version.empty() ? std::string() : "Spark " + l.version + ": ") << l.jobs.size() << " jobs, "
                  << l.stages.size() << " stages, " << l.tasks.size() << " tasks, " << l.executoren.size() << " executors"
                  << (fehlgeschlagen > 0 ? ", " + std::to_string(fehlgeschlagen) + " failed" : "")
                  << (offen > 0 ? ", " + std::to_string(offen) + " without an end" : "") << (l.ende == 0 ? ", not finished" : "")
                  << ")\n";
    }
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

/// Wie viele Rahmen einen Knoten oder Rahmen umfassen, der nicht in ihnen steht (Datenfluss, Plan: die Rahmen
/// kennen ihre Eltern, die Knoten ihren innersten Rahmen).
template <typename Diagramm>
std::size_t ueberdeckungen(const Diagramm& d, const c2d::diagramm::FlussAnordnung& a, std::string& erste)
{
    const auto schneidet = [](const c2d::diagramm::Rechteck& f, const c2d::diagramm::Rechteck& x) {
        return x.x < f.x + f.breite && x.x + x.breite > f.x && x.y < f.y + f.hoehe && x.y + x.hoehe > f.y;
    };
    const auto vorfahr = [&](std::size_t oben, std::size_t r) {
        for (; r != c2d::projekt::kKeine; r = d.rahmen[r].eltern)
            if (r == oben) return true;
        return false;
    };
    std::size_t aus = 0;
    for (std::size_t g = 0; g < d.rahmen.size() && g < a.gruppen.size(); ++g)
    {
        for (std::size_t k = 0; k < d.knoten.size() && k < a.knoten.size(); ++k)
            if (!vorfahr(g, d.knoten[k].gruppe) && !(d.knoten[k].gruppe != c2d::projekt::kKeine && vorfahr(d.knoten[k].gruppe, g)) &&
                schneidet(a.gruppen[g], a.knoten[k]))
            {
                if (aus++ == 0) erste = "'" + d.rahmen[g].titel + "' over node '" + d.knoten[k].titel + "'";
            }
        for (std::size_t h = g + 1; h < d.rahmen.size() && h < a.gruppen.size(); ++h)
            if (!vorfahr(g, h) && !vorfahr(h, g) && schneidet(a.gruppen[g], a.gruppen[h]))
            {
                if (aus++ == 0)
                {
                    const auto r = [](const c2d::diagramm::Rechteck& x) {
                        return "[" + std::to_string(static_cast<int>(x.x)) + "," + std::to_string(static_cast<int>(x.y)) + " " +
                               std::to_string(static_cast<int>(x.breite)) + "x" + std::to_string(static_cast<int>(x.hoehe)) + "]";
                    };
                    erste = "'" + d.rahmen[g].titel + "' " + r(a.gruppen[g]) + " over frame '" + d.rahmen[h].titel + "' " +
                            r(a.gruppen[h]);
                }
            }
    }
    return aus;
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
    bool konfiguration = false, breite = false, schalter = false, abhaengigkeiten = false;
    std::string objekt;  // --abhaengigkeiten <objekt>
    bool abfrage = false;
    std::string abfrageName;  // --abfrage <name>|<zeile>
    bool datenfluss = false;
    std::string flussName;  // --datenfluss <funktion>
    bool plan = false;
    std::string planName;   // --plan <name>
    bool lauf = false;
    std::string laufName;   // --lauf <name>
    std::string abschnitt;  // --abschnitt physical|optimized|analyzed|parsed
    int tabellenZeile = 0;  // 0 = alle Tabellen der Funktion
    c2d::projekt::AufrufgraphOptionen graph;
    c2d::diagramm::ErNotation notation = c2d::diagramm::ErNotation::Tabellen;  // --notation (Entscheid N5)
    c2d::diagramm::ErAttribute attribute = c2d::diagramm::ErAttribute::Alle;

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
        else if (a == "--notation")
        {
            const std::string n = wert();
            const auto liste = c2d::diagramm::erNotationen();
            const auto x = std::find_if(liste.begin(), liste.end(), [&](const auto& e) { return e.name == n; });
            if (x == liste.end()) return aufruf();
            notation = x->notation;
            struktur = true;
        }
        else if (a == "--attribute")
        {
            const std::string n = wert();
            if (n == "alle") attribute = c2d::diagramm::ErAttribute::Alle;
            else if (n == "schluessel") attribute = c2d::diagramm::ErAttribute::Schluessel;
            else if (n == "keine") attribute = c2d::diagramm::ErAttribute::Keine;
            else return aufruf();
            struktur = true;
        }
        else if (a == "--aufrufgraph")
        {
            aufrufgraph = true;
            // der Name der Funktion darf folgen, wenn das Ziel schon dasteht
            if (!ziel.empty() && i + 1 < argc && argv[i + 1][0] != '-') funktion = argv[++i];
        }
        else if (a == "--abfrage")
        {
            abfrage = true;
            // der Name (oder die Zeile) darf folgen, wenn das Ziel schon dasteht
            if (!ziel.empty() && i + 1 < argc && argv[i + 1][0] != '-') abfrageName = argv[++i];
        }
        else if (a == "--datenfluss")
        {
            datenfluss = true;
            // der Name der Funktion (oder Zelle) darf folgen, wenn das Ziel schon dasteht
            if (!ziel.empty() && i + 1 < argc && argv[i + 1][0] != '-') flussName = argv[++i];
        }
        else if (a == "--plan")
        {
            plan = true;
            // der Name darf folgen, wenn das Ziel schon dasteht
            if (!ziel.empty() && i + 1 < argc && argv[i + 1][0] != '-') planName = argv[++i];
        }
        else if (a == "--lauf")
        {
            lauf = true;
            // der Name (App Name oder App ID) darf folgen, wenn das Ziel schon dasteht
            if (!ziel.empty() && i + 1 < argc && argv[i + 1][0] != '-') laufName = argv[++i];
        }
        else if (a == "--abschnitt")
        {
            abschnitt = wert();
            if (abschnitt != "physical" && abschnitt != "optimized" && abschnitt != "analyzed" && abschnitt != "parsed") return aufruf();
            plan = true;
        }
        else if (a == "--abhaengigkeiten")
        {
            abhaengigkeiten = true;
            // der Name des Objekts darf folgen, wenn das Ziel schon dasteht
            if (!ziel.empty() && i + 1 < argc && argv[i + 1][0] != '-') objekt = argv[++i];
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
        if (!c2d::projekt::kannLesen(name) && !c2d::sprache::eventLogText(inhalt))
            std::cerr << "c2d: no reader for this file type - read as " << c2d::sprache::c().name() << '\n';
        p = c2d::projekt::liesText(inhalt, name, lesen);
    }

    // Meldungen: je Datei mit Zeile und Spalte (in einem Notebook dazu die Zelle), dann die des Projekts
    const auto vorn = [&](const std::string& pfad) { return ordner ? pfad : ziel; };
    for (const c2d::projekt::Datei& d : p.dateien)
        for (const c2d::quelle::Meldung& m : d.ergebnis.meldungen)
            std::cerr << vorn(d.pfad) << ':' << m.ort.zeile << ':' << m.ort.spalte
                      << (d.zellen.empty() ? std::string() : " (" + c2d::projekt::stelleIn(d, m.ort.zeile) + ")") << ": "
                      << m.text << '\n';
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

    if (!ordner && p.dateien.empty()) return 2;  // ein Notebook, das sich nicht lesen liess (gemeldet)

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
        std::size_t geprueft = 0, zuBreit = 0, ueberdeckt = 0;
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
                    // ein Rahmen um einen fremden Knoten (Datenfluss, Plan)
                    using Art = std::decay_t<decltype(diagramm)>;
                    if constexpr (std::is_same_v<Art, Datenflussdiagramm> || std::is_same_v<Art, Plandiagramm>)
                    {
                        std::string erste;
                        if (const std::size_t n = ueberdeckungen(diagramm, a, erste); n > 0)
                        {
                            ++ueberdeckt;
                            std::cout << hier << ": " << n << " frame(s) over a node or frame not in them, first " << erste << '\n';
                        }
                    }
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
            for (std::size_t q = 0; q < einheit.abfragen.size(); ++q)
                pruefe(c2d::projekt::abfrageIn(p, i, q), "query " + einheit.abfragen[q].name);
            if (!c2d::projekt::datenfluesseIn(p, i).empty())
                pruefe(c2d::projekt::datenflussIn(p, i, ""), "data flow of the file");
            for (const c2d::modell::Plan& x : einheit.plaene) pruefe(planVon(x), "plan " + x.name);
        }
        std::cout << "checked " << geprueft << " layouts, " << zuBreit << " wider than x" << kHoechstesQuerVerhaeltnis
                  << " of their widest layer";
        if (geprueft > 0) std::cout << "; widest x" << std::fixed << std::setprecision(2) << hoechstes << " in " << wo;
        if (ueberdeckt > 0) std::cout << "; " << ueberdeckt << " with a frame over a foreign node";
        std::cout << '\n';
        return zuBreit > 0 || ueberdeckt > 0 ? 3 : 0;
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

    // --datenfluss: der Datenfluss (Spark Schicht 1, Entscheid F6) einer Funktion oder Zelle, ohne Namen der Datei
    if (datenfluss)
    {
        const std::size_t d = gewaehlteDatei != c2d::projekt::kKeine ? gewaehlteDatei : 0;
        std::string name = flussName;
        if (!name.empty())
        {
            // der Name allein oder mit Bereich; im Datenfluss steht der volle
            for (const c2d::modell::Funktion& f : p.dateien[d].ergebnis.einheit.funktionen)
                if (f.rumpf && (f.name == flussName || f.vollerName() == flussName)) name = f.vollerName();
        }
        const c2d::diagramm::Datenflussdiagramm fluss = c2d::projekt::datenflussIn(p, d, name);
        for (const c2d::diagramm::DatenflussHinweis& h : fluss.hinweise)
            std::cerr << vorn(p.dateien[d].pfad) << ":" << h.zeile << ": " << h.text << '\n';
        if (fluss.knoten.empty())
        {
            std::cerr << "c2d: no Spark data flow" << (flussName.empty() ? std::string() : " in '" + flussName + "'") << " in "
                      << vorn(p.dateien[d].pfad) << " - see --liste (dataflow)\n";
            return 1;
        }
        return schreibe(c2d::ausgabe::Diagrammart::Datenfluss, fluss);
    }

    // --lauf: ein Lauf von Spark aus seinem Event-Log (Schicht 3) als Zeitachse — nach App Name oder ID, sonst der erste
    if (lauf)
    {
        const std::size_t d = gewaehlteDatei != c2d::projekt::kKeine ? gewaehlteDatei : 0;
        const std::vector<c2d::modell::Lauf>& laeufe = p.dateien[d].ergebnis.einheit.laeufe;
        const auto x = std::find_if(laeufe.begin(), laeufe.end(), [&](const c2d::modell::Lauf& y) {
            return laufName.empty() || y.name == laufName || y.id == laufName;
        });
        if (x == laeufe.end())
        {
            std::cerr << "c2d: no Spark run" << (laufName.empty() ? std::string() : " '" + laufName + "'") << " in "
                      << vorn(p.dateien[d].pfad) << " - see --liste (run)\n";
            return 1;
        }
        const c2d::diagramm::Zeitachse z = c2d::diagramm::zeitachseVon(*x);
        for (const std::string& h : z.hinweise) std::cerr << vorn(p.dateien[d].pfad) << ": " << h << '\n';
        return schreibe(c2d::ausgabe::Diagrammart::Zeitachse, z);
    }

    // --plan: ein Plan von Spark (Schicht 2, Entscheid P2-4) — nach Name, sonst der erste der Datei; --abschnitt
    // waehlt einen logischen
    if (plan)
    {
        const std::size_t d = gewaehlteDatei != c2d::projekt::kKeine ? gewaehlteDatei : 0;
        const std::vector<c2d::modell::Plan>& plaene = p.dateien[d].ergebnis.einheit.plaene;
        const auto x = std::find_if(plaene.begin(), plaene.end(),
                                    [&](const c2d::modell::Plan& y) { return planName.empty() || y.name == planName; });
        if (x == plaene.end())
        {
            std::cerr << "c2d: no Spark plan" << (planName.empty() ? std::string() : " '" + planName + "'") << " in "
                      << vorn(p.dateien[d].pfad) << " - see --liste (plan)\n";
            return 1;
        }
        std::size_t baum = c2d::projekt::kKeine;
        if (!abschnitt.empty())
        {
            for (std::size_t b = 0; b < x->baeume.size(); ++b)
                if (c2d::diagramm::kurzname(x->baeume[b]) == abschnitt) baum = b;
            if (baum == c2d::projekt::kKeine)
            {
                std::cerr << "c2d: the plan '" << x->name << "' has no " << abschnitt << " plan\n";
                return 1;
            }
        }
        const c2d::diagramm::Plandiagramm d2 = c2d::diagramm::planVon(*x, baum);
        for (const std::string& h : d2.hinweise) std::cerr << vorn(p.dateien[d].pfad) << ": " << x->name << ": " << h << '\n';
        return schreibe(c2d::ausgabe::Diagrammart::Plan, d2);
    }

    // --abfrage: das Diagramm einer Abfrage (Entscheid Q5) — nach Name, nach Zeile, sonst die erste der Datei
    if (abfrage)
    {
        const std::size_t d = gewaehlteDatei != c2d::projekt::kKeine ? gewaehlteDatei : 0;
        const std::vector<c2d::modell::Abfrage>& abfragen = p.dateien[d].ergebnis.einheit.abfragen;
        std::size_t welche = abfragen.empty() ? c2d::projekt::kKeine : 0;
        if (!abfrageName.empty())
        {
            welche = c2d::projekt::kKeine;
            if (std::all_of(abfrageName.begin(), abfrageName.end(), [](char c) { return c >= '0' && c <= '9'; }))
                welche = c2d::diagramm::abfrageZurZeile(abfragen, std::stoi(abfrageName));
            for (std::size_t k = 0; k < abfragen.size() && welche == c2d::projekt::kKeine; ++k)
                if (abfragen[k].name == abfrageName) welche = k;
        }
        if (welche == c2d::projekt::kKeine)
        {
            std::cerr << "c2d: no query" << (abfrageName.empty() ? std::string() : " '" + abfrageName + "'") << " in "
                      << vorn(p.dateien[d].pfad) << (abfragen.empty() ? std::string() : " - see --liste (query)") << '\n';
            return 1;
        }
        return schreibe(c2d::ausgabe::Diagrammart::Abfrage, c2d::projekt::abfrageIn(p, d, welche));
    }

    // --abhaengigkeiten: was liest, schreibt, ruft (Entscheid A1) — ab einem Objekt, oder alles einer Datei
    if (abhaengigkeiten)
    {
        c2d::projekt::AbhaengigkeitsOptionen o;
        o.richtung = graph.richtung;
        o.tiefe = graph.tiefe;
        o.hoechstens = graph.hoechstens;
        c2d::projekt::AbhaengigkeitsErgebnis e;
        if (!objekt.empty())
        {
            // die Datei des Objekts: die gewaehlte, sonst die erste, die es kennt
            std::size_t d = gewaehlteDatei;
            for (std::size_t k = 0; k < p.dateien.size() && d == c2d::projekt::kKeine; ++k)
            {
                const std::vector<std::string> namen = c2d::projekt::datenobjekteIn(p, k);
                if (std::find(namen.begin(), namen.end(), objekt) != namen.end()) d = k;
            }
            if (d == c2d::projekt::kKeine)
            {
                std::cerr << "c2d: no table, view, trigger (name@table), routine or function with SQL '" << objekt
                          << "' - see --liste\n";
                return 1;
            }
            e = c2d::projekt::abhaengigkeitenAb(p, d, objekt, o);
        }
        else
        {
            const std::size_t d = gewaehlteDatei != c2d::projekt::kKeine ? gewaehlteDatei : 0;
            e = c2d::projekt::abhaengigkeitenDerDatei(p, d);
        }
        if (e.begrenzt)
            std::cerr << "c2d: data dependencies stopped at " << o.hoechstens << " nodes - lower the depth or raise --hoechstens\n";
        if (e.graph.knoten.empty())
        {
            std::cerr << "c2d: nothing reads, writes or calls here - see --liste (object, access)\n";
            return 1;
        }
        return schreibe(c2d::ausgabe::Diagrammart::Abhaengigkeiten, e.graph);
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
        c2d::diagramm::Strukturdiagramm d = c2d::diagramm::strukturVon(p.dateien[gewaehlteDatei].ergebnis.einheit, typ);
        d.notation = notation;
        d.attribute = attribute;
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
    c2d::diagramm::Strukturdiagramm d = c2d::diagramm::strukturVon(alle);
    d.notation = notation;
    d.attribute = attribute;
    return schreibe(c2d::ausgabe::Diagrammart::Datentypen, d);
}
