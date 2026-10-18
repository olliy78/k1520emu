/**
 * @file test_p8000_wega_kermit.cpp
 * @brief Kermit auf WEGA 3.1 (Entwurf 36 AP K1): C-Kermit 4.2(030) steht auf der ausgelieferten Platte
 *        `disks/p8000_wega31/p8000_wega31_platte.avr.img.gz` (/usr/bin/kermit, setuid root) und der
 *        Benutzer `kermit` (Kennwort `kermit`, Shell `/usr/bin/kermit`, Home `/usr/kermit` mit `.kermrc`:
 *        `server`, `exit`) macht aus einem getty-Anschluss einen Kermit-Server.
 *
 * Kaltstart wie am Geraet (Netz ein -> MON8 -> Koppelsoftware -> MON16 -> NMI -> boot -> WEGA), Konsole am
 * Originalterminal (tty1).  Geprueft wird
 *   (a) an der Konsole: `kermit` startet, `show parameters` antwortet (das Z8000-Binary laeuft unter WEGA 3.1),
 *   (b) auf tty0 (Hub-Kanal als Telnet-Server, Gegenstelle = ein rohes Socket + TelnetCodec, KEIN Terminal):
 *       Anmeldung `kermit`/`kermit`, danach laeuft der Server — ein von Hand gebautes Init-Paket
 *       (Typ `I`, Pruefsumme 1) wird mit einem `Y`-Paket beantwortet; `G F` (Finish) und `G L` (Logout)
 *       beenden die Sitzung, getty meldet sich wieder mit `login:`.
 * Der Paketsender hier ist absichtlich minimal (EIN Paket je Schritt, kein Protokoll): die Bibliothek
 * entsteht in `kermit/` (AP K2).  Ablauf und gesehene Texte: doc/p8000/kermit_wega.md.
 *
 * Platte: `K1520_WEGA31_PLATTE` (roh oder .gz) uebersteuert die ausgelieferte (zum Pruefen einer noch
 * nicht eingecheckten Fassung).  Gearbeitet wird immer an einer Temp-Kopie.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "core/logger.h"
#include "core/machines/p8000/p8000.h"
#include "core/serial/hub.h"
#include "core/serial/net/socket.h"
#include "core/serial/telnet_codec.h"
#include "core/util/gzip_datei.h"
#include "tests/support/p8000_input.h"
#include "tests/support/p8000_wega.h"
#include "tests/support/temp_path.h"

using namespace k1520test::p8000;
using namespace k1520::serial;
namespace net = k1520::serial::net;

#ifndef P8000_WEGA31_DIR
#define P8000_WEGA31_DIR "disks/p8000_wega31"
#endif

namespace {

void stumm() { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR); }

std::string rtrim(std::string s) {
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

/// Entpackte Plattenkopie (Temp) aus der ausgelieferten oder per Umgebung genannten Datei.
bool plattenKopie(const std::string& ziel, std::string* quelle) {
    std::string q = P8000_WEGA31_DIR "/p8000_wega31_platte.avr.img.gz";
    if (const char* e = std::getenv("K1520_WEGA31_PLATTE"); e && *e) q = e;
    *quelle = q;
    std::vector<uint8_t> d;
    std::string f;
    if (!k1520::gzip::laden(q, d, nullptr, &f)) return false;
    std::ofstream o(ziel, std::ios::binary);
    o.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
    return bool(o);
}

/// Kermit-Paket (Grundformat): SOH, LEN, SEQ, TYPE, DATA, CHK (Pruefsumme 1), Abschluss CR.
std::string kermitPaket(int seq, char typ, const std::string& daten) {
    std::string p;
    p += char(32 + 3 + int(daten.size()));      // LEN = SEQ + TYPE + DATA + CHK
    p += char(32 + seq);
    p += typ;
    p += daten;
    int s = 0;
    for (unsigned char c : p) s += c;
    p += char(32 + ((s + ((s & 192) / 64)) & 63));
    return std::string(1, '\x01') + p + "\r";
}

/// Erstes gueltiges Paket vom Typ @p typ im Strom @p rx (Pruefsumme 1 geprueft); Daten in @p daten.
bool findePaket(const std::string& rx, char typ, std::string* daten, int* seq = nullptr) {
    for (size_t i = 0; i < rx.size(); ++i) {
        if (rx[i] != '\x01' || i + 2 >= rx.size()) continue;
        const int len = (unsigned char)rx[i + 1] - 32;
        if (len < 3 || i + 2 + size_t(len) > rx.size()) continue;
        const std::string body = rx.substr(i + 1, size_t(len) + 1);   // LEN SEQ TYPE DATA CHK
        int s = 0;
        for (size_t k = 0; k + 1 < body.size(); ++k) s += (unsigned char)body[k];
        if (char(32 + ((s + ((s & 192) / 64)) & 63)) != body.back()) continue;
        if (body[2] != typ) continue;
        if (daten) *daten = body.substr(3, body.size() - 4);
        if (seq) *seq = (unsigned char)body[1] - 32;
        return true;
    }
    return false;
}

std::string sichtbar(const std::string& s) {
    std::string o;
    char b[8];
    for (unsigned char c : s) {
        if (c == '\r') o += "\\r";
        else if (c == '\n') o += "\\n";
        else if (c >= 32 && c < 127) o += char(c);
        else { std::snprintf(b, sizeof b, "<%02X>", c); o += b; }
    }
    return o;
}

/// Die Anlage: Rechner mit Konsole am Originalterminal + ein Telnet-Client an einem Hub-Kanal.
struct Anlage {
    k1520test::p8000::WegaLauf l;
    net::Socket sock;
    ::serial::TelnetCodec tel{::serial::TelnetRolle::Client};
    std::string rx;     ///< Nutzdaten vom Hub-Kanal (Telnet dekodiert)

    P8000Machine& m() { return *l.m; }
    std::string zeile(int z) { return rtrim(m().konsole().text(z)); }
    std::string cursorZeile() { return zeile(m().konsole().zeile()); }
    std::string bild() {
        std::string s;
        for (int z = 0; z < 24; ++z) s += zeile(z) + "\n";
        return s;
    }

    /// Netz: Socket lesen, Telnet verarbeiten, Antworten senden.
    void pumpe() {
        if (!sock) return;
        uint8_t buf[512];
        for (;;) {
            const auto r = net::empfangen(sock.fd(), buf, sizeof buf);
            if (r.status != net::IoStatus::Ok || r.n == 0) break;
            tel.eingabe(buf, r.n);
        }
        const auto aus = tel.nimmAusgabe();
        if (!aus.empty()) net::senden(sock.fd(), aus.data(), aus.size());
        const auto nutz = tel.nimmNutzdaten();
        rx.append(nutz.begin(), nutz.end());
    }
    void schritt() {
        m().run(int(kBatch));
        pumpe();
    }
    void laufe(long long takte) {
        for (long long t = 0; t < takte; t += kBatch) schritt();
    }
    void melde(long long t) {
        if (t > 0 && t % 400'000'000 == 0) {
            std::string z = cursorZeile();
            while (!z.empty() && z.front() == ' ') z.erase(z.begin());
            std::fprintf(stderr, "  [%6.0f Mio. Takte] %s\n", double(m().totalCycles()) / 1e6, z.c_str());
        }
    }
    bool bisText(const std::string& nadel, long long grenze) {
        for (long long t = 0, seit = 0; t < grenze; t += kBatch, seit += kBatch) {
            schritt();
            if (seit >= 100'000) {
                seit = 0;
                if (bild().find(nadel) != std::string::npos) return true;
            }
            melde(t);
        }
        return bild().find(nadel) != std::string::npos;
    }
    int frage(const std::vector<std::string>& fragen, long long grenze) {
        auto passt = [&]() -> int {
            const std::string z = cursorZeile();
            for (size_t i = 0; i < fragen.size(); ++i)
                if (endetMit(z, rtrim(fragen[i]))) return int(i);
            return -1;
        };
        for (long long t = 0, seit = 0; t < grenze; t += kBatch, seit += kBatch) {
            schritt();
            if (seit >= 100'000) {
                seit = 0;
                if (int i = passt(); i >= 0) return i;
            }
            melde(t);
        }
        return passt();
    }
    void tippe(const std::string& text) {
        for (char c : text) m().keyPress((c == '\r' || c == '\n') ? 0x01000004u : uint8_t(c), false, false);
        for (long long t = 0; t < 400'000'000 && !m().originalTerminal()->tastenFertig(); t += kBatch) schritt();
    }
    void tippeZeile(const std::string& text) { tippe(text + "\r"); }

    static bool istPrompt(const std::string& z) {
        static const std::regex re(R"((^|\s)([#%][0-9]+|[$#%])$)");
        return std::regex_search(z, re);
    }
    bool bisPrompt(long long grenze) {
        laufe(400'000);
        for (long long t = 0, seit = 0; t < grenze; t += kBatch, seit += kBatch) {
            schritt();
            if (seit >= 100'000) {
                seit = 0;
                if (m().originalTerminal()->tastenFertig() && istPrompt(cursorZeile())) return true;
            }
            melde(t);
        }
        return m().originalTerminal()->tastenFertig() && istPrompt(cursorZeile());
    }
    ::testing::AssertionResult kommando(const std::string& cmd, long long grenze = 800'000'000) {
        tippeZeile(cmd);
        if (!bisPrompt(grenze)) return ::testing::AssertionFailure() << "'" << cmd << "' ohne Prompt\n" << bild();
        return ::testing::AssertionSuccess();
    }
    ::testing::AssertionResult anmeldenKonsole(const std::string& name, const std::string& pw) {
        if (frage({"login:"}, 40'000'000) < 0) {
            tippe("\r");
            if (frage({"login:"}, 400'000'000) < 0) return ::testing::AssertionFailure() << "kein login:\n" << bild();
        }
        laufe(200'000);
        tippeZeile(name);
        if (!pw.empty()) {
            if (frage({"Password:"}, 400'000'000) < 0) return ::testing::AssertionFailure() << "kein Password:\n" << bild();
            laufe(200'000);
            tippeZeile(pw);
        }
        for (long long t = 0; t < 2'000'000'000LL; t += 1'000'000) {
            laufe(1'000'000);
            if (m().originalTerminal()->tastenFertig() && istPrompt(cursorZeile())) return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << "kein Prompt nach der Anmeldung\n" << bild();
    }

    // ── Hub-Kanal als Telnet-Server, Gegenstelle ein Socket ───────────────────────────────
    bool kanalVerbinden(const std::string& name) {
        SerialHub* hub = m().serialHub();
        for (int i = 0; i < hub->anzahl(); ++i) {
            if (hub->info(i).name != name) continue;
            SerialKonfig k = hub->konfig(i);
            k.betriebsart = Betriebsart::Telnet;
            k.rolle = Rolle::Server;
            k.host = "127.0.0.1";
            k.port = 0;
            k.loop = false;
            if (!hub->konfigurieren(i, k) || !hub->start(i)) return false;
            const int port = hub->status(i).port_aktiv;
            net::Fehler f;
            sock = net::verbindenAlle(net::aufloesen("127.0.0.1", port, &f), 2000, &f);
            if (!sock) return false;
            for (int n = 0; n < 300 && hub->status(i).zustand != Zustand::Verbunden; ++n)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return hub->status(i).zustand == Zustand::Verbunden;
        }
        return false;
    }
    void sendeRoh(const std::string& s) {
        tel.sende(reinterpret_cast<const uint8_t*>(s.data()), s.size());
        const auto aus = tel.nimmAusgabe();
        net::senden(sock.fd(), aus.data(), aus.size());
    }
    /// Strom ab @p ab mit geloeschtem Bit 7: getty gibt die Anmeldezeile nach einer Sitzung MIT Paritaet aus
    /// (gerade Paritaet, `WEGA login:` kommt als `<D7><C5>GA<A0>lo<E7>i<EE>:`) — ein 7-Bit-Terminal sieht
    /// Klartext, ein Login-Skript muss ebenso maskieren (doc/p8000/kermit_wega.md).
    std::string rx7(size_t ab = 0) const {
        std::string o = rx.substr(ab);
        for (char& c : o) c = char(c & 0x7F);
        return o;
    }
    bool bisRx(const std::string& nadel, long long grenze, size_t ab = 0) {
        for (long long t = 0; t < grenze; t += kBatch) {
            if (rx7(ab).find(nadel) != std::string::npos) return true;
            schritt();
            melde(t);
        }
        return rx7(ab).find(nadel) != std::string::npos;
    }
    bool bisPaket(char typ, long long grenze, std::string* daten, size_t ab) {
        for (long long t = 0; t < grenze; t += kBatch) {
            if (findePaket(rx.substr(ab), typ, daten)) return true;
            schritt();
            melde(t);
        }
        return findePaket(rx.substr(ab), typ, daten);
    }
};

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
    // boot0.md startet WEGA selbst (Merkposten P22) — nichts tippen
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

}  // namespace

TEST(P8000WegaKermit, ServerHinterGetty) {
    stumm();
    std::string quelle;
    if (!k1520test::p8000::dateiDa(P8000_WEGA31_DIR "/p8000_wega31_udos_boot.dmk"))
        GTEST_SKIP() << "Startdiskette disks/p8000_wega31/p8000_wega31_udos_boot.dmk fehlt";
    Anlage a;
    a.l.platte = k1520test::tempPath("k1520_kermit_platte.img");
    a.l.start = k1520test::tempPath("k1520_kermit_start.dmk");
    if (!plattenKopie(a.l.platte, &quelle)) GTEST_SKIP() << "Platte nicht lesbar: " << quelle;
    ASSERT_TRUE(k1520test::p8000::kopiere(P8000_WEGA31_DIR "/p8000_wega31_udos_boot.dmk", a.l.start));
    P8000Machine::Config c = wegaConfig(a.l.platte, false);
    c.terminal = P8000Machine::Config::TerminalArt::Original;
    a.l.m = std::make_unique<P8000Machine>(c);
    P8000Machine& m = a.m();
    ASSERT_TRUE(m.mountDisk(0, a.l.start, m.defaultFormatName(0), false)) << m.lastError();
    const auto t0 = std::chrono::steady_clock::now();
    ASSERT_NO_FATAL_FAILURE(kaltstartBisLogin(a));
    std::fprintf(stderr, "  [login: an der Konsole nach %.1f s Maschinenzeit]\n", double(m.totalCycles()) / 4e6);
    ASSERT_TRUE(a.anmeldenKonsole("wega", "root"));
    std::fprintf(stderr, "  [Konsole angemeldet nach %.1f s Maschinenzeit]\n", double(m.totalCycles()) / 4e6);

    // ── (a) Konsole: das Binary laeuft ────────────────────────────────────────────────────
    ASSERT_TRUE(a.kommando("ls -l /usr/bin/kermit /usr/kermit /etc/passwd"));
    std::fprintf(stderr, "--- ls\n%s", a.bild().c_str());
    EXPECT_NE(a.bild().find("-rwsr-xr-x"), std::string::npos) << a.bild();   // setuid-Bit steht
    a.tippeZeile("kermit");
    ASSERT_GE(a.frage({"C-Kermit>"}, 400'000'000), 0) << a.bild();
    EXPECT_NE(a.bild().find("C-Kermit"), std::string::npos);
    std::fprintf(stderr, "--- kermit gestartet\n%s", a.bild().c_str());
    a.tippeZeile("show parameters");
    a.laufe(8'000'000);   // Prompt der vorigen Zeile steht noch in der Cursorzeile — Antwort abwarten
    ASSERT_GE(a.frage({"C-Kermit>"}, 400'000'000), 0) << a.bild();
    const std::string shown = a.bild();
    std::fprintf(stderr, "--- show parameters\n%s", shown.c_str());
    EXPECT_NE(shown.find("Transactions:"), std::string::npos) << shown;
    a.tippeZeile("exit");
    ASSERT_TRUE(a.bisPrompt(400'000'000)) << a.bild();

    // Handbuchseite (nur Befund, kein Waechter: man/nroff haengen vom Terminaltyp ab)
    a.tippeZeile("man kermit | head -8");
    a.bisPrompt(800'000'000);
    std::fprintf(stderr, "--- man kermit\n%s", a.bild().c_str());

    // ── (b) tty0 als Telnet-Server, Gegenstelle ohne Terminal ─────────────────────────────
    ASSERT_TRUE(a.kanalVerbinden("tty0"));
    a.laufe(2'000'000);

    // Eine Sitzung: Anmelden als `kermit`, Init-Paket, danach das Beendigungs-Paket @p ende.
    auto sitzung = [&](const char* ende_daten, const char* titel) {
        a.rx.clear();
        if (!a.bisRx("login:", 40'000'000)) {   // getty hatte ins Leere gesendet: RETURN holt es
            a.sendeRoh("\r");
            ASSERT_TRUE(a.bisRx("login:", 600'000'000)) << sichtbar(a.rx);
        }
        a.laufe(200'000);
        a.rx.clear();
        a.sendeRoh("kermit\r");
        // System-III-login verlangt bei leerem Kennwortfeld ein neues Kennwort ("You don't have a
        // password. Choose one.") — ein kennwortloser Benutzer ist unmoeglich, daher Kennwort "kermit".
        ASSERT_TRUE(a.bisRx("Password:", 600'000'000)) << sichtbar(a.rx);
        a.laufe(200'000);
        a.sendeRoh("kermit\r");
        ASSERT_TRUE(a.bisRx("C-Kermit", 600'000'000)) << sichtbar(a.rx);
        a.laufe(40'000'000);   // 10 s Maschinenzeit: .kermrc laeuft, `server` stellt die Leitung auf Rohbetrieb
        std::fprintf(stderr, "--- %s: nach der Anmeldung empfangen (%zu Byte): %s\n", titel, a.rx.size(),
                     sichtbar(a.rx).c_str());
        EXPECT_FALSE(Anlage::istPrompt(a.rx.substr(a.rx.find_last_of("\n") == std::string::npos ? 0 : a.rx.find_last_of("\n") + 1)))
            << "Shell-Prompt statt Server: " << sichtbar(a.rx);

        // Init-Paket: MAXL 94, TIME 10, NPAD 0, PADC ^@, EOL CR, QCTL '#', QBIN 'Y', CHKT '1'
        const size_t ab = a.rx.size();
        a.sendeRoh(kermitPaket(0, 'I', "~* @-#Y1"));
        std::string antwort;
        const bool ok = a.bisPaket('Y', 400'000'000, &antwort, ab);
        std::fprintf(stderr, "--- %s: Antwort auf I (%zu Byte): %s\n", titel, a.rx.size() - ab,
                     sichtbar(a.rx.substr(ab)).c_str());
        ASSERT_TRUE(ok) << sichtbar(a.rx.substr(ab));
        EXPECT_GE(antwort.size(), 6u) << sichtbar(antwort);   // Y mit Parametern (MAXL, TIME, …)

        // Beenden: `G F` bzw. `G L`; danach muss getty sich wieder melden.
        const size_t ab2 = a.rx.size();
        a.sendeRoh(kermitPaket(1, 'G', ende_daten));
        ASSERT_TRUE(a.bisPaket('Y', 400'000'000, &antwort, ab2)) << sichtbar(a.rx.substr(ab2));
        ASSERT_TRUE(a.bisRx("login:", 800'000'000, ab2)) << "keine Abmeldung nach " << titel << ": " << sichtbar(a.rx.substr(ab2));
        std::fprintf(stderr, "--- %s: nach dem Ende (%zu Byte): %s\n", titel, a.rx.size() - ab2,
                     sichtbar(a.rx.substr(ab2)).c_str());
    };
    ASSERT_NO_FATAL_FAILURE(sitzung("F", "Finish (G F)"));
    ASSERT_NO_FATAL_FAILURE(sitzung("L", "Logout (G L)"));

    ASSERT_TRUE(a.kommando("who"));
    std::fprintf(stderr, "--- who (Konsole)\n%s", a.bild().c_str());
    // Saubere Abschaltung (die Temp-Platte wird ohnehin verworfen)
    ASSERT_TRUE(a.kommando("sync;sync"));
    std::fprintf(stderr, "  [fertig: %.1f s Maschinenzeit, %.0f s Rechenzeit]\n", double(m.totalCycles()) / 4e6,
                 std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
}
