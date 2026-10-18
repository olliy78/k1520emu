# Kermit-Bibliothek (doc/design/36_kermit.md §4.1).
# AP K2: Protokollschicht als statische Bibliothek k1520kermit_core — ohne C-ABI.
# AP K3: Netzleitungen (leitung_netz.*, loginskript.*) ueber k1520_serial_codec/_net.
# AP K4: libk1520kermit.so/.dll (C-ABI, src/api.cpp) — unten.
# Keine Abhaengigkeit von Z80, Karten oder k1520_fs; kein POSIX (std::filesystem).
find_package(Threads REQUIRED)

add_library(k1520kermit_core STATIC
    src/leitung_netz.cpp
    src/loginskript.cpp
    src/paket.cpp
    src/leitung.cpp
    src/dateien.cpp
    src/protokoll.cpp
    src/server.cpp
)
target_include_directories(k1520kermit_core PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(k1520kermit_core PUBLIC Threads::Threads k1520_serial_codec k1520_serial_net)
# K4 baut daraus eine gemeinsame Bibliothek: lageunabhaengiger Code von Anfang an.
set_target_properties(k1520kermit_core PROPERTIES
    CXX_STANDARD 17
    POSITION_INDEPENDENT_CODE ON
    # Wie libk1520core/libk1520disk im Wurzel-build/ (dort suchen app/paths.py und die Tests;
    # unter Windows liegt die DLL neben den Testprogrammen).
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# AP K4: gemeinsame Bibliothek mit stabiler C-ABI (libk1520kermit.so / k1520kermit.dll) fuer
# Python und die Kommandozeile — benannt und gebaut wie k1520disk (SHARED, K1520K_BUILD_SHARED statt
# K1520_BUILD_SHARED, keine SOVERSION).  Die statischen Bausteine (Protokoll, Codecs, Sockets)
# werden hineingelinkt; ws2_32 kommt ueber k1520_serial_net.
add_library(k1520kermit SHARED src/api.cpp)
target_link_libraries(k1520kermit PRIVATE k1520kermit_core)
target_include_directories(k1520kermit PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_definitions(k1520kermit PRIVATE K1520K_BUILD_SHARED)
set_target_properties(k1520kermit PROPERTIES
    CXX_STANDARD 17
    POSITION_INDEPENDENT_CODE ON
    # Wie libk1520core/libk1520disk im Wurzel-build/ (dort suchen app/paths.py und die Tests;
    # unter Windows liegt die DLL neben den Testprogrammen).
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
