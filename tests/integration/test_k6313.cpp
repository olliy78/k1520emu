/**
 * @file test_k6313.cpp
 * @brief K6313 (Entwurf 49 WP8c): Original-ROM 33538013 (Fassung kurz, V.24-Kassette) bis in die Grundstellung.
 *        Kein Druckbild-Orakel (WP8d).  Stand des Selbsttests: Merkposten drucker.md Nr. 32.
 */
#include <gtest/gtest.h>
#include <string>

#include "core/machines/printer/printer_machine.h"

using Typ = PrinterMachine::Config::Typ;

namespace {
struct Lauf {
    std::unique_ptr<PrinterMachine> m;
    explicit Lauf(PrinterMachine::Config c = {})
    {
        c.typ = Typ::K6313_V24;
        m = std::make_unique<PrinterMachine>(c);
    }
    K6313Ccb& st() { return m->k6313(); }
    void fahre(uint64_t takte)
    {
        const uint64_t z = m->totalCycles() + takte;
        while (m->totalCycles() < z) m->run(2000);
    }
};
}  // namespace

TEST(K6313Modul, ErreichtDieGrundstellung)
{
    Lauf l;
    l.fahre(30'000'000);
    EXPECT_TRUE(l.st().lampe(1)) << "ON LINE";
    EXPECT_TRUE(l.st().lampe(2)) << "POW";
    EXPECT_FALSE(l.st().lampe(3)) << "ERR";
    EXPECT_FALSE(l.st().lampe(4)) << "PE";
    // Wagen am linken Anschlag synchronisiert (Phasenrichtung [?]: am Lauf erreicht die Firmware eine Lage nahe 0), steht, nicht blockiert
    const int pos = l.m->druck().wagen().position();
    EXPECT_GE(pos, 0);
    EXPECT_LE(pos, 20);
    EXPECT_FALSE(static_cast<SchrittWagen&>(l.m->druck().wagen()).blockiert());
    EXPECT_EQ(l.m->druck().papier(0).schritte(), 1u) << "Anfangsvorschub um einen Schritt (Richtung der Formularphasen [?])";
    EXPECT_EQ(l.st().nadeln(), 0);
    EXPECT_TRUE(l.st().sioAusgabe(0).empty()) << "ohne XON/XOFF (17-1 OFF) laeuft das DTR-Protokoll, es wird nichts gesendet";
}

TEST(K6313Modul, GrundstellungAusMehrerenStartlagenDesWagens)
{
    for (int start : {0, 5, 400, 1000, 1700}) {
        PrinterMachine::Config c;
        c.k6313.wagen_start = start;
        Lauf l(c);
        l.fahre(30'000'000);
        EXPECT_TRUE(l.st().lampe(1)) << start;
        EXPECT_LE(l.m->druck().wagen().position(), 20) << start;
    }
}

TEST(K6313Modul, DtrFolgtDemPapierendeUeberKanalB)
{
    // Die Firmware schreibt DTR/RTS an SIO-Kanal B (WR5 = 8AH bei Papier, 0AH bei Papierende, ROM 1EDDH-1EFDH [F])
    Lauf l;
    l.fahre(30'000'000);
    EXPECT_TRUE(l.m->serielleAnschluesse()[1]->dtr());
    l.st().setPapierDa(false);
    l.fahre(3'000'000);
    EXPECT_TRUE(l.st().lampe(4));
    EXPECT_FALSE(l.m->serielleAnschluesse()[1]->dtr()) << "Papierende nimmt DTR zurueck";
}

TEST(K6313Modul, OhneDil52VerlaesstDieFirmwareDenNormalpfad)
{
    // Spalte 3, Zeile 2/3 (5-2 ON, 5-1 OFF) liest die Firmware beim Einschalten (ROM 018CH) [F]
    PrinterMachine::Config c;
    c.k6313.dil = 0;
    Lauf l(c);
    l.fahre(30'000'000);
    Lauf n;
    n.fahre(30'000'000);
    EXPECT_NE(l.m->cpuPC(), n.m->cpuPC()) << "die Leerlaufschleife (Normalpfad) wird nicht erreicht";
}
