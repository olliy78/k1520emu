/**
 * @file rtc.cpp
 * @brief Echtzeituhr-Karte des P8000 — RTC-72421 (siehe rtc.h fuer Modell und Annahmen).
 */

#include "core/cards/p8000/rtc.h"
#include "core/logger.h"
#include "core/util/zustand.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace {

constexpr uint8_t SAVE_VERSION = 2;   // 1 = R1/R2 (Versatz), ladbar
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

int64_t unixJetzt(const std::function<int64_t()>& zq) { return zq ? zq() : int64_t(std::time(nullptr)); }

/// Wirtszeit (Unix UTC) als Wandzeit: UTC oder Ortszeit des Wirts.
int64_t wandAusUnix(int64_t t, bool utc) {
    if (utc) return t;
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

/// Wandzeit mit Jahresversatz (nur Anzeige/Gast); ein 29.2. im Nicht-Schaltjahr wird 28.2.
int64_t mitJahresversatz(int64_t wand, int versatz) {
    if (versatz == 0) return wand;
    const int64_t tage = floorDiv(wand, 86400);
    const int64_t sod = wand - tage * 86400;
    int j, mo, t;
    zivilAusTagen(tage, j, mo, t);
    j += versatz;
    if (mo == 2 && t == 29 && !schaltjahr(j)) t = 28;
    return tageSeit1970(j, mo, t) * 86400 + sod;
}

// ─── Batteriedatei ───────────────────────────────────────────────────────────

struct Batterie {
    int fassung = 0;       ///< 0 = keine/unbrauchbar, 1 = R1 (versatz=), 2 = Stand + Wirtszeit
    int64_t stand = 0, wirt = 0, versatz = 0;
};

Batterie leseBatterie(const std::string& pfad) {
    Batterie b;
    if (pfad.empty()) return b;
    std::ifstream f(pfad);
    if (!f) return b;   // noch keine Batteriedatei
    std::string kopf, zeile;
    std::getline(f, kopf);
    const int fassung = kopf == "K1520-P8000-RTC 2" ? 2 : kopf == "K1520-P8000-RTC 1" ? 1 : 0;
    if (!fassung) { LOG_WARN("P8000", "RTC: Batteriedatei '%s' unbekannt", pfad.c_str()); return b; }
    bool stand = false, wirt = false;
    auto zahl = [](const std::string& z, size_t n, int64_t& v) {
        try { v = std::stoll(z.substr(n)); return true; } catch (...) { return false; }
    };
    while (std::getline(f, zeile)) {
        if (zeile.compare(0, 6, "stand=") == 0) stand = zahl(zeile, 6, b.stand);
        else if (zeile.compare(0, 5, "wirt=") == 0) wirt = zahl(zeile, 5, b.wirt);
        else if (zeile.compare(0, 8, "versatz=") == 0) zahl(zeile, 8, b.versatz);
    }
    if (fassung == 2 && stand && wirt) b.fassung = 2;
    else if (fassung == 1) { b.fassung = 1; }
    return b;
}

bool schreibeBatterie(const std::string& pfad, int64_t stand, int64_t wirt) {
    std::ofstream f(pfad, std::ios::trunc);
    if (!f) { LOG_WARN("P8000", "RTC: Batteriedatei '%s' nicht schreibbar", pfad.c_str()); return false; }
    f << "K1520-P8000-RTC 2\nstand=" << stand << "\nwirt=" << wirt << "\n";
    return bool(f);
}

/// Startregel: max(Stand + Abwesenheit, Wirtszeit jetzt); Abwesenheit < 0 zaehlt als 0.
int64_t startWand(const std::string& pfad, bool utc, const std::function<int64_t()>& zq) {
    const int64_t jetzt = unixJetzt(zq);
    const int64_t host = wandAusUnix(jetzt, utc);
    const Batterie b = leseBatterie(pfad);
    if (b.fassung == 2) {
        const int64_t weg = jetzt > b.wirt ? jetzt - b.wirt : 0;
        return std::max(b.stand + weg, host);
    }
    if (b.fassung == 1) return std::max(host + b.versatz, host);
    return host;
}

}  // namespace

