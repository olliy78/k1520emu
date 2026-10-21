/**
 * @file wega_kermit_stand.h
 * @brief Gemeinsame Anlage der Kermit-Systemtests gegen WEGA 3.1: Testdateien, Plattenkopie, EINGEFRORENER
 *        Zustand (Save-State + Platte + Startdiskette, `~/.cache/k1520emu/p8000_wega`), `Anlage` (Maschine +
 *        Konsole am Originalterminal), Kaltstart bis `login:` und `Motor` (Maschine im eigenen Faden).
 *
 * Aus `test_p8000_wega_kermit_austausch.cpp` herausgeloest, damit `test_kermit80_wega.cpp` denselben
 * Zustand (`kStand`) nutzt statt einen zweiten zu bauen.  Beschreibung der Faeden und des Zustands:
 * dort (Dateikopf), Entwurf 36 §6.5.
 */
#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/logger.h"
#include "core/machines/p8000/p8000.h"
#include "core/serial/hub.h"
#include "core/util/gzip_datei.h"
#include "tests/support/p8000_input.h"
#include "tests/support/p8000_wega.h"
#include "tests/support/temp_path.h"

using namespace k1520test::p8000;
using namespace k1520::serial;
namespace fs = std::filesystem;

#ifndef P8000_WEGA31_DIR
#define P8000_WEGA31_DIR "disks/p8000_wega31"
#endif
#ifndef K1520_DISKTOOL
#define K1520_DISKTOOL "k1520disktool"
#endif

namespace {

constexpr const char* kStand = "kermit31_k7_v3";   // Version anheben, wenn sich die Testdateien aendern
constexpr double kTakt = 4e6;                        // P8000: 4 MHz Maschinentakt (Zeitrechnung wie K1)

void stumm() { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR); }

