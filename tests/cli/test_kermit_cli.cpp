/**
 * @file test_kermit_cli.cpp
 * @brief Kommandozeile `k1520kermit` als PROZESS (doc/design/36_kermit.md §4.5, AP K5).
 *
 * Je Fall laufen zwei echte Prozesse der CLI gegeneinander ueber Loopback — `serve` und ein
 * Client-Kommando (`put/get/ls/rm/cd/finish`) — in beiden Leitungsarten (Telnet, RFC 2217) und
 * in beiden Rollen der Leitung (der Server lauscht ODER der Client lauscht).  Nie feste Ports:
 * die Seite, die lauscht, bekommt Port 0 und meldet `Port: N` auf stderr; der Test liest ihn von dort.
 * Ein lauschender Prozess nimmt EINE Verbindung an, deshalb ist jedes Kommando ein eigenes Paar.
 *
 * Geprueft werden ausserdem: Binaerdatei 100 KB, Namensfallen, Rueckgabewerte 0/1/2,
 * Ausgabekonvention (stdout = nur Nutzlast, `--json` = gueltiges JSON), Login gegen eine
 * getty-Attrappe und (nur POSIX) Strg+C.
 *
 * Prozesse starten: `std::system` mit Umleitung in Dateien (laeuft unter sh wie unter cmd);
 * ein Prozess im Hintergrund = ein `std::thread`, der `std::system` ruft.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "kermit/src/dateien.h"
#include "kermit/src/leitung_netz.h"
#include "kermit/src/protokoll.h"

#ifndef _WIN32
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifndef K1520KERMIT_EXE
#error "K1520KERMIT_EXE fehlt (CMake: DEFS)"
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

// ─── Hilfen ──────────────────────────────────────────────────────────────────

fs::path s_wurzel;   ///< Arbeitsordner dieses Testlaufs

struct TempOrdner {
    fs::path p;
    TempOrdner() {
        std::random_device rd;
        p = fs::temp_directory_path() / ("k1520kermit_cli_" + std::to_string(rd()) + std::to_string(rd()));
        fs::create_directories(p);
    }
    ~TempOrdner() {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
    fs::path operator/(const std::string& n) const { return p / n; }
    std::string s() const { return p.string(); }
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
    std::ostringstream o;
    o << f.rdbuf();
    return o.str();
}

std::string q(const std::string& a) { return "\"" + a + "\""; }

struct Lauf {
    int rc = -1;
    std::string out, err;
};

std::atomic<int> g_zaehler{0};

/// Fuehrt die CLI mit Argumenten aus (blockierend).  `vorspann` = Shell-Text vor dem Aufruf (POSIX).
Lauf starte(const std::vector<std::string>& args, const std::string& vorspann = "", int nummer = 0) {
    const int nr = nummer > 0 ? nummer : ++g_zaehler;
    const fs::path o = s_wurzel / ("out" + std::to_string(nr) + ".txt");
    const fs::path e = s_wurzel / ("err" + std::to_string(nr) + ".txt");
    std::string cmd = vorspann + q(K1520KERMIT_EXE);
    for (const auto& a : args) cmd += " " + q(a);
    cmd += " > " + q(o.string()) + " 2> " + q(e.string());
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";   // cmd /c entfernt die aeusseren Anfuehrungszeichen
#endif
    const int st = std::system(cmd.c_str());
    Lauf l;
#ifdef _WIN32
    l.rc = st;
#else
    l.rc = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
#endif
    l.out = lies(o);
    l.err = lies(e);
    return l;
}

/// Prozess im Hintergrund: Faden, der die CLI blockierend ruft.
struct Hintergrund {
    std::future<Lauf> ergebnis;
    fs::path errDatei;
    explicit Hintergrund(const std::vector<std::string>& args, const std::string& vorspann = "") {
        const int nr = ++g_zaehler;   // die Nummer steht vor dem Start fest (Name der stderr-Datei)
        errDatei = s_wurzel / ("err" + std::to_string(nr) + ".txt");
        ergebnis = std::async(std::launch::async, [args, vorspann, nr] { return starte(args, vorspann, nr); });
    }
    /// Wartet, bis der Prozess `Port: N` gemeldet hat.
    int port() const {
        const auto ende = std::chrono::steady_clock::now() + 20s;
        while (std::chrono::steady_clock::now() < ende) {
            const std::string t = lies(errDatei);
            const auto k = t.find("Port: ");
            if (k != std::string::npos && t.find('\n', k) != std::string::npos)
                return std::atoi(t.c_str() + k + 6);
            std::this_thread::sleep_for(20ms);
        }
        return 0;
    }
    Lauf warten() { return ergebnis.get(); }
};

enum class Art { Telnet, Rfc2217 };
enum class Lauscher { Server, Client };   ///< wer an der Leitung lauscht

struct Paar {
    Lauf server, client;
};

std::string leitungsschalter(Art a, bool lauschen) {
    const std::string n = a == Art::Telnet ? "telnet" : "rfc2217";
    return lauschen ? "--listen-" + n : "--" + n;
}

/// Startet `serve` und ein Client-Kommando gegeneinander.  `clientArgs` = Kommando und Argumente
/// OHNE die Leitung; `serveArgs` = zusaetzliche Schalter fuer den Server (z. B. --nur-lesen).
Paar paar(Art art, Lauscher lauscher, const fs::path& ordner, const std::vector<std::string>& clientArgs,
          const std::vector<std::string>& serveArgs = {}) {
    Paar r;
    std::vector<std::string> sa = {"serve"};
    std::vector<std::string> ca = clientArgs;
    for (const auto& a : serveArgs) sa.push_back(a);
    if (lauscher == Lauscher::Server) {
        sa.push_back(leitungsschalter(art, true));
        sa.push_back("0");
        sa.push_back(ordner.string());
        Hintergrund s(sa);
        const int port = s.port();
        EXPECT_GT(port, 0) << "Server meldet keinen Port";
        ca.push_back(leitungsschalter(art, false));
        ca.push_back("127.0.0.1:" + std::to_string(port));
        r.client = starte(ca);
        r.server = s.warten();
    } else {
        ca.push_back(leitungsschalter(art, true));
        ca.push_back("0");
        Hintergrund c(ca);
        const int port = c.port();
        EXPECT_GT(port, 0) << "Client meldet keinen Port";
        sa.push_back(leitungsschalter(art, false));
        sa.push_back("127.0.0.1:" + std::to_string(port));
        sa.push_back(ordner.string());
        r.server = starte(sa);
        r.client = c.warten();
    }
    return r;
}

std::vector<std::string> zeilen(const std::string& t) {
    std::vector<std::string> v;
    std::istringstream is(t);
    std::string z;
    while (std::getline(is, z)) {
        if (!z.empty() && z.back() == '\r') z.pop_back();
        if (!z.empty()) v.push_back(z);
    }
    return v;
}

// Kleiner JSON-Pruefer (Syntax, keine Auswertung): Objekt/Array/String/Zahl/true/false/null.
struct Json {
    const std::string& s;
    std::size_t i = 0;
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i; }
    bool str() {
        if (i >= s.size() || s[i] != '"') return false;
        for (++i; i < s.size(); ++i) {
            if (s[i] == '\\') { ++i; continue; }
            if (s[i] == '"') { ++i; return true; }
            if (static_cast<unsigned char>(s[i]) < 0x20) return false;
        }
        return false;
    }
    bool wert() {
        ws();
        if (i >= s.size()) return false;
        const char c = s[i];
        if (c == '"') return str();
        if (c == '{') {
            ++i; ws();
            if (i < s.size() && s[i] == '}') { ++i; return true; }
            for (;;) {
                ws();
                if (!str()) return false;
                ws();
                if (i >= s.size() || s[i++] != ':') return false;
                if (!wert()) return false;
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == '}') { ++i; return true; }
                return false;
            }
        }
        if (c == '[') {
            ++i; ws();
            if (i < s.size() && s[i] == ']') { ++i; return true; }
            for (;;) {
                if (!wert()) return false;
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == ']') { ++i; return true; }
                return false;
            }
        }
        for (const char* lit : {"true", "false", "null"})
            if (s.compare(i, std::string(lit).size(), lit) == 0) { i += std::string(lit).size(); return true; }
        const std::size_t a = i;
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '+' ||
                                s[i] == '.' || s[i] == 'e' || s[i] == 'E'))
            ++i;
        return i > a;
    }
    bool gueltig() {
        if (!wert()) return false;
        ws();
        return i == s.size();
    }
};
bool jsonGueltig(const std::string& t) {
    Json j{t};
    return j.gueltig();
}

class KermitCli : public ::testing::TestWithParam<std::tuple<Art, Lauscher>> {
protected:
    TempOrdner tmp;
    void SetUp() override { s_wurzel = tmp.p; }
    Art art() const { return std::get<0>(GetParam()); }
    Lauscher lauscher() const { return std::get<1>(GetParam()); }
};

}  // namespace

// ─── Uebertragungen ──────────────────────────────────────────────────────────

TEST_P(KermitCli, PutBinaerdatei100KBLandetBytegleichImOrdner) {
    fs::create_directories(tmp / "frei");
    const std::string daten = zufall(100 * 1024, 7);
    schreibe(tmp / "gross.bin", daten);
    const Paar p = paar(art(), lauscher(), tmp / "frei", {"put", (tmp / "gross.bin").string()});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_EQ(p.server.rc, 0) << p.server.err;
    EXPECT_EQ(lies(tmp / "frei" / "gross.bin"), daten);
    // Nutzlast: nur der gesendete lokale Pfad; alles andere auf stderr.
    EXPECT_EQ(zeilen(p.client.out), std::vector<std::string>{(tmp / "gross.bin").string()});
    EXPECT_NE(p.server.err.find("Empfangen: gross.bin"), std::string::npos) << p.server.err;
    EXPECT_TRUE(p.server.out.empty()) << p.server.out;
}

TEST_P(KermitCli, GetBinaerdatei100KBKommtBytegleichAn) {
    fs::create_directories(tmp / "frei");
    fs::create_directories(tmp / "ziel");
    const std::string daten = zufall(100 * 1024, 8);
    schreibe(tmp / "frei" / "daten.bin", daten);
    const Paar p = paar(art(), lauscher(), tmp / "frei", {"get", "daten.bin", "--to", (tmp / "ziel").string()});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_EQ(lies(tmp / "ziel" / "daten.bin"), daten);
    EXPECT_EQ(zeilen(p.client.out), std::vector<std::string>{(tmp / "ziel" / "daten.bin").string()});
    // Die Quelle bleibt unveraendert.
    EXPECT_EQ(lies(tmp / "frei" / "daten.bin"), daten);
}

TEST_P(KermitCli, PutMehrereDateienUndAsName) {
    fs::create_directories(tmp / "frei");
    schreibe(tmp / "a.txt", "alpha");
    schreibe(tmp / "b.txt", "beta");
    Paar p = paar(art(), lauscher(), tmp / "frei", {"put", (tmp / "a.txt").string(), (tmp / "b.txt").string()});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_EQ(lies(tmp / "frei" / "a.txt"), "alpha");
    EXPECT_EQ(lies(tmp / "frei" / "b.txt"), "beta");
    EXPECT_EQ(zeilen(p.client.out).size(), 2u);
    p = paar(art(), lauscher(), tmp / "frei", {"put", (tmp / "a.txt").string(), "--as", "neu.dat"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_EQ(lies(tmp / "frei" / "neu.dat"), "alpha");
}

TEST_P(KermitCli, LsNamenLangUndJson) {
    fs::create_directories(tmp / "frei" / "unter");
    schreibe(tmp / "frei" / "eins.txt", "12345");
    schreibe(tmp / "frei" / "zwei.bin", std::string(300, 'x'));

    // ohne -l: nur die Namen, einer je Zeile
    Paar p = paar(art(), lauscher(), tmp / "frei", {"ls"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    auto namen = zeilen(p.client.out);
    std::sort(namen.begin(), namen.end());
    EXPECT_EQ(namen, (std::vector<std::string>{"eins.txt", "unter", "zwei.bin"})) << p.client.out;

    // -l: die Zeilen des Servers (ls -l)
    p = paar(art(), lauscher(), tmp / "frei", {"ls", "-l"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_NE(p.client.out.find("eins.txt"), std::string::npos);
    EXPECT_NE(p.client.out.find("300"), std::string::npos) << p.client.out;
    EXPECT_NE(p.client.out.find("rw"), std::string::npos) << p.client.out;

    // --json: zerlegte Eintraege, gueltiges JSON, nichts anderes auf stdout
    p = paar(art(), lauscher(), tmp / "frei", {"ls", "--json"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_TRUE(jsonGueltig(p.client.out)) << p.client.out;
    EXPECT_EQ(p.client.out.front(), '[');
    EXPECT_NE(p.client.out.find("\"name\":\"zwei.bin\""), std::string::npos) << p.client.out;
    EXPECT_NE(p.client.out.find("\"groesse\":300"), std::string::npos) << p.client.out;
    EXPECT_NE(p.client.out.find("\"art\":\"d\""), std::string::npos) << p.client.out;
}

TEST_P(KermitCli, LsMitPfadUndCdSchalter) {
    fs::create_directories(tmp / "frei" / "unter");
    schreibe(tmp / "frei" / "unter" / "tief.txt", "t");
    Paar p = paar(art(), lauscher(), tmp / "frei", {"ls", "unter"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_EQ(zeilen(p.client.out), std::vector<std::string>{"tief.txt"}) << p.client.out;
    p = paar(art(), lauscher(), tmp / "frei", {"ls", "--cd", "unter"});
    EXPECT_EQ(zeilen(p.client.out), std::vector<std::string>{"tief.txt"}) << p.client.out;
    p = paar(art(), lauscher(), tmp / "frei", {"cd", "unter"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    p = paar(art(), lauscher(), tmp / "frei", {"cd", "gibtsnicht"});
    EXPECT_EQ(p.client.rc, 1) << p.client.err;
    // --tempo (K7) wird angenommen und aendert am Ergebnis nichts
    p = paar(art(), lauscher(), tmp / "frei", {"ls", "unter", "--tempo", "10"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_EQ(zeilen(p.client.out), std::vector<std::string>{"tief.txt"}) << p.client.out;
    p = paar(art(), lauscher(), tmp / "frei", {"ls", "unter", "--tempo", "auto"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
}

TEST_P(KermitCli, RmLoeschtUndMeldetFehlendes) {
    fs::create_directories(tmp / "frei");
    schreibe(tmp / "frei" / "weg.txt", "x");
    schreibe(tmp / "frei" / "bleibt.txt", "y");
    Paar p = paar(art(), lauscher(), tmp / "frei", {"rm", "weg.txt"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_FALSE(fs::exists(tmp / "frei" / "weg.txt"));
    EXPECT_TRUE(fs::exists(tmp / "frei" / "bleibt.txt"));
    p = paar(art(), lauscher(), tmp / "frei", {"rm", "weg.txt"});
    EXPECT_EQ(p.client.rc, 1) << "zweimal loeschen";
}

TEST_P(KermitCli, FinishUndLogoutBeendenDenServer) {
    fs::create_directories(tmp / "frei");
    for (const char* k : {"finish", "logout"}) {
        const Paar p = paar(art(), lauscher(), tmp / "frei", {k});
        EXPECT_EQ(p.client.rc, 0) << k << ": " << p.client.err;
        EXPECT_EQ(p.server.rc, 0) << k << ": " << p.server.err;
        EXPECT_NE(p.server.err.find("Server beendet"), std::string::npos) << p.server.err;
    }
}

TEST_P(KermitCli, JsonBeiPutGetUndFehler) {
    fs::create_directories(tmp / "frei");
    schreibe(tmp / "a.txt", "alpha");
    Paar p = paar(art(), lauscher(), tmp / "frei", {"put", (tmp / "a.txt").string(), "--json"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
    EXPECT_TRUE(jsonGueltig(p.client.out)) << p.client.out;
    EXPECT_NE(p.client.out.find("\"ok\":true"), std::string::npos);
    p = paar(art(), lauscher(), tmp / "frei", {"get", "gibtsnicht", "--to", tmp.s(), "--json"});
    EXPECT_EQ(p.client.rc, 1) << p.client.err;
    EXPECT_TRUE(jsonGueltig(p.client.out)) << p.client.out;
    EXPECT_NE(p.client.out.find("\"ok\":false"), std::string::npos) << p.client.out;
}

// ─── Namensfallen und Abweisung ─────────────────────────────────────────────

TEST_P(KermitCli, NamensfallenWerdenAbgewiesenUndNichtAusserhalbAbgelegt) {
    fs::create_directories(tmp / "frei");
    schreibe(tmp / "a.txt", "alpha");
    schreibe(tmp / "geheim.txt", "nicht herausgeben");
    // put unter einem Namen, der aus der Freigabe hinausfuehrt
    Paar p = paar(art(), lauscher(), tmp / "frei", {"put", (tmp / "a.txt").string(), "--as", "../x"});
    EXPECT_EQ(p.client.rc, 1) << p.client.err << p.client.out;
    EXPECT_FALSE(fs::exists(tmp / "x"));
    EXPECT_FALSE(fs::exists(tmp / "frei" / "x"));
    // get auf eine Datei ausserhalb
    fs::create_directories(tmp / "ziel");
    p = paar(art(), lauscher(), tmp / "frei", {"get", "../geheim.txt", "--to", (tmp / "ziel").string()});
    EXPECT_EQ(p.client.rc, 1) << p.client.err;
    EXPECT_FALSE(fs::exists(tmp / "ziel" / "geheim.txt"));
    // absoluter Pfad
    p = paar(art(), lauscher(), tmp / "frei", {"get", (tmp / "geheim.txt").string(), "--to", (tmp / "ziel").string()});
    EXPECT_EQ(p.client.rc, 1) << p.client.err;
    EXPECT_TRUE(fs::is_empty(tmp / "ziel"));
}

TEST_P(KermitCli, NurLesenWeistSchreibenUndLoeschenAb) {
    fs::create_directories(tmp / "frei");
    schreibe(tmp / "frei" / "da.txt", "da");
    schreibe(tmp / "a.txt", "alpha");
    Paar p = paar(art(), lauscher(), tmp / "frei", {"put", (tmp / "a.txt").string()}, {"--nur-lesen"});
    EXPECT_EQ(p.client.rc, 1) << p.client.err;
    EXPECT_FALSE(fs::exists(tmp / "frei" / "a.txt"));
    p = paar(art(), lauscher(), tmp / "frei", {"rm", "da.txt"}, {"--nur-lesen"});
    EXPECT_EQ(p.client.rc, 1) << p.client.err;
    EXPECT_TRUE(fs::exists(tmp / "frei" / "da.txt"));
    // Lesen geht weiter
    p = paar(art(), lauscher(), tmp / "frei", {"ls"}, {"--nur-lesen"});
    EXPECT_EQ(p.client.rc, 0) << p.client.err;
}

TEST_P(KermitCli, ZuLangerZielnameWirdAbgelehntNichtGekuerzt) {
    fs::create_directories(tmp / "frei");
    schreibe(tmp / "a.txt", "alpha");
    const Paar p = paar(art(), lauscher(), tmp / "frei",
                        {"put", (tmp / "a.txt").string(), "--as", "einsehrlangerdateiname.txt"}, {"--max-namen", "14"});
    EXPECT_EQ(p.client.rc, 1) << p.client.err;
    std::size_t n = 0;
    for (const auto& e : fs::directory_iterator(tmp / "frei")) { (void)e; ++n; }
    EXPECT_EQ(n, 0u);
}

INSTANTIATE_TEST_SUITE_P(Leitungen, KermitCli,
    ::testing::Combine(::testing::Values(Art::Telnet, Art::Rfc2217),
                       ::testing::Values(Lauscher::Server, Lauscher::Client)),
    [](const ::testing::TestParamInfo<std::tuple<Art, Lauscher>>& i) {
        return std::string(std::get<0>(i.param) == Art::Telnet ? "Telnet" : "Rfc2217") +
               (std::get<1>(i.param) == Lauscher::Server ? "_ServerLauscht" : "_ClientLauscht");
    });

// ─── Aufruf- und Verbindungsfehler (Rueckgabe 2) ─────────────────────────────

namespace {
/// Ein Port, auf dem sicher niemand lauscht: kurz lauschen, Port merken, schliessen.
int freierPort() {
    auto l = k1520::kermit::TelnetLeitung::lauschen(0);
    const int p = l ? l->port() : 0;
    return p;
}
}  // namespace

TEST(KermitCliFehler, KeinServerErreichbarGibtZwei) {
    TempOrdner tmp;
    s_wurzel = tmp.p;
    const int port = freierPort();
    ASSERT_GT(port, 0);
    for (const char* art : {"--telnet", "--rfc2217"}) {
        const Lauf l = starte({"ls", art, "127.0.0.1:" + std::to_string(port), "--frist", "2"});
        EXPECT_EQ(l.rc, 2) << art << ": " << l.err;
        EXPECT_NE(l.err.find("Fehler:"), std::string::npos);
        EXPECT_TRUE(l.out.empty()) << l.out;
    }
    // mit --json steht der Fehler auch als gueltiges JSON auf stdout
    const Lauf j = starte({"ls", "--telnet", "127.0.0.1:" + std::to_string(port), "--frist", "2", "--json"});
    EXPECT_EQ(j.rc, 2);
    EXPECT_TRUE(jsonGueltig(j.out)) << j.out;
    EXPECT_NE(j.out.find("\"ok\":false"), std::string::npos);
}

TEST(KermitCliFehler, AufrufFehlerGebenZwei) {
    TempOrdner tmp;
    s_wurzel = tmp.p;
    schreibe(tmp / "a.txt", "a");
    schreibe(tmp / "b.txt", "b");
    const std::string lt = "127.0.0.1:9";   // wird nie erreicht: jeder Fall scheitert vorher
    EXPECT_EQ(starte({}).rc, 2);                                                            // kein Kommando
    EXPECT_EQ(starte({"tanzen", "--telnet", lt}).rc, 2);                                    // unbekanntes Kommando
    EXPECT_EQ(starte({"ls"}).rc, 2);                                                        // keine Leitung
    EXPECT_EQ(starte({"ls", "--telnet", lt, "--rfc2217", lt}).rc, 2);                       // zwei Leitungen
    EXPECT_EQ(starte({"ls", "--telnet", "ohneport"}).rc, 2);                                // HOST:PORT
    EXPECT_EQ(starte({"ls", "--telnet", lt, "--gibtsnicht"}).rc, 2);                        // Schalter
    EXPECT_EQ(starte({"ls", "--telnet", lt, "--pruefsumme", "7"}).rc, 2);
    EXPECT_EQ(starte({"ls", "--telnet", lt, "--namen", "komisch"}).rc, 2);
    EXPECT_EQ(starte({"ls", "--telnet", lt, "--tempo", "schnell"}).rc, 2);                  // K7: --tempo auto|Zahl
    EXPECT_EQ(starte({"ls", "--telnet", lt, "--tempo", "0"}).rc, 2);
    EXPECT_EQ(starte({"ls", "--telnet", lt, "--tempo", "5000"}).rc, 2);
    EXPECT_EQ(starte({"put", "--telnet", lt, (tmp / "fehlt.txt").string()}).rc, 2);         // lokale Datei fehlt
    EXPECT_EQ(starte({"put", "--telnet", lt, (tmp / "a.txt").string(), (tmp / "b.txt").string(), "--as", "x"}).rc, 2);
    EXPECT_EQ(starte({"serve", "--telnet", lt, (tmp / "gibtsnicht").string()}).rc, 2);      // kein Ordner
    EXPECT_EQ(starte({"rm", "--telnet", lt}).rc, 2);                                        // Argument fehlt
    EXPECT_EQ(starte({"--help"}).rc, 0);
}

TEST(KermitCliFehler, ServerWeistDateiAbGibtEins) {
    // Der Server verweigert (nur lesen): der Client meldet 1 — nicht 2, die Verbindung stand.
    TempOrdner tmp;
    s_wurzel = tmp.p;
    fs::create_directories(tmp / "frei");
    schreibe(tmp / "a.txt", "a");
    const Paar p = paar(Art::Telnet, Lauscher::Server, tmp / "frei", {"put", (tmp / "a.txt").string()}, {"--nur-lesen"});
    EXPECT_EQ(p.client.rc, 1) << p.client.err;
    EXPECT_NE(p.client.err.find("Fehler:"), std::string::npos);
}

// ─── Anmeldung gegen eine getty-Attrappe ─────────────────────────────────────

TEST(KermitCliLogin, MeldetSichAnUndSprichtDanachKermit) {
    using namespace k1520::kermit;
    TempOrdner tmp;
    s_wurzel = tmp.p;
    fs::create_directories(tmp / "frei");
    schreibe(tmp / "frei" / "ding.txt", "inhalt");

    auto lausch = TelnetLeitung::lauschen(0);
    ASSERT_TRUE(lausch);
    const int port = lausch->port();
    std::atomic<bool> loginOk{false};
    std::thread attrappe([&] {
        if (!lausch->annehmen(20000)) return;
        auto sende = [&](const std::string& t) { lausch->schreiben(reinterpret_cast<const uint8_t*>(t.data()), t.size()); };
        // Liest Zeilen (vorangehende leere RETURNs des Login-Skripts werden uebergangen),
        // bis eine den erwarteten Text traegt.
        auto zeile = [&](const std::string& erwartet) {
            std::string z;
            for (int i = 0; i < 1000; ++i) {
                uint8_t b;
                if (lausch->lesen(&b, 1, 50) != 1) continue;
                if (b == '\r') {
                    if (z.find(erwartet) != std::string::npos) return true;
                    z.clear();
                } else {
                    z += static_cast<char>(b);
                }
            }
            return false;
        };
        sende("WEGA login: ");
        if (!zeile("kermit")) return;
        sende("Password: ");
        if (!zeile("geheim")) return;
        loginOk = true;
        sende("\r\nkermit: server starting\r\n");
        Freigabe fr(tmp / "frei");
        Sitzung s(*lausch);
        s.serve(fr);
    });
    const Lauf l = starte({"ls", "--telnet", "127.0.0.1:" + std::to_string(port), "--login", "kermit:geheim"});
    attrappe.join();
    EXPECT_TRUE(loginOk);
    EXPECT_EQ(l.rc, 0) << l.err;
    EXPECT_EQ(zeilen(l.out), std::vector<std::string>{"ding.txt"}) << l.out;
}

TEST(KermitCliLogin, FalscheAnmeldungGibtZwei) {
    using namespace k1520::kermit;
    TempOrdner tmp;
    s_wurzel = tmp.p;
    auto lausch = TelnetLeitung::lauschen(0);
    ASSERT_TRUE(lausch);
    const int port = lausch->port();
    std::thread stumm([&] {
        if (!lausch->annehmen(20000)) return;
        // Meldet sich nie; nach einer Sekunde trennt die Attrappe die Leitung.
        uint8_t b[16];
        for (int i = 0; i < 20; ++i)
            if (lausch->lesen(b, sizeof b, 50) < 0) break;
        lausch->schliessen();
    });
    const Lauf l = starte({"ls", "--telnet", "127.0.0.1:" + std::to_string(port), "--login", "kermit:kermit"});
    stumm.join();
    EXPECT_EQ(l.rc, 2) << l.err;
    EXPECT_NE(l.err.find("Anmeldung"), std::string::npos) << l.err;
}

// ─── Strg+C (POSIX) ──────────────────────────────────────────────────────────

#ifndef _WIN32
namespace {
int pidAus(const fs::path& f) {
    for (int i = 0; i < 400; ++i) {
        const std::string t = lies(f);
        if (!t.empty() && t.find('\n') != std::string::npos) return std::atoi(t.c_str());
        std::this_thread::sleep_for(20ms);
    }
    return 0;
}
}  // namespace

TEST(KermitCliAbbruch, StrgCBeendetEinenWartendenServerSauber) {
    TempOrdner tmp;
    s_wurzel = tmp.p;
    fs::create_directories(tmp / "frei");
    const fs::path pidf = tmp / "pid";
    Hintergrund s({"serve", "--listen-telnet", "0", (tmp / "frei").string()}, "echo $$ > " + q(pidf.string()) + "; exec ");
    ASSERT_GT(s.port(), 0);
    const int pid = pidAus(pidf);
    ASSERT_GT(pid, 0);
    ::kill(pid, SIGINT);
    const auto t0 = std::chrono::steady_clock::now();
    const Lauf l = s.warten();
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 5s);
    EXPECT_EQ(l.rc, 1) << l.err;
    EXPECT_NE(l.err.find("Abbruch"), std::string::npos) << l.err;
}

TEST(KermitCliAbbruch, StrgCMitten_in_derUebertragungLaesstKeineHalbeDateiLiegen) {
    // Kleine Pakete machen die Uebertragung langsam genug, um sie mittendrin zu unterbrechen.
    TempOrdner tmp;
    s_wurzel = tmp.p;
    fs::create_directories(tmp / "frei");
    fs::create_directories(tmp / "ziel");
    schreibe(tmp / "frei" / "riesig.bin", zufall(6 * 1024 * 1024, 3));
    Hintergrund srv({"serve", "--listen-telnet", "0", (tmp / "frei").string()});
    const int port = srv.port();
    ASSERT_GT(port, 0);
    const fs::path pidf = tmp / "pid";
    Hintergrund cli({"get", "riesig.bin", "--to", (tmp / "ziel").string(), "--telnet", "127.0.0.1:" + std::to_string(port),
                     "--paket", "20"},
                    "echo $$ > " + q(pidf.string()) + "; exec ");
    const int pid = pidAus(pidf);
    ASSERT_GT(pid, 0);
    // warten, bis die Datei angefangen ist
    for (int i = 0; i < 250 && !fs::exists(tmp / "ziel" / "riesig.bin"); ++i) std::this_thread::sleep_for(20ms);
    std::this_thread::sleep_for(150ms);
    ::kill(pid, SIGINT);
    const Lauf c = cli.warten();
    srv.warten();
    if (c.rc == 0) GTEST_SKIP() << "Uebertragung war schon fertig (Rechner zu schnell)";
    EXPECT_EQ(c.rc, 1) << c.err;
    EXPECT_FALSE(fs::exists(tmp / "ziel" / "riesig.bin")) << "halbe Datei blieb liegen";
}
#endif
