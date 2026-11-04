/**
 * @file test_fs_catalog.cpp
 * @brief GoogleTests fuer @ref FsCatalog — die Sektion `filesystems:` der formats.yaml.
 *
 * Zwei Blickrichtungen:
 *   1. der **ausgelieferte** Katalog `data/formats.yaml` ist vollstaendig und plausibel;
 *   2. das Schema wehrt fehlerhafte Definitionen ab, ohne den Rest mitzureissen.
 *
 * @see core/filesystem/fs_catalog.h
 * @see doc/design/13_k1520disktool.md §6
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include "core/filesystem/fs_catalog.h"
#include "tests/support/temp_path.h"

namespace {

std::string shippedCatalog() { return K1520_FORMATS_DEFAULT; }

const FormatCatalog& formate() {
    static FormatCatalog c = [] {
        std::string fatal;
        FormatCatalog k = FormatCatalog::load({shippedCatalog()}, &fatal);
        EXPECT_TRUE(fatal.empty()) << fatal;
        return k;
    }();
    return c;
}

/// @brief Katalog aus einem YAML-Schnipsel bauen (Temp-Datei, raeumt sich weg).
class TempKatalog {
public:
    explicit TempKatalog(const std::string& yaml) {
        path_ = k1520test::tempPath(("k1520_test_fscat_" + std::to_string(++zaehler_) + ".yaml"));
        std::ofstream(path_) << yaml;
    }
    ~TempKatalog() { std::error_code ec; std::filesystem::remove(path_, ec); }
    const std::string& path() const { return path_; }

private:
    std::string   path_;
    static inline int zaehler_ = 0;
};

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Der ausgelieferte Katalog
// ─────────────────────────────────────────────────────────────────────────────

TEST(FsCatalog, AusgelieferterKatalogLaedtOhneBeanstandung) {
    std::string fatal;
    FsCatalog cat = FsCatalog::load({shippedCatalog()}, formate(), &fatal);
    ASSERT_TRUE(fatal.empty()) << fatal;

    for (const auto& i : cat.issues()) ADD_FAILURE() << "Beanstandung: " << i;
    EXPECT_FALSE(cat.profiles().empty());
}

TEST(FsCatalog, ProfilnamenSindEinStabilerVertrag) {
    // Wie bei den Formatnamen: CLI, GUI und C-API nennen diese Zeichenketten.
    std::string fatal;
    FsCatalog cat = FsCatalog::load({shippedCatalog()}, formate(), &fatal);
    ASSERT_TRUE(fatal.empty()) << fatal;

    // Die Liste ist bewusst KURZ: seit die CP/A-Regel den DPB selbst ausrechnet
    // (@ref CpaDpbRule), braucht ein Eintrag hier einen eigenen Grund — UDOS (andere
    // Familie) oder ein Name, den `create --fs` und die Oberflaeche brauchen.
    const std::vector<std::string> erwartet = {
        "cpa_ds80_5x1024_boot", "cpa_ds80_5x1024_daten",
        "scpx_ds80_16x256_boot", "scpx_ds80_5x1024_boot",
        "udos_ds77_26x128", "udos_ss77_26x128", "udos_ss40_26x128",
        // UDOS auf dem 8-Zoll-MF3200 (FM): `create` nimmt das Verfahren aus dem Format
        // (MUTOS-Ladediskette, Entwurf 39 9.13).
        "udos_fm_ss77_26x128",
        "udos1715_ds80_16x256", "udos1715_ss80_16x256", "udos1715_ss40_16x256",
        // dieselbe NDOS-Diskette, unterschieden nur an der Fuellung hinter dem Plan
        "udos_p8000_ds80_16x256", "udos50_ds80_16x256",
        // CP/M-86 des A7100: eigener Eintrag, weil die CP/A-Regel hier NICHT gilt
        // (sie bildet das CP/A-BIOS nach, nicht das SCP1700).
        "scp1700_ds80_16x256_boot",
        // SCPX 8915 (K8915): festes OFF 2 auch ohne Systemspuren.  Nur mit `--fs`
        // (`detect: false`) — eine LEERE Diskette ist sonst von `cpa_ds80_5x1024_daten` nicht zu
        // unterscheiden (doc/design/16_k8915.md §8a AP-E5a).
        "scpx8915_ds80_5x1024_boot",
        // PC 1715 (AP-D): CP/A 1715 mit Bootkopf IM Verzeichnis (erkannt nur mit Kopf) und
        // der Name fuer die SCP-1715-/SCP-3.0-Systemdiskette (`detect: false`).
        "cpa1715_ds80_5x1024_boot", "scp1715_ds80_5x1024_boot",
        // WEGA (P8000, UNIX System III): andere Familie, beschreibt sich selbst
        // (doc/design/27_wega_dateisystem.md) — ein Eintrag fuer die 720-K-Diskette.
        "wega_ds80_9x512",
        // MUTOS 8000 (A5120.16): dasselbe Dateisystem, Blockraum ab Zylinder 1.
        "mutos8k_ss77_8x512", "mutos8k_ss80_8x512", "mutos1700_ds80_9x512", "mutos1700_ds80_16x256", "mutos8k_fm_ss77_26x128",
        // … dieselbe 8k-Wurzel in MFM (K5601); Rang -10, damit sie vor udos_ss77_26x128 gewinnt.
        "mutos8k_ss77_26x128",
        // tar-Disketten (P8000-Lieferungen ohne Dateisystem, nur lesbar): letzter
        // Erkennungsversuch hinter allen echten Dateisystemen (disk_volume.cpp).
        "tar_ds80_9x512",
        // … und die LEERE tar-Diskette (ganz Null; nimmt genau ein .tar auf, `leer: true`).
        "tar_ds80_9x512_leer",
        // … und tar im MUTOS-8000-Layout (Rohgeraet /dev/rrkN, Entwurf 39): MFM (K5601) und FM (MF3200),
        // je mit der leeren Fassung.  Erkennung VOR den echten Dateisystemen (disk_volume.cpp).
        "tar_ss77_26x128", "tar_ss77_26x128_leer", "tar_fm_ss77_26x128", "tar_fm_ss77_26x128_leer",
        // … und im 8x512-Zuschnitt (DISK-Satz, lineares Rohgeraet ab Zylinder 1, 608 Bloecke; Entwurf 39 A5c).
        "tar_ss77_8x512", "tar_ss77_8x512_leer",
        // BCU 880: Arbeitsdiskette „ohne SYL“ (Name, `detect: false`) und die Startdiskette
        // „SYL auf Spur 1“ (erkannt am Lader; `nur_boot`).
        "bcu880_ss40_26x128_arbeit", "bcu880_ss40_26x128_syl", "bcu880_ss40_26x128_kombi", "bcu880_ss80_26x128_kombi", "bcu880_ss80_26x128_arbeit",
        // OS/M (P8000, doc/design/34_osm.md): Name, Bootabbild-Pruefung (`boot_header: osm`)
        // und — bei 800K — die leere Diskette, die sonst `cpa_ds80_5x1024_daten` ab c0h0 waere.  Erkannt nur
        // mit Urlader; `osm_ds80_16x256_boot` ist deckungsgleich mit `scpx_ds80_16x256_boot`.
        "osm_ds80_16x256_boot", "osm_ds80_5x1024_boot",
        // SIOS 1526 (doc/sios_diskettenformat.md): Erkennung am Nachweis ERMAP + VOL1.
        "sios_ss40_26x128", "sios_ss80_26x128",
        // MicroDOS 1526/1715 (Rostock): Lader im Verzeichnis (Erkennung am Lader) und die
        // Datendisketten ohne Lader (`detect: false`), Entwurf 13 §25.
        "microdos_ds80_5x1024_boot", "microdos_ss40_5x1024_boot", "microdos_ss40_26x128_boot", "microdos_ds80_5x1024_daten", "microdos_ss40_5x1024_daten", "microdos1715_ds80_5x1024_boot",
        // MS-DOS FAT12 (Entwurf 44): erkannt am BPB, `create --fs dosNNN` nimmt den Namen.
        "dos_ss40_8x512", "dos_ss40_9x512", "dos_ds40_8x512", "dos_ds40_9x512", "dos_ss80_9x512", "dos_ds80_8x512", "dos_ds80_9x512", "dos_ds80_10x512", "dos_ds80_15x512_hd", "dos_ds80_18x512_hd", "dos_ds80_36x512_ed",
    };
    for (const auto& n : erwartet)
        EXPECT_NE(cat.find(n), nullptr) << "Dateisystem '" << n << "' fehlt";
    for (const auto& p : cat.profiles())
        EXPECT_NE(std::find(erwartet.begin(), erwartet.end(), p.name), erwartet.end())
            << "Unerwartetes Dateisystem '" << p.name << "' — Erwartungsliste mitpflegen";
    EXPECT_EQ(cat.profiles().size(), erwartet.size());
}

TEST(FsCatalog, AlteProfilnamenLoesenAlsAliasAufDasNeueProfilAuf) {
    // Vertrag: Konfigurationen, Archive und Skripte nennen die alten MUTOS-8000-Namen weiter.
    std::string fatal;
    FsCatalog cat = FsCatalog::load({shippedCatalog()}, formate(), &fatal);
    ASSERT_TRUE(fatal.empty()) << fatal;
    const std::vector<std::pair<std::string, std::string>> paare = {
        {"mutos77", "mutos8k_ss77_8x512"},          {"mutos80", "mutos8k_ss80_8x512"},
        {"mutos8k_fm", "mutos8k_fm_ss77_26x128"},       {"mutos8k_mfm", "mutos8k_ss77_26x128"},
        {"tar_mutos8k_fm", "tar_fm_ss77_26x128"}, {"tar_mutos8k_fm_leer", "tar_fm_ss77_26x128_leer"},
        {"tar_mutos8k_mfm", "tar_ss77_26x128"}, {"tar_mutos8k_mfm_leer", "tar_ss77_26x128_leer"},
    };
    for (const auto& [alt, neu] : paare) {
        const FsProfile* a = cat.find(alt);
        const FsProfile* n = cat.find(neu);
        ASSERT_NE(n, nullptr) << neu;
        EXPECT_EQ(a, n) << alt << " muss auf dasselbe Profil wie " << neu << " aufloesen";
        // Ein Alias ist KEIN eigener Eintrag: nie in der Liste, nie als Profilname.
        for (const auto& p : cat.profiles()) EXPECT_NE(p.name, alt);
    }
}

TEST(FsCatalog, JedesProfilVerweistAufEinVorhandenesFormat) {
    std::string fatal;
    FsCatalog cat = FsCatalog::load({shippedCatalog()}, formate(), &fatal);
    ASSERT_TRUE(fatal.empty()) << fatal;

    for (const auto& p : cat.profiles()) {
        const DiskFormat* f = formate().find(p.format);
        ASSERT_NE(f, nullptr) << p.name << " → unbekanntes Format " << p.format;
        EXPECT_NE(f->findTrack(p.data_cyl, p.data_head), nullptr)
            << p.name << ": data_start liegt ausserhalb der Geometrie";
    }
}

TEST(FsCatalog, Cpa780BeginntBeiZylinder2) {
    std::string fatal;
    FsCatalog cat = FsCatalog::load({shippedCatalog()}, formate(), &fatal);
    const FsProfile* p = cat.find("cpa_ds80_5x1024_boot");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->type, FsType::Cpm);
    EXPECT_EQ(p->data_cyl, 2);
    EXPECT_EQ(p->data_head, 0);
    EXPECT_EQ(p->block_size, 2048u);
    EXPECT_EQ(p->dir_entries, 128);
}

/**
 * @test Auf der 16×256-Geometrie liegt GENAU EIN Profil OHNE positiven Nachweis — und es
 *       beginnt bei c2h0.
 * @par Kriterium  `forFormat("ds80_16x256")` liefert `scpx_ds80_16x256_boot` (data_cyl 2) und sonst nur
 *                 `osm_ds80_16x256_boot`, das ausschliesslich mit OS/M-Urlader erkannt wird und
 *                 deckungsgleich liegt (Entwurf 34 §6: keine Mehrdeutigkeit).
 * @par Warum      Bis 2026-08-11 stand daneben ein `ds80_16x256` mit `data_cyl == 0`,
 *                 gedacht als Beispiel fuer „mehrere Dateisysteme je Geometrie".  Das
 *                 Beispiel war falsch: CP/A kann so eine Diskette nicht erzeugen, denn
 *                 fuer 256-B-Sektoren traegt `dtrsl1` ein FESTES Offset von 4 logischen
 *                 Spuren (`CpaDpb.TabellenzeilenEntsprechenDemBios`).  Der Eintrag
 *                 bewirkte nur, dass jede 16×256-Diskette „nicht eindeutig" meldete.
 *                 Dieser Test haelt die Korrektur fest.
 */
