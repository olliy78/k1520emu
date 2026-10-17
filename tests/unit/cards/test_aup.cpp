/**
 * @file test_aup.cpp
 * @brief Unit-Tests der AUP 062-8531, Tastaturteil (Registermodell, Entwurf 33 N10c).
 *
 * Geprueft: Anmeldung 40H-43H, Gueltigkeit/Tastencode (Lesen loescht), Entprellzeit, Schlange,
 * Kommandobyte (Lampen, Hupe, FFH, Kennung), /RESET, Alpha-Umschaltung (Annahme A3),
 * Dauerfunktion, Schluesselschalter — und als Waechter ALLE 72 + 16 Tabelleneintraege gegen eine
 * zweite, unabhaengig aus dem Beleg abgeschriebene Fassung (doc/a5130_k8924/aup_analyse.md §1.5).
 */

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "core/bus/k1520_bus.h"
#include "core/cards/aup/aup.h"

namespace {
using T = AUP::Tastatur;

struct Pruefstand {
    uint64_t uhr = 0;
    K1520Bus bus;
    AUP aup;
    explicit Pruefstand(AUP::Alpha a = AUP::Alpha::NachLed) : aup(cfg(a)) { aup.attachToBus(bus); }
    AUP::Config cfg(AUP::Alpha a) {
        AUP::Config c;
        c.uhr = &uhr;
        c.alpha = a;
        return c;
    }
    uint64_t ms(int n) const { return uint64_t(aup.config().takt_hz) * n / 1000; }
    void warte(int n_ms) { uhr += ms(n_ms); }
    uint8_t in(uint8_t p) { return bus.ioRead(p); }
    void out(uint8_t p, uint8_t d) { bus.ioWrite(p, d); }
    /// Taste antippen, ersten Gastblick (stempelt), Entprellzeit abwarten.
    void tippe(T t, int pos) {
        aup.taste(t, pos, true);
        aup.taste(t, pos, false);
        in(0x41);
        warte(20);
    }
};
}  // namespace

TEST(AUP, BelegtGenau40Bis43) {
    K1520Bus bus;
    AUP a;
    a.attachToBus(bus);
    for (int p = 0x40; p <= 0x43; ++p) EXPECT_EQ(bus.ioOwner(uint8_t(p)), &a) << p;
    EXPECT_EQ(bus.ioOwner(0x3F), nullptr);
    EXPECT_EQ(bus.ioOwner(0x44), nullptr);
}

TEST(AUP, RuhezustandUndUnbeschriebeneTore) {
    Pruefstand s;
    EXPECT_EQ(s.in(0x41), 0x00);
    EXPECT_EQ(s.in(0x40), 0xFF) << "Annahme A2";
    EXPECT_EQ(s.in(0x43), 0xFF) << "Annahme A2";
    EXPECT_EQ(s.aup.kommando(), 0xFF);
    EXPECT_EQ(s.aup.lampen(), 0);
}

TEST(AUP, CodeErstNachEntprellzeitUndLesenLoescht) {
    Pruefstand s;
    ASSERT_TRUE(s.aup.taste(T::K7633, 0x12, true));   // A08 = '0'
    EXPECT_EQ(s.in(0x41), 0x00) << "Zeitstempel beim ersten Blick, Entprellung laeuft";
    s.warte(10);
    EXPECT_EQ(s.in(0x41), 0x00) << "nach 10 ms noch nicht (Annahme A4: 16 ms)";
    s.warte(10);
    EXPECT_EQ(s.in(0x41), 0x01) << "Bit 0 = K7633 (Annahme A1)";
    EXPECT_EQ(s.in(0x42), 0x30);
    EXPECT_EQ(s.in(0x41), 0x00) << "IN 42 loescht die Gueltigkeit";
    EXPECT_EQ(s.in(0x42), 0x30) << "A6 behaelt den Code";
    s.aup.taste(T::K7633, 0x12, false);
    s.warte(100);
    EXPECT_EQ(s.in(0x41), 0x00) << "Loslassen sendet nichts";
}

