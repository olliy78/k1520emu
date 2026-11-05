/**
 * @file printer_machine.h
 * @brief Drucker (Entwurf 49, Ticket #28) — siebte Maschine des Kerns (K1520_MACHINE_PRINTER = 7).
 *
 * Stand WP1: **Geruest.**  Der SD 1157 (`Sd1157Steuerung`: U880 2,4576 MHz, Original-ROM 0000H–297FH,
 * RAM 2C00H/3800H, ZG1 3000H, CTC DCH, PIO F4H/F8H/ECH, SIO 7CH) laeuft an; es gibt noch kein
 * Druckwerk, kein Blatt, keinen Anschluss der SIO nach aussen und keine Bedienoberflaeche.
 * Die K6316-Typen sind als Werte vorgesehen und werden mit klarer Meldung abgewiesen (WP2a).
 *
 * Die Maschine kennt nur den Bus ihrer Steuerung (der Drucker hat keinen K1520-Steckplatz fuer
 * Zusatzkarten, @ref zusatzkartenSteckbar = false), keine Laufwerke, keine Tastatur, kein Bild.
 * Eine Steuerungsklasse je Geraet, kein `if (typ == …)` im Druckwerk (Entwurf 49 §3).
 *
 * Interruptkette und Speicher stehen in der Karte (core/cards/printer/sd1157_steuerung.h).
 */

#pragma once
#include "core/bus/k1520_bus.h"
#include "core/cards/printer/sd1157_steuerung.h"
#include "core/machines/machine.h"
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class PrinterMachine : public K1520Machine {
public:
    struct Config {
        /// Druckertyp; `K6316_*` sind vorgesehen, aber erst in WP2a lauffaehig (Anlegen wirft).
        enum class Typ { Sd1157_264, K6316_Ifss, K6316_V24 };
        Typ typ = Typ::Sd1157_264;
        /// Aufbau des SD 1157 (DIP-Schalter, Interface-Wahl, Ruhelage der Tasten).
        Sd1157Steuerung::Config sd1157;
    };

    static constexpr uint32_t CPU_HZ = Sd1157Steuerung::CPU_HZ;

    PrinterMachine() : PrinterMachine(Config{}) {}
    explicit PrinterMachine(const Config& cfg);
    ~PrinterMachine() override;

    static const char* typName(Config::Typ t);

    // ─── Lebenslauf ──────────────────────────────────────────────────────────
    void powerOn() override;
    void reset() override;
    int  run(int max_cycles) override;
    void stop() override { stop_.store(true); }
    /// NMI-Eingang des SD 1157: Farbbandhavarie + Spannungsausfall (hardware.md §8), Vektor 0066H.
    void nmi() override { nmi_taster_.store(true, std::memory_order_relaxed); }

    // ─── Kein Bild, keine Tastatur ───────────────────────────────────────────
    const uint8_t* framebuffer() const override { static const uint8_t leer = 0; return &leer; }
    int  fbWidth()  const override { return 0; }
    int  fbHeight() const override { return 0; }
    bool fbDirty()  const override { return false; }
    void fbClearDirty() override {}
    void setConsoleMode(bool) override {}
    bool consolePoll(int&, int&, char&) override { return false; }
    uint8_t screenChar(int, int) const override { return 0; }
    void keyPress(uint32_t, bool, bool) override {}
    void keyRelease(uint32_t) override {}
    void setKeyRepeatRealtime(bool) override {}

    int machineType() const override { return 7; }   // K1520_MACHINE_PRINTER
    Config::Typ typ() const { return cfg_.typ; }

    // ─── Keine Laufwerke ─────────────────────────────────────────────────────
    bool mountDisk(int, const std::string&, const std::string&, bool) override { return false; }
    bool mountDiskImage(int, std::unique_ptr<DiskImage>, bool) override { return false; }
    bool createDisk(int, const std::string&, const std::string&, bool) override { return false; }
    bool saveDiskAs(int, const std::string&, const std::string&) override { return false; }
    bool unmountDisk(int) override { return false; }
    bool flushDisks() override { return false; }
    bool isDiskRawCompatible(int) const override { return false; }
    std::string diskPath(int) const override { return ""; }
    std::string diskContainer(int) const override { return ""; }
    std::string diskNotice(int) const override { return ""; }
    std::string detectedFormatName(int) const override { return ""; }
    std::string defaultFormatName(int) const override { return ""; }
    std::vector<std::string> compatibleFormats(int) const override { return {}; }
    std::string formatDescription(const std::string&) const override { return ""; }
    const FormatCatalog& formatCatalog() const override;
    bool isDiskActive(int) const override { return false; }
    bool isDiskWriteProtected(int) const override { return false; }
    bool isDiskLedOn(int) const override { return false; }
    bool isMotorOn(int) const override { return false; }
    bool isHeadLoaded() const override { return false; }
    void setDiskWriteProtect(int, bool) override {}

    // ─── Serielle Schnittstellen: noch nicht angeschlossen (WP4) ─────────────
    void setDFUECallback(SerialCb) override {}
    void dfueSend(uint8_t) override {}

    // ─── Keine Zusatzkarten ──────────────────────────────────────────────────
    bool zusatzkartenSteckbar() const override { return false; }

    // ─── Diagnose ────────────────────────────────────────────────────────────
    uint8_t memReadDebug(uint16_t a) override { return bus_.memRead(a); }
    void    memWriteDebug(uint16_t a, uint8_t d) override { bus_.memWrite(a, d); }
    uint8_t ioReadDebug(uint8_t port) override { return bus_.ioRead(port); }
    std::string lastError() const override { return ""; }

    Sd1157Steuerung& sd1157() { return *steuerung_; }
    K1520Bus& bus() { return bus_; }
    uint64_t  totalCycles() const { return total_cycles_; }
    void      clearStop() { stop_.store(false); }
    uint16_t  cpuPC() const { return steuerung_->cpu().PC; }
    // Debugger/Werkzeuge: Befehlsbeobachter und Bus-Protokoll.
    void setCpuTraceCallback(std::function<void(const Z80&)> cb) { steuerung_->cpu().traceCallback = std::move(cb); }
    void setBusTrace(K1520Bus::BusTrace cb) { bus_.setTraceCallback(std::move(cb)); }

protected:
    K1520Bus& systemBus() override { return bus_; }
    uint32_t  k6022TaktHz() const override { return CPU_HZ; }

private:
    static Config pruefe(const Config& cfg);
    void resetHardware();

    const Config cfg_;
    K1520Bus bus_;
    std::unique_ptr<Sd1157Steuerung> steuerung_;

    std::atomic<bool> stop_{false};
    std::atomic<bool> nmi_taster_{false};
    uint64_t total_cycles_ = 0;
};
