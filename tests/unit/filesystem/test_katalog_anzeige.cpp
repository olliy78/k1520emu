/**
 * @file test_katalog_anzeige.cpp
 * @brief Anzeigefelder title/capacity/machines (Entwurf 47, Schritt 4) und die C-ABI dazu.
 *
 * Test-eigener Katalog `formats_anzeige.yaml`; die Fuellung des ausgelieferten Katalogs
 * prueft erst Schritt 5.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "core/filesystem/fs_catalog.h"

namespace {

namespace fs = std::filesystem;

struct Kataloge {
    FormatCatalog formate;
    FsCatalog     dateisysteme;
};

Kataloge laden() {
    const std::string p = (fs::path(FIXTURE_DIR) / ".." / "formats_anzeige.yaml").string();
    Kataloge k;
    std::string fatal;
    k.formate = FormatCatalog::load({p}, &fatal);
    EXPECT_TRUE(fatal.empty()) << fatal;
    k.dateisysteme = FsCatalog::load({p}, k.formate, &fatal);
    EXPECT_TRUE(fatal.empty()) << fatal;
    return k;
}

TEST(KatalogAnzeige, FormatFelderWerdenGelesen) {
    const Kataloge k = laden();
    const DiskFormat* a = k.formate.find("anz_a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->title, "Anzeigeformat A");
    EXPECT_EQ(a->capacity, "800K");
}

TEST(KatalogAnzeige, FehlendeFormatFelderBleibenLeer) {
    const Kataloge k = laden();
    const DiskFormat* b = k.formate.find("anz_b");
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->title.empty());
    EXPECT_TRUE(b->capacity.empty());
}

TEST(KatalogAnzeige, ProfilFelderUndMehrereRechner) {
    const Kataloge k = laden();
    const FsProfile* a = k.dateisysteme.find("anzfs_neu_a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->title, "Anzeigeprofil A");
    EXPECT_EQ(a->capacity, "1,44M");
    ASSERT_EQ(a->machines.size(), 2u);
    EXPECT_EQ(a->machines[0], "PC 1715");
    EXPECT_EQ(a->machines[1], "PC 1715W");
}

TEST(KatalogAnzeige, FehlendeProfilFelderBleibenLeer) {
    const Kataloge k = laden();
    const FsProfile* b = k.dateisysteme.find("anzfs_neu_b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->title, "Anzeigeprofil B");
    EXPECT_TRUE(b->capacity.empty());
    EXPECT_TRUE(b->machines.empty());   // leer = unbekannt, kein Fehler
}

TEST(KatalogAnzeige, AliasLiefertDieFelderDesNeuenProfils) {
    const Kataloge k = laden();
    const FsProfile* p = k.dateisysteme.find("anzfs_alt_a");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->name, "anzfs_neu_a");
    EXPECT_EQ(p->title, "Anzeigeprofil A");
}

}  // namespace
