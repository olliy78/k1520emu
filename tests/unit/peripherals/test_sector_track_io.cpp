/**
 * @file test_sector_track_io.cpp
 * @brief GoogleTests fuer SectorTrackIo (Sektorlisten ⇄ DiskMedium, Entwurf 38 AP A0).
 *
 * Schwerpunkte: Rundlauf Liste → Medium → Liste (FM und MFM), Interleave, gemischte
 * Sektorgroessen, geloeschte/fehlerhafte/fehlende Datenfelder, Ablehnungen, und der
 * Befund, was DiskMedium::rawCompatible dazu sagt.
 *
 * @see core/peripherals/floppy_drive/sector_track_io.h
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "core/peripherals/floppy_drive/image_codec.h"
#include "core/peripherals/floppy_drive/sector_track_io.h"

using namespace SectorTrackIo;

namespace {

SectorRecord sek(uint8_t c, uint8_t h, uint8_t r, uint8_t n, uint8_t fill) {
    SectorRecord s;
    s.c = c; s.h = h; s.r = r; s.n = n;
    s.data.assign(s.size(), fill);
    return s;
}

TrackRecord spur(uint8_t cyl, uint8_t head, Encoding enc, std::vector<SectorRecord> secs) {
    TrackRecord t;
    t.cyl = cyl; t.head = head; t.enc = enc;
    t.rate_kbps = enc == Encoding::FM ? 250 : 250;
    t.sectors = std::move(secs);
    return t;
}

}  // namespace

class SectorTrackIoRund : public ::testing::TestWithParam<Encoding> {};

TEST_P(SectorTrackIoRund, InterleaveGemischteGroessenUndFlags) {
    const Encoding enc = GetParam();
    std::vector<SectorRecord> s0;
    for (uint8_t r : {1, 4, 2, 5, 3}) s0.push_back(sek(0, 0, r, 0, r));
    SectorRecord gel = sek(0, 0, 6, 1, 0x66); gel.deleted = true;
    SectorRecord dfe = sek(0, 0, 7, 0, 0x77); dfe.data_error = true;
    SectorRecord ife = sek(0, 0, 8, 0, 0x88); ife.id_error = true;
    SectorRecord nod = sek(0, 0, 9, 0, 0);    nod.no_data = true; nod.data.clear();
    s0.push_back(gel); s0.push_back(dfe); s0.push_back(ife); s0.push_back(nod);

    std::vector<TrackRecord> in;
    in.push_back(spur(0, 0, enc, s0));
    in.push_back(spur(0, 1, enc, {sek(0, 1, 1, 2, 0xC1), sek(0, 1, 2, 2, 0xC2)}));
    in.push_back(spur(2, 0, enc, {sek(2, 0, 1, 0, 0xD1)}));   // Spur 1 bleibt unformatiert

    DiskMedium m;
    std::string err;
    ASSERT_TRUE(toMedium(in, m, err)) << err;
    EXPECT_EQ(m.numCylinders(), 3);
    EXPECT_EQ(m.numHeads(), 2);
    EXPECT_FALSE(m.dirty());
    EXPECT_TRUE(m.peek(1, 0).empty());

    std::vector<TrackRecord> out;
    ASSERT_TRUE(fromMedium(m, out, err)) << err;
    ASSERT_EQ(out.size(), 3u);   // leere Spuren entfallen
    for (size_t i = 0; i < in.size(); ++i) {
        ASSERT_EQ(out[i].sectors.size(), in[i].sectors.size()) << i;
        EXPECT_EQ(out[i].cyl, in[i].cyl);
        EXPECT_EQ(out[i].head, in[i].head);
        EXPECT_EQ(out[i].enc, enc);
        for (size_t k = 0; k < in[i].sectors.size(); ++k) {
            const auto& a = in[i].sectors[k];
            const auto& b = out[i].sectors[k];
            EXPECT_EQ(b.r, a.r); EXPECT_EQ(b.n, a.n); EXPECT_EQ(b.c, a.c); EXPECT_EQ(b.h, a.h);
            EXPECT_EQ(b.deleted, a.deleted) << "r" << int(a.r);
            EXPECT_EQ(b.data_error, a.data_error) << "r" << int(a.r);
            EXPECT_EQ(b.id_error, a.id_error) << "r" << int(a.r);
            EXPECT_EQ(b.no_data, a.no_data) << "r" << int(a.r);
            EXPECT_EQ(b.data, a.data) << "r" << int(a.r);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(FmMfm, SectorTrackIoRund,
                         ::testing::Values(Encoding::FM, Encoding::MFM));

TEST(SectorTrackIo, ToMediumLehntFalscheDatenlaengeAb) {
    SectorRecord s = sek(0, 0, 1, 1, 0xAA);
    s.data.resize(100);
    DiskMedium m;
    std::string err;
    EXPECT_FALSE(toMedium({spur(0, 0, Encoding::MFM, {s})}, m, err));
    EXPECT_NE(err.find("100"), std::string::npos);
    EXPECT_EQ(m.numCylinders(), 0);   // Ausgabe unberuehrt
}

TEST(SectorTrackIo, ToMediumLehntLeereListeUndCodeUeber6Ab) {
    DiskMedium m;
    std::string err;
    EXPECT_FALSE(toMedium({}, m, err));
    SectorRecord s; s.n = 7; s.data.assign(16384, 0);
    EXPECT_FALSE(toMedium({spur(0, 0, Encoding::MFM, {s})}, m, err));
}

TEST(SectorTrackIo, GroessencodesBis6GehenDurchsMedium) {
    for (uint8_t n = 4; n <= 6; ++n) {
        SectorRecord s;
        s.c = 0; s.h = 0; s.r = 1; s.n = n;
        s.data.resize(128u << n);
        for (size_t i = 0; i < s.data.size(); ++i) s.data[i] = static_cast<uint8_t>(i * 7 + n);
        DiskMedium m;
        std::string err;
        ASSERT_TRUE(toMedium({spur(0, 0, Encoding::MFM, {s})}, m, err)) << err;
        std::vector<TrackRecord> back;
        ASSERT_TRUE(fromMedium(m, back, err)) << err;
        ASSERT_EQ(back.size(), 1u);
        ASSERT_EQ(back[0].sectors.size(), 1u);
        EXPECT_EQ(back[0].sectors[0].n, n);
        EXPECT_EQ(back[0].sectors[0].data, s.data);
        EXPECT_FALSE(back[0].sectors[0].data_error);
    }
}

TEST(SectorTrackIo, FromMediumLehntUnformatiertAb) {
    DiskMedium m(2, 1, Encoding::MFM);
    std::vector<TrackRecord> out;
    std::string err;
    EXPECT_FALSE(fromMedium(m, out, err));
    EXPECT_EQ(err, "unformatiert");
}

TEST(SectorTrackIo, NachspannMitNutzdatenIstNichtDarstellbar) {
    // UDOS-Sektorkontrollblock: Daten hinter der Daten-CRC gehen in keinem Container mit.
    std::vector<LogicalSector> v;
    LogicalSector l; l.cyl = 0; l.head = 0; l.id = 1; l.size = 128; l.data.assign(128, 0x5A);
    l.tail = {0x01, 0x02, 0x03, 0x04};
    v.push_back(l);
    DiskMedium m(1, 1, Encoding::MFM);
    m.setTrack(0, 0, TrackCodec::buildTrack(v, Encoding::MFM));
    std::string why;
    EXPECT_FALSE(representable(m, why));
    EXPECT_NE(why.find("Spur 0/0"), std::string::npos);
    std::vector<TrackRecord> out;
    EXPECT_FALSE(fromMedium(m, out, why));
}

// Befund zu rawCompatible (siehe Kopf von sector_track_io.h): CRC-Fehler und fehlende
// Datenfelder sperren es, geloeschte Marken und gemischte Groessen nicht — fuer
// .td0/.imd sind alle vier darstellbar.
TEST(SectorTrackIo, BefundRawCompatibleGegenDarstellbar) {
    auto mediumMit = [](auto aendere) {
        std::vector<SectorRecord> s{sek(0, 0, 1, 0, 1), sek(0, 0, 2, 1, 2)};   // gemischt 128/256
        aendere(s);
        DiskMedium m;
        std::string err;
        EXPECT_TRUE(toMedium({spur(0, 0, Encoding::MFM, s)}, m, err)) << err;
        return m;
    };
    std::string why;
    {   // gemischte Groessen: raw ja
        DiskMedium m = mediumMit([](auto&) {});
        EXPECT_TRUE(m.rawCompatible());
        EXPECT_TRUE(representable(m, why));
    }
    {   // geloescht: raw ja (verliert die Marke still!), darstellbar ja
        DiskMedium m = mediumMit([](auto& s) { s[0].deleted = true; });
        EXPECT_TRUE(m.rawCompatible());
        EXPECT_TRUE(representable(m, why));
    }
    {   // Daten-CRC-Fehler: raw nein, darstellbar ja
        DiskMedium m = mediumMit([](auto& s) { s[0].data_error = true; });
        EXPECT_FALSE(m.rawCompatible());
        EXPECT_TRUE(representable(m, why)) << why;
    }
    {   // ID-CRC-Fehler: raw nein, darstellbar ja
        DiskMedium m = mediumMit([](auto& s) { s[1].id_error = true; });
        EXPECT_FALSE(m.rawCompatible());
        EXPECT_TRUE(representable(m, why)) << why;
    }
    {   // ohne Datenfeld: raw nein, darstellbar ja
        DiskMedium m = mediumMit([](auto& s) { s[1].no_data = true; s[1].data.clear(); });
        EXPECT_FALSE(m.rawCompatible());
        EXPECT_TRUE(representable(m, why)) << why;
    }
}

TEST(SectorTrackIo, RateWirdAusVerfahrenUndSpurlaengeGeraten) {
    EXPECT_EQ(guessRate(Encoding::MFM, 6250), 250);
    EXPECT_EQ(guessRate(Encoding::MFM, 10416), 500);
    EXPECT_EQ(guessRate(Encoding::FM, 3125), 250);
    EXPECT_EQ(guessRate(Encoding::FM, 5208), 500);
}

TEST(SectorTrackIo, ContainerTypenTd0Imd) {
    EXPECT_STREQ(ImageCodec::name(ContainerType::Td0), "td0");
    EXPECT_STREQ(ImageCodec::name(ContainerType::Imd), "imd");
    EXPECT_STREQ(ImageCodec::extension(ContainerType::Td0), ".td0");
    EXPECT_STREQ(ImageCodec::extension(ContainerType::Imd), ".imd");
    EXPECT_TRUE(ImageCodec::selfDescribing(ContainerType::Td0));
    EXPECT_TRUE(ImageCodec::selfDescribing(ContainerType::Imd));
    EXPECT_FALSE(ImageCodec::needsDiskFormat(ContainerType::Td0));
    EXPECT_EQ(ImageCodec::fromExtension("x/A.TD0"), ContainerType::Td0);
    EXPECT_EQ(ImageCodec::fromExtension("a.imd"), ContainerType::Imd);
    DiskMedium m;
    std::string err;
    EXPECT_FALSE(ImageCodec::load("nicht_da.td0", ContainerType::Td0, nullptr, m, err));
    EXPECT_NE(err.find("nicht_da.td0"), std::string::npos);
    EXPECT_FALSE(ImageCodec::save("x.imd", ContainerType::Imd, nullptr, m, err));
}

TEST(SectorTrackIo, DetectErkenntKennungenAberKeinRohesImageMitTD) {
    namespace fs = std::filesystem;
    const fs::path d = fs::temp_directory_path() / "k1520_a0_detect";
    fs::create_directories(d);
    auto schreibe = [&](const char* name, const std::string& inhalt) {
        std::ofstream(d / name, std::ios::binary) << inhalt;
        return (d / name).string();
    };
    // IMD
    EXPECT_EQ(ImageCodec::detect(schreibe("a.dat", "IMD 1.18: 01/01/2026\r\n\x1a")), ContainerType::Imd);
    // roher Datentraeger, der zufaellig mit "TD" beginnt, ist KEIN TD0 (Kopf-CRC fehlt)
    EXPECT_EQ(ImageCodec::detect(schreibe("b.dat", std::string("TD") + std::string(600, 'x'))),
              ContainerType::Img);
    // TD0-Kopf mit gueltiger CRC (Werte: 'TD', 0,0,21, rate 0, typ 2, 0,0, 2 Seiten)
    unsigned char k[12] = {'T', 'D', 0, 0, 21, 0, 2, 0, 0, 2, 0, 0};
    uint16_t crc = 0;
    for (int i = 0; i < 10; ++i) {
        crc = static_cast<uint16_t>(crc ^ (k[i] << 8));
        for (int b = 0; b < 8; ++b)
            crc = static_cast<uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0xA097 : (crc << 1));
    }
    k[10] = crc & 0xFF; k[11] = crc >> 8;
    EXPECT_EQ(ImageCodec::detect(schreibe("c.dat", std::string(reinterpret_cast<char*>(k), 12))),
              ContainerType::Td0);
    fs::remove_all(d);
}
