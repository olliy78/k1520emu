/**
 * @file test_nadeldruckwerk.cpp
 * @brief Nadeldruckwerk (Entwurf 49 §4.3): Zusammenspiel von Wagen, Papierweg, Nadelkopf und Blatt.
 */

#include <gtest/gtest.h>
#include <memory>

#include "core/peripherals/druckwerk/geregelter_wagen.h"
#include "core/peripherals/druckwerk/nadeldruckwerk.h"

namespace {
Nadeldruckwerk neu(int bahnen = 1)
{
    NadeldruckParameter p;
    p.bahnen = bahnen;
    return Nadeldruckwerk(p, std::make_unique<GeregelterWagen>());
}
void laufe(Nadeldruckwerk& d, int takte) { for (int t = 0; t < takte; t += 10) d.takt(10); }
}  // namespace

TEST(Nadeldruckwerk, DerPunktLandetBeiWagenpositionUndPapierlage)
{
    Nadeldruckwerk d = neu();
    d.netzEin(300);
    const int y = d.papierY(0);
    EXPECT_EQ(y, d.parameter().rand_oben_y);
    EXPECT_EQ(d.feuere(0x001, true), 2);
    EXPECT_TRUE(d.blatt(0).punkt(2 * 300, y));
    EXPECT_TRUE(d.blatt(0).punkt(2 * 300 + 1, y));
    EXPECT_EQ(d.kopfX(), 300);
    EXPECT_EQ(d.kopfXPixel(), 600);
}

TEST(Nadeldruckwerk, DiePapierlageVerschiebtDiePunkteNachUntenUndStrecktDasBlatt)
{
    Nadeldruckwerk d = neu();
    d.netzEin(300);
    const int h0 = d.blatt(0).hoehe();
    d.papier(0).ausgaenge(true, true, true);
    laufe(d, 4 * 7373 + 100);
    d.papier(0).ausgaenge(false, true, true);
    ASSERT_EQ(d.papier(0).lage(), 4);
    EXPECT_EQ(d.papierY(0), d.parameter().rand_oben_y + 4 * d.parameter().pv_schritt_y);
    EXPECT_GT(d.blatt(0).hoehe(), h0) << "das Blatt waechst mit dem Papier";
    d.feuere(0x001, true);
    EXPECT_TRUE(d.blatt(0).punkt(600, d.papierY(0)));
    EXPECT_FALSE(d.blatt(0).punkt(600, d.parameter().rand_oben_y));
}

TEST(Nadeldruckwerk, RueckwaertsfahrtGleichtDieFlugzeitAus)
{
    // vorwaerts ausgeloest bei 301, rueckwaerts eine Position spaeter (302) = dieselben Pixel
    Nadeldruckwerk vor = neu(), rueck = neu();
    vor.netzEin(300);
    rueck.netzEin(303);
    WagenEingang e;
    e.lfg = true;
    e.vorwaerts = true;
    vor.wagen().eingang(e);
    laufe(vor, 1365 + 10);
    ASSERT_EQ(vor.kopfX(), 301);
    ASSERT_EQ(vor.wagen().richtung(), +1);
    vor.feuere(0x155, true);

    e.vorwaerts = false;
    rueck.wagen().eingang(e);
    laufe(rueck, 1365 + 10);
    ASSERT_EQ(rueck.kopfX(), 302);
    ASSERT_EQ(rueck.wagen().richtung(), -1);
    rueck.feuere(0x155, true);

    ASSERT_GT(vor.blatt(0).punktzahl(), 0u);
    EXPECT_EQ(vor.blatt(0).punktzahl(), rueck.blatt(0).punktzahl());
    for (int y = 0; y < 60; ++y)
        for (int x = 590; x < 620; ++x) ASSERT_EQ(vor.blatt(0).punkt(x, y), rueck.blatt(0).punkt(x, y)) << x << "," << y;
}

TEST(Nadeldruckwerk, ZweiBahnenHabenEigenesBlattUndEigenenPapierweg)
{
    Nadeldruckwerk d = neu(2);
    ASSERT_EQ(d.bahnen(), 2);
    d.netzEin(300);
    d.papier(1).ausgaenge(true, true, true);
    laufe(d, 2 * 7373 + 100);
    EXPECT_EQ(d.papier(0).lage(), 0);
    EXPECT_EQ(d.papier(1).lage(), 2);
    EXPECT_NE(&d.blatt(0), &d.blatt(1));
}

TEST(Nadeldruckwerk, HaltLaesstPositionPapierUndBlattNetzEinLeertAlles)
{
    Nadeldruckwerk d = neu();
    d.netzEin(300);
    d.feuere(0x1FF, true);
    d.papier(0).ausgaenge(true, true, true);
    laufe(d, 3 * 7373 + 100);
    const int lage = d.papier(0).lage();
    ASSERT_GT(lage, 0);
    d.halt();
    laufe(d, 50000);
    EXPECT_EQ(d.papier(0).lage(), lage);
    EXPECT_GT(d.blatt(0).punktzahl(), 0u);
    d.netzEin(300);
    EXPECT_EQ(d.papier(0).lage(), 0);
    EXPECT_EQ(d.blatt(0).punktzahl(), 0u);
}

TEST(Nadeldruckwerk, KopfNichtAnliegendUndBlattLeeren)
{
    Nadeldruckwerk d = neu();
    d.netzEin(300);
    EXPECT_EQ(d.feuere(0x003, false), 0);
    EXPECT_EQ(d.kopf().abgewiesen(), 2u);
    d.feuere(0x003, true);
    ASSERT_GT(d.blatt(0).punktzahl(), 0u);
    const uint64_t v = d.blatt(0).version();
    d.blattLeeren();
    EXPECT_EQ(d.blatt(0).punktzahl(), 0u);
    EXPECT_GT(d.blatt(0).version(), v);
}
