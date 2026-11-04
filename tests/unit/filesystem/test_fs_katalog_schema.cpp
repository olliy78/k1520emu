/**
 * @file test_fs_katalog_schema.cpp
 * @brief Waechter W4 (Entwurf 47 §2): das Namensschema der Dateisystemprofile.
 *
 * FsKatalogSchema.*  -  jeder Profilname des AUSGELIEFERTEN Katalogs ist
 *                       `<os>_<format ohne fuehrende Systemzonen>[_<rolle>]`, `os` aus einer geschlossenen
 *                       Liste, `rolle` aus einem geschlossenen Wortschatz; `format:` zeigt auf ein
 *                       existierendes Format; Namen sind eindeutig und kein Alias verdeckt einen Namen;
 *                       die Tabelle `profilnamen_neu.txt` stimmt mit dem Katalog ueberein.
 *
 * Os und Rolle werden NICHT aus dem Katalog erraten: die Tabelle haelt sie fest, der Test prueft nur,
 * dass der Name aus Format + (os, rolle) aus der geschlossenen Liste aufgebaut ist.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "core/filesystem/fs_catalog.h"

namespace {

namespace fs = std::filesystem;

std::string fixture(const char* name) {
    return (fs::path(FIXTURE_DIR) / ".." / name).string();
}

struct Kataloge {
    FormatCatalog formate;
    FsCatalog     dateisysteme;
};

const Kataloge& kataloge() {
    static const Kataloge k = [] {
        Kataloge r;
        std::string fatal;
        r.formate = FormatCatalog::load({K1520_FORMATS_DEFAULT}, &fatal);
        EXPECT_TRUE(fatal.empty()) << fatal;
        r.dateisysteme = FsCatalog::load({K1520_FORMATS_DEFAULT}, r.formate, &fatal);
        EXPECT_TRUE(fatal.empty()) << fatal;
        return r;
    }();
    return k;
}

/// Geschlossene Liste der Betriebssystem-/Familienkuerzel (Entwurf 47 §2).
const std::set<std::string>& osListe() {
    static const std::set<std::string> s = {
        "cpa", "scpx", "scpx8915", "cpa1715", "scp1715", "microdos", "microdos1715", "osm", "bcu880",
        "scp1700", "udos", "udos1715", "udos_p8000", "udos50", "sios", "wega", "mutos1700", "mutos8k",
        "tar", "dos"};
    return s;
}

/// Geschlossener Wortschatz der Rollen.
const std::set<std::string>& rollen() {
    static const std::set<std::string> s = {"boot", "daten", "arbeit", "syl", "kombi", "leer"};
    return s;
}

/// Formatname ohne die fuehrenden Systemzonen `<n>t<S>x<B>[fm]-`; `fm_`-Praefix und Suffixe bleiben.
std::string ohneSystemzonen(const std::string& format) {
    static const std::regex kopf("^((?:fm_)?(?:ss|ds)[0-9]+_)([^_]+)(.*)$");
    std::smatch m;
    if (!std::regex_match(format, m, kopf)) return format;
    const std::string zonen = m[2];
    const auto strich = zonen.rfind('-');
    const std::string daten = strich == std::string::npos ? zonen : zonen.substr(strich + 1);
    return m[1].str() + daten + m[3].str();
}

/// Passt @p name auf `<os>_<erwartet>[_<rolle>]` mit os/rolle aus den geschlossenen Mengen?
bool passtAufSchema(const std::string& name, const std::string& erwartet) {
    for (const auto& os : osListe()) {
        const std::string kopf = os + "_" + erwartet;
        if (name == kopf) return true;
        if (name.rfind(kopf + "_", 0) == 0 && rollen().count(name.substr(kopf.size() + 1))) return true;
    }
    return false;
}

}  // namespace

TEST(FsKatalogSchema, OhneSystemzonenAnBeispielen) {
    EXPECT_EQ(ohneSystemzonen("ds80_3t26x128-5x1024"), "ds80_5x1024");
    EXPECT_EQ(ohneSystemzonen("ds80_1t16x128fm-16x256"), "ds80_16x256");
    EXPECT_EQ(ohneSystemzonen("ss77_1t26x128-8x512"), "ss77_8x512");
    EXPECT_EQ(ohneSystemzonen("fm_ss77_26x128"), "fm_ss77_26x128");
    EXPECT_EQ(ohneSystemzonen("ds80_18x512_hd"), "ds80_18x512_hd");
    EXPECT_EQ(ohneSystemzonen("ds40_5x1024_dstep"), "ds40_5x1024_dstep");
}

TEST(FsKatalogSchema, JederProfilnameFolgtDemSchema) {
    for (const FsProfile& p : kataloge().dateisysteme.profiles()) {
        const std::string erwartet = ohneSystemzonen(p.format);
        EXPECT_TRUE(passtAufSchema(p.name, erwartet))
            << "Profil '" << p.name << "' folgt nicht <os>_" << erwartet << "[_<rolle>] (format: " << p.format << ")";
        const std::string zeichen = "abcdefghijklmnopqrstuvwxyz0123456789_-";
        EXPECT_EQ(p.name.find_first_not_of(zeichen), std::string::npos) << p.name;
    }
}

TEST(FsKatalogSchema, FormatZeigtAufEinExistierendesFormat) {
    for (const FsProfile& p : kataloge().dateisysteme.profiles()) {
        const DiskFormat* f = kataloge().formate.find(p.format);
        ASSERT_NE(f, nullptr) << p.name << ": Format '" << p.format << "' fehlt";
        EXPECT_EQ(f->name, p.format) << p.name << ": 'format:' soll den echten Namen tragen, nicht einen Alias";
    }
}

TEST(FsKatalogSchema, NamenEindeutigUndKeinAliasVerdecktEinenNamen) {
    std::set<std::string> namen;
    for (const FsProfile& p : kataloge().dateisysteme.profiles())
        EXPECT_TRUE(namen.insert(p.name).second) << "doppelter Profilname " << p.name;
    std::set<std::string> aliase;
    for (const FsProfile& p : kataloge().dateisysteme.profiles())
        for (const auto& a : p.aliases) {
            EXPECT_EQ(namen.count(a), 0u) << "Alias '" << a << "' von " << p.name << " verdeckt einen Profilnamen";
            EXPECT_TRUE(aliase.insert(a).second) << "doppelter Alias " << a;
        }
}

TEST(FsKatalogSchema, UmbenennungstabelleStimmtMitDemKatalogUeberein) {
    std::ifstream in(fixture("profilnamen_neu.txt"));
    ASSERT_TRUE(in.good());
    std::string zeile;
    int n = 0;
    std::set<std::string> neue;
    while (std::getline(in, zeile)) {
        if (zeile.empty()) continue;
        const auto tab = zeile.find('\t');
        ASSERT_NE(tab, std::string::npos) << zeile;
        const std::string alt = zeile.substr(0, tab), neu = zeile.substr(tab + 1);
        const FsProfile* p = kataloge().dateisysteme.find(alt);
        ASSERT_NE(p, nullptr) << alt;
        EXPECT_EQ(p->name, neu) << alt;
        EXPECT_NE(std::find(p->aliases.begin(), p->aliases.end(), alt), p->aliases.end())
            << alt << " ist kein Alias von " << neu;
        EXPECT_NE(kataloge().dateisysteme.find(neu), nullptr) << neu;
        neue.insert(neu);
        ++n;
    }
    EXPECT_EQ(n, 58);
    // Der Katalog hat kein Profil, das die Tabelle nicht kennt - ausser den nach Schema 47 neu
    // angelegten, die nie einen Altnamen hatten (Z2: CPA1715_5600).
    const std::set<std::string> ohneAltnamen = {"cpa1715_ss40_5x1024_boot"};
    for (const FsProfile& p : kataloge().dateisysteme.profiles())
        if (ohneAltnamen.count(p.name))
            EXPECT_EQ(neue.count(p.name), 0u) << p.name << " hat keinen Altnamen, steht aber in der Tabelle";
        else
            EXPECT_EQ(neue.count(p.name), 1u) << p.name << " fehlt in profilnamen_neu.txt";
}

TEST(FsKatalogSchema, JederAlteProfilnameLoestWeiterAuf) {
    std::ifstream in(fixture("formatnamen_alt.txt"));
    ASSERT_TRUE(in.good());
    std::string zeile;
    int n = 0;
    while (std::getline(in, zeile)) {
        if (zeile.rfind("fs ", 0) != 0) continue;
        ++n;
        EXPECT_NE(kataloge().dateisysteme.find(zeile.substr(3)), nullptr) << zeile;
    }
    EXPECT_GE(n, 50);
}
