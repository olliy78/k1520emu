/**
 * @file test_kermit_netz.cpp
 * @brief Kermit-Netzleitungen (doc/design/36_kermit.md §4.3, AP K3): Telnet und RFC 2217 ueber
 *        echte Loopback-Sockets (nie feste Ports — Port 0, der Port kommt aus `port()`),
 *        Client <-> Server in beiden Rollenkombinationen, danach ein voller Kermit-Austausch
 *        (100 KB binaer) ueber alle vier Leitungsarten, Trennung mitten in der Datei,
 *        Login-Skript gegen eine WEGA-getty-Attrappe mit gerader Paritaet und die Gegenprobe
 *        gegen einen echten `SerialHub`-Kanal des Emulators (Attrappe `SerialAnschluss`).
 */
#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "core/serial/hub.h"
#include "kermit/src/leitung_netz.h"
#include "kermit/src/loginskript.h"
#include "kermit/src/protokoll.h"
#include "tests/unit/util/attrappe_anschluss.h"

namespace fs = std::filesystem;
using namespace k1520::kermit;
using Art = NetzLeitung::Art;

namespace {

struct TempOrdner {
    fs::path p;
    TempOrdner() {
        std::random_device rd;
        p = fs::temp_directory_path() / ("k1520kermit_n_" + std::to_string(rd()) + std::to_string(rd()));
        fs::create_directories(p);
    }
    ~TempOrdner() {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
    std::string s() const { return p.u8string(); }
};

std::string zufall(std::size_t n, unsigned saat) {
    std::mt19937 g(saat);
    std::string s(n, '\0');
    for (auto& c : s) c = static_cast<char>(g());
    return s;
}
void schreibe(const fs::path& p, const std::string& s) {
    std::ofstream f(p, std::ios::binary);
    f << s;
}
std::string lies(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

/// Ein verbundenes Leitungspaar.  `a` ist die Seite, die VERBINDET (Client), `b` die, die LAUSCHT —
/// oder umgekehrt, je nach `aVerbindet`.  Die Namen beziehen sich auf die Rolle im Test, nicht im Netz.
struct Paar {
    std::unique_ptr<NetzLeitung> a, b;
    std::string fehler;
};

Paar baue(Art art, bool aVerbindet, NetzOptionen oClient = {}, NetzOptionen oServer = {}) {
    Paar p;
    std::unique_ptr<NetzLeitung> server, client;
    std::string f;
    if (art == Art::Telnet) server = TelnetLeitung::lauschen(0, oServer, &f);
    else server = Rfc2217Leitung::lauschen(0, oServer, &f);
    if (!server) {
        p.fehler = f;
        return p;
    }
    const int port = server->port();
    // Der Client blockiert im Aushandeln, bis der Server angenommen hat: eigener Faden.
    auto fut = std::async(std::launch::async, [&]() -> std::unique_ptr<NetzLeitung> {
        std::string fe;
        auto l = art == Art::Telnet ? std::unique_ptr<NetzLeitung>(TelnetLeitung::verbinden("127.0.0.1", port, 5000, oClient, &fe))
                                    : std::unique_ptr<NetzLeitung>(Rfc2217Leitung::verbinden("127.0.0.1", port, 5000, oClient, &fe));
        if (!l) p.fehler = fe;
        return l;
    });
    if (!server->annehmen(5000, &f)) p.fehler = f;
    client = fut.get();
    if (!client || !p.fehler.empty()) return p;
    if (aVerbindet) {
        p.a = std::move(client);
        p.b = std::move(server);
    } else {
        p.a = std::move(server);
        p.b = std::move(client);
    }
    return p;
}

/// Liest genau n Bytes (mit Gesamtfrist).
std::string leseGenau(Leitung& l, std::size_t n, int64_t fristMs = 10000) {
    std::string s;
    const int64_t ende = l.jetztMs() + fristMs;
    uint8_t buf[4096];
    while (s.size() < n && l.jetztMs() < ende) {
        const int k = l.lesen(buf, std::min(sizeof buf, n - s.size()), ende - l.jetztMs());
        if (k < 0) break;
        s.append(reinterpret_cast<char*>(buf), static_cast<std::size_t>(k));
    }
    return s;
}
bool schreibeText(Leitung& l, const std::string& s) {
    return l.schreiben(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

struct Kombi {
    Art art;
    bool aVerbindet;
};
std::string kombiName(const ::testing::TestParamInfo<Kombi>& i) {
    return std::string(i.param.art == Art::Telnet ? "Telnet" : "Rfc2217") + (i.param.aVerbindet ? "_AVerbindet" : "_ALauscht");
}
const Kombi KOMBIS[] = {{Art::Telnet, true}, {Art::Telnet, false}, {Art::Rfc2217, true}, {Art::Rfc2217, false}};

}  // namespace

// ─── Rohe Bytes ─────────────────────────────────────────────────────────────

class KermitNetz : public ::testing::TestWithParam<Kombi> {};

TEST_P(KermitNetz, AchtBitBinaerInBeideRichtungen) {
    Paar p = baue(GetParam().art, GetParam().aVerbindet);
    ASSERT_TRUE(p.a && p.b) << p.fehler;
    EXPECT_TRUE(p.a->verbunden());
    EXPECT_FALSE(p.a->gegenstelle().empty());
    EXPECT_TRUE(p.a->binaerAktiv()) << p.a->hinweis();
    EXPECT_TRUE(p.b->binaerAktiv()) << p.b->hinweis();

    // Alle 256 Werte, IAC-Folgen, CR/LF/NUL-Muster (kein CR-NUL, kein Echo).
    std::string alle;
    for (int i = 0; i < 256; ++i) alle.push_back(static_cast<char>(i));
    alle += std::string("\xFF\xFF\xFF\xFA\xF0\r\n\r\0\r\r\0\0\xFF", 17);
    alle += zufall(20000, 7);
    ASSERT_TRUE(schreibeText(*p.a, alle));
    EXPECT_EQ(leseGenau(*p.b, alle.size()), alle);
    const std::string zurueck = zufall(30000, 9);
    ASSERT_TRUE(schreibeText(*p.b, zurueck));
    EXPECT_EQ(leseGenau(*p.a, zurueck.size()), zurueck);
}

TEST_P(KermitNetz, FristWeckenUndTrennung) {
    Paar p = baue(GetParam().art, GetParam().aVerbindet);
    ASSERT_TRUE(p.a && p.b) << p.fehler;
    uint8_t b[8];
    auto t0 = std::chrono::steady_clock::now();
    EXPECT_EQ(p.a->lesen(b, 8, 150), 0);   // Frist
    EXPECT_GE(std::chrono::steady_clock::now() - t0, std::chrono::milliseconds(120));

    // wecken() aus einem anderen Faden beendet ein langes lesen sofort — und ist "klebrig".
    std::thread t([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        p.a->wecken();
    });
    t0 = std::chrono::steady_clock::now();
    EXPECT_EQ(p.a->lesen(b, 8, 20000), 0);
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::seconds(5));
    t.join();
    p.a->wecken();
    EXPECT_EQ(p.a->lesen(b, 8, 20000), 0);   // das naechste lesen kehrt ebenfalls sofort zurueck

    // Reste vor der Trennung kommen noch an, danach -1.
    ASSERT_TRUE(schreibeText(*p.b, "rest"));
    // Erst den eigenen Eingang leeren (Reste der Verhandlung): ein close() mit ungelesenen Daten
    // sendet unter Windows ein RST und die Gegenseite verliert dabei noch unterwegs befindliche Bytes.
    p.b->lesen(b, 8, 100);
    p.b->schliessen();
    EXPECT_EQ(leseGenau(*p.a, 4), "rest");
    EXPECT_EQ(p.a->lesen(b, 8, 5000), -1);
    EXPECT_FALSE(p.a->verbunden());
}

INSTANTIATE_TEST_SUITE_P(Rollen, KermitNetz, ::testing::ValuesIn(KOMBIS), kombiName);

TEST(KermitNetzEinzel, Rfc2217WunschUndGastWert) {
    // Der Client WUENSCHT 9600, der Server antwortet mit seinem Wert (19200): der Gast ist massgeblich,
    // die Bytes laufen trotzdem.
    NetzOptionen oc, os;
    os.baud = 19200;
    Paar p = baue(Art::Rfc2217, true, oc, os);
    ASSERT_TRUE(p.a && p.b) << p.fehler;
    ASSERT_TRUE(schreibeText(*p.a, "x"));
    ASSERT_EQ(leseGenau(*p.b, 1), "x");   // der Server hat die SET-* des Clients dabei verarbeitet
    ASSERT_TRUE(schreibeText(*p.b, "y"));
    ASSERT_EQ(leseGenau(*p.a, 1), "y");
    EXPECT_EQ(p.a->gegenBaud(), 19200u);
}

TEST(KermitNetzEinzel, FehlerTexte) {
    std::string f;
    // Nichts lauscht auf einem frisch freigegebenen Port.
    int frei;
    {
        auto l = TelnetLeitung::lauschen(0, {}, &f);
        ASSERT_TRUE(l) << f;
        frei = l->port();
    }
    EXPECT_FALSE(TelnetLeitung::verbinden("127.0.0.1", frei, 2000, {}, &f));
    EXPECT_NE(f.find("Verbinden"), std::string::npos) << f;

    // annehmen: Frist, und wecken() bricht es ab.
    auto srv = Rfc2217Leitung::lauschen(0, {}, &f);
    ASSERT_TRUE(srv) << f;
    EXPECT_GT(srv->port(), 0);
    EXPECT_FALSE(srv->annehmen(150, &f));
    EXPECT_NE(f.find("Frist"), std::string::npos) << f;
    std::thread t([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        srv->wecken();
    });
    EXPECT_FALSE(srv->annehmen(20000, &f));
    EXPECT_EQ(f, "abgebrochen");
    t.join();
}

TEST(KermitNetzEinzel, SchlichteGegenstelleOhneTelnetBrauchtKeinBinary) {
    // Ein Server, der kein Telnet spricht (rohes TCP): das Verbinden endet nach der Aushandelfrist
    // trotzdem, mit einem Hinweis.
    using namespace k1520::serial::net;
    ASSERT_TRUE(netzStarten());
    Lauscher l = lauschen(0);
    ASSERT_TRUE(l.sock.gueltig());
    NetzOptionen o;
    o.aushandeln_ms = 300;
    std::string f;
    auto c = TelnetLeitung::verbinden("127.0.0.1", l.port, 3000, o, &f);
    ASSERT_TRUE(c) << f;
    EXPECT_FALSE(c->binaerAktiv());
    EXPECT_FALSE(c->hinweis().empty());
}

// ─── Voller Kermit-Austausch ────────────────────────────────────────────────

class KermitNetzProtokoll : public ::testing::TestWithParam<Kombi> {};

TEST_P(KermitNetzProtokoll, HunderttausendBytesBinaerHinUndZurueck) {
    Paar p = baue(GetParam().art, GetParam().aVerbindet);
    ASSERT_TRUE(p.a && p.b) << p.fehler;
    TempOrdner wurzel, lokal, hol;
    Freigabe freigabe(wurzel.p);
    Ergebnis serverErgebnis;
    std::thread server([&] {
        Einstellungen e;
        e.frist_ms = 10000;
        Sitzung s(*p.b, e);
        serverErgebnis = s.serve(freigabe);
    });

    std::string inhalt = zufall(100 * 1024, 42);
    inhalt += std::string("\r\n\r\0\xFF\xFF\xFF", 8);   // die heiklen Bytes ans Ende
    schreibe(lokal.p / "gross.bin", inhalt);
    Einstellungen e;
    e.frist_ms = 10000;
    Sitzung c(*p.a, e);
    Ergebnis r = c.senden((lokal.p / "gross.bin").u8string(), "gross.bin");
    ASSERT_TRUE(r.ok()) << r.text;
    EXPECT_EQ(lies(wurzel.p / "gross.bin"), inhalt);

    r = c.holen("gross.bin", hol.s());
    ASSERT_TRUE(r.ok()) << r.text;
    EXPECT_EQ(lies(hol.p / "gross.bin"), inhalt);

    r = c.entferntVerzeichnis("");
    ASSERT_TRUE(r.ok()) << r.text;
    EXPECT_NE(r.ausgabe.find("gross.bin"), std::string::npos) << r.ausgabe;
    r = c.entferntAbmelden();
    EXPECT_TRUE(r.ok()) << r.text;
    server.join();
    EXPECT_TRUE(serverErgebnis.ok()) << serverErgebnis.text;
}

TEST_P(KermitNetzProtokoll, TrennungMittenInDerDatei) {
    Paar p = baue(GetParam().art, GetParam().aVerbindet);
    ASSERT_TRUE(p.a && p.b) << p.fehler;
    TempOrdner wurzel, lokal;
    Freigabe freigabe(wurzel.p);
    Ergebnis serverErgebnis;
    std::thread server([&] {
        Einstellungen e;
        e.frist_ms = 10000;
        Rueckrufe rr;
        // Im Faden der Serversitzung (dem Besitzer der Leitung) schliessen: die Leitung bricht nach
        // etwa 20 KB weg, mitten im Datenstrom.
        rr.fortschritt = [&](const std::string&, int64_t b, int64_t) {
            if (b > 20000) p.b->schliessen();
        };
        Sitzung s(*p.b, e, rr);
        serverErgebnis = s.serve(freigabe);
    });
    schreibe(lokal.p / "gross.bin", zufall(100 * 1024, 5));
    Einstellungen e;
    e.frist_ms = 10000;
    Sitzung c(*p.a, e);
    const auto t0 = std::chrono::steady_clock::now();
    const Ergebnis r = c.senden((lokal.p / "gross.bin").u8string(), "gross.bin");
    server.join();
    EXPECT_FALSE(r.ok());
    EXPECT_EQ(r.art, Fehlerart::LeitungGetrennt) << r.text;
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::seconds(8)) << "Trennung wurde erst nach Fristen erkannt";
    EXPECT_FALSE(serverErgebnis.ok());
}

INSTANTIATE_TEST_SUITE_P(Leitungsarten, KermitNetzProtokoll, ::testing::ValuesIn(KOMBIS), kombiName);

// ─── Login-Skript gegen eine WEGA-getty-Attrappe ────────────────────────────

namespace {

/// Gerade Paritaet auf Bit 7, wie die getty nach einer Sitzung.
uint8_t gerade(uint8_t c) {
    c &= 0x7F;
    int n = 0;
    for (int i = 0; i < 7; ++i) n += (c >> i) & 1;
    return static_cast<uint8_t>(c | ((n & 1) ? 0x80 : 0));
}

struct GettyAttrappe {
    Leitung& l;
    int ignorierteReturns = 2;     ///< die ersten RETURNs verhallen (getty hatte ins Leere gesendet)
    std::string benutzer = "kermit", kennwort = "kermit";
    bool paritaet = true;
    bool ohneKennwortZeile = false;   ///< Benutzer ohne Kennwort: nach dem Namen gleich der Server
    int stilleMs = 10000;             ///< "Anmeldung + .kermrc" in (virtueller) Zeit
    bool dannKermitServer = false;
    Freigabe* freigabe = nullptr;
    bool stumm = false;

