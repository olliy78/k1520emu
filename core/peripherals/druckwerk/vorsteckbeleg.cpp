/**
 * @file vorsteckbeleg.cpp
 * @brief Vorsteckbeleg des K6316 — Lage, Lichtschranken, eigenes Blatt.
 * @see vorsteckbeleg.h, doc/printer/k6316/hardware.md §11
 */

#include "core/peripherals/druckwerk/vorsteckbeleg.h"
#include <algorithm>

bool Vorsteckbeleg::einlegen(int laenge, int breite_px)
{
    if (da_ || laenge < 1 || breite_px < 1) return false;
    if (blatt_) basis_ += blatt_->version() + 1;   // die Version waechst auch ueber den Blattwechsel hinweg
    blatt_ = std::make_unique<Blattmodell>(breite_px);
    blatt_->streckeBis(laenge * p_.zeilen_je_schritt);
    laenge_ = laenge;
    breite_px_ = breite_px;
    u_ = p_.einlege_lage;
    da_ = true;
    return true;
}

void Vorsteckbeleg::bewege(int schritte)
{
    if (!da_) return;
    u_ = std::min(u_ + schritte, p_.u_max);      // Einzugsende: der Motor laeuft leer
    if (u_ < p_.u_hinaus) da_ = false;           // Auswurf zu Ende: der Beleg ist ganz aus dem Schacht
}
