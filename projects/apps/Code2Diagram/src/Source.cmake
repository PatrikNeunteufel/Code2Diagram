# ==============================================================================
# Source.cmake for src/
# CMake Architecture V2 - App-Container
# ==============================================================================
# Jede neue Quelldatei hier eintragen (sources.mode: explicit, kein Glob).
#
#   ui/         Fenster
#   sichttest/  Steuerung durch den Tester Sichttest.exe (nur mit --testing)
#
# Lesen, Ableiten und Schreiben der Diagramme liegt nicht hier, sondern in der
# Bibliothek C2dKern (projects/libs/C2dKern) — ohne Qt.
# ==============================================================================

dbg(DBG_OFTEN
    "${CMAKE_CURRENT_LIST_DIR}/Source.cmake
          =============================================\n" ID INCLUDE_MSG)

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/AppInfo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Application.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sichttest/Anbindung.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ui/Hauptfenster.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ui/Konfiguration.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ui/Vorschau.cpp"
)

list(APPEND ${TARGET_NAME}_SOURCES ${_local_sources})

unset(_local_sources)
