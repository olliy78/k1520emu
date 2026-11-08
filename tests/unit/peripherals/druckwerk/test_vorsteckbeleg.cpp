/**
 * @file test_vorsteckbeleg.cpp
 * @brief Vorsteckbeleg (hardware.md §11): Lichtschranken aus der Lage, Klemmen, Hinausfahren, Druckzeile, eigenes Blatt.
 */

#include <gtest/gtest.h>

#include "core/peripherals/druckwerk/vorsteckbeleg.h"

TEST(Vorsteckbeleg, OhneBelegSindBeideLichtschrankenFreiUndBewegungWirktNicht)
{
    Vorsteckbeleg b;
    EXPECT_FALSE(b.eingelegt());
    EXPECT_FALSE(b.hatBlatt());
    b.bewege(100);
    EXPECT_FALSE(b.fso());
    EXPECT_FALSE(b.fsu());
    EXPECT_FALSE(b.unterDruckzeile());
}

TEST(Vorsteckbeleg, LichtschrankenFolgenSpitzeUndEnde)
{
    Vorsteckbeleg b;
    const Vorsteckbeleg::Parameter& p = b.parameter();
    ASSERT_TRUE(b.einlegen(400, 1000));
    EXPECT_FALSE(b.einlegen(400, 1000)) << "erst entnehmen";
    EXPECT_EQ(b.u(), p.einlege_lage);
    EXPECT_TRUE(b.fso());
    EXPECT_FALSE(b.fsu()) << "Spitze hinter FSO, vor FSU";
    b.bewege(p.abstand_fso_fsu - p.einlege_lage);
    EXPECT_TRUE(b.fsu());
    b.bewege(400 - b.u());          // u = L: das Ende steht an FSO
    EXPECT_TRUE(b.fso());
    b.bewege(1);                    // Ende hinter FSO
    EXPECT_FALSE(b.fso());
    EXPECT_TRUE(b.fsu());
    b.bewege(p.abstand_fso_fsu);    // Ende hinter FSU
    EXPECT_FALSE(b.fsu());
    b.bewege(-(b.u() - 5));         // zurueck: Spitze 5 hinter FSO
    EXPECT_TRUE(b.fso());
    EXPECT_FALSE(b.fsu());
}

TEST(Vorsteckbeleg, EinzugKlemmtAmEndeUndAuswurfFuehrtDenBelegGanzHinaus)
{
    Vorsteckbeleg b;
    ASSERT_TRUE(b.einlegen(300, 500));
    b.bewege(100000);
    EXPECT_EQ(b.u(), b.parameter().u_max) << "der Motor laeuft leer";
    EXPECT_TRUE(b.eingelegt());
    b.bewege(-100000);
    EXPECT_FALSE(b.eingelegt()) << "ganz aus dem Schacht";
    EXPECT_FALSE(b.fso());
    EXPECT_TRUE(b.hatBlatt()) << "das Blatt bleibt zum Ansehen";
    EXPECT_TRUE(b.einlegen(300, 500)) << "ein neuer Beleg geht nun";
}

TEST(Vorsteckbeleg, DruckzeileLiegtErstUntenAufDemBelegUndWandertMitDemEinzugNachOben)
{
    Vorsteckbeleg b;
    const int P = b.parameter().druckzeile;
    ASSERT_TRUE(b.einlegen(500, 1000));
    EXPECT_FALSE(b.unterDruckzeile()) << "Spitze noch vor der Druckzeile";
    b.bewege(P - b.u());
    EXPECT_TRUE(b.unterDruckzeile());
    const int y0 = b.druckzeileY();
    b.bewege(9);                    // Beleg 9 Schritte weiter hinein: die Druckzeile liegt 9 Schritte (18 Zeilen) weiter oben
    EXPECT_EQ(b.druckzeileY(), y0 - 18);
    b.bewege(500 + P - b.u() + 1);  // Ende an der Druckzeile vorbei
    EXPECT_FALSE(b.unterDruckzeile());
    EXPECT_TRUE(b.trifftX(b.xLinks()));
    EXPECT_FALSE(b.trifftX(b.xLinks() - 1));
    EXPECT_FALSE(b.trifftX(b.xLinks() + 1000));
}

TEST(Vorsteckbeleg, BlattHatDieMasseDesBelegsUndDieVersionWaechstUeberDenWechselHinweg)
{
    Vorsteckbeleg b;
    ASSERT_TRUE(b.einlegen(320, 640));
    EXPECT_EQ(b.blatt().breite(), 640);
    EXPECT_EQ(b.blatt().hoehe(), 640);   // 2 Zeilen je Schritt
    b.blatt().setze(3, 4);
    const uint64_t v1 = b.version();
    EXPECT_GT(v1, 0u);
    b.entnehmen();
    ASSERT_TRUE(b.einlegen(330, 700));
    EXPECT_GT(b.version(), v1) << "auch ein frisches Blatt zaehlt weiter";
    EXPECT_EQ(b.blatt().punktzahl(), 0u);
}
