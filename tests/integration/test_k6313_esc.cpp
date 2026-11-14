/**
 * @file test_k6313_esc.cpp
 * @brief K6313 (Entwurf 49 WP8h-a, Merkposten drucker.md Nr. 37): Schriftarten und Steuerfolgen gegen das Orakel `tests/support/k6313_orakel_esc.h`
 *        (Zeichengenerator im ROM + Datenweg der Firmware) und gegen die Massangaben des Handbuchs (Zeilenabstand, Formularlaenge).
 *        EPSON-Befehlssatz am 33538013 (Fassung kurz), ISO-Befehlssatz am 30835007 (Fassung lang).
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

#include "tests/support/k6313_lauf.h"
#include "tests/support/k6313_orakel.h"
#include "tests/support/k6313_orakel_esc.h"

using namespace k6313test;
using Typ = PrinterMachine::Config::Typ;

#ifndef K6313_ABZUG_DIR
#error "K6313_ABZUG_DIR fehlt (tests/integration/CMakeLists.txt)"
#endif

namespace {
std::vector<std::string> alleZeichen()
{
    std::string alle;
    for (int c = 0x21; c <= 0x7E; ++c) alle += static_cast<char>(c);
    std::vector<std::string> z;
    for (size_t k = 0; k < alle.size(); k += 32) z.push_back(alle.substr(k, 32));
    return z;
}

PrinterMachine::Config konfig(const std::string& name)
{
    PrinterMachine::Config c;
    c.k6313.rom_extern = rom(K6313_ABZUG_DIR, name);
    return c;
}

/// Alle 94 Zeichen in drei Zeilen (vorwaerts, rueckwaerts, vorwaerts), jede Zeile beginnt mit @p vorsatz; Punkt fuer Punkt gegen das Orakel.
/// Rueckgabe: Ursprung in x der drei Zeilen.
std::array<int, 3> pruefeSchrift(const std::string& abzug, const std::string& vorsatz, const Schrift& s)
{
    const auto r = rom(K6313_ABZUG_DIR, abzug);
    Lauf l(konfig(abzug));
    l.bereit();
    const auto zeilen = alleZeichen();
    std::string t;
    for (auto& z : zeilen) t += vorsatz + z + "\r\n";
    l.sende(1, t);
    l.fahre(100'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    std::array<int, 3> ox{};
    EXPECT_EQ(b.size(), 3u) << abzug << " " << vorsatz;
    for (size_t k = 0; k < zeilen.size() && k < b.size(); ++k)
        EXPECT_EQ(vergleicheHuelle(bl, erwartet3(r, zeilen[k], s), b[k].first, b[k].second, &ox[k]), "") << abzug << " Zeile " << k;
    return ox;
}
}  // namespace

// ─── EPSON-Befehlssatz (33538013, DIL 13-2 ON) ───────────────────────────────

TEST(K6313Esc, PicaAlle94ZeichenInDreiZeilen)
{
    const auto ox = pruefeSchrift("33538013", "", Schrift{});
    EXPECT_NEAR(ox[0], ox[2], 1) << "Vorwaertszeilen haben denselben Ursprung";
    EXPECT_NEAR(ox[0], ox[1], 3) << "Rueckwaertszeile: Abgleich des Kopfmodells [?]";
}

TEST(K6313Esc, FettEscE)
{
    // ESC E: Spalte i := g[i] AND g[i-1] (sub_1A52) - jeder Punkt wird um eine Spalte nach rechts verbreitert, auch Nadel 9
    const auto ox = pruefeSchrift("33538013", "\x1b" "E", Schrift{Modus::Fett, false});
    EXPECT_NEAR(ox[0], ox[2], 1);
    EXPECT_NEAR(ox[0], ox[1], 6) << "Rueckwaertszeile: Versatz gegen die Vorwaertszeile ~ +5 Pixel [?]";
}

TEST(K6313Esc, BreitEscW1)
{
    // ESC W 1: verbreiterte Spalten im Abstand von zwei Spalten, Zeichenabstand 24 Spalten
    const auto ox = pruefeSchrift("33538013", "\x1b" "W1", Schrift{Modus::Breit, false});
    EXPECT_NEAR(ox[0], ox[2], 1);
    EXPECT_NEAR(ox[0], ox[1], 3);
}

TEST(K6313Esc, BreitEscSoGiltNurFuerEineZeile)
{
    // SO (0EH) schaltet Breit ein, LF hebt es auf [Handbuch]: die Zeile mit SO ist breit, die naechste Pica
    const auto r = rom(K6313_ABZUG_DIR, "33538013");
    Lauf l(konfig("33538013"));
    l.bereit();
    l.sende(1, std::string("\x0e") + "BREIT\r\nNORMAL\r\n");
    l.fahre(80'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(vergleicheHuelle(bl, erwartet3(r, "BREIT", Schrift{Modus::Breit, false}), b[0].first, b[0].second), "");
    EXPECT_EQ(vergleicheHuelle(bl, erwartet3(r, "NORMAL", Schrift{}), b[1].first, b[1].second), "");
}

TEST(K6313Esc, UnterstreichenEscMinus1ErsetztDieEigeneNadel9)
{
    // ESC - 1: Nadel 9 auf den geraden Spalten der Zelle; die eigene Nadel 9 des Zeichens (g j p q y , ; _) entfaellt: _ (nur ungerade Spalten) und q (Spalte 7) verlieren ihre Punkte
    const auto ox = pruefeSchrift("33538013", "\x1b-1", Schrift{Modus::Normal, true});
    EXPECT_NEAR(ox[0], ox[2], 1);
    EXPECT_NEAR(ox[0], ox[1], 3);
}

TEST(K6313Esc, EliteEscMUndPicaEscP)
{
    // ESC M: 1/12 Zoll = 40 Pixel je Zeichen, die 9 Spalten im Abstand 8/3 Pixel (Punkte ueberlappen)
    const auto ox = pruefeSchrift("33538013", "\x1b" "M", Schrift{Modus::Elite, false});
    EXPECT_NEAR(ox[0], ox[2], 1);
    EXPECT_NEAR(ox[0], ox[1], 4) << "Rueckwaertszeile ~ +3 Pixel [?]";
    pruefeSchrift("33538013", "\x1b" "M\x1b" "P", Schrift{});   // ESC P hebt ESC M auf
}

TEST(K6313Esc, KomprimiertEscSiUndDc2)
{
    // ESC SI: 1/17 Zoll = 28 Pixel je Zeichen, Spaltenabstand 2 Pixel
    const auto ox = pruefeSchrift("33538013", "\x0f", Schrift{Modus::Komprimiert, false});
    EXPECT_NEAR(ox[0], ox[2], 1);
    EXPECT_NEAR(ox[0], ox[1], 6) << "Rueckwaertszeile ~ +5 Pixel [?]";
    pruefeSchrift("33538013", "\x0f\x12", Schrift{});   // DC2 hebt SI auf
}

// ─── ISO-Befehlssatz (30835007, ISO 6429-artig; Handbuch "ISO-Befehlssatz") ──

TEST(K6313Esc, IsoBreitBdeEsc1mGiltFuerEineZeile)
{
    const auto ox = pruefeSchrift("30835007", "\x1b[1m", Schrift{Modus::Breit, false});
    EXPECT_NEAR(ox[0], ox[2], 1);
    // LF hebt BDE auf: die zweite Zeile ohne Vorsatz ist Pica
    const auto r = rom(K6313_ABZUG_DIR, "30835007");
    Lauf l(konfig("30835007"));
    l.bereit();
    l.sende(1, std::string("\x1b[1m") + "BREIT\r\nNORMAL\r\n");
    l.fahre(80'000'000);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(vergleicheHuelle(bl, erwartet3(r, "NORMAL", Schrift{}), b[1].first, b[1].second), "");
}

TEST(K6313Esc, IsoUnterstreichenUdlEsc4m)
{
    pruefeSchrift("30835007", "\x1b[4m", Schrift{Modus::Normal, true});
}

TEST(K6313Esc, IsoZeichenbreite1_12Und1_17Zoll)
{
    pruefeSchrift("30835007", "\x1b[1 K", Schrift{Modus::Elite, false});         // CPI96
    pruefeSchrift("30835007", "\x1b[2 K", Schrift{Modus::Elite, false});         // zweite Schreibweise
    pruefeSchrift("30835007", "\x1b[4 K", Schrift{Modus::Komprimiert, false});   // CPI137
    pruefeSchrift("30835007", "\x1b[3 K", Schrift{Modus::Komprimiert, false});
    pruefeSchrift("30835007", "\x1b[1 K\x1b[0 K", Schrift{});                    // CPI80 hebt auf
}

// ─── Zeilenabstand, Vorschub, Formularlaenge ─────────────────────────────────

namespace {
/// Lage der Punktzeilen '.' (eine Nadelzeile hoch, die Zeilen verschmelzen nicht): Abstaende zwischen den Baendern.
std::vector<int> abstaende(const std::string& abzug, const std::string& text, PrinterMachine::Config c = {}, uint64_t takte = 150'000'000)
{
    c.k6313.rom_extern = rom(K6313_ABZUG_DIR, abzug);
    Lauf l(c);
    l.bereit();
    l.sende(1, text);
    l.fahre(takte);
    std::vector<int> d;
    const auto b = baender(l.m->druck().blatt(0));
    for (size_t k = 1; k < b.size(); ++k) d.push_back(b[k].first - b[k - 1].first);
    return d;
}
}  // namespace

TEST(K6313Esc, ZeilenabstandEpsonEsc0Esc1Esc2Esc3UndEscJ)
{
    // Vorgabe 1/6 Zoll = 36 Schritte (216 je Zoll); der erste Abstand gehoert noch dem Vorgabewert (LF nach der ersten Zeile)
    EXPECT_EQ(abstaende("33538013", ".\r\n.\r\n"), (std::vector<int>{36}));
    EXPECT_EQ(abstaende("33538013", "\x1b" "0.\r\n.\r\n.\r\n"), (std::vector<int>{27, 27})) << "ESC 0: 1/8 Zoll";
    EXPECT_EQ(abstaende("33538013", "\x1b" "1.\r\n.\r\n.\r\n"), (std::vector<int>{21, 21})) << "ESC 1: 7/72 Zoll";
    EXPECT_EQ(abstaende("33538013", "\x1b" "0\x1b" "2.\r\n.\r\n.\r\n"), (std::vector<int>{36, 36})) << "ESC 2: 1/6 Zoll";
    EXPECT_EQ(abstaende("33538013", std::string("\x1b" "3\x28.\r\n.\r\n.\r\n")), (std::vector<int>{40, 40})) << "ESC 3 n: n/216 Zoll (n = 40)";
    EXPECT_EQ(abstaende("33538013", std::string("\x1b" "A\x06\x1b" "2.\r\n.\r\n.\r\n")), (std::vector<int>{18, 18})) << "ESC A n, ESC 2: n/72 Zoll (n = 6)";
    EXPECT_EQ(abstaende("33538013", std::string(".\r\n\x1b" "J\x14.\r\n.\r\n")), (std::vector<int>{56, 36})) << "ESC J n: einmalig n/216 Zoll mehr (n = 20)";
}

TEST(K6313Esc, ZeilenabstandUeberDil11_2EinAchtelZoll)
{
    PrinterMachine::Config c;
    c.k6313.dil |= 1u << K6313Ccb::dilIndex(11, 2);   // 11-2 ON: 1/8 Zoll (hardware.md §5.1; Auswertung 0459H)
    EXPECT_EQ(abstaende("33538013", ".\r\n.\r\n.\r\n", c), (std::vector<int>{27, 27}));
}

TEST(K6313Esc, FormularvorschubFfBisZumNaechstenFormularanfang)
{
    // FF: Papiervorschub zum Anfang des naechsten Formulars; Formatlaenge durch DIL 12-1/12-2: 12 Zoll (OFF OFF), 11 (ON OFF), 6 (OFF ON), 5,5 (ON ON)
    struct F { bool a, b; int zoll216; };
    for (const F& f : {F{false, false, 12 * 216}, F{true, false, 11 * 216}, F{false, true, 6 * 216}, F{true, true, 1188}}) {
        PrinterMachine::Config c;
        c.k6313.dil &= ~((1u << K6313Ccb::dilIndex(12, 1)) | (1u << K6313Ccb::dilIndex(12, 2)));
        if (f.a) c.k6313.dil |= 1u << K6313Ccb::dilIndex(12, 1);
        if (f.b) c.k6313.dil |= 1u << K6313Ccb::dilIndex(12, 2);
        const auto d = abstaende("33538013", ".\r\n\x0c.\r\n.\r\n", c, 300'000'000);
        ASSERT_EQ(d.size(), 2u) << f.zoll216;
        EXPECT_EQ(d[0], f.zoll216) << "Formatlaenge 12-1=" << f.a << " 12-2=" << f.b;
        EXPECT_EQ(d[1], 36);
    }
}

TEST(K6313Esc, IsoZeilenabstandUndVertikalpositionierung)
{
    EXPECT_EQ(abstaende("30835007", ".\r\n.\r\n.\r\n"), (std::vector<int>{36, 36})) << "LF: 1/6 Zoll";
    // VPRV ESC [ n e: Papiervorschub um n/12 Zoll (18 Schritte je 1/12 Zoll), zusaetzlich zum LF der Zeile davor
    EXPECT_EQ(abstaende("30835007", ".\r\n\x1b[12e.\r\n.\r\n"), (std::vector<int>{36 + 216, 36}));
    EXPECT_EQ(abstaende("30835007", ".\r\n\x1b[3e.\r\n.\r\n"), (std::vector<int>{36 + 54, 36}));
    // VPA ESC [ n d: absolut, n/12 Zoll vom Formularanfang; der Anfang liegt eine Zeile (36 Schritte) vor der ersten Druckzeile
    EXPECT_EQ(abstaende("30835007", ".\r\n\x1b[30d.\r\n.\r\n"), (std::vector<int>{30 * 18 - 36, 36}));
    EXPECT_EQ(abstaende("30835007", ".\r\n\x0c.\r\n.\r\n", {}, 300'000'000), (std::vector<int>{12 * 216, 36})) << "FF: 12 Zoll";
}

TEST(K6313Esc, GegenprobeDasOrakelUnterscheidetDieSchriftarten)
{
    // Das Huellenorakel (+-1 Pixel) darf eine falsche Schriftart nicht durchlassen
    const auto r = rom(K6313_ABZUG_DIR, "33538013");
    const std::string text = "HAGEDORN";
    struct M { const char* vorsatz; Schrift ist; };
    const M faelle[] = {{"", Schrift{}}, {"\x1b" "E", Schrift{Modus::Fett, false}}, {"\x1b" "W1", Schrift{Modus::Breit, false}}, {"\x1b" "M", Schrift{Modus::Elite, false}},
                        {"\x0f", Schrift{Modus::Komprimiert, false}}, {"\x1b-1", Schrift{Modus::Normal, true}}};
    for (const M& m : faelle) {
        Lauf l(konfig("33538013"));
        l.bereit();
        l.sende(1, std::string(m.vorsatz) + text + "\r\n");
        l.fahre(60'000'000);
        const Blattmodell& bl = l.m->druck().blatt(0);
        const auto b = baender(bl);
        ASSERT_EQ(b.size(), 1u);
        EXPECT_EQ(vergleicheHuelle(bl, erwartet3(r, text, m.ist), b[0].first, b[0].second), "") << m.vorsatz;
        for (const M& n : faelle) {
            if (n.vorsatz == m.vorsatz) continue;
            EXPECT_NE(vergleicheHuelle(bl, erwartet3(r, text, n.ist), b[0].first, b[0].second), "") << "Gegenprobe: " << m.vorsatz << " gegen " << n.vorsatz;
        }
    }
}

// ─── Hexdump (FORM FEED + LINE FEED vor beim Netz-Ein, Handbuch 1.4.5) ───────

namespace {
std::string hexText(const std::string& daten)
{
    std::string h;
    for (char c : daten) { char b[8]; std::snprintf(b, sizeof b, "%02X ", static_cast<uint8_t>(c)); h += b; }
    return h;
}

/// Hexdump-Lauf: Tasten LF vor (2) + FF (4) beim Netz-Ein, @p daten senden, dann ON/OFF LINE (der Rest des Druckpuffers wird gedruckt).
std::unique_ptr<Lauf> hexdumpLauf(const std::string& abzug, const std::string& daten)
{
    auto l = std::make_unique<Lauf>(konfig(abzug));
    l->st().setTaste(2, true);
    l->st().setTaste(4, true);
    l->fahre(30'000'000);
    l->st().setTaste(2, false);
    l->st().setTaste(4, false);
    l->m->serialHub()->wandler(1).anbinden();
    l->fahre(1'000'000);
    l->sende(1, daten);
    l->fahre(20'000'000);
    l->st().setTaste(1, true);
    l->fahre(3'000'000);
    l->st().setTaste(1, false);
    l->fahre(60'000'000);
    return l;
}
}  // namespace

TEST(K6313Esc, IsoHexdumpBeispielDesHandbuchsESC4mHexDump)
{
    // ISO-Ergaenzung S. 1: "ESC [ 4 m HEX - DUMP CR LF" ergibt im Hexdump "1B 5B 34 6D 48 45 58 20 2D 20 44 55 4D 50 0D 0A"; gedruckt wird in 12 Zeichen je Zoll (Elite-Teilung)
    const auto r = rom(K6313_ABZUG_DIR, "30835007");
    const std::string daten = "\x1b[4mHEX - DUMP\r\n";
    auto l = hexdumpLauf("30835007", daten);
    const Blattmodell& bl = l->m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(hexText(daten), "1B 5B 34 6D 48 45 58 20 2D 20 44 55 4D 50 0D 0A ");
    EXPECT_EQ(vergleicheHuelle(bl, erwartet3(r, hexText(daten), Schrift{Modus::Elite, false}), b[0].first, b[0].second), "");
    EXPECT_NE(vergleicheHuelle(bl, erwartet3(r, hexText(daten), Schrift{}), b[0].first, b[0].second), "") << "Gegenprobe: nicht Pica";
}

TEST(K6313Esc, EpsonHexdumpDrucktZweiunddreissigByteJeZeile)
{
    // Handbuch: "Der Druck erfolgt nur, wenn der Druckpuffer voll ist" - 128 Byte = vier Zeilen zu 32 Byte (Elite-Teilung)
    const auto r = rom(K6313_ABZUG_DIR, "33538013");
    std::string daten;
    for (int i = 0; i < 47; ++i) daten += i % 10 == 0 ? "HEX-DUMP " : "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    daten += "\r\n";
    auto l = hexdumpLauf("33538013", daten);
    const Blattmodell& bl = l->m->druck().blatt(0);
    const auto b = baender(bl);
    ASSERT_GE(b.size(), 4u);
    for (size_t k = 0; k < 4; ++k)
        EXPECT_EQ(vergleicheHuelle(bl, erwartet3(r, hexText(daten.substr(32 * k, 32)), Schrift{Modus::Elite, false}), b[k].first, b[k].second), "") << "Zeile " << k;
}
