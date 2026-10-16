/**
 * @file test_a5120_bildschirm.cpp
 * @brief Zwei Bildschirmkarten mit Umschalter (doc/design/33_a5130_k8924.md §3.2–§3.5, AP N2).
 *
 * `Config::bildschirm = Beide`: K7024 (F800H) und K7023 (FC00H) stecken zugleich.  Jeder
 * Schreibzugriff in FC00–FFFFH trifft beide Karten, gelesen wird dort die K7023 (zuletzt
 * registriert).  Der Umschalter waehlt das angezeigte Bild (framebuffer, screenChar) und die
 * Wickelbruecke Port 0AH Bit 6; der Bildspeicher bleibt unberuehrt.  Dazu der Zwischenstand
 * (v9) und der CP/A-Kaltstart in beiden Schalterstellungen.
 */
#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/machines/a5120/a5120.h"
#include "tests/support/fixtures.h"
#include "tests/support/machine_run.h"

namespace {

using C = A5120Machine::Config;
using B = C::Bildschirm;

std::unique_ptr<A5120Machine> baue(B bildschirm, B schalter = B::K7024) {
    C c;
    c.bildschirm          = bildschirm;
    c.bildschirm_schalter = schalter;
    auto m = std::make_unique<A5120Machine>(c);
    m->powerOn();
    return m;
}

/// Port 0AH (BS-PIO B) in Betriebsart 3, alle Leitungen Eingang — wie CP/A beim Kaltstart.
uint8_t port0A(A5120Machine& m) {
    m.bus().ioWrite(0x0B, 0xCF);
    m.bus().ioWrite(0x0B, 0xFF);
    return m.bus().ioRead(0x0A);
}

/// K7024-Zelle einer Busadresse in F800–FF7FH (80 Spalten).
uint8_t k7024Zelle(const A5120Machine& m, uint16_t addr) {
    const int off = addr - 0xF800;
    return m.k7024().vramRead(off % 80, off / 80);
}

/// Text der angezeigten Karte (64×16 bzw. 80×24), nur ueber screenChar.
std::string bildText(const A5120Machine& m) {
    const int sp = m.zeigtK7023() ? 64 : 80, ze = m.zeigtK7023() ? 16 : 24;
    std::string s;
    for (int r = 0; r < ze; ++r) {
        for (int c = 0; c < sp; ++c) {
            const uint8_t ch = m.screenChar(c, r) & 0x7F;
            s.push_back((ch >= 0x20 && ch < 0x7F) ? static_cast<char>(ch) : ' ');
        }
        s.push_back('\n');
    }
    return s;
}

std::vector<uint8_t> speicherbild(A5120Machine& m) {
    std::vector<uint8_t> v(0x10000);
    for (uint32_t a = 0; a < 0x10000; ++a) v[a] = m.memReadDebug(static_cast<uint16_t>(a));
    return v;
}

std::vector<uint8_t> dateiInhalt(const std::string& pfad) {
    std::ifstream f(pfad, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

}  // namespace

TEST(A5120Bildschirm, BeideKartenAmBus) {
    auto m = baue(B::Beide);
    ASSERT_NE(m->k7023(), nullptr);
    EXPECT_EQ(m->bildschirm(), B::Beide);
    EXPECT_EQ(m->bildschirmSchalter(), B::K7024);
    EXPECT_FALSE(m->zeigtK7023());
    // Schreiben nach FC10H trifft beide Karten in derselben Zelle.
    m->memWriteDebug(0xFC10, 0x41);
    EXPECT_EQ(m->k7023()->vramRead(16, 0), 0x41);
    EXPECT_EQ(k7024Zelle(*m, 0xFC10), 0x41);
    // F800–FBFFH gehoert nur der K7024 (die K7023 beginnt bei FC00H).
    m->memWriteDebug(0xF805, 'Z');
    EXPECT_EQ(k7024Zelle(*m, 0xF805), 'Z');
    EXPECT_EQ(m->memReadDebug(0xF805), 'Z');
    EXPECT_EQ(m->screenChar(5, 0), 'Z');
}

/// Waechter (Entwurf 33 N2, Fallen): `Beide` haengt am Bus-Verhalten „Lesen = zuletzt
/// registrierte Karte, Schreiben an alle".  Laufen die Inhalte auseinander (nur am Bus vorbei
/// moeglich), liest FC00–FFFFH die K7023.
TEST(A5120Bildschirm, UeberlappungLesenLetzteSchreibenAlle) {
    auto m = baue(B::Beide);
    m->memWriteDebug(0xFC20, 0x11);
    ASSERT_EQ(m->k7023()->vramRead(32, 0), 0x11);
    ASSERT_EQ(k7024Zelle(*m, 0xFC20), 0x11);
    m->k7023()->vramWrite(32, 0, 0x22);   // nur die K7023 (Kartenzugriff, nicht der Bus)
    EXPECT_EQ(k7024Zelle(*m, 0xFC20), 0x11);
    EXPECT_EQ(m->memReadDebug(0xFC20), 0x22) << "Lesen in FC00–FFFFH = K7023";
    EXPECT_EQ(m->memReadDebug(0xFFFF), m->k7023()->vramRead(63, 15));
}

TEST(A5120Bildschirm, UmschaltenAendertNurDasBild) {
    auto m = baue(B::Beide);
    m->memWriteDebug(0xF800, 'A');   // K7024 Zelle (0,0)
    m->memWriteDebug(0xFC00, 'B');   // K7023 Zelle (0,0) = K7024 Zelle (64,12)
    m->fbClearDirty();
    const auto vorher = speicherbild(*m);

    EXPECT_EQ(m->fbWidth(), 640);
    EXPECT_EQ(m->fbHeight(), 288);
    EXPECT_EQ(m->framebuffer(), m->k7024().getFramebuffer());
    EXPECT_EQ(m->screenChar(0, 0), 'A');
    EXPECT_EQ(m->screenChar(64, 12), 'B');

    m->setBildschirmSchalter(B::K7023);
    EXPECT_EQ(m->bildschirmSchalter(), B::K7023);
    EXPECT_TRUE(m->zeigtK7023());
    EXPECT_TRUE(m->fbDirty()) << "neues Bild muss gezeichnet werden";
    EXPECT_EQ(m->fbWidth(), 512);
    EXPECT_EQ(m->fbHeight(), 256);
    EXPECT_EQ(m->framebuffer(), m->k7023()->getFramebuffer());
    EXPECT_EQ(m->screenChar(0, 0), 'B');
    EXPECT_EQ(speicherbild(*m), vorher) << "Umschalten aendert den Speicher nicht";
    m->fbClearDirty();
    EXPECT_FALSE(m->fbDirty());

    m->setBildschirmSchalter(B::K7023);   // gleiche Stellung: kein neues Bild
    EXPECT_FALSE(m->fbDirty());
    m->setBildschirmSchalter(B::K7024);
    EXPECT_EQ(m->screenChar(0, 0), 'A');
    EXPECT_EQ(speicherbild(*m), vorher);
}

TEST(A5120Bildschirm, BrueckeFolgtDemSchalter) {
    auto ref = baue(B::K7024);
    const uint8_t p_ref = port0A(*ref);
    auto m = baue(B::Beide);
    const uint8_t p24 = port0A(*m);
    EXPECT_EQ(p24 & 0x40, 0) << "K7024 → BAB2";
    EXPECT_EQ(p24, p_ref) << "Stellung K7024 = K7024 allein";
    // Zur Laufzeit, ohne Reset: der naechste IN 0AH sieht die neue Bruecke.
    m->setBildschirmSchalter(B::K7023);
    const uint8_t p23 = m->bus().ioRead(0x0A);
    EXPECT_EQ(p23 & 0x40, 0x40) << "K7023 → BAB1";
    EXPECT_EQ(p23 & ~0x40 & 0xFF, p_ref & ~0x40 & 0xFF) << "Bit 5/7 (und alle anderen) unveraendert";
    m->setBildschirmSchalter(B::K7024);
    EXPECT_EQ(m->bus().ioRead(0x0A), p_ref);
    // Anfangsstellung aus der Config.
    auto n = baue(B::Beide, B::K7023);
    EXPECT_EQ(n->bildschirmSchalter(), B::K7023);
    EXPECT_EQ(port0A(*n) & 0x40, 0x40);
}

TEST(A5120Bildschirm, SchalterNurBeiBeide) {
    auto m = baue(B::K7024);
    EXPECT_THROW(m->setBildschirmSchalter(B::K7023), std::logic_error);
    EXPECT_EQ(m->bildschirmSchalter(), B::K7024);
    auto n = baue(B::K7023);
    EXPECT_THROW(n->setBildschirmSchalter(B::K7024), std::logic_error);
    EXPECT_EQ(n->bildschirmSchalter(), B::K7023);
    EXPECT_TRUE(n->zeigtK7023());
    auto b = baue(B::Beide);
    EXPECT_THROW(b->setBildschirmSchalter(B::Beide), std::invalid_argument);
    C c;
    c.bildschirm = c.bildschirm_schalter = B::Beide;
    EXPECT_THROW(A5120Machine{c}, std::invalid_argument);
}

TEST(A5120Bildschirm, ZwischenstandMitBeidenKarten) {
    auto m = baue(B::Beide);
    m->memWriteDebug(0xF800, 'K');   // nur K7024
    m->memWriteDebug(0xFC00, 'S');   // beide
    m->setBildschirmSchalter(B::K7023);
    A5120Machine::MachineSnapshot s;
    m->captureState(s);

    auto datei = k1520test::TempDisk::empty("a5120_beide.k1520state");
    ASSERT_TRUE(m->saveState(datei.path()));
    EXPECT_EQ(dateiInhalt(datei.path()).at(7), 9) << "Zustandsversion 9";

    auto n = baue(B::Beide);   // Schalter K7024, Bildspeicher leer
    ASSERT_TRUE(n->loadState(datei.path())) << n->stateError();
    EXPECT_EQ(n->bildschirmSchalter(), B::K7023) << "Schalterstellung bleibt";
    EXPECT_EQ(n->screenChar(0, 0), 'S');
    EXPECT_EQ(k7024Zelle(*n, 0xF800), 'K');
    EXPECT_EQ(k7024Zelle(*n, 0xFC00), 'S');

    // Rundlauf im Speicher (Debugger-Snapshot) setzt den Schalter ebenso zurueck.
    n->setBildschirmSchalter(B::K7024);
    ASSERT_TRUE(n->restoreState(s));
    EXPECT_EQ(n->bildschirmSchalter(), B::K7023);

    // Ein Stand mit beiden Karten in eine Maschine mit K7024 allein: nur die K7024 zaehlt.
    auto e = baue(B::K7024);
    ASSERT_TRUE(e->loadState(datei.path())) << e->stateError();
    EXPECT_EQ(e->screenChar(0, 0), 'K');
}

/// K7024 allein: Geraeteteil wie v8 (nur die Versionsnummer ist 9); ein v8-Stand laedt unveraendert.
TEST(A5120Bildschirm, ZwischenstandK7024AlleinUnveraendert) {
    auto m = baue(B::K7024);
    m->memWriteDebug(0xF800, 'V');
    auto datei = k1520test::TempDisk::empty("a5120_k7024.k1520state");
    ASSERT_TRUE(m->saveState(datei.path()));
    auto bytes = dateiInhalt(datei.path());
    ASSERT_GT(bytes.size(), 8u);
    bytes[7] = 8;   // derselbe Inhalt als v8-Stand
    auto v8 = k1520test::TempDisk::empty("a5120_k7024_v8.k1520state");
    {
        std::ofstream f(v8.path(), std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    auto n = baue(B::K7024);
    ASSERT_TRUE(n->loadState(v8.path())) << n->stateError();
    EXPECT_EQ(n->screenChar(0, 0), 'V');
    // Ein K7024-Stand in eine Maschine mit beiden Karten: K7023 und Schalter bleiben.
    auto b = baue(B::Beide, B::K7023);
    ASSERT_TRUE(b->loadState(datei.path())) << b->stateError();
    EXPECT_EQ(b->bildschirmSchalter(), B::K7023);
    EXPECT_EQ(k7024Zelle(*b, 0xF800), 'V');
}

// ─── CP/A-Kaltstart in beiden Schalterstellungen (Abnahme N2) ──────────────────

namespace {

bool laufeBis(A5120Machine& m, const std::string& needle, long long max_cycles) {
    for (long long n = 0; n < max_cycles; n += k1520test::kSmallBatch) {
        m.run(static_cast<int>(k1520test::kSmallBatch));
        if ((n % 100'000) != 0) continue;
        if (bildText(m).find(needle) != std::string::npos) return true;
    }
    return bildText(m).find(needle) != std::string::npos;
}

}  // namespace

class BootIntegrationBeide : public ::testing::TestWithParam<B> {};

/**
 * @brief CP/A 25.09.89 bootet mit beiden Karten bis `A>` — im Format der Schalterstellung beim
 *        Kaltstart (K7024 → 24×80 ab F800H, K7023 → 16×64 ab FC00H).  Danach Umschalten ohne
 *        Reset: die andere Karte zeigt dieselben Bytes anders gedeutet, das Bild der zuerst
 *        gewaehlten bleibt formatgerecht (§3.2 Punkt 3).
 */
TEST_P(BootIntegrationBeide, CpaBisPromptImFormatDerStellung) {
    const B stellung = GetParam();
    const B andere   = stellung == B::K7024 ? B::K7023 : B::K7024;
    auto m = baue(B::Beide, stellung);
    k1520test::TempDisk a("cpa_cpa780_k5601_noclock.hfe");
    ASSERT_TRUE(m->mountDisk(0, a.path(), "cpa780", false)) << m->lastError();
    ASSERT_TRUE(laufeBis(*m, "\nA>", 80'000'000)) << "kein A>:\n" << bildText(*m);
    const std::string bild = bildText(*m);

    // Ein Programmstueck schreibt im K7023-Format nach FC00H (Zeile 0 der K7023).
    const std::string text = "BAB1-TEXT";
    for (size_t i = 0; i < text.size(); ++i)
        m->memWriteDebug(static_cast<uint16_t>(0xFC00 + i), static_cast<uint8_t>(text[i]));
    m->setBildschirmSchalter(B::K7023);
    EXPECT_EQ(bildText(*m).substr(0, text.size()), text) << "K7023 zeigt das Programmbild";
    m->setBildschirmSchalter(B::K7024);
    EXPECT_NE(bildText(*m).substr(0, text.size()), text)
        << "die K7024 deutet FC00H als Zeile 12, Spalte 64";
    EXPECT_EQ(bildText(*m).substr(12 * 81 + 64, text.size()), text);

    // Zurueck zur Stellung des Kaltstarts; die andere Stellung zeigt kein A> an derselben Stelle.
    m->setBildschirmSchalter(andere);
    const std::string fremd = bildText(*m);
    m->setBildschirmSchalter(stellung);
    EXPECT_NE(fremd, bild);
    if (stellung == B::K7024)
        EXPECT_EQ(bildText(*m).substr(0, 12 * 81), bild.substr(0, 12 * 81))
            << "das K7024-Bild oberhalb von FC00H bleibt formatgerecht";
}

INSTANTIATE_TEST_SUITE_P(Stellung, BootIntegrationBeide, ::testing::Values(B::K7024, B::K7023),
                         [](const ::testing::TestParamInfo<B>& i) {
                             return i.param == B::K7024 ? std::string("K7024") : std::string("K7023");
                         });
