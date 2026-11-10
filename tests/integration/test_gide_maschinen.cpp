/**
 * @file test_gide_maschinen.cpp
 * @brief GIDE (IDE-Karte + RTC 72421) als Option in den Maschinen (Entwurf 50, AP G4):
 *        Stecken nach dem Anlegen (Basis 90H und eine abweichende), zu spaet, doppelt, Tor belegt,
 *        ungueltige Basis, nicht oeffenbares Abbild — und mit ECHTEM Z80-Code in der Maschine einen
 *        Sektor schreiben, zuruecklesen, vergleichen und die RTC lesen.  Die RTC zaehlt in Maschinenzeit
 *        mit dem Takt der jeweiligen Maschine.
 *
 * Gastcode laeuft im RAM (4000H), ohne Boot: Maschine einschalten, PC setzen, `run()`.  Grenze: am
 * PC 1715W und am MC80 wird nur gesteckt, kein Gastcode gefahren.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
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
using Bytes = std::vector<uint8_t>;

constexpr int64_t T0 = 1794314096;   // 2026-11-10 12:34:56 UTC
constexpr uint16_t kCode = 0x4000, kFertig = 0x7000, kErgebnis = 0x7001, kQuelle = 0x5000, kZiel = 0x5400;
constexpr uint8_t kSektor = 5;

void ein(Bytes& p, std::initializer_list<uint8_t> b) { p.insert(p.end(), b); }
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

void poke(K1520Machine& m, uint16_t a, const Bytes& b) {
    for (size_t i = 0; i < b.size(); ++i) m.memWriteDebug(uint16_t(a + i), b[i]);
}

Gide::RtcConfig rtcCfg() {
    Gide::RtcConfig rc;
    rc.zeitquelle = [] { return T0; };
    return rc;
}

/// Ein Stoerer auf einem der Tore.
struct Belegt : BusDevice {
    uint8_t ioRead(uint8_t) override { return 0x00; }
    void ioWrite(uint8_t, uint8_t) override {}
    const char* deviceName() const override { return "Stoerer"; }
};

struct Leise : ::testing::Test {
    void SetUp() override { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN); }
};
using GideMaschine = Leise;

/// Bestueckungsfaelle je Maschine (@p neu liefert eine frische Maschine, @p konflikt eine Basis mit belegten Toren oder -1).
template <class Neu>
void pruefeBestueckung(Neu neu, int konflikt = -1) {
    Gide::Config c;
    {   // Ohne Karte: niemand auf den Toren
        auto m = neu();
        m->powerOn();
        EXPECT_EQ(m->gide(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(0x94), nullptr);
    }
    for (uint8_t basis : {uint8_t(0x90), uint8_t(0xB0)}) {   // Vorgabe und abweichende Jumper (A0H laege am A5120.16 auf dem EM, am K8915 auf A8H)
        auto m = neu();
        c.basis = basis;
        ASSERT_TRUE(m->installGide(c, nullptr)) << m->gideFehler();
        ASSERT_NE(m->gide(), nullptr);
        EXPECT_TRUE(m->gideFehler().empty());
        EXPECT_NE(m->bus().ioOwner(uint8_t(basis + 4)), nullptr);
        EXPECT_NE(m->bus().ioOwner(uint8_t(basis + 0xF)), nullptr);
        EXPECT_EQ(m->bus().ioOwner(uint8_t(basis + 3)), nullptr);
        EXPECT_EQ(m->gide()->config().basis, basis);
        EXPECT_FALSE(m->installGide(c));
        EXPECT_NE(m->gideFehler().find("bereits"), std::string::npos) << m->gideFehler();
        m->powerOn();
        m->run(100);
        m->reset();
        EXPECT_NE(m->gide(), nullptr);
    }
    {   // Zu spaet
        auto m = neu();
        m->powerOn();
        m->run(100);
        c.basis = 0x90;
        EXPECT_FALSE(m->installGide(c));
        EXPECT_NE(m->gideFehler().find("vor dem ersten"), std::string::npos) << m->gideFehler();
        EXPECT_EQ(m->gide(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(0x94), nullptr);
    }
    {   // Tor belegt: false mit Grund, KEIN Tor angemeldet
        auto m = neu();
        Belegt st;
        m->bus().registerIO(&st, 0x9A, 1);
        c.basis = 0x90;
        EXPECT_FALSE(m->installGide(c));
        EXPECT_NE(m->gideFehler().find("9AH"), std::string::npos) << m->gideFehler();
        EXPECT_EQ(m->gide(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(0x94), nullptr) << "halb angemeldete Karte";
    }
    {   // Ungueltige Basis (Tiefnibble) -> Fehlertext, kein Absturz
        auto m = neu();
        c.basis = 0x91;
        EXPECT_FALSE(m->installGide(c));
        EXPECT_FALSE(m->gideFehler().empty());
        EXPECT_EQ(m->gide(), nullptr);
    }
    {   // Abbild nicht oeffenbar: Karte bleibt ungesteckt
        auto m = neu();
        c.basis = 0x90;
        EXPECT_FALSE(m->installGide(c, nullptr, "/nicht/vorhanden/platte.img"));
        EXPECT_FALSE(m->gideFehler().empty());
        EXPECT_EQ(m->gide(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(0x94), nullptr);
    }
    if (konflikt >= 0) {   // Basis, deren Tore auf einer ZRE-Karte liegen (K8915: A8H = Speicherumschaltung)
        auto m = neu();
        c.basis = uint8_t(konflikt);
        EXPECT_FALSE(m->installGide(c));
        EXPECT_NE(m->gideFehler().find("belegt"), std::string::npos) << m->gideFehler();
        EXPECT_EQ(m->gide(), nullptr);
    }
}

/// Gast schreibt/liest/vergleicht einen Sektor und liest die RTC; Maschinenzeit treibt die RTC mit dem Maschinentakt.
template <class M>
void gastLaeuft(M& m, Z80& cpu, uint8_t basis, uint32_t cpu_hz, std::function<void()> nachEin = {}) {
    const std::string img = k1520test::tempPath("gide_maschine.img");
    std::filesystem::remove(img);
    ASSERT_TRUE(AtaGeraet::abbildAnlegen(img, 2048));
    Gide::Config c;
    c.basis = basis;
    const Gide::RtcConfig rc = rtcCfg();
    ASSERT_TRUE(m.installGide(c, &rc, img)) << m.gideFehler();
    m.powerOn();
    if (nachEin) nachEin();

    Bytes quelle(512);
    for (size_t i = 0; i < quelle.size(); ++i) quelle[i] = uint8_t(i * 7 + basis);
    poke(m, kCode, programm(basis));
    poke(m, kQuelle, quelle);
    for (uint16_t a = kZiel; a < kZiel + 512; ++a) m.memWriteDebug(a, 0);
    m.memWriteDebug(kFertig, 0);
    cpu.PC = kCode;
    cpu.halted = false;
    for (long long t = 0; t < 8'000'000 && m.memReadDebug(kFertig) != 2; t += m.run(1000)) {}
    ASSERT_EQ(m.memReadDebug(kFertig), 2) << "Gastprogramm nicht fertig: PC=" << std::hex << cpu.PC;

    Bytes ziel(512);
    for (size_t i = 0; i < 512; ++i) ziel[i] = m.memReadDebug(uint16_t(kZiel + i));
    EXPECT_EQ(ziel, quelle) << "Sektor zurueckgelesen weicht ab";
    EXPECT_EQ(m.gide()->zaehler().daten_schreiben, 512u);
    EXPECT_EQ(m.gide()->zaehler().daten_lesen, 512u);
    EXPECT_EQ(m.gide()->zaehler().befehle, 2u);
    EXPECT_EQ(m.gide()->letzterBefehl(), 0x20);
    EXPECT_EQ(m.memReadDebug(kErgebnis + 3) & 0x01, 0) << "ERR nach dem Lesen";
    EXPECT_EQ(m.memReadDebug(kErgebnis + 0), 0x04);   // RTC Register F: 24-h-Betrieb
    EXPECT_EQ(m.memReadDebug(kErgebnis + 1), 0x06);   // Y1 (Jahr 2026)
    EXPECT_EQ(m.memReadDebug(kErgebnis + 2), 0x02);   // Y10

    // Die Datei hat den Sektor (Platte schreibt sofort ins Abbild, die Datei folgt beim Flush)
    ASSERT_TRUE(m.gide()->platte().flush());
    std::ifstream f(img, std::ios::binary);
    f.seekg(kSektor * 512);
    Bytes datei(512);
    f.read(reinterpret_cast<char*>(datei.data()), 512);
    EXPECT_EQ(datei, quelle) << "Abbilddatei traegt den Sektor nicht";

    // RTC in Maschinenzeit mit DEM Takt der Maschine: vier Maschinensekunden = vier Uhrsekunden
    const int64_t z0 = m.gide()->rtc()->gastZeit();
    for (long long t = 0; t < 4LL * cpu_hz; ) t += m.run(100000);
    const int64_t d = m.gide()->rtc()->gastZeit() - z0;
    EXPECT_GE(d, 4);
    EXPECT_LE(d, 4) << "RTC-Takt passt nicht zum Maschinentakt";   // zu hoher/niedriger takt_hz gaebe 2/3 bzw. 5/6
    m.gide()->platte().auswerfen();
    std::filesystem::remove(img);
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

TEST_F(GideMaschine, Bestueckung_A5120) { pruefeBestueckung([] { return std::make_unique<A5120Machine>(); }); }
TEST_F(GideMaschine, Bestueckung_A5120_16) { pruefeBestueckung([] { return std::make_unique<A5120Machine>(em256()); }); }
TEST_F(GideMaschine, Bestueckung_K8915) { pruefeBestueckung([] { return std::make_unique<K8915Machine>(); }, 0xA0); }
TEST_F(GideMaschine, Bestueckung_Prg710) {
    pruefeBestueckung([] { return std::make_unique<Prg710Machine>(prg(Prg710Machine::Config::Variante::Prg710)); }, 0x80);
}
TEST_F(GideMaschine, Bestueckung_Prg710_1) {
    pruefeBestueckung([] { return std::make_unique<Prg710Machine>(prg(Prg710Machine::Config::Variante::Prg710_1)); }, 0x80);
}
TEST_F(GideMaschine, Bestueckung_Pc1715) { pruefeBestueckung([] { return std::make_unique<Pc1715Machine>(); }); }

TEST_F(GideMaschine, A5120_Z80SchreibtLiestUndLiestRtc) {
    A5120Machine m;
    gastLaeuft(m, m.cpuDebug(), 0x90, 2'457'600);
}
TEST_F(GideMaschine, A5120_AbweichendeBasisB0) {
    A5120Machine m;
    gastLaeuft(m, m.cpuDebug(), 0xB0, 2'457'600);
}
TEST_F(GideMaschine, K8915_Z80SchreibtLiestUndLiestRtc) {
    K8915Machine m;
    gastLaeuft(m, m.zre().cpu(), 0x90, 2'457'600, [&] { m.zre().setReg(0x87); });   // alles RAM
}
TEST_F(GideMaschine, Prg710_1_Z80SchreibtLiestUndLiestRtc) {
    Prg710Machine m(prg(Prg710Machine::Config::Variante::Prg710_1));
    gastLaeuft(m, m.zre().cpu(), 0x90, 2'457'600);
}
TEST_F(GideMaschine, Pc1715_Z80SchreibtLiestUndLiestRtc) {
    Pc1715Machine m;
    gastLaeuft(m, m.zre().cpu(), 0x90, Pc1715Machine::CPU_HZ);
}
