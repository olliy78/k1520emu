/**
 * @file test_gun_net_maschinen.cpp
 * @brief KCNet-Karte als Option in den Maschinen (Entwurf 50, AP N2): Stecken nach dem Anlegen, zu spaet, doppelt,
 *        Tor belegt (RAF 88H/89H, ZRE-Tore), ungueltige Basis - und mit ECHTEM Z80-Code in der Maschine: PIO-Init
 *        nach TFTP1287 (Treiber 0109), Kommando 09 (Firmware-Version) mit Handshake absetzen und beide Antwortbytes holen.
 *
 * Gastcode laeuft im RAM (4000H), ohne Boot: Maschine einschalten, PC setzen, `run()`.
 */
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/k8915/k8915.h"
#include "core/machines/mc80/mc80.h"
#include "core/machines/mc80/mc80_2x.h"
#include "core/machines/pc1715/pc1715.h"
#include "core/machines/prg710/prg710.h"

namespace {

using k1520::GunNet;
using Bytes = std::vector<uint8_t>;

constexpr uint16_t kCode = 0x4000, kFertig = 0x7000, kErgebnis = 0x7010;

void ein(Bytes& p, std::initializer_list<uint8_t> b) { p.insert(p.end(), b); }

/// Gastprogramm: PIO-Init (TFTP 0109), Dummy-Lesen, PB7 muss 0 sein (-> Erkennung), Kommando 09 senden,
/// Version (Lo, Hi) holen; Ergebnis[0..1] = Version, Ergebnis[2] = PB7 nach dem Dummy-Lesen; Fertig = 2.
Bytes programm(uint8_t b) {
    const uint8_t pa = b, pb = b + 1, pas = b + 2, pbs = b + 3;
    Bytes p;
    ein(p, {0xF3, 0x31, 0x00, 0x7F});                                          // DI ; LD SP,7F00
    ein(p, {0x3E, 0x8F, 0xD3, pas, 0x3E, 0x07, 0xD3, pas});                    // Port A: Betriebsart 2, kein INT
    ein(p, {0x3E, 0xCF, 0xD3, pbs, 0x3E, 0xFF, 0xD3, pbs, 0x3E, 0x17, 0xD3, pbs, 0x3E, 0xFF, 0xD3, pbs});   // Port B
    ein(p, {0xDB, pa});                                                        // Dummy-Lesen
    ein(p, {0xDB, pb, 0xE6, 0x80, 0x32, uint8_t(kErgebnis + 2), uint8_t((kErgebnis + 2) >> 8)});   // PB7 -> Ergebnis[2]
    ein(p, {0x3E, 0x09, 0xD3, pa});                                            // Kommando 09
    ein(p, {0xDB, pb, 0x0F, 0x38, 0xFB});                                      // warten bis PB0 = 0
    for (int i = 0; i < 2; ++i) {
        ein(p, {0xDB, pb, 0x07, 0x30, 0xFB});                                  // warten bis PB7 = 1
        ein(p, {0xDB, pa, 0x32, uint8_t(kErgebnis + i), uint8_t((kErgebnis + i) >> 8)});
    }
    ein(p, {0x3E, 0x02, 0x32, uint8_t(kFertig), uint8_t(kFertig >> 8), 0x76});
    return p;
}

void poke(K1520Machine& m, uint16_t a, const Bytes& b) {
    for (size_t i = 0; i < b.size(); ++i) m.memWriteDebug(uint16_t(a + i), b[i]);
}

struct Belegt : BusDevice {
    uint8_t ioRead(uint8_t) override { return 0x00; }
    void ioWrite(uint8_t, uint8_t) override {}
    const char* deviceName() const override { return "Stoerer"; }
};

struct Leise : ::testing::Test {
    void SetUp() override { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN); }
};
using GunNetMaschine = Leise;

GunNet::Config cfg(uint8_t basis) {
    GunNet::Config c;
    c.basis = basis;
    c.richtlinie.aussenErlaubt = false;
    return c;
}

