/**
 * @file gide.h
 * @brief GIDE: IDE-Karte (ATA-Platte + RTC-Hook) am K1520-Bus, Entwurf 50 Abschnitt 3 (AP G2).
 *
 * Belegt die E/A-Tore basis+4 .. basis+F (GAL2: A4-A7 gegen die Jumper `basis`, /IORQ, A3 oder A2):
 * @code
 *   +4  exp      ohne Wirkung (Lesen FFh, Schreiben verschluckt)
 *   +5  rtc      RTC 72421, Registernummer = A8..A11 (Hook, Wirkung in AP G3)
 *   +6  cs1      Alternate Status (lesen) / Device Control (schreiben)
 *   +7  cs1      Drive Address
 *   +8  cs0      16-Bit-Datenregister ueber das Wechsel-Flipflop LH (siehe unten)
 *   +9..+F cs0   Task-File: Error/Features, SectorCount, SectorNumber, CylLow, CylHigh, SDH, Status/Command
 * @endcode
 *
 * LH (Registerausgang im GAL1): jeder Zugriff auf +8 und +9..+F kippt LH am ZUGRIFFSENDE, ein Zugriff auf
 * +9..+F setzt ihn dabei auf 0 (statt zu kippen).  Der Zugriff selbst sieht also den alten Wert.
 * +4..+7 aendern LH nicht.  Lesen +8: LH=0 holt das ganze Wort von der Platte und liefert das Low-Byte,
 * LH=1 liefert das gemerkte High-Byte (kein IORD).  Schreiben +8: LH=0 merkt das Low-Byte, LH=1 schreibt
 * das Wort (Low aus dem Merker, High = Busbyte).  Kein Interrupt, kein /WAIT.
 */
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "core/bus/k1520_bus.h"
#include "core/cards/p8000/rtc.h"
#include "core/peripherals/ata/ata_geraet.h"

namespace k1520 {

class Gide : public BusDevice {
public:
    /// Bestueckung/Jumper der Karte (keine Laufzeitumschaltung).
    struct Config {
        uint8_t basis = 0x90;        ///< Hochnibble A4-A7 (Jumper ja4..ja7); gueltig: Vielfache von 10h
        bool rtc_vorhanden = true;   ///< RTC-Chip bestueckt (Port +5 spricht sonst nicht an)
    };
    static constexpr unsigned ANZAHL_TORE = 12;   ///< basis+4 .. basis+F

    Gide();
    explicit Gide(const Config& cfg);     ///< wirft std::invalid_argument bei basis mit gesetztem Tiefnibble

    // ---- BusDevice (ABSOLUTE Portnummer) ----
    uint8_t ioRead(uint8_t port) override;
    void ioWrite(uint8_t port, uint8_t d) override;
    const char* deviceName() const override { return "GIDE"; }

    /// registerIO(this, basis+4, 12); wirft std::runtime_error bei belegtem Tor.
    void attachToBus(K1520Bus& bus);
    void attachToBus(K1520Bus* bus) { attachToBus(*bus); }
    /// Pruefende Fassung fuer installGide: "" wenn alle zwoelf Tore frei, sonst Text ("E/A-Tor 94H belegt (RAF)").
    std::string belegtGrund(const K1520Bus& bus) const;

    // ---- Lebenslauf ----
    void reset();      ///< systemweiter /RESET: LH=0, Platte reset(); Medium bleibt
    void powerOn() { reset(); letzter_befehl_ = 0; zaehler_ = Zaehler(); }
    /// Maschinenzeit (absolute Takte) an die Platte weiterreichen.
    /// Die RTC zaehlt dieselben Maschinentakte (Zeit laeuft in Maschinenzeit, Entwurf 50 Abschnitt 4).
    void takt(uint64_t maschinentakte);

    // ---- RTC 72421 (AP G3): Chipmodell = P8000Rtc72421 (Registerschnittstelle), Zeitpolitik wie P8000-Uhr ----
    struct RtcConfig {
        std::string zustand;                   ///< Batteriedatei (Kopf `K1520-GIDE-RTC 2`), "" = nur im Speicher
        uint32_t takt_hz = 4'000'000;          ///< Maschinentakt, in dem takt() zaehlt
        std::function<int64_t()> zeitquelle;   ///< Unix-Sekunden UTC; leer = std::time
        bool utc = true;                       ///< Wirtszeit als UTC (Oberflaeche speist UTC)
        int jahresversatz = 0;
    };
    /// Chipmodell anlegen und rtcLese/rtcSchreibe darauf verdrahten (nur sinnvoll mit rtc_vorhanden).
    void rtcAnlegen(const RtcConfig& rc);
    P8000Rtc72421* rtc() { return rtc_.get(); }

    // ---- RTC-Hook (AP G3): Registernummer = A8..A11 des Zyklus ----
    std::function<uint8_t(uint8_t reg)> rtcLese;
    std::function<void(uint8_t reg, uint8_t d)> rtcSchreibe;

    // ---- Eigenschaften / Debug (ohne Nebenwirkung) ----
    const Config& config() const { return cfg_; }
    AtaGeraet& platte() { return platte_; }
    const AtaGeraet& platte() const { return platte_; }
    bool lh() const { return lh_; }
    uint8_t letzterBefehl() const { return letzter_befehl_; }   ///< zuletzt nach +F geschrieben (0 = keiner)
    struct Zaehler {
        uint64_t daten_lesen = 0, daten_schreiben = 0;          ///< Zugriffe auf +8
        uint64_t taskfile = 0;                                  ///< Zugriffe auf +9..+F
        uint64_t befehle = 0;                                   ///< Schreibzugriffe auf +F
    };
    const Zaehler& zaehler() const { return zaehler_; }

private:
    Config cfg_;
    AtaGeraet platte_;
    K1520Bus* bus_ = nullptr;
    std::unique_ptr<P8000Rtc72421> rtc_;
    uint64_t rtc_takt_ = 0;
    bool rtc_takt_init_ = false;
    bool lh_ = false;
    uint8_t merker_ = 0;          ///< 646-Register: High-Byte (Lesen) bzw. Low-Byte (Schreiben)
    uint8_t letzter_befehl_ = 0;
    Zaehler zaehler_;
};

}  // namespace k1520
