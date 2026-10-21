"""Programm-Icons von Code2Diagram bauen: SVG erzeugen, daraus .ico und Vorschau.

Aufruf (normales Python mit PySide6 und Pillow):

    python assets/images/Logo/icons/build_icons.py

- schreibt je Icon eine `*.svg` (Quelle, 128 x 128) in diesen Ordner;
- legt daneben die gleichnamige `.ico` mit den Groessen 16-256 px ab;
- legt unter `vorschau/` je Icon ein PNG (64 px) und das Blatt
  `uebersicht.png` ab, das jedes Icon in 64, 32 und 16 px zeigt, links auf
  hellem, rechts auf dunklem Grund.

Die SVG entstehen aus der Tabelle `APP` unten. Wer ein Icon aendert, aendert
die Tabelle und baut neu; die erzeugten Dateien werden ueberschrieben.
Aufbau nach dem Muster von UART (`assets/images/Logo/icons/build_icons.py`).

Stand: erster Wurf (C2D_Session10, 2026-10-21). Gewaehlt ist
`app_2_zeilen_zu_knoten`; die Projektdateien `Code2Diagram.qrc`,
`Code2Diagram.rc` und `c2d.rc` verweisen auf seine .svg und .ico hier.
"""
import io
import pathlib
import sys

from PIL import Image, ImageDraw
from PySide6.QtCore import QBuffer, QByteArray, QIODevice, Qt
from PySide6.QtGui import QGuiApplication, QImage, QPainter
from PySide6.QtSvg import QSvgRenderer

ORDNER = pathlib.Path(__file__).parent
VORSCHAU = ORDNER / "vorschau"
GROESSEN = (16, 24, 32, 48, 64, 128, 256)

# Farben. Kachel (Aubergine, damit es neben dem Navy von UART unterscheidbar
# bleibt), Kanten, Knoten; dunklere Toene fuer Knoten auf hellem Blatt.
KACHEL, KACHEL2, KANTE, GEDAEMPFT = "#2A2340", "#3A3157", "#B9B3D0", "#8C86A8"
GRUEN, GELB, ROT, BLAU, HELL = "#5DCAA5", "#FAC775", "#F0997B", "#85B7EB", "#F1EEF8"
BLAU_KRAEFTIG, GELB_KRAEFTIG = "#378ADD", "#D9961F"

RUND = 'fill="none" stroke-linecap="round" stroke-linejoin="round"'

# Programm-Icon: Inhalt in einem Feld von 88 x 88, die Kachel kommt dazu.
APP = {
    # Geschweifte Klammern um einen kleinen Ablauf: Start, Raute, Schritt.
    "app_1_klammer_ablauf": f'''
<path d="M20,8 q-7,0 -7,7 v17 q0,7 -7,12 q7,5 7,12 v17 q0,7 7,7" {RUND} stroke="{HELL}" stroke-width="4.5"/>
<path d="M68,8 q7,0 7,7 v17 q0,7 7,12 q-7,5 -7,12 v17 q0,7 -7,7" {RUND} stroke="{HELL}" stroke-width="4.5"/>
<path d="M44,24 v8 M44,56 v8" stroke="{KANTE}" stroke-width="3.5"/>
<rect x="30" y="11" width="28" height="13" rx="6.5" fill="{GRUEN}"/>
<polygon points="44,31 57,44 44,57 31,44" fill="{GELB}"/>
<rect x="30" y="64" width="28" height="13" rx="2.5" fill="{BLAU}"/>''',
    # Links Codezeilen, rechts der Ablauf daraus; die farbige Zeile ist die Bedingung.
    "app_2_zeilen_zu_knoten": f'''
<path d="M6,18 h26 M12,30 h22 M12,54 h18 M6,66 h14" {RUND} stroke="{GEDAEMPFT}" stroke-width="4.5"/>
<path d="M12,42 h24" {RUND} stroke="{GELB}" stroke-width="4.5"/>
<polyline points="41,37 47,43 41,49" {RUND} stroke="{HELL}" stroke-width="4"/>
<path d="M68,24 v8 M68,56 v8" stroke="{KANTE}" stroke-width="3.5"/>
<rect x="55" y="11" width="26" height="13" rx="3" fill="{BLAU}"/>
<polygon points="68,31 80,44 68,57 56,44" fill="{GELB}"/>
<rect x="55" y="64" width="26" height="13" rx="3" fill="{GRUEN}"/>''',
    # Die Raute des Flussdiagramms mit ihren beiden Ausgaengen.
    "app_3_raute": f'''
<path d="M34,2 v10 M34,56 v10 M56,34 h14 v32" {RUND} stroke="{KANTE}" stroke-width="4"/>
<polygon points="34,11 57,34 34,57 11,34" fill="{GELB}"/>
<rect x="18" y="66" width="32" height="16" rx="3" fill="{GRUEN}"/>
<rect x="56" y="66" width="28" height="16" rx="3" fill="{ROT}"/>''',
    # Ein Struktogramm (Nassi-Shneiderman): Schritt, Verzweigung, zwei Spalten.
    "app_4_struktogramm": f'''
<rect x="8" y="8" width="72" height="18" fill="{BLAU}"/>
<rect x="8" y="26" width="72" height="18" fill="{GELB}"/>
<rect x="8" y="44" width="36" height="18" fill="{GRUEN}"/>
<rect x="8" y="62" width="36" height="18" fill="{GRUEN}"/>
<rect x="44" y="44" width="36" height="36" fill="{ROT}"/>
<path d="M8,26 h72 M8,26 L44,44 L80,26 M8,44 h72 M44,44 v36 M8,62 h36" {RUND} stroke="{KACHEL}" stroke-width="2.5"/>
<rect x="8" y="8" width="72" height="72" {RUND} stroke="{HELL}" stroke-width="3"/>''',
    # Ein Aufrufgraph in drei Schichten; der graue Knoten ist ein fremder Aufruf.
    "app_5_aufrufgraph": f'''
<path d="M44,22 L24,38 M44,22 L64,38 M24,52 L15,68 M24,52 L44,68 M64,52 L44,68 M64,52 L73,68" {RUND} stroke="{KANTE}" stroke-width="3.5"/>
<rect x="31" y="8" width="26" height="14" rx="4" fill="{GELB}"/>
<rect x="11" y="38" width="26" height="14" rx="4" fill="{BLAU}"/>
<rect x="51" y="38" width="26" height="14" rx="4" fill="{BLAU}"/>
<rect x="4" y="68" width="22" height="13" rx="4" fill="{GRUEN}"/>
<rect x="33" y="68" width="22" height="13" rx="4" fill="{GRUEN}"/>
<rect x="62" y="68" width="22" height="13" rx="4" fill="{GEDAEMPFT}"/>''',
    # Ein Blatt Quelltext, dessen untere Haelfte zum Ablauf wird.
    "app_6_blatt_wird_ablauf": f'''
<path d="M14,4 h46 l14,14 v66 h-60 z" fill="{HELL}"/>
<path d="M60,4 v14 h14" {RUND} stroke="{GEDAEMPFT}" stroke-width="2.5"/>
<path d="M22,16 h28 M28,25 h26 M28,34 h18" {RUND} stroke="{GEDAEMPFT}" stroke-width="4"/>
<path d="M44,54 v6" stroke="{KACHEL}" stroke-width="3"/>
<rect x="31" y="43" width="26" height="11" rx="3" fill="{BLAU_KRAEFTIG}"/>
<polygon points="44,59 54,69 44,79 34,69" fill="{GELB_KRAEFTIG}"/>''',
}

