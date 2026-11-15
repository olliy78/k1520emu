/**
 * @file geregelter_wagen.cpp
 * @brief Geregelter Wagen des SD 1157.
 * @see geregelter_wagen.h
 */

#include "core/peripherals/druckwerk/geregelter_wagen.h"

void GeregelterWagen::halt()
{
    e_ = WagenEingang{};
    laeuft_ = false;
    bremsrest_ = -1;
    akku_ = 0;
    puls_vor_ = puls_rueck_ = 0;
}

void GeregelterWagen::netzEin(int position)
{
    halt();
    pos_ = position;
    richtung_ = +1;
}

void GeregelterWagen::eingang(const WagenEingang& e)
{
    if (e.lfg && !e_.lfg) {            // LFG 0 -> 1: LAUF binnen Mikrosekunden
        laeuft_ = true;
        bremsrest_ = -1;
        akku_ = 0;
    } else if (!e.lfg && e_.lfg && laeuft_) {
        bremsrest_ = e.schnell ? p_.bremsweg_tab : p_.bremsweg_druck;   // Auslauf, Richtung bleibt
        if (bremsrest_ <= 0) laeuft_ = false;
    }
    if (e.lfg) richtung_ = e.vorwaerts ? +1 : -1;   // nur unter LFG folgt die Richtung VW (§10.1)
    e_ = e;
}

bool GeregelterWagen::takt(int t)
{
    const WagenRueckmeldung vorher = rueckmeldung();
    if (puls_vor_ > 0)   puls_vor_ -= t;
    if (puls_rueck_ > 0) puls_rueck_ -= t;
    if (laeuft_) {
        akku_ += t;
        const int abstand = e_.schnell ? p_.abstand_tab : p_.abstand_druck;
        while (laeuft_ && akku_ >= abstand) {
            akku_ -= abstand;
            const int neu = pos_ + richtung_;
            if (neu < p_.pos_min || neu > p_.pos_max) {   // weicher Anschlag: bleibt stehen
                laeuft_ = false;
                break;
            }
            pos_ = neu;
            if (richtung_ > 0) puls_vor_ = p_.takt_impuls; else puls_rueck_ = p_.takt_impuls;
            if (bremsrest_ > 0 && --bremsrest_ == 0) laeuft_ = false;
        }
    }
    return rueckmeldung() != vorher;
}

WagenRueckmeldung GeregelterWagen::rueckmeldung() const
{
    WagenRueckmeldung r;
    r.lauf = e_.lfg || laeuft_;
    r.takt_vor = puls_vor_ > 0;
    r.takt_rueck = puls_rueck_ > 0;
    r.vk = pos_ > p_.vk_bis;
    r.lr = pos_ > p_.lr_bis;
    r.rr = pos_ < p_.rr_ab;
    return r;
}
