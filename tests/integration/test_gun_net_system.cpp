/**
 * @file test_gun_net_system.cpp
 * @brief KCNet (Entwurf 50 AP N2, Protokoll 50a): die ECHTEN Gastprogramme TFTP1287.COM (TFTP 1.2) und CPMN1587.COM
 *        (CPMNET 1.5) gegen die Karte.
 *
 * Die Programme (fuer die Basis C0H gebaut) liegen NUR lokal in `~/Documents/K1520emu/GIDE_Software/kcnet/sw/cpmnet87/`
 * (Lizenz ungeklaert, nie im Repo; `K1520_GIDE_SW` ueberschreibt den Ort der GIDE_Software).  Fehlen sie, werden die
 * Faelle uebersprungen.  Sie kommen auf eine TEMP-KOPIE der CP/A-Bootdiskette; die Karte steckt auf Basis C0H.
 * Das Netz des TESTS bleibt Loopback; die Karte selbst ist unbeschraenkt.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "core/filesystem/cpm/cpm_fs.h"
#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/cards/gun_net/gun_net.h"
#include "core/machines/a5120/a5120.h"
#include "core/serial/net/socket.h"
#include "core/peripherals/floppy_drive/disk_image.h"
#include "tests/support/fixtures.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"

using k1520test::runCycles;
using k1520test::runSmallUntil;
using k1520test::runUntilVramContains;
using k1520test::TempDisk;
using k1520test::typeKey;
using k1520test::typeString;
using k1520test::vramText;
namespace fs = std::filesystem;

namespace {

constexpr int kBootBudget  = 90'000'000;
constexpr int kInputBudget = 40'000'000;
constexpr int kLangBudget  = 200'000'000;
constexpr const char* kFs = "cpa_ds80_5x1024_boot";

std::string swVerzeichnis() {
    if (const char* e = std::getenv("K1520_GIDE_SW")) return e;
    if (const char* h = std::getenv("HOME")) return std::string(h) + "/Documents/K1520emu/GIDE_Software";
    return "";
}
std::vector<uint8_t> dateiLesen(const fs::path& pfad) {
    std::ifstream f(pfad, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}
void dateiSchreiben(const fs::path& pfad, const std::vector<uint8_t>& d) {
    std::ofstream f(pfad, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
}

const FormatCatalog& formate() {
    static FormatCatalog c = [] { std::string f; return FormatCatalog::loadDefault(&f); }();
    return c;
}
const FsCatalog& dateisysteme() {
    static FsCatalog c = [] { std::string f; return FsCatalog::loadDefault(formate(), &f); }();
    return c;
}
bool diskettePut(const std::string& pfad, const std::vector<std::pair<std::string, std::vector<uint8_t>>>& dateien) {
    const FsProfile* p = dateisysteme().find(kFs);
    if (!p) return false;
    const DiskFormat* f = formate().find(p->format);
    if (!f) return false;
    auto disk = DiskImage::open(pfad, std::optional<DiskFormat>(*f), false);
    if (!disk) return false;
    SectorSpace space(disk->medium(), *f);
    std::string err;
    auto vol = CpmFileSystem::mount(space, *p, err);
    if (!vol) return false;
    for (const auto& d : dateien) {
        WriteOptions o;
        o.overwrite = true;
        if (!vol->write(d.first, d.second, o)) return false;
    }
    return disk->flush();
}
std::vector<uint8_t> dateiVonDiskette(const std::string& pfad, const std::string& name) {
    const FsProfile* p = dateisysteme().find(kFs);
    const DiskFormat* f = formate().find(p->format);
    auto disk = DiskImage::open(pfad, std::optional<DiskFormat>(*f), true);
    std::vector<uint8_t> out;
    if (!disk) return out;
    SectorSpace space(disk->medium(), *f);
    std::string err;
    auto vol = CpmFileSystem::mount(space, *p, err);
    if (vol) vol->read(name, out);
    return out;
}

/// Bildschirm zur Diagnose an die Datei anhaengen, die K1520_TEST_ZEIGEN nennt (ctest verschluckt stderr gruener Faelle).
void zeige(const char* was, A5120Machine& m) {
    if (const char* d = std::getenv("K1520_TEST_ZEIGEN")) {
        std::ofstream f(d, std::ios::app);
        f << "=== " << was << " ===\n" << vramText(m) << "\n";
    }
}

/// Kommando tippen und bis zum NEUEN Prompt laufen (Schirm vorher wischen).
bool lauf(A5120Machine& m, const std::string& cmd, int budget = kInputBudget) {
    k1520test::wipeVram(m);
    typeString(m, cmd);
    typeKey(m, k1520test::QK_RETURN);
    return k1520test::runSmallUntilVisible(m, "A>", budget);
}


struct Leise : ::testing::Test {
    void SetUp() override { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN); }
};
using GunNetSystem = Leise;

/// Eine temporaere CP/A-Diskette mit den Gastprogrammen; leer, wenn sie lokal fehlen.
struct Gast {
    std::vector<uint8_t> tftp, cpmn;
    fs::path sw;
    Gast() {
        sw = fs::path(swVerzeichnis()) / "kcnet" / "sw" / "cpmnet87";
        tftp = dateiLesen(sw / "TFTP1287.COM");
        cpmn = dateiLesen(sw / "CPMN1587.COM");
    }
    bool da() const { return !tftp.empty() && !cpmn.empty(); }
};

/// Freier Loopback-TCP-Port (kurz gebunden und wieder freigegeben; die Gast-Eingabe verlangt eine Zahl).
int freierPort() {
    auto l = k1520::serial::net::lauschenLoopback(0);
    return k1520::serial::net::lokalerPort(l.sock.fd());
}

/// Die Maschine mit gesteckter Karte (Basis C0H, Ziele nur Loopback), CP/A gebootet bis "A>".
struct Rechner {
    TempDisk a;
    A5120Machine m;
    explicit Rechner(const char* name) : a("cpa_cpa780_k5601_noclock.img", name) {}
    void start(const Gast& gast, bool link = true) {
        ASSERT_TRUE(diskettePut(a.path(), {{"TFTP.COM", gast.tftp}, {"CPMNET.COM", gast.cpmn}}));
        k1520::GunNet::Config gc;
        gc.basis = 0xC0;                       // die Programme sind fuer C0H..C3H gebaut
        gc.richtlinie.aussenErlaubt = false;   // der TEST bleibt auf Loopback (die Karte selbst waere frei)
        gc.link_online = link;
        ASSERT_TRUE(m.installGunNet(gc)) << m.gunNetFehler();
        ASSERT_TRUE(m.mountDisk(0, a.path(), "cpa780", false)) << m.lastError();
        m.powerOn();
        ASSERT_TRUE(runUntilVramContains(m, "TPA ist OK!", kBootBudget)) << vramText(m);
        ASSERT_TRUE(runSmallUntil(m, "A>", kInputBudget)) << vramText(m);
    }
    /// Zeichen tippen (optional mit Return) und eine Weile laufen lassen.
    void taste(const std::string& t, bool cr = false, long long zyklen = 8'000'000) {
        typeString(m, t);
        if (cr) typeKey(m, k1520test::QK_RETURN);
        runCycles(m, zyklen);
    }
};

}  // namespace

/**
 * @test GunNetSystem/ProgrammeErkennenDieKarte
 * @brief CP/A am A5120, Karte auf C0H..C3H: TFTP und CPMNET starten, erkennen die Karte, lesen Version und Link.
 */
