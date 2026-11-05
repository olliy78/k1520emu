/**
 * @file test_bs600_check.cpp
 * @brief Pruefung (fsck), Reparatur, Rettung und Urlader-Zuordnung des BS600-Dateisystems (Entwurf 48, AP-B4/B5).
 *
 * Reihenfolge wie bei den anderen Dateisystemen: ZUERST der Waechter gegen Falschmeldungen (beide Originale muessen
 * ohne Befund pruefen), dann je Schaden ein Fall, dann die Reparaturen, dann die Rettung.  Beschaedigt wird ueber
 * DiskVolume::writeSectorAt - derselbe Weg, den der Diskeditor nimmt.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/filesystem/bs600/bs600_fs.h"
#include "core/filesystem/check/fs_check.h"
#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;

namespace {

std::string fixture(const char* name) { return (fs::path(FIXTURE_DIR) / ".." / "bs600" / name).string(); }

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

std::unique_ptr<DiskVolume> neu(const std::string& name) {
    std::string err;
    const std::string pfad = k1520test::tempPath(name);
    fs::remove(pfad);
    auto v = DiskVolume::create(pfad, "bs600_ss40_15x256_boot", "TEST", formate(), dateisysteme(), err);
    EXPECT_TRUE(v) << err;
    return v;
}

std::unique_ptr<DiskVolume> kopieOeffnen(const char* fix, const std::string& als, bool ro) {
    const std::string k = k1520test::tempPath(als);
    fs::copy_file(fixture(fix), k, fs::copy_options::overwrite_existing);
    std::string err;
    auto v = DiskVolume::open(k, "", formate(), dateisysteme(), err, ro);
    EXPECT_TRUE(v) << err;
    return v;
}

std::vector<uint8_t> muster(size_t n, uint8_t seed) {
    std::vector<uint8_t> d(n);
    for (size_t i = 0; i < n; ++i) d[i] = static_cast<uint8_t>(seed + i * 7);
    return d;
}

Bs600FileSystem& bfs(DiskVolume& v) { return *v.bs600Fs(); }

bool ablegen(DiskVolume& v, const std::string& name, const std::vector<uint8_t>& d, uint16_t lade = 0x2A00) {
    WriteOptions wo;
    wo.bs600_load = lade;
    wo.bs600_author = "TST";
    wo.bs600_date = "041124";
    return bfs(v).write(name, d, wo);
}

std::string kennungen(const FsCheckReport& r) {
    std::string s;
    for (const FsFinding& f : r.findings)
        s += (s.empty() ? "" : "; ") + f.id + " (" + fsSeverityName(f.severity) + ": " + f.text + ")";
    return s;
}

std::vector<FsFinding> mitId(const FsCheckReport& r, const std::string& id) {
    std::vector<FsFinding> out;
    for (const FsFinding& f : r.findings) if (f.id == id) out.push_back(f);
    return out;
}

/// Sektor n (1-basiert) der Spur 0 lesen/schreiben.
std::vector<uint8_t> lies0(DiskVolume& v, int sektor) {
    std::vector<uint8_t> d;
    uint16_t crc = 0;
    EXPECT_TRUE(v.readSectorAt(0, 0, sektor - 1, d, crc));
    return d;
}
void schreib0(DiskVolume& v, int sektor, const std::vector<uint8_t>& d) {
    ASSERT_TRUE(v.writeSectorAt(0, 0, sektor - 1, d, nullptr));
}

/// Einen 42-Byte-Eintrag auf Platz p in einem Verzeichnissektor aendern.
void eintragAendern(DiskVolume& v, int platz, const std::function<void(uint8_t*)>& f) {
    const int sek = 8 + platz / 3;
    auto d = lies0(v, sek);
    f(d.data() + (platz % 3) * 42);
    schreib0(v, sek, d);
}

}  // namespace

// ═══ 1. Keine Falschmeldungen ═════════════════════════════════════════════════

/// @test Beide Originalabzuege pruefen ohne Befund; PRG 700 nur mit dem Hinweis auf seinen Urlader.
TEST(Bs600Check, DieOriginaleHabenKeinenBefund) {
    for (const char* name : {"bs620_prg710-1.hfe", "bs600_prg700.hfe"}) {
        auto v = kopieOeffnen(name, std::string("bs600chk_") + name, /*ro=*/true);
        ASSERT_TRUE(v) << name;
        for (FsCheckLevel stufe : {FsCheckLevel::Schnell, FsCheckLevel::Voll}) {
            const FsCheckReport& r = v->check(stufe, true);
            EXPECT_TRUE(r.vollstaendig) << name;
            EXPECT_EQ(0, r.zaehlerAb(FsSeverity::Warnung)) << name << ": " << kennungen(r);
            for (const FsFinding& f : r.findings)
                EXPECT_EQ(f.id, "bs600.urlader.prg700") << name << ": " << kennungen(r);
        }
        EXPECT_FALSE(v->dirty()) << name << ": die Pruefung hat geschrieben";
    }
}

