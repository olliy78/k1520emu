/**
 * @file test_disk_image.cpp
 * @brief GoogleTests fuer DiskImage — Dateibindung, Autosave und „Speichern unter".
 *
 * Schwerpunkte:
 *   - createBlank(): echte Leerdiskette in Laufwerksgeometrie, ohne Datei
 *   - Autosave: geaenderte Spuren landen VERZOEGERT in der gebundenen Datei
 *   - saveAs(): Containerwechsel (.hfe ⇄ .dmk ⇄ .img) inklusive Umbinden —
 *     ab dann folgt der Autosave der NEUEN Datei
 *   - `.img` als Ziel wird abgelehnt, solange das Medium nicht darstellbar ist
 *   - Fuer `.hfe`/`.dmk` wird KEIN Diskettenformat verlangt (self-describing)
 *
 * @see core/peripherals/floppy_drive/disk_image.h
 * @see doc/design/09_floppy_drive.md §6
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

#include "core/peripherals/floppy_drive/disk_image.h"
#include "core/peripherals/floppy_drive/track_codec.h"
#include "tests/support/temp_path.h"

namespace {

std::string tmpPath(const char* name) {
    return k1520test::tempPath(name);
}

/// @brief 2 Zylinder × 1 Kopf × 2 × 128 B.
DiskFormat makeSimpleFormat() {
    DiskFormat fmt;
    fmt.name = "test_simple";
    fmt.tracks.push_back({0, 1, 0, 0, 2, 128});
    return fmt;
}

TrackImage makeTrack(uint8_t cyl, int nsec, uint16_t size, uint8_t fill) {
    std::vector<LogicalSector> secs;
    for (int i = 1; i <= nsec; ++i) {
        LogicalSector ls;
        ls.cyl  = cyl;
        ls.head = 0;
        ls.id   = static_cast<uint8_t>(i);
        ls.size = size;
        ls.data.assign(size, fill);
        secs.push_back(std::move(ls));
    }
    return TrackCodec::buildTrack(secs, Encoding::MFM);
}

}  // namespace

// ─── createBlank ─────────────────────────────────────────────────────────────

/**
 * @test DiskImageBlank/LiefertUnformatiertesMediumOhneDatei
 * @brief Der Kern der neuen Leerdiskette: Geometrie kommt vom Laufwerk, alle Spuren
 *        sind unformatiert, es gibt (noch) keine Datei.
 */
TEST(DiskImageBlank, LiefertUnformatiertesMediumOhneDatei) {
    auto img = DiskImage::createBlank(80, 2, Encoding::MFM);
    ASSERT_NE(img, nullptr);

    EXPECT_FALSE(img->hasFile());
    EXPECT_TRUE(img->path().empty());
    EXPECT_EQ(img->geometry().num_cyls,  80u);
    EXPECT_EQ(img->geometry().num_heads,  2u);
    EXPECT_FALSE(img->medium().formatted());
    EXPECT_TRUE(img->readTrack(0, 0).empty());
    EXPECT_FALSE(img->rawCompatible()) << "Leerdiskette darf nicht als .img gelten";
    EXPECT_TRUE(img->writable());

    // Ohne Bindung ist flush() ein No-op und darf nicht scheitern.
    EXPECT_TRUE(img->writeTrack(3, 1, makeTrack(3, 2, 128, 0xE5)));
    EXPECT_TRUE(img->flush());
}

TEST(DiskImageBlank, UnplausibleGeometrie_gibtNullptr) {
    EXPECT_EQ(DiskImage::createBlank(0, 2, Encoding::MFM), nullptr);
    EXPECT_EQ(DiskImage::createBlank(80, 0, Encoding::MFM), nullptr);
}

// ─── saveAs / Umbinden ───────────────────────────────────────────────────────

/**
 * @test DiskImageSaveAs/LeerdisketteNachHfeUndZurueck
 * @brief Eine Leerdiskette laesst sich als `.hfe` sichern; danach ist sie an diese
 *        Datei gebunden und der Autosave schreibt dorthin.
 */
