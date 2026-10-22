/**
 * @file test_p8000_rtc.cpp
 * @brief Echtzeituhr-Karte RTC-72421 am 16-Bit-Teil des P8000 (Entwurf 38, AP R1): Register, BUSY,
 *        HOLD, Stellen/Lesen, 24/12 h, Schaltjahr, Jahreswechsel, Jahresversatz, Batteriedatei,
 *        Save-State, unbestueckt = bitgleich, und der ECHTE Treibercode `r421init/get/set/start`
 *        (`sa.timer/common/rtc72421.c` des WEGA-Quellbaums, O. Lehmann, BSD) gegen die Karte ueber
 *        die E/A-Zyklen der `Karte16`.
 */

#include <gtest/gtest.h>
#include "core/cards/p8000/karte16.h"
#include "core/cards/p8000/rtc.h"
#include "tests/support/temp_path.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

namespace {

// Tage seit 1970 (Hinnant) — unabhaengig vom Kartencode, damit der Test nicht sich selbst prueft.
int64_t utc(int j, int mo, int t, int h = 0, int mi = 0, int s = 0) {
    const int64_t y = j - (mo <= 2);
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + t - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (era * 146097 + doe - 719468) * 86400 + h * 3600 + mi * 60 + s;
}

constexpr uint16_t REG_D = 0xFDFB, REG_E = 0xFDFD, REG_F = 0xFDFF;
constexpr uint16_t S1 = 0xFDE1, S10 = 0xFDE3, MI1 = 0xFDE5, MI10 = 0xFDE7, H1 = 0xFDE9, H10 = 0xFDEB,
                   D1 = 0xFDED, D10 = 0xFDEF, MO1 = 0xFDF1, MO10 = 0xFDF3, Y1 = 0xFDF5, Y10 = 0xFDF7;

struct P8000Rtc72421 : ::testing::Test {
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
    std::string datei;
    std::unique_ptr<P8000Karte16> k;

    void SetUp() override { bestuecken(); }
    void TearDown() override { if (!datei.empty()) std::remove(datei.c_str()); }

    P8000Karte16::Config konfig(int versatzJahre = 0) {
        P8000Karte16::Config c;
        c.rtc.typ = P8000RtcTyp::Rtc72421;
        c.rtc.utc = true;
        c.rtc.jahresversatz = versatzJahre;
        c.rtc.zustand = datei;
        c.rtc.zeitquelle = [this] { return jetzt; };
        return c;
    }
    /// @p sek Sekunden vergehen: Maschinenzeit der Karte UND (zur Gegenprobe) die Wirtszeit.
    void vergeht(int64_t sek) {
        jetzt += sek;
        for (int64_t t = sek * 4'000'000; t > 0;) { const int n = int(std::min<int64_t>(t, 1 << 30)); k->rtc()->takte(n); t -= n; }
    }
    void bestuecken(int versatzJahre = 0) { k = std::make_unique<P8000Karte16>(konfig(versatzJahre)); }

    static Z8kBusCycle io(uint16_t port, bool lesen) {
        Z8kBusCycle c;
        c.st = Z8kStatus::Io; c.system = true; c.word = false; c.read = lesen; c.addr = port;
        return c;
    }
    // Jeder Zugriff kostet etwas Maschinenzeit (ein E/A-Befehl des U8001), sonst stuende BUSY ewig.
    void out(uint16_t port, uint8_t d) { k->busSchreiben(io(port, false), uint16_t(d << 8 | d)); k->rtc()->takte(40); }
    uint8_t in(uint16_t port) { const uint8_t d = uint8_t(k->busLesen(io(port, true))); k->rtc()->takte(40); return d; }
    int nib(uint16_t port) { return in(port) & 0x0F; }

