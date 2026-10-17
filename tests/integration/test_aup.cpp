/**
 * @file test_aup.cpp
 * @brief AUP 062-8531 (Tastaturteil) in der A5120-Maschine (Entwurf 33 N10c): Option aus = Tore
 *        40H-43H unbelegt, Stecken (Config, installAup, Konfigurationstext), /RESET, und ein
 *        CP/A-Gastlauf mit dem eingecheckten AUPTEST.COM (Codes am Bildschirm, Lampen/Hupe ueber
 *        die Rechnertastatur, Meldung ohne Karte).
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include <fstream>
#include <iterator>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/a5120/a5120_konfig_text.h"
#include "core/serial/hub.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"

#ifndef K1520_AUPTEST_COM
#error "K1520_AUPTEST_COM fehlt (tools/auptest/auptest.com)"
#endif
#ifndef K1520_AUPTSIO_COM
#error "K1520_AUPTSIO_COM fehlt (tools/auptest/auptsio.com)"
#endif

namespace {
using C = A5120Machine::Config;
using T = AUP::Tastatur;
}  // namespace

TEST(AupMaschine, OhneOptionSindDieToreFrei) {
    C c;
    EXPECT_FALSE(c.aup);
    A5120Machine m(c);
    m.powerOn();
    EXPECT_EQ(m.aup(), nullptr);
    for (int p = 0x40; p <= 0x43; ++p) EXPECT_EQ(m.ioReadDebug(uint8_t(p)), 0xFF) << p;
}

TEST(AupMaschine, ConfigSteckt) {
    C c;
    c.aup = true;
    A5120Machine m(c);
    ASSERT_NE(m.aup(), nullptr);
    EXPECT_EQ(m.ioReadDebug(0x41), 0x00);
    EXPECT_FALSE(m.installAup()) << "doppelt";
    EXPECT_EQ(m.aupFehler(), "Es steckt bereits eine AUP");
}

TEST(AupMaschine, InstallNurVorDemErstenLauf) {
    A5120Machine a;
    EXPECT_TRUE(a.installAup(AUP::Alpha::Mit)) << a.aupFehler();
    EXPECT_EQ(a.aup()->config().alpha, AUP::Alpha::Mit);
    A5120Machine b;
    b.powerOn();
    b.run(1000);
    EXPECT_FALSE(b.installAup());
    EXPECT_EQ(b.aup(), nullptr);
}

TEST(AupMaschine, Konfigurationstext) {
    C c;
    std::string err;
    ASSERT_TRUE(a5120konfig::text(c, "aup=1;aup_alpha=ohne", err)) << err;
    EXPECT_TRUE(c.aup);
    EXPECT_EQ(c.aup_alpha, AUP::Alpha::Ohne);
    C d;
    EXPECT_FALSE(a5120konfig::text(d, "aup_alpha=mit", err));
    EXPECT_EQ(err, "aup_alpha: nur mit aup=1");
    C e;
    EXPECT_FALSE(a5120konfig::text(e, "aup=2", err));
}

TEST(AupMaschine, ResetSetztKommandoZurueck) {
    C c;
    c.aup = true;
    A5120Machine m(c);
    m.powerOn();
    m.aup()->ioWrite(0x43, 0xCC);
    EXPECT_EQ(m.aup()->kommando(), 0xCC);
    m.reset();
    EXPECT_EQ(m.aup()->kommando(), 0xFF);
}

// ─── CP/A-Gastlauf mit AUPTEST.COM ───────────────────────────────────────────
namespace {
using k1520test::runSmallUntil;
using k1520test::runSmallUntilVisible;
using k1520test::runUntilVramContains;
using k1520test::typeKey;
using k1520test::typeString;
using k1520test::vramLines;

constexpr long long kBootBudget = 90'000'000;
constexpr long long kFrist      = 40'000'000;

bool comAufDiskette(const std::string& disk, const char* quelle = K1520_AUPTEST_COM,
                    const char* name = "AUPTEST.COM") {
    std::string fe, err;
    static const FormatCatalog fk = FormatCatalog::loadDefault(&fe);
    static const FsCatalog     fs = FsCatalog::loadDefault(fk, &fe);
    auto vol = DiskVolume::open(disk, "cpa780", fk, fs, err, /*read_only=*/false);
    if (!vol) return false;
    FileRef ref;
    ref.name = name;
    return vol->insert(quelle, ref, TransferOptions{}) && vol->flush();
}

