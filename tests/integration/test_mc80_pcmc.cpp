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

/// Ein Glied der EGOS-Kette, wie RS0 (egos.prn 092EH) sie liest.
struct Glied { uint16_t adr; uint8_t kennbyte; std::string name; };

/// ROM-Kette ab 0AFEH ablaufen wie RS0: Kennbyte >= 1DH = Ende, naechstes Glied = Adresse hinter
/// der Distanz + Distanz.  Abbruch an 4000H (RAM-Anfang, dort beginnt der Teil von INIT/RKET);
/// `ende` = Adresse, an der die ROM-Kette aufhoert.
std::vector<Glied> romKette(Mc80Machine& m, uint16_t& ende) {
    std::vector<Glied> r;
    uint16_t a = 0x0AFE;
    while (a < 0x4000 && r.size() < 64) {
        const uint8_t kb = m.memReadDebug(a);
        if (kb >= 0x1D) break;
        Glied g{a, kb, {}};
        for (uint16_t n = a + 3; m.memReadDebug(n) < 0x7F; ++n) g.name += char(m.memReadDebug(n));
        r.push_back(g);
        const uint16_t dist = m.memReadDebug(a + 1) | (m.memReadDebug(a + 2) << 8);
        const uint16_t weiter = static_cast<uint16_t>(a + 3 + dist);
        EXPECT_GT(weiter, a) << "Kette laeuft rueckwaerts bei " << std::hex << a;
        if (weiter <= a) break;
        a = weiter;
    }
    ende = a;
    return r;
}

std::vector<std::string> namen(const std::vector<Glied>& k) {
    std::vector<std::string> r;
    for (const auto& g : k) r.push_back(g.kennbyte == 0x01 || g.kennbyte == 0x02 ? g.name : "(" + std::to_string(g.kennbyte) + ")");
    return r;
}

const std::vector<std::string> ROM_KETTE = {
    "CON", "INIT", "(3)", "KLOG", "KINIT", "READ", "WRITE", "(3)", "(27)", "RELAD", "DIR", "LADT",
    "DEFINE", "ACT", "DEACT", "(3)", "CAT", "CATA", "RKET"};

}  // namespace

TEST(Mc80Pcmc, KetteOhneErweiterungFuehrtVonRketNach4000) {
    auto m = maschine(false);
    uint16_t ende = 0;
    const auto k = romKette(*m, ende);
    EXPECT_EQ(namen(k), ROM_KETTE);
    EXPECT_EQ(ende, 0x4000) << std::hex << ende;
    for (const auto& g : k) EXPECT_LT(g.adr, 0x2800) << g.name << " im Leerbereich";
}

TEST(Mc80Pcmc, KetteMitErweiterungFuehrtUeberPcmcNach4000) {
    auto m = maschine(true);
    uint16_t ende = 0;
    const auto k = romKette(*m, ende);
    auto soll = ROM_KETTE;
    soll.push_back("PCMC");
    EXPECT_EQ(namen(k), soll);
    ASSERT_FALSE(k.empty());
    EXPECT_EQ(k.back().adr, 0x2800);
    EXPECT_EQ(ende, 0x4000) << std::hex << ende;
}

TEST(Mc80Pcmc, CatFindetPcmcUndLaeuftBisZumPrompt) {
    Pc p(true);
    p.boot();
    p.tippe("cat\n", 0);
    for (int i = 0; i < 400 && p.m->textZeile(24) != ">"; ++i) p.fahre(100'000);
    EXPECT_EQ(p.m->textZeile(24), ">") << p.bild();
    EXPECT_TRUE(p.text("%01 %01.26CF")) << p.bild();   // RKET
    EXPECT_TRUE(p.text("PCMC")) << p.bild();
    EXPECT_TRUE(p.text("%01 %01.2800")) << p.bild();
}

TEST(Mc80Pcmc, OhneErweiterungKeinPcmc) {
    Pc p(false);
    EXPECT_EQ(p.m->memReadDebug(0x2800), 0xFF);
    EXPECT_EQ(p.m->memReadDebug(0x26D1), 0x19);   // RKET kettet nach 4000H
}

TEST(Mc80Pcmc, MitErweiterungStehtPcmcImEprom) {
    Pc p(true);
    EXPECT_EQ(p.m->memReadDebug(0x2800), 0x01);   // Kennbyte
    EXPECT_EQ(p.m->memReadDebug(0x2803), 'P');
    EXPECT_EQ(p.m->memReadDebug(0x2807), 0xFF);
    // Zum Umbau gehoert das getauschte 2400H-EPROM: RKET kettet auf PCMC (26D2H + 012EH),
    // PCMC weiter auf den RAM-Anfang (2803H + 17FDH).
    EXPECT_EQ(p.m->memReadDebug(0x26D1), 0x01);
    EXPECT_EQ(p.m->memReadDebug(0x2801) | (p.m->memReadDebug(0x2802) << 8), 0x4000 - 0x2803);
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
