/**
 * @file tar_fs.cpp
 * @brief Implementierung des schreibgeschuetzten Tar-Archivs auf der Diskette.
 *
 * @see core/filesystem/tar/tar_fs.h
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include "core/filesystem/tar/tar_fs.h"
#include "core/filesystem/tar/tar_archiv.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>

namespace tar {

bool nullblock(const uint8_t* b) { return alleNull(b, kBlock); }

bool alleNull(const uint8_t* d, size_t n) {
    for (size_t i = 0; i < n; ++i) if (d[i]) return false;
    return true;
}

namespace {

/// Oktalfeld: fuehrende Leerzeichen/Nullen, Ende bei NUL oder Leerzeichen.
bool oktal(const uint8_t* f, size_t n, uint64_t& out) {
    size_t i = 0;
    while (i < n && f[i] == ' ') ++i;
    uint64_t v = 0;
    bool da = false;
    for (; i < n && f[i] >= '0' && f[i] <= '7'; ++i) { v = v * 8 + (f[i] - '0'); da = true; }
    for (; i < n; ++i) if (f[i] != 0 && f[i] != ' ') return false;
    out = v;
    return da;
}

uint64_t oktalOder0(const uint8_t* f, size_t n) {
    uint64_t v = 0;
    return oktal(f, n, v) ? v : 0;
}

std::string text(const uint8_t* f, size_t n) {
    size_t l = 0;
    while (l < n && f[l]) ++l;
    return std::string(reinterpret_cast<const char*>(f), l);
}

bool druckbar(const std::string& s) {
    for (unsigned char c : s) if (c < 0x20 || c == 0x7F) return false;
    return true;
}

} // namespace

bool kopfLesen(const uint8_t* b, Kopf& k, std::string* warum) {
    auto nein = [&](const char* w) { if (warum) *warum = w; return false; };
    if (nullblock(b)) return nein("Nullblock");
    uint64_t soll = 0;
    if (!oktal(b + 148, 8, soll)) return nein("Pruefsummenfeld ist keine Oktalzahl");
    uint32_t su = 256, ss = 256;     // Feld zaehlt als 8 Leerzeichen; vorzeichenlose / vorzeichenbehaftete Summe
    for (uint32_t i = 0; i < kBlock; ++i) {
        if (i >= 148 && i < 156) continue;
        su += b[i];
        ss += static_cast<uint32_t>(static_cast<int8_t>(b[i]));   // alte tar auf char-signed Maschinen
    }
    if (soll != su && soll != ss) return nein("Kopfpruefsumme stimmt nicht");

    Kopf t;
    t.name = text(b, 100);
    if (std::memcmp(b + 257, "ustar", 5) == 0) {
        const std::string pre = text(b + 345, 155);
        if (!pre.empty()) t.name = pre + "/" + t.name;
    }
    if (t.name.empty() || !druckbar(t.name)) return nein("Name leer oder nicht druckbar");

    if (b[124] & 0x80) {             // GNU: Groesse binaer (base-256)
        uint64_t v = b[124] & 0x7F;
        for (int i = 125; i < 136; ++i) v = (v << 8) | b[i];
        t.size = v;
    } else if (!oktal(b + 124, 12, t.size)) {
        return nein("Groessenfeld ist keine Oktalzahl");
    }
    if (t.size > (1ULL << 34)) return nein("Groesse unplausibel");

    t.typ = b[156] ? static_cast<char>(b[156]) : '0';
    if (!((t.typ >= '0' && t.typ <= '7') || t.typ == 'x' || t.typ == 'g' || t.typ == 'L' || t.typ == 'K'))
        return nein("unbekannter Typ");
    t.mode  = static_cast<uint32_t>(oktalOder0(b + 100, 8));
    t.uid   = static_cast<uint32_t>(oktalOder0(b + 108, 8));
    t.gid   = static_cast<uint32_t>(oktalOder0(b + 116, 8));
    t.mtime = static_cast<uint32_t>(oktalOder0(b + 136, 12));
    t.linkname = text(b + 157, 100);
    k = t;
    return true;
}

namespace {

/// Hat der Eintrag nach seinem Kopf Datenbloecke?  (Wie in TarFileSystem::scanAb.)
bool mitDaten(char typ) {
    return typ == '0' || typ == '7' || typ == 'L' || typ == 'K' || typ == 'x' || typ == 'g';
}

} // namespace

uint64_t exportLaenge(const uint8_t* strom, uint64_t n, bool* endmarke) {
    if (endmarke) *endmarke = false;
    Kopf k;
    if (n < kBlock || !kopfLesen(strom, k)) return n;       // Folgeteil: alles
    uint64_t p = 0;
    for (;;) {
        if (p + kBlock > n) return n;                        // Strom endet ohne Endmarke
        if (nullblock(strom + p)) {
            if (p + 2 * kBlock <= n && nullblock(strom + p + kBlock)) {
                if (endmarke) *endmarke = true;
                return p + 2 * kBlock;
            }
            return n;                                        // ein einzelner Nullblock: Struktur unklar
        }
        if (!kopfLesen(strom + p, k)) return n;              // kein Kopf an der Kopfposition
        const uint64_t daten = mitDaten(k.typ) ? k.size : 0;
        const uint64_t next = p + kBlock + (daten + kBlock - 1) / kBlock * kBlock;
        if (next > n) return n;                              // Datei ragt ueber das Stromende
        p = next;
    }
}

QuellBefund quelleBeurteilen(const std::vector<uint8_t>& d, uint64_t kapazitaet) {
    QuellBefund b;
    if (d.empty()) { b.fehler = "Die Datei ist leer."; return b; }
    if (d.size() % kBlock) {
        b.fehler = "Die Dateigroesse (" + std::to_string(d.size()) + " Byte) ist kein Vielfaches von "
                   "512 — ein tar-Strom besteht aus 512-Byte-Bloecken.  Mit `tar` erzeugte "
                   "Archive und Teile von `split -b 737280` sind es immer; wurde die Datei "
                   "anders geteilt, fehlt Auffuellung bis zum Blockrand.";
        return b;
    }
    if (d.size() > kapazitaet) {
        b.fehler = "Die Datei ist " + std::to_string(d.size()) + " Byte gross, die Diskette fasst "
                   + std::to_string(kapazitaet) + " Byte (720 KiB).  Ein groesseres Archiv laesst sich "
                   "mit `split -b " + std::to_string(kapazitaet) + " archiv.tar teil_` auf mehrere "
                   "Disketten verteilen; jedes Teil wird einzeln geschrieben.";
        return b;
    }
    Kopf k;
    b.mit_kopf = kopfLesen(d.data(), k);
    if (!b.mit_kopf) return b;                               // Folgeteil: nichts zu sehen

    std::vector<std::string> arten;
    auto merke = [&](const std::string& a) {
        if (std::find(arten.begin(), arten.end(), a) == arten.end()) arten.push_back(a);
    };
    uint64_t p = 0;
    while (p + kBlock <= d.size()) {
        const uint8_t* h = d.data() + p;
        if (nullblock(h) || !kopfLesen(h, k)) break;
        if (std::memcmp(h + 257, "ustar", 5) == 0)
            merke(std::memcmp(h + 257, "ustar  ", 7) == 0 ? "GNU-tar-Kopf (Magic \"ustar  \")"
                                                         : "POSIX-ustar-Kopf (Magic \"ustar\")");
        if (k.typ == 'x' || k.typ == 'g') merke("pax-Erweiterungskopf (Typ x/g)");
        if (k.typ == 'L' || k.typ == 'K') merke("GNU-Langname (Typ L/K)");
        if (k.typ == '3' || k.typ == '4' || k.typ == '6' || k.typ == '7')
            merke(std::string("Sondertyp '") + k.typ + "' (Geraet/FIFO/zusammenhaengend)");
        if (h[99] != 0) merke("Name mit 100 Zeichen oder laenger");
        if (h[124] & 0x80) merke("binaeres Groessenfeld (GNU)");
        p += kBlock + ((mitDaten(k.typ) ? k.size : 0) + kBlock - 1) / kBlock * kBlock;
    }
    if (!arten.empty()) {
        std::string w = "Das Archiv ist kein reines V7-tar (wie es der P8000-/WEGA-tar kennt): ";
        for (size_t i = 0; i < arten.size(); ++i) w += (i ? "; " : "") + arten[i];
        w += ".  Das Zielsystem kann es womoeglich nicht lesen.  Mit `tar --format=v7` erzeugt "
             "GNU tar ein V7-Archiv (Namen bis 99 Zeichen).";
        b.warnung = w;
    }
    return b;
}

} // namespace tar

using tar::kBlock;

bool TarSpaceQuelle::ort(uint64_t off, FsRecoverOrt& out) const {
    for (size_t i = 0; i < s_.trackCount(); ++i) {
        const SectorSpace::TrackRef t = s_.trackAt(i);
        if (off >= t.start && off < t.start + t.bytes && t.sector_size) {
            out.cyl    = t.cyl;
            out.head   = t.head;
            out.sector = t.first_id + static_cast<int>((off - t.start) / t.sector_size);
            return true;
        }
    }
    return false;
}

bool TarSpeicherQuelle::read(uint64_t off, uint8_t* dst, size_t n) const {
    if (off + n > d_.size()) return false;
    std::memcpy(dst, d_.data() + off, n);
    return true;
}

namespace {

/// Pfad bereinigen (gemeinsame Regel mit dem Archivmodell, siehe tar_archiv.h).
std::string bereinige(const std::string& roh, bool& geaendert) {
    return tar::bereinigePfad(roh, geaendert);
}

std::string datumText(uint32_t t) {
    if (t == 0) return {};
    const int64_t tage = t / 86400, rest = t % 86400;
    int64_t z = tage + 719468;
    const int64_t era = z / 146097, doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    const int64_t d = doy - (153 * mp + 2) / 5 + 1;
    const int64_t m = mp + (mp < 10 ? 3 : -9);
    if (m <= 2) ++y;
    char b[32];
    std::snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d", static_cast<int>(y), static_cast<int>(m),
                  static_cast<int>(d), static_cast<int>(rest / 3600), static_cast<int>(rest % 3600 / 60));
    return b;
}

std::string modeText(char art, uint32_t mode) {
    std::string s(1, art);
    static const char* rwx = "rwxrwxrwx";
    for (int i = 0; i < 9; ++i) s += (mode & (0400u >> i)) ? rwx[i] : '-';
    if (mode & 04000) s[3] = (mode & 0100) ? 's' : 'S';
    if (mode & 02000) s[6] = (mode & 0010) ? 's' : 'S';
    if (mode & 01000) s[9] = (mode & 0001) ? 't' : 'T';
    return s;
}

/// Art-Buchstabe und Dateiart-Bits (S_IF*) zum tar-Typ.
void art(char typ, char& buchstabe, uint16_t& bits) {
    switch (typ) {
        case '5': buchstabe = 'd'; bits = 0040000; break;
        case '2': buchstabe = 'l'; bits = 0120000; break;
        case '3': buchstabe = 'c'; bits = 0020000; break;
        case '4': buchstabe = 'b'; bits = 0060000; break;
        case '6': buchstabe = 'p'; bits = 0010000; break;
        default:  buchstabe = '-'; bits = 0100000; break;
    }
}

} // namespace

TarFileSystem::Scan TarFileSystem::scanAb(const TarQuelle& q, uint64_t start) {
    Scan s;
    uint64_t p = start;
    const uint64_t gesamt = q.size();
    std::string lang_name, lang_link;
    uint8_t b[kBlock];
    for (;;) {
        if (p + kBlock > gesamt) { s.ende = Ende::Streamende; s.ende_off = p; break; }
        if (!q.read(p, b, kBlock)) { s.ende = Ende::Streamende; s.ende_off = p; break; }
        if (tar::nullblock(b)) { s.ende = Ende::Nullblock; s.ende_off = p; break; }
        tar::Kopf k;
        if (!tar::kopfLesen(b, k)) {
            if (p == start) return s;          // schon der erste Kopf taugt nicht
            s.ende = Ende::Pruefsumme; s.ende_off = p; break;
        }
        s.ok = true;
        const uint64_t daten = p + kBlock;
        const bool mit_daten = k.typ == '0' || k.typ == '7' || k.typ == 'L' || k.typ == 'K'
                            || k.typ == 'x' || k.typ == 'g';
        const uint64_t soll = mit_daten ? k.size : 0;
        const uint64_t ist  = std::min<uint64_t>(soll, gesamt > daten ? gesamt - daten : 0);
        const uint64_t next = daten + (soll + kBlock - 1) / kBlock * kBlock;

        if (k.typ == 'L' || k.typ == 'K') {
            std::vector<uint8_t> d(static_cast<size_t>(std::min<uint64_t>(ist, 4096)));
            if (!d.empty()) q.read(daten, d.data(), d.size());
            std::string n(d.begin(), d.end());
            n.erase(std::find(n.begin(), n.end(), '\0'), n.end());
            (k.typ == 'L' ? lang_name : lang_link) = n;
        } else if (k.typ != 'x' && k.typ != 'g') {
            Eintrag e;
            e.k = k;
            if (!lang_name.empty()) e.k.name = lang_name;
            if (!lang_link.empty()) e.k.linkname = lang_link;
            lang_name.clear(); lang_link.clear();
            if (e.k.typ == '0' || e.k.typ == '7') {
                if (!e.k.name.empty() && e.k.name.back() == '/') e.k.typ = '5';   // V7: Verzeichnis am '/'
            }
            e.kopf_off = p; e.daten_off = daten; e.ist = ist;
            e.k.size = soll;
            e.abgeschnitten = ist < soll;
            e.name = bereinige(e.k.name, e.pfad_geaendert);
            if (!e.name.empty()) s.e.push_back(std::move(e));
            if (ist < soll) { s.ende = Ende::Abgeschnitten; s.ende_off = daten; break; }
        }
        p = next;
    }
    return s;
}

TarFileSystem::Scan TarFileSystem::scanAlles(const TarQuelle& q) {
    uint8_t b[kBlock];
    tar::Kopf k;
    if (q.size() >= kBlock && q.read(0, b, kBlock) && tar::kopfLesen(b, k)) {
        Scan s = scanAb(q, 0);
        if (s.ok) {
            // doppelte Namen zaehlen
            std::map<std::string, int> n;
            for (const Eintrag& e : s.e) ++n[e.name];
            for (const auto& [name, c] : n) s.doppelt += c - 1;
            return s;
        }
    }
    // Fortsetzung: erster 512-ausgerichteter Kopf, ab dem der Strom sauber auslaeuft.
    for (uint64_t off = kBlock; off + kBlock <= q.size(); off += kBlock) {
        if (!q.read(off, b, kBlock) || !tar::kopfLesen(b, k)) continue;
        Scan s = scanAb(q, off);
        if (!s.ok || s.e.empty() || s.ende == Ende::Pruefsumme) continue;
        s.anfang = off;
        std::map<std::string, int> n;
        for (const Eintrag& e : s.e) ++n[e.name];
        for (const auto& [name, c] : n) s.doppelt += c - 1;
        return s;
    }
    return Scan{};
}

std::unique_ptr<TarFileSystem> TarFileSystem::mount(std::unique_ptr<TarQuelle> q, std::string& why) {
    if (!q) { why = "kein Datenstrom"; return nullptr; }
    if (!looksLikeTar(*q, &why)) return nullptr;
    std::unique_ptr<TarFileSystem> t(new TarFileSystem(std::move(q)));
    t->bewerte();
    return t;
}

bool TarFileSystem::istLeer(const TarQuelle& q, std::string* why) {
    std::vector<uint8_t> b(tar::kBlock);
    for (uint64_t off = 0; off + kBlock <= q.size(); off += kBlock) {
        if (!q.read(off, b.data(), kBlock)) { if (why) *why = "Datenstrom nicht lesbar"; return false; }
        if (!tar::nullblock(b.data())) {
            if (why) *why = "die Diskette ist nicht leer (Byte " + std::to_string(off) + " ist nicht 0)";
            return false;
        }
    }
    return q.size() >= kBlock;
}

std::unique_ptr<TarFileSystem> TarFileSystem::mountLeer(std::unique_ptr<TarQuelle> q, std::string& why) {
    if (!q) { why = "kein Datenstrom"; return nullptr; }
    if (!istLeer(*q, &why)) return nullptr;
    std::unique_ptr<TarFileSystem> t(new TarFileSystem(std::move(q)));
    t->leer_ = true;
    t->bewerte();
    return t;
}

std::unique_ptr<TarFileSystem> TarFileSystem::mountRoh(std::unique_ptr<TarQuelle> q, std::string& why) {
    if (!q) { why = "kein Datenstrom"; return nullptr; }
    std::unique_ptr<TarFileSystem> t(new TarFileSystem(std::move(q)));
    t->roh_ = true;
    t->bewerte();
    (void)why;
    return t;
}

void TarFileSystem::bewerte() {
    schreibbar_ = false;
    grund_.clear();
    if (!q_->beschreibbar()) {
        grund_ = "Dieser tar-Strom (Satz aus mehreren Abbildern) ist nur lesbar — Saetze lassen "
                 "sich noch nicht bearbeiten.";
        return;
    }
    if (leer_) { schreibbar_ = true; return; }
    const Scan s = roh_ ? Scan{} : scanAlles(*q_);
    if (!s.ok || s.anfang > 0) {
        grund_ = "Diese Diskette beginnt mitten in einer Datei (Folgeteil eines tar-Satzes) und ist "
                 "nur lesbar.  Zum Bearbeiten alle Disketten des Satzes zusammen als tar-Satz oeffnen.";
        return;
    }
    switch (s.ende) {
        case Ende::Abgeschnitten:
            grund_ = "Das tar-Archiv ist nicht abgeschlossen (die letzte Datei ist abgeschnitten, der "
                     "Strom setzt sich auf der naechsten Diskette fort) und ist nur lesbar.  Zum "
                     "Bearbeiten alle Disketten des Satzes als tar-Satz oeffnen.";
            return;
        case Ende::Pruefsumme:
            grund_ = "Hinter dem letzten Eintrag folgt weder eine Endmarke noch ein gueltiger tar-Kopf "
                     "(Pruefsumme stimmt nicht): das tar-Archiv ist nicht abgeschlossen oder beschaedigt "
                     "und nur lesbar (ein Neuaufbau verloere den Rest).";
            return;
        case Ende::Streamende:
            grund_ = "Das tar-Archiv hat keine Endmarke (zwei Nullbloecke) und ist nur lesbar — es "
                     "setzt sich vermutlich auf einer weiteren Diskette fort (tar-Satz).";
            return;
        case Ende::Nullblock: break;
    }
    uint8_t b[kBlock];
    if (s.ende_off + 2 * kBlock > q_->size() || !q_->read(s.ende_off + kBlock, b, kBlock)
        || !tar::nullblock(b)) {
        grund_ = "Das tar-Archiv endet mit einem einzelnen Nullblock statt der Endmarke (zwei "
                 "Nullbloecke) und ist nur lesbar.";
        return;
    }
    schreibbar_ = true;
}

TarFileSystem::Scan TarFileSystem::scan() const {
    if (leer_) { Scan s; s.ok = true; s.ende = Ende::Nullblock; return s; }
    Scan s = scanAlles(*q_);
    if (!s.ok && roh_) {            // kein Kopfblock irgendwo: der ganze Strom ist Fortsetzung
        s = Scan{};
        s.ok = true;
        s.anfang = q_->size();
        s.ende = Ende::Streamende;
        s.ende_off = q_->size();
    }
    return s;
}

bool TarFileSystem::looksLikeTar(const TarQuelle& q, std::string* why) {
    const Scan s = scanAlles(q);
    if (s.ok) return true;
    if (why) *why = "kein tar-Kopfblock bei Byte 0 und keiner, hinter dem ein tar-Strom sauber ausliefe";
    return false;
}

uint64_t TarFileSystem::fortsetzungAnfang() const { return scan().anfang; }

const TarFileSystem::Eintrag* TarFileSystem::finde(const Scan& s, const std::string& name) const {
    const Eintrag* r = nullptr;
    for (const Eintrag& e : s.e) if (e.name == name) r = &e;     // der letzte gewinnt
    return r;
}

std::vector<FileEntry> TarFileSystem::list() const {
    std::vector<FileEntry> out;
    const Scan s = scan();
    if (!s.ok) return out;
    if (s.anfang > 0) {
        FileEntry f;
        f.name = tar::kFortsetzungName;
        f.size = s.anfang;
        f.type = "-";
        f.unix_mode = 0100644;
        f.attributes = modeText('-', 0644);
        out.push_back(f);
    }
    std::map<std::string, size_t> letzter;
    for (size_t i = 0; i < s.e.size(); ++i) letzter[s.e[i].name] = i;
    for (size_t i = 0; i < s.e.size(); ++i) {
        const Eintrag& e = s.e[i];
        if (letzter[e.name] != i) continue;
        FileEntry f;
        char a; uint16_t bits;
        art(e.k.typ, a, bits);
        f.name = e.name;
        f.type = std::string(1, a);
        f.size = e.ist;
        if (e.k.typ == '1') {                  // harter Link: Inhalt des Ziels
            bool g = false;
            const Eintrag* z = finde(s, bereinige(e.k.linkname, g));
            f.size = z ? z->ist : 0;
            // Ziel auf einer anderen Diskette des Satzes: die Datei bleibt leer und gilt als
            // unvollstaendig (tar.linkziel) — ein Abbruch des ganzen Auszugs verlore alles.
            if (!z) f.damaged = true;
        }
        if (e.k.typ == '5' || e.k.typ == '3' || e.k.typ == '4' || e.k.typ == '6') f.size = 0;
        if (e.k.typ == '2') f.size = e.k.linkname.size();
        f.unix_mode  = static_cast<uint16_t>(bits | (e.k.mode & 07777));
        f.attributes = modeText(a, e.k.mode);
        f.unix_uid   = static_cast<int>(e.k.uid);
        f.unix_gid   = static_cast<int>(e.k.gid);
        f.unix_nlink = e.k.typ == '1' ? 2 : 1;
        f.unix_inode = static_cast<uint32_t>(i + 1);
        f.unix_mtime = e.k.mtime;
        f.date       = datumText(e.k.mtime);
        if (e.abgeschnitten) { f.damaged = true; f.declared_size = e.k.size; }
        out.push_back(std::move(f));
    }
    return out;
}

bool TarFileSystem::lies(const Scan& s, const Eintrag& e, std::vector<uint8_t>& out, int tiefe) const {
    out.clear();
    if (e.k.typ == '5') return fail(e.name + " ist ein Verzeichnis");
    if (e.k.typ == '3' || e.k.typ == '4' || e.k.typ == '6')
        return fail(e.name + " ist eine Geraetedatei/FIFO und hat keinen Inhalt");
    if (e.k.typ == '2') { out.assign(e.k.linkname.begin(), e.k.linkname.end()); return true; }
    if (e.k.typ == '1') {
        bool g = false;
        if (tiefe >= 8) return fail(e.name + ": Linkkette zu tief (Schleife?)");
        const Eintrag* z = finde(s, bereinige(e.k.linkname, g));
        // Ziel nicht im Strom (liegt auf einer anderen Diskette des Satzes): leer lesen,
        // list() markiert den Eintrag als unvollstaendig, check() meldet tar.linkziel.
        if (!z) return true;
        return lies(s, *z, out, tiefe + 1);
    }
    out.resize(static_cast<size_t>(e.ist));
    if (e.ist && !q_->read(e.daten_off, out.data(), out.size()))
        return fail(e.name + ": Datenstrom nicht lesbar");
    return true;
}

bool TarFileSystem::read(const std::string& name, std::vector<uint8_t>& out) {
    const Scan s = scan();
    if (!s.ok) return fail("kein tar-Strom");
    if (s.anfang > 0 && name == tar::kFortsetzungName) {
        out.resize(static_cast<size_t>(s.anfang));
        return q_->read(0, out.data(), out.size()) || fail("Datenstrom nicht lesbar");
    }
    bool g = false;
    const Eintrag* e = finde(s, bereinige(name, g));
    if (!e) return fail("Datei nicht gefunden: " + name);
    return lies(s, *e, out, 0);
}

bool TarFileSystem::firstSector(const std::string& name, FsRecoverOrt& out) const {
    const Scan s = scan();
    if (s.anfang > 0 && name == tar::kFortsetzungName) return q_->ort(0, out);
    bool g = false;
    const Eintrag* e = finde(s, bereinige(name, g));
    return e && q_->ort(e->kopf_off, out);
}

bool TarFileSystem::ladeArchiv(tar::TarArchiv& a) const {
    const Scan s = scan();
    a.eintraege().clear();
    for (const Eintrag& e : s.e) {
        tar::ArchivEintrag n;
        n.k = e.k;
        if (n.k.typ == '7') n.k.typ = '0';          // "zusammenhaengend" ist fuer uns eine Datei
        if (n.k.typ == '0') {
            n.daten.resize(static_cast<size_t>(e.ist));
            if (e.ist && !q_->read(e.daten_off, n.daten.data(), n.daten.size()))
                return fail(e.name + ": Datenstrom nicht lesbar");
        }
        a.eintraege().push_back(std::move(n));
    }
    return true;
}

bool TarFileSystem::speichere(const tar::TarArchiv& a) {
    const Scan alt = scan();
    std::vector<uint8_t> strom;
    std::string err;
    if (!a.strom(strom, err)) return fail(err);
    if (strom.size() > q_->size())
        return fail("Das Archiv wuerde " + std::to_string(strom.size()) + " Byte belegen (Koepfe, Daten, "
                    "Endmarke), der Datentraeger fasst nur " + std::to_string(q_->size())
                    + " Byte.  Ein tar-Satz ueber mehrere Disketten ist hier noch nicht moeglich.");
    // Der Rest des alten Archivs (samt alter Endmarke) wird Null; ein leeres Archiv ist wieder
    // die ganz-Null-Diskette und wird so auch beim Oeffnen als leer erkannt.
    uint64_t alt_ende = leer_ ? 0 : std::min<uint64_t>(q_->size(), alt.ende_off + 2 * kBlock);
    if (a.eintraege().empty()) alt_ende = q_->size();
    strom.resize(static_cast<size_t>(std::max<uint64_t>(strom.size(), alt_ende)), 0);
    if (!q_->schreibe(0, strom.data(), strom.size()))
        return fail("Schreiben in den tar-Strom fehlgeschlagen");
    leer_ = a.eintraege().empty();
    return true;
}

bool TarFileSystem::write(const std::string& name, const std::vector<uint8_t>& data, const WriteOptions& opt) {
    if (!schreibbar_) return fail(grund_);
    tar::TarArchiv a;
    if (!ladeArchiv(a)) return false;
    tar::NeuAngaben n;
    n.mode  = opt.wega_mode_gesetzt ? (opt.wega_mode & 07777u) : 0644u;
    n.uid   = static_cast<uint32_t>(opt.wega_uid);
    n.gid   = static_cast<uint32_t>(opt.wega_gid);
    n.mtime = opt.wega_mtime;
    std::string err;
    if (!a.hinzufuegen(name, data, n, opt.overwrite, err)) return fail(err);
    return speichere(a);
}

bool TarFileSystem::erase(const std::string& name) {
    if (!schreibbar_) return fail(grund_);
    tar::TarArchiv a;
    if (!ladeArchiv(a)) return false;
    std::string err;
    if (!a.loeschen(name, err)) return fail(err);
    return speichere(a);
}

bool TarFileSystem::makeDirectory(const std::string& name) {
    if (!schreibbar_) return fail(grund_);
    tar::TarArchiv a;
    if (!ladeArchiv(a)) return false;
    tar::NeuAngaben n;
    n.mode = 0755;
    std::string err;
    if (!a.verzeichnis(name, n, err)) return fail(err);
    return speichere(a);
}

bool TarFileSystem::setAttributes(const std::string& name, const UnixAttrs& u) {
    if (!schreibbar_) return fail(grund_);
    if ((u.set_uid && (u.uid < 0 || u.uid > 65535)) || (u.set_gid && (u.gid < 0 || u.gid > 65535)))
        return fail("uid/gid muessen zwischen 0 und 65535 liegen");
    tar::TarArchiv a;
    if (!ladeArchiv(a)) return false;
    const uint32_t mode = u.mode, uid = static_cast<uint32_t>(u.uid), gid = static_cast<uint32_t>(u.gid),
                   mtime = u.mtime;
    std::string err;
    if (!a.attribute(name, u.set_mode ? &mode : nullptr, u.set_uid ? &uid : nullptr,
                     u.set_gid ? &gid : nullptr, u.set_mtime ? &mtime : nullptr, err))
        return fail(err);
    return speichere(a);
}

bool TarFileSystem::wouldFit(const std::vector<PlannedFile>& files, FitReport& out) const {
    out = FitReport{};
    if (!schreibbar_) { out.detail = grund_; return true; }
    tar::TarArchiv a;
    if (!ladeArchiv(a)) return false;
    // Der Bedarf des neuen Stroms: bestehende Eintraege, die ersetzt werden, fallen weg;
    // jede geplante Datei bringt Kopf + aufgefuellte Daten mit.
    uint64_t n = a.stromLaenge();
    std::map<std::string, uint64_t> geplant;
    for (const PlannedFile& f : files) {
        bool g = false;
        geplant[tar::bereinigePfad(f.name, g)] = kBlock + (f.size + kBlock - 1) / kBlock * kBlock;
    }
    for (const tar::ArchivEintrag& e : a.eintraege()) {
        const auto it = geplant.find(e.schluessel());
        if (it != geplant.end() && e.k.typ == '0') n -= e.belegt();
    }
    for (const auto& [name, b] : geplant) {
        n += b;
        if (name.size() > tar::kNameMax) {
            out.detail = "Der Name '" + name + "' ist laenger als " + std::to_string(tar::kNameMax)
                         + " Zeichen — das V7-tar des P8000 kennt keine laengeren Namen.";
            return true;
        }
    }
    out.needed    = n;
    out.available = q_->size();
    out.fits      = n <= q_->size();
    if (!out.fits)
        out.detail = "Das Archiv wuerde " + std::to_string(n) + " Byte belegen, der Datentraeger fasst "
                     + std::to_string(q_->size()) + " Byte.  Ein tar-Satz ueber mehrere Disketten ist "
                     "hier noch nicht moeglich.";
    return true;
}
bool TarFileSystem::mkfs() {
    return fail(schreibbar_ ? "Eine tar-Diskette hat kein Dateisystem, das sich anlegen liesse"
                            : grund_);
}

FsInfo TarFileSystem::info() const {
    FsInfo i;
    const Scan s = scan();
    i.total_bytes = q_->size();
    uint64_t belegt = s.anfang;
    for (const Eintrag& e : s.e) belegt += kBlock + (e.ist + kBlock - 1) / kBlock * kBlock;
    i.used_bytes = std::min(belegt, i.total_bytes);
    i.free_bytes = i.total_bytes - i.used_bytes;     // Rest hinter dem Archivende (Altbestand)
    int n = 0;
    for (const FileEntry& e : list()) if (e.type != "d") ++n;
    i.files = n;
    return i;
}

FsCheckReport TarFileSystem::check(FsCheckLevel level, bool nachladen) const {
    (void)nachladen;
    FsCheckReport r;
    r.level = level;
    FsFindings f(r);
    r.schritt("tar.schritt.kopfbloecke", "Kopfbloecke, Pruefsummen und Strom");
    const Scan s = scan();
    if (!s.ok) {
        f.add("tar.kein_strom", FsSeverity::Fehler, FsLayer::Verwaltung, "(Strom)",
              "Kein tar-Strom gefunden");
        r.schrittEnde();
        return r;
    }
    if (s.anfang > 0)
        f.add("tar.fortsetzung", FsSeverity::Info, FsLayer::Verwaltung, "Byte 0",
              "Die Diskette beginnt mitten in einer Datei (" + std::to_string(s.anfang)
              + " Byte Anfangsstueck, als " + tar::kFortsetzungName + " lesbar); der Strom setzt "
              "bei Byte " + std::to_string(s.anfang) + " wieder auf.  Mit den vorigen Disketten "
              "zusammen: tar-satz");
    if (s.ende == Ende::Pruefsumme)
        f.add("tar.pruefsumme", FsSeverity::Fehler, FsLayer::Verwaltung,
              "Byte " + std::to_string(s.ende_off),
              "Kopfblock ohne gueltige Pruefsumme — der Rest des Stroms ist nicht lesbar");
    if (s.ende == Ende::Abgeschnitten && !s.e.empty()) {
        const Eintrag& e = s.e.back();
        f.add("tar.abgeschnitten", FsSeverity::Warnung, FsLayer::Verwaltung, e.name,
              "Die letzte Datei ist abgeschnitten: " + std::to_string(e.ist) + " von "
              + std::to_string(e.k.size) + " Byte auf dieser Diskette; der Strom setzt sich "
              "auf der naechsten Diskette fort (tar-satz).  Extrahiert wird das Teilstueck");
    }
    for (const Eintrag& e : s.e) {
        if (e.k.typ != '1') continue;
        bool g = false;
        if (finde(s, bereinige(e.k.linkname, g))) continue;
        f.add("tar.linkziel", FsSeverity::Warnung, FsLayer::Verwaltung, e.name,
              "Hartlink auf '" + e.k.linkname + "': das Ziel steht nicht auf dieser Diskette; "
              "die Datei bleibt leer.  Mit den uebrigen Disketten zusammen: tar-satz");
    }
    if (s.doppelt > 0)
        f.add("tar.doppelt", FsSeverity::Info, FsLayer::Verwaltung, "(Namen)",
              std::to_string(s.doppelt) + " Name(n) kommen mehrfach vor — gezeigt wird je Name "
              "die letzte Fassung, wie bei tar -x");
    int ge = 0;
    for (const Eintrag& e : s.e) if (e.pfad_geaendert) ++ge;
    if (ge > 0)
        f.add("tar.pfad", FsSeverity::Info, FsLayer::Verwaltung, "(Namen)",
              std::to_string(ge) + " Name(n) mit fuehrendem '/' oder '..' — beim Auflisten "
              "relativ gemacht ('..' wird '__')");
    r.schrittEnde();
    return r;
}
