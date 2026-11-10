/**
 * @file test_gun_net.cpp
 * @brief KCNet-Karte (Entwurf 50 AP N2, Protokoll 50a): Z80-PIO + ATmega-Modell + W5100 mit echtem Z80.
 *        Die Gastroutinen sind die Treiberprimitiven aus TFTP1287/CPMN1587 (014C senden, 0154 empfangen,
 *        PIO-Init 0109, Blockschleifen OUTI/INI 01F4/0201) von Hand uebersetzt.  Netz nur Loopback, Port 0.
 */
#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "core/cards/gun_net/gun_net.h"
#include "core/primitives/z80.h"

using k1520::GunNet;
namespace reg = k1520::w5100::reg;
namespace net = k1520::serial::net;

namespace {

struct Sonde : BusDevice {
    uint8_t ioRead(uint8_t) override { return 0x5A; }
    void ioWrite(uint8_t, uint8_t) override {}
    const char* deviceName() const override { return "Sonde"; }
};

bool warteAuf(const std::function<bool()>& f, int ms = 4000) {
    const auto ende = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < ende) {
        if (f()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return f();
}

GunNet::Config cfgLoopback(uint8_t basis) {
    GunNet::Config c;
    c.basis = basis;
    c.richtlinie.aussenErlaubt = false;   // der Test selbst bleibt auf Loopback
    return c;
}

struct Rig {
    K1520Bus bus;
    GunNet net;
    Z80 cpu;
    std::array<uint8_t, 65536> ram{};
    uint8_t b;   // Basis

    explicit Rig(uint8_t basis = 0xC0) : net(cfgLoopback(basis)), b(basis) {
        net.attachToBus(bus);
        cpu.readByte  = [this](uint16_t a) { return ram[a]; };
        cpu.writeByte = [this](uint16_t a, uint8_t d) { ram[a] = d; };
        cpu.readPort  = [this](uint16_t p) { return bus.ioRead(p); };
        cpu.writePort = [this](uint16_t p, uint8_t d) { bus.ioWrite(p, d); };
        const uint8_t pa = b, pb = b + 1;
        // 0200 sub_014C: OUT (pa),A ; L: IN A,(pb) ; RRCA ; JR C,L ; RET
        poke(0x200, {0xD3, pa, 0xDB, pb, 0x0F, 0x38, 0xFB, 0xC9});
        // 0208 sub_0154: L: IN A,(pb) ; RLCA ; JR NC,L ; IN A,(pa) ; RET
        poke(0x208, {0xDB, pb, 0x07, 0x30, 0xFB, 0xDB, pa, 0xC9});
        // 0220 Blockschreiben (01F4): OUTI ; L: IN A,(pb) ; RRCA ; JR C,L ; JR NZ,0220 ; RET   (C = Port, B = Anzahl, HL = Quelle)
        poke(0x220, {0xED, 0xA3, 0xDB, pb, 0x0F, 0x38, 0xFB, 0x20, 0xF7, 0xC9});
        // 0230 Blocklesen (0201): L: IN A,(pb) ; RLCA ; JR NC,L ; INI ; JR NZ,0230 ; RET
        poke(0x230, {0xDB, pb, 0x07, 0x30, 0xFB, 0xED, 0xA2, 0x20, 0xF7, 0xC9});
    }
    void poke(uint16_t a, std::initializer_list<uint8_t> l) { for (uint8_t v : l) ram[a++] = v; }

    void run(const std::vector<uint8_t>& code, uint8_t a = 0) {
        for (size_t i = 0; i < code.size(); ++i) ram[0x100 + i] = code[i];
        cpu.reset();
        cpu.PC = 0x100; cpu.A = a; cpu.SP = 0xF000;
        for (int i = 0; i < 2000000 && !cpu.halted; ++i) cpu.step();
        ASSERT_TRUE(cpu.halted) << "Programm hat nicht angehalten";
    }

    // sub_0109: PIO-Init + Erkennung; true = Karte gefunden (PB7 = 0 nach Dummy-Lesen)
    bool init() {
        const uint8_t pas = b + 2, pbs = b + 3;
        run({0x3E, 0x8F, 0xD3, pas, 0x3E, 0x07, 0xD3, pas,                       // Port A: Betriebsart 2, kein INT
             0x3E, 0xCF, 0xD3, pbs, 0x3E, 0xFF, 0xD3, pbs, 0x3E, 0x17, 0xD3, pbs, 0x3E, 0xFF, 0xD3, pbs,   // Port B: Bit, alles Eingang
             0xDB, b,                                                              // Dummy-Lesen
             0xDB, uint8_t(b + 1), 0x07, 0x76});                                   // RLCA: PB7 -> Carry
        return !(cpu.F & Z80::FLAG_C);
    }
    uint8_t status() { run({0xDB, uint8_t(b + 1), 0x76}); return cpu.A; }
    void send(uint8_t v) { run({0xCD, 0x00, 0x02, 0x76}, v); }
    uint8_t recv() { run({0xCD, 0x08, 0x02, 0x76}); return cpu.A; }
    void sendHL(uint16_t v) { send(v & 255); send(v >> 8); }
    uint16_t recv16() { const uint8_t l = recv(); return uint16_t(l | (recv() << 8)); }
    void blockOut(const std::string& d) {   // Stuecke bis 255 wie der Treiber (B = 0 waere 256)
        size_t o = 0;
        while (o < d.size()) {
            const size_t n = std::min<size_t>(d.size() - o, 255);
            for (size_t i = 0; i < n; ++i) ram[0x8000 + i] = uint8_t(d[o + i]);
            run({0x21, 0x00, 0x80, 0x06, uint8_t(n), 0x0E, b, 0xCD, 0x20, 0x02, 0x76});
            o += n;
        }
    }
    std::string blockIn(size_t cnt) {
        std::string r;
        while (r.size() < cnt) {
            const size_t n = std::min<size_t>(cnt - r.size(), 255);
            run({0x21, 0x00, 0x90, 0x06, uint8_t(n), 0x0E, b, 0xCD, 0x30, 0x02, 0x76});
            for (size_t i = 0; i < n; ++i) r.push_back(char(ram[0x9000 + i]));
        }
        return r;
    }

    // ---- Treiberprimitiven (TFTP 0186, 0199, 01BA, 01C3) ----
    void wr(uint16_t adr, uint8_t v) { send(4); sendHL(adr); send(v); }
    uint8_t rd(uint16_t adr) { send(5); sendHL(adr); return recv(); }
    void wr16(uint16_t adr, uint16_t w) { wr(adr, w >> 8); wr(adr + 1, w & 255); }
    uint16_t rd16(uint16_t adr) { const uint8_t h = rd(adr); return uint16_t((h << 8) | rd(adr + 1)); }
    void blockWrite(uint16_t adr, const std::string& d) {
        send(2); sendHL(adr);
        send(0); sendHL(uint16_t(d.size()));
        blockOut(d);
    }
    std::string blockRead(uint16_t adr, uint16_t cnt) {
        send(2); sendHL(adr);
        send(1); sendHL(cnt);
        return blockIn(cnt);
    }
};

}  // namespace

TEST(GunNet, ErkennungKarteDaUndVersion) {
    Rig r;
    ASSERT_TRUE(r.init()) << "PB7 muss nach dem Dummy-Lesen 0 sein";
    r.send(9);
    EXPECT_EQ(r.recv(), 2);   // Lo = Unterversion
    EXPECT_EQ(r.recv(), 1);   // Hi = Hauptversion (der Gast verlangt >= 1.2)
    r.send(0x0A);
    EXPECT_EQ(r.recv16(), 0x0100);   // Hardware 1.0: Lo = 0, Hi = 1
}

TEST(GunNet, OhneKarteLiestDerGastEinsenAnPb7) {
    // Kontrolle der Erkennung: ein offener Bus (Sonde = 5AH hat Bit 7 = 0, 0FFH hat 1) -> "nicht gefunden"
    K1520Bus bus;
    struct Offen : BusDevice {
        uint8_t ioRead(uint8_t) override { return 0xFF; }
        void ioWrite(uint8_t, uint8_t) override {}
        const char* deviceName() const override { return "offen"; }
    } offen;
    bus.registerIO(&offen, 0xC0, 4);
    EXPECT_TRUE(bus.ioRead(0xC1) & 0x80);
    Rig r;
    ASSERT_TRUE(r.init());
    EXPECT_FALSE(r.status() & 0x80);
}

TEST(GunNet, HandshakePb0NachOutEinsPb7NachInNull) {
    Rig r;
    ASSERT_TRUE(r.init());
    // OUT ohne Warteschleife: PB0 = 1 bis zur ersten Statusabfrage, danach 0
    r.run({0xD3, r.b, 0x76}, 9);
    EXPECT_EQ(r.status() & 1, 1);
    EXPECT_EQ(r.status() & 1, 0);
    // Antwort (2 Bytes): PB7 = 1 bei der Statusabfrage, nach IN (C0) sofort 0, danach das zweite Byte
    EXPECT_EQ(r.status() & 0x80, 0x80);
    r.run({0xDB, r.b, 0x76});
    EXPECT_EQ(r.cpu.A, 2);
    r.run({0xDB, uint8_t(r.b + 1), 0x76});
    EXPECT_EQ(r.cpu.A & 0x80, 0x80) << "zweites Byte";
    r.run({0xDB, r.b, 0x76});
    EXPECT_EQ(r.cpu.A, 1);
    EXPECT_EQ(r.status() & 0x80, 0);
}

TEST(GunNet, LinkKennungUndDns) {
    Rig r;
    ASSERT_TRUE(r.init());
    r.send(0x0B);
    EXPECT_EQ(r.recv(), 1);
    r.net.setzeLink(false);
    r.send(0x0B);
    EXPECT_EQ(r.recv(), 0);
    r.send(0x0C);
    std::string id;
    for (uint8_t c; (c = r.recv()) != 0;) id.push_back(char(c));
    EXPECT_EQ(id, r.net.config().id_string);
    r.send(7); r.send(0);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(r.recv(), 0) << "DNS-Vorgabe 0.0.0.0";
    r.send(6); r.send(0); r.send(10); r.send(0); r.send(0); r.send(1);
    r.send(7); r.send(0);
    EXPECT_EQ(r.recv(), 10); EXPECT_EQ(r.recv(), 0); EXPECT_EQ(r.recv(), 0); EXPECT_EQ(r.recv(), 1);
}

TEST(GunNet, UngueltigesKommandoWirdGezaehltUndVerworfen) {
    Rig r;
    ASSERT_TRUE(r.init());
    r.send(0x7F);
    r.send(0x0E);
    r.send(0x0B);                       // danach ist der Automat wieder in Ordnung
    EXPECT_EQ(r.recv(), 1);
    r.send(0x0D);
    EXPECT_EQ(r.recv16(), 2);
    EXPECT_EQ(r.net.zaehler().fehlerkommandos, 2u);
}

TEST(GunNet, EinzelbyteZugriffMitBit15AufW5100) {
    Rig r;
    ASSERT_TRUE(r.init());
    r.wr(0x8001, 0xC0);                 // GAR
    r.wr(0x8002, 0xA8);
    EXPECT_EQ(r.net.w5100().lese(reg::GAR), 0xC0);
    EXPECT_EQ(r.net.w5100().lese(reg::GAR + 1), 0xA8);
    EXPECT_EQ(r.rd(0x8001), 0xC0);
    EXPECT_EQ(r.rd(0x8017), 0x07) << "RTR-Vorgabe 07D0H";
    EXPECT_EQ(r.rd(0x8019), 0x08) << "RCR-Vorgabe";
}

TEST(GunNet, BlockSchreibenUndLesenUeberTxPufferUndRegister) {
    Rig r;
    ASSERT_TRUE(r.init());
    r.blockWrite(0x800F, std::string("\x0A\x00\x00\x07", 4));   // SIPR
    for (int i = 0; i < 4; ++i) EXPECT_EQ(r.net.w5100().lese(reg::SIPR + i), i == 0 ? 10 : (i == 3 ? 7 : 0));
    EXPECT_EQ(r.blockRead(0x800F, 4), std::string("\x0A\x00\x00\x07", 4));
    // Tx-Puffer Socket 1: Gast C000H + 800H -> W5100 4800H
    std::string d;
    for (int i = 0; i < 300; ++i) d.push_back(char(i * 7));   // laenger als 255: zwei Stuecke
    r.blockWrite(0xC800, d);
    for (int i = 0; i < 300; ++i) ASSERT_EQ(r.net.w5100().lese(uint16_t(0x4800 + i)), uint8_t(d[i])) << i;
    EXPECT_EQ(r.blockRead(0xC800, 300), d);
    EXPECT_EQ(r.net.zeiger(), uint16_t(0xC800 + 300)) << "Zeiger zaehlt im Block weiter";
}

TEST(GunNet, TimerLaeuftInMaschinenzeitMitUmlauf60000) {
    Rig r;
    r.net.setzeTaktHz(2'457'600);
    ASSERT_TRUE(r.init());
    r.net.takt(0);
    r.send(3);
    EXPECT_EQ(r.recv16(), 0);
    r.net.takt(2'457'600);              // genau eine Sekunde Maschinenzeit
    r.send(3);
    EXPECT_EQ(r.recv16(), 1000);
    r.net.takt(2'457'600ull * 61);      // 61000 ms -> Umlauf bei 60000
    r.send(3);
    EXPECT_EQ(r.recv16(), 1000);
    r.net.takt(2'457'600ull * 59 + 2'457'000);   // knapp unter 60 s
    r.send(3);
    EXPECT_LT(r.recv16(), 60000);
}

TEST(GunNet, DynamischerPortZaehltAb49152) {
    Rig r;
    ASSERT_TRUE(r.init());
    r.send(8);
    EXPECT_EQ(r.recv16(), 49152);
    r.send(8);
    EXPECT_EQ(r.recv16(), 49153);
}

TEST(GunNet, NeuinitialisierungWirftOffeneAntwortenWeg) {
    Rig r;
    ASSERT_TRUE(r.init());
    r.run({0xD3, r.b, 0x76}, 9);        // Firmware-Version angefordert, nicht abgeholt
    EXPECT_EQ(r.net.antwortenOffen(), 2u);
    ASSERT_TRUE(r.init()) << "nach der Neuinitialisierung wieder 'Karte da'";
    EXPECT_EQ(r.net.antwortenOffen(), 0u);
    // angefangenes Kommando (04 ohne Parameter) wird ebenfalls verworfen
    r.run({0xD3, r.b, 0x76}, 4);
    ASSERT_TRUE(r.init());
    r.send(0x0B);
    EXPECT_EQ(r.recv(), 1);
}

TEST(GunNet, ResetSetztW5100ZurueckRichtlinieBleibt) {
    Rig r;
    ASSERT_TRUE(r.init());
    r.wr(0x8001, 0x55);
    r.net.reset();
    EXPECT_EQ(r.net.w5100().lese(reg::GAR), 0);
    ASSERT_TRUE(r.init());
    r.send(0x0B);
    EXPECT_EQ(r.recv(), 1);
}

TEST(GunNet, BusAnmeldungUndKonflikt) {
    K1520Bus bus;
    Sonde s;
    bus.registerIO(&s, 0x89, 1);
    GunNet g;   // Vorgabe 88H
    EXPECT_NE(g.belegtGrund(bus).find("89H belegt"), std::string::npos);
    GunNet frei(GunNet::Config{0xC0});
    EXPECT_EQ(frei.belegtGrund(bus), "");
    frei.attachToBus(bus);
    EXPECT_EQ(bus.ioOwner(0xC3), &frei);
    EXPECT_EQ(bus.ioOwner(0xC4), nullptr);
    GunNet::Config schief;
    schief.basis = 0xC1;
    EXPECT_THROW(GunNet{schief}, std::invalid_argument);
}

TEST(GunNet, UdpRundreiseUeberDenW5100GegenLoopbackServer) {
    Rig r;
    ASSERT_TRUE(r.init());
    net::Fehler f;
    net::Socket peer = net::udpOeffnen(0, true, &f);
    ASSERT_TRUE(peer.gueltig());
    const int peerPort = net::lokalerPort(peer.fd());

    // Socket 0 als UDP oeffnen (TFTP 0617): MR, PORT, CR=OPEN, SR gegen 22H
    r.wr(0x8400, reg::PROTO_UDP);
    r.wr16(0x8404, 0);
    r.wr(0x8401, reg::CMD_OPEN);
    EXPECT_EQ(r.rd(0x8403), reg::SR_UDP);
    ASSERT_TRUE(warteAuf([&] { return r.net.w5100().wirtsPort(0) != 0; }));

    // Ziel 127.0.0.1:peerPort, "ping" in den Tx-Puffer (C000H), TX_WR nachziehen, SEND
    r.blockWrite(0x840C, std::string("\x7F\x00\x00\x01", 4));
    r.wr16(0x8410, uint16_t(peerPort));
    const uint16_t wrp = r.rd16(0x8424);
    EXPECT_EQ(wrp, 0);
    r.blockWrite(uint16_t(0xC000 + (wrp & 0x7FF)), "ping");
    r.wr16(0x8424, uint16_t(wrp + 4));
    r.wr(0x8401, reg::CMD_SEND);

    char buf[64];
    unsigned char ip[4];
    int port = 0;
    net::IoErgebnis e;
    ASSERT_TRUE(warteAuf([&] { e = net::udpEmpfangen(peer.fd(), buf, sizeof buf, ip, &port); return e.status == net::IoStatus::Ok; }));
    EXPECT_EQ(std::string(buf, e.n), "ping");
    EXPECT_EQ(port, r.net.w5100().wirtsPort(0));

    // Antwort des Servers: 8 Byte Kopf + "pong!" -> RX_RSR = 13, Daten ab E000H, RX_RD nachziehen, RECV
    const unsigned char loop[4] = {127, 0, 0, 1};
    net::udpSenden(peer.fd(), loop, r.net.w5100().wirtsPort(0), "pong!", 5);
    ASSERT_TRUE(warteAuf([&] { return r.net.w5100().lese16(reg::sock(0, reg::Sn_RX_RSR)) >= 13; }));
    EXPECT_EQ(r.rd16(0x8426), 13);
    const uint16_t rdp = r.rd16(0x8428);
    const std::string d = r.blockRead(uint16_t(0xE000 + (rdp & 0x7FF)), 13);
    EXPECT_EQ(d.substr(8), "pong!");
    EXPECT_EQ(uint8_t(d[0]), 127);
    r.wr16(0x8428, uint16_t(rdp + 13));
    r.wr(0x8401, reg::CMD_RECV);
    EXPECT_EQ(r.rd16(0x8426), 0);
    EXPECT_GT(r.net.zaehler().bytes_zum_gast, 13u);
}
