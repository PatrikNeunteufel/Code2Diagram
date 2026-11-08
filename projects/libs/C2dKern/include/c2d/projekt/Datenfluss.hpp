/**
 * @file Datenfluss.hpp
 * @brief Der Datenfluss (Spark Schicht 1) im Projekt: welche Funktionen einen haben, und der Fluss einer
 *        Funktion, einer Zelle oder der ganzen Datei (Entscheid F6)
 *
 * Das Projekt sagt der Ableitung (diagramm/Datenfluss.hpp), was sie selbst nicht wissen kann: ob die Datei
 * ein Notebook ist (`spark`, `sc` vorgegeben), welche Namen Funktionen des Projekts sind, was eine Funktion
 * des Projekts liefert (`spark = create_spark_session()`) und welche Abfrage es aus dem SQL einer Zeile
 * gelesen hat (`spark.sql("…")`).
 */
#pragma once

#include "c2d/diagramm/Datenfluss.hpp"
#include "c2d/projekt/Funktionen.hpp"
#include "c2d/projekt/Projekt.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace c2d::projekt
{

/// Die Funktionen der Datei (voller Name, in Textfolge), deren Fluss mindestens eine Quelle, einen Schritt
/// oder eine Aktion zeigt — auch `<module>` und die Zellen eines Notebooks.
[[nodiscard]] std::vector<std::string> datenfluesseIn(const Funktionsverzeichnis& funktionen, std::size_t datei);

/// Der Datenfluss der Funktion `funktion` (voller Name) der Datei; leer = die ganze Datei: im Notebook alle
/// Zellen mit gemeinsamen Namen, sonst jede Funktion in ihrem Rahmen. Eine Zelle zeigt, was sie tut und was
/// sie aus frueheren Zellen liest.
[[nodiscard]] diagramm::Datenflussdiagramm datenflussIn(const Funktionsverzeichnis& funktionen, std::size_t datei,
                                                        std::string_view funktion);

}  // namespace c2d::projekt
