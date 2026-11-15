/**
 * @file test_sound_system.cpp
 * @brief SOUND-Karte K1520 mit der ORIGINAL-Gastsoftware (Entwurf 51, AP S7): TGCPM, SKTEST4, STCTEST1,
 *        PLAYSTC unter CP/A und SCPX am A5120 und SKTEST4 unter CP/A 1715; Karte immer 38H (die PC-1715-Fassung
 *        der Archive benutzt 38H/3CH, nur der Interruptvektor ist ein anderer, F7F0H; E0H ist die Vorgabe der
 *        USB-Player mit SOUND.INI, nicht dieser Programme).
 *
 * Die Programme (`tools/sound/fremd/{a5120,pc1715}/`, bytegleich aus den Archiven in `tools/sound/fremd/original/`)
 * werden mit dem DiskTool auf KOPIEN der Systemdisketten gespielt (`TempDisk`), der Rechner bootet,
 * die Programme werden ueber die Tastatur bedient.  Gemessen wird die WAV-Mitschrift der Karte
 * (Maschinenzeit, 48 kHz) und die Zahl der CTC-Interrupts der Karte je Sekunde Maschinenzeit.
 *
 * Tastenfolgen (aus den .ASM-Quellen der Archive):
 *   TGCPM    : Taste (Takt: A 1,75 / B 1,79 / C 2,00 MHz) -> `B`, dann `A` = Ton Kanal A (Periode 6FH), `0` = aus
 *   SKTEST4  : beliebige Taste (Hinweisseite), `B` (Takt), Menue `1` Ton / `4` Lied ueber CTC-Interrupt, `0` Abbruch
 *   STCTEST1 : keine Eingabe; Lied (2 Wiederholungen) laeuft im Hintergrund, der Prompt kommt zurueck
 *   PLAYSTC  : `PLAYSTC SONG.STC`
 *
 * Je Fall 2-5 s (Kaltstart in Release): kein Langsam-Label; Fachlabel `fach_sound` fuer tools/test_auswahl.py.
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/pc1715/pc1715.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "core/util/os_compat.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/pc1715_input.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"

#ifndef K1520_SOUND_FIXTURES
#error "K1520_SOUND_FIXTURES fehlt — k1520_add_test(... DEFS K1520_SOUND_FIXTURES=\"…\")"
#endif

using k1520::SoundK1520;
using k1520test::QK_RETURN;
using k1520test::TempDisk;

namespace {

const std::string kFix = K1520_SOUND_FIXTURES;

struct Datei { std::string host, name; };

/// Dateien mit dem DiskTool auf die Diskette @p pfad schreiben; leer = Erfolg.
std::string aufspielen(const std::string& pfad, const std::vector<Datei>& dateien) {
    std::string f;
    static const FormatCatalog formate = FormatCatalog::loadDefault(&f);
    static const FsCatalog     fs      = FsCatalog::loadDefault(formate, &f);
    std::string err;
    auto v = DiskVolume::open(pfad, "", formate, fs, err, /*read_only=*/false);
    if (!v) return "DiskVolume::open: " + err;
    v->setBackup(false);
    TransferOptions o;
    o.overwrite = true;
    for (const auto& d : dateien)
        if (!v->insert(kFix + "/" + d.host, FileRef::parse(d.name), o)) return "insert " + d.name + ": " + v->lastError();
    if (!v->flush()) return "flush: " + v->lastError();
    return {};
}

const std::vector<Datei> kA5120Dateien = {
    {"a5120/TGCPM.COM", "TGCPM.COM"},     {"a5120/SKTEST4.COM", "SKTEST4.COM"}, {"a5120/STCTEST1.COM", "STCTEST1.COM"},
    {"a5120/PLAYSTC.COM", "PLAYSTC.COM"}, {"a5120/SONG.STC", "SONG.STC"},
};
const std::vector<Datei> kPcDateien = {
    {"pc1715/SKTEST4.COM", "SKTEST4.COM"}, {"pc1715/STCTEST1.COM", "STCTEST1.COM"},
};

