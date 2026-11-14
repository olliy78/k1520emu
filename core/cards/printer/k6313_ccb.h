/**
 * @file k6313_ccb.h
 * @brief Steuerplatine (CCB) des Hard-Copy-Druckers robotron K6313: U880, 2 CTC (U857), 2 PIO (U855), SIO auf der
 *        V.24-Kassette, Speichermodul "kurz" (ROM 0000H-27FFH, RAM 3800H) — nach Schaltplan und Original-Firmware.
 *
 * Quelle fuer alles: `doc/printer/k6313/hardware.md` (Schaltplan CCB, Firmwareabzug 33538013), `firmware_diff.md`.
 * Marken wie dort: `[S]` Schaltplan, `[F]` aus der Firmware, `[?]` Annahme.  Diese Karte ist **nicht** die K6316-Karte
 * (`K6316Steuerung`): die CCB hat keinen E/A-Dekoder, jeder Baustein haengt mit /CE an einer Adressleitung A2...A7.
 *
 * **Speicher (Fassung "kurz", 10 KiB):** ROM 0000H-27FFH (5 x 2716), RAM 3000H-33FFH und 3800H-3BFFH (siehe RAM_BASIS); sonst Lesen FFH.  Die 16-KiB-Fassung (RAM F000H) gibt es noch nicht.
 *
 * **E/A (jeder Baustein an einer Leitung tief, hardware.md §2.2):** SIO 7CH-7FH (Kassette, Kanal A = Daten), PIO D1 der
 * Centronics-Kassette BCH-BFH (auf der V.24-Kassette nicht vorhanden: Lesen FFH), PIO D105 DCH-DFH (Tastenmatrix, Nadeln
 * 1...8), CTC D102 ECH-EFH (Kanal 2 = Baudtakt), CTC D101 F4H-F7H (Kanal 0 = Wagenschritt-Takt, Kanal 2 = CMT), PIO D104
 * F8H-FBH (Motoren, Sensoren, Nadel 9).  Registerfolge je PIO/SIO: A-Daten, B-Daten, A-Steuer, B-Steuer; CTC: Kanal 0...3.
 *
 * **Tastenmatrix** (D105 A): die Zeilen KB01...KB04 lesen sich an A0...A3, die Spalte waehlen Bit 7...5 des Ausgabebytes
 * (3 Bit, CD4028; Waehlcode = `(Ausgabe >> 5) & 7`).  Taste/DIL-Schalter geschlossen = Zeile 1, wenn ihre Spalte gewaehlt ist.
 * Die Zuordnung (Waehlcode, Zeile) -> Schalter ist **aus der Firmware abgelesen** (Einlesen 0041H-005BH, Auswertung 0613H-06CAH,
 * 042AH-0494H), nicht aus einem Plan: sie ist in `DIL_MATRIX` mit den Befunden je Zelle dokumentiert; die Zellen mit [?] sind
 * Annahmen aus dem Muster "je Spalte zwei Paare, zuerst -2 dann -1".
 *
 * **Druckwerk:** Wagenmotor /CM1...4 (D104 A1,3,5,7, Hochpegel = Phase bestromt [F: Phasenmuster 3824H rotiert um 2]), Formularmotor PFA1...4
 * (D104 B1,3,5,7), Nadeln PP1...8 (D105 B7...0, Tiefpegel = Nadel), PP9 (D104 B0), erst nach der Einrichtung der Tore.  Das Phasenregister
 * D115 (DL175, Takt = CMT = D101 ZC/TO2) ist im Zaehlbetrieb von Kanal 2 getaktet nachgebildet (der Wagen schaltet mit der CMT-Flanke), sonst
 * transparent.  **Zeitbasis (WP8d, [S:4]):** CTC D102 ZC/TO0 = HCP speist C/TRG1 und C/TRG2 von D101 (Draht 57); die Firmware zaehlt damit
 * Schrittimpulse (Kanal 2: 9 HCP je Wagenschritt) und Spaltenimpulse (Kanal 1: ISR 1F10H, 3 HCP je Spalte).  C/TRG0 und C/TRG3 sind im
 * Plan nur Pruefpunkte (169, 170), Kanal 3 und 0 laufen als reine Zeitgeber.  D104 A0 (Pin 15, Zeichnung unklar neben /CM1) liest die
 * Firmware nur als Flanke "A0 high" (Wagen steht -> 26-ms-Zeitgeber, 1ACFH/1B30H); es folgt der Taktscheibe [?].  Das Druckbild wird in
 * `tests/support/k6313_orakel.h` Punkt fuer Punkt gegen den Zeichengenerator im ROM geprueft (Merkposten Nr. 33).
 *
 * **Schnittstelle:** zuerst V.24 an der SIO (Kassettenkennung PE1 = D104 B4 liest 0, Centronics-Kassette 1 [F:0096H]).  IFSS und
 * Centronics fehlen noch; fuer den `SerialHub` gibt es trotzdem beide Anschluesse (wie K6316): IFSS (Kanal B) ohne Funktion.
 */

