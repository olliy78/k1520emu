/**
 * @file k6316_steuerung.h
 * @brief Steuerung des Schalterdruckers robotron K6316: Steckeinheit Logik (U880, 5 x 2716, 1 KiB RAM, 2 CTC,
 *        3 PIO) mit Schnittstellenmodul (SIO) — nach Schaltplan und Original-Firmware.
 *
 * Quelle fuer alles: `doc/printer/k6316/hardware.md` (Schaltplanblaetter 03/10/11, Servicehandbuch, Firmware).
 *
 * **Speicher:** ROM 0000H–27FFH (Chips I1…I5 bzw. V1…V5, je 2 KiB), RAM 3800H–3BFFH (2 x 2114); sonst nichts
 * (Lesen FFH).  **E/A:** Decoder D8 (DS8205): PIO D12 80H, CTC D5 84H, CTC D10 88H, PIO D14 8CH, Modul A0H–A3H
 * (frei) und SIO A4H–A7H, PIO D11 A8H.  PIO/SIO zaehlen A-Daten/B-Daten/A-Steuer/B-Steuer.
 *
 * **Schnittstellenmodul** = Firmwaresatz: IFSS (Stromschleife, SIO Kanal B) oder V.24 (SIO Kanal A) — die Saetze
 * gehoeren zum Modul (@ref Config::Schnittstelle).  Beide Anschluesse gibt es fuer den `SerialHub` immer; aktiv ist der
 * des gewaehlten Moduls.
 *
 * **Bedienfeld und Schalter** (hardware.md §5): Tasten S1 (ON/OFF-LINE, /ITON, low-aktiv am PIO), S2 LINE FEED vor
 * (TPVV), S3 LINE FEED rueck (TPVR), S4 Belegauswurf (TBAW), S5 Schneiden (TSCH); die vier Spalten-Tasten legen ihre
 * Spalte der Diodenmatrix auf high.  16 DIL-Schalter S11–S26 liegen als Diodenmatrix zwischen Zeilen AM1–AM4 (D11 B)
 * und denselben Spalten: eine Spalte ist high, wenn ein geschlossener Schalter an einer auf high getriebenen Zeile
 * haengt.  Drehschalter Betriebsart ueber zwei Kontakte (/MELO, /MAUF).  Haube und Druckkopf: ein Meldekontakt MHA
 * (1 = Haube offen oder Kopf abgeschwenkt).  Papierende PE (1 = Ende), Lichtschranken im Vorsteckschacht FSO/FSU
 * (1 = Papier).  Lampen H1–H5.
 *
 * Das Druckwerk: @ref SchrittWagen (Phasen /SW1–/SW4 rein, Taktscheibe /G1 raus), Papiermotor als Phasenmotor
 * (/SF1–/SF4), Vorsteckmotor (/SV1–/SV4), Nadelanschlag: eine Nadel schlaegt an, sobald /DNx = 0 wird und /DTFG = 0
 * (Freigabe) gilt (Schaltplan Blatt 08: Treiber folgt dem Pegel, Strom/Dauer macht die Firmware).
 */

#pragma once
#include <cstdint>
#include <vector>
#include "core/bus/k1520_bus.h"
#include "core/cards/printer/drucker_steuerung.h"
#include "core/peripherals/druckwerk/nadeldruckwerk.h"
#include "core/peripherals/druckwerk/schrittwagen.h"
#include "core/peripherals/druckwerk/vorsteckbeleg.h"
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

class K6316Steuerung : public BusDevice, public MemDevice, public InterruptSlave, public DruckerSteuerung {
public:
    /// Taktfrequenz: Quarz 9830,4 kHz durch 4 (Schaltplan Blatt 03).
    static constexpr uint32_t CPU_HZ = 2'457'600;

    static constexpr uint16_t ROM_ENDE   = 0x2800;   ///< ROM 0000H–27FFH
    static constexpr uint16_t RAM_BASIS  = 0x3800;   ///< RAM 3800H–3BFFH
    static constexpr uint16_t RAM_GROESSE = 0x0400;

    enum class Betriebsart { Aufwickeln, Schneiden, Endlos };   ///< Drehschalter (hardware.md §5)