    /// Ziffern der Uhr (24 h) als „JJJJ-MM-TT hh:mm:ss"-Zahlenfolge JJMMTThhmmss.
    int64_t lesenZeit() {
        auto b = [&](uint16_t z, uint16_t e) { return nib(z) * 10 + nib(e); };
        return ((((b(Y10, Y1) * 100LL + b(MO10, MO1)) * 100 + b(D10, D1)) * 100 + b(H10, H1)) * 100 + b(MI10, MI1)) * 100 +
               b(S10, S1);
    }
    void stelle(int yy, int mo, int t, int h, int mi, int s, uint8_t f24 = 0x04) {
        out(REG_F, uint8_t(0x03 | f24));
        auto w = [&](uint16_t z, uint16_t e, int v) { out(z, uint8_t(v / 10)); out(e, uint8_t(v % 10)); };
        w(S10, S1, s); w(MI10, MI1, mi); w(H10, H1, h); w(D10, D1, t); w(MO10, MO1, mo); w(Y10, Y1, yy);
        out(REG_F, f24);
    }
};

TEST_F(P8000Rtc72421, KennungUndOffenerBus) {
    EXPECT_EQ(in(S1) & 0xF0, 0x30);
    EXPECT_EQ(in(REG_F) & 0xF0, 0x30);
    EXPECT_EQ(in(0xFDE2), 0xFF);   // gerade Adresse [R7]
    EXPECT_EQ(in(0xFDDF), 0xFF);   // darunter
    EXPECT_EQ(in(0xFE01), 0xFF);   // darueber
}

TEST_F(P8000Rtc72421, LiestDieWirtszeit2026) {
    EXPECT_EQ(lesenZeit(), 261022143546LL);
    EXPECT_EQ(nib(Y10), 2);
    EXPECT_EQ(nib(Y1), 6);
    EXPECT_EQ(nib(0xFDF9), 4);   // 22.10.2026 war ein Donnerstag
    EXPECT_EQ(k->rtc()->versatz(), 0);
}

TEST_F(P8000Rtc72421, UhrLaeuftMitDerWirtszeit) {
    vergeht(25);
    EXPECT_EQ(lesenZeit(), 261022143611LL);
}

TEST_F(P8000Rtc72421, RegisterDEUndF) {
    out(REG_E, 0x0A);
    EXPECT_EQ(in(REG_E), 0x3A);
    out(REG_F, 0x0C);   // TEST + 24 h, uhr laeuft
    EXPECT_EQ(in(REG_F), 0x3C);
    out(REG_D, 0x04);   // IRQ-Flag
    EXPECT_EQ(in(REG_D) & 0x05, 0x04);
    out(REG_D, 0x00);
    EXPECT_EQ(in(REG_D) & 0x05, 0x00);
}

TEST_F(P8000Rtc72421, BusyFensterJeSekundeMaschinenzeit) {
    // Phase 0 = Anfang der Sekunde: BUSY (122 us = 488 Takte bei 4 MHz).
    EXPECT_EQ(k->busLesen(io(REG_D, true)) & 0x02, 0x02);
    k->rtc()->takte(487);
    EXPECT_EQ(k->busLesen(io(REG_D, true)) & 0x02, 0x02);
    k->rtc()->takte(1);   // 488
    EXPECT_EQ(k->busLesen(io(REG_D, true)) & 0x02, 0x00);
    k->rtc()->takte(3'999'512);   // genau eine Sekunde seit Beginn → wieder BUSY
    EXPECT_EQ(k->busLesen(io(REG_D, true)) & 0x02, 0x02);
}

TEST_F(P8000Rtc72421, HoldHaeltDieZeitFest) {
    k->rtc()->takte(1000);
    out(REG_D, 0x01);
    EXPECT_EQ(in(REG_D) & 0x01, 0x01);
    vergeht(7);
    EXPECT_EQ(lesenZeit(), 261022143546LL);
    out(REG_D, 0x00);
    EXPECT_EQ(lesenZeit(), 261022143553LL);
}

TEST_F(P8000Rtc72421, StellenUndLesen) {
    stelle(27, 3, 14, 8, 15, 30);
    EXPECT_EQ(lesenZeit(), 270314081530LL);
    vergeht(90);
    EXPECT_EQ(lesenZeit(), 270314081700LL);
    EXPECT_EQ(k->rtc()->versatz(), utc(2027, 3, 14, 8, 15, 30) - (utc(2026, 10, 22, 14, 35, 46)));
}

TEST_F(P8000Rtc72421, WaehrendStopStehtDieZeit) {
    out(REG_F, 0x07);   // RESET|STOP|24 h
    const int64_t vorher = lesenZeit();
    vergeht(100);
    EXPECT_EQ(lesenZeit(), vorher);
    out(REG_F, 0x04);   // ohne Stellen weiterlaufen: Versatz unveraendert
    EXPECT_EQ(k->rtc()->versatz(), 0);
}

TEST_F(P8000Rtc72421, SchreibenBeiLaufenderUhrIstWirkungslos) {
    out(S1, 9); out(MI1, 9);
    EXPECT_EQ(lesenZeit(), 261022143546LL);
}

TEST_F(P8000Rtc72421, ZwoelfStundenBetrieb) {
    out(REG_F, 0x00);   // 12 h, laeuft
    EXPECT_EQ(nib(H1), 2);
    EXPECT_EQ(nib(H10), 4);   // 14 Uhr = 2 PM, Bit 2 = PM
    jetzt = utc(2026, 10, 22, 0, 5, 0);
    k->rtc()->stellen(jetzt);
    EXPECT_EQ(nib(H1), 2); EXPECT_EQ(nib(H10), 1);   // 0 Uhr = 12 AM
    // in 12 h stellen: 7 PM = 19 Uhr
    out(REG_F, 0x03);
    out(H10, 4); out(H1, 7);
    out(REG_F, 0x00);   // 12 h, laeuft: 7 PM
    EXPECT_EQ(nib(H1), 7); EXPECT_EQ(nib(H10), 4);
    out(REG_F, 0x04);   // 24 h: 19 Uhr
    EXPECT_EQ(nib(H10) * 10 + nib(H1), 19);
}

TEST_F(P8000Rtc72421, Schaltjahr2028) {
    stelle(28, 2, 29, 23, 59, 59);
    EXPECT_EQ(lesenZeit(), 280229235959LL);
    vergeht(1);
    EXPECT_EQ(lesenZeit(), 280301000000LL);
    vergeht(365LL * 86400);   // 1.3.2029 ... 2029 ist kein Schaltjahr
    EXPECT_EQ(lesenZeit(), 290301000000LL);
}

TEST_F(P8000Rtc72421, Jahreswechsel2026Auf2027) {
    stelle(26, 12, 31, 23, 59, 59);
    EXPECT_EQ(lesenZeit(), 261231235959LL);
    vergeht(1);
    EXPECT_EQ(lesenZeit(), 270101000000LL);
}

TEST_F(P8000Rtc72421, UngueltigesWirdBegrenzt) {
    stelle(26, 2, 31, 25, 70, 99);   // 31. Feb, 25 h, 70 min, 99 s
    EXPECT_EQ(lesenZeit(), 260228235959LL);
}

TEST_F(P8000Rtc72421, Jahresversatz) {
    bestuecken(-28);
    EXPECT_EQ(nib(Y10) * 10 + nib(Y1), 98);   // 2026 − 28 = 1998
    stelle(98, 12, 31, 23, 59, 59);          // gestellt wie angezeigt → wirkliches 2026
    EXPECT_EQ(lesenZeit(), 981231235959LL);
    vergeht(1);
    EXPECT_EQ(lesenZeit(), 990101000000LL);
    // wirkliche Zeit liegt im Jahr 2027 (+1 s ueber 31.12.2026)
    EXPECT_EQ(k->rtc()->versatz(), utc(2026, 12, 31, 23, 59, 59) - utc(2026, 10, 22, 14, 35, 46));
}

TEST_F(P8000Rtc72421, OrtszeitFolgtDemWirt) {
    // Ortszeit = UTC + Zonenversatz des Wirts; die Uhr muss dieselbe Wanduhr zeigen wie localtime().
    auto c = konfig();
    c.rtc.utc = false;
    P8000Karte16 loc(c);
    const std::time_t t = std::time_t(jetzt);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    auto n = [&](uint16_t port) { return int(uint8_t(loc.busLesen(io(port, true))) & 0x0F); };
    EXPECT_EQ(n(H10) * 10 + n(H1), lt.tm_hour);
    EXPECT_EQ(n(MI10) * 10 + n(MI1), lt.tm_min);
}

TEST_F(P8000Rtc72421, Dreissigsekundenabgleich) {
    out(REG_D, 0x08);   // 14:35:46 → 14:36:00
    EXPECT_EQ(lesenZeit(), 261022143600LL);
    EXPECT_EQ(in(REG_D) & 0x08, 0);
    vergeht(20);
    out(REG_D, 0x08);   // 14:36:20 → 14:36:00 (ab)
    EXPECT_EQ(lesenZeit(), 261022143600LL);
}

TEST_F(P8000Rtc72421, BatteriedateiUeberlebtNeustart) {
    datei = k1520test::tempPath("k1520_test_rtc.txt");
    std::remove(datei.c_str());
    bestuecken();
    stelle(27, 3, 14, 8, 15, 30);
    const int64_t vs = k->rtc()->versatz();
    ASSERT_TRUE(std::filesystem::exists(datei));
    k.reset();
    jetzt += 3600;   // die Emulation war aus, die Wirtsuhr lief weiter
    bestuecken();
    EXPECT_EQ(k->rtc()->versatz(), vs);
    EXPECT_EQ(lesenZeit(), 270314091530LL);
}

TEST_F(P8000Rtc72421, SaveStateRundreise) {
    stelle(27, 3, 14, 8, 15, 30);
    out(REG_E, 0x05);
    std::vector<uint8_t> b;
    k->rtc()->serialize(b);
    bestuecken();   // frische Karte
    ASSERT_EQ(k->rtc()->versatz(), 0);
    const uint8_t* p = b.data();
    ASSERT_TRUE(k->rtc()->deserialize(p, b.data() + b.size()));
    EXPECT_EQ(p, b.data() + b.size());
    EXPECT_EQ(lesenZeit(), 270314081530LL);
    EXPECT_EQ(in(REG_E) & 0x0F, 0x05);
    const uint8_t* q = b.data();
    EXPECT_FALSE(k->rtc()->deserialize(q, b.data() + 3));   // abgeschnitten
}

TEST_F(P8000Rtc72421, UnbestuecktIstBitgleich) {
    P8000Karte16 leer;   // Vorgabe: keine Uhrenkarte
    EXPECT_EQ(leer.rtc(), nullptr);
    for (uint16_t p : {S1, REG_D, REG_F, uint16_t(0xFDE2), uint16_t(0xFE01)}) {
        EXPECT_EQ(uint8_t(leer.busLesen(io(p, true))), leer.config().leer8) << std::hex << p;
        leer.busSchreiben(io(p, false), 0x0505);   // darf nichts bewirken
    }
    // und der Bausteinbereich ist unberuehrt: SIO0 Steuerkanal lesbar wie sonst
    P8000Karte16 zweite;
    EXPECT_EQ(leer.busLesen(io(0xFF83, true)), zweite.busLesen(io(0xFF83, true)));
}

// ── Der echte Treibercode ─────────────────────────────────────────────────────
// Uebertragen aus ~/projects/robotron/P8000/github_P8000/WEGA/src/cmd/standalone/sa.timer/common/
// rtc72421.c (+ timegm.c, gmtime.c): r421init, r421set, r421start, r421get samt r421bsy/rdbyte/wrbyte.
// (Der Treiber schreibt fuer das Jahr 2000 die Ziffern 10/0 — tm_year = 100 ist nicht > 100;
// das Jahr 2000 liegt hier deshalb nicht in der Pruefliste.)
// Aenderungen nur: C++-Syntax, inb/outb gegen die Karte, gmtime ueber die Hinnant-Rechnung (die Quelle
// nutzt die WEGA-Libc), timegm wie in timegm.c.
struct Treiber {
    std::function<void(unsigned, int)> outb;
    std::function<int(unsigned)> inb;

