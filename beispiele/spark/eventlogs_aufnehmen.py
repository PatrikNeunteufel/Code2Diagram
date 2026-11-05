"""Event-Logs von Spark aufnehmen: eigene Skripte und Notebooks einmal laufen lassen (Entscheid P4 a).

Aufruf (mit dem Python, in dem PySpark sonst laeuft; fuer Notebooks braucht es dort nbconvert und
ipykernel — `python -m pip install nbconvert ipykernel`):

    python beispiele/spark/eventlogs_aufnehmen.py <datei.py | notebook.ipynb> [...]

Jede Datei laeuft in ihrem eigenen Ordner (wie von Hand gestartet). Das Skript aendert keine der
Dateien: es legt eine `spark-defaults.conf` in einen eigenen Ordner und zeigt mit `SPARK_CONF_DIR`
darauf, so schreibt jede SparkSession, die dabei entsteht, ihren Event-Log nach
`beispiele/spark/fremd/eventlogs/` (unversioniert). Ein Notebook wird mit nbconvert ausgefuehrt; das
ausgefuehrte Notebook (mit den Ausgaben von `explain()`) kommt nach `beispiele/spark/fremd/notebooks/`,
das Original bleibt unberuehrt.

Ein Event-Log ist eine Datei je Anwendung (`local-<Zeit>` bzw. `app-<Zeit>`), ein JSON-Ereignis je
Zeile. Ein Lauf, der vor `spark.stop()` abbricht, hinterlaesst `….inprogress` — der ist unvollstaendig.
"""
import importlib.util
import os
import pathlib
import subprocess
import sys

HIER = pathlib.Path(__file__).resolve().parent
ZIEL = HIER / "fremd"
LOGS = ZIEL / "eventlogs"
NOTEBOOKS = ZIEL / "notebooks"
KONF = ZIEL / "spark-conf"

# Was jede Session dazubekommt. Unkomprimiert, damit der Log ohne Werkzeug lesbar bleibt
# (Spark 4 komprimiert sonst vorgegeben).
EINSTELLUNGEN = {
    "spark.eventLog.enabled": "true",
    "spark.eventLog.compress": "false",
    "spark.eventLog.rolling.enabled": "false",
}


def schreibe_konfiguration() -> None:
    KONF.mkdir(parents=True, exist_ok=True)
    LOGS.mkdir(parents=True, exist_ok=True)
    zeilen = [f"{k} {v}" for k, v in EINSTELLUNGEN.items()]
    zeilen.append(f"spark.eventLog.dir {LOGS.as_uri()}")
    (KONF / "spark-defaults.conf").write_text("\n".join(zeilen) + "\n", encoding="utf-8")


def laufe(datei: pathlib.Path, umgebung: dict) -> int:
    if datei.suffix == ".ipynb":
        # nbconvert aus demselben Python wie PySpark: ein `jupyter` auf dem Pfad kann einer
        # anderen Umgebung gehoeren (und dort fehlt nbconvert oder pyspark).
        if importlib.util.find_spec("nbconvert") is None or importlib.util.find_spec("ipykernel") is None:
            print(f"{datei}: nbconvert oder ipykernel fehlt in {sys.executable} -"
                  f" {sys.executable} -m pip install nbconvert ipykernel", file=sys.stderr)
            return 1
        NOTEBOOKS.mkdir(parents=True, exist_ok=True)
        befehl = [sys.executable, "-m", "nbconvert", "--to", "notebook", "--execute",
                  "--ExecutePreprocessor.kernel_name=python3",
                  "--ExecutePreprocessor.timeout=-1", "--output-dir", str(NOTEBOOKS), str(datei)]
    else:
        befehl = [sys.executable, str(datei)]
    print(">", " ".join(befehl))
    return subprocess.run(befehl, cwd=datei.parent, env=umgebung).returncode


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    if importlib.util.find_spec("pyspark") is None:
        print(f"pyspark fehlt in {sys.executable} - mit dem Python der Umgebung starten, in der Spark"
              " laeuft (IR_BD: SW/anew/.venv/Scripts/python.exe)", file=sys.stderr)
        return 1
    schreibe_konfiguration()
    umgebung = dict(os.environ, SPARK_CONF_DIR=str(KONF))
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
        print(f"  {log.name}  {log.stat().st_size / 1e6:.2f} MB")
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
