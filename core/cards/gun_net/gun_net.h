/**
 * @file gun_net.h
 * @brief KCNet-Netzkarte (GUN-Netzteil / K1520-Netzkarte): Z80-PIO + Verhaltensmodell der ATmega162-Firmware
 *        vor einem W5100, Entwurf 50 Abschnitt 6 (AP N2), Protokoll rekonstruiert in `doc/design/50a_kcnet_protokoll.md`.
 *
 * Vier E/A-Tore ab `basis` (GUN fest 88H; K1520-Netzkarte per Jumper C0H oder 28H; hier frei, Vielfache von 4):
 * @code
 *   +0  Daten A    Betriebsart 2 (bidirektional): Kommando-/Datenbytes zur Karte, Antwortbytes zum Gast
 *   +1  Daten B    Bitbetrieb, nur Eingaenge:  PB0 = "letztes Byte noch nicht uebernommen" (1 = ja),
 *                                              PB7 = "Antwortbyte da"
 *   +2  Steuer A
 *   +3  Steuer B
 * @endcode
 * (A0 waehlt Port A/B, A1 Daten/Steuer — die Z80-PIO selbst zaehlt 0=DA 1=SA 2=DB 3=SB; die Karte vertauscht.)
 *
 * Byteprotokoll (50a Abschnitt 4): Kommandobyte + feste Parameter (16 Bit Low zuerst), Antwort sofort danach.
 *   00 Block Gast -> W5100 ab Zeiger (nach 02)   01 Block W5100 -> Gast   02 Zeiger setzen   03 Timer (ms, 0..59999)
 *   04 Byte schreiben   05 Byte lesen   06/07 Server-IP setzen/lesen (Index 0 = DNS)   08 dynamischer Port
 *   09 Firmware-Version   0A Hardware-Version   0B Link   0C Kennung   0D Zaehler ungueltiger Kommandos
 * W5100-Adressen: das Gast-Bit 15 wird vom W5100-Modell maskiert (8000H|adr -> adr; Tx C000H.. -> 4000H.., Rx E000H.. -> 6000H..).
 *
 * Handshake-Modell: die Karte verarbeitet ein Byte sofort beim `OUT`; PB0 steht danach genau EINE Statusabfrage lang auf 1
 * (die Schleife `IN (C1); RRCA; JR C` laeuft einmal durch).  Antwortbytes legt die Karte erst bei der Statusabfrage (Lesen von
 * Daten B) ins Eingaberegister von Port A (BSTB-Impuls) und setzt PB7; das Lesen von Daten A nimmt PB7 sofort wieder weg.
 * Ohne Antwort ist PB7 = 0 ("Karte da", der Erkennungstest des Gastes).  Schreibt der Gast ein Betriebsartwort an Steuer A
 * (Neuinitialisierung der PIO), wirft die Karte angefangene Kommandos und unabgeholte Antworten weg [Modellentscheidung:
 * der echte ATmega kann das nicht sehen, so ueberlebt aber ein abgebrochenes Programm den naechsten Start].
 *
 * Zeit: Kommando 03 zaehlt in MASCHINENZEIT (`takt(absolute Takte)`), nicht in Wirtszeit.  Das Netz arbeitet im eigenen
 * Faden des W5100-Modells; der Gast blockiert nie.  Die PIO haengt wie die USB-Karte hinten in der Interruptkette
 * (kein INT wird benutzt).  Kein Save-State.
 */
#pragma once
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "core/bus/k1520_bus.h"
#include "core/peripherals/w5100/w5100.h"
#include "core/primitives/z80_pio.h"

namespace k1520 {

class GunNet : public BusDevice {
public:
    /// Bestueckung/Jumper der Karte.
    struct Config {
        uint8_t basis = 0x88;                          ///< erstes Tor; gueltig: Vielfache von 4 (GUN 88H, Netzkarte C0H/28H)
        w5100::W5100::Richtlinie richtlinie;           ///< Ziele des W5100 (Vorgabe: frei, Entscheidung des Anwenders)
        uint8_t firmware_haupt = 1, firmware_neben = 2;   ///< Kommando 09, "H.L"; der Gast verlangt mindestens 1.2
        uint8_t hardware_haupt = 1, hardware_neben = 0;   ///< Kommando 0A
        std::string id_string = "KCNet K1520 emulation";  ///< Kommando 0C (nullterminiert gesendet)
        uint8_t mac[6] = {0x02, 0x4B, 0x43, 0x4E, 0x45, 0x54};   ///< SHAR nach /RESET (die Firmware laedt sie aus dem EEPROM): 02:"KCNET" (lokal verwaltet)
        uint8_t dns_server[4] = {0, 0, 0, 0};          ///< Kommando 07 Index 0 (Vorgabe 0.0.0.0 = keiner)
        bool link_online = true;                       ///< Kommando 0B
        uint32_t takt_hz = 0;                          ///< Maschinentakt fuer Kommando 03; 0 = die Maschine setzt ihren Takt
    };
    static constexpr unsigned ANZAHL_TORE = 4;

