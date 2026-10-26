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
    EXPECT_EQ(dv->detection().filesystem, "mutos8k_8x512_mfm_80");
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
    EXPECT_EQ(dv->detection().filesystem, "mutos8k_8x512_mfm");
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
    wiederherstellen("mutos8000_ss80_root.td0", "mutos8k_8x512_mfm_80", 0);
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
    const FsProfile* q = dateisysteme().find("mutos8k_8x512_mfm");
    EXPECT_EQ(DiskVolume::unixStandardInodes(*q, *formate().find(q->format)), 192u);
    // kein UNIX-Typ: 0
    const FsProfile* c = dateisysteme().find("cpa800");
    EXPECT_EQ(DiskVolume::unixStandardInodes(*c, *formate().find(c->format)), 0u);
    std::filesystem::remove_all(dir);
}

// ─── Dateisystemgroesse beim Anlegen (--blocks, s_fsize) ─────────────────────────────────
//
// Auf M8KROOT endet das Dateisystem bei s_fsize 250; dahinter liegt der Auslagerungsbereich (swap)
// des Kernels auf demselben Geraet.  Ein bis Block 494 reichendes Dateisystem wurde dort vom
// Prozessabbild ueberschrieben (Entwurf 39 §4.8).

TEST(MutosFs, Blocks_BegrenztDasDateisystemAufDieAngegebeneGroesse) {
    const auto dir = std::filesystem::temp_directory_path() / "mutos_blocks_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "src");
    std::string err;
    // ganze Diskette: 494 Bloecke, 104 Inodes = 13 Inode-Bloecke -> s_isize 15
    {
        auto dv = DiskVolume::create((dir / "voll.dmk").string(), "mutos8k_26x128_fm", "t", formate(),
                                     dateisysteme(), err, "", 104, 0);
        ASSERT_TRUE(dv) << err;
        EXPECT_EQ(dv->volumeInfo(0).total_bytes, (494u - 15u) * 512u);
    }
    auto dv = DiskVolume::create((dir / "klein.dmk").string(), "mutos8k_26x128_fm", "t", formate(),
                                 dateisysteme(), err, "", 104, 250);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->volumeInfo(0).total_bytes, (250u - 15u) * 512u);          // 235 Datenbloecke

    // 100000 B passen (196 Bloecke samt Indirektbloecken), weitere 30000 B nicht mehr:
    // waeren Bloecke ab 250 frei, ginge das (494 - 15 Bloecke Platz).
    const std::vector<uint8_t> inhalt(100000, 0xA5), mehr(30000, 0x5A);
    auto schreibe = [&](const char* name, const std::vector<uint8_t>& d) {
        std::ofstream(dir / "src" / name, std::ios::binary)
            .write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
    };
    schreibe("a", inhalt);
    ASSERT_TRUE(dv->insertAll((dir / "src").string(), TransferOptions{})) << dv->lastError();
    ASSERT_TRUE(dv->flush()) << dv->lastError();
    const FsCheckReport& r = dv->check(FsCheckLevel::Voll, true);
    EXPECT_EQ(r.alsText().find("Fehler"), std::string::npos) << r.alsText();
    EXPECT_EQ(r.alsText().find("Warnung"), std::string::npos) << r.alsText();
    EXPECT_LT(dv->volumeInfo(0).free_bytes, 40000u);                       // Rest < 79 Bloecke

    std::filesystem::remove(dir / "src" / "a");
    schreibe("b", mehr);
    EXPECT_FALSE(dv->insertAll((dir / "src").string(), TransferOptions{}));
    std::filesystem::remove_all(dir);
}

