/**
 * @file test_p8000_wega_diskette.cpp
 * @brief P8000/WEGA: Disketten formatieren, beschreiben und im DiskTool lesen (Handbuch „Arbeiten mit
 *        Disketten unter WEGA", doc/design/25_p8000.md).
 *
 * Ausgangspunkt ist der eingefrorene Zustand `p15_6_login` (WEGA im Mehrbenutzerbetrieb an der
 * Anmeldung, tests/support/p8000_wega.h) — der Boot wird nicht jedes Mal abgewartet.  Fehlt er,
 * wird uebersprungen.  Alles wird an der Konsole getippt (Floppy-Kommandos gehen in WEGA nur dort).
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <regex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/p8000/p8000.h"
#include "tests/support/p8000_input.h"
#include "tests/support/p8000_wega.h"

using namespace k1520test::p8000;

namespace {
void stumm() { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR); }

/// Eine Konsolensitzung am laufenden WEGA: Anmeldung und Kommandos mit mitgezaehltem Prompt `#N`.
struct Sitzung {
    WegaLauf l;
    int n = 1;   ///< Nummer des NAECHSTEN Prompts
    /// Disketten fuer `insert next disk :` (Mehrdisketten-tar), der Reihe nach; vorformatiert, Laufwerk 1.
    std::vector<std::string> folge;
    size_t naechste = 0;
    int gewechselt = 0;   ///< wie oft WEGA nach der naechsten Diskette gefragt hat

    P8000Machine& m() { return *l.m; }

    bool anmelden() {
        tippeZeile(m(), "wega");
        if (warteAufFrage(m(), {"Password:"}, 400'000'000) != 0) return false;
        laufe(m(), 200'000);
        tippeZeile(m(), "root");
        if (warteAufFrage(m(), {"#1"}, 2'000'000'000LL) < 0) return false;
        n = 2;
        return true;
    }

    /// Kommando tippen, `ready? ` mit `y` beantworten, auf den naechsten Prompt warten.  Liefert den
    /// Bildschirm (die Zeilen seit dem Kommando stehen darin; bei langer Ausgabe scrollt es).
    bool kommando(const std::string& cmd, long long grenze = 2'000'000'000LL) {
        tippeZeile(m(), cmd);
        const std::string prompt = "#" + std::to_string(n);
        for (;;) {
            const int i = warteAufFrage(m(), {prompt, "ready?", "insert next disk :"}, grenze);
            if (i < 0) return false;
            if (i == 0) break;
            if (i == 2) {   // Mehrdisketten-tar: Diskette wechseln, RETURN
                if (naechste >= folge.size()) return false;
                m().flushDisks();
                m().unmountDisk(1);
                if (!m().mountDisk(1, folge[naechste++], kWegaFormat, false)) return false;
                ++gewechselt;
                laufe(m(), 1'000'000);
                tippeZeile(m(), "");
                laufe(m(), 1'000'000);
                continue;
            }
            tippeZeile(m(), "y");
            laufe(m(), 1'000'000);
        }
        ++n;
        return true;
    }
};

std::string lies(const char* pfad) {
    std::ifstream f(pfad);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

namespace stdfs = std::filesystem;

const FormatCatalog& formate() {
    static FormatCatalog c = [] {
        std::string f;
        FormatCatalog k = FormatCatalog::loadDefault(&f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

const FsCatalog& dateisysteme() {
    static FsCatalog c = [] {
        std::string f;
        FsCatalog k = FsCatalog::loadDefault(formate(), &f);
        EXPECT_TRUE(f.empty()) << f;
        return k;
    }();
    return c;
}

/// Diskette wie das DiskTool: schreibgeschuetzt oeffnen (@p schreibend = false) bzw. zum Aendern.
std::unique_ptr<DiskVolume> oeffne(const std::string& pfad, bool schreibend = false) {
    std::string err;
    auto v = DiskVolume::open(pfad, "", formate(), dateisysteme(), err, !schreibend, false);
    EXPECT_TRUE(v) << pfad << ": " << err;
    return v;
}

/// Die Befunde der Vollpruefung als Text (leer = ohne Befund).
std::string pruefe(DiskVolume& v) {
    const FsCheckReport& r = v.check(FsCheckLevel::Voll, true);
    std::string s;
    for (const FsFinding& f : r.findings) s += f.id + " ";
    return s;
}

std::string leseDatei(const std::string& pfad) {
    std::ifstream f(pfad, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

/// Version-7-tar-Strom (das Format, das WEGA-tar versteht): je Datei ein 512-B-Kopf (kein `ustar`),
/// Daten auf 512 aufgefuellt, am Ende zwei Nullbloecke.  Nur Dateien, keine Verzeichniseintraege —
/// so macht es auch `find -type f | tar --format=v7 -T -` auf dem Linux-Rechner.
std::string v7Archiv(const std::vector<std::pair<std::string, std::string>>& dateien) {
    std::string out;
    for (const auto& [name, inhalt] : dateien) {
        char k[512] = {};
        std::memcpy(k, name.data(), name.size());                       // name[100]
        std::snprintf(k + 100, 8, "%06o ", 0644);                       // mode
        std::snprintf(k + 108, 8, "%06o ", 0);                          // uid
        std::snprintf(k + 116, 8, "%06o ", 0);                          // gid
        std::snprintf(k + 124, 12, "%011llo", static_cast<unsigned long long>(inhalt.size()));
        std::snprintf(k + 136, 12, "%011o", 0x2A000000);                // mtime (egal)
        k[156] = '0';                                                   // normale Datei
        std::memset(k + 148, ' ', 8);                                   // Pruefsumme zaehlt als Leerzeichen
        unsigned sum = 0;
        for (char c : k) sum += static_cast<unsigned char>(c);
        std::snprintf(k + 148, 7, "%06o", sum);
        k[154] = 0; k[155] = ' ';
        out.append(k, 512);
        out += inhalt;
        out.append((512 - inhalt.size() % 512) % 512, '\0');
    }
    out.append(1024, '\0');
    return out;
}
}  // namespace

/// Werkzeug (kein Waechter): Kommandos aus der Datei `K1520_WEGA_SKRIPT` an der Konsole ausfuehren.
/// Zeilen: `@einlegen PFAD` (Laufwerk 1; fehlt die Datei, entsteht eine Leerdiskette), `@auswerfen`,
/// `@merke PFAD` (Diskette im Laufwerk 1 auf die Platte zurueckschreiben und kopieren), `# Kommentar`,
/// sonst ein Kommando.  `--gtest_also_run_disabled_tests --gtest_filter='*Skript*'`.
TEST(P8000WegaDiskette, DISABLED_Skript) {
    stumm();
    const char* sk = std::getenv("K1520_WEGA_SKRIPT");
    ASSERT_NE(sk, nullptr);
    Sitzung s;
    std::string fehler;
    ASSERT_TRUE(stufeLaden(s.l, "p15_6_login", &fehler)) << fehler;
    ASSERT_TRUE(s.anmelden()) << bild(s.m());
    std::istringstream in(lies(sk));
    std::string zl, eingelegt;
    while (std::getline(in, zl)) {
        if (zl.empty() || zl[0] == '#') continue;
        if (zl.rfind("@einlegen ", 0) == 0) {
            eingelegt = zl.substr(10);
            s.m().unmountDisk(1);
            const bool da = dateiDa(eingelegt);
            ASSERT_TRUE(da ? s.m().mountDisk(1, eingelegt, kWegaFormat, false)
                           : s.m().createDisk(1, eingelegt, "", false))
                << s.m().lastError();
            continue;
        }
        if (zl.rfind("@folge ", 0) == 0) { s.folge.push_back(zl.substr(7)); continue; }   // Diskette fuer `insert next disk`
        if (zl == "@auswerfen") { s.m().flushDisks(); s.m().unmountDisk(1); continue; }
        const std::string kopf = "#" + std::to_string(s.n - 1) + " ";
        const bool ok = s.kommando(zl);
        // Nur die Zeilen ab dem Kommando zeigen (der Bildschirm scrollt, der Rest ist Altbestand).
        const std::string b = bild(s.m());
        const size_t ab = b.rfind(kopf);
        std::fprintf(stderr, "%s", (ab == std::string::npos ? b : b.substr(ab)).c_str());
        std::fprintf(stderr, "  [%.0f Mio. Takte]\n", double(s.m().totalCycles()) / 1e6);
        ASSERT_TRUE(ok) << "kein Prompt #" << s.n;
    }
    s.m().flushDisks();
}

namespace {
/// Bildschirm ab dem zuletzt getippten Kommando (der Bildschirm scrollt, davor ist Altbestand).
std::string ausgabe(Sitzung& s, const std::string& cmd) {
    const std::string kopf = "#" + std::to_string(s.n - 1) + " ";
    const std::string b = bild(s.m());
    const size_t ab = b.rfind(kopf);
    (void)cmd;
    return ab == std::string::npos ? b : b.substr(ab);
}

bool enthaelt(const std::string& heu, const std::string& nadel) { return heu.find(nadel) != std::string::npos; }

/// Kommando ausfuehren und die Ausgabe zurueckgeben; bleibt der Prompt aus, scheitert der Test.
std::string fuehre(Sitzung& s, const std::string& cmd, long long grenze = 2'000'000'000LL) {
    EXPECT_TRUE(s.kommando(cmd, grenze)) << "kein Prompt nach '" << cmd << "'\n" << bild(s.m());
    return ausgabe(s, cmd);
}

/// Leere Diskette in Laufwerk 1 (B:) legen: eine echte unformatierte Scheibe, das Formatieren macht WEGA.
bool leerdisketteEinlegen(Sitzung& s, const std::string& pfad) {
    s.m().flushDisks();
    s.m().unmountDisk(1);
    stdfs::remove(pfad);
    return s.m().createDisk(1, pfad, "", false);
}

void auswerfen(Sitzung& s) { s.m().flushDisks(); s.m().unmountDisk(1); }

bool diskEinlegen(Sitzung& s, const std::string& pfad) {
    s.m().unmountDisk(1);
    return s.m().mountDisk(1, pfad, kWegaFormat, false);
}

/// Anzahl der Zeilen der WEGA-fsck-Ausgabe, die auf ein Problem deuten.
bool fsckSauber(const std::string& a) {
    for (const char* w : {"BAD", "DUP", "MISSING", "UNREF", "CLEAR", "SALVAGE", "FIX", "MISMATCH", "ADJUST", "?"})
        if (enthaelt(a, w)) return false;
    return enthaelt(a, "** Phase 5 - Check Free List") && enthaelt(a, " files ");
}
}  // namespace

/**
 * @test Der Diskettenweg von WEGA, durchgespielt wie im Handbuch („Arbeiten mit Disketten unter WEGA"):
 *       (1) Diskette mit `format` + `mkfs` anlegen, mounten, beschreiben, `umount`, `fsck`;
 *       (2) ein **Mehrdisketten-Archiv**: `tar cvf /dev/rfd1` ueber 1,2 MB, WEGA fragt `insert next disk :`;
 *       (3) das DiskTool liest alle Disketten fehlerfrei (Dateisystem erkannt, Vollpruefung ohne Befund, `tar-satz`);
 *       (4) Rueckweg: Datei auf der WEGA-Diskette loeschen + neue schreiben, ein V7-Strom vom Linux-Rechner mit `split`
 *           auf zwei Disketten (`tar-put`), WEGA-`fsck`/`mount`/`cat`, `tar x` ueber beide Disketten in ein
 *           temporaeres Verzeichnis, Inhalt bitgleich zu /usr/man.
 *       Startet aus dem eingefrorenen Zustand p15_6_login (kein Boot).
 */
