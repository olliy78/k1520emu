/**
 * @file phasenmotor.h
 * @brief Vierphasiger Schrittmotor, von der Firmware selbst per Phasenmuster geschaltet (Entwurf 49 §4.3, K6316).
 *
 * Gemeinsam fuer Druckwagen (6-Grad-Motor, 60 Schritte je Umdrehung), Papiermotor und Vorsteckmotor des K6316:
 * die Firmware legt Phasenmuster an, der Rotor folgt dem Feld.  Ideal, bis die Firmware Traegheit verlangt:
 * der Rotor steht sofort im neuen Feld.
 *
 * **Feldwinkel in Halbschritten** (je Phase 90 Grad elektrisch = 2 Halbschritte): eine Phase k -> 2k, zwei
 * benachbarte Phasen (k, k+1) -> 2k+1, drei Phasen -> die mittlere.  Alles andere (keine, vier, gegenueber-
 * liegende) erzeugt kein eindeutiges Feld: der Rotor bleibt.  Das Weiterschalten um 1 Halbschritt im Feld ist
 * ein Halbschritt; die Firmware des K6316 schaltet Vollschritte (zwei Phasen an, Muster um eine Phase weiter).
 * Die Zaehlrichtung steigender Phasennummer ist `+`.
 */

#pragma once
#include <cstdint>

class PhasenMotor {
public:
    /// Neues Phasenmuster (Bit k = Phase k+1 bestromt).  @return Aenderung der Rotorlage in Halbschritten.
    int phasen(uint8_t aktiv)
    {
        aktiv &= 0x0F;
        const int a = feldwinkel(aktiv);
        if (a < 0) return 0;
        if (feld_ < 0) { feld_ = a; return 0; }
        int d = (a - feld_ + 16) % 8;
        if (d == 4) return 0;   // gegenueberliegendes Feld: Drehrichtung unbestimmt
        if (d > 4) d -= 8;
        feld_ = a;
        lage_ += d;
        return d;
    }

    int  lage() const { return lage_; }           ///< Rotorlage in Halbschritten seit dem Netz-Ein
    void setzeLage(int h) { lage_ = h; }          ///< Anschlaege: die Mechanik haelt den Rotor an
    void netzEin() { feld_ = -1; lage_ = 0; }

private:
    /// Feldwinkel mod 8 zum Muster, -1 = kein eindeutiges Feld.
    static int feldwinkel(uint8_t m)
    {
        // Index = Muster (Bit k = Phase k+1): eine Phase 2k, zwei benachbarte 2k+1, drei = mittlere (fehlt m: k = m+2)
        static const int8_t TAB[16] = {-1, 0, 2, 1, 4, -1, 3, 2, 6, 7, -1, 0, 5, 6, 4, -1};
        return TAB[m & 0x0F];
    }

    int feld_ = -1;   ///< Feldwinkel mod 8, -1 = noch kein gueltiges Muster
    int lage_ = 0;
};
