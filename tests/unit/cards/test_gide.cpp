/**
 * @file test_gide.cpp
 * @brief GIDE-Karte (Entwurf 50 Abschnitt 3, AP G2): Portdekodierung, Wechsel-Flipflop LH, Rehs
 *        Zugriffsmuster (HDInit, Sektor lesen/schreiben mit INIR/OTIR x2) mit echtem Z80, RTC-Hook.
 */
#include <gtest/gtest.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "core/cards/gide/gide.h"
#include "core/primitives/z80.h"

using k1520::AtaGeraet;
using k1520::Gide;

namespace {

/// Temporaeres Verzeichnis, wird am Ende samt Inhalt entfernt (Mitglied VOR der Rig deklarieren).
struct TempDir {
    std::filesystem::path p;
    TempDir() {
        static std::atomic<int> n{0};
        p = std::filesystem::temp_directory_path() /
            ("k1520_gide_test_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
        std::filesystem::create_directories(p);
    }
    ~TempDir() { std::error_code ec; std::filesystem::remove_all(p, ec); }
};

/// Platzhalter-Karte, die Zugriffe nur zaehlt.
struct Sonde : BusDevice {
    int lesen = 0, schreiben = 0;
    uint8_t ioRead(uint8_t) override { ++lesen; return 0x5A; }
    void ioWrite(uint8_t, uint8_t) override { ++schreiben; }
    const char* deviceName() const override { return "Sonde"; }
};

struct Rig {
    TempDir dir;
    K1520Bus bus;
    Gide gide;
    Z80 cpu;
    std::array<uint8_t, 65536> ram{};
    uint8_t basis;

    explicit Rig(Gide::Config c = {}, bool mit_platte = true) : gide(c), basis(c.basis) {
        gide.attachToBus(bus);
        if (mit_platte) {
            const auto pfad = (dir.p / "platte.img").string();
            std::string f;
            EXPECT_TRUE(AtaGeraet::abbildAnlegen(pfad, 256, &f)) << f;
            EXPECT_TRUE(gide.platte().abbildOeffnen(pfad, AtaGeraet::Geometrie(), &f)) << f;
        }
        cpu.readByte  = [this](uint16_t a) { return ram[a]; };
        cpu.writeByte = [this](uint16_t a, uint8_t d) { ram[a] = d; };
        cpu.readPort  = [this](uint16_t p) { return bus.ioRead(p); };
        cpu.writePort = [this](uint16_t p, uint8_t d) { bus.ioWrite(p, d); };
    }

    void run(const std::vector<uint8_t>& code, uint16_t bc = 0, uint16_t hl = 0, uint8_t a = 0) {
        for (size_t i = 0; i < code.size(); ++i) ram[0x100 + i] = code[i];
        cpu.reset();
        cpu.PC = 0x100; cpu.BC = bc; cpu.HL = hl; cpu.A = a;
        for (int i = 0; i < 200000 && !cpu.halted; ++i) cpu.step();
        ASSERT_TRUE(cpu.halted) << "Programm hat nicht angehalten";
    }
    /// OUT (C),A mit B als A8..A15.
    void out(unsigned off, uint8_t wert, uint8_t b = 0) { run({0xED, 0x79, 0x76}, uint16_t(b << 8 | uint8_t(basis + off)), 0, wert); }
    /// IN A,(C).
    uint8_t in(unsigned off, uint8_t b = 0) { run({0xED, 0x78, 0x76}, uint16_t(b << 8 | uint8_t(basis + off))); return cpu.A; }

    void warteBsyAus() {
        for (int i = 0; i < 100; ++i) if (!(in(0xF) & 0x80)) return;
        FAIL() << "BSY bleibt";
    }
    bool warteDrq() {
        for (int i = 0; i < 100; ++i) if ((in(0xF) & 0x88) == 0x08) return true;
        return false;
    }

