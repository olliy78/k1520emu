/**
 * @file test_p8000_osm.cpp
 * @brief OS/M 2.0 und 3.0 (P8000, 8-Bit-Teil) booten von den ausgelieferten Systemdisketten bis `A:` —
 *        am eingebauten Kern-Terminal UND am Originalterminal an tty1.
 *
 * Der Urlader der OS/M-Version 2.0 (15.04.1987) prueft vor dem Start die Leitungen der Konsole: bleibt /DCD
 * an tty1 inaktiv ("Kabel ab"), meldet sich OS/M nie, und der Monitor steht still.  Beide Terminals sind
 * DEE am Kanal und muessen DTR (= /DCD, /DSR) und RTS (= /CTS) aktiv legen (`leitungenSetzen`, Merkposten
 * p8000 "Terminal und V.24-Leitungen").  Genau das fiel erst mit der 2.0-Diskette auf.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "core/logger.h"
#include "core/machines/p8000/p8000.h"
#include "tests/support/p8000_input.h"
#include "tests/support/temp_path.h"

namespace fs = std::filesystem;
using namespace k1520test::p8000;

namespace {
void stumm() { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR); }

/// Kaltstart von einer Temp-Kopie der Diskette: Monitor, zweimal RETURN, OS/M-Banner, Prompt `A:`.
bool bootetBisA(P8000Machine::Config::TerminalArt art, const char* diskette, std::string* bildOut) {
    const std::string kopie = k1520test::tempPath("k1520_osm_boot.hfe");
    std::error_code ec;
    fs::copy_file(diskette, kopie, fs::copy_options::overwrite_existing, ec);
    if (ec) { *bildOut = "Diskette kopieren: " + ec.message(); return false; }
    P8000Machine::Config c;
    c.terminal = art;
    bool ok = false;
    {
        P8000Machine m(c);
        if (!m.mountDisk(0, kopie, m.defaultFormatName(0), false)) {
            *bildOut = "mountDisk: " + m.lastError();
        } else {
            m.powerOn();
            ok = laufeBisText(m, "Press RETURN", 200'000'000);
            if (ok) { tippe(m, "\r"); ok = laufeBisPrompt(m, ">", 20'000'000); }
            if (ok) { tippe(m, "\r"); ok = laufeBisText(m, "OS/M", 400'000'000) && laufeBisPrompt(m, "A:", 200'000'000); }
            *bildOut = bild(m);
        }
    }
    fs::remove(kopie, ec);
    return ok;
}
using Art = P8000Machine::Config::TerminalArt;
}  // namespace

TEST(P8000Osm, Osm20BootetAmKernTerminal) {
    stumm();
    std::string b;
    EXPECT_TRUE(bootetBisA(Art::Kern, K1520_OSM20_DISK, &b)) << b;
    EXPECT_NE(b.find("OS/M"), std::string::npos) << b;
}

TEST(P8000Osm, Osm20BootetAmOriginalTerminal) {
    stumm();
    std::string b;
    EXPECT_TRUE(bootetBisA(Art::Original, K1520_OSM20_DISK, &b)) << b;
}

TEST(P8000Osm, Osm30BootetAmKernTerminal) {
    stumm();
    std::string b;
    EXPECT_TRUE(bootetBisA(Art::Kern, K1520_OSM30_DISK, &b)) << b;
}
