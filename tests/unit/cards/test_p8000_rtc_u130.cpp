/**
 * @file test_p8000_rtc_u130.cpp
 * @brief Echtzeituhr-Karte U130X (Original, Entwurf 38, AP R2) am 16-Bit-Teil des P8000: Kennung,
 *        Siebensegment-/COM-/Takt-Modell, Zustandsautomat der Armbanduhr, Zeitquelle/Versatz/
 *        Batteriedatei/Save-State, unbestueckt = bitgleich — und der ECHTE Treibercode:
 *        `u130get/u130set/u130start` samt `invalue/rdvalue/outvalue` aus
 *        `sa.timer/common/u130.c` (OlliL, BSD) sowie `gettime()` samt eigenem
 *        `invalue/rdvalue` aus dem Original-Kern `uts/sys/timer.c` (P. Hoge 1988), beide gegen die
 *        Karte ueber die E/A-Zyklen der `Karte16`.
 */

#include <gtest/gtest.h>
#include "core/cards/p8000/karte16.h"
#include "core/cards/p8000/rtc.h"
#include "tests/support/temp_path.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <vector>

using Karte = ::P8000RtcU130;   // die Karte; das Fixture heisst wie die Testreihe

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

constexpr unsigned U130BASE = 0xfef1, U130YYx1 = 0xfef5, U130YY1x = 0xfef7, U130DDx1 = 0xfef9, U130DD1x = 0xfefb,
                   U130MMx1 = 0xfefd, U130MM1x = 0xfeff, U130SSx1 = 0xfef5, U130SS1x = 0xfef7, U130HHx1 = 0xfefd,
                   U130HH1x = 0xfeff, U130MIx1 = 0xfef9, U130MI1x = 0xfefb;

// ── Der echte Treibercode ─────────────────────────────────────────────────────
// Uebertragen aus .../sa.timer/common/u130.c (u130set, u130start, u130get, outvalue, invalue,
// rdvalue; timegm aus timegm.c).  Aenderungen nur: C++-Syntax, inb/outb gegen die Karte, die
// Wartezeit `t = 10000` (Zaehlschleife) als Vorruecken der Maschinenzeit, gmtime ueber die
// Hinnant-Rechnung (die Quelle nutzt die WEGA-Libc).
struct Treiber {
    std::function<void(unsigned, int)> outb;
    std::function<int(unsigned)> inb;
    std::function<void(int)> warte;

    void outvalue(int t) { outb(U130BASE, t); warte(100000); }   // 10000 Schleifendurchlaeufe ~ 25 ms

    int invalue(unsigned addr, int flag) {
        int seg7, takt;
        seg7 = inb(addr);
        if (seg7 != inb(addr))
            seg7 = inb(addr);
        takt = seg7 & 0x80;      /* nur gueltig bei 6. Stelle */
        if (seg7 & 0x20)
            seg7 = ~seg7;
        if (addr == U130MM1x)    /* 6.Stelle */
            seg7 &= 0x07;
        seg7 &= 0x1f;
        if (seg7 == 0x0f)
            return 0;
        if (seg7 == 0x00) {
            if (flag == 0)
                return 0;
            if (addr == U130MM1x && takt)
                return 0;
        }
        if (seg7 == 0x02) return 1;
        if (seg7 == 0x17 || seg7 == 0x05) return 2;
        if (seg7 == 0x13) return 3;
        if (seg7 == 0x1a) return 4;
        if (seg7 == 0x19) return 5;
        if (seg7 == 0x1d) return 6;
        if (seg7 == 0x03) return 7;
        if (seg7 == 0x1f) return 8;
        if (seg7 == 0x1b) return 9;
        return 0xff;
    }
    int rdvalue(unsigned addr) { return invalue(addr, 0) + invalue(addr + 2, 0) * 10; }

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

    long u130get() {
        int tm_year, tm_mday, tm_mon, tm_sec, tm_min, tm_hour;
        outvalue(0x02);                       /* Umschalten auf Datum */
        tm_year = rdvalue(U130YYx1);
        if (tm_year < 70)
            tm_year += 100;
        tm_mday = rdvalue(U130DDx1);
        tm_mon = rdvalue(U130MMx1);
        tm_mon--;
        outvalue(0x00);
        tm_sec = inb(U130MM1x) & 0x28;        /* Umschalten auf Zeit */
        if (tm_sec == 0x08 || tm_sec == 0x20) {
            outvalue(0x02);
            outvalue(0x00);
        }
        while ((tm_sec = rdvalue(U130SSx1)) == 59)
            ;
        tm_min = rdvalue(U130MIx1);
        tm_hour = rdvalue(U130HHx1);
        return timegm_(tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec);
    }

