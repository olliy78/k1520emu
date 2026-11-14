/**
 * @file test_k6311.cpp
 * @brief K6311 (Entwurf 49 WP8e/WP8f): ROM 12128062 auf der Karte `K6313Ccb` (Variante `k6311`).  **Es gibt keinen Schaltplan des K6311**: die
 *        Hardware ist nur aus dem ROM erschlossen [?] (doc/printer/k6313/firmware_diff.md §6, Merkposten drucker.md Nr. 34/35).  Diese Tests
 *        pruefen den Anlauf, die serielle Einrichtung und (WP8f) das **Druckbild Punkt fuer Punkt gegen den Zeichengenerator im ROM**
 *        (`tests/support/k6311_orakel.h`): Text ueber V.24 in beiden Druckrichtungen, alle 94 Zeichen, Selbsttest, Zeichensatzbyte 27F7H,
 *        Zeilenvorschub/Formularvorschub, Tasten und Papierende.  Die Zuordnung der E/A-Bits ist erschlossen [?], das Druckbild nicht.
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/machines/printer/printer_machine.h"
#include "tests/support/k6311_orakel.h"

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


namespace {
std::string zeilen(const std::vector<std::string>& z)
{
    std::string t;
    for (auto& e : z) t += e + "\r\n";
    return t;
}

/// Prueft alle Baender des Blatts gegen die Zeilen; Rueckgabe "" = gleich.  Ursprung aller Zeilen: +-2 Pixel gleich (Abgleich Vor-/Rueckwaertsdruck).
std::string pruefe(Lauf& l, const std::vector<uint8_t>& rom, const std::vector<std::string>& soll, int* ursprung0 = nullptr)
{
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = k6311test::baender(bl);
    if (b.size() != soll.size()) return "Baender: " + std::to_string(b.size()) + " statt " + std::to_string(soll.size());
    double ox0 = -1;
    for (size_t i = 0; i < b.size(); ++i) {
        const auto e = k6311test::vergleiche(bl, rom, soll[i], b[i].first, b[i].second);
        if (!e.fehler.empty()) return "Zeile " + std::to_string(i) + " '" + soll[i].substr(0, 20) + "': " + e.fehler;
        if (ox0 < 0) ox0 = e.ursprung;
        if (std::abs(e.ursprung - ox0) > 2) return "Zeile " + std::to_string(i) + ": Ursprung " + std::to_string(e.ursprung) + " statt " + std::to_string(ox0);
    }
    if (ursprung0) *ursprung0 = static_cast<int>(ox0);
    return "";
}
}  // namespace

TEST(K6311Modul, TextUeberV24DrucktUndDeckeSichPunktFuerPunktMitDemZeichengenerator)
{
    // Zeile 1 vorwaerts, Zeile 2 rueckwaerts (die Firmware druckt bidirektional): beide stimmen mit den Zellen im ROM (2280H + 8 * (Code - 20H)) ueberein.
    const auto r = k6311test::rom(K6311_ABZUG_DIR);
    Lauf l;
    l.fahre(20'000'000);
    ASSERT_TRUE(l.st().lampe(1)) << "ON LINE";
    l.sende(zeilen({"HELLO 123", "ZWEI", "ROBOTRON K6311 0987654321"}));
    l.fahre(80'000'000);
    EXPECT_EQ(pruefe(l, r, {"HELLO 123", "ZWEI", "ROBOTRON K6311 0987654321"}), "");
    const auto b = k6311test::baender(l.m->druck().blatt(0));
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[1].first - b[0].first, 24) << "Zeilenabstand 1/6 Zoll = 8 Papierschritte zu 1/48 Zoll";
    EXPECT_EQ(b[2].first - b[1].first, 24);
}

TEST(K6311Modul, AlleDruckbarenZeichenInklusiveUnterlaengenDeckenSichMitDemZeichengenerator)
{
    // Unterlaengen (g j p q y , ;) tragen in Byte 0 der Zelle die Nadel-9-Maske (Bit k = Spalte k+1, 0 = Nadel 9 schlaegt an).
    const auto r = k6311test::rom(K6311_ABZUG_DIR);
    Lauf l;
    l.fahre(20'000'000);
    const std::string z1 = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ", z2 = "[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
    l.sende(zeilen({z1, z2}));
    l.fahre(120'000'000);
    EXPECT_EQ(pruefe(l, r, {z1, z2}), "");
}

TEST(K6311Modul, ZeichensatzByte27F7WaehltDieNationaleTabelleUndDasDruckbildFolgt)
{
    // 12128062: 27F7H = 20H -> Tabelle 2220H = ASCII; 12128062_1: 27F7H = 30H -> Tabelle 2230H: # = 7FH (Pfund), @ = 82H (Paragraph), [ \ ] = Ae Oe Ue,
    // { | } ~ = ae oe ue ss.  Die Etikettenliste nennt ...802 "GERMAN" und ...803 "UK ASCII" - das Druckbild widerspricht ihr (Merkposten Nr. 35).
    for (const char* name : {"12128062", "12128062_1"}) {
        const auto r = k6311test::rom(K6311_ABZUG_DIR, name);
        ASSERT_EQ(r.size(), 10240u) << name;
        PrinterMachine::Config c;
        c.k6313.rom_extern = r;
        Lauf l(c);
        l.fahre(20'000'000);
        l.sende(zeilen({"#$@[\\]^`{|}~()<>"}));
        l.fahre(40'000'000);
        EXPECT_EQ(pruefe(l, r, {"#$@[\\]^`{|}~()<>"}), "") << name;
        const int zelle_hash = k6311test::zelleVon(r, '#');
        EXPECT_EQ(zelle_hash, r[0x27F7] == 0x30 ? 0x7F : 0x23) << name;
    }
}

TEST(K6311Modul, SelbsttestDruckKennungUndZeichenumlaufStimmenMitDemProgrammImRomUeberein)
{
    // ON LINE aus (Taste 1: D104 A2), dann Taste 2 (D105 A4) im Offline-Zustand: Selbsttest ab 0264H [F]: Kennungszeile aus den ROM-Bytes 27F8H-27FFH
    // (0F48H-0F5CH), danach je Zeile die Zeichen B+1...7EH, 20H... rund, 80 je Zeile, B = 20H, 21H, ...
    const auto r = k6311test::rom(K6311_ABZUG_DIR);
    Lauf l;
    l.fahre(20'000'000);
    l.st().setTaste(1, true);
    l.fahre(3'000'000);
    l.st().setTaste(1, false);
    l.fahre(6'000'000);
    ASSERT_FALSE(l.st().lampe(1)) << "OFF LINE";
    l.st().setTaste(2, true);
    l.fahre(60'000'000);
    l.st().setTaste(2, false);
    const Blattmodell& bl = l.m->druck().blatt(0);
    const auto b = k6311test::baender(bl);
    ASSERT_GE(b.size(), 8u);
    // Kennung: 27FBH-27FDH, 27F8H-27FAH, '0' + oberes Nibble von 27F7H (RLD), 27FEH-27FFH
    std::string kennung;
    for (int a : {0x27FB, 0x27FC, 0x27FD, 0x27F8, 0x27F9, 0x27FA}) kennung += static_cast<char>(r[static_cast<size_t>(a)]);
    kennung += static_cast<char>(0x30 | (r[0x27F7] >> 4));
    kennung += static_cast<char>(r[0x27FE]);
    kennung += static_cast<char>(r[0x27FF]);
    EXPECT_EQ(kennung, "121280262");
    EXPECT_EQ(k6311test::vergleiche(bl, r, kennung, b[0].first, b[0].second).fehler, "") << "Kennungszeile";
    for (int i = 1; i < 8; ++i) {
        std::string z;
        int a = 0x20 + i - 1;
        for (int n = 0; n < 80; ++n) { ++a; if (a == 0x7F) a = 0x20; z += static_cast<char>(a); }
        EXPECT_EQ(k6311test::vergleiche(bl, r, z, b[static_cast<size_t>(i)].first, b[static_cast<size_t>(i)].second).fehler, "") << "Selbsttest-Zeile " << i;
    }
}

TEST(K6311Modul, TasteOnLineSchaltetOfflineUndWiederOnline)
{
    // D104 A2 (aktiv low) ist die Taste ON LINE: die Firmware geht beim Druck offline (0226H loescht D105 A2 = Lampe), beim naechsten Druck wieder online (0180H).
    Lauf l;
    l.fahre(20'000'000);
    ASSERT_TRUE(l.st().lampe(1));
    ASSERT_FALSE(l.st().lampe(3));
    l.st().setTaste(1, true); l.fahre(3'000'000); l.st().setTaste(1, false); l.fahre(6'000'000);
    EXPECT_FALSE(l.st().lampe(1)) << "OFF LINE";
    l.st().setTaste(1, true); l.fahre(3'000'000); l.st().setTaste(1, false); l.fahre(6'000'000);
    EXPECT_TRUE(l.st().lampe(1)) << "wieder ON LINE";
    EXPECT_FALSE(l.st().lampe(3));
}

TEST(K6311Modul, TastenZeilenvorschubWirkenAufDasPapierSolangeSieGehaltenWerden)
{
    // Offline: Taste 3 (D105 A5) schiebt das Papier rueckwaerts (Dauer, Viererschritte, ROM 1126H), Taste 4 (D105 A6) vorwaerts (Dauer, 8 Schritte = 1/6 Zoll,
    // der Wagen faehrt vorher nach rechts); Zuordnung der Tasten zu Beschriftungen [?].
    for (int taste : {3, 4}) {
        Lauf l;
        l.fahre(20'000'000);
        l.st().setTaste(1, true); l.fahre(3'000'000); l.st().setTaste(1, false); l.fahre(6'000'000);
        ASSERT_FALSE(l.st().lampe(1));
        const int p0 = l.m->druck().papier(0).lage();
        l.st().setTaste(taste, true);
        l.fahre(8'000'000);
        const int p1 = l.m->druck().papier(0).lage();
        l.st().setTaste(taste, false);
        l.fahre(3'000'000);
        const int p2 = l.m->druck().papier(0).lage();
        if (taste == 3) EXPECT_LT(p1, p0 - 20) << "Papier rueckwaerts";
        else            EXPECT_GT(p1, p0 + 20) << "Papier vorwaerts";
        EXPECT_LE(std::abs(p2 - p1), 12) << "nach dem Loslassen laeuft es nicht weiter";
    }
}

TEST(K6311Modul, PapierendeSensorD104A6SchaltetDieFirmwareBeimNaechstenVorschubOfflineUndEsGehtMitPapierWeiter)
{
    // D104 A6 = 1 (Papierende [?]) tastet die Firmware je vier Papierschritte ab (1178H): sie geht offline (D105 A2 aus).  Mit Papier und ON LINE druckt sie weiter.
    const auto r = k6311test::rom(K6311_ABZUG_DIR);
    Lauf l;
    l.fahre(20'000'000);
    l.st().setPapierDa(false);
    l.sende(zeilen({"AB", "CD"}));
    l.fahre(30'000'000);
    EXPECT_FALSE(l.st().lampe(1)) << "offline nach Papierende";
    l.st().setPapierDa(true);
    l.st().setTaste(1, true); l.fahre(3'000'000); l.st().setTaste(1, false); l.fahre(20'000'000);
    EXPECT_TRUE(l.st().lampe(1)) << "wieder ON LINE";
    const uint64_t vorher = l.m->druck().kopf().anschlaege();
    l.sende(zeilen({"EF"}));
    l.fahre(30'000'000);
    EXPECT_GT(l.m->druck().kopf().anschlaege(), vorher);
    EXPECT_EQ(k6311test::vergleiche(l.m->druck().blatt(0), r, "EF", k6311test::baender(l.m->druck().blatt(0)).back().first, k6311test::baender(l.m->druck().blatt(0)).back().second).fehler, "");
}

TEST(K6311Modul, FormfeedSchiebtDasPapierUmDieFormatlaengeVonZwoelfZoll)
{
    // 0CH: 576 Papierschritte zu 1/48 Zoll = 12 Zoll (Vorgabe der Firmware) -> 1728 Rasterzeilen (1/144 Zoll)
    const auto r = k6311test::rom(K6311_ABZUG_DIR);
    Lauf l;
    l.fahre(20'000'000);
    l.sende("A\fB\r\n");
    l.fahre(60'000'000);
    const auto b = k6311test::baender(l.m->druck().blatt(0));
    ASSERT_GE(b.size(), 2u);
    EXPECT_EQ(b[1].first - b[0].first, 1728);
    EXPECT_EQ(k6311test::vergleiche(l.m->druck().blatt(0), r, "A", b[0].first, b[0].second).fehler, "");
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

