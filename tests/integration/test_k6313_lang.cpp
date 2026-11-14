/**
 * @file test_k6313_lang.cpp
 * @brief K6313 Fassung "lang" (WP8h): 16-KiB-Abzug 30835007 (ISO, V.24/IFSS), Speicherplan ROM 0000H-3FFFH, RAM F000H-F7FFH.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <iterator>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

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
    explicit Lauf(PrinterMachine::Config c = {}, Typ t = Typ::K6313_V24)
    {
        c.typ = t;
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


namespace {
void senden(Lauf& l, int idx, const std::string& t)
{
    auto& w = l.m->serialHub()->wandler(idx);
    for (char ch : t) {
        const uint8_t b = static_cast<uint8_t>(ch);
        int g = 0;
        while (w.fernFrei() == 0 && g++ < 1000) l.fahre(3000);
        w.fernGib(&b, 1);
        l.fahre(3000);
        uint8_t rest[64];
        while (w.fernNimm(rest, sizeof rest) > 0) {}
    }
}

}  // namespace

namespace {
const std::vector<uint8_t>& abzug(const char* name = "30835007")
{
    static std::map<std::string, std::vector<uint8_t>> m;
    auto it = m.find(name);
    if (it == m.end()) it = m.emplace(name, rom(K6313_ABZUG_DIR, name)).first;
    return it->second;
}

PrinterMachine::Config konfig(const char* name = "30835007")
{
    PrinterMachine::Config c;
    c.k6313.rom_extern = abzug(name);
    return c;
}

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

TEST(K6313Lang, SpeicherplanLangWirdAnDerLaengeErkanntUndRamLiegtBeiF000)
{
    ASSERT_EQ(abzug().size(), 16384u);
    Lauf l(konfig());
    EXPECT_TRUE(l.st().config().lang);
    // ROM 0000H-3FFFH, RAM F000H-F7FFH (2 KiB); dazwischen und darueber leerer Bus
    for (uint16_t a : {uint16_t(0x0000), uint16_t(0x27F8), uint16_t(0x3FF8), uint16_t(0x3FFF)})
        EXPECT_EQ(l.st().memRead(a), abzug()[a]) << std::hex << a;
    EXPECT_EQ(l.st().memRead(0x4000), 0xFF);
    EXPECT_EQ(l.st().memRead(0x3800), 0xFF) << "der kurze Plan (RAM bei 3800H) gilt hier nicht";
    EXPECT_EQ(l.st().memRead(0xF800), 0xFF);
    l.st().memWrite(0xF000, 0x5A);
    l.st().memWrite(0xF7FF, 0xA5);
    l.st().memWrite(0x3800, 0x11);
    l.st().memWrite(0x0000, 0x22);
    EXPECT_EQ(l.st().memRead(0xF000), 0x5A);
    EXPECT_EQ(l.st().memRead(0xF7FF), 0xA5);
    EXPECT_EQ(l.st().memRead(0x3800), 0xFF);
    EXPECT_EQ(l.st().memRead(0x0000), abzug()[0]) << "ROM ist nicht beschreibbar";
}

TEST(K6313Lang, ErreichtDieGrundstellungMitStapelUndVektortabelleBeiF000)
{
    Lauf l(konfig());
    l.fahre(30'000'000);
    EXPECT_TRUE(l.st().lampe(1)) << "ON LINE";
    EXPECT_FALSE(l.st().lampe(3)) << "ERR";
    EXPECT_LE(l.m->druck().wagen().position(), 20);
    EXPECT_FALSE(static_cast<SchrittWagen&>(l.m->druck().wagen()).blockiert());
    EXPECT_EQ(l.m->druck().kopf().anschlaege(), 0u);
    // I = F0H, Stapel F800H [F: 0000H-0009H]
    EXPECT_EQ(l.st().cpu().I, 0xF0);
    EXPECT_GE(l.st().cpu().SP, 0xF400);
    EXPECT_LE(l.st().cpu().SP, 0xF800);
}
namespace {
/// Selbsttest mit LINE FEED vor (Kennungszeile + Zeichenumlauf) bzw. zurueck (ohne Kennung) des Abzugs @p name gegen das Orakel.
void selbsttest(const char* name, const std::string& kennung, bool mit_kennung)
{
    const auto& r = abzug(name);
    Lauf l(konfig(name));
    l.st().setTaste(mit_kennung ? 2 : 3, true);   // LINE FEED vor / zurueck beim Netz-Ein gehalten (Matrix wie am 10-KiB-Abzug; dieselbe Firmwarefamilie)
    const uint64_t z = l.m->totalCycles() + 80'000'000;
    while (l.m->druck().kopf().anschlaege() == 0 && l.m->totalCycles() < z) l.m->run(2000);
    ASSERT_GT(l.m->druck().kopf().anschlaege(), 0u) << name << ": der Selbsttest druckt nichts";
    l.fahre(30'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    const size_t k = mit_kennung ? 1 : 0;
    ASSERT_GE(b.size(), 4u + k) << name;
    int xs[4] = {0, 0, 0, 0};
    if (mit_kennung) EXPECT_EQ(vergleiche(bl, r, kennung, b[0].first, b[0].second, &xs[3]), "") << name << " Kennungszeile";
    int x1 = 0, x2 = 0, x3 = 0;
    EXPECT_EQ(vergleiche(bl, r, zeileVor(), b[k].first, b[k].second, &x1), "") << name << " Zeile 1 (vorwaerts)";
    EXPECT_EQ(vergleiche(bl, r, zeileRueck(), b[k + 1].first, b[k + 1].second, &x2), "") << name << " Zeile 2 (rueckwaerts)";
    EXPECT_EQ(vergleiche(bl, r, zeileVor(), b[k + 2].first, b[k + 2].second, &x3), "") << name << " Zeile 3 (vorwaerts)";
    if (mit_kennung) EXPECT_NEAR(xs[3], x1, 2);
    EXPECT_NEAR(x1, x2, 2);
    EXPECT_NEAR(x2, x3, 2);
    EXPECT_EQ(b[k + 1].first - b[k].first, 36) << name;
    EXPECT_EQ(b[k + 2].first - b[k + 1].first, 36) << name;
}
}  // namespace

TEST(K6313Lang, SelbsttestMitLfVorDruckKennungUndZeichengeneratorPunktFuerPunkt)
{
    // Kennungszeile: das ROM traegt die Ziffern bei 3FF8H ("35030807"), gedruckt wird das Etikett 3.08-3-50/07 = 30835007 (Permutation wie am 10-KiB-Abzug)
    std::string rom_id(abzug().begin() + 0x3FF8, abzug().begin() + 0x4000);
    EXPECT_TRUE(std::is_permutation(rom_id.begin(), rom_id.end(), std::string("30835007").begin())) << rom_id;
    selbsttest("30835007", "30835007", true);
}

TEST(K6313Lang, SelbsttestMitLfZurueckDruckOhneKennungszeile) { selbsttest("30835007", "", false); }

TEST(K6313Lang, DerZweiteDeutscheAbzug30835207DrucktSelbsttestWieDerErste)
{
    // Liste: "nur 10k beschrieben" - Messung dagegen (Daten bis 3FFFH, Kennung 35230807 bei 3FF8H); er laeuft auf demselben Plan "lang"
    selbsttest("30835207", "30835207", true);
    selbsttest("30835207", "", false);
}

namespace {
/// Zwei Zeilen ueber den Wandler @p idx (0 IFSS, 1 V.24) und Punkt fuer Punkt gegen das Orakel.
void druckeText(Typ typ, int idx, const char* name = "30835007")
{
    Lauf l(konfig(name), typ);
    l.fahre(30'000'000);
    ASSERT_TRUE(l.st().lampe(1)) << name << " ON LINE";
    l.m->serialHub()->wandler(idx).anbinden();
    l.fahre(1'000'000);
    const std::string t1 = "HELLO 123 ROBOTRON K6313", t2 = "ZEILE ZWEI 0987654321";
    senden(l, idx, t1 + "\r\n" + t2 + "\r\n");
    l.fahre(80'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_GE(b.size(), 2u) << name;
    int x1 = 0, x2 = 0;
    EXPECT_EQ(vergleiche(bl, abzug(name), t1, b[0].first, b[0].second, &x1), "") << name << " Zeile 1";
    EXPECT_EQ(vergleiche(bl, abzug(name), t2, b[1].first, b[1].second, &x2), "") << name << " Zeile 2";
    EXPECT_NEAR(x1, x2, 2);
    EXPECT_EQ(b[1].first - b[0].first, 36) << name << " Zeilenabstand 1/6 Zoll";
}
}  // namespace

TEST(K6313Lang, TextUeberV24)  { druckeText(Typ::K6313_V24, 1); }
TEST(K6313Lang, TextUeberIfss) { druckeText(Typ::K6313_Ifss, 0); }
TEST(K6313Lang, TextMit30835207UeberV24UndIfss)
{
    druckeText(Typ::K6313_V24, 1, "30835207");
    druckeText(Typ::K6313_Ifss, 0, "30835207");
}
