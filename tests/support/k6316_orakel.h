/**
 * @file k6316_orakel.h
 * @brief Unabhaengiges Orakel fuer das Druckbild des K6316 (Entwurf 49, WP6a): erwartet wird, was der Zeichengenerator
 *        **im ROM** und die Regeln des Handbuchs verlangen, nicht was der Emulator zeichnet.
 *
 * Herleitung aus der Firmware (am IFSS-Abzug nachgelesen: Zeichenausgabe `sub_1303`, 1303H–1334H): die Matrix eines
 * Zeichens liegt bei **2400H + 8 * Code**, je Spalte ein Byte, 8 Spalten je Zeichen.  Das Byte geht *unveraendert* auf das
 * Nadelregister 8DH, in dem 0 = Nadel schlaegt an: ein Punkt in Zeile k (k = 0 oben = Nadel 1) ist also das Bit
 * (7 - k) des **invertierten** ROM-Bytes.  Zeichenabstand 1/10 Zoll = 10 Drucktakte; die Spalten stehen im Abstand eines
 * Drucktakts (1/100 Zoll).  Spalte 0 der Zeichen `, ; @ _ g j p q y` ist Steuerdaten (Unterlaenge), kein Druckbild.
 *
 * Raster des Blatts (Kern, 300 dpi in x, 216 dpi in y): Drucktakt = 3 Pixel, Zeilenabstand der Nadeln = 3 Einheiten,
 * Punkt 3 x 2.  Der Ursprung (links oben) ist der einzige frei gefittete Wert: er haengt am mechanischen Nullpunkt des
 * Geraets, den kein Dokument festlegt.
 */
#pragma once
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/peripherals/druckwerk/blattmodell.h"

namespace k6316test {

constexpr int DRUCKTAKT = 3;      ///< Pixel je Drucktakt
constexpr int ZEICHEN   = 30;     ///< Pixel je Zeichen (1/10 Zoll)
constexpr int NADEL     = 3;      ///< Einheiten zwischen zwei Nadeln

/// ROM-Abzug eines Firmwaresatzes (5 Chips hintereinander, 0000H–27FFH).
inline std::vector<uint8_t> rom(const std::string& dir, bool v24)
{
    std::vector<uint8_t> r;
    for (int i = 1; i <= 5; ++i) {
        std::ifstream f(dir + (v24 ? "/v24/V" : "/ifss/I") + std::to_string(i) + ".BIN", std::ios::binary);
        r.insert(r.end(), std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    return r;
}

/// Spaltenbytes (Bit 7 - k = Nadel k+1 schlaegt an) des Zeichens @p c aus dem ROM.
inline std::vector<uint8_t> glyphe(const std::vector<uint8_t>& rom, int c)
{
    std::vector<uint8_t> g;
    for (int i = 0; i < 8; ++i) g.push_back(static_cast<uint8_t>(~rom[static_cast<size_t>(0x2400 + (c & 0x7F) * 8 + i)]));
    return g;
}

inline bool hatSteuerSpalte0(int c) { return c == ',' || c == ';' || c == '@' || c == '_' || c == 'g' || c == 'j' || c == 'p' || c == 'q' || c == 'y'; }

using Punkte = std::set<std::pair<int, int>>;   ///< (x, y) der linken oberen Ecke eines Punkts

/// Erwartete Punkte eines Textes im Normaldruck relativ zum Ursprung (0, 0) = Spalte 0 des ersten Zeichens, Nadel 1.
inline Punkte erwartet(const std::vector<uint8_t>& rom, const std::string& text)
{
    Punkte p;
    for (size_t n = 0; n < text.size(); ++n) {
        const auto g = glyphe(rom, text[n]);
        for (int c = 0; c < 8; ++c) {
            if (c == 0 && hatSteuerSpalte0(text[n])) continue;
            for (int k = 0; k < 8; ++k)
                if ((g[static_cast<size_t>(c)] >> (7 - k)) & 1) p.insert({static_cast<int>(n) * ZEICHEN + c * DRUCKTAKT, k * NADEL});
        }
    }
    return p;
}

/// Alle Punkte des Blatts (linke obere Ecke von 3 x 2 Pixeln) mit y in [ya, yb).
inline Punkte gedruckt(const Blattmodell& b, int ya, int yb, int xa = 0, int xb = 1 << 30)
{
    Punkte p;
    xb = std::min(xb, b.breite());
    for (int y = std::max(ya, 0); y < yb && y < b.hoehe(); ++y)
        for (int x = xa; x < xb; ++x)
            if (b.punkt(x, y) && !(y > 0 && b.punkt(x, y - 1)) && !(x > 0 && b.punkt(x - 1, y))) p.insert({x, y});
    return p;
}

/// Zeilenbaender des Blatts: zusammenhaengende Bereiche beschriebener Rasterzeilen (Luecke > 6 trennt).
inline std::vector<std::pair<int, int>> baender(const Blattmodell& b)
{
    std::vector<std::pair<int, int>> r;
    int start = -1, letzte = -100;
    for (int y = 0; y < b.hoehe(); ++y) {
        bool any = false;
        for (int x = 0; x < b.breite() && !any; ++x) any = b.punkt(x, y);
        if (!any) continue;
        if (start < 0 || y - letzte > 6) { if (start >= 0) r.push_back({start, letzte + 1}); start = y; }
        letzte = y;
    }
    if (start >= 0) r.push_back({start, letzte + 1});
    return r;
}

/// Vergleicht den Text mit dem Band [ya, yb): der Ursprung wird aus dem ersten Punkt gewonnen (einziger freier Wert),
/// dann muss die Punktmenge im Fenster des Textes **gleich** sein.  Rueckgabe: "" = gleich, sonst Beschreibung; @p ursprung_x
/// liefert den Ursprung in x.
inline std::string vergleiche(const Blattmodell& b, const std::vector<uint8_t>& rom, const std::string& text, int ya, int yb,
                              int* ursprung_x = nullptr)
{
    const Punkte exp = erwartet(rom, text);
    const Punkte alle = gedruckt(b, ya, yb);
    if (exp.empty() || alle.empty()) return "leer";
    int ex = 1 << 30, ey = 1 << 30, ax = 1 << 30, ay = 1 << 30;
    for (auto& q : exp) { ex = std::min(ex, q.first); ey = std::min(ey, q.second); }
    for (auto& q : alle) { ax = std::min(ax, q.first); ay = std::min(ay, q.second); }
    const int ox = ax - ex, oy = ay - ey;
    if (ursprung_x) *ursprung_x = ox;
    Punkte act;
    const int xb = ox + static_cast<int>(text.size()) * ZEICHEN;
    // Zeile 9 (Nadel 9, y = 24) ist kein Teil der 8 Bit des Zeichengenerators: sie gehoert der Unterlaenge/dem Unterstreichen
    for (auto& q : alle) if (q.first >= ox && q.first < xb && q.second - oy < 8 * NADEL) act.insert({q.first - ox, q.second - oy});
    std::string msg;
    int n = 0;
    for (auto& q : exp) if (!act.count(q) && n++ < 5) msg += " fehlt(" + std::to_string(q.first) + "," + std::to_string(q.second) + ")";
    n = 0;
    for (auto& q : act) if (!exp.count(q) && n++ < 5) msg += " zuviel(" + std::to_string(q.first) + "," + std::to_string(q.second) + ")";
    return msg;
}

}  // namespace k6316test
