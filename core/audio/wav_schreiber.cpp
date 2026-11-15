#include "core/audio/wav_schreiber.h"

#include <cerrno>
#include <cstring>
#include <vector>

#include "core/audio/tonquelle.h"

namespace k1520::audio {

namespace {
void le32(unsigned char* p, uint32_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}
void le16(unsigned char* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
}  // namespace

int16_t WavSchreiber::zuS16(float x) {
    if (!(x == x)) return 0;  // NaN
    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;
    const float s = x * 32767.0f;
    return static_cast<int16_t>(s >= 0 ? s + 0.5f : s - 0.5f);
}

bool WavSchreiber::oeffnen(const std::string& pfad, std::string* fehler) {
    schliessen();
    f_ = std::fopen(pfad.c_str(), "wb");
    if (!f_) {
        if (fehler) *fehler = "WAV-Datei '" + pfad + "' nicht anlegbar: " + std::strerror(errno);
        return false;
    }
    frames_ = 0;
    seitKopf_ = 0;
    kopfSchreiben();
    return true;
}

void WavSchreiber::kopfSchreiben() {
    unsigned char k[44];
    const uint64_t daten64 = frames_ * 4;
    const uint32_t daten = daten64 > 0xFFFFFFD0ull ? 0xFFFFFFD0u : static_cast<uint32_t>(daten64);
    std::memcpy(k, "RIFF", 4);
    le32(k + 4, 36 + daten);
    std::memcpy(k + 8, "WAVEfmt ", 8);
    le32(k + 16, 16);
    le16(k + 20, 1);                 // PCM
    le16(k + 22, 2);                 // Stereo
    le32(k + 24, kAbtastrate);
    le32(k + 28, kAbtastrate * 4);   // Bytes je Sekunde
    le16(k + 32, 4);                 // Blockausrichtung
    le16(k + 34, 16);                // Bit je Wert
    std::memcpy(k + 36, "data", 4);
    le32(k + 40, daten);
    std::fseek(f_, 0, SEEK_SET);
    std::fwrite(k, 1, sizeof k, f_);
    std::fseek(f_, 0, SEEK_END);
    std::fflush(f_);
    seitKopf_ = 0;
}

void WavSchreiber::schreibe(const float* lr, std::size_t frames) {
    if (!f_ || frames == 0) return;
    std::vector<unsigned char> b(frames * 4);
    for (std::size_t i = 0; i < frames * 2; ++i)
        le16(&b[i * 2], static_cast<uint16_t>(zuS16(lr[i])));
    std::fwrite(b.data(), 1, b.size(), f_);
    frames_ += frames;
    seitKopf_ += frames;
    if (seitKopf_ >= kAbtastrate) kopfSchreiben();
}

void WavSchreiber::schliessen() {
    if (!f_) return;
    kopfSchreiben();
    std::fclose(f_);
    f_ = nullptr;
}

}  // namespace k1520::audio
