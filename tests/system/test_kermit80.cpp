/**
 * @file test_kermit80.cpp
 * @brief Kermit-80 4.11 (Columbia University, 1991) unter CP/A, SCPX usw. im Emulator gegen die
 *        eigene Kermit-Bibliothek (`kermit/`) — Entwurf 36 §6.6.
 *
 * Kermit-80 ist NUR Client (laut CPKERM.TXT „Act as server: No", „Talk to server: Yes"): die
 * Bibliothek arbeitet hier als SERVER (`Sitzung::serve` ueber `Freigabe`), Kermit-80 sendet
 * (`SEND`), holt (`GET`) und fragt Serverbefehle (`REMOTE DIRECTORY`, `FINISH`).
 *
 * Die Ueberlagerungen je Rechner liegen eingecheckt unter `tools/kermit80/kermit_<rechner>.com`
 * (`bau.py` erzeugt sie aus `tools/kermit80/quelle/`; der Fall `Kermit80Bau.*` prueft, dass sie
 * dazu passen — er braucht die CP/M-Werkzeugkette `CPA_TOOLS` und wird sonst uebersprungen).
 * Umgebungsvariable `K1520_KERMIT80_COM_<RECHNER>` (z. B. `..._A5120`) setzt eine andere .COM.
 *
 * Neben dem A5120 (eigene Fassungen oben) prueft `Kermit80Rechner.*` die Ueberlagerungen fuer
 * K8915, PC 1715 und PRG 710/710-1: Kermit-80 meldet sich und schickt sich selbst an den PC-Server.
 *
 * Die Maschine laeuft im Testfaden (und tippt am CP/A-Prompt), die Bibliothek im eigenen Faden;
 * die V.24-Leitung der Maschine ist ein Hub-Kanal als Telnet-CLIENT zum lauschenden PC-Server.
 */
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/p8000/p8000.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "core/serial/hub.h"
#include "kermit/src/leitung_netz.h"
#include "kermit/src/protokoll.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/p8000_input.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"
#include "tests/system/kermit80_hilfen.h"
#include "tests/system/sertest_hilfen.h"

#ifndef K1520_OSM_P8000_DISK
#error "K1520_OSM_P8000_DISK fehlt (disks/p8000_osm30/p8000_osm30_system.hfe)"
#endif
#ifndef K1520_KERMIT80_DIR
#error "K1520_KERMIT80_DIR fehlt (tools/kermit80/)"
#endif

namespace fs = std::filesystem;
namespace km = k1520::kermit;
using namespace k1520::serial;

namespace {

void stumm() { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR); }

std::string lies(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}
void schreibe(const fs::path& p, const std::string& s) { std::ofstream(p, std::ios::binary) << s; }

std::string zufall(size_t n, unsigned saat) {
    std::mt19937 g(saat);
    std::string s(n, '\0');
    for (auto& c : s) c = char(g() & 0xFF);
    return s;
}

/// Auf 128-Byte-Records mit @p fuell aufgefuellt (CP/M kennt keine Bytelaenge).
std::string recordAuffuellen(std::string s, char fuell) {
    while (s.size() % 128) s += fuell;
    return s;
}

struct TempOrdner {
    fs::path p = fs::path(k1520test::tempPath("k1520_k80_ordner"));
    TempOrdner() { fs::remove_all(p); fs::create_directories(p); }
    ~TempOrdner() { std::error_code ec; fs::remove_all(p, ec); }
};

/// Eingecheckte Kermit-80-Ueberlagerung fuer @p rechner; leer + @p grund, wenn sie fehlt.
std::string kermitCom(const std::string& rechner, std::string* grund) {
    std::string up = rechner;
    for (auto& c : up) c = char(std::toupper(static_cast<unsigned char>(c)));
    if (const char* e = std::getenv(("K1520_KERMIT80_COM_" + up).c_str()); e && *e) return e;
    const std::string pfad = std::string(K1520_KERMIT80_DIR) + "/kermit_" + rechner + ".com";
    if (!fs::exists(pfad)) {
        *grund = "fehlt: " + pfad;
        return "";
    }
    return pfad;
}

/// KERMIT.COM mit dem DiskTool auf die Diskette @p pfad schreiben; leer bei Erfolg.
std::string aufspielen(const std::string& pfad, const std::string& com, const std::string& dateiname = "KERMIT.COM") {
    std::string f;
    static const FormatCatalog formate = FormatCatalog::loadDefault(&f);
    static const FsCatalog fsk = FsCatalog::loadDefault(formate, &f);
    std::string err;
    auto v = DiskVolume::open(pfad, "", formate, fsk, err, /*read_only=*/false);
    if (!v) return "DiskVolume::open: " + err;
    v->setBackup(false);
    TransferOptions o;
    o.overwrite = true;
    if (!v->insert(com, FileRef::parse(dateiname), o)) return "insert: " + v->lastError();
    if (!v->flush()) return "flush: " + v->lastError();
    return {};
}

