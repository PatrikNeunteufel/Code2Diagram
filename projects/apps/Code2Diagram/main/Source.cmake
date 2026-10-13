# ==============================================================================
# Source.cmake for main/
# CMake Architecture V2 - App-Container
# ==============================================================================

dbg(DBG_OFTEN
    "${CMAKE_CURRENT_LIST_DIR}/Source.cmake
          =============================================\n" ID INCLUDE_MSG)

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
)

list(APPEND ${TARGET_NAME}_SOURCES ${_local_sources})

unset(_local_sources)