// ---- WAV auswerten ----------------------------------------------------------------------------

struct Wav {
    std::vector<float> l, r;
    bool lade(const std::string& pfad) {
        std::ifstream f(pfad, std::ios::binary);
        if (!f.good()) return false;
        std::vector<char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (d.size() < 44) return false;
        const size_t n = (d.size() - 44) / 4;
        for (size_t i = 0; i < n; ++i) {
            int16_t a, b;
            std::memcpy(&a, &d[44 + 4 * i], 2);
            std::memcpy(&b, &d[44 + 4 * i + 2], 2);
            l.push_back(a);
            r.push_back(b);
        }
        return true;
    }
};

double effektiv(const std::vector<float>& v, size_t a, size_t b) {
    b = std::min(b, v.size());
    if (a >= b) return 0;
    double m = 0, q = 0;
    for (size_t i = a; i < b; ++i) m += v[i];
    m /= double(b - a);
    for (size_t i = a; i < b; ++i) q += (v[i] - m) * (v[i] - m);   // Wechselanteil
    return std::sqrt(q / double(b - a));
}

/// Frequenz aus den Nulldurchgaengen (steigend) um den Mittelwert; 0 = keine.
double frequenz(const std::vector<float>& v, size_t a, size_t b) {
    b = std::min(b, v.size());
    if (a + 100 >= b) return 0;
    double mittel = 0;
    for (size_t i = a; i < b; ++i) mittel += v[i];
    mittel /= double(b - a);
    int n = 0;
    size_t erster = 0, letzter = 0;
    for (size_t i = a + 1; i < b; ++i)
        if (v[i - 1] < mittel && v[i] >= mittel) { if (!n) erster = i; letzter = i; ++n; }
    if (n < 50) return 0;
    return double(n - 1) * 48000.0 / double(letzter - erster);
}

// ---- Fahrer je Maschine -----------------------------------------------------------------------