std::string rtrim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    return s;
}

// ═════════════════════════════════════════════════════════════════════════════════════════════
// A5120 mit CP/A
// ═════════════════════════════════════════════════════════════════════════════════════════════

class K80A5120 {
public:
    static constexpr const char* kFixture = "cpa_cpa780_k5601_noclock.img";
    explicit K80A5120(const std::string& com, const std::map<std::string, std::string>& dateien = {})
        : disk_(kFixture, std::string("kermit80_a5120_") + kFixture) {
        fehler_ = aufspielen(disk_.path(), com);
        for (const auto& [name, inhalt] : dateien) {
            if (!fehler_.empty()) break;
            const fs::path tmp = fs::path(k1520test::tempPath("k1520_k80_dat"));
            fs::create_directories(tmp);
            schreibe(tmp / "quelle", inhalt);
            fehler_ = aufspielen(disk_.path(), (tmp / "quelle").string(), name);
            fs::remove_all(tmp);
        }
        if (fehler_.empty() && !m_.mountDisk(0, disk_.path(), "cpa780", false)) fehler_ = "mountDisk: " + m_.lastError();
    }
    const std::string& fehler() const { return fehler_; }
    A5120Machine& m() { return m_; }

    /// Dateien der Diskette (nach flushDisks) — Name -> Inhalt (Nutzbytes, 128-B-Records), leer wenn keine.
    std::map<std::string, std::string> diskDateien(std::string* fehler = nullptr) {
        std::map<std::string, std::string> r;
        for (int i = 0; i < 6; ++i) schritt();
        m_.flushDisks();
        std::string f;
        static const FormatCatalog formate = FormatCatalog::loadDefault(&f);
        static const FsCatalog fsk = FsCatalog::loadDefault(formate, &f);
        std::string err;
        auto v = DiskVolume::open(disk_.path(), "", formate, fsk, err, /*read_only=*/true);
        if (!v) { if (fehler) *fehler = err; return r; }
        for (const FileEntry& e : v->list()) {
            const fs::path tmp = fs::path(k1520test::tempPath("k1520_k80_aus"));
            if (!v->extract(FileRef::parse(e.name), tmp.string(), TransferOptions{})) continue;
            r[(e.user ? std::to_string(e.user) + ":" : "") + e.name] = lies(tmp);
            fs::remove(tmp);
        }
        return r;
    }

    long long takte = 0;
    long long schritt() { const long long n = m_.run(k1520test::kSmallBatch); takte += n; return n; }
    std::string bild() { return k1520test::vramLines(m_); }
    void lauf(long long takte) {
        for (long long t = 0; t < takte; t += k1520test::kSmallBatch) schritt();
    }
    static std::string letzte(const std::string& bild) {
        std::string l;
        size_t a = 0;
        while (a <= bild.size()) {
            size_t e = bild.find('\n', a);
            if (e == std::string::npos) e = bild.size();
            const std::string z = rtrim(bild.substr(a, e - a));
            if (!z.empty()) l = z;
            a = e + 1;
        }
        return l;
    }
    bool bisPrompt(long long frist) {
        for (long long t = 0; t < frist; t += k1520test::kSmallBatch) {
            schritt();
            if (letzte(bild()) == "A>") return true;
        }
        return false;
    }
    bool kaltstart() {
        m_.powerOn();
        return bisPrompt(200'000'000);
    }
    void tippe(const std::string& s) {
        for (char c : s) {
            if (c == '\r') k1520test::typeKey(m_, k1520test::QK_RETURN);
            else k1520test::typeKey(m_, static_cast<uint8_t>(c));
        }
    }
    /// Hub-Kanal V.24 als Telnet-Client; leer bei Erfolg.
    std::string kanalClient(int port) {
        SerialHub* hub = m_.serialHub();
        for (int i = 0; i < hub->anzahl(); ++i) {
            if (hub->info(i).name.find("V.24") == std::string::npos) continue;
            SerialKonfig k = hub->konfig(i);
            k.betriebsart = Betriebsart::Telnet;
            k.rolle = Rolle::Client;
            k.host = "127.0.0.1";
            k.port = uint16_t(port);
            k.loop = false;
            if (!hub->konfigurieren(i, k) || !hub->start(i)) return "Hub-Kanal starten: " + hub->status(i).meldung;
            for (int n = 0; n < 400 && hub->status(i).zustand != Zustand::Verbunden; ++n) lauf(200'000);
            return hub->status(i).zustand == Zustand::Verbunden ? "" : "nicht verbunden: " + hub->status(i).meldung;
        }
        return "kein V.24-Kanal";
    }

private:
    k1520test::TempDisk disk_;
    A5120Machine m_;
    std::string fehler_;
};

/// Der PC-Server der Bibliothek in eigenem Faden.
struct PcServer {
    TempOrdner frei;
    std::unique_ptr<km::TelnetLeitung> lausch;
    std::thread t;
    std::atomic<bool> fertig{false};
    km::Ergebnis ergebnis;
    km::Statistik stat;
    km::Einstellungen einst;
    std::mutex em;
    std::vector<std::string> ereignisse;
    std::vector<std::string> pakete;

