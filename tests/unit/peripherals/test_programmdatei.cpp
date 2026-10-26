/**
 * @file test_programmdatei.cpp
 * @brief JKCEMU-Programmdateiformate (Headersave, KCC, KC-TAP, Intel-HEX): hin und zurueck,
 *        Bytelage nach doc/kassette/jkcemu_formate.md, strenge Pruefung.
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/peripherals/kassette/programmdatei.h"

using namespace kassette::programmdatei;

namespace {

Abbild beispiel(size_t n = 300) {
    Abbild a;
    a.anfang = 0x0400;
    a.start = 0x0434;
    a.name = "TEST";
    a.typ = 'C';
    for (size_t i = 0; i < n; ++i) a.daten.push_back(uint8_t(i * 7 + 1));
    return a;
}

std::vector<uint8_t> schreibe(const Abbild& a, Format f, const SchreibOptionen& o = {}) {
    std::string fe;
    auto r = schreiben(a, f, fe, o);
    EXPECT_TRUE(r) << fe;
    return r.value_or(std::vector<uint8_t>{});
}

}  // namespace

TEST(Programmdatei, HeadersaveBytelage) {
    auto d = schreibe(beispiel(), Format::Headersave);
    ASSERT_EQ(d.size(), 32u + 300u);
    EXPECT_EQ(d[0], 0x00); EXPECT_EQ(d[1], 0x04);   // Anfang 0400H
    EXPECT_EQ(d[2], 0x2B); EXPECT_EQ(d[3], 0x05);   // Ende 052BH inklusiv
    EXPECT_EQ(d[4], 0x34); EXPECT_EQ(d[5], 0x04);   // Start
    EXPECT_EQ(d[12], 'C');
    EXPECT_EQ(d[13], 0xD3); EXPECT_EQ(d[14], 0xD3); EXPECT_EQ(d[15], 0xD3);
    EXPECT_EQ(std::string(d.begin() + 16, d.begin() + 20), "TEST");
    EXPECT_EQ(d[20], ' ');
    EXPECT_EQ(erkenne(d), Format::Headersave);
}

TEST(Programmdatei, KccBytelage) {
    auto d = schreibe(beispiel(), Format::Kcc);
    ASSERT_EQ(d.size(), 128u + 300u);
    EXPECT_EQ(d[16], 3);
    EXPECT_EQ(d[17], 0x00); EXPECT_EQ(d[18], 0x04);
    EXPECT_EQ(d[19], 0x2C); EXPECT_EQ(d[20], 0x05);  // KC85: Ende + 1
    EXPECT_EQ(d[21], 0x34); EXPECT_EQ(d[22], 0x04);
    auto z = schreibe(beispiel(), Format::Kcc, {false});
    EXPECT_EQ(z[19], 0x2B);  // Z9001/JTC: reale Endadresse
}

TEST(Programmdatei, KcTapBytelage) {
    auto d = schreibe(beispiel(), Format::KcTap);
    ASSERT_EQ(d.size(), 16u + 129u * 4);  // Kopf + 3 Datenbloecke (300 B)
    EXPECT_EQ(d[0], 0xC3); EXPECT_EQ(d[1], 'K');
    EXPECT_EQ(d[16], 0);          // Kopfblock-Nr
    EXPECT_EQ(d[16 + 129], 1);
    EXPECT_EQ(d[16 + 258], 2);
    EXPECT_EQ(d[16 + 387], 0xFF);  // letzter Block
    EXPECT_EQ(erkenne(d), Format::KcTap);
}

TEST(Programmdatei, IntelHexBytelage) {
    Abbild a;
    a.anfang = 0x1234;
    a.daten = {1, 2, 3};
    auto d = schreibe(a, Format::IntelHex);
    EXPECT_EQ(std::string(d.begin(), d.end()), ":03123400010203B1\r\n:00000001FF\r\n");
}

TEST(Programmdatei, HinUndZurueckJedesFormat) {
    for (Format f : {Format::Headersave, Format::Kcc, Format::KcTap, Format::IntelHex}) {
        for (size_t n : {1u, 127u, 128u, 129u, 300u}) {
            Abbild a = beispiel(n);
            std::string fe;
            auto d = schreiben(a, f, fe);
            ASSERT_TRUE(d) << fe;
            auto b = lesen(*d, f == Format::Kcc ? f : Format::Auto, fe);
            ASSERT_TRUE(b) << formatName(f) << " n=" << n << ": " << fe;
            EXPECT_EQ(b->anfang, a.anfang) << formatName(f);
            if (f == Format::Kcc || f == Format::KcTap) {
                // KC85-Endadresse (Ende+1) → JKCEMU laedt ein Byte mehr, sofern die Datei es hergibt
                ASSERT_GE(b->daten.size(), n);
                EXPECT_EQ(std::vector<uint8_t>(b->daten.begin(), b->daten.begin() + long(n)), a.daten);
                EXPECT_LE(b->daten.size(), n + 1);
            } else {
                EXPECT_EQ(b->daten, a.daten) << formatName(f) << " n=" << n;
            }
            if (f != Format::IntelHex) EXPECT_EQ(b->start, a.start) << formatName(f);
        }
    }
}

TEST(Programmdatei, RohHatKeinenKopf) {
    std::string fe;
    auto a = lesen({1, 2, 3}, Format::Auto, fe);
    ASSERT_TRUE(a);
    EXPECT_EQ(a->anfang, 0);
    EXPECT_FALSE(a->mit_adresse);
    EXPECT_EQ(a->daten, (std::vector<uint8_t>{1, 2, 3}));
}

TEST(Programmdatei, MultiKcTapMeldetWeitere) {
    auto e = schreibe(beispiel(10), Format::KcTap);
    auto zwei = e;
    zwei.insert(zwei.end(), e.begin(), e.end());
    std::string fe;
    bool w = false;
    ASSERT_TRUE(lesen(zwei, Format::KcTap, fe, &w));
    EXPECT_TRUE(w);
    ASSERT_TRUE(lesen(e, Format::KcTap, fe, &w));
    EXPECT_FALSE(w);
}

TEST(Programmdatei, WeistKaputtesAb) {
    std::string fe;
    auto d = schreibe(beispiel(), Format::Headersave);
    d.resize(d.size() - 1);
    EXPECT_FALSE(lesen(d, Format::Headersave, fe));  // kuerzer als Endadresse
    auto h = schreibe(beispiel(), Format::IntelHex);
    h[10] = (h[10] == '0') ? '1' : '0';
    EXPECT_FALSE(lesen(h, Format::IntelHex, fe));  // Pruefsumme
    EXPECT_FALSE(lesen({}, Format::Auto, fe));
    auto k = schreibe(beispiel(), Format::KcTap);
    k.resize(16 + 129 + 129);  // Datenbloecke enden vor der Endadresse, kein FFH-Block
    EXPECT_FALSE(lesen(k, Format::KcTap, fe));
    Abbild gross = beispiel(0x200);
    gross.anfang = 0xFF00;
    EXPECT_FALSE(schreiben(gross, Format::Headersave, fe));
}
