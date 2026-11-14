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
#include <ctime>
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

    explicit Rig(const std::string& zustand = "", int64_t jetzt = T0, Gide::Config c = {}, int versatz = 0)
        : gide(c) {
        gide.attachToBus(bus);
        Gide::RtcConfig rc;
        rc.jahresversatz = versatz;
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

// ---- Oberflaechenwunsch 2026-11-11: Zeitversatz, schneller Lauf, PC-Uhr, Netz-Aus ----

namespace {
/// "stand=" der Batteriedatei (Wandzeit OHNE Jahresversatz); -1 = nicht lesbar.
int64_t dateiStand(const std::string& datei) {
    std::ifstream f(datei);
    std::string z;
    while (std::getline(f, z))
        if (z.compare(0, 6, "stand=") == 0) return std::stoll(z.substr(6));
    return -1;
}
}  // namespace

TEST(GideRtc, JahresversatzMinus28UndMinus40ZeigenDemGastEinAnderesJahr) {
    for (const auto& [versatz, jahr_bcd] : {std::pair<int, uint8_t>{-28, 0x98}, {-40, 0x86}, {0, 0x26}}) {
        Rig r("", T0, {}, versatz);
        ASSERT_TRUE(r.rufe(0x200));
        const Zeit t = r.lies();
        EXPECT_EQ(t[0], jahr_bcd) << "Versatz " << versatz;
        EXPECT_EQ(t[1], 0x11); EXPECT_EQ(t[2], 0x10);       // Monat/Tag bleiben (10.11.)
        EXPECT_EQ(t[3], 0x12); EXPECT_EQ(t[4], 0x34);
    }
}

TEST(GideRtc, VersatzBeimStellenWirdZurueckgerechnetUndNichtGespeichert) {
    TempDir d;
    const std::string datei = (d.p / "versatz.dat").string();
    {
        Rig r(datei, T0, {}, -28);
        ASSERT_TRUE(r.rufe(0x200));
        r.stelle({0x99, 0x03, 0x05, 0x08, 0x15, 0x00});     // Gast stellt 5.3.1999
    }
    // gespeichert wird im Jahr des Wirts: 1999 + 28 = 2027
    const int64_t stand = dateiStand(datei);
    ASSERT_GE(stand, 0);
    EXPECT_GE(stand, 1804234500) << stand;                   // 2027-03-05 08:15:00 UTC
    EXPECT_LE(stand, 1804234500 + 5);                        // (nicht 1999: dort laege es bei ~920 Mio)
}

TEST(GideRtc, ErststartStelltSichAusDerPcUhr) {
    // ohne injizierte Zeitquelle: std::time, UTC
    K1520Bus bus;
    Gide g;
    g.attachToBus(bus);
    Gide::RtcConfig rc;
    rc.takt_hz = HZ;
    g.rtcAnlegen(rc);
    const int64_t jetzt = int64_t(std::time(nullptr));
    EXPECT_NEAR(double(g.rtc()->gastZeit()), double(jetzt), 3.0);
}

TEST(GideRtc, SchnellerLaufRechnetInMaschinenzeitUndNeustartSpringtNieZurueck) {
    // Maschine 10 Stunden Maschinenzeit in einer Wirtsstunde (10fache Geschwindigkeit): die Uhr geht 10 h,
    // die Wirtszeit nur 1 h.  Nach dem Beenden und Neustart steht die Uhr bei Stand + Abwesenheit,
    // nicht bei der (kleineren) Wirtszeit.
    TempDir d;
    const std::string datei = (d.p / "schnell.dat").string();
    {
        Rig r(datei, T0);
        ASSERT_TRUE(r.rufe(0x200));
        r.weiter(10.0 * 3600);                              // 10 h Maschinenzeit
        const Zeit t = r.lies();
        EXPECT_EQ(t[3], 0x22); EXPECT_EQ(t[4], 0x34);       // 12:34 + 10 h
    }                                                       // Beenden sichert
    {
        Rig r2(datei, T0 + 3600);                           // Wirtsuhr nur 1 h weiter
        ASSERT_TRUE(r2.rufe(0x200));
        const Zeit t = r2.lies();
        EXPECT_EQ(t[1], 0x11); EXPECT_EQ(t[2], 0x10);
        EXPECT_EQ(t[3], 0x23); EXPECT_EQ(t[4], 0x34);       // 22:34 + 1 h Abwesenheit, NICHT 13:34
    }
}

TEST(GideRtc, AbsturzNachSchnellemLaufVerliertHoechstensEineMinuteMaschinenzeit) {
    TempDir d;
    const std::string datei = (d.p / "absturz.dat").string();
    const std::string kopie = (d.p / "kopie.dat").string();
    {
        Rig r(datei, T0);
        ASSERT_TRUE(r.rufe(0x200));
        r.weiter(2.0 * 3600);                               // 2 h Maschinenzeit, kein Beenden
        std::filesystem::copy_file(datei, kopie);           // Zustand der Datei beim "Absturz"
    }
    const int64_t stand = dateiStand(kopie);
    EXPECT_GE(stand, T0 + 7200 - 60);                       // je 60 s Maschinenzeit fortgeschrieben
    EXPECT_LE(stand, T0 + 7200);
    Rig r2(kopie, T0 + 5);
    ASSERT_TRUE(r2.rufe(0x200));
    const Zeit t = r2.lies();
    EXPECT_EQ(t[3], 0x14);                                  // 14:3x, nicht 12:35
}

TEST(GideRtc, NetzAusUndEinLaesstDieUhrUmDieAusDauerWeiterlaufenNieZurueck) {
    TempDir d;
    const std::string datei = (d.p / "netz.dat").string();
    int64_t jetzt = T0;
    K1520Bus bus;
    Gide g;
    g.attachToBus(bus);
    Gide::RtcConfig rc;
    rc.zustand = datei;
    rc.takt_hz = HZ;
    rc.zeitquelle = [&jetzt] { return jetzt; };
    g.rtcAnlegen(rc);
    g.takt(0);
    g.takt(uint64_t(5000) * HZ);                            // 5000 s Maschinenzeit (schnell): Uhr = T0 + 5000
    ASSERT_EQ(g.rtc()->gastZeit(), T0 + 5000);
    g.rtc()->netzAus();
    jetzt = T0 + 100;                                       // Wirt: nur 100 s vergangen -> Uhr bleibt VOR der Wirtszeit
    g.powerOn();
    EXPECT_EQ(g.rtc()->gastZeit(), T0 + 5100);              // Stand + Abwesenheit
    g.rtc()->netzAus();
    jetzt = T0 + 90000;                                     // 89900 s weg: Stand + Abwesenheit, Wirtszeit liegt darunter
    g.powerOn();
    EXPECT_EQ(g.rtc()->gastZeit(), T0 + 5100 + 89900);
    g.rtc()->netzAus();
    jetzt = T0 + 400000;                                    // sehr lange weg: Stand + Abwesenheit bleibt VOR der Wirtszeit
    g.powerOn();
    EXPECT_EQ(g.rtc()->gastZeit(), T0 + 5100 + 89900 + 310000);
    g.rtc()->netzAus();
    jetzt = T0 - 5000;                                      // Wirtsuhr zurueckgestellt: Abwesenheit 0
    g.powerOn();
    EXPECT_GE(g.rtc()->gastZeit(), T0 + 5100 + 89900 + 310000);   // nie zurueck
    // ohne Netz-Aus davor (Kaltstart der laufenden Maschine) bleibt die Uhr unberuehrt
    const int64_t vorher = g.rtc()->gastZeit();
    g.powerOn();
    EXPECT_EQ(g.rtc()->gastZeit(), vorher);
}

TEST(GideRtc, StartzeitStatischMitGideKopf) {
    TempDir d;
    const std::string datei = (d.p / "start.dat").string();
    {
        Rig r(datei, T0);
        ASSERT_TRUE(r.rufe(0x200));
        r.weiter(1000);
    }
    const auto zq = [] { return T0 + 50; };
    // mit dem GIDE-Kopf erkannt: Stand + Abwesenheit; mit dem P8000-Kopf unbekannt -> Wirtszeit
    EXPECT_GE(P8000Rtc::startzeit(datei, 0, true, zq, Gide::RTC_KOPF), T0 + 1050 - 2);
    EXPECT_EQ(P8000Rtc::startzeit(datei, 0, true, zq), T0 + 50);
}