TEST(AUP, KundentastaturHatEigenesBit) {
    Pruefstand s;
    s.tippe(T::Kunde, 0x40);   // '5'
    EXPECT_EQ(s.in(0x41), 0x02);
    EXPECT_EQ(s.in(0x42), 0x35);
}

TEST(AUP, SchlangeLiefertNacheinander) {
    Pruefstand s;
    s.aup.taste(T::Kunde, 0x42, true);  s.aup.taste(T::Kunde, 0x42, false);   // '1'
    s.aup.taste(T::K7633, 0x47, true);  s.aup.taste(T::K7633, 0x47, false);   // F01
    s.aup.taste(T::Kunde, 0x41, true);  s.aup.taste(T::Kunde, 0x41, false);   // '2'
    s.in(0x41);
    s.warte(20);
    const std::pair<uint8_t, uint8_t> soll[] = {{0x02, 0x31}, {0x01, 0xC0}, {0x02, 0x32}};
    for (auto [bit, code] : soll) {
        EXPECT_EQ(s.in(0x41), bit);
        EXPECT_EQ(s.in(0x42), code);
    }
    EXPECT_EQ(s.in(0x41), 0x00);
}

TEST(AUP, OhneAbholenBleibtDerCodeStehen) {
    Pruefstand s;
    s.tippe(T::K7633, 0x13);
    s.aup.taste(T::K7633, 0x14, true);
    s.aup.taste(T::K7633, 0x14, false);
    s.warte(100);
    EXPECT_EQ(s.in(0x41), 0x01);
    EXPECT_EQ(s.in(0x42), 0x31) << "der erste bleibt, bis er abgeholt ist";
    s.warte(20);
    EXPECT_EQ(s.in(0x41), 0x01);
    EXPECT_EQ(s.in(0x42), 0x34);
}

TEST(AUP, GehalteneTasteSendetNurEinmal) {
    Pruefstand s;
    s.aup.taste(T::K7633, 0x15, true);
    s.aup.taste(T::K7633, 0x15, true);   // Wiederholung des Wirts ohne Loslassen
    s.in(0x41);
    s.warte(2000);
    EXPECT_EQ(s.in(0x42), 0x37);
    s.warte(2000);
    EXPECT_EQ(s.in(0x41), 0x00) << "keine Dauerfunktionstaste: kein zweiter Code";
}

TEST(AUP, PositionOhneTasteOderOhneCode) {
    Pruefstand s;
    EXPECT_FALSE(s.aup.taste(T::K7633, 0x10, true)) << "'-' im Beleg";
    EXPECT_FALSE(s.aup.taste(T::K7633, 0x48, true));
    EXPECT_FALSE(s.aup.taste(T::Kunde, 0x38, true));
    EXPECT_FALSE(s.aup.taste(T::Kunde, 0x37, true));
    EXPECT_TRUE(s.aup.taste(T::K7633, 0x1B, true)) << "B07 hat einen Namen";
    s.in(0x41);
    s.warte(100);
    EXPECT_EQ(s.in(0x41), 0x00) << "Code 00H liefert nichts (Annahme A7)";
}

TEST(AUP, KommandobyteSteuertLampen) {
    Pruefstand s;
    s.out(0x43, 0xFE);
    EXPECT_EQ(s.aup.lampen(), AUP::kLampeSel);
    s.out(0x43, 0xFD);
    EXPECT_EQ(s.aup.lampen(), AUP::kLampeAlpha);
    s.out(0x43, 0xEF);
    EXPECT_EQ(s.aup.lampen(), AUP::kLampeGet);
    s.out(0x43, 0xDF);
    EXPECT_EQ(s.aup.lampen(), AUP::kLampeFehler);
    s.out(0x43, 0xCC);   // 1100 1100: Hupe aus, alle vier an
    EXPECT_EQ(s.aup.lampen(), AUP::kLampeSel | AUP::kLampeAlpha | AUP::kLampeGet | AUP::kLampeFehler);
    EXPECT_EQ(s.aup.kommando(), 0xCC);
    s.out(0x40, 0x00);
    s.out(0x41, 0x00);
    s.out(0x42, 0x00);
    EXPECT_EQ(s.aup.kommando(), 0xCC) << "40H..42H wirkungslos (Annahme A2)";
}

