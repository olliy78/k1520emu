/**
 * @file test_sios_boot.cpp
 * @brief SIOS 1526 auf dem A5120 (doc/design/43_sios1526.md, AP S3/S4): Kaltstart bis zur
 *        Systemzeile "MONITOR ===== SIOS 1526/x.y (M)".
 *
 * Diskette = IBM-3740-artig MFM 26x128 einseitig; SIOS 1.1 ist fuer die K7637, 0.9 fuer die
 * K7636 generiert.  Voraussetzung ist das Q240-Modell der K2526 (doc/merkposten/a5120_alt.md,
 * "Q240"): SIOS programmiert das Schutz-RAM ueber AB0 (B2 = 1) und schaltet es dann scharf
 * (B2 = 0); mit dem alten Tabellenmodell holte die ZVE1 danach nur noch 00H.
 *
 * Zweite Voraussetzung: der CTC kennt keinen Einzelschuss (Kanal 3 = Zaehler mit D3=1 als
 * 100-ms-Takt; ohne ihn hing der Monitor in 1CD4H, doc/sios/README.md "Befund S2b").
 * SIOS 0.9 (K7636): noch DISABLED_, Laufwerks-/Abbildfrage S4.
 */
#include <gtest/gtest.h>

#include "core/machines/a5120/a5120.h"
#include "core/peripherals/k7637/k7637.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"

namespace {

using k1520test::TempDisk;

void bootetBisSystemzeile(const char* disk, const char* tastatur, const char* version) {
    A5120Machine::Config cfg;
    cfg.drive_profiles = {"K5600.20", "K5600.20", "none", "none"};
    cfg.tastatur = std::string(tastatur) == "k7636" ? A5120Machine::Config::Tastatur::K7636
                                                    : A5120Machine::Config::Tastatur::K7637;
    A5120Machine m(cfg);
    m.powerOn();
    TempDisk a(disk);
    ASSERT_TRUE(m.mountDisk(0, a.path(), "k5601_ss80_26x128", false)) << m.lastError();
    const bool ok = k1520test::runSmallUntil(m, "MONITOR", 300'000'000LL, 100'000);
    const std::string v = k1520test::vramText(m);
    EXPECT_TRUE(ok) << "keine Systemzeile:\n" << v;
    EXPECT_NE(v.find(version), std::string::npos) << v;
}

// --- Bedienung (AP S5/S6, doc/sios/bedienung.md) ------------------------------------------------
// SIOS 1.1 am K7637: Zeichen als ASCII, ET1 = 0xFF, ET2 = 0xFE, M = 0xB0, CI = RESET = 0xAF.
// Tasten ohne Qt-Gegenstueck gehen nur als Rohcode (K7637::QK_RAW_BASE).
constexpr uint32_t kET1 = K7637::QK_RAW_BASE | 0xFF;
constexpr uint32_t kET2 = K7637::QK_RAW_BASE | 0xFE;
constexpr uint32_t kMon = K7637::QK_RAW_BASE | 0xB0;

/// SIOS-Rechner: Sicherheitsstecker 1 (Chefschalter), SYLs in A:, DP1 in B:, bis zur Systemzeile.
struct SiosRechner {
    A5120Machine m;
    TempDisk a{"sios1526_syls.hfe"};
    TempDisk b{"sios1526_dp1.hfe", "sios_dp1_b.hfe"};
    static A5120Machine::Config cfg() {
        A5120Machine::Config c;
        c.drive_profiles = {"K5600.20", "K5600.20", "none", "none"};
        c.sicherheitsstecker = 1;
        return c;
    }
    SiosRechner() : m(cfg()) {
        m.powerOn();
        EXPECT_TRUE(m.mountDisk(0, a.path(), "k5601_ss80_26x128", false)) << m.lastError();
        EXPECT_TRUE(m.mountDisk(1, b.path(), "k5601_ss40_26x128", false)) << m.lastError();
        EXPECT_TRUE(k1520test::runSmallUntil(m, "(M)", 300'000'000LL, 100'000))
            << k1520test::vramText(m);
    }
    void tippe(const std::string& s) { k1520test::typeString(m, s); }
    void taste(uint32_t code) { k1520test::typeKey(m, code); }
    /// 5 000 Takte je Pruefung, sobald die Tastatur im Spiel ist (K7637 = 9600 Bd + Zeitgeber-ISR).
    bool warte(const std::string& t, long long takte = 60'000'000LL) {
        return k1520test::runSmallUntil(m, t, takte, 5'000);
    }
    std::string schirm() { return k1520test::vramText(m); }
};

}  // namespace

TEST(SiosBoot, V11_ZeigtSystemzeile) {
    bootetBisSystemzeile("sios1526_syls.hfe", "k7637", "SIOS 1526/1.1");
}

TEST(SiosBoot, DISABLED_V09_ZeigtSystemzeile) {
    bootetBisSystemzeile("sios1526_sylp.hfe", "k7636", "SIOS 1526/0.9");
}

// DAT / CLK: die Systemzeile zeigt Kommando und Wert; ET2 bricht ab (Eingabe wieder leer).
TEST(SiosMonitor, DatUndClk) {
    SiosRechner r;
    r.tippe("DAT");
    r.taste(kET1);
    EXPECT_TRUE(r.warte("DAT     00.00.00")) << r.schirm();   // Datum noch nicht gesetzt
    r.taste(kET2);
    EXPECT_TRUE(r.warte("(M)_")) << r.schirm();
    r.tippe("CLK");
    r.taste(kET1);
    EXPECT_TRUE(r.warte("CLK     00.00.")) << r.schirm();     // Uhr laeuft (Sekunden zaehlen)
    r.taste(kET2);
    EXPECT_TRUE(r.warte("(M)_")) << r.schirm();
}

// ASN 0315 66 macht Diskette 2 (physisch 66) zum Programmlaufwerk; CAL laedt dann aus dem PHASELB
// von DP1.  DIDI meldet sich mit seinem Eingabeschirm, M fuehrt in den Monitor "(P)" (Programm
// bleibt geladen).
TEST(SiosMonitor, CalStartetProgrammVonDp1) {
    SiosRechner r;
    r.tippe("ASN");
    r.taste(kET1);
    r.tippe("0315");
    r.taste(kET1);
    EXPECT_TRUE(r.warte("0315_     50")) << r.schirm();        // Vorgabe: Diskette 1
    r.tippe("66");
    r.taste(kET1);
    r.taste(kET2);
    EXPECT_TRUE(r.warte("(M)_")) << r.schirm();

    r.tippe("CAL");
    r.taste(kET1);
    EXPECT_TRUE(r.warte("CAL_")) << r.schirm();
    r.tippe("DIDI");
    r.taste(kET1);
    ASSERT_TRUE(r.warte("DIDI 1520 (SIEX) V 1.1", 150'000'000LL)) << r.schirm();

    r.taste(kMon);
    EXPECT_TRUE(r.warte("SIOS 1526/1.1 (P)")) << r.schirm();
}

// Ohne ASN sucht CAL auf Diskette 1 (SYLs); CATM liegt nur auf DP1 -> "ERR FD 1 NFND".
TEST(SiosMonitor, CalMeldetNfndWennProgrammFehlt) {
    SiosRechner r;
    r.tippe("CAL");
    r.taste(kET1);
    EXPECT_TRUE(r.warte("CAL_")) << r.schirm();
    r.tippe("CATM");
    r.taste(kET1);
    EXPECT_TRUE(r.warte("NFND", 150'000'000LL)) << r.schirm();
}
