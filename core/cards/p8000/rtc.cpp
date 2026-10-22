/**
 * @file rtc.cpp
 * @brief Echtzeituhr-Karte des P8000 — RTC-72421 (siehe rtc.h fuer Modell und Annahmen).
 */

#include "core/cards/p8000/rtc.h"
#include "core/logger.h"
#include "core/util/zustand.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace {

constexpr uint8_t SAVE_VERSION = 1;
constexpr uint8_t KENNUNG = 0x30;   ///< obere vier Bit beim Lesen (Pull-ups, Treiber `STR_R421`)

int64_t floorDiv(int64_t a, int64_t b) { int64_t q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }

bool schaltjahr(int j) { return (j % 4 == 0 && j % 100 != 0) || j % 400 == 0; }
int monatsTage(int j, int mo) {
    static const int t[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return mo == 2 && schaltjahr(j) ? 29 : t[mo - 1];
}

/// Tage seit 1970-01-01 (proleptisch gregorianisch, Algorithmus nach H. Hinnant).
int64_t tageSeit1970(int j, int mo, int t) {
    const int64_t y = j - (mo <= 2);
    const int64_t era = floorDiv(y, 400);
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + t - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void zivilAusTagen(int64_t z, int& j, int& mo, int& t) {
    z += 719468;
    const int64_t era = floorDiv(z, 146097);
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t y = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    t = int(doy - (153 * mp + 2) / 5 + 1);
    mo = int(mp < 10 ? mp + 3 : mp - 9);
    j = int(y + (mo <= 2));
}

int klemme(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

std::unique_ptr<P8000Rtc> P8000Rtc::anlegen(const P8000RtcConfig& cfg, std::string& fehler) {
    fehler.clear();
    switch (cfg.typ) {
        case P8000RtcTyp::Rtc72421: return std::make_unique<P8000Rtc72421>(cfg);
        case P8000RtcTyp::U130X:    fehler = "P8000: RTC U130X noch nicht verfuegbar"; return nullptr;
        default:                    return nullptr;
    }
}

P8000Rtc72421::P8000Rtc72421(const P8000RtcConfig& cfg) : cfg_(cfg) {
    if (cfg_.takt_hz == 0) cfg_.takt_hz = 4'000'000;
    busy_takte_ = uint64_t(cfg_.takt_hz) * BUSY_US / 1'000'000;
    laden();
}

int64_t P8000Rtc72421::wirtWand() const {
    const int64_t t = cfg_.zeitquelle ? cfg_.zeitquelle() : int64_t(std::time(nullptr));
    if (cfg_.utc) return t;
    // Ortszeit ohne tm_gmtoff (Windows kennt es nicht): die Wanduhr als „Sekunden seit 1970" lesen.
    const std::time_t tt = std::time_t(t);
    std::tm lt{};
#ifdef _WIN32
    if (localtime_s(&lt, &tt) != 0) return t;
#else
    if (!localtime_r(&tt, &lt)) return t;
#endif
    return tageSeit1970(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday) * 86400 + lt.tm_hour * 3600 +
           lt.tm_min * 60 + lt.tm_sec;
}

// Stellregister aus einer Wandzeit; Jahresversatz nur hier (Anzeige).  [R1]
void P8000Rtc72421::ladeNibbel(int64_t wand, uint8_t* n) const {
    const int64_t tage = floorDiv(wand, 86400);
    const int sod = int(wand - tage * 86400);
    int j, mo, t;
    zivilAusTagen(tage, j, mo, t);
    const int dow = int(((tage % 7) + 11) % 7);   // 1970-01-01 war ein Donnerstag (4)
    j += cfg_.jahresversatz;
    if (mo == 2 && t == 29 && !schaltjahr(j)) t = 28;
    const int h = sod / 3600, mi = sod / 60 % 60, s = sod % 60;
    int hh = h, pm = 0;
    if (!(f_ & 0x04)) { hh = h % 12 == 0 ? 12 : h % 12; pm = h >= 12 ? 4 : 0; }
    const int jj = ((j % 100) + 100) % 100;
    n[0] = uint8_t(s % 10);  n[1] = uint8_t(s / 10);
    n[2] = uint8_t(mi % 10); n[3] = uint8_t(mi / 10);
    n[4] = uint8_t(hh % 10); n[5] = uint8_t(hh / 10 | pm);
    n[6] = uint8_t(t % 10);  n[7] = uint8_t(t / 10);
    n[8] = uint8_t(mo % 10); n[9] = uint8_t(mo / 10);
    n[10] = uint8_t(jj % 10); n[11] = uint8_t(jj / 10);
    n[12] = uint8_t(dow);
}

// Stellregister → Wandzeit der Uhr (ohne Jahresversatz).  Unbrauchbares wird begrenzt [R3, R4].
bool P8000Rtc72421::wandAusNibbeln(int64_t& wand) const {
    const uint8_t* n = zeit_;
    const int s = klemme(n[1] * 10 + n[0], 0, 59);
    const int mi = klemme(n[3] * 10 + n[2], 0, 59);
    int h;
    if (f_ & 0x04) h = klemme((n[5] & 3) * 10 + n[4], 0, 23);
    else h = klemme((n[5] & 3) * 10 + n[4], 1, 12) % 12 + ((n[5] & 4) ? 12 : 0);
    const int mo = klemme((n[9] & 1) * 10 + n[8], 1, 12);
    const int yy = klemme(n[11] * 10 + n[10], 0, 99);
    // Jahrhundert: das dem jetzt angezeigten Jahr naechste.
    int jetzt, m_, t_;
    zivilAusTagen(floorDiv(uhrWand(), 86400), jetzt, m_, t_);
    jetzt += cfg_.jahresversatz;
    int j = jetzt / 100 * 100 + yy;
    if (j - jetzt > 50) j -= 100;
    else if (jetzt - j > 50) j += 100;
    j -= cfg_.jahresversatz;
    const int t = klemme((n[7] & 3) * 10 + n[6], 1, monatsTage(j, mo));
    wand = tageSeit1970(j, mo, t) * 86400 + h * 3600 + mi * 60 + s;
    return true;
}

void P8000Rtc72421::festschreiben() {
    int64_t wand;
    if (!wandAusNibbeln(wand)) return;
    versatz_ = wand - wirtWand();
    sichern();
    LOG_INFO("P8000", "RTC-72421 gestellt, Versatz zur Wirtszeit %lld s", static_cast<long long>(versatz_));
}

void P8000Rtc72421::minuteRunden() {
    const int s = int(uhrWand() - floorDiv(uhrWand(), 60) * 60);
    versatz_ += s >= 30 ? 60 - s : -s;
    sichern();
}

bool P8000Rtc72421::lesen(uint16_t port, uint8_t& d) {
    if (!(port & 1) || (port & 0xFFE0) != 0xFDE0) return false;   // [R7]
    const int n = (port >> 1) & 0x0F;
    uint8_t v = 0;
    if (n <= 12) {
        if (hold_ || angehalten()) v = zeit_[n];
        else {
            uint8_t lebend[13];
            ladeNibbel(uhrWand(), lebend);
            v = lebend[n];
        }
    } else if (n == 13) {
        const bool busy = phase_ < busy_takte_;
        v = uint8_t((hold_ ? 1 : 0) | (busy ? 2 : 0) | (d_ & 0x04));
    } else if (n == 14) v = e_;
    else v = f_;
    d = uint8_t(KENNUNG | (v & 0x0F));
    return true;
}

bool P8000Rtc72421::schreiben(uint16_t port, uint8_t d) {
    if (!(port & 1) || (port & 0xFFE0) != 0xFDE0) return false;
    const int n = (port >> 1) & 0x0F;
    const uint8_t v = d & 0x0F;
    if (n <= 12) {
        if (angehalten()) { zeit_[n] = v; stell_geaendert_ = true; }   // [R3]
    } else if (n == 13) {
        const bool neu = v & 1;
        if (neu && !hold_ && !angehalten()) ladeNibbel(uhrWand(), zeit_);  // [R2]
        hold_ = neu;
        d_ = uint8_t(v & 0x04);
        if (v & 0x08) minuteRunden();                                   // [R6]
    } else if (n == 14) {
        e_ = v;
    } else {
        const bool war = angehalten();
        f_ = v;
        if (!war && angehalten()) { ladeNibbel(uhrWand(), zeit_); stell_geaendert_ = false; }
        else if (war && !angehalten() && stell_geaendert_) { festschreiben(); stell_geaendert_ = false; }
    }
    return true;
}

void P8000Rtc72421::takte(int n) {
    if (n > 0) phase_ = (phase_ + uint64_t(n)) % cfg_.takt_hz;
}

// ─── Batteriedatei ───────────────────────────────────────────────────────────

void P8000Rtc72421::sichern() const {
    if (cfg_.zustand.empty()) return;
    std::ofstream f(cfg_.zustand, std::ios::trunc);
    if (!f) { LOG_WARN("P8000", "RTC: Batteriedatei '%s' nicht schreibbar", cfg_.zustand.c_str()); return; }
    f << "K1520-P8000-RTC 1\nversatz=" << versatz_ << "\n";
}

void P8000Rtc72421::laden() {
    if (cfg_.zustand.empty()) return;
    std::ifstream f(cfg_.zustand);
    if (!f) return;   // noch keine Batteriedatei: Uhr = Wirtszeit
    std::string kopf, zeile;
    std::getline(f, kopf);
    if (kopf != "K1520-P8000-RTC 1") { LOG_WARN("P8000", "RTC: Batteriedatei '%s' unbekannt", cfg_.zustand.c_str()); return; }
    while (std::getline(f, zeile))
        if (zeile.compare(0, 8, "versatz=") == 0) {
            try { versatz_ = std::stoll(zeile.substr(8)); } catch (...) { versatz_ = 0; }
        }
}

// ─── Save-State ──────────────────────────────────────────────────────────────

void P8000Rtc72421::serialize(std::vector<uint8_t>& out) const {
    out.push_back(SAVE_VERSION);
    auto a = k1520::ZAr::schreiber(out);
    int64_t vs = versatz_;
    uint8_t d = d_, e = e_, f = f_;
    bool h = hold_, g = stell_geaendert_;
    uint64_t ph = phase_;
    a.num(vs); a.num(d); a.num(e); a.num(f); a.flag(h); a.flag(g); a.num(ph);
    a.raw(const_cast<uint8_t*>(zeit_), sizeof zeit_);
}

bool P8000Rtc72421::deserialize(const uint8_t*& p, const uint8_t* end) {
    if (end - p < 1 || *p != SAVE_VERSION) return false;
    ++p;
    auto a = k1520::ZAr::leser(p, end);
    int64_t vs = 0;
    uint8_t d = 0, e = 0, f = 0;
    bool h = false, g = false;
    uint64_t ph = 0;
    uint8_t z[sizeof zeit_] = {};
    a.num(vs); a.num(d); a.num(e); a.num(f); a.flag(h); a.flag(g); a.num(ph);
    a.raw(z, sizeof z);
    if (!a.ok) return false;
    p = a.p;
    versatz_ = vs; d_ = d & 0x04; e_ = e & 0x0F; f_ = f & 0x0F; hold_ = h; stell_geaendert_ = g;
    phase_ = ph % cfg_.takt_hz;
    for (size_t i = 0; i < sizeof z; ++i) zeit_[i] = z[i] & 0x0F;
    sichern();   // die Batteriedatei folgt dem geladenen Stand
    return true;
}
