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
        case Config::Typ::K6313_V24:  return "K6313 (V.24)";
        case Config::Typ::K6311_V24:  return "K6311 (V.24)";
    }
    return "Drucker ?";
}

namespace {
static_assert(Sd1157Steuerung::CPU_HZ == K6316Steuerung::CPU_HZ && Sd1157Steuerung::CPU_HZ == K6313Ccb::CPU_HZ,
              "alle Drucker laufen mit 2,4576 MHz");

std::unique_ptr<DruckerSteuerung> neueSteuerung(const PrinterMachine::Config& cfg, K1520Bus& bus)
{
    switch (cfg.typ) {
        case PrinterMachine::Config::Typ::Sd1157_264:
            return std::make_unique<Sd1157Steuerung>(bus, cfg.sd1157);
        case PrinterMachine::Config::Typ::K6316_Ifss:
        case PrinterMachine::Config::Typ::K6316_V24: {
            K6316Steuerung::Config c = cfg.k6316;
            c.schnittstelle = cfg.typ == PrinterMachine::Config::Typ::K6316_V24 ? K6316Steuerung::Config::Schnittstelle::V24
                                                                               : K6316Steuerung::Config::Schnittstelle::Ifss;
            return std::make_unique<K6316Steuerung>(bus, c);
        }
        case PrinterMachine::Config::Typ::K6313_V24:
            return std::make_unique<K6313Ccb>(bus, cfg.k6313);
        case PrinterMachine::Config::Typ::K6311_V24: {
            K6313Ccb::Config c = cfg.k6313;
            c.alsK6311();   // dieselbe Karte, Hardware aus dem ROM erschlossen [?]
            return std::make_unique<K6313Ccb>(bus, c);
        }
    }
    throw std::invalid_argument("unbekannter Druckertyp");
}
}  // namespace

PrinterMachine::PrinterMachine(const Config& cfg)
    : cfg_(cfg), steuerung_(neueSteuerung(cfg_, bus_))
{
    sd1157_ = dynamic_cast<Sd1157Steuerung*>(steuerung_.get());
    k6316_  = dynamic_cast<K6316Steuerung*>(steuerung_.get());
    k6313_  = dynamic_cast<K6313Ccb*>(steuerung_.get());
    steuerung_->attachToBus(bus_);
    // Kette mit der Steuerung als einziger Karte (CTC, PIO, SIO stehen in ihr, Reihenfolge dort).
    bus_.setInterruptChain({&steuerung_->unterbrechung()});
    // Schnittstellen nach aussen (Entwurf 19 §3, 49 §4.4): Reihenfolge = Hub-Index = C-ABI-Index: IFSS (B), V.24 (A).
    steuerung_->setzeHubAktiv(true);
    hub_.registriere(steuerung_->serialAnschluss(false));
    hub_.registriere(steuerung_->serialAnschluss(true));
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
    return {&steuerung_->serialAnschluss(false), &steuerung_->serialAnschluss(true)};
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
