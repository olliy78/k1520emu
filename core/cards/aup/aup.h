/**
 * @file aup.h
 * @brief AUP 062-8531 (Adapter Universelle Ports), Tastaturteil — Registermodell (Entwurf 33 N10c).
 *
 * Quelle: Betriebsdokumentation AUP 062-8531 (11 S.), ausgewertet in
 * doc/a5130_k8924/aup_analyse.md §1.  Die Karte bedient im Schalterterminal K8924 die numerische
 * Terminaltastatur K7633 (Leitung DIO1) und die Kundentastatur (DIO2) mit einem eigenen U880 + 1-K-EPROM.
 * Die AUP-Firmware fehlt; nachgebildet wird die Sicht der ZRE auf Registerebene (Hochebene, wie
 * K7636/K7637 ohne ihre Firmware).
 *
 * Belegt [SRC AUP S. 5-6]:
 *  - E/A 40H-43H (Vorgabe fuer SIOS 1526).  `IN 41H` = Gueltigkeitsregister A5 (je Tastatur
 *    unterschiedlich), `IN 42H` = Tastencoderegister A6 (das Lesen LOESCHT die Gueltigkeit),
 *    `OUT 43H` = Kommandobyte A4.
 *  - KEIN Interrupt fuer die Tastaturen: die ZRE fragt A5 zyklisch ab.
 *  - Kommandobyte: Bit 7 Kennung = 1, Bit 6 Hupe (0 = ein), Bit 5 Fehlerlampe (0 = ein, blinkend),
 *    Bit 4 GET-LED der Kundentastatur (0 = GET aktiv, LED an), Bit 3/2 frei, Bit 1 ALPHA-LED
 *    (0 = an), Bit 0 SEL-LED (0 = an).  FFH = RESET AUP (Synchronisation beider Rechner).
 *  - Codetabellen K7633 ohne/mit Alpha (Positionen 00H-47H), Kundentastatur (38H-47H),
 *    Dauerfunktionstasten 1AH/16H/0EH (core/cards/aup/aup_tabelle.h).
 *  - Taste gilt als gueltig, wenn sie zweimal hintereinander beim gleichen Zaehlerstand erkannt
 *    wurde (Entprellung ueber zwei Matrixdurchlaeufe).
 *
 * ANNAHMEN (unbelegt, Schaltplan/EPROM fehlen — Merkposten a5130_k8924, als Config gefuehrt):
 *  - [A1] Bitlage des Gueltigkeitsregisters: Bit 0 = Terminaltastatur K7633, Bit 1 = Kundentastatur,
 *         alle uebrigen Bits 0, aktiv high (@ref Config::bit_terminal, @ref Config::bit_kunde).
 *  - [A2] `IN 40H` und `IN 43H` liefern FFH (nicht beschrieben); `OUT 40H..42H` wirkungslos.
 *  - [A3] Alpha-Umschaltung: die AUP waehlt die Tabelle "mit Alpha", solange das Kommandobyte die
 *         ALPHA-LED einschaltet (Bit 1 = 0) — @ref Alpha::NachLed.  Fest "ohne"/"mit" waehlbar.
 *  - [A4] Entprellzeit 16 ms (2 Durchlaeufe a 72 Positionen bei ~10 kHz + Synchronpause),
 *         Dauerfunktion: erste Wiederholung nach 500 ms, dann alle 100 ms.
 *  - [A5] Hupe: jedes gueltige Kommando mit Bit 6 = 0 ist EIN Signal (@ref hupe zaehlt).
 *  - [A6] Kommando mit Bit 7 = 0 (Kennung fehlt) wird verworfen.  /RESET wirkt wie FFH.
 *  - [A7] Positionen mit Code 00H liefern nichts (unbelegte Positionen; B07/B12 tragen einen Namen,
 *         aber Code 00H — vielleicht die Alpha-Taste, unbelegt).  Gesendet wird beim Druecken,
 *         nicht beim Loslassen; ein Schluesselschalter (SS1/SS2 = Position 00H/01H, AAH/A9H) sendet
 *         beim Einschalten seinen Code, das Ausschalten sendet nichts.
 *  - [A8] Tastendruecke gehen nicht verloren: sie warten in einer Schlange (hoechstens
 *         @ref kSchlange), bis die ZRE den vorigen Code abgeholt hat.  Am Geraet wartet die AUP-CPU
 *         ebenfalls (Eingang DI3 "A6 noch nicht abgeholt"); ob sie dabei Tasten verliert, ist offen.
 *
 * SONDERSCHNITTSTELLE (N10d, @ref Config::sonder; Beleg doc/a5130_k8924/aup_analyse.md §1.6):
 *  - CTC U857 (A12) auf E/A B0H-B3H, SIO U856 (A13) auf B4H-B7H (Basis @ref Config::basis_sonder),
 *    beide Bausteine unveraendert (Z80CTC/Z80SIO).  Nur SIO-Kanal B geht nach aussen (X2 A16 /CTSB,
 *    A17 RXDB, A18 /DTRB, A19 /RTSB, A20 TXDB, TTL-Pegel, kein Pegelwandler), getaktet von CTC-Kanal 0.
 *    Die Karte haengt in der IEI/IEO-Kette; der Vektor kommt vom SIO (A22-08 -> DIEN).
 *  - Nach aussen als @ref anschluss (`SerialAnschluss` "Sonderschnittstelle", Stecker X2) am SerialHub
 *    der Maschine.  Kanal A der SIO ist nicht herausgefuehrt (kein Anschluss).
 *  - [A9]  Registerordnung wie K8025: B4H = Daten A, B5H = Steuer A, B6H = Daten B, B7H = Steuer B
 *          (A0 = Daten/Steuer, A1 = Kanal; Z80SIO::ioRead(port & 3)); CTC-Kanaele B0H-B3H.  Der Beleg nennt sie nicht.
 *  - [A10] Platz in der Interruptkette: DIREKT HINTER der K8025 (Vorschlag der Analyse); die
 *          Wickelverdrahtung der Kette liegt nicht vor.  Innerhalb der Karte SIO vor CTC.
 *  - [A11] CTC-Interrupts sind im Beleg nicht erwaehnt; sie werden wie bei jeder CTC der Kette
 *          ausgeliefert (CTC-Kanal 0 laeuft ohnehin als Baudratentakt, ohne INT-Freigabe).
 *  - [A12] CTC-Eingang CLK/TRG0..3 unbeschaltet (Zeitgeber vom Systemtakt); /CTSB/DCD-Eingang inaktiv
 *          wie an der IFSS der K8025 (RR0 D5/D3 = 0), RTS/DTR nur als Ausgang gemeldet.
 *  - Takt im Laufweg NUR mit Sonderschnittstelle (@ref clockTick, die Maschine ruft es dann je
 *    Instruktion); ohne bleibt der Tastaturteil ohne Takt und die Maschine bitgleich.
 *
 * Zeit: die Karte liest die Maschinenzeit ueber einen Zeiger (@ref Config::uhr, Taktzyklen) erst
 * beim Zugriff des Gastes — kein Takt im Laufweg.  Ein Tastendruck von aussen erhaelt seinen
 * Zeitstempel beim naechsten Zugriff des Gastes (die Entprellzeit zaehlt ab dort).
 *
 * Kennungen (Vertrag fuer C-ABI/Oberflaeche): Tastatur 0 = K7633 (Terminaltastatur), 1 =
 * Kundentastatur; Position = Matrixposition aus dem Beleg (K7633 00H-47H, Kundentastatur 38H-47H).
 *
 * Fadensicherheit: @ref taste aus jedem Faden (Schlange unter Mutex); alles andere im Rechenfaden
 * bzw. unter der Kernsperre.  Kein Teil des Zwischenstands.
 */

