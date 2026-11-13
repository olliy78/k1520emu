// W5100-Registermodell gegen Loopback (Entwurf 50 AP N1).  Nur Port 0, nur 127.0.0.1.
#include <gtest/gtest.h>

#include "core/peripherals/w5100/w5100.h"

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

using namespace k1520::w5100;
namespace net = k1520::serial::net;

namespace {

bool warteAuf(const std::function<bool()>& f, int ms = 4000) {
    const auto ende = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < ende) {
        if (f()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return f();
}

uint8_t sr(W5100& w, int n) { return w.lese(reg::sock(n, reg::Sn_SR)); }
uint8_t ir(W5100& w, int n) { return w.lese(reg::sock(n, reg::Sn_IR)); }
void cmd(W5100& w, int n, uint8_t c) { w.schreibe(reg::sock(n, reg::Sn_CR), c); }

void ziel(W5100& w, int n, const uint8_t ip[4], uint16_t port) {
    for (int i = 0; i < 4; ++i) w.schreibe(reg::sock(n, reg::Sn_DIPR + i), ip[i]);
    w.schreibe16(reg::sock(n, reg::Sn_DPORT), port);
}

const uint8_t kLoop[4] = {127, 0, 0, 1};

void oeffne(W5100& w, int n, uint8_t proto, uint16_t lokalPort = 0) {
    w.schreibe(reg::sock(n, reg::Sn_MR), proto);
    w.schreibe16(reg::sock(n, reg::Sn_PORT), lokalPort);
    cmd(w, n, reg::CMD_OPEN);
}

// Daten in den TX-Ring legen, TX_WR vorschieben, SEND geben.
void sende(W5100& w, int n, const std::string& d) {
    const uint16_t wr = w.lese16(reg::sock(n, reg::Sn_TX_WR));
    const uint16_t g = 2048;
    for (size_t i = 0; i < d.size(); ++i)
        w.schreibe(static_cast<uint16_t>(reg::TX_SPEICHER + n * g + ((wr + i) & (g - 1))), static_cast<uint8_t>(d[i]));
    w.schreibe16(reg::sock(n, reg::Sn_TX_WR), static_cast<uint16_t>(wr + d.size()));
    cmd(w, n, reg::CMD_SEND);
}

// n Bytes aus dem RX-Ring holen und RX_RD nachziehen (+ RECV).
std::string hole(W5100& w, int n, size_t len) {
    const uint16_t rd = w.lese16(reg::sock(n, reg::Sn_RX_RD));
    const uint16_t g = 2048;
    std::string s;
    for (size_t i = 0; i < len; ++i)
        s += static_cast<char>(w.lese(static_cast<uint16_t>(reg::RX_SPEICHER + n * g + ((rd + i) & (g - 1)))));
    w.schreibe16(reg::sock(n, reg::Sn_RX_RD), static_cast<uint16_t>(rd + len));
    cmd(w, n, reg::CMD_RECV);
    return s;
}

bool bereit(W5100& w, int n, size_t len) { return w.lese16(reg::sock(n, reg::Sn_RX_RSR)) >= len; }

struct TestServer {
    net::Lauscher l;
    net::Socket   k;
    TestServer() : l(net::lauschenLoopback(0)) {}
    bool annehmen() {
        return warteAuf([&] { k = net::annehmen(l.sock.fd()); return k.gueltig(); });
    }
    std::string lesen(size_t len) {
        std::string s;
        warteAuf([&] {
            char b[4096];
            auto e = net::empfangen(k.fd(), b, sizeof b);
            if (e.status == net::IoStatus::Ok) s.append(b, e.n);
            return s.size() >= len;
        });
        return s;
    }
    void schicken(const std::string& s) { net::senden(k.fd(), s.data(), s.size()); }
};

std::string muster(size_t n, char start = 'A') {
    std::string s;
    for (size_t i = 0; i < n; ++i) s += static_cast<char>(start + (i % 26));
    return s;
}

} // namespace

TEST(W5100, ResetVorgaben) {
    W5100 w;
    EXPECT_EQ(w.lese16(reg::RTR), 0x07D0);
    EXPECT_EQ(w.lese(reg::RMSR), 0x55);
    EXPECT_EQ(w.lese(reg::TMSR), 0x55);
    EXPECT_EQ(sr(w, 0), reg::SR_CLOSED);
    EXPECT_EQ(w.lese16(reg::sock(0, reg::Sn_TX_FSR)), 2048);
    w.schreibe(reg::SIPR, 10);
    w.schreibe(reg::RMSR, 0x03);
    w.schreibe(reg::MR, reg::MR_RESET);
    EXPECT_EQ(w.lese(reg::SIPR), 0);
    EXPECT_EQ(w.lese(reg::RMSR), 0x55);
    EXPECT_EQ(w.lese(reg::MR) & reg::MR_RESET, 0);
}

TEST(W5100, AdressregisterSindSchreibbar) {
    W5100 w;
    for (int i = 0; i < 6; ++i) w.schreibe(reg::SHAR + i, 0x10 + i);
    for (int i = 0; i < 6; ++i) EXPECT_EQ(w.lese(reg::SHAR + i), 0x10 + i);
    w.schreibe(reg::GAR, 192); EXPECT_EQ(w.lese(reg::GAR), 192);
}

TEST(W5100, PuffergroessenFolgenDemMsr) {
    W5100 w;
    w.schreibe(reg::TMSR, 0x03);   // Socket 0 = 8 KB, Rest passt nicht mehr
    w.schreibe(reg::sock(0, reg::Sn_MR), reg::PROTO_TCP);
    cmd(w, 0, reg::CMD_OPEN);
    EXPECT_EQ(w.lese16(reg::sock(0, reg::Sn_TX_FSR)), 8192);
    EXPECT_EQ(w.lese16(reg::sock(1, reg::Sn_TX_FSR)), 0);
    w.schreibe(reg::TMSR, 0x00);   // 1 KB je Socket
    EXPECT_EQ(w.lese16(reg::sock(2, reg::Sn_TX_FSR)), 1024);
}

TEST(W5100, TcpClientSendenEmpfangenUndRingumlauf) {
    TestServer srv;
    ASSERT_TRUE(srv.l.sock);
    W5100 w;
    oeffne(w, 0, reg::PROTO_TCP);
    EXPECT_EQ(sr(w, 0), reg::SR_INIT);
    ziel(w, 0, kLoop, static_cast<uint16_t>(srv.l.port));
    cmd(w, 0, reg::CMD_CONNECT);
    ASSERT_TRUE(warteAuf([&] { return sr(w, 0) == reg::SR_ESTABLISHED; }));
    EXPECT_TRUE(ir(w, 0) & reg::IR_CON);
    ASSERT_TRUE(srv.annehmen());

    // Dreimal 1500 Byte: das dritte Mal quert die 2-KB-Kante des TX- und des RX-Rings.
    for (int runde = 0; runde < 3; ++runde) {
        const std::string d = muster(1500, 'a' + runde);
        sende(w, 0, d);
        ASSERT_TRUE(warteAuf([&] { return ir(w, 0) & reg::IR_SEND_OK; }));
        w.schreibe(reg::sock(0, reg::Sn_IR), reg::IR_SEND_OK);
        EXPECT_EQ(w.lese16(reg::sock(0, reg::Sn_TX_FSR)), 2048);
        ASSERT_EQ(srv.lesen(d.size()), d);

        srv.schicken(d);   // Echo
        ASSERT_TRUE(warteAuf([&] { return bereit(w, 0, d.size()); }));
        EXPECT_EQ(hole(w, 0, d.size()), d);
        EXPECT_EQ(w.lese16(reg::sock(0, reg::Sn_RX_RSR)), 0);
    }
    EXPECT_GT(w.lese16(reg::sock(0, reg::Sn_TX_WR)), 2048 + 1);   // Zeiger sind ueber die Kante gelaufen
}

TEST(W5100, GegenstelleSchliesstGibtCloseWait) {
    TestServer srv;
    W5100 w;
    oeffne(w, 0, reg::PROTO_TCP);
    ziel(w, 0, kLoop, static_cast<uint16_t>(srv.l.port));
    cmd(w, 0, reg::CMD_CONNECT);
    ASSERT_TRUE(warteAuf([&] { return sr(w, 0) == reg::SR_ESTABLISHED; }));
    ASSERT_TRUE(srv.annehmen());
    srv.k.schliessen();
    ASSERT_TRUE(warteAuf([&] { return sr(w, 0) == reg::SR_CLOSE_WAIT; }));
    EXPECT_TRUE(ir(w, 0) & reg::IR_DISCON);
    cmd(w, 0, reg::CMD_CLOSE);
    EXPECT_EQ(sr(w, 0), reg::SR_CLOSED);
}

TEST(W5100, DisconSchliesstDieVerbindung) {
    TestServer srv;
    W5100 w;
    oeffne(w, 0, reg::PROTO_TCP);
    ziel(w, 0, kLoop, static_cast<uint16_t>(srv.l.port));
    cmd(w, 0, reg::CMD_CONNECT);
    ASSERT_TRUE(warteAuf([&] { return sr(w, 0) == reg::SR_ESTABLISHED; }));
    ASSERT_TRUE(srv.annehmen());
    cmd(w, 0, reg::CMD_DISCON);
    ASSERT_TRUE(warteAuf([&] { return sr(w, 0) == reg::SR_CLOSED; }));
    // Der Test-Server sieht das Ende.
    char b[8];
    ASSERT_TRUE(warteAuf([&] { return net::empfangen(srv.k.fd(), b, sizeof b).status == net::IoStatus::Geschlossen; }));
}

TEST(W5100, TcpServerListenUndVerbindung) {
    W5100 w;
    oeffne(w, 1, reg::PROTO_TCP, 0);
    cmd(w, 1, reg::CMD_LISTEN);
    EXPECT_EQ(sr(w, 1), reg::SR_LISTEN);
    ASSERT_TRUE(warteAuf([&] { return w.wirtsPort(1) != 0; }));
    net::Fehler f;
    auto z = net::aufloesen("127.0.0.1", w.wirtsPort(1), &f);
    ASSERT_FALSE(z.empty());
    net::Socket cl = net::verbindenAlle(z, 2000, &f);
    ASSERT_TRUE(cl.gueltig());
    ASSERT_TRUE(warteAuf([&] { return sr(w, 1) == reg::SR_ESTABLISHED; }));
    EXPECT_EQ(w.lese(reg::sock(1, reg::Sn_DIPR)), 127);
    EXPECT_NE(w.lese16(reg::sock(1, reg::Sn_DPORT)), 0);
    net::senden(cl.fd(), "hallo", 5);
    ASSERT_TRUE(warteAuf([&] { return bereit(w, 1, 5); }));
    EXPECT_EQ(hole(w, 1, 5), "hallo");
}

TEST(W5100, UdpMitAchtByteKopf) {
    W5100 w;
    oeffne(w, 2, reg::PROTO_UDP, 0);
    EXPECT_EQ(sr(w, 2), reg::SR_UDP);
    ASSERT_TRUE(warteAuf([&] { return w.wirtsPort(2) != 0; }));

    net::Fehler f;
    net::Socket peer = net::udpOeffnen(0, true, &f);
    ASSERT_TRUE(peer.gueltig());
    const int peerPort = net::lokalerPort(peer.fd());

    // Gast -> Peer
    ziel(w, 2, kLoop, static_cast<uint16_t>(peerPort));
    sende(w, 2, "ping");
    ASSERT_TRUE(warteAuf([&] { return ir(w, 2) & reg::IR_SEND_OK; }));
    char buf[64];
    unsigned char ip[4]; int port = 0;
    net::IoErgebnis e;
    ASSERT_TRUE(warteAuf([&] { e = net::udpEmpfangen(peer.fd(), buf, sizeof buf, ip, &port); return e.status == net::IoStatus::Ok; }));
    EXPECT_EQ(std::string(buf, e.n), "ping");
    EXPECT_EQ(port, w.wirtsPort(2));

    // Peer -> Gast: Kopf = IP, Port, Laenge
    const unsigned char loop[4] = {127, 0, 0, 1};
    net::udpSenden(peer.fd(), loop, w.wirtsPort(2), "pong!", 5);
    ASSERT_TRUE(warteAuf([&] { return bereit(w, 2, 13); }));
    const std::string d = hole(w, 2, 13);
    EXPECT_EQ(static_cast<uint8_t>(d[0]), 127);
    EXPECT_EQ(static_cast<uint8_t>(d[3]), 1);
    EXPECT_EQ((static_cast<uint8_t>(d[4]) << 8) | static_cast<uint8_t>(d[5]), peerPort);
    EXPECT_EQ((static_cast<uint8_t>(d[6]) << 8) | static_cast<uint8_t>(d[7]), 5);
    EXPECT_EQ(d.substr(8), "pong!");
}

TEST(W5100, ConnectOhneServerMeldetTimeout) {
    int freierPort;
    { auto l = net::lauschenLoopback(0); freierPort = l.port; }   // Lauscher zu: Port jetzt frei
    W5100 w;
    w.setzeVerbindenFristMax(300);
    oeffne(w, 0, reg::PROTO_TCP);
    ziel(w, 0, kLoop, static_cast<uint16_t>(freierPort));
    cmd(w, 0, reg::CMD_CONNECT);
    ASSERT_TRUE(warteAuf([&] { return ir(w, 0) & reg::IR_TIMEOUT; }));
    EXPECT_EQ(sr(w, 0), reg::SR_CLOSED);
    EXPECT_TRUE(w.interrupt() == false);   // IMR = 0
    w.schreibe(reg::IMR, 0x01);
    EXPECT_TRUE(w.interrupt());
}

TEST(W5100, VorgabeWeistKeinZielAb) {
    W5100 w;   // nur die Entscheidungsfunktion: es wird kein Aussenziel kontaktiert
    EXPECT_TRUE(w.zielErlaubt(0xC0000201u, 80));   // 192.0.2.1
    EXPECT_TRUE(w.zielErlaubt(0x7F000001u, 80));
    W5100::Richtlinie nur;
    nur.aussenErlaubt = false;
    w.setzeRichtlinie(nur);
    EXPECT_FALSE(w.zielErlaubt(0xC0000201u, 80));   // abschaltbar
    EXPECT_TRUE(w.zielErlaubt(0x7F000001u, 80));
}

TEST(W5100, RueckrufKannEinZielVerweigern) {
    W5100::Richtlinie r;
    int gefragt = 0;
    r.frage = [&](uint32_t ip, uint16_t) { ++gefragt; return ip != 0xC0000201u ? true : false; };
    W5100 w(r);
    w.setzeVerbindenFristMax(100);
    oeffne(w, 0, reg::PROTO_TCP);
    const uint8_t aussen[4] = {192, 0, 2, 1};
    ziel(w, 0, aussen, 80);
    cmd(w, 0, reg::CMD_CONNECT);
    ASSERT_TRUE(warteAuf([&] { return ir(w, 0) & reg::IR_TIMEOUT; }));
    EXPECT_EQ(gefragt, 1);
    EXPECT_EQ(w.verbindungsversuche(), 0u);   // Rueckruf sagte nein
}

TEST(W5100, PufferVollBegrenztTxFsr) {
    W5100 w;
    oeffne(w, 0, reg::PROTO_TCP);   // nicht verbunden: nichts leert den TX-Puffer
    w.schreibe16(reg::sock(0, reg::Sn_TX_WR), 2000);
    EXPECT_EQ(w.lese16(reg::sock(0, reg::Sn_TX_FSR)), 48);
    w.schreibe16(reg::sock(0, reg::Sn_TX_WR), 2048);
    EXPECT_EQ(w.lese16(reg::sock(0, reg::Sn_TX_FSR)), 0);
}

TEST(W5100, EmpfangsRueckstauVerliertNichts) {
    TestServer srv;
    W5100 w;
    oeffne(w, 0, reg::PROTO_TCP);
    ziel(w, 0, kLoop, static_cast<uint16_t>(srv.l.port));
    cmd(w, 0, reg::CMD_CONNECT);
    ASSERT_TRUE(warteAuf([&] { return sr(w, 0) == reg::SR_ESTABLISHED; }));
    ASSERT_TRUE(srv.annehmen());
    const std::string d = muster(6000);   // mehr als der 2-KB-RX-Puffer
    std::thread t([&] {
        size_t ab = 0;
        warteAuf([&] {
            auto e = net::senden(srv.k.fd(), d.data() + ab, d.size() - ab);
            if (e.status == net::IoStatus::Ok) ab += e.n;
            return ab >= d.size();
        }, 8000);
    });
    std::string got;
    const bool ok = warteAuf([&] {
        const uint16_t n = w.lese16(reg::sock(0, reg::Sn_RX_RSR));
        EXPECT_LE(n, 2048);
        if (n) got += hole(w, 0, n);
        return got.size() >= d.size();
    }, 8000);
    t.join();
    ASSERT_TRUE(ok);
    EXPECT_EQ(got, d);
}
