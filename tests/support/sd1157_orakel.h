/**
 * @file sd1157_orakel.h
 * @brief Unabhaengiges Orakel fuer das Druckbild des SD 1157 (aus `1157zg.rom`, nicht aus einem Emulatorlauf).
 *
 * Gemeinsam fuer `test_sd1157_druck.cpp` (Selbsttest) und `test_sd1157_text.cpp` (Text ueber die SIO).
 * Regeln und Herkunft: Kopf von test_sd1157_druck.cpp und hardware.md §3, §10.5, §12.  Das Orakel wird nicht dem
 * Emulator angepasst: ein Unterschied ist ein Befund.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/peripherals/druckwerk/blattmodell.h"

#ifndef SD1157_ABZUG_DIR
#error "SD1157_ABZUG_DIR fehlt (tests/integration/CMakeLists.txt)"
#endif

namespace sd1157test {

constexpr uint16_t HPT3 = 0x0FA6;   // ROM-Adresse von `hpt3` (Ende des Teststroms: pop hl / in a,(P3BD) / bit 6): Bytemuster E1 DB ED CB 77
constexpr int ZELLE_NORMAL = 10, ZELLE_BREIT = 20;
constexpr int DRUCKSPALTE_0 = 80;   // Takt der ersten Druckspalte (hardware.md §10.1)

using Pixel = std::pair<int, int>;
using Pixelmenge = std::set<Pixel>;

// ─── Zeichengenerator und Teststrom aus dem Abzug ───────────────────────────
struct Zg {
    std::vector<uint8_t> b;
    static Zg laden()
    {
        std::ifstream f(std::string(SD1157_ABZUG_DIR) + "/1157zg.rom", std::ios::binary);
        Zg z;
        z.b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        return z;
    }
    const uint8_t* glyphe(int code) const { return &b[static_cast<size_t>((code - 0x20) * 8)]; }
};

enum class Modus { Normal, Breit, Schraeg };
struct Segment { std::string text; Modus modus; };
struct Zeile { std::vector<Segment> seg; };

// Teststrom ab 0300H: CR LF, Text mit ESC [ 1 m / ESC [ 3 m, NULs, dann Zeilen 2 und 3 als Bereichsmarken (Byte >= 80H =
// Anfangszeichen).  Ohne ZG2 endet die erste Bereichszeile bei '_' und die zweite bei '~' (hpt7, hpt12); danach `SI LF` (hardware.md §12).
inline std::vector<Zeile> testZeilen(const Zg& zg)
{
    std::vector<Zeile> z;
    size_t o = 0x300;
    while (zg.b[o] == 0x0D || zg.b[o] == 0x0A) ++o;
    Zeile titel;
    Modus modus = Modus::Normal;
    std::string text;
    auto schliesse = [&] { if (!text.empty()) titel.seg.push_back({text, modus}); text.clear(); };
    while (zg.b[o] != 0x00) {
        if (zg.b[o] == 0x1B && zg.b[o + 1] == '[') {
            schliesse();
            const uint8_t p = zg.b[o + 2];
            EXPECT_EQ(zg.b[o + 3], 'm');
            modus = p == '1' ? Modus::Breit : p == '3' ? Modus::Schraeg : Modus::Normal;
            o += 4;
        } else {
            text.push_back(static_cast<char>(zg.b[o++]));
        }
    }
    schliesse();
    z.push_back(titel);
    while (zg.b[o] == 0x00) ++o;   // 39 x NUL
    for (; zg.b[o] != 0xFF; ++o) {
        if (zg.b[o] < 0x80) continue;   // LF, LF2, CR
        Zeile zl;
        std::string t;
        const int von = zg.b[o] & 0x7F, bis = von == 0x21 ? '_' : '~';
        for (int c = von; c <= bis; ++c) t.push_back(static_cast<char>(c));
        zl.seg.push_back({t, Modus::Normal});
        z.push_back(zl);
        if (bis == '~') break;   // ohne ZG2 folgt `SI LF`
    }
    return z;
}

inline void dot(Pixelmenge& m, int takt, int y)
{
    m.insert({2 * takt, y});       // 2 Pixel je Takt, Punktbreite = 1 Takt (Parametersatz)
    m.insert({2 * takt + 1, y});
}

// Erwartete Punkte einer Zeile; y0 = Zeile der Nadel 1, nadel_teilung in Rasterzeilen
inline Pixelmenge orakelZeile(const Zeile& zeile, const Zg& zg, int y0, int teilung)
{
    Pixelmenge m;
    int c0 = DRUCKSPALTE_0;
    for (const Segment& s : zeile.seg) {
        for (char ch : s.text) {
            const uint8_t* g = zg.glyphe(static_cast<uint8_t>(ch));
            auto n9 = [&](int k) { return !((g[7] >> (6 - k)) & 1); };
            switch (s.modus) {
                case Modus::Normal:
                    for (int k = 0; k < 7; ++k) {
                        for (int j = 0; j < 8; ++j) if (!((g[k] >> j) & 1)) dot(m, c0 + k, y0 + teilung * j);
                        if (n9(k)) dot(m, c0 + k, y0 + teilung * 8);
                    }
                    c0 += ZELLE_NORMAL;
                    break;
                case Modus::Breit: {
                    uint8_t vor = 0xFF;   // je Zeichen neu (m1aeb)
                    for (int k = 0; k < 8; ++k) {   // Spalte 0-6, dann Spalte 6 wiederholt
                        const uint8_t byte = k < 7 ? static_cast<uint8_t>(g[k] & vor) : vor;
                        for (int j = 0; j < 8; ++j) if (!((byte >> j) & 1)) dot(m, c0 + 2 * k, y0 + teilung * j);
                        const bool f9 = k < 7 ? (n9(k) || (k > 0 && n9(k - 1))) : n9(6);
                        if (f9) dot(m, c0 + 2 * k, y0 + teilung * 8);
                        if (k < 7) vor = g[k];
                    }
                    c0 += ZELLE_BREIT;
                    break;
                }
                case Modus::Schraeg:
                    for (int k = 0; k < 7; ++k) {
                        for (int j = 0; j < 8; ++j) if (!((g[k] >> j) & 1)) dot(m, c0 + k + (8 - j), y0 + teilung * j);
                        if (n9(k)) dot(m, c0 + k, y0 + teilung * 8);
                    }
                    c0 += ZELLE_NORMAL;
                    break;
            }
        }
    }
    return m;
}

// Bandweises Vergleichen eines Blatts mit der erwarteten Pixelmenge; "" = gleich, sonst Meldung mit ASCII-Abzug
inline std::string vergleiche(const Blattmodell& b, const Pixelmenge& erw, int y0, int y1, int dx = 0)
{
    int fehlt = 0, zuviel = 0;
    Pixel erstes_fehlt{-1, -1}, erstes_zuviel{-1, -1};
    for (const Pixel& p : erw) {
        if (p.second < y0 || p.second >= y1) continue;
        if (!b.punkt(p.first + dx, p.second)) { if (!fehlt++) erstes_fehlt = p; }
    }
    uint64_t erw_im_band = 0;
    for (const Pixel& p : erw) if (p.second >= y0 && p.second < y1) ++erw_im_band;
    for (int y = y0; y < y1 && y < b.hoehe(); ++y)
        for (int x = 0; x < b.breite(); ++x)
            if (b.punkt(x, y) && !erw.count({x - dx, y})) { if (!zuviel++) erstes_zuviel = {x, y}; }
    if (!fehlt && !zuviel) return "";
    std::ostringstream o;
    o << "Band y=" << y0 << ".." << y1 - 1 << ": " << fehlt << " erwartete Punkte fehlen, " << zuviel << " zuviel (erwartet "
      << erw_im_band << ")\n";
    const Pixel an = fehlt ? erstes_fehlt : erstes_zuviel;
    const int x0 = std::max(0, an.first - 40), ya = std::max(y0, an.second - 4);
    Blattmodell soll(b.breite());
    for (const Pixel& p : erw) soll.setze(p.first + dx, p.second);
    o << "erstes Fehlen bei x=" << erstes_fehlt.first << " y=" << erstes_fehlt.second << ", erstes Zuviel bei x=" << erstes_zuviel.first
      << " y=" << erstes_zuviel.second << "\nerwartet (x " << x0 << "..," << 2 << " Pixel je Zeichen):\n"
      << blattAscii(soll, x0, ya, 160, 12, 2, 1) << "gedruckt:\n" << blattAscii(b, x0, ya, 160, 12, 2, 1);
    return o.str();
}

}  // namespace sd1157test