#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#include <memory>
#include <string>

#include "core/bus/k1520_bus.h"
#include "core/primitives/z80_ctc.h"
#include "core/primitives/z80_sio.h"
#include "core/serial/anschluss.h"

class AUP : public BusDevice, public InterruptSlave {
public:
    static constexpr uint8_t kBasis    = 0x40;  ///< Vorgabe fuer SIOS 1526
    static constexpr int     kSchlange = 32;    ///< Tastendruecke, die auf Abholung warten
    static constexpr uint8_t kBasisSonder = 0xB0; ///< Sonderschnittstelle (N10d)

    enum class Tastatur : uint8_t { K7633 = 0, Kunde = 1 };
    /// [A3] Wer zwischen den K7633-Tabellen umschaltet.
    enum class Alpha : uint8_t { NachLed = 0, Ohne = 1, Mit = 2 };

    struct Config {
        uint8_t  basis = kBasis;
        uint32_t takt_hz = 2'457'600;           ///< CPU-Takt der Maschine (Zeitumrechnung)
        const uint64_t* uhr = nullptr;          ///< Maschinenzeit in Takten (nullptr = steht bei 0)
        Alpha    alpha = Alpha::NachLed;        ///< [A3]
        uint8_t  bit_terminal = 0x01;           ///< [A1] Gueltigkeitsbit K7633
        uint8_t  bit_kunde    = 0x02;           ///< [A1] Gueltigkeitsbit Kundentastatur
        uint32_t entprell_us     = 16'000;      ///< [A4]
        uint32_t wiederhol_ab_us = 500'000;     ///< [A4] erste Wiederholung einer Dauerfunktionstaste
        uint32_t wiederhol_us    = 100'000;     ///< [A4] Abstand der Wiederholungen
        bool     sonder = false;                ///< N10d: CTC + SIO auf basis_sonder..+7
        uint8_t  basis_sonder = 0xB0;           ///< CTC B0H-B3H, SIO B4H-B7H
    };

