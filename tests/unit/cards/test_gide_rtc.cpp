/**
 * @file test_gide_rtc.cpp
 * @brief GIDE-RTC (Entwurf 50 Abschnitt 4, AP G3): Reh's `gidertc.mac` (RTCinit, RTCwait, RTCread, RTCset)
 *        als Z80-Maschinencode gegen das 72421-Modell; Maschinenzeit, Hold/Busy, 24h/12h, Batteriedatei.
 *
 * Die Gastroutinen sind von Hand assembliert (Quelle: Tilmann Reh, gidertc.mac, nur als Vorlage gelesen).
 * Die Maschinentakte laufen mit: jeder Z80-Schritt wird an Gide::takt() gemeldet.
 */
#include <gtest/gtest.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <vector>

#include "core/cards/gide/gide.h"
#include "core/primitives/z80.h"

using k1520::Gide;

namespace {

constexpr int64_t T0 = 1794314096;   // 2026-11-10 12:34:56 UTC
constexpr uint32_t HZ = 4'000'000;
constexpr uint8_t RTC = 0x95;        // Basis 90h + 5

struct TempDir {
    std::filesystem::path p;
    TempDir() {
        static std::atomic<int> n{0};
        p = std::filesystem::temp_directory_path() /
            ("k1520_gide_rtc_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
        std::filesystem::create_directories(p);
    }
    ~TempDir() { std::error_code ec; std::filesystem::remove_all(p, ec); }
};

// RTCinit @0200, RTCwait @0220, RTCread @0240 (Ablage ab 3000h), RTCset @0280
const std::vector<uint8_t> INIT = {0x01, RTC, 0x0F, 0x3E, 0x05, 0xED, 0x79, 0x3D, 0xED, 0x79,
                                   0x05, 0x05, 0xAF, 0xED, 0x79, 0xC9};
const std::vector<uint8_t> WAIT = {0x01, RTC, 0x0D, 0x3E, 0x01, 0xED, 0x79, 0xED, 0x78, 0xCB, 0x4F,
                                   0xC8, 0xAF, 0xED, 0x79, 0x18, 0xEF};
const std::vector<uint8_t> READ = {0xCD, 0x20, 0x02, 0x21, 0x00, 0x30, 0x06, 0x0B,
                                   0xED, 0x78, 0xE6, 0x0F, 0x07, 0x07, 0x07, 0x07, 0x57, 0x05,
                                   0xED, 0x78, 0xE6, 0x0F, 0xB2, 0x77, 0x23, 0x05, 0xF2, 0x48, 0x02,
                                   0x06, 0x0D, 0xAF, 0xED, 0x79, 0xC9};
const std::vector<uint8_t> SET = {0xCD, 0x20, 0x02, 0x21, 0x00, 0x30, 0x06, 0x0B,
                                  0x7E, 0x0F, 0x0F, 0x0F, 0x0F, 0xE6, 0x0F, 0xED, 0x79,
                                  0x7E, 0xE6, 0x0F, 0x05, 0xED, 0x79, 0x23, 0x05, 0xF2, 0x88, 0x02,
                                  0x06, 0x0D, 0xAF, 0xED, 0x79, 0xC9};

struct Rig {
    K1520Bus bus;
    Gide gide;
    Z80 cpu;
    std::array<uint8_t, 65536> ram{};
    uint64_t takte = 0;

