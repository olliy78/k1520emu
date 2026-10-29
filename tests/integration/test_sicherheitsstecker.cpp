/**
 * @file test_sicherheitsstecker.cpp
 * @brief Sicherheitsstecker (Sicherheitsstecker) der K7637 am A5120 (AP S3b, Entwurf 43).
 *
 * Die Bausteintests stehen in test_k7637.cpp (`K7637Sicherheitsstecker.*`).  Hier: die
 * Maschine — Konfigurationstext, Laufzeit-Setter, K7636 ohne Element, und dass CP/A
 * mit gestecktem Element (zusaetzliche Bytes nach TYP) unveraendert bis zum Prompt kommt.
 */
#include <gtest/gtest.h>

#include <string>

#include "core/machines/a5120/a5120.h"
#include "core/machines/a5120/a5120_konfig_text.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"

using namespace k1520test;

namespace {
constexpr int kBootBudget  = 90'000'000;
constexpr int kInputBudget = 40'000'000;
}  // namespace

/// @test K7637SicherheitssteckerMaschine/SetterUndGetter
/// @par Pass criterion  Vorgabe 0; Setzen 0..6 ok, 7 und -1 abgewiesen; Geraetekennung merkbar.
TEST(K7637SicherheitssteckerMaschine, SetterUndGetter) {
    A5120Machine m{A5120Machine::Config{}};
    EXPECT_EQ(m.sicherheitsstecker(), 0);
    EXPECT_TRUE(m.setSicherheitsstecker(5));
    EXPECT_EQ(m.sicherheitsstecker(), 5);
    EXPECT_TRUE(m.sicherheitssteckerGeraetPasst());
    EXPECT_TRUE(m.setSicherheitsstecker(6, false));
    EXPECT_FALSE(m.sicherheitssteckerGeraetPasst());
    EXPECT_FALSE(m.setSicherheitsstecker(7));
    EXPECT_FALSE(m.setSicherheitsstecker(-1));
    EXPECT_EQ(m.sicherheitsstecker(), 6);
}

/// @test K7637SicherheitssteckerMaschine/KonfigText
/// @par Pass criterion  `sicherheitsstecker=3` gelangt in die Config; 9 und K7636 werden abgewiesen.
TEST(K7637SicherheitssteckerMaschine, KonfigText) {
    A5120Machine::Config cfg;
    std::string err;
    ASSERT_TRUE(a5120konfig::text(cfg, "sicherheitsstecker=3;sicherheitsstecker_geraet=falsch", err)) << err;
    EXPECT_EQ(cfg.sicherheitsstecker, 3);
    EXPECT_FALSE(cfg.sicherheitsstecker_geraet_passt);
    EXPECT_TRUE(a5120konfig::pruefe(cfg, false, err));
    A5120Machine m{cfg};
    EXPECT_EQ(m.sicherheitsstecker(), 3);

    A5120Machine::Config c2;
    EXPECT_FALSE(a5120konfig::text(c2, "sicherheitsstecker=9", err));
    A5120Machine::Config c3;
    EXPECT_FALSE(a5120konfig::text(c3, "sicherheitsstecker=1;tastatur=k7636", err));
    EXPECT_NE(err.find("K7637"), std::string::npos) << err;
}

/// @test K7637SicherheitssteckerMaschine/K7636HatKeinElement
/// @par Pass criterion  An der K7636 (kein K7637) meldet der Setter false.
TEST(K7637SicherheitssteckerMaschine, K7636HatKeinElement) {
    A5120Machine::Config cfg;
    cfg.tastatur = A5120Machine::Config::Tastatur::K7636;
    A5120Machine m{cfg};
    EXPECT_FALSE(m.setSicherheitsstecker(1));
}

/// @test K7637SicherheitssteckerMaschine/CpaBootetMitElement
/// @par Pass criterion  CP/A-Kaltstart mit Element 6 (Kaltstart-Vorgabe) erreicht den Prompt und
///      nimmt danach eine Eingabe an — die Zusatzbytes nach TYP stoeren die Tastaturerkennung nicht.
TEST(K7637SicherheitssteckerMaschine, CpaBootetMitElement) {
    TempDisk a("cpa_cpa780_k5601_noclock.img", "k1520_sicherheitsstecker.img");
    A5120Machine::Config cfg;
    cfg.sicherheitsstecker = 6;
    A5120Machine m{cfg};
    ASSERT_TRUE(m.mountDisk(0, a.path(), "cpa780", false)) << m.lastError();
    m.powerOn();
    ASSERT_TRUE(runUntilVramContains(m, "TPA ist OK!", kBootBudget)) << vramLines(m);
    ASSERT_TRUE(runSmallUntilVisible(m, "A>", kInputBudget)) << vramLines(m);
    typeString(m, "dir");
    typeKey(m, QK_RETURN);
    EXPECT_TRUE(runSmallUntil(m, "A: @OS      COM", kInputBudget)) << vramLines(m);
}
