/**
 * @file test_mc80_pcmc.cpp
 * @brief MC80.30 mit dem EPROM „mit PCMC-Erweiterung“ (2800H, Issue #24): Datenaustausch PC <-> MC80
 *        ueber die IFSS (ASP SIO B).  Die PC-Seite spielt der Test selbst, direkt am `SerialAnschluss`
 *        (ohne Wandler/Hub); Protokoll nach doc/mc80/pcmc.md.
 *
 * - `Mc80Pcmc.OhneErweiterungKeinPcmc` / `MitErweiterungStehtPcmcImEprom`: das EPROM ist Option.
 * - `Mc80Pcmc.EmpfangVomPcLaedtInDenRam`: Menue `E`, Kopf (Start/Laenge) und Daten mit Echo je Byte.
 * - `Mc80Pcmc.SendenZumPcLiefertDenRam`: Menue `S`, PC nennt Start/Ende und bestaetigt jedes Byte.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <chrono>
#include <thread>

#include "core/machines/mc80/mc80.h"
#include "core/serial/hub.h"
#include "core/serial/net/socket.h"

using namespace k1520::serial;

namespace {

std::unique_ptr<Mc80Machine> maschine(bool pcmc) {
    Mc80Machine::Config c;
    c.pcmc = pcmc;
    return std::make_unique<Mc80Machine>(c);
}

/// Die PC-Seite: ein Telnet-Client auf dem Hub-Anschluss „IFSS“ (Server auf Port 0).  Telnet mit
/// BINARY in beide Richtungen, damit 0DH/FFH unverfaelscht durchgehen (wie `app/pcmc.py`).
struct Pc {
    std::unique_ptr<Mc80Machine> m;
    net::Socket s;
    std::vector<uint8_t> rx;       ///< dekodierte Nutzbytes von der Maschine
    enum { Daten, Iac, Verb } z = Daten;

    explicit Pc(bool pcmc) : m(maschine(pcmc)) {}

    void verbinden() {
        auto* hub = m->serialHub();
        SerialKonfig k = hub->konfig(0);
        k.betriebsart = Betriebsart::Telnet;
        k.rolle = Rolle::Server;
        k.port = 0;
        ASSERT_TRUE(hub->konfigurieren(0, k));
        ASSERT_TRUE(hub->start(0));
        net::Fehler fe;
        auto ziele = net::aufloesen("127.0.0.1", hub->status(0).port_aktiv, &fe);
        ASSERT_FALSE(ziele.empty());
        s = net::verbindenAlle(ziele, 2000, &fe);
        ASSERT_TRUE(s.gueltig());
        const uint8_t neg[] = {255, 251, 0, 255, 253, 0};   // WILL BINARY, DO BINARY
        net::senden(s.fd(), neg, sizeof neg);
    }
    void einlesen() {
        uint8_t buf[256];
        const auto e = net::empfangen(s.fd(), buf, sizeof buf);
        if (e.status != net::IoStatus::Ok) return;
        for (size_t i = 0; i < e.n; ++i) {
            const uint8_t b = buf[i];
            switch (z) {
                case Daten: if (b == 255) z = Iac; else rx.push_back(b); break;
                case Iac:   if (b == 255) { rx.push_back(b); z = Daten; }
                            else z = (b >= 251 && b <= 254) ? Verb : Daten; break;
                case Verb:  z = Daten; break;
            }
        }
    }
    void fahre(uint64_t takte) {
        const uint64_t ziel = m->totalCycles() + takte;
        while (m->totalCycles() < ziel) m->run(2000);
    }
    void tippe(const char* text, uint64_t nach) {
        m->tastatur().tippe(text);
        while (!m->tastatur().leer()) m->run(5000);
        fahre(nach);
    }
    std::string bild() const {
        std::string r;
        for (int i = 0; i < 25; ++i) r += std::to_string(i) + ": " + m->textZeile(i) + "\n";
        return r;
    }
    bool text(const std::string& t) const {
        for (int i = 0; i < 25; ++i)
            if (m->textZeile(i).find(t) != std::string::npos) return true;
        return false;
    }
    void boot() {
        fahre(4'000'000);
        tippe("init", 0);
        tippe("\n", 4'000'000);
    }
    void sende(uint8_t b) {
        ASSERT_GT(net::senden(s.fd(), &b, 1).n, 0u);
    }
    /// Auf ein Byte der Maschine warten (Maschinenlauf in kleinen Schritten, Netz dazwischen).
    int hole() {
        for (int i = 0; i < 200'000; ++i) {
            if (!rx.empty()) { const int b = rx.front(); rx.erase(rx.begin()); return b; }
            m->run(2000);
            einlesen();
            if (rx.empty() && i % 64 == 63) std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        return -1;
    }
};

}  // namespace

TEST(Mc80Pcmc, OhneErweiterungKeinPcmc) {
    Pc p(false);
    EXPECT_EQ(p.m->memReadDebug(0x2800), 0x00);
}

TEST(Mc80Pcmc, MitErweiterungStehtPcmcImEprom) {
    Pc p(true);
    EXPECT_EQ(p.m->memReadDebug(0x2800), 0x01);   // Kennbyte
    EXPECT_EQ(p.m->memReadDebug(0x2803), 'P');
    EXPECT_EQ(p.m->memReadDebug(0x2807), 0xFF);
}

TEST(Mc80Pcmc, EmpfangVomPcLaedtInDenRam) {
    Pc p(true);
    p.boot();
    p.verbinden();
    p.tippe("pcmc", 0);
    p.tippe("\n", 3'000'000);
    ASSERT_TRUE(p.text("HAUPTMENUE")) << "PCMC meldet sich";
    p.tippe("e", 3'000'000); 
    ASSERT_TRUE(p.text("Senden auf PC")) << p.bild();

    // Datei: Start 8000H, Laenge-1 = 0x1F  -> 32 Byte.
    std::vector<uint8_t> daten;
    for (int i = 0; i < 32; ++i) daten.push_back(static_cast<uint8_t>(0xA0 + i));
    const std::vector<uint8_t> kopf = {0x00, 0x80, 0x1F, 0x00};
    for (uint8_t b : kopf) {
        p.sende(b);
        ASSERT_EQ(p.hole(), b) << "Echo des Kopfbytes\n" << p.bild();
    }
    for (uint8_t b : daten) {
        p.sende(b);
        ASSERT_EQ(p.hole(), b) << "Echo";
    }
    p.fahre(3'000'000);
    EXPECT_TRUE(p.text("Empfang beendet"));
    for (size_t i = 0; i < daten.size(); ++i)
        EXPECT_EQ(p.m->memReadDebug(static_cast<uint16_t>(0x8000 + i)), daten[i]) << i;
}

TEST(Mc80Pcmc, SendenZumPcLiefertDenRam) {
    Pc p(true);
    p.boot();
    p.verbinden();
    for (int i = 0; i < 16; ++i) p.m->memWriteDebug(static_cast<uint16_t>(0x9000 + i),
                                                    static_cast<uint8_t>(0x30 + i));
    p.tippe("pcmc", 0);
    p.tippe("\n", 3'000'000);
    ASSERT_TRUE(p.text("HAUPTMENUE"));
    p.tippe("s", 3'000'000);
    ASSERT_TRUE(p.text("Empfang auf PC")) << "Senden laeuft";

    // PC nennt Start 9000H und Ende 900FH (ohne Echo).
    for (uint8_t b : {0x00, 0x90, 0x0F, 0x90}) p.sende(b);
    std::vector<uint8_t> got;
    for (int i = 0; i < 16; ++i) {
        const int b = p.hole();
        ASSERT_GE(b, 0);
        got.push_back(static_cast<uint8_t>(b));
        p.sende(static_cast<uint8_t>(b));   // Bestaetigung
    }
    p.fahre(3'000'000);
    EXPECT_TRUE(p.text("Sendung beendet"));
    for (int i = 0; i < 16; ++i) EXPECT_EQ(got[static_cast<size_t>(i)], 0x30 + i);
}
