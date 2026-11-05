/**
 * @file test_bs600_fs.cpp
 * @brief GoogleTests fuer @ref Bs600FileSystem (Entwurf 48, AP-B3).
 *
 * Sollwerte vom Original BS600.hfe (`doc/bs600_diskettenformat.md`): 28 Dateien, `BS620` Spur 6/15 bis
 * 13/13 laedt 6D00..D4FF, Start 6D5F.  Das Original liegt ausserhalb des Repos (GTEST_SKIP, wenn es fehlt)
 * und wird nie direkt gemountet, sondern als Kopie im Temp-Verzeichnis.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "core/filesystem/bs600/bs600_fs.h"
#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;

namespace {

std::string fixture(const char* name) { return (fs::path(FIXTURE_DIR) / ".." / "bs600" / name).string(); }

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

std::unique_ptr<DiskVolume> neu(const std::string& name) {
    std::string err;
    const std::string pfad = k1520test::tempPath(name);
    fs::remove(pfad);
    auto v = DiskVolume::create(pfad, "bs600_ss40_15x256_boot", "TEST", formate(), dateisysteme(), err);
    EXPECT_TRUE(v) << err;
    return v;
}

std::vector<uint8_t> muster(size_t n, uint8_t seed) {
    std::vector<uint8_t> d(n);
    for (size_t i = 0; i < n; ++i) d[i] = static_cast<uint8_t>(seed + i * 7);
    return d;
}

Bs600FileSystem& bfs(DiskVolume& v) { return *v.bs600Fs(); }

bool ablegen(DiskVolume& v, const std::string& name, const std::vector<uint8_t>& d, bool ueberschreiben = false,
             uint16_t lade = 0x2A00, uint16_t start = 0) {
    WriteOptions wo;
    wo.overwrite = ueberschreiben;
    wo.bs600_load = lade;
    wo.bs600_entry = start;
    wo.bs600_author = "TST";
    wo.bs600_date = "041124";
    return bfs(v).write(name, d, wo);
}

}  // namespace

TEST(Bs600Fs, LeereDisketteMitKatalogprofil) {
    auto v = neu("bs600_leer.hfe");
    ASSERT_TRUE(v);
    EXPECT_EQ(v->fsType(), FsType::Bs600);
    EXPECT_TRUE(v->list().empty());
    EXPECT_EQ(v->volumeInfo(0).label, "TEST");
    EXPECT_EQ(v->volumeInfo(0).free_bytes, 37u * 15u * 256u);
}

TEST(Bs600Fs, PutLesenUndBelegungstabelle) {
    auto v = neu("bs600_put.hfe");
    ASSERT_TRUE(v);
    const auto a = muster(1000, 1), b = muster(256, 9), c = muster(5000, 3);
    ASSERT_TRUE(ablegen(*v, "aaa", a)) << bfs(*v).lastError();
    ASSERT_TRUE(ablegen(*v, "BBB", b));
    ASSERT_TRUE(ablegen(*v, "CCC", c, false, 0x3000, 0x3010));
    std::vector<uint8_t> r;
    ASSERT_TRUE(bfs(*v).read("AAA", r));
    EXPECT_EQ(r, a);
    ASSERT_TRUE(bfs(*v).read("BBB", r));
    EXPECT_EQ(r, b);
    ASSERT_TRUE(bfs(*v).read("CCC", r));
    EXPECT_EQ(r, c);
    const auto e = bfs(*v).eintraege();
    ASSERT_EQ(e.size(), 3u);
    EXPECT_EQ(e[0].anf_spur, 1); EXPECT_EQ(e[0].anf_sek, 1); EXPECT_EQ(e[0].end_spur, 1); EXPECT_EQ(e[0].end_sek, 4);
    EXPECT_EQ(e[1].anf_sek, 5);
    EXPECT_EQ(e[2].ladeadr, 0x3000); EXPECT_EQ(e[2].endadr, 0x3000 + 5000 - 1); EXPECT_EQ(e[2].startadr, 0x3010);
    EXPECT_EQ(e[0].datum, "041124"); EXPECT_EQ(e[0].signum, "TST");
    // 4 + 1 + 20 = 25 Sektoren: Spur 1 voll, Spur 2 zehn Sektoren
    const auto t = bfs(*v).belegungstabelle();
    int n = 0;
    for (uint8_t x : t) n += x;
    EXPECT_EQ(n, 25);
    EXPECT_EQ(t[1 * 16 + 15], 1);
    EXPECT_EQ(t[2 * 16 + 10], 1);
    EXPECT_EQ(t[2 * 16 + 11], 0);
    EXPECT_EQ(v->volumeInfo(0).free_bytes, (37u * 15u - 25u) * 256u);
}

TEST(Bs600Fs, LoeschenGibtFreiUndLueckeWirdGefuellt) {
    auto v = neu("bs600_rm.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(256 * 3, 1)));
    ASSERT_TRUE(ablegen(*v, "B", muster(256 * 3, 2)));
    ASSERT_TRUE(ablegen(*v, "C", muster(256 * 3, 3)));
    ASSERT_TRUE(bfs(*v).erase("B"));
    EXPECT_EQ(bfs(*v).eintraege().size(), 2u);
    auto t = bfs(*v).belegungstabelle();
    EXPECT_EQ(t[1 * 16 + 3], 1);
    EXPECT_EQ(t[1 * 16 + 4], 0);
    EXPECT_EQ(t[1 * 16 + 6], 0);
    EXPECT_EQ(t[1 * 16 + 7], 1);
    ASSERT_TRUE(ablegen(*v, "D", muster(256 * 3, 4)));            // erster freier Lauf = die Luecke
    bool ok = false;
    for (const auto& e : bfs(*v).eintraege())
        if (e.name == "D") { EXPECT_EQ(e.anf_sek, 4); ok = true; }
    EXPECT_TRUE(ok);
    ASSERT_TRUE(bfs(*v).erase("D"));
    ASSERT_TRUE(ablegen(*v, "E", muster(256 * 4, 5)));            // zu gross fuer die Luecke
    for (const auto& e : bfs(*v).eintraege())
        if (e.name == "E") EXPECT_EQ(e.anf_sek, 10);
    EXPECT_FALSE(ablegen(*v, "E", muster(10, 1)));
    EXPECT_TRUE(ablegen(*v, "E", muster(10, 1), true));
}

TEST(Bs600Fs, ReorganizeSchliesstLueckenOhneDatenverlust) {
    auto v = neu("bs600_reorg.hfe");
    ASSERT_TRUE(v);
    const auto a = muster(700, 1), c = muster(2000, 3);
    ASSERT_TRUE(ablegen(*v, "A", a));
    ASSERT_TRUE(ablegen(*v, "B", muster(900, 2)));
    ASSERT_TRUE(ablegen(*v, "C", c, false, 0x4000, 0x4001));
    ASSERT_TRUE(bfs(*v).erase("B"));
    int bewegt = 0;
    ASSERT_TRUE(bfs(*v).reorganize(&bewegt)) << bfs(*v).lastError();
    EXPECT_EQ(bewegt, 1);
    std::vector<uint8_t> r;
    ASSERT_TRUE(bfs(*v).read("C", r));
    EXPECT_EQ(r, c);
    ASSERT_TRUE(bfs(*v).read("A", r));
    EXPECT_EQ(r, a);
    for (const auto& e : bfs(*v).eintraege())
        if (e.name == "C") {
            EXPECT_EQ(e.anf_spur * 100 + e.anf_sek, 104);        // A belegt 3 Sektoren (700 B)
            EXPECT_EQ(e.sektoren(), 8);                          // 2000 B: das ENDE muss mitwandern (Fehler in AP-B5 gefunden)
            EXPECT_EQ(e.startadr, 0x4001);
        }
    const auto t = bfs(*v).belegungstabelle();
    int n = 0;
    for (uint8_t x : t) n += x;
    EXPECT_EQ(n, 3 + 8);
    ASSERT_TRUE(bfs(*v).reorganize(&bewegt));
    EXPECT_EQ(bewegt, 0);
}

TEST(Bs600Fs, VierundfuenfzigEintraegeUndDiskVoll) {
    auto v = neu("bs600_54.hfe");
    ASSERT_TRUE(v);
    for (int i = 0; i < 54; ++i)
        ASSERT_TRUE(ablegen(*v, "F" + std::to_string(i), muster(10, static_cast<uint8_t>(i))))
            << i << " " << bfs(*v).lastError();
    EXPECT_FALSE(ablegen(*v, "F54", muster(10, 1)));
    EXPECT_NE(bfs(*v).lastError().find("voll"), std::string::npos) << bfs(*v).lastError();
    ASSERT_TRUE(bfs(*v).erase("F7"));
    ASSERT_TRUE(ablegen(*v, "NEU", muster(10, 1)));
    EXPECT_EQ(bfs(*v).eintraege().size(), 54u);

    auto w = neu("bs600_voll.hfe");
    ASSERT_TRUE(w);
    // Eine Datei fasst hoechstens 64 KB: Spur 1..37 (555 Sektoren) in vier Dateien zu 138 + eine zu 3 Sektoren.
    for (int i = 0; i < 4; ++i)
        ASSERT_TRUE(ablegen(*w, "G" + std::to_string(i), muster(138 * 256, 1), false, 0)) << bfs(*w).lastError();
    ASSERT_TRUE(ablegen(*w, "REST", muster(3 * 256, 1), false, 0)) << bfs(*w).lastError();
    EXPECT_EQ(w->volumeInfo(0).free_bytes, 0u);
    EXPECT_FALSE(ablegen(*w, "MEHR", muster(1, 1)));
    EXPECT_NE(bfs(*w).lastError().find("DISK VOLL"), std::string::npos);
}

TEST(Bs600Fs, AttributeUmbenennenUndRestBleibt) {
    auto v = neu("bs600_attr.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "PROG", muster(300, 1), false, 0x2A00, 0x2A10));
    Bs600Attrs a;
    a.type = "L"; a.set_date = true; a.date = "01.02.88"; a.set_author = true; a.author = "KRU";
    a.set_load = true; a.load = 0x6D00; a.set_entry = true; a.entry = 0;
    ASSERT_TRUE(bfs(*v).setAttributes("PROG", a)) << bfs(*v).lastError();
    auto e = bfs(*v).eintraege();
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e[0].typ, 'L'); EXPECT_EQ(e[0].datum, "010288"); EXPECT_EQ(e[0].signum, "KRU");
    EXPECT_EQ(e[0].ladeadr, 0x6D00); EXPECT_EQ(e[0].endadr, 0x6D00 + 300 - 1); EXPECT_EQ(e[0].startadr, 0);
    ASSERT_TRUE(bfs(*v).rename("PROG", "NEUNAM"));
    EXPECT_EQ(bfs(*v).eintraege()[0].name, "NEUNAM");
    EXPECT_FALSE(bfs(*v).rename("NEUNAM", "ZU.LANG"));
    a = Bs600Attrs{}; a.type = "X";
    EXPECT_FALSE(bfs(*v).setAttributes("NEUNAM", a));
}

TEST(Bs600Fs, UrladerKopfWirdGeprueft) {
    auto v = neu("bs600_boot.hfe");
    ASSERT_TRUE(v);
    std::vector<uint8_t> boot(384, 0);
    EXPECT_FALSE(v->writeBootImage(boot));                        // kein Kopf
    boot[0] = 0x18; boot[1] = 0x0E;
    const std::string k = "SYL FOR BS620 ";
    std::copy(k.begin(), k.end(), boot.begin() + 2);
    ASSERT_TRUE(v->writeBootImage(boot)) << v->lastError();
    std::vector<uint8_t> r;
    ASSERT_TRUE(v->readBootImage(r));
    EXPECT_EQ(r, boot);
    boot.resize(400);
    EXPECT_FALSE(v->writeBootImage(boot));                        // mehr als Sektor 1..3
    ASSERT_TRUE(ablegen(*v, "X", muster(10, 1)));
    EXPECT_EQ(v->list().size(), 1u);
}

TEST(Bs600Fs, GegenprobeOriginalRundlauf) {
    const std::string kopie = k1520test::tempPath("bs600_original.hfe");
    fs::copy_file(fixture("bs620_prg710-1.hfe"), kopie, fs::copy_options::overwrite_existing);
    std::string err;
    auto o = DiskVolume::open(kopie, "", formate(), dateisysteme(), err, /*read_only=*/true);
    ASSERT_TRUE(o) << err;
    EXPECT_EQ(o->fsType(), FsType::Bs600);
    EXPECT_EQ(o->detection().filesystem, "bs600_ss40_15x256_boot");
    const auto liste = o->list();
    ASSERT_EQ(liste.size(), 28u);
    std::map<std::string, std::vector<uint8_t>> inhalt;
    for (const FileEntry& e : liste) {
        EXPECT_FALSE(e.damaged) << e.name;
        std::vector<uint8_t> d;
        ASSERT_TRUE(bfs(*o).read(e.name, d)) << e.name;
        EXPECT_EQ(d.size(), e.size) << e.name;
        inhalt[e.name] = d;
    }
    ASSERT_TRUE(inhalt.count("BS620"));
    EXPECT_EQ(inhalt["BS620"].size(), 0xD4FFu - 0x6D00u + 1u);
    const auto alt = bfs(*o).eintraege();
    bool gefunden = false;
    for (const auto& e : alt)
        if (e.name == "BS620") {
            gefunden = true;
            EXPECT_EQ(e.ladeadr, 0x6D00); EXPECT_EQ(e.startadr, 0x6D5F);
            EXPECT_EQ(e.datum, "201288"); EXPECT_EQ(e.signum, "KRU");
        }
    EXPECT_TRUE(gefunden);
    EXPECT_EQ(o->volumeInfo(0).free_bytes, 174u * 256u);   // 204 frei laut Tabelle, davon 30 in Spur 38/39 (nie vergeben)

    // Rundlauf: alle Dateien mit ihren Angaben auf eine neue Diskette, Urlader mit.
    std::vector<uint8_t> boot;
    ASSERT_TRUE(o->readBootImage(boot));
    auto n = neu("bs600_rund.hfe");
    ASSERT_TRUE(n);
    ASSERT_TRUE(n->writeBootImage(boot)) << n->lastError();
    for (const auto& e : alt) {
        WriteOptions wo;
        wo.bs600_type = std::string(1, e.typ);
        wo.bs600_date = e.datum; wo.bs600_author = e.signum;
        wo.bs600_load = static_cast<uint16_t>(e.ladeadr); wo.bs600_entry = static_cast<uint16_t>(e.startadr);
        ASSERT_TRUE(bfs(*n).write(e.name, inhalt[e.name], wo)) << e.name << ": " << bfs(*n).lastError();
    }
    for (const auto& [name, d] : inhalt) {
        std::vector<uint8_t> r;
        ASSERT_TRUE(bfs(*n).read(name, r)) << name;
        EXPECT_EQ(r, d) << name;
    }
    const auto neuE = bfs(*n).eintraege();
    ASSERT_EQ(alt.size(), neuE.size());
    for (size_t i = 0; i < alt.size(); ++i) {
        EXPECT_EQ(alt[i].ladeadr, neuE[i].ladeadr);
        EXPECT_EQ(alt[i].startadr, neuE[i].startadr);
        EXPECT_EQ(alt[i].datum, neuE[i].datum);
    }
    EXPECT_EQ(n->volumeInfo(0).free_bytes, o->volumeInfo(0).free_bytes);
    ASSERT_TRUE(n->flush());
    std::string err2;
    auto w = DiskVolume::open(n->path(), "", formate(), dateisysteme(), err2);
    ASSERT_TRUE(w) << err2;
    EXPECT_EQ(w->fsType(), FsType::Bs600);
    EXPECT_EQ(w->list().size(), 28u);
}

