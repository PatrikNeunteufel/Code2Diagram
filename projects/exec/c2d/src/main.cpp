// c2d — Code2Diagram fuer die Kommandozeile.
//
// Aufruf:  c2d <datei|ordner> [--liste] [--datei <pfad>] [--funktion <name>] [--einbetten <name>]...
//                             [--struktur] [-D NAME[=wert]]... [--alle-zweige]
//                             [--format <name>] [--richtung TD|LR] [--roh]
//
//   <ordner>       ein Projekt: alle Quelldateien darunter, mit code2diagram.json, wenn es sie gibt
//   ohne Auswahl   das Diagramm der ersten Funktion mit Rumpf; gibt es keine,
//                  das Strukturdiagramm
//   --liste        nennt, was die Datei (das Projekt: jede Datei) enthaelt, und endet
//   --datei        im Projekt: die Datei, um die es geht (relativ zum Ordner)
//   --funktion     das Flussdiagramm dieser Funktion (Name allein oder mit Bereich); im
//                  Projekt muss sie eindeutig sein, sonst --datei
//   --einbetten    den Ablauf dieser gerufenen Funktion hinter ihren ersten Aufruf setzen,
//                  umrahmt — im Projekt auch aus einer anderen Datei
//   --struktur     das Diagramm der Datentypen (im Projekt: aller Dateien, mit --datei: einer)
//   --automat      das Zustandsdiagramm des Automaten mit dieser Variable (auch nur ihr
//                  letztes Glied: Bus_State); --liste nennt die erkannten
//   -D NAME[=wert] gilt beim Auswerten von #if/#ifdef als definiert (ohne Wert: 1); mehrfach
//                  erlaubt; geht der Projektdatei vor
//   --alle-zweige  bedingte Uebersetzung nicht auswerten: alle Zweige lesen
//   --format       mermaid (Vorgabe), svg, dot oder drawio
//   --roh          je Anweisung ein Knoten, nichts gebuendelt
//
// Das Diagramm geht auf die Standardausgabe, Meldungen auf die Fehlerausgabe.
// Exit: 0 gut, 1 Aufruf falsch oder nichts zu zeichnen, 2 Datei oder Ordner nicht lesbar.

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/projekt/Projekt.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

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
                 "                         [--struktur] [--automat <variable>] [-D NAME[=value]]... [--alle-zweige]\n"
                 "                         [--format <name>] [--richtung TD|LR] [--roh]\n"
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

void liste(const c2d::projekt::Datei& d)
{
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
    std::string ziel, datei, funktion, automat;
    std::vector<std::string> einbetten;
    c2d::projekt::ProjektOptionen lesen;
    std::string formatName = "mermaid";
    std::string richtung = "TD";
    bool zeigeListe = false, struktur = false, roh = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view a = argv[i];
        const auto wert = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--liste") zeigeListe = true;
        else if (a == "--struktur") struktur = true;
        else if (a == "--roh") roh = true;
        else if (a == "--datei") datei = wert();
        else if (a == "--funktion") funktion = wert();
        else if (a == "--automat") automat = wert();
        else if (a == "--einbetten") einbetten.push_back(wert());
        else if (a == "--alle-zweige") lesen.alleZweige = true;
        else if (a == "-D") lesen.definitionen.push_back(c2d::sprache::definitionAus(wert()));
        else if (a.starts_with("-D")) lesen.definitionen.push_back(c2d::sprache::definitionAus(a.substr(2)));
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
        std::ifstream strom(alsWeg(ziel), std::ios::binary);
        if (!strom)
        {
            std::cerr << "c2d: cannot read " << ziel << '\n';
            return 2;
        }
        std::ostringstream inhalt;
        inhalt << strom.rdbuf();
        // die Einheit heisst wie die Datei, ohne den Weg dorthin
        const std::size_t strich = ziel.find_last_of("/\\");
        const std::string name = strich == std::string::npos ? ziel : ziel.substr(strich + 1);
        if (c2d::sprache::spracheFuer(name) == nullptr)
            std::cerr << "c2d: no reader for this file type - read as " << c2d::sprache::c().name() << '\n';
        p = c2d::projekt::liesText(inhalt.str(), name, lesen);
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
        for (std::size_t i = 0; i < p.dateien.size(); ++i)
        {
            if (gewaehlteDatei != c2d::projekt::kKeine && i != gewaehlteDatei) continue;
            if (ordner) std::cout << "== " << p.dateien[i].pfad << '\n';
            liste(p.dateien[i]);
        }
        return 0;
    }

    const c2d::ausgabe::Optionen optionen{.richtung = richtung};

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
        std::cout << format->schreibe(treffer.front(), optionen);
        return 0;
    }

    std::optional<Fundstelle> gewaehlt;
    if (!struktur)
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
    if (gewaehlt)
    {
        using namespace c2d::diagramm;
        const auto ablauf = [&](const c2d::modell::Funktion& f) {
            Flussdiagramm d = flussVon(f);
            return roh ? d : fasseAnweisungsfolgenZusammen(d);
        };
        Flussdiagramm d = ablauf(c2d::projekt::funktion(p, *gewaehlt));
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
                    gelungen = neu.gruppen().size() > d.gruppen().size();
                    if (gelungen) d = std::move(neu);
                    break;
                }
            if (gelungen) continue;
            if (!mehrdeutig.empty())
                std::cerr << "c2d: cannot embed '" << name << "': the call is ambiguous (" << orte(p, mehrdeutig) << ")\n";
            else
                std::cerr << "c2d: cannot embed '" << name << "'\n";
        }
        std::cout << format->schreibe(d, optionen);
        return 0;
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
    std::cout << format->schreibe(c2d::diagramm::strukturVon(alle), optionen);
    return 0;
}
