/**
 * @file test_gun_usb_system.cpp
 * @brief GUN-USB (Entwurf 50, AP U2): die ECHTEN UTools 1.4 (USB, UDIR, UPUT, UGET) gegen die Karte.
 *
 * Die Programme (Leubner/Haensel, CP/M-Fassung fuer PIO DCh..DFh) liegen NUR lokal in
 * `~/Documents/K1520emu/GIDE_Software/utools14/` (Lizenz ungeklaert, nie im Repo; `K1520_GIDE_SW`
 * ueberschreibt den Ort der GIDE_Software).  Fehlen sie, werden die Faelle uebersprungen.  Sie kommen
 * auf eine TEMP-KOPIE der CP/A-Bootdiskette; die Karte steckt auf Basis DCh (frei einstellbar, kein
 * Patchen noetig) mit einem Temp-Verzeichnis als Stick.
 *
 * Gepruft: USB (Sync, FWV), UDIR zeigt die Stickdatei, UPUT legt eine CP/A-Datei bitgleich im Wirtsverzeichnis
 * ab, UGET holt eine Wirtsdatei bitgleich auf die CP/A-Diskette.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "core/filesystem/cpm/cpm_fs.h"
#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
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
namespace fs = std::filesystem;

namespace {

constexpr int kBootBudget  = 90'000'000;
constexpr int kInputBudget = 40'000'000;
constexpr int kLangBudget  = 200'000'000;
constexpr const char* kFs = "cpa_ds80_5x1024_boot";

std::string swVerzeichnis() {
    if (const char* e = std::getenv("K1520_GIDE_SW")) return e;
    if (const char* h = std::getenv("HOME")) return std::string(h) + "/Documents/K1520emu/GIDE_Software";
    return "";
}
std::vector<uint8_t> dateiLesen(const fs::path& pfad) {
    std::ifstream f(pfad, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}
void dateiSchreiben(const fs::path& pfad, const std::vector<uint8_t>& d) {
    std::ofstream f(pfad, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
}

const FormatCatalog& formate() {
    static FormatCatalog c = [] { std::string f; return FormatCatalog::loadDefault(&f); }();
    return c;
}
const FsCatalog& dateisysteme() {
    static FsCatalog c = [] { std::string f; return FsCatalog::loadDefault(formate(), &f); }();
    return c;
}
bool diskettePut(const std::string& pfad, const std::vector<std::pair<std::string, std::vector<uint8_t>>>& dateien) {
    const FsProfile* p = dateisysteme().find(kFs);
    if (!p) return false;
    const DiskFormat* f = formate().find(p->format);
    if (!f) return false;
    auto disk = DiskImage::open(pfad, std::optional<DiskFormat>(*f), false);
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
std::vector<uint8_t> dateiVonDiskette(const std::string& pfad, const std::string& name) {
    const FsProfile* p = dateisysteme().find(kFs);
    const DiskFormat* f = formate().find(p->format);
    auto disk = DiskImage::open(pfad, std::optional<DiskFormat>(*f), true);
    std::vector<uint8_t> out;
    if (!disk) return out;
    SectorSpace space(disk->medium(), *f);
    std::string err;
    auto vol = CpmFileSystem::mount(space, *p, err);
    if (vol) vol->read(name, out);
    return out;
}

/// Bildschirm zur Diagnose an die Datei anhaengen, die K1520_TEST_ZEIGEN nennt (ctest verschluckt stderr gruener Faelle).
void zeige(const char* was, A5120Machine& m) {
    if (const char* d = std::getenv("K1520_TEST_ZEIGEN")) {
        std::ofstream f(d, std::ios::app);
        f << "=== " << was << " ===\n" << vramText(m) << "\n";
    }
}

/// Kommando tippen und bis zum NEUEN Prompt laufen (Schirm vorher wischen).
bool lauf(A5120Machine& m, const std::string& cmd, int budget = kInputBudget) {
    k1520test::wipeVram(m);
    typeString(m, cmd);
    typeKey(m, k1520test::QK_RETURN);
    return k1520test::runSmallUntilVisible(m, "A>", budget);
}

struct Leise : ::testing::Test {
    void SetUp() override { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN); }
};
using GunUsbSystem = Leise;

}  // namespace

/**
 * @test GunUsbSystem/UToolsA5120
 * @brief CP/A am A5120, Karte auf DCh..DFh: USB FWV, UDIR, UPUT, UGET mit den echten UTools-Binaerdateien.
 */
