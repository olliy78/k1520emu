#include "core/cards/sound_k1520/sound_k1520.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace k1520 {

namespace {
constexpr double PI = 3.14159265358979323846;
}

SoundK1520::SoundK1520() : SoundK1520(Config{}) {}

SoundK1520::SoundK1520(const Config& cfg)
    : cfg_(cfg), ctc_("SOUND-CTC"),
      ay_(cfg.chip, cfg.takt == Takt::System ? cfg.systemtakt_hz
                    : cfg.takt == Takt::Quarz ? cfg.quarz_hz : cfg.quarz_hz / 2) {
    if (cfg.basis & 7) throw std::invalid_argument("SoundK1520: basis muss ein Vielfaches von 8 sein");
    if (cfg.systemtakt_hz == 0 || cfg.quarz_hz == 0) throw std::invalid_argument("SoundK1520: Takt 0");
    switch (cfg.takt) {
        case Takt::System:     num_ = 1;           den_ = 1; break;
        case Takt::Quarz:      num_ = cfg.quarz_hz; den_ = cfg.systemtakt_hz; break;
        case Takt::QuarzHalbe: num_ = cfg.quarz_hz; den_ = 2ull * cfg.systemtakt_hz; break;
    }
    hp_koef_ = static_cast<float>(std::exp(-2.0 * PI * HOCHPASS_HZ / audio::kAbtastrate));
    tp_koef_ = static_cast<float>(1.0 - std::exp(-2.0 * PI * TIEFPASS_HZ / audio::kAbtastrate));

    // Kaskaden TO0->TR1 usw.; Maske nur fuer verdrahtete Kanaele (jeder maskierte Kanal ist teurer)
    const uint8_t k = cfg.kaskade & 7;
    if (k) {
        ctc_.setZCTOCallback([this, k](int kanal, bool pegel) {
            if (kanal >= 0 && kanal < 3 && ((k >> kanal) & 1)) ctc_.clkTrg(kanal + 1, pegel);
        }, k);
    }
}

uint8_t SoundK1520::ioRead(uint8_t port) {
    const unsigned rel = uint8_t(port - cfg_.basis);
    if (rel == 0) return ay_.datenLesen();
    if (rel >= 4 && rel <= 7) return ctc_.ioRead(uint8_t(rel - 4));
    return 0xFF;   // +1 (Bus offen) und alles Uebrige
}

void SoundK1520::ioWrite(uint8_t port, uint8_t d) {
    const unsigned rel = uint8_t(port - cfg_.basis);
    if (rel == 0) {
        ay_.adresseSchreiben(d);
    } else if (rel == 1) {
        ay_.datenSchreiben(d, chip_ticks_);   // rechnet zuerst bis zum letzten gemeldeten Takt nach
    } else if (rel >= 4 && rel <= 7) {
        ctc_.ioWrite(uint8_t(rel - 4), d);
    }
}

std::string SoundK1520::belegtGrund(const K1520Bus& bus) const {
    static const unsigned tore[] = {0, 1, 4, 5, 6, 7};
    for (unsigned t : tore) {
        const uint8_t p = uint8_t(cfg_.basis + t);
        if (const BusDevice* d = bus.ioOwner(p)) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "E/A-Tor %02XH belegt (%s)", p, d->deviceName());
            return buf;
        }
    }
    return "";
}

void SoundK1520::attachToBus(K1520Bus& bus) {
    bus.registerIO(this, cfg_.basis, 2);
    bus.registerIO(this, uint8_t(cfg_.basis + 4), 4);
}

void SoundK1520::reset() {
    ay_.reset();
    ctc_.reset();
    for (int i = 0; i < 2; ++i) x1_[i] = y1_[i] = tp_[i] = 0;
}

void SoundK1520::zeitStand(uint64_t total) {
    if (total < last_total_) {          // Maschinenzeit zurueckgesetzt: neu aufsetzen, Chipzeit bleibt monoton
        last_total_ = total;
        acc_ = 0;
        return;
    }
    const uint64_t delta = total - last_total_;
    last_total_ = total;
    if (num_ == den_) {                 // Systemtakt: 1:1
        chip_ticks_ += delta;
    } else {
        acc_ += delta * num_;           // delta ist klein (Befehle/Laufscheibe), kein Ueberlauf
        chip_ticks_ += acc_ / den_;
        acc_ %= den_;
    }
}

bool SoundK1520::takt(int used, uint64_t total) {
    const bool dirty = used > 0 ? ctc_.clockTick(used) : false;
    zeitStand(total);
    if (senke_) ayNachrechnen();
    return dirty;
}

bool SoundK1520::takt(uint64_t total) {
    uint64_t rest = total >= last_total_ ? total - last_total_ : 0;
    bool dirty = false;
    if (total < last_total_) { zeitStand(total); return false; }
    while (rest) {
        const int teil = int(std::min<uint64_t>(rest, 1u << 20));
        rest -= teil;
        dirty |= takt(teil, total - rest);
    }
    return dirty;
}

void SoundK1520::setzeSenke(audio::TonSenke* senke) {
    if (senke == senke_) return;
    senke_ = senke;
    if (senke_) {
        // ab jetzt Abtastwerte; Chip zuerst auf den Stand bringen, damit nichts Altes ausgegeben wird
        ay_.rechneBis(chip_ticks_);
        ay_.ausgabeEin(audio::kAbtastrate, 8000.0f);   // Vorfilter gegen Spiegeltoene (Entwurf 51 Nr. 2)
    } else {
        ay_.ausgabeAus();
    }
}

void SoundK1520::ayNachrechnen() {
    ay_.rechneBis(chip_ticks_);
    for (;;) {
        const size_t n = ay_.abholbereit();
        if (!n) break;
        const size_t teil = std::min<size_t>(n, 4096);
        static thread_local std::vector<Ay38910::Abtastwert> w;
        w.resize(teil);
        ay_.abholen(w.data(), teil);
        mischen(w.data(), teil);
    }
}

void SoundK1520::mischen(const Ay38910::Abtastwert* w, size_t n) {
    puffer_.resize(n * 2);
    const float k = MITTE_GEWICHT;
    const float skala = cfg_.kanal == Kanal::Mono ? 1.0f / 3.0f : 1.0f / (1.0f + k);   // drei volle Kanaele: kein Uebersteuern
    for (size_t i = 0; i < n; ++i) {
        float l, r;
        switch (cfg_.kanal) {
            case Kanal::ABC: l = w[i].a + k * w[i].b; r = w[i].c + k * w[i].b; break;
            case Kanal::ACB: l = w[i].a + k * w[i].c; r = w[i].b + k * w[i].c; break;
            default:         l = r = w[i].a + w[i].b + w[i].c; break;
        }
        const float x[2] = {l * skala, r * skala};
        for (int s = 0; s < 2; ++s) {
            float v = x[s];
            if (cfg_.tiefpass) { tp_[s] += tp_koef_ * (v - tp_[s]); v = tp_[s]; }
            const float y = v - x1_[s] + hp_koef_ * y1_[s];   // Gleichanteil weg
            x1_[s] = v;
            y1_[s] = y;
            puffer_[2 * i + s] = y;
        }
    }
    if (senke_) senke_->schreibe(puffer_.data(), n);
}

}  // namespace k1520
