/**
 * @file test_tar_satz.cpp
 * @brief GoogleTests fuer den tar-SATZ im Bearbeitungsmodus (Entwurf 37, AP T2):
 *        Verteilen auf N Abbilder, Nummerierung, Rundlauf, Ueberlauf einer Einzeldiskette,
 *        Schutz alter Nummern, Gegenprobe mit dem `tar` des Systems.
 *
 * @see core/filesystem/disk_volume.h (tarSatzNeu/tarZuSatz/tarSatzSpeichern),
 *      doc/design/37_tar_satz_und_platte.md
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

/// Deterministische Fuellung, je Startwert verschieden (kein Zufall im Test).
std::vector<uint8_t> muster(size_t n, uint32_t start) {
    std::vector<uint8_t> d(n);
    uint32_t x = start * 2654435761u + 1;
    for (auto& b : d) { x = x * 1664525u + 1013904223u; b = static_cast<uint8_t>(x >> 24); }
    return d;
}

/// Ein eigener Ordner je Test, damit Nummern und `~`-Dateien nicht miteinander kollidieren.
struct Ordner {
    std::string pfad;
    explicit Ordner(const std::string& name) : pfad(k1520test::tempPath(name)) {
        fs::remove_all(pfad);
        fs::create_directories(pfad);
    }
    ~Ordner() { std::error_code ec; fs::remove_all(pfad, ec); }
    std::string operator/(const std::string& n) const { return pfad + "/" + n; }
};

/// Eine Linux-Datei als Quelle fuer insert.
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

std::unique_ptr<DiskVolume> oeffneSatz(const std::vector<std::string>& pfade) {
    std::string err;
    auto v = DiskVolume::openTarSatz(pfade, formate(), dateisysteme(), err);
    EXPECT_TRUE(v) << err;
    return v;
}

std::vector<std::string> namen(const DiskVolume& v) {
    std::vector<std::string> r;
    for (const FileEntry& e : v.list()) r.push_back(e.name);
    return r;
}

std::vector<uint8_t> inhalt(DiskVolume& v, const std::string& name) {
    const std::string tmp = k1520test::tempPath("satz_get.bin");
    TransferOptions t;
    t.overwrite = true;
    EXPECT_TRUE(v.extract(FileRef::parse(name, 0), tmp, t)) << v.lastError();
    auto d = leseDatei(tmp);
    fs::remove(tmp);
    return d;
}

}  // namespace

// ── Verteilen, Nummerierung, Rundlauf ────────────────────────────────────────

TEST(TarSatz, VerteiltAufNAbbilderMitNummerVorDerEndung) {
    Ordner o("tarsatz_verteilen");
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_ss40_16x256", o / "satz.hfe", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    EXPECT_TRUE(v->tarSatz());
    EXPECT_FALSE(v->readOnly());
    EXPECT_FALSE(v->dirty());

    const auto a = muster(150000, 1), b = muster(200000, 2);   // 160 KiB je Abbild -> 3 Abbilder
    ASSERT_TRUE(einfuegen(*v, quelle(o, "a.bin", a), "a.bin")) << v->lastError();
    ASSERT_TRUE(einfuegen(*v, quelle(o, "b.bin", b), "b.bin")) << v->lastError();
    EXPECT_TRUE(v->dirty());

    const TarSatzPlan plan = v->tarSatzPlanen();
    ASSERT_TRUE(plan.ok) << plan.fehler;
    EXPECT_EQ(plan.abbilder, 3);
    EXPECT_EQ(plan.kapazitaet, 163840u);
    ASSERT_EQ(plan.namen.size(), 3u);
    EXPECT_EQ(plan.namen[0], o / "satz1.hfe");
    EXPECT_EQ(plan.namen[2], o / "satz3.hfe");
    EXPECT_TRUE(plan.ueberschreibt.empty());

    std::vector<std::string> hinweise, geschrieben;
    ASSERT_TRUE(v->tarSatzSpeichern("", &hinweise, &geschrieben)) << v->lastError();
    EXPECT_TRUE(hinweise.empty());
    EXPECT_EQ(geschrieben.size(), 3u);
    EXPECT_FALSE(v->dirty());
    for (const char* n : {"satz1.hfe", "satz2.hfe", "satz3.hfe"}) EXPECT_TRUE(fs::exists(o / n)) << n;
    EXPECT_FALSE(fs::exists(o / "satz4.hfe"));
    EXPECT_FALSE(fs::exists(o / "satz.hfe"));

    // Rundlauf: als Satz wieder oeffnen, Namen und Inhalte stimmen.
    auto w = oeffneSatz({o / "satz1.hfe", o / "satz2.hfe", o / "satz3.hfe"});
    ASSERT_TRUE(w);
    EXPECT_EQ(namen(*w), (std::vector<std::string>{"a.bin", "b.bin"}));
    EXPECT_EQ(inhalt(*w, "a.bin"), a);
    EXPECT_EQ(inhalt(*w, "b.bin"), b);
    // Der Basisname ergibt sich aus dem ersten Abbild.
    EXPECT_EQ(w->tarSatzBasis(), o / "satz.hfe");
}

TEST(TarSatz, LetztesAbbildMitNullenAufgefuellt) {
    Ordner o("tarsatz_nullen");
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_ss40_16x256", o / "n.img", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    ASSERT_TRUE(einfuegen(*v, quelle(o, "x", muster(200000, 3)), "x"));
    ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();
    EXPECT_EQ(fs::file_size(o / "n1.img"), 163840u);          // .img: nur Nutzbytes
    EXPECT_EQ(fs::file_size(o / "n2.img"), 163840u);
    const auto z = leseDatei(o / "n2.img");
    // 200000 B Datei + Kopf + Endmarke -> Strom 201 072 B; der Rest von Abbild 2 (ab 37 232) ist Null.
    for (size_t i = 40000; i < z.size(); ++i) ASSERT_EQ(z[i], 0u) << i;
}

TEST(TarSatz, GegenprobeSystemTarUeberDieAbbildDatenbereiche) {
    if (std::system("tar --version > /dev/null 2>&1") != 0) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarsatz_systemtar");
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_ss40_16x256", o / "s.img", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    const auto a = muster(100000, 4), b = muster(90000, 5);
    ASSERT_TRUE(einfuegen(*v, quelle(o, "a.bin", a), "a.bin"));
    ASSERT_TRUE(einfuegen(*v, quelle(o, "b.bin", b), "dir/b.bin"));
    ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();

    // `cat s1.img s2.img …` : die .img-Dateien SIND die Datenbereiche.
    std::vector<uint8_t> cat;
    for (int i = 1; fs::exists(o / ("s" + std::to_string(i) + ".img")); ++i) {
        const auto d = leseDatei(o / ("s" + std::to_string(i) + ".img"));
        cat.insert(cat.end(), d.begin(), d.end());
    }
    schreibeDatei(o / "alles.tar", cat);
    EXPECT_EQ(std::system(("tar -tf '" + o / "alles.tar" + "' > '" + o / "liste.txt" + "' 2>&1").c_str()), 0);
    fs::create_directories(o / "aus");
    ASSERT_EQ(std::system(("tar -xf '" + o / "alles.tar" + "' -C '" + o / "aus" + "'").c_str()), 0);
    EXPECT_EQ(leseDatei(o / "aus/a.bin"), a);
    EXPECT_EQ(leseDatei(o / "aus/dir/b.bin"), b);
}

// ── Mehrere Formate und Container ────────────────────────────────────────────

TEST(TarSatz, RundlaufInAllenContainernUndFormaten) {
    const char* formen[] = {"k5601_9x512", "k5601_ss40_16x256", "k5601_ss40_26x128"};
    const char* exts[]   = {"img", "hfe", "dmk", "cqm"};
    for (const char* fm : formen)
        for (const char* ex : exts) {
            SCOPED_TRACE(std::string(fm) + "." + ex);
            Ordner o(std::string("tarsatz_form_") + fm + "_" + ex);
            std::string err;
            auto v = DiskVolume::tarSatzNeu(fm, o / (std::string("f.") + ex), formate(), dateisysteme(), err);
            ASSERT_TRUE(v) << err;
            const uint64_t kap = v->tarSatzPlanen().kapazitaet;
            ASSERT_GT(kap, 0u);
            const auto d = muster(static_cast<size_t>(kap + kap / 2), 7);   // 1,5 Abbilder -> 2 Abbilder
            ASSERT_TRUE(einfuegen(*v, quelle(o, "d.bin", d), "d.bin")) << v->lastError();
            std::vector<std::string> geschrieben;
            ASSERT_TRUE(v->tarSatzSpeichern("", nullptr, &geschrieben)) << v->lastError();
            ASSERT_EQ(geschrieben.size(), 2u);
            auto w = oeffneSatz(geschrieben);
            ASSERT_TRUE(w);
            EXPECT_EQ(inhalt(*w, "d.bin"), d);
        }
}

TEST(TarSatz, GemischtesFormatIstKeinSatzformat) {
    std::string err;
    auto v = DiskVolume::tarSatzNeu("cpa780", "/tmp/x.img", formate(), dateisysteme(), err);
    EXPECT_FALSE(v);
    EXPECT_NE(err.find("nicht geeignet"), std::string::npos) << err;
    auto w = DiskVolume::tarSatzNeu("gibt_es_nicht", "/tmp/x.img", formate(), dateisysteme(), err);
    EXPECT_FALSE(w);
    auto x = DiskVolume::tarSatzNeu("k5601_9x512", "/tmp/x.bin", formate(), dateisysteme(), err);
    EXPECT_FALSE(x);
    EXPECT_NE(err.find("Endung"), std::string::npos) << err;
}

// ── Einzeldiskette -> Satz ───────────────────────────────────────────────────

TEST(TarSatz, EinzeldisketteMitUeberlaufWirdBeimSpeichernZumSatz) {
    Ordner o("tarsatz_ueberlauf");
    std::string err;
    const std::string einzel = o / "e.img";
    {
        auto v = DiskVolume::create(einzel, "tar720_leer", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        ASSERT_TRUE(einfuegen(*v, quelle(o, "klein", muster(1000, 8)), "klein")) << v->lastError();
        ASSERT_TRUE(v->flush());
    }
    const auto vorher = leseDatei(einzel);

    std::string e2;
    auto v = DiskVolume::open(einzel, "", formate(), dateisysteme(), e2, false, true);
    ASSERT_TRUE(v) << e2;
    // Die Einzeldiskette hat eine harte Grenze ...
    EXPECT_FALSE(einfuegen(*v, quelle(o, "gross", muster(800000, 9)), "gross"));
    EXPECT_NE(v->lastError().find("fasst"), std::string::npos) << v->lastError();
    // ... der Satzmodus nicht.
    EXPECT_FALSE(v->tarSatz());
    ASSERT_TRUE(v->tarZuSatz(o / "ueb.img")) << v->lastError();
    EXPECT_TRUE(v->tarSatz());
    EXPECT_EQ(v->tarSatzPlanen().abbilder, 1);
    const auto gross = muster(800000, 9);
    ASSERT_TRUE(einfuegen(*v, quelle(o, "gross", gross), "gross")) << v->lastError();
    const TarSatzPlan plan = v->tarSatzPlanen();
    ASSERT_TRUE(plan.ok) << plan.fehler;
    EXPECT_EQ(plan.abbilder, 2);
    EXPECT_FALSE(plan.einzel);
    ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();
    EXPECT_TRUE(fs::exists(o / "ueb1.img"));
    EXPECT_TRUE(fs::exists(o / "ueb2.img"));
    EXPECT_EQ(leseDatei(einzel), vorher) << "die Datei der Einzeldiskette darf nicht angefasst werden";

    auto w = oeffneSatz({o / "ueb1.img", o / "ueb2.img"});
    ASSERT_TRUE(w);
    EXPECT_EQ(namen(*w), (std::vector<std::string>{"klein", "gross"}));
    EXPECT_EQ(inhalt(*w, "gross"), gross);
}

// ── Alte Nummern ─────────────────────────────────────────────────────────────

TEST(TarSatz, SchrumpfenLoeschtAlteNummernNieStillschweigend) {
    Ordner o("tarsatz_altnummern");
    std::string err;
    {
        auto v = DiskVolume::tarSatzNeu("k5601_ss40_16x256", o / "a.img", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        ASSERT_TRUE(einfuegen(*v, quelle(o, "g", muster(400000, 10)), "g"));
        ASSERT_TRUE(einfuegen(*v, quelle(o, "k", muster(500, 11)), "k"));
        ASSERT_TRUE(v->tarSatzSpeichern());
    }
    ASSERT_TRUE(fs::exists(o / "a3.img"));
    auto w = oeffneSatz({o / "a1.img", o / "a2.img", o / "a3.img"});
    ASSERT_TRUE(w);
    w->setReadOnly(false);
    ASSERT_FALSE(w->readOnly()) << w->lastError();
    ASSERT_TRUE(w->erase(FileRef::parse("g", 0))) << w->lastError();
    const TarSatzPlan plan = w->tarSatzPlanen();
    ASSERT_TRUE(plan.ok);
    EXPECT_EQ(plan.abbilder, 1);
    ASSERT_EQ(plan.altlasten.size(), 2u);
    EXPECT_EQ(plan.altlasten[0], o / "a2.img");
    EXPECT_EQ(plan.altlasten[1], o / "a3.img");
    EXPECT_EQ(plan.ueberschreibt, (std::vector<std::string>{o / "a1.img"}));

    const auto a2 = leseDatei(o / "a2.img"), a3 = leseDatei(o / "a3.img");
    std::vector<std::string> hinweise;
    ASSERT_TRUE(w->tarSatzSpeichern("", &hinweise)) << w->lastError();
    ASSERT_EQ(hinweise.size(), 2u);
    EXPECT_NE(hinweise[0].find("a2.img"), std::string::npos);
    EXPECT_NE(hinweise[1].find("a3.img"), std::string::npos);
    EXPECT_EQ(leseDatei(o / "a2.img"), a2);                    // unberuehrt
    EXPECT_EQ(leseDatei(o / "a3.img"), a3);
    EXPECT_TRUE(fs::exists(o / "a1.img~"));                   // das ueberschriebene Abbild ist gesichert
}

TEST(TarSatz, BasisnameOhneZiffernUndMitZiffern) {
    EXPECT_EQ(DiskVolume::tarSatzBasisname("/x/ariane1.cqm"), "/x/ariane.cqm");
    EXPECT_EQ(DiskVolume::tarSatzBasisname("/x/w11data12.hfe"), "/x/w11data.hfe");
    EXPECT_EQ(DiskVolume::tarSatzBasisname("/x/satz.img"), "/x/satz.img");
}

// ── Schutz und Sperren ───────────────────────────────────────────────────────

TEST(TarSatz, OffenerSatzIstGeschuetztUndNurMitFolgeteilenBearbeitbar) {
    Ordner o("tarsatz_schutz");
    std::string err;
    {
        auto v = DiskVolume::tarSatzNeu("k5601_ss40_16x256", o / "p.img", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        ASSERT_TRUE(einfuegen(*v, quelle(o, "g", muster(400000, 12)), "g"));
        ASSERT_TRUE(v->tarSatzSpeichern());
    }
    // Vorgabe: schreibgeschuetzt wie jedes fremde Abbild.
    auto w = oeffneSatz({o / "p1.img", o / "p2.img", o / "p3.img"});
    ASSERT_TRUE(w);
    EXPECT_TRUE(w->readOnly());
    EXPECT_FALSE(einfuegen(*w, quelle(o, "z", muster(10, 1)), "z"));
    w->setReadOnly(false);
    EXPECT_FALSE(w->readOnly());
    EXPECT_TRUE(einfuegen(*w, quelle(o, "z", muster(10, 1)), "z")) << w->lastError();

    // Nur das erste Abbild: die Endmarke fehlt, lesbar, aber nicht schreibbar (Hinweis im Grund).
    auto t = oeffneSatz({o / "p1.img"});
    ASSERT_TRUE(t);
    EXPECT_FALSE(t->list().empty());
    t->setReadOnly(false);
    EXPECT_TRUE(t->readOnly());
    EXPECT_NE(t->lastError().find("nicht abgeschlossen"), std::string::npos) << t->lastError();
}

TEST(TarSatz, FehlerhafteAenderungLaesstDenStromUnveraendert) {
    Ordner o("tarsatz_fehler");
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_9x512", o / "f.img", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    ASSERT_TRUE(einfuegen(*v, quelle(o, "ok", muster(700, 13)), "ok"));
    ASSERT_TRUE(v->tarSatzSpeichern());
    EXPECT_FALSE(v->dirty());
    const std::string lang(150, 'x');                       // V7: Namen hoechstens 100 Zeichen
    EXPECT_FALSE(einfuegen(*v, quelle(o, "l", muster(10, 2)), lang));
    EXPECT_FALSE(v->dirty()) << "ein abgelehnter Schritt darf den Satz nicht veraendern";
    EXPECT_EQ(namen(*v), (std::vector<std::string>{"ok"}));
}

TEST(TarSatz, PhysischSchreibenIstFuerSaetzeGesperrtMitBegruendung) {
    Ordner o("tarsatz_gw");
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_9x512", o / "g.img", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    const std::string grund = v->physischSchreibenGrund();
    EXPECT_FALSE(grund.empty());
    EXPECT_NE(grund.find("physische"), std::string::npos) << grund;
    EXPECT_NE(grund.find("Einzelabbilder"), std::string::npos) << grund;
    // Eine gewoehnliche Diskette hat diesen Grund nicht.
    const std::string p = o / "e.img";
    auto e = DiskVolume::create(p, "tar720_leer", "", formate(), dateisysteme(), err);
    ASSERT_TRUE(e) << err;
    EXPECT_TRUE(e->physischSchreibenGrund().empty());
    // Ein Satz ist kein einzelnes Abbild: saveAs/exportImage verweigern sich.
    EXPECT_FALSE(v->saveAs(o / "z.hfe"));
    EXPECT_FALSE(v->exportImage(o / "z.hfe"));
}

TEST(TarSatz, LeererSatzRundlauf) {
    Ordner o("tarsatz_leer");
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_9x512", o / "l.dmk", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    EXPECT_TRUE(v->list().empty());
    ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();
    EXPECT_TRUE(fs::exists(o / "l1.dmk"));
    auto w = oeffneSatz({o / "l1.dmk"});
    ASSERT_TRUE(w);
    EXPECT_TRUE(w->list().empty());
}

TEST(TarSatz, AttributeUndOrdnerImSatz) {
    Ordner o("tarsatz_attr");
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_ss40_16x256", o / "t.hfe", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    ASSERT_TRUE(v->makeDirectory(FileRef::parse("sub", 0))) << v->lastError();
    ASSERT_TRUE(einfuegen(*v, quelle(o, "f", muster(4000, 14)), "sub/f"));
    UnixAttrs u;
    u.set_mode = true; u.mode = 0600;
    u.set_uid = true;  u.uid = 42;
    ASSERT_TRUE(v->setAttributes(FileRef::parse("sub/f", 0), u)) << v->lastError();
    ASSERT_TRUE(v->tarSatzSpeichern());
    auto w = oeffneSatz({o / "t1.hfe"});
    ASSERT_TRUE(w);
    bool gefunden = false;
    for (const FileEntry& e : w->list())
        if (e.name == "sub/f") { gefunden = true; EXPECT_EQ(e.unix_uid, 42); EXPECT_NE(e.attributes.find("rw-------"), std::string::npos) << e.attributes; }
    EXPECT_TRUE(gefunden);
}
