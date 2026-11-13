/**
 * @file test_gun_usb_maschinen.cpp
 * @brief GUN-USB (Z80-PIO + VDIP1) als Option in den Maschinen (Entwurf 50, AP U2): Stecken nach dem Anlegen
 *        (Basis 84H bzw. DCh), zu spaet, doppelt, Tor belegt (K8915 V3: 80H..83H, K8915 Gen 2 und PRG 710: 80H..87H gehoeren der ZRE),
 *        ungueltige Basis, nicht oeffenbarer Stickordner - und mit ECHTEM Z80-Code in der Maschine: Port-Init
 *        nach USBINC.Z80 und die Startmeldung des VDIP1 byteweise abholen.
 *
 * Gastcode laeuft im RAM (4000H), ohne Boot: Maschine einschalten, PC setzen, `run()`.  Grenze: am
 * PC 1715W und am MC80 wird nicht gesteckt/gefahren.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/k8915/k8915.h"
#include "core/machines/pc1715/pc1715.h"
#include "core/machines/prg710/prg710.h"
#include "tests/support/temp_path.h"

namespace {

using k1520::GunUsb;
using Bytes = std::vector<uint8_t>;

constexpr uint16_t kCode = 0x4000, kFertig = 0x7000, kErgebnis = 0x7010;

void ein(Bytes& p, std::initializer_list<uint8_t> b) { p.insert(p.end(), b); }

/// Gastprogramm: Init (USBINC.Z80), auf RXF# = 0 warten, acht Bytes mit RD#-Zyklus holen, Fertig = 2, HALT.
Bytes programm(uint8_t b) {
    const uint8_t pa = b, pb = b + 1, pas = b + 2, pbs = b + 3;
    Bytes p;
    ein(p, {0xF3, 0x31, 0x00, 0x7F});                                       // DI ; LD SP,7F00
    ein(p, {0x3E, 0xCF, 0xD3, pbs, 0x3E, 0x33, 0xD3, pbs, 0x3E, 0x07, 0xD3, pbs, 0x3E, 0xC4, 0xD3, pb});
    ein(p, {0x3E, 0x8F, 0xD3, pas, 0x3E, 0x07, 0xD3, pas, 0xDB, pa});
    ein(p, {0xDB, pb, 0xE6, 0x01, 0x20, 0xFA});                              // warten bis RXF# = 0
    ein(p, {0x21, uint8_t(kErgebnis), uint8_t(kErgebnis >> 8), 0x06, 0x08});   // LD HL,Ergebnis ; LD B,8
    ein(p, {0x3E, 0xC0, 0xD3, pb, 0xDB, pa, 0x77, 0x23, 0x3E, 0xC4, 0xD3, pb, 0x10, 0xF2});
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
using GunUsbMaschine = Leise;

/// Bestueckungsfaelle je Maschine; @p basis = eine Basis, die an dieser Maschine frei ist.
template <class Neu>
void pruefeBestueckung(Neu neu, uint8_t basis, int konflikt = -1) {
    GunUsb::Config c;
    c.basis = basis;
    {   // Ohne Karte
        auto m = neu();
        m->powerOn();
        EXPECT_EQ(m->gunUsb(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(basis), nullptr);
    }
    {   // Stecken, doppelt, Reset/Lauf behalten die Karte
        auto m = neu();
        ASSERT_TRUE(m->installGunUsb(c)) << m->gunUsbFehler();
        ASSERT_NE(m->gunUsb(), nullptr);
        EXPECT_TRUE(m->gunUsbFehler().empty());
        for (int i = 0; i < 4; ++i) EXPECT_NE(m->bus().ioOwner(uint8_t(basis + i)), nullptr);
        EXPECT_EQ(m->bus().ioOwner(uint8_t(basis + 4)), nullptr);
        EXPECT_FALSE(m->installGunUsb(c));
        EXPECT_NE(m->gunUsbFehler().find("bereits"), std::string::npos) << m->gunUsbFehler();
        m->powerOn();
        m->run(100);
        m->reset();
        EXPECT_NE(m->gunUsb(), nullptr);
    }
    {   // Zu spaet
        auto m = neu();
        m->powerOn();
        m->run(100);
        EXPECT_FALSE(m->installGunUsb(c));
        EXPECT_NE(m->gunUsbFehler().find("vor dem ersten"), std::string::npos) << m->gunUsbFehler();
        EXPECT_EQ(m->gunUsb(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(basis), nullptr);
    }
    {   // Tor belegt: false mit Grund, KEIN Tor angemeldet
        auto m = neu();
        Belegt st;
        m->bus().registerIO(&st, uint8_t(basis + 2), 1);
        EXPECT_FALSE(m->installGunUsb(c));
        EXPECT_NE(m->gunUsbFehler().find("belegt"), std::string::npos) << m->gunUsbFehler();
        EXPECT_EQ(m->gunUsb(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(basis), nullptr) << "halb angemeldete Karte";
    }
    {   // Ungueltige Basis
        auto m = neu();
        GunUsb::Config k = c;
        k.basis = uint8_t(basis + 1);
        EXPECT_FALSE(m->installGunUsb(k));
        EXPECT_FALSE(m->gunUsbFehler().empty());
        EXPECT_EQ(m->gunUsb(), nullptr);
    }
    {   // Stickordner nicht oeffenbar: Karte bleibt ungesteckt
        auto m = neu();
        GunUsb::Config k = c;
        k.ordner = "/nicht/vorhanden/stick";
        EXPECT_FALSE(m->installGunUsb(k));
        EXPECT_FALSE(m->gunUsbFehler().empty());
        EXPECT_EQ(m->gunUsb(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(basis), nullptr);
    }
    if (konflikt >= 0) {   // 84H..87H gehoeren der ZRE (K8915/PRG 710)
        auto m = neu();
        GunUsb::Config k = c;
        k.basis = uint8_t(konflikt);
        EXPECT_FALSE(m->installGunUsb(k));
        EXPECT_NE(m->gunUsbFehler().find("belegt"), std::string::npos) << m->gunUsbFehler();
        EXPECT_EQ(m->gunUsb(), nullptr);
    }
}

/// Gast initialisiert die PIO und holt die Startmeldung des VDIP1 ab.
template <class M>
void gastLaeuft(M& m, Z80& cpu, uint8_t basis, std::function<void()> nachEin = {}) {
    GunUsb::Config c;
    c.basis = basis;
    ASSERT_TRUE(m.installGunUsb(c)) << m.gunUsbFehler();
    m.powerOn();
    if (nachEin) nachEin();
    poke(m, kCode, programm(basis));
    for (uint16_t a = kErgebnis; a < kErgebnis + 8; ++a) m.memWriteDebug(a, 0);
    m.memWriteDebug(kFertig, 0);
    cpu.PC = kCode;
    cpu.halted = false;
    for (long long t = 0; t < 8'000'000 && m.memReadDebug(kFertig) != 2; t += m.run(1000)) {}
    ASSERT_EQ(m.memReadDebug(kFertig), 2) << "Gastprogramm nicht fertig: PC=" << std::hex << cpu.PC;
    std::string s;
    for (int i = 0; i < 8; ++i) s.push_back(char(m.memReadDebug(uint16_t(kErgebnis + i))));
    EXPECT_EQ(s, "Ver03.68") << "Startmeldung des VDIP1 nicht angekommen";
    EXPECT_EQ(m.gunUsb()->zaehler().gelesen, 8u);
    EXPECT_EQ(m.gunUsb()->zaehler().geschrieben, 0u);
    EXPECT_EQ(m.gunUsb()->zaehler().resets, 1u);

    // /RESET der Maschine setzt PIO und VDIP1 zurueck (Zaehler bleiben), Stick bleibt
    m.reset();
    EXPECT_NE(m.gunUsb(), nullptr);
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

TEST_F(GunUsbMaschine, Bestueckung_A5120) { pruefeBestueckung([] { return std::make_unique<A5120Machine>(); }, 0x84); }
TEST_F(GunUsbMaschine, Bestueckung_A5120_16) { pruefeBestueckung([] { return std::make_unique<A5120Machine>(em256()); }, 0xDC); }
// K8915 V3: die ZRE-CTC belegt nur 80H..83H, 84H..87H sind frei (Befund AP U2; Entwurf 50 Abschnitt 7.1a vermutete mehr).
TEST_F(GunUsbMaschine, Bestueckung_K8915) { pruefeBestueckung([] { return std::make_unique<K8915Machine>(); }, 0x84, 0x80); }
// K8915 Gen 2 (K2521): CTC/PIO auf 80H..87H -> 84H belegt, DCh frei.
TEST_F(GunUsbMaschine, Bestueckung_K8915_Gen2) {
    auto neu = [] {
        K8915Machine::Config c;
        c.generation = K8915Machine::Config::Generation::Gen2;
        return std::make_unique<K8915Machine>(c);
    };
    pruefeBestueckung(neu, 0xDC, 0x84);
}
TEST_F(GunUsbMaschine, Bestueckung_Prg710) {
    pruefeBestueckung([] { return std::make_unique<Prg710Machine>(prg(Prg710Machine::Config::Variante::Prg710)); }, 0xDC, 0x84);
}
TEST_F(GunUsbMaschine, Bestueckung_Prg710_1) {
    pruefeBestueckung([] { return std::make_unique<Prg710Machine>(prg(Prg710Machine::Config::Variante::Prg710_1)); }, 0xDC, 0x84);
}
TEST_F(GunUsbMaschine, Bestueckung_Pc1715) { pruefeBestueckung([] { return std::make_unique<Pc1715Machine>(); }, 0x84); }

TEST_F(GunUsbMaschine, A5120_Z80HoltStartmeldung) {
    A5120Machine m;
    gastLaeuft(m, m.cpuDebug(), 0x84);
}
TEST_F(GunUsbMaschine, A5120_BasisDC) {
    A5120Machine m;
    gastLaeuft(m, m.cpuDebug(), 0xDC);
}
TEST_F(GunUsbMaschine, K8915_Z80HoltStartmeldung) {
    K8915Machine m;
    gastLaeuft(m, m.zre().cpu(), 0xDC, [&] { m.zre().setReg(0x87); });   // alles RAM
}
TEST_F(GunUsbMaschine, Prg710_1_Z80HoltStartmeldung) {
    Prg710Machine m(prg(Prg710Machine::Config::Variante::Prg710_1));
    gastLaeuft(m, m.zre().cpu(), 0xDC);
}
TEST_F(GunUsbMaschine, Pc1715_Z80HoltStartmeldung) {
    Pc1715Machine m;
    gastLaeuft(m, m.zre().cpu(), 0x84);
}
