/**
 * @file test_prg710_atp.cpp
 * @brief PRG 710-1 (Entwurf 48, AP-B1): ATP-Zusatz - SIO C0H-C3H mit Kanalschleife Tx->Rx,
 *        CTC CCH-CFH mit Takt an CLK/TRG0/1, Anzeige F4H/F5H - und der BS620-Selbsttest.
 *
 * `Prg710Atp.*` treibt die Bausteine ueber den Systembus, waehrend das ROM der Maschine in
 * seiner Tastaturabfrage laeuft.  `Prg710Bs620.SelbsttestOhneFehler` startet die echte
 * Diskette `tests/fixtures/bs600/bs620_prg710-1.hfe` (Kopie ueber TempDisk).
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/logger.h"
#include "core/machines/prg710/prg710.h"
#include "tests/support/fixtures.h"

namespace {
const FormatCatalog& formate() {
    static FormatCatalog c = [] { std::string f; return FormatCatalog::loadDefault(&f); }();
    return c;
}
const FsCatalog& dateisysteme() {
    static FsCatalog c = [] { std::string f; return FsCatalog::loadDefault(formate(), &f); }();
    return c;
}
/// Sektor @p s (1-basiert) der Spur @p t einer BS600-Diskette roh lesen (Spur 0: 26 x 128 B, sonst 15 x 256 B).
std::vector<uint8_t> bsSektor(const DiskVolume& v, int t, int s) {
    std::vector<uint8_t> b; uint16_t crc = 0;
    EXPECT_TRUE(v.readSectorAt(static_cast<uint8_t>(t), 0, s - 1, b, crc)) << t << "/" << s;
    return b;
}
using V = Prg710Machine::Config::Variante;

Prg710Machine::Config cfg(bool atp) {
    Prg710Machine::Config c;
    c.variante   = V::Prg710_1;
    c.atp_zusatz = atp;
    return c;
}

// Z80-SIO-Initialisierung des BS620 (6D10H): Kanalreset, x16 8E2, 8 Bit Tx, 8 Bit Rx.
void sioInit(Prg710Machine& m, uint8_t steuer) {
    static const uint8_t b[] = {0x00, 0x18, 0x04, 0x4F, 0x05, 0x68, 0x01, 0x00, 0x03, 0xC1};
    for (uint8_t v : b) m.bus().ioWrite(steuer, v);
}

// Sendet AAH ueber Datenport @p daten und liefert, was binnen @p frist Takten ankommt (-1 = nichts).
int schleife(Prg710Machine& m, uint8_t daten, uint8_t steuer, int frist = 12'000) {
    sioInit(m, steuer);
    m.bus().ioWrite(daten, 0xAA);
    int gelaufen = 0;
    while (gelaufen < frist) {
        gelaufen += m.run(200);
        if (m.bus().ioRead(steuer) & 0x01) return m.bus().ioRead(daten);
    }
    return -1;
}
}  // namespace

class Prg710Atp : public ::testing::Test {
protected:
    void SetUp() override {
        k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    }
};

TEST_F(Prg710Atp, SioKanalAHatSchleife) {
    Prg710Machine m(cfg(true));
    m.powerOn();
    EXPECT_EQ(schleife(m, 0xC0, 0xC2), 0xAA);
}

TEST_F(Prg710Atp, SioKanalBHatSchleife) {
    Prg710Machine m(cfg(true));
    m.powerOn();
    EXPECT_EQ(schleife(m, 0xC1, 0xC3), 0xAA);
}

TEST_F(Prg710Atp, SioKommtErstNachDerZeichenzeit) {
    // 12 Bit x16 x4 Takte = 768 Takte; BS620 wartet ca. 10 000 Takte.
    Prg710Machine m(cfg(true));
    m.powerOn();
    sioInit(m, 0xC2);
    m.bus().ioWrite(0xC0, 0xAA);
    m.run(300);
    EXPECT_EQ(m.bus().ioRead(0xC2) & 0x01, 0) << "Zeichen darf nicht sofort da sein";
    m.run(2000);
    EXPECT_EQ(m.bus().ioRead(0xC2) & 0x01, 1);
}

TEST_F(Prg710Atp, OhneZusatzKeineSio) {
    Prg710Machine m(cfg(false));
    m.powerOn();
    EXPECT_FALSE(m.atpZusatz());
    // unbelegter Port: Lesen liefert FFH (kein Zeichen-Bit-Sinn), Schreiben verpufft
    EXPECT_EQ(m.bus().ioRead(0xC2), 0xFF);
    EXPECT_EQ(m.bus().ioOwner(0xC2), nullptr);
}

TEST_F(Prg710Atp, CtcZaehltImZaehlerbetrieb) {
    // BS620 (6F4AH): 57H, 1FH, sofort lesen - der Zaehler muss weitergelaufen sein.
    Prg710Machine m(cfg(true));
    m.powerOn();
    for (uint8_t port : {0xCC, 0xCD}) {
        m.bus().ioWrite(port, 0x57);
        m.bus().ioWrite(port, 0x1F);
        m.run(12);
        EXPECT_NE(m.bus().ioRead(port), 0x1F) << "Kanal " << (port & 3);
    }
}

TEST_F(Prg710Atp, CtcStehtOhneZusatz) {
    Prg710Machine m(cfg(false));
    m.powerOn();
    m.bus().ioWrite(0xCD, 0x57);
    m.bus().ioWrite(0xCD, 0x1F);
    m.run(2000);
    EXPECT_EQ(m.bus().ioRead(0xCD), 0x1F);
}

TEST_F(Prg710Atp, CtcZeitgeberDesFernschreibersBleibt) {
    // CDH als Zeitgeber (07H, 18H) ignoriert CLK/TRG: ZC/TO1 = phi/384 wie ohne Zusatz.
    Prg710Machine a(cfg(false)), b(cfg(true));
    for (Prg710Machine* m : {&a, &b}) {
        m->powerOn();
        m->bus().ioWrite(0xCD, 0x07);
        m->bus().ioWrite(0xCD, 0x18);
    }
    EXPECT_EQ(a.ass590069().ctc().teilerTakte(1), 384u);
    EXPECT_EQ(b.ass590069().ctc().teilerTakte(1), 384u);
}

TEST_F(Prg710Atp, AnzeigeF4F5ProtokolliertSchreiben) {
    Prg710Machine m(cfg(true));
    m.powerOn();
    m.bus().ioWrite(0xF5, 0x31);
    m.bus().ioWrite(0xF4, 0x3E);
    const auto& a = m.atpAnzeige();
    EXPECT_EQ(a.f5, 0x31);
    EXPECT_EQ(a.f4, 0x3E);
    EXPECT_EQ(a.anzahl, 2u);
    ASSERT_EQ(a.protokoll.size(), 2u);
    EXPECT_EQ(a.protokoll[0].port, 0xF5);
    EXPECT_EQ(a.protokoll[1].port, 0xF4);
    EXPECT_EQ(m.bus().ioRead(0xF4), 0xFF) << "Lesen: nichts";
}

TEST_F(Prg710Atp, Seite04ImSpeicherIstRam) {
    Prg710Machine m(cfg(true));
    m.powerOn();
    m.bus().ioWrite(0x70E8, 0x04);   // E8H[7] := 04H
    m.bus().ioWrite(0x00EB, 0x0F);
    m.memWriteDebug(0x7000, 0x42);
    EXPECT_EQ(m.memReadDebug(0x7000), 0x42);
}

TEST_F(Prg710Atp, SteckenNurVorDemErstenLauf) {
    Prg710Machine m(cfg(false));
    std::string fehler;
    EXPECT_TRUE(m.steckeAtpZusatz(&fehler)) << fehler;
    EXPECT_FALSE(m.steckeAtpZusatz(&fehler));   // schon gesteckt
    Prg710Machine n(cfg(false));
    n.powerOn();
    n.run(1000);
    EXPECT_FALSE(n.steckeAtpZusatz(&fehler));
    EXPECT_FALSE(n.atpZusatz());
}

// --- BS620 -------------------------------------------------------------------

namespace {
std::string bildText(Prg710Machine& m) {
    std::string s;
    for (int r = 0; r < 24; ++r) {
        std::string z;
        for (int c = 0; c < 80; ++c) {
            const uint8_t ch = static_cast<uint8_t>(m.screenChar(c, r) & 0x7F);
            z += (ch >= 0x20 && ch < 0x7F) ? static_cast<char>(ch) : ' ';
        }
        while (!z.empty() && z.back() == ' ') z.pop_back();
        s += z + "\n";
    }
    return s;
}
}  // namespace

TEST(Prg710Bs620, SelbsttestOhneFehler) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    // Fixture tests/fixtures/bs600/bs620_prg710-1.hfe (BS620, vom Anwender freigegeben); nur ueber TempDisk.
    k1520test::TempDisk kopie("../bs600/bs620_prg710-1.hfe", "bs620_selbsttest.hfe");
    ASSERT_FALSE(k1520test::readFileBytes(kopie.path()).empty()) << "Fixture bs620_prg710-1.hfe fehlt";

    Prg710Machine m(cfg(true));
    m.powerOn();
    ASSERT_TRUE(m.mountDisk(0, kopie.path(), m.defaultFormatName(0), false)) << m.lastError();

    // NKM-LOADER abwarten, ET/ENTER = Laden vom linken Laufwerk
    long long done = 0;
    while (done < 60'000'000 && bildText(m).find("NKM-LOADER") == std::string::npos) done += m.run(20'000);
    ASSERT_NE(bildText(m).find("NKM-LOADER"), std::string::npos) << bildText(m);
    m.run(2'000'000);
    m.keyPress(0x01000004, false, false);
    m.keyRelease(0x01000004);

    const char* fehler[] = {"RAM-ZELLE", "PIO ZE PORT", "CTC ZE KANAL", "CTC ATP KANAL", "SIO  KANAL"};
    bool menue = false;
    done = 0;
    while (done < 200'000'000) {
        done += m.run(100'000);
        const std::string b = bildText(m);
        for (const char* f : fehler)
            ASSERT_EQ(b.find(f), std::string::npos)
                << "Selbsttestfehler nach " << done << " Takten, PC=" << std::hex << m.cpuPC() << "\n" << b;
        if (b.find("DIENSTPROGRAMME") != std::string::npos) { menue = true; break; }
    }
    EXPECT_TRUE(menue) << "Hauptmenue nicht erreicht, PC=" << std::hex << m.cpuPC() << "\n" << bildText(m);
}

namespace {
// Bis zum Hauptmenue von BS620 booten (Maschine mit Fixture-Kopie); liefert false bei Misserfolg.
bool bootBs620(Prg710Machine& m, const std::string& pfad) {
    m.powerOn();
    if (!m.mountDisk(0, pfad, m.defaultFormatName(0), false)) return false;
    long long done = 0;
    while (done < 60'000'000 && bildText(m).find("NKM-LOADER") == std::string::npos) done += m.run(20'000);
    m.run(2'000'000);
    m.keyPress(0x01000004, false, false);
    m.keyRelease(0x01000004);
    for (done = 0; done < 200'000'000; done += m.run(100'000))
        if (bildText(m).find("DIENSTPROGRAMME") != std::string::npos) { m.run(3'000'000); return true; }
    return false;
}

// Taste tippen (K7672-Matrixtaste oder Host-Code), 60 ms halten, danach @p warte Takte laufen.
void taste(Prg710Machine& m, uint32_t code, long long warte = 8'000'000) {
    m.keyPress(code, false, false);
    m.run(150'000);
    m.keyRelease(code);
    m.run(warte);
}
constexpr uint32_t T_UP = 0x03000000 | 0x79, T_DOWN = 0x03000000 | 0x18, T_LEFT = 0x03000000 | 0x28,
                   T_RIGHT = 0x03000000 | 0x08, T_ENTER = 0x01000004, T_ESC = 0x01000000;
}  // namespace

// Zeile mit der Cursormarke '<' (Menues: "NAME  :  <").
std::string markierte(Prg710Machine& m) {
    const std::string t = bildText(m);
    size_t k = t.find(" <");
    if (k == std::string::npos) return "";
    size_t a = t.rfind('\n', k) + 1;
    std::string r = t.substr(a, t.find('\n', k) - a);
    return r.substr(r.find_first_not_of(' '));
}

// Menuefuehrung von BS620 (doc/merkposten/bs600.md): Cursor = ESC [ A/B (K7672-Matrix 79H/18H),
// ENTER = 0DH.  Hauptmenue: vier Eintraege, '<' markiert; der Cursor bleibt am Ende stehen.
TEST(Prg710Bs620, MenueCursorUndUnterbild) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie("../bs600/bs620_prg710-1.hfe", "bs620_menue.hfe");
    Prg710Machine m(cfg(true));
    ASSERT_TRUE(bootBs620(m, kopie.path())) << bildText(m);
    EXPECT_NE(bildText(m).find("BS600                           VERSION: 04"), std::string::npos) << bildText(m);
    const char* eintraege[] = {"PROG/TEST PC600    : <", "DIENSTPROGRAMME    : <", "PROG PMC600/CNC600 : <",
                               "PROG PMC600/CNC-H  : <", "PROG PMC600/CNC-H  : <"};
    EXPECT_EQ(markierte(m), eintraege[0]);
    for (int i = 1; i <= 4; ++i) {          // dreimal abwaerts, dann Anschlag
        taste(m, T_DOWN, 3'000'000);
        EXPECT_EQ(markierte(m), eintraege[i]) << "nach " << i << " x abwaerts";
    }
    taste(m, T_UP, 3'000'000);
    EXPECT_EQ(markierte(m), eintraege[2]);
    taste(m, 0x01000013, 3'000'000);        // Host-Kursor auf (Qt::Key_Up) wie die Matrixtaste
    taste(m, T_UP, 3'000'000);
    EXPECT_EQ(markierte(m), eintraege[0]);

    // ENTER auf PROG/TEST PC600 -> Unterbild BETRIEBSARTEN mit acht Eintraegen
    taste(m, T_ENTER, 80'000'000);
    const std::string b = bildText(m);
    EXPECT_NE(b.find("BETRIEBSARTEN"), std::string::npos) << b;
    for (const char* e : {"ANZEIGE", "KORREKTUR", "LOGIKTRANSFER", "EIN-/AUSGABE", "ZWANGSSETZEN", "DOKUMENTATION",
                          "MONITOR", "SYSTEMDISK"})
        EXPECT_NE(b.find(e), std::string::npos) << e;
    EXPECT_EQ(markierte(m), "ANZEIGE       : <");
}

// DIENSTPROGRAMME fragt "LAUFWERK 1  J" (J/N).  N + ENTER -> Laufwerk 2 ist leer -> blinkende
// Meldung "LAUFWERK 2"; ESC quittiert und fuehrt ins Hauptmenue zurueck.
TEST(Prg710Bs620, DienstprogrammeLaufwerkFrageUndEscZurueck) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie("../bs600/bs620_prg710-1.hfe", "bs620_menue2.hfe");
    Prg710Machine m(cfg(true));
    ASSERT_TRUE(bootBs620(m, kopie.path())) << bildText(m);
    taste(m, T_DOWN, 3'000'000);
    taste(m, T_ENTER, 100'000'000);
    EXPECT_NE(bildText(m).find("LAUFWERK 1  J"), std::string::npos) << bildText(m);
    taste(m, 'N', 2'000'000);
    taste(m, T_ENTER, 150'000'000);
    EXPECT_NE(bildText(m).find("LAUFWERK 2"), std::string::npos) << bildText(m);   // Meldung (blinkt)
    taste(m, T_ESC, 50'000'000);
    EXPECT_NE(bildText(m).find("VERSION: 04"), std::string::npos) << bildText(m);
    EXPECT_NE(bildText(m).find("DIENSTPROGRAMME    :"), std::string::npos);
}


// einlegen -> Laufwerk J -> Geraet.  Liefert die Maschine am Geraetemenue.
void zumListenGeraet(Prg710Machine& m) {
    taste(m, T_DOWN, 3'000'000);
    taste(m, T_ENTER, 100'000'000);
    taste(m, 'J', 2'000'000);
    for (int i = 0; i < 4; ++i) taste(m, T_ENTER, i < 2 ? 300'000'000 : 600'000'000);
    taste(m, 'J', 2'000'000);
    taste(m, T_ENTER, 1'500'000'000);
}

// AP-B2b: Der Plattentreiber von BS620 (12E3H-1318H) wartet nach dem Strobe auf Marken-FF = 0 (`IN (12H) / AND 02H /
// JR NZ`), also low-aktiv wie am 710 - das ROM des 710-1 aber auf 1.  Mit der Polaritaet des 710-1 (high-aktiv) liest er
// die Luecken vor der Marke (4E 00 00 00) und haengt.  Nach dem Boot wird deshalb umgeschaltet (merkposten bs600 Nr. 5).
TEST(Prg710Bs620, DienstprogrammeLiestDenDatentraeger) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie("../bs600/bs620_prg710-1.hfe", "bs620_liste.hfe");
    Prg710Machine m(cfg(true));
    ASSERT_TRUE(bootBs620(m, kopie.path())) << bildText(m);
    zumListenGeraet(m);
    taste(m, T_DOWN, 20'000'000);               // GERAET: Anzeige BS (der Cursor laeuft im Kreis)
    taste(m, T_ENTER, 3'000'000'000);
    const std::string kopf = bildText(m);
    EXPECT_NE(kopf.find("DISK-NAME   : BS600-"), std::string::npos) << kopf;
    EXPECT_NE(kopf.find("DISK ENTHAELT 28 VON 54 MOEGLICHEN DATEIEN"), std::string::npos) << kopf;
    taste(m, T_DOWN, 1'500'000'000);
    const std::string liste = bildText(m);
    for (const char* e : {"SYSVSP  220983  HOF  P  01/01  01/01", "SYSNLD  081288  KRU  P  01/02  01/03",
                          "SYSDMF  250285  HOF  P  01/04  01/04"})
        EXPECT_NE(liste.find(e), std::string::npos) << e << "\n" << liste;
}

// Wartet bis @p text auf dem Bild steht (hoechstens @p max Takte), dann noch etwas Ruhe.
bool warteAuf(Prg710Machine& m, const char* text, long long max = 6'000'000'000) {
    for (long long g = 0; g < max; g += 2'000'000) {
        if (bildText(m).find(text) != std::string::npos) { m.run(8'000'000); return true; }
        m.run(2'000'000);
    }
    return false;
}
void tippe(Prg710Machine& m, const std::string& t) { for (char ch : t) taste(m, static_cast<uint32_t>(ch), 3'000'000); }

// AP-B2b, Gegenprobe zu den DiskTool-Annahmen (doc/bs600_diskettenformat.md §5/§6): BS620 kopiert SYSVSP
// (Quell- und Zieldisk sind hier dieselbe Diskette) als KOPIE1.  Danach muessen stimmen:
//  - ein neuer Verzeichniseintrag (42 B) auf dem ERSTEN freien Platz (Sektor 16, Platz 1), Typ P, Anfang = Ende = Spur 26/
//    Sektor 7 (Binaerbytes 1AH 07H), Rest wie das Original (Datum, Signum, Adressen);
//  - das Belegungsbit von Spur 26, Sektor 7 gesetzt (Sektor 26, Byte 2*(26-1), Bit 1) - sonst unveraendert;
//  - Allokation: erster freier Sektor ueber Spurgrenzen (Spur 26 Sektor 7 war der erste freie);
//  - der Datensektor ist byteweise der von SYSVSP; alle anderen Sektoren unveraendert.
TEST(Prg710Bs620, DateiKopierenAendertNurEintragBelegungUndDaten) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie("../bs600/bs620_prg710-1.hfe", "bs620_cp.hfe");
    std::string err;
    std::vector<std::vector<uint8_t>> vor;                       // alle 611 Sektoren vorher
    {
        auto v = DiskVolume::open(kopie.path(), "", formate(), dateisysteme(), err, true, true);
        ASSERT_TRUE(v) << err;
        for (int t = 0; t < 40; ++t)
            for (int s = 1; s <= (t == 0 ? 26 : 15); ++s) vor.push_back(bsSektor(*v, t, s));
    }
    {
        Prg710Machine m(cfg(true));
        ASSERT_TRUE(bootBs620(m, kopie.path())) << bildText(m);
        taste(m, T_DOWN, 3'000'000);
        taste(m, T_ENTER, 100'000'000);
        taste(m, 'J', 2'000'000);
        taste(m, T_ENTER, 100'000'000);
        ASSERT_TRUE(warteAuf(m, "DISK-NAME :")) << bildText(m);
        taste(m, T_ENTER, 100'000'000);
        ASSERT_TRUE(warteAuf(m, "COPY   ")) << bildText(m);
        taste(m, T_DOWN, 20'000'000);
        taste(m, T_ENTER, 100'000'000);
        ASSERT_TRUE(warteAuf(m, "STECKE QUELLDISK")) << bildText(m);
        taste(m, T_ENTER, 100'000'000);
        ASSERT_TRUE(warteAuf(m, "LAUFWERK 1  J")) << bildText(m);
        taste(m, T_ENTER, 100'000'000);
        ASSERT_TRUE(warteAuf(m, "DATEI-NAME")) << bildText(m);
        tippe(m, "SYSVSP");
        taste(m, T_ENTER, 100'000'000);
        ASSERT_TRUE(warteAuf(m, "START-ADR")) << bildText(m);
        taste(m, T_ENTER, 100'000'000);
        ASSERT_TRUE(warteAuf(m, "STECKE ZIELDISK")) << bildText(m);
        taste(m, T_ENTER, 100'000'000);
        ASSERT_TRUE(warteAuf(m, "LAUFWERK 1  J")) << bildText(m);
        taste(m, T_ENTER, 100'000'000);
        ASSERT_TRUE(warteAuf(m, "DATEI-NAME", 12'000'000'000)) << bildText(m);
        tippe(m, "KOPIE1");
        taste(m, T_ENTER, 100'000'000);
        // Einzellaufwerk: BS620 verlangt Quell- und Zieldisk abwechselnd (STECKE ...) - jeweils mit ENTER bestaetigen.
        bool fertig = false;
        for (int runde = 0; runde < 8 && !fertig; ++runde) {
            for (long long g = 0; g < 6'000'000'000 && !fertig; g += 2'000'000) {
                const std::string b = bildText(m);
                fertig = b.find("SYSVSP KOPIERT") != std::string::npos;
                if (fertig || b.find("STECKE") != std::string::npos) break;
                m.run(2'000'000);
            }
            if (!fertig) { m.run(8'000'000); taste(m, T_ENTER, 100'000'000); }
        }
        ASSERT_TRUE(fertig) << bildText(m);
        m.run(100'000'000);                   // Schreibpause des Mediums (autoFlush ~0.5 s Maschinenzeit)
    }
    auto v = DiskVolume::open(kopie.path(), "", formate(), dateisysteme(), err, true, true);
    ASSERT_TRUE(v) << err;
    std::vector<std::vector<uint8_t>> nach;
    for (int t = 0; t < 40; ++t)
        for (int s = 1; s <= (t == 0 ? 26 : 15); ++s) nach.push_back(bsSektor(*v, t, s));
    ASSERT_EQ(vor.size(), nach.size());
    auto idx = [](int t, int s) { return t == 0 ? s - 1 : 26 + (t - 1) * 15 + (s - 1); };

    std::vector<int> geaendert;
    for (size_t i = 0; i < vor.size(); ++i) if (vor[i] != nach[i]) geaendert.push_back(static_cast<int>(i));
    EXPECT_EQ(geaendert, (std::vector<int>{idx(0, 16), idx(0, 26), idx(26, 7)}));

    // Eintrag: Sektor 16 Platz 1 (Versatz 42) war leer (20H) und ist jetzt SYSVSP-Eintrag mit neuem Namen/Lage.
    const auto& e = nach[idx(0, 16)];
    const std::string eintrag(e.begin() + 42, e.begin() + 84);
    EXPECT_EQ(eintrag, std::string("KOPIE1P\x1A\x07\x1A\x07" "220983HOF290029FF2900          "));
    for (int i = 0; i < 42; ++i) EXPECT_EQ(e[i], vor[idx(0, 16)][i]) << "Platz 0 unveraendert, Byte " << i;
    // Original-Eintrag SYSVSP (Sektor 8 Platz 0) als Vorlage: gleiche Felder hinter dem Namen.
    const auto& o = nach[idx(0, 8)];
    EXPECT_EQ(std::string(o.begin() + 6, o.begin() + 7), "P");
    EXPECT_EQ(std::string(o.begin() + 11, o.begin() + 42), std::string(e.begin() + 42 + 11, e.begin() + 84));

    // Belegungstabelle: nur Bit 1 (Sektor 7) im ersten Byte von Spur 26
    const auto& b0 = vor[idx(0, 26)];
    const auto& b1 = nach[idx(0, 26)];
    for (size_t i = 0; i < b0.size(); ++i) {
        if (i == 2 * (26 - 1)) EXPECT_EQ(b1[i], b0[i] | 0x02) << "Belegung Spur 26";
        else                   EXPECT_EQ(b1[i], b0[i]) << "Belegungsbyte " << i;
    }
    EXPECT_EQ(b0[2 * (26 - 1)] & 0x02, 0) << "Sektor 7 war frei";
    EXPECT_EQ(b0[2 * (26 - 1)] & 0xFC, 0xFC) << "Sektor 1-6 waren belegt: erster freier Sektor";

    // Daten: byteweise die von SYSVSP (Spur 1, Sektor 1)
    EXPECT_EQ(nach[idx(26, 7)], nach[idx(1, 1)]);
}
