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
 * SIOS 0.9 (K7636, S4): bootet ohne weitere Kernaenderung; Tasten nur mit Sicherheitsstecker.
 */
#include <gtest/gtest.h>

#include "core/machines/a5120/a5120.h"
#include "core/peripherals/k7636/k7636.h"
#include "core/peripherals/k7637/k7637.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"
#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/peripherals/floppy_drive/format_catalog.h"

namespace {

const FormatCatalog& formate() {
    static FormatCatalog c = [] {
        std::string f;
        FormatCatalog k = FormatCatalog::loadDefault(&f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

const FsCatalog& dateisysteme() {
    static FsCatalog c = [] {
        std::string f;
        FsCatalog k = FsCatalog::loadDefault(formate(), &f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

using k1520test::TempDisk;

void bootetBisSystemzeile(const char* disk, const char* tastatur, const char* version,
                          const char* laufwerk = "K5600.20",
                          const char* format = "k5601_ss80_26x128") {
    A5120Machine::Config cfg;
    cfg.drive_profiles = {laufwerk, "K5600.20", "none", "none"};
    cfg.tastatur = std::string(tastatur) == "k7636" ? A5120Machine::Config::Tastatur::K7636
                                                    : A5120Machine::Config::Tastatur::K7637;
    A5120Machine m(cfg);
    m.powerOn();
    TempDisk a(disk);
    ASSERT_TRUE(m.mountDisk(0, a.path(), format, false)) << m.lastError();
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

/// SIOS 0.9 am K7636 (S4): SYLp-Original in A: (K5600.20), DP1 in B:, Stecker nach Wunsch.
struct Sios09Rechner {
    A5120Machine m;
    TempDisk a{"sios1526_sylp.hfe"};
    TempDisk b{"sios1526_dp1.hfe", "sios_dp1_b09.hfe"};
    static A5120Machine::Config cfg(int stecker) {
        A5120Machine::Config c;
        c.drive_profiles = {"K5600.20", "K5600.20", "none", "none"};
        c.tastatur = A5120Machine::Config::Tastatur::K7636;
        c.sicherheitsstecker = stecker;
        return c;
    }
    explicit Sios09Rechner(int stecker) : m(cfg(stecker)) {
        m.powerOn();
        EXPECT_TRUE(m.mountDisk(0, a.path(), "k5601_ss80_26x128", false)) << m.lastError();
        EXPECT_TRUE(m.mountDisk(1, b.path(), "k5601_ss40_26x128", false)) << m.lastError();
        EXPECT_TRUE(k1520test::runSmallUntil(m, "(M)", 300'000'000LL, 100'000))
            << k1520test::vramText(m);
    }
    void tippe(const std::string& t) { k1520test::typeString(m, t); }
    void taste(uint32_t code) { k1520test::typeKey(m, code); }
    bool warte(const std::string& t, long long takte = 60'000'000LL) {
        return k1520test::runSmallUntil(m, t, takte, 5'000);
    }
    std::string schirm() { return k1520test::vramText(m); }
};
constexpr uint32_t kRueckgabe09 = 0x01000004;                 // Qt::Key_Return -> ET1 (FFH)
constexpr uint32_t kET2_09 = K7636::QK_TASTE_BASE | 0x67;     // Taste A00 = ET2 (FEH)

}  // namespace

TEST(SiosBoot, V11_ZeigtSystemzeile) {
    bootetBisSystemzeile("sios1526_syls.hfe", "k7637", "SIOS 1526/1.1");
}

// SIOS 0.9 am K7636 (S4).  Das Original SYLp (80 Zyl. x 2 Seiten ausgelesen, 40-Spur-Inhalt) bootet
// im K5600.20 unveraendert (Spur n liegt auf Position n, also OHNE Doppelschritt); die bereinigte
// 40-Spur-Fassung (Fixture sios1526_sylp40.hfe) im K5600.10, dem Laufwerk, fuer das SIOS 0.9
// einfach schrittet.  Im K5600.20 liefe sie nur mit Doppelschritt, den SIOS nicht fordert: kein Fall.
TEST(SiosBoot, V09_ZeigtSystemzeile) {
    bootetBisSystemzeile("sios1526_sylp.hfe", "k7636", "SIOS 1526/0.9");
}
TEST(SiosBoot, V09_Bereinigt_K560010) {
    bootetBisSystemzeile("sios1526_sylp40.hfe", "k7636", "SIOS 1526/0.9", "K5600.10",
                         "k5601_ss40_26x128");
}

// SIOS 0.9 / K7636 (S4): die Zeitgeber-ISR fragt Port 01H/06H ab.  Mit Sicherheitsstecker 1 (Chefschalter;
// Code A9H wie bei der K7637, Annahme [?]) nimmt der Monitor die Tasten an: DAT zeigt Kommando und Wert.
TEST(SiosMonitor, V09_DatMitK7636) {
    Sios09Rechner r(1);
    r.tippe("DAT");
    r.taste(kRueckgabe09);
    EXPECT_TRUE(r.warte("DAT     00.00.00")) << r.schirm();
    r.taste(kET2_09);
    EXPECT_TRUE(r.warte("(M)_")) << r.schirm();
    r.tippe("CLK");
    r.taste(kRueckgabe09);
    EXPECT_TRUE(r.warte("CLK     00.00.")) << r.schirm();
    r.taste(kET2_09);
    EXPECT_TRUE(r.warte("(M)_")) << r.schirm();
}

// GASTVERHALTEN: ohne Stecker ist die Bedienebene 0, und der Tastaturcode-Prozessor (28EEH..2935H, 00CBH
// and FEH = 0) verwirft jede Zeichentaste.  Die Tasten kommen an (K7636 liefert sie), SIOS ignoriert sie.
TEST(SiosMonitor, V09_OhneSteckerIgnoriertDieTasten) {
    Sios09Rechner r(0);
    r.tippe("CLK");
    r.taste(kRueckgabe09);
    EXPECT_FALSE(r.warte("CLK     00.", 30'000'000LL)) << r.schirm();
    EXPECT_NE(r.schirm().find("(M)_"), std::string::npos) << r.schirm();
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

// Zweites CAL nach Programmende ohne Neu-Mount: bis zur /IEO-Korrektur (2026-10-30) meldete es
// "ERR FD 1 NFND" (Befund doc/sios/befund_nfnd_84h.md: das RETI einer fremden ISR loeschte den IUS
// des BS-PIO B, OUT 00H trat in die laufende INT-BS-ISR ein).  CI = 0xAF beendet DIDI, ET2 -> Monitor.
constexpr uint32_t kCI = K7637::QK_RAW_BASE | 0xAF;
TEST(SiosMonitor, ZweitesCalNachProgrammende) {
    SiosRechner r;
    r.tippe("ASN");
    r.taste(kET1);
    r.tippe("0315");
    r.taste(kET1);
    EXPECT_TRUE(r.warte("0315_     50")) << r.schirm();
    r.tippe("66");
    r.taste(kET1);
    r.taste(kET2);
    EXPECT_TRUE(r.warte("(M)_")) << r.schirm();
    for (int lauf = 1; lauf <= 2; ++lauf) {
        r.tippe("CAL");
        r.taste(kET1);
        EXPECT_TRUE(r.warte("CAL_")) << r.schirm();
        r.tippe("DIDI");
        r.taste(kET1);
        ASSERT_TRUE(r.warte("DIDI 1520", 200'000'000LL)) << "Lauf " << lauf << ":\n" << r.schirm();
        EXPECT_EQ(r.schirm().find("NFND"), std::string::npos) << r.schirm();
        r.taste(kCI);
        r.taste(kET2);
        ASSERT_TRUE(r.warte("(M)_", 100'000'000LL)) << "Lauf " << lauf << ":\n" << r.schirm();
    }
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

// --- INIT (AP S6/D4, doc/sios/befund_init.md) ----------------------------------------------------
// Programm INIT von DP1 in A: (Programmlaufwerk 04), Zieldiskette in B: (05).  Die Zieldiskette
// wird vollstaendig neu beschrieben (INIT ALL = Y).
TEST(SiosInit, InitialisiertDiskette) {
    A5120Machine::Config c = SiosRechner::cfg();
    c.drive_profiles = {"K5600.20", "K5600.10", "none", "none"};   // dp2 = 40 Spuren
    A5120Machine m(c);
    m.powerOn();
    TempDisk syls("sios1526_syls.hfe");
    TempDisk prog("sios1526_dp1.hfe", "sios_init_prog.hfe");
    TempDisk ziel("sios1526_dp2.hfe", "sios_init_ziel.hfe");
    ASSERT_TRUE(m.mountDisk(0, syls.path(), "k5601_ss80_26x128", false)) << m.lastError();
    ASSERT_TRUE(k1520test::runSmallUntil(m, "(M)", 300'000'000LL, 100'000)) << k1520test::vramText(m);
    ASSERT_TRUE(m.mountDisk(0, prog.path(), "k5601_ss40_26x128", false)) << m.lastError();
    ASSERT_TRUE(m.mountDisk(1, ziel.path(), "k5601_ss40_26x128", false)) << m.lastError();
    auto warte = [&](const std::string& t, long long takte = 60'000'000LL) {
        return k1520test::runSmallUntil(m, t, takte, 5'000);
    };
    k1520test::typeString(m, "CAL");
    k1520test::typeKey(m, kET1);
    ASSERT_TRUE(warte("CAL_")) << k1520test::vramText(m);
    k1520test::typeString(m, "INIT");
    k1520test::typeKey(m, kET1);
    ASSERT_TRUE(warte("DISKETTE", 150'000'000LL)) << k1520test::vramText(m);
    k1520test::runSmallUntil(m, "ZZZZ", 25'000'000LL, 5'000);   // Frage 8" abwarten (befund_init.md 4)
    k1520test::typeString(m, "N");
    k1520test::typeKey(m, kET1);
    for (int i = 0; i < 5; ++i) {            // Menue 1, Satzlaenge, Folge, 2 x Defektspur
        k1520test::runSmallUntil(m, "\x01", 20'000'000LL, 5'000);
        k1520test::typeKey(m, kET1);
    }
    k1520test::typeString(m, "Y");
    k1520test::typeKey(m, kET1);
    // INIT schreibt alle 40 Spuren und liest sie zurueck (~2 min Geraetezeit).
    const bool fertig = warte("FINISHED", 1'500'000'000LL);
    const std::string v = k1520test::vramText(m);
    ASSERT_TRUE(fertig) << v;
    EXPECT_EQ(v.find("ERR"), std::string::npos) << v;

    // Gegenprobe mit dem DiskTool-Kern: die von SIOS initialisierte Diskette ist eine
    // SIOS-Diskette (ERMAP + VOL1 in Spur 0) und die Pruefung findet nichts.
    ASSERT_TRUE(m.unmountDisk(1));
    std::string err;
    auto vol = DiskVolume::open(ziel.path(), "", formate(), dateisysteme(), err);
    ASSERT_TRUE(vol) << err;
    ASSERT_TRUE(vol->hasFileSystem()) << "nicht als SIOS erkannt";
    EXPECT_EQ(vol->profile().name, "sios_ss40");
    EXPECT_EQ(vol->check(FsCheckLevel::Schnell, false).zaehlerAb(FsSeverity::Warnung), 0);
}
