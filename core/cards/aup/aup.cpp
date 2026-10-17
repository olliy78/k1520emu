/**
 * @file aup.cpp
 * @brief AUP 062-8531, Tastaturteil (Registermodell, Entwurf 33 N10c).  Annahmen: aup.h.
 */
#include "core/cards/aup/aup.h"

#include <utility>

#include "core/cards/aup/aup_tabelle.h"
#include "core/serial/sio_format.h"

// ─── Sonderschnittstelle: SIO-Kanal B als SerialAnschluss (N10d) ─────────────
// Muster K8025::Anschluss.  TTL-Pegel, keine V.24-Steuerleitungen (v24() = false).
class AUP::Sonder : public k1520::serial::SerialAnschluss {
public:
    explicit Sonder(AUP& k) : k_(k) {}

    // Name nach Beleg: der Beleg nennt "Sonderschnittstelle" und X2; das Wort SIMO kommt nicht vor.
    const char* name() const override { return "Sonderschnittstelle"; }
    const char* stecker() const override { return "X2"; }
    bool v24() const override { return false; }

    k1520::serial::SerialFormat format() const override {
        return k1520::serial::serialFormatAusSio(kanal(), *k_.ctc_, 0);   // Takt: CTC-Kanal 0
    }
    bool senderHatZeichen() const override { return kanal().senderHatZeichen(); }
    uint8_t senderNimm() override {
        uint8_t b = kanal().txGet();
        const uint8_t bits = kanal().format().tx_bits;
        if (bits < 8) b &= static_cast<uint8_t>((1u << bits) - 1);
        k_.seriell_geaendert_ = true;   // Tx leer -> Tx-Interrupt moeglich
        return b;
    }
    bool empfaengerFrei() const override { return kanal().empfaengerFrei(); }
    void empfange(uint8_t b) override {
        kanal().rxByte(b);
        k_.kette();
        k_.seriell_geaendert_ = true;
    }
    bool rts() const override { return kanal().rts(); }
    bool dtr() const override { return kanal().dtr(); }
    bool breakGesendet() const override { return kanal().breakSenden(); }
    void breakEmpfang(bool aktiv) override {
        kanal().setzeBreakEmpfang(aktiv);
        k_.kette();
        k_.seriell_geaendert_ = true;
    }

private:
    Z80SIO::Channel& kanal() const { return k_.sio_->channelB(); }
    AUP& k_;
};

AUP::AUP() : AUP(Config{}) {}

AUP::AUP(const Config& cfg) : cfg_(cfg) {
    if (cfg_.sonder) {
        ctc_ = std::make_unique<Z80CTC>("AUP-CTC-A12");
        sio_ = std::make_unique<Z80SIO>("AUP-SIO-A13");
        anschluss_ = std::make_unique<Sonder>(*this);
    }
}

AUP::~AUP() = default;

k1520::serial::SerialAnschluss* AUP::anschluss() { return anschluss_.get(); }

void AUP::attachToBus(K1520Bus& bus) {
    bus.registerIO(this, cfg_.basis, 4);
    if (cfg_.sonder) bus.registerIO(this, cfg_.basis_sonder, 8);
}

void AUP::reset() {
    allesLoeschen();      // [A6] /RESET wirkt wie FFH
    kommando_ = 0xFF;
    if (cfg_.sonder) {    // systemweiter /RESET: Baustein-Reset (wie K8025::reset)
        ctc_->reset();
        sio_->reset();
        kette();
    }
}

// ─── Interruptkette (nur Sonderschnittstelle) ───────────────────────────────

void AUP::kette() {
    if (!sio_) return;
    sio_->setIEI(iei_);                 // [A10] SIO vor CTC
    ctc_->setIEI(sio_->getIEO());
}

void AUP::setIEI(bool iei) { iei_ = iei; kette(); }

bool AUP::getIEO() const {
    if (!sio_) return iei_;             // ohne Sonderschnittstelle: durchgereicht
    if (!iei_) return false;
    return !sio_->hasInterrupt() && !ctc_->hasInterrupt();
}