TEST(MutosFs, Blocks_ZuGrossOderZuKleinIstEinFehler) {
    const auto dir = std::filesystem::temp_directory_path() / "mutos_blocks_fehler";
    std::filesystem::create_directories(dir);
    std::string err;
    // mehr als der Blockraum (494)
    EXPECT_FALSE(DiskVolume::create((dir / "a.dmk").string(), "mutos8k_26x128_fm", "t", formate(),
                                    dateisysteme(), err, "", 104, 495));
    EXPECT_NE(err.find("495"), std::string::npos) << err;
    // nicht groesser als der Inode-Bereich (104 Inodes = s_isize 15; 20 Bloecke waeren 5 Daten, 15 zu klein)
    err.clear();
    EXPECT_FALSE(DiskVolume::create((dir / "b.dmk").string(), "mutos8k_26x128_fm", "t", formate(),
                                    dateisysteme(), err, "", 400, 20));
    EXPECT_FALSE(err.empty());
    std::filesystem::remove_all(dir);
}

// ─── M8K-Satz (MUTOS 8000, 8″ FM 26×128 und 5¼″ 8×512) ───────────────────────────────────
//
// Fixtures aus `~/projects/robotron/A5120/discs/MUTOS8000/M8K*.TD0` (TeleDisk 2011):
//  - CMD, USR : 77 Spuren 26×128 FM.  Der Treiber legt 512-B-Bloecke auf je 4 logische Sektoren
//    mit Verschraenkung 2 und 6 Sektoren Spurversatz (`Mutos8kDev`, Entwurf 39 §4).
//  - BIN      : 8×512 ab Zylinder 1 (gewoehnliches `mutos8k_8x512_mfm`), Zylinder 0 mit Zusatzsektor ID 126.
//  - ROOT     : NICHT lesbar (Nachweis in `doc/merkposten/disktool.md`, MUTOS 8000 M8K-Satz).
// Quelldefekte (fehlende/CRC-Sektoren auf CMD/USR) sind Befund der Quelle, nicht des Werkzeugs.

namespace {

/// Wahr, wenn die Datei ein Z8000-a.out (Magic E807) ist und ihre Groesse zum Kopf passt:
/// 16 + Text + Daten + Symbole.  Die Kopffelder sind unabhaengig vom Dateisystem —
/// eine falsche Blockabbildung bricht diese Gleichung sofort.
bool aoutPasst(const std::vector<uint8_t>& d) {
    if (d.size() < 16 || d[0] != 0xE8 || d[1] != 0x07) return false;
    auto w = [&](size_t i) { return static_cast<size_t>(d[i] << 8 | d[i + 1]); };
    return 16 + w(2) + w(4) + w(8) == d.size();
}

/// Alle Dateien unter bin/ einzeln herausholen; liefert die Namen, die NICHT lesbar waren.
std::vector<std::string> binPruefen(DiskVolume& dv, int& geprueft, const std::string& tag,
                                   const std::string& praefix = "bin/") {
    std::vector<std::string> schlecht;
    const auto tmp = std::filesystem::temp_directory_path() / ("mutos_m8k_aout_" + tag);
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    geprueft = 0;
    for (const FileEntry& e : dv.list()) {
        if (e.name.rfind(praefix, 0) != 0 || e.type == "d") continue;
        const auto ziel = tmp / "datei";
        std::filesystem::remove(ziel);
        if (!dv.extract(FileRef{0, e.name}, ziel.string(), TransferOptions{})) { schlecht.push_back(e.name); continue; }
        const std::string b = k1520test::readFileBytes(ziel.string());
        if (!aoutPasst(std::vector<uint8_t>(b.begin(), b.end()))) { schlecht.push_back(e.name); continue; }
        ++geprueft;
    }
    std::filesystem::remove_all(tmp);
    return schlecht;
}

}  // namespace

TEST(MutosFs, M8k_CmdUndUsrWerdenAlsMutos8kErkannt) {
    for (const char* fx : {"mutos8000_m8k_cmd.td0", "mutos8000_m8k_usr.td0"}) {
        k1520test::TempDisk t(fx, std::string("m8k_erk_") + fx);
        std::string err;
        auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, false);
        ASSERT_TRUE(dv) << fx << ": " << err;
        EXPECT_EQ(dv->detection().filesystem, "mutos8k_26x128_fm") << fx;
        EXPECT_TRUE(dv->istWega());
        const auto l = liste(*dv);
        ASSERT_TRUE(l.count("bin")) << fx;
        EXPECT_EQ(l.at("bin").type, "d");
    }
}

