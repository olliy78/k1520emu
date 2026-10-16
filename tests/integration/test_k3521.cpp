/**
 * @file test_k3521.cpp
 * @brief K3521.30 in der A5120-Maschine (Entwurf 33 N10b): Option aus = unveraendert, Adresswahl,
 *        Ueberdeckung des RAMs, /MEMDI ueber BS-PIO A7, Inhalt ueber RESET und Netz-Aus/Ein,
 *        Konfigurationstext, dauerhafte Ablage ueber zwei Maschineninstanzen — und ein
 *        CP/A-Gastlauf (ein .COM schreibt 4 KB, neue Maschine liest sie wieder).
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/a5120/a5120_konfig_text.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"

namespace {
using C = A5120Machine::Config;

std::unique_ptr<A5120Machine> baue(const C& c) {
    auto m = std::make_unique<A5120Machine>(c);
    m->powerOn();
    return m;
}
C mitK3521(int basis, K3521::Memdi md = K3521::Memdi::Bus) {
    C c;
    c.k3521_basis = basis;
    c.k3521_memdi = md;
    return c;
}
}  // namespace

TEST(K3521Maschine, OhneOptionGibtEsKeineKarte) {
    C c;
    EXPECT_EQ(c.k3521_basis, -1);
    auto m = baue(c);
    EXPECT_EQ(m->k3521(), nullptr);
}

TEST(K3521Maschine, UnzulaessigeBasenWerfen) {
    EXPECT_THROW(A5120Machine{mitK3521(0x0800)}, std::invalid_argument);
    EXPECT_THROW(A5120Machine{mitK3521(0x10000)}, std::invalid_argument);
}

TEST(K3521Maschine, UeberdecktDasRamBeimLesenSchreibenTrifftBeide) {
    auto m = baue(mitK3521(0x4000));
    ASSERT_NE(m->k3521(), nullptr);
    m->memWriteDebug(0x4010, 0x6B);
    EXPECT_EQ(m->memReadDebug(0x4010), 0x6B);
    EXPECT_EQ(m->k3521()->peek(0x10), 0x6B);
    // Inhalt der Karte direkt veraendern: Lesen liefert die Karte, nicht das RAM darunter.
    m->k3521()->poke(0x10, 0x11);
    EXPECT_EQ(m->memReadDebug(0x4010), 0x11);
    // Ausserhalb des Fensters bleibt das normale RAM.
    m->memWriteDebug(0x5000, 0x22);
    EXPECT_EQ(m->memReadDebug(0x5000), 0x22);
    EXPECT_EQ(m->k3521()->peek(0xFFF), 0xFF);
}

TEST(K3521Maschine, InhaltUeberlebtResetUndNetzAus) {
    auto m = baue(mitK3521(0x8000));
    m->memWriteDebug(0x8123, 0x5A);
    m->k3521()->setzeAkkuLeer(true);
    m->reset();
    EXPECT_EQ(m->k3521()->peek(0x123), 0x5A) << "RESET erhaelt den Inhalt";
    EXPECT_FALSE(m->k3521()->sue()) << "RESET loescht den /SUE-Merker";
    m->powerOn();
    EXPECT_EQ(m->k3521()->peek(0x123), 0x5A) << "Netz-Aus/Ein: die Akkus puffern";
}

TEST(K3521Maschine, KoppelMemdiUeberBsPioA7SperrtDieKarte) {
    auto m = baue(mitK3521(0x4000, K3521::Memdi::Memdi1));
    m->memWriteDebug(0x4000, 0x77);
    m->bus().ioWrite(0x09, 0x0F);    // BS-PIO Port A: Betriebsart 0 (Ausgabe)
    m->bus().ioWrite(0x08, 0x80);    // A7 = MEMDI1/2 aktiv
    EXPECT_TRUE(m->k3521()->memDI());
    EXPECT_EQ(m->memReadDebug(0x4000), 0xFF) << "gesperrt: Lesen 0xFF";
    m->bus().ioWrite(0x08, 0x00);
    EXPECT_FALSE(m->k3521()->memDI());
    EXPECT_EQ(m->memReadDebug(0x4000), 0x77);
}

TEST(K3521Maschine, BusMemdiBeeinflusstDieKoppelwahlNicht) {
    // Auf Bus-/MEMDI gewaehlt reagiert die Karte NICHT auf A7 (das ist MEMDI1/2).
    auto m = baue(mitK3521(0x4000, K3521::Memdi::Bus));
    m->bus().ioWrite(0x09, 0x0F);
    m->bus().ioWrite(0x08, 0x80);
    EXPECT_FALSE(m->k3521()->memDI());
}

TEST(K3521Maschine, KonfigurationstextUndFehler) {
    C c;
    std::string err;
    ASSERT_TRUE(a5120konfig::text(c, "k3521=C000;k3521_memdi=MEMDI2", err)) << err;
    EXPECT_EQ(c.k3521_basis, 0xC000);
    EXPECT_EQ(c.k3521_memdi, K3521::Memdi::Memdi2);
    C d;
    ASSERT_TRUE(a5120konfig::text(d, "k3521=1000", err)) << err;
    EXPECT_EQ(d.k3521_basis, 0x1000);
    ASSERT_TRUE(a5120konfig::text(d, "k3521=aus", err)) << err;
    EXPECT_EQ(d.k3521_basis, -1);
    C e;
    EXPECT_FALSE(a5120konfig::text(e, "k3521=4800", err));
    EXPECT_NE(err.find("4-KB-Raster"), std::string::npos) << err;
    EXPECT_FALSE(a5120konfig::text(e, "k3521=zzz", err));
    EXPECT_FALSE(a5120konfig::text(e, "k3521_memdi=memdi1", err));
    EXPECT_NE(err.find("nur mit k3521"), std::string::npos) << err;
    EXPECT_FALSE(a5120konfig::text(e, "k3521=4000;k3521_memdi=x", err));
}

TEST(K3521Maschine, DateiUeberlebtZweiMaschineninstanzen) {
    const std::string p = k1520test::tempPath("k3521_maschine.bin");
    std::filesystem::remove(p);
    C c = mitK3521(0x4000);
    c.k3521_datei = p;
    {
        auto m = baue(c);
        m->memWriteDebug(0x4ABC, 0xC3);
    }   // Destruktor schreibt zurueck
    ASSERT_TRUE(std::filesystem::exists(p));
    EXPECT_EQ(std::filesystem::file_size(p), 4096u);
    auto m2 = baue(c);
    EXPECT_EQ(m2->k3521()->peek(0xABC), 0xC3);
    EXPECT_EQ(m2->memReadDebug(0x4ABC), 0xC3);
    std::filesystem::remove(p);
}

TEST(K3521Maschine, DateiMitFalscherGroesseLaesstDenBauScheitern) {
    const std::string p = k1520test::tempPath("k3521_kaputt.bin");
    { std::FILE* f = std::fopen(p.c_str(), "wb"); std::fputs("zu kurz", f); std::fclose(f); }
    C c = mitK3521(0x4000);
    c.k3521_datei = p;
    EXPECT_THROW(A5120Machine{c}, std::runtime_error);
    std::filesystem::remove(p);
}

// ─── CP/A-Gastlauf ───────────────────────────────────────────────────────────
// SCHREIB.COM fuellt 8000H..8FFFH mit (3 + 7·i) mod 256, LESE.COM kopiert 8000H..8FFFH nach
// 4000H.  Die Karte steckt bei 8000H, weil der CP/A-Kaltstart (Messung 2026-10-17, Karte
// je 4-KB-Seite mit Muster vorbelegt) 0000H-6FFFH und C000H-FFFFH ueberschreibt, 7000H-AFFFH
// aber nicht anfasst (b000H teilweise) — an anderer Stelle ginge der Inhalt beim BOOT verloren,
// nicht durch die Karte.  Beide enden mit JP 0 (Warmstart).  Die Maschine lebt zweimal (Kaltstart, Ende, neue
// Maschine mit derselben Datei): was die zweite sieht, hat die Karte ueber die Datei behalten.
namespace {
using k1520test::runSmallUntilVisible;
using k1520test::runUntilVramContains;
using k1520test::typeKey;
using k1520test::typeString;
using k1520test::vramLines;

constexpr long long kBootBudget = 90'000'000;
constexpr long long kBefehl     = 60'000'000;

const std::vector<uint8_t> kSchreib = {
    0x21, 0x00, 0x80,        // LD HL,8000H
    0x11, 0x00, 0x10,        // LD DE,1000H
    0x0E, 0x03,              // LD C,3
    0x71, 0x79, 0xC6, 0x07, 0x4F, 0x23, 0x1B, 0x7A, 0xB3,   // loop: LD (HL),C / LD A,C / ADD A,7 / LD C,A / INC HL / DEC DE / LD A,D / OR E
    0x20, 0xF5,              // JR NZ,loop
    0xC3, 0x00, 0x00};       // JP 0
const std::vector<uint8_t> kLese = {
    0x21, 0x00, 0x80,        // LD HL,8000H
    0x11, 0x00, 0x40,        // LD DE,4000H
    0x01, 0x00, 0x10,        // LD BC,1000H
    0xED, 0xB0,              // LDIR
    0xC3, 0x00, 0x00};       // JP 0

bool comAufDiskette(const std::string& disk, const std::string& name, const std::vector<uint8_t>& b) {
    const std::string host = k1520test::tempPath(name);
    { std::ofstream f(host, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), b.size()); }
    std::string fe, err;
    static const FormatCatalog fk = FormatCatalog::loadDefault(&fe);
    static const FsCatalog     fs = FsCatalog::loadDefault(fk, &fe);
    auto vol = DiskVolume::open(disk, "cpa780", fk, fs, err, /*read_only=*/false);
    bool ok = false;
    if (vol) {
        FileRef ref;
        ref.name = name;
        ok = vol->insert(host, ref, TransferOptions{}) && vol->flush();
    }
    std::filesystem::remove(host);
    return ok;
}

