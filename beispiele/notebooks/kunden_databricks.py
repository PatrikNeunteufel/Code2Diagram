# Databricks notebook source
# MAGIC %md
# MAGIC # Kunden
# MAGIC Ein Databricks-Notebook in Python: Zellen zwischen `COMMAND`, eine Zelle in SQL, eine mit `%run`.

# COMMAND ----------

# MAGIC %run ./einstellungen

# COMMAND ----------

# DBTITLE 1,Kunden laden
kunden = spark.read.table("roh.kunden")
aktiv = kunden.filter("aktiv = true")

# COMMAND ----------

# MAGIC %sql
# MAGIC CREATE OR REPLACE TABLE gold.kunden_je_land AS
# MAGIC SELECT k.land, count(*) AS anzahl
# MAGIC FROM roh.kunden k
# MAGIC JOIN roh.laender l ON l.code = k.land
# MAGIC GROUP BY k.land

# COMMAND ----------

for zeile in spark.sql("SELECT land, anzahl FROM gold.kunden_je_land").collect():
    if zeile.anzahl > 1000:
        print(zeile.land)