KOPF = ('<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" '
        'version="1.2" baseProfile="tiny" width="128" height="128" viewBox="0 0 128 128">\n')


def app_svg(inhalt: str) -> str:
    return (f'{KOPF}<rect x="4" y="4" width="120" height="120" rx="24" fill="{KACHEL}"/>\n'
            f'<g transform="translate(4,4) scale(1.3636)">{inhalt}\n</g>\n</svg>\n')


def als_bild(svg: str, kante: int) -> Image.Image:
    renderer = QSvgRenderer(QByteArray(svg.encode("utf-8")))
    if not renderer.isValid():
        raise ValueError("SVG laesst sich nicht lesen")
    img = QImage(kante, kante, QImage.Format_ARGB32)
    img.fill(Qt.transparent)
    p = QPainter(img)
    p.setRenderHint(QPainter.Antialiasing, True)
    p.setRenderHint(QPainter.SmoothPixmapTransform, True)
    renderer.render(p)
    p.end()
    daten = QByteArray()
    puffer = QBuffer(daten)
    puffer.open(QIODevice.WriteOnly)
    img.save(puffer, "PNG")
    return Image.open(io.BytesIO(bytes(daten))).convert("RGBA")


def uebersicht(bilder: dict) -> Image.Image:
    """Je Icon eine Zeile: 64, 32 und 16 px auf hellem, dann auf dunklem Grund."""
    halb, zelle_h = 150, 96
    blatt = Image.new("RGBA", (2 * halb + 200, len(bilder) * zelle_h), "#FFFFFF")
    stift = ImageDraw.Draw(blatt)
    stift.rectangle((halb, 0, 2 * halb, blatt.height), fill="#1E1E1E")
    for nr, (name, stufen) in enumerate(bilder.items()):
        y = nr * zelle_h + 16
        for x in (10, halb + 10):
            blatt.alpha_composite(stufen[64], (x, y))
            blatt.alpha_composite(stufen[32], (x + 76, y + 32))
            blatt.alpha_composite(stufen[16], (x + 120, y + 48))
        stift.text((2 * halb + 12, y + 26), name, fill="#2C2C2A")
    return blatt


def main() -> int:
    app = QGuiApplication.instance() or QGuiApplication(sys.argv[:1] + ["-platform", "offscreen"])
    VORSCHAU.mkdir(exist_ok=True)
    bilder = {}
    for name, inhalt in APP.items():
        svg = app_svg(inhalt)
        (ORDNER / f"{name}.svg").write_text(svg, encoding="utf-8", newline="\n")
        stufen = {k: als_bild(svg, k) for k in GROESSEN}
        stufen[256].save(ORDNER / f"{name}.ico", format="ICO",
                         sizes=[(k, k) for k in GROESSEN],
                         append_images=[stufen[k] for k in GROESSEN if k != 256])
        stufen[64].save(VORSCHAU / f"{name}.png")
        bilder[name] = stufen
    uebersicht(bilder).save(VORSCHAU / "uebersicht.png")
    print(f"{len(bilder)} Icons gebaut: je .svg, .ico ({len(GROESSEN)} Stufen), Vorschau; dazu uebersicht.png")
    del app
    return 0


if __name__ == "__main__":
    sys.exit(main())