TEST(FsCatalog, SechzehnMalZweihundertsechsundfuenfzigHatNurEinProfilAbZylinderZwei) {
    std::string fatal;
    FsCatalog cat = FsCatalog::load({shippedCatalog()}, formate(), &fatal);
    ASSERT_TRUE(fatal.empty()) << fatal;

    const auto auf640 = cat.forFormat("ds80_16x256");
    std::vector<const FsProfile*> ohne_nachweis;
    for (const FsProfile* p : auf640) {
        // Seit Entwurf 47 teilt sich die Geometrie den Eintrag mit den NDOS-Profilen
        // (frueher `k5601_16x256`); hier geht es nur um die CP/M-Profile.
        if (p->type != FsType::Cpm) continue;
        EXPECT_EQ(p->data_cyl, 2) << p->name;
        EXPECT_EQ(p->dir_entries, 128) << p->name;
        EXPECT_EQ(p->block_size, 2048u) << p->name;
        if (p->boot_header != "osm") ohne_nachweis.push_back(p);
    }
    ASSERT_EQ(ohne_nachweis.size(), 1u) << "ein zweites Profil ohne Nachweis auf dieser "
                                           "Geometrie macht jede 16×256-Diskette wieder mehrdeutig";
    EXPECT_EQ(ohne_nachweis.front()->name, "scpx_ds80_16x256_boot");
    size_t cpm = 0;
    for (const FsProfile* p : auf640) cpm += p->type == FsType::Cpm;
    EXPECT_EQ(cpm, 2u);
    EXPECT_EQ(cat.find("ds80_16x256"), nullptr);
}

