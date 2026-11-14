/**
 * @file k6313_orakel_esc.h
 * @brief Orakel der Schriftarten des K6313 (Entwurf 49 WP8h-a, Merkposten drucker.md Nr. 37): erwartet wird, was die Zeichenaufbereitung der Firmware aus
 *        dem Zeichengenerator im ROM macht, nicht was der Emulator zeichnet.  Aufbauend auf `k6313_orakel.h` (Zelle, Treffer, Raster).
 *
 * **Datenweg (am ROM gelesen, 33538013 `1934H-1A73H`, 30835007 `13A0H-1430H`):** die 9 Datenspalten g[0..8] einer Zelle stehen unveraendert in Puffer A (g[9] = FFH),
 * Puffer B traegt Nadel 9 (Bit 0 jeder Spalte, aus der Maske Byte 0 der Zelle: Spalte k < 8 = Bit k, Spalte 8 = FFH).  Im Datenbyte heisst 0 = Nadel schlaegt an.
 *   * **Fett** (Attributbit 5 oder 3, `sub_1A52`/`sub_140F`): Spalte i := g[i] AND g[i-1] (i = 1...9, mit den ORIGINALwerten der Vorspalte) - jeder Punkt wird in
 *     die naechste Spalte verbreitert; auch fuer Puffer B (ausser Attributbit 1).
 *   * **Breit** (ESC W 1 / ESC SO / ISO `ESC [ 1 m`): dieselbe Verbreiterung, die Spalten stehen aber im Abstand von **zwei** Spalten; Zeichenabstand 24 Spalten.
 *   * **Elite** (1/12 Zoll, ESC M / ISO `ESC [ 1 SP K`): Zeichenabstand **10** Spalten; die 9 Datenspalten werden zu 6 zusammengefasst (0,1)(2)(3,4)(5)(6,7)(8) mit AND.
 *   * **komprimiert** (1/17 Zoll, ESC SI / ISO `ESC [ 4 SP K`): Zeichenabstand **7** Spalten; zu 5 Spalten (0,1)(2,3)(4,5)(6,7)(8).
 *   * **Unterstreichen** (ESC - 1 / ISO `ESC [ 4 m`): Nadel 9 auf den geraden Spalten 0, 2, ... der Zelle.
 * Die Zusammenfassungen von Elite/komprimiert sind **an den Druckbildern aller 94 Zeichen gemessen** (eine Regel fuer alle), nicht Befehl fuer Befehl im ROM
 * nachverfolgt `[E]`; Spaltenabstand 1/120 Zoll = 4 Pixel (`k6313test::SPALTE`).
 */
#pragma once
#include <array>
#include <set>
#include <string>
#include <vector>

#include "tests/support/k6313_orakel.h"

