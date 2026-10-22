/**
 * @file test_imd_codec.cpp
 * @brief GoogleTests fuer ImdCodec (ImageDisk-Abbild, Lesen und Schreiben, Entwurf 38 AP I1).
 *
 * Schwerpunkte:
 *   - die drei handgerechneten Testvektoren aus doc/design/38a (A: MFM mit Kompression,
 *     B: FM mit beiden Maps, geloescht/Datenfehler/ohne Datenfeld, C: leere Spur)
 *   - Rundlauf Medium → Datei → Medium (Sektordaten, Spurbytes + Marken, Geometrie),
 *     FM und MFM, Interleave, geloeschte Marken, Datenfehler, fehlende Datenfelder,
 *     abweichende Zylinder-/Kopf-IDs, 300 kbit/s ueber die Vorlage
 *   - alle neun Sektortypen beim Schreiben, Kompression "alle Bytes gleich"
 *   - Ablehnungen mit Begruendung (gemischte Groessen, UDOS-Nachspann, ID-CRC-Fehler,
 *     unformatiert) und kaputte/abgeschnittene Dateien
 *   - atomares Speichern, ImageCodec-Verdrahtung
 *   - `ImdGegenprobe.SchreibtBeispiele`: schreibt mit `K1520_IMD_AUSGABE=<Ordner>` Beispiele
 *     samt Inhaltsangabe (JSON) fuer den unabhaengigen Python-Leser (tests/python/imd_bau.py)
 *
 * @see core/peripherals/floppy_drive/imd_codec.h
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
#include <sstream>
#include <string>
#include <vector>

#include "core/peripherals/floppy_drive/image_codec.h"
#include "core/peripherals/floppy_drive/imd_codec.h"
#include "core/peripherals/floppy_drive/sector_track_io.h"
#include "core/peripherals/floppy_drive/track_codec.h"
#include "tests/support/temp_path.h"

using namespace SectorTrackIo;
using Bytes = std::vector<uint8_t>;

namespace {

namespace fs = std::filesystem;

// ─── Baukasten ───────────────────────────────────────────────────────────────

const std::string kKopfText = "IMD 1.17: 22/10/2026 12:00:00\r\nVektor\r\n";

Bytes kopf() {
    Bytes b(kKopfText.begin(), kKopfText.end());
    b.push_back(0x1A);
    return b;
}

Bytes cat(Bytes a, const Bytes& b) { a.insert(a.end(), b.begin(), b.end()); return a; }

Bytes steigend(size_t n) { Bytes b(n); for (size_t i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(i); return b; }

SectorRecord sek(uint8_t c, uint8_t h, uint8_t r, uint8_t n, uint8_t fill) {
    SectorRecord s;
    s.c = c; s.h = h; s.r = r; s.n = n;
    s.data.assign(s.size(), fill);
    return s;
}

/// Sektor mit nicht-konstantem Inhalt (verhindert die Kompression)
SectorRecord sekMuster(uint8_t c, uint8_t h, uint8_t r, uint8_t n, unsigned seed) {
    SectorRecord s = sek(c, h, r, n, 0);
    for (size_t i = 0; i < s.data.size(); ++i) s.data[i] = static_cast<uint8_t>(seed * 31u + i * 7u + (i >> 3));
    return s;
}

TrackRecord spur(uint8_t cyl, uint8_t head, Encoding enc, std::vector<SectorRecord> secs,
                 uint16_t rate = 250) {
    TrackRecord t;
    t.cyl = cyl; t.head = head; t.enc = enc; t.rate_kbps = rate;
    t.sectors = std::move(secs);
    return t;
}

DiskMedium medium(const std::vector<TrackRecord>& t) {
    DiskMedium m;
    std::string err;
    EXPECT_TRUE(toMedium(t, m, err)) << err;
    return m;
}

/// Medium → Datei → Medium
struct Lauf {
    Bytes datei;
    DiskMedium zurueck;
    ImdCodec::SourceInfo schrieb, las;
};

Lauf rundlauf(const DiskMedium& m, const ImdCodec::SourceInfo* vorlage = nullptr) {
    Lauf l;
    std::string err;
    EXPECT_TRUE(ImdCodec::encode(m, vorlage, l.datei, &l.schrieb, err)) << err;
    EXPECT_TRUE(ImdCodec::loadBuffer(l.datei, l.zurueck, &l.las, err)) << err;
    return l;
}

void gleicheMedien(const DiskMedium& a, const DiskMedium& b) {
    ASSERT_EQ(a.numCylinders(), b.numCylinders());
    ASSERT_EQ(a.numHeads(), b.numHeads());
    for (uint8_t c = 0; c < a.numCylinders(); ++c)
        for (uint8_t h = 0; h < a.numHeads(); ++h) {
            const TrackImage& x = a.peek(c, h);
            const TrackImage& y = b.peek(c, h);
            EXPECT_EQ(x.bytes, y.bytes) << "Spur " << int(c) << "/" << int(h);
            EXPECT_EQ(x.marks, y.marks) << "Spur " << int(c) << "/" << int(h);
            EXPECT_EQ(x.encoding, y.encoding) << "Spur " << int(c) << "/" << int(h);
        }
}

bool laedt(const Bytes& d, std::string* err = nullptr) {
    DiskMedium m;
    std::string e;
    const bool ok = ImdCodec::loadBuffer(d, m, nullptr, e);
    if (err) *err = e;
    return ok;
}

// ─── die Vektoren aus 38a ────────────────────────────────────────────────────

Bytes vektorA() {
    Bytes d = kopf();
    for (uint8_t b : {0x05, 0x00, 0x00, 0x02, 0x00, 0x01, 0x02}) d.push_back(b);
    d.push_back(0x02); d.push_back(0xE5);
    d.push_back(0x01);
    const Bytes n = steigend(128);
    d.insert(d.end(), n.begin(), n.end());
    return d;
}

Bytes vektorB() {
    Bytes d = kopf();
    for (uint8_t b : {0x02, 0x03, 0xC1, 0x03, 0x00,
                      0x09, 0x08, 0x07,
                      0x04, 0x03, 0x03,
                      0x00, 0x01, 0x00})
        d.push_back(b);
    d.push_back(0x04); d.push_back(0x00);
    d.push_back(0x05); d.insert(d.end(), 128, 0xAA);
    d.push_back(0x00);
    return d;
}

Bytes vektorC() {
    Bytes d = kopf();
    for (uint8_t b : {0x03, 0x01, 0x00, 0x00, 0x02,
                      0x03, 0x02, 0x00, 0x01, 0x02,
                      0x01,
                      0x02, 0x5A})
        d.push_back(b);
    return d;
}

}  // namespace

// ─── Vektoren ────────────────────────────────────────────────────────────────

TEST(ImdCodec, VektorA_MfmMitKompression) {
    const Bytes d = vektorA();
    EXPECT_EQ(d.size(), 40u + 5 + 2 + 2 + 129);   // 38a nennt 0x24/0xB2; der Kopf hat tatsaechlich 40 Byte
    DiskMedium m;
    ImdCodec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(ImdCodec::loadBuffer(d, m, &si, err)) << err;
    EXPECT_EQ(m.numCylinders(), 1);
    EXPECT_EQ(m.numHeads(), 1);
    EXPECT_EQ(si.version, "1.17");
    EXPECT_EQ(si.timestamp, "22/10/2026 12:00:00");
    EXPECT_EQ(si.comment, "Vektor");
    ASSERT_EQ(si.modes.size(), 1u);
    EXPECT_EQ(si.modes[0].mode, 5);

    const TrackImage& t = m.peek(0, 0);
    EXPECT_EQ(t.encoding, Encoding::MFM);
    const auto s = TrackCodec::parseTrack(t);
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].cyl, 0); EXPECT_EQ(s[0].head, 0); EXPECT_EQ(s[0].id, 1);
    EXPECT_EQ(s[1].id, 2);
    EXPECT_EQ(s[0].data, Bytes(128, 0xE5));
    EXPECT_EQ(s[1].data, steigend(128));
    for (const auto& x : s) { EXPECT_FALSE(x.deleted); EXPECT_TRUE(x.id_crc_ok); EXPECT_TRUE(x.data_crc_ok); EXPECT_EQ(x.size, 128); }

    // Zurueckschreiben mit Vorlage ist bytegleich zur Quelle (Kopf, Modus, Typen)
    Bytes neu;
    ASSERT_TRUE(ImdCodec::encode(m, &si, neu, nullptr, err)) << err;
    EXPECT_EQ(neu, d);
}

TEST(ImdCodec, VektorB_FmMitBeidenMaps) {
    DiskMedium m;
    ImdCodec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(ImdCodec::loadBuffer(vektorB(), m, &si, err)) << err;
    EXPECT_EQ(m.numCylinders(), 4);
    EXPECT_EQ(m.numHeads(), 2);
    ASSERT_EQ(si.modes.size(), 1u);
    EXPECT_EQ(si.modes[0].mode, 2);

    const TrackImage& t = m.peek(3, 1);
    EXPECT_EQ(t.encoding, Encoding::FM);
    const auto s = TrackCodec::parseTrack(t);
    ASSERT_EQ(s.size(), 3u);
    // (C,H,R,N)
    EXPECT_EQ(s[0].cyl, 4); EXPECT_EQ(s[0].head, 0); EXPECT_EQ(s[0].id, 9);
    EXPECT_TRUE(s[0].deleted); EXPECT_TRUE(s[0].data_crc_ok);
    EXPECT_EQ(s[0].data, Bytes(128, 0x00));
    EXPECT_EQ(s[1].cyl, 3); EXPECT_EQ(s[1].head, 1); EXPECT_EQ(s[1].id, 8);
    EXPECT_FALSE(s[1].deleted); EXPECT_FALSE(s[1].data_crc_ok);
    EXPECT_EQ(s[1].data, Bytes(128, 0xAA));
    EXPECT_EQ(s[2].cyl, 3); EXPECT_EQ(s[2].head, 0); EXPECT_EQ(s[2].id, 7);
    EXPECT_EQ(s[2].data_pos, SIZE_MAX);   // ohne Datenfeld
    // alle anderen Spuren sind unformatiert
    EXPECT_TRUE(m.peek(0, 0).empty());
    EXPECT_TRUE(m.peek(3, 0).empty());

    // Rundlauf mit Vorlage: gleiche Spuren; die fuer die Geometrie noetigen leeren
    // Spuren kommen als n = 0 hinzu
    const Lauf l = rundlauf(m, &si);
    gleicheMedien(m, l.zurueck);
    EXPECT_EQ(l.schrieb.tracks, 8u);
    EXPECT_EQ(l.schrieb.empty_tracks, 7u);
}

TEST(ImdCodec, VektorC_LeereSpurUndFolgetrack) {
    DiskMedium m;
    ImdCodec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(ImdCodec::loadBuffer(vektorC(), m, &si, err)) << err;
    EXPECT_EQ(m.numCylinders(), 3);
    EXPECT_EQ(m.numHeads(), 1);
    EXPECT_EQ(si.tracks, 2u);
    EXPECT_EQ(si.empty_tracks, 1u);
    EXPECT_TRUE(m.peek(1, 0).empty());
    const auto s = TrackCodec::parseTrack(m.peek(2, 0));
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0].id, 1);
    EXPECT_EQ(s[0].size, 512);
    EXPECT_EQ(s[0].data, Bytes(512, 0x5A));
    EXPECT_EQ(m.peek(2, 0).encoding, Encoding::MFM);
}

// ─── Rundlauf ────────────────────────────────────────────────────────────────

class ImdRund : public ::testing::TestWithParam<Encoding> {};

TEST_P(ImdRund, InterleaveFlagsUndMaps) {
    const Encoding enc = GetParam();
    std::vector<SectorRecord> s0;
    for (uint8_t r : {1, 5, 2, 6, 3, 7, 4}) s0.push_back(sekMuster(0, 0, r, 1, r));   // Interleave 2
    SectorRecord gel = sekMuster(0, 0, 8, 1, 80); gel.deleted = true;
    SectorRecord dfe = sekMuster(0, 0, 9, 1, 90); dfe.data_error = true;
    SectorRecord bei = sekMuster(0, 0, 10, 1, 100); bei.deleted = bei.data_error = true;
    SectorRecord kom = sek(0, 0, 11, 1, 0xE5);
    SectorRecord nod = sek(0, 0, 12, 1, 0); nod.no_data = true; nod.data.clear();
    s0.push_back(gel); s0.push_back(dfe); s0.push_back(bei); s0.push_back(kom); s0.push_back(nod);

    std::vector<TrackRecord> in;
    in.push_back(spur(0, 0, enc, s0));
    // Kopf 1: Kopf-ID weicht ab (Kopf-Map), Zylinder-ID gleich
    in.push_back(spur(0, 1, enc, {sekMuster(0, 7, 1, 2, 1), sekMuster(0, 7, 2, 2, 2)}));
    // Zylinder 1 fehlt ganz, Zylinder 2: Zylinder-ID weicht ab (Zylinder-Map) und doppelte Nummer
    in.push_back(spur(2, 0, enc, {sekMuster(9, 0, 1, 0, 3), sekMuster(2, 0, 1, 0, 4), sekMuster(2, 0, 3, 0, 5)}));
    const DiskMedium m = medium(in);

    const Lauf l = rundlauf(m);
    gleicheMedien(m, l.zurueck);
    EXPECT_EQ(l.las.sectors, l.schrieb.sectors);

    // Reihenfolge (Interleave) und ID-Felder bleiben
    const auto s = TrackCodec::parseTrack(l.zurueck.peek(0, 0));
    ASSERT_EQ(s.size(), s0.size());
    const uint8_t soll[] = {1, 5, 2, 6, 3, 7, 4, 8, 9, 10, 11, 12};
    for (size_t i = 0; i < s.size(); ++i) EXPECT_EQ(s[i].id, soll[i]) << i;
    const auto k1 = TrackCodec::parseTrack(l.zurueck.peek(0, 1));
    ASSERT_EQ(k1.size(), 2u);
    EXPECT_EQ(k1[0].head, 7);
    const auto z2 = TrackCodec::parseTrack(l.zurueck.peek(2, 0));
    ASSERT_EQ(z2.size(), 3u);
    EXPECT_EQ(z2[0].cyl, 9);
    EXPECT_EQ(z2[1].cyl, 2);
    EXPECT_TRUE(l.zurueck.peek(1, 0).empty());
    // zweiter Schreibvorgang aus dem geladenen Medium ist bytegleich zum ersten
    Bytes zweit;
    std::string err;
    ASSERT_TRUE(ImdCodec::encode(l.zurueck, &l.las, zweit, nullptr, err)) << err;
    EXPECT_EQ(zweit, l.datei);
}

INSTANTIATE_TEST_SUITE_P(FmMfm, ImdRund, ::testing::Values(Encoding::FM, Encoding::MFM));

TEST(ImdCodec, AlleNeunSektortypenBeimSchreiben) {
    // Reihenfolge der Sektoren = Typ 1..8, dann 0
    std::vector<SectorRecord> sec;
    for (unsigned t = 1; t <= 8; ++t) {
        const bool komp = ((t - 1) & 1) != 0;
        SectorRecord s = komp ? sek(0, 0, static_cast<uint8_t>(t), 0, static_cast<uint8_t>(0x10 + t))
                              : sekMuster(0, 0, static_cast<uint8_t>(t), 0, t);
        s.deleted    = ((t - 1) & 2) != 0;
        s.data_error = ((t - 1) & 4) != 0;
        sec.push_back(s);
    }
    SectorRecord nod = sek(0, 0, 9, 0, 0); nod.no_data = true; nod.data.clear();
    sec.push_back(nod);
    const Lauf l = rundlauf(medium({spur(0, 0, Encoding::MFM, sec)}));
    gleicheMedien(medium({spur(0, 0, Encoding::MFM, sec)}), l.zurueck);

    // Typbytes aus der Datei lesen: ab Kopf + 5 + 9 Nummern
    size_t p = static_cast<size_t>(std::find(l.datei.begin(), l.datei.end(), uint8_t{0x1A}) - l.datei.begin()) + 1 + 5 + 9;
    const uint8_t soll[] = {1, 2, 3, 4, 5, 6, 7, 8, 0};
    for (size_t i = 0; i < 9; ++i) {
        ASSERT_LT(p, l.datei.size());
        EXPECT_EQ(l.datei[p], soll[i]) << "Sektor " << i + 1;
        p += 1;
        if (soll[i] == 0) continue;
        p += (soll[i] % 2 == 0) ? 1 : 128;   // gerade Typen sind komprimiert
    }
    EXPECT_EQ(p, l.datei.size());
}

TEST(ImdCodec, Modus300UeberDieVorlageUndAbleitungOhne) {
    // Datei mit Modus 4 (MFM 300) und Modus 1 (FM 300)
    Bytes d = kopf();
    for (uint8_t b : {0x04, 0x00, 0x00, 0x01, 0x00, 0x01}) d.push_back(b);
    d.push_back(0x01); { const Bytes n = steigend(128); d.insert(d.end(), n.begin(), n.end()); }
    for (uint8_t b : {0x01, 0x01, 0x00, 0x01, 0x00, 0x01}) d.push_back(b);
    d.push_back(0x01); { const Bytes n = steigend(128); d.insert(d.end(), n.begin(), n.end()); }
    DiskMedium m;
    ImdCodec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(ImdCodec::loadBuffer(d, m, &si, err)) << err;
    EXPECT_EQ(si.modes.size(), 2u);

    Bytes mit, ohne;
    ASSERT_TRUE(ImdCodec::encode(m, &si, mit, nullptr, err)) << err;
    ASSERT_TRUE(ImdCodec::encode(m, nullptr, ohne, nullptr, err)) << err;
    EXPECT_EQ(mit, d);                      // Kopf und Modi wie die Quelle
    // je Spursatz 5 (Kopf) + 1 (Nummer) + 1 (Typ) + 128 (Daten) = 135 Byte
    EXPECT_EQ(ohne[ohne.size() - 270], 5);   // MFM 250
    EXPECT_EQ(ohne[ohne.size() - 135], 2);   // FM 250
}

TEST(ImdCodec, NeueDateiHatStandardkopf) {
    Bytes d;
    std::string err;
    ASSERT_TRUE(ImdCodec::encode(medium({spur(0, 0, Encoding::MFM, {sek(0, 0, 1, 0, 1)})}),
                                 nullptr, d, nullptr, err)) << err;
    ASSERT_GT(d.size(), 40u);
    EXPECT_EQ(std::string(d.begin(), d.begin() + 10), "IMD 1.17: ");
    ImdCodec::SourceInfo si;
    DiskMedium m;
    ASSERT_TRUE(ImdCodec::loadBuffer(d, m, &si, err)) << err;
    EXPECT_EQ(si.comment, "Image file created by k1520emu");
    EXPECT_EQ(si.version, "1.17");
    EXPECT_EQ(si.timestamp.size(), 19u);
}

// ─── Ablehnungen beim Schreiben ──────────────────────────────────────────────

TEST(ImdCodec, GemischteGroessenInEinerSpurWerdenAbgelehnt) {
    const DiskMedium m = medium({spur(0, 0, Encoding::MFM, {sek(0, 0, 1, 0, 1), sek(0, 0, 2, 1, 2)})});
    Bytes d;
    std::string err;
    EXPECT_FALSE(ImdCodec::encode(m, nullptr, d, nullptr, err));
    EXPECT_NE(err.find("gemischten Sektorgroessen"), std::string::npos) << err;
    EXPECT_NE(err.find("Spur 0/0"), std::string::npos) << err;
    // unterschiedliche Groessen in VERSCHIEDENEN Spuren sind erlaubt
    EXPECT_TRUE(ImdCodec::encode(medium({spur(0, 0, Encoding::MFM, {sek(0, 0, 1, 0, 1)}),
                                         spur(1, 0, Encoding::MFM, {sek(1, 0, 1, 2, 2)})}),
                                 nullptr, d, nullptr, err)) << err;
}

TEST(ImdCodec, UdosNachspannIdCrcFehlerUndUnformatiertWerdenAbgelehnt) {
    Bytes d;
    std::string err;
    {   // Nachspann mit Nutzdaten (UDOS-Sektorkontrollblock)
        std::vector<LogicalSector> v;
        LogicalSector l; l.id = 1; l.size = 128; l.data.assign(128, 0x5A); l.tail = {1, 2, 3, 4};
        v.push_back(l);
        DiskMedium m(1, 1, Encoding::MFM);
        m.setTrack(0, 0, TrackCodec::buildTrack(v, Encoding::MFM));
        EXPECT_FALSE(ImdCodec::encode(m, nullptr, d, nullptr, err));
        EXPECT_NE(err.find("Daten-CRC"), std::string::npos) << err;
    }
    {   // ID-CRC-Fehler
        SectorRecord s = sek(0, 0, 1, 0, 1); s.id_error = true;
        EXPECT_FALSE(ImdCodec::encode(medium({spur(0, 0, Encoding::MFM, {s})}), nullptr, d, nullptr, err));
        EXPECT_NE(err.find("ID-CRC"), std::string::npos) << err;
    }
    {   // unformatiert
        DiskMedium m(2, 1, Encoding::MFM);
        EXPECT_FALSE(ImdCodec::encode(m, nullptr, d, nullptr, err));
        EXPECT_NE(err.find("unformatiert"), std::string::npos) << err;
    }
}

// ─── kaputte Dateien ─────────────────────────────────────────────────────────

TEST(ImdCodec, KaputteKopfzeilenWerdenAbgewiesen) {
    std::string err;
    EXPECT_FALSE(laedt({}, &err));
    EXPECT_NE(err.find("Kennung"), std::string::npos);
    Bytes d = vektorA();
    d[0] = 'X';
    EXPECT_FALSE(laedt(d, &err));
    EXPECT_NE(err.find("Kennung"), std::string::npos);
    d = vektorB();             // (Vektor A enthaelt selbst ein 0x1A als Datenbyte)
    d.erase(d.begin() + 39);   // das 0x1A fehlt
    EXPECT_FALSE(laedt(d, &err)) << err;
    EXPECT_NE(err.find("0x1A"), std::string::npos) << err;
    // nur der Kopf, keine Spur
    EXPECT_FALSE(laedt(kopf(), &err));
    EXPECT_NE(err.find("keine Spuren"), std::string::npos) << err;
}

TEST(ImdCodec, UngueltigeFelderWerdenMitGrundAbgewiesen) {
    struct Fall { size_t off; uint8_t wert; const char* wort; };
    const Fall faelle[] = {
        {40, 6, "Modus"},        // Modus 6
        {42, 2, "Kopf"},         // Kopf 2
        {44, 7, "Sektorgroessen-Code"},
        {44, 4, "1024"},         // 2048 Byte: nicht darstellbar
        {47, 9, "Sektortyp"},    // erstes Typbyte
    };
    for (const Fall& f : faelle) {
        Bytes d = vektorA();
        d[f.off] = f.wert;
        std::string err;
        EXPECT_FALSE(laedt(d, &err)) << f.wort;
        EXPECT_NE(err.find(f.wort), std::string::npos) << f.wort << ": " << err;
    }
}

TEST(ImdCodec, AbgeschnittenOhneZwangsErfolg) {
    for (const Bytes& voll : {vektorA(), vektorB()}) {
        for (size_t n = 0; n < voll.size(); ++n) {
            const Bytes d(voll.begin(), voll.begin() + static_cast<long>(n));
            std::string err;
            EXPECT_FALSE(laedt(d, &err)) << "Laenge " << n;
        }
    }
}

TEST(ImdCodec, RestUnterFuenfByteWirdMitWarnungToleriert) {
    Bytes d = cat(vektorA(), {1, 2, 3});
    DiskMedium m;
    ImdCodec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(ImdCodec::loadBuffer(d, m, &si, err)) << err;
    EXPECT_EQ(si.trailing_bytes, 3u);
    ASSERT_EQ(si.warnings.size(), 1u);
    // 5 Byte Rest sind dagegen ein (abgeschnittener) Satz
    d = cat(vektorA(), {3, 1, 0, 1, 0});
    EXPECT_FALSE(laedt(d, &err));
}

TEST(ImdCodec, DoppelterSpursatzUeberschreibtMitWarnung) {
    Bytes zweit;   // Zyl 0 Kopf 0, 1 Sektor 128 B, Fuellbyte 0x77
    for (uint8_t b : {0x05, 0x00, 0x00, 0x01, 0x00, 0x09, 0x02, 0x77}) zweit.push_back(b);
    DiskMedium m;
    ImdCodec::SourceInfo si;
    std::string err;
    ASSERT_TRUE(ImdCodec::loadBuffer(cat(vektorA(), zweit), m, &si, err)) << err;
    EXPECT_EQ(si.tracks, 1u);
    EXPECT_EQ(si.warnings.size(), 1u);
    const auto s = TrackCodec::parseTrack(m.peek(0, 0));
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0].id, 9);
    EXPECT_EQ(s[0].data, Bytes(128, 0x77));
}

TEST(ImdCodec, LeereSpurDarfBeliebigenGroessencodeTragenBis6) {
    Bytes d = kopf();
    for (uint8_t b : {0x05, 0x00, 0x00, 0x00, 0x06,      // n = 0, s = 6: leer, in Ordnung
                      0x05, 0x01, 0x00, 0x01, 0x00, 0x01, 0x02, 0x11})
        d.push_back(b);
    std::string err;
    EXPECT_TRUE(laedt(d, &err)) << err;
    d[44] = 7;   // aber 7 ist nie gueltig
    EXPECT_FALSE(laedt(d, &err));
}

TEST(ImdCodec, DoppelteSektornummernBleibenErhalten) {
    const DiskMedium m = medium({spur(0, 0, Encoding::MFM,
        {sekMuster(0, 0, 1, 0, 1), sekMuster(0, 0, 1, 0, 2), sekMuster(0, 0, 2, 0, 3)})});
    const Lauf l = rundlauf(m);
    gleicheMedien(m, l.zurueck);
}

TEST(ImdCodec, GrosseGeometrieMitKleinemSektorRundlauf) {
    // 80 Zylinder x 2 Koepfe, 5 x 1024 (MFM), Kopf 1 teilweise unformatiert
    std::vector<TrackRecord> in;
    for (uint8_t c = 0; c < 80; ++c) {
        std::vector<SectorRecord> s;
        for (uint8_t r = 1; r <= 5; ++r) s.push_back(sekMuster(c, 0, r, 3, c * 5u + r));
        in.push_back(spur(c, 0, Encoding::MFM, s));
    }
    const DiskMedium m = medium(in);
    const Lauf l = rundlauf(m);
    gleicheMedien(m, l.zurueck);
}

// ─── Datei / ImageCodec ──────────────────────────────────────────────────────

TEST(ImdCodec, SpeichernIstAtomarUndBehaeltDieAlteDateiBeiFehler) {
    const std::string pfad = k1520test::tempPath("k1520_test_imd_codec_a.imd");
    fs::remove(pfad);
    const DiskMedium gut = medium({spur(0, 0, Encoding::MFM, {sek(0, 0, 1, 0, 0x42)})});
    std::string err;
    ASSERT_TRUE(ImdCodec::save(pfad, gut, err)) << err;
    EXPECT_FALSE(fs::exists(pfad + ".k1520tmp"));
    EXPECT_TRUE(ImdCodec::isImd(pfad));

    std::ifstream f(pfad, std::ios::binary);
    const Bytes vorher((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    // nicht darstellbares Medium: Datei bleibt unveraendert, keine Nachbardatei
    const DiskMedium schlecht = medium({spur(0, 0, Encoding::MFM, {sek(0, 0, 1, 0, 1), sek(0, 0, 2, 1, 2)})});
    EXPECT_FALSE(ImdCodec::save(pfad, schlecht, err));
    EXPECT_FALSE(fs::exists(pfad + ".k1520tmp"));
    std::ifstream g(pfad, std::ios::binary);
    const Bytes nachher((std::istreambuf_iterator<char>(g)), std::istreambuf_iterator<char>());
    EXPECT_EQ(vorher, nachher);

    DiskMedium m;
    ASSERT_TRUE(ImdCodec::load(pfad, m, nullptr, err)) << err;
    gleicheMedien(gut, m);
    fs::remove(pfad);
    EXPECT_FALSE(ImdCodec::load(pfad, m, nullptr, err));
}

TEST(ImdCodec, ErkennungUndImageCodec) {
    EXPECT_FALSE(ImdCodec::isImd(Bytes{'I', 'M', 'D', ' ', '1', '.', '1', '7'}.data(), 8));   // ohne 0x1A
    EXPECT_TRUE(ImdCodec::isImd(vektorA().data(), vektorA().size()));

    // ImageCodec: Erkennung an der Kennung, auch ohne Endung; Laden/Speichern ueber die Fabrik
    const std::string pfad = k1520test::tempPath("k1520_test_imd_codec_b.bin");
    {
        const Bytes d = vektorC();
        std::ofstream o(pfad, std::ios::binary);
        o.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
    }
    EXPECT_EQ(ImageCodec::detect(pfad), ContainerType::Imd);
    EXPECT_EQ(ImageCodec::fromExtension("x.IMD"), ContainerType::Imd);
    DiskMedium m;
    std::string err;
    ASSERT_TRUE(ImageCodec::load(pfad, ContainerType::Imd, nullptr, m, err)) << err;
    EXPECT_EQ(m.numCylinders(), 3);
    const std::string aus = k1520test::tempPath("k1520_test_imd_codec_c.imd");
    ASSERT_TRUE(ImageCodec::save(aus, ContainerType::Imd, nullptr, m, err)) << err;
    DiskMedium m2;
    ASSERT_TRUE(ImageCodec::load(aus, ContainerType::Imd, nullptr, m2, err)) << err;
    gleicheMedien(m, m2);
    fs::remove(pfad);
    fs::remove(aus);
}

// ─── Beispiele fuer den unabhaengigen Python-Leser ───────────────────────────

namespace {

std::string hex(const Bytes& b) {
    static const char* z = "0123456789abcdef";
    std::string s;
    for (uint8_t x : b) { s += z[x >> 4]; s += z[x & 15]; }
    return s;
}

/// Inhaltsangabe aus dem MEDIUM (nicht aus dem Schreiber): JSON-Feld "tracks".
std::string inhaltJson(const DiskMedium& m) {
    std::vector<TrackRecord> t;
    std::string err;
    EXPECT_TRUE(fromMedium(m, t, err)) << err;
    std::ostringstream o;
    o << "{\"cyls\":" << int(m.numCylinders()) << ",\"heads\":" << int(m.numHeads()) << ",\"tracks\":[";
    for (size_t i = 0; i < t.size(); ++i) {
        o << (i ? "," : "") << "{\"cyl\":" << int(t[i].cyl) << ",\"head\":" << int(t[i].head)
          << ",\"enc\":\"" << (t[i].enc == Encoding::FM ? "fm" : "mfm") << "\",\"sectors\":[";
        for (size_t k = 0; k < t[i].sectors.size(); ++k) {
            const auto& s = t[i].sectors[k];
            o << (k ? "," : "") << "{\"c\":" << int(s.c) << ",\"h\":" << int(s.h) << ",\"r\":" << int(s.r)
              << ",\"n\":" << int(s.n) << ",\"deleted\":" << (s.deleted ? 1 : 0)
              << ",\"data_error\":" << (s.data_error ? 1 : 0) << ",\"no_data\":" << (s.no_data ? 1 : 0)
              << ",\"data\":\"" << hex(s.data) << "\"}";
        }
        o << "]}";
    }
    o << "]}";
    return o.str();
}

void schreibeBeispiel(const fs::path& dir, const std::string& name, const DiskMedium& m) {
    std::string err;
    ASSERT_TRUE(ImdCodec::save((dir / (name + ".imd")).string(), m, err)) << err;
    std::ofstream(dir / (name + ".json")) << inhaltJson(m);
}

}  // namespace

TEST(ImdGegenprobe, SchreibtBeispiele) {
    const char* ziel = std::getenv("K1520_IMD_AUSGABE");
    if (!ziel || !*ziel) GTEST_SKIP() << "K1520_IMD_AUSGABE nicht gesetzt (nur fuer tests/python/test_imd_gegenprobe.py)";
    const fs::path dir(ziel);
    fs::create_directories(dir);

    {   // 2 x 2 x 9 x 512, MFM 250, Interleave 2: der Fall, den auch gw versteht
        std::vector<TrackRecord> t;
        for (uint8_t c = 0; c < 2; ++c)
            for (uint8_t h = 0; h < 2; ++h) {
                std::vector<SectorRecord> s;
                for (uint8_t r : {1, 6, 2, 7, 3, 8, 4, 9, 5}) s.push_back(sekMuster(c, h, r, 2, c * 100u + h * 10u + r));
                t.push_back(spur(c, h, Encoding::MFM, s));
            }
        schreibeBeispiel(dir, "mfm_interleave", medium(t));
    }
    {   // FM mit Maps, geloescht, Datenfehler, ohne Datenfeld, komprimiert, 300 kbit/s per Vorlage entfaellt
        SectorRecord gel = sekMuster(1, 0, 3, 1, 3); gel.deleted = true;
        SectorRecord dfe = sekMuster(1, 0, 4, 1, 4); dfe.data_error = true;
        SectorRecord nod = sek(1, 0, 5, 1, 0); nod.no_data = true; nod.data.clear();
        std::vector<TrackRecord> t;
        t.push_back(spur(0, 0, Encoding::FM, {sek(0, 0, 1, 0, 0xE5), sekMuster(0, 0, 2, 0, 2)}));
        t.push_back(spur(1, 0, Encoding::FM, {sekMuster(5, 0, 1, 1, 1), sek(1, 3, 2, 1, 0), gel, dfe, nod}));
        schreibeBeispiel(dir, "fm_alles", medium(t));
    }
    {   // gemischt: FM- und MFM-Spuren, grosse Sektoren, leere Spur in der Mitte
        std::vector<TrackRecord> t;
        t.push_back(spur(0, 0, Encoding::FM, {sekMuster(0, 0, 1, 0, 9), sekMuster(0, 0, 2, 0, 8)}));
        t.push_back(spur(2, 0, Encoding::MFM, {sekMuster(2, 0, 1, 3, 7), sekMuster(2, 0, 2, 3, 6)}));
        schreibeBeispiel(dir, "gemischt_leer", medium(t));
    }
}