    /// Aufbau des Geraets zur Bauzeit: Schnittstellenmodul, DIL-Schalter, Stellung der Schalter, Mechanik.
    struct Config {
        enum class Schnittstelle { Ifss, V24 };
        Schnittstelle schnittstelle = Schnittstelle::Ifss;   ///< Modul = Firmwaresatz = SIO-Kanal
        /// ROM-Satz aus einer Datei (`romdatei=`, Entwurf 49 §11.2): die fuenf Chips lueckenlos, genau `ROM_ENDE` Bytes.
        /// Leer = eingebetteter Satz je `schnittstelle`; nicht leer hat Vorrang.
        std::vector<uint8_t> rom_extern;
        /// DIL-Schalter S11–S26: Bit i = S(11+i) geschlossen (Ein).  Auslieferungszustand: alle Aus (Handbuch 5.8.2:
        /// „Aus entspricht Nichtbestueckung des Schalterblocks“).
        uint16_t dil = 0;
        Betriebsart betriebsart = Betriebsart::Endlos;   ///< wird beim Netz-Ein gelesen (Handbuch 5.3)
        bool kopf_angeschwenkt = true;                   ///< Einstellhebel: Kopf angeschwenkt
        bool haube_zu = true;                            ///< vordere Abdeckhaube geschlossen
        bool papier_da = true;                           ///< Endlospapier eingelegt
        uint8_t ram_fill = 0x00;
        int  wagen_start = 400;                          ///< Startlage des Wagens (Halbschritte)
        NadeldruckParameter druck;                       ///< Raster, Nadeln; Bahnzahl 1
        SchrittWagen::Parameter wagen;
        Papierweg::Parameter papier;
        /// Schneideinrichtung: rein mechanisch, kein Signal (Servicehandbuch 4.2.19).  Die Anschlagschraube rechts loest das
        /// Messer aus, wenn der Wagen in seiner maximalen Endstellung steht (die Firmware faehrt in der Betriebsart Schneiden
        /// bis dorthin, am Lauf: 1621 Halbschritte); links setzt der Wagen es zurueck.
        int schnitt_pos = 1600;           ///< ab dieser Wagenlage (Halbschritte) schneidet das Messer
        int schnitt_ruecksetz = 120;      ///< bis zu dieser Lage zurueck ist das Messer wieder gespannt
        /// Abstand der Schneidkante unter der Zeile der Nadel 7 am Druckkopf (Raster-Einheiten).  Die Firmware schneidet am
        /// Lauf SOFORT nach der letzten Zeile (ohne vorher vorzuschieben) und schiebt den Beleg danach aus (225 Schritte); der
        /// kleinste Fussabstand ist laut Handbuch 2.6 Nr. 4 3,5 mm (n = 0) = 30 Einheiten bei 216 je Zoll [H, abgeleitet].
        int schnitt_abstand_y = 30;
        /// Vorsteckbeleg (hardware.md §11): Abstand FSO-FSU, Druckzeile, x-Lage; Annahmen [?] dort.
        Vorsteckbeleg::Parameter beleg;

        /// Raster des K6316 (Servicehandbuch 1.1.3.1, Handbuch 2.1): ein Motorschritt des Wagens = 1/75 Zoll, die Position zaehlt
        /// Halbschritte (1/150 Zoll, 2 Pixel je Einheit = 300 dpi); Papierschritt 1/108 Zoll [H] (Service 1.2.5), Nadelteilung
        /// 1/72 Zoll (Handbuch 2.1 Nr. 6): gemeinsames Raster 1/216 Zoll (Papierschritt 2, Nadelteilung 3).
        Config() {
            druck.takte_je_zoll = 150;
            druck.x_je_takt = 2;
            druck.y_je_zoll = 216;
            druck.pv_schritt_y = 2;
            druck.nadel_teilung_y = 3;
            druck.punkt_breite = 3;     // [?] Punktmass
            druck.punkt_hoehe = 2;      // [?]
            // Nadeln in zwei versetzten Spalten (Schaltplan Blatt 08, Servicehandbuch 1.2.4.2 „Halbspalte“): die geraden Nadeln
            // (N2, N4, N6, N8) sitzen 3 Motorschritte = 4 Drucktakte (12 Pixel) links der ungeraden.  Die Firmware loest sie
            // 12 Pixel Fahrweg spaeter aus (vorwaerts; rueckwaerts umgekehrt), damit das Zeichen senkrecht steht [F, gemessen
            // am Selbsttest, hardware.md §10].
            for (int k = 1; k < 8; k += 2) druck.nadel_x[k] = -12;
            // Vorwaerts- und Rueckwaertszeilen liegen mit dem Wagen-Modell ohne Versatz 4 Pixel auseinander (rueckwaerts links,
            // am Selbsttest gemessen): symmetrisch ausgeglichen, absolut [?] (Flugzeit/Rotorlag nicht trennbar).
            druck.versatz_vor = -2;
            druck.versatz_rueck = 2;
            druck.bahnen = 1;
            druck.blatt_breite = 1700;  // 11,3 Zoll: Papierbreite bis 270 mm (Handbuch 2.3) [?]
            druck.rand_oben_y = 36;     // [?] fuer Rueckzuege ueber die Lage 0 hinaus
        }

        static Config k6316_ifss() { return Config{}; }
        static Config k6316_v24() { Config c; c.schnittstelle = Schnittstelle::V24; return c; }
    };

