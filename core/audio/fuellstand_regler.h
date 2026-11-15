/**
 * @file fuellstand_regler.h
 * @brief Gleichlauf zwischen Maschinenzeit und Wanduhr (Entwurf 51 §4.1 Nr. 5).
 *
 * Reine Logik (ohne Faden, ohne Geraet), damit sie einzeln testbar ist.  Der Ausgabefaden
 * fragt je Block den Fuellstand des Ringpuffers: weit ueber dem Ziel -> ein Frame
 * verwerfen, weit darunter -> einen verdoppeln, sonst nichts.  Ein Frame je Block
 * (10 ms) ist <= 0,2 % Tempoabweichung, unhoerbar.
 */

#pragma once

#include <cstddef>

namespace k1520::audio {

class FuellstandRegler {
public:
    /// @param zielFrames   Sollfuellstand (60 ms = 2880 Frames bei 48 kHz)
    /// @param toleranzFrames Abstand, ab dem eingegriffen wird (20 ms = 960)
    constexpr FuellstandRegler(std::size_t zielFrames = 2880, std::size_t toleranzFrames = 960)
        : ziel_(zielFrames), tol_(toleranzFrames) {}

    /// -1 = ein Frame verwerfen, +1 = ein Frame verdoppeln, 0 = nichts.
    constexpr int korrektur(std::size_t fuellstand) const {
        if (fuellstand > ziel_ + tol_) return -1;
        if (fuellstand + tol_ < ziel_) return +1;
        return 0;
    }

    constexpr std::size_t ziel() const { return ziel_; }
    constexpr std::size_t toleranz() const { return tol_; }

private:
    std::size_t ziel_;
    std::size_t tol_;
};

}  // namespace k1520::audio
