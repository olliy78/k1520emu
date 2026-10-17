/**
 * @file test_osm.cpp
 * @brief OS/M-Disketten des P8000 (doc/design/34_osm.md, AP 2 + AP 3).
 *
 * OS/M ist CP/M 2.2 auf den SCP-Formaten des PC 1715; eigene Profile `osm640`/`osm800`
 * gibt es nur fuer den NAMEN, die Pruefung des Bootabbilds und die leere 800K-Diskette.
 * Erkannt wird NUR mit Urlader (`boot_header: osm`).  Die Abbilder hier sind synthetisch
 * (tests/support/osm_urlader.h: die drei Merkmale des Urladers, sonst 0xE5); die echten
 * Systemdisketten der pofo-Sammlung prueft `OsmEchtdaten.*` mit `K1520_CQM_DIR`.
 *
 * @see core/filesystem/disk_volume.cpp (Erkennung, „Deckungsgleiche Profile"),
 *      core/filesystem/check/cpm_check.cpp (`osm.fdconf`)
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/filesystem/check/fs_check.h"
#include "core/filesystem/disk_volume.h"
#include "tests/support/osm_urlader.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;

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

struct Weg {
    std::string p;
    explicit Weg(const char* name) : p(k1520test::tempPath(name)) {}
    ~Weg() { std::error_code ec; fs::remove_all(p, ec); }
};

std::unique_ptr<DiskVolume> oeffne(const std::string& pfad, const std::string& fs_name = "") {
    std::string err;
    auto v = DiskVolume::open(pfad, fs_name, formate(), dateisysteme(), err);
    EXPECT_TRUE(v) << pfad << ": " << err;
    return v;
}

/// @brief Legt @p pfad mit Profil @p profil an, optional mit Bootabbild (FDCONF @p fdconf).
bool anlegen(const std::string& pfad, const char* profil, int fdconf, size_t groesse,
             std::string& err) {
    std::string boot;
    if (fdconf >= 0) {
        boot = pfad + ".bin";
        k1520test::schreibeDatei(boot, k1520test::osmBootabbild(static_cast<uint8_t>(fdconf), groesse));
    }
    auto v = DiskVolume::create(pfad, profil, "", formate(), dateisysteme(), err, boot);
    if (!boot.empty()) { std::error_code ec; fs::remove(boot, ec); }
    return v && v->flush();
}

/// @brief Eine Datei auf die Diskette (Profil @p fs_name, "" = Erkennung).
void dateiDrauf(const std::string& pfad, const std::string& fs_name, const char* name) {
    Weg ordner("k1520_osm_quelle");
    fs::create_directories(ordner.p);
    std::ofstream(fs::path(ordner.p) / name, std::ios::binary) << "OS/M\r\n";
    std::string err;
    auto v = DiskVolume::open(pfad, fs_name, formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    v->setReadOnly(false);
    ASSERT_TRUE(v->insertAll(ordner.p, TransferOptions{})) << v->lastError();
    ASSERT_TRUE(v->flush()) << v->lastError();
}

std::vector<uint8_t> roh(const std::string& pfad) {
    std::ifstream f(pfad, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::vector<FsFinding> mitId(const FsCheckReport& r, const std::string& id) {
    std::vector<FsFinding> out;
    for (const FsFinding& f : r.findings) if (f.id == id) out.push_back(f);
    return out;
}

}  // namespace

// ─── Erkennung ───────────────────────────────────────────────────────────────

/// @test Systemdiskette 640K (Urlader, FDCONF 55): `osm640`, EINDEUTIG — obwohl `scpx640`
///       dieselbe Diskette ebenso liest (deckungsgleiche Profile sind keine Mehrdeutigkeit).
TEST(OsmProfile, Systemdiskette640WirdOsm640Eindeutig) {
    Weg d("k1520_osm640_sys.img");
    std::string err;
    ASSERT_TRUE(anlegen(d.p, "osm640", 0x55, 16384, err)) << err;
    dateiDrauf(d.p, "", "A.TXT");
    auto v = oeffne(d.p);
    ASSERT_TRUE(v);
    EXPECT_EQ(v->detection().filesystem, "osm640");
    EXPECT_TRUE(v->detection().unambiguous);
    EXPECT_TRUE(v->detection().alternatives.empty());
    EXPECT_EQ(v->profile().data_cyl, 2);
    EXPECT_EQ(v->list().size(), 1u);
    // Die Gegenprobe hat nichts gegenzuproben — und es gibt keinen Befund.
    EXPECT_TRUE(v->check(FsCheckLevel::Voll, true).findings.empty());
}

/// @test Systemdiskette 800K (FDCONF 77): `osm800`, eindeutig, Verzeichnis ab c2h0.
TEST(OsmProfile, Systemdiskette800WirdOsm800Eindeutig) {
    Weg d("k1520_osm800_sys.img");
    std::string err;
    ASSERT_TRUE(anlegen(d.p, "osm800", 0x77, 20480, err)) << err;
    dateiDrauf(d.p, "", "B.TXT");
    auto v = oeffne(d.p);
    ASSERT_TRUE(v);
    EXPECT_EQ(v->detection().filesystem, "osm800");
    EXPECT_TRUE(v->detection().unambiguous);
    EXPECT_EQ(v->profile().data_cyl, 2);
    EXPECT_EQ(v->profile().dir_entries, 128);
    EXPECT_EQ(v->bootAreaSize(), 20480u);
    EXPECT_EQ(v->list().size(), 1u);
    EXPECT_TRUE(v->check(FsCheckLevel::Voll, true).findings.empty());
}

/// @test Ohne Urlader kein „OS/M": Datendisketten bleiben, was sie heute sind.
///       Der UDOS-Urlader des P8000 traegt dieselbe Kennung P8000SYS — er zaehlt nicht.
TEST(OsmProfile, OhneUrladerBleibtEsBeiScpUndCpa) {
    Weg d640("k1520_osm640_daten.img");
    std::string err;
    ASSERT_TRUE(anlegen(d640.p, "scpx640", -1, 0, err)) << err;
    dateiDrauf(d640.p, "", "C.TXT");
    {
        auto v = oeffne(d640.p);
        ASSERT_TRUE(v);
        EXPECT_EQ(v->detection().filesystem, "scpx640");
        EXPECT_TRUE(v->detection().unambiguous);
    }
    // Kennung allein (Code eines anderen Urladers davor): kein OS/M.
    {
        std::vector<uint8_t> b = roh(d640.p);
        auto img = k1520test::osmBootabbild(0x55, 256);
        img[0] = 0x97;                      // UDOS-Urlader beginnt anders
        std::copy(img.begin(), img.end(), b.begin());
        k1520test::schreibeDatei(d640.p, b);
        auto v = oeffne(d640.p);
        ASSERT_TRUE(v);
        EXPECT_EQ(v->detection().filesystem, "scpx640");
    }
}

/// @test Die LEERE 800K-Diskette bleibt `cpa800` (das Medium unterscheidet sie nicht),
///       nennt aber den Ausweg `--fs osm800` — die Wahl aendert der Hinweis nie.
TEST(OsmProfile, Leere800NenntOsm800AlsAusweg) {
    Weg d("k1520_osm800_leer.img");
    std::string err;
    ASSERT_TRUE(anlegen(d.p, "cpa800", -1, 0, err)) << err;
    auto v = oeffne(d.p);
    ASSERT_TRUE(v);
    EXPECT_EQ(v->detection().filesystem, "cpa800");
    EXPECT_TRUE(v->detection().unambiguous);
    EXPECT_NE(v->detection().remarks.find("--fs osm800"), std::string::npos)
        << v->detection().remarks;
    // Kein Befund daraus: die Diskette ist in Ordnung, nur ihr Zweck unbekannt.
    EXPECT_TRUE(v->check(FsCheckLevel::Schnell, true).findings.empty());

    auto o = oeffne(d.p, "osm800");
    ASSERT_TRUE(o);
    EXPECT_EQ(o->profile().data_cyl, 2);
    EXPECT_EQ(o->profile().dir_entries, 128);
}

/// @test Auf 640K kein solcher Hinweis: `osm640` liegt deckungsgleich auf `scpx640`.
TEST(OsmProfile, Leere640OhneHinweisRauschen) {
    Weg d("k1520_osm640_leer.img");
    std::string err;
    ASSERT_TRUE(anlegen(d.p, "scpx640", -1, 0, err)) << err;
    auto v = oeffne(d.p);
    ASSERT_TRUE(v);
    EXPECT_EQ(v->detection().filesystem, "scpx640");
    EXPECT_EQ(v->detection().remarks.find("osm640"), std::string::npos)
        << v->detection().remarks;
}

/// @test `create --fs osm800` ohne System: `put` schreibt NICHT in die Systemzylinder
///       (Zylinder 0-1 bleiben 0xE5), und beim Wiederoeffnen liegt das Verzeichnis ab c2h0.
TEST(OsmProfile, LeereOsm800SchuetztDieSystemzylinder) {
    Weg d("k1520_osm800_neu.img");
    std::string err;
    ASSERT_TRUE(anlegen(d.p, "osm800", -1, 0, err)) << err;
    dateiDrauf(d.p, "osm800", "D.TXT");

    const std::vector<uint8_t> b = roh(d.p);
    ASSERT_EQ(b.size(), 819200u);
    for (size_t i = 0; i < 20480; ++i)
        ASSERT_EQ(b[i], 0xE5) << "Systemzylinder beschrieben bei Byte " << i;

    auto v = oeffne(d.p);
    ASSERT_TRUE(v);
    // Ohne Urlader ist es keine nachgewiesene OS/M-Diskette — aber die CP/A-Regel liest
    // das Verzeichnis an derselben Stelle wie OS/M.
    EXPECT_EQ(v->profile().data_cyl, 2) << v->detection().filesystem;
    EXPECT_EQ(v->profile().dir_entries, 128);
    ASSERT_EQ(v->list().size(), 1u);
    EXPECT_EQ(v->list().front().name, "D.TXT");
}

// ─── Bootabbild ──────────────────────────────────────────────────────────────

/// @test Das Bootabbild muss den OS/M-Urlader mit dem Typ der Geometrie tragen und lang
///       genug sein — sonst entsteht eine Diskette, die heil aussieht und nicht startet.
TEST(OsmProfile, BootabbildWirdGeprueft) {
    std::string err;
    {
        Weg d("k1520_osm_boot_typ.img");
        EXPECT_FALSE(anlegen(d.p, "osm800", 0x55, 20480, err));
        EXPECT_NE(err.find("Laufwerkstyp 5"), std::string::npos) << err;
    }
    {
        Weg d("k1520_osm_boot_typ640.img");
        EXPECT_FALSE(anlegen(d.p, "osm640", 0x77, 16384, err));
        EXPECT_NE(err.find("Laufwerkstyp 7"), std::string::npos) << err;
    }
    {
        Weg d("k1520_osm_boot_kurz.img");
        EXPECT_FALSE(anlegen(d.p, "osm640", 0x55, 4096, err));
        EXPECT_NE(err.find("28H Saetze"), std::string::npos) << err;
    }
    {
        Weg d("k1520_osm_boot_fremd.img");
        Weg b("k1520_osm_boot_fremd.bin");
        k1520test::schreibeDatei(b.p, std::vector<uint8_t>(16384, 0x00));
        EXPECT_FALSE(DiskVolume::create(d.p, "osm640", "", formate(), dateisysteme(), err, b.p));
        EXPECT_NE(err.find("keinen OS/M-Urlader"), std::string::npos) << err;
    }
    {
        // boot-put auf eine erkannte OS/M-Diskette prueft ebenso (auch ueber --fs scpx640).
        Weg d("k1520_osm_bootput.img");
        ASSERT_TRUE(anlegen(d.p, "osm640", 0x55, 16384, err)) << err;
        auto v = oeffne(d.p, "scpx640");
        ASSERT_TRUE(v);
        v->setReadOnly(false);
        EXPECT_FALSE(v->writeBootImage(std::vector<uint8_t>(16384, 0x00)));
        EXPECT_NE(v->lastError().find("OS/M-Urlader"), std::string::npos) << v->lastError();
    }
}

// ─── fsck: FDCONF des Urladers (AP 3) ────────────────────────────────────────

/// @test `osm.fdconf` (Vertrag): Urlader mit Typ 7 auf einer 640K-Diskette → Warnung,
///       c0h0.  Passender Typ → kein Befund.
TEST(FsCheckOsm, FdconfPasstNichtZurGeometrie) {
    Weg d("k1520_osm_fdconf.img");
    std::string err;
    ASSERT_TRUE(anlegen(d.p, "osm640", 0x55, 16384, err)) << err;
    {
        auto v = oeffne(d.p);
        ASSERT_TRUE(v);
        EXPECT_TRUE(mitId(v->check(FsCheckLevel::Schnell, true), "osm.fdconf").empty());
    }
    std::vector<uint8_t> b = roh(d.p);
    b[0x7E] = 0x77;
    k1520test::schreibeDatei(d.p, b);
    auto v = oeffne(d.p);
    ASSERT_TRUE(v);
    EXPECT_EQ(v->detection().filesystem, "osm640");   // die Geometrie entscheidet
    const auto f = mitId(v->check(FsCheckLevel::Schnell, true), "osm.fdconf");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_EQ(f.front().severity, FsSeverity::Warnung);
    EXPECT_EQ(f.front().layer, FsLayer::Verwaltung);
    EXPECT_NE(f.front().text.find("Typ 7"), std::string::npos) << f.front().text;
    EXPECT_NE(f.front().text.find("Typ 5"), std::string::npos) << f.front().text;
}

/// @test Typ 8/9 (Nutzerformate, ebenfalls 5×1024) auf einer 800K-Systemdiskette → Befund;
///       Typ 7 im oberen Nibble (B:) zaehlt nicht — geprueft wird A:.
TEST(FsCheckOsm, NurDasLaufwerkANibbleZaehlt) {
    Weg d("k1520_osm_fdconf8.img");
    std::string err;
    ASSERT_TRUE(anlegen(d.p, "osm800", 0x77, 20480, err)) << err;
    std::vector<uint8_t> b = roh(d.p);
    b[0x7E] = 0x57;                       // A: Typ 7, B: Typ 5
    k1520test::schreibeDatei(d.p, b);
    {
        auto v = oeffne(d.p);
        ASSERT_TRUE(v);
        EXPECT_TRUE(mitId(v->check(FsCheckLevel::Schnell, true), "osm.fdconf").empty());
    }
    b[0x7E] = 0x79;                       // A: Typ 9 (CP/A-Nutzerformat)
    k1520test::schreibeDatei(d.p, b);
    auto v = oeffne(d.p);
    ASSERT_TRUE(v);
    EXPECT_EQ(mitId(v->check(FsCheckLevel::Schnell, true), "osm.fdconf").size(), 1u);
}

// ─── Echtdaten (K1520_CQM_DIR) ───────────────────────────────────────────────

namespace {

/// @brief Profil je Abbild, wie es die Erkennung NACH AP 2 liefert.  Alle Abbilder, die
///        nicht tar720/wega720/udos1715 sind, stehen einzeln da; vor AP 2 waren die acht
///        OS/M-Systemdisketten scpx640 bzw. cpa_auto, alle anderen genau so.
const std::map<std::string, std::string>& erwartet() {
    static const std::map<std::string, std::string> m = {
        {"osm30", "osm640"},    {"osmsys", "osm640"},   {"osm_org", "osm640"},
        {"osmspiel", "osm800"}, {"osmut", "osm800"},    {"bsosm3", "osm800"},
        {"osmdb", "osm800"},    {"osmredab", "osm800"},
        {"c-beschr", "scpx640"}, {"ingdbd1", "scpx640"}, {"ingdbd23", "scpx640"},
        {"p8000dok", "scpx640"}, {"rempc172", "scpx640"}, {"wegrem23", "scpx640"},
        {"rempc171", "cpa_auto"}, {"konsul2", "cpa_auto"},
        {"wdos_dcp", ""},        {"wegrem33", ""},       // FAT — kein Dateisystem
    };
    return m;
}

/// @brief Alle Abbilder der Sammlung: `*.cqm` und die vollstaendigen `*.cqm.crdownload`
///        (als Kopie mit `.cqm`, die Erkennung geht nach der Endung).
std::vector<std::pair<std::string, std::string>> sammlung(const std::string& dir,
                                                          const std::string& kopien) {
    std::vector<std::pair<std::string, std::string>> r;
    fs::create_directories(kopien);
    for (const auto& e : fs::directory_iterator(dir)) {
        const std::string n = e.path().filename().string();
        const std::string cqm = ".cqm", cr = ".cqm.crdownload";
        if (n.size() > cqm.size() && n.compare(n.size() - cqm.size(), cqm.size(), cqm) == 0) {
            r.emplace_back(n.substr(0, n.size() - cqm.size()), e.path().string());
        } else if (n.size() > cr.size() && n.compare(n.size() - cr.size(), cr.size(), cr) == 0) {
            const std::string stamm = n.substr(0, n.size() - cr.size());
            const std::string ziel  = (fs::path(kopien) / (stamm + ".cqm")).string();
            fs::copy_file(e.path(), ziel, fs::copy_options::overwrite_existing);
            r.emplace_back(stamm, ziel);
        }
    }
    return r;
}

}  // namespace

/// @test Die ganze pofo-Sammlung: genau die acht Systemdisketten werden osm640/osm800,
///       alles andere behaelt sein Profil (Wachter gegen Erkennungsverschiebung).
TEST(OsmEchtdaten, ErkennungUeberDieGanzeSammlung) {
    const char* e = std::getenv("K1520_CQM_DIR");
    if (!e) GTEST_SKIP() << "K1520_CQM_DIR nicht gesetzt";
    Weg kopien("k1520_osm_crdownload");
    std::map<std::string, int> zahl;
    int n = 0;
    for (const auto& [stamm, pfad] : sammlung(e, kopien.p)) {
        ++n;
        std::string err;
        auto v = DiskVolume::open(pfad, "", formate(), dateisysteme(), err, true, true);
        ASSERT_TRUE(v) << stamm << ": " << err;
        const std::string fs_name = v->hasFileSystem() ? v->detection().filesystem : "";
        const auto it = erwartet().find(stamm);
        if (it != erwartet().end()) {
            EXPECT_EQ(fs_name, it->second) << stamm;
            if (!fs_name.empty()) EXPECT_TRUE(v->detection().unambiguous) << stamm;
        } else {
            ++zahl[fs_name];
            EXPECT_TRUE(fs_name == "tar720" || fs_name == "wega720" || fs_name == "udos1715")
                << stamm << " → " << fs_name;
        }
    }
    // 151 Abbilder (Stand 2026-10-17); einzelne Abbilder koennen beim Nachladen der
    // Sammlung fehlen (rempc172) — die Zuordnung der vorhandenen zaehlt.
    EXPECT_GE(n, 150);
    EXPECT_EQ(zahl["tar720"], 88);
    EXPECT_EQ(zahl["wega720"], 32);
    EXPECT_EQ(zahl["udos1715"], 13);
}

/// @test Die acht Systemdisketten: Dateizahl wie vorher, KEIN `osm.fdconf`, nur die
///       bekannten echten Befunde (osmut: 2 × `cpm.dir.name` fuer GW/ZSID.DOC).
TEST(OsmEchtdaten, SystemdiskettenOhneFalschmeldung) {
    const char* e = std::getenv("K1520_CQM_DIR");
    if (!e) GTEST_SKIP() << "K1520_CQM_DIR nicht gesetzt";
    const std::map<std::string, size_t> dateien = {
        {"osm30", 32}, {"osmsys", 33}, {"osm_org", 36}, {"osmspiel", 34},
        {"osmut", 32}, {"bsosm3", 81}, {"osmdb", 40},   {"osmredab", 86},
    };
    for (const auto& [stamm, anzahl] : dateien) {
        auto v = oeffne((fs::path(e) / (stamm + ".cqm")).string());
        ASSERT_TRUE(v) << stamm;
        EXPECT_EQ(v->list().size(), anzahl) << stamm;
        const FsCheckReport& r = v->check(FsCheckLevel::Voll, true);
        EXPECT_TRUE(mitId(r, "osm.fdconf").empty()) << stamm;
        EXPECT_EQ(r.zaehlerAb(FsSeverity::Warnung), stamm == "osmut" ? 2 : 0) << stamm;
        if (stamm == "osmut") EXPECT_EQ(mitId(r, "cpm.dir.name").size(), 2u);
    }
}
