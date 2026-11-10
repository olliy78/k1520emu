#include "core/cards/gun_usb/gun_usb.h"

#include <cstdio>
#include <stdexcept>

namespace k1520 {

namespace {
constexpr uint8_t B_RXF = 0x01, B_TXE = 0x02, B_RD = 0x04, B_WR = 0x08, B_RES = 0x40;
}

GunUsb::GunUsb() : GunUsb(Config()) {}

GunUsb::GunUsb(const Config& cfg) : cfg_(cfg), pio_("GUN-USB-PIO") {
    if (cfg_.basis & 0x03) throw std::invalid_argument("GUN-USB: Basis muss ein Vielfaches von 4 sein");
    if (cfg_.stick_vorhanden && !cfg_.ordner.empty()) {
        std::string f;
        if (!vdip_.stickEinstecken({cfg_.ordner, cfg_.schreibgeschuetzt}, &f)) throw std::invalid_argument("GUN-USB: " + f);
        vdip_.reset();   // Startmeldung nach dem Einstecken neu (Stick war beim Hochlauf schon da)
    }
    verdrahten();
}

void GunUsb::verdrahten() {
    pio_.setPullupsB(0xFF);   // B4/B5 offen -> High
    pegelAnlegen();
    pb_alt_ = pio_.pinsB();
    astb_low_ = false;
}

std::string GunUsb::belegtGrund(const K1520Bus& bus) const {
    for (unsigned i = 0; i < ANZAHL_TORE; ++i) {
        const uint8_t p = uint8_t(cfg_.basis + i);
        if (const BusDevice* d = bus.ioOwner(p)) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "E/A-Tor %02XH belegt (%s)", p, d->deviceName());
            return buf;
        }
    }
    return "";
}

void GunUsb::attachToBus(K1520Bus& bus) { bus.registerIO(this, cfg_.basis, ANZAHL_TORE); }

void GunUsb::reset() {
    pio_.reset();
    vdip_.reset();
    verdrahten();
}

bool GunUsb::stickEinstecken(const std::filesystem::path& ordner, bool schreibgeschuetzt, std::string* fehler) {
    return vdip_.stickEinstecken({ordner, schreibgeschuetzt}, fehler);
}

void GunUsb::stickZiehen() { vdip_.stickZiehen(); }

void GunUsb::pegelAnlegen() {
    // RXF# = 0, wenn Daten da sind; TXE# = 0, wenn der Chip annimmt.  Nur B0/B1 sind getrieben.
    const uint8_t v = uint8_t((vdip_.datenBereit() ? 0 : B_RXF) | (vdip_.sendenBereit() ? 0 : B_TXE));
    pio_.setExternB(v, uint8_t(B_RXF | B_TXE));
}

void GunUsb::flankenAuswerten(bool ereignisse) {
    const uint8_t neu = pio_.pinsB();
    const uint8_t alt = pb_alt_;
    pb_alt_ = neu;
    if (!ereignisse) { pegelAnlegen(); return; }   // Pegel nachfuehren, keine Bus-Zyklen

    // ASTB = NAND(B2,B3): low bei RD# inaktiv UND WR aktiv (Pegel, nicht Flanke).
    const bool astb_low = (neu & B_RD) && (neu & B_WR);

    // WR# des Chips: das Byte wird mit dem Ende des WR-Impulses (B3 high -> low) uebernommen.
    if ((alt & B_WR) && !(neu & B_WR)) {
        if (vdip_.sendenBereit()) {
            vdip_.senden(pio_.portARead());   // Ausgaberegister von Port A (Betriebsart 2)
            ++zaehler_.geschrieben;
        }
    }
    if (astb_low != astb_low_) {
        astb_low_ = astb_low;
        pio_.setStbA(!astb_low);
    }

    // RD# aktiv (B2 high -> low): naechstes Byte auf den Bus, BSTB-Impuls laedt es ins Eingaberegister.
    if ((alt & B_RD) && !(neu & B_RD) && vdip_.datenBereit()) {
        pio_.setExternA(vdip_.empfangen(), 0xFF);
        pio_.setStbB(false);
        pio_.setStbB(true);
        pio_.setExternA(0, 0x00);   // VDIP1 gibt den Bus wieder frei
        ++zaehler_.gelesen;
    }

    // RES# low -> high: Firmware-Neustart.
    if ((neu & B_RES) && !(alt & B_RES)) {
        vdip_.reset();
        ++zaehler_.resets;
    }
    pegelAnlegen();
}

bool GunUsb::portBKonfiguriert() const {
    // Erst wenn Port B im Bitbetrieb ist UND RD#, WR, RES# Ausgaenge sind, sind die Leitungen am VDIP1 definiert.
    // Davor (Einschaltzustand, halbfertiges Initialisieren) haengen die Pegel an Pull-ups bzw. dem Rest des
    // Ausgaberegisters; solche Fluchten wuerden sonst als Bus-Zyklen gezaehlt (ein 00h-Byte, ein verlorenes Datum).
    const auto st = pio_.debugState();
    return st.port[1].mode == 3 && (st.port[1].dir & (B_RD | B_WR | B_RES)) == 0;
}

uint8_t GunUsb::ioRead(uint8_t port) {
    pegelAnlegen();   // Stick kann zwischen den Zugriffen gesteckt/gezogen worden sein
    static const uint8_t abb[4] = {0, 2, 1, 3};   // A0 = Port, A1 = Daten/Steuer -> PIO-Zaehlung
    const bool vor = portBKonfiguriert();
    const uint8_t v = pio_.ioRead(abb[(port - cfg_.basis) & 3]);
    flankenAuswerten(vor && portBKonfiguriert());
    return v;
}

void GunUsb::ioWrite(uint8_t port, uint8_t d) {
    static const uint8_t abb[4] = {0, 2, 1, 3};
    const bool vor = portBKonfiguriert();
    pio_.ioWrite(abb[(port - cfg_.basis) & 3], d);
    flankenAuswerten(vor && portBKonfiguriert());
}

}  // namespace k1520