TEST_F(GunNetSystem, ProgrammeErkennenDieKarte) {
    Gast g;
    if (!g.da()) GTEST_SKIP() << "KCNet-Software nicht vorhanden (" << g.sw.string() << ")";
    Rechner r("k1520_gunnet_a.img");
    r.start(g);
    A5120Machine& m = r.m;

    ASSERT_TRUE(lauf(m, "TFTP", kLangBudget)) << vramText(m);
    zeige("TFTP", m);
    EXPECT_NE(vramText(m).find("TFTP Client&Server CP/M 2.x"), std::string::npos) << vramText(m);
    EXPECT_NE(vramText(m).find("Version 1.2"), std::string::npos) << vramText(m);
    EXPECT_EQ(vramText(m).find("not found"), std::string::npos) << "KCNET-Interface not found !";

    // ohne Netzkonfiguration (IP-Register 0): die Karte ist erkannt, Firmware >= 1.2 und Kabel online sind durch
    ASSERT_TRUE(lauf(m, "TFTP -h", kLangBudget)) << vramText(m);
    zeige("TFTP -h", m);
    EXPECT_NE(vramText(m).find("network not configured !"), std::string::npos) << vramText(m);
    EXPECT_EQ(vramText(m).find("not found"), std::string::npos) << vramText(m);
    EXPECT_EQ(vramText(m).find("network-cable"), std::string::npos) << vramText(m);
    EXPECT_EQ(vramText(m).find("requires at least"), std::string::npos) << vramText(m);

    // CPMNET: Statusseite des Interface-Menues
    k1520test::wipeVram(m);
    typeString(m, "CPMNET");
    typeKey(m, k1520test::QK_RETURN);
    ASSERT_TRUE(runSmallUntil(m, "NETWORK MENU", kLangBudget)) << vramText(m);
    r.taste("I");
    zeige("CPMNET I", m);
    EXPECT_NE(vramText(m).find("Software 1.2"), std::string::npos) << vramText(m);
    EXPECT_NE(vramText(m).find("Hardware 1.0"), std::string::npos) << vramText(m);
    EXPECT_NE(vramText(m).find("online"), std::string::npos) << vramText(m);
    r.taste("0");
    EXPECT_NE(vramText(m).find("COMMAND ERRORS = 0"), std::string::npos) << vramText(m);
    EXPECT_EQ(m.gunNet()->zaehler().fehlerkommandos, 0u);
    EXPECT_GT(m.gunNet()->zaehler().kommandos, 10u);
}

