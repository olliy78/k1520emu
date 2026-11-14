/**
 * @file test_sound_k1520.cpp
 * @brief SOUND-Karte K1520 (Entwurf 51, AP S3): Tore, AY-Zugriff, CTC (50 Hz, Kaskade),
 *        Mischer ABC/ACB/Mono, Taktumrechnung, Senke.
 */
#include <gtest/gtest.h>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "core/cards/sound_k1520/sound_k1520.h"

using k1520::SoundK1520;

namespace {

constexpr double SYS = 2457600.0;

/// Senke-Attrappe: sammelt Stereo-Frames.
struct Sammler : k1520::audio::TonSenke {
    std::vector<float> l, r;
    void schreibe(const float* lr, std::size_t frames) override {
        for (std::size_t i = 0; i < frames; ++i) { l.push_back(lr[2 * i]); r.push_back(lr[2 * i + 1]); }
    }
};

/// Platzhalter-Karte fuer Tor-Konflikte.
struct Sonde : BusDevice {
    uint8_t ioRead(uint8_t) override { return 0x5A; }
    void ioWrite(uint8_t, uint8_t) override {}
    const char* deviceName() const override { return "Sonde"; }
};

struct Rig {
    K1520Bus bus;
    SoundK1520 k;
    Sammler s;
    uint64_t total = 0;
    explicit Rig(SoundK1520::Config c = {}) : k(c) { k.attachToBus(bus); }

    void ayReg(int r, uint8_t d) {
        bus.ioWrite(k.config().basis, uint8_t(r));
        bus.ioWrite(uint8_t(k.config().basis + 1), d);
    }
    /// Maschinenzeit in Stuecken von 100 Takten (wie nach einem Befehl) laufen lassen.
    void laufe(uint64_t takte) {
        for (uint64_t t = 0; t < takte; t += 100) { total += 100; k.takt(100, total); }
    }
    void ton(int kanal, int per) {   // kanal 0..2, nur dieser Kanal an, Pegel 15
        ayReg(kanal * 2, per & 0xFF);
        ayReg(kanal * 2 + 1, (per >> 8) & 0x0F);
        ayReg(7, uint8_t(0x3F & ~(1 << kanal)));
        ayReg(8 + kanal, 0x0F);
    }
};

/// Effektivwert ab Index @p ab.
double rms(const std::vector<float>& v, size_t ab) {
    double s = 0; size_t n = 0;
    for (size_t i = ab; i < v.size(); ++i) { s += double(v[i]) * v[i]; ++n; }
    return n ? std::sqrt(s / n) : 0.0;
}

}  // namespace

TEST(SoundK1520, ToreBelegtUndFrei) {
    Rig g;
    for (unsigned t : {0u, 1u, 4u, 5u, 6u, 7u}) EXPECT_EQ(g.bus.ioOwner(uint8_t(0x38 + t)), &g.k) << t;
    EXPECT_EQ(g.bus.ioOwner(0x3A), nullptr);
    EXPECT_EQ(g.bus.ioOwner(0x3B), nullptr);
    EXPECT_STREQ(g.k.deviceName(), "SOUND-K1520");
}

TEST(SoundK1520, BasisKeinVielfachesVon8WirdAbgelehnt) {
    SoundK1520::Config c; c.basis = 0x3C;
    EXPECT_THROW(SoundK1520{c}, std::invalid_argument);
}

TEST(SoundK1520, TorPlusEinsLesenIstFF) {
    Rig g;
    EXPECT_EQ(g.bus.ioRead(0x39), 0xFF);
}

TEST(SoundK1520, RegisterSchreibenUndLesen) {
    Rig g;
    g.ayReg(2, 0xAB);
    EXPECT_EQ(g.k.ay().reg(2), 0xAB);
    g.bus.ioWrite(0x38, 2);
    EXPECT_EQ(g.bus.ioRead(0x38), 0xAB);
    g.ayReg(3, 0xFF);                       // R3 hat nur 4 Bit
    g.bus.ioWrite(0x38, 3);
    EXPECT_EQ(g.bus.ioRead(0x38), 0x0F);
}

TEST(SoundK1520, R7RuecklesenWieAypbcInc) {
    Rig g;
    g.bus.ioWrite(0x38, 7);                 // OUT (38H),7
    g.bus.ioWrite(0x39, 0xB8);              // Portbits + Mischer
    g.bus.ioWrite(0x38, 7);
    EXPECT_EQ(g.bus.ioRead(0x38), 0xB8);    // IN A,(38H) liefert R7 samt Portbits
}

