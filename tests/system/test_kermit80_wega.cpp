/**
 * @file test_kermit80_wega.cpp
 * @brief Kermit-80 (A5120 mit CP/A) als CLIENT gegen den ECHTEN C-Kermit 4.2(030) als Server unter
 *        WEGA 3.1 auf dem P8000 — zwei gekoppelte Emulatoren (Entwurf 36 §6.8).
 *
 * Aufbau: Der P8000 startet aus dem eingefrorenen Zustand von `test_p8000_wega_kermit_austausch.cpp`
 * (`wega_kermit_stand.h`, getty auf tty0 wartet).  tty0 ist ein Hub-Kanal als Telnet-SERVER, der V.24-Kanal
 * des A5120 verbindet sich als Telnet-CLIENT.  Beide Maschinen laufen in EINEM Faden in Scheiben gleicher
 * Maschinenzeit (wie `test_sertest_kopplung.cpp`), gedrosselt auf hoechstens ein Vielfaches der Echtzeit.
 *
 * Ablauf wie am Geraet: `kermit` am CP/A-Prompt, `connect`, am getty als `kermit` anmelden (danach laeuft der
 * Kermit-Server der WEGA), mit Ctrl-\ `c` zurueck zum Kermit-80-Prompt, dann `remote directory`, `get`,
 * `send`, `finish`.  Kermit-80 hat keinen Anmeldeablauf; die Anmeldung tippt der Test im Terminalmodus.
 *
 * Ohne Startdiskette/Platte der WEGA uebersprungen.  Diagnose: `K1520_KERMIT_LAUT=1` gibt jedes Bild aus.
 */
#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <thread>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/machines/a5120/a5120.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"
#include "tests/system/kermit80_hilfen.h"
#include "tests/system/wega_kermit_stand.h"

#ifndef K1520_KERMIT80_DIR
#error "K1520_KERMIT80_DIR fehlt (tools/kermit80/)"
#endif

