/**
 * @file test_sound_k1520.cpp
 * @brief SOUND-Karte K1520 (Entwurf 51, AP S3): Tore, AY-Zugriff, CTC (50 Hz, Kaskade),
 *        Mischer ABC/ACB/Mono, Taktumrechnung, Senke.
 */
#include <gtest/gtest.h>
#include <algorithm>
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
        SoundK1520::Config c; c.kaskade = kaskade;   // ausdruecklich, die Vorgabe ist 7
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

TEST(SoundK1520, VorgabeKanalzuordnungIstAcb) {   // Entwurf 51 7a: Anwenderentscheid nach dem Hoervergleich
    EXPECT_EQ(SoundK1520::Config{}.kanal, SoundK1520::Kanal::ACB);
}

TEST(SoundK1520, MischerAbcNurAIstNurLinks) {
    SoundK1520::Config c; c.kanal = SoundK1520::Kanal::ABC;
    Rig g(c); g.k.setzeSenke(&g.s);
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
    SoundK1520::Config c; c.kanal = SoundK1520::Kanal::ABC;
    Rig g(c); g.k.setzeSenke(&g.s);
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
    auto pegel = [](uint32_t hz) {
        SoundK1520::Config c; c.tiefpass_hz = hz;
        Rig g(c); g.k.setzeSenke(&g.s);
        g.ton(0, 4);                        // ~11 kHz
        g.laufe(uint64_t(SYS * 0.2));
        return rms(g.s.l, 3000);
    };
    const double aus = pegel(0);
    EXPECT_LT(pegel(SoundK1520::TIEFPASS_HZ), aus * 0.6);
    EXPECT_LT(pegel(3000), pegel(12000)) << "tiefere Grenzfrequenz daempft staerker";
    EXPECT_LT(pegel(3000), aus * 0.4);
    EXPECT_NEAR(pegel(0), aus, 1e-9) << "aus = ungefiltert";
}

TEST(SoundK1520, TiefpassUmstellenImLauf) {
    Rig g; g.k.setzeSenke(&g.s);
    g.ton(0, 4);
    g.laufe(uint64_t(SYS * 0.2));
    const size_t n1 = g.s.l.size();
    auto c = g.k.config(); c.tiefpass_hz = 3000;
    ASSERT_TRUE(g.k.umstellen(c));
    EXPECT_EQ(g.k.config().tiefpass_hz, 3000u);
    g.laufe(uint64_t(SYS * 0.2));
    const std::vector<float> vorher(g.s.l.begin() + 3000, g.s.l.begin() + n1);
    const std::vector<float> nachher(g.s.l.begin() + n1 + 3000, g.s.l.end());
    EXPECT_LT(rms(nachher, 0), rms(vorher, 0) * 0.4);
    c.tiefpass_hz = 0;
    ASSERT_TRUE(g.k.umstellen(c));
}