bool AUP::hasInterrupt() const {
    if (!sio_ || !iei_) return false;
    return sio_->hasInterrupt() || ctc_->hasInterrupt();
}

uint8_t AUP::getVector() const {
    if (!sio_) return 0xFF;
    if (sio_->hasInterrupt()) return sio_->getVector();
    if (ctc_->hasInterrupt()) return ctc_->getVector();
    return 0xFF;
}

void AUP::onRETI() {
    if (!sio_) return;
    sio_->onRETI();
    ctc_->onRETI();
    kette();
}

const char* AUP::intDeviceName() const {
    if (sio_ && sio_->hasInterrupt()) return "AUP SIO-A13 (Sonderschnittstelle)";
    if (ctc_ && ctc_->hasInterrupt()) return "AUP CTC-A12";
    return "AUP";
}

// ─── Tabellen ───────────────────────────────────────────────────────────────

int AUP::code(Tastatur t, int pos, bool alpha) {
    if (t == Tastatur::K7633) {
        if (pos < 0 || pos >= 0x48 || aup_tabelle::kK7633[pos].name[0] == '\0') return -1;
        return alpha ? aup_tabelle::kK7633[pos].mit : aup_tabelle::kK7633[pos].ohne;
    }
    if (t == Tastatur::Kunde) {
        const int i = pos - aup_tabelle::kKundeErste;
        if (i < 0 || i >= 16 || aup_tabelle::kKundeName[i][0] == '\0') return -1;
        return aup_tabelle::kKunde[i];
    }
    return -1;
}

const char* AUP::name(Tastatur t, int pos) {
    if (t == Tastatur::K7633) return (pos >= 0 && pos < 0x48) ? aup_tabelle::kK7633[pos].name : "";
    if (t == Tastatur::Kunde) {
        const int i = pos - aup_tabelle::kKundeErste;
        return (i >= 0 && i < 16) ? aup_tabelle::kKundeName[i] : "";
    }
    return "";
}

bool AUP::dauerfunktion(Tastatur t, int pos) {
    if (t != Tastatur::K7633) return false;
    for (uint8_t p : aup_tabelle::kDauer)
        if (p == pos) return true;
    return false;
}

// ─── Zustand ────────────────────────────────────────────────────────────────

bool AUP::alphaAktiv() const {
    switch (cfg_.alpha) {
        case Alpha::Ohne: return false;
        case Alpha::Mit:  return true;
        case Alpha::NachLed: break;
    }
    return (kommando_ & 0x02) == 0;   // [A3] ALPHA-LED an = Tabelle "mit Alpha"
}

uint8_t AUP::lampen() const {
    uint8_t m = 0;
    if (!(kommando_ & 0x01)) m |= kLampeSel;
    if (!(kommando_ & 0x02)) m |= kLampeAlpha;
    if (!(kommando_ & 0x10)) m |= kLampeGet;
    if (!(kommando_ & 0x20)) m |= kLampeFehler;
    return m;
}

bool AUP::schluesselAn(int n) const {
    return (n == 1 || n == 2) && schluessel_[n - 1].load();
}

bool AUP::taste(Tastatur t, int pos, bool gedrueckt) {
    if (code(t, pos, false) < 0) return false;
    if (t == Tastatur::K7633 && pos < 2) schluessel_[pos].store(gedrueckt);
    std::lock_guard<std::mutex> lk(mutex_);
    // Eine Flut ohne abfragenden Gast nicht unbegrenzt sammeln (die Schlange begrenzt ohnehin).
    if (eingang_.size() < 4 * kSchlange) eingang_.push_back({t, static_cast<uint8_t>(pos), gedrueckt});
    return true;
}

void AUP::allesLoeschen() {
    a5_ = 0;
    wartend_.clear();
    wiederhol_pos_ = -1;
}

