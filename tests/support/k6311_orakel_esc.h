/**
 * @file k6311_orakel_esc.h
 * @brief Orakel der Schriftarten des K6311 (WP8h-a, Merkposten drucker.md Nr. 37) am ROM 12128062 (ISO-artige Folgen `ESC [ n m`).  Aufbauend auf
 *        `k6311_orakel.h` (Zelle: Nadel-9-Maske + 7 Datenspalten, 10 Spalten zu 3 Pixeln je Zeichen).
 *
 * Am Druckbild aller 94 Zeichen gemessen `[E]` (Regel fuer alle Zeichen gleich, nicht im ROM nachverfolgt):
 *   * **Breit** (`ESC [ 1 m`, bis LF): 20 Spalten zu 3 Pixeln je Zeichen (60 Pixel); Spalte 2j+1 (0-basiert) = g[j], Spalte 2j = Punkte von g[j-1] und g[j]
 *     (die Punkte beider Nachbarn, g[-1] = g[7] = leer) - die Zeichen sind lueckenlos "interpoliert", anders als am K6313 (dort stehen verbreiterte Spalten im
 *     Abstand von zwei Spalten).  Dasselbe fuer Nadel 9.
 *   * **Unterstreichen** (`ESC [ 4 m`, bis LF): Nadel 9 auf den geraden Spalten 0, 2, 4, 6, 8 der 10-Spalten-Zelle; hat das Zeichen eine eigene Nadel 9 (g j p q y , ; _ ...), bleibt es bei dieser und es gibt KEINE Striche (Regel des K6316; am K6313 dagegen ersetzen die Striche die eigene Nadel 9).
 * Nicht belegt: Schraeg (`ESC [ 3 m`), Zeichenbreiten (`ESC [ n SP K` aendert nichts, `ESC [ n K` ohne Leerzeichen druckt das K als Zeichen - das ROM kennt
 * die Funktion nicht).
 */
#pragma once
#include <array>
#include <set>
#include <string>
#include <vector>

#include "tests/support/k6311_orakel.h"

namespace k6311test {

enum class Schrift { Normal, Breit, Unterstrich };

inline Punkte erwartet(const std::vector<uint8_t>& rom, const std::string& text, Schrift s)
{
    Punkte p;
    const int zeichen = s == Schrift::Breit ? 20 : ZEICHEN;
    for (size_t n = 0; n < text.size(); ++n) {
        const uint8_t c = static_cast<uint8_t>(text[n]);
        if (c < 0x20 || c > 0x7F) continue;
        const size_t a = static_cast<size_t>(glyphe0(rom) + 8 * (zelleVon(rom, c) - 0x20));
        // g[0..6] (Datenspalten 1...7), n9[0..6] = Nadel 9 der Spalte (Bit k der Maske = Datenspalte k + 1)
        std::array<uint8_t, 8> g{}, n9{};
        g.fill(0xFF); n9.fill(1);
        for (int j = 0; j < 7; ++j) { g[static_cast<size_t>(j)] = rom[a + 1 + static_cast<size_t>(j)]; n9[static_cast<size_t>(j)] = static_cast<uint8_t>((rom[a] >> j) & 1); }
        const auto setze = [&](int spalte, uint8_t d, uint8_t k9) {
            for (int nd = 1; nd <= 8; ++nd) if (!((d >> (8 - nd)) & 1)) p.insert({static_cast<int>(n) * zeichen + spalte, nd});
            if (!k9) p.insert({static_cast<int>(n) * zeichen + spalte, 9});
        };
        if (s == Schrift::Breit) {
            for (int j = 0; j < 7; ++j) setze(2 * j + 2, g[static_cast<size_t>(j)], n9[static_cast<size_t>(j)]);
            for (int j = 0; j < 8; ++j) {
                const uint8_t a1 = j > 0 ? g[static_cast<size_t>(j - 1)] : 0xFF, b1 = g[static_cast<size_t>(j)];
                const uint8_t a9 = j > 0 ? n9[static_cast<size_t>(j - 1)] : 1, b9 = n9[static_cast<size_t>(j)];
                setze(2 * j + 1, static_cast<uint8_t>(a1 & b1), static_cast<uint8_t>(a9 & b9));
            }
        } else {
            for (int j = 0; j < 7; ++j) setze(j + 1, g[static_cast<size_t>(j)], n9[static_cast<size_t>(j)]);
            bool eigen = false;
            for (int j = 0; j < 7; ++j) eigen = eigen || !n9[static_cast<size_t>(j)];
            if (s == Schrift::Unterstrich && !eigen) for (int c = 0; c < ZEICHEN; c += 2) p.insert({static_cast<int>(n) * ZEICHEN + c + 1, 9});
        }
    }
    return p;
}

}  // namespace k6311test
