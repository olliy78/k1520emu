// Stumm-Backend: kein Wirtsgeraet (Tests, CI, K1520_AUDIO=stumm).
// Es verbraucht die Bloecke im Takt der Wanduhr, damit der Ausgabefaden und die
// Fuellstandsregelung sich genau wie mit einem echten Geraet verhalten.

#include <chrono>
#include <thread>

#include "core/audio/ausgabe_backend.h"
#include "core/audio/tonquelle.h"

namespace k1520::audio {
namespace {

class Stumm : public AusgabeBackend {
public:
    bool oeffnen(std::string&) override {
        naechster_ = std::chrono::steady_clock::now();
        return true;
    }
    void schreibe(const int16_t*, std::size_t frames) override {
        naechster_ += std::chrono::microseconds(1000000LL * static_cast<long long>(frames) / kAbtastrate);
        std::this_thread::sleep_until(naechster_);
    }
    void schliessen() override {}
    bool hoerbar() const override { return false; }

private:
    std::chrono::steady_clock::time_point naechster_;
};

}  // namespace

std::unique_ptr<AusgabeBackend> neuesBackendStumm() { return std::make_unique<Stumm>(); }

}  // namespace k1520::audio