TEST(FsCatalog, UdosProfileSindNiemalsImgFaehig) {
    // Der Sektorkontrollblock steht HINTER der Daten-CRC — ein rohes Sektorabbild
    // verliert die gesamte Dateiverkettung (doc/udos_diskettenformat.md, Vorspann).
    std::string fatal;
    FsCatalog cat = FsCatalog::load({shippedCatalog()}, formate(), &fatal);
    for (const auto& p : cat.profiles()) {
        if (p.type != FsType::Udos) continue;
        EXPECT_FALSE(p.allow_img) << p.name << " darf nicht als .img erlaubt sein";
        EXPECT_TRUE(p.allow_hfe);
        EXPECT_TRUE(p.allow_dmk);
        EXPECT_TRUE(p.sides_separate);
        EXPECT_EQ(p.directory_track, 22) << "16H laut FORMATPC.MAC";
        EXPECT_EQ(p.bitmap_track,    23) << "17H laut FORMATPC.MAC";
        EXPECT_EQ(p.boot_track,      21) << "15H laut FORMATPC.MAC";
    }
}

TEST(FsCatalog, Udos1715ProfileSindImgFaehigUndEinseitigGezaehlt) {
    // Umgekehrt zu ZDOS: NDOS haelt ALLES im Sektor (Zeigersektoren statt
    // Gap-Zeigern), deshalb ist `.img` hier moeglich und richtig
    // (doc/udos1715_diskettenformat.md §8).  Und die Spur umfasst beide Seiten —
    // `sides_separate` gibt es dort nicht.
    std::string fatal;
    FsCatalog cat = FsCatalog::load({shippedCatalog()}, formate(), &fatal);
    ASSERT_TRUE(fatal.empty()) << fatal;

    int gefunden = 0;
    for (const auto& p : cat.profiles()) {
        if (p.type != FsType::Udos1715) continue;
        ++gefunden;
        EXPECT_TRUE(p.allow_img) << p.name;
        EXPECT_TRUE(p.allow_hfe) << p.name;
        EXPECT_TRUE(p.allow_dmk) << p.name;
        EXPECT_FALSE(p.sides_separate) << p.name << ": die Spur ist der ganze Zylinder";
        EXPECT_EQ(p.directory_track, 22) << "16H laut Handbuch §1.2.1";
        EXPECT_EQ(p.bitmap_track,    23) << "17H laut Handbuch §1.2.1";
        const DiskFormat* f = formate().find(p.format);
        ASSERT_NE(f, nullptr) << p.name;
        EXPECT_EQ(f->findTrack(0, 0)->bytes_per_sec, 256)
            << p.name << ": NDOS verlangt 256-B-Sektoren (Handbuch §1.1)";
    }
    // Die drei Formate aus Handbuch §1.2 — und fuer das 640K-Format zwei weitere Profile
    // (P8000, UDOS 5.0 am A5120), die sich nur in `ndos_fill` unterscheiden.
    EXPECT_EQ(gefunden, 5);
    std::map<std::string, std::string> fuellung;
    for (const auto& p : cat.profiles())
        if (p.type == FsType::Udos1715 && p.format == "ds80_16x256") fuellung[p.name] = p.ndos_fill;
    const std::map<std::string, std::string> soll = {
        {"udos1715_ds80_16x256", "00"}, {"udos_p8000_ds80_16x256", "77"}, {"udos50_ds80_16x256", "ff"}};
    EXPECT_EQ(fuellung, soll) << "jedes 640K-NDOS-Profil braucht eine EIGENE Fuellung, "
                                 "sonst ist die Erkennung mehrdeutig";
}

