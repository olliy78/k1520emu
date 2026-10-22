/**
 * @file test_p8000_wega_rtc.cpp
 * @brief Abnahme der P8000-Uhrenkarten am laufenden WEGA (Entwurf 38, AP R4): was zeigt `date` nach dem
 *        Kaltstart mit der Karte, welcher Kerntreiber steckt auf welcher Platte, Schaltjahr, Zeitverhalten,
 *        kein Ruecksprung ueber einen Neustart.
 *
 * Zwei Platten:
 *   - W30: Zwischenstand `p15_7_sync` (ausserhalb des Repos, aus den WEGA-3.0-Disketten installiert; sein
 *     Kern ist byteweise der der ausgelieferten `disks/p8000_wega30`-Platte).  Ohne ihn uebersprungen.
 *   - W31: die ausgelieferte `disks/p8000_wega31/p8000_wega31_platte.avr.img.gz` (Kern mit `u130.o`/`rtc72421`).
 * Kaltstart wie am Geraet (Netz ein -> MON8 -> Koppelsoftware -> MON16 -> NMI -> boot0 -> WEGA), Konsole am
 * Kern-Terminal.  Die Zeitquelle der Karte ist fest eingestellt (Wirtszeit steht, UTC), die Uhr zaehlt in
 * Maschinenzeit.  Befunde: doc/design/38_p8000_rtc.md Abschnitt 11.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <memory>
#include <regex>
#include <set>
#include <string>

#include "core/logger.h"
#include "core/machines/p8000/p8000.h"
#include "core/util/gzip_datei.h"
#include "tests/support/p8000_input.h"
#include "tests/support/p8000_wega.h"
#include "tests/support/temp_path.h"

using namespace k1520test::p8000;

#ifndef P8000_WEGA31_DIR
#define P8000_WEGA31_DIR "disks/p8000_wega31"
#endif

namespace {

void stumm() { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::ERROR); }

using RtcTyp = P8000Machine::Config::Rtc;

int64_t utc(int j, int mo, int t, int h = 0, int mi = 0, int s = 0) {
    const int64_t y = j - (mo <= 2);
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + t - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (era * 146097 + doe - 719468) * 86400 + h * 3600 + mi * 60 + s;
}

/// Auf Text im Bild warten, Fortschrittszeile je 400 Mio. Takte.
bool langBisText(P8000Machine& m, const std::string& nadel, long long grenze) {
    for (long long t = 0; t < grenze; t += 400'000'000) {
        if (laufeBisText(m, nadel, std::min<long long>(400'000'000, grenze - t), 1'000'000)) return true;
        std::string z = cursorZeile(m);
        while (!z.empty() && z.front() == ' ') z.erase(z.begin());
        std::fprintf(stderr, "  [%6.0f Mio. Takte] %s\n", double(m.totalCycles()) / 1e6, z.c_str());
    }
    return false;
}

/// Frage abwarten (Cursorzeile endet darauf), dann @p antwort tippen.
bool frage(P8000Machine& m, const std::string& text, const std::string& antwort, long long grenze = 400'000'000) {
    if (warteAufFrage(m, {text}, grenze) != 0) return false;
    laufe(m, 200'000);
    tippeZeile(m, antwort);
    return true;
}

enum class Platte { W30, W31 };

struct Aufbau {
    Platte platte = Platte::W31;
    RtcTyp rtc = RtcTyp::U130X;
    int versatz = 0;
    int64_t wirt = utc(2026, 10, 22, 14, 30, 0);
    std::string batterie;          ///< Batteriedatei ("" = nur im Speicher)
};

/// Was beim Kaltstart auf dem Bildschirm vorbeikam.
struct BootBefund {
    bool login = false;
    bool datum_gefragt = false;   ///< datem fragte „Enter Date" (tut es immer; kein Zeichen fuer eine fehlende Uhr)
    std::set<std::string> zeilen; ///< alle je gesehenen Bildschirmzeilen (fuer „... RTC found")
    std::string alles() const { std::string s; for (const auto& z : zeilen) s += z + "\n"; return s; }
    /// Zeile „Last known date and time: ..." von datem (= Kernzeit nach dem Lesen der Karte), sonst "".
    std::string zuletzt() const {
        std::string best;   // die Zeile steht in Teilstuecken im Satz (Bild waehrend der Ausgabe) - die laengste gilt
        for (const auto& z : zeilen) { const size_t p = z.find("Last known date and time:"); if (p != std::string::npos && z.size() - p > best.size()) best = z.substr(p); }
        return best;
    }
    bool hat(const std::string& n) const { for (const auto& z : zeilen) if (z.find(n) != std::string::npos) return true; return false; }
};

/// Eine laufende Maschine mit Uhrenkarte und eigener Wirtszeit.
struct Rechner {
    WegaLauf l;
    int64_t wirt = 0;
    int prompt = 1;                ///< Nummer des NAECHSTEN Prompts

    P8000Machine& m() { return *l.m; }
    int64_t uhr() { return m().karte16()->rtc()->gastZeit(); }

    void sammle(BootBefund& b) {
        for (int r = 0; r < k1520::p8000::Terminal::ZEILEN; ++r) {
            const std::string z = zeile(m(), r);
            if (!z.empty()) b.zeilen.insert(z);
        }
    }

    /// Wie warteAufFrage, sammelt aber alle Bildschirmzeilen.
    int warte(BootBefund& b, const std::vector<std::string>& fragen, long long grenze) {
        long long t = 0, seit = 0;
        auto passt = [&]() -> int {
            const std::string z = cursorZeile(m());
            for (size_t i = 0; i < fragen.size(); ++i) {
                std::string f = fragen[i];
                while (!f.empty() && f.back() == ' ') f.pop_back();
                if (endetMit(z, f)) return int(i);
            }
            return -1;
        };
        while (t < grenze) {
            m().run(int(kBatch));
            t += kBatch; seit += kBatch;
            if (seit >= 100'000) {
                seit = 0;
                sammle(b);
                if (int i = passt(); i >= 0) return i;
            }
        }
        return passt();
    }

    /// Maschine bauen (Temp-Kopien von Platte und Startdiskette), Kaltstart bis zur Anmeldung am Prompt.
    /// Der Anmelde-Prompt ist `#1` (root).  Liefert false mit Meldung bei einem Fehlschlag.
    bool bauen(const Aufbau& a, bool nur_bauen, std::string* fehler) {
        wirt = a.wirt;
        std::string start_quelle;
        if (a.platte == Platte::W30) {
            if (!stufeDa("p15_7_sync")) { *fehler = "SKIP:kein Zwischenstand p15_7_sync"; return false; }
            if (!kopiere(stufenPfad("p15_7_sync", "platte.img"), l.platte) ||
                !kopiere(stufenPfad("p15_7_sync", "start.hfe"), l.start)) { *fehler = "Kopie"; return false; }
        } else {
            start_quelle = P8000_WEGA31_DIR "/p8000_wega31_udos_boot.dmk";
            if (!dateiDa(start_quelle)) { *fehler = "SKIP:keine W31-Startdiskette"; return false; }
            l.start = k1520test::tempPath("k1520_rtc_start.dmk");
            std::vector<uint8_t> d;
            std::string f;
            if (!k1520::gzip::laden(P8000_WEGA31_DIR "/p8000_wega31_platte.avr.img.gz", d, nullptr, &f)) {
                *fehler = "SKIP:W31-Platte nicht lesbar: " + f;
                return false;
            }
            std::ofstream o(l.platte, std::ios::binary);
            o.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size()));
            if (!o || !kopiere(start_quelle, l.start)) { *fehler = "Kopie"; return false; }
        }
        return neu(a, nur_bauen, fehler);
    }

    /// Nur die Maschine neu bauen (Platte/Startdiskette/Batteriedatei bleiben) — fuer den Neustartfall.
    bool neu(const Aufbau& a, bool, std::string* fehler) {
        wirt = a.wirt;
        P8000Machine::Config c = wegaConfig(l.platte, false);
        c.rtc = a.rtc;
        c.rtc_jahresversatz = a.versatz;
        c.rtc_utc = true;
        c.rtc_zustand = a.batterie;
        c.rtc_zeitquelle = [this] { return wirt; };
        l.m = std::make_unique<P8000Machine>(c);
        if (!m().mountDisk(0, l.start, m().defaultFormatName(0), false)) { *fehler = "Startdiskette: " + m().lastError(); return false; }
        return true;
    }

    /// Kaltstart bis `login:`, dann als root anmelden.  Tippt auf Datums-/Zeitfragen des Kerns nur RETURN bzw.
    /// eine feste Zeit (Befund `datum_gefragt`).
    bool kaltstart(BootBefund& b, std::string* fehler) {
        P8000Machine& mm = m();
        mm.powerOn();
        if (!laufeBisText(mm, "U880-Softwaremonitor Version 3.1 - Press RETURN", 200'000'000)) { *fehler = bild(mm); return false; }
        tippe(mm, "\r");
        if (!laufeBisPrompt(mm, ">", 4'000'000)) { *fehler = bild(mm); return false; }
        tippe(mm, "\r");
        if (!laufeBisText(mm, "U8000-Softwaremonitor Version 3.1 - Press NMI", 80'000'000)) { *fehler = bild(mm); return false; }
        laufe(mm, 2'000'000);
        mm.nmi();
        if (!laufeBisText(mm, "MAXSEG=<0F>", 400'000'000)) { *fehler = bild(mm); return false; }
        if (!langBisText(mm, "WEGA Kernel -- Release 3.2", 2'000'000'000LL)) { *fehler = bild(mm); return false; }
        // /etc/datem (rc_csh) fragt IMMER „Enter Date/Time" und zeigt „Last known date and time: ..." - das ist
        // die Zeit, die der Kern beim Start von der Uhrenkarte las.  Nicht antworten: nach 2 min Frist laeuft der
        // Start mit eben dieser Zeit weiter (eine Antwort ueberschriebe den Kartenwert).
        for (;;) {
            const int i = warte(b, {"login:", "#1"}, 8'000'000'000LL);
            if (i < 0) { *fehler = bild(mm); return false; }
            if (i == 0) break;
            laufe(mm, 200'000);
            tippeZeile(mm, "init 2");
        }
        for (const auto& z : b.zeilen)
            if (z.find("Enter Date") != std::string::npos) b.datum_gefragt = true;
        b.login = true;
        tippeZeile(mm, "wega");
        if (!frage(mm, "Password:", "root", 400'000'000)) { *fehler = bild(mm); return false; }
        if (warteAufFrage(mm, {"#1"}, 2'000'000'000LL) < 0) { *fehler = bild(mm); return false; }
        prompt = 2;
        sammle(b);
        return true;
    }

    /// Kommando am Prompt; liefert den Bildschirm danach (Prompt gezaehlt).
    bool kommando(const std::string& cmd, std::string* bild_aus = nullptr, long long grenze = 2'000'000'000LL) {
        tippeZeile(m(), cmd);
        const std::string p = "#" + std::to_string(prompt);
        if (warteAufFrage(m(), {p}, grenze) < 0) return false;
        ++prompt;
        if (bild_aus) *bild_aus = bild(m());
        return true;
    }

    /// `date` an der Konsole; liefert die Ausgabezeile (z. B. "Thu Oct 22 14:33:07 GMT 2026"), "" bei Fehler.
    std::string date(int64_t* uhr_dabei = nullptr) {
        std::string b;
        if (!kommando("date", &b)) return "";
        if (uhr_dabei) *uhr_dabei = uhr();
        static const std::regex re(R"(((Mon|Tue|Wed|Thu|Fri|Sat|Sun) [A-Z][a-z]{2} +[0-9]{1,2} [0-9:]{8} (\S+ )?[0-9]{4}))");
        std::smatch mt;
        std::string letzte;
        std::string::const_iterator it = b.begin();
        while (std::regex_search(it, b.cend(), mt, re)) { letzte = mt[1]; it = mt.suffix().first; }
        return letzte;
    }
};

/// Kalenderzeit aus `date`-Ausgabe (Wanduhr, als UTC), 0 bei Fehler.  Zeitzonenname wird ignoriert.
int64_t datumSekunden(const std::string& d) {
    static const std::regex re(R"([A-Z][a-z]{2} ([A-Z][a-z]{2}) +([0-9]{1,2}) ([0-9]{2}):([0-9]{2}):([0-9]{2}) (?:\S+ )?([0-9]{4}))");
    std::smatch mt;
    if (!std::regex_search(d, mt, re)) return 0;
    static const char* mon = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const size_t p = std::string(mon).find(mt[1].str());
    if (p == std::string::npos) return 0;
    return utc(std::stoi(mt[6]), int(p / 3) + 1, std::stoi(mt[2]), std::stoi(mt[3]), std::stoi(mt[4]), std::stoi(mt[5]));
}

/// Eine Boot-Abnahme: Kaltstart, Befund, `date`.  Gibt alles auf stderr aus (Befundprotokoll).
struct Fall {
    BootBefund boot;
    std::string date;
    int64_t uhr_dabei = 0;
};

bool abnahme(const char* titel, const Aufbau& a, Fall& f, std::string* skip = nullptr) {
    Rechner r;
    std::string fehler;
    if (!r.bauen(a, false, &fehler)) {
        if (fehler.rfind("SKIP:", 0) == 0 && skip) { *skip = fehler.substr(5); return false; }
        ADD_FAILURE() << titel << ": " << fehler;
        return false;
    }
    if (!r.kaltstart(f.boot, &fehler)) { ADD_FAILURE() << titel << ": Kaltstart\n" << fehler; return false; }
    f.date = r.date(&f.uhr_dabei);
    std::fprintf(stderr, "[R4] %-34s datum_gefragt=%d  date: %s   (Karte: %lld)\n", titel, f.boot.datum_gefragt ? 1 : 0,
                 f.date.c_str(), static_cast<long long>(f.uhr_dabei));
    std::fprintf(stderr, "[R4]    datem: %s\n", f.boot.zuletzt().c_str());
    for (const auto& z : f.boot.zeilen)
        if (z.find("found") != std::string::npos || z.find("RTC") != std::string::npos || z.find("known") != std::string::npos)
            std::fprintf(stderr, "[R4]    Boot: %s\n", z.c_str());
    return true;
}

}  // namespace

// ── Treiberbefund (ohne Boot): Kernabbild der Platten ─────────────────────────────────────────────────
// Der W30-Kern (Generated 02/12/88) enthaelt weder timer.o noch einen Zugriff auf FEF1/FDE1 (Portadressen der Karten);
// der W31-Kern (Generated 06/29/116) traegt `u130.o`, `rtc72421`, "RFT U130X RTC", "Seiko Epson RTC-72421", "%s found".
// Belegt per `k1520disktool get <platte> md2/wega` + `strings`, Befund in Entwurf 38 Abschnitt 11; der Kaltstart unten
// zeigt es am laufenden Kern (W31 meldet „... RTC found", W30 nicht).

// ── Kaltstart mit den Karten: was liest der Kern? ─────────────────────────────────────────────────────

Aufbau aufbau(Platte p, RtcTyp t, int versatz, int64_t wirt) {
    Aufbau a; a.platte = p; a.rtc = t; a.versatz = versatz; a.wirt = wirt;
    return a;
}

/// Jahr aus der `date`-Ausgabe.
int datumJahr(const std::string& d) {
    const size_t p = d.find_last_of(' ');
    return p == std::string::npos ? 0 : std::atoi(d.c_str() + p + 1);
}

/// Prueft `date` gegen die Gastzeit @p gast (UTC-Kalender, mit Versatz) beim Start der Karte: `date` zeigt die Ortszeit
/// (TZ=MEZ-1MES: +@p zone_s) und ist um die Bootzeit weiter (UDOS-/Monitor-Phase + Kernstart ≈ 6 Min bis zum Lesen der
/// Karte, danach 2 Min datem-Frist).  @p tage_frueh: Tage, um die der GAST (Kerntreiber) falsch rechnet.
void pruefeDatum(const Fall& f, int64_t gast, int zone_s, int tage_frueh = 0) {
    ASSERT_FALSE(f.date.empty());
    const int64_t d = datumSekunden(f.date) - (gast - int64_t(tage_frueh) * 86400 + zone_s);
    EXPECT_GE(d, 60) << f.date;
    EXPECT_LE(d, 1200) << f.date;
    // Die Karte selbst hat den Wert richtig geliefert (nur der Kerntreiber irrt): Karte = Gastzeit + Laufzeit.
    EXPECT_GE(f.uhr_dabei - gast, 60);
    EXPECT_LE(f.uhr_dabei - gast, 1200);
}

TEST(P8000WegaRtc, W31_U130X_2026_Versatz0) {
    stumm();
    Fall f; std::string skip;
    if (!abnahme("W31 U130X 2026 Versatz 0", aufbau(Platte::W31, RtcTyp::U130X, 0, utc(2026, 10, 22, 14, 30)), f, &skip)) { if (!skip.empty()) GTEST_SKIP() << skip; return; }
    EXPECT_TRUE(f.boot.hat("RFT U130X RTC found"));
    EXPECT_FALSE(f.boot.hat("RTC-72421 found"));
    EXPECT_EQ(f.date.substr(0, 3), "Thu");   // 22.10.2026 ist ein Donnerstag: WEGA kann 2026
    EXPECT_EQ(datumJahr(f.date), 2026);
    pruefeDatum(f, utc(2026, 10, 22, 14, 30), 7200);
}
TEST(P8000WegaRtc, W31_U130X_2026_Versatz40) {
    stumm();
    Fall f; std::string skip;
    if (!abnahme("W31 U130X 2026 Versatz -40", aufbau(Platte::W31, RtcTyp::U130X, -40, utc(2026, 10, 22, 14, 30)), f, &skip)) { if (!skip.empty()) GTEST_SKIP() << skip; return; }
    EXPECT_EQ(datumJahr(f.date), 1986);
    EXPECT_EQ(f.date.substr(0, 3), "Wed");   // 22.10.1986 war ein Mittwoch (Wochentag rechnet WEGA selbst, Schaltjahrmuster stimmt)
    pruefeDatum(f, utc(1986, 10, 22, 14, 30), 7200);
}
TEST(P8000WegaRtc, W31_U130X_2026_Versatz28) {
    stumm();
    Fall f; std::string skip;
    if (!abnahme("W31 U130X 2026 Versatz -28", aufbau(Platte::W31, RtcTyp::U130X, -28, utc(2026, 10, 22, 14, 30)), f, &skip)) { if (!skip.empty()) GTEST_SKIP() << skip; return; }
    EXPECT_EQ(datumJahr(f.date), 1998);
    EXPECT_EQ(f.date.substr(0, 3), "Thu");   // gleicher Wochentag wie 2026
    pruefeDatum(f, utc(1998, 10, 22, 14, 30), 7200);
}
TEST(P8000WegaRtc, W31_Rtc72421_2026_Versatz0) {
    stumm();
    Fall f; std::string skip;
    if (!abnahme("W31 RTC-72421 2026 Versatz 0", aufbau(Platte::W31, RtcTyp::Rtc72421, 0, utc(2026, 10, 22, 14, 30)), f, &skip)) { if (!skip.empty()) GTEST_SKIP() << skip; return; }
    EXPECT_TRUE(f.boot.hat("Seiko Epson RTC-72421 found"));
    EXPECT_FALSE(f.boot.hat("RFT U130X RTC found"));
    EXPECT_EQ(datumJahr(f.date), 2026);
    pruefeDatum(f, utc(2026, 10, 22, 14, 30), 7200);
}
TEST(P8000WegaRtc, W31_Rtc72421_2026_Versatz40) {
    stumm();
    Fall f; std::string skip;
    if (!abnahme("W31 RTC-72421 2026 Versatz -40", aufbau(Platte::W31, RtcTyp::Rtc72421, -40, utc(2026, 10, 22, 14, 30)), f, &skip)) { if (!skip.empty()) GTEST_SKIP() << skip; return; }
    EXPECT_EQ(datumJahr(f.date), 1986);
    pruefeDatum(f, utc(1986, 10, 22, 14, 30), 7200);
}

/// Der Kern der WEGA-3.0-Platte (Generated 02/12/88) kennt keine Uhrenkarte: er laeuft ohne RTC-Zeit
/// (datem nennt den letzten bekannten Stand aus dem Dateisystem), ohne Absturz — mit beiden Kartentypen.
TEST(P8000WegaRtc, W30_KernOhneUhrentreiberIgnoriertDieKarte) {
    stumm();
    for (const RtcTyp t : {RtcTyp::U130X, RtcTyp::Rtc72421}) {
        Fall f; std::string skip;
        if (!abnahme(t == RtcTyp::U130X ? "W30 U130X" : "W30 RTC-72421", aufbau(Platte::W30, t, 0, utc(2026, 10, 22, 14, 30)), f, &skip)) {
            if (!skip.empty()) GTEST_SKIP() << skip;
            return;
        }
        EXPECT_FALSE(f.boot.hat("RTC found"));
        EXPECT_FALSE(f.boot.hat("RTC-72421"));
        EXPECT_TRUE(f.boot.login);
        EXPECT_NE(datumJahr(f.date), 2026) << f.date;   // nicht der Kartenwert
    }
}

// Schaltjahr 2028 (Karte und Kern): 29.2. stimmt.  Im Maerz rechnet der Kerntreiber der W31-Platte einen Tag zu frueh
// (Gastfehler `timegm.c`, Entwurf 38 §7/§11) — beide Karten liefern den richtigen Wert (Karte = Gastzeit).
TEST(P8000WegaRtc, W31_U130X_Schaltjahr2028) {
    stumm();
    for (const bool maerz : {false, true}) {
        Fall f; std::string skip;
        const int64_t w = maerz ? utc(2028, 3, 15, 12, 0) : utc(2028, 2, 29, 12, 0);
        if (!abnahme(maerz ? "W31 U130X 2028-03-15" : "W31 U130X 2028-02-29", aufbau(Platte::W31, RtcTyp::U130X, 0, w), f, &skip)) { if (!skip.empty()) GTEST_SKIP() << skip; return; }
        pruefeDatum(f, w, 3600, maerz ? 1 : 0);
        EXPECT_EQ(f.date.find(maerz ? "Mar 14" : "Feb 29") != std::string::npos, true) << f.date;
    }
}
TEST(P8000WegaRtc, W31_Rtc72421_Schaltjahr2028) {
    stumm();
    for (const bool maerz : {false, true}) {
        Fall f; std::string skip;
        const int64_t w = maerz ? utc(2028, 3, 15, 12, 0) : utc(2028, 2, 29, 12, 0);
        if (!abnahme(maerz ? "W31 RTC-72421 2028-03-15" : "W31 RTC-72421 2028-02-29", aufbau(Platte::W31, RtcTyp::Rtc72421, 0, w), f, &skip)) { if (!skip.empty()) GTEST_SKIP() << skip; return; }
        pruefeDatum(f, w, 3600, maerz ? 1 : 0);
        EXPECT_EQ(f.date.find(maerz ? "Mar 14" : "Feb 29") != std::string::npos, true) << f.date;
    }
}

// ── 5. Zeitverhalten: Kernuhr (CTC) und Karte laufen in Maschinenzeit gleich schnell ────────────────

TEST(P8000WegaRtc, W31_U130X_KernUndKarteLaufenGleichSchnell) {
    stumm();
    Rechner r; BootBefund b; std::string fehler;
    Aufbau a = aufbau(Platte::W31, RtcTyp::U130X, 0, utc(2026, 10, 22, 14, 30));
    if (!r.bauen(a, false, &fehler)) { if (fehler.rfind("SKIP:", 0) == 0) GTEST_SKIP() << fehler.substr(5); FAIL() << fehler; }
    ASSERT_TRUE(r.kaltstart(b, &fehler)) << fehler;
    int64_t u1 = 0, u2 = 0;
    const std::string d1 = r.date(&u1);
    ASSERT_FALSE(d1.empty());
    // Versatz Kern zu Karte: Ortszeit - GMT, ein ganzes Vielfaches von Stunden (MES = +2 h im Oktober), sonst ±3 s
    const int64_t off = datumSekunden(d1) - u1;
    const int64_t std_h = (off + 1800) / 3600;
    std::fprintf(stderr, "[R4] Zeitverhalten: date1 %s  Karte %lld  Zonenabstand %lld s\n", d1.c_str(), static_cast<long long>(u1),
                 static_cast<long long>(off));
    EXPECT_LE(std::llabs(off - std_h * 3600), 3) << "Kernuhr und Karte weichen schon beim Start voneinander ab";
    laufe(r.m(), 5LL * 60 * 4'000'000);   // 5 Maschinenminuten
    const std::string d2 = r.date(&u2);
    ASSERT_FALSE(d2.empty());
    const int64_t dk = datumSekunden(d2) - datumSekunden(d1), du = u2 - u1;
    std::fprintf(stderr, "[R4] Zeitverhalten: date2 %s  Karte %lld  Delta Kern %lld s, Karte %lld s\n", d2.c_str(),
                 static_cast<long long>(u2), static_cast<long long>(dk), static_cast<long long>(du));
    EXPECT_NEAR(double(dk), double(du), 2.0);
    EXPECT_NEAR(double(du), 300.0, 3.0);   // 5 Minuten Maschinenzeit (die zwei `date` laufen dazu)
}

// ── 6. Kein Ruecksprung ueber einen Neustart ──────────────────────────────────────────────────────

TEST(P8000WegaRtc, W31_U130X_NeustartSpringtNichtHinterDieDateizeit) {
    stumm();
    const std::string batterie = k1520test::tempPath("k1520_rtc_wega_batterie.txt");
    std::remove(batterie.c_str());
    Rechner r; BootBefund b; std::string fehler;
    Aufbau a = aufbau(Platte::W31, RtcTyp::U130X, 0, utc(2026, 10, 22, 14, 30));
    a.batterie = batterie;
    if (!r.bauen(a, false, &fehler)) { if (fehler.rfind("SKIP:", 0) == 0) GTEST_SKIP() << fehler.substr(5); FAIL() << fehler; }
    ASSERT_TRUE(r.kaltstart(b, &fehler)) << fehler;
    laufe(r.m(), 5LL * 60 * 4'000'000);                  // die Maschine laeuft Minuten, die Wirtszeit steht
    std::string bild_ls;
    ASSERT_TRUE(r.kommando("touch /probe", nullptr));
    ASSERT_TRUE(r.kommando("sync", nullptr));
    ASSERT_TRUE(r.kommando("ls -l /probe", &bild_ls));
    int64_t u_a = 0;
    const std::string d_a = r.date(&u_a);
    ASSERT_FALSE(d_a.empty());
    // „-rw-r--r--   1 root  ...   0 Oct 22 16:37 /probe": Monat Tag hh:mm der Dateizeit (Minutengenauigkeit)
    static const std::regex re(R"(([A-Z][a-z]{2}) +([0-9]{1,2}) ([0-9]{2}):([0-9]{2}) /probe)");
    std::smatch mt;
    ASSERT_TRUE(std::regex_search(bild_ls, mt, re)) << bild_ls;
    static const char* mon = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const int mo = int(std::string(mon).find(mt[1].str()) / 3) + 1;
    const int64_t mtime = utc(2026, mo, std::stoi(mt[2]), std::stoi(mt[3]), std::stoi(mt[4]));   // Ortszeit, Minute
    std::fprintf(stderr, "[R4] Neustart: ls: %s   date A: %s   Karte A: %lld\n", mt[0].str().c_str(), d_a.c_str(), static_cast<long long>(u_a));
    EXPECT_GE(datumSekunden(d_a), mtime);

    // Maschine wegwerfen (Netz aus, Platte sichern), neue aus derselben Platte und Batteriedatei; die Wirtszeit
    // ist um 30 s weiter - weit weniger als die gelaufene Maschinenzeit.
    const int64_t stand_a = r.uhr();
    r.m().powerOff();
    r.m().hdFlush();
    r.m().flushDisks();
    r.l.m.reset();
    a.wirt += 30;
    ASSERT_TRUE(r.neu(a, false, &fehler)) << fehler;
    EXPECT_GE(r.uhr(), stand_a) << "Karte nach dem Neubau hinter dem Stand der alten";
    BootBefund b2;
    ASSERT_TRUE(r.kaltstart(b2, &fehler)) << fehler;
    int64_t u_b = 0;
    const std::string d_b = r.date(&u_b);
    ASSERT_FALSE(d_b.empty());
    std::fprintf(stderr, "[R4] Neustart: Karte A-Ende %lld  datem B: %s  date B: %s  Karte B: %lld\n", static_cast<long long>(stand_a),
                 b2.zuletzt().c_str(), d_b.c_str(), static_cast<long long>(u_b));
    EXPECT_GE(datumSekunden(d_b), mtime) << "date nach dem Neustart liegt vor der Dateizeit";
    EXPECT_GE(datumSekunden(d_b), datumSekunden(d_a));
    EXPECT_GE(u_b, stand_a);
    std::string ls2;
    ASSERT_TRUE(r.kommando("ls -l /probe", &ls2));
    EXPECT_NE(ls2.find("/probe"), std::string::npos) << ls2;   // die Datei hat den Neustart ueberlebt
    std::remove(batterie.c_str());
}