std::string rtrim(std::string s) {
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

std::string sichtbar(const std::string& s, size_t max = 600) {
    std::string o;
    char b[8];
    for (unsigned char c : s) {
        if (c == '\r') o += "\\r";
        else if (c == '\n') o += "\\n";
        else if (c >= 32 && c < 127) o += char(c);
        else { std::snprintf(b, sizeof b, "<%02X>", c); o += b; }
    }
    if (o.size() > max) o = "..." + o.substr(o.size() - max);
    return o;
}

std::string lies(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}
void schreibe(const fs::path& p, const std::string& s) { std::ofstream(p, std::ios::binary) << s; }

// ─── Testdateien (deterministisch; dieselben Bytes landen per DiskTool auf der Platte) ─────────

std::string zufall(size_t n, unsigned saat) {
    std::mt19937 g(saat);
    std::string s(n, '\0');
    for (auto& c : s) c = char(g() & 0xFF);
    return s;
}

const std::map<std::string, std::string>& testdateien() {
    static const std::map<std::string, std::string> d = [] {
        std::map<std::string, std::string> m;
        std::string t;
        char z[96];
        for (int i = 1; i <= 100; ++i) {
            std::snprintf(z, sizeof z, "Zeile %03d: C-Kermit 4.2 Gegenprobe, Text mit LF am Zeilenende.\n", i);
            t += z;
        }
        m["text.txt"] = t;
        std::string a(4096, '\0');
        for (size_t i = 0; i < a.size(); ++i) a[i] = char(i & 0xFF);   // jeder Bytewert 16x
        m["alle256"] = a;
        m["zufall100k"] = zufall(102400, 1520);
        m["leer"] = "";
        m["eins"] = std::string(1, char(0xE5));
        m["b94"] = zufall(94, 94);
        m["b95"] = zufall(95, 95);
        m["vierzehn_zeich"] = "genau vierzehn Zeichen im Namen\n";   // 14 Zeichen = WEGA-Grenze
        m["MixedCase.Txt"] = "Gross und klein\n";
        return m;
    }();
    return d;
}

// ─── Plattenkopie und eingefrorener Zustand ──────────────────────────────────────────────────

std::string quellPlatte() {
    if (const char* e = std::getenv("K1520_WEGA31_PLATTE"); e && *e) return e;
    return P8000_WEGA31_DIR "/p8000_wega31_platte.avr.img.gz";
}
std::string quellStart() { return P8000_WEGA31_DIR "/p8000_wega31_udos_boot.dmk"; }

bool entpacke(const std::string& q, const std::string& ziel) {
    std::vector<uint8_t> d;
    std::string f;
    if (!k1520::gzip::laden(q, d, nullptr, &f)) return false;
    std::ofstream o(ziel, std::ios::binary);
    o.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
    return bool(o);
}

std::string stempel() {
    std::error_code ec;
    std::ostringstream s;
    s << kStand << " " << fs::file_size(quellPlatte(), ec) << " "
      << fs::last_write_time(quellPlatte(), ec).time_since_epoch().count() << " " << fs::file_size(quellStart(), ec);
    return s.str();
}

std::string standPfad(const char* art) { return wegaAblage() + "/" + kStand + "." + art; }

bool standAktuell() {
    return dateiDa(standPfad("p8ks")) && dateiDa(standPfad("platte.img")) && dateiDa(standPfad("start.dmk")) &&
           lies(standPfad("stempel")) == stempel();
}

/// Konfiguration: Vollgeraet (16 Bit, WDC 4.2), Konsole am Originalterminal (wie K1).
P8000Machine::Config konfig(const std::string& platte) {
    P8000Machine::Config c = wegaConfig(platte, false);
    c.terminal = P8000Machine::Config::TerminalArt::Original;
    return c;
}

/// Die Anlage: Maschine + Konsole (Originalterminal) + Hilfen fuer den Testfaden.
struct Anlage {
    std::string platte = k1520test::tempPath("k1520_k7_platte.img");
    std::string start = k1520test::tempPath("k1520_k7_start.dmk");
    std::unique_ptr<P8000Machine> mp;

    ~Anlage() {
        mp.reset();
        std::error_code ec;
        fs::remove(platte, ec);
        fs::remove(start, ec);
    }
    P8000Machine& m() { return *mp; }
    double sekunden() { return double(m().totalCycles()) / kTakt; }
    std::string zeile(int z) { return rtrim(m().konsole().text(z)); }
    std::string cursorZeile() { return zeile(m().konsole().zeile()); }
    std::string bild() {
        std::string s;
        for (int z = 0; z < 24; ++z) s += zeile(z) + "\n";
        return s;
    }
    void laufe(long long takte) {
        for (long long t = 0; t < takte; t += kBatch) m().run(int(kBatch));
    }
    bool bisText(const std::string& nadel, long long grenze) {
        for (long long t = 0; t < grenze; t += 100'000) {
            laufe(100'000);
            if (bild().find(nadel) != std::string::npos) return true;
        }
        return false;
    }
    int frage(const std::vector<std::string>& fragen, long long grenze) {
        auto passt = [&]() -> int {
            const std::string z = cursorZeile();
            for (size_t i = 0; i < fragen.size(); ++i)
                if (endetMit(z, rtrim(fragen[i]))) return int(i);
            return -1;
        };
        for (long long t = 0; t < grenze; t += 100'000) {
            laufe(100'000);
            if (int i = passt(); i >= 0) return i;
        }
        return passt();
    }
    void tippe(const std::string& text) {
        for (char c : text) m().keyPress((c == '\r' || c == '\n') ? 0x01000004u : uint8_t(c), false, false);
        for (long long t = 0; t < 400'000'000 && !m().originalTerminal()->tastenFertig(); t += kBatch) m().run(int(kBatch));
    }
    void tippeZeile(const std::string& text) { tippe(text + "\r"); }
    static bool istPrompt(const std::string& z) {
        static const std::regex re(R"((^|\s)([#%][0-9]+|[$#%])$)");
        return std::regex_search(z, re);
    }
    bool bisPrompt(long long grenze) {
        laufe(400'000);
        for (long long t = 0; t < grenze; t += 100'000) {
            laufe(100'000);
            if (m().originalTerminal()->tastenFertig() && istPrompt(cursorZeile())) return true;
        }
        return false;
    }
    /// Wartet, bis die Cursorzeile auf `C-Kermit>` endet (nach einem Kommando der Konsole).
    bool bisKermitPrompt(long long grenze) {
        laufe(4'000'000);
        return frage({"C-Kermit>"}, grenze) >= 0;
    }
    ::testing::AssertionResult anmeldenKonsole(const std::string& name, const std::string& pw) {
        if (frage({"login:"}, 40'000'000) < 0) {
            tippe("\r");
            if (frage({"login:"}, 400'000'000) < 0) return ::testing::AssertionFailure() << "kein login:\n" << bild();
        }
        laufe(200'000);
        tippeZeile(name);
        if (frage({"Password:"}, 400'000'000) < 0) return ::testing::AssertionFailure() << "kein Password:\n" << bild();
        laufe(200'000);
        tippeZeile(pw);
        for (long long t = 0; t < 2'000'000'000LL; t += 1'000'000) {
            laufe(1'000'000);
            if (m().originalTerminal()->tastenFertig() && istPrompt(cursorZeile())) return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << "kein Prompt nach der Anmeldung\n" << bild();
    }

    /// Hub-Kanal @p name als Telnet-Server (Port 0) oder -Client; liefert den Port des Servers.
    int kanal(const std::string& name, Rolle rolle, int port = 0) {
        SerialHub* hub = m().serialHub();
        for (int i = 0; i < hub->anzahl(); ++i) {
            if (hub->info(i).name != name) continue;
            SerialKonfig k = hub->konfig(i);
            k.betriebsart = Betriebsart::Telnet;
            k.rolle = rolle;
            k.host = "127.0.0.1";
            k.port = uint16_t(port);
            k.loop = false;
            if (!hub->konfigurieren(i, k) || !hub->start(i)) return -1;
            return rolle == Rolle::Server ? hub->status(i).port_aktiv : port;
        }
        return -1;
    }
    Zustand kanalZustand(const std::string& name) {
        SerialHub* hub = m().serialHub();
        for (int i = 0; i < hub->anzahl(); ++i)
            if (hub->info(i).name == name) return hub->status(i).zustand;
        return Zustand::Fehler;
    }
};

/// Kaltstart wie am Geraet bis `login:` an der Konsole (wie test_p8000_wega_kermit.cpp).
void kaltstartBisLogin(Anlage& a) {
    P8000Machine& m = a.m();
    m.powerOn();
    ASSERT_TRUE(a.bisText("U880-Softwaremonitor Version 3.1 - Press RETURN", 200'000'000)) << a.bild();
    a.tippe("\r");
    ASSERT_GE(a.frage({">"}, 20'000'000), 0) << a.bild();
    a.tippe("\r");
    ASSERT_TRUE(a.bisText("U8000-Softwaremonitor Version 3.1 - Press NMI", 80'000'000)) << a.bild();
    a.laufe(2'000'000);
    m.nmi();
    ASSERT_TRUE(a.bisText("MAXSEG=<0F>", 400'000'000)) << a.bild();
    ASSERT_TRUE(a.bisText("WEGA Kernel -- Release 3.2", 2'000'000'000LL)) << a.bild();
    for (;;) {
        const int i = a.frage({"Enter Date (MM/DD/YY or <cr>):", "Enter Time (HH:MM):", "login:", "#1"}, 8'000'000'000LL);
        ASSERT_GE(i, 0) << a.bild();
        if (i == 2) break;
        a.laufe(200'000);
        if (i == 3) { a.tippeZeile("init 2"); continue; }
        a.tippeZeile(i == 0 ? "" : "21:10");
    }
}

bool disktool(const std::string& args) {
    const std::string cmd = std::string("\"") + K1520_DISKTOOL + "\" " + args + " > /dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

/// Befund K7: C-Kermit 4.2 fuehrt die Serverbefehle G D/G E/G T/G U/G W ueber die LOGIN-SHELL aus
/// `/etc/passwd` aus (ckzunx.c zxcmd: execl(pw_shell, "-c", "ls -l ...")).  Mit der Shell
/// `/usr/bin/kermit` (Stand K1) lieferte G D deshalb die Gebrauchsanweisung von kermit.  Abhilfe:
/// Shell `/bin/sh`, `/usr/kermit/.profile` = `exec /usr/bin/kermit` (dann liest kermit `.kermrc`).
/// Steht die Platte schon so (ausgelieferte Fassung nach K7), aendert das hier nichts.
::testing::AssertionResult kermitShellAufSh(const std::string& platte) {
    const std::string dir = k1520test::tempPath("k1520_k7_pw");
    fs::create_directories(dir);
    if (!disktool("get \"" + platte + "\" md2/etc/passwd --to \"" + dir + "\""))
        return ::testing::AssertionFailure() << "passwd nicht lesbar";
    const fs::path pw = fs::path(dir) / "md2" / "etc" / "passwd";
    std::string t = lies(pw);
    const std::string alt = ":/usr/kermit:/usr/bin/kermit\n", neu = ":/usr/kermit:/bin/sh\n";
    const size_t p = t.find(alt);
    if (p != std::string::npos) {
        t.replace(p, alt.size(), neu);
        schreibe(pw, t);
        if (!disktool("put \"" + platte + "\" \"" + pw.string() + "\" --as md2/etc/passwd --no-backup --force") ||
            !disktool("attr \"" + platte + "\" md2/etc/passwd --uid 0 --gid 0 --mode 0644 --no-backup"))
            return ::testing::AssertionFailure() << "passwd nicht schreibbar";
        const fs::path pr = fs::path(dir) / ".profile";
        schreibe(pr, "exec /usr/bin/kermit\n");
        if (!disktool("put \"" + platte + "\" \"" + pr.string() + "\" --as md0/kermit/.profile --no-backup --force") ||
            !disktool("attr \"" + platte + "\" md0/kermit/.profile --uid 7 --gid 1 --mode 0644 --no-backup"))
            return ::testing::AssertionFailure() << ".profile nicht schreibbar";
        std::fprintf(stderr, "  [Platte (Kopie): Shell des Benutzers kermit -> /bin/sh + .profile]\n");
    } else if (t.find(neu) == std::string::npos) {
        return ::testing::AssertionFailure() << "Benutzer kermit nicht in passwd:\n" << t;
    }
    // Befund K7: `set file names literal` legt in C-Kermit 4.2 das Holen lahm — sfile() kopiert bei
    // fncnv == 0 ohne Sendenamen den UNINITIALISIERTEN Paketnamen in den Dateinamen
    // (`strcpy(filnam,pktnam)` statt umgekehrt), jedes R endet mit „Can't open file".
    if (!disktool("get \"" + platte + "\" md0/kermit/.kermrc --to \"" + dir + "\""))
        return ::testing::AssertionFailure() << ".kermrc nicht lesbar";
    const fs::path rc = fs::path(dir) / "md0" / "kermit" / ".kermrc";
    std::string k = lies(rc);
    if (const size_t q = k.find("set file names literal\n"); q != std::string::npos) {
        k.erase(q, std::string("set file names literal\n").size());
        schreibe(rc, k);
        if (!disktool("put \"" + platte + "\" \"" + rc.string() + "\" --as md0/kermit/.kermrc --no-backup --force") ||
            !disktool("attr \"" + platte + "\" md0/kermit/.kermrc --uid 7 --gid 1 --mode 0644 --no-backup"))
            return ::testing::AssertionFailure() << ".kermrc nicht schreibbar";
        std::fprintf(stderr, "  [Platte (Kopie): .kermrc ohne 'set file names literal']\n");
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
    return ::testing::AssertionSuccess();
}

/// Testdateien per DiskTool nach md0/kermit (= /usr/kermit), Besitzer kermit (7:1), 0644.
::testing::AssertionResult dateienAufPlatte(const std::string& platte) {
    const std::string tmp = k1520test::tempPath("k1520_k7_datei");
    for (const auto& [name, inhalt] : testdateien()) {
        schreibe(tmp, inhalt);
        const std::string ziel = "md0/kermit/" + name;
        const std::string put = std::string("\"") + K1520_DISKTOOL + "\" put \"" + platte + "\" \"" + tmp + "\" --as " + ziel +
                                " --no-backup --force > /dev/null 2>&1";
        const std::string attr = std::string("\"") + K1520_DISKTOOL + "\" attr \"" + platte + "\" " + ziel +
                                 " --uid 7 --gid 1 --mode 0644 --no-backup > /dev/null 2>&1";
        if (std::system(put.c_str()) != 0) return ::testing::AssertionFailure() << "put scheitert: " << put;
        if (std::system(attr.c_str()) != 0) return ::testing::AssertionFailure() << "attr scheitert: " << attr;
    }
    std::error_code ec;
    fs::remove(tmp, ec);
    return ::testing::AssertionSuccess();
}

/// Baut den eingefrorenen Zustand (einmal ~90 s Rechenzeit).
void standBauen() {
    Anlage a;
    ASSERT_TRUE(entpacke(quellPlatte(), a.platte)) << quellPlatte();
    ASSERT_TRUE(kermitShellAufSh(a.platte));
    ASSERT_TRUE(dateienAufPlatte(a.platte));
    ASSERT_TRUE(kopiere(quellStart(), a.start));
    a.mp = std::make_unique<P8000Machine>(konfig(a.platte));
    ASSERT_TRUE(a.m().mountDisk(0, a.start, a.m().defaultFormatName(0), false)) << a.m().lastError();
    const auto t0 = std::chrono::steady_clock::now();
    ASSERT_NO_FATAL_FAILURE(kaltstartBisLogin(a));
    a.laufe(4'000'000);
    a.m().hdFlush();
    a.m().flushDisks();
    const auto st = a.m().stateBytes();
    {
        std::ofstream f(standPfad("p8ks"), std::ios::binary);
        f.write(reinterpret_cast<const char*>(st.data()), std::streamsize(st.size()));
        ASSERT_TRUE(bool(f));
    }
    ASSERT_TRUE(kopiere(a.platte, standPfad("platte.img")));
    ASSERT_TRUE(kopiere(a.start, standPfad("start.dmk")));
    schreibe(standPfad("stempel"), stempel());
    std::fprintf(stderr, "  [Zustand %s gebaut: %.0f s Maschinenzeit, %.0f s Rechenzeit, %s]\n", kStand, a.sekunden(),
                 std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), wegaAblage().c_str());
}

/// Laedt den Zustand in Temp-Kopien (baut ihn bei Bedarf).  false + GTEST_SKIP-Grund in @p grund.
bool standLaden(Anlage& a, std::string* grund) {
    if (!dateiDa(quellStart())) { *grund = "Startdiskette fehlt: " + quellStart(); return false; }
    if (!dateiDa(quellPlatte())) { *grund = "Platte fehlt: " + quellPlatte(); return false; }
    if (!dateiDa(K1520_DISKTOOL)) { *grund = std::string("DiskTool fehlt: ") + K1520_DISKTOOL; return false; }
    if (!standAktuell()) {
        standBauen();
        if (::testing::Test::HasFatalFailure()) { *grund = "Zustand nicht baubar"; return false; }
    }
    if (!kopiere(standPfad("platte.img"), a.platte) || !kopiere(standPfad("start.dmk"), a.start)) {
        *grund = "Zustand nicht kopierbar";
        return false;
    }
    a.mp = std::make_unique<P8000Machine>(konfig(a.platte));
    if (!a.m().mountDisk(0, a.start, a.m().defaultFormatName(0), false)) { *grund = a.m().lastError(); return false; }
    std::ifstream f(standPfad("p8ks"), std::ios::binary);
    std::vector<uint8_t> st{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    if (!a.m().restoreStateBytes(st)) { *grund = "Save-State: " + a.m().stateError(); return false; }
    return true;
}

// ─── Motor: die Maschine im eigenen Faden (frei oder auf ein Tempo gedrosselt) ────────────────

struct Motor {
    P8000Machine& m;
    std::atomic<bool> halt{false};
    std::atomic<uint64_t> takte{0};
    std::thread t;
    explicit Motor(P8000Machine& mm) : m(mm) { takte = m.totalCycles(); }
    /// @p tempo 0 = so schnell wie moeglich, sonst Maschinenzeit = tempo x Wirtszeit.
    void start(double tempo) {
        halt = false;
        t = std::thread([this, tempo] {
            const auto w0 = std::chrono::steady_clock::now();
            const uint64_t c0 = m.totalCycles();
            while (!halt) {
                m.run(int(kBatch));
                const uint64_t c = m.totalCycles();
                takte = c;
                if (tempo > 0) {
                    const double soll_ms = double(c - c0) / kTakt * 1000.0 / tempo;
                    const double ist_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - w0).count();
                    if (soll_ms > ist_ms + 2) std::this_thread::sleep_for(std::chrono::microseconds(int64_t((soll_ms - ist_ms) * 1000)));
                }
            }
        });
    }
    void stop() {
        halt = true;
        if (t.joinable()) t.join();
    }
    ~Motor() { stop(); }
    double sekunden() const { return double(takte.load()) / kTakt; }
};

}  // namespace
