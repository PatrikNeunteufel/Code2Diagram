-- Ein kleines Lager: Schema fuer das ER-Diagramm (Datentypen) von Code2Diagram.
-- Zeigt Primaerschluessel an der Spalte und an der Tabelle, Fremdschluessel an der Spalte, an der
-- Tabelle und ueber ALTER TABLE, einen zusammengesetzten Schluessel, einen Selbstverweis und Namen
-- in Begrenzern. View und Trigger werden erkannt, aber noch nicht gezeigt.

CREATE TABLE lieferant (
    lieferant_id INTEGER PRIMARY KEY,
    name         VARCHAR(80) NOT NULL,
    land         CHAR(2)
);

CREATE TABLE artikel (
    artikel_id   INTEGER PRIMARY KEY,
    bezeichnung  VARCHAR(120) NOT NULL,
    preis        NUMERIC(10, 2) DEFAULT 0.00,
    lieferant_id INTEGER REFERENCES lieferant (lieferant_id),
    ersetzt_durch INTEGER REFERENCES artikel (artikel_id)
);

CREATE TABLE "Lagerort" (
    "OrtId"  INTEGER NOT NULL,
    "Regal"  VARCHAR(10),
    CONSTRAINT pk_ort PRIMARY KEY ("OrtId")
);

CREATE TABLE bestand (
    artikel_id INTEGER NOT NULL,
    ort_id     INTEGER NOT NULL,
    menge      INTEGER DEFAULT 0 CHECK (menge >= 0),
    PRIMARY KEY (artikel_id, ort_id),
    CONSTRAINT fk_bestand_artikel FOREIGN KEY (artikel_id) REFERENCES artikel (artikel_id) ON DELETE CASCADE
);

ALTER TABLE bestand ADD CONSTRAINT fk_bestand_ort FOREIGN KEY (ort_id) REFERENCES lagerort ("OrtId");

CREATE VIEW knapp AS
    SELECT a.bezeichnung, b.menge FROM artikel a JOIN bestand b ON b.artikel_id = a.artikel_id WHERE b.menge < 5;

CREATE TRIGGER nie_negativ BEFORE UPDATE ON bestand
BEGIN
    SELECT RAISE(ABORT, 'menge < 0') WHERE NEW.menge < 0;
END;