TEST(AUP, HupeJeKommandoMitBit6Null) {
    Pruefstand s;
    EXPECT_EQ(s.aup.hupe(), 0u);
    s.out(0x43, 0xBF);
    s.out(0x43, 0xBF);
    s.out(0x43, 0xFE);
    EXPECT_EQ(s.aup.hupe(), 2u) << "Annahme A5";
    EXPECT_FALSE(s.aup.hupeAn());
}

TEST(AUP, KommandoOhneKennungWirdVerworfen) {
    Pruefstand s;
    s.out(0x43, 0xFE);
    s.out(0x43, 0x00);
    EXPECT_EQ(s.aup.kommando(), 0xFE) << "Annahme A6";
    EXPECT_EQ(s.aup.verworfen(), 1u);
    EXPECT_EQ(s.aup.hupe(), 0u);
}

TEST(AUP, FFhLoeschtGueltigkeitUndSchlange) {
    Pruefstand s;
    s.out(0x43, 0xCC);
    s.tippe(T::K7633, 0x13);
    s.aup.taste(T::K7633, 0x14, true);
    s.aup.taste(T::K7633, 0x14, false);
    s.out(0x43, 0xFF);
    EXPECT_EQ(s.aup.lampen(), 0);
    s.warte(100);
    EXPECT_EQ(s.in(0x41), 0x00);
    s.tippe(T::K7633, 0x15);
    EXPECT_EQ(s.in(0x41), 0x01) << "nach dem Reset wieder bereit";
    EXPECT_EQ(s.in(0x42), 0x37);
}

TEST(AUP, ResetWieFFh) {
    Pruefstand s;
    s.out(0x43, 0xCC);
    s.tippe(T::K7633, 0x13);
    s.aup.reset();
    EXPECT_EQ(s.aup.kommando(), 0xFF);
    EXPECT_EQ(s.in(0x41), 0x00);
}

TEST(AUP, AlphaFolgtDerLedAnnahmeA3) {
    Pruefstand s;
    s.tippe(T::K7633, 0x47);
    EXPECT_EQ(s.in(0x42), 0xC0) << "ALPHA-LED aus: ohne Alpha";
    s.out(0x43, 0xFD);
    EXPECT_TRUE(s.aup.alphaAktiv());
    s.tippe(T::K7633, 0x47);
    EXPECT_EQ(s.in(0x42), 0x41) << "ALPHA-LED an: mit Alpha ('A')";
    s.tippe(T::K7633, 0x12);
    EXPECT_EQ(s.in(0x42), 0x30) << "Ziffern gleich";
}

TEST(AUP, AlphaFestEingestellt) {
    Pruefstand ohne(AUP::Alpha::Ohne), mit(AUP::Alpha::Mit);
    ohne.out(0x43, 0xFD);
    ohne.tippe(T::K7633, 0x2A);
    EXPECT_EQ(ohne.in(0x42), 0xDB);
    mit.tippe(T::K7633, 0x2A);
    EXPECT_EQ(mit.in(0x42), 0x59);   // 'Y'
}

