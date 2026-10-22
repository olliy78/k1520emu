/**
 * @file test_p8000_wega_rtc.cpp
 * @brief Abnahme der P8000-Uhrenkarten am laufenden WEGA (Entwurf 38, AP R4): was zeigt der Kern nach dem Kaltstart
 *        mit der Karte, welcher Kerntreiber steckt auf welcher Platte, Schaltjahr, Zeitverhalten, kein Ruecksprung
 *        ueber einen Neustart.
 *
 * Zwei Platten:
 *   - W30: Zwischenstand `p15_7_sync` (ausserhalb des Repos, aus den WEGA-3.0-Disketten installiert; sein
 *     Kern ist byteweise der der ausgelieferten `disks/p8000_wega30`-Platte).  Ohne ihn uebersprungen.
 *   - W31: die ausgelieferte `disks/p8000_wega31/p8000_wega31_platte.avr.img.gz` (Kern mit `u130.o`/`rtc72421`).
 *
 * VERKUERZUNG (Entwurf 38 §11 „Verkuerzung"): Der Kaltstart (MON8 -> Koppelsoftware -> MON16 -> WEGA-Kern) kostet
 * ~6 Maschinenminuten und liegt VOR dem ersten Lesen der Karte.  Er wird deshalb je Platte EINMAL gefahren und als
 * eingefrorener Zustand (`~/.cache/k1520emu/p8000_wega`, Stand „Kernbanner") abgelegt — OHNE Uhrenabschnitt, damit
 * er zu jedem Kartentyp passt.  Jeder Fall laedt ihn, steckt die Karte (Typ/Versatz/Zeit stellt `Rechner::neu` und
 * `stellen`) und laeuft ab dort: Kernstart liest die Karte, `/etc/datem` wird SOFORT beantwortet (Datum RETURN, Zeit =
 * der „Last known"-Wert) statt 2 Minuten abzuwarten.  Der Kartenwert steht in der Zeile „Last known date and time"
 * (= Kernzeit nach dem Lesen der Karte); sie ist der Messwert, `date` nach datem nur Gegenprobe.
 * Die Faelle selbst bleiben vollstaendig; nur die Wartezeit entfaellt.  Zwei Abstriche: der Gast prueft bei jedem
 * Mehrbenutzerstart alle Dateisysteme (`fsck` in `/etc/rc_csh`, ~5 Maschinenminuten vor `datem`, ohne Bezug zur Uhr);
 * die vier Zeilen entfallen auf der Kopie der Platte (`fsckAus`).  Und der Neustartfall startet die zweite Maschine
 * ebenfalls aus dem Zustand „Kernbanner" (neue Maschine, gleiche Platte und Batteriedatei; ihre Karte beginnt nach der
 * Startregel aus der Batteriedatei) statt aus dem Netz-Ein.
 * Die Zeitquelle der Karte ist fest eingestellt (Wirtszeit steht, UTC), die Uhr zaehlt in Maschinenzeit.
 * Befunde: doc/design/38_p8000_rtc.md Abschnitt 11.
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
#include <filesystem>
#include <sstream>

#include "core/util/gzip_datei.h"
#include "tests/support/p8000_input.h"
#include "tests/support/p8000_wega.h"
#include "tests/support/temp_path.h"

using namespace k1520test::p8000;

#ifndef K1520_DISKTOOL
#define K1520_DISKTOOL "k1520disktool"
#endif
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
    int64_t uhr_bei_datem = 0;    ///< Kartenstand (gastsichtig), als datem fragte
    std::string alles() const { std::string s; for (const auto& z : zeilen) s += z + "\n"; return s; }
    /// Zeile „Last known date and time: ..." von datem (= Kernzeit nach dem Lesen der Karte), sonst "".
    std::string zuletzt() const {
        std::string best;   // die Zeile steht in Teilstuecken im Satz (Bild waehrend der Ausgabe) - die laengste gilt
        for (const auto& z : zeilen) { const size_t p = z.find("Last known date and time:"); if (p != std::string::npos && z.size() - p > best.size()) best = z.substr(p); }
        return best;
    }
    bool hat(const std::string& n) const { for (const auto& z : zeilen) if (z.find(n) != std::string::npos) return true; return false; }
};

// ── Eingefrorener Zustand „Kernbanner" je Platte ─────────────────────────────────────────────────────
// Kurz VOR dem ersten Lesen der Uhrenkarte (Bild zeigt „WEGA Kernel -- Release 3.2").  Der Uhrenabschnitt (P8KS-
// Abschnitt 9) wird herausgeschnitten: ohne ihn behaelt die geladene Maschine ihre frisch gebaute Karte (beliebiger
// Typ/Versatz/Batteriedatei); „Fehlende Abschnitte lassen den Ist-Zustand stehen" (P8000Machine::wendeAbschnitteAn).
constexpr uint8_t kAbsRtc = 9;
constexpr int kStufenStand = 2;   // anheben, wenn sich der Aufbau des Zustands aendert

struct Stufe {
    std::string name, start_ext;
};
Stufe stufeVon(Platte p) {
    return p == Platte::W30 ? Stufe{"rtc30_banner_v1", "hfe"} : Stufe{"rtc31_banner_v1", "dmk"};
}

std::vector<uint8_t> ohneRtcAbschnitt(const std::vector<uint8_t>& b) {
    std::vector<uint8_t> o(b.begin(), b.begin() + 5);   // "P8KS" + Version
    size_t pos = 5;
    while (pos + 5 <= b.size()) {
        uint32_t n = 0;
        for (int i = 0; i < 4; ++i) n |= uint32_t(b[pos + 1 + i]) << (8 * i);
        const size_t ende = pos + 5 + n;
        if (ende > b.size()) break;
        if (b[pos] != kAbsRtc) o.insert(o.end(), b.begin() + std::ptrdiff_t(pos), b.begin() + std::ptrdiff_t(ende));
        pos = ende;
    }
    return o;
}

std::string stufenStempel(Platte p) {
    std::error_code ec;
    const std::string q = p == Platte::W30 ? stufenPfad("p15_7_sync", "platte.img")
                                           : std::string(P8000_WEGA31_DIR "/p8000_wega31_platte.avr.img.gz");
    return std::to_string(kStufenStand) + " " + std::to_string(std::filesystem::file_size(q, ec)) + " " +
           std::to_string(std::filesystem::last_write_time(q, ec).time_since_epoch().count());
}

std::string stufeDatei(Platte p, const char* art) {
    const Stufe s = stufeVon(p);
    return stufenPfad(s.name, art);
}

bool dateiInhalt(const std::string& pf, std::string& s) {
    std::ifstream f(pf, std::ios::binary);
    if (!f) return false;
    s.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}

/// Nimmt die vier `fsck`-Zeilen aus `/etc/rc_csh` der Platte @p platte (Temp-Kopie des Zustands).  Der Gast prueft bei
/// JEDEM Mehrbenutzerstart alle Dateisysteme (~5 Maschinenminuten vor `datem`, davon unabhaengig von der Uhrenkarte);
/// die Pruefung ist nicht Gegenstand dieser Abnahme.  false, wenn die Datei fehlt (dann bleibt die Platte, wie sie ist).
bool fsckAus(const std::string& platte) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::path(platte).parent_path() / "k1520_rtc_rc";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    auto disktool = [&](const std::string& args) {
        const std::string cmd = std::string("\"") + K1520_DISKTOOL + "\" " + args + " > /dev/null 2>&1";
        return std::system(cmd.c_str()) == 0;
    };
    bool ok = false;
    if (disktool("get \"" + platte + "\" md2/etc/rc_csh --to \"" + dir.string() + "\"")) {
        const fs::path rc = dir / "md2" / "etc" / "rc_csh";
        std::ifstream in(rc, std::ios::binary);
        std::string zeile, neu;
        int n = 0;
        while (std::getline(in, zeile)) {
            if (zeile.find("/etc/fsck") != std::string::npos) { ++n; continue; }
            neu += zeile + "\n";
        }
        in.close();
        if (n > 0) {
            std::ofstream(rc, std::ios::binary) << neu;
            ok = disktool("put \"" + platte + "\" \"" + rc.string() + "\" --as md2/etc/rc_csh --no-backup --force") &&
                 disktool("attr \"" + platte + "\" md2/etc/rc_csh --uid 0 --gid 0 --mode 0744 --no-backup");
        }
    }
    fs::remove_all(dir, ec);
    return ok;
}

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

    /// Platte und Startdiskette der Platte @p p als Temp-Kopien anlegen (W30: Zwischenstand p15_7_sync,
    /// W31: ausgelieferte Platte + Startdiskette).  false mit "SKIP:..." wenn die Quellen fehlen.
    bool plattenAnlegen(Platte p, std::string* fehler) {
        if (p == Platte::W30) {
            if (!stufeDa("p15_7_sync")) { *fehler = "SKIP:kein Zwischenstand p15_7_sync"; return false; }
            if (!kopiere(stufenPfad("p15_7_sync", "platte.img"), l.platte) ||
                !kopiere(stufenPfad("p15_7_sync", "start.hfe"), l.start)) { *fehler = "Kopie"; return false; }
            return true;
        }
        const std::string start_quelle = P8000_WEGA31_DIR "/p8000_wega31_udos_boot.dmk";
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
        return true;
    }

    /// Nur die Maschine neu bauen (Platte/Startdiskette/Batteriedatei bleiben) — fuer den Neustartfall.
    bool neu(const Aufbau& a, std::string* fehler) {
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

    /// Voller Kaltstart (Netz ein -> MON8 -> Koppelsoftware -> MON16 -> NMI -> boot0) bis das Bild
    /// „WEGA Kernel -- Release 3.2" zeigt — der Zeitpunkt des eingefrorenen Zustands.
    bool bisKernbanner(std::string* fehler) {
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
        return true;
    }

    /// Zustand „Kernbanner" der Platte @p p anlegen, falls er fehlt oder nicht mehr zur Quelle passt.
    static bool stufeSicherstellen(Platte p, std::string* fehler) {
        const std::string stempel = stufenStempel(p);
        std::string alt;
        if (dateiDa(stufeDatei(p, "p8ks")) && dateiDa(stufeDatei(p, "platte.img")) && dateiDa(stufeDatei(p, "start")) &&
            dateiInhalt(stufeDatei(p, "stempel"), alt) && alt == stempel) return true;
        std::fprintf(stderr, "  [Zustand '%s' wird gebaut (einmalig, ~6 Maschinenminuten)]\n", stufeVon(p).name.c_str());
        Rechner r;
        if (!r.plattenAnlegen(p, fehler)) return false;
        if (!fsckAus(r.l.platte)) std::fprintf(stderr, "  [Hinweis: rc_csh nicht angepasst, fsck laeuft mit]\n");
        Aufbau a; a.platte = p;
        if (!r.neu(a, fehler)) return false;
        if (!r.bisKernbanner(fehler)) return false;
        r.m().hdFlush();
        r.m().flushDisks();
        const std::vector<uint8_t> st = ohneRtcAbschnitt(r.m().stateBytes());
        {
            std::ofstream f(stufeDatei(p, "p8ks"), std::ios::binary);
            f.write(reinterpret_cast<const char*>(st.data()), std::streamsize(st.size()));
            if (!f) { *fehler = "p8ks nicht schreibbar"; return false; }
        }
        if (!kopiere(r.l.platte, stufeDatei(p, "platte.img")) || !kopiere(r.l.start, stufeDatei(p, "start"))) {
            *fehler = "Zustand nicht kopierbar";
            return false;
        }
        std::ofstream(stufeDatei(p, "stempel")) << stempel;
        return true;
    }

    /// Maschine aus dem Zustand „Kernbanner" (Platte frisch kopiert, ausser @p platte_behalten): Karte nach @p a,
    /// bei frischer Platte auf die Wirtszeit gestellt.  Mit @p platte_behalten (Neustart): Platte/Startdiskette/Batterie
    /// bleiben, die Karte beginnt nach der Startregel (nicht gestellt).
    bool ausStufe(const Aufbau& a, bool platte_behalten, std::string* fehler, bool wiederholt = false) {
        if (!stufeSicherstellen(a.platte, fehler)) return false;
        if (!platte_behalten) {
            if (!kopiere(stufeDatei(a.platte, "platte.img"), l.platte)) { *fehler = "Kopie Platte"; return false; }
            l.start = k1520test::tempPath(std::string("k1520_rtc_start.") + stufeVon(a.platte).start_ext);
            if (!kopiere(stufeDatei(a.platte, "start"), l.start)) { *fehler = "Kopie Start"; return false; }
        }
        if (!neu(a, fehler)) return false;
        std::string roh;
        dateiInhalt(stufeDatei(a.platte, "p8ks"), roh);
        const std::vector<uint8_t> st(roh.begin(), roh.end());
        if (!m().restoreStateBytes(st)) {
            // Der Emulator hat das Zustandsformat geaendert o. ae.: einmal neu bauen lassen (Stempel loeschen).
            if (!wiederholt) {
                std::error_code ec;
                std::filesystem::remove(stufeDatei(a.platte, "stempel"), ec);
                return ausStufe(a, platte_behalten, fehler, true);
            }
            *fehler = "Zustand nicht ladbar: " + m().stateError();
            return false;
        }
        if (!platte_behalten) m().karte16()->rtc()->stellen(a.wirt);
        return true;
    }

    /// Ab dem Kernbanner: Kernstart (liest die Karte), `/etc/datem` SOFORT beantworten (Datum nur RETURN, Zeit = der
    /// „Last known"-Wert auf die Minute: die Kernzeit bleibt, was die Karte lieferte), dann als root anmelden.
    bool anmelden(BootBefund& b, std::string* fehler) {
        P8000Machine& mm = m();
        static const std::string kDatum = "Enter Date (MM/DD/YY or <cr>):";
        static const std::string kZeit = "Enter Time (HH:MM):";
        for (;;) {
            const int i = warte(b, {"login:", "#1", kDatum}, 8'000'000'000LL);
            if (i < 0) { *fehler = bild(mm); return false; }
            if (i == 0) break;
            laufe(mm, 200'000);
            if (i == 1) { tippeZeile(mm, "init 2"); continue; }
            b.uhr_bei_datem = uhr();
            b.datum_gefragt = true;
            tippeZeile(mm, "");   // nur Return: das Datum bleibt
            if (warte(b, {kZeit}, 400'000'000) != 0) { *fehler = bild(mm); return false; }
            static const std::regex re(R"((\d{2}):(\d{2}):\d{2})");
            std::smatch mt;
            const std::string z = b.zuletzt();
            if (!std::regex_search(z, mt, re)) { *fehler = "keine Zeit in '" + z + "'\n" + bild(mm); return false; }
            laufe(mm, 200'000);
            tippeZeile(mm, mt[1].str() + ":" + mt[2].str());
        }
        b.login = true;
        laufe(mm, 200'000);
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

/// Eine Boot-Abnahme: Zustand laden, Kernstart, Befund, `date`.  Gibt alles auf stderr aus (Befundprotokoll).
struct Fall {
    BootBefund boot;
    std::string date;
    int64_t uhr_dabei = 0;
};

bool abnahme(const char* titel, const Aufbau& a, Fall& f, std::string* skip = nullptr) {
    Rechner r;
    std::string fehler;
    if (!r.ausStufe(a, false, &fehler) || !r.anmelden(f.boot, &fehler)) {
        if (fehler.rfind("SKIP:", 0) == 0 && skip) { *skip = fehler.substr(5); return false; }
        ADD_FAILURE() << titel << ": " << fehler;
        return false;
    }
    f.date = r.date(&f.uhr_dabei);
    std::fprintf(stderr, "[R4] %-34s datum_gefragt=%d  date: %s   (Karte: %lld)\n", titel, f.boot.datum_gefragt ? 1 : 0,
                 f.date.c_str(), static_cast<long long>(f.uhr_dabei));
    std::fprintf(stderr, "[R4]    datem: %s   (Karte bei datem: %lld)\n", f.boot.zuletzt().c_str(), static_cast<long long>(f.boot.uhr_bei_datem));
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

/// Prueft die Kernzeit („Last known date and time" von datem = Kernzeit nach dem Lesen der Karte) gegen die Gastzeit
/// @p gast (UTC-Kalender, mit Versatz), auf die die Karte beim Kernbanner gestellt wurde: die Zeile zeigt die Ortszeit
/// (TZ=MEZ-1MES: +@p zone_s) und ist um die Zeit vom Kernbanner bis zum Lesen der Karte weiter (Sekunden bis ~2 Min).
/// @p tage_frueh: Tage, um die der GAST (Kerntreiber) falsch rechnet.
void pruefeDatum(const Fall& f, int64_t gast, int zone_s, int tage_frueh = 0) {
    ASSERT_FALSE(f.date.empty());
    const int64_t d = datumSekunden(f.boot.zuletzt()) - (gast - int64_t(tage_frueh) * 86400 + zone_s);
    EXPECT_GE(d, 0) << f.boot.zuletzt();
    EXPECT_LE(d, 150) << f.boot.zuletzt();
    // `date` nach datem (Kernzeit auf die Minute gestellt) liegt dahinter, nicht davor.
    EXPECT_GE(datumSekunden(f.date), datumSekunden(f.boot.zuletzt()) - 60) << f.date;
    // Die Karte selbst hat den Wert richtig geliefert (nur der Kerntreiber irrt): Karte = Gastzeit + Laufzeit.
    EXPECT_GE(f.boot.uhr_bei_datem - gast, 0);
    EXPECT_LE(f.boot.uhr_bei_datem - gast, 150);
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
/// (datem nennt den letzten bekannten Stand aus dem Dateisystem), ohne Absturz — mit beiden Kartentypen.  Die Karte
/// steht auf 2031: ein Kern, der sie las, zeigte 2031 (der Dateisystemstand der Platte liegt davor).
TEST(P8000WegaRtc, W30_KernOhneUhrentreiberIgnoriertDieKarte) {
    stumm();
    for (const RtcTyp t : {RtcTyp::U130X, RtcTyp::Rtc72421}) {
        Fall f; std::string skip;
        if (!abnahme(t == RtcTyp::U130X ? "W30 U130X" : "W30 RTC-72421", aufbau(Platte::W30, t, 0, utc(2031, 5, 5, 14, 30)), f, &skip)) {
            if (!skip.empty()) GTEST_SKIP() << skip;
            return;
        }
        EXPECT_FALSE(f.boot.hat("RTC found"));
        EXPECT_FALSE(f.boot.hat("RTC-72421"));
        EXPECT_TRUE(f.boot.login);
        EXPECT_NE(datumJahr(f.date), 2031) << f.date;   // nicht der Kartenwert
        EXPECT_EQ(f.boot.zuletzt().find("2031"), std::string::npos) << f.boot.zuletzt();
    }
}

// Schaltjahr 2028 (Karte und Kern): 29.2. stimmt.  Im Maerz rechnet der Kerntreiber der W31-Platte einen Tag zu frueh
// (Gastfehler `timegm.c`, Entwurf 38 §7/§11) — die Karte liefert den richtigen Wert (Karte = Gastzeit).  Beide Karten
// nutzen denselben Kerntreiber.
void schaltjahr(RtcTyp t) {
    stumm();
    const char* n = t == RtcTyp::U130X ? "W31 U130X " : "W31 RTC-72421 ";
    for (const bool maerz : {false, true}) {
        Fall f; std::string skip;
        const int64_t w = maerz ? utc(2028, 3, 15, 12, 0) : utc(2028, 2, 29, 12, 0);
        if (!abnahme((std::string(n) + (maerz ? "2028-03-15" : "2028-02-29")).c_str(), aufbau(Platte::W31, t, 0, w), f, &skip)) { if (!skip.empty()) GTEST_SKIP() << skip; return; }
        pruefeDatum(f, w, 3600, maerz ? 1 : 0);
        EXPECT_EQ(f.date.find(maerz ? "Mar 14" : "Feb 29") != std::string::npos, true) << f.date;
    }
}

TEST(P8000WegaRtc, W31_U130X_Schaltjahr2028) { schaltjahr(RtcTyp::U130X); }
TEST(P8000WegaRtc, W31_Rtc72421_Schaltjahr2028) { schaltjahr(RtcTyp::Rtc72421); }

// ── 5. Zeitverhalten: Kernuhr (CTC) und Karte laufen in Maschinenzeit gleich schnell ────────────────

TEST(P8000WegaRtc, W31_U130X_KernUndKarteLaufenGleichSchnell) {
    stumm();
    Rechner r; BootBefund b; std::string fehler;
    Aufbau a = aufbau(Platte::W31, RtcTyp::U130X, 0, utc(2026, 10, 22, 14, 30));
    if (!r.ausStufe(a, false, &fehler)) { if (fehler.rfind("SKIP:", 0) == 0) GTEST_SKIP() << fehler.substr(5); FAIL() << fehler; }
    ASSERT_TRUE(r.anmelden(b, &fehler)) << fehler;
    int64_t u1 = 0, u2 = 0;
    const std::string d1 = r.date(&u1);
    ASSERT_FALSE(d1.empty());
    // Versatz Kern zu Karte: Ortszeit - GMT, ein ganzes Vielfaches von Stunden (MES = +2 h im Oktober).  datem hat die
    // Kernzeit auf die Minute gestellt (bis 59 s zurueck): der Zonenabstand stimmt daher auf eine Minute.
    const int64_t off = datumSekunden(d1) - u1;
    const int64_t std_h = (off + 1800) / 3600;
    std::fprintf(stderr, "[R4] Zeitverhalten: date1 %s  Karte %lld  Zonenabstand %lld s\n", d1.c_str(), static_cast<long long>(u1),
                 static_cast<long long>(off));
    EXPECT_LE(std::llabs(off - std_h * 3600), 61) << "Kernuhr und Karte weichen schon beim Start voneinander ab";
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
    if (!r.ausStufe(a, false, &fehler)) { if (fehler.rfind("SKIP:", 0) == 0) GTEST_SKIP() << fehler.substr(5); FAIL() << fehler; }
    ASSERT_TRUE(r.anmelden(b, &fehler)) << fehler;
    laufe(r.m(), 2LL * 60 * 4'000'000);                  // die Maschine laeuft Minuten, die Wirtszeit steht
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
    // ist um 30 s weiter - weit weniger als die gelaufene Maschinenzeit.  Die neue Maschine beginnt im Zustand
    // „Kernbanner" (kein Voll-Kaltstart), ihre Karte aber nach der Startregel aus der Batteriedatei.
    const int64_t stand_a = r.uhr();
    r.m().powerOff();
    r.m().hdFlush();
    r.m().flushDisks();
    r.l.m.reset();
    a.wirt += 30;
    ASSERT_TRUE(r.ausStufe(a, true, &fehler)) << fehler;
    EXPECT_GE(r.uhr(), stand_a) << "Karte nach dem Neubau hinter dem Stand der alten";
    BootBefund b2;
    ASSERT_TRUE(r.anmelden(b2, &fehler)) << fehler;
    int64_t u_b = 0;
    const std::string d_b = r.date(&u_b);
    ASSERT_FALSE(d_b.empty());
    std::fprintf(stderr, "[R4] Neustart: Karte A-Ende %lld  datem B: %s  date B: %s  Karte B: %lld\n", static_cast<long long>(stand_a),
                 b2.zuletzt().c_str(), d_b.c_str(), static_cast<long long>(u_b));
    // Kernzeit nach dem Lesen der Karte (datem-Zeile, noch vor dem Stellen) und `date` danach: beide nicht vor der Dateizeit.
    EXPECT_GE(datumSekunden(b2.zuletzt()), mtime) << "Kernzeit nach dem Neustart liegt vor der Dateizeit";
    EXPECT_GE(datumSekunden(d_b), mtime) << "date nach dem Neustart liegt vor der Dateizeit";
    EXPECT_GE(u_b, stand_a);
    std::string ls2;
    ASSERT_TRUE(r.kommando("ls -l /probe", &ls2));
    EXPECT_NE(ls2.find("/probe"), std::string::npos) << ls2;   // die Datei hat den Neustart ueberlebt
    std::remove(batterie.c_str());
}
