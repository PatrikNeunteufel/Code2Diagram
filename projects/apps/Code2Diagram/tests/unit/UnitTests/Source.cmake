# ==============================================================================
# Source.cmake for tests/unit/UnitTests/
# CMake Architecture V2 - App-Container
# ==============================================================================
# Unit-Tests der Anwendung (doctest): das Fenster, ohne es zu zeigen. Was ohne
# Qt pruefbar ist, gehoert zu den Tests des Kerns (projects/libs/C2dKern/tests).
# Jede neue Testdatei hier eintragen.
# ==============================================================================

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/test_main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Einstellungen.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Hauptfenster.cpp"
)

list(APPEND ${TARGET_NAME}_SOURCES ${_local_sources})

unset(_local_sources)
