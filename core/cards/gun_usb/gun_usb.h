/**
 * @file gun_usb.h
 * @brief GUN-USB: Z80-PIO + VDIP1 (Vinculum, Firmware VDAP 3.68) am K1520-Bus, Entwurf 50 Abschnitt 5 (AP U2).
 *
 * Vier E/A-Tore ab `basis` (GUN fest 84H, Jumper: Vielfache von 4; UTools-CP/M-Fassung DCh..DFh):
 * @code
 *   +0  Daten A    AD0..AD7 des VDIP1 (Betriebsart 2, bidirektional)
 *   +1  Daten B    B0 = RXF# (E), B1 = TXE# (E), B2 = RD# (A, aktiv low), B3 = WR (A, Impuls aktiv high),
 *                  B4/B5 = Eingaenge ohne Funktion, B6 = RES# (A), B7 = PRG# (A)
 *   +2  Steuer A
 *   +3  Steuer B
 * @endcode
 * (A0 waehlt Port A/B, A1 Daten/Steuer — die Z80-PIO selbst zaehlt 0=DA 1=SA 2=DB 3=SB; die Karte vertauscht.)
 *
 * Die Gatter des Schaltplans (74LS00) werden NICHT einzeln nachgebaut, sondern als beobachtbares Verhalten:
 *  - RXF#/TXE# werden bei jedem Zugriff aus dem VdipGeraet auf B0/B1 gelegt (Eingabebits des Bit-Ports).
 *  - Fallende Flanke an B2 (RD# aktiv) mit RXF# = 0: das naechste VDIP-Byte steht auf AD0..7 und wird sofort
 *    ins Eingaberegister von Port A uebernommen (BSTB-Impuls), `IN PIOA` liefert es.  Die Z80-PIO selbst laedt
 *    erst bei der STEIGENDEN BSTB-Flanke; die Karte legt den Impuls an den Beginn von RD#, weil der Gast
 *    (USBINC.Z80) Port A VOR dem Deaktivieren von RD# liest.  Ohne Datum (RXF# = 1) bleibt das Register.
 *  - ASTB = NAND(B2,B3): geht low bei B2 = B3 = 1 (Gast: OUT PIOB 0CCh), wieder high bei der naechsten
 *    Aenderung.  Das Byte (Ausgaberegister von Port A) uebernimmt das VDIP1 bei der FALLENDEN Flanke von B3
 *    (WR# des Chips ist invertiert zu B3: Gast `OUT PIOB 0C4h`), vorausgesetzt TXE# = 0.
 *  - B6 (RES#) low -> high: Neustart der Firmware (VdipGeraet::reset, Startmeldung in der Ausgabe).  PRG# (B7)
 *    wird nicht ausgewertet (der Programmiermodus des Chips ist nicht nachgebildet).
 * Kein Zeitmodell (TXE# ist immer bereit), kein Interrupt ausser der Kettendurchschaltung der PIO
 * (die UTools benutzen ihn nicht; die PIO haengt wie K6022/K5020 in der Interruptkette).
 */
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

#include "core/bus/k1520_bus.h"
#include "core/peripherals/vdip/vdip_geraet.h"
#include "core/primitives/z80_pio.h"

namespace k1520 {

class GunUsb : public BusDevice {
public:
    /// Bestueckung/Jumper der Karte (keine Laufzeitumschaltung, ausser Stick stecken/ziehen).
    struct Config {
        uint8_t basis = 0x84;                  ///< erstes Tor (A2..A7 gegen Jumper); gueltig: Vielfache von 4
        std::filesystem::path ordner;          ///< Wirtsverzeichnis des "Sticks"; leer = kein Stick
        bool schreibgeschuetzt = false;
        bool stick_vorhanden = true;           ///< false: ordner wird nicht gesteckt (Stick-Fach leer)
    };
    static constexpr unsigned ANZAHL_TORE = 4;

    GunUsb();
    explicit GunUsb(const Config& cfg);    ///< wirft std::invalid_argument bei basis mit gesetzten Bits 0/1

    // ---- BusDevice (ABSOLUTE Portnummer) ----
    uint8_t ioRead(uint8_t port) override;
    void ioWrite(uint8_t port, uint8_t d) override;
    const char* deviceName() const override { return "GUN-USB"; }

    /// registerIO(this, basis, 4); wirft std::runtime_error bei belegtem Tor.
    void attachToBus(K1520Bus& bus);
    void attachToBus(K1520Bus* bus) { attachToBus(*bus); }
    /// Pruefende Fassung fuer installGunUsb: "" wenn alle vier Tore frei, sonst Text.
    std::string belegtGrund(const K1520Bus& bus) const;

    // ---- Lebenslauf ----
    void reset();      ///< systemweiter /RESET: PIO und VDIP1 zurueck; der Stick bleibt gesteckt
    void powerOn() { reset(); zaehler_ = Zaehler(); }

    // ---- Stick (Laufzeit, ueber die C-ABI) ----
    bool stickEinstecken(const std::filesystem::path& ordner, bool schreibgeschuetzt, std::string* fehler = nullptr);
    void stickZiehen();

    // ---- Eigenschaften / Debug ----
    const Config& config() const { return cfg_; }
    VdipGeraet& vdip() { return vdip_; }
    const VdipGeraet& vdip() const { return vdip_; }
    Z80PIO& pio() { return pio_; }
    const Z80PIO& pio() const { return pio_; }
    struct Zaehler {
        uint64_t gelesen = 0;     ///< Bytes VDIP1 -> Gast
        uint64_t geschrieben = 0; ///< Bytes Gast -> VDIP1
        uint64_t resets = 0;      ///< RES#-Impulse
    };
    const Zaehler& zaehler() const { return zaehler_; }

private:
    void verdrahten();
    void pegelAnlegen();    ///< RXF#/TXE# auf B0/B1
    bool portBKonfiguriert() const;
    void flankenAuswerten(bool ereignisse);///< nach jedem Zugriff: RD#/WR/ASTB/RES# aus den Pegeln von Port B ableiten

    Config cfg_;
    Z80PIO pio_;
    VdipGeraet vdip_;
    uint8_t pb_alt_ = 0xFF;   ///< letzter Pegel an Port B (Flankenerkennung)
    bool astb_low_ = false;
    Zaehler zaehler_;
};

}  // namespace k1520
