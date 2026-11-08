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
 * `erwartet`/`vergleiche` (erste Fassung) pruefen nur Nadel 1-8 im Normaldruck.  Vollstaendig ist `treffer`/`vergleicheZeile`
 * nach doc/printer/k6316/hardware.md §10: Spalte 0 = Nadel-9-Maske, Breit, Schraeg, Unterstrich.
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

// ─── Schriftarten nach hardware.md §10 (aus dem ROM erschlossen, Zellenbau sub_1303 / sub_0865 / 133DH) ─────────────────
//
// Punkte als (Takt, Nadel 1..9) relativ zum Zeichenanfang; Bit 0 = Nadel schlaegt an im ROHEN ROM-Byte.  Spalte 0 der Glyphe
// ist KEINE Druckspalte, sondern die Nadel-9-Maske (Bit k = Takt k); die Datenspalte g[c] (c = 1..7) steht im Takt c - 1.

enum class Schrift { Normal = 0, Breit = 1, Schraeg = 2 };

using Treffer = std::set<std::pair<int, int>>;   ///< (Takt, Nadel)

inline std::vector<uint8_t> roheGlyphe(const std::vector<uint8_t>& rom, int c)
{
    std::vector<uint8_t> g;
    for (int i = 0; i < 8; ++i) g.push_back(rom[static_cast<size_t>(0x2400 + (c & 0x7F) * 8 + i)]);
    return g;
}

inline bool trifft(uint8_t roh, int bit) { return ((roh >> bit) & 1) == 0; }

/// Takte je Zeichen (§10.9): Breit belegt zwei Zellen.
inline int breite(Schrift s) { return s == Schrift::Breit ? 20 : 10; }

/// Treffer eines Zeichens; @p unterstrich = Modus + 3 (§10.5: Nadel 9 auf 0,2,4,6,8 je Zelle ohne eigenes Nadel-9-Bild).
inline Treffer treffer(const std::vector<uint8_t>& rom, int code, Schrift s, bool unterstrich = false)
{
    const auto g = roheGlyphe(rom, code);
    Treffer t;
    switch (s) {
    case Schrift::Normal:   // §10.2
        for (int c = 1; c <= 7; ++c)
            for (int j = 1; j <= 8; ++j) if (trifft(g[static_cast<size_t>(c)], 8 - j)) t.insert({c - 1, j});
        for (int k = 0; k <= 7; ++k) if (trifft(g[0], k)) t.insert({k, 9});
        break;
    case Schrift::Breit: {   // §10.3: Spalte m = g[m] & g[m+1] (roh) im Takt 2m, mit g[0] = g[8] = FF
        uint8_t gg[9];
        gg[0] = 0xFF; gg[8] = 0xFF;
        for (int c = 1; c <= 7; ++c) gg[c] = g[static_cast<size_t>(c)];
        for (int m = 0; m <= 7; ++m) {
            const uint8_t w = static_cast<uint8_t>(gg[m] & gg[m + 1]);
            for (int j = 1; j <= 8; ++j) if (trifft(w, 8 - j)) t.insert({2 * m, j});
        }
        for (int j = 0; j <= 6; ++j) if (trifft(g[0], j)) { t.insert({2 * j, 9}); t.insert({2 * j + 2, 9}); }
        break;
    }
    case Schrift::Schraeg:   // §10.4: Spaltenbit b (Nadel 8 - b) der Spalte c im Takt c + b; Nadel 9 ungeschert
        for (int c = 1; c <= 7; ++c)
            for (int b = 0; b <= 7; ++b) if (trifft(g[static_cast<size_t>(c)], b)) t.insert({c + b, 8 - b});
        for (int k = 0; k <= 7; ++k) if (trifft(g[0], k)) t.insert({k, 9});
        break;
    }
    if (unterstrich) {
        // je Zelle (Breit: zwei) ohne eigenes Nadel-9-Bild: N[0] (Takte 0..7) und N[1] (Takte 8, 9) der Zelle sind FF
        const int zellen = s == Schrift::Breit ? 2 : 1;
        for (int z = 0; z < zellen; ++z) {
            bool eigen = false;
            for (auto& q : t) if (q.second == 9 && q.first >= 10 * z && q.first < 10 * z + 10) eigen = true;
            if (!eigen) for (int k = 0; k <= 8; k += 2) t.insert({10 * z + k, 9});
        }
    }
    return t;
}

/// Ein Stueck Zeile in einer Schriftart ab Takt @p start (relativ zum Zeilenanfang).
struct Abschnitt {
    std::string text;
    Schrift schrift = Schrift::Normal;
    bool unterstrich = false;
    int start = 0;
};

/// Erwartete Punkte (x, y) einer Zeile aus Abschnitten; Ursprung = Takt 0, Nadel 1.  Leerzeichen drucken nichts (ausser im Unterstrich).
inline Punkte erwartetZeile(const std::vector<uint8_t>& rom, const std::vector<Abschnitt>& teile)
{
    Punkte p;
    for (const auto& a : teile) {
        int takt = a.start;
        for (char ch : a.text) {
            for (auto& q : treffer(rom, static_cast<uint8_t>(ch), a.schrift, a.unterstrich))
                p.insert({(takt + q.first) * DRUCKTAKT, (q.second - 1) * NADEL});
            takt += breite(a.schrift);
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

/// Vergleicht eine GANZE Zeile (alle Abschnitte, alle neun Nadeln) mit dem Band [ya, yb).  Ursprung (@p ox, @p oy: Takt 0 /
/// Nadel 1 in Pixeln) vorgegeben oder (Vorgabe) aus dem ersten Punkt gewonnen — der einzige freie Wert.
inline std::string vergleicheZeile(const Blattmodell& b, const std::vector<uint8_t>& rom, const std::vector<Abschnitt>& teile,
                                   int ya, int yb, int ox = INT32_MIN, int oy = 0)
{
    const Punkte exp = erwartetZeile(rom, teile);
    if (ox == INT32_MIN) {   // Ursprung aus dem ersten Punkt (Zeile beginnt links oben mit einem Treffer in beiden Mengen)
        const Punkte alle = gedruckt(b, ya, yb);
        if (exp.empty() || alle.empty()) return "leer";
        int ex = 1 << 30, ey = 1 << 30, ax = 1 << 30, ay = 1 << 30;
        for (auto& q : exp) { ex = std::min(ex, q.first); ey = std::min(ey, q.second); }
        for (auto& q : alle) { ax = std::min(ax, q.first); ay = std::min(ay, q.second); }
        ox = ax - ex; oy = ay - ey;
    }
    Punkte act;
    for (auto& q : gedruckt(b, ya, yb)) act.insert({q.first - ox, q.second - oy});
    std::string msg;
    int n = 0, nf = 0, nz = 0;
    for (auto& q : exp) if (!act.count(q)) { ++nf; if (n++ < 6) msg += " fehlt(" + std::to_string(q.first) + "," + std::to_string(q.second) + ")"; }
    n = 0;
    for (auto& q : act) if (!exp.count(q)) { ++nz; if (n++ < 6) msg += " zuviel(" + std::to_string(q.first) + "," + std::to_string(q.second) + ")"; }
    if (!msg.empty()) msg = "fehlend " + std::to_string(nf) + ", zuviel " + std::to_string(nz) + ":" + msg;
    return msg;
}

}  // namespace k6316test
