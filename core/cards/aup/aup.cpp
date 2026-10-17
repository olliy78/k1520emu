/**
 * @file aup.cpp
 * @brief AUP 062-8531, Tastaturteil (Registermodell, Entwurf 33 N10c).  Annahmen: aup.h.
 */
#include "core/cards/aup/aup.h"

#include "core/cards/aup/aup_tabelle.h"

AUP::AUP() : AUP(Config{}) {}

AUP::AUP(const Config& cfg) : cfg_(cfg) {}

void AUP::attachToBus(K1520Bus& bus) { bus.registerIO(this, cfg_.basis, 4); }

void AUP::reset() {
    allesLoeschen();      // [A6] /RESET wirkt wie FFH
    kommando_ = 0xFF;
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
    if (static_cast<uint8_t>(port - cfg_.basis) != 3) return;   // [A2] 40H..42H wirkungslos
    aktualisieren();   // Tasten vor dem Kommando einarbeiten (FFH verwirft sie dann)
    kommandoAusfuehren(d);
}