    explicit K6316Steuerung(K1520Bus& bus);
    K6316Steuerung(K1520Bus& bus, const Config& cfg);
    ~K6316Steuerung();

    // ─── BusDevice / MemDevice ───────────────────────────────────────────────
    uint8_t     ioRead(uint8_t port) override;
    void        ioWrite(uint8_t port, uint8_t data) override;
    const char* deviceName() const override { return "K6316 Steuerung"; }
    uint8_t     memRead(uint16_t a) override;
    void        memWrite(uint16_t a, uint8_t d) override;

    // ─── InterruptSlave: Kette CTC D5 -> CTC D10 -> PIO D12 -> SIO (Modul) ───
    void        setIEI(bool iei) override;
    bool        getIEO() const override       { return ieo_; }
    bool        hasInterrupt() const override;
    uint8_t     getVector() const override;
    void        onRETI() override;
    bool        inService() const override;
    const char* intDeviceName() const override;

    // ─── DruckerSteuerung ────────────────────────────────────────────────────
    void attachToBus(K1520Bus& bus) override;
    void powerOn() override;
    void reset() override;                ///< /RESET: CPU, CTC, PIO, SIO; Mechanik bleibt stehen
    bool clockTick(int ticks) override;
    Z80&       cpu() override { return cpu_; }
    Nadeldruckwerk&       druck() override       { return druck_; }
    const Nadeldruckwerk& druck() const override { return druck_; }
    InterruptSlave& unterbrechung() override { return *this; }
    k1520::serial::SerialAnschluss& serialAnschluss(bool v24) override;
    void setzeHubAktiv(bool aktiv) override { senke_ = !aktiv; }
    bool nimmSeriellGeaendert() override { const bool r = seriell_geaendert_; seriell_geaendert_ = false; return r; }
    void serialAbnehmer(bool v24, std::function<void(uint8_t)> cb) override;
    void serialEinspeisen(bool v24, uint8_t byte) override;

    const Z80& cpu() const { return cpu_; }
    Z80CTC& ctc5()  { return ctc5_; }
    Z80CTC& ctc10() { return ctc10_; }
    Z80PIO& pio11() { return pio11_; }   ///< A8H: Vorsteckmotor, Tasten TSCH/TBAW, DIL, Matrixzeilen
    Z80PIO& pio12() { return pio12_; }   ///< 80H: Papiermotor, Lichtschranken, /ITON, Wagenmotor, /G1, MHA, PE
    Z80PIO& pio14() { return pio14_; }   ///< 8CH: Nadeln, Lampen, TPVV/TPVR, /MELO
    Z80SIO& sio()   { return sio_; }
    const Config& config() const { return cfg_; }

    // ─── Bedienfeld und Schalter (Pegel, wirken sofort) ──────────────────────
    /// Tasten 1..5: S1 ON/OFF-LINE, S2 LINE FEED vor, S3 LINE FEED rueck, S4 Belegauswurf, S5 Schneiden.
    void setTaste(int nr, bool gedrueckt);
    bool taste(int nr) const { return nr >= 1 && nr <= 5 && taste_[nr - 1]; }
    void setBetriebsart(Betriebsart b);
    Betriebsart betriebsart() const { return betriebsart_; }
    void setKopf(bool angeschwenkt);       ///< Einstellhebel
    void setHaube(bool zu);                ///< vordere Abdeckhaube
    bool kopf() const { return kopf_; }
    bool haube() const { return haube_; }
    void setPapierDa(bool da);             ///< Endlospapier (PE)
    bool papierDa() const { return !pe_; }
    void setBeleg(bool oben, bool unten);  ///< Lichtschranken im Vorsteckschacht: Papier vor FSO / FSU
    void setMpv(bool beleg_vorn);          ///< Meldekontakt /MPV (0 = aktiv)
    bool beleg() const { return fso_ || fsu_; }
    bool fso() const { return fso_; }
    bool fsu() const { return fsu_; }
    void setDil(int nr, bool ein);         ///< S11..S26
    bool dil(int nr) const { return nr >= 11 && nr <= 26 && ((dil_ >> (nr - 11)) & 1); }
    uint16_t dilMaske() const { return dil_; }

    // ─── Ausgaben (Beobachtung) ──────────────────────────────────────────────
    /// Lampen H1..H5 (1 = leuchtet): H1 Test, H2 Fehler, H3 Papierende, H4 (LSPAUS), H5 ON-LINE.
    bool lampe(int h) const;
    uint16_t nadeln() const;               ///< Bit k = Nadel k+1 treibt (Pegel low und freigegeben)
    uint8_t  portAusgabe(int pio, int kanal) const;   ///< Pin-Pegel: pio = 11/12/14, kanal 0 = A, 1 = B
    Papierweg& vorsteck() { return vorsteck_; }
    int schnitte() const { return schnitte_; }   ///< bisher ausgeloeste Schnitte

