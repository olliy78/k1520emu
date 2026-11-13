/**
 * @file sd1157_steuerung.h
 * @brief Steuerung des Seriendruckers robotron SD 1157 (Sloteinheit): U880, ROM, RAM, CTC,
 *        drei PIO, SIO — rekonstruiert aus der Firmware.
 *
 * Quelle fuer alles: `doc/printer/sd1157/hardware.md` (WP0b, Entwurf 49; kein Schaltplan).
 * Stand WP3/WP2b: das Original-ROM laeuft und druckt auf ein Blatt im Kern (@ref Nadeldruckwerk,
 * core/peripherals/druckwerk/: geregelter Wagen, Papierweg, Nadelkopf, Blattmodell).  Der Nadelanschlag
 * ist die **Flanke FGDI (2B.5) 0 -> 1** (hardware.md §15 Nr. 14): dann schlagen die Nadeln an, deren Bit im
 * Latch BEH/BFH 0 ist.  Tasten und Schalter sind eine API (setOnline, setTastePV1/2/Test, setKopf, setLpi6,
 * setPapierDa), Lampen lesbar (lampeOnline, lampeFehler).  **Die SIO geht ueber je einen `SerialAnschluss` nach
 * aussen** (WP4, @ref anschluss: Kanal B = „IFSS", Kanal A = „V.24"; Anschluss an den `SerialHub` der
 * `PrinterMachine`).  Ohne Hub (Karte allein, Unit-Tests) leert eine Senke den Sender.  KEINE Bedienoberflaeche.
 *
 * **Speicher** (hardware.md §2): ROM 0000H–297FH (`1157.rom`, das Abbild, nicht die Quelle),
 * 2980H–2BFFH unbelegt (FFH [?] §14 Nr. 14), RAM 2C00H–2FFFH, ZG1 3000H–33FFH (`1157zg.rom`),
 * **ZG2 3400H–37FFH liest FFH** (sonst hielte die Firmware ZG2 fuer bestueckt), RAM 3800H–3BFFH.
 *
 * **E/A** (§4): 6FH (IN) Schalterblock 3, SIO 7CH–7FH, BCH (IN) 8212-System, BEH/BFH (OUT) Nadeln,
 * CTC DCH–DFH, PIO 3 ECH–EFH, PIO 1 F4H–F7H, PIO 2 F8H–FBH.  PIO und SIO zaehlen
 * A-Daten/B-Daten/A-Steuer/B-Steuer (AB0 = Kanal, AB1 = Steuer), also dieselbe Uebersetzung wie
 * bei der K2521: `((rel & 1) << 1) | (rel >> 1)`.
 *
 * **Interruptkette** (§8): CTC -> PIO 2 -> PIO 1 -> SIO -> PIO 3.  Die Reihenfolge ist
 * **eine Annahme [?]** (hardware.md §8 „nicht belegbar“): die Vektoradressen legen sie nahe,
 * die Firmware verlaesst sich nirgends auf eine Rangfolge (Takt-Int laufen mit gesperrtem Int).
 * PIO 3 steht hinten, weil sie sich die Vektoren FCH/FEH mit SIO A teilt (Alternativbestueckung).
 *
 * **Eingaenge** (§15 Nr. 6, 8): PIO-Eingangsbits liegen als externe Treiber an den Pins
 * (`Z80PIO::setExtern*`), werden nach jedem Steuerwort erneut angelegt (der Baustein uebernimmt
 * sie erst im Modus 3, den die Firmware nach dem Reset einstellt), Ausgangsbits lesen ihr Latch.
 */