TEST(MutosFs, M8k_CmdLiestBaumUndProgrammeBisAufDieQuelldefekte) {
    k1520test::TempDisk t("mutos8000_m8k_cmd.td0", "m8k_cmd_lesen.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, false);
    ASSERT_TRUE(dv) << err;
    const auto l = liste(*dv);
    EXPECT_TRUE(l.count("etc/mkfs"));
    EXPECT_TRUE(l.count("bin/ls"));
    int ok = 0;
    const auto schlecht = binPruefen(*dv, ok, "cmd");
    // 24 Programme unter bin/; bin/pr liegt auf einem CRC-/Fehlsektor der Quelle (Block 127/142)
    EXPECT_EQ(schlecht, std::vector<std::string>{"bin/pr"});
    EXPECT_GE(ok, 20);
    // fsck findet genau diese Datei und sonst nichts Schlimmeres
    const FsCheckReport& r = dv->check(FsCheckLevel::Voll, true);
    for (const FsFinding& f : r.findings)
        if (f.severity == FsSeverity::Fehler && !f.object.empty()) EXPECT_EQ(f.object, "bin/pr") << f.text;
}

TEST(MutosFs, M8k_UsrLiestAsCcUndDieHeader) {
    k1520test::TempDisk t("mutos8000_m8k_usr.td0", "m8k_usr_lesen.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, false);
    ASSERT_TRUE(dv) << err;
    const auto l = liste(*dv);
    EXPECT_TRUE(l.count("lib/libc.a"));
    EXPECT_EQ(l.at("lib/libc.a").size, 65250u);
    EXPECT_EQ(l.at("include/stdio.h").size, 937u);
    int ok = 0;
    const auto schlecht = binPruefen(*dv, ok, "usr");
    EXPECT_EQ(schlecht, std::vector<std::string>{"bin/ed"});   // Quelldefekt (Indirektblock)
    EXPECT_GE(ok, 3);                                           // as, cc, ld
}

TEST(MutosFs, M8k_SchreibenRundlaufUndUnabhaengigerBefund) {
    k1520test::TempDisk t("mutos8000_m8k_usr.td0", "m8k_usr_schreiben.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, false);
    ASSERT_TRUE(dv) << err;
    // 1500 Byte = 3 Bloecke (USR hat 5 frei), im Wurzelverzeichnis
    std::vector<uint8_t> muster(1500);
    for (size_t i = 0; i < muster.size(); ++i) muster[i] = static_cast<uint8_t>(i * 7 + 3);
    const auto quelle = std::filesystem::path(t.path()).parent_path() / "m8k_quelle";
    std::filesystem::remove_all(quelle);
    std::filesystem::create_directories(quelle);
    { FILE* f = fopen((quelle / "m.bin").string().c_str(), "wb"); ASSERT_TRUE(f);
      fwrite(muster.data(), 1, muster.size(), f); fclose(f); }
    ASSERT_TRUE(dv->insertAll(quelle.string(), TransferOptions{})) << dv->lastError();
    ASSERT_TRUE(dv->flush()) << dv->lastError();
    dv.reset();

    dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, "mutos8k_26x128_fm");
    ASSERT_TRUE(liste(*dv).count("m.bin"));
    const auto ziel = quelle / "aus.bin";
    ASSERT_TRUE(dv->extract(FileRef{0, "m.bin"}, ziel.string(), TransferOptions{})) << dv->lastError();
    const std::string b = k1520test::readFileBytes(ziel.string());
    EXPECT_EQ(std::vector<uint8_t>(b.begin(), b.end()), muster);
    // die Quelldefekte (bin/ed) bleiben die einzigen Fehler, nichts Neues
    for (const FsFinding& f : dv->check(FsCheckLevel::Voll, true).findings)
        if (f.severity == FsSeverity::Fehler && !f.object.empty()) EXPECT_EQ(f.object, "bin/ed") << f.text;
    std::filesystem::remove_all(quelle);
}

TEST(MutosFs, M8k_NeueDisketteRundlaufUndFsize494) {
    const auto dir = std::filesystem::temp_directory_path() / "mutos_m8k_neu";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "q" / "usr");
    std::vector<uint8_t> gross(120000);              // direkt + einfach indirekt
    for (size_t i = 0; i < gross.size(); ++i) gross[i] = static_cast<uint8_t>(i % 253);
    { FILE* f = fopen((dir / "q" / "usr" / "g.bin").string().c_str(), "wb"); ASSERT_TRUE(f);
      fwrite(gross.data(), 1, gross.size(), f); fclose(f); }
    std::string err;
    const std::string neu = (dir / "neu.dmk").string();
    {
        auto dv = DiskVolume::create(neu, "mutos8k_26x128_fm", "m8k", formate(), dateisysteme(), err);
        ASSERT_TRUE(dv) << err;
        ASSERT_TRUE(dv->insertAll((dir / "q").string(), TransferOptions{})) << dv->lastError();
        ASSERT_TRUE(dv->flush()) << dv->lastError();
        EXPECT_EQ(dv->check(FsCheckLevel::Voll, true).alsText().find("Warnung"), std::string::npos);
    }
    auto dv = DiskVolume::open(neu, "", formate(), dateisysteme(), err);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, "mutos8k_26x128_fm");
    const auto ziel = dir / "aus.bin";
    ASSERT_TRUE(dv->extract(FileRef{0, "usr/g.bin"}, ziel.string(), TransferOptions{})) << dv->lastError();
    const std::string b = k1520test::readFileBytes(ziel.string());
    EXPECT_EQ(std::vector<uint8_t>(b.begin(), b.end()), gross);
    std::filesystem::remove_all(dir);
}

