/**
 * @file test_tar_write.cpp
 * @brief GoogleTests fuer das Schreiben in tar-Disketten (Entwurf 37, AP T1):
 *        @ref tar::TarArchiv und das schreibbare @ref TarFileSystem.
 *
 * Gegenprobe ist das `tar` des Systems: ein aus dem Modell auf die Diskette geschriebenes
 * Archiv muss `tar -tvf` / `tar -xOf` fehlerfrei lesen (nur unter Linux; sonst uebersprungen).
 *
 * @see core/filesystem/tar/tar_archiv.h, doc/design/37_tar_satz_und_platte.md
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fileinfo.h"
#include "core/filesystem/fs_catalog.h"
#include "core/filesystem/tar/tar_archiv.h"
#include "core/filesystem/tar/tar_fs.h"
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

constexpr size_t kDisk = 737280;

std::vector<uint8_t> leseDatei(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void schreibeDatei(const std::string& p, const std::vector<uint8_t>& d) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
}

std::vector<uint8_t> bytes(const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); }

/// Ist das Archiv der Diskette LEER (ganz Null)?  (Ersatz fuer das entfallene `DiskVolume::tarLeer()`.)
bool tarLeer(const DiskVolume& v) {
    const TarFileSystem* t = v.tarFs();
    return t && t->leer();
}

/// Die LEERE tar-Diskette als frische Datei, schreibbar geoeffnet.
std::unique_ptr<DiskVolume> neueLeere(const std::string& name) {
    std::string err;
    const std::string p = k1520test::tempPath(name);
    fs::remove(p);
    auto v = DiskVolume::create(p, "tar720_leer", "", formate(), dateisysteme(), err);
    EXPECT_TRUE(v) << err;
    return v;
}

std::unique_ptr<DiskVolume> oeffne(const std::string& p, bool schreibend) {
    std::string err;
    auto v = DiskVolume::open(p, "", formate(), dateisysteme(), err, !schreibend, true);
    EXPECT_TRUE(v) << err;
    return v;
}

/// Eine Linux-Datei mit Inhalt anlegen (Quelle fuer insert).
std::string quelle(const std::string& name, const std::string& inhalt) {
    const std::string p = k1520test::tempPath(name);
    schreibeDatei(p, bytes(inhalt));
    return p;
}

const FileEntry* finde(const std::vector<FileEntry>& l, const std::string& n) {
    for (const FileEntry& e : l) if (e.name == n) return &e;
    return nullptr;
}

std::string lies(DiskVolume& v, const std::string& name) {
    const std::string z = k1520test::tempPath("k1520_test_tarw_lies.bin");
    TransferOptions o;
    o.overwrite = true;
    if (!v.extract(FileRef{0, name}, z, o)) return "FEHLER: " + v.lastError();
    const auto d = leseDatei(z);
    fs::remove(z);
    fs::remove(k1520::fileinfo::fileinfoPfad(z));
    return std::string(d.begin(), d.end());
}

#ifndef _WIN32
bool systemTarDa() { return std::system("tar --version >/dev/null 2>&1") == 0; }

/// Ausgabe eines Befehls.
std::string erfasse(const std::string& cmd, int* rc = nullptr) {
    std::string out;
    FILE* p = popen((cmd + " 2>&1").c_str(), "r");
    if (!p) return out;
    char b[512];
    size_t n;
    while ((n = std::fread(b, 1, sizeof b, p)) > 0) out.append(b, n);
    const int r = pclose(p);
    if (rc) *rc = r;
    return out;
}

/// Das Archiv der Diskette in eine Datei exportieren und mit dem System-tar listen.
std::string systemTarListe(DiskVolume& v, int* rc) {
    const std::string z = k1520test::tempPath("k1520_test_tarw_sys.tar");
    EXPECT_TRUE(v.tarExport(z)) << v.lastError();
    const std::string r = erfasse("tar -tvf '" + z + "'", rc);
    fs::remove(z);
    return r;
}
#endif

}  // namespace

// ─── Das Archivmodell ────────────────────────────────────────────────────────

/// @test Der gebaute Strom ist V7: Koepfe gueltig, Daten aufgefuellt, Endmarke, kein ustar-Magic.
TEST(TarWrite, ModellBautenV7Strom) {
    tar::TarArchiv a;
    std::string err;
    tar::NeuAngaben n;
    n.mode = 0755; n.uid = 12; n.gid = 34; n.mtime = 600000000;
    ASSERT_TRUE(a.verzeichnis("bin", n, err)) << err;
    ASSERT_TRUE(a.hinzufuegen("bin/ls", std::vector<uint8_t>(700, 'x'), n, false, err)) << err;
    ASSERT_TRUE(a.hinzufuegen("leer", {}, n, false, err)) << err;
    std::vector<uint8_t> s;
    ASSERT_TRUE(a.strom(s, err)) << err;
    // Kopf bin/ + Kopf bin/ls + 2 Datenbloecke + Kopf leer + Endmarke
    EXPECT_EQ(s.size(), (1 + 1 + 2 + 1 + 2) * 512u);
    EXPECT_EQ(s.size(), a.stromLaenge());
    tar::Kopf k;
    ASSERT_TRUE(tar::kopfLesen(s.data(), k));
    EXPECT_EQ(k.name, "bin/");
    EXPECT_EQ(k.typ, '5');
    EXPECT_EQ(k.mode, 0755u);
    EXPECT_EQ(k.uid, 12u);
    EXPECT_EQ(k.gid, 34u);
    EXPECT_EQ(k.mtime, 600000000u);
    EXPECT_NE(std::memcmp(s.data() + 257, "ustar", 5), 0);
    ASSERT_TRUE(tar::kopfLesen(s.data() + 512, k));
    EXPECT_EQ(k.name, "bin/ls");
    EXPECT_EQ(k.size, 700u);
    EXPECT_EQ(s[1024 + 699], 'x');
    EXPECT_EQ(s[1024 + 700], 0);                      // aufgefuellt
    bool marke = false;
    EXPECT_EQ(tar::exportLaenge(s.data(), s.size(), &marke), s.size());
    EXPECT_TRUE(marke);
}

/// @test Namen ueber 100 Zeichen passen nicht ins V7-Format: klarer Fehler, nichts gebaut.
TEST(TarWrite, ZuLangerNameWirdAbgelehnt) {
    tar::TarArchiv a;
    std::string err;
    EXPECT_FALSE(a.hinzufuegen(std::string(101, 'n'), bytes("x"), {}, false, err));
    EXPECT_NE(err.find("100"), std::string::npos) << err;
    EXPECT_TRUE(a.eintraege().empty());
    EXPECT_TRUE(a.hinzufuegen(std::string(100, 'n'), bytes("x"), {}, false, err)) << err;
}

/// @test Hinzufuegen/Ersetzen/Loeschen/Attribute am Modell.
TEST(TarWrite, ModellOperationen) {
    tar::TarArchiv a;
    std::string err;
    tar::NeuAngaben n;
    n.mtime = 1000;
    ASSERT_TRUE(a.hinzufuegen("a", bytes("eins"), n, false, err));
    EXPECT_FALSE(a.hinzufuegen("a", bytes("zwei"), n, false, err));       // ohne Ersetzen
    EXPECT_NE(err.find("existiert"), std::string::npos);
    ASSERT_TRUE(a.hinzufuegen("/a", bytes("zwei"), n, true, err)) << err;  // bereinigter Name
    ASSERT_EQ(a.eintraege().size(), 1u);
    EXPECT_EQ(a.eintraege()[0].daten, bytes("zwei"));
    ASSERT_TRUE(a.verzeichnis("d/e", n, err));                              // legt d und d/e an
    EXPECT_EQ(a.eintraege().size(), 3u);
    EXPECT_FALSE(a.loeschen("d", err));                                     // nicht leer
    EXPECT_NE(err.find("nicht leer"), std::string::npos);
    EXPECT_FALSE(a.verzeichnis("a", n, err));                               // a ist eine Datei
    EXPECT_FALSE(a.hinzufuegen("d", bytes("x"), n, true, err));             // d ist ein Verzeichnis
    ASSERT_TRUE(a.loeschen("d/e", err));
    ASSERT_TRUE(a.loeschen("d", err));
    const uint32_t mode = 0600, uid = 5, mtime = 4242;
    ASSERT_TRUE(a.attribute("a", &mode, &uid, nullptr, &mtime, err));
    EXPECT_EQ(a.eintraege()[0].k.mode, 0600u);
    EXPECT_EQ(a.eintraege()[0].k.uid, 5u);
    EXPECT_EQ(a.eintraege()[0].k.gid, 0u);
    EXPECT_EQ(a.eintraege()[0].k.mtime, 4242u);
    EXPECT_FALSE(a.loeschen("gibt-es-nicht", err));
}

// ─── Schreiben auf die Diskette ──────────────────────────────────────────────

/// @test Leere tar-Diskette: Dateien einfuegen, speichern, wieder oeffnen; das System-tar liest es.
TEST(TarWrite, LeereDisketteRundlaufMitSystemTar) {
    const std::string pfad = k1520test::tempPath("k1520_test_tarw_a.img");
    std::string tarName;
    {
        auto v = neueLeere("k1520_test_tarw_a.img");
        ASSERT_TRUE(v);
        ASSERT_TRUE(tarLeer(*v));
        EXPECT_FALSE(v->readOnlyForced());
        const std::string q1 = quelle("k1520_test_tarw_q1.txt", "Hallo\n");
        const std::string q2 = quelle("k1520_test_tarw_q2.bin", std::string(1300, 'z'));
        ASSERT_TRUE(v->makeDirectory(FileRef{0, "dir"})) << v->lastError();
        ASSERT_TRUE(v->insert(q1, FileRef{0, "a.txt"}, TransferOptions{})) << v->lastError();
        ASSERT_TRUE(v->insert(q2, FileRef{0, "dir/b.bin"}, TransferOptions{})) << v->lastError();
        EXPECT_FALSE(tarLeer(*v));
        EXPECT_EQ(v->detection().filesystem, "tar720_leer");     // Profil bleibt bis zum Wiederoeffnen
        ASSERT_TRUE(v->flush()) << v->lastError();
        fs::remove(q1); fs::remove(q2);
    }
    auto w = oeffne(pfad, false);
    ASSERT_TRUE(w);
    EXPECT_EQ(w->detection().filesystem, "tar720");
    EXPECT_FALSE(w->readOnlyForced());                           // abgeschlossen: schreibbar
    const auto l = w->list();
    ASSERT_EQ(l.size(), 3u);
    const FileEntry* d = finde(l, "dir");
    ASSERT_TRUE(d);
    EXPECT_EQ(d->type, "d");
    EXPECT_EQ(d->unix_mode & 07777, 0755);
    const FileEntry* a = finde(l, "a.txt");
    ASSERT_TRUE(a);
    EXPECT_EQ(a->size, 6u);
    EXPECT_EQ(a->unix_mode & 07777, 0644);                       // Vorgabe: nicht ausfuehrbar
    EXPECT_EQ(a->unix_uid, 0);
    EXPECT_EQ(a->unix_gid, 0);
    EXPECT_NEAR(static_cast<double>(a->unix_mtime), static_cast<double>(std::time(nullptr)), 3600.0);
    EXPECT_EQ(lies(*w, "a.txt"), "Hallo\n");
    EXPECT_EQ(lies(*w, "dir/b.bin"), std::string(1300, 'z'));
#ifndef _WIN32
    if (systemTarDa()) {
        int rc = -1;
        const std::string liste = systemTarListe(*w, &rc);
        EXPECT_EQ(rc, 0) << liste;
        EXPECT_NE(liste.find("drwxr-xr-x"), std::string::npos) << liste;
        EXPECT_NE(liste.find("-rw-r--r--"), std::string::npos) << liste;
        EXPECT_NE(liste.find("dir/b.bin"), std::string::npos) << liste;
        // Inhalt durch das System-tar gelesen
        const std::string z = k1520test::tempPath("k1520_test_tarw_inh.tar");
        ASSERT_TRUE(w->tarExport(z));
        EXPECT_EQ(erfasse("tar -xOf '" + z + "' dir/b.bin"), std::string(1300, 'z'));
        fs::remove(z);
    }
#endif
    w.reset();
    fs::remove(pfad);
}

/// @test Rundlauf Ersetzen / Loeschen / Attribute auf einem bestehenden, abgeschlossenen Archiv;
///       der Rest des alten Archivs wird Null (kein alter Kopf hinter der neuen Endmarke).
TEST(TarWrite, ErsetzenLoeschenAttribute) {
    const std::string pfad = k1520test::tempPath("k1520_test_tarw_b.img");
    {
        auto v = neueLeere("k1520_test_tarw_b.img");
        ASSERT_TRUE(v);
        const std::string q = quelle("k1520_test_tarw_qb.bin", std::string(9000, 'g'));
        ASSERT_TRUE(v->insert(q, FileRef{0, "gross"}, TransferOptions{}));
        ASSERT_TRUE(v->insert(quelle("k1520_test_tarw_qb2.txt", "klein\n"), FileRef{0, "klein"},
                              TransferOptions{}));
        ASSERT_TRUE(v->flush());
    }
    auto v = oeffne(pfad, true);
    ASSERT_TRUE(v);
    ASSERT_FALSE(v->readOnly());
    // ohne overwrite abgelehnt, mit overwrite ersetzt
    const std::string neu = quelle("k1520_test_tarw_qb3.txt", "neu\n");
    EXPECT_FALSE(v->insert(neu, FileRef{0, "klein"}, TransferOptions{}));
    EXPECT_NE(v->lastError().find("existiert"), std::string::npos) << v->lastError();
    TransferOptions o;
    o.overwrite = true;
    ASSERT_TRUE(v->insert(neu, FileRef{0, "klein"}, o)) << v->lastError();
    EXPECT_EQ(lies(*v, "klein"), "neu\n");
    // Attribute
    UnixAttrs ua;
    ua.set_mode = true;  ua.mode = 04755;
    ua.set_uid = true;   ua.uid = 7;
    ua.set_gid = true;   ua.gid = 9;
    ua.set_mtime = true; ua.mtime = 600000000;
    ASSERT_TRUE(v->setAttributes(FileRef{0, "klein"}, ua)) << v->lastError();
    const FileEntry* k = nullptr;
    auto l = v->list();
    k = finde(l, "klein");
    ASSERT_TRUE(k);
    EXPECT_EQ(k->unix_mode & 07777, 04755);
    EXPECT_EQ(k->unix_uid, 7);
    EXPECT_EQ(k->unix_gid, 9);
    EXPECT_EQ(k->unix_mtime, 600000000u);
    EXPECT_FALSE(v->setAttributes(FileRef{0, "nichts"}, ua));
    // Loeschen der grossen Datei: das Archiv schrumpft, der alte Rest wird Null
    ASSERT_TRUE(v->erase(FileRef{0, "gross"})) << v->lastError();
    EXPECT_FALSE(v->erase(FileRef{0, "gross"}));
    ASSERT_TRUE(v->flush());
    v.reset();

    auto w = oeffne(pfad, false);
    ASSERT_TRUE(w);
    l = w->list();
    ASSERT_EQ(l.size(), 1u);
    EXPECT_EQ(l[0].name, "klein");
    EXPECT_EQ(l[0].unix_uid, 7);
    EXPECT_EQ(lies(*w, "klein"), "neu\n");
    // Strom: Kopf + 1 Datenblock + Endmarke, dahinter nur Nullen
    std::vector<uint8_t> roh = leseDatei(pfad);
    ASSERT_EQ(roh.size(), kDisk);
    bool marke = false;
    EXPECT_EQ(tar::exportLaenge(roh.data(), roh.size(), &marke), 4u * 512);
    EXPECT_TRUE(marke);
    EXPECT_TRUE(tar::alleNull(roh.data() + 3 * 512, roh.size() - 3 * 512));
    w.reset();
    fs::remove(pfad);
}

/// @test Alles loeschen: die Diskette ist wieder ganz Null und wird beim Oeffnen als LEER erkannt.
TEST(TarWrite, AllesLoeschenGibtDieLeereDiskette) {
    const std::string pfad = k1520test::tempPath("k1520_test_tarw_c.img");
    {
        auto v = neueLeere("k1520_test_tarw_c.img");
        ASSERT_TRUE(v);
        ASSERT_TRUE(v->insert(quelle("k1520_test_tarw_qc.txt", "x"), FileRef{0, "x"}, TransferOptions{}));
        ASSERT_TRUE(v->erase(FileRef{0, "x"})) << v->lastError();
        EXPECT_TRUE(tarLeer(*v));
        ASSERT_TRUE(v->flush());
    }
    const auto roh = leseDatei(pfad);
    EXPECT_TRUE(tar::alleNull(roh.data(), roh.size()));
    auto w = oeffne(pfad, false);
    ASSERT_TRUE(w);
    EXPECT_TRUE(tarLeer(*w));
    w.reset();
    fs::remove(pfad);
}

/// @test Verzeichnisse: mkdir -p, doppeltes Anlegen ist kein Fehler, nicht leer loeschen scheitert.
TEST(TarWrite, VerzeichnisseAnlegenUndLoeschen) {
    auto v = neueLeere("k1520_test_tarw_d.img");
    ASSERT_TRUE(v);
    ASSERT_TRUE(v->makeDirectory(FileRef{0, "a/b/c"})) << v->lastError();
    ASSERT_TRUE(v->makeDirectory(FileRef{0, "a/b"})) << v->lastError();
    EXPECT_EQ(v->list().size(), 3u);
    ASSERT_TRUE(v->insert(quelle("k1520_test_tarw_qd.txt", "x"), FileRef{0, "a/b/c/f"}, TransferOptions{}));
    EXPECT_FALSE(v->erase(FileRef{0, "a/b/c"}));
    EXPECT_NE(v->lastError().find("nicht leer"), std::string::npos) << v->lastError();
    ASSERT_TRUE(v->erase(FileRef{0, "a/b/c/f"}));
    ASSERT_TRUE(v->erase(FileRef{0, "a/b/c"}));
    EXPECT_EQ(v->list().size(), 2u);
    const std::string p = v->path();
    v.reset();
    fs::remove(p);
}

/// @test Folgeteil, abgeschnittenes Archiv und Archiv ohne Endmarke: lesbar, nicht schreibbar,
///       mit Hinweis, der den Weg nennt.
TEST(TarWrite, UnabgeschlosseneArchiveBleibenLesbar) {
    // Ein Archiv MIT Endmarke als Vorlage bauen, dann verstuemmeln.
    tar::TarArchiv a;
    std::string err;
    ASSERT_TRUE(a.hinzufuegen("eins", std::vector<uint8_t>(1500, 'e'), {}, false, err));
    ASSERT_TRUE(a.hinzufuegen("zwei", std::vector<uint8_t>(600, 'z'), {}, false, err));
    std::vector<uint8_t> s;
    ASSERT_TRUE(a.strom(s, err));
    struct Fall { const char* name; std::vector<uint8_t> strom; const char* hinweis; };
    std::vector<Fall> faelle;
    {   // ohne Endmarke (der Strom laeuft in 0xE5 aus)
        std::vector<uint8_t> t(s.begin(), s.end() - 1024);
        faelle.push_back({"ohneendmarke", t, "nur lesbar"});
    }
    {   // abgeschnitten mitten in der letzten Datei
        tar::TarArchiv g;
        ASSERT_TRUE(g.hinzufuegen("gross", std::vector<uint8_t>(kDisk + 5000, 'g'), {}, false, err));
        std::vector<uint8_t> t;
        ASSERT_TRUE(g.strom(t, err));
        t.resize(kDisk);
        faelle.push_back({"abgeschnitten", std::move(t), "nicht abgeschlossen"});
    }
    {   // Folgeteil: beginnt mitten in einer Datei, dahinter ein gueltiger Kopf
        std::vector<uint8_t> t(1024);
        for (size_t i = 0; i < t.size(); ++i) t[i] = static_cast<uint8_t>('A' + i % 23);
        t.insert(t.end(), s.begin() + 512 + 1536, s.end());
        faelle.push_back({"folgeteil", std::move(t), "mitten in einer Datei"});
    }
    {   // einzelner Nullblock statt Endmarke
        std::vector<uint8_t> t(s.begin(), s.end() - 1024);
        t.resize(t.size() + 512, 0);
        t.resize(t.size() + 512, 0xE5);
        faelle.push_back({"einnullblock", std::move(t), "einzelnen Nullblock"});
    }
    for (auto& f : faelle) {
        SCOPED_TRACE(f.name);
        f.strom.resize(kDisk, 0xE5);
        const std::string pfad = k1520test::tempPath(std::string("k1520_test_tarw_e_") + f.name + ".img");
        schreibeDatei(pfad, f.strom);
        auto v = oeffne(pfad, true);
        ASSERT_TRUE(v);
        ASSERT_TRUE(v->hasFileSystem());
        EXPECT_TRUE(v->istTar());
        ASSERT_FALSE(v->list().empty());
        EXPECT_TRUE(v->readOnlyForced());
        EXPECT_TRUE(v->readOnly());
        v->setReadOnly(false);
        EXPECT_TRUE(v->readOnly());
        EXPECT_NE(v->lastError().find(f.hinweis), std::string::npos) << v->lastError();
        EXPECT_FALSE(v->insert(quelle("k1520_test_tarw_qe.txt", "x"), FileRef{0, "neu"}, TransferOptions{}));
        EXPECT_FALSE(v->erase(FileRef{0, v->list()[0].name}));
        EXPECT_FALSE(v->makeDirectory(FileRef{0, "d"}));
        // unveraendert
        EXPECT_EQ(leseDatei(pfad), f.strom);
        v.reset();
        fs::remove(pfad);
    }
}

/// @test Passt der neue Strom nicht mehr auf EIN Abbild: die Aenderung bleibt im Speicher (die tar-Diskette
///       ist ein Satz, der wachsen darf), `flush()` nennt klar den Weg und schreibt nichts, die Datei bleibt
///       unveraendert; schrumpft der Inhalt wieder, geht er ins selbe Abbild zurueck.
///       (Vorher: der Einfuegeschritt selbst wurde mit „fasst nur ...“ abgelehnt.)
TEST(TarWrite, ZuGrossFuerEinAbbildBleibtImSpeicherUndFlushNenntDenWeg) {
    const std::string pfad = k1520test::tempPath("k1520_test_tarw_f.img");
    auto v = neueLeere("k1520_test_tarw_f.img");
    ASSERT_TRUE(v);
    ASSERT_TRUE(v->insert(quelle("k1520_test_tarw_qf1.bin", std::string(500000, 'a')), FileRef{0, "a"},
                          TransferOptions{}));
    ASSERT_TRUE(v->flush()) << v->lastError();
    const auto vorher = leseDatei(pfad);
    const std::string q2 = quelle("k1520_test_tarw_qf2.bin", std::string(400000, 'b'));
    ASSERT_TRUE(v->insert(q2, FileRef{0, "b"}, TransferOptions{})) << v->lastError();
    EXPECT_TRUE(v->dirty());
    EXPECT_EQ(v->list().size(), 2u);
    EXPECT_EQ(lies(*v, "b"), std::string(400000, 'b'));
    const TarSatzPlan plan = v->tarSatzPlanen();
    ASSERT_TRUE(plan.ok) << plan.fehler;
    EXPECT_EQ(plan.abbilder, 2);
    EXPECT_FALSE(v->flush());
    EXPECT_NE(v->lastError().find("nummerierten Abbildern"), std::string::npos) << v->lastError();
    EXPECT_EQ(leseDatei(pfad), vorher) << "ein abgelehntes Speichern aendert die Datei nicht";
    // die Platzpruefung des Stapelwegs lehnt nichts mehr ab: auch der Ordner geht in den Speicher
    const std::string ordner = k1520test::tempPath("k1520_test_tarw_fo");
    fs::remove_all(ordner);
    fs::create_directories(ordner);
    fs::copy_file(q2, ordner + "/c");
    std::string bericht;
    EXPECT_TRUE(v->checkFit(ordner, bericht)) << bericht;
    fs::remove_all(ordner);
    // Wieder auf EIN Abbild geschrumpft: es geht in dasselbe Abbild zurueck.
    ASSERT_TRUE(v->erase(FileRef{0, "b"})) << v->lastError();
    TransferOptions o;
    o.overwrite = true;
    ASSERT_TRUE(v->insert(q2, FileRef{0, "a"}, o)) << v->lastError();
    ASSERT_TRUE(v->flush()) << v->lastError();
    v.reset();
    auto w = oeffne(pfad, false);
    ASSERT_TRUE(w);
    EXPECT_EQ(lies(*w, "a"), std::string(400000, 'b'));
    w.reset();
    fs::remove(pfad);
}

/// @test `.fileinfo` (fs=tar): Extrahieren schreibt Modus/Besitzer/Zeit, Einfuegen liest sie wieder;
///       ohne `.fileinfo` gelten die Vorgaben (ausfuehrbare Quelle -> 0755).
TEST(TarWrite, FileinfoRundlaufUndVorgaben) {
    auto a = neueLeere("k1520_test_tarw_g1.img");
    auto b = neueLeere("k1520_test_tarw_g2.img");
    ASSERT_TRUE(a && b);
    ASSERT_TRUE(a->insert(quelle("k1520_test_tarw_qg.txt", "inhalt\n"), FileRef{0, "t.txt"}, TransferOptions{}));
    UnixAttrs ua;
    ua.set_mode = true;  ua.mode = 0640;
    ua.set_uid = true;   ua.uid = 11;
    ua.set_gid = true;   ua.gid = 22;
    ua.set_mtime = true; ua.mtime = 612345678;
    ASSERT_TRUE(a->setAttributes(FileRef{0, "t.txt"}, ua));

    const std::string ordner = k1520test::tempPath("k1520_test_tarw_go");
    fs::remove_all(ordner);
    fs::create_directories(ordner);
    ASSERT_TRUE(a->extract(FileRef{0, "t.txt"}, ordner + "/t.txt", TransferOptions{})) << a->lastError();
    k1520::fileinfo::FileAngaben ang;
    ASSERT_TRUE(k1520::fileinfo::leseFileinfo(ordner + "/t.txt.fileinfo", ang));
    EXPECT_TRUE(ang.tar);
    EXPECT_EQ(ang.x.mode & 07777, 0640);
    EXPECT_EQ(ang.x.uid, 11);
    EXPECT_EQ(ang.x.mtime, 612345678u);

    ASSERT_TRUE(b->insert(ordner + "/t.txt", FileRef{0, "t.txt"}, TransferOptions{})) << b->lastError();
    auto l = b->list();
    ASSERT_EQ(l.size(), 1u);
    EXPECT_EQ(l[0].unix_mode & 07777, 0640);
    EXPECT_EQ(l[0].unix_uid, 11);
    EXPECT_EQ(l[0].unix_gid, 22);
    EXPECT_EQ(l[0].unix_mtime, 612345678u);

    // ohne .fileinfo: Vorgaben; mit Ausfuehrungsrecht der Linux-Quelle -> 0755
    fs::remove(ordner + "/t.txt.fileinfo");
#ifndef _WIN32
    fs::permissions(ordner + "/t.txt", fs::perms::owner_exec, fs::perm_options::add);
    ASSERT_TRUE(b->insert(ordner + "/t.txt", FileRef{0, "x.sh"}, TransferOptions{}));
    const auto l2 = b->list();
    const FileEntry* x = finde(l2, "x.sh");
    ASSERT_TRUE(x);
    EXPECT_EQ(x->unix_mode & 07777, 0755);
    EXPECT_EQ(x->unix_uid, 0);
    EXPECT_EQ(x->unix_gid, 0);
#endif
    fs::remove_all(ordner);
    const std::string pa = a->path(), pb = b->path();
    a.reset(); b.reset();
    fs::remove(pa); fs::remove(pb);
}

/// @test Stapel: ein Ordnerbaum (mit leerem Unterordner) und `.fileinfo` der Verzeichnisse kommen
///       ins Archiv; das System-tar liest das Ergebnis.
TEST(TarWrite, StapelMitUnterordnern) {
    const std::string ordner = k1520test::tempPath("k1520_test_tarw_h");
    fs::remove_all(ordner);
    fs::create_directories(ordner + "/bin");
    fs::create_directories(ordner + "/leer");
    schreibeDatei(ordner + "/bin/ls", bytes("binaer"));
    schreibeDatei(ordner + "/readme", bytes("lies mich\n"));
    {   // Verzeichnisangaben fs=tar
        k1520::fileinfo::FileAngaben ang;
        ang.tar = true;
        ang.name = "bin";
        ang.x.art = "d";
        ang.x.mode = 040700; ang.x.mode_da = true;
        ang.x.uid = 3; ang.x.gid = 4; ang.x.owner_da = true;
        ang.x.mtime = 600000000;
        ASSERT_TRUE(k1520::fileinfo::schreibeFileinfo(ordner + "/bin.fileinfo", ang));
    }
    auto v = neueLeere("k1520_test_tarw_h.img");
    ASSERT_TRUE(v);
    ASSERT_TRUE(v->insertAll(ordner, TransferOptions{})) << v->lastError();
    const auto l = v->list();
    ASSERT_EQ(l.size(), 4u);
    const FileEntry* bin = finde(l, "bin");
    ASSERT_TRUE(bin);
    EXPECT_EQ(bin->type, "d");
    EXPECT_EQ(bin->unix_mode & 07777, 0700);
    EXPECT_EQ(bin->unix_uid, 3);
    EXPECT_EQ(bin->unix_mtime, 600000000u);
    EXPECT_TRUE(finde(l, "leer"));
    EXPECT_TRUE(finde(l, "bin/ls"));
#ifndef _WIN32
    if (systemTarDa()) {
        int rc = -1;
        const std::string liste = systemTarListe(*v, &rc);
        EXPECT_EQ(rc, 0) << liste;
        EXPECT_NE(liste.find("drwx------ 3/4"), std::string::npos) << liste;
        EXPECT_NE(liste.find("bin/ls"), std::string::npos) << liste;
    }
#endif
    fs::remove_all(ordner);
    const std::string p = v->path();
    v.reset();
    fs::remove(p);
}

/// @test Ein vom System-tar (V7) erzeugtes Archiv laesst sich bearbeiten und bleibt fuer tar lesbar.
TEST(TarWrite, SystemTarArchivBearbeitenUndGegenprobe) {
#ifdef _WIN32
    GTEST_SKIP() << "nur unter Linux";
#else
    if (!systemTarDa()) GTEST_SKIP() << "kein tar";
    const std::string dir = k1520test::tempPath("k1520_test_tarw_i");
    fs::remove_all(dir);
    fs::create_directories(dir + "/q/sub");
    schreibeDatei(dir + "/q/a.txt", bytes("Hallo\n"));
    schreibeDatei(dir + "/q/sub/b.txt", bytes("Welt\n"));
    const std::string t = dir + "/v7.tar";
    if (std::system(("cd '" + dir + "' && tar --format=v7 -cf '" + t + "' q 2>/dev/null").c_str()) != 0)
        GTEST_SKIP() << "tar ohne --format=v7";
    auto d = leseDatei(t);
    ASSERT_FALSE(d.empty());
    d.resize(kDisk, 0xE5);                                  // Altbestand hinter dem Archiv
    const std::string pfad = k1520test::tempPath("k1520_test_tarw_i.img");
    schreibeDatei(pfad, d);
    {
        auto v = oeffne(pfad, true);
        ASSERT_TRUE(v);
        ASSERT_FALSE(v->readOnlyForced()) << v->lastError();
        ASSERT_TRUE(v->hasFileSystem());
        ASSERT_FALSE(v->readOnly());
        ASSERT_TRUE(v->erase(FileRef{0, "q/a.txt"})) << v->lastError();
        ASSERT_TRUE(v->insert(quelle("k1520_test_tarw_qi.txt", "neu\n"), FileRef{0, "q/sub/c.txt"},
                              TransferOptions{})) << v->lastError();
        UnixAttrs ua;
        ua.set_mode = true; ua.mode = 0600;
        ASSERT_TRUE(v->setAttributes(FileRef{0, "q/sub/b.txt"}, ua));
        ASSERT_TRUE(v->flush());
        int rc = -1;
        const std::string liste = systemTarListe(*v, &rc);
        EXPECT_EQ(rc, 0) << liste;
        EXPECT_EQ(liste.find("a.txt"), std::string::npos) << liste;
        EXPECT_NE(liste.find("c.txt"), std::string::npos) << liste;
        EXPECT_NE(liste.find("-rw------- "), std::string::npos) << liste;
    }
    // Wiederoeffnen: dasselbe, ohne Reste des alten Archivs
    auto w = oeffne(pfad, false);
    ASSERT_TRUE(w);
    const auto l = w->list();
    EXPECT_TRUE(finde(l, "q/sub/c.txt"));
    EXPECT_FALSE(finde(l, "q/a.txt"));
    EXPECT_EQ(lies(*w, "q/sub/b.txt"), "Welt\n");
    w.reset();
    fs::remove(pfad);
    fs::remove_all(dir);
#endif
}

/// @test Eine fertige .tar als Container auf die leere Diskette (frueher `tar-put`): danach ist die
///       Diskette NICHT schreibgeschuetzt und ganz normal bearbeitbar; eine zweite .tar kommt nur noch
///       als Datei hinein (Container nur auf leerem Archiv).
TEST(TarWrite, ContainerAufLeereDisketteDanachBearbeitbar) {
    tar::TarArchiv a;
    std::string err;
    ASSERT_TRUE(a.hinzufuegen("x.txt", bytes("x"), {}, false, err));
    std::vector<uint8_t> s;
    ASSERT_TRUE(a.strom(s, err));
    const std::string src = k1520test::tempPath("k1520_test_tarw_j.tar");
    schreibeDatei(src, s);
    auto v = neueLeere("k1520_test_tarw_j.img");
    ASSERT_TRUE(v);
    ASSERT_TRUE(v->tarDateiAblegen(src, TarDateiWeg::AlsContainer)) << v->lastError();
    ASSERT_TRUE(v->flush()) << v->lastError();
    EXPECT_FALSE(v->readOnly());                 // kein Einmalwurf mehr: nicht schreibgeschuetzt
    EXPECT_FALSE(v->readOnlyForced());
    EXPECT_FALSE(v->tarDateiBeurteilen(src).als_container);   // zweiter Container: nein ...
    EXPECT_FALSE(v->tarDateiAblegen(src, TarDateiWeg::AlsContainer));
    EXPECT_TRUE(v->tarDateiBeurteilen(src).als_datei);        // ... als Datei ja
    ASSERT_TRUE(v->insert(quelle("k1520_test_tarw_qj.txt", "y"), FileRef{0, "y.txt"}, TransferOptions{}))
        << v->lastError();
    EXPECT_EQ(v->list().size(), 2u);
    ASSERT_TRUE(v->flush()) << v->lastError();
    const std::string p = v->path();
    v.reset();
    auto w = oeffne(p, false);
    ASSERT_TRUE(w);
    EXPECT_EQ(w->list().size(), 2u);
    w.reset();
    fs::remove(p); fs::remove(src);
}