TEST(SoundK1520, KaskadeIstAbWerkGestecktSieben) {
    EXPECT_EQ(SoundK1520::Config{}.kaskade, 7);
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

// Kein Clipping (Anwenderwunsch): schlimmster Fall = drei volle Kanaele, Gleichspannung (Ton aus, Rauschen aus) nach
// langer Stille, danach Rechteck mit grosser Periode und Huellkurvenform 13; Spitze <= 0,7071 (-3 dBFS).
namespace {
void pruefeSpitze(SoundK1520::Kanal kanal) {
    SoundK1520::Config c; c.kanal = kanal;
    Rig g(c); g.k.setzeSenke(&g.s);
    g.laufe(uint64_t(SYS * 1.0));                       // lange Stille: Hochpass eingeschwungen auf 0
    g.ayReg(7, 0x3F);                                   // alles aus
    for (int k = 0; k < 3; ++k) g.ayReg(8 + k, 0x0F);   // Gleichspannung voll: Sprung 0 -> 1
    g.laufe(uint64_t(SYS * 0.3));
    for (int k = 0; k < 3; ++k) { g.ayReg(k * 2, 0xFF); g.ayReg(k * 2 + 1, 0x0F); }
    g.ayReg(7, 0x38);                                   // Rechteck, Periode gross
    g.laufe(uint64_t(SYS * 0.5));
    for (int k = 0; k < 3; ++k) g.ayReg(8 + k, 0x10);   // Huellkurve
    g.ayReg(11, 0x20); g.ayReg(12, 0x00);
    g.ayReg(13, 13);
    g.laufe(uint64_t(SYS * 0.5));
    ASSERT_GT(g.s.l.size(), 10000u);
    float m = 0;
    for (float v : g.s.l) m = std::max(m, std::fabs(v));
    for (float v : g.s.r) m = std::max(m, std::fabs(v));
    EXPECT_GT(m, 0.3f) << "Probe zu leise, sagt nichts";
    EXPECT_LE(m, 0.7072f);
}
}  // namespace

TEST(SoundK1520, SpitzeAbcHoechstensMinus3dBFS) { pruefeSpitze(SoundK1520::Kanal::ABC); }
TEST(SoundK1520, SpitzeAcbHoechstensMinus3dBFS) { pruefeSpitze(SoundK1520::Kanal::ACB); }
TEST(SoundK1520, SpitzeMonoHoechstensMinus3dBFS) { pruefeSpitze(SoundK1520::Kanal::Mono); }

// ---- Umstellung im Lauf (Entwurf 51, Merkposten sound.md Nr. 17) ----

namespace {
/// Frequenz der Nulldurchgaenge im Abschnitt [von, bis) des linken Kanals.
double freqAbschnitt(const std::vector<float>& l, size_t von, size_t bis) {
    int n = 0; size_t erst = 0, letzt = 0;
    for (size_t i = von + 1; i < bis && i < l.size(); ++i)
        if (l[i - 1] < 0 && l[i] >= 0) { if (!n) erst = i; letzt = i; ++n; }
    return n > 2 ? (n - 1) / ((letzt - erst) / 48000.0) : 0.0;
}
}  // namespace

TEST(SoundK1520, UmstellenTaktwechselImLaufVerdoppeltDieTonfrequenz) {
    Rig g; g.k.setzeSenke(&g.s);           // QuarzHalbe
    g.ton(0, 0x6F);
    g.laufe(uint64_t(SYS));
    const size_t n1 = g.s.l.size();
    const uint64_t chipVorher = g.k.ay().takt();
    SoundK1520::Config c = g.k.config();
    c.takt = SoundK1520::Takt::Quarz;
    std::string grund;
    ASSERT_TRUE(g.k.umstellen(c, &grund)) << grund;
    EXPECT_GE(g.k.ay().takt(), chipVorher) << "Chipzeit monoton";
    g.laufe(uint64_t(SYS));
    const double f1 = freqAbschnitt(g.s.l, 4800, n1 - 100);
    const double f2 = freqAbschnitt(g.s.l, n1 + 4800, g.s.l.size());
    ASSERT_GT(f1, 0.0);
    ASSERT_GT(f2, 0.0);
    EXPECT_NEAR(f2 / f1, 2.0, 0.02);
    // Rueckweg
    c.takt = SoundK1520::Takt::QuarzHalbe;
    ASSERT_TRUE(g.k.umstellen(c));
    const size_t n2 = g.s.l.size();
    g.laufe(uint64_t(SYS));
    EXPECT_NEAR(freqAbschnitt(g.s.l, n2 + 4800, g.s.l.size()), f1, f1 * 0.01);
    // Gleich viele Abtastwerte je Maschinensekunde in jedem Takt
    EXPECT_NEAR(double(g.s.l.size()), 3.0 * 48000, 400);
}

TEST(SoundK1520, UmstellenKanalwechselAbcNachMonoWirkt) {
    SoundK1520::Config c; c.kanal = SoundK1520::Kanal::ABC;
    Rig g(c); g.k.setzeSenke(&g.s);
    g.ton(0, 0x40);                        // nur A: in ABC nur links
    g.laufe(uint64_t(SYS * 0.2));
    const size_t n = g.s.l.size();
    EXPECT_LT(rms(g.s.r, 3000), 1e-4);
    c.kanal = SoundK1520::Kanal::Mono;
    ASSERT_TRUE(g.k.umstellen(c));
    g.laufe(uint64_t(SYS * 0.2));
    std::vector<float> r2(g.s.r.begin() + n + 2000, g.s.r.end());
    EXPECT_GT(rms(r2, 0), 0.01);           // Mono: A auch rechts
}

TEST(SoundK1520, UmstellenKaskadeAnUndAusImLauf) {
    auto lauf = [](SoundK1520& k, Rig& g) {
        g.bus.ioWrite(0x3D, 0x47);   // Kanal 1: Zaehler, TC folgt
        g.bus.ioWrite(0x3D, 0xFF);
        g.bus.ioWrite(0x3C, 0x05);   // Kanal 0: Zeitgeber, Vorteiler 16
        g.bus.ioWrite(0x3C, 10);
        const int vor = k.ctc().getCount(1);
        g.laufe(1700);
        return vor - k.ctc().getCount(1);
    };
    Rig g;
    ASSERT_TRUE(g.k.umstellen([&] { auto c = g.k.config(); c.kaskade = 1; return c; }()));
    EXPECT_NEAR(lauf(g.k, g), 10, 1);
    auto c = g.k.config(); c.kaskade = 0;
    ASSERT_TRUE(g.k.umstellen(c));
    g.bus.ioWrite(0x3D, 0x47); g.bus.ioWrite(0x3D, 0xFF);
    const int vor = g.k.ctc().getCount(1);
    g.laufe(1700);
    EXPECT_EQ(g.k.ctc().getCount(1), vor) << "Kaskade aus: kein Zaehlimpuls mehr";
}

TEST(SoundK1520, UmstellenChipwechselBehaeltRegister) {
    Rig g;
    g.ayReg(14, 0x55);
    g.ton(0, 0x40);
    auto c = g.k.config(); c.chip = Ay38910::Variante::AY8912;
    ASSERT_TRUE(g.k.umstellen(c));
    EXPECT_EQ(g.k.ay().reg(14), 0x55);
    EXPECT_EQ(g.k.ay().reg(8), 0x0F);
    EXPECT_EQ(g.k.ay().variante(), Ay38910::Variante::AY8912);
}

TEST(SoundK1520, UmstellenMitAndererBasisOderSystemtaktIstFalse) {
    Rig g;
    auto c = g.k.config(); c.basis = 0x40;
    std::string grund;
    EXPECT_FALSE(g.k.umstellen(c, &grund));
    EXPECT_NE(grund.find("Basis"), std::string::npos);
    c = g.k.config(); c.systemtakt_hz = 1000000;
    EXPECT_FALSE(g.k.umstellen(c, &grund));
    EXPECT_EQ(g.k.config().basis, 0x38);
}

TEST(SoundK1520, TempoZweiHaeltTonhoeheUndHalbiertAbtastwerte) {
    auto lauf = [](double f, size_t& n, double& hz) {
        Rig g; g.k.setzeSenke(&g.s);
        g.k.setzeTempo(f);
        g.ton(0, 0x6F);
        g.laufe(uint64_t(SYS));                 // 1 s MASCHINENzeit
        n = g.s.l.size();
        hz = freqAbschnitt(g.s.l, 4800 / size_t(f), n);   // Abtastwerte: Frequenz je Wirtssekunde
        return g.k.ay().takt();
    };
    size_t n1 = 0, n2 = 0; double f1 = 0, f2 = 0;
    const uint64_t c1 = lauf(1.0, n1, f1);
    const uint64_t c2 = lauf(2.0, n2, f2);
    EXPECT_NEAR(double(n2), n1 / 2.0, 200);                  // halb so viele Abtastwerte je Maschinensekunde
    EXPECT_NEAR(double(c2), c1 / 2.0, c1 * 0.001);           // halb so viele Chiptakte
    // je Wirtssekunde (= 0,5 Maschinensekunden) gleich viele Werte, Tonhoehe in Werten gemessen gleich
    ASSERT_GT(f1, 0.0);
    EXPECT_NEAR(f2, f1, f1 * 0.01);
    // Umschalten im Lauf: Chipzeit monoton, danach halbe Rate
    Rig g; g.k.setzeSenke(&g.s);
    g.ton(0, 0x6F);
    g.laufe(uint64_t(SYS / 2));
    const uint64_t vor = g.k.ay().takt();
    g.k.setzeTempo(2.0);
    EXPECT_GE(g.k.ay().takt(), vor);
    const size_t n = g.s.l.size();
    g.laufe(uint64_t(SYS));
    EXPECT_NEAR(double(g.s.l.size() - n), 24000.0, 200);
}
