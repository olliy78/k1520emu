/**
 * @file test_format_schema.cpp
 * @brief Waechter W1/W2 (Entwurf 47 §1): das Namensschema der Formate.
 *
 * FormatKatalogSchema.*  -  `DiskFormat::schemaName()` an Beispielen, und der AUSGELIEFERTE
 *                           Katalog: jeder Name beschreibt seine eigene Spurliste (W1), keine zwei
 *                           Formate haben dieselbe Spurliste samt Optionen (W2).
 *
 * Regel `_ed` (der Katalog fuehrt fuer pc_ed keine eigene Rate): Rate 500 => `_hd`, sonst
 * 36 Sektoren x 512 Byte => `_ed`.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "core/peripherals/floppy_drive/format_catalog.h"

namespace {

TrackFormat tf(int c0, int c1, int h0, int h1, int sek, int gr,
               Encoding enc = Encoding::MFM, int erste = 1) {
    TrackFormat t;
    t.cyl_first = static_cast<uint8_t>(c0);  t.cyl_last  = static_cast<uint8_t>(c1);
    t.head_first = static_cast<uint8_t>(h0); t.head_last = static_cast<uint8_t>(h1);
    t.secs_per_track = static_cast<uint8_t>(sek);
    t.bytes_per_sec  = static_cast<uint16_t>(gr);
    t.encoding = enc;
    t.first_sector_id = static_cast<uint8_t>(erste);
    return t;
}

DiskFormat fmt(std::vector<TrackFormat> tr, int step = 1, int rate = 250) {
    DiskFormat f;
    f.tracks = std::move(tr);
    f.step = static_cast<uint8_t>(step);
    f.rate_kbps = static_cast<uint16_t>(rate);
    return f;
}

const FormatCatalog& katalog() {
    static const FormatCatalog k = [] {
        std::string fatal;
        FormatCatalog c = FormatCatalog::load({K1520_FORMATS_DEFAULT}, &fatal);
        EXPECT_TRUE(fatal.empty()) << fatal;
        return c;
    }();
    return k;
}

}  // namespace

TEST(FormatKatalogSchema, EinfacheFormate) {
    EXPECT_EQ(fmt({tf(0, 79, 0, 1, 5, 1024)}).schemaName(), "ds80_5x1024");
    EXPECT_EQ(fmt({tf(0, 79, 0, 0, 5, 1024)}).schemaName(), "ss80_5x1024");
    EXPECT_EQ(fmt({tf(0, 76, 0, 1, 26, 128)}).schemaName(), "ds77_26x128");
}

TEST(FormatKatalogSchema, SystemzonenZaehlenSpurseiten) {
    // cpa780: c0 beide Koepfe + c1h0 = 3 Spurseiten 26x128, dann 1024er
    EXPECT_EQ(fmt({tf(0, 0, 0, 1, 26, 128), tf(1, 1, 0, 0, 26, 128),
                   tf(1, 1, 1, 1, 5, 1024), tf(2, 79, 0, 1, 5, 1024)}).schemaName(),
              "ds80_3t26x128-5x1024");
    EXPECT_EQ(fmt({tf(0, 2, 0, 1, 26, 128), tf(3, 79, 0, 1, 5, 1024)}).schemaName(),
              "ds80_6t26x128-5x1024");
    EXPECT_EQ(fmt({tf(0, 0, 0, 0, 26, 128), tf(1, 76, 0, 0, 8, 512)}).schemaName(),
              "ss77_1t26x128-8x512");
}

TEST(FormatKatalogSchema, Kodierung) {
    // ganze Diskette FM => Praefix, keine Zonensuffixe
    EXPECT_EQ(fmt({tf(0, 76, 0, 0, 4, 1024, Encoding::FM)}).schemaName(), "fm_ss77_4x1024");
    // nur die Systemzone FM => Suffix `fm`
    EXPECT_EQ(fmt({tf(0, 1, 0, 0, 26, 128, Encoding::FM), tf(2, 76, 0, 0, 8, 1024)}).schemaName(),
              "ss77_2t26x128fm-8x1024");
    EXPECT_EQ(fmt({tf(0, 0, 0, 0, 16, 128, Encoding::FM), tf(0, 0, 1, 1, 16, 256),
                   tf(1, 79, 0, 1, 16, 256)}).schemaName(),
              "ds80_1t16x128fm-16x256");
}

TEST(FormatKatalogSchema, Optionen) {
    EXPECT_EQ(fmt({tf(0, 39, 0, 0, 9, 512)}, 2).schemaName(), "ss40_9x512_dstep");
    EXPECT_EQ(fmt({tf(0, 39, 0, 0, 9, 512, Encoding::MFM, 65)}).schemaName(), "ss40_9x512_id65");
    EXPECT_EQ(fmt({tf(0, 39, 0, 0, 10, 512, Encoding::MFM, 0)}).schemaName(), "ss40_10x512_id0");
    EXPECT_EQ(fmt({tf(0, 79, 0, 1, 18, 512)}, 1, 500).schemaName(), "ds80_18x512_hd");
    EXPECT_EQ(fmt({tf(0, 79, 0, 1, 36, 512)}).schemaName(), "ds80_36x512_ed");
}

// W1: der Name ist die Beschreibung der eigenen Spurliste
TEST(FormatKatalogSchema, JederNameEntsprichtSeinerSpurliste) {
    ASSERT_GT(katalog().formats().size(), 50u);
    for (const DiskFormat& f : katalog().formats()) {
        EXPECT_EQ(f.name, f.schemaName()) << "Name luegt ueber die Spurliste";
        for (char c : f.name)
            EXPECT_TRUE((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
                << f.name;
    }
}

// W2: zwei Eintraege mit gleicher Spurliste (samt Schritt/Rate/Drehzahl) sind ein Fehler
TEST(FormatKatalogSchema, KeineDoppeltenSpurlisten) {
    using Schluessel = std::tuple<std::vector<std::tuple<int, int, int, int, int, int, int, int, int>>,
                                  int, int, int>;
    std::map<Schluessel, std::string> gesehen;
    for (const DiskFormat& f : katalog().formats()) {
        std::vector<std::tuple<int, int, int, int, int, int, int, int, int>> tr;
        for (const TrackFormat& t : f.tracks)
            tr.emplace_back(t.cyl_first, t.cyl_last, t.head_first, t.head_last, t.secs_per_track,
                            t.bytes_per_sec, static_cast<int>(t.encoding), t.first_sector_id,
                            t.cell_factor);
        std::sort(tr.begin(), tr.end());
        const Schluessel k{tr, f.step, f.rate_kbps, f.rpm};
        const auto it = gesehen.find(k);
        EXPECT_TRUE(it == gesehen.end())
            << f.name << " hat dieselbe Spurliste wie " << (it == gesehen.end() ? "" : it->second);
        gesehen[k] = f.name;
    }
}

// Die Umbenennungstabelle (Schritt 2): jeder alte Name loest auf genau den neuen auf.
TEST(FormatKatalogSchema, UmbenennungstabelleStimmt) {
    std::ifstream in(std::string(FIXTURE_DIR) + "/../formatnamen_neu.txt");
    ASSERT_TRUE(in.good());
    std::string zeile;
    int n = 0;
    while (std::getline(in, zeile)) {
        if (zeile.empty()) continue;
        const auto tab = zeile.find('\t');
        ASSERT_NE(tab, std::string::npos) << zeile;
        const std::string alt = zeile.substr(0, tab), neu = zeile.substr(tab + 1);
        const DiskFormat* f = katalog().find(alt);
        ASSERT_NE(f, nullptr) << alt;
        EXPECT_EQ(f->name, neu) << alt;
        ++n;
    }
    EXPECT_GT(n, 60);
}
