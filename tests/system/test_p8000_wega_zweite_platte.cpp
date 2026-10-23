/**
 * @file test_p8000_wega_zweite_platte.cpp
 * @brief Zweite Festplatte (WDC-Laufwerk 1) unter WEGA 3.1 einrichten, formatieren, mounten und dauerhaft
 *        einbinden (Ticket #15, Merkposten p8000 Nr. 64; Handbuch „Zweite Festplatte unter WEGA einrichten").
 *
 * Verfahren nach dem WEGA-Systemhandbuch, Abschnitt 4.4 („Einbinden eines zweiten Hard-Disk-Laufwerks") und 5.5
 * (sysgen), am Emulator nachgewiesen:
 *   1. Neue Platte vom Typ der Platte 0 (hier WEGA31-AVR, 1380/10/18; unformatiert) an Laufwerk 1, Kaltstart.  Der Hardwaretest bemaengelt den fehlenden
 *      Parametersatz von LW 1 und bleibt am Monitor stehen (`*`, kein AUTOBOOT).
 *   2. `O U` -> `boot` -> `md(0,16000)sa.format`: Laufwerk 1, vorgeschlagener PAR (= LW 0) `y`, ganze Platte formatieren, PAR&BTT
 *      schreiben.  Danach `md(0,16000)wega` (der ausgelieferte Kern).
 *   3. Der ausgelieferte Kern kennt LW 1 nur mit Groesse 0 (`md_sizes[10..19]` in `wpar.c` aus
 *      `/usr/sys/h/mdsize.h`): `cd /usr/sys/conf; sysgen` mit „add second harddisk" -> neuer Kern, `cp` nach /wega.
 *   4. Kaltstart mit dem neuen Kern: `mknod`/`chmog` fuer md10/rmd10, `mkdir /usr2`, `mkfs`, `labelit`, `mount`,
 *      `mlf`; Eintraege in `/etc/mfs` (= `/etc/umfs`, harter Link), `/etc/rc_csh` (fsck) und `/etc/checklist`.
 *      Daten schreiben, `sync`.
 *   5. Kaltstart: `rc_csh` prueft /dev/rmd10, `mfs` mountet /usr2, die Daten sind da.
 * Ein /etc/fstab gibt es in WEGA nicht; eingebunden wird ueber die Shell-Prozedur /etc/mfs, die /etc/rc_csh beim
 * Uebergang in den Mehrbenutzerbetrieb aufruft.
 *
 * VERKUERZUNG (wie test_p8000_wega_rtc.cpp): Schritt 1–2 und 3 liegen als eingefrorene Zustaende im Ablageordner
 * (`~/.cache/k1520emu/p8000_wega`, Namen `zp31_*`), gebaut beim ersten Lauf (Formatieren der ganzen Platte und der
 * Kernbau kosten zusammen ~ 30 Maschinenminuten).  Der Stempel haengt an Groesse/Zeit der ausgelieferten Platte und
 * an kStufenStand; `K1520_ZP_NEU=1` baut alles neu.  Schritt 4 und 5 (Einrichten und Dauerhaftigkeit) laufen bei
 * JEDEM Lauf.
 *
 * Platten als `.img.gz` (die Platte packt selbst, Merkposten 56) — die Zustaende bleiben klein.  Konsole =
 * Kern-Terminal (schnelles Tippen); getippt wird wie am Geraet.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "core/logger.h"
#include "core/machines/p8000/p8000.h"
#include "tests/support/p8000_input.h"
#include "tests/support/p8000_wega.h"
#include "tests/support/temp_path.h"

using namespace k1520test::p8000;

#ifndef P8000_WEGA31_DIR
#define P8000_WEGA31_DIR "disks/p8000_wega31"
#endif

namespace {

void stumm() { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR); }

constexpr int kStufenStand = 2;   // anheben, wenn sich der Ablauf bis zu einem Zustand aendert
const std::string kPlatte0Quelle = P8000_WEGA31_DIR "/p8000_wega31_platte.avr.img.gz";
const std::string kStartQuelle = P8000_WEGA31_DIR "/p8000_wega31_udos_boot.dmk";

/// Groessen wie im ausgelieferten /usr/sys/h/mdsize.h (sysgen fragt ALLE Bereiche von LW 0 neu ab).
constexpr const char* kUsr = "13000";
constexpr const char* kSwap = "3000";
constexpr const char* kRoot = "7000";
constexpr const char* kTmp = "4000";
constexpr const char* kZ = "221220";
/// LW 1 = derselbe Typ wie LW 0 (WEGA31-AVR, 1380/10/18): Zylinder 0 traegt PAR/BTT, nutzbar 1379 × 10 × 18 = 248220 Bloecke.
constexpr const char* kMd10 = "248220";

void fortschritt(const P8000Machine& m) {
    std::string z = cursorZeile(m);
    while (!z.empty() && z.front() == ' ') z.erase(z.begin());
    std::fprintf(stderr, "  [%6.0f Mio. Takte] %s\n", double(m.totalCycles()) / 1e6, z.c_str());
}

/// Auf Text im Bild warten, Fortschrittszeile je 400 Mio. Takte.
bool langBisText(P8000Machine& m, const std::string& nadel, long long grenze) {
    for (long long t = 0; t < grenze; t += 400'000'000) {
        if (laufeBisText(m, nadel, std::min<long long>(400'000'000, grenze - t), 1'000'000)) return true;
        fortschritt(m);
    }
    return false;
}

std::string stufenDatei(const std::string& name, const char* art) { return stufenPfad(name, art); }

bool dateiInhalt(const std::string& pf, std::string& s) {
    std::ifstream f(pf, std::ios::binary);
    if (!f) return false;
    s.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}

std::string stempel() {
    std::error_code ec;
    return std::to_string(kStufenStand) + " " + std::to_string(std::filesystem::file_size(kPlatte0Quelle, ec)) + " " +
           std::to_string(std::filesystem::last_write_time(kPlatte0Quelle, ec).time_since_epoch().count());
}

/// Die Anlage: P8000 mit WDC 4.2, LW 0 = WEGA 3.1, LW 1 = zweite Platte, Startdiskette in Floppy 0.
struct Anlage {
    std::string platte0 = k1520test::tempPath("k1520_zp_platte0.img.gz");
    std::string platte1 = k1520test::tempPath("k1520_zp_platte1.img.gz");
    std::string start = k1520test::tempPath("k1520_zp_start.dmk");
    std::unique_ptr<P8000Machine> m_;
    int prompt = 1;   ///< Nummer des NAECHSTEN csh-Prompts `#N`

    Anlage() = default;
    Anlage(const Anlage&) = delete;
    Anlage& operator=(const Anlage&) = delete;
    ~Anlage() {
        m_.reset();
        std::error_code ec;
        for (const auto& p : {platte0, platte1, start}) std::filesystem::remove(p, ec);
    }
    P8000Machine& m() { return *m_; }

    /// Maschine bauen; @p mit_lw1: LW 1 aus der Konfiguration (Platte existiert schon).
    ::testing::AssertionResult bauen(bool mit_lw1) {
        P8000Machine::Config c = wegaConfig(platte0, false);
        if (mit_lw1) c.platte1 = platte1;
        m_ = std::make_unique<P8000Machine>(c);
        if (!m().mountDisk(0, start, m().defaultFormatName(0), false))
            return ::testing::AssertionFailure() << "Startdiskette: " << m().lastError();
        return ::testing::AssertionSuccess();
    }

    ::testing::AssertionResult sichern(const std::string& name) {
        m().hdFlush();
        m().flushDisks();
        const auto st = m().stateBytes();
        {
            std::ofstream f(stufenDatei(name, "p8ks"), std::ios::binary);
            f.write(reinterpret_cast<const char*>(st.data()), std::streamsize(st.size()));
            if (!f) return ::testing::AssertionFailure() << "p8ks nicht schreibbar";
        }
        if (!kopiere(platte0, stufenDatei(name, "platte0.img.gz")) || !kopiere(platte1, stufenDatei(name, "platte1.img.gz")) ||
            !kopiere(start, stufenDatei(name, "start.dmk")))
            return ::testing::AssertionFailure() << "Zustand nicht kopierbar";
        std::ofstream(stufenDatei(name, "stempel")) << stempel() << " " << prompt;
        std::fprintf(stderr, "  [Zustand '%s' gesichert: %s]\n", name.c_str(), wegaAblage().c_str());
        return ::testing::AssertionSuccess();
    }

    /// Zustand laden, wenn er da ist und der Stempel passt; false sonst (dann wird er gebaut).
    bool laden(const std::string& name) {
        if (const char* e = std::getenv("K1520_ZP_NEU"); e && *e == '1') return false;
        std::string s;
        if (!dateiInhalt(stufenDatei(name, "stempel"), s)) return false;
        const std::string st = stempel() + " ";
        if (s.compare(0, st.size(), st) != 0) return false;
        prompt = std::atoi(s.c_str() + st.size());
        if (!kopiere(stufenDatei(name, "platte0.img.gz"), platte0) || !kopiere(stufenDatei(name, "platte1.img.gz"), platte1) ||
            !kopiere(stufenDatei(name, "start.dmk"), start))
            return false;
        if (!bauen(true)) return false;
        std::string roh;
        dateiInhalt(stufenDatei(name, "p8ks"), roh);
        if (!m().restoreStateBytes(std::vector<uint8_t>(roh.begin(), roh.end()))) {
            std::fprintf(stderr, "  [Zustand '%s' nicht ladbar: %s - wird neu gebaut]\n", name.c_str(), m().stateError().c_str());
            return false;
        }
        std::fprintf(stderr, "  [Zustand '%s' geladen]\n", name.c_str());
        return true;
    }

    /// Frage abwarten (Cursorzeile endet darauf), dann @p antwort tippen.
    ::testing::AssertionResult frage(const std::string& text, const std::string& antwort, long long grenze = 400'000'000) {
        if (warteAufFrage(m(), {text}, grenze) != 0) return ::testing::AssertionFailure() << "fehlt: '" << text << "'\n" << bild(m());
        laufe(m(), 200'000);
        tippeZeile(m(), antwort);
        return ::testing::AssertionSuccess();
    }

    /// Kommando am csh-Prompt; wartet auf den naechsten `#N`.
    ::testing::AssertionResult kommando(const std::string& cmd, long long grenze = 2'000'000'000LL) {
        tippeZeile(m(), cmd);
        const std::string p = "#" + std::to_string(prompt);
        if (warteAufFrage(m(), {p}, grenze) < 0) return ::testing::AssertionFailure() << "'" << cmd << "' ohne Prompt " << p << "\n" << bild(m());
        ++prompt;
        return ::testing::AssertionSuccess();
    }

    /// Ein Kommando, das `ed` interaktiv bedient: @p eingaben werden Zeile fuer Zeile getippt (ed hat keinen Prompt).
    ::testing::AssertionResult ed(const std::string& datei, const std::vector<std::string>& eingaben) {
        tippeZeile(m(), "ed " + datei);
        laufe(m(), 20'000'000);
        for (const auto& e : eingaben) {
            tippeZeile(m(), e);
            laufe(m(), 8'000'000);
        }
        tippeZeile(m(), "q");
        const std::string p = "#" + std::to_string(prompt);
        if (warteAufFrage(m(), {p}, 400'000'000) < 0) return ::testing::AssertionFailure() << "ed " << datei << " ohne Prompt\n" << bild(m());
        ++prompt;
        if (bild(m()).find("\n?\n") != std::string::npos) return ::testing::AssertionFailure() << "ed meldet '?'\n" << bild(m());
        return ::testing::AssertionSuccess();
    }

    /// Kaltstart bis `MAXSEG` (danach AUTOBOOT bzw. `*`).
    ::testing::AssertionResult bisMaxseg() {
        m().powerOn();
        if (!laufeBisText(m(), "U880-Softwaremonitor Version 3.1 - Press RETURN", 200'000'000)) return ::testing::AssertionFailure() << bild(m());
        tippe(m(), "\r");
        if (!laufeBisPrompt(m(), ">", 4'000'000)) return ::testing::AssertionFailure() << bild(m());
        tippe(m(), "\r");
        if (!laufeBisText(m(), "U8000-Softwaremonitor Version 3.1 - Press NMI", 80'000'000)) return ::testing::AssertionFailure() << bild(m());
        laufe(m(), 2'000'000);
        m().nmi();
        if (!laufeBisText(m(), "MAXSEG=<0F>", 400'000'000)) return ::testing::AssertionFailure() << bild(m());
        return ::testing::AssertionSuccess();
    }

    /// Vom Kernstart bis zur Anmeldung als `wega` (datem beantworten, Einbenutzer -> init 2).  Sammelt das Bild
    /// in @p gesehen (Zeilen, die je vorbeikamen — fuer fsck/mfs-Ausgaben).
    ::testing::AssertionResult anmelden(std::string* gesehen = nullptr) {
        std::vector<std::string> zeilen;
        auto sammle = [&] {
            if (!gesehen) return;
            for (int r = 0; r < k1520::p8000::Terminal::ZEILEN; ++r) {
                const std::string z = zeile(m(), r);
                if (!z.empty() && std::find(zeilen.begin(), zeilen.end(), z) == zeilen.end()) zeilen.push_back(z);
            }
        };
        for (;;) {
            int i = -1;
            for (long long t = 0; t < 40'000'000'000LL && i < 0; t += 100'000'000) {
                i = warteAufFrage(m(), {"login:", "#1", "Enter Date (MM/DD/YY or <cr>):", "Enter Time (HH:MM):"}, 100'000'000);
                sammle();
                if (t % 2'000'000'000LL == 0) fortschritt(m());
            }
            if (i < 0) return ::testing::AssertionFailure() << "kein login:\n" << bild(m());
            if (i == 0) break;
            laufe(m(), 200'000);
            if (i == 1) { tippeZeile(m(), "init 2"); continue; }
            tippeZeile(m(), i == 2 ? "" : "12:00");
        }
        if (gesehen) for (const auto& z : zeilen) *gesehen += z + "\n";
        laufe(m(), 200'000);
        tippeZeile(m(), "wega");
        if (!frage("Password:", "root")) return ::testing::AssertionFailure() << "kein Password:\n" << bild(m());
        if (warteAufFrage(m(), {"#1"}, 2'000'000'000LL) < 0) return ::testing::AssertionFailure() << "kein #1\n" << bild(m());
        prompt = 2;
        return ::testing::AssertionSuccess();
    }
};

/// Schritt 1–2: neue Platte an LW 1, Kaltstart, sa.format fuer LW 1, WEGA mit dem alten Kern bis zur Anmeldung.
void stufeFormatiert(Anlage& a) {
    if (a.laden("zp31_formatiert")) return;
    std::fprintf(stderr, "  [Zustand 'zp31_formatiert' wird gebaut: sa.format der ganzen Platte]\n");
    ASSERT_TRUE(kopiere(kPlatte0Quelle, a.platte0));
    ASSERT_TRUE(kopiere(kStartQuelle, a.start));
    ASSERT_TRUE(a.bauen(false));
    P8000Machine& m = a.m();
    ASSERT_TRUE(m.hdCreate(1, a.platte1, "WEGA31-AVR:unformatiert")) << m.hdError();   // „Neue Platte…", Typ wie LW 0
    ASSERT_TRUE(a.bisMaxseg());
    // LW 1 ohne Parametersatz: Hardwaretest meldet einen Fehler, kein AUTOBOOT, Monitor-Prompt.
    ASSERT_TRUE(laufeBisPrompt(m, "*", 400'000'000)) << bild(m);
    std::fprintf(stderr, "--- Hardwaretest mit unformatiertem LW 1\n%s", bild(m).c_str());
    tippeZeile(m, "O U");
    ASSERT_TRUE(laufeBisText(m, "BOOTING FROM UDOS FLOPPY", 40'000'000)) << bild(m);
    ASSERT_TRUE(laufeBisPrompt(m, ">", 40'000'000)) << bild(m);
    tippeZeile(m, "boot");
    ASSERT_TRUE(laufeBisPrompt(m, ":", 400'000'000)) << bild(m);
    tippeZeile(m, "md(0,16000)sa.format");
    ASSERT_TRUE(laufeBisText(m, "Firmwareversion 'WDC_4.2'", 400'000'000)) << bild(m);
    ASSERT_TRUE(laufeBisText(m, "Number of Drives: 2  (Drive 0 1)", 40'000'000)) << bild(m);
    ASSERT_TRUE(laufeBisText(m, "Error in PAR&BTT on Drive 1 (PAR not ok) (BTT not ok)", 40'000'000)) << bild(m);
    ASSERT_TRUE(a.frage("No. of Drive formatted:", "1"));
    // LW 0 hat einen PAR: sa.format schlaegt DESSEN Werte vor (WDC-RAM).  Genau die braucht LW 1: die Firmware 4.2
    // uebernimmt beim Hochlauf den PAR von LW 0 und verlangt fuer LW 1/2 denselben (`parram`, wdc.firm.s Z. 965) —
    // ein anderer Typ an LW 1 endet im Hardwaretest mit `ERROR 52 3A` (Init-Fehler 38 + LW 1).
    ASSERT_TRUE(laufeBisText(m, "PAR --- Cylinders: 1380  Heads: 10  Sectors: 18  Praecomp: 1380  Ramp: 1", 400'000'000)) << bild(m);
    std::fprintf(stderr, "--- sa.format LW 1\n%s", bild(m).c_str());
    ASSERT_TRUE(a.frage("Parameter for Drive ok ? (y/l/p/q)", "y"));
    ASSERT_TRUE(a.frage("Manual Input of bad Track of Drive 1 (y/n/q) ?", "n"));
    ASSERT_TRUE(a.frage("Format Begin: Cylinder (a/Start-Cylinder)", "a"));
    ASSERT_TRUE(a.frage("to Cyl 1379 Hd 9 ? (y/n/q)", "y"));
    // Ganze Platte (13800 Spuren mit Ruecklesen) ≈ 13 Mrd. Takte.
    for (long long t = 0; t < 24'000'000'000LL; t += 400'000'000) {
        if (warteAufFrage(m, {"Rewrite PAR&BTT from WDC-RAM to HD-Drive 1 ? (y/n)"}, 400'000'000) == 0) break;
        fortschritt(m);
    }
    ASSERT_TRUE(a.frage("Rewrite PAR&BTT from WDC-RAM to HD-Drive 1 ? (y/n)", "y", 40'000'000));
    EXPECT_EQ(bild(m).find("New Entry in BTT"), std::string::npos) << bild(m);
    ASSERT_TRUE(a.frage("End of 'sa.format' (y/n) ?", "y"));
    ASSERT_TRUE(laufeBisPrompt(m, ":", 400'000'000)) << bild(m);
    std::fprintf(stderr, "--- sa.format fertig\n%s", bild(m).c_str());
    // WEGA mit dem ausgelieferten Kern
    tippeZeile(m, "md(0,16000)wega");
    ASSERT_TRUE(langBisText(m, "WEGA Kernel -- Release 3.2", 2'000'000'000LL)) << bild(m);
    ASSERT_TRUE(a.anmelden());
    ASSERT_TRUE(a.sichern("zp31_formatiert"));
}

/// Schritt 3: sysgen mit zweiter Platte, neuen Kern nach /wega.
void stufeKern(Anlage& a) {
    if (a.laden("zp31_kern")) return;
    ASSERT_NO_FATAL_FAILURE(stufeFormatiert(a));
    std::fprintf(stderr, "  [Zustand 'zp31_kern' wird gebaut: sysgen]\n");
    P8000Machine& m = a.m();
    // Ausgangslage belegen: der ausgelieferte Kern kennt LW 1 nur mit Groesse 0.
    ASSERT_TRUE(a.kommando("cat /usr/sys/h/mdsize.h"));
    std::fprintf(stderr, "--- mdsize.h vorher\n%s", bild(m).c_str());
    ASSERT_TRUE(std::regex_search(bild(m), std::regex(R"(#define MD10 +0\n)"))) << bild(m);   // Tab -> Leerzeichen
    ASSERT_TRUE(a.kommando("cd /usr/sys/conf"));
    tippeZeile(m, "sysgen");
    ASSERT_TRUE(a.frage("Do you want to change system constants ? :", "n", 800'000'000));
    ASSERT_TRUE(a.frage("Do you want to change file system layout ? :", "y", 800'000'000));
    ASSERT_TRUE(a.frage("size of /usr :", kUsr));
    ASSERT_TRUE(a.frage("size of /swap :", kSwap));
    ASSERT_TRUE(a.frage("size of /root :", kRoot));
    ASSERT_TRUE(a.frage("size of /tmp :", kTmp));
    ASSERT_TRUE(a.frage("size of /z :", kZ));
    ASSERT_TRUE(a.frage("size of /md5 :", "0"));
    ASSERT_TRUE(a.frage("Do you want to add an second harddisk ? :", "y", 800'000'000));
    ASSERT_TRUE(a.frage("size of /md10 :", kMd10));
    ASSERT_TRUE(a.frage("size of /md11 :", "0"));
    ASSERT_TRUE(a.frage("Do you want to change version parameters ? :", "n", 800'000'000));
    ASSERT_TRUE(a.frage("Do you want to add new driver to the system ? :", "n", 800'000'000));
    std::fprintf(stderr, "--- sysgen Fragen beantwortet\n%s", bild(m).c_str());
    const std::string p = "#" + std::to_string(a.prompt);
    for (long long t = 0; t < 40'000'000'000LL; t += 400'000'000) {
        if (warteAufFrage(m, {p}, 400'000'000) == 0) break;
        fortschritt(m);
    }
    ASSERT_EQ(warteAufFrage(m, {p}, 1'000'000), 0) << bild(m);
    ++a.prompt;
    std::fprintf(stderr, "--- sysgen fertig\n%s", bild(m).c_str());
    EXPECT_NE(bild(m).find("New wega : /usr/sys/conf/wega"), std::string::npos) << bild(m);
    // Handbuch 5.5: alten Kern aufheben, neuen einsetzen
    ASSERT_TRUE(a.kommando("cp /wega /wega.old"));
    ASSERT_TRUE(a.kommando("cp /usr/sys/conf/wega /wega"));
    ASSERT_TRUE(a.kommando("chmod 400 /wega"));
    ASSERT_TRUE(a.kommando("sync"));
    ASSERT_TRUE(a.kommando("ls -l /wega /wega.old; cat /usr/sys/h/mdsize.h"));
    std::fprintf(stderr, "--- neuer Kern\n%s", bild(m).c_str());
    ASSERT_TRUE(a.sichern("zp31_kern"));
}

}  // namespace

TEST(P8000WegaZweitePlatte, EinrichtenMountenUndNachNeustartEingebunden) {
    stumm();
    if (!dateiDa(kStartQuelle) || !dateiDa(kPlatte0Quelle)) GTEST_SKIP() << "WEGA-3.1-Platte/Startdiskette fehlt";
    const auto t0 = std::chrono::steady_clock::now();
    Anlage a;
    ASSERT_NO_FATAL_FAILURE(stufeKern(a));
    P8000Machine& m0 = a.m();
    ASSERT_TRUE(m0.platte(1)) << "LW 1 nicht angeschlossen";

    // ── Schritt 4: Kaltstart mit dem neuen Kern (AUTOBOOT: beide Platten mit PAR) ──
    ASSERT_TRUE(a.bisMaxseg());
    P8000Machine& m = a.m();
    ASSERT_TRUE(langBisText(m, "WEGA Kernel -- Release 3.2", 2'000'000'000LL)) << bild(m);
    ASSERT_TRUE(a.anmelden());
    std::fprintf(stderr, "  [neuer Kern angemeldet: %.0f s Rechenzeit]\n",
                 std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    ASSERT_TRUE(a.kommando("/etc/mknod /dev/md10 b 0 10"));
    ASSERT_TRUE(a.kommando("/etc/mknod /dev/rmd10 c 0 10"));
    ASSERT_TRUE(a.kommando("/etc/chmog 640 bin 0 /dev/md10 /dev/rmd10"));
    ASSERT_TRUE(a.kommando("mkdir /usr2"));
    ASSERT_TRUE(a.kommando(std::string("/etc/mkfs /dev/rmd10 ") + kMd10, 80'000'000'000LL));
    std::fprintf(stderr, "--- mkfs\n%s", bild(m).c_str());
    ASSERT_TRUE(a.kommando("/etc/labelit /dev/rmd10 /usr2 1"));
    ASSERT_TRUE(a.kommando("/etc/mount /dev/md10 /usr2"));
    ASSERT_TRUE(a.kommando("/etc/mlf /usr2"));
    ASSERT_TRUE(a.kommando("/etc/chmog 644 bin 0 /usr2/lost+found"));
    // Dauerhaft einbinden: mfs (= umfs), fsck in rc_csh, checklist
    ASSERT_TRUE(a.ed("/etc/mfs", {"/etc\\/mount \\/dev\\/usr/a", "\t\t\t/etc/mount /dev/md10 /usr2", ".",
                                  "/etc\\/umount \\/dev\\/usr/a", "\t\t\t/etc/umount /dev/md10", ".", "w"}));
    ASSERT_TRUE(a.ed("/etc/rc_csh", {"/dev\\/rz/a", "\t/etc/fsck -y -t /tmp/scratch /dev/rmd10", ".", "w"}));
    ASSERT_TRUE(a.kommando("echo /dev/rmd10 >> /etc/checklist"));
    ASSERT_TRUE(a.kommando("cat /etc/mfs"));
    std::fprintf(stderr, "--- /etc/mfs\n%s", bild(m).c_str());
    EXPECT_NE(bild(m).find("/etc/mount /dev/md10 /usr2"), std::string::npos) << bild(m);
    EXPECT_NE(bild(m).find("/etc/umount /dev/md10"), std::string::npos) << bild(m);
    ASSERT_TRUE(a.kommando("cat /etc/umfs /etc/checklist; grep md10 /etc/rc_csh"));
    std::fprintf(stderr, "--- umfs/checklist/rc_csh\n%s", bild(m).c_str());
    EXPECT_NE(bild(m).find("/etc/fsck -y -t /tmp/scratch /dev/rmd10"), std::string::npos) << bild(m);
    // Daten auf die zweite Platte
    ASSERT_TRUE(a.kommando("cp /etc/passwd /usr2/passwd.kopie"));
    ASSERT_TRUE(a.kommando("echo zweite Platte LW1 > /usr2/notiz"));
    ASSERT_TRUE(a.kommando("sum /etc/passwd /usr2/passwd.kopie"));
    const std::string sum_vorher = bild(m);
    ASSERT_TRUE(a.kommando("df; /etc/mount"));
    std::fprintf(stderr, "--- df/mount\n%s", bild(m).c_str());
    EXPECT_NE(bild(m).find("/usr2 on /dev/md10"), std::string::npos) << bild(m);
    ASSERT_TRUE(a.kommando("sync"));
    ASSERT_TRUE(a.kommando("sync"));
    laufe(m, 40'000'000);

    // ── Schritt 5: Kaltstart, automatische Einbindung pruefen ──
    ASSERT_TRUE(a.bisMaxseg());
    ASSERT_TRUE(langBisText(m, "WEGA Kernel -- Release 3.2", 2'000'000'000LL)) << bild(m);
    std::string gesehen;
    ASSERT_TRUE(a.anmelden(&gesehen));
    std::fprintf(stderr, "--- Mehrbenutzerstart (gesammelte Zeilen)\n%s", gesehen.c_str());
    EXPECT_NE(gesehen.find("/dev/rmd10"), std::string::npos) << "fsck prueft /dev/rmd10 nicht\n" << gesehen;
    EXPECT_NE(gesehen.find("File System: /usr2 Volume: 1"), std::string::npos) << "fsck ohne labelit-Namen\n" << gesehen;
    // Die Ausgabe von mfs rollt mit datem aus dem Bild; der Nachweis ist /etc/mount nach der Anmeldung (Zeitpunkt
    // des Einhaengens = Mehrbenutzerstart, nicht dieses Kommando).
    ASSERT_TRUE(a.kommando("/etc/mount; cat /usr2/notiz; ls /usr2"));
    std::fprintf(stderr, "--- nach dem Neustart\n%s", bild(m).c_str());
    EXPECT_NE(bild(m).find("/usr2 on /dev/md10"), std::string::npos) << bild(m);
    EXPECT_NE(bild(m).find("zweite Platte LW1"), std::string::npos) << bild(m);
    EXPECT_NE(bild(m).find("passwd.kopie"), std::string::npos) << bild(m);
    ASSERT_TRUE(a.kommando("sum /usr2/passwd.kopie"));
    {
        // gleiche Pruefsumme wie vor dem Neustart (Zeile "NNNNN N /usr2/passwd.kopie")
        const std::string nach = bild(m);
        auto summe = [](const std::string& b) {
            const size_t p = b.rfind(" /usr2/passwd.kopie\n");
            if (p == std::string::npos) return std::string();
            const size_t a0 = b.rfind('\n', p);
            return b.substr(a0 + 1, p - a0 - 1);
        };
        EXPECT_FALSE(summe(sum_vorher).empty()) << sum_vorher;
        EXPECT_EQ(summe(sum_vorher), summe(nach)) << sum_vorher << "\n---\n" << nach;
    }
    ASSERT_TRUE(a.kommando("sync"));
    std::fprintf(stderr, "  [fertig: %.0f s Rechenzeit]\n",
                 std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
}