TEST(AUP, DauerfunktionWiederholt) {
    Pruefstand s;
    ASSERT_TRUE(AUP::dauerfunktion(T::K7633, 0x1A));
    s.aup.taste(T::K7633, 0x1A, true);   // A07 = 20H
    s.in(0x41);
    s.warte(20);
    EXPECT_EQ(s.in(0x41), 0x01);
    EXPECT_EQ(s.in(0x42), 0x20);
    s.warte(300);
    EXPECT_EQ(s.in(0x41), 0x00) << "vor der ersten Wiederholung (500 ms)";
    s.warte(300);
    EXPECT_EQ(s.in(0x41), 0x01);
    EXPECT_EQ(s.in(0x42), 0x20);
    int n = 0;
    for (int i = 0; i < 10; ++i) {   // 1 s: alle 100 ms eine
        s.warte(100);
        if (s.in(0x41)) { EXPECT_EQ(s.in(0x42), 0x20); ++n; }
    }
    EXPECT_EQ(n, 10);
    s.aup.taste(T::K7633, 0x1A, false);
    s.warte(1000);
    EXPECT_EQ(s.in(0x41), 0x00);
}

TEST(AUP, SchluesselschalterSendenBeimEinschalten) {
    Pruefstand s;
    EXPECT_FALSE(s.aup.schluesselAn(1));
    ASSERT_TRUE(s.aup.schluessel(1, true));
    EXPECT_TRUE(s.aup.schluesselAn(1)) << "sofort, ohne Gastzugriff";
    s.in(0x41);
    s.warte(20);
    EXPECT_EQ(s.in(0x42), 0xAA);
    s.aup.schluessel(2, true);
    s.in(0x41);
    s.warte(20);
    EXPECT_EQ(s.in(0x42), 0xA9);
    s.aup.schluessel(1, false);
    EXPECT_FALSE(s.aup.schluesselAn(1));
    EXPECT_TRUE(s.aup.schluesselAn(2));
    s.warte(100);
    EXPECT_EQ(s.in(0x41), 0x00) << "Ausschalten sendet nichts (Annahme A7)";
    EXPECT_FALSE(s.aup.schluessel(3, true));
}

// ─── Waechter: Codetabellen gegen eine zweite Abschrift des Belegs ─────────────

// Spaltenweise wie im Beleg (AUP S. 6-7): Taste, Position, Code ohne/mit Alpha; "-" = keine Taste.
static const char* const kBelegK7633 =
    "SS1 00 AA AA  SS2 01 A9 A9  A10 02 12 12  B10 03 33 33  C10 04 36 36  D10 05 39 39 "
    "E10 06 0A 0A  F10 07 A0 A0  A13 08 FC FC  C13 09 FA FA  A09 0A 11 11  B09 0B 32 32 "
    "C09 0C 35 35  D09 0D 38 38  E09 0E 07 07  F09 0F 01 01  -   10 00 00  F12 11 AF AF "
    "A08 12 30 30  B08 13 31 31  C08 14 34 34  D08 15 37 37  E08 16 06 06  F08 17 0B 0B "
    "E13 18 F8 F8  E12 19 DD DD  A07 1A 20 20  B07 1B 00 00  C07 1C 2D 2D "
    "D07 1D 19 19  E07 1E 0C 0C  F07 1F 10 10  D13 20 F9 F9  D12 21 FD FD  A05 22 DC DC "
    "B05 23 D7 55  C05 24 D2 2C  D05 25 CD 2E  E05 26 C8 2F  F05 27 A1 A1  B13 28 FB FB "
    "B12 29 00 00  A04 2A DB 59  B04 2B D6 54  C04 2C D1 50  D04 2D CC 4C  E04 2E C7 48 "
    "F04 2F C3 44  -   30 00 00  -   31 00 00  A03 32 DA 58  B03 33 D5 53  C03 34 D0 4F "
    "D03 35 CB 4B  E03 36 C6 47  F03 37 C2 43  -   38 00 00  -   39 00 00  A02 3A D9 57 "
    "B02 3B D4 52  C02 3C CF 4E  D02 3D CA 4A  E02 3E C5 46  F02 3F C1 42  A12 40 FF FF "
    "C12 41 FE FE  A01 42 D8 56  B01 43 D3 51  C01 44 CE 4D  D01 45 C9 49  E01 46 C4 45 "
    "F01 47 C0 41";

