/**
 * @file test_ata_geraet.cpp
 * @brief ATA-/IDE-Geraet (Entwurf 50 AP G1): Task-File, Befehle, Identify, CHS/LBA, Reset, Slave,
 *        BSY/DRQ-Folge, Rohabbild und .img.gz, Sperrdatei, Zurueckschreiben.
 */
#include "core/peripherals/ata/ata_geraet.h"
#include "tests/support/temp_path.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using k1520::AtaGeraet;
using R = AtaGeraet::Reg;

namespace {

constexpr uint64_t SEKTOREN = 5040;   // 5 Zylinder x 16 Koepfe x 63 Sektoren

struct Platte {
    std::string pfad;
    AtaGeraet g;
    explicit Platte(const std::string& name = "ata.img", uint64_t sektoren = SEKTOREN) {
        pfad = k1520test::tempPath("k1520_" + name);
        std::error_code ec;
        std::filesystem::remove(pfad, ec);
        std::filesystem::remove(pfad + ".lock", ec);
        std::string f;
        EXPECT_TRUE(AtaGeraet::abbildAnlegen(pfad, sektoren, &f)) << f;
        EXPECT_TRUE(g.abbildOeffnen(pfad, {}, &f)) << f;
    }
    ~Platte() {
        g.auswerfen();
        std::error_code ec;
        std::filesystem::remove(pfad, ec);
        std::filesystem::remove(pfad + ".lock", ec);
        std::filesystem::remove(pfad + ".tmp", ec);
    }
    uint8_t st() { return static_cast<uint8_t>(g.lese(R::StatusCommand)); }
    // Auf "nicht BSY" warten (hoechstens 10 Abfragen: sonst haengt eine Gastschleife)
    uint8_t warte() {
        for (int i = 0; i < 10; ++i) {
            uint8_t s = st();
            if (!(s & 0x80)) return s;
        }
        ADD_FAILURE() << "BSY endet nicht";
        return 0xFF;
    }
    void chs(unsigned c, unsigned h, unsigned s, unsigned n) {
        g.schreibe(R::SectorCount, n);
        g.schreibe(R::SectorNumber, s);
        g.schreibe(R::CylLow, c & 0xFF);
        g.schreibe(R::CylHigh, c >> 8);
        g.schreibe(R::DriveHead, 0xA0 | h);
    }
    void lba(uint32_t l, unsigned n) {
        g.schreibe(R::SectorCount, n);
        g.schreibe(R::SectorNumber, l & 0xFF);
        g.schreibe(R::CylLow, (l >> 8) & 0xFF);
        g.schreibe(R::CylHigh, (l >> 16) & 0xFF);
        g.schreibe(R::DriveHead, 0xE0 | ((l >> 24) & 0x0F));
    }
    void befehl(uint8_t c) { g.schreibe(R::StatusCommand, c); }
    std::vector<uint16_t> lies256() {
        std::vector<uint16_t> v;
        for (int i = 0; i < 256; ++i) v.push_back(g.lese(R::Data));
        return v;
    }
    void schreib256(uint16_t basis) {
        for (int i = 0; i < 256; ++i) g.schreibe(R::Data, static_cast<uint16_t>(basis + i));
    }
};

std::string wortString(const std::vector<uint16_t>& w, size_t a, size_t n) {
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        s += static_cast<char>(w[a + i] >> 8);
        s += static_cast<char>(w[a + i] & 0xFF);
    }
    return s;
}

}  // namespace

