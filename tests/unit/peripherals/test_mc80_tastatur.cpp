/**
 * @file test_mc80_tastatur.cpp
 * @brief Tastatur des MC80.3x auf Zeichenebene (AP M3): Praefixe, Zeichenabstand 9600 Bd 7O2,
 *        Anschlagabstand, Rueckstau bei vollem Empfaenger, Qt-Abbildung, Summer/Anzeige.
 *        Quelle: doc/mc80/egos.md §3.
 */

#include <gtest/gtest.h>
#include <vector>
#include "core/peripherals/mc80_tastatur/mc80_tastatur.h"

namespace {

struct Leitung {
    Mc80Tastatur t;
    std::vector<uint8_t> empfangen;
    std::vector<uint64_t> zeit;
    uint64_t jetzt = 0;
    bool frei = true;
    Leitung() {
        t.empfaengerFrei = [this] { return frei; };
        t.senden = [this](uint8_t b) { empfangen.push_back(b); zeit.push_back(jetzt); };
    }
    void lauf(uint64_t takte, int schritt = 16) {
        for (uint64_t i = 0; i < takte; i += static_cast<uint64_t>(schritt)) { jetzt += static_cast<uint64_t>(schritt); t.takt(schritt); }
    }
};

TEST(Mc80Tastatur, ZeichentastenMitPraefix1C) {
    Leitung l;
    l.t.tippe("a\n");
    l.lauf(200'000);
    EXPECT_EQ(l.empfangen, (std::vector<uint8_t>{0x1C, 'a', 0x1C, 0x11}));   // ENTER = 11H
    EXPECT_TRUE(l.t.leer());
}

TEST(Mc80Tastatur, ZeichenabstandUndAnschlagabstand) {
    Leitung l;
    l.t.tippe("ab");
    l.lauf(200'000, 1);
    ASSERT_EQ(l.empfangen.size(), 4u);
    // Praefix → Code: ein Zeichen (11 Bit × 256 Takte)
    EXPECT_EQ(l.zeit[1] - l.zeit[0], static_cast<uint64_t>(Mc80Tastatur::TAKTE_JE_ZEICHEN));
    // Code → naechster Anschlag: 20 ms [Annahme]
    EXPECT_EQ(l.zeit[2] - l.zeit[1], static_cast<uint64_t>(Mc80Tastatur::TAKTE_JE_TASTE));
}

TEST(Mc80Tastatur, VollerEmpfaengerStautZurueck) {
    Leitung l;
    l.frei = false;
    l.t.zeichen('x');
    l.lauf(100'000);
    EXPECT_TRUE(l.empfangen.empty());
    EXPECT_EQ(l.t.wartend(), 2u);
    l.frei = true;
    l.lauf(10'000);
    EXPECT_EQ(l.empfangen, (std::vector<uint8_t>{0x1C, 'x'}));
}

TEST(Mc80Tastatur, SondertastenAlsBitmaske) {
    Leitung l;
    l.t.sondertaste(0);
    l.t.sondertaste(6);
    l.t.sondertaste(7);
    l.t.sondertaste(8);   // gibt es nicht
    l.lauf(300'000);
    EXPECT_EQ(l.empfangen, (std::vector<uint8_t>{0x1E, 0x40, 0x1E, 0x01, 0x1E, 0x00}));
}

TEST(Mc80Tastatur, QtAbbildung) {
    Leitung l;
    EXPECT_TRUE(l.t.qtTaste('A', false, false));           // Kappe ohne Umschalter: klein
    EXPECT_TRUE(l.t.qtTaste('A', true, false));            // mit: gross
    EXPECT_TRUE(l.t.qtTaste('C', false, true));            // Strg+C = 1FH + Kappe [?]
    EXPECT_TRUE(l.t.qtTaste(0x01000004, false, false));    // Return = ENTER
    EXPECT_TRUE(l.t.qtTaste(0x01000000, false, false));    // Escape = OFF
    EXPECT_TRUE(l.t.qtTaste(0x01000031, false, false));    // F2 = Sondertaste 1
    EXPECT_TRUE(l.t.qtTaste('1', true, false));            // Ziffern/Zeichen unveraendert
    EXPECT_FALSE(l.t.qtTaste(0x01000020, false, false));   // Shift allein: nichts
    l.lauf(1'000'000);
    EXPECT_EQ(l.empfangen, (std::vector<uint8_t>{0x1C, 'a', 0x1C, 'A', 0x1F, 'C', 0x1C, 0x11,
                                                 0x1C, 0x13, 0x1E, 0x20, 0x1C, '1'}));
}

// Codes der Nicht-Zeichentasten laut Forum robotrontechnik.de threadid=7918.
TEST(Mc80Tastatur, KursorCLUndZiffernblockCodes) {
    Leitung l;
    for (uint32_t k : {0x01000013u, 0x01000015u, 0x01000012u, 0x01000014u, 0x01000007u,
                       Mc80Tastatur::TASTE_CRLF})
        EXPECT_TRUE(l.t.qtTaste(k, false, false));
    EXPECT_TRUE(l.t.qtTaste(Mc80Tastatur::TASTE_ZB_CL, false, false));
    EXPECT_TRUE(l.t.qtTaste(Mc80Tastatur::TASTE_ZB_LEER, false, false));
    EXPECT_TRUE(l.t.qtTaste(Mc80Tastatur::TASTE_ZB_ZEICHEN + '7', false, false));
    EXPECT_TRUE(l.t.qtTaste(Mc80Tastatur::TASTE_ZB_00, false, false));
    l.lauf(3'000'000);
    EXPECT_EQ(l.empfangen, (std::vector<uint8_t>{
        0x1C, 0x0B, 0x1C, 0x0A, 0x1C, 0x08, 0x1C, 0x0C, 0x1C, 0x7F, 0x1C, 0x0D,   // ↑ ↓ ← → CL CRLF
        0x1D, 0x7F, 0x1D, 0x20, 0x1D, '7', 0x1D, '0', 0x1D, '0'}));
}

TEST(Mc80Tastatur, SummerUndDauertonVomRechner) {
    Mc80Tastatur t;
    t.vomRechner(0x07);
    t.vomRechner(0x87);   // 7 Bit
    EXPECT_EQ(t.summerZaehler(), 2u);
    t.vomRechner(0x10);
    EXPECT_TRUE(t.dauerton());
    t.vomRechner(0x07);   // Piep schaltet den Dauerton aus
    EXPECT_FALSE(t.dauerton());
}

TEST(Mc80Tastatur, SperreUndQuittungen) {
    Leitung l;
    EXPECT_TRUE(l.t.bereit());
    l.t.vomRechner(0x04);                       // Matrix sperren → NAK
    EXPECT_FALSE(l.t.bereit());
    EXPECT_FALSE(l.t.qtTaste('a', false, false)) << "gesperrte Matrix sendet nichts";
    l.t.vomRechner(0x05);                       // freigeben → ACK
    EXPECT_TRUE(l.t.bereit());
    EXPECT_TRUE(l.t.qtTaste('a', false, false));
    l.t.vomRechner(0x41);                       // unbekanntes Wort → CAN
    l.t.vomRechner(0x14);                       // Autorepeat: wirkungslos, keine Antwort
    l.lauf(600'000);
    EXPECT_EQ(l.empfangen, (std::vector<uint8_t>{0x15, 0x06, 0x1C, 'a', 0x18}));
}

TEST(Mc80Tastatur, UmschaltungSperrenMit0Eund0F) {
    Leitung l;
    l.t.vomRechner(0x0E);
    EXPECT_TRUE(l.t.umschaltungGesperrt());
    EXPECT_TRUE(l.t.qtTaste('a', true, false));   // Shift wirkt nicht
    l.t.vomRechner(0x0F);
    EXPECT_TRUE(l.t.qtTaste('a', true, false));
    l.lauf(600'000);
    EXPECT_EQ(l.empfangen, (std::vector<uint8_t>{0x1C, 'a', 0x1C, 'A'}));
}

TEST(Mc80Tastatur, ResetLeertDieWarteschlange) {
    Leitung l;
    l.t.tippe("abc");
    l.t.reset();
    l.lauf(500'000);
    EXPECT_TRUE(l.empfangen.empty());
}

}  // namespace