TEST(AUPTabelle, K7633AlleZweiundsiebzigPositionen) {
    std::istringstream is(kBelegK7633);
    std::string name, pos, ohne, mit;
    int n = 0;
    while (is >> name >> pos >> ohne >> mit) {
        const int p = std::stoi(pos, nullptr, 16);
        EXPECT_EQ(p, n) << "Abschrift lueckenlos";
        if (name == "-") {
            EXPECT_EQ(AUP::code(T::K7633, p, false), -1) << pos;
            EXPECT_STREQ(AUP::name(T::K7633, p), "") << pos;
        } else {
            EXPECT_STREQ(AUP::name(T::K7633, p), name.c_str()) << pos;
            EXPECT_EQ(AUP::code(T::K7633, p, false), std::stoi(ohne, nullptr, 16)) << name;
            EXPECT_EQ(AUP::code(T::K7633, p, true), std::stoi(mit, nullptr, 16)) << name;
        }
        ++n;
    }
    EXPECT_EQ(n, 72);
    EXPECT_EQ(AUP::code(T::K7633, 0x48, false), -1);
    EXPECT_EQ(AUP::code(T::K7633, -1, false), -1);
}

TEST(AUPTabelle, KundentastaturAlleSechzehnPositionen) {
    // Beleg: 38-3B -> 00, 3C -> 36, 3D -> 19, 3E -> 30, 3F -> 19, 40 -> 35, 41 -> 32, 42 -> 31,
    //        43 -> 34, 44 -> 33, 45 -> 39, 46 -> 38, 47 -> 37.
    const int soll[16] = {-1, -1, -1, -1, 0x36, 0x19, 0x30, 0x19,
                          0x35, 0x32, 0x31, 0x34, 0x33, 0x39, 0x38, 0x37};
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(AUP::code(T::Kunde, 0x38 + i, false), soll[i]) << std::hex << 0x38 + i;
        EXPECT_EQ(AUP::code(T::Kunde, 0x38 + i, true), soll[i]) << "eine Tabelle";
    }
    for (int p = 0x3C; p <= 0x47; ++p) {
        const int c = AUP::code(T::Kunde, p, false);
        if (c >= 0x30 && c <= 0x39) EXPECT_EQ(std::string(AUP::name(T::Kunde, p)), std::string(1, char(c)));
    }
    EXPECT_EQ(AUP::code(T::Kunde, 0x37, false), -1);
    EXPECT_EQ(AUP::code(T::Kunde, 0x48, false), -1);
}

TEST(AUPTabelle, DauerfunktionstastenLautEprom) {
    for (int p = 0; p < 0x48; ++p)
        EXPECT_EQ(AUP::dauerfunktion(T::K7633, p), p == 0x1A || p == 0x16 || p == 0x0E) << p;
    EXPECT_FALSE(AUP::dauerfunktion(T::Kunde, 0x40));
}

// ─── Sonderschnittstelle (N10d): CTC B0H-B3H, SIO B4H-B7H ────────────────────
namespace {
struct SonderStand {
    uint64_t uhr = 0;
    K1520Bus bus;
    AUP aup;
    SonderStand() : aup(cfg()) { aup.attachToBus(bus); bus.setInterruptChain({&aup}); }
    static AUP::Config cfg() {
        AUP::Config c;
        c.sonder = true;
        return c;
    }
    uint8_t in(uint8_t p) { return bus.ioRead(p); }
    void out(uint8_t p, uint8_t d) { bus.ioWrite(p, d); }
    void sioB(uint8_t reg, uint8_t wert) { out(0xB7, reg); out(0xB7, wert); }
};
}  // namespace