int64_t P8000Rtc::startzeit(const std::string& zustand, int jahresversatz, bool utc,
                            const std::function<int64_t()>& zeitquelle) {
    return mitJahresversatz(startWand(zustand, utc, zeitquelle), jahresversatz);
}

bool P8000Rtc::stellenDatei(const std::string& zustand, int64_t wand, const std::function<int64_t()>& zeitquelle) {
    if (zustand.empty()) return false;
    return schreibeBatterie(zustand, wand, unixJetzt(zeitquelle));
}

std::unique_ptr<P8000Rtc> P8000Rtc::anlegen(const P8000RtcConfig& cfg, std::string& fehler) {
    fehler.clear();
    switch (cfg.typ) {
        case P8000RtcTyp::Rtc72421: return std::make_unique<P8000Rtc72421>(cfg);
        case P8000RtcTyp::U130X:    return std::make_unique<P8000RtcU130>(cfg);
        default:                    return nullptr;
    }
}

P8000RtcBasis::P8000RtcBasis(const P8000RtcConfig& cfg) : cfg_(cfg) {
    if (cfg_.takt_hz == 0) cfg_.takt_hz = 4'000'000;
    sek_ = startWand(cfg_.zustand, cfg_.utc, cfg_.zeitquelle);
}

P8000RtcBasis::~P8000RtcBasis() { sichern(); }

int64_t P8000RtcBasis::gastZeit() const { return mitJahresversatz(sek_, cfg_.jahresversatz); }

void P8000RtcBasis::stellen(int64_t wand) { setzeUhr(wand, true); }

void P8000RtcBasis::setzeUhr(int64_t wand, bool sekunde0) {
    sek_ = wand;
    if (sekunde0) rest_ = 0;
    sichern();
}

// Maschinenzeit weiter; je 60 s Maschinenzeit die Batteriedatei fortschreiben.
void P8000RtcBasis::zaehle(int n) {
    if (n <= 0) return;
    rest_ += uint64_t(n);
    if (rest_ >= cfg_.takt_hz) { sek_ += int64_t(rest_ / cfg_.takt_hz); rest_ %= cfg_.takt_hz; }
    seit_sicher_ += uint64_t(n);
    if (seit_sicher_ >= uint64_t(SICHER_SEKUNDEN) * cfg_.takt_hz) sichern();
}

P8000Rtc72421::P8000Rtc72421(const P8000RtcConfig& cfg) : P8000RtcBasis(cfg) {
    busy_takte_ = uint64_t(cfg_.takt_hz) * BUSY_US / 1'000'000;
}

int64_t P8000RtcBasis::wirtWand() const { return wandAusUnix(unixJetzt(cfg_.zeitquelle), cfg_.utc); }

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
    setzeUhr(wand, true);
    LOG_INFO("P8000", "RTC-72421 gestellt, Uhr %s", uhrText().c_str());
}

void P8000Rtc72421::minuteRunden() {
    const int s = int(uhrWand() - floorDiv(uhrWand(), 60) * 60);
    setzeUhr(sek_ + (s >= 30 ? 60 - s : -s), true);
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
    if (n > 0) { phase_ = (phase_ + uint64_t(n)) % cfg_.takt_hz; zaehle(n); }
}

// ─── Batteriedatei ───────────────────────────────────────────────────────────

std::string P8000RtcBasis::uhrText() const {
    const int64_t w = uhrWand();
    const int64_t tage = floorDiv(w, 86400);
    const int sod = int(w - tage * 86400);
    int j, m, t;
    zivilAusTagen(tage, j, m, t);
    char b[48];
    snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d:%02d", j + cfg_.jahresversatz, m, t, sod / 3600, sod / 60 % 60, sod % 60);
    return b;
}

std::string P8000Rtc72421::text() const {
    char b[160];
    snprintf(b, sizeof b, "RTC-72421  Uhr %s %s  Abstand zur Wirtszeit %lld s  D=%X E=%X F=%X%s", uhrText().c_str(),
             cfg_.utc ? "UTC" : "Ort", static_cast<long long>(versatz()), d_ | (hold_ ? 1 : 0), e_, f_,
             angehalten() ? "  (STOP/RESET)" : "");
    return b;
}

