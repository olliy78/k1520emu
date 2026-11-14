/**
 * @file ringpuffer.h
 * @brief Sperrfreier Ringpuffer fuer Stereo-Frames, ein Erzeuger / ein Verbraucher (SPSC).
 *
 * Erzeuger = Emulationsfaden (`schreibe`), Verbraucher = Ausgabefaden (`lies`).
 * Nie blockierend.  Ueberlauf: es wird so viel geschrieben wie hineinpasst, der Rest
 * (das NEUE) wird verworfen und in `verworfen()` gezaehlt — so bleibt das Aeltere
 * erhalten und der Erzeuger wird nie gebremst.
 *
 * Positionen laufen als 64-Bit-Zaehler monoton (kein Verwechseln von voll/leer);
 * die Kapazitaet ist eine Zweierpotenz.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace k1520::audio {

class Ringpuffer {
public:
    /// @param mindestFrames Kapazitaet in Frames; wird auf die naechste Zweierpotenz aufgerundet.
    explicit Ringpuffer(std::size_t mindestFrames = 16384) {
        std::size_t n = 2;
        while (n < mindestFrames) n <<= 1;
        kap_ = n;
        puffer_.assign(n * 2, 0.0f);
    }

    std::size_t kapazitaet() const { return kap_; }

    /// Frames, die gerade gelesen werden koennen (von beiden Seiten aufrufbar, Momentaufnahme).
    std::size_t fuellstand() const {
        return static_cast<std::size_t>(schreibPos_.load(std::memory_order_acquire) -
                                        lesePos_.load(std::memory_order_acquire));
    }

    /// Erzeuger: schreibt bis zu `frames`; Rueckgabe = tatsaechlich geschrieben.
    std::size_t schreibe(const float* lr, std::size_t frames) {
        const uint64_t w = schreibPos_.load(std::memory_order_relaxed);
        const uint64_t r = lesePos_.load(std::memory_order_acquire);
        const std::size_t frei = kap_ - static_cast<std::size_t>(w - r);
        std::size_t n = frames < frei ? frames : frei;
        if (n < frames) verworfen_.fetch_add(frames - n, std::memory_order_relaxed);
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t p = static_cast<std::size_t>((w + i) & (kap_ - 1)) * 2;
            puffer_[p] = lr[2 * i];
            puffer_[p + 1] = lr[2 * i + 1];
        }
        schreibPos_.store(w + n, std::memory_order_release);
        return n;
    }

    /// Verbraucher: liest bis zu `frames`; Rueckgabe = tatsaechlich gelesen (Unterlauf = weniger).
    std::size_t lies(float* lr, std::size_t frames) {
        const uint64_t r = lesePos_.load(std::memory_order_relaxed);
        const uint64_t w = schreibPos_.load(std::memory_order_acquire);
        const std::size_t da = static_cast<std::size_t>(w - r);
        std::size_t n = frames < da ? frames : da;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t p = static_cast<std::size_t>((r + i) & (kap_ - 1)) * 2;
            lr[2 * i] = puffer_[p];
            lr[2 * i + 1] = puffer_[p + 1];
        }
        lesePos_.store(r + n, std::memory_order_release);
        return n;
    }

    /// Verbraucher: verwirft alles Vorhandene.
    void leeren() {
        lesePos_.store(schreibPos_.load(std::memory_order_acquire), std::memory_order_release);
    }

    /// Summe der wegen Ueberlaufs verworfenen Frames.
    uint64_t verworfen() const { return verworfen_.load(std::memory_order_relaxed); }

private:
    std::size_t kap_ = 0;
    std::vector<float> puffer_;
    std::atomic<uint64_t> schreibPos_{0};
    std::atomic<uint64_t> lesePos_{0};
    std::atomic<uint64_t> verworfen_{0};
};

}  // namespace k1520::audio