    explicit Rig(const std::string& zustand = "", int64_t jetzt = T0, Gide::Config c = {}) : gide(c) {
        gide.attachToBus(bus);
        Gide::RtcConfig rc;
        rc.zustand = zustand;
        rc.takt_hz = HZ;
        rc.zeitquelle = [jetzt] { return jetzt; };
        gide.rtcAnlegen(rc);
        gide.takt(0);
        auto laden = [&](unsigned a, const std::vector<uint8_t>& v) { std::copy(v.begin(), v.end(), ram.begin() + a); };
        laden(0x200, INIT); laden(0x220, WAIT); laden(0x240, READ); laden(0x280, SET);
        cpu.readByte  = [this](uint16_t a) { return ram[a]; };
        cpu.writeByte = [this](uint16_t a, uint8_t d) { ram[a] = d; };
        cpu.readPort  = [this](uint16_t p) { return bus.ioRead(p); };
        cpu.writePort = [this](uint16_t p, uint8_t d) { bus.ioWrite(p, d); };
    }
    /// Ruft die Gastroutine @p adr auf (CALL adr; HALT bei 0100h); Maschinenzeit laeuft mit.
    bool rufe(uint16_t adr) {
        ram[0x100] = 0xCD; ram[0x101] = uint8_t(adr); ram[0x102] = uint8_t(adr >> 8); ram[0x103] = 0x76;
        cpu.reset();
        cpu.PC = 0x100; cpu.SP = 0xF000;
        for (int i = 0; i < 400000 && !cpu.halted; ++i) { takte += uint64_t(cpu.step()); gide.takt(takte); }
        return cpu.halted;
    }
    void weiter(double sek) { takte += uint64_t(sek * HZ); gide.takt(takte); }
    void chip(uint8_t reg, uint8_t d) { bus.ioWrite(uint16_t(reg << 8 | RTC), d); }
    uint8_t chip(uint8_t reg) { return bus.ioRead(uint16_t(reg << 8 | RTC)); }
    /// BCD-Zeit [Jahr, Monat, Tag, Std, Min, Sek] in den Speicher legen und mit RTCset (STOP drumherum) stellen.
    void stelle(std::array<uint8_t, 6> t) {
        std::copy(t.begin(), t.end(), ram.begin() + 0x3000);
        ASSERT_TRUE(rufe(0x280));         // Rehs RTCset unveraendert: nur HOLD, nie STOP
    }
    std::array<uint8_t, 6> lies() {
        std::fill(ram.begin() + 0x3000, ram.begin() + 0x3006, 0xEE);
        EXPECT_TRUE(rufe(0x240));
        std::array<uint8_t, 6> t;
        std::copy(ram.begin() + 0x3000, ram.begin() + 0x3006, t.begin());
        return t;
    }
};
using Zeit = std::array<uint8_t, 6>;

}  // namespace

TEST(GideRtc, InitUndLesenLiefertWirtszeitBeimErststart) {
    Rig r;
    ASSERT_TRUE(r.rufe(0x200));                       // RTCinit
    EXPECT_EQ(r.chip(0x0F), 0x04);                    // 24h, kein Reset/Stop
    EXPECT_EQ(r.chip(0x0D) & 0x01, 0);                // kein Hold
    const Zeit t = r.lies();
    EXPECT_EQ(t[0], 0x26); EXPECT_EQ(t[1], 0x11); EXPECT_EQ(t[2], 0x10);
    EXPECT_EQ(t[3], 0x12); EXPECT_EQ(t[4], 0x34);
    EXPECT_GE(t[5], 0x56); EXPECT_LE(t[5], 0x57);     // ein paar Taktzyklen der Routine laufen mit
    EXPECT_EQ(r.chip(0x0D) & 0x01, 0);                // RTCread hat Hold geloescht
}

TEST(GideRtc, LesenLiefertReinenNibbleOhneP8000Kennung) {
    Rig r;
    ASSERT_TRUE(r.rufe(0x200));
    for (uint8_t n = 0; n < 16; ++n) EXPECT_LT(r.chip(n), 0x10) << "Register " << int(n);
}

TEST(GideRtc, RundlaufStellenUndLesenMitJahrhundertwechselInMaschinenzeit) {
    Rig r;
    ASSERT_TRUE(r.rufe(0x200));
    r.stelle({0x99, 0x12, 0x31, 0x23, 0x59, 0x50});
    Zeit t = r.lies();
    EXPECT_EQ(t[0], 0x99); EXPECT_EQ(t[1], 0x12); EXPECT_EQ(t[2], 0x31);
    EXPECT_EQ(t[3], 0x23); EXPECT_EQ(t[4], 0x59); EXPECT_GE(t[5], 0x50); EXPECT_LE(t[5], 0x51);
    r.weiter(15);                                     // Maschinenzeit, keine Wirtszeit
    t = r.lies();
    EXPECT_EQ(t[0], 0x00); EXPECT_EQ(t[1], 0x01); EXPECT_EQ(t[2], 0x01);
    EXPECT_EQ(t[3], 0x00); EXPECT_EQ(t[4], 0x00); EXPECT_GE(t[5], 0x05); EXPECT_LE(t[5], 0x06);
}

TEST(GideRtc, ZeitStehtOhneMaschinentakteUndLaeuftMitIhnen) {
    Rig r;
    r.chip(0x0F, 0x04);
    const uint8_t s0 = r.chip(0);
    EXPECT_EQ(r.chip(0), s0);                         // ohne Takte keine Aenderung
    r.weiter(3);
    EXPECT_EQ(r.chip(0), uint8_t((s0 + 3) % 10));     // Sekunden-Einer 6 -> 9
}

TEST(GideRtc, HoldHaeltDieZeitFestUndBusyWirdVonRTCwaitAbgewartet) {
    Rig r;
    ASSERT_TRUE(r.rufe(0x200));
    ASSERT_EQ(r.chip(0x0D) & 0x02, 0x02) << "BUSY direkt nach dem Start (Phase 0)";
    ASSERT_TRUE(r.rufe(0x220));                       // RTCwait: Hold setzen, Busy pruefen, ggf. loeschen, neu
    EXPECT_EQ(r.chip(0x0D) & 0x03, 0x01);             // Hold gesetzt, nicht busy
    const uint8_t s0 = r.chip(0), s1 = r.chip(1);
    r.weiter(4);
    EXPECT_EQ(r.chip(0), s0);                         // festgehalten
    EXPECT_EQ(r.chip(1), s1);
    r.chip(0x0D, 0);                                  // Hold loeschen
    EXPECT_NE(uint8_t(r.chip(1) * 10 + r.chip(0)), uint8_t(s1 * 10 + s0));
}

TEST(GideRtc, ZwoelfStundenModusZeigtPmBitImZehnerDerStunde) {
    Rig r;
    r.stelle({0x26, 0x11, 0x10, 0x13, 0x05, 0x00});
    r.chip(0x0F, 0x00);                               // 12h, laufend
    r.chip(0x0D, 0x01);
    EXPECT_EQ(r.chip(4), 1);                          // 1 Uhr
    EXPECT_EQ(r.chip(5), 4);                          // PM (Bit 2), Zehner 0
    r.chip(0x0D, 0x00);
    r.chip(0x0F, 0x04);                               // 24h
    r.chip(0x0D, 0x01);
    EXPECT_EQ(r.chip(4), 3); EXPECT_EQ(r.chip(5), 1); // 13 Uhr
}

TEST(GideRtc, KeinZuruecksprungNachNeustartUeberBatteriedatei) {
    TempDir d;
    const std::string datei = (d.p / "gide_rtc.dat").string();
    {
        Rig r(datei, T0);
        ASSERT_TRUE(r.rufe(0x200));
        r.stelle({0x27, 0x01, 0x01, 0x00, 0x00, 0x00});   // Uhr VORAUS der Wirtszeit stellen
    }                                                     // Zerstoeren sichert
    {
        std::ifstream f(datei);
        std::string kopf; std::getline(f, kopf);
        EXPECT_EQ(kopf, "K1520-GIDE-RTC 2");
    }
    {
        Rig r2(datei, T0 + 3600);                         // Wirt eine Stunde spaeter, Uhr bleibt voraus
        ASSERT_TRUE(r2.rufe(0x200));
        const Zeit t = r2.lies();
        EXPECT_EQ(t[0], 0x27); EXPECT_EQ(t[1], 0x01); EXPECT_EQ(t[2], 0x01);
        EXPECT_EQ(t[3], 0x01);                            // Stand + Abwesenheit (1 h)
    }
    {
        Rig r3(datei, T0 - 86400);                        // Wirtsuhr zurueckgestellt: negative Abwesenheit = 0
        ASSERT_TRUE(r3.rufe(0x200));
        const Zeit t = r3.lies();
        EXPECT_EQ(t[0], 0x27); EXPECT_EQ(t[1], 0x01); EXPECT_EQ(t[2], 0x01);
        EXPECT_GE(t[3], 0x01);
    }
}

TEST(GideRtc, NichtBestuecktBleibtFFh) {
    Gide::Config c; c.rtc_vorhanden = false;
    Rig r("", T0, c);
    EXPECT_EQ(r.chip(0x0D), 0xFF);
}
