/**
 * @file test_sios_fs.cpp
 * @brief GoogleTests fuer @ref SiosFileSystem — SIOS 1526 (A5120/A5130/K8924), Lesen und Schreiben (D4).
 *
 * ### Woher die Sollwerte stammen
 * Alle Werte sind an den fuenf Originalabbildern gemessen und stehen in
 * `doc/sios_diskettenformat.md` (Herkunft der Abbilder: `tools/sios_a5120/original/README.md`,
 * Kopien unter `tests/fixtures/disks/sios1526_*.hfe`):
 *
 * | Sollwert | Quelle |
 * |---|---|
 * | VOL1 `ROBINI` / `MWD051` / `MWD002`, HDR1 `SYSARE11` BOE 01001 EOE 12026 EOD 12013 | §3.3, §4.1 |
 * | `PHASELB` DP1: 15 Objekte, AADR 0013 0037 0083 00A5 00FB ..., ND 03B3 | §7.2 |
 * | SG1 `SYSORG09` Satzlaenge 1024, `EC01`; `PHASELB` SGEN/SOPH/SYSD 0013/0049/0063 | §4.1, §7.3 |
 *
 * Wichtigste Wachter: @ref SiosFs.ObjektBytesSindDerAusschnittDerBibliothek (Verzeichnis und
 * Dateiinhalt stimmen ueberein) und @ref SiosFs.NurMitNachweisErkannt (kein CP/A-, UDOS- oder
 * BCU-Abbild wird SIOS).
 *
 * @see core/filesystem/sios/sios_fs.h
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/filesystem/sios/sios_fs.h"
#include "core/peripherals/floppy_drive/disk_image.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;

namespace {

std::string fixture(const char* name) { return (fs::path(FIXTURE_DIR) / name).string(); }

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

std::unique_ptr<DiskVolume> oeffne(const char* name, std::string& err, const std::string& fsname = "") {
    return DiskVolume::open(fixture(name), fsname, formate(), dateisysteme(), err);
}

/// @brief Die SIOS-Schicht allein (ohne DiskVolume) auf einem Abbild.
struct Diskette {
    std::unique_ptr<DiskImage>       disk;
    std::unique_ptr<SectorSpace>     space;
    std::unique_ptr<SiosFileSystem>  fs;
    std::string                      error;
};

Diskette direkt(const std::string& pfad, const char* fsname, bool nur_lesen = true) {
    Diskette d;
    const FsProfile* p = dateisysteme().find(fsname);
    const DiskFormat* f = p ? formate().find(p->format) : nullptr;
    if (!f) { d.error = "Katalog unvollstaendig"; return d; }
    d.disk = DiskImage::open(pfad, std::nullopt, nur_lesen);
    if (!d.disk) { d.error = "Abbild nicht ladbar"; return d; }
    d.space = std::make_unique<SectorSpace>(d.disk->medium(), *f);
    d.fs    = SiosFileSystem::mount(*d.space, *p, d.error);
    return d;
}

const FileEntry* finde(const std::vector<FileEntry>& l, const std::string& name) {
    for (const FileEntry& e : l) if (e.name == name) return &e;
    return nullptr;
}

}  // namespace

// ─── Erkennung ───────────────────────────────────────────────────────────────

TEST(SiosFs, ErkennungJedesAbbild) {
    struct Soll { const char* datei; const char* profil; const char* format; const char* label; };
    const Soll soll[] = {
        {"sios1526_dp1.hfe",  "sios_ss40", "k5601_ss40_26x128", "MWD051"},
        {"sios1526_dp2.hfe",  "sios_ss40", "k5601_ss40_26x128", "MWD051"},
        {"sios1526_sg1.hfe",  "sios_ss40", "k5601_ss40_26x128", "MWD002"},
        {"sios1526_syls.hfe", "sios_ss80", "k5601_ss80_26x128", "ROBINI"},
    };
    for (const Soll& s : soll) {
        std::string err;
        auto v = oeffne(s.datei, err);
        ASSERT_TRUE(v) << s.datei << ": " << err;
        EXPECT_EQ(v->detection().filesystem, s.profil) << s.datei;
        EXPECT_EQ(v->detection().format, s.format) << s.datei;
        EXPECT_TRUE(v->detection().unambiguous) << s.datei;
        EXPECT_EQ(v->volumeCount(), 1) << s.datei;
        EXPECT_EQ(v->volumeInfo(0).label, s.label) << s.datei;
        EXPECT_EQ(v->fsType(), FsType::Sios) << s.datei;
    }
}

TEST(SiosFs, SylpIstNurMitFsErkennbar) {
    // SYLp ist ein 40-Spur-Abbild aus einem 80er-Laufwerk (Zylinder 40-68 unformatiert, ein
    // Streusektor c15h1): die Geometrie passt zu keinem Katalogformat, die Erkennung ohne
    // Vorgabe liest es deshalb nicht.  Mit --fs sios_ss40 geht es (nur Seite 0, Spuren 0-39).
    std::string err;
    EXPECT_FALSE(oeffne("sios1526_sylp.hfe", err)) << "Befund geaendert? Dann Doku nachziehen";
    auto v = oeffne("sios1526_sylp.hfe", err, "sios_ss40");
    ASSERT_TRUE(v) << err;
    EXPECT_EQ(v->volumeInfo(0).label, "ROBINI");
    const auto l = v->list();
    ASSERT_EQ(l.size(), 1u);
    EXPECT_EQ(l[0].name, "SYSARE09");
}

TEST(SiosFs, NurMitNachweisErkannt) {
    // 26x128-Abbilder anderer Systeme duerfen nie SIOS werden.
    for (const char* datei : {"bcu12_ss40-26x128_k5600-10_ohnesyl-sys1.img",
                              "bcu12_ss40-26x128_k5600-10_syl-spur1-satz1.img",
                              "cpa_cpa780_k5601_noclock.hfe", "udos_boot_scp.hfe",
                              "cpa_combo5zoll_noclock_k5600_10_boot.hfe"}) {
        std::string err;
        auto v = oeffne(datei, err);
        ASSERT_TRUE(v) << datei << ": " << err;
        EXPECT_NE(v->fsType(), FsType::Sios) << datei;
        EXPECT_EQ(v->detection().filesystem.rfind("sios", 0), std::string::npos) << datei;
    }
    // MUTOS-8000-Disketten sind ebenfalls SIOS-initialisiert (VOL1 `MUTOS`, UNIX-Dateisystem ab
    // Zylinder 1): ihr eigener Nachweis (Superblock) geht vor.  Liegen in disks/, nicht in den Fixtures.
    {
        const fs::path mutos = fs::path(FIXTURE_DIR) / ".." / ".." / ".." / "disks" / "a5120_16_MUTOS"
                             / "a5120_mutos8000_usr_mfm.hfe";
        if (fs::exists(mutos)) {
            std::string err;
            auto v = DiskVolume::open(mutos.string(), "", formate(), dateisysteme(), err);
            ASSERT_TRUE(v) << err;
            EXPECT_EQ(v->fsType(), FsType::WegaMutos);
            EXPECT_GT(v->list().size(), 5u);
        }
    }
    // ... und erzwungen scheitert das Mounten ohne ERMAP/VOL1 mit einem Grund.
    std::string err;
    EXPECT_FALSE(oeffne("bcu12_ss40-26x128_k5600-10_ohnesyl-sys1.img", err, "sios_ss40"));
    EXPECT_NE(err.find("SIOS"), std::string::npos) << err;
}

// ─── Etiketten ───────────────────────────────────────────────────────────────

TEST(SiosFs, EtikettenSystemdiskette) {
    auto d = direkt(fixture("sios1526_syls.hfe"), "sios_ss80");
    ASSERT_TRUE(d.fs) << d.error;
    EXPECT_EQ(d.fs->volumeName(), "ROBINI");
    EXPECT_EQ(d.fs->etikettenText(7).substr(0, 10), "VOL1ROBINI");
    EXPECT_EQ(d.fs->etikettenText(5).substr(0, 5), "ERMAP");
    const auto et = d.fs->etiketten();
    ASSERT_EQ(et.size(), 1u);
    EXPECT_EQ(et[0].name, "SYSARE11");
    EXPECT_EQ(et[0].satzlaenge, 128u);
    EXPECT_EQ(et[0].boe_text, "01001");
    EXPECT_EQ(et[0].eoe_text, "12026");
    EXPECT_EQ(et[0].eod_text, "12013");
    EXPECT_EQ(et[0].schutz, 'P');
    EXPECT_EQ(et[0].austausch, ' ');
    EXPECT_TRUE(et[0].gueltig);
    EXPECT_EQ(et[0].anzahl(), (12 - 1) * 26 + 12);      // 298 Sektoren (Spur 1 Sektor 1 .. Spur 12 Sektor 12)
}

TEST(SiosFs, EtikettenSg1) {
    auto d = direkt(fixture("sios1526_sg1.hfe"), "sios_ss40");
    ASSERT_TRUE(d.fs) << d.error;
    const auto et = d.fs->etiketten();     // 17 DDR1-Platzhalter (DATA10..26) sind NICHT aktiv
    ASSERT_EQ(et.size(), 2u);
    EXPECT_EQ(et[0].name, "SYSORG09");
    EXPECT_EQ(et[0].satzlaenge, 1024u);
    EXPECT_EQ(et[0].boe_text, "01001");
    EXPECT_EQ(et[0].eoe_text, "28026");
    EXPECT_EQ(et[0].eod_text, "29001");
    EXPECT_EQ(et[0].austausch, 'E');
    EXPECT_EQ(et[0].fortsetzung, 'C');
    EXPECT_EQ(et[0].folge, "01");
    EXPECT_EQ(et[0].anzahl(), 28 * 26);
    EXPECT_EQ(et[1].name, "PHASELB");
    EXPECT_EQ(et[1].satzlaenge, 128u);
    EXPECT_EQ(et[1].boe_text, "29001");
    EXPECT_EQ(et[1].eoe_text, "34026");
    EXPECT_EQ(et[1].eod_text, "35001");
}

TEST(SiosFs, DkoiTabelle) {
    EXPECT_EQ(SiosFileSystem::dkoi(0xE5), 'V');
    EXPECT_EQ(SiosFileSystem::dkoi(0xD6), 'O');
    EXPECT_EQ(SiosFileSystem::dkoi(0xD3), 'L');
    EXPECT_EQ(SiosFileSystem::dkoi(0xF1), '1');
    EXPECT_EQ(SiosFileSystem::dkoi(0x40), ' ');
    // die fuenf Abweichungen von EBCDIC (Formatdokument §2)
    EXPECT_EQ(SiosFileSystem::dkoi(0x4A), '[');
    EXPECT_EQ(SiosFileSystem::dkoi(0x4F), '!');
    EXPECT_EQ(SiosFileSystem::dkoi(0x5A), ']');
    EXPECT_EQ(SiosFileSystem::dkoi(0x5F), '^');
    EXPECT_EQ(SiosFileSystem::dkoi(0x6A), '/');
}

// ─── Verzeichnis ─────────────────────────────────────────────────────────────

TEST(SiosFs, Dp1PhaselbObjekte) {
    auto d = direkt(fixture("sios1526_dp1.hfe"), "sios_ss40");
    ASSERT_TRUE(d.fs) << d.error;
    const auto et = d.fs->etiketten();
    ASSERT_EQ(et.size(), 1u);
    EXPECT_EQ(et[0].name, "PHASELB");
    EXPECT_EQ(et[0].eod_text, "38001");

    std::vector<SiosObjekt> o;
    int nd = 0;
    ASSERT_TRUE(d.fs->bibliothek(et[0], o, &nd));
    EXPECT_EQ(nd, 0x03B3);
    const std::vector<std::pair<std::string, int>> soll = {
        {"CATM", 0x13},  {"CODK", 0x37},  {"COCA", 0x83},  {"COPD", 0xA5},  {"DINH", 0xFB},
        {"DIMO", 0x10D}, {"DIKA", 0x12B}, {"DMPM", 0x155}, {"EDIT", 0x167}, {"FGEN", 0x22D},
        {"INIT", 0x295}, {"DIDI", 0x2D5}, {"KORP", 0x303}, {"LBSV", 0x335}, {"CODP", 0x385}};
    ASSERT_EQ(o.size(), soll.size());
    for (size_t i = 0; i < soll.size(); ++i) {
        EXPECT_EQ(o[i].name, soll[i].first) << i;
        EXPECT_EQ(o[i].aadr, soll[i].second) << soll[i].first;
    }
    // Laenge = Abstand zum naechsten AADR, beim letzten zu ND
    EXPECT_EQ(o[0].laenge, 0x37 - 0x13);           // CATM: 36 Sektoren
    EXPECT_EQ(o[0].ml, 0x12);
    EXPECT_EQ(o.back().laenge, 0x3B3 - 0x385);     // CODP

    const auto l = d.fs->list();
    ASSERT_EQ(l.size(), 16u);                       // PHASELB + 15 Objekte
    EXPECT_EQ(l[0].name, "PHASELB");
    EXPECT_EQ(l[0].type, "B");
    EXPECT_EQ(l[0].size, 37u * 26u * 128u);
    EXPECT_EQ(l[0].attributes, "E");
    EXPECT_EQ(l[0].extent, "BOE 01001 EOE 37026 EOD 38001");
    EXPECT_EQ(l[1].name, "PHASELB/CATM");
    EXPECT_EQ(l[1].type, "O");
    EXPECT_EQ(l[1].size, 36u * 128u);
    EXPECT_EQ(l[1].extent, "AADR 0013 ML 12");
}

TEST(SiosFs, Dp2UndSg1Bibliotheken) {
    {
        auto d = direkt(fixture("sios1526_dp2.hfe"), "sios_ss40");
        ASSERT_TRUE(d.fs) << d.error;
        const auto l = d.fs->list();
        EXPECT_EQ(l.size(), 14u);                   // PHASELB + 13 Objekte
        EXPECT_NE(finde(l, "PHASELB/PRMFS"), nullptr);   // fuenf Zeichen, kein Nullbyte
        EXPECT_NE(finde(l, "PHASELB/SUS07"), nullptr);
        EXPECT_NE(finde(l, "PHASELB/SYSD"), nullptr);
    }
    {
        auto d = direkt(fixture("sios1526_sg1.hfe"), "sios_ss40");
        ASSERT_TRUE(d.fs) << d.error;
        const auto l = d.fs->list();
        ASSERT_EQ(l.size(), 5u);                    // SYSORG09, PHASELB, SGEN, SOPH, SYSD
        EXPECT_EQ(l[0].name, "SYSORG09");
        EXPECT_EQ(l[0].type, "F");                  // kein "LB"-Name: keine Bibliothek
        EXPECT_EQ(l[0].record_len, 1024);
        EXPECT_EQ(l[0].attributes, "EC");
        EXPECT_EQ(l[0].size, 28u * 26u * 128u);
        EXPECT_EQ(l[2].name, "PHASELB/SGEN");
        EXPECT_EQ(l[2].extent.substr(0, 10), "AADR 0013 ");
        EXPECT_EQ(l[3].extent.substr(0, 10), "AADR 0049 ");   // SOPH
        EXPECT_EQ(l[4].extent.substr(0, 10), "AADR 0063 ");   // SYSD
    }
}

TEST(SiosFs, SystemdisketteListe) {
    std::string err;
    auto v = oeffne("sios1526_syls.hfe", err);
    ASSERT_TRUE(v) << err;
    const auto l = v->list();
    ASSERT_EQ(l.size(), 1u);
    EXPECT_EQ(l[0].name, "SYSARE11");
    EXPECT_EQ(l[0].type, "F");
    EXPECT_EQ(l[0].attributes, "P");
    EXPECT_EQ(l[0].size, 298u * 128u);              // BOE .. EOD-1
    EXPECT_EQ(l[0].extent, "BOE 01001 EOE 12026 EOD 12013");
}

// ─── Lesen ───────────────────────────────────────────────────────────────────

TEST(SiosFs, ObjektBytesSindDerAusschnittDerBibliothek) {
    auto d = direkt(fixture("sios1526_dp1.hfe"), "sios_ss40");
    ASSERT_TRUE(d.fs) << d.error;
    std::vector<uint8_t> lib;
    ASSERT_TRUE(d.fs->read("PHASELB", lib));
    EXPECT_EQ(lib.size(), 962u * 128u);
    std::vector<uint8_t> init;
    ASSERT_TRUE(d.fs->read("PHASELB/INIT", init));
    ASSERT_EQ(init.size(), (0x2D5u - 0x295u) * 128u);
    const size_t ab = (0x295u - 1u) * 128u;
    EXPECT_TRUE(std::equal(init.begin(), init.end(), lib.begin() + static_cast<long>(ab)));
    // Gross-/Kleinschreibung des Namens ist gleich
    std::vector<uint8_t> init2;
    ASSERT_TRUE(d.fs->read("phaselb/init", init2));
    EXPECT_EQ(init, init2);
    // Das Programm nennt sich selbst (Formatdokument §7.3): "INIT  1520 (SIEX) V 1.4"
    const std::string txt(init.begin(), init.end());
    EXPECT_NE(txt.find("INIT  1520 (SIEX)"), std::string::npos);
}

TEST(SiosFs, SystemdateiBisEod) {
    auto d = direkt(fixture("sios1526_syls.hfe"), "sios_ss80");
    ASSERT_TRUE(d.fs) << d.error;
    std::vector<uint8_t> sys;
    ASSERT_TRUE(d.fs->read("SYSARE11", sys));
    ASSERT_EQ(sys.size(), 298u * 128u);
    // Systemdatei beginnt mit dem SYL-Kopf (§3.5): "SYL 01 00 88 00"
    EXPECT_EQ(std::string(sys.begin(), sys.begin() + 3), "SYL");
    EXPECT_EQ(sys[3], 0x01);
    EXPECT_EQ(sys[5], 0x88);
}

TEST(SiosFs, Sg1SatzlaengeAendertNichtsAmLesen) {
    auto d = direkt(fixture("sios1526_sg1.hfe"), "sios_ss40");
    ASSERT_TRUE(d.fs) << d.error;
    std::vector<uint8_t> org;
    ASSERT_TRUE(d.fs->read("SYSORG09", org));
    ASSERT_EQ(org.size(), 728u * 128u);
    // Satz 0 = Verzeichnis: erster Eintrag "GRUND   " (ASCII), Anzahl "06", Position 6 (§8.1)
    EXPECT_EQ(std::string(org.begin(), org.begin() + 10), "GRUND   06");
    EXPECT_EQ(org[10], 6);
}

TEST(SiosFs, UnbekannterNameUndFirstSector) {
    auto d = direkt(fixture("sios1526_dp1.hfe"), "sios_ss40");
    ASSERT_TRUE(d.fs) << d.error;
    std::vector<uint8_t> x;
    EXPECT_FALSE(d.fs->read("GIBTESNICHT", x));
    EXPECT_FALSE(d.fs->lastError().empty());
    FsRecoverOrt ort;
    ASSERT_TRUE(d.fs->firstSector("PHASELB", ort));
    EXPECT_EQ(ort.cyl, 1);
    EXPECT_EQ(ort.head, 0);
    EXPECT_EQ(ort.sector, 1);
    ASSERT_TRUE(d.fs->firstSector("PHASELB/CODK", ort));      // AADR 0037 = Sektor 55 -> Spur 3 Sektor 3
    EXPECT_EQ(ort.cyl, 1 + 54 / 26);
    EXPECT_EQ(ort.sector, 54 % 26 + 1);
}

// ─── Schreiben (D4) ──────────────────────────────────────────────────────────

namespace {

/// @brief Beschreibbare Kopie einer Fixture (oder nur ein freier Pfad); raeumt sich weg.
struct Kopie {
    std::string pfad;
    Kopie(const char* fix, const std::string& name) {
        pfad = k1520test::tempPath(name);
        std::error_code ec;
        fs::remove(pfad, ec);
        if (fix) fs::copy_file(fixture(fix), pfad, fs::copy_options::overwrite_existing, ec);
        EXPECT_FALSE(ec);
    }
    ~Kopie() { std::error_code ec; fs::remove(pfad, ec); }
};

std::vector<uint8_t> muster(size_t n, uint8_t start = 1) {
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(start + i * 7);
    return v;
}

void schreibeDatei(const std::string& pfad, const std::vector<uint8_t>& d) {
    std::ofstream o(pfad, std::ios::binary);
    o.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
}

std::vector<uint8_t> leseDatei2(const std::string& pfad) {
    std::ifstream i(pfad, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(i), std::istreambuf_iterator<char>());
}

/// @brief Neue leere SIOS-Diskette (Datei) ueber DiskVolume::create.
std::unique_ptr<DiskVolume> neueDiskette(const std::string& pfad, const char* fsname,
                                         const std::string& label, std::string& err,
                                         const std::string& boot = "") {
    return DiskVolume::create(pfad, fsname, label, formate(), dateisysteme(), err, boot);
}

int befunde(DiskVolume& v) { return v.checkReport().zaehlerAb(FsSeverity::Warnung); }
int befunde(const SiosFileSystem& f) { return f.check(FsCheckLevel::Schnell, false).zaehlerAb(FsSeverity::Warnung); }

bool geloescht(Diskette& d, int sektor) {
    for (const LogicalSector& ls : d.space->trackSectors(0, 0))
        if (ls.id == sektor) return ls.deleted;
    ADD_FAILURE() << "Sektor " << sektor << " fehlt";
    return false;
}

}  // namespace

TEST(SiosFsSchreiben, LeereDisketteHatDenInhaltVonParagraph10) {
    Kopie k(nullptr, "k1520_test_sios_leer.hfe");
    std::string err;
    {
        auto v = neueDiskette(k.pfad, "sios_ss40", "test01", err);
        ASSERT_TRUE(v) << err;
        EXPECT_TRUE(v->list().empty());
        EXPECT_EQ(befunde(*v), 0) << v->checkReport().alsText();
    }
    auto d = direkt(k.pfad, "sios_ss40");
    ASSERT_TRUE(d.fs) << d.error;
    EXPECT_EQ(d.fs->volumeName(), "TEST01");
    EXPECT_EQ(d.fs->spuren(), 40);

    // Spur 0 Byte fuer Byte (§10)
    auto sektor = [&](int id) { SectorData s; EXPECT_TRUE(d.space->readSector(0, 0, static_cast<uint8_t>(id), s)); return s.data; };
    for (int id = 1; id <= 4; ++id) EXPECT_EQ(sektor(id), std::vector<uint8_t>(128, 0x40)) << id;
    std::vector<uint8_t> ermap(128, 0x40);
    for (size_t i = 80; i < 128; ++i) ermap[i] = 0;
    const uint8_t er[5] = {0xC5, 0xD9, 0xD4, 0xC1, 0xD7};
    std::copy(er, er + 5, ermap.begin());
    EXPECT_EQ(sektor(5), ermap);
    std::vector<uint8_t> leer6(128, 0x40);
    for (size_t i = 80; i < 128; ++i) leer6[i] = 0;
    EXPECT_EQ(sektor(6), leer6);
    const std::vector<uint8_t> v1 = sektor(7);
    EXPECT_EQ(d.fs->etikettenText(7).substr(0, 10), "VOL1TEST01");
    EXPECT_EQ(v1[70], 0xD4);                       // 'M'
    EXPECT_EQ(v1[76], 0xF0);
    EXPECT_EQ(v1[77], 0xF1);
    EXPECT_EQ(v1[79], 0xE6);                       // 'W'
    for (size_t i = 80; i < 128; ++i) EXPECT_EQ(v1[i], 0) << i;
    for (int id = 8; id <= 26; ++id) {
        std::vector<uint8_t> soll(128, 0x40);
        soll[0] = 0xC4;
        for (size_t i = 80; i < 128; ++i) soll[i] = 0;
        EXPECT_EQ(sektor(id), soll) << id;
    }
    // Datenmarken: 1..7 normal, 8..26 geloescht
    for (int id = 1; id <= 26; ++id) EXPECT_EQ(geloescht(d, id), id >= 8) << id;
    // Datenspuren: 26 x 128, natuerliche Folge, E5
    for (int cyl : {1, 20, 39}) {
        const auto& sl = d.space->trackSectors(static_cast<uint8_t>(cyl), 0);
        ASSERT_EQ(sl.size(), 26u) << cyl;
        for (size_t i = 0; i < sl.size(); ++i) {
            EXPECT_EQ(sl[i].id, i + 1) << cyl;
            EXPECT_EQ(sl[i].data, std::vector<uint8_t>(128, 0xE5));
            EXPECT_FALSE(sl[i].deleted);
        }
    }
}

TEST(SiosFsSchreiben, AnlegenMitAchtzigSpurenUndNameRegeln) {
    Kopie k(nullptr, "k1520_test_sios_leer80.hfe");
    std::string err;
    auto v = neueDiskette(k.pfad, "sios_ss80", "", err);
    ASSERT_TRUE(v) << err;
    EXPECT_EQ(befunde(*v), 0);
    Kopie k2(nullptr, "k1520_test_sios_leer_x.hfe");
    EXPECT_FALSE(neueDiskette(k2.pfad, "sios_ss40", "ZUVIELE1", err));       // 7 Zeichen
    EXPECT_NE(err.find("SIOS"), std::string::npos) << err;
    EXPECT_FALSE(neueDiskette(k2.pfad, "sios_ss40", "A\xC3\xA4" "B", err));
    // --boot ist (noch) nicht umgesetzt
    EXPECT_FALSE(neueDiskette(k2.pfad, "sios_ss40", "X", err, "/dev/null"));
    EXPECT_NE(err.find("SIOS"), std::string::npos) << err;
}

TEST(SiosFsSchreiben, RundlaufPhysischeDatei) {
    Kopie k(nullptr, "k1520_test_sios_rund.hfe");
    std::string err;
    { auto v = neueDiskette(k.pfad, "sios_ss40", "RUND", err); ASSERT_TRUE(v) << err; }
    const std::vector<uint8_t> a = muster(128 * 30), b = muster(128, 9), c = muster(128 * 26, 3);
    WriteOptions wo;
    {
        auto d = direkt(k.pfad, "sios_ss40", false);
        ASSERT_TRUE(d.fs) << d.error;
        ASSERT_TRUE(d.fs->write("alpha", a, wo)) << d.fs->lastError();
        ASSERT_TRUE(d.fs->write("BETA.1", b, wo));
        ASSERT_TRUE(d.fs->write("GAMMA", c, wo));
        const auto l = d.fs->list();
        ASSERT_EQ(l.size(), 3u);
        // erste Luecke ab Spur 1: ALPHA 2 Spuren (30 Sektoren), BETA 1, GAMMA 1
        EXPECT_EQ(finde(l, "ALPHA")->extent, "BOE 01001 EOE 02026 EOD 02005");
        EXPECT_EQ(finde(l, "BETA.1")->extent, "BOE 03001 EOE 03026 EOD 03002");
        EXPECT_EQ(finde(l, "GAMMA")->extent, "BOE 04001 EOE 04026 EOD 05001");
        EXPECT_EQ(finde(l, "ALPHA")->record_len, 128);
        std::vector<uint8_t> x;
        ASSERT_TRUE(d.fs->read("ALPHA", x));
        EXPECT_EQ(x, a);                                // 128-Vielfaches: bytegleich (EOD begrenzt)
        ASSERT_TRUE(d.fs->read("beta.1", x));
        EXPECT_EQ(x, b);
        ASSERT_TRUE(d.fs->read("GAMMA", x));
        EXPECT_EQ(x, c);
        EXPECT_EQ(befunde(*d.fs), 0);

        // Datenmarken der Etikettensaetze: 8,9,10 aktiv (FB), 11.. geloescht
        for (int id = 8; id <= 26; ++id) EXPECT_EQ(geloescht(d, id), id >= 11) << id;
        // HDR1-Text wie die Originale (Formatdokument §4.1)
        EXPECT_EQ(d.fs->etikettenText(8),
                  "HDR1 ALPHA            00128 01001 02026    E                              02005 ");
        // Rest bis EOE bleibt E5 (frische Diskette)
        SectorData s;
        ASSERT_TRUE(d.space->readSector(2, 0, 10, s));
        EXPECT_EQ(s.data, std::vector<uint8_t>(128, 0xE5));
        // Unvollstaendiger letzter Sektor wird mit E5 aufgefuellt
        ASSERT_TRUE(d.fs->write("KURZ", muster(130), WriteOptions{}));
        ASSERT_TRUE(d.fs->read("KURZ", x));
        ASSERT_EQ(x.size(), 256u);
        EXPECT_TRUE(std::equal(x.begin(), x.begin() + 130, muster(130).begin()));
        EXPECT_EQ(x[130], 0xE5);
        // leere Datei: eine Spur, EOD = BOE
        ASSERT_TRUE(d.fs->write("NIX", {}, WriteOptions{}));
        EXPECT_EQ(finde(d.fs->list(), "NIX")->extent, "BOE 06001 EOE 06026 EOD 06001");
        d.disk->flush();
    }
    // Neu oeffnen (Container-Rueckschreiben) - alles bleibt
    {
        auto d = direkt(k.pfad, "sios_ss40");
        ASSERT_TRUE(d.fs) << d.error;
        EXPECT_EQ(d.fs->list().size(), 5u);
    }
}

TEST(SiosFsSchreiben, LoeschenUndWiederverwenden) {
    Kopie k(nullptr, "k1520_test_sios_rm.hfe");
    std::string err;
    { auto v = neueDiskette(k.pfad, "sios_ss40", "RM", err); ASSERT_TRUE(v) << err; }
    auto d = direkt(k.pfad, "sios_ss40", false);
    ASSERT_TRUE(d.fs) << d.error;
    ASSERT_TRUE(d.fs->write("A", muster(128), WriteOptions{}));
    ASSERT_TRUE(d.fs->write("B", muster(128), WriteOptions{}));
    const std::string b_vorher = d.fs->etikettenText(9);
    ASSERT_TRUE(d.fs->erase("a"));
    auto l = d.fs->list();
    ASSERT_EQ(l.size(), 1u);
    EXPECT_EQ(l[0].name, "B");
    // HDR1 -> DDR1, Rest des Satzes bleibt, Marke F8
    const std::string t = d.fs->etikettenText(8);
    EXPECT_EQ(t.substr(0, 10), "DDR1 A    ") << t;
    EXPECT_TRUE(geloescht(d, 8));
    EXPECT_EQ(d.fs->etikettenText(9), b_vorher);
    // Daten bleiben stehen
    SectorData s;
    ASSERT_TRUE(d.space->readSector(1, 0, 1, s));
    EXPECT_EQ(s.data, muster(128));
    EXPECT_FALSE(d.fs->erase("A"));                      // nicht mehr da
    EXPECT_EQ(befunde(*d.fs), 0);
    // freier Platz: Spur 1 und Etikett 8 werden wiederverwendet
    ASSERT_TRUE(d.fs->write("C", muster(128, 5), WriteOptions{}));
    EXPECT_EQ(finde(d.fs->list(), "C")->extent, "BOE 01001 EOE 01026 EOD 01002");
    EXPECT_EQ(d.fs->etikettenText(8).substr(0, 7), "HDR1 C ");
    EXPECT_FALSE(geloescht(d, 8));
}

TEST(SiosFsSchreiben, UeberschreibenUndAblehnungen) {
    Kopie k(nullptr, "k1520_test_sios_abl.hfe");
    std::string err;
    { auto v = neueDiskette(k.pfad, "sios_ss40", "ABL", err); ASSERT_TRUE(v) << err; }
    auto d = direkt(k.pfad, "sios_ss40", false);
    ASSERT_TRUE(d.fs) << d.error;
    WriteOptions wo, ow;
    ow.overwrite = true;
    ASSERT_TRUE(d.fs->write("X", muster(128), wo));
    EXPECT_FALSE(d.fs->write("x", muster(128), wo));                 // existiert
    EXPECT_NE(d.fs->lastError().find("existiert"), std::string::npos);
    ASSERT_TRUE(d.fs->write("X", muster(128 * 40, 2), ow));          // ersetzt, braucht 2 Spuren
    std::vector<uint8_t> x;
    ASSERT_TRUE(d.fs->read("X", x));
    EXPECT_EQ(x, muster(128 * 40, 2));
    EXPECT_EQ(d.fs->list().size(), 1u);
    // Namen
    for (const char* schlecht : {"", "NEUNZEICHEN", "A B", "AE\xC3\xA4"}) {
        EXPECT_FALSE(d.fs->write(schlecht, muster(128), wo)) << schlecht;
        EXPECT_NE(d.fs->lastError().find("SIOS"), std::string::npos) << schlecht;
    }
    EXPECT_EQ(d.fs->list().size(), 1u);
    // zu gross: Spur 1..37 nutzbar, X belegt 2 -> 35 frei
    EXPECT_FALSE(d.fs->write("RIESIG", std::vector<uint8_t>(128u * 26 * 36, 1), wo));
    EXPECT_NE(d.fs->lastError().find("zusammenhaengend"), std::string::npos) << d.fs->lastError();
    EXPECT_TRUE(d.fs->write("PASST", std::vector<uint8_t>(128u * 26 * 35, 1), wo)) << d.fs->lastError();
    // alle Spuren 1..37 belegt: noch eine Datei geht nicht
    EXPECT_FALSE(d.fs->write("ZU", muster(1), wo));
    EXPECT_EQ(befunde(*d.fs), 0);
    // Reserve: Spur 38/39 unberuehrt
    for (int cyl : {38, 39}) {
        SectorData s;
        ASSERT_TRUE(d.space->readSector(static_cast<uint8_t>(cyl), 0, 1, s));
        EXPECT_EQ(s.data, std::vector<uint8_t>(128, 0xE5));
    }
}

TEST(SiosFsSchreiben, NeunzehnEtikettenDannVoll) {
    Kopie k(nullptr, "k1520_test_sios_19.hfe");
    std::string err;
    { auto v = neueDiskette(k.pfad, "sios_ss40", "VOLL", err); ASSERT_TRUE(v) << err; }
    auto d = direkt(k.pfad, "sios_ss40", false);
    ASSERT_TRUE(d.fs) << d.error;
    for (int i = 0; i < 19; ++i)
        ASSERT_TRUE(d.fs->write("F" + std::to_string(i), muster(128), WriteOptions{})) << i;
    EXPECT_FALSE(d.fs->write("F19", muster(128), WriteOptions{}));
    EXPECT_NE(d.fs->lastError().find("gefuellt"), std::string::npos) << d.fs->lastError();
}

TEST(SiosFsSchreiben, PlatzpruefungOhneSchreiben) {
    Kopie k(nullptr, "k1520_test_sios_fit.hfe");
    std::string err;
    { auto v = neueDiskette(k.pfad, "sios_ss40", "FIT", err); ASSERT_TRUE(v) << err; }
    auto d = direkt(k.pfad, "sios_ss40");
    ASSERT_TRUE(d.fs) << d.error;
    FitReport r;
    PlannedFile gross; gross.name = "G"; gross.size = 128u * 26 * 40;
    ASSERT_TRUE(d.fs->wouldFit({gross}, r));
    EXPECT_FALSE(r.fits);
    EXPECT_NE(r.detail.find("zusammenhaengend"), std::string::npos) << r.detail;
    PlannedFile ok1; ok1.name = "A"; ok1.size = 5000;
    PlannedFile ok2; ok2.name = "B"; ok2.size = 100;
    ASSERT_TRUE(d.fs->wouldFit({ok1, ok2}, r));
    EXPECT_TRUE(r.fits) << r.detail;
    EXPECT_EQ(r.dir_needed, 2);
    EXPECT_EQ(r.dir_free, 19);
    EXPECT_TRUE(d.fs->list().empty());
}

TEST(SiosFsSchreiben, FehlspurkartenEintragSperrt) {
    Kopie k("sios1526_dp1.hfe", "k1520_test_sios_ermap.hfe");
    auto d = direkt(k.pfad, "sios_ss40", false);
    ASSERT_TRUE(d.fs) << d.error;
    SectorData s;
    ASSERT_TRUE(d.space->readSector(0, 0, 5, s));
    s.data[6] = 0xF0;
    s.data[7] = 0xF7;                                 // irgendein Eintrag
    ASSERT_TRUE(d.space->writeSector(0, 0, 5, s.data));
    EXPECT_FALSE(d.fs->write("NEU", muster(128), WriteOptions{}));
    EXPECT_NE(d.fs->lastError().find("Fehlspurkarte"), std::string::npos) << d.fs->lastError();
}

// ─── Bibliotheken ────────────────────────────────────────────────────────────

TEST(SiosFsSchreiben, BibliothekObjektAnhaengenUndLoeschen) {
    Kopie k("sios1526_sg1.hfe", "k1520_test_sios_bib.hfe");     // PHASELB: 3 Objekte, ND 0087, Groesse 156
    auto d = direkt(k.pfad, "sios_ss40", false);
    ASSERT_TRUE(d.fs) << d.error;
    std::vector<uint8_t> vorher;
    ASSERT_TRUE(d.fs->read("PHASELB", vorher));
    const std::vector<uint8_t> obj = muster(128 * 5 + 7, 11);    // 6 Sektoren
    WriteOptions wo;
    ASSERT_TRUE(d.fs->write("phaselb/neu", obj, wo)) << d.fs->lastError();
    auto l = d.fs->list();
    const FileEntry* e = finde(l, "PHASELB/NEU");
    ASSERT_TRUE(e);
    EXPECT_EQ(e->extent, "AADR 0087 ML 03");                    // ND 0087 hex, ML = 6/2
    EXPECT_EQ(e->size, 6u * 128);
    std::vector<uint8_t> gelesen;
    ASSERT_TRUE(d.fs->read("PHASELB/NEU", gelesen));
    ASSERT_EQ(gelesen.size(), 768u);
    EXPECT_TRUE(std::equal(obj.begin(), obj.end(), gelesen.begin()));
    EXPECT_EQ(gelesen[obj.size()], 0xE5);
    // die alten Objekte unveraendert: nur Verzeichnissektor (Sektor 2) und die neuen Sektoren aendern sich
    std::vector<uint8_t> nachher;
    ASSERT_TRUE(d.fs->read("PHASELB", nachher));
    ASSERT_EQ(nachher.size(), vorher.size());
    for (size_t i = 0; i < vorher.size(); ++i) {
        const size_t sek = i / 128;                              // 0-basiert; AADR = sek + 1
        const bool verz = sek == 1, neu = sek >= 0x87 - 1 && sek < 0x87 - 1 + 6;
        if (!verz && !neu) ASSERT_EQ(nachher[i], vorher[i]) << i;
    }
    // Verzeichnis: 4. Eintrag, Fuss ND/NV (Formatdokument §7.2)
    const std::vector<uint8_t> dir(nachher.begin() + 128, nachher.begin() + 256);
    EXPECT_EQ(std::string(dir.begin() + 24, dir.begin() + 27), "NEU");
    EXPECT_EQ(dir[24 + 5], 0x87);
    EXPECT_EQ(dir[24 + 6], 0x00);
    EXPECT_EQ(dir[24 + 7], 3);
    EXPECT_EQ(dir[122], 0x8D);
    EXPECT_EQ(dir[123], 0x00);                                   // ND 0087 + 6
    EXPECT_EQ((dir[120] << 8) | dir[121], 0x0200 + 8 * 4);       // NV
    EXPECT_EQ(dir[124], 0x9C);                                   // Groesse unveraendert
    EXPECT_EQ(befunde(*d.fs), 0);

    // doppelt ablehnen, ueberschreiben = alt wird blank, neu wird angehaengt
    EXPECT_FALSE(d.fs->write("PHASELB/NEU", obj, wo));
    WriteOptions ow;
    ow.overwrite = true;
    ASSERT_TRUE(d.fs->write("PHASELB/NEU", muster(128, 2), ow)) << d.fs->lastError();
    l = d.fs->list();
    int neu_zahl = 0;
    for (const FileEntry& f : l) if (f.name == "PHASELB/NEU") ++neu_zahl;
    EXPECT_EQ(neu_zahl, 1);
    EXPECT_EQ(finde(l, "PHASELB/NEU")->extent, "AADR 008D ML 01");

    // Loeschen: Name blank, AADR/ML bleiben
    ASSERT_TRUE(d.fs->erase("PHASELB/neu"));
    l = d.fs->list();
    EXPECT_FALSE(finde(l, "PHASELB/NEU"));
    EXPECT_TRUE(finde(l, "PHASELB/SGEN"));
    std::vector<uint8_t> bib;
    ASSERT_TRUE(d.fs->read("PHASELB", bib));
    EXPECT_EQ(bib[128 + 4 * 8 + 5], 0x8D);                       // 5. Eintrag: AADR bleibt
    EXPECT_EQ(bib[128 + 4 * 8 + 0], 0x20);                       // Name blank
    EXPECT_FALSE(d.fs->erase("PHASELB/NEU"));
    EXPECT_EQ(befunde(*d.fs), 0);
}

TEST(SiosFsSchreiben, BibliothekAblehnungen) {
    Kopie k("sios1526_dp1.hfe", "k1520_test_sios_bib2.hfe");     // 15 Eintraege, voll
    auto d = direkt(k.pfad, "sios_ss40", false);
    ASSERT_TRUE(d.fs) << d.error;
    WriteOptions wo;
    EXPECT_FALSE(d.fs->write("PHASELB/XYZ", muster(128), wo));
    EXPECT_NE(d.fs->lastError().find("voll"), std::string::npos) << d.fs->lastError();
    EXPECT_FALSE(d.fs->write("NOLB/XYZ", muster(128), wo));
    EXPECT_NE(d.fs->lastError().find("nicht gefunden"), std::string::npos);
    EXPECT_FALSE(d.fs->write("PHASELB/ZUVIELE", muster(128), wo));    // 6 Zeichen
    EXPECT_FALSE(d.fs->write("PHASELB/", muster(128), wo));
    EXPECT_FALSE(d.fs->write("PHASELB/A/B", muster(128), wo));
    // loeschen im vollen Verzeichnis geht trotzdem (nur der Name wird blank)
    ASSERT_TRUE(d.fs->erase("PHASELB/CATM")) << d.fs->lastError();
    EXPECT_FALSE(finde(d.fs->list(), "PHASELB/CATM"));

    Kopie k2("sios1526_sg1.hfe", "k1520_test_sios_bib3.hfe");
    auto d2 = direkt(k2.pfad, "sios_ss40", false);
    ASSERT_TRUE(d2.fs) << d2.error;
    EXPECT_FALSE(d2.fs->write("SYSORG09/ABC", muster(128), wo));      // keine Bibliothek
    // PHASELB: 156 - 0x87 + 1 = 22 Sektoren frei
    EXPECT_FALSE(d2.fs->write("PHASELB/GROSS", muster(128 * 23), wo));
    EXPECT_NE(d2.fs->lastError().find("Platz"), std::string::npos) << d2.fs->lastError();
    EXPECT_TRUE(d2.fs->write("PHASELB/PASST", muster(128 * 22), wo)) << d2.fs->lastError();
    EXPECT_FALSE(d2.fs->write("PHASELB/NOCH", muster(128), wo));
    EXPECT_EQ(befunde(*d2.fs), 0);
}

// ─── Ueber DiskVolume (Container, Originale unveraendert) ────────────────────

TEST(SiosFsSchreiben, UeberDiskVolumeUndOriginalBleibt) {
    const std::string orig = fixture("sios1526_dp1.hfe");
    const auto vorher = leseDatei2(orig);
    Kopie k("sios1526_dp1.hfe", "k1520_test_sios_dv.hfe");
    const std::string quelle = k1520test::tempPath("k1520_test_sios_dv_q.bin");
    schreibeDatei(quelle, muster(300));
    {
        std::string err;
        auto v = DiskVolume::open(k.pfad, "", formate(), dateisysteme(), err, false);
        ASSERT_TRUE(v) << err;
        EXPECT_FALSE(v->readOnly());
        EXPECT_FALSE(v->readOnlyForced());
        TransferOptions to;
        // DP1 ist voll (Spur 1..37 durch PHASELB): put einer physischen Datei scheitert mit Grund
        EXPECT_FALSE(v->insert(quelle, FileRef{0, "NEU"}, to));
        EXPECT_NE(v->lastError().find("SIOS"), std::string::npos) << v->lastError();
        EXPECT_FALSE(v->insert(quelle, FileRef{0, "PHASELB/XX"}, to));   // Verzeichnis voll
        ASSERT_TRUE(v->erase(FileRef{0, "PHASELB"})) << v->lastError();
        ASSERT_TRUE(v->insert(quelle, FileRef{0, "NEU"}, to)) << v->lastError();
        EXPECT_EQ(befunde(*v), 0) << v->checkReport().alsText();
        ASSERT_TRUE(v->flush()) << v->lastError();
    }
    {
        std::string err;
        auto v = DiskVolume::open(k.pfad, "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        const auto l = v->list();
        ASSERT_EQ(l.size(), 1u);
        EXPECT_EQ(l[0].name, "NEU");
        EXPECT_EQ(l[0].size, 384u);
        EXPECT_EQ(befunde(*v), 0) << v->checkReport().alsText();
    }
    std::error_code ec;
    fs::remove(quelle, ec);
    EXPECT_EQ(leseDatei2(orig), vorher);                 // Fixture unangetastet
}

TEST(SiosFsSchreiben, ContainerDmkUndImg) {
    for (const char* ext : {"dmk", "img"}) {
        Kopie k(nullptr, std::string("k1520_test_sios_c.") + ext);
        const std::string quelle = k1520test::tempPath("k1520_test_sios_c_q.bin");
        schreibeDatei(quelle, muster(256));
        std::string err;
        {
            auto v = neueDiskette(k.pfad, "sios_ss40", "CONT", err);
            ASSERT_TRUE(v) << ext << ": " << err;
            TransferOptions to;
            ASSERT_TRUE(v->insert(quelle, FileRef{0, "DATEI"}, to)) << v->lastError();
            ASSERT_TRUE(v->flush()) << v->lastError();
        }
        std::error_code ec;
        fs::remove(quelle, ec);
        auto v = DiskVolume::open(k.pfad, "sios_ss40", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << ext << ": " << err;
        ASSERT_EQ(v->list().size(), 1u) << ext;
        EXPECT_EQ(v->list()[0].size, 256u);
        EXPECT_EQ(befunde(*v), 0) << ext;
    }
}

// ─── Pruefung ────────────────────────────────────────────────────────────────

TEST(SiosFsCheck, OriginaleSindOhneBefund) {
    for (const char* datei : {"sios1526_dp1.hfe", "sios1526_dp2.hfe", "sios1526_sg1.hfe",
                              "sios1526_syls.hfe"}) {
        std::string err;
        auto v = oeffne(datei, err);
        ASSERT_TRUE(v) << datei << ": " << err;
        EXPECT_EQ(v->checkReport().zaehlerAb(FsSeverity::Warnung), 0)
            << datei << "\n" << v->checkReport().alsText();
    }
    std::string err;
    auto v = oeffne("sios1526_sylp.hfe", err, "sios_ss40");
    ASSERT_TRUE(v) << err;
    EXPECT_EQ(v->checkReport().zaehlerAb(FsSeverity::Warnung), 0) << v->checkReport().alsText();
}

TEST(SiosFsCheck, UeberlappendeBereicheWerdenGemeldet) {
    // SG1 in einer Kopie veraendern: BOE von PHASELB (Sektor 9) auf 20001 -> liegt in SYSORG09 (1..28).
    const std::string pfad = k1520test::tempPath("k1520_test_sios_ueberlapp.hfe");
    std::error_code ec;
    fs::copy_file(fixture("sios1526_sg1.hfe"), pfad, fs::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec);
    {
        auto d = direkt(pfad, "sios_ss40", /*nur_lesen=*/false);
        ASSERT_TRUE(d.fs) << d.error;
        SectorData s;
        ASSERT_TRUE(d.space->readSector(0, 0, 9, s));
        const uint8_t neu[5] = {0xF2, 0xF0, 0xF0, 0xF0, 0xF1};     // "20001"
        for (int i = 0; i < 5; ++i) s.data[static_cast<size_t>(28 + i)] = neu[i];
        ASSERT_TRUE(d.space->writeSector(0, 0, 9, s.data));
        const FsCheckReport r = d.fs->check(FsCheckLevel::Schnell, false);
        bool gefunden = false;
        for (const FsFinding& f : r.findings)
            if (f.id == "sios.hdr1.ueberlappt") gefunden = true;
        EXPECT_TRUE(gefunden) << r.alsText();
        EXPECT_GE(r.zaehlerAb(FsSeverity::Fehler), 1);
    }
    fs::remove(pfad, ec);
}