TEST(AtaGeraet, IdentifyLiefertGeometrieKapazitaetUndModell) {
    Platte p;
    p.befehl(0xEC);
    uint8_t s = p.warte();
    EXPECT_EQ(s & 0xC9, 0x48);   // DRDY + DRQ, kein BSY/ERR
    const auto w = p.lies256();
    EXPECT_EQ(w[1], 5);
    EXPECT_EQ(w[3], 16);
    EXPECT_EQ(w[6], 63);
    EXPECT_EQ(w[54], 5);
    EXPECT_EQ(w[55], 16);
    EXPECT_EQ(w[56], 63);
    EXPECT_NE(w[49] & 0x0200, 0);          // LBA
    EXPECT_EQ(w[60] | (w[61] << 16), SEKTOREN);
    EXPECT_EQ(w[57] | (w[58] << 16), 5u * 16 * 63);
    EXPECT_EQ(wortString(w, 27, 20).substr(0, 12), "K1520EMU IDE");
    EXPECT_EQ(wortString(w, 27, 20).substr(12), std::string(28, ' '));
    // Byteweise ATA-Konvention: erstes Zeichen im hohen Byte des ersten Wortes
    EXPECT_EQ(w[27], ('K' << 8) | '1');
    EXPECT_EQ(w[28], ('5' << 8) | '2');
    EXPECT_EQ(wortString(w, 23, 4), "1.00    ");
    EXPECT_EQ(wortString(w, 10, 10).substr(0, 12), "K1520EMU0001");
    EXPECT_FALSE(p.g.drq());               // nach 256 Woertern fertig
    EXPECT_EQ(p.st() & 0x89, 0);
}

TEST(AtaGeraet, BsyIstMindestensEineAbfrageSichtbarDannDrq) {
    Platte p;
    p.lba(0, 1);
    p.befehl(0x20);
    EXPECT_EQ(p.st(), 0x80);          // erste Abfrage: BSY
    uint8_t s = p.st();
    EXPECT_EQ(s & AtaGeraet::ST_BSY, 0);
    EXPECT_TRUE(s & AtaGeraet::ST_DRQ);
    EXPECT_TRUE(s & AtaGeraet::ST_DRDY);
    EXPECT_TRUE(s & AtaGeraet::ST_DSC);
}

TEST(AtaGeraet, BsyEndetAuchOhneAbfrageNachTakten) {
    Platte p;
    p.lba(0, 1);
    p.g.takt(1000);
    p.befehl(0x20);
    p.g.takt(1000 + 100);
    EXPECT_TRUE(p.g.bsy());
    p.g.takt(1000 + 30000);
    EXPECT_FALSE(p.g.bsy());
    EXPECT_TRUE(p.g.drq());
}

TEST(AtaGeraet, SchreibenUndLesenCHS) {
    Platte p;
    p.chs(2, 3, 5, 1);
    p.befehl(0x30);
    EXPECT_TRUE(p.warte() & AtaGeraet::ST_DRQ);
    p.schreib256(0x1000);
    EXPECT_EQ(p.warte() & 0x89, 0);
    p.chs(2, 3, 5, 1);
    p.befehl(0x20);
    EXPECT_TRUE(p.warte() & AtaGeraet::ST_DRQ);
    const auto w = p.lies256();
    for (int i = 0; i < 256; ++i) EXPECT_EQ(w[i], 0x1000 + i);
    // gleicher Sektor ueber LBA: ((2*16)+3)*63 + 4
    p.lba(35 * 63 + 4, 1);
    p.befehl(0x20);
    p.warte();
    EXPECT_EQ(p.lies256()[7], 0x1007);
}

TEST(AtaGeraet, SchreibenUndLesenLBAErsterUndLetzterSektor) {
    Platte p;
    for (uint32_t l : {0u, static_cast<uint32_t>(SEKTOREN - 1)}) {
        p.lba(l, 1);
        p.befehl(0x30);
        p.warte();
        p.schreib256(static_cast<uint16_t>(l));
        EXPECT_EQ(p.warte() & AtaGeraet::ST_ERR, 0);
    }
    for (uint32_t l : {0u, static_cast<uint32_t>(SEKTOREN - 1)}) {
        p.lba(l, 1);
        p.befehl(0x21);
        p.warte();
        EXPECT_EQ(p.lies256()[0], static_cast<uint16_t>(l));
    }
}

TEST(AtaGeraet, MehrsektorLesenAktualisiertTaskFile) {
    Platte p;
    for (uint32_t l = 10; l < 13; ++l) {
        p.lba(l, 1);
        p.befehl(0x30);
        p.warte();
        p.schreib256(static_cast<uint16_t>(l * 256));
        p.warte();
    }
    p.lba(10, 3);
    p.befehl(0x20);
    p.warte();
    for (uint32_t l = 10; l < 13; ++l) EXPECT_EQ(p.lies256()[0], l * 256) << l;
    EXPECT_EQ(p.warte() & 0x89, 0);
    EXPECT_EQ(p.g.lese(R::SectorNumber), 12);
}

