/**
 * @file test_prg710_bs6xx.cpp
 * @brief BS600/BS620 am PRG 710 und 710-1 (Entwurf 48, AP-B2d): je Geraet zwei Disketten (`_ff_low`, `_ff_high`,
 *        `tools/bs600/bau_disketten.py`), jede gegen die drei Leser-Modelle der K5122 (`Config::leser_modell` 0/1/2).
 *
 * Gemessen wird, wie weit die Diskette kommt: Boot bis zum Hauptmenue, Dienstprogramme lesen den Datentraeger
 * (`DISK-NAME`), LIST zeigt die Dateizahl.  Die Tabelle in `Prg710Bs6xxDisketten.Tabelle` ist der festgeschriebene
 * Befund des MODELLS, nicht des Geraets (doc/bs600/marken_ff.md, doc/bs600/geraetetest.md).
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "core/filesystem/bs600/bs600_fs.h"
#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/prg710/prg710.h"
#include "tests/support/fixtures.h"
#include "tests/support/temp_path.h"

namespace {
using V = Prg710Machine::Config::Variante;

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

/// Tastenbelegung je Geraet: 710 = K7609 am 8279 (Matrixtasten, BS600-ENTER = ET2 38H), 710-1 = K7672 (Return, Kursor als Matrixtaste).
struct Tasten {
    V v;
    uint32_t start() const { return v == V::Prg710 ? (0x03000000u | 0x37u) : 0x01000004u; }   // ROM: ET1 bzw. ENTER
    uint32_t enter() const { return v == V::Prg710 ? (0x03000000u | 0x38u) : 0x01000004u; }
    uint32_t down()  const { return v == V::Prg710 ? 0x01000015u : (0x03000000u | 0x18u); }
    uint32_t up()    const { return v == V::Prg710 ? 0x01000013u : (0x03000000u | 0x79u); }
};

void druecke(Prg710Machine& m, uint32_t code, long long warte) {
    m.keyPress(code, false, false);
    m.run(150'000);
    m.keyRelease(code);
    m.run(warte);
}

bool warteAuf(Prg710Machine& m, const char* text, long long max) {
    for (long long g = 0; g < max; g += 2'000'000) {
        if (bildText(m).find(text) != std::string::npos) { m.run(8'000'000); return true; }
        m.run(2'000'000);
    }
    return false;
}

enum class Stufe { Boot = 0, Menue = 1, DiskName = 2, Liste = 3 };
const char* stufeName(Stufe s) {
    switch (s) {
        case Stufe::Boot:     return "Boot/Selbsttest fehlt";
        case Stufe::Menue:    return "Hauptmenue";
        case Stufe::DiskName: return "Disk-Name gelesen";
        case Stufe::Liste:    return "LIST ok";
    }
    return "?";
}

struct Ergebnis { Stufe stufe; uint16_t pc; std::string bild; };

/// Eine Diskette (Kopie) gegen das Modell: Boot, DIENSTPROGRAMME, LIST (Geraet BS).
Ergebnis fahrePfad(V v, const std::string& pfad, int modell, const char* listeText) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    struct { const std::string& p; const std::string& path() const { return p; } } kopie{pfad};
    Prg710Machine::Config c;
    c.variante = v;
    c.atp_zusatz = true;
    c.leser_modell = modell;
    Prg710Machine m(c);
    const Tasten t{v};
    auto ende = [&](Stufe s) { return Ergebnis{s, m.cpuPC(), bildText(m)}; };
    m.powerOn();
    if (!m.mountDisk(0, kopie.path(), m.defaultFormatName(0), false)) return ende(Stufe::Boot);
    long long d = 0;
    while (d < 60'000'000 && bildText(m).find("NKM-LOADER") == std::string::npos) d += m.run(20'000);
    m.run(2'000'000);
    druecke(m, t.start(), 1'000'000);
    if (!warteAuf(m, "DIENSTPROGRAMME", 400'000'000)) return ende(Stufe::Boot);
    m.run(3'000'000);
    druecke(m, t.down(), 3'000'000);
    druecke(m, t.enter(), 100'000'000);
    druecke(m, 'J', 2'000'000);
    druecke(m, t.enter(), 100'000'000);
    if (!warteAuf(m, "DISK-NAME :", 3'000'000'000)) return ende(Stufe::Menue);
    druecke(m, t.enter(), 100'000'000);
    if (!warteAuf(m, "LIST", 600'000'000)) return ende(Stufe::DiskName);
    druecke(m, t.enter(), 300'000'000);
    druecke(m, t.enter(), 600'000'000);
    druecke(m, t.enter(), 600'000'000);
    druecke(m, 'J', 2'000'000);
    druecke(m, t.enter(), 1'500'000'000);
    druecke(m, t.down(), 20'000'000);
    druecke(m, t.enter(), 3'000'000'000);
    if (!warteAuf(m, listeText, 1'500'000'000)) return ende(Stufe::DiskName);
    return ende(Stufe::Liste);
}

Ergebnis fahre(V v, const std::string& fixture, int modell, const char* listeText) {
    k1520test::TempDisk kopie(fixture, "bs6xx_tabelle.hfe");
    return fahrePfad(v, kopie.path(), modell, listeText);
}

struct Fall { const char* name; V v; const char* fixture; const char* liste; };
const Fall kFaelle[] = {
    {"710-1 BS620 low  (Original)", V::Prg710_1, "../bs600/bs620_prg710-1.hfe",  "DISK ENTHAELT 28 VON 54"},
    {"710-1 BS620 high (gepatcht)", V::Prg710_1, "../bs600/prg710-1_bs620_ff_high.hfe", "DISK ENTHAELT 28 VON 54"},
    {"710   BS600 low  (Eigenbau-Urlader)", V::Prg710, "../bs600/prg710_bs600_ff_low.hfe",  "DISK ENTHAELT 25 VON 54"},
    {"710   BS600 high (gepatcht)", V::Prg710, "../bs600/prg710_bs600_ff_high.hfe", "DISK ENTHAELT 25 VON 54"},
};
}  // namespace

// Festgeschriebener Befund des Modells (Stufe je Diskette und Leser-Modell).  Stufen unter LIST sind
// keine Fehler des Tests, sondern das Ergebnis der Hypothesenpruefung (marken_ff.md).  `Disk-Name gelesen` am 710: die
// BS600-Datei fuehrt Einfachschritt aus (Test unten) und scheitert am Nachladen von Spur >= 1 (`FC 06`).
TEST(Prg710Bs6xxDisketten, Tabelle) {
    // [Fall][Leser-Modell 0 bisher | 1 einheitlich | 2 einheitlich + Sofortmarke]
    const Stufe erwartet[4][3] = {
        {Stufe::Menue,    Stufe::Liste, Stufe::Liste},     // 710-1 low:  bisher haengt der Treiber in 1312H
        {Stufe::Liste,    Stufe::Liste, Stufe::DiskName},  // 710-1 high: Modell 2 haengt nach dem Kennsatz
        {Stufe::DiskName, Stufe::Boot,  Stufe::DiskName},  // 710 low:    FC 06 nach dem Kennsatz (BS600 = Einfachschritt, s. u.)
        {Stufe::Menue,    Stufe::Boot,  Stufe::DiskName},  // 710 high
    };
    printf("\n%-38s | %-20s | %-20s | %-20s\n", "Diskette", "0 bisher", "1 einheitlich", "2 einh.+Sofortmarke");
    for (int i = 0; i < 4; ++i) {
        Ergebnis e[3];
        for (int mo = 0; mo < 3; ++mo) e[mo] = fahre(kFaelle[i].v, kFaelle[i].fixture, mo, kFaelle[i].liste);
        printf("%-38s | %-20s | %-20s | %-20s\n", kFaelle[i].name, stufeName(e[0].stufe), stufeName(e[1].stufe), stufeName(e[2].stufe));
        for (int mo = 0; mo < 3; ++mo)
            EXPECT_EQ(static_cast<int>(e[mo].stufe), static_cast<int>(erwartet[i][mo]))
                << kFaelle[i].name << " Modell " << mo << " PC=" << std::hex << e[mo].pc << "\n" << e[mo].bild;
    }
}

namespace {
/// BS600 (PRG-710-Weg) bis zum Hauptmenue (Eigenbau-Urlader, low).  Maschine bleibt am Menue stehen.
bool bootBs600(Prg710Machine& m, const std::string& pfad) {
    m.powerOn();
    if (!m.mountDisk(0, pfad, m.defaultFormatName(0), false)) return false;
    long long d = 0;
    while (d < 60'000'000 && bildText(m).find("NKM-LOADER") == std::string::npos) d += m.run(20'000);
    m.run(2'000'000);
    druecke(m, 0x03000000u | 0x37u, 1'000'000);
    if (!warteAuf(m, "DIENSTPROGRAMME", 400'000'000)) return false;
    m.run(3'000'000);
    return true;
}
}  // namespace

// Tastenbelegung der BS600 am PRG 710 (K7609): die Tabelle bei 02B3H (Index = 8279-Code, `LD HL,02B3H / ADD HL,BC`) liefert
// fuer 37H (ET1, ROM-Starttaste) 1EH (kein ENTER), fuer 38H (ET2) 84H = ENTER.  Am Lauf bestaetigt; 39H-3BH sind Kursor.
TEST(Prg710Bs600, EnterIstMatrixtaste38) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie("../bs600/prg710_bs600_ff_low.hfe", "bs6xx_t1.hfe");
    Prg710Machine::Config c; c.variante = V::Prg710; c.atp_zusatz = true; c.leser_modell = 2;
    Prg710Machine m(c);
    ASSERT_TRUE(bootBs600(m, kopie.path())) << bildText(m);
    EXPECT_NE(bildText(m).find("VERSION: 02"), std::string::npos);
    druecke(m, 0x03000000u | 0x37u, 60'000'000);
    EXPECT_EQ(bildText(m).find("BETRIEBSARTEN"), std::string::npos) << "ET1 (37H) ist in BS600 kein ENTER";
    druecke(m, 0x03000000u | 0x28u, 60'000'000);
    EXPECT_EQ(bildText(m).find("BETRIEBSARTEN"), std::string::npos) << "CL (28H) ist kein ENTER";
    druecke(m, 0x03000000u | 0x38u, 60'000'000);
    EXPECT_NE(bildText(m).find("BETRIEBSARTEN"), std::string::npos) << "ET2 (38H) = ENTER (84H)\n" << bildText(m);
}

// BEFUND (AP-B2d): die BS600-Datei der PRG-700-Diskette macht EINEN Schrittimpuls je Spur (Spur 1 = 1 Impuls; BS620: 2).
// Am K5601 (96 tpi, 40-Spur-Diskette auf geraden Zylindern) liest sie deshalb nur Spur 0: der Datentraeger-Kennsatz
// (`DISK-NAME`) kommt, das Nachladen des Menues von Spur >= 1 scheitert mit `FC 06`.  Dasselbe Verhalten ergaebe ein echtes
// K5601; auf einem 40-Spur-Laufwerk (K5600.x) wuerde dagegen der Doppelschritt des 710-ROMs (Boot) scheitern.
TEST(Prg710Bs600, LiestAmK5601NurSpur0FC06NachDemKennsatz) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie("../bs600/prg710_bs600_ff_low.hfe", "bs6xx_t2.hfe");
    Prg710Machine::Config c; c.variante = V::Prg710; c.atp_zusatz = true; c.leser_modell = 2;
    Prg710Machine m(c);
    ASSERT_TRUE(bootBs600(m, kopie.path())) << bildText(m);
    uint8_t prev = 0xFF; int schritte = 0, innen = 0;
    m.setBusTrace([&](bool isIO, bool rd, uint16_t a, uint8_t d) {
        if (!isIO || rd || (a & 0xFF) != 0x10) return;
        if ((prev & 0x80) && !(d & 0x80)) { ++schritte; if (d & 0x20) ++innen; }
        prev = d;
    });
    druecke(m, 0x01000015u, 3'000'000);                  // Kursor ab
    druecke(m, 0x03000000u | 0x38u, 100'000'000);        // ENTER: DIENSTPROGRAMME
    druecke(m, 'J', 2'000'000);
    druecke(m, 0x03000000u | 0x38u, 100'000'000);
    ASSERT_TRUE(warteAuf(m, "DISK-NAME :", 3'000'000'000)) << bildText(m);
    druecke(m, 0x03000000u | 0x38u, 300'000'000);
    EXPECT_NE(bildText(m).find("FC 06"), std::string::npos) << bildText(m);
    EXPECT_LE(innen, 3) << "wenige Schritte nach innen (Einfachschritt): " << innen << " von " << schritte;
}

// AP-B8 Schritt 1 (Gegenprobe zur Einfachschritt-Erklaerung, BEFUND: bestaetigt): Laufwerk 0 = K5601 (das 710-ROM bootet die
// Diskette mit Doppelschritt), Laufwerk 1 = K5600.10 (40 Spuren, 48 tpi: Spur n = Zylinder n, kein TrackPitch) mit einer Kopie
// derselben Diskette.  Unter DIENSTPROGRAMME waehlt 'N' Laufwerk 2 (= K5122-Laufwerk 1): Menue-Ueberlagerungen und LIST kommen
// von dort, BS600 V02 liest mit Einfachschritt alle Spuren und zeigt 25 Dateien (am K5601 dagegen FC 06, Test oben).
// Leser-Modell = Kernvorgabe des 710 (0).
TEST(Prg710Bs600, ZweitesLaufwerkK5600_10ListetAlleDateien) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk sys("../bs600/prg710_bs600_ff_low.hfe", "bs6xx_lw0.hfe");
    k1520test::TempDisk dat("../bs600/prg710_bs600_ff_low.hfe", "bs6xx_lw1.hfe");
    Prg710Machine::Config c; c.variante = V::Prg710; c.atp_zusatz = true;
    c.laufwerke = {"K5601", "K5600.10", "none", "none"};
    Prg710Machine m(c);
    ASSERT_TRUE(m.mountDisk(1, dat.path(), m.defaultFormatName(1), false)) << m.lastError();
    ASSERT_TRUE(bootBs600(m, sys.path())) << bildText(m);
    const uint32_t enter = 0x03000000u | 0x38u;
    druecke(m, 0x01000015u, 3'000'000);                  // Kursor ab: DIENSTPROGRAMME
    druecke(m, enter, 100'000'000);
    druecke(m, 'N', 2'000'000);                          // LAUFWERK 1  J -> N = Laufwerk 2
    druecke(m, enter, 100'000'000);
    ASSERT_TRUE(warteAuf(m, "DISK-NAME :", 3'000'000'000)) << bildText(m);
    EXPECT_NE(bildText(m).find("LAUFWERK 2"), std::string::npos) << bildText(m);
    druecke(m, enter, 300'000'000);                      // Menue der Dienstprogramme (Ueberlagerung von Spur >= 1)
    ASSERT_TRUE(warteAuf(m, "COPYDISK", 600'000'000)) << bildText(m);
    EXPECT_EQ(bildText(m).find("FC 06"), std::string::npos) << bildText(m);
    druecke(m, enter, 300'000'000);                      // LIST
    druecke(m, enter, 600'000'000);                      // DRUCKROUTINE
    druecke(m, enter, 600'000'000);                      // DISK EINLEGEN
    druecke(m, 'N', 2'000'000);                          // LAUFWERK 1  J -> N
    druecke(m, enter, 1'500'000'000);
    druecke(m, 0x01000015u, 20'000'000);                 // GERAET: BS
    druecke(m, enter, 3'000'000'000);
    EXPECT_TRUE(warteAuf(m, "DISK ENTHAELT 25 VON 54", 1'500'000'000)) << "PC=" << std::hex << m.cpuPC() << "\n" << bildText(m);
}

// Gegenprobe: dieselben Leser-Modelle mit den GERAETEdisketten (UDOS) - das 710-1 bootet UDOS nur mit Modell 0 und 1, das 710 nur
// mit 0 und 2.  Ein einziges Modell fuer beide Geraete gibt es damit NICHT (marken_ff.md, Abschnitt 8).
TEST(Prg710Bs6xxDisketten, UdosBootetJeLeserModell) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    struct F { V v; const char* disk; int modell; bool erwartet; };
    const F faelle[] = {
        {V::Prg710_1, "prg710-1_udos43_k5601_v43_189.hfe", 0, true},
        {V::Prg710_1, "prg710-1_udos43_k5601_v43_189.hfe", 1, true},
        {V::Prg710_1, "prg710-1_udos43_k5601_v43_189.hfe", 2, false},
        {V::Prg710,   "prg710_udos43_k5601_mrs_boot.hfe",  0, true},
        {V::Prg710,   "prg710_udos43_k5601_mrs_boot.hfe",  1, false},
        {V::Prg710,   "prg710_udos43_k5601_mrs_boot.hfe",  2, true},
    };
    for (const F& f : faelle) {
        k1520test::TempDisk d(f.disk, "bs6xx_udos.hfe");
        Prg710Machine::Config c; c.variante = f.v; c.atp_zusatz = true; c.leser_modell = f.modell;
        Prg710Machine m(c);
        ASSERT_TRUE(m.mountDisk(0, d.path(), m.defaultFormatName(0), false)) << m.lastError();
        m.powerOn();
        m.run(3'000'000);
        druecke(m, 0x01000004u, 1'000'000);   // Return: 710-1 ENTER, 710 ET1 (37H)
        const bool ok = warteAuf(m, "Neues Datum", 150'000'000);
        EXPECT_EQ(ok, f.erwartet) << f.disk << " Modell " << f.modell << " PC=" << std::hex << m.cpuPC() << "\n" << bildText(m);
    }
}

namespace {
const FormatCatalog& formate() {
    static FormatCatalog c = [] { std::string f; return FormatCatalog::loadDefault(&f); }();
    return c;
}
const FsCatalog& dateisysteme() {
    static FsCatalog c = [] { std::string f; return FsCatalog::loadDefault(formate(), &f); }();
    return c;
}
}  // namespace

// Gegenprobe zum DiskTool (AP-B2e): das ORIGINAL (BS620 am 710-1) liest eine vom DiskTool beschriebene Diskette.
// Dem Original-Abzug wird per `put` eine Datei hinzugefuegt (Verzeichnisplatz in der Reihenfolge des Originals, Belegung
// Spur 26/Sektor 7 wie bei COPY); BS620 zeigt `LIST` mit 29 Dateien, das DiskTool prueft ohne Befund.
TEST(Prg710Bs6xxDisketten, VomDiskToolErgaenzteDisketteLiestDasOriginal) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk kopie("../bs600/bs620_prg710-1.hfe", "bs6xx_put.hfe");
    {
        std::string err;
        auto v = DiskVolume::open(kopie.path(), "", formate(), dateisysteme(), err, /*read_only=*/false);
        ASSERT_TRUE(v) << err;
        ASSERT_EQ(v->fsType(), FsType::Bs600);
        WriteOptions wo;
        wo.bs600_type = "L"; wo.bs600_date = "051125"; wo.bs600_author = "TST"; wo.bs600_load = 0x2900;
        std::vector<uint8_t> d(300, 0x41);
        ASSERT_TRUE(v->bs600Fs()->write("DISKTL", d, wo)) << v->bs600Fs()->lastError();
        ASSERT_TRUE(v->flush()) << v->lastError();
        const FsCheckReport& r = v->check(FsCheckLevel::Schnell, true);
        EXPECT_EQ(r.findings.size(), 0u);
        EXPECT_EQ(v->list().size(), 29u);
    }
    const Ergebnis e = fahrePfad(V::Prg710_1, kopie.path(), -1, "DISK ENTHAELT 29 VON 54");
    EXPECT_EQ(static_cast<int>(e.stufe), static_cast<int>(Stufe::Liste)) << e.bild;
}