    std::string verlauf;   ///< was wir empfangen haben

    void aus(const std::string& s) {
        std::string t = s;
        for (auto& c : t) if (paritaet) c = static_cast<char>(gerade(static_cast<uint8_t>(c)));
        l.schreiben(reinterpret_cast<const uint8_t*>(t.data()), t.size());
    }
    // Liest eine Zeile (bis CR); false = Leitung weg.
    bool zeile(std::string& z) {
        z.clear();
        for (;;) {
            uint8_t b;
            const int n = l.lesen(&b, 1, 600000);
            if (n < 0) return false;
            if (n == 0) continue;
            verlauf.push_back(static_cast<char>(b));
            if (b == '\r') return true;
            z.push_back(static_cast<char>(b));
        }
    }
    void lauf() {
        if (stumm) {
            uint8_t b;
            while (l.lesen(&b, 1, 600000) >= 0) {}
            return;
        }
        std::string z;
        int returns = 0;
        for (;;) {
            // Auf RETURN warten, dann die Anmeldezeile ausgeben.
            if (!zeile(z)) return;
            if (++returns <= ignorierteReturns) continue;
            for (;;) {
                aus("\r\n\r\nWEGA login: ");
                if (!zeile(z)) return;
                if (z != benutzer) {
                    aus("\r\nLogin incorrect\r\n");
                    continue;
                }
                if (!ohneKennwortZeile) {
                    aus("\r\nPassword: ");
                    std::string k;
                    if (!zeile(k)) return;
                    if (k != kennwort) {
                        aus("\r\nLogin incorrect\r\n");
                        continue;
                    }
                }
                goto angemeldet;
            }
        }
    angemeldet:
        // Tagesmeldung (~1,9 KB mit ESC-Folgen), dann ca. 10 s Stille, dann der Kermit-Banner.
        std::string motd = "\r\n";
        while (motd.size() < 1900) motd += "\x1b[1;7mWEGA  WEGA  WEGA  WEGA\x1b[0m  Wissenschaftlich-Technisches Zentrum\r\n";
        aus(motd);
        uint8_t b;
        int64_t bis = l.jetztMs() + stilleMs;
        while (l.jetztMs() < bis) l.lesen(&b, 1, bis - l.jetztMs());   // virtuell: springt
        aus("C-Kermit 4.2(030) PRERELEASE # 2, 5 March 85, AT&T System III/System V\r\nType ? for help\r\n\r\n"
            "C-Kermit server starting.  Return to your local machine by typing its escape sequence\r\n"
            "for closing the connection, and issue further commands from there.  To shut down the\r\n"
            "C-Kermit server, issue the FINISH command and then reconnect.\n\r\n");
        if (!dannKermitServer) {
            // Der Server laeuft weiter: die Attrappe bleibt an der Leitung, bis sie geschlossen wird.
            while (l.lesen(&b, 1, 600000) >= 0) {}
            return;
        }
        if (freigabe) {
            Einstellungen e;
            e.capas_senden = false;
            e.langpakete = false;
            e.maxl = 94;
            e.frist_ms = 20000;
            Sitzung s(l, e);
            s.serve(*freigabe);
        }
    }
};

LoginOptionen schnell() {
    LoginOptionen o;
    o.frist_login_ms = 30000;
    o.return_abstand_ms = 1000;
    o.frist_kennwort_ms = 30000;
    o.frist_server_ms = 60000;
    return o;
}

}  // namespace

TEST(KermitLogin, VollerAblaufMitGeraderParitaetUndKermitDanach) {
    SpeicherLeitungPaar paar;   // virtuelle Zeit: die 10 s Stille kosten keine Echtzeit
    TempOrdner wurzel;
    schreibe(wurzel.p / "hallo.txt", "hallo welt\n");
    Freigabe freigabe(wurzel.p);
    GettyAttrappe g{paar.ende(1)};
    g.dannKermitServer = true;
    g.freigabe = &freigabe;
    std::thread t([&] { g.lauf(); });

    const LoginErgebnis r = loginSkript(paar.ende(0), schnell());
    ASSERT_TRUE(r.ok) << r.text;
    // Das Login-Skript hat die getty mit gerader Paritaet gelesen und Bit 7 geloescht.
    EXPECT_NE(r.gelesen.find("WEGA login:"), std::string::npos) << r.gelesen;
    EXPECT_NE(r.gelesen.find("server starting"), std::string::npos);
    EXPECT_GE(paar.jetzt(), 10000) << "die 10 s Stille muessen uebersprungen worden sein";
    // Die getty bekam RETURN (mehrfach), Name, Kennwort.
    EXPECT_NE(g.verlauf.find("kermit\rkermit\r"), std::string::npos) << g.verlauf;

    // Danach laeuft das Protokoll auf derselben Leitung.
    Einstellungen e;
    e.frist_ms = 20000;
    Sitzung c(paar.ende(0), e);
    Ergebnis v = c.entferntVerzeichnis("");
    ASSERT_TRUE(v.ok()) << v.text;
    EXPECT_NE(v.ausgabe.find("hallo.txt"), std::string::npos) << v.ausgabe;
    EXPECT_TRUE(c.entferntAbmelden().ok());
    paar.ende(0).schliessen();
    t.join();
}

TEST(KermitLogin, OhneParitaetUndOhneKennwortzeile) {
    SpeicherLeitungPaar paar;
    GettyAttrappe g{paar.ende(1)};
    g.paritaet = false;
    g.ignorierteReturns = 0;
    g.ohneKennwortZeile = true;
    g.stilleMs = 0;
    std::thread t([&] { g.lauf(); });
    LoginOptionen o = schnell();
    o.mit_kennwort = true;   // das Skript erkennt den Serverbeginn auch ohne Password:
    const LoginErgebnis r = loginSkript(paar.ende(0), o);
    EXPECT_TRUE(r.ok) << r.text;
    paar.ende(0).schliessen();
    t.join();
}

TEST(KermitLogin, FalschesKennwortGibtKlarenText) {
    SpeicherLeitungPaar paar;
    GettyAttrappe g{paar.ende(1)};
    g.ignorierteReturns = 0;
    std::thread t([&] { g.lauf(); });
    LoginOptionen o = schnell();
    o.kennwort = "falsch";
    const LoginErgebnis r = loginSkript(paar.ende(0), o);
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.text.find("Kennwort abgelehnt"), std::string::npos) << r.text;
    EXPECT_NE(r.text.find("Login incorrect"), std::string::npos) << "zuletzt Gelesenes fehlt: " << r.text;
    paar.ende(0).schliessen();
    t.join();
}

TEST(KermitLogin, FalscherNameGibtKlarenText) {
    SpeicherLeitungPaar paar;
    GettyAttrappe g{paar.ende(1)};
    g.ignorierteReturns = 0;
    std::thread t([&] { g.lauf(); });
    LoginOptionen o = schnell();
    o.name = "niemand";
    const LoginErgebnis r = loginSkript(paar.ende(0), o);
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.text.find("abgelehnt"), std::string::npos) << r.text;
    paar.ende(0).schliessen();
    t.join();
}