    bool start(std::string* f) {
        lausch = km::TelnetLeitung::lauschen(0, {}, f);
        if (!lausch) return false;
        t = std::thread([this] {
            std::string fe;
            if (!lausch->annehmen(600000, &fe)) {
                ergebnis.art = km::Fehlerart::LeitungGetrennt;
                ergebnis.text = "annehmen: " + fe;
                fertig = true;
                return;
            }
            km::Freigabe fg(frei.p);
            km::Rueckrufe r;
            r.ereignis = [&](const std::string& s) {
                std::lock_guard<std::mutex> lk(em);
                ereignisse.push_back(s);
                if (std::getenv("K1520_KERMIT_LAUT")) std::fprintf(stderr, "    [PC-Server] %s\n", s.c_str());
            };
            if (std::getenv("K1520_KERMIT_PAKETE"))
                r.paket = [](bool g, char ty, int q, const std::string& d) {
                    std::string o;
                    for (unsigned char c : d) {
                        char b[8];
                        if (c >= 32 && c < 127) o += char(c);
                        else { std::snprintf(b, sizeof b, "<%02X>", c); o += b; }
                    }
                    if (o.size() > 100) o = o.substr(0, 100) + "...";
                    std::fprintf(stderr, "    %s %c%-2d %s\n", g ? ">>" : "<<", ty, q, o.c_str());
                };
            km::Sitzung s(*lausch, einst, r);
            ergebnis = s.serve(fg);
            stat = s.statistik();
            fertig = true;
        });
        return true;
    }
    int port() const { return lausch->port(); }
    ~PcServer() {
        if (lausch) { lausch->wecken(); lausch->schliessen(); }
        if (t.joinable()) t.join();
    }
};

/// Bedienung von Kermit-80 am CP/A-Prompt; gibt jedes Bild mit aus (Messprotokoll).
struct K80Bedienung {
    K80A5120& a;
    bool laut = std::getenv("K1520_KERMIT_LAUT") != nullptr;

    bool imKermit(const std::string& bild) {
        const std::string l = K80A5120::letzte(bild);
        return l.rfind("Kermit-80", 0) == 0 && l.back() == '>';
    }
    bool bisKermit(long long frist) {
        for (long long t = 0; t < frist; t += k1520test::kSmallBatch) {
            a.schritt();
            if (imKermit(a.bild())) return true;
        }
        return false;
    }
    /// Befehl tippen und bis zum naechsten Kermit-Prompt laufen.
    bool befehl(const std::string& cmd, long long frist = 2'000'000'000LL) {
        const double m0 = double(a.takte);
        const auto w0 = std::chrono::steady_clock::now();
        a.tippe(cmd + "\r");
        // erst abwarten, bis das Echo den Prompt aus der letzten Zeile geschoben hat
        for (long long t = 0; t < 2'000'000 && imKermit(a.bild()); t += k1520test::kSmallBatch) a.schritt();
        const bool ok = bisKermit(frist);
        if (laut)
            std::fprintf(stderr, "--- Kermit-80> %s  (%.1f s Maschine, %.1f s Wirt)\n%s", cmd.c_str(),
                         (double(a.takte) - m0) / 2.4576e6,
                         std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count(), a.bild().c_str());
        return ok;
    }
};

}  // namespace


/// Ein Lauf: A5120 gebootet, PC-Server lauscht, V.24 verbunden, Kermit-80 gestartet.
struct Lauf {
    K80A5120 a;
    PcServer srv;
    K80Bedienung b{a};
    Lauf(const std::string& com, const std::map<std::string, std::string>& disk = {}) : a(com, disk) {}

    ::testing::AssertionResult bereit(const std::vector<std::string>& einstellungen = {}) {
        if (!a.fehler().empty()) return ::testing::AssertionFailure() << a.fehler();
        if (!a.kaltstart()) return ::testing::AssertionFailure() << "kein A>\n" << a.bild();
        std::string f;
        if (!srv.start(&f)) return ::testing::AssertionFailure() << f;
        const std::string e = a.kanalClient(srv.port());
        if (!e.empty()) return ::testing::AssertionFailure() << e;
        a.tippe("kermit\r");
        if (!b.bisKermit(400'000'000)) return ::testing::AssertionFailure() << "kein Kermit-80>\n" << a.bild();
        for (const auto& s : einstellungen)
            if (!b.befehl(s)) return ::testing::AssertionFailure() << s << "\n" << a.bild();
        return ::testing::AssertionSuccess();
    }
    /// Messzeile (stderr) zu einem Befehl: Maschinen- und Wirtszeit, Pakete des Servers.
    bool messen(const std::string& cmd, size_t bytes, long long frist = 20'000'000'000LL) {
        const long long t0 = a.takte;
        const auto w0 = std::chrono::steady_clock::now();
        const bool ok = b.befehl(cmd, frist);
        const double ms = double(a.takte - t0) / 2.4576e6;
        const double ws = std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count();
        std::fprintf(stderr, "  MESSUNG %-34s %7zu B  %7.1f s Maschine  %6.1f s Wirt  %6.0f B/s Maschine\n", cmd.c_str(), bytes,
                     ms, ws, bytes / std::max(1e-3, ms));
        return ok;
    }
};

