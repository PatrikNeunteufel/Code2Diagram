# ==============================================================================
# Source.cmake for include/
# CMake Architecture V2 - App-Container
# ==============================================================================
# Jede neue Kopfdatei hier eintragen (sources.mode: explicit, kein Glob).
# ==============================================================================

dbg(DBG_OFTEN
    "${CMAKE_CURRENT_LIST_DIR}/Source.cmake
          =============================================\n" ID INCLUDE_MSG)

set(_local_headers
    "${CMAKE_CURRENT_LIST_DIR}/AppInfo.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/Application.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/sichttest/Anbindung.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/ui/Hauptfenster.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/ui/Einstellungen.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/ui/Konfiguration.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/ui/Thema.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/ui/Vorschau.hpp"
)

list(APPEND ${TARGET_NAME}_HEADERS ${_local_headers})

unset(_local_headers)
