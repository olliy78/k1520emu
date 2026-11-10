/**
 * @file test_gun_usb.cpp
 * @brief GUN-USB (Entwurf 50 Abschnitt 5, AP U2): Z80-PIO + VDIP1 mit echtem Z80.  Die Gastroutinen sind
 *        USBINC.Z80 (UTools 1.4) von Hand uebersetzt: Init (Port B Bitbetrieb 33h, Port A Betriebsart 2),
 *        get (RD# aktiv, IN PIOA, RD# inaktiv), put (OUT PIOA, TXE# abwarten, WR-Impuls).
 */
#include <gtest/gtest.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "core/cards/gun_usb/gun_usb.h"
#include "core/primitives/z80.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;
using k1520::GunUsb;

namespace {

std::string by(std::initializer_list<int> l) {
    std::string s;
    for (int v : l) s.push_back(static_cast<char>(v));
    return s;
}
std::string num(uint32_t n) { return by({int(n >> 24), int((n >> 16) & 255), int((n >> 8) & 255), int(n & 255)}); }
const std::string SCS = by({0x10}), IPH = by({0x91}), OP_DIR = by({0x01}), OP_WRF = by({0x08}), OP_OPW = by({0x09}),
                  OP_CLF = by({0x0A}), OP_RDF = by({0x0B}), OP_OPR = by({0x0E});
const std::string OK_K = ">\r";

std::string lies(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream o;
    o << f.rdbuf();
    return o.str();
}
void schreib(const fs::path& p, const std::string& d) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << d;
}

struct Sonde : BusDevice {
    uint8_t ioRead(uint8_t) override { return 0x5A; }
    void ioWrite(uint8_t, uint8_t) override {}
    const char* deviceName() const override { return "Sonde"; }
};

struct Rig {
    fs::path wurzel;
    K1520Bus bus;
    GunUsb usb;
    Z80 cpu;
    std::array<uint8_t, 65536> ram{};
    uint8_t b;   // Basis

    static fs::path neu() {
        const std::string n = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        fs::path p = k1520test::tempPath("k1520_gunusb_" + n);
        std::error_code ec;
        fs::remove_all(p, ec);
        fs::create_directories(p);
        return p;
    }
    static GunUsb::Config cfg(uint8_t basis, const fs::path& stick, bool mitStick) {
        GunUsb::Config c;
        c.basis = basis;
        if (mitStick) c.ordner = stick;
        return c;
    }
    explicit Rig(uint8_t basis = 0x84, bool mitStick = true)
        : wurzel(neu()), usb(cfg(basis, wurzel, mitStick)), b(basis) {
        usb.attachToBus(bus);
        cpu.readByte  = [this](uint16_t a) { return ram[a]; };
        cpu.writeByte = [this](uint16_t a, uint8_t d) { ram[a] = d; };
        cpu.readPort  = [this](uint16_t p) { return bus.ioRead(p); };
        cpu.writePort = [this](uint16_t p, uint8_t d) { bus.ioWrite(p, d); };
    }
    ~Rig() { std::error_code ec; fs::remove_all(wurzel, ec); }

    void run(const std::vector<uint8_t>& code, uint8_t a = 0) {
        for (size_t i = 0; i < code.size(); ++i) ram[0x100 + i] = code[i];
        cpu.reset();
        cpu.PC = 0x100; cpu.A = a;
        for (int i = 0; i < 200000 && !cpu.halted; ++i) cpu.step();
        ASSERT_TRUE(cpu.halted) << "Programm hat nicht angehalten";
    }

    // ---- die Gastroutinen aus USBINC.Z80 ----
    void init() {
        const uint8_t pa = b, pb = b + 1, pas = b + 2, pbs = b + 3;
        run({0x3E, 0xCF, 0xD3, pbs,   // Bitbetrieb
             0x3E, 0x33, 0xD3, pbs,   // B0,B1,B4,B5 Eingaenge
             0x3E, 0x07, 0xD3, pbs,   // kein INT
             0x3E, 0xC4, 0xD3, pb,    // PRG#=1 RES#=1 RD#/WR inaktiv
             0x3E, 0x8F, 0xD3, pas,   // bidirektional
             0x3E, 0x07, 0xD3, pas,   // kein INT
             0xDB, pa,                // Dummy-Eingabe
             0x76});
    }
    uint8_t status() { run({0xDB, uint8_t(b + 1), 0x76}); return cpu.A; }
    uint8_t get() {
        const uint8_t pa = b, pb = b + 1;
        run({0x3E, 0xC0, 0xD3, pb, 0xDB, pa, 0x4F, 0x3E, 0xC4, 0xD3, pb, 0x79, 0x76});
        return cpu.A;
    }
    void put(uint8_t v) {
        const uint8_t pa = b, pb = b + 1;
        run({0xD3, pa, 0xF5, 0xDB, pb, 0x0F, 0x0F, 0x38, 0xFA, 0x3E, 0xCC, 0xD3, pb, 0x3E, 0xC4, 0xD3, pb, 0xF1, 0x76}, v);
    }
    void tx(const std::string& s) { for (unsigned char c : s) put(c); }
    /// Alles abholen, solange RXF# = 0 (Bit 0 von PIOB).
    std::string rx() {
        std::string r;
        while (!(status() & 1)) r.push_back(char(get()));
        return r;
    }
    std::string cmd(const std::string& s) { tx(s + "\r"); return rx(); }

