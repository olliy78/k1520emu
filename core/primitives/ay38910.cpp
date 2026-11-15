/**
 * @file ay38910.cpp
 * @brief AY-3-8910 / AY-3-8912 - Umsetzung, siehe ay38910.h
 */
#include "ay38910.h"
#include <cmath>

namespace {
// Lesemaske je Register (Bits, die das Datenblatt als vorhanden fuehrt)
constexpr uint8_t kLeseMaske[16] = {
    0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF,
    0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF};
}  // namespace

Ay38910::Pegeltabelle Ay38910::vorgabePegel() {
    Pegeltabelle t{};
    t[0] = 0.0f;
    for (int n = 1; n < 16; ++n)
        t[n] = static_cast<float>(std::pow(2.0, -(15 - n) / 2.0));
    return t;
}

Ay38910::Ay38910(Variante v, uint32_t takt) : variante_(v), takt_(takt), pegel_(vorgabePegel()) {
    reset();
}

void Ay38910::reset() {
    reg_.fill(0);
    latch_ = 0;
    tonZ_.fill(0);
    tonBit_.fill(0);
    rauschZ_ = 0;
    lfsr_ = 1;
    rauschBit_ = 1;
    halbTakt_ = false;
    huellkurveStart();
}

void Ay38910::adresseSchreiben(uint8_t a) { latch_ = a; }

void Ay38910::datenSchreiben(uint8_t d, uint64_t chipTakt) {
    rechneBis(chipTakt);
    datenSchreiben(d);
}

void Ay38910::datenSchreiben(uint8_t d) {
    if (latch_ >= 16) return;
    reg_[latch_] = d;
    if (latch_ == 13) huellkurveStart();
}

uint8_t Ay38910::portPins(int port) const {
    const int p = port & 1;
    const bool aus = (reg_[7] >> (6 + p)) & 1;
    // Der 8912 fuehrt nur Port A heraus: Port B hat keine Pins, Pull-ups bleiben
    if (aus && !(p == 1 && variante_ == Variante::AY8912)) return reg_[14 + p];
    return p == 1 && variante_ == Variante::AY8912 ? 0xFF : pin_[p];
}

uint8_t Ay38910::datenLesen() const {
    if (latch_ >= 16) return 0xFF;
    if (latch_ == 14 || latch_ == 15) {
        const int p = latch_ - 14;
        const bool aus = (reg_[7] >> (6 + p)) & 1;
        if (aus) return reg_[latch_];     // Ausgang: Registerwert
        // 8912: Port B nicht herausgefuehrt -> Pull-up
        return (p == 1 && variante_ == Variante::AY8912) ? 0xFF : pin_[p];
    }
    return reg_[latch_] & kLeseMaske[latch_];
}

void Ay38910::huellkurveStart() {
    const uint8_t f = reg_[13] & 0x0F;
    hkZ_ = 0;
    hkStufe_ = 0;
    hkAuf_ = (f & 0x04) != 0;   // ATT
    hkHalt_ = false;
    hkPegel_ = hkAuf_ ? 0 : 15;
}

void Ay38910::huellkurveSchritt() {
    if (hkHalt_) return;
    const uint8_t f = reg_[13] & 0x0F;
    const bool cont = f & 0x08, alt = f & 0x02, halt = f & 0x01;
    if (hkStufe_ < 15) {
        ++hkStufe_;
    } else {
        // Durchlauf zu Ende
        const int ende = hkAuf_ ? 15 : 0;
        if (!cont) {
            hkHalt_ = true; hkPegel_ = 0; return;
        }
        if (halt) {
            hkHalt_ = true; hkPegel_ = alt ? 15 - ende : ende; return;
        }
        if (alt) hkAuf_ = !hkAuf_;
        hkStufe_ = 0;
    }
    hkPegel_ = hkAuf_ ? hkStufe_ : 15 - hkStufe_;
}

uint8_t Ay38910::pegelKanal(int k) const {
    const uint8_t r = reg_[8 + k];
    return (r & 0x10) ? static_cast<uint8_t>(hkPegel_) : static_cast<uint8_t>(r & 0x0F);
}

