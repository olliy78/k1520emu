/**
 * @file k3521.h
 * @brief K3521.30 – 4-KB-CMOS-RAM mit Akkupufferung (Option der A5130/K8924).
 *
 * Quelle: Betriebsdokumentation Speichersteckeinheiten K3525/K3521.30/OSS 062-8471, §14
 * (doc/a5130_k8924/aup_analyse.md §2, Entwurf 33 N10b).
 *
 * Belegt: 4 KByte statisches CMOS-RAM (STE 045-8120); Adresse im 4-KB-Raster ueber S1:01
 * (0000H … F000H); /MEMDI-Quelle waehlbar (Bus-/MEMDI X1:B09, MEMDI1 X2:A21, MEMDI2 X2:B21);
 * Datenerhalt >= 200 h ueber drei Knopfzellen, Unterspannung setzt einen Merker -> /SUE;
 * der Merker faellt nur durch /RESET (oder S1:03-03/04).
 *
 * Modell:
 *  - Inhalt bleibt ueber /RESET UND ueber Netz-Aus/Ein (das IST die Akkupufferung).  Dauerhaft
 *    ueber das Ende des Emulators hinaus wird er, wenn eine Datei gebunden ist (@ref dateiBinden):
 *    Rohdatei, genau 4096 Byte; fehlt sie, entsteht sie beim ersten Zurueckschreiben.
 *    Zurueckgeschrieben wird nach einer Schreibpause (@ref autoFlush, Maschinenzeit, wie bei
 *    Disketten/Stanzer/Kassette), sofort bei @ref flush und im Destruktor.
 *  - [Annahme] Ungeladener Einschaltzustand = 0xFF (wie die dRAM-Karten; das Datenblatt sagt dazu
 *    nichts).
 *  - [Annahme] /SUE ist nur ein ZUSTAND (@ref sue / @ref setzeAkkuLeer).  Welches BS-PIO-Bit der
 *    ZRE ihn auswertet, ist unbelegt (ZRE-Schaltplan fehlt) -> NICHT verdrahtet.  Ein leerer
 *    Akku aendert den Inhalt im Modell nicht; wer ihn verlieren will, ruft @ref fill.
 *  - [Nicht modelliert] 2 WAIT-Zyklen je Zugriff (M1-synchronisiert): die A5120-Laufschleife
 *    wertet K1520Bus::takeWaitCycles() nicht aus; der heisse Pfad bleibt unberuehrt (bitgleich).
 *    Folge: das Modell ist um diese Takte zu schnell, sonst ohne Wirkung.
 *  - Gesperrt (/MEMDI): Lesen 0xFF, Schreiben verworfen; isWritable() = true, damit das Lade-ROM
 *    der K2526 bei 0000H das RAM beim Lesen ueberdecken darf, Schreiben aber durchkommt (wie K3525).
 */

#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include "core/bus/k1520_bus.h"

class K3521 : public MemDevice {
public:
    static constexpr uint32_t kGroesse = 0x1000;   ///< 4 KB
    static constexpr uint32_t kRaster  = 0x1000;   ///< Startadresse im 4-KB-Raster (S1:01)
    /// Schreibpause in Taktzyklen (0,5 s bei 4 MHz Bustakt), bevor die Datei beschrieben wird.
    static constexpr uint64_t kSchreibpause = 2000000;

    /// /MEMDI-Wahl (S1:02): Bus-/MEMDI (X1:B09) oder MEMDI1/MEMDI2 der Rueckverdrahtung.
    enum class Memdi : uint8_t { Bus = 0, Memdi1 = 1, Memdi2 = 2 };

    struct Config {
        uint16_t base_addr = 0x0000;       ///< 4-KB-ausgerichtet
        Memdi    memdi     = Memdi::Bus;
    };

    K3521();
    /** @throws std::invalid_argument bei unausgerichteter Basisadresse. */
    explicit K3521(const Config& cfg);
    ~K3521() override;   ///< schreibt eine gebundene Datei zurueck

    uint8_t memRead(uint16_t addr) override;
    void    memWrite(uint16_t addr, uint8_t data) override;
    bool    isWritable() const override { return true; }

    /** @brief Belegt 4 KB ab Basisadresse auf dem Bus (merkt sich ihn fuer Bus-/MEMDI). */
    void attachToBus(K1520Bus& bus);

    /** @brief /MEMDI1 bzw. /MEMDI2 der Rueckverdrahtung (nur wirksam, wenn dorthin gewaehlt). */
    void setMemDI(bool disabled) { memdi_ = disabled; }
    bool memDI() const { return memdi_; }

    const Config& config() const { return cfg_; }

    // ─── Akku / /SUE (nur Zustand, s. Dateikopf) ─────────────────────────────
    bool sue() const { return sue_; }                ///< Akku leer gemeldet
    void setzeAkkuLeer(bool leer) { sue_ = leer; }
    /** @brief /RESET: nur der /SUE-Merker faellt, der Inhalt bleibt. */
    void reset() { sue_ = false; }

    // ─── Inhalt ──────────────────────────────────────────────────────────────
    void fill(uint8_t value = 0xFF);
    uint8_t peek(uint16_t offset) const { return offset < kGroesse ? mem_[offset] : 0xFF; }
    void    poke(uint16_t offset, uint8_t v);
    const uint8_t* rawPtr() const { return mem_.data(); }

    // ─── Dauerhafte Ablage ───────────────────────────────────────────────────
    /**
     * @brief Datei binden; existiert sie, wird ihr Inhalt geladen (genau 4096 Byte, sonst
     *        false + @p fehler und NICHTS geaendert), sonst entsteht sie beim ersten
     *        Zurueckschreiben.  Eine zuvor gebundene Datei bekommt vorher ihren Stand.
     */
    bool dateiBinden(const std::string& pfad, std::string& fehler);
    const std::string& datei() const { return datei_; }
    /** @brief Sofort zurueckschreiben (wenn gebunden und geaendert); false bei Schreibfehler. */
    bool flush();
    /** @brief Ende jeder run()-Scheibe: nach einer Schreibpause von @ref kSchreibpause Takten
     *         zurueckschreiben.  @p jetzt = Maschinenzeit in Taktzyklen. */
    void autoFlush(uint64_t jetzt);
    const std::string& dateiFehler() const { return datei_fehler_; }

private:
    bool inRange(uint16_t addr) const {
        return addr >= cfg_.base_addr && addr < cfg_.base_addr + kGroesse;
    }
    bool schreiben();

    Config                          cfg_;
    const K1520Bus*                 bus_ = nullptr;
    std::array<uint8_t, kGroesse>   mem_{};
    bool                            memdi_ = false;
    bool                            sue_   = false;

    std::string                     datei_;
    std::string                     datei_fehler_;
    std::atomic<uint32_t>           revision_{0};      ///< zaehlt jede Inhaltsaenderung
    uint32_t                        gesichert_ = 0;    ///< revision_ beim letzten Schreiben
    uint32_t                        gesehen_   = 0;    ///< revision_ beim letzten autoFlush
    uint64_t                        ruhe_ab_   = 0;    ///< Zeit der letzten beobachteten Aenderung
};
