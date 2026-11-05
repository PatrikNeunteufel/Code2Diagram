"""Ein Lager in SQLite: das Schema steht in Zeichenketten (Beispiel fuer SQL in Python, Code2Diagram).

Data types zeigt die Klasse ``Lager`` und daneben die Tabellen der Zeichenketten; ein Fremdschluessel
findet seine Tabelle auch in einer anderen Zeichenkette. Die Zeichenkette in ``neue_tabelle`` wird zur
Laufzeit zusammengesetzt und bleibt aussen vor (Hinweis).
"""
import sqlite3

SCHEMA = """
CREATE TABLE IF NOT EXISTS lieferant (
    lieferant_id INTEGER PRIMARY KEY,
    name         TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS artikel (
    artikel_id   INTEGER PRIMARY KEY,
    bezeichnung  TEXT NOT NULL,
    lieferant_id INTEGER REFERENCES lieferant (lieferant_id)
);
"""

BESTAND = ("CREATE TABLE IF NOT EXISTS bestand ("
           "  artikel_id INTEGER NOT NULL REFERENCES artikel (artikel_id),"
           "  menge      INTEGER DEFAULT 0,"
           "  PRIMARY KEY (artikel_id))")


class Lager:
    def __init__(self, pfad):
        self.verbindung = sqlite3.connect(pfad)
        self.verbindung.executescript(SCHEMA)
        self.verbindung.execute(BESTAND)

    def menge(self, artikel_id):
        zeile = self.verbindung.execute("SELECT menge FROM bestand WHERE artikel_id = ?", (artikel_id,)).fetchone()
        return zeile[0] if zeile else 0

    def neue_tabelle(self, name):
        self.verbindung.execute("CREATE TABLE " + name + " (wert TEXT)")