TEST_F(GunUsbSystem, UToolsA5120) {
    const fs::path sw = fs::path(swVerzeichnis()) / "utools14";
    const auto usb = dateiLesen(sw / "usb.com"), udir = dateiLesen(sw / "udir.com"),
               uput = dateiLesen(sw / "uput.com"), uget = dateiLesen(sw / "uget.com");
    if (usb.empty() || udir.empty() || uput.empty() || uget.empty())
        GTEST_SKIP() << "UTools nicht vorhanden (" << sw.string() << ")";

    TempDisk a("cpa_cpa780_k5601_noclock.img", "k1520_gunusb_a.img");
    std::vector<uint8_t> nutz(5000), stickDatei(3000);
    for (size_t i = 0; i < nutz.size(); ++i) nutz[i] = uint8_t((i * 7 + (i >> 8)) & 0xFF);
    for (size_t i = 0; i < stickDatei.size(); ++i) stickDatei[i] = uint8_t((i * 13 + 5 + (i >> 7)) & 0xFF);
    ASSERT_TRUE(diskettePut(a.path(), {{"USB.COM", usb}, {"UDIR.COM", udir}, {"UPUT.COM", uput},
                                       {"UGET.COM", uget}, {"NUTZ.BIN", nutz}}));

    const fs::path stick = k1520test::tempPath("k1520_gunusb_stick");
    std::error_code ec;
    fs::remove_all(stick, ec);
    fs::create_directories(stick);
    dateiSchreiben(stick / "STICK.BIN", stickDatei);

    {
        A5120Machine m;
        k1520::GunUsb::Config gc;
        gc.basis = 0xDC;   // die UTools-CP/M-Fassung ist fuer DCh..DFh gebaut
        gc.ordner = stick;
        ASSERT_TRUE(m.installGunUsb(gc)) << m.gunUsbFehler();
        ASSERT_TRUE(m.mountDisk(0, a.path(), "cpa780", false)) << m.lastError();
        m.powerOn();
        ASSERT_TRUE(runUntilVramContains(m, "TPA ist OK!", kBootBudget)) << vramText(m);
        ASSERT_TRUE(runSmallUntil(m, "A>", kInputBudget)) << vramText(m);

        ASSERT_TRUE(lauf(m, "USB FWV", kLangBudget)) << vramText(m);
        zeige("USB FWV", m);
        EXPECT_EQ(vramText(m).find("Error"), std::string::npos) << vramText(m);
        EXPECT_GE(m.gunUsb()->zaehler().resets, 1u);

        ASSERT_TRUE(lauf(m, "UDIR", kLangBudget)) << vramText(m);
        zeige("UDIR", m);
        EXPECT_NE(vramText(m).find("STICK"), std::string::npos) << vramText(m);

        ASSERT_TRUE(lauf(m, "UPUT NUTZ.BIN", kLangBudget)) << vramText(m);
        zeige("UPUT NUTZ.BIN", m);
        ASSERT_TRUE(lauf(m, "UGET STICK.BIN", kLangBudget)) << vramText(m);
        zeige("UGET STICK.BIN", m);
        ASSERT_TRUE(lauf(m, "DIR", kInputBudget)) << vramText(m);
        zeige("DIR", m);
        ASSERT_TRUE(m.flushDisks());
    }
    // UPUT: Wirtsdatei bitgleich (auf 128-B-Grenze aufgefuellt, wie CP/M-Dateien sind)
    const auto hoch = dateiLesen(stick / "NUTZ.BIN");
    ASSERT_GE(hoch.size(), nutz.size());
    EXPECT_LE(hoch.size(), nutz.size() + 127);
    EXPECT_TRUE(std::equal(nutz.begin(), nutz.end(), hoch.begin())) << "UPUT-Datei weicht ab";
    // UGET: CP/A-Datei bitgleich
    const auto runter = dateiVonDiskette(a.path(), "STICK.BIN");
    ASSERT_GE(runter.size(), stickDatei.size());
    EXPECT_LE(runter.size(), stickDatei.size() + 127);
    EXPECT_TRUE(std::equal(stickDatei.begin(), stickDatei.end(), runter.begin())) << "UGET-Datei weicht ab";
    fs::remove_all(stick, ec);
}
