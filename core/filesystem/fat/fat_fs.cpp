/**
 * @file fat_fs.cpp
 * @brief FAT12 (MS-DOS) — siehe fat_fs.h.
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include "core/filesystem/fat/fat_fs.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <set>

namespace fat {

// ═════════════════════════════════════════════════════════════════════════════
//  Zeichensaetze: UTF-8 <-> UTF-16 (lange Namen) und die OEM-Seite 437 (Kurznamen)
// ═════════════════════════════════════════════════════════════════════════════

namespace {

/// @brief Codepage 437 fuer 0x80..0xFF (die Kurznamen der meisten PC-DOS-Disketten).
const uint16_t kCp437[128] = {
    0x00C7,0x00FC,0x00E9,0x00E2,0x00E4,0x00E0,0x00E5,0x00E7,0x00EA,0x00EB,0x00E8,0x00EF,0x00EE,0x00EC,0x00C4,0x00C5,
    0x00C9,0x00E6,0x00C6,0x00F4,0x00F6,0x00F2,0x00FB,0x00F9,0x00FF,0x00D6,0x00DC,0x00A2,0x00A3,0x00A5,0x20A7,0x0192,
    0x00E1,0x00ED,0x00F3,0x00FA,0x00F1,0x00D1,0x00AA,0x00BA,0x00BF,0x2310,0x00AC,0x00BD,0x00BC,0x00A1,0x00AB,0x00BB,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,0x2555,0x2563,0x2551,0x2557,0x255D,0x255C,0x255B,0x2510,
    0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x255E,0x255F,0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256B,0x256A,0x2518,0x250C,0x2588,0x2584,0x258C,0x2590,0x2580,
    0x03B1,0x00DF,0x0393,0x03C0,0x03A3,0x03C3,0x00B5,0x03C4,0x03A6,0x0398,0x03A9,0x03B4,0x221E,0x03C6,0x03B5,0x2229,
    0x2261,0x00B1,0x2265,0x2264,0x2320,0x2321,0x00F7,0x2248,0x00B0,0x2219,0x00B7,0x221A,0x207F,0x00B2,0x25A0,0x00A0};

void utf8An(std::string& s, uint32_t cp) {
    if (cp < 0x80) s += static_cast<char>(cp);
    else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

/// @brief UTF-8 in Codepunkte; ungueltige Bytes werden als Latin-1 gelesen (Linux-Dateinamen sind Bytes).
std::vector<uint32_t> utf8Zerlegen(const std::string& s) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i < s.size();) {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        const int n = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
        bool ok = n >= 0 && i + static_cast<size_t>(n) < s.size() + (n == 0 ? 1 : 0);
        uint32_t cp = n <= 0 ? c : (c & (0x3Fu >> n));
        for (int k = 1; ok && k <= n; ++k) {
            if (i + static_cast<size_t>(k) >= s.size() || (static_cast<uint8_t>(s[i + static_cast<size_t>(k)]) & 0xC0) != 0x80) ok = false;
            else cp = (cp << 6) | (static_cast<uint8_t>(s[i + static_cast<size_t>(k)]) & 0x3F);
        }
        if (!ok) { out.push_back(c); ++i; continue; }
        out.push_back(cp);
        i += static_cast<size_t>(n) + 1;
    }
    return out;
}

std::string oemZuUtf8(const uint8_t* p, size_t n) {
    std::string s;
    for (size_t i = 0; i < n; ++i) utf8An(s, p[i] < 0x80 ? p[i] : kCp437[p[i] - 0x80]);
    return s;
}

/// @brief Codepunkt -> OEM-Byte; -1 = in der Seite 437 nicht vorhanden.
int cpZuOem(uint32_t cp) {
    if (cp < 0x80) return static_cast<int>(cp);
    for (int i = 0; i < 128; ++i) if (kCp437[i] == cp) return 0x80 + i;
    return -1;
}

/// @brief Zeichen, die in KEINEM DOS-Namen stehen duerfen.
bool verbotenesZeichen(uint32_t c) {
    if (c < 0x20 || c == 0x7F) return true;
    switch (c) {
        case '\\': case '/': case ':': case '*': case '?': case '"': case '<': case '>': case '|': return true;
        default: return false;
    }
}

/// @brief Im 8.3-Namen erlaubt?  (Grossbuchstaben, Ziffern, ! # $ % & ' ( ) - @ ^ _ ` { } ~ und >= 0x80)
bool kurznamenZeichen(int oem) {
    if (oem < 0) return false;
    if (oem >= 0x80) return true;
    if (std::isalnum(oem) && !std::islower(oem)) return true;
    return std::strchr("!#$%&'()-@^_`{}~", oem) != nullptr && oem != 0;
}

uint32_t grossbuchstabe(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 32;
    if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) return cp - 0x20;     // Latin-1: ae, oe, ue, e-akut …
    return cp;
}

bool hatKleinbuchstaben(const std::vector<uint32_t>& v) {
    for (uint32_t c : v) if (c < 0x80 ? std::islower(static_cast<int>(c)) != 0 : grossbuchstabe(c) != c) return true;
    return false;
}
bool hatGrossbuchstaben(const std::vector<uint32_t>& v) {
    for (uint32_t c : v) if (c < 0x80 && std::isupper(static_cast<int>(c))) return true;
    return false;
}

std::string grossschrift(const std::string& s) {
    std::string out;
    for (uint32_t c : utf8Zerlegen(s)) utf8An(out, grossbuchstabe(c));
    return out;
}
std::string kleinschreibASCII(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// @brief Namensvergleich ohne Beruecksichtigung der Gross-/Kleinschreibung (ASCII + Umlaute).
bool gleichName(const std::string& a, const std::string& b) {
    return grossschrift(a) == grossschrift(b);
}

} // namespace

int64_t utcZuOrtszeit(int64_t utc) {
    const std::time_t t = static_cast<std::time_t>(utc);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    // Die Uhr, wie sie zeigt, als UTC gelesen.
    std::tm u = lt;
    u.tm_isdst = 0;
#ifdef _WIN32
    return static_cast<int64_t>(_mkgmtime(&u));
#else
    return static_cast<int64_t>(timegm(&u));
#endif
}

int64_t ortszeitZuUtc(int64_t ort) {
    const std::time_t t = static_cast<std::time_t>(ort);
    std::tm g{};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    g.tm_isdst = -1;
    return static_cast<int64_t>(std::mktime(&g));
}

int64_t jetztOrtszeit() { return utcZuOrtszeit(static_cast<int64_t>(std::time(nullptr))); }

namespace {
int64_t tageSeit1970(int y, int m, int d) {
    // Hinnant: days_from_civil
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
void zivilAusTagen(int64_t z, int& y, int& m, int& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<int>(yoe + era * 400);
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y += m <= 2;
}
} // namespace

int64_t dosZuEpoche(uint16_t datum, uint16_t zeit) {
    if (datum == 0) return 0;
    const int y = 1980 + (datum >> 9);
    int m = (datum >> 5) & 15, d = datum & 31;
    if (m < 1) m = 1;
    if (m > 12) m = 12;
    if (d < 1) d = 1;
    const int hh = std::min<int>(zeit >> 11, 23), mm = std::min<int>((zeit >> 5) & 63, 59),
              ss = std::min<int>((zeit & 31) * 2, 59);
    return tageSeit1970(y, m, d) * 86400 + hh * 3600 + mm * 60 + ss;
}

void epocheZuDos(int64_t t, uint16_t& datum, uint16_t& zeit) {
    if (t < tageSeit1970(1980, 1, 1) * 86400) { datum = 0x0021; zeit = 0; return; }   // 1980-01-01
    int64_t tage = t / 86400;
    const int64_t rest = t - tage * 86400;
    int y, m, d;
    zivilAusTagen(tage, y, m, d);
    if (y > 2107) { y = 2107; m = 12; d = 31; }
    datum = static_cast<uint16_t>(((y - 1980) << 9) | (m << 5) | d);
    zeit  = static_cast<uint16_t>(((rest / 3600) << 11) | (((rest / 60) % 60) << 5) | ((rest % 60) / 2));
}

uint8_t kurznamenSumme(const uint8_t* n) {
    uint8_t s = 0;
    for (int i = 0; i < 11; ++i) s = static_cast<uint8_t>(((s & 1) ? 0x80 : 0) + (s >> 1) + n[i]);
    return s;
}

// ─── BPB ─────────────────────────────────────────────────────────────────────

bool Bpb::berechne(std::string* warum) {
    auto nein = [&](const std::string& w) { if (warum) *warum = w; return false; };
    if (bps < 128 || bps > 4096 || (bps & (bps - 1))) return nein("Sektorgroesse " + std::to_string(bps) + " ist keine Zweierpotenz 128..4096");
    if (spc == 0 || (spc & (spc - 1)) || spc > 128) return nein("Sektoren je Cluster " + std::to_string(spc) + " unzulaessig");
    if (rsv == 0) return nein("keine reservierten Sektoren (Bootsektor fehlt)");
    if (nfats == 0 || nfats > 4) return nein("FAT-Anzahl " + std::to_string(nfats) + " unzulaessig");
    if (rde == 0) return nein("Wurzelverzeichnis ohne Eintraege (FAT32 ist nicht unterstuetzt)");
    if (total == 0) return nein("Gesamtsektorzahl 0");
    if (media < 0xF0) return nein("Medienkennung 0x" + std::to_string(media) + " < F0");
    if (spf == 0) return nein("FAT ohne Sektoren (FAT32 ist nicht unterstuetzt)");
    fat_start  = rsv;
    root_start = rsv + static_cast<uint32_t>(nfats) * spf;
    root_secs  = (static_cast<uint32_t>(rde) * 32 + bps - 1) / bps;
    data_start = root_start + root_secs;
    if (data_start >= total) return nein("Verwaltungsbereich (" + std::to_string(data_start) + " Sektoren) ist groesser als die Diskette");
    clusters = (total - data_start) / spc;
    if (clusters == 0) return nein("kein einziger Datencluster");
    if (clusters >= 4085) return nein(std::to_string(clusters) + " Cluster - das ist FAT16, nicht FAT12 (dafuer gibt es keine Diskette)");
    if (static_cast<uint64_t>(spf) * bps * 2 / 3 < clusters + 2)
        return nein("die FAT (" + std::to_string(spf) + " Sektoren) ist fuer " + std::to_string(clusters) + " Cluster zu klein");
    return true;
}

bool standardBpb(uint64_t bytes, uint16_t heads, uint16_t spt, Bpb& o) {
    struct Z { uint64_t b; uint8_t media, spc; uint16_t rde, spf, spt, heads; };
    static const Z tab[] = {
        {163840,  0xFE, 1, 64,  1, 8,  1},
        {184320,  0xFC, 1, 64,  2, 9,  1},
        {327680,  0xFF, 2, 112, 1, 8,  2},
        {368640,  0xFD, 2, 112, 2, 9,  2},
        {655360,  0xFB, 2, 112, 2, 8,  2},
        {737280,  0xF9, 2, 112, 3, 9,  2},
        {1228800, 0xF9, 1, 224, 7, 15, 2},
        {1474560, 0xF0, 1, 224, 9, 18, 2},
        {2949120, 0xF0, 2, 240, 9, 36, 2},
    };
    for (const Z& z : tab) {
        if (z.b != bytes) continue;
        if (heads && heads != z.heads) continue;
        if (spt && spt != z.spt) continue;
        o = Bpb{};
        o.bps = 512; o.spc = z.spc; o.rsv = 1; o.nfats = 2; o.rde = z.rde;
        o.total = static_cast<uint32_t>(bytes / 512); o.media = z.media; o.spf = z.spf;
        o.spt = z.spt; o.heads = z.heads;
        return o.berechne(nullptr);
    }
    return false;
}

} // namespace fat

using namespace fat;

// ═════════════════════════════════════════════════════════════════════════════
//  Erkennung und Bootsektor
// ═════════════════════════════════════════════════════════════════════════════

namespace {

uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p) { return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24)); }
void wr16(uint8_t* p, uint32_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); }
void wr32(uint8_t* p, uint32_t v) { wr16(p, v & 0xFFFF); wr16(p + 2, v >> 16); }

/// @brief Mittlere Sektorgroesse/Koepfe/Sektoren der Geometrie (aus der ersten Spur).
bool geometrieVon(const SectorSpace& raum, uint16_t& bps, uint16_t& spt, uint16_t& heads, bool& einheitlich) {
    if (raum.trackCount() == 0) return false;
    const SectorSpace::TrackRef t0 = raum.trackAt(0);
    bps = t0.sector_size; spt = t0.sectors;
    einheitlich = true;
    uint8_t maxHead = 0;
    for (size_t i = 0; i < raum.trackCount(); ++i) {
        const SectorSpace::TrackRef t = raum.trackAt(i);
        if (t.sector_size != bps || t.sectors != spt) einheitlich = false;
        maxHead = std::max(maxHead, t.head);
    }
    heads = static_cast<uint16_t>(maxHead + 1);
    return true;
}

/// @brief Liest die Rohwerte des Bootsektors in @p b; true = die Felder des BPB sehen vernuenftig aus.
bool bpbAusSektor(const uint8_t* s, Bpb& b) {
    b = Bpb{};
    b.bps = rd16(s + 11); b.spc = s[13]; b.rsv = rd16(s + 14); b.nfats = s[16]; b.rde = rd16(s + 17);
    const uint16_t t16 = rd16(s + 19);
    b.media = s[21]; b.spf = rd16(s + 22); b.spt = rd16(s + 24); b.heads = rd16(s + 26);
    b.hidden = rd16(s + 28);                       // DOS 3.0: Wort; ab 3.31 Doppelwort (siehe unten)
    b.total = t16;
    if (t16 == 0) b.total = rd32(s + 32);
    // Doppelwort „versteckte Sektoren" (DOS 3.31+): nur uebernehmen, wenn das hohe Wort plausibel 0 ist.
    b.hidden = rd32(s + 28);
    if (b.hidden > b.total) b.hidden = rd16(s + 28);
    b.erweitert = (s[38] == 0x29 || s[38] == 0x28);
    for (int i = 3; i < 11; ++i) if (s[i] >= 0x20 && s[i] < 0x7F) b.oem += static_cast<char>(s[i]); else if (s[i] == 0) break;
    while (!b.oem.empty() && b.oem.back() == ' ') b.oem.pop_back();
    return true;
}

} // namespace

bool FatFileSystem::bpbFuerRaum(const SectorSpace& raum, Bpb& o, std::string* warum) {
    auto nein = [&](const std::string& w) { if (warum) *warum = w; return false; };
    uint16_t bps, spt, heads; bool ein;
    if (!geometrieVon(raum, bps, spt, heads, ein)) return nein("kein Spurraum");
    if (!ein) return nein("uneinheitliche Geometrie (FAT braucht gleiche Spuren)");
    if (standardBpb(raum.size(), heads, spt, o) && bps == 512) return true;
    // Keine Normgroesse: Werte rechnen.
    if (bps != 512 && bps != 256 && bps != 1024) return nein("Sektorgroesse " + std::to_string(bps) + " ist fuer FAT ungewoehnlich");
    o = Bpb{};
    o.bps = bps; o.rsv = 1; o.nfats = 2; o.spt = spt; o.heads = heads;
    o.total = static_cast<uint32_t>(raum.size() / bps);
    o.media = heads == 1 ? 0xF8 : 0xF9;
    // kleinster Cluster, bei dem alles in 12 Bit passt
    for (uint8_t spc = 1;; spc = static_cast<uint8_t>(spc * 2)) {
        o.spc = spc;
        o.rde = o.total * bps <= 400000 ? 112 : 224;
        const uint32_t rootSecs = (static_cast<uint32_t>(o.rde) * 32 + bps - 1) / bps;
        uint32_t spf = 1;
        for (int i = 0; i < 8; ++i) {
            const uint32_t daten = o.total - o.rsv - 2 * spf - rootSecs;
            const uint32_t cl = daten / spc;
            spf = std::max<uint32_t>(1, ((cl + 2) * 3 / 2 + bps - 1) / bps);
        }
        o.spf = static_cast<uint16_t>(spf);
        const uint32_t daten = o.total - o.rsv - 2 * spf - rootSecs;
        if (daten / spc < 4085) break;
        if (spc >= 64) return nein("Diskette zu gross fuer FAT12");
    }
    return o.berechne(warum);
}

bool FatFileSystem::looksLikeFat(const SectorSpace& raum, std::string* warum) {
    auto nein = [&](const std::string& w) { if (warum) *warum = w; return false; };
    if (raum.size() < 4096) return nein("Raum zu klein fuer FAT");
    uint8_t s[512];
    if (!raum.read(0, s, sizeof s)) return nein("Bootsektor nicht lesbar");
    Bpb b;
    bpbAusSektor(s, b);
    const bool sprung = s[0] == 0xE9 || (s[0] == 0xEB && (s[2] == 0x90 || s[2] == 0x00)) || s[0] == 0x69;

    std::string w;
    if (b.berechne(&w)) {
        if (static_cast<uint64_t>(b.total) * b.bps > raum.size())
            return nein("BPB nennt " + std::to_string(b.total) + " Sektoren, die Diskette fasst nur "
                        + std::to_string(raum.size() / b.bps));
        // Dieselbe Bytezahl tragen mehrere Geometrien (360K = 40x2x9 ODER 80x1x9): die Kopfzahl des BPB
        // entscheidet.  Nur bei uebereinstimmender Sektorzahl je Spur - sonst waere der BPB ohnehin ungenau.
        {
            uint16_t gb, gs, gh; bool ge;
            if (geometrieVon(raum, gb, gs, gh, ge) && ge && b.spt == gs && (b.heads == 1 || b.heads == 2) && b.heads != gh)
                return nein("der BPB nennt " + std::to_string(b.heads) + " Koepfe, die Geometrie hat " + std::to_string(gh));
        }
        // Mindestens ein weiteres Indiz: Sprung + Kennung 55AA, oder die FAT beginnt mit der Medienkennung.
        uint8_t f[3] = {};
        raum.read(static_cast<uint64_t>(b.fat_start) * b.bps, f, 3);
        const bool fatOk = f[0] == b.media && f[1] == 0xFF && f[2] == 0xFF;
        if (sprung || fatOk) return true;
        return nein("BPB plausibel, aber weder Sprungbefehl noch FAT-Kopf (Medienkennung "
                    + std::to_string(b.media) + ") bestaetigen ihn");
    }
    // BPB-lose Diskette (DOS 1.x/2.0): Geometrie und Medienkennung der FAT
    uint16_t bps, spt, heads; bool ein;
    if (!geometrieVon(raum, bps, spt, heads, ein) || !ein) return nein(w);
    uint8_t f[3] = {};
    if (!raum.read(bps, f, 3)) return nein("FAT nicht lesbar");
    if (f[0] < 0xF8 || f[1] != 0xFF || f[2] != 0xFF) return nein("kein BPB (" + w + ") und kein FAT-Kopf hinter dem Bootsektor");
    Bpb std;
    if (!standardBpb(raum.size(), heads, spt, std) || std.media != f[0])
        return nein("kein BPB (" + w + "); Medienkennung 0x" + std::to_string(f[0]) + " passt zu keiner Normgroesse");
    return true;
}

int FatFileSystem::programmAnfang(const std::vector<uint8_t>& s) {
    if (s.size() < 64) return 0;
    if (s[0] == 0xEB) return 2 + static_cast<int8_t>(s[1]);
    if (s[0] == 0xE9) return 3 + static_cast<int16_t>(rd16(&s[1]));
    return 0;
}

// ─── Konstruktion / mount / format ───────────────────────────────────────────

FatFileSystem::FatFileSystem(SectorSpace& raum, const Bpb& b) : raum_(raum), bpb_(b) {}

std::unique_ptr<FatFileSystem> FatFileSystem::mount(SectorSpace& raum, const FsProfile&, std::string& warum) {
    uint8_t s[512];
    if (!raum.read(0, s, sizeof s)) { warum = "Bootsektor nicht lesbar"; return nullptr; }
    Bpb b;
    bpbAusSektor(s, b);
    std::string w;
    if (!b.berechne(&w)) {
        uint16_t bps, spt, heads; bool ein;
        if (!geometrieVon(raum, bps, spt, heads, ein) || !ein
            || !standardBpb(raum.size(), heads, spt, b)) { warum = w; return nullptr; }
        uint8_t f[3] = {};
        raum.read(bps, f, 3);
        if (f[0] != b.media) { warum = "kein BPB und keine passende Medienkennung (" + w + ")"; return nullptr; }
        b.mit_bpb = false;
        b.oem.clear();
    }
    if (static_cast<uint64_t>(b.total) * b.bps > raum.size()) {
        warum = "BPB nennt " + std::to_string(b.total) + " Sektoren, die Diskette fasst nur "
              + std::to_string(raum.size() / b.bps);
        return nullptr;
    }
    std::unique_ptr<FatFileSystem> f(new FatFileSystem(raum, b));
    if (!f->ladeFat()) { warum = "FAT nicht lesbar: " + f->lastError(); return nullptr; }
    return f;
}

std::vector<uint8_t> FatFileSystem::leererBootsektor(const Bpb& b, const std::string& label,
                                                     const FatFormatOptions& opt) {
    std::vector<uint8_t> s(b.bps < 512 ? 512 : b.bps, 0);
    s[0] = 0xEB; s[1] = 0x3C; s[2] = 0x90;
    std::string oem = opt.oem; oem.resize(8, ' ');
    std::memcpy(&s[3], oem.data(), 8);
    wr16(&s[11], b.bps); s[13] = b.spc; wr16(&s[14], b.rsv); s[16] = b.nfats; wr16(&s[17], b.rde);
    if (b.total < 65536) wr16(&s[19], b.total);
    s[21] = b.media; wr16(&s[22], b.spf); wr16(&s[24], b.spt); wr16(&s[26], b.heads);
    wr32(&s[28], b.hidden);
    if (b.total >= 65536) wr32(&s[32], b.total);
    s[36] = 0x00;           // Laufwerksnummer (Diskette)
    s[38] = 0x29;
    uint32_t nr = opt.serial ? opt.serial : static_cast<uint32_t>(std::time(nullptr)) * 2654435761u;
    wr32(&s[39], nr);
    std::string l = grossschrift(label);
    if (l.empty()) l = "NO NAME";
    l.resize(11, ' ');
    std::memcpy(&s[43], l.data(), 11);
    std::memcpy(&s[54], "FAT12   ", 8);
    // Programm bei 0x3E: Meldung ausgeben, Taste abwarten, Neustart (INT 19h).
    static const uint8_t code[] = {
        0x0E, 0x1F,                    // push cs ; pop ds
        0xBE, 0x00, 0x00,              // mov si, msg        (Adresse wird unten eingesetzt)
        0xAC,                          // lodsb
        0x08, 0xC0,                    // or al, al
        0x74, 0x09,                    // jz fertig
        0xB4, 0x0E, 0xBB, 0x07, 0x00,  // mov ah,0Eh ; mov bx,0007h
        0xCD, 0x10,                    // int 10h
        0xEB, 0xF2,                    // jmp lodsb
        0x31, 0xC0, 0xCD, 0x16,        // xor ax,ax ; int 16h (Taste)
        0xCD, 0x19};                   // int 19h (Neustart)
    static const char msg[] = "\r\nNon-System disk or disk error\r\nReplace and press any key when ready\r\n";
    std::memcpy(&s[0x3E], code, sizeof code);
    const size_t msgOff = 0x3E + sizeof code;
    wr16(&s[0x3E + 3], 0x7C00 + static_cast<uint32_t>(msgOff));
    std::memcpy(&s[msgOff], msg, sizeof msg);   // einschliesslich der 0 am Ende
    s[510] = 0x55; s[511] = 0xAA;
    return s;
}

std::unique_ptr<FatFileSystem> FatFileSystem::format(SectorSpace& raum, const FsProfile&, const std::string& label,
                                                    std::string& err, const FatFormatOptions& opt) {
    Bpb b;
    if (!bpbFuerRaum(raum, b, &err)) { err = "FAT12: " + err; return nullptr; }
    if (label.size() > 11) { err = "Datentraegername hoechstens 11 Zeichen"; return nullptr; }
    std::unique_ptr<FatFileSystem> f(new FatFileSystem(raum, b));
    f->bpb_.oem = opt.oem;
    f->bpb_.erweitert = true;
    const std::vector<uint8_t> boot = leererBootsektor(b, label, opt);
    if (!raum.write(0, boot.data(), std::min<size_t>(boot.size(), b.bps))) { err = "Bootsektor nicht schreibbar: " + raum.lastError(); return nullptr; }
    if (!f->mkfs()) { err = f->lastError(); return nullptr; }
    if (!label.empty() && !f->setLabel(label)) { err = f->lastError(); return nullptr; }
    return f;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Rohzugriff, FAT
// ═════════════════════════════════════════════════════════════════════════════

uint64_t FatFileSystem::clusterOffset(uint16_t c) const {
    return sektorOffset(bpb_.data_start) + static_cast<uint64_t>(c - 2) * bpb_.clusterBytes();
}
bool FatFileSystem::lesen(uint64_t off, uint8_t* dst, size_t n) const {
    if (!raum_.read(off, dst, n)) return fail("Lesefehler bei Byte " + std::to_string(off) + ": " + raum_.lastError());
    return true;
}
bool FatFileSystem::schreiben(uint64_t off, const uint8_t* src, size_t n) {
    if (!raum_.write(off, src, n)) return fail("Schreibfehler bei Byte " + std::to_string(off) + ": " + raum_.lastError());
    return true;
}

bool FatFileSystem::ladeFat() {
    const size_t n = static_cast<size_t>(bpb_.spf) * bpb_.bps;
    fat_roh_.assign(n, 0);
    if (!lesen(sektorOffset(bpb_.fat_start), fat_roh_.data(), n)) return false;
    fat_.assign(bpb_.clusters + 2, 0);
    for (uint32_t i = 0; i < fat_.size(); ++i) {
        const size_t o = static_cast<size_t>(i) * 3 / 2;
        const uint16_t w = static_cast<uint16_t>(fat_roh_[o] | (fat_roh_[o + 1] << 8));
        fat_[i] = (i & 1) ? static_cast<uint16_t>(w >> 4) : static_cast<uint16_t>(w & 0xFFF);
    }
    return true;
}

bool FatFileSystem::schreibeFat() {
    std::vector<uint8_t> roh = fat_roh_;
    for (uint32_t i = 0; i < fat_.size(); ++i) {
        const size_t o = static_cast<size_t>(i) * 3 / 2;
        const uint16_t v = fat_[i] & 0xFFF;
        if (i & 1) { roh[o] = static_cast<uint8_t>((roh[o] & 0x0F) | ((v & 0x0F) << 4)); roh[o + 1] = static_cast<uint8_t>(v >> 4); }
        else       { roh[o] = static_cast<uint8_t>(v & 0xFF); roh[o + 1] = static_cast<uint8_t>((roh[o + 1] & 0xF0) | (v >> 8)); }
    }
    if (roh == fat_roh_) {
        // Nichts geaendert in FAT 1 — die Kopien koennten trotzdem abweichen (Reparatur): dann angleichen.
        std::vector<uint8_t> k(roh.size());
        bool gleich = true;
        for (int c = 1; c < bpb_.nfats && gleich; ++c) {
            if (!lesen(sektorOffset(bpb_.fat_start + static_cast<uint32_t>(c) * bpb_.spf), k.data(), k.size())) return false;
            if (k != roh) gleich = false;
        }
        if (gleich) return true;
    }
    for (int c = 0; c < bpb_.nfats; ++c)
        if (!schreiben(sektorOffset(bpb_.fat_start + static_cast<uint32_t>(c) * bpb_.spf), roh.data(), roh.size())) return false;
    fat_roh_ = std::move(roh);
    return true;
}

std::vector<uint16_t> FatFileSystem::kette(uint16_t start, bool* heil, std::string* grund) const {
    std::vector<uint16_t> k;
    if (heil) *heil = true;
    if (start == 0) return k;
    std::vector<uint8_t> gesehen(fat_.size(), 0);
    uint32_t c = start;
    while (true) {
        if (!gueltigerCluster(c)) {
            if (heil) *heil = false;
            if (grund) *grund = "Zeiger auf Cluster " + std::to_string(c) + " ausserhalb des Datenbereichs";
            return k;
        }
        if (gesehen[c]) { if (heil) *heil = false; if (grund) *grund = "Kette schliesst sich zur Schleife (Cluster " + std::to_string(c) + ")"; return k; }
        gesehen[c] = 1;
        k.push_back(static_cast<uint16_t>(c));
        const uint16_t n = fat_[c];
        if (istEnde(n)) return k;
        if (n == 0 || istDefekt(n) || (n >= 0xFF0)) {
            if (heil) *heil = false;
            if (grund) *grund = n == 0 ? "Kette endet in einem freien Cluster (nach " + std::to_string(c) + ")"
                                       : "Kette fuehrt in einen defekten/reservierten Cluster (nach " + std::to_string(c) + ")";
            return k;
        }
        c = n;
    }
}

bool FatFileSystem::findeFrei(uint32_t n, std::vector<uint16_t>& out) const {
    out.clear();
    for (uint32_t c = 2; c < bpb_.clusters + 2 && out.size() < n; ++c)
        if (fat_[c] == 0) out.push_back(static_cast<uint16_t>(c));
    return out.size() == n;
}

uint32_t FatFileSystem::freiCluster() const {
    uint32_t n = 0;
    for (uint32_t c = 2; c < bpb_.clusters + 2; ++c) if (fat_[c] == 0) ++n;
    return n;
}

void FatFileSystem::verketten(const std::vector<uint16_t>& c) {
    for (size_t i = 0; i < c.size(); ++i) fat_[c[i]] = i + 1 < c.size() ? c[i + 1] : 0xFFF;
}

void FatFileSystem::freigeben(uint16_t start) {
    for (uint16_t c : kette(start)) fat_[c] = 0;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Verzeichnisse
// ═════════════════════════════════════════════════════════════════════════════

bool FatFileSystem::ladeVerz(uint16_t cluster, Verz& v) const {
    v = Verz{};
    v.cluster = cluster;
    if (cluster == 0) {
        v.bytes.assign(static_cast<size_t>(bpb_.rde) * 32, 0);
        if (!lesen(sektorOffset(bpb_.root_start), v.bytes.data(), v.bytes.size())) return false;
    } else {
        bool heil = true; std::string grund;
        v.kette = kette(cluster, &heil, &grund);
        v.defekt = !heil;
        if (v.kette.empty()) return fail("Verzeichnis ohne gueltigen Cluster (" + grund + ")");
        const uint32_t cb = bpb_.clusterBytes();
        v.bytes.assign(v.kette.size() * cb, 0);
        for (size_t i = 0; i < v.kette.size(); ++i)
            if (!lesen(clusterOffset(v.kette[i]), &v.bytes[i * cb], cb)) return false;
    }
    zerlege(v);
    return true;
}

void FatFileSystem::zerlege(Verz& v) {
    v.eintraege.clear();
    struct Teil { uint8_t seq; uint8_t summe; std::u16string z; size_t index; };
    std::vector<Teil> teile;
    for (size_t i = 0; i + 32 <= v.bytes.size(); ++i) {
        const uint8_t* e = &v.bytes[i * 32];
        if (i * 32 + 32 > v.bytes.size()) break;
        if (e[0] == 0x00) break;               // Ende des Verzeichnisses
        if (e[0] == 0xE5) { teile.clear(); continue; }
        if ((e[11] & 0x3F) == kAttrLfn) {
            Teil t; t.seq = e[0]; t.summe = e[13]; t.index = i;
            auto hole = [&](int von, int n) { for (int k = 0; k < n; ++k) { const char16_t c = static_cast<char16_t>(e[von + 2 * k] | (e[von + 2 * k + 1] << 8)); t.z += c; } };
            hole(1, 5); hole(14, 6); hole(28, 2);
            teile.push_back(std::move(t));
            continue;
        }
        Eintrag en;
        std::memcpy(en.name, e, 11);
        en.attr = e[11]; en.nt = e[12];
        en.zeit = rd16(e + 22); en.datum = rd16(e + 24);
        en.cluster = rd16(e + 26); en.groesse = rd32(e + 28);
        en.slot = i; en.erster = i;
        if (!teile.empty() && !(en.attr & kAttrVol)) {
            // Folge pruefen: absteigende Nummern, letzte mit 0x40, gleiche Pruefsumme.
            bool ok = (teile.front().seq & 0x40) != 0;
            const uint8_t summe = kurznamenSumme(en.name);
            const size_t n = teile.size();
            for (size_t k = 0; k < n && ok; ++k) {
                const int erwartet = static_cast<int>(n - k);
                if ((teile[k].seq & 0x3F) != erwartet || teile[k].summe != summe) ok = false;
            }
            if (ok) {
                std::u16string u;
                for (size_t k = n; k-- > 0;) u += teile[k].z;
                const size_t z0 = u.find(u'\0');
                if (z0 != std::u16string::npos) u.resize(z0);
                for (size_t k = 0; k < u.size(); ++k) {
                    uint32_t cp = u[k];
                    if (cp >= 0xD800 && cp < 0xDC00 && k + 1 < u.size() && u[k + 1] >= 0xDC00 && u[k + 1] < 0xE000) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (u[k + 1] - 0xDC00); ++k;
                    }
                    utf8An(en.lang, cp);
                }
                en.erster = teile.front().index;
            } else en.lfn_gueltig = false;
        }
        teile.clear();
        v.eintraege.push_back(std::move(en));
    }
}

bool FatFileSystem::speichereVerz(Verz& v, const std::vector<uint8_t>& alt) {
    // Nur Stuecke schreiben, die sich geaendert haben: jeder Sektor ist ein Zurueckschreiben auf das Medium.
    auto offsetVon = [&](size_t b) -> uint64_t {
        if (v.cluster == 0) return sektorOffset(bpb_.root_start) + b;
        const uint32_t cb = bpb_.clusterBytes();
        return clusterOffset(v.kette[b / cb]) + (b % cb);
    };
    const size_t schritt = bpb_.bps;
    for (size_t b = 0; b < v.bytes.size(); b += schritt) {
        const size_t n = std::min(schritt, v.bytes.size() - b);
        if (b + n <= alt.size() && std::memcmp(&v.bytes[b], &alt[b], n) == 0) continue;
        if (!schreiben(offsetVon(b), &v.bytes[b], n)) return false;
    }
    zerlege(v);
    return true;
}

std::vector<std::string> FatFileSystem::pfadTeile(const std::string& pfad) {
    std::vector<std::string> t;
    std::string cur;
    for (char c : pfad) {
        if (c == '/' || c == '\\') { if (!cur.empty()) t.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) t.push_back(cur);
    return t;
}

/// @brief Der 8.3-Kurzname eines Eintrags als UTF-8 ("README.TXT"), ohne Kleinschreibbits.
static std::string kurznameText(const uint8_t* n11) {
    uint8_t n[11];
    std::memcpy(n, n11, 11);
    if (n[0] == 0x05) n[0] = 0xE5;
    std::string basis = oemZuUtf8(n, 8), erw = oemZuUtf8(n + 8, 3);
    while (!basis.empty() && basis.back() == ' ') basis.pop_back();
    while (!erw.empty() && erw.back() == ' ') erw.pop_back();
    return basis + (erw.empty() ? "" : "." + erw);
}

int FatFileSystem::findeName(const Verz& v, const std::string& name) const {
    for (size_t i = 0; i < v.eintraege.size(); ++i) {
        const Eintrag& e = v.eintraege[i];
        if (e.attr & kAttrVol) continue;
        if (e.name[0] == '.' && (e.name[1] == ' ' || (e.name[1] == '.' && e.name[2] == ' '))) continue;
        if (!e.lang.empty() && gleichName(e.lang, name)) return static_cast<int>(i);
        if (gleichName(kurznameText(e.name), name)) return static_cast<int>(i);
    }
    return -1;
}

bool FatFileSystem::findeVerz(const std::vector<std::string>& teile, size_t n, Verz& v, std::string& fehler) const {
    if (!ladeVerz(0, v)) { fehler = lastError(); return false; }
    for (size_t i = 0; i < n; ++i) {
        const int k = findeName(v, teile[i]);
        if (k < 0) { fehler = "Verzeichnis '" + teile[i] + "' gibt es nicht"; return false; }
        const Eintrag& e = v.eintraege[static_cast<size_t>(k)];
        if (!e.istDir()) { fehler = "'" + teile[i] + "' ist eine Datei, kein Verzeichnis"; return false; }
        const uint16_t c = e.cluster;
        if (!gueltigerCluster(c)) { fehler = "Verzeichnis '" + teile[i] + "' hat keinen gueltigen Anfangscluster"; return false; }
        if (!ladeVerz(c, v)) { fehler = lastError(); return false; }
    }
    return true;
}

bool FatFileSystem::sucheEintrag(const std::string& pfad, Verz& eltern, int& index, std::string& blatt,
                                 bool& elternGefunden) const {
    const std::vector<std::string> t = pfadTeile(pfad);
    elternGefunden = false; index = -1;
    if (t.empty()) return false;
    blatt = t.back();
    std::string f;
    if (!findeVerz(t, t.size() - 1, eltern, f)) { fail(f); return false; }
    elternGefunden = true;
    index = findeName(eltern, blatt);
    return index >= 0;
}

FileEntry FatFileSystem::zuFileEntry(const Eintrag& e, const std::string& pfad) const {
    FileEntry fe;
    uint8_t n[11];
    std::memcpy(n, e.name, 11);
    if (n[0] == 0x05) n[0] = 0xE5;            // 0x05 steht fuer ein echtes 0xE5
    std::string basis = oemZuUtf8(n, 8), erw = oemZuUtf8(n + 8, 3);
    while (!basis.empty() && basis.back() == ' ') basis.pop_back();
    while (!erw.empty() && erw.back() == ' ') erw.pop_back();
    if (e.nt & 0x08) basis = kleinschreibASCII(basis);
    if (e.nt & 0x10) erw = kleinschreibASCII(erw);
    std::string kurz = basis + (erw.empty() ? "" : "." + erw);
    fe.name = pfad + (e.lang.empty() ? kurz : e.lang);
    if (!e.lang.empty()) fe.short_name = pfad + kurz;
    fe.size = e.istDir() ? 0 : e.groesse;
    fe.type = e.istDir() ? "d" : "";
    std::string a;
    auto add = [&](const char* s) { if (!a.empty()) a += ' '; a += s; };
    if (e.attr & kAttrRo) add("RO");
    if (e.attr & kAttrHid) add("HID");
    if (e.attr & kAttrSys) add("SYS");
    if (e.attr & kAttrArc) add("ARC");
    fe.attributes = a;
    fe.hidden = (e.attr & (kAttrHid | kAttrSys)) != 0;
    fe.dos_mtime = dosZuEpoche(e.datum, e.zeit);
    if (e.datum) {
        const int y = 1980 + (e.datum >> 9), m = (e.datum >> 5) & 15, d = e.datum & 31;
        char buf[32];
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d", y, m, d, e.zeit >> 11, (e.zeit >> 5) & 63, (e.zeit & 31) * 2);
        fe.date = buf;
    }
    return fe;
}

void FatFileSystem::listeRekursiv(uint16_t cluster, const std::string& prefix, std::vector<FileEntry>& out,
                                  std::vector<uint16_t>& besucht, int tiefe) const {
    Verz v;
    if (!ladeVerz(cluster, v)) return;
    for (const Eintrag& e : v.eintraege) {
        if (e.attr & kAttrVol) continue;
        if (e.name[0] == '.' && (e.name[1] == ' ' || (e.name[1] == '.' && e.name[2] == ' '))) continue;
        FileEntry fe = zuFileEntry(e, prefix);
        if (!e.istDir()) {
            bool heil = true;
            const auto k = kette(e.cluster, &heil);
            const uint64_t kap = static_cast<uint64_t>(k.size()) * bpb_.clusterBytes();
            fe.damaged = !heil || kap < e.groesse;
        }
        const bool dir = e.istDir();
        const uint16_t c = e.cluster;
        out.push_back(fe);
        if (dir && gueltigerCluster(c) && tiefe < 32
            && std::find(besucht.begin(), besucht.end(), c) == besucht.end()) {
            besucht.push_back(c);
            listeRekursiv(c, fe.name + "/", out, besucht, tiefe + 1);
        } else if (dir) out.back().damaged = true;
    }
}

std::vector<FileEntry> FatFileSystem::list() const {
    std::vector<FileEntry> out;
    std::vector<uint16_t> besucht;
    listeRekursiv(0, "", out, besucht, 0);
    return out;
}

std::string FatFileSystem::label() const {
    Verz v;
    if (ladeVerz(0, v)) {
        for (const Eintrag& e : v.eintraege)
            if ((e.attr & kAttrVol) && (e.attr & kAttrLfn) != kAttrLfn) {
                std::string s = oemZuUtf8(e.name, 11);
                while (!s.empty() && s.back() == ' ') s.pop_back();
                return s;
            }
    }
    uint8_t b[512];
    if (bpb_.erweitert && raum_.read(0, b, sizeof b)) {
        std::string s(reinterpret_cast<char*>(b + 43), 11);
        while (!s.empty() && (s.back() == ' ' || s.back() == 0)) s.pop_back();
        if (s != "NO NAME") return s;
    }
    return {};
}

// ═════════════════════════════════════════════════════════════════════════════
//  Namen
// ═════════════════════════════════════════════════════════════════════════════

bool FatFileSystem::pruefeName(const std::string& teil, std::string& err) const {
    if (teil.empty() || teil == "." || teil == "..") { err = "ungueltiger Name '" + teil + "'"; return false; }
    const std::vector<uint32_t> cps = utf8Zerlegen(teil);
    if (cps.size() > 255) { err = "Name laenger als 255 Zeichen"; return false; }
    for (uint32_t c : cps)
        if (verbotenesZeichen(c)) { err = "Zeichen '" + std::string(1, c < 0x80 ? static_cast<char>(c) : '?') + "' ist in DOS-Namen nicht erlaubt: " + teil; return false; }
    if (teil.back() == ' ' || teil.back() == '.') { err = "Name endet auf Leerzeichen oder Punkt: " + teil; return false; }
    // Geraetenamen
    std::string b = grossschrift(teil.substr(0, teil.find('.')));
    for (const char* g : {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "LPT1", "LPT2", "LPT3", "CLOCK$"})
        if (b == g) { err = "'" + teil + "' ist ein DOS-Geraetename und als Dateiname nicht brauchbar"; return false; }
    return true;
}

bool FatFileSystem::langeNamenUeblich() const {
    // Disketten, die ein Windows ab 95, Linux (mkfs.fat) oder ein anderes VFAT-System angelegt hat.
    const std::string& o = bpb_.oem;
    if (o.rfind("MSWIN", 0) == 0 || o.rfind("mkfs.fat", 0) == 0 || o.rfind("mkdosfs", 0) == 0
        || o.rfind("MSDOS5", 0) == 0 || o.rfind("IBM  7", 0) == 0 || o.rfind("IBM  6", 0) == 0
        || o.rfind("IBM  5", 0) == 0 || o.rfind("K1520", 0) == 0 || o.rfind("MSDOS", 0) == 0) {
        // MSDOS5.0 steht auch auf DOS-5/6-Disketten; dort schadet ein Eintrag fuer lange Namen nicht,
        // aber er ist unnoetig.  Deshalb: nur wenn die Wurzel schon einen traegt, ODER Win/Linux.
        if (o.rfind("MSWIN", 0) == 0 || o.rfind("mkfs.fat", 0) == 0 || o.rfind("mkdosfs", 0) == 0) return true;
    }
    Verz v;
    if (ladeVerz(0, v))
        for (const Eintrag& e : v.eintraege) if (!e.lang.empty()) return true;
    return false;
}

/// @brief 8.3-Kurznamen aus @p name bilden (Windows-95-Verfahren, siehe fat_fs.h).
bool FatFileSystem::legeNameAn(const Verz& v, const std::string& name, bool lfn, uint8_t kurz[11], uint8_t& nt,
                               bool& brauchtLfn, std::string& err) const {
    if (!pruefeName(name, err)) return false;
    nt = 0;
    brauchtLfn = false;
    const std::vector<uint32_t> cps = utf8Zerlegen(name);
    // Name in Basis und Erweiterung teilen (letzter Punkt)
    size_t punkt = std::string::npos;
    for (size_t i = 0; i < cps.size(); ++i) if (cps[i] == '.') punkt = i;
    std::vector<uint32_t> basis(cps.begin(), punkt == std::string::npos || punkt == 0 ? cps.end() : cps.begin() + static_cast<long>(punkt));
    std::vector<uint32_t> erw;
    if (punkt != std::string::npos && punkt != 0) erw.assign(cps.begin() + static_cast<long>(punkt) + 1, cps.end());
    // fuehrende Punkte weg, Leerzeichen weg (verlustbehaftet)
    bool verlust = false;
    auto reinigen = [&](std::vector<uint32_t>& t, bool basisTeil) {
        std::vector<uint32_t> o;
        for (uint32_t c : t) {
            if (c == ' ') { verlust = true; continue; }
            if (c == '.') { verlust = true; continue; }
            uint32_t g = grossbuchstabe(c);
            int oem = cpZuOem(g);
            if (!kurznamenZeichen(oem)) { verlust = true; o.push_back('_'); continue; }
            if (g != c && c >= 0x80) verlust = true;
            o.push_back(g);
        }
        (void)basisTeil;
        t = std::move(o);
    };
    std::vector<uint32_t> basisOrig = basis, erwOrig = erw;
    reinigen(basis, true); reinigen(erw, false);
    if (basis.size() > 8) { basis.resize(8); verlust = true; }
    if (erw.size() > 3) { erw.resize(3); verlust = true; }
    if (basis.empty()) { basis.push_back('_'); verlust = true; }
    // Gross-/Kleinschreibung: rein klein (je Teil) laesst sich ueber die NT-Bits ohne langen Namen halten
    const bool bKlein = hatKleinbuchstaben(basisOrig), bGross = hatGrossbuchstaben(basisOrig);
    const bool eKlein = hatKleinbuchstaben(erwOrig), eGross = hatGrossbuchstaben(erwOrig);
    bool gemischt = (bKlein && bGross) || (eKlein && eGross);
    if (!verlust && !gemischt) {
        if (bKlein) nt |= 0x08;
        if (eKlein) nt |= 0x10;
    } else if (!verlust && gemischt) {
        brauchtLfn = true;
    } else brauchtLfn = true;

    std::memset(kurz, ' ', 11);
    auto setze = [&](uint8_t* dst, const std::vector<uint32_t>& t) { for (size_t i = 0; i < t.size(); ++i) dst[i] = static_cast<uint8_t>(cpZuOem(t[i])); };
    setze(kurz, basis); setze(kurz + 8, erw);
    if (kurz[0] == 0xE5) kurz[0] = 0x05;

    if (verlust) {
        // BASIS~N.ERW — N so waehlen, dass der Kurzname im Verzeichnis neu ist.
        std::vector<uint32_t> b6 = basis;
        for (int n = 1; n < 1000000; ++n) {
            const std::string num = "~" + std::to_string(n);
            std::vector<uint32_t> bb = b6;
            if (bb.size() + num.size() > 8) bb.resize(8 - num.size());
            for (char c : num) bb.push_back(static_cast<uint32_t>(c));
            uint8_t k2[11];
            std::memset(k2, ' ', 11);
            setze(k2, bb); setze(k2 + 8, erw);
            if (k2[0] == 0xE5) k2[0] = 0x05;
            bool frei = true;
            for (const Eintrag& e : v.eintraege) if (std::memcmp(e.name, k2, 11) == 0) { frei = false; break; }
            if (frei) { std::memcpy(kurz, k2, 11); break; }
        }
    }
    if (brauchtLfn && !lfn) {
        // Ohne lange Namen bleibt nur der Kurzname; ist er neu, ist es gut, sonst die ~N-Fassung.
        brauchtLfn = false;
        for (const Eintrag& e : v.eintraege)
            if (std::memcmp(e.name, kurz, 11) == 0) {
                // gleicher Kurzname schon da (anderer Datei): die ~N-Fassung benutzen
                std::vector<uint32_t> bb = basis;
                for (int n = 1; n < 1000; ++n) {
                    const std::string num = "~" + std::to_string(n);
                    std::vector<uint32_t> b2 = basis;
                    if (b2.size() + num.size() > 8) b2.resize(8 - num.size());
                    for (char c : num) b2.push_back(static_cast<uint32_t>(c));
                    uint8_t k2[11]; std::memset(k2, ' ', 11); setze(k2, b2); setze(k2 + 8, erw);
                    bool frei = true;
                    for (const Eintrag& f : v.eintraege) if (std::memcmp(f.name, k2, 11) == 0) { frei = false; break; }
                    if (frei) { std::memcpy(kurz, k2, 11); break; }
                }
                break;
            }
        nt = 0;
    }
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Eintraege schreiben
// ═════════════════════════════════════════════════════════════════════════════

bool FatFileSystem::slotsBeschaffen(Verz& v, size_t anzahl, size_t& erster) {
    const size_t gesamt = v.bytes.size() / 32;
    // Ende des benutzten Teils suchen (erstes 0x00 ODER letztes belegtes + 1)
    auto frei = [&](size_t i) { return v.bytes[i * 32] == 0x00 || v.bytes[i * 32] == 0xE5; };
    size_t lauf = 0;
    for (size_t i = 0; i < gesamt; ++i) {
        if (frei(i)) {
            if (++lauf == anzahl) { erster = i + 1 - anzahl; return true; }
        } else lauf = 0;
    }
    if (v.cluster == 0) return fail("Das Wurzelverzeichnis ist voll (" + std::to_string(bpb_.rde) + " Eintraege) - es braucht "
                                     + std::to_string(anzahl) + " freie Plaetze");
    // Unterverzeichnis: einen Cluster anhaengen
    std::vector<uint16_t> neu;
    if (!findeFrei(1, neu)) return fail("Kein freier Cluster fuer die Erweiterung des Verzeichnisses");
    const uint16_t letzter = v.kette.back();
    fat_[letzter] = neu[0];
    fat_[neu[0]] = 0xFFF;
    v.kette.push_back(neu[0]);
    const uint32_t cb = bpb_.clusterBytes();
    // Der neue Cluster wird sofort mit Nullen angelegt (die FAT folgt beim Abschluss des Aufrufers)
    std::vector<uint8_t> nullen(cb, 0);
    if (!schreiben(clusterOffset(neu[0]), nullen.data(), nullen.size())) return false;
    v.bytes.insert(v.bytes.end(), cb, 0);
    return slotsBeschaffen(v, anzahl, erster);
}

bool FatFileSystem::trageEin(Verz& v, const std::string& lang, const uint8_t kurz[11], uint8_t nt, uint8_t attr,
                             uint16_t cluster, uint32_t groesse, int64_t mtime, bool lfn) {
    std::vector<uint16_t> ucs;
    if (lfn) {
        for (uint32_t cp : utf8Zerlegen(lang)) {
            if (cp >= 0x10000) { cp -= 0x10000; ucs.push_back(static_cast<uint16_t>(0xD800 + (cp >> 10))); ucs.push_back(static_cast<uint16_t>(0xDC00 + (cp & 0x3FF))); }
            else ucs.push_back(static_cast<uint16_t>(cp));
        }
    }
    const size_t nfrag = lfn ? (ucs.size() + 12) / 13 : 0;
    size_t erster = 0;
    const std::vector<uint8_t> alt = v.bytes;
    if (!slotsBeschaffen(v, nfrag + 1, erster)) return false;
    const uint8_t summe = kurznamenSumme(kurz);
    for (size_t k = 0; k < nfrag; ++k) {
        uint8_t* e = &v.bytes[(erster + k) * 32];
        std::memset(e, 0, 32);
        const size_t nr = nfrag - k;               // erster Slot = hoechste Nummer
        e[0] = static_cast<uint8_t>(nr | (k == 0 ? 0x40 : 0));
        e[11] = kAttrLfn; e[13] = summe;
        auto zeichen = [&](size_t i) -> uint16_t { size_t p = (nr - 1) * 13 + i; return p < ucs.size() ? ucs[p] : (p == ucs.size() ? 0x0000 : 0xFFFF); };
        for (int i = 0; i < 5; ++i)  wr16(e + 1 + 2 * i, zeichen(static_cast<size_t>(i)));
        for (int i = 0; i < 6; ++i)  wr16(e + 14 + 2 * i, zeichen(static_cast<size_t>(5 + i)));
        for (int i = 0; i < 2; ++i)  wr16(e + 28 + 2 * i, zeichen(static_cast<size_t>(11 + i)));
    }
    uint8_t* e = &v.bytes[(erster + nfrag) * 32];
    std::memset(e, 0, 32);
    std::memcpy(e, kurz, 11);
    e[11] = attr; e[12] = nt;
    uint16_t d, z; epocheZuDos(mtime ? mtime : jetztOrtszeit(), d, z);
    wr16(e + 14, z); wr16(e + 16, d); wr16(e + 18, d);      // Erstellung = Zugriff = Aenderung
    wr16(e + 22, z); wr16(e + 24, d);
    wr16(e + 26, cluster); wr32(e + 28, groesse);
    return speichereVerz(v, alt);
}

bool FatFileSystem::loescheEintrag(Verz& v, size_t index) {
    const std::vector<uint8_t> alt = v.bytes;
    const Eintrag e = v.eintraege[index];
    for (size_t i = e.erster; i <= e.slot; ++i) v.bytes[i * 32] = 0xE5;
    return speichereVerz(v, alt);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Dateien: lesen, schreiben, loeschen, Verzeichnisse
// ═════════════════════════════════════════════════════════════════════════════

bool FatFileSystem::read(const std::string& name, std::vector<uint8_t>& out) {
    Verz v; int i; std::string blatt; bool eltern;
    if (!sucheEintrag(name, v, i, blatt, eltern)) {
        if (!eltern && !lastError().empty()) return false;
        return fail("Datei '" + name + "' nicht gefunden");
    }
    const Eintrag& e = v.eintraege[static_cast<size_t>(i)];
    if (e.istDir()) return fail("'" + name + "' ist ein Verzeichnis");
    out.clear();
    if (e.groesse == 0) return true;
    bool heil = true; std::string grund;
    const std::vector<uint16_t> k = kette(e.cluster, &heil, &grund);
    const uint32_t cb = bpb_.clusterBytes();
    const uint64_t kap = static_cast<uint64_t>(k.size()) * cb;
    if (!heil || kap < e.groesse)
        return fail("Die Clusterkette von '" + name + "' ist beschaedigt: "
                    + (!heil ? grund : "sie fasst " + std::to_string(kap) + " Byte, die Datei soll " + std::to_string(e.groesse) + " haben"));
    out.resize(e.groesse);
    // zusammenhaengende Cluster in einem Zug lesen
    uint64_t pos = 0;
    for (size_t a = 0; a < k.size() && pos < e.groesse;) {
        size_t b = a + 1;
        while (b < k.size() && k[b] == k[b - 1] + 1) ++b;
        const uint64_t bytes = std::min<uint64_t>(static_cast<uint64_t>(b - a) * cb, e.groesse - pos);
        if (!lesen(clusterOffset(k[a]), &out[pos], bytes)) { out.clear(); return false; }
        pos += bytes;
        a = b;
    }
    return true;
}

bool FatFileSystem::wouldFit(const std::vector<PlannedFile>& files, FitReport& r) const {
    r = FitReport{};
    const uint32_t cb = bpb_.clusterBytes();
    // Verzeichnisplaetze: Wurzel zaehlt streng, Unterverzeichnisse wachsen
    Verz w;
    int freiWurzel = 0;
    if (ladeVerz(0, w)) for (size_t i = 0; i < w.bytes.size() / 32; ++i) if (w.bytes[i * 32] == 0 || w.bytes[i * 32] == 0xE5) ++freiWurzel;
    uint64_t cl = 0;
    int wurzelNeu = 0;
    std::set<std::string> neueDirs;
    for (const PlannedFile& f : files) {
        const std::vector<std::string> t = pfadTeile(f.name);
        if (t.empty()) continue;
        uint64_t alt = 0;
        bool ersetzt = false;
        Verz eltern; int idx; std::string blatt; bool ep;
        if (sucheEintrag(f.name, eltern, idx, blatt, ep)) {
            const Eintrag& e = eltern.eintraege[static_cast<size_t>(idx)];
            if (!e.istDir()) { ersetzt = true; alt = kette(e.cluster).size(); }
        }
        const uint64_t brauch = (f.size + cb - 1) / cb;
        cl += brauch > alt ? brauch - alt : 0;
        // Verzeichnisse, die es noch nicht gibt
        std::string akt;
        for (size_t i = 0; i + 1 < t.size(); ++i) {
            akt += "/" + grossschrift(t[i]);
            Verz d; std::string fehler;
            std::vector<std::string> teil(t.begin(), t.begin() + static_cast<long>(i) + 1);
            if (!findeVerz(teil, teil.size(), d, fehler) && neueDirs.insert(akt).second) {
                cl += 1;
                if (i == 0) ++wurzelNeu;
            }
        }
        if (t.size() == 1 && !ersetzt) {
            const size_t lfn = utf8Zerlegen(t[0]).size() > 12 ? (utf8Zerlegen(t[0]).size() + 12) / 13 : 0;
            wurzelNeu += 1 + static_cast<int>(lfn);
        }
        (void)ersetzt;
    }
    r.needed = cl * cb;
    r.available = static_cast<uint64_t>(freiCluster()) * cb;
    r.dir_needed = wurzelNeu;
    r.dir_free = freiWurzel;
    r.fits = r.needed <= r.available && wurzelNeu <= freiWurzel;
    if (!r.fits) {
        if (r.needed > r.available)
            r.detail = "Bedarf " + std::to_string(r.needed) + " Byte (in Clustern zu " + std::to_string(cb) + " Byte), frei sind "
                     + std::to_string(r.available);
        else
            r.detail = "Das Wurzelverzeichnis hat nur " + std::to_string(freiWurzel) + " freie Plaetze, benoetigt werden " + std::to_string(wurzelNeu);
    }
    return true;
}

bool FatFileSystem::write(const std::string& name, const std::vector<uint8_t>& data, const WriteOptions& opt) {
    const std::vector<std::string> t = pfadTeile(name);
    if (t.empty()) return fail("Kein Dateiname");
    if (data.size() > 0xFFFFFFFFull) return fail("Datei zu gross fuer FAT (4 GB)");
    // ── Platz vorab (kein halbes Ergebnis) ───────────────────────────────────
    {
        FitReport r;
        PlannedFile pf; pf.name = name; pf.size = data.size();
        wouldFit({pf}, r);
        if (!r.fits) return fail("Kein Platz fuer '" + name + "': " + r.detail);
    }
    // ── Verzeichnisse anlegen, wo sie fehlen ─────────────────────────────────
    if (t.size() > 1) {
        std::string pfad;
        for (size_t i = 0; i + 1 < t.size(); ++i) { pfad += (i ? "/" : "") + t[i]; if (!makeDirectory(pfad)) return false; }
    }
    Verz v; std::string fehler;
    if (!findeVerz(t, t.size() - 1, v, fehler)) return fail(fehler);
    const std::string& blatt = t.back();
    const int vorh = findeName(v, blatt);
    const uint32_t cb = bpb_.clusterBytes();
    const uint64_t n = (data.size() + cb - 1) / cb;

    bool lfn = opt.fat_lfn < 0 ? langeNamenUeblich() : opt.fat_lfn != 0;
    uint8_t kurz[11], nt = 0; bool brauchtLfn = false;
    uint8_t attr = opt.fat_attr_gesetzt ? opt.fat_attr : kAttrArc;
    if (vorh >= 0) {
        const Eintrag& e = v.eintraege[static_cast<size_t>(vorh)];
        if (e.istDir()) return fail("'" + name + "' ist ein Verzeichnis");
        if (!opt.overwrite) return fail("'" + name + "' existiert schon");
        if (e.attr & kAttrRo) return fail("'" + name + "' ist schreibgeschuetzt (R/O) - zuerst `attr --no-ro`");
        if (!opt.fat_attr_gesetzt) attr = static_cast<uint8_t>(e.attr | kAttrArc);
    } else {
        std::string err;
        if (!legeNameAn(v, blatt, lfn, kurz, nt, brauchtLfn, err)) return fail(err);
    }

    // ── Cluster vergeben (der alte Inhalt gibt seine zuerst zurueck) ─────────
    const std::vector<uint16_t> altFat = fat_;
    if (vorh >= 0) freigeben(v.eintraege[static_cast<size_t>(vorh)].cluster);
    std::vector<uint16_t> neu;
    if (n && !findeFrei(static_cast<uint32_t>(n), neu)) { fat_ = altFat; return fail("Kein Platz fuer '" + name + "'"); }
    verketten(neu);

    // ── Daten schreiben (zusammenhaengende Laeufe in einem Zug) ──────────────
    std::vector<uint8_t> puffer;
    uint64_t pos = 0;
    for (size_t a = 0; a < neu.size();) {
        size_t b = a + 1;
        while (b < neu.size() && neu[b] == neu[b - 1] + 1) ++b;
        const uint64_t bytes = static_cast<uint64_t>(b - a) * cb;
        const uint64_t echt = std::min<uint64_t>(bytes, data.size() - pos);
        puffer.assign(bytes, 0);
        std::memcpy(puffer.data(), &data[pos], echt);
        if (!schreiben(clusterOffset(neu[a]), puffer.data(), puffer.size())) { fat_ = altFat; return false; }
        pos += echt;
        a = b;
    }
    const uint16_t start = neu.empty() ? 0 : neu[0];
    const int64_t mt = opt.fat_mtime ? opt.fat_mtime : jetztOrtszeit();

    if (vorh >= 0) {
        const std::vector<uint8_t> alt = v.bytes;
        uint8_t* e = &v.bytes[v.eintraege[static_cast<size_t>(vorh)].slot * 32];
        uint16_t d, z; epocheZuDos(mt, d, z);
        e[11] = attr;
        wr16(e + 22, z); wr16(e + 24, d); wr16(e + 18, d);
        wr16(e + 26, start); wr32(e + 28, static_cast<uint32_t>(data.size()));
        if (!speichereVerz(v, alt)) { fat_ = altFat; return false; }
    } else {
        if (!trageEin(v, blatt, kurz, nt, attr, start, static_cast<uint32_t>(data.size()), mt, brauchtLfn && lfn)) { fat_ = altFat; return false; }
    }
    return schreibeFat();
}

bool FatFileSystem::makeDirectory(const std::string& name) {
    const std::vector<std::string> t = pfadTeile(name);
    if (t.empty()) return fail("Kein Verzeichnisname");
    // mkdir -p: jede Stufe einzeln
    for (size_t k = 1; k <= t.size(); ++k) {
        Verz v; std::string fehler;
        if (!findeVerz(t, k - 1, v, fehler)) return fail(fehler);
        const int vorh = findeName(v, t[k - 1]);
        if (vorh >= 0) {
            if (!v.eintraege[static_cast<size_t>(vorh)].istDir()) return fail("'" + t[k - 1] + "' ist eine Datei, kein Verzeichnis");
            continue;
        }
        const bool lfn = langeNamenUeblich();
        uint8_t kurz[11], nt; bool brauchtLfn; std::string err;
        if (!legeNameAn(v, t[k - 1], lfn, kurz, nt, brauchtLfn, err)) return fail(err);
        std::vector<uint16_t> c;
        if (!findeFrei(1, c)) return fail("Kein freier Cluster fuer das Verzeichnis '" + t[k - 1] + "'");
        const std::vector<uint16_t> altFat = fat_;
        fat_[c[0]] = 0xFFF;
        // Inhalt: "." und ".."
        std::vector<uint8_t> blk(bpb_.clusterBytes(), 0);
        const int64_t jetzt = jetztOrtszeit();
        uint16_t d, z; epocheZuDos(jetzt, d, z);
        auto punkt = [&](size_t i, const char* nm, uint16_t cl) {
            uint8_t* e = &blk[i * 32];
            std::memset(e, ' ', 11); std::memcpy(e, nm, std::strlen(nm));
            e[11] = kAttrDir; wr16(e + 14, z); wr16(e + 16, d); wr16(e + 18, d); wr16(e + 22, z); wr16(e + 24, d);
            wr16(e + 26, cl); wr32(e + 28, 0);
            (void)nm;
        };
        punkt(0, ".", c[0]);
        punkt(1, "..", v.cluster);
        if (!schreiben(clusterOffset(c[0]), blk.data(), blk.size())) { fat_ = altFat; return false; }
        if (!trageEin(v, t[k - 1], kurz, nt, kAttrDir, c[0], 0, jetzt, brauchtLfn && lfn)) { fat_ = altFat; return false; }
        if (!schreibeFat()) return false;
    }
    return true;
}

bool FatFileSystem::erase(const std::string& name) {
    Verz v; int i; std::string blatt; bool eltern;
    if (!sucheEintrag(name, v, i, blatt, eltern)) {
        if (!eltern && !lastError().empty()) return false;
        return fail("'" + name + "' nicht gefunden");
    }
    const Eintrag e = v.eintraege[static_cast<size_t>(i)];
    if (e.attr & kAttrRo) return fail("'" + name + "' ist schreibgeschuetzt (R/O) - zuerst `attr --no-ro`");
    if (e.istDir()) {
        Verz d;
        if (gueltigerCluster(e.cluster) && ladeVerz(e.cluster, d)) {
            for (const Eintrag& k : d.eintraege) {
                if (k.attr & kAttrVol) continue;
                if (k.name[0] == '.' && (k.name[1] == ' ' || (k.name[1] == '.' && k.name[2] == ' '))) continue;
                return fail("Verzeichnis '" + name + "' ist nicht leer");
            }
        }
    }
    const std::vector<uint16_t> altFat = fat_;
    freigeben(e.cluster);
    if (!loescheEintrag(v, static_cast<size_t>(i))) { fat_ = altFat; return false; }
    return schreibeFat();
}

bool FatFileSystem::setAttributes(const std::string& name, const FatAttrs& a) {
    Verz v; int i; std::string blatt; bool eltern;
    if (!sucheEintrag(name, v, i, blatt, eltern)) {
        if (!eltern && !lastError().empty()) return false;
        return fail("'" + name + "' nicht gefunden");
    }
    const std::vector<uint8_t> alt = v.bytes;
    uint8_t* e = &v.bytes[v.eintraege[static_cast<size_t>(i)].slot * 32];
    auto bit = [&](bool set, bool wert, uint8_t m) { if (set) e[11] = static_cast<uint8_t>(wert ? (e[11] | m) : (e[11] & ~m)); };
    bit(a.set_read_only, a.read_only, kAttrRo);
    bit(a.set_hidden, a.hidden, kAttrHid);
    bit(a.set_system, a.system, kAttrSys);
    bit(a.set_archive, a.archive, kAttrArc);
    if (a.set_mtime) { uint16_t d, z; epocheZuDos(a.mtime, d, z); wr16(e + 22, z); wr16(e + 24, d); }
    return speichereVerz(v, alt);
}

bool FatFileSystem::mkfs() {
    // FAT: Medienkennung + zwei Endekennungen, Rest frei; Wurzel leer; Datenbereich bleibt unberuehrt.
    fat_.assign(bpb_.clusters + 2, 0);
    fat_[0] = static_cast<uint16_t>(0xF00 | bpb_.media);
    fat_[1] = 0xFFF;
    fat_roh_.assign(static_cast<size_t>(bpb_.spf) * bpb_.bps, 0);
    if (!schreibeFat()) return false;
    std::vector<uint8_t> wurzel(static_cast<size_t>(bpb_.root_secs) * bpb_.bps, 0);
    return schreiben(sektorOffset(bpb_.root_start), wurzel.data(), wurzel.size());
}

bool FatFileSystem::setLabel(const std::string& name) {
    const std::string g = grossschrift(name);
    if (g.size() > 11) return fail("Datentraegername hoechstens 11 Zeichen");
    uint8_t n[11]; std::memset(n, ' ', 11);
    size_t k = 0;
    for (uint32_t c : utf8Zerlegen(g)) {
        const int o = cpZuOem(c);
        if (o < 0 || (o < 0x20) || std::strchr("\\/:*?\"<>|.", o ? o : 1)) return fail("Zeichen im Datentraegernamen nicht erlaubt");
        if (k < 11) n[k++] = static_cast<uint8_t>(o);
    }
    Verz v;
    if (!ladeVerz(0, v)) return false;
    const std::vector<uint8_t> alt = v.bytes;
    int slot = -1;
    for (const Eintrag& e : v.eintraege) if ((e.attr & kAttrVol) && (e.attr & kAttrLfn) != kAttrLfn) { slot = static_cast<int>(e.slot); break; }
    if (name.empty()) {
        if (slot >= 0) v.bytes[static_cast<size_t>(slot) * 32] = 0xE5;
    } else if (slot >= 0) {
        std::memcpy(&v.bytes[static_cast<size_t>(slot) * 32], n, 11);
    } else {
        size_t erster;
        if (!slotsBeschaffen(v, 1, erster)) return false;
        uint8_t* e = &v.bytes[erster * 32];
        std::memset(e, 0, 32); std::memcpy(e, n, 11); e[11] = kAttrVol;
        uint16_t d, z; epocheZuDos(jetztOrtszeit(), d, z);
        wr16(e + 22, z); wr16(e + 24, d);
    }
    if (!speichereVerz(v, alt)) return false;
    if (bpb_.erweitert) {
        uint8_t b[512];
        if (raum_.read(0, b, sizeof b) && (b[38] == 0x29 || b[38] == 0x28)) {
            std::string l = name.empty() ? "NO NAME" : g;
            l.resize(11, ' ');
            std::memcpy(b + 43, l.data(), 11);
            if (!schreiben(0, b, sizeof b)) return false;
        }
    }
    return true;
}

FsInfo FatFileSystem::info() const {
    FsInfo i;
    i.label = label();
    const uint32_t cb = bpb_.clusterBytes();
    i.total_bytes = static_cast<uint64_t>(bpb_.clusters) * cb;
    i.free_bytes = static_cast<uint64_t>(freiCluster()) * cb;
    i.used_bytes = i.total_bytes - i.free_bytes;
    for (const FileEntry& e : list()) if (e.type != "d") ++i.files;
    return i;
}

bool FatFileSystem::firstSector(const std::string& name, FsRecoverOrt& out) const {
    Verz v; int i; std::string blatt; bool eltern;
    if (!sucheEintrag(name, v, i, blatt, eltern)) return false;
    const Eintrag& e = v.eintraege[static_cast<size_t>(i)];
    if (!gueltigerCluster(e.cluster)) return false;
    const uint64_t off = clusterOffset(e.cluster);
    for (size_t k = 0; k < raum_.trackCount(); ++k) {
        const SectorSpace::TrackRef t = raum_.trackAt(k);
        if (off >= t.start && off < t.start + t.bytes) {
            out.cyl = t.cyl; out.head = t.head;
            out.sector = t.first_id + static_cast<int>((off - t.start) / t.sector_size);
            return true;
        }
    }
    return false;
}

std::string FatFileSystem::blockList(const std::string& name) const {
    Verz v; int i; std::string blatt; bool eltern;
    if (!sucheEintrag(name, v, i, blatt, eltern)) return {};
    const auto k = kette(v.eintraege[static_cast<size_t>(i)].cluster);
    std::string s;
    for (size_t a = 0; a < k.size();) {
        size_t b = a;
        while (b + 1 < k.size() && k[b + 1] == k[b] + 1) ++b;
        if (!s.empty()) s += ' ';
        s += std::to_string(k[a]);
        if (b > a) s += "-" + std::to_string(k[b]);
        a = b + 1;
    }
    return s;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Bootsektor
// ═════════════════════════════════════════════════════════════════════════════

bool FatFileSystem::readBootSector(std::vector<uint8_t>& out) const {
    out.assign(512, 0);
    return lesen(0, out.data(), out.size());
}

bool FatFileSystem::bootsektorMitBpb(const std::vector<uint8_t>& bild, const Bpb& b,
                                     const std::vector<uint8_t>& aktuell, std::vector<uint8_t>& out,
                                     std::string& err) {
    if (bild.size() != 512) { err = "Ein FAT-Bootsektor ist genau 512 Byte gross (hier " + std::to_string(bild.size()) + ")"; return false; }
    if (bild[510] != 0x55 || bild[511] != 0xAA) { err = "Der Bootsektor traegt keine Kennung 55 AA bei Byte 510"; return false; }
    const int prog = programmAnfang(bild);
    if (prog < 0x0B + 13 || prog > 510) { err = "Der Bootsektor beginnt nicht mit einem Sprungbefehl (EB xx 90 / E9) auf ein Programm hinter dem BPB"; return false; }
    out = bild;
    // BPB der Diskette einsetzen: die Felder, die der Sektor ueberhaupt fuehrt (Sprungziel begrenzt)
    auto setze16 = [&](int off, uint32_t v) { if (off + 2 <= prog) wr16(&out[static_cast<size_t>(off)], v); };
    auto setze8  = [&](int off, uint32_t v) { if (off + 1 <= prog) out[static_cast<size_t>(off)] = static_cast<uint8_t>(v); };
    setze16(11, b.bps); setze8(13, b.spc); setze16(14, b.rsv); setze8(16, b.nfats); setze16(17, b.rde);
    setze16(19, b.total < 65536 ? b.total : 0);
    setze8(21, b.media); setze16(22, b.spf); setze16(24, b.spt); setze16(26, b.heads);
    if (prog >= 32) { wr16(&out[28], b.hidden & 0xFFFF); if (prog >= 32) wr16(&out[30], b.hidden >> 16); }
    else if (prog >= 30) wr16(&out[28], b.hidden & 0xFFFF);
    if (prog >= 36 && b.total >= 65536) wr32(&out[32], b.total);
    else if (prog >= 36) wr32(&out[32], 0);
    // erweiterter BPB: Seriennummer, Name, Typ vom aktuellen Sektor uebernehmen (wenn dort vorhanden)
    if (prog >= 62 && out[38] == 0x29 && aktuell.size() == 512 && aktuell[38] == 0x29) {
        std::memcpy(&out[39], &aktuell[39], 4);
        std::memcpy(&out[43], &aktuell[43], 11);
    }
    return true;
}

bool FatFileSystem::writeBootSector(const std::vector<uint8_t>& bild) {
    std::vector<uint8_t> alt, neu;
    if (!readBootSector(alt)) return false;
    std::string err;
    if (!bootsektorMitBpb(bild, bpb_, alt, neu, err)) return fail(err);
    return schreiben(0, neu.data(), neu.size());
}

// ═════════════════════════════════════════════════════════════════════════════
//  Pruefung und Reparatur
// ═════════════════════════════════════════════════════════════════════════════

namespace {

struct Eigner { std::string pfad; bool dir; uint32_t groesse; uint16_t start; };

} // namespace

struct FatPruefer {
    const FatFileSystem& f;
    FsCheckReport& rep;
    FsFindings fi;
    std::vector<int> besitzer;          ///< Index in `eigner` oder -1
    std::vector<Eigner> eigner;
    FatPruefer(const FatFileSystem& fs, FsCheckReport& r) : f(fs), rep(r), fi(r), besitzer(fs.fat_.size(), -1) {}

    void wandere(uint16_t cluster, const std::string& pfad, int tiefe, std::set<uint16_t>& dirs) {
        FatFileSystem::Verz v;
        if (!f.ladeVerz(cluster, v)) {
            fi.add("fat.verz.unlesbar", FsSeverity::Fehler, FsLayer::Dateien, pfad.empty() ? "/" : pfad, "Verzeichnis nicht lesbar: " + f.lastError());
            return;
        }
        if (v.defekt) fi.add("fat.verz.kette", FsSeverity::Fehler, FsLayer::Dateien, pfad.empty() ? "/" : pfad, "Die Clusterkette des Verzeichnisses ist beschaedigt");
        // Beschlagnahme der Verzeichnisketten selbst
        if (cluster) for (uint16_t c : v.kette) besitze(c, pfad, true, 0, cluster);
        // . und ..
        if (cluster) {
            bool hatPunkt = false, hatDoppel = false;
            for (const auto& e : v.eintraege) {
                if (e.name[0] == '.' && e.name[1] == ' ') { hatPunkt = true; if (e.cluster != cluster) { auto& x = fi.add("fat.verz.punkt", FsSeverity::Fehler, FsLayer::Dateien, pfad, "Der Eintrag '.' zeigt auf Cluster " + std::to_string(e.cluster) + ", das Verzeichnis liegt auf " + std::to_string(cluster)); FsRepair r; r.kind = "fat.verz.punkte"; r.text = "'.' und '..' neu setzen"; r.empfohlen = true; r.s = pfad; x.repairs.push_back(r); } }
                if (e.name[0] == '.' && e.name[1] == '.' && e.name[2] == ' ') hatDoppel = true;
            }
            if (!hatPunkt || !hatDoppel) { auto& x = fi.add("fat.verz.punkt", FsSeverity::Fehler, FsLayer::Dateien, pfad, "Dem Verzeichnis fehlt '.' bzw. '..'"); FsRepair r; r.kind = "fat.verz.punkte"; r.text = "'.' und '..' neu anlegen"; r.empfohlen = true; r.s = pfad; x.repairs.push_back(r); }
        }
        for (const auto& e : v.eintraege) {
            if (e.attr & fat::kAttrVol) continue;
            if (e.name[0] == '.' && (e.name[1] == ' ' || (e.name[1] == '.' && e.name[2] == ' '))) continue;
            const std::string name = f.zuFileEntry(e, pfad.empty() ? "" : pfad + "/").name;
            if (!e.lfn_gueltig) fi.add("fat.lfn.pruefsumme", FsSeverity::Info, FsLayer::Verwaltung, name, "Eintraege fuer den langen Namen passen nicht zum Kurznamen (werden ignoriert)");
            if (e.istDir()) {
                if (!f.gueltigerCluster(e.cluster)) {
                    fi.add("fat.kette.wild", FsSeverity::Fehler, FsLayer::Dateien, name, "Verzeichnis ohne gueltigen Anfangscluster (" + std::to_string(e.cluster) + ")");
                    continue;
                }
                if (dirs.count(e.cluster) || tiefe > 32) { fi.add("fat.verz.schleife", FsSeverity::Fehler, FsLayer::Dateien, name, "Verzeichnisschleife"); continue; }
                dirs.insert(e.cluster);
                wandere(e.cluster, name, tiefe + 1, dirs);
                continue;
            }
            pruefeDatei(e, name);
        }
    }

    std::set<std::string> gemeldet;     ///< Dateien, fuer die der Kreuzverweis schon gemeldet ist (einer je Datei)

    void besitze(uint16_t c, const std::string& pfad, bool dir, uint32_t groesse, uint16_t start) {
        if (besitzer[c] >= 0) {
            if (gemeldet.insert(pfad).second) {
                auto& x = fi.add("fat.cluster.doppelt", FsSeverity::Gefahr, FsLayer::Dateien, pfad,
                                 "Cluster " + std::to_string(c) + " gehoert auch '" + eigner[static_cast<size_t>(besitzer[c])].pfad + "' (Kreuzverweis)");
                FsRepair r; r.kind = "fat.doppelt.trennen"; r.text = "Die gemeinsamen Cluster fuer '" + pfad + "' kopieren (jede Datei bekommt ihre eigenen)";
                r.empfohlen = true; r.s = pfad; x.repairs.push_back(r);
            }
            return;
        }
        eigner.push_back({pfad, dir, groesse, start});
        besitzer[c] = static_cast<int>(eigner.size() - 1);
    }

    void pruefeDatei(const FatFileSystem::Eintrag& e, const std::string& name) {
        const uint32_t cb = f.bpb_.clusterBytes();
        if (e.cluster == 0) {
            if (e.groesse != 0) {
                auto& x = fi.add("fat.groesse.ohne.kette", FsSeverity::Fehler, FsLayer::Dateien, name, "Die Datei soll " + std::to_string(e.groesse) + " Byte haben, hat aber keinen Cluster");
                FsRepair r; r.kind = "fat.groesse.angleichen"; r.text = "Groesse auf 0 setzen"; r.datenverlust = false; r.empfohlen = true; r.s = name; r.a = 0; x.repairs.push_back(r);
            }
            return;
        }
        bool heil = true; std::string grund;
        const std::vector<uint16_t> k = f.kette(e.cluster, &heil, &grund);
        for (uint16_t c : k) besitze(c, name, false, e.groesse, e.cluster);
        if (!heil) {
            auto& x = fi.add("fat.kette.wild", FsSeverity::Fehler, FsLayer::Dateien, name, "Die Clusterkette ist beschaedigt: " + grund);
            FsRepair r; r.kind = "fat.kette.kappen"; r.text = "Die Datei an der Bruchstelle abschneiden (" + std::to_string(k.size()) + " Cluster bleiben)";
            r.datenverlust = true; r.empfohlen = true; r.s = name; r.a = static_cast<int>(k.size()); x.repairs.push_back(r);
            return;
        }
        const uint64_t kap = static_cast<uint64_t>(k.size()) * cb;
        const uint64_t noetig = (static_cast<uint64_t>(e.groesse) + cb - 1) / cb;
        if (kap < e.groesse) {
            auto& x = fi.add("fat.groesse.zukurz", FsSeverity::Fehler, FsLayer::Dateien, name, "Die Kette fasst nur " + std::to_string(kap) + " Byte, die Datei soll " + std::to_string(e.groesse) + " haben");
            FsRepair r; r.kind = "fat.groesse.angleichen"; r.text = "Die Groesse auf " + std::to_string(kap) + " Byte kuerzen"; r.empfohlen = true; r.s = name; r.a = static_cast<int>(kap); x.repairs.push_back(r);
        } else if (k.size() > noetig) {
            auto& x = fi.add("fat.kette.zulang", FsSeverity::Warnung, FsLayer::Dateien, name, "Die Kette hat " + std::to_string(k.size() - noetig) + " Cluster mehr, als die Groesse braucht");
            FsRepair r; r.kind = "fat.kette.kuerzen"; r.text = "Ueberzaehlige Cluster freigeben"; r.empfohlen = true; r.s = name; r.a = static_cast<int>(noetig); x.repairs.push_back(r);
        }
    }
};

FsCheckReport FatFileSystem::check(FsCheckLevel level, bool nachladen) const {
    FsCheckReport rep;
    rep.level = level;
    FsFindings fi(rep);
    const auto bekannt = [&](uint64_t von, uint64_t laenge) {
        for (uint64_t o = von; o < von + laenge; o += 512) {
            for (size_t k = 0; k < raum_.trackCount(); ++k) {
                const SectorSpace::TrackRef t = raum_.trackAt(k);
                if (o >= t.start && o < t.start + t.bytes) { if (!raum_.trackKnown(t.cyl, t.head)) return false; break; }
            }
        }
        return true;
    };

    // ── Schritt 1: Bootsektor und BPB ────────────────────────────────────────
    rep.schritt("fat.schritt.bpb", "Bootsektor und BIOS-Parameterblock");
    {
        uint16_t bps, spt, heads; bool ein;
        if (geometrieVon(raum_, bps, spt, heads, ein) && ein) {
            if (bpb_.mit_bpb && (bpb_.spt != spt || bpb_.heads != heads))
                fi.add("fat.bpb.geometrie", FsSeverity::Info, FsLayer::Verwaltung, "Bootsektor",
                       "Der BPB nennt " + std::to_string(bpb_.heads) + " Koepfe/" + std::to_string(bpb_.spt) + " Sektoren je Spur, die Diskette hat "
                       + std::to_string(heads) + "/" + std::to_string(spt));
        }
        if (!bpb_.mit_bpb) fi.add("fat.bpb.fehlt", FsSeverity::Info, FsLayer::Verwaltung, "Bootsektor", "Kein BPB (DOS 1.x/2.0): die Lage folgt aus Groesse und Medienkennung");
        const uint64_t unbenutzt = raum_.size() - static_cast<uint64_t>(bpb_.total) * bpb_.bps;
        if (unbenutzt >= bpb_.bps * 2 && bpb_.mit_bpb)
            fi.add("fat.bpb.klein", FsSeverity::Info, FsLayer::Verwaltung, "Bootsektor", std::to_string(unbenutzt / bpb_.bps) + " Sektoren hinter dem Dateisystem bleiben ungenutzt");
    }

    // ── Schritt 2: FAT ───────────────────────────────────────────────────────
    const uint64_t fatBytes = static_cast<uint64_t>(bpb_.spf) * bpb_.bps;
    const uint64_t fatGesamt = fatBytes * bpb_.nfats;
    if (!nachladen && !bekannt(sektorOffset(bpb_.fat_start), fatGesamt)) {
        rep.schrittEntfaellt("fat.schritt.fat", "FAT und ihre Kopien", "noch nicht von der Diskette gelesen");
        rep.vollstaendig = false;
    } else {
        rep.schritt("fat.schritt.fat", "FAT und ihre Kopien");
        if ((fat_[0] & 0xFF) != bpb_.media)
            fi.add("fat.fat.medien", FsSeverity::Warnung, FsLayer::Verwaltung, "FAT", "Die FAT beginnt mit Medienkennung 0x" + [&] { char b[8]; std::snprintf(b, sizeof b, "%02X", fat_[0] & 0xFF); return std::string(b); }()
                   + ", der BPB nennt 0x" + [&] { char b[8]; std::snprintf(b, sizeof b, "%02X", bpb_.media); return std::string(b); }());
        std::vector<uint8_t> k0(fatBytes), kn(fatBytes);
        const bool ok0 = raum_.read(sektorOffset(bpb_.fat_start), k0.data(), k0.size());
        for (int c = 1; c < bpb_.nfats && ok0; ++c) {
            if (!raum_.read(sektorOffset(bpb_.fat_start + static_cast<uint32_t>(c) * bpb_.spf), kn.data(), kn.size())) {
                fi.add("fat.fat.unlesbar", FsSeverity::Fehler, FsLayer::Medium, "FAT " + std::to_string(c + 1), "nicht lesbar");
                continue;
            }
            // Vergleich der benutzten Eintraege
            int abw = 0;
            const size_t nutz = std::min<size_t>(k0.size(), (static_cast<size_t>(bpb_.clusters + 2) * 3 + 1) / 2);
            for (size_t i = 0; i < nutz; ++i) if (k0[i] != kn[i]) ++abw;
            if (abw) {
                auto& x = fi.add("fat.fat.abweichend", FsSeverity::Fehler, FsLayer::Verwaltung, "FAT " + std::to_string(c + 1),
                                 "FAT " + std::to_string(c + 1) + " weicht von FAT 1 in " + std::to_string(abw) + " Byte ab");
                FsRepair r1; r1.kind = "fat.fat.angleichen"; r1.text = "FAT 1 auf alle Kopien schreiben"; r1.empfohlen = true; r1.a = 0; x.repairs.push_back(r1);
                FsRepair r2; r2.kind = "fat.fat.angleichen"; r2.text = "FAT " + std::to_string(c + 1) + " auf alle Kopien schreiben (FAT 1 verwerfen)"; r2.datenverlust = true; r2.a = c; x.repairs.push_back(r2);
            }
        }
        int defekt = 0;
        for (uint32_t c = 2; c < fat_.size(); ++c) if (fat_[c] == 0xFF7) ++defekt;
        if (defekt) fi.add("fat.fat.defekt", FsSeverity::Info, FsLayer::Verwaltung, "FAT", std::to_string(defekt) + " Cluster sind als defekt gekennzeichnet");
    }

    // ── Schritt 3: Verzeichnisse und Ketten (nur Vollpruefung) ───────────────
    if (level == FsCheckLevel::Voll) {
        rep.schritt("fat.schritt.baum", "Verzeichnisbaum, Ketten, Querverweise, verlorene Cluster");
        FatPruefer p(*this, rep);
        std::set<uint16_t> dirs;
        p.wandere(0, "", 0, dirs);
        // verlorene Cluster: belegt, aber von niemandem erreicht
        std::vector<uint16_t> verloren;
        for (uint32_t c = 2; c < fat_.size(); ++c)
            if (fat_[c] != 0 && fat_[c] != 0xFF7 && p.besitzer[c] < 0) verloren.push_back(static_cast<uint16_t>(c));
        if (!verloren.empty()) {
            // zu Ketten zusammenfassen: Anfaenge sind Cluster, auf die kein anderer verlorener zeigt
            std::set<uint16_t> ziel;
            for (uint16_t c : verloren) if (!istEnde(fat_[c]) && fat_[c] < fat_.size()) ziel.insert(fat_[c]);
            int ketten = 0;
            for (uint16_t c : verloren) if (!ziel.count(c)) ++ketten;
            if (ketten == 0) ketten = 1;
            auto& x = fi.add("fat.cluster.verloren", FsSeverity::Warnung, FsLayer::Dateien, "FAT",
                             std::to_string(verloren.size()) + " Cluster (" + std::to_string(verloren.size() * bpb_.clusterBytes()) + " Byte) in "
                             + std::to_string(ketten) + " Ketten sind belegt, gehoeren aber keiner Datei");
            FsRepair r1; r1.kind = "fat.verloren.retten"; r1.text = "Jede Kette als FILE0000.CHK im Wurzelverzeichnis sichern"; r1.empfohlen = true; x.repairs.push_back(r1);
            FsRepair r2; r2.kind = "fat.verloren.freigeben"; r2.text = "Die verlorenen Cluster freigeben"; r2.datenverlust = true; x.repairs.push_back(r2);
        }
    } else {
        rep.schrittEntfaellt("fat.schritt.baum", "Verzeichnisbaum, Ketten, Querverweise, verlorene Cluster", "nur bei der Vollpruefung");
    }
    rep.schrittEnde();
    rep.spuren_gelesen = rep.spuren_gesamt = static_cast<int>(raum_.trackCount());
    rep.sortieren();
    return rep;
}

bool FatFileSystem::repair(const FsRepair& r) {
    if (r.gesperrt) return fail("Gesperrt: " + r.warum);
    if (r.kind == "fat.fat.angleichen") {
        const size_t n = static_cast<size_t>(bpb_.spf) * bpb_.bps;
        std::vector<uint8_t> quelle(n);
        if (!lesen(sektorOffset(bpb_.fat_start + static_cast<uint32_t>(r.a) * bpb_.spf), quelle.data(), n)) return false;
        for (int c = 0; c < bpb_.nfats; ++c)
            if (c != r.a && !schreiben(sektorOffset(bpb_.fat_start + static_cast<uint32_t>(c) * bpb_.spf), quelle.data(), n)) return false;
        fat_roh_ = quelle;
        return ladeFat();
    }
    if (r.kind == "fat.verloren.retten" || r.kind == "fat.verloren.freigeben") {
        // Pruefung neu laufen lassen: nur so ist die Menge der verlorenen Cluster aktuell.
        FsCheckReport rep;
        FatPruefer p(*this, rep);
        std::set<uint16_t> dirs;
        p.wandere(0, "", 0, dirs);
        std::vector<uint16_t> verloren;
        for (uint32_t c = 2; c < fat_.size(); ++c)
            if (fat_[c] != 0 && fat_[c] != 0xFF7 && p.besitzer[c] < 0) verloren.push_back(static_cast<uint16_t>(c));
        if (r.kind == "fat.verloren.freigeben") {
            for (uint16_t c : verloren) fat_[c] = 0;
            return schreibeFat();
        }
        std::set<uint16_t> ziel;
        for (uint16_t c : verloren) if (!istEnde(fat_[c]) && fat_[c] < fat_.size()) ziel.insert(fat_[c]);
        int nr = 0;
        for (uint16_t c : verloren) {
            if (ziel.count(c)) continue;                 // keine Kopfstelle
            bool heil = true;
            const auto k = kette(c, &heil);
            if (!heil && k.empty()) continue;
            // Kette ggf. am Bruch abschliessen
            if (!heil) fat_[k.back()] = 0xFFF;
            char nm[32]; std::snprintf(nm, sizeof nm, "FILE%04d.CHK", nr++);
            Verz w;
            if (!ladeVerz(0, w)) return false;
            uint8_t kurz[11]; std::memset(kurz, ' ', 11);
            std::memcpy(kurz, nm, 8); std::memcpy(kurz + 8, "CHK", 3);
            if (!trageEin(w, nm, kurz, 0, kAttrArc, k[0], static_cast<uint32_t>(k.size() * bpb_.clusterBytes()), 0, false)) return false;
        }
        // Verbleibende Schleifenreste (keine Kopfstelle) freigeben
        return schreibeFat();
    }
    if (r.kind == "fat.kette.kappen" || r.kind == "fat.kette.kuerzen" || r.kind == "fat.groesse.angleichen") {
        Verz v; int i; std::string blatt; bool ep;
        if (!sucheEintrag(r.s, v, i, blatt, ep)) return fail("'" + r.s + "' nicht (mehr) gefunden");
        const std::vector<uint8_t> alt = v.bytes;
        Eintrag e = v.eintraege[static_cast<size_t>(i)];
        uint8_t* d = &v.bytes[e.slot * 32];
        if (r.kind == "fat.groesse.angleichen") {
            wr32(d + 28, static_cast<uint32_t>(r.a));
            if (r.a == 0 && e.cluster) { freigeben(e.cluster); wr16(d + 26, 0); }
        } else {
            const auto k = kette(e.cluster);
            const size_t behalten = static_cast<size_t>(r.a);
            if (behalten == 0) { freigeben(e.cluster); wr16(d + 26, 0); wr32(d + 28, 0); }
            else if (behalten <= k.size()) {
                for (size_t q = behalten; q < k.size(); ++q) fat_[k[q]] = 0;
                fat_[k[behalten - 1]] = 0xFFF;
                const uint64_t kap = static_cast<uint64_t>(behalten) * bpb_.clusterBytes();
                if (e.groesse > kap) wr32(d + 28, static_cast<uint32_t>(kap));
            }
        }
        if (!speichereVerz(v, alt)) return false;
        return schreibeFat();
    }
    if (r.kind == "fat.doppelt.trennen") {
        Verz v; int i; std::string blatt; bool ep;
        if (!sucheEintrag(r.s, v, i, blatt, ep)) return fail("'" + r.s + "' nicht (mehr) gefunden");
        const Eintrag e = v.eintraege[static_cast<size_t>(i)];
        // Wer gehoert alles wem?  Alle anderen Dateien durchgehen; was sie beanspruchen, wird kopiert.
        FsCheckReport rep;
        FatPruefer p(*this, rep);
        std::set<uint16_t> dirs;
        // Beanspruchung durch ANDERE Eintraege (alle ausser diesem) ermitteln
        std::vector<char> fremd(fat_.size(), 0);
        for (const FileEntry& fe : list()) {
            if (fe.name == r.s) continue;
            Verz v2; int j; std::string b2; bool ep2;
            if (!sucheEintrag(fe.name, v2, j, b2, ep2)) continue;
            for (uint16_t c : kette(v2.eintraege[static_cast<size_t>(j)].cluster)) fremd[c] = 1;
        }
        std::vector<uint16_t> k = kette(e.cluster);
        std::vector<size_t> teilen;
        for (size_t q = 0; q < k.size(); ++q) if (fremd[k[q]]) teilen.push_back(q);
        if (teilen.empty()) return true;
        std::vector<uint16_t> neu;
        if (!findeFrei(static_cast<uint32_t>(teilen.size()), neu)) return fail("Zu wenig freie Cluster, um die gemeinsamen Cluster zu kopieren");
        const uint32_t cb = bpb_.clusterBytes();
        std::vector<uint8_t> blk(cb);
        for (size_t q = 0; q < teilen.size(); ++q) {
            if (!lesen(clusterOffset(k[teilen[q]]), blk.data(), cb) || !schreiben(clusterOffset(neu[q]), blk.data(), cb)) return false;
            k[teilen[q]] = neu[q];
        }
        // FAT neu aufbauen (alte Eintraege des Besitzers bleiben fuer die anderen stehen)
        for (size_t q = 0; q < k.size(); ++q) fat_[k[q]] = q + 1 < k.size() ? k[q + 1] : 0xFFF;
        const std::vector<uint8_t> alt = v.bytes;
        wr16(&v.bytes[e.slot * 32 + 26], k[0]);
        if (!speichereVerz(v, alt)) return false;
        return schreibeFat();
    }
    if (r.kind == "fat.verz.punkte") {
        // '.' und '..' eines Unterverzeichnisses neu setzen
        const std::vector<std::string> t = pfadTeile(r.s);
        Verz eltern; std::string fehler;
        if (!findeVerz(t, t.size() - 1, eltern, fehler)) return fail(fehler);
        const int i = findeName(eltern, t.back());
        if (i < 0) return fail("Verzeichnis nicht gefunden");
        const uint16_t c = eltern.eintraege[static_cast<size_t>(i)].cluster;
        Verz d;
        if (!ladeVerz(c, d)) return false;
        const std::vector<uint8_t> alt = d.bytes;
        uint16_t dt, z; epocheZuDos(jetztOrtszeit(), dt, z);
        auto setze = [&](size_t slot, const char* nm, uint16_t cl) {
            uint8_t* e = &d.bytes[slot * 32];
            std::memset(e, 0, 32); std::memset(e, ' ', 11); std::memcpy(e, nm, std::strlen(nm));
            e[11] = kAttrDir; wr16(e + 22, z); wr16(e + 24, dt); wr16(e + 26, cl);
        };
        // sind Slot 0/1 belegt (andere Dateien)? dann Abbruch
        for (size_t s = 0; s < 2; ++s) {
            const uint8_t b = d.bytes[s * 32];
            const bool punktOderLeer = b == 0 || b == 0xE5 || b == '.';
            if (!punktOderLeer) return fail("Die ersten Plaetze des Verzeichnisses sind belegt - '.' und '..' lassen sich nicht ohne Umbau anlegen");
        }
        setze(0, ".", c); setze(1, "..", eltern.cluster);
        return speichereVerz(d, alt);
    }
    return FileSystem::repair(r);
}
