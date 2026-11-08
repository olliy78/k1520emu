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

TEST_P(K6316Modul, LichtschrankentestMitS1ZeigtFsoUndFsuAufH2UndH5)
{
    // hardware.md §8 Nr. 1, Handbuch [M:6/7]: S1 beim Netz-Ein = Lichtschrankentest (Firmware 00E5H -> 0524H).  Die Tasten werden erst
    // NACH der Synchronisation (~8 s, ~20 Mio. Takte) gelesen, also lange halten.  0524H: H2 an, wenn FSO = 0 (kein Papier), H5 an, wenn
    // FSU = 0 — die Lampe zeigt die FREIE Lichtschranke ("ohne Papier", Handbuch).  S1 erneut beendet den Test.
    Lauf l(GetParam());
    l.st().setTaste(1, true);
    l.fahre(40'000'000);
    l.st().setTaste(1, false);
    struct F { bool o, u; };
    for (F f : {F{false, false}, F{true, false}, F{false, true}, F{true, true}, F{false, false}}) {
        l.st().setBeleg(f.o, f.u);
        l.fahre(3'000'000);
        EXPECT_EQ(l.st().lampe(2), !f.o) << "H2 = FSO frei, oben=" << f.o << " unten=" << f.u;
        EXPECT_EQ(l.st().lampe(5), !f.u) << "H5 = FSU frei, oben=" << f.o << " unten=" << f.u;
    }
}

