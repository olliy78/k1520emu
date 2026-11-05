/**
 * @file test_sd1157_text.cpp
 * @brief SD 1157 an der seriellen Schnittstelle (Entwurf 49 WP4): Anschluesse am `SerialHub`, Baud neben der
 *        Druck-Zeitbasis der CTC 0, Online/Offline zur Laufzeit, Empfangspfad des ROM.
 *
 * Stand: Der Empfangspfad des ROM-Abzugs ist **nicht** als Druckweg nachweisbar (Test `Sd1157Empfang.Befund...`,
 * [?]); darum gibt es hier noch keinen Text-Druck-Test mit Orakel — siehe doc/merkposten/drucker.md Nr. 16.
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <string>

#include "core/machines/printer/printer_machine.h"
#include "tests/support/sd1157_orakel.h"

using namespace sd1157test;
using k1520::serial::Wandler;

namespace {


/// Gegenseite am Wandler: sammelt, was der Drucker sendet, und schickt Text (hoechstens 64 Zeichen je Pumpe).
struct Gegenstelle {
    Wandler& w;
    std::string rx, tx;
    size_t gesendet = 0;
    void pump()
    {
        uint8_t b[256];
        size_t n;
        while ((n = w.fernNimm(b, sizeof b)) > 0) rx.append(reinterpret_cast<char*>(b), n);
        if (gesendet < tx.size()) {
            const size_t k = std::min<size_t>({w.fernFrei(), tx.size() - gesendet, 64});
            if (k) { w.fernGib(reinterpret_cast<const uint8_t*>(tx.data()) + gesendet, k); gesendet += k; }
        }
    }
};

struct Lauf {
    std::unique_ptr<PrinterMachine> m;
    explicit Lauf(PrinterMachine::Config c = {}) : m(std::make_unique<PrinterMachine>(c)) {}
    Sd1157Steuerung& st() { return m->sd1157(); }
    void fahre(uint64_t takte, Gegenstelle* g = nullptr)
    {
        const uint64_t z = m->totalCycles() + takte;
        while (m->totalCycles() < z) { m->run(5000); if (g) g->pump(); }
    }
};

PrinterMachine::Config offlineStart()
{
    PrinterMachine::Config c;
    c.sd1157.online = false;
    return c;
}

}  // namespace

// ─── Anschluesse ─────────────────────────────────────────────────────────────

TEST(Sd1157Seriell, ZweiAnschluesseNachDerGeraetebeschriftung)
{
    Lauf l;
    auto* hub = l.m->serialHub();
    ASSERT_NE(hub, nullptr);
    ASSERT_EQ(hub->anzahl(), 2);
    EXPECT_EQ(hub->info(0).name, "IFSS");
    EXPECT_FALSE(hub->info(0).v24);
    EXPECT_EQ(hub->info(1).name, "V.24");
    EXPECT_TRUE(hub->info(1).v24);
    EXPECT_EQ(l.m->serielleAnschluesse().size(), 2u);
}

TEST(Sd1157Seriell, DasDc1NachDemEinschaltenErreichtDieGegenstelleAmIfss)
{
    Lauf l;
    Gegenstelle g{l.m->serialHub()->wandler(0)};
    l.m->serialHub()->wandler(0).anbinden();
    l.fahre(5'000'000, &g);
    EXPECT_EQ(g.rx, std::string("\x11"));   // hardware.md §11 Nr. 8: Freigabe an den Host
}

TEST(Sd1157Seriell, V24SendetNurMitCtsUndDcdDasDc1WartetAufDieGegenstelle)
{
    PrinterMachine::Config c;
    c.sd1157.schnittstelle = Sd1157Steuerung::Config::Schnittstelle::V24;
    Lauf l(c);
    l.fahre(5'000'000);
    // Kabel ab: CTS/DCD inaktiv, Auto Enables halten das Zeichen im Sender
    EXPECT_TRUE(l.st().sio().channelA().tx_buf.has_value());
    Gegenstelle g{l.m->serialHub()->wandler(1)};
    l.m->serialHub()->wandler(1).anbinden();   // Telnet/Datei: alle Eingaenge aktiv
    l.fahre(300'000, &g);
    EXPECT_EQ(g.rx, std::string("\x11"));
}

TEST(Sd1157Seriell, DieBaudrateBleibtBeiDerDruckZeitbasisDerCtc0Stabil)
{
    // CTC 0 ist Baudgenerator UND (Takt-Interrupt m1a0d: 17H,17H = TC 23) Zeit des Drucks, am Zeilenende gestoppt (13H).
    // Ohne Filter in Sd1157Steuerung::baudTakte() sprange das Format des Wandlers waehrend des Selbsttests auf 417 Bd.
    Lauf l(offlineStart());
    Wandler& w = l.m->serialHub()->wandler(0);
    w.anbinden();
    Gegenstelle g{w};
    l.fahre(5'000'000, &g);
    l.st().setTasteTest(true);
    bool los = false;
    int roh_druck = 0, abweichend = 0, proben = 0;
    l.m->setCpuTraceCallback([&](const Z80& c) {
        if (c.PC == HPT3 && !los) { los = true; l.st().setTasteTest(false); }
        if ((++proben & 0x3FF) == 0) {
            if (l.st().ctc().teilerTakte(0) == 16u * 23u) ++roh_druck;
            const auto f = w.sicht().format;
            if (!f.gueltig || f.baud_nenn != 9600) ++abweichend;
        }
    });
    l.fahre(40'000'000, &g);
    l.m->setCpuTraceCallback(nullptr);
    ASSERT_TRUE(los);
    EXPECT_GT(roh_druck, 0) << "die Zeitbasis TC = 23 war nie zu sehen: der Test prueft nichts";
    EXPECT_EQ(abweichend, 0);
    EXPECT_GT(l.st().druck().blatt(0).punktzahl(), 0u);
}

// ─── Online/Offline zur Laufzeit (Merkposten Nr. 10) ─────────────────────────

TEST(Sd1157Online, ZurLaufzeitWirktPv1AberNichtTestUndDieLampeBleibtAn)
{
    // Quelle: `off` (IX+5 Bit 7) wird nur in sy40 (Anlauf offline) und m05b3 gesetzt; m05b3 ist der einzige Ort, der TEST
    // auswertet, und nur ueber m05ee (off = 1) erreichbar.  Mit off = 0 laeuft nach ONLINE -> offline der Zweig m0646/ta00:
    // PV1/PV2 gelten, TEST nicht, die Lampe (2A.1) geht nicht aus.
    Lauf l;
    l.fahre(5'000'000);
    l.st().setOnline(false);
    l.fahre(2'000'000);
    EXPECT_TRUE(l.st().lampeOnline());
    l.st().setTastePV1(true); l.fahre(300'000); l.st().setTastePV1(false); l.fahre(3'000'000);
    EXPECT_EQ(l.st().druck().papier(0).lage(), 8) << "PV1 wirkt offline";

    l.st().setTasteTest(true);
    bool los = false;
    l.m->setCpuTraceCallback([&](const Z80& c) { if (c.PC == HPT3) los = true; });
    l.fahre(20'000'000);
    l.m->setCpuTraceCallback(nullptr);
    l.st().setTasteTest(false);
    EXPECT_FALSE(los) << "TEST startet den Selbsttest zur Laufzeit nicht";
    EXPECT_EQ(l.st().druck().blatt(0).punktzahl(), 0u);
    EXPECT_TRUE(l.st().lampeOnline());

    // wieder online: PV1 wirkt nicht mehr
    l.st().setOnline(true);
    l.fahre(2'000'000);
    const int lage = l.st().druck().papier(0).lage();
    l.st().setTastePV1(true); l.fahre(300'000); l.st().setTastePV1(false); l.fahre(3'000'000);
    EXPECT_EQ(l.st().druck().papier(0).lage(), lage);
}

TEST(Sd1157Online, EinKurzerOfflineImpulsImLeerlaufAendertNichts)
{
    // Falls ONLINE am Geraet ein Taster ist ([?], hardware.md §14 Nr. 7): ein Impuls im Leerlauf wird nicht ausgewertet
    // (die Firmware liest den Pegel nur in den Zweigen oben); Lampe und Zustand bleiben.
    Lauf l;
    l.fahre(5'000'000);
    l.st().setOnline(false); l.fahre(100'000); l.st().setOnline(true);
    l.fahre(3'000'000);
    EXPECT_TRUE(l.st().lampeOnline());
    EXPECT_EQ(l.st().druck().papier(0).lage(), 0);
}

TEST(Sd1157Online, NachDemResetOfflineStartenSchaltetDieLampeAusUndTestLaeuft)
{
    Lauf l;
    l.fahre(5'000'000);
    l.st().setOnline(false);
    l.m->reset();                // sy00 liest ONLINE neu: sy40 setzt `off`
    l.fahre(10'000'000);
    EXPECT_FALSE(l.st().lampeOnline());
    l.st().setTasteTest(true);
    bool los = false;
    l.m->setCpuTraceCallback([&](const Z80& c) { if (c.PC == HPT3 && !los) { los = true; l.st().setTasteTest(false); } });
    l.fahre(40'000'000);
    l.m->setCpuTraceCallback(nullptr);
    EXPECT_TRUE(los);
    EXPECT_GT(l.st().druck().blatt(0).punktzahl(), 0u);
}

// ─── Empfangspfad des ROM ─────────────────────────────────────────────────────

TEST(Sd1157Empfang, ZeichenKommenVollstaendigImPufferAnUndDasRomLeertIhnNicht)
{
    // [?] BEFUND, nicht als Gastverhalten bestaetigt.  Der SIO-Empfang ist verlustfrei (alle Zeichen im Puffer 3800H ff., der
    // Wandler liefert im Zeichentakt).  Beim Holen schiebt das ROM (0D20H) den Puffer um 1 und zieht ipzf um 1 und ipzl um 2
    // ab; da ipzl vorher um 1 erhoeht wurde, bleibt der Abstand ipzf - ipzl konstant, ipzl < ipzf gilt dauernd, der Puffer
    // erscheint nie leer.  Folge im Emulator: nach den Textzeichen holt die Firmware endlos NUL; der Druck der Zeilen bleibt
    // aus (oder, bei laengerem Text, verspaetet).  Ob das am ROM-Abzug liegt oder an einer Annahme der Karte, ist offen.
    Lauf l;
    Wandler& w = l.m->serialHub()->wandler(0);
    w.anbinden();
    Gegenstelle g{w};
    l.fahre(5'000'000, &g);
    g.tx = "AAA\r\n";
    l.fahre(1'000'000, &g);
    EXPECT_EQ(w.sicht().bytes_empfangen, 5u);
    EXPECT_EQ(w.sicht().puffer_empfangen, 0u);
    auto wort = [&](uint16_t a) { return l.m->memReadDebug(a) | (l.m->memReadDebug(a + 1) << 8); };
    const int abstand1 = wort(0x2C7E) - wort(0x2C80);
    l.fahre(5'000'000, &g);
    const int abstand2 = wort(0x2C7E) - wort(0x2C80);
    EXPECT_GT(abstand1, 0);
    EXPECT_EQ(abstand1, abstand2) << "Abstand ipzf - ipzl konstant: der Puffer leert sich nie";
    EXPECT_LT(wort(0x2C7E), 0x37F0) << "ipzf laeuft unter den Pufferanfang";
}
