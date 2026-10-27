#include "core/peripherals/kassette/programmdatei.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace kassette::programmdatei {

namespace {

constexpr char kTapKennung[16] = {char(0xC3), 'K', 'C', '-', 'T', 'A', 'P', 'E', ' ', 'b', 'y', ' ', 'A', 'F', '.', ' '};
constexpr size_t kKcKopf = 128;       // KCC-Kopf = Kopfblock des KC-TAP
constexpr size_t kHeadersaveKopf = 32;

uint16_t le16(const std::vector<uint8_t>& d, size_t o) { return uint16_t(d[o] | (d[o + 1] << 8)); }
void put16(std::vector<uint8_t>& d, unsigned v) {
    d.push_back(uint8_t(v));
    d.push_back(uint8_t(v >> 8));
}

/// Nutzbarer Namensteil: Zeichen 20H..7EH, rechts ohne Leerzeichen/Nullen.
std::string namensText(const uint8_t* p, size_t n) {
    std::string s;
    for (size_t i = 0; i < n; ++i) s.push_back(p[i] >= 0x20 && p[i] < 0x7F ? char(p[i]) : ' ');
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

void namensFeld(std::vector<uint8_t>& d, const std::string& s, size_t n, uint8_t fuell) {
    for (size_t i = 0; i < n; ++i)
        d.push_back(i < s.size() ? uint8_t(std::toupper(static_cast<unsigned char>(s[i]))) : fuell);
}

bool istTap(const std::vector<uint8_t>& d, size_t o = 0) {
    return d.size() >= o + 16 && std::memcmp(d.data() + o, kTapKennung, 16) == 0;
}

std::optional<Abbild> lesenHeadersave(const std::vector<uint8_t>& d, std::string& fehler) {
    if (d.size() <= kHeadersaveKopf || d[13] != 0xD3 || d[14] != 0xD3 || d[15] != 0xD3) {
        fehler = "kein Headersave-Kopf (D3 D3 D3 bei Versatz 13 fehlt)";
        return std::nullopt;
    }
    Abbild a;
    a.anfang = le16(d, 0);
    const unsigned ende = le16(d, 2);
    if (ende < a.anfang) { fehler = "Headersave: Endadresse vor Anfangsadresse"; return std::nullopt; }
    const size_t soll = size_t(ende) - a.anfang + 1, ist = d.size() - kHeadersaveKopf;
    if (ist < soll) { fehler = "Headersave: Datei kuerzer als Anfangs-/Endadresse verlangen"; return std::nullopt; }
    if (d[12] >= 0x20 && d[12] < 0x7F) {
        a.typ = char(d[12]);
        if (a.typ == 'C') a.start = le16(d, 4);  // Startadresse nur bei selbststartenden Programmen
    }
    a.name = namensText(d.data() + 16, 16);
    a.daten.assign(d.begin() + kHeadersaveKopf, d.begin() + long(kHeadersaveKopf + soll));
    return a;
}

/// Kopf (128 B) → Abbild ohne Daten; @p laenge = Zahl der zu ladenden Bytes.
std::optional<Abbild> kcKopf(const uint8_t* k, size_t& laenge, std::string& fehler) {
    Abbild a;
    std::vector<uint8_t> h(k, k + kKcKopf);
    const unsigned n = h[16];
    if (n < 2) { fehler = "KC-Kopf: weniger als zwei Adressen"; return std::nullopt; }
    a.anfang = le16(h, 17);
    const unsigned ende = le16(h, 19);
    if (n >= 3) a.start = le16(h, 21);
    if (ende < a.anfang) { fehler = "KC-Kopf: Endadresse vor Anfangsadresse"; return std::nullopt; }
    laenge = size_t(ende) - a.anfang + 1;  // JKCEMU: bis EINSCHLIESSLICH der Endadresse (nie ein Byte zu wenig)
    std::string nm = namensText(k, 8), ty = namensText(k + 8, 3);
    a.name = ty.empty() ? nm : nm + "." + ty;
    return a;
}

std::optional<Abbild> lesenKcc(const std::vector<uint8_t>& d, std::string& fehler) {
    if (d.size() <= kKcKopf) { fehler = "KCC: Datei kuerzer als der 128-Byte-Kopf"; return std::nullopt; }
    size_t laenge = 0;
    auto a = kcKopf(d.data(), laenge, fehler);
    if (!a) return a;
    laenge = std::min(laenge, d.size() - kKcKopf);  // Endadresse + 1 (KC85) oder abgeschnittene Datei
    a->daten.assign(d.begin() + long(kKcKopf), d.begin() + long(kKcKopf + laenge));
    return a;
}

std::optional<Abbild> lesenKcTap(const std::vector<uint8_t>& d, std::string& fehler, bool* weitere) {
    if (!istTap(d)) { fehler = "keine KC-TAP-Kennung"; return std::nullopt; }
    if (d.size() < 16 + 129) { fehler = "KC-TAP: Kopfblock fehlt"; return std::nullopt; }
    size_t laenge = 0;
    auto a = kcKopf(d.data() + 17, laenge, fehler);
    if (!a) return a;
    size_t o = 16 + 129;
    std::vector<uint8_t> roh;
    bool letzter = false;
    while (!letzter && o + 129 <= d.size() && roh.size() < laenge) {
        letzter = d[o] == 0xFF;
        roh.insert(roh.end(), d.begin() + long(o + 1), d.begin() + long(o + 129));
        o += 129;
    }
    if (roh.size() < laenge && !letzter) { fehler = "KC-TAP: Datenbloecke enden vor der Endadresse"; return std::nullopt; }
    roh.resize(std::min(roh.size(), laenge));  // letzter Block ist mit Fuellbytes aufgefuellt
    a->daten = std::move(roh);
    // Multi-KC-TAP: hinter den Bloecken beginnt die naechste Aufnahme mit der Kennung.
    if (weitere) *weitere = istTap(d, o);
    return a;
}

int hexWert(uint8_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

std::optional<Abbild> lesenHex(const std::vector<uint8_t>& d, std::string& fehler) {
    std::vector<uint8_t> mem(0x10000, 0);
    unsigned lo = 0x10000, hi = 0;  // belegter Bereich [lo, hi)
    unsigned segment = 0;           // Typ 02/04: Erweiterungen werden nur akzeptiert, wenn die Adresse im 16-Bit-Raum bleibt
    size_t i = 0;
    int zeile = 0;
    bool ende = false;
    while (i < d.size() && !ende) {
        while (i < d.size() && (d[i] == '\r' || d[i] == '\n' || d[i] == ' ' || d[i] == '\t')) ++i;
        if (i >= d.size()) break;
        ++zeile;
        if (d[i] != ':') { fehler = "Intel-HEX Zeile " + std::to_string(zeile) + ": ':' fehlt"; return std::nullopt; }
        ++i;
        std::vector<uint8_t> z;
        while (i + 1 < d.size() && hexWert(d[i]) >= 0 && hexWert(d[i + 1]) >= 0) {
            z.push_back(uint8_t(hexWert(d[i]) * 16 + hexWert(d[i + 1])));
            i += 2;
        }
        if (z.size() < 5 || z.size() != size_t(z[0]) + 5) { fehler = "Intel-HEX Zeile " + std::to_string(zeile) + ": Laenge passt nicht"; return std::nullopt; }
        unsigned summe = 0;
        for (uint8_t b : z) summe += b;
        if ((summe & 0xFF) != 0) { fehler = "Intel-HEX Zeile " + std::to_string(zeile) + ": Pruefsumme falsch"; return std::nullopt; }
        const unsigned adr = unsigned(z[1] << 8 | z[2]);
        switch (z[3]) {
        case 0:
            if (segment + adr + z[0] > 0x10000) { fehler = "Intel-HEX: Adresse ueber FFFFH"; return std::nullopt; }
            for (unsigned k = 0; k < z[0]; ++k) mem[segment + adr + k] = z[4 + k];
            if (z[0]) { lo = std::min(lo, segment + adr); hi = std::max(hi, segment + adr + z[0]); }
            break;
        case 1: ende = true; break;
        case 2: segment = unsigned(z[4] << 8 | z[5]) << 4; break;
        case 4:
            if (z[4] || z[5]) { fehler = "Intel-HEX: lineare Adresse ueber 16 Bit"; return std::nullopt; }
            segment = 0;
            break;
        default: break;  // 03/05 (Startadressen) tragen fuer ein Band nichts bei
        }
    }
    if (!ende) { fehler = "Intel-HEX: Endesatz (:00000001FF) fehlt"; return std::nullopt; }
    if (hi <= lo) { fehler = "Intel-HEX: keine Daten"; return std::nullopt; }
    Abbild a;
    a.anfang = uint16_t(lo);
    a.daten.assign(mem.begin() + long(lo), mem.begin() + long(hi));
    return a;
}

}  // namespace

std::optional<Format> formatVonName(const std::string& s0) {
    std::string s;
    for (char c : s0) if (c != '.') s.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    if (s == "auto") return Format::Auto;
    if (s == "roh" || s == "raw" || s == "bin") return Format::Roh;
    if (s == "headersave" || s == "z80") return Format::Headersave;
    if (s == "kcc" || s == "kcb" || s == "kcm" || s == "jtc") return Format::Kcc;
    if (s == "kctap" || s == "tap") return Format::KcTap;
    if (s == "hex" || s == "ihx" || s == "intelhex") return Format::IntelHex;
    return std::nullopt;
}

const char* formatName(Format f) {
    switch (f) {
    case Format::Auto: return "auto";
    case Format::Roh: return "roh";
    case Format::Headersave: return "headersave";
    case Format::Kcc: return "kcc";
    case Format::KcTap: return "kctap";
    case Format::IntelHex: return "hex";
    }
    return "?";
}

Format erkenne(const std::vector<uint8_t>& d) {
    if (istTap(d)) return Format::KcTap;
    if (d.size() > kHeadersaveKopf && d[13] == 0xD3 && d[14] == 0xD3 && d[15] == 0xD3) return Format::Headersave;
    if (d.size() > 10 && d[0] == ':' && hexWert(d[1]) >= 0 && hexWert(d[2]) >= 0 && hexWert(d[3]) >= 0) {
        const std::string s(d.begin(), d.begin() + std::min<size_t>(d.size(), 16));
        if (s.find_first_not_of(":0123456789ABCDEFabcdef\r\n") == std::string::npos) return Format::IntelHex;
    }
    return Format::Roh;
}

std::optional<Abbild> lesen(const std::vector<uint8_t>& d, Format f, std::string& fehler, bool* weitere) {
    if (weitere) *weitere = false;
    if (d.empty()) { fehler = "Datei ist leer"; return std::nullopt; }
    if (f == Format::Auto) f = erkenne(d);
    switch (f) {
    case Format::Roh: {
        Abbild a;
        a.mit_adresse = false;
        a.daten = d;
        return a;
    }
    case Format::Headersave: return lesenHeadersave(d, fehler);
    case Format::Kcc: return lesenKcc(d, fehler);
    case Format::KcTap: return lesenKcTap(d, fehler, weitere);
    case Format::IntelHex: return lesenHex(d, fehler);
    case Format::Auto: break;
    }
    fehler = "unbekanntes Format";
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> schreiben(const Abbild& a, Format f, std::string& fehler, const SchreibOptionen& o) {
    if (a.daten.empty()) { fehler = "keine Daten"; return std::nullopt; }
    if (f == Format::Auto) { fehler = "Format Auto kann nicht geschrieben werden"; return std::nullopt; }
    const size_t ende = size_t(a.anfang) + a.daten.size() - 1;  // inklusiv
    if (f != Format::Roh && ende > 0xFFFF) { fehler = "Anfangsadresse + Laenge ueberschreitet FFFFH"; return std::nullopt; }
    std::vector<uint8_t> out;
    switch (f) {
    case Format::Roh: return a.daten;
    case Format::Headersave: {
        put16(out, a.anfang);
        put16(out, unsigned(ende));
        put16(out, a.start.value_or(0));
        out.resize(12, 0);
        out.push_back(uint8_t(a.typ ? a.typ : (a.start ? 'C' : 'M')));
        out.insert(out.end(), {0xD3, 0xD3, 0xD3});
        namensFeld(out, a.name, 16, ' ');
        out.insert(out.end(), a.daten.begin(), a.daten.end());
        return out;
    }
    case Format::Kcc:
    case Format::KcTap: {
        std::vector<uint8_t> k;  // 128-Byte-Kopf
        const size_t punkt = a.name.find('.');
        namensFeld(k, a.name.substr(0, punkt), 8, 0);
        namensFeld(k, punkt == std::string::npos ? "" : a.name.substr(punkt + 1), 3, 0);
        k.resize(16, 0);
        k.push_back(a.start ? 3 : 2);
        put16(k, a.anfang);
        put16(k, unsigned(ende + (o.kc_ende_exklusiv ? 1 : 0)));
        put16(k, a.start.value_or(0));
        k.resize(kKcKopf, 0);
        if (f == Format::Kcc) {
            out = k;
            out.insert(out.end(), a.daten.begin(), a.daten.end());
            return out;
        }
        out.assign(kTapKennung, kTapKennung + 16);
        out.push_back(0);  // Blocknummer des Kopfblocks
        out.insert(out.end(), k.begin(), k.end());
        uint8_t nr = 1;
        for (size_t p = 0; p < a.daten.size(); p += 128) {
            const size_t n = std::min<size_t>(128, a.daten.size() - p);
            out.push_back(p + 128 < a.daten.size() ? nr++ : 0xFF);  // letzter Block immer FFH (wie JKCEMU)
            out.insert(out.end(), a.daten.begin() + long(p), a.daten.begin() + long(p + n));
            out.insert(out.end(), 128 - n, 0);
        }
        return out;
    }
    case Format::IntelHex: {
        std::string s;
        char z[16];
        for (size_t p = 0; p < a.daten.size(); p += 32) {  // 32 Bytes je Zeile wie JKCEMU, CRLF
            const size_t n = std::min<size_t>(32, a.daten.size() - p);
            const unsigned adr = unsigned(a.anfang + p);
            unsigned summe = unsigned(n) + (adr & 0xFF) + (adr >> 8);
            std::snprintf(z, sizeof z, ":%02X%04X00", unsigned(n), adr);
            s += z;
            for (size_t i = 0; i < n; ++i) {
                std::snprintf(z, sizeof z, "%02X", a.daten[p + i]);
                s += z;
                summe += a.daten[p + i];
            }
            std::snprintf(z, sizeof z, "%02X\r\n", (0x100 - (summe & 0xFF)) & 0xFF);
            s += z;
        }
        s += ":00000001FF\r\n";
        return std::vector<uint8_t>(s.begin(), s.end());
    }
    case Format::Auto: break;
    }
    return std::nullopt;
}

}  // namespace kassette::programmdatei
