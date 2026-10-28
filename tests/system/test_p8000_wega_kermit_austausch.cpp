/**
 * @file test_p8000_wega_kermit_austausch.cpp
 * @brief Gegenprobe der Kermit-Bibliothek (`kermit/`) am ECHTEN C-Kermit 4.2(030) unter WEGA 3.1
 *        im Emulator (Entwurf 36 AP K7), beide Richtungen.
 *
 *   Fall 1  WEGA = Server hinter getty (Benutzer `kermit` auf tty0, Hub-Kanal als Telnet-Server),
 *           PC = `Sitzung` ueber `TelnetLeitung` mit dem Login-Skript der Bibliothek:
 *           G D / G C / R / S / G E / G F / G L, Dateien 0 B … 100 KB (alle 256 Bytewerte),
 *           Namen > 14 Zeichen, Namensumsetzung, Abbruch mitten in der Datei.
 *   Fall 2  WEGA = Client (`kermit` an der Konsole, `set line /dev/tty3`), PC = `Sitzung::serve`
 *           ueber `Freigabe`; tty3 (Druckerleitung) ist ein Hub-Kanal als Telnet-CLIENT.
 *   Messungen  8-Bit-Durchgang (QBIN Y/&), Blockpruefung 1/2/3 mit 8-Bit-Rohdaten, Durchsatz,
 *           Tempo-Schaetzung bei gedrosselter (1x) und freier Maschine, Verbindungsabbruch.
 *
 * Ausgangspunkt ist ein EINGEFRORENER Zustand (Save-State + Plattenabbild, `~/.cache/k1520emu/
 * p8000_wega`, tests/support/p8000_wega.h): Kopie der ausgelieferten Platte
 * `disks/p8000_wega31/p8000_wega31_platte.img.gz`, in die das DiskTool vorab die Testdateien
 * nach `/usr/kermit` legt, einmal kalt gebootet bis `login:` an der Konsole (~390 s Maschinenzeit).
 * Fehlt er oder hat sich die Platte geaendert, wird er hier gebaut.  Gearbeitet wird immer an
 * Temp-Kopien.
 *
 * Faeden: der Hub taktet die Gast-Baudrate in Maschinentakten — die Maschine muss laufen, solange
 * die Bibliothek spricht.  Fall 1: Maschine im `Motor`-Faden (frei oder auf ein Tempo gedrosselt),
 * Bibliothek im Testfaden.  Fall 2: Bibliothek (Server) im eigenen Faden, Maschine + Konsole im
 * Testfaden.  Befunde: doc/merkposten/kermit.md, doc/design/36_kermit.md §6.5.
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
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "core/logger.h"
#include "core/machines/p8000/p8000.h"
#include "core/serial/hub.h"
#include "core/util/gzip_datei.h"
#include "kermit/src/leitung_netz.h"
#include "kermit/src/loginskript.h"
#include "kermit/src/protokoll.h"
#include "tests/support/p8000_input.h"
#include "tests/support/p8000_wega.h"
#include "tests/support/temp_path.h"
#include "tests/system/wega_kermit_stand.h"

using namespace k1520test::p8000;
using namespace k1520::serial;
namespace km = k1520::kermit;
namespace fs = std::filesystem;

#ifndef P8000_WEGA31_DIR
#define P8000_WEGA31_DIR "disks/p8000_wega31"
#endif
#ifndef K1520_DISKTOOL
#define K1520_DISKTOOL "k1520disktool"
#endif

namespace {

// Anlage, eingefrorener Zustand, Kaltstart und Motor: tests/system/wega_kermit_stand.h

/// Stoppuhr ueber Wirts- und Maschinenzeit.
struct Uhr {
    const Motor& mo;
    std::chrono::steady_clock::time_point w0 = std::chrono::steady_clock::now();
    double m0;
    explicit Uhr(const Motor& m) : mo(m), m0(m.sekunden()) {}
    double wirt() const { return std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count(); }
    double maschine() const { return mo.sekunden() - m0; }
};

/// Messprotokoll: eine Zeile je Messung auf stderr (fuer doc/merkposten/kermit.md).
void messung(const char* was, size_t bytes, const Uhr& u, const km::Sitzung& s) {
    const auto& st = s.statistik();
    std::fprintf(stderr,
                 "  MESSUNG %-28s %7zu B  %6.1f s Wirt  %6.1f s Maschine  %6.0f B/s Wirt  %5.0f B/s Maschine  "
                 "Tempo %.2f  Frist %lld ms  RTT %.0f ms  Wdh %d  NAKe %d/%d  Fristen %d  defekt %d\n",
                 was, bytes, u.wirt(), u.maschine(), bytes / std::max(1e-3, u.wirt()), bytes / std::max(1e-3, u.maschine()),
                 s.tempo(), (long long)s.aktuelleFrist(), s.umlaufzeit(), st.wiederholungen, st.naks_gesendet,
                 st.naks_empfangen, st.fristen, st.fehlerhaft);
}

/// Client-Seite von Fall 1: Leitung zum tty0-Kanal, Anmeldung, Sitzung.
struct Client {
    std::unique_ptr<km::TelnetLeitung> l;
    std::unique_ptr<km::Sitzung> s;
    std::vector<std::string> ereignisse;
    km::LoginErgebnis login;

    ::testing::AssertionResult verbinden(int port, km::Einstellungen e = {}) {
        std::string f;
        l = km::TelnetLeitung::verbinden("127.0.0.1", port, 5000, {}, &f);
        if (!l) return ::testing::AssertionFailure() << "verbinden: " << f;
        km::Rueckrufe r;
        r.ereignis = [this](const std::string& t) {
            ereignisse.push_back(t);
            if (std::getenv("K1520_KERMIT_LAUT")) std::fprintf(stderr, "    [ereignis] %s\n", t.c_str());
        };
        if (std::getenv("K1520_KERMIT_PAKETE"))
            r.paket = [](bool g, char t, int q, const std::string& d) {
                std::fprintf(stderr, "    %s %c%-2d %s\n", g ? ">>" : "<<", t, q, sichtbar(d, 120).c_str());
            };
        s = std::make_unique<km::Sitzung>(*l, e, r);
        return ::testing::AssertionSuccess();
    }
    ::testing::AssertionResult anmelden(km::LoginOptionen o = {}) {
        login = km::loginSkript(*l, o);
        if (!login.ok) return ::testing::AssertionFailure() << login.text;
        if (login.tempo > 0) s->setzeTempoSchaetzung(login.tempo);
        std::fprintf(stderr, "  [angemeldet: %zu Zeichen gelesen, %.0f Zeichen/s, Tempo %.2f]\n", login.gelesen.size(),
                     login.zeichen_pro_s, login.tempo);
        return ::testing::AssertionSuccess();
    }
    std::string letztesAusgehandelt() const {
        for (auto it = ereignisse.rbegin(); it != ereignisse.rend(); ++it)
            if (it->rfind("Ausgehandelt", 0) == 0) return *it;
        return "";
    }
};

/// Liest roh von der Leitung, bis @p nadel (Bit 7 geloescht) kommt — fuer `login:` nach G F/G L.
bool rohBis(km::Leitung& l, const std::string& nadel, int ms, std::string* gelesen) {
    const auto ende = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    std::string rx;
    while (std::chrono::steady_clock::now() < ende) {
        uint8_t b[256];
        const int n = l.lesen(b, sizeof b, 500);
        if (n < 0) break;
        for (int i = 0; i < n; ++i) rx += char(b[i] & 0x7F);
        if (rx.find(nadel) != std::string::npos) break;
        if (n == 0) l.schreiben(reinterpret_cast<const uint8_t*>("\r"), 1);
    }
    if (gelesen) *gelesen = rx;
    return rx.find(nadel) != std::string::npos;
}

struct TempOrdner {
    static inline int zaehler = 0;
    fs::path p = fs::path(k1520test::tempPath("k1520_k7_ordner" + std::to_string(++zaehler)));
    TempOrdner() { fs::create_directories(p); }
    ~TempOrdner() {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
    std::string s() const { return p.u8string(); }
};

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════════════════════
// Fall 1: WEGA = Server hinter getty, PC = Client der Bibliothek
// ═════════════════════════════════════════════════════════════════════════════════════════════

TEST(P8000WegaKermit, PcClientGegenWegaServer) {
    stumm();
    Anlage a;
    std::string grund;
    if (!standLaden(a, &grund)) {
        if (HasFatalFailure()) return;
        GTEST_SKIP() << grund;
    }
    const int port = a.kanal("tty0", Rolle::Server);
    ASSERT_GT(port, 0);
    Motor mo(a.m());
    mo.start(0);

    Client c;
    km::Einstellungen e;
    e.max_namenslaenge = 14;
    ASSERT_TRUE(c.verbinden(port, e));
    ASSERT_TRUE(c.anmelden());
    km::Sitzung& s = *c.s;
    TempOrdner hier;

    // ── G D: ls -l des Gastes, zerlegt ──
    {
        Uhr u(mo);
        const km::Ergebnis r = s.entferntVerzeichnis("");
        ASSERT_TRUE(r.ok()) << r.text;
        messung("G D", r.ausgabe.size(), u, s);
        std::fprintf(stderr, "--- G D (%zu Zeichen)\n%s\n", r.ausgabe.size(), r.ausgabe.c_str());
        std::fprintf(stderr, "  %s\n", c.letztesAusgehandelt().c_str());
        const auto liste = km::zerlegeLsListe(r.ausgabe);
        std::map<std::string, int64_t> gross;
        for (const auto& v : liste) gross[v.name] = v.groesse;
        for (const auto& [name, inhalt] : testdateien()) {
            ASSERT_TRUE(gross.count(name)) << "fehlt in G D: " << name;
            EXPECT_EQ(gross[name], int64_t(inhalt.size())) << name;
        }
    }

    // ── R: Dateien holen (binaer, literal) ──
    for (const char* name : {"text.txt", "alle256", "leer", "eins", "b94", "b95", "vierzehn_zeich"}) {
        Uhr u(mo);
        s.statistikZuruecksetzen();
        const km::Ergebnis r = s.holen(name, hier.s());
        messung((std::string("R ") + name).c_str(), testdateien().at(name).size(), u, s);
        ASSERT_TRUE(r.ok()) << name << ": " << r.text;
        ASSERT_EQ(r.dateien.size(), 1u) << name;
        EXPECT_EQ(lies(hier.p / r.dateien[0]), testdateien().at(name)) << name << " als " << r.dateien[0];
        if (std::string(name) == "alle256") std::fprintf(stderr, "  %s\n", c.letztesAusgehandelt().c_str());
    }
    {
        Uhr u(mo);
        s.statistikZuruecksetzen();
        const km::Ergebnis r = s.holen("zufall100k", hier.s());
        messung("R zufall100k", 102400, u, s);
        ASSERT_TRUE(r.ok()) << r.text;
        EXPECT_EQ(lies(hier.p / "zufall100k"), testdateien().at("zufall100k"));
    }

    // ── G C /tmp, S, Rueckholen, G E ──
    {
        const km::Ergebnis r = s.entferntWechseln("/tmp");
        ASSERT_TRUE(r.ok()) << r.text;
        std::fprintf(stderr, "  [G C /tmp -> \"%s\"]\n", r.ausgabe.c_str());
        schreibe(hier.p / "rueck100k", testdateien().at("zufall100k"));
        Uhr u(mo);
        s.statistikZuruecksetzen();
        const km::Ergebnis w = s.senden((hier.p / "rueck100k").u8string(), "rueck100k");
        messung("S rueck100k", 102400, u, s);
        ASSERT_TRUE(w.ok()) << w.text;
        fs::remove(hier.p / "rueck100k");
        const km::Ergebnis d = s.entferntVerzeichnis("");
        ASSERT_TRUE(d.ok()) << d.text;
        std::fprintf(stderr, "--- G D in /tmp\n%s\n", d.ausgabe.c_str());
        const km::Ergebnis g = s.holen("rueck100k", hier.s());
        ASSERT_TRUE(g.ok()) << g.text;
        EXPECT_EQ(lies(hier.p / "rueck100k"), testdateien().at("zufall100k")) << "Rundweg PC -> WEGA -> PC";
        const km::Ergebnis x = s.entferntLoeschen("rueck100k");
        EXPECT_TRUE(x.ok()) << x.text;
        std::fprintf(stderr, "  [G E -> \"%s\"]\n", sichtbar(x.ausgabe).c_str());
        const km::Ergebnis d2 = s.entferntVerzeichnis("");
        EXPECT_EQ(d2.ausgabe.find("rueck100k"), std::string::npos) << d2.ausgabe;
    }

    // ── Namen: > 14 Zeichen (abgelehnt bzw. vom Gast gekuerzt), Umsetzung ──
    {
        schreibe(hier.p / "einsehrlangername.bin", "lang\n");
        km::Ergebnis r = s.senden((hier.p / "einsehrlangername.bin").u8string(), "");
        EXPECT_EQ(r.art, km::Fehlerart::NameZuLang) << r.text;
        s.einstellungen().max_namenslaenge = 0;
        r = s.senden((hier.p / "einsehrlangername.bin").u8string(), "");
        EXPECT_TRUE(r.ok()) << r.text;
        s.einstellungen().max_namenslaenge = 14;
        const km::Ergebnis d = s.entferntVerzeichnis("ein*");
        std::fprintf(stderr, "--- Name mit 21 Zeichen ohne Grenze gesendet, G D ein*:\n%s\n", d.ausgabe.c_str());
        EXPECT_NE(d.ausgabe.find("einsehrlangern"), std::string::npos) << "WEGA kuerzt auf 14 Zeichen";
        s.entferntLoeschen("einsehrlangern");
        // Server sendet konvertiert (Grossschrift): Empfang konvertiert -> Kleinschrift, literal -> wie gesendet
        ASSERT_TRUE(s.entferntWechseln("/usr/kermit").ok());
        r = s.holen("MixedCase.Txt", hier.s());
        ASSERT_TRUE(r.ok()) << r.text;
        ASSERT_EQ(r.dateien.size(), 1u);
        std::fprintf(stderr, "  [R MixedCase.Txt, Empfang konvertiert -> %s]\n", r.dateien[0].c_str());
        EXPECT_EQ(r.dateien[0], "mixedcase.txt");
        s.einstellungen().namen_empfang = km::Namen::Literal;
        r = s.holen("MixedCase.Txt", hier.s());
        ASSERT_TRUE(r.ok()) << r.text;
        std::fprintf(stderr, "  [R MixedCase.Txt, Empfang literal -> %s]\n", r.dateien.empty() ? "?" : r.dateien[0].c_str());
        EXPECT_EQ(r.dateien.at(0), "MIXEDCASE.TXT");
        s.einstellungen().namen_empfang = km::Namen::Konvertiert;
        // Senden konvertiert: "Klein.dat" geht als "KLEIN.DAT", C-Kermit legt "klein.dat" an (zrtol)
        ASSERT_TRUE(s.entferntWechseln("/tmp").ok());
        schreibe(hier.p / "Klein.dat", "k\n");
        s.einstellungen().namen_senden = km::Namen::Konvertiert;
        r = s.senden((hier.p / "Klein.dat").u8string(), "");
        s.einstellungen().namen_senden = km::Namen::Literal;
        ASSERT_TRUE(r.ok()) << r.text;
        schreibe(hier.p / "Gross.dat", "g\n");
        r = s.senden((hier.p / "Gross.dat").u8string(), "");
        ASSERT_TRUE(r.ok()) << r.text;
        const km::Ergebnis d2 = s.entferntVerzeichnis("");
        std::fprintf(stderr, "--- /tmp nach Klein.dat (konvertiert) und Gross.dat (literal):\n%s\n", d2.ausgabe.c_str());
    }

    // ── Abbruch mitten in der Datei: holen (X) und senden (Z "D") ──
    {
        bool einmal = false;
        km::Sitzung* sp = &s;
        km::Rueckrufe rr{[&](const std::string&, int64_t b, int64_t) {
                             if (b > 20000 && !einmal) { einmal = true; sp->abbrechenDatei(); }
                         },
                         nullptr};
        km::Sitzung sa(*c.l, s.einstellungen(), rr);
        sp = &sa;
        ASSERT_TRUE(sa.entferntWechseln("/usr/kermit").ok());
        TempOrdner ab;
        km::Ergebnis r = sa.holen("zufall100k", ab.s());
        std::fprintf(stderr, "  [Abbruch beim Holen: %s / %s]\n", km::fehlerText(r.art), r.text.c_str());
        EXPECT_EQ(r.art, km::Fehlerart::Abgebrochen) << r.text;
        EXPECT_FALSE(fs::exists(ab.p / "zufall100k")) << "halbe Datei beim PC";
        ASSERT_TRUE(sa.entferntVerzeichnis("").ok()) << "Server nach dem Abbruch nicht mehr bedienbereit";
        ASSERT_TRUE(sa.entferntWechseln("/tmp").ok());
        einmal = false;
        schreibe(hier.p / "abbruch.bin", testdateien().at("zufall100k"));
        r = sa.senden((hier.p / "abbruch.bin").u8string(), "");
        std::fprintf(stderr, "  [Abbruch beim Senden: %s / %s]\n", km::fehlerText(r.art), r.text.c_str());
        EXPECT_EQ(r.art, km::Fehlerart::Abgebrochen) << r.text;
        const km::Ergebnis d = sa.entferntVerzeichnis("");
        ASSERT_TRUE(d.ok()) << d.text;
        std::fprintf(stderr, "--- /tmp nach abgebrochenem Senden (C-Kermit 4.2 behaelt/verwirft?):\n%s\n", d.ausgabe.c_str());
        sa.entferntLoeschen("abbruch.bin");
    }

    // ── G F: Server beendet, getty meldet sich wieder ──
    {
        const km::Ergebnis r = s.entferntBeenden();
        ASSERT_TRUE(r.ok()) << r.text;
        std::string rx;
        EXPECT_TRUE(rohBis(*c.l, "login:", 120000, &rx)) << sichtbar(rx);
    }
    // ── zweite Sitzung auf derselben Verbindung: wieder anmelden, G L ──
    {
        ASSERT_TRUE(c.anmelden());
        const km::Ergebnis r = s.entferntAbmelden();
        ASSERT_TRUE(r.ok()) << r.text;
        std::string rx;
        EXPECT_TRUE(rohBis(*c.l, "login:", 120000, &rx)) << sichtbar(rx);
    }
    mo.stop();
    std::fprintf(stderr, "  [fertig: %.0f s Maschinenzeit]\n", a.sekunden());
}

// ═════════════════════════════════════════════════════════════════════════════════════════════
// Fall 2: WEGA = Client an der Konsole, PC = Server der Bibliothek (Freigabe) an tty3
// ═════════════════════════════════════════════════════════════════════════════════════════════

TEST(P8000WegaKermit, WegaClientGegenPcServer) {
    stumm();
    Anlage a;
    std::string grund;
    if (!standLaden(a, &grund)) {
        if (HasFatalFailure()) return;
        GTEST_SKIP() << grund;
    }
    // PC-Server: lauscht auf einem freien Port, gibt einen Ordner frei.
    TempOrdner frei;
    fs::create_directories(frei.p / "unter");
    const std::string pcBin = zufall(3000, 3000) + std::string(testdateien().at("alle256"));
    schreibe(frei.p / "pcbin", pcBin);
    schreibe(frei.p / "pctext.txt", testdateien().at("text.txt"));
    std::string f;
    auto lausch = km::TelnetLeitung::lauschen(0, {}, &f);
    ASSERT_TRUE(lausch) << f;
    const int pcPort = lausch->port();
    std::vector<std::string> ereignisse;
    std::mutex em;
    km::Ergebnis serverErgebnis;
    std::atomic<bool> serverFertig{false};
    km::Statistik serverStat;
    std::thread server([&] {
        std::string fe;
        if (!lausch->annehmen(600000, &fe)) {
            serverErgebnis.art = km::Fehlerart::LeitungGetrennt;
            serverErgebnis.text = "annehmen: " + fe;
            serverFertig = true;
            return;
        }
        km::Freigabe fg(frei.p);
        km::Rueckrufe r;
        r.ereignis = [&](const std::string& t) {
            std::lock_guard<std::mutex> lk(em);
            ereignisse.push_back(t);
            std::fprintf(stderr, "    [PC-Server] %s\n", t.c_str());
        };
        km::Einstellungen e;
        e.max_namenslaenge = 0;
        km::Sitzung s(*lausch, e, r);
        serverErgebnis = s.serve(fg);
        serverStat = s.statistik();
        serverFertig = true;
    });
    struct Join {
        std::thread& t;
        km::TelnetLeitung& l;
        ~Join() {
            l.wecken();
            l.schliessen();
            if (t.joinable()) t.join();
        }
    } join{server, *lausch};

    // tty3 (Druckerleitung, kein getty) als Telnet-CLIENT zum PC-Server.
    ASSERT_EQ(a.kanal("tty3", Rolle::Client, pcPort), pcPort);
    for (int i = 0; i < 200 && a.kanalZustand("tty3") != Zustand::Verbunden; ++i) a.laufe(400'000);
    ASSERT_EQ(a.kanalZustand("tty3"), Zustand::Verbunden);

    ASSERT_TRUE(a.anmeldenKonsole("wega", "root"));
    ASSERT_TRUE(a.bisPrompt(40'000'000)) << a.bild();
    a.tippeZeile("cd /tmp");
    ASSERT_TRUE(a.bisPrompt(400'000'000)) << a.bild();
    a.tippeZeile("kermit");
    ASSERT_GE(a.frage({"C-Kermit>"}, 400'000'000), 0) << a.bild();
    auto kermit = [&](const std::string& cmd, long long grenze = 4'000'000'000LL) {
        const double m0 = a.sekunden();
        const auto w0 = std::chrono::steady_clock::now();
        a.tippeZeile(cmd);
        const bool ok = a.bisKermitPrompt(grenze);
        std::fprintf(stderr, "--- C-Kermit> %s  (%.1f s Maschine, %.1f s Wirt)\n%s", cmd.c_str(), a.sekunden() - m0,
                     std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count(), a.bild().c_str());
        return ok;
    };
    ASSERT_TRUE(kermit("set line /dev/tty3"));
    ASSERT_TRUE(kermit("set speed 9600"));
    ASSERT_TRUE(kermit("show parameters"));

    // send im Textmodus (Vorgabe von C-Kermit 4.2): LF → CR LF auf der Leitung, keine Attribute —
    // der PC-Server (binaer) legt die Datei mit CR LF ab.
    ASSERT_TRUE(kermit("send /usr/kermit/text.txt"));
    {
        std::string crlf;
        for (char ch : testdateien().at("text.txt")) {
            if (ch == '\n') crlf += '\r';
            crlf += ch;
        }
        EXPECT_EQ(lies(frei.p / "text.txt"), crlf) << "Textdatei: C-Kermit sendet CR LF, Name konvertiert (TEXT.TXT -> text.txt)";
    }
    ASSERT_TRUE(kermit("set file type binary"));
    // send: WEGA → PC, binaer
    ASSERT_TRUE(kermit("send /usr/kermit/alle256"));
    EXPECT_EQ(lies(frei.p / "alle256"), testdateien().at("alle256")) << "send WEGA -> PC (alle 256 Bytewerte)";

    // get: PC → WEGA, danach zurueck zum PC unter neuem Namen und vergleichen
    ASSERT_TRUE(kermit("get pcbin"));
    ASSERT_TRUE(kermit("send pcbin zurueck"));
    EXPECT_EQ(lies(frei.p / "zurueck"), pcBin) << "Rundweg PC -> WEGA (get) -> PC (send)";

    // remote dir / remote cwd
    ASSERT_TRUE(kermit("remote directory"));
    EXPECT_NE(a.bild().find("pcbin"), std::string::npos) << a.bild();
    // `remote cwd` fragt in C-Kermit 4.2 nach einem Kennwort (zweites Feld von G C) — leer lassen.
    a.tippeZeile("remote cwd unter");
    ASSERT_GE(a.frage({"Password:"}, 400'000'000), 0) << a.bild();
    ASSERT_TRUE(kermit(""));
    ASSERT_TRUE(kermit("send /usr/kermit/b95"));
    EXPECT_EQ(lies(frei.p / "unter" / "b95"), testdateien().at("b95"));

    // finish: der PC-Server endet
    ASSERT_TRUE(kermit("finish"));
    for (int i = 0; i < 100 && !serverFertig; ++i) a.laufe(4'000'000);
    EXPECT_TRUE(serverFertig);
    EXPECT_TRUE(serverErgebnis.ok()) << serverErgebnis.text;
    std::fprintf(stderr, "  [PC-Server: %d Pakete, %d Wdh, %d NAKe gesendet, %d defekt]\n", serverStat.pakete_gesendet,
                 serverStat.wiederholungen, serverStat.naks_gesendet, serverStat.fehlerhaft);
    a.tippeZeile("exit");
    EXPECT_TRUE(a.bisPrompt(400'000'000)) << a.bild();
}

// ═════════════════════════════════════════════════════════════════════════════════════════════
// Messungen: 8-Bit-Durchgang (QBIN), Blockpruefung 1/2/3 mit 8-Bit-Rohdaten, gegen C-Kermit 4.2
// ═════════════════════════════════════════════════════════════════════════════════════════════

TEST(P8000WegaKermit, PruefsummenUnd8Bit) {
    stumm();
    Anlage a;
    std::string grund;
    if (!standLaden(a, &grund)) {
        if (HasFatalFailure()) return;
        GTEST_SKIP() << grund;
    }
    const int port = a.kanal("tty0", Rolle::Server);
    ASSERT_GT(port, 0);
    Motor mo(a.m());
    mo.start(0);
    Client c;
    km::Einstellungen e;
    e.max_wiederholungen = 4;
    ASSERT_TRUE(c.verbinden(port, e));
    ASSERT_TRUE(c.anmelden());
    km::Sitzung& s = *c.s;
    TempOrdner hier;
    ASSERT_TRUE(s.entferntWechseln("/tmp").ok());
    const std::string& alle = testdateien().at("alle256");
    schreibe(hier.p / "quelle", alle);

    // Nach einem Fehlschlag haengt C-Kermit noch in seinen Wiederholungen (resend() leert den Eingang,
    // bis die Leitung 1 s still ist): warten, bis sein Leerlauf-NAK kommt und die Leitung still ist.
    auto erholen = [&]() {
        std::string rx;
        const auto ende = std::chrono::steady_clock::now() + std::chrono::seconds(120);
        bool nak = false;
        while (std::chrono::steady_clock::now() < ende) {
            uint8_t b[256];
            const int n = c.l->lesen(b, sizeof b, 2000);
            if (n < 0) break;
            if (n == 0 && nak) break;
            rx.append(reinterpret_cast<char*>(b), size_t(std::max(0, n)));
            if (rx.find("\x01# N") != std::string::npos) nak = true;   // N(0), Pruefung 1
        }
        // C-Kermit 4.2 behaelt nach einem mitten in der Datei abgebrochenen Senden Reste des letzten
        // Pakets (getpkt: `static char leftover[6]`) und stellt sie dem NAECHSTEN Paket voran — beim
        // Holen kam dann ein Dateiname mit einem fremden Byte davor an.  Ein G D (sxpack → getpkt)
        // nimmt die Reste auf.
        const char q = s.einstellungen().qbin;
        const int k = s.einstellungen().pruefsumme;
        s.einstellungen().qbin = '&';
        s.einstellungen().pruefsumme = 3;
        s.entferntVerzeichnis("/usr/kermit/leer");
        s.einstellungen().qbin = q;
        s.einstellungen().pruefsumme = k;
    };

    std::string tabelle;
    auto zeile = [&](const char* richtung, char qbin, int chk, bool ok, const km::Ergebnis& r, int wdh) {
        char z[400];
        std::snprintf(z, sizeof z, "  TABELLE %s QBIN %c Pruefung %d: %s (Wdh %d, ausgehandelt Pruefung %d, QBIN %s)%s%s\n",
                      richtung, qbin, chk, ok ? "ok" : "FEHLT", wdh, s.vereinbarung().chkt,
                      s.vereinbarung().kod.qbin ? std::string(1, s.vereinbarung().kod.qbin).c_str() : "aus",
                      r.ok() ? "" : " — ", r.ok() ? "" : r.text.c_str());
        tabelle += z;
        std::fprintf(stderr, "%s", z);
    };
    // 1) WEGA → PC (R): alle Kombinationen
    // Reihenfolge: der erwartete Fehlschlag (QBIN Y + Pruefung 2) zuletzt.
    for (auto [qbin, chk] : std::vector<std::pair<char, int>>{{'Y', 1}, {'Y', 3}, {'&', 1}, {'&', 2}, {'&', 3}, {'Y', 2}}) {
        {
            s.einstellungen().qbin = qbin;
            s.einstellungen().pruefsumme = chk;
            s.statistikZuruecksetzen();
            Uhr u(mo);
            const km::Ergebnis r = s.holen("/usr/kermit/alle256", hier.s());
            const std::string da = lies(hier.p / "alle256");
            const bool ok = r.ok() && da == alle;
            if (r.ok() && !ok) {
                size_t i = 0;
                while (i < da.size() && i < alle.size() && da[i] == alle[i]) ++i;
                std::fprintf(stderr, "  [R Q%c P%d: %zu statt %zu Bytes, erster Unterschied bei %zu (dateien: %s)]\n", qbin, chk,
                             da.size(), alle.size(), i, r.dateien.empty() ? "-" : r.dateien[0].c_str());
            }
            messung((std::string("R alle256 Q") + qbin + " P" + std::to_string(chk)).c_str(), alle.size(), u, s);
            zeile("R (WEGA->PC)", qbin, chk, ok, r, s.statistik().wiederholungen + s.statistik().naks_gesendet);
            std::error_code ec;
            fs::remove(hier.p / "alle256", ec);
            if (!ok) erholen();
        }
    }
    // 2) PC → WEGA (S), zuerst mit Praefix, die Rohfaelle (QBIN Y) zuletzt
    for (char qbin : {'&', 'Y'})
        for (int chk : {1, 2, 3}) {
            s.einstellungen().qbin = qbin;
            s.einstellungen().pruefsumme = chk;
            const std::string ziel = std::string("s") + (qbin == '&' ? "a" : "y") + std::to_string(chk);
            s.statistikZuruecksetzen();
            Uhr u(mo);
            const km::Ergebnis w = s.senden((hier.p / "quelle").u8string(), ziel);
            messung(("S alle256 " + ziel).c_str(), alle.size(), u, s);
            const int wdh = s.statistik().wiederholungen + s.statistik().naks_gesendet;
            const km::Vereinbarung v = s.vereinbarung();
            bool ok = false;
            if (!w.ok()) erholen();
            // zurueck mit dem sicheren Weg (QBIN &, Pruefung 3) und vergleichen
            s.einstellungen().qbin = '&';
            s.einstellungen().pruefsumme = 3;
            const km::Ergebnis g = s.holen(ziel, hier.s());
            ok = w.ok() && g.ok() && lies(hier.p / ziel) == alle;
            if (!g.ok()) erholen();
            s.entferntLoeschen(ziel);
            char z[400];
            std::snprintf(z, sizeof z, "  TABELLE S (PC->WEGA) QBIN %c Pruefung %d: %s (Wdh %d, ausgehandelt Pruefung %d, QBIN %s)%s%s\n",
                          qbin, chk, ok ? "ok" : "FEHLT", wdh, v.chkt, v.kod.qbin ? std::string(1, v.kod.qbin).c_str() : "aus",
                          w.ok() ? "" : " — ", w.ok() ? "" : w.text.c_str());
            tabelle += z;
            std::fprintf(stderr, "%s", z);
        }
    std::fprintf(stderr, "%s", tabelle.c_str());
    s.einstellungen().qbin = '&';
    s.einstellungen().pruefsumme = 3;
    EXPECT_TRUE(s.entferntBeenden().ok());
    // Waechter: mit 8-Bit-Praefix (Vorgabe der Bibliothek) tragen alle drei Blockpruefungen in beiden
    // Richtungen; ohne Praefix nur WEGA -> PC mit Pruefung 1/3 (Messwerte: Entwurf 36 §6.5).
    for (int chk : {1, 2, 3}) {
        const std::string n = std::to_string(chk);
        EXPECT_NE(tabelle.find("R (WEGA->PC) QBIN & Pruefung " + n + ": ok"), std::string::npos) << tabelle;
        EXPECT_NE(tabelle.find("S (PC->WEGA) QBIN & Pruefung " + n + ": ok"), std::string::npos) << tabelle;
    }
    EXPECT_NE(tabelle.find("R (WEGA->PC) QBIN Y Pruefung 3: ok"), std::string::npos) << tabelle;
}

// ═════════════════════════════════════════════════════════════════════════════════════════════
// Messung: Tempo der Gegenstelle — Maschine frei (≈ 5–6x), gedrosselt auf 1x und auf 0.3x (Last)
// ═════════════════════════════════════════════════════════════════════════════════════════════

class P8000WegaKermitTempo : public ::testing::TestWithParam<double> {};

TEST_P(P8000WegaKermitTempo, AdaptiveFristen) {
    stumm();
    const double tempo = GetParam();
    Anlage a;
    std::string grund;
    if (!standLaden(a, &grund)) {
        if (HasFatalFailure()) return;
        GTEST_SKIP() << grund;
    }
    const int port = a.kanal("tty0", Rolle::Server);
    ASSERT_GT(port, 0);
    Motor mo(a.m());
    mo.start(tempo);
    Client c;
    ASSERT_TRUE(c.verbinden(port));   // Vorgaben der Bibliothek: Automatik
    {
        Uhr u(mo);
        ASSERT_TRUE(c.anmelden());
        std::fprintf(stderr, "  MESSUNG Tempo %.1f: Anmeldung %.1f s Wirt, %.1f s Maschine, Login-Schaetzung %.2f\n", tempo,
                     u.wirt(), u.maschine(), c.login.tempo);
    }
    km::Sitzung& s = *c.s;
    TempOrdner hier;
    char tag[64];
    std::snprintf(tag, sizeof tag, "Tempo %.1f G D", tempo);
    {
        Uhr u(mo);
        const km::Ergebnis r = s.entferntVerzeichnis("");
        ASSERT_TRUE(r.ok()) << r.text;
        messung(tag, r.ausgabe.size(), u, s);
    }
    std::snprintf(tag, sizeof tag, "Tempo %.1f R alle256", tempo);
    {
        s.statistikZuruecksetzen();
        Uhr u(mo);
        const km::Ergebnis r = s.holen("alle256", hier.s());
        messung(tag, 4096, u, s);
        ASSERT_TRUE(r.ok()) << r.text;
        EXPECT_EQ(lies(hier.p / "alle256"), testdateien().at("alle256"));
        EXPECT_EQ(s.statistik().wiederholungen, 0) << "Wiederholung ins Leere bei Tempo " << tempo;
    }
    const double ist = mo.sekunden();
    (void)ist;
    if (tempo > 0) {
        EXPECT_GT(s.tempoSchaetzung(), tempo * 0.5) << "Schaetzung zu klein";
        EXPECT_LT(s.tempoSchaetzung(), tempo * 1.5) << "Schaetzung zu gross";
    }
    EXPECT_TRUE(s.entferntBeenden().ok());
}

INSTANTIATE_TEST_SUITE_P(Takt, P8000WegaKermitTempo, ::testing::Values(0.0, 1.0, 0.3),
                         [](const ::testing::TestParamInfo<double>& i) {
                             return i.param == 0.0 ? std::string("frei") : i.param == 1.0 ? std::string("echtzeit") : std::string("last");
                         });

// ═════════════════════════════════════════════════════════════════════════════════════════════
// Messung: Verbindungsabbruch mitten in der Datei, danach wieder verbinden
// ═════════════════════════════════════════════════════════════════════════════════════════════

TEST(P8000WegaKermit, VerbindungsabbruchUndWiederaufnahme) {
    stumm();
    Anlage a;
    std::string grund;
    if (!standLaden(a, &grund)) {
        if (HasFatalFailure()) return;
        GTEST_SKIP() << grund;
    }
    const int port = a.kanal("tty0", Rolle::Server);
    ASSERT_GT(port, 0);
    Motor mo(a.m());
    mo.start(0);
    TempOrdner hier;
    {
        Client c;
        ASSERT_TRUE(c.verbinden(port));
        ASSERT_TRUE(c.anmelden());
        km::Rueckrufe r;
        std::atomic<bool> getrennt{false};
        km::Leitung* l = c.l.get();
        // Nach 20 KB: Verbindung hart trennen (Socket zu) — wie ein abgestuerzter PC.
        c.s = std::make_unique<km::Sitzung>(*c.l, km::Einstellungen{}, km::Rueckrufe{
            [&](const std::string&, int64_t b, int64_t) {
                if (b > 20000 && !getrennt) {
                    getrennt = true;
                    l->schliessen();
                }
            },
            nullptr});
        const km::Ergebnis e = c.s->holen("zufall100k", hier.s());
        EXPECT_EQ(e.art, km::Fehlerart::LeitungGetrennt) << e.text;
        EXPECT_FALSE(fs::exists(hier.p / "zufall100k")) << "halbe Datei liegen geblieben";
    }
    // Gast 20 s weiterlaufen lassen: C-Kermit steckt dann noch in der alten Transaktion (NAKt ihr
    // naechstes Paket, ohne Eingriff bis MAXTRY x (TIME + Leerpause) > 120 s); der neue Client bricht
    // sie mit einem E-Paket ab (Sitzung::iAustausch).
    const double t0 = mo.sekunden();
    while (mo.sekunden() - t0 < 20) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Client c2;
    ASSERT_TRUE(c2.verbinden(port));
    km::Einstellungen e2;
    e2.max_wiederholungen = 3;
    c2.s->einstellungen() = e2;
    const km::Ergebnis d = c2.s->entferntVerzeichnis("");
    std::fprintf(stderr, "  MESSUNG Abbruch: nach 20 s Maschinenzeit wieder verbunden; G D ohne Anmeldung: %s (%s)\n",
                 d.ok() ? "Server lebt" : "keine Antwort", d.text.c_str());
    for (const auto& ev : c2.ereignisse) std::fprintf(stderr, "    [ereignis] %s\n", ev.c_str());
    EXPECT_TRUE(d.ok()) << "Server nach der Trennung nicht wieder erreichbar";
    if (d.ok()) {
        const km::Ergebnis g = c2.s->holen("zufall100k", hier.s());
        EXPECT_TRUE(g.ok()) << g.text;
        EXPECT_EQ(lies(hier.p / "zufall100k"), testdateien().at("zufall100k"));
        EXPECT_TRUE(c2.s->entferntAbmelden().ok());
        std::string rx;
        EXPECT_TRUE(rohBis(*c2.l, "login:", 120000, &rx)) << sichtbar(rx);
    } else {
        // Server nicht mehr da: getty? Anmelden muss gehen.
        ASSERT_TRUE(c2.anmelden());
        EXPECT_TRUE(c2.s->entferntVerzeichnis("").ok());
        EXPECT_TRUE(c2.s->entferntAbmelden().ok());
    }
}

/// Werkzeug (kein Waechter): eine Datei an den WEGA-Server senden, Pakete mit K1520_KERMIT_PAKETE=1.
/// `K1520_KERMIT_DATEI` (Vorgabe zufall100k), `K1520_KERMIT_QBIN` (Y/&), `K1520_KERMIT_CHK` (1..3).
TEST(P8000WegaKermit, DISABLED_SendenEinzeln) {
    stumm();
    Anlage a;
    std::string grund;
    if (!standLaden(a, &grund)) GTEST_SKIP() << grund;
    const int port = a.kanal("tty0", Rolle::Server);
    Motor mo(a.m());
    mo.start(0);
    Client c;
    km::Einstellungen e;
    if (const char* q = std::getenv("K1520_KERMIT_QBIN")) e.qbin = q[0];
    if (const char* k = std::getenv("K1520_KERMIT_CHK")) e.pruefsumme = std::atoi(k);
    ASSERT_TRUE(c.verbinden(port, e));
    ASSERT_TRUE(c.anmelden());
    TempOrdner hier;
    const char* dn = std::getenv("K1520_KERMIT_DATEI");
    const std::string name = dn ? dn : "zufall100k";
    schreibe(hier.p / name, testdateien().at(name));
    ASSERT_TRUE(c.s->entferntWechseln("/tmp").ok());
    Uhr u(mo);
    const km::Ergebnis w = c.s->senden((hier.p / name).u8string(), "");
    messung(("S " + name).c_str(), testdateien().at(name).size(), u, *c.s);
    EXPECT_TRUE(w.ok()) << w.text;
}
