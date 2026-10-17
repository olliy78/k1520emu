/**
 * @file test_aup.cpp
 * @brief AUP 062-8531 (Tastaturteil) in der A5120-Maschine (Entwurf 33 N10c): Option aus = Tore
 *        40H-43H unbelegt, Stecken (Config, installAup, Konfigurationstext), /RESET, und ein
 *        CP/A-Gastlauf mit dem eingecheckten AUPTEST.COM (Codes am Bildschirm, Lampen/Hupe ueber
 *        die Rechnertastatur, Meldung ohne Karte).
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/a5120/a5120_konfig_text.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"

#ifndef K1520_AUPTEST_COM
#error "K1520_AUPTEST_COM fehlt (tools/auptest/auptest.com)"
#endif

namespace {
using C = A5120Machine::Config;
using T = AUP::Tastatur;
}  // namespace

TEST(AupMaschine, OhneOptionSindDieToreFrei) {
    C c;
    EXPECT_FALSE(c.aup);
    A5120Machine m(c);
    m.powerOn();
    EXPECT_EQ(m.aup(), nullptr);
    for (int p = 0x40; p <= 0x43; ++p) EXPECT_EQ(m.ioReadDebug(uint8_t(p)), 0xFF) << p;
}

TEST(AupMaschine, ConfigSteckt) {
    C c;
    c.aup = true;
    A5120Machine m(c);
    ASSERT_NE(m.aup(), nullptr);
    EXPECT_EQ(m.ioReadDebug(0x41), 0x00);
    EXPECT_FALSE(m.installAup()) << "doppelt";
    EXPECT_EQ(m.aupFehler(), "Es steckt bereits eine AUP");
}

TEST(AupMaschine, InstallNurVorDemErstenLauf) {
    A5120Machine a;
    EXPECT_TRUE(a.installAup(AUP::Alpha::Mit)) << a.aupFehler();
    EXPECT_EQ(a.aup()->config().alpha, AUP::Alpha::Mit);
    A5120Machine b;
    b.powerOn();
    b.run(1000);
    EXPECT_FALSE(b.installAup());
    EXPECT_EQ(b.aup(), nullptr);
}

TEST(AupMaschine, Konfigurationstext) {
    C c;
    std::string err;
    ASSERT_TRUE(a5120konfig::text(c, "aup=1;aup_alpha=ohne", err)) << err;
    EXPECT_TRUE(c.aup);
    EXPECT_EQ(c.aup_alpha, AUP::Alpha::Ohne);
    C d;
    EXPECT_FALSE(a5120konfig::text(d, "aup_alpha=mit", err));
    EXPECT_EQ(err, "aup_alpha: nur mit aup=1");
    C e;
    EXPECT_FALSE(a5120konfig::text(e, "aup=2", err));
}

TEST(AupMaschine, ResetSetztKommandoZurueck) {
    C c;
    c.aup = true;
    A5120Machine m(c);
    m.powerOn();
    m.aup()->ioWrite(0x43, 0xCC);
    EXPECT_EQ(m.aup()->kommando(), 0xCC);
    m.reset();
    EXPECT_EQ(m.aup()->kommando(), 0xFF);
}

// ─── CP/A-Gastlauf mit AUPTEST.COM ───────────────────────────────────────────
namespace {
using k1520test::runSmallUntil;
using k1520test::runSmallUntilVisible;
using k1520test::runUntilVramContains;
using k1520test::typeKey;
using k1520test::typeString;
using k1520test::vramLines;

constexpr long long kBootBudget = 90'000'000;
constexpr long long kFrist      = 40'000'000;

bool comAufDiskette(const std::string& disk) {
    std::string fe, err;
    static const FormatCatalog fk = FormatCatalog::loadDefault(&fe);
    static const FsCatalog     fs = FsCatalog::loadDefault(fk, &fe);
    auto vol = DiskVolume::open(disk, "cpa780", fk, fs, err, /*read_only=*/false);
    if (!vol) return false;
    FileRef ref;
    ref.name = "AUPTEST.COM";
    return vol->insert(K1520_AUPTEST_COM, ref, TransferOptions{}) && vol->flush();
}

