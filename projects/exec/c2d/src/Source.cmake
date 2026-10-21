# ==============================================================================
# Source.cmake for exec/c2d/src/
# CMake Architecture V2 - Source Collection
# ==============================================================================
# c2d — das Werkzeug fuer die Kommandozeile: Quelldatei rein, Diagramm-Text raus.
# ==============================================================================

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
)
if(WIN32)
    list(APPEND _local_sources "${CMAKE_CURRENT_LIST_DIR}/c2d.rc")
endif()

list(APPEND ${TARGET_NAME}_SOURCES ${_local_sources})

unset(_local_sources)
