/**
 * @file test_geregelter_wagen.cpp
 * @brief GeregelterWagen (SD 1157, hardware.md §10.1/§15): LAUF sofort, Takte je Position, Bremsweg, Raender, /VK.
 */

#include <gtest/gtest.h>

#include "core/peripherals/druckwerk/geregelter_wagen.h"

namespace {
WagenEingang lauf(bool vor, bool schnell = false)
{
    WagenEingang e;
    e.lfg = true;
    e.vorwaerts = vor;
    e.schnell = schnell;
    return e;
}
void laufe(Wagen& w, int takte) { for (int t = 0; t < takte; ++t) w.takt(1); }
}  // namespace

TEST(GeregelterWagen, LaufIstSofortMitLfgOhneZeitVerstrichen)
{
    GeregelterWagen w;
    w.netzEin(500);
    EXPECT_FALSE(w.rueckmeldung().lauf);
    w.eingang(lauf(true));
    EXPECT_TRUE(w.rueckmeldung().lauf);
    EXPECT_TRUE(w.laeuft());
    EXPECT_EQ(w.position(), 500);
}

TEST(GeregelterWagen, EinTaktJePositionImAbstandNachVtab)
{
    GeregelterWagen w;
    const auto par = w.parameter();
    w.netzEin(500);
    w.eingang(lauf(true));
    laufe(w, 3 * par.abstand_druck);
    EXPECT_EQ(w.position(), 503);
    w.eingang(lauf(true, true));
    const int p0 = w.position();
    laufe(w, 4 * par.abstand_tab);
    EXPECT_EQ(w.position(), p0 + 4);
}

TEST(GeregelterWagen, TaktImpulseAufRichtungsgetrenntenLeitungenAktivHighMitBreite)
{
    GeregelterWagen w;
    w.netzEin(500);
    w.eingang(lauf(true));
    int hoch = 0, hoch_takte = 0;
    bool rueck_gesehen = false;
    for (int t = 0; t < 4 * w.parameter().abstand_druck + w.parameter().takt_impuls; ++t) {
        const bool vorher = w.rueckmeldung().takt_vor;
        w.takt(1);
        const auto r = w.rueckmeldung();
        if (!vorher && r.takt_vor) ++hoch;
        if (r.takt_vor) ++hoch_takte;
        rueck_gesehen |= r.takt_rueck;
    }
    EXPECT_EQ(hoch, 4);
    EXPECT_EQ(hoch_takte, 4 * w.parameter().takt_impuls);
    EXPECT_FALSE(rueck_gesehen);
    // rueckwaerts ist die andere Leitung
    w.eingang(lauf(false));
    bool rueck = false, vor_dabei = false;
    for (int t = 0; t < 2 * w.parameter().abstand_druck; ++t) {
        w.takt(1);
        rueck |= w.rueckmeldung().takt_rueck;
        vor_dabei |= w.rueckmeldung().takt_rueck && w.rueckmeldung().takt_vor;
    }
    EXPECT_TRUE(rueck);
    EXPECT_FALSE(vor_dabei);
}

TEST(GeregelterWagen, DieRichtungFolgtVwNurUnterLfg)
{
    GeregelterWagen w;
    w.netzEin(500);
    w.eingang(lauf(true));
    WagenEingang e = lauf(true);
    e.lfg = false;
    w.eingang(e);                    // LFG weg, Auslauf
    e.vorwaerts = false;
    w.eingang(e);                    // VW umschalten waehrend des Auslaufs: wirkungslos
    EXPECT_EQ(w.richtung(), +1);
    laufe(w, 100000);
    EXPECT_GT(w.position(), 500) << "der Wagen lief vorwaerts aus";
}

TEST(GeregelterWagen, AuslaufNachLfgWegIstDerBremswegVonDruckUndTab)
{
    for (bool schnell : {false, true}) {
        GeregelterWagen w;
        const auto par = w.parameter();
        w.netzEin(500);
        w.eingang(lauf(true, schnell));
        WagenEingang aus = lauf(true, schnell);
        aus.lfg = false;
        w.eingang(aus);
        EXPECT_TRUE(w.rueckmeldung().lauf) << "LAUF bleibt, bis der Wagen steht";
        laufe(w, 200000);
        EXPECT_FALSE(w.rueckmeldung().lauf);
        EXPECT_FALSE(w.laeuft());
        EXPECT_EQ(w.position(), 500 + (schnell ? par.bremsweg_tab : par.bremsweg_druck)) << "schnell=" << schnell;
    }
}

