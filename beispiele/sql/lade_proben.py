"""Laedt die SQL- und Spark-Proben (Entscheide D1 b, P4 a) einzeln als Rohtext. Fuehrt nichts aus.

Aufruf: python -I beispiele/sql/lade_proben.py beispiele/sql/fremd [ordner ...]

Ohne Ordner alle Quellen der Tabelle QUELLEN; Quellen und Lizenzen: QUELLEN.md daneben.
"""
import json
import pathlib
import sys
import time
import urllib.parse
import urllib.request

UA = {"User-Agent": "c2d-probe"}

# (Ordner, Repo, Zweig, Pfad im Repo, Endungen; leerer Pfad = ganzes Repo, nur = Liste fester Dateien)
QUELLEN = [
    ("spark-sql-tests", "apache/spark", "master", "sql/core/src/test/resources/sql-tests/inputs", (".sql",), None),
    ("wwi-ssdt", "microsoft/sql-server-samples", "master", "samples/databases/wide-world-importers/wwi-ssdt", (".sql",), None),
    ("pagila", "xzilla/pagila", "master", "", (".sql",), ["pagila-schema.sql"]),
    ("chinook", "lerocha/chinook-database", "master", "ChinookDatabase/DataSources", (".sql",), None),
    ("sakila", "openpotato/sakila-db", "main", "", (".sql",),
     ["MySQL/sakila-schema.sql", "PostgreSQL/sakila-schema.sql", "Firebird/sakila-schema.sql"]),
    ("duckdb-tpch", "duckdb/duckdb", "main", "extension/tpch/dbgen/queries", (".sql",), None),
    ("dbdemos", "databricks-demos/dbdemos-notebooks", "main", "", (".sql",), None),
    ("spark-examples", "apache/spark", "master", "examples/src/main/python", (".py",), None),
]
LIZENZEN = {
    "spark-sql-tests": "LICENSE", "spark-examples": "LICENSE", "wwi-ssdt": "license.txt", "chinook": "LICENSE.md",
    "sakila": "LICENSE", "duckdb-tpch": "LICENSE", "dbdemos": "LICENSE", "pagila": "README",
}
# Je Quelle mit einem Dialekt eine Projektdatei (Entscheid S3): `c2d <ordner>` liest sie dann so.
# Chinook und Sakila tragen mehrere Dialekte in einem Ordner: ohne.
DIALEKTE = {"spark-sql-tests": "spark", "dbdemos": "spark", "wwi-ssdt": "tsql", "pagila": "postgres",
            "duckdb-tpch": "duckdb"}


def hole(url: str) -> bytes:
    for versuch in range(4):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
                return r.read()
        except Exception as fehler:  # noqa: BLE001 - erneut versuchen, dann melden
            if versuch == 3:
                raise RuntimeError(f"{url}: {fehler}") from fehler
            time.sleep(2 * (versuch + 1))
    raise AssertionError


def baum(repo: str, zweig: str, pfad: str) -> list[str]:
    ref = f"{zweig}:{pfad}" if pfad else zweig
    d = json.loads(hole(f"https://api.github.com/repos/{repo}/git/trees/{urllib.parse.quote(ref)}?recursive=1"))
    if d.get("truncated"):
        print(f"WARNUNG {repo} {pfad}: Baum abgeschnitten", file=sys.stderr)
    return [x["path"] for x in d["tree"] if x["type"] == "blob"]


def main() -> int:
    ziel = pathlib.Path(sys.argv[1])
    ziel.mkdir(parents=True, exist_ok=True)
    nur_ordner = set(sys.argv[2:])
    for ordner, repo, zweig, pfad, endungen, nur in QUELLEN:
        if nur_ordner and ordner not in nur_ordner:
            continue
        dateien = nur if nur is not None else [p for p in baum(repo, zweig, pfad) if p.lower().endswith(endungen)]
        basis = f"{pfad}/" if pfad else ""
        summe = 0
        for rel in dateien:
            daten = hole(f"https://raw.githubusercontent.com/{repo}/{zweig}/{urllib.parse.quote(basis + rel)}")
            datei = ziel / ordner / rel
            datei.parent.mkdir(parents=True, exist_ok=True)
            datei.write_bytes(daten)
            summe += len(daten)
        try:
            lizenz = hole(f"https://raw.githubusercontent.com/{repo}/{zweig}/{LIZENZEN[ordner]}")
            (ziel / ordner / ("_README_der_Quelle.txt" if ordner == "pagila" else "_LIZENZ.txt")).write_bytes(lizenz)
        except RuntimeError as fehler:
            print(f"  keine Lizenzdatei: {fehler}", file=sys.stderr)
        if ordner in DIALEKTE:
            (ziel / ordner / "code2diagram.json").write_text(f'{{ "sql": "{DIALEKTE[ordner]}" }}\n', encoding="utf-8")
        print(f"{ordner}: {len(dateien)} Dateien, {summe / 1e6:.2f} MB ({repo} {zweig}:{pfad or '/'})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
