# ==============================================================================
# Source.cmake for libs/C2dKern/src/
# CMake Architecture V2 - Source Collection
# ==============================================================================
# Der Kern von Code2Diagram — ohne Qt. Jede neue Datei hier eintragen
# (sources.mode: explicit, kein Glob).
#
# Schichten, jede kennt nur die ueber ihr:
#   quelle/    Ort im Quelltext, Meldungen
#   lexer/     der allgemeine Lexer (Text -> Token), gesteuert von einer Sprachtabelle
#   modell/    das Codemodell — fuer alle Sprachen gleich
#   sprache/   je Sprache eine Tabelle und ein Leser (Token -> Codemodell)
#   diagramm/  die Diagramm-Modelle und ihre Ableitung aus dem Codemodell
#   ausgabe/   die Formate (Diagramm -> Text)
#   projekt/   ein Ordner als Projekt: Projektdatei, Suche, Lesen ueber Dateien
#              hinweg, Aufloesen von Aufrufen; der Aufrufgraph, weil nur das Projekt
#              weiss, was ein Aufruf meint (kennt sprache, modell und das Modell des
#              Aufrufgraphen — kein Diagramm kennt das Projekt)
# ==============================================================================

dbg(${DBG_OFTEN}
    "${CMAKE_CURRENT_LIST_DIR}/Source.cmake
          =============================================\n" ID INCLUDE_MSG)

set(_c2d_inc "${CMAKE_CURRENT_LIST_DIR}/../include/c2d")

set(_local_sources
    "${CMAKE_CURRENT_LIST_DIR}/lexer/Lexer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/modell/Code.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/Leser.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/Sprache.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/c/CSprache.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/cfamilie/Bedingt.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/cfamilie/CFamilie.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/cfamilie/Logik.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/cpp/CppSprache.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/diagramm/Ableitung.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/diagramm/Anordnung.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/diagramm/Aufrufgraph.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/diagramm/Farben.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/diagramm/Flussdiagramm.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/diagramm/Struktogramm.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/diagramm/StruktoAnordnung.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/diagramm/Wahrheitstabelle.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/diagramm/TabellenAnordnung.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ausgabe/Ausgabe.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ausgabe/Csv.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ausgabe/Dot.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ausgabe/Drawio.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ausgabe/Mermaid.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ausgabe/Svg.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/projekt/Aufrufgraph.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/projekt/Json.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/projekt/Projekt.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/projekt/Projektdatei.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/projekt/Typen.cpp"
)

set(_local_headers
    "${_c2d_inc}/quelle/Meldung.hpp"
    "${_c2d_inc}/lexer/Lexer.hpp"
    "${_c2d_inc}/modell/Code.hpp"
    "${_c2d_inc}/sprache/Sprache.hpp"
    "${_c2d_inc}/diagramm/Ableitung.hpp"
    "${_c2d_inc}/diagramm/Anordnung.hpp"
    "${_c2d_inc}/diagramm/Aufrufgraph.hpp"
    "${_c2d_inc}/diagramm/Farben.hpp"
    "${_c2d_inc}/diagramm/Flussdiagramm.hpp"
    "${_c2d_inc}/diagramm/Strukturdiagramm.hpp"
    "${_c2d_inc}/diagramm/Struktogramm.hpp"
    "${_c2d_inc}/diagramm/Wahrheitstabelle.hpp"
    "${_c2d_inc}/diagramm/Zustandsdiagramm.hpp"
    "${_c2d_inc}/ausgabe/Ausgabe.hpp"
    "${_c2d_inc}/projekt/Aufrufgraph.hpp"
    "${_c2d_inc}/projekt/Projekt.hpp"
    "${_c2d_inc}/projekt/Projektdatei.hpp"
    "${_c2d_inc}/projekt/Typen.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/projekt/Json.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/Leser.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/cfamilie/Bedingt.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/cfamilie/Logik.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/sprache/cfamilie/CFamilie.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/ausgabe/Text.hpp"
)

list(APPEND ${TARGET_NAME}_SOURCES ${_local_sources})
list(APPEND ${TARGET_NAME}_HEADERS ${_local_headers})

unset(_local_sources)
unset(_local_headers)
unset(_c2d_inc)