    // ─── Vorsteckbeleg als eigenes Blatt (hardware.md §11) ───────────────────
    /// Beleg von Hand einlegen (Spitze hinter FSO, vor FSU); FSO/FSU folgen ab jetzt seiner Lage, die der Vorsteckmotor
    /// bewegt.  false = es liegt schon einer im Schacht oder das Mass ist unsinnig.
    bool belegEinlegen(double laenge_mm, double breite_mm);
    void belegEntnehmen();                         ///< FSO/FSU frei; das Belegblatt bleibt zum Ansehen
    const Vorsteckbeleg& beleg_modell() const { return beleg_; }
    Vorsteckbeleg& beleg_modell() { return beleg_; }
    /// Das Blatt des Belegs (nur nach dem ersten Einlegen vorhanden: @ref Vorsteckbeleg::hatBlatt).
    const Blattmodell& belegBlatt() const { return beleg_.blatt(); }
    bool magnetUnten() const { return !(pio11_.pinsB(0) & 0x01); }   ///< /YABU
    bool magnetOben() const  { return !(pio11_.pinsB(0) & 0x08); }   ///< /YABO

    /// Mitschrift: alles, was die SIO gesendet hat: 0 = Kanal A (V.24), 1 = B (IFSS).
    const std::string& sioAusgabe(int kanal) const { return sio_aus_[kanal & 1]; }
    /// Maschinentakte je ZC/TO-Impuls von CTC D10 Kanal 2 (Baudtakt), 0 = noch nicht programmiert.
    uint64_t baudTakte() const;

private:
    class Anschluss;
    friend class Anschluss;

    static uint8_t chipPort(uint8_t rel) {
        return static_cast<uint8_t>(((rel & 1) << 1) | ((rel >> 1) & 1));
    }
    void eingaengeSetzen();                ///< alle Pins an die PIOs anlegen
    uint8_t spalten() const;               ///< Pegel der fuenf Spalten (Bit 0 TSCH, 1 TBAW, 2 DIL, 3 TPVV, 4 TPVR)
    void ausgaengeAuswerten();             ///< Motorphasen, Nadeln
    void belegAbgleichen();                ///< Vorsteckmotor hat sich bewegt: Beleg nachfuehren, FSO/FSU setzen
    void ketteNeu();
    bool sioSenke();

    K1520Bus& bus_;
    Config    cfg_;
    Z80       cpu_;
    Z80CTC    ctc5_{"K6316-CTC-D5"};
    Z80CTC    ctc10_{"K6316-CTC-D10"};
    Z80PIO    pio11_{"K6316-PIO-D11"};
    Z80PIO    pio12_{"K6316-PIO-D12"};
    Z80PIO    pio14_{"K6316-PIO-D14"};
    Z80SIO    sio_{"K6316-SIO"};
    std::array<uint8_t, RAM_GROESSE> ram_{};

    // Tasten/Schalter
    std::array<bool, 5> taste_{};
    Betriebsart betriebsart_;
    bool kopf_, haube_, pe_;
    bool fso_ = false, fsu_ = false, mpv_ = true;   ///< mpv_: Pegel /MPV (1 = kein Beleg)
    uint16_t dil_;

    Nadeldruckwerk druck_;
    Papierweg vorsteck_;
    Vorsteckbeleg beleg_;
    int beleg_lage_ = 0;          ///< Lage des Vorsteckmotors beim letzten Abgleich mit dem Beleg
    uint16_t nadeln_aktiv_ = 0;   ///< zuletzt aktive Nadeln (fuer die Flankenerkennung)
    bool g1_ = false;             ///< zuletzt an D12 B Bit 0 gelegter Pegel
    bool messer_bereit_ = true;   ///< Messer gespannt (rechter Anschlag loest aus, linker spannt)
    int  schnitte_ = 0;
    bool eingang_neu_ = false;    ///< Eingangspegel geaendert (Taste/Schalter von aussen): Interruptzustand neu bewerten

    // Interruptkette
    bool iei_in_ = false;
    bool ieo_ = true;
    std::array<InterruptSlave*, 4> kette_;
    static constexpr std::array<const char*, 4> KETTE_NAMEN = {"K6316 CTC D5", "K6316 CTC D10", "K6316 PIO D12", "K6316 SIO"};

    std::array<std::string, 2> sio_aus_;
    bool senke_ = true;
    bool seriell_geaendert_ = false;
    std::array<std::unique_ptr<Anschluss>, 2> anschluesse_;   ///< [0] IFSS (Kanal B), [1] V.24 (Kanal A)
};
