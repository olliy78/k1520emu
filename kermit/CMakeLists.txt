# Kermit-Bibliothek (doc/design/36_kermit.md §4.1).
# AP K2: Protokollschicht als statische Bibliothek k1520kermit_core — ohne Netz,
# ohne C-ABI.  K3 (Netzleitungen ueber k1520_serial_codec/k1520_serial_net) und
# K4 (libk1520kermit.so mit C-ABI) setzen hier an, ohne dieses Ziel umzubauen.
# Keine Abhaengigkeit von Z80, Karten oder k1520_fs; kein POSIX (std::filesystem).
find_package(Threads REQUIRED)

add_library(k1520kermit_core STATIC
    src/paket.cpp
    src/leitung.cpp
    src/dateien.cpp
    src/protokoll.cpp
    src/server.cpp
)
target_include_directories(k1520kermit_core PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(k1520kermit_core PUBLIC Threads::Threads)
# K4 baut daraus eine gemeinsame Bibliothek: lageunabhaengiger Code von Anfang an.
set_target_properties(k1520kermit_core PROPERTIES
    CXX_STANDARD 17
    POSITION_INDEPENDENT_CODE ON
)
