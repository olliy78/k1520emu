/**
 * @file test_k6316.cpp
 * @brief K6316 (Entwurf 49 WP2a/WP6a): Original-ROM (IFSS und V.24) bis in die Grundstellung, Selbsttest, Text ueber die SIO,
 *        Betriebsarten/Schnitt, Papierende, Diodenmatrix.  Das Druckbild wird gegen `tests/support/k6316_orakel.h` geprueft
 *        (Zeichengenerator im ROM), nicht gegen eine Momentaufnahme des Emulators.
 */
#include <gtest/gtest.h>
#include <string>

#include "core/machines/printer/printer_machine.h"
#include "tests/support/k6316_orakel.h"

using namespace k6316test;
using Typ = PrinterMachine::Config::Typ;

namespace {
inline bool l_v24(Typ t) { return t == Typ::K6316_V24; }

std::string abzugDir() { return K6316_ABZUG_DIR; }

struct Lauf {
    std::unique_ptr<PrinterMachine> m;
    bool v24;
    explicit Lauf(Typ t, PrinterMachine::Config c = {}) : v24(t == Typ::K6316_V24)
    {
        c.typ = t;
        // V.24: S17 (AM3 x TPVR) schaltet das XON/XOFF-Protokoll ein (V.24-ROM 197FH: `BIT 3,E` nach Zeile AM3); ohne ihn laeuft das
        // DTR-Protokoll und die Firmware sendet kein DC1 (hardware.md §5).
        if (v24 && !c.k6316.dil) c.k6316.dil = 1u << 6;
        m = std::make_unique<PrinterMachine>(c);
    }
    /// V.24: der Host legt /CTS und /DCD an (Wandler/Gegenstelle); erst dann gibt die Firmware das erste DC1 frei.
    void hostAn()
    {
        if (v24) m->serialHub()->wandler(1).anbinden();   // Gegenstelle: alle Eingaenge aktiv, Auto Enables geben das DC1 frei
    }
    K6316Steuerung& st() { return m->k6316(); }
    void fahre(uint64_t takte)
    {
        const uint64_t z = m->totalCycles() + takte;
        while (m->totalCycles() < z) m->run(2000);
    }
    /// Bis @p bed wahr wird, hoechstens @p max Takte; true = erreicht.
    template <class F> bool bis(F bed, uint64_t max)
    {
        const uint64_t z = m->totalCycles() + max;
        while (m->totalCycles() < z) { m->run(500); if (bed()) return true; }
        return false;
    }
    /// Bis die Firmware das erste Zeichen sendet (DC1/DC4); am V.24 legt der Host nach 35 Mio. Takten /CTS und /DCD an.
    bool erstesZeichen(uint64_t max)
    {
        if (v24) { fahre(35'000'000); hostAn(); }
        return bis([&] { return !sio().empty(); }, max);
    }
    const std::string& sio() { return st().sioAusgabe(v24 ? 0 : 1); }
    /// Text an die SIO: ein Zeichen je 3000 Takte (9600 Bd ~ 2560 Takte je Zeichen).
    void sende(const std::string& s)
    {
        for (char ch : s) {
            const uint8_t b = static_cast<uint8_t>(ch);
            if (v24) { while (m->serialHub()->wandler(1).fernFrei() == 0) fahre(3000); m->serialHub()->wandler(1).fernGib(&b, 1); }
            else m->printerSend(b);   // IFSS ohne Gegenstelle am Wandler
            fahre(3000);
            if (v24) { uint8_t rest[64]; while (m->serialHub()->wandler(1).fernNimm(rest, sizeof rest) > 0) {} }
        }
    }
    const Blattmodell& blatt() { return m->druck().blatt(0); }
};

std::string kennung(const std::vector<uint8_t>& r, int adr, int n)
{
    return std::string(r.begin() + adr, r.begin() + adr + n);
}
}  // namespace

// ─── Abzug und Kennung ───────────────────────────────────────────────────────

