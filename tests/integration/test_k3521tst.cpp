/**
 * @file test_k3521tst.cpp
 * @brief CP/A-Gastlauf mit dem eingecheckten K3521TST.COM (Entwurf 33 N10i): Hilfetext ohne/mit
 *        falschen Parametern, Modus T (gruen auf der Karte bei 8000H, zerstoerungsfrei, FEHLER bei
 *        gesperrter Karte), Modus E (Tippen schreibt in die Karte, Pfeiltasten, Bildlauf, Cursor,
 *        Persistenz ueber Netz-Aus/Ein).
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"

#ifndef K1520_K3521TST_COM
#error "K1520_K3521TST_COM fehlt (tools/k3521tst/k3521tst.com)"
#endif

namespace {
using C = A5120Machine::Config;
using k1520test::runSmallUntil;
using k1520test::runSmallUntilVisible;
using k1520test::runUntilVramContains;
using k1520test::typeCtrl;
using k1520test::typeKey;
using k1520test::typeString;
using k1520test::vramLines;

constexpr long long kBootBudget = 90'000'000;
constexpr long long kFrist      = 60'000'000;
constexpr uint32_t QK_LEFT = 0x01000012, QK_UP = 0x01000013, QK_RIGHT = 0x01000014, QK_DOWN = 0x01000015;

bool comAufDiskette(const std::string& disk) {
    std::string fe, err;
    static const FormatCatalog fk = FormatCatalog::loadDefault(&fe);
    static const FsCatalog     fs = FsCatalog::loadDefault(fk, &fe);
    auto vol = DiskVolume::open(disk, "cpa780", fk, fs, err, /*read_only=*/false);
    if (!vol) return false;
    FileRef ref;
    ref.name = "K3521TST.COM";
    return vol->insert(K1520_K3521TST_COM, ref, TransferOptions{}) && vol->flush();
}

void kaltstart(A5120Machine& m) {
    m.powerOn();
    ASSERT_TRUE(runUntilVramContains(m, "TPA ist OK!", kBootBudget)) << vramLines(m);
    ASSERT_TRUE(runSmallUntilVisible(m, "A>", kBootBudget)) << vramLines(m);
    k1520test::runCycles(m, 5'000'000);
}

void befehl(A5120Machine& m, const std::string& cmd) {
    typeString(m, cmd);
    typeKey(m, k1520test::QK_RETURN);
}

C mitKarte() {
    C c;
    c.k3521_basis = 0x8000;
    return c;
}

/// Maschine mit Diskette, gebootet.
struct Gast {
    k1520test::TempDisk disk;
    A5120Machine m;
    explicit Gast(const char* name, const C& c = mitKarte())
        : disk("cpa_cpa780_k5601_noclock.img", name), m(c) {}
    void hoch() {
        k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
        ASSERT_TRUE(comAufDiskette(disk.path()));
        ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
        ASSERT_NO_FATAL_FAILURE(kaltstart(m));
    }
};
}  // namespace

TEST(K3521Tst, OhneParameterZeigtDenHilfetext) {
    Gast g("k1520_k3521tst_hilfe.img");
    ASSERT_NO_FATAL_FAILURE(g.hoch());
    befehl(g.m, "k3521tst");
    ASSERT_TRUE(runSmallUntil(g.m, "Keine Parameter.", kFrist)) << vramLines(g.m);
    const std::string s = vramLines(g.m);
    EXPECT_NE(s.find("Pruefprogramm fuer die K3521.30"), std::string::npos) << s;
    EXPECT_NE(s.find("olliy78"), std::string::npos) << s;
    EXPECT_NE(s.find("Aufruf:   K3521TST <adresse> <modus>"), std::string::npos) << s;
    EXPECT_NE(s.find("Beispiel: K3521TST 8000 T"), std::string::npos) << s;
    EXPECT_NE(s.find("ZERSTOERUNGSFREI"), std::string::npos) << s;
}

TEST(K3521Tst, FalscheParameterMeldenSichUndZeigenDenHilfetext) {
    Gast g("k1520_k3521tst_falsch.img");
    ASSERT_NO_FATAL_FAILURE(g.hoch());
    struct F { const char* cmd; const char* meldung; };
    const std::vector<F> faelle = {
        {"k3521tst 8001 t", "Adresse nicht zulaessig"},   // nicht im 4-KB-Raster
        {"k3521tst 0 t", "Adresse nicht zulaessig"},      // Seite 0 / Programm
        {"k3521tst c000 t", "Adresse nicht zulaessig"},   // Fenster reicht ueber das BDOS
        {"k3521tst f000 e", "Adresse nicht zulaessig"},
        {"k3521tst zz t", "keine Hex-Zahl"},
        {"k3521tst 12345 t", "keine Hex-Zahl"},
        {"k3521tst 8000 x", "Modus nicht zulaessig"},
        {"k3521tst 8000", "Modus fehlt"},
        {"k3521tst 8000 t t", "Zu viele Parameter"},
    };
    for (const auto& f : faelle) {
        befehl(g.m, f.cmd);
        ASSERT_TRUE(runSmallUntil(g.m, f.meldung, kFrist)) << f.cmd << "\n" << vramLines(g.m);
        k1520test::runCycles(g.m, 10'000'000);
    }
}