    static constexpr unsigned R421REGD = 0xfdfb, R421REGE = 0xfdfd, R421REGF = 0xfdff;
    static constexpr int R421CLR = 0, R421RST = 1, R421STOP = 2, R4212412 = 4, R421HOLD = 1, R421BUSY = 2,
                         R421IRQF = 4;
    static constexpr unsigned SSx1 = 0xfde1, SS1x = 0xfde3, MIx1 = 0xfde5, MI1x = 0xfde7, HHx1 = 0xfde9,
                              HH1x = 0xfdeb, DDx1 = 0xfded, DD1x = 0xfdef, MMx1 = 0xfdf1, MM1x = 0xfdf3,
                              YYx1 = 0xfdf5, YY1x = 0xfdf7;

    void init() { outb(R421REGD, R421IRQF); outb(R421REGE, R421CLR); outb(R421REGF, R4212412); }
    int rdnibble(unsigned a) { return inb(a) & ~0xf0; }
    void bsy() {
        for (;;) {
            outb(R421REGD, R421HOLD);
            if (rdnibble(R421REGD) & R421BUSY) { outb(R421REGD, R421CLR); continue; }
            outb(R421REGD, R421CLR);
            return;
        }
    }
    void wrbyte(unsigned a1, unsigned a2, int b) { bsy(); outb(a1, b / 10); bsy(); outb(a2, b % 10); }
    int rdbyte(unsigned a1, unsigned a2) { bsy(); const int a = rdnibble(a2); bsy(); const int b = rdnibble(a1); return a + b * 10; }

