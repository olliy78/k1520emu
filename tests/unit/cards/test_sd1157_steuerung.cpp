/**
 * @file test_sd1157_steuerung.cpp
 * @brief Steuerung des SD 1157 (Entwurf 49, WP1): Speicherkarte (ROM 0000H–297FH, RAM 2C00H/3800H,
 *        ZG1 3000H, ZG2 3400H liest FFH), ROM-Abbild gegen die MD5 aus doc/EPROMS/SD1157/README.md,
 *        E/A-Tore (6FH, BCH, BEH/BFH, CTC DCH, PIO F4H/F8H/ECH, SIO 7CH) mit den Ruhepegeln aus
 *        hardware.md §15, Wagen/Papier am Druckwerk (LAUF sofort, Raender), Nadelanschlag mit FGDI, Tasten/Lampen-API, Interruptkette.
 *        Quelle fuer alle Zahlen: doc/printer/sd1157/hardware.md (Paragraph je Fall).
 */

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "core/cards/printer/rom_sd1157.h"
#include "core/cards/printer/rom_sd1157_zg.h"
#include "core/cards/printer/sd1157_steuerung.h"

#ifndef SD1157_ABZUG_DIR
#error "SD1157_ABZUG_DIR fehlt (tests/unit/CMakeLists.txt)"
#endif

namespace {

// ─── MD5 (RFC 1321), nur fuer die Pruefsumme des ROM-Abbilds ────────────────
std::string md5Hex(const uint8_t* data, size_t len)
{
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                              5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                              6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
    std::vector<uint8_t> m(data, data + len);
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    const uint64_t bits = static_cast<uint64_t>(len) * 8;
    for (int i = 0; i < 8; ++i) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; ++i)
            w[i] = m[off + 4 * i] | (m[off + 4 * i + 1] << 8) | (m[off + 4 * i + 2] << 16) |
                   (static_cast<uint32_t>(m[off + 4 * i + 3]) << 24);
        uint32_t a = a0, b = b0, c = c0, d = d0;
        for (int i = 0; i < 64; ++i) {
            uint32_t f;
            int g;
            if (i < 16)      { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d;          g = (3 * i + 5) % 16; }
            else             { f = c ^ (b | ~d);       g = (7 * i) % 16; }
            f = f + a + K[i] + w[g];
            a = d; d = c; c = b;
            b = b + ((f << S[i]) | (f >> (32 - S[i])));
        }
        a0 += a; b0 += b; c0 += c; d0 += d;
    }
    char buf[33];
    uint32_t r[4] = {a0, b0, c0, d0};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) std::snprintf(buf + 8 * i + 2 * j, 3, "%02x", (r[i] >> (8 * j)) & 0xFF);
    return std::string(buf, 32);
}

/// MD5 der Datei @p name aus der Tabelle in doc/EPROMS/SD1157/README.md.
std::string md5AusReadme(const std::string& name)
{
    std::ifstream f(std::string(SD1157_ABZUG_DIR) + "/README.md");
    std::string zeile;
    while (std::getline(f, zeile)) {
        if (zeile.rfind("| `" + name + "`", 0) != 0) continue;
        // | `1157.rom` | 10 624 B (0000H–297FH) | 09c7... | Mikroprogramm |
        size_t pos = 0;
        for (int i = 0; i < 2; ++i) pos = zeile.find('|', pos + 1);   // vor der dritten Spalte (MD5)
        const size_t ende = zeile.find('|', pos + 1);
        std::string md5 = zeile.substr(pos + 1, ende - pos - 1);
        const size_t a = md5.find_first_not_of(' ');
        const size_t b = md5.find_last_not_of(' ');
        return a == std::string::npos ? std::string() : md5.substr(a, b - a + 1);
    }
    return {};
}