// ─── Anmeldung auf Abruf (K10) ──────────────────────────────────────────────

TEST(KermitLoginAbruf, RichtigeDatenGebenOkUndDerRueckrufKommtErstNachLogin) {
    SpeicherLeitungPaar paar;   // gerade Paritaet: Bit 7 wird vor dem Vergleich geloescht
    GettyAttrappe g{paar.ende(1)};
    g.stilleMs = 0;
    std::thread t([&] { g.lauf(); });
    LoginOptionen o = schnell();
    int aufrufe = 0;
    std::string gesehen;
    o.abfrage = [&](const AnmeldeAnfrage& a, std::string& n, std::string& k) {
        ++aufrufe;
        gesehen = a.gelesen;
        EXPECT_EQ(a.versuch, 0);
        n = "kermit";
        k = "kermit";
        return true;
    };
    const LoginErgebnis r = loginSkript(paar.ende(0), o);
    EXPECT_TRUE(r.ok) << r.text;
    EXPECT_EQ(r.art, LoginArt::Ok);
    EXPECT_EQ(aufrufe, 1);
    EXPECT_EQ(r.versuche, 1);
    EXPECT_NE(gesehen.find("login:"), std::string::npos) << "Rueckruf kam vor dem login:: " << gesehen;
    paar.ende(0).schliessen();
    t.join();
}

