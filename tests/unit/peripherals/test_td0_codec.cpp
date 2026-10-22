/**
 * @file test_td0_codec.cpp
 * @brief GoogleTests fuer Td0Codec (TeleDisk-Abbild, Lesen, Entwurf 38 AP T1).
 *
 * Schwerpunkte:
 *   - synthetische Mini-Dateien je Methode 0/1/2, `TD` und `td` (LZHUF), FM global/je Spur,
 *     Flags (Datenfehler, geloescht, ohne Datenfeld, Pseudosektor), ID aus dem Sektorkopf,
 *     doppelte Sektornummern, Nummern >= 100, Spuren ohne Sektoren, Sektorgroessen bis 8192 B
 *   - Kopf-/Spur-CRC (harte Fehler), Kommentar-/Sektor-CRC (Warnung), kaputte und
 *     abgeschnittene Dateien, 0-/20-Byte-Dateien
 *   - der `td`-Vektor stammt aus `tests/python/td0_pack.py` (unabhaengiger Packer, gegen den
 *     C-Entpacker von Greaseweazle geprueft); der Anfang der echten D02V032.TD0 steht in 38a
 *   - `Td0Echtdaten.*` nur mit `K1520_TD0_DIR=<Ordner>` (rekursiv *.td0), sonst uebersprungen
 *   - `Td0Gegenprobe.SchreibtSektorlisten` mit `K1520_TD0_AUSGABE=<Ordner>`: Inhaltsangabe je
 *     Datei fuer tests/python/test_td0_gegenprobe.py (Vergleich mit Greaseweazle)
 *
 * @see core/peripherals/floppy_drive/td0_codec.h
 * @see doc/design/38_td0_imd.md, doc/design/38a_td0_imd_formatbeschreibung.md
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/peripherals/floppy_drive/image_codec.h"
#include "core/peripherals/floppy_drive/sector_track_io.h"
#include "core/peripherals/floppy_drive/td0_codec.h"
#include "core/peripherals/floppy_drive/track_codec.h"
#include "tests/support/temp_path.h"

using Bytes = std::vector<uint8_t>;

namespace {

namespace fs = std::filesystem;

// ─── Baukasten ───────────────────────────────────────────────────────────────

Bytes cat(Bytes a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); return a; }

Bytes fuell(size_t n, uint8_t b) { return Bytes(n, b); }

Bytes steigend(size_t n, unsigned seed = 0) {
    Bytes b(n);
    for (size_t i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(seed + i * 7u + (i >> 3));
    return b;
}

/// Kopf mit gueltiger CRC
Bytes kopf(const char* sig = "TD", uint8_t rate = 0, uint8_t laufwerk = 2, uint8_t schritt = 0,
           uint8_t dos = 0, uint8_t seiten = 2, uint8_t version = 0x15) {
    Bytes h = {static_cast<uint8_t>(sig[0]), static_cast<uint8_t>(sig[1]), 0, 0x4B, version, rate,
               laufwerk, schritt, dos, seiten};
    const uint16_t c = Td0Codec::crc16(h.data(), 10);
    h.push_back(c & 0xFF);
    h.push_back(c >> 8);
    return h;
}

Bytes mitKopfCrc(Bytes h) {
    const uint16_t c = Td0Codec::crc16(h.data(), 10);
    h.resize(10);
    h.push_back(c & 0xFF);
    h.push_back(c >> 8);
    return h;
}

/// Kommentarblock (Zeitstempel Jahr-1900, Monat 0-basiert, Tag, h, min, s)
Bytes kommentar(const std::string& text, bool crcFalsch = false) {
    Bytes b = {static_cast<uint8_t>(text.size() & 0xFF), static_cast<uint8_t>(text.size() >> 8),
               107, 9, 11, 2, 42, 51};
    b.insert(b.end(), text.begin(), text.end());
    uint16_t c = Td0Codec::crc16(b.data(), b.size());
    if (crcFalsch) c ^= 0x0101;
    Bytes out = {static_cast<uint8_t>(c & 0xFF), static_cast<uint8_t>(c >> 8)};
    return cat(out, b);
}

struct S {
    uint8_t c = 0, h = 0, r = 1, n = 0, fl = 0;
    bool block = true;
    uint8_t meth = 0;
    Bytes nutz;       ///< Nutzdaten des Blocks (je nach Methode)
    Bytes daten;      ///< entpackte Daten (Soll)
    int crc8 = -1;    ///< -1: richtig berechnen
};

S roh(uint8_t r, uint8_t n, Bytes d, uint8_t fl = 0) {
    S s; s.r = r; s.n = n; s.fl = fl; s.meth = 0; s.nutz = d; s.daten = std::move(d);
    return s;
}

S wiederholt(uint8_t r, uint8_t n, uint16_t zaehler, uint8_t m0, uint8_t m1, uint8_t fl = 0) {
    S s; s.r = r; s.n = n; s.fl = fl; s.meth = 1;
    s.nutz = {static_cast<uint8_t>(zaehler & 0xFF), static_cast<uint8_t>(zaehler >> 8), m0, m1};
    for (unsigned i = 0; i < zaehler; ++i) { s.daten.push_back(m0); s.daten.push_back(m1); }
    return s;
}

S ohneDaten(uint8_t r, uint8_t n, uint8_t fl) {
    S s; s.r = r; s.n = n; s.fl = fl; s.block = false;
    return s;
}

Bytes sektor(const S& s) {
    Bytes b = {s.c, s.h, s.r, s.n, s.fl, 0};
    if (!s.block) return b;
    const uint8_t crc = s.crc8 >= 0 ? static_cast<uint8_t>(s.crc8)
                                    : static_cast<uint8_t>(Td0Codec::crc16(s.daten.data(), s.daten.size()) & 0xFF);
    b[5] = crc;
    const size_t dlen = s.nutz.size() + 1;
    b.push_back(dlen & 0xFF);
    b.push_back(dlen >> 8);
    b.push_back(s.meth);
    return cat(b, s.nutz);
}

Bytes spur(uint8_t cyl, uint8_t kopfByte, const std::vector<S>& secs, int crcFalsch = 0) {
    Bytes k = {static_cast<uint8_t>(secs.size()), cyl, kopfByte};
    Bytes b = k;
    b.push_back(static_cast<uint8_t>((Td0Codec::crc16(k.data(), 3) & 0xFF) ^ crcFalsch));
    for (const S& s : secs) b = cat(b, sektor(s));
    return b;
}

Bytes ende() { return {0xFF, 0x00, 0x00, 0x00}; }

Bytes datei(const Bytes& k, const std::vector<Bytes>& teile) {
    Bytes d = k;
    for (const Bytes& t : teile) d = cat(d, t);
    return cat(d, ende());
}

bool laedt(const Bytes& d, std::string* err = nullptr, Td0Codec::SourceInfo* si = nullptr,
           DiskMedium* out = nullptr) {
    DiskMedium m;
    std::string e;
    const bool ok = Td0Codec::loadBuffer(d, out ? *out : m, si, e);
    if (err) *err = e;
    return ok;
}

std::vector<LogicalSector> sektoren(const DiskMedium& m, uint8_t c, uint8_t h) {
    return TrackCodec::parseTrack(m.peek(c, h));
}

bool hatWarnung(const Td0Codec::SourceInfo& si, const std::string& teil) {
    for (const auto& w : si.warnings)
        if (w.find(teil) != std::string::npos) return true;
    return false;
}

// ─── der td-Vektor (tests/python/td0_pack.py) ────────────────────────────────

const Bytes kVektorPlain = {
    0x54,0x44,0x00,0x4B,0x15,0x00,0x02,0x80,0x00,0x02,0xC8,0x35,0x11,0xCD,0x19,0x00,
    0x6B,0x09,0x0B,0x02,0x2A,0x33,0x54,0x65,0x73,0x74,0x76,0x65,0x6B,0x74,0x6F,0x72,
    0x00,0x7A,0x77,0x65,0x69,0x74,0x65,0x20,0x5A,0x65,0x69,0x6C,0x65,0x00,0x00,0x03,
    0x00,0x00,0xBC,0x00,0x00,0x01,0x00,0x00,0x5B,0x81,0x00,0x00,0x54,0x65,0x6C,0x65,
    0x44,0x69,0x73,0x6B,0x20,0x54,0x65,0x73,0x74,0x76,0x65,0x6B,0x74,0x6F,0x72,0x20,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x20,0x54,0x65,0x6C,0x65,0x44,
    0x69,0x73,0x6B,0x20,0x54,0x65,0x73,0x74,0x76,0x65,0x6B,0x74,0x6F,0x72,0x20,0x30,
    0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x20,0x54,0x65,0x6C,0x65,0x44,0x69,
    0x73,0x6B,0x20,0x54,0x65,0x73,0x74,0x76,0x65,0x6B,0x74,0x6F,0x72,0x20,0x30,0x31,
    0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x20,0x54,0x65,0x6C,0x65,0x44,0x69,0x73,
    0x6B,0x20,0x54,0x65,0x73,0x74,0x76,0x65,0x6B,0x74,0x6F,0x72,0x20,0x30,0x31,0x32,
    0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x20,0x54,0x65,0x6C,0x65,0x00,0x00,0x02,0x00,
    0x00,0xD1,0x05,0x00,0x01,0x40,0x00,0xE5,0xE5,0x00,0x00,0x03,0x00,0x02,0xA1,0x25,
    0x00,0x02,0x01,0x14,0x00,0x00,0x00,0x01,0x01,0x00,0x01,0x02,0x00,0x01,0x03,0x00,
    0x01,0x04,0x00,0x01,0x05,0x00,0x01,0x06,0x00,0x01,0x07,0x00,0x01,0x08,0x01,0x14,
    0xAB,0xCD,0x01,0x14,0xE5,0xE5,0x01,0x01,0x80,0x92,0x01,0x00,0x01,0x01,0x04,0x06,
    0x05,0x00,0x01,0x80,0x00,0x00,0x11,0xFF,0x00,0x00,0x00,
};

const Bytes kVektorTd = {
    0x74,0x64,0x00,0x4B,0x15,0x00,0x02,0x80,0x00,0x02,0xF0,0x52,0xCE,0xAC,0xE9,0x71,
    0x9F,0x7C,0xAE,0x5F,0x1D,0xB6,0xDF,0xF8,0x3E,0x3F,0xF0,0x00,0x2B,0xAC,0x1B,0x8F,
    0xDF,0xFB,0x08,0x18,0x0E,0xBF,0xD6,0xAD,0x9D,0x67,0x9A,0x9B,0x0F,0xC5,0x34,0xA9,
    0xE3,0xE7,0x9D,0x22,0x8A,0x71,0xB2,0x97,0x9C,0x36,0x52,0xBB,0x9E,0xA6,0x7E,0x84,
    0xDB,0x99,0x95,0xB6,0x71,0x73,0xFD,0xE6,0xF7,0x7D,0x79,0xC0,0xE0,0xF0,0xB8,0x7C,
    0x4E,0x2E,0x08,0x2B,0x9D,0x0F,0x63,0x43,0xD0,0x85,0xF4,0x21,0x0D,0xC8,0xAD,0x17,
    0x31,0xA7,0x2F,0x98,0x33,0x6D,0xD9,0x6E,0xC5,0x5E,0xB9,0xD0,0x2C,0x51,0xC2,0xED,
    0x58,0x61,0x59,0xA8,0x57,0x32,0x0A,0xE2,0xC1,0x0E,0x49,0x4F,0x93,0x54,0x65,0x10,
    0xE9,0x07,0xC4,0x4A,0x04,0x29,0xD8,0xC0,0xC1,0xE6,0xE2,0x16,0x36,0xB6,0x18,0x1B,
    0x4C,0x55,0xC5,0xBB,0x9A,0x33,0x79,0x00,0x00,
};

}  // namespace

// ─── CRC / Erkennung ─────────────────────────────────────────────────────────

TEST(Td0Codec, Crc16Pruefwert) {
    const std::string s = "123456789";
    EXPECT_EQ(Td0Codec::crc16(reinterpret_cast<const uint8_t*>(s.data()), s.size()), 0x0FB3);
}

TEST(Td0Codec, ErkennungBrauchtKennungUndKopfCrc) {
    EXPECT_TRUE(Td0Codec::isTd0(kopf().data(), 12));
    EXPECT_TRUE(Td0Codec::isTd0(kopf("td").data(), 12));
    Bytes schlecht = kopf();
    schlecht[11] ^= 1;
    EXPECT_FALSE(Td0Codec::isTd0(schlecht.data(), 12));
    EXPECT_FALSE(Td0Codec::isTd0(kopf().data(), 11));
    Bytes fremd = kopf();
    fremd[0] = 'X';
    EXPECT_FALSE(Td0Codec::isTd0(fremd.data(), 12));
    // Beispielkopf aus 38a (D02V032, Kennung td)
    const uint8_t d02[12] = {0x74, 0x64, 0x00, 0x4B, 0x15, 0x00, 0x02, 0x80, 0x00, 0x02, 0xF0, 0x52};
    EXPECT_TRUE(Td0Codec::isTd0(d02, 12));
}

// ─── Methoden ────────────────────────────────────────────────────────────────

TEST(Td0Codec, Methode0Und1UndDerInhaltStimmt) {
    const S a = roh(1, 0, steigend(128));
    const S b = wiederholt(2, 0, 64, 0xE5, 0xE5);
    const Bytes d = datei(kopf(), {spur(0, 0, {a, b})});
    DiskMedium m;
    Td0Codec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(laedt(d, &err, &si, &m)) << err;
    const auto s = sektoren(m, 0, 0);
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].id, 1);
    EXPECT_EQ(s[0].data, a.daten);
    EXPECT_EQ(s[1].id, 2);
    EXPECT_EQ(s[1].data, fuell(128, 0xE5));
    EXPECT_TRUE(s[0].data_crc_ok);
    EXPECT_EQ(si.sectors, 2u);
    EXPECT_EQ(si.crc8_mismatch, 0u);
    EXPECT_EQ(m.peek(0, 0).encoding, Encoding::MFM);
}

TEST(Td0Codec, Methode1MitMehrerenPaaren) {
    S s; s.r = 1; s.n = 0; s.meth = 1;
    s.nutz = {0x20, 0x00, 0xAA, 0xBB, 0x20, 0x00, 0xCC, 0xDD};
    for (int i = 0; i < 32; ++i) { s.daten.push_back(0xAA); s.daten.push_back(0xBB); }
    for (int i = 0; i < 32; ++i) { s.daten.push_back(0xCC); s.daten.push_back(0xDD); }
    DiskMedium m;
    std::string err;
    ASSERT_TRUE(laedt(datei(kopf(), {spur(0, 0, {s})}), &err, nullptr, &m)) << err;
    EXPECT_EQ(sektoren(m, 0, 0)[0].data, s.daten);
}

TEST(Td0Codec, Methode2LiteralWiederholungUndAbschluss) {
    // 38a "Methode 2 (gemischt)": Literal 5, 37 x 4040, Literal 1, 24 x 0000
    S s; s.r = 5; s.n = 0; s.meth = 2;
    s.nutz = {0x00, 0x05, 0xC5, 0xD9, 0xD4, 0xC1, 0xD7, 0x01, 0x25, 0x40, 0x40,
              0x00, 0x01, 0x40, 0x01, 0x18, 0x00, 0x00};
    s.daten = {0xC5, 0xD9, 0xD4, 0xC1, 0xD7};
    for (int i = 0; i < 37; ++i) { s.daten.push_back(0x40); s.daten.push_back(0x40); }
    s.daten.push_back(0x40);
    for (int i = 0; i < 24; ++i) { s.daten.push_back(0); s.daten.push_back(0); }
    ASSERT_EQ(s.daten.size(), 128u);
    // 38a "nur Wiederholung" mit Abschluss `00 00` (Literal der Laenge 0), 256 B
    S t; t.r = 6; t.n = 1; t.meth = 2;
    t.nutz = {0x01, 0x80, 0xE5, 0xE5, 0x00, 0x00};
    t.daten = fuell(256, 0xE5);
    DiskMedium m;
    Td0Codec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(laedt(datei(kopf(), {spur(0, 0, {s, t})}), &err, &si, &m)) << err;
    const auto v = sektoren(m, 0, 0);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0].data, s.daten);
    EXPECT_EQ(v[1].data, t.daten);
    EXPECT_FALSE(hatWarnung(si, "Musterlaenge"));
}

TEST(Td0Codec, Methode2LangesMusterGiltAls2MalCUndWirdGemeldet) {
    S s; s.r = 1; s.n = 0; s.meth = 2;
    s.nutz = {0x02, 0x20, 0x01, 0x02, 0x03, 0x04};   // c = 2: Muster 4 Byte, 32 mal = 128
    for (int i = 0; i < 32; ++i) for (uint8_t b : {1, 2, 3, 4}) s.daten.push_back(b);
    DiskMedium m;
    Td0Codec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(laedt(datei(kopf(), {spur(0, 0, {s})}), &err, &si, &m)) << err;
    EXPECT_EQ(sektoren(m, 0, 0)[0].data, s.daten);
    EXPECT_TRUE(hatWarnung(si, "Musterlaenge"));
}

TEST(Td0Codec, FalscheMethodenUndLaengenWerdenMitGrundAbgewiesen) {
    struct Fall { const char* wort; S s; };
    std::vector<Fall> faelle;
    {   S s = roh(1, 0, steigend(128)); s.meth = 3; faelle.push_back({"ompressionsmethode", s}); }
    {   S s = roh(1, 0, steigend(100)); faelle.push_back({"entpackte Laenge", s}); }          // Methode 0, zu kurz
    {   S s = wiederholt(1, 0, 70, 1, 2); faelle.push_back({"mehr Daten", s}); }              // 140 > 128
    {   S s = wiederholt(1, 0, 32, 1, 2); faelle.push_back({"entpackte Laenge", s}); }        // 64 < 128
    {   S s; s.r = 1; s.meth = 2; s.nutz = {0x00, 0x20, 0x01}; s.daten = fuell(128, 0);
        faelle.push_back({"abgeschnitten", s}); }                                              // Literal 32, nur 1 da
    {   S s; s.r = 1; s.meth = 2; s.nutz = {0x01}; s.daten = fuell(128, 0);
        faelle.push_back({"abgeschnitten", s}); }
    for (const Fall& f : faelle) {
        std::string err;
        EXPECT_FALSE(laedt(datei(kopf(), {spur(0, 0, {f.s})}), &err)) << f.wort;
        EXPECT_NE(err.find(f.wort), std::string::npos) << f.wort << ": " << err;
    }
}

// ─── Kopf, Kommentar, FM ─────────────────────────────────────────────────────

TEST(Td0Codec, KopfFelderUndKommentarWerdenGemeldet) {
    const Bytes d = datei(kopf("TD", 2, 5, 0x80 | 1, 0, 1),
                          {kommentar(std::string("Zeile eins\0Zeile zwei\0\0", 23)),
                           spur(0, 0, {roh(1, 0, steigend(128))})});
    Td0Codec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(laedt(d, &err, &si)) << err;
    EXPECT_FALSE(si.advanced);
    EXPECT_EQ(si.version, 0x15);
    EXPECT_EQ(si.rate_kbps, 500);
    EXPECT_EQ(si.drive_type, 5);
    EXPECT_EQ(si.stepping, 1);
    EXPECT_TRUE(si.has_comment);
    EXPECT_EQ(si.sides, 1);
    EXPECT_EQ(si.comment, "Zeile eins\nZeile zwei");
    EXPECT_EQ(si.timestamp, "11.10.2007 02:42:51");   // Monatsbyte ist 0-basiert
    EXPECT_EQ(si.raw_header.size(), 12u);
    EXPECT_TRUE(si.warnings.empty()) << si.warnings.front();
    EXPECT_EQ(Td0Codec::infoField(si, "rate"), "500");
    EXPECT_EQ(Td0Codec::infoField(si, "version"), "1.5");
}

TEST(Td0Codec, KommentarCrcFehlerIstNurEineWarnung) {
    const Bytes d = datei(kopf("TD", 0, 2, 0x80), {kommentar("abc", true),
                                                    spur(0, 0, {roh(1, 0, steigend(128))})});
    Td0Codec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(laedt(d, &err, &si)) << err;
    EXPECT_TRUE(hatWarnung(si, "Kommentar-CRC"));
    EXPECT_EQ(si.comment, "abc");
}

TEST(Td0Codec, FmAusKopfOderSpurOderBeidem) {
    const std::vector<S> sec = {roh(1, 0, steigend(128))};
    {   // nur Spurbit
        DiskMedium m;
        ASSERT_TRUE(laedt(datei(kopf(), {spur(0, 0, sec), spur(1, 0x80, sec)}), nullptr, nullptr, &m));
        EXPECT_EQ(m.peek(0, 0).encoding, Encoding::MFM);
        EXPECT_EQ(m.peek(1, 0).encoding, Encoding::FM);
    }
    {   // globales Bit (Datenrate 0x82 = FM, 500)
        Td0Codec::SourceInfo si;
        DiskMedium m;
        ASSERT_TRUE(laedt(datei(kopf("TD", 0x82), {spur(0, 0, sec), spur(1, 0x81, sec)}), nullptr, &si, &m));
        EXPECT_TRUE(si.fm_global);
        EXPECT_EQ(si.rate_kbps, 500);
        EXPECT_EQ(m.peek(0, 0).encoding, Encoding::FM);
        EXPECT_EQ(m.peek(1, 1).encoding, Encoding::FM);   // Kopf 129 -> Seite 1 UND FM
        EXPECT_EQ(sektoren(m, 1, 1).size(), 1u);
    }
}

TEST(Td0Codec, UnbekannteDatenrateUndKopfWerdenAbgewiesen) {
    std::string err;
    EXPECT_FALSE(laedt(datei(kopf("TD", 3), {spur(0, 0, {roh(1, 0, steigend(128))})}), &err));
    EXPECT_NE(err.find("Datenrate"), std::string::npos) << err;
    EXPECT_FALSE(laedt(datei(kopf(), {spur(0, 2, {roh(1, 0, steigend(128))})}), &err));
    EXPECT_NE(err.find("Kopf"), std::string::npos) << err;
}

// ─── Flags und Sektorkoepfe ──────────────────────────────────────────────────

TEST(Td0Codec, FlagsDatenfehlerGeloeschtOhneDatenfeldPseudosektor) {
    const Bytes d = datei(kopf(), {spur(0, 0, {
        roh(1, 0, steigend(128, 1), 0x02),                 // Datenfehler, Daten vorhanden
        roh(2, 0, steigend(128, 2), 0x04),                 // geloeschte Marke
        roh(3, 0, steigend(128, 3), 0x06),                 // beides
        ohneDaten(4, 0, 0x20),                             // nur ID
        ohneDaten(5, 0, 0x10),                             // DOS-Allocation: uebersprungen
        roh(105, 0, steigend(128, 5), 0x40),               // Pseudosektor: nicht ins Medium
        roh(6, 0, steigend(128, 6)),
    })});
    DiskMedium m;
    Td0Codec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(laedt(d, &err, &si, &m)) << err;
    const auto s = sektoren(m, 0, 0);
    ASSERT_EQ(s.size(), 6u);
    EXPECT_FALSE(s[0].data_crc_ok);
    EXPECT_FALSE(s[0].deleted);
    EXPECT_EQ(s[0].data, steigend(128, 1));
    EXPECT_TRUE(s[1].data_crc_ok);
    EXPECT_TRUE(s[1].deleted);
    EXPECT_TRUE(s[2].deleted);
    EXPECT_FALSE(s[2].data_crc_ok);
    EXPECT_EQ(s[3].data_pos, SIZE_MAX);   // kein Datenfeld
    EXPECT_EQ(s[3].id, 4);
    EXPECT_EQ(s[4].data_pos, SIZE_MAX);
    EXPECT_EQ(s[5].id, 6);
    EXPECT_TRUE(s[5].data_crc_ok);
    EXPECT_EQ(si.sectors, 6u);
    EXPECT_EQ(si.no_data_sectors, 2u);
    EXPECT_EQ(si.data_error_sectors, 2u);
    EXPECT_EQ(si.deleted_sectors, 2u);
    EXPECT_EQ(si.pseudo_sectors, 1u);
    EXPECT_TRUE(hatWarnung(si, "Pseudosektor"));
}

TEST(Td0Codec, IdAusDemSektorkopfDoppelteNummernUndRAb100) {
    S a = roh(4, 0, steigend(128, 1)); a.c = 89; a.h = 1;     // ID weicht vom Spurkopf ab
    S b = roh(22, 0, steigend(128, 2));
    S c = roh(22, 0, steigend(128, 3));                       // doppelt, ohne Flag 0x01
    S e = roh(120, 0, steigend(128, 4));                      // R >= 100 ohne Flag 0x40: echter Sektor
    const Bytes d = datei(kopf(), {spur(5, 0, {a, b, c, e})});
    DiskMedium m;
    Td0Codec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(laedt(d, &err, &si, &m)) << err;
    const auto s = sektoren(m, 5, 0);
    ASSERT_EQ(s.size(), 4u);
    EXPECT_EQ(s[0].cyl, 89);
    EXPECT_EQ(s[0].head, 1);
    EXPECT_EQ(s[1].id, 22);
    EXPECT_EQ(s[2].id, 22);
    EXPECT_EQ(s[1].data, steigend(128, 2));
    EXPECT_EQ(s[2].data, steigend(128, 3));
    EXPECT_EQ(s[3].id, 120);
    EXPECT_EQ(si.duplicate_ids, 1u);
    EXPECT_EQ(si.pseudo_sectors, 0u);
}

TEST(Td0Codec, SektorCrc8IstNurEinHinweis) {
    S a = roh(1, 0, steigend(128, 1));
    a.crc8 = (Td0Codec::crc16(a.daten.data(), 128) & 0xFF) ^ 0x55;
    Td0Codec::SourceInfo si;
    DiskMedium m;
    std::string err;
    ASSERT_TRUE(laedt(datei(kopf(), {spur(0, 0, {a})}), &err, &si, &m)) << err;
    EXPECT_EQ(si.crc8_mismatch, 1u);
    EXPECT_TRUE(hatWarnung(si, "Sektor-CRC8"));
    EXPECT_EQ(sektoren(m, 0, 0)[0].data, a.daten);
}

TEST(Td0Codec, SektorgroessenBis8192) {
    for (uint8_t n = 0; n <= 6; ++n) {
        SCOPED_TRACE(int(n));
        const Bytes dat = steigend(128u << n, n);
        DiskMedium m;
        std::string err;
        ASSERT_TRUE(laedt(datei(kopf(), {spur(0, 0, {roh(1, n, dat)})}), &err, nullptr, &m)) << err;
        const auto s = sektoren(m, 0, 0);
        ASSERT_EQ(s.size(), 1u);
        EXPECT_EQ(s[0].size, 128u << n);
        EXPECT_EQ(s[0].data, dat);
        EXPECT_TRUE(s[0].data_crc_ok);
        EXPECT_TRUE(s[0].id_crc_ok);
    }
    // 7 mit Datenblock ist ein Fehler, ohne Datenblock wird der Sektor ausgelassen
    std::string err;
    S s7 = roh(1, 7, steigend(128));
    EXPECT_FALSE(laedt(datei(kopf(), {spur(0, 0, {s7})}), &err));
    EXPECT_NE(err.find("Sektorgroessen-Code"), std::string::npos) << err;
    Td0Codec::SourceInfo si;
    ASSERT_TRUE(laedt(datei(kopf(), {spur(0, 0, {roh(1, 0, steigend(128)), ohneDaten(2, 9, 0x20)})}),
                      &err, &si)) << err;
    EXPECT_EQ(si.sectors, 1u);
    EXPECT_TRUE(hatWarnung(si, "Groessen-Code"));
}

// ─── Spuren ──────────────────────────────────────────────────────────────────

TEST(Td0Codec, SpurenOhneSektorenUndLeereDiskette) {
    DiskMedium m;
    Td0Codec::SourceInfo si;
    std::string err;
    // die 20-Byte-Datei aus 38a: Kopf, eine Spur ohne Sektoren (Zyl 0, Seite 1), Endesatz
    const Bytes klein = {0x54, 0x44, 0x00, 0x1F, 0x15, 0x02, 0x05, 0x00, 0x00, 0x02, 0x4D, 0xCB,
                         0x00, 0x00, 0x01, 0x97, 0xFF, 0x00, 0x01, 0x97};
    ASSERT_EQ(klein.size(), 20u);
    ASSERT_TRUE(laedt(klein, &err, &si, &m)) << err;
    EXPECT_EQ(si.tracks, 1u);
    EXPECT_EQ(si.empty_tracks, 1u);
    EXPECT_EQ(si.sectors, 0u);
    EXPECT_EQ(si.rate_kbps, 500);
    EXPECT_TRUE(hatWarnung(si, "keine Sektoren"));
    EXPECT_TRUE(sektoren(m, 0, 1).empty());

    // leere Spur zwischen belegten
    const Bytes d = datei(kopf(), {spur(0, 0, {roh(1, 0, steigend(128))}), spur(1, 0, {}),
                                   spur(2, 0, {roh(1, 0, steigend(128))})});
    ASSERT_TRUE(laedt(d, &err, &si, &m)) << err;
    EXPECT_EQ(si.empty_tracks, 1u);
    EXPECT_EQ(m.numCylinders(), 3);
    EXPECT_TRUE(sektoren(m, 1, 0).empty());
    EXPECT_EQ(sektoren(m, 2, 0).size(), 1u);
}

TEST(Td0Codec, DoppelterSpursatzDerLetzteGilt) {
    const Bytes d = datei(kopf(), {spur(0, 0, {roh(1, 0, steigend(128, 1))}),
                                   spur(0, 0, {roh(1, 0, steigend(128, 2))})});
    Td0Codec::SourceInfo si;
    DiskMedium m;
    std::string err;
    ASSERT_TRUE(laedt(d, &err, &si, &m)) << err;
    EXPECT_EQ(si.tracks, 1u);
    EXPECT_TRUE(hatWarnung(si, "mehrfach"));
    EXPECT_EQ(sektoren(m, 0, 0)[0].data, steigend(128, 2));
}

TEST(Td0Codec, InterleaveBleibtErhalten) {
    std::vector<S> v;
    for (uint8_t r : {1, 6, 2, 7, 3, 8, 4, 9, 5}) v.push_back(roh(r, 0, steigend(128, r)));
    DiskMedium m;
    std::string err;
    ASSERT_TRUE(laedt(datei(kopf(), {spur(0, 0, v)}), &err, nullptr, &m)) << err;
    const auto s = sektoren(m, 0, 0);
    ASSERT_EQ(s.size(), 9u);
    const uint8_t soll[] = {1, 6, 2, 7, 3, 8, 4, 9, 5};
    for (size_t i = 0; i < 9; ++i) EXPECT_EQ(s[i].id, soll[i]);
}

TEST(Td0Codec, EndemarkeOhneAuswertungDerRestbytes) {
    Bytes d = datei(kopf(), {spur(0, 0, {roh(1, 0, steigend(128))})});
    d[d.size() - 2] = 0x77;   // Inhalt hinter dem 0xFF ist beliebig
    d[d.size() - 1] = 0x99;
    EXPECT_TRUE(laedt(d));
}

// ─── kaputte und abgeschnittene Dateien ──────────────────────────────────────

TEST(Td0Codec, KopfCrcUndSpurCrcSindHarteFehler) {
    Bytes d = datei(kopf(), {spur(0, 0, {roh(1, 0, steigend(128))})});
    std::string err;
    Bytes k = d; k[5] ^= 0x01;   // Datenrate geaendert, CRC nicht
    EXPECT_FALSE(laedt(k, &err));
    EXPECT_NE(err.find("Kopf-CRC"), std::string::npos) << err;

    EXPECT_FALSE(laedt(datei(kopf(), {spur(0, 0, {roh(1, 0, steigend(128))}, 1)}), &err));
    EXPECT_NE(err.find("Spur-CRC"), std::string::npos) << err;
    EXPECT_NE(err.find("Spur 0/0"), std::string::npos) << err;
}

TEST(Td0Codec, LeereUndZuKurzeDateienUndKeineSpuren) {
    std::string err;
    EXPECT_FALSE(laedt({}, &err));
    EXPECT_NE(err.find("leer"), std::string::npos) << err;
    const Bytes k12 = kopf();
    EXPECT_FALSE(laedt(Bytes(k12.begin(), k12.begin() + 11), &err));
    EXPECT_NE(err.find("zu kurz"), std::string::npos) << err;
    EXPECT_FALSE(laedt(cat(kopf(), ende()), &err));
    EXPECT_NE(err.find("keine Spuren"), std::string::npos) << err;
    EXPECT_FALSE(laedt(Bytes(100, 0x55), &err));
    EXPECT_NE(err.find("Kennung"), std::string::npos) << err;
}

TEST(Td0Codec, JedesAbgeschnitteneStueckScheitertMitGrund) {
    const Bytes d = datei(kopf("TD", 0, 2, 0x80),
                          {kommentar("Kommentar"), spur(0, 0, {roh(1, 0, steigend(128)), wiederholt(2, 0, 64, 0, 0)}),
                           spur(1, 0x80, {roh(1, 1, steigend(256))})});
    ASSERT_TRUE(laedt(d));
    for (size_t n = 0; n < d.size() - 3; ++n) {   // ab dem 0xFF gilt die Datei als vollstaendig
        const Bytes k(d.begin(), d.begin() + static_cast<long>(n));
        std::string err;
        EXPECT_FALSE(laedt(k, &err)) << "Laenge " << n;
        EXPECT_FALSE(err.empty()) << "Laenge " << n;
    }
    // nur der Endesatz fehlt
    std::string err;
    const Bytes ohneEnde(d.begin(), d.end() - 4);
    EXPECT_FALSE(laedt(ohneEnde, &err));
    EXPECT_NE(err.find("Endemarke"), std::string::npos) << err;
}

// ─── Advanced Compression ────────────────────────────────────────────────────

TEST(Td0Codec, EntpackenDesAnfangsDerEchtenD02V032) {
    // 38a Teil 4: die ersten gepackten Bytes -> die ersten 14 entpackten Bytes (Kommentarkopf + Text)
    const Bytes gepackt = {0x77, 0x6B, 0x3A, 0xC6, 0x7D, 0xF2, 0xB9, 0x7C, 0x76, 0xDB, 0x7F, 0xD1,
                           0xE7, 0xEB, 0x37, 0xBC};
    const Bytes soll = {0xEB, 0xDF, 0xAE, 0x00, 0x6B, 0x09, 0x0B, 0x02, 0x2A, 0x33, 0x45, 0x43, 0x20, 0x31};
    Bytes out;
    std::string err;
    ASSERT_TRUE(Td0Codec::unpack(gepackt.data(), gepackt.size(), out, err)) << err;
    ASSERT_GE(out.size(), soll.size());
    EXPECT_EQ(Bytes(out.begin(), out.begin() + static_cast<long>(soll.size())), soll);
}

TEST(Td0Codec, TdVektorGleichTdUndTdEntpacktFehlerfrei) {
    EXPECT_EQ(kVektorPlain[0], 'T');
    EXPECT_EQ(kVektorTd[0], 't');
    DiskMedium a, b;
    Td0Codec::SourceInfo sa, sb;
    std::string err;
    ASSERT_TRUE(laedt(kVektorPlain, &err, &sa, &a)) << err;
    ASSERT_TRUE(laedt(kVektorTd, &err, &sb, &b)) << err;
    EXPECT_FALSE(sa.advanced);
    EXPECT_TRUE(sb.advanced);
    EXPECT_EQ(sb.comment, "Testvektor\nzweite Zeile");
    EXPECT_EQ(sa.comment, sb.comment);
    EXPECT_EQ(sb.sectors, 4u);
    EXPECT_TRUE(sb.warnings.empty()) << sb.warnings.front();
    ASSERT_EQ(a.numCylinders(), b.numCylinders());
    for (uint8_t c = 0; c < a.numCylinders(); ++c) {
        EXPECT_EQ(a.peek(c, 0).bytes, b.peek(c, 0).bytes) << int(c);
        EXPECT_EQ(a.peek(c, 0).encoding, b.peek(c, 0).encoding) << int(c);
    }
    EXPECT_EQ(a.peek(1, 0).encoding, Encoding::FM);
    // Inhalt: Methode 0 / 1 / 2, Datenfehler, geloescht
    const auto s0 = sektoren(b, 0, 0);
    ASSERT_EQ(s0.size(), 3u);
    EXPECT_EQ(s0[0].data.size(), 128u);
    EXPECT_EQ(s0[1].data, fuell(128, 0xE5));
    EXPECT_FALSE(s0[2].data_crc_ok);
    EXPECT_EQ(s0[2].data[0], 0x00);
    EXPECT_EQ(s0[2].data[40], 0x01);
    const auto s1 = sektoren(b, 1, 0);
    ASSERT_EQ(s1.size(), 1u);
    EXPECT_TRUE(s1[0].deleted);
    EXPECT_EQ(s1[0].size, 256);
    EXPECT_EQ(s1[0].data[0], 0x00);
    EXPECT_EQ(s1[0].data[1], 0x11);
}

TEST(Td0Codec, TdAbgeschnittenOderVerfaelschtScheitertSauber) {
    std::string err;
    for (size_t n = 12; n + 8 < kVektorTd.size(); n += 7) {
        const Bytes k(kVektorTd.begin(), kVektorTd.begin() + static_cast<long>(n));
        EXPECT_FALSE(laedt(k, &err)) << "Laenge " << n;
        EXPECT_FALSE(err.empty());
    }
    // ein verfaelschtes Byte mitten im Strom: irgendein Fehler (CRC, Laenge, Struktur), nie ein Absturz
    for (size_t i = 12; i < kVektorTd.size(); ++i) {
        Bytes k = kVektorTd;
        k[i] ^= 0x10;
        DiskMedium m;
        (void)Td0Codec::loadBuffer(k, m, nullptr, err);
    }
}

// ─── Verdrahtung ─────────────────────────────────────────────────────────────

TEST(Td0Codec, ImageCodecErkenntUndLaedtUndSchreibenIstNochNichtDa) {
    const std::string pfad = k1520test::tempPath("k1520_test_td0_codec.td0");
    {
        const Bytes d = datei(kopf(), {spur(0, 0, {roh(1, 0, steigend(128))})});
        std::ofstream(pfad, std::ios::binary).write(reinterpret_cast<const char*>(d.data()),
                                                    static_cast<std::streamsize>(d.size()));
    }
    EXPECT_EQ(ImageCodec::detect(pfad), ContainerType::Td0);
    DiskMedium m;
    std::string err;
    ASSERT_TRUE(ImageCodec::load(pfad, ContainerType::Td0, nullptr, m, err)) << err;
    EXPECT_EQ(sektoren(m, 0, 0).size(), 1u);
    EXPECT_FALSE(ImageCodec::save(pfad + ".neu", ContainerType::Td0, nullptr, m, err));
    EXPECT_FALSE(fs::exists(pfad + ".neu"));
    fs::remove(pfad);
    EXPECT_FALSE(ImageCodec::load("gibt_es_nicht.td0", ContainerType::Td0, nullptr, m, err));
}

// ─── Echte Abbilder (nur mit K1520_TD0_DIR) ──────────────────────────────────

namespace {

std::vector<fs::path> td0Dateien() {
    std::vector<fs::path> r;
    const char* e = std::getenv("K1520_TD0_DIR");
    if (!e || !*e) return r;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(e, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string x = it->path().extension().string();
        std::transform(x.begin(), x.end(), x.begin(), [](unsigned char c) { return std::tolower(c); });
        if (x == ".td0") r.push_back(it->path());
    }
    std::sort(r.begin(), r.end());
    return r;
}

Bytes leseDatei(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(f), {});
}

}  // namespace

/// @test Jede echte Datei laedt (Ausnahme: die 0-Byte-Datei wird mit Grund abgelehnt), alle
///       Kopf-/Spur-/Sektor-CRCs stimmen, und das Medium traegt genau die entpackten Sektordaten.
TEST(Td0Echtdaten, AlleDateienLadenMitPruefsummen) {
    const auto liste = td0Dateien();
    if (liste.empty()) GTEST_SKIP() << "K1520_TD0_DIR nicht gesetzt oder ohne *.td0";
    size_t geladen = 0, leer = 0, sektorenGesamt = 0, td = 0;
    for (const auto& p : liste) {
        SCOPED_TRACE(p.string());
        const Bytes d = leseDatei(p);
        std::string err;
        if (d.empty()) {
            EXPECT_FALSE(laedt(d, &err));
            EXPECT_NE(err.find("leer"), std::string::npos) << err;
            ++leer;
            continue;
        }
        std::vector<SectorTrackIo::TrackRecord> tracks;
        Td0Codec::SourceInfo si;
        ASSERT_TRUE(Td0Codec::decodeBuffer(d, tracks, &si, err)) << err;
        EXPECT_EQ(si.crc8_mismatch, 0u);
        EXPECT_LE(si.trailing_bytes, 4u + 4u);   // Endesatz (4) + Strompadding bei td
        DiskMedium m;
        ASSERT_TRUE(Td0Codec::loadBuffer(d, m, nullptr, err)) << err;
        for (const auto& t : tracks) {
            const auto s = sektoren(m, t.cyl, t.head);
            ASSERT_EQ(s.size(), t.sectors.size()) << "Spur " << int(t.cyl) << "/" << int(t.head);
            for (size_t i = 0; i < s.size(); ++i) {
                const auto& r = t.sectors[i];
                EXPECT_EQ(s[i].cyl, r.c);
                EXPECT_EQ(s[i].head, r.h);
                EXPECT_EQ(s[i].id, r.r);
                EXPECT_EQ(s[i].size, r.size());
                if (r.no_data) { EXPECT_EQ(s[i].data_pos, SIZE_MAX); continue; }
                EXPECT_EQ(s[i].data, r.data);
                EXPECT_EQ(s[i].deleted, r.deleted);
                EXPECT_EQ(!s[i].data_crc_ok, r.data_error);
                EXPECT_TRUE(s[i].id_crc_ok);
            }
        }
        sektorenGesamt += si.sectors;
        td += si.advanced ? 1 : 0;
        ++geladen;
    }
    std::printf("[ INFO ] Td0Echtdaten: %zu geladen (davon %zu td), %zu leer abgelehnt, %zu Sektoren\n",
                geladen, td, leer, sektorenGesamt);
}

/// @test Schreibt je Datei die Inhaltsangabe, wie sie aus dem MEDIUM kommt, fuer den
///       Vergleich mit Greaseweazle (tests/python/test_td0_gegenprobe.py, td0_referenz.py).
///       Format je Datei `NNN.json`: Pfad, Sektorliste (Spur, ID, Groesse, Kennzeichen, FNV-1a-64
///       der Daten), Zaehler.
TEST(Td0Gegenprobe, SchreibtSektorlisten) {
    const char* ziel = std::getenv("K1520_TD0_AUSGABE");
    if (!ziel || !*ziel) GTEST_SKIP() << "K1520_TD0_AUSGABE nicht gesetzt (nur fuer tests/python/test_td0_gegenprobe.py)";
    const auto liste = td0Dateien();
    if (liste.empty()) GTEST_SKIP() << "K1520_TD0_DIR nicht gesetzt oder ohne *.td0";
    fs::create_directories(ziel);
    size_t nr = 0;
    for (const auto& p : liste) {
        const Bytes d = leseDatei(p);
        DiskMedium m;
        Td0Codec::SourceInfo si;
        std::string err;
        std::ostringstream o;
        o << "{\"datei\":\"" << p.string() << "\",";
        if (!Td0Codec::loadBuffer(d, m, &si, err)) {
            o << "\"fehler\":\"";
            for (char c : err) o << (c == '"' || c == '\\' ? '_' : c);
            o << "\"}";
        } else {
            o << "\"pseudo\":" << si.pseudo_sectors << ",\"sektoren\":[";
            bool erst = true;
            for (uint8_t c = 0; c < m.numCylinders(); ++c)
                for (uint8_t h = 0; h < m.numHeads(); ++h)
                    for (const auto& s : sektoren(m, c, h)) {
                        uint64_t fnv = 1469598103934665603ull;
                        for (uint8_t b : s.data) { fnv ^= b; fnv *= 1099511628211ull; }
                        char buf[200];
                        std::snprintf(buf, sizeof(buf), "%s[%d,%d,%d,%d,%d,%d,%d,%d,\"%016llx\"]",
                                      erst ? "" : ",", c, h, s.cyl, s.head, s.id, s.size,
                                      s.data_pos == SIZE_MAX ? 1 : 0, s.deleted ? 1 : 0,
                                      static_cast<unsigned long long>(fnv));
                        o << buf;
                        erst = false;
                    }
            o << "]}";
        }
        char name[32];
        std::snprintf(name, sizeof(name), "%03zu.json", nr++);
        std::ofstream(fs::path(ziel) / name) << o.str();
    }
}
