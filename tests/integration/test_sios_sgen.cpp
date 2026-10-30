/**
 * @file test_sios_sgen.cpp
 * @brief SIOS 1526/1.1 SYSD und SGEN (Entwurf 43, AP S7, doc/sios/sgen.md).
 *
 * Gesichert ist, was bis heute laeuft: SYSD zeigt den Kopf der Systemdiskette, SGEN fuehrt bis
 * zur Modulauswahl und nimmt die Markierungen an.  Die Erzeugung der Systemdatei selbst endet
 * in `ERR FD 1 NFND` auf der Ausgabediskette (Ursache offen, sgen.md Abschn. 5) — der Lauf steht
 * als DISABLED_-Test hier, damit der naechste Anlauf nicht von vorn beginnen muss.
 *
 * Bedienfallen, die der Aufbau beachtet (sgen.md Abschn. 1):
 *  - Programme starten mit gesetzter Fehlersperre (00CBH Bit 7): erst CI (0xAF), dann ET1.
 *  - Nach dem ersten Plattenzugriff merkt sich SIOS die Kopfspur (befund_can_nfnd.md): vor dem
 *    naechsten Zugriff die Diskette neu einlegen.
 *  - B: ist ein 80-Zylinder-Laufwerk (K5600.20); SIOS doppelschrittet 40-Spur-Disketten selbst.
 *    Nur INIT braucht ein 40-Zylinder-Laufwerk (K5600.10).
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/machines/a5120/a5120.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "core/peripherals/k7637/k7637.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"

namespace {

constexpr uint32_t kET1 = K7637::QK_RAW_BASE | 0xFF;
constexpr uint32_t kET2 = K7637::QK_RAW_BASE | 0xFE;
constexpr uint32_t kCI  = K7637::QK_RAW_BASE | 0xAF;   // Fehler loeschen
using k1520test::TempDisk;

const FormatCatalog& formate() {
    static FormatCatalog c = [] {
        std::string f;
        FormatCatalog k = FormatCatalog::loadDefault(&f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

const FsCatalog& dateisysteme() {
    static FsCatalog c = [] {
        std::string f;
        FsCatalog k = FsCatalog::loadDefault(formate(), &f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

/// SIOS-Rechner: Sicherheitsstecker 1, SYLs in A: bis zur Systemzeile, @p b in B:.
struct Rechner {
    A5120Machine m;
    TempDisk a{"sios1526_syls.hfe"};
    TempDisk b;
    /// @p b40: B: als 40-Zylinder-Laufwerk (nur zum Formatieren mit INIT).
    static A5120Machine::Config cfg(bool b40 = false) {
        A5120Machine::Config c;
        c.drive_profiles = {"K5600.20", b40 ? "K5600.10" : "K5600.20", "none", "none"};
        c.sicherheitsstecker = 1;
        return c;
    }
    Rechner(const char* b_fixture, bool b40 = false)
        : m(cfg(b40)), b(b_fixture, std::string("sios_sgen_b_") + b_fixture) {
        m.powerOn();
        EXPECT_TRUE(m.mountDisk(0, a.path(), "k5601_ss80_26x128", false)) << m.lastError();
        EXPECT_TRUE(k1520test::runSmallUntil(m, "(M)", 300'000'000LL, 100'000));
        EXPECT_TRUE(m.mountDisk(1, b.path(), "k5601_ss40_26x128", false)) << m.lastError();
    }
    void tippe(const std::string& s) { k1520test::typeString(m, s); }
    void taste(uint32_t c) { k1520test::typeKey(m, c); }
    bool warte(const std::string& t, long long n = 60'000'000LL) {
        return k1520test::runSmallUntil(m, t, n, 5'000);
    }
    void ruhe(long long n) { k1520test::runSmallUntil(m, "\x01", n, 5'000); }
    std::string schirm() { return k1520test::vramText(m); }
    bool ein40(int drive, const std::string& pfad) {
        m.unmountDisk(drive);
        return m.mountDisk(drive, pfad, "k5601_ss40_26x128", false);
    }
};

/// EBCDIC fuer die Zeichen der Etiketten (Ziffern, Grossbuchstaben, Blank).
uint8_t ebcdic(char c) {
    if (c == ' ') return 0x40;
    if (c >= '0' && c <= '9') return uint8_t(0xF0 + (c - '0'));
    if (c >= 'A' && c <= 'I') return uint8_t(0xC1 + (c - 'A'));
    if (c >= 'J' && c <= 'R') return uint8_t(0xD1 + (c - 'J'));
    return uint8_t(0xE2 + (c - 'S'));   // S..Z
}

/// Zieldiskette per SIOS-INIT (wie SiosInit.InitialisiertDiskette), danach wie eine
/// Systemdiskette vorbelegt: SYSARE09 (BOE 01001 EOE 12026, Etikett wie bei SYLs/SYLp) + DATA
/// fuer den Rest.  Einmal je Prozess (~15 s).
const std::string& vorbereiteteZieldiskette() {
    static std::unique_ptr<TempDisk> ziel;
    if (ziel) return ziel->path();
    A5120Machine m(Rechner::cfg(true));
    m.powerOn();
    TempDisk syls("sios1526_syls.hfe", "sios_sgen_init_syls.hfe");
    TempDisk prog("sios1526_dp1.hfe", "sios_sgen_init_prog.hfe");
    ziel = std::make_unique<TempDisk>("sios1526_dp2.hfe", "sios_sgen_init_ziel.hfe");
    EXPECT_TRUE(m.mountDisk(0, syls.path(), "k5601_ss80_26x128", false));
    EXPECT_TRUE(k1520test::runSmallUntil(m, "(M)", 300'000'000LL, 100'000));
    EXPECT_TRUE(m.mountDisk(0, prog.path(), "k5601_ss40_26x128", false));
    EXPECT_TRUE(m.mountDisk(1, ziel->path(), "k5601_ss40_26x128", false));
    auto warte = [&](const std::string& t, long long n) {
        return k1520test::runSmallUntil(m, t, n, 5'000);
    };
    k1520test::typeString(m, "CAL");
    k1520test::typeKey(m, kET1);
    EXPECT_TRUE(warte("CAL_", 60'000'000LL));
    k1520test::typeString(m, "INIT");
    k1520test::typeKey(m, kET1);
    EXPECT_TRUE(warte("DISKETTE", 150'000'000LL));
    k1520test::runSmallUntil(m, "ZZZZ", 25'000'000LL, 5'000);   // 8"-Frage abwarten (befund_init.md)
    k1520test::typeString(m, "N");
    k1520test::typeKey(m, kET1);
    for (int i = 0; i < 5; ++i) {
        k1520test::runSmallUntil(m, "\x01", 20'000'000LL, 5'000);
        k1520test::typeKey(m, kET1);
    }
    k1520test::typeString(m, "Y");
    k1520test::typeKey(m, kET1);
    EXPECT_TRUE(warte("FINISHED", 1'500'000'000LL)) << k1520test::vramText(m);
    EXPECT_TRUE(m.unmountDisk(1));

    // INIT legt DATA ueber die ganze Diskette; SGEN erwartet davor die Systemdatei.
    const std::string dir = ziel->path() + ".d";
    std::filesystem::create_directories(dir);
    auto schreibe = [&](const char* name, size_t n) {
        std::ofstream f(dir + "/" + name, std::ios::binary);
        const std::string e5(n, '\xE5');
        f.write(e5.data(), static_cast<std::streamsize>(e5.size()));
    };
    std::string err;
    auto vol = DiskVolume::open(ziel->path(), "", formate(), dateisysteme(), err);
    EXPECT_TRUE(vol) << err;
    if (vol) {
        vol->setReadOnly(false);
        EXPECT_TRUE(vol->erase(FileRef::parse("DATA", 0))) << vol->lastError();
        schreibe("SYSARE09", 12 * 26 * 128);
        EXPECT_TRUE(vol->insertAll(dir, TransferOptions{})) << vol->lastError();
        std::filesystem::remove(dir + "/SYSARE09");
        schreibe("DATA", 25 * 26 * 128);
        EXPECT_TRUE(vol->insertAll(dir, TransferOptions{})) << vol->lastError();
        // Etikett wie bei den Systemdisketten: Kennzeichen P, Verfallsdatum 999999
        const char* hdr1 = "HDR1 SYSARE09         00128 01001 12026   P    000004             999999  12013 ";
        std::vector<uint8_t> sec(128, 0);
        for (int i = 0; i < 80; ++i) sec[i] = ebcdic(hdr1[i]);
        EXPECT_TRUE(vol->writeSectorAt(0, 0, 7, sec, nullptr)) << vol->lastError();
        EXPECT_TRUE(vol->flush());
    }
    std::filesystem::remove_all(dir);
    return ziel->path();
}

/// SGEN starten (SG1 in A:), OS-Version 09, Menuepunkt 1, Ausgabegeraet bestaetigen; endet
/// auf dem Auswahlbild "SIGN YOUR WISHES".
void sgenBisAuswahl(Rechner& r, TempDisk& sg1) {
    ASSERT_TRUE(r.ein40(0, sg1.path())) << r.m.lastError();
    r.tippe("CAL");
    r.taste(kET1);
    ASSERT_TRUE(r.warte("CAL_")) << r.schirm();
    r.tippe("SGEN");
    r.taste(kET1);
    ASSERT_TRUE(r.warte("GENERATE OPERATING SYSTEM", 300'000'000LL)) << r.schirm();
    r.ruhe(60'000'000LL);
    r.tippe("09");                                 // Feld ueberschreibt "06"; SYSORG06 gibt es nicht
    r.taste(kET1);
    ASSERT_TRUE(r.warte("1 SYSTEM LOADER", 100'000'000LL)) << r.schirm();
    r.ruhe(20'000'000LL);
    r.tippe("1");
    r.taste(kET1);
    ASSERT_TRUE(r.warte("DA/OUTPUT-DEVICE", 100'000'000LL)) << r.schirm();
    r.ruhe(20'000'000LL);
    ASSERT_TRUE(r.ein40(0, sg1.path()));           // veraltete Kopfspur heilen
    r.taste(kET1);
    ASSERT_TRUE(r.warte("SIGN YOUR WISHES", 300'000'000LL)) << r.schirm();
    r.ruhe(10'000'000LL);
}

/// Markierung der 43 Eintraege der ersten Seite: 'X' = aufnehmen, Blank = uebergehen.
void markiere(Rechner& r, const std::string& wahl) {
    for (size_t e = 0; e < 43; ++e) {
        r.tippe(e < wahl.size() && wahl[e] == 'X' ? "X" : " ");
        r.ruhe(600'000LL);
    }
}

}  // namespace

// SYSD auf der Systemdiskette (SYLs in A: = Laufwerk 04), Programm von DP2 in B: (ASN 0315 66).
// Nach CI + ET1 fragt es "PRINT Y/N:"; "N" zeigt Kopf und Modulliste der Systemdiskette.  Bis zur
// /IEO-Korrektur (2026-10-30) endete SYSD V 1.5 hier mit "ERR FD 1 NFND" (sgen.md Abschn. 2).
TEST(SiosSgen, SysdZeigtKopfDerSystemdiskette) {
    Rechner r("sios1526_dp2.hfe");
    r.tippe("ASN");
    r.taste(kET1);
    r.tippe("0315");
    r.taste(kET1);
    ASSERT_TRUE(r.warte("0315_     50")) << r.schirm();
    r.tippe("66");
    r.taste(kET1);
    r.taste(kET2);
    ASSERT_TRUE(r.warte("(M)_")) << r.schirm();
    r.tippe("CAL");
    r.taste(kET1);
    ASSERT_TRUE(r.warte("CAL_")) << r.schirm();
    r.tippe("SYSD");
    r.taste(kET1);
    ASSERT_TRUE(r.warte("LOAD SYSTEM-DISK DRIVE 04", 200'000'000LL)) << r.schirm();
    r.ruhe(30'000'000LL);
    r.taste(kCI);          // ohne CI nimmt SYSD die Tasten nicht an (Fehlersperre 00CBH Bit 7)
    r.ruhe(5'000'000LL);
    r.taste(kET1);
    ASSERT_TRUE(r.warte("PRINT Y/N", 300'000'000LL)) << r.schirm();
    r.tippe("N");
    r.taste(kET1);
    ASSERT_TRUE(r.warte("OPERATING SYSTEM VERSION 11", 300'000'000LL)) << r.schirm();
    r.ruhe(20'000'000LL);
    const std::string v = r.schirm();
    EXPECT_NE(v.find("VOL-NAME: ROBINI"), std::string::npos) << v;
    EXPECT_NE(v.find("SYSTEM-LOADER"), std::string::npos) << v;
    EXPECT_NE(v.find("BOE: 01001"), std::string::npos) << v;
    EXPECT_NE(v.find("EOE: 12026"), std::string::npos) << v;
    // Modulliste (Bildschirm woertlich, doc/sios/sgen.md Abschn. 2), nach dem Kopf; die Zeilen stehen in
    // Spalte 9 (vramText traegt die Spalten 1:1, Leerzeichen als Leerstellen).
    for (const char* m : {"GRUND", "T-K7637", "BAB", "BIN ARI", "DEZ ARI", "BCD ARI", "SD 3-I",
                          "LOG-FD2", "DFUE-A", "START ->ET1 / MONITOR ->ET2", "34 K-BYTE"})
        EXPECT_NE(v.find(m), std::string::npos) << m << "\n" << v;
}

// SGEN von SG1 (SYSORG09 + PHASELB) bis zur Modulauswahl.  Die Liste kommt aus dem Verzeichnis
// von SYSORG09 (Satz 0, Formatdokument 8.1); jeder Eintrag zeigt die Satzzahl.
TEST(SiosSgen, SgenZeigtDieModulliste) {
    Rechner r("sios1526_dp2.hfe");
    TempDisk sg1("sios1526_sg1.hfe", "sios_sgen_sg1.hfe");
    sgenBisAuswahl(r, sg1);
    const std::string v = r.schirm();
    for (const char* e : {"GRUND    06", "T-K7637  04", "T-K7606  03", "KUT      01", "BAB      04",
                          "PH-FD640 06", "LOG FD2  05", "LOG FD1  04", "BSC7CZ   06"})
        EXPECT_NE(v.find(e), std::string::npos) << e << "\n" << v;
    EXPECT_NE(v.find("SYSTEM LOADER=DA/OUT 05"), std::string::npos) << v;
    EXPECT_NE(v.find("OS V 09"), std::string::npos) << v;

    // Markieren: ein Zeichen vor dem Eintrag, der Zeiger rueckt weiter.
    r.tippe("X");
    r.tippe("X");
    r.tippe(" ");
    r.ruhe(3'000'000LL);
    const std::string w = r.schirm();
    EXPECT_NE(w.find("X GRUND    06   X T-K7637  04     T-K7606"), std::string::npos) << w;
}

// Ziel: eine generierte Systemdiskette erzeugen und booten.  Heute endet SGEN nach "SGEN IS
// WORKING" mit "ERR FD 1  NFND  ....  D66" (Ausgabelaufwerk); es loescht dabei das Etikett
// SYSARE09 (HDR1 -> DDR1) und sucht danach etwas, das es auf der Diskette nicht findet.
TEST(SiosSgen, GeneriertEineSystemdiskette) {
    const std::string ziel = vorbereiteteZieldiskette();
    Rechner r("sios1526_dp2.hfe");
    TempDisk sg1("sios1526_sg1.hfe", "sios_sgen_sg1.hfe");
    sgenBisAuswahl(r, sg1);
    //          GRUND T-K7637                 KUT BAB   (Eintraege 0 1 5 6, 22 23 24)
    markiere(r, "XX   XX               " "XXX");
    ASSERT_TRUE(r.ein40(1, ziel));
    ASSERT_TRUE(r.ein40(0, sg1.path()));
    r.taste(kET1);                                 // -> ***LISTING***
    r.ruhe(100'000'000LL);
    r.taste(kET1);                                 // -> SGEN IS WORKING
    ASSERT_TRUE(r.warte("SGEN FINISHED", 600'000'000LL)) << r.schirm();
}


// Gegenprobe zum AUP-/K7633-Modell (doc/sios/module_k7633_kut.md): ein System mit T-K7633 + KUT
// (statt T-K7637) generieren und an einem A5120/K8924 mit AUP booten.  Beleg fuer die Gueltigkeitsbits
// (Bit 1 = Terminal), das Typzeichen nach Reset und die Schluesselschalter (SS2 -> (00CBH) Bit 1).
// Die Zieldiskette ist 40-spurig und bootet nur in einem K5600.10 (SIOS 0.9, wie Sios09Rechner).
TEST(SiosSgen, GeneriertEinSystemMitK7633UndBootetAmAup) {
    const std::string ziel = vorbereiteteZieldiskette();
    TempDisk sys("sios1526_dp2.hfe", "sios_sgen_k7633_system.hfe");
    {
        Rechner r("sios1526_dp2.hfe");
        TempDisk sg1("sios1526_sg1.hfe", "sios_sgen_sg1_k7633.hfe");
        sgenBisAuswahl(r, sg1);
        //          GRUND T-K7633 KUT BAB        (Eintraege 0 4 5 6; 22 23 24 = PH-FD640, LOG FD2, LOG FD1)
        markiere(r, "X   XXX               " "XXX");
        ASSERT_TRUE(r.ein40(1, ziel));
        ASSERT_TRUE(r.ein40(0, sg1.path()));
        r.taste(kET1);
        r.ruhe(100'000'000LL);
        r.taste(kET1);
        ASSERT_TRUE(r.warte("SGEN FINISHED", 600'000'000LL)) << r.schirm();
        ASSERT_TRUE(r.m.unmountDisk(1));
    }
    std::filesystem::copy_file(ziel, sys.path(), std::filesystem::copy_options::overwrite_existing);

    A5120Machine::Config c;
    c.drive_profiles = {"K5600.10", "K5600.20", "none", "none"};
    c.aup = true;
    A5120Machine m(c);
    m.powerOn();
    ASSERT_TRUE(m.mountDisk(0, sys.path(), "k5601_ss40_26x128", false)) << m.lastError();
    // Die Systemzeile heisst, der Treiber hat das Typzeichen 80H bekommen (sonst Fehler nach ~0,5 s).
    ASSERT_TRUE(k1520test::runSmallUntil(m, "SIOS 1526/0.9 (M)_", 300'000'000LL, 100'000))
        << k1520test::vramText(m);

    using T = AUP::Tastatur;
    auto lauf = [&](long long n) { k1520test::runSmallUntil(m, "\x01", n, 5'000); };
    auto drueck = [&](int pos) {
        ASSERT_TRUE(m.aup()->taste(T::K7633, pos, true));
        lauf(1'000'000);
        m.aup()->taste(T::K7633, pos, false);
        lauf(1'000'000);
    };
    auto positionVon = [](int code) {
        for (int p = 0; p < 0x48; ++p)
            if (AUP::code(T::K7633, p, true) == code) return p;
        return -1;
    };
    auto cb = [&] { return m.memReadDebug(0x00CB); };

    // Schluesselschalter SS2 (A9H) kippt Bit 1 von (00CBH), bei jeder Drehung.
    EXPECT_EQ(cb() & 0x02, 0);
    ASSERT_TRUE(m.aup()->schluessel(2, true));
    lauf(5'000'000);
    EXPECT_EQ(cb() & 0x02, 0x02) << "SS2 ein";

    // Tasten ueber die K7633: F05 (A1H) schaltet ALPHA (Lampe folgt dem Kommando), dann DAT + ET1.
    drueck(0x27);
    EXPECT_NE(m.aup()->lampen() & AUP::kLampeAlpha, 0);
    for (char ch : std::string("DAT")) {
        const int p = positionVon(ch);
        ASSERT_GE(p, 0) << ch;
        drueck(p);
    }
    drueck(0x40);   // ET1 = FFH
    EXPECT_TRUE(k1520test::runSmallUntil(m, "DAT     00.00.00", 60'000'000LL, 5'000)) << k1520test::vramText(m);
    drueck(0x41);   // ET2 = FEH
    EXPECT_TRUE(k1520test::runSmallUntil(m, "(M)_", 60'000'000LL, 5'000)) << k1520test::vramText(m);

    ASSERT_TRUE(m.aup()->schluessel(2, false));
    lauf(5'000'000);
    EXPECT_EQ(cb() & 0x02, 0) << "SS2 aus";
}

// SYSORG09 geht ueber zwei Disketten (HDR1-Kennzeichen "EC01" = auf der naechsten fortgesetzt): SG1
// traegt nur die Saetze bis 91.  Wird ein Modul dahinter gewaehlt (hier SLE, Satz 98), verlangt SGEN
// den naechsten Datentraeger: ERR FD 6 SEQU (Handbuch S. 25/26 "der naechste Datentraeger ist
// erforderlich", S. 100 Datentraegerwechsel).  Belegt damit: es gab eine Folgediskette.
TEST(SiosSgen, ModulAufDerFolgedisketteVerlangtDenNaechstenDatentraeger) {
    const std::string ziel = vorbereiteteZieldiskette();
    Rechner r("sios1526_dp2.hfe");
    TempDisk sg1("sios1526_sg1.hfe", "sios_sgen_sg1_folge.hfe");
    sgenBisAuswahl(r, sg1);
    //          GRUND T-K7637                 KUT BAB   ... SLE (Eintrag 29)
    markiere(r, "XX   XX               " "XXX" "     " "X");
    ASSERT_TRUE(r.ein40(1, ziel));
    ASSERT_TRUE(r.ein40(0, sg1.path()));
    r.taste(kET1);                                 // -> ***LISTING***
    r.ruhe(100'000'000LL);
    r.taste(kET1);                                 // -> SGEN IS WORKING
    ASSERT_TRUE(r.warte("SEQU", 600'000'000LL)) << r.schirm();
    const std::string v = r.schirm();
    EXPECT_NE(v.find("ERR FD 6  SEQU SYSORG09 98"), std::string::npos) << v;
    EXPECT_EQ(v.find("FINISHED"), std::string::npos) << v;
}
