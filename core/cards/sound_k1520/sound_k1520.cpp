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
    tiefpassSetzen(cfg.tiefpass_hz);

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

namespace {
/// Feste Aussteuerung nach Mischung und Filtern (Anwenderwunsch: kein Clipping, Spitze <= 0,7071 = -3 dBFS), fuer ALLE
/// Stufen gleich, damit das Umschalten des Tiefpasses keinen Pegelsprung macht.  Herleitung: die Mischung liegt in
/// [0,1]; Tiefpass und Gleichanteilfilter sind linear, mit ihrer Impulsantwort g (Summe 0) gilt |y| <= B =
/// max(Summe g+, Summe g-).  B gemessen (1 s Impulsantwort, Hochpass 15 Hz, 48 kHz): ueberbrueckt 1,000; 3 kHz 1,125;
/// 8 kHz 1,193; 16 kHz 1,423 (Butterworth 4. Ordnung ueberschwingt).  0,495 * 1,423 = 0,7046 <= 0,7071 fuer alle vier
/// Stufen.  Zwischenwerte ueber die C-ABI (bis 21,6 kHz, B bis ~1,72) sichert nur die harte Begrenzung.
constexpr float AUSSTEUERUNG = 0.495f;
}  // namespace

void SoundK1520::tiefpassSetzen(uint32_t hz) {
    // Butterworth 4. Ordnung = zwei Biquads mit Guete Q = 1/(2 sin(pi/8)) und 1/(2 sin(3 pi/8)); bilineare
    // Transformation mit Vorverzerrung K = tan(pi fc / fs).  Ab 0,45 * fs wird ueberbrueckt (bilinear waere nahe
    // Nyquist entartet; 24 kHz = ungefiltert).  Die Biquads laufen auch ueberbrueckt mit (Grenze 0,45 * fs), damit ihr
    // Zustand dem Signal folgt.
    // Umschalten im Lauf: der bisherige Filter (Koeffizienten + Zustand) laeuft UEBERBLEND_WERTE Ausgabewerte weiter
    // und wird linear in den neuen uebergeblendet -- neue Koeffizienten auf einen fremden Zustand gaeben sonst einen
    // hoerbaren Sprung.  Der neue Filter startet mit dem Zustand des alten.
    if (tp_initialisiert_) {
        for (int i = 0; i < 2; ++i) tp_alt_bq_[i] = tp_bq_[i];
        std::copy(&tp_z_[0][0][0], &tp_z_[0][0][0] + 8, &tp_alt_z_[0][0][0]);
        tp_alt_aktiv_ = tp_aktiv_;
        tp_blend_ = UEBERBLEND_WERTE;
    }
    tp_initialisiert_ = true;
    tp_aktiv_ = hz != 0 && double(hz) < 0.45 * audio::kAbtastrate;
    const double fc = tp_aktiv_ ? double(hz) : 0.45 * audio::kAbtastrate;
    const double K = std::tan(PI * fc / audio::kAbtastrate);
    const double qs[2] = {1.0 / (2.0 * std::sin(PI / 8.0)), 1.0 / (2.0 * std::sin(3.0 * PI / 8.0))};
    for (int i = 0; i < 2; ++i) {
        const double n = 1.0 / (1.0 + K / qs[i] + K * K);
        tp_bq_[i].b0 = float(K * K * n);
        tp_bq_[i].b1 = float(2.0 * K * K * n);
        tp_bq_[i].b2 = float(K * K * n);
        tp_bq_[i].a1 = float(2.0 * (K * K - 1.0) * n);
        tp_bq_[i].a2 = float((1.0 - K / qs[i] + K * K) * n);
    }
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
    cfg_.tiefpass_hz = neu.tiefpass_hz;
    tiefpassSetzen(neu.tiefpass_hz);
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
    for (int i = 0; i < 2; ++i) { x1_[i] = y1_[i] = 0; for (auto& a : tp_z_[i]) a[0] = a[1] = 0; }
    tp_blend_ = 0;
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


float SoundK1520::biquadsRechnen(const Biquad* bq, float (*z)[2], float f) {
    for (int q = 0; q < 2; ++q) {                  // Direktform II transponiert
        const float o = bq[q].b0 * f + z[q][0];
        z[q][0] = bq[q].b1 * f - bq[q].a1 * o + z[q][1];
        z[q][1] = bq[q].b2 * f - bq[q].a2 * o;
        f = o;
    }
    return f;
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
            const float roh = x[s];
            const float f = biquadsRechnen(tp_bq_, tp_z_[s], roh);
            float v = tp_aktiv_ ? f : roh;
            if (tp_blend_ > 0) {                                  // Ueberblenden vom alten Filter
                const float fa = biquadsRechnen(tp_alt_bq_, tp_alt_z_[s], roh);
                const float va = tp_alt_aktiv_ ? fa : roh;
                const float a = float(tp_blend_) / float(UEBERBLEND_WERTE);
                v = a * va + (1.0f - a) * v;
            }
            const float y = v - x1_[s] + hp_koef_ * y1_[s];   // Gleichanteil weg
            x1_[s] = v;
            y1_[s] = y;
            // Reserve + letzte Sicherung: nie ueber [-1,1] hinaus (ein Ueberlauf waere ein Knall)
            const float z = y * AUSSTEUERUNG;
            puffer_[2 * i + s] = z > 1.0f ? 1.0f : (z < -1.0f ? -1.0f : z);
        }
        if (tp_blend_ > 0) --tp_blend_;
    }
    if (senke_) senke_->schreibe(puffer_.data(), n);
}

}  // namespace k1520
