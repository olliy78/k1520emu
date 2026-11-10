#include "core/cards/gide/gide.h"

#include <cstdio>
#include <stdexcept>

namespace k1520 {

using Reg = AtaGeraet::Reg;

Gide::Gide() : Gide(Config()) {}

Gide::Gide(const Config& cfg) : cfg_(cfg) {
    if (cfg_.basis & 0x0F) throw std::invalid_argument("GIDE: Basis muss ein Vielfaches von 10h sein");
}

std::string Gide::belegtGrund(const K1520Bus& bus) const {
    for (unsigned i = 0; i < ANZAHL_TORE; ++i) {
        const uint8_t p = uint8_t(cfg_.basis + 4 + i);
        if (const BusDevice* d = bus.ioOwner(p)) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "E/A-Tor %02XH belegt (%s)", p, d->deviceName());
            return buf;
        }
    }
    return "";
}

void Gide::attachToBus(K1520Bus& bus) {
    bus_ = &bus;
    bus.registerIO(this, uint8_t(cfg_.basis + 4), ANZAHL_TORE);
}

void Gide::reset() {
    lh_ = false;
    merker_ = 0;
    platte_.reset();
}

uint8_t Gide::ioRead(uint8_t port) {
    const unsigned off = uint8_t(port - cfg_.basis);
    if (off < 4 || off > 0x0F) return 0xFF;
    switch (off) {
        case 4: return 0xFF;
        case 5:
            if (cfg_.rtc_vorhanden && rtcLese)
                return rtcLese(uint8_t(bus_ ? (bus_->ioAddress() >> 8) & 0x0F : 0));
            return 0xFF;
        case 6: return uint8_t(platte_.lese(Reg::AltStatusDeviceControl, true));
        case 7: return uint8_t(platte_.lese(Reg::DriveAddress, true));
        case 8: {
            ++zaehler_.daten_lesen;
            uint8_t r;
            if (!lh_) {
                const uint16_t w = platte_.lese(Reg::Data);
                merker_ = uint8_t(w >> 8);
                r = uint8_t(w);
            } else {
                r = merker_;
            }
            lh_ = !lh_;           // Takt am Zugriffsende
            return r;
        }
        default: {
            ++zaehler_.taskfile;
            const uint8_t r = uint8_t(platte_.lese(Reg(off - 8)));
            lh_ = false;
            return r;
        }
    }
}

void Gide::ioWrite(uint8_t port, uint8_t d) {
    const unsigned off = uint8_t(port - cfg_.basis);
    if (off < 4 || off > 0x0F) return;
    switch (off) {
        case 4: return;
        case 5:
            if (cfg_.rtc_vorhanden && rtcSchreibe)
                rtcSchreibe(uint8_t(bus_ ? (bus_->ioAddress() >> 8) & 0x0F : 0), d);
            return;
        case 6: platte_.schreibe(Reg::AltStatusDeviceControl, d, true); return;
        case 7: return;                                   // Drive Address ist nur lesbar
        case 8:
            ++zaehler_.daten_schreiben;
            if (!lh_) {
                merker_ = d;
            } else {
                platte_.schreibe(Reg::Data, uint16_t(merker_ | (uint16_t(d) << 8)));
            }
            lh_ = !lh_;
            return;
        default:
            ++zaehler_.taskfile;
            if (off == 0x0F) { letzter_befehl_ = d; ++zaehler_.befehle; }
            platte_.schreibe(Reg(off - 8), d);
            lh_ = false;
            return;
    }
}

}  // namespace k1520