TEST(AUPSonder, OhneOptionSindDieToreB0bisB7Frei) {
    Pruefstand p;
    EXPECT_FALSE(p.aup.sonder());
    EXPECT_EQ(p.aup.anschluss(), nullptr);
    EXPECT_EQ(p.aup.ctc(), nullptr);
    EXPECT_FALSE(p.aup.clockTick(1000));
    for (int port = 0xB0; port <= 0xB7; ++port) EXPECT_EQ(p.bus.ioOwner(uint8_t(port)), nullptr) << port;
    EXPECT_FALSE(p.aup.hasInterrupt());
    EXPECT_TRUE(p.aup.getVector() == 0xFF);
}

TEST(AUPSonder, MeldetAchtToreAn) {
    SonderStand s;
    for (int port = 0xB0; port <= 0xB7; ++port) EXPECT_EQ(s.bus.ioOwner(uint8_t(port)), &s.aup) << port;
    for (int port = 0x40; port <= 0x43; ++port) EXPECT_EQ(s.bus.ioOwner(uint8_t(port)), &s.aup) << port;
    EXPECT_EQ(s.bus.ioOwner(0xB8), nullptr);
    EXPECT_EQ(s.bus.ioOwner(0xAF), nullptr);
}

TEST(AUPSonder, CtcAufB0BisB3) {
    SonderStand s;
    // Kanal 1 (B1): Zeitgeber, Vorteiler 16, Zeitkonstante folgt; Konstante 9.
    s.out(0xB1, 0x05);
    s.out(0xB1, 0x09);
    EXPECT_TRUE(s.aup.ctc()->isTimerMode(1));
    EXPECT_EQ(s.aup.ctc()->teilerTakte(1), 16u * 9u);
    // Die anderen Kanaele sind unberuehrt (kein Zeitgeber, nicht laufend).
    EXPECT_EQ(s.aup.ctc()->teilerTakte(0), 0u);
    // Kanal 0 (B0): Konstante 1 -> 16 Takte je ZC/TO; die Karte hat KEINE zweite CTC auf anderen Toren.
    s.out(0xB0, 0x05);
    s.out(0xB0, 0x01);
    EXPECT_EQ(s.aup.ctc()->teilerTakte(0), 16u);
}

TEST(AUPSonder, SioAufB4BisB7) {
    SonderStand s;
    // B7 = Steuer B (B6 Daten B): WR4 setzen, am Baustein ablesbar; RR0 liest Tx-leer (Bit 2).
    s.sioB(4, 0x44);
    EXPECT_EQ(s.aup.sio()->channelB().format().teiler, 16);
    EXPECT_EQ(s.aup.sio()->channelB().format().stopp_halbe, 2);
    EXPECT_EQ(s.in(0xB7) & 0x04, 0x04);
    // B5 = Steuer A: Kanal A bleibt davon unberuehrt.
    EXPECT_EQ(s.aup.sio()->channelA().format().teiler, 1);
    s.out(0xB5, 4);
    s.out(0xB5, 0xC4);   // x64
    EXPECT_EQ(s.aup.sio()->channelA().format().teiler, 64);
    EXPECT_EQ(s.aup.sio()->channelB().format().teiler, 16);
    // B6 = Daten B: ein Empfangsbyte (Kanal B) kommt dort an, nicht an B4.
    s.sioB(3, 0xC1);
    s.aup.anschluss()->empfange(0x5A);
    EXPECT_EQ(s.in(0xB7) & 1, 1);
    EXPECT_EQ(s.in(0xB6), 0x5A);
    EXPECT_EQ(s.in(0xB7) & 1, 0);
}

TEST(AUPSonder, TastaturtoreBleibenUnberuehrt) {
    SonderStand s;
    EXPECT_EQ(s.in(0x41), 0x00);
    EXPECT_EQ(s.in(0x40), 0xFF);
    s.out(0x43, 0xCC);
    EXPECT_EQ(s.aup.kommando(), 0xCC);
    EXPECT_EQ(s.aup.sio()->channelB().format().teiler, 1);
}

