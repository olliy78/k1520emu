/**
 * @file test_fat_fs.cpp
 * @brief GoogleTests fuer @ref FatFileSystem — MS-DOS-FAT12 (PC, A7150/DCP, PC 1834).
 *
 * ### Woher die Sollwerte stammen
 * | Sollwert | Quelle |
 * |---|---|
 * | BPB der Normgroessen (Medienkennung, Cluster, Wurzel, FAT-Laenge) | die Lieferdisketten (720K: `A7150_DCP33`, `A7150_BAS`, `1834_*`) und die Tabelle der DOS-Formate |
 * | FAT12-Verpackung (3 Byte je 2 Eintraege), `0xFF8…0xFFF` = Ende | Microsoft FAT-Spezifikation |
 * | 8.3-Kurzname, Pruefsumme des Kurznamens, Eintraege fuer lange Namen | dito, VFAT |
 * | DOS-Datum/Uhrzeit (Bits) | dito |
 *
 * Die echten Disketten (`K1520_DOS_DISKETTEN` = Ordner mit den `A7150_*.hfe`/`1834_*.hfe`,
 * Vorgabe `~/projects/robotron/Diskettensammlung_von_DL/Diskettenarbeit/Disketten_Images_RT`) werden nur
 * geprueft, wenn sie da sind.  Alles andere laeuft auf selbst angelegten Disketten.
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fat/fat_fs.h"
#include "core/filesystem/fs_catalog.h"
#include "core/peripherals/floppy_drive/disk_image.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;

namespace {

const FormatCatalog& formate() {
    static FormatCatalog c = [] {
        std::string f;
        FormatCatalog k = FormatCatalog::load({K1520_FORMATS_DEFAULT}, &f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

const FsCatalog& dateisysteme() {
    static FsCatalog c = [] {
        std::string f;
        FsCatalog k = FsCatalog::load({K1520_FORMATS_DEFAULT}, formate(), &f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

/// @brief Eine formatierte Diskette im Speicher samt FAT-Dateisystem.
struct Diskette {
    std::unique_ptr<DiskImage> img;
    std::unique_ptr<SectorSpace> raum;
    std::unique_ptr<FatFileSystem> fat;
    std::string pfad;
    std::string profil;

    explicit Diskette(const std::string& profilName = "dos_ds80_9x512", const std::string& label = "TEST") {
        profil = profilName;
        const FsProfile* p = dateisysteme().find(profilName);
        EXPECT_TRUE(p) << profilName;
        const DiskFormat* f = p ? formate().find(p->format) : nullptr;
        EXPECT_TRUE(f);
        if (!p || !f) return;
        pfad = k1520test::tempPath("k1520_test_fat_" + profilName + ".img");
        std::string grund;
        img = DiskImage::create(pfad, *f, false, f->predominantEncoding(), &grund);
        EXPECT_TRUE(img) << grund;
        if (!img) return;
        raum = std::make_unique<SectorSpace>(img->medium(), *f);
        std::string err;
        fat = FatFileSystem::format(*raum, *p, label, err);
        EXPECT_TRUE(fat) << err;
    }
    ~Diskette() { fat.reset(); raum.reset(); img.reset(); std::error_code ec; fs::remove(pfad, ec); }

    /// @brief Das ganze Medium als Bytefolge (Vergleich vorher/nachher).
    std::vector<uint8_t> abzug() const {
        std::vector<uint8_t> v(static_cast<size_t>(raum->size()));
        EXPECT_TRUE(raum->read(0, v.data(), v.size()));
        return v;
    }
    /// @brief Neu mounten (alles, was nur im Speicher des Objekts stand, geht verloren).
    void neuMounten() {
        std::string w;
        fat = FatFileSystem::mount(*raum, *dateisysteme().find(profil), w);
        ASSERT_TRUE(fat) << w;
    }
};

std::vector<uint8_t> muster(size_t n, uint8_t start = 1) {
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(start + i * 31 + i / 512 * 7);
    return v;
}

WriteOptions neu(int lfn = -1) {
    WriteOptions o;
    o.fat_lfn = lfn;
    return o;
}

const FileEntry* finde(const std::vector<FileEntry>& l, const std::string& name) {
    for (const FileEntry& e : l) if (e.name == name) return &e;
    return nullptr;
}

std::string befunde(const FsCheckReport& r) { return r.alsText(); }

std::string dosOrdner() {
    if (const char* e = std::getenv("K1520_DOS_DISKETTEN")) return e;
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/projects/robotron/Diskettensammlung_von_DL/Diskettenarbeit/Disketten_Images_RT" : "";
}

}  // namespace

// ─── BPB und Normgroessen ────────────────────────────────────────────────────

TEST(FatFs, StandardBpbDerNormgroessen) {
    struct Soll { uint64_t bytes; uint16_t heads, spt; uint8_t media, spc; uint16_t rde, spf; uint32_t cluster; };
    // Werte der DOS-Formattabelle; `cluster` = (Sektoren - 1 - 2*spf - Wurzel) / spc
    const Soll tab[] = {
        {163840,  1, 8,  0xFE, 1, 64,  1, 313},
        {184320,  1, 9,  0xFC, 1, 64,  2, 351},
        {327680,  2, 8,  0xFF, 2, 112, 1, 315},
        {368640,  2, 9,  0xFD, 2, 112, 2, 354},
        {737280,  2, 9,  0xF9, 2, 112, 3, 713},
        {1228800, 2, 15, 0xF9, 1, 224, 7, 2371},
        {1474560, 2, 18, 0xF0, 1, 224, 9, 2847},
        {2949120, 2, 36, 0xF0, 2, 240, 9, 2863},
    };
    for (const Soll& s : tab) {
        fat::Bpb b;
        ASSERT_TRUE(fat::standardBpb(s.bytes, s.heads, s.spt, b)) << s.bytes;
        EXPECT_EQ(b.media, s.media) << s.bytes;
        EXPECT_EQ(b.spc, s.spc) << s.bytes;
        EXPECT_EQ(b.rde, s.rde) << s.bytes;
        EXPECT_EQ(b.spf, s.spf) << s.bytes;
        EXPECT_EQ(b.nfats, 2);
        EXPECT_EQ(b.clusters, s.cluster) << s.bytes;
    }
    fat::Bpb b;
    EXPECT_FALSE(fat::standardBpb(1000000, 2, 9, b));
}

TEST(FatFs, FormatiertAlleProfileUndMountetSieWieder) {
    for (const char* name : {"dos_ss40_8x512", "dos_ss40_9x512", "dos_ds40_8x512", "dos_ds40_9x512", "dos_ss80_9x512", "dos_ds80_8x512", "dos_ds80_9x512", "dos_ds80_10x512", "dos_ds80_15x512_hd", "dos_ds80_18x512_hd", "dos_ds80_36x512_ed"}) {
        Diskette d(name, "PRUEFUNG");
        ASSERT_TRUE(d.fat) << name;
        const FsInfo i = d.fat->info();
        EXPECT_EQ(i.label, "PRUEFUNG") << name;
        EXPECT_EQ(i.free_bytes, i.total_bytes) << name;
        EXPECT_EQ(i.files, 0) << name;
        std::string w;
        EXPECT_TRUE(FatFileSystem::looksLikeFat(*d.raum, &w)) << name << ": " << w;
        d.neuMounten();
        const FsCheckReport r = d.fat->check(FsCheckLevel::Voll, true);
        EXPECT_TRUE(r.ohneBefund()) << name << "\n" << befunde(r);
        std::vector<uint8_t> boot;
        ASSERT_TRUE(d.fat->readBootSector(boot));
        EXPECT_EQ(boot[510], 0x55);
        EXPECT_EQ(boot[511], 0xAA);
    }
}

TEST(FatFs, ErkenntKeinCpmUndKeineLeereDiskette) {
    // Eine ganz mit 0xE5 gefuellte Diskette (frisch formatiertes CP/M) ist kein FAT.
    Diskette d("dos_ds80_9x512");
    ASSERT_TRUE(d.fat);
    const std::vector<uint8_t> e5(512, 0xE5);
    for (uint64_t o = 0; o < 20 * 512; o += 512) ASSERT_TRUE(d.raum->write(o, e5.data(), e5.size()));
    std::string w;
    EXPECT_FALSE(FatFileSystem::looksLikeFat(*d.raum, &w));
    EXPECT_FALSE(w.empty());
}

// ─── Dateien ─────────────────────────────────────────────────────────────────

TEST(FatFs, SchreibenUndLesenInAllenGroessen) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    const size_t groessen[] = {0, 1, 511, 512, 513, 1023, 1024, 1025, 4096, 12345, 100000};
    int nr = 0;
    std::map<std::string, std::vector<uint8_t>> soll;
    for (size_t g : groessen) {
        const std::string name = "F" + std::to_string(nr++) + ".BIN";
        soll[name] = muster(g, static_cast<uint8_t>(g));
        ASSERT_TRUE(d.fat->write(name, soll[name], neu())) << name << ": " << d.fat->lastError();
    }
    d.neuMounten();
    for (const auto& [name, daten] : soll) {
        std::vector<uint8_t> aus;
        ASSERT_TRUE(d.fat->read(name, aus)) << name << ": " << d.fat->lastError();
        EXPECT_EQ(aus, daten) << name;
    }
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund()) << befunde(d.fat->check(FsCheckLevel::Voll, true));
}

TEST(FatFs, UeberschreibenGibtDenAltenPlatzFrei) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    const uint64_t frei0 = d.fat->info().free_bytes;
    ASSERT_TRUE(d.fat->write("A.DAT", muster(50000), neu()));
    const uint64_t frei1 = d.fat->info().free_bytes;
    EXPECT_LT(frei1, frei0);
    WriteOptions o = neu();
    EXPECT_FALSE(d.fat->write("A.DAT", muster(10), o)) << "ohne overwrite";
    o.overwrite = true;
    ASSERT_TRUE(d.fat->write("A.DAT", muster(10), o));
    EXPECT_GT(d.fat->info().free_bytes, frei1);
    std::vector<uint8_t> aus;
    ASSERT_TRUE(d.fat->read("a.dat", aus));         // Gross-/Kleinschreibung egal
    EXPECT_EQ(aus, muster(10));
}

TEST(FatFs, LoeschenGibtClusterFreiUndLoeschtNamenSpur) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    const uint64_t frei0 = d.fat->info().free_bytes;
    ASSERT_TRUE(d.fat->write("X.TXT", muster(30000), neu()));
    ASSERT_TRUE(d.fat->erase("X.TXT"));
    EXPECT_EQ(d.fat->info().free_bytes, frei0);
    std::vector<uint8_t> aus;
    EXPECT_FALSE(d.fat->read("X.TXT", aus));
    EXPECT_FALSE(d.fat->erase("X.TXT"));
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund());
}

TEST(FatFs, EineVolleDisketteIstEineTransaktion) {
    Diskette d("dos_ds40_9x512");
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->write("KLEIN.DAT", muster(1000), neu()));
    const std::vector<uint8_t> vorher = d.abzug();
    // 360K-Diskette fasst ~354 KB: das hier passt nicht
    EXPECT_FALSE(d.fat->write("GROSS.DAT", muster(400000), neu()));
    EXPECT_NE(d.fat->lastError().find("Platz"), std::string::npos) << d.fat->lastError();
    EXPECT_EQ(d.abzug(), vorher) << "ein Fehlschlag darf nichts hinterlassen";
}

TEST(FatFs, DisketteBisZumLetztenClusterFuellen) {
    // Prueft die 12-Bit-Verpackung ueber die ganze FAT: gerade/ungerade Eintraege, letzte Cluster.
    Diskette d("dos_ds40_9x512");
    ASSERT_TRUE(d.fat);
    const uint32_t cb = d.fat->bpb().clusterBytes();
    const uint32_t frei = static_cast<uint32_t>(d.fat->info().free_bytes / cb);
    std::map<std::string, std::vector<uint8_t>> soll;
    uint32_t rest = frei;
    for (int i = 0; rest > 0; ++i) {
        const uint32_t n = std::min<uint32_t>(rest, 37);
        const std::string name = "D" + std::to_string(i) + ".DAT";
        soll[name] = muster(static_cast<size_t>(n) * cb - (i % 3), static_cast<uint8_t>(i));
        ASSERT_TRUE(d.fat->write(name, soll[name], neu())) << name << ": " << d.fat->lastError();
        rest -= n;
    }
    EXPECT_EQ(d.fat->info().free_bytes, 0u);
    d.neuMounten();
    for (const auto& [name, daten] : soll) {
        std::vector<uint8_t> aus;
        ASSERT_TRUE(d.fat->read(name, aus)) << name;
        EXPECT_EQ(aus, daten) << name;
    }
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund());
}

// ─── Verzeichnisse ───────────────────────────────────────────────────────────

TEST(FatFs, UnterverzeichnisseMkdirPUndPfade) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->write("SUB/TIEF/B.BIN", muster(3000), neu()));   // legt die Ordner an
    ASSERT_TRUE(d.fat->makeDirectory("LEER"));
    ASSERT_TRUE(d.fat->makeDirectory("sub"));                           // schon da: kein Fehler
    const auto l = d.fat->list();
    ASSERT_TRUE(finde(l, "SUB"));
    EXPECT_EQ(finde(l, "SUB")->type, "d");
    ASSERT_TRUE(finde(l, "SUB/TIEF"));
    ASSERT_TRUE(finde(l, "SUB/TIEF/B.BIN"));
    EXPECT_EQ(finde(l, "SUB/TIEF/B.BIN")->size, 3000u);
    ASSERT_TRUE(finde(l, "LEER"));
    std::vector<uint8_t> aus;
    ASSERT_TRUE(d.fat->read("sub/tief/b.bin", aus));
    EXPECT_EQ(aus, muster(3000));
    EXPECT_FALSE(d.fat->read("SUB", aus)) << "ein Verzeichnis ist keine Datei";
    EXPECT_FALSE(d.fat->makeDirectory("SUB/TIEF/B.BIN/X")) << "durch eine Datei kein Weg";
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund()) << befunde(d.fat->check(FsCheckLevel::Voll, true));
}

TEST(FatFs, PunkteintraegeStimmen) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->makeDirectory("A/B"));
    // Rohlesen: '.' zeigt auf sich, '..' auf den Elter (0 bei der Wurzel).
    std::vector<uint8_t> w(static_cast<size_t>(d.fat->bpb().root_secs) * d.fat->bpb().bps);
    ASSERT_TRUE(d.raum->read(static_cast<uint64_t>(d.fat->bpb().root_start) * d.fat->bpb().bps, w.data(), w.size()));
    ASSERT_EQ(std::memcmp(w.data(), "A          ", 11) == 0 || std::memcmp(w.data() + 32, "A          ", 11) == 0
                  || w[0] == 'A', true);
    uint16_t ac = 0;
    for (size_t i = 0; i < w.size(); i += 32) if (w[i] == 'A' && w[i + 1] == ' ') ac = static_cast<uint16_t>(w[i + 26] | (w[i + 27] << 8));
    ASSERT_GE(ac, 2);
    std::vector<uint8_t> blk(d.fat->bpb().clusterBytes());
    ASSERT_TRUE(d.raum->read(static_cast<uint64_t>(d.fat->bpb().data_start) * d.fat->bpb().bps
                                 + static_cast<uint64_t>(ac - 2) * d.fat->bpb().clusterBytes(), blk.data(), blk.size()));
    EXPECT_EQ(std::memcmp(blk.data(), ".          ", 11), 0);
    EXPECT_EQ(blk[26] | (blk[27] << 8), ac);
    EXPECT_EQ(std::memcmp(blk.data() + 32, "..         ", 11), 0);
    EXPECT_EQ(blk[32 + 26] | (blk[32 + 27] << 8), 0) << "'..' der obersten Ebene zeigt auf 0";
}

TEST(FatFs, VerzeichnisWaechstUeberEinenClusterHinaus) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->makeDirectory("VIELE"));
    const int n = 100;      // Cluster = 1024 B = 32 Eintraege (2 fuer . ..): mindestens 4 Cluster
    for (int i = 0; i < n; ++i)
        ASSERT_TRUE(d.fat->write("VIELE/F" + std::to_string(i) + ".TXT", muster(10 + i), neu(0))) << i << d.fat->lastError();
    d.neuMounten();
    const auto l = d.fat->list();
    int gezaehlt = 0;
    for (const FileEntry& e : l) if (e.name.rfind("VIELE/", 0) == 0) ++gezaehlt;
    EXPECT_EQ(gezaehlt, n);
    std::vector<uint8_t> aus;
    ASSERT_TRUE(d.fat->read("VIELE/F99.TXT", aus));
    EXPECT_EQ(aus, muster(109));
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund()) << befunde(d.fat->check(FsCheckLevel::Voll, true));
    // alles wieder loeschen, dann das Verzeichnis
    for (int i = 0; i < n; ++i) ASSERT_TRUE(d.fat->erase("VIELE/F" + std::to_string(i) + ".TXT"));
    ASSERT_TRUE(d.fat->erase("VIELE"));
    EXPECT_EQ(d.fat->info().free_bytes, d.fat->info().total_bytes);
}

TEST(FatFs, WurzelIstEndlichUndVollMeldetEs) {
    Diskette d("dos_ss40_8x512");        // 64 Eintraege in der Wurzel
    ASSERT_TRUE(d.fat);
    int geschrieben = 0;
    for (int i = 0; i < 70; ++i) {
        if (!d.fat->write("W" + std::to_string(i) + ".DAT", muster(5), neu(0))) break;
        ++geschrieben;
    }
    // 64 Plaetze, einer davon fuer den Datentraegernamen
    EXPECT_EQ(geschrieben, 63);
    const std::vector<uint8_t> vorher = d.abzug();
    EXPECT_FALSE(d.fat->write("ZUVIEL.DAT", muster(5), neu(0)));
    EXPECT_NE(d.fat->lastError().find("Wurzelverzeichnis"), std::string::npos) << d.fat->lastError();
    EXPECT_EQ(d.abzug(), vorher);
    // Unterverzeichnisse haben die Grenze nicht
    EXPECT_FALSE(d.fat->makeDirectory("NOCH"));            // die Wurzel ist voll
    ASSERT_TRUE(d.fat->erase("W0.DAT"));
    ASSERT_TRUE(d.fat->makeDirectory("NOCH"));
    ASSERT_TRUE(d.fat->write("NOCH/EINS.DAT", muster(5), neu(0)));
}

// ─── Namen ───────────────────────────────────────────────────────────────────

TEST(FatFs, KurznamenUndKleinschreibung) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->write("readme.txt", muster(5), neu()));       // rein klein: NT-Bits, kein langer Name
    ASSERT_TRUE(d.fat->write("DATEI.COM", muster(5), neu()));
    ASSERT_TRUE(d.fat->write("123", muster(5), neu()));
    const auto l = d.fat->list();
    ASSERT_TRUE(finde(l, "readme.txt"));
    EXPECT_TRUE(finde(l, "readme.txt")->short_name.empty()) << "braucht keinen langen Namen";
    ASSERT_TRUE(finde(l, "DATEI.COM"));
    ASSERT_TRUE(finde(l, "123"));
    // ... und ist auch unter anderer Schreibweise zu finden
    std::vector<uint8_t> aus;
    EXPECT_TRUE(d.fat->read("README.TXT", aus));
    EXPECT_FALSE(d.fat->write("Readme.Txt", muster(5), neu())) << "gleicher Name, ohne overwrite";
}

TEST(FatFs, LangeNamenWerdenGeschriebenUndGelesen) {
    Diskette d;                       // OEM MSWIN4.1: lange Namen sind Voreinstellung
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->langeNamenUeblich());
    ASSERT_TRUE(d.fat->write("Eine lange Datei.Text", muster(12), neu()));
    ASSERT_TRUE(d.fat->write("Mixed.Txt", muster(12), neu()));
    ASSERT_TRUE(d.fat->write("Zweite lange Datei.Text", muster(7), neu()));
    ASSERT_TRUE(d.fat->write("\xC3\xA4rger.txt", muster(3), neu()));   // ae-Umlaut (UTF-8)
    d.neuMounten();
    const auto l = d.fat->list();
    const FileEntry* e = finde(l, "Eine lange Datei.Text");
    ASSERT_TRUE(e);
    EXPECT_EQ(e->short_name, "EINELA~1.TEX");
    ASSERT_TRUE(finde(l, "Zweite lange Datei.Text"));
    EXPECT_EQ(finde(l, "Zweite lange Datei.Text")->short_name, "ZWEITE~1.TEX");
    ASSERT_TRUE(finde(l, "Mixed.Txt"));
    EXPECT_EQ(finde(l, "Mixed.Txt")->short_name, "MIXED.TXT");        // passt in 8.3: nur die Schreibung braucht den langen Namen
    ASSERT_TRUE(finde(l, "\xC3\xA4rger.txt"));
    // beide Namen fuehren zur selben Datei
    std::vector<uint8_t> a, b;
    ASSERT_TRUE(d.fat->read("Eine lange Datei.Text", a));
    ASSERT_TRUE(d.fat->read("EINELA~1.TEX", b));
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, muster(12));
    ASSERT_TRUE(d.fat->read("eine LANGE datei.text", a)) << "Gross-/Kleinschreibung egal";
    // Loeschen nimmt auch die Eintraege des langen Namens mit: danach ist die Wurzel wieder wie vorher belegt
    ASSERT_TRUE(d.fat->erase("Eine lange Datei.Text"));
    EXPECT_FALSE(finde(d.fat->list(), "Eine lange Datei.Text"));
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund()) << befunde(d.fat->check(FsCheckLevel::Voll, true));
}

TEST(FatFs, LangeNamenAus_EchterDosKurzname) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->write("Eine lange Datei.Text", muster(12), neu(0)));    // 0 = nie
    const auto l = d.fat->list();
    ASSERT_EQ(l.size(), 1u);
    EXPECT_EQ(l[0].name, "EINELA~1.TEX");
    EXPECT_TRUE(l[0].short_name.empty());
}

TEST(FatFs, PruefsummeDesKurznamens) {
    // Beispiel der Spezifikation: "FOO     BAR" → Summe nach der Rotationsformel
    uint8_t n[11];
    std::memcpy(n, "FOO     BAR", 11);
    uint8_t s = 0;
    for (int i = 0; i < 11; ++i) s = static_cast<uint8_t>(((s & 1) ? 0x80 : 0) + (s >> 1) + n[i]);
    EXPECT_EQ(fat::kurznamenSumme(n), s);
}

TEST(FatFs, VerbotenesWirdAbgewiesen) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    const std::vector<uint8_t> vorher = d.abzug();
    for (const char* name : {"a*b.txt", "x?.dat", "co:n.txt", "a|b", "q\"r", "x<y", "CON", "nul.txt", "PRN.DAT", "aux", "name.", "name "}) {
        EXPECT_FALSE(d.fat->write(name, muster(3), neu())) << name;
    }
    EXPECT_FALSE(d.fat->makeDirectory("a:b"));
    EXPECT_EQ(d.abzug(), vorher);
}

// ─── Attribute und Zeit ──────────────────────────────────────────────────────

TEST(FatFs, DosDatumUndUhrzeit) {
    // 1989-03-31 01:42:22 → Datum = (9<<9)|(3<<5)|31, Zeit = (1<<11)|(42<<5)|11
    const int64_t t = fat::dosZuEpoche(static_cast<uint16_t>((9 << 9) | (3 << 5) | 31),
                                        static_cast<uint16_t>((1 << 11) | (42 << 5) | 11));
    uint16_t d, z;
    fat::epocheZuDos(t, d, z);
    EXPECT_EQ(d, (9 << 9) | (3 << 5) | 31);
    EXPECT_EQ(z, (1 << 11) | (42 << 5) | 11);
    // Schaltjahr
    fat::epocheZuDos(fat::dosZuEpoche(static_cast<uint16_t>((20 << 9) | (2 << 5) | 29), 0), d, z);
    EXPECT_EQ(d, (20 << 9) | (2 << 5) | 29);
    // vor 1980 und nach 2107 werden begrenzt, nicht verbogen
    fat::epocheZuDos(0, d, z);
    EXPECT_EQ(d, 0x0021);
}

TEST(FatFs, AttributeUndZeitstempel) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    WriteOptions o = neu();
    o.fat_mtime = fat::dosZuEpoche(static_cast<uint16_t>((18 << 9) | (6 << 5) | 15), static_cast<uint16_t>((13 << 11) | (30 << 5) | 5));
    ASSERT_TRUE(d.fat->write("T.TXT", muster(10), o));
    FileEntry e = *finde(d.fat->list(), "T.TXT");
    EXPECT_EQ(e.date, "1998-06-15 13:30:10");
    EXPECT_EQ(e.attributes, "ARC");
    FatAttrs a;
    a.set_read_only = true; a.read_only = true;
    a.set_hidden = true;    a.hidden = true;
    a.set_archive = true;   a.archive = false;
    ASSERT_TRUE(d.fat->setAttributes("T.TXT", a));
    e = *finde(d.fat->list(), "T.TXT");
    EXPECT_EQ(e.attributes, "RO HID");
    EXPECT_TRUE(e.hidden);
    // R/O schuetzt vor Ueberschreiben und Loeschen
    WriteOptions ow = neu(); ow.overwrite = true;
    EXPECT_FALSE(d.fat->write("T.TXT", muster(3), ow));
    EXPECT_FALSE(d.fat->erase("T.TXT"));
    a = FatAttrs{};
    a.set_read_only = true; a.read_only = false;
    a.set_mtime = true; a.mtime = fat::dosZuEpoche(static_cast<uint16_t>((30 << 9) | (12 << 5) | 31), static_cast<uint16_t>((23 << 11) | (59 << 5) | 29));
    ASSERT_TRUE(d.fat->setAttributes("T.TXT", a));
    e = *finde(d.fat->list(), "T.TXT");
    EXPECT_EQ(e.date, "2010-12-31 23:59:58");
    ASSERT_TRUE(d.fat->erase("T.TXT"));
}

TEST(FatFs, OverwriteBehaeltAttributeSetztAberArchiv) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    WriteOptions o = neu();
    o.fat_attr = fat::kAttrHid; o.fat_attr_gesetzt = true;
    ASSERT_TRUE(d.fat->write("H.DAT", muster(3), o));
    WriteOptions ow = neu(); ow.overwrite = true;
    ASSERT_TRUE(d.fat->write("H.DAT", muster(4), ow));
    EXPECT_EQ(finde(d.fat->list(), "H.DAT")->attributes, "HID ARC");
}

// ─── Platzpruefung ───────────────────────────────────────────────────────────

TEST(FatFs, WouldFitRechnetClusterUndVerzeichnisse) {
    Diskette d("dos_ds40_9x512");
    ASSERT_TRUE(d.fat);
    const uint32_t cb = d.fat->bpb().clusterBytes();
    PlannedFile p; p.name = "A.DAT"; p.size = 3 * cb + 1;
    FitReport r;
    ASSERT_TRUE(d.fat->wouldFit({p}, r));
    EXPECT_TRUE(r.fits);
    EXPECT_EQ(r.needed, 4u * cb);
    PlannedFile q; q.name = "ORDNER/B.DAT"; q.size = 1;
    ASSERT_TRUE(d.fat->wouldFit({q}, r));
    EXPECT_EQ(r.needed, 2u * cb) << "ein Cluster fuer den Ordner, einer fuer die Datei";
    PlannedFile gross; gross.name = "G.DAT"; gross.size = d.fat->info().free_bytes + 1;
    ASSERT_TRUE(d.fat->wouldFit({gross}, r));
    EXPECT_FALSE(r.fits);
    EXPECT_FALSE(r.detail.empty());
}

// ─── Pruefung und Reparatur ──────────────────────────────────────────────────

TEST(FatFs, PruefungFindetFatKopieMitAbweichung) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->write("A.DAT", muster(3000), neu()));
    // FAT 2 zerstoeren
    const fat::Bpb& b = d.fat->bpb();
    std::vector<uint8_t> z(b.bps, 0x5A);
    ASSERT_TRUE(d.raum->write(static_cast<uint64_t>(b.fat_start + b.spf) * b.bps, z.data(), z.size()));
    d.neuMounten();
    FsCheckReport r = d.fat->check(FsCheckLevel::Schnell, true);
    ASSERT_GE(r.zaehlerAb(FsSeverity::Fehler), 1) << befunde(r);
    const FsFinding* f = nullptr;
    for (const FsFinding& x : r.findings) if (x.id == "fat.fat.abweichend") f = &x;
    ASSERT_TRUE(f) << befunde(r);
    ASSERT_FALSE(f->repairs.empty());
    ASSERT_TRUE(d.fat->repair(f->repairs[0]));       // FAT 1 auf alle Kopien
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund()) << befunde(d.fat->check(FsCheckLevel::Voll, true));
}

TEST(FatFs, VerloreneClusterWerdenGefundenUndGerettet) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->write("A.DAT", muster(5000), neu()));
    ASSERT_TRUE(d.fat->write("B.DAT", muster(2500), neu()));
    // B.DAT aus dem Verzeichnis loeschen, ohne die FAT freizugeben: Wurzeleintrag mit 0xE5 markieren
    const fat::Bpb& b = d.fat->bpb();
    std::vector<uint8_t> w(512);
    const uint64_t off = static_cast<uint64_t>(b.root_start) * b.bps;
    ASSERT_TRUE(d.raum->read(off, w.data(), w.size()));
    int slot = -1;
    for (int i = 0; i < 16; ++i) if (std::memcmp(&w[i * 32], "B       DAT", 11) == 0) slot = i;
    ASSERT_GE(slot, 0);
    w[slot * 32] = 0xE5;
    ASSERT_TRUE(d.raum->write(off, w.data(), w.size()));
    d.neuMounten();
    const FsCheckReport r = d.fat->check(FsCheckLevel::Voll, true);
    const FsFinding* f = nullptr;
    for (const FsFinding& x : r.findings) if (x.id == "fat.cluster.verloren") f = &x;
    ASSERT_TRUE(f) << befunde(r);
    FsRepair retten;
    for (const FsRepair& x : f->repairs) if (x.kind == "fat.verloren.retten") retten = x;
    ASSERT_EQ(retten.kind, "fat.verloren.retten");
    ASSERT_TRUE(d.fat->repair(retten)) << d.fat->lastError();
    d.neuMounten();
    const auto l = d.fat->list();
    const FileEntry* chk = finde(l, "FILE0000.CHK");
    ASSERT_TRUE(chk) << "gerettete Kette";
    std::vector<uint8_t> aus;
    ASSERT_TRUE(d.fat->read("FILE0000.CHK", aus));
    EXPECT_EQ(std::vector<uint8_t>(aus.begin(), aus.begin() + 2500), muster(2500));
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund()) << befunde(d.fat->check(FsCheckLevel::Voll, true));
}

TEST(FatFs, QuerverweisWirdGetrenntOhneDatenverlust) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->write("EINS.DAT", muster(4000, 10), neu()));
    ASSERT_TRUE(d.fat->write("ZWEI.DAT", muster(4000, 99), neu()));
    // Startcluster von ZWEI.DAT auf den von EINS.DAT biegen (klassischer Kreuzverweis)
    const fat::Bpb& b = d.fat->bpb();
    std::vector<uint8_t> w(512);
    const uint64_t off = static_cast<uint64_t>(b.root_start) * b.bps;
    ASSERT_TRUE(d.raum->read(off, w.data(), w.size()));
    int e1 = -1, e2 = -1;
    for (int i = 0; i < 16; ++i) {
        if (std::memcmp(&w[i * 32], "EINS    DAT", 11) == 0) e1 = i;
        if (std::memcmp(&w[i * 32], "ZWEI    DAT", 11) == 0) e2 = i;
    }
    ASSERT_GE(e1, 0); ASSERT_GE(e2, 0);
    std::memcpy(&w[e2 * 32 + 26], &w[e1 * 32 + 26], 2);
    ASSERT_TRUE(d.raum->write(off, w.data(), w.size()));
    d.neuMounten();
    const FsCheckReport r = d.fat->check(FsCheckLevel::Voll, true);
    const FsFinding* f = nullptr;
    for (const FsFinding& x : r.findings) if (x.id == "fat.cluster.doppelt") f = &x;
    ASSERT_TRUE(f) << befunde(r);
    EXPECT_EQ(f->severity, FsSeverity::Gefahr);
    ASSERT_FALSE(f->repairs.empty());
    EXPECT_FALSE(f->repairs[0].datenverlust);
    ASSERT_TRUE(d.fat->repair(f->repairs[0])) << d.fat->lastError();
    d.neuMounten();
    std::vector<uint8_t> a, c;
    ASSERT_TRUE(d.fat->read("EINS.DAT", a));
    ASSERT_TRUE(d.fat->read("ZWEI.DAT", c));
    EXPECT_EQ(a, muster(4000, 10));
    EXPECT_EQ(c, muster(4000, 10)) << "ZWEI zeigte auf die Cluster von EINS: es bekommt jetzt EINE EIGENE KOPIE davon";
    const FsCheckReport nach = d.fat->check(FsCheckLevel::Voll, true);
    EXPECT_EQ(nach.zaehlerAb(FsSeverity::Fehler), 0) << befunde(nach);
}

TEST(FatFs, GroessenFehlerWerdenGemeldetUndAngeglichen) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->write("A.DAT", muster(2000), neu()));
    const fat::Bpb& b = d.fat->bpb();
    std::vector<uint8_t> w(512);
    const uint64_t off = static_cast<uint64_t>(b.root_start) * b.bps;
    ASSERT_TRUE(d.raum->read(off, w.data(), w.size()));
    int e = -1;
    for (int i = 0; i < 16; ++i) if (std::memcmp(&w[i * 32], "A       DAT", 11) == 0) e = i;
    ASSERT_GE(e, 0);
    w[e * 32 + 28] = 0xFF; w[e * 32 + 29] = 0xFF;      // Groesse 65535: die Kette (2 Cluster) fasst nur 2048
    ASSERT_TRUE(d.raum->write(off, w.data(), w.size()));
    d.neuMounten();
    const FsCheckReport r = d.fat->check(FsCheckLevel::Voll, true);
    const FsFinding* f = nullptr;
    for (const FsFinding& x : r.findings) if (x.id == "fat.groesse.zukurz") f = &x;
    ASSERT_TRUE(f) << befunde(r);
    ASSERT_TRUE(d.fat->repair(f->repairs[0]));
    EXPECT_EQ(finde(d.fat->list(), "A.DAT")->size, 2048u);
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund());
}

// ─── Bootsektor ──────────────────────────────────────────────────────────────

TEST(FatFs, EingebauterBootsektorMeldetKeineSystemdiskette) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    std::vector<uint8_t> b;
    ASSERT_TRUE(d.fat->readBootSector(b));
    EXPECT_EQ(b[0], 0xEB);
    EXPECT_EQ(b[2], 0x90);
    EXPECT_EQ(FatFileSystem::programmAnfang(b), 0x3E);
    const std::string s(reinterpret_cast<char*>(b.data()), b.size());
    EXPECT_NE(s.find("Non-System disk or disk error"), std::string::npos);
    EXPECT_EQ(b[38], 0x29);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(&b[54]), 8), "FAT12   ");
}

TEST(FatFs, BootsektorMitBpbBehaeltCodeUndSetztDenBpbDerDiskette) {
    // Ein DCP-artiger Sektor: Sprung auf 0x36, Code ab 0x36, der BPB steht davor
    std::vector<uint8_t> bild(512, 0);
    bild[0] = 0xEB; bild[1] = 0x34; bild[2] = 0x90;
    std::memcpy(&bild[3], "DCP  3.3", 8);
    for (int i = 0x36; i < 0x1E0; ++i) bild[static_cast<size_t>(i)] = static_cast<uint8_t>(i * 13);
    bild[510] = 0x55; bild[511] = 0xAA;
    Diskette d("dos_ds40_9x512");
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->writeBootSector(bild)) << d.fat->lastError();
    std::vector<uint8_t> aus;
    ASSERT_TRUE(d.fat->readBootSector(aus));
    EXPECT_EQ(std::memcmp(&aus[0x36], &bild[0x36], 0x1E0 - 0x36), 0) << "der Code bleibt";
    EXPECT_EQ(std::string(reinterpret_cast<char*>(&aus[3]), 8), "DCP  3.3");
    const fat::Bpb& b = d.fat->bpb();
    EXPECT_EQ(aus[11] | (aus[12] << 8), b.bps);
    EXPECT_EQ(aus[13], b.spc);
    EXPECT_EQ(aus[21], b.media);
    EXPECT_EQ(aus[22] | (aus[23] << 8), b.spf);
    EXPECT_EQ(aus[24] | (aus[25] << 8), b.spt);
    EXPECT_EQ(aus[26] | (aus[27] << 8), b.heads);
    d.neuMounten();
    EXPECT_EQ(d.fat->bpb().oem, "DCP  3.3");
    EXPECT_FALSE(d.fat->langeNamenUeblich()) << "ein DCP-DOS kennt keine langen Namen: Kurznamen schreiben";
    EXPECT_TRUE(d.fat->check(FsCheckLevel::Voll, true).ohneBefund());
}

TEST(FatFs, BootsektorMitBpbLehntFremdesAb) {
    Diskette d;
    ASSERT_TRUE(d.fat);
    std::vector<uint8_t> aus, kurz(100, 0), ohneKennung(512, 0), ohneSprung(512, 0);
    ohneKennung[0] = 0xEB; ohneKennung[1] = 0x3C;
    ohneSprung[510] = 0x55; ohneSprung[511] = 0xAA;
    std::string err;
    EXPECT_FALSE(FatFileSystem::bootsektorMitBpb(kurz, d.fat->bpb(), {}, aus, err));
    EXPECT_FALSE(FatFileSystem::bootsektorMitBpb(ohneKennung, d.fat->bpb(), {}, aus, err));
    EXPECT_FALSE(FatFileSystem::bootsektorMitBpb(ohneSprung, d.fat->bpb(), {}, aus, err));
    EXPECT_FALSE(d.fat->writeBootSector(ohneSprung));
}

TEST(FatFs, LeseBpbloseDisketteAusDerMedienkennung) {
    // DOS 1.x/2.0: kein BPB im Bootsektor, nur die Medienkennung der FAT — 360K-Diskette
    Diskette d("dos_ds40_9x512");
    ASSERT_TRUE(d.fat);
    ASSERT_TRUE(d.fat->write("ALT.TXT", muster(700), neu(0)));
    std::vector<uint8_t> b;
    ASSERT_TRUE(d.fat->readBootSector(b));
    std::fill(b.begin() + 11, b.begin() + 62, 0);          // BPB und Namen weg
    b[0] = 0xEB; b[1] = 0x1C; b[2] = 0x90;
    ASSERT_TRUE(d.raum->write(0, b.data(), 512));
    std::string w;
    ASSERT_TRUE(FatFileSystem::looksLikeFat(*d.raum, &w)) << w;
    d.neuMounten();
    EXPECT_FALSE(d.fat->bpb().mit_bpb);
    std::vector<uint8_t> aus;
    ASSERT_TRUE(d.fat->read("ALT.TXT", aus));
    EXPECT_EQ(aus, muster(700));
}

TEST(FatFs, DatentraegernameSetzenUndLesen) {
    Diskette d("dos_ds80_9x512", "ERSTER");
    ASSERT_TRUE(d.fat);
    EXPECT_EQ(d.fat->label(), "ERSTER");
    ASSERT_TRUE(d.fat->setLabel("zweiter"));
    EXPECT_EQ(d.fat->label(), "ZWEITER");
    EXPECT_FALSE(d.fat->setLabel("ZU LANGER NAME"));
    EXPECT_FALSE(d.fat->setLabel("A*B"));
    ASSERT_TRUE(d.fat->setLabel(""));
    EXPECT_EQ(d.fat->label(), "");
    EXPECT_EQ(d.fat->info().files, 0);
}

// ─── DiskVolume: Erkennung, Container, SYS ───────────────────────────────────

namespace {
std::unique_ptr<DiskVolume> volume(const std::string& pfad, std::string& err, bool ro = true) {
    return DiskVolume::open(pfad, "", formate(), dateisysteme(), err, ro);
}
}  // namespace

TEST(FatDiskVolume, AnlegenUndErkennenInAllenContainern) {
    struct F { const char* profil; const char* endung; };
    const F faelle[] = {{"dos_ds80_9x512", "img"}, {"dos_ds80_9x512", "hfe"}, {"dos_ds80_9x512", "dmk"}, {"dos_ds40_9x512", "img"}, {"dos_ds40_9x512", "hfe"},
                        {"dos_ds40_8x512", "img"}, {"dos_ds80_8x512", "img"}, {"dos_ds80_10x512", "img"}, {"dos_ss40_8x512", "img"}, {"dos_ss40_9x512", "hfe"},
                        {"dos_ds80_18x512_hd", "img"}, {"dos_ds80_15x512_hd", "img"}, {"dos_ds80_36x512_ed", "img"}, {"dos_ss80_9x512", "img"}};
    for (const F& f : faelle) {
        const std::string pfad = k1520test::tempPath(std::string("k1520_test_fatvol_") + f.profil + "." + f.endung);
        std::string err;
        auto v = DiskVolume::create(pfad, f.profil, "VOLTEST", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << f.profil << "." << f.endung << ": " << err;
        const std::vector<uint8_t> daten = muster(7777);
        const std::string quelle = k1520test::tempPath("k1520_test_fatvol_quelle.bin");
        { std::ofstream o(quelle, std::ios::binary); o.write(reinterpret_cast<const char*>(daten.data()), static_cast<std::streamsize>(daten.size())); }
        TransferOptions t; t.overwrite = true;
        ASSERT_TRUE(v->insert(quelle, FileRef{0, "ORDNER/Eine lange Datei.bin"}, t)) << v->lastError();
        ASSERT_TRUE(v->flush());
        v.reset();
        auto w = volume(pfad, err);
        ASSERT_TRUE(w) << f.profil << "." << f.endung << ": " << err;
        EXPECT_EQ(w->profile().name, f.profil) << f.endung;
        EXPECT_TRUE(w->istFat());
        bool gefunden = false;
        for (const FileEntry& e : w->list()) if (e.name == "ORDNER/Eine lange Datei.bin") { gefunden = true; EXPECT_EQ(e.size, 7777u); }
        EXPECT_TRUE(gefunden) << f.profil << "." << f.endung;
        const std::string aus = k1520test::tempPath("k1520_test_fatvol_aus.bin");
        TransferOptions tx; tx.overwrite = true;
        ASSERT_TRUE(w->extract(FileRef{0, "ORDNER/Eine lange Datei.bin"}, aus, tx)) << w->lastError();
        std::ifstream i(aus, std::ios::binary);
        const std::vector<uint8_t> gelesen((std::istreambuf_iterator<char>(i)), {});
        EXPECT_EQ(gelesen, daten) << f.profil << "." << f.endung;
        w.reset();
        std::error_code ec;
        fs::remove(pfad, ec); fs::remove(quelle, ec); fs::remove(aus, ec);
    }
}

TEST(FatDiskVolume, ZweiGeometrienGleicherGroesseUnterscheidetDerBpb) {
    // 360K = 40x2x9 ODER 80x1x9: beide .img sind gleich gross, der BPB (Koepfe) entscheidet.
    for (const char* profil : {"dos_ds40_9x512", "dos_ss80_9x512", "dos_ds40_8x512", "dos_ds80_8x512", "dos_ds80_10x512"}) {
        const std::string pfad = k1520test::tempPath(std::string("k1520_test_fatvol_gleich_") + profil + ".img");
        std::string err;
        { auto v = DiskVolume::create(pfad, profil, "", formate(), dateisysteme(), err); ASSERT_TRUE(v) << profil << err; }
        auto w = volume(pfad, err);
        ASSERT_TRUE(w) << profil << ": " << err;
        EXPECT_EQ(w->profile().name, profil);
        std::error_code ec; fs::remove(pfad, ec);
    }
}

// ── Hochdichte (Entwurf 45, AP H1): 500 kbit/s in allen Containern ────────────────────────────

/// Kopfbytes 0x0C/0x0D (Bitrate) und 0x0E/0x0F (rpm) einer HFE-Datei.
static void hfeKopf(const std::string& pfad, unsigned& rate, unsigned& rpm, unsigned& spuren) {
    std::ifstream i(pfad, std::ios::binary);
    std::vector<uint8_t> h(32);
    i.read(reinterpret_cast<char*>(h.data()), 32);
    rate   = h[0x0C] | (h[0x0D] << 8);
    rpm    = h[0x0E] | (h[0x0F] << 8);
    spuren = h[0x09];
}

TEST(FatDiskVolume, HdRundlaufInAllenContainern) {
    struct F { const char* profil; unsigned rate_soll; unsigned rpm_soll; };
    const F profile[] = {{"dos_ds80_18x512_hd", 500, 300}, {"dos_ds80_15x512_hd", 500, 360}};
    for (const F& f : profile)
        for (const char* ext : {"img", "hfe", "dmk", "imd", "td0", "cqm"}) {
            const std::string was = std::string(f.profil) + "." + ext;
            const std::string pfad = k1520test::tempPath("k1520_test_hd_" + was);
            std::string err;
            auto v = DiskVolume::create(pfad, f.profil, "HDTEST", formate(), dateisysteme(), err);
            ASSERT_TRUE(v) << was << ": " << err;
            const std::vector<uint8_t> daten = muster(40000);
            const std::string quelle = k1520test::tempPath("k1520_test_hd_quelle.bin");
            { std::ofstream o(quelle, std::ios::binary); o.write(reinterpret_cast<const char*>(daten.data()), static_cast<std::streamsize>(daten.size())); }
            TransferOptions t; t.overwrite = true;
            ASSERT_TRUE(v->insert(quelle, FileRef{0, "GROSS.BIN"}, t)) << was << ": " << v->lastError();
            ASSERT_TRUE(v->flush()) << was;
            v.reset();

            auto w = volume(pfad, err);
            ASSERT_TRUE(w) << was << ": " << err;
            EXPECT_EQ(w->profile().name, f.profil) << was;
            EXPECT_TRUE(w->istFat()) << was;
            const std::string aus = k1520test::tempPath("k1520_test_hd_aus.bin");
            TransferOptions tx; tx.overwrite = true;
            ASSERT_TRUE(w->extract(FileRef{0, "GROSS.BIN"}, aus, tx)) << was << ": " << w->lastError();
            std::ifstream i(aus, std::ios::binary);
            const std::vector<uint8_t> gelesen((std::istreambuf_iterator<char>(i)), {});
            EXPECT_EQ(gelesen, daten) << was;
            w.reset();

            // Das neu geoeffnete Medium traegt die Hochdichte-Zeitbasis (ausser .img: dort aus dem Format).
            std::string e2;
            auto img = DiskImage::open(pfad, *formate().find(dateisysteme().find(f.profil)->format), true);
            if (img) {
                EXPECT_EQ(img->medium().nominalRateKbps(), f.rate_soll) << was;
                EXPECT_EQ(img->medium().nominalRpm(), f.rpm_soll) << was;
            }
            if (std::string(ext) == "hfe") {
                unsigned rate = 0, rpm = 0, n = 0;
                hfeKopf(pfad, rate, rpm, n);
                EXPECT_EQ(rate, f.rate_soll) << "HFE-Kopf: Bitrate";
                EXPECT_EQ(rpm, f.rpm_soll) << "HFE-Kopf: rpm";
                // Zellen je Umdrehung = Rate x 2 x 60 000 / rpm  (1,44M: 200 000; 1,2M: 166 667)
                const size_t zellen = (static_cast<size_t>(f.rate_soll) * 120000 + f.rpm_soll / 2) / f.rpm_soll;
                std::ifstream h(pfad, std::ios::binary);
                std::vector<uint8_t> kopf(1024);
                h.read(reinterpret_cast<char*>(kopf.data()), 1024);
                const unsigned len = kopf[512 + 2] | (kopf[512 + 3] << 8);   // LUT-Eintrag Zylinder 0
                EXPECT_EQ(len / 2, (zellen / 8 + 255) / 256 * 256) << "Seitenlaenge in Byte";
            }
            std::error_code ec;
            fs::remove(pfad, ec); fs::remove(quelle, ec); fs::remove(aus, ec);
        }
}

TEST(FatDiskVolume, EdBleibtNurImg) {
    std::string err;
    EXPECT_FALSE(DiskVolume::create(k1520test::tempPath("k1520_test_fat_ed.hfe"), "dos_ds80_36x512_ed", "", formate(), dateisysteme(), err));
    EXPECT_FALSE(DiskVolume::create(k1520test::tempPath("k1520_test_fat_ed.cqm"), "dos_ds80_36x512_ed", "", formate(), dateisysteme(), err));
}

TEST(FatDiskVolume, HdSpurHatDieHochdichteUmdrehung) {
    for (auto [profil, bytes] : {std::pair<const char*, size_t>{"dos_ds80_18x512_hd", 12500}, {"dos_ds80_15x512_hd", 10416}, {"dos_ds80_9x512", 6250}}) {
        const DiskFormat* f = formate().find(dateisysteme().find(profil)->format);
        const std::string pfad = k1520test::tempPath(std::string("k1520_test_hd_umdr_") + profil + ".img");
        std::string grund;
        auto img = DiskImage::create(pfad, *f, false, f->predominantEncoding(), &grund);
        ASSERT_TRUE(img) << grund;
        EXPECT_EQ(img->medium().track(0, 0).bytes.size(), bytes) << profil;
        EXPECT_EQ(img->medium().track(79, 1).bytes.size(), bytes) << profil;
        img.reset();
        std::error_code ec; fs::remove(pfad, ec);
    }
}

TEST(FatDiskVolume, OrdnerEinfuegenUndAuspackenRundlauf) {
    const fs::path wurzel = fs::path(k1520test::tempPath("k1520_test_fat_ordner"));
    std::error_code ec;
    fs::remove_all(wurzel, ec);
    fs::create_directories(wurzel / "quelle" / "sub" / "tief");
    fs::create_directories(wurzel / "quelle" / "leer");
    auto schreibe = [&](const fs::path& p, const std::vector<uint8_t>& d) {
        std::ofstream o(p, std::ios::binary); o.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
    };
    schreibe(wurzel / "quelle" / "gross.bin", muster(5000, 3));
    schreibe(wurzel / "quelle" / "sub" / "a.dat", muster(2, 4));
    schreibe(wurzel / "quelle" / "sub" / "tief" / "b.bin", muster(3000, 5));
    schreibe(wurzel / "quelle" / "Lang Name mit Leerzeichen.TXT", muster(77, 6));

    const std::string img = (wurzel / "t.img").string();
    std::string err;
    auto v = DiskVolume::create(img, "dos_ds80_9x512", "RUNDLAUF", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    TransferOptions t;
    ASSERT_TRUE(v->insertAll((wurzel / "quelle").string(), t)) << v->lastError();
    ASSERT_TRUE(v->flush());
    v.reset();

    auto w = volume(img, err);
    ASSERT_TRUE(w) << err;
    EXPECT_TRUE(w->check(FsCheckLevel::Voll, true).ohneBefund());
    TransferOptions tx; tx.overwrite = true;
    ASSERT_TRUE(w->extractAll((wurzel / "aus").string(), tx)) << w->lastError();
    for (const char* rel : {"gross.bin", "sub/a.dat", "sub/tief/b.bin", "Lang Name mit Leerzeichen.TXT"}) {
        std::ifstream a(wurzel / "quelle" / rel, std::ios::binary), b(wurzel / "aus" / rel, std::ios::binary);
        ASSERT_TRUE(b) << rel;
        const std::vector<uint8_t> x((std::istreambuf_iterator<char>(a)), {}), y((std::istreambuf_iterator<char>(b)), {});
        EXPECT_EQ(x, y) << rel;
    }
    EXPECT_TRUE(fs::is_directory(wurzel / "aus" / "leer")) << "auch leere Ordner kommen mit";
    w.reset();
    fs::remove_all(wurzel, ec);
}

TEST(FatDiskVolume, SysUebertraegtBootsektorUndSystemdateienInReihenfolge) {
    const std::string quelle = k1520test::tempPath("k1520_test_fat_sys_quelle.img");
    const std::string ziel = k1520test::tempPath("k1520_test_fat_sys_ziel.img");
    std::string err;
    {
        auto q = DiskVolume::create(quelle, "dos_ds80_9x512", "SYSQUELLE", formate(), dateisysteme(), err);
        ASSERT_TRUE(q) << err;
        // ein DOS-artiger Bootsektor (Sprung 0x36, Name 'DCP  3.3')
        std::vector<uint8_t> bild(512, 0);
        bild[0] = 0xEB; bild[1] = 0x34; bild[2] = 0x90;
        std::memcpy(&bild[3], "DCP  3.3", 8);
        for (int i = 0x36; i < 0x1E0; ++i) bild[static_cast<size_t>(i)] = static_cast<uint8_t>(i * 7);
        bild[510] = 0x55; bild[511] = 0xAA;
        ASSERT_TRUE(q->writeBootImage(bild)) << q->lastError();
        ASSERT_TRUE(q->flush()) << q->lastError();
    }
    // Systemdateien und eine normale Datei einspielen (ueber die Kernschnittstelle des Dateisystems)
    {
        const FsProfile* p = dateisysteme().find("dos_ds80_9x512");
        const DiskFormat* fm = formate().find(p->format);
        auto img = DiskImage::open(quelle, *fm, false);
        ASSERT_TRUE(img);
        SectorSpace raum(img->medium(), *fm);
        std::string w;
        auto fat = FatFileSystem::mount(raum, *p, w);
        ASSERT_TRUE(fat) << w;
        WriteOptions o; o.fat_lfn = 0; o.fat_attr = fat::kAttrRo | fat::kAttrHid | fat::kAttrSys | fat::kAttrArc; o.fat_attr_gesetzt = true;
        ASSERT_TRUE(fat->write("IO.SYS", muster(9000, 1), o));
        ASSERT_TRUE(fat->write("DCDOS.SYS", muster(20000, 2), o));
        WriteOptions n; n.fat_lfn = 0;
        ASSERT_TRUE(fat->write("COMMAND.COM", muster(300, 3), n));
        ASSERT_TRUE(img->flush());
    }
    {
        auto z = DiskVolume::create(ziel, "dos_ds40_9x512", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(z) << err;
        auto q = volume(quelle, err);
        ASSERT_TRUE(q) << err;
        std::string bericht;
        ASSERT_TRUE(z->dosSystemUebertragen(*q, bericht)) << z->lastError();
        EXPECT_EQ(bericht.find("WARNUNG"), std::string::npos) << bericht;
        ASSERT_TRUE(z->flush());
        // zweiter Versuch: das Ziel ist nicht mehr leer
        std::string b2;
        EXPECT_FALSE(z->dosSystemUebertragen(*q, b2));
    }
    auto z = volume(ziel, err);
    ASSERT_TRUE(z) << err;
    const auto l = z->list();
    ASSERT_EQ(l.size(), 2u) << "COMMAND.COM ist keine Systemdatei";
    EXPECT_EQ(l[0].name, "IO.SYS");
    EXPECT_EQ(l[1].name, "DCDOS.SYS");
    EXPECT_EQ(l[0].attributes, "RO HID SYS ARC");
    std::vector<uint8_t> b;
    ASSERT_TRUE(z->readBootImage(b));
    EXPECT_EQ(std::string(reinterpret_cast<char*>(&b[3]), 8), "DCP  3.3");
    EXPECT_EQ(b[21], 0xFD) << "der BPB ist der der 360K-Zieldiskette";
    EXPECT_EQ(b[0x100], static_cast<uint8_t>(0x100 * 7)) << "der Code der Quelle";
    EXPECT_TRUE(z->check(FsCheckLevel::Voll, true).ohneBefund());
    z.reset();
    std::error_code ec; fs::remove(quelle, ec); fs::remove(ziel, ec);
}

TEST(FatDiskVolume, SysOhneSystemdateienUndNichtFatWirdAbgewiesen) {
    const std::string a = k1520test::tempPath("k1520_test_fat_sys_a.img"), b = k1520test::tempPath("k1520_test_fat_sys_b.img");
    std::string err;
    auto z = DiskVolume::create(a, "dos_ds80_9x512", "", formate(), dateisysteme(), err);
    auto q = DiskVolume::create(b, "dos_ds80_9x512", "", formate(), dateisysteme(), err);
    ASSERT_TRUE(z && q) << err;
    std::string bericht;
    EXPECT_FALSE(z->dosSystemUebertragen(*q, bericht));
    EXPECT_NE(z->lastError().find("Systemdiskette"), std::string::npos) << z->lastError();
    std::error_code ec; z.reset(); q.reset(); fs::remove(a, ec); fs::remove(b, ec);
}

// ─── die echten Disketten ────────────────────────────────────────────────────

TEST(FatLieferdisketten, A7150UndPc1834WerdenErkanntUndPruefenOhneFehler) {
    const std::string ordner = dosOrdner();
    if (ordner.empty() || !fs::is_directory(ordner)) GTEST_SKIP() << "Ordner mit den DCP-Disketten fehlt (K1520_DOS_DISKETTEN)";
    int gesehen = 0;
    struct Soll { const char* datei; const char* oem; int dateien_min; };
    const Soll soll[] = {
        {"A7150_DCP33.hfe", "DCP  3.3", 50}, {"A7150_DCPGX.hfe", "DCP 3.20", 10}, {"A7150_BAS.hfe", "MDISK", 10},
        {"A7150_DCP_SCPDRV.hfe", "DCP  3.3", 3}, {"A7150_MASM.hfe", "DCP  3.2", 5}, {"A7150_SIBE.hfe", "DCP 3.20", 5},
        {"1834_DCP33.hfe", "DCP  3.3", 10}, {"1834_TP.hfe", "DCP  3.3", 5},
    };
    for (const Soll& s : soll) {
        const fs::path p = fs::path(ordner) / s.datei;
        if (!fs::exists(p)) continue;
        ++gesehen;
        std::string err;
        auto v = volume(p.string(), err);
        ASSERT_TRUE(v) << s.datei << ": " << err;
        EXPECT_EQ(v->profile().name, "dos_ds80_9x512") << s.datei;
        std::vector<uint8_t> boot;
        ASSERT_TRUE(v->readBootImage(boot));
        EXPECT_EQ(std::string(reinterpret_cast<char*>(&boot[3]), std::strlen(s.oem)), s.oem) << s.datei;
        int dateien = 0;
        for (const FileEntry& e : v->list()) if (e.type != "d") ++dateien;
        EXPECT_GE(dateien, s.dateien_min) << s.datei;
        const FsCheckReport r = v->check(FsCheckLevel::Voll, true);
        EXPECT_EQ(r.zaehlerAb(FsSeverity::Fehler), 0) << s.datei << "\n" << befunde(r);
    }
    if (gesehen == 0) GTEST_SKIP() << "keine der erwarteten Dateien im Ordner";
}

TEST(FatLieferdisketten, SystemdisketteLaesstSichUebertragen) {
    const std::string ordner = dosOrdner();
    const fs::path q = fs::path(ordner) / "A7150_DCP33.hfe";
    if (!fs::exists(q)) GTEST_SKIP() << "A7150_DCP33.hfe fehlt";
    const std::string ziel = k1520test::tempPath("k1520_test_fat_dcp_sys.img");
    std::string err;
    auto z = DiskVolume::create(ziel, "dos_ds80_9x512", "", formate(), dateisysteme(), err);
    ASSERT_TRUE(z) << err;
    auto src = volume(q.string(), err);
    ASSERT_TRUE(src) << err;
    std::string bericht;
    ASSERT_TRUE(z->dosSystemUebertragen(*src, bericht)) << z->lastError();
    EXPECT_EQ(bericht.find("WARNUNG"), std::string::npos) << bericht;
    std::vector<uint8_t> boot_neu, boot_alt;
    ASSERT_TRUE(z->readBootImage(boot_neu));
    ASSERT_TRUE(src->readBootImage(boot_alt));
    EXPECT_EQ(boot_neu, boot_alt) << "gleiche Groesse, gleicher BPB: der Sektor ist byteweise der Originalsektor";
    std::error_code ec; z.reset(); fs::remove(ziel, ec);
}