#define KERMIT80_COM_ODER_SKIP(rechner) \
    std::string grund; \
    const std::string com = kermitCom(rechner, &grund); \
    if (com.empty()) GTEST_SKIP() << grund

// ─── Kermit-80 holt Dateien vom PC-Server (GET) ───────────────────────────────────────────────

TEST(Kermit80A5120, GetGroessenUndInhalt) {
    stumm();
    KERMIT80_COM_ODER_SKIP("a5120");
    Lauf l(com);
    const std::string zuf = zufall(102400, 80);
    std::string alle(4096, '\0');
    for (size_t i = 0; i < alle.size(); ++i) alle[i] = char(i & 0xFF);
    const std::map<std::string, std::string> pc = {{"g0", ""}, {"g1", std::string(1, char(0xE5))}, {"g128", zufall(128, 128)},
                                                   {"g129", zufall(129, 129)}, {"alle256", alle}, {"g100k", zuf}};
    for (const auto& [n, s] : pc) schreibe(l.srv.frei.p / n, s);
    ASSERT_TRUE(l.bereit({"set file-mode binary"}));
    for (const auto& [n, s] : pc) ASSERT_TRUE(l.messen("get " + n, s.size())) << l.a.bild();
    ASSERT_TRUE(l.b.befehl("finish"));
    std::string fe;
    const auto auf = l.a.diskDateien(&fe);
    ASSERT_EQ(fe, "");
    for (const auto& [n, s] : pc) {
        std::string up = n;
        for (auto& c : up) c = char(std::toupper(static_cast<unsigned char>(c)));
        auto it = auf.find(up);
        if (it == auf.end()) { ADD_FAILURE() << "Datei " << up << " fehlt auf der Diskette"; continue; }
        const std::string& d = it->second;
        std::fprintf(stderr, "  %-8s PC %6zu B  CP/M %6zu B  Auffuellbytes:", up.c_str(), s.size(), d.size());
        for (size_t i = s.size(); i < d.size() && i < s.size() + 3; ++i) std::fprintf(stderr, " %02X", (unsigned char)d[i]);
        std::fprintf(stderr, "\n");
        EXPECT_EQ(d.substr(0, s.size()), s) << up;
        EXPECT_EQ(d.size() % 128, 0u);
        EXPECT_LT(d.size(), s.size() + 128) << up;
    }
}

// ─── Kermit-80 sendet Dateien an den PC-Server (SEND) ─────────────────────────────────────────

