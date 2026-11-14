/**
 * @file test_k6313.cpp
 * @brief K6313 (Entwurf 49 WP8c/WP8d): Original-ROM 33538013 (Fassung kurz, V.24-Kassette) bis in die Grundstellung, Selbsttest
 *        (LF vor / LF zurueck beim Netz-Ein) und Text ueber V.24 gegen das Orakel `tests/support/k6313_orakel.h` (Zeichengenerator im
 *        ROM).  Merkposten drucker.md Nr. 32 und 33.
 */
#include <gtest/gtest.h>
#include <string>

#include <algorithm>
#include "core/machines/printer/printer_machine.h"
#include "tests/support/k6313_orakel.h"

using namespace k6313test;
using Typ = PrinterMachine::Config::Typ;

#ifndef K6313_ABZUG_DIR
#error "K6313_ABZUG_DIR fehlt (tests/integration/CMakeLists.txt)"
#endif

namespace {
struct Lauf {
    std::unique_ptr<PrinterMachine> m;
    explicit Lauf(PrinterMachine::Config c = {})
    {
        c.typ = Typ::K6313_V24;
        m = std::make_unique<PrinterMachine>(c);
    }
    K6313Ccb& st() { return m->k6313(); }
    void fahre(uint64_t takte)
    {
        const uint64_t z = m->totalCycles() + takte;
        while (m->totalCycles() < z) m->run(2000);
    }
};
}  // namespace

TEST(K6313Modul, ErreichtDieGrundstellung)
{
    Lauf l;
    l.fahre(30'000'000);
    EXPECT_TRUE(l.st().lampe(1)) << "ON LINE";
    EXPECT_TRUE(l.st().lampe(2)) << "POW";
    EXPECT_FALSE(l.st().lampe(3)) << "ERR";
    EXPECT_FALSE(l.st().lampe(4)) << "PE";
    // Wagen am linken Anschlag synchronisiert (Phasenrichtung [?]: am Lauf erreicht die Firmware eine Lage nahe 0), steht, nicht blockiert
    const int pos = l.m->druck().wagen().position();
    EXPECT_GE(pos, 0);
    EXPECT_LE(pos, 20);
    EXPECT_FALSE(static_cast<SchrittWagen&>(l.m->druck().wagen()).blockiert());
    EXPECT_EQ(l.m->druck().papier(0).schritte(), 1u) << "Anfangsvorschub um einen Schritt (Richtung der Formularphasen [?])";
    EXPECT_EQ(l.st().nadeln(), 0);
    EXPECT_TRUE(l.st().sioAusgabe(0).empty()) << "ohne XON/XOFF (17-1 OFF) laeuft das DTR-Protokoll, es wird nichts gesendet";
}

TEST(K6313Modul, GrundstellungAusMehrerenStartlagenDesWagens)
{
    for (int start : {0, 5, 400, 1000, 1700}) {
        PrinterMachine::Config c;
        c.k6313.wagen_start = start;
        Lauf l(c);
        l.fahre(30'000'000);
        EXPECT_TRUE(l.st().lampe(1)) << start;
        EXPECT_LE(l.m->druck().wagen().position(), 20) << start;
    }
}

TEST(K6313Modul, DtrFolgtDemPapierendeUeberKanalB)
{
    // Die Firmware schreibt DTR/RTS an SIO-Kanal B (WR5 = 8AH bei Papier, 0AH bei Papierende, ROM 1EDDH-1EFDH [F])
    Lauf l;
    l.fahre(30'000'000);
    EXPECT_TRUE(l.m->serielleAnschluesse()[1]->dtr());
    l.st().setPapierDa(false);
    l.fahre(3'000'000);
    EXPECT_TRUE(l.st().lampe(4));
    EXPECT_FALSE(l.m->serielleAnschluesse()[1]->dtr()) << "Papierende nimmt DTR zurueck";
}

TEST(K6313Modul, OhneDil52VerlaesstDieFirmwareDenNormalpfad)
{
    // Spalte 3, Zeile 2/3 (5-2 ON, 5-1 OFF) liest die Firmware beim Einschalten (ROM 018CH) [F]
    PrinterMachine::Config c;
    c.k6313.dil = 0;
    Lauf l(c);
    l.fahre(30'000'000);
    Lauf n;
    n.fahre(30'000'000);
    EXPECT_NE(l.m->cpuPC(), n.m->cpuPC()) << "die Leerlaufschleife (Normalpfad) wird nicht erreicht";
}

// ─── Druckbild gegen den Zeichengenerator im ROM (WP8d, Orakel tests/support/k6313_orakel.h) ───────────────────────────────

namespace {
/// Selbsttest: Zeilen zu je 80 Zeichen aus dem Umlauf 21H...7EH, abwechselnd 80 Zeichen vorwaerts (21H...70H) und der Rest (71H...7EH)
/// rueckwaerts (Merkposten Nr. 33).
std::string zeileVor()
{
    std::string t;
    for (int c = 0x21; c <= 0x70; ++c) t += static_cast<char>(c);
    return t;
}
std::string zeileRueck()
{
    std::string t;
    for (int c = 0x71; c <= 0x7E; ++c) t += static_cast<char>(c);
    return t;
}
}  // namespace