    void u130set(long time) {
        // gmtime der WEGA-Libc ersetzt durch die Tageszaehlung (tm_year = Jahre seit 1900 usw.)
        int64_t tage = time / 86400; const int sod = int(time % 86400);
        tage += 719468;
        const int64_t era = tage / 146097, doe = tage - era * 146097;
        const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
        const int tm_mday = int(doy - (153 * mp + 2) / 5 + 1);
        const int tm_mon = int(mp < 10 ? mp + 3 : mp - 9);
        int tm_year = int(yoe + era * 400 + (tm_mon <= 2)) - 1900;
        const int tm_hour = sod / 3600, tm_min = sod / 60 % 60;
        if (tm_year > 100)
            tm_year -= 100;
        // (die Quelle macht hier `tm_mon++` auf den 0-basierten gmtime-Monat; unser Monat ist schon 1-basiert)

        outvalue(0x02);        /* Datum einstellen */
        outvalue(0x03);        /* Korrektur Monat */
        outvalue(0x00);
        while ((invalue(U130MMx1, 1) + invalue(U130MM1x, 1) * 10) != tm_mon)
            ;
        outvalue(0x01);        /* Korrektur Zehnerstelle Tag */
        outvalue(0x00);
        while (invalue(U130DD1x, 1) != tm_mday / 10)
            ;
        outvalue(0x01);        /* Korrektur Einerstelle Tag */
        outvalue(0x00);
        while (invalue(U130DDx1, 1) != tm_mday % 10)
            ;
        outvalue(0x01);        /* Korrektur Zehnerstelle Jahr */
        outvalue(0x00);
        while (invalue(U130YY1x, 1) != tm_year / 10)
            ;
        outvalue(0x01);        /* Korrektur Einerstelle Jahr */
        outvalue(0x00);
        while (invalue(U130YYx1, 1) != tm_year % 10)
            ;
        outvalue(0x02);        /* Korrektur Ende */
        outvalue(0x00);

        outvalue(0x01);        /* Zeit stellen */
        outvalue(0x00);
        outvalue(0x02);        /* Sekunden auf Null */
        outvalue(0x00);
        outvalue(0x01);
        outvalue(0x00);
        outvalue(0x01);        /* Korrektur Stunden */
        outvalue(0x00);
        while ((invalue(U130HHx1, 1) + invalue(U130HH1x, 1) * 10) != tm_hour)
            ;
        outvalue(0x01);        /* Korrektur Zehner-Minuten */
        outvalue(0x00);
        while (invalue(U130MI1x, 1) != tm_min / 10)
            ;
        outvalue(0x01);        /* Korrektur Einer-Minuten */
        outvalue(0x00);
        while (invalue(U130MIx1, 1) != tm_min % 10)
            ;
        outvalue(0x01);        /* Korrektur Stop */
        outvalue(0x00);
    }

    void u130start() { outvalue(0x01); outvalue(0x00); }   /* Start der Zeit */
};

// Kern 3.2 `uts/sys/timer.c`: gettime() mit eigenem invalue/rdvalue/outvalue.
struct Kern {
    std::function<void(unsigned, int)> outb;
    std::function<int(unsigned)> inb;
    std::function<void(int)> warte;

    void outvalue(int t) { outb(0xfef1, t); warte(100000); }
    int invalue(unsigned addr) {
        int seg7 = inb(addr);
        if (seg7 != inb(addr))
            seg7 = inb(addr);
        if (seg7 & 0x20)
            seg7 = ~seg7;
        if (addr == 0xfeff)      /* 6.Stelle */
            seg7 &= 0x07;
        seg7 &= 0x1f;
        if (seg7 == 0x0f || seg7 == 0x00)
            return 0;
        if (seg7 == 0x02) return 1;
        if (seg7 == 0x17 || seg7 == 0x05) return 2;
        if (seg7 == 0x13) return 3;
        if (seg7 == 0x1a) return 4;
        if (seg7 == 0x19) return 5;
        if (seg7 == 0x1d) return 6;
        if (seg7 == 0x03) return 7;
        if (seg7 == 0x1f) return 8;
        if (seg7 == 0x1b) return 9;
        return -1;
    }
    int rdvalue(unsigned addr) { return invalue(addr) + invalue(addr + 2) * 10; }