TEST(K3521Tst, ModusTLaeuftGruenAufDerKarteUndLaesstDenInhaltStehen) {
    Gast g("k1520_k3521tst_t.img");
    ASSERT_NO_FATAL_FAILURE(g.hoch());
    for (int i = 0; i < 4096; ++i) g.m.k3521()->poke(static_cast<uint16_t>(i), static_cast<uint8_t>(i * 7 + 3));
    befehl(g.m, "k3521tst 8000 t");   // Kleinschreibung zulaessig
    ASSERT_TRUE(runSmallUntil(g.m, "K3521TST OK", 3 * kFrist)) << vramLines(g.m);
    const std::string s = vramLines(g.m);
    EXPECT_NE(s.find("Muster AA"), std::string::npos) << s;
    EXPECT_NE(s.find("March C-"), std::string::npos) << s;
    EXPECT_NE(s.find("Rueckschreiben"), std::string::npos) << s;
    EXPECT_EQ(s.find("FEHLER"), std::string::npos) << s;
    for (int i = 0; i < 4096; ++i)
        ASSERT_EQ(g.m.k3521()->peek(static_cast<uint16_t>(i)), static_cast<uint8_t>(i * 7 + 3)) << "Inhalt bei " << i;
}

TEST(K3521Tst, ModusTMeldetFehlerBeiGesperrterKarte) {
    Gast g("k1520_k3521tst_fehler.img");
    ASSERT_NO_FATAL_FAILURE(g.hoch());
    // /MEMDI sperrt die Karte: Lesen liefert FFH, Schreiben wird verworfen -> jedes Muster ausser FF faellt durch.
    g.m.k3521()->setMemDI(true);
    befehl(g.m, "k3521tst 8000 t");
    ASSERT_TRUE(runSmallUntil(g.m, "K3521TST FEHLER", 3 * kFrist)) << vramLines(g.m);
    const std::string s = vramLines(g.m);
    EXPECT_NE(s.find("FEHLER"), std::string::npos) << s;
    EXPECT_NE(s.find("Soll 00 Ist FF"), std::string::npos) << s;
}

TEST(K3521Tst, EditorSchreibtSofortInDieKarteBewegtDenCursorUndBleibtNachNetzAus) {
    Gast g("k1520_k3521tst_e.img");
    ASSERT_NO_FATAL_FAILURE(g.hoch());
    g.m.k3521()->poke(80 * 24, 'Z');   // Zeile 24: erst nach dem Bildlauf sichtbar
    befehl(g.m, "k3521tst 8000 e");
    ASSERT_TRUE(runSmallUntil(g.m, "..........", kFrist)) << vramLines(g.m);   // leere Karte = FFH = Punkte
    typeString(g.m, "Hallo");
    EXPECT_EQ(g.m.k3521()->peek(0), 'H');
    EXPECT_EQ(g.m.k3521()->peek(4), 'o');
    EXPECT_EQ(g.m.memReadDebug(0x8000), 'H') << "die Karte antwortet am Bus";
    EXPECT_EQ(vramLines(g.m).substr(0, 5), "Hallo");
    typeKey(g.m, QK_LEFT);
    typeKey(g.m, QK_LEFT);
    typeString(g.m, "X");
    EXPECT_EQ(g.m.k3521()->peek(3), 'X');
    EXPECT_EQ(g.m.k3521()->peek(2), 'l');
    EXPECT_EQ(g.m.k3521()->peek(4), 'o');
    typeKey(g.m, QK_DOWN);
    typeString(g.m, "u");
    EXPECT_EQ(g.m.k3521()->peek(80 + 4), 'u') << "eine Zeile tiefer, gleiche Spalte (5 -> 4 nach dem X)";
    typeKey(g.m, QK_UP);
    typeKey(g.m, QK_RIGHT);
    typeString(g.m, "!");
    EXPECT_EQ(g.m.k3521()->peek(6), '!');

    // Der sichtbare Cursor (Bit 7 einer Zelle, vom BIOS-Treiber) steht an der Schreibstelle.
    int cursorzelle = -1;
    for (int i = 0; i < 40 && cursorzelle < 0; ++i) {
        k1520test::runCycles(g.m, 1'000'000);
        for (int z = 0; z < 80 * 24; ++z)
            if (g.m.memReadDebug(static_cast<uint16_t>(0xF800 + z)) & 0x80) { cursorzelle = z; break; }
    }
    EXPECT_EQ(cursorzelle, 7) << "Cursor hinter dem '!' (Zeile 0, Spalte 7)";

    // Bildlauf: 23 Zeilen tiefer steht Zeile 24 ('Z') ab Zeile 23 des Bildschirms.
    for (int i = 0; i < 23; ++i) typeKey(g.m, QK_DOWN);
    EXPECT_EQ(vramLines(g.m).find("Z"), std::string::npos) << "noch nicht sichtbar";
    typeKey(g.m, QK_DOWN);
    EXPECT_NE(vramLines(g.m).find("Z"), std::string::npos) << vramLines(g.m);

    typeCtrl(g.m, 'c');   // ^C beendet
    ASSERT_TRUE(runSmallUntil(g.m, "K3521TST ENDE", kFrist)) << vramLines(g.m);

    // Netz-Aus/Ein und neuer CP/A-Kaltstart: die Karte behaelt den Text.
    ASSERT_NO_FATAL_FAILURE(kaltstart(g.m));
    EXPECT_EQ(g.m.k3521()->peek(0), 'H');
    EXPECT_EQ(g.m.k3521()->peek(3), 'X');
    befehl(g.m, "k3521tst 8000 e");
    // Zelle 0 traegt den Cursor (Bit 7) und erscheint in vramLines nicht als 'H'.
    ASSERT_TRUE(runSmallUntil(g.m, "alXo.!", kFrist)) << vramLines(g.m);
    typeCtrl(g.m, 'c');
    ASSERT_TRUE(runSmallUntil(g.m, "K3521TST ENDE", kFrist)) << vramLines(g.m);
}
