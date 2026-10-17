/**
 * @file StruktoAnordnung.cpp
 * @brief Ein Struktogramm angeordnet: Bloecke als Rechtecke, Koepfe mit Diagonalen, Spalten nach Inhalt
 *
 * Zwei Durchgaenge: von unten nach oben misst jeder Block, wie breit er mindestens
 * ist (sein Text, die Summe seiner Spalten) und wie hoch (Hoehen haengen nicht
 * von der Breite ab — lange Zeilen werden gekuerzt, nicht umgebrochen); von oben
 * nach unten bekommt er seine Breite, und eine Verzweigung teilt den Ueberschuss
 * nach den Mindestbreiten ihrer Spalten auf, nicht haelftig.
 */
#include "c2d/diagramm/Anordnung.hpp"

#include <algorithm>
#include <map>

namespace c2d::diagramm
{

namespace
{

constexpr double kRand = 24;          // um das ganze Diagramm, wie im Ablauf
constexpr double kLuft = 8;           // links und rechts neben einem Text
constexpr double kEinzug = 20;        // die Leiste links neben dem Rumpf einer Schleife
constexpr double kLeer = 30;          // so breit ist eine leere Spalte mindestens
constexpr double kPfeil = 14;         // Platz fuer den Pfeil eines Aussprungs
constexpr double kSeite = 8;          // die doppelten Seiten eines Aufrufs
constexpr double kRahmenRand = 4;     // zwischen dem Rahmen eines bedingten Bereichs und seinem Inhalt
constexpr double kLegendenFeld = 12;  // das Farbfeld einer Zeile der Legende

using Feld = StruktoAnordnung::Feld;
using FeldArt = StruktoAnordnung::FeldArt;

struct Mass
{
    double breite = 0;
    double hoehe = 0;
};

class Ordner
{
public:
    Ordner(const AnordnungOptionen& o, StruktoAnordnung& a) : m_o(o), m_a(a), m_zeile(o.zeilenHoehe + 8) {}

    [[nodiscard]] double textBreite(const std::string& text) const
    {
        const std::size_t n = m_o.maxZeilenLaenge == 0 ? text.size() : std::min(text.size(), m_o.maxZeilenLaenge);
        return static_cast<double>(n) * m_o.zeichenBreite + 2 * kLuft;
    }

    [[nodiscard]] double zeile() const { return m_zeile; }

    /// Wie hoch das Dreieck eines Kopfs ist: oben der Text; bei if unten in den Ecken yes und no.
    [[nodiscard]] double dreieck(const Block& b) const
    {
        return b.art == BlockArt::Verzweigung ? 3 * m_o.zeilenHoehe + 8 : 2 * m_o.zeilenHoehe + 8;
    }

    /// Die Zeile der Fallwerte unter dem Dreieck (switch, Varianten): je Spalte ein Feld.
    [[nodiscard]] double werteZeile(const Block& b) const
    {
        return b.art == BlockArt::Verzweigung ? 0 : m_o.zeilenHoehe + 6;
    }

    /// Ein Block mit Spalten: wie hoch sein Kopf ist.
    [[nodiscard]] double kopf(const Block& b) const
    {
        switch (b.art)
        {
            case BlockArt::Verzweigung:
            case BlockArt::Fallunterscheidung:
            case BlockArt::Varianten: return dreieck(b) + werteZeile(b);
            case BlockArt::Bedingt: return m_o.zeilenHoehe + 4;
            default: return m_zeile;
        }
    }

    /// Das Dreieck verengt sich nach unten: unter der ersten Zeile ist es um diesen Anteil schmaler.
    [[nodiscard]] double verengt(const Block& b) const { return (4 + m_o.zeilenHoehe) / dreieck(b); }

    /// So breit wird eine Spalte unter einem Kopf mindestens: ihr Inhalt, ihre Beschriftung —
    /// bei if steht die in der Ecke des Dreiecks und darf seine Diagonale nicht schneiden.
    [[nodiscard]] double spaltenBreite(const Block& b, const Spalte& s) const
    {
        const double inhalt = folge(s.bloecke).breite;
        if (b.art != BlockArt::Verzweigung) return std::max(inhalt, textBreite(s.titel));
        const double unten = m_o.zeilenHoehe + 2;  // die Beschriftung steht in der untersten Zeile
        const double schild = textBreite(s.titel) - kLuft;
        return std::max(inhalt, schild * dreieck(b) / (dreieck(b) - unten));
    }