TEST(AtaGeraet, SektorHinterDemEndeGibtIdnf) {
    Platte p;
    p.lba(static_cast<uint32_t>(SEKTOREN), 1);
    p.befehl(0x20);
    uint8_t s = p.warte();
    EXPECT_TRUE(s & AtaGeraet::ST_ERR);
    EXPECT_FALSE(s & AtaGeraet::ST_DRQ);
    EXPECT_EQ(p.g.lese(R::ErrorFeatures), 0x10);
    p.chs(5, 0, 1, 1);                     // Zylinder 5 gibt es nicht
    p.befehl(0x30);
    s = p.warte();
    EXPECT_TRUE(s & AtaGeraet::ST_ERR);
    EXPECT_FALSE(s & AtaGeraet::ST_DRQ);
    EXPECT_EQ(p.g.lese(R::ErrorFeatures), 0x10);
    p.chs(0, 0, 0, 1);                     // Sektor 0 gibt es in CHS nicht
    p.befehl(0x20);
    EXPECT_TRUE(p.warte() & AtaGeraet::ST_ERR);
    p.lba(static_cast<uint32_t>(SEKTOREN - 1), 2);   // laeuft ueber das Ende
    p.befehl(0x41);
    EXPECT_TRUE(p.warte() & AtaGeraet::ST_ERR);
    p.lba(static_cast<uint32_t>(SEKTOREN - 1), 1);
    p.befehl(0x40);
    EXPECT_EQ(p.warte() & AtaGeraet::ST_ERR, 0);
}

TEST(AtaGeraet, UnbekannterBefehlGibtAbrt) {
    Platte p;
    p.befehl(0xA0);
    uint8_t s = p.warte();
    EXPECT_TRUE(s & AtaGeraet::ST_ERR);
    EXPECT_EQ(p.g.lese(R::ErrorFeatures), 0x04);
    p.befehl(0xEF);                        // tolerant: Erfolg, ERR/Error loeschen sich
    EXPECT_EQ(p.warte() & AtaGeraet::ST_ERR, 0);
    EXPECT_EQ(p.g.lese(R::ErrorFeatures), 0);
}

TEST(AtaGeraet, ToleranteBefehle) {
    Platte p;
    for (uint8_t c : {0x10, 0x1F, 0xE0, 0xE1, 0xE7, 0xEF}) {
        p.befehl(c);
        EXPECT_EQ(p.warte() & AtaGeraet::ST_ERR, 0) << int(c);
    }
    p.lba(0, 4);
    p.befehl(0xC6);
    EXPECT_EQ(p.warte() & AtaGeraet::ST_ERR, 0);
    p.lba(0, 1);
    p.befehl(0xC4);
    EXPECT_TRUE(p.warte() & AtaGeraet::ST_DRQ);
    // C4 ohne vorheriges C6 -> ABRT
    Platte q("ata_c4.img");
    q.lba(0, 1);
    q.befehl(0xC4);
    EXPECT_TRUE(q.warte() & AtaGeraet::ST_ERR);
    q.lba(0, 1);
    q.befehl(0x70);
    EXPECT_EQ(q.warte() & AtaGeraet::ST_ERR, 0);
    q.lba(static_cast<uint32_t>(SEKTOREN), 1);
    q.befehl(0x70);
    EXPECT_TRUE(q.warte() & AtaGeraet::ST_ERR);
}

TEST(AtaGeraet, InitParamsAendertDieUebersetzung) {
    Platte p;
    // 91h: 32 Sektoren, 4 Koepfe -> LBA = (c*4+h)*32 + s-1; Zylinder = 5040/128 = 39
    p.g.schreibe(R::SectorCount, 32);
    p.g.schreibe(R::DriveHead, 0xA0 | 3);
    p.befehl(0x91);
    EXPECT_EQ(p.warte() & AtaGeraet::ST_ERR, 0);
    p.chs(1, 0, 1, 1);
    p.befehl(0x30);
    p.warte();
    p.schreib256(0x2000);
    p.warte();
    p.lba(4 * 32, 1);
    p.befehl(0x20);
    p.warte();
    EXPECT_EQ(p.lies256()[0], 0x2000);
    p.befehl(0xEC);
    p.warte();
    const auto w = p.lies256();
    EXPECT_EQ(w[1], 5);                    // physisch unveraendert
    EXPECT_EQ(w[55], 4);
    EXPECT_EQ(w[56], 32);
    EXPECT_EQ(w[54], 39);
    // ungueltig: 0 Sektoren
    p.g.schreibe(R::SectorCount, 0);
    p.befehl(0x91);
    EXPECT_TRUE(p.warte() & AtaGeraet::ST_ERR);
}

