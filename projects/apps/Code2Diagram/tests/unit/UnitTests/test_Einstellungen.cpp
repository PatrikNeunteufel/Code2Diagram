/**
 * @file test_Einstellungen.cpp
 * @brief File > Settings: Tabelle, Speichern, Dialog, Thema; die Vorschau folgt der Palette
 */
#include <doctest.h>

#include "ui/Einstellungen.hpp"
#include "ui/Hauptfenster.hpp"
#include "ui/Thema.hpp"

#include <QApplication>
#include <QComboBox>
#include <QListWidget>
#include <QSettings>

using namespace c2d::ui;

namespace
{

const QString kThema = QString::fromLatin1(einstellungen::kThema);

}  // namespace

TEST_SUITE("Einstellungen")
{
    TEST_CASE("Gespeichert wird nur, was erlaubt ist; sonst gilt die Vorgabe")
    {
        QSettings().remove(kThema);
        REQUIRE(einstellungen::finde(kThema) != nullptr);
        CHECK(einstellungen::wert(kThema) == "system");

        CHECK(einstellungen::setze(kThema, "dark"));
        CHECK(einstellungen::wert(kThema) == "dark");
        CHECK(QSettings().value(kThema).toString() == "dark");
        CHECK_FALSE(einstellungen::setze(kThema, "lila"));
        CHECK(einstellungen::wert(kThema) == "dark");  // bleibt
        CHECK_FALSE(einstellungen::setze("gibt/es_nicht", "x"));
        CHECK(einstellungen::wert("gibt/es_nicht").isEmpty());

        // von Hand verdorben: die Vorgabe
        QSettings().setValue(kThema, "lila");
        CHECK(einstellungen::wert(kThema) == "system");
        QSettings().remove(kThema);
    }

    TEST_CASE("Das Thema: Kennungen und was gesetzt ist")
    {
        CHECK(thema::arten().size() == 3);
        CHECK(thema::ausId("light") == thema::Thema::Hell);
        CHECK_FALSE(thema::ausId("Light").has_value());
        CHECK(thema::idVon(thema::Thema::Dunkel) == "dark");
        thema::wende(thema::Thema::Dunkel);
        CHECK(thema::gilt() == thema::Thema::Dunkel);
        thema::wende(thema::Thema::System);
        CHECK(thema::gilt() == thema::Thema::System);
    }

    TEST_CASE("Der Dialog: Seiten aus der Tabelle, Apply speichert und setzt das Thema")
    {
        QSettings().remove(kThema);
        Hauptfenster fenster;
        EinstellungenDialog* dialog = fenster.zeigeEinstellungen();
        REQUIRE(dialog != nullptr);
        CHECK(fenster.zeigeEinstellungen() == dialog);  // offen: derselbe

        const auto* seiten = dialog->findChild<QListWidget*>("seiten");
        REQUIRE(seiten != nullptr);
        REQUIRE(seiten->count() == 1);
        CHECK(seiten->item(0)->text() == "Appearance");

        QComboBox* feld = dialog->feld(kThema);
        REQUIRE(feld != nullptr);
        CHECK(feld->count() == 3);
        CHECK(feld->currentData().toString() == "system");
        CHECK(dialog->feld("gibt/es_nicht") == nullptr);

        feld->setCurrentIndex(feld->findData("light"));
        CHECK(einstellungen::wert(kThema) == "system");  // erst mit Apply
        dialog->uebernimm();
        CHECK(einstellungen::wert(kThema) == "light");
        CHECK(thema::gilt() == thema::Thema::Hell);

        // wieder geoeffnet, zeigt er, was gespeichert ist
        dialog->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        EinstellungenDialog* neu = fenster.zeigeEinstellungen();
        CHECK(neu->feld(kThema)->currentData().toString() == "light");
        neu->close();

        QSettings().remove(kThema);
        thema::wende(thema::Thema::System);
    }

    TEST_CASE("Wechselt die Palette, zeichnet die Vorschau in den neuen Farben")
    {
        Hauptfenster fenster;
        fenster.zeigeBeispiel();
        const QPalette vorher = QApplication::palette();
        const QColor grundVorher = fenster.vorschauBild().pixelColor(0, 0);
        REQUIRE(grundVorher != QColor(20, 20, 20));

        QPalette dunkel = vorher;
        dunkel.setColor(QPalette::Base, QColor(20, 20, 20));
        dunkel.setColor(QPalette::Text, QColor(230, 230, 230));
        QApplication::setPalette(dunkel);
        // farbenNeu() ist angestossen, waehrend die Ereignisse laufen: eine Runde mehr
        const auto ereignisse = [] {
            for (int runde = 0; runde < 3; ++runde) QCoreApplication::processEvents();
        };
        ereignisse();
        CHECK(fenster.vorschauBild().pixelColor(0, 0) == QColor(20, 20, 20));
        CHECK(fenster.vorschauFormen() > 0);

        QApplication::setPalette(vorher);
        ereignisse();
        CHECK(fenster.vorschauBild().pixelColor(0, 0) == grundVorher);
    }
}
