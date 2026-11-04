/**
 * @file test_p8000_ohne_terminal.cpp
 * @brief P8000 mit `TerminalArt::None` (Ticket #26): tty1 (Konsole) geht wie tty0/2/3 ueber den
 *        SerialHub nach aussen; kein Kern-/Originalterminal, Terminal-Abfragen verhalten sich
 *        wie bei "kein Terminal".
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>

#include "core/api/k1520_api.h"
#include "core/logger.h"
#include "core/machines/p8000/p8000.h"
#include "tests/support/p8000_input.h"
#include "tests/support/temp_path.h"

using namespace k1520test::p8000;
using namespace k1520::serial;

namespace {
void stumm() { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR); }

P8000Machine::Config ohneTerminal() {
    P8000Machine::Config c;
    c.terminal = P8000Machine::Config::TerminalArt::None;
    return c;
}

std::vector<std::string> hubNamen(P8000Machine& m) {
    std::vector<std::string> v;
    for (int i = 0; i < m.serialHub()->anzahl(); ++i) v.push_back(m.serialHub()->info(i).name);
    return v;
}
}  // namespace

TEST(P8000OhneTerminal, KonfigtextTerminalNone) {
    stumm();
    K1520Handle h = k1520_create_p8000("terminal=none");
    ASSERT_NE(h, nullptr) << k1520_last_init_error();
    EXPECT_EQ(k1520_term_count(h), 0);
    EXPECT_EQ(k1520_term_char(h, 0, 0, 0), 0);
    EXPECT_EQ(k1520_term_bell_count(h, 0), 0u);
    EXPECT_FALSE(k1520_term_key(h, 0, 'a', false, false));
    k1520_destroy(h);

    EXPECT_EQ(k1520_create_p8000("terminal=quatsch"), nullptr);
    EXPECT_NE(std::string(k1520_last_init_error()).find("none"), std::string::npos) << k1520_last_init_error();

    h = k1520_create_p8000("terminal=kern");
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(k1520_term_count(h), 1);
    k1520_destroy(h);
}

TEST(P8000OhneTerminal, Tty1StehtInNatuerlicherReihenfolgeAmHub) {
    stumm();
    P8000Machine m(ohneTerminal());
    EXPECT_FALSE(m.hatTerminal());
    EXPECT_TRUE(m.festeSchnittstellen().empty());
    EXPECT_EQ(hubNamen(m), (std::vector<std::string>{"tty0", "tty1 CONSOLE", "tty2", "tty3 PRINTER"}));
    EXPECT_EQ(m.bellCount(), 0u);
    EXPECT_EQ(m.screenChar(0, 0), 0);
    EXPECT_EQ(m.terminalZeile(0), "");
    EXPECT_THROW(m.konsole(), std::logic_error);
    EXPECT_THROW(m.terminal(), std::logic_error);

    // Mit Terminal bleibt es bei tty0, tty2, tty3.
    P8000Machine mt;
    EXPECT_EQ(hubNamen(mt), (std::vector<std::string>{"tty0", "tty2", "tty3 PRINTER"}));
    EXPECT_EQ(mt.festeSchnittstellen().size(), 1u);
}

TEST(P8000OhneTerminal, MonitorBootetUndSchreibtAnTty1InDieDatei) {
    stumm();
    P8000Machine m(ohneTerminal());
    const std::string pfad = k1520test::tempPath("p8000_tty1_aus.txt");
    std::remove(pfad.c_str());
    SerialHub* hub = m.serialHub();
    SerialKonfig k = hub->konfig(1);
    k.betriebsart = Betriebsart::Datei;
    k.datei = pfad;
    ASSERT_TRUE(hub->konfigurieren(1, k));
    ASSERT_TRUE(hub->start(1));
    m.powerOn();
    m.keyPress('a', false, false);   // ohne Terminal: wirkungslos, kein Absturz

    std::string inhalt;
    for (int i = 0; i < 400 && inhalt.find("Press RETURN") == std::string::npos; ++i) {
        laufe(m, 2'000'000);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::ifstream f(pfad, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        inhalt = ss.str();
    }
    hub->stop(1);
    std::remove(pfad.c_str());
    EXPECT_NE(inhalt.find("U880-Softwaremonitor"), std::string::npos) << inhalt;
    EXPECT_NE(inhalt.find("Press RETURN"), std::string::npos) << inhalt;
}

TEST(P8000OhneTerminal, SaveStateRundlaufUndArtMussPassen) {
    stumm();
    P8000Machine a(ohneTerminal()), b(ohneTerminal());
    a.powerOn(); b.powerOn();
    laufe(a, 3'000'000);
    const auto stand = a.stateBytes();
    ASSERT_TRUE(b.restoreStateBytes(stand)) << b.stateError();
    EXPECT_EQ(b.stateBytes(), stand);
    laufe(a, 1'000'000); laufe(b, 1'000'000);
    EXPECT_EQ(a.stateBytes(), b.stateBytes());

    // Andere Terminalart => abgewiesen, in beiden Richtungen.
    P8000Machine kern;
    kern.powerOn();
    const auto vorher = kern.stateBytes();
    EXPECT_FALSE(kern.restoreStateBytes(stand));
    EXPECT_EQ(kern.stateBytes(), vorher);
    EXPECT_FALSE(b.restoreStateBytes(vorher));
}
