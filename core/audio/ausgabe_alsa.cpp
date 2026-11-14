// ALSA-Backend (Linux): libasound.so.2 per dlopen, KEINE Bauabhaengigkeit.
// Die wenigen benoetigten Funktionen werden mit eigenen Minimaldeklarationen
// angesprochen (kein alsa/asoundlib.h).  Fehlt die Bibliothek oder das Geraet,
// meldet oeffnen() einen Grund — nie eine Ausnahme (Entwurf 51 §4.1 Nr. 6).

#if !defined(_WIN32)

#include <dlfcn.h>

#include <chrono>
#include <thread>

#include "core/audio/ausgabe_backend.h"
#include "core/audio/tonquelle.h"

namespace k1520::audio {
namespace {

struct snd_pcm_t;  // undurchsichtig

using FnOpen = int (*)(snd_pcm_t**, const char*, int, int);
using FnSetParams = int (*)(snd_pcm_t*, int, int, unsigned, unsigned, int, unsigned);
using FnWritei = long (*)(snd_pcm_t*, const void*, unsigned long);
using FnRecover = int (*)(snd_pcm_t*, int, int);
using FnClose = int (*)(snd_pcm_t*);
using FnStrerror = const char* (*)(int);

constexpr int kStreamPlayback = 0;
constexpr int kFormatS16LE = 2;
constexpr int kAccessRwInterleaved = 3;

class Alsa : public AusgabeBackend {
public:
    explicit Alsa(std::string lib) : lib_(lib.empty() ? "libasound.so.2" : std::move(lib)) {}
    ~Alsa() override { schliessen(); }

    bool oeffnen(std::string& fehler) override {
        dl_ = ::dlopen(lib_.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!dl_) {
            fehler = "Tonausgabe: " + lib_ + " nicht ladbar (kein ALSA)";
            return false;
        }
        open_ = reinterpret_cast<FnOpen>(::dlsym(dl_, "snd_pcm_open"));
        setParams_ = reinterpret_cast<FnSetParams>(::dlsym(dl_, "snd_pcm_set_params"));
        writei_ = reinterpret_cast<FnWritei>(::dlsym(dl_, "snd_pcm_writei"));
        recover_ = reinterpret_cast<FnRecover>(::dlsym(dl_, "snd_pcm_recover"));
        close_ = reinterpret_cast<FnClose>(::dlsym(dl_, "snd_pcm_close"));
        strerror_ = reinterpret_cast<FnStrerror>(::dlsym(dl_, "snd_strerror"));
        if (!open_ || !setParams_ || !writei_ || !recover_ || !close_ || !strerror_) {
            fehler = "Tonausgabe: " + lib_ + " ist keine brauchbare ALSA-Bibliothek";
            freigeben();
            return false;
        }
        int r = open_(&pcm_, "default", kStreamPlayback, 0);
        if (r < 0) {
            fehler = std::string("Tonausgabe: kein Audiogeraet (") + strerror_(r) + ")";
            pcm_ = nullptr;
            freigeben();
            return false;
        }
        // 48 kHz, S16, Stereo, weich umrechnen lassen, 100 ms Gesamtpuffer
        r = setParams_(pcm_, kFormatS16LE, kAccessRwInterleaved, 2, kAbtastrate, 1, 100000);
        if (r < 0) {
            fehler = std::string("Tonausgabe: Geraet nicht einstellbar (") + strerror_(r) + ")";
            close_(pcm_);
            pcm_ = nullptr;
            freigeben();
            return false;
        }
        return true;
    }

    void schreibe(const int16_t* lr, std::size_t frames) override {
        if (!pcm_) return;
        long r = writei_(pcm_, lr, frames);
        if (r < 0) r = recover_(pcm_, static_cast<int>(r), 1);
        if (r < 0) {
            // Geraet dauerhaft gestoert: nicht heiss drehen, im Takt weiterschlafen
            std::this_thread::sleep_for(std::chrono::microseconds(1000000LL * frames / kAbtastrate));
        }
    }

    void schliessen() override {
        if (pcm_ && close_) close_(pcm_);
        pcm_ = nullptr;
        freigeben();
    }

    bool hoerbar() const override { return true; }

private:
    void freigeben() {
        if (dl_) ::dlclose(dl_);
        dl_ = nullptr;
    }

    std::string lib_;
    void* dl_ = nullptr;
    snd_pcm_t* pcm_ = nullptr;
    FnOpen open_ = nullptr;
    FnSetParams setParams_ = nullptr;
    FnWritei writei_ = nullptr;
    FnRecover recover_ = nullptr;
    FnClose close_ = nullptr;
    FnStrerror strerror_ = nullptr;
};

}  // namespace

std::unique_ptr<AusgabeBackend> neuesBackendAlsa(const std::string& bibliothek) {
    return std::make_unique<Alsa>(bibliothek);
}

}  // namespace k1520::audio

#endif