std::string P8000RtcU130::text() const {
    int pos[6];
    ziffern(pos);
    char z[8];
    for (int i = 0; i < 6; ++i) z[5 - i] = pos[i] < 0 ? ' ' : char('0' + pos[i]);
    z[6] = 0;
    char b[200];
    snprintf(b, sizeof b, "U130X  Uhr %s %s  Abstand zur Wirtszeit %lld s  Anzeige [%s] %s  Tasten=%X  Zustand %s", uhrText().c_str(),
             cfg_.utc ? "UTC" : "Ort", static_cast<long long>(versatz()), z, datumAnzeige() ? "Datum (MMTTJJ)" : "Zeit (hhmmss)",
             tasten_, zustandName(z_));
    return b;
}

void P8000RtcBasis::sichern() const {
    if (cfg_.zustand.empty()) return;
    seit_sicher_ = 0;
    schreibeBatterie(cfg_.zustand, sek_, unixJetzt(cfg_.zeitquelle));
}

// ─── Save-State ──────────────────────────────────────────────────────────────

void P8000Rtc72421::serialize(std::vector<uint8_t>& out) const {
    out.push_back(SAVE_VERSION);
    auto a = k1520::ZAr::schreiber(out);
    int64_t sk = sek_;
    uint64_t rs = rest_;
    uint8_t d = d_, e = e_, f = f_;
    bool h = hold_, g = stell_geaendert_;
    uint64_t ph = phase_;
    a.num(sk); a.num(rs); a.num(d); a.num(e); a.num(f); a.flag(h); a.flag(g); a.num(ph);
    a.raw(const_cast<uint8_t*>(zeit_), sizeof zeit_);
}

bool P8000Rtc72421::deserialize(const uint8_t*& p, const uint8_t* end) {
    if (end - p < 1 || (*p != SAVE_VERSION && *p != 1)) return false;
    const bool alt = *p == 1;   // R1/R2: Versatz zur Wirtszeit statt Uhrstand
    ++p;
    auto a = k1520::ZAr::leser(p, end);
    int64_t vs = 0;
    uint64_t rs = 0;
    uint8_t d = 0, e = 0, f = 0;
    bool h = false, g = false;
    uint64_t ph = 0;
    uint8_t z[sizeof zeit_] = {};
    a.num(vs);
    if (!alt) a.num(rs);
    a.num(d); a.num(e); a.num(f); a.flag(h); a.flag(g); a.num(ph);
    a.raw(z, sizeof z);
    if (!a.ok) return false;
    p = a.p;
    if (alt) { sek_ = wirtWand() + vs; rest_ = 0; } else { sek_ = vs; rest_ = rs % cfg_.takt_hz; }
    d_ = d & 0x04; e_ = e & 0x0F; f_ = f & 0x0F; hold_ = h; stell_geaendert_ = g;
    phase_ = ph % cfg_.takt_hz;
    for (size_t i = 0; i < sizeof z; ++i) zeit_[i] = z[i] & 0x0F;
    sichern();   // die Batteriedatei folgt dem geladenen Stand
    return true;
}

// ═══ U130X (Original) ════════════════════════════════════════════════════════
// Modell und Annahmen [U1]-[U9]: siehe rtc.h.

namespace {
constexpr uint8_t U130_SAVE = 0x82;   // 0x81 = R2 (Versatz), ladbar
// Segmentcodes der fuenf abgetasteten Segmente je Ziffer (Bit0-4, aktiv = 1), wie `invalue()` der Treiber.
constexpr uint8_t SEG7[10] = {0x0F, 0x02, 0x17, 0x13, 0x1A, 0x19, 0x1D, 0x03, 0x1F, 0x1B};
}  // namespace

P8000RtcU130::P8000RtcU130(const P8000RtcConfig& cfg) : P8000RtcBasis(cfg) {
    com_takte_ = cfg_.takt_hz / 1000 ? cfg_.takt_hz / 1000 : 1;   // [U2] 1 ms
    schritt_takte_ = cfg_.takt_hz * 3 / 20 ? cfg_.takt_hz * 3 / 20 : 1;   // [U6] 150 ms
}

