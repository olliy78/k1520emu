/**
 * @file test_sd1157_text.cpp
 * @brief SD 1157 an der seriellen Schnittstelle (Entwurf 49 WP4): Anschluesse am `SerialHub`, Baud neben der
 *        Druck-Zeitbasis der CTC 0, Online/Offline zur Laufzeit, Empfangspfad des ROM.
 *
 * Der Original-Abzug leert den Eingabepuffer nie (Byte 0D34H, Test `Sd1157Empfang.OriginalAbzug...`); die Vorgabe der Karte ist
 * `1157_repariert.rom` (doc/EPROMS/SD1157/README.md).  Der Textdruck wird gegen das unabhaengige Orakel (tests/support/
 * sd1157_orakel.h) geprueft: erwartet wird, was `1157zg.rom` und die Regeln der Quelle verlangen, nicht was der Emulator tut.
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <vector>
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
    bool folgt = false;   ///< Host beachtet DC3 (stoppt) und DC1 (weiter)
    bool frei = true;
    void pump()
    {
        uint8_t b[256];
        size_t n;
        while ((n = w.fernNimm(b, sizeof b)) > 0) {
            rx.append(reinterpret_cast<char*>(b), n);
            for (size_t i = 0; i < n; ++i) { if (b[i] == 0x13) frei = false; else if (b[i] == 0x11) frei = true; }
        }
        if (gesendet < tx.size() && (!folgt || frei)) {
            // folgt: duenne Leitung wie ein UART (hoechstens 2 Zeichen unterwegs), sonst stuende der Rueckstau des Wandlers dem
            // Host als Puffer zur Verfuegung, den es an der echten Leitung nicht gibt.
            const size_t luft = folgt ? (w.sicht().puffer_empfangen < 2 ? 2 - w.sicht().puffer_empfangen : 0) : 64;
            const size_t k = std::min<size_t>({w.fernFrei(), tx.size() - gesendet, luft});
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

// ─── Textdruck ───────────────────────────────────────────────────────────────

/// Lauf mit Gegenstelle am IFSS (Kanal 0) oder V.24 (Kanal 1); merkt je Zeile die Druckrichtung.
struct Textlauf {
    Lauf l;
    Wandler& w;
    Gegenstelle g;
    std::map<int, std::set<int>> richtungen;   ///< Papierzeile der Nadel 1 -> Fahrtrichtungen beim Anschlag
    struct Ereignis { uint64_t takt; uint8_t byte; size_t gesendet; };
    std::vector<Ereignis> empfangen;           ///< was die Gegenstelle vom Drucker sah, mit Maschinenzeit

    explicit Textlauf(PrinterMachine::Config c = {})
        : l(c), w(l.m->serialHub()->wandler(c.sd1157.schnittstelle == Sd1157Steuerung::Config::Schnittstelle::V24 ? 1 : 0)), g{w}
    {
        Sd1157Steuerung& st = l.st();
        l.m->setBusTrace([this, &st](bool io, bool rd, uint16_t a, uint8_t d) {
            if (io && !rd && (a & 0xFF) == 0xF9 && (d & 0x20) && st.nadeln())
                richtungen[st.druck().papierY(0)].insert(st.druck().wagen().richtung());
        });
        w.anbinden();
        pumpe(8'000'000);   // Grundstellung (ab ~5,6 Mio. Takten fragt die Firmware den SIO-Puffer ab), DC1 an den Host
    }
    ~Textlauf() { l.m->setBusTrace(nullptr); }

    void pumpe(uint64_t takte)
    {
        const uint64_t z = l.m->totalCycles() + takte;
        while (l.m->totalCycles() < z) {
            l.m->run(5000);
            const size_t vorher = g.rx.size();
            g.pump();
            for (size_t i = vorher; i < g.rx.size(); ++i) empfangen.push_back({l.m->totalCycles(), static_cast<uint8_t>(g.rx[i]), g.gesendet});
        }
    }
    /// Text senden und warten, bis nichts mehr passiert: alles gesendet, Puffer leer, 4 Mio. Takte (1,6 s) ohne neuen Punkt/Vorschub.
    void drucke(const std::string& t, uint64_t frist = 120'000'000)
    {
        g.tx += t;
        uint64_t ruhig = 0;
        auto stand = [&] { return l.st().druck().blatt(0).punktzahl() * 4096 + static_cast<uint64_t>(l.st().druck().papier(0).lage() + 8192); };
        uint64_t alt = stand();
        for (uint64_t d = 0; d < frist && ruhig < 4'000'000; d += 100'000) {
            pumpe(100'000);
            const uint64_t neu = stand();
            // Eingabepuffer leer = ipzl >= ipzf (2C80H/2C7E): das ROM holt ein Zeichen in ~21 000 Takten (DI-Schiebepuffer)
            auto wort = [&](uint16_t ad) { return l.m->memReadDebug(ad) | (l.m->memReadDebug(ad + 1) << 8); };
            const bool offen = g.gesendet < g.tx.size() || w.sicht().puffer_empfangen > 0 || wort(0x2C80) < wort(0x2C7E);
            ruhig = (neu == alt && !offen) ? ruhig + 100'000 : 0;
            alt = neu;
        }
    }
    const Blattmodell& blatt() { return l.st().druck().blatt(0); }
    int lage() { return l.st().druck().papier(0).lage(); }
    /// y der Nadel 1 der Zeile bei Papierlage @p schritte (nach dem Rand oben).
    int y0(int schritte)
    {
        const NadeldruckParameter& p = l.st().druck().parameter();
        return p.rand_oben_y + schritte * p.pv_schritt_y + p.nadel1_y;
    }
    /// Orakelvergleich der Zeile bei Papierlage @p schritte; "" = gleich.
    std::string pruefe(const Zeile& z, int schritte, const Zg& zg)
    {
        const NadeldruckParameter& p = l.st().druck().parameter();
        const int ya = y0(schritte);
        return vergleiche(blatt(), orakelZeile(z, zg, ya, p.nadel_teilung_y), ya - 2, ya + 18);
    }
    uint64_t punkte() { return blatt().punktzahl(); }
};

Zeile zeile(const std::string& t, Modus m = Modus::Normal) { return Zeile{{Segment{t, m}}}; }

/// Orakel-Punkte aller erwarteten Zeilen (je eine Papierlage) und Gesamtvergleich: nichts anderes auf dem Blatt.
std::string pruefeAlle(Textlauf& t, const std::vector<std::pair<int, Zeile>>& zeilen, const Zg& zg)
{
    std::string fehler;
    uint64_t soll = 0;
    const NadeldruckParameter& p = t.l.st().druck().parameter();
    for (const auto& z : zeilen) {
        const std::string m = t.pruefe(z.second, z.first, zg);
        if (!m.empty()) fehler += "Zeile bei Lage " + std::to_string(z.first) + ":\n" + m;
        soll += orakelZeile(z.second, zg, t.y0(z.first), p.nadel_teilung_y).size();
    }
    if (t.punkte() != soll) fehler += "Punktzahl " + std::to_string(t.punkte()) + " statt " + std::to_string(soll) + "\n";
    return fehler;
}

TEST(Sd1157Empfang, OriginalAbzugLeertDenPufferNie)
{
    // BEFUND am Original-Abzug (`rom=original`); die Vorgabe `1157_repariert.rom` leert den Puffer (Tests Sd1157Text.*).  Der SIO-Empfang ist verlustfrei (alle Zeichen im Puffer 3800H ff., der
    // Wandler liefert im Zeichentakt).  Beim Holen schiebt das ROM (0D20H) den Puffer um 1 und zieht ipzf um 1 und ipzl um 2
    // ab; da ipzl vorher um 1 erhoeht wurde, bleibt der Abstand ipzf - ipzl konstant, ipzl < ipzf gilt dauernd, der Puffer
    // erscheint nie leer.  Folge im Emulator: nach den Textzeichen holt die Firmware endlos NUL; der Druck der Zeilen bleibt
    // aus (oder, bei laengerem Text, verspaetet).  Ob das am ROM-Abzug liegt oder an einer Annahme der Karte, ist offen.
    PrinterMachine::Config oc;
    oc.sd1157.rom_repariert = false;
    Lauf l(oc);
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

// ─── Textdruck mit Orakel ────────────────────────────────────────────────────

TEST(Sd1157Text, KurzerTextMitCrLfDrucktDasOrakelbild)
{
    const Zg zg = Zg::laden();
    Textlauf t;
    t.drucke("Hallo\r\n");
    EXPECT_EQ(pruefeAlle(t, {{0, zeile("Hallo")}}, zg), "");
    EXPECT_EQ(t.lage(), 8) << "ein LF = 8 Schritte (6 lpi)";
}

TEST(Sd1157Text, MehrereZeilenInBeideDruckrichtungen)
{
    const Zg zg = Zg::laden();
    Textlauf t;
    // kurz, lang, kurz, lang: der kuerzeste Weg wechselt die Richtung
    const std::vector<std::string> z = {"Ein", "Zeile mit deutlich mehr Zeichen darin", "kurz", "Und wieder eine lange Zeile zum Schluss ok"};
    std::string text;
    std::vector<std::pair<int, Zeile>> erw;
    for (size_t i = 0; i < z.size(); ++i) { text += z[i] + "\r\n"; erw.push_back({static_cast<int>(i) * 8, zeile(z[i])}); }
    t.drucke(text);
    EXPECT_EQ(pruefeAlle(t, erw, zg), "");
    bool vor = false, rueck = false;
    for (const auto& kv : t.richtungen) {
        EXPECT_EQ(kv.second.size(), 1u) << "Zeile y=" << kv.first << ": Richtung wechselt nicht in der Zeile";
        vor |= kv.second.count(+1) > 0;
        rueck |= kv.second.count(-1) > 0;
    }
    EXPECT_TRUE(vor);
    EXPECT_TRUE(rueck) << "keine Zeile rueckwaerts gedruckt: der Test prueft die Richtungen nicht";
    EXPECT_EQ(t.richtungen.size(), z.size());
}

TEST(Sd1157Text, TabulatorLiegtAufVielfachenVonAcht)
{
    // Nach dem Einschalten stehen Tabmarken im Abstand 8 (1157.txt: "Modulo-8").  Spalte 0 -> 8, 3 -> 8, 8 -> 16.
    const Zg zg = Zg::laden();
    Textlauf t;
    t.drucke("A\tB\r\nxyz\tC\r\n12345678\tD\r\n");
    EXPECT_EQ(pruefeAlle(t, {{0, zeile("A       B")}, {8, zeile("xyz     C")}, {16, zeile("12345678        D")}}, zg), "");
}

// ─── Steuerfolgen ────────────────────────────────────────────────────────────

TEST(Sd1157Text, FormularvorschubFaehrtBisZurFormularlaengeUndDieNaechsteZeileStehtAufDerNaechstenSeite)
{
    // FF (0CH) = Vorschub bis zur Formularlaenge (BCH.5..4, 12" = 576 Schritte), unabhaengig von der aktuellen Zeile.
    const Zg zg = Zg::laden();
    Textlauf t;
    t.drucke("Seite eins\r\nzweite Zeile\r\n\f");
    EXPECT_EQ(t.lage(), 576);
    t.drucke("Seite zwei\r\n");
    EXPECT_EQ(pruefeAlle(t, {{0, zeile("Seite eins")}, {8, zeile("zweite Zeile")}, {576, zeile("Seite zwei")}}, zg), "");
}

TEST(Sd1157Text, FormularvorschubFolgtDerFormularlaengeDerBrueckeVierZoll)
{
    PrinterMachine::Config c;
    c.sd1157.formular_bahn1 = 0;   // 4" = 192 Schritte
    Textlauf t(c);
    t.drucke("x\r\n\f");
    EXPECT_EQ(t.lage(), 192);
}

TEST(Sd1157Text, ZeileUeber132SpaltenWirdAbgeschnittenUndMeldetFormatueberlauf)
{
    // Quelle m13ab: dpz >= 84H (132) setzt `fof` (zmb Bit 3); weitere Zeichen werden nicht gedruckt, es gibt KEINEN Umbruch.
    const Zg zg = Zg::laden();
    Textlauf t;
    std::string z;
    for (int i = 0; i < 140; ++i) z.push_back(static_cast<char>('A' + i % 26));
    t.drucke(z + "\r\n");
    EXPECT_EQ(pruefeAlle(t, {{0, zeile(z.substr(0, 132))}}, zg), "");
    EXPECT_EQ(t.lage(), 8) << "kein Umbruch auf eine zweite Zeile";
    EXPECT_NE(t.l.m->memReadDebug(0x2C5A) & 0x08, 0) << "zmb fof";
}

TEST(Sd1157Text, SchriftartenUndAchtLpiNachDenEscapeFolgenDerQuelle)
{
    // ESC [ 1 m Breit, ESC [ 3 m Schraeg, ESC [ 0 m Normal; ESC [ 5 SP L = 8 lpi (6 Schritte je LF), ESC [ 0 SP L = 6 lpi.
    const Zg zg = Zg::laden();
    Textlauf t;
    t.drucke("a\x1b[1mBre\x1b[3mSchr\x1b[0mnorm\r\n\x1b[5 L" "eins\r\nzwei\r\n\x1b[0 L" "drei\r\n");
    // Quelle m14a5: beim Verlassen von Schraegdruck zaehlt die Firmware den Druckpositionszaehler um 1 hoch (`inc (ix+0fh)`):
    // hinter einem Schraeg-Lauf bleibt eine Zelle LEER (Platz fuer den schiefen Auslauf).  Am Zeilenende ohne Folgen.
    Zeile z1{{Segment{"a", Modus::Normal}, Segment{"Bre", Modus::Breit}, Segment{"Schr", Modus::Schraeg}, Segment{" norm", Modus::Normal}}};
    EXPECT_EQ(pruefeAlle(t, {{0, z1}, {8, zeile("eins")}, {14, zeile("zwei")}, {20, zeile("drei")}}, zg), "");
    EXPECT_EQ(t.lage(), 28);
}

TEST(Sd1157Text, UngueltigeEscapeFolgenWerdenUebergangenUndNichtGedruckt)
{
    // 1157.txt: falsche ESC-Sequenzen werden ignoriert, keine Meldung an den Rechner (Aenderung 24.04.1986).
    const Zg zg = Zg::laden();
    Textlauf t;
    t.drucke("ab\x1b[7xcd\x1b" "Zef\r\n");
    const size_t n0 = t.empfangen.size();
    EXPECT_EQ(pruefeAlle(t, {{0, zeile("abcdef")}}, zg), "");
    for (size_t i = 0; i < n0; ++i) EXPECT_TRUE(t.empfangen[i].byte == 0x11 || t.empfangen[i].byte == 0x13) << "kein DC4/Fehlerbyte";
}

// ─── Rueckmeldungen an den Rechner (Quelle isiote, ist1..ist20) ─────────────

static std::string abDc3(const Textlauf& t)
{
    // Rueckmeldung = Bytes nach dem ersten DC3 bis vor das letzte DC1 (Fluss DC1/DC3 herausgefiltert)
    std::string r;
    for (const auto& e : t.empfangen) if (e.byte != 0x11 && e.byte != 0x13) r.push_back(static_cast<char>(e.byte));
    return r;
}

TEST(Sd1157Text, GeraetekennungUndStatusAmIfss)
{
    {   Textlauf t; t.drucke("\x1b[0c"); EXPECT_EQ(abDc3(t), "\x1b[4c") << "264: Einbahn (EBD = 1), schmal"; }
    {   Textlauf t; t.drucke("\x1b[5n"); EXPECT_EQ(abDc3(t), "\x1b[0n") << "Nullstatus"; }
    {   PrinterMachine::Config c; c.sd1157.breit = true;
        Textlauf t(c); t.drucke("\x1b[0c"); EXPECT_EQ(abDc3(t), "\x1b[5c") << "Einbahn, breit"; }
    {   PrinterMachine::Config c; c.sd1157.einbahn = false;
        Textlauf t(c); t.drucke("\x1b[0c"); EXPECT_EQ(abDc3(t), "\x1b[6c") << "zwei Bahnen (267)"; }
}

TEST(Sd1157Text, GeraetekennungUndStatusAmV24)
{
    PrinterMachine::Config c;
    c.sd1157.schnittstelle = Sd1157Steuerung::Config::Schnittstelle::V24;
    Textlauf t(c);
    t.drucke("\x1b[0c");
    EXPECT_EQ(abDc3(t), "\x1b[4c");
}

TEST(Sd1157Text, FormatueberlaufUndPapierendeErscheinenImStatus)
{
    // ist13: Papierende ('2') nur, wenn /PE tatsaechlich 0 ist (BCH.0); Papier kommt (Papierweg) -> der Status loescht sich.
    PrinterMachine::Config c;
    c.sd1157.papier.laenge = 40;
    Textlauf t(c);
    std::string txt;
    for (int i = 0; i < 8; ++i) txt += std::string(10, 'a' + i) + "\r\n";
    t.g.folgt = true;
    t.drucke(txt);
    EXPECT_NE(t.l.m->memReadDebug(0x2C5A) & 0x04, 0) << "zmb pe";
    t.empfangen.clear();
    t.drucke("\x1b[5n");
    EXPECT_EQ(abDc3(t), "\x1b[2n");
}

// ─── Papierende im Auftrag, Offline im Auftrag ───────────────────────────────

TEST(Sd1157Text, PapierendeImAuftragStoppetDenDruckNicht)
{
    // Merkposten 11: die Firmware setzt Papierende-Zustand, druckt und schiebt aber bis zum Auftragsende weiter; Lampen aendern sich nicht.
    const Zg zg = Zg::laden();
    PrinterMachine::Config c;
    c.sd1157.papier.laenge = 40;   // Papier zu Ende bei Lage 40 = nach 5 Zeilen
    Textlauf t(c);
    t.g.folgt = true;
    std::string txt;
    std::vector<std::pair<int, Zeile>> erw;
    for (int i = 0; i < 10; ++i) { const std::string z(20, 'a' + i); txt += z + "\r\n"; erw.push_back({i * 8, zeile(z)}); }
    t.drucke(txt);
    EXPECT_FALSE(t.l.st().druck().papier(0).papierDa());
    EXPECT_EQ(t.lage(), 80);
    EXPECT_EQ(pruefeAlle(t, erw, zg), "") << "alle zehn Zeilen gedruckt, auch hinter dem Papierende";
    EXPECT_TRUE(t.l.st().lampeOnline());
    EXPECT_FALSE(t.l.st().lampeFehler());
    EXPECT_EQ(t.empfangen.back().byte, 0x11) << "der Host bekommt am Ende die Freigabe";
}

TEST(Sd1157Text, OfflineMittenImAuftragDruckenLaeuftWeiterDieFreigabeKommtErstOnline)
{
    // Gemessen (Merkposten 10/17): der laufende Auftrag und der waehrend offline empfangene Text werden gedruckt; DC1 (Freigabe)
    // kommt erst nach dem Wechsel auf online (m0c8c ist nur im Online-Leerlauf erreichbar).  Kein zusaetzliches DC3 beim Empfang:
    // das Merkbit `dc3` ist seit dem Druckbeginn gesetzt.
    const Zg zg = Zg::laden();
    auto block = [](int von, int n) { std::string s; for (int i = von; i < von + n; ++i) s += std::string(20, 'a' + i) + "\r\n"; return s; };
    Textlauf t;
    t.g.tx = block(0, 4);
    while (t.lage() < 8) t.pumpe(100'000);   // Druck laeuft
    t.l.st().setOnline(false);
    t.g.tx += block(4, 4);                   // waehrend offline
    t.pumpe(80'000'000);
    const uint64_t offline_ende = t.l.m->totalCycles();
    EXPECT_EQ(t.empfangen.back().byte, 0x13) << "offline: keine Freigabe";
    std::vector<std::pair<int, Zeile>> erw;
    for (int i = 0; i < 8; ++i) erw.push_back({i * 8, zeile(std::string(20, 'a' + i))});
    EXPECT_EQ(pruefeAlle(t, erw, zg), "");
    EXPECT_EQ(t.lage(), 64);
    t.l.st().setOnline(true);
    t.pumpe(5'000'000);
    ASSERT_EQ(t.empfangen.back().byte, 0x11);
    EXPECT_GT(t.empfangen.back().takt, offline_ende);
}

TEST(Sd1157Text, OfflineImLeerlaufSendetBeimEmpfangDc3)
{
    // seri8: offline -> seri5 -> DC3 (sofort, noch vor dem Druck), sofern DC1 vorher die Sperre aufgehoben hatte.
    Textlauf t;
    t.drucke("x\r\n");
    ASSERT_EQ(t.empfangen.back().byte, 0x11);
    t.l.st().setOnline(false);
    t.pumpe(1'000'000);
    const size_t n = t.empfangen.size();
    t.g.tx += "y";    // ein Zeichen, kein Zeilenende: kein Druckbeginn
    t.pumpe(300'000);
    ASSERT_GT(t.empfangen.size(), n);
    EXPECT_EQ(t.empfangen[n].byte, 0x13);
    EXPECT_EQ(t.empfangen.size(), n + 1);
}

// ─── Flusssteuerung ──────────────────────────────────────────────────────────

TEST(Sd1157Fluss, IfssDc3VorDemPufferLaufUndDc1WennDerPufferLeerIstKeinZeichenGehtVerloren)
{
    // XOFF bei ipzf >= 3BF5H (nach 1014 Zeichen), XON, sobald der Puffer leer ist.  Der Host beachtet beides ueber eine duenne
    // Leitung (hoechstens 2 Zeichen unterwegs): dann kommt jede der 40 Zeilen an.
    const Zg zg = Zg::laden();
    Textlauf t;
    t.g.folgt = true;
    std::string txt;
    std::vector<std::pair<int, Zeile>> erw;
    for (int i = 0; i < 40; ++i) { const std::string z(39, 'a' + i % 26); txt += z + "\r\n"; erw.push_back({i * 8, zeile(z)}); }
    t.drucke(txt, 400'000'000);
    EXPECT_EQ(t.g.gesendet, txt.size());
    EXPECT_EQ(pruefeAlle(t, erw, zg), "");
    ASSERT_GE(t.empfangen.size(), 3u);
    EXPECT_EQ(t.empfangen.front().byte, 0x11) << "Freigabe nach dem Einschalten";
    EXPECT_EQ(t.empfangen.back().byte, 0x11) << "Freigabe am Ende";
    size_t erstes_dc3 = 0;
    for (const auto& e : t.empfangen) if (e.byte == 0x13) { erstes_dc3 = e.gesendet; break; }
    EXPECT_GE(erstes_dc3, 1014u);
    EXPECT_LE(erstes_dc3, 1018u) << "XOFF an der Puffergrenze 3BF5H (1014 Zeichen + 2 unterwegs)";
    // der Host hat zwischen DC3 und dem folgenden DC1 nichts gesendet
    for (size_t i = 1; i < t.empfangen.size(); ++i)
        if (t.empfangen[i - 1].byte == 0x13 && t.empfangen[i].byte == 0x11)
            EXPECT_LE(t.empfangen[i].gesendet - t.empfangen[i - 1].gesendet, 2u);
}

TEST(Sd1157Fluss, EinHostOhneFlusssteuerungVerliertHinterDemPufferdaten)
{
    // Gastverhalten, kein Emulatorfehler: der Puffer fasst 1 KiB (3800H..3BFFH); mehr, als waehrend des Drucks hereinkommt, geht verloren
    // (seri10: Ueberlauf, Byte verworfen).  Ein 1640-Byte-Auftrag ohne Beachtung von DC3 druckt weniger als 40 Zeilen.
    Textlauf t;
    std::string txt;
    for (int i = 0; i < 40; ++i) txt += std::string(39, 'a' + i % 26) + "\r\n";
    t.drucke(txt, 400'000'000);
    EXPECT_EQ(t.g.gesendet, txt.size());
    EXPECT_LT(t.lage(), 40 * 8);
    EXPECT_GE(t.lage(), 24 * 8) << "die ersten Zeilen (der Pufferinhalt) kommen";
}

TEST(Sd1157Fluss, V24GibtDc1JeGelesenemZeichenFreiUndDc3BeiDruckbeginn)
{
    // Der Patch im ROM (0D20H) sendet nach JEDEM aus dem Puffer geholten Zeichen DC1 an den V.24-Kanal (zeichenweise Freigabe);
    // zusaetzlich DC3 bei Druckbeginn (m0d70) und DC1 im Leerlauf (m0c92).  "AB\r\n" = 4 Zeichen.
    PrinterMachine::Config c;
    c.sd1157.schnittstelle = Sd1157Steuerung::Config::Schnittstelle::V24;
    Textlauf t(c);
    const size_t vor = t.empfangen.size();
    t.drucke("AB\r\n");
    int dc1_nach_dc3 = 0;
    bool dc3 = false;
    for (size_t i = vor; i < t.empfangen.size(); ++i) {
        if (t.empfangen[i].byte == 0x13) dc3 = true;
        else if (t.empfangen[i].byte == 0x11 && dc3) ++dc1_nach_dc3;
    }
    EXPECT_TRUE(dc3);
    EXPECT_EQ(dc1_nach_dc3, 4 + 1) << "ein DC1 je Zeichen und eins im Leerlauf";
}

TEST(Sd1157Fluss, DasEinschaltDc1KommtAmIfssGenauEinmalVorDemErstenZeichen)
{
    // Am IFSS keine Steuerleitungen: das DC1 nach dem Einschalten geht sofort hinaus (am V.24 haelt es die Karte bis zum Anbinden,
    // Merkposten 16).  Gemessen: bei angebundener Gegenstelle genau ein DC1, vor jedem Zeichen.
    Textlauf t;
    ASSERT_FALSE(t.empfangen.empty());
    EXPECT_EQ(t.empfangen.front().byte, 0x11);
    EXPECT_EQ(t.empfangen.size(), 1u);
}

// ─── Loopback-Telnet-Rundlauf ────────────────────────────────────────────────

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include "core/serial/hub.h"

TEST(Sd1157Telnet, TextUeberLoopbackTelnetDruckBildGleichDemOrakelUndDc1ZurueckAmSocket)
{
    using namespace k1520::serial;
    const Zg zg = Zg::laden();
    Lauf l;
    SerialHub& hub = *l.m->serialHub();
    SerialKonfig k = hub.konfig(0);
    k.betriebsart = Betriebsart::Telnet;
    k.rolle = Rolle::Server;
    k.port = 0;
    ASSERT_TRUE(hub.konfigurieren(0, k));
    ASSERT_TRUE(hub.start(0));
    const uint16_t port = hub.status(0).port_aktiv;
    ASSERT_NE(port, 0);

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // nur Loopback
    ASSERT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof a), 0);
    ::fcntl(fd, F_SETFL, O_NONBLOCK);

    std::string daten;   // empfangene Nutzbytes ohne Telnet-Verhandlung
    auto lies = [&] {
        uint8_t b[512];
        const ssize_t n = ::recv(fd, b, sizeof b, 0);
        for (ssize_t i = 0; i < n; ++i) {
            if (b[i] != 255) { daten.push_back(static_cast<char>(b[i])); continue; }
            if (i + 2 < n && (b[i + 1] == 251 || b[i + 1] == 253)) {   // WILL/DO x -> DO/WILL x (BINARY, SGA, ...)
                const uint8_t r[3] = {255, static_cast<uint8_t>(b[i + 1] == 251 ? 253 : 251), b[i + 2]};
                ::send(fd, r, 3, 0);
                i += 2;
            } else if (i + 1 < n && b[i + 1] == 255) { daten.push_back(static_cast<char>(255)); ++i; }
            else i += 2;
        }
    };
    auto frist = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < frist && hub.status(0).zustand != Zustand::Verbunden)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_EQ(hub.status(0).zustand, Zustand::Verbunden) << hub.status(0).meldung;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    lies();
    for (int i = 0; i < 100; ++i) { l.m->run(100'000); lies(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }   // Grundstellung

    const std::string text = "Hallo Welt\r\nzweite Zeile\tTab\r\n";
    ASSERT_EQ(::send(fd, text.data(), text.size(), 0), static_cast<ssize_t>(text.size()));
    frist = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < frist &&
           !(l.st().druck().papier(0).lage() >= 16 && !daten.empty() && daten.back() == 0x11 && daten.find('\x13') != std::string::npos)) {
        l.m->run(20'000);
        lies();
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    hub.stopAlle();
    ::close(fd);
    ASSERT_EQ(l.st().druck().papier(0).lage(), 16);
    EXPECT_NE(daten.find('\x13'), std::string::npos) << "DC3 bei Druckbeginn";
    EXPECT_EQ(daten.back(), 0x11) << "DC1 am Ende";
    const NadeldruckParameter& p = l.st().druck().parameter();
    Pixelmenge soll;
    for (const auto& z : {std::pair<int, Zeile>{0, zeile("Hallo Welt")}, {8, zeile("zweite Zeile    Tab")}}) {
        const int y0 = p.rand_oben_y + z.first * p.pv_schritt_y + p.nadel1_y;
        for (const Pixel& px : orakelZeile(z.second, zg, y0, p.nadel_teilung_y)) soll.insert(px);
    }
    EXPECT_EQ(vergleiche(l.st().druck().blatt(0), soll, 0, 200), "");
}
#endif