/// @test Die Schnellpruefung fasst nur Spur 0 an; die Vollpruefung liest die Dateispuren.
TEST(Bs600Check, SchnellpruefungBleibtAufSpurNull) {
    auto v = kopieOeffnen("bs620_prg710-1.hfe", "bs600chk_spuren.hfe", true);
    ASSERT_TRUE(v);
    const FsCheckReport& s = v->check(FsCheckLevel::Schnell, true);
    EXPECT_EQ(1, s.spuren_gesamt);
    EXPECT_EQ(1, s.spuren_gelesen);
    const FsCheckReport& f = v->check(FsCheckLevel::Voll, true);
    EXPECT_EQ(f.spuren_gelesen, f.spuren_gesamt);
    EXPECT_GT(f.spuren_gesamt, 20);          // Spur 0 + die belegten Spuren 1..26
}

/// @test Eine frisch angelegte Diskette mit Dateien, Loeschung und Reorganisation prueft sauber.
TEST(Bs600Check, AngelegteDisketteIstSauber) {
    auto v = neu("bs600chk_neu.hfe");
    ASSERT_TRUE(v);
    EXPECT_EQ(0, v->check(FsCheckLevel::Voll, true).zaehlerAb(FsSeverity::Info)) << "leer";
    ASSERT_TRUE(ablegen(*v, "A", muster(700, 1)));
    ASSERT_TRUE(ablegen(*v, "B", muster(5000, 2), 0x3000));
    ASSERT_TRUE(ablegen(*v, "C", muster(256, 3)));
    {
        const FsCheckReport& r1 = v->check(FsCheckLevel::Voll, true);
        EXPECT_EQ(0, r1.zaehlerAb(FsSeverity::Info)) << kennungen(r1);
    }
    ASSERT_TRUE(bfs(*v).erase("B"));
    // Die Luecke ist ein Hinweis, kein Mangel.
    const FsCheckReport& r = v->check(FsCheckLevel::Voll, true);
    EXPECT_EQ(0, r.zaehlerAb(FsSeverity::Warnung)) << kennungen(r);
    ASSERT_EQ(1u, mitId(r, "bs600.luecke").size()) << kennungen(r);
    EXPECT_EQ(FsSeverity::Info, mitId(r, "bs600.luecke")[0].severity);
    ASSERT_TRUE(bfs(*v).reorganize(nullptr));
    {
        const FsCheckReport& r2 = v->check(FsCheckLevel::Voll, true);
        EXPECT_EQ(0, r2.zaehlerAb(FsSeverity::Info)) << kennungen(r2);
    }
}

// ═══ 2. Schaeden ══════════════════════════════════════════════════════════════

