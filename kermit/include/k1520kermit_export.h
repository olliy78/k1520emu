/**
 * @file k1520kermit_export.h
 * @brief Sichtbarkeit der C-ABI-Funktionen von `libk1520kermit` — `K1520K_API` vor jeder
 *        exportierten Funktion (Muster `core/api/k1520_export.h`).
 *
 * Unter Linux exportiert eine `.so` per Vorgabe alles, unter Windows exportiert eine DLL
 * per Vorgabe NICHTS: ohne Ausfuhrkennzeichnung baut MSVC eine leere `k1520kermit.dll`,
 * und `ctypes` findet keine einzige Funktion.
 *   - `K1520K_BUILD_SHARED`  beim Bau der Bibliothek selbst  → ausfuehren
 *   - `K1520K_USE_SHARED`    bei einem C/C++-Nutzer der DLL  → einfuehren
 *   - keins von beiden       statisch mitgebaut / Werkzeuge  → neutral
 * Eigener Name statt `K1520_API`, weil beide Makros in einer Uebersetzungseinheit
 * nebeneinander stehen duerfen (andere Bibliothek, andere Richtung).
 */
#pragma once

#if defined(_WIN32)
#  if defined(K1520K_BUILD_SHARED)
#    define K1520K_API __declspec(dllexport)
#  elif defined(K1520K_USE_SHARED)
#    define K1520K_API __declspec(dllimport)
#  else
#    define K1520K_API
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define K1520K_API __attribute__((visibility("default")))
#else
#  define K1520K_API
#endif
