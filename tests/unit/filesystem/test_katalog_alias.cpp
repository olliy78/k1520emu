/**
 * @file test_katalog_alias.cpp
 * @brief Wacher W3 (Entwurf 47): `aliases:` bei Formaten und Dateisystemprofilen.
 *
 * FormatKatalogAlias.* / FsKatalogAlias.*  -  test-eigener Katalog `formats_alias.yaml`
 * KatalogAltnamen.*                         -  der AUSGELIEFERTE Katalog loest jeden alten Namen auf
 *                                              (eingefrorene Liste `formatnamen_alt.txt`).
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/filesystem/fs_catalog.h"
#include "tests/support/temp_path.h"

namespace {

namespace fs = std::filesystem;

std::string fixture(const char* name) {
    return (fs::path(FIXTURE_DIR) / ".." / name).string();
}

struct Kataloge {
    FormatCatalog formate;
    FsCatalog     dateisysteme;
    std::string   fatal;
};

Kataloge laden(const std::vector<std::string>& pfade) {
    Kataloge k;
    k.formate = FormatCatalog::load(pfade, &k.fatal);
    EXPECT_TRUE(k.fatal.empty()) << k.fatal;
    k.dateisysteme = FsCatalog::load(pfade, k.formate, &k.fatal);
    EXPECT_TRUE(k.fatal.empty()) << k.fatal;
    return k;
}

/// @brief Katalog aus YAML-Text (Temp-Datei).
Kataloge ausText(const std::string& yaml) {
    static int zaehler = 0;
    const std::string p = k1520test::tempPath("k1520_test_alias_" + std::to_string(++zaehler) + ".yaml");
    std::ofstream(p) << yaml;
    Kataloge k = laden({p});
    std::error_code ec;
    fs::remove(p, ec);
    return k;
}

bool hatHinweis(const std::vector<std::string>& issues, const std::string& teil) {
    return std::any_of(issues.begin(), issues.end(),
                       [&](const std::string& s) { return s.find(teil) != std::string::npos; });
}

const std::string kFormate =
    "version: 1\nformats:\n"
    "  - name: f_a\n    drives: [K5601]\n    encoding: mfm\n"
    "    tracks:\n      - { cyls: 0-79, heads: 0-1, sectors: 5, size: 1024 }\n"
    "  - name: f_b\n    drives: [K5601]\n    encoding: mfm\n"
    "    tracks:\n      - { cyls: 0-79, heads: 0-1, sectors: 16, size: 256 }\n";

std::string fsEintrag(const std::string& name, const std::string& aliases) {
    return "  - name: " + name + "\n    aliases: " + aliases + "\n    format: f_a\n    type: cpm\n"
           "    data_start: { cyl: 0, head: 0 }\n    block_size: 2048\n    dir_entries: 128\n";
}

}  // namespace

// ── Formate ──────────────────────────────────────────────────────────────────

TEST(FormatKatalogAlias, AliasLoestAufDasObjektMitNeuemNamenAuf) {
    const Kataloge k = laden({fixture("formats_alias.yaml")});
    EXPECT_TRUE(k.formate.issues().empty());
    for (const char* alt : {"alias_alt_a", "alias_alt_a2"}) {
        const DiskFormat* f = k.formate.find(alt);
        ASSERT_NE(f, nullptr) << alt;
        EXPECT_EQ(f->name, "alias_neu_a");
    }
    ASSERT_NE(k.formate.find("alias_alt_b"), nullptr);
    EXPECT_EQ(k.formate.find("alias_alt_b")->name, "alias_neu_b");
    EXPECT_EQ(k.formate.find("alias_neu_b")->name, "alias_neu_b");
    EXPECT_EQ(k.formate.find("gibt_es_nicht"), nullptr);
}

TEST(FormatKatalogAlias, ListenEnthaltenKeineAliase) {
    const Kataloge k = laden({fixture("formats_alias.yaml")});
    ASSERT_EQ(k.formate.formats().size(), 2u);
    for (const DiskFormat& f : k.formate.formats())
        EXPECT_EQ(f.name.rfind("alias_neu_", 0), 0u) << f.name;
}

TEST(FormatKatalogAlias, DoppelterAliasWirdGemeldetUndVerworfen) {
    std::string y = kFormate;
    y.replace(y.find("name: f_a\n") + 10, 0, "    aliases: [gemeinsam]\n");
    y.replace(y.find("name: f_b\n") + 10, 0, "    aliases: [gemeinsam, nur_b]\n");
    const Kataloge k = ausText(y);
    EXPECT_TRUE(hatHinweis(k.formate.issues(), "Alias 'gemeinsam' verworfen"));
    EXPECT_EQ(k.formate.find("gemeinsam")->name, "f_a");   // der erste behaelt ihn
    EXPECT_EQ(k.formate.find("nur_b")->name, "f_b");
}

TEST(FormatKatalogAlias, AliasDerEinenKatalogNamenVerdecktWirdVerworfen) {
    std::string y = kFormate;
    y.replace(y.find("name: f_a\n") + 10, 0, "    aliases: [f_b, f_a]\n");
    const Kataloge k = ausText(y);
    EXPECT_TRUE(hatHinweis(k.formate.issues(), "Alias 'f_b' verworfen"));
    EXPECT_TRUE(hatHinweis(k.formate.issues(), "Alias 'f_a' verworfen"));   // eigener Name
    EXPECT_EQ(k.formate.find("f_b")->name, "f_b");
    EXPECT_EQ(k.formate.find("f_a")->name, "f_a");
}

// ── Dateisystemprofile ───────────────────────────────────────────────────────

TEST(FsKatalogAlias, AliasLoestAufDasObjektMitNeuemNamenAuf) {
    const Kataloge k = laden({fixture("formats_alias.yaml")});
    EXPECT_TRUE(k.dateisysteme.issues().empty());
    for (const char* alt : {"fsalias_alt_b", "fsalias_alt_b2"}) {
        const FsProfile* p = k.dateisysteme.find(alt);
        ASSERT_NE(p, nullptr) << alt;
        EXPECT_EQ(p->name, "fsalias_neu_b");
    }
    EXPECT_EQ(k.dateisysteme.find("fsalias_alt_a")->name, "fsalias_neu_a");
    EXPECT_EQ(k.dateisysteme.find("gibt_es_nicht"), nullptr);
}

TEST(FsKatalogAlias, ListenEnthaltenKeineAliase) {
    const Kataloge k = laden({fixture("formats_alias.yaml")});
    ASSERT_EQ(k.dateisysteme.profiles().size(), 2u);
    for (const FsProfile& p : k.dateisysteme.profiles())
        EXPECT_EQ(p.name.rfind("fsalias_neu_", 0), 0u) << p.name;
}

TEST(FsKatalogAlias, DoppelterAliasWirdGemeldetUndVerworfen) {
    const Kataloge k = ausText(kFormate + "filesystems:\n" + fsEintrag("p_a", "[gemeinsam]")
                               + fsEintrag("p_b", "[gemeinsam, nur_b]"));
    EXPECT_TRUE(hatHinweis(k.dateisysteme.issues(), "Alias 'gemeinsam' verworfen"));
    EXPECT_EQ(k.dateisysteme.find("gemeinsam")->name, "p_a");
    EXPECT_EQ(k.dateisysteme.find("nur_b")->name, "p_b");
}

TEST(FsKatalogAlias, AliasDerEinenProfilNamenVerdecktWirdVerworfen) {
    const Kataloge k = ausText(kFormate + "filesystems:\n" + fsEintrag("p_a", "[p_b, p_a]")
                               + fsEintrag("p_b", "[]"));
    EXPECT_TRUE(hatHinweis(k.dateisysteme.issues(), "Alias 'p_b' verworfen"));
    EXPECT_TRUE(hatHinweis(k.dateisysteme.issues(), "Alias 'p_a' verworfen"));
    EXPECT_EQ(k.dateisysteme.find("p_b")->name, "p_b");
}

// ── Der ausgelieferte Katalog ────────────────────────────────────────────────

TEST(KatalogAltnamen, AusgelieferterKatalogBleibtLadbarOhneBeanstandung) {
    const Kataloge k = laden({K1520_FORMATS_DEFAULT});
    EXPECT_TRUE(k.formate.issues().empty());
    EXPECT_TRUE(k.dateisysteme.issues().empty());
}

TEST(KatalogAltnamen, JederAlteNameLoestAuf) {
    const Kataloge k = laden({K1520_FORMATS_DEFAULT});
    std::ifstream in(fixture("formatnamen_alt.txt"));
    ASSERT_TRUE(in.good());
    std::string zeile;
    int n = 0;
    while (std::getline(in, zeile)) {
        if (zeile.empty()) continue;
        ++n;
        if (zeile.rfind("format ", 0) == 0)
            EXPECT_NE(k.formate.find(zeile.substr(7)), nullptr) << "Format nicht mehr aufloesbar: " << zeile;
        else if (zeile.rfind("fs ", 0) == 0)
            EXPECT_NE(k.dateisysteme.find(zeile.substr(3)), nullptr) << "Profil nicht mehr aufloesbar: " << zeile;
        else
            ADD_FAILURE() << "unbekannte Zeile: " << zeile;
    }
    EXPECT_GT(n, 100);
}