    [[nodiscard]] Mass folge(const std::vector<Block>& bloecke) const
    {
        if (bloecke.empty()) return {kLeer, m_zeile};
        Mass m;
        for (const Block& b : bloecke)
        {
            const Mass x = block(b);
            m.breite = std::max(m.breite, x.breite);
            m.hoehe += x.hoehe;
        }
        return m;
    }


    [[nodiscard]] Mass block(const Block& b) const
    {
        const double text = textBreite(b.text);
        switch (b.art)
        {
            case BlockArt::Anweisung:
            case BlockArt::Marke: return {text, m_zeile};
            case BlockArt::Aufruf: return {text + 2 * kSeite, m_zeile};
            case BlockArt::Aussprung: return {text + kPfeil, m_zeile};
            case BlockArt::Verzweigung:
            case BlockArt::Fallunterscheidung:
            case BlockArt::Varianten:
            {
                double summe = 0;
                double hoch = 0;
                for (const Spalte& s : b.spalten)
                {
                    summe += spaltenBreite(b, s);
                    hoch = std::max(hoch, folge(s.bloecke).hoehe);
                }
                // der Text passt unter der Spitze zwischen die Diagonalen
                return {std::max(text / (1 - verengt(b)), summe), kopf(b) + hoch};
            }
            case BlockArt::Kopfschleife:
            case BlockArt::Endlosschleife:
            case BlockArt::Fussschleife:
            {
                const Mass rumpf = b.spalten.empty() ? Mass{kLeer, m_zeile} : folge(b.spalten[0].bloecke);
                return {std::max(text, kEinzug + rumpf.breite), m_zeile + rumpf.hoehe};
            }
            case BlockArt::Versuch:
            case BlockArt::Abschnitt:
            {
                Mass m;
                for (const Spalte& s : b.spalten)
                {
                    const Mass rumpf = folge(s.bloecke);
                    m.breite = std::max({m.breite, textBreite(s.titel), kEinzug + rumpf.breite});
                    m.hoehe += m_zeile + rumpf.hoehe;
                }
                return m;
            }
            case BlockArt::Bedingt:
            {
                const Mass rumpf = b.spalten.empty() ? Mass{kLeer, m_zeile} : folge(b.spalten[0].bloecke);
                return {rumpf.breite + 2 * kRahmenRand, kopf(b) + rumpf.hoehe + kRahmenRand};
            }
        }
        return {text, m_zeile};
    }

    /// Die Bloecke untereinander in (x, y, breite); was unter dem letzten bis `hoehe` frei bleibt, ein leeres Feld.
    void legeFolge(const std::vector<Block>& bloecke, double x, double y, double breite, double hoehe)
    {
        double unten = y;
        for (const Block& b : bloecke)
        {
            const double h = block(b).hoehe;
            legeBlock(b, x, unten, breite, h);
            unten += h;
        }
        if (y + hoehe - unten > 0.5) leer({x, unten, breite, y + hoehe - unten});
    }

