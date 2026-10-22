/**
 * @file test_platten_volume.cpp
 * @brief GoogleTests fuer @ref PlattenVolume — die Plattenfassade (md0 ... md4) mit `.fileinfo`.
 *
 * Die Platte wird synthetisch gebaut (PAR + zwei WEGA-Partitionen md0 und md2 der
 * Standardtabelle); Fixtures und Umgebungsvariablen werden nicht gebraucht.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/filesystem/fileinfo.h"
#include "core/filesystem/wega/platten_volume.h"
#include "core/util/gzip_datei.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;

namespace {

constexpr int kZyl = 800, kKoepfe = 2, kSek = 17;     // 799*34 = 27166 Bloecke > md2 (16000+7000)

/// Platte mit PAR, md0 (/usr, 13000 Bloecke) und md2 (/, 7000 Bloecke ab 16000) formatiert.
std::vector<uint8_t> baueplatte() {
    std::vector<uint8_t> b(size_t(kZyl) * kKoepfe * kSek * 512, 0xE5);
    uint8_t* s = b.data();
    std::memset(s, 0, 512);
    std::memcpy(s, "DEFEKT", 6);
    s[6] = 0;
    s[8] = s[9] = s[10] = 0xFF;
    std::memcpy(s + 256, "PARMTR", 6);
    std::memcpy(s + 262, "TEST PLATTE ", 12);
    s[277] = kZyl & 0xFF; s[278] = kZyl >> 8; s[279] = kKoepfe; s[280] = kSek;
    for (const auto& [start, n] : {std::pair<uint32_t, uint32_t>{0, 13000}, {16000, 7000}}) {
        auto off = [start = start](uint32_t bn) -> int64_t {
            return (int64_t(start) + bn + kKoepfe * kSek) * 512;
        };
        std::string err;
        EXPECT_TRUE(WegaFileSystem::format(std::make_unique<WegaSpeicherDev>(b, n, off, nullptr),
                                           "t", err, 0, 3, 34)) << err;
    }
    return b;
}

void schreibe(const std::string& pfad, const std::vector<uint8_t>& b) {
    std::ofstream(pfad, std::ios::binary).write(reinterpret_cast<const char*>(b.data()),
                                                static_cast<std::streamsize>(b.size()));
}

/// Eine frische Platte als Datei; Aufraeumen im Destruktor.
struct TempPlatte {
    std::string pfad;
    explicit TempPlatte(const std::string& name, bool gz = false) : pfad(k1520test::tempPath(name)) {
        const std::vector<uint8_t> b = baueplatte();
        if (gz) k1520::gzip::speichern(pfad, b.data(), b.size(), k1520::gzip::Art::Gzip);
        else    schreibe(pfad, b);
    }
    ~TempPlatte() {
        std::error_code ec;
        fs::remove(pfad, ec);
        fs::remove(pfad + "~", ec);
    }
};

std::unique_ptr<PlattenVolume> oeffne(const std::string& pfad, bool ro = false) {
    std::string err;
    auto v = PlattenVolume::open(pfad, err, ro);
    EXPECT_TRUE(v) << err;
    return v;
}

const FileEntry* finde(const std::vector<FileEntry>& l, const std::string& n) {
    for (const FileEntry& e : l) if (e.name == n) return &e;
    return nullptr;
}

std::vector<uint8_t> muster(size_t n, int k) {
    std::vector<uint8_t> d(n);
    for (size_t i = 0; i < n; ++i) d[i] = static_cast<uint8_t>(i * k);
    return d;
}

}  // namespace

TEST(PlattenVolume, WurzelZeigtPartitionenAlsOrdner) {
    TempPlatte t("pv_wurzel.img");
    auto v = oeffne(t.pfad, true);
    ASSERT_TRUE(v);
    EXPECT_TRUE(v->readOnly());
    const auto l = v->list();
    const FileEntry* md0 = finde(l, "md0");
    const FileEntry* md2 = finde(l, "md2");
    ASSERT_TRUE(md0 && md2);
    EXPECT_EQ(md0->type, "d");
    EXPECT_EQ(md2->volume, 1);
    EXPECT_EQ(finde(l, "md3"), nullptr);          // liegt hinter der Platte
}

TEST(PlattenVolume, SchreibschutzVerweigertSchreibenUndAendern) {
    TempPlatte t("pv_ro.img");
    auto v = oeffne(t.pfad, true);
    ASSERT_TRUE(v);
    EXPECT_FALSE(v->write("md2/x", {1, 2, 3}, {}));
    EXPECT_NE(v->lastError().find("schreibgeschuetzt"), std::string::npos);
    EXPECT_FALSE(v->makeDirectory("md2/d"));
    EXPECT_FALSE(v->erase("md2/x"));
    UnixAttrs a; a.set_mode = true; a.mode = 0600;
    EXPECT_FALSE(v->setAttributes("md2/x", a));
    EXPECT_FALSE(v->dirty());
    v->setReadOnly(false);
    EXPECT_TRUE(v->write("md2/x", {1, 2, 3}, {})) << v->lastError();
    EXPECT_TRUE(v->dirty());
}

TEST(PlattenVolume, SchreibenLesenLoeschenUndMkdir) {
    TempPlatte t("pv_rw.img");
    const auto inhalt = muster(70000, 7);
    {
        auto v = oeffne(t.pfad);
        ASSERT_TRUE(v);
        ASSERT_TRUE(v->makeDirectory("md2/etc/sub")) << v->lastError();
        WriteOptions w;
        w.wega_mode = 0640; w.wega_mode_gesetzt = true;
        w.wega_uid = 3; w.wega_gid = 4; w.wega_owner_gesetzt = true;
        ASSERT_TRUE(v->write("md2/etc/sub/datei", inhalt, w)) << v->lastError();
        ASSERT_TRUE(v->write("md0/lib/zwei", {9, 8, 7}, {})) << v->lastError();
        EXPECT_FALSE(v->write("md2", {1}, {}));         // Partitionswurzel ist keine Datei
        EXPECT_FALSE(v->write("md9/x", {1}, {}));       // gibt es nicht
        ASSERT_TRUE(v->flush()) << v->lastError();
    }
    EXPECT_TRUE(fs::exists(t.pfad + "~"));              // Sicherung vor dem 1. Schreiben
    {
        auto v = oeffne(t.pfad);
        std::vector<uint8_t> z;
        ASSERT_TRUE(v->read("md2/etc/sub/datei", z)) << v->lastError();
        EXPECT_EQ(z, inhalt);
        const auto l = v->list();
        const FileEntry* e = finde(l, "md2/etc/sub/datei");
        ASSERT_TRUE(e);
        EXPECT_EQ(e->unix_mode & 07777, 0640);
        EXPECT_EQ(e->unix_uid, 3);
        EXPECT_EQ(e->unix_gid, 4);
        ASSERT_TRUE(finde(l, "md2/etc/sub"));
        EXPECT_EQ(finde(l, "md2/etc/sub")->type, "d");
        EXPECT_TRUE(finde(l, "md0/lib/zwei"));          // Partitionsgrenze: md0 getrennt von md2
        EXPECT_FALSE(finde(l, "md2/lib/zwei"));
        EXPECT_FALSE(v->erase("md2/etc/sub"));          // nicht leer
        ASSERT_TRUE(v->erase("md2/etc/sub/datei")) << v->lastError();
        ASSERT_TRUE(v->erase("md2/etc/sub")) << v->lastError();
        EXPECT_FALSE(finde(v->list(), "md2/etc/sub"));
        EXPECT_FALSE(v->read("md2/etc/sub/datei", z));
        ASSERT_TRUE(v->flush());
        EXPECT_TRUE(v->check(FsCheckLevel::Voll).ohneBefund());
    }
}

TEST(PlattenVolume, SetAttributesAendertModusBesitzerZeit) {
    TempPlatte t("pv_attr.img");
    auto v = oeffne(t.pfad);
    ASSERT_TRUE(v);
    ASSERT_TRUE(v->write("md2/f", {1}, {})) << v->lastError();
    UnixAttrs a;
    a.set_mode = true;  a.mode = 04755;
    a.set_uid = true;   a.uid = 12;
    a.set_gid = true;   a.gid = 34;
    a.set_mtime = true; a.mtime = 1234567890;
    ASSERT_TRUE(v->setAttributes("md2/f", a)) << v->lastError();
    ASSERT_TRUE(v->flush());
    auto w = oeffne(t.pfad, true);
    const auto l = w->list();
    const FileEntry* e = finde(l, "md2/f");
    ASSERT_TRUE(e);
    EXPECT_EQ(e->unix_mode & 07777, 04755);
    EXPECT_EQ(e->unix_uid, 12);
    EXPECT_EQ(e->unix_gid, 34);
    EXPECT_EQ(e->unix_mtime, 1234567890u);
}

TEST(PlattenVolume, RundlaufExtractInsertBehaeltAngaben) {
    TempPlatte t("pv_rund.img");
    const std::string host = k1520test::tempPath("pv_rund_host");
    const std::string host2 = k1520test::tempPath("pv_rund_host2");
    fs::remove_all(host); fs::remove_all(host2);
    const auto inhalt = muster(5000, 3);
    {
        auto v = oeffne(t.pfad);
        ASSERT_TRUE(v);
        ASSERT_TRUE(v->makeDirectory("md2/usr/lib")) << v->lastError();
        WriteOptions w;
        w.wega_mode = 0751; w.wega_mode_gesetzt = true;
        w.wega_uid = 7; w.wega_gid = 8; w.wega_owner_gesetzt = true;
        w.wega_mtime = 1000000000;
        ASSERT_TRUE(v->write("md2/usr/lib/prog", inhalt, w)) << v->lastError();
        UnixAttrs a; a.set_mode = true; a.mode = 0700; a.set_uid = a.set_gid = true; a.uid = 5; a.gid = 6;
        a.set_mtime = true; a.mtime = 1100000000;
        ASSERT_TRUE(v->setAttributes("md2/usr/lib", a));
        ASSERT_TRUE(v->extract("md2", host)) << v->lastError();
        ASSERT_TRUE(v->flush());
    }
    ASSERT_TRUE(fs::exists(fs::path(host) / "md2/usr/lib/prog"));
    ASSERT_TRUE(fs::exists(fs::path(host) / "md2/usr/lib/prog.fileinfo"));
    ASSERT_TRUE(fs::exists(fs::path(host) / "md2/usr/lib.fileinfo"));
    k1520::fileinfo::FileAngaben fa;
    ASSERT_TRUE(k1520::fileinfo::leseFileinfo(fs::path(host) / "md2/usr/lib/prog.fileinfo", fa));
    EXPECT_TRUE(fa.wega);
    EXPECT_EQ(fa.x.mode & 07777, 0751);
    EXPECT_EQ(fa.x.uid, 7);

    // zurueck auf eine ZWEITE, frische Platte
    TempPlatte t2("pv_rund2.img");
    {
        auto v = oeffne(t2.pfad);
        ASSERT_TRUE(v);
        int n = 0;
        ASSERT_TRUE(v->insert((fs::path(host) / "md2/usr").string(), "md2/usr", false,
                              [&n](const std::string&, const std::string&) { ++n; }))
            << v->lastError();
        EXPECT_EQ(n, 1);
        ASSERT_TRUE(v->flush());
    }
    auto w = oeffne(t2.pfad, true);
    std::vector<uint8_t> z;
    ASSERT_TRUE(w->read("md2/usr/lib/prog", z));
    EXPECT_EQ(z, inhalt);
    const auto l = w->list();
    const FileEntry* e = finde(l, "md2/usr/lib/prog");
    ASSERT_TRUE(e);
    EXPECT_EQ(e->unix_mode & 07777, 0751);
    EXPECT_EQ(e->unix_uid, 7);
    EXPECT_EQ(e->unix_gid, 8);
    EXPECT_EQ(e->unix_mtime, 1000000000u);
    const FileEntry* d = finde(l, "md2/usr/lib");
    ASSERT_TRUE(d);
    EXPECT_EQ(d->unix_mode & 07777, 0700);              // Verzeichnisrechte zuletzt gesetzt
    EXPECT_EQ(d->unix_uid, 5);
    EXPECT_EQ(d->unix_mtime, 1100000000u);
    EXPECT_TRUE(w->check(FsCheckLevel::Voll).ohneBefund());
    fs::remove_all(host);
}

TEST(PlattenVolume, EinfuegenOhneFileinfoNimmtVorgaben) {
    TempPlatte t("pv_vorg.img");
    const std::string q = k1520test::tempPath("pv_vorg_quelle.bin");
    const auto inhalt = muster(300, 11);
    schreibe(q, inhalt);
    {
        auto v = oeffne(t.pfad);
        ASSERT_TRUE(v->insert(q, "md2/bin/neu")) << v->lastError();
        EXPECT_FALSE(v->insert(q, "md2/bin/neu")) << "ohne overwrite darf nichts ersetzt werden";
        EXPECT_TRUE(v->insert(q, "md2/bin/neu", true)) << v->lastError();
        EXPECT_FALSE(v->insert(q, "md2")) << "Datei braucht einen Zielnamen";
        ASSERT_TRUE(v->flush());
    }
    auto w = oeffne(t.pfad, true);
    const FileEntry* e = nullptr;
    const auto l = w->list();
    e = finde(l, "md2/bin/neu");
    ASSERT_TRUE(e);
    EXPECT_EQ(e->unix_mode & 07777, 0644);              // Windows: immer 0644; Linux: Quelle nicht ausfuehrbar
    EXPECT_EQ(e->unix_uid, 0);
    EXPECT_EQ(e->unix_gid, 0);
    EXPECT_EQ(e->size, inhalt.size());
    std::error_code ec;
    fs::remove(q, ec);
}

TEST(PlattenVolume, GepacktesAbbildBleibtGepacktUndHatSicherung) {
    TempPlatte t("pv_gz.img.gz", /*gz=*/true);
    {
        auto v = oeffne(t.pfad);
        ASSERT_TRUE(v);
        ASSERT_TRUE(v->write("md0/gz", {'g', 'z'}, {})) << v->lastError();
        ASSERT_TRUE(v->flush()) << v->lastError();
    }
    EXPECT_EQ(k1520::gzip::artVon(t.pfad), k1520::gzip::Art::Gzip);
    EXPECT_EQ(k1520::gzip::artVon(t.pfad + "~"), k1520::gzip::Art::Gzip);
    auto w = oeffne(t.pfad, true);
    std::vector<uint8_t> z;
    ASSERT_TRUE(w->read("md0/gz", z));
    EXPECT_EQ(z, (std::vector<uint8_t>{'g', 'z'}));
}
