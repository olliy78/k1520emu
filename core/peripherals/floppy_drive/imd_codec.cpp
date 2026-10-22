/**
 * @file imd_codec.cpp
 * @brief Implementierung von ImdCodec (ImageDisk-Abbild, Lesen und Schreiben).
 *
 * @see core/peripherals/floppy_drive/imd_codec.h
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include "core/peripherals/floppy_drive/imd_codec.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <utility>

namespace {

using SectorTrackIo::SectorRecord;
using SectorTrackIo::TrackRecord;

constexpr size_t kPruefBytes = 64 * 1024;   // so weit sucht isImd nach dem 0x1A
constexpr size_t kMaxDatei   = 64u * 1024u * 1024u;

std::string spurText(unsigned c, unsigned h) {
    return "Spur " + std::to_string(c) + "/" + std::to_string(h);
}

std::string trimmen(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

bool leseDatei(const std::string& path, std::vector<uint8_t>& out, std::string& err) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { err = "Kann Datei nicht oeffnen: " + path; return false; }
    const auto n = static_cast<size_t>(f.tellg());
    if (n > kMaxDatei) { err = "ImageDisk-Datei unplausibel gross: " + path; return false; }
    out.resize(n);
    f.seekg(0);
    if (n && !f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(n))) {
        err = "Lesefehler: " + path;
        return false;
    }
    return true;
}

/// @brief Stand-Zeit als "TT/MM/JJJJ hh:mm:ss" (UTC).
std::string zeitText() {
    const std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d", tmv.tm_mday, tmv.tm_mon + 1,
                  tmv.tm_year + 1900, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return buf;
}

/// @brief Sektorliste vergleichen (Gegenprobe); ohne Datenfeld zaehlen nur die ID-Felder.
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

}  // namespace

// ─── Modus ───────────────────────────────────────────────────────────────────

bool ImdCodec::modeToEncoding(uint8_t mode, Encoding& enc, uint16_t& rate) {
    if (mode > 5) return false;
    enc = mode < 3 ? Encoding::FM : Encoding::MFM;
    static const uint16_t kRate[3] = {500, 300, 250};
    rate = kRate[mode % 3];
    return true;
}

uint8_t ImdCodec::encodingToMode(Encoding enc, uint16_t rate) {
    const uint8_t basis = enc == Encoding::FM ? 0 : 3;
    return static_cast<uint8_t>(basis + (rate >= 500 ? 0 : rate == 300 ? 1 : 2));
}

// ─── Erkennung ───────────────────────────────────────────────────────────────

bool ImdCodec::isImd(const uint8_t* d, size_t n) {
    if (n < 5 || std::memcmp(d, "IMD ", 4) != 0) return false;
    return std::memchr(d, 0x1A, std::min(n, kPruefBytes)) != nullptr;
}

bool ImdCodec::isImd(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> buf(kPruefBytes);
    f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return isImd(buf.data(), static_cast<size_t>(f.gcount()));
}

// ─── Laden ───────────────────────────────────────────────────────────────────

bool ImdCodec::decodeBuffer(const std::vector<uint8_t>& d, std::vector<TrackRecord>& tracks,
                            SourceInfo* info, std::string& err) {
    SourceInfo si;
    tracks.clear();
    if (d.size() < 4 || std::memcmp(d.data(), "IMD ", 4) != 0) {
        err = "Keine ImageDisk-Datei (Kennung 'IMD ' fehlt)";
        return false;
    }
    const auto it = std::find(d.begin(), d.end(), uint8_t{0x1A});
    if (it == d.end()) {
        err = "ImageDisk: Kopf nicht abgeschlossen (Endekennzeichen 0x1A fehlt)";
        return false;
    }
    const size_t ende = static_cast<size_t>(it - d.begin());

    // Kopfzeile "IMD v.vv: TT/MM/JJJJ hh:mm:ss", dann Kommentar bis 0x1A
    size_t z = 0;
    while (z < ende && d[z] != '\r' && d[z] != '\n') ++z;
    const std::string zeile(d.begin(), d.begin() + static_cast<long>(z));
    const size_t dp = zeile.find(':');
    if (dp != std::string::npos) {
        si.version   = trimmen(zeile.substr(4, dp - 4));
        si.timestamp = trimmen(zeile.substr(dp + 1));
    }
    size_t k = z;
    if (k < ende && d[k] == '\r') ++k;
    if (k < ende && d[k] == '\n') ++k;
    si.comment = trimmen(std::string(d.begin() + static_cast<long>(k), d.begin() + static_cast<long>(ende)));
    si.raw_header.assign(d.begin(), d.begin() + static_cast<long>(ende));

    std::map<unsigned, size_t> lage;   // Zylinder*2+Kopf -> Index in tracks
    size_t pos = ende + 1;
    while (pos < d.size()) {
        if (d.size() - pos < 5) {
            si.trailing_bytes = d.size() - pos;
            si.warnings.push_back("Rest von " + std::to_string(si.trailing_bytes)
                                  + " Byte(s) hinter dem letzten Spursatz ignoriert");
            break;
        }
        const size_t satz = pos;   // fuer Fehlertexte
        const uint8_t mode = d[pos], cyl = d[pos + 1], hb = d[pos + 2], nsec = d[pos + 3],
                      sc = d[pos + 4];
        pos += 5;
        const unsigned head = hb & 0x3F;
        const std::string wo = spurText(cyl, head) + " (Offset " + std::to_string(satz) + ")";

        TrackRecord tr;
        tr.cyl = cyl;
        tr.head = static_cast<uint8_t>(head);
        if (!modeToEncoding(mode, tr.enc, tr.rate_kbps)) {
            err = "ImageDisk: " + wo + ": unbekannter Modus " + std::to_string(mode);
            return false;
        }
        if (head > 1) {
            err = "ImageDisk: " + wo + ": ungueltiger Kopf " + std::to_string(head);
            return false;
        }
        if (sc > 6) {
            err = "ImageDisk: " + wo + ": ungueltiger Sektorgroessen-Code " + std::to_string(sc);
            return false;
        }

        if (nsec > 0) {
            const bool cmap = (hb & 0x80) != 0, hmap = (hb & 0x40) != 0;
            const size_t kopfBytes = static_cast<size_t>(nsec) * (1u + (cmap ? 1u : 0u) + (hmap ? 1u : 0u));
            if (d.size() - pos < kopfBytes) {
                err = "ImageDisk: " + wo + ": Datei abgeschnitten (Sektornummern/Maps)";
                return false;
            }
            const uint8_t* rmap = &d[pos];
            const uint8_t* cm   = cmap ? rmap + nsec : nullptr;
            const uint8_t* hm   = hmap ? rmap + nsec * (cmap ? 2u : 1u) : nullptr;
            pos += kopfBytes;

            const size_t groesse = 128u << sc;
            for (unsigned i = 0; i < nsec; ++i) {
                if (pos >= d.size()) {
                    err = "ImageDisk: " + wo + ": Datei abgeschnitten (Sektor " + std::to_string(i) + ")";
                    return false;
                }
                const uint8_t typ = d[pos++];
                if (typ > 8) {
                    err = "ImageDisk: " + wo + ": unbekannter Sektortyp " + std::to_string(typ);
                    return false;
                }
                SectorRecord s;
                s.c = cm ? cm[i] : cyl;
                s.h = hm ? hm[i] : static_cast<uint8_t>(head);
                s.r = rmap[i];
                s.n = sc;
                if (typ == 0) {
                    s.no_data = true;
                } else {
                    const unsigned t = typ - 1u;
                    const bool komprimiert = (t & 1u) != 0;
                    s.deleted    = (t & 2u) != 0;
                    s.data_error = (t & 4u) != 0;
                    const size_t bytes = komprimiert ? 1 : groesse;
                    if (d.size() - pos < bytes) {
                        err = "ImageDisk: " + wo + ": Datei abgeschnitten (Daten von Sektor "
                              + std::to_string(s.r) + ")";
                        return false;
                    }
                    if (komprimiert) s.data.assign(groesse, d[pos]);
                    else             s.data.assign(d.begin() + static_cast<long>(pos),
                                                   d.begin() + static_cast<long>(pos + groesse));
                    pos += bytes;
                }
                tr.sectors.push_back(std::move(s));
            }
        }

        const unsigned key = static_cast<unsigned>(cyl) * 2u + head;
        const auto f = lage.find(key);
        if (f != lage.end()) {
            si.warnings.push_back(spurText(cyl, head) + " kommt mehrfach vor, der letzte Satz gilt");
            tracks[f->second] = std::move(tr);
        } else {
            lage[key] = tracks.size();
            tracks.push_back(std::move(tr));
        }
    }

    for (const TrackRecord& t : tracks) {
        si.modes.push_back({t.cyl, t.head, encodingToMode(t.enc, t.rate_kbps)});
        ++si.tracks;
        si.sectors += t.sectors.size();
        if (t.sectors.empty()) ++si.empty_tracks;
    }
    if (tracks.empty()) {
        err = "ImageDisk: die Datei enthaelt keine Spuren";
        return false;
    }
    if (info) *info = std::move(si);
    return true;
}

bool ImdCodec::loadBuffer(const std::vector<uint8_t>& datei, DiskMedium& out, SourceInfo* info,
                          std::string& err) {
    std::vector<TrackRecord> tracks;
    SourceInfo si;
    if (!decodeBuffer(datei, tracks, &si, err)) return false;
    if (!SectorTrackIo::toMedium(tracks, out, err)) {
        err = "ImageDisk: " + err;
        return false;
    }
    if (info) *info = std::move(si);
    return true;
}

bool ImdCodec::load(const std::string& path, DiskMedium& out, SourceInfo* info, std::string& err) {
    std::vector<uint8_t> d;
    if (!leseDatei(path, d, err)) return false;
    return loadBuffer(d, out, info, err);
}

std::string ImdCodec::infoField(const SourceInfo& si, const std::string& key) {
    if (key == "version")   return si.version;
    if (key == "timestamp") return si.timestamp;
    if (key == "comment")   return si.comment;
    if (key == "tracks")    return std::to_string(si.tracks);
    if (key == "sectors")   return std::to_string(si.sectors);
    if (key == "warnings") {
        std::string s;
        for (const auto& w : si.warnings) s += (s.empty() ? "" : "\n") + w;
        return s;
    }
    return "";
}

// ─── Schreiben ───────────────────────────────────────────────────────────────

bool ImdCodec::encode(const DiskMedium& in, const SourceInfo* vorlage, std::vector<uint8_t>& out,
                      SourceInfo* geschrieben, std::string& err) {
    std::vector<TrackRecord> formatiert;
    if (!SectorTrackIo::fromMedium(in, formatiert, err)) {
        err = "ImageDisk: " + err;
        return false;
    }

    // Vorlagen-Modi je Spur
    std::map<unsigned, uint8_t> vModi;
    if (vorlage)
        for (const TrackMode& m : vorlage->modes) vModi[m.cyl * 2u + m.head] = m.mode;

    std::map<unsigned, size_t> vorhanden;
    for (size_t i = 0; i < formatiert.size(); ++i)
        vorhanden[formatiert[i].cyl * 2u + formatiert[i].head] = i;

    // Pruefung und Rate je Spur; dann die vollstaendige Soll-Liste (mit leeren Spuren)
    const Encoding standard = formatiert.front().enc;
    std::vector<TrackRecord> soll;
    for (uint8_t c = 0; c < in.numCylinders(); ++c)
        for (uint8_t h = 0; h < in.numHeads(); ++h) {
            const unsigned key = c * 2u + h;
            const auto v = vorhanden.find(key);
            TrackRecord tr;
            if (v == vorhanden.end()) {
                tr.cyl = c; tr.head = h; tr.enc = standard; tr.rate_kbps = 250;
                const auto m = vModi.find(key);
                if (m != vModi.end()) modeToEncoding(m->second, tr.enc, tr.rate_kbps);
                soll.push_back(std::move(tr));
                continue;
            }
            tr = std::move(formatiert[v->second]);
            if (tr.sectors.size() > 255) {
                err = "ImageDisk: " + spurText(c, h) + " hat mehr als 255 Sektoren";
                return false;
            }
            for (const SectorRecord& s : tr.sectors) {
                if (s.n != tr.sectors.front().n) {
                    err = "ImageDisk kennt keine gemischten Sektorgroessen in einer Spur ("
                          + spurText(c, h) + " enthaelt " + std::to_string(128u << tr.sectors.front().n)
                          + " und " + std::to_string(128u << s.n)
                          + " Byte). Bitte .hfe oder .dmk verwenden.";
                    return false;
                }
                if (s.id_error) {
                    err = "ImageDisk kann keinen ID-CRC-Fehler darstellen (" + spurText(c, h)
                          + ", Sektor " + std::to_string(s.r) + "). Bitte .hfe oder .dmk verwenden.";
                    return false;
                }
            }
            const auto m = vModi.find(key);
            Encoding menc; uint16_t mrate;
            if (m != vModi.end() && modeToEncoding(m->second, menc, mrate) && menc == tr.enc)
                tr.rate_kbps = mrate;   // Kanalrate der Quelle (das Medium fuehrt sie nicht)
            soll.push_back(std::move(tr));
        }

    // Datei aufbauen
    std::vector<uint8_t> f;
    if (vorlage && !vorlage->raw_header.empty()
        && std::find(vorlage->raw_header.begin(), vorlage->raw_header.end(), uint8_t{0x1A})
               == vorlage->raw_header.end()
        && vorlage->raw_header.size() >= 4
        && std::memcmp(vorlage->raw_header.data(), "IMD ", 4) == 0) {
        f = vorlage->raw_header;
    } else {
        const std::string kopf = "IMD 1.17: " + zeitText() + "\r\nImage file created by k1520emu\r\n";
        f.assign(kopf.begin(), kopf.end());
    }
    f.push_back(0x1A);

    for (const TrackRecord& tr : soll) {
        uint8_t hb = tr.head;
        for (const SectorRecord& s : tr.sectors) {
            if (s.c != tr.cyl)  hb |= 0x80;
            if (s.h != tr.head) hb |= 0x40;
        }
        const uint8_t sc = tr.sectors.empty() ? 0 : tr.sectors.front().n;
        f.push_back(encodingToMode(tr.enc, tr.rate_kbps));
        f.push_back(tr.cyl);
        f.push_back(hb);
        f.push_back(static_cast<uint8_t>(tr.sectors.size()));
        f.push_back(sc);
        for (const SectorRecord& s : tr.sectors) f.push_back(s.r);
        if (hb & 0x80) for (const SectorRecord& s : tr.sectors) f.push_back(s.c);
        if (hb & 0x40) for (const SectorRecord& s : tr.sectors) f.push_back(s.h);
        for (const SectorRecord& s : tr.sectors) {
            if (s.no_data) { f.push_back(0); continue; }
            const bool komprimiert = !s.data.empty()
                && std::all_of(s.data.begin(), s.data.end(), [&](uint8_t b) { return b == s.data[0]; });
            f.push_back(static_cast<uint8_t>(1 + (komprimiert ? 1 : 0) + (s.deleted ? 2 : 0)
                                             + (s.data_error ? 4 : 0)));
            if (komprimiert) f.push_back(s.data[0]);
            else             f.insert(f.end(), s.data.begin(), s.data.end());
        }
    }

    // Gegenprobe: was wir geschrieben haben, muss der Leser unveraendert zurueckgeben.
    std::vector<TrackRecord> zurueck;
    SourceInfo si;
    std::string e2;
    if (!decodeBuffer(f, zurueck, &si, e2) || !gleich(soll, zurueck)) {
        err = "ImageDisk: interner Fehler beim Kodieren (Gegenprobe fehlgeschlagen"
              + (e2.empty() ? std::string() : ": " + e2) + ")";
        return false;
    }
    out = std::move(f);
    if (geschrieben) *geschrieben = std::move(si);
    return true;
}

bool ImdCodec::save(const std::string& path, const DiskMedium& in, const SourceInfo* vorlage,
                    SourceInfo* geschrieben, std::string& err) {
    std::vector<uint8_t> datei;
    if (!encode(in, vorlage, datei, geschrieben, err)) return false;   // Zieldatei unberuehrt

    // Erst in eine Nachbardatei, dann umbenennen: nie ein halb geschriebenes Abbild.
    const std::string tmp = path + ".k1520tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) { err = "Kann Datei nicht schreiben: " + tmp; return false; }
        f.write(reinterpret_cast<const char*>(datei.data()),
                static_cast<std::streamsize>(datei.size()));
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

bool ImdCodec::save(const std::string& path, const DiskMedium& in, std::string& err) {
    return save(path, in, nullptr, nullptr, err);
}
