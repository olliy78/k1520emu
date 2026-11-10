/**
 * @file test_gide_system.cpp
 * @brief GIDE (Entwurf 50, AP G5c): ECHTE Fremdtreiber gegen die Karte.
 *
 * Die Treiber (Heiko Poppe `GIDEHPD0.COM`, Kingstener `gidn@D0h.com`) liegen NUR lokal in
 * `~/Documents/K1520emu/GIDE_Software/` (Lizenz ungeklaert, nie im Repo; `K1520_GIDE_SW` ueberschreibt
 * den Ort).  Fehlen sie, werden die Faelle uebersprungen.  Sie werden auf eine TEMP-KOPIE einer
 * CP/A-/SCPX-Bootdiskette geschrieben, die Maschine bootet, der Treiber installiert sich und
 * die Platte (leeres Abbild) wird wie ein Laufwerk benutzt.
 *
 * Was der Poppe-Treiber vom Modell verlangt: Soft-Reset ueber DOR (06H dann 02H), danach Status
 * mit (S & D1H) == 50H, Diagnose 90H (Error = 01H), Identify ECH, CHS-Adressierung ohne 91H mit
 * Kopf = Laufwerk (SDH A0H+n), Sektor 1..32, Zylinder bis 499 — die Abbildgroesse muss also die
 * abgeleitete Geometrie (16 x 63) bis Zylinder 500 tragen (504 000 Sektoren).
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
using GideSystem = Leise;

}  // namespace

/**
 * @test GideSystem/PoppeTreiberA5120
 * @brief CP/A am A5120: GIDEHPD0 (Basis D0H) erkennt die Platte, ERADIR C:, PIP hin und zurueck,
 *        Datei bitgleich, Abbild auf dem Wirt enthaelt die Daten.
 */