TEST(P8000WegaDiskette, FormatMountTarUndDiskTool) {
    stumm();
    if (!stufeDa("p15_6_login")) GTEST_SKIP() << "kein Zwischenstand p15_6_login (tests/system/test_p8000_wega_install.cpp)";
    Sitzung s;
    std::string fehler;
    ASSERT_TRUE(stufeLaden(s.l, "p15_6_login", &fehler)) << fehler;
    ASSERT_TRUE(s.anmelden()) << bild(s.m());

    const std::string dir = k1520test::tempPath("k1520_wega_diskette");
    stdfs::remove_all(dir);
    stdfs::create_directories(dir);
    const std::string fsd = dir + "/wega_fs.hfe", tara = dir + "/tar_a.hfe", tarb = dir + "/tar_b.hfe";

    // ── 1  Diskette mit Dateisystem ─────────────────────────────────────────────────────────────
    ASSERT_TRUE(leerdisketteEinlegen(s, fsd)) << s.m().lastError();
    EXPECT_TRUE(enthaelt(fuehre(s, "format fd1"), "ready? y"));
    EXPECT_TRUE(enthaelt(fuehre(s, "/etc/mkfs /dev/fd1 1440"), "isize = 472"));
    fuehre(s, "mkdir /dsk");
    EXPECT_TRUE(enthaelt(fuehre(s, "/etc/mount /dev/fd1 /dsk"), "mounting"));
    fuehre(s, "echo \"Hallo von WEGA\" > /dsk/hallo.txt");
    fuehre(s, "mkdir /dsk/man");
    fuehre(s, "cp /usr/man/man1/tar.1 /dsk/man/tar.1");
    const std::string ls = fuehre(s, "ls -lR /dsk");
    EXPECT_TRUE(enthaelt(ls, "hallo.txt")) << ls;
    EXPECT_TRUE(enthaelt(ls, "tar.1")) << ls;
    EXPECT_TRUE(enthaelt(fuehre(s, "cat /dsk/hallo.txt"), "Hallo von WEGA"));
    fuehre(s, "/etc/umount /dev/fd1");
    const std::string fsck1 = fuehre(s, "/etc/fsck /dev/fd1");
    EXPECT_TRUE(fsckSauber(fsck1)) << fsck1;
    EXPECT_TRUE(enthaelt(fsck1, "5 files")) << fsck1;
    fuehre(s, "sync");
    auswerfen(s);

    // ── 2  Mehrdisketten-tar auf dem Raw-Geraet ──────────────────────────────────────────────────
    // Beide Disketten werden VORHER formatiert: waehrend tar auf die naechste wartet, ist die Konsole belegt.
    for (const std::string& p : {tara, tarb}) {
        ASSERT_TRUE(leerdisketteEinlegen(s, p)) << s.m().lastError();
        fuehre(s, "format fd1");
        auswerfen(s);
    }
    ASSERT_TRUE(diskEinlegen(s, tara)) << s.m().lastError();
    s.folge = {tarb};
    s.naechste = 0;
    s.gewechselt = 0;
    const std::string t = fuehre(s, "cd /usr/man; tar cvf /dev/rfd1 man1 man2 man3 manM > /z/tar.log; tail -2 /z/tar.log");
    EXPECT_EQ(s.gewechselt, 1) << t;   // WEGA hat nach der zweiten Diskette gefragt
    EXPECT_TRUE(enthaelt(t, "tar: allocated space on disk")) << t;
    auswerfen(s);

    // ── 3  Das DiskTool liest alles ───────────────────────────────────────────────────────────────
    {
        auto v = oeffne(fsd);
        ASSERT_TRUE(v);
        EXPECT_EQ(v->detection().filesystem, "wega720");
        EXPECT_EQ(pruefe(*v), "");
        bool hallo = false, tar1 = false;
        for (const FileEntry& e : v->list()) {
            if (e.name == "hallo.txt") { hallo = true; EXPECT_EQ(e.size, 15u); }
            if (e.name == "man/tar.1") { tar1 = true; EXPECT_EQ(e.size, 5801u); }
        }
        EXPECT_TRUE(hallo && tar1);
        const std::string aus = dir + "/hallo.txt";
        ASSERT_TRUE(v->extract(FileRef::parse("hallo.txt"), aus, TransferOptions{})) << v->lastError();
        EXPECT_EQ(leseDatei(aus), "Hallo von WEGA\n");
    }
    for (const std::string& p : {tara, tarb}) {   // jede Diskette ist voll und einzeln als tar-Diskette erkannt
        auto v = oeffne(p);
        ASSERT_TRUE(v);
        EXPECT_EQ(v->detection().filesystem, "tar720");
        uint64_t n = 0;
        ASSERT_TRUE(v->tarExport(dir + "/export.tar", &n)) << v->lastError();
        EXPECT_EQ(n, 737'280u);
    }
    const std::string lokal = dir + "/lokal";
    {
        std::string err;
        auto satz = DiskVolume::openTarSatz({tara, tarb}, formate(), dateisysteme(), err);
        ASSERT_TRUE(satz) << err;
        EXPECT_EQ(pruefe(*satz), "");
        bool adb = false;
        size_t dateien = 0;
        for (const FileEntry& e : satz->list()) {
            ++dateien;
            if (e.name == "man1/adb.1") { adb = true; EXPECT_EQ(e.size, 16360u); }
        }
        EXPECT_TRUE(adb);
        EXPECT_EQ(dateien, 451u);   // man1 236 + man2 53 + man3 99 + manM 63
        ASSERT_TRUE(satz->extractAll(lokal, TransferOptions{})) << satz->lastError();
    }

    // ── 4  Rueckweg ────────────────────────────────────────────────────────────────────────────────
    // WEGA-Diskette: Datei loeschen, neue schreiben (DiskTool: `rm` + `put`).
    {
        auto v = oeffne(fsd, true);
        ASSERT_TRUE(v);
        ASSERT_TRUE(v->erase(FileRef::parse("hallo.txt"))) << v->lastError();
        const std::string quelle = dir + "/neu.txt";
        { std::ofstream(quelle) << "Datei vom Linux-Rechner\n"; }
        ASSERT_TRUE(v->insert(quelle, FileRef::parse("neu.txt"), TransferOptions{})) << v->lastError();
        ASSERT_TRUE(v->flush()) << v->lastError();
    }
    // Linux: eine Datei hinzufuegen, EIN V7-Strom (nur Dateien); er wird als Container in einen tar-Satz
    // gelegt und beim Speichern auf Abbilder zu 737280 Byte verteilt (jede ist eine tar-Diskette).
    { std::ofstream(lokal + "/man2/NEU.TXT") << "Neu vom Linux-Rechner\n"; }
    std::vector<std::pair<std::string, std::string>> dateien;
    for (const auto& e : stdfs::recursive_directory_iterator(lokal)) {
        if (!e.is_regular_file() || e.path().extension() == ".fileinfo") continue;
        dateien.emplace_back(stdfs::relative(e.path(), lokal).generic_string(), leseDatei(e.path().string()));
    }
    std::sort(dateien.begin(), dateien.end());
    ASSERT_EQ(dateien.size(), 452u);
    const std::string strom = v7Archiv(dateien);
    const std::string gesamt = dir + "/gesamt.tar";
    { std::ofstream(gesamt, std::ios::binary) << strom; }
    std::vector<std::string> teile;
    {
        std::string err;
        auto satz = DiskVolume::tarSatzNeu("k5601_9x512", dir + "/teil.hfe", formate(), dateisysteme(), err);
        ASSERT_TRUE(satz) << err;
        ASSERT_TRUE(satz->tarDateiAblegen(gesamt, TarDateiWeg::AlsContainer)) << satz->lastError();
        EXPECT_TRUE(satz->lastTarWarnung().empty()) << satz->lastTarWarnung();   // V7: keine Rueckfrage
        ASSERT_TRUE(satz->tarSatzSpeichern("", nullptr, &teile)) << satz->lastError();
    }
    ASSERT_EQ(teile.size(), 2u) << strom.size();

    // Auf WEGA: veraenderte Dateisystemdiskette pruefen, mounten, cat.
    ASSERT_TRUE(diskEinlegen(s, fsd)) << s.m().lastError();
    const std::string fsck2 = fuehre(s, "/etc/fsck /dev/fd1");
    EXPECT_TRUE(fsckSauber(fsck2)) << fsck2;
    fuehre(s, "/etc/mount /dev/fd1 /dsk");
    const std::string ls2 = fuehre(s, "ls /dsk");
    EXPECT_TRUE(enthaelt(ls2, "neu.txt")) << ls2;
    EXPECT_FALSE(enthaelt(ls2, "hallo.txt")) << ls2;
    EXPECT_TRUE(enthaelt(fuehre(s, "cat /dsk/neu.txt"), "Datei vom Linux-Rechner"));
    fuehre(s, "/etc/umount /dev/fd1");
    auswerfen(s);

    // Beide tar-Disketten (ein Archiv ueber zwei Disketten) in ein temporaeres Verzeichnis entpacken.
    fuehre(s, "mkdir /z/tmp");
    ASSERT_TRUE(diskEinlegen(s, teile[0])) << s.m().lastError();
    s.folge = {teile[1]};
    s.naechste = 0;
    s.gewechselt = 0;
    const std::string x = fuehre(s, "cd /z/tmp; tar xvf /dev/rfd1 > /z/entpackt.txt; tail -1 /z/entpackt.txt");
    EXPECT_EQ(s.gewechselt, 1) << x;
    EXPECT_FALSE(enthaelt(x, "cannot")) << x;
    auswerfen(s);
    EXPECT_TRUE(enthaelt(fuehre(s, "find /z/tmp -type f -print | wc -l"), "452"));
    EXPECT_TRUE(enthaelt(fuehre(s, "cat /z/tmp/man2/NEU.TXT"), "Neu vom Linux-Rechner"));
    // Inhalt bitgleich zum Original (die Grenze zwischen den Disketten liegt mitten in einer Datei).
    for (const char* d : {"man1", "man3"}) {
        const auto summe = [&](const std::string& wurzel) {
            const std::string a = fuehre(s, "cd " + wurzel + "; cat " + d + "/* | sum");
            std::smatch m;
            const std::string rest = a.substr(a.find('\n') == std::string::npos ? 0 : a.find('\n'));
            return std::regex_search(rest, m, std::regex("\n(\\d+ \\d+)")) ? m[1].str() : std::string();
        };
        const std::string neu = summe("/z/tmp"), alt = summe("/usr/man");
        EXPECT_FALSE(neu.empty());
        EXPECT_EQ(neu, alt) << d;
    }

    if (const char* o = std::getenv("K1520_WEGA_DISKETTEN_AUS"); o && *o) {   // fuer das Handbuch von Hand nachsehen
        stdfs::create_directories(o);
        for (const auto& e : stdfs::directory_iterator(dir))
            if (e.path().extension() == ".hfe" || e.path().extension() == ".tar")
                stdfs::copy_file(e.path(), stdfs::path(o) / e.path().filename(), stdfs::copy_options::overwrite_existing);
    }
    stdfs::remove_all(dir);
}