    // UPUT: OPW, WRF in 128er-Bloecken, CLF
    void hochladen(const std::string& name, const std::string& d) {
        ASSERT_EQ(cmd(OP_OPW + " " + name), OK_K);
        for (size_t o = 0; o < d.size(); o += 128) {
            const size_t n = std::min<size_t>(128, d.size() - o);
            tx(OP_WRF + " " + num(uint32_t(n)) + "\r" + d.substr(o, n));
            ASSERT_EQ(rx(), OK_K) << "Block bei " << o;
        }
        ASSERT_EQ(cmd(OP_CLF + " " + name), OK_K);
    }
    // UGET: DIR name (Groesse), OPR, RDF-Schleife, CLF
    std::string herunter(const std::string& name) {
        const std::string a = cmd(OP_DIR + " " + name);
        const size_t sp = a.find(' ');
        EXPECT_NE(sp, std::string::npos) << a;
        if (sp == std::string::npos) return {};
        uint32_t groesse = 0;
        for (int i = 3; i >= 0; --i) groesse = (groesse << 8) | uint8_t(a[sp + 1 + i]);
        EXPECT_EQ(cmd(OP_OPR + " " + name), OK_K);
        std::string d;
        for (uint32_t rest = groesse; rest > 0;) {
            const uint32_t n = std::min<uint32_t>(128, rest);
            const std::string r = cmd(OP_RDF + " " + num(n));
            EXPECT_EQ(r.size(), n + 2u);
            if (r.size() != n + 2u) break;
            d += r.substr(0, n);
            rest -= n;
        }
        EXPECT_EQ(cmd(OP_CLF + " " + name), OK_K);
        return d;
    }
    /// Init + Sync nach USBINC.Z80: alles Anstehende abholen, `CR`, `E CR`, `e CR`.
    void sync(bool mitStick = true) {
        init();
        rx();
        EXPECT_EQ(cmd(""), mitStick ? "D:\\>\r" : "No Disk\r");   // Prompt = Status (ECS-Vorgabemodus)
        EXPECT_EQ(cmd("E"), "E\r");
        EXPECT_EQ(cmd("e"), "e\r");
    }
};

std::string muster(size_t n) {
    std::string s(n, '\0');
    for (size_t i = 0; i < n; ++i) s[i] = char((i * 131 + 7 + (i >> 8)) & 0xFF);
    return s;
}

}  // namespace

class GunUsbBasis : public ::testing::TestWithParam<int> {};

TEST_P(GunUsbBasis, InitSyncDirUndDateienRundlauf) {
    Rig r{uint8_t(GetParam())};
    schreib(r.wurzel / "HALLO.TXT", "Hallo Welt");
    r.init();
    // Nach dem Init steht die Startmeldung an (RES#-Impuls im Init), nichts ging verloren und nichts wurde gesendet.
    EXPECT_EQ(r.usb.zaehler().geschrieben, 0u);
    EXPECT_EQ(r.usb.zaehler().resets, 1u);
    EXPECT_EQ(r.rx().substr(0, 8), "Ver03.68");
    EXPECT_EQ(r.status() & 3, 1);   // RXF# = 1 (leer), TXE# = 0 (bereit)
    EXPECT_EQ(r.cmd(""), "D:\\>\r");
    EXPECT_EQ(r.cmd("E"), "E\r");
    EXPECT_EQ(r.cmd("e"), "e\r");
    // DIR im Kurzsatz wie UDIR
    EXPECT_EQ(r.cmd(SCS), OK_K);
    EXPECT_EQ(r.cmd(IPH), OK_K);
    const std::string dir = r.cmd(OP_DIR);
    EXPECT_NE(dir.find("HALLO.TXT"), std::string::npos) << dir;
    // Rundlauf wie UPUT/UGET: hoch (mehrere Bloecke), Datei im Wirtsverzeichnis bitgleich, wieder herunter
    const std::string d = muster(1000);
    r.hochladen("PUT.BIN", d);
    EXPECT_EQ(lies(r.wurzel / "PUT.BIN"), d);
    EXPECT_EQ(r.herunter("PUT.BIN"), d);
    EXPECT_EQ(r.herunter("HALLO.TXT"), "Hallo Welt");
    EXPECT_GT(r.usb.zaehler().gelesen, 1000u);
    EXPECT_GT(r.usb.zaehler().geschrieben, 1000u);
}

