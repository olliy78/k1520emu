/**
 * @file k6313_orakel.h
 * @brief Unabhaengiges Orakel fuer das Druckbild des K6313 (Entwurf 49, WP8d): erwartet wird, was der Zeichengenerator
 *        **im ROM** verlangt, nicht was der Emulator zeichnet.
 *
 * Herleitung aus der Firmware 33538013 (Zeichenausgabe `sub_1934`, 1934H-1998H): die Zelle eines Zeichens liegt bei
 * **2300H + 10 * (Code - 20H)**, 10 Bytes: Byte 0 = **Nadel-9-Maske** (Bit k = Spalte k, 8 Spalten), Byte 1...9 = die 9 Datenspalten.
 * Die Datenbytes gehen unveraendert auf das Nadelregister DDH, in dem 0 = Nadel schlaegt an; Bit (8 - n) = Nadel n (n = 1...8).
 * Nadel 9 (PP9 = F9H Bit 0, ebenfalls 0 = an) schlaegt in Spalte k an, wenn Bit k der Maske 0 ist (Rotation der Maske beim
 * Fuellen des zweiten Ringpuffers, 1988H-1998H).  Ein Zeichen belegt 12 Spalten (9 Daten + 3 leer) = 1/10 Zoll (Pica, 10 Z/Z); der
 * Spaltenabstand ist 1/120 Zoll = 4 Pixel (Blattraster 480 dpi in x, 216 dpi in y, Nadelteilung 1/72 Zoll = 3 Einheiten).
 *
 * Die Firmware verzoegert die geraden Nadeln um 4 Spalten gegen die ungeraden (zwei Nadelspalten im Kopf); das Orakel erwartet die
 * Punkte der FERTIGEN Zeichen, also ohne diesen Versatz — er gehoert ins Kopfmodell (`NadeldruckParameter::nadel_x`).  Der Ursprung
 * (links oben) ist der einzige frei gefittete Wert.
 *
 * Selbsttest (Taste LINE FEED vor bzw. zurueck beim Netz-Ein): Zeilen zu je 80 Zeichen aus dem Umlauf 21H...7EH; die Firmware druckt
 * abwechselnd 80 Zeichen vorwaerts (21H...70H) und den Rest (71H...7EH) rueckwaerts.  Die Kennungszeile steht nur beim Test mit LF vor.
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

namespace k6313test {

constexpr int SPALTE   = 4;                 ///< Pixel je Spalte (1/120 Zoll)
constexpr int ZEICHEN  = 12 * SPALTE;       ///< Pixel je Zeichen (Pica)
constexpr int NADEL    = 3;                 ///< Einheiten zwischen zwei Nadeln
constexpr int PUNKT_B  = 4;                 ///< Punktbreite in Pixeln
constexpr int GLYPHE0  = 0x2300;            ///< Zelle von 20H

/// Lage der Zellen im Abzug: Basis = Zelle von 20H, Schritt = Bytes je Zelle.
struct Zellen { int basis; int schritt; };

/// Gesucht wird die Zelle von 'A' (41H) des Referenzabzugs; daraus folgt die Basis bei Schritt 10 (33538013 und Verwandte: 2300H; 33830803: 2134H, WP8g)
/// oder Schritt 12 (16-KiB-Abzug 30835007, WP8h: Zelle = Maske + 9 Spalten + 2 Fuellbytes, Basis 2E10H = 2C90H + 12 * 20H, am ROM 13CBH-13E4H gelesen).
/// Gilt der Schritt 10, wenn die Zelle von 20H (Leerzeichen) lauter FF ist, sonst 12.  Nicht gefunden = Vorgabe 2300H/10 (der Vergleich schlaegt dann fehl,
/// statt etwas Falsches zu belegen).
inline Zellen zellen(const std::vector<uint8_t>& rom)
{
    static const uint8_t A[10] = {0xFF, 0xE1, 0xDF, 0xB7, 0x7F, 0xF7, 0x7F, 0xB7, 0xDF, 0xE1};
    static thread_local const uint8_t* zp = nullptr;   // letzter Fund (Zeiger + Lage), billig nachgeprueft
    static thread_local Zellen z{GLYPHE0, 10};
    const auto leer = [&](int base, int n) {
        for (int i = 0; i < n; ++i) if (static_cast<size_t>(base + i) >= rom.size() || rom[static_cast<size_t>(base + i)] != 0xFF) return false;
        return true;
    };
    if (zp == rom.data() && zp) {   // Treffer nur gueltig, solange die Zelle von 'A' noch dort liegt (ein anderer Abzug kann dieselbe Adresse bekommen)
        const size_t i = static_cast<size_t>(z.basis + z.schritt * 0x21);
        if (i + 10 <= rom.size() && std::equal(A, A + 10, rom.begin() + static_cast<std::ptrdiff_t>(i)) && leer(z.basis, z.schritt)) return z;
    }
    for (size_t i = 12u * 0x21u; i + 10 <= rom.size(); ++i) {
        if (!std::equal(A, A + 10, rom.begin() + static_cast<std::ptrdiff_t>(i))) continue;
        const int b10 = static_cast<int>(i) - 10 * 0x21;
        const int b12 = static_cast<int>(i) - 12 * 0x21;
        if (b10 >= 0 && leer(b10, 10)) z = {b10, 10};
        else if (b12 >= 0 && leer(b12, 12)) z = {b12, 12};
        else continue;
        zp = rom.data();
        return z;
    }
    return {GLYPHE0, 10};
}
inline int glyphe0(const std::vector<uint8_t>& rom) { return zellen(rom).basis; }

/// ROM-Abzug (10 KiB, 0000H-27FFH) aus `<dir>/33538013.bin`.
inline std::vector<uint8_t> rom(const std::string& dir, const std::string& name = "33538013")
{
    std::ifstream f(dir + "/" + name + ".bin", std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

using Treffer = std::set<std::pair<int, int>>;   ///< (Spalte 0...8, Nadel 1...9)

/// Treffer der Zelle von Zeichen @p code (20H...7EH).
inline Treffer treffer(const std::vector<uint8_t>& rom, int code)
{
    Treffer t;
    const Zellen zl = zellen(rom);
    const size_t a = static_cast<size_t>(zl.basis + zl.schritt * (code - 0x20));
    for (int j = 0; j < 9; ++j)
        for (int n = 1; n <= 8; ++n)
            if (!((rom[a + 1 + static_cast<size_t>(j)] >> (8 - n)) & 1)) t.insert({j, n});
    for (int k = 0; k < 8; ++k)
        if (!((rom[a] >> k) & 1)) t.insert({k, 9});
    return t;
}

using Punkte = std::set<std::pair<int, int>>;   ///< (x, y) der linken oberen Ecke eines Punkts

/// Erwartete Punkte eines Textes (Pica) relativ zum Ursprung (Spalte 0 des ersten Zeichens, Nadel 1).
inline Punkte erwartet(const std::vector<uint8_t>& rom, const std::string& text)
{
    Punkte p;
    for (size_t n = 0; n < text.size(); ++n)
        for (auto& q : treffer(rom, static_cast<uint8_t>(text[n])))
            p.insert({static_cast<int>(n) * ZEICHEN + q.first * SPALTE, (q.second - 1) * NADEL});
    return p;
}

/// Alle Punkte des Blatts mit y in [ya, yb): ein Punkt ist 4 Pixel breit; benachbarte Spalten derselben Nadel verschmelzen zu einem
/// Streifen und werden wieder in 4er-Stuecke geteilt.
inline Punkte gedruckt(const Blattmodell& b, int ya, int yb)
{
    Punkte p;
    for (int y = std::max(ya, 0); y < yb && y < b.hoehe(); ++y) {
        for (int x = 0; x < b.breite();) {
            if (b.punkt(x, y) && !(y > 0 && b.punkt(x, y - 1))) {
                const int s = x;
                while (x < b.breite() && b.punkt(x, y)) ++x;
                const int n = std::max(1, (x - s + PUNKT_B / 2) / PUNKT_B);
                for (int k = 0; k < n; ++k) p.insert({s + k * PUNKT_B, y});
            } else ++x;
        }
    }
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

/// Vergleicht den Text mit dem Band [ya, yb): der Ursprung wird gesucht (einziger freier Wert; Start = erster Punkt, +-3 Pixel), die
/// Punkte werden auf Spalten zu 4 Pixeln gerundet — die Lage zwischen den Motorschritten ist auf einen Pixel genau (Kopfmodell
/// interpoliert, 3 Pixel je Halbschritt), nicht auf eine Spalte.  Danach muss die Punktmenge **gleich** sein.
/// Rueckgabe "" = gleich, sonst Beschreibung; @p ursprung_x liefert den besten Ursprung in x (Pixel).
inline std::string vergleiche(const Blattmodell& b, const std::vector<uint8_t>& rom, const std::string& text, int ya, int yb,
                              int* ursprung_x = nullptr)
{
    const Punkte exp = erwartet(rom, text);
    const Punkte alle = gedruckt(b, ya, yb);
    if (exp.empty() || alle.empty()) return "leer";
    int ex = 1 << 30, ey = 1 << 30, ax = 1 << 30, ay = 1 << 30;
    for (auto& q : exp) { ex = std::min(ex, q.first); ey = std::min(ey, q.second); }
    for (auto& q : alle) { ax = std::min(ax, q.first); ay = std::min(ay, q.second); }
    const int oy = ay - ey;
    std::string beste;
    size_t beste_fehler = SIZE_MAX;
    int beste_ox = 0;
    for (int ox = ax - ex - 3; ox <= ax - ex + 3; ++ox) {
        Punkte act;
        for (auto& q : alle) {
            const int d = q.first - ox;
            act.insert({(d >= 0 ? (d + SPALTE / 2) / SPALTE : -((-d + SPALTE / 2) / SPALTE)) * SPALTE, q.second - oy});
        }
        std::string msg;
        int n = 0, nf = 0, nz = 0;
        for (auto& q : exp) if (!act.count(q)) { ++nf; if (n++ < 6) msg += " fehlt(" + std::to_string(q.first) + "," + std::to_string(q.second) + ")"; }
        n = 0;
        for (auto& q : act) if (!exp.count(q)) { ++nz; if (n++ < 6) msg += " zuviel(" + std::to_string(q.first) + "," + std::to_string(q.second) + ")"; }
        if (!msg.empty()) msg = "fehlend " + std::to_string(nf) + ", zuviel " + std::to_string(nz) + ":" + msg;
        if (static_cast<size_t>(nf + nz) < beste_fehler) { beste_fehler = static_cast<size_t>(nf + nz); beste = msg; beste_ox = ox; }
    }
    if (ursprung_x) *ursprung_x = beste_ox;
    return beste;
}

/// Liest ein Band in Text zurueck (je 48-Pixel-Zelle der Code 20H...7EH, dessen Treffer GENAU passen, sonst '?'); der Ursprung wird
/// bei @p ox_fest >= 0 vorgegeben.  Nur Diagnose fuer Fehlertexte.
inline std::string lesen(const Blattmodell& b, const std::vector<uint8_t>& rom, int ya, int yb, int ox)
{
    const Punkte alle = gedruckt(b, ya, yb);
    if (alle.empty()) return "";
    int ay = 1 << 30, bx = 0;
    for (auto& q : alle) { ay = std::min(ay, q.second); bx = std::max(bx, q.first); }
    std::string s;
    for (int x0 = ox; x0 <= bx; x0 += ZEICHEN) {
        Treffer z;
        for (auto& q : alle)
            if (q.first >= x0 && q.first < x0 + ZEICHEN) z.insert({(q.first - x0) / SPALTE, (q.second - ay) / NADEL + 1});
        char ch = z.empty() ? ' ' : '?';
        for (int c = 0x20; c < 0x7F && ch == '?'; ++c) if (treffer(rom, c) == z) ch = static_cast<char>(c);
        s += ch;
    }
    return s;
}

}  // namespace k6313test
