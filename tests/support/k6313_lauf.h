/**
 * @file k6313_lauf.h
 * @brief Gemeinsame Hilfen der K6313-Tests ab WP8h: Maschine fahren, Text ueber einen Wandler senden.
 */
#pragma once
#include <memory>
#include <string>

#include "core/machines/printer/printer_machine.h"

namespace k6313test {

struct Lauf {
    std::unique_ptr<PrinterMachine> m;
    explicit Lauf(PrinterMachine::Config c = {}, PrinterMachine::Config::Typ t = PrinterMachine::Config::Typ::K6313_V24)
    {
        c.typ = t;
        m = std::make_unique<PrinterMachine>(c);
    }
    K6313Ccb& st() { return m->k6313(); }
    void fahre(uint64_t takte)
    {
        const uint64_t z = m->totalCycles() + takte;
        while (m->totalCycles() < z) m->run(2000);
    }
    /// Text ueber den Wandler @p idx (0 IFSS, 1 V.24) in die SIO; CTS/DCD kommen vom Wandler (`anbinden`).
    void sende(int idx, const std::string& t)
    {
        auto& w = m->serialHub()->wandler(idx);
        for (char ch : t) {
            const uint8_t b = static_cast<uint8_t>(ch);
            int g = 0;
            while (w.fernFrei() == 0 && g++ < 1000) fahre(3000);
            w.fernGib(&b, 1);
            fahre(3000);
            uint8_t rest[64];
            while (w.fernNimm(rest, sizeof rest) > 0) {}
        }
    }
    /// Bis ON LINE fahren und den Wandler anbinden.
    void bereit(int idx = 1)
    {
        fahre(30'000'000);
        m->serialHub()->wandler(idx).anbinden();
        fahre(1'000'000);
    }
};

}  // namespace k6313test