TEST(KermitLoginAbruf, FalscheDatenGebenAbgelehntDannErneutOkAufDerselbenLeitung) {
    for (const bool falscherName : {false, true}) {
        SpeicherLeitungPaar paar;
        GettyAttrappe g{paar.ende(1)};
        g.stilleMs = 0;
        std::thread t([&] { g.lauf(); });
        LoginOptionen o = schnell();
        std::vector<int> versuche;
        std::string zweitesGelesen;
        o.abfrage = [&](const AnmeldeAnfrage& a, std::string& n, std::string& k) {
            versuche.push_back(a.versuch);
            if (a.versuch == 0) {
                n = falscherName ? "niemand" : "kermit";
                k = "falsch";
            } else {
                zweitesGelesen = a.gelesen;
                n = "kermit";
                k = "kermit";
            }
            return true;
        };
        const LoginErgebnis r = loginSkript(paar.ende(0), o);
        EXPECT_TRUE(r.ok) << (falscherName ? "Name: " : "Kennwort: ") << r.text;
        EXPECT_EQ(r.art, LoginArt::Ok);
        EXPECT_EQ(versuche, (std::vector<int>{0, 1}));
        EXPECT_EQ(r.versuche, 2);
        EXPECT_NE(zweitesGelesen.find("Login incorrect"), std::string::npos) << zweitesGelesen;
        paar.ende(0).schliessen();
        t.join();
        // Zweiter Versuch auf derselben Leitung: das richtige Paar kam an.
        EXPECT_NE(g.verlauf.find("kermit\rkermit\r"), std::string::npos) << g.verlauf;
    }
}