struct A5120Fahrer {
    A5120Machine m;
    void laufe(long long t) { k1520test::runCycles(m, t); }
    bool bis(const std::string& s, long long grenze) { return k1520test::runSmallUntil(m, s, grenze, 100'000); }
    void tippe(const std::string& s) { k1520test::typeString(m, s); }
    void zeile(const std::string& s) { k1520test::typeString(m, s); k1520test::typeKey(m, QK_RETURN); }
    /// Eine Bildzeile ist genau @p prompt (der Prompt ist wieder da; "A>BEFEHL" zaehlt nicht).
    bool promptDa(const std::string& prompt) {
        const std::string b = k1520test::vramLines(m);
        size_t p = 0;
        while (p < b.size()) {
            size_t e = b.find('\n', p);
            if (e == std::string::npos) e = b.size();
            std::string z = b.substr(p, e - p);
            while (!z.empty() && z.back() == ' ') z.pop_back();
            if (z == prompt) return true;
            p = e + 1;
        }
        return false;
    }
    bool bisPrompt(const std::string& prompt, long long grenze) {
        for (long long t = 0; t < grenze; t += 100'000) {
            k1520test::runCycles(m, 100'000);
            if (promptDa(prompt)) return true;
        }
        return false;
    }
    std::string bild() { return k1520test::vramLines(m); }
};

struct PcFahrer {
    Pc1715Machine m;
    void laufe(long long t) { k1520test::pc1715::laufe(m, t); }
    bool bis(const std::string& s, long long grenze) { return k1520test::pc1715::laufeBisText(m, s, grenze); }
    void tippe(const std::string& s) { k1520test::pc1715::tippe(m, s); }
    void zeile(const std::string& s) { k1520test::pc1715::tippeZeile(m, s); }
    bool bisPrompt(const std::string& prompt, long long grenze) {
        return k1520test::pc1715::laufeBisPrompt(m, prompt, grenze);
    }
    std::string bild() { return k1520test::pc1715::bild(m); }
};

/// Eine Maschine mit gesteckter Karte, laufender WAV-Mitschrift und CTC-Zaehler.
template <class F>
struct Sys {
    F f;
    TempDisk disk;
    std::string wav;
    std::shared_ptr<long> irq = std::make_shared<long>(0);   // Zeitgeber-Ablaeufe der Karten-CTC, Kanal 0

    Sys(const char* fixture, const char* tempname) : disk(fixture, tempname) {}

    ~Sys() {
        f.m.audioWavStop();
        std::error_code ec;
        if (!wav.empty()) std::filesystem::remove(wav, ec);
    }

    /// Karte stecken (Basis @p basis), Zaehler einhaengen.  Vor dem ersten Lauf.
    /// @p irqZaehlen = false: die Karte bleibt unberuehrt (der Zaehlrueckruf wuerde die Kaskade der Karte ersetzen).
    bool karte(uint8_t basis, bool irqZaehlen = true) {
        SoundK1520::Config c;
        c.basis = basis;                      // Takt Q1/2 (Vorgabe), ABC
        if (!f.m.installSound(c)) return false;
        if (!irqZaehlen) return true;
        auto n = irq;
        f.m.soundK1520()->ctc().setZCTOCallback([n](int kanal, bool pegel) { if (kanal == 0 && pegel) ++*n; }, 0x01);
        return true;
    }
    bool wavAn(const char* name) {
        wav = k1520test::tempPath(name);
        return f.m.audioWavStart(wav);
    }
    /// 1 s Maschinenzeit = nenntaktHz() Takte, in 5000er-Batches (Tastatur beteiligt).
    void sekunden(double s) {
        const long long soll = (long long)(s * f.m.nenntaktHz());
        long long t = 0;
        while (t < soll) t += f.m.run(5000);
    }
    uint64_t frames() { return f.m.audioWavFrames(); }
};

struct Messung { double eff_l, eff_r, hz; };

/// Fenster [a, b) der geschlossenen WAV-Datei.
Messung messe(Sys<A5120Fahrer>& s, uint64_t a, uint64_t b) {
    s.f.m.audioWavStop();
    Wav w;
    EXPECT_TRUE(w.lade(s.wav));
    return {effektiv(w.l, a, b), effektiv(w.r, a, b), frequenz(w.l, a, b)};
}
Messung messe(Sys<PcFahrer>& s, uint64_t a, uint64_t b) {
    s.f.m.audioWavStop();
    Wav w;
    EXPECT_TRUE(w.lade(s.wav));
    return {effektiv(w.l, a, b), effektiv(w.r, a, b), frequenz(w.l, a, b)};
}

/// Schwelle "nicht still": Wechselanteil mindestens so gross (int16-Stufen; ein Vollton A liegt bei ~10000+).
constexpr double kNichtStill = 500.0;

/// A5120 + CP/A (Fixture ohne Uhr): bis zum Prompt.
void bootCpa(Sys<A5120Fahrer>& s, bool irqZaehlen = true) {
    ASSERT_EQ(aufspielen(s.disk.path(), kA5120Dateien), "");
    ASSERT_TRUE(s.f.m.mountDisk(0, s.disk.path(), "cpa780", /*wp=*/false)) << s.f.m.lastError();
    ASSERT_TRUE(s.karte(0x38, irqZaehlen)) << s.f.m.soundFehler();
    s.f.m.powerOn();
    ASSERT_TRUE(k1520test::runSmallUntilVisible(s.f.m, "A>", 90'000'000)) << s.f.bild();
    s.f.laufe(2'000'000);
    ASSERT_TRUE(s.wavAn("sound_system_cpa.wav")) << s.f.m.audioFehler();
}

/// A5120 + SCPX 1.7: bis zum Prompt.
void bootScpx(Sys<A5120Fahrer>& s) {
    ASSERT_EQ(aufspielen(s.disk.path(), kA5120Dateien), "");
    ASSERT_TRUE(s.f.m.mountDisk(0, s.disk.path(), "cpa780", /*wp=*/false)) << s.f.m.lastError();
    ASSERT_TRUE(s.karte(0x38)) << s.f.m.soundFehler();
    s.f.m.powerOn();
    ASSERT_TRUE(k1520test::runSmallUntil(s.f.m, "SCPX 1526 - V 1.7", 40'000'000)) << s.f.bild();
    ASSERT_TRUE(k1520test::runSmallUntilVisible(s.f.m, "A>", 5'000'000)) << s.f.bild();
    s.f.laufe(2'000'000);
    ASSERT_TRUE(s.wavAn("sound_system_scpx.wav")) << s.f.m.audioFehler();
}

/// PC 1715 + CP/A 1715, Karte 38H (so ist die Fassung aus K1520_SOUND_PC1715_CPA.zip gebaut): bis zum Prompt.
void bootPc(Sys<PcFahrer>& s) {
    ASSERT_EQ(aufspielen(s.disk.path(), kPcDateien), "");
    ASSERT_TRUE(s.f.m.mountDisk(0, s.disk.path(), s.f.m.defaultFormatName(0), false)) << s.f.m.lastError();
    ASSERT_TRUE(s.karte(0x38)) << s.f.m.soundFehler();
    s.f.m.powerOn();
    ASSERT_TRUE(s.f.bisPrompt("A>", 40'000'000)) << s.f.bild();
    s.f.laufe(2'000'000);
    ASSERT_TRUE(s.wavAn("sound_system_pc1715.wav")) << s.f.m.audioFehler();
}

/// SKTEST4 bis zum Testmenue: Hinweisseite, Takt B.
template <class S>
void sktestMenue(S& s) {
    s.f.zeile("SKTEST4");
    ASSERT_TRUE(s.f.bis("STANDARD IO-ADRESSE", 20'000'000)) << s.f.bild();
    s.f.laufe(2'000'000);
    s.f.tippe("x");                                            // WEITER mit Taste
    ASSERT_TRUE(s.f.bis("AUSWAHL DER TAKTFREQUENZ", 5'000'000)) << s.f.bild();
    s.f.laufe(1'000'000);
    s.f.tippe("b");                                            // 1,79 MHz (Periode 6FH)
    ASSERT_TRUE(s.f.bis("AUSWAHL DES TESTS", 5'000'000)) << s.f.bild();
    s.f.laufe(1'000'000);
}

/// SKTEST4 bis zum laufenden Lied (Menue 4): Hinweisseite, Takt B, Test 4.
template <class S>
void sktestLied(S& s) {
    sktestMenue(s);
    s.f.tippe("4");                                            // Lied ueber CTC-Interrupt
    ASSERT_TRUE(s.f.bis("Song wird gespielt", 20'000'000)) << s.f.bild();
}

/// 2 s Maschinenzeit nach dem Start des Liedes: nicht still, CTC-Interrupts ~ 50/s.
template <class S>
void liedPruefen(S& s) {
    s.sekunden(0.2);                                           // Anlauf
    const uint64_t a = s.frames();
    const long n0 = *s.irq;
    s.sekunden(2.0);
    const long n = *s.irq - n0;
    const uint64_t b = s.frames();
    const Messung me = messe(s, a, b);
    std::printf("[S7] Lied: Interrupts in 2 s = %ld, RMS links %.0f rechts %.0f\n", n, me.eff_l, me.eff_r);
    EXPECT_NEAR(n, 100, 2) << "CTC-Interrupts der Karte je 2 s Maschinenzeit (50/s)";
    EXPECT_GT(std::max(me.eff_l, me.eff_r), kNichtStill) << "Lied stumm";
}

}  // namespace

// 1) TGCPM: Takt B, Ton Kanal A -> 1 kHz (Periode 6FH bei 1,79 MHz = 1007,8 Hz), nur linker Kanal (A liegt in ACB wie in ABC links).
TEST(SoundSystem, A5120_Cpa_TGCPM_Ton1kHzNurLinks) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN);
    k1520::os::setEnv("K1520_AUDIO", "stumm");
    Sys<A5120Fahrer> s("cpa_cpa780_k5601_noclock.hfe", "k1520_sound_tgcpm.hfe");
    bootCpa(s);
    s.f.zeile("TGCPM");
    ASSERT_TRUE(s.f.bis("AUSWAHL TAKTFREQUENZ", 20'000'000)) << s.f.bild();
    s.f.laufe(1'000'000);
    s.f.tippe("b");
    ASSERT_TRUE(s.f.bis("A=RECHTS", 5'000'000)) << s.f.bild();
    s.f.laufe(1'000'000);
    s.f.tippe("a");                                            // Ton Kanal A
    s.sekunden(0.1);
    const uint64_t a = s.frames();
    s.sekunden(1.0);
    const uint64_t b = s.frames();
    const Messung me = messe(s, a, b);
    std::printf("[S7] TGCPM: %.1f Hz, RMS links %.0f rechts %.0f\n", me.hz, me.eff_l, me.eff_r);
    EXPECT_NEAR(me.hz, 1007.8, 1007.8 * 0.01);
    EXPECT_GT(me.eff_l, kNichtStill);
    EXPECT_LT(me.eff_r, me.eff_l * 0.1) << "Kanal A liegt in ACB (wie in ABC) nur links";
}

