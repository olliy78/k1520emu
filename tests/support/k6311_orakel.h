/**
 * @file k6311_orakel.h
 * @brief Unabhaengiges Orakel fuer das Druckbild des K6311 (Entwurf 49, WP8f): erwartet wird, was der Zeichengenerator **im ROM 12128062**
 *        verlangt, nicht was der Emulator zeichnet.
 *
 * Herleitung aus dem ROM (Merkposten drucker.md Nr. 35): der Zeichengenerator liegt bei **2280H + 8 * (Code - 20H)**, 8 Byte je Zeichen (96 Zeichen,
 * 2280H-257FH).  Byte 0 ist die **Nadel-9-Maske** (Bit k = Datenspalte k + 1, 0 = Nadel 9 schlaegt an; FFH bei Zeichen ohne Unterlaenge - g j p q y , ;
 * tragen dort Nullbits), Byte 1...7 sind die 7 Datenspalten.  Das Format ist das des K6313 (dort 10 Byte, 9 Spalten): am Druckbild gemessen.  Die Datenbytes gehen unveraendert auf das Nadelregister DDH, in dem 0 = Nadel schlaegt an; Bit (8 - n) = Nadel n (n = 1...8).
 * Ein Zeichen belegt 12 Spalten (8 Zellspalten + 4 leer) = 1/10 Zoll (Pica); der Spaltenabstand ist 1/120 Zoll = 3 Pixel (Blattraster 360 dpi in x,
 * 144/Zoll in y, Nadelteilung 1/72 Zoll = 2 Einheiten). 
 *
 * Die Firmware verzoegert die geraden Nadeln um 4 Spalten gegen die ungeraden; das Orakel erwartet die Punkte der FERTIGEN Zeichen, also ohne diesen
 * Versatz - er gehoert ins Kopfmodell (`NadeldruckParameter::nadel_x`).  **Frei angepasst werden zwei Werte:** der Ursprung (links oben) und der
 * Spaltenabstand in Pixeln (der Wagenschritt in Pixeln ist nicht belegt; die Firmware taktet Schritte und Spalten getrennt, 12 Spalten = 9,04
 * Schritte); verglichen wird danach im **Spaltenindex**, Punkt fuer Punkt.
 */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/peripherals/druckwerk/blattmodell.h"

