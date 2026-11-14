/**
 * @file test_k6311_esc.cpp
 * @brief K6311 (WP8h-a, Merkposten drucker.md Nr. 37): Schriftarten Breit (`ESC [ 1 m`) und Unterstreichen (`ESC [ 4 m`) am ROM 12128062 gegen das
 *        Orakel `tests/support/k6311_orakel_esc.h`.  Hardware nur aus dem ROM erschlossen [?], siehe Merkposten Nr. 34/35.
 */
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "tests/support/k6313_lauf.h"
#include "tests/support/k6311_orakel_esc.h"

using namespace k6311test;
using Typ = PrinterMachine::Config::Typ;

#ifndef K6311_ABZUG_DIR
#error "K6311_ABZUG_DIR fehlt (tests/integration/CMakeLists.txt)"
#endif

namespace {
/// Alle 94 Zeichen in vier Zeilen zu je 24 (3 Spalten Breit: 20 je Zeichen = 480 Spalten = 1440 Pixel, passt auf das Blatt), jede mit @p vorsatz.
void pruefe(const std::string& abzug, const std::string& vorsatz, Schrift s)
{
    const auto r = rom(K6311_ABZUG_DIR, abzug);
    PrinterMachine::Config c;
    c.k6313.rom_extern = r;
    k6313test::Lauf l(c, Typ::K6311_V24);
    l.bereit();
    std::string alle;
    for (int ch = 0x21; ch <= 0x7E; ++ch) alle += static_cast<char>(ch);
    std::vector<std::string> zeilen;
    for (size_t k = 0; k < alle.size(); k += 24) zeilen.push_back(alle.substr(k, 24));
    std::string t;
    for (auto& z : zeilen) t += vorsatz + z + "\r\n";
    l.sende(1, t);
    l.fahre(150'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_EQ(b.size(), zeilen.size()) << vorsatz;
    for (size_t k = 0; k < zeilen.size(); ++k)
        EXPECT_EQ(vergleichePunkte(bl, erwartet(r, zeilen[k], s), b[k].first, b[k].second).fehler, "") << vorsatz << " Zeile " << k;
}
}  // namespace

TEST(K6311Esc, PicaAlle94ZeichenInVierZeilen) { pruefe("12128062", "", Schrift::Normal); }
TEST(K6311Esc, BreitEsc1mGiltBisZumZeilenvorschub) { pruefe("12128062", "\x1b[1m", Schrift::Breit); }
TEST(K6311Esc, UnterstreichenEsc4mErsetztDieEigeneNadel9) { pruefe("12128062", "\x1b[4m", Schrift::Unterstrich); }

TEST(K6311Esc, GegenprobeDasOrakelUnterscheidetDieSchriftarten)
{
    const auto r = rom(K6311_ABZUG_DIR, "12128062");
    struct M { const char* vorsatz; Schrift ist; };
    const M faelle[] = {{"", Schrift::Normal}, {"\x1b[1m", Schrift::Breit}, {"\x1b[4m", Schrift::Unterstrich}};
    for (const M& m : faelle) {
        PrinterMachine::Config c;
        c.k6313.rom_extern = r;
        k6313test::Lauf l(c, Typ::K6311_V24);
        l.bereit();
        l.sende(1, std::string(m.vorsatz) + "HAGEDORN\r\n");
        l.fahre(60'000'000);
        const Blattmodell& bl = l.m->druck().blatt(0);
        const auto b = baender(bl);
        ASSERT_EQ(b.size(), 1u);
        EXPECT_EQ(vergleichePunkte(bl, erwartet(r, "HAGEDORN", m.ist), b[0].first, b[0].second).fehler, "") << m.vorsatz;
        for (const M& n : faelle)
            if (n.vorsatz != m.vorsatz)
                EXPECT_NE(vergleichePunkte(bl, erwartet(r, "HAGEDORN", n.ist), b[0].first, b[0].second).fehler, "") << m.vorsatz << " gegen " << n.vorsatz;
    }
}

TEST(K6311Esc, ZeichenbreitenfolgenDesIsoSatzesKenntDiesesRomNicht)
{
    // `ESC [ 2 K` ohne Leerzeichen ist am ROM 12128062 keine bekannte Folge: das K wird als Zeichen gedruckt; `ESC [ 1 SP K` ist bekannt, aendert aber den
    // Zeichenabstand nicht [?] (die Schriftartwahl haengt an einer weiteren Bedingung (IX+78).4, Merkposten Nr. 35)
    const auto r = rom(K6311_ABZUG_DIR, "12128062");
    for (const char* vorsatz : {"\x1b[1 K", "\x1b[4 K"}) {
        PrinterMachine::Config c;
        c.k6313.rom_extern = r;
        k6313test::Lauf l(c, Typ::K6311_V24);
        l.bereit();
        l.sende(1, std::string(vorsatz) + "HAGEDORN\r\n");
        l.fahre(60'000'000);
        const Blattmodell& bl = l.m->druck().blatt(0);
        const auto b = baender(bl);
        ASSERT_EQ(b.size(), 1u);
        EXPECT_EQ(vergleichePunkte(bl, erwartet(r, "HAGEDORN", Schrift::Normal), b[0].first, b[0].second).fehler, "") << vorsatz;
    }
    k6313test::Lauf l({}, Typ::K6311_V24);
    l.bereit();
    l.sende(1, "\x1b[2KHAGEDORN\r\n");
    l.fahre(60'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(vergleichePunkte(bl, erwartet(r, "KHAGEDORN", Schrift::Normal), b[0].first, b[0].second).fehler, "") << "das K erscheint als Zeichen";
}