namespace {

using Uhr = std::chrono::steady_clock;

/// KERMIT.COM bzw. eine Testdatei mit dem DiskTool-Kern auf die Diskette schreiben; leer bei Erfolg.
std::string aufspielen(const std::string& pfad, const std::string& quelle, const std::string& name) {
    std::string f;
    static const FormatCatalog formate = FormatCatalog::loadDefault(&f);
    static const FsCatalog fsk = FsCatalog::loadDefault(formate, &f);
    std::string err;
    auto v = DiskVolume::open(pfad, "", formate, fsk, err, /*read_only=*/false);
    if (!v) return "DiskVolume::open: " + err;
    v->setBackup(false);
    TransferOptions o;
    o.overwrite = true;
    if (!v->insert(quelle, FileRef::parse(name), o)) return "insert: " + v->lastError();
    if (!v->flush()) return "flush: " + v->lastError();
    return {};
}

using k80h::letzteZeile;

/// Der A5120 mit CP/A, KERMIT.COM und einer Sendedatei auf A:.
class A5120Seite {
public:
    static constexpr const char* kFixture = "cpa_cpa780_k5601_noclock.img";
    A5120Seite(const std::string& com, const std::map<std::string, std::string>& dateien)
        : disk_(kFixture, std::string("kermit80_wega_") + kFixture) {
        fehler_ = aufspielen(disk_.path(), com, "KERMIT.COM");
        for (const auto& [name, pfad] : dateien)
            if (fehler_.empty()) fehler_ = aufspielen(disk_.path(), pfad, name);
        if (fehler_.empty() && !m_.mountDisk(0, disk_.path(), "cpa780", false)) fehler_ = "mountDisk: " + m_.lastError();
    }
    const std::string& fehler() const { return fehler_; }
    A5120Machine& m() { return m_; }
    std::string bild() { return k1520test::vramLines(m_); }
    long long lauf(long long takte) {
        long long n = 0;
        while (n < takte) n += m_.run(5'000);
        return n;
    }
    void tippe(const std::string& s) {
        for (char c : s) {
            if (c == '\r') k1520test::typeKey(m_, k1520test::QK_RETURN);
            else k1520test::typeKey(m_, static_cast<uint8_t>(c));
        }
    }
    std::string kanalClient(int port) {
        SerialHub* hub = m_.serialHub();
        std::string namen;
        for (int i = 0; i < hub->anzahl(); ++i) namen += " [" + hub->info(i).name + "]";
        for (int i = 0; i < hub->anzahl(); ++i) {
            if (hub->info(i).name.find("V.24") == std::string::npos) continue;
            SerialKonfig k = hub->konfig(i);
            k.betriebsart = Betriebsart::Telnet;
            k.rolle = Rolle::Client;
            k.host = "127.0.0.1";
            k.port = uint16_t(port);
            k.loop = false;
            if (!hub->konfigurieren(i, k) || !hub->start(i)) return "Hub-Kanal starten: " + hub->status(i).meldung;
            return "";
        }
        return "kein V.24-Kanal; vorhanden:" + namen;
    }
    bool verbunden() {
        SerialHub* hub = m_.serialHub();
        for (int i = 0; i < hub->anzahl(); ++i)
            if (hub->info(i).name.find("V.24") != std::string::npos) return hub->status(i).zustand == Zustand::Verbunden;
        return false;
    }
    /// Dateien der Diskette nach flushDisks: Name -> Inhalt.
    std::map<std::string, std::string> dateien() {
        std::map<std::string, std::string> r;
        m_.flushDisks();
        std::string f;
        static const FormatCatalog formate = FormatCatalog::loadDefault(&f);
        static const FsCatalog fsk = FsCatalog::loadDefault(formate, &f);
        std::string err;
        auto v = DiskVolume::open(disk_.path(), "", formate, fsk, err, /*read_only=*/true);
        if (!v) return r;
        for (const FileEntry& e : v->list()) {
            const fs::path tmp = fs::path(k1520test::tempPath("k1520_k80w_aus"));
            if (!v->extract(FileRef::parse(e.name), tmp.string(), TransferOptions{})) continue;
            r[e.name] = lies(tmp);
            fs::remove(tmp);
        }
        return r;
    }

private:
    k1520test::TempDisk disk_;
    A5120Machine m_;
    std::string fehler_;
};

/// Beide Maschinen in Scheiben gleicher Maschinenzeit, gedrosselt (Hub und Netz laufen in Uhrzeit).
struct Kopplung {
    Anlage& w;        ///< P8000 / WEGA (4 MHz)
    A5120Seite& a;    ///< A5120 (2,4576 MHz)
    static constexpr double kFaktor = 8.0;             ///< hoechstens 8x Echtzeit
    static constexpr long long kScheibeW = 20'000;     ///< 5 ms
    static constexpr long long kScheibeA = 12'288;     ///< 5 ms
    long long takteA = 0;
    bool laut = std::getenv("K1520_KERMIT_LAUT") != nullptr;
    Uhr::time_point t0 = Uhr::now();
    double nW = 0;

