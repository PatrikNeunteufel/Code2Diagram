/**
 * @file Vorschau.cpp
 * @brief Das Diagramm gezeichnet: Formen je Knotenart, Kanten mit Pfeil, Rahmen, Kaesten
 */
#include "pch.h"
#include "ui/Vorschau.hpp"

#include "c2d/diagramm/Anordnung.hpp"
#include "c2d/diagramm/Farben.hpp"

#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QFontDatabase>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QTextDocument>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace c2d::ui
{

namespace
{

using namespace c2d::diagramm;

// Woran ein gezeichnetes Stueck haengt: Kennung + 1, damit 0 "nichts" heisst.
constexpr int kKnoten = 0;
constexpr int kGruppe = 1;
constexpr int kKasten = 2;
constexpr int kEintrag = 3;
constexpr int kTabelle = 4;  // das Tabellensymbol eines Knotens

constexpr qsizetype kMaxZeichen = 60;  // wie AnordnungOptionen::maxZeilenLaenge

QString gekuerzt(const std::string& zeile)
{
    const QString text = QString::fromStdString(zeile);
    return text.size() <= kMaxZeichen ? text : text.left(kMaxZeichen - 3) + QStringLiteral("...");
}

QRectF alsRect(const Rechteck& r)
{
    return {r.x, r.y, r.breite, r.hoehe};
}

QPointF alsPunkt(const Punkt& p)
{
    return {p.x, p.y};
}

/// Die Farbe einer Bedingung (rahmenFarbe); kKeine: keine (ungueltig).
QColor farbeVon(std::size_t farbe)
{
    if (farbe == kKeine) return {};
    const std::string_view f = rahmenFarbe(farbe);
    return QColor(QString::fromLatin1(f.data(), static_cast<qsizetype>(f.size())));
}

/// Eine Farbe der Tabelle (Farben.hpp), hell oder dunkel.
QColor ausTabelle(Farbe welche, bool dunkel)
{
    const std::string_view f = farbe(welche, dunkel);
    return QColor(QString::fromLatin1(f.data(), static_cast<qsizetype>(f.size())));
}

/// Zeichnet auf eine Szene, mit den Farben der Tabelle — dieselben wie in der SVG-Datei.
class Zeichner
{
public:
    Zeichner(QGraphicsScene& szene, bool dunkel) : m_szene(szene)
    {
        m_schrift = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        m_schrift.setPixelSize(12);
        m_strich = QPen(ausTabelle(Farbe::Strich, dunkel), 1.2);
        m_text = ausTabelle(Farbe::Text, dunkel);
        m_flaeche = ausTabelle(Farbe::Flaeche, dunkel);
        m_getoent = ausTabelle(Farbe::Getoent, dunkel);
        m_fremd = ausTabelle(Farbe::Fremd, dunkel);
        m_fremdStrich = ausTabelle(Farbe::FremdStrich, dunkel);
        m_verweis = QPen(ausTabelle(Farbe::Verweis, dunkel), 2.2);
        m_rahmen = QPen(ausTabelle(Farbe::Rahmen, dunkel), 1.2, Qt::DashLine);
    }

    /// Text, mittig im Rechteck.
    QGraphicsSimpleTextItem* text(const QString& inhalt, const QRectF& in, bool fett = false, bool kursiv = false)
    {
        QFont schrift = m_schrift;
        schrift.setBold(fett);
        schrift.setItalic(kursiv);
        QGraphicsSimpleTextItem* stueck = m_szene.addSimpleText(inhalt, schrift);
        stueck->setBrush(m_text);
        const QRectF masse = stueck->boundingRect();
        stueck->setPos(in.center().x() - masse.width() / 2, in.center().y() - masse.height() / 2);
        stueck->setZValue(3);
        return stueck;
    }

    QGraphicsSimpleTextItem* textLinks(const QString& inhalt, QPointF ort, bool kursiv = false)
    {
        QFont schrift = m_schrift;
        schrift.setItalic(kursiv);
        QGraphicsSimpleTextItem* stueck = m_szene.addSimpleText(inhalt, schrift);
        stueck->setBrush(m_text);
        stueck->setPos(ort);
        stueck->setZValue(3);
        return stueck;
    }

    void knoten(KnotenId id, const Knoten& k, const QRectF& r, const std::vector<std::string>& links)
    {
        const bool verweis = !links.empty();
        QPainterPath form;
        switch (k.art)
        {
            case KnotenArt::Start:
            case KnotenArt::Ende:
                form.addRoundedRect(r, r.height() / 2, r.height() / 2);
                break;
            case KnotenArt::Verzweigung:
                form.moveTo(r.center().x(), r.top());
                form.lineTo(r.right(), r.center().y());
                form.lineTo(r.center().x(), r.bottom());
                form.lineTo(r.left(), r.center().y());
                form.closeSubpath();
                break;
            case KnotenArt::Aufruf:  // ein Rechteck mit doppelten Seiten
                form.addRect(r);
                form.moveTo(r.left() + 8, r.top());
                form.lineTo(r.left() + 8, r.bottom());
                form.moveTo(r.right() - 8, r.top());
                form.lineTo(r.right() - 8, r.bottom());
                break;
            case KnotenArt::Variante:
            {
                const std::vector<c2d::diagramm::Punkt> ecken =
                    c2d::diagramm::sechseck({r.x(), r.y(), r.width(), r.height()});
                QPolygonF vieleck;
                for (const c2d::diagramm::Punkt& p : ecken) vieleck << QPointF(p.x, p.y);
                form.addPolygon(vieleck);
                form.closeSubpath();
                break;
            }
            case KnotenArt::Anweisung:
                form.addRect(r);
                break;
        }
        const bool rand = k.art == KnotenArt::Start || k.art == KnotenArt::Ende;
        QPen stift = verweis ? m_verweis : m_strich;
        QColor pinsel = rand ? m_getoent : m_flaeche;
        if (k.unerreichbar)
        {
            // nie erreicht: grau und gestrichelt, wie ein Aufruf ohne Rumpf
            stift = QPen(m_fremdStrich, 1.2, Qt::DashLine);
            pinsel = m_fremd;
        }
        QGraphicsPathItem* stueck = m_szene.addPath(form, stift, pinsel);
        stueck->setData(kKnoten, static_cast<qulonglong>(id) + 1);
        stueck->setZValue(2);
        if (verweis) stueck->setToolTip(QStringLiteral("Click: show in the source. Double-click: go to the function or type it names. Right-click: more."));

        QStringList zeilen;
        for (const std::string& z : k.zeilen) zeilen << gekuerzt(z);
        // der Text gehoert zum Knoten: ein Klick auf ihn trifft den Knoten
        if (!verweis)
        {
            text(zeilen.join(QLatin1Char('\n')), r)->setData(kKnoten, static_cast<qulonglong>(id) + 1);
            return;
        }
        // je Zeile mittig, die Namen, die fuehren, hervorgehoben
        const double hoehe = QFontMetricsF(m_schrift).lineSpacing();
        const double oben = r.center().y() - hoehe * static_cast<double>(zeilen.size()) / 2;
        for (qsizetype i = 0; i < zeilen.size(); ++i)
        {
            QGraphicsTextItem* stueck = linkZeile(zeilen[i], links, {});
            stueck->setPos(r.center().x() - stueck->boundingRect().width() / 2, oben + hoehe * static_cast<double>(i));
            stueck->setData(kKnoten, static_cast<qulonglong>(id) + 1);
        }
    }

    /// Ein Knoten des Aufrufgraphen: eine Funktion wie ein Aufruf im Ablauf (der Ausgangspunkt
    /// getoent und kraeftig umrandet), Fremd grau und kursiv, ein Makro dazu gestrichelt.
    void aufrufKnoten(KnotenId id, const AufrufKnoten& k, const QRectF& r)
    {
        QPainterPath form;
        form.addRect(r);
        QPen stift = m_strich;
        QColor pinsel = m_flaeche;
        const bool funktion = k.art == AufrufKnotenArt::Funktion;
        if (funktion)
        {
            form.moveTo(r.left() + 8, r.top());
            form.lineTo(r.left() + 8, r.bottom());
            form.moveTo(r.right() - 8, r.top());
            form.lineTo(r.right() - 8, r.bottom());
            if (k.wurzel)
            {
                stift.setWidthF(2.4);
                pinsel = m_getoent;
            }
        }
        else
        {
            stift = QPen(m_fremdStrich, 1.2, k.art == AufrufKnotenArt::Makro ? Qt::DashLine : Qt::SolidLine);
            pinsel = m_fremd;
        }
        QGraphicsPathItem* stueck = m_szene.addPath(form, stift, pinsel);
        stueck->setData(kKnoten, static_cast<qulonglong>(id) + 1);
        stueck->setZValue(2);
        stueck->setToolTip(funktion ? QStringLiteral("Click: show in the source. Double-click: show its flow. Right-click: more.")
                                    : k.art == AufrufKnotenArt::Makro ? QStringLiteral("A macro of the project. Click: show in the source.")
                                                                       : QStringLiteral("No body in the project"));
        text(gekuerzt(knotenText(k)), r, false, !funktion)->setData(kKnoten, static_cast<qulonglong>(id) + 1);
    }

    /// Ein Zustand: ein Kasten mit runden Ecken; der Punkt von aussen: ein gefuellter Kreis.
    void zustand(const Zustand& z, const QRectF& r)
    {
        if (z.aussen)
        {
            const double d = std::min(r.width(), r.height());
            m_szene.addEllipse(QRectF(r.center().x() - d / 2, r.center().y() - d / 2, d, d), m_strich, m_strich.color())
                ->setZValue(2);
            return;
        }
        QPainterPath form;
        form.addRoundedRect(r, 10, 10);
        m_szene.addPath(form, m_strich, m_flaeche)->setZValue(2);
        text(gekuerzt(z.name), r);
    }

    /// Ein Linienzug mit Pfeilspitze am Ende; `inhalt` steht an `ort`. `stift` gueltig
    /// (Breite > 0): so gezogen — gestrichelt, gepunktet, in einer Farbe.
    void kante(const Linienzug& linie, const QString& inhalt, const Punkt& ort, const QPen& stift = QPen(Qt::NoPen))
    {
        if (linie.size() < 2) return;
        const QPen& zug = stift.style() == Qt::NoPen ? m_strich : stift;
        QPainterPath weg(alsPunkt(linie[0]));
        for (std::size_t i = 1; i < linie.size(); ++i) weg.lineTo(alsPunkt(linie[i]));
        m_szene.addPath(weg, zug)->setZValue(1);
        spitze(alsPunkt(linie[linie.size() - 2]), alsPunkt(linie.back()), true, 9, zug.color());

        if (inhalt.isEmpty()) return;
        // die Beschriftung auf einem Feld in der Farbe des Grunds: die Linie laeuft nicht durch die Schrift
        QGraphicsSimpleTextItem* schild = textLinks(inhalt, {});
        const QRectF masse = schild->boundingRect();
        schild->setPos(ort.x - masse.width() / 2, ort.y - masse.height() / 2);
        schild->setZValue(5);
        m_szene.addRect(schild->sceneBoundingRect().adjusted(-2, 0, 2, 0), Qt::NoPen, m_flaeche)->setZValue(4);
    }

    /// Ein Dreieck am Punkt `an`, das von `von` her zeigt; gefuellt oder hohl, in `farbe` (ungueltig: die des Strichs).
    void spitze(QPointF von, QPointF an, bool gefuellt, double laenge = 9, QColor farbe = QColor())
    {
        if (!farbe.isValid()) farbe = m_strich.color();
        const double winkel = std::atan2(an.y() - von.y(), an.x() - von.x());
        const auto ecke = [&](double drehung) {
            return QPointF(an.x() - laenge * std::cos(winkel + drehung), an.y() - laenge * std::sin(winkel + drehung));
        };
        QPolygonF dreieck;
        dreieck << an << ecke(0.42) << ecke(-0.42);
        m_szene.addPolygon(dreieck, QPen(farbe, m_strich.widthF()), gefuellt ? QBrush(farbe) : QBrush(m_flaeche))->setZValue(2);
    }

    /// Eine Raute am Punkt `an`, auf der Linie nach `nach` (der Besitzer einer Komposition).
    void raute(QPointF an, QPointF nach)
    {
        const double winkel = std::atan2(nach.y() - an.y(), nach.x() - an.x());
        const auto auf = [&](double weit, double seitlich) {
            return QPointF(an.x() + weit * std::cos(winkel) - seitlich * std::sin(winkel),
                           an.y() + weit * std::sin(winkel) + seitlich * std::cos(winkel));
        };
        QPolygonF form;
        form << an << auf(8, 5) << auf(16, 0) << auf(8, -5);
        m_szene.addPolygon(form, m_strich, m_strich.color())->setZValue(2);
    }

    /// Ein Rahmen; `farbe` gueltig: der Rahmen einer Bedingung — Rand und Titel in ihrer Farbe, die Flaeche ein Hauch davon.
    void rahmen(std::size_t gruppe, const QString& titel, const QRectF& r, const QColor& farbe, const QString& ganz)
    {
        QPen stift = m_rahmen;
        QBrush pinsel(Qt::NoBrush);
        if (farbe.isValid())
        {
            stift = QPen(farbe, 1.4);
            pinsel = QBrush(hauch(farbe));
        }
        QGraphicsRectItem* stueck = m_szene.addRect(r, stift, pinsel);
        stueck->setData(kGruppe, static_cast<qulonglong>(gruppe) + 1);
        stueck->setZValue(0);
        QGraphicsSimpleTextItem* kopf = textLinks(titel, r.topLeft() + QPointF(8, 4), true);
        kopf->setData(kGruppe, static_cast<qulonglong>(gruppe) + 1);
        kopf->setToolTip(ganz);
        if (farbe.isValid()) kopf->setBrush(farbe);
        // auf einem Schild: eine Kante, die in den Rahmen laeuft, geht hinter dem Titel durch
        m_szene.addRect(kopf->sceneBoundingRect().adjusted(-3, 0, 3, 0), Qt::NoPen, m_flaeche)->setZValue(1.5);
    }

    /// Eine Zeile der Legende: ein Farbfeld, dahinter die Bedingung.
    void legende(const QRectF& feld, const QColor& farbe, const QString& text, QPointF ort)
    {
        m_szene.addRect(feld, QPen(farbe, 1.4), QBrush(hauch(farbe)))->setZValue(1);
        QGraphicsSimpleTextItem* stueck = textLinks(text, ort);
        stueck->setPos(ort.x(), ort.y() - stueck->boundingRect().height() / 2);
    }

    [[nodiscard]] static QColor hauch(QColor farbe)
    {
        farbe.setAlphaF(0.10);
        return farbe;
    }

    /// Ein Kasten; `links[e]`: die Namen in Glied e, die zu einem Typ fuehren (hervorgehoben).
    void kasten(std::size_t id, const Kasten& k, const QRectF& r, double kopf, double zeilenHoehe,
                const std::vector<std::vector<std::string>>& links)
    {
        const auto merke = [&](QGraphicsItem* stueck, std::size_t eintrag) {
            stueck->setData(kKasten, static_cast<qulonglong>(id) + 1);
            stueck->setData(kEintrag, eintrag == kKeine ? 0ULL : static_cast<qulonglong>(eintrag) + 1);
        };
        QGraphicsRectItem* rumpf = m_szene.addRect(r, m_strich, m_flaeche);
        rumpf->setZValue(2);
        merke(rumpf, kKeine);
        QGraphicsRectItem* kopfFeld = m_szene.addRect(QRectF(r.left(), r.top(), r.width(), kopf), m_strich, m_getoent);
        kopfFeld->setZValue(2);
        merke(kopfFeld, kKeine);
        QString art;
        switch (k.art)
        {
            // in Guillemets (U+00AB, U+00BB), als Bytes: die Datei bleibt reines ASCII
            case KastenArt::Struktur: art = QString::fromUtf8("\xC2\xAB" "struct" "\xC2\xBB"); break;
            case KastenArt::Vereinigung: art = QString::fromUtf8("\xC2\xAB" "union" "\xC2\xBB"); break;
            case KastenArt::Aufzaehlung: art = QString::fromUtf8("\xC2\xAB" "enumeration" "\xC2\xBB"); break;
            case KastenArt::Klasse: break;
        }
        const QRectF oben(r.left(), r.top(), r.width(), kopf);
        if (art.isEmpty())
            text(gekuerzt(k.name), oben, true);
        else
        {
            text(art, QRectF(oben.left(), oben.top() + 2, oben.width(), kopf / 2), false, true);
            text(gekuerzt(k.name), QRectF(oben.left(), oben.top() + kopf / 2 - 2, oben.width(), kopf / 2), true);
        }

        double y = r.top() + kopf + 5;
        // ein unsichtbares Feld ueber die ganze Zeile: ein Klick trifft das Glied, auch neben dem Text
        const auto feldUeber = [&](std::size_t eintrag) {
            QGraphicsRectItem* feld = m_szene.addRect(QRectF(r.left() + 1, y, r.width() - 2, zeilenHoehe), Qt::NoPen, Qt::NoBrush);
            feld->setZValue(2.5);
            merke(feld, eintrag);
            return feld;
        };
        for (std::size_t e = 0; e < k.eintraege.size(); ++e)
        {
            const Eintrag& eintrag = k.eintraege[e];
            const QString vorn = eintrag.sicht == '\0' ? QString() : QString(QLatin1Char(eintrag.sicht)) + QLatin1Char(' ');
            const QString inhalt = vorn + gekuerzt(eintrag.text());
            QGraphicsRectItem* feld = feldUeber(e);
            if (e < links.size() && !links[e].empty())
            {
                QGraphicsTextItem* stueck = linkZeile(inhalt, links[e], QPointF(r.left() + 8, y));
                merke(stueck, e);
                feld->setToolTip(QStringLiteral("Click: show in the source. Double-click or right-click: go to the type."));
            }
            else
                merke(textLinks(inhalt, QPointF(r.left() + 8, y)), e);
            y += zeilenHoehe;
        }
        if (k.methoden.empty()) return;
        y += 3;
        m_szene.addLine(r.left(), y, r.right(), y, m_strich)->setZValue(2);
        y += 5;
        // die Methoden zaehlen hinter den Feldern weiter
        for (std::size_t m = 0; m < k.methoden.size(); ++m)
        {
            const Methode& methode = k.methoden[m];
            const QString vorn = methode.sicht == '\0' ? QString() : QString(QLatin1Char(methode.sicht)) + QLatin1Char(' ');
            feldUeber(k.eintraege.size() + m);
            // abstrakt: kursiv, wie in UML
            merke(textLinks(vorn + gekuerzt(methode.text()), QPointF(r.left() + 8, y), methode.abstrakt),
                  k.eintraege.size() + m);
            y += zeilenHoehe;
        }
    }

    /// Eine Zeile, in der die Namen `links` (ganze Woerter) als Link hervorgehoben sind.
    /// Ein Feld eines Struktogramms: sein Rechteck, sein Text — die Namen in `links` hervorgehoben.
    void struktoFeld(const StruktoAnordnung::Feld& f, const std::vector<std::string>& links)
    {
        using FeldArt = StruktoAnordnung::FeldArt;
        const QRectF r(f.ort.x, f.ort.y, f.ort.breite, f.ort.hoehe);
        QPen stift = m_strich;
        QColor pinsel = f.art == FeldArt::Titel ? m_getoent : m_flaeche;
        // was weiterfuehrt, ist umrandet — nur ein Block ohne Spalten, sonst umrandete er alles darin
        const bool blatt = f.art == FeldArt::Anweisung || f.art == FeldArt::Aufruf || f.art == FeldArt::Aussprung;
        if (!links.empty() && blatt) stift = m_verweis;
        if (f.unerreichbar)
        {
            stift = QPen(m_fremdStrich, 1.2, Qt::DashLine);
            pinsel = m_fremd;
        }
        QGraphicsRectItem* stueck = m_szene.addRect(r, stift, pinsel);
        stueck->setZValue(2);
        const auto merke = [&](QGraphicsItem* teil) {
            if (f.block != kKeine) teil->setData(kKnoten, static_cast<qulonglong>(f.block) + 1);
        };
        merke(stueck);
        if (!links.empty())
            stueck->setToolTip(QStringLiteral("Click: show in the source. Double-click: go to the function or type it names. Right-click: more."));
        if (f.zeilen.empty()) return;

        QStringList zeilen;
        for (const std::string& z : f.zeilen) zeilen << gekuerzt(z);
        const double hoehe = QFontMetricsF(m_schrift).lineSpacing();
        const double oben = f.text.y - hoehe * static_cast<double>(zeilen.size()) / 2;
        for (qsizetype i = 0; i < zeilen.size(); ++i)
        {
            QGraphicsItem* teil = nullptr;
            const double y = oben + hoehe * static_cast<double>(i);
            if (links.empty())
            {
                QGraphicsSimpleTextItem* t = text(zeilen[i], QRectF(f.text.x - 1, y, 2, hoehe), f.art == FeldArt::Titel);
                teil = t;
            }
            else
            {
                QGraphicsTextItem* t = linkZeile(zeilen[i], links, {});
                t->setPos(f.text.x - t->boundingRect().width() / 2, y);
                teil = t;
            }
            merke(teil);
        }
    }

    /// Ein kleines Tabellensymbol: es gibt eine Tabelle (`tipp` sagt welche); ein Klick zeigt sie. Innen
    /// rechts oben in `r` — bei einer Raute (`spitz`) rechts neben ihrer Spitze, die Ecke von `r` laege
    /// neben der Form.
    void tabellenSymbol(KnotenId id, const QRectF& r, const QString& tipp, bool spitz = false)
    {
        const QRectF feld = spitz ? QRectF(r.right() + 3, r.center().y() - 14, 12, 9) : QRectF(r.right() - 16, r.top() + 3, 12, 9);
        QPen stift(m_verweis.color(), 1);
        m_szene.addRect(feld, stift, m_flaeche)->setZValue(6);
        m_szene.addLine(feld.left(), feld.top() + 3, feld.right(), feld.top() + 3, stift)->setZValue(6);
        m_szene.addLine(feld.center().x(), feld.top(), feld.center().x(), feld.bottom(), stift)->setZValue(6);
        // die Flaeche zum Klicken etwas groesser als das Symbol
        QGraphicsRectItem* treffer = m_szene.addRect(feld.adjusted(-3, -3, 3, 3), Qt::NoPen, Qt::NoBrush);
        treffer->setZValue(6.5);
        treffer->setData(kTabelle, static_cast<qulonglong>(id) + 1);
        treffer->setToolTip(tipp);
        treffer->setCursor(Qt::PointingHandCursor);
    }

    /// Eine Zelle einer Tabelle: getoent im Kopf, in den MC/DC-Zeilen und im Zweig, der gilt; grauer Text
    /// in der Spalte eines Namens, den die Datei selbst definiert; ein Klick trifft ihren Knoten.
    void tabellenZelle(const TabellenAnordnung::Zelle& z)
    {
        const QRectF r(z.ort.x, z.ort.y, z.ort.breite, z.ort.hoehe);
        QGraphicsRectItem* feld = m_szene.addRect(r, m_strich, z.kopf || z.getoent ? m_getoent : m_flaeche);
        feld->setZValue(2);
        const qulonglong knoten = z.knoten == kKeine ? 0ULL : static_cast<qulonglong>(z.knoten) + 1;
        feld->setData(kKnoten, knoten);
        QStringList zeilen;
        for (const std::string& s : z.zeilen) zeilen << QString::fromStdString(s);
        QGraphicsSimpleTextItem* t = nullptr;
        if (z.links)
        {
            t = textLinks(zeilen.join(QLatin1Char('\n')), {}, false);
            if (z.kopf)
            {
                QFont fett = t->font();
                fett.setBold(true);
                t->setFont(fett);
            }
            t->setPos(r.left() + 8, r.center().y() - t->boundingRect().height() / 2);
        }
        else
            t = text(zeilen.join(QLatin1Char('\n')), r, z.kopf);
        if (z.grau) t->setBrush(m_fremdStrich);
        t->setData(kKnoten, knoten);
        if (!z.ganz.empty())
        {
            feld->setToolTip(QString::fromStdString(z.ganz));
            t->setToolTip(QString::fromStdString(z.ganz));
        }
    }

    /// Eine Linie ohne Spitze (Diagonalen und Trennstriche eines Struktogramms).
    void linie(const Linienzug& l)
    {
        if (l.size() < 2) return;
        QPainterPath weg(alsPunkt(l[0]));
        for (std::size_t i = 1; i < l.size(); ++i) weg.lineTo(alsPunkt(l[i]));
        m_szene.addPath(weg, m_strich)->setZValue(3.5);
    }

    /// Ein Rahmen um einen bedingten Bereich im Struktogramm: farbig, ueber seinem Inhalt, Titel auf einem Schild.
    void bereich(const QRectF& r, const QColor& farbe, const QString& titel, const QString& ganz)
    {
        m_szene.addRect(r, QPen(farbe, 1.4), QBrush(hauch(farbe)))->setZValue(4);
        QGraphicsSimpleTextItem* kopf = textLinks(titel, r.topLeft() + QPointF(8, 2), true);
        kopf->setBrush(farbe);
        kopf->setToolTip(ganz);
        kopf->setZValue(5);
        m_szene.addRect(kopf->sceneBoundingRect().adjusted(-3, 0, 3, 0), Qt::NoPen, m_flaeche)->setZValue(4.5);
    }

    QGraphicsTextItem* linkZeile(const QString& inhalt, const std::vector<std::string>& links, QPointF ort)
    {
        QStringList muster;
        for (const std::string& name : links) muster << QRegularExpression::escape(QString::fromStdString(name));
        const QRegularExpression wort(QStringLiteral("(?<![A-Za-z0-9_:])(%1)(?![A-Za-z0-9_:])").arg(muster.join(QLatin1Char('|'))));
        QString html;
        qsizetype bis = 0;
        for (QRegularExpressionMatchIterator it = wort.globalMatch(inhalt); it.hasNext();)
        {
            const QRegularExpressionMatch treffer = it.next();
            html += inhalt.mid(bis, treffer.capturedStart() - bis).toHtmlEscaped();
            html += QStringLiteral("<span style=\"color:%1; text-decoration:underline;\">%2</span>")
                        .arg(m_verweis.color().name(), treffer.captured().toHtmlEscaped());
            bis = treffer.capturedEnd();
        }
        html += inhalt.mid(bis).toHtmlEscaped();
        auto* stueck = new QGraphicsTextItem;
        stueck->document()->setDocumentMargin(0);
        stueck->setFont(m_schrift);
        stueck->setDefaultTextColor(m_text);
        stueck->setHtml(QStringLiteral("<span style=\"white-space:pre\">%1</span>").arg(html));
        stueck->setPos(ort);
        stueck->setZValue(3);
        m_szene.addItem(stueck);
        return stueck;
    }

    const QPen& strich() const { return m_strich; }
    QGraphicsScene& szene() { return m_szene; }

private:
    QGraphicsScene& m_szene;
    QFont m_schrift;
    QPen m_strich;
    QPen m_verweis;
    QPen m_rahmen;
    QColor m_text;
    QColor m_flaeche;
    QColor m_getoent;
    QColor m_fremd;
    QColor m_fremdStrich;
};

}  // namespace

Vorschau::Vorschau(QWidget* parent) : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setBackgroundBrush(ausTabelle(Farbe::Grund, dunkel()));
    setAlignment(Qt::AlignCenter);
}

