/**
 * @file test_gide_platte.cpp
 * @brief Waechter fuer @ref GidePlatte - die GIDE-Platte im DiskTool (Layouts Poppe und Kingstener).
 *
 * Alles synthetisch (temporaere Abbilder, `.img.gz` fuer die grossen); das eingecheckte Golden-Fixture
 * (vom Poppe-Treiber im Emulator erzeugt) prueft `GidePlatteGolden.*`.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/filesystem/gide/gide_platte.h"
#include "core/util/gzip_datei.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;
namespace gl = gide_layout;

namespace {

std::string tmp(const std::string& n) { return k1520test::tempPath(n); }

void aufraeumen(const std::string& p) {
    std::error_code ec;
    fs::remove(p, ec);
    fs::remove(p + ".lock", ec);
    fs::remove(p + "~", ec);
}

std::vector<uint8_t> muster(size_t n, uint8_t start) {
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(start + i * 7 + (i >> 8));
    return v;
}

/// Beginnt @p ist mit @p soll?  (Dateien werden auf ganze 128-B-Saetze aufgefuellt.)
bool praefix(const std::vector<uint8_t>& soll, const std::vector<uint8_t>& ist) {
    return ist.size() >= soll.size() && std::equal(soll.begin(), soll.end(), ist.begin());
}

/// Platte anlegen + oeffnen (schreibbar, Layout ausdruecklich) + @p lw formatieren.
std::unique_ptr<GidePlatte> neu(const std::string& pfad, GideLayout l, std::initializer_list<int> lws) {
    std::string err;
    EXPECT_TRUE(GidePlatte::anlegen(pfad, l, 0, err)) << err;
    auto p = GidePlatte::open(pfad, err, false, l);
    EXPECT_TRUE(p) << err;
    if (!p) return nullptr;
    for (int lw : lws) EXPECT_TRUE(p->formatLaufwerk(lw)) << p->lastError();
    return p;
}

}  // namespace

// --- Abbildung CHS <-> LBA (nach GIDEHP.ASM / gidn5004.mac) -------------------------------------------------------

TEST(GideLayoutAbbildung, PoppeLbaFormel) {
    EXPECT_EQ(gl::poppeLba(0, 0, 0), 0u);
    EXPECT_EQ(gl::poppeLba(1, 0, 0), 63u);                       // Kopf = Laufwerk
    EXPECT_EQ(gl::poppeLba(0, 1, 0), 16u * 63);                  // naechste Spur = naechster Zylinder
    EXPECT_EQ(gl::poppeLba(7, 499, 31), (499u * 16 + 7) * 63 + 31);
    EXPECT_LT(gl::poppeLba(7, 499, 31), gl::kPoppeMindestSektoren);
    // tfsetup: Sektornummer = Hostsektor + 1, Zylinder = Spur, Kopf = Laufwerk
    const auto c = gl::poppeChs(3, 123, 5);
    EXPECT_EQ(c.zylinder, 123u); EXPECT_EQ(c.kopf, 3u); EXPECT_EQ(c.sektor, 6u);
}

TEST(GideLayoutAbbildung, PoppeUmkehrungUndRandfaelle) {
    unsigned lw, sp, hs;
    ASSERT_TRUE(gl::poppeVonLba(gl::poppeLba(5, 77, 31), lw, sp, hs));
    EXPECT_EQ(lw, 5u); EXPECT_EQ(sp, 77u); EXPECT_EQ(hs, 31u);
    EXPECT_FALSE(gl::poppeVonLba(gl::poppeLba(0, 0, 0) + 32, lw, sp, hs));   // Hostsektor 32..62 gehoert keinem Laufwerk
    EXPECT_FALSE(gl::poppeVonLba(63u * 8, lw, sp, hs));                      // Kopf 8..15 unbenutzt
    EXPECT_FALSE(gl::poppeVonLba(gl::poppeLba(0, 500, 0), lw, sp, hs));      // Spur 500 gibt es nicht
}

TEST(GideLayoutAbbildung, KingstenerLbaIstDerLineareSatz) {
    EXPECT_EQ(gl::kingLba(0, 0), 0u);
    EXPECT_EQ(gl::kingLba(1, 0), 64u);
    EXPECT_EQ(gl::kingLba(1022, 63), 65471u);
    unsigned sp, sa;
    ASSERT_TRUE(gl::kingVonLba(65471, sp, sa));
    EXPECT_EQ(sp, 1022u); EXPECT_EQ(sa, 63u);
    EXPECT_FALSE(gl::kingVonLba(65472, sp, sa));
    // HdPrep: Kopf = Spur und 3, Zylinder = Spur / 4, Sektor = Satz + 1  -> LBA = (Zyl*4+Kopf)*64 + Sektor-1
    const auto c = gl::kingChs(1022, 63);
    EXPECT_EQ(c.kopf, 2u); EXPECT_EQ(c.zylinder, 255u); EXPECT_EQ(c.sektor, 64u);
    EXPECT_EQ((uint64_t(c.zylinder) * 4 + c.kopf) * 64 + c.sektor - 1, gl::kingLba(1022, 63));
}

TEST(GideLayoutName, Namen) {
    GideLayout l;
    EXPECT_TRUE(gideLayoutVon("Poppe", l)); EXPECT_EQ(l, GideLayout::Poppe);
    EXPECT_TRUE(gideLayoutVon("kingstener", l)); EXPECT_EQ(l, GideLayout::Kingstener);
    EXPECT_TRUE(gideLayoutVon("auto", l)); EXPECT_EQ(l, GideLayout::Auto);
    EXPECT_FALSE(gideLayoutVon("quark", l));
    EXPECT_STREQ(gideLayoutName(GideLayout::Poppe), "poppe");
}

TEST(GideVerzeichnis, Plausibilitaet) {
    std::vector<uint8_t> d(32 * 4, 0xE5);
    EXPECT_TRUE(GidePlatte::verzeichnisPlausibel(d.data(), 4, 1000));
    std::vector<uint8_t> nullen(32 * 4, 0);
    EXPECT_FALSE(GidePlatte::verzeichnisPlausibel(nullen.data(), 4, 1000)) << "Nullen sind kein Verzeichnis";
    std::vector<uint8_t> e(32, 0xE5);
    e[0] = 0; std::memcpy(&e[1], "TEST    COM", 11); e[12] = e[13] = e[14] = 0; e[15] = 3;
    for (int i = 16; i < 32; ++i) e[i] = 0;
    e[16] = 4;
    EXPECT_TRUE(GidePlatte::verzeichnisPlausibel(e.data(), 1, 1000));
    e[17] = 0x10;                                   // Blockzeiger 4100 > DSM
    EXPECT_FALSE(GidePlatte::verzeichnisPlausibel(e.data(), 1, 1000));
    e[17] = 0; e[0] = 0x20;                         // Nutzerbereich 32
    EXPECT_FALSE(GidePlatte::verzeichnisPlausibel(e.data(), 1, 1000));
}

// --- Rundlauf ------------------------------------------------------------------------------------------------------

TEST(GidePlatteRundlauf, PoppeGzipAnlegenFormatierenPutGetRmFsck) {
    const std::string pfad = tmp("gide_poppe.img.gz");
    aufraeumen(pfad);
    {
        auto p = neu(pfad, GideLayout::Poppe, {0, 1});
        ASSERT_TRUE(p);
        EXPECT_EQ(p->laufwerkAnzahl(), 8);
        const auto a = muster(20000, 1), b = muster(100, 9), c = muster(70000, 33);
        WriteOptions o;
        ASSERT_TRUE(p->write("lw0/GROSS.DAT", a, o)) << p->lastError();
        ASSERT_TRUE(p->write("lw1/KLEIN.COM", b, o)) << p->lastError();
        ASSERT_TRUE(p->write("lw0/3:ZWEI.BIN", c, o)) << p->lastError();
        EXPECT_FALSE(p->write("lw2/NIX.COM", b, o)) << "lw2 ist unformatiert";
        ASSERT_TRUE(p->flush()) << p->lastError();
    }
    {   // frisch oeffnen: Auto erkennt Poppe, die Dateien sind bitgleich da
        std::string err;
        auto p = GidePlatte::open(pfad, err, true);
        ASSERT_TRUE(p) << err;
        EXPECT_EQ(p->layout(), GideLayout::Poppe);
        EXPECT_TRUE(p->erkennung().sicher);
        std::vector<uint8_t> d;
        ASSERT_TRUE(p->read("lw0/GROSS.DAT", d)) << p->lastError();
        EXPECT_EQ(d.size(), 20096u);                    // auf ganze Saetze aufgefuellt (CP/M kennt keine Bytelaenge)
        EXPECT_TRUE(praefix(muster(20000, 1), d));
        ASSERT_TRUE(p->read("lw1/KLEIN.COM", d));
        EXPECT_TRUE(praefix(muster(100, 9), d));
        ASSERT_TRUE(p->read("lw0/3:ZWEI.BIN", d));
        EXPECT_TRUE(praefix(muster(70000, 33), d));
        EXPECT_EQ(p->list().size(), 3u);
        EXPECT_TRUE(p->check(FsCheckLevel::Voll).ohneBefund() ||
                    p->check(FsCheckLevel::Voll).hoechste() <= FsSeverity::Info)
            << p->check(FsCheckLevel::Voll).alsText();
    }
    {   // loeschen
        std::string err;
        auto p = GidePlatte::open(pfad, err, false);
        ASSERT_TRUE(p) << err;
        ASSERT_TRUE(p->erase("lw1/KLEIN.COM")) << p->lastError();
        ASSERT_TRUE(p->flush());
        EXPECT_EQ(p->list().size(), 2u);
    }
    {
        std::string err;
        auto p = GidePlatte::open(pfad, err, true);
        ASSERT_TRUE(p);
        EXPECT_EQ(p->list().size(), 2u);
        const auto fsck = p->check(FsCheckLevel::Voll);
        EXPECT_LE(int(fsck.hoechste()), int(FsSeverity::Info)) << fsck.alsText();
    }
    aufraeumen(pfad);
}

TEST(GidePlatteRundlauf, KingstenerRohVierfachesViertelUndSicherung) {
    const std::string pfad = tmp("gide_king.img");
    aufraeumen(pfad);
    {
        auto p = neu(pfad, GideLayout::Kingstener, {0});
        ASSERT_TRUE(p);
        EXPECT_EQ(p->laufwerkAnzahl(), 1);
        const auto a = muster(5000, 3);
        WriteOptions o;
        ASSERT_TRUE(p->write("lw0/DATEI.TXT", a, o)) << p->lastError();
        ASSERT_TRUE(p->write("KURZ.COM", muster(10, 1), o)) << "Kingstener: ein Laufwerk, Praefix darf fehlen";
        ASSERT_TRUE(p->flush()) << p->lastError();
        // Treiberverhalten WrIDE: derselbe 128-B-Satz VIERMAL im 512-B-Hostsektor
        const auto& img = p->abbild();
        for (uint64_t lba = 0; lba < 300; ++lba)
            for (int q = 1; q < 4; ++q)
                ASSERT_EQ(std::memcmp(&img[lba * 512], &img[lba * 512 + q * 128], 128), 0) << "Sektor " << lba;
        // der Verzeichnissatz 0 liegt im LETZTEN Viertel (RdIDE)
        EXPECT_EQ(img[384 + 0], 0x00) << "erster Eintrag: Nutzer 0";
        EXPECT_EQ(std::memcmp(&img[384 + 1], "DATEI   TXT", 11), 0);
    }
    {
        std::string err;
        auto p = GidePlatte::open(pfad, err, true);
        ASSERT_TRUE(p) << err;
        EXPECT_EQ(p->layout(), GideLayout::Kingstener);
        std::vector<uint8_t> d;
        ASSERT_TRUE(p->read("lw0/DATEI.TXT", d));
        EXPECT_TRUE(praefix(muster(5000, 3), d));
        EXPECT_TRUE(p->check(FsCheckLevel::Voll).findings.empty() || p->check(FsCheckLevel::Voll).hoechste() <= FsSeverity::Info);
    }
    aufraeumen(pfad);
}

TEST(GidePlatteRundlauf, KingstenerErkenntFremdesViertelAlsWarnung) {
    const std::string pfad = tmp("gide_king_v.img");
    aufraeumen(pfad);
    { auto p = neu(pfad, GideLayout::Kingstener, {0}); ASSERT_TRUE(p); ASSERT_TRUE(p->flush()); }
    {   // ein Hostsektor mit ungleichen Vierteln im Datenbereich
        std::fstream f(pfad, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(40000LL * 512 + 5);
        const char x = 0x55;
        f.write(&x, 1);
    }
    std::string err;
    auto p = GidePlatte::open(pfad, err, true);
    ASSERT_TRUE(p) << err;
    const auto voll = p->check(FsCheckLevel::Voll);
    bool gefunden = false;
    for (const auto& f : voll.findings) gefunden |= f.id == "gide.kingstener.viertel";
    EXPECT_TRUE(gefunden) << voll.alsText();
    aufraeumen(pfad);
}

// --- Erkennung -----------------------------------------------------------------------------------------------------

TEST(GideErkennung, UnformatiertIstUnbekanntAutoOeffnetNicht) {
    const std::string pfad = tmp("gide_leer.img");
    aufraeumen(pfad);
    std::string err;
    ASSERT_TRUE(GidePlatte::anlegen(pfad, GideLayout::Poppe, 0, err)) << err;
    EXPECT_FALSE(GidePlatte::open(pfad, err, true));
    EXPECT_NE(err.find("--gide-layout"), std::string::npos) << err;
    auto p = GidePlatte::open(pfad, err, true, GideLayout::Poppe);        // ausdruecklich: ja
    ASSERT_TRUE(p) << err;
    EXPECT_FALSE(p->laufwerkInfo(0).formatiert);
    EXPECT_TRUE(p->list().empty());
    aufraeumen(pfad);
}

TEST(GideErkennung, MehrdeutigSchreibtNichtOhneAusdruecklicheWahl) {
    const std::string pfad = tmp("gide_mehr.img");
    aufraeumen(pfad);
    std::string err;
    ASSERT_TRUE(GidePlatte::anlegen(pfad, GideLayout::Poppe, 0, err)) << err;
    {   // leeres Verzeichnis nach BEIDEN Layouts: Poppe-Laufwerk 0 (ab Spur 2) und Kingstener (Satz 0..255)
        std::fstream f(pfad, std::ios::in | std::ios::out | std::ios::binary);
        const std::vector<uint8_t> e5(64 * 512, 0xE5);
        for (int i = 0; i < 4; ++i) { f.seekp(i * 64 * 512); f.write(reinterpret_cast<const char*>(e5.data()), long(e5.size())); }
        for (unsigned t = 2; t < 4; ++t) {       // die beiden Verzeichnisspuren von Laufwerk 0 (je 32 Hostsektoren)
            f.seekp(std::streamoff(gl::poppeLba(0, t, 0) * 512));
            f.write(reinterpret_cast<const char*>(e5.data()), 32 * 512);
        }
    }
    EXPECT_FALSE(GidePlatte::open(pfad, err, true)) << "Auto raet nicht";
    EXPECT_NE(err.find("nicht eindeutig"), std::string::npos) << err;
    auto p = GidePlatte::open(pfad, err, false, GideLayout::Kingstener);
    ASSERT_TRUE(p) << err;
    EXPECT_TRUE(p->erkennung().mehrdeutig);
    EXPECT_FALSE(p->erkennung().sicher);
    const auto r = p->check(FsCheckLevel::Schnell);
    bool w = false;
    for (const auto& f : r.findings) w |= f.id == "gide.layout.mehrdeutig";
    EXPECT_TRUE(w);
    aufraeumen(pfad);
}

TEST(GideErkennung, Poppe_Platte_Als_Kingstener_Gewaehlt_Warnt_Und_Formatiert_Nicht) {
    const std::string pfad = tmp("gide_falsch.img");
    aufraeumen(pfad);
    { auto p = neu(pfad, GideLayout::Poppe, {0}); ASSERT_TRUE(p); WriteOptions o; ASSERT_TRUE(p->write("lw0/A.COM", muster(300, 1), o)); ASSERT_TRUE(p->flush()); }
    std::string err;
    auto k = GidePlatte::open(pfad, err, false, GideLayout::Kingstener);
    ASSERT_TRUE(k) << err;
    EXPECT_NE(k->erkennung().text.find("anderen Layout"), std::string::npos) << k->erkennung().text;
    EXPECT_FALSE(k->formatLaufwerk(0)) << "Datenbereich der anderen Aufteilung darf nicht ueberschrieben werden";
    EXPECT_FALSE(k->write("lw0/X.COM", muster(10, 1), WriteOptions{})) << "unformatiert (im gewaehlten Layout)";
    EXPECT_FALSE(k->dirty());
    aufraeumen(pfad);
}

TEST(GideErkennung, FormatierenVerweigertBeiDateienOhneErzwingen) {
    const std::string pfad = tmp("gide_fmt.img");
    aufraeumen(pfad);
    auto p = neu(pfad, GideLayout::Poppe, {0});
    ASSERT_TRUE(p);
    ASSERT_TRUE(p->write("lw0/A.COM", muster(300, 1), WriteOptions{}));
    ASSERT_TRUE(p->flush());
    std::string err;
    p.reset();
    p = GidePlatte::open(pfad, err, false, GideLayout::Poppe);
    ASSERT_TRUE(p) << err;
    EXPECT_FALSE(p->formatLaufwerk(0));
    EXPECT_TRUE(p->formatLaufwerk(0, true)) << p->lastError();
    EXPECT_TRUE(p->list().empty());
    aufraeumen(pfad);
}

// --- Sperre / Schreibschutz ----------------------------------------------------------------------------------------

TEST(GidePlatteSperre, FremdeSperreVerbietetSchreibenLesenGeht) {
    const std::string pfad = tmp("gide_lock.img");
    aufraeumen(pfad);
    { auto p = neu(pfad, GideLayout::Poppe, {0}); ASSERT_TRUE(p); ASSERT_TRUE(p->flush()); }
    // das Werkzeug lebt: wer sonst haelt die Sperre?  Wir tun so, als waere es der Emulator (eigene PID lebt).
    { std::ofstream l(pfad + ".lock"); l << k1520::os::processId() << "\n"; }
    std::string err;
    EXPECT_FALSE(GidePlatte::open(pfad, err, false)) << "schreibbar geht nicht";
    EXPECT_NE(err.find("gesperrt"), std::string::npos) << err;
    auto p = GidePlatte::open(pfad, err, true);
    ASSERT_TRUE(p) << err;
    EXPECT_TRUE(p->fremdGesperrt());
    EXPECT_TRUE(p->list().empty());
    EXPECT_FALSE(p->setReadOnly(false));
    EXPECT_NE(p->lastError().find("gesperrt"), std::string::npos);
    EXPECT_FALSE(p->write("lw0/A.COM", muster(10, 1), WriteOptions{}));
    aufraeumen(pfad);
}

TEST(GidePlatteSperre, VerwaisteSperreWirdUebernommenUndGeloest) {
    const std::string pfad = tmp("gide_lock2.img");
    aufraeumen(pfad);
    { auto p = neu(pfad, GideLayout::Poppe, {0}); ASSERT_TRUE(p); ASSERT_TRUE(p->flush()); }
    { std::ofstream l(pfad + ".lock"); l << "2147483646\n"; }      // gibt es nicht
    std::string err;
    {
        auto p = GidePlatte::open(pfad, err, false);
        ASSERT_TRUE(p) << err;
        EXPECT_TRUE(fs::exists(pfad + ".lock"));
        std::string e2;
        EXPECT_FALSE(GidePlatte::open(pfad, e2, false)) << "zweites schreibbares Oeffnen scheitert";
    }
    EXPECT_FALSE(fs::exists(pfad + ".lock")) << "die Sperre verschwindet mit dem Schliessen";
    aufraeumen(pfad);
}

TEST(GidePlatteSperre, SchreibschutzIstVorgabe) {
    const std::string pfad = tmp("gide_ro.img");
    aufraeumen(pfad);
    { auto p = neu(pfad, GideLayout::Poppe, {0}); ASSERT_TRUE(p); ASSERT_TRUE(p->flush()); }
    std::string err;
    auto p = GidePlatte::open(pfad, err);
    ASSERT_TRUE(p);
    EXPECT_TRUE(p->readOnly());
    EXPECT_FALSE(p->write("lw0/A.COM", muster(10, 1), WriteOptions{}));
    EXPECT_NE(p->lastError().find("schreibgeschuetzt"), std::string::npos);
    EXPECT_TRUE(p->setReadOnly(false));
    EXPECT_TRUE(p->write("lw0/A.COM", muster(10, 1), WriteOptions{}));
    aufraeumen(pfad);
}

// --- Golden-Fixture (vom Poppe-Treiber im Emulator geschrieben, siehe tests/fixtures/README.md) ---------------------

TEST(GidePlatteGolden, PoppeTreiberAbbildWirdGelesen) {
    const std::string fx = std::string(K1520_FIXTURE_DIR) + "/gide/poppe_cpa.img.gz";
    if (!fs::exists(fx)) GTEST_SKIP() << "Fixture fehlt: " << fx;
    std::string err;
    // mit einer Kopie arbeiten (nie ein eingechecktes Abbild direkt oeffnen)
    const std::string kopie = tmp("gide_golden.img.gz");
    aufraeumen(kopie);
    fs::copy_file(fx, kopie);
    auto p = GidePlatte::open(kopie, err, true);
    ASSERT_TRUE(p) << err;
    EXPECT_EQ(p->layout(), GideLayout::Poppe);
    EXPECT_TRUE(p->erkennung().sicher);
    std::vector<uint8_t> d;
    ASSERT_TRUE(p->read("lw1/HALLO.TXT", d)) << p->lastError();
    const std::string soll = "Hallo von Poppe, geschrieben im Emulator.\r\n";
    ASSERT_GE(d.size(), soll.size());
    EXPECT_EQ(std::string(d.begin(), d.begin() + soll.size()), soll);
    // lw0: 20000 Byte Muster (i*7 + (i>>8)), mehrere 8-KB-Bloecke und Extents
    ASSERT_TRUE(p->read("lw0/NUTZ.BIN", d)) << p->lastError();
    ASSERT_EQ(d.size(), 20096u);
    for (size_t i = 0; i < 20000; ++i) ASSERT_EQ(d[i], uint8_t((i * 7 + (i >> 8)) & 0xFF)) << "Byte " << i;
    EXPECT_EQ(p->list().size(), 2u);
    EXPECT_FALSE(p->laufwerkInfo(2).formatiert);
    EXPECT_LE(int(p->check(FsCheckLevel::Voll).hoechste()), int(FsSeverity::Info));
    aufraeumen(kopie);
}
