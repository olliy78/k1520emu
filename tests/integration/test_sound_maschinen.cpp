/**
 * @file test_sound_maschinen.cpp
 * @brief SOUND-Karte K1520 in den Maschinen (Entwurf 51, AP S4): Stecken/Fehlerfaelle je Maschine, P8000 lehnt ab,
 *        und je A5120 und K8915 ein ECHTES Z80-Gastprogramm: die CTC der Karte liefert ~50 Interrupts je Sekunde
 *        Maschinenzeit (IM 2), und ein AY-Ton von ~1008 Hz erscheint in der WAV-Mitschrift.
 *
 * Tonausgabe immer ueber K1520_AUDIO=stumm (kein Wirtsgeraet).  Gastcode laeuft im RAM (4000H), ohne Boot.
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/k8915/k8915.h"
#include "core/machines/mc80/mc80.h"
#include "core/machines/mc80/mc80_2x.h"
#include "core/machines/p8000/p8000.h"
#include "core/machines/pc1715/pc1715.h"
#include "core/machines/prg710/prg710.h"
#include "core/util/os_compat.h"
#include "tests/support/temp_path.h"

namespace {

using k1520::SoundK1520;
using Bytes = std::vector<uint8_t>;

struct SoundMaschine : ::testing::Test {
    void SetUp() override {
        k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN);
        k1520::os::setEnv("K1520_AUDIO", "stumm");
    }
};

struct Belegt : BusDevice {
    uint8_t ioRead(uint8_t) override { return 0xFF; }
    void ioWrite(uint8_t, uint8_t) override {}
    const char* deviceName() const override { return "Stoerer"; }
};

SoundK1520::Config cfg(uint8_t basis) {
    SoundK1520::Config c;
    c.basis = basis;
    return c;
}

/// Bestueckungsfaelle je Maschine; @p basis frei an dieser Maschine.
template <class Neu>
void pruefeBestueckung(Neu neu, uint8_t basis) {
    const SoundK1520::Config c = cfg(basis);
    {   // Stecken, doppelt, Reset/Lauf behalten die Karte; Tore +0,+1,+4..+7 belegt, +2/+3 nicht
        auto m = neu();
        ASSERT_TRUE(m->installSound(c)) << m->soundFehler();
        ASSERT_NE(m->soundK1520(), nullptr);
        EXPECT_EQ(m->soundK1520()->config().systemtakt_hz, m->nenntaktHz()) << "Maschinentakt immer aus der Maschine";
        for (int i : {0, 1, 4, 5, 6, 7}) EXPECT_NE(m->bus().ioOwner(uint8_t(basis + i)), nullptr) << i;
        EXPECT_EQ(m->bus().ioOwner(uint8_t(basis + 2)), nullptr);
        EXPECT_FALSE(m->installSound(c));
        EXPECT_NE(m->soundFehler().find("bereits"), std::string::npos) << m->soundFehler();
        m->powerOn();
        m->run(100);
        m->reset();
        EXPECT_NE(m->soundK1520(), nullptr);
    }
    {   // Zu spaet
        auto m = neu();
        m->powerOn();
        m->run(100);
        EXPECT_FALSE(m->installSound(c));
        EXPECT_NE(m->soundFehler().find("vor dem ersten"), std::string::npos) << m->soundFehler();
        EXPECT_EQ(m->soundK1520(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(basis), nullptr);
    }
    {   // Tor belegt: false mit Grund, KEIN Tor angemeldet
        auto m = neu();
        Belegt st;
        m->bus().registerIO(&st, uint8_t(basis + 5), 1);
        EXPECT_FALSE(m->installSound(c));
        EXPECT_NE(m->soundFehler().find("belegt"), std::string::npos) << m->soundFehler();
        EXPECT_EQ(m->soundK1520(), nullptr);
        EXPECT_EQ(m->bus().ioOwner(basis), nullptr) << "halb angemeldete Karte";
    }
    {   // Ungueltige Basis (3CH ist kein Vielfaches von 8)
        auto m = neu();
        EXPECT_FALSE(m->installSound(cfg(0x3C)));
        EXPECT_FALSE(m->soundFehler().empty());
        EXPECT_EQ(m->soundK1520(), nullptr);
    }
    {   // Ohne Karte: Ausgabe-Aufrufe sind definiert, kein Absturz
        auto m = neu();
        EXPECT_FALSE(m->audioStart());
        EXPECT_FALSE(m->audioFehler().empty());
        EXPECT_FALSE(m->audioWavStart(k1520test::tempPath("sound_ohne_karte.wav")));
        m->audioStop();
        m->audioWavStop();
        m->audioLautstaerke(0.5f);
        m->audioStumm(true);
        m->audioEchtzeit(false);
        EXPECT_EQ(m->audioStatus(), k1520::audio::TonAusgabe::Status::Aus);
    }
}

}  // namespace

TEST_F(SoundMaschine, Bestueckung_A5120) { pruefeBestueckung([] { return std::make_unique<A5120Machine>(); }, 0x38); }
TEST_F(SoundMaschine, Bestueckung_K8915) { pruefeBestueckung([] { return std::make_unique<K8915Machine>(); }, 0x38); }
TEST_F(SoundMaschine, Bestueckung_Prg710) {
    pruefeBestueckung([] {
        Prg710Machine::Config c;
        c.variante = Prg710Machine::Config::Variante::Prg710_1;
        return std::make_unique<Prg710Machine>(c);
    }, 0x38);
}
TEST_F(SoundMaschine, Bestueckung_Pc1715) { pruefeBestueckung([] { return std::make_unique<Pc1715Machine>(); }, 0x38); }
TEST_F(SoundMaschine, Bestueckung_Pc1715W) {
    pruefeBestueckung([] {
        Pc1715Machine::Config c;
        c.variante = Pc1715Machine::Config::Variante::Pc1715W;
        return std::make_unique<Pc1715Machine>(c);
    }, 0xE0);
}
TEST_F(SoundMaschine, Bestueckung_Mc80) { pruefeBestueckung([] { return std::make_unique<Mc80Machine>(); }, 0x38); }
TEST_F(SoundMaschine, Bestueckung_Mc80_2x) { pruefeBestueckung([] { return std::make_unique<Mc80_2xMachine>(); }, 0x38); }

// PC 1715W: 38H..3FH gehoeren der Senke "PC1715W-E/A" -> 38H meldet Konflikt mit Grund.
TEST_F(SoundMaschine, Pc1715W_38HIstBelegt) {
    Pc1715Machine::Config c;
    c.variante = Pc1715Machine::Config::Variante::Pc1715W;
    Pc1715Machine m(c);
    EXPECT_FALSE(m.installSound(cfg(0x38)));
    EXPECT_NE(m.soundFehler().find("belegt"), std::string::npos) << m.soundFehler();
    EXPECT_NE(m.soundFehler().find("PC1715W"), std::string::npos) << m.soundFehler();
}

// Umstellen im Lauf: ohne Karte false mit Grund; mit Karte wirkt es ohne Neuaufbau, Basis bleibt fest.
TEST_F(SoundMaschine, SoundUmstellenOhneKarte) {
    A5120Machine m;
    EXPECT_FALSE(m.soundUmstellen(cfg(0x38)));
    EXPECT_NE(m.soundFehler().find("Keine SOUND-Karte"), std::string::npos) << m.soundFehler();
}

TEST_F(SoundMaschine, SoundUmstellenImLauf) {
    A5120Machine m;
    ASSERT_TRUE(m.installSound(cfg(0x38)));
    m.powerOn();
    m.run(1000);
    k1520::SoundK1520* vor = m.soundK1520();
    SoundK1520::Config c = cfg(0x38);
    c.takt = SoundK1520::Takt::System;
    c.kanal = SoundK1520::Kanal::Mono;
    ASSERT_TRUE(m.soundUmstellen(c)) << m.soundFehler();
    EXPECT_EQ(m.soundK1520(), vor);
    EXPECT_EQ(m.soundK1520()->config().takt, SoundK1520::Takt::System);
    EXPECT_EQ(m.soundK1520()->config().systemtakt_hz, m.nenntaktHz());
    EXPECT_EQ(m.soundK1520()->ay().chipTakt(), m.nenntaktHz());
    EXPECT_FALSE(m.soundUmstellen(cfg(0x40)));
    EXPECT_NE(m.soundFehler().find("Basis"), std::string::npos) << m.soundFehler();
    m.run(1000);
}

// P8000: kein K1520-Steckplatz.
TEST_F(SoundMaschine, P8000LehntAb) {
    P8000Machine m;
    EXPECT_FALSE(m.installSound(cfg(0x38)));
    EXPECT_NE(m.soundFehler().find("Steckplatz"), std::string::npos) << m.soundFehler();
    EXPECT_EQ(m.soundK1520(), nullptr);
}

namespace {

constexpr uint16_t kCode = 0x4000, kIsr = 0x4100, kZaehler = 0x7002;

void ein(Bytes& p, std::initializer_list<uint8_t> b) { p.insert(p.end(), b); }
void poke(K1520Machine& m, uint16_t a, const Bytes& b) {
    for (size_t i = 0; i < b.size(); ++i) m.memWriteDebug(uint16_t(a + i), b[i]);
}
void ausg(Bytes& p, uint8_t port, uint8_t v) { ein(p, {0x3E, v, 0xD3, port}); }   // LD A,n ; OUT (port),A

/// Maschinenzeit in Takten fahren (Scheiben von 5000 Takten).
void fahreTakte(K1520Machine& m, long long takte) {
    for (long long t = 0; t < takte; t += m.run(5000)) {}
}

/// CTC der Karte (Basis+4) auf 50 Hz, IM 2, ISR zaehlt; nach 1 s Maschinenzeit ~50 Interrupts.
void ctcLaeuft(K1520Machine& m, Z80& cpu, uint8_t basis, std::function<void()> nachEin = {}) {
    SoundK1520::Config c = cfg(basis);
    ASSERT_TRUE(m.installSound(c)) << m.soundFehler();
    m.powerOn();
    if (nachEin) nachEin();
    // Vektortabelle (I = 60H): 60D8H -> ISR
    m.memWriteDebug(0x60D8, uint8_t(kIsr)); m.memWriteDebug(0x60D9, uint8_t(kIsr >> 8));
    m.memWriteDebug(kZaehler, 0); m.memWriteDebug(kZaehler + 1, 0);
    Bytes isr;
    ein(isr, {0xE5, 0x2A, uint8_t(kZaehler), uint8_t(kZaehler >> 8), 0x23, 0x22, uint8_t(kZaehler), uint8_t(kZaehler >> 8),
              0xE1, 0xFB, 0xED, 0x4D});                      // PUSH HL ; LD HL,(Z) ; INC HL ; LD (Z),HL ; POP HL ; EI ; RETI
    poke(m, kIsr, isr);
    Bytes p;
    ein(p, {0xF3, 0x31, 0x00, 0x7F, 0x3E, 0x60, 0xED, 0x47, 0xED, 0x5E});   // DI ; LD SP,7F00 ; LD A,60H ; LD I,A ; IM 2
    ausg(p, uint8_t(basis + 4), 0xD8);                       // Vektor
    ausg(p, uint8_t(basis + 4), 0xA7);                       // Zeitgeber, Vorteiler 256, Interrupt an, ZK folgt
    ausg(p, uint8_t(basis + 4), 0xC0);                       // 2457600 / 256 / 192 = 50 Hz
    ein(p, {0xFB, 0x18, 0xFE});                              // EI ; JR $
    poke(m, kCode, p);
    cpu.PC = kCode;
    cpu.halted = false;
    fahreTakte(m, 1000);                                     // Programmieren abwarten, dann den Zaehler nullen
    m.memWriteDebug(kZaehler, 0); m.memWriteDebug(kZaehler + 1, 0);
    fahreTakte(m, m.nenntaktHz());
    const int n = m.memReadDebug(kZaehler) | (m.memReadDebug(kZaehler + 1) << 8);
    EXPECT_NEAR(n, 50, 1) << "Interrupts in 1 s Maschinenzeit";
}

/// AY-Ton A (Periode 111 bei Q1/2 = 1789772 Hz: 1007,8 Hz) -> WAV, Frequenz aus Nulldurchgaengen um den Mittelwert.
void tonInWav(K1520Machine& m, Z80& cpu, uint8_t basis, const char* was, std::function<void()> nachEin = {}) {
    ASSERT_TRUE(m.installSound(cfg(basis))) << m.soundFehler();
    m.powerOn();
    if (nachEin) nachEin();
    const std::string pfad = k1520test::tempPath(std::string("sound_") + was + ".wav");
    ASSERT_TRUE(m.audioWavStart(pfad)) << m.audioFehler();
    Bytes p;
    ein(p, {0xF3, 0x31, 0x00, 0x7F});
    auto reg = [&](uint8_t r, uint8_t v) { ausg(p, basis, r); ausg(p, uint8_t(basis + 1), v); };
    reg(0, 111); reg(1, 0); reg(7, 0x3E); reg(8, 15);        // Ton A an, Rauschen/B/C aus, Lautstaerke 15
    ein(p, {0x18, 0xFE});
    poke(m, kCode, p);
    cpu.PC = kCode;
    cpu.halted = false;
    fahreTakte(m, m.nenntaktHz() + m.nenntaktHz() / 5);      // 1,2 s (die erste 0,2 s laesst den Gleichanteilfilter einschwingen)
    m.audioWavStop();

    std::ifstream f(pfad, std::ios::binary);
    ASSERT_TRUE(f.good());
    std::vector<char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    std::error_code ec;
    std::filesystem::remove(pfad, ec);
    ASSERT_GT(d.size(), 44u + 4u * 40000u) << "WAV zu kurz";
    const size_t frames = (d.size() - 44) / 4;
    std::vector<double> l;
    for (size_t i = 9600; i < frames; ++i) {                 // erste 0,2 s auslassen
        int16_t v;
        std::memcpy(&v, &d[44 + 4 * i], 2);
        l.push_back(v);
    }
    double mittel = 0;
    for (double v : l) mittel += v;
    mittel /= double(l.size());
    int nulldurchgaenge = 0;
    size_t erster = 0, letzter = 0;
    for (size_t i = 1; i < l.size(); ++i)
        if (l[i - 1] < mittel && l[i] >= mittel) { if (!nulldurchgaenge) erster = i; letzter = i; ++nulldurchgaenge; }
    ASSERT_GT(nulldurchgaenge, 100) << "kein Ton in der WAV";
    const double hz = double(nulldurchgaenge - 1) * 48000.0 / double(letzter - erster);
    EXPECT_NEAR(hz, 1007.8, 1007.8 * 0.005) << was;
}

}  // namespace

TEST_F(SoundMaschine, A5120_CtcLiefert50Hz) {
    A5120Machine m;
    ctcLaeuft(m, m.cpuDebug(), 0x38);
}
TEST_F(SoundMaschine, K8915_CtcLiefert50Hz) {
    K8915Machine m;
    ctcLaeuft(m, m.zre().cpu(), 0x38, [&] { m.zre().setReg(0x87); });   // alles RAM (nach powerOn)
}
TEST_F(SoundMaschine, A5120_TonInWav) {
    A5120Machine m;
    tonInWav(m, m.cpuDebug(), 0x38, "a5120");
}
TEST_F(SoundMaschine, K8915_TonInWav) {
    K8915Machine m;
    tonInWav(m, m.zre().cpu(), 0x38, "k8915", [&] { m.zre().setReg(0x87); });
}

// Ohne Wirtsausgabe und ohne WAV erzeugt die Karte keine Abtastwerte (keine Senke); mit WAV schon.
TEST_F(SoundMaschine, SenkeNurBeiBedarf) {
    A5120Machine m;
    ASSERT_TRUE(m.installSound(cfg(0x38))) << m.soundFehler();
    EXPECT_EQ(m.soundK1520()->senke(), nullptr);
    const std::string pfad = k1520test::tempPath("sound_senke.wav");
    ASSERT_TRUE(m.audioWavStart(pfad)) << m.audioFehler();
    EXPECT_NE(m.soundK1520()->senke(), nullptr);
    m.audioWavStop();
    EXPECT_EQ(m.soundK1520()->senke(), nullptr);
    std::error_code ec;
    std::filesystem::remove(pfad, ec);
    // Stumm-Backend: Start gelingt, Status Stumm, nichts hoerbar -> weiter keine Senke
    ASSERT_TRUE(m.audioStart()) << m.audioFehler();
    EXPECT_EQ(m.audioStatus(), k1520::audio::TonAusgabe::Status::Stumm);
    EXPECT_EQ(m.soundK1520()->senke(), nullptr);
    m.audioStop();
    EXPECT_EQ(m.audioStatus(), k1520::audio::TonAusgabe::Status::Aus);
}
