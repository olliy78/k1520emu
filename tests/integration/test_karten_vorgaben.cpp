/**
 * @file test_karten_vorgaben.cpp
 * @brief Die VORAUSWAHL der Steckkarten-Basen (Entwurf 50, Anwenderauftrag 2026-11-13) laeuft an jeder
 *        Maschinenvariante: GIDE + RTC, GUN-USB und KCNet-Netz stecken ZUGLEICH an den Basen des Profils
 *        (`app/profil.py`: GIDE 90H, USB 84H bzw. 8CH, Netz C0H bzw. 28H) — zusammen mit RAF, K5020 und
 *        K6022 — und je ein ECHTES Z80-Gastprogramm benutzt jede Karte (Sektor schreiben/lesen + RTC,
 *        VDIP-Startmeldung, KCNet-Firmware-Version).
 *
 * Die Tabelle unten ist die C++-Gegenseite von `VORAUSWAHL` in `tests/python/test_karten_vorauswahl.py`; wer eine
 * Vorauswahl aendert, aendert beide (der Python-Waechter prueft die Tabelle gegen das Profil).
 * Gastcode laeuft im RAM (4000H), ohne Boot.  Die drei Gastprogramme sind Kopien der Programme aus
 * `test_gide_maschinen.cpp`, `test_gun_usb_maschinen.cpp`, `test_gun_net_maschinen.cpp`.
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

using k1520::AtaGeraet;
using k1520::Gide;
using k1520::GunNet;
using k1520::GunUsb;
using Bytes = std::vector<uint8_t>;

constexpr uint16_t kCode = 0x4000, kFertig = 0x7000, kQuelle = 0x5000, kZiel = 0x5400;
constexpr uint8_t kSektor = 5;
constexpr int64_t T0 = 1794314096;   // 2026-11-10 12:34:56 UTC

void ein(Bytes& p, std::initializer_list<uint8_t> b) { p.insert(p.end(), b); }
void poke(K1520Machine& m, uint16_t a, const Bytes& b) {
    for (size_t i = 0; i < b.size(); ++i) m.memWriteDebug(uint16_t(a + i), b[i]);
}

namespace gidep {
constexpr uint16_t kErgebnis = 0x7001;
void ausg(Bytes& p, uint8_t port, uint8_t v) { ein(p, {0x3E, v, 0xD3, port}); }   // LD A,n ; OUT (port),A
void warteDrq(Bytes& p, uint8_t status) {                                            // IN A,(st) ; AND 8 ; JR Z,-6
    ein(p, {0xDB, status, 0xE6, 0x08, 0x28, 0xFA});
}
void warteNichtBsy(Bytes& p, uint8_t status) { ein(p, {0xDB, status, 0x87, 0x38, 0xFB}); }   // ADD A,A -> Carry = BSY
void lsb16(Bytes& p, uint16_t v) { p.push_back(uint8_t(v)); p.push_back(uint8_t(v >> 8)); }

/// Gastprogramm: Sektor kSektor schreiben (Quelle 5000H), zuruecklesen (nach 5400H), RTC-Register F, A, B lesen,
/// Endstatus ablegen, fertig = 2, HALT.
Bytes programm(uint8_t b) {
    const uint8_t dat = b + 8, sc = b + 0xA, sn = b + 0xB, cl = b + 0xC, ch = b + 0xD, sdh = b + 0xE, st = b + 0xF;
    Bytes p;
    ein(p, {0xF3, 0x31, 0x00, 0x7F});                                     // DI ; LD SP,7F00
    ausg(p, sdh, 0xE0); ausg(p, sc, 1); ausg(p, sn, kSektor); ausg(p, cl, 0); ausg(p, ch, 0);
    ausg(p, st, 0x30);                                                    // WRITE SECTORS
    warteDrq(p, st);
    ein(p, {0x21}); lsb16(p, kQuelle); ein(p, {0x01, 0x00, 0x02});        // LD HL,Quelle ; LD BC,512
    size_t s = p.size();
    ein(p, {0x7E, 0xD3, dat, 0x23, 0x0B, 0x78, 0xB1, 0x20});             // LD A,(HL) ; OUT ; INC HL ; DEC BC ; LD A,B ; OR C ; JR NZ
    p.push_back(uint8_t(s - (p.size() + 1)));
    warteNichtBsy(p, st);
    ausg(p, st, 0x20);                                                    // READ SECTORS (Task-File steht noch)
    warteDrq(p, st);
    ein(p, {0x21}); lsb16(p, kZiel); ein(p, {0x01, 0x00, 0x02});
    s = p.size();
    ein(p, {0xDB, dat, 0x77, 0x23, 0x0B, 0x78, 0xB1, 0x20});             // IN A,(dat) ; LD (HL),A ; INC HL ; ...
    p.push_back(uint8_t(s - (p.size() + 1)));
    warteNichtBsy(p, st);
    ein(p, {0xDB, st, 0x32}); lsb16(p, uint16_t(kErgebnis + 3));          // Endstatus
    ein(p, {0x0E, uint8_t(b + 5)});                                       // LD C,RTC-Tor
    for (int i = 0; i < 3; ++i) {                                         // IN A,(C) mit B = Registernummer (A8..A11)
        const uint8_t reg[3] = {0x0F, 0x0A, 0x0B};
        ein(p, {0x06, reg[i], 0xED, 0x78, 0x32}); lsb16(p, uint16_t(kErgebnis + i));
    }
    ein(p, {0x3E, 0x02, 0x32}); lsb16(p, kFertig); ein(p, {0x76});
    return p;
}

}  // namespace gidep

namespace usbp {
constexpr uint16_t kErgebnis = 0x7010;
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

}  // namespace usbp

namespace netp {
constexpr uint16_t kErgebnis = 0x7010;
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

}  // namespace netp

/// Basen einer Variante (Vorauswahl des Profils).
struct Basen { uint8_t gide, usb, net; };

struct Leise : ::testing::Test {
    void SetUp() override { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN); }
};
using KartenVorgaben = Leise;

/// Faehrt das Gastprogramm ab kCode bis kFertig == 2.
void fahre(K1520Machine& m, Z80& cpu, const Bytes& prog, const char* was) {
    poke(m, kCode, prog);
    m.memWriteDebug(kFertig, 0);
    cpu.PC = kCode;
    cpu.halted = false;
    for (long long t = 0; t < 8'000'000 && m.memReadDebug(kFertig) != 2; t += m.run(1000)) {}
    ASSERT_EQ(m.memReadDebug(kFertig), 2) << was << ": Gastprogramm nicht fertig, PC=" << std::hex << cpu.PC;
}

/// Alle Karten an den Vorgaben stecken, dann je Karte ein Gastprogramm.
template <class M>
void alleKarten(M& m, Z80& cpu, Basen b, std::function<void()> nachEin = {}) {
    const std::string img = k1520test::tempPath("karten_vorgaben.img");
    { std::error_code ec0; std::filesystem::remove(img, ec0); }
    ASSERT_TRUE(AtaGeraet::abbildAnlegen(img, 2048));

    Gide::Config gc;
    gc.basis = b.gide;
    Gide::RtcConfig rc;
    rc.zeitquelle = [] { return T0; };
    GunUsb::Config uc;
    uc.basis = b.usb;
    GunNet::Config nc;
    nc.basis = b.net;
    nc.richtlinie.aussenErlaubt = false;
    ASSERT_TRUE(m.installGide(gc, &rc, img)) << m.gideFehler();
    ASSERT_TRUE(m.installGunUsb(uc)) << m.gunUsbFehler();
    ASSERT_TRUE(m.installGunNet(nc)) << m.gunNetFehler();
    // die uebrigen Optionen derselben Maschine muessen daneben stecken (Vorauswahlen sind disjunkt)
    ASSERT_TRUE(m.installRaf(RAF::Typ::RAF512)) << m.rafFehler();
    ASSERT_TRUE(m.installK5020()) << m.k5020Fehler();
    ASSERT_TRUE(m.installK6022()) << m.k6022Fehler();
    m.powerOn();
    if (nachEin) nachEin();

    // GIDE + RTC
    Bytes quelle(512);
    for (size_t i = 0; i < quelle.size(); ++i) quelle[i] = uint8_t(i * 7 + b.gide);
    poke(m, kQuelle, quelle);
    for (uint16_t a = kZiel; a < kZiel + 512; ++a) m.memWriteDebug(a, 0);
    fahre(m, cpu, gidep::programm(b.gide), "GIDE");
    Bytes ziel(512);
    for (size_t i = 0; i < 512; ++i) ziel[i] = m.memReadDebug(uint16_t(kZiel + i));
    EXPECT_EQ(ziel, quelle) << "GIDE: Sektor zurueckgelesen weicht ab";
    EXPECT_EQ(m.memReadDebug(gidep::kErgebnis + 3) & 0x01, 0) << "GIDE: ERR nach dem Lesen";
    EXPECT_EQ(m.memReadDebug(gidep::kErgebnis + 1), 0x06) << "RTC Jahr (Y1)";
    EXPECT_EQ(m.memReadDebug(gidep::kErgebnis + 2), 0x02) << "RTC Jahr (Y10)";

    // USB
    for (uint16_t a = usbp::kErgebnis; a < usbp::kErgebnis + 8; ++a) m.memWriteDebug(a, 0);
    fahre(m, cpu, usbp::programm(b.usb), "USB");
    std::string s;
    for (int i = 0; i < 8; ++i) s.push_back(char(m.memReadDebug(uint16_t(usbp::kErgebnis + i))));
    EXPECT_EQ(s, "Ver03.68") << "USB: Startmeldung des VDIP1";

    // Netz
    for (uint16_t a = netp::kErgebnis; a < netp::kErgebnis + 4; ++a) m.memWriteDebug(a, 0xEE);
    fahre(m, cpu, netp::programm(b.net), "Netz");
    EXPECT_EQ(m.memReadDebug(netp::kErgebnis + 2), 0) << "Netz: PB7 nach dem Dummy-Lesen (Karte da)";
    EXPECT_EQ(m.memReadDebug(netp::kErgebnis + 0), 2) << "Netz: Firmware-Unterversion";
    EXPECT_EQ(m.memReadDebug(netp::kErgebnis + 1), 1) << "Netz: Firmware-Hauptversion";

    m.gide()->platte().auswerfen();
    std::error_code ec;
    std::filesystem::remove(img, ec);
}

constexpr Basen kStandard{0x90, 0x84, 0xC0};   // A5120 (alle Modelle, auch .16), K8915 V3, PC 1715/1715W
constexpr Basen kGen2{0x90, 0x8C, 0xC0};       // K8915 Gen 2: 80H..87H gehoeren der K2521
constexpr Basen kPrg{0x90, 0x8C, 0x28};        // PRG 710/-1: 80H..87H K2521, C0H..C3H ATP-Zusatz

Prg710Machine::Config prg(Prg710Machine::Config::Variante v) {
    Prg710Machine::Config c;
    c.variante = v;
    return c;
}

}  // namespace

TEST_F(KartenVorgaben, A5120) {
    A5120Machine m;
    alleKarten(m, m.cpuDebug(), kStandard);
}
TEST_F(KartenVorgaben, A5120_16_EM256) {
    A5120Machine::Config c;
    c.em = A5120Machine::Config::Em::em256;
    A5120Machine m(c);
    alleKarten(m, m.cpuDebug(), kStandard);
}
TEST_F(KartenVorgaben, K8915_V3) {
    K8915Machine m;
    alleKarten(m, m.zre().cpu(), kStandard, [&] { m.zre().setReg(0x87); });   // alles RAM
}
TEST_F(KartenVorgaben, K8915_Gen2) {
    K8915Machine::Config c;
    c.generation = K8915Machine::Config::Generation::Gen2;
    K8915Machine m(c);
    alleKarten(m, m.k2521().cpu(), kGen2, [&] { m.ops().setReg(0x87); });   // alles RAM
}
TEST_F(KartenVorgaben, Prg710) {
    Prg710Machine m(prg(Prg710Machine::Config::Variante::Prg710));
    alleKarten(m, m.zre().cpu(), kPrg);
}
TEST_F(KartenVorgaben, Prg710_1) {
    Prg710Machine m(prg(Prg710Machine::Config::Variante::Prg710_1));
    alleKarten(m, m.zre().cpu(), kPrg);
}
TEST_F(KartenVorgaben, Pc1715) {
    Pc1715Machine m;
    alleKarten(m, m.zre().cpu(), kStandard);
}
TEST_F(KartenVorgaben, Pc1715W) {
    Pc1715Machine::Config c;
    c.variante = Pc1715Machine::Config::Variante::Pc1715W;
    Pc1715Machine m(c);
    alleKarten(m, m.zre().cpu(), kStandard);
}