    void scheibe() {
        w.laufe(kScheibeW);
        nW += double(kScheibeW);
        takteA += a.lauf(kScheibeA);
        const auto soll = std::chrono::duration<double>(nW / 4e6 / kFaktor);
        const auto ist = Uhr::now() - t0;
        if (ist + std::chrono::milliseconds(2) < soll) std::this_thread::sleep_for(soll - ist);
    }
    void laufe(double sekunden) {
        for (double n = 0; n < sekunden * 4e6; n += double(kScheibeW)) scheibe();
    }
    bool bis(const std::function<bool()>& fertig, double max_sekunden) {
        for (double n = 0; n < max_sekunden * 4e6; n += double(kScheibeW)) {
            if (fertig()) return true;
            scheibe();
        }
        return fertig();
    }
    /// Siehe kermit80_hilfen.h: nackte Eingabeaufforderung irgendwo im Bild; Warten auf eine ruhige.
    bool nackterPrompt() { return k80h::nackterPrompt(a.bild()); }
    bool wartePrompt(double max_sekunden, bool aenderung = true, std::string start = "", double ruhe = 1.5) {
        return k80h::wartePrompt([&] { scheibe(); }, [&] { return a.bild(); }, double(kScheibeW) / 4e6, max_sekunden,
                                 aenderung, std::move(start), ruhe);
    }
    bool bisKermit(double sek) { return wartePrompt(sek, false); }
    bool bisText(const std::string& t, double sek) {
        return bis([&] { return a.bild().find(t) != std::string::npos; }, sek);
    }
    /// Befehl am Kermit-80-Prompt: tippen, warten bis wieder eine ruhige Eingabeaufforderung im Bild steht.
    bool befehl(const std::string& cmd, double sek = 600) {
        k1520test::wipeVram(a.m());          // altes Bild weg: die neue Eingabeaufforderung steht dann ohne Reste in ihrer Zeile
        const std::string vor = a.bild();   // Tippen laesst die Maschine laufen: das Echo steht danach schon im Bild
        a.tippe(cmd + "\r");
        const bool ok = wartePrompt(sek, true, vor);
        if (laut) std::fprintf(stderr, "--- Kermit-80> %s\n%s", cmd.c_str(), a.bild().c_str());
        return ok;
    }
};

}  // namespace

TEST(Kermit80Wega, ClientGegenC_Kermit42Server) {
    stumm();
    const std::string com = std::string(K1520_KERMIT80_DIR) + "/kermit_a5120.com";
    if (!fs::exists(com)) GTEST_SKIP() << "fehlt: " << com;

    // Sendedatei: reiner 7-Bit-Text (tty0 loescht Bit 7 auf dem Weg PC -> WEGA; Kermit-80 bietet kein 8-Bit-Praefix an)
    std::string sende;
    for (int i = 1; i <= 30; ++i) {
        char z[80];
        std::snprintf(z, sizeof z, "Sendezeile %02d von Kermit-80 an C-Kermit 4.2.\r\n", i);
        sende += z;
    }
    sende += '\x1a';
    while (sende.size() % 128) sende += '\x1a';
    const std::string sdatei = k1520test::tempPath("k1520_k80w_sende.txt");
    schreibe(sdatei, sende);
    // Binaerdatei zum Senden: jeder Bytewert 16x (4096 B = 32 Records, kein Auffuellen)
    std::string bin(4096, '\0');
    for (size_t i = 0; i < bin.size(); ++i) bin[i] = char(i & 0xFF);
    const std::string bdatei = k1520test::tempPath("k1520_k80w_sende.bin");
    schreibe(bdatei, bin);

    Anlage w;
    std::string grund;
    if (!standLaden(w, &grund)) {
        if (HasFatalFailure()) return;
        GTEST_SKIP() << grund;
    }
    const int port = w.kanal("tty0", Rolle::Server);
    ASSERT_GT(port, 0);

    A5120Seite a(com, {{"SENDTEST.TXT", sdatei}, {"SENDBIN.BIN", bdatei}});
    ASSERT_EQ(a.fehler(), "");
    Kopplung k{w, a};
    a.m().powerOn();
    ASSERT_TRUE(k.bis([&] { return letzteZeile(a.bild()) == "A>"; }, 120)) << a.bild();
    ASSERT_EQ(a.kanalClient(port), "");
    ASSERT_TRUE(k.bis([&] { return a.verbunden(); }, 60)) << "A5120 nicht mit tty0 verbunden";

    a.tippe("kermit\r");
    ASSERT_TRUE(k.bisKermit(120)) << a.bild();
    ASSERT_TRUE(k.befehl("set file-mode binary")) << a.bild();

    // ── Anmeldung im Terminalmodus: getty meldet sich auf einen Zeilenumbruch ──
    a.tippe("connect\r");
    k.laufe(2);
    a.tippe("\r");
    ASSERT_TRUE(k.bisText("login:", 120)) << a.bild();
    a.tippe("kermit\r");
    ASSERT_TRUE(k.bisText("Password:", 60)) << a.bild();
    a.tippe("kermit\r");
    k.laufe(60);   // Anmeldung, .profile, exec kermit, .kermrc -> server
    if (k.laut) std::fprintf(stderr, "--- nach der Anmeldung\n%s", a.bild().c_str());
    k1520test::wipeVram(a.m());
    k1520test::typeCtrl(a.m(), '\\');   // Fluchtzeichen dieser Fassung: Ctrl-\ ("Type Control-\C to return")
    k.laufe(1);
    k1520test::typeKey(a.m(), 'c');
    ASSERT_TRUE(k.bisKermit(120)) << "kein Kermit-80> nach Ctrl-\\ c\n" << a.bild();

    // ── remote directory (G D): ls -l der WEGA ──
    ASSERT_TRUE(k.befehl("remote directory")) << a.bild();
    const std::string verz = a.bild();
    std::fprintf(stderr, "--- remote directory\n%s", verz.c_str());
    EXPECT_NE(verz.find("text.txt"), std::string::npos);

    // ── get: Datei von der WEGA holen ──
    ASSERT_TRUE(k.befehl("get text.txt", 1800)) << a.bild();
    // ── send: Datei zur WEGA ──
    ASSERT_TRUE(k.befehl("send sendtest.txt", 1800)) << a.bild();
    ASSERT_TRUE(k.befehl("remote directory")) << a.bild();
    EXPECT_NE(a.bild().find("sendtest.txt"), std::string::npos) << a.bild();

    // ── Binaerdateien ueber die 7-Bit-Leitung: Paritaet gesetzt => Kermit-80 versucht das 8-Bit-Praefix ──
    // (tty0 loescht Bit 7; ohne Praefix kaeme jedes Byte >= 80H verstuemmelt an.  C-Kermit 4.2 schaltet das
    // Praefix nur ein, wenn die Gegenseite ein Zeichen nennt.)
    ASSERT_TRUE(k.befehl("set parity space")) << a.bild();
    for (const char* n : {"alle256", "b94", "b95"}) ASSERT_TRUE(k.befehl(std::string("get ") + n, 1800)) << n << "\n" << a.bild();
    // Hinweg und zurueck: senden, lokal loeschen, wieder holen — nur so ist der Inhalt auf der WEGA-Seite belegt
    ASSERT_TRUE(k.befehl("send sendbin.bin", 1800)) << a.bild();
    ASSERT_TRUE(k.befehl("erase sendbin.bin")) << a.bild();
    ASSERT_TRUE(k.befehl("get sendbin.bin", 1800)) << a.bild();
    ASSERT_TRUE(k.befehl("finish")) << a.bild();

    const auto auf = a.dateien();
    ASSERT_TRUE(auf.count("TEXT.TXT")) << "TEXT.TXT fehlt auf der Diskette";
    const std::string soll = testdateien().at("text.txt");
    const std::string& ist = auf.at("TEXT.TXT");
    std::fprintf(stderr, "  TEXT.TXT: WEGA %zu B, CP/M %zu B\n", soll.size(), ist.size());
    EXPECT_EQ(ist.substr(0, soll.size()), soll);
    EXPECT_EQ(ist.size() % 128, 0u);
    for (const char* n : {"ALLE256", "B94", "B95", "SENDBIN.BIN"}) {
        const std::string klein = [&] { std::string t = n; for (auto& c : t) c = char(std::tolower((unsigned char)c)); return t; }();
        const std::string erw = n == std::string("SENDBIN.BIN") ? bin : testdateien().at(klein);
        ASSERT_TRUE(auf.count(n)) << n << " fehlt auf der Diskette";
        std::fprintf(stderr, "  %-12s erwartet %zu B, CP/M %zu B\n", n, erw.size(), auf.at(n).size());
        EXPECT_EQ(auf.at(n).substr(0, erw.size()), erw) << n << " (Bytes mit Bit 7?)";
    }
    std::error_code ec;
    fs::remove(sdatei, ec);
    fs::remove(bdatei, ec);
    w.m().serialHub()->stopAlle();
    a.m().serialHub()->stopAlle();
}