namespace k6311test {

constexpr int SPALTE   = 3;                 ///< Pixel je Spalte (1/120 Zoll), Nennwert
constexpr int ZEICHEN  = 10;                ///< Spalten je Zeichen (7 Zellspalten + 3 leer)
constexpr int NADEL    = 2;                 ///< Einheiten zwischen zwei Nadeln
constexpr int PUNKT_B  = 3;                 ///< Punktbreite in Pixeln
constexpr int GLYPHE0  = 0x2280;            ///< Zelle von 20H (Abzuege mit 10 KiB)
constexpr int GLYPHE0_8K = 0x1D00;          ///< dito in den 8-KiB-Abzuegen 13120012/13325212 (WP8g: Zelle 'A' bei 1E08H, ASCII-Zellen gleich, am ROM gemessen)

/// Lage der Zeichenzelle 20H: 8-KiB-Abzug (vier Chips) bei 1D00H, sonst 2280H.
inline int glyphe0(const std::vector<uint8_t>& rom) { return rom.size() <= 0x2000 ? GLYPHE0_8K : GLYPHE0; }

inline std::vector<uint8_t> rom(const std::string& dir, const std::string& name = "12128062")
{
    std::ifstream f(dir + "/" + name + ".bin", std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

using Treffer = std::set<std::pair<int, int>>;   ///< (Spalte 1...7, Nadel 1...9)

inline Treffer treffer(const std::vector<uint8_t>& rom, int code)
{
    Treffer t;
    const size_t a = static_cast<size_t>(glyphe0(rom) + 8 * (code - 0x20));
    for (int j = 1; j < 8; ++j)
        for (int n = 1; n <= 8; ++n)
            if (!((rom[a + static_cast<size_t>(j)] >> (8 - n)) & 1)) t.insert({j, n});
    for (int k = 0; k < 7; ++k)
        if (!((rom[a] >> k) & 1)) t.insert({k + 1, 9});
    return t;
}

using Punkte = std::set<std::pair<int, int>>;   ///< (Spaltenindex, Nadel)

/// Nationale Ersatztabelle (ROM 1855H-1863H): die 16 Zeichen # $ @ [ \ ] ^ ` { | } ~ ( ) < > werden ueber die Tabelle bei **2200H + (27F7H) + Index** auf
/// Zellen des Generators abgebildet (27F7H = 20H: ASCII, 30H: # = 7FH, @ = 82H, [ \ ] = 8BH 8DH 8CH, { | } ~ = 99H 9BH 9AH 9CH), alle anderen Zeichen
/// bleiben.  Das Byte 27F7H kommt aus dem ROM selbst (0074H: Kopie nach 383BH, 1855H: OR C).
inline int zelleVon(const std::vector<uint8_t>& rom, int code)
{
    if (rom.size() <= 0x2000) return code;   // 8-KiB-Abzug: die Ersatztabelle liegt woanders [?], nationale Zeichen werden hier nicht geprueft
    static const char NAT[] = "#$@[\\]^`{|}~()<>";
    for (int i = 0; i < 16; ++i)
        if (code == NAT[i]) return rom[static_cast<size_t>(0x2200 + rom[0x27F7] + i)];
    return code;
}

/// Erwartete Punkte eines Textes: Spaltenindex = 12 * Zeichenindex + Zellspalte.
inline Punkte erwartet(const std::vector<uint8_t>& rom, const std::string& text)
{
    Punkte p;
    for (size_t n = 0; n < text.size(); ++n) {
        const uint8_t c = static_cast<uint8_t>(text[n]);
        if (c < 0x20 || c > 0x7F) continue;
        for (auto& q : treffer(rom, zelleVon(rom, c))) p.insert({static_cast<int>(n) * ZEICHEN + q.first, q.second});
    }
    return p;
}

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

struct Ergebnis {
    std::string fehler;   ///< "" = gleich
    double ursprung = 0;
    int oy = 0;
};

/// Vergleicht den Text mit dem Band [ya, yb).  Frei angepasst wird nur der Ursprung (x in Pixeln, y der Nadel 1 in Einheiten); der Spaltenabstand ist
/// FEST 3 Pixel.  Danach muss jeder erwartete Punkt (Spaltenindex, Nadel) im Blatt gesetzt sein (Mittelpixel), und **kein** gesetztes Pixel darf ausserhalb
/// der erwarteten Punktzellen liegen (Zelle 3 x 2 Pixel, in x +-1 Pixel Rundung).  Benachbarte Punkte verschmelzen im Blatt; deshalb wird hier
/// geprueft statt Punkte aus dem Blatt zu "zaehlen".  Rueckgabe: Fehlerbeschreibung ("" = gleich).
inline Ergebnis vergleiche(const Blattmodell& b, const std::vector<uint8_t>& rom, const std::string& text, int ya, int yb)
{
    Ergebnis e;
    const Punkte exp = erwartet(rom, text);
    if (exp.empty()) { e.fehler = "kein erwarteter Punkt"; return e; }
    int ax = 1 << 30, ay = 1 << 30, ek = 1 << 30, nmin = 99;
    for (int y = std::max(ya, 0); y < yb && y < b.hoehe(); ++y)
        for (int x = 0; x < b.breite(); ++x)
            if (b.punkt(x, y)) { ax = std::min(ax, x); ay = std::min(ay, y); }
    if (ax == (1 << 30)) { e.fehler = "leer"; return e; }
    for (auto& q : exp) { ek = std::min(ek, q.first); nmin = std::min(nmin, q.second); }
    size_t beste_fehler = SIZE_MAX;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int ox = ax - ek * SPALTE - 3; ox <= ax - ek * SPALTE + 3; ++ox) {
            const int y1 = ay + dy - (nmin - 1) * NADEL;   // y der Nadel 1
            std::set<std::pair<int, int>> zelle;            // (x, y) der abgedeckten Pixel
            int nf = 0;
            std::string msg;
            for (auto& q : exp) {
                const int x0 = ox + q.first * SPALTE, y0 = y1 + (q.second - 1) * NADEL;
                if (!b.punkt(x0 + 1, y0)) { if (nf++ < 6) msg += " fehlt(" + std::to_string(q.first) + "," + std::to_string(q.second) + ")"; }
                for (int yy = 0; yy < NADEL; ++yy) for (int xx = -1; xx <= PUNKT_B; ++xx) zelle.insert({x0 + xx, y0 + yy});
            }
            int nz = 0;
            for (int y = std::max(ya, 0); y < yb && y < b.hoehe(); ++y)
                for (int x = 0; x < b.breite(); ++x)
                    if (b.punkt(x, y) && !zelle.count({x, y})) { if (nz++ < 3) msg += " zuviel(x" + std::to_string(x) + ",y" + std::to_string(y) + ")"; }
            const size_t f = static_cast<size_t>(nf) * 6 + static_cast<size_t>(nz);
            if (f < beste_fehler) {
                beste_fehler = f;
                e.ursprung = ox;
                e.oy = y1;
                e.fehler = f ? "fehlend " + std::to_string(nf) + ", zuviel " + std::to_string(nz) + " Pixel:" + msg : std::string();
            }
        }
    }
    return e;
}

/// Liest ein Band in Text zurueck: je 10-Spalten-Zelle ab @p ox das Zeichen 20H...7EH, dessen Punkte die Zelle GENAU fuellen; '?' =
/// kein Treffer.  Zellen ohne Punkte = ' '.  Nur Diagnose fuer Fehlertexte und das Selbsttest-Orakel.
inline std::string lesen(const Blattmodell& b, const std::vector<uint8_t>& rom, int ya, int yb, int ox, int oy, int zellen)
{
    std::string s;
    for (int n = 0; n < zellen; ++n) {
        Treffer z;
        for (int k = 0; k < 8; ++k)
            for (int nd = 1; nd <= 9; ++nd) {
                const int x0 = ox + (n * ZEICHEN + k) * SPALTE, y0 = oy + (nd - 1) * NADEL;
                if (y0 >= ya - 8 && y0 < yb + 8 && b.punkt(x0 + 1, y0)) z.insert({k, nd});
            }
        char ch = z.empty() ? ' ' : '?';
        for (int c = 0x21; c < 0x7F && ch == '?'; ++c) {
            if (treffer(rom, c) == z) ch = static_cast<char>(c);
        }
        s += ch;
    }
    return s;
}

}  // namespace k6311test