INSTANTIATE_TEST_SUITE_P(Basen, GunUsbBasis, ::testing::Values(0x84, 0xDC, 0x10));

TEST(GunUsb, SyncWieUsbinc) {
    Rig r;
    r.sync();
    EXPECT_EQ(r.usb.zaehler().resets, 1u);
}

TEST(GunUsb, PegelRxfTxe) {
    Rig r;
    r.init();
    EXPECT_EQ(r.status() & 1, 0);          // Startmeldung: RXF# = 0
    r.rx();
    EXPECT_EQ(r.status() & 3, 1);          // leer: RXF# = 1, TXE# = 0
    r.put('E'); r.put('\r');
    EXPECT_EQ(r.status() & 1, 0);          // Antwort da
}

TEST(GunUsb, LesenOhneDatumAendertNichts) {
    Rig r;
    r.init();
    r.rx();
    const auto z0 = r.usb.zaehler();
    r.get();                               // RD#-Impuls bei leerem FIFO
    EXPECT_EQ(r.usb.zaehler().gelesen, z0.gelesen);
    EXPECT_EQ(r.status() & 1, 1);
}

TEST(GunUsb, ResImpulsStartetFirmwareNeu) {
    Rig r;
    r.sync();
    EXPECT_EQ(r.cmd(SCS), OK_K);           // Kurzsatz aktiv
    ASSERT_TRUE(r.usb.vdip().kurzSatz());
    const uint8_t pb = r.b + 1;
    r.run({0x3E, 0x84, 0xD3, pb, 0x3E, 0xC4, 0xD3, pb, 0x76});   // RES# low, dann wieder high
    EXPECT_FALSE(r.usb.vdip().kurzSatz());
    EXPECT_EQ(r.usb.zaehler().resets, 2u);
    EXPECT_EQ(r.rx().substr(0, 8), "Ver03.68");
}

TEST(GunUsb, StickWechselZeigtSichOhneZwischenzugriff) {
    Rig r(0x84, false);
    r.sync(false);
    EXPECT_EQ(r.status() & 1, 1);
    std::string f;
    ASSERT_TRUE(r.usb.stickEinstecken(r.wurzel, false, &f)) << f;
    EXPECT_EQ(r.status() & 1, 0);          // "Device Detected" steht ohne weiteren Kartenzugriff an
    EXPECT_NE(r.rx().find("Device Detected"), std::string::npos);
    r.usb.stickZiehen();
    EXPECT_NE(r.rx().find("Device Removed"), std::string::npos);
    EXPECT_FALSE(r.usb.vdip().stickVorhanden());
}

TEST(GunUsb, SchreibgeschuetzterStickLehntSchreibenAb) {
    Rig r;
    r.sync();
    std::string f;
    ASSERT_TRUE(r.usb.stickEinstecken(r.wurzel, true, &f)) << f;
    r.rx();
    const std::string a = r.cmd(OP_OPW + " X.BIN");
    EXPECT_NE(a, OK_K);
    EXPECT_FALSE(fs::exists(r.wurzel / "X.BIN"));
}

TEST(GunUsb, ResetDerKarteLaesstStickGesteckt) {
    Rig r;
    r.sync();
    r.usb.reset();
    EXPECT_TRUE(r.usb.vdip().stickVorhanden());
    r.init();
    EXPECT_EQ(r.rx().substr(0, 8), "Ver03.68");
}

TEST(GunUsb, BusAnmeldungUndKonflikt) {
    K1520Bus bus;
    Sonde s;
    bus.registerIO(&s, 0x86, 1);
    GunUsb g;
    EXPECT_NE(g.belegtGrund(bus).find("86H belegt"), std::string::npos);
    GunUsb frei(GunUsb::Config{0x90, {}, false, true});
    EXPECT_EQ(frei.belegtGrund(bus), "");
    frei.attachToBus(bus);
    EXPECT_EQ(bus.ioOwner(0x93), &frei);
    EXPECT_EQ(bus.ioOwner(0x94), nullptr);
    EXPECT_THROW(GunUsb(GunUsb::Config{0x85, {}, false, true}), std::invalid_argument);
}
