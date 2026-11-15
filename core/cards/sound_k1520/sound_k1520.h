/**
 * @file sound_k1520.h
 * @brief SOUND-Karte K1520: AY-3-8910/8912 + eigene Z80-CTC am K1520-Bus (Entwurf 51, AP S3).
 *
 * Tore (8er-Raster ab `basis`, ABSOLUTE Portnummer im Handler):
 * @code
 *   +0  schreiben = AY-Latch (Registeradresse), lesen = AY-Register (AYPBC.INC liest R7 zurueck)
 *   +1  schreiben = AY-Register,                lesen = FFh (Bus offen)
 *   +2,+3  nicht belegt (nicht angemeldet)
 *   +4..+7  CTC Kanal 0..3 (Takt = Systemtakt, in der Interruptkette ueber ctc())
 * @endcode
 *
 * Zeit: Die Maschine meldet nach jedem Befehl die verbrauchten Takte und die absolute Taktzahl
 * (`takt(used, total)`).  Die CTC bekommt `clockTick(used)`, der AY rechnet in seinem Chiptakt
 * (exakte Bruchrechnung mit Rest, ohne Drift).  Ein Schreibzugriff auf den AY rechnet den Chip
 * vorher bis zum zuletzt gemeldeten Takt nach ("lazy catch-up").  GRENZE: die Aufloesung ist die
 * der Laufscheibe der Maschine (das OUT sieht den Stand NACH dem Vorgaengerbefehl, Versatz
 * <= ~23 Takte, ~9 us).  Hat eine Maschine beim ioWrite den aktuellen Zyklus zur Hand, kann S4
 * ihn per `zeitStand()` vorab melden und die Grenze aufheben.
 *
 * Ausgabe: mit Senke fuehrt die Karte AY-Abtastwerte (48 kHz Maschinenzeit) durch den Mischer
 * (ABC/ACB/Mono, Gewichte der Widerstandsnetze 10k/22k), Gleichanteilfilter und optional den
 * Tiefpass C16/C17 und schreibt sie als Stereo in die Senke.  Ohne Senke wird nur der
 * Registerzustand nachgefuehrt.  Nicht im Save-State.
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/audio/tonquelle.h"
#include "core/bus/k1520_bus.h"
#include "core/primitives/ay38910.h"
#include "core/primitives/z80_ctc.h"

namespace k1520 {

class SoundK1520 : public BusDevice {
public:
    enum class Takt { Quarz, QuarzHalbe, System };
    enum class Kanal { ABC, ACB, Mono };

    /// Bestueckung/Jumper der Karte (keine Laufzeitumschaltung).
    struct Config {
        uint8_t basis = 0x38;                              ///< Vielfaches von 8 (Jumper JP1)
        Ay38910::Variante chip = Ay38910::Variante::AY8910;
        Takt takt = Takt::QuarzHalbe;                      ///< JP2: Q1, Q1/2 (empfohlen) oder Systemtakt
        uint32_t quarz_hz = 3579545;                       ///< bestueckter Quarz
        Kanal kanal = Kanal::ACB;                          ///< S8/JP4; Vorgabe ACB nach Hoervergleich (Entwurf 51 7a)
        uint32_t tiefpass_hz = 0;                          ///< C16/C17: Grenzfrequenz des einpoligen Tiefpasses, 0 = aus
        uint8_t kaskade = 7;                               ///< Bit0 TO0->TR1, Bit1 TO1->TR2, Bit2 TO2->TR3 (ab Werk alle gesteckt, SKTEST4 Menue 3)
        uint32_t systemtakt_hz = 2457600;                  ///< Takt der Maschine (CTC-Takt)
    };

    /// Gewicht des Mittelkanals je Seite: 10k (Seite) / 22k (Mitte).
    static constexpr float MITTE_GEWICHT = 10.0f / 22.0f;
    /// Grenzfrequenz des zuschaltbaren Tiefpasses "C16/C17" in der Altbedeutung "an" (C-ABI 1, Python True): 8 kHz
    /// (Anwenderentscheid 2026-11-15); einstellbar ist jede Frequenz ueber Config::tiefpass_hz.
    /// Nach dem Schaltplan V2 laegen 1 nF an etwa 4-7 kOhm (Mischwiderstaende, Lastpoti) bei 23-37 kHz und
    /// waeren kaum hoerbar; der Emulator nimmt bewusst einen hoerbaren Wert (doc/merkposten/sound.md).
    static constexpr uint32_t TIEFPASS_HZ = 8000;
    /// Grenzfrequenz des Gleichanteilfilters (AY liefert nur 0..1).
    static constexpr float HOCHPASS_HZ = 15.0f;

    SoundK1520();
    explicit SoundK1520(const Config& cfg);   ///< wirft std::invalid_argument bei basis kein Vielfaches von 8

    // ---- BusDevice (ABSOLUTE Portnummer) ----
    uint8_t ioRead(uint8_t port) override;
    void ioWrite(uint8_t port, uint8_t d) override;
    const char* deviceName() const override { return "SOUND-K1520"; }

    /// Meldet die Tore +0,+1,+4..+7 an; wirft std::runtime_error bei belegtem Tor.
    void attachToBus(K1520Bus& bus);
    void attachToBus(K1520Bus* bus) { attachToBus(*bus); }
    /// "" wenn alle sechs Tore frei, sonst Text ("E/A-Tor 38H belegt (...)").
    std::string belegtGrund(const K1520Bus& bus) const;

    // ---- Lebenslauf ----
    void reset();       ///< systemweiter /RESET: AY und CTC zurueck (Zeit und Verdrahtung bleiben)
    void powerOn() { reset(); }

    /// Zeit: @p used verbrauchte Takte, @p total absolute Maschinentakte.
    /// @return true, wenn die CTC eine Interruptaenderung melden koennte (Aufrufer: bus.markIntDirty()).
    bool takt(int used, uint64_t total);
    /// Bequemfassung: verbrauchte Takte = Unterschied zum letzten Stand.
    bool takt(uint64_t total);
    /// Nur den Zeitstand fuer taktgenaue AY-Schreibzugriffe nachziehen (kein CTC-Takt).
    void zeitStand(uint64_t total);

    /// Stellt Takt-Art, Quarz, Chip, Kanalzuordnung, Tiefpass und Kaskade IM LAUF um (JP2/S8/Bestueckung), ohne
    /// Bus-Neuanmeldung.  Basis und systemtakt_hz muessen gleich bleiben (sonst false, Grund in @p grund).
    /// Der AY rechnet zuerst mit den ALTEN Werten bis zum Stand nach; Chipzeit bleibt monoton, Filterzustand bleibt.
    bool umstellen(const Config& neu, std::string* grund = nullptr);

    /// Tempo-Faktor der Maschine (1.0 = Echtzeit).  Der AY hat einen eigenen Quarz und laeuft in ECHTER Zeit: bei
    /// Faktor f vergehen je Maschinentakt nur ay_hz/(systemtakt_hz*f) Chiptakte, je Wirtssekunde entstehen also
    /// weiter 48000 Abtastwerte und die Tonhoehe bleibt (die Musik wird nur schneller).  Wie umstellen():
    /// erst mit dem alten Faktor nachrechnen, dann umschalten, Chipzeit monoton.  f <= 0 wird wie 1.0 behandelt.
    void setzeTempo(double f);
    double tempo() const { return tempo_milli_ / 1000.0; }

    // ---- Ausgabe ----
    /// nullptr = keine Senke (nur Register nachfuehren).  Auch zur Laufzeit umschaltbar.
    void setzeSenke(audio::TonSenke* senke);
    audio::TonSenke* senke() const { return senke_; }

    // ---- Zugriff / Debug ----
    Z80CTC& ctc() { return ctc_; }
    const Z80CTC& ctc() const { return ctc_; }
    Ay38910& ay() { return ay_; }
    const Ay38910& ay() const { return ay_; }
    const Config& config() const { return cfg_; }

private:
    void ayNachrechnen();     ///< Chip bis chip_ticks_ rechnen, Abtastwerte abholen, mischen, schreiben
    void mischen(const Ay38910::Abtastwert* w, size_t n);

    void taktBruch(const Config& c);
    void vorNachrechnen();
    uint32_t tempo_milli_ = 1000;   ///< Tempo-Faktor in Tausendstel (Bruch bleibt ganzzahlig, driftfrei)
    void kaskadeSetzen(uint8_t maske);

    Config cfg_;
    Z80CTC ctc_;
    Ay38910 ay_;
    audio::TonSenke* senke_ = nullptr;

    // Zeit: chip_ticks_ = absolute Chiptakte zum Stand last_total_ (Bruch num_/den_ mit Rest acc_)
    uint64_t num_ = 1, den_ = 1;
    uint64_t last_total_ = 0, chip_ticks_ = 0, acc_ = 0;

    // Mischer/Filter
    float hp_koef_ = 0, tp_koef_ = 0;
    void tiefpassSetzen(uint32_t hz);
    float x1_[2] = {0, 0}, y1_[2] = {0, 0}, tp_[2] = {0, 0};
    std::vector<float> puffer_;
};

}  // namespace k1520
