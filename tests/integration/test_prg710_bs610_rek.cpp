/**
 * @file test_prg710_bs610_rek.cpp
 * @brief "BS610-Rekonstruktion" am PRG 710 (Entwurf 48, AP-B8 Schritt 3): BS620 mit dem Tastaturteil von BS600 V02
 *        (K7609 am 8279 statt K7672 an der K8025-SIO), gebaut von `tools/bs600/bau_disketten.py`
 *        (`disks/prg710_bs610_rekonstruktion/prg710_bs610_rek.hfe`, EIGENBAU, kein Original; doc/bs600/tastatur.md).
 *
 * Maschine: `prg710` in der Kernvorgabe (2 x K5601, ATP-Zusatz, Leser-Modell nach Variante = 0).  Tasten der K7609:
 * ET1 (37H) startet das ROM, ET2 (38H) = ENTER (84H), CL (28H) = 83H, Kursor 39H/3AH/3BH/19H (rechts/links/runter/hoch).
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/filesystem/bs600/bs600_fs.h"
#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/prg710/prg710.h"
#include "tests/support/fixtures.h"

namespace {
using V = Prg710Machine::Config::Variante;
const char* const kRek = "../../../disks/prg710_bs610_rekonstruktion/prg710_bs610_rek.hfe";

constexpr uint32_t K(uint32_t code) { return 0x03000000u | code; }   // physische Taste der K7609
constexpr uint32_t T_ET1 = K(0x37), T_ENTER = K(0x38), T_CL = K(0x28), T_DOWN = K(0x3B), T_UP = K(0x19);

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

void taste(Prg710Machine& m, uint32_t code, long long warte) {
    m.keyPress(code, false, false);
    m.run(150'000);
    m.keyRelease(code);
    m.run(warte);
}

bool warteAuf(Prg710Machine& m, const char* text, long long max = 6'000'000'000) {
    for (long long g = 0; g < max; g += 2'000'000) {
        if (bildText(m).find(text) != std::string::npos) { m.run(8'000'000); return true; }
        m.run(2'000'000);
    }
    return false;
}

void tippe(Prg710Machine& m, const std::string& t) { for (char ch : t) taste(m, static_cast<uint32_t>(ch), 3'000'000); }

std::string markierte(Prg710Machine& m) {
    const std::string t = bildText(m);
    size_t k = t.find(" <");
    if (k == std::string::npos) return "";
    size_t a = t.rfind('\n', k) + 1;
    std::string r = t.substr(a, t.find('\n', k) - a);
    return r.substr(r.find_first_not_of(' '));
}

Prg710Machine::Config kernvorgabe710() {
    Prg710Machine::Config c;
    c.variante = V::Prg710;                 // sonst Kernvorgabe: 2 x K5601, ATP-Zusatz, Leser-Modell -1 (= 0 am 710)
    return c;
}

/// Boot bis zum Hauptmenue; jeder Selbsttestfehlertext bricht ab (@p fehler bekommt ihn).
bool bootRek(Prg710Machine& m, const std::string& pfad, std::string* fehler = nullptr) {
    m.powerOn();
    if (!m.mountDisk(0, pfad, m.defaultFormatName(0), false)) return false;
    long long d = 0;
    while (d < 60'000'000 && bildText(m).find("NKM-LOADER") == std::string::npos) d += m.run(20'000);
    m.run(2'000'000);
    taste(m, T_ET1, 1'000'000);
    const char* texte[] = {"RAM-ZELLE", "PIO ZE PORT", "CTC ZE KANAL", "CTC ATP KANAL", "SIO  KANAL", "PRUEFSUMME"};
    for (d = 0; d < 400'000'000; d += m.run(100'000)) {
        const std::string b = bildText(m);
        for (const char* f : texte)
            if (b.find(f) != std::string::npos) { if (fehler) *fehler = f; return false; }
        if (b.find("DIENSTPROGRAMME") != std::string::npos) { m.run(3'000'000); return true; }
    }
    return false;
}

const FormatCatalog& formate() {
    static FormatCatalog c = [] { std::string f; return FormatCatalog::loadDefault(&f); }();
    return c;
}
const FsCatalog& dateisysteme() {
    static FsCatalog c = [] { std::string f; return FsCatalog::loadDefault(formate(), &f); }();
    return c;
}
}  // namespace

// Boot vom K5601 (710-ROM, Doppelschritt), Selbsttest ohne Fehlertext, Hauptmenue VERSION 04; Kursor ab/auf mit den
// K7609-Kursortasten, ENTER (ET2) fuehrt ins Unterbild BETRIEBSARTEN (Ueberlagerung von der Diskette).
TEST(Prg710Bs610Rek, BootSelbsttestMenueUndKursor) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie(kRek, "bs610rek_menue.hfe");
    Prg710Machine m(kernvorgabe710());
    std::string fehler;
    ASSERT_TRUE(bootRek(m, kopie.path(), &fehler)) << "Selbsttest: " << fehler << " PC=" << std::hex << m.cpuPC() << "\n" << bildText(m);
    EXPECT_NE(bildText(m).find("VERSION: 04"), std::string::npos) << bildText(m);
    const char* eintraege[] = {"PROG/TEST PC600    : <", "DIENSTPROGRAMME    : <", "PROG PMC600/CNC600 : <",
                               "PROG PMC600/CNC-H  : <", "PROG PMC600/CNC-H  : <"};
    EXPECT_EQ(markierte(m), eintraege[0]);
    for (int i = 1; i <= 4; ++i) {
        taste(m, T_DOWN, 3'000'000);
        EXPECT_EQ(markierte(m), eintraege[i]) << "nach " << i << " x abwaerts\n" << bildText(m);
    }
    taste(m, T_UP, 3'000'000);
    taste(m, T_UP, 3'000'000);
    taste(m, T_UP, 3'000'000);
    EXPECT_EQ(markierte(m), eintraege[0]);
    taste(m, T_ENTER, 80'000'000);
    const std::string b = bildText(m);
    EXPECT_NE(b.find("BETRIEBSARTEN"), std::string::npos) << b;
    EXPECT_EQ(markierte(m), "ANZEIGE       : <") << b;
}

// CL quittiert eine Fehlermeldung: DIENSTPROGRAMME, N = Laufwerk 2 (leer) -> blinkende Meldung LAUFWERK 2, CL -> Hauptmenue.
TEST(Prg710Bs610Rek, ClQuittiertLeeresLaufwerk) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie(kRek, "bs610rek_cl.hfe");
    Prg710Machine m(kernvorgabe710());
    ASSERT_TRUE(bootRek(m, kopie.path())) << bildText(m);
    taste(m, T_DOWN, 3'000'000);
    taste(m, T_ENTER, 100'000'000);
    EXPECT_NE(bildText(m).find("LAUFWERK 1  J"), std::string::npos) << bildText(m);
    taste(m, 'N', 2'000'000);
    taste(m, T_ENTER, 150'000'000);
    EXPECT_NE(bildText(m).find("LAUFWERK 2"), std::string::npos) << bildText(m);
    taste(m, T_CL, 50'000'000);
    EXPECT_NE(bildText(m).find("VERSION: 04"), std::string::npos) << bildText(m);
    EXPECT_NE(bildText(m).find("DIENSTPROGRAMME    :"), std::string::npos) << bildText(m);
}

// DIENSTPROGRAMME -> LIST (Geraet BS): der Plattentreiber von BS620 liest am 710 (Leser-Modell 0) alle Spuren.
TEST(Prg710Bs610Rek, ListZeigt28Dateien) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie(kRek, "bs610rek_list.hfe");
    Prg710Machine m(kernvorgabe710());
    ASSERT_TRUE(bootRek(m, kopie.path())) << bildText(m);
    taste(m, T_DOWN, 3'000'000);
    taste(m, T_ENTER, 100'000'000);
    taste(m, 'J', 2'000'000);
    taste(m, T_ENTER, 100'000'000);
    ASSERT_TRUE(warteAuf(m, "DISK-NAME :", 3'000'000'000)) << bildText(m);
    taste(m, T_ENTER, 300'000'000);
    ASSERT_TRUE(warteAuf(m, "COPYDISK", 600'000'000)) << bildText(m);
    taste(m, T_ENTER, 300'000'000);                // LIST
    taste(m, T_ENTER, 600'000'000);                // DRUCKROUTINE
    taste(m, T_ENTER, 600'000'000);                // DISK EINLEGEN
    taste(m, 'J', 2'000'000);
    taste(m, T_ENTER, 1'500'000'000);
    taste(m, T_DOWN, 20'000'000);                  // GERAET: BS
    taste(m, T_ENTER, 3'000'000'000);
    ASSERT_TRUE(warteAuf(m, "DISK ENTHAELT 28 VON 54", 1'500'000'000)) << "PC=" << std::hex << m.cpuPC() << "\n" << bildText(m);
    taste(m, T_DOWN, 1'500'000'000);
    const std::string liste = bildText(m);
    for (const char* e : {"SYSVSP  220983  HOF  P  01/01  01/01", "SYSNLD  081288  KRU  P  01/02  01/03"})
        EXPECT_NE(liste.find(e), std::string::npos) << e << "\n" << liste;
}

// COPY SYSVSP -> KOPIE1 auf derselben Diskette (Einzellaufwerk, STECKE QUELLDISK/ZIELDISK je mit ENTER).  Danach liest das
// DiskTool 29 Dateien, KOPIE1 = SYSVSP byteweise, Pruefung ohne Befund.
TEST(Prg710Bs610Rek, CopyKopiertEineDatei) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie(kRek, "bs610rek_copy.hfe");
    {
        Prg710Machine m(kernvorgabe710());
        ASSERT_TRUE(bootRek(m, kopie.path())) << bildText(m);
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
        m.run(100'000'000);                           // Schreibpause des Mediums (autoFlush)
    }
    std::string err;
    auto v = DiskVolume::open(kopie.path(), "", formate(), dateisysteme(), err, true);
    ASSERT_TRUE(v) << err;
    ASSERT_EQ(v->fsType(), FsType::Bs600);
    EXPECT_EQ(v->list().size(), 29u);
    std::vector<uint8_t> a, b;
    ASSERT_TRUE(v->bs600Fs()->read("SYSVSP", a));
    ASSERT_TRUE(v->bs600Fs()->read("KOPIE1", b));
    EXPECT_EQ(a, b);
    EXPECT_EQ(v->check(FsCheckLevel::Schnell, true).findings.size(), 0u);
}