TEST(DiskImageSaveAs, LeerdisketteNachHfeUndZurueck) {
    const auto path = tmpPath("k1520_test_di_blank.hfe");
    std::filesystem::remove(path);

    auto img = DiskImage::createBlank(40, 1, Encoding::MFM);
    ASSERT_NE(img, nullptr);
    ASSERT_TRUE(img->saveAs(path, std::nullopt)) << img->lastError();

    EXPECT_TRUE(img->hasFile());
    EXPECT_EQ(img->path(), path);
    EXPECT_EQ(img->container(), ContainerType::Hfe);
    EXPECT_TRUE(std::filesystem::exists(path));

    auto wieder = DiskImage::open(path, std::nullopt, false);
    ASSERT_NE(wieder, nullptr);
    EXPECT_EQ(wieder->geometry().num_cyls, 40u);
    EXPECT_FALSE(wieder->medium().formatted());

    std::filesystem::remove(path);
}

/**
 * @test DiskImageSaveAs/LehntImgFuerLeerdisketteAb
 * @brief „Ein Abspeichern dieses Abbildes als .img muss zu einer Fehlermeldung fuehren."
 */
TEST(DiskImageSaveAs, LehntImgFuerLeerdisketteAb) {
    const auto path = tmpPath("k1520_test_di_blank.img");
    std::filesystem::remove(path);

    auto img = DiskImage::createBlank(2, 1, Encoding::MFM);
    ASSERT_NE(img, nullptr);

    EXPECT_FALSE(img->saveAs(path, makeSimpleFormat()));
    EXPECT_NE(std::string(img->lastError()).find("unformatiert"), std::string::npos)
        << img->lastError();
    EXPECT_FALSE(std::filesystem::exists(path));
    EXPECT_FALSE(img->hasFile()) << "gescheitertes saveAs darf nicht umbinden";
}

/**
 * @test DiskImageSaveAs/ImgOhneFormat_wirdAbgelehnt
 * @brief Das Diskettenformat ist NUR fuer `.img` noetig — und dort Pflicht.
 */
TEST(DiskImageSaveAs, ImgOhneFormat_wirdAbgelehnt) {
    auto img = DiskImage::createBlank(2, 1, Encoding::MFM);
    ASSERT_NE(img, nullptr);
    img->writeTrack(0, 0, makeTrack(0, 2, 128, 0x11));
    img->writeTrack(1, 0, makeTrack(1, 2, 128, 0x22));
    ASSERT_TRUE(img->rawCompatible());

    EXPECT_FALSE(img->saveAs(tmpPath("k1520_test_di_nofmt.img"), std::nullopt));
    EXPECT_NE(std::string(img->lastError()).find("Diskettenformat"), std::string::npos)
        << img->lastError();
}

/**
 * @test DiskImageSaveAs/ContainerwechselHfeNachDmkNachImg
 * @brief Dieselbe Diskette durch alle drei Container — die Nutzdaten ueberleben, und
 *        die Bindung folgt jeweils der neuen Datei.
 */
