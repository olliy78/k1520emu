#include "core/audio/ausgabe.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "core/audio/ausgabe_backend.h"

namespace k1520::audio {

namespace {
constexpr std::size_t kBlock = 480;  // 10 ms

std::string umgebung(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}
}  // namespace

TonAusgabe::TonAusgabe(const Optionen& opt) : opt_(opt), ring_(opt.ringFrames) {}

TonAusgabe::~TonAusgabe() {
    stop();
    wavStop();
}

bool TonAusgabe::start() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (laeuft_.load()) return true;
    fehler_.clear();
    kein_geraet_ = false;

    std::string wahl = opt_.backend.empty() ? umgebung("K1520_AUDIO") : opt_.backend;
    std::unique_ptr<AusgabeBackend> b;
    if (wahl == "stumm") {
        b = neuesBackendStumm();
    } else {
#if defined(_WIN32)
        b = neuesBackendWaveOut();
#else
        const std::string lib =
            opt_.alsaBibliothek.empty() ? umgebung("K1520_AUDIO_ALSA_LIB") : opt_.alsaBibliothek;
        b = neuesBackendAlsa(lib);
#endif
    }
    std::string grund;
    if (!b->oeffnen(grund)) {
        fehler_ = grund.empty() ? "Tonausgabe nicht verfuegbar" : grund;
        kein_geraet_ = true;
        return false;
    }
    backend_ = std::move(b);
    geraetOk_ = backend_->hoerbar();
    ring_.leeren();
    beenden_ = false;
    laeuft_ = true;
    faden_ = std::thread([this] { faden(); });
    return true;
}

void TonAusgabe::stop() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!laeuft_.load()) return;
    beenden_ = true;
    if (faden_.joinable()) faden_.join();
    backend_->schliessen();
    backend_.reset();
    laeuft_ = false;
    geraetOk_ = false;
    ring_.leeren();
}

void TonAusgabe::lautstaerke(float v) {
    if (!(v >= 0.0f)) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    lautstaerke_.store(v);
}

void TonAusgabe::echtzeit(bool e) { echtzeit_.store(e); }

TonAusgabe::Status TonAusgabe::status() const {
    if (kein_geraet_.load()) return Status::KeinGeraet;
    if (!laeuft_.load()) return Status::Aus;
    if (!geraetOk_.load() || stumm_.load() || !echtzeit_.load()) return Status::Stumm;
    return Status::Hoerbar;
}

std::string TonAusgabe::fehler() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return fehler_;
}

void TonAusgabe::schreibe(const float* lr, std::size_t frames) {
    if (frames == 0) return;
    {
        std::lock_guard<std::mutex> lk(wavMtx_);
        if (wav_.offen()) wav_.schreibe(lr, frames);
    }
    // In den Ringpuffer nur, wenn jemand zuhoeren kann; sonst wuerde er sich fuellen und
    // beim Umschalten auf Echtzeit alten Ton nachspielen (§4.1 Nr. 4).
    if (laeuft_.load(std::memory_order_relaxed) && geraetOk_.load(std::memory_order_relaxed) &&
        echtzeit_.load(std::memory_order_relaxed))
        ring_.schreibe(lr, frames);
}

bool TonAusgabe::wavStart(const std::string& pfad, std::string* fehler) {
    std::lock_guard<std::mutex> lk(wavMtx_);
    return wav_.oeffnen(pfad, fehler);
}

void TonAusgabe::wavStop() {
    std::lock_guard<std::mutex> lk(wavMtx_);
    wav_.schliessen();
}

bool TonAusgabe::wavLaeuft() const {
    std::lock_guard<std::mutex> lk(wavMtx_);
    return wav_.offen();
}

uint64_t TonAusgabe::wavFrames() const {
    std::lock_guard<std::mutex> lk(wavMtx_);
    return wav_.frames();
}

void TonAusgabe::faden() {
    std::vector<float> f(kBlock * 2);
    std::vector<int16_t> s(kBlock * 2);
    while (!beenden_.load()) {
        std::fill(f.begin(), f.end(), 0.0f);
        const bool hoeren = echtzeit_.load() && !stumm_.load();
        if (!echtzeit_.load()) {
            ring_.leeren();  // Wirtsausgabe still, Puffer verwirft (§4.1 Nr. 4)
        } else {
            const int k = opt_.regler.korrektur(ring_.fuellstand());
            if (k < 0) {
                // einen Frame weglesen und verwerfen
                float skip[2];
                ring_.lies(skip, 1);
            }
            std::size_t soll = kBlock;
            if (k > 0) soll = kBlock - 1;
            const std::size_t n = ring_.lies(f.data(), soll);  // Unterlauf: Rest bleibt 0 (Stille)
            if (k > 0 && n == soll && n > 0) {  // letzten Frame verdoppeln
                f[2 * n] = f[2 * (n - 1)];
                f[2 * n + 1] = f[2 * (n - 1) + 1];
            }
        }
        const float v = hoeren ? lautstaerke_.load() : 0.0f;
        for (std::size_t i = 0; i < kBlock * 2; ++i) s[i] = WavSchreiber::zuS16(f[i] * v);
        backend_->schreibe(s.data(), kBlock);
    }
}

}  // namespace k1520::audio
