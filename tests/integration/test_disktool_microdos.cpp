/**
 * @file test_disktool_microdos.cpp
 * @brief DiskTool fuer MicroDOS 1526 / 1715 (Rostock, CP/M-2.2-Anpassung), doc/design/13_k1520disktool.md §16.
 *
 * Eingechecktes Material sind nur die Bootabbilder `data/bootsektoren/boot_microdos_*.bin`; die
 * Disketten entstehen hier per `create` (Fixture "MicroDOS-Leerdiskette").  Die Originalabzuege
 * (Sammlung, nicht im Repo) werden zusaetzlich geprueft, wenn `K1520_MICRODOS_DIR` auf ihren
 * Ordner zeigt (sonst uebersprungen).
 *
 *  - Erkennung: SYL-Lader (acht Plaetze) = positiver Nachweis; Datendiskette bleibt cpa800.
 *  - Der Lader gehoert dem Urlader: `list`/`put`/`rm`/`attr`/`fsck` fassen ihn nicht an.
 *  - `MicroDOS.SYS` liegt an FESTEN Sektoren (c1..c4 h0, Sektor 1-4) und ist gemischt geschrieben.
 *  - boot-get -> create --boot -> boot-get ist bitgleich; fremde Lader werden abgewiesen.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "core/filesystem/cpm/cpm_fs.h"
#include "core/filesystem/disk_volume.h"
#include "core/logger.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "tests/support/fixtures.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;

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

class TempPfad {
public:
    explicit TempPfad(const char* name) : pfad_(k1520test::tempPath(name)) { weg(); }
    ~TempPfad() { weg(); }
    const std::string& get() const { return pfad_; }
private:
    void weg() {
        std::error_code ec;
        fs::remove_all(pfad_, ec);
        fs::remove(pfad_ + "~", ec);
    }
    std::string pfad_;
};

std::vector<uint8_t> datei(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
void schreibe(const std::string& p, const std::vector<uint8_t>& d) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
}

/// data/bootsektoren/<name> (Quellbaum: von tests/fixtures/disks drei Ebenen hoch).
std::string bootBin(const char* name) {
    const fs::path fix = fs::path(k1520test::diskPath("x.hfe")).parent_path();
    return (fix / ".." / ".." / ".." / "data" / "bootsektoren" / name).lexically_normal().string();
}

std::unique_ptr<DiskVolume> oeffne(const std::string& pfad, bool schreibend = false,
                                   const char* fs_name = "") {
    std::string err;
    auto dv = DiskVolume::open(pfad, fs_name, formate(), dateisysteme(), err, !schreibend);
    EXPECT_NE(dv, nullptr) << pfad << ": " << err;
    return dv;
}

bool lege(DiskVolume& dv, const std::string& name, const std::vector<uint8_t>& daten) {
    TempPfad quelle("k1520_microdos_quelle.bin");
    schreibe(quelle.get(), daten);
    FileRef ref;
    ref.name = name;
    return dv.insert(quelle.get(), ref, TransferOptions{});
}

std::vector<uint8_t> hole(DiskVolume& dv, const std::string& name) {
    TempPfad ziel("k1520_microdos_hole.bin");
    FileRef ref;
    ref.name = name;
    EXPECT_TRUE(dv.extract(ref, ziel.get(), TransferOptions{})) << name << ": " << dv.lastError();
    return datei(ziel.get());
}

std::string kennungen(const FsCheckReport& r) {
    std::string s;
    for (const FsFinding& f : r.findings) s += f.id + " ";
    return s;
}

/// Neue MicroDOS-Systemdiskette (Lader + MicroDOS.SYS) im Profil @p profil.
std::unique_ptr<DiskVolume> neu(const TempPfad& pfad, const char* profil, const char* bin) {
    std::string err;
    auto dv = DiskVolume::create(pfad.get(), profil, "", formate(), dateisysteme(), err, bootBin(bin));
    EXPECT_NE(dv, nullptr) << err;
    return dv;
}

}  // namespace

/// @test Leere MicroDOS-Systemdiskette: Lader + MicroDOS.SYS, wird als microdos800 erkannt,
///       prueft ohne Befund; ohne Lader bleibt dieselbe Geometrie cpa800.
TEST(DisktoolMicrodos, ErzeugteSystemdisketteWirdErkanntUndPrueftSauber) {
    TempPfad pfad("k1520_microdos_neu.hfe");
    {
        auto dv = neu(pfad, "microdos800", "boot_microdos_1526.bin");
        ASSERT_NE(dv, nullptr);
        EXPECT_EQ(dv->bootAreaSize(), 256u + 16384u);
        ASSERT_TRUE(dv->flush()) << dv->lastError();
    }
    auto dv = oeffne(pfad.get());
    ASSERT_NE(dv, nullptr);
    EXPECT_EQ(dv->detection().filesystem, "microdos800");
    EXPECT_TRUE(dv->detection().unambiguous);
    const auto liste = dv->list();
    ASSERT_EQ(liste.size(), 1u);                       // nur MicroDOS.SYS, der Lader ist keine Datei
    EXPECT_EQ(liste[0].name, "MicroDOS.SYS");
    EXPECT_EQ(liste[0].size, 16384u);
    for (FsCheckLevel stufe : {FsCheckLevel::Schnell, FsCheckLevel::Voll}) {
        const FsCheckReport& r = dv->check(stufe, true);
        EXPECT_EQ(0, r.zaehlerAb(FsSeverity::Warnung)) << kennungen(r);
    }

    // Dieselbe Geometrie ohne Lader ist eine Datendiskette: cpa800, nicht MicroDOS.
    TempPfad daten("k1520_microdos_daten.hfe");
    std::string err;
    auto d = DiskVolume::create(daten.get(), "cpa800", "", formate(), dateisysteme(), err);
    ASSERT_NE(d, nullptr) << err;
    ASSERT_TRUE(lege(*d, "T.TXT", std::vector<uint8_t>(300, 'x')));
    ASSERT_TRUE(d->flush());
    d.reset();
    auto dd = oeffne(daten.get());
    ASSERT_NE(dd, nullptr);
    EXPECT_EQ(dd->detection().filesystem, "cpa800");
}

/// @test boot-get -> create --boot -> boot-get ist bitgleich (Lader und Systemdatei),
///       und put/rm/attr (inkl. ARC) lassen den Lader unangetastet.
TEST(DisktoolMicrodos, BootabbildRundlaufUndSchreibenLaesstLaderUnangetastet) {
    TempPfad pfad("k1520_microdos_rund.hfe");
    std::vector<uint8_t> vorher;
    {
        auto dv = neu(pfad, "microdos800", "boot_microdos_1526.bin");
        ASSERT_NE(dv, nullptr);
        dv->setBackup(false);
        ASSERT_TRUE(dv->readBootImage(vorher));
        EXPECT_EQ(vorher, datei(bootBin("boot_microdos_1526.bin")));

        ASSERT_TRUE(lege(*dv, "A.TXT", std::vector<uint8_t>(5000, 'a'))) << dv->lastError();
        ASSERT_TRUE(lege(*dv, "B.COM", std::vector<uint8_t>(30000, 'b'))) << dv->lastError();
        FileRef a;
        a.name = "A.TXT";
        CpmAttrs at;
        at.set_archived = true; at.archived = true;
        at.set_read_only = true; at.read_only = true;
        ASSERT_TRUE(dv->setAttributes(a, at)) << dv->lastError();
        FileRef weg;
        weg.name = "B.COM";
        ASSERT_TRUE(dv->erase(weg)) << dv->lastError();
        ASSERT_TRUE(dv->flush()) << dv->lastError();
    }
    auto dv = oeffne(pfad.get());
    ASSERT_NE(dv, nullptr);
    EXPECT_EQ(dv->profile().name, "microdos800");
    std::vector<uint8_t> nachher;
    ASSERT_TRUE(dv->readBootImage(nachher));
    EXPECT_EQ(vorher, nachher);
    std::string attr;
    for (const FileEntry& e : dv->list())
        if (e.name == "A.TXT") attr = e.attributes;
    EXPECT_NE(attr.find("ARC"), std::string::npos) << attr;
    EXPECT_NE(attr.find("RO"), std::string::npos) << attr;
    EXPECT_EQ(hole(*dv, "A.TXT").size(), 5120u);
    EXPECT_EQ(0, dv->check(FsCheckLevel::Voll, true).zaehlerAb(FsSeverity::Warnung));
}

/// @test 200K (K5600.20, 1-KB-Bloecke, 64 Plaetze) und 1715 (Bootkopf, MICRODOS.SYS als Datei).
TEST(DisktoolMicrodos, Profil200KUnd1715) {
    {
        TempPfad pfad("k1520_microdos_200k.hfe");
        {
            auto dv = neu(pfad, "microdos200", "boot_microdos_1526_200k.bin");
            ASSERT_NE(dv, nullptr);
            ASSERT_TRUE(lege(*dv, "X.TXT", std::vector<uint8_t>(2000, 'x'))) << dv->lastError();
            ASSERT_TRUE(dv->flush());
        }
        auto dv = oeffne(pfad.get());
        ASSERT_NE(dv, nullptr);
        EXPECT_EQ(dv->detection().filesystem, "microdos200");
        EXPECT_EQ(dv->list().size(), 2u);
        EXPECT_EQ(0, dv->check(FsCheckLevel::Voll, true).zaehlerAb(FsSeverity::Warnung));
    }
    {
        TempPfad pfad("k1520_microdos_1715.hfe");
        {
            auto dv = neu(pfad, "microdos1715", "boot_microdos_1715.bin");
            ASSERT_NE(dv, nullptr);
            EXPECT_EQ(dv->bootAreaSize(), 128u);
            // Ohne MICRODOS.SYS ist es (noch) keine MicroDOS-Diskette, der Kopf allein ist cpa1715.
            ASSERT_TRUE(lege(*dv, "MICRODOS.SYS", std::vector<uint8_t>(14336, 's'))) << dv->lastError();
            ASSERT_TRUE(dv->flush());
        }
        auto dv = oeffne(pfad.get());
        ASSERT_NE(dv, nullptr);
        EXPECT_EQ(dv->detection().filesystem, "microdos1715");
        EXPECT_EQ(0, dv->check(FsCheckLevel::Voll, true).zaehlerAb(FsSeverity::Warnung));
    }
}

/// @test Ein CP/A- oder PC-1715-Lader wird als MicroDOS-Bootabbild abgewiesen, VOR dem Anlegen.
TEST(DisktoolMicrodos, FremdesBootabbildWirdAbgewiesen) {
    std::vector<uint8_t> syl(2048, 0xE5);
    syl[0] = 'S'; syl[1] = 'Y'; syl[2] = 'L';        // A5120-SYL, aber ohne den Text "MicroDOS"
    TempPfad fremd("k1520_microdos_fremd.bin");
    schreibe(fremd.get(), syl);
    TempPfad pfad("k1520_microdos_fremd.hfe");
    std::string err;
    auto dv = DiskVolume::create(pfad.get(), "microdos800", "", formate(), dateisysteme(), err, fremd.get());
    EXPECT_EQ(dv, nullptr);
    EXPECT_NE(err.find("MicroDOS-SYL-Lader"), std::string::npos) << err;
    EXPECT_FALSE(fs::exists(pfad.get()));
}

/// @test Originalabzuege der Sammlung (nur mit K1520_MICRODOS_DIR): Erkennung und Dateizahl.
TEST(DisktoolMicrodos, OriginalabzuegeDerSammlung) {
    const char* dir = std::getenv("K1520_MICRODOS_DIR");
    if (!dir) GTEST_SKIP() << "K1520_MICRODOS_DIR nicht gesetzt";
    struct Z { const char* datei; const char* fs; size_t dateien; };
    const Z soll[] = {
        {"MicroDOS1526s.hfe", "microdos800", 57}, {"MicroDOS1526p.hfe", "microdos800", 57},
        {"MicroDOS1526s_5600.hfe", "microdos200", 13}, {"MicroDOS1526p_5600.hfe", "microdos200", 13},
        {"MicroDOS1715.hfe", "microdos1715", 12}, {"MicroDOS1715_LW_C.hfe", "microdos1715", 13},
    };
    for (const Z& z : soll) {
        const fs::path q = fs::path(dir) / z.datei;
        if (!fs::exists(q)) continue;
        TempPfad kopie("k1520_microdos_orig.hfe");
        fs::copy_file(q, kopie.get(), fs::copy_options::overwrite_existing);
        auto dv = oeffne(kopie.get());
        ASSERT_NE(dv, nullptr) << z.datei;
        EXPECT_EQ(dv->detection().filesystem, z.fs) << z.datei;
        EXPECT_EQ(dv->list().size(), z.dateien) << z.datei;
        EXPECT_EQ(0, dv->check(FsCheckLevel::Voll, true).zaehlerAb(FsSeverity::Warnung)) << z.datei;
    }
}
