/**
 * @file test_kermit_serverlogin.cpp
 * @brief Kermit-Server mit Anmeldezwang (doc/design/36_kermit.md §6.10 c, AP K12): `Sitzung::serve`
 *        spricht mit `Einstellungen::login_pflicht` wie getty + login der WEGA — `login:`,
 *        `Password:` (ohne Echo), `Login incorrect`, nach FINISH wieder `login:`.  Leitung im
 *        Speicher mit virtueller Uhr, der Test ist das Terminal.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>

#include "kermit/src/protokoll.h"

namespace fs = std::filesystem;
using namespace k1520::kermit;

namespace {

struct TempOrdner {
    fs::path p;
    TempOrdner() {
        std::random_device rd;
        p = fs::temp_directory_path() / ("k1520kermit_l_" + std::to_string(rd()) + std::to_string(rd()));
        fs::create_directories(p);
    }
    ~TempOrdner() {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
    std::string s() const { return p.u8string(); }
};

std::string lies(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

Einstellungen clientE() {
    Einstellungen e;
    e.frist_ms = 2000;
    return e;
}

/// Server mit Pflicht in eigenem Faden an Ende 1; Ende 0 gehoert dem Test.
struct Aufbau {
    SpeicherLeitungPaar leitung;
    TempOrdner wurzel;
    Freigabe freigabe{wurzel.p};
    std::thread faden;
    Ergebnis serverErgebnis;
    std::vector<std::string> ereignisse;

    void starte(bool pflicht = true, std::string name = "kermit", std::string kennwort = "kermit") {
        Einstellungen e;
        e.frist_ms = 3000;
        e.login_pflicht = pflicht;
        e.login_name = name;
        e.login_kennwort = kennwort;
        faden = std::thread([this, e] {
            Rueckrufe r;
            r.ereignis = [this](const std::string& t) { ereignisse.push_back(t); };
            Sitzung s(leitung.ende(1), e, r);
            serverErgebnis = s.serve(freigabe);
        });
    }
    void ende() {
        if (faden.joinable()) {
            leitung.ende(0).schliessen();
            faden.join();
        }
    }
    ~Aufbau() { ende(); }
};

/// Das Terminal des Anwenders: sendet Zeichen und wartet auf Text.
struct Terminal {
    Leitung& l;
    std::string alles;   ///< seit dem letzten `neu()` Gelesenes (Bit 7 geloescht)
    void sende(const std::string& s) { l.schreiben(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }
    void neu() { alles.clear(); }
    /// Liest, bis `text` im Gelesenen steht; Rest des Strom-Stuecks bleibt in `alles`.
    bool warte(const std::string& text, int64_t frist = 20000) {
        const int64_t ende = l.jetztMs() + frist;
        while (alles.find(text) == std::string::npos) {
            const int64_t rest = ende - l.jetztMs();
            if (rest <= 0) return false;
            uint8_t b[512];
            const int n = l.lesen(b, sizeof b, rest);
            if (n < 0) return false;
            for (int i = 0; i < n; ++i) alles.push_back(static_cast<char>(b[i] & 0x7F));
        }
        return true;
    }
    /// Wartet auf `text` und verwirft alles bis dahinter.
    bool nimm(const std::string& text, int64_t frist = 20000) {
        if (!warte(text, frist)) return false;
        alles.erase(0, alles.find(text) + text.size());
        return true;
    }
    bool anmelden(const std::string& name, const std::string& kennwort, const std::string& eol = "\r") {
        if (!nimm("login:")) return false;
        sende(name + eol);
        if (!nimm("Password:")) return false;
        sende(kennwort + eol);
        return true;
    }
};

std::string mitBit7(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(static_cast<unsigned char>(c) | 0x80);
    return r;
}

}  // namespace

TEST(KermitServerLogin, RichtigeDatenStartenDieServerschleife) {
    Aufbau a;
    std::ofstream(a.wurzel.p / "hallo.txt", std::ios::binary) << "Hallo Welt";
    a.starte();
    Terminal t{a.leitung.ende(0)};
    ASSERT_TRUE(t.anmelden("kermit", "kermit"));
    ASSERT_TRUE(t.nimm("server starting")) << t.alles;
    Sitzung c(a.leitung.ende(0), clientE());
    TempOrdner hol;
    const Ergebnis e = c.holen("hallo.txt", hol.s());
    ASSERT_TRUE(e.ok()) << e.text;
    EXPECT_EQ(lies(hol.p / "hallo.txt"), "Hallo Welt");
}

TEST(KermitServerLogin, NameWirdGeechotDasKennwortNicht) {
    Aufbau a;
    a.starte(true, "olli", "geheim123");
    Terminal t{a.leitung.ende(0)};
    ASSERT_TRUE(t.anmelden("olli", "geheim123"));
    ASSERT_TRUE(t.nimm("server starting")) << t.alles;
    EXPECT_EQ(t.alles.find("geheim123"), std::string::npos) << "Kennwort wurde zurueckgeechot";
}

TEST(KermitServerLogin, FalscheDatenGebenLoginIncorrectUndErlaubenDenNaechstenVersuch) {
    for (int welcher = 0; welcher < 2; ++welcher) {   // 0: falsches Kennwort, 1: falscher Name
        Aufbau a;
        std::ofstream(a.wurzel.p / "x.txt", std::ios::binary) << "xyz";
        a.starte();
        Terminal t{a.leitung.ende(0)};
        ASSERT_TRUE(t.anmelden(welcher ? "niemand" : "kermit", welcher ? "kermit" : "falsch"));
        ASSERT_TRUE(t.nimm("Login incorrect")) << t.alles;
        ASSERT_TRUE(t.anmelden("kermit", "kermit"));   // wartet auf das NEUE login:
        ASSERT_TRUE(t.nimm("server starting")) << t.alles;
        Sitzung c(a.leitung.ende(0), clientE());
        TempOrdner hol;
        ASSERT_TRUE(c.holen("x.txt", hol.s()).ok());
        EXPECT_EQ(lies(hol.p / "x.txt"), "xyz");
    }
}

TEST(KermitServerLogin, NachDreiFehlversuchenBeginntEsVonVornOhneSperre) {
    Aufbau a;
    a.starte();
    Terminal t{a.leitung.ende(0)};
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(t.anmelden("kermit", "falsch" + std::to_string(i))) << i;
        ASSERT_TRUE(t.nimm("Login incorrect")) << i << ": " << t.alles;
    }
    ASSERT_TRUE(t.anmelden("kermit", "kermit"));
    ASSERT_TRUE(t.nimm("server starting")) << t.alles;
    int abgelehnt = 0;
    for (const auto& e : a.ereignisse) abgelehnt += e.find("Anmeldung abgelehnt") == 0;
    EXPECT_EQ(abgelehnt, 4);
}

TEST(KermitServerLogin, Bit7AmNamenUndAmKennwortWirdVorDemVergleichGeloescht) {
    Aufbau a;
    a.starte();
    Terminal t{a.leitung.ende(0)};
    ASSERT_TRUE(t.nimm("login:"));
    t.sende(mitBit7("kermit") + "\x8d");   // gerade Paritaet: auch das CR traegt Bit 7
    ASSERT_TRUE(t.nimm("Password:"));
    t.sende(mitBit7("kermit") + "\x8d");
    EXPECT_TRUE(t.nimm("server starting")) << t.alles;
}

TEST(KermitServerLogin, CrLfBeendetEineEingabeUndEinLeeresReturnZeigtNurLoginErneut) {
    Aufbau a;
    a.starte();
    Terminal t{a.leitung.ende(0)};
    ASSERT_TRUE(t.nimm("login:"));
    t.sende("\r");                       // leer: nur erneut login:, kein Fehlversuch
    ASSERT_TRUE(t.nimm("login:"));
    t.sende("kermit\r\nkermit\r\n");     // CR LF
    EXPECT_TRUE(t.nimm("server starting")) << t.alles;
    EXPECT_EQ(t.alles.find("incorrect"), std::string::npos);
}

TEST(KermitServerLogin, NachFinishKehrtDerServerZuLoginZurueckUndBedientEineZweiteAnmeldung) {
    Aufbau a;
    std::ofstream(a.wurzel.p / "eins.txt", std::ios::binary) << "1";
    a.starte();
    Terminal t{a.leitung.ende(0)};
    ASSERT_TRUE(t.anmelden("kermit", "kermit"));
    ASSERT_TRUE(t.nimm("server starting"));
    {
        Sitzung c(a.leitung.ende(0), clientE());
        TempOrdner hol;
        ASSERT_TRUE(c.holen("eins.txt", hol.s()).ok());
        ASSERT_TRUE(c.entferntBeenden().ok());   // G F
    }
    // Was nach der Quittung kommt, kann in der Sitzung des Clients haengen — ein RETURN ruft login: sicher.
    t.neu();
    t.sende("\r");
    ASSERT_TRUE(t.anmelden("kermit", "kermit")) << t.alles;
    ASSERT_TRUE(t.nimm("server starting"));
    {
        Sitzung c(a.leitung.ende(0), clientE());
        TempOrdner hol;
        ASSERT_TRUE(c.holen("eins.txt", hol.s()).ok());
        ASSERT_TRUE(c.entferntAbmelden().ok());   // G L
    }
    t.sende("\r");
    ASSERT_TRUE(t.nimm("login:"));
    a.leitung.ende(0).schliessen();
    a.faden.join();
    EXPECT_EQ(a.serverErgebnis.art, Fehlerart::LeitungGetrennt);   // die Pflicht endet nur durch Trennung
}

TEST(KermitServerLogin, EinClientDerSofortPaketeSchicktBlockiertNichts) {
    Aufbau a;
    a.starte();
    {
        Sitzung c(a.leitung.ende(0), clientE());
        const Ergebnis e = c.entferntVerzeichnis("");
        EXPECT_FALSE(e.ok());   // ohne Anmeldung gibt es kein Protokoll
    }
    Terminal t{a.leitung.ende(0)};
    t.sende("\r");
    ASSERT_TRUE(t.nimm("login:"));
    t.sende("kermit\r");
    ASSERT_TRUE(t.warte("Password:"));
    t.neu();
    t.sende("kermit\r");
    // Je nach Lage der Reste ist die Anmeldung eine Zeile spaeter dran: notfalls noch einmal.
    if (!t.warte("server starting", 5000)) {
        t.sende("\r");
        ASSERT_TRUE(t.anmelden("kermit", "kermit")) << t.alles;
    }
    EXPECT_TRUE(t.nimm("server starting")) << t.alles;
}

TEST(KermitServerLogin, OhnePflichtEndetServeNachFinishWieBisher) {
    Aufbau a;
    std::ofstream(a.wurzel.p / "e.txt", std::ios::binary) << "e";
    a.starte(false);
    Sitzung c(a.leitung.ende(0), clientE());
    TempOrdner hol;
    ASSERT_TRUE(c.holen("e.txt", hol.s()).ok());
    ASSERT_TRUE(c.entferntBeenden().ok());
    a.faden.join();
    EXPECT_TRUE(a.serverErgebnis.ok());   // serve endet nach FINISH, kein login:
}

TEST(KermitServerLogin, AbbruchBeendetDenServerInDerAnmeldung) {
    SpeicherLeitungPaar leitung;
    TempOrdner w;
    Freigabe f{w.p};
    Einstellungen e;
    e.login_pflicht = true;
    Sitzung s(leitung.ende(1), e);
    Ergebnis r;
    std::thread th([&] { r = s.serve(f); });
    Terminal t{leitung.ende(0)};
    ASSERT_TRUE(t.nimm("login:"));
    s.abbrechen();
    th.join();
    EXPECT_EQ(r.art, Fehlerart::Abgebrochen);
}