const char* P8000RtcU130::zustandName(Z z) {
    static const char* n[] = {"Zeit", "Datum", "Korr.Monat", "Korr.TagZ", "Korr.TagE", "Korr.JahrZ", "Korr.JahrE",
                              "Zeit-Sek", "Zeit-Vor", "Korr.Std", "Korr.MinZ", "Korr.MinE", "Halt"};
    return size_t(z) < sizeof n / sizeof *n ? n[size_t(z)] : "?";
}

// Sechs Stellen in der Reihenfolge der Adressen FEF5..FEFF; -1 = leer.
void P8000RtcU130::ziffern(int* pos) const {
    if (datumAnzeige()) {
        int mo, d1, d0, j1, j0;
        if (korrDatum()) { mo = mo_; d1 = dz_; d0 = de_; j1 = jz_; j0 = je_; }
        else {
            int j, m, t;
            zivilAusTagen(floorDiv(uhrWand(), 86400), j, m, t);
            j += cfg_.jahresversatz;
            if (m == 2 && t == 29 && !schaltjahr(j)) t = 28;
            const int jj = ((j % 100) + 100) % 100;
            mo = m; d1 = t / 10; d0 = t % 10; j1 = jj / 10; j0 = jj % 10;
        }
        pos[0] = j0; pos[1] = j1; pos[2] = d0; pos[3] = d1; pos[4] = mo % 10; pos[5] = mo >= 10 ? mo / 10 : -1;
    } else {
        const int64_t w = eingefroren() ? frozen_ : uhrWand();
        const int sod = int(w - floorDiv(w, 86400) * 86400);
        int h = sod / 3600, mi = sod / 60 % 60;
        const int s = sod % 60;
        if (korrZeit()) { h = std_; mi = mz_ * 10 + me_; }
        pos[0] = s % 10; pos[1] = s / 10; pos[2] = mi % 10; pos[3] = mi / 10; pos[4] = h % 10;
        pos[5] = h >= 10 ? h / 10 : -1;   // [U4]
    }
}

uint8_t P8000RtcU130::lesenStelle(int idx) {
    int pos[6];
    ziffern(pos);
    // [U10] Stellenpaar: der Treiber liest Einer und Zehner nacheinander (rund 20 us auseinander); die echte
    // Anzeige wechselt beide Stellen gleichzeitig.  Die Karte haelt darum beim Lesen des Einers den Zehner
    // fest und gibt ihn bei der folgenden Zehnerlesung (binnen 100 us) heraus: kein zerrissener Wert beim
    // Umlauf (59 -> 00, Monat 12 -> 1, Stunde 9 -> 10), der den Treiber ins Leere laufen liesse.
    if ((idx & 1) == 0) { zehner_idx_ = idx + 1; zehner_ = pos[idx + 1]; zehner_stempel_ = total_; }
    else if (zehner_idx_ == idx && total_ - zehner_stempel_ <= 400) pos[idx] = zehner_;
    const bool c = com();
    const int v = pos[idx];
    uint8_t code = 0;
    if (idx == 5) code = v == 1 ? 0x02 : v == 2 ? 0x05 : 0x00;   // nur Bit0-2 sind abgetastet
    else if (v >= 0) code = SEG7[v];
    // Segment aktiv <=> Pegel != COM
    uint8_t r = 0;
    const int bits = idx == 5 ? 3 : 5;
    for (int b = 0; b < bits; ++b) r |= uint8_t((((code >> b) & 1) ? !c : c) << b);
    if (idx == 5) {
        const bool dat = datumAnzeige();
        r |= uint8_t((dat ? !c : c) << 3);
        r |= uint8_t(c << 4);
        if ((total_ % cfg_.takt_hz) < cfg_.takt_hz / 2) r |= 0x80;   // Sekundentakt [U2]
    }
    if (c) r |= 0x20;
    return r;
}

bool P8000RtcU130::lesen(uint16_t port, uint8_t& d) {
    if (!(port & 1)) return false;
    if (port == BASIS) { d = KENNUNG; return true; }
    if (port < 0xFEF5 || port > 0xFEFF) return false;   // [U8]
    d = lesenStelle(int((port - 0xFEF5) >> 1));
    return true;
}

bool P8000RtcU130::schreiben(uint16_t port, uint8_t d) {
    if (!(port & 1) || port < BASIS || port > 0xFEFF) return false;
    if (port == BASIS) taste(d & 0x07);
    return true;
}