#pragma once
#include <cstdint>
#include <vector>
#include "core/bus/k1520_bus.h"
#include "core/cards/printer/drucker_steuerung.h"
#include "core/peripherals/druckwerk/geregelter_wagen.h"
#include "core/peripherals/druckwerk/nadeldruckwerk.h"
#include "core/primitives/z80.h"
#include "core/primitives/z80_ctc.h"
#include "core/primitives/z80_pio.h"
#include "core/primitives/z80_sio.h"
#include "core/serial/anschluss.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class Sd1157Steuerung : public BusDevice, public MemDevice, public InterruptSlave, public DruckerSteuerung {
public:
    /// Taktfrequenz: aus der Baudprogrammierung abgeleitet [F, ?] hardware.md §6/§14 Nr. 1.
    static constexpr uint32_t CPU_HZ = 2'457'600;

    // Speicherkarte, hardware.md §2
    static constexpr uint16_t ROM_ENDE   = 0x2980;   ///< ROM 0000H–297FH
    static constexpr uint16_t RAM1_BASIS = 0x2C00;   ///< RAM 1 KiB (Stapel SP = 2C28H, Vektortabelle 2CE0H)
    static constexpr uint16_t ZG1_BASIS  = 0x3000;   ///< Zeichengenerator 1
    static constexpr uint16_t ZG2_BASIS  = 0x3400;   ///< Zeichengenerator 2, nicht bestueckt (FFH)
    static constexpr uint16_t RAM2_BASIS = 0x3800;   ///< SIO-Eingabepuffer 1 KiB
    static constexpr uint16_t RAM_GROESSE = 0x0400;

    /// DIP-Schalter, Steckbruecken, Tasten-Ruhelage: Aufbau des Geraets zur Bauzeit (§4.1, §5.4).
    struct Config {
        enum class Schnittstelle { Ifss, V24 };
        /// ROM-Abzug: der Original-Abzug (`1157.rom`) leert den SIO-Eingabepuffer nie (Byte 0D34H = 2BH statt 0BH, [?] Bitkipp,
        /// doc/EPROMS/SD1157/README.md); `true` = `1157_repariert.rom` (nur 0D34H = 0BH), Vorgabe.
        bool rom_repariert = true;
        /// ROM-Satz aus einer Datei (Konfigurationsschluessel `romdatei=`, Entwurf 49 §11.2): genau `ROM_ENDE` Bytes ab 0000H.
        /// Leer = das eingebettete Abbild (`rom_repariert`); nicht leer hat Vorrang.  Die Laenge prueft der Aufrufer (C-ABI).
        std::vector<uint8_t> rom_extern;
        Schnittstelle schnittstelle = Schnittstelle::Ifss;   ///< 3A.0: 0 = IFSS (SIO B), 1 = V.24 (SIO A) §5.4
        // PIO 3 A, Schalter des V.24-Betriebs (§5.4); Vorgaben [?]
        uint8_t stopbits_code = 1;       ///< Bit 2/1 = 01 -> 1 Stoppbit
        bool    paritaet_ein = false;    ///< 3A.4
        bool    paritaet_gerade = false; ///< 3A.3
        uint8_t baud_code = 0;           ///< 3A.7..5: 000 = 9600 Bd (§7 Baudtabelle)
        // 6FH, Schalterblock 3 (§4/§7): D1 OPR, D3 HDUPL, D4 EXT/ETX, D5 8-Bit; Vorgabe nur 8-Bit [?]
        uint8_t schalter_6f = 0x20;
        // BCH (§4.1): Ausfuehrung und Formularlaenge — je Typ einstellbar
        bool    lpi6 = true;             ///< BCH.1 Taste 6/8 lpi: 1 = 6 lpi [?] Vorgabe
        bool    einbahn = true;          ///< BCH.2 EBD: 1 = Einbahndrucker (Typ 264)
        bool    breit = false;           ///< BCH.3 BDA: 1 = breit (Rand 220), 0 = schmal (132 Zeichen, Rand 142)
        uint8_t formular_bahn1 = 3;      ///< BCH.5..4: 00 = 4", 01 = 6", 10 = 8", 11 = 12" [?] Vorgabe 12"
        uint8_t formular_bahn2 = 3;      ///< BCH.7..6
        // Ruhelage der Bedienfeldeingaenge (§5.4)
        bool    online = true;           ///< 3B.1 = 0 (Taste ONLINE: Pegel, 0 = online) [?] Vorgabe online
        bool    kopf_dauernd = false;    ///< 3B.4: 1 = Kopf dauernd angeschwenkt
        uint8_t ram_fill = 0x00;         ///< Vorbelegung der RAMs beim Einschalten [?]
        int     wagen_start = 300;       ///< Startlage des Wagens (Takte) [?] §15 Nr. 11
        // Druckwerk: Mechanikparameter (Raster, Nadeln, Papier); Bahnzahl und Blattbreite folgen EBD/BDA.
        NadeldruckParameter       druck;
        GeregelterWagen::Parameter wagen;   ///< rr_ab folgt BDA (1420 schmal / 2200 breit)
        Papierweg::Parameter      papier;

        /// SD 1157/264: 132 Spalten, eine Bahn (§13).
        static Config sd1157_264() { return Config{}; }
    };

    explicit Sd1157Steuerung(K1520Bus& bus);
    Sd1157Steuerung(K1520Bus& bus, const Config& cfg);
    ~Sd1157Steuerung();

    // ─── BusDevice ───────────────────────────────────────────────────────────
    uint8_t     ioRead(uint8_t port) override;
    void        ioWrite(uint8_t port, uint8_t data) override;
    const char* deviceName() const override { return "SD1157 Steuerung"; }

    // ─── MemDevice: ROM, RAM, ZG ─────────────────────────────────────────────
    uint8_t memRead(uint16_t a) override;
    void    memWrite(uint16_t a, uint8_t d) override;

    // ─── InterruptSlave: Kette CTC -> PIO 2 -> PIO 1 -> SIO -> PIO 3 [?] ─────
    void        setIEI(bool iei) override;
    bool        getIEO() const override       { return ieo_; }
    bool        hasInterrupt() const override;
    uint8_t     getVector() const override;
    void        onRETI() override;
    bool        inService() const override;
    const char* intDeviceName() const override;

    // ─── Lebenslauf ──────────────────────────────────────────────────────────
    void attachToBus(K1520Bus& bus) override;
    void reset() override;              ///< /RESET: CPU, CTC, PIO, SIO, Mechanik; RAM bleibt
    void powerOn() override;            ///< RAM vorbelegen, dann reset()
    /// Takte fortschreiben; true = Interruptzustand kann sich geaendert haben.
    bool clockTick(int ticks) override;

    Z80&       cpu() override { return cpu_; }
    const Z80& cpu() const { return cpu_; }
    Z80CTC&    ctc()       { return ctc_; }
    Z80PIO&    pio1()      { return pio1_; }   ///< F4H–F7H: Papiervorschub, Raender
    Z80PIO&    pio2()      { return pio2_; }   ///< F8H–FBH: Wagen, Druck, Lampen
    Z80PIO&    pio3()      { return pio3_; }   ///< ECH–EFH: Bedienfeld, Interface-Schalter
    Z80SIO&    sio()       { return sio_; }
    const Config& config() const { return cfg_; }

    // ─── Eingaben des Bedienfelds / der Mechanik (Pegel, wirken sofort) ──────
    void setOnline(bool online);          ///< Taste ONLINE (3B.1): true = online
    void setTastePV1(bool gedrueckt);     ///< 3B.2
    void setTastePV2(bool gedrueckt);     ///< 3B.3
    void setTasteTest(bool gedrueckt);    ///< 3B.6
    void setPapierDa(bool da);            ///< BCH.0 /PE: true = Papier da (false = Papier entnommen/leer)
    void setLpi6(bool lpi6);              ///< BCH.1; die Firmware liest es NUR beim Anlauf (sy34), wirksam erst nach /RESET
    void setKopf(bool dauernd);           ///< 3B.4: Taste Kopf: true = dauernd angeschwenkt
    /// Schalter am Geraet, die die Firmware liest (merkposten/drucker.md Nr. 19): Formularlaenge je Bahn (BCH.5..4 / .7..6:
    /// 0 = 4", 1 = 6", 2 = 8", 3 = 12"), Schalterblock 6FH, PIO 3 A (Bit 1..7: Stoppbits, Paritaet, Baud; Bit 0 folgt der
    /// Schnittstelle).  Die Firmware liest sie bei Bedarf; wer sicher gehen will, setzt sie vor dem Netz-Ein.
    void setFormular(int bahn, uint8_t code) { (bahn == 2 ? cfg_.formular_bahn2 : cfg_.formular_bahn1) = code & 3; }
    void setSchalter6f(uint8_t b) { cfg_.schalter_6f = b; }
    void setSchalter3a(uint8_t b) {
        cfg_.stopbits_code = (b >> 1) & 3;
        cfg_.paritaet_gerade = (b & 0x08) != 0;
        cfg_.paritaet_ein = (b & 0x10) != 0;
        cfg_.baud_code = (b >> 5) & 7;
        eingaengeSetzen();
    }
    uint8_t schalter3a() const { return pio3a(); }
    bool online() const { return online_; }
    bool lpi6() const { return lpi6_; }
    bool kopf() const { return kopf_dauernd_; }
    bool tastePV1() const { return taste_pv1_; }
    bool tastePV2() const { return taste_pv2_; }
    bool tasteTest() const { return taste_test_; }
    bool papierDa() const { return druck_.papier(0).papierDa(); }

    // ─── Ausgaben, soweit gespeichert (Beobachtung fuer Werkzeuge und Tests) ──
    /// Nadelmuster: Bit k = Nadel k+1 feuert (low-aktiv invertiert), Bit 8 = Nadel 9 (hardware.md §10.4).
    uint16_t nadeln() const {
        return static_cast<uint16_t>((~nad18_ & 0xFF) | (((~nad9_ >> 7) & 1) << 8));
    }
    uint8_t  nad18() const { return nad18_; }
    uint8_t  nad9() const { return nad9_; }
    bool     lampeOnline() const { return (p2a_aus_ & 0x02) != 0; }   ///< 2A.1, 1 = an (§5.2)
    bool     lampeFehler() const { return (p2b_aus_ & 0x08) == 0; }   ///< 2B.3 /TF, 0 = an (§5.2)
    uint8_t  portAusgabe(int pio, int kanal) const;                    ///< zuletzt geschriebenes Datenbyte

    /// Mitschrift: alles, was die SIO gesendet hat (egal, ob Wandler oder Senke es abgenommen hat): 0 = Kanal A (V.24), 1 = B (IFSS).
    const std::string& sioAusgabe(int kanal) const { return sio_aus_[kanal & 1]; }

    // ─── Serielle Schnittstellen nach aussen (WP4, Entwurf 19, 49 §4.4) ──────────
    /// Anschluss je Kanal (lebt so lange wie die Karte); Hub-Reihenfolge der Maschine: IFSS (B), V.24 (A).
    k1520::serial::SerialAnschluss& anschluss(Config::Schnittstelle s);
    /// Ein `SerialHub` nimmt den Sender ab: die Senke (Karte allein) schweigt.
    void setzeHubAktiv(bool aktiv) override { senke_ = !aktiv; }
    /// Ein Anschluss hat SIO-Zustand geaendert (Zeichen abgenommen/zugestellt, Leitungen) — Maschine: Interrupt neu bewerten.
    bool nimmSeriellGeaendert() override { const bool r = seriell_geaendert_; seriell_geaendert_ = false; return r; }
    /// Alter Unterbau (`k1520_serial_set_rx_cb`/`_send`): Abnehmer der vom Gast gesendeten Bytes / Byte von aussen einspeisen.
    using Abnehmer = std::function<void(uint8_t)>;
    void setAbnehmer(Config::Schnittstelle s, Abnehmer cb);
    void einspeisen(Config::Schnittstelle s, uint8_t byte);
    /// Maschinentakte je ZC/TO-Impuls der CTC 0 als BAUDGENERATOR (hardware.md §7: Vorteiler 16 x TC aus der Baudtabelle);
    /// die Druck-Zeitbasis (TC = 23, `m1a0d`) und der Stopp (13H) aendern sie nicht.  0 = noch nicht programmiert.
    uint64_t baudTakte() const;

    /// Druckwerk (Wagen, Papier, Blatt) — Lesezugriff fuer Oberflaeche, Werkzeuge und Tests.
    Nadeldruckwerk&       druck() override       { return druck_; }
    const Nadeldruckwerk& druck() const override { return druck_; }
    InterruptSlave& unterbrechung() override { return *this; }
    k1520::serial::SerialAnschluss& serialAnschluss(bool v24) override {
        return anschluss(v24 ? Config::Schnittstelle::V24 : Config::Schnittstelle::Ifss);
    }
    void serialAbnehmer(bool v24, std::function<void(uint8_t)> cb) override {
        setAbnehmer(v24 ? Config::Schnittstelle::V24 : Config::Schnittstelle::Ifss, std::move(cb));
    }
    void serialEinspeisen(bool v24, uint8_t byte) override {
        einspeisen(v24 ? Config::Schnittstelle::V24 : Config::Schnittstelle::Ifss, byte);
    }
    bool kopfAngeschwenkt() const { return kopf_dauernd_ || (p2b_aus_ & 0x04) != 0; }   ///< KAS (2B.2) oder Taste

