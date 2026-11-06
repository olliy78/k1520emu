/**
 * @file schrittwagen.cpp
 * @brief Druckwagen des K6316 — Phasenmuster zu Position und Taktscheibe.
 * @see schrittwagen.h
 */

#include "core/peripherals/druckwerk/schrittwagen.h"

void SchrittWagen::netzEin(int position)
{
    pos_ = position;
    richtung_ = +1;
    blockiert_ = false;
    n_ = 0;
    e_ = WagenEingang{};
    t_ = t_schritt_ = 0;
    letzter_ = 0;
    motor_.netzEin();
}

void SchrittWagen::eingang(const WagenEingang& e)
{
    e_ = e;
    int d = motor_.phasen(e.phasen);
    if (!d) return;
    if (p_.umgekehrt) d = -d;
    ++n_;
    richtung_ = d > 0 ? +1 : -1;
    int neu = pos_ + d;
    if (neu < p_.pos_min) { neu = p_.pos_min; blockiert_ = true; }
    else if (neu > p_.pos_max) { neu = p_.pos_max; blockiert_ = true; }
    else blockiert_ = false;
    if (neu != pos_) {
        const uint64_t ab = t_ - t_schritt_;
        if (ab >= 300 && ab <= 60000) abstand_ = ab;   // Schrittabstand der Fahrt (sonst: der alte bleibt)
        letzter_ = neu - pos_;
        t_schritt_ = t_;
    }
    pos_ = neu;
}

int SchrittWagen::positionPixel(int x_je_takt) const
{
    // Lage = befohlene Lage minus noch nicht gefahrener Rest des letzten Schritts
    const uint64_t dt = t_ - t_schritt_;
    const double rest = dt >= abstand_ ? 0.0 : (1.0 - static_cast<double>(dt) / static_cast<double>(abstand_));
    const double lage = pos_ - letzter_ * rest;
    const double px = lage * x_je_takt;
    return static_cast<int>(px < 0 ? px - 0.5 : px + 0.5);
}

WagenRueckmeldung SchrittWagen::rueckmeldung() const
{
    WagenRueckmeldung r;
    // Taktscheibe: wechselt je Vollschritt (zwei Halbschritte); gerade Lagen sind die Vollschritte.
    r.g1 = (((pos_ + 1) >> 1) & 1) != 0;
    return r;
}
