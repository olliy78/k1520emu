/**
 * @file test_sd1157_rom.cpp
 * @brief Das Original-ROM des SD 1157 laeuft an (Entwurf 49, WP1): Stapel bei 2C28H, CTC programmiert,
 *        Vektortabelle 2CE0H gefuellt, 13 CTC-1-Interrupts (~345 ms), Wagenfahrt bis /LR, Anfangs-
 *        Papiervorschub, Online-Lampe, DC1 an den IFSS, danach die Leerlaufschleife.  Dazu die
 *        Maschine selbst (Kennung 7, K6316 wird abgewiesen).  Quelle fuer alle Zahlen:
 *        doc/printer/sd1157/hardware.md §7, §8, §11; Adressen aus dem ROM (Q = 1157.mac, F = ROM).
 *        Wagen und Papier sind seit WP3 das Druckwerk (core/peripherals/druckwerk/): geprueft wird hier
 *        das Anlaufverhalten der Firmware gegen den geregelten Wagen und den Papierweg, auch aus allen
 *        Startlagen; das Druckbild steht in test_sd1157_druck.cpp.
 */

#include <gtest/gtest.h>
#include <bitset>
#include <memory>
#include <stdexcept>

#include "core/machines/printer/printer_machine.h"

namespace {

struct Lauf {
    std::unique_ptr<PrinterMachine> m;
    std::bitset<0x10000> besucht;
    bool sp_2c28 = false;
    explicit Lauf(PrinterMachine::Config cfg = {}) : m(std::make_unique<PrinterMachine>(cfg)) {
        m->setCpuTraceCallback([this](const Z80& c) {
            besucht.set(c.PC);
            if (c.SP == 0x2C28) sp_2c28 = true;
        });
    }
    void fahre(uint64_t takte) {
        while (m->totalCycles() < takte) m->run(20000);
    }
    uint16_t wort(uint16_t a) { return static_cast<uint16_t>(m->memReadDebug(a) | (m->memReadDebug(a + 1) << 8)); }
};

TEST(K1520PrinterMaschine, KennungUndK6316TypenWerdenAngelegt)
{
    PrinterMachine m;
    EXPECT_EQ(m.machineType(), 7);
    EXPECT_EQ(m.cpuPC(), 0x0000);
    EXPECT_EQ(m.memReadDebug(0x0000), 0xED);   // IM 2
    EXPECT_FALSE(m.zusatzkartenSteckbar());
    EXPECT_FALSE(m.installRaf(RAF::Typ::RAF512));
    EXPECT_FALSE(m.istK6316());

    for (auto t : {PrinterMachine::Config::Typ::K6316_Ifss, PrinterMachine::Config::Typ::K6316_V24}) {
        PrinterMachine::Config c;
        c.typ = t;
        PrinterMachine x(c);
        EXPECT_TRUE(x.istK6316());
        EXPECT_EQ(x.machineType(), 7);
        EXPECT_EQ(x.memReadDebug(0x0000), 0xDD);   // LD IX,3824H
    }
}

TEST(Sd1157Rom, StapelCtcUndVektortabelleNachDemAnlauf)
{
    Lauf l;
    l.fahre(10'000);   // sy00 .. EI bei 022AH liegt bei ~6 600 Takten (trace: c6600)
    auto& b = l.besucht;
    EXPECT_TRUE(b.test(0x0000)) << "sy00";
    EXPECT_TRUE(b.test(0x000B)) << "sy01: LD SP,2C28H";
    EXPECT_TRUE(l.sp_2c28) << "Stapel bei 2C28H eingerichtet";

    // CTC: Vektorbasis E0H, Kanal 0 mit TC 1 (9600 Bd), Kanal 1 als Zeitgeber mit Interrupt (B7H, FFH)
    const auto c = l.m->sd1157().ctc().debugState();
    EXPECT_EQ(c.vecBase, 0xE0);
    EXPECT_TRUE(c.ch[0].running);
    EXPECT_EQ(c.ch[0].timeConst, 0x01);
    EXPECT_TRUE(c.ch[1].running);
    EXPECT_TRUE(c.ch[1].intEn);
    EXPECT_EQ(c.ch[1].timeConst, 0xFF);

    // PIO-Vektoren (hardware.md §5.1): PIO 2 A E8H, B EAH; PIO 1 A ECH, B EEH
    auto& st = l.m->sd1157();
    EXPECT_EQ(st.pio2().debugState().port[0].vector, 0xE8);
    EXPECT_EQ(st.pio2().debugState().port[1].vector, 0xEA);
    EXPECT_EQ(st.pio1().debugState().port[0].vector, 0xEC);
    EXPECT_EQ(st.pio1().debugState().port[1].vector, 0xEE);
    // PIO 3 A wurde fuer das serielle Interface auf Modus 3 umprogrammiert (§5.1)
    EXPECT_EQ(st.pio3().debugState().port[0].mode, 3);

    // Vektortabelle bei 2CE0H (I = 2CH), hardware.md §7/§8 mit den ROM-Adressen
    EXPECT_EQ(l.m->sd1157().cpu().I, 0x2C);
    EXPECT_EQ(l.wort(0x2CE2), 0x0A8F) << "CTC 1 ictc1";
    EXPECT_EQ(l.wort(0x2CE4), 0x269C) << "CTC 2 ictc2a";
    EXPECT_EQ(l.wort(0x2CE6), 0x26DC) << "CTC 3 ictc3";
    EXPECT_EQ(l.wort(0x2CE8), 0x1A54) << "PIO 2 A Takt vorwaerts";
    EXPECT_EQ(l.wort(0x2CEA), 0x1A83) << "PIO 2 B Takt rueckwaerts";
    EXPECT_EQ(l.wort(0x2CEC), 0x0993) << "PIO 1 A linker Rand / PV 1";
    EXPECT_EQ(l.wort(0x2CEE), 0x09D3) << "PIO 1 B rechter Rand / PV 2";
    EXPECT_EQ(l.wort(0x2CF0), 0x172A) << "SIO B Tx leer (isiote)";
    EXPECT_EQ(l.wort(0x2CF4), 0x0E25) << "SIO B Rx (serinp)";
    EXPECT_EQ(l.wort(0x2CF6), 0x0F10) << "SIO B Sonderfall (errint)";
    // Kanal A (V.24) bekommt seine Eintraege nur im V.24-Betrieb: am IFSS bleiben sie 0 (§7)
    EXPECT_EQ(l.wort(0x2CF8), 0x0000);
    EXPECT_EQ(l.wort(0x2CFA), 0x0000);
}

TEST(Sd1157Rom, VektortabelleImV24Betrieb)
{
    PrinterMachine::Config c;
    c.sd1157.schnittstelle = Sd1157Steuerung::Config::Schnittstelle::V24;
    Lauf l(c);
    l.fahre(10'000);
    EXPECT_EQ(l.wort(0x2CF8), 0x172A) << "SIO A Tx leer";
    EXPECT_EQ(l.wort(0x2CFA), 0x0F44) << "SIO A Ext/Status (serest)";
    EXPECT_EQ(l.wort(0x2CFC), 0x0E25) << "SIO A Rx";
    EXPECT_EQ(l.wort(0x2CFE), 0x0F10) << "SIO A Sonderfall";
    // WR2 immer in Kanal B (§7); der V.24-Kanal bekommt WR3 61H/E1H mit Auto-Enables
    EXPECT_EQ(l.m->sd1157().sio().debugState().ch[1].wr2, 0xF0);
    l.fahre(4'000'000);
    auto& st = l.m->sd1157();
    // Auch im V.24-Betrieb (CTS/DCD fest aktiv) kommt die Firmware bis zur Leerlaufschleife; DC1 geht an Kanal A
    EXPECT_TRUE(st.lampeOnline());
    EXPECT_FALSE(st.lampeFehler());
    // Ohne Gegenstelle ("Kabel ab") ist /CTS inaktiv: das DC1 bleibt wegen WR3-Auto-Enables im Sender stehen (WP4,
    // wie an der K8025) und geht erst hinaus, wenn ein Transport anbindet — ein spaeter verbundener Host sieht es also.
    EXPECT_EQ(st.sioAusgabe(0), "");
    EXPECT_EQ(st.sioAusgabe(1), "");
    EXPECT_TRUE(st.sio().channelA().tx_buf.has_value());
    l.m->serialHub()->wandler(1).anbinden();
    l.fahre(l.m->totalCycles() + 100'000);
    EXPECT_EQ(st.sioAusgabe(0), std::string("\x11"));
    EXPECT_EQ(st.sioAusgabe(1), "");
}

TEST(Sd1157Rom, GrundstellungBisZurLeerlaufschleife)
{
    Lauf l;
    l.fahre(4'000'000);   // trace: Wagen steht links bei ~1,9 Mio., Anfangs-PV bis ~2,6 Mio. Takten
    auto& st = l.m->sd1157();

    // 13 CTC-1-Interrupts a 26,6 ms ~ 345 ms, danach faehrt der Wagen (hardware.md §11 Nr. 5-7)
    EXPECT_TRUE(l.besucht.test(0x0A8F)) << "ictc1";
    EXPECT_TRUE(l.besucht.test(0x1A83)) << "Takt-Int rueckwaerts: der Wagen fuhr nach links";
    EXPECT_TRUE(l.besucht.test(0x0993)) << "Randsensor links / PV-Schritt";

    // Wagen am linken Rand (/LR = 0), steht (LAUF = 0)
    EXPECT_LE(st.druck().wagen().position(), 76);
    EXPECT_FALSE(st.druck().wagen().laeuft());
    // Anfangs-PV, EXAKT: 24 vor, Rueckzug 24 + 4 (Spielausgleich, 24B1H-Sprung `add a,4`) zurueck, dann 4 vor
    // = 56 Schritte, Nettolage 0.  Einbahndrucker: es gibt nur Bahn 1.
    EXPECT_EQ(st.druck().bahnen(), 1);
    EXPECT_EQ(st.druck().papier(0).schritteVor(), 28u);
    EXPECT_EQ(st.druck().papier(0).schritteRueck(), 28u);
    EXPECT_EQ(st.druck().papier(0).lage(), 0);

    // Online-Lampe an, keine Fehlerlampe, kein Fehler-Havarie-Pfad (NMI 0066H)
    EXPECT_TRUE(st.lampeOnline());
    EXPECT_FALSE(st.lampeFehler());
    EXPECT_FALSE(l.besucht.test(0x0433)) << "NMI-Routine";
    // Nach dem Einschalten geht DC1 an den Host: am IFSS (Kanal B), nichts am V.24 (hardware.md §11 Nr. 8)
    EXPECT_EQ(st.sioAusgabe(1), std::string("\x11"));
    EXPECT_EQ(st.sioAusgabe(0), "");
    EXPECT_EQ(st.cpu().SP, 0x2C28);
    EXPECT_FALSE(st.cpu().halted);
    // keine Nadel feuert
    EXPECT_EQ(st.nadeln(), 0x000);
    // Code nur aus dem ROM
    for (uint32_t a = 0x2980; a < 0x10000; ++a) ASSERT_FALSE(l.besucht.test(a)) << std::hex << a;
}

TEST(Sd1157Rom, DieLeerlaufschleifeBleibtStehenOhneAbsturz)
{
    Lauf l;
    l.fahre(4'000'000);
    const int pv = static_cast<int>(l.m->sd1157().druck().papier(0).schritte());
    const int pos = l.m->sd1157().druck().wagen().position();
    l.fahre(8'000'000);
    auto& st = l.m->sd1157();
    EXPECT_EQ(static_cast<int>(st.druck().papier(0).schritte()), pv) << "kein Papiervorschub im Leerlauf";
    EXPECT_EQ(st.druck().wagen().position(), pos) << "der Wagen bleibt links stehen";
    EXPECT_TRUE(st.lampeOnline());
    EXPECT_FALSE(st.lampeFehler());
    EXPECT_FALSE(l.besucht.test(0x0433)) << "NMI-Routine";
    EXPECT_EQ(st.cpu().SP, 0x2C28);
    // Der CTC-1-Zeitgeber laeuft weiter (Kopfueberwachung/Leerlaufzeit, 26,6 ms)
    EXPECT_TRUE(st.ctc().debugState().ch[1].running);
}

TEST(Sd1157Rom, GrundstellungLaeuftAusAnderenStartlagenDurch)
{
    // hardware.md §14 Nr. 5 (Pruefkriterium fuer WP2b): aus beliebigem Startpunkt, auch aus dem Randbereich
    // (/LR und /VK schon 0, z. B. Wagen beim Ausschalten links) und von den Anschlaegen.  Die Randlagen sind der
    // Fall, der mit /VK <= 10 haengenblieb: die LFG-Spitze des Einschaltens bewegt den Wagen um den Bremsweg
    // (70 Takte bei VTAB = 1), der danach gelesene /VK muss dann noch im /LR-Bereich liegen
    // (GeregelterWagen::Parameter::vk_bis >= lr_bis, merkposten/drucker.md).
    for (int start : {-200, -150, -50, 0, 3, 10, 11, 50, 70, 76, 77, 100, 101, 150, 300, 1000, 1419, 1420, 2000, 2399}) {
        PrinterMachine::Config c;
        c.sd1157.wagen_start = start;
        Lauf l(c);
        l.fahre(10'000'000);
        auto& st = l.m->sd1157();
        EXPECT_LE(st.druck().wagen().position(), 76) << "Start " << start;
        EXPECT_GT(st.druck().wagen().position(), -200) << "Start " << start << ": Anschlag erreicht";
        EXPECT_FALSE(st.druck().wagen().laeuft()) << "Start " << start;
        EXPECT_EQ(st.druck().papier(0).schritteVor(), 28u) << "Start " << start;
        EXPECT_EQ(st.druck().papier(0).schritteRueck(), 28u) << "Start " << start;
        EXPECT_EQ(st.druck().papier(0).lage(), 0) << "Start " << start;
        EXPECT_TRUE(st.lampeOnline()) << "Start " << start;
        EXPECT_FALSE(st.lampeFehler()) << "Start " << start;
        EXPECT_FALSE(l.besucht.test(0x0433)) << "NMI-Routine, Start " << start;
    }
}

TEST(Sd1157Rom, ResetStartetNeuUndErreichtDieLeerlaufschleifeWieder)
{
    Lauf l;
    l.fahre(4'000'000);
    l.m->reset();   // /RESET versetzt den Wagen nicht: er steht links im Randbereich (der Fall aus WP1)
    EXPECT_EQ(l.m->cpuPC(), 0x0000);
    EXPECT_LE(l.m->sd1157().druck().wagen().position(), 76);
    const uint64_t t0 = l.m->totalCycles();
    l.fahre(t0 + 10'000'000);   // aus dem Randbereich: erst 30 Spalten vor, dann zurueck bis /LR (~8,6 Mio. Takte)
    EXPECT_TRUE(l.m->sd1157().lampeOnline());
    EXPECT_FALSE(l.m->sd1157().lampeFehler());
    EXPECT_FALSE(l.besucht.test(0x0433));
    EXPECT_EQ(l.m->sd1157().sioAusgabe(1), std::string("\x11"));
}

}  // namespace
