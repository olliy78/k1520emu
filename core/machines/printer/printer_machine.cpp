/**
 * @file printer_machine.cpp
 * @brief Drucker — Aufbau, Lebenslauf, Laufschleife (WP1, Entwurf 49).
 * @see printer_machine.h, doc/design/49_k1520printer.md
 */

#include "core/machines/printer/printer_machine.h"
#include "core/logger.h"

const char* PrinterMachine::typName(Config::Typ t)
{
    switch (t) {
        case Config::Typ::Sd1157_264: return "SD 1157/264";
        case Config::Typ::K6316_Ifss: return "K6316 (IFSS)";
        case Config::Typ::K6316_V24:  return "K6316 (V.24)";
    }
    return "Drucker ?";
}

PrinterMachine::Config PrinterMachine::pruefe(const Config& cfg)
{
    switch (cfg.typ) {
        case Config::Typ::Sd1157_264:
            break;
        case Config::Typ::K6316_Ifss:
        case Config::Typ::K6316_V24:
            throw std::invalid_argument(std::string(typName(cfg.typ)) +
                                        " ist noch nicht gebaut (K6316Steuerung folgt in WP2a, Entwurf 49)");
    }
    return cfg;
}

PrinterMachine::PrinterMachine(const Config& cfg)
    : cfg_(pruefe(cfg)), steuerung_(std::make_unique<Sd1157Steuerung>(bus_, cfg_.sd1157))
{
    steuerung_->attachToBus(bus_);
    // Kette mit der Steuerung als einziger Karte (CTC, PIO, SIO stehen in ihr, Annahme der Reihenfolge dort).
    bus_.setInterruptChain({steuerung_.get()});
    // Schnittstellen nach aussen (Entwurf 19 §3, 49 §4.4): Reihenfolge = Hub-Index = C-ABI-Index: IFSS (B), V.24 (A).
    steuerung_->setzeHubAktiv(true);
    hub_.registriere(steuerung_->anschluss(Sd1157Steuerung::Config::Schnittstelle::Ifss));
    hub_.registriere(steuerung_->anschluss(Sd1157Steuerung::Config::Schnittstelle::V24));
    powerOn();
}

PrinterMachine::~PrinterMachine() = default;

const FormatCatalog& PrinterMachine::formatCatalog() const
{
    static const FormatCatalog leer;
    return leer;
}

std::vector<k1520::serial::SerialAnschluss*> PrinterMachine::serielleAnschluesse()
{
    return {&steuerung_->anschluss(Sd1157Steuerung::Config::Schnittstelle::Ifss),
            &steuerung_->anschluss(Sd1157Steuerung::Config::Schnittstelle::V24)};
}

void PrinterMachine::altRueckruf(Sd1157Steuerung::Config::Schnittstelle s, SerialCb cb)
{
    steuerung_->setAbnehmer(s, std::move(cb));
}

void PrinterMachine::powerOn()
{
    steuerung_->powerOn();
    total_cycles_ = 0;
    resetHardware();
    LOG_INFO("PRINTER", "%s: Netz ein", typName(cfg_.typ));
}

void PrinterMachine::reset()
{
    bestueckungAbschliessen();
    resetHardware();
}

void PrinterMachine::resetHardware()
{
    steuerung_->reset();
    hub_.gastZurueckgesetzt();   // XOFF-/RTS-Halt des alten Gastes gilt nicht weiter
    serial_naechst_ = 0;
    bus_.clearNMI();
    bus_.releaseINT();
    bus_.markIntDirty();
}

int PrinterMachine::run(int max_cycles)
{
    bestueckungAbschliessen();
    if (nmi_taster_.exchange(false, std::memory_order_relaxed)) bus_.assertNMI();
    Z80& cpu = steuerung_->cpu();
    int remaining = max_cycles;
    while (remaining > 0 && !stop_.load(std::memory_order_relaxed)) {
        k1520::logging::Logger::instance().update(total_cycles_, cpu.PC, 0);
        bus_.updateInterruptChain();
        // Nach `EI` erst nach dem folgenden Befehl (Z80::ei_schatten, Fund A8).
        if (bus_.isINT() && cpu.interruptAnnehmbar()) cpu.interrupt(bus_.interruptAcknowledge());
        if (bus_.isNMI()) {
            cpu.nmi();
            bus_.clearNMI();
        }
        const int used = cpu.step();
        if (used == 0 && stop_.load(std::memory_order_relaxed)) break;   // Debugger-Halt
        remaining     -= used;
        total_cycles_ += static_cast<uint64_t>(used);
        bool dirty = steuerung_->clockTick(used);
        // Schnittstellen nach aussen (Entwurf 19 §6): der Wandler arbeitet nur alle 1/16 Zeichenzeit.
        if (total_cycles_ >= serial_naechst_) {
            serial_naechst_ = hub_.takt(total_cycles_);
            dirty |= steuerung_->nimmSeriellGeaendert();
        }
        if (dirty) bus_.markIntDirty();
    }
    return max_cycles - remaining;
}