/// @test Die 8k-Wurzel in MFM (K5601) ist dieselbe Diskette in anderer Aufzeichnung: eigenes Profil
///       `mutos8k_26x128_mfm` (Format udos_ss77, Rang -10 - gewinnt vor dem UDOS-Profil derselben Spurlage);
///       dieselbe Wurzel in FM bleibt `mutos8k_26x128_fm`, eine UDOS-Diskette auf demselben Format `udos_ss77`
///       bleibt UDOS (Entwurf 39 9.13).
TEST(MutosFs, M8k_MfmUndFmErkennungUndUdosBleibtUdos) {
    const auto dir = std::filesystem::temp_directory_path() / "mutos_m8k_mfm";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "q" / "etc");
    std::vector<uint8_t> inhalt(5000);
    for (size_t i = 0; i < inhalt.size(); ++i) inhalt[i] = static_cast<uint8_t>(i % 251);
    { FILE* f = fopen((dir / "q" / "etc" / "d.bin").string().c_str(), "wb"); ASSERT_TRUE(f);
      fwrite(inhalt.data(), 1, inhalt.size(), f); fclose(f); }
    std::string err;
    struct Fall { const char* profil; const char* format; };
    for (const Fall& fall : {Fall{"mutos8k_26x128_mfm", "udos_ss77"}, Fall{"mutos8k_26x128_fm", "mf3200_26x128"}}) {
        const std::string neu = (dir / (std::string(fall.profil) + ".hfe")).string();
        {
            auto dv = DiskVolume::create(neu, fall.profil, "m8k", formate(), dateisysteme(), err);
            ASSERT_TRUE(dv) << err;
            ASSERT_TRUE(dv->insertAll((dir / "q").string(), TransferOptions{})) << dv->lastError();
            ASSERT_TRUE(dv->flush()) << dv->lastError();
        }
        auto dv = DiskVolume::open(neu, "", formate(), dateisysteme(), err);
        ASSERT_TRUE(dv) << err;
        EXPECT_EQ(dv->detection().filesystem, fall.profil);
        EXPECT_EQ(dv->detection().format, fall.format);
        const auto ziel = dir / (std::string(fall.profil) + ".aus");
        ASSERT_TRUE(dv->extract(FileRef{0, "etc/d.bin"}, ziel.string(), TransferOptions{})) << dv->lastError();
        const std::string b = k1520test::readFileBytes(ziel.string());
        EXPECT_EQ(std::vector<uint8_t>(b.begin(), b.end()), inhalt) << fall.profil;
    }
    // UDOS auf demselben MFM-Format und auf FM (Ladediskette): bleibt UDOS, wird nie als MUTOS-Wurzel gelesen
    for (const Fall& fall : {Fall{"udos_ss77", "udos_ss77"}, Fall{"udos_ss77_fm", "mf3200_26x128"}}) {
        const std::string neu = (dir / (std::string(fall.profil) + ".hfe")).string();
        { auto dv = DiskVolume::create(neu, fall.profil, "", formate(), dateisysteme(), err);
          ASSERT_TRUE(dv) << err; ASSERT_TRUE(dv->flush()) << dv->lastError(); }
        auto dv = DiskVolume::open(neu, "", formate(), dateisysteme(), err);
        ASSERT_TRUE(dv) << err;
        EXPECT_EQ(dv->detection().filesystem, fall.profil);
        EXPECT_EQ(dv->detection().format, fall.format);
    }
    std::filesystem::remove_all(dir);
}

