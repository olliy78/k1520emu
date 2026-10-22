/**
 * @file kermit80_hilfen.h
 * @brief Gemeinsame Bildhilfen der Kermit-80-Systemfaelle im TERMINALMODUS (CONNECT) —
 *        `test_kermit80.cpp` (Kermit80Pc, PC-Server mit Anmeldezwang) und `test_kermit80_wega.cpp`
 *        (Kermit80Wega, C-Kermit 4.2 der WEGA).
 *
 * Nach CONNECT laeuft der Cursor der einfachen Datensichtstation ueber den Bildrand; die Eingabeaufforderung
 * `Kermit-80   0A:>` steht danach nicht mehr in der LETZTEN Bildzeile.  Darum sucht @ref nackterPrompt in
 * ALLEN Zeilen, und @ref wartePrompt verlangt, dass das Bild eine Weile unveraendert bleibt (waehrend einer
 * Uebertragung zaehlt Kermit-80 laufend Pakete; ein alter nackter Prompt im Bild genuegt deshalb nicht).
 */
#pragma once

#include <string>

namespace k80h {

inline std::string rtrimBild(std::string z) {
    while (!z.empty() && (z.back() == ' ' || z.back() == '\0')) z.pop_back();
    return z;
}

/// Letzte nichtleere Zeile eines Bildes (ohne Fuellzeichen).
inline std::string letzteZeile(const std::string& bild) {
    std::string l;
    size_t a = 0;
    while (a <= bild.size()) {
        size_t e = bild.find('\n', a);
        if (e == std::string::npos) e = bild.size();
        const std::string z = rtrimBild(bild.substr(a, e - a));
        if (!z.empty()) l = z;
        a = e + 1;
    }
    return l;
}

/// Steht irgendwo eine nackte Kermit-80-Eingabeaufforderung ("Kermit-80   0A:>" ohne Befehl dahinter) im Bild?
inline bool nackterPrompt(const std::string& bild) {
    size_t p = 0;
    while (p <= bild.size()) {
        size_t e = bild.find('\n', p);
        if (e == std::string::npos) e = bild.size();
        const std::string z = rtrimBild(bild.substr(p, e - p));
        if (z.rfind("Kermit-80", 0) == 0 && !z.empty() && z.back() == '>') return true;
        p = e + 1;
    }
    return false;
}

/// Wartet auf eine nackte Eingabeaufforderung, die @p ruhe Sekunden Maschinenzeit unveraendert im Bild steht.
/// @p scheibe laesst die Maschine(n) um @p scheibeSek Maschinenzeit weiterlaufen, @p bild liefert das Bild.
/// @p aenderung: das Bild muss sich seit dem Aufruf geaendert haben (das Echo des Befehls); @p start ist das
/// Bild "vorher" (leer = jetziges).
template <class Scheibe, class Bild>
bool wartePrompt(Scheibe&& scheibe, Bild&& bild, double scheibeSek, double max_sekunden, bool aenderung = true,
                 std::string start = "", double ruhe = 1.5) {
    if (start.empty()) start = bild();
    std::string zuletzt = start;
    double seit = 0;
    bool geaendert = !aenderung;
    for (double n = 0; n < max_sekunden; n += scheibeSek) {
        scheibe();
        const std::string jetzt = bild();
        if (jetzt != zuletzt) {
            zuletzt = jetzt;
            seit = 0;
            if (jetzt != start) geaendert = true;
        } else {
            seit += scheibeSek;
        }
        if (geaendert && seit >= ruhe && nackterPrompt(jetzt)) return true;
    }
    return false;
}

}  // namespace k80h