#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "core/bus/k1520_bus.h"
#include "core/cards/printer/drucker_steuerung.h"
#include "core/peripherals/druckwerk/nadeldruckwerk.h"
#include "core/peripherals/druckwerk/schrittwagen.h"
#include "core/primitives/z80.h"
#include "core/primitives/z80_ctc.h"
#include "core/primitives/z80_pio.h"
#include "core/primitives/z80_sio.h"
#include "core/serial/anschluss.h"

class K6313Ccb : public BusDevice, public MemDevice, public InterruptSlave, public DruckerSteuerung {
public:
    /// Taktfrequenz: Quarz 9830,4 kHz durch 4 (Schaltplan CCB), wie K6316.
    static constexpr uint32_t CPU_HZ = 2'457'600;

    static constexpr uint16_t ROM_ENDE    = 0x2800;   ///< ROM 0000H-27FFH (Fassung "kurz")
    /// RAM (WP8d): zwei Fenster zu je 1 KiB, 3000H-33FFH und 3800H-3BFFH.  Die Firmware nutzt 3000H-3011H (Nadelspalten-Ring), 3124H-3323H
    /// (Punktdaten, +100H = zweiter Satz) UND 3800H-3BFFH (Variablen, Stapel) [F]; dazwischen (3400H-37FFH) nichts angefasst - ob dort RAM
    /// sitzt, ist nicht belegt [?] (Lage der Chips im Speichermodul unbekannt), der Emulator laesst es leer (Lesen FFH).
    static constexpr uint16_t RAM_BASIS   = 0x3800;
    static constexpr uint16_t RAM_GROESSE = 0x0400;
    static constexpr uint16_t RAM2_BASIS  = 0x3000;   ///< zweites Fenster (Punktdaten)

    /// DIL-Schalter: Bit i = Schalter `(5 + i/2)-(1 + i%2)` (5-1 = 0, 5-2 = 1, 6-1 = 2 ... 18-2 = 27), 1 = ON.
    static constexpr int DIL_ANZAHL = 28;
    static constexpr int dilIndex(int paar, int n) { return (paar - 5) * 2 + (n - 1); }