/**
 * @test GunNetSystem/KabelAbgezogenWirdGemeldet
 * @brief Gegenprobe: meldet Kommando 0B "offline", bricht TFTP mit "network-cable not connected !" ab.
 */
TEST_F(GunNetSystem, KabelAbgezogenWirdGemeldet) {
    Gast g;
    if (!g.da()) GTEST_SKIP() << "KCNet-Software nicht vorhanden (" << g.sw.string() << ")";
    Rechner r("k1520_gunnet_k.img");
    r.start(g, false);
    A5120Machine& m = r.m;
    // IP einstellen (CPMNET), damit TFTP nicht schon an "network not configured" haelt
    k1520test::wipeVram(m);
    typeString(m, "CPMNET");
    typeKey(m, k1520test::QK_RETURN);
    ASSERT_TRUE(runSmallUntil(m, "NETWORK MENU", kLangBudget)) << vramText(m);
    r.taste("T"); r.taste("1"); r.taste("127.0.0.2", true);
    r.taste("2"); r.taste("255.0.0.0", true);
    r.taste("3"); r.taste("127.0.0.1", true);
    r.taste("Q");
    ASSERT_TRUE(k1520test::runSmallUntilVisible(m, "A>", kInputBudget)) << vramText(m);
    ASSERT_TRUE(lauf(m, "TFTP -h", kLangBudget)) << vramText(m);
    zeige("TFTP -h ohne Kabel", m);
    EXPECT_NE(vramText(m).find("network-cable not connected !"), std::string::npos) << vramText(m);
}

/**
 * @test GunNetSystem/CpmnetTcpServerEchoMitLoopbackClient
 * @brief CPMNET: IP einstellen (T-Menue), "S" = TCP-Server-Socket auf einem freien Port; ein Loopback-Client
 *        verbindet sich, der Gast zeigt die empfangenen Daten, im Echo-Modus ("E") kommen sie zurueck.
 *        Alles durch den W5100 (Socket 3 LISTEN, Rx-Ring, RX_RD, RECV, SEND) mit den echten Gast-Treiberroutinen.
 */
