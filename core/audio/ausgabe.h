/**
 * @file ausgabe.h
 * @brief Tonstrecke: Ringpuffer + Ausgabefaden + WAV-Mitschrift (Entwurf 51 §4, S2).
 *
 * Der Emulationsfaden ruft `schreibe` (Push, nie blockierend).  Ein Faden im Kern
 * liest aus dem Ringpuffer und gibt an das Wirtsgeraet aus (Linux ALSA per dlopen,
 * Windows waveOut, sonst/auf Wunsch stumm) — ohne Kernsperre, ohne Python.
 *
 * Festlegungen (Entwurf 51 §4.1):
 *  - Nr. 4: bei `echtzeit(false)` ist die Wirtsausgabe still und der Puffer verwirft.
 *    Die WAV-Mitschrift laeuft davon unabhaengig in jedem Tempo mit (sie wird im
 *    Erzeugerpfad geschrieben, deterministisch in Maschinenzeit).
 *  - Nr. 5: Fuellstandsregelung (`FuellstandRegler`); Unterlauf = Stille.
 *  - Nr. 6: ohne Geraet / ohne libasound keine Ausnahme: Zustand `KeinGeraet`, Grund
 *    in `fehler()`.
 *
 * Backend-Wahl: `Optionen::backend`, sonst Umgebungsvariable `K1520_AUDIO`:
 *   "stumm"  -> kein Wirtsgeraet (Tests/CI; der Faden laeuft im Takt der Wanduhr weiter)
 *   "" / "auto" -> Linux ALSA, Windows waveOut
 * ALSA-Bibliothek: `Optionen::alsaBibliothek`, sonst `K1520_AUDIO_ALSA_LIB`, sonst
 * "libasound.so.2" (Testhaken fuer den Fehlerfall).
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "core/audio/fuellstand_regler.h"
#include "core/audio/ringpuffer.h"
#include "core/audio/tonquelle.h"
#include "core/audio/wav_schreiber.h"

namespace k1520::audio {

class AusgabeBackend;

class TonAusgabe : public TonSenke {
public:
    struct Optionen {
        std::string backend;          ///< "", "auto", "stumm"; leer -> K1520_AUDIO
        std::string alsaBibliothek;   ///< leer -> K1520_AUDIO_ALSA_LIB -> "libasound.so.2"
        std::size_t ringFrames = 16384;
        FuellstandRegler regler{};
    };

    enum class Status {
        Aus,         ///< nicht gestartet
        Stumm,       ///< laeuft, aber nichts zu hoeren (Stumm-Backend, stumm(true) oder echtzeit(false))
        Hoerbar,     ///< laeuft und gibt an das Wirtsgeraet aus
        KeinGeraet   ///< Start gescheitert; `fehler()` nennt den Grund
    };

    TonAusgabe() : TonAusgabe(Optionen{}) {}
    explicit TonAusgabe(const Optionen& opt);
    ~TonAusgabe() override;
    TonAusgabe(const TonAusgabe&) = delete;
    TonAusgabe& operator=(const TonAusgabe&) = delete;

    /// Geraet oeffnen und Ausgabefaden starten.  false bei Misserfolg (dann Status
    /// KeinGeraet, `fehler()` gesetzt); wirft nie.  Zweiter Aufruf ohne stop() tut nichts.
    bool start();
    /// Faden synchron beenden und Geraet schliessen.
    void stop();

    /// Erzeugerpfad (Emulationsfaden): WAV mitschreiben, dann in den Ringpuffer (nur wenn hoerbar moeglich).
    void schreibe(const float* lr, std::size_t frames) override;

    void lautstaerke(float v);     ///< 0..1
    float lautstaerke() const { return lautstaerke_.load(); }
    void stumm(bool s) { stumm_.store(s); }
    bool stumm() const { return stumm_.load(); }
    void echtzeit(bool e);
    bool echtzeit() const { return echtzeit_.load(); }

    Status status() const;
    std::string fehler() const;
    uint64_t verworfen() const { return ring_.verworfen(); }
    std::size_t fuellstand() const { return ring_.fuellstand(); }

    /// WAV-Mitschrift (unabhaengig von start() und vom Tempo).  false + `fehler` bei Misserfolg.
    bool wavStart(const std::string& pfad, std::string* fehler = nullptr);
    void wavStop();
    bool wavLaeuft() const;
    uint64_t wavFrames() const;

private:
    void faden();

    Optionen opt_;
    Ringpuffer ring_;
    std::unique_ptr<AusgabeBackend> backend_;
    std::thread faden_;
    std::atomic<bool> laeuft_{false};
    std::atomic<bool> beenden_{false};
    std::atomic<bool> echtzeit_{true};
    std::atomic<bool> stumm_{false};
    std::atomic<float> lautstaerke_{1.0f};
    std::atomic<bool> geraetOk_{false};  ///< Ausgabe hat ein Geraet, das etwas hoerbar macht
    std::atomic<bool> kein_geraet_{false};

    mutable std::mutex mtx_;  ///< schuetzt fehler_ und start/stop
    std::string fehler_;

    mutable std::mutex wavMtx_;
    WavSchreiber wav_;
};

}  // namespace k1520::audio