    GunNet();
    explicit GunNet(const Config& cfg);    ///< wirft std::invalid_argument bei basis mit gesetzten Bits 0/1
    ~GunNet() override;

    // ---- BusDevice (ABSOLUTE Portnummer) ----
    uint8_t ioRead(uint8_t port) override;
    void ioWrite(uint8_t port, uint8_t d) override;
    const char* deviceName() const override { return "KCNet"; }

    /// registerIO(this, basis, 4); wirft std::runtime_error bei belegtem Tor.
    void attachToBus(K1520Bus& bus);
    void attachToBus(K1520Bus* bus) { attachToBus(*bus); }
    /// Pruefende Fassung fuer installGunNet: "" wenn alle vier Tore frei, sonst Text.
    std::string belegtGrund(const K1520Bus& bus) const;

    // ---- Lebenslauf ----
    /// Systemweiter /RESET: PIO und Automat zurueck, der W5100 wird zurueckgesetzt (Sockets zu); die Richtlinie bleibt.
    void reset();
    void powerOn() { reset(); zaehler_ = Zaehler(); dyn_port_ = 49152; }
    /// Fortschreibung: absolute Maschinentakte (fuer Kommando 03).
    void takt(uint64_t maschinentakte) { takte_ = maschinentakte; }

    // ---- Eigenschaften / Debug ----
    const Config& config() const { return cfg_; }
    void setzeTaktHz(uint32_t hz) { if (hz) cfg_.takt_hz = hz; }
    void setzeLink(bool online) { cfg_.link_online = online; }
    w5100::W5100& w5100() { return *w5100_; }
    const w5100::W5100& w5100() const { return *w5100_; }
    Z80PIO& pio() { return pio_; }
    const Z80PIO& pio() const { return pio_; }
    struct Zaehler {
        uint64_t kommandos = 0;         ///< gueltige Kommandos
        uint64_t fehlerkommandos = 0;   ///< unbekannte Kommandobytes (Kommando 0D)
        uint64_t bytes_zur_karte = 0;   ///< Gast -> Karte
        uint64_t bytes_zum_gast = 0;    ///< Karte -> Gast
    };
    const Zaehler& zaehler() const { return zaehler_; }
    uint8_t letztesKommando() const { return letztes_; }
    uint16_t zeiger() const { return zeiger_; }
    std::size_t antwortenOffen() const { return antwort_.size() + (vorgelegt_ ? 1 : 0); }

private:
    enum class Phase { Bereit, Parameter, DatenSchreiben };
    void verdrahten();
    void pegelAnlegen();
    void verwerfen();                 ///< Automat und Antworten zurueck
    void byteVomGast(uint8_t b);
    void ausfuehren();
    void vorlegen();                  ///< naechstes Antwortbyte ins Eingaberegister von Port A, wenn keins vorliegt
    void antwort16(uint16_t w) { antwort_.push_back(w & 0xFF); antwort_.push_back(w >> 8); }
    uint16_t adr() const { return zeiger_ & 0x7FFF; }

    Config cfg_;
    Z80PIO pio_;
    std::unique_ptr<w5100::W5100> w5100_;
    uint64_t takte_ = 0;
    Phase phase_ = Phase::Bereit;
    uint8_t befehl_ = 0, erwartet_ = 0, par_[5] = {0, 0, 0, 0, 0}, par_n_ = 0;
    uint32_t rest_ = 0;               ///< noch zu schreibende Datenbytes (Kommando 00)
    uint16_t zeiger_ = 0;
    uint8_t letztes_ = 0;
    uint8_t server_ip_[8][4] = {};
    uint16_t dyn_port_ = 49152;
    uint16_t fehler_zaehl_ = 0;       ///< Kommando 0D (16 Bit, laeuft ueber)
    std::deque<uint8_t> antwort_;
    bool vorgelegt_ = false;          ///< ein Antwortbyte liegt im Eingaberegister (PB7 = 1)
    bool pb0_ = false;                ///< "noch nicht uebernommen": bis zur naechsten Statusabfrage
    bool out_neu_ = false;
    uint8_t out_byte_ = 0;
    Zaehler zaehler_;
};

}  // namespace k1520
