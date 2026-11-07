"""Event-Logs von Spark aufnehmen: eigene Skripte und Notebooks einmal laufen lassen (Entscheid P4 a).

Aufruf (mit dem Python, in dem PySpark sonst laeuft; fuer Notebooks braucht es dort nbconvert und
ipykernel — `python -m pip install nbconvert ipykernel`):

    python beispiele/spark/eventlogs_aufnehmen.py <datei.py | notebook.ipynb> [...]

Das Skript aendert keine der Dateien: es legt eine `spark-defaults.conf` in einen eigenen Ordner und
zeigt mit `SPARK_CONF_DIR` darauf, so schreibt jede SparkSession, die dabei entsteht, ihren Event-Log
nach `beispiele/spark/fremd/eventlogs/` (unversioniert). Unter Windows setzt es `HADOOP_HOME` auf den
ersten Ordner mit `bin/winutils.exe` (C:\\hadoop, C:\\Hadoop, C:\\winutils, ~\\hadoop), wenn es nicht
gesetzt ist — sonst bricht Spark beim Anlegen des Logs ab.

Ein Skript laeuft in seinem eigenen Ordner (wie von Hand gestartet). Ein Notebook laeuft wie in
Databricks: `spark` und `sc` sind vorgegeben (eine Startdatei von IPython legt sie an), es laeuft als
Abschrift in `beispiele/spark/fremd/arbeit/<name>/`, und dort liegen, wo die Tabelle BEISPIELDATEN
unten es sagt, seine Daten. An die Abschrift kommt zuletzt eine Zelle `spark.stop()` — ohne sie bliebe
der Log `….inprogress`; die Zellen davor behalten ihre Nummern. Das ausgefuehrte Notebook (mit den
Ausgaben von `explain()`) kommt nach `beispiele/spark/fremd/notebooks/`.

Ein Event-Log ist eine Datei je Anwendung (`local-<Zeit>` bzw. `app-<Zeit>`), ein JSON-Ereignis je
Zeile. Ein Lauf, der vor `spark.stop()` abbricht, hinterlaesst `….inprogress` — der ist unvollstaendig.
"""
import importlib.util
import json
import os
import pathlib
import platform
import shutil
import subprocess
import sys

HIER = pathlib.Path(__file__).resolve().parent
ZIEL = HIER / "fremd"
LOGS = ZIEL / "eventlogs"
NOTEBOOKS = ZIEL / "notebooks"
ARBEIT = ZIEL / "arbeit"
KONF = ZIEL / "spark-conf"
IPYTHON = ZIEL / "ipython"

# Was jede Session dazubekommt. Unkomprimiert, damit der Log ohne Werkzeug lesbar bleibt
# (Spark 4 komprimiert sonst vorgegeben).
EINSTELLUNGEN = {
    "spark.eventLog.enabled": "true",
    "spark.eventLog.compress": "false",
    "spark.eventLog.rolling.enabled": "false",
}

# Wo Hadoop unter Windows gesucht wird (wie IR_BD `pyspark_setup.find_hadoop_home`).
HADOOP_ORTE = [r"C:\hadoop", r"C:\Hadoop", r"C:\winutils", str(pathlib.Path.home() / "hadoop")]

# Startdatei eines Notebooks: `spark` und `sc` wie in Databricks.
START = '''\
from pyspark.sql import SparkSession
spark = SparkSession.builder.appName({name!r}).master("local[2]").getOrCreate()
sc = spark.sparkContext
'''

# Je Notebook (Dateiname) der Code, der seine Daten im Arbeitsordner anlegt, falls sie fehlen.
BEISPIELDATEN = {
    "lager_explain.ipynb": '''\
import os as _os
if not _os.path.isdir("daten/bestand"):
    spark.createDataFrame([(i, ["Nord", "Sued", "Ost"][i % 3], i * 3 % 17) for i in range(300)],
                          ["nr", "lager", "menge"]).write.parquet("daten/bestand")
    spark.createDataFrame([(i, f"Artikel {i}") for i in range(300)],
                          ["nr", "name"]).write.parquet("daten/artikel")
''',
}


def schreibe_konfiguration() -> None:
    KONF.mkdir(parents=True, exist_ok=True)
    LOGS.mkdir(parents=True, exist_ok=True)
    zeilen = [f"{k} {v}" for k, v in EINSTELLUNGEN.items()]
    zeilen.append(f"spark.eventLog.dir {LOGS.as_uri()}")
    (KONF / "spark-defaults.conf").write_text("\n".join(zeilen) + "\n", encoding="utf-8")