    struct Config {
        /// ROM-Satz aus einer Datei (`romdatei=`): genau `ROM_ENDE` Bytes.  Leer = eingebettetes 33538013.
        std::vector<uint8_t> rom_extern;
        /// DIL-Schalter (siehe @ref dilIndex).  Vorgabe: 5-2 ON und 13-2 ON — ohne 5-2 verlaesst die Firmware beim Einschalten den
        /// Normalpfad (Lesen 018CH: Spalte 3, Zeilen 2/3 muessen 1/0 sein, [F]); 13-2 ON = EPSON-Steuercodes (`ESC`), Beispiel des
        /// Anwenders `K6313_DIPswitches.png`.  Rest OFF (Pica, US ASCII, 12 Zoll, 9600 Bd) — kein Auslieferungszustand, nur Vorgabe.
        uint32_t dil = (1u << dilIndex(5, 2)) | (1u << dilIndex(13, 2));
        bool papier_da = true;
        /// K6311 (WP8e): dieselbe Karte, aber die Hardware ist **nur aus dem ROM 12128062 erschlossen [?]** (kein Plan).  Wirkt: BCH/BDH liefern
        /// `schalter_baud`/`schalter_format` (Eingangs-PIO der V.24-Platte, Quelle der Bits unbekannt [?]), Lampen ON LINE/ERR/PE sind nicht belegt (nur POW).
        /// Bedienmatrix, Motorbelegung und Nadelports des K6311 sind NICHT nachgebildet (die K6313-Belegung gilt dort nachweislich nicht).
        bool k6311 = false;
        /// BCH (nur K6311): Bit 5/6 waehlen den Teiler, Bit 7 = 0 verdoppelt ihn; FFH = 9600 Bd [E: ROM 00AFH-00CCH, Quelle der Bits [?]].
        uint8_t schalter_baud = 0xFF;
        /// BDH (nur K6311): Bit 7 = 1: 7, 0: 8 Datenbits; Bit 6 = 1: Paritaet; Bit 5 = 1: ungerade; Bit 3 = 1: 1 Stopbit [E: ROM 00FAH-0161H].
        /// Vorgabe 3FH = 8 Bit, keine Paritaet, 1 Stopbit (Annahme des Emulators [?], kein Auslieferungszustand).
        uint8_t schalter_format = 0x3F;
        uint8_t ram_fill = 0x00;
        int  wagen_start = 400;                    ///< Startlage des Wagens (Halbschritte)
        NadeldruckParameter druck;                 ///< Raster/Nadeln: Platzhalter der K6316-Werte [?], WP8d
        SchrittWagen::Parameter wagen;
        Papierweg::Parameter papier;
        Config() {
            // Raster [F]: die Firmware feuert 3 Spalten je 4 Positionseinheiten (Halbschritte) und druckt Pica mit 12 Spalten je Zeichen
            // (80 Zeichen = 960 Spalten = 1284 Einheiten gemessen) -> 160 Einheiten je Zoll, Spaltenabstand 1/120 Zoll.  Blattraster 480 dpi:
            // 3 Pixel je Einheit, 4 Pixel je Spalte.  Senkrecht 72 Punkte je Zoll [Handbuch 1.2], Papierschritt 1/216 Zoll [Firmware: 1/6 Zoll = 36].
            druck.takte_je_zoll = 160;
            druck.x_je_takt = 3;
            druck.y_je_zoll = 216;
            druck.pv_schritt_y = 1;
            druck.nadel_teilung_y = 3;
            druck.punkt_breite = 4;
            druck.punkt_hoehe = 2;
            druck.bahnen = 1;
            druck.blatt_breite = 2240;   // 14 Zoll
            // Zwei Nadelspalten: die Firmware verzoegert die geraden Nadeln (2, 4, 6, 8) um 4 Spalten gegen die ungeraden (1, 3, 5, 7, 9) -
            // Ringpuffer 3000H, Versatz 4 Eintraege [F: ISR 1F10H, Aufbau 16CCH]; damit die Punkte in einer Spalte landen, stehen die geraden
            // Nadeln 4 Spalten = 16 Pixel links der ungeraden (wie beim K6316) [F: Selbsttest-Druckbild, Orakel k6313_orakel.h]
            for (int k = 1; k < 8; k += 2) druck.nadel_x[k] = -16;
            // Flugzeit/Rotornacheilung: so abgeglichen, dass vor- und rueckwaerts gedruckte Zeilen denselben Ursprung haben (die Firmware druckt die
            // Rueckwaertszeile ohne eigenen Ausgleich; wie beim K6316 sind Flugzeit und Rotornacheilung nicht zu trennen) [?]
            druck.versatz_vor = 1;
            druck.versatz_rueck = 4;
            druck.rand_oben_y = 36;
            // Formularmotor: die Phasenfolge, die der Selbsttest beim Vorwaerts-Zeilenvorschub ausgibt, laeuft auf steigender Phasennummer
            // [?: Richtung nur aus dem Verhalten (Test druckt Zeile um Zeile nach unten), kein Plan] -> umgekehrt = Papier vorwaerts
            papier.phasen_umgekehrt = true;
        }
    };

    explicit K6313Ccb(K1520Bus& bus);
    K6313Ccb(K1520Bus& bus, const Config& cfg);
    ~K6313Ccb();

    // BusDevice / MemDevice
    uint8_t     ioRead(uint8_t port) override;
    void        ioWrite(uint8_t port, uint8_t data) override;
    const char* deviceName() const override { return cfg_.k6311 ? "K6311 CCB [?]" : "K6313 CCB"; }
    uint8_t     memRead(uint16_t a) override;
    void        memWrite(uint16_t a, uint8_t d) override;

    // InterruptSlave: Kette CTC D101 -> CTC D102 -> PIO D104 -> PIO D105 -> SIO (Reihenfolge aus dem Plan gelesen [?])
    void        setIEI(bool iei) override;
    bool        getIEO() const override { return ieo_; }
    bool        hasInterrupt() const override;
    uint8_t     getVector() const override;
    void        onRETI() override;
    bool        inService() const override;
    const char* intDeviceName() const override;

    // DruckerSteuerung
    void attachToBus(K1520Bus& bus) override;
    void powerOn() override;
    void reset() override;
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
    Z80CTC& ctc101() { return ctc101_; }
    Z80CTC& ctc102() { return ctc102_; }
    Z80PIO& pio104() { return pio104_; }   ///< F8H: Motoren, Sensoren, Nadel 9
    Z80PIO& pio105() { return pio105_; }   ///< DCH: Matrix, Nadeln 1...8, LED ERR
    Z80SIO& sio()    { return sio_; }
    const Config& config() const { return cfg_; }

