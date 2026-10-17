# ==============================================================================
# Source.cmake for libs/C2dKern/tests/unit/
# CMake Architecture V2 - Source Collection
# ==============================================================================
# Unit-Tests des Kerns (doctest): ohne Qt, ohne Fenster, in Sekundenbruchteilen.
# Jede neue Testdatei hier eintragen.
# ==============================================================================

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/test_main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Lexer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Bedingt.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_CSprache.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_CppSprache.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Diagramme.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Ausgabe.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Einbettung.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Projekt.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Automat.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Varianten.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Rahmen.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Schalter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Aufrufgraph.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Typen.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Struktogramm.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Wahrheitstabelle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Entscheidungstabelle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Zuordnung.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Konfigurationstabelle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Schaltstellen.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Breite.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test_Python.cpp"
)

set(_local_headers
    "${CMAKE_CURRENT_LIST_DIR}/Beispiele.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/TempOrdner.hpp"
)

list(APPEND ${TARGET_NAME}_SOURCES ${_local_sources})
list(APPEND ${TARGET_NAME}_HEADERS ${_local_headers})

unset(_local_sources)
unset(_local_headers)