TEST_P(K6316Modul, VorstecktestMitS3BewegtDenVorsteckmotor)
{
    // Handbuch [M:6/7]: S3 beim Netz-Ein = Test der Vorsteckeinrichtung.  Ohne S3 ruht der Vorsteckmotor nach der Grundstellung.
    Lauf ohne(GetParam());
    ASSERT_TRUE(ohne.erstesZeichen(80'000'000));
    const uint32_t ruhe = ohne.st().vorsteck().schritte();
    ohne.fahre(20'000'000);
    EXPECT_EQ(ohne.st().vorsteck().schritte(), ruhe) << "ohne Test bleibt der Vorsteckmotor stehen";

    Lauf l(GetParam());
    l.st().setTaste(3, true);   // 8CH Bit 3 (00DAH -> 0257H), wie S1 erst nach der Synchronisation gelesen
    l.fahre(40'000'000);
    l.st().setTaste(3, false);
    const uint32_t a = l.st().vorsteck().schritte();
    l.fahre(20'000'000);
    EXPECT_GT(l.st().vorsteck().schritte(), a) << "der Vorstecktest faehrt den Motor";
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

// ─── Vorsteckbeleg als eigenes Blatt (hardware.md §11) ───────────────────────
//
// Die Firmware ist massgeblich: die Erwartungen unten sind am Lauf des IFSS-ROMs gemessen (hardware.md §11.8) und fuer die
// Zeitpunkte nur als Schrittzahlen des Vorsteckmotors (`Papierweg::schritte()`) formuliert, nicht als Taktzahlen.

namespace {
/// Einzug-Messung: alles in Vorsteckschritten seit dem Senden von BEZ.
struct Einzug {
    int  fsu_schritt = -1;     ///< Schritt der ersten FSU-Flanke (steigend)
    int  fso_fall = -1;        ///< Schritt, in dem FSO faellt (Ende des Belegs hat FSO passiert)
    int  u_start = 0, u_max = 0, u_ende = 0;
    int  lage_start = 0, lage_max_u = 0;   ///< Motorlage beim Start und im Umkehrpunkt
    int  schritte = 0;
};

/// Die ersten BEZ nach dem Netz-Ein scheitert in Endlos (hardware.md §11.8 Nr. 1); fuer alle weiteren Messungen vorher ausraeumen.
void belegAufwaermen(Lauf& l)
{
    ASSERT_TRUE(l.st().belegEinlegen(210, 148));
    l.sende("\x1b[0t");
    l.fahre(20'000'000);
    l.st().belegEntnehmen();
    l.fahre(1'000'000);
}

/// Beleg einlegen, BEZ @p n senden und @p takte lang beobachten.
Einzug bez(Lauf& l, double mm, double breite_mm, int n, uint64_t takte = 30'000'000)
{
    Einzug e;
    K6316Steuerung& s = l.st();
    EXPECT_TRUE(s.belegEinlegen(mm, breite_mm));
    l.sende("\x1b[" + std::to_string(n) + "t");
    const uint32_t n0 = s.vorsteck().schritte();
    e.u_start = e.u_max = e.u_ende = s.beleg_modell().u();
    e.lage_start = s.vorsteck().lage();
    bool fsu = s.fsu(), fso = s.fso();
    const uint64_t z = l.m->totalCycles() + takte;
    while (l.m->totalCycles() < z) {
        l.m->run(500);
        const int sch = static_cast<int>(s.vorsteck().schritte() - n0);
        if (s.fsu() && !fsu && e.fsu_schritt < 0) e.fsu_schritt = sch;
        if (!s.fso() && fso && e.fso_fall < 0) e.fso_fall = sch;
        fsu = s.fsu();
        fso = s.fso();
        if (s.beleg_modell().u() > e.u_max) { e.u_max = s.beleg_modell().u(); e.lage_max_u = s.vorsteck().lage(); }
        e.schritte = sch;
    }
    e.u_ende = s.beleg_modell().u();
    return e;
}

std::string statusAntwort(Lauf& l)
{
    const size_t n = l.sio().size();
    l.sende("\x1b[5n");
    l.fahre(1'500'000);
    return l.sio().substr(n);
}
}  // namespace

TEST(K6316Beleg, ErstesBezNachNetzEinScheitertWeilDerFirmwareZaehlerFFIst)
{
    // hardware.md §11.8 Nr. 1: Netz-Ein laesst (IX+22) = 3824H+22 = FFH; BEZ erlaubt in Endlos nur so viele Schritte OHNE FSU, wie
    // dieser Zaehler bis zum Ueberlauf hergibt — also keinen.  Der Fehler wirft den Beleg wieder hinaus (Status 5 = Belegfehler),
    // und BEZ setzt den Zaehler auf 0: erst das zweite BEZ arbeitet.
    Lauf l(Typ::K6316_Ifss);
    ASSERT_TRUE(l.erstesZeichen(80'000'000));
    l.fahre(2'000'000);
    EXPECT_EQ(l.m->memReadDebug(0x383A), 0xFF);
    ASSERT_TRUE(l.st().belegEinlegen(210, 148));
    l.sende("\x1b[0t");
    l.fahre(20'000'000);
    EXPECT_EQ(l.m->memReadDebug(0x383A), 0x00);
    EXPECT_FALSE(l.st().fso());
    EXPECT_FALSE(l.st().fsu());
    EXPECT_TRUE(l.st().beleg_modell().eingelegt()) << "der Beleg steckt noch im Schacht, nur nicht mehr unter den Lichtschranken";
    EXPECT_EQ(statusAntwort(l), "\x13\x1b[5n\x14") << "DC3, ESC [ 5 n (Belegfehler), DC4";
}

TEST(K6316Beleg, EinzugFolgtDerMessungAnDerFirmwareUndDerBelegHatEinEigenesBlatt)
{
    // hardware.md §11.7 Nr. 1 und 3: BEZ0 mit einem A5-Beleg.  Einzug = die Lage des Vorsteckmotors FAELLT (Auswurf = steigend).
    Lauf l(Typ::K6316_Ifss);
    ASSERT_TRUE(l.erstesZeichen(80'000'000));
    l.fahre(2'000'000);
    belegAufwaermen(l);
    const auto& cfg = l.st().config().beleg;
    const Einzug e = bez(l, 210, 148, 0);
    const int L = l.st().beleg_modell().laenge();
    EXPECT_EQ(L, 893) << "210 mm zu 108 Schritten je Zoll";
    ASSERT_GE(e.fsu_schritt, 1);
    EXPECT_LE(e.fsu_schritt, 255) << "die Firmware gibt ohne FSU hoechstens 255 Schritte";
    EXPECT_EQ(e.fsu_schritt, cfg.abstand_fso_fsu - cfg.einlege_lage) << "FSU steigt bei u = D";
    EXPECT_LT(e.lage_max_u, e.lage_start) << "Einzug = Lage faellt";
    // FSO faellt, wenn das Ende des Belegs (u - L) an FSO vorbei ist
    ASSERT_GT(e.fso_fall, 0);
    EXPECT_EQ(e.fso_fall, L + 1 - cfg.einlege_lage);
    EXPECT_GE(e.fso_fall - e.fsu_schritt, 279) << "mehr als 31 Gruppen zu 9 Schritten, sonst 'Beleg zu kurz'";
    // Ende: 12 Gruppen nach dem Verlassen von FSU, dann 22 Schritte Auslauf, dann 18 zurueck
    const int ende_fsu = L + cfg.abstand_fso_fsu + 1;
    EXPECT_NEAR(e.u_max - ende_fsu, 12 * 9 + 22, 9) << "Gruppenraster der Firmware";
    EXPECT_EQ(e.u_max - e.u_ende, 18) << "dann 18 Schritte (zwei Gruppen) in Auswurfrichtung zurueck";
    // Kopfzeilenabstand 8,46 mm = 36 Schritte: das Ende des Belegs steht 28...36 Schritte vor der Bezugslinie
    EXPECT_NEAR(cfg.druckzeile - (e.u_ende - L), 36, 9);
    EXPECT_TRUE(l.st().beleg_modell().unterDruckzeile());
    EXPECT_FALSE(l.st().fso());
    EXPECT_FALSE(l.st().fsu());
    // Das Blatt des Belegs: 148 mm = 1748 Pixel breit, 2 Zeilen je Schritt hoch, eigenstaendig
    ASSERT_TRUE(l.st().beleg_modell().hatBlatt());
    EXPECT_EQ(l.st().belegBlatt().breite(), 1748);
    EXPECT_EQ(l.st().belegBlatt().hoehe(), 2 * L);
    EXPECT_EQ(l.st().belegBlatt().punktzahl(), 0u);
}

TEST(K6316Beleg, EinzugMitParameterLaeuftNeunSchrittJeEinheitAbFsu)
{
    // hardware.md §11.2 (d): BEZ n zaehlt (n + 16) Gruppen ab der ersten FSU-Erkennung, dann 22 Schritte Auslauf, dann 18 zurueck —
    // die Spitze steht danach bei u = D + 148 + 9 n.  (Fussabstand 4,23 mm = 18 Schritte unter der Bezugslinie bei n = 1.)
    for (int n : {1, 5}) {
        Lauf l(Typ::K6316_Ifss);
        ASSERT_TRUE(l.erstesZeichen(80'000'000));
        l.fahre(2'000'000);
        belegAufwaermen(l);
        const auto& cfg = l.st().config().beleg;
        const Einzug e = bez(l, 210, 148, n);
        EXPECT_EQ(e.u_ende, cfg.abstand_fso_fsu + 148 + 9 * n) << "n=" << n;
        EXPECT_EQ(e.u_max - e.u_ende, 18) << n;
        EXPECT_EQ(e.u_ende - cfg.druckzeile, 18 + 9 * (n - 1)) << "Spitze n Zeilen unter der Bezugslinie, n=" << n;
        EXPECT_TRUE(l.st().fso() && l.st().fsu()) << "die Spitze steht weit hinter FSU, das Ende noch davor";
    }
}

TEST(K6316Beleg, AuswurfMitS4LaeuftBisFsuFreiPlusNachlauf)
{
    // hardware.md §11.2: S4 offline -> Auswurf; die Firmware faehrt, bis FSU frei ist, dann 36 Schritte (4 Gruppen) Nachlauf —
    // die Spitze ragt danach ueber FSO hinaus, der Beleg liegt zur Entnahme im Schacht (FSO/FSU frei).
    Lauf l(Typ::K6316_Ifss);
    ASSERT_TRUE(l.erstesZeichen(80'000'000));
    l.fahre(2'000'000);
    belegAufwaermen(l);
    const auto& cfg = l.st().config().beleg;
    (void)bez(l, 210, 148, 0);
    K6316Steuerung& s = l.st();
    s.setTaste(1, true); l.fahre(2'000'000); s.setTaste(1, false); l.fahre(3'000'000);   // S1: OFF-LINE
    ASSERT_FALSE(s.lampe(5));
    const uint32_t n0 = s.vorsteck().schritte();
    int fsu_frei = -1, ende = 0;
    bool fsu_war = false;
    s.setTaste(4, true);
    const uint64_t z = l.m->totalCycles() + 15'000'000;
    while (l.m->totalCycles() < z) {
        l.m->run(500);
        const int sch = static_cast<int>(s.vorsteck().schritte() - n0);
        if (s.fsu()) fsu_war = true;
        else if (fsu_war && fsu_frei < 0) fsu_frei = sch;
        ende = sch;
        if (sch > 100) s.setTaste(4, false);
    }
    ASSERT_GT(fsu_frei, 0) << "das Ende kam unter FSU durch";
    EXPECT_EQ(s.beleg_modell().u(), cfg.abstand_fso_fsu - 1 - (ende - fsu_frei)) << "Spitze: FSU frei bei u = D - 1, dann Nachlauf";
    EXPECT_NEAR(ende - fsu_frei, 36, 9) << "4 Gruppen Nachlauf";
    EXPECT_FALSE(s.fso());
    EXPECT_FALSE(s.fsu());
    EXPECT_TRUE(s.beleg_modell().eingelegt()) << "bleibt im Schacht, bis der Anwender ihn entnimmt";
    EXPECT_GT(s.beleg_modell().u(), s.beleg_modell().parameter().u_hinaus);
}

TEST(K6316Beleg, TextLandetAufDemBelegblattUndNichtAufDemEndlospapier)
{
    const auto r = rom(abzugDir(), false);
    Lauf l(Typ::K6316_Ifss);
    ASSERT_TRUE(l.erstesZeichen(80'000'000));
    l.fahre(2'000'000);
    belegAufwaermen(l);
    (void)bez(l, 210, 148, 0);
    K6316Steuerung& s = l.st();
    ASSERT_TRUE(s.beleg_modell().unterDruckzeile());
    const uint64_t haupt = l.blatt().punktzahl();
    const std::string t = "HELLO 123 ROBOTRON K6316";
    l.sende(t + "\r\n");
    l.fahre(8'000'000);
    EXPECT_EQ(l.blatt().punktzahl(), haupt) << "kein Punkt auf dem Endlospapier";
    const auto b = baender(s.belegBlatt());
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(vergleicheZeile(s.belegBlatt(), r, {{t, Schrift::Normal, false, 0}}, b[0].first, b[0].second), "");
    // Lage: Oberkante des Textes liegt Kopfzeilenabstand (36 Schritte, +-Gruppenraster) minus Zeichenhoehe unter dem Ende des Belegs
    EXPECT_GE(b[0].first, 2 * 28 - 21 - 4);
    EXPECT_LE(b[0].first, 2 * 36 - 21 + 4);
    // x: 30 Pixel vom Linksrand (der erste Textpunkt der Maschine liegt bei 425, die Belegkante bei 395)
    int xmin = 1 << 30;
    for (int y = b[0].first; y < b[0].second; ++y)
        for (int x = 0; x < s.belegBlatt().breite(); ++x) if (s.belegBlatt().punkt(x, y)) xmin = std::min(xmin, x);
    EXPECT_EQ(xmin, 425 - s.config().beleg.x_links);
}

TEST(K6316Beleg, TextHinterDemBelegrandTrifftDasEndlospapier)
{
    // Beleg 74 mm breit (874 Pixel, Kante bei 395 -> 1269): ein Zeichen in Spalte 45 (Pixel ~1775) liegt daneben.
    Lauf l(Typ::K6316_Ifss);
    ASSERT_TRUE(l.erstesZeichen(80'000'000));
    l.fahre(2'000'000);
    belegAufwaermen(l);
    (void)bez(l, 210, 74, 0);
    K6316Steuerung& s = l.st();
    ASSERT_TRUE(s.beleg_modell().unterDruckzeile());
    const uint64_t haupt = l.blatt().punktzahl(), beleg = s.belegBlatt().punktzahl();
    l.sende(std::string(45, ' ') + "X\r\n");
    l.fahre(8'000'000);
    EXPECT_GT(l.blatt().punktzahl(), haupt) << "das X steht rechts neben dem Beleg: Endlospapier";
    EXPECT_EQ(s.belegBlatt().punktzahl(), beleg);
}

TEST(K6316Beleg, ZuKurzerBelegGibtBelegfehlerUndWirdWiederHinausgefahren)
{
    // hardware.md §11.2 (c): Schwelle am Lauf L >= D + 270 Schritte (D = 36: 306 = Mindestlaenge 72 mm, Handbuch 2.5).  68 mm = 289 Schritte
    // fehlen; 72 mm geht.  Fehler = Status 5 (Belegfehler, `ESC [ 5 n`) und DC4; der Beleg kommt zurueck bis ueber FSO.
    {
        Lauf l(Typ::K6316_Ifss);
        ASSERT_TRUE(l.erstesZeichen(80'000'000));
        l.fahre(2'000'000);
        belegAufwaermen(l);
        (void)bez(l, 68, 148, 0, 20'000'000);
        EXPECT_FALSE(l.st().fso());
        EXPECT_FALSE(l.st().fsu());
        EXPECT_LT(l.st().beleg_modell().u(), 0) << "zurueck ueber FSO hinaus";
        EXPECT_EQ(statusAntwort(l), "\x13\x1b[5n\x14");
    }
    {
        Lauf l(Typ::K6316_Ifss);
        ASSERT_TRUE(l.erstesZeichen(80'000'000));
        l.fahre(2'000'000);
        belegAufwaermen(l);
        (void)bez(l, 72, 148, 0, 20'000'000);
        EXPECT_TRUE(l.st().beleg_modell().unterDruckzeile());
        EXPECT_EQ(statusAntwort(l), "\x13\x1b[0n\x11");
    }
}
