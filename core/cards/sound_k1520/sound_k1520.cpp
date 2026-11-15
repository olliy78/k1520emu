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
    taktBruch(cfg);
    hp_koef_ = static_cast<float>(std::exp(-2.0 * PI * HOCHPASS_HZ / audio::kAbtastrate));
    tp_koef_ = static_cast<float>(1.0 - std::exp(-2.0 * PI * TIEFPASS_HZ / audio::kAbtastrate));

    kaskadeSetzen(cfg.kaskade);
}

void SoundK1520::taktBruch(const Config& c) {
    // Chiptakte je Maschinentakt = ay_hz / (systemtakt * f); f = tempo_milli_/1000 (Bruch, ohne Drift)
    uint64_t ay_num, ay_den;
    switch (c.takt) {
        case Takt::System:     ay_num = c.systemtakt_hz; ay_den = 1; break;
        case Takt::Quarz:      ay_num = c.quarz_hz;      ay_den = 1; break;
        default:               ay_num = c.quarz_hz;      ay_den = 2; break;
    }
    num_ = ay_num * 1000ull;
    den_ = ay_den * uint64_t(c.systemtakt_hz) * tempo_milli_;
}

void SoundK1520::vorNachrechnen() {
    // Mit den ALTEN Einstellungen bis zum Stand nachrechnen (Abtastwerte mit altem Mischer abliefern)
    if (senke_) ayNachrechnen(); else ay_.rechneBis(chip_ticks_);
}

void SoundK1520::setzeTempo(double f) {
    const uint32_t m = f > 0.0 ? uint32_t(std::min(std::max(f * 1000.0 + 0.5, 1.0), 1.0e6)) : 1000u;
    if (m == tempo_milli_) return;
    vorNachrechnen();
    tempo_milli_ = m;
    taktBruch(cfg_);
    acc_ = 0;
}

void SoundK1520::kaskadeSetzen(uint8_t maske) {
    // Kaskaden TO0->TR1 usw.; Maske nur fuer verdrahtete Kanaele (jeder maskierte Kanal ist teurer)
    const uint8_t k = maske & 7;
    if (k) {
        ctc_.setZCTOCallback([this, k](int kanal, bool pegel) {
            if (kanal >= 0 && kanal < 3 && ((k >> kanal) & 1)) ctc_.clkTrg(kanal + 1, pegel);
        }, k);
    } else {
        ctc_.setZCTOCallback(nullptr, 0);
    }
}

bool SoundK1520::umstellen(const Config& neu, std::string* grund) {
    auto fehler = [&](const char* t) { if (grund) *grund = t; return false; };
    if (neu.basis != cfg_.basis) return fehler("SOUND: Basis laesst sich nur durch Neuaufbau aendern");
    if (neu.systemtakt_hz != cfg_.systemtakt_hz) return fehler("SOUND: Systemtakt laesst sich nicht umstellen");
    if (neu.quarz_hz == 0) return fehler("SOUND: Quarz 0");
    vorNachrechnen();
    cfg_.takt = neu.takt;
    cfg_.quarz_hz = neu.quarz_hz;
    cfg_.chip = neu.chip;
    cfg_.kanal = neu.kanal;
    cfg_.tiefpass = neu.tiefpass;
    cfg_.kaskade = neu.kaskade & 7;
    taktBruch(cfg_);
    acc_ = 0;   // Bruchrest gehoerte zum alten Teiler (< 1 Chiptakt)
    ay_.setzeTakt(neu.takt == Takt::System ? neu.systemtakt_hz
                  : neu.takt == Takt::Quarz ? neu.quarz_hz : neu.quarz_hz / 2);
    ay_.setzeVariante(neu.chip);
    kaskadeSetzen(cfg_.kaskade);
    return true;
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
    if (num_ == den_) {                 // 1:1
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

namespace {
/// Feste Aussteuerung nach Mischung und Filtern (Anwenderwunsch: kein Clipping).  Herleitung: die Mischung liegt
/// nach der Skalierung in [0,1]; der Gleichanteilfilter y = x - x1 + c*y1 (0 < c < 1) liefert fuer x in [0,1]
/// |y| <= 1 (schlimmster Fall: Sprung von 0 auf 1 nach langer Stille); Tiefpass (einpolig) und Kastenfilter
/// ueberschwingen nicht.  Damit ist die Spitze <= 0,7071 = -3 dBFS mathematisch garantiert, fuer Stereo wie Mono.
constexpr float AUSSTEUERUNG = 0.7071f;
}  // namespace

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
            // Reserve + letzte Sicherung: nie ueber [-1,1] hinaus (ein Ueberlauf waere ein Knall)
            const float z = y * AUSSTEUERUNG;
            puffer_[2 * i + s] = z > 1.0f ? 1.0f : (z < -1.0f ? -1.0f : z);
        }
    }
    if (senke_) senke_->schreibe(puffer_.data(), n);
}

}  // namespace k1520