namespace k6313test {

enum class Modus { Normal, Fett, Breit, Elite, Komprimiert };

struct Schrift {
    Modus modus = Modus::Normal;
    bool unterstrich = false;
};

/// Zeitraster der Spalten (in Drittelpixeln, 480 dpi; 1/3 Pixel = 1/1440 Zoll) - am Druckbild gemessen und gegen die Zaehlerwerte der Firmware plausibel [E]:
/// Zeichenabstand `zeichen3`, Spaltenabstand `spalte3`.  Pica 12 Spalten zu 4 Pixeln (1/120 Zoll); Elite 60 Einheiten zu 2/3 Pixel = 40 Pixel (1/12 Zoll),
/// die 9 Spalten stehen im Abstand 8/3 Pixel; komprimiert 28 Pixel (1/17 Zoll, 7,06 Spalten), Spaltenabstand 2 Pixel; Breit doppelter Pica.
/// Bei Elite und komprimiert sind die Spalten dichter als die Punktbreite (4 Pixel): die Punkte ueberlappen, die Firmware DRUCKT jede der 9 Datenspalten einzeln
/// (kein Zusammenfassen von Spalten) - die fruehere Annahme "(0,1)(2)(3,4)..." war ein Artefakt der 4-Pixel-Rasterung der Auswertung.
struct Raster { int zeichen3; int spalte3; };
inline Raster raster(Modus m)
{
    switch (m) {
        case Modus::Breit: return {288, 24};
        case Modus::Elite: return {120, 8};
        case Modus::Komprimiert: return {84, 6};
        default: return {144, 12};
    }
}

/// Rohe Spalten (0 = Nadel an) von Puffer A (g[0..9], g[9] = FFH) und Puffer B (Nadel 9 in Bit 0, b[0..9]).
inline void spalten(const std::vector<uint8_t>& rom, int code, std::array<uint8_t, 10>& g, std::array<uint8_t, 10>& b)
{
    const Zellen zl = zellen(rom);
    const size_t a = static_cast<size_t>(zl.basis + zl.schritt * (code - 0x20));
    g[9] = 0xFF;
    b[8] = b[9] = 0xFF;
    for (int j = 0; j < 9; ++j) g[static_cast<size_t>(j)] = rom[a + 1 + static_cast<size_t>(j)];
    for (int k = 0; k < 8; ++k) b[static_cast<size_t>(k)] = static_cast<uint8_t>(0xFE | ((rom[a] >> k) & 1));
}

/// Verbreiterung: Spalte i AND Vorspalte (Originalwerte), i = 1...9.
inline std::array<uint8_t, 10> verbreitert(const std::array<uint8_t, 10>& v)
{
    std::array<uint8_t, 10> r = v;
    for (size_t i = 1; i < 10; ++i) r[i] = static_cast<uint8_t>(v[i] & v[i - 1]);
    return r;
}

/// Punkte (Spaltenindex 0...9, Nadel 1...9) eines Zeichens in der Schriftart @p s.
inline Treffer treffer(const std::vector<uint8_t>& rom, int code, const Schrift& s)
{
    std::array<uint8_t, 10> g, b;
    spalten(rom, code, g, b);
    if (s.modus == Modus::Fett || s.modus == Modus::Breit) { g = verbreitert(g); b = verbreitert(b); }
    if (s.unterstrich) for (auto& x : b) x = 0xFF;   // eigene Nadel 9 entfaellt, nur die Striche (unten)
    Treffer t;
    for (int j = 0; j < 10; ++j) {
        for (int n = 1; n <= 8; ++n) if (!((g[static_cast<size_t>(j)] >> (8 - n)) & 1)) t.insert({j, n});
        if (!(b[static_cast<size_t>(j)] & 1)) t.insert({j, 9});
    }
    return t;
}

/// Erwartete Punkte: linke Kante in Drittelpixeln und Nadel 1...9, Ursprung = Spalte 0 des ersten Zeichens.
using Punkte3 = std::set<std::pair<int, int>>;
inline Punkte3 erwartet3(const std::vector<uint8_t>& rom, const std::string& text, const Schrift& s)
{
    const Raster r = raster(s.modus);
    Punkte3 p;
    for (size_t n = 0; n < text.size(); ++n) {
        for (auto& q : treffer(rom, static_cast<uint8_t>(text[n]), s))
            p.insert({static_cast<int>(n) * r.zeichen3 + q.first * r.spalte3, q.second});
        if (s.unterstrich)   // Nadel 9 auf den geraden Spalten der Zelle (Zelle = Zeichenabstand / Spaltenabstand Spalten)
            for (int c = 0; c < r.zeichen3 / r.spalte3; c += 2) p.insert({static_cast<int>(n) * r.zeichen3 + c * r.spalte3, 9});
    }
    return p;
}

/// Vergleich mit Hülle: die Lage eines Punkts ist auf ein Pixel genau (das Kopfmodell interpoliert zwischen den Motorschritten); ein erwarteter Punkt
/// belegt die Pixel [X+1, X+3) sicher und hoechstens [X-1, X+5) (Punktbreite 4).  Je Nadelzeile: sichere Pixel muessen gedruckt sein, Gedrucktes muss in der
/// Huelle liegen.  Senkrecht gilt der Nadelabstand 3 exakt (Ursprung aus dem ersten Pixel).  Ursprung in x frei (+-6 Pixel um den Fund).
/// Rueckgabe "" = gleich; @p ursprung_x liefert den besten Ursprung.
inline std::string vergleicheHuelle(const Blattmodell& b, const Punkte3& exp, int ya, int yb, int* ursprung_x = nullptr)
{
    if (exp.empty()) return "leer";
    int nmin = 99, nmax = 0, ex3 = 1 << 30;
    for (auto& q : exp) { nmin = std::min(nmin, q.second); nmax = std::max(nmax, q.second); ex3 = std::min(ex3, q.first); }
    const int oy = ya - NADEL * (nmin - 1);
    if (yb - 1 != oy + NADEL * (nmax - 1) + 1) return "Hoehe: Band " + std::to_string(ya) + ".." + std::to_string(yb) + ", erwartet Nadeln " + std::to_string(nmin) + ".." + std::to_string(nmax);
    // gedruckte Pixel je Nadelzeile (obere Pixelzeile des Punkts)
    int ax = 1 << 30;
    std::vector<std::set<int>> akt(10);
    for (int n = nmin; n <= nmax; ++n)
        for (int x = 0; x < b.breite(); ++x)
            if (b.punkt(x, oy + NADEL * (n - 1))) { akt[static_cast<size_t>(n)].insert(x); ax = std::min(ax, x); }
    if (ax == (1 << 30)) return "leer";
    std::string beste;
    size_t beste_fehler = SIZE_MAX;
    int beste_ox = 0;
    for (int ox = ax - ex3 / 3 - 6; ox <= ax - ex3 / 3 + 6; ++ox) {
        std::vector<std::set<int>> sicher(10), huelle(10);
        for (auto& q : exp) {
            const int X = q.first / 3 + ox;   // Teilung abgerundet; der Rest (0...2 Drittelpixel) liegt in der Toleranz
            for (int x = X + 1; x < X + 3; ++x) sicher[static_cast<size_t>(q.second)].insert(x);
            for (int x = X - 1; x < X + 5; ++x) huelle[static_cast<size_t>(q.second)].insert(x);
        }
        size_t fehler = 0;
        std::string msg;
        for (int n = nmin; n <= nmax; ++n) {
            int k = 0;
            for (int x : sicher[static_cast<size_t>(n)]) if (!akt[static_cast<size_t>(n)].count(x)) { ++fehler; if (k++ < 3) msg += " fehlt(x" + std::to_string(x - ox) + ",n" + std::to_string(n) + ")"; }
            k = 0;
            for (int x : akt[static_cast<size_t>(n)]) if (!huelle[static_cast<size_t>(n)].count(x)) { ++fehler; if (k++ < 3) msg += " zuviel(x" + std::to_string(x - ox) + ",n" + std::to_string(n) + ")"; }
        }
        if (fehler < beste_fehler) { beste_fehler = fehler; beste = fehler ? std::to_string(fehler) + " Pixel:" + msg : ""; beste_ox = ox; }
    }
    if (ursprung_x) *ursprung_x = beste_ox;
    return beste;
}

}  // namespace k6313test