    /// Rehs HDInit: DOR 06h/02h, BSY-Wait, Task-File, Befehl 91h.
    void hdInit() {
        out(6, 0x06); out(6, 0x02);
        warteBsyAus();
        const auto g = gide.platte().geometrie();
        out(0xA, uint8_t(g.sektoren));
        out(0xC, 0); out(0xD, 0);
        out(0xE, uint8_t(0xA0 | (g.koepfe - 1)));
        out(0xF, 0x91);
        warteBsyAus();
    }
    void taskFile(uint8_t sn, uint8_t cl, uint8_t ch, uint8_t kopf) {
        out(0xA, 1); out(0xB, sn); out(0xC, cl); out(0xD, ch); out(0xE, uint8_t(0xA0 | kopf));
    }
    /// Sektor lesen wie gideide.mac: 20h, DRQ, INIR x2, BSY-Wait, Status & 89h == 0.
    std::vector<uint8_t> sektorLesen(uint8_t sn, uint8_t cl, uint8_t ch, uint8_t kopf) {
        taskFile(sn, cl, ch, kopf);
        out(0xF, 0x20);
        EXPECT_TRUE(warteDrq());
        run({0xED, 0xB2, 0xED, 0xB2, 0x76}, uint16_t(0 << 8 | uint8_t(basis + 8)), 0x4000);
        warteBsyAus();
        EXPECT_EQ(in(0xF) & 0x89, 0);
        return std::vector<uint8_t>(ram.begin() + 0x4000, ram.begin() + 0x4200);
    }
    void sektorSchreiben(uint8_t sn, uint8_t cl, uint8_t ch, uint8_t kopf, const std::vector<uint8_t>& d) {
        taskFile(sn, cl, ch, kopf);
        out(0xF, 0x30);
        ASSERT_TRUE(warteDrq());
        std::copy(d.begin(), d.end(), ram.begin() + 0x5000);
        run({0xED, 0xB3, 0xED, 0xB3, 0x76}, uint16_t(0 << 8 | uint8_t(basis + 8)), 0x5000);
        warteBsyAus();
        EXPECT_EQ(in(0xF) & 0x89, 0);
    }
};

std::vector<uint8_t> muster(uint8_t start = 3) {
    std::vector<uint8_t> m(512);
    for (int i = 0; i < 512; ++i) m[i] = uint8_t(start + i * 7 + (i >> 8) * 0x55);
    return m;
}

/// Vorbelegen: Sektor (kopf 0, sn 1) mit Muster ueber den normalen Weg.
void vorbelegen(Rig& r, const std::vector<uint8_t>& m) {
    r.hdInit();
    r.sektorSchreiben(1, 0, 0, 0, m);
}

}  // namespace

TEST(Gide, NurBasisPlus4BisFAntworten) {
    K1520Bus bus;
    Sonde nachbar_unten, nachbar_oben, innen;
    bus.registerIO(&nachbar_unten, 0x8F, 1);
    bus.registerIO(&innen, 0x90, 4);          // +0..+3 gehoeren nicht der Karte
    bus.registerIO(&nachbar_oben, 0xA0, 1);
    Gide g;
    g.attachToBus(bus);
    for (unsigned p : {0x8Fu, 0x90u, 0x93u, 0xA0u}) { bus.ioRead(uint16_t(p)); bus.ioWrite(uint16_t(p), 1); }
    EXPECT_EQ(nachbar_unten.lesen + nachbar_unten.schreiben, 2);
    EXPECT_EQ(innen.lesen + innen.schreiben, 4);
    EXPECT_EQ(nachbar_oben.lesen + nachbar_oben.schreiben, 2);
    EXPECT_EQ(g.zaehler().taskfile + g.zaehler().daten_lesen, 0u);
    for (unsigned p = 0x94; p <= 0x9F; ++p) EXPECT_EQ(bus.ioOwner(uint8_t(p)), &g) << p;
    EXPECT_EQ(bus.ioOwner(0xA0), &nachbar_oben);
    // Direkter Aufruf ausserhalb: FFh, keine Wirkung
    EXPECT_EQ(g.ioRead(0x93), 0xFF);
    g.ioWrite(0x93, 0xAB);
    EXPECT_EQ(g.zaehler().taskfile, 0u);
}

TEST(Gide, ExpPortOhneWirkung) {
    Rig r({}, false);
    EXPECT_EQ(r.in(4), 0xFF);
    r.out(4, 0x12);
    EXPECT_FALSE(r.gide.lh());
    EXPECT_EQ(r.gide.letzterBefehl(), 0);
}

TEST(Gide, BasisA0) {
    Gide::Config c; c.basis = 0xA0;
    Rig r(c);
    EXPECT_NE(r.bus.ioOwner(0xA4), nullptr);
    EXPECT_EQ(r.bus.ioOwner(0x94), nullptr);
    r.hdInit();
    const auto m = muster();
    r.sektorSchreiben(2, 0, 0, 0, m);
    EXPECT_EQ(r.sektorLesen(2, 0, 0, 0), m);
}

TEST(Gide, PortkonfliktWirftUndBasisIstVielfachesVon16) {
    K1520Bus bus;
    Gide a;
    a.attachToBus(bus);
    Gide b;
    EXPECT_NE(b.belegtGrund(bus), "");
    EXPECT_NE(b.belegtGrund(bus).find("GIDE"), std::string::npos);
    EXPECT_THROW(b.attachToBus(bus), std::runtime_error);
    Gide::Config c; c.basis = 0xA0;
    Gide ok(c);
    EXPECT_EQ(ok.belegtGrund(bus), "");
    EXPECT_NO_THROW(ok.attachToBus(bus));
    Gide::Config schief; schief.basis = 0x95;
    EXPECT_THROW(Gide{schief}, std::invalid_argument);
}

TEST(Gide, RundlaufHdInitSchreibenLesenByteweiseGleich) {
    Rig r;
    r.hdInit();
    const auto m = muster();
    r.sektorSchreiben(3, 0, 0, 1, m);
    EXPECT_EQ(r.sektorLesen(3, 0, 0, 1), m);
    // Nachbar unberuehrt
    EXPECT_NE(r.sektorLesen(2, 0, 0, 1), m);
    EXPECT_EQ(r.gide.letzterBefehl(), 0x20);
    EXPECT_GT(r.gide.zaehler().daten_schreiben, 0u);
}

TEST(Gide, LowByteZuerst) {
    Rig r;
    const auto m = muster();
    vorbelegen(r, m);
    // Host liest dasselbe Wort direkt von der Platte: Low = erstes Byte.
    r.taskFile(1, 0, 0, 0);
    r.out(0xF, 0x20);
    ASSERT_TRUE(r.warteDrq());
    EXPECT_EQ(r.in(8), m[0]);    // 1. Zugriff: Low
    EXPECT_EQ(r.in(8), m[1]);    // 2. Zugriff: High aus dem Merker
    EXPECT_EQ(r.in(8), m[2]);
}

TEST(Gide, TaskFileZugriffMittenImSektorSetztFolgeZurueck) {
    Rig r;
    const auto m = muster();
    vorbelegen(r, m);
    r.taskFile(1, 0, 0, 0);
    r.out(0xF, 0x20);
    ASSERT_TRUE(r.warteDrq());
    EXPECT_EQ(r.in(8), m[0]);
    EXPECT_EQ(r.in(8), m[1]);
    EXPECT_EQ(r.in(8), m[2]);
    EXPECT_TRUE(r.gide.lh());
    r.in(0xA);                              // Task-File: LH := 0
    EXPECT_FALSE(r.gide.lh());
    EXPECT_EQ(r.in(8), m[4]);               // Wort 2 Low; m[3] ging verloren
    // Auch bei LH=0 kippt ein Task-File-Zugriff nicht, er setzt auf 0 (Lesen und Schreiben).
    EXPECT_EQ(r.in(8), m[5]);
    ASSERT_FALSE(r.gide.lh());
    r.in(0xA);
    EXPECT_FALSE(r.gide.lh());
    r.out(0xA, 1);
    EXPECT_FALSE(r.gide.lh());
}

TEST(Gide, ZugriffAufCs1AendertLhNicht) {
    Rig r;
    const auto m = muster();
    vorbelegen(r, m);
    r.taskFile(1, 0, 0, 0);
    r.out(0xF, 0x20);
    ASSERT_TRUE(r.warteDrq());
    EXPECT_EQ(r.in(8), m[0]);
    EXPECT_EQ(r.in(8), m[1]);
    EXPECT_EQ(r.in(8), m[2]);
    ASSERT_TRUE(r.gide.lh());
    r.in(6); r.in(7); r.out(6, 0x02); r.in(4); r.out(4, 0); r.in(5);
    EXPECT_TRUE(r.gide.lh());
    EXPECT_EQ(r.in(8), m[3]);               // High-Byte aus dem Merker
}

TEST(Gide, UngeradeZugriffszahlLiestUndSchreibtNichtVorzeitig) {
    Rig r;
    const auto m = muster();
    vorbelegen(r, m);
    // Schreiben mit 511 Bytes: Sektor bleibt unveraendert, DRQ steht noch.
    const auto neu = muster(0x90);
    r.taskFile(1, 0, 0, 0);
    r.out(0xF, 0x30);
    ASSERT_TRUE(r.warteDrq());
    for (int i = 0; i < 511; ++i) r.out(8, neu[i]);
    EXPECT_TRUE(r.gide.lh());               // 511 = ungerade
    EXPECT_TRUE(r.gide.platte().drq());
    r.out(8, neu[511]);                     // 512. Byte schliesst das letzte Wort
    EXPECT_FALSE(r.gide.lh());
    r.warteBsyAus();
    EXPECT_EQ(r.sektorLesen(1, 0, 0, 0), neu);
}

TEST(Gide, RtcHookRegisterNummerAusA8bisA11) {
    Rig r({}, false);
    EXPECT_EQ(r.in(5, 0x0D), 0xFF);         // kein Hook
    r.out(5, 0x77, 0x0D);                   // verschluckt
    std::vector<std::pair<int, int>> w;
    r.gide.rtcLese = [](uint8_t reg) { return uint8_t(0x40 + reg); };
    r.gide.rtcSchreibe = [&](uint8_t reg, uint8_t d) { w.emplace_back(reg, d); };
    EXPECT_EQ(r.in(5, 0x0D), 0x4D);
    EXPECT_EQ(r.in(5, 0xA3), 0x43);         // A12-A15 unbeachtet
    r.out(5, 0x05, 0x0F);
    ASSERT_EQ(w.size(), 1u);
    EXPECT_EQ(w[0], std::make_pair(0x0F, 0x05));
    EXPECT_FALSE(r.gide.lh());              // RTC beruehrt LH nicht
}

TEST(Gide, RtcNichtBestueckt_IgnoriertDenHook) {
    Gide::Config c; c.rtc_vorhanden = false;
    Rig r(c, false);
    int n = 0;
    r.gide.rtcLese = [&](uint8_t) { ++n; return uint8_t(0); };
    r.gide.rtcSchreibe = [&](uint8_t, uint8_t) { ++n; };
    EXPECT_EQ(r.in(5, 3), 0xFF);
    r.out(5, 1, 3);
    EXPECT_EQ(n, 0);
}

TEST(Gide, ResetSetztLhZurueckMediumBleibt) {
    Rig r;
    const auto m = muster();
    vorbelegen(r, m);
    r.taskFile(1, 0, 0, 0);
    r.out(0xF, 0x20);
    ASSERT_TRUE(r.warteDrq());
    r.in(8);
    ASSERT_TRUE(r.gide.lh());
    r.gide.reset();
    EXPECT_FALSE(r.gide.lh());
    EXPECT_TRUE(r.gide.platte().mediumVorhanden());
    EXPECT_FALSE(r.gide.platte().drq());
    r.hdInit();
    EXPECT_EQ(r.sektorLesen(1, 0, 0, 0), m);
}
