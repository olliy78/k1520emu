// Tonstrecke (Entwurf 51, S2): Ringpuffer, WAV-Schreiber, Fuellstandsregler, TonAusgabe.
// Kein Test setzt ein Audiogeraet voraus.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>

#include "core/audio/ausgabe.h"
#include "core/audio/fuellstand_regler.h"
#include "core/audio/ringpuffer.h"
#include "core/audio/wav_schreiber.h"
#include "core/util/os_compat.h"

using namespace k1520::audio;

namespace {

std::string tempPfad(const char* name) {
    return ((std::filesystem::temp_directory_path() / "k1520_audio_test_").string() + name + "_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + ".wav");
}

std::vector<float> rampe(std::size_t ab, std::size_t n) {
    std::vector<float> v(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        v[2 * i] = static_cast<float>(ab + i);
        v[2 * i + 1] = -static_cast<float>(ab + i);
    }
    return v;
}

uint32_t rd32(const std::vector<unsigned char>& b, std::size_t o) {
    return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (static_cast<uint32_t>(b[o + 3]) << 24);
}
uint16_t rd16(const std::vector<unsigned char>& b, std::size_t o) { return b[o] | (b[o + 1] << 8); }

std::vector<unsigned char> lies(const std::string& pfad) {
    std::ifstream f(pfad, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(f), {});
}

}  // namespace

TEST(Ringpuffer, KapazitaetWirdZweierpotenz) {
    EXPECT_EQ(Ringpuffer(1000).kapazitaet(), 1024u);
    EXPECT_EQ(Ringpuffer(1024).kapazitaet(), 1024u);
}

TEST(Ringpuffer, UeberlaufVerwirftDasNeueUndBehaeltDasAlte) {
    Ringpuffer r(8);
    auto a = rampe(0, 6), b = rampe(6, 6);
    EXPECT_EQ(r.schreibe(a.data(), 6), 6u);
    EXPECT_EQ(r.schreibe(b.data(), 6), 2u);  // nur 2 frei
    EXPECT_EQ(r.verworfen(), 4u);
    std::vector<float> o(16);
    EXPECT_EQ(r.lies(o.data(), 8), 8u);
    for (std::size_t i = 0; i < 8; ++i) EXPECT_EQ(o[2 * i], static_cast<float>(i)) << i;
}

TEST(Ringpuffer, UnterlaufLiefertWenigerUndBleibtStabil) {
    Ringpuffer r(8);
    std::vector<float> o(16);
    EXPECT_EQ(r.lies(o.data(), 4), 0u);
    auto a = rampe(0, 3);
    r.schreibe(a.data(), 3);
    EXPECT_EQ(r.lies(o.data(), 4), 3u);
    EXPECT_EQ(r.fuellstand(), 0u);
}

TEST(Ringpuffer, UmlaufErhaeltReihenfolge) {
    Ringpuffer r(8);
    std::vector<float> o(16);
    std::size_t naechst = 0;
    for (int runde = 0; runde < 50; ++runde) {
        auto a = rampe(runde * 5, 5);
        ASSERT_EQ(r.schreibe(a.data(), 5), 5u);
        ASSERT_EQ(r.lies(o.data(), 5), 5u);
        for (int i = 0; i < 5; ++i) ASSERT_EQ(o[2 * i], static_cast<float>(naechst++));
    }
}

TEST(Ringpuffer, FadentestKeineVerlusteReihenfolgeErhalten) {
    constexpr std::size_t N = 400000;
    Ringpuffer r(1024);
    std::atomic<bool> fertig{false};
    std::thread erzeuger([&] {
        std::size_t i = 0;
        while (i < N) {
            std::size_t n = std::min<std::size_t>(37, N - i);
            auto a = rampe(i, n);
            // Rueckstau nur im Test (der echte Erzeuger verwirft): wiederholen, bis alles drin ist
            std::size_t ok = 0;
            while (ok < n) {
                ok += r.schreibe(a.data() + 2 * ok, n - ok);
                if (ok < n) std::this_thread::yield();
            }
            i += n;
        }
        fertig = true;
    });
    std::size_t erwartet = 0;
    std::vector<float> o(2 * 64);
    bool fehler = false;
    while (erwartet < N && !fehler) {
        std::size_t n = r.lies(o.data(), 64);
        if (n == 0) { std::this_thread::yield(); continue; }
        for (std::size_t i = 0; i < n; ++i)
            if (o[2 * i] != static_cast<float>(erwartet + i) || o[2 * i + 1] != -static_cast<float>(erwartet + i)) fehler = true;
        erwartet += n;
    }
    erzeuger.join();
    EXPECT_FALSE(fehler);
    EXPECT_EQ(erwartet, N);
}

TEST(WavSchreiber, KopfByteweise) {
    const std::string p = tempPfad("kopf");
    {
        WavSchreiber w;
        ASSERT_TRUE(w.oeffnen(p));
        std::vector<float> f(2 * 1000, 0.0f);
        f[0] = 1.0f;    // links +voll
        f[1] = -1.0f;   // rechts -voll
        f[2] = 2.0f;    // wird begrenzt
        w.schreibe(f.data(), 1000);
        EXPECT_EQ(w.frames(), 1000u);
    }  // Destruktor berichtigt den Kopf
    auto b = lies(p);
    std::remove(p.c_str());
    ASSERT_EQ(b.size(), 44u + 4000u);
    EXPECT_EQ(std::string(b.begin(), b.begin() + 4), "RIFF");
    EXPECT_EQ(rd32(b, 4), 36u + 4000u);
    EXPECT_EQ(std::string(b.begin() + 8, b.begin() + 16), "WAVEfmt ");
    EXPECT_EQ(rd32(b, 16), 16u);
    EXPECT_EQ(rd16(b, 20), 1u);
    EXPECT_EQ(rd16(b, 22), 2u);
    EXPECT_EQ(rd32(b, 24), 48000u);
    EXPECT_EQ(rd32(b, 28), 192000u);
    EXPECT_EQ(rd16(b, 32), 4u);
    EXPECT_EQ(rd16(b, 34), 16u);
    EXPECT_EQ(std::string(b.begin() + 36, b.begin() + 40), "data");
    EXPECT_EQ(rd32(b, 40), 4000u);
    EXPECT_EQ(static_cast<int16_t>(rd16(b, 44)), 32767);
    EXPECT_EQ(static_cast<int16_t>(rd16(b, 46)), -32767);
    EXPECT_EQ(static_cast<int16_t>(rd16(b, 48)), 32767);  // 2.0 begrenzt
}

TEST(WavSchreiber, KopfStimmtNachlaengerLaufSchonOhneSchliessen) {
    // Abbruchfall: nach >1 s Material ist der Kopf auf Platte schon berichtigt.
    const std::string p = tempPfad("abbruch");
    WavSchreiber w;
    ASSERT_TRUE(w.oeffnen(p));
    std::vector<float> f(2 * 48000, 0.25f);
    w.schreibe(f.data(), 48000);
    auto b = lies(p);  // Datei noch offen
    EXPECT_EQ(rd32(b, 40), 48000u * 4);
    w.schliessen();
    std::remove(p.c_str());
}

TEST(WavSchreiber, UnbeschreibbarerPfadMeldetGrund) {
    WavSchreiber w;
    std::string f;
    EXPECT_FALSE(w.oeffnen("/nicht/vorhanden/x.wav", &f));
    EXPECT_FALSE(f.empty());
}

TEST(FuellstandRegler, VerwirftDarueberVerdoppeltDarunter) {
    constexpr FuellstandRegler r(2880, 960);
    EXPECT_EQ(r.korrektur(2880), 0);
    EXPECT_EQ(r.korrektur(2880 + 960), 0);
    EXPECT_EQ(r.korrektur(2880 + 961), -1);
    EXPECT_EQ(r.korrektur(2880 - 960), 0);
    EXPECT_EQ(r.korrektur(2880 - 961), +1);
    EXPECT_EQ(r.korrektur(0), +1);
    EXPECT_EQ(r.korrektur(100000), -1);
}

TEST(TonAusgabe, StummStartetUndStopptOhneGeraet) {
    TonAusgabe::Optionen o;
    o.backend = "stumm";
    TonAusgabe a(o);
    EXPECT_EQ(a.status(), TonAusgabe::Status::Aus);
    ASSERT_TRUE(a.start());
    EXPECT_EQ(a.status(), TonAusgabe::Status::Stumm);
    EXPECT_TRUE(a.fehler().empty());
    a.stop();
    EXPECT_EQ(a.status(), TonAusgabe::Status::Aus);
    ASSERT_TRUE(a.start());  // Neustart moeglich
    // Destruktor ruft stop()
}

TEST(TonAusgabe, UmgebungsvariableWaehltStumm) {
    ASSERT_TRUE(k1520::os::setEnv("K1520_AUDIO", "stumm"));
    TonAusgabe a;
    EXPECT_TRUE(a.start());
    EXPECT_EQ(a.status(), TonAusgabe::Status::Stumm);
    a.stop();
    k1520::os::unsetEnv("K1520_AUDIO");
}

#if !defined(_WIN32)
TEST(TonAusgabe, AlsaBibliothekFehltLiefertGrundStattAbsturz) {
    TonAusgabe::Optionen o;
    o.backend = "auto";
    o.alsaBibliothek = "libgibtesnicht_k1520.so.99";
    TonAusgabe a(o);
    EXPECT_FALSE(a.start());
    EXPECT_EQ(a.status(), TonAusgabe::Status::KeinGeraet);
    EXPECT_FALSE(a.fehler().empty());
    // Schreiben/WAV bleiben harmlos bzw. nutzbar
    std::vector<float> f(2 * 100, 0.1f);
    a.schreibe(f.data(), 100);
    a.stop();
}

TEST(TonAusgabe, AlsaBibliothekUeberUmgebungsvariable) {
    ASSERT_TRUE(k1520::os::setEnv("K1520_AUDIO_ALSA_LIB", "libgibtesnicht_k1520.so.99"));
    ASSERT_TRUE(k1520::os::setEnv("K1520_AUDIO", "auto"));
    TonAusgabe a;
    EXPECT_FALSE(a.start());
    EXPECT_EQ(a.status(), TonAusgabe::Status::KeinGeraet);
    k1520::os::unsetEnv("K1520_AUDIO_ALSA_LIB");
    k1520::os::unsetEnv("K1520_AUDIO");
}
#endif

TEST(TonAusgabe, WavLaeuftUnabhaengigVomGeraetUndVomTempo) {
    const std::string p = tempPfad("ausgabe");
    TonAusgabe::Optionen o;
    o.backend = "stumm";
    TonAusgabe a(o);
    ASSERT_TRUE(a.start());
    a.echtzeit(false);  // schneller als Echtzeit
    ASSERT_TRUE(a.wavStart(p));
    std::vector<float> f(2 * 4800, 0.5f);
    for (int i = 0; i < 20; ++i) a.schreibe(f.data(), 4800);
    EXPECT_EQ(a.wavFrames(), 96000u);
    a.wavStop();
    auto b = lies(p);
    std::remove(p.c_str());
    ASSERT_EQ(b.size(), 44u + 96000u * 4);
    EXPECT_EQ(rd32(b, 40), 96000u * 4);
}

TEST(TonAusgabe, WavOhneStartMoeglich) {
    const std::string p = tempPfad("ohnestart");
    TonAusgabe a;
    ASSERT_TRUE(a.wavStart(p));
    std::vector<float> f(2 * 10, 0.0f);
    a.schreibe(f.data(), 10);
    a.wavStop();
    EXPECT_EQ(lies(p).size(), 44u + 40u);
    std::remove(p.c_str());
}

TEST(TonAusgabe, OhneEchtzeitVerwirftDerPufferNichtsStauend) {
    // Stumm-Backend ist nie "hoerbar": der Erzeugerpfad legt nichts in den Ringpuffer.
    TonAusgabe::Optionen o;
    o.backend = "stumm";
    TonAusgabe a(o);
    ASSERT_TRUE(a.start());
    std::vector<float> f(2 * 4800, 0.5f);
    for (int i = 0; i < 100; ++i) a.schreibe(f.data(), 4800);
    EXPECT_EQ(a.fuellstand(), 0u);
    EXPECT_EQ(a.verworfen(), 0u);
    a.echtzeit(false);
    EXPECT_FALSE(a.echtzeit());
    EXPECT_EQ(a.status(), TonAusgabe::Status::Stumm);
}

TEST(TonAusgabe, LautstaerkeWirdBegrenzt) {
    TonAusgabe a;
    a.lautstaerke(7.0f);
    EXPECT_EQ(a.lautstaerke(), 1.0f);
    a.lautstaerke(-1.0f);
    EXPECT_EQ(a.lautstaerke(), 0.0f);
    a.stumm(true);
    EXPECT_TRUE(a.stumm());
}

// Ueberlauf waere ein Knall: zuS16 begrenzt (Anwenderwunsch kein Clipping, Merkposten sound.md Nr. 16).
TEST(WavSchreiber, ZuS16BegrenztStattUeberzulaufen) {
    using k1520::audio::WavSchreiber;
    EXPECT_EQ(WavSchreiber::zuS16(1.5f), 32767);
    EXPECT_EQ(WavSchreiber::zuS16(-1.5f), -32767);
    EXPECT_EQ(WavSchreiber::zuS16(1.0f), 32767);
    EXPECT_EQ(WavSchreiber::zuS16(0.0f), 0);
}