TEST(AtaGeraet, SoftResetFolge) {
    Platte p;
    p.lba(77, 3);
    p.befehl(0x20);
    p.warte();
    p.g.schreibe(R::AltStatusDeviceControl, 0x06, true);   // SRST + nIEN
    EXPECT_EQ(p.g.lese(R::AltStatusDeviceControl, true), 0x80);
    EXPECT_EQ(p.st(), 0x80);                   // bleibt BSY, solange SRST gesetzt
    EXPECT_EQ(p.st(), 0x80);
    p.g.schreibe(R::AltStatusDeviceControl, 0x02, true);
    EXPECT_EQ(p.g.lese(R::AltStatusDeviceControl, true), 0x80);   // Diagnose laeuft
    const uint8_t s = p.warte();
    EXPECT_EQ(s & 0xC9, 0x40);
    EXPECT_EQ(p.g.lese(R::ErrorFeatures), 0x01);
    EXPECT_EQ(p.g.lese(R::SectorCount), 1);
    EXPECT_EQ(p.g.lese(R::SectorNumber), 1);
    EXPECT_EQ(p.g.lese(R::CylLow), 0);
    EXPECT_EQ(p.g.lese(R::CylHigh), 0);
    EXPECT_EQ(p.g.lese(R::DriveHead), 0xA0);
    EXPECT_FALSE(p.g.drq());
    // Hardware-Reset und Befehl 90h liefern dieselben Werte
    p.lba(5, 9);
    p.g.reset();
    EXPECT_EQ(p.g.lese(R::SectorCount), 1);
    p.lba(5, 9);
    p.befehl(0x90);
    p.warte();
    EXPECT_EQ(p.g.lese(R::ErrorFeatures), 0x01);
    EXPECT_EQ(p.g.lese(R::SectorCount), 1);
}

TEST(AtaGeraet, SlaveIstNichtVorhanden) {
    Platte p;
    p.g.schreibe(R::DriveHead, 0xB0);      // Bit 4: Slave
    EXPECT_TRUE(p.g.slaveGewaehlt());
    EXPECT_EQ(p.st(), 0x00);
    EXPECT_EQ(p.g.lese(R::AltStatusDeviceControl, true), 0x00);
    p.befehl(0xEC);                        // wird ignoriert
    EXPECT_EQ(p.st(), 0x00);
    EXPECT_FALSE(p.g.drq());
    EXPECT_EQ(p.g.lese(R::Data), 0);
    // zurueck auf Master: wieder bereit, der Befehl hat nichts bewirkt
    p.g.schreibe(R::DriveHead, 0xA0);
    EXPECT_EQ(p.st() & 0xC9, 0x40);
}

TEST(AtaGeraet, DatenregisterOhneDrqLiefertNullUndSchreibtNichts) {
    Platte p;
    EXPECT_EQ(p.g.lese(R::Data), 0);
    p.g.schreibe(R::Data, 0x1234);
    p.lba(0, 1);
    p.befehl(0x20);
    p.warte();
    EXPECT_EQ(p.lies256()[0], 0);          // Nullen, die Schreibversuche wirkten nicht
}

TEST(AtaGeraet, DriveAddressZeigtKopfUndLaufwerk) {
    Platte p;
    p.g.schreibe(R::DriveHead, 0xA5);
    EXPECT_EQ(p.g.lese(R::DriveAddress, true) & 0x3F, ((~5 & 0xF) << 2) | 0x02 | 0x00 /* nDS0 = 0 */);
}