TEST(K6316Rom, KennungDerBeidenSaetzeStimmtMitDemAbzugUeberein)
{
    // Der Selbsttest druckt diese Kennung aus dem ROM (24F8H, 27F8H); sie ist der Waechter fuer die Chipreihenfolge I1…I5/V1…V5.
    const auto ifss = rom(abzugDir(), false), v24 = rom(abzugDir(), true);
    ASSERT_EQ(ifss.size(), 10240u);
    ASSERT_EQ(v24.size(), 10240u);
    EXPECT_EQ(kennung(ifss, 0x24F8, 3), "540");
    EXPECT_EQ(kennung(ifss, 0x27F8, 8), "19461306");
    EXPECT_EQ(kennung(v24, 0x24F8, 3), "501");
    EXPECT_EQ(kennung(v24, 0x27F8, 8), "15263105");
    for (Typ t : {Typ::K6316_Ifss, Typ::K6316_V24}) {
        PrinterMachine::Config c;
        c.typ = t;
        PrinterMachine m(c);
        const auto& r = t == Typ::K6316_V24 ? v24 : ifss;
        for (int a = 0; a < 0x2800; a += 0x1F) EXPECT_EQ(m.memReadDebug(static_cast<uint16_t>(a)), r[static_cast<size_t>(a)]) << a;
        EXPECT_EQ(m.memReadDebug(0x2800), 0xFF);   // nichts bis zum RAM
        m.memWriteDebug(0x3800, 0x5A);
        EXPECT_EQ(m.memReadDebug(0x3800), 0x5A);
        m.memWriteDebug(0x0100, 0x00);
        EXPECT_EQ(m.memReadDebug(0x0100), r[0x100]);   // ROM
    }
}

// ─── Grundstellung ───────────────────────────────────────────────────────────

class K6316Modul : public ::testing::TestWithParam<Typ> {};

TEST_P(K6316Modul, ErreichtDieGrundstellungUndMeldetDC1AufDemRichtigenKanal)
{
    Lauf l(GetParam());
    ASSERT_TRUE(l.erstesZeichen(80'000'000)) << "kein Zeichen von der Firmware";
    EXPECT_EQ(l.sio()[0], 0x11) << "DC1 nach dem Einschalten (Handbuch 10.2.4.4.1: Papier eingelegt, kein Fehler)";
    EXPECT_TRUE(l.st().sioAusgabe(l.v24 ? 1 : 0).empty()) << "der andere Kanal bleibt still";
    l.fahre(2'000'000);
    EXPECT_TRUE(l.st().lampe(5)) << "ON-LINE";
    EXPECT_TRUE(l.st().lampe(4)) << "BEREIT";
    EXPECT_FALSE(l.st().lampe(1));
    EXPECT_FALSE(l.st().lampe(2));
    EXPECT_FALSE(l.st().lampe(3));
    // Synchronisation: Wagen am linken Anschlag (0), dann 3,5 Schritte weg (Servicehandbuch 1.1.3.3)
    EXPECT_GE(l.m->druck().wagen().position(), 6);
    EXPECT_LE(l.m->druck().wagen().position(), 12);
    EXPECT_FALSE(static_cast<SchrittWagen&>(l.m->druck().wagen()).blockiert());
}

TEST_P(K6316Modul, GrundstellungAusMehrerenStartlagenDesWagens)
{
    // Lehre SD 1157 (Merkposten Nr. 6/7): die Synchronisation darf nicht an der Startlage haengen — auch nicht am Anschlag.
    for (int start : {0, 5, 400, 1000, 1700}) {
        PrinterMachine::Config c;
        c.k6316.wagen_start = start;
        Lauf l(GetParam(), c);
        ASSERT_TRUE(l.erstesZeichen(80'000'000)) << "Startlage " << start;
        EXPECT_EQ(l.sio()[0], 0x11) << start;
        EXPECT_GE(l.m->druck().wagen().position(), 6) << start;
        EXPECT_LE(l.m->druck().wagen().position(), 12) << start;
    }
}