// ─────────────────────────────────────────────────────────────────────────────
// Schema-Abwehr
// ─────────────────────────────────────────────────────────────────────────────

TEST(FsCatalog, FehlendeSektionIstKeinFehler) {
    // §6.5: der Emulator kommt ohne `filesystems:` aus.
    TempKatalog k("version: 1\nformats:\n  - name: x\n    drives: [K5601]\n"
                  "    tracks:\n      - { cyls: 0, heads: 0, sectors: 5, size: 1024 }\n");
    std::string fatal;
    FsCatalog cat = FsCatalog::load({k.path()}, formate(), &fatal);
    EXPECT_TRUE(fatal.empty()) << fatal;
    EXPECT_TRUE(cat.profiles().empty());
}

TEST(FsCatalog, UnbekanntesFormatWirdUebersprungenNichtAkzeptiert) {
    TempKatalog k("filesystems:\n"
                  "  - name: kaputt\n    format: gibtsnicht\n    type: cpm\n"
                  "  - name: gut\n    format: ds80_5x1024\n    type: cpm\n");
    std::string fatal;
    FsCatalog cat = FsCatalog::load({k.path()}, formate(), &fatal);
    EXPECT_TRUE(fatal.empty());
    EXPECT_EQ(cat.find("kaputt"), nullptr);
    ASSERT_NE(cat.find("gut"), nullptr) << "eine fehlerhafte Definition darf die "
                                           "uebrigen nicht mitreissen";
    ASSERT_FALSE(cat.issues().empty());
    EXPECT_NE(cat.issues()[0].find("gibtsnicht"), std::string::npos) << cat.issues()[0];
}