bool Vorschau::dunkel() const
{
    // nach der Palette, nicht nach dem gewaehlten Thema: so folgt die Vorschau auch dem System
    return palette().color(QPalette::Base).lightness() < 128;
}

QImage Vorschau::alsBild() const
{
    const QRectF ganz = scene()->sceneRect();
    QImage bild(ganz.size().toSize().expandedTo({1, 1}), QImage::Format_ARGB32);
    bild.fill(backgroundBrush().color());
    QPainter maler(&bild);
    maler.setRenderHint(QPainter::Antialiasing);
    scene()->render(&maler, QRectF(QPointF(0, 0), ganz.size()), ganz);
    return bild;
}

void Vorschau::leere()
{
    scene()->clear();  // nimmt auch die Marke mit
    setBackgroundBrush(ausTabelle(Farbe::Grund, dunkel()));  // das Thema kann gewechselt haben
    scene()->setSceneRect(QRectF());
    m_formen = 0;
    m_rahmen = 0;
    m_orte.clear();
    m_marke = nullptr;
    m_markiert = kKeine;
    m_struktur = false;
}

QStringList Vorschau::tabellenSymbole() const
{
    std::vector<std::pair<double, QString>> symbole;
    for (const QGraphicsItem* stueck : scene()->items())
        if (stueck->data(kTabelle).toULongLong() != 0) symbole.emplace_back(stueck->sceneBoundingRect().top(), stueck->toolTip());
    std::sort(symbole.begin(), symbole.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    QStringList aus;
    for (const auto& s : symbole) aus << s.second;
    return aus;
}

bool Vorschau::markiere(KnotenId knoten)
{
    if (m_marke != nullptr)
    {
        scene()->removeItem(m_marke);
        delete m_marke;
        m_marke = nullptr;
    }
    m_markiert = kKeine;
    if (knoten >= m_orte.size()) return false;
    // ein Ring um den Knoten, ueber allem: woher man kam, oder wo gerufen wird
    const QRectF ring = m_orte[knoten].adjusted(-7, -7, 7, 7);
    m_marke = scene()->addRect(ring, QPen(ausTabelle(Farbe::Verweis, dunkel()), 3, Qt::SolidLine), Qt::NoBrush);
    m_marke->setZValue(6);
    m_markiert = knoten;
    centerOn(ring.center());
    return true;
}

void Vorschau::zeige(const Flussdiagramm& diagramm, bool nachRechts, const std::vector<std::vector<std::string>>& links,
                     const std::vector<QString>& tabellen)
{
    leere();
    AnordnungOptionen optionen;
    optionen.nachRechts = nachRechts;
    optionen.maxZeilenLaenge = static_cast<std::size_t>(kMaxZeichen);
    const FlussAnordnung a = ordneAn(diagramm, optionen);

    Zeichner z(*scene(), dunkel());
    const std::vector<std::size_t> farben = farbenDerRahmen(diagramm);
    const auto farbe = [&](std::size_t g) { return farbeVon(farben[g]); };
    for (std::size_t g = 0; g < a.gruppen.size(); ++g)
    {
        // der Titel einer Bedingung passt in seinen Rahmen — ganz steht er in der Legende und im Tooltip
        const QString titel = QString::fromStdString(diagramm.gruppen()[g].titel);
        const auto passt = static_cast<qsizetype>(titelZeichen(a.gruppen[g], optionen));
        const QString kurz = farben[g] == kKeine ? gekuerzt(diagramm.gruppen()[g].titel)
                             : titel.size() <= passt ? titel
                                                     : titel.left(passt - 3) + QStringLiteral("...");
        z.rahmen(g, kurz, alsRect(a.gruppen[g]), farbe(g), titel);
    }
    for (const FlussAnordnung::Legende& l : a.legende)
        z.legende(alsRect(l.feld), farbeVon(l.farbe), gekuerzt(l.titel), alsPunkt(l.text));
    for (std::size_t e = 0; e < a.kanten.size(); ++e)
        z.kante(a.kanten[e], QString::fromStdString(diagramm.kanten()[e].beschriftung), a.beschriftungen[e]);
    for (std::size_t i = 0; i < a.knoten.size(); ++i)
        z.knoten(i, diagramm.knoten()[i], alsRect(a.knoten[i]), i < links.size() ? links[i] : std::vector<std::string>{});
    for (std::size_t i = 0; i < a.knoten.size() && i < tabellen.size(); ++i)
        if (!tabellen[i].isEmpty())
            z.tabellenSymbol(i, alsRect(a.knoten[i]), tabellen[i],
                             diagramm.knoten()[i].art == KnotenArt::Verzweigung ||
                                 diagramm.knoten()[i].art == KnotenArt::Variante);
    for (const Rechteck& r : a.knoten) m_orte.push_back(alsRect(r));

    scene()->setSceneRect(0, 0, a.breite, a.hoehe);
    m_formen = static_cast<int>(a.knoten.size());
    m_rahmen = static_cast<int>(a.gruppen.size());
}

void Vorschau::zeige(const Struktogramm& diagramm, const std::vector<std::vector<std::string>>& links,
                     const std::vector<QString>& tabellen)
{
    leere();
    AnordnungOptionen optionen;
    optionen.maxZeilenLaenge = static_cast<std::size_t>(kMaxZeichen);
    const StruktoAnordnung a = ordneAn(diagramm, optionen);

    Zeichner z(*scene(), dunkel());
    // die Felder der Reihe nach: ein Block vor seinen Spalten, die darueber liegen
    for (const StruktoAnordnung::Feld& f : a.felder)
        z.struktoFeld(f, f.block < links.size() ? links[f.block] : std::vector<std::string>{});
    for (const Linienzug& l : a.linien) z.linie(l);
    for (const StruktoAnordnung::Beschriftung& b : a.beschriftungen)
        z.text(gekuerzt(b.text), QRectF(b.mitte.x - 1, b.mitte.y - 1, 2, 2));
    for (const StruktoAnordnung::Rahmen& r : a.rahmen)
    {
        const QRectF ort(r.ort.x, r.ort.y, r.ort.breite, r.ort.hoehe);
        const QString titel = QString::fromStdString(r.titel);
        const auto passt = static_cast<qsizetype>(titelZeichen(r.ort, optionen));
        z.bereich(ort, farbeVon(r.farbe), titel.size() <= passt ? titel : titel.left(passt - 3) + QStringLiteral("..."), titel);
    }
    for (const FlussAnordnung::Legende& l : a.legende)
        z.legende(alsRect(l.feld), farbeVon(l.farbe), gekuerzt(l.titel), alsPunkt(l.text));
    for (const StruktoAnordnung::Feld& f : a.felder)
        if (f.block < tabellen.size() && !tabellen[f.block].isEmpty())
            z.tabellenSymbol(f.block, alsRect(f.ort), tabellen[f.block]);
    for (const Rechteck& r : a.bloecke) m_orte.push_back(alsRect(r));

    scene()->setSceneRect(0, 0, a.breite, a.hoehe);
    m_formen = static_cast<int>(a.bloecke.size());
    m_rahmen = static_cast<int>(a.rahmen.size());
}

void Vorschau::zeige(std::span<const Wahrheitstabelle> tabellen)
{
    leere();
    AnordnungOptionen optionen;
    optionen.maxZeilenLaenge = static_cast<std::size_t>(kMaxZeichen);
    const TabellenAnordnung a = ordneAn(tabellen, optionen);
    Zeichner z(*scene(), dunkel());
    for (const TabellenAnordnung::Zelle& zelle : a.zellen) z.tabellenZelle(zelle);
    for (const Rechteck& r : a.knotenOrte) m_orte.push_back(alsRect(r));
    for (const TabellenAnordnung::Text& t : a.texte)
    {
        QGraphicsSimpleTextItem* stueck = z.textLinks(QString::fromStdString(t.text), {});
        if (t.fett)
        {
            QFont fett = stueck->font();
            fett.setBold(true);
            stueck->setFont(fett);
        }
        stueck->setPos(t.links.x, t.links.y - stueck->boundingRect().height() / 2);
        stueck->setData(kKnoten, 1ULL);
    }
    scene()->setSceneRect(0, 0, a.breite, a.hoehe);
    m_formen = static_cast<int>(a.zellen.size());
}

void Vorschau::zeige(const Entscheidungstabelle& tabelle, const std::vector<QString>& tabellen)
{
    AnordnungOptionen optionen;
    optionen.maxZeilenLaenge = static_cast<std::size_t>(kMaxZeichen);
    zeigeTabelle(ordneAn(tabelle, optionen), tabellen);
}

void Vorschau::zeige(std::span<const Zuordnung> tabellen)
{
    AnordnungOptionen optionen;
    optionen.maxZeilenLaenge = static_cast<std::size_t>(kMaxZeichen);
    zeigeTabelle(ordneAn(tabellen, optionen), {});
}

void Vorschau::zeige(std::span<const Konfigurationstabelle> tabellen)
{
    AnordnungOptionen optionen;
    optionen.maxZeilenLaenge = static_cast<std::size_t>(kMaxZeichen);
    zeigeTabelle(ordneAn(tabellen, optionen), {});
}

void Vorschau::zeigeTabelle(const TabellenAnordnung& a, const std::vector<QString>& tabellen)
{
    leere();
    Zeichner z(*scene(), dunkel());
    for (const TabellenAnordnung::Zelle& zelle : a.zellen)
    {
        z.tabellenZelle(zelle);
        // das Symbol in der ersten Spalte der Zeile
        if (zelle.links && zelle.knoten < tabellen.size() && !tabellen[zelle.knoten].isEmpty())
            z.tabellenSymbol(zelle.knoten, alsRect(zelle.ort), tabellen[zelle.knoten]);
    }
    for (const Linienzug& l : a.linien) z.linie(l);
    for (const TabellenAnordnung::Text& t : a.texte)
    {
        QGraphicsSimpleTextItem* stueck = z.textLinks(QString::fromStdString(t.text), {});
        if (t.fett)
        {
            QFont fett = stueck->font();
            fett.setBold(true);
            stueck->setFont(fett);
        }
        stueck->setPos(t.links.x, t.links.y - stueck->boundingRect().height() / 2);
    }
    for (const Rechteck& r : a.knotenOrte) m_orte.push_back(alsRect(r));
    scene()->setSceneRect(0, 0, a.breite, a.hoehe);
    m_formen = static_cast<int>(a.knotenOrte.size());
}

void Vorschau::zeige(const Zustandsdiagramm& diagramm, bool nachRechts)
{
    leere();
    AnordnungOptionen optionen;
    optionen.nachRechts = nachRechts;
    optionen.maxZeilenLaenge = static_cast<std::size_t>(kMaxZeichen);
    const FlussAnordnung a = ordneAn(diagramm, optionen);

    Zeichner z(*scene(), dunkel());
    for (std::size_t e = 0; e < a.kanten.size(); ++e)
        z.kante(a.kanten[e], gekuerzt(diagramm.uebergaenge[e].bedingung), a.beschriftungen[e]);
    for (std::size_t i = 0; i < a.knoten.size(); ++i) z.zustand(diagramm.zustaende[i], alsRect(a.knoten[i]));

    scene()->setSceneRect(0, 0, a.breite, a.hoehe);
    m_formen = static_cast<int>(a.knoten.size());
}

void Vorschau::zeige(const Aufrufgraph& graph, bool nachRechts)
{
    leere();
    AnordnungOptionen optionen;
    optionen.nachRechts = nachRechts;
    optionen.maxZeilenLaenge = static_cast<std::size_t>(kMaxZeichen);
    const FlussAnordnung a = ordneAn(graph, optionen);

    Zeichner z(*scene(), dunkel());
    // je Datei ein Rahmen mit ihrem Pfad; nach unten hat das Band ihm Platz gelassen
    for (std::size_t d = 0; d < a.gruppen.size() && d < graph.dateien.size(); ++d)
    {
        const QString pfad = QString::fromStdString(graph.dateien[d]);
        const auto passt = static_cast<qsizetype>(titelZeichen(a.gruppen[d], optionen));
        z.rahmen(d, pfad.size() <= passt ? pfad : pfad.left(passt - 3) + QStringLiteral("..."), alsRect(a.gruppen[d]),
                 QColor(), pfad);
    }
    for (const FlussAnordnung::Legende& l : a.legende)
        z.legende(alsRect(l.feld), farbeVon(l.farbe), gekuerzt(l.titel), alsPunkt(l.text));
    // mehrdeutig gestrichelt, aus einem Zweig, der nicht gilt, gepunktet; eine Bedingung, die gilt, in ihrer Farbe
    const std::vector<std::size_t> farben = farbenDerBedingungen(graph);
    for (std::size_t e = 0; e < a.kanten.size(); ++e)
    {
        const AufrufKante& k = graph.kanten[e];
        QPen stift(farben[e] == kKeine ? z.strich().color() : farbeVon(farben[e]), 1.2);
        if (!k.gilt) stift.setStyle(Qt::DotLine);
        else if (k.art == AufrufKantenArt::Mehrdeutig) stift.setStyle(Qt::DashLine);
        z.kante(a.kanten[e], gekuerzt(beschriftung(k)), a.beschriftungen[e], stift);
    }
    for (std::size_t i = 0; i < a.knoten.size(); ++i) z.aufrufKnoten(i, graph.knoten[i], alsRect(a.knoten[i]));
    for (const Rechteck& r : a.knoten) m_orte.push_back(alsRect(r));

    scene()->setSceneRect(0, 0, a.breite, a.hoehe);
    m_formen = static_cast<int>(a.knoten.size());
    m_rahmen = static_cast<int>(a.gruppen.size());
}

void Vorschau::zeige(const Strukturdiagramm& diagramm, bool nachRechts,
                     const std::vector<std::vector<std::vector<std::string>>>& links)
{
    leere();
    m_struktur = true;
    AnordnungOptionen optionen;
    optionen.nachRechts = nachRechts;
    optionen.maxZeilenLaenge = static_cast<std::size_t>(kMaxZeichen);
    const StrukturAnordnung a = ordneAn(diagramm, optionen);

    Zeichner z(*scene(), dunkel());
    for (std::size_t e = 0; e < a.beziehungen.size(); ++e)
    {
        const Linienzug& linie = a.beziehungen[e];
        const Beziehung& b = diagramm.beziehungen[e];
        if (linie.size() < 2) continue;
        QPainterPath weg(alsPunkt(linie[0]));
        for (std::size_t i = 1; i < linie.size(); ++i) weg.lineTo(alsPunkt(linie[i]));
        z.szene().addPath(weg, z.strich())->setZValue(1);
        const QPointF vorletzter = alsPunkt(linie[linie.size() - 2]);
        switch (b.art)
        {
            case BeziehungArt::Enthaelt: z.raute(alsPunkt(linie[0]), alsPunkt(linie[1])); break;
            case BeziehungArt::Verweist: z.spitze(vorletzter, alsPunkt(linie.back()), true); break;
            case BeziehungArt::Erbt: z.spitze(vorletzter, alsPunkt(linie.back()), false, 13); break;
        }
        if (!b.beschriftung.empty())
        {
            // mittig dort, wo die Anordnung sie hinstellt
            QGraphicsSimpleTextItem* text = z.textLinks(gekuerzt(b.beschriftung), {}, true);
            const QRectF masse = text->boundingRect();
            text->setPos(a.beschriftungen[e].x - masse.width() / 2, a.beschriftungen[e].y - masse.height() / 2);
        }
    }
    for (std::size_t i = 0; i < a.kaesten.size(); ++i)
        z.kasten(i, diagramm.kaesten[i], alsRect(a.kaesten[i]), kopfHoehe(optionen), optionen.zeilenHoehe,
                 i < links.size() ? links[i] : std::vector<std::vector<std::string>>{});
    // markiere() umringt einen Kasten wie einen Knoten
    for (const Rechteck& r : a.kaesten) m_orte.push_back(alsRect(r));

    scene()->setSceneRect(0, 0, a.breite, a.hoehe);
    m_formen = static_cast<int>(a.kaesten.size());
}

void Vorschau::trefferBei(const QPoint& ort, KnotenId& knoten, std::size_t& gruppe) const
{
    knoten = kKeine;
    gruppe = kKeine;
    // von oben nach unten: der erste Knoten, die erste (also innerste) Gruppe
    for (const QGraphicsItem* stueck : items(ort))
    {
        if (knoten == kKeine && stueck->data(kKnoten).toULongLong() != 0)
            knoten = static_cast<KnotenId>(stueck->data(kKnoten).toULongLong() - 1);
        if (gruppe == kKeine && stueck->data(kGruppe).toULongLong() != 0)
            gruppe = static_cast<std::size_t>(stueck->data(kGruppe).toULongLong() - 1);
    }
}

void Vorschau::kastenBei(const QPoint& ort, std::size_t& kasten, std::size_t& eintrag) const
{
    kasten = kKeine;
    eintrag = kKeine;
    for (const QGraphicsItem* stueck : items(ort))
    {
        if (stueck->data(kKasten).toULongLong() == 0) continue;
        kasten = static_cast<std::size_t>(stueck->data(kKasten).toULongLong() - 1);
        if (stueck->data(kEintrag).toULongLong() != 0)
        {
            eintrag = static_cast<std::size_t>(stueck->data(kEintrag).toULongLong() - 1);
            return;  // das Glied liegt ueber seinem Kasten
        }
    }
}

void Vorschau::mousePressEvent(QMouseEvent* ereignis)
{
    m_klickMoeglich = ereignis->button() == Qt::LeftButton;
    m_gedrueckt = ereignis->pos();
    QGraphicsView::mousePressEvent(ereignis);  // beginnt das Ziehen
}

void Vorschau::mouseReleaseEvent(QMouseEvent* ereignis)
{
    QGraphicsView::mouseReleaseEvent(ereignis);
    // ein Klick nur, wenn die Maus kaum wanderte — sonst wurde das Bild gezogen
    const bool klick = m_klickMoeglich && ereignis->button() == Qt::LeftButton &&
                       (ereignis->pos() - m_gedrueckt).manhattanLength() < QApplication::startDragDistance();
    m_klickMoeglich = false;
    if (!klick) return;
    // das Tabellensymbol eines Knotens zuerst: es liegt ueber ihm
    for (const QGraphicsItem* stueck : items(ereignis->pos()))
        if (stueck->data(kTabelle).toULongLong() != 0)
        {
            if (beiTabelle) beiTabelle(static_cast<KnotenId>(stueck->data(kTabelle).toULongLong() - 1));
            return;
        }
    if (m_struktur)
    {
        std::size_t kasten = kKeine;
        std::size_t eintrag = kKeine;
        kastenBei(ereignis->pos(), kasten, eintrag);
        if (kasten != kKeine && beiKlickKasten) beiKlickKasten(kasten, eintrag);
        return;
    }
    KnotenId knoten = kKeine;
    std::size_t gruppe = kKeine;
    trefferBei(ereignis->pos(), knoten, gruppe);
    if (knoten != kKeine && beiKlick) beiKlick(knoten);
}

void Vorschau::mouseDoubleClickEvent(QMouseEvent* ereignis)
{
    m_klickMoeglich = false;  // das Loslassen danach ist kein zweiter Klick
    if (m_struktur)
    {
        std::size_t kasten = kKeine;
        std::size_t eintrag = kKeine;
        kastenBei(ereignis->pos(), kasten, eintrag);
        if (kasten != kKeine && beiDoppelklickKasten)
        {
            beiDoppelklickKasten(kasten, eintrag);
            ereignis->accept();
            return;
        }
        QGraphicsView::mouseDoubleClickEvent(ereignis);
        return;
    }
    KnotenId knoten = kKeine;
    std::size_t gruppe = kKeine;
    trefferBei(ereignis->pos(), knoten, gruppe);
    if (knoten != kKeine && beiDoppelklick)
    {
        beiDoppelklick(knoten);
        ereignis->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(ereignis);
}

void Vorschau::contextMenuEvent(QContextMenuEvent* ereignis)
{
    if (m_struktur)
    {
        std::size_t kasten = kKeine;
        std::size_t eintrag = kKeine;
        kastenBei(ereignis->pos(), kasten, eintrag);
        if (beiKontextKasten) beiKontextKasten(kasten, eintrag, ereignis->globalPos());
        ereignis->accept();
        return;
    }
    KnotenId knoten = kKeine;
    std::size_t gruppe = kKeine;
    trefferBei(ereignis->pos(), knoten, gruppe);
    if (beiKontext) beiKontext(knoten, gruppe, ereignis->globalPos());
    ereignis->accept();
}

void Vorschau::wheelEvent(QWheelEvent* ereignis)
{
    if (!(ereignis->modifiers() & Qt::ControlModifier))
    {
        QGraphicsView::wheelEvent(ereignis);
        return;
    }
    const double faktor = ereignis->angleDelta().y() > 0 ? 1.15 : 1 / 1.15;
    const double danach = transform().m11() * faktor;
    if (danach > 0.15 && danach < 6) scale(faktor, faktor);
    ereignis->accept();
}

}  // namespace c2d::ui
