/**
 * @file sector_track_io.cpp
 * @brief Implementierung von SectorTrackIo (Sektorlisten ⇄ DiskMedium).
 *
 * @see core/peripherals/floppy_drive/sector_track_io.h
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include "core/peripherals/floppy_drive/sector_track_io.h"

#include <algorithm>

namespace SectorTrackIo {

namespace {

std::string spurText(uint8_t c, uint8_t h) {
    return "Spur " + std::to_string(c) + "/" + std::to_string(h);
}

bool istLuecke(uint8_t b) { return b == 0x4E || b == 0xFF || b == 0x00; }

/// Traegt der Nachspann hinter der Daten-CRC Nutzdaten?  Gap-Fueller sind keine;
/// auf MFM-Spuren gilt zusaetzlich ein mit 4E/4F beginnender Schreibnachlauf als Luecke
/// (UDOS' Kontrollblock beginnt nie mit 4E/4F, s. DiskMedium::trackRawCompatible).
bool nachspannHatNutzdaten(const LogicalSector& s, Encoding enc) {
    if (std::all_of(s.tail.begin(), s.tail.end(), istLuecke)) return false;
    if (enc == Encoding::MFM && !s.tail.empty() && (s.tail[0] == 0x4E || s.tail[0] == 0x4F))
        return false;
    return true;
}

}  // namespace

uint16_t guessRate(Encoding enc, size_t track_bytes) {
    if (enc == Encoding::MFM) return track_bytes >= 8000 ? 500 : 250;
    return track_bytes >= 4000 ? 500 : 250;
}

bool representable(const DiskMedium& m, std::string& why) {
    why.clear();
    if (m.numCylinders() == 0 || m.numHeads() == 0) { why = "kein Medium"; return false; }
    if (!m.formatted())                             { why = "unformatiert"; return false; }
    const bool schnell = m.rawCompatible();   // Positivweg: dann ist ohnehin nichts zu pruefen
    for (uint8_t c = 0; c < m.numCylinders(); ++c)
        for (uint8_t h = 0; h < m.numHeads(); ++h) {
            const TrackImage& t = m.peek(c, h);
            if (t.empty()) continue;
            const auto secs = TrackCodec::parseTrack(t);
            for (const LogicalSector& s : secs) {
                if (!schnell && s.data_pos != SIZE_MAX && nachspannHatNutzdaten(s, t.encoding)) {
                    why = spurText(c, h) + ": Daten hinter der Daten-CRC (z. B. UDOS-Sektorkontrollblock)";
                    return false;
                }
            }
        }
    return true;
}

bool fromMedium(const DiskMedium& m, std::vector<TrackRecord>& out, std::string& err) {
    out.clear();
    std::string why;
    if (!representable(m, why)) { err = why; return false; }
    for (uint8_t c = 0; c < m.numCylinders(); ++c)
        for (uint8_t h = 0; h < m.numHeads(); ++h) {
            const TrackImage& t = m.peek(c, h);
            if (t.empty()) continue;
            const auto secs = TrackCodec::parseTrack(t);
            if (secs.empty()) continue;   // Spur ohne Adressmarke = unformatiert
            TrackRecord tr;
            tr.cyl = c; tr.head = h; tr.enc = t.encoding;
            tr.rate_kbps = guessRate(t.encoding, t.bytes.size());
            for (const LogicalSector& s : secs) {
                SectorRecord r;
                r.c = s.cyl; r.h = s.head; r.r = s.id;
                r.n = 0;
                while (r.n < 6 && (128u << r.n) < s.size) ++r.n;
                r.id_error = !s.id_crc_ok;
                if (s.data_pos == SIZE_MAX || s.end_pos == SIZE_MAX) {
                    r.no_data = true;   // kein (vollstaendiges) Datenfeld
                } else {
                    r.deleted    = s.deleted;
                    r.data_error = !s.data_crc_ok;
                    r.data       = s.data;
                }
                tr.sectors.push_back(std::move(r));
            }
            out.push_back(std::move(tr));
        }
    if (out.empty()) { err = "keine Spur mit Sektoren"; return false; }
    return true;
}

bool toMedium(const std::vector<TrackRecord>& tracks, DiskMedium& out, std::string& err) {
    if (tracks.empty()) { err = "keine Spuren"; return false; }
    unsigned maxC = 0, maxH = 0;
    for (const TrackRecord& tr : tracks) {
        maxC = std::max<unsigned>(maxC, tr.cyl);
        maxH = std::max<unsigned>(maxH, tr.head);
        for (const SectorRecord& s : tr.sectors) {
            if (s.n > 6) {
                err = spurText(tr.cyl, tr.head) + ": Sektorgroessen-Code " + std::to_string(s.n)
                      + " nicht darstellbar (nur 128..8192 Byte)";
                return false;
            }
            if (!s.no_data && s.data.size() != s.size()) {
                err = spurText(tr.cyl, tr.head) + ": Sektor " + std::to_string(s.r)
                      + " hat " + std::to_string(s.data.size()) + " statt "
                      + std::to_string(s.size()) + " Byte";
                return false;
            }
        }
    }
    if (maxC > 254 || maxH > 1) {   // DiskMedium rechnet in uint8_t; mehr als 2 Koepfe gibt es nicht
        err = "Zylinder/Kopf ausserhalb des darstellbaren Bereichs";
        return false;
    }

    DiskMedium neu(static_cast<uint8_t>(maxC + 1), static_cast<uint8_t>(maxH + 1),
                   tracks.front().enc);
    for (const TrackRecord& tr : tracks) {
        if (tr.sectors.empty()) continue;
        std::vector<LogicalSector> ls;
        ls.reserve(tr.sectors.size());
        for (const SectorRecord& s : tr.sectors) {
            LogicalSector l;
            l.cyl = s.c; l.head = s.h; l.id = s.r; l.size = s.size();
            l.deleted     = s.deleted;
            l.id_crc_ok   = !s.id_error;
            l.data_crc_ok = !s.data_error;
            l.no_data     = s.no_data;
            if (!s.no_data) l.data = s.data;
            ls.push_back(std::move(l));
        }
        neu.setTrack(tr.cyl, tr.head, TrackCodec::buildTrack(ls, tr.enc));
    }
    neu.clearDirty();
    out = std::move(neu);
    return true;
}

}  // namespace SectorTrackIo