TEST(SoundK1520, BelegtGrundMeldetBelegtesTor) {
    K1520Bus bus;
    SoundK1520 k;
    EXPECT_EQ(k.belegtGrund(bus), "");
    Sonde s;
    bus.registerIO(&s, 0x3D, 1);
    const std::string g = k.belegtGrund(bus);
    EXPECT_NE(g.find("3DH"), std::string::npos) << g;
    EXPECT_NE(g.find("Sonde"), std::string::npos) << g;
    // +2/+3 gehoeren der Karte nicht: ein Fremdgeraet dort stoert nicht
    K1520Bus bus2;
    Sonde s2;
    bus2.registerIO(&s2, 0x3A, 2);
    EXPECT_EQ(k.belegtGrund(bus2), "");
}

TEST(SoundK1520, CtcKanal0Liefert50InterruptsJeSekunde) {
    Rig g;
    g.k.ctc().setIEI(true);
    g.bus.ioWrite(0x3C, 0xD8);   // Vektor
    g.bus.ioWrite(0x3C, 0xA7);   // Zeitgeber, Vorteiler 256, Interrupt an, TC folgt
    g.bus.ioWrite(0x3C, 0xC0);   // 2457600 / 256 / 192 = 50 Hz
    int n = 0;
    for (uint64_t t = 0; t < uint64_t(SYS); t += 100) {
        g.total += 100;
        g.k.takt(100, g.total);
        if (g.k.ctc().hasInterrupt()) {
            EXPECT_EQ(g.k.ctc().getVector(), 0xD8);
            g.k.ctc().onRETI();
            ++n;
        }
    }
    EXPECT_EQ(n, 50);
}

TEST(SoundK1520, KaskadeTo0NachTr1ZaehltNulldurchgaenge) {
    auto lauf = [](uint8_t kaskade) {
        SoundK1520::Config c; c.kaskade = kaskade;
        Rig g(c);
        g.bus.ioWrite(0x3D, 0x47);   // Kanal 1: Zaehler, TC folgt
        g.bus.ioWrite(0x3D, 0xFF);
        g.bus.ioWrite(0x3C, 0x05);   // Kanal 0: Zeitgeber, Vorteiler 16, ohne Interrupt
        g.bus.ioWrite(0x3C, 10);     // alle 160 Takte ein Nulldurchgang
        g.laufe(1700);               // 10 Nulldurchgaenge + Rest
        return g.k.ctc().getCount(1);
    };
    EXPECT_EQ(lauf(0), 255);                 // ohne Jumper zaehlt nichts
    const int mit = lauf(1);
    EXPECT_NEAR(mit, 255 - 10, 1);           // Jumper TO0->TR1: zehn Zaehlimpulse
}

TEST(SoundK1520, MischerAbcNurAIstNurLinks) {
    Rig g; g.k.setzeSenke(&g.s);
    g.ton(0, 32);
    g.laufe(uint64_t(SYS * 0.3));
    ASSERT_GT(g.s.l.size(), 10000u);
    EXPECT_GT(rms(g.s.l, 5000), 0.1);
    EXPECT_LT(rms(g.s.r, 5000), 1e-6);
}

TEST(SoundK1520, MischerAcbNurAIstNurLinks) {
    SoundK1520::Config c; c.kanal = SoundK1520::Kanal::ACB;
    Rig g(c); g.k.setzeSenke(&g.s);
    g.ton(0, 32);
    g.laufe(uint64_t(SYS * 0.3));
    EXPECT_GT(rms(g.s.l, 5000), 0.1);
    EXPECT_LT(rms(g.s.r, 5000), 1e-6);
}

TEST(SoundK1520, MischerAbcNurBIstMitte) {
    Rig g; g.k.setzeSenke(&g.s);
    g.ton(1, 32);
    g.laufe(uint64_t(SYS * 0.3));
    const double l = rms(g.s.l, 5000), r = rms(g.s.r, 5000);
    EXPECT_GT(l, 0.03);
    EXPECT_NEAR(l, r, l * 1e-4);
}

TEST(SoundK1520, MischerAcbNurBIstNurRechts) {
    SoundK1520::Config c; c.kanal = SoundK1520::Kanal::ACB;
    Rig g(c); g.k.setzeSenke(&g.s);
    g.ton(1, 32);
    g.laufe(uint64_t(SYS * 0.3));
    EXPECT_LT(rms(g.s.l, 5000), 1e-6);
    EXPECT_GT(rms(g.s.r, 5000), 0.1);
}

TEST(SoundK1520, MischerAcbNurCIstMitte) {
    SoundK1520::Config c; c.kanal = SoundK1520::Kanal::ACB;
    Rig g(c); g.k.setzeSenke(&g.s);
    g.ton(2, 32);
    g.laufe(uint64_t(SYS * 0.3));
    const double l = rms(g.s.l, 5000), r = rms(g.s.r, 5000);
    EXPECT_GT(l, 0.03);
    EXPECT_NEAR(l, r, l * 1e-4);
}