    static long timegm_(int tm_year, int tm_mon, int tm_mday, int tm_hour, int tm_min, int tm_sec) {
        static const int dmsize[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
        long time = 0;
        if (tm_mday > 31 || tm_mon > 12) return time;
        int i = dmsize[tm_mon];
        if (tm_year % 4 == 0 && tm_mon > 2) i++;
        if (tm_year > 137) return time;
        time = (tm_year - 69) / 4;
        time += 365 * (tm_year - 70) + i;
        time += tm_mday - 1;
        if (tm_sec > 59 || tm_min > 59 || tm_hour > 23) return time;
        time = 24 * time + tm_hour;
        time = 60 * time + tm_min;
        time = 60 * time + tm_sec;
        return time;
    }
    long get() {
        const int sec = rdbyte(SS1x, SSx1), min = rdbyte(MI1x, MIx1), hour = rdbyte(HH1x, HHx1);
        const int mday = rdbyte(DD1x, DDx1);
        int mon = rdbyte(MM1x, MMx1), year = rdbyte(YY1x, YYx1);
        if (year < 70) year += 100;
        mon--;
        return timegm_(year, mon, mday, hour, min, sec);
    }
    void set(long time) {
        // gmtime der WEGA-Libc ersetzt durch die Tageszaehlung (Ergebnis tm_year = Jahre seit 1900 usw.)
        int64_t tage = time / 86400; const int sod = int(time % 86400);
        tage += 719468;
        const int64_t era = tage / 146097, doe = tage - era * 146097;
        const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
        const int mday = int(doy - (153 * mp + 2) / 5 + 1), mon = int(mp < 10 ? mp + 3 : mp - 9);
        int year = int(yoe + era * 400 + (mon <= 2)) - 1900;
        if (year > 100) year -= 100;
        outb(R421REGF, R421RST | R421STOP | R4212412);
        wrbyte(SS1x, SSx1, sod % 60);
        wrbyte(MI1x, MIx1, sod / 60 % 60);
        wrbyte(HH1x, HHx1, sod / 3600);
        wrbyte(DD1x, DDx1, mday);
        wrbyte(MM1x, MMx1, mon);
        wrbyte(YY1x, YYx1, year);
    }
    void start() { outb(R421REGF, R4212412); }
};

TEST_F(P8000Rtc72421, EchterTreiberRundreise) {
    Treiber d;
    d.outb = [&](unsigned a, int v) { out(uint16_t(a), uint8_t(v)); };
    d.inb = [&](unsigned a) { return int(in(uint16_t(a))); };
    // Erkennung wie `sa.timer`/`timer.c`: inb(0xfde1) & 0xf0 == 0x30
    EXPECT_EQ(d.inb(0xfde1) & 0xf0, 0x30);
    d.init();
    EXPECT_EQ(d.get(), long(jetzt));   // Wirtszeit 2026-10-22 14:35:46 UTC
    for (int64_t t : {utc(2026, 12, 31, 23, 59, 58), utc(2028, 2, 29, 12, 0, 0), utc(2037, 12, 31, 23, 59, 59),
                      utc(2001, 1, 1, 0, 0, 0)}) {
        d.set(long(t));
        d.start();
        EXPECT_EQ(d.get(), long(t)) << t;
    }
    vergeht(3);
    EXPECT_EQ(d.get(), long(utc(2001, 1, 1, 0, 0, 0)) + 3);
}

}  // namespace