TEST(KermitLoginAbruf, DieAnmeldungOhneRueckrufMeldetDieArtAbgelehnt) {
    SpeicherLeitungPaar paar;
    GettyAttrappe g{paar.ende(1)};
    g.ignorierteReturns = 0;
    std::thread t([&] { g.lauf(); });
    LoginOptionen o = schnell();
    o.kennwort = "falsch";
    const LoginErgebnis r = loginSkript(paar.ende(0), o);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.art, LoginArt::Abgelehnt) << r.text;
    paar.ende(0).schliessen();
    t.join();
}

TEST(KermitLoginAbruf, StummeGegenstelleGibtFristUndRuftNichtAuf) {
    SpeicherLeitungPaar paar;
    paar.setzeBeteiligt(1, false);
    LoginOptionen o = schnell();
    o.frist_login_ms = 5000;
    int aufrufe = 0;
    o.abfrage = [&](const AnmeldeAnfrage&, std::string&, std::string&) { ++aufrufe; return true; };
    const LoginErgebnis r = loginSkript(paar.ende(0), o);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.art, LoginArt::Frist) << r.text;
    EXPECT_EQ(aufrufe, 0);
}

TEST(KermitLoginAbruf, LeitungWaehrendDerEingabeGeschlossenGibtLeitung) {
    SpeicherLeitungPaar paar;
    GettyAttrappe g{paar.ende(1)};
    g.ignorierteReturns = 0;
    std::thread t([&] { g.lauf(); });
    LoginOptionen o = schnell();
    o.abfrage = [&](const AnmeldeAnfrage&, std::string& n, std::string& k) {
        paar.ende(1).schliessen();   // die Gegenstelle legt auf, waehrend der Anwender tippt
        n = "kermit";
        k = "kermit";
        return true;
    };
    const LoginErgebnis r = loginSkript(paar.ende(0), o);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.art, LoginArt::Leitung) << r.text;
    paar.ende(0).schliessen();
    t.join();
}