TEST(Bs600Fs, Prg700DisketteWirdGelesen) {
    const std::string kopie = k1520test::tempPath("bs600_prg700.hfe");
    fs::copy_file(fixture("bs600_prg700.hfe"), kopie, fs::copy_options::overwrite_existing);
    std::string err;
    auto o = DiskVolume::open(kopie, "", formate(), dateisysteme(), err, /*read_only=*/true);
    ASSERT_TRUE(o) << err;
    EXPECT_EQ(o->fsType(), FsType::Bs600);
    ASSERT_EQ(o->list().size(), 25u);
    for (const FileEntry& e : o->list()) {
        EXPECT_FALSE(e.damaged) << e.name;
        std::vector<uint8_t> d;
        ASSERT_TRUE(bfs(*o).read(e.name, d)) << e.name;
        EXPECT_EQ(d.size(), e.size) << e.name;
    }
    EXPECT_EQ(o->volumeInfo(0).free_bytes, 307u * 256u);           // 337 frei laut Tabelle, 30 in Spur 38/39
    std::vector<uint8_t> boot;
    ASSERT_TRUE(o->readBootImage(boot));
    EXPECT_EQ(std::string(boot.begin() + 2, boot.begin() + 15), "BOOTSTRAP FOR");
}

TEST(Bs600Fs, VerdraengtKeinAnderesProfil) {
    for (const FsProfile& p : dateisysteme().profiles())
        if (p.format == "ss40_1t26x128-15x256") EXPECT_EQ(p.type, FsType::Bs600) << p.name;
}
