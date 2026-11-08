-- Databricks notebook source
-- MAGIC %md
-- MAGIC # Bestand
-- MAGIC Jede SQL-Zelle hat ihren Ablauf (`<cell n>`); eine Anweisung endet spaetestens mit ihrer Zelle.

-- COMMAND ----------

CREATE TABLE IF NOT EXISTS artikel (id INT NOT NULL, name STRING, bestand INT)

-- COMMAND ----------

CREATE TABLE IF NOT EXISTS lager (artikel_id INT NOT NULL, ort STRING, menge INT)

-- COMMAND ----------

-- ohne ';' am Ende: die Abfrage endet mit der Zelle
SELECT a.name, l.ort FROM artikel a JOIN lager l ON l.artikel_id = a.id AND l.menge > 0 WHERE a.bestand < 5 OR a.name IS NULL

-- COMMAND ----------

-- ein Block: der Ablauf der Zelle
BEGIN
  DECLARE n INT DEFAULT 0;
  WHILE n < 10 DO
    SET n = n + 1;
    IF n > 3 AND n < 8 THEN
      ITERATE;
    END IF;
  END WHILE;
  VALUES (n);
END;

-- COMMAND ----------

-- eine Prozedur bleibt eine eigene Funktion; die Zelle fuehrt zu ihr
CREATE OR REPLACE PROCEDURE nachbestellen(IN grenze INT)
LANGUAGE SQL
AS BEGIN
  INSERT INTO lager SELECT id, 'Zentrale', grenze FROM artikel WHERE bestand < grenze;
END;
