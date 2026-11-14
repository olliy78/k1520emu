// Tests des Chipmodells AY-3-8910/8912 (Entwurf 51, S1)
#include <gtest/gtest.h>
#include <cmath>
#include <complex>
#include <vector>
#include "core/primitives/ay38910.h"

namespace {

void wr(Ay38910& c, int r, uint8_t d) {
    c.adresseSchreiben(static_cast<uint8_t>(r));
    c.datenSchreiben(d);
}
uint8_t rd(Ay38910& c, int r) {
    c.adresseSchreiben(static_cast<uint8_t>(r));
    return c.datenLesen();
}

// Kanal A: nur Ton, fester Pegel 15
void tonA(Ay38910& c, int per) {
    wr(c, 0, per & 0xFF);
    wr(c, 1, (per >> 8) & 0x0F);
    wr(c, 7, 0x3E);   // Ton A an, Rest aus
    wr(c, 8, 0x0F);
}

std::vector<float> sammle(Ay38910& c, double sekunden, int rate) {
    const uint64_t t0 = c.takt();
    c.ausgabeEin(rate, 0.0f);
    c.rechneBis(t0 + static_cast<uint64_t>(sekunden * c.chipTakt()));
    std::vector<Ay38910::Abtastwert> w(c.abholbereit());
    c.abholen(w.data(), w.size());
    std::vector<float> a;
    for (auto& x : w) a.push_back(x.a);
    return a;
}

// Frequenz aus aufsteigenden Nulldurchgaengen (Schwelle = halber Pegel), erster bis letzter
double frequenz(const std::vector<float>& a, int rate) {
    double erst = -1, letzt = -1;
    int n = 0;
    for (size_t i = 1; i < a.size(); ++i) {
        if (a[i - 1] < 0.5f && a[i] >= 0.5f) {
            const double pos = (i - 1) + (0.5 - a[i - 1]) / (a[i] - a[i - 1]);
            if (erst < 0) erst = pos;
            letzt = pos;
            ++n;
        }
    }
    if (n < 2) return 0;
    return rate * (n - 1) / (letzt - erst);
}

}  // namespace

TEST(Ay38910, LesemaskenAllerRegister) {
    Ay38910 c;
    const uint8_t maske[16] = {0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF,
                               0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF};
    wr(c, 7, 0xFF);   // beide Ports Ausgang -> R14/R15 lesen den Registerwert
    for (int r = 0; r < 16; ++r) {
        if (r == 7) continue;
        wr(c, r, 0xFF);
        EXPECT_EQ(rd(c, r), maske[r]) << "R" << r;
    }
    EXPECT_EQ(rd(c, 7), 0xFF);
}

TEST(Ay38910, AbwahlAbSechzehn) {
    Ay38910 c;
    wr(c, 0, 0x55);
    for (int a : {16, 17, 0x80, 0xFF}) {
        c.adresseSchreiben(static_cast<uint8_t>(a));
        EXPECT_EQ(c.datenLesen(), 0xFF);
        c.datenSchreiben(0x12);
    }
    EXPECT_EQ(rd(c, 0), 0x55);
    for (int r = 1; r < 16; ++r) EXPECT_EQ(c.reg(r), 0) << r;   // nichts verstellt
    EXPECT_EQ(c.reg(0), 0x55);
}

TEST(Ay38910, PortsEingangAusgangPullup) {
    Ay38910 c;
    EXPECT_EQ(rd(c, 14), 0xFF);          // Eingang, offen = Pull-up
    EXPECT_EQ(rd(c, 15), 0xFF);
    c.setzePortEingang(0, 0xA5);
    c.setzePortEingang(1, 0x3C);
    EXPECT_EQ(rd(c, 14), 0xA5);
    EXPECT_EQ(rd(c, 15), 0x3C);
    wr(c, 14, 0x11);                     // Schreiben auf Eingang: Register, Pin bleibt
    EXPECT_EQ(rd(c, 14), 0xA5);
    EXPECT_EQ(c.portPins(0), 0xA5);
    wr(c, 7, 0x40);                      // Port A Ausgang
    EXPECT_EQ(rd(c, 14), 0x11);
    EXPECT_EQ(c.portPins(0), 0x11);
    EXPECT_EQ(rd(c, 15), 0x3C);          // B noch Eingang
    wr(c, 15, 0x22);
    wr(c, 7, 0xC0);
    EXPECT_EQ(rd(c, 15), 0x22);
}