TEST_F(GunNetSystem, CpmnetTcpServerEchoMitLoopbackClient) {
    namespace net = k1520::serial::net;
    Gast g;
    if (!g.da()) GTEST_SKIP() << "KCNet-Software nicht vorhanden (" << g.sw.string() << ")";
    Rechner r("k1520_gunnet_s.img");
    r.start(g);
    A5120Machine& m = r.m;

    k1520test::wipeVram(m);
    typeString(m, "CPMNET");
    typeKey(m, k1520test::QK_RETURN);
    ASSERT_TRUE(runSmallUntil(m, "NETWORK MENU", kLangBudget)) << vramText(m);

    // TCP/IP-Stack: Adresse/Maske/Gateway (der Gast verlangt "configured", der Wirt benutzt die Werte nicht)
    r.taste("T"); r.taste("1"); r.taste("127.0.0.2", true);
    r.taste("2"); r.taste("255.0.0.0", true);
    r.taste("3"); r.taste("127.0.0.1", true);
    r.taste("C");
    zeige("IP-Config", m);
    EXPECT_NE(vramText(m).find("Network: configured"), std::string::npos) << vramText(m);
    EXPECT_NE(vramText(m).find("MAC-Address: 02:4B:43:4E:45:54"), std::string::npos) << vramText(m);
    r.taste(" ");

    // lokaler Port, dann TCP-Server
    const int port = freierPort();
    r.taste("N"); r.taste("2"); r.taste(std::to_string(port), true);
    r.taste("S");
    zeige("TCP-Server", m);
    ASSERT_NE(vramText(m).find("TCP SERVER READY (PORT: " + std::to_string(port) + ")"), std::string::npos) << vramText(m);

    net::Socket cl;
    for (int i = 0; i < 100 && !cl.gueltig(); ++i) {
        runCycles(m, 500'000);
        auto z = net::aufloesen("127.0.0.1", port);
        if (!z.empty()) cl = net::verbinden(z[0], 500);
    }
    ASSERT_TRUE(cl.gueltig()) << "Loopback-Client kam nicht an den Gast-Server";
    ASSERT_TRUE(runSmallUntil(m, "CLIENT CONNECTED", 20'000'000)) << vramText(m);
    ASSERT_TRUE(runSmallUntil(m, "IP: 127.0.0.1", 20'000'000)) << vramText(m);

    net::senden(cl.fd(), "HALLO VOM HOST\r\n", 16);
    // Seit Kommando 8 die Portnummer in Net-Order liefert (offizielle Beschreibung V1.2), oeffnet der Gast fuer die DNS-
    // Rueckfrage zur Client-Adresse wirklich ein UDP-Socket (frueher byteverdreht = Port 192 = Bindung scheiterte sofort);
    // die drei Anfragen an 127.0.0.1:53 laufen ins Leere und kosten Maschinenzeit, bevor der Gast die Daten zeigt.
    ASSERT_TRUE(runSmallUntil(m, "RECEIVED: HALLO VOM HOST", kLangBudget)) << vramText(m);
    zeige("empfangen", m);

    r.taste("E");   // Echo-Modus: was der Client schickt, geht zurueck
    ASSERT_TRUE(runSmallUntil(m, "ECHO MODE ON", 20'000'000)) << vramText(m);
    net::senden(cl.fd(), "ZURUECK 123", 11);
    std::string echo;
    for (int i = 0; i < 100 && echo.size() < 11; ++i) {
        runCycles(m, 500'000);
        char buf[64];
        auto e = net::empfangen(cl.fd(), buf, sizeof buf);
        if (e.status == net::IoStatus::Ok) echo.append(buf, e.n);
    }
    zeige("Echo", m);
    EXPECT_EQ(echo, "ZURUECK 123");
    EXPECT_EQ(m.gunNet()->zaehler().fehlerkommandos, 0u);
}

/**
 * @test GunNetSystem/TftpGetVonLoopbackServer
 * @brief `TFTP GET 127.0.0.1 DATEN.BIN`: ein Test-Server auf Loopback UDP 69 (privilegierter Port: ohne Recht wird
 *        uebersprungen) liefert 600 Byte in zwei DATA-Bloecken (eigene TID); die Datei liegt danach bitgleich auf der
 *        CP/A-Diskette.  Der Gast benutzt dabei UDP-Socket 0 des W5100, Tx/Rx-Ringe und DHCP-freie Konfiguration.
 */
TEST_F(GunNetSystem, TftpGetVonLoopbackServer) {
    namespace net = k1520::serial::net;
    Gast g;
    if (!g.da()) GTEST_SKIP() << "KCNet-Software nicht vorhanden (" << g.sw.string() << ")";
    net::Fehler f;
    net::Socket srv = net::udpOeffnen(69, true, &f);
    if (!srv.gueltig()) GTEST_SKIP() << "UDP-Port 69 (TFTP) auf Loopback nicht bindbar (privilegiert)";

    Rechner r("k1520_gunnet_t.img");
    r.start(g);
    A5120Machine& m = r.m;
    k1520test::wipeVram(m);
    typeString(m, "CPMNET");
    typeKey(m, k1520test::QK_RETURN);
    ASSERT_TRUE(runSmallUntil(m, "NETWORK MENU", kLangBudget)) << vramText(m);
    r.taste("T"); r.taste("1"); r.taste("127.0.0.2", true);
    r.taste("2"); r.taste("255.0.0.0", true);
    r.taste("3"); r.taste("127.0.0.1", true);
    r.taste("Q");
    ASSERT_TRUE(k1520test::runSmallUntilVisible(m, "A>", kInputBudget)) << vramText(m);

    std::string daten;
    for (int i = 0; i < 600; ++i) daten.push_back(char((i * 5 + (i >> 4)) & 0xFF));
    k1520test::wipeVram(m);
    typeString(m, "TFTP GET 127.0.0.1 DATEN.BIN");
    typeKey(m, k1520test::QK_RETURN);

    auto empfange = [&](net::SockFd fd, std::string& d, unsigned char ip[4], int& port) {
        for (int i = 0; i < 400; ++i) {
            runCycles(m, 200'000);
            char buf[1024];
            auto e = net::udpEmpfangen(fd, buf, sizeof buf, ip, &port);
            if (e.status == net::IoStatus::Ok) { d.assign(buf, e.n); return true; }
        }
        return false;
    };
    std::string rrq;
    unsigned char gip[4] = {};
    int gport = 0;
    ASSERT_TRUE(empfange(srv.fd(), rrq, gip, gport)) << "kein Lesewunsch (RRQ) angekommen\n" << vramText(m);
    ASSERT_GE(rrq.size(), 4u);
    EXPECT_EQ(rrq[0], 0);
    EXPECT_EQ(rrq[1], 1) << "Opcode RRQ";
    EXPECT_NE(rrq.find("DATEN.BIN"), std::string::npos) << rrq;
    EXPECT_NE(rrq.find("OCTET"), std::string::npos) << "Modus OCTET (Grossbuchstaben)";

    net::Socket tid = net::udpOeffnen(0, true, &f);   // Antwort aus eigener TID, wie ein echter TFTP-Server
    ASSERT_TRUE(tid.gueltig());
    auto block = [&](int nr, const std::string& nutz) {
        std::string p = std::string("\0\3", 2);
        p.push_back(char(nr >> 8)); p.push_back(char(nr & 255));
        p += nutz;
        const unsigned char ziel[4] = {127, 0, 0, 1};
        net::udpSenden(tid.fd(), ziel, gport, p.data(), p.size());
    };
    auto ack = [&](int nr) {
        std::string a;
        unsigned char ip[4]; int port = 0;
        if (!empfange(tid.fd(), a, ip, port)) return false;
        return a.size() >= 4 && a[1] == 4 && uint8_t(a[2]) == (nr >> 8) && uint8_t(a[3]) == (nr & 255);
    };
    block(1, daten.substr(0, 512));
    ASSERT_TRUE(ack(1)) << "ACK 1 fehlt\n" << vramText(m);
    block(2, daten.substr(512));
    ASSERT_TRUE(ack(2)) << "ACK 2 fehlt\n" << vramText(m);

    ASSERT_TRUE(k1520test::runSmallUntilVisible(m, "A>", kLangBudget)) << vramText(m);
    zeige("TFTP GET", m);
    ASSERT_TRUE(m.flushDisks());
    const auto datei = dateiVonDiskette(r.a.path(), "DATEN.BIN");
    ASSERT_GE(datei.size(), daten.size());
    EXPECT_LE(datei.size(), daten.size() + 127);
    EXPECT_TRUE(std::equal(daten.begin(), daten.end(), datei.begin(), [](char a, uint8_t b) { return uint8_t(a) == b; }))
        << "TFTP-Datei weicht ab";
}
