/**
 * @file test_gide_austausch.cpp
 * @brief GIDE-Platten im DiskTool: AUSTAUSCH mit dem Emulator-Gast (echte Treiber, nur lokal).
 *
 * Gast schreibt, DiskTool liest bitgleich - und umgekehrt: das DiskTool legt eine Datei ab, ein frisch gebooteter
 * Gast liest sie mit PIP.  Poppe (`GIDEHPD0`, CP/A am A5120) und Kingstener (`gidn`, SCPX am K8915).  Fehlen die
 * Fremdtreiber (Lizenz ungeklaert, nie im Repo), werden die Faelle uebersprungen; das eingecheckte Golden-Fixture
 * (`GidePlatteGolden.*`, test_gide_platte.cpp) deckt die Poppe-Richtung Gast -> DiskTool dann ohne Fremdsoftware ab.
 *
 * Mit `K1520_GIDE_GOLDEN_OUT=<datei.img.gz>` schreibt der Poppe-Fall zusaetzlich das Golden-Fixture neu.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "core/filesystem/cpm/cpm_fs.h"
#include "core/filesystem/gide/gide_platte.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/filesystem/disk_volume.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/k8915/k8915.h"
#include "core/peripherals/floppy_drive/disk_image.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"

using k1520test::runSmallUntil;
using k1520test::runUntilVramContains;
using k1520test::TempDisk;
using k1520test::typeKey;
using k1520test::typeString;
using k1520test::vramText;
using k1520test::vramLines;
namespace fs = std::filesystem;

namespace {

constexpr int kBootBudget  = 90'000'000;
constexpr int kInputBudget = 40'000'000;
constexpr int kLangBudget  = 400'000'000;   // Verzeichnis loeschen/Kopieren ueber 8-MB-Platten

std::string swVerzeichnis() {
    if (const char* e = std::getenv("K1520_GIDE_SW")) return e;
    if (const char* h = std::getenv("HOME")) return std::string(h) + "/Documents/K1520emu/GIDE_Software";
    return "";
}

std::vector<uint8_t> dateiLesen(const std::string& pfad) {
    std::ifstream f(pfad, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}

/// Fremdsoftware holen; leer = nicht vorhanden (Test ueberspringen).
std::vector<uint8_t> fremd(const std::string& unter) {
    return dateiLesen(swVerzeichnis() + "/" + unter);
}

const FormatCatalog& formate() {
    static FormatCatalog c = [] { std::string f; return FormatCatalog::loadDefault(&f); }();
    return c;
}
const FsCatalog& dateisysteme() {
    static FsCatalog c = [] { std::string f; return FsCatalog::loadDefault(formate(), &f); }();
    return c;
}

/// Dateien (Name -> Inhalt) auf eine CP/M-Diskette schreiben bzw. eine lesen.
bool diskettePut(const std::string& pfad, const char* fsname,
                 const std::vector<std::pair<std::string, std::vector<uint8_t>>>& dateien) {
    const FsProfile* p = dateisysteme().find(fsname);
    if (!p) return false;
    const DiskFormat* f = formate().find(p->format);
    if (!f) return false;
    const bool istImg = pfad.size() > 4 && pfad.compare(pfad.size() - 4, 4, ".img") == 0;
    auto disk = DiskImage::open(pfad, istImg ? std::optional<DiskFormat>(*f) : std::nullopt, false);
    if (!disk) return false;
    SectorSpace space(disk->medium(), *f);
    std::string err;
    auto vol = CpmFileSystem::mount(space, *p, err);
    if (!vol) return false;
    for (const auto& d : dateien) {
        WriteOptions o;
        o.overwrite = true;
        if (!vol->write(d.first, d.second, o)) return false;
    }
    return disk->flush();
}

std::vector<uint8_t> dateiVonDiskette(const std::string& pfad, const char* fsname, const std::string& name) {
    const FsProfile* p = dateisysteme().find(fsname);
    const DiskFormat* f = formate().find(p->format);
    const bool istImg = pfad.size() > 4 && pfad.compare(pfad.size() - 4, 4, ".img") == 0;
    auto disk = DiskImage::open(pfad, istImg ? std::optional<DiskFormat>(*f) : std::nullopt, true);
    std::vector<uint8_t> out;
    if (!disk) return out;
    SectorSpace space(disk->medium(), *f);
    std::string err;
    auto vol = CpmFileSystem::mount(space, *p, err);
    if (vol) vol->read(name, out);
    return out;
}

bool kommando(A5120Machine& m, const std::string& cmd, const std::string& erwartet, int budget = kInputBudget) {
    typeString(m, cmd);
    typeKey(m, k1520test::QK_RETURN);
    return runSmallUntil(m, erwartet, budget);
}

/// Kommando eingeben und bis zum NEUEN Prompt laufen: Schirm vorher wischen, sonst steht der alte `A>` noch da.
bool lauf(A5120Machine& m, const std::string& cmd, int budget = kInputBudget) {
    k1520test::wipeVram(m);
    typeString(m, cmd);
    typeKey(m, k1520test::QK_RETURN);
    return k1520test::runSmallUntilVisible(m, "A>", budget);
}

struct Leise : ::testing::Test {
    void SetUp() override { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN); }
};
using GideAustausch = Leise;

/// Muster-Nutzdaten (mehrere Bloecke und Extents).
std::vector<uint8_t> muster(size_t n, int mul) {
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = uint8_t((i * mul + (i >> 8)) & 0xFF);
    return v;
}

bool beginntMit(const std::vector<uint8_t>& soll, const std::vector<uint8_t>& ist) {
    return ist.size() >= soll.size() && std::equal(soll.begin(), soll.end(), ist.begin());
}

}  // namespace

namespace {

constexpr int       kSchritt = 100'000;
constexpr long long kBefehl  = 400'000'000;

bool bis(K8915Machine& m, const std::string& text, long long frist) {
    auto da = [&] { return k1520test::vramText(m).find(text) != std::string::npos; };
    for (long long t = 0; t < frist; t += m.run(kSchritt))
        if (da()) return true;
    return da();
}

std::string letzteZeile(K8915Machine& m) {
    const std::string t = vramText(m);
    std::string letzte;
    for (size_t z = 0; z + 80 <= t.size(); z += 80) {
        std::string zeile = t.substr(z, 80);
        while (!zeile.empty() && (zeile.back() == ' ' || zeile.back() == '\0')) zeile.pop_back();
        if (!zeile.empty()) letzte = zeile;
    }
    return letzte;
}

void eingabe(K8915Machine& m, const std::string& s) {
    for (char c : s) m.keyboard().sendeZeichen(static_cast<uint8_t>(c));
}

/// Kommando tippen und bis zum naechsten Prompt (Muster test_raf_k8915.cpp).
bool befehl(K8915Machine& m, const std::string& cmd, const std::string& zwischen = "", char antwort = 0) {
    eingabe(m, cmd);
    m.keyboard().sendeZeichen(0x0D);
    bool echo = false, beantwortet = antwort == 0;
    for (long long t = 0; t < kBefehl; t += m.run(kSchritt)) {
        if (!beantwortet && vramText(m).find(zwischen) != std::string::npos) {
            m.keyboard().sendeZeichen(static_cast<uint8_t>(antwort));
            beantwortet = true;
        }
        const bool prompt = letzteZeile(m) == "A>";
        echo = echo || !prompt;
        if (beantwortet && echo && prompt && !m.keyboard().sendetNoch() && m.memReadDebug(0xF150) == 0) return true;
    }
    return false;
}

std::string ausgabe(K8915Machine& m, const std::string& cmd) {
    const std::string t = vramText(m);
    const size_t p = t.rfind("A>" + cmd);
    return p == std::string::npos ? std::string() : t.substr(p);
}

bool kaltstartK8915(K8915Machine& m) {
    m.powerOn();
    m.zre().bankPoke(0, 0x0000, 0xC3);   // JP bei 0000H/0005H: Selbsttest uebergehen (wie test_k8915_scpx.cpp)
    m.zre().bankPoke(0, 0x0005, 0xC3);
    if (!bis(m, "* Coldstart *  Disk on A: ready", 5'000'000)) return false;
    m.keyboard().sendeZeichen(0x0D);
    if (!bis(m, "size: 1 kByte groups 0 ... 03FH", 150'000'000)) return false;
    for (long long t = 0; t < 20'000'000 && letzteZeile(m) != "A>"; t += m.run(kSchritt)) {}
    return letzteZeile(m) == "A>";
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Poppe am A5120 (CP/A): Gast -> DiskTool, dann DiskTool -> Gast
// ---------------------------------------------------------------------------------------------------------------

/// CP/A booten, GIDEHPD0 installieren (Basis D0H).
static bool poppeBoot(A5120Machine& m, const std::string& disk, const std::string& abbild) {
    k1520::Gide::Config gc;
    gc.basis = 0xD0;
    gc.rtc_vorhanden = false;
    if (!m.installGide(gc, nullptr, abbild)) return false;
    if (!m.mountDisk(0, disk, "cpa780", false)) return false;
    m.powerOn();
    if (!runUntilVramContains(m, "TPA ist OK!", kBootBudget)) return false;
    if (!runSmallUntil(m, "A>", kInputBudget)) return false;
    return lauf(m, "GIDEHPD0", kLangBudget) && vramText(m).find("GIDE-Treiber installiert") != std::string::npos;
}

static bool eradir(A5120Machine& m, const char* lw) {
    k1520test::wipeVram(m);
    typeString(m, std::string("ERADIR ") + lw);
    typeKey(m, k1520test::QK_RETURN);
    if (!runSmallUntil(m, "(y/n, CR=n)", kLangBudget)) return false;
    typeString(m, "Y");
    typeKey(m, k1520test::QK_RETURN);
    return k1520test::runSmallUntilVisible(m, "A>", kLangBudget) &&
           vramText(m).find("Successful Directory erase") != std::string::npos;
}

TEST_F(GideAustausch, PoppeGastSchreibtDiskToolLiestUndUmgekehrt) {
    const auto treiber = fremd("poppe_gidehp/gidehpd0.com");
    const auto era     = fremd("poppe_gidehp/eradir.com");
    if (treiber.empty() || era.empty()) GTEST_SKIP() << "Fremdsoftware nicht vorhanden (" << swVerzeichnis() << ")";

    const auto nutz = muster(20000, 7);
    const std::string hallo = "Hallo von Poppe, geschrieben im Emulator.\r\n";
    const std::vector<uint8_t> halloB(hallo.begin(), hallo.end());
    const std::string abbild = k1520test::tempPath("k1520_gide_austausch_poppe.img");
    std::error_code ec;
    fs::remove(abbild, ec);
    ASSERT_TRUE(k1520::AtaGeraet::abbildAnlegen(abbild, 504'100));

    // ---- Gast -> DiskTool ----
    {
        TempDisk a("cpa_cpa780_k5601_noclock.img", "k1520_gide_austausch_a.img");
        ASSERT_TRUE(diskettePut(a.path(), "cpa_ds80_5x1024_boot",
                                {{"GIDEHPD0.COM", treiber}, {"ERADIR.COM", era}, {"NUTZ.BIN", nutz}, {"HALLO.TXT", halloB}}));
        A5120Machine m;
        ASSERT_TRUE(poppeBoot(m, a.path(), abbild)) << vramText(m);
        ASSERT_TRUE(eradir(m, "D:")) << vramText(m);
        // Treiber-/BDOS-Eigenheit (test_gide_system.cpp): nach ERADIR sucht der ERSTE Befehl noch auf D: - erst D:, dann A: ansprechen
        ASSERT_TRUE(lauf(m, "DIR D:", kLangBudget)) << vramText(m);
        ASSERT_TRUE(lauf(m, "DIR A:", kLangBudget)) << vramText(m);
        ASSERT_TRUE(eradir(m, "E:")) << vramText(m);
        ASSERT_TRUE(lauf(m, "DIR E:", kLangBudget)) << vramText(m);
        ASSERT_TRUE(lauf(m, "DIR A:", kLangBudget)) << vramText(m);
        ASSERT_TRUE(lauf(m, "PIP D:=A:NUTZ.BIN", kLangBudget)) << vramText(m);
        ASSERT_TRUE(lauf(m, "PIP E:=A:HALLO.TXT", kLangBudget)) << vramText(m);
        ASSERT_TRUE(m.flushDisks());
        ASSERT_TRUE(m.gide()->platte().flush());
    }
    {
        std::string err;
        auto p = GidePlatte::open(abbild, err, true);                 // Layout wird ERKANNT
        ASSERT_TRUE(p) << err;
        EXPECT_EQ(p->layout(), GideLayout::Poppe);
        EXPECT_TRUE(p->erkennung().sicher);
        EXPECT_FALSE(p->erkennung().mehrdeutig);
        EXPECT_EQ(p->erkennung().poppe_laufwerke, (std::vector<int>{0, 1})) << "ERADIR D: und E: = Laufwerk 0 und 1";
        std::vector<uint8_t> d;
        ASSERT_TRUE(p->read("lw0/NUTZ.BIN", d)) << p->lastError();
        EXPECT_TRUE(beginntMit(nutz, d)) << "Gast-Datei weicht ab";
        EXPECT_EQ(d.size(), nutz.size() + (128 - nutz.size() % 128) % 128);
        ASSERT_TRUE(p->read("lw1/HALLO.TXT", d)) << p->lastError();
        EXPECT_TRUE(beginntMit(halloB, d));
        const auto fsck = p->check(FsCheckLevel::Voll);
        EXPECT_LE(int(fsck.hoechste()), int(FsSeverity::Info)) << fsck.alsText();
    }
    if (const char* g = std::getenv("K1520_GIDE_GOLDEN_OUT")) {
        std::vector<uint8_t> roh;
        std::string err;
        ASSERT_TRUE(k1520::gzip::laden(abbild, roh, nullptr, &err)) << err;
        fs::remove(g, ec);
        ASSERT_TRUE(k1520::gzip::speichern(g, roh.data(), roh.size(), k1520::gzip::Art::Gzip, 9, &err)) << err;
    }

    // ---- DiskTool -> Gast ----
    const auto rueber = muster(30000, 11);
    {
        std::string err;
        auto p = GidePlatte::open(abbild, err, false);                // schreibbar, Layout erkannt
        ASSERT_TRUE(p) << err;
        WriteOptions o;
        ASSERT_TRUE(p->write("lw0/RUEBER.BIN", rueber, o)) << p->lastError();
        ASSERT_TRUE(p->write("lw1/KLEIN.TXT", halloB, o)) << p->lastError();
        ASSERT_TRUE(p->erase("lw0/NUTZ.BIN")) << p->lastError();
        ASSERT_TRUE(p->flush()) << p->lastError();
    }
    {
        TempDisk a("cpa_cpa780_k5601_noclock.img", "k1520_gide_austausch_b.img");
        ASSERT_TRUE(diskettePut(a.path(), "cpa_ds80_5x1024_boot", {{"GIDEHPD0.COM", treiber}}));
        A5120Machine m;
        ASSERT_TRUE(poppeBoot(m, a.path(), abbild)) << vramText(m);
        ASSERT_TRUE(lauf(m, "DIR D:", kLangBudget)) << vramText(m);
        const std::string dir = vramText(m);
        EXPECT_NE(dir.find("D: RUEBER   BIN"), std::string::npos) << dir;
        EXPECT_EQ(dir.find("NUTZ"), std::string::npos) << "vom DiskTool geloescht";
        ASSERT_TRUE(lauf(m, "DIR A:", kLangBudget)) << vramText(m);
        ASSERT_TRUE(lauf(m, "PIP A:BACK.BIN=D:RUEBER.BIN", kLangBudget)) << vramText(m);
        ASSERT_TRUE(lauf(m, "PIP A:KLEIN.TXT=E:KLEIN.TXT", kLangBudget)) << vramText(m);
        ASSERT_TRUE(m.flushDisks());
        const auto back = dateiVonDiskette(a.path(), "cpa_ds80_5x1024_boot", "BACK.BIN");
        EXPECT_TRUE(beginntMit(rueber, back)) << "DiskTool-Datei vom Gast anders gelesen (Groesse " << back.size() << ")";
        EXPECT_TRUE(beginntMit(halloB, dateiVonDiskette(a.path(), "cpa_ds80_5x1024_boot", "KLEIN.TXT")));
    }
    fs::remove(abbild, ec);
}

// ---------------------------------------------------------------------------------------------------------------
// Kingstener am K8915 (SCPX 8915): Gast -> DiskTool, dann DiskTool -> Gast
// ---------------------------------------------------------------------------------------------------------------

TEST_F(GideAustausch, KingstenerGastSchreibtDiskToolLiestUndUmgekehrt) {
    const auto treiber = fremd("dl_gidn/gidn@D0h.com");
    if (treiber.empty()) GTEST_SKIP() << "Fremdsoftware nicht vorhanden (" << swVerzeichnis() << ")";

    const auto nutz = muster(20000, 7);
    const auto rueber = muster(15000, 13);
    const std::string abbild = k1520test::tempPath("k1520_gide_austausch_king.img");
    std::error_code ec;
    fs::remove(abbild, ec);
    ASSERT_TRUE(k1520::AtaGeraet::abbildAnlegen(abbild, 65'600));

    std::string f, err;
    static const FormatCatalog fk = FormatCatalog::loadDefault(&f);
    static const FsCatalog     fsk = FsCatalog::loadDefault(fk, &f);
    auto diskeMit = [&](TempDisk& a, const std::vector<std::pair<std::string, std::vector<uint8_t>>>& dateien) {
        const std::string pfad = k1520test::tempPath("k1520_gide_austausch_k.dat");
        auto vol = DiskVolume::open(a.path(), "", fk, fsk, err, /*read_only=*/false);
        if (!vol) return false;
        for (const auto& d : dateien) {
            { std::ofstream o(pfad, std::ios::binary); o.write(reinterpret_cast<const char*>(d.second.data()), std::streamsize(d.second.size())); }
            FileRef r; r.name = d.first;
            if (!vol->insert(pfad, r, TransferOptions{})) return false;
        }
        fs::remove(pfad, ec);
        return vol->flush();
    };
    auto diskeLies = [&](TempDisk& a, const std::string& name) {
        const std::string pfad = k1520test::tempPath("k1520_gide_austausch_k2.dat");
        auto vol = DiskVolume::open(a.path(), "", fk, fsk, err, /*read_only=*/true);
        std::vector<uint8_t> out;
        if (vol) { FileRef r; r.name = name; if (vol->extract(r, pfad, TransferOptions{})) out = dateiLesen(pfad); }
        fs::remove(pfad, ec);
        return out;
    };

    // ---- Gast -> DiskTool ----
    {
        TempDisk a("k8915scpx_cpa800_k5601_bios55k-disk900.hfe", "k1520_gide_austausch_k8915a.hfe");
        ASSERT_TRUE(diskeMit(a, {{"GIDN.COM", treiber}, {"NUTZ.BIN", nutz}})) << err;
        K8915Machine m;
        k1520::Gide::Config gc;
        gc.basis = 0xD0;
        gc.rtc_vorhanden = false;
        ASSERT_TRUE(m.installGide(gc, nullptr, abbild)) << m.gideFehler();
        ASSERT_TRUE(m.mountDisk(0, a.path(), "cpa800", false)) << m.lastError();
        ASSERT_TRUE(kaltstartK8915(m)) << vramLines(m);
        ASSERT_TRUE(befehl(m, "gidn /f", "formatieren J/N?", 'J')) << vramLines(m);
        ASSERT_TRUE(befehl(m, "pip n:=a:nutz.bin")) << vramLines(m);
        ASSERT_TRUE(m.flushDisks());
        ASSERT_TRUE(m.gide()->platte().flush());
    }
    {
        std::string e;
        auto p = GidePlatte::open(abbild, e, true);
        ASSERT_TRUE(p) << e;
        EXPECT_EQ(p->layout(), GideLayout::Kingstener);
        EXPECT_TRUE(p->erkennung().sicher);
        std::vector<uint8_t> d;
        ASSERT_TRUE(p->read("lw0/NUTZ.BIN", d)) << p->lastError();
        EXPECT_TRUE(beginntMit(nutz, d)) << "Gast-Datei weicht ab";
        // Treiberverhalten: jeder Hostsektor traegt den Satz vierfach (WrIDE) - fsck Voll ohne Warnung
        const auto fsck = p->check(FsCheckLevel::Voll);
        EXPECT_LE(int(fsck.hoechste()), int(FsSeverity::Info)) << fsck.alsText();
    }

    // ---- DiskTool -> Gast ----
    {
        std::string e;
        auto p = GidePlatte::open(abbild, e, false);
        ASSERT_TRUE(p) << e;
        ASSERT_TRUE(p->write("lw0/RUEBER.BIN", rueber, WriteOptions{})) << p->lastError();
        ASSERT_TRUE(p->flush()) << p->lastError();
    }
    {
        TempDisk a("k8915scpx_cpa800_k5601_bios55k-disk900.hfe", "k1520_gide_austausch_k8915b.hfe");
        ASSERT_TRUE(diskeMit(a, {{"GIDN.COM", treiber}})) << err;
        K8915Machine m;
        k1520::Gide::Config gc;
        gc.basis = 0xD0;
        gc.rtc_vorhanden = false;
        ASSERT_TRUE(m.installGide(gc, nullptr, abbild)) << m.gideFehler();
        ASSERT_TRUE(m.mountDisk(0, a.path(), "cpa800", false)) << m.lastError();
        ASSERT_TRUE(kaltstartK8915(m)) << vramLines(m);
        ASSERT_TRUE(befehl(m, "gidn")) << vramLines(m);
        ASSERT_TRUE(befehl(m, "dir n:")) << vramLines(m);
        EXPECT_NE(ausgabe(m, "dir n:").find(": RUEBER   BIN"), std::string::npos) << vramLines(m);
        ASSERT_TRUE(befehl(m, "pip a:back.bin=n:rueber.bin")) << vramLines(m);
        ASSERT_TRUE(m.flushDisks());
        EXPECT_TRUE(beginntMit(rueber, diskeLies(a, "BACK.BIN"))) << "DiskTool-Datei vom Gast anders gelesen";
    }
    fs::remove(abbild, ec);
}