TEST(Ay38910, Variante8912NurPortA) {
    Ay38910 c(Ay38910::Variante::AY8912);
    wr(c, 15, 0x77);                     // R15 bleibt Register
    EXPECT_EQ(c.reg(15), 0x77);
    wr(c, 7, 0x80);                      // B als Ausgang
    EXPECT_EQ(rd(c, 15), 0x77);
    EXPECT_EQ(c.portPins(1), 0xFF);   // Port B hat am 8912 keine Pins
    wr(c, 7, 0x00);                      // B als Eingang: keine Pins -> Pull-up
    c.setzePortEingang(1, 0x00);
    EXPECT_EQ(rd(c, 15), 0xFF);
    c.setzePortEingang(0, 0x42);
    EXPECT_EQ(rd(c, 14), 0x42);
}

TEST(Ay38910, TonfrequenzAusNulldurchgaengen) {
    struct F { int per; double toleranz; };
    for (F f : {F{0x6F, 0.005}, F{0xFFF, 0.005}, F{0x10, 0.005}}) {
        Ay38910 c(Ay38910::Variante::AY8910, 1789772);
        tonA(c, f.per);
        const double soll = 1789772.0 / (16.0 * f.per);
        auto a = sammle(c, f.per == 0xFFF ? 1.0 : 0.3, 48000);
        const double ist = frequenz(a, 48000);
        EXPECT_NEAR(ist, soll, soll * f.toleranz) << "Periode " << f.per;
    }
}

TEST(Ay38910, TonPeriodeEinsUndNullGleich) {
    // Periode 1: ein Rasterschritt je Halbwelle -> f/16; Rate = f/8 (ein Rasterschritt je Wert)
    for (int per : {1, 0}) {
        Ay38910 c(Ay38910::Variante::AY8910, 1792000);
        tonA(c, per);
        c.ausgabeEin(224000, 0.0f);
        c.rechneBis(1792000 / 100);
        std::vector<Ay38910::Abtastwert> w(c.abholbereit());
        c.abholen(w.data(), w.size());
        ASSERT_GT(w.size(), 100u);
        for (size_t i = 1; i < w.size(); ++i)
            EXPECT_NE(w[i].a > 0.5f, w[i - 1].a > 0.5f) << "Periode " << per << " Wert " << i;
        EXPECT_NEAR(frequenz([&] { std::vector<float> v; for (auto& x : w) v.push_back(x.a); return v; }(),
                             224000), 1792000.0 / 16.0, 1792000.0 / 16.0 * 0.005);
    }
}

TEST(Ay38910, RauschfolgeWiederholtSich) {
    // Rauschen allein hoerbar: Kanal A, Ton aus, Pegel 15; Periode 1 (Schiebetakt Takt/16)
    Ay38910 c(Ay38910::Variante::AY8910, 1792000);
    wr(c, 6, 1);
    wr(c, 7, 0x37);   // Ton aus (alle), Rauschen A an
    wr(c, 8, 0x0F);
    c.ausgabeEin(224000, 0.0f);   // ein Wert je Rasterschritt, jedes Rauschbit liegt zweimal an
    const size_t N = 131071;   // 2^17 - 1
    c.rechneBis(static_cast<uint64_t>(N) * 2 * 16 + 64);
    std::vector<Ay38910::Abtastwert> roh(c.abholbereit());
    c.abholen(roh.data(), roh.size());
    std::vector<Ay38910::Abtastwert> w;   // jedes zweite = ein Rauschbit
    for (size_t i = 1; i < roh.size(); i += 2) w.push_back(roh[i]);
    ASSERT_GE(w.size(), 2 * N);
    int diffNachN = 0, diffKurz = 0;
    for (size_t i = 0; i < N; ++i) {
        diffNachN += (w[i].a > 0.5f) != (w[i + N].a > 0.5f);
        diffKurz += (w[i].a > 0.5f) != (w[i + N / 2].a > 0.5f);
    }
    EXPECT_EQ(diffNachN, 0);
    EXPECT_GT(diffKurz, 1000);    // keine kuerzere Periode
    // Folge ist nicht konstant: etwa halb Einsen
    int einsen = 0;
    for (size_t i = 0; i < N; ++i) einsen += w[i].a > 0.5f;
    EXPECT_NEAR(einsen, N / 2, N / 100);   // 2^16 von 2^17-1 Bits
}

