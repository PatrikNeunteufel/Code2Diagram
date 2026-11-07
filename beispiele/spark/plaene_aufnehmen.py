"""Plaene von Spark aufnehmen: explain() in jedem Modus, mit und ohne AQE (Spark Schicht 2, Entscheid P2-1).

Aufruf (mit dem Python, in dem PySpark laeuft; IR_BD: SW/anew/.venv/Scripts/python.exe):

    python beispiele/spark/plaene_aufnehmen.py

Baut aus kleinen Tabellen (als Parquet in einen eigenen Ordner geschrieben, damit die Plaene FileScan zeigen)
einige Abfragen — Join (Broadcast und Sort-Merge), groupBy, orderBy, eine Unterabfrage, Union — und schreibt je
Abfrage und Modus (simple, extended, formatted, cost, codegen) den Text von explain() nach
`beispiele/spark/fremd/plaene-eigen/<abfrage>_<modus>[_aqe].txt` (unversioniert). Mit AQE zusaetzlich nach dem
Ausfuehren (`collect()`) noch einmal: dann steht der endgueltige Plan da (`isFinalPlan=true`).

Das Skript aendert nichts ausserhalb von `beispiele/spark/fremd/`. Danach liest c2d die Dateien:

    c2d beispiele/spark/fremd/plaene-eigen --liste
"""
import contextlib
import importlib.util
import io
import pathlib
import shutil
import sys

HIER = pathlib.Path(__file__).resolve().parent
ZIEL = HIER / "fremd" / "plaene-eigen"
DATEN = HIER / "fremd" / "plaene-daten"
MODI = ["simple", "extended", "formatted", "cost", "codegen"]


def abfragen(spark):
    from pyspark.sql import functions as F

    kunden = spark.read.parquet(str(DATEN / "kunden"))
    bestellungen = spark.read.parquet(str(DATEN / "bestellungen"))
    orte = spark.read.parquet(str(DATEN / "orte"))
    kunden.createOrReplaceTempView("kunden")
    bestellungen.createOrReplaceTempView("bestellungen")
    return {
        # klein gegen gross: Broadcast
        "join_broadcast": bestellungen.join(F.broadcast(orte), "ort").groupBy("land").agg(F.sum("betrag").alias("summe")),
        # zwei grosse Seiten: Sort-Merge mit zwei Exchanges
        "join_sortmerge": bestellungen.join(kunden.hint("merge"), "kunde").orderBy(F.desc("betrag")),
        # Unterabfrage im Filter (skalar) und eine Mengenoperation
        "unterabfrage": spark.sql("SELECT kunde, betrag FROM bestellungen "
                                  "WHERE betrag > (SELECT avg(betrag) FROM bestellungen)"),
        "union": bestellungen.select("kunde").union(kunden.select("kunde")).distinct(),
    }


def schreibe_daten(spark) -> None:
    if DATEN.exists():
        shutil.rmtree(DATEN)
    orte = [(i, f"Ort {i}", ["CH", "DE", "AT"][i % 3]) for i in range(30)]
    kunden = [(i, f"Kunde {i}", i % 30) for i in range(2000)]
    bestellungen = [(i, i % 2000, (i * 7) % 30, float(i % 97)) for i in range(20000)]
    spark.createDataFrame(orte, ["ort", "name", "land"]).write.parquet(str(DATEN / "orte"))
    spark.createDataFrame(kunden, ["kunde", "name", "heimat"]).write.parquet(str(DATEN / "kunden"))
    spark.createDataFrame(bestellungen, ["nr", "kunde", "ort", "betrag"]).write.parquet(str(DATEN / "bestellungen"))


def text_von_explain(df, modus: str) -> str:
    puffer = io.StringIO()
    with contextlib.redirect_stdout(puffer):
        df.explain(mode=modus)
    return puffer.getvalue()


def main() -> int:
    if importlib.util.find_spec("pyspark") is None:
        print(f"pyspark fehlt in {sys.executable} - mit dem Python der Umgebung starten, in der Spark laeuft"
              " (IR_BD: SW/anew/.venv/Scripts/python.exe)", file=sys.stderr)
        return 1
    from pyspark.sql import SparkSession

    ZIEL.mkdir(parents=True, exist_ok=True)
    geschrieben = 0
    for aqe in (False, True):
        spark = (SparkSession.builder.appName("plaene_aufnehmen").master("local[2]")
                 .config("spark.sql.adaptive.enabled", str(aqe).lower())
                 .config("spark.sql.shuffle.partitions", "8")
                 .getOrCreate())
        if not aqe:
            schreibe_daten(spark)
        nachsatz = "_aqe" if aqe else ""
        for name, df in abfragen(spark).items():
            for modus in MODI:
                (ZIEL / f"{name}_{modus}{nachsatz}.txt").write_text(text_von_explain(df, modus), encoding="utf-8")
                geschrieben += 1
            if aqe:
                df.collect()  # danach kennt AQE den endgueltigen Plan
                for modus in ("simple", "formatted"):
                    (ZIEL / f"{name}_{modus}_aqe_final.txt").write_text(text_von_explain(df, modus), encoding="utf-8")
                    geschrieben += 1
        spark.stop()
    print(f"{geschrieben} Plaene nach {ZIEL}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