/// @test Die Belegungstabelle vergisst eine Datei: Gefahr, Reparatur ohne Datenverlust ist empfohlen.
TEST(Bs600CheckSchaden, TabelleFehltIstGefahr) {
    auto v = neu("bs600chk_tfehlt.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(1000, 1)));
    ASSERT_TRUE(ablegen(*v, "B", muster(1000, 2)));
    auto t = lies0(*v, 26);
    t[0] = 0x00; t[1] = 0x00;                    // Spur 1 (A und B liegen darin) in der Tabelle leer
    schreib0(*v, 26, t);
    const FsCheckReport& r = v->check(FsCheckLevel::Schnell, true);
    const auto f = mitId(r, "bs600.tabelle.fehlt");
    ASSERT_EQ(2u, f.size()) << kennungen(r);
    for (const FsFinding& x : f) {
        EXPECT_EQ(FsSeverity::Gefahr, x.severity);
        ASSERT_EQ(1u, x.repairs.size());
        EXPECT_TRUE(x.repairs[0].empfohlen);
        EXPECT_FALSE(x.repairs[0].datenverlust);
    }
    // Reparieren: nur Eintrag der ersten Datei waehlen - die Reparatur rechnet die ganze Tabelle.
    int fi = -1;
    for (size_t i = 0; i < r.findings.size(); ++i) if (r.findings[i].id == "bs600.tabelle.fehlt") { fi = static_cast<int>(i); break; }
    ASSERT_GE(fi, 0);
    EXPECT_EQ(1, v->applyRepairs({{fi, 0}})) << v->lastError();
    EXPECT_EQ(0, v->check(FsCheckLevel::Schnell, true).zaehlerAb(FsSeverity::Info)) << kennungen(v->check(FsCheckLevel::Schnell, true));
    std::vector<uint8_t> d;
    ASSERT_TRUE(bfs(*v).read("A", d));
    EXPECT_EQ(d, muster(1000, 1));
}

/// @test Belegte Bits ohne Datei: Warnung; das Freigeben ist NICHT vorausgewaehlt, aber moeglich.
TEST(Bs600CheckSchaden, TabelleUeberzaehligIstVerlorenerPlatz) {
    auto v = neu("bs600chk_tueber.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(256, 1)));
    auto t = lies0(*v, 26);
    t[2 * 4] = 0xFF; t[2 * 4 + 1] = 0xFE;        // Spur 5 ganz belegt
    schreib0(*v, 26, t);
    const FsCheckReport& r = v->check(FsCheckLevel::Schnell, true);
    const auto f = mitId(r, "bs600.tabelle.ueberzaehlig");
    ASSERT_EQ(1u, f.size()) << kennungen(r);
    EXPECT_EQ(FsSeverity::Warnung, f[0].severity);
    EXPECT_EQ(5, f[0].cyl);
    ASSERT_EQ(1u, f[0].repairs.size());
    EXPECT_FALSE(f[0].repairs[0].empfohlen);
    EXPECT_FALSE(f[0].repairs[0].gesperrt);
    int fi = 0;
    for (size_t i = 0; i < r.findings.size(); ++i) if (r.findings[i].id == "bs600.tabelle.ueberzaehlig") fi = static_cast<int>(i);
    EXPECT_EQ(1, v->applyRepairs({{fi, 0}})) << v->lastError();
    EXPECT_EQ(0, v->check(FsCheckLevel::Schnell, true).zaehlerAb(FsSeverity::Info));
}

/// @test Zwei Dateien auf denselben Sektoren: Gefahr, keine Reparatur (der Mensch muss entscheiden).
TEST(Bs600CheckSchaden, UeberlappungIstGefahr) {
    auto v = neu("bs600chk_ueber.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(1000, 1)));
    ASSERT_TRUE(ablegen(*v, "B", muster(1000, 2)));
    eintragAendern(*v, 1, [](uint8_t* p) { p[7] = 1; p[8] = 3; });       // B beginnt in A (Sektor 3)
    const FsCheckReport& r = v->check(FsCheckLevel::Schnell, true);
    const auto f = mitId(r, "bs600.eintrag.ueberlappt");
    ASSERT_EQ(1u, f.size()) << kennungen(r);
    EXPECT_EQ(FsSeverity::Gefahr, f[0].severity);
    EXPECT_TRUE(f[0].repairs.empty());
}

TEST(Bs600CheckSchaden, NamenLageTypAdresseDatum) {
    auto v = neu("bs600chk_felder.hfe");
    ASSERT_TRUE(v);
    for (const char* n : {"A", "B", "C", "D", "E", "F", "G"}) ASSERT_TRUE(ablegen(*v, n, muster(300, 1)));
    eintragAendern(*v, 1, [](uint8_t* p) { std::fill(p, p + 6, 0x20); });                     // B: Name fehlt
    eintragAendern(*v, 2, [](uint8_t* p) { p[0] = 'A'; });                                    // C heisst jetzt A: doppelt
    eintragAendern(*v, 3, [](uint8_t* p) { p[9] = 60; });                                     // D: Endspur 60
    eintragAendern(*v, 4, [](uint8_t* p) { p[6] = 'X'; });                                    // E: Typ X
    eintragAendern(*v, 5, [](uint8_t* p) { p[24] = '0'; p[25] = '0'; p[26] = '0'; p[27] = '1'; });   // F: Ende 0001 < Anfang 2A00
    eintragAendern(*v, 6, [](uint8_t* p) { p[11] = '9'; p[12] = '9'; p[13] = '1'; p[14] = '3'; });    // G: Datum 99 13 ..
    const FsCheckReport& r = v->check(FsCheckLevel::Schnell, true);
    EXPECT_EQ(1u, mitId(r, "bs600.name.fehlt").size()) << kennungen(r);
    EXPECT_EQ(1u, mitId(r, "bs600.name.doppelt").size()) << kennungen(r);
    EXPECT_EQ(1u, mitId(r, "bs600.eintrag.lage").size()) << kennungen(r);
    EXPECT_EQ(1u, mitId(r, "bs600.eintrag.typ").size()) << kennungen(r);
    EXPECT_EQ(1u, mitId(r, "bs600.adresse.ende").size()) << kennungen(r);
    const auto d = mitId(r, "bs600.datum.unplausibel");
    ASSERT_EQ(1u, d.size()) << kennungen(r);
    EXPECT_EQ(FsSeverity::Info, d[0].severity);
    // Reparaturen, die einen Verweis verwerfen, sind NIE vorausgewaehlt.
    for (const FsFinding& f : r.findings)
        for (const FsRepair& rep : f.repairs)
            if (rep.datenverlust) EXPECT_FALSE(rep.empfohlen) << f.id;
    // Der Eintrag mit unmoeglicher Lage laesst sich loeschen - das Freigeben der Tabelle ist dann gesperrt.
    const auto l = mitId(r, "bs600.eintrag.lage");
    ASSERT_EQ(1u, l[0].repairs.size());
    EXPECT_TRUE(l[0].repairs[0].datenverlust);
}

TEST(Bs600CheckSchaden, EndeVorAnfangUndNutzspur) {
    auto v = neu("bs600chk_lage.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(2000, 1)));
    ASSERT_TRUE(ablegen(*v, "B", muster(300, 2)));
    eintragAendern(*v, 0, [](uint8_t* p) { p[9] = 1; p[10] = 1; p[8] = 5; });                 // A: Ende 1/1 vor Anfang 1/5
    eintragAendern(*v, 1, [](uint8_t* p) { p[9] = 38; p[10] = 1; p[7] = 38; p[8] = 1; });    // B: auf Spur 38
    const FsCheckReport& r = v->check(FsCheckLevel::Schnell, true);
    EXPECT_EQ(1u, mitId(r, "bs600.eintrag.ende").size()) << kennungen(r);
    EXPECT_EQ(1u, mitId(r, "bs600.eintrag.nutzspur").size()) << kennungen(r);
}

TEST(Bs600CheckSchaden, AdressenUndSektorzahl) {
    auto v = neu("bs600chk_laenge.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(600, 1)));        // 3 Sektoren, 2A00..2C57
    ASSERT_TRUE(ablegen(*v, "B", muster(600, 2)));
    eintragAendern(*v, 0, [](uint8_t* p) { p[24] = '2'; p[25] = 'F'; p[26] = 'F'; p[27] = 'F'; });   // A: Adressen nennen 6 Sektoren
    eintragAendern(*v, 1, [](uint8_t* p) { p[24] = '2'; p[25] = 'A'; p[26] = '0'; p[27] = 'F'; });   // B: Adressen nennen 1 Sektor
    const auto f = mitId(v->check(FsCheckLevel::Schnell, true), "bs600.adresse.laenge");
    ASSERT_EQ(2u, f.size());
    int fehler = 0, warnung = 0;
    for (const FsFinding& x : f) { fehler += x.severity == FsSeverity::Fehler; warnung += x.severity == FsSeverity::Warnung; }
    EXPECT_EQ(1, fehler);        // Adressen verlangen MEHR als die Lage
    EXPECT_EQ(1, warnung);
}

TEST(Bs600CheckSchaden, KennsatzUndUnlesbareDatei) {
    auto v = neu("bs600chk_kopf.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(256, 1)));
    auto k = lies0(*v, 7);
    k[11] = 'X';
    schreib0(*v, 7, k);
    EXPECT_EQ(1u, mitId(v->check(FsCheckLevel::Schnell, true), "bs600.kennsatz.prg").size());
    k[0] = 0; k[1] = 0;
    schreib0(*v, 7, k);
    EXPECT_EQ(1u, mitId(v->check(FsCheckLevel::Schnell, true), "bs600.kennsatz.fehlt").size());

    // Defekte CRC im ersten Datensektor von A (Spur 1 Sektor 1): nur die Vollpruefung sieht es.
    std::vector<uint8_t> d;
    uint16_t crc = 0;
    ASSERT_TRUE(v->readSectorAt(1, 0, 0, d, crc));
    const uint16_t schlecht = static_cast<uint16_t>(crc ^ 0xFFFF);
    ASSERT_TRUE(v->writeSectorAt(1, 0, 0, d, &schlecht));
    EXPECT_TRUE(mitId(v->check(FsCheckLevel::Schnell, true), "bs600.datei.unlesbar").empty());
    const auto u = mitId(v->check(FsCheckLevel::Voll, true), "bs600.datei.unlesbar");
    ASSERT_EQ(1u, u.size());
    EXPECT_EQ(FsLayer::Medium, u[0].layer);
    EXPECT_EQ(1, u[0].cyl);
}

// ═══ 3. Reparatur ═════════════════════════════════════════════════════════════

TEST(Bs600CheckReparatur, UnbrauchbarerEintragWirdGeloeschtDieDateienBleiben) {
    auto v = neu("bs600chk_rep.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(700, 1)));
    ASSERT_TRUE(ablegen(*v, "B", muster(700, 2)));
    eintragAendern(*v, 1, [](uint8_t* p) { p[9] = 60; });
    const FsCheckReport& r = v->check(FsCheckLevel::Schnell, true);
    int fi = -1;
    for (size_t i = 0; i < r.findings.size(); ++i) if (r.findings[i].id == "bs600.eintrag.lage") fi = static_cast<int>(i);
    ASSERT_GE(fi, 0);
    EXPECT_EQ(1, v->applyRepairs({{fi, 0}})) << v->lastError();
    ASSERT_EQ(1u, v->list().size());
    std::vector<uint8_t> d;
    ASSERT_TRUE(bfs(*v).read("A", d));
    EXPECT_EQ(d, muster(700, 1));
    // Die Tabelle fuehrt B's Sektoren weiter als belegt: ein Hinweis auf verlorenen Platz, nichts geriet durcheinander.
    EXPECT_EQ(1u, mitId(v->check(FsCheckLevel::Schnell, true), "bs600.tabelle.ueberzaehlig").size());
}

TEST(Bs600CheckReparatur, FreigebenIstBeiUnbrauchbaremEintragGesperrt) {
    auto v = neu("bs600chk_sperre.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(256, 1)));
    ASSERT_TRUE(ablegen(*v, "B", muster(256, 2)));
    eintragAendern(*v, 1, [](uint8_t* p) { p[9] = 60; });
    const FsCheckReport& r = v->check(FsCheckLevel::Schnell, true);
    // B's Sektor steht noch in der Tabelle, ist aber keiner gueltigen Datei zugeordnet -> ueberzaehlig, gesperrt.
    const auto f = mitId(r, "bs600.tabelle.ueberzaehlig");
    ASSERT_EQ(1u, f.size()) << kennungen(r);
    EXPECT_TRUE(f[0].repairs[0].gesperrt);
}

TEST(Bs600CheckReparatur, ReorganisierenUeberDieReparatur) {
    auto v = neu("bs600chk_reorg.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(300, 1)));
    ASSERT_TRUE(ablegen(*v, "B", muster(300, 2)));
    ASSERT_TRUE(ablegen(*v, "C", muster(300, 3)));
    ASSERT_TRUE(bfs(*v).erase("A"));
    const FsCheckReport& r = v->check(FsCheckLevel::Schnell, true);
    int fi = -1;
    for (size_t i = 0; i < r.findings.size(); ++i) if (r.findings[i].id == "bs600.luecke") fi = static_cast<int>(i);
    ASSERT_GE(fi, 0) << kennungen(r);
    EXPECT_FALSE(r.findings[static_cast<size_t>(fi)].repairs[0].empfohlen);
    EXPECT_EQ(1, v->applyRepairs({{fi, 0}})) << v->lastError();
    {
        const FsCheckReport& r3 = v->check(FsCheckLevel::Schnell, true);
        EXPECT_EQ(0, r3.zaehlerAb(FsSeverity::Info)) << kennungen(r3);
    }
    std::vector<uint8_t> d;
    ASSERT_TRUE(bfs(*v).read("C", d));
    EXPECT_EQ(d, muster(300, 3));
}

// ═══ 4. Rettung ═══════════════════════════════════════════════════════════════

/// @test Nach dem Loeschen bleibt der Inhalt: die Oberflaechensuche findet ihn als Bruchstueck, ohne Namen.
TEST(Bs600Recover, GeloeschterInhaltWirdAlsRohbereichGefunden) {
    auto v = neu("bs600rec_geloescht.hfe");
    ASSERT_TRUE(v);
    const auto a = muster(700, 5);
    ASSERT_TRUE(ablegen(*v, "KEEP", muster(256, 1)));
    ASSERT_TRUE(ablegen(*v, "WEG", a));
    ASSERT_TRUE(bfs(*v).erase("WEG"));

    // Stufe Verzeichnis: nichts - die Tabelle fuehrt die Sektoren nicht mehr, der Name ist verloren.
    EXPECT_TRUE(v->recoverScan(FsRecoverLevel::Verzeichnis, true).leer());

    const FsRecoverReport& r = v->recoverScan(FsRecoverLevel::Oberflaeche, true);
    ASSERT_EQ(1u, r.funde.size()) << r.alsText();
    const FsRecoverFind& f = r.funde[0];
    EXPECT_EQ(FsRecoverQuality::Bruchstueck, f.quality);
    EXPECT_FALSE(f.wiederherstellbar);
    EXPECT_FALSE(f.warum_nicht.empty());
    EXPECT_TRUE(f.name.empty());
    EXPECT_EQ(3u * 256u, f.size);
    std::vector<uint8_t> roh;
    ASSERT_TRUE(v->recoverRead(0, roh));
    ASSERT_GE(roh.size(), a.size());
    EXPECT_TRUE(std::equal(a.begin(), a.end(), roh.begin()));
    // Wiederherstellen eines Eintrags ist nicht moeglich und sagt warum.
    EXPECT_FALSE(bfs(*v).recoverRestore(f, "WEG"));
    EXPECT_NE(bfs(*v).lastError().find("Name"), std::string::npos);
    // Gesucht und gelesen wurde nur - die Diskette ist unveraendert (der Befund ist derselbe wie zuvor).
    EXPECT_EQ(1u, v->list().size());
}

/// @test Belegte Sektoren ohne Eintrag erkennt schon die Stufe Verzeichnis (Guete wahrscheinlich).
TEST(Bs600Recover, BelegteSektorenOhneEintrag) {
    auto v = neu("bs600rec_tabelle.hfe");
    ASSERT_TRUE(v);
    ASSERT_TRUE(ablegen(*v, "A", muster(256, 1)));
    ASSERT_TRUE(ablegen(*v, "B", muster(512, 2)));
    eintragAendern(*v, 1, [](uint8_t* p) { std::fill(p, p + 42, 0x20); });        // Eintrag B weg, Tabelle behaelt ihn
    const FsRecoverReport& r = v->recoverScan(FsRecoverLevel::Verzeichnis, true);
    ASSERT_EQ(1u, r.funde.size()) << r.alsText();
    EXPECT_EQ(FsRecoverQuality::Wahrscheinlich, r.funde[0].quality);
    EXPECT_EQ(512u, r.funde[0].size);
    EXPECT_EQ(1, r.funde[0].cyl);
    EXPECT_EQ(2, r.funde[0].sector_index);
    std::vector<uint8_t> roh;
    ASSERT_TRUE(v->recoverRead(0, roh));
    EXPECT_EQ(roh, muster(512, 2));
}

TEST(Bs600Recover, OriginaleHabenNichtsZuRetten) {
    auto v = kopieOeffnen("bs620_prg710-1.hfe", "bs600rec_orig.hfe", true);
    ASSERT_TRUE(v);
    const FsRecoverReport& r = v->recoverScan(FsRecoverLevel::Verzeichnis, true);
    EXPECT_TRUE(r.leer()) << r.alsText();
}

// ═══ 5. Urlader ═══════════════════════════════════════════════════════════════

TEST(Bs600Urlader, KennungUndGeraetPassenZusammen) {
    auto a = kopieOeffnen("bs620_prg710-1.hfe", "bs600urlader_a.hfe", true);
    auto b = kopieOeffnen("bs600_prg700.hfe", "bs600urlader_b.hfe", true);
    ASSERT_TRUE(a); ASSERT_TRUE(b);
    std::vector<uint8_t> syl, boot;
    ASSERT_TRUE(a->readBootImage(syl));
    ASSERT_TRUE(b->readBootImage(boot));

    const Bs600Urlader ua = Bs600FileSystem::urladerKennung(syl), ub = Bs600FileSystem::urladerKennung(boot);
    EXPECT_EQ(Bs600Urlader::Art::Syl, ua.art);
    EXPECT_EQ("BS620", ua.name);
    EXPECT_EQ("710-1", ua.schalter);
    EXPECT_EQ(Bs600Urlader::Art::Bootstrap, ub.art);
    EXPECT_EQ("700", ub.schalter);
    EXPECT_EQ(ua.text, bfs(*a).urlader().text);

    EXPECT_EQ("", Bs600FileSystem::urladerGegenWunsch(syl, "710-1"));
    EXPECT_NE("", Bs600FileSystem::urladerGegenWunsch(syl, "700"));
    EXPECT_NE("", Bs600FileSystem::urladerGegenWunsch(syl, "710"));        // BS620 ist die 710-1-Fassung
    EXPECT_EQ("", Bs600FileSystem::urladerGegenWunsch(boot, "700"));
    const std::string falsch = Bs600FileSystem::urladerGegenWunsch(boot, "710-1");
    EXPECT_NE(std::string::npos, falsch.find("PRG 700")) << falsch;
    EXPECT_NE("", Bs600FileSystem::urladerGegenWunsch(syl, "711"));
    EXPECT_NE("", Bs600FileSystem::urladerGegenWunsch(std::vector<uint8_t>(384, 0xE5), "710-1"));

    // Der Hinweis auf den PRG-700-Urlader steht auch im Pruefbericht, der des SYL-Laders nicht.
    EXPECT_EQ(1u, mitId(b->check(FsCheckLevel::Schnell, true), "bs600.urlader.prg700").size());
    EXPECT_TRUE(mitId(a->check(FsCheckLevel::Schnell, true), "bs600.urlader.prg700").empty());
}