TEST(DiskImageSaveAs, ContainerwechselHfeNachDmkNachImg) {
    const auto hfe = tmpPath("k1520_test_di_chain.hfe");
    const auto dmk = tmpPath("k1520_test_di_chain.dmk");
    const auto img_p = tmpPath("k1520_test_di_chain.img");
    for (const auto& p : {hfe, dmk, img_p}) std::filesystem::remove(p);

    auto img = DiskImage::createBlank(2, 1, Encoding::MFM);
    ASSERT_NE(img, nullptr);
    img->writeTrack(0, 0, makeTrack(0, 2, 128, 0xAA));
    img->writeTrack(1, 0, makeTrack(1, 2, 128, 0xBB));

    ASSERT_TRUE(img->saveAs(hfe, std::nullopt)) << img->lastError();
    EXPECT_EQ(img->container(), ContainerType::Hfe);

    ASSERT_TRUE(img->saveAs(dmk, std::nullopt)) << img->lastError();
    EXPECT_EQ(img->container(), ContainerType::Dmk);
    EXPECT_EQ(img->path(), dmk);

    ASSERT_TRUE(img->saveAs(img_p, makeSimpleFormat())) << img->lastError();
    EXPECT_EQ(img->container(), ContainerType::Img);
    ASSERT_NE(img->diskFormat(), nullptr);

    // Alle drei Dateien tragen dieselben Nutzdaten.
    struct Fall { std::string pfad; std::optional<DiskFormat> fmt; };
    const Fall faelle[] = {
        {hfe,   std::nullopt},
        {dmk,   std::nullopt},
        {img_p, makeSimpleFormat()},
    };
    for (const auto& f : faelle) {
        auto back = DiskImage::open(f.pfad, f.fmt, false);
        ASSERT_NE(back, nullptr) << f.pfad;
        auto s0 = TrackCodec::parseTrack(back->readTrack(0, 0));
        auto s1 = TrackCodec::parseTrack(back->readTrack(1, 0));
        ASSERT_EQ(s0.size(), 2u) << f.pfad;
        ASSERT_EQ(s1.size(), 2u) << f.pfad;
        EXPECT_EQ(s0[0].data[0], 0xAA) << f.pfad;
        EXPECT_EQ(s1[1].data[5], 0xBB) << f.pfad;
    }

    for (const auto& p : {hfe, dmk, img_p}) std::filesystem::remove(p);
}

// ─── Autosave ────────────────────────────────────────────────────────────────

/**
 * @test DiskImageAutoFlush/SchreibtErstNachDerRuhezeit
 * @brief Der Autosave fasst Schreibbursts zusammen: erst wenn die letzte Aenderung
 *        @ref kAutoFlushDelayCycles Takte zurueckliegt, geht sie in die Datei.
 */
TEST(DiskImageAutoFlush, SchreibtErstNachDerRuhezeit) {
    const auto path = tmpPath("k1520_test_di_autoflush.hfe");
    std::filesystem::remove(path);

    auto img = DiskImage::createBlank(2, 1, Encoding::MFM);
    ASSERT_NE(img, nullptr);
    img->writeTrack(0, 0, makeTrack(0, 2, 128, 0x11));
    ASSERT_TRUE(img->saveAs(path, std::nullopt)) << img->lastError();

    // Aenderung, die noch nicht geschrieben sein darf.
    img->writeTrack(1, 0, makeTrack(1, 2, 128, 0x77));
    EXPECT_FALSE(img->autoFlush(1000)) << "erste Pruefung merkt sich nur den Zeitpunkt";
    EXPECT_FALSE(img->autoFlush(1000 + kAutoFlushDelayCycles / 2));
    {
        auto zwischen = DiskImage::open(path, std::nullopt, false);
        ASSERT_NE(zwischen, nullptr);
        EXPECT_TRUE(zwischen->readTrack(1, 0).empty())
            << "Aenderung zu frueh in der Datei";
    }

    // Nach Ablauf der Ruhezeit wird geschrieben.
    EXPECT_TRUE(img->autoFlush(1000 + kAutoFlushDelayCycles + 1));
    EXPECT_FALSE(img->medium().dirty());
    {
        auto danach = DiskImage::open(path, std::nullopt, false);
        ASSERT_NE(danach, nullptr);
        auto secs = TrackCodec::parseTrack(danach->readTrack(1, 0));
        ASSERT_EQ(secs.size(), 2u);
        EXPECT_EQ(secs[0].data[0], 0x77);
    }

    // Ohne weitere Aenderung passiert nichts mehr.
    EXPECT_FALSE(img->autoFlush(1000 + 10 * kAutoFlushDelayCycles));

    std::filesystem::remove(path);
}

/**
 * @test DiskImageAutoFlush/DestruktorSchreibtAusstehendeAenderungen
 */