TEST(KermitLoginAbruf, AbbruchWaehrendDerEingabe) {
    // (1) der Anwender bricht ab (Rueckruf gibt false), (2) das Abbruch-Flag wird waehrend der Eingabe gesetzt.
    for (const bool ueberFlag : {false, true}) {
        SpeicherLeitungPaar paar;
        GettyAttrappe g{paar.ende(1)};
        g.ignorierteReturns = 0;
        std::thread t([&] { g.lauf(); });
        std::atomic<bool> stop{false};
        LoginOptionen o = schnell();
        o.abbruch = &stop;
        o.abfrage = [&](const AnmeldeAnfrage&, std::string& n, std::string& k) {
            if (ueberFlag) {
                stop = true;
                n = "kermit";
                k = "kermit";
                return true;
            }
            return false;
        };
        const LoginErgebnis r = loginSkript(paar.ende(0), o);
        EXPECT_FALSE(r.ok);
        EXPECT_EQ(r.art, LoginArt::Abbruch) << r.text;
        EXPECT_EQ(r.versuche, 0) << "nach dem Abbruch darf nichts mehr gesendet worden sein";
        paar.ende(0).schliessen();
        t.join();
    }
}

TEST(KermitLogin, StummeGegenstelleLaeuftInDieFrist) {
    SpeicherLeitungPaar paar;
    paar.setzeBeteiligt(1, false);
    LoginOptionen o = schnell();
    o.frist_login_ms = 5000;
    const LoginErgebnis r = loginSkript(paar.ende(0), o);
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.text.find("kein \"login:\""), std::string::npos) << r.text;
}