void P8000RtcU130::taste(uint8_t neu) {
    const uint8_t flanken = neu & uint8_t(~tasten_);
    const bool htGehalten = (neu & 0x02) != 0;
    tasten_ = neu;
    if (flanken & 0x02) ht();
    if (flanken & 0x01) vt(htGehalten);
}

void P8000RtcU130::ht() {
    switch (z_) {
        case Z::Zeit:  z_ = Z::Datum; datum_bis_ = total_ + 2ull * cfg_.takt_hz; break;
        case Z::Datum: z_ = Z::Zeit; break;
        case Z::ZSek: {
            const int s = int(frozen_ - floorDiv(frozen_, 60) * 60);
            frozen_ += s >= 30 ? 60 - s : -s;
            break;
        }
        default:
            if (korrDatum()) { datumUebernehmen(); z_ = Z::Zeit; }
            break;   // [U7]
    }
}

void P8000RtcU130::vt(bool htGehalten) {
    switch (z_) {
        case Z::Zeit:
        case Z::Datum:
            if (z_ == Z::Datum && htGehalten) { ladeDatumStell(); z_ = Z::KMonat; }
            else { frozen_ = uhrWand(); z_ = Z::ZSek; }
            break;
        case Z::KMonat: z_ = Z::KTagZ; break;
        case Z::KTagZ:  z_ = Z::KTagE; break;
        case Z::KTagE:  z_ = Z::KJahrZ; break;
        case Z::KJahrZ: z_ = Z::KJahrE; break;
        case Z::KJahrE: datumUebernehmen(); z_ = Z::Zeit; return;
        case Z::ZSek:   z_ = Z::ZVor; return;
        case Z::ZVor:   ladeZeitStell(); z_ = Z::KStd; break;
        case Z::KStd:   z_ = Z::KMinZ; break;
        case Z::KMinZ:  z_ = Z::KMinE; break;
        case Z::KMinE: {
            const int64_t tage = floorDiv(frozen_, 86400);
            const int s = int(frozen_ - floorDiv(frozen_, 60) * 60);   // [U9]
            frozen_ = tage * 86400 + std_ * 3600 + (mz_ * 10 + me_) * 60 + s;
            z_ = Z::Halt;
            return;
        }
        case Z::Halt:   // Start der Zeit
            setzeUhr(frozen_, true);
            LOG_INFO("P8000", "U130X gestellt, Uhr %s", uhrText().c_str());
            z_ = Z::Zeit;
            return;
        default: return;
    }
    schritt_bis_ = total_ + schritt_takte_;
}

void P8000RtcU130::ladeDatumStell() {
    int j, m, t;
    zivilAusTagen(floorDiv(uhrWand(), 86400), j, m, t);
    j += cfg_.jahresversatz;
    if (m == 2 && t == 29 && !schaltjahr(j)) t = 28;
    const int jj = ((j % 100) + 100) % 100;
    mo_ = uint8_t(m); dz_ = uint8_t(t / 10); de_ = uint8_t(t % 10); jz_ = uint8_t(jj / 10); je_ = uint8_t(jj % 10);
    schritt_bis_ = total_ + schritt_takte_;
}

void P8000RtcU130::ladeZeitStell() {
    const int sod = int(frozen_ - floorDiv(frozen_, 86400) * 86400);
    std_ = uint8_t(sod / 3600); mz_ = uint8_t(sod / 60 % 60 / 10); me_ = uint8_t(sod / 60 % 10);
}

// Gestelltes Datum (Zeit des Tages bleibt) in den Versatz uebernehmen; [R4]-Jahrhundertwahl wie beim 72421.
void P8000RtcU130::datumUebernehmen() {
    const int64_t wand = uhrWand();
    const int64_t tage = floorDiv(wand, 86400);
    const int64_t sod = wand - tage * 86400;
    int jetzt, m_, t_;
    zivilAusTagen(tage, jetzt, m_, t_);
    jetzt += cfg_.jahresversatz;
    const int yy = jz_ * 10 + je_;
    int j = jetzt / 100 * 100 + yy;
    if (j - jetzt > 50) j -= 100;
    else if (jetzt - j > 50) j += 100;
    j -= cfg_.jahresversatz;
    const int mo = klemme(mo_, 1, 12);
    const int t = klemme(dz_ * 10 + de_, 1, monatsTage(j, mo));
    setzeUhr(sek_ + tageSeit1970(j, mo, t) * 86400 + sod - wand, false);
    LOG_INFO("P8000", "U130X Datum gestellt, Uhr %s", uhrText().c_str());
}

