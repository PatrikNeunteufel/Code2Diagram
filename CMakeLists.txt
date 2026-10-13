# ==============================================================================
# CMakeLists.txt – Code2Diagram (Konsument von CMakeCraft)
# ==============================================================================
#
# Das Build-System (CMakeCraft) liegt nicht im Repo: Es wird beim Configure in
# der gepinnten Version (cmakecraft.pin) nach .externals/ geholt. Was gebaut
# wird, definiert ausschliesslich Solution.json.
#
#   Version wechseln:            cmakecraft.pin editieren
#   Build-System entwickeln:     -DCMAKECRAFT_LOCAL_DIR=../CMakeCraft
#
# ==============================================================================

cmake_minimum_required(VERSION 3.26)

include("${CMAKE_CURRENT_LIST_DIR}/CMakeCraftBootstrap.cmake")

# ==============================================================================
# Name und Version an den Code weiterreichen
# ==============================================================================
# SSOT ist Solution.json (apps[0]). Gelesen wird direkt aus der Datei: CMakeCraft
# erzeugt keine Kopfdatei mit der Version. Muster: LumiViz (dort seit S73).
# ------------------------------------------------------------------------------

file(READ "${CMAKE_CURRENT_LIST_DIR}/Solution.json" _c2d_solution_json)
string(JSON _c2d_app_name GET "${_c2d_solution_json}" "apps" 0 "name")
string(JSON _c2d_app_ver  GET "${_c2d_solution_json}" "apps" 0 "version")

if(_c2d_app_name STREQUAL "" OR _c2d_app_ver STREQUAL "")
    message(FATAL_ERROR
        "[AppInfo] Name oder Version fehlen in Solution.json (apps[0]) — "
        "ohne sie kann der Code sie nicht anzeigen.")
endif()

message(STATUS "[AppInfo] ${_c2d_app_name} ${_c2d_app_ver} (aus Solution.json)")

foreach(_t IN ITEMS "${_c2d_app_name}" "${_c2d_app_name}.Core")
    if(TARGET "${_t}")
        target_compile_definitions("${_t}" PRIVATE
            C2D_APP_NAME="${_c2d_app_name}"
            C2D_APP_VERSION="${_c2d_app_ver}")
    endif()
endforeach()