void Ay38910::rasterSchritt() {
    // Tonzaehler: jeder Rasterschritt (Takt/8), Kippen bei Erreichen der Periode
    for (int k = 0; k < 3; ++k) {
        uint32_t per = reg_[2 * k] | ((reg_[2 * k + 1] & 0x0F) << 8);
        if (per == 0) per = 1;
        if (++tonZ_[k] >= per) { tonZ_[k] = 0; tonBit_[k] ^= 1; }
    }
    // Takt/16-Zaehler: Rauschen und Huellkurve
    halbTakt_ = !halbTakt_;
    if (!halbTakt_) {
        uint32_t np = reg_[6] & 0x1F;
        if (np == 0) np = 1;
        if (++rauschZ_ >= np) {
            rauschZ_ = 0;
            const uint32_t rueck = (lfsr_ ^ (lfsr_ >> 3)) & 1;
            lfsr_ = (lfsr_ >> 1) | (rueck << 16);
            rauschBit_ = static_cast<uint8_t>(lfsr_ & 1);
        }
        uint32_t ep = reg_[11] | (reg_[12] << 8);
        if (ep == 0) ep = 1;
        if (++hkZ_ >= ep) { hkZ_ = 0; huellkurveSchritt(); }
    }
}

void Ay38910::abtasten() {
    // Kanalwerte dieses Rasterschritts (mit optionalem Vorfilter auf dem Raster)
    const uint8_t mix = reg_[7];
    double v[3];
    for (int k = 0; k < 3; ++k) {
        const bool ton = tonBit_[k] || ((mix >> k) & 1);
        const bool rau = rauschBit_ || ((mix >> (3 + k)) & 1);
        v[k] = (ton && rau) ? pegel_[pegelKanal(k) & 15] : 0.0f;
        if (tpKoef_ > 0.0f) {
            for (int s = 0; s < 2; ++s) {
                tpZ_[k][s] += tpKoef_ * (static_cast<float>(v[k]) - tpZ_[k][s]);
                v[k] = tpZ_[k][s];
            }
        }
    }
    // Kastenfilter mit EXAKTEN Grenzen: der Rasterschritt [rasterNr-1, rasterNr) wird anteilig
    // auf die Ausgabewerte verteilt, in die er faellt (sonst rastet die Grenze auf ganze
    // Rasterschritte ein und erzeugt bei hohen Toenen hoerbaren Jitter-Rauschteppich).
    const double T = takt_ / (8.0 * rate_);          // Rasterschritte je Ausgabewert
    double von = static_cast<double>(rasterNr_ - 1);
    const double bis = static_cast<double>(rasterNr_);
    if (von < abtastNr_ * T) von = abtastNr_ * T;   // Start mitten in einem Wert: erst ab dem naechsten
    if (bis <= von) return;
    for (;;) {
        const double grenze = (abtastNr_ + 1) * T;
        if (bis <= grenze) {
            for (int k = 0; k < 3; ++k) summe_[k] += v[k] * (bis - von);
            break;
        }
        for (int k = 0; k < 3; ++k) summe_[k] += v[k] * (grenze - von);
        fertig_.push_back({static_cast<float>(summe_[0] / T), static_cast<float>(summe_[1] / T),
                           static_cast<float>(summe_[2] / T)});
        summe_ = {0, 0, 0};
        ++abtastNr_;
        von = grenze;
    }
}

void Ay38910::rechneBis(uint64_t chipTakt) {
    const uint64_t ziel = chipTakt / 8;
    while (rasterNr_ < ziel) {
        rasterSchritt();
        ++rasterNr_;
        if (rate_ != 0) abtasten();
    }
}

void Ay38910::ausgabeEin(uint32_t rate, float tiefpassHz) {
    rate_ = rate;
    summe_ = {0, 0, 0};
    tpZ_ = {};
    fertig_.clear();
    // Abtastzaehler auf die aktuelle Chipzeit ausrichten
    abtastNr_ = rate ? static_cast<uint64_t>(std::ceil(rasterNr_ * (8.0 * rate) / takt_)) : 0;
    if (tiefpassHz > 0.0f) {
        const double fs = takt_ / 8.0;
        tpKoef_ = static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265358979 * tiefpassHz / fs));
    } else {
        tpKoef_ = 0.0f;
    }
}

void Ay38910::ausgabeAus() {
    rate_ = 0;
    fertig_.clear();
}

size_t Ay38910::abholen(Abtastwert* ziel, size_t max) {
    size_t n = 0;
    while (n < max && !fertig_.empty()) { ziel[n++] = fertig_.front(); fertig_.pop_front(); }
    return n;
}