void kaltstart(A5120Machine& m) {
    m.powerOn();
    ASSERT_TRUE(runUntilVramContains(m, "TPA ist OK!", kBootBudget)) << vramLines(m);
    ASSERT_TRUE(runSmallUntilVisible(m, "A>", kBootBudget)) << vramLines(m);
    k1520test::runCycles(m, 5'000'000);
}

void starte(A5120Machine& m) {
    typeString(m, "auptest");
    typeKey(m, k1520test::QK_RETURN);
    ASSERT_TRUE(runSmallUntil(m, "AUPTEST V0.1", kFrist)) << vramLines(m);
}
}  // namespace

TEST(AupMaschine, CpaGastZeigtCodesUndSchaltetLampen) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk disk("cpa_cpa780_k5601_noclock.img", "k1520_aup_cpa.img");
    ASSERT_TRUE(comAufDiskette(disk.path()));
    C c;
    c.aup = true;
    A5120Machine m(c);
    ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
    ASSERT_NO_FATAL_FAILURE(kaltstart(m));
    ASSERT_NO_FATAL_FAILURE(starte(m));
    ASSERT_TRUE(runSmallUntil(m, "Q = ENDE", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->kommando(), 0xFF) << "Programm setzt die AUP zurueck";

    // K7633 F01 (ohne Alpha C0H), Kundentastatur '5', Schluesselschalter SS1.
    ASSERT_TRUE(m.aup()->taste(T::K7633, 0x47, true));
    m.aup()->taste(T::K7633, 0x47, false);
    EXPECT_TRUE(runSmallUntil(m, "TT C0", kFrist)) << vramLines(m);
    m.aup()->taste(T::Kunde, 0x40, true);
    m.aup()->taste(T::Kunde, 0x40, false);
    EXPECT_TRUE(runSmallUntil(m, "KT 35 '5'", kFrist)) << vramLines(m);
    m.aup()->schluessel(1, true);
    EXPECT_TRUE(runSmallUntil(m, "TT AA", kFrist)) << vramLines(m);

    // Lampen ueber die Rechnertastatur: S = SEL, A = ALPHA (danach Tabelle "mit Alpha").
    typeString(m, "s");
    EXPECT_TRUE(runSmallUntil(m, "KMD FE", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->lampen(), AUP::kLampeSel);
    typeString(m, "a");
    EXPECT_TRUE(runSmallUntil(m, "KMD FC", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->lampen(), AUP::kLampeSel | AUP::kLampeAlpha);
    m.aup()->taste(T::K7633, 0x47, true);
    m.aup()->taste(T::K7633, 0x47, false);
    EXPECT_TRUE(runSmallUntil(m, "TT 41 'A'", kFrist)) << vramLines(m);
    const uint32_t h0 = m.aup()->hupe();
    typeString(m, "h");
    EXPECT_TRUE(runSmallUntil(m, "KMD BC", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->hupe(), h0 + 1);
    typeString(m, "q");
    EXPECT_TRUE(runSmallUntil(m, "AUPTEST ENDE", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->kommando(), 0xFF);
    EXPECT_EQ(m.aup()->lampen(), 0);
}

TEST(AupMaschine, CpaGastOhneKarteMeldetEs) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk disk("cpa_cpa780_k5601_noclock.img", "k1520_aup_cpa_ohne.img");
    ASSERT_TRUE(comAufDiskette(disk.path()));
    A5120Machine m;
    ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
    ASSERT_NO_FATAL_FAILURE(kaltstart(m));
    ASSERT_NO_FATAL_FAILURE(starte(m));
    EXPECT_TRUE(runSmallUntil(m, "Keine AUP auf 40H-43H (IN 41H = FFH)", kFrist)) << vramLines(m);
}