TEST(AtaGeraet, ZweitesOeffnenDesselbenAbbildsScheitertSperrdateiWirdGeloest) {
    Platte p;
    AtaGeraet zweit;
    std::string f;
    EXPECT_FALSE(zweit.abbildOeffnen(p.pfad, {}, &f));
    EXPECT_NE(f.find("gesperrt"), std::string::npos) << f;
    EXPECT_TRUE(std::filesystem::exists(p.pfad + ".lock"));
    p.g.auswerfen();
    EXPECT_FALSE(std::filesystem::exists(p.pfad + ".lock"));
    EXPECT_TRUE(zweit.abbildOeffnen(p.pfad, {}, &f)) << f;
}

TEST(AtaGeraet, VerwaisteSperreEinesToterProzessesWirdUebernommen) {
    Platte p;
    p.g.auswerfen();
    {
        std::ofstream l(p.pfad + ".lock");
        l << "2147483646\n";               // Prozessnummer, die es nicht gibt
    }
    std::string f;
#if defined(_WIN32)
    GTEST_SKIP() << "Prozessabfrage nur unter POSIX";
#endif
    EXPECT_TRUE(p.g.abbildOeffnen(p.pfad, {}, &f)) << f;
}

TEST(AtaGeraet, OeffnenFehlerfaelle) {
    AtaGeraet g;
    std::string f;
    EXPECT_FALSE(g.abbildOeffnen(k1520test::tempPath("k1520_ata_gibtsnicht.img"), {}, &f));
    EXPECT_FALSE(f.empty());
    EXPECT_FALSE(g.mediumVorhanden());
    const std::string krumm = k1520test::tempPath("k1520_ata_krumm.img");
    { std::ofstream o(krumm, std::ios::binary); o << "abc"; }
    EXPECT_FALSE(g.abbildOeffnen(krumm, {}, &f));
    EXPECT_FALSE(std::filesystem::exists(krumm + ".lock"));
    std::filesystem::remove(krumm);
    // Anlegen ueberschreibt nichts
    Platte p;
    EXPECT_FALSE(AtaGeraet::abbildAnlegen(p.pfad, 10, &f));
}

TEST(AtaGeraet, OhneMediumGibtDatenbefehlAbrtAberIdentifyGeht) {
    AtaGeraet g;
    g.schreibe(R::SectorCount, 1);
    g.schreibe(R::StatusCommand, 0x20);
    for (int i = 0; i < 3 && (g.lese(R::StatusCommand) & 0x80); ++i) {}
    EXPECT_EQ(g.lese(R::ErrorFeatures), 0x04);
    EXPECT_TRUE(g.status() & AtaGeraet::ST_ERR);
}

TEST(AtaGeraet, DatenBleibenNachFlushUndNeuOeffnenErhalten) {
    Platte p;
    p.lba(100, 1);
    p.befehl(0x30);
    p.warte();
    p.schreib256(0xA000);
    p.warte();
    EXPECT_TRUE(p.g.schmutzig());
    // noch nichts in der Datei
    {
        std::ifstream i(p.pfad, std::ios::binary);
        i.seekg(100 * 512);
        char b[2];
        i.read(b, 2);
        EXPECT_EQ(b[0], 0);
        EXPECT_EQ(b[1], 0);
    }
    EXPECT_TRUE(p.g.flush());
    EXPECT_FALSE(p.g.schmutzig());
    {
        std::ifstream i(p.pfad, std::ios::binary);
        i.seekg(100 * 512);
        unsigned char b[4];
        i.read(reinterpret_cast<char*>(b), 4);
        EXPECT_EQ(b[0], 0x00);
        EXPECT_EQ(b[1], 0xA0);   // Low-Byte zuerst
        EXPECT_EQ(b[2], 0x01);
        EXPECT_EQ(b[3], 0xA0);
        EXPECT_EQ(std::filesystem::file_size(p.pfad), SEKTOREN * 512);
    }
    p.g.auswerfen();
    std::string f;
    ASSERT_TRUE(p.g.abbildOeffnen(p.pfad, {}, &f)) << f;
    p.lba(100, 1);
    p.befehl(0x20);
    p.warte();
    EXPECT_EQ(p.lies256()[255], 0xA0FF);
}