TEST_P(K6316Modul, KopfAbgeschwenktBlinktH2UndSynchronisiertErstNachDemAnschwenken)
{
    PrinterMachine::Config c;
    c.k6316.kopf_angeschwenkt = false;   // Handbuch 5.2.1: Synchronisation nur bei angeschwenktem Kopf und geschlossener Haube
    Lauf l(GetParam(), c);
    l.fahre(10'000'000);
    EXPECT_TRUE(l.sio().empty());
    EXPECT_EQ(l.m->druck().wagen().position(), c.k6316.wagen_start) << "der Wagen bleibt stehen";
    bool h2 = false, aus = false;
    for (int i = 0; i < 400; ++i) { l.fahre(100'000); (l.st().lampe(2) ? h2 : aus) = true; }
    EXPECT_TRUE(h2 && aus) << "H2 blinkt (Bedienfehler, Handbuch 5.5.2)";
    l.st().setKopf(true);
    ASSERT_TRUE(l.erstesZeichen(90'000'000));
    EXPECT_EQ(l.sio()[0], 0x11);
    EXPECT_FALSE(l.st().lampe(2));
}

TEST_P(K6316Modul, PapierendeSchaltetH3UndDieMeldungAufDC4)
{
    PrinterMachine::Config c;
    c.k6316.papier_da = false;
    Lauf l(GetParam(), c);
    EXPECT_TRUE(l.st().lampe(3));
    ASSERT_TRUE(l.erstesZeichen(80'000'000));
    // IFSS: DC4 statt DC1 (Handbuch 10.2.4.4.1).  Das V.24-ROM sendet auch ohne Papier DC1 (am Lauf gemessen; die V.24-Beschreibung
    // des Handbuchs nennt es nicht, ob das Gastverhalten oder eine Eigenheit des XON/XOFF-Protokolls ist [?]).
    EXPECT_EQ(l.sio()[0], l.v24 ? 0x11 : 0x14);
}

TEST(K6316V24, OhneS17LaeuftDasDtrProtokollUndEsGehtKeinDC1Raus)
{
    PrinterMachine::Config c;
    c.typ = Typ::K6316_V24;
    c.k6316.dil = 0;   // Auslieferungszustand: alle Schalter Aus
    PrinterMachine m(c);
    m.serialHub()->wandler(1).anbinden();
    const uint64_t z = 60'000'000;
    while (m.totalCycles() < z) m.run(5000);
    EXPECT_TRUE(m.k6316().sioAusgabe(0).empty());
    EXPECT_TRUE(m.k6316().lampe(5)) << "trotzdem ON-LINE";
}

TEST(K6316V24, BaudrateKommtVonDenDilSchaltern24Bis26)
{
    // S26 = Bit 0, S25 = Bit 1, S24 = Bit 2 des Baudcodes: Zeitkonstante 1 << Code von CTC D10 Kanal 2 (V.24-ROM 0093H–00BAH,
    // Code 7 = 200 Bd).  Gemessen an der Baudperiode der CTC.
    struct F { uint16_t dil; uint64_t takte; };
    for (F f : {F{1u << 6, 16u * 1}, F{(1u << 6) | (1u << 15), 16u * 2}, F{(1u << 6) | (1u << 14) | (1u << 15), 16u * 8}}) {
        PrinterMachine::Config c;
        c.typ = Typ::K6316_V24;
        c.k6316.dil = f.dil;
        PrinterMachine m(c);
        while (m.totalCycles() < 3'000'000) m.run(5000);
        EXPECT_EQ(m.k6316().ctc10().teilerTakte(2), f.takte) << f.dil;
    }
}

// ─── Selbsttest ──────────────────────────────────────────────────────────────

TEST_P(K6316Modul, SelbsttestDruckStimmtMitDemZeichengeneratorImRomUeberein)
{
    const auto r = rom(abzugDir(), l_v24(GetParam()));
    Lauf l(GetParam());
    l.st().setTaste(5, true);   // Handbuch 5.6.1: S5 beim Netzeinschalten bis der Ausdruck beginnt
    ASSERT_TRUE(l.bis([&] { return l.m->druck().kopf().anschlaege() > 0; }, 80'000'000));
    l.st().setTaste(5, false);
    l.fahre(14'000'000);
    const auto b = baender(l.blatt());
    ASSERT_GE(b.size(), 4u);
    // Zeile 0: Kennung aus dem ROM (24F8H/27F8H), danach das Zeichenmuster: je Zeile 26 Zeichen im Normaldruck ab 0x21 + Zeile
    const std::string id = kennung(r, 0x24F8, 3) + kennung(r, 0x27F8, 8);
    int x0 = 0, x1 = 0, x2 = 0, x3 = 0;
    EXPECT_EQ(vergleiche(l.blatt(), r, id, b[0].first, b[0].second, &x0), "") << "Kennungszeile";
    auto zeile = [](int i) { std::string t; for (int n = 0; n < 26; ++n) t += static_cast<char>(0x21 + ((i + n) % 95)); return t; };
    EXPECT_EQ(vergleiche(l.blatt(), r, zeile(0), b[1].first, b[1].second, &x1), "") << "Zeile 1 (vorwaerts)";
    EXPECT_EQ(vergleiche(l.blatt(), r, zeile(1), b[2].first, b[2].second, &x2), "") << "Zeile 2 (rueckwaerts)";
    EXPECT_EQ(vergleiche(l.blatt(), r, zeile(2), b[3].first, b[3].second, &x3), "") << "Zeile 3 (vorwaerts)";
    // Vor- und Rueckwaertszeilen decken sich in x (die Firmware kompensiert die Nadelspalten, der Kopf die Richtung)
    EXPECT_EQ(x0, x1);
    EXPECT_EQ(x1, x2);
    EXPECT_EQ(x2, x3);
    // Zeilenabstand 1/6 Zoll = 36 Einheiten
    EXPECT_EQ(b[2].first - b[1].first, 36);
    EXPECT_EQ(b[3].first - b[2].first, 36);
    EXPECT_FALSE(l.st().lampe(2)) << "kein Havariezustand";
    EXPECT_TRUE(l.st().lampe(1)) << "H1 TEST";
}

TEST_P(K6316Modul, SelbsttestZeilenMitBreitUnterstrichSchraegUndNadel9StimmenMitDemRomUeberein)
{
    // Merkposten Nr. 22 / hardware.md §10.7: je Zeile 64 Zeichen aus dem Umlauf 21H…7EH, 20H — 26 Normal, Leerzeichen, 13 Breit mit
    // Unterstrich (ab Takt 270), Leerzeichen, 25 Schraeg (ab Takt 540).  Geprueft wird die GANZE Zeile mit allen neun Nadeln.
    const auto r = rom(abzugDir(), l_v24(GetParam()));
    Lauf l(GetParam());
    l.st().setTaste(5, true);
    ASSERT_TRUE(l.bis([&] { return l.m->druck().kopf().anschlaege() > 0; }, 80'000'000));
    l.st().setTaste(5, false);
    l.fahre(14'000'000);
    const auto b = baender(l.blatt());
    ASSERT_GE(b.size(), 4u);
    auto umlauf = [](int k) { const int c = 0x21 + k % 95; return static_cast<char>(c == 0x7F ? 0x20 : c); };
    for (int i = 0; i < 3; ++i) {
        std::string n, w, s;
        for (int k = 0; k < 26; ++k) n += umlauf(i + k);
        for (int k = 26; k < 39; ++k) w += umlauf(i + k);
        for (int k = 39; k < 64; ++k) s += umlauf(i + k);
        const std::vector<Abschnitt> teile = {{n, Schrift::Normal, false, 0}, {w, Schrift::Breit, true, 270}, {s, Schrift::Schraeg, false, 540}};
        EXPECT_EQ(vergleicheZeile(l.blatt(), r, teile, b[static_cast<size_t>(i + 1)].first, b[static_cast<size_t>(i + 1)].second), "")
            << "Zeile " << i + 1 << (i == 1 ? " (rueckwaerts)" : " (vorwaerts)");
    }
}

TEST_P(K6316Modul, TextUeberDieSioWirdGedrucktUndBeideRichtungenDecken)
{
    const auto r = rom(abzugDir(), l_v24(GetParam()));
    Lauf l(GetParam());
    ASSERT_TRUE(l.erstesZeichen(80'000'000));
    l.fahre(2'000'000);
    const std::string t1 = "HELLO 123 ROBOTRON K6316", t2 = "ZEILE ZWEI 0987654321";
    l.sende(t1 + "\r\n" + t2 + "\r\n");
    l.fahre(8'000'000);
    const auto b = baender(l.blatt());
    ASSERT_EQ(b.size(), 2u);
    int xa = 0, xb = 0;
    EXPECT_EQ(vergleiche(l.blatt(), r, t1, b[0].first, b[0].second, &xa), "");
    EXPECT_EQ(vergleiche(l.blatt(), r, t2, b[1].first, b[1].second, &xb), "");
    EXPECT_EQ(xa, xb) << "Vor-/Rueckwaertsdruck decken sich";
}

TEST_P(K6316Modul, SchriftartenUeberEscFolgenUndUnterlaengenMitNadel9)
{
    // hardware.md §10.2–10.5: SGR 1 = Breit, 3 = Schraeg, 4 = Unterstrich, 0 = zurueck; Unterlaengen ueber Nadel 9 (Spalte 0 der Glyphe).
    const auto r = rom(abzugDir(), l_v24(GetParam()));
    Lauf l(GetParam());
    ASSERT_TRUE(l.erstesZeichen(80'000'000));
    l.fahre(2'000'000);
    const std::string esc = "\x1b[";
    const std::string z1 = "gjpqy_,;@ Agj", z2 = "BREIT gj", z3 = "SCHRAEG gj", z4 = "UNTER STRICH";
    l.sende(z1 + "\r\n" + esc + "1m" + z2 + esc + "0m\r\n" + esc + "3m" + z3 + esc + "0m\r\n" + esc + "4m" + z4 + esc + "0m\r\n");
    l.fahre(12'000'000);
    const auto b = baender(l.blatt());
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(vergleicheZeile(l.blatt(), r, {{z1, Schrift::Normal, false, 0}}, b[0].first, b[0].second), "") << "Normal + Nadel 9";
    EXPECT_EQ(vergleicheZeile(l.blatt(), r, {{z2, Schrift::Breit, false, 0}}, b[1].first, b[1].second), "") << "ESC[1m Breit";
    EXPECT_EQ(vergleicheZeile(l.blatt(), r, {{z3, Schrift::Schraeg, false, 0}}, b[2].first, b[2].second), "") << "ESC[3m Schraeg";
    EXPECT_EQ(vergleicheZeile(l.blatt(), r, {{z4, Schrift::Normal, true, 0}}, b[3].first, b[3].second), "") << "ESC[4m Unterstrich";
}

TEST_P(K6316Modul, HexdumpMitS2BeimEinschaltenDrucktJedesByteAlsDreiZeichen)
{
    // hardware.md §10.8: S2 beim Netz-Ein (8CH Bit 2) -> jedes empfangene Byte als '0'+hi, lo als 0-9/A-F, Leerzeichen; 32 Bytes = 96 Zeichen je Zeile, gedruckt mit 12,5 Z/Z (Takt 1/125 Zoll = 2,4 Pixel; am Lauf gemessen).
    const auto r = rom(abzugDir(), l_v24(GetParam()));
    Lauf l(GetParam());
    l.st().setTaste(2, true);
    ASSERT_TRUE(l.erstesZeichen(80'000'000));
    l.st().setTaste(2, false);
    l.fahre(2'000'000);
    std::string daten, text;
    for (int i = 0; i < 32; ++i) {
        // Die Firmware programmiert die SIO auf 7 Datenbits (WR3 = 61H, hardware.md §2): Bit 7 kommt nie an, ein Hi-Nibble >= 8 also
        // auch nicht — der Pfad "hi >= A druckt : ; < = > ?" (§10.8) ist ueber die Schnittstelle nicht erreichbar.
        const int bb = (i * 0x1D + 0x07) & 0x7F;   // alle Lo-Nibble, Hi 0..7
        daten += static_cast<char>(bb);
        text += static_cast<char>(0x30 | (bb >> 4));
        text += "0123456789ABCDEF"[bb & 15];
        text += ' ';
    }
    l.sende(daten);
    l.fahre(12'000'000);
    const auto b = baender(l.blatt());
    ASSERT_EQ(b.size(), 1u) << "eine Zeile mit 32 Bytes";
    constexpr double TAKT_125 = 2.4;
    EXPECT_EQ(vergleicheZeile(l.blatt(), r, {{text, Schrift::Normal, false, 0}}, b[0].first, b[0].second, INT32_MIN, 0, TAKT_125), "")
        << "gelesen: \"" << lesen(l.blatt(), r, b[0].first, b[0].second, TAKT_125) << "\"\nerwartet: \"" << text << "\"";
}

INSTANTIATE_TEST_SUITE_P(Module, K6316Modul, ::testing::Values(Typ::K6316_Ifss, Typ::K6316_V24));

// ─── Betriebsarten und Schnitt ───────────────────────────────────────────────

TEST(K6316Betriebsart, SchneidenSchneidetNurInDerStellungSchneiden)
{
    struct F { K6316Steuerung::Betriebsart b; int erwartet; };
    for (F f : {F{K6316Steuerung::Betriebsart::Schneiden, 1}, F{K6316Steuerung::Betriebsart::Endlos, 0},
                F{K6316Steuerung::Betriebsart::Aufwickeln, 0}}) {
        PrinterMachine::Config c;
        c.k6316.betriebsart = f.b;
        Lauf l(Typ::K6316_Ifss, c);
        ASSERT_TRUE(l.erstesZeichen(80'000'000));
        l.fahre(3'000'000);
        l.st().setTaste(1, true); l.fahre(2'000'000); l.st().setTaste(1, false);   // S1: OFF-LINE (nur offline wirken S4/S5)
        l.fahre(3'000'000);
        ASSERT_FALSE(l.st().lampe(5)) << "offline";
        l.st().setTaste(5, true); l.fahre(500'000);
        l.st().setTaste(4, true);
        l.fahre(70'000'000);
        l.st().setTaste(4, false); l.st().setTaste(5, false);
        EXPECT_EQ(l.st().schnitte(), f.erwartet) << static_cast<int>(f.b);
        EXPECT_EQ(static_cast<int>(l.blatt().schnitte().size()), f.erwartet);
        EXPECT_FALSE(l.st().lampe(2));
    }
}

TEST(K6316Betriebsart, DrehschalterWirdBeimNetzEinGelesen)
{
    // /MELO, /MAUF: Endlos 37, Aufwickeln 38, Schneiden 39 (Firmware 04F7H; Status ESC [ <n> c).
    struct F { K6316Steuerung::Betriebsart b; uint8_t code; };
    for (F f : {F{K6316Steuerung::Betriebsart::Endlos, 0x37}, F{K6316Steuerung::Betriebsart::Aufwickeln, 0x38},
                F{K6316Steuerung::Betriebsart::Schneiden, 0x39}}) {
        PrinterMachine::Config c;
        c.k6316.betriebsart = f.b;
        Lauf l(Typ::K6316_Ifss, c);
        l.fahre(500'000);
        EXPECT_EQ(l.m->memReadDebug(0x3824 + 102), f.code);
    }
}

// ─── Diodenmatrix und Tasten ─────────────────────────────────────────────────

TEST(K6316Matrix, DilSchalterWirkenNurAufGetriebenerZeileTastenImmer)
{
    PrinterMachine::Config c;
    c.typ = Typ::K6316_Ifss;
    PrinterMachine m(c);
    K6316Steuerung& k = m.k6316();
    // PIO D11: A8H/A9H, Modus 3 wie die Firmware es einrichtet: A: Bit 0..2 Eingang, B: Bit 2, 6, 7 Eingang
    k.ioWrite(0xAA, 0xFF); k.ioWrite(0xAA, 0x07);
    k.ioWrite(0xAB, 0xFF); k.ioWrite(0xAB, 0xC4);
    auto zeile = [&](uint8_t am) { k.ioWrite(0xA9, am); return k.ioRead(0xA8) & 7; };
    const uint8_t AM1 = 0x10, AM3 = 0x20, AM4 = 0x02;
    k.setDil(14, true);                         // S14: AM1 x DIL (Spalte Bit 2)
    EXPECT_EQ(zeile(AM1), 0x04);
    EXPECT_EQ(zeile(AM3), 0x00) << "AM1 nicht getrieben: S14 unsichtbar";
    k.setDil(11, true);                         // S11: AM4 x TBAW (Bit 1)
    EXPECT_EQ(zeile(AM4), 0x02);
    EXPECT_EQ(zeile(AM4 | AM1), 0x06);
    k.setDil(20, true);                         // S20: AM3 x DIL
    EXPECT_EQ(zeile(AM3), 0x04);
    k.setTaste(5, true);                        // S5 Schneiden: Spalte TSCH (Bit 0), unabhaengig von der Zeile
    EXPECT_EQ(zeile(0), 0x01);
    k.setTaste(4, true);                        // S4 Belegauswurf: TBAW (Bit 1)
    EXPECT_EQ(zeile(0), 0x03);
    k.setDil(11, false); k.setDil(14, false); k.setDil(20, false);
    EXPECT_EQ(zeile(AM1 | AM3 | AM4), 0x03) << "nur die Tasten";
    EXPECT_EQ(k.dilMaske(), 0);
}
