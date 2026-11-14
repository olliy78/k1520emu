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

// "Blockierende Zustaende bei PIO-Ausgabe bzw. -Eingabe werden nach 524 ms abgebrochen und die komplette Befehlsverarbeitung
// zurueckgesetzt" (KCNET Protokoll V1.2).  Hier: ein angefangenes Kommando oder eine nicht abgeholte Antwort verfaellt, wenn
// 524 ms MASCHINENZEIT lang kein Byte geflossen ist.  Der Zeiger (Kommando 02) und die Zaehler bleiben.
void GunNet::fristPruefen() {
    if (phase_ == Phase::Bereit && antwort_.empty() && !vorgelegt_) return;
    const uint64_t hz = cfg_.takt_hz ? cfg_.takt_hz : 2'457'600;
    if (takte_ >= aktiv_seit_ && takte_ - aktiv_seit_ > hz * FRIST_MS / 1000) {
        verwerfen();
        pegelAnlegen();
        ++zaehler_.fristabbrueche;
    }
}

void GunNet::schreibeGast(uint16_t a, uint8_t b) {
    if (a < 0x8000) return;                  // PIO schreibt nur im TCP/IP-Stack (8000H..FFFFH)
    if (a >= 0x8009 && a <= 0x800E) return;  // Ethernet-MAC (SHAR) ist fuer die PIO nicht beschreibbar
    w5100_->schreibe(uint16_t(a & 0x7FFF), b);
}

void GunNet::reset() {
    pio_.reset();
    verwerfen();
    zeiger_ = 0;
    w5100_->schreibe(w5100::reg::MR, w5100::reg::MR_RESET);   // Sockets zu, Register leer; Richtlinie bleibt
    for (int i = 0; i < 6; ++i) w5100_->schreibe(uint16_t(w5100::reg::SHAR + i), cfg_.mac[i]);   // Firmware setzt die MAC
    verdrahten();
}

// Port-B-Beschaltung der Karte (Hardware 1.2, Artikel "KCNET 1.2" von R. Kaestner): ARDY der PIO geht direkt auf PB0,
// BRDY INVERTIERT auf PB7, PB1..PB6 liegen an Masse (bei Eingabe immer 0).  Daraus folgen die beiden Warteschleifen des Gastes:
//  - Senden:    ARDY ist 1, solange das Ausgaberegister von Port A gefuellt ist und die Gegenseite (/ASTB) es noch nicht
//               abgeholt hat -> "warte bis PB0 = 0" heisst "Byte uebernommen".
//  - Empfangen: BRDY ist 1, solange das Eingaberegister LEER ist (die Gegenseite darf /BSTB geben); nach /BSTB faellt es auf 0,
//               nach dem Lesen von Daten A steigt es wieder.  Invertiert auf PB7 heisst das: PB7 = 1 <=> Byte da,
//               "warte bis PB7 = 1" und nach dem Lesen sofort wieder 0.
// Das Modell fuehrt beides als Zustandsbits (pb0_/vorgelegt_); der echte ATmega ist schnell genug, dass PB0 nach dem OUT
// mindestens eine Abfrage lang 1 ist (50a Abschnitt 3).  Dass die K1520-Platine B1/B5/B6 fuer die EEPROM-Segmentwahl
// (eepload.COM) benutzt, betrifft eine andere Bestueckung; die Netzseite braucht nur B0 und B7.
void GunNet::pegelAnlegen() {
    const uint8_t v = uint8_t((pb0_ ? PB_UEBERNOMMEN : 0) | (vorgelegt_ ? PB_DATEN : 0));
    pio_.setExternB(v, 0xFF);   // PB1..PB6 = 0
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
    aktiv_seit_ = takte_;
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
            schreibeGast(zeiger_, b);
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
        case 0x04: schreibeGast(p16, par_[2]); break;
        case 0x05: antwort_.push_back(w5100_->lese(uint16_t(p16 & 0x7FFF))); break;
        case 0x06:   // IP-Nummer 0..7; groessere Nummer = Kommandofehler (Annahme [?], die Beschreibung sagt es nicht)
            if (par_[0] > 7) { ++zaehler_.fehlerkommandos; ++fehler_zaehl_; break; }
            for (int i = 0; i < 4; ++i) server_ip_[par_[0]][i] = par_[1 + i];
            break;
        case 0x07:   // Antwort: Net-Order (hoechstwertiges Byte zuerst), so wie gespeichert
            if (par_[0] > 7) { ++zaehler_.fehlerkommandos; ++fehler_zaehl_; break; }
            for (int i = 0; i < 4; ++i) antwort_.push_back(server_ip_[par_[0]][i]);
            break;
        case 0x08:   // dynamische Portnummer: Net-Order (b2, b1) = hoeherwertiges Byte ZUERST
            antwort_.push_back(uint8_t(dyn_port_ >> 8)); antwort_.push_back(uint8_t(dyn_port_ & 0xFF));
            dyn_port_ = dyn_port_ == 0xFFFF ? uint16_t(49152) : uint16_t(dyn_port_ + 1);
            break;
        case 0x09: antwort_.push_back(cfg_.firmware_neben); antwort_.push_back(cfg_.firmware_haupt); break;
        case 0x0A: antwort_.push_back(cfg_.hardware_neben); antwort_.push_back(cfg_.hardware_haupt); break;
        case 0x0B: antwort_.push_back(cfg_.link_online ? 1 : 0); break;
        case 0x0C:
            for (std::size_t i = 0; i < cfg_.id_string.size() && i < 127; ++i) antwort_.push_back(uint8_t(cfg_.id_string[i]));
            antwort_.push_back(0);   // Zeichenkette + 0, hoechstens 128 Byte
            break;
        case 0x0D: antwort16(fehler_zaehl_); break;
        default: break;
    }
}

uint8_t GunNet::ioRead(uint8_t port) {
    static const uint8_t abb[4] = {0, 2, 1, 3};   // A0 = Port, A1 = Daten/Steuer -> PIO-Zaehlung
    const unsigned i = (port - cfg_.basis) & 3;
    fristPruefen();
    if (i == 0) aktiv_seit_ = takte_;   // Abholen einer Antwort = Aktivitaet
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
    fristPruefen();
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