void kaltstart(A5120Machine& m) {
    m.powerOn();
    ASSERT_TRUE(runUntilVramContains(m, "TPA ist OK!", kBootBudget)) << vramLines(m);
    ASSERT_TRUE(runSmallUntilVisible(m, "A>", kBootBudget)) << vramLines(m);
    k1520test::runCycles(m, 5'000'000);
}

void starte(A5120Machine& m) {
    typeString(m, "auptest");
    typeKey(m, k1520test::QK_RETURN);
    ASSERT_TRUE(runSmallUntil(m, "AUPTEST V0.1", kFrist)) << vramLines(m);
}
}  // namespace

TEST(AupMaschine, CpaGastZeigtCodesUndSchaltetLampen) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk disk("cpa_cpa780_k5601_noclock.img", "k1520_aup_cpa.img");
    ASSERT_TRUE(comAufDiskette(disk.path()));
    C c;
    c.aup = true;
    A5120Machine m(c);
    ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
    ASSERT_NO_FATAL_FAILURE(kaltstart(m));
    ASSERT_NO_FATAL_FAILURE(starte(m));
    ASSERT_TRUE(runSmallUntil(m, "Q = ENDE", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->kommando(), 0xFF) << "Programm setzt die AUP zurueck";

    // K7633 F01 (ohne Alpha C0H), Kundentastatur '5', Schluesselschalter SS1.
    ASSERT_TRUE(m.aup()->taste(T::K7633, 0x47, true));
    m.aup()->taste(T::K7633, 0x47, false);
    EXPECT_TRUE(runSmallUntil(m, "TT C0", kFrist)) << vramLines(m);
    m.aup()->taste(T::Kunde, 0x40, true);
    m.aup()->taste(T::Kunde, 0x40, false);
    EXPECT_TRUE(runSmallUntil(m, "KT 35 '5'", kFrist)) << vramLines(m);
    m.aup()->schluessel(1, true);
    EXPECT_TRUE(runSmallUntil(m, "TT AA", kFrist)) << vramLines(m);

    // Lampen ueber die Rechnertastatur: S = SEL, A = ALPHA (danach Tabelle "mit Alpha").
    typeString(m, "s");
    EXPECT_TRUE(runSmallUntil(m, "KMD FE", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->lampen(), AUP::kLampeSel);
    typeString(m, "a");
    EXPECT_TRUE(runSmallUntil(m, "KMD FC", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->lampen(), AUP::kLampeSel | AUP::kLampeAlpha);
    m.aup()->taste(T::K7633, 0x47, true);
    m.aup()->taste(T::K7633, 0x47, false);
    EXPECT_TRUE(runSmallUntil(m, "TT 41 'A'", kFrist)) << vramLines(m);
    const uint32_t h0 = m.aup()->hupe();
    typeString(m, "h");
    EXPECT_TRUE(runSmallUntil(m, "KMD BC", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->hupe(), h0 + 1);
    typeString(m, "q");
    EXPECT_TRUE(runSmallUntil(m, "AUPTEST ENDE", kFrist)) << vramLines(m);
    EXPECT_EQ(m.aup()->kommando(), 0xFF);
    EXPECT_EQ(m.aup()->lampen(), 0);
}

TEST(AupMaschine, CpaGastOhneKarteMeldetEs) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk disk("cpa_cpa780_k5601_noclock.img", "k1520_aup_cpa_ohne.img");
    ASSERT_TRUE(comAufDiskette(disk.path()));
    A5120Machine m;
    ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
    ASSERT_NO_FATAL_FAILURE(kaltstart(m));
    ASSERT_NO_FATAL_FAILURE(starte(m));
    EXPECT_TRUE(runSmallUntil(m, "Keine AUP auf 40H-43H (IN 41H = FFH)", kFrist)) << vramLines(m);
}

// ─── Sonderschnittstelle (N10d) ──────────────────────────────────────────────
namespace {
using namespace k1520::serial;

C cfgSonder() {
    C c;
    c.aup = true;
    c.aup_sonder = true;
    return c;
}

/// CTC-Kanal 0 / SIO-Kanal B wie das Gastprogramm: 9600 Bd 8N1, Senden und Empfangen frei.
void sondeEinrichten(A5120Machine& m) {
    AUP& a = *m.aup();
    a.ioWrite(0xB0, 0x05);
    a.ioWrite(0xB0, 0x01);
    for (auto rw : {std::pair<int, int>{0x18, 0}, {4, 0x44}, {3, 0xC1}, {5, 0x6A}}) {
        a.ioWrite(0xB7, uint8_t(rw.first));
        if (rw.first != 0x18) a.ioWrite(0xB7, uint8_t(rw.second));
    }
}

bool bisEmpfang(A5120Machine& m, long long frist) {
    for (long long d = 0; d < frist; d += m.run(20'000))
        if (m.aup()->ioRead(0xB7) & 1) return true;
    return false;
}
}  // namespace

TEST(AupSonder, OhneOptionBleibtDieMaschineWieSie_War) {
    A5120Machine a;
    EXPECT_EQ(a.serielleAnschluesse().size(), 3u);
    EXPECT_EQ(a.serialHub()->anzahl(), 3);
    C c;
    c.aup = true;   // Tastaturteil allein: weiter drei Schnittstellen, B0H-B7H frei
    A5120Machine b(c);
    EXPECT_EQ(b.serielleAnschluesse().size(), 3u);
    EXPECT_FALSE(b.aup()->sonder());
    for (int p = 0xB0; p <= 0xB7; ++p) EXPECT_EQ(b.ioReadDebug(uint8_t(p)), 0xFF) << p;
    // Ohne Karte dasselbe.
    for (int p = 0xB0; p <= 0xB7; ++p) EXPECT_EQ(a.ioReadDebug(uint8_t(p)), 0xFF) << p;
}

TEST(AupSonder, ConfigSteckt) {
    A5120Machine m(cfgSonder());
    ASSERT_NE(m.aup(), nullptr);
    EXPECT_TRUE(m.aup()->sonder());
    ASSERT_EQ(m.serielleAnschluesse().size(), 4u);
    EXPECT_EQ(m.serialHub()->anzahl(), 4);
    auto* a = m.serielleAnschluesse()[3];
    EXPECT_STREQ(a->name(), "Sonderschnittstelle");
    EXPECT_STREQ(a->stecker(), "X2");
    EXPECT_EQ(m.serialHub()->info(3).name, "Sonderschnittstelle");
    EXPECT_FALSE(m.serialHub()->info(3).v24);
    // Die drei K8025-Schnittstellen behalten ihren Index.
    EXPECT_NE(std::string(m.serielleAnschluesse()[0]->name()).find("V.24"), std::string::npos);
    EXPECT_STREQ(m.serielleAnschluesse()[2]->name(), "Drucker");
    EXPECT_FALSE(m.installAup(AUP::Alpha::NachLed, true)) << "doppelt";
}

TEST(AupSonder, InstallNurVorDemErstenLauf) {
    A5120Machine a;
    EXPECT_TRUE(a.installAup(AUP::Alpha::NachLed, true)) << a.aupFehler();
    EXPECT_EQ(a.serielleAnschluesse().size(), 4u);
    A5120Machine b;
    b.powerOn();
    b.run(1000);
    EXPECT_FALSE(b.installAup(AUP::Alpha::NachLed, true));
    EXPECT_EQ(b.serielleAnschluesse().size(), 3u);
}

TEST(AupSonder, Konfigurationstext) {
    C c;
    std::string err;
    ASSERT_TRUE(a5120konfig::text(c, "aup=1;aup_sonder=1", err)) << err;
    EXPECT_TRUE(c.aup);
    EXPECT_TRUE(c.aup_sonder);
    C d;
    EXPECT_FALSE(a5120konfig::text(d, "aup_sonder=1", err));
    EXPECT_EQ(err, "aup_sonder: nur mit aup=1");
    C e;
    EXPECT_FALSE(a5120konfig::text(e, "aup=1;aup_sonder=2", err));
    C f;
    ASSERT_TRUE(a5120konfig::text(f, "aup=1;aup_sonder=0", err)) << err;
    EXPECT_FALSE(f.aup_sonder);
}

TEST(AupSonder, BelegtesTorVerhindertDasStecken) {
    A5120Machine a;
    // Ein Karte auf B0H-B7H: die RAF liegt auf 88H/89H, die K6022 auf E0H — beide kollidieren nicht;
    // deshalb ein Fremdgeraet von Hand.
    struct Fremd : BusDevice {
        uint8_t ioRead(uint8_t) override { return 0; }
        void ioWrite(uint8_t, uint8_t) override {}
        const char* deviceName() const override { return "Fremd"; }
    } fremd;
    a.bus().registerIO(&fremd, 0xB4, 1);
    EXPECT_FALSE(a.installAup(AUP::Alpha::NachLed, true));
    EXPECT_EQ(a.aupFehler(), "E/A-Tor B4H belegt (Fremd)");
    EXPECT_EQ(a.aup(), nullptr) << "nichts halb gesteckt";
    EXPECT_EQ(a.serielleAnschluesse().size(), 3u);
}

// Prioritaet in der Kette: K8025 vor der AUP (Annahme A10), AUP vor der ZRE.
TEST(AupSonder, KetteLiegtHinterDerK8025) {
    A5120Machine m(cfgSonder());
    auto& bus = m.bus();
    AUP& a = *m.aup();
    // AUP-SIO: Empfang mit Interrupt, Vektor 20H.
    a.ioWrite(0xB7, 2); a.ioWrite(0xB7, 0x20);
    a.ioWrite(0xB7, 1); a.ioWrite(0xB7, 0x18);
    a.ioWrite(0xB7, 3); a.ioWrite(0xB7, 0xC1);
    // K8025-SIO A33 Kanal A (DFUE/V.24): Empfang mit Interrupt, Vektor 10H (WR2 ueber Kanal B, 53H).
    bus.ioWrite(0x53, 2); bus.ioWrite(0x53, 0x10);
    bus.ioWrite(0x51, 1); bus.ioWrite(0x51, 0x18);
    bus.ioWrite(0x51, 3); bus.ioWrite(0x51, 0xC1);
    a.anschluss()->empfange(0x22);
    m.dfueSend(0x11);
    bus.markIntDirty();
    bus.updateInterruptChain();
    ASSERT_TRUE(bus.isINT());
    EXPECT_EQ(bus.interruptAcknowledge() & 0xF0, 0x10) << "K8025 zuerst";
    EXPECT_EQ(bus.ioRead(0x50), 0x11);
    bus.signalRETI();
    ASSERT_TRUE(bus.isINT());
    EXPECT_EQ(bus.interruptAcknowledge() & 0xF0, 0x20) << "dann die AUP";
    EXPECT_EQ(bus.ioRead(0xB6), 0x22);
    bus.signalRETI();
    EXPECT_FALSE(bus.isINT());
}

TEST(AupSonder, InDerKetteMitErweiterungsmodulVorDemEm) {
    C c = cfgSonder();
    c.em = C::Em::em256;
    A5120Machine m(c);
    AUP& a = *m.aup();
    a.ioWrite(0xB7, 2); a.ioWrite(0xB7, 0x20);
    a.ioWrite(0xB7, 1); a.ioWrite(0xB7, 0x18);
    a.ioWrite(0xB7, 3); a.ioWrite(0xB7, 0xC1);
    a.anschluss()->empfange(0x33);
    m.bus().markIntDirty();
    m.bus().updateInterruptChain();
    ASSERT_TRUE(m.bus().isINT());
    EXPECT_EQ(m.bus().interruptAcknowledge() & 0xF0, 0x20);
}

// Schleife SIO B -> Hub (Loop): was der Gast sendet, kommt im Empfaenger an.
TEST(AupSonder, LoopSendetUndEmpfaengt) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    A5120Machine m(cfgSonder());
    SerialKonfig k = m.serialHub()->konfig(3);
    k.loop = true;
    ASSERT_TRUE(m.serialHub()->konfigurieren(3, k));
    m.powerOn();
    sondeEinrichten(m);
    const std::string text = "HALLO";
    for (char c : text) {
        while (!(m.aup()->ioRead(0xB7) & 0x04)) m.run(5'000);
        m.aup()->ioWrite(0xB6, uint8_t(c));
    }
    std::string rx;
    for (size_t i = 0; i < text.size(); ++i) {
        ASSERT_TRUE(bisEmpfang(m, 3'000'000)) << "Zeichen " << i << " bisher [" << rx << "]";
        rx += char(m.aup()->ioRead(0xB6));
    }
    EXPECT_EQ(rx, text);
    const SerialStatus st = m.serialHub()->status(3);
    EXPECT_TRUE(st.format_gueltig);
    EXPECT_EQ(st.baud_nenn, 9600u);
}

// Datei: das Gesendete landet in der Datei (gleiche Zeichenzeit des Wandlers).
TEST(AupSonder, DateiTraegtDasGesendete) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    const std::string pfad = k1520test::tempPath("aup_sonder_ausgabe.txt");
    std::filesystem::remove(std::filesystem::u8path(pfad));
    A5120Machine m(cfgSonder());
    SerialKonfig k = m.serialHub()->konfig(3);
    k.betriebsart = Betriebsart::Datei;
    k.datei = pfad;
    k.loop = false;
    ASSERT_TRUE(m.serialHub()->konfigurieren(3, k));
    ASSERT_TRUE(m.serialHub()->start(3));
    m.powerOn();
    sondeEinrichten(m);
    for (char c : std::string("AUP-SIO")) {
        while (!(m.aup()->ioRead(0xB7) & 0x04)) m.run(5'000);
        m.aup()->ioWrite(0xB6, uint8_t(c));
    }
    for (int i = 0; i < 40; ++i) m.run(100'000);   // Zeichenzeiten abwarten
    m.serialHub()->stop(3);
    std::ifstream f(std::filesystem::u8path(pfad), std::ios::binary);
    const std::string aus((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_EQ(aus, "AUP-SIO");
}

TEST(AupSonder, ResetSetztSioUndCtcZurueck) {
    A5120Machine m(cfgSonder());
    m.powerOn();
    sondeEinrichten(m);
    EXPECT_EQ(m.aup()->sio()->channelB().format().teiler, 16);
    EXPECT_EQ(m.aup()->ctc()->teilerTakte(0), 16u);
    m.reset();
    EXPECT_EQ(m.aup()->sio()->channelB().format().teiler, 1);
    EXPECT_EQ(m.aup()->ctc()->teilerTakte(0), 0u);
}

// CP/A-Gastlauf mit AUPTSIO.COM: Gast programmiert CTC/SIO ueber B0H-B7H, sendet "HALLO",
// im Loop kommt es zurueck.
TEST(AupSonder, CpaGastSendetUndEmpfaengtImLoop) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk disk("cpa_cpa780_k5601_noclock.img", "k1520_aup_sonder_cpa.img");
    ASSERT_TRUE(comAufDiskette(disk.path(), K1520_AUPTSIO_COM, "AUPTSIO.COM"));
    A5120Machine m(cfgSonder());
    SerialKonfig k = m.serialHub()->konfig(3);
    k.loop = true;
    ASSERT_TRUE(m.serialHub()->konfigurieren(3, k));
    ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
    ASSERT_NO_FATAL_FAILURE(kaltstart(m));
    typeString(m, "auptsio");
    typeKey(m, k1520test::QK_RETURN);
    ASSERT_TRUE(runSmallUntil(m, "AUPTSIO V0.1", kFrist)) << vramLines(m);
    EXPECT_TRUE(runSmallUntil(m, "AUPTSIO OK", kFrist)) << vramLines(m);
    EXPECT_TRUE(runSmallUntil(m, "EMPF HALLO", kFrist)) << vramLines(m);
}

// Ohne Loop und ohne Gegenstelle: gesendet wird (Datei), empfangen nichts -> "KEIN EMPFANG".
TEST(AupSonder, CpaGastOhneGegenstelleMeldetKeinenEmpfang) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk disk("cpa_cpa780_k5601_noclock.img", "k1520_aup_sonder_cpa2.img");
    ASSERT_TRUE(comAufDiskette(disk.path(), K1520_AUPTSIO_COM, "AUPTSIO.COM"));
    const std::string pfad = k1520test::tempPath("aup_sonder_gast.txt");
    std::filesystem::remove(std::filesystem::u8path(pfad));
    A5120Machine m(cfgSonder());
    SerialKonfig k = m.serialHub()->konfig(3);
    k.betriebsart = Betriebsart::Datei;
    k.datei = pfad;
    k.loop = false;
    ASSERT_TRUE(m.serialHub()->konfigurieren(3, k));
    ASSERT_TRUE(m.serialHub()->start(3));
    ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
    ASSERT_NO_FATAL_FAILURE(kaltstart(m));
    typeString(m, "auptsio");
    typeKey(m, k1520test::QK_RETURN);
    ASSERT_TRUE(runSmallUntil(m, "AUPTSIO KEIN EMPFANG", kFrist)) << vramLines(m);
    k1520test::runCycles(m, 5'000'000);
    m.serialHub()->stop(3);
    std::ifstream f(std::filesystem::u8path(pfad), std::ios::binary);
    const std::string aus((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_EQ(aus, "HALLO");
}

TEST(AupSonder, CpaGastOhneKarteMeldetEs) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR);
    k1520test::TempDisk disk("cpa_cpa780_k5601_noclock.img", "k1520_aup_sonder_cpa3.img");
    ASSERT_TRUE(comAufDiskette(disk.path(), K1520_AUPTSIO_COM, "AUPTSIO.COM"));
    C c;
    c.aup = true;   // Tastaturteil ohne Sonderschnittstelle
    A5120Machine m(c);
    ASSERT_TRUE(m.mountDisk(0, disk.path(), "cpa780", false)) << m.lastError();
    ASSERT_NO_FATAL_FAILURE(kaltstart(m));
    typeString(m, "auptsio");
    typeKey(m, k1520test::QK_RETURN);
    EXPECT_TRUE(runSmallUntil(m, "Keine Sonderschnittstelle (IN B7H = FFH)", kFrist)) << vramLines(m);
}