    // Lampenbits von @ref lampen (1 = leuchtet) — Vertrag der C-ABI k1520_aup_lampen.
    static constexpr uint8_t kLampeSel    = 0x01;
    static constexpr uint8_t kLampeAlpha  = 0x02;
    static constexpr uint8_t kLampeGet    = 0x04;
    static constexpr uint8_t kLampeFehler = 0x08;

    AUP();
    explicit AUP(const Config& cfg);

    ~AUP();

    /** @brief Meldet 40H..43H (ab @ref Config::basis) an, mit Sonderschnittstelle auch B0H..B7H. */
    void attachToBus(K1520Bus& bus);
    /** @brief Systemweiter /RESET: wie Kommando FFH [A6]; gehaltene Tasten bleiben gehalten. */
    void reset();

    uint8_t ioRead(uint8_t port) override;
    void    ioWrite(uint8_t port, uint8_t d) override;
    const char* deviceName() const override { return "AUP 062-8531 (Tastaturen)"; }

    // ─── Sonderschnittstelle (N10d) ──────────────────────────────────────────
    bool sonder() const { return cfg_.sonder; }
    /// CTC U857 / SIO U856 (nullptr ohne Sonderschnittstelle).
    Z80CTC* ctc() { return ctc_.get(); }
    Z80SIO* sio() { return sio_.get(); }
    const Z80CTC* ctc() const { return ctc_.get(); }
    const Z80SIO* sio() const { return sio_.get(); }
    /// Anschluss fuer den SerialHub (nullptr ohne Sonderschnittstelle).
    k1520::serial::SerialAnschluss* anschluss();
    /// CTC um @p ticks Systemtakte weiter; true = ZC/TO-Flanke (Kette neu bewerten).
    bool clockTick(int ticks) { return ctc_ && ctc_->clockTick(ticks); }
    /// Ein Anschluss hat SIO-Zustand geaendert — liest und loescht (wie K8025).
    bool nimmSeriellGeaendert() { const bool g = seriell_geaendert_; seriell_geaendert_ = false; return g; }