TEST(Kermit80A5120, SendGroessenUndInhalt) {
    stumm();
    KERMIT80_COM_ODER_SKIP("a5120");
    std::string alle(4096, '\0');
    for (size_t i = 0; i < alle.size(); ++i) alle[i] = char(i & 0xFF);
    // CP/M-Dateien sind Vielfache von 128 B; der Rest einer Textdatei ist mit ^Z gefuellt.
    std::string text;
    for (int i = 1; i <= 40; ++i) {
        char z[80];
        std::snprintf(z, sizeof z, "Zeile %02d: Kermit-80 sendet Text mit CR LF.\r\n", i);
        text += z;
    }
    const std::map<std::string, std::string> cpm = {
        {"S1.BIN", recordAuffuellen(std::string(1, char(0xE5)), 0)},
        {"S128.BIN", zufall(128, 1128)},
        {"S256.BIN", zufall(256, 1256)},
        {"ALLE256.BIN", alle},
        {"S100K.BIN", zufall(102400, 1100)},
        {"TEXT.TXT", recordAuffuellen(text, 0x1A)},
    };
    Lauf l(com, cpm);
    ASSERT_TRUE(l.bereit({"set file-mode binary"}));
    for (const char* n : {"s1.bin", "s128.bin", "s256.bin", "alle256.bin", "s100k.bin"}) {
        std::string up = n;
        for (auto& c : up) c = char(std::toupper(static_cast<unsigned char>(c)));
        ASSERT_TRUE(l.messen(std::string("send ") + n, cpm.at(up).size())) << l.a.bild();
    }
    ASSERT_TRUE(l.b.befehl("set file-mode ascii"));
    ASSERT_TRUE(l.messen("send text.txt", text.size())) << l.a.bild();
    ASSERT_TRUE(l.b.befehl("finish"));
    for (int i = 0; i < 100 && !l.srv.fertig; ++i) l.a.lauf(400'000);
    EXPECT_TRUE(l.srv.fertig);
    EXPECT_TRUE(l.srv.ergebnis.ok()) << l.srv.ergebnis.text;
    for (const auto& [n, s] : cpm) {
        std::string klein = n;
        for (auto& c : klein) c = char(std::tolower(static_cast<unsigned char>(c)));
        const fs::path p = l.srv.frei.p / klein;
        if (!fs::exists(p)) { ADD_FAILURE() << "Server hat " << klein << " nicht bekommen"; continue; }
        const std::string d = lies(p);
        std::fprintf(stderr, "  %-12s CP/M %6zu B  Server %6zu B\n", n.c_str(), s.size(), d.size());
        if (n == "TEXT.TXT") EXPECT_EQ(d, text) << "ASCII-Modus: Ende am ersten ^Z, CR LF bleibt";
        else EXPECT_EQ(d, s) << n;
    }
    for (const auto& e : l.srv.ereignisse) std::fprintf(stderr, "    [PC-Server] %s\n", e.c_str());
}

// ─── 8-Bit-Durchgang (Paritaet, QBIN) und Blockpruefung 1/2/3 ─────────────────────────────────

TEST(Kermit80A5120, ParitaetUndBlockpruefung) {
    stumm();
    KERMIT80_COM_ODER_SKIP("a5120");
    std::string alle(4096, '\0');
    for (size_t i = 0; i < alle.size(); ++i) alle[i] = char(i & 0xFF);
    Lauf l(com, {{"ALLE256.BIN", alle}});
    ASSERT_TRUE(l.bereit({"set file-mode binary"}));
    struct Fall { const char* parity; const char* check; };
    const Fall faelle[] = {{"none", "1-character-checksum"}, {"none", "2-character-checksum"}, {"none", "3-character-crc-ccitt"},
                           {"even", "1-character-checksum"}, {"mark", "3-character-crc-ccitt"}, {"space", "3-character-crc-ccitt"},
                           {"odd", "2-character-checksum"}};
    int n = 0;
    for (const Fall& f : faelle) {
        SCOPED_TRACE(std::string("parity ") + f.parity + ", " + f.check);
        ASSERT_TRUE(l.b.befehl(std::string("set parity ") + f.parity));
        ASSERT_TRUE(l.b.befehl(std::string("set block-check-type ") + f.check));
        const std::string ziel = "p" + std::to_string(n++);
        ASSERT_TRUE(l.messen("send alle256.bin " + ziel, alle.size())) << l.a.bild();
        const fs::path p = l.srv.frei.p / ziel;
        ASSERT_TRUE(fs::exists(p)) << "nicht angekommen\n" << l.a.bild();
        EXPECT_EQ(lies(p), alle);
        std::string v;
        {
            std::lock_guard<std::mutex> lk(l.srv.em);
            for (const auto& e : l.srv.ereignisse) if (e.rfind("Ausgehandelt", 0) == 0) v = e;
        }
        std::fprintf(stderr, "  [parity %-5s %-24s] %s\n", f.parity, f.check, v.c_str());
    }
    ASSERT_TRUE(l.b.befehl("finish"));
}


// ─── Die uebrigen Rechner: K8915, PC 1715, PRG 710/710-1 ──────────────────────────────────────
//
// Kermit-80 wird als SERTEST.COM auf eine Systemdiskette der Fixtures gelegt (die Hilfen aus
// sertest_hilfen.h installieren immer diesen Namen) und mit `sertest` gestartet.  Gepruft wird, was an
// der Ueberlagerung haengt: richtiger Datenport, Zeitgeber/SIO-Anfangsfolge (9600 Bd), Statusbits.
// Die Datei geht als `send` an den PC-Server und muss dort bytegleich ankommen.

namespace {

template <class S>
struct FremdLauf {
    S s;
    PcServer srv;
    std::string com;
    std::string kanalName = "V.24";        ///< Teil des Hub-Kanalnamens
    std::string start = "sertest";         ///< Kermit-80 liegt in den Tests als SERTEST.COM auf der Diskette
    std::string datei = "sertest.com";
    template <class... A>
    FremdLauf(std::string c, A&&... a) : s(std::forward<A>(a)...), com(std::move(c)) {}

    static std::string letzte(const std::string& bild) { return K80A5120::letzte(bild); }
    bool imKermit() {
        const std::string l = letzte(s.bild());
        return l.rfind("Kermit-80", 0) == 0 && l.back() == '>';
    }
    bool bisKermit(long long frist) {
        for (long long t = 0; t < frist; t += 20'000) {
            s.schritt();
            if (imKermit()) return true;
        }
        return false;
    }
    bool befehl(const std::string& cmd, long long frist = 3'000'000'000LL) {
        s.tippe(cmd + "\r");
        for (long long t = 0; t < 4'000'000 && imKermit(); t += 20'000) s.schritt();
        return bisKermit(frist);
    }
    std::string kanalClient(int port) {
        SerialHub* hub = s.maschine().serialHub();
        std::string namen;
        for (int i = 0; i < hub->anzahl(); ++i) namen += " [" + hub->info(i).name + "]";
        for (int i = 0; i < hub->anzahl(); ++i) {
            if (hub->info(i).name.find(kanalName) == std::string::npos) continue;
            SerialKonfig k = hub->konfig(i);
            k.betriebsart = Betriebsart::Telnet;
            k.rolle = Rolle::Client;
            k.host = "127.0.0.1";
            k.port = uint16_t(port);
            k.loop = false;
            if (!hub->konfigurieren(i, k) || !hub->start(i)) return "Hub-Kanal starten: " + hub->status(i).meldung;
            for (int n = 0; n < 400 && hub->status(i).zustand != Zustand::Verbunden; ++n)
                for (int b = 0; b < 10; ++b) s.schritt();
            return hub->status(i).zustand == Zustand::Verbunden ? "" : "nicht verbunden: " + hub->status(i).meldung;
        }
        return "kein V.24-Kanal; vorhanden:" + namen;
    }
    ::testing::AssertionResult pruefe() {
        if (!s.fehler().empty()) return ::testing::AssertionFailure() << s.fehler();
        if (!s.kaltstart()) return ::testing::AssertionFailure() << "kein Prompt\n" << s.bild();
        std::string f;
        if (!srv.start(&f)) return ::testing::AssertionFailure() << f;
        if (const std::string e = kanalClient(srv.port()); !e.empty()) return ::testing::AssertionFailure() << e;
        s.tippe(start + "\r");
        if (!bisKermit(600'000'000)) return ::testing::AssertionFailure() << "kein Kermit-80>\n" << s.bild();
        if (!befehl("set file-mode binary")) return ::testing::AssertionFailure() << "set\n" << s.bild();
        if (!befehl("send " + datei + " p0")) return ::testing::AssertionFailure() << "send\n" << s.bild();
        const fs::path p = srv.frei.p / "p0";
        if (!fs::exists(p)) return ::testing::AssertionFailure() << "nicht angekommen\n" << s.bild();
        if (lies(p) != lies(com)) return ::testing::AssertionFailure() << "Inhalt weicht ab (" << lies(p).size() << " B)";
        return ::testing::AssertionSuccess();
    }
};

}  // namespace

#define KERMIT80_RECHNER_COM(rechner) \
    std::string grund; \
    const std::string com = kermitCom(rechner, &grund); \
    if (com.empty()) GTEST_SKIP() << grund

TEST(Kermit80Rechner, K8915) {
    stumm();
    KERMIT80_RECHNER_COM("k8915");
    FremdLauf<sertest::SertestK8915> l(com, sertest::kDisketteK8915, com);
    EXPECT_TRUE(l.pruefe());
}

TEST(Kermit80Rechner, Pc1715) {
    stumm();
    KERMIT80_RECHNER_COM("pc1715");
    FremdLauf<sertest::SertestPc1715> l(com, com);
    EXPECT_TRUE(l.pruefe());
}

TEST(Kermit80Rechner, Pc1715W) {
    stumm();
    KERMIT80_RECHNER_COM("pc1715w");
    FremdLauf<sertest::SertestPc1715W> l(com, com);
    EXPECT_TRUE(l.pruefe());
}

TEST(Kermit80Rechner, Prg710) {
    stumm();
    KERMIT80_RECHNER_COM("prg710");
    FremdLauf<sertest::SertestPrg> l(com, sertest::SertestPrg::V::Prg710, com);
    EXPECT_TRUE(l.pruefe());
}

TEST(Kermit80Rechner, Prg710_1) {
    stumm();
    KERMIT80_RECHNER_COM("prg710");
    FremdLauf<sertest::SertestPrg> l(com, sertest::SertestPrg::V::Prg710_1, com);
    EXPECT_TRUE(l.pruefe());
}

// ─── P8000 (8-Bit-Teil) mit OS/M ──────────────────────────────────────────────────────────────
//
// Die ausgelieferte OS/M-Systemdiskette (disks/p8000_osm30/, tools/osm_p8000/) traegt KERMIT.COM schon;
// sie wird als Temp-Kopie gemountet.  Kermit-80 geht an tty0 (SIO0-A); tty1 ist das Terminal.

namespace {

class K80P8000 {
public:
    K80P8000() : pfad_(k1520test::tempPath("k1520_k80_osm30.hfe")) {
        std::error_code ec;
        fs::copy_file(K1520_OSM_P8000_DISK, pfad_, fs::copy_options::overwrite_existing, ec);
        if (ec) fehler_ = "Diskette kopieren: " + ec.message();
        if (fehler_.empty() && !m_.mountDisk(0, pfad_, m_.defaultFormatName(0), false)) fehler_ = "mountDisk: " + m_.lastError();
    }
    ~K80P8000() { std::error_code ec; fs::remove(pfad_, ec); }
    const std::string& fehler() const { return fehler_; }
    bool kaltstart() {
        if (!fehler_.empty()) return false;
        m_.powerOn();
        namespace p = k1520test::p8000;
        if (!p::laufeBisText(m_, "Press RETURN", 200'000'000)) return false;
        p::tippe(m_, "\r");
        if (!p::laufeBisPrompt(m_, ">", 4'000'000)) return false;
        p::tippe(m_, "\r");
        return p::laufeBisText(m_, "OS/M", 400'000'000) && p::laufeBisPrompt(m_, "A:", 200'000'000);
    }
    void tippe(const std::string& t) {
        for (char c : t) k1520test::p8000::tippe(m_, std::string(1, c));
    }
    std::string bild() { return k1520test::p8000::bild(m_); }
    long long schritt() { return m_.run(int(k1520test::p8000::kBatch)); }
    P8000Machine& maschine() { return m_; }

private:
    std::string  pfad_;
    P8000Machine m_;
    std::string  fehler_;
};

}  // namespace

TEST(Kermit80Rechner, P8000MitOsm) {
    stumm();
    KERMIT80_RECHNER_COM("p8000");
    FremdLauf<K80P8000> l(com);
    l.start = "kermit";
    l.datei = "kermit.com";
    l.kanalName = "tty0";
    EXPECT_TRUE(l.pruefe());
}

// ─── Die eingecheckten Ueberlagerungen gehoeren zu den Quellen ────────────────────────────────

TEST(Kermit80Bau, EingechecktesComIstDasErgebnisVonBauPy) {
    const char* cpa = std::getenv("CPA_TOOLS");
    const fs::path tools = cpa ? fs::path(cpa) : fs::path(std::getenv("HOME") ? std::getenv("HOME") : "") /
                                                     "projects/CPA_Workbench/tools";
    if (!fs::exists(tools / "m80.com")) GTEST_SKIP() << "CP/M-Werkzeugkette fehlt (CPA_TOOLS): " << tools;
    for (const char* r : {"a5120", "k8915", "pc1715", "pc1715w", "prg710", "p8000"}) {
        const std::string out = k1520test::tempPath(std::string("k1520_kermit80_bau_") + r + ".com");
        const std::string cmd = std::string("python3 \"") + K1520_KERMIT80_DIR + "/bau.py\" " + r + " --out \"" + out +
                                "\" >/dev/null 2>&1";
        ASSERT_EQ(std::system(cmd.c_str()), 0) << cmd;
        EXPECT_EQ(lies(out), lies(std::string(K1520_KERMIT80_DIR) + "/kermit_" + r + ".com"))
            << r << ": neu bauen mit python3 tools/kermit80/bau.py " << r << " --out tools/kermit80/kermit_" << r << ".com";
        fs::remove(out);
    }
}


// ─── Kermit-80 meldet sich im Terminalmodus an UNSEREM Server mit Anmeldezwang an (K12) ───────
// Gegenstueck zu Kermit80Wega: dort spricht getty + login der WEGA, hier `Sitzung::serve` mit
// `login_pflicht`.  Ablauf wie am Geraet: CONNECT, RETURN bis `login:`, Name, `Password:`, Kennwort, dann
// Strg-\ `c` zurueck zum Kermit-80-Prompt, REMOTE DIRECTORY, GET, SEND, FINISH.

TEST(Kermit80Pc, MeldetSichAmServerMitAnmeldezwangAn) {
    stumm();
    KERMIT80_COM_ODER_SKIP("a5120");
    const std::string sende = recordAuffuellen("Sendezeile von Kermit-80 an den PC-Server mit Anmeldung.\r\n", 0x1A);
    Lauf l(com, {{"SENDTEST.TXT", sende}});
    l.srv.einst.login_pflicht = true;
    l.srv.einst.login_name = "olli";
    l.srv.einst.login_kennwort = "geheim";
    const std::string holtext = "Datei auf dem PC-Server, 7 Bit, mit Anmeldung geholt.\r\n";
    schreibe(l.srv.frei.p / "holen.txt", holtext);
    ASSERT_TRUE(l.bereit({"set file-mode binary"}));

    const double scheibe = double(k1520test::kSmallBatch) / 2.4576e6;
    auto bisText = [&](const std::string& t, double sek) {
        for (double n = 0; n < sek; n += scheibe) {
            l.a.schritt();
            if (l.a.bild().find(t) != std::string::npos) return true;
        }
        return false;
    };
    auto bisPrompt = [&](double sek, bool aenderung = false, const std::string& vor = "") {
        return k80h::wartePrompt([&] { l.a.schritt(); }, [&] { return l.a.bild(); }, scheibe, sek, aenderung, vor);
    };
    auto befehl = [&](const std::string& cmd, double sek = 600) {
        k1520test::wipeVram(l.a.m());
        const std::string vor = l.a.bild();
        l.a.tippe(cmd + "\r");
        const bool ok = bisPrompt(sek, true, vor);
        if (l.b.laut) std::fprintf(stderr, "--- Kermit-80> %s\n%s", cmd.c_str(), l.a.bild().c_str());
        return ok;
    };
    auto anmelden = [&](const std::string& name, const std::string& kennwort) {
        k1520test::wipeVram(l.a.m());
        l.a.tippe(name + "\r");
        if (!bisText("Password:", 30)) return false;
        l.a.tippe(kennwort + "\r");
        return true;
    };

    // ── CONNECT: der Server hat sein erstes `login:` schon gesendet (vor dem CONNECT, ungelesen); ein RETURN ruft es ──
    l.a.tippe("connect\r");
    l.a.lauf(2 * 2'457'600);
    l.a.tippe("\r");
    ASSERT_TRUE(bisText("login:", 60)) << l.a.bild();

    // ── falsches Kennwort: Login incorrect, danach wieder login: ──
    ASSERT_TRUE(anmelden("olli", "falsch")) << l.a.bild();
    ASSERT_TRUE(bisText("Login incorrect", 60)) << l.a.bild();
    l.a.lauf(2'457'600);
    EXPECT_NE(l.a.bild().find("login:"), std::string::npos) << "kein erneutes login: nach Login incorrect\n" << l.a.bild();
    // das Kennwort wurde nicht zurueckgeechot
    EXPECT_EQ(l.a.bild().find("falsch"), std::string::npos) << l.a.bild();

    // ── richtig anmelden ──
    ASSERT_TRUE(anmelden("olli", "geheim")) << l.a.bild();
    ASSERT_TRUE(bisText("server starting", 120)) << l.a.bild();
    EXPECT_EQ(l.a.bild().find("geheim"), std::string::npos) << "Kennwort im Bild\n" << l.a.bild();
    l.a.lauf(2'457'600);

    // ── zurueck zu Kermit-80 mit Strg-\ c (nicht Strg-]) ──
    k1520test::wipeVram(l.a.m());
    k1520test::typeCtrl(l.a.m(), '\\');
    l.a.lauf(2'457'600);
    k1520test::typeKey(l.a.m(), 'c');
    ASSERT_TRUE(bisPrompt(120)) << "kein Kermit-80> nach Strg-\\ c\n" << l.a.bild();

    ASSERT_TRUE(befehl("remote directory")) << l.a.bild();
    EXPECT_NE(l.a.bild().find("holen.txt"), std::string::npos) << l.a.bild();
    ASSERT_TRUE(befehl("get holen.txt", 1800)) << l.a.bild();
    ASSERT_TRUE(befehl("send sendtest.txt", 1800)) << l.a.bild();
    ASSERT_TRUE(befehl("finish")) << l.a.bild();

    // gehoert der Server nach FINISH wieder der Anmeldung?  Ein neues CONNECT zeigt `login:` (kein Protokollende).
    EXPECT_FALSE(l.srv.fertig) << "mit Anmeldezwang endet serve nach FINISH nicht";
    k1520test::wipeVram(l.a.m());
    l.a.tippe("connect\r");
    l.a.lauf(2 * 2'457'600);
    l.a.tippe("\r");
    EXPECT_TRUE(bisText("login:", 60)) << l.a.bild();

    // ── Inhalte: geholte Datei auf der Diskette, gesendete im Server-Ordner ──
    const auto auf = l.a.diskDateien();
    ASSERT_TRUE(auf.count("HOLEN.TXT")) << "HOLEN.TXT fehlt auf der Diskette";
    EXPECT_EQ(auf.at("HOLEN.TXT").substr(0, holtext.size()), holtext);
    const fs::path ges = l.srv.frei.p / "sendtest.txt";
    ASSERT_TRUE(fs::exists(ges)) << "Server hat sendtest.txt nicht bekommen";
    EXPECT_EQ(lies(ges).substr(0, sende.size()), sende);
    bool angemeldet = false, abgelehnt = false, beendet = false;
    {
        std::lock_guard<std::mutex> lk(l.srv.em);
        for (const auto& e : l.srv.ereignisse) {
            angemeldet |= e == "Angemeldet: olli";
            abgelehnt |= e.rfind("Anmeldung abgelehnt", 0) == 0;
            beendet |= e.find("Server beendet (FINISH)") != std::string::npos;
        }
    }
    EXPECT_TRUE(angemeldet);
    EXPECT_TRUE(abgelehnt);
    EXPECT_TRUE(beendet);
    l.a.m().serialHub()->stopAlle();
}
