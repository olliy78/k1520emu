/**
 * @file test_k6313_ccb.cpp
 * @brief K6313-CCB (Entwurf 49 WP8c): Speicherkarte (ROM 0000H-27FFH, RAM 3000H-33FFH und 3800H-3BFFH), ROM gegen die MD5 aus
 *        doc/EPROMS/K6313/README.md, E/A-Adressen "eine Leitung tief" (7CH SIO, DCH PIO D105, ECH CTC D102, F4H CTC D101,
 *        F8H PIO D104, BCH nicht bestueckt), Tastenmatrix (Waehlcode x Zeile) und Sensoren.
 *        Quelle fuer alle Zahlen: doc/printer/k6313/hardware.md (§2.2 E/A, §3 PIO-Belegung, §5 Matrix) und die Firmware.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/cards/printer/k6313_ccb.h"
#include "core/cards/printer/rom_k6313_33538013.h"

#ifndef K6313_ABZUG_DIR
#error "K6313_ABZUG_DIR fehlt (tests/unit/CMakeLists.txt)"
#endif

namespace {

struct Karte {
    K1520Bus bus;
    K6313Ccb ccb;
    explicit Karte(const K6313Ccb::Config& c = {}) : ccb(bus, c)
    {
        ccb.attachToBus(bus);
        ccb.powerOn();
    }
    uint8_t in(uint8_t p) { return bus.ioRead(p); }
    void out(uint8_t p, uint8_t d) { bus.ioWrite(p, d); }
    /// Spalte der Matrix waehlen (Bit 7...5 von DCH) und die Zeilen lesen.  D105 A steht dafuer in Modus 3 mit Eingabemaske 0FH
    /// (die Firmware richtet es bei 002AH ein).
    uint8_t zeilen(int spalte)
    {
        out(0xDE, 0xFF);   // Steuerwort: Modus 3
        out(0xDE, 0x0F);   // Ein-/Ausgabemaske: A0...A3 Eingang, A4...A7 Ausgang
        out(0xDC, static_cast<uint8_t>(spalte << 5));
        return static_cast<uint8_t>(in(0xDC) & 0x0F);
    }
};

std::vector<uint8_t> lies(const std::string& pfad)
{
    std::ifstream f(pfad, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST(K6313Ccb, EingebettetesRomIstDerAbzug33538013)
{
    const auto r = lies(std::string(K6313_ABZUG_DIR) + "/33538013.bin");
    ASSERT_EQ(r.size(), 10240u);
    ASSERT_EQ(sizeof K6313_ROM_33538013, r.size());
    for (size_t i = 0; i < r.size(); ++i) ASSERT_EQ(K6313_ROM_33538013[i], r[i]) << i;
    // ASCII-Kennung F ZS x nn PS am ROM-Ende: 3.35-3-80/13 -> "38033513" (README, Abschnitt Etikett und Kennung)
    EXPECT_EQ(std::string(r.begin() + 0x27F8, r.begin() + 0x2800), "38033513");
}

TEST(K6313Ccb, SpeicherplanKurzRom0000Ram3000Und3800)
{
    Karte k;
    for (int a = 0; a < 0x2800; a += 0x1F) EXPECT_EQ(k.bus.memRead(static_cast<uint16_t>(a)), K6313_ROM_33538013[a]) << a;
    EXPECT_EQ(k.bus.memRead(0x2800), 0xFF);   // unbelegt
    EXPECT_EQ(k.bus.memRead(0x37FF), 0xFF);
    k.bus.memWrite(0x3800, 0xA5);
    k.bus.memWrite(0x3BFF, 0x5A);
    EXPECT_EQ(k.bus.memRead(0x3800), 0xA5);
    EXPECT_EQ(k.bus.memRead(0x3BFF), 0x5A);
    EXPECT_EQ(k.bus.memRead(0x3C00), 0xFF);
    // zweites RAM-Fenster 3000H-33FFH (Punktdaten der Firmware, ISR 1F10H / sub_1934), dazwischen nichts (WP8d)
    k.bus.memWrite(0x3000, 0x11);
    k.bus.memWrite(0x33FF, 0x22);
    EXPECT_EQ(k.bus.memRead(0x3000), 0x11);
    EXPECT_EQ(k.bus.memRead(0x33FF), 0x22);
    EXPECT_EQ(k.bus.memRead(0x3800), 0xA5) << "die Fenster sind getrennt";
    k.bus.memWrite(0x3400, 0x33);
    EXPECT_EQ(k.bus.memRead(0x3400), 0xFF);
    EXPECT_EQ(k.bus.memRead(0x2FFF), 0xFF);
    k.bus.memWrite(0x0100, 0x00);
    EXPECT_EQ(k.bus.memRead(0x0100), K6313_ROM_33538013[0x100]) << "ROM ist nicht beschreibbar";
}

TEST(K6313Ccb, RomAusDateiHatVorrang)
{
    K6313Ccb::Config c;
    c.rom_extern.assign(K6313Ccb::ROM_ENDE, 0x42);
    Karte k(c);
    EXPECT_EQ(k.bus.memRead(0x0000), 0x42);
    EXPECT_EQ(k.bus.memRead(0x27FF), 0x42);
}

TEST(K6313Ccb, EAAdressenEineLeitungTief)
{
    Karte k;
    // PIO D104 (F8H) und D105 (DCH): nach dem Reset sind die Tore Eingaenge; Steuerwort 0FH = Modus 0 (Ausgabe)
    k.out(0xFA, 0x0F);
    k.out(0xFB, 0x0F);
    k.out(0xDF, 0x0F);
    k.out(0xF8, 0x5A);
    EXPECT_EQ(k.ccb.portAusgabe(104, 0), 0x5A);
    k.out(0xF9, 0xA5);
    EXPECT_EQ(k.ccb.portAusgabe(104, 1), 0xA5);
    // PIO D105 (DCH): A = DCH, B = DDH
    k.out(0xDD, 0x3C);
    EXPECT_EQ(k.ccb.portAusgabe(105, 1), 0x3C);
    // CTC D102 Kanal 2 (EEH) = Baudtakt: Zeitgeberbetrieb, Vorteiler 16, Zeitkonstante 1 (0613H-063CH: 17H, 01H = 9600 Bd)
    EXPECT_EQ(k.ccb.baudTakte(), 0u);
    k.out(0xEE, 0x17);
    k.out(0xEE, 0x01);
    EXPECT_EQ(k.ccb.baudTakte(), 16u);
    // SIO 7CH-7FH: WR2 ueber Kanal B (7FH) schreiben und nichts an den falschen Adressen
    k.out(0x7F, 0x02);
    k.out(0x7F, 0x10);
    // Centronics-PIO (BCH) ist auf der V.24-Kassette nicht bestueckt: offener Bus
    EXPECT_EQ(k.in(0xBC), 0xFF);
    EXPECT_EQ(k.in(0xBF), 0xFF);
    // sonst nichts: leere Tore lesen FFH
    EXPECT_EQ(k.in(0x00), 0xFF);
    EXPECT_EQ(k.in(0x80), 0xFF);   // die K6316-Karte liegt hier — die K6313 nicht
}

TEST(K6313Ccb, KassettenkennungPE1LiestNullBeiV24)
{
    // D104 B4 (PE1): die Firmware liest sie bei 0096H; V.24 = 0, Centronics = 1 (hardware.md §3)
    Karte k;
    k.out(0xFB, 0xFF);
    k.out(0xFB, 0x10);   // Modus 3, B4 Eingang (008EH-0094H)
    EXPECT_EQ(k.in(0xF9) & 0x10, 0);
}

TEST(K6313Ccb, PapierendeStehtAnD104A2)
{
    Karte k;
    k.out(0xFA, 0xFF);
    k.out(0xFA, 0x05);   // Modus 3, A0 und A2 Eingang (0070H-0080H)
    EXPECT_EQ(k.in(0xF8) & 0x04, 0) << "Papier da";
    k.ccb.setPapierDa(false);
    EXPECT_EQ(k.in(0xF8) & 0x04, 0x04) << "Papierende = 1";
    EXPECT_TRUE(k.ccb.lampe(4));
}

TEST(K6313Ccb, TastenmatrixDilSchalterUndTastenJeSpalteUndZeile)
{
    K6313Ccb::Config c;
    c.dil = 0;
    Karte k(c);
    for (int s = 0; s < 8; ++s) EXPECT_EQ(k.zeilen(s), 0) << "alles offen, Spalte " << s;
    // Geschlossener Schalter legt die Spalte auf seine Zeile (Auswertung im ROM: ON = 1)
    k.ccb.setDil(14, 2, true);   // 14-2 = Baud Bit 0: Spalte 5, Zeile 0 (ROM 0613H-063CH)
    EXPECT_EQ(k.zeilen(5), 0x01);
    EXPECT_EQ(k.zeilen(4), 0x00);
    k.ccb.setDil(15, 1, true);   // 15-1: Spalte 5, Zeile 3
    EXPECT_EQ(k.zeilen(5), 0x09);
    k.ccb.setDil(6, 1, true);    // 6-1 Papierendekontakt: Spalte 6, Zeile 1 (ROM 0074H)
    EXPECT_EQ(k.zeilen(6), 0x02);
    k.ccb.setDil(18, 2, true);   // 18-2 Stopbits: Spalte 3, Zeile 0 (ROM 0677H)
    EXPECT_EQ(k.zeilen(3), 0x01);
    k.ccb.setDil(9, 2, true);    // 9-2 (Yen): Spalte 7, Zeile 2 (ROM 0473H: Bit 6)
    EXPECT_EQ(k.zeilen(7), 0x04);
    k.ccb.setDil(14, 2, false);
    EXPECT_EQ(k.zeilen(5), 0x08);
    // Tasten: Spalte 1
    EXPECT_EQ(k.zeilen(1), 0);
    k.ccb.setTaste(1, true);   // ON/OFF LINE = Zeile 3
    EXPECT_EQ(k.zeilen(1), 0x08);
    k.ccb.setTaste(1, false);
    k.ccb.setTaste(2, true);   // LF vor = Zeile 1
    k.ccb.setTaste(4, true);   // FF = Zeile 2
    EXPECT_EQ(k.zeilen(1), 0x06);
    EXPECT_EQ(k.zeilen(0), 0) << "die Tasten haengen nur an der Spalte 1";
}

TEST(K6313Ccb, JederSchalterHatGenauEineZelleDerMatrix)
{
    int zaehler[K6313Ccb::DIL_ANZAHL] = {};
    int tasten[5] = {};
    for (int s = 0; s < 8; ++s)
        for (int z = 0; z < 4; ++z) {
            const int e = K6313Ccb::matrixZelle(s, z);
            if (e >= 0) { ASSERT_LT(e, K6313Ccb::DIL_ANZAHL); ++zaehler[e]; }
            else if (e >= -4) ++tasten[-e];
        }
    for (int i = 0; i < K6313Ccb::DIL_ANZAHL; ++i) EXPECT_EQ(zaehler[i], 1) << "Schalter " << i / 2 + 5 << "-" << i % 2 + 1;
    for (int t = 1; t <= 4; ++t) EXPECT_EQ(tasten[t], 1) << "Taste " << t;
}

TEST(K6313Ccb, VorgabeDilSetzt52Und132)
{
    Karte k;
    EXPECT_TRUE(k.ccb.dil(5, 2));
    EXPECT_TRUE(k.ccb.dil(13, 2));
    EXPECT_FALSE(k.ccb.dil(5, 1));
    EXPECT_EQ(__builtin_popcount(k.ccb.dilMaske()), 2);
    EXPECT_EQ(k.zeilen(3), 0x04) << "Normalstellung beim Einschalten: Spalte 3, Zeile 2 = 1, Zeile 3 = 0 (ROM 018CH)";
}

TEST(K6313Ccb, WagenphasenStehenAnD104AUndBewegenDenWagen)
{
    Karte k;
    const int start = k.ccb.druck().wagen().position();
    k.out(0xFA, 0x0F);   // D104 A: Modus 0 (Ausgabe)
    // Phasen /CM1...4 an A1, A3, A5, A7, Hochpegel = bestromt: eine Phase halten, dann im Kreis weiterschalten
    const uint8_t muster[] = {0x02, 0x08, 0x20, 0x80};
    for (int runde = 0; runde < 3; ++runde)
        for (uint8_t m : muster) k.out(0xF8, m);
    EXPECT_NE(k.ccb.druck().wagen().position(), start) << "der Wagen folgt den Phasen";
}

TEST(K6313Ccb, InterruptketteCtcD101VorCtcD102)
{
    Karte k;
    k.ccb.setIEI(true);
    // CTC D101 (F4H) Vektor 00H, D102 (ECH) Vektor 08H (0018H-0022H); beide Kanal 0 im Zeitgeberbetrieb mit Interrupt
    k.out(0xF4, 0x00);
    k.out(0xEC, 0x08);
    k.out(0xF4, 0xA5);   // Interrupt ein, Zeitgeber, Vorteiler 16, Zeitkonstante folgt
    k.out(0xF4, 0x02);
    k.out(0xEC, 0xA5);
    k.out(0xEC, 0x02);
    for (int i = 0; i < 200 && !k.ccb.hasInterrupt(); ++i) k.ccb.clockTick(16);
    ASSERT_TRUE(k.ccb.hasInterrupt());
    EXPECT_EQ(k.ccb.getVector(), 0x00) << "D101 hat Vorrang vor D102";
}

TEST(K6313Ccb, HcpVonD102Kanal0TaktetDieZaehlerkanaeleVonD101)
{
    // Schaltplan CCB: D102 ZC/TO0 (HCP, Draht 57) -> D101 C/TRG1 und C/TRG2.  Die Firmware zaehlt damit Spalten- und Schrittimpulse (WP8d).
    Karte k;
    k.out(0xEC, 0x17);   // D102 Kanal 0: Zeitgeber, Teiler 16, Zeitkonstante folgt
    k.out(0xEC, 4);      // 16 * 4 = 64 Takte je HCP-Periode
    k.out(0xF5, 0x47);   // D101 Kanal 1: Zaehler, fallende Flanke, Zeitkonstante folgt
    k.out(0xF5, 5);
    k.out(0xF6, 0x47);   // Kanal 2
    k.out(0xF6, 9);
    k.ccb.clockTick(64 * 3 + 10);
    EXPECT_EQ(k.ccb.ctc101().debugState().ch[1].counter, 5 - 3);
    EXPECT_EQ(k.ccb.ctc101().debugState().ch[2].counter, 9 - 3);
    k.ccb.clockTick(64 * 3);   // Kanal 1 laeuft ueber 0 und laedt nach
    EXPECT_EQ(k.ccb.ctc101().debugState().ch[1].counter, 5 - 6 + 5);
    EXPECT_EQ(k.ccb.ctc101().debugState().ch[2].counter, 9 - 6);
}