TEST(AUPSonder, AnschlussNameUndFormatAusSioUndCtc) {
    SonderStand s;
    auto* a = s.aup.anschluss();
    ASSERT_NE(a, nullptr);
    EXPECT_STREQ(a->name(), "Sonderschnittstelle");
    EXPECT_STREQ(a->stecker(), "X2");
    EXPECT_FALSE(a->v24());
    EXPECT_TRUE(a->taktquellen().empty());
    EXPECT_FALSE(a->format().gueltig) << "CTC noch nicht programmiert";
    // 9600 Bd 8N1: CTC Kanal 0 Vorteiler 16 x TC 1, SIO x16.
    s.out(0xB0, 0x05);
    s.out(0xB0, 0x01);
    s.sioB(4, 0x44);
    s.sioB(3, 0xC1);
    s.sioB(5, 0x68);
    const auto f = a->format();
    EXPECT_TRUE(f.gueltig);
    EXPECT_EQ(f.baud_nenn, 9600u);
    EXPECT_EQ(f.daten, 8);
    EXPECT_EQ(f.paritaet, 0);
    EXPECT_EQ(f.stopp_halbe, 2);
    EXPECT_EQ(f.zeichen_takte, 10u * 16u * 16u);
}

TEST(AUPSonder, SendenLiefertAmAnschlussDieProgrammiertenBits) {
    SonderStand s;
    s.sioB(3, 0xC1);
    s.sioB(5, 0x68);   // 8 Bit, Senden frei
    s.out(0xB6, 0xA7);
    auto* a = s.aup.anschluss();
    ASSERT_TRUE(a->senderHatZeichen());
    EXPECT_EQ(a->senderNimm(), 0xA7);
    EXPECT_TRUE(s.aup.nimmSeriellGeaendert());
    EXPECT_FALSE(s.aup.nimmSeriellGeaendert()) << "liest und loescht";
    s.sioB(5, 0x28);   // 7 Bit
    s.out(0xB6, 0xA7);
    EXPECT_EQ(a->senderNimm(), 0x27);
}

TEST(AUPSonder, RtsDtrUndBreakAmAnschluss) {
    SonderStand s;
    auto* a = s.aup.anschluss();
    EXPECT_FALSE(a->rts());
    EXPECT_FALSE(a->dtr());
    s.sioB(5, 0xEA);   // DTR + 8 Bit + Senden + RTS
    EXPECT_TRUE(a->rts());
    EXPECT_TRUE(a->dtr());
    EXPECT_FALSE(a->breakGesendet());
    s.sioB(5, 0xFA);
    EXPECT_TRUE(a->breakGesendet());
}

// Interrupt-Auslieferung: Vektor vom SIO, CTC dahinter, RETI gibt frei.
TEST(AUPSonder, SioInterruptWirdMitVektorAusgeliefert) {
    SonderStand s;
    s.sioB(2, 0x20);   // WR2 Kanal B: Basisvektor
    s.sioB(1, 0x18);   // WR1: Rx-Interrupt bei jedem Zeichen
    s.sioB(3, 0xC1);
    s.bus.markIntDirty();
    s.bus.updateInterruptChain();
    EXPECT_FALSE(s.bus.isINT());
    s.aup.anschluss()->empfange(0x41);
    s.bus.markIntDirty();
    s.bus.updateInterruptChain();
    EXPECT_TRUE(s.aup.hasInterrupt());
    EXPECT_TRUE(s.bus.isINT());
    EXPECT_STREQ(s.aup.intDeviceName(), "AUP SIO-A13 (Sonderschnittstelle)");
    const uint8_t vec = s.bus.interruptAcknowledge();
    EXPECT_EQ(vec & 0xF1, 0x20) << std::hex << int(vec);
    EXPECT_EQ(s.in(0xB6), 0x41);          // Zeichen lesen
    s.bus.signalRETI();
    s.bus.markIntDirty();
    s.bus.updateInterruptChain();
    EXPECT_FALSE(s.aup.hasInterrupt());
    EXPECT_TRUE(s.aup.getIEO()) << "nach RETI frei";
    EXPECT_TRUE(s.bus.isINT() == false);
}