void P8000RtcU130::schritt() {
    switch (z_) {
        case Z::KMonat: mo_ = uint8_t(mo_ % 12 + 1); break;
        case Z::KTagZ:  dz_ = uint8_t((dz_ + 1) % 4); break;
        case Z::KTagE:  de_ = uint8_t((de_ + 1) % 10); break;
        case Z::KJahrZ: jz_ = uint8_t((jz_ + 1) % 10); break;
        case Z::KJahrE: je_ = uint8_t((je_ + 1) % 10); break;
        case Z::KStd:   std_ = uint8_t((std_ + 1) % 24); break;
        case Z::KMinZ:  mz_ = uint8_t((mz_ + 1) % 6); break;
        case Z::KMinE:  me_ = uint8_t((me_ + 1) % 10); break;
        default: break;
    }
}

void P8000RtcU130::takte(int n) {
    if (n <= 0) return;
    total_ += uint64_t(n);
    zaehle(n);
    if (z_ == Z::Datum && total_ >= datum_bis_) z_ = Z::Zeit;
    if ((korrDatum() || korrZeit()) && total_ >= schritt_bis_) {
        uint64_t k = (total_ - schritt_bis_) / schritt_takte_ + 1;
        schritt_bis_ += k * schritt_takte_;
        k %= 120;   // kgV der Stellenbereiche 12, 4, 10, 24, 6 - die Abkuerzung ist exakt
        while (k--) schritt();
    }
}

void P8000RtcU130::serialize(std::vector<uint8_t>& out) const {
    out.push_back(U130_SAVE);
    auto a = k1520::ZAr::schreiber(out);
    int64_t vs = sek_, fr = frozen_;
    uint64_t rs = rest_;
    uint8_t z = uint8_t(z_), t = tasten_, mo = mo_, dz = dz_, de = de_, jz = jz_, je = je_, sd = std_, mz = mz_, me = me_;
    uint64_t tot = total_, db = datum_bis_, sb = schritt_bis_;
    a.num(vs); a.num(rs); a.num(fr); a.num(z); a.num(t); a.num(tot); a.num(db); a.num(sb);
    a.num(mo); a.num(dz); a.num(de); a.num(jz); a.num(je); a.num(sd); a.num(mz); a.num(me);
}

bool P8000RtcU130::deserialize(const uint8_t*& p, const uint8_t* end) {
    if (end - p < 1 || (*p != U130_SAVE && *p != 0x81)) return false;
    const bool alt = *p == 0x81;   // R2: Versatz zur Wirtszeit statt Uhrstand
    ++p;
    auto a = k1520::ZAr::leser(p, end);
    int64_t vs = 0, fr = 0;
    uint8_t z = 0, t = 0, mo = 1, dz = 0, de = 0, jz = 0, je = 0, sd = 0, mz = 0, me = 0;
    uint64_t tot = 0, db = 0, sb = 0, rs = 0;
    a.num(vs);
    if (!alt) a.num(rs);
    a.num(fr); a.num(z); a.num(t); a.num(tot); a.num(db); a.num(sb);
    a.num(mo); a.num(dz); a.num(de); a.num(jz); a.num(je); a.num(sd); a.num(mz); a.num(me);
    if (!a.ok || z >= uint8_t(Z::ANZAHL)) return false;
    p = a.p;
    if (alt) { sek_ = wirtWand() + vs; rest_ = 0; } else { sek_ = vs; rest_ = rs % cfg_.takt_hz; }
    frozen_ = fr; z_ = Z(z); tasten_ = t & 7; total_ = tot; datum_bis_ = db; schritt_bis_ = sb;
    mo_ = uint8_t(klemme(mo, 1, 12)); dz_ = dz % 4; de_ = de % 10; jz_ = jz % 10; je_ = je % 10;
    std_ = sd % 24; mz_ = mz % 6; me_ = me % 10;
    sichern();
    return true;
}