TEST(KermitLogin, GetrenntUndAbbruch) {
    {
        SpeicherLeitungPaar paar;
        paar.ende(1).schliessen();
        const LoginErgebnis r = loginSkript(paar.ende(0), schnell());
        EXPECT_FALSE(r.ok);
        EXPECT_NE(r.text.find("getrennt"), std::string::npos) << r.text;
    }
    {
        SpeicherLeitungPaar paar(false);   // Echtzeit
        paar.setzeBeteiligt(1, false);
        std::atomic<bool> stop{false};
        LoginOptionen o = schnell();
        o.abbruch = &stop;
        o.frist_login_ms = 60000;
        o.return_abstand_ms = 60000;
        std::thread t([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            stop = true;
            paar.ende(0).wecken();
        });
        const auto t0 = std::chrono::steady_clock::now();
        const LoginErgebnis r = loginSkript(paar.ende(0), o);
        t.join();
        EXPECT_FALSE(r.ok);
        EXPECT_NE(r.text.find("abgebrochen"), std::string::npos) << r.text;
        EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::seconds(5));
    }
}

TEST(KermitLogin, SichtbarMachen) {
    EXPECT_EQ(sichtbarMachen(std::string("\xD7\xC5GA\xA0lo\xE7i\xEE:")), "WEGA login:");
    EXPECT_EQ(sichtbarMachen("a\r\n\x1b" "b"), "a\\r\\n<1B>b");
    EXPECT_EQ(sichtbarMachen(std::string(500, 'x'), 10), "..." + std::string(10, 'x'));
}

// ─── Gegenprobe: ein echter SerialHub-Kanal des Emulators ───────────────────

namespace {

using k1520::serial::Betriebsart;
using k1520::serial::Rolle;
using k1520::serial::SerialHub;
using k1520::serial::SerialKonfig;
using k1520::serial::Zustand;

struct HubSeite {
    k1520test::AttrappeAnschluss a;
    SerialHub hub;
    HubSeite() { hub.registriere(a); }
};

/// Der "Gast" des Hub-Kanals: eine Leitung, die die Maschinenzeit selbst taktet (im Faden der
/// Gast-Sitzung) und Bytes aus/in die Attrappe schiebt — so kann ein Kermit-SERVER als Gast laufen.
struct GastLeitung : Leitung {
    HubSeite& s;
    uint64_t z = 0;
    std::size_t gelesen = 0;
    bool spur = std::getenv("K1520_KERMIT_SPUR") != nullptr;   // Fehlersuche: Bytes des Gastes auf stderr
    explicit GastLeitung(HubSeite& h) : s(h) {}

