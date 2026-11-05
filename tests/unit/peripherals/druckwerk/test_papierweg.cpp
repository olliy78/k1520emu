/**
 * @file test_papierweg.cpp
 * @brief Papierweg (Entwurf 49 §4.3, hardware.md §10.2): Schrittgeber mit Freigabe, Richtung, Hochlauf; Papierende.
 */

#include <gtest/gtest.h>

#include "core/peripherals/druckwerk/papierweg.h"

namespace {
// Maschinentakte vorruecken, in kleinen Schritten wie die CPU (<= 23 T je Befehl)
int laufe(Papierweg& p, int takte, int schritt = 20)
{
    int flanken = 0;
    for (int t = 0; t < takte; t += schritt) flanken += p.takt(schritt) ? 1 : 0;
    return flanken;
}
}  // namespace

TEST(Papierweg, OhneFreigabeKeineImpulse)
{
    Papierweg p;
    p.ausgaenge(false, true, true);
    EXPECT_EQ(laufe(p, 200000), 0);
    EXPECT_EQ(p.lage(), 0);
    EXPECT_EQ(p.schritte(), 0u);
}

TEST(Papierweg, ImpulsabstandFolgtDemHochlaufPegel)
{
    const Papierweg::Parameter par;
    Papierweg schnell, anlauf;
    schnell.ausgaenge(true, true, true);
    anlauf.ausgaenge(true, true, false);
    laufe(schnell, 3 * par.abstand_schnell + 20);
    laufe(anlauf, 3 * par.abstand_anlauf + 20);
    EXPECT_EQ(schnell.lage(), 3);
    EXPECT_EQ(anlauf.lage(), 3);
    // und der Anlauf ist langsamer als der Lauf
    Papierweg a2, s2;
    a2.ausgaenge(true, true, false);
    s2.ausgaenge(true, true, true);
    laufe(a2, 100000);
    laufe(s2, 100000);
    EXPECT_LT(a2.lage(), s2.lage());
}

TEST(Papierweg, JederImpulsAbstandBleibtUnterDemWaechterDerFirmware)
{
    // CTC 2/3 (7,08 ms bei 2,4576 MHz = 17400 T): ein laengerer Abstand ist ein Papiervorschubfehler (§6, §10.2)
    const Papierweg::Parameter par;
    EXPECT_LT(par.abstand_schnell, 17400);
    EXPECT_LT(par.abstand_anlauf, 17400);
    EXPECT_GE(par.impuls, 25) << ">= 10 us (hardware.md §15 Nr. 13)";
}

TEST(Papierweg, RichtungRueckwaertsZaehltGetrennt)
{
    Papierweg p;
    p.ausgaenge(true, false, true);
    laufe(p, 4 * 7373 + 20);
    EXPECT_EQ(p.lage(), -4);
    EXPECT_EQ(p.schritteRueck(), 4u);
    EXPECT_EQ(p.schritteVor(), 0u);
    p.ausgaenge(true, true, true);
    laufe(p, 2 * 7373);
    EXPECT_EQ(p.lage(), -2);
    EXPECT_EQ(p.schritteVor(), 2u);
    EXPECT_EQ(p.schritte(), 6u);
}

TEST(Papierweg, ImpulsLiegtImpulsbreiteAnUndMeldetBeideFlanken)
{
    Papierweg p;
    p.ausgaenge(true, true, true);
    int hoch = 0, runter = 0;
    int hoch_takte = 0;
    for (int t = 0; t < 7373 * 2 + 200; ++t) {
        const bool vorher = p.impuls();
        const bool flanke = p.takt(1);
        EXPECT_EQ(flanke, vorher != p.impuls());
        if (flanke && p.impuls()) ++hoch;
        if (flanke && !p.impuls()) ++runter;
        if (p.impuls()) ++hoch_takte;
    }
    EXPECT_EQ(hoch, 2);
    EXPECT_EQ(runter, 2);
    EXPECT_EQ(hoch_takte, 2 * Papierweg::Parameter{}.impuls);
}

TEST(Papierweg, FreigabeWegStopptDenGeberDerImpulsLaeuftAus)
{
    Papierweg p;
    p.ausgaenge(true, true, true);
    laufe(p, 7373 + 20);
    EXPECT_EQ(p.lage(), 1);
    p.ausgaenge(false, true, true);
    laufe(p, 100000);
    EXPECT_EQ(p.lage(), 1);
    EXPECT_FALSE(p.impuls());
}

TEST(Papierweg, ErsterImpulsKommtErstNachEinemVollenAbstand)
{
    Papierweg p;
    p.ausgaenge(true, true, true);
    for (int t = 0; t < 7373 - 1; ++t) p.takt(1);
    EXPECT_EQ(p.lage(), 0);
    p.takt(1);
    EXPECT_EQ(p.lage(), 1);
}

TEST(Papierweg, PapierendeNachLaengeUndPerAnwender)
{
    Papierweg::Parameter par;
    par.laenge = 3;
    Papierweg p(par);
    EXPECT_TRUE(p.papierDa());
    p.ausgaenge(true, true, true);
    laufe(p, 2 * 7373 + 20);
    EXPECT_TRUE(p.papierDa());
    laufe(p, 7373);
    EXPECT_EQ(p.lage(), 3);
    EXPECT_FALSE(p.papierDa());
    p.ausgaenge(true, false, true);          // zurueck: wieder Papier
    laufe(p, 7373);
    EXPECT_TRUE(p.papierDa());
    p.setzePapierEnde(true);
    EXPECT_FALSE(p.papierDa());
    p.setzePapierEnde(false);
    p.setzeLaenge(-1);
    EXPECT_TRUE(p.papierDa());
}

TEST(Papierweg, HaltBehaeltDieLageNetzEinLoeschtSie)
{
    Papierweg p;
    p.ausgaenge(true, true, true);
    laufe(p, 5 * 7373 + 20);
    ASSERT_EQ(p.lage(), 5);
    p.halt();
    laufe(p, 50000);
    EXPECT_EQ(p.lage(), 5);
    p.netzEin();
    EXPECT_EQ(p.lage(), 0);
    EXPECT_EQ(p.schritte(), 0u);
}
