# SQL-Probecode aus dem Netz

Fremder Code, nur gelesen, **nicht versioniert** (`/beispiele/sql/fremd/` in `.gitignore`).
Geladen am 2026-11-05 (C2D_Session10, Entscheid D1 b / D2 b) als Rohtext von
`raw.githubusercontent.com`, je Datei einzeln, nichts ausgeführt. Neben jeder Quelle liegt ihre
Lizenzdatei (`_LIZENZ.txt`, bei Pagila `_README_der_Quelle.txt`).

| Ordner | Quelle (Zweig: Pfad) | Dialekt | Dateien | Grösse | Lizenz (woher) | Wofür |
|---|---|---|---:|---:|---|---|
| `spark-sql-tests` | [apache/spark](https://github.com/apache/spark) `master:sql/core/src/test/resources/sql-tests/inputs` | Spark SQL (Dialekt von Databricks) | 386 | 2,80 MB | Apache-2.0 (Lizenzdatei) | Abfragen, DDL, CTE, Fensterfunktionen |
| `wwi-ssdt` | [microsoft/sql-server-samples](https://github.com/microsoft/sql-server-samples) `master:samples/databases/wide-world-importers/wwi-ssdt` | T-SQL | 336 | 10,30 MB | MIT (`license.txt`) | Tabellen, Views, Prozeduren, Funktionen |
| `pagila` | [xzilla/pagila](https://github.com/xzilla/pagila) `master:pagila-schema.sql` | Postgres, PL/pgSQL | 1 | 0,06 MB | PostgreSQL License (README, Abschnitt LICENSE) | Schema mit Funktionen und Triggern |
| `chinook` | [lerocha/chinook-database](https://github.com/lerocha/chinook-database) `master:ChinookDatabase/DataSources` | SQLite, Postgres, MySQL, SQL Server, Oracle, DB2 | 11 | 6,43 MB | MIT-artig (`LICENSE.md`) | ein Schema in vielen Dialekten, mit `INSERT`-Daten |
| `sakila` | [openpotato/sakila-db](https://github.com/openpotato/sakila-db) `main`: die drei `*-schema.sql` | MySQL, Postgres, Firebird | 3 | 0,07 MB | BSD-2-Clause (Lizenzdatei) | Schema samt Triggern |
| `duckdb-tpch` | [duckdb/duckdb](https://github.com/duckdb/duckdb) `main:extension/tpch/dbgen/queries` | DuckDB | 22 | 0,01 MB | MIT (Lizenzdatei) | die 22 TPC-H-Abfragen |
| `spark-examples` | [apache/spark](https://github.com/apache/spark) `master:examples/src/main/python` | PySpark | 150 `.py` | 0,36 MB | Apache-2.0 (Lizenzdatei) | Spark Schicht 1 (Datenfluss), P4 a |
| `dbdemos` | [databricks-demos/dbdemos-notebooks](https://github.com/databricks-demos/dbdemos-notebooks) `main`, alle `.sql` | Databricks SQL, Notebook-Quellformat | 79 | 0,58 MB | **„DB license“**: Nutzung nur „in connection with your use of the Databricks Services“ | Databricks-Notebooks (`-- COMMAND ----------`, `-- MAGIC`) |

Dazu, geladen am 2026-11-07 (C2D_Session11, Entscheid P2-1 a) nach **`beispiele/spark/plaene/`** (eigener
Eintrag in `.gitignore`, Lizenzdatei `_LIZENZ.txt`):

| Ordner | Quelle (Zweig: Pfad) | Form | Dateien | Grösse | Lizenz | Wofür |
|---|---|---|---:|---:|---|---|
| `tpcds-plan-stability/approved-plans-{v1_4,v2_7,modified}/q*/` | [apache/spark](https://github.com/apache/spark) `master:sql/core/src/test/resources/tpcds-plan-stability` | `explain.txt` (formatted), `simplified.txt` (Baum) | 628 | 8,21 MB | Apache-2.0 | Spark Schicht 2 (Plan) |
| `sql-tests/results/explain*.sql.out` | dieselbe, `…/sql-tests/results` | `EXPLAIN` in allen Modi, AQE, CBO | 3 | 0,08 MB | Apache-2.0 | dasselbe |

Geladen mit dem Skript der Sitzung aus der Baumabfrage der GitHub-API (`git/trees`), je Datei die Grösse
geprüft (631 / 631 gleich).

Zusammen 988 Dateien, rund 20 MB. Auf Wunsch Patriks auch dbdemos (Option b), obwohl die Lizenz die
Nutzung an die Databricks-Dienste bindet — nicht weitergeben, nicht ins Repo.

Neu laden: `python -I beispiele/sql/lade_proben.py beispiele/sql/fremd [ordner …]` (Tabelle
`QUELLEN` im Skript). Es legt in die Ordner mit einem Dialekt eine `code2diagram.json` (`"sql"`:
spark-sql-tests und dbdemos `spark`, wwi-ssdt `tsql`, pagila `postgres`, duckdb-tpch `duckdb`);
Chinook und Sakila tragen mehrere Dialekte und bleiben ohne.

Eigene Event-Logs von Spark: `beispiele/spark/eventlogs_aufnehmen.py` (nach
`beispiele/spark/fremd/eventlogs/`, unversioniert). Eigene Pläne in jedem Modus:
`beispiele/spark/plaene_aufnehmen.py` (nach `beispiele/spark/fremd/plaene-eigen/`).

Geladen (Stand 2026-11-07): `log-example/app-20170708141026-0013` aus
[xiandong79/Spark-Log-Parser](https://github.com/xiandong79/Spark-Log-Parser) nach
`beispiele/spark/fremd/eventlogs-netz/spark-log-parser/` (ein Event-Log, 5,6 MB, 2394 Ereignisse, MIT mit
`_LIZENZ.txt`, Spark 2.0.0, Scala-Anwendung KMeans: 32 Executoren mit je einem Kern, 33 Stages, 1112 Tasks).
Nicht geladen: der Zenodo-Datensatz 10516345 lehnt den automatischen Abruf ab (403), Grösse und Lizenz ungeprüft.