    long gettime() {
        static const int dmsize[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
        long time = 0;
        int d1, d2, d3, i;
        if (inb(0xfef1) != 0xbf)  /* Uhrmodul vorhanden ? */
            return time;
        outvalue(0x02);           /* Umschalten auf Datum */
        d1 = rdvalue(0xfef5);     /* Jahr */
        d2 = rdvalue(0xfef9);     /* Tag */
        d3 = rdvalue(0xfefd);     /* Monat */
        outvalue(0x00);
        if (d1 > 100 || d2 > 31 || d3 > 12)
            return time;
        i = dmsize[d3 - 1];
        if (d1 % 4 == 0 && d3 > 2) i++;
        if (d1 < 70) d1 += 100;
        if (d1 > 137) return time;
        time = (d1 - 69) / 4;
        time += 365 * (d1 - 70) + i;
        time += d2 - 1;

        d1 = inb(0xfeff) & 0x28;  /* Umschalten auf Zeit */
        if (d1 == 0x08 || d1 == 0x20) {
            outvalue(0x02);
            outvalue(0x00);
        }
        while ((d1 = rdvalue(0xfef5)) == 59)
            ;                     /* Sekunden */
        d2 = rdvalue(0xfef9);     /* Minuten */
        d3 = rdvalue(0xfefd);     /* Stunden */
        if (d1 > 59 || d2 > 59 || d3 > 23)
            return time;
        time = 24 * time + d3;
        time = 60 * time + d2;
        time = 60 * time + d1;
        return time;
    }
};

struct P8000RtcU130 : ::testing::Test {
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
    bool gekoppelt = false;      ///< Wirtszeit laeuft mit der Maschinenzeit der Karte mit
    int64_t ticks = 0;           ///< Maschinentakte seit dem Bestuecken
    long zugriffe = 0;           ///< Waechter gegen Endlosschleifen des Treibers
    std::string datei;
    std::unique_ptr<P8000Karte16> k;
    Treiber d;
    Kern kern;

    void SetUp() override {
        bestuecken();
        d.outb = [&](unsigned a, int v) { out(a, v); };
        d.inb = [&](unsigned a) { return in(a); };
        d.warte = [&](int n) { takte(n); };
        kern.outb = d.outb; kern.inb = d.inb; kern.warte = d.warte;
    }
    void TearDown() override { if (!datei.empty()) std::remove(datei.c_str()); }

    P8000Karte16::Config konfig(int versatzJahre = 0) {
        P8000Karte16::Config c;
        c.rtc.typ = P8000RtcTyp::U130X;
        c.rtc.utc = true;
        c.rtc.jahresversatz = versatzJahre;
        c.rtc.zustand = datei;
        c.rtc.zeitquelle = [this] { return jetzt + (gekoppelt ? ticks / 4'000'000 : 0); };
        return c;
    }
    void bestuecken(int versatzJahre = 0) { k = std::make_unique<P8000Karte16>(konfig(versatzJahre)); ticks = 0; }

