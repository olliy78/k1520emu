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

#include <array>
#include <cstdio>
#include <vector>
#include <filesystem>
#include <fstream>

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

// --- Ausgelieferte Beispieldisketten (AP S8, disks/a5120_sios1526/, tools/sios_a5120/build.py) -----------
// Die Disketten kommen aus disks/ (Lieferstand), nie direkt mounten: Kopie im Temp-Ordner.
#ifndef A5120_LIEFERDISKETTEN
#error "A5120_LIEFERDISKETTEN fehlt - k1520_add_test(... DEFS A5120_LIEFERDISKETTEN=\"<repo>/disks\")"
#endif

namespace {

class Lieferkopie {
public:
    Lieferkopie(const std::string& rel, const std::string& name) {
        namespace fs = std::filesystem;
        const fs::path quelle = fs::path(rel).is_absolute() ? fs::path(rel)
                                                            : fs::path(A5120_LIEFERDISKETTEN) / rel;
        ok_ = fs::is_regular_file(quelle);
        path_ = (fs::temp_directory_path() / ("k1520_sios_" + name)).string();
        if (ok_) fs::copy_file(quelle, path_, fs::copy_options::overwrite_existing);
    }
    ~Lieferkopie() { std::error_code ec; std::filesystem::remove(path_, ec); }
    bool ok() const { return ok_; }
    const std::string& path() const { return path_; }
private:
    bool ok_ = false;
    std::string path_;
};

/// Eine Bestueckung: System in A:, Programmdisketten in B: (programme1) und C: (programme2).
struct Bestueckung {
    const char* system;                        // Pfad unter disks/
    std::array<const char*, 3> laufwerke;      // Profilnamen A:, B:, C:
    bool seriell;                              // K7637, sonst K7636
    const char* prog1 = nullptr;               // B: (Vorgabe programme1)
    const char* prog2 = nullptr;               // C: (Vorgabe programme2)
};

struct Ergebnis {
    bool bootet = false;     // Systemzeile erschienen
    bool cal = false;        // CAL DIDI von programme1 in B: laeuft
    std::string schirm;
};

constexpr const char* kProg1 = "a5120_sios1526/a5120_sios_programme1.hfe";
constexpr const char* kProg2 = "a5120_sios1526/a5120_sios_programme2.hfe";

/// Boot bis zur Systemzeile, danach `ASN 0315 66` und `CAL DIDI`.  Stecker 01 (Chefschalter).
Ergebnis probiere(const Bestueckung& b, const char* tag, bool mitCal = true) {
    Ergebnis e;
    A5120Machine::Config c;
    c.drive_profiles = {b.laufwerke[0], b.laufwerke[1], b.laufwerke[2], "none"};
    c.tastatur = b.seriell ? A5120Machine::Config::Tastatur::K7637
                           : A5120Machine::Config::Tastatur::K7636;
    c.sicherheitsstecker = 1;
    A5120Machine m(c);
    m.powerOn();
    Lieferkopie a(b.system, std::string(tag) + "_a.hfe");
    Lieferkopie p1(b.prog1 ? b.prog1 : kProg1, std::string(tag) + "_b.hfe");
    Lieferkopie p2(b.prog2 ? b.prog2 : kProg2, std::string(tag) + "_c.hfe");
    if (!a.ok() || !p1.ok() || !p2.ok()) { e.schirm = "Disketten fehlen"; return e; }
    // .hfe: die Geometrie steht in der Datei, der Formatname ist nur Platzhalter.
    m.mountDisk(0, a.path(), "k5601_ss80_26x128", false);   // Platzhalter; .img: 80 x 26 x 128
    m.mountDisk(1, p1.path(), "k5601_ss40_26x128", false);
    m.mountDisk(2, p2.path(), "k5601_ss40_26x128", false);
    e.bootet = k1520test::runSmallUntil(m, "(M)", 300'000'000LL, 100'000);
    e.schirm = k1520test::vramText(m);
    if (!e.bootet || !mitCal) return e;
    const uint32_t et1 = b.seriell ? kET1 : 0x01000004u;
    const uint32_t et2 = b.seriell ? kET2 : (K7636::QK_TASTE_BASE | 0x67);
    auto warte = [&](const std::string& t, long long n = 60'000'000LL) {
        return k1520test::runSmallUntil(m, t, n, 5'000);
    };
    auto tippe = [&](const std::string& t) { k1520test::typeString(m, t); };
    tippe("ASN");  k1520test::typeKey(m, et1);
    tippe("0315"); k1520test::typeKey(m, et1);
    if (!warte("0315_     50")) { e.schirm = k1520test::vramText(m); return e; }
    tippe("66");   k1520test::typeKey(m, et1);
    k1520test::typeKey(m, et2);
    if (!warte("(M)_")) { e.schirm = k1520test::vramText(m); return e; }
    tippe("CAL");  k1520test::typeKey(m, et1);
    if (!warte("CAL_")) { e.schirm = k1520test::vramText(m); return e; }
    tippe("DIDI");
    k1520test::runSmallUntil(m, "ZZZZ", 20'000'000LL, 5'000);
    k1520test::typeKey(m, et1);
    e.cal = warte("DIDI 1520 (SIEX)", 400'000'000LL);
    e.schirm = k1520test::vramText(m);
    return e;
}

}  // namespace

// Sondierung der Pruefmatrix (doc/sios/bedienung.md): kein Waechter, nur Ausgabe.
//   ./build/k1520_test_sios_boot --gtest_also_run_disabled_tests --gtest_filter=*Sondierung*
TEST(SiosMatrix, DISABLED_Sondierung) {
    const char* s11 = "a5120_sios1526/a5120_sios11_3xk5601_seriell.hfe";
    const char* s09 = "a5120_sios1526/a5120_sios09_3xk5600_10_parallel.hfe";
    struct F { const char* tag; Bestueckung b; };
    const F faelle[] = {
        {"11_k5601_ser",   {s11, {"K5601", "K5601", "K5601"}, true}},
        {"11_k5601_par",   {s11, {"K5601", "K5601", "K5601"}, false}},
        {"11_k560020_ser", {s11, {"K5600.20", "K5600.20", "K5600.20"}, true}},
        {"11_k560010_ser", {s11, {"K5600.10", "K5600.10", "K5600.10"}, true}},
        {"09_k5601_par",   {s09, {"K5601", "K5601", "K5601"}, false}},
        {"09_k5601_ser",   {s09, {"K5601", "K5601", "K5601"}, true}},
        {"09_k560020_par", {s09, {"K5600.20", "K5600.20", "K5600.20"}, false}},
        {"09_k560010_par", {s09, {"K5600.10", "K5600.10", "K5600.10"}, false}},
        {"09_k560010_ser", {s09, {"K5600.10", "K5600.10", "K5600.10"}, true}},
    };
    for (const F& f : faelle) {
        Ergebnis e = probiere(f.b, f.tag);
        std::fprintf(stderr, "MATRIX %-16s boot=%d cal=%d\n", f.tag, e.bootet, e.cal);
        if (!e.cal) std::fprintf(stderr, "%s\n", e.schirm.c_str());
    }
}

// Hilfsausgabe der Sondierung: nur belegte Bildschirmzeilen.
static std::string belegteZeilen(const std::string& schirm) {
    std::string z;
    for (size_t i = 0; i + 80 <= schirm.size(); i += 80) {
        std::string l = schirm.substr(i, 80);
        if (l.find_first_not_of(' ') != std::string::npos) z += l + "\n";
    }
    return z;
}

// --- Vorgabe-Bestueckung: 3 x K5601 + serielle Tastatur (K7637), Stecker 01 ----------------------------
// Aus disks/a5120_sios1526/ (Lieferstand, Kopie): System in A:, programme1 in B:, programme2 in C:.
namespace {

/// Alles ueber die ausgelieferten Disketten.  Tastatur und Systemdiskette waehlbar.
struct LieferRechner {
    A5120Machine m;
    Lieferkopie a, b, c;
    static A5120Machine::Config cfg(const char* laufwerk, bool seriell) {
        A5120Machine::Config k;
        k.drive_profiles = {laufwerk, laufwerk, laufwerk, "none"};
        k.tastatur = seriell ? A5120Machine::Config::Tastatur::K7637
                             : A5120Machine::Config::Tastatur::K7636;
        k.sicherheitsstecker = 1;
        return k;
    }
    uint32_t et1, et2;
    LieferRechner(const char* system, const char* laufwerk, bool seriell, const char* tag,
                  const char* version = "(M)")
        : m(cfg(laufwerk, seriell)),
          a(system, std::string(tag) + "_a.hfe"), b(kProg1, std::string(tag) + "_b.hfe"),
          c(kProg2, std::string(tag) + "_c.hfe"),
          et1(seriell ? kET1 : 0x01000004u),
          et2(seriell ? kET2 : (K7636::QK_TASTE_BASE | 0x67)) {
        m.powerOn();
        EXPECT_TRUE(a.ok() && b.ok() && c.ok()) << "disks/a5120_sios1526/ unvollstaendig";
        // .hfe/.img: Geometrie aus der Datei bzw. 80 x 26 x 128; der Name ist nur Platzhalter.
        EXPECT_TRUE(m.mountDisk(0, a.path(), "k5601_ss80_26x128", false)) << m.lastError();
        EXPECT_TRUE(m.mountDisk(1, b.path(), "k5601_ss40_26x128", false)) << m.lastError();
        EXPECT_TRUE(m.mountDisk(2, c.path(), "k5601_ss40_26x128", false)) << m.lastError();
        EXPECT_TRUE(k1520test::runSmallUntil(m, version, 300'000'000LL, 100'000))
            << k1520test::vramText(m);
    }
    void tippe(const std::string& s) { k1520test::typeString(m, s); }
    void taste(uint32_t code) { k1520test::typeKey(m, code); }
    bool warte(const std::string& t, long long takte = 60'000'000LL) {
        return k1520test::runSmallUntil(m, t, takte, 5'000);
    }
    std::string schirm() { return k1520test::vramText(m); }
    /// `ASN 0315 <phys>`: Programmlaufwerk umstellen (66 = B:, 7C = C:).
    void programmlaufwerk(const char* phys) {
        tippe("ASN");  taste(et1);
        tippe("0315"); taste(et1);
        ASSERT_TRUE(warte("0315_     50")) << schirm();
        tippe(phys);   taste(et1);
        taste(et2);
        ASSERT_TRUE(warte("(M)_")) << schirm();
    }
    void aufruf(const char* programm) {
        tippe("CAL");  taste(et1);
        ASSERT_TRUE(warte("CAL_")) << schirm();
        tippe(programm); taste(et1);
    }
};

}  // namespace

TEST(SiosVorgabe, SystemzeileUndCalVonB) {
    LieferRechner r("a5120_sios1526/a5120_sios11_3xk5601_seriell.hfe", "K5601", true, "vorg_b");
    EXPECT_NE(r.schirm().find("SIOS 1526/1.1"), std::string::npos) << r.schirm();
    r.programmlaufwerk("66");
    r.aufruf("DIDI");
    EXPECT_TRUE(r.warte("DIDI 1520 (SIEX) V 1.1", 400'000'000LL)) << r.schirm();
}

TEST(SiosVorgabe, CalVonC) {
    LieferRechner r("a5120_sios1526/a5120_sios11_3xk5601_seriell.hfe", "K5601", true, "vorg_c");
    r.programmlaufwerk("7C");
    r.aufruf("SYSD");
    // SYSD liegt NUR auf programme2 (C:); sein erster Schirm beweist, dass SIOS Laufwerk 06 kennt.
    EXPECT_TRUE(r.warte("LOAD SYSTEM-DISK DRIVE 04", 400'000'000LL)) << r.schirm();
}

// --- Weitere belegte Bestueckungen (Matrix in doc/sios/bedienung.md) -------------------------------------
// Jede ausgelieferte Systemdiskette hat einen Boot-Waechter: Systemzeile plus DAT (Tastatur laeuft).
namespace {

void systemzeileUndDat(const char* system, const char* laufwerk, bool seriell, const char* tag,
                       const char* version) {
    LieferRechner r(system, laufwerk, seriell, tag);
    EXPECT_NE(r.schirm().find(version), std::string::npos) << r.schirm();
    r.tippe("DAT");
    r.taste(r.et1);
    EXPECT_TRUE(r.warte("DAT     00.00.00")) << r.schirm();
}

}  // namespace

TEST(SiosBeispiel, V11_Kurz_3xK5601_Seriell_MitCal) {
    LieferRechner r("a5120_sios1526/a5120_sios11_3xk5601_seriell_sl1.hfe", "K5601", true, "sl1");
    EXPECT_NE(r.schirm().find("SIOS 1526/1.1"), std::string::npos) << r.schirm();
    r.programmlaufwerk("66");
    r.aufruf("DIDI");
    EXPECT_TRUE(r.warte("DIDI 1520 (SIEX) V 1.1", 400'000'000LL)) << r.schirm();
}

TEST(SiosBeispiel, V11_3xK560020_Seriell) {
    systemzeileUndDat("a5120_sios1526/a5120_sios11_3xk5601_seriell.hfe", "K5600.20", true, "v11_20",
                      "SIOS 1526/1.1");
}

TEST(SiosBeispiel, V09_3xK5601_Parallel) {
    systemzeileUndDat("a5120_sios1526/a5120_sios09_3xk5601_parallel.hfe", "K5601", false, "v09_5601",
                      "SIOS 1526/0.9");
}

TEST(SiosBeispiel, V09_3xK560020_Parallel) {
    systemzeileUndDat("a5120_sios1526/a5120_sios09_3xk5601_parallel.hfe", "K5600.20", false, "v09_20",
                      "SIOS 1526/0.9");
}

TEST(SiosBeispiel, V09_3xK5600_10_Parallel) {
    systemzeileUndDat("a5120_sios1526/a5120_sios09_3xk5600_10_parallel.hfe", "K5600.10", false,
                      "v09_10", "SIOS 1526/0.9");
}

// --- INIT in der Vorgabe-Bestueckung (3 x K5601, K7637): was geht mit welchem Ziel? ---------------------
namespace {

struct InitErgebnis {
    bool fertig = false;
    std::string schirm;
    std::string erkannt;   // Profilname des DiskTool-Kerns oder Fehler
    int warnungen = -1;
    std::string befund;
};

/// Leere (unformatierte) Diskette in der Geometrie eines Laufwerkstyps anlegen.
std::string leerdiskette(const char* laufwerk, const char* name) {
    A5120Machine::Config c;
    c.drive_profiles = {laufwerk, "none", "none", "none"};
    A5120Machine m(c);
    m.powerOn();
    auto t = TempDisk::empty(name);
    EXPECT_TRUE(m.createDisk(0, t.path(), "", false)) << m.lastError();
    m.unmountDisk(0);
    static std::vector<TempDisk> halter;
    halter.push_back(std::move(t));
    return halter.back().path();
}

/// INIT.COM von programme1 in A:, Ziel in B: (K5601).  @p menue = Eingabe am Menue "DOUBLE DENSITY..."
/// (leer = Vorgabe 1).
InitErgebnis initLauf(const std::string& ziel, const std::string& menue, const char* tag) {
    InitErgebnis e;
    A5120Machine::Config c = LieferRechner::cfg("K5601", true);
    A5120Machine m(c);
    m.powerOn();
    Lieferkopie syls("a5120_sios1526/a5120_sios11_3xk5601_seriell.hfe", std::string(tag) + "_s.hfe");
    Lieferkopie prog(kProg1, std::string(tag) + "_p.hfe");
    EXPECT_TRUE(m.mountDisk(0, syls.path(), "k5601_ss80_26x128", false)) << m.lastError();
    EXPECT_TRUE(k1520test::runSmallUntil(m, "(M)", 300'000'000LL, 100'000));
    EXPECT_TRUE(m.mountDisk(0, prog.path(), "k5601_ss40_26x128", false)) << m.lastError();
    EXPECT_TRUE(m.mountDisk(1, ziel, "k5601_ss80_26x128", false)) << m.lastError();
    auto warte = [&](const std::string& t, long long n = 60'000'000LL) {
        return k1520test::runSmallUntil(m, t, n, 5'000);
    };
    k1520test::typeString(m, "CAL");
    k1520test::typeKey(m, kET1);
    warte("CAL_");
    k1520test::typeString(m, "INIT");
    k1520test::typeKey(m, kET1);
    if (!warte("DISKETTE", 150'000'000LL)) { e.schirm = k1520test::vramText(m); return e; }
    k1520test::runSmallUntil(m, "ZZZZ", 25'000'000LL, 5'000);   // Frage 8" abwarten
    k1520test::typeString(m, "N");
    k1520test::typeKey(m, kET1);
    for (int i = 0; i < 5; ++i) {
        k1520test::runSmallUntil(m, "\x01", 20'000'000LL, 5'000);
        if (i == 0 && !menue.empty()) k1520test::typeString(m, menue);
        k1520test::typeKey(m, kET1);
    }
    k1520test::typeString(m, "Y");
    k1520test::typeKey(m, kET1);
    e.fertig = warte("FINISHED", 1'500'000'000LL);
    e.schirm = k1520test::vramText(m);
    m.unmountDisk(1);
    std::string err;
    auto vol = DiskVolume::open(ziel, "", formate(), dateisysteme(), err);
    if (!vol) { e.erkannt = "Fehler: " + err; return e; }
    e.erkannt = vol->hasFileSystem() ? vol->profile().name : "unbekannt";
    if (vol->hasFileSystem())
        e.warnungen = vol->check(FsCheckLevel::Schnell, false).zaehlerAb(FsSeverity::Warnung);
    return e;
}

}  // namespace

// Ziel = 80-Zylinder-Leerdiskette (Geometrie des K5601): INIT laeuft bis FINISHED (Menue vorgabe, 40 Spuren auf
// den Positionen 0..39); der DiskTool-Kern erkennt sie als SIOS-Diskette, Pruefung ohne Warnung.
TEST(SiosVorgabe, InitMitLeerdiskette80Zylinder) {
    const std::string z = leerdiskette("K5601", "sios_init_z80.hfe");
    InitErgebnis e = initLauf(z, "", "init80");
    EXPECT_TRUE(e.fertig) << e.schirm;
    EXPECT_EQ(e.erkannt, "sios_ss40");
    EXPECT_EQ(e.warnungen, 0);
}

// GRENZE (belegt): eine 40-Zylinder-Leerdiskette im K5601 bekommt Halbspuren; INIT meldet `ERR 9B`
// mit DEFEKTIV TRACKS 01, 03, 05 (jede zweite Position hat keine Spur).  Dafuer gibt es das K5600.10.
TEST(SiosVorgabe, InitMitLeerdiskette40ZylinderScheitertImK5601) {
    const std::string z = leerdiskette("K5600.10", "sios_init_z40.hfe");
    InitErgebnis e = initLauf(z, "", "init40");
    EXPECT_FALSE(e.fertig);
    EXPECT_NE(e.schirm.find("ERR 9B"), std::string::npos) << e.schirm;
}

// --- Rundlauf DiskTool -> SIOS (D4) ------------------------------------------------------------------------
// Datendiskette wie `k1520disktool create --fs sios_ss40` + `put`, in C: (Laufwerk 06) des K5601-Rechners;
// DIDI (programme1 in B:, ASN 0315 66) zeigt das Etikett HDR1 der Datei mit BOE/EOE/EOD.
// Grenze (belegt): die Satzanzeige danach kehrt zum Monitor zurueck, auch an der Original-DP2 - nicht geklaert.
TEST(SiosVorgabe, DidiZeigtDenKennsatzEinerMitDiskToolAngelegtenDatei) {
    namespace fs = std::filesystem;
    const std::string pfad = (fs::temp_directory_path() / "k1520_sios_daten.hfe").string();
    const fs::path quelle = fs::temp_directory_path() / "k1520_sios_daten_quelle";
    fs::remove_all(quelle);
    fs::create_directories(quelle);
    {
        std::string text = "RUNDLAUF D4 ALPHA";
        text.resize(256, '.');
        std::ofstream(quelle / "ALPHA", std::ios::binary) << text;
    }
    std::string err;
    {
        auto v = DiskVolume::create(pfad, "sios_ss40", "TEST01", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        TransferOptions to;
        ASSERT_TRUE(v->insertAll(quelle.string(), to)) << v->lastError();
    }
    LieferRechner r("a5120_sios1526/a5120_sios11_3xk5601_seriell.hfe", "K5601", true, "daten",
                    "(M)");
    r.m.unmountDisk(2);
    EXPECT_TRUE(r.m.mountDisk(2, pfad, "k5601_ss40_26x128", false)) << r.m.lastError();
    r.programmlaufwerk("66");
    r.aufruf("DIDI");
    ASSERT_TRUE(r.warte("DIDI 1520 (SIEX) V 1.1", 400'000'000LL)) << r.schirm();
    r.warte("ZZZZ", 20'000'000LL);
    r.tippe("6");     r.taste(r.et1);
    r.warte("ZZZZ", 20'000'000LL);
    r.tippe("A");     r.taste(r.et1);
    r.warte("ZZZZ", 20'000'000LL);
    r.tippe("ALPHA"); r.taste(r.et1);
    EXPECT_TRUE(r.warte("HDR1 ALPHA            00128 01001 01026", 100'000'000LL)) << r.schirm();
    std::error_code ec;
    fs::remove(pfad, ec);
    fs::remove_all(quelle, ec);
}