    // ─── Bedienfeld (Pegel, wirken sofort) ───────────────────────────────────
    /// Tasten 1...4: ON/OFF LINE, LINE FEED vor (LF ↑), LINE FEED zurueck (LF ↓), FORM FEED.  Zuordnung zu den Zeilen der
    /// Spalte 1 aus der Firmware abgelesen (hardware.md §5) — die Beschriftung der Tasten auf den Zeilen ist [?].
    void setTaste(int nr, bool gedrueckt);
    bool taste(int nr) const { return nr >= 1 && nr <= 4 && taste_[nr - 1]; }
    void setPapierDa(bool da);
    bool papierDa() const { return !pe_; }
    void setDil(int paar, int n, bool ein);
    bool dil(int paar, int n) const { return paar >= 5 && paar <= 18 && n >= 1 && n <= 2 && ((dil_ >> dilIndex(paar, n)) & 1); }
    uint32_t dilMaske() const { return dil_; }

    // ─── Ausgaben (Beobachtung) ──────────────────────────────────────────────
    /// Lampen: 1 ON LINE, 2 POW, 3 ERR, 4 PE (1 = leuchtet).  ON LINE = D104 A6 high, ERR = D105 A4 high [?] (Polaritaet
    /// nicht aus dem Plan lesbar; wie K6316 positiv angenommen), POW = Netz, PE = Papierende (/LPE = NAND(PE, PEX)).
    bool lampe(int nr) const;
    uint16_t nadeln() const { return nadeln_aktiv_; }
    uint8_t  portAusgabe(int pio, int kanal) const;   ///< Pin-Pegel: pio = 104/105, kanal 0 = A, 1 = B
    /// Waehlcode der Tastenmatrix (Bit 7...5 des zuletzt nach DCH geschriebenen Bytes).
    int  matrixSpalte() const { return (pio105_.pinsA(0) >> 5) & 7; }
    const std::string& sioAusgabe(int kanal) const { return sio_aus_[kanal & 1]; }
    uint64_t baudTakte() const;

    /// Matrix: Schalter/Taste je (Waehlcode, Zeile).  >= 0: DIL-Index; -1 .. -4: Taste 1...4 (-nr); 0x7F: nicht belegt.
    /// Ablesung und Sicherheit je Zelle: siehe k6313_ccb.cpp.
    static int matrixZelle(int spalte, int zeile);

private:
    class Anschluss;
    friend class Anschluss;
    static uint8_t chipPort(uint8_t rel) { return static_cast<uint8_t>(((rel & 1) << 1) | ((rel >> 1) & 1)); }
    void eingaengeSetzen();
    uint8_t zeilen() const;        ///< Pegel der Zeilen KB01...KB04 bei der gewaehlten Spalte
    void ausgaengeAuswerten();
    bool cmtZaehlt() const;      ///< D101 Kanal 2 im Zaehlbetrieb (Phasenregister D115 getaktet)
    void phasenAnlegen();
    void cmtBeobachten();
    void ketteNeu();
    bool sioSenke();

    K1520Bus& bus_;
    Config    cfg_;
    Z80       cpu_;
    Z80CTC    ctc101_{"K6313-CTC-D101"};
    Z80CTC    ctc102_{"K6313-CTC-D102"};
    Z80PIO    pio104_{"K6313-PIO-D104"};
    Z80PIO    pio105_{"K6313-PIO-D105"};
    Z80SIO    sio_{"K6313-SIO"};
    std::array<uint8_t, 2 * RAM_GROESSE> ram_{};   ///< [0, 1K) = 3800H-Fenster, [1K, 2K) = 3000H-Fenster

    std::array<bool, 4> taste_{};
    bool pe_;
    uint32_t dil_;

    Nadeldruckwerk druck_;
    uint16_t nadeln_aktiv_ = 0;
    uint8_t cm_wunsch_ = 0;          ///< /CM1...4 der Firmware (Bit k = Phase k+1), Eingang von D115
    bool cmt_beobachtet_ = false;
    bool g1_ = false;
    bool eingang_neu_ = false;

    bool iei_in_ = false;
    bool ieo_ = true;
    std::array<InterruptSlave*, 5> kette_;
    static constexpr std::array<const char*, 5> KETTE_NAMEN = {"K6313 CTC D101", "K6313 CTC D102", "K6313 PIO D104", "K6313 PIO D105", "K6313 SIO"};

    std::array<std::string, 2> sio_aus_;
    bool senke_ = true;
    bool seriell_geaendert_ = false;
    std::array<std::unique_ptr<Anschluss>, 2> anschluesse_;   ///< [0] IFSS (Kanal B), [1] V.24 (Kanal A)
};