TEST(GeregelterWagen, EineLfgSpitzeBewegtDenWagenUmDenBremswegDasMachtDieFirmwareBeimEinschalten)
{
    // §11 Nr. 6: LFG kurz auf 1 und wieder 0 (zwei OUT hintereinander) bei VTAB = 1
    GeregelterWagen w;
    w.netzEin(500);
    w.eingang(lauf(false, true));
    EXPECT_TRUE(w.rueckmeldung().lauf);
    WagenEingang aus = lauf(false, true);
    aus.lfg = false;
    w.eingang(aus);
    EXPECT_TRUE(w.rueckmeldung().lauf);
    laufe(w, 100000);
    EXPECT_FALSE(w.rueckmeldung().lauf);
    EXPECT_EQ(w.position(), 500 - w.parameter().bremsweg_tab);
}

TEST(GeregelterWagen, RaenderSindPegelDerPosition)
{
    GeregelterWagen w;
    const auto par = w.parameter();
    for (int pos : {par.lr_bis, par.lr_bis + 1, par.rr_ab - 1, par.rr_ab}) {
        w.netzEin(pos);
        const auto r = w.rueckmeldung();
        EXPECT_EQ(r.lr, pos > par.lr_bis) << pos;           // /LR = 0 fuer <= 76
        EXPECT_EQ(r.rr, pos < par.rr_ab) << pos;            // /RR = 0 ab 1420
    }
    EXPECT_EQ(par.lr_bis, 76) << "[Q:1239-1240]";
}

TEST(GeregelterWagen, VorwarnkontaktDecktDenLrBereichAb)
{
    // Ohne das findet die Grundstellung aus Positionen im Randbereich die /LR-Flanke nie (merkposten/drucker.md)
    GeregelterWagen w;
    const auto par = w.parameter();
    EXPECT_GE(par.vk_bis, par.lr_bis);
    for (int pos = par.pos_min; pos <= par.lr_bis; ++pos) {
        w.netzEin(pos);
        ASSERT_FALSE(w.rueckmeldung().vk) << pos << ": /VK muss im /LR-Bereich 0 sein";
    }
    w.netzEin(par.vk_bis + 1);
    EXPECT_TRUE(w.rueckmeldung().vk);
}

TEST(GeregelterWagen, WeicheAnschlaegeHaltenDenWagenAn)
{
    GeregelterWagen w;
    const auto par = w.parameter();
    w.netzEin(par.pos_min + 2);
    w.eingang(lauf(false, true));
    laufe(w, 10 * par.abstand_tab);
    EXPECT_EQ(w.position(), par.pos_min);
    EXPECT_FALSE(w.laeuft());
    EXPECT_TRUE(w.rueckmeldung().lauf) << "LFG ist noch 1: LAUF = LFG ODER bewegt";
}

TEST(GeregelterWagen, HaltBehaeltDiePositionNetzEinSetztSie)
{
    GeregelterWagen w;
    w.netzEin(500);
    w.eingang(lauf(true, true));
    laufe(w, 5 * w.parameter().abstand_tab);
    const int p = w.position();
    ASSERT_GT(p, 500);
    w.halt();
    EXPECT_FALSE(w.laeuft());
    EXPECT_FALSE(w.rueckmeldung().lauf);
    laufe(w, 100000);
    EXPECT_EQ(w.position(), p) << "ein /RESET versetzt den Wagen nicht";
    w.netzEin(42);
    EXPECT_EQ(w.position(), 42);
}

TEST(GeregelterWagen, TaktMeldetJedenWechselEinerRueckmeldung)
{
    GeregelterWagen w;
    w.netzEin(500);
    w.eingang(lauf(true));
    int wechsel = 0;
    for (int t = 0; t < 2 * w.parameter().abstand_druck + 100; ++t) wechsel += w.takt(1) ? 1 : 0;
    EXPECT_EQ(wechsel, 4) << "zwei Takt-Impulse: je steigende und fallende Flanke";
}