void AUP::aktualisieren() {
    const uint64_t t = jetzt();
    std::vector<Ereignis> ev;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        ev.swap(eingang_);
    }
    for (const Ereignis& e : ev) {
        const int k = static_cast<int>(e.t);
        bool& g = gehalten_[k][e.pos];
        if (!e.gedrueckt) {
            g = false;
            if (e.t == Tastatur::K7633 && e.pos == wiederhol_pos_) wiederhol_pos_ = -1;
            continue;
        }
        if (g) continue;   // schon gehalten: kein zweiter Code
        g = true;
        const int c = code(e.t, e.pos, alphaAktiv());
        if (c > 0 && static_cast<int>(wartend_.size()) < kSchlange)      // [A7] 00H liefert nichts
            wartend_.push_back({e.t, static_cast<uint8_t>(c), t + takte(cfg_.entprell_us)});
        if (dauerfunktion(e.t, e.pos)) {
            wiederhol_pos_ = e.pos;
            wiederhol_ab_  = t + takte(cfg_.entprell_us) + takte(cfg_.wiederhol_ab_us);
        }
    }
    // Dauerfunktion: nur nachlegen, wenn nichts mehr ansteht (keine Stapelung).
    if (wiederhol_pos_ >= 0 && t >= wiederhol_ab_ && a5_ == 0 && wartend_.empty()) {
        const int c = code(Tastatur::K7633, wiederhol_pos_, alphaAktiv());
        if (c > 0) wartend_.push_back({Tastatur::K7633, static_cast<uint8_t>(c), t});
        wiederhol_ab_ = t + takte(cfg_.wiederhol_us);
    }
    if (a5_ == 0 && !wartend_.empty() && wartend_.front().ab <= t) {
        const Code& c = wartend_.front();
        a6_ = c.code;
        a5_ = c.t == Tastatur::K7633 ? cfg_.bit_terminal : cfg_.bit_kunde;
        wartend_.pop_front();
    }
}

void AUP::kommandoAusfuehren(uint8_t d) {
    if (!(d & 0x80)) { ++verworfen_; return; }   // [A6] Kennung fehlt
    if (d == 0xFF) {                             // RESET AUP
        allesLoeschen();
        kommando_ = 0xFF;
        return;
    }
    kommando_ = d;
    if (!(d & 0x40)) ++hupe_;                    // [A5] ein Signal je Kommando
}

// ─── Bus ────────────────────────────────────────────────────────────────────

uint8_t AUP::ioRead(uint8_t port) {
    if (cfg_.sonder && static_cast<uint8_t>(port - cfg_.basis_sonder) < 8) {
        const uint8_t rel = static_cast<uint8_t>(port - cfg_.basis_sonder);
        const uint8_t r = rel < 4 ? ctc_->ioRead(rel & 3) : sio_->ioRead(rel & 3);   // [A9]
        kette();
        return r;
    }
    switch (static_cast<uint8_t>(port - cfg_.basis)) {
        case 1:
            aktualisieren();
            return a5_;
        case 2: {
            aktualisieren();
            const uint8_t v = a6_;
            a5_ = 0;          // Lesen loescht die Gueltigkeit (A6-23 -> A16-12 -> A5)
            return v;
        }
        default:
            return 0xFF;      // [A2] 40H, 43H
    }
}

void AUP::ioWrite(uint8_t port, uint8_t d) {
    if (cfg_.sonder && static_cast<uint8_t>(port - cfg_.basis_sonder) < 8) {
        const uint8_t rel = static_cast<uint8_t>(port - cfg_.basis_sonder);
        if (rel < 4) ctc_->ioWrite(rel & 3, d);
        else         sio_->ioWrite(rel & 3, d);
        kette();
        return;
    }
    if (static_cast<uint8_t>(port - cfg_.basis) != 3) return;   // [A2] 40H..42H wirkungslos
    aktualisieren();   // Tasten vor dem Kommando einarbeiten (FFH verwirft sie dann)
    kommandoAusfuehren(d);
}