    // InterruptSlave (nur mit Sonderschnittstelle wirksam; sonst nie anfordernd, IEO = IEI).
    void    setIEI(bool iei) override;
    bool    getIEO() const override;
    bool    hasInterrupt() const override;
    uint8_t getVector() const override;
    void    onRETI() override;
    const char* intDeviceName() const override;

    // ─── Von aussen ──────────────────────────────────────────────────────────
    /**
     * @brief Taste druecken/loesen (fadensicher).
     * @return false bei unbekannter Tastatur/Position (keine Taste dort).
     */
    bool taste(Tastatur t, int pos, bool gedrueckt);
    /** @brief Schluesselschalter @p n (1 = SS1, 2 = SS2) ein/aus = Position 00H/01H der K7633. */
    bool schluessel(int n, bool an) { return (n == 1 || n == 2) && taste(Tastatur::K7633, n - 1, an); }
    /** @brief Stellung eines Schluesselschalters (sofort nach @ref schluessel, fadensicher). */
    bool schluesselAn(int n) const;

    // ─── Abfragen (Oberflaeche/Tests) ────────────────────────────────────────
    uint8_t  kommando() const { return kommando_; }      ///< zuletzt angenommenes Kommandobyte
    uint8_t  lampen() const;                              ///< kLampe*-Bits, 1 = leuchtet
    bool     hupeAn() const { return (kommando_ & 0x40) == 0; }
    uint32_t hupe() const { return hupe_; }               ///< Zahl der Signale seit Anlegen [A5]
    uint32_t verworfen() const { return verworfen_; }     ///< Kommandos ohne Kennung [A6]
    bool     alphaAktiv() const;                          ///< [A3] welche K7633-Tabelle gilt
    const Config& config() const { return cfg_; }

    /** @brief Code der Position laut Tabelle; -1 = keine Taste, 0 = Position ohne Code [A7]. */
    static int code(Tastatur t, int pos, bool alpha);
    /** @brief Positionsname (K7633 "A10", "SS1"; Kunde "0".."9"); "" = keine Taste. */
    static const char* name(Tastatur t, int pos);
    /** @brief Dauerfunktionstaste (wiederholt beim Halten)? */
    static bool dauerfunktion(Tastatur t, int pos);

private:
    struct Ereignis { Tastatur t; uint8_t pos; bool gedrueckt; };
    struct Code { Tastatur t; uint8_t code; uint64_t ab; };

    uint64_t jetzt() const { return cfg_.uhr ? *cfg_.uhr : 0; }
    uint64_t takte(uint32_t us) const { return uint64_t(cfg_.takt_hz) * us / 1'000'000u; }
    /// Ereignisse einarbeiten, Wiederholung, naechsten Code nach A6 — bei jedem Gastzugriff.
    void aktualisieren();
    void kommandoAusfuehren(uint8_t d);
    void allesLoeschen();

    class Sonder;   // SerialAnschluss von SIO-Kanal B (aup.cpp)
    void kette();   ///< IEI durch SIO -> CTC reichen

    Config cfg_;
    std::unique_ptr<Z80CTC> ctc_;
    std::unique_ptr<Z80SIO> sio_;
    std::unique_ptr<Sonder> anschluss_;
    bool iei_ = false;
    bool seriell_geaendert_ = false;

    std::mutex            mutex_;      ///< schuetzt nur eingang_
    std::vector<Ereignis> eingang_;

    std::array<std::array<bool, 0x48>, 2> gehalten_{};
    std::array<std::atomic<bool>, 2> schluessel_{};   ///< SS1/SS2, gesetzt in taste()
    std::deque<Code> wartend_;
    int      wiederhol_pos_ = -1;      ///< gehaltene Dauerfunktionstaste der K7633 (-1 = keine)
    uint64_t wiederhol_ab_  = 0;

    uint8_t  a5_ = 0;                  ///< Gueltigkeit
    uint8_t  a6_ = 0;                  ///< Tastencode (bleibt nach dem Lesen stehen)
    uint8_t  kommando_ = 0xFF;
    uint32_t hupe_ = 0;
    uint32_t verworfen_ = 0;
};
