/**
 * @file td0_codec.cpp
 * @brief Implementierung von Td0Codec (TeleDisk-Abbild, Lesen und Schreiben).
 *
 * @see core/peripherals/floppy_drive/td0_codec.h
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include "core/peripherals/floppy_drive/td0_codec.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <utility>

namespace {

using SectorTrackIo::SectorRecord;
using SectorTrackIo::TrackRecord;
using Td0Codec::PseudoSector;

constexpr size_t kMaxDatei   = 64u * 1024u * 1024u;
constexpr size_t kMaxEntpackt = 256u * 1024u * 1024u;

std::string spurText(unsigned c, unsigned h) {
    return "Spur " + std::to_string(c) + "/" + std::to_string(h);
}

bool leseDatei(const std::string& path, std::vector<uint8_t>& out, std::string& err) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { err = "Kann Datei nicht oeffnen: " + path; return false; }
    const auto n = static_cast<size_t>(f.tellg());
    if (n > kMaxDatei) { err = "TeleDisk-Datei unplausibel gross: " + path; return false; }
    out.resize(n);
    f.seekg(0);
    if (n && !f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(n))) {
        err = "Lesefehler: " + path;
        return false;
    }
    return true;
}

// ─── LZHUF-Dekoder (Okumura/Yoshizaki) ──────────────────────────────────────

constexpr int kN        = 4096;            // Ringpuffer
constexpr int kF        = 60;              // Look-ahead
constexpr int kSchwelle = 2;
constexpr int kNChar    = 256 - kSchwelle + kF;   // 314 Symbole
constexpr int kT        = kNChar * 2 - 1;         // 627 Knoten
constexpr int kR        = kT - 1;                 // Wurzel
constexpr unsigned kMaxFreq = 0x8000;

/// MSB-zuerst-Bitleser; liest hinter dem Ende Nullbits und merkt sich das.
class BitLeser {
public:
    BitLeser(const uint8_t* d, size_t n) : d_(d), bits_(n * 8) {}
    unsigned bit() {
        const size_t p = pos_++;
        if (p >= bits_) return 0;
        return (d_[p >> 3] >> (7 - (p & 7))) & 1u;
    }
    unsigned byte() {
        unsigned v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 1) | bit();
        return v;
    }
    bool ueberzogen() const { return pos_ > bits_; }   // mehr Bits verlangt, als es gibt
private:
    const uint8_t* d_;
    size_t bits_;
    size_t pos_ = 0;
};

/// Praefixcode der oberen 6 Positionsbits (kanonisch aufsteigend, s. 38a 2.7), als 8-Bit-Tabelle.
struct PosTabelle {
    std::array<uint8_t, 256> code{};
    std::array<uint8_t, 256> laenge{};
    PosTabelle() {
        struct Stufe { int ab; int bis; int len; };
        static const Stufe s[] = {{0, 0, 3}, {1, 3, 4}, {4, 11, 5}, {12, 23, 6}, {24, 47, 7}, {48, 63, 8}};
        size_t i = 0;
        for (const Stufe& st : s)
            for (int v = st.ab; v <= st.bis; ++v)
                for (int k = 0; k < (1 << (8 - st.len)); ++k, ++i) {
                    code[i] = static_cast<uint8_t>(v);
                    laenge[i] = static_cast<uint8_t>(st.len);
                }
    }
};

class Huffman {
public:
    Huffman() {
        for (int i = 0; i < kNChar; ++i) {
            freq_[i] = 1;
            son_[i] = static_cast<uint16_t>(i + kT);
            prnt_[i + kT] = static_cast<uint16_t>(i);
        }
        int i = 0, j = kNChar;
        while (j <= kR) {
            freq_[j] = static_cast<uint16_t>(freq_[i] + freq_[i + 1]);
            son_[j] = static_cast<uint16_t>(i);
            prnt_[i] = prnt_[i + 1] = static_cast<uint16_t>(j);
            i += 2;
            ++j;
        }
        freq_[kT] = 0xFFFF;   // Waechter hinter der Wurzel
        prnt_[kR] = 0;
    }

    int decodeChar(BitLeser& b) {
        unsigned c = son_[kR];
        while (c < static_cast<unsigned>(kT)) c = son_[c + b.bit()];
        c -= kT;
        update(static_cast<int>(c));
        return static_cast<int>(c);
    }

private:
    void rebuild() {
        int j = 0;
        for (int i = 0; i < kT; ++i)
            if (son_[i] >= kT) {
                freq_[j] = static_cast<uint16_t>((freq_[i] + 1) / 2);
                son_[j] = son_[i];
                ++j;
            }
        for (int i = 0, jj = kNChar; jj < kT; i += 2, ++jj) {
            const int k0 = i + 1;
            const unsigned f = freq_[jj] = static_cast<uint16_t>(freq_[i] + freq_[k0]);
            int k = jj - 1;
            while (f < freq_[k]) --k;
            ++k;
            const int l = jj - k;
            std::memmove(&freq_[k + 1], &freq_[k], static_cast<size_t>(l) * sizeof(uint16_t));
            freq_[k] = static_cast<uint16_t>(f);
            std::memmove(&son_[k + 1], &son_[k], static_cast<size_t>(l) * sizeof(uint16_t));
            son_[k] = static_cast<uint16_t>(i);
        }
        for (int i = 0; i < kT; ++i) {
            const int k = son_[i];
            if (k >= kT) prnt_[k] = static_cast<uint16_t>(i);
            else         prnt_[k] = prnt_[k + 1] = static_cast<uint16_t>(i);
        }
    }

    void update(int c) {
        if (freq_[kR] == kMaxFreq) rebuild();
        c = prnt_[c + kT];
        do {
            const unsigned k = ++freq_[c];
            int l = c + 1;
            if (k > freq_[l]) {
                while (k > freq_[++l]) {}
                --l;
                freq_[c] = freq_[l];
                freq_[l] = static_cast<uint16_t>(k);
                const int i = son_[c];
                prnt_[i] = static_cast<uint16_t>(l);
                if (i < kT) prnt_[i + 1] = static_cast<uint16_t>(l);
                const int j = son_[l];
                son_[l] = static_cast<uint16_t>(i);
                prnt_[j] = static_cast<uint16_t>(c);
                if (j < kT) prnt_[j + 1] = static_cast<uint16_t>(c);
                son_[c] = static_cast<uint16_t>(j);
                c = l;
            }
        } while ((c = prnt_[c]) != 0);
    }

    uint16_t freq_[kT + 1] = {};
    uint16_t prnt_[kT + kNChar] = {};
    uint16_t son_[kT] = {};
};

// ─── Hilfen zum Lesen der Datei ─────────────────────────────────────────────

/// Zeiger mit Grenzpruefung; `ok` kippt bei jeder Ueberschreitung.
struct Leser {
    const std::vector<uint8_t>& d;
    size_t pos;
    bool ok = true;
    size_t rest() const { return d.size() - pos; }
    bool braucht(size_t n) {
        if (rest() < n) { ok = false; return false; }
        return true;
    }
};

/// Methode-0/1/2-Nutzdaten @p p (Laenge @p n) auf @p soll Bytes entpacken.
bool entpackeSektor(uint8_t methode, const uint8_t* p, size_t n, size_t soll,
                    std::vector<uint8_t>& out, bool& langesMuster, std::string& why) {
    out.clear();
    out.reserve(soll);
    switch (methode) {
        case 0:
            out.assign(p, p + n);
            break;
        case 1: {
            size_t o = 0;
            while (o < n) {
                if (n - o < 4) { why = "Methode 1: Paar abgeschnitten"; return false; }
                const size_t zaehler = p[o] | (static_cast<size_t>(p[o + 1]) << 8);
                if (out.size() + zaehler * 2 > soll) { why = "Methode 1: mehr Daten als die Sektorgroesse"; return false; }
                for (size_t k = 0; k < zaehler; ++k) { out.push_back(p[o + 2]); out.push_back(p[o + 3]); }
                o += 4;
            }
            break;
        }
        case 2: {
            size_t o = 0;
            while (o < n) {
                if (n - o < 2) { why = "Methode 2: Paar abgeschnitten"; return false; }
                const unsigned c = p[o], z = p[o + 1];
                o += 2;
                if (c == 0) {   // Literalblock von z Bytes
                    if (n - o < z) { why = "Methode 2: Literalblock abgeschnitten"; return false; }
                    if (out.size() + z > soll) { why = "Methode 2: mehr Daten als die Sektorgroesse"; return false; }
                    out.insert(out.end(), p + o, p + o + z);
                    o += z;
                } else {        // Muster aus 2*c Bytes, z-mal  [Q: Musterlaenge nur aus der Quelle]
                    const size_t mlen = static_cast<size_t>(c) * 2;
                    if (c > 1) langesMuster = true;
                    if (n - o < mlen) { why = "Methode 2: Muster abgeschnitten"; return false; }
                    if (out.size() + mlen * z > soll) { why = "Methode 2: mehr Daten als die Sektorgroesse"; return false; }
                    for (unsigned k = 0; k < z; ++k) out.insert(out.end(), p + o, p + o + mlen);
                    o += mlen;
                }
            }
            break;
        }
        default:
            why = "unbekannte Kompressionsmethode " + std::to_string(methode);
            return false;
    }
    if (out.size() != soll) {
        why = "entpackte Laenge " + std::to_string(out.size()) + " statt " + std::to_string(soll);
        return false;
    }
    return true;
}

std::string datumText(const uint8_t s[6]) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%02u.%02u.%04u %02u:%02u:%02u", s[2], s[1] + 1u, 1900u + s[0],
                  s[3], s[4], s[5]);
    return buf;
}

PseudoSector pseudoAus(uint8_t cyl, uint8_t head, size_t pos, const uint8_t* kopf,
                      const uint8_t* block, size_t blockLen) {
    PseudoSector p;
    p.cyl = cyl;
    p.head = head;
    p.pos = static_cast<uint16_t>(pos);
    std::memcpy(p.header, kopf, 6);
    if (block) p.block.assign(block, block + blockLen);
    return p;
}

}  // namespace

// ─── CRC / Erkennung ─────────────────────────────────────────────────────────

uint16_t Td0Codec::crc16(const uint8_t* d, size_t n) {
    uint16_t crc = 0;
    for (size_t i = 0; i < n; ++i) {
        crc = static_cast<uint16_t>(crc ^ (d[i] << 8));
        for (int b = 0; b < 8; ++b)
            crc = static_cast<uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0xA097 : (crc << 1));
    }
    return crc;
}

bool Td0Codec::isTd0(const uint8_t* d, size_t n) {
    if (n < 12) return false;
    if (!((d[0] == 'T' && d[1] == 'D') || (d[0] == 't' && d[1] == 'd'))) return false;
    return crc16(d, 10) == static_cast<uint16_t>(d[10] | (d[11] << 8));
}

bool Td0Codec::isTd0(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    uint8_t buf[12] = {};
    f.read(reinterpret_cast<char*>(buf), sizeof(buf));
    return isTd0(buf, static_cast<size_t>(f.gcount()));
}

// ─── Entpacken ('td') ────────────────────────────────────────────────────────

bool Td0Codec::unpack(const uint8_t* packed, size_t n, std::vector<uint8_t>& out, std::string& err) {
    static const PosTabelle pos;
    out.clear();
    out.reserve(n * 2);
    BitLeser b(packed, n);
    Huffman h;
    std::vector<uint8_t> ring(kN, 0x20);
    int r = kN - kF;
    for (;;) {
        const int c = h.decodeChar(b);
        if (b.ueberzogen()) break;   // Stromende (Padding): dieses Symbol ist keins mehr
        if (c < 256) {
            out.push_back(static_cast<uint8_t>(c));
            ring[static_cast<size_t>(r)] = static_cast<uint8_t>(c);
            r = (r + 1) & (kN - 1);
        } else {
            unsigned i = b.byte();
            const unsigned oben = pos.code[i];
            int j = pos.laenge[i] - 2;
            while (j-- > 0) i = (i << 1) + b.bit();
            if (b.ueberzogen()) break;
            const unsigned dist = (oben << 6) | (i & 0x3F);
            int von = (r - static_cast<int>(dist) - 1) & (kN - 1);
            const int len = c - 255 + kSchwelle;
            for (int k = 0; k < len; ++k) {
                const uint8_t v = ring[static_cast<size_t>((von + k) & (kN - 1))];
                out.push_back(v);
                ring[static_cast<size_t>(r)] = v;
                r = (r + 1) & (kN - 1);
            }
        }
        if (out.size() > kMaxEntpackt) {
            err = "TeleDisk: entpackter Strom unplausibel gross";
            return false;
        }
    }
    return true;
}

// ─── Laden ───────────────────────────────────────────────────────────────────

bool Td0Codec::decodeBuffer(const std::vector<uint8_t>& datei, std::vector<TrackRecord>& tracks,
                            SourceInfo* info, std::string& err) {
    SourceInfo si;
    tracks.clear();
    if (datei.empty()) { err = "TeleDisk: Datei ist leer (0 Byte)"; return false; }
    if (datei.size() < 12) {
        err = "TeleDisk: Datei zu kurz fuer den Kopf (" + std::to_string(datei.size()) + " von 12 Byte)";
        return false;
    }
    const uint8_t* h = datei.data();
    if (!((h[0] == 'T' && h[1] == 'D') || (h[0] == 't' && h[1] == 'd'))) {
        err = "Keine TeleDisk-Datei (Kennung 'TD'/'td' fehlt)";
        return false;
    }
    const uint16_t kopfCrc = static_cast<uint16_t>(h[10] | (h[11] << 8));
    if (crc16(h, 10) != kopfCrc) {
        err = "TeleDisk: Kopf-CRC stimmt nicht (gespeichert " + std::to_string(kopfCrc) + ")";
        return false;
    }
    si.advanced   = h[0] == 't';
    si.volume_seq = h[2];
    si.check_seq  = h[3];
    si.version    = h[4];
    si.fm_global  = (h[5] & 0x80) != 0;
    const unsigned rate = h[5] & 0x7F;
    if (rate > 2) { err = "TeleDisk: unbekannte Datenrate " + std::to_string(rate); return false; }
    si.rate_kbps = static_cast<uint16_t>(rate == 0 ? 250 : rate == 1 ? 300 : 500);
    si.drive_type = h[6];
    si.stepping = h[7] & 0x7F;
    si.has_comment = (h[7] & 0x80) != 0;
    si.dos_allocation = h[8] != 0;
    si.sides = h[9] == 1 ? 1 : 2;
    si.raw_header.assign(h, h + 12);
    if (si.version < 0x0A || si.version > 0x15)
        si.warnings.push_back("Teledisk-Version " + std::to_string(si.version >> 4) + "."
                              + std::to_string(si.version & 15) + " ist unueblich");
    if (si.dos_allocation)
        si.warnings.push_back("DOS-Allocation-Flag gesetzt: nur belegte Sektoren wurden gelesen");

    // Strom: bei 'td' ab Byte 12 komprimiert
    std::vector<uint8_t> entpackt;
    const std::vector<uint8_t>* dp = &datei;
    if (si.advanced) {
        std::vector<uint8_t> roh;
        if (!unpack(h + 12, datei.size() - 12, roh, err)) return false;
        entpackt.assign(h, h + 12);
        entpackt.insert(entpackt.end(), roh.begin(), roh.end());
        dp = &entpackt;
    }
    const std::vector<uint8_t>& d = *dp;
    const std::string stromName = si.advanced ? "Datei abgeschnitten (entpackter Strom endet vorzeitig)"
                                              : "Datei abgeschnitten";

    Leser in{d, 12};

    // Kommentarblock
    if (si.has_comment) {
        if (!in.braucht(10)) { err = "TeleDisk: " + stromName + " (Kommentarkopf)"; return false; }
        const uint8_t* c = &d[in.pos];
        const uint16_t crc = static_cast<uint16_t>(c[0] | (c[1] << 8));
        const size_t laenge = static_cast<size_t>(c[2] | (c[3] << 8));
        std::memcpy(si.stamp, c + 4, 6);
        if (!in.braucht(10 + laenge)) { err = "TeleDisk: " + stromName + " (Kommentartext)"; return false; }
        if (crc16(c + 2, 8 + laenge) != crc)
            si.warnings.push_back("Kommentar-CRC stimmt nicht");
        si.comment_raw.assign(c + 10, c + 10 + laenge);
        si.timestamp = datumText(si.stamp);
        // Zeilen sind NUL-getrennt; abschliessende Leerzeilen entfallen
        std::string text;
        for (uint8_t b : si.comment_raw) text.push_back(b == 0 ? '\n' : static_cast<char>(b));
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        si.comment = text;
        in.pos += 10 + laenge;
    }

    // Spursaetze bis zur Endemarke
    std::map<unsigned, size_t> lage;   // Zylinder*2+Kopf -> Index in tracks
    bool langesMuster = false;
    bool endeGesehen = false;
    while (in.pos < d.size()) {
        if (d[in.pos] == 0xFF) { endeGesehen = true; si.trailing_bytes = d.size() - in.pos; break; }
        const size_t satz = in.pos;
        if (!in.braucht(4)) { err = "TeleDisk: " + stromName + " (Spurkopf, Offset " + std::to_string(satz) + ")"; return false; }
        const uint8_t nsec = d[in.pos], cyl = d[in.pos + 1], hb = d[in.pos + 2], crc8 = d[in.pos + 3];
        in.pos += 4;
        const unsigned head = hb & 0x7F;
        const std::string wo = spurText(cyl, head) + " (Offset " + std::to_string(satz) + ")";
        if ((crc16(&d[satz], 3) & 0xFF) != crc8) {
            err = "TeleDisk: " + wo + ": Spur-CRC stimmt nicht";
            return false;
        }
        if (head > 1) {
            err = "TeleDisk: " + wo + ": ungueltiger Kopf " + std::to_string(head);
            return false;
        }
        TrackRecord tr;
        tr.cyl = cyl;
        tr.head = static_cast<uint8_t>(head);
        const bool spurFm = (hb & 0x80) != 0;
        tr.enc = (spurFm || si.fm_global) ? Encoding::FM : Encoding::MFM;
        tr.rate_kbps = si.rate_kbps;

        std::set<uint8_t> gesehen;
        std::vector<PseudoSector> pseudoSpur;
        for (unsigned i = 0; i < nsec; ++i) {
            if (!in.braucht(6)) { err = "TeleDisk: " + wo + ": " + stromName + " (Sektorkopf " + std::to_string(i) + ")"; return false; }
            const uint8_t sc = d[in.pos], sh = d[in.pos + 1], sr = d[in.pos + 2], sn = d[in.pos + 3],
                          fl = d[in.pos + 4], scrc = d[in.pos + 5];
            const size_t kopfPos = in.pos;
            in.pos += 6;
            const bool hatBlock = (fl & 0x30) == 0;
            std::vector<uint8_t> nutz;   // entpackte Daten
            if (hatBlock) {
                if (!in.braucht(3)) { err = "TeleDisk: " + wo + ": " + stromName + " (Datenblock)"; return false; }
                const size_t dlen = static_cast<size_t>(d[in.pos] | (d[in.pos + 1] << 8));
                if (dlen == 0) { err = "TeleDisk: " + wo + ": Datenblock der Laenge 0 (Sektor " + std::to_string(sr) + ")"; return false; }
                if (!in.braucht(2 + dlen)) { err = "TeleDisk: " + wo + ": " + stromName + " (Daten von Sektor " + std::to_string(sr) + ")"; return false; }
                const uint8_t methode = d[in.pos + 2];
                const uint8_t* np = &d[in.pos + 3];
                const size_t blockPos = in.pos;
                in.pos += 2 + dlen;
                if (fl & 0x40) {   // Pseudosektor: Daten ohne ID-Feld, nicht ins Medium, nur roh gemerkt
                    ++si.pseudo_sectors;
                    pseudoSpur.push_back(pseudoAus(cyl, static_cast<uint8_t>(head), tr.sectors.size(),
                                                   &d[kopfPos], &d[blockPos], 2 + dlen));
                    continue;
                }
                if (sn > 6) {
                    err = "TeleDisk: " + wo + ": ungueltiger Sektorgroessen-Code " + std::to_string(sn)
                          + " (Sektor " + std::to_string(sr) + ")";
                    return false;
                }
                std::string why;
                if (!entpackeSektor(methode, np, dlen - 1, 128u << sn, nutz, langesMuster, why)) {
                    err = "TeleDisk: " + wo + ": Sektor " + std::to_string(sr) + ": " + why;
                    return false;
                }
                if ((crc16(nutz.data(), nutz.size()) & 0xFF) != scrc) ++si.crc8_mismatch;
            } else {
                if (fl & 0x40) {
                    ++si.pseudo_sectors;
                    pseudoSpur.push_back(pseudoAus(cyl, static_cast<uint8_t>(head), tr.sectors.size(),
                                                   &d[kopfPos], nullptr, 0));
                    continue;
                }
                if (fl & 0x10) si.warnings.push_back(wo + ": Sektor " + std::to_string(sr)
                                                     + " von der DOS-Allocation uebersprungen (keine Daten)");
                if (sn > 6) {   // kein Datenfeld und keine darstellbare Groesse: nicht abbildbar
                    si.warnings.push_back(wo + ": Sektor " + std::to_string(sr)
                                          + " ohne Daten mit Groessen-Code " + std::to_string(sn) + " ausgelassen");
                    continue;
                }
            }
            SectorRecord s;
            s.c = sc; s.h = sh; s.r = sr; s.n = sn;
            if (!gesehen.insert(sr).second) ++si.duplicate_ids;
            if (!hatBlock) {
                s.no_data = true;
                ++si.no_data_sectors;
            } else {
                s.data = std::move(nutz);
                s.data_error = (fl & 0x02) != 0;
                s.deleted    = (fl & 0x04) != 0;
                if (s.data_error) ++si.data_error_sectors;
                if (s.deleted)    ++si.deleted_sectors;
            }
            tr.sectors.push_back(std::move(s));
        }

        for (PseudoSector& p : pseudoSpur) p.real = static_cast<uint16_t>(tr.sectors.size());
        const unsigned key = static_cast<unsigned>(cyl) * 2u + head;
        const auto f = lage.find(key);
        if (f != lage.end()) {
            si.pseudo.erase(std::remove_if(si.pseudo.begin(), si.pseudo.end(),
                                           [&](const PseudoSector& p) { return p.cyl == cyl && p.head == head; }),
                            si.pseudo.end());
            si.warnings.push_back(spurText(cyl, head) + " kommt mehrfach vor, der letzte Satz gilt");
            tracks[f->second] = std::move(tr);
            si.heads[f->second] = {cyl, static_cast<uint8_t>(head), spurFm};
        } else {
            lage[key] = tracks.size();
            tracks.push_back(std::move(tr));
            si.heads.push_back({cyl, static_cast<uint8_t>(head), spurFm});
        }
        for (PseudoSector& p : pseudoSpur) si.pseudo.push_back(std::move(p));
    }
    if (!endeGesehen) { err = "TeleDisk: " + stromName + " (Endemarke fehlt)"; return false; }

    for (const TrackRecord& t : tracks) {
        ++si.tracks;
        si.sectors += t.sectors.size();
        if (t.sectors.empty()) ++si.empty_tracks;
    }
    if (tracks.empty()) { err = "TeleDisk: die Datei enthaelt keine Spuren"; return false; }
    if (si.sectors == 0) si.warnings.push_back("Die Diskette enthaelt keine Sektoren (leer)");
    if (si.pseudo_sectors)
        si.warnings.push_back(std::to_string(si.pseudo_sectors)
                              + " Pseudosektor(en) (Daten ohne ID-Feld) nicht uebernommen");
    if (si.crc8_mismatch)
        si.warnings.push_back(std::to_string(si.crc8_mismatch)
                              + " Sektor(en) mit abweichender Sektor-CRC8 (Daten trotzdem uebernommen)");
    if (langesMuster)
        si.warnings.push_back("Methode 2 mit Musterlaenge 2*c fuer c > 1 (nicht an Echtdaten belegt)");
    if (info) *info = std::move(si);
    return true;
}

bool Td0Codec::loadBuffer(const std::vector<uint8_t>& datei, DiskMedium& out, SourceInfo* info,
                          std::string& err) {
    std::vector<TrackRecord> tracks;
    SourceInfo si;
    if (!decodeBuffer(datei, tracks, &si, err)) return false;
    if (!SectorTrackIo::toMedium(tracks, out, err)) {
        err = "TeleDisk: " + err;
        return false;
    }
    if (info) *info = std::move(si);
    return true;
}

bool Td0Codec::load(const std::string& path, DiskMedium& out, SourceInfo* info, std::string& err) {
    std::vector<uint8_t> d;
    if (!leseDatei(path, d, err)) return false;
    if (!loadBuffer(d, out, info, err)) {
        err += " (" + path + ")";
        return false;
    }
    return true;
}

std::string Td0Codec::infoField(const SourceInfo& si, const std::string& key) {
    if (key == "version")   return std::to_string(si.version >> 4) + "." + std::to_string(si.version & 15);
    if (key == "timestamp") return si.timestamp;
    if (key == "comment")   return si.comment;
    if (key == "rate")      return std::to_string(si.rate_kbps);
    if (key == "tracks")    return std::to_string(si.tracks);
    if (key == "sectors")   return std::to_string(si.sectors);
    if (key == "warnings") {
        std::string s;
        for (const auto& w : si.warnings) s += (s.empty() ? "" : "\n") + w;
        return s;
    }
    return "";
}

// ─── Schreiben (AP T2) ───────────────────────────────────────────────────────

namespace {

/// Sektorliste vergleichen (Gegenprobe); ohne Datenfeld zaehlen nur die ID-Felder.
bool gleich(const SectorRecord& a, const SectorRecord& b) {
    if (a.c != b.c || a.h != b.h || a.r != b.r || a.n != b.n || a.no_data != b.no_data) return false;
    if (a.no_data) return true;
    return a.deleted == b.deleted && a.data_error == b.data_error && a.data == b.data;
}

bool gleich(const std::vector<TrackRecord>& a, const std::vector<TrackRecord>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].cyl != b[i].cyl || a[i].head != b[i].head || a[i].enc != b[i].enc
            || a[i].rate_kbps != b[i].rate_kbps || a[i].sectors.size() != b[i].sectors.size())
            return false;
        for (size_t k = 0; k < a[i].sectors.size(); ++k)
            if (!gleich(a[i].sectors[k], b[i].sectors[k])) return false;
    }
    return true;
}

void stempelJetzt(uint8_t st[6]) {
    const std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    st[0] = static_cast<uint8_t>(tmv.tm_year);   // Jahr - 1900
    st[1] = static_cast<uint8_t>(tmv.tm_mon);    // 0-basiert
    st[2] = static_cast<uint8_t>(tmv.tm_mday);
    st[3] = static_cast<uint8_t>(tmv.tm_hour);
    st[4] = static_cast<uint8_t>(tmv.tm_min);
    st[5] = static_cast<uint8_t>(tmv.tm_sec);
}

void put16(std::vector<uint8_t>& f, unsigned v) {
    f.push_back(static_cast<uint8_t>(v & 0xFF));
    f.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

/// Datenblock eines Sektors: Methode 1, wenn die Daten ein 2-Byte-Muster wiederholen
/// (auch ein Einzelbyte), sonst Methode 0.  Methode 2 schreiben wir nie.
void datenBlock(std::vector<uint8_t>& f, const std::vector<uint8_t>& d) {
    bool muster = d.size() >= 2 && d.size() % 2 == 0;
    for (size_t i = 2; muster && i < d.size(); ++i) muster = d[i] == d[i - 2];
    if (muster) {
        put16(f, 5);   // Methodenbyte + Zaehler (2) + Muster (2)
        f.push_back(1);
        put16(f, static_cast<unsigned>(d.size() / 2));
        f.push_back(d[0]);
        f.push_back(d[1]);
    } else {
        put16(f, static_cast<unsigned>(d.size() + 1));
        f.push_back(0);
        f.insert(f.end(), d.begin(), d.end());
    }
}

/// Laufwerkstyp (Kopfbyte 6, nur informativ) aus Rate, Verfahren und Geometrie raten.
uint8_t laufwerkstypRaten(uint16_t rate, bool fm, unsigned zylinder, size_t maxSektoren) {
    if (rate >= 500) {
        if (fm) return 5;                       // 8"
        return maxSektoren >= 18 ? 4 : 2;       // 1,44M | 1,2M
    }
    return zylinder <= 45 ? 1 : 3;              // 360K | 720K
}

}  // namespace

bool Td0Codec::encode(const DiskMedium& in, const SourceInfo* vorlage, std::vector<uint8_t>& out,
                      SourceInfo* geschrieben, std::string& err) {
    std::vector<TrackRecord> formatiert;
    if (!SectorTrackIo::fromMedium(in, formatiert, err)) {
        err = "TeleDisk: " + err;
        return false;
    }
    std::vector<std::string> warnungen;

    // Vorlage nur bei gueltigem Kopf UND unveraenderter Geometrie
    const SourceInfo* v = nullptr;
    if (vorlage && vorlage->raw_header.size() == 12
        && isTd0(vorlage->raw_header.data(), vorlage->raw_header.size())) {
        unsigned vc = 0, vh = 0;
        for (const TrackHead& t : vorlage->heads) {
            vc = std::max<unsigned>(vc, t.cyl + 1u);
            vh = std::max<unsigned>(vh, t.head + 1u);
        }
        if (vc == in.numCylinders() && vh == in.numHeads()) v = vorlage;
        else warnungen.push_back("Kopfdaten der Vorlage verworfen: die Geometrie hat sich geaendert");
    }

    // Pruefung der belegten Spuren
    size_t maxSektoren = 0;
    for (const TrackRecord& tr : formatiert) {
        if (tr.sectors.size() > 254) {   // 0xFF waere die Endemarke
            err = "TeleDisk: " + spurText(tr.cyl, tr.head) + " hat mehr als 254 Sektoren";
            return false;
        }
        maxSektoren = std::max(maxSektoren, tr.sectors.size());
        for (const SectorRecord& s : tr.sectors) {
            if (s.id_error) {
                err = "TeleDisk kann keinen ID-CRC-Fehler darstellen (" + spurText(tr.cyl, tr.head)
                      + ", Sektor " + std::to_string(s.r) + "). Bitte .hfe oder .dmk verwenden.";
                return false;
            }
            if (s.n > 6) {
                err = "TeleDisk: " + spurText(tr.cyl, tr.head) + ": Sektorgroessen-Code "
                      + std::to_string(s.n) + " nicht darstellbar";
                return false;
            }
        }
    }

    // Eine Datenrate je Datei
    uint16_t rate = v ? v->rate_kbps : formatiert.front().rate_kbps;
    if (rate != 250 && rate != 300 && rate != 500) rate = 250;
    if (!v) {
        for (const TrackRecord& tr : formatiert)
            if (tr.rate_kbps != rate) {
                warnungen.push_back("Die Spuren haben verschiedene Datenraten, TeleDisk fuehrt nur eine: "
                                    + std::to_string(rate) + " kbit/s (erste belegte Spur) gilt");
                break;
            }
    }
    bool alleFm = true;
    for (const TrackRecord& tr : formatiert) alleFm = alleFm && tr.enc == Encoding::FM;
    const bool globalFm = alleFm && (!v || v->fm_global);
    const Encoding standard = formatiert.front().enc;

    std::map<unsigned, size_t> vorhanden;
    for (size_t i = 0; i < formatiert.size(); ++i)
        vorhanden[formatiert[i].cyl * 2u + formatiert[i].head] = i;
    std::map<unsigned, const TrackHead*> vKoepfe;
    if (v) for (const TrackHead& t : v->heads) vKoepfe[t.cyl * 2u + t.head] = &t;

    // Pseudosektoren der Vorlage je Spur, soweit die Spur noch dazu passt
    std::map<unsigned, std::vector<const PseudoSector*>> pseudoJe;
    size_t pseudoVerloren = 0, pseudoGeschrieben = 0;
    if (v) {
        for (const PseudoSector& p : v->pseudo) {
            const unsigned key = p.cyl * 2u + p.head;
            const auto z = vorhanden.find(key);
            const size_t echte = z == vorhanden.end() ? 0 : formatiert[z->second].sectors.size();
            if (p.real == echte && p.pos <= echte && (echte + 1) <= 254) pseudoJe[key].push_back(&p);
            else ++pseudoVerloren;
        }
    } else if (vorlage) {
        pseudoVerloren = vorlage->pseudo.size();
    }
    if (pseudoVerloren)
        warnungen.push_back(std::to_string(pseudoVerloren) + " Pseudosektor(en) (Flag 0x40) nicht zurueckgeschrieben"
                            + (v ? " (Spur hat sich geaendert)" : " (keine passende Vorlage)"));

    // Vollstaendige Soll-Liste (auch leere Spuren) samt Spurbit
    struct Spur { TrackRecord tr; bool fmBit; };
    std::vector<Spur> soll;
    for (uint8_t c = 0; c < in.numCylinders(); ++c)
        for (uint8_t h = 0; h < in.numHeads(); ++h) {
            const unsigned key = c * 2u + h;
            const auto th = vKoepfe.find(key);
            const TrackHead* kopfV = th == vKoepfe.end() ? nullptr : th->second;
            const auto vi = vorhanden.find(key);
            Spur sp;
            bool wollFm;
            if (vi == vorhanden.end()) {
                sp.tr.cyl = c; sp.tr.head = h;
                wollFm = kopfV ? (kopfV->fm || v->fm_global) : standard == Encoding::FM;
            } else {
                sp.tr = std::move(formatiert[vi->second]);
                wollFm = sp.tr.enc == Encoding::FM;
            }
            bool bit = wollFm;
            if (bit && globalFm && kopfV && !kopfV->fm) bit = false;   // wie in der Quelle: nur das globale Bit
            sp.fmBit = bit;
            sp.tr.enc = (bit || globalFm) ? Encoding::FM : Encoding::MFM;
            sp.tr.rate_kbps = rate;
            soll.push_back(std::move(sp));
        }

    // Datei aufbauen: Kopf
    std::vector<uint8_t> f;
    uint8_t kopf[12] = {'T', 'D', 0, 0, 0x15, 0, 0, 0, 0, 2, 0, 0};
    bool kommentarDa = true;
    if (v) {
        std::memcpy(kopf, v->raw_header.data(), 10);
        kopf[0] = 'T'; kopf[1] = 'D';          // td wird als TD zurueckgeschrieben
        kommentarDa = (kopf[7] & 0x80) != 0;
    } else {
        kopf[6] = laufwerkstypRaten(rate, alleFm, in.numCylinders(), maxSektoren);
        kopf[7] = 0x80;
        kopf[9] = in.numHeads() == 1 ? 1 : 2;
    }
    kopf[5] = static_cast<uint8_t>((rate == 250 ? 0 : rate == 300 ? 1 : 2) | (globalFm ? 0x80 : 0));
    const uint16_t kcrc = crc16(kopf, 10);
    kopf[10] = static_cast<uint8_t>(kcrc & 0xFF);
    kopf[11] = static_cast<uint8_t>(kcrc >> 8);
    f.assign(kopf, kopf + 12);

    // Kommentarblock
    if (kommentarDa) {
        std::vector<uint8_t> k;   // ab Byte 2 des Blocks: Laenge, Zeitstempel, Text
        std::vector<uint8_t> text;
        uint8_t stamp[6];
        if (v) {
            text = v->comment_raw;
            std::memcpy(stamp, v->stamp, 6);
        } else {
            static const char kText[] = "Image file created by k1520emu";
            text.assign(kText, kText + sizeof(kText));   // inkl. abschliessendem NUL
            stempelJetzt(stamp);
        }
        put16(k, static_cast<unsigned>(text.size()));
        k.insert(k.end(), stamp, stamp + 6);
        k.insert(k.end(), text.begin(), text.end());
        put16(f, crc16(k.data(), k.size()));
        f.insert(f.end(), k.begin(), k.end());
    }

    // Spursaetze
    uint8_t letzteKopie[4] = {0xFF, 0, 0, 0};
    for (const Spur& sp : soll) {
        const TrackRecord& tr = sp.tr;
        const unsigned key = tr.cyl * 2u + tr.head;
        const auto pj = pseudoJe.find(key);
        const size_t nPseudo = pj == pseudoJe.end() ? 0 : pj->second.size();
        const uint8_t hb = static_cast<uint8_t>(tr.head | (sp.fmBit ? 0x80 : 0));
        uint8_t th[4] = {static_cast<uint8_t>(tr.sectors.size() + nPseudo), tr.cyl, hb, 0};
        th[3] = static_cast<uint8_t>(crc16(th, 3) & 0xFF);
        f.insert(f.end(), th, th + 4);
        std::memcpy(letzteKopie, th, 4);

        size_t nextP = 0;
        auto pseudosBis = [&](size_t pos) {
            while (nPseudo && nextP < nPseudo && pj->second[nextP]->pos <= pos) {
                const PseudoSector& p = *pj->second[nextP++];
                f.insert(f.end(), p.header, p.header + 6);
                f.insert(f.end(), p.block.begin(), p.block.end());
                ++pseudoGeschrieben;
            }
        };
        for (size_t i = 0; i < tr.sectors.size(); ++i) {
            pseudosBis(i);
            const SectorRecord& s = tr.sectors[i];
            uint8_t fl = 0;
            if (s.no_data) fl = 0x20;
            else {
                if (s.data_error) fl |= 0x02;
                if (s.deleted)    fl |= 0x04;
            }
            const uint8_t crc8 = s.no_data ? 0 : static_cast<uint8_t>(crc16(s.data.data(), s.data.size()) & 0xFF);
            const uint8_t sk[6] = {s.c, s.h, s.r, s.n, fl, crc8};
            f.insert(f.end(), sk, sk + 6);
            if (!s.no_data) datenBlock(f, s.data);
        }
        pseudosBis(tr.sectors.size());
    }
    // Endemarke: Kopie des letzten Spurkopfes mit 0xFF
    f.push_back(0xFF);
    f.insert(f.end(), letzteKopie + 1, letzteKopie + 4);

    // Gegenprobe: was wir geschrieben haben, muss der Leser unveraendert zurueckgeben.
    std::vector<TrackRecord> zurueck, sollListe;
    for (const Spur& sp : soll) sollListe.push_back(sp.tr);
    SourceInfo si;
    std::string e2;
    if (!decodeBuffer(f, zurueck, &si, e2) || !gleich(sollListe, zurueck)
        || si.pseudo_sectors != pseudoGeschrieben) {
        err = "TeleDisk: interner Fehler beim Kodieren (Gegenprobe fehlgeschlagen"
              + (e2.empty() ? std::string() : ": " + e2) + ")";
        return false;
    }
    for (const std::string& w : warnungen) si.warnings.push_back(w);
    out = std::move(f);
    if (geschrieben) *geschrieben = std::move(si);
    return true;
}

bool Td0Codec::save(const std::string& path, const DiskMedium& in, const SourceInfo* vorlage,
                    SourceInfo* geschrieben, std::string& err) {
    std::vector<uint8_t> datei;
    if (!encode(in, vorlage, datei, geschrieben, err)) return false;   // Zieldatei unberuehrt

    // Erst in eine Nachbardatei, dann umbenennen: nie ein halb geschriebenes Abbild.
    const std::string tmp = path + ".k1520tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) { err = "Kann Datei nicht schreiben: " + tmp; return false; }
        f.write(reinterpret_cast<const char*>(datei.data()), static_cast<std::streamsize>(datei.size()));
        f.flush();
        if (!f) {
            f.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            err = "Schreibfehler (unvollstaendig): " + path;
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::error_code ec2;
        std::filesystem::remove(tmp, ec2);
        err = "Kann Datei nicht ersetzen: " + path + " (" + ec.message() + ")";
        return false;
    }
    return true;
}

bool Td0Codec::save(const std::string& path, const DiskMedium& in, std::string& err) {
    return save(path, in, nullptr, nullptr, err);
}