    void legeBlock(const Block& b, double x, double y, double w, double h)
    {
        const std::size_t id = m_a.bloecke.size();
        m_a.bloecke.push_back({x, y, w, h});
        Feld f{{x, y, w, h}, FeldArt::Anweisung, {b.text}, {x + w / 2, y + h / 2}, id, b.unerreichbar};
        switch (b.art)
        {
            case BlockArt::Anweisung:
            case BlockArt::Marke: m_a.felder.push_back(std::move(f)); return;
            case BlockArt::Aufruf:
                f.art = FeldArt::Aufruf;
                m_a.felder.push_back(std::move(f));
                m_a.linien.push_back({{x + kSeite, y}, {x + kSeite, y + h}});
                m_a.linien.push_back({{x + w - kSeite, y}, {x + w - kSeite, y + h}});
                return;
            case BlockArt::Aussprung:
                // DIN 66261: ein Pfeil nach links — hinaus
                f.art = FeldArt::Aussprung;
                f.text = {x + kPfeil + (w - kPfeil) / 2, y + h / 2};
                m_a.felder.push_back(std::move(f));
                m_a.linien.push_back({{x + kPfeil - 2, y + 4}, {x + 4, y + h / 2}, {x + kPfeil - 2, y + h - 4}});
                return;
            case BlockArt::Verzweigung:
            case BlockArt::Fallunterscheidung:
            case BlockArt::Varianten: spalten(b, f, x, y, w, h); return;
            case BlockArt::Kopfschleife:
            case BlockArt::Endlosschleife:
                f.art = FeldArt::Kopf;
                f.text = {x + w / 2, y + m_zeile / 2};
                m_a.felder.push_back(std::move(f));
                legeFolge(rumpf(b), x + kEinzug, y + m_zeile, w - kEinzug, h - m_zeile);
                return;
            case BlockArt::Fussschleife:
                f.art = FeldArt::Kopf;
                f.text = {x + w / 2, y + h - m_zeile / 2};
                m_a.felder.push_back(std::move(f));
                legeFolge(rumpf(b), x + kEinzug, y, w - kEinzug, h - m_zeile);
                return;
            case BlockArt::Versuch:
            case BlockArt::Abschnitt:
            {
                f.art = FeldArt::Kopf;
                f.zeilen.clear();  // die Koepfe der Abschnitte stehen als Beschriftungen
                m_a.felder.push_back(std::move(f));
                double oben = y;
                for (std::size_t i = 0; i < b.spalten.size(); ++i)
                {
                    const Spalte& s = b.spalten[i];
                    if (i > 0) m_a.linien.push_back({{x, oben}, {x + w, oben}});
                    m_a.beschriftungen.push_back({{x + w / 2, oben + m_zeile / 2}, s.titel});
                    const double hoch = folge(s.bloecke).hoehe;
                    legeFolge(s.bloecke, x + kEinzug, oben + m_zeile, w - kEinzug, hoch);
                    oben += m_zeile + hoch;
                }
                return;
            }
            case BlockArt::Bedingt:
            {
                f.art = FeldArt::Kopf;
                f.zeilen.clear();  // der Titel steht am Rahmen
                m_a.felder.push_back(std::move(f));
                const auto [stelle, neu] = m_farben.emplace(b.text, m_farben.size());
                m_a.rahmen.push_back({{x, y, w, h}, b.text, stelle->second});
                const double k = kopf(b);
                legeFolge(rumpf(b), x + kRahmenRand, y + k, w - 2 * kRahmenRand, h - k - kRahmenRand);
                return;
            }
        }
    }

    /// Die Bedingungen der Rahmen in der Reihenfolge, in der sie zuerst vorkommen.
    [[nodiscard]] std::vector<std::pair<std::string, std::size_t>> farbzeilen() const
    {
        std::vector<std::pair<std::string, std::size_t>> aus(m_farben.size());
        for (const auto& [titel, farbe] : m_farben) aus[farbe] = {titel, farbe};
        return aus;
    }

    void leer(const Rechteck& r) { m_a.felder.push_back({r, FeldArt::Leer, {}, r.mitte(), kKeine, false}); }

private:
    static const std::vector<Block>& rumpf(const Block& b)
    {
        static const std::vector<Block> kLeerFolge;
        return b.spalten.empty() ? kLeerFolge : b.spalten[0].bloecke;
    }