    static Z8kBusCycle io(uint16_t port, bool lesen) {
        Z8kBusCycle c;
        c.st = Z8kStatus::Io; c.system = true; c.word = false; c.read = lesen; c.addr = port;
        return c;
    }
    void takte(int n) { k->rtc()->takte(n); ticks += n; }
    /// @p sek Sekunden vergehen: Maschinenzeit der Karte UND (zur Gegenprobe) die Wirtszeit.
    void vergeht(int64_t sek) {
        jetzt += sek;
        for (int64_t t = sek * 4'000'000; t > 0;) { const int n = int(std::min<int64_t>(t, 1 << 30)); k->rtc()->takte(n); t -= n; }
    }
    /// Uhr (und Wirtszeit) auf @p t setzen, ohne Resttakte: wie eine frisch bestueckte Karte zu dieser Wirtszeit.
    void starte(int64_t t) { jetzt = t; k->rtc()->stellen(t); }
    // Jeder E/A-Zugriff kostet etwas Maschinenzeit (ein E/A-Befehl des U8001).
    void out(unsigned port, int d) {
        k->busSchreiben(io(uint16_t(port), false), uint16_t(d << 8 | (d & 0xFF)));
        takte(40);
    }
    int in(unsigned port) {
        if (++zugriffe > 40'000'000) throw std::runtime_error(std::string("Treiber laeuft endlos in ") + Karte::zustandName(karte()->zustand()) + " port " + std::to_string(port) + " raw=" + std::to_string(uint8_t(k->busLesen(io(uint16_t(port), true)))));
        const int d = uint8_t(k->busLesen(io(uint16_t(port), true)));
        takte(40);
        return d;
    }
    Karte* karte() { return dynamic_cast<Karte*>(k->rtc()); }
};

// ── Karte: Register, Siebensegment, Phase ─────────────────────────────────────

TEST_F(P8000RtcU130, KennungUndOffenerBus) {
    EXPECT_EQ(in(0xFEF1), 0xBF);
    EXPECT_EQ(in(0xFEF3), 0xFF);   // [U8]
    EXPECT_EQ(in(0xFEF2), 0xFF);   // gerade Adresse
    EXPECT_EQ(in(0xFEF0 - 1), 0xFF);
    EXPECT_EQ(in(0xFF01), 0xFF);
    EXPECT_EQ(in(0xFDE1), 0xFF);   // die Adresse des RTC-72421 gehoert diesem Typ nicht
    EXPECT_EQ(karte()->zustand(), Karte::Z::Zeit);
    EXPECT_EQ(k->rtc()->versatz(), 0);
}

TEST_F(P8000RtcU130, SiebensegmentUndComPhase) {
    // 14:35:46 — sechs Stellen von rechts: Sekunde 6 4, Minute 5 3, Stunde 4 1.
    const int erwartet[6] = {6, 4, 5, 3, 4, 1};
    const uint8_t code[10] = {0x0F, 0x02, 0x17, 0x13, 0x1A, 0x19, 0x1D, 0x03, 0x1F, 0x1B};
    bool sah0 = false, sah1 = false;
    for (int i = 0; i < 400; ++i) {          // ~16 ms: mehrere COM-Wechsel
        const unsigned a = 0xFEF5 + 2 * (i % 6);
        const int v = in(a), idx = int((a - 0xFEF5) / 2);
        const bool c = (v & 0x20) != 0;
        (c ? sah1 : sah0) = true;
        int s = c ? ~v : v;
        if (idx == 5) s &= 0x07;
        s &= 0x1F;
        if (idx == 5) EXPECT_EQ(s, 0x02);      // Stundenzehner 1
        else EXPECT_EQ(s, code[erwartet[idx]]) << std::hex << a << " v=" << v;
        EXPECT_EQ(v & 0x40, 0);
        if (idx != 5) EXPECT_EQ(v & 0x80, 0);
    }
    EXPECT_TRUE(sah0);
    EXPECT_TRUE(sah1);
}

TEST_F(P8000RtcU130, ComWechseltInMaschinenzeitNichtInWirtszeit) {
    const int v0 = in(0xFEF5) & 0x20;
    jetzt += 100;                 // Wirtszeit allein bewegt die Phase nicht
    EXPECT_EQ(in(0xFEF5) & 0x20, v0);
    takte(4000);                  // 1 ms [U2]
    EXPECT_NE(in(0xFEF5) & 0x20, v0);
}

TEST_F(P8000RtcU130, SekundentaktUndDatumsSymbol) {
    // Sekundentakt Bit 7 von FEFF: erste Haelfte der Maschinensekunde 1.
    EXPECT_EQ(in(0xFEFF) & 0x80, 0x80);
    takte(2'100'000);
    EXPECT_EQ(in(0xFEFF) & 0x80, 0x00);
    takte(2'000'000);
    EXPECT_EQ(in(0xFEFF) & 0x80, 0x80);
    // Datums-Symbol: Zeit = inaktiv (Bit3 == COM), Datum = aktiv (Bit3 != COM)
    auto dat = [&] { const int v = in(0xFEFF); return ((v >> 3) & 1) != ((v >> 5) & 1); };
    EXPECT_FALSE(dat());
    out(U130BASE, 0x02);
    EXPECT_TRUE(dat());
    out(U130BASE, 0x00);
    EXPECT_TRUE(dat());           // bleibt bis zum Rueckfall
    takte(8'100'000);             // 2 s Maschinenzeit
    EXPECT_FALSE(dat());
    EXPECT_EQ(karte()->zustand(), Karte::Z::Zeit);
}

TEST_F(P8000RtcU130, HtZweimalSofortZurueckAufZeit) {
    out(U130BASE, 0x02); out(U130BASE, 0x00);
    EXPECT_EQ(karte()->zustand(), Karte::Z::Datum);
    out(U130BASE, 0x02); out(U130BASE, 0x00);
    EXPECT_EQ(karte()->zustand(), Karte::Z::Zeit);
    // gehaltene Taste wirkt nur einmal (Flanke)
    out(U130BASE, 0x02); out(U130BASE, 0x02); out(U130BASE, 0x02);
    EXPECT_EQ(karte()->zustand(), Karte::Z::Datum);
}

TEST_F(P8000RtcU130, DatumszifferJahr2026) {
    Treiber t; t.outb = [&](unsigned a, int v) { out(a, v); }; t.inb = [&](unsigned a) { return in(a); };
    t.warte = [&](int n) { takte(n); };
    t.outvalue(0x02);
    EXPECT_EQ(t.rdvalue(U130YYx1), 26);     // Jahr 2026, zweistellig
    EXPECT_EQ(t.rdvalue(U130DDx1), 22);
    EXPECT_EQ(t.rdvalue(U130MMx1), 10);
    t.outvalue(0x00);
}

TEST_F(P8000RtcU130, DoppelleseZweigBeiPhasenwechsel) {
    // Die erste Lesung kippt die COM-Phase: der Treiber sieht zwei verschiedene Rohwerte und liest ein
    // drittes Mal.  Das Ergebnis muss trotzdem stimmen.
    int zaehl = 0;
    d.inb = [&](unsigned a) {
        const int v = in(a);
        if (a == U130SSx1 && ++zaehl == 1) takte(4000);   // genau ein COM-Wechsel nach der ersten Lesung
        return v;
    };
    EXPECT_EQ(d.invalue(U130SSx1, 0), 6);
    EXPECT_EQ(zaehl, 3);                // drei Lesungen = der Doppellese-Zweig lief
    zaehl = 100;                        // ohne Wechsel: zwei Lesungen
    int n = 0;
    d.inb = [&](unsigned a) { ++n; return in(a); };
    EXPECT_EQ(d.invalue(U130SSx1, 0), 6);
    EXPECT_EQ(n, 2);
}

TEST_F(P8000RtcU130, LeereStundenzehnerUndTaktImStellmodus) {
    // Stundenzehner 0 ist leer: mit flag = 1 liefert der Treiber 0 nur bei gesetztem Sekundentakt, sonst 0xff.
    starte(utc(2026, 10, 22, 7, 5, 9));
    EXPECT_EQ(d.invalue(U130HHx1, 1), 7);
    int hohe = 0, tiefe = 0;
    for (int i = 0; i < 100; ++i) {
        const int v = d.invalue(U130HH1x, 1);
        if (v == 0) ++hohe; else { EXPECT_EQ(v, 0xff); ++tiefe; }
        takte(100'000);
    }
    EXPECT_GT(hohe, 10);
    EXPECT_GT(tiefe, 10);
    EXPECT_EQ(d.invalue(U130HH1x, 0), 0);   // ohne flag immer 0
}

// ── Treiber u130get / Kern gettime ────────────────────────────────────────────

TEST_F(P8000RtcU130, KernGettimeLiestWirtszeit2026) {
    EXPECT_EQ(kern.gettime(), long(jetzt));
    EXPECT_EQ(karte()->zustand(), Karte::Z::Zeit);
    EXPECT_EQ(d.u130get(), long(jetzt));
    EXPECT_EQ(karte()->zustand(), Karte::Z::Zeit);
    vergeht(25);
    EXPECT_EQ(kern.gettime(), long(jetzt));
}

TEST_F(P8000RtcU130, GettimeWartetBeiSekunde59AufDenUebertrag) {
    // Wirtszeit laeuft mit der Maschinenzeit; ueber mehrere Abtastversaetze zur Sekundengrenze.  Die Stellenpaar-
    // Haltung [U10] verhindert den zerrissenen Wert (Einer 9 + Zehner 0 = 9 s), den der Treiber nicht erkennt.
    for (int versatz : {0, 17, 53, 101, 131, 159}) {
        bestuecken();
        gekoppelt = true;
        starte(utc(2026, 10, 22, 14, 35, 59));
        takte(versatz);
        const long t = kern.gettime();
        EXPECT_GT(ticks, 3'000'000) << versatz;   // es hat wirklich auf die Sekunde gewartet
        EXPECT_GE(t, long(utc(2026, 10, 22, 14, 36, 0))) << versatz;
        EXPECT_LE(t, long(utc(2026, 10, 22, 14, 36, 1))) << versatz;
    }
}

TEST_F(P8000RtcU130, StellenpaarWirdNichtZerrissen) {
    // Direkt: Einer lesen, Sekundengrenze ueberschreiten, Zehner lesen -> der Zehner gehoert noch zum Einer.
    gekoppelt = true;
    starte(utc(2026, 10, 22, 14, 35, 59));
    takte(4'000'000 - 60);
    d.inb = [&](unsigned a) { return in(a); };
    const int einer = in(0xFEF5);    // 9 (Sekunde 59), danach liegt die Grenze zwischen den Lesungen
    takte(80);
    const int zehner = in(0xFEF7);
    auto dekod = [](int v, bool idx5 = false) { int s = (v & 0x20) ? ~v : v; if (idx5) s &= 7; s &= 0x1f; return s; };
    EXPECT_EQ(dekod(einer), 0x1B);   // 9
    EXPECT_EQ(dekod(zehner), 0x19);  // 5 — nicht 0
    takte(1000);                     // spaeter: wieder frisch (Sekunde 00)
    EXPECT_EQ(dekod(in(0xFEF7)), 0x0F);
}

TEST_F(P8000RtcU130, GettimeNachAbgelaufenemDatumsFenster) {
    // Das Datum steht noch (2 s noch nicht um): ein erneuter HT schaltet auf Zeit, der Treiber liest dann
    // Zeitziffern als Datum; nach Ablauf der 2 s steht die Uhr wieder auf Zeit und alles stimmt.
    out(U130BASE, 0x02); out(U130BASE, 0x00);
    ASSERT_EQ(karte()->zustand(), Karte::Z::Datum);
    takte(8'100'000);
    ASSERT_EQ(karte()->zustand(), Karte::Z::Zeit);
    EXPECT_EQ(kern.gettime(), long(jetzt) + 2);   // die Uhr zaehlt in Maschinenzeit: 8,1M Takte = 2 s
}

// ── Treiber u130set / u130start ───────────────────────────────────────────────

TEST_F(P8000RtcU130, SetUndGetRundreise) {
    // Sekunden sind 0 (HT in der Sekundenstufe)
    for (int64_t t : {utc(2026, 12, 31, 23, 59, 0), utc(2028, 2, 29, 12, 0, 0), utc(2037, 12, 31, 23, 59, 0),
                      utc(2001, 1, 1, 0, 0, 0), utc(2031, 3, 7, 5, 9, 0), utc(2029, 11, 23, 17, 48, 0),
                      utc(2026, 10, 22, 14, 35, 0)}) {
        d.u130set(long(t));
        d.u130start();
        EXPECT_EQ(karte()->zustand(), Karte::Z::Zeit);
        EXPECT_EQ(kern.gettime(), long(t)) << t;
        EXPECT_EQ(d.u130get(), long(t)) << t;
    }
}

TEST_F(P8000RtcU130, StelltAlleStellen) {
    // Jede Stelle von der Vorgabe 22.10.26 14:35 verschieden: Monat, Tag-Z/E, Jahr-Z/E, Std, Min-Z/E
    const int64_t t = utc(2033, 8, 19, 21, 47, 0);
    d.u130set(long(t));
    d.u130start();
    EXPECT_EQ(kern.gettime(), long(t));
    // und das Jahrhundert: Jahr 26 ist 2026, nicht 1926
    d.u130set(long(utc(2026, 1, 9, 6, 6, 0)));
    d.u130start();
    EXPECT_EQ(kern.gettime(), long(utc(2026, 1, 9, 6, 6, 0)));
}

TEST_F(P8000RtcU130, UhrLaeuftNachStartMitDerWirtszeit) {
    d.u130set(long(utc(2026, 12, 31, 23, 59, 0)));
    d.u130start();
    vergeht(60);                                // eine Minute Wirtszeit
    EXPECT_EQ(kern.gettime(), long(utc(2027, 1, 1, 0, 0, 0)));   // Jahreswechsel 2026 -> 2027
    vergeht(58);   // (bei :59 wartet der Treiber auf die naechste Sekunde — bei stehender Wirtszeit ewig)
    EXPECT_EQ(kern.gettime(), long(utc(2027, 1, 1, 0, 0, 58)));
}

TEST_F(P8000RtcU130, Schaltjahr2028) {
    d.u130set(long(utc(2028, 2, 28, 23, 59, 0)));
    d.u130start();
    vergeht(60);
    EXPECT_EQ(kern.gettime(), long(utc(2028, 2, 29, 0, 0, 0)));
    vergeht(86400);
    // Der Kern rechnet 1-basiert (d3 > 2): der 1.3.2028 stimmt.
    EXPECT_EQ(kern.gettime(), long(utc(2028, 3, 1, 0, 0, 0)));
    // Befund: `timegm.c` des Standalone-Treibers (u130get/r421get) bekommt den schon dekrementierten
    // Monat und prueft `tm_mon > 2` — im Maerz eines Schaltjahrs fehlt dort der 29.2.: ein Tag zu frueh.
    // Die Karte liefert die richtigen Ziffern (der Kern zeigt es); das ist ein Fehler des Treibers.
    EXPECT_EQ(d.u130get(), long(utc(2028, 3, 1, 0, 0, 0)) - 86400);
    vergeht(365LL * 86400);                       // 2029 kein Schaltjahr: 1.3.2029 (Jahr 29 -> 29%4 != 0)
    EXPECT_EQ(kern.gettime(), long(utc(2029, 3, 1, 0, 0, 0)));
}

TEST_F(P8000RtcU130, SekundenWerdenBeimStellenNull) {
    d.u130set(long(utc(2026, 5, 17, 9, 30, 37)));   // Sekunden 37 gehen verloren (HT in der Sekundenstufe)
    d.u130start();
    EXPECT_EQ(kern.gettime(), long(utc(2026, 5, 17, 9, 30, 0)));
}

TEST_F(P8000RtcU130, SekundenSchnellkorrekturRundet) {
    // Von Hand: VT (Zeit stellen, Uhr steht), HT (Sekunden auf 0), dann durch alle Stellen und Start.
    auto taste = [&](int v) { out(U130BASE, v); out(U130BASE, 0x00); };
    auto durch = [&] { for (int i = 0; i < 6; ++i) taste(0x01); };    // ZVor, Std, MinZ, MinE, Halt, Start
    // 14:35:46 -> >= 30 s: Minute + 1
    taste(0x01);
    taste(0x02);
    durch();
    EXPECT_EQ(karte()->zustand(), Karte::Z::Zeit);
    EXPECT_EQ(kern.gettime(), long(utc(2026, 10, 22, 14, 36, 0)));
    EXPECT_EQ(k->rtc()->versatz(), 14);
    // 14:36:12 (Wirtszeit +26 s) -> < 30 s: Sekunden 0, Minute bleibt
    vergeht(26);
    taste(0x01);
    taste(0x02);
    durch();
    EXPECT_EQ(kern.gettime(), long(utc(2026, 10, 22, 14, 36, 0)));
}


// ── Versatz, Batterie, Save-State ─────────────────────────────────────────────

TEST_F(P8000RtcU130, GestelltesWirdAlsVersatzMerkbar) {
    d.u130set(long(utc(2027, 3, 14, 8, 15, 0)));
    d.u130start();
    EXPECT_EQ(k->rtc()->versatz(), utc(2027, 3, 14, 8, 15, 0) - jetzt);
}

TEST_F(P8000RtcU130, Jahresversatzminus28) {
    bestuecken(-28);
    EXPECT_EQ(kern.gettime(), long(utc(1998, 10, 22, 14, 35, 46)));   // 2026 − 28 = 1998
    d.u130set(long(utc(1998, 12, 31, 23, 59, 0)));     // gestellt wie angezeigt -> wirkliches 2026
    d.u130start();
    EXPECT_EQ(k->rtc()->versatz(), utc(2026, 12, 31, 23, 59, 0) - jetzt);
    vergeht(60);
    EXPECT_EQ(kern.gettime(), long(utc(1999, 1, 1, 0, 0, 0)));
}

TEST_F(P8000RtcU130, BatteriedateiUeberlebtNeustart) {
    datei = k1520test::tempPath("k1520_test_rtc_u130.txt");
    std::remove(datei.c_str());
    bestuecken();
    d.u130set(long(utc(2027, 3, 14, 8, 15, 0)));
    d.u130start();
    const int64_t vs = k->rtc()->versatz();
    ASSERT_TRUE(std::filesystem::exists(datei));
    k.reset();
    jetzt += 3600;                                  // Emulation war aus, die Wirtsuhr lief weiter
    bestuecken();
    EXPECT_EQ(k->rtc()->versatz(), vs);
    EXPECT_EQ(kern.gettime(), long(utc(2027, 3, 14, 9, 15, 0)));
}

TEST_F(P8000RtcU130, SaveStateRundreise) {
    d.u130set(long(utc(2027, 3, 14, 8, 15, 0)));
    d.u130start();
    // mitten in einer Korrektur: HT+VT -> Monat
    out(U130BASE, 0x02); out(U130BASE, 0x03); out(U130BASE, 0x00);
    takte(1'000'000);                               // zwei Schritte
    const auto z = karte()->zustand();
    ASSERT_EQ(z, Karte::Z::KMonat);
    const int mon = d.rdvalue(U130MMx1) + d.rdvalue(U130MM1x) * 10;
    std::vector<uint8_t> b;
    k->rtc()->serialize(b);
    bestuecken();
    ASSERT_EQ(k->rtc()->versatz(), 0);
    const uint8_t* p = b.data();
    ASSERT_TRUE(k->rtc()->deserialize(p, b.data() + b.size()));
    EXPECT_EQ(p, b.data() + b.size());
    EXPECT_EQ(karte()->zustand(), z);
    EXPECT_EQ(d.rdvalue(U130MMx1) + d.rdvalue(U130MM1x) * 10, mon);
    EXPECT_EQ(k->rtc()->versatz(), utc(2027, 3, 14, 8, 15, 0) - jetzt);
    const uint8_t* q = b.data();
    EXPECT_FALSE(k->rtc()->deserialize(q, b.data() + 3));            // abgeschnitten
    // Der Typ-Wechsel laedt nicht: ein 72421 lehnt den U130X-Abschnitt ab und umgekehrt.
    P8000RtcConfig rc; rc.typ = P8000RtcTyp::Rtc72421; rc.utc = true;
    std::string f;
    auto r421 = P8000Rtc::anlegen(rc, f);
    ASSERT_NE(r421, nullptr);
    const uint8_t* s = b.data();
    EXPECT_FALSE(r421->deserialize(s, b.data() + b.size()));
    std::vector<uint8_t> b2;
    r421->serialize(b2);
    const uint8_t* s2 = b2.data();
    EXPECT_FALSE(k->rtc()->deserialize(s2, b2.data() + b2.size()));
}

TEST_F(P8000RtcU130, UnbestuecktIstBitgleich) {
    P8000Karte16 leer;   // Vorgabe: keine Uhrenkarte
    EXPECT_EQ(leer.rtc(), nullptr);
    for (uint16_t p : {uint16_t(0xFEF1), uint16_t(0xFEF5), uint16_t(0xFEFF), uint16_t(0xFEF2)}) {
        EXPECT_EQ(uint8_t(leer.busLesen(io(p, true))), leer.config().leer8) << std::hex << p;
        leer.busSchreiben(io(p, false), 0x0202);   // darf nichts bewirken
    }
    P8000Karte16 zweite;
    EXPECT_EQ(leer.busLesen(io(0xFF83, true)), zweite.busLesen(io(0xFF83, true)));
}

TEST_F(P8000RtcU130, FabrikLiefertDenTyp) {
    EXPECT_EQ(k->rtc()->typ(), P8000RtcTyp::U130X);
}

}  // namespace
