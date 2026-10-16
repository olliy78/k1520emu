/**
 * @file test_a5120_optionen.cpp
 * @brief Einzeloptionen des A5120 (doc/design/33_a5130_k8924.md §3a, AP N1).
 *
 * `A5120Machine::Config` kennt RAM (K3526 | 1×/2×/4× K3525), Bildschirm (K7024 | K7023),
 * Tastatur (K7637 | K7636, frei kombinierbar), Kassette und Lade-ROM einzeln; die Ausfuehrungen
 * `Neu/Alt/Kmbg` sind nur noch eine Huelle (`Config::fuerAusfuehrung`).  Geprueft wird die
 * Bestueckung am Bus und — fuer die neu erlaubte Kombination K7023 + K3525 + K7637 — ein
 * CP/A-Kaltstart bis `A>` mit `dir` ueber die K7637.
 */
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "core/machines/a5120/a5120.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"

namespace {

using C = A5120Machine::Config;
using A = C::Ausfuehrung;
using R = C::Ram;
using B = C::Bildschirm;
using T = C::Tastatur;

std::unique_ptr<A5120Machine> baue(const C& c) {
    auto m = std::make_unique<A5120Machine>(c);
    m->powerOn();
    return m;
}

/// Port 0AH (BS-PIO B) in Betriebsart 3, alle Leitungen Eingang — wie CP/A beim Kaltstart.
uint8_t port0A(A5120Machine& m) {
    m.bus().ioWrite(0x0B, 0xCF);
    m.bus().ioWrite(0x0B, 0xFF);
    return m.bus().ioRead(0x0A);
}

/// Ist @p a beschreibbar (zwei Muster zurueckgelesen)?
bool beschreibbar(A5120Machine& m, uint16_t a) {
    m.memWriteDebug(a, 0x5A);
    const bool eins = m.memReadDebug(a) == 0x5A;
    m.memWriteDebug(a, 0xA5);
    return eins && m.memReadDebug(a) == 0xA5;
}

}  // namespace

TEST(A5120Optionen, VorgabeIstK3526K7024K7637OhneKassette) {
    const C c;
    EXPECT_EQ(c.ram, R::K3526);
    EXPECT_EQ(c.bildschirm, B::K7024);
    EXPECT_EQ(c.tastatur, T::Vorgabe);
    EXPECT_FALSE(c.kassette);
    EXPECT_EQ(c.laderom, K2526::Laderom::V09_2);
    auto m = baue(c);
    EXPECT_EQ(m->ram(), R::K3526);
    EXPECT_EQ(m->bildschirm(), B::K7024);
    EXPECT_EQ(m->k3525(0), nullptr);
    EXPECT_EQ(m->k7023(), nullptr);
    EXPECT_EQ(m->k7636(), nullptr);
    EXPECT_EQ(m->k5020(), nullptr);
    EXPECT_EQ(port0A(*m) & 0x40, 0);
    for (uint16_t a : {0x0400, 0x4000, 0x8000, 0xC000, 0xF7FF})
        EXPECT_TRUE(beschreibbar(*m, a)) << std::hex << a;
}

TEST(A5120Optionen, HuelleSetztNurDieEinzelfelder) {
    const C neu = C::fuerAusfuehrung(A::Neu);
    EXPECT_EQ(neu.ram, R::K3526);
    EXPECT_EQ(neu.bildschirm, B::K7024);
    EXPECT_EQ(neu.tastatur, T::Vorgabe);
    EXPECT_FALSE(neu.kassette);
    EXPECT_EQ(neu.drive_profiles, C{}.drive_profiles);

    const C alt = C::fuerAusfuehrung(A::Alt);
    EXPECT_EQ(alt.ram, R::K3525_4x);
    EXPECT_EQ(alt.bildschirm, B::K7023);
    EXPECT_EQ(alt.tastatur, T::K7636);
    EXPECT_FALSE(alt.kassette);
    EXPECT_EQ(alt.drive_profiles[0], "K5600.10");

    const C kmbg = C::fuerAusfuehrung(A::Kmbg);
    EXPECT_EQ(kmbg.ram, R::K3525_4x);
    EXPECT_EQ(kmbg.bildschirm, B::K7023);
    EXPECT_EQ(kmbg.tastatur, T::K7636);
    EXPECT_TRUE(kmbg.kassette);

    // Das Feld `ausfuehrung` allein steckt nichts um (nur Huelle fuer die C-ABI-Abfrage).
    C nurFeld;
    nurFeld.ausfuehrung = A::Alt;
    auto m = baue(nurFeld);
    EXPECT_EQ(m->ausfuehrung(), A::Alt);
    EXPECT_EQ(m->k7023(), nullptr);
    EXPECT_EQ(m->k3525(0), nullptr);
}

