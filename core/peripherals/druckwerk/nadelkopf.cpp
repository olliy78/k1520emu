/**
 * @file nadelkopf.cpp
 * @brief Nadelkopf — Muster zu Punkten.
 * @see nadelkopf.h
 */

#include "core/peripherals/druckwerk/nadelkopf.h"

int Nadelkopf::feuere(uint16_t muster, int x, int y_papier, int richtung, bool anliegend, Blattmodell& blatt)
{
    muster &= 0x1FF;
    if (!muster) return 0;
    ++anschlaege_;
    int n = 0;
    // Landeort = Ausloeseort + Flugzeit in Fahrtrichtung; der Punkt ist um seine Mitte zentriert
    const int x0 = x * p_.x_je_takt + (richtung < 0 ? p_.versatz_rueck : p_.versatz_vor) - p_.punkt_breite / 2;
    for (int k = 0; k < NADELN; ++k) {
        if (!(muster & (1u << k))) continue;
        if (!anliegend) {
            ++abgewiesen_;
            continue;
        }
        const int y0 = y_papier + p_.nadel1_y + k * p_.nadel_teilung_y;
        for (int dy = 0; dy < p_.punkt_hoehe; ++dy)
            for (int dx = 0; dx < p_.punkt_breite; ++dx)
                if (blatt.setze(x0 + dx, y0 + dy)) ++n;
    }
    gesetzt_ += static_cast<uint64_t>(n);
    return n;
}