// 2) SKTEST4 `B`, Menue 4: Lied ueber CTC-Interrupt.
TEST(SoundSystem, A5120_Cpa_SKTEST4_LiedMitCtcInterrupt) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN);
    k1520::os::setEnv("K1520_AUDIO", "stumm");
    Sys<A5120Fahrer> s("cpa_cpa780_k5601_noclock.hfe", "k1520_sound_sktest4.hfe");
    bootCpa(s);
    sktestLied(s);
    liedPruefen(s);
}

// 2b) SKTEST4 `B`, Menue 3: kaskadierte CTC-Kanaele.  Das Programm wartet auf den ersten Zaehlimpuls von Kanal 3 und
// fragt die Taste "0" erst DANACH ab -- ohne die drei Kaskadenbruecken haengt es.  Die Karte kommt mit der Vorgabe
// (alle Bruecken gesteckt) in die Maschine, nichts wird umgestellt.
TEST(SoundSystem, A5120_Cpa_SKTEST4_Menue3KaskadeHaengtNichtMitVorgabe) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN);
    k1520::os::setEnv("K1520_AUDIO", "stumm");
    Sys<A5120Fahrer> s("cpa_cpa780_k5601_noclock.hfe", "k1520_sound_sktest4.hfe");
    bootCpa(s, /*irqZaehlen=*/false);
    ASSERT_EQ(s.f.m.soundK1520()->config().kaskade, 7);
    s.f.zeile("SKTEST4");
    ASSERT_TRUE(s.f.bis("STANDARD IO-ADRESSE", 20'000'000)) << s.f.bild();
    s.f.laufe(2'000'000);
    s.f.tippe("x");
    ASSERT_TRUE(s.f.bis("AUSWAHL DER TAKTFREQUENZ", 5'000'000)) << s.f.bild();
    s.f.laufe(1'000'000);
    s.f.tippe("b");
    ASSERT_TRUE(s.f.bis("AUSWAHL DES TESTS", 5'000'000)) << s.f.bild();
    s.f.laufe(1'000'000);
    s.f.tippe("3");
    ASSERT_TRUE(s.f.bis("GELESENES BYTE", 20'000'000)) << s.f.bild();
    s.sekunden(3.0);                                           // Kanal 3 zaehlt etwa 2 Hz
    s.f.tippe("0");                                            // nur nach dem ersten Impuls wird die Taste gelesen
    EXPECT_TRUE(s.f.bis("AUSWAHL DES TESTS", 20'000'000)) << "Menue 3 haengt (Kaskade nicht gesteckt?)\n" << s.f.bild();
}

