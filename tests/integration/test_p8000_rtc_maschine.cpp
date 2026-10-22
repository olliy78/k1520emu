/**
 * @file test_p8000_rtc_maschine.cpp
 * @brief Echtzeituhr-Option in der P8000-Maschine (Entwurf 37, AP R1/R2): nur mit 16-Bit-Karte,
 *        Save-State als eigener Abschnitt (Stand 4 bleibt, alte Staende laden weiter).
 */

#include <gtest/gtest.h>
#include "core/machines/p8000/p8000.h"
#include "tests/support/temp_path.h"
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

namespace {

// Tage seit 1970 (Hinnant), wie im Kartentest.
int64_t utc(int j, int mo, int t, int h = 0, int mi = 0, int s = 0) {
    const int64_t y = j - (mo <= 2);
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + t - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (era * 146097 + doe - 719468) * 86400 + h * 3600 + mi * 60 + s;
}

// ── Maschine: Konfiguration und Zustand ───────────────────────────────────────

TEST(P8000RtcMaschine, NurMitKarte16) {
    P8000Machine::Config c;
    c.karte16 = false;
    c.rtc = P8000Machine::Config::Rtc::Rtc72421;
    EXPECT_THROW(P8000Machine m(c), std::invalid_argument);
}

TEST(P8000RtcMaschine, U130XNurMitKarte16UndKennungAmBus) {
    P8000Machine::Config c;
    c.karte16 = false;
    c.rtc = P8000Machine::Config::Rtc::U130X;
    EXPECT_THROW(P8000Machine m(c), std::invalid_argument);
    c.karte16 = true;
    c.rtc_utc = true;
    P8000Machine m(c);
    ASSERT_NE(m.karte16()->rtc(), nullptr);
    EXPECT_EQ(m.karte16()->rtc()->typ(), P8000RtcTyp::U130X);
    Z8kBusCycle z; z.st = Z8kStatus::Io; z.system = true; z.word = false; z.read = true; z.addr = 0xFEF1;
    EXPECT_EQ(uint8_t(m.karte16()->busLesen(z)), 0xBF);   // Kennung des Originals
}

TEST(P8000RtcMaschine, ZustandRundreiseUndAlterStandOhneUhr) {
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
    P8000Machine::Config c;
    c.karte16 = true;
    c.rtc = P8000Machine::Config::Rtc::Rtc72421;
    c.rtc_utc = true;
    c.rtc_zeitquelle = [&] { return jetzt; };
    P8000Machine a(c);
    ASSERT_NE(a.karte16()->rtc(), nullptr);
    // Uhr stellen (Versatz +1 h), Zustand sichern, in einer zweiten Maschine laden
    auto out = [&](P8000Machine& m, uint16_t p, uint8_t d) {
        Z8kBusCycle z; z.st = Z8kStatus::Io; z.system = true; z.word = false; z.read = false; z.addr = p;
        m.karte16()->busSchreiben(z, uint16_t(d << 8 | d));
    };
    out(a, 0xFDFF, 0x07);
    out(a, 0xFDE5, 5);    // Minute 5 …
    out(a, 0xFDE7, 0);
    out(a, 0xFDFF, 0x04);
    const int64_t vs = a.karte16()->rtc()->versatz();
    EXPECT_NE(vs, 0);
    const std::vector<uint8_t> b = a.stateBytes();
    P8000Machine z(c);
    ASSERT_EQ(z.karte16()->rtc()->versatz(), 0);
    ASSERT_TRUE(z.restoreStateBytes(b)) << z.stateError();
    EXPECT_EQ(z.karte16()->rtc()->versatz(), vs);
    // Stand OHNE Uhr (aeltere Fassung) laedt in eine Maschine MIT Uhr; der Versatz bleibt
    P8000Machine::Config o = c;
    o.rtc = P8000Machine::Config::Rtc::Aus;
    P8000Machine ohne(o);
    const std::vector<uint8_t> alt = ohne.stateBytes();
    EXPECT_EQ(P8000Machine::P8000_STAND, 4);   // Version bleibt: die Uhr ist ein eigener, ueberspringbarer Abschnitt
    ASSERT_TRUE(z.restoreStateBytes(alt)) << z.stateError();
    EXPECT_EQ(z.karte16()->rtc()->versatz(), vs);
    // Stand MIT Uhr in eine Maschine OHNE Uhr: der Abschnitt wird uebersprungen
    ASSERT_TRUE(ohne.restoreStateBytes(b)) << ohne.stateError();
}

// ── Maschinenzeit (AP R2b) ────────────────────────────────────────────────────

TEST(P8000RtcMaschine, UhrZaehltInMaschinenzeitUndStehtBeiPause) {
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
    P8000Machine::Config c;
    c.karte16 = true;
    c.rtc = P8000Machine::Config::Rtc::Rtc72421;
    c.rtc_utc = true;
    c.rtc_zeitquelle = [&] { return jetzt; };
    P8000Machine m(c);
    ASSERT_NE(m.karte16()->rtc(), nullptr);
    const int64_t z0 = m.karte16()->rtc()->gastZeit();
    EXPECT_EQ(z0, jetzt);
    int64_t n = 0;
    while (n < 3 * 4'000'000) n += m.run(100'000);   // ~3 s Maschinenzeit
    const int64_t z1 = m.karte16()->rtc()->gastZeit();
    EXPECT_EQ(z1 - z0, 3) << "gelaufene Takte " << n;
    jetzt += 100000;                                  // Wirtszeit laeuft, Maschine steht
    EXPECT_EQ(m.karte16()->rtc()->gastZeit(), z1);
}

TEST(P8000RtcMaschine, BatteriedateiBeimZerstoerenUndZwischenstandSchreibtSieFort) {
    const std::string datei = k1520test::tempPath("k1520_test_rtc_maschine.txt");
    std::remove(datei.c_str());
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
    P8000Machine::Config c;
    c.karte16 = true;
    c.rtc = P8000Machine::Config::Rtc::U130X;
    c.rtc_utc = true;
    c.rtc_zustand = datei;
    c.rtc_zeitquelle = [&] { return jetzt; };
    std::vector<uint8_t> zwischen;
    int64_t stand = 0, ende = 0;
    {
        P8000Machine m(c);
        int64_t n = 0;
        while (n < 2 * 4'000'000) n += m.run(100'000);
        stand = m.karte16()->rtc()->gastZeit();
        zwischen = m.stateBytes();
        n = 0;
        while (n < 4'000'000) n += m.run(100'000);   // danach weiter - der Zwischenstand bleibt zurueck
        ende = m.karte16()->rtc()->gastZeit();
    }
    auto stand_der_datei = [&] {
        std::ifstream f(datei);
        std::stringstream ss; ss << f.rdbuf();
        const std::string t = ss.str();
        const size_t p = t.find("stand=");
        return p == std::string::npos ? int64_t(-1) : std::stoll(t.substr(p + 6));
    };
    EXPECT_EQ(stand_der_datei(), ende);   // beim Zerstoeren geschrieben
    // Neustart (Wirtszeit stand): nie zurueck; Zwischenstand laden stellt den Stand wieder her wie er war
    P8000Machine m2(c);
    EXPECT_EQ(m2.karte16()->rtc()->gastZeit(), ende);
    ASSERT_TRUE(m2.restoreStateBytes(zwischen)) << m2.stateError();
    EXPECT_EQ(m2.karte16()->rtc()->gastZeit(), stand);
    EXPECT_EQ(stand_der_datei(), stand);       // die Batteriedatei folgt dem geladenen Stand
    std::remove(datei.c_str());
}

// ── Netz-Aus/Ein, Reset, Neubau (AP R3b) ──────────────────────────────────────

namespace {
P8000Machine::Config r3bConfig(int64_t& jetzt, const std::string& datei) {
    P8000Machine::Config c;
    c.karte16 = true;
    c.rtc = P8000Machine::Config::Rtc::U130X;
    c.rtc_utc = true;
    c.rtc_zustand = datei;
    c.rtc_zeitquelle = [&jetzt] { return jetzt; };
    return c;
}
void laufe(P8000Machine& m, int sek) {
    int64_t n = 0;
    while (n < int64_t(sek) * 4'000'000) n += m.run(100'000);
}
}  // namespace

TEST(P8000RtcMaschine, NetzAusEinLaesstDieUhrUmDieAusDauerWeiterlaufen) {
    for (const bool mit_datei : {true, false}) {
        SCOPED_TRACE(mit_datei ? "mit Batteriedatei" : "nur im Speicher");
        const std::string datei = mit_datei ? k1520test::tempPath("k1520_test_rtc_netzaus.txt") : "";
        if (mit_datei) std::remove(datei.c_str());
        int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
        P8000Machine m(r3bConfig(jetzt, datei));
        laufe(m, 3);
        const int64_t stand = m.karte16()->rtc()->gastZeit();
        EXPECT_EQ(stand, utc(2026, 10, 22, 14, 35, 49));
        m.powerOff();
        jetzt += 3600;                                    // eine Stunde aus
        // Anzeige der Oberflaeche (Startregel ohne Maschine) und echtes Einschalten muessen gleich sein.
        if (mit_datei) EXPECT_EQ(P8000Rtc::startzeit(datei, 0, true, [&] { return jetzt; }), stand + 3600);
        m.powerOn();
        EXPECT_EQ(m.karte16()->rtc()->gastZeit(), stand + 3600);
        if (mit_datei) EXPECT_EQ(P8000Rtc::startzeit(datei, 0, true, [&] { return jetzt; }), stand + 3600);

        // Wirtszeit springt zurueck waehrend aus: die Uhr geht nie zurueck.
        laufe(m, 2);
        const int64_t st2 = m.karte16()->rtc()->gastZeit();
        m.powerOff();
        jetzt -= 7200;
        m.powerOn();
        EXPECT_EQ(m.karte16()->rtc()->gastZeit(), std::max(st2, jetzt));
        EXPECT_GE(m.karte16()->rtc()->gastZeit(), st2);
        std::remove(datei.c_str());
    }
}

TEST(P8000RtcMaschine, KaltstartOhneNetzAusUndResetLassenDieUhrUnberuehrt) {
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
    P8000Machine m(r3bConfig(jetzt, ""));
    laufe(m, 5);
    const int64_t stand = m.karte16()->rtc()->gastZeit();
    jetzt += 1000;                    // Wirtszeit laeuft, die Maschine steht
    m.reset();                        // Taste /RESP
    EXPECT_EQ(m.karte16()->rtc()->gastZeit(), stand);
    m.powerOn();                      // Kaltstart der laufenden Maschine, kein Netz-Aus davor
    EXPECT_EQ(m.karte16()->rtc()->gastZeit(), stand);
    laufe(m, 2);
    // (Wie weit die Karte16 nach dem Kaltstart nachzieht, ist nicht Sache der Uhr - nur: kein Sprung um die Wirtszeit.)
    EXPECT_GE(m.karte16()->rtc()->gastZeit(), stand + 2);
    EXPECT_LT(m.karte16()->rtc()->gastZeit(), stand + 100);
}

TEST(P8000RtcMaschine, AlteKarteUeberschreibtKeineFortgeschriebeneBatteriedatei) {
    const std::string datei = k1520test::tempPath("k1520_test_rtc_neubau.txt");
    std::remove(datei.c_str());
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
    auto stand_der_datei = [&] {
        std::ifstream f(datei);
        std::stringstream ss; ss << f.rdbuf();
        const std::string t = ss.str();
        const size_t p = t.find("stand=");
        return p == std::string::npos ? int64_t(-1) : std::stoll(t.substr(p + 6));
    };
    auto alt = std::make_unique<P8000Machine>(r3bConfig(jetzt, datei));
    laufe(*alt, 4);
    // Neubau: die neue Maschine liest die Datei, schreibt weiter; erst danach stirbt die alte.
    jetzt += 10;
    auto neu = std::make_unique<P8000Machine>(r3bConfig(jetzt, datei));
    laufe(*neu, 90);                                   // periodisches Speichern der neuen
    neu->karte16()->rtc()->stellen(utc(2031, 1, 2, 3, 4, 5));
    alt.reset();                                       // darf die Datei nicht zurueckdrehen
    EXPECT_EQ(stand_der_datei(), utc(2031, 1, 2, 3, 4, 5));
    neu.reset();
    EXPECT_EQ(stand_der_datei(), utc(2031, 1, 2, 3, 4, 5));   // der Besitzer schreibt weiter
    std::remove(datei.c_str());
}

}  // namespace