TEST(A5120Optionen, RamEineK3525Bis3FFF) {
    C c;
    c.ram = R::K3525_1x;
    auto m = baue(c);
    ASSERT_NE(m->k3525(0), nullptr);
    EXPECT_EQ(m->k3525(1), nullptr);
    EXPECT_TRUE(beschreibbar(*m, 0x0400));
    EXPECT_TRUE(beschreibbar(*m, 0x3FFF));
    for (uint16_t a : {0x4000, 0x7FFF, 0x8000, 0xC000, 0xF7FF}) {
        EXPECT_FALSE(beschreibbar(*m, a)) << std::hex << a;
        EXPECT_EQ(m->memReadDebug(a), 0xFF) << "offener Bus @" << std::hex << a;
    }
    // Der Bildspeicher der K7024 bleibt darueber (F800H).
    m->memWriteDebug(0xF800, 'K');
    EXPECT_EQ(m->screenChar(0, 0), 'K');
}

TEST(A5120Optionen, RamZweiK3525Bis7FFF) {
    C c;
    c.ram = R::K3525_2x;
    auto m = baue(c);
    ASSERT_NE(m->k3525(1), nullptr);
    EXPECT_EQ(m->k3525(2), nullptr);
    EXPECT_EQ(m->k3525(1)->config().base_addr, 0x4000);
    EXPECT_TRUE(beschreibbar(*m, 0x3FFF));
    EXPECT_TRUE(beschreibbar(*m, 0x4000));
    EXPECT_TRUE(beschreibbar(*m, 0x7FFF));
    EXPECT_FALSE(beschreibbar(*m, 0x8000));
    EXPECT_FALSE(beschreibbar(*m, 0xC000));
}

TEST(A5120Optionen, RamVierK3525MitK7024) {
    C c;
    c.ram = R::K3525_4x;   // Bildschirm bleibt K7024
    auto m = baue(c);
    for (int i = 0; i < 4; ++i) ASSERT_NE(m->k3525(i), nullptr) << i;
    EXPECT_EQ(m->k7023(), nullptr);
    for (uint16_t a : {0x0400, 0x4000, 0x8000, 0xC000, 0xF7FF})
        EXPECT_TRUE(beschreibbar(*m, a)) << std::hex << a;
    EXPECT_EQ(m->fbWidth(), 640);
    m->memWriteDebug(0xF800, 'V');
    EXPECT_EQ(m->screenChar(0, 0), 'V') << "K7024 ueberdeckt die vierte K3525";
    EXPECT_EQ(port0A(*m) & 0x40, 0) << "Bruecke folgt dem Bildschirm, nicht dem RAM";
}

TEST(A5120Optionen, K7023MitK3526) {
    C c;
    c.bildschirm = B::K7023;   // RAM bleibt K3526
    auto m = baue(c);
    ASSERT_NE(m->k7023(), nullptr);
    EXPECT_EQ(m->k3525(0), nullptr);
    EXPECT_EQ(m->fbWidth(), 512);
    EXPECT_EQ(m->fbHeight(), 256);
    m->memWriteDebug(0xFC00, 'Q');
    EXPECT_EQ(m->screenChar(0, 0), 'Q');
    EXPECT_TRUE(beschreibbar(*m, 0xF800)) << "F800–FBFFH ist RAM der K3526";
    EXPECT_NE(port0A(*m) & 0x40, 0) << "K7023 → BAB1";
}

TEST(A5120Optionen, BrueckeAendertNurBit6) {
    C k7024;
    C k7023;
    k7023.bildschirm = B::K7023;
    auto a = baue(k7024);
    auto b = baue(k7023);
    const uint8_t pa = port0A(*a), pb = port0A(*b);
    EXPECT_EQ(pa & 0x40, 0);
    EXPECT_EQ(pb & 0x40, 0x40);
    EXPECT_EQ(pa & ~0x40 & 0xFF, pb & ~0x40 & 0xFF) << "Bit 5/7 (und alle anderen) unveraendert";
}

TEST(A5120Optionen, TastaturFreiKombinierbar) {
    // K7637 an K7023 + K3525 (frueher abgelehnt, Entwurf 32 §3 A5).
    C c = C::fuerAusfuehrung(A::Alt);
    c.tastatur = T::K7637;
    auto m = baue(c);
    EXPECT_EQ(m->k7636(), nullptr);
    EXPECT_NE(m->k7023(), nullptr);
    // K7636 an K3526 + K7024.
    C d;
    d.tastatur = T::K7636;
    auto n = baue(d);
    EXPECT_NE(n->k7636(), nullptr);
    EXPECT_EQ(n->k7023(), nullptr);
}