// 3) STCTEST1: Lied im Hintergrund, Prompt kommt zurueck, DIR bedienbar, waehrenddessen nicht still.
TEST(SoundSystem, A5120_Cpa_STCTEST1_HintergrundUndDir) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN);
    k1520::os::setEnv("K1520_AUDIO", "stumm");
    Sys<A5120Fahrer> s("cpa_cpa780_k5601_noclock.hfe", "k1520_sound_stctest1.hfe");
    bootCpa(s);
    const uint64_t a = s.frames();
    s.f.zeile("STCTEST1");
    ASSERT_TRUE(s.f.bisPrompt("A>", 20'000'000)) << "Prompt kam nicht zurueck\n" << s.f.bild();
    s.f.laufe(500'000);
    s.f.zeile("DIR");
    ASSERT_TRUE(s.f.bis("TGCPM", 20'000'000)) << "DIR zeigt kein Verzeichnis\n" << s.f.bild();
    // Nach dem DIR laeuft das Lied im Hintergrund weiter: eigenes Fenster (1 s) hinter der Verzeichnisausgabe.
    const uint64_t a2 = s.frames();
    const long n0 = *s.irq;
    s.sekunden(1.0);
    const long n = *s.irq - n0;
    const uint64_t b = s.frames();
    const Messung me = messe(s, a, b);
    Wav w;
    ASSERT_TRUE(w.lade(s.wav));
    const double nach = std::max(effektiv(w.l, a2, b), effektiv(w.r, a2, b));
    std::printf("[S7] STCTEST1 + DIR: RMS ganz links %.0f rechts %.0f (%.2f s); nach DIR max %.0f, Interrupts/s %ld\n",
                me.eff_l, me.eff_r, double(b - a) / 48000.0, nach, n);
    EXPECT_GT(std::max(me.eff_l, me.eff_r), kNichtStill);
    EXPECT_GT(nach, kNichtStill) << "Lied nach DIR still";
    EXPECT_NEAR(n, 50, 2) << "CTC-Interrupts nach DIR je Sekunde";
}