TEST(AtaGeraet, VerzoegertesZurueckschreibenNachSchreibpause) {
    Platte p;
    p.g.takt(10);
    p.lba(3, 1);
    p.befehl(0x30);
    p.warte();
    p.schreib256(0x4242);
    p.warte();
    p.g.takt(100);
    EXPECT_TRUE(p.g.schmutzig());          // Pause noch nicht um
    p.g.takt(10 + 2'000'000 + 1);
    EXPECT_FALSE(p.g.schmutzig());         // Auftrag abgegeben
    p.g.flush();                           // wartet auf den Faden
    std::ifstream i(p.pfad, std::ios::binary);
    i.seekg(3 * 512);
    unsigned char b[2];
    i.read(reinterpret_cast<char*>(b), 2);
    EXPECT_EQ(b[0], 0x42);
}

TEST(AtaGeraet, AuswerfenSchreibtSofortZurueck) {
    Platte p;
    p.lba(9, 1);
    p.befehl(0x30);
    p.warte();
    p.schreib256(0x0707);
    p.g.auswerfen();
    EXPECT_FALSE(p.g.mediumVorhanden());
    std::ifstream i(p.pfad, std::ios::binary);
    i.seekg(9 * 512);
    unsigned char b[2];
    i.read(reinterpret_cast<char*>(b), 2);
    EXPECT_EQ(b[0], 0x07);
}

TEST(AtaGeraet, GzipRundlauf) {
    Platte p("ata_rund.img.gz", 2016);
    EXPECT_LT(std::filesystem::file_size(p.pfad), 2016u * 512 / 4);   // gepackt
    EXPECT_EQ(p.g.sektorenGesamt(), 2016u);
    p.lba(2015, 1);
    p.befehl(0x30);
    p.warte();
    p.schreib256(0x3C00);
    p.lba(0, 1);
    p.befehl(0x30);
    p.warte();
    p.schreib256(0x0001);
    p.g.auswerfen();
    {
        std::ifstream i(p.pfad, std::ios::binary);
        char m[2];
        i.read(m, 2);
        EXPECT_EQ(static_cast<unsigned char>(m[0]), 0x1F);
        EXPECT_EQ(static_cast<unsigned char>(m[1]), 0x8B);
    }
    std::string f;
    ASSERT_TRUE(p.g.abbildOeffnen(p.pfad, {}, &f)) << f;
    p.lba(2015, 1);
    p.befehl(0x20);
    p.warte();
    auto w = p.lies256();
    for (int i = 0; i < 256; ++i) ASSERT_EQ(w[i], 0x3C00 + i);
    p.lba(0, 1);
    p.befehl(0x20);
    p.warte();
    EXPECT_EQ(p.lies256()[5], 6);
    p.lba(1000, 1);
    p.befehl(0x20);
    p.warte();
    EXPECT_EQ(p.lies256()[5], 0);
    p.g.flush();
    // Geometrie fuer kleines Abbild: 2016 = 2 Zylinder x 16 x 63
    EXPECT_EQ(p.g.geometrie().zylinder, 2u);
}

TEST(AtaGeraet, GeometrieAusGroesseUndKonfiguration) {
    EXPECT_EQ(AtaGeraet::geometrieAbleiten(100).koepfe, 4u);
    EXPECT_EQ(AtaGeraet::geometrieAbleiten(100).sektoren, 16u);
    EXPECT_EQ(AtaGeraet::geometrieAbleiten(100).zylinder, 1u);
    EXPECT_EQ(AtaGeraet::geometrieAbleiten(500'000).zylinder, 496u);
    EXPECT_EQ(AtaGeraet::geometrieAbleiten(100'000'000).zylinder, 16383u);
    Platte p("ata_geo.img", 1000);
    p.g.auswerfen();
    std::string f;
    ASSERT_TRUE(p.g.abbildOeffnen(p.pfad, {10, 4, 25}, &f)) << f;
    p.befehl(0xEC);
    p.warte();
    const auto w = p.lies256();
    EXPECT_EQ(w[1], 10);
    EXPECT_EQ(w[3], 4);
    EXPECT_EQ(w[6], 25);
    EXPECT_EQ(w[60], 1000);
}
