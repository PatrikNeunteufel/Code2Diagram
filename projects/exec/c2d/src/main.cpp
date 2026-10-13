// c2d — Code2Diagram fuer die Kommandozeile.
//
// Aufruf:  c2d <datei> [--liste] [--funktion <name>] [--einbetten <name>]... [--struktur]
//                      [--format <name>] [--richtung TD|LR] [--roh]
//
//   ohne Auswahl   das Diagramm der ersten Funktion mit Rumpf; gibt es keine,
//                  das Strukturdiagramm
//   --liste        nennt, was die Datei enthaelt, und endet
//   --funktion     das Flussdiagramm dieser Funktion (Name allein oder mit Bereich)
//   --einbetten    den Ablauf dieser gerufenen Funktion hinter ihren ersten Aufruf setzen, umrahmt
//   --struktur     das Diagramm der Datentypen
//   -D NAME[=wert] gilt beim Auswerten von #if/#ifdef als definiert (ohne Wert: 1); mehrfach erlaubt
//   --alle-zweige  bedingte Uebersetzung nicht auswerten: alle Zweige lesen
//   --format       mermaid (Vorgabe), dot oder drawio
//   --roh          je Anweisung ein Knoten, nichts gebuendelt
//
// Das Diagramm geht auf die Standardausgabe, Meldungen auf die Fehlerausgabe.
// Exit: 0 gut, 1 Aufruf falsch oder nichts zu zeichnen, 2 Datei nicht lesbar.

#include "c2d/ausgabe/Ausgabe.hpp"
#include "c2d/diagramm/Ableitung.hpp"
#include "c2d/sprache/Sprache.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{

int aufruf()
{
    std::cerr << "usage: c2d <file> [--liste] [--funktion <name>] [--einbetten <name>]... [--struktur]\n"
                 "                  [-D NAME[=value]]... [--alle-zweige]\n"
                 "                  [--format <name>] [--richtung TD|LR] [--roh]\n"
                 "formats:";
    for (const c2d::ausgabe::Format* f : c2d::ausgabe::formate()) std::cerr << ' ' << f->name();
    std::cerr << '\n';
    return 1;
}

}  // namespace

