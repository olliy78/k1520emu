/**
 * @file test_sios_fs.cpp
 * @brief GoogleTests fuer @ref SiosFileSystem — SIOS 1526 (A5120/A5130/K8924), lesend.
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

// ─── Nur lesen ───────────────────────────────────────────────────────────────

TEST(SiosFs, SchreibenIstGesperrt) {
    std::string err;
    auto v = oeffne("sios1526_dp1.hfe", err);
    ASSERT_TRUE(v) << err;
    EXPECT_TRUE(v->readOnly());
    v->setReadOnly(false);                           // laesst sich nicht aufheben
    EXPECT_TRUE(v->readOnly());
    EXPECT_TRUE(v->readOnlyForced());
    EXPECT_NE(v->lastError().find("SIOS"), std::string::npos) << v->lastError();

    auto d = direkt(fixture("sios1526_dp1.hfe"), "sios_ss40");
    ASSERT_TRUE(d.fs) << d.error;
    EXPECT_FALSE(d.fs->write("X", {1, 2, 3}, WriteOptions{}));
    EXPECT_NE(d.fs->lastError().find("SIOS"), std::string::npos);
    EXPECT_FALSE(d.fs->erase("PHASELB"));
    EXPECT_FALSE(d.fs->mkfs());
    FitReport fit;
    EXPECT_TRUE(d.fs->wouldFit({}, fit));            // Auskunft, kein Fehler
    EXPECT_FALSE(fit.fits);
    EXPECT_NE(fit.detail.find("SIOS"), std::string::npos);

    // Anlegen einer leeren SIOS-Diskette: erst D4
    const std::string pfad = k1520test::tempPath("k1520_test_sios_neu.hfe");
    std::string e2;
    EXPECT_FALSE(DiskVolume::create(pfad, "sios_ss40", "TEST", formate(), dateisysteme(), e2));
    EXPECT_NE(e2.find("SIOS"), std::string::npos);
    std::error_code ec;
    fs::remove(pfad, ec);
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