private:
    static uint8_t chipPort(uint8_t rel) {
        return static_cast<uint8_t>(((rel & 1) << 1) | ((rel >> 1) & 1));
    }
    uint8_t bch() const;
    uint8_t pio3a() const;
    uint8_t pio3b() const;
    void eingaengeSetzen();            ///< alle Pins an die PIOs anlegen
    /// Pegel der Eingangspins aus dem Druckwerk, wie die Firmware sie liest.
    struct Pegel {
        bool lauf = false, takt_vor = false, takt_rueck = false;   ///< aktiv high
        bool vk = true, lr = true, rr = true;                     ///< 0 = aktiv
        bool pv1 = true, pv2 = true;                              ///< PV-Schritt (0 = Impuls)
        bool operator==(const Pegel& o) const {
            return lauf == o.lauf && takt_vor == o.takt_vor && takt_rueck == o.takt_rueck && vk == o.vk &&
                   lr == o.lr && rr == o.rr && pv1 == o.pv1 && pv2 == o.pv2;
        }
    };
    Pegel pegelNeu() const;
    void mechanikAusgaenge();
    void mechanikPegel();
    void ketteNeu();                   ///< interne IEI/IEO-Kette (CTC -> PIO2 -> PIO1 -> SIO -> PIO3)
    bool sioSenke();
    class Anschluss;   // sd1157_steuerung.cpp
    friend class Anschluss;

    K1520Bus& bus_;
    Config    cfg_;
    Z80       cpu_;
    Z80CTC    ctc_{"SD1157-CTC"};
    Z80PIO    pio1_{"SD1157-PIO1"};
    Z80PIO    pio2_{"SD1157-PIO2"};
    Z80PIO    pio3_{"SD1157-PIO3"};
    Z80SIO    sio_{"SD1157-SIO"};
    std::array<uint8_t, RAM_GROESSE> ram1_{};
    std::array<uint8_t, RAM_GROESSE> ram2_{};

    // Tasten/Schalter (Laufzeit)
    bool online_, taste_pv1_ = false, taste_pv2_ = false, taste_test_ = false;
    bool lpi6_, kopf_dauernd_;

    // zuletzt geschriebene Ausgangsbytes; Startwerte nach hardware.md §5.1 (P2AD=10H, P2BD=18H, P1xD=08H)
    uint8_t p1a_aus_ = 0x08, p1b_aus_ = 0x08, p2a_aus_ = 0x10, p2b_aus_ = 0x18;
    uint8_t nad18_ = 0xFF, nad9_ = 0xFF;   ///< FFH = Nadeln aus (§10.4)

    Nadeldruckwerk druck_;
    Pegel pegel_;   ///< zuletzt an die PIOs gelegt

    // Interruptkette
    bool iei_in_ = false;
    bool ieo_ = true;
    std::array<InterruptSlave*, 5> kette_;
    static constexpr std::array<const char*, 5> KETTE_NAMEN = {
        "SD1157 CTC", "SD1157 PIO2", "SD1157 PIO1", "SD1157 SIO", "SD1157 PIO3"};

    std::array<std::string, 2> sio_aus_;
    bool senke_ = true;                 ///< ohne Hub leert sioSenke() den Sender
    bool seriell_geaendert_ = false;
    mutable uint64_t baud_takte_ = 0;   ///< zuletzt programmierte Baud-Periode der CTC 0 (baudTakte)
    std::array<std::unique_ptr<Anschluss>, 2> anschluesse_;   ///< [Config::Schnittstelle]
};