def ergaenze_windows(umgebung: dict) -> bool:
    """Setzt HADOOP_HOME (und bin im PATH), falls noetig; False, wenn kein winutils zu finden ist."""
    if platform.system() != "Windows":
        return True
    if not umgebung.get("HADOOP_HOME"):
        gefunden = next((o for o in HADOOP_ORTE if (pathlib.Path(o) / "bin" / "winutils.exe").is_file()), None)
        if gefunden is None:
            print("HADOOP_HOME ist nicht gesetzt und kein bin\\winutils.exe gefunden in " + ", ".join(HADOOP_ORTE),
                  file=sys.stderr)
            return False
        umgebung["HADOOP_HOME"] = gefunden
        print(f"HADOOP_HOME = {gefunden}")
    umgebung["PATH"] = str(pathlib.Path(umgebung["HADOOP_HOME"]) / "bin") + os.pathsep + umgebung.get("PATH", "")
    # Python-Worker (RDD, UDF) mit demselben Python wie der Treiber, sonst sucht Spark `python3`.
    umgebung.setdefault("PYSPARK_PYTHON", sys.executable)
    umgebung.setdefault("PYSPARK_DRIVER_PYTHON", sys.executable)
    return True


def bereite_notebook_vor(datei: pathlib.Path, umgebung: dict) -> pathlib.Path:
    """Abschrift mit `spark.stop()` am Ende in einen eigenen Arbeitsordner, Startdatei fuer `spark`."""
    arbeit = ARBEIT / datei.stem
    arbeit.mkdir(parents=True, exist_ok=True)
    nb = json.loads(datei.read_text(encoding="utf-8"))
    nb["cells"].append({"cell_type": "code", "execution_count": None, "metadata": {}, "outputs": [],
                        "source": ["spark.stop()"]})
    abschrift = arbeit / datei.name
    abschrift.write_text(json.dumps(nb, ensure_ascii=False, indent=1), encoding="utf-8")

    start = IPYTHON / "profile_default" / "startup"
    if start.exists():
        shutil.rmtree(start)
    start.mkdir(parents=True)
    (start / "00-spark.py").write_text(START.format(name=datei.stem) + BEISPIELDATEN.get(datei.name, ""),
                                       encoding="utf-8")
    umgebung["IPYTHONDIR"] = str(IPYTHON)
    return abschrift


def laufe(datei: pathlib.Path, umgebung: dict) -> int:
    if datei.suffix == ".ipynb":
        # nbconvert aus demselben Python wie PySpark: ein `jupyter` auf dem Pfad kann einer
        # anderen Umgebung gehoeren (und dort fehlt nbconvert oder pyspark).
        if importlib.util.find_spec("nbconvert") is None or importlib.util.find_spec("ipykernel") is None:
            print(f"{datei}: nbconvert oder ipykernel fehlt in {sys.executable} -"
                  f" {sys.executable} -m pip install nbconvert ipykernel", file=sys.stderr)
            return 1
        NOTEBOOKS.mkdir(parents=True, exist_ok=True)
        umgebung = dict(umgebung)
        abschrift = bereite_notebook_vor(datei, umgebung)
        befehl = [sys.executable, "-m", "nbconvert", "--to", "notebook", "--execute",
                  "--ExecutePreprocessor.kernel_name=python3",
                  "--ExecutePreprocessor.timeout=-1", "--output-dir", str(NOTEBOOKS), str(abschrift)]
        ordner = abschrift.parent
    else:
        befehl = [sys.executable, str(datei)]
        ordner = datei.parent
    print(">", " ".join(befehl))
    return subprocess.run(befehl, cwd=ordner, env=umgebung).returncode


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    if importlib.util.find_spec("pyspark") is None:
        print(f"pyspark fehlt in {sys.executable} - mit dem Python der Umgebung starten, in der Spark"
              " laeuft (IR_BD: SW/anew/.venv/Scripts/python.exe)", file=sys.stderr)
        return 1
    umgebung = dict(os.environ, SPARK_CONF_DIR=str(KONF))
    if not ergaenze_windows(umgebung):
        return 1
    schreibe_konfiguration()
    vorher = set(LOGS.iterdir())
    fehler = 0
    for angabe in sys.argv[1:]:
        datei = pathlib.Path(angabe).resolve()
        if not datei.is_file():
            print(f"{angabe}: nicht gefunden", file=sys.stderr)
            fehler += 1
            continue
        if laufe(datei, umgebung) != 0:
            print(f"{angabe}: Lauf mit Fehler beendet", file=sys.stderr)
            fehler += 1
    neu = sorted(set(LOGS.iterdir()) - vorher)
    print(f"{len(neu)} neue Event-Logs in {LOGS}:")
    for log in neu:
        offen = "  (nicht beendet)" if log.name.endswith(".inprogress") else ""
        print(f"  {log.name}  {log.stat().st_size / 1e6:.2f} MB{offen}")
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