TEST(A5120Optionen, KassetteAlsOption) {
    C c;
    c.kassette = true;   // am Vorgabegeraet (K3526 + K7024)
    A5120Machine m(c);
    ASSERT_NE(m.k5020(), nullptr);
    EXPECT_FALSE(m.installK5020()) << "kein zweites Stecken";
    m.powerOn();
    EXPECT_NE(m.bus().ioOwner(0x30), nullptr);
    EXPECT_EQ(m.k7023(), nullptr);

    A5120Machine ohne{C{}};
    EXPECT_EQ(ohne.k5020(), nullptr);
    EXPECT_TRUE(ohne.installK5020()) << "nachtraegliche Installation bleibt";
}

TEST(A5120Optionen, EmNurMitK3526) {
    for (R r : {R::K3525_1x, R::K3525_2x, R::K3525_4x}) {
        C c;
        c.ram = r;
        c.em = C::Em::em256;
        EXPECT_THROW(A5120Machine{c}, std::invalid_argument) << static_cast<int>(r);
    }
    // `Beide` gibt es seit N2 (test_a5120_bildschirm.cpp).
}

TEST(A5120Optionen, ZwischenstandMitZweiK3525) {
    C c;
    c.ram = R::K3525_2x;
    auto m = baue(c);
    m->memWriteDebug(0x5000, 0x11);
    m->memWriteDebug(0xF810, 'X');
    A5120Machine::MachineSnapshot s;
    m->captureState(s);
    EXPECT_EQ(s.ram[0x5000], 0x11);
    EXPECT_EQ(s.ram[0x9000], 0xFF) << "unbelegt = offener Bus";
    m->memWriteDebug(0x5000, 0x99);
    ASSERT_TRUE(m->restoreState(s));
    EXPECT_EQ(m->memReadDebug(0x5000), 0x11);
    EXPECT_EQ(m->screenChar(16, 0), 'X');
}

// ─── CP/A auf K7023 + 4× K3525 mit der K7637 (Abnahme N1) ──────────────────────

namespace {

std::string bab1Text(A5120Machine& m) {
    std::string s;
    for (int r = 0; r < 16; ++r) {
        for (int c = 0; c < 64; ++c) {
            const uint8_t ch = m.screenChar(c, r) & 0x7F;
            s.push_back((ch >= 0x20 && ch < 0x7F) ? static_cast<char>(ch) : ' ');
        }
        s.push_back('\n');
    }
    return s;
}

bool laufeBis(A5120Machine& m, const std::string& needle, long long max_cycles) {
    for (long long n = 0; n < max_cycles; n += k1520test::kSmallBatch) {
        m.run(static_cast<int>(k1520test::kSmallBatch));
        if ((n % 100'000) != 0) continue;
        if (bab1Text(m).find(needle) != std::string::npos) return true;
    }
    return bab1Text(m).find(needle) != std::string::npos;
}

}  // namespace

/**
 * @brief K7023 (BAB1) + 4× K3525 + K7637 an der K8025: CP/A 25.09.89 bootet bis `A>`, `dir`
 *        wird ueber die K7637 getippt.  Bild nur ueber screenChar (K7023), nie mem_read.
 */
TEST(BootIntegrationOptionen, CpaK7023K3525MitK7637BisPrompt) {
    C c = C::fuerAusfuehrung(A::Alt);
    c.tastatur       = T::K7637;
    c.drive_profiles = {"K5601", "K5601", "K5601", "none"};
    A5120Machine m(c);
    m.powerOn();
    k1520test::TempDisk a("cpa_cpa780_k5601_noclock.hfe");
    ASSERT_TRUE(m.mountDisk(0, a.path(), "cpa780", false)) << m.lastError();
    ASSERT_TRUE(laufeBis(m, "\nA>", 80'000'000)) << "kein A> auf BAB1:\n" << bab1Text(m);

    k1520test::runCycles(m, 5'000'000);   // Tastaturinitialisierung abschliessen
    k1520test::typeString(m, "dir");
    k1520test::typeKey(m, k1520test::QK_RETURN);
    ASSERT_TRUE(laufeBis(m, "A>dir", 40'000'000))
        << "Eingabe ueber die K7637 kommt nicht an:\n" << bab1Text(m);
    ASSERT_TRUE(laufeBis(m, "A: @OS      COM", 40'000'000))
        << "dir liefert kein Verzeichnis:\n" << bab1Text(m);
}