TEST(K6313Modul, SelbsttestMitLfVorDruckStimmtMitDemZeichengeneratorImRomUeberein)
{
    const auto r = rom(K6313_ABZUG_DIR);
    ASSERT_EQ(r.size(), 10240u);
    Lauf l;
    l.st().setTaste(2, true);   // LINE FEED vor beim Netz-Ein gehalten: Selbsttest (Matrixzelle Spalte 1, Zeile 1 [?], hardware.md §5)
    const uint64_t z = l.m->totalCycles() + 80'000'000;
    while (l.m->druck().kopf().anschlaege() == 0 && l.m->totalCycles() < z) l.m->run(2000);
    ASSERT_GT(l.m->druck().kopf().anschlaege(), 0u) << "der Selbsttest druckt nichts";
    l.fahre(30'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_GE(b.size(), 5u);
    // Kennungszeile: die Firmware druckt die ROM-Kennung (27F8H "38033513", Nibblefolge wie auf dem Etikett 3.35-3-80/13 -> 33538013)
    ASSERT_EQ(r[0x27F8], '3');
    std::string rom_id(r.begin() + 0x27F8, r.begin() + 0x2800);
    EXPECT_TRUE(std::is_permutation(rom_id.begin(), rom_id.end(), std::string("33538013").begin())) << rom_id;
    int x0 = 0, x1 = 0, x2 = 0, x3 = 0;
    EXPECT_EQ(vergleiche(bl, r, "33538013", b[0].first, b[0].second, &x0), "") << "Kennungszeile";
    EXPECT_EQ(vergleiche(bl, r, zeileVor(), b[1].first, b[1].second, &x1), "") << "Zeile 1 (vorwaerts): "
        << lesen(bl, r, b[1].first, b[1].second, x1);
    EXPECT_EQ(vergleiche(bl, r, zeileRueck(), b[2].first, b[2].second, &x2), "") << "Zeile 2 (rueckwaerts): "
        << lesen(bl, r, b[2].first, b[2].second, x2);
    EXPECT_EQ(vergleiche(bl, r, zeileVor(), b[3].first, b[3].second, &x3), "") << "Zeile 3 (vorwaerts)";
    // Vorwaerts- und Rueckwaertszeilen haben denselben Ursprung (Firmware: Nadelspalten, Kopfmodell: Flugzeit abgeglichen [?]; +-2 Pixel Rundung)
    EXPECT_NEAR(x0, x1, 2);
    EXPECT_NEAR(x1, x2, 2);
    EXPECT_NEAR(x2, x3, 2);
    // Zeilenabstand 1/6 Zoll = 36 Einheiten (216 je Zoll)
    EXPECT_EQ(b[2].first - b[1].first, 36);
    EXPECT_EQ(b[3].first - b[2].first, 36);
    // Zeilenlaenge: 80 Zeichen Pica = 8 Zoll = 960 Spalten
    int rechts = 0;
    for (auto& q : gedruckt(bl, b[1].first, b[1].second)) rechts = std::max(rechts, q.first);
    EXPECT_NEAR(rechts - x1, 80 * ZEICHEN, ZEICHEN);
}

TEST(K6313Modul, SelbsttestMitLfZurueckDruckStimmtMitDemZeichengeneratorImRomUeberein)
{
    const auto r = rom(K6313_ABZUG_DIR);
    Lauf l;
    l.st().setTaste(3, true);   // LINE FEED zurueck: gleicher Zeichenumlauf, ohne Kennungszeile
    const uint64_t z = l.m->totalCycles() + 80'000'000;
    while (l.m->druck().kopf().anschlaege() == 0 && l.m->totalCycles() < z) l.m->run(2000);
    ASSERT_GT(l.m->druck().kopf().anschlaege(), 0u);
    l.fahre(30'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_GE(b.size(), 4u);
    int x0 = 0, x1 = 0, x2 = 0;
    EXPECT_EQ(vergleiche(bl, r, zeileVor(), b[0].first, b[0].second, &x0), "") << "Zeile 1 (vorwaerts)";
    EXPECT_EQ(vergleiche(bl, r, zeileRueck(), b[1].first, b[1].second, &x1), "") << "Zeile 2 (rueckwaerts)";
    EXPECT_EQ(vergleiche(bl, r, zeileVor(), b[2].first, b[2].second, &x2), "") << "Zeile 3 (vorwaerts)";
    EXPECT_NEAR(x0, x1, 2);
    EXPECT_NEAR(x1, x2, 2);
}

TEST(K6313Modul, TextUeberV24WirdGedrucktUndBeideRichtungenDecken)
{
    // Wie K6316 (Merkposten Nr. 23): CTS/DCD kommen vom Host; der Text geht ueber den Wandler (fernGib) in die SIO an Kanal A
    const auto r = rom(K6313_ABZUG_DIR);
    Lauf l;
    l.fahre(30'000'000);
    ASSERT_TRUE(l.st().lampe(1)) << "ON LINE";
    l.m->serialHub()->wandler(1).anbinden();
    l.fahre(1'000'000);
    const std::string t1 = "HELLO 123 ROBOTRON K6313", t2 = "ZEILE ZWEI 0987654321";
    for (char ch : t1 + "\r\n" + t2 + "\r\n") {
        const uint8_t b = static_cast<uint8_t>(ch);
        int g = 0;
        while (l.m->serialHub()->wandler(1).fernFrei() == 0 && g++ < 1000) l.fahre(3000);
        l.m->serialHub()->wandler(1).fernGib(&b, 1);
        l.fahre(3000);
        uint8_t rest[64];
        while (l.m->serialHub()->wandler(1).fernNimm(rest, sizeof rest) > 0) {}
    }
    l.fahre(80'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_GE(b.size(), 2u);
    int x1 = 0, x2 = 0;
    EXPECT_EQ(vergleiche(bl, r, t1, b[0].first, b[0].second, &x1), "") << "Zeile 1";
    EXPECT_EQ(vergleiche(bl, r, t2, b[1].first, b[1].second, &x2), "") << "Zeile 2";
    EXPECT_NEAR(x1, x2, 2) << "beide Zeilen beginnen am linken Rand";
    EXPECT_EQ(b[1].first - b[0].first, 36) << "Zeilenabstand 1/6 Zoll (DIL 11-2 OFF)";
}
