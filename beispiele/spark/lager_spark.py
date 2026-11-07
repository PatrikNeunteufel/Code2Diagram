"""Beispiel fuer den Datenfluss (Spark Schicht 1): ein Lager-Bericht mit DataFrames, SQL und einem RDD."""
from pyspark.sql import SparkSession, functions as F
from pyspark.sql.functions import udf


def sitzung():
    return (SparkSession.builder.appName("Lager")
            .master("local[4]")
            .config("spark.sql.shuffle.partitions", 8)
            .getOrCreate())


def kuerze(text):
    return text.strip()[:20]


def bericht(ein, aus, nur_neu):
    spark = sitzung()
    artikel = (spark.read.format("csv")
               .option("header", "true")
               .load(ein + "/artikel.csv"))
    bestand = spark.read.parquet(ein + "/bestand")
    if nur_neu:
        bestand = bestand.filter(F.col("datum") >= "2026-01-01")
    name = udf(kuerze)
    lager = (bestand.join(artikel, "artikel_id")
             .withColumn("wert", F.col("menge") * F.col("preis"))
             .withColumn("kurz", name("titel")))
    lager.cache()
    summe = lager.groupBy("lager").agg(F.sum("wert").alias("summe"))
    summe.createOrReplaceTempView("summen")
    oben = spark.sql("SELECT lager, summe FROM summen WHERE summe > 1000 ORDER BY summe DESC")
    oben.write.mode("overwrite").parquet(aus)
    lager.select("kurz").distinct().show()
    gelesen = spark.read.parquet(aus)
    print(gelesen.count())


def woerter(pfad):
    spark = sitzung()
    sc = spark.sparkContext
    zeilen = sc.textFile(pfad)
    paare = zeilen.flatMap(lambda z: z.split(" ")).map(lambda w: (w, 1))
    zahlen = paare.reduceByKey(lambda a, b: a + b)
    print(zahlen.take(10))
    print(zahlen.count())
