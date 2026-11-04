/**
 * @file test_disktool_microdos.cpp
 * @brief DiskTool fuer MicroDOS 1526 / 1715 (Rostock, CP/M-2.2-Anpassung), doc/design/13_k1520disktool.md §16.
 *
 * Eingechecktes Material sind nur die Bootabbilder `data/bootsektoren/boot_microdos_*.bin`; die
 * Disketten entstehen hier per `create` (Fixture "MicroDOS-Leerdiskette").  Die Originalabzuege
 * (Sammlung, nicht im Repo) werden zusaetzlich geprueft, wenn `K1520_MICRODOS_DIR` auf ihren
 * Ordner zeigt (sonst uebersprungen).
 *
 *  - Erkennung: SYL-Lader (acht Plaetze) = positiver Nachweis; Datendiskette bleibt cpa_ds80_5x1024_daten.
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

/// @test Leere MicroDOS-Systemdiskette: Lader + MicroDOS.SYS, wird als microdos_ds80_5x1024_boot erkannt,
///       prueft ohne Befund; ohne Lader bleibt dieselbe Geometrie cpa_ds80_5x1024_daten.
TEST(DisktoolMicrodos, ErzeugteSystemdisketteWirdErkanntUndPrueftSauber) {
    TempPfad pfad("k1520_microdos_neu.hfe");
    {
        auto dv = neu(pfad, "microdos_ds80_5x1024_boot", "boot_microdos_1526.bin");
        ASSERT_NE(dv, nullptr);
        EXPECT_EQ(dv->bootAreaSize(), 256u + 16384u);
        ASSERT_TRUE(dv->flush()) << dv->lastError();
    }
    auto dv = oeffne(pfad.get());
    ASSERT_NE(dv, nullptr);
    EXPECT_EQ(dv->detection().filesystem, "microdos_ds80_5x1024_boot");
    EXPECT_TRUE(dv->detection().unambiguous);
    const auto liste = dv->list();
    ASSERT_EQ(liste.size(), 1u);                       // nur MicroDOS.SYS, der Lader ist keine Datei
    EXPECT_EQ(liste[0].name, "MicroDOS.SYS");
    EXPECT_EQ(liste[0].size, 16384u);
    for (FsCheckLevel stufe : {FsCheckLevel::Schnell, FsCheckLevel::Voll}) {
        const FsCheckReport& r = dv->check(stufe, true);
        EXPECT_EQ(0, r.zaehlerAb(FsSeverity::Warnung)) << kennungen(r);
    }

    // Dieselbe Geometrie ohne Lader ist eine Datendiskette: cpa_ds80_5x1024_daten, nicht MicroDOS.
    TempPfad daten("k1520_microdos_daten.hfe");
    std::string err;
    auto d = DiskVolume::create(daten.get(), "cpa_ds80_5x1024_daten", "", formate(), dateisysteme(), err);
    ASSERT_NE(d, nullptr) << err;
    ASSERT_TRUE(lege(*d, "T.TXT", std::vector<uint8_t>(300, 'x')));
    ASSERT_TRUE(d->flush());
    d.reset();
    auto dd = oeffne(daten.get());
    ASSERT_NE(dd, nullptr);
    EXPECT_EQ(dd->detection().filesystem, "cpa_ds80_5x1024_daten");
}

/// @test boot-get -> create --boot -> boot-get ist bitgleich (Lader und Systemdatei),
///       und put/rm/attr (inkl. ARC) lassen den Lader unangetastet.
TEST(DisktoolMicrodos, BootabbildRundlaufUndSchreibenLaesstLaderUnangetastet) {
    TempPfad pfad("k1520_microdos_rund.hfe");
    std::vector<uint8_t> vorher;
    {
        auto dv = neu(pfad, "microdos_ds80_5x1024_boot", "boot_microdos_1526.bin");
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
    EXPECT_EQ(dv->profile().name, "microdos_ds80_5x1024_boot");
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

/// @test 128-Byte-Systemdiskette (Lade-ROM V0.6): create --boot, Erkennung, Pruefung, put ueber den Systembereich
///       hinweg (Block 18 bleibt Nullen), boot-get ist bitgleich; Lader der anderen Sektorlaenge werden abgewiesen.
TEST(DisktoolMicrodos, Systemdiskette128BootabbildRundlaufUndFremdeLader) {
    TempPfad pfad("k1520_microdos_128.hfe");
    const std::vector<uint8_t> bild = datei(bootBin("boot_microdos_1526_128.bin"));
    ASSERT_EQ(bild.size(), 16640u);
    {
        auto dv = neu(pfad, "microdos_ss40_26x128_boot", "boot_microdos_1526_128.bin");
        ASSERT_NE(dv, nullptr);
        dv->setBackup(false);
        EXPECT_EQ(dv->bootAreaSize(), 256u + 16384u);
        std::vector<uint8_t> v;
        ASSERT_TRUE(dv->readBootImage(v));
        EXPECT_EQ(v, bild);
        ASSERT_TRUE(lege(*dv, "A.TXT", std::vector<uint8_t>(5000, 'a'))) << dv->lastError();
        ASSERT_TRUE(dv->flush()) << dv->lastError();
    }
    auto dv = oeffne(pfad.get());
    ASSERT_NE(dv, nullptr);
    EXPECT_EQ(dv->detection().filesystem, "microdos_ss40_26x128_boot");
    for (FsCheckLevel stufe : {FsCheckLevel::Schnell, FsCheckLevel::Voll}) {
        const FsCheckReport& r = dv->check(stufe, true);
        EXPECT_EQ(0, r.zaehlerAb(FsSeverity::Warnung)) << kennungen(r);
    }
    EXPECT_EQ(dv->list().size(), 2u);
    EXPECT_EQ(hole(*dv, "A.TXT").size(), 5120u);
    std::vector<uint8_t> nachher;
    ASSERT_TRUE(dv->readBootImage(nachher));
    EXPECT_EQ(nachher, bild);                       // boot-get -> create -> boot-get bitgleich

    // Die Datei darf die Systembloecke nicht erreichen: A.TXT liegt ab Block 20.
    // Fremde Lader: 1024-B-Lader auf dem 128-Profil und umgekehrt, CP/A-Lader.
    for (const auto& z : {std::make_pair("microdos_ss40_26x128_boot", "boot_microdos_1526.bin"),
                          std::make_pair("microdos_ds80_5x1024_boot", "boot_microdos_1526_128.bin"),
                          std::make_pair("microdos_ss40_5x1024_boot", "boot_microdos_1526_128.bin")}) {
        TempPfad p2("k1520_microdos_fremd.hfe");
        std::string err;
        auto d = DiskVolume::create(p2.get(), z.first, "", formate(), dateisysteme(), err, bootBin(z.second));
        EXPECT_EQ(d, nullptr) << z.first << " + " << z.second;
        EXPECT_NE(err.find("Lader"), std::string::npos) << err;
    }
}

/// @test 200K (K5600.10, 1-KB-Bloecke, 64 Plaetze) und 1715 (Bootkopf, MICRODOS.SYS als Datei).
TEST(DisktoolMicrodos, Profil200KUnd1715) {
    {
        TempPfad pfad("k1520_microdos_200k.hfe");
        {
            auto dv = neu(pfad, "microdos_ss40_5x1024_boot", "boot_microdos_1526_200k.bin");
            ASSERT_NE(dv, nullptr);
            ASSERT_TRUE(lege(*dv, "X.TXT", std::vector<uint8_t>(2000, 'x'))) << dv->lastError();
            ASSERT_TRUE(dv->flush());
        }
        auto dv = oeffne(pfad.get());
        ASSERT_NE(dv, nullptr);
        EXPECT_EQ(dv->detection().filesystem, "microdos_ss40_5x1024_boot");
        EXPECT_EQ(dv->list().size(), 2u);
        EXPECT_EQ(0, dv->check(FsCheckLevel::Voll, true).zaehlerAb(FsSeverity::Warnung));
    }
    {
        // 1715: Bootabbild = 128-B-Kopf + MICRODOS.SYS (erste Datei); mit dem vollen Bootabbild
        // entsteht sofort eine erkannte, saubere Systemdiskette, boot-get gibt es bitgleich zurueck.
        TempPfad pfad("k1520_microdos_1715.hfe");
        std::vector<uint8_t> vorher;
        {
            auto dv = neu(pfad, "microdos1715_ds80_5x1024_boot", "boot_microdos_1715.bin");
            ASSERT_NE(dv, nullptr);
            EXPECT_EQ(dv->bootAreaSize(), 128u + 16384u);
            ASSERT_TRUE(dv->readBootImage(vorher));
            EXPECT_EQ(vorher, datei(bootBin("boot_microdos_1715.bin")));
            ASSERT_TRUE(lege(*dv, "A.TXT", std::vector<uint8_t>(5000, 'a'))) << dv->lastError();
            ASSERT_TRUE(dv->flush());
        }
        auto dv = oeffne(pfad.get());
        ASSERT_NE(dv, nullptr);
        EXPECT_EQ(dv->detection().filesystem, "microdos1715_ds80_5x1024_boot");
        EXPECT_EQ(dv->list().size(), 2u);
        std::vector<uint8_t> nachher;
        ASSERT_TRUE(dv->readBootImage(nachher));
        EXPECT_EQ(vorher, nachher);
        EXPECT_EQ(0, dv->check(FsCheckLevel::Voll, true).zaehlerAb(FsSeverity::Warnung));
        // MICRODOS.SYS liegt in den ersten Datenbloecken (dort liest sie der Urlader)
        EXPECT_EQ(hole(*dv, "MICRODOS.SYS").size(), 14336u);
    }
    {
        // Nur der Kopf (128 B): ohne MICRODOS.SYS noch keine MicroDOS-Diskette; die Datei danach
        // als ERSTE Datei einfuegen genuegt (der Kopf allein ist cpa1715_ds80_5x1024_boot).
        TempPfad pfad("k1520_microdos_1715k.hfe");
        std::vector<uint8_t> kopf = datei(bootBin("boot_microdos_1715.bin"));
        kopf.resize(128);
        TempPfad kopfDatei("k1520_microdos_1715_kopf.bin");
        {
            std::ofstream o(kopfDatei.get(), std::ios::binary);
            o.write(reinterpret_cast<const char*>(kopf.data()), static_cast<std::streamsize>(kopf.size()));
        }
        std::string err;
        {
            auto dv = DiskVolume::create(pfad.get(), "microdos1715_ds80_5x1024_boot", "", formate(), dateisysteme(), err,
                                         kopfDatei.get());
            ASSERT_NE(dv, nullptr) << err;
            EXPECT_EQ(dv->bootAreaSize(), 128u + 16384u);
            ASSERT_TRUE(lege(*dv, "MICRODOS.SYS", std::vector<uint8_t>(14336, 's'))) << dv->lastError();
            ASSERT_TRUE(dv->flush());
        }
        auto dv = oeffne(pfad.get());
        ASSERT_NE(dv, nullptr);
        EXPECT_EQ(dv->detection().filesystem, "microdos1715_ds80_5x1024_boot");
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
    auto dv = DiskVolume::create(pfad.get(), "microdos_ds80_5x1024_boot", "", formate(), dateisysteme(), err, fremd.get());
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
        {"MicroDOS1526s.hfe", "microdos_ds80_5x1024_boot", 57}, {"MicroDOS1526p.hfe", "microdos_ds80_5x1024_boot", 57},
        {"MicroDOS1526s_5600.hfe", "microdos_ss40_5x1024_boot", 13}, {"MicroDOS1526p_5600.hfe", "microdos_ss40_5x1024_boot", 13},
        {"MicroDOS1715.hfe", "microdos1715_ds80_5x1024_boot", 12}, {"MicroDOS1715_LW_C.hfe", "microdos1715_ds80_5x1024_boot", 13},
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

// ─── Rettung (recover) und Pruefung des Systemortes ─────────────────────────────────────────────

namespace {

std::vector<uint8_t> muster(size_t n, uint8_t salz) {
    std::vector<uint8_t> d(n);
    for (size_t i = 0; i < n; ++i) d[i] = static_cast<uint8_t>((i * 7 + salz) & 0xFF);
    return d;
}

/// Bootabbild + Dateisystem je Profil; die Rettungsprobe laeuft fuer alle drei.
struct Profil { const char* fs; const char* bin; };
const Profil kProfile[] = {{"microdos_ds80_5x1024_boot", "boot_microdos_1526.bin"},
                           {"microdos_ss40_5x1024_boot", "boot_microdos_1526_200k.bin"},
                           {"microdos1715_ds80_5x1024_boot", "boot_microdos_1715.bin"}};

}  // namespace

/// @test Datei loeschen -> recover findet sie unter ihrem Namen, Inhalt bitgleich -> --restore traegt sie
///       wieder ein; Lader und MicroDOS.SYS bleiben dabei byteweise unberuehrt, fsck bleibt sauber.
///       (Suchtiefe Verzeichnis UND Oberflaeche: letztere darf Lader/System nie als Fund anbieten.)
TEST(DisktoolMicrodos, RettungFindetGeloeschteDateienUndLaesstSystemUnberuehrt) {
    for (const Profil& p : kProfile) {
        SCOPED_TRACE(p.fs);
        TempPfad pfad("k1520_microdos_rettung.hfe");
        std::vector<uint8_t> boot_vorher;
        const std::vector<uint8_t> inhalt_a = muster(3000, 1), inhalt_b = muster(1500, 2);
        {
            auto dv = neu(pfad, p.fs, p.bin);
            ASSERT_NE(dv, nullptr);
            dv->setBackup(false);
            ASSERT_TRUE(lege(*dv, "A.TXT", inhalt_a)) << dv->lastError();
            ASSERT_TRUE(lege(*dv, "B.TXT", inhalt_b)) << dv->lastError();
            ASSERT_TRUE(dv->readBootImage(boot_vorher));
            FileRef a;
            a.name = "A.TXT";
            ASSERT_TRUE(dv->erase(a)) << dv->lastError();
            ASSERT_TRUE(dv->flush()) << dv->lastError();
        }
        auto dv = oeffne(pfad.get(), true);
        ASSERT_NE(dv, nullptr);
        dv->setBackup(false);
        for (FsRecoverLevel stufe : {FsRecoverLevel::Verzeichnis, FsRecoverLevel::Oberflaeche}) {
            const FsRecoverReport& r = dv->recoverScan(stufe, true);
            int treffer = 0;
            for (const FsRecoverFind& f : r.funde) {
                // Lader/MicroDOS.SYS sind belegt -- sie duerfen in keinem Fund auftauchen
                EXPECT_NE(f.name, "MicroDOS.SYS");
                EXPECT_NE(f.name, "MICRODOS.SYS");
                if (f.name == "A.TXT") ++treffer;
            }
            EXPECT_EQ(treffer, 1);
        }
        const FsRecoverReport& r = dv->recoverScan(FsRecoverLevel::Verzeichnis, true);
        int fund = -1;
        for (size_t i = 0; i < r.funde.size(); ++i)
            if (r.funde[i].name == "A.TXT") fund = static_cast<int>(i);
        ASSERT_GE(fund, 0);
        EXPECT_EQ(r.funde[fund].quality, FsRecoverQuality::Sicher);
        EXPECT_TRUE(r.funde[fund].wiederherstellbar) << r.funde[fund].warum_nicht;
        std::vector<uint8_t> gelesen;
        ASSERT_TRUE(dv->recoverRead(fund, gelesen));
        gelesen.resize(std::min<size_t>(gelesen.size(), inhalt_a.size()));
        EXPECT_EQ(gelesen, inhalt_a);

        ASSERT_TRUE(dv->recoverRestore(fund, "")) << dv->lastError();
        ASSERT_TRUE(dv->flush()) << dv->lastError();
        auto n = oeffne(pfad.get());
        ASSERT_NE(n, nullptr);
        EXPECT_EQ(n->detection().filesystem, p.fs);
        std::vector<uint8_t> a_neu = hole(*n, "A.TXT");
        a_neu.resize(std::min<size_t>(a_neu.size(), inhalt_a.size()));
        EXPECT_EQ(a_neu, inhalt_a);
        EXPECT_GE(hole(*n, "B.TXT").size(), inhalt_b.size());
        std::vector<uint8_t> boot_nachher;
        ASSERT_TRUE(n->readBootImage(boot_nachher));
        EXPECT_EQ(boot_vorher, boot_nachher);
        const FsCheckReport& c = n->check(FsCheckLevel::Voll, true);
        EXPECT_EQ(0, c.zaehlerAb(FsSeverity::Warnung)) << kennungen(c);
    }
}

/// @test `cpm.dir.geloescht` meldet nur Reste, aus denen `recover` etwas holen kann: ein geloeschter
///       LEERER Eintrag (0 Saetze, Zeiger 0 -- so auf MicroDOS1526s) ist keine Wiederherstellung wert.
TEST(DisktoolMicrodos, GeloeschtHinweisNurBeiWiederherstellbaremRest) {
    TempPfad pfad("k1520_microdos_leerrest.hfe");
    {
        auto dv = neu(pfad, "microdos_ds80_5x1024_boot", "boot_microdos_1526.bin");
        ASSERT_NE(dv, nullptr);
        dv->setBackup(false);
        // CP/M legt keine leere Datei an; der Rest entsteht also von Hand wie auf MicroDOS1526s
        // ('WSNEW.COM', geloescht, Nutzerbyte E5, 0 Saetze, alle Zeiger 0) in Platz 9.
        std::vector<uint8_t> sek;
        uint16_t crc = 0;
        ASSERT_TRUE(dv->readSectorAt(0, 0, 0, sek, crc));
        const size_t o = 9 * 32;
        ASSERT_EQ(sek[o], 0xE5);
        const char name[] = "WSNEW   COM";
        sek[o] = 0xE5;
        for (int i = 0; i < 11; ++i) sek[o + 1 + i] = static_cast<uint8_t>(name[i]);
        for (int i = 12; i < 32; ++i) sek[o + i] = 0;
        ASSERT_TRUE(dv->writeSectorAt(0, 0, 0, sek, nullptr));
        ASSERT_TRUE(dv->flush());
    }
    {
        auto dv = oeffne(pfad.get());
        ASSERT_NE(dv, nullptr);
        EXPECT_EQ(kennungen(dv->check(FsCheckLevel::Voll, true)).find("cpm.dir.geloescht"), std::string::npos);
        EXPECT_TRUE(dv->recoverScan(FsRecoverLevel::Verzeichnis, true).funde.empty());
    }
    {
        auto dv = oeffne(pfad.get(), true);
        dv->setBackup(false);
        ASSERT_TRUE(lege(*dv, "X.TXT", muster(2500, 3)));
        FileRef f;
        f.name = "X.TXT";
        ASSERT_TRUE(dv->erase(f));
        ASSERT_TRUE(dv->flush());
    }
    auto dv = oeffne(pfad.get());
    EXPECT_NE(kennungen(dv->check(FsCheckLevel::Voll, true)).find("cpm.dir.geloescht"), std::string::npos);
    EXPECT_FALSE(dv->recoverScan(FsRecoverLevel::Verzeichnis, true).funde.empty());
}

/// @test `cpm.system.ort`: MicroDOS.SYS (1526) bzw. MICRODOS.SYS (1715) nicht am Ort des Urladers ->
///       Warnung; auf gesunden Systemdisketten, Datendisketten und nach put/rm anderer Dateien nie.
TEST(DisktoolMicrodos, PruefungMeldetSystemdateiAmFalschenOrt) {
    // 1715: nach einer anderen Datei eingefuegt liegt MICRODOS.SYS nicht vorne
    {
        TempPfad pfad("k1520_microdos_ort1715.hfe"), kopf("k1520_microdos_ort1715.bin");
        std::vector<uint8_t> k = datei(bootBin("boot_microdos_1715.bin"));
        k.resize(128);
        schreibe(kopf.get(), k);
        {
            std::string err;
            auto dv = DiskVolume::create(pfad.get(), "microdos1715_ds80_5x1024_boot", "", formate(), dateisysteme(), err, kopf.get());
            ASSERT_NE(dv, nullptr) << err;
            dv->setBackup(false);
            ASSERT_TRUE(lege(*dv, "X.COM", muster(3000, 4))) << dv->lastError();
            ASSERT_TRUE(lege(*dv, "MICRODOS.SYS", muster(14336, 5))) << dv->lastError();
            ASSERT_TRUE(dv->flush());
        }
        auto dv = oeffne(pfad.get());
        ASSERT_EQ(dv->detection().filesystem, "microdos1715_ds80_5x1024_boot");
        const std::string k1 = kennungen(dv->check(FsCheckLevel::Voll, true));
        EXPECT_NE(k1.find("cpm.system.ort"), std::string::npos) << k1;
    }
    // 1526: Zeiger der Systemdatei verbogen / Eintrag geloescht (Lader bleibt)
    for (int fall = 0; fall < 2; ++fall) {
        TempPfad pfad("k1520_microdos_ort1526.hfe");
        {
            auto dv = neu(pfad, "microdos_ds80_5x1024_boot", "boot_microdos_1526.bin");
            ASSERT_NE(dv, nullptr);
            dv->setBackup(false);
            std::vector<uint8_t> sek;
            uint16_t crc = 0;
            ASSERT_TRUE(dv->readSectorAt(0, 0, 0, sek, crc));
            const size_t o = 8 * 32;                    // Platz 8 = MicroDOS.SYS (0-7 gehoeren dem Lader)
            ASSERT_EQ(sek[o + 1], 'M');
            if (fall == 0) sek[o + 16] = static_cast<uint8_t>(sek[o + 16] + 2);   // erster Zeiger 5 -> 7
            else           sek[o] = 0xE5;
            ASSERT_TRUE(dv->writeSectorAt(0, 0, 0, sek, nullptr));
            ASSERT_TRUE(dv->flush());
        }
        auto dv = oeffne(pfad.get());
        const std::string k1 = kennungen(dv->check(FsCheckLevel::Voll, true));
        EXPECT_NE(k1.find(fall == 0 ? "cpm.system.ort" : "cpm.system.fehlt"), std::string::npos) << k1;
    }
    // gesunde Systemdisketten und eine Datendiskette: nie
    for (const Profil& p : kProfile) {
        TempPfad pfad("k1520_microdos_ortok.hfe");
        auto dv = neu(pfad, p.fs, p.bin);
        ASSERT_NE(dv, nullptr);
        dv->setBackup(false);
        ASSERT_TRUE(lege(*dv, "A.TXT", muster(4000, 6)));
        ASSERT_TRUE(lege(*dv, "B.TXT", muster(900, 7)));
        FileRef f;
        f.name = "A.TXT";
        ASSERT_TRUE(dv->erase(f));
        ASSERT_TRUE(dv->flush());
        const std::string k1 = kennungen(dv->check(FsCheckLevel::Voll, true));
        EXPECT_EQ(k1.find("cpm.system"), std::string::npos) << p.fs << ": " << k1;
    }
    TempPfad daten("k1520_microdos_ortdaten.hfe");
    std::string err;
    auto dd = DiskVolume::create(daten.get(), "microdos_ds80_5x1024_daten", "", formate(), dateisysteme(), err);
    ASSERT_NE(dd, nullptr) << err;
    ASSERT_TRUE(lege(*dd, "T.TXT", muster(300, 8)));
    EXPECT_EQ(kennungen(dd->check(FsCheckLevel::Voll, true)).find("cpm.system"), std::string::npos);
}
