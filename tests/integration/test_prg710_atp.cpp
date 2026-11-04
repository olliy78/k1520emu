/**
 * @file test_prg710_atp.cpp
 * @brief PRG 710-1 (Entwurf 48, AP-B1): ATP-Zusatz - SIO C0H-C3H mit Kanalschleife Tx->Rx,
 *        CTC CCH-CFH mit Takt an CLK/TRG0/1, Anzeige F4H/F5H - und der BS620-Selbsttest.
 *
 * `Prg710Atp.*` treibt die Bausteine ueber den Systembus, waehrend das ROM der Maschine in
 * seiner Tastaturabfrage laeuft.  `Prg710Bs620.SelbsttestOhneFehler` startet die echte
 * Diskette `tests/fixtures/bs600/BS600.hfe` (Kopie ueber TempDisk).
 */

#include <gtest/gtest.h>

#include <string>

#include "core/logger.h"
#include "core/machines/prg710/prg710.h"
#include "tests/support/fixtures.h"

namespace {
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
    // Fixture tests/fixtures/bs600/BS600.hfe (BS620, vom Anwender freigegeben); nur ueber TempDisk.
    k1520test::TempDisk kopie("../bs600/BS600.hfe", "bs620_selbsttest.hfe");
    ASSERT_FALSE(k1520test::readFileBytes(kopie.path()).empty()) << "Fixture BS600.hfe fehlt";

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
