/**
 * @file wav_schreiber.h
 * @brief WAV-Datei schreiben: 16 Bit PCM, Stereo, 48 000 Hz.
 *
 * Der 44-Byte-Kopf wird beim Oeffnen mit Laenge 0 geschrieben und alle ~1 s Material
 * sowie beim Schliessen/Destruktor mit den echten Laengen berichtigt; ein Abbruch
 * hinterlaesst also hoechstens die letzte Sekunde ungezaehlt, nie eine unlesbare Datei.
 * Nicht fadensicher — der Aufrufer (TonAusgabe) serialisiert.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace k1520::audio {

class WavSchreiber {
public:
    WavSchreiber() = default;
    ~WavSchreiber() { schliessen(); }
    WavSchreiber(const WavSchreiber&) = delete;
    WavSchreiber& operator=(const WavSchreiber&) = delete;

    /// Datei anlegen (ueberschreibt).  Bei Fehler false und Text in `fehler` (falls != nullptr).
    bool oeffnen(const std::string& pfad, std::string* fehler = nullptr);

    /// Float-Frames (-1..+1, interleaved L/R) anhaengen; uebersteuerte Werte werden begrenzt.
    void schreibe(const float* lr, std::size_t frames);

    /// Kopf berichtigen und schliessen (mehrfach aufrufbar).
    void schliessen();

    bool offen() const { return f_ != nullptr; }
    uint64_t frames() const { return frames_; }

    /// Float -> int16 mit Begrenzung (auch fuer Tests).
    static int16_t zuS16(float x);

private:
    void kopfSchreiben();

    std::FILE* f_ = nullptr;
    uint64_t frames_ = 0;
    uint64_t seitKopf_ = 0;
};

}  // namespace k1520::audio