std::vector<uint8_t> dateiBytes(const std::string& name)
{
    std::ifstream f(std::string(SD1157_ABZUG_DIR) + "/" + name, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

struct Karte {
    K1520Bus bus;
    Sd1157Steuerung st;
    explicit Karte(const Sd1157Steuerung::Config& cfg = {}) : st(bus, cfg) {
        st.attachToBus(bus);
        bus.setInterruptChain({&st});
        st.powerOn();
    }
    uint8_t in(uint8_t p) { return bus.ioRead(p); }
    void out(uint8_t p, uint8_t d) { bus.ioWrite(p, d); }
    uint8_t rd(uint16_t a) { return bus.memRead(a); }
    void wr(uint16_t a, uint8_t d) { bus.memWrite(a, d); }
    void takte(int n, int schritt = 50) {
        for (int i = 0; i < n; i += schritt) st.clockTick(schritt);
    }
    bool intAn() { bus.markIntDirty(); bus.updateInterruptChain(); return bus.isINT(); }
};

// ─── ROM ─────────────────────────────────────────────────────────────────────

TEST(Sd1157Karte, RomAbbildHatDieMd5AusDemReadme)
{
    ASSERT_FALSE(md5AusReadme("1157.rom").empty());
    EXPECT_EQ(sizeof(SD1157_ROM), 10624u);   // 0000H–297FH
    EXPECT_EQ(md5Hex(SD1157_ROM, sizeof(SD1157_ROM)), md5AusReadme("1157.rom"));
    EXPECT_EQ(sizeof(SD1157_ZG1), 1024u);
    EXPECT_EQ(md5Hex(SD1157_ZG1, sizeof(SD1157_ZG1)), md5AusReadme("1157zg.rom"));
}

TEST(Sd1157Karte, EingebettetesAbbildIstDieDateiUnveraendert)
{
    const auto rom = dateiBytes("1157.rom");
    ASSERT_EQ(rom.size(), sizeof(SD1157_ROM));
    EXPECT_TRUE(std::equal(rom.begin(), rom.end(), SD1157_ROM));
    const auto zg = dateiBytes("1157zg.rom");
    ASSERT_EQ(zg.size(), sizeof(SD1157_ZG1));
    EXPECT_TRUE(std::equal(zg.begin(), zg.end(), SD1157_ZG1));
}

TEST(Sd1157Karte, DieVorgabeIstDerRepariertAbzugUnterscheidetSichNurInByte0D34H)
{
    const auto orig = dateiBytes("1157.rom");
    const auto rep = dateiBytes("1157_repariert.rom");
    ASSERT_EQ(orig.size(), rep.size());
    ASSERT_FALSE(md5AusReadme("1157_repariert.rom").empty());
    EXPECT_EQ(md5Hex(rep.data(), rep.size()), md5AusReadme("1157_repariert.rom"));
    int diff = 0;
    for (size_t i = 0; i < orig.size(); ++i)
        if (orig[i] != rep[i]) { ++diff; EXPECT_EQ(i, 0x0D34u); EXPECT_EQ(orig[i], 0x2B); EXPECT_EQ(rep[i], 0x0B); }
    EXPECT_EQ(diff, 1);
    Karte vorgabe;
    Sd1157Steuerung::Config co; co.rom_repariert = false;
    Karte original(co);
    for (size_t i = 0; i < rep.size(); ++i) {
        ASSERT_EQ(vorgabe.rd(static_cast<uint16_t>(i)), rep[i]) << i;
        ASSERT_EQ(original.rd(static_cast<uint16_t>(i)), orig[i]) << i;
    }
}

// ─── Speicherkarte (hardware.md §2) ──────────────────────────────────────────

TEST(Sd1157Karte, RomLiegtAuf0000BisFFFFH297FUndIstNichtBeschreibbar)
{
    Karte k;
    EXPECT_EQ(k.rd(0x0000), SD1157_ROM[0]);          // IM 2 / LD A,2CH (sy00)
    EXPECT_EQ(k.rd(0x0000), 0xED);                   // IM 2 = ED 5E
    EXPECT_EQ(k.rd(0x297F), SD1157_ROM[0x297F]);
    EXPECT_EQ(k.rd(0x0066), 0xC3);                   // NMI-Vektor: JP 0433H
    const uint8_t alt = k.rd(0x0100);
    k.wr(0x0100, static_cast<uint8_t>(alt ^ 0xFF));
    EXPECT_EQ(k.rd(0x0100), alt);
}

TEST(Sd1157Karte, UnbelegterRaumZwischenRomUndRamLiestFF)
{
    Karte k;
    EXPECT_EQ(k.rd(0x2980), 0xFF);
    EXPECT_EQ(k.rd(0x2BFF), 0xFF);
    k.wr(0x2BFF, 0x55);
    EXPECT_EQ(k.rd(0x2BFF), 0xFF);
}

TEST(Sd1157Karte, RamGrenzen2C00Bis2FFFUnd3800Bis3BFF)
{
    Karte k;
    k.wr(0x2C00, 0x11); k.wr(0x2FFF, 0x22); k.wr(0x3800, 0x33); k.wr(0x3BFF, 0x44);
    EXPECT_EQ(k.rd(0x2C00), 0x11);
    EXPECT_EQ(k.rd(0x2FFF), 0x22);
    EXPECT_EQ(k.rd(0x3800), 0x33);
    EXPECT_EQ(k.rd(0x3BFF), 0x44);
    // eine Zelle ausserhalb schreibt nirgends hinein
    k.wr(0x2BFF, 0x99); k.wr(0x3C00, 0x99);
    EXPECT_EQ(k.rd(0x2C00), 0x11);
    EXPECT_EQ(k.rd(0x3BFF), 0x44);
    EXPECT_EQ(k.rd(0x3C00), 0xFF);
}

TEST(Sd1157Karte, Zg1Auf3000HUndZg2Auf3400HLiestFF)
{
    Karte k;
    for (int i = 0; i < 1024; ++i) ASSERT_EQ(k.rd(static_cast<uint16_t>(0x3000 + i)), SD1157_ZG1[i]) << i;
    // Der Testtext der Selbstpruefung beginnt bei 3300H: CR LF "SERIENDRUCKER" (§3)
    EXPECT_EQ(k.rd(0x3300), 0x0D);
    EXPECT_EQ(k.rd(0x3301), 0x0A);
    EXPECT_EQ(k.rd(0x3302), 'S');
    // ZG2 fehlt und muss FFH lesen, nicht 00H — sonst hielte die Firmware ZG2 fuer bestueckt (§2)
    EXPECT_EQ(k.rd(0x3400), 0xFF);
    EXPECT_EQ(k.rd(0x37FF), 0xFF);
    k.wr(0x37FF, 0x00);
    EXPECT_EQ(k.rd(0x37FF), 0xFF);
    // ZG1 ist ROM
    k.wr(0x3000, 0x12);
    EXPECT_EQ(k.rd(0x3000), SD1157_ZG1[0]);
}

// ─── Eingabeports (hardware.md §4, §15) ──────────────────────────────────────

TEST(Sd1157Karte, Bch8212SystemRuhepegel)
{
    Karte k;
    // /PE=1 (Papier da), 6 lpi, EBD=1 (Einbahn), BDA=0 (schmal), Formularlaenge 12" je Bahn
    EXPECT_EQ(k.in(0xBC), 0xF7);
    k.st.setPapierDa(false);
    EXPECT_EQ(k.in(0xBC) & 0x01, 0);
    k.st.setPapierDa(true);
    k.st.setLpi6(false);
    EXPECT_EQ(k.in(0xBC) & 0x02, 0);
}

TEST(Sd1157Karte, BchBreitUndZweibahnigAusDerConfig)
{
    Sd1157Steuerung::Config c;
    c.breit = true;
    c.einbahn = false;
    c.formular_bahn1 = 0;
    Karte k(c);
    const uint8_t v = k.in(0xBC);
    EXPECT_EQ(v & 0x04, 0);        // EBD = 0: 2 Bahnen
    EXPECT_EQ(v & 0x08, 0x08);     // BDA = 1: breit
    EXPECT_EQ((v >> 4) & 3, 0);    // Bahn 1: 4"
    EXPECT_EQ((v >> 6) & 3, 3);    // Bahn 2: 12"
}

TEST(Sd1157Karte, Schalterblock3Auf6FH)
{
    Karte k;
    EXPECT_EQ(k.in(0x6F), 0x20);   // nur D5 (8 Bit) [?]
}

TEST(Sd1157Karte, NadelnBEHUndBFHWerdenNurGespeichert)
{
    Karte k;
    EXPECT_EQ(k.st.nadeln(), 0x000);          // FFH/FFH = alle aus
    k.out(0xBE, 0xFE);                        // Nadel 1 low = feuert
    EXPECT_EQ(k.st.nadeln(), 0x001);
    k.out(0xBF, 0x7F);                        // D7 = 0: Nadel 9
    EXPECT_EQ(k.st.nadeln(), 0x101);
    k.out(0xBF, 0xFF);
    k.out(0xBE, 0xFF);
    EXPECT_EQ(k.st.nadeln(), 0x000);
    EXPECT_EQ(k.in(0xBE), 0xFF);              // nur Ausgabe
    EXPECT_EQ(k.in(0xBF), 0xFF);
}

TEST(Sd1157Karte, Pio3BTastenUndSerParRuhepegel)
{
    Karte k;
    k.out(0xEF, 0xCF);   // PIO 3 B: Modus 3, alles Eingaenge (Q:57-58, §5.1)
    k.out(0xEF, 0xFF);
    // 3B.0 = 0 seriell, .1 = 0 online, .2/.3/.6 = 1 Tasten nicht gedrueckt, .4 = 0, .5 = 1 (ZFV), .7 = 1 [?]
    EXPECT_EQ(k.in(0xED), 0xEC);
    k.st.setOnline(false);
    EXPECT_EQ(k.in(0xED) & 0x02, 0x02);
    k.st.setTastePV1(true);
    EXPECT_EQ(k.in(0xED) & 0x04, 0x00);
    k.st.setTastePV2(true);
    EXPECT_EQ(k.in(0xED) & 0x08, 0x00);
    k.st.setTasteTest(true);
    EXPECT_EQ(k.in(0xED) & 0x40, 0x00);
    EXPECT_EQ(k.in(0xED) & 0x20, 0x20);       // ZFV bleibt 1
}

TEST(Sd1157Karte, Pio3AMode3LiefertInterfaceSchalter)
{
    Karte k;
    // Firmware stellt PIO 3 A erst auf Modus 2, dann (serielles Interface) auf Modus 3 um (§5.1)
    k.out(0xEE, 0x8F); k.out(0xEE, 0x87); k.out(0xEE, 0xFC);
    k.out(0xEE, 0xCF); k.out(0xEE, 0xFF); k.out(0xEE, 0x07);
    EXPECT_EQ(k.in(0xEC), 0x02);               // D0 = 0: IFSS; Stoppbits 01 -> 1; 9600 Bd

    Sd1157Steuerung::Config c;
    c.schnittstelle = Sd1157Steuerung::Config::Schnittstelle::V24;
    c.baud_code = 5;                           // 300 Bd (§7)
    c.paritaet_ein = true;
    Karte v(c);
    v.out(0xEE, 0xCF); v.out(0xEE, 0xFF);
    EXPECT_EQ(v.in(0xEC), 0x01 | 0x02 | 0x10 | (5 << 5));
}

TEST(Sd1157Karte, Pio1RaenderPapierschrittUndKarteneinzug)
{
    Karte k;
    k.out(0xF6, 0xCF); k.out(0xF6, 0x30);      // PIO 1 A: Eingaenge D4 D5 (§5.1)
    k.out(0xF7, 0xCF); k.out(0xF7, 0x31);      // PIO 1 B: Eingaenge D0 D4 D5
    // Startlage Takt 300: weder links noch rechts am Rand, kein PV-Impuls
    EXPECT_EQ(k.in(0xF4) & 0x30, 0x30);        // /LR = 1, PV-Schritt 1 = 1
    EXPECT_EQ(k.in(0xF5) & 0x31, 0x30);        // INA = 0 (kein Karteneinzug), PV-Schritt 2, /RR = 1
}

TEST(Sd1157Karte, Pio2AusgangsbitsLesenIhrLatchEingaengeDenPin)
{
    Karte k;
    k.out(0xFA, 0xCF); k.out(0xFA, 0x29);      // PIO 2 A: Eingaenge D0 D3 D5 (§5.1)
    k.out(0xF8, 0x12);                         // Ausgaenge D1 (Online-Lampe) und D4 (VW)
    const uint8_t v = k.in(0xF8);
    EXPECT_EQ(v & 0x12, 0x12);                 // Latch
    EXPECT_EQ(v & 0x08, 0x08);                 // /VK = 1 (Wagen nicht ganz links)
    EXPECT_EQ(v & 0x20, 0x00);                 // LAUF = 0
    EXPECT_TRUE(k.st.lampeOnline());
    k.out(0xF9, 0x00);
    EXPECT_TRUE(k.st.lampeFehler());           // 2B.3 = 0 -> /TF aktiv
    k.out(0xF9, 0x08);
    EXPECT_FALSE(k.st.lampeFehler());
}

// ─── CTC, SIO ────────────────────────────────────────────────────────────────

TEST(Sd1157Karte, CtcAufDCHBisDFH)
{
    Karte k;
    k.out(0xDC, 0xE0);                         // Vektorbasis E0H (Q:135-137)
    k.out(0xDC, 0x17);                         // Kanal 0: Zeitgeber, Vorteiler 16, TC folgt (§6)
    k.out(0xDC, 0x01);
    const auto st = k.st.ctc().debugState();
    EXPECT_EQ(st.vecBase, 0xE0);
    EXPECT_TRUE(st.ch[0].running);
    EXPECT_EQ(st.ch[0].timeConst, 0x01);
    EXPECT_FALSE(st.ch[1].running);
    k.out(0xDC, 0x13);                         // Reset ohne TC stoppt den Kanal
    EXPECT_FALSE(k.st.ctc().debugState().ch[0].running);
}

TEST(Sd1157Karte, SioKanaeleSindA7CHDatenB7DHDatenUndSteuerUnten)
{
    Karte k;
    k.out(0x7F, 0x02); k.out(0x7F, 0xF0);      // WR2 immer in Kanal B (§7)
    EXPECT_EQ(k.st.sio().debugState().ch[1].wr2, 0xF0);
    // Kanal A (V.24): WR5 = 68H (Tx 8 Bit, ein), dann ein Zeichen auf 7CH -> geht in die Senke
    k.out(0x7E, 0x05); k.out(0x7E, 0x68);
    k.out(0x7C, 'X');
    k.takte(100);
    EXPECT_EQ(k.st.sioAusgabe(0), "X");
    EXPECT_EQ(k.st.sioAusgabe(1), "");
    // Kanal B (IFSS): 7DH
    k.out(0x7F, 0x05); k.out(0x7F, 0x68);
    k.out(0x7D, 'Y');
    k.takte(100);
    EXPECT_EQ(k.st.sioAusgabe(1), "Y");
    EXPECT_EQ(k.st.sioAusgabe(0), "X");
}

// ─── Interruptkette ──────────────────────────────────────────────────────────

TEST(Sd1157Karte, CtcKanal1LiefertVektorE2UndRetiRaeumtAuf)
{
    Karte k;
    k.out(0xDC, 0xE0);
    k.out(0xDD, 0xB7); k.out(0xDD, 0x01);      // Kanal 1: Zeitgeber, Vorteiler 256, Interrupt frei, TC 1
    EXPECT_FALSE(k.intAn());
    k.takte(600);                              // 256 Takte -> Nulldurchgang
    ASSERT_TRUE(k.intAn());
    EXPECT_EQ(k.bus.interruptAcknowledge(), 0xE2);
    k.bus.signalRETI();
    k.bus.markIntDirty(); k.bus.updateInterruptChain();
    EXPECT_FALSE(k.st.inService());
}

TEST(Sd1157Karte, CtcHatVorrangVorPio2)
{
    Karte k;
    // CTC Kanal 1 und PIO 2 A (Takt vorwaerts, Vektor E8H) fordern gleichzeitig an: CTC zuerst (§8, Annahme [?])
    k.out(0xDC, 0xE0);
    k.out(0xFA, 0xCF); k.out(0xFA, 0x29); k.out(0xFA, 0xB7); k.out(0xFA, 0xFE); k.out(0xFA, 0xE8);
    k.out(0xDD, 0xB7); k.out(0xDD, 0x01);
    k.out(0xF8, 0x50);                         // LFG + VW: Wagen faehrt vorwaerts, Takt-Impulse auf 2A.0
    k.takte(20000, 20);
    ASSERT_TRUE(k.intAn());
    EXPECT_EQ(k.bus.interruptAcknowledge(), 0xE2);
    k.bus.signalRETI();
    ASSERT_TRUE(k.intAn());
    EXPECT_EQ(k.bus.interruptAcknowledge(), 0xE8);
}

// ─── Wagen und Papier am Druckwerk (WP3) ─────────────────────────────────────

TEST(Sd1157Karte, LaufIstSofortMitLfgUndRaenderFolgenDerPosition)
{
    Karte k;
    k.out(0xFA, 0xCF); k.out(0xFA, 0x29);      // PIO 2 A: Eingaenge D0 D3 D5
    k.out(0xF6, 0xCF); k.out(0xF6, 0x30);      // PIO 1 A: Eingaenge D4 D5
    EXPECT_EQ(k.in(0xF8) & 0x20, 0x00);        // LAUF = 0
    EXPECT_EQ(k.in(0xF4) & 0x20, 0x20);        // /LR = 1 (Startlage 300)

    k.out(0xF8, 0x40);                         // LFG = 1, VW = 0: rueckwaerts, VTAB = 0
    EXPECT_EQ(k.in(0xF8) & 0x20, 0x20);        // LAUF binnen µs, ohne dass Zeit verging (§10.1)

    // bis /LR = 0 (Position <= 76): 224 Takt-Abstaende
    int n = 0;
    while ((k.in(0xF4) & 0x20) && n++ < 400000) k.takte(50);
    EXPECT_LT(n, 400000);
    EXPECT_LE(k.st.druck().wagen().position(), 76);
    EXPECT_GT(k.st.druck().wagen().position(), 60);

    // LFG weg: Auslauf, LAUF bleibt kurz 1 und faellt dann (§15 Nr. 10)
    k.out(0xF8, 0x00);
    EXPECT_EQ(k.in(0xF8) & 0x20, 0x20);
    k.takte(100000);
    EXPECT_EQ(k.in(0xF8) & 0x20, 0x00);
    EXPECT_FALSE(k.st.druck().wagen().laeuft());
    EXPECT_EQ(k.in(0xF4) & 0x20, 0x00);        // /LR bleibt 0, der Wagen steht links
}

TEST(Sd1157Karte, TaktImpulseAufRichtungsgetrenntenLeitungen)
{
    Karte k;
    k.out(0xFA, 0xCF); k.out(0xFA, 0x29);      // PIO 2 A: D0 = Takt vorwaerts
    k.out(0xFB, 0xCF); k.out(0xFB, 0x01);      // PIO 2 B: D0 = Takt rueckwaerts
    k.out(0xF8, 0x50);                         // vorwaerts
    int hoch_vor = 0, hoch_rueck = 0;
    for (int i = 0; i < 400; ++i) {
        k.takte(10, 10);
        hoch_vor += k.in(0xF8) & 0x01;
        hoch_rueck += k.in(0xF9) & 0x01;
    }
    EXPECT_GT(hoch_vor, 0);
    EXPECT_EQ(hoch_rueck, 0);
}

TEST(Sd1157Karte, PapierschritteKommenNurBeiXpfgUndImmerUnter7Ms)
{
    Karte k;
    k.out(0xF6, 0xCF); k.out(0xF6, 0x30);      // PIO 1 A: D4 = PV-Schritt 1
    k.takte(20000);
    EXPECT_EQ(k.st.druck().papier(0).schritte(), 0u);
    k.out(0xF4, 0x08 | 0x04);                  // XPFG1 = 1 (XPWV1 = 1)
    k.takte(7 * 2457);                         // 7 ms
    EXPECT_GE(k.st.druck().papier(0).schritte(), 1u);   // Anlauf FRH = 0: 4,5 ms < 7,08 ms (§10.2)
    k.out(0xF4, 0x08);
    const uint32_t n = k.st.druck().papier(0).schritte();
    k.takte(40000);
    EXPECT_EQ(k.st.druck().papier(0).schritte(), n);
}

// ─── Nadelanschlag mit FGDI (hardware.md §10.4, §15 Nr. 14) ─────────────────

TEST(Sd1157Karte, DieFlankeFgdiSchlaegtDasNadelmusterAnNichtDasSchreibenInsLatch)
{
    Karte k;
    const auto& b = k.st.druck().blatt(0);
    const int y = k.st.druck().papierY(0);
    k.out(0xBE, 0xFE);                          // Nadel 1 (low = feuern)
    k.out(0xBF, 0x7F);                          // Nadel 9 (D7 = 0)
    EXPECT_EQ(b.punktzahl(), 0u) << "das Schreiben ins Latch loest nichts aus (§15 Nr. 7)";
    k.out(0xF9, 0x04 | 0x18);                   // 2B: KAS = 1, FGDI = 0
    EXPECT_EQ(b.punktzahl(), 0u);
    k.out(0xF9, 0x04 | 0x18 | 0x20);            // FGDI 0 -> 1: Anschlag bei der Wagenposition (Start 300)
    EXPECT_EQ(b.punktzahl(), 4u) << "Nadel 1 und 9, je zwei Pixel";
    EXPECT_TRUE(b.punkt(600, y));
    EXPECT_TRUE(b.punkt(601, y));
    EXPECT_TRUE(b.punkt(600, y + 16));
    EXPECT_FALSE(b.punkt(600, y + 2));
    k.out(0xF9, 0x04 | 0x18 | 0x20);            // FGDI bleibt 1: keine zweite Flanke
    EXPECT_EQ(k.st.druck().kopf().anschlaege(), 1u);
    k.out(0xF9, 0x04 | 0x18);
    k.out(0xF9, 0x04 | 0x18 | 0x20);            // zweite Flanke, derselbe Ort: keine neuen Punkte
    EXPECT_EQ(k.st.druck().kopf().anschlaege(), 2u);
    EXPECT_EQ(b.punktzahl(), 4u);
}

TEST(Sd1157Karte, KopfNichtAngeschwenktBringtKeinenPunktAufsPapier)
{
    Karte k;
    k.out(0xBE, 0xFE);
    k.out(0xF9, 0x18 | 0x20);                   // KAS = 0
    EXPECT_EQ(k.st.druck().blatt(0).punktzahl(), 0u);
    EXPECT_EQ(k.st.druck().kopf().abgewiesen(), 1u);
    // die Taste Kopf haelt ihn dauernd am Papier
    k.st.setKopf(true);
    k.out(0xF9, 0x18);
    k.out(0xF9, 0x18 | 0x20);
    EXPECT_EQ(k.st.druck().blatt(0).punktzahl(), 2u);
}

// ─── Tasten, Schalter, Lampen als API ────────────────────────────────────────

TEST(Sd1157Karte, TastenUndSchalterErscheinenAlsPegelAmPortPio3BUndBch)
{
    Karte k;
    k.out(0xEF, 0xCF); k.out(0xEF, 0xFF);       // PIO 3 B: Modus 3, alles Eingaenge
    EXPECT_EQ(k.in(0xED) & 0x7E, 0x6C) << "online: Bit 1 = 0, Tasten PV1/PV2/TEST nicht gedrueckt = 1, ZFV = 1";
    EXPECT_EQ(k.in(0xED) & 0x02, 0x00) << "ONLINE: 0 = online";
    k.st.setOnline(false);
    EXPECT_EQ(k.in(0xED) & 0x02, 0x02);
    EXPECT_FALSE(k.st.online());
    k.st.setTastePV1(true);  EXPECT_EQ(k.in(0xED) & 0x04, 0x00); EXPECT_TRUE(k.st.tastePV1());
    k.st.setTastePV2(true);  EXPECT_EQ(k.in(0xED) & 0x08, 0x00); EXPECT_TRUE(k.st.tastePV2());
    k.st.setTasteTest(true); EXPECT_EQ(k.in(0xED) & 0x40, 0x00); EXPECT_TRUE(k.st.tasteTest());
    k.st.setTastePV1(false); k.st.setTastePV2(false); k.st.setTasteTest(false);
    EXPECT_EQ(k.in(0xED) & 0x4C, 0x4C);
    EXPECT_EQ(k.in(0xED) & 0x10, 0x00) << "Kopf nicht dauernd";
    k.st.setKopf(true);
    EXPECT_EQ(k.in(0xED) & 0x10, 0x10);
    EXPECT_TRUE(k.st.kopf());
    k.st.setLpi6(false);  EXPECT_EQ(k.in(0xBC) & 0x02, 0x00);
    k.st.setLpi6(true);   EXPECT_EQ(k.in(0xBC) & 0x02, 0x02);
    EXPECT_EQ(k.in(0xBC) & 0x01, 0x01) << "/PE = 1: Papier da";
    k.st.setPapierDa(false);
    EXPECT_EQ(k.in(0xBC) & 0x01, 0x00);
    EXPECT_FALSE(k.st.papierDa());
}

TEST(Sd1157Karte, LampenSindLesbarOnlineHighAktivFehlerLowAktiv)
{
    Karte k;
    EXPECT_FALSE(k.st.lampeOnline());           // Ausgang 2A.1 nach dem Einschalten 0
    EXPECT_FALSE(k.st.lampeFehler());           // /TF = 1 (2B = 18H)
    k.out(0xF8, 0x02);
    EXPECT_TRUE(k.st.lampeOnline());
    k.out(0xF9, 0x10);                          // 2B.3 = 0: /TF aktiv
    EXPECT_TRUE(k.st.lampeFehler());
}

TEST(Sd1157Karte, ResetLoeschtRegisterAberNichtDenRam)
{
    Karte k;
    k.wr(0x2C10, 0x5A);
    k.out(0xDC, 0xE0); k.out(0xDC, 0x17); k.out(0xDC, 0x01);
    k.out(0xBE, 0x00);
    k.st.reset();
    EXPECT_EQ(k.rd(0x2C10), 0x5A);
    EXPECT_FALSE(k.st.ctc().debugState().ch[0].running);
    EXPECT_EQ(k.st.nadeln(), 0x000);
    EXPECT_EQ(k.st.cpu().PC, 0x0000);
}

}  // namespace
