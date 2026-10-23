/**
 * @file test_mutos_fs.cpp
 * @brief MUTOS-8000-Disketten (A5120.16): System-III-Dateisystem wie WEGA, Blockraum ab Zylinder 1.
 *
 * Fixtures (TeleDisk, vom Anwender gelesen): `mutos8000_ss80_root.td0` (Wurzel, 80 Zylinder)
 * und `mutos8000_ss77_cc2.td0` (Compiler-Diskette 2, Zylinder 77 unformatiert).
 * Zylinder 0 ist ein 26×128-Bereich ausserhalb des Blockraums (`data_start` der Profile).
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <string>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "tests/support/fixtures.h"

namespace {

const FormatCatalog& formate() {
    static FormatCatalog c = [] {
        std::string f;
        FormatCatalog k = FormatCatalog::load({K1520_FORMATS_DEFAULT}, &f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

const FsCatalog& dateisysteme() {
    static FsCatalog c = [] {
        std::string f;
        FsCatalog k = FsCatalog::load({K1520_FORMATS_DEFAULT}, formate(), &f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

std::map<std::string, FileEntry> liste(const DiskVolume& dv) {
    std::map<std::string, FileEntry> l;
    for (const FileEntry& e : dv.list()) l[e.name] = e;
    return l;
}

}  // namespace

TEST(MutosFs, WurzeldisketteWirdErkanntUndGelesen) {
    k1520test::TempDisk t("mutos8000_ss80_root.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, "mutos80");
    EXPECT_TRUE(dv->istWega());
    const auto l = liste(*dv);
    ASSERT_TRUE(l.count("etc"));
    EXPECT_EQ(l.at("etc").type, "d");
    EXPECT_TRUE(l.count("dev/console"));
}

TEST(MutosFs, SchreibenUndPruefenAufCompilerDiskette) {
    k1520test::TempDisk t("mutos8000_ss77_cc2.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, false);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, "mutos77");
    EXPECT_EQ(dv->check(FsCheckLevel::Voll, true).alsText().find("Warnung"), std::string::npos);

    const auto quelle = std::filesystem::path(t.path()).parent_path() / "mutos_quelle";
    std::filesystem::create_directories(quelle / "usr");
    { FILE* f = fopen((quelle / "usr" / "h.txt").string().c_str(), "wb"); ASSERT_TRUE(f); fputs("hallo\n", f); fclose(f); }
    ASSERT_TRUE(dv->insertAll(quelle.string(), TransferOptions{})) << dv->lastError();
    ASSERT_TRUE(dv->flush()) << dv->lastError();
    dv.reset();

    dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(liste(*dv).at("usr/h.txt").size, 6u);
    EXPECT_EQ(dv->check(FsCheckLevel::Voll, true).alsText().find("Warnung"), std::string::npos);
    std::filesystem::remove_all(quelle);
}

// ─── MUTOS 1700 (A7100/8086): little endian, PDP-Langworte, 1024-B-Bloecke ──────

namespace {

void roundtrip(const char* fixture, const char* profil, size_t groesse) {
    k1520test::TempDisk t(fixture);
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, false);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, profil);
    EXPECT_TRUE(dv->istWega());
    EXPECT_EQ(dv->check(FsCheckLevel::Voll, true).alsText().find("Warnung"), std::string::npos);

    std::vector<uint8_t> muster(groesse);
    for (size_t i = 0; i < muster.size(); ++i) muster[i] = static_cast<uint8_t>(i % 251);
    const auto quelle = std::filesystem::path(t.path()).parent_path() / "mutos1700_quelle";
    std::filesystem::create_directories(quelle / "tmp");
    {
        FILE* f = fopen((quelle / "tmp" / "g.bin").string().c_str(), "wb");
        ASSERT_TRUE(f);
        fwrite(muster.data(), 1, muster.size(), f);
        fclose(f);
    }
    ASSERT_TRUE(dv->insertAll(quelle.string(), TransferOptions{})) << dv->lastError();
    ASSERT_TRUE(dv->flush()) << dv->lastError();
    dv.reset();

    dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(liste(*dv).at("tmp/g.bin").size, groesse);
    EXPECT_EQ(dv->check(FsCheckLevel::Voll, true).alsText().find("Warnung"), std::string::npos);
    const auto ziel = quelle / "aus";
    ASSERT_TRUE(dv->extractAll(ziel.string(), TransferOptions{})) << dv->lastError();
    const std::string z = k1520test::readFileBytes((ziel / "tmp" / "g.bin").string());
    ASSERT_EQ(z.size(), muster.size());
    EXPECT_EQ(0, memcmp(z.data(), muster.data(), muster.size()));
    dv.reset();
    std::filesystem::remove_all(quelle);
}

}  // namespace

TEST(MutosFs, Mutos1700_720_LiestSchreibtUndPruefen) {
    // 300 000 Byte: direkt + einfach indirekt (256 Adressen je Block zu 1024 B)
    roundtrip("mutos1700_720_uucp.td0", "mutos1700_720", 300000);
}

TEST(MutosFs, Mutos1700_A7100_LadespurLiegtAusserhalbDesBlockraums) {
    k1520test::TempDisk t("mutos1700_a7100_root1gn.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, "mutos1700_a7100");
    const auto l = liste(*dv);
    ASSERT_TRUE(l.count("etc/passwd"));
    EXPECT_EQ(l.at("etc/passwd").size, 127u);
    roundtrip("mutos1700_a7100_root1gn.td0", "mutos1700_a7100", 9000);
}
