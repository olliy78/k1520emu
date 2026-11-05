/**
 * @file papierweg.cpp
 * @brief Papierweg — Schrittgeber.
 * @see papierweg.h
 */

#include "core/peripherals/druckwerk/papierweg.h"

void Papierweg::netzEin()
{
    frei_ = false;
    vor_ = true;
    schnell_ = false;
    akku_ = puls_ = lage_ = 0;
    n_vor_ = n_rueck_ = 0;
}

void Papierweg::halt()
{
    frei_ = false;
    akku_ = 0;
}

void Papierweg::ausgaenge(bool freigabe, bool vorwaerts, bool schnell)
{
    if (freigabe && !frei_) akku_ = 0;   // Start: der erste Impuls kommt nach einem vollen Abstand
    if (!freigabe) akku_ = 0;
    frei_ = freigabe;
    vor_ = vorwaerts;
    schnell_ = schnell;
}

bool Papierweg::takt(int ticks)
{
    const bool vorher = puls_ > 0;
    if (puls_ > 0) puls_ -= ticks;
    if (frei_) {
        akku_ += ticks;
        const int abstand = schnell_ ? p_.abstand_schnell : p_.abstand_anlauf;
        if (akku_ >= abstand) {
            akku_ -= abstand;
            puls_ = p_.impuls;
            if (vor_) { ++lage_; ++n_vor_; }
            else      { --lage_; ++n_rueck_; }
        }
    }
    return (puls_ > 0) != vorher;
}
