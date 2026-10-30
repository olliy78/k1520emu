/**
 * @file test_sios_rest.cpp
 * @brief SIOS 1526: Restpunkte nach der /IEO-Korrektur (DIDI-Satzanzeige, Rundlauf D4, 0.9-CAL,
 *        Diskettentausch in A:, INIT/80 Zylinder, SGEN mit T-K7637).  doc/sios/bedienung.md.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/machines/a5120/a5120.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "core/peripherals/k7636/k7636.h"
#include "core/peripherals/k7637/k7637.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"

#ifndef A5120_LIEFERDISKETTEN
#error "A5120_LIEFERDISKETTEN fehlt"
#endif

namespace {
namespace fs = std::filesystem;

constexpr uint32_t kET1 = K7637::QK_RAW_BASE | 0xFF;
constexpr uint32_t kET2 = K7637::QK_RAW_BASE | 0xFE;
constexpr uint32_t kCI  = K7637::QK_RAW_BASE | 0xAF;

const FormatCatalog& formate() {
    static FormatCatalog c = [] { std::string f; return FormatCatalog::loadDefault(&f); }();
    return c;
}
const FsCatalog& dateisysteme() {
    static FsCatalog c = [] { std::string f; return FsCatalog::loadDefault(formate(), &f); }();
    return c;
}

/// Kopie einer ausgelieferten Diskette (nie direkt mounten).
struct Kopie {
    std::string path;
    Kopie(const std::string& rel, const std::string& name) {
        path = (fs::temp_directory_path() / ("k1520_sios_rest_" + name)).string();
        fs::copy_file(fs::path(A5120_LIEFERDISKETTEN) / rel, path, fs::copy_options::overwrite_existing);
    }
    ~Kopie() { std::error_code ec; fs::remove(path, ec); }
};

constexpr const char* kSys11 = "a5120_sios1526/a5120_sios11_3xk5601_seriell.hfe";
constexpr const char* kSys09 = "a5120_sios1526/a5120_sios09_3xk5601_parallel.hfe";
constexpr const char* kProg1 = "a5120_sios1526/a5120_sios_programme1.hfe";
constexpr const char* kProg2 = "a5120_sios1526/a5120_sios_programme2.hfe";

std::string belegt(const std::string& s) {
    std::string z;
    for (size_t i = 0; i + 80 <= s.size(); i += 80) {
        std::string l = s.substr(i, 80);
        if (l.find_first_not_of(' ') != std::string::npos) z += l + "\n";
    }
    return z;
}

/// SIOS-Rechner mit beliebigen Laufwerken; Tastatur seriell (1.1) oder parallel (0.9).
struct Rig {
    A5120Machine m;
    bool seriell;
    uint32_t et1, et2, ci;
    static A5120Machine::Config cfg(const char* lw, bool ser) {
        A5120Machine::Config c;
        c.drive_profiles = {lw, lw, lw, "none"};
        c.tastatur = ser ? A5120Machine::Config::Tastatur::K7637 : A5120Machine::Config::Tastatur::K7636;
        c.sicherheitsstecker = 1;
        return c;
    }
    Rig(const char* lw, bool ser)
        : m(cfg(lw, ser)), seriell(ser), et1(ser ? kET1 : 0x01000004u),
          et2(ser ? kET2 : (K7636::QK_TASTE_BASE | 0x67)),
          ci(ser ? kCI : (K7636::QK_TASTE_BASE | 0x5B)) { m.powerOn(); }
    bool mount(int d, const std::string& p, const char* fmt) { return m.mountDisk(d, p, fmt, false); }
    bool bootWarte() { return k1520test::runSmallUntil(m, "(M)", 300'000'000LL, 100'000); }
    void tippe(const std::string& s) { k1520test::typeString(m, s); }
    void taste(uint32_t c) { k1520test::typeKey(m, c); }
    bool warte(const std::string& t, long long n = 60'000'000LL) {
        return k1520test::runSmallUntil(m, t, n, 5'000);
    }
    void ruhe(long long n) { k1520test::runSmallUntil(m, "\x01", n, 5'000); }
    std::string schirm() { return k1520test::vramText(m); }
    void zeige(const std::string& wo) { std::fprintf(stderr, "--- %s\n%s", wo.c_str(), belegt(schirm()).c_str()); }
    /// Eingabefeld: Text, ET1.
    void feld(const std::string& t) { ruhe(15'000'000LL); if (!t.empty()) tippe(t); taste(et1); }
    void asn66() {
        tippe("ASN"); taste(et1); tippe("0315"); taste(et1);
        ASSERT_TRUE(warte("0315_     50")) << schirm();
        tippe("66"); taste(et1); taste(et2);
        ASSERT_TRUE(warte("(M)_")) << schirm();
    }
    void cal(const char* prog) {
        tippe("CAL"); taste(et1);
        ASSERT_TRUE(warte("CAL_")) << schirm();
        tippe(prog); taste(et1);
    }
};

/// Datendiskette wie `create --fs sios_ss40` + `put`.
std::string datendiskette(const std::string& name,
                          const std::vector<std::pair<std::string, std::string>>& dateien) {
    const std::string pfad = (fs::temp_directory_path() / name).string();
    const fs::path q = fs::temp_directory_path() / (name + ".q");
    fs::remove_all(q);
    fs::create_directories(q);
    for (auto& [n, t] : dateien) std::ofstream(q / n, std::ios::binary) << t;
    std::string err;
    auto v = DiskVolume::create(pfad, "sios_ss40", "TEST01", formate(), dateisysteme(), err);
    EXPECT_TRUE(v) << err;
    if (v) { TransferOptions to; EXPECT_TRUE(v->insertAll(q.string(), to)) << v->lastError(); }
    fs::remove_all(q);
    return pfad;
}

/// Leere (unformatierte) Diskette in der Geometrie eines Laufwerkstyps (Anwenderfall "Leere Diskette").
std::string leerdiskette(const char* laufwerk, const char* name) {
    A5120Machine::Config c;
    c.drive_profiles = {laufwerk, "none", "none", "none"};
    A5120Machine m(c);
    m.powerOn();
    const std::string pfad = (fs::temp_directory_path() / name).string();
    fs::remove(pfad);
    EXPECT_TRUE(m.createDisk(0, pfad, "", false)) << m.lastError();
    m.unmountDisk(0);
    return pfad;
}

}  // namespace

// Freie Sondierung ueber die Umgebung (kein Waechter).  SIOS_STEPS: Schritte getrennt durch '|':
//   t=Text  e1  e2  ci  m  w=Text[,Mio. Takte]  r=Mio. Takte  s  u=Laufwerk (auswerfen)  a=Datei (nach A: einlegen)
// SIOS_PROG: Programm (CAL), SIOS_DATEN: "NAME:inhalt;..." fuer C: (inhalt "#n" = n Saetze zu 128 B).
TEST(SiosRest, DISABLED_Sond_Frei) {
    const char* st = std::getenv("SIOS_STEPS");
    const char* prog = std::getenv("SIOS_PROG");
    if (!st) GTEST_SKIP();
    std::vector<std::pair<std::string, std::string>> d;
    if (const char* dd = std::getenv("SIOS_DATEN")) {
        std::string s = dd;
        size_t p = 0;
        while (p < s.size()) {
            size_t e = s.find(';', p);
            if (e == std::string::npos) e = s.size();
            std::string kv = s.substr(p, e - p);
            size_t c = kv.find(':');
            std::string inh = kv.substr(c + 1);
            if (inh.rfind("#", 0) == 0) inh = std::string(std::stoul(inh.substr(1)) * 128, 'x');
            d.emplace_back(kv.substr(0, c), inh);
            p = e + 1;
        }
    }
    const std::string daten = datendiskette("k1520_sios_rest_frei.hfe", d);
    std::vector<std::pair<std::string, std::string>> d2;
    if (const char* dd = std::getenv("SIOS_DATEN2")) d2.emplace_back(dd, std::string(4 * 128, 'y'));
    const std::string daten2 = datendiskette("k1520_sios_rest_frei2.hfe", d2);
    const bool v09 = std::getenv("SIOS_V09") != nullptr;
    Rig r(std::getenv("SIOS_LW") ? std::getenv("SIOS_LW") : "K5601", !v09);
    Kopie a(std::getenv("SIOS_SYS") ? std::getenv("SIOS_SYS") : (v09 ? kSys09 : kSys11), "fa.hfe"), b(kProg1, "fb.hfe"), c2(kProg2, "fc.hfe");
    ASSERT_TRUE(r.mount(0, a.path, std::getenv("SIOS_SYS") ? "k5601_ss40_26x128" : "k5601_ss80_26x128"));
    ASSERT_TRUE(r.mount(1, b.path, "k5601_ss40_26x128"));
    ASSERT_TRUE(r.mount(2, daten, "k5601_ss40_26x128"));
    ASSERT_TRUE(r.bootWarte());
    if (!std::getenv("SIOS_OHNE_ASN")) r.asn66();
    if (prog) { r.cal(prog); r.ruhe(150'000'000LL); r.zeige("Programm gestartet"); }
    std::string s = st;
    size_t p = 0;
    while (p <= s.size()) {
        size_t e = s.find('|', p);
        if (e == std::string::npos) e = s.size();
        const std::string k = s.substr(p, e - p);
        p = e + 1;
        if (k.rfind("t=", 0) == 0) { r.ruhe(5'000'000LL); r.tippe(k.substr(2)); }
        else if (k == "e1") { r.ruhe(5'000'000LL); r.taste(r.et1); }
        else if (k == "e2") { r.ruhe(5'000'000LL); r.taste(r.et2); }
        else if (k == "ci") { r.ruhe(5'000'000LL); r.taste(r.ci); }
        else if (k == "m") { r.ruhe(5'000'000LL); r.taste(K7637::QK_RAW_BASE | 0xB0); }
        else if (k == "x") { r.ruhe(5'000'000LL); r.taste(r.seriell ? kET2 : (K7636::QK_TASTE_BASE | 0x67)); }
        else if (k.rfind("w=", 0) == 0) {
            std::string t = k.substr(2);
            long long n = 100'000'000LL;
            size_t cpos = t.find(',');
            if (cpos != std::string::npos) { n = std::stoll(t.substr(cpos + 1)) * 1'000'000LL; t = t.substr(0, cpos); }
            const bool ok = r.warte(t, n);
            std::fprintf(stderr, "[%s '%s']\n", ok ? "gefunden" : "NICHT gefunden", t.c_str());
        }
        else if (k.rfind("r=", 0) == 0) r.ruhe(std::stoll(k.substr(2)) * 1'000'000LL);
        else if (k == "s") r.zeige("Schirm");
        else if (k.rfind("u=", 0) == 0) r.m.unmountDisk(std::stoi(k.substr(2)));
        else if (k.rfind("m", 0) == 0 && k.size() > 3 && k[2] == '=') {   // mN=daten|daten2
            r.m.unmountDisk(k[1] - '0');
            const std::string w = k.substr(3);
            if (w == "leer80") {
                static const std::string l = leerdiskette("K5601", "k1520_sios_rest_leer80.hfe");
                r.mount(k[1] - '0', l, "k5601_ss80_26x128");
            } else {
                r.mount(k[1] - '0', w == "daten2" ? daten2 : daten, "k5601_ss40_26x128");
            }
        }
        else if (k.rfind("a=", 0) == 0) {   // Datei aus disks/ nach A: (alte Diskette ersetzen)
            static std::vector<std::unique_ptr<Kopie>> halter;
            halter.push_back(std::make_unique<Kopie>(k.substr(2), "tausch" + std::to_string(halter.size()) + ".hfe"));
            r.m.unmountDisk(0);
            r.mount(0, halter.back()->path, "k5601_ss40_26x128");
        }
    }
    r.zeige("Ende");
    for (int i = 0; i < 3; ++i) r.m.unmountDisk(i);
    if (const char* dump = std::getenv("SIOS_DUMP")) {   // je Zylinder: Anfang von Sektor 1 (Index 0)
        std::string err;
        auto v = DiskVolume::open(dump, "", formate(), dateisysteme(), err);
        if (v) {
            for (int cyl = 0; cyl < 80; ++cyl) {
                std::vector<uint8_t> b;
                uint16_t crc = 0;
                std::string t;
                if (v->readSectorAt(uint8_t(cyl), 0, 0, b, crc))
                    for (int i = 0; i < 6; ++i) t += char(b[i] >= 0x20 && b[i] < 0x7F ? b[i] : '.');
                else t = "-";
                std::fprintf(stderr, "Z%02d:%s ", cyl, t.c_str());
                if (cyl % 8 == 7) std::fprintf(stderr, "\n");
            }
        } else std::fprintf(stderr, "dump: %s\n", err.c_str());
    }
    if (const char* sav = std::getenv("SIOS_SAVE")) fs::copy_file(daten, sav, fs::copy_options::overwrite_existing);
    if (const char* sav = std::getenv("SIOS_SAVE2")) fs::copy_file(daten2, sav, fs::copy_options::overwrite_existing);
}


// ======================================================================================================
// Waechter
// ======================================================================================================
namespace {

/// n Saetze "SATZ 01 ...." zu je 128 Byte (eine DiskTool-Datei).
std::string saetze(int n) {
    std::string t;
    for (int i = 1; i <= n; ++i) {
        char b[32];
        std::snprintf(b, sizeof b, "SATZ %02d ", i);
        std::string z = b;
        z.resize(128, '.');
        t += z;
    }
    return t;
}

/// 1.1 mit K5601-Laufwerken: A: = SYLs; der Test tauscht A: gegen die Programmdiskette (kein ASN noetig).
struct Rig11 : Rig {
    Kopie a{kSys11, "w_a.hfe"}, p1{kProg1, "w_p1.hfe"};
    Rig11() : Rig("K5601", true) {
        EXPECT_TRUE(mount(0, a.path, "k5601_ss80_26x128"));
        EXPECT_TRUE(bootWarte()) << schirm();
    }
    void programmInA() {
        m.unmountDisk(0);
        EXPECT_TRUE(mount(0, p1.path, "k5601_ss40_26x128"));
    }
};

/// INIT (V 1.4) auf der Diskette in B:, Programm schon in A:.  @p menue = "1" (40 Spuren) oder "2" (Double Tracks).
void initiere(Rig& r, const char* menue) {
    r.tippe("CAL");
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("CAL_")) << r.schirm();
    r.tippe("INIT");
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("DISKETTE", 150'000'000LL)) << r.schirm();
    r.ruhe(25'000'000LL);                         // Frage nach 8" abwarten (befund_init.md)
    r.tippe("N");
    r.taste(r.et1);
    r.ruhe(20'000'000LL);
    r.taste(r.et1);                               // "5,25" DISKETTE" -> Menue
    r.ruhe(20'000'000LL);
    r.tippe(menue);                               // 1 DOUBLE DENSITY | 2 DOUBLE DENSITY, DOUBLE TRACKS
    r.taste(r.et1);
    for (int i = 0; i < 4; ++i) {                 // Satzlaenge, Folge, 2 x Defektspur
        r.ruhe(20'000'000LL);
        r.taste(r.et1);
    }
    r.ruhe(20'000'000LL);
    r.tippe("Y");                                 // INIT ALL
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("FINISHED", 1'500'000'000LL)) << r.schirm();
    r.ruhe(5'000'000LL);
    r.taste(r.ci);
    r.ruhe(5'000'000LL);
    r.taste(r.et2);
    ASSERT_TRUE(r.warte("(M)_")) << r.schirm();
}

/// COPD "COPY ALL" von A: nach B: (ohne markierte Saetze, ohne Druck); Programm ist schon geladen.
void copdAlles(Rig& r) {
    r.ruhe(150'000'000LL);
    r.tippe("C");
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("USE DISK", 200'000'000LL)) << r.schirm();   // Quelle
    r.ruhe(10'000'000LL);
    r.taste(r.et1);
    r.ruhe(60'000'000LL);
    r.taste(r.et2);                               // PRINT DICTIONARY -> NO
    ASSERT_TRUE(r.warte("OUTPUT DISK", 100'000'000LL)) << r.schirm();
    r.ruhe(60'000'000LL);
    r.taste(r.et1);                               // USE DISK (Ziel)
    r.ruhe(100'000'000LL);
    r.taste(r.et2);                               // PRINT DICTIONARY -> NO
    r.ruhe(100'000'000LL);
    r.tippe("A");                                 // COPY ALL
    r.taste(r.et1);
    r.ruhe(300'000'000LL);
    r.tippe("N");                                 // ohne markierte Saetze
    r.taste(r.et1);
    r.ruhe(800'000'000LL);
}

/// Alle Dateien einer Diskette holen, Inhalt von @p name lesen.
std::string holeDatei(const std::string& abbild, const std::string& name, const char* tag) {
    std::string err;
    auto v = DiskVolume::open(abbild, "", formate(), dateisysteme(), err);
    EXPECT_TRUE(v) << err;
    if (!v) return {};
    const fs::path aus = fs::temp_directory_path() / (std::string("k1520_sios_rest_aus_") + tag);
    fs::remove_all(aus);
    fs::create_directories(aus);
    TransferOptions to;
    EXPECT_TRUE(v->extractAll(aus.string(), to)) << v->lastError();
    std::ifstream f(aus / name, std::ios::binary);
    std::string got((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    fs::remove_all(aus);
    return got;
}

}  // namespace

// 1. DIDI zeigt die Datensaetze einer mit dem DiskTool angelegten Datei (Rundlauf D4, DiskTool -> SIOS).
// Bedienung: DEVICE 6, FORM A, NAME; die Satznummer verlangt FUENF Stellen (00001) - "1" wird nicht angenommen
// und das Feld bleibt stehen; Leer-ET1 springt zu KEY POSITION (Suche ueber Schluessel).  Nach der Nummer fragt
// CHECK, jedes weitere ET1 zeigt den naechsten Satz.  (Der alte Befund "DIDI springt zum Monitor" war Bedienung.)
TEST(SiosRest, DidiZeigtDieDatensaetzeEinerDiskToolDatei) {
    const std::string daten = datendiskette("k1520_sios_rest_w1.hfe", {{"ALPHA", saetze(4)}});
    Rig11 r;
    Kopie b(kProg1, "w1_b.hfe");
    ASSERT_TRUE(r.mount(1, b.path, "k5601_ss40_26x128"));
    ASSERT_TRUE(r.mount(2, daten, "k5601_ss40_26x128"));
    r.asn66();
    r.cal("DIDI");
    ASSERT_TRUE(r.warte("DIDI 1520 (SIEX) V 1.1", 400'000'000LL)) << r.schirm();
    r.feld("6");
    r.feld("A");
    r.feld("ALPHA");
    ASSERT_TRUE(r.warte("HDR1 ALPHA            00128 01001 01026", 100'000'000LL)) << r.schirm();
    r.feld("00001");
    ASSERT_TRUE(r.warte("CHECK", 100'000'000LL)) << r.schirm();
    r.feld("");
    ASSERT_TRUE(r.warte("SATZ 01 ....", 100'000'000LL)) << r.schirm();
    EXPECT_NE(r.schirm().find("ALPHA                          00001"), std::string::npos) << r.schirm();
    for (int i = 2; i <= 4; ++i) {
        r.ruhe(15'000'000LL);
        r.taste(r.et1);
        char z[32];
        std::snprintf(z, sizeof z, "SATZ %02d ....", i);
        ASSERT_TRUE(r.warte(z, 100'000'000LL)) << i << "\n" << r.schirm();
    }
    fs::remove(daten);
}

// 2. Rundlauf SIOS -> DiskTool: COPD kopiert die Diskette (alle Spuren, mit dem SIOS-Treiber geschrieben) auf
// eine DiskTool-Diskette; das DiskTool holt die Datei byteweise gleich zurueck, die Pruefung ist sauber.
TEST(SiosRest, VonSiosGeschriebeneDateiLiestDasDiskTool) {
    const std::string quelle = saetze(8);
    const std::string daten = datendiskette("k1520_sios_rest_w2q.hfe", {{"ALPHA", quelle}});
    const std::string ziel = datendiskette("k1520_sios_rest_w2z.hfe", {{"ZIEL", std::string(512, 'y')}});
    Rig11 r;
    r.programmInA();
    ASSERT_TRUE(r.mount(1, ziel, "k5601_ss40_26x128"));
    r.tippe("CAL");
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("CAL_")) << r.schirm();
    r.tippe("COPD");
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("LOAD DISKS", 300'000'000LL)) << r.schirm();
    r.m.unmountDisk(0);
    ASSERT_TRUE(r.mount(0, daten, "k5601_ss40_26x128"));
    copdAlles(r);
    r.m.unmountDisk(0);
    r.m.unmountDisk(1);

    std::string err;
    auto v = DiskVolume::open(ziel, "", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    ASSERT_TRUE(v->hasFileSystem());
    EXPECT_EQ(v->check(FsCheckLevel::Schnell, false).zaehlerAb(FsSeverity::Warnung), 0);
    v.reset();
    const std::string got = holeDatei(ziel, "ALPHA", "w2");
    ASSERT_GE(got.size(), quelle.size()) << "ALPHA fehlt/zu kurz";
    EXPECT_EQ(got.substr(0, quelle.size()), quelle);
    fs::remove(daten);
    fs::remove(ziel);
}

// 3. SIOS 0.9 (K7636): CAL laedt Programme der DP1/DP2.  Fehlt der Name, meldet CAL ERR FD 1 NFND; erst CI
// (K7636: Taste F54, AFH) gibt die Tasten wieder frei.  Zweites CAL geht.
TEST(SiosRest, V09_CalLaedtProgrammeUndMeldetNfnd) {
    Rig r("K5601", false);
    Kopie a(kSys09, "w3_a.hfe"), b(kProg1, "w3_b.hfe");
    ASSERT_TRUE(r.mount(0, a.path, "k5601_ss80_26x128"));
    ASSERT_TRUE(r.mount(1, b.path, "k5601_ss40_26x128"));
    ASSERT_TRUE(r.bootWarte()) << r.schirm();
    r.tippe("CAL");
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("CAL_")) << r.schirm();
    r.tippe("ZZZZ");
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("ERR FD 1  NFND", 200'000'000LL)) << r.schirm();
    r.taste(r.ci);
    ASSERT_TRUE(r.warte("(M)_")) << r.schirm();
    r.asn66();
    for (int lauf = 1; lauf <= 2; ++lauf) {
        r.tippe("CAL");
        r.taste(r.et1);
        ASSERT_TRUE(r.warte("CAL_")) << r.schirm();
        r.tippe("DIDI");
        r.taste(r.et1);
        ASSERT_TRUE(r.warte("DIDI 1520 (SIEX) V 1.1", 400'000'000LL)) << "Lauf " << lauf << "\n" << r.schirm();
        r.ruhe(30'000'000LL);
        r.taste(r.ci);
        r.ruhe(5'000'000LL);
        r.taste(r.et2);
        ASSERT_TRUE(r.warte("(M)_", 100'000'000LL)) << "Lauf " << lauf << "\n" << r.schirm();
    }
}

// 4. Systemdiskette in A: gegen die Programmdiskette tauschen: CAL DIDI ohne ASN (Programmlaufwerk A:), zweimal.
// Bis zur /IEO-Korrektur endete das mit ERR FD 3 R/W.
TEST(SiosRest, ProgrammdisketteInATauschen) {
    Rig11 r;
    for (int lauf = 1; lauf <= 2; ++lauf) {
        r.programmInA();
        r.tippe("CAL");
        r.taste(r.et1);
        ASSERT_TRUE(r.warte("CAL_")) << r.schirm();
        r.tippe("DIDI");
        r.taste(r.et1);
        ASSERT_TRUE(r.warte("DIDI 1520 (SIEX) V 1.1", 400'000'000LL)) << "Lauf " << lauf << "\n" << r.schirm();
        EXPECT_EQ(r.schirm().find("ERR"), std::string::npos) << r.schirm();
        r.ruhe(30'000'000LL);
        r.taste(r.ci);
        r.ruhe(5'000'000LL);
        r.taste(r.et2);
        ASSERT_TRUE(r.warte("(M)_", 100'000'000LL)) << r.schirm();
    }
}

// 5a. INIT-Menue "2 DOUBLE DENSITY, DOUBLE TRACKS" (V 1.4): die Eingabe gehoert an das MENUE hinter
// "5,25" DISKETTE" (nicht an die Frage nach 8"; dort ergab sie ERR 90).  Ergebnis ist eine 80-Zylinder-
// Diskette (Spuren 01...76 laut Anzeige), die das DiskTool als sios_ss80 erkennt.
TEST(SiosRest, InitDoubleTracksLegtEine80ZylinderDisketteAn) {
    const std::string z = leerdiskette("K5601", "k1520_sios_rest_w5a.hfe");
    Rig11 r;
    r.programmInA();
    ASSERT_TRUE(r.mount(1, z, "k5601_ss80_26x128"));
    initiere(r, "2");
    r.m.unmountDisk(1);
    std::string err;
    auto v = DiskVolume::open(z, "", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    ASSERT_TRUE(v->hasFileSystem());
    EXPECT_EQ(v->profile().name, "sios_ss80");
    EXPECT_EQ(v->check(FsCheckLevel::Schnell, false).zaehlerAb(FsSeverity::Warnung), 0);
    fs::remove(z);
}

// 5b. INIT mit Menue 1 im K5601 (einfacher Schritt, Positionen 0..39): SIOS schreibt eine Datei ueber Spur 0
// hinaus (COPD) und liest sie mit DIDI wieder; das DiskTool liest dieselben Bytes.
TEST(SiosRest, InitEinfachImK5601_DateiUeberSpur0HinausLesbar) {
    const std::string quelle = saetze(8);
    const std::string daten = datendiskette("k1520_sios_rest_w5b.hfe", {{"ALPHA", quelle}});
    const std::string z = leerdiskette("K5601", "k1520_sios_rest_w5b_z.hfe");
    Rig11 r;
    r.programmInA();
    ASSERT_TRUE(r.mount(1, z, "k5601_ss80_26x128"));
    initiere(r, "1");
    r.tippe("CAL");
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("CAL_")) << r.schirm();
    r.tippe("COPD");
    r.taste(r.et1);
    ASSERT_TRUE(r.warte("LOAD DISKS", 300'000'000LL)) << r.schirm();
    r.m.unmountDisk(0);
    ASSERT_TRUE(r.mount(0, daten, "k5601_ss40_26x128"));
    copdAlles(r);
    r.m.unmountDisk(0);
    r.m.unmountDisk(1);
    // Frischer Start: die beschriebene Diskette in B:, DIDI (Programmdiskette in A:) liest sie mit SIOS-Treibern.
    Rig11 r2;
    r2.programmInA();
    ASSERT_TRUE(r2.mount(1, z, "k5601_ss80_26x128"));
    r2.tippe("CAL");
    r2.taste(r2.et1);
    ASSERT_TRUE(r2.warte("CAL_")) << r2.schirm();
    r2.tippe("DIDI");
    r2.taste(r2.et1);
    ASSERT_TRUE(r2.warte("DIDI 1520 (SIEX) V 1.1", 400'000'000LL)) << r2.schirm();
    r2.feld("5");
    r2.feld("A");
    r2.feld("ALPHA");
    ASSERT_TRUE(r2.warte("HDR1 ALPHA", 100'000'000LL)) << r2.schirm();
    r2.feld("00008");
    ASSERT_TRUE(r2.warte("CHECK", 100'000'000LL)) << r2.schirm();
    r2.feld("");
    EXPECT_TRUE(r2.warte("SATZ 08 ....", 100'000'000LL)) << r2.schirm();

    r2.m.unmountDisk(0);
    r2.m.unmountDisk(1);
    const std::string got = holeDatei(z, "ALPHA", "w5b");
    ASSERT_GE(got.size(), quelle.size());
    EXPECT_EQ(got.substr(0, quelle.size()), quelle);
    fs::remove(daten);
    fs::remove(z);
}
