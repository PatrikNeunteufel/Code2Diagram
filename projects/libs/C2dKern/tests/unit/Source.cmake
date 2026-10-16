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
)

set(_local_headers
    "${CMAKE_CURRENT_LIST_DIR}/Beispiele.hpp"
)

list(APPEND ${TARGET_NAME}_SOURCES ${_local_sources})
list(APPEND ${TARGET_NAME}_HEADERS ${_local_headers})

unset(_local_sources)
unset(_local_headers)
