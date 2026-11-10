#include "core/cards/gun_net/gun_net.h"

#include <cstdio>
#include <stdexcept>

namespace k1520 {

namespace {
constexpr uint8_t PB_UEBERNOMMEN = 0x01, PB_DATEN = 0x80;
// Zahl der Parameterbytes je Kommando (50a Abschnitt 4); -1 = unbekannt
int parameterZahl(uint8_t c) {
    switch (c) {
        case 0x00: case 0x01: case 0x02: case 0x05: return 2;
        case 0x04: return 3;
        case 0x06: return 5;
        case 0x07: return 1;
        case 0x03: case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: return 0;
        default: return -1;
    }
}
}  // namespace

GunNet::GunNet() : GunNet(Config()) {}

GunNet::GunNet(const Config& cfg) : cfg_(cfg), pio_("KCNet-PIO") {
    if (cfg_.basis & 0x03) throw std::invalid_argument("KCNet: Basis muss ein Vielfaches von 4 sein");
    w5100_ = std::make_unique<w5100::W5100>(cfg_.richtlinie);
    for (int i = 0; i < 4; ++i) server_ip_[0][i] = cfg_.dns_server[i];
    pio_.setPortAOutputCallback([this](uint8_t d) { out_neu_ = true; out_byte_ = d; });
    verdrahten();
}

GunNet::~GunNet() = default;

void GunNet::verdrahten() {
    pio_.setPullupsB(0xFF);
    pegelAnlegen();
}

std::string GunNet::belegtGrund(const K1520Bus& bus) const {
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

void GunNet::attachToBus(K1520Bus& bus) { bus.registerIO(this, cfg_.basis, ANZAHL_TORE); }

void GunNet::verwerfen() {
    phase_ = Phase::Bereit;
    par_n_ = 0;
    rest_ = 0;
    antwort_.clear();
    vorgelegt_ = false;
    pb0_ = false;
}

void GunNet::reset() {
    pio_.reset();
    verwerfen();
    zeiger_ = 0;
    w5100_->schreibe(w5100::reg::MR, w5100::reg::MR_RESET);   // Sockets zu, Register leer; Richtlinie bleibt
    for (int i = 0; i < 6; ++i) w5100_->schreibe(uint16_t(w5100::reg::SHAR + i), cfg_.mac[i]);   // Firmware setzt die MAC
    verdrahten();
}

void GunNet::pegelAnlegen() {
    const uint8_t v = uint8_t((pb0_ ? PB_UEBERNOMMEN : 0) | (vorgelegt_ ? PB_DATEN : 0));
    pio_.setExternB(v, uint8_t(PB_UEBERNOMMEN | PB_DATEN));
}

void GunNet::vorlegen() {
    if (vorgelegt_ || antwort_.empty()) return;
    const uint8_t b = antwort_.front();
    antwort_.pop_front();
    pio_.setExternA(b, 0xFF);
    pio_.setStbB(false);
    pio_.setStbB(true);     // BSTB-Impuls: Eingaberegister von Port A laden
    pio_.setExternA(0, 0x00);
    vorgelegt_ = true;
    ++zaehler_.bytes_zum_gast;
}

void GunNet::byteVomGast(uint8_t b) {
    ++zaehler_.bytes_zur_karte;
    switch (phase_) {
        case Phase::Bereit: {
            const int n = parameterZahl(b);
            if (n < 0) { ++zaehler_.fehlerkommandos; ++fehler_zaehl_; return; }   // verwerfen und zaehlen
            befehl_ = b;
            erwartet_ = uint8_t(n);
            par_n_ = 0;
            ++zaehler_.kommandos;
            letztes_ = b;
            if (n == 0) ausfuehren();
            else phase_ = Phase::Parameter;
            return;
        }
        case Phase::Parameter:
            par_[par_n_++] = b;
            if (par_n_ >= erwartet_) { phase_ = Phase::Bereit; ausfuehren(); }
            return;
        case Phase::DatenSchreiben:
            w5100_->schreibe(adr(), b);
            ++zeiger_;
            if (--rest_ == 0) phase_ = Phase::Bereit;
            return;
    }
}

void GunNet::ausfuehren() {
    const uint16_t p16 = uint16_t(par_[0] | (par_[1] << 8));
    switch (befehl_) {
        case 0x00:   // Block schreiben: Anzahl, dann Daten ab Zeiger
            rest_ = p16;
            if (rest_) phase_ = Phase::DatenSchreiben;
            break;
        case 0x01:   // Block lesen
            for (uint32_t i = 0; i < p16; ++i) { antwort_.push_back(w5100_->lese(adr())); ++zeiger_; }
            break;
        case 0x02: zeiger_ = p16; break;
        case 0x03: {
            const uint32_t hz = cfg_.takt_hz ? cfg_.takt_hz : 2'457'600;
            antwort16(uint16_t((takte_ * 1000ull / hz) % 60000ull));
            break;
        }
        case 0x04: w5100_->schreibe(uint16_t(p16 & 0x7FFF), par_[2]); break;
        case 0x05: antwort_.push_back(w5100_->lese(uint16_t(p16 & 0x7FFF))); break;
        case 0x06:
            for (int i = 0; i < 4; ++i) server_ip_[par_[0] & 7][i] = par_[1 + i];
            break;
        case 0x07:
            for (int i = 0; i < 4; ++i) antwort_.push_back(server_ip_[par_[0] & 7][i]);
            break;
        case 0x08:
            antwort16(dyn_port_);
            dyn_port_ = dyn_port_ == 0xFFFF ? uint16_t(49152) : uint16_t(dyn_port_ + 1);
            break;
        case 0x09: antwort_.push_back(cfg_.firmware_neben); antwort_.push_back(cfg_.firmware_haupt); break;
        case 0x0A: antwort_.push_back(cfg_.hardware_neben); antwort_.push_back(cfg_.hardware_haupt); break;
        case 0x0B: antwort_.push_back(cfg_.link_online ? 1 : 0); break;
        case 0x0C:
            for (char c : cfg_.id_string) antwort_.push_back(uint8_t(c));
            antwort_.push_back(0);
            break;
        case 0x0D: antwort16(fehler_zaehl_); break;
        default: break;
    }
}

uint8_t GunNet::ioRead(uint8_t port) {
    static const uint8_t abb[4] = {0, 2, 1, 3};   // A0 = Port, A1 = Daten/Steuer -> PIO-Zaehlung
    const unsigned i = (port - cfg_.basis) & 3;
    if (i == 1) {            // Statusabfrage: die Karte hat inzwischen geantwortet
        vorlegen();
        pegelAnlegen();
    }
    const uint8_t v = pio_.ioRead(abb[i]);
    if (i == 1) { pb0_ = false; pegelAnlegen(); }       // "uebernommen"
    else if (i == 0) { vorgelegt_ = false; pegelAnlegen(); }   // Lesen von Daten A leert das Eingaberegister
    return v;
}

void GunNet::ioWrite(uint8_t port, uint8_t d) {
    static const uint8_t abb[4] = {0, 2, 1, 3};
    const unsigned i = (port - cfg_.basis) & 3;
    out_neu_ = false;
    pio_.ioWrite(abb[i], d);
    if (i == 2 && (d & 0x0F) == 0x0F) { verwerfen(); pegelAnlegen(); }   // Betriebsartwort: Neuinitialisierung
    if (i == 0 && out_neu_) {
        out_neu_ = false;
        const auto st = pio_.debugState();
        if (st.port[0].mode == 2) {
            byteVomGast(out_byte_);
            pio_.setStbA(false);   // ASTB: die Karte hat das Byte uebernommen (ARDY)
            pio_.setStbA(true);
            pb0_ = true;
            pegelAnlegen();
        }
    }
}

}  // namespace k1520
