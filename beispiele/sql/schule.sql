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

-- Abfragen (Diagram > Queries): Joins mit Bedingung, ein Join aus WHERE, WITH, eine korrelierte
-- Unterabfrage, eine abgeleitete Tabelle, UNION und ein INSERT … SELECT

CREATE VIEW notenliste AS
SELECT s.name AS schueler, k.titel AS kurs, b.note, l.name AS lehrer
  FROM schueler s
  JOIN belegt b ON b.schueler_id = s.schueler_id
  JOIN kurs k ON k.kurs_id = b.kurs_id
  LEFT JOIN lehrer l ON l.lehrer_id = k.lehrer_id
 WHERE b.note IS NOT NULL
 ORDER BY s.name;

-- der Join steht im WHERE (Komma in FROM)
SELECT kl.bezeichnung, s.name
  FROM klasse kl, schueler s
 WHERE s.klasse_id = kl.klasse_id AND kl.bezeichnung LIKE '5%';

-- WITH und eine korrelierte Unterabfrage: wer in jedem Kurs besser ist als der Schnitt des Kurses
WITH schnitt AS (
    SELECT kurs_id, avg(note) AS mittel FROM belegt GROUP BY kurs_id
)
SELECT s.name, b.kurs_id, b.note
  FROM schueler s
  JOIN belegt b ON b.schueler_id = s.schueler_id
  JOIN schnitt x ON x.kurs_id = b.kurs_id
 WHERE b.note < x.mittel
   AND EXISTS (SELECT 1 FROM ausweis a WHERE a.schueler_id = s.schueler_id AND a.gueltig_bis > current_date);

-- eine abgeleitete Tabelle und UNION
SELECT t.name, t.rolle FROM (
    SELECT name, 'teacher' AS rolle FROM lehrer
    UNION ALL
    SELECT name, 'pupil' FROM schueler
) t
ORDER BY t.name;

-- INSERT … SELECT: wohin die Abfrage schreibt
INSERT INTO ausweis (ausweis_id, schueler_id, gueltig_bis)
SELECT s.schueler_id + 1000, s.schueler_id, DATE '2027-07-31'
  FROM schueler s
 WHERE NOT EXISTS (SELECT 1 FROM ausweis a WHERE a.schueler_id = s.schueler_id);