int main(int argc, char* argv[])
{
    std::string datei, funktion;
    std::vector<std::string> einbetten;
    c2d::sprache::LeseOptionen lesen;
    std::string formatName = "mermaid";
    std::string richtung = "TD";
    bool liste = false, struktur = false, roh = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view a = argv[i];
        const auto wert = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--liste") liste = true;
        else if (a == "--struktur") struktur = true;
        else if (a == "--roh") roh = true;
        else if (a == "--funktion") funktion = wert();
        else if (a == "--einbetten") einbetten.push_back(wert());
        else if (a == "--alle-zweige") lesen.alleZweige = true;
        else if (a == "-D") lesen.definitionen.push_back(c2d::sprache::definitionAus(wert()));
        else if (a.starts_with("-D")) lesen.definitionen.push_back(c2d::sprache::definitionAus(a.substr(2)));
        else if (a == "--format") formatName = wert();
        else if (a == "--richtung") richtung = wert();
        else if (a.starts_with("-") || !datei.empty()) return aufruf();
        else datei = a;
    }
    const c2d::ausgabe::Format* format = c2d::ausgabe::format(formatName);
    if (datei.empty() || format == nullptr || (richtung != "TD" && richtung != "LR")) return aufruf();

    std::ifstream strom(datei, std::ios::binary);
    if (!strom)
    {
        std::cerr << "c2d: cannot read " << datei << '\n';
        return 2;
    }
    std::ostringstream inhalt;
    inhalt << strom.rdbuf();
    const std::string text = inhalt.str();

    const c2d::sprache::Sprache* sprache = c2d::sprache::spracheFuer(datei, text);
    if (sprache == nullptr)
    {
        sprache = &c2d::sprache::c();
        std::cerr << "c2d: no reader for this file type - read as " << sprache->name() << '\n';
    }
    // die Einheit heisst wie die Datei, ohne den Weg dorthin
    const std::size_t strich = datei.find_last_of("/\\");
    const c2d::sprache::Ergebnis ergebnis =
        sprache->lies(text, strich == std::string::npos ? datei : datei.substr(strich + 1), lesen);
    for (const c2d::quelle::Meldung& m : ergebnis.meldungen)
        std::cerr << datei << ':' << m.ort.zeile << ':' << m.ort.spalte << ": " << m.text << '\n';
    const c2d::modell::Einheit& einheit = ergebnis.einheit;

    if (liste)
    {
        for (const c2d::modell::Funktion& f : einheit.funktionen)
            std::cout << (f.rumpf ? "function  " : "prototype ") << f.signatur() << '\n';
        for (const c2d::modell::Typ& t : einheit.typen)
            std::cout << "type      " << t.vollerName() << " (" << t.felder.size() << " + " << t.methoden.size()
                      << ")\n";
        for (const c2d::modell::Alias& a : einheit.aliase)
            std::cout << "alias     " << a.name << " = " << a.ziel << '\n';
        for (const c2d::modell::Variable& v : einheit.variablen)
            std::cout << "variable  " << v.typ << ' ' << v.name << v.zusatz << '\n';
        for (const c2d::modell::Makro& m : einheit.makros) std::cout << "macro     " << m.name << '\n';
        for (const c2d::modell::Einbindung& e : einheit.einbindungen)
            std::cout << "include   " << e.ziel << '\n';
        // wonach ein #if gefragt hat und was nirgends definiert ist: mit -D waehlbar
        for (const std::string& name : ergebnis.unbekannteNamen) std::cout << "undefined " << name << '\n';
        return 0;
    }

    const c2d::ausgabe::Optionen optionen{.richtung = richtung};
    const c2d::modell::Funktion* gewaehlt = nullptr;
    if (!struktur)
    {
        for (const c2d::modell::Funktion& f : einheit.funktionen)
        {
            // der Name allein oder mit Bereich: `starte` oder `antrieb::Motor::starte`
            if (!f.rumpf || (!funktion.empty() && f.name != funktion && f.vollerName() != funktion)) continue;
            gewaehlt = &f;
            break;
        }
        if (gewaehlt == nullptr && !funktion.empty())
        {
            std::cerr << "c2d: no function '" << funktion << "' with a body in " << datei << '\n';
            return 1;
        }
    }
    if (gewaehlt != nullptr)
    {
        using namespace c2d::diagramm;
        const auto ablauf = [&](const c2d::modell::Funktion& f) {
            Flussdiagramm d = flussVon(f);
            return roh ? d : fasseAnweisungsfolgenZusammen(d);
        };
        Flussdiagramm d = ablauf(*gewaehlt);
        // --einbetten: hinter den ersten Knoten, der die Funktion ruft und noch keine Einbettung traegt
        for (const std::string& name : einbetten)
        {
            const c2d::modell::Funktion* gerufen = nullptr;
            for (const c2d::modell::Funktion& f : einheit.funktionen)
                if (f.rumpf && f.name == name && gerufen == nullptr) gerufen = &f;
            bool gelungen = false;
            for (KnotenId i = 0; gerufen != nullptr && !gelungen && i < d.knoten().size(); ++i)
            {
                const std::vector<std::string>& aufrufe = d.knoten()[i].aufrufe;
                if (std::find(aufrufe.begin(), aufrufe.end(), name) == aufrufe.end()) continue;
                Flussdiagramm neu = betteEin(d, i, ablauf(*gerufen), gerufen->signatur());
                gelungen = neu.gruppen().size() > d.gruppen().size();
                if (gelungen) d = std::move(neu);
            }
            if (!gelungen) std::cerr << "c2d: cannot embed '" << name << "'\n";
        }
        std::cout << format->schreibe(d, optionen);
        return 0;
    }
    if (einheit.typen.empty())
    {
        std::cerr << "c2d: nothing to draw in " << datei << '\n';
        return 1;
    }
    std::cout << format->schreibe(c2d::diagramm::strukturVon(einheit), optionen);
    return 0;
}
