/**
 * @file test_k3521.cpp
 * @brief Unit-Tests der K3521.30 (akkugepuffertes 4-KB-RAM, Entwurf 33 N10b).
 *
 * Geprueft: Lesen/Schreiben, Adresswahl im 4-KB-Raster, Ablehnung unzulaessiger Basen,
 * Power-on 0xFF, /MEMDI (Koppelleitung und Bus-/MEMDI), Ueberdeckung durch ein Lese-ROM,
 * /SUE als reiner Zustand, dauerhafte Ablage (Laden, Zurueckschreiben nach Schreibpause,
 * Destruktor, falsche Dateigroesse).
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include "core/cards/k3521/k3521.h"
#include "tests/support/temp_path.h"

namespace {
K3521 karte(uint16_t base, K3521::Memdi m = K3521::Memdi::Bus) {
    K3521::Config c;
    c.base_addr = base;
    c.memdi = m;
    return K3521(c);
}

class Vorrang5000 : public MemdiDriver {
public:
    uint8_t cell = 0x5A;
    bool drivesMemdi(uint16_t a) override { return (a & 0xF000) == 0x5000; }
    uint8_t memRead(uint16_t) override { return cell; }
    void memWrite(uint16_t, uint8_t d) override { cell = d; }
};

class Rom : public MemDevice {
public:
    uint8_t memRead(uint16_t) override { return 0xC3; }
    void memWrite(uint16_t, uint8_t) override {}
    bool isWritable() const override { return false; }
};

std::vector<uint8_t> datei(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
void schreibe(const std::string& p, size_t n, uint8_t v) {
    std::ofstream f(p, std::ios::binary);
    std::vector<char> b(n, static_cast<char>(v));
    f.write(b.data(), static_cast<std::streamsize>(n));
}
}  // namespace

TEST(K3521, PowerOnLiest0xFF) {
    K3521 k;
    EXPECT_EQ(k.memRead(0x0000), 0xFF);
    EXPECT_EQ(k.peek(0x0FFF), 0xFF);
}

TEST(K3521, LesenSchreibenAnBasisUndEnde) {
    auto k = karte(0xF000);
    k.memWrite(0xF000, 0x12);
    k.memWrite(0xFFFF, 0x34);
    EXPECT_EQ(k.memRead(0xF000), 0x12);
    EXPECT_EQ(k.memRead(0xFFFF), 0x34);
    EXPECT_EQ(k.rawPtr()[0], 0x12);
    EXPECT_EQ(k.rawPtr()[0xFFF], 0x34);
}

TEST(K3521, AdressenAusserhalbWerdenIgnoriert) {
    auto k = karte(0x4000);
    k.memWrite(0x3FFF, 0x11);
    k.memWrite(0x5000, 0x22);
    EXPECT_EQ(k.memRead(0x3FFF), 0xFF);
    EXPECT_EQ(k.memRead(0x5000), 0xFF);
    EXPECT_EQ(k.peek(0), 0xFF);
    EXPECT_EQ(k.peek(0x0FFF), 0xFF);
}

TEST(K3521, JedeVierKilobyteBasisIstErlaubt) {
    for (int i = 0; i < 16; ++i) EXPECT_NO_THROW(karte(static_cast<uint16_t>(i * 0x1000)));
    EXPECT_THROW(karte(0x0800), std::invalid_argument);
    EXPECT_THROW(karte(0x4001), std::invalid_argument);
}

TEST(K3521, AmBusUndUeberdecktVonEinemLeseRom) {
    K1520Bus bus;
    auto k = karte(0x0000);
    k.attachToBus(bus);
    Rom rom;
    bus.registerMem(&rom, 0x0000, 0x400);
    bus.memWrite(0x0010, 0x77);          // ROM nimmt nicht an, die K3521 schon
    bus.memWrite(0x0500, 0x66);
    EXPECT_EQ(bus.memRead(0x0010), 0xC3);   // ROM ueberdeckt beim Lesen
    EXPECT_EQ(k.peek(0x0010), 0x77);
    EXPECT_EQ(bus.memRead(0x0500), 0x66);
}

TEST(K3521, KoppelMemdiSperrtLesenUndSchreiben) {
    auto k = karte(0x4000, K3521::Memdi::Memdi1);
    k.memWrite(0x4000, 0x11);
    k.setMemDI(true);
    EXPECT_EQ(k.memRead(0x4000), 0xFF);
    k.memWrite(0x4000, 0x22);
    k.setMemDI(false);
    EXPECT_EQ(k.memRead(0x4000), 0x11) << "gesperrte Schreibzugriffe wirken nicht";
}

TEST(K3521, BusMemdiVerwirftSchreibenBeiVorrangspeicher) {
    K1520Bus bus;
    auto k = karte(0x5000);
    k.attachToBus(bus);
    Vorrang5000 v;
    bus.addMemdiDriver(&v);
    bus.memWrite(0x5000, 0x99);
    EXPECT_EQ(k.peek(0), 0xFF) << "Bus-/MEMDI aktiv: die K3521 nimmt nicht an";
    EXPECT_EQ(v.cell, 0x99);
}

TEST(K3521, SueIstNurEinZustandUndResetLoeschtNurIhn) {
    auto k = karte(0x4000);
    k.memWrite(0x4000, 0x42);
    EXPECT_FALSE(k.sue());
    k.setzeAkkuLeer(true);
    EXPECT_TRUE(k.sue());
    EXPECT_EQ(k.memRead(0x4000), 0x42) << "ein leerer Akku aendert den Inhalt im Modell nicht";
    k.reset();
    EXPECT_FALSE(k.sue());
    EXPECT_EQ(k.memRead(0x4000), 0x42) << "RESET erhaelt den Inhalt";
}

TEST(K3521, DateiNeuWirdBeimFlushAngelegtUndGeladen) {
    const std::string p = k1520test::tempPath("k3521_neu.bin");
    std::filesystem::remove(p);
    {
        auto k = karte(0x4000);
        std::string f;
        ASSERT_TRUE(k.dateiBinden(p, f)) << f;
        EXPECT_FALSE(std::filesystem::exists(p)) << "angelegt wird erst beim Zurueckschreiben";
        k.memWrite(0x4001, 0xAB);
        ASSERT_TRUE(k.flush());
        const auto d = datei(p);
        ASSERT_EQ(d.size(), 4096u);
        EXPECT_EQ(d[1], 0xAB);
        EXPECT_EQ(d[0], 0xFF);
    }
    auto k2 = karte(0x8000);   // andere Adresse, derselbe Inhalt (die Batterie sitzt in der Karte)
    std::string f;
    ASSERT_TRUE(k2.dateiBinden(p, f)) << f;
    EXPECT_EQ(k2.memRead(0x8001), 0xAB);
    std::filesystem::remove(p);
}

TEST(K3521, AutoFlushWartetDieSchreibpauseAb) {
    const std::string p = k1520test::tempPath("k3521_pause.bin");
    std::filesystem::remove(p);
    auto k = karte(0x4000);
    std::string f;
    ASSERT_TRUE(k.dateiBinden(p, f)) << f;
    k.memWrite(0x4000, 0x01);
    k.autoFlush(1000);                                   // Aenderung bemerkt, Pause beginnt
    k.autoFlush(1000 + K3521::kSchreibpause - 1);
    EXPECT_FALSE(std::filesystem::exists(p));
    k.memWrite(0x4000, 0x02);                            // weiter geschrieben: Pause beginnt neu
    k.autoFlush(1000 + K3521::kSchreibpause);
    EXPECT_FALSE(std::filesystem::exists(p));
    k.autoFlush(1000 + 2 * K3521::kSchreibpause);
    ASSERT_TRUE(std::filesystem::exists(p));
    EXPECT_EQ(datei(p)[0], 0x02);
    std::filesystem::remove(p);
}

TEST(K3521, DestruktorSchreibtZurueck) {
    const std::string p = k1520test::tempPath("k3521_dtor.bin");
    std::filesystem::remove(p);
    {
        auto k = karte(0x4000);
        std::string f;
        ASSERT_TRUE(k.dateiBinden(p, f)) << f;
        k.memWrite(0x4010, 0x5C);
    }
    ASSERT_TRUE(std::filesystem::exists(p));
    EXPECT_EQ(datei(p)[0x10], 0x5C);
    std::filesystem::remove(p);
}

TEST(K3521, FalscheDateigroesseWirdAbgelehntUndAendertNichts) {
    const std::string p = k1520test::tempPath("k3521_falsch.bin");
    schreibe(p, 4095, 0x11);
    auto k = karte(0x4000);
    k.memWrite(0x4000, 0x33);
    std::string f;
    EXPECT_FALSE(k.dateiBinden(p, f));
    EXPECT_NE(f.find("4096"), std::string::npos) << f;
    EXPECT_EQ(k.memRead(0x4000), 0x33);
    EXPECT_TRUE(k.datei().empty());
    schreibe(p, 4097, 0x11);
    EXPECT_FALSE(k.dateiBinden(p, f));
    std::filesystem::remove(p);
}

TEST(K3521, AufKoppelMemdiGebruecktIgnoriertSieBusMemdi) {
    K1520Bus bus;
    auto k = karte(0x5000, K3521::Memdi::Memdi2);
    k.attachToBus(bus);
    Vorrang5000 v;
    bus.addMemdiDriver(&v);
    bus.memWrite(0x5000, 0x99);
    EXPECT_EQ(k.peek(0), 0x99);
}
