/**
 * @file test_k6311.cpp
 * @brief K6311 (Entwurf 49 WP8e): ROM 12128062 auf der Karte `K6313Ccb` (Variante `k6311`).  **Es gibt keinen Schaltplan des K6311**: die
 *        Hardware ist nur aus dem ROM erschlossen [?] (doc/printer/k6313/firmware_diff.md §6, Merkposten drucker.md Nr. 34).  Diese Tests
 *        pruefen deshalb nur, was das ROM selbst belegt (Anlauf bis zur Leerlaufschleife, Auswertung von BCH/BDH, Annahme von Text,
 *        ROM-Byte 27F7H) und halten die **Grenze** fest: es wird nichts gedruckt, weil die Motor-/Nadelbelegung des K6311 nicht bekannt ist.
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/machines/printer/printer_machine.h"

using Typ = PrinterMachine::Config::Typ;

#ifndef K6311_ABZUG_DIR
#error "K6311_ABZUG_DIR fehlt (tests/integration/CMakeLists.txt)"
#endif

namespace {
struct Lauf {
    std::unique_ptr<PrinterMachine> m;
    explicit Lauf(PrinterMachine::Config c = {})
    {
        c.typ = Typ::K6311_V24;
        m = std::make_unique<PrinterMachine>(c);
    }
    K6313Ccb& st() { return m->k6313(); }
    void fahre(uint64_t takte)
    {
        const uint64_t z = m->totalCycles() + takte;
        while (m->totalCycles() < z) m->run(2000);
    }
    void sende(const std::string& t)
    {
        m->serialHub()->wandler(1).anbinden();
        fahre(1'000'000);
        for (char ch : t) {
            const uint8_t b = static_cast<uint8_t>(ch);
            int g = 0;
            while (m->serialHub()->wandler(1).fernFrei() == 0 && g++ < 1000) fahre(3000);
            m->serialHub()->wandler(1).fernGib(&b, 1);
            fahre(3000);
        }
    }
    uint16_t wort(uint16_t a) { return static_cast<uint16_t>(m->memReadDebug(a) | (m->memReadDebug(a + 1) << 8)); }
};

std::vector<uint8_t> abzug(const char* name)
{
    std::ifstream f(std::string(K6311_ABZUG_DIR) + "/" + name, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

k1520::serial::SerialFormat format(Lauf& l) { return l.m->serielleAnschluesse()[1]->format(); }
}  // namespace

TEST(K6311Modul, EingebettetesRomIstDerAbzug12128062)
{
    const auto r = abzug("12128062.bin");
    ASSERT_EQ(r.size(), 10240u);
    Lauf l;
    for (uint16_t a = 0; a < 0x2800; ++a) ASSERT_EQ(l.m->memReadDebug(a), r[a]) << a;
    EXPECT_STREQ(l.st().deviceName(), "K6311 CCB [?]");
}

TEST(K6311Modul, LaeuftAnBisZurLeerlaufschleifeUndRichtetDieSioEin)
{
    // Das ROM durchlaeuft Einrichtung, Wagen-/Formularmotor-Ablauf und SIO-Einrichtung (7EH/7FH) und endet in der Schleife 01AAH-01D9H
    // (Eingangspuffer 03C4H leer, SIO-Status 04CDH) [E: Trace].  Nach ~1 Mio. Takten, ein Vielfaches davon als Reserve.
    Lauf l;
    l.fahre(3'000'000);
    int in_schleife = 0;
    for (int i = 0; i < 200; ++i) {
        l.fahre(997);
        const uint16_t pc = l.m->cpuPC();
        if (pc >= 0x01AA && pc <= 0x04F0) ++in_schleife;
    }
    EXPECT_EQ(in_schleife, 200) << "die Firmware verlaesst die Leerlaufschleife nicht";
    EXPECT_TRUE(l.m->cpu().IFF1);
    EXPECT_TRUE(l.st().sioAusgabe(0).empty());
}

TEST(K6311Modul, BchBdhWerdenVomRomAlsBaudUndFormatSchalterAusgewertet)
{
    // Auswertung im ROM (00AFH-00CCH Baud, 00FAH-0161H Format) [E]; dass dort DIL-Schalter einer V.24-Platte liegen, ist [?].
    struct F { uint8_t bc, bd; uint32_t baud; uint8_t daten, par; };
    const F faelle[] = {
        {0xFF, 0x3F, 9600, 8, 0},   // Vorgabe des Emulators
        {0xBF, 0x3F, 4800, 8, 0},   // Bit 5 = 1, Bit 6 = 0, Bit 7 = 1: Zeitkonstante 2
        {0x3F, 0x3F, 2400, 8, 0},   // Bit 7 = 0 verdoppelt: 4
        {0xDF, 0x3F, 1200, 8, 0},   // Bit 5 = 0, Bit 6 = 1: 8
        {0x9F, 0x3F, 300, 8, 0},    // Bit 5 = 0, Bit 6 = 0: 32
        {0xFF, 0xFF, 9600, 7, 1},   // BDH Bit 7 = 1: 7 Bit; Bit 6 = 1 Paritaet; Bit 5 = 1 ungerade
        {0xFF, 0xDF, 9600, 7, 2},   // Bit 5 = 0: gerade
    };
    for (const F& f : faelle) {
        PrinterMachine::Config c;
        c.k6313.schalter_baud = f.bc;
        c.k6313.schalter_format = f.bd;
        Lauf l(c);
        l.fahre(3'000'000);
        const auto fm = format(l);
        EXPECT_TRUE(fm.gueltig) << std::hex << int(f.bc) << "/" << int(f.bd);
        EXPECT_EQ(fm.baud_nenn, f.baud) << std::hex << int(f.bc);
        EXPECT_EQ(fm.daten, f.daten) << std::hex << int(f.bd);
        EXPECT_EQ(fm.paritaet, f.par) << std::hex << int(f.bd);
    }
}

TEST(K6311Modul, RomByte27F7WirdInDreiRamZellenKopiert)
{
    // 12128062 (…802): 27F7H = 20H; 12128062_1 (…803): 30H.  Das ROM kopiert es nach 3836H, 386DH, 383BH (0074H-007DH) [E].
    // Dass es der Zeichensatz (Deutsch / UK) ist, ist nur aus der Etikettenliste geschlossen [?] und wird hier nicht geprueft.
    for (const char* name : {"12128062.bin", "12128062_1.bin"}) {
        PrinterMachine::Config c;
        c.k6313.rom_extern = abzug(name);
        ASSERT_EQ(c.k6313.rom_extern.size(), 10240u) << name;
        const uint8_t soll = c.k6313.rom_extern[0x27F7];
        Lauf l(c);
        l.fahre(200'000);
        EXPECT_EQ(l.m->memReadDebug(0x3836), soll) << name;
        EXPECT_EQ(l.m->memReadDebug(0x386D), soll) << name;
        EXPECT_EQ(l.m->memReadDebug(0x383B), soll) << name;
    }
}

TEST(K6311Modul, TextUeberV24WirdAngenommenUndAusDemEingangspufferGelesen)
{
    // Der Ringpuffer-Zeiger (384CH/384EH, Start 3977H) laeuft um genau die gesendete Zeichenzahl weiter und beide Zeiger treffen sich wieder,
    // d. h. das ROM hat jedes Zeichen aus der SIO geholt und abgearbeitet [E].  Das Druckwerk reagiert erst im naechsten Test.
    Lauf l;
    l.fahre(3'000'000);
    const uint16_t start = l.wort(0x384C);
    ASSERT_EQ(start, 0x3977);
    const std::string t = "HELLO 123\r\nZWEI\r\n";
    l.sende(t);
    l.fahre(40'000'000);
    EXPECT_EQ(l.wort(0x384C), start + t.size());
    EXPECT_EQ(l.wort(0x384E), l.wort(0x384C));
}

TEST(K6311Modul, GrenzeDieBelegungVonWagenUndNadelnIstUnbekanntEsWirdNichtGedruckt)
{
    // **Dokumentierte Luecke, kein Sollverhalten.**  Das ROM schreibt Wagen-Phasen als Zwei-Bit-Code in D104 A4/A5 (Werte 00H/20H/30H/10H an F8H)
    // statt in A1/A3/A5/A7 wie am K6313, die Formularmotor-Muster an F9H (93H/1EH/38H/F0H) passen nicht zur K6313-Belegung, und DDH
    // (Nadeln am K6313) wird nur beim Einrichten beschrieben.  Folge im Emulator: der Wagen steht, der Papiervorschub laeuft, es faellt
    // kein Nadelanschlag.  Wird die Belegung irgendwann erschlossen und nachgebildet, muss dieser Test durch ein Orakel ersetzt werden.
    Lauf l;
    l.fahre(3'000'000);
    const int wagen = l.m->druck().wagen().position();
    l.sende("HELLO 123\r\nZWEI\r\n");
    l.fahre(40'000'000);
    EXPECT_EQ(l.m->druck().kopf().anschlaege(), 0u);
    EXPECT_EQ(l.m->druck().wagen().position(), wagen);
    EXPECT_FALSE(l.st().lampe(1));
    EXPECT_FALSE(l.st().lampe(3));
    EXPECT_TRUE(l.st().lampe(2));
}

TEST(K6311Modul, DerK6313BleibtUnberuehrt)
{
    // BCH/BDH lesen am K6313 weiter FFH (Centronics-PIO nicht bestueckt), die Lampen bleiben belegt
    PrinterMachine::Config c;
    c.typ = Typ::K6313_V24;
    PrinterMachine m(c);
    EXPECT_FALSE(m.istK6311());
    EXPECT_EQ(m.ioReadDebug(0xBC), 0xFF);
    EXPECT_EQ(m.ioReadDebug(0xBD), 0xFF);
    EXPECT_STREQ(m.k6313().deviceName(), "K6313 CCB");
}