TEST(Ay38910, HuellkurvenFormen) {
    // Stufe = Datenblatt: D = 15..0, U = 0..15
    const std::string D = "FEDCBA9876543210", U = "0123456789ABCDEF";
    const std::string erw[16] = {
        D + std::string(24, '0'),            // 0
        D + std::string(24, '0'),            // 1
        D + std::string(24, '0'),            // 2
        D + std::string(24, '0'),            // 3
        U + std::string(24, '0'),            // 4
        U + std::string(24, '0'),            // 5
        U + std::string(24, '0'),            // 6
        U + std::string(24, '0'),            // 7
        D + D + D,                           // 8
        D + std::string(24, '0'),            // 9
        D + U + D,                           // 10
        D + std::string(24, 'F'),            // 11
        U + U + U,                           // 12
        U + std::string(24, 'F'),            // 13
        U + D + U,                           // 14
        U + std::string(24, '0'),            // 15
    };
    for (int form = 0; form < 16; ++form) {
        Ay38910 c;
        wr(c, 11, 1);
        wr(c, 12, 0);
        wr(c, 13, static_cast<uint8_t>(form));
        std::string ist;
        for (int k = 0; k < 40; ++k) {
            c.rechneBis(static_cast<uint64_t>(k) * 16);
            ist += "0123456789ABCDEF"[c.huellkurvePegel()];
        }
        EXPECT_EQ(ist, erw[form].substr(0, 40)) << "Form " << form;
    }
}

TEST(Ay38910, HuellkurvenPeriodeUndNeustart) {
    Ay38910 c;
    wr(c, 11, 3);   // EP = 3 -> Schritt alle 48 Takte
    wr(c, 13, 0x0C);
    c.rechneBis(47);
    EXPECT_EQ(c.huellkurvePegel(), 0);
    c.rechneBis(48);
    EXPECT_EQ(c.huellkurvePegel(), 1);
    c.rechneBis(48 * 5);
    EXPECT_EQ(c.huellkurvePegel(), 5);
    wr(c, 13, 0x0C);   // Neustart
    EXPECT_EQ(c.huellkurvePegel(), 0);
    c.rechneBis(48 * 5 + 40);
    EXPECT_EQ(c.huellkurvePegel(), 0);   // Teiler ebenfalls neu
    c.rechneBis(48 * 5 + 48);
    EXPECT_EQ(c.huellkurvePegel(), 1);
    // Schreiben mit Zeitangabe rechnet vorher nach
    c.adresseSchreiben(13);
    c.datenSchreiben(0x08, 48 * 10);
    EXPECT_EQ(c.huellkurvePegel(), 15);
}

TEST(Ay38910, HuellkurveAlsKanalpegel) {
    Ay38910 c(Ay38910::Variante::AY8910, 1792000);
    wr(c, 7, 0x3F);       // alles aus -> Kanal liegt konstant auf dem Pegel
    wr(c, 8, 0x10);       // Huellkurve
    wr(c, 11, 1);
    wr(c, 13, 0x0D);      // /^^^ : hoch und halten
    c.ausgabeEin(112000, 0.0f);
    c.rechneBis(16 * 40);
    std::vector<Ay38910::Abtastwert> w(c.abholbereit());
    c.abholen(w.data(), w.size());
    ASSERT_GE(w.size(), 30u);
    EXPECT_NEAR(w.back().a, 1.0f, 1e-6);
    EXPECT_LT(w[0].a, 0.01f);          // erster Wert mittelt noch Stufe 0/1
    EXPECT_GT(w[20].a, w[10].a);
}

