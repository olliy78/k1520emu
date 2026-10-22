/**
 * @file test_tar_neu.cpp
 * @brief GoogleTests fuer Entwurf 37, AP T3: neue leere tar-Diskette/-Satz mit GEWAEHLTEM Format,
 *        Formatliste, vorgegebene Geometrie beim Oeffnen eines `.img`-Satzes und die
 *        Entscheidung "`.tar` als Datei oder als fertiger Container".
 *
 * @see core/filesystem/disk_volume.h (tarFormate/tarDisketteNeu/tarDateiBeurteilen/tarDateiAblegen),
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


namespace {

/// Ein V7-tar mit dem System-`tar` aus Dateien erzeugen; leer = nicht verfuegbar.
bool systemTar(const Ordner& o, const std::string& ziel, const std::vector<std::string>& dateien) {
    std::string cmd = "tar --format=v7 -cf '" + ziel + "' -C '" + o / "q" + "'";
    for (const auto& d : dateien) cmd += " '" + d + "'";
    return std::system(cmd.c_str()) == 0;
}

bool hatTar() { return std::system("tar --version > /dev/null 2>&1") == 0; }

const TarFormatInfo* finde(const std::vector<TarFormatInfo>& l, const std::string& n) {
    for (const auto& f : l) if (f.name == n) return &f;
    return nullptr;
}

}  // namespace

// ── Formatliste ──────────────────────────────────────────────────────────────

TEST(TarNeu, FormatlisteHatNurEinheitlicheFormateUndJedesLaesstSichAnlegen) {
    const auto l = DiskVolume::tarFormate(formate());
    ASSERT_FALSE(l.empty());
    EXPECT_LT(l.size(), formate().formats().size()) << "gemischte Formate (Systemspuren/Datenspuren) gehoeren nicht hinein";
    ASSERT_TRUE(finde(l, "k5601_9x512"));
    EXPECT_EQ(finde(l, "k5601_9x512")->bytes, 737280u);
    ASSERT_TRUE(finde(l, "k5601_ss40_16x256"));
    EXPECT_EQ(finde(l, "k5601_ss40_16x256")->bytes, 163840u);
    Ordner o("tarneu_formate");
    for (const auto& f : l) {
        EXPECT_GE(f.bytes, 1024u) << f.name;
        std::string err;
        const std::string ext = f.img ? ".img" : (f.hfe ? ".hfe" : ".dmk");
        auto v = DiskVolume::tarSatzNeu(f.name, o / ("x" + ext), formate(), dateisysteme(), err);
        EXPECT_TRUE(v) << f.name << ": " << err;
    }
}

// ── Neue leere Diskette im gewaehlten Format ─────────────────────────────────

TEST(TarNeu, LeereDisketteImGewaehltenFormatRundlauf) {
    Ordner o("tarneu_disk");
    std::string err;
    const std::string pfad = o / "klein.img";
    {
        auto v = DiskVolume::tarDisketteNeu(pfad, "k5601_ss40_16x256", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        EXPECT_TRUE(v->tarSatz());
        EXPECT_FALSE(v->readOnly());
        EXPECT_FALSE(v->dirty());
        // Die Datei gibt es sofort, mit genau diesem Namen, ganz Null.
        ASSERT_TRUE(fs::exists(pfad));
        EXPECT_EQ(fs::file_size(pfad), 163840u);
        for (uint8_t b : leseDatei(pfad)) ASSERT_EQ(b, 0u);

        const auto a = muster(50000, 11);
        ASSERT_TRUE(einfuegen(*v, quelle(o, "a.bin", a), "a.bin")) << v->lastError();
        const TarSatzPlan plan = v->tarSatzPlanen();
        ASSERT_TRUE(plan.ok) << plan.fehler;
        EXPECT_TRUE(plan.einzel);
        ASSERT_EQ(plan.namen.size(), 1u);
        EXPECT_EQ(plan.namen[0], pfad) << "ein Einzelabbild behaelt seinen Namen ohne Nummer";
        EXPECT_TRUE(plan.ueberschreibt.size() == 1u);          // die Nulldatei wird ersetzt ...
        std::vector<std::string> hinweise;
        ASSERT_TRUE(v->tarSatzSpeichern("", &hinweise)) << v->lastError();
        EXPECT_FALSE(fs::exists(pfad + "~")) << "... aber ohne Sicherungskopie einer Nulldatei";
    }
    EXPECT_EQ(fs::file_size(pfad), 163840u);
    std::string e2;
    auto w = DiskVolume::openTarSatz({pfad}, formate(), dateisysteme(), e2, "k5601_ss40_16x256");
    ASSERT_TRUE(w) << e2;
    EXPECT_EQ(namen(*w), (std::vector<std::string>{"a.bin"}));
    EXPECT_EQ(inhalt(*w, "a.bin"), muster(50000, 11));
    if (hatTar()) {                                             // die Datei IST ein tar-Strom
        EXPECT_EQ(std::system(("tar -tf '" + pfad + "' > /dev/null 2>&1").c_str()), 0);
    }
}

TEST(TarNeu, WaechstDerInhaltUeberEineDisketteWirdBeimSpeichernEinSatz) {
    Ordner o("tarneu_waechst");
    std::string err;
    const std::string pfad = o / "w.hfe";
    auto v = DiskVolume::tarDisketteNeu(pfad, "k5601_ss40_16x256", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    ASSERT_TRUE(einfuegen(*v, quelle(o, "gross", muster(300000, 12)), "gross")) << v->lastError();
    const TarSatzPlan plan = v->tarSatzPlanen();
    ASSERT_TRUE(plan.ok);
    EXPECT_EQ(plan.abbilder, 2);
    EXPECT_EQ(plan.namen[0], o / "w1.hfe");                    // jetzt nummeriert
    ASSERT_TRUE(v->tarSatzSpeichern());
    EXPECT_TRUE(fs::exists(o / "w1.hfe") && fs::exists(o / "w2.hfe"));
    auto w = oeffneSatz({o / "w1.hfe", o / "w2.hfe"});
    ASSERT_TRUE(w);
    EXPECT_EQ(inhalt(*w, "gross"), muster(300000, 12));
}

TEST(TarNeu, AbgelehnteFormateUndContainer) {
    Ordner o("tarneu_abgelehnt");
    std::string err;
    EXPECT_FALSE(DiskVolume::tarDisketteNeu(o / "x.img", "gibt_es_nicht", formate(), dateisysteme(), err));
    EXPECT_NE(err.find("Katalog"), std::string::npos) << err;
    EXPECT_FALSE(fs::exists(o / "x.img")) << "bei einem Fehler entsteht keine Datei";
    EXPECT_FALSE(DiskVolume::tarDisketteNeu(o / "x.txt", "k5601_9x512", formate(), dateisysteme(), err));
    EXPECT_FALSE(fs::exists(o / "x.txt"));
    // Ein gemischtes Format (nicht in der Liste) wird abgelehnt.
    const auto l = DiskVolume::tarFormate(formate());
    for (const DiskFormat& f : formate().formats()) {
        if (finde(l, f.name)) continue;
        EXPECT_FALSE(DiskVolume::tarDisketteNeu(o / "m.hfe", f.name, formate(), dateisysteme(), err)) << f.name;
        EXPECT_FALSE(fs::exists(o / "m.hfe"));
        break;
    }
}

TEST(TarNeu, OeffnenMitVorgegebenemFormatUndUnbekanntesFormat) {
    Ordner o("tarneu_vorgabe");
    std::string err;
    const std::string pfad = o / "g.img";
    {
        auto v = DiskVolume::tarDisketteNeu(pfad, "k5601_ss40_16x256", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        ASSERT_TRUE(einfuegen(*v, quelle(o, "n", muster(2000, 13)), "n"));
        ASSERT_TRUE(v->tarSatzSpeichern());
    }
    std::string e2;
    auto gut = DiskVolume::openTarSatz({pfad}, formate(), dateisysteme(), e2, "k5601_ss40_16x256");
    ASSERT_TRUE(gut) << e2;
    EXPECT_EQ(namen(*gut), (std::vector<std::string>{"n"}));
    std::string e3;
    EXPECT_FALSE(DiskVolume::openTarSatz({pfad}, formate(), dateisysteme(), e3, "unbekannt"));
}

// ── .tar: als Datei oder als fertiger Container ──────────────────────────────

TEST(TarNeu, TarAufLeereDisketteBietetBeideWegeAufNichtLeereNurDatei) {
    if (!hatTar()) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarneu_wahl");
    quelle(o, "x.bin", muster(3000, 14));
    ASSERT_TRUE(systemTar(o, o / "paket.tar", {"x.bin"}));
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_9x512", o / "z.img", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;

    TarDateiUrteil u = v->tarDateiBeurteilen(o / "paket.tar");
    ASSERT_TRUE(u.ok) << u.fehler;
    EXPECT_TRUE(u.ziel_leer);
    EXPECT_TRUE(u.als_datei);
    EXPECT_TRUE(u.als_container) << u.container_grund;
    EXPECT_FALSE(u.wird_satz);
    EXPECT_EQ(u.abbilder, 1);

    // Weg 1: als Datei.
    ASSERT_TRUE(v->tarDateiAblegen(o / "paket.tar", TarDateiWeg::AlsDatei)) << v->lastError();
    EXPECT_EQ(namen(*v), (std::vector<std::string>{"paket.tar"}));
    // Das Archiv ist nicht mehr leer: nur noch "als Datei".
    u = v->tarDateiBeurteilen(o / "paket.tar");
    ASSERT_TRUE(u.ok);
    EXPECT_FALSE(u.ziel_leer);
    EXPECT_FALSE(u.als_container);
    EXPECT_FALSE(u.container_grund.empty());
    EXPECT_FALSE(v->tarDateiAblegen(o / "paket.tar", TarDateiWeg::AlsContainer));
    // Gleichnamig ohne "ersetzen" abgelehnt, mit ersetzen angenommen.
    EXPECT_FALSE(v->tarDateiAblegen(o / "paket.tar", TarDateiWeg::AlsDatei));
    EXPECT_TRUE(v->tarDateiAblegen(o / "paket.tar", TarDateiWeg::AlsDatei, "", true)) << v->lastError();
    EXPECT_TRUE(v->tarDateiAblegen(o / "paket.tar", TarDateiWeg::AlsDatei, "dir/anders.tar")) << v->lastError();
}

TEST(TarNeu, ContainerWirdDasArchivEinesSatzes) {
    if (!hatTar()) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarneu_container");
    const auto a = muster(100000, 15), b = muster(90000, 16);
    quelle(o, "a.bin", a);
    quelle(o, "b.bin", b);
    ASSERT_TRUE(systemTar(o, o / "paket.tar", {"a.bin", "b.bin"}));
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_ss40_16x256", o / "c.img", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    ASSERT_TRUE(v->tarDateiAblegen(o / "paket.tar", TarDateiWeg::AlsContainer)) << v->lastError();
    EXPECT_EQ(namen(*v), (std::vector<std::string>{"a.bin", "b.bin"})) << "der Inhalt WIRD das Archiv";
    EXPECT_TRUE(v->dirty());
    ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();
    auto w = oeffneSatz({o / "c1.img", o / "c2.img"});
    ASSERT_TRUE(w);
    EXPECT_EQ(inhalt(*w, "b.bin"), b);
    // Weiter bearbeitbar: ein Container ist ein ganz normales abgeschlossenes Archiv.
    w->setReadOnly(false);
    EXPECT_TRUE(einfuegen(*w, quelle(o, "n", muster(100, 17)), "n")) << w->lastError();
}

TEST(TarNeu, ContainerUeberDisketteGroesseWirdEinSatz) {
    if (!hatTar()) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarneu_gross");
    std::vector<std::string> dateien;
    for (int i = 0; i < 3; ++i) {
        const std::string n = "d" + std::to_string(i);
        quelle(o, n, muster(300000, 20 + static_cast<uint32_t>(i)));
        dateien.push_back(n);
    }
    ASSERT_TRUE(systemTar(o, o / "gross.tar", dateien));
    // Eine Einzeldiskette (720K, frisch angelegt): der Container sprengt sie.
    std::string err;
    const std::string einzel = o / "e.img";
    auto v = DiskVolume::create(einzel, "tar720_leer", "", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    const TarDateiUrteil u = v->tarDateiBeurteilen(o / "gross.tar");
    ASSERT_TRUE(u.ok) << u.fehler;
    EXPECT_TRUE(u.als_container) << u.container_grund;
    EXPECT_TRUE(u.wird_satz);
    EXPECT_EQ(u.abbilder, 2);                                    // ~900 KB auf 720 KiB-Abbilder
    const auto vorher = leseDatei(einzel);
    ASSERT_TRUE(v->tarDateiAblegen(o / "gross.tar", TarDateiWeg::AlsContainer)) << v->lastError();
    EXPECT_TRUE(v->tarSatz());
    EXPECT_EQ(leseDatei(einzel), vorher) << "die Einzeldiskette bleibt bis zum Speichern unberuehrt";
    ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();
    auto w = oeffneSatz({o / "e1.img", o / "e2.img"});
    ASSERT_TRUE(w);
    EXPECT_EQ(namen(*w), dateien);
    EXPECT_EQ(inhalt(*w, "d2"), muster(300000, 22));
}

TEST(TarNeu, ContainerPasstAufEinzelDisketteGehtInDasselbeAbbild) {
    if (!hatTar()) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarneu_put");
    quelle(o, "k", muster(5000, 30));
    ASSERT_TRUE(systemTar(o, o / "k.tar", {"k"}));
    std::string err;
    const std::string einzel = o / "p.img";
    {
        auto v = DiskVolume::create(einzel, "tar720_leer", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        const TarDateiUrteil u = v->tarDateiBeurteilen(o / "k.tar");
        ASSERT_TRUE(u.als_container) << u.container_grund;
        EXPECT_FALSE(u.wird_satz);
        ASSERT_TRUE(v->tarDateiAblegen(o / "k.tar", TarDateiWeg::AlsContainer)) << v->lastError();
        EXPECT_TRUE(v->tarSatzEinzel());            // eine tar-Diskette ist ein Satz mit einem Abbild
        ASSERT_TRUE(v->flush()) << v->lastError();  // und geht in dasselbe Abbild zurueck
        EXPECT_FALSE(v->readOnly());
    }
    std::string e2;
    auto w = DiskVolume::open(einzel, "", formate(), dateisysteme(), e2, true);
    ASSERT_TRUE(w) << e2;
    EXPECT_EQ(namen(*w), (std::vector<std::string>{"k"}));
}

TEST(TarNeu, FolgeteilUndUnbrauchbareDateienSindKeinContainer) {
    Ordner o("tarneu_folgeteil");
    std::string err;
    auto v = DiskVolume::tarSatzNeu("k5601_ss40_16x256", o / "f.img", formate(), dateisysteme(), err);
    ASSERT_TRUE(v) << err;
    // Mitten in Daten: kein Kopf am Anfang.
    schreibeDatei(o / "teil.tar", muster(1024, 40));
    TarDateiUrteil u = v->tarDateiBeurteilen(o / "teil.tar");
    ASSERT_TRUE(u.ok);
    EXPECT_TRUE(u.als_datei);
    EXPECT_FALSE(u.als_container);
    EXPECT_FALSE(u.container_grund.empty());
    EXPECT_FALSE(v->tarDateiAblegen(o / "teil.tar", TarDateiWeg::AlsContainer));
    EXPECT_FALSE(v->lastError().empty());
    // Kein Vielfaches von 512.
    schreibeDatei(o / "krumm.tar", muster(700, 41));
    u = v->tarDateiBeurteilen(o / "krumm.tar");
    EXPECT_FALSE(u.als_container);
    // Datei fehlt.
    EXPECT_FALSE(v->tarDateiBeurteilen(o / "nix.tar").ok);
}

TEST(TarNeu, SchreibschutzUndKeinTarVolumeSindKlareAbsagen) {
    if (!hatTar()) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarneu_sperre");
    quelle(o, "k", muster(100, 50));
    ASSERT_TRUE(systemTar(o, o / "k.tar", {"k"}));
    std::string err;
    {
        auto v = DiskVolume::tarDisketteNeu(o / "s.img", "k5601_ss40_16x256", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
    }
    auto w = DiskVolume::openTarSatz({o / "s.img"}, formate(), dateisysteme(), err, "k5601_ss40_16x256");
    ASSERT_TRUE(w) << err;
    ASSERT_TRUE(w->readOnly());                                  // Vorgabe beim Oeffnen
    const TarDateiUrteil u = w->tarDateiBeurteilen(o / "k.tar");
    EXPECT_FALSE(u.ok);
    EXPECT_FALSE(u.fehler.empty());
    EXPECT_FALSE(w->tarDateiAblegen(o / "k.tar", TarDateiWeg::AlsDatei));
    // Ein Satz laesst sich nicht direkt auf eine physische Diskette schreiben.
    EXPECT_FALSE(w->physischSchreibenGrund().empty());
}

// ── Vereinheitlichung: eine tar-Diskette ist immer ein Satz mit N >= 1 Abbildern ────────────

/// @test Auf einer LEEREN Einzeldiskette bieten sich BEIDE Wege an; jeder fuehrt zum erwarteten Archiv.
TEST(TarNeu, LeereEinzelDisketteBietetBeideWegeUndJederFuehrtZumArchiv) {
    if (!hatTar()) GTEST_SKIP() << "kein tar im System";
    Ordner o("tarneu_einzel_beide");
    quelle(o, "x.bin", muster(3000, 60));
    ASSERT_TRUE(systemTar(o, o / "paket.tar", {"x.bin"}));
    std::string err;
    for (const bool als_container : {false, true}) {
        SCOPED_TRACE(als_container ? "Container" : "Datei");
        const std::string pfad = o / (als_container ? "c.hfe" : "d.hfe");
        {
            auto v = DiskVolume::create(pfad, "tar720_leer", "", formate(), dateisysteme(), err);
            ASSERT_TRUE(v) << err;
            const TarDateiUrteil u = v->tarDateiBeurteilen(o / "paket.tar");
            ASSERT_TRUE(u.ok) << u.fehler;
            EXPECT_TRUE(u.ziel_leer);
            EXPECT_TRUE(u.als_datei);
            EXPECT_TRUE(u.als_container) << u.container_grund;
            EXPECT_FALSE(u.wird_satz);
            ASSERT_TRUE(v->tarDateiAblegen(o / "paket.tar",
                        als_container ? TarDateiWeg::AlsContainer : TarDateiWeg::AlsDatei)) << v->lastError();
            ASSERT_TRUE(v->flush()) << v->lastError();
        }
        auto w = DiskVolume::open(pfad, "", formate(), dateisysteme(), err, true, true);
        ASSERT_TRUE(w) << err;
        EXPECT_EQ(namen(*w), als_container ? std::vector<std::string>{"x.bin"}
                                           : std::vector<std::string>{"paket.tar"});
        EXPECT_EQ(w->containerName(), "hfe");
    }
}

/// @test Eine geoeffnete tar-Diskette ist ein Satz mit EINEM Abbild: Aenderungen gehen beim Speichern in
///       DASSELBE Abbild (Container bleibt), `<name>~` genau einmal je Sitzung; dirty folgt dem Strom.
TEST(TarNeu, GeoeffneteTarDisketteSchreibtInsSelbeAbbildZurueck) {
    Ordner o("tarneu_inplace");
    std::string err;
    const std::string pfad = o / "ariane1.cqm";
    {
        auto v = DiskVolume::create(pfad, "tar720_leer", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        ASSERT_TRUE(einfuegen(*v, quelle(o, "a", muster(2000, 61)), "a")) << v->lastError();
        ASSERT_TRUE(v->flush()) << v->lastError();
    }
    auto v = DiskVolume::open(pfad, "", formate(), dateisysteme(), err, false, true);
    ASSERT_TRUE(v) << err;
    EXPECT_TRUE(v->tarSatz());
    EXPECT_TRUE(v->tarSatzEinzel());
    EXPECT_FALSE(v->readOnly());
    EXPECT_FALSE(v->dirty());
    ASSERT_TRUE(einfuegen(*v, quelle(o, "b", muster(3000, 62)), "b")) << v->lastError();
    EXPECT_TRUE(v->dirty());
    const TarSatzPlan plan = v->tarSatzPlanen();
    ASSERT_TRUE(plan.ok) << plan.fehler;
    ASSERT_EQ(plan.namen.size(), 1u);
    EXPECT_EQ(plan.namen[0], pfad) << "ohne Nummer, solange der Inhalt auf EIN Abbild passt";
    ASSERT_TRUE(v->flush()) << v->lastError();
    EXPECT_FALSE(v->dirty());
    EXPECT_TRUE(fs::exists(pfad + "~")) << "die erste Aenderung sichert das Fremdabbild";
    const auto sicherung = leseDatei(pfad + "~");
    ASSERT_TRUE(einfuegen(*v, quelle(o, "c", muster(100, 63)), "c")) << v->lastError();
    ASSERT_TRUE(v->flush()) << v->lastError();
    EXPECT_EQ(leseDatei(pfad + "~"), sicherung) << "die Sicherung entsteht nur einmal je Sitzung";
    v.reset();
    auto w = DiskVolume::open(pfad, "", formate(), dateisysteme(), err, true, true);
    ASSERT_TRUE(w) << err;
    EXPECT_EQ(w->containerName(), "cqm");
    EXPECT_EQ(namen(*w), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(inhalt(*w, "b"), muster(3000, 62));
}

/// @test Wuchs der Inhalt ueber ein Abbild, wird beim Speichern nummeriert (Ziffern am Namensende
///       entfallen); danach ist es ein echter Satz.  Folgeteile bleiben unveraendert nur lesbar.
TEST(TarNeu, GeoeffneteTarDisketteWaechstZumSatz) {
    Ordner o("tarneu_waechst_offen");
    std::string err;
    const std::string pfad = o / "ariane1.img";
    {
        auto v = DiskVolume::create(pfad, "tar720_leer", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        ASSERT_TRUE(v->flush());
    }
    auto v = DiskVolume::open(pfad, "", formate(), dateisysteme(), err, false, true);
    ASSERT_TRUE(v) << err;
    ASSERT_TRUE(einfuegen(*v, quelle(o, "gross", muster(900000, 64)), "gross")) << v->lastError();
    const TarSatzPlan plan = v->tarSatzPlanen();
    ASSERT_TRUE(plan.ok) << plan.fehler;
    EXPECT_EQ(plan.abbilder, 2);
    EXPECT_EQ(plan.namen, (std::vector<std::string>{o / "ariane1.img", o / "ariane2.img"}));
    EXPECT_FALSE(v->flush());
    ASSERT_TRUE(v->tarSatzSpeichern()) << v->lastError();
    EXPECT_FALSE(v->tarSatzEinzel());
    auto w = oeffneSatz({o / "ariane1.img", o / "ariane2.img"});
    ASSERT_TRUE(w);
    EXPECT_EQ(inhalt(*w, "gross"), muster(900000, 64));

    // Ein Folgeteil (Teil 2 allein) ist KEIN Satz mit einem Abbild: lesbar, nicht bearbeitbar.
    auto teil2 = DiskVolume::open(o / "ariane2.img", "", formate(), dateisysteme(), err, false, true);
    ASSERT_TRUE(teil2) << err;
    EXPECT_FALSE(teil2->tarSatz());
    EXPECT_TRUE(!teil2->hasFileSystem() || teil2->readOnlyForced());
}

/// @test Eine tar-Diskette lebt als Strom im Speicher: Sektoreingriffe am Medium werden abgelehnt (sie
///       gingen beim Speichern verloren); Export des Abbilds / Speichern unter gehen nur im gespeicherten
///       Zustand und binden die Diskette an den neuen Namen.
TEST(TarNeu, TarDisketteSektorenSperreUndSpeichernUnter) {
    Ordner o("tarneu_sektor");
    std::string err;
    const std::string pfad = o / "s.img";
    {
        auto v = DiskVolume::create(pfad, "tar720_leer", "", formate(), dateisysteme(), err);
        ASSERT_TRUE(v) << err;
        ASSERT_TRUE(einfuegen(*v, quelle(o, "a", muster(2000, 70)), "a")) << v->lastError();
        ASSERT_TRUE(v->flush()) << v->lastError();
    }
    auto v = DiskVolume::open(pfad, "", formate(), dateisysteme(), err, false, true);
    ASSERT_TRUE(v) << err;
    EXPECT_FALSE(v->writeSectorAt(0, 0, 0, std::vector<uint8_t>(512, 1), nullptr));
    EXPECT_NE(v->lastError().find("Strom im Speicher"), std::string::npos) << v->lastError();

    ASSERT_TRUE(einfuegen(*v, quelle(o, "b", muster(100, 71)), "b")) << v->lastError();
    EXPECT_FALSE(v->saveAs(o / "u.hfe")) << "mit ungespeicherten Aenderungen: erst speichern";
    EXPECT_FALSE(v->exportImage(o / "u.hfe"));
    ASSERT_TRUE(v->flush()) << v->lastError();
    ASSERT_TRUE(v->exportImage(o / "x.hfe")) << v->lastError();      // gespeichert: geht, ohne umzubinden
    EXPECT_EQ(v->path(), pfad);
    ASSERT_TRUE(v->saveAs(o / "u.hfe")) << v->lastError();           // und bindet an den neuen Namen
    EXPECT_EQ(v->path(), o / "u.hfe");
    ASSERT_TRUE(einfuegen(*v, quelle(o, "c", muster(100, 72)), "c")) << v->lastError();
    ASSERT_TRUE(v->flush()) << v->lastError();                        // schreibt in u.hfe, nicht in s.img
    auto w = DiskVolume::open(o / "u.hfe", "", formate(), dateisysteme(), err, true, true);
    ASSERT_TRUE(w) << err;
    EXPECT_EQ(namen(*w), (std::vector<std::string>{"a", "b", "c"}));
    auto alt = DiskVolume::open(pfad, "", formate(), dateisysteme(), err, true, true);
    ASSERT_TRUE(alt) << err;
    EXPECT_EQ(namen(*alt), (std::vector<std::string>{"a", "b"}));
}
