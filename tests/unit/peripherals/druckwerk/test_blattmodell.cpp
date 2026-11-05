/**
 * @file test_blattmodell.cpp
 * @brief Blattmodell (Entwurf 49 §4.3): waechst nach unten, zeilenblockweise, Versionszaehler, Lesen je Zeilenbereich.
 */

#include <gtest/gtest.h>
#include <vector>

#include "core/peripherals/druckwerk/blattmodell.h"

TEST(Blattmodell, WaechstNachUntenMitDemTiefstenPunkt)
{
    Blattmodell b(100);
    EXPECT_EQ(b.hoehe(), 0);
    EXPECT_TRUE(b.setze(5, 200));
    EXPECT_EQ(b.hoehe(), 201);   // Zeilen 0..200
    EXPECT_TRUE(b.setze(7, 3));
    EXPECT_EQ(b.hoehe(), 201) << "ein hoeherer Punkt vergroessert das Blatt nicht";
    EXPECT_TRUE(b.punkt(5, 200));
    EXPECT_FALSE(b.punkt(5, 199));
    EXPECT_FALSE(b.punkt(6, 200));
}

TEST(Blattmodell, VersionSteigtBeiNeuemPunktWachstumUndLeerenNichtBeiWiederholung)
{
    Blattmodell b(64);
    const uint64_t v0 = b.version();
    b.setze(1, 1);
    const uint64_t v1 = b.version();
    EXPECT_GT(v1, v0);
    b.setze(1, 1);                       // derselbe Punkt: nichts Neues
    EXPECT_EQ(b.version(), v1);
    b.streckeBis(500);
    EXPECT_GT(b.version(), v1);
    const uint64_t v2 = b.version();
    b.streckeBis(400);                   // schrumpft nie
    EXPECT_EQ(b.version(), v2);
    EXPECT_EQ(b.hoehe(), 500);
    b.leeren();
    EXPECT_GT(b.version(), v2);
    EXPECT_EQ(b.hoehe(), 0);
    EXPECT_EQ(b.punktzahl(), 0u);
}

TEST(Blattmodell, PunktzahlZaehltJedenOrtNurEinmal)
{
    Blattmodell b(64);
    for (int i = 0; i < 3; ++i) { b.setze(10, 10); b.setze(11, 10); }
    EXPECT_EQ(b.punktzahl(), 2u);
}

TEST(Blattmodell, AusserhalbWirdVerworfenUndGezaehlt)
{
    Blattmodell b(50);
    EXPECT_FALSE(b.setze(-1, 0));
    EXPECT_FALSE(b.setze(50, 0));
    EXPECT_FALSE(b.setze(0, -1));
    EXPECT_TRUE(b.setze(49, 0));
    EXPECT_EQ(b.ausserhalb(), 3u);
    EXPECT_EQ(b.punktzahl(), 1u);
    EXPECT_EQ(b.hoehe(), 1);
}

TEST(Blattmodell, ZeilenformatBit7IstDerLinkstePunkt)
{
    Blattmodell b(13);                   // 2 Bytes je Zeile
    EXPECT_EQ(b.zeilenBytes(), 2);
    b.setze(0, 0);
    b.setze(7, 0);
    b.setze(8, 0);
    b.setze(12, 1);
    uint8_t z[4] = {};
    ASSERT_EQ(b.lies(0, 2, z, sizeof z), 2);
    EXPECT_EQ(z[0], 0x81);               // Punkte 0 und 7
    EXPECT_EQ(z[1], 0x80);               // Punkt 8
    EXPECT_EQ(z[2], 0x00);
    EXPECT_EQ(z[3], 0x08);               // Punkt 12 = Bit 7-(12-8) = Bit 3
}

TEST(Blattmodell, LiesLiefertNurVorhandeneZeilenUndBeachtetDenPuffer)
{
    Blattmodell b(8);
    b.setze(0, 9);                       // 10 Zeilen
    std::vector<uint8_t> z(100, 0xAA);
    EXPECT_EQ(b.lies(0, 100, z.data(), z.size()), 10);
    EXPECT_EQ(z[9], 0x80);
    EXPECT_EQ(z[0], 0x00) << "nie beschriebene Zeile liest als leer";
    EXPECT_EQ(b.lies(5, 100, z.data(), z.size()), 5);
    EXPECT_EQ(b.lies(10, 5, z.data(), z.size()), 0) << "ab der Hoehe nichts";
    EXPECT_EQ(b.lies(-1, 5, z.data(), z.size()), 0);
    EXPECT_EQ(b.lies(0, 100, z.data(), 3), 3) << "Puffer begrenzt";
}

TEST(Blattmodell, BlockgrenzenSindUnsichtbar)
{
    Blattmodell b(16);
    for (int y : {Blattmodell::BLOCK_ZEILEN - 1, Blattmodell::BLOCK_ZEILEN, 3 * Blattmodell::BLOCK_ZEILEN + 1})
        b.setze(y % 16, y);
    std::vector<uint8_t> z(2 * static_cast<size_t>(Blattmodell::BLOCK_ZEILEN + 2));
    const int n = b.lies(Blattmodell::BLOCK_ZEILEN - 1, 2, z.data(), z.size());
    ASSERT_EQ(n, 2);
    EXPECT_TRUE(b.punkt((Blattmodell::BLOCK_ZEILEN - 1) % 16, Blattmodell::BLOCK_ZEILEN - 1));
    EXPECT_TRUE(b.punkt(Blattmodell::BLOCK_ZEILEN % 16, Blattmodell::BLOCK_ZEILEN));
    EXPECT_FALSE(b.punkt(0, 2 * Blattmodell::BLOCK_ZEILEN)) << "ein nie angelegter Block liest als leer";
}

TEST(Blattmodell, AsciiAbzugZeigtDenAusschnittUndFasstPunkteZusammen)
{
    Blattmodell b(8);
    b.setze(1, 0);
    b.setze(2, 1);
    b.setze(7, 3);
    EXPECT_EQ(blattAscii(b, 0, 0, 4, 2), ".#..\n..#.\n");
    // sx = 2, sy = 2: ein Zeichen = 2 x 2 Pixel
    EXPECT_EQ(blattAscii(b, 0, 0, 8, 4, 2, 2), "##..\n...#\n");
}