bool kommando(A5120Machine& m, const std::string& cmd) {
    typeString(m, cmd);
    typeKey(m, k1520test::QK_RETURN);
    // Der Warmstart druckt einen neuen Prompt; einige Millionen Takte Ruhe genuegen.
    k1520test::runCycles(m, 30'000'000);
    return true;
}

void kaltstart(A5120Machine& m) {
    m.powerOn();
    ASSERT_TRUE(runUntilVramContains(m, "TPA ist OK!", kBootBudget)) << vramLines(m);
    ASSERT_TRUE(runSmallUntilVisible(m, "A>", kBootBudget)) << vramLines(m);
    k1520test::runCycles(m, 5'000'000);
}
}  // namespace

TEST(K3521Maschine, CpaGastSchreibtUndLiestNachNeustartDerMaschine) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk disk("cpa_cpa780_k5601_noclock.img", "k1520_k3521_cpa.img");
    ASSERT_TRUE(comAufDiskette(disk.path(), "SCHREIB.COM", kSchreib));
    ASSERT_TRUE(comAufDiskette(disk.path(), "LESE.COM", kLese));
    const std::string datei = k1520test::tempPath("k3521_cpa.bin");
    std::filesystem::remove(datei);
    C c = mitK3521(0x8000);
    c.k3521_datei = datei;
    {
        A5120Machine m(c);
        ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
        ASSERT_NO_FATAL_FAILURE(kaltstart(m));
        kommando(m, "SCHREIB");
        for (int i = 0; i < 4096; ++i)
            ASSERT_EQ(m.k3521()->peek(static_cast<uint16_t>(i)), (3 + 7 * i) & 0xFF) << i;
    }
    ASSERT_TRUE(std::filesystem::exists(datei));
    {
        A5120Machine m(c);   // neue Maschine, Karte kommt aus der Datei
        ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
        ASSERT_NE(m.k3521(), nullptr);
        EXPECT_EQ(m.k3521()->peek(5), (3 + 7 * 5) & 0xFF) << "vor dem Boot geladen";
        ASSERT_NO_FATAL_FAILURE(kaltstart(m));
        for (int i = 0; i < 4096; ++i)
            ASSERT_EQ(m.k3521()->peek(static_cast<uint16_t>(i)), (3 + 7 * i) & 0xFF)
                << "nach CP/A-Kaltstart bei " << i;
        kommando(m, "LESE");
        for (int i = 0; i < 4096; ++i)
            ASSERT_EQ(m.memReadDebug(static_cast<uint16_t>(0x4000 + i)), (3 + 7 * i) & 0xFF)
                << "vom Gast gelesen bei " << i;
    }
    std::filesystem::remove(datei);
}