TEST(MutosFs, M8k_BinIstGewoehnlichesMutos77UndSauber) {
    k1520test::TempDisk t("mutos8000_m8k_bin.td0", "m8k_bin.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, false);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, "mutos8k_8x512_mfm");
    // Zylinder 77 (unformatiert) und der CRC-Sektor auf Zylinder 51 liegen HINTER s_fsize = 364
    EXPECT_EQ(dv->check(FsCheckLevel::Voll, true).alsText().find("Fehler"), std::string::npos);
    int ok = 0;
    auto schlecht = binPruefen(*dv, ok, "bin", "");
    std::sort(schlecht.begin(), schlecht.end());
    // Quelldefekte, von fsck nicht zu sehen: die zuletzt geschriebenen Bloecke (ab Block 315 = Zyl. 40) sind
    // unbeschriebene E5-Fuellung - wc und size bestehen nur daraus, passwd ist hinten abgeschnitten.
    EXPECT_EQ(schlecht, (std::vector<std::string>{"passwd", "size", "wc"}));
    EXPECT_EQ(ok, 18);
}

TEST(MutosFs, M8k_RootDisketteWirdNichtFalschErkannt) {
    // ROOT: jeder Sektor traegt in den letzten 48 Byte dieselbe Fremdtabelle und die Wurzel-Inode hat
    // Modus 0x83FE statt 0x41FF.  Kein Dateisystem - und das ist die richtige Antwort.
    k1520test::TempDisk t("mutos8000_m8k_root.td0", "m8k_root.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, false);
    EXPECT_FALSE(dv && dv->hasFileSystem());
}

// ─── 8x512 (DISK-Satz): Gegenprobe am Emulator, Entwurf 39 A5c ───────────────────────────────────────

namespace {
uint32_t fnv1a(const std::string& d) {
    uint32_t h = 0x811C9DC5u;
    for (unsigned char c : d) { h ^= c; h *= 0x01000193u; }
    return h;
}
}  // namespace

TEST(MutosFs, M8x512_VonMutosGeschriebenesLiestDasDiskToolUndFsckIstSauber) {
    // Die Diskette hat das DiskTool angelegt (`create --fs mutos8k_8x512_mfm`, bin/ed, d/od); danach hat MUTOS 8000
    // unter der Ladediskette `k5601_8x512` ZUGESCHRIEBEN: `cp adb` (27 770 B, also mit Indirektblock), `mkdir neu`,
    // `cp tar neu/tar2`, `rm d/od`, `echo hallo > hallo`, `sync`.  Blockabbildung und Dateisystem des DiskTool
    // stimmen genau dann, wenn alles (Inhalt, Verzeichnisse, Freiliste) hier unverletzt ankommt.
    k1520test::TempDisk t("mutos8000_8x512_geschrieben.td0");
    std::string err;
    auto dv = DiskVolume::open(t.path(), "", formate(), dateisysteme(), err, true);
    ASSERT_TRUE(dv) << err;
    EXPECT_EQ(dv->detection().filesystem, "mutos8k_8x512_mfm");
    const auto l = liste(*dv);
    EXPECT_EQ(l.at("adb").size, 27770u);
    EXPECT_EQ(l.at("neu/tar2").size, 17000u);
    EXPECT_EQ(l.at("bin/ed").size, 11472u);
    EXPECT_EQ(l.at("hallo").size, 6u);
    EXPECT_EQ(l.count("d/od"), 0u);                       // von MUTOS geloescht
    const FsCheckReport& r = dv->check(FsCheckLevel::Voll, true);
    EXPECT_EQ(r.alsText().find("Fehler"), std::string::npos) << r.alsText();
    EXPECT_EQ(r.alsText().find("Warnung"), std::string::npos) << r.alsText();

    const auto aus = std::filesystem::path(t.path()).parent_path() / "m8x512_aus";
    std::filesystem::remove_all(aus);
    ASSERT_TRUE(dv->extractAll(aus.string(), TransferOptions{})) << dv->lastError();
    EXPECT_EQ(k1520test::readFileBytes((aus / "hallo").string()), "hallo\n");
    // FNV-1a der Originale (DISK-CMD bin/adb und bin/tar, gelesen aus cmd_8x512)
    EXPECT_EQ(fnv1a(k1520test::readFileBytes((aus / "adb").string())), 0x0278799Au);
    EXPECT_EQ(fnv1a(k1520test::readFileBytes((aus / "neu/tar2").string())), 0xFB2F3506u);
    std::filesystem::remove_all(aus);
}

TEST(MutosFs, M8x512_NeueDisketteNimmtDieBlockgrenzeDesKernelsEin) {
    // Der Kernel lehnt Block >= 608 (76 Zylinder x 8) ab; auch auf einer 80-Zylinder-Diskette bekommt das
    // Dateisystem ohne ausdrueckliches `--blocks` hoechstens 608 Bloecke (sonst lege es Dateien dahin, die MUTOS nie liest).
    const auto dir = std::filesystem::temp_directory_path() / "mutos_8x512_neu_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string err;
    auto a = DiskVolume::create((dir / "a77.hfe").string(), "mutos8k_8x512_mfm", "t", formate(), dateisysteme(), err);
    ASSERT_TRUE(a) << err;
    auto b = DiskVolume::create((dir / "a80.hfe").string(), "mutos8k_8x512_mfm_80", "t", formate(), dateisysteme(), err);
    ASSERT_TRUE(b) << err;
    EXPECT_EQ(a->volumeInfo(0).total_bytes, b->volumeInfo(0).total_bytes);
    EXPECT_LT(b->volumeInfo(0).total_bytes, 608u * 512u);              // 608 Bloecke inklusive Verwaltung
    // ausdruecklich mehr ist erlaubt (zum Beispiel fuer ein Abbild ohne MUTOS)
    auto c = DiskVolume::create((dir / "c80.hfe").string(), "mutos8k_8x512_mfm_80", "t", formate(), dateisysteme(), err, "", 0, 632);
    ASSERT_TRUE(c) << err;
    EXPECT_GT(c->volumeInfo(0).total_bytes, b->volumeInfo(0).total_bytes);
    std::filesystem::remove_all(dir);
}
