/**
 * @file test_tar_mutos.cpp
 * @brief GoogleTests fuer tar-Disketten und tar-Saetze im MUTOS-8000-Layout
 *        (Profile tar_mutos8k_26x128_mfm / tar_mutos8k_26x128_fm, Entwurf 39 Teil A, Merkposten tar_satz.md Nr. 17).
 *
 * Das V7-`tar` des MUTOS schreibt seinen Strom auf das Rohgeraet /dev/rrkN: 512-B-Bloecke ab
 * Zylinder 1, Verschraenkung 2, Spurversatz 6, 494 Bloecke je Diskette.  Die Abbildung steht hier
 * ein zweites Mal (`sektorVon`), UNABHAENGIG von `Mutos8kDev`: stimmte nur die Klasse mit sich selbst
 * ueberein, bliebe ein Fehler in der Formel unbemerkt.  Die Fixtures `mutos8000_tar_*.dmk` hat MUTOS
 * selbst im Emulator geschrieben (`tar cvf /dev/rrk0 ...`, Satz ueber `dd`).
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/filesystem/tar/tar_archiv.h"
#include "tests/support/fixtures.h"
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

std::vector<uint8_t> leseDatei(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void schreibeDatei(const std::string& p, const std::vector<uint8_t>& d) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
}

std::vector<uint8_t> muster(size_t n, uint32_t start) {
    std::vector<uint8_t> d(n);
    uint32_t x = start * 2654435761u + 1;
    for (auto& b : d) { x = x * 1664525u + 1013904223u; b = static_cast<uint8_t>(x >> 24); }
    return d;
}

struct Ordner {
    std::string pfad;
    explicit Ordner(const std::string& name) : pfad(k1520test::tempPath(name)) {
        fs::remove_all(pfad);
        fs::create_directories(pfad);
    }
    ~Ordner() { std::error_code ec; fs::remove_all(pfad, ec); }
    std::string operator/(const std::string& n) const { return pfad + "/" + n; }
};

std::string quelle(const Ordner& o, const std::string& name, const std::vector<uint8_t>& d) {
    fs::create_directories(o / "q");
    const std::string p = o / ("q/" + name);
    schreibeDatei(p, d);
    return p;
}

bool einfuegen(DiskVolume& v, const std::string& pfad, const std::string& ziel) {
    TransferOptions t;
    t.overwrite = true;
    return v.insert(pfad, FileRef::parse(ziel, 0), t);
}

std::vector<uint8_t> inhalt(DiskVolume& v, const std::string& name) {
    const std::string tmp = k1520test::tempPath("tarmutos_get.bin");
    TransferOptions t;
    t.overwrite = true;
    EXPECT_TRUE(v.extract(FileRef::parse(name, 0), tmp, t)) << v.lastError();
    auto d = leseDatei(tmp);
    fs::remove(tmp);
    return d;
}

std::vector<std::string> namen(const DiskVolume& v) {
    std::vector<std::string> r;
    for (const FileEntry& e : v.list()) r.push_back(e.name);
    return r;
}

// ── Die Abbildung Block -> (Zylinder, ID), unabhaengig von Mutos8kDev (Entwurf 39 4.6.3) ──

constexpr uint32_t kBloecke = 494;
constexpr uint32_t kStromBytes = kBloecke * 512;

void sektorVon(uint32_t l, uint8_t& cyl, uint8_t& id) {
    const uint32_t rel = l / 26, pos = l % 26;
    const uint32_t platz = pos < 13 ? 2 * pos : 2 * (pos - 13) + 1;
    cyl = static_cast<uint8_t>(1 + rel);
    id  = static_cast<uint8_t>((platz + 6 * rel) % 26 + 1);
}

/// Das Rohgeraet einer Diskette, wie MUTOS es sieht (494 Bloecke).  @p format: Katalogformat der Geometrie.
std::vector<uint8_t> rohgeraet(const std::string& pfad, const std::string& format) {
    const DiskFormat* f = formate().find(format);
    EXPECT_TRUE(f);
    std::optional<DiskFormat> of;
    if (fs::path(pfad).extension() == ".img") of = *f;
    auto img = DiskImage::open(pfad, of, true);
    EXPECT_TRUE(img) << pfad;
    std::vector<uint8_t> strom;
    if (!img || !f) return strom;
    SectorSpace sp(img->medium(), *f);
    for (uint32_t l = 0; l < kBloecke * 4; ++l) {
        uint8_t cyl = 0, id = 0;
        sektorVon(l, cyl, id);
        SectorData sd;
        const bool ok = sp.readSector(cyl, 0, id, sd);
        EXPECT_TRUE(ok) << "Sektor " << int(cyl) << "/" << int(id);
        EXPECT_EQ(sd.data.size(), 128u);
        if (!ok || sd.data.size() != 128) { strom.insert(strom.end(), 128, 0); continue; }
        strom.insert(strom.end(), sd.data.begin(), sd.data.end());
    }
    return strom;
}

/// Alle Sektoren des Zylinders 0 (zum Beweis, dass er unberuehrt bleibt).
std::vector<uint8_t> zylinder0(const std::string& pfad, const std::string& format) {
    const DiskFormat* f = formate().find(format);
    std::optional<DiskFormat> of;
    if (fs::path(pfad).extension() == ".img") of = *f;
    auto img = DiskImage::open(pfad, of, true);
    std::vector<uint8_t> r;
    if (!img || !f) return r;
    SectorSpace sp(img->medium(), *f);
    for (int id = 1; id <= 26; ++id) {
        SectorData sd;
        EXPECT_TRUE(sp.readSector(0, 0, static_cast<uint8_t>(id), sd));
        r.insert(r.end(), sd.data.begin(), sd.data.end());
    }
    return r;
}

bool systemTarDa() { return std::system("tar --version > /dev/null 2>&1") == 0; }

}  // namespace

// ── Lesen: von MUTOS selbst geschriebene Disketten ───────────────────────────

TEST(TarMutos, LiestEineVonMutosGeschriebeneEinzeldiskette) {
    k1520test::TempDisk d("mutos8000_tar_crt0.dmk");
    std::string err;
    auto v = DiskVolume::open(d.path(), "", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    EXPECT_EQ(v->detection().filesystem, "tar_mutos8k_26x128_mfm");
    EXPECT_EQ(v->detection().format, "udos_ss77");
    ASSERT_EQ(namen(*v), std::vector<std::string>{"crt0.o"});
    EXPECT_EQ(inhalt(*v, "crt0.o").size(), 200u);
}

TEST(TarMutos, LiestDenMehrdisketten_SatzDenMutosMitDdGeschriebenHat) {
    // 35 Dateien (/usr und /bin), 677 Bloecke; bin/date beginnt auf Diskette 1 und endet auf Diskette 2.
    k1520test::TempDisk a("mutos8000_tar_satz1.dmk"), b("mutos8000_tar_satz2.dmk");
    std::string err;
    auto v = DiskVolume::openTarSatz({a.path(), b.path()}, formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    EXPECT_EQ(v->list().size(), 35u);
    EXPECT_EQ(v->detection().filesystem, "tar_mutos8k_26x128_mfm");
    EXPECT_EQ(v->tarSatzPlanen().kapazitaet, uint64_t(kStromBytes));
    const auto date = inhalt(*v, "bin/date");
    EXPECT_EQ(date.size(), 5774u);

    // Gegenprobe: Strom aus den Sektoren mit der EIGENEN Formel, System-tar liest ihn.
    if (!systemTarDa()) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarmutos_lesen_satz");
    auto s = rohgeraet(a.path(), "udos_ss77");
    const auto s2 = rohgeraet(b.path(), "udos_ss77");
    s.insert(s.end(), s2.begin(), s2.end());
    schreibeDatei(o / "alles.tar", s);
    fs::create_directories(o / "aus");
    ASSERT_EQ(std::system(("tar -xf '" + o / "alles.tar" + "' -C '" + o / "aus" + "' 2>/dev/null").c_str()), 0);
    EXPECT_EQ(leseDatei(o / "aus/bin/date"), date);
}

// ── Schreiben: Satz ueber mehrere Disketten, Rundlauf und Gegenprobe ─────────

TEST(TarMutos, SatzRundlaufInAllenContainernUndBeidenAufzeichnungen) {
    struct Fall { const char* profil; const char* format; const char* ext; };
    const Fall faelle[] = {
        {"tar_mutos8k_26x128_mfm", "udos_ss77",     "hfe"},
        {"tar_mutos8k_26x128_mfm", "udos_ss77",     "dmk"},
        {"tar_mutos8k_26x128_mfm", "udos_ss77",     "img"},
        {"tar_mutos8k_26x128_fm",  "mf3200_26x128", "hfe"},
    };
    for (const Fall& fa : faelle) {
        SCOPED_TRACE(std::string(fa.profil) + "." + fa.ext);
        Ordner o(std::string("tarmutos_satz_") + fa.profil + "_" + fa.ext);
        std::string err;
        auto v = DiskVolume::tarSatzNeu(fa.profil, o / (std::string("m.") + fa.ext), formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        EXPECT_EQ(v->tarSatzPlanen().kapazitaet, uint64_t(kStromBytes));

        const auto a = muster(300, 1), b = muster(5000, 2), c = muster(140000, 3), d = muster(150000, 4);
        ASSERT_TRUE(einfuegen(*v, quelle(o, "a.bin", a), "a.bin")) << v->lastError();
        ASSERT_TRUE(einfuegen(*v, quelle(o, "b.bin", b), "dir/b.bin")) << v->lastError();
        ASSERT_TRUE(einfuegen(*v, quelle(o, "c.bin", c), "c.bin")) << v->lastError();
        ASSERT_TRUE(einfuegen(*v, quelle(o, "d.bin", d), "d.bin")) << v->lastError();
        const TarSatzPlan plan = v->tarSatzPlanen();
        ASSERT_TRUE(plan.ok) << plan.fehler;
        ASSERT_EQ(plan.abbilder, 2);
        ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();
        const std::string n1 = o / (std::string("m1.") + fa.ext), n2 = o / (std::string("m2.") + fa.ext);
        ASSERT_TRUE(fs::exists(n1) && fs::exists(n2));

        // Der Strom auf den Disketten: EIGENE Formel, ohne Mutos8kDev.  Rest der letzten Diskette Null.
        auto strom = rohgeraet(n1, fa.format);
        const auto strom2 = rohgeraet(n2, fa.format);
        strom.insert(strom.end(), strom2.begin(), strom2.end());
        ASSERT_EQ(strom.size(), 2u * kStromBytes);
        const uint64_t nutz = tar::exportLaenge(strom.data(), strom.size());
        ASSERT_GT(nutz, uint64_t(kStromBytes));
        EXPECT_TRUE(tar::alleNull(strom.data() + nutz, strom.size() - nutz)) << "Auffuellung nicht Null";

        // Wiederoeffnen als Satz: die Erkennung findet das Layout am ERSTEN Abbild.
        auto r = DiskVolume::openTarSatz({n1, n2}, formate(), dateisysteme(), err);
        ASSERT_TRUE(r) << err;
        EXPECT_EQ(r->detection().filesystem, fa.profil);
        EXPECT_EQ(inhalt(*r, "a.bin"), a);
        EXPECT_EQ(inhalt(*r, "dir/b.bin"), b);
        EXPECT_EQ(inhalt(*r, "c.bin"), c);
        EXPECT_EQ(inhalt(*r, "d.bin"), d);

        // ... auch mit vorgegebenem Profil (wie `--format tar_mutos8k_26x128_mfm`)
        auto r2 = DiskVolume::openTarSatz({n1, n2}, formate(), dateisysteme(), err, fa.profil);
        ASSERT_TRUE(r2) << err;
        EXPECT_EQ(inhalt(*r2, "d.bin"), d);

        // Gegenprobe System-tar ueber die unabhaengig gelesenen Strome
        if (systemTarDa()) {
            schreibeDatei(o / "alles.tar", strom);
            fs::create_directories(o / "aus");
            ASSERT_EQ(std::system(("tar -xf '" + o / "alles.tar" + "' -C '" + o / "aus" + "' 2>/dev/null").c_str()), 0);
            EXPECT_EQ(leseDatei(o / "aus/a.bin"), a);
            EXPECT_EQ(leseDatei(o / "aus/dir/b.bin"), b);
            EXPECT_EQ(leseDatei(o / "aus/c.bin"), c);
            EXPECT_EQ(leseDatei(o / "aus/d.bin"), d);
        }
    }
}

TEST(TarMutos, SatzBearbeitenDateiHinzufuegenUndEntfernen) {
    Ordner o("tarmutos_satz_bearbeiten");
    std::string err;
    {
        auto v = DiskVolume::tarSatzNeu("tar_mutos8k_26x128_mfm", o / "s.hfe", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        ASSERT_TRUE(einfuegen(*v, quelle(o, "g1.bin", muster(150000, 11)), "g1.bin"));
        ASSERT_TRUE(einfuegen(*v, quelle(o, "g2.bin", muster(150000, 12)), "g2.bin"));
        ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();
    }
    const std::string n1 = o / "s1.hfe", n2 = o / "s2.hfe";
    ASSERT_TRUE(fs::exists(n2));
    auto v = DiskVolume::openTarSatz({n1, n2}, formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    EXPECT_TRUE(v->readOnly());
    v->setReadOnly(false);
    ASSERT_FALSE(v->readOnly()) << v->lastError();
    const auto neu = muster(400, 13);
    ASSERT_TRUE(einfuegen(*v, quelle(o, "klein.bin", neu), "klein.bin")) << v->lastError();
    ASSERT_TRUE(v->erase(FileRef::parse("g1.bin", 0))) << v->lastError();
    ASSERT_TRUE(v->tarSatzSpeichern(o / "s.hfe")) << v->lastError();   // passt jetzt auf EIN Abbild (-> s1.hfe)
    // die zweite Nummer bleibt liegen (nie stillschweigend loeschen), s1 traegt alles
    auto r = DiskVolume::open(n1, "", formate(), dateisysteme(), err);
    ASSERT_TRUE(r) << err;
    EXPECT_EQ(r->detection().filesystem, "tar_mutos8k_26x128_mfm");
    EXPECT_EQ(inhalt(*r, "klein.bin"), neu);
    EXPECT_EQ(inhalt(*r, "g2.bin"), muster(150000, 12));
    for (const std::string& n : namen(*r)) EXPECT_NE(n, "g1.bin");
}

// ── Einzeldiskette: leer anlegen, beschreiben, Zylinder 0 bleibt ─────────────

TEST(TarMutos, LeereDisketteHatNullenImDatenbereichUndLaesstZylinder0Liegen) {
    for (const char* profil : {"tar_mutos8k_26x128_mfm", "tar_mutos8k_26x128_fm"}) {
        SCOPED_TRACE(profil);
        const std::string fmt = std::string(profil) == "tar_mutos8k_26x128_fm" ? "mf3200_26x128" : "udos_ss77";
        Ordner o(std::string("tarmutos_leer_") + profil);
        std::string err;
        // Vergleichsstand: dieselbe Diskette frisch formatiert (E5), ohne tar
        {
            std::string grund;
            auto img = DiskImage::create(o / "ref.hfe", *formate().find(fmt), false,
                                         formate().find(fmt)->predominantEncoding(), &grund);
            ASSERT_TRUE(img) << grund;
        }
        auto v = DiskVolume::tarDisketteNeu(o / "t.hfe", profil, formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        EXPECT_TRUE(v->tarSatzEinzel());
        v.reset();

        // Datenbereich: lauter Nullbloecke (MUTOS-tar liest sie als leeres Archiv, E5 nicht)
        const auto strom = rohgeraet(o / "t.hfe", fmt);
        ASSERT_EQ(strom.size(), size_t(kStromBytes));
        EXPECT_TRUE(tar::alleNull(strom.data(), strom.size()));
        // Zylinder 0 unberuehrt (E5 wie auf der frisch formatierten Diskette)
        EXPECT_EQ(zylinder0(o / "t.hfe", fmt), zylinder0(o / "ref.hfe", fmt));

        auto r = DiskVolume::open(o / "t.hfe", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(r) << err;
        EXPECT_EQ(r->detection().filesystem, std::string(profil) + "_leer");
    }
}

TEST(TarMutos, EinzeldisketteBeschreibenSpeichernUndWiederLesen) {
    Ordner o("tarmutos_einzel");
    std::string err;
    const auto a = muster(300, 21), b = muster(30000, 22);
    {
        auto v = DiskVolume::tarDisketteNeu(o / "e.hfe", "tar_mutos8k_26x128_mfm", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        ASSERT_TRUE(einfuegen(*v, quelle(o, "a.bin", a), "a.bin")) << v->lastError();
        ASSERT_TRUE(einfuegen(*v, quelle(o, "b.bin", b), "b.bin")) << v->lastError();
        ASSERT_TRUE(v->flush()) << v->lastError();
    }
    EXPECT_FALSE(fs::exists(o / "e1.hfe"));          // die Diskette behaelt ihren Namen
    {
        auto v = DiskVolume::open(o / "e.hfe", "", formate(), dateisysteme(), err, false);
        ASSERT_TRUE(v) << err;
        EXPECT_EQ(v->detection().filesystem, "tar_mutos8k_26x128_mfm");
        EXPECT_EQ(inhalt(*v, "a.bin"), a);
        EXPECT_EQ(inhalt(*v, "b.bin"), b);
        ASSERT_TRUE(v->erase(FileRef::parse("a.bin", 0))) << v->lastError();
        ASSERT_TRUE(v->erase(FileRef::parse("b.bin", 0))) << v->lastError();
        ASSERT_TRUE(v->flush()) << v->lastError();
    }
    // Alles entfernt: wieder ein leeres Archiv, der Datenbereich ganz Null (kein Rest alter Bloecke)
    const auto strom = rohgeraet(o / "e.hfe", "udos_ss77");
    EXPECT_TRUE(tar::alleNull(strom.data(), strom.size()));
    auto r = DiskVolume::open(o / "e.hfe", "", formate(), dateisysteme(), err);
    ASSERT_TRUE(r) << err;
    EXPECT_EQ(r->detection().filesystem, "tar_mutos8k_26x128_mfm_leer");
}

TEST(TarMutos, AnlegenMitCreateFuerAlleLeerenProfileUndContainer) {
    for (const char* profil : {"tar_mutos8k_26x128_mfm_leer", "tar_mutos8k_26x128_fm_leer"}) {
        for (const char* ext : {"hfe", "dmk", "img"}) {
            // Ein .img traegt kein Aufzeichnungsverfahren: 77 x 26 x 128 ist ohne Angabe immer MFM
            // (K5601); die FM-Fassung als .img oeffnet man mit --fs / vorgegebenem Profil.
            if (std::string(ext) == "img" && std::string(profil) == "tar_mutos8k_26x128_fm_leer") continue;
            SCOPED_TRACE(std::string(profil) + "." + ext);
            Ordner o(std::string("tarmutos_create_") + profil + "_" + ext);
            std::string err;
            auto v = DiskVolume::create(o / (std::string("c.") + ext), profil, "", formate(), dateisysteme(), err);
            ASSERT_TRUE(v) << err;
            v.reset();
            auto r = DiskVolume::open(o / (std::string("c.") + ext), "", formate(), dateisysteme(), err);
            ASSERT_TRUE(r) << err;
            EXPECT_EQ(r->detection().filesystem, profil);
        }
    }
}

// ── Erkennung: nichts anderes wird zum tar ───────────────────────────────────

TEST(TarMutos, AndereDisketteGleicherGeometrieBleibenWasSieSind) {
    Ordner o("tarmutos_erkennung");
    std::string err;
    {   // leere UDOS-Diskette (E5 ueberall): bleibt UDOS, nicht "leeres tar"
        auto v = DiskVolume::create(o / "u.hfe", "udos_ss77", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
    }
    {   // MUTOS-Dateisystem auf derselben Geometrie
        auto v = DiskVolume::create(o / "m.hfe", "mutos8k_26x128_mfm", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
    }
    auto u = DiskVolume::open(o / "u.hfe", "", formate(), dateisysteme(), err);
    ASSERT_TRUE(u) << err;
    EXPECT_EQ(u->detection().filesystem, "udos_ss77");
    auto m = DiskVolume::open(o / "m.hfe", "", formate(), dateisysteme(), err);
    ASSERT_TRUE(m) << err;
    EXPECT_EQ(m->detection().filesystem, "mutos8k_26x128_mfm");
}

TEST(TarMutos, FormatlisteNenntDieMutosProfileMitDerKapazitaetDesRohgeraets) {
    bool mfm = false, fm = false;
    for (const TarFormatInfo& i : DiskVolume::tarFormate(formate(), dateisysteme())) {
        if (i.name == "tar_mutos8k_26x128_mfm") { mfm = true; EXPECT_EQ(i.bytes, uint64_t(kStromBytes)); EXPECT_TRUE(i.hfe); }
        if (i.name == "tar_mutos8k_26x128_fm")  { fm = true;  EXPECT_EQ(i.bytes, uint64_t(kStromBytes)); }
    }
    EXPECT_TRUE(mfm && fm);
    for (const TarFormatInfo& i : DiskVolume::tarFormate(formate()))      // die alte Fassung kennt sie nicht
        EXPECT_NE(i.name, "tar_mutos8k_26x128_mfm");
}


// ══ 8x512 (DISK-Satz 1987, Ladediskette ..._k5601_8x512; Entwurf 39 A5c) ═════════════════════════════════════
//
// Dort rechnet `mutos_io` LINEAR: Block n = Zylinder 1 + n/8, Sektor-ID n mod 8 + 1, 76 Zylinder = 608 Bloecke
// (der Kernel lehnt Block >= 608 ab).  Die Formel steht hier wieder ein zweites Mal, unabhaengig von WegaSpaceDev.
// Die Fixtures `mutos8000_tar_8x512_*.dmk` hat MUTOS selbst im Emulator geschrieben (`tar cvf /dev/rrk3 ...`,
// Satz: `tar cf - etc bin mnt | dd of=/dev/rrk3 bs=512 count=608`, Folgediskette `skip=608`).

namespace {

constexpr uint32_t kBloecke512 = 608;
constexpr uint32_t kStromBytes512 = kBloecke512 * 512;

std::vector<uint8_t> rohgeraet512(const std::string& pfad) {
    const DiskFormat* f = formate().find("mutos_ss77");
    EXPECT_TRUE(f);
    std::optional<DiskFormat> of;
    if (fs::path(pfad).extension() == ".img") of = *f;
    auto img = DiskImage::open(pfad, of, true);
    EXPECT_TRUE(img) << pfad;
    std::vector<uint8_t> strom;
    if (!img || !f) return strom;
    SectorSpace sp(img->medium(), *f);
    for (uint32_t n = 0; n < kBloecke512; ++n) {
        SectorData sd;
        const uint8_t cyl = static_cast<uint8_t>(1 + n / 8), id = static_cast<uint8_t>(n % 8 + 1);
        const bool ok = sp.readSector(cyl, 0, id, sd);
        EXPECT_TRUE(ok) << "Sektor " << int(cyl) << "/" << int(id);
        EXPECT_EQ(sd.data.size(), 512u);
        if (!ok || sd.data.size() != 512) { strom.insert(strom.end(), 512, 0); continue; }
        strom.insert(strom.end(), sd.data.begin(), sd.data.end());
    }
    return strom;
}

std::vector<uint8_t> zylinder0Mutos(const std::string& pfad) {
    const DiskFormat* f = formate().find("mutos_ss77");
    std::optional<DiskFormat> of;
    if (fs::path(pfad).extension() == ".img") of = *f;
    auto img = DiskImage::open(pfad, of, true);
    std::vector<uint8_t> r;
    if (!img || !f) return r;
    SectorSpace sp(img->medium(), *f);
    for (int id = 1; id <= 26; ++id) {
        SectorData sd;
        EXPECT_TRUE(sp.readSector(0, 0, static_cast<uint8_t>(id), sd));
        r.insert(r.end(), sd.data.begin(), sd.data.end());
    }
    return r;
}

}  // namespace

TEST(TarMutos8x512, LiestEineVonMutosGeschriebeneEinzeldiskette) {
    // `cd /; tar cvf /dev/rrk3 etc/group etc/ttys etc/passwd bin/sync` unter MUTOS (8x512-Ladediskette)
    k1520test::TempDisk d("mutos8000_tar_8x512_einzel.dmk");
    std::string err;
    auto v = DiskVolume::open(d.path(), "", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    EXPECT_EQ(v->detection().filesystem, "tar_mutos8k_8x512_mfm");
    EXPECT_EQ(v->detection().format, "mutos_ss77");
    EXPECT_EQ(namen(*v), (std::vector<std::string>{"etc/group", "etc/ttys", "etc/passwd", "bin/sync"}));
    const auto ttys = inhalt(*v, "etc/ttys");
    EXPECT_EQ(std::string(ttys.begin(), ttys.end()), "1wconsole\n02tty1\n");
    EXPECT_EQ(inhalt(*v, "bin/sync").size(), 132u);

    // Gegenprobe: der Strom aus den Sektoren mit der EIGENEN Formel, das System-tar liest ihn
    if (!systemTarDa()) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarmutos8x512_einzel");
    schreibeDatei(o / "alles.tar", rohgeraet512(d.path()));
    fs::create_directories(o / "aus");
    ASSERT_EQ(std::system(("tar -xf '" + o / "alles.tar" + "' -C '" + o / "aus" + "' 2>/dev/null").c_str()), 0);
    EXPECT_EQ(leseDatei(o / "aus/etc/ttys"), ttys);
    EXPECT_EQ(leseDatei(o / "aus/bin/sync"), inhalt(*v, "bin/sync"));
}

TEST(TarMutos8x512, LiestDenMehrdisketten_SatzDenMutosMitDdGeschriebenHat) {
    // 54 Eintraege (etc, bin, mnt), 642 Bloecke; mnt/bin/tar (17000 B) beginnt auf Block 586 und endet
    // auf Diskette 2 (Block 620 > 608).
    k1520test::TempDisk a("mutos8000_tar_8x512_satz1.dmk"), b("mutos8000_tar_8x512_satz2.dmk");
    std::string err;
    auto v = DiskVolume::openTarSatz({a.path(), b.path()}, formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    EXPECT_EQ(v->list().size(), 54u);
    EXPECT_EQ(v->detection().filesystem, "tar_mutos8k_8x512_mfm");
    EXPECT_EQ(v->tarSatzPlanen().kapazitaet, uint64_t(kStromBytes512));
    const auto tar = inhalt(*v, "mnt/bin/tar");
    EXPECT_EQ(tar.size(), 17000u);

    if (!systemTarDa()) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarmutos8x512_lesen_satz");
    auto s = rohgeraet512(a.path());
    const auto s2 = rohgeraet512(b.path());
    s.insert(s.end(), s2.begin(), s2.end());
    schreibeDatei(o / "alles.tar", s);
    fs::create_directories(o / "aus");
    ASSERT_EQ(std::system(("tar -xf '" + o / "alles.tar" + "' -C '" + o / "aus" + "' 2>/dev/null").c_str()), 0);
    EXPECT_EQ(leseDatei(o / "aus/mnt/bin/tar"), tar);
}

TEST(TarMutos8x512, SatzRundlaufInAllenContainern) {
    for (const char* ext : {"hfe", "dmk", "img", "td0", "imd"}) {
        SCOPED_TRACE(ext);
        Ordner o(std::string("tarmutos8x512_satz_") + ext);
        std::string err;
        auto v = DiskVolume::tarSatzNeu("tar_mutos8k_8x512_mfm", o / (std::string("m.") + ext), formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        EXPECT_EQ(v->tarSatzPlanen().kapazitaet, uint64_t(kStromBytes512));

        const auto a = muster(300, 1), b = muster(5000, 2), c = muster(140000, 3), d = muster(190000, 4);
        ASSERT_TRUE(einfuegen(*v, quelle(o, "a.bin", a), "a.bin")) << v->lastError();
        ASSERT_TRUE(einfuegen(*v, quelle(o, "b.bin", b), "dir/b.bin")) << v->lastError();
        ASSERT_TRUE(einfuegen(*v, quelle(o, "c.bin", c), "c.bin")) << v->lastError();
        ASSERT_TRUE(einfuegen(*v, quelle(o, "d.bin", d), "d.bin")) << v->lastError();
        const TarSatzPlan plan = v->tarSatzPlanen();
        ASSERT_TRUE(plan.ok) << plan.fehler;
        ASSERT_EQ(plan.abbilder, 2);
        ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();
        const std::string n1 = o / (std::string("m1.") + ext), n2 = o / (std::string("m2.") + ext);
        ASSERT_TRUE(fs::exists(n1) && fs::exists(n2));

        // Der Strom auf den Disketten: EIGENE Formel; Rest der letzten Diskette Null
        auto strom = rohgeraet512(n1);
        const auto strom2 = rohgeraet512(n2);
        strom.insert(strom.end(), strom2.begin(), strom2.end());
        ASSERT_EQ(strom.size(), 2u * kStromBytes512);
        const uint64_t nutz = tar::exportLaenge(strom.data(), strom.size());
        ASSERT_GT(nutz, uint64_t(kStromBytes512));
        EXPECT_TRUE(tar::alleNull(strom.data() + nutz, strom.size() - nutz)) << "Auffuellung nicht Null";

        // Wiederoeffnen: das Layout wird am ERSTEN Abbild erkannt, auch mit vorgegebenem Profil
        auto r = DiskVolume::openTarSatz({n1, n2}, formate(), dateisysteme(), err);
        ASSERT_TRUE(r) << err;
        EXPECT_EQ(r->detection().filesystem, "tar_mutos8k_8x512_mfm");
        EXPECT_EQ(inhalt(*r, "a.bin"), a);
        EXPECT_EQ(inhalt(*r, "dir/b.bin"), b);
        EXPECT_EQ(inhalt(*r, "c.bin"), c);
        EXPECT_EQ(inhalt(*r, "d.bin"), d);
        auto r2 = DiskVolume::openTarSatz({n1, n2}, formate(), dateisysteme(), err, "tar_mutos8k_8x512_mfm");
        ASSERT_TRUE(r2) << err;
        EXPECT_EQ(inhalt(*r2, "d.bin"), d);

        if (systemTarDa()) {
            schreibeDatei(o / "alles.tar", strom);
            fs::create_directories(o / "aus");
            ASSERT_EQ(std::system(("tar -xf '" + o / "alles.tar" + "' -C '" + o / "aus" + "' 2>/dev/null").c_str()), 0);
            EXPECT_EQ(leseDatei(o / "aus/a.bin"), a);
            EXPECT_EQ(leseDatei(o / "aus/dir/b.bin"), b);
            EXPECT_EQ(leseDatei(o / "aus/c.bin"), c);
            EXPECT_EQ(leseDatei(o / "aus/d.bin"), d);
        }
    }
}

TEST(TarMutos8x512, LeereDisketteHatNullenImDatenbereichUndLaesstZylinder0Liegen) {
    Ordner o("tarmutos8x512_leer");
    std::string err;
    {   // Vergleichsstand: dieselbe Diskette frisch formatiert (E5), ohne tar
        std::string grund;
        auto img = DiskImage::create(o / "ref.hfe", *formate().find("mutos_ss77"), false,
                                     formate().find("mutos_ss77")->predominantEncoding(), &grund);
        ASSERT_TRUE(img) << grund;
    }
    auto v = DiskVolume::tarDisketteNeu(o / "t.hfe", "tar_mutos8k_8x512_mfm", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    EXPECT_TRUE(v->tarSatzEinzel());
    v.reset();
    const auto strom = rohgeraet512(o / "t.hfe");
    ASSERT_EQ(strom.size(), size_t(kStromBytes512));
    EXPECT_TRUE(tar::alleNull(strom.data(), strom.size()));
    EXPECT_EQ(zylinder0Mutos(o / "t.hfe"), zylinder0Mutos(o / "ref.hfe"));
    auto r = DiskVolume::open(o / "t.hfe", "", formate(), dateisysteme(), err);
    ASSERT_TRUE(r) << err;
    EXPECT_EQ(r->detection().filesystem, "tar_mutos8k_8x512_mfm_leer");
}

TEST(TarMutos8x512, EinzeldisketteBeschreibenSpeichernUndWiederLesen) {
    for (const char* ext : {"hfe", "dmk", "img", "td0", "imd"}) {
        SCOPED_TRACE(ext);
        Ordner o(std::string("tarmutos8x512_einzel_") + ext);
        std::string err;
        const auto a = muster(300, 21), b = muster(30000, 22);
        {
            auto v = DiskVolume::tarDisketteNeu(o / (std::string("e.") + ext), "tar_mutos8k_8x512_mfm", formate(), dateisysteme(), err);
            ASSERT_TRUE(v) << err;
            ASSERT_TRUE(einfuegen(*v, quelle(o, "a.bin", a), "a.bin")) << v->lastError();
            ASSERT_TRUE(einfuegen(*v, quelle(o, "b.bin", b), "b.bin")) << v->lastError();
            ASSERT_TRUE(v->flush()) << v->lastError();
        }
        {
            auto v = DiskVolume::open(o / (std::string("e.") + ext), "", formate(), dateisysteme(), err, false);
            ASSERT_TRUE(v) << err;
            EXPECT_EQ(v->detection().filesystem, "tar_mutos8k_8x512_mfm");
            EXPECT_EQ(inhalt(*v, "a.bin"), a);
            EXPECT_EQ(inhalt(*v, "b.bin"), b);
            ASSERT_TRUE(v->erase(FileRef::parse("a.bin", 0))) << v->lastError();
            ASSERT_TRUE(v->erase(FileRef::parse("b.bin", 0))) << v->lastError();
            ASSERT_TRUE(v->flush()) << v->lastError();
        }
        const auto strom = rohgeraet512(o / (std::string("e.") + ext));
        EXPECT_TRUE(tar::alleNull(strom.data(), strom.size()));
        auto r = DiskVolume::open(o / (std::string("e.") + ext), "", formate(), dateisysteme(), err);
        ASSERT_TRUE(r) << err;
        EXPECT_EQ(r->detection().filesystem, "tar_mutos8k_8x512_mfm_leer");
    }
}

TEST(TarMutos8x512, AnlegenMitCreateUndAndereDisketteBleibenWasSieSind) {
    Ordner o("tarmutos8x512_erkennung");
    std::string err;
    for (const char* ext : {"hfe", "dmk", "img", "td0", "imd"}) {
        auto v = DiskVolume::create(o / (std::string("c.") + ext), "tar_mutos8k_8x512_mfm_leer", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << ext << ": " << err;
        v.reset();
        auto r = DiskVolume::open(o / (std::string("c.") + ext), "", formate(), dateisysteme(), err);
        ASSERT_TRUE(r) << ext << ": " << err;
        EXPECT_EQ(r->detection().filesystem, "tar_mutos8k_8x512_mfm_leer") << ext;
    }
    {   // das UNIX-Dateisystem auf derselben Geometrie und die leer formatierte Diskette bleiben, was sie sind
        auto v = DiskVolume::create(o / "u.hfe", "mutos8k_8x512_mfm", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        std::string grund;
        auto img = DiskImage::create(o / "e5.hfe", *formate().find("mutos_ss77"), false,
                                     formate().find("mutos_ss77")->predominantEncoding(), &grund);
        ASSERT_TRUE(img) << grund;
    }
    auto u = DiskVolume::open(o / "u.hfe", "", formate(), dateisysteme(), err);
    ASSERT_TRUE(u) << err;
    EXPECT_EQ(u->detection().filesystem, "mutos8k_8x512_mfm");
    auto e = DiskVolume::open(o / "e5.hfe", "", formate(), dateisysteme(), err, true, /*roh_erlaubt=*/true);
    ASSERT_TRUE(e) << err;
    EXPECT_NE(e->detection().filesystem.rfind("tar", 0), 0u) << e->detection().filesystem;
}

TEST(TarMutos8x512, FormatlisteNenntDasProfilMitDerKapazitaetDesRohgeraets) {
    bool da = false;
    for (const TarFormatInfo& i : DiskVolume::tarFormate(formate(), dateisysteme()))
        if (i.name == "tar_mutos8k_8x512_mfm") { da = true; EXPECT_EQ(i.bytes, uint64_t(kStromBytes512)); EXPECT_TRUE(i.hfe); EXPECT_TRUE(i.img); }
    EXPECT_TRUE(da);
}