TEST(DiskImageAutoFlush, DestruktorSchreibtAusstehendeAenderungen) {
    const auto path = tmpPath("k1520_test_di_dtor.dmk");
    std::filesystem::remove(path);

    {
        auto img = DiskImage::createBlank(2, 1, Encoding::MFM);
        ASSERT_NE(img, nullptr);
        img->writeTrack(0, 0, makeTrack(0, 2, 128, 0x33));
        ASSERT_TRUE(img->saveAs(path, std::nullopt)) << img->lastError();
        img->writeTrack(1, 0, makeTrack(1, 2, 128, 0x44));
        // KEIN flush() — der Destruktor muss es erledigen.
    }

    auto back = DiskImage::open(path, std::nullopt, false);
    ASSERT_NE(back, nullptr);
    auto secs = TrackCodec::parseTrack(back->readTrack(1, 0));
    ASSERT_EQ(secs.size(), 2u);
    EXPECT_EQ(secs[0].data[0], 0x44);

    std::filesystem::remove(path);
}

/**
 * @test DiskImageAutoFlush/SchreibgeschuetzteDisketteBleibtUnberuehrt
 */
TEST(DiskImageAutoFlush, SchreibgeschuetzteDisketteBleibtUnberuehrt) {
    const auto path = tmpPath("k1520_test_di_wp.hfe");
    std::filesystem::remove(path);

    auto img = DiskImage::createBlank(2, 1, Encoding::MFM);
    ASSERT_NE(img, nullptr);
    img->writeTrack(0, 0, makeTrack(0, 2, 128, 0x55));
    ASSERT_TRUE(img->saveAs(path, std::nullopt)) << img->lastError();

    img->setWriteProtect(true);
    EXPECT_FALSE(img->writeTrack(1, 0, makeTrack(1, 2, 128, 0x66)));
    EXPECT_FALSE(img->medium().dirty());

    auto back = DiskImage::open(path, std::nullopt, false);
    ASSERT_NE(back, nullptr);
    EXPECT_TRUE(back->readTrack(1, 0).empty());

    std::filesystem::remove(path);
}

// ─── TeleDisk (.td0) und ImageDisk (.imd) als gebundene Container (AP K1, #9) ──

namespace {

/// 2 Zylinder x 1 Kopf mit je 2 Sektoren zu 128 B (0xAA / 0xBB), als .hfe-Quelle.
std::unique_ptr<DiskImage> kleineDiskette() {
    auto img = DiskImage::createBlank(2, 1, Encoding::MFM);
    img->writeTrack(0, 0, makeTrack(0, 2, 128, 0xAA));
    img->writeTrack(1, 0, makeTrack(1, 2, 128, 0xBB));
    return img;
}

const char* const kSektorContainer[] = {".td0", ".imd"};

}  // namespace

/// @test saveAs in td0/imd, zurueck nach hfe: Nutzdaten bleiben, Bindung folgt, Info-Zugriff gesetzt
TEST(DiskImageTd0Imd, ContainerwechselHfeNachTd0NachImdNachHfe) {
    const auto td0 = tmpPath("k1520_test_di_k1.td0");
    const auto imd = tmpPath("k1520_test_di_k1.imd");
    const auto hfe = tmpPath("k1520_test_di_k1.hfe");
    for (const auto& p : {td0, imd, hfe}) std::filesystem::remove(p);

    auto img = kleineDiskette();
    ASSERT_TRUE(img->saveAs(td0, std::nullopt)) << img->lastError();
    EXPECT_EQ(img->container(), ContainerType::Td0);
    ASSERT_NE(img->td0Info(), nullptr);
    EXPECT_EQ(img->imdInfo(), nullptr);
    EXPECT_EQ(img->td0Info()->sectors, 4u);

    ASSERT_TRUE(img->saveAs(imd, std::nullopt)) << img->lastError();
    EXPECT_EQ(img->container(), ContainerType::Imd);
    EXPECT_EQ(img->td0Info(), nullptr) << "Vorlage des alten Containers muss weg";
    ASSERT_NE(img->imdInfo(), nullptr);
    EXPECT_EQ(img->imdInfo()->sectors, 4u);

    ASSERT_TRUE(img->saveAs(hfe, std::nullopt)) << img->lastError();
    EXPECT_EQ(img->imdInfo(), nullptr);

    for (const auto& p : {td0, imd, hfe}) {
        auto back = DiskImage::open(p, std::nullopt, false);
        ASSERT_NE(back, nullptr) << p;
        auto s0 = TrackCodec::parseTrack(back->readTrack(0, 0));
        auto s1 = TrackCodec::parseTrack(back->readTrack(1, 0));
        ASSERT_EQ(s0.size(), 2u) << p;
        ASSERT_EQ(s1.size(), 2u) << p;
        EXPECT_EQ(s0[0].data[0], 0xAA) << p;
        EXPECT_EQ(s1[1].data[5], 0xBB) << p;
    }
    for (const auto& p : {td0, imd, hfe}) std::filesystem::remove(p);
}