TEST(SoundK1520, MonoBeideSeitenGleich) {
    SoundK1520::Config c; c.kanal = SoundK1520::Kanal::Mono;
    Rig g(c); g.k.setzeSenke(&g.s);
    g.ton(0, 32);
    g.laufe(uint64_t(SYS * 0.3));
    EXPECT_GT(rms(g.s.l, 5000), 0.05);
    ASSERT_EQ(g.s.l.size(), g.s.r.size());
    for (size_t i = 0; i < g.s.l.size(); ++i) ASSERT_EQ(g.s.l[i], g.s.r[i]);
}

TEST(SoundK1520, DreiVolleKanaeleUebersteuernNicht) {
    Rig g; g.k.setzeSenke(&g.s);
    for (int k = 0; k < 3; ++k) { g.ayReg(k * 2, 32); g.ayReg(8 + k, 0x0F); }
    g.ayReg(7, 0x38);
    g.laufe(uint64_t(SYS * 0.2));
    for (float v : g.s.l) ASSERT_LE(std::fabs(v), 1.0f);
}

TEST(SoundK1520, TiefpassDaempftHoheToene) {
    auto pegel = [](bool tp) {
        SoundK1520::Config c; c.tiefpass = tp;
        Rig g(c); g.k.setzeSenke(&g.s);
        g.ton(0, 4);                        // ~11 kHz
        g.laufe(uint64_t(SYS * 0.2));
        return rms(g.s.l, 3000);
    };
    EXPECT_LT(pegel(true), pegel(false) * 0.6);
}

TEST(SoundK1520, QuarzHalbeTaktLiefertRichtigeFrequenz) {
    Rig g; g.k.setzeSenke(&g.s);            // QuarzHalbe ist Vorgabe
    g.ton(0, 0x6F);
    g.laufe(uint64_t(SYS));                 // 1 s Maschinenzeit
    // Aufsteigende Nulldurchgaenge nach dem Einschwingen
    const size_t ab = 4800;
    ASSERT_GE(g.s.l.size(), 47000u);
    int n = 0; size_t erst = 0, letzt = 0;
    for (size_t i = ab + 1; i < g.s.l.size(); ++i)
        if (g.s.l[i - 1] < 0 && g.s.l[i] >= 0) { if (!n) erst = i; letzt = i; ++n; }
    ASSERT_GT(n, 100);
    const double f = (n - 1) / ((letzt - erst) / 48000.0);
    const double soll = 1789772.5 / (16.0 * 111.0);
    EXPECT_NEAR(f, soll, soll * 0.005);
}

TEST(SoundK1520, SystemtaktUndQuarzVolleRechnenRichtig) {
    auto freq = [](SoundK1520::Takt t) {
        SoundK1520::Config c; c.takt = t;
        Rig g(c); g.k.setzeSenke(&g.s);
        g.ton(0, 0x6F);
        g.laufe(uint64_t(SYS));
        int n = 0; size_t erst = 0, letzt = 0;
        for (size_t i = 4801; i < g.s.l.size(); ++i)
            if (g.s.l[i - 1] < 0 && g.s.l[i] >= 0) { if (!n) erst = i; letzt = i; ++n; }
        return (n - 1) / ((letzt - erst) / 48000.0);
    };
    EXPECT_NEAR(freq(SoundK1520::Takt::System), SYS / (16.0 * 111), SYS / (16.0 * 111) * 0.005);
    EXPECT_NEAR(freq(SoundK1520::Takt::Quarz), 3579545.0 / (16.0 * 111), 3579545.0 / (16.0 * 111) * 0.005);
}

TEST(SoundK1520, OhneSenkeKeineAbtastwerte) {
    Rig g;
    g.ton(0, 32);
    g.laufe(uint64_t(SYS * 0.1));
    EXPECT_EQ(g.k.ay().abholbereit(), 0u);
    EXPECT_FALSE(g.k.ay().ausgabeAktiv());
    // Senke ein, dann wieder aus: danach kommt nichts mehr an
    g.k.setzeSenke(&g.s);
    g.laufe(uint64_t(SYS * 0.1));
    const size_t n = g.s.l.size();
    EXPECT_GT(n, 4000u);
    g.k.setzeSenke(nullptr);
    g.laufe(uint64_t(SYS * 0.1));
    EXPECT_EQ(g.s.l.size(), n);
    EXPECT_FALSE(g.k.ay().ausgabeAktiv());
}

TEST(SoundK1520, ResetIstStill) {
    Rig g; g.k.setzeSenke(&g.s);
    g.ton(0, 32);
    g.laufe(uint64_t(SYS * 0.1));
    g.k.reset();
    EXPECT_EQ(g.k.ay().reg(8), 0);
    EXPECT_EQ(g.k.ay().reg(7), 0);
    const size_t ab = g.s.l.size() + 4000;   // Gleichanteilfilter ist zurueckgesetzt
    g.laufe(uint64_t(SYS * 0.2));
    EXPECT_LT(rms(g.s.l, ab), 1e-4);
    EXPECT_LT(rms(g.s.r, ab), 1e-4);
}