TEST(Ay38910, PegeltabelleLogarithmisch) {
    auto t = Ay38910::vorgabePegel();
    EXPECT_FLOAT_EQ(t[0], 0.0f);
    EXPECT_FLOAT_EQ(t[15], 1.0f);
    for (int n = 2; n < 16; ++n) EXPECT_NEAR(t[n - 1] / t[n], 1.0 / std::sqrt(2.0), 1e-5);
}

TEST(Ay38910, ResetIstStill) {
    Ay38910 c(Ay38910::Variante::AY8910, 1792000);
    tonA(c, 5);
    wr(c, 13, 0x0A);
    c.rechneBis(1000);
    c.reset();
    for (int r = 0; r < 16; ++r) {
        if (r == 14 || r == 15) continue;   // Eingang, Pins
        EXPECT_EQ(c.reg(r), 0) << r;
    }
    EXPECT_EQ(c.latch(), 0);
    c.ausgabeEin(48000, 0.0f);
    c.rechneBis(c.takt() + 17920);
    std::vector<Ay38910::Abtastwert> w(c.abholbereit());
    c.abholen(w.data(), w.size());
    ASSERT_GT(w.size(), 10u);
    for (auto& x : w) { EXPECT_EQ(x.a, 0.0f); EXPECT_EQ(x.b, 0.0f); EXPECT_EQ(x.c, 0.0f); }
}

TEST(Ay38910, OhneAusgabeNurRegisterNachfuehren) {
    Ay38910 c;
    tonA(c, 3);
    c.rechneBis(1789772);
    EXPECT_EQ(c.abholbereit(), 0u);
    EXPECT_FALSE(c.ausgabeAktiv());
}

// Aliasing-Probe: hoher Ton, Rechteck mit Oberwellen weit ueber der halben Abtastrate.  Das
// Kastenfilter allein (exakte Grenzen) reicht bei manchen Perioden nicht; deshalb hat der Chip
// einen Vorfilter auf dem Raster (zwei einpolige Stufen, hier 8 kHz).  Die Oberwellen eines
// Rechtecks lassen sich damit nicht vollstaendig unterdruecken (Periode 9: 3. Oberwelle
// faltet nach 10,7 kHz) - die Grenze gilt fuer Periode 6/7 (ca. 18,6/16 kHz), fuer die uebrigen
// wird nur verlangt, dass der Vorfilter deutlich hilft.
static double spiegelton(float tiefpass, int per) {
    Ay38910 c(Ay38910::Variante::AY8910, 1789772);
    tonA(c, per);
    c.ausgabeEin(48000, tiefpass);
    c.rechneBis(1789772 / 4);   // Einschwingen
    c.ausgabeEin(48000, tiefpass);   // neu starten (Einschwingen des Vorfilters liegt davor)
    c.rechneBis(c.takt() + 1789772 / 4);
    std::vector<Ay38910::Abtastwert> w(c.abholbereit());
    c.abholen(w.data(), w.size());
    const size_t N = 8192;
    EXPECT_GE(w.size(), N);
    std::vector<double> x(N);
    double mittel = 0;
    for (size_t i = 0; i < N; ++i) mittel += w[i].a;
    mittel /= N;
    for (size_t i = 0; i < N; ++i)
        x[i] = (w[i].a - mittel) * (0.5 - 0.5 * std::cos(2 * M_PI * i / (N - 1)));
    const double fgrund = 1789772.0 / (16.0 * per);
    double grund = 0, spur = 0;
    for (size_t k = 1; k < N / 2; ++k) {
        const double f = 48000.0 * k / N;
        if (f > 20000) break;
        std::complex<double> s = 0;
        for (size_t i = 0; i < N; ++i) s += x[i] * std::polar(1.0, -2 * M_PI * k * i / N);
        const double m = std::abs(s);
        if (std::fabs(f - fgrund) < 400) grund = std::max(grund, m);
        else spur = std::max(spur, m);
    }
    return 20 * std::log10(spur / grund);
}

TEST(Ay38910, AliasingProbe) {
    for (int per : {6, 7}) EXPECT_LT(spiegelton(8000.0f, per), -40.0) << "Periode " << per;
    for (int per : {9, 12})
        EXPECT_LT(spiegelton(8000.0f, per), spiegelton(0.0f, per) - 10.0) << "Periode " << per;
}