/// @test Autoflush schreibt nach der Ruhezeit in td0/imd zurueck; Kommentar der Quelle ueberlebt
TEST(DiskImageTd0Imd, AutoflushSchreibtZurueckUndBehaeltDenKopf) {
    for (const char* ext : kSektorContainer) {
        SCOPED_TRACE(ext);
        const auto path = tmpPath((std::string("k1520_test_di_k1_af") + ext).c_str());
        std::filesystem::remove(path);

        auto img = kleineDiskette();
        ASSERT_TRUE(img->saveAs(path, std::nullopt)) << img->lastError();
        const std::string kommentar0 = std::string(ext) == ".td0" ? img->td0Info()->comment
                                                                   : img->imdInfo()->comment;
        EXPECT_NE(kommentar0.find("k1520emu"), std::string::npos) << kommentar0;

        img->writeTrack(1, 0, makeTrack(1, 2, 128, 0x77));
        EXPECT_FALSE(img->autoFlush(1000));
        EXPECT_FALSE(img->autoFlush(1000 + kAutoFlushDelayCycles / 2));
        EXPECT_TRUE(img->autoFlush(1000 + kAutoFlushDelayCycles + 1)) << img->lastError();
        EXPECT_FALSE(img->saveFailed());

        auto back = DiskImage::open(path, std::nullopt, false);
        ASSERT_NE(back, nullptr);
        auto secs = TrackCodec::parseTrack(back->readTrack(1, 0));
        ASSERT_EQ(secs.size(), 2u);
        EXPECT_EQ(secs[0].data[0], 0x77);
        const std::string kommentar1 = std::string(ext) == ".td0" ? back->td0Info()->comment
                                                                   : back->imdInfo()->comment;
        EXPECT_EQ(kommentar1, kommentar0);
        std::filesystem::remove(path);
    }
}

/// @test Ein selbst geschriebener IMD-Kommentar uebersteht Autoflush UND saveAs auf eine zweite .imd
TEST(DiskImageTd0Imd, ImdKommentarUebersteheSpeichernUndSaveAs) {
    const auto a = tmpPath("k1520_test_di_k1_kom_a.imd");
    const auto b = tmpPath("k1520_test_di_k1_kom_b.imd");
    for (const auto& p : {a, b}) std::filesystem::remove(p);

    // Quelle mit eigener Kopfzeile + Kommentar schreiben (Vorlage ueber ImdCodec).
    {
        auto img = kleineDiskette();
        ImdCodec::SourceInfo vorlage;
        const std::string kopf = "IMD 1.18: 01/02/2020 03:04:05\r\nMein Kommentar\r\n";
        vorlage.raw_header.assign(kopf.begin(), kopf.end());
        std::string err;
        ASSERT_TRUE(ImdCodec::save(a, img->medium(), &vorlage, nullptr, err)) << err;
    }
    auto img = DiskImage::open(a, std::nullopt, false);
    ASSERT_NE(img, nullptr);
    ASSERT_NE(img->imdInfo(), nullptr);
    EXPECT_EQ(img->imdInfo()->version, "1.18");
    EXPECT_NE(img->imdInfo()->comment.find("Mein Kommentar"), std::string::npos);

    img->writeTrack(0, 0, makeTrack(0, 2, 128, 0x42));
    ASSERT_TRUE(img->flush()) << img->lastError();
    ASSERT_TRUE(img->saveAs(b, std::nullopt)) << img->lastError();
    for (const auto& p : {a, b}) {
        auto back = DiskImage::open(p, std::nullopt, false);
        ASSERT_NE(back, nullptr) << p;
        EXPECT_EQ(back->imdInfo()->version, "1.18") << p;
        EXPECT_NE(back->imdInfo()->comment.find("Mein Kommentar"), std::string::npos) << p;
        EXPECT_EQ(TrackCodec::parseTrack(back->readTrack(0, 0))[0].data[0], 0x42) << p;
    }
    for (const auto& p : {a, b}) std::filesystem::remove(p);
}

