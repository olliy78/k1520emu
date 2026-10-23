/**
 * @file test_mutos_fs.cpp
 * @brief MUTOS-8000-Disketten (A5120.16): System-III-Dateisystem wie WEGA, Blockraum ab Zylinder 1.
 *
 * Fixtures (TeleDisk, vom Anwender gelesen): `mutos8000_ss80_root.td0` (Wurzel, 80 Zylinder)
 * und `mutos8000_ss77_cc2.td0` (Compiler-Diskette 2, Zylinder 77 unformatiert).
 * Zylinder 0 ist ein 26×128-Bereich ausserhalb des Blockraums (`data_start` der Profile).
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
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

    const auto quelle = std::filesystem::path(t.path()).parent_path() / (std::string("mutos_quelle_") + "cc2");
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

void roundtrip(const char* fixture, const char* profil, size_t groesse, const char* tag) {
    // eigener Name je Test: ctest startet die Faelle parallel, dieselbe Fixture darf nicht kollidieren
    k1520test::TempDisk t(fixture, std::string(tag) + "_" + fixture);
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, false);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, profil);
    EXPECT_TRUE(dv->istWega());
    EXPECT_EQ(dv->check(FsCheckLevel::Voll, true).alsText().find("Warnung"), std::string::npos);

    std::vector<uint8_t> muster(groesse);
    for (size_t i = 0; i < muster.size(); ++i) muster[i] = static_cast<uint8_t>(i % 251);
    const auto quelle = std::filesystem::path(t.path()).parent_path() / (std::string("mutos1700_quelle_") + tag);
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
    roundtrip("mutos1700_720_uucp.td0", "mutos1700_720", 300000, "a");
}

TEST(MutosFs, Mutos1700_A7100_LadespurLiegtAusserhalbDesBlockraums) {
    k1520test::TempDisk t("mutos1700_a7100_root1gn.td0", "ladespur_root1gn.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, "mutos1700_a7100");
    const auto l = liste(*dv);
    ASSERT_TRUE(l.count("etc/passwd"));
    EXPECT_EQ(l.at("etc/passwd").size, 127u);
    roundtrip("mutos1700_a7100_root1gn.td0", "mutos1700_a7100", 9000, "b");
}

// ─── Vollstaendige Wiederherstellung: auspacken -> neue Diskette -> wieder auspacken ──────
// Geprueft wird das Beiblatt `wega-dateiangaben.txt` (Modus, Besitzer, ALLE drei Zeiten, Links,
// Geraetenummern, Wurzel ".") ohne die Inode-Nummern (die vergibt der Kern neu) und der Inhalt.

namespace {

std::vector<std::string> beiblattOhneInode(const std::filesystem::path& ordner) {
    std::vector<std::string> out;
    std::ifstream f(ordner / "wega-dateiangaben.txt");
    for (std::string z; std::getline(f, z);) {
        if (z.empty() || z[0] == '#') continue;
        const size_t i = z.find(" inode=");
        if (i != std::string::npos) {
            const size_t j = z.find(' ', i + 1);
            z.erase(i, j == std::string::npos ? std::string::npos : j - i);
        }
        out.push_back(z);
    }
    std::sort(out.begin(), out.end());
    return out;
}

void wiederherstellen(const char* fixture, const char* neues_profil, uint32_t inodes) {
    k1520test::TempDisk t(fixture, std::string("rt_") + fixture);
    const auto arbeit = std::filesystem::path(t.path()).parent_path() / (std::string("mutos_rt_") + fixture);
    std::filesystem::remove_all(arbeit);
    std::filesystem::create_directories(arbeit);
    std::string err;
    {
        auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err);
        ASSERT_TRUE(dv) << err;
        ASSERT_TRUE(dv->extractAll((arbeit / "ex").string(), TransferOptions{})) << dv->lastError();
    }
    const std::string neu = (arbeit / "neu.dmk").string();
    {
        auto dv = DiskVolume::create(neu, neues_profil, "rt", formate(), dateisysteme(), err, "", inodes);
        ASSERT_TRUE(dv) << err;
        ASSERT_TRUE(dv->insertAll((arbeit / "ex").string(), TransferOptions{})) << dv->lastError();
        ASSERT_TRUE(dv->flush()) << dv->lastError();
        const FsCheckReport& r = dv->check(FsCheckLevel::Voll, true);
        EXPECT_EQ(r.alsText().find("Warnung"), std::string::npos) << r.alsText();
    }
    {
        auto dv = DiskVolume::open(neu, "", formate(), dateisysteme(), err);
        ASSERT_TRUE(dv) << err;
        ASSERT_TRUE(dv->extractAll((arbeit / "zurueck").string(), TransferOptions{})) << dv->lastError();
    }
    const auto a = beiblattOhneInode(arbeit / "ex");
    const auto b = beiblattOhneInode(arbeit / "zurueck");
    ASSERT_FALSE(a.empty());
    EXPECT_EQ(a, b);
    // Geraetedateien muessen dabei sein (nur im Beiblatt)
    EXPECT_TRUE(std::any_of(a.begin(), a.end(), [](const std::string& z) {
        return z.find(" art=c ") != std::string::npos || z.find(" art=b ") != std::string::npos; }));
    // Inhalt aller Dateien
    for (const auto& e : std::filesystem::recursive_directory_iterator(arbeit / "ex")) {
        if (!e.is_regular_file() || e.path().extension() == ".fileinfo"
            || e.path().filename() == "wega-dateiangaben.txt") continue;
        const auto rel = std::filesystem::relative(e.path(), arbeit / "ex");
        EXPECT_EQ(k1520test::readFileBytes(e.path().string()),
                  k1520test::readFileBytes((arbeit / "zurueck" / rel).string())) << rel;
    }
    std::filesystem::remove_all(arbeit);
}

}  // namespace

TEST(MutosFs, Mutos8000_VollstaendigeWiederherstellung) {
    wiederherstellen("mutos8000_ss80_root.td0", "mutos80", 0);
}

TEST(MutosFs, Mutos1700_A7100_VollstaendigeWiederherstellungMitLinksUndGeraeten) {
    // ROOT1GN traegt Geraetedateien mit nlink 2/3 (harte Links auf Geraete)
    wiederherstellen("mutos1700_a7100_root1gn.td0", "mutos1700_a7100", 0);
}

// ─── Inode-Zahl beim Anlegen ─────────────────────────────────────────────────────────

namespace {

/// Inodes einer frisch angelegten Diskette, aus s_isize = s_fsize - Datenbytes/Block abgeleitet.
uint32_t inodesAngelegt(const std::string& pfad, const char* profil, uint32_t inodes, uint32_t blockgroesse,
                        uint32_t fsize) {
    std::string err;
    auto dv = DiskVolume::create(pfad, profil, "t", formate(), dateisysteme(), err, "", inodes);
    EXPECT_TRUE(dv) << err;
    if (!dv) return 0;
    const uint64_t daten = dv->volumeInfo(0).total_bytes;          // (fsize - isize) * Block
    const uint32_t isize = fsize - static_cast<uint32_t>(daten / blockgroesse);
    return (isize - 2) * (blockgroesse / 64);
}

}  // namespace

TEST(MutosFs, InodeZahl_VorgabeUndAbweichung) {
    const auto dir = std::filesystem::temp_directory_path() / "mutos_inodes_test";
    std::filesystem::create_directories(dir);
    const FsProfile* p = dateisysteme().find("mutos1700_720");
    const DiskFormat* f = formate().find(p->format);
    ASSERT_TRUE(p && f);
    // 720 Bloecke zu 1 KB: 720/25 = 28 Inode-Bloecke * 16 = 448 (UUCP traegt genau s_isize 30)
    EXPECT_EQ(DiskVolume::unixStandardInodes(*p, *f), 448u);
    EXPECT_EQ(inodesAngelegt((dir / "a.dmk").string(), "mutos1700_720", 0, 1024, 720), 448u);
    // abweichend: auf ganze Inode-Bloecke aufgerundet (272 = ROOT1_D)
    EXPECT_EQ(inodesAngelegt((dir / "b.dmk").string(), "mutos1700_720", 272, 1024, 720), 272u);
    EXPECT_EQ(inodesAngelegt((dir / "c.dmk").string(), "mutos1700_720", 100, 1024, 720), 112u);
    // MUTOS 8000: 608 Bloecke zu 512 B, 24 Inode-Bloecke * 8 = 192
    const FsProfile* q = dateisysteme().find("mutos77");
    EXPECT_EQ(DiskVolume::unixStandardInodes(*q, *formate().find(q->format)), 192u);
    // kein UNIX-Typ: 0
    const FsProfile* c = dateisysteme().find("cpa800");
    EXPECT_EQ(DiskVolume::unixStandardInodes(*c, *formate().find(c->format)), 0u);
    std::filesystem::remove_all(dir);
}
