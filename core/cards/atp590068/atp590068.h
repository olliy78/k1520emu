/**
 * @file atp590068.h
 * @brief ATP 590068 — „Adapter fuer Tastatur und Programmierzusatz“ des PRG 710.
 *
 * Belegt (doc/design/20_prg710.md §3.4, doc/prg710/resident.md §5):
 * - **8279 an C8H (Daten) / C9H (Befehl/Status)** fuer die Tastatur K7609.
 * - **EPROMmer D0H–D4H** (AP-P7b): PIO + Steuerregister + virtueller Sockel,
 *   @ref Eprommer590068 (doc/prg710/eprommer.md).
 * - **ATP-Zusatz** (Entwurf 48, AP-B1, nur wenn gesteckt): SIO an **C0H–C3H** mit fester
 *   Kanalschleife Tx→Rx ("Diagnosebetrieb", Handbuch PRG 710 §1.4) und die Anzeige-Ports
 *   **F4H/F5H** (nur Schreiben, protokolliert).  Die CTC CCH–CFH gehoert der Ass590069 (gleiche
 *   Ports) — die Maschine gibt ihr den Takt an CLK/TRG0/1, siehe Prg710Machine.
 *   Befund: doc/bs600/atp_hardware.md, doc/bs600/selbsttest.md.
 * Das Peripheriemodell (Tasten) haengt die Maschine an @ref kbc.
 */

#pragma once
#include "core/bus/k1520_bus.h"
#include "core/primitives/i8279.h"
#include "core/cards/atp590068/eprommer590068.h"
#include "core/primitives/z80_sio.h"
#include <vector>
#include <cstdint>

class Atp590068 {
public:
    struct Config {
        uint8_t kbc_base = 0xC8;       ///< 8279 (Daten, Befehl/Status)
        uint8_t eprommer_base = 0xD0;  ///< EPROMmer (PIO D0H–D3H, Steuerregister D4H)
        // ATP-Zusatz (nur nach attachZusatz): [Disk] BS620-Selbsttest
        uint8_t sio_base = 0xC0;       ///< SIO "Diagnosebetrieb" (Daten A/B, Steuer A/B)
        uint8_t anzeige_base = 0xF4;   ///< Anzeige F4H/F5H (nur Schreiben)
        bool    schleife_a = true;     ///< Kanal A: Sender mit eigenem Empfaenger verbunden
        bool    schleife_b = true;     ///< Kanal B: dito
        int     bittakt_teiler = 4;    ///< Bittakt der Schleife = phi / teiler (614,4 kHz) [?]
    };

    Atp590068() : Atp590068(Config{}) {}
    explicit Atp590068(const Config& cfg);

    /** @brief Tastatur und EPROMmer am Systembus anmelden. */
    void attachToBus(K1520Bus& bus) { attachTastatur(bus); attachEprommer(bus); }
    /** @brief Nur den 8279 (C8H/C9H) anmelden. */
    void attachTastatur(K1520Bus& bus);
    /** @brief Nur den EPROMmer (D0H–D4H) anmelden. */
    void attachEprommer(K1520Bus& bus) { eprommer_.attachToBus(bus); }
    /** @brief /RESET. */
    void reset() { kbc_.reset(); eprommer_.reset(); resetZusatz(); }

    // ─── ATP-Zusatz (SIO C0H–C3H mit Schleife, Anzeige F4H/F5H) ─────────────
    /** @brief SIO C0H–C3H und Anzeige F4H/F5H am Systembus anmelden (einmal, vor dem Lauf). */
    void attachZusatz(K1520Bus& bus);
    bool zusatz() const { return zusatz_; }
    /** @brief Maschinentakte fortschreiben (Zeichenlaufzeit der Schleife); true = SIO-Zustand geaendert. */
    bool clockTick(int takte);
    Z80SIO& diagSio() { return sio_; }

    /** @brief Ein Schreibzugriff auf F4H/F5H. */
    struct AnzeigeSchreiben { uint8_t port; uint8_t wert; };
    /** @brief Zuletzt geschriebene Werte der Anzeige (Port F4H bzw. F5H); 0 = noch nie. */
    struct Anzeige {
        uint8_t f4 = 0, f5 = 0;
        uint32_t anzahl = 0;                       ///< Zahl aller Schreibzugriffe
        std::vector<AnzeigeSchreiben> protokoll;   ///< die ersten 256 Zugriffe in Reihenfolge
    };
    const Anzeige& anzeige() const { return anzeige_; }

    I8279& kbc() { return kbc_; }
    const I8279& kbc() const { return kbc_; }
    Eprommer590068& eprommer() { return eprommer_; }
    const Config& config() const { return cfg_; }

private:
    struct Tastatur : BusDevice {
        explicit Tastatur(I8279& k) : kbc(k) {}
        uint8_t ioRead(uint8_t port) override { return (port & 1) ? kbc.readStatus() : kbc.readData(); }
        void ioWrite(uint8_t port, uint8_t d) override {
            if (port & 1) kbc.writeCommand(d); else kbc.writeData(d);
        }
        const char* deviceName() const override { return "ATP 590068 8279"; }
        I8279& kbc;
    };

    struct SioPorts : BusDevice {   // C0/C1/C2/C3 → Z80SIO 0=A-D, 1=A-S, 2=B-D, 3=B-S
        explicit SioPorts(Z80SIO& s) : sio(s) {}
        static uint8_t rel(uint8_t p) { return static_cast<uint8_t>(((p & 1) << 1) | ((p >> 1) & 1)); }
        uint8_t ioRead(uint8_t p) override { return sio.ioRead(rel(p & 3)); }
        void ioWrite(uint8_t p, uint8_t d) override { sio.ioWrite(rel(p & 3), d); }
        const char* deviceName() const override { return "ATP 590068 SIO (Diagnose)"; }
        Z80SIO& sio;
    };
    struct AnzeigePorts : BusDevice {
        explicit AnzeigePorts(Atp590068& a) : atp(a) {}
        uint8_t ioRead(uint8_t) override { return 0xFF; }   // am Geraet nie gelesen [Disk]
        void ioWrite(uint8_t p, uint8_t d) override { atp.anzeigeSchreiben(p, d); }
        const char* deviceName() const override { return "ATP 590068 Anzeige F4H/F5H"; }
        Atp590068& atp;
    };
    void resetZusatz();
    void anzeigeSchreiben(uint8_t port, uint8_t wert);
    /// Zeichenlaufzeit eines Kanals in Maschinentakten (aus WR3/4/5, x teiler Bittakt).
    uint64_t zeichenTakte(const Z80SIO::Channel& c) const;

    Config   cfg_;
    I8279    kbc_;
    Tastatur tastatur_{kbc_};
    Eprommer590068 eprommer_;

    // ATP-Zusatz
    bool           zusatz_ = false;
    Z80SIO         sio_{"ATP SIO"};
    SioPorts       sio_ports_{sio_};
    AnzeigePorts   anzeige_ports_{*this};
    Anzeige        anzeige_;
    uint64_t       zeit_ = 0;
    /// Je Kanal: Zeichen unterwegs bis zu diesem Zeitpunkt (0 = keins).
    uint64_t       faellig_[2] = {0, 0};
    uint8_t        unterwegs_[2] = {0, 0};
};