/// @test Unformatiertes Medium: saveAs lehnt td0/imd mit Grund ab, Datei entsteht nicht, keine Umbindung
TEST(DiskImageTd0Imd, LeerdisketteWirdAbgelehnt) {
    for (const char* ext : kSektorContainer) {
        SCOPED_TRACE(ext);
        const auto path = tmpPath((std::string("k1520_test_di_k1_blank") + ext).c_str());
        std::filesystem::remove(path);
        auto img = DiskImage::createBlank(2, 1, Encoding::MFM);
        EXPECT_FALSE(img->saveAs(path, std::nullopt));
        EXPECT_NE(std::string(img->lastError()).find("unformatiert"), std::string::npos)
            << img->lastError();
        EXPECT_FALSE(std::filesystem::exists(path));
        EXPECT_FALSE(img->hasFile());
    }
}

/// @test Nicht darstellbare Aenderung (Nachspann hinter der Daten-CRC) im gebundenen td0/imd:
///       flush scheitert mit Grund, Datei bleibt, saveFailed() gesetzt
TEST(DiskImageTd0Imd, NichtDarstellbaresMediumMeldetLastError) {
    for (const char* ext : kSektorContainer) {
        SCOPED_TRACE(ext);
        const auto path = tmpPath((std::string("k1520_test_di_k1_fail") + ext).c_str());
        std::filesystem::remove(path);
        auto img = kleineDiskette();
        ASSERT_TRUE(img->saveAs(path, std::nullopt)) << img->lastError();

        // Spur mit Nutzdaten hinter der Daten-CRC (UDOS-Kontrollblock).
        std::vector<LogicalSector> secs;
        for (int i = 1; i <= 2; ++i) {
            LogicalSector ls;
            ls.cyl = 1; ls.head = 0; ls.id = static_cast<uint8_t>(i); ls.size = 128;
            ls.data.assign(128, 0x11);
            ls.tail.assign(4, 0x99);
            secs.push_back(std::move(ls));
        }
        img->writeTrack(1, 0, TrackCodec::buildTrack(secs, Encoding::MFM));
        EXPECT_FALSE(img->flush());
        EXPECT_TRUE(img->saveFailed());
        EXPECT_NE(std::string(img->lastError()), "");
        auto back = DiskImage::open(path, std::nullopt, false);
        ASSERT_NE(back, nullptr);
        EXPECT_EQ(TrackCodec::parseTrack(back->readTrack(1, 0))[0].data[0], 0xBB)
            << "Datei darf nicht verstuemmelt sein";
        std::filesystem::remove(path);
    }
}

/// @test create() mit Format legt td0/imd vorformatiert an (Interleave-/Sektorzahl aus dem Format)
TEST(DiskImageTd0Imd, CreateMitFormatLegtVorformatiertAn) {
    for (const char* ext : kSektorContainer) {
        SCOPED_TRACE(ext);
        const auto path = tmpPath((std::string("k1520_test_di_k1_create") + ext).c_str());
        std::filesystem::remove(path);
        std::string err;
        auto img = DiskImage::create(path, makeSimpleFormat(), false, Encoding::MFM, &err);
        ASSERT_NE(img, nullptr) << err;
        EXPECT_TRUE(std::filesystem::exists(path));
        auto back = DiskImage::open(path, std::nullopt, false);
        ASSERT_NE(back, nullptr);
        EXPECT_EQ(back->geometry().num_cyls, 2u);
        EXPECT_EQ(TrackCodec::parseTrack(back->readTrack(1, 0)).size(), 2u);
        img.reset(); back.reset();
        std::filesystem::remove(path);
    }
}
