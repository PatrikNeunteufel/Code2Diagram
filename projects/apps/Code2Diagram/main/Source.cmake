# ==============================================================================
# Source.cmake for main/
# CMake Architecture V2 - App-Container
# ==============================================================================

dbg(DBG_OFTEN
    "${CMAKE_CURRENT_LIST_DIR}/Source.cmake
          =============================================\n" ID INCLUDE_MSG)

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Code2Diagram.qrc"
)
if(WIN32)
    list(APPEND _local_sources "${CMAKE_CURRENT_LIST_DIR}/Code2Diagram.rc")
endif()

list(APPEND ${TARGET_NAME}_SOURCES ${_local_sources})

unset(_local_sources)
