/**
 * @file tonquelle.h
 * @brief Schnittstelle zwischen einer Tonquelle der Maschine und der Tonstrecke.
 *
 * Entscheidung (Entwurf 51 §4.1 Nr. 3): die Quelle SCHREIBT (Push), die Strecke holt
 * nicht.  Grund: die Karte rechnet ohnehin im Emulationsfaden in Maschinenzeit und
 * kennt ihre fertigen Abtastwerte genau dort; ein Abholen aus dem Ausgabefaden
 * brauchte die Kernsperre oder eine zweite Pufferung.  So schreibt der Emulationsfaden
 * in `TonSenke::schreibe`, und alles Weitere (Ringpuffer, WAV, Ausgabefaden) hinter
 * dieser Schnittstelle ist Sache der Tonstrecke.  `schreibe` blockiert nie.
 *
 * Format: Stereo, interleaved (L, R, L, R ...), float im Bereich -1..+1,
 * 48 000 Hz MASCHINENZEIT (nicht Wanduhr).
 */

#pragma once

#include <cstddef>

namespace k1520::audio {

/// Abtastrate der gesamten Tonstrecke.
constexpr unsigned kAbtastrate = 48000;

/// Senke fuer Stereo-Frames; wird vom Emulationsfaden gerufen.
class TonSenke {
public:
    virtual ~TonSenke() = default;

    /// @param lr     `frames` Paare L,R (also 2*frames floats), -1..+1
    /// @param frames Anzahl Stereo-Frames
    /// Blockiert nie, wirft nicht.
    virtual void schreibe(const float* lr, std::size_t frames) = 0;
};

}  // namespace k1520::audio
