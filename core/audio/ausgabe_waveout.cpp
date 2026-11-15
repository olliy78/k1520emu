// waveOut-Backend (Windows, winmm).  Vier Puffer zu je einem Block, Abfrage statt
// Rueckruf (kein Faden-/Sperrenaerger); der Aufruf blockiert, bis ein Puffer frei ist.

#if defined(_WIN32)

#include <windows.h>
#include <mmsystem.h>

#include <cstring>

#include "core/audio/ausgabe_backend.h"
#include "core/audio/tonquelle.h"

namespace k1520::audio {
namespace {

constexpr int kPuffer = 4;
constexpr std::size_t kMaxFrames = 4096;

class WaveOut : public AusgabeBackend {
public:
    ~WaveOut() override { schliessen(); }

    bool oeffnen(std::string& fehler) override {
        WAVEFORMATEX f{};
        f.wFormatTag = WAVE_FORMAT_PCM;
        f.nChannels = 2;
        f.nSamplesPerSec = kAbtastrate;
        f.wBitsPerSample = 16;
        f.nBlockAlign = 4;
        f.nAvgBytesPerSec = kAbtastrate * 4;
        const MMRESULT r = waveOutOpen(&hwo_, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL);
        if (r != MMSYSERR_NOERROR) {
            hwo_ = nullptr;
            char t[MAXERRORLENGTH] = {0};
            waveOutGetErrorTextA(r, t, sizeof t);
            fehler = std::string("Tonausgabe: kein Audiogeraet (") + t + ")";
            return false;
        }
        for (int i = 0; i < kPuffer; ++i) {
            std::memset(&hdr_[i], 0, sizeof hdr_[i]);
            hdr_[i].lpData = daten_[i];
            hdr_[i].dwBufferLength = 0;
            waveOutPrepareHeader(hwo_, &hdr_[i], sizeof hdr_[i]);
            hdr_[i].dwFlags |= WHDR_DONE;  // gilt als frei
        }
        naechster_ = 0;
        return true;
    }

    void schreibe(const int16_t* lr, std::size_t frames) override {
        if (!hwo_) return;
        if (frames > kMaxFrames) frames = kMaxFrames;
        WAVEHDR& h = hdr_[naechster_];
        while (!(h.dwFlags & WHDR_DONE)) Sleep(1);
        std::memcpy(h.lpData, lr, frames * 4);
        h.dwBufferLength = static_cast<DWORD>(frames * 4);
        h.dwFlags &= ~static_cast<DWORD>(WHDR_DONE);
        waveOutWrite(hwo_, &h, sizeof h);
        naechster_ = (naechster_ + 1) % kPuffer;
    }

    void schliessen() override {
        if (!hwo_) return;
        waveOutReset(hwo_);
        for (int i = 0; i < kPuffer; ++i) waveOutUnprepareHeader(hwo_, &hdr_[i], sizeof hdr_[i]);
        waveOutClose(hwo_);
        hwo_ = nullptr;
    }

    bool hoerbar() const override { return true; }

private:
    HWAVEOUT hwo_ = nullptr;
    WAVEHDR hdr_[kPuffer];
    char daten_[kPuffer][kMaxFrames * 4];
    int naechster_ = 0;
};

}  // namespace

std::unique_ptr<AusgabeBackend> neuesBackendWaveOut() { return std::make_unique<WaveOut>(); }

}  // namespace k1520::audio

#endif