// Eine VOLLSTAENDIG vom DiskTool gebaute Diskette (create --boot-Urlader + alle 28 Dateien des Originals mit ihren Angaben)
// bootet am 710-1 bis zum Hauptmenue und zeigt LIST mit 28 Dateien.
TEST(Prg710Bs6xxDisketten, VomDiskToolNeuGebauteDisketteBootet) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk orig("../bs600/bs620_prg710-1.hfe", "bs6xx_orig.hfe");
    const std::string ziel = k1520test::tempPath("bs6xx_neu.hfe");
    std::filesystem::remove(ziel);
    {
        std::string err;
        auto o = DiskVolume::open(orig.path(), "", formate(), dateisysteme(), err, true);
        ASSERT_TRUE(o) << err;
        std::vector<uint8_t> boot;
        ASSERT_TRUE(o->readBootImage(boot));
        auto n = DiskVolume::create(ziel, "bs600_ss40_15x256_boot", "BS600", formate(), dateisysteme(), err);
        ASSERT_TRUE(n) << err;
        ASSERT_TRUE(n->writeBootImage(boot)) << n->lastError();
        for (const auto& e : o->bs600Fs()->eintraege()) {
            std::vector<uint8_t> d;
            ASSERT_TRUE(o->bs600Fs()->read(e.name, d)) << e.name;
            WriteOptions wo;
            wo.bs600_type = std::string(1, e.typ);
            wo.bs600_date = e.datum; wo.bs600_author = e.signum;
            wo.bs600_load = static_cast<uint16_t>(e.ladeadr); wo.bs600_entry = static_cast<uint16_t>(e.startadr);
            ASSERT_TRUE(n->bs600Fs()->write(e.name, d, wo)) << e.name << ": " << n->bs600Fs()->lastError();
        }
        ASSERT_TRUE(n->flush()) << n->lastError();
        EXPECT_EQ(n->list().size(), 28u);
    }
    const Ergebnis e = fahrePfad(V::Prg710_1, ziel, -1, "DISK ENTHAELT 28 VON 54");
    EXPECT_EQ(static_cast<int>(e.stufe), static_cast<int>(Stufe::Liste)) << e.bild;
    std::filesystem::remove(ziel);
}