TEST(FsCatalog, DataStartAusserhalbDerGeometrieWirdAbgelehnt) {
    TempKatalog k("filesystems:\n"
                  "  - name: zuweit\n    format: ds80_5x1024\n    type: cpm\n"
                  "    data_start: { cyl: 200, head: 0 }\n");
    std::string fatal;
    FsCatalog cat = FsCatalog::load({k.path()}, formate(), &fatal);
    EXPECT_EQ(cat.find("zuweit"), nullptr);
    ASSERT_FALSE(cat.issues().empty());
    EXPECT_NE(cat.issues()[0].find("data_start"), std::string::npos) << cat.issues()[0];
}

TEST(FsCatalog, UngueltigeWerteWerdenBenannt) {
    TempKatalog k("filesystems:\n"
                  "  - name: a\n    format: ds80_5x1024\n    type: raetsel\n"
                  "  - name: b\n    format: ds80_5x1024\n    type: cpm\n    block_size: 3000\n"
                  "  - name: c\n    format: ds80_5x1024\n    type: cpm\n    os: cpm4\n");
    std::string fatal;
    FsCatalog cat = FsCatalog::load({k.path()}, formate(), &fatal);
    EXPECT_TRUE(cat.profiles().empty());
    ASSERT_EQ(cat.issues().size(), 3u);
    EXPECT_NE(cat.issues()[0].find("type"), std::string::npos)       << cat.issues()[0];
    EXPECT_NE(cat.issues()[1].find("block_size"), std::string::npos) << cat.issues()[1];
    EXPECT_NE(cat.issues()[2].find("os"), std::string::npos)         << cat.issues()[2];
}