TEST(AUPSonder, CtcInterruptHinterDemSio) {
    SonderStand s;
    s.out(0xB0, 0x40);   // CTC-Vektor-Basis 40H (Bit 0 = 0)
    s.out(0xB1, 0x85);   // Kanal 1: Interrupt frei, Zeitgeber, Vorteiler 16, Konstante folgt
    s.out(0xB1, 0x02);
    s.sioB(2, 0x20);
    s.sioB(1, 0x18);
    s.sioB(3, 0xC1);
    EXPECT_TRUE(s.aup.clockTick(64));    // ZC/TO-Flanke gemeldet
    s.bus.markIntDirty();
    s.bus.updateInterruptChain();
    EXPECT_TRUE(s.aup.hasInterrupt());
    EXPECT_STREQ(s.aup.intDeviceName(), "AUP CTC-A12");
    // SIO meldet sich zusaetzlich: hat Vorrang (SIO vor CTC).
    s.aup.anschluss()->empfange(0x42);
    s.bus.markIntDirty();
    s.bus.updateInterruptChain();
    EXPECT_STREQ(s.aup.intDeviceName(), "AUP SIO-A13 (Sonderschnittstelle)");
    EXPECT_EQ(s.bus.interruptAcknowledge() & 0xF1, 0x20);   // getVector() quittiert: nur einmal rufen
    s.in(0xB6);
    s.bus.signalRETI();
    s.bus.markIntDirty();
    s.bus.updateInterruptChain();
    // Jetzt ist die CTC dran: Basis 40H | Kanal 1 << 1.
    EXPECT_TRUE(s.aup.hasInterrupt());
    EXPECT_EQ(s.bus.interruptAcknowledge(), 0x42);
    s.bus.signalRETI();
    s.bus.markIntDirty();
    s.bus.updateInterruptChain();
    EXPECT_FALSE(s.aup.hasInterrupt());
}

TEST(AUPSonder, IeiSperrtDieKarte) {
    SonderStand s;
    s.sioB(2, 0x20);
    s.sioB(1, 0x18);
    s.sioB(3, 0xC1);
    s.aup.anschluss()->empfange(0x41);
    s.aup.setIEI(false);
    EXPECT_FALSE(s.aup.hasInterrupt()) << "ohne /IEI keine Anforderung";
    EXPECT_FALSE(s.aup.getIEO());
    s.aup.setIEI(true);
    EXPECT_TRUE(s.aup.hasInterrupt());
}

TEST(AUPSonder, OhneSonderschnittstelleIstDieKarteInDerKetteDurchsichtig) {
    Pruefstand p;
    p.aup.setIEI(true);
    EXPECT_TRUE(p.aup.getIEO());
    p.aup.setIEI(false);
    EXPECT_FALSE(p.aup.getIEO());
    EXPECT_FALSE(p.aup.hasInterrupt());
}

TEST(AUPSonder, ResetSetztCtcUndSioZurueck) {
    SonderStand s;
    s.sioB(4, 0x44);
    s.out(0xB0, 0x05);
    s.out(0xB0, 0x01);
    s.aup.reset();
    EXPECT_EQ(s.aup.sio()->channelB().format().teiler, 1);
    EXPECT_FALSE(s.aup.ctc()->isTimerMode(0) && s.aup.ctc()->teilerTakte(0) != 0);
    EXPECT_EQ(s.aup.kommando(), 0xFF);
}