/// Bestueckungsfaelle je Maschine; @p basis = eine Basis, die an dieser Maschine frei ist; @p konflikt = Basis mit belegtem Tor.
template <class Neu>
void pruefeBestueckung(Neu neu, uint8_t basis, int konflikt = -1) {
    const GunNet::Config c = cfg(basis);
    {   // Ohne Karte
        auto m = neu();
        m->powerOn();
        EXPECT_EQ(m->gunNet(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(basis), nullptr);
    }
    {   // Stecken, doppelt, Reset/Lauf behalten die Karte
        auto m = neu();
        ASSERT_TRUE(m->installGunNet(c)) << m->gunNetFehler();
        ASSERT_NE(m->gunNet(), nullptr);
        EXPECT_TRUE(m->gunNetFehler().empty());
        for (int i = 0; i < 4; ++i) EXPECT_NE(m->bus().ioOwner(uint8_t(basis + i)), nullptr);
        EXPECT_EQ(m->bus().ioOwner(uint8_t(basis + 4)), nullptr);
        EXPECT_FALSE(m->installGunNet(c));
        EXPECT_NE(m->gunNetFehler().find("bereits"), std::string::npos) << m->gunNetFehler();
        m->powerOn();
        m->run(100);
        m->reset();
        EXPECT_NE(m->gunNet(), nullptr);
        EXPECT_GT(m->gunNet()->config().takt_hz, 0u) << "Maschinentakt fuer Kommando 03";
    }
    {   // Zu spaet
        auto m = neu();
        m->powerOn();
        m->run(100);
        EXPECT_FALSE(m->installGunNet(c));
        EXPECT_NE(m->gunNetFehler().find("vor dem ersten"), std::string::npos) << m->gunNetFehler();
        EXPECT_EQ(m->gunNet(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(basis), nullptr);
    }
    {   // Tor belegt: false mit Grund, KEIN Tor angemeldet
        auto m = neu();
        Belegt st;
        m->bus().registerIO(&st, uint8_t(basis + 2), 1);
        EXPECT_FALSE(m->installGunNet(c));
        EXPECT_NE(m->gunNetFehler().find("belegt"), std::string::npos) << m->gunNetFehler();
        EXPECT_EQ(m->gunNet(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(basis), nullptr) << "halb angemeldete Karte";
    }
    {   // Ungueltige Basis
        auto m = neu();
        GunNet::Config k = c;
        k.basis = uint8_t(basis + 1);
        EXPECT_FALSE(m->installGunNet(k));
        EXPECT_FALSE(m->gunNetFehler().empty());
        EXPECT_EQ(m->gunNet(), nullptr);
    }
    if (konflikt >= 0) {
        auto m = neu();
        EXPECT_FALSE(m->installGunNet(cfg(uint8_t(konflikt))));
        EXPECT_NE(m->gunNetFehler().find("belegt"), std::string::npos) << m->gunNetFehler();
        EXPECT_EQ(m->gunNet(), nullptr);
    }
}

/// Gast initialisiert die PIO, erkennt die Karte (PB7 = 0) und holt die Firmware-Version.
template <class M>
void gastLaeuft(M& m, Z80& cpu, uint8_t basis, std::function<void()> nachEin = {}) {
    ASSERT_TRUE(m.installGunNet(cfg(basis))) << m.gunNetFehler();
    m.powerOn();
    if (nachEin) nachEin();
    poke(m, kCode, programm(basis));
    for (uint16_t a = kErgebnis; a < kErgebnis + 4; ++a) m.memWriteDebug(a, 0xEE);
    m.memWriteDebug(kFertig, 0);
    cpu.PC = kCode;
    cpu.halted = false;
    for (long long t = 0; t < 8'000'000 && m.memReadDebug(kFertig) != 2; t += m.run(1000)) {}
    ASSERT_EQ(m.memReadDebug(kFertig), 2) << "Gastprogramm nicht fertig: PC=" << std::hex << cpu.PC;
    EXPECT_EQ(m.memReadDebug(kErgebnis + 2), 0) << "PB7 nach dem Dummy-Lesen muss 0 sein (Karte da)";
    EXPECT_EQ(m.memReadDebug(kErgebnis + 0), 2) << "Firmware-Unterversion";
    EXPECT_EQ(m.memReadDebug(kErgebnis + 1), 1) << "Firmware-Hauptversion";
    EXPECT_EQ(m.gunNet()->zaehler().kommandos, 1u);
    EXPECT_EQ(m.gunNet()->zaehler().bytes_zum_gast, 2u);

    // /RESET der Maschine setzt PIO und Automat zurueck, die Karte bleibt gesteckt
    m.reset();
    EXPECT_NE(m.gunNet(), nullptr);
}

A5120Machine::Config em256() {
    A5120Machine::Config c;
    c.em = A5120Machine::Config::Em::em256;
    return c;
}
Prg710Machine::Config prg(Prg710Machine::Config::Variante v) {
    Prg710Machine::Config c;
    c.variante = v;
    return c;
}

}  // namespace

// A5120: RAF belegt 88H/89H (fest) -> GUN-Vorgabe 88H kollidiert mit der RAF, C0H und 28H sind frei.
TEST_F(GunNetMaschine, Bestueckung_A5120) { pruefeBestueckung([] { return std::make_unique<A5120Machine>(); }, 0x88); }
TEST_F(GunNetMaschine, Bestueckung_A5120_Netzkarte) { pruefeBestueckung([] { return std::make_unique<A5120Machine>(); }, 0xC0); }
TEST_F(GunNetMaschine, Bestueckung_A5120_Netzkarte28) { pruefeBestueckung([] { return std::make_unique<A5120Machine>(); }, 0x28); }
TEST_F(GunNetMaschine, Bestueckung_A5120_16) { pruefeBestueckung([] { return std::make_unique<A5120Machine>(em256()); }, 0xC0); }
TEST_F(GunNetMaschine, Bestueckung_K8915) { pruefeBestueckung([] { return std::make_unique<K8915Machine>(); }, 0xC0, 0x80); }
TEST_F(GunNetMaschine, Bestueckung_K8915_Gen2) {
    auto neu = [] {
        K8915Machine::Config c;
        c.generation = K8915Machine::Config::Generation::Gen2;
        return std::make_unique<K8915Machine>(c);
    };
    pruefeBestueckung(neu, 0xC0, 0x84);
}
TEST_F(GunNetMaschine, Bestueckung_Prg710) {
    pruefeBestueckung([] { return std::make_unique<Prg710Machine>(prg(Prg710Machine::Config::Variante::Prg710)); }, 0x88, 0x84);
}
TEST_F(GunNetMaschine, Bestueckung_Prg710_1) {
    pruefeBestueckung([] { return std::make_unique<Prg710Machine>(prg(Prg710Machine::Config::Variante::Prg710_1)); }, 0x88, 0x84);
}
TEST_F(GunNetMaschine, Bestueckung_Pc1715) { pruefeBestueckung([] { return std::make_unique<Pc1715Machine>(); }, 0x88); }

TEST_F(GunNetMaschine, A5120_RafKollidiertMitDerGunBasis) {
    A5120Machine m;
    ASSERT_TRUE(m.installRaf(RAF::Typ::RAF512)) << m.rafFehler();
    EXPECT_FALSE(m.installGunNet(cfg(0x88)));
    EXPECT_NE(m.gunNetFehler().find("88H belegt"), std::string::npos) << m.gunNetFehler();
    EXPECT_EQ(m.gunNet(), nullptr);
    EXPECT_TRUE(m.installGunNet(cfg(0xC0))) << m.gunNetFehler();
}

TEST_F(GunNetMaschine, A5120_Z80ErkenntKarteUndHoltVersion) {
    A5120Machine m;
    gastLaeuft(m, m.cpuDebug(), 0xC0);
}
TEST_F(GunNetMaschine, A5120_GunBasis88) {
    A5120Machine m;
    gastLaeuft(m, m.cpuDebug(), 0x88);
}
TEST_F(GunNetMaschine, K8915_Z80ErkenntKarte) {
    K8915Machine m;
    gastLaeuft(m, m.zre().cpu(), 0xC0, [&] { m.zre().setReg(0x87); });   // alles RAM
}
TEST_F(GunNetMaschine, Prg710_1_Z80ErkenntKarte) {
    Prg710Machine m(prg(Prg710Machine::Config::Variante::Prg710_1));
    gastLaeuft(m, m.zre().cpu(), 0x88);
}
// PRG 710: die ATP-Zusatz-SIO belegt C0H..C3H (BS600/BS620) -> die Netzkarte-Basis C0H geht dort nicht.
TEST_F(GunNetMaschine, Prg710_AtpZusatzBelegtC0) {
    Prg710Machine m(prg(Prg710Machine::Config::Variante::Prg710_1));
    EXPECT_FALSE(m.installGunNet(cfg(0xC0)));
    EXPECT_NE(m.gunNetFehler().find("C0H belegt"), std::string::npos) << m.gunNetFehler();
}
TEST_F(GunNetMaschine, Pc1715_Z80ErkenntKarte) {
    Pc1715Machine m;
    gastLaeuft(m, m.zre().cpu(), 0xC0);
}

// MC80.3x (VIS 2a C0H..CFH, ASP 88H..93H belegt) und MC80.2x: Stecken, doppelt, zu spaet (Gast-Z80 laeuft dort nicht ohne EGOS/BSYS, daher nur die Bestueckung)
TEST_F(GunNetMaschine, Bestueckung_Mc80) { pruefeBestueckung([] { return std::make_unique<Mc80Machine>(); }, 0x28); }
TEST_F(GunNetMaschine, Bestueckung_Mc80_2x) { pruefeBestueckung([] { return std::make_unique<Mc80_2xMachine>(); }, 0xC0); }