TEST(FsCatalog, ImgFuerUdosWirdEntferntUndGemeldet) {
    TempKatalog k("filesystems:\n"
                  "  - name: u\n    format: ss77_26x128\n    type: udos\n"
                  "    containers: [img, hfe]\n");
    std::string fatal;
    FsCatalog cat = FsCatalog::load({k.path()}, formate(), &fatal);
    const FsProfile* p = cat.find("u");
    ASSERT_NE(p, nullptr) << "das Profil bleibt gueltig, nur img faellt weg";
    EXPECT_FALSE(p->allow_img);
    EXPECT_TRUE(p->allow_hfe);
    ASSERT_FALSE(cat.issues().empty());
    EXPECT_NE(cat.issues()[0].find("img"), std::string::npos) << cat.issues()[0];
}

TEST(FsCatalog, SpaetereDateiUeberschreibtGleichenNamen) {
    TempKatalog a("filesystems:\n  - name: x\n    format: ds80_5x1024\n    type: cpm\n"
                  "    dir_entries: 64\n");
    TempKatalog b("filesystems:\n  - name: x\n    format: ds80_5x1024\n    type: cpm\n"
                  "    dir_entries: 256\n");
    std::string fatal;
    FsCatalog cat = FsCatalog::load({a.path(), b.path()}, formate(), &fatal);
    ASSERT_EQ(cat.profiles().size(), 1u);
    EXPECT_EQ(cat.find("x")->dir_entries, 256) << "spaetere Datei hat Vorrang";
}

/**
 * @test FsCatalog.DetectUndBootHeaderWerdenGeprueft
 * @brief AP-T1a (Felder aus AP-E5a/E5c): `detect` nur true/false, `boot_header` nur
 *        `k8915` oder leer — alles andere wird mit Feldnamen beanstandet und der
 *        Eintrag uebersprungen; ein gueltiger Eintrag uebernimmt beide Werte.
 */
TEST(FsCatalog, DetectUndBootHeaderWerdenGeprueft) {
    TempKatalog k("filesystems:\n"
                  "  - name: a\n    format: ds80_5x1024\n    type: cpm\n    detect: vielleicht\n"
                  "  - name: b\n    format: ds80_5x1024\n    type: cpm\n    boot_header: a5120\n"
                  "  - name: c\n    format: ds80_5x1024\n    type: cpm\n    detect: false\n"
                  "    boot_header: k8915\n");
    std::string fatal;
    FsCatalog cat = FsCatalog::load({k.path()}, formate(), &fatal);
    ASSERT_EQ(cat.issues().size(), 2u);
    EXPECT_NE(cat.issues()[0].find("detect"), std::string::npos)      << cat.issues()[0];
    EXPECT_NE(cat.issues()[1].find("boot_header"), std::string::npos) << cat.issues()[1];
    ASSERT_EQ(cat.profiles().size(), 1u);
    EXPECT_EQ(cat.profiles()[0].name, "c");
    EXPECT_FALSE(cat.profiles()[0].detect);
    EXPECT_EQ(cat.profiles()[0].boot_header, "k8915");
}
