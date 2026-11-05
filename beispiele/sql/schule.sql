-- Eine kleine Schule: Beispiel fuer die Notationen des ER-Diagramms (Tables, Crow's foot, Chen,
-- (min,max)). Zeigt Pflicht und Kann (NOT NULL), eine 1:1-Beziehung (UNIQUE), eine m:n-Beziehung
-- ueber eine Verbindungstabelle mit eigenem Attribut, einen eindeutigen Index und
-- ALTER COLUMN ... SET NOT NULL.

CREATE TABLE lehrer (
    lehrer_id  INTEGER PRIMARY KEY,
    name       VARCHAR(60) NOT NULL,
    fach       VARCHAR(30)
);

-- 1:1, darf leer sein: jede Klasse hat hoechstens einen Klassenlehrer, jeder Lehrer hoechstens eine Klasse
CREATE TABLE klasse (
    klasse_id     INTEGER PRIMARY KEY,
    bezeichnung   VARCHAR(10) NOT NULL,
    klassenlehrer INTEGER UNIQUE REFERENCES lehrer (lehrer_id)
);

-- 1:n, Pflicht: jeder Schueler gehoert zu genau einer Klasse
CREATE TABLE schueler (
    schueler_id INTEGER PRIMARY KEY,
    name        VARCHAR(60) NOT NULL,
    geboren     DATE,
    klasse_id   INTEGER NOT NULL REFERENCES klasse (klasse_id)
);

-- 1:n, darf leer sein: ein Kurs ohne Lehrer ist erlaubt
CREATE TABLE kurs (
    kurs_id    INTEGER PRIMARY KEY,
    titel      VARCHAR(40) NOT NULL,
    lehrer_id  INTEGER,
    CONSTRAINT leitet FOREIGN KEY (lehrer_id) REFERENCES lehrer (lehrer_id)
);

-- m:n: wer welchen Kurs belegt, mit Note
CREATE TABLE belegt (
    schueler_id INTEGER REFERENCES schueler (schueler_id),
    kurs_id     INTEGER REFERENCES kurs (kurs_id),
    note        INTEGER CHECK (note BETWEEN 1 AND 6),
    PRIMARY KEY (schueler_id, kurs_id)
);

-- 1:1, Pflicht, aus drei Anweisungen: ein eindeutiger Index, der Fremdschluessel, NOT NULL danach
CREATE TABLE ausweis (
    ausweis_id  INTEGER PRIMARY KEY,
    schueler_id INTEGER,
    gueltig_bis DATE
);
CREATE UNIQUE INDEX ausweis_je_schueler ON ausweis (schueler_id);
ALTER TABLE ausweis ADD CONSTRAINT gehoert FOREIGN KEY (schueler_id) REFERENCES schueler (schueler_id);
ALTER TABLE ausweis ALTER COLUMN schueler_id SET NOT NULL;
