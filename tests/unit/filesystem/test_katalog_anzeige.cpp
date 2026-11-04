/**
 * @file test_katalog_anzeige.cpp
 * @brief Anzeigefelder title/capacity/machines (Entwurf 47, Schritt 4) und die C-ABI dazu.
 *
 * Test-eigener Katalog `formats_anzeige.yaml`; die Fuellung des ausgelieferten Katalogs
 * prueft erst Schritt 5.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <tuple>

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

// W5: der AUSGELIEFERTE Katalog ist vollstaendig beschriftet (Entwurf 47, Schritt 5).
TEST(KatalogAnzeige, AusgelieferterKatalogIstVollstaendig) {
    std::string fatal;
    const FormatCatalog fk = FormatCatalog::load({K1520_FORMATS_DEFAULT}, &fatal);
    ASSERT_TRUE(fatal.empty()) << fatal;
    const FsCatalog pk = FsCatalog::load({K1520_FORMATS_DEFAULT}, fk, &fatal);
    ASSERT_TRUE(fatal.empty()) << fatal;

    const std::regex kap("^\\d+(,\\d+)?[KM]$");
    // Geschlossener Wortschatz der Rechnernamen (Kopfkommentar von data/formats.yaml).
    const std::set<std::string> wortschatz = {
        "A5120", "A5130", "K8924", "A5120.16", "K8915", "K8915 V2", "PRG 710", "PRG 710-1",
        "PC 1715", "PC 1715W", "P8000", "A7100", "A7150", "PC 1834", "MC80"};

    std::set<std::string> formatTitel;
    for (const DiskFormat& f : fk.formats()) {
        EXPECT_FALSE(f.title.empty()) << f.name;
        EXPECT_TRUE(std::regex_match(f.capacity, kap)) << f.name << ": '" << f.capacity << "'";
        // Eine Zeile der Formatliste muss eindeutig sein: (Kapazitaet, Titel).
        EXPECT_TRUE(formatTitel.insert(f.capacity + "|" + f.title).second)
            << f.name << ": Kapazitaet+Titel doppelt";
    }
    // Profile: (Kapazitaet, Rechner, Titel) unterscheidet zwei Zeilen der Auswahlliste.
    std::set<std::string> zeilen;
    for (const FsProfile& p : pk.profiles()) {
        EXPECT_FALSE(p.title.empty()) << p.name;
        EXPECT_TRUE(std::regex_match(p.capacity, kap)) << p.name << ": '" << p.capacity << "'";
        std::string ms;
        for (const std::string& m : p.machines) {
            EXPECT_TRUE(wortschatz.count(m)) << p.name << ": unbekannter Rechner '" << m << "'";
            ms += m + ",";
        }
        EXPECT_TRUE(zeilen.insert(p.capacity + "|" + ms + "|" + p.title).second)
            << p.name << ": Kapazitaet+Rechner+Titel doppelt";
    }
}

}  // namespace