    /// Ein Kopf mit Diagonalen und darunter die Spalten nebeneinander.
    void spalten(const Block& b, Feld f, double x, double y, double w, double h)
    {
        const double k = kopf(b);
        const double d = dreieck(b);
        f.art = FeldArt::Kopf;
        m_a.felder.push_back(std::move(f));
        Feld& kopfFeld = m_a.felder.back();
        if (b.spalten.empty())
        {
            kopfFeld.text = {x + w / 2, y + 4 + m_o.zeilenHoehe / 2};
            return;
        }

        // den Ueberschuss nach den Mindestbreiten verteilen: der breitere Zweig bekommt mehr
        std::vector<double> breiten;
        double summe = 0;
        for (const Spalte& s : b.spalten)
        {
            breiten.push_back(spaltenBreite(b, s));
            summe += breiten.back();
        }
        double links = x;
        std::vector<double> lagen;  // linker Rand je Spalte
        for (std::size_t i = 0; i < breiten.size(); ++i)
        {
            breiten[i] = i + 1 == breiten.size() ? x + w - links : breiten[i] + (w - summe) * breiten[i] / summe;
            lagen.push_back(links);
            links += breiten[i];
        }
        const double spitze = y + d;  // wo sich die Diagonalen treffen
        const double unten = y + k;   // wo die Spalten beginnen
        // die Diagonalen treffen sich am linken Rand der letzten Spalte (bei if: zwischen yes und no)
        const double treffen = b.art == BlockArt::Verzweigung && lagen.size() > 1 ? lagen[1] : lagen.back();
        if (treffen > x) m_a.linien.push_back({{x, y}, {treffen, spitze}});
        m_a.linien.push_back({{x + w, y}, {treffen, spitze}});
        for (std::size_t i = 1; i < lagen.size(); ++i)
            if (lagen[i] < treffen)  // ein Trennstrich von der Diagonale nach unten
                m_a.linien.push_back({{lagen[i], y + d * (lagen[i] - x) / (treffen - x)}, {lagen[i], spitze}});
        // der Text oben, mittig zwischen den Diagonalen unter seiner Zeile
        const double v = verengt(b);
        const double textLinks = x + (treffen - x) * v;
        const double textRechts = x + w - (x + w - treffen) * v;
        kopfFeld.text = {(textLinks + textRechts) / 2, y + 4 + m_o.zeilenHoehe / 2};

        if (b.art != BlockArt::Verzweigung)
        {
            // die Zeile der Werte: quer ein Strich, je Spalte ein Feld
            m_a.linien.push_back({{x, spitze}, {x + w, spitze}});
            for (std::size_t i = 1; i < lagen.size(); ++i) m_a.linien.push_back({{lagen[i], spitze}, {lagen[i], unten}});
        }

        for (std::size_t i = 0; i < b.spalten.size(); ++i)
        {
            const Spalte& s = b.spalten[i];
            if (b.art == BlockArt::Verzweigung)
            {
                // yes links unten, no rechts unten — in ihren Ecken, unter der Diagonale
                const double schild = textBreite(s.titel) - kLuft;
                const double mitte = i == 0 ? x + 4 + schild / 2 : x + w - 4 - schild / 2;
                m_a.beschriftungen.push_back({{mitte, unten - m_o.zeilenHoehe / 2 - 1}, s.titel});
            }
            else
                m_a.beschriftungen.push_back({{lagen[i] + breiten[i] / 2, spitze + (k - d) / 2}, s.titel});
            legeFolge(s.bloecke, lagen[i], unten, breiten[i], h - k);
        }
    }

    const AnordnungOptionen& m_o;
    StruktoAnordnung& m_a;
    double m_zeile;
    std::map<std::string, std::size_t> m_farben;  // Bedingung eines Rahmens -> Farbe
};

}  // namespace

StruktoAnordnung ordneAn(const Struktogramm& diagramm, const AnordnungOptionen& optionen)
{
    StruktoAnordnung a;
    Ordner o(optionen, a);

    // oben die Signatur; kehrt die Funktion nie zurueck, steht es darunter (wie im Ablauf)
    std::vector<std::string> titel{diagramm.titel};
    if (diagramm.kehrtNieZurueck) titel.emplace_back("(never returns)");
    double titelBreite = 0;
    for (const std::string& z : titel) titelBreite = std::max(titelBreite, o.textBreite(z));
    const double titelHoehe = static_cast<double>(titel.size()) * optionen.zeilenHoehe + 8;

    const Mass rumpf = o.folge(diagramm.bloecke);
    const double breite = std::max(titelBreite, rumpf.breite);
    const Rechteck kopf{kRand, kRand, breite, titelHoehe};
    a.felder.push_back({kopf, FeldArt::Titel, titel, kopf.mitte(), kKeine, false});
    o.legeFolge(diagramm.bloecke, kRand, kRand + titelHoehe, breite, rumpf.hoehe);

    a.breite = kRand + breite + kRand;
    a.hoehe = kRand + titelHoehe + rumpf.hoehe + kRand;

    // die Legende der Rahmen: rechts daneben, oben — wie im Ablauf
    const double links = a.breite;
    double y = kRand;
    for (const auto& [bedingung, farbe] : o.farbzeilen())
    {
        const std::size_t zeichen =
            optionen.maxZeilenLaenge == 0 ? bedingung.size() : std::min(bedingung.size(), optionen.maxZeilenLaenge);
        FlussAnordnung::Legende eintrag{{links, y + (optionen.zeilenHoehe - kLegendenFeld) / 2, kLegendenFeld, kLegendenFeld},
                                        {links + kLegendenFeld + 6, y + optionen.zeilenHoehe / 2},
                                        bedingung,
                                        farbe};
        a.breite = std::max(a.breite, eintrag.text.x + static_cast<double>(zeichen) * optionen.zeichenBreite + kRand);
        a.legende.push_back(std::move(eintrag));
        y += optionen.zeilenHoehe + 4;
    }
    a.hoehe = std::max(a.hoehe, y + kRand);
    return a;
}

}  // namespace c2d::diagramm