// 4) PLAYSTC SONG.STC
TEST(SoundSystem, A5120_Cpa_PLAYSTC_SpieltSong) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN);
    k1520::os::setEnv("K1520_AUDIO", "stumm");
    Sys<A5120Fahrer> s("cpa_cpa780_k5601_noclock.hfe", "k1520_sound_playstc.hfe");
    bootCpa(s);
    s.f.zeile("PLAYSTC SONG.STC");
    ASSERT_TRUE(s.f.bis("Song wird gespielt", 40'000'000)) << s.f.bild();
    const uint64_t a = s.frames();
    s.sekunden(2.0);
    const uint64_t b = s.frames();
    const Messung me = messe(s, a, b);
    std::printf("[S7] PLAYSTC: RMS links %.0f rechts %.0f\n", me.eff_l, me.eff_r);
    EXPECT_GT(std::max(me.eff_l, me.eff_r), kNichtStill);
}

// 5) Dasselbe wie 2) unter SCPX 1.7.
TEST(SoundSystem, A5120_Scpx_SKTEST4_LiedMitCtcInterrupt) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN);
    k1520::os::setEnv("K1520_AUDIO", "stumm");
    Sys<A5120Fahrer> s("scpx17_cpa780_k5601.hfe", "k1520_sound_scpx.hfe");
    bootScpx(s);
    sktestLied(s);
    liedPruefen(s);
}

// 6) PC 1715 + CP/A 1715, Karte 38H, PC-1715-Fassung von SKTEST4 (Vektor F7F0H).
TEST(SoundSystem, Pc1715_Cpa_SKTEST4_LiedMitCtcInterrupt) {
    k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN);
    k1520::os::setEnv("K1520_AUDIO", "stumm");
    Sys<PcFahrer> s("pc1715_cpa1715_boot_4lw.hfe", "k1520_sound_pc1715.hfe");
    bootPc(s);
    sktestLied(s);
    liedPruefen(s);
}