TEST_F(GideSystem, PoppeTreiberA5120) {
    const auto treiber = fremd("poppe_gidehp/gidehpd0.com");
    const auto eradir  = fremd("poppe_gidehp/eradir.com");
    if (treiber.empty() || eradir.empty()) GTEST_SKIP() << "Fremdsoftware nicht vorhanden (" << swVerzeichnis() << ")";

    TempDisk a("cpa_cpa780_k5601_noclock.img", "k1520_gide_a.img");
    // Nutzdatei: 20000 Bytes Muster (mehrere 8-KB-Bloecke und Extents)
    std::vector<uint8_t> nutz(20000);
    for (size_t i = 0; i < nutz.size(); ++i) nutz[i] = uint8_t((i * 7 + (i >> 8)) & 0xFF);
    ASSERT_TRUE(diskettePut(a.path(), "cpa_ds80_5x1024_boot",
                            {{"GIDEHPD0.COM", treiber}, {"ERADIR.COM", eradir}, {"NUTZ.BIN", nutz}}));

    const std::string abbild = k1520test::tempPath("k1520_gide_poppe.img");
    std::error_code ec;
    fs::remove(abbild, ec);
    ASSERT_TRUE(k1520::AtaGeraet::abbildAnlegen(abbild, 504'100));   // 500 Zyl x 16 Koepfe x 63 Sektoren

    {
        A5120Machine m;
        k1520::Gide::Config gc;
        gc.basis = 0xD0;
        gc.rtc_vorhanden = false;
        ASSERT_TRUE(m.installGide(gc, nullptr, abbild)) << m.gideFehler();
        ASSERT_TRUE(m.mountDisk(0, a.path(), "cpa780", false)) << m.lastError();
        m.powerOn();
        ASSERT_TRUE(runUntilVramContains(m, "TPA ist OK!", kBootBudget)) << vramText(m);
        ASSERT_TRUE(runSmallUntil(m, "A>", kInputBudget)) << vramText(m);

        ASSERT_TRUE(lauf(m, "GIDEHPD0", kLangBudget)) << vramText(m);
        const std::string t0 = vramText(m);
        EXPECT_NE(t0.find("GIDE-Treiber fuer CP/M 2.x  V 0.2"), std::string::npos) << t0;
        EXPECT_NE(t0.find("GIDE-Modul auf Adresse D0H"), std::string::npos) << t0;
        EXPECT_NE(t0.find("500 Cyl; 16 Head; 63 Sec;"), std::string::npos) << t0;
        EXPECT_NE(t0.find("D: E: F: G: H: I: J: K:"), std::string::npos) << t0;
        EXPECT_NE(t0.find("GIDE-Treiber installiert"), std::string::npos) << t0;

        // Verzeichnis von D: loeschen (ERADIR fragt y/n, wartet auf Return), dann hin- und zurueckkopieren.
        k1520test::wipeVram(m);
        typeString(m, "ERADIR D:");
        typeKey(m, k1520test::QK_RETURN);
        ASSERT_TRUE(runSmallUntil(m, "(y/n, CR=n)", kLangBudget)) << vramText(m);
        typeString(m, "Y");
        typeKey(m, k1520test::QK_RETURN);
        ASSERT_TRUE(k1520test::runSmallUntilVisible(m, "A>", kLangBudget)) << vramText(m);
        EXPECT_NE(vramText(m).find("Successful Directory erase"), std::string::npos) << vramText(m);

        // Frisch geloeschtes D: ist leer.  (Beobachtung am Treiber: der ERSTE Befehl nach ERADIR sucht noch auf D: statt
        // auf A: — BDOS ruft SELDSK nach dem Warmstart nicht erneut, hdisk bleibt gesetzt.  Erst ein Zugriff auf D: und
        // dann auf A: stellt das her; das ist Treiber-/BDOS-Verhalten, nicht die Karte.)
        ASSERT_TRUE(lauf(m, "DIR D:", kLangBudget)) << vramText(m);
        EXPECT_NE(vramText(m).find("No File"), std::string::npos) << vramText(m);
        ASSERT_TRUE(lauf(m, "DIR A:", kLangBudget)) << vramText(m);
        EXPECT_NE(vramText(m).find("NUTZ"), std::string::npos) << vramText(m);
        ASSERT_TRUE(lauf(m, "PIP D:=A:NUTZ.BIN", kLangBudget)) << vramText(m);
        EXPECT_EQ(vramText(m).find("PIP?"), std::string::npos) << vramText(m);
        ASSERT_TRUE(lauf(m, "DIR D:", kLangBudget)) << vramText(m);
        EXPECT_NE(vramText(m).find("D: NUTZ     BIN"), std::string::npos) << vramText(m);
        ASSERT_TRUE(lauf(m, "PIP A:ZURUECK.BIN=D:NUTZ.BIN", kLangBudget)) << vramText(m);
        ASSERT_TRUE(lauf(m, "DIR A:ZURUECK.BIN", kLangBudget)) << vramText(m);
        EXPECT_NE(vramText(m).find("A: ZURUECK  BIN"), std::string::npos) << vramText(m);
        ASSERT_TRUE(m.flushDisks());
        ASSERT_TRUE(m.gide()->platte().flush());
        const auto zurueck = dateiVonDiskette(a.path(), "cpa_ds80_5x1024_boot", "ZURUECK.BIN");
        EXPECT_EQ(zurueck.size(), nutz.size() + (128 - nutz.size() % 128) % 128);
        ASSERT_GE(zurueck.size(), nutz.size());
        EXPECT_TRUE(std::equal(nutz.begin(), nutz.end(), zurueck.begin())) << "Rueckkopie weicht ab";
    }
    {   // Das Abbild auf dem Wirt traegt die Daten: Muster des Nutzdatei-Blocks irgendwo in der Datei
        const auto img = dateiLesen(abbild);
        ASSERT_FALSE(img.empty());
        const std::vector<uint8_t> probe(nutz.begin() + 1000, nutz.begin() + 1064);
        EXPECT_NE(std::search(img.begin(), img.end(), probe.begin(), probe.end()), img.end())
            << "Nutzdaten stehen nicht im Plattenabbild";
    }
    fs::remove(abbild, ec);
}

// ---------------------------------------------------------------------------------------------------------------
// K8915 mit SCPX 8915 und dem nachladbaren Treiber von Kingstener (Laufwerk N:)
// ---------------------------------------------------------------------------------------------------------------
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

/**
 * @test GideSystem/KingstenerTreiberK8915
 * @brief SCPX 8915: `GIDN` (V0.4, Basis D0H, Laufwerk N:) gibt 91H, `GIDN /F` formatiert (256 Sektoren),
 *        PIP hin und zurueck, Datei bitgleich, Abbild auf dem Wirt traegt die Daten.
 */
TEST_F(GideSystem, KingstenerTreiberK8915) {
    const auto treiber = fremd("dl_gidn/gidn@D0h.com");
    if (treiber.empty()) GTEST_SKIP() << "Fremdsoftware nicht vorhanden (" << swVerzeichnis() << ")";

    TempDisk a("k8915scpx_cpa800_k5601_bios55k-disk900.hfe", "k1520_gide_k8915.hfe");
    const std::string treiberDatei = k1520test::tempPath("k1520_gide_gidn.com");
    const std::string nutzDatei    = k1520test::tempPath("k1520_gide_nutz8915.bin");
    const std::string zurueckDatei = k1520test::tempPath("k1520_gide_zurueck8915.bin");
    std::vector<uint8_t> nutz(20000);
    for (size_t i = 0; i < nutz.size(); ++i) nutz[i] = uint8_t((i * 7 + (i >> 8)) & 0xFF);
    { std::ofstream(treiberDatei, std::ios::binary).write(reinterpret_cast<const char*>(treiber.data()), std::streamsize(treiber.size())); }
    { std::ofstream(nutzDatei, std::ios::binary).write(reinterpret_cast<const char*>(nutz.data()), std::streamsize(nutz.size())); }

    std::string f, err;
    static const FormatCatalog fk = FormatCatalog::loadDefault(&f);
    static const FsCatalog     fsk = FsCatalog::loadDefault(fk, &f);
    {
        auto vol = DiskVolume::open(a.path(), "", fk, fsk, err, /*read_only=*/false);
        ASSERT_TRUE(vol) << err;
        FileRef r1; r1.name = "GIDN.COM";
        ASSERT_TRUE(vol->insert(treiberDatei, r1, TransferOptions{})) << vol->lastError();
        FileRef r2; r2.name = "NUTZ.BIN";
        ASSERT_TRUE(vol->insert(nutzDatei, r2, TransferOptions{})) << vol->lastError();
        ASSERT_TRUE(vol->flush()) << vol->lastError();
    }

    const std::string abbild = k1520test::tempPath("k1520_gide_gidn.img");
    std::error_code ec;
    fs::remove(abbild, ec);
    ASSERT_TRUE(k1520::AtaGeraet::abbildAnlegen(abbild, 65'600));   // 256 Zyl x 4 Koepfe x 64 Sektoren (+ Rest)
    {
        K8915Machine m;
        k1520::Gide::Config gc;
        gc.basis = 0xD0;
        gc.rtc_vorhanden = false;
        ASSERT_TRUE(m.installGide(gc, nullptr, abbild)) << m.gideFehler();
        ASSERT_TRUE(m.mountDisk(0, a.path(), "cpa800", false)) << m.lastError();
        ASSERT_TRUE(kaltstartK8915(m)) << vramLines(m);

        ASSERT_TRUE(befehl(m, "gidn /f", "formatieren J/N?", 'J')) << vramLines(m);
        const std::string t = ausgabe(m, "gidn /f");
        EXPECT_NE(t.find("Nachladbarer GIDE-Treiber V0.4"), std::string::npos) << vramLines(m);
        EXPECT_NE(t.find("Laufwerk formatieren J/N?"), std::string::npos) << vramLines(m);
        EXPECT_NE(t.find("GIDE als Laufwerk N: installiert"), std::string::npos) << vramLines(m);

        ASSERT_TRUE(befehl(m, "pip n:=a:nutz.bin")) << vramLines(m);
        ASSERT_TRUE(befehl(m, "dir n:")) << vramLines(m);
        EXPECT_NE(ausgabe(m, "dir n:").find("N: NUTZ     BIN"), std::string::npos) << vramLines(m);
        ASSERT_TRUE(befehl(m, "pip a:zurueck.bin=n:nutz.bin")) << vramLines(m);
        ASSERT_TRUE(befehl(m, "dir a:zurueck.bin")) << vramLines(m);
        EXPECT_NE(ausgabe(m, "dir a:zurueck.bin").find("A: ZURUECK  BIN"), std::string::npos) << vramLines(m);
        ASSERT_TRUE(m.flushDisks());
        ASSERT_TRUE(m.gide()->platte().flush());
    }
    {
        auto vol = DiskVolume::open(a.path(), "", fk, fsk, err, /*read_only=*/true);
        ASSERT_TRUE(vol) << err;
        FileRef r; r.name = "ZURUECK.BIN";
        ASSERT_TRUE(vol->extract(r, zurueckDatei, TransferOptions{})) << vol->lastError();
    }
    const auto zurueck = dateiLesen(zurueckDatei);
    ASSERT_GE(zurueck.size(), nutz.size());
    EXPECT_TRUE(std::equal(nutz.begin(), nutz.end(), zurueck.begin())) << "Rueckkopie weicht ab";
    const auto img = dateiLesen(abbild);
    const std::vector<uint8_t> probe(nutz.begin() + 1000, nutz.begin() + 1064);
    EXPECT_NE(std::search(img.begin(), img.end(), probe.begin(), probe.end()), img.end())
        << "Nutzdaten stehen nicht im Plattenabbild";
    for (const auto& p : {treiberDatei, nutzDatei, zurueckDatei, abbild}) fs::remove(p, ec);
}