    void takte() {
        for (int i = 0; i < 100; ++i) {
            z += 64;
            s.hub.takt(z);
            s.a.lies();
        }
    }
    int lesen(uint8_t* buf, std::size_t n, int64_t fristMs) override {
        const int64_t ende = jetztMs() + fristMs;
        for (;;) {
            if (weck_.exchange(false)) return 0;
            if (gelesen < s.a.gelesen.size()) {
                const std::size_t k = std::min(n, s.a.gelesen.size() - gelesen);
                for (std::size_t i = 0; i < k; ++i) buf[i] = s.a.gelesen[gelesen + i];
                if (spur) { std::fprintf(stderr, "[gast rx %zu]", k); for (std::size_t i = 0; i < k; ++i) std::fprintf(stderr, " %02X", buf[i]); std::fprintf(stderr, "\n"); }
                gelesen += k;
                return static_cast<int>(k);
            }
            if (jetztMs() >= ende) return 0;
            takte();
            std::this_thread::yield();
        }
    }
    bool schreiben(const uint8_t* buf, std::size_t n) override {
        if (spur) { std::fprintf(stderr, "[%lld ms gast tx %zu]", static_cast<long long>(jetztMs() % 1000000), n); for (std::size_t i = 0; i < n; ++i) std::fprintf(stderr, " %02X", buf[i]); std::fprintf(stderr, "\n"); }
        s.a.sende(std::vector<uint8_t>(buf, buf + n));
        return true;
    }
    void schliessen() override {}
    void wecken() override { weck_ = true; }
    std::atomic<bool> weck_{false};
};

}  // namespace

TEST(KermitNetzHub, KermitGegenEinenEchtenSerialHubKanal) {
    // Der Hub ist Telnet-SERVER auf Port 0 (der Emulator als Gegenstelle), die Leitung hier verbindet
    // als Client.  Telnet-BINARY wird gegen den Hub ausgehandelt; die Nutzbytes laufen durch Wandler
    // (9600 Bd, Maschinenzeit), durch das 3-stufige SIO-FIFO der Attrappe — und kommen bitgleich an.
    HubSeite seite;
    SerialKonfig k;
    k.betriebsart = Betriebsart::Telnet;
    k.rolle = Rolle::Server;
    k.port = 0;
    ASSERT_TRUE(seite.hub.konfigurieren(0, k));
    ASSERT_TRUE(seite.hub.start(0));
    const int port = seite.hub.status(0).port_aktiv;
    ASSERT_GT(port, 0);

    TempOrdner wurzel, lokal, hol;
    Freigabe freigabe(wurzel.p);
    GastLeitung gast(seite);
    Ergebnis gastErgebnis;
    std::thread gastFaden([&] {
        // Erst die Maschine laufen lassen, bis die Verbindung steht, dann Kermit-Server (wie C-Kermit 4.2).
        const auto ende = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (seite.hub.status(0).zustand != Zustand::Verbunden && std::chrono::steady_clock::now() < ende) {
            gast.takte();
            std::this_thread::yield();
        }
        Einstellungen e;
        e.capas_senden = false;
        e.langpakete = false;
        e.attribute = false;
        e.maxl = 94;
        e.pruefsumme = 1;
        e.frist_ms = 20000;
        Sitzung s(gast, e);
        gastErgebnis = s.serve(freigabe);
        // Die Schlussquittung (G L) liegt noch im Sendepuffer des Gastes: weitertakten, bis sie draussen ist.
        const auto bis = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while ((!seite.a.gastSendet.empty() || seite.hub.status(0).puffer_senden > 0) &&
               std::chrono::steady_clock::now() < bis) {
            gast.takte();
            std::this_thread::yield();
        }
        for (int i = 0; i < 200; ++i) gast.takte();
    });

    std::string f;
    auto leitung = TelnetLeitung::verbinden("127.0.0.1", port, 5000, {}, &f);
    ASSERT_TRUE(leitung) << f;
    EXPECT_TRUE(leitung->binaerAktiv()) << "Hub und Leitung haben BINARY nicht ausgehandelt: " << leitung->hinweis();

    // Alle Bytewerte, darunter IAC, CR NUL, CR LF: 6 KB reichen (6 KB bei 9600 Bd = 7 s Maschinenzeit).
    std::string inhalt;
    for (int i = 0; i < 256; ++i) inhalt.push_back(static_cast<char>(i));
    inhalt += std::string("\r\0\r\n\xFF\xFF", 6) + zufall(6000, 3);
    schreibe(lokal.p / "daten.bin", inhalt);

    Einstellungen e;
    e.frist_ms = 20000;
    Sitzung c(*leitung, e);
    Ergebnis r = c.senden((lokal.p / "daten.bin").u8string(), "daten.bin");
    ASSERT_TRUE(r.ok()) << r.text;
    EXPECT_EQ(lies(wurzel.p / "daten.bin"), inhalt);
    r = c.holen("daten.bin", hol.s());
    ASSERT_TRUE(r.ok()) << r.text;
    EXPECT_EQ(lies(hol.p / "daten.bin"), inhalt);
    r = c.entferntAbmelden();
    EXPECT_TRUE(r.ok()) << r.text;
    gastFaden.join();
    EXPECT_TRUE(gastErgebnis.ok()) << gastErgebnis.text;
    EXPECT_EQ(seite.a.ueberlauf, 0u);
    seite.hub.stop(0);
}
