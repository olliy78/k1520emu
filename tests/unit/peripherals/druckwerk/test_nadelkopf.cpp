/**
 * @file test_nadelkopf.cpp
 * @brief Nadelkopf (Entwurf 49 §4.3): Muster + Wagenposition + Papierlage = Punkte; Flugzeit je Richtung.
 */

#include <gtest/gtest.h>

#include "core/peripherals/druckwerk/nadelkopf.h"

namespace {
NadeldruckParameter par() { return NadeldruckParameter{}; }   // 2 Pixel je Takt, Teilung 2, Punkt 2 x 1, Flug +-1
}

TEST(Nadelkopf, MusterBitKIstNadelKPlus1ObenNachUnten)
{
    NadeldruckParameter p = par();
    p.nadel1_y = 4;
    Nadelkopf k(p);
    Blattmodell b(400);
    EXPECT_EQ(k.feuere(0x101, 10, 100, +1, true, b), 4) << "Nadel 1 und 9: je ein Punkt aus zwei Pixeln";
    // Nadel 1 bei y = 100 + 4, Nadel 9 bei 100 + 4 + 8 * 2
    EXPECT_TRUE(b.punkt(20, 104));
    EXPECT_TRUE(b.punkt(21, 104));
    EXPECT_TRUE(b.punkt(20, 120));
    EXPECT_FALSE(b.punkt(20, 106)) << "Nadel 2 schlug nicht an";
    EXPECT_EQ(b.punktzahl(), 4u);
}

TEST(Nadelkopf, AlleNeunNadelnLiegenImAbstandDerTeilung)
{
    NadeldruckParameter p = par();
    p.nadel_teilung_y = 3;
    Nadelkopf k(p);
    Blattmodell b(100);
    k.feuere(0x1FF, 5, 0, +1, true, b);
    for (int j = 0; j < 9; ++j) EXPECT_TRUE(b.punkt(10, 3 * j)) << "Nadel " << j + 1;
    EXPECT_FALSE(b.punkt(10, 1));
    EXPECT_EQ(b.hoehe(), 3 * 8 + 1);
}

TEST(Nadelkopf, DasMusterWirdAufNeunBitBegrenzt)
{
    Nadelkopf k(par());
    Blattmodell b(100);
    EXPECT_EQ(k.feuere(0xFE00, 5, 0, +1, true, b), 0);
    EXPECT_EQ(k.anschlaege(), 0u) << "ein leerer Anschlag zaehlt nicht";
}

TEST(Nadelkopf, VorUndRueckwaertsLandenMitFlugzeitAufDenselbenPunkten)
{
    // Vorwaerts loest der Wagen bei Position p aus, rueckwaerts eine Position spaeter (die Firmware
    // gleicht damit die Flugzeit aus): beide Anschlaege muessen auf denselben Pixeln landen.
    Nadelkopf k(par());
    Blattmodell vor(100), rueck(100);
    k.feuere(0x155, 10, 0, +1, true, vor);
    k.feuere(0x155, 11, 0, -1, true, rueck);
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 100; ++x) ASSERT_EQ(vor.punkt(x, y), rueck.punkt(x, y)) << x << "," << y;
    EXPECT_EQ(vor.punktzahl(), rueck.punktzahl());
    EXPECT_GT(vor.punktzahl(), 0u);
}

TEST(Nadelkopf, OhneRichtungsabhaengigeFlugzeitLiegtDerRueckwaertsdruckEinenTaktRechts)
{
    NadeldruckParameter p = par();
    p.versatz_vor = p.versatz_rueck = +1;   // gleiche Landung in beiden Richtungen
    Nadelkopf k(p);
    Blattmodell vor(100), rueck(100);
    k.feuere(0x001, 10, 0, +1, true, vor);
    k.feuere(0x001, 11, 0, -1, true, rueck);
    EXPECT_TRUE(vor.punkt(20, 0));
    EXPECT_TRUE(rueck.punkt(22, 0));
    EXPECT_FALSE(rueck.punkt(20, 0));
    EXPECT_FALSE(vor.punkt(22, 0));
}

TEST(Nadelkopf, KopfNichtAngeschwenktBringtKeinenPunktAufsPapier)
{
    Nadelkopf k(par());
    Blattmodell b(100);
    EXPECT_EQ(k.feuere(0x003, 5, 0, +1, false, b), 0);
    EXPECT_EQ(b.punktzahl(), 0u);
    EXPECT_EQ(k.abgewiesen(), 2u);
    EXPECT_EQ(k.anschlaege(), 1u);
}

TEST(Nadelkopf, PunkteAusserhalbDesBlattsZaehlenNichtAlsGesetzt)
{
    Nadelkopf k(par());
    Blattmodell b(30);
    EXPECT_EQ(k.feuere(0x001, 100, 0, +1, true, b), 0);
    EXPECT_EQ(k.punkteGesetzt(), 0u);
    EXPECT_GT(b.ausserhalb(), 0u);
    EXPECT_EQ(k.feuere(0x001, 5, 0, +1, true, b), 2);
    EXPECT_EQ(k.punkteGesetzt(), 2u);
}

TEST(Nadelkopf, PunktmassKommtAusDemParametersatz)
{
    NadeldruckParameter p = par();
    p.punkt_breite = 3;
    p.punkt_hoehe = 2;
    Nadelkopf k(p);
    Blattmodell b(100);
    EXPECT_EQ(k.feuere(0x001, 10, 10, +1, true, b), 6);
    EXPECT_EQ(b.punktzahl(), 6u);
}
