/**
 * @file sector_track_io.h
 * @brief SectorTrackIo – gemeinsame Zwischenschicht der Sektor-Container (.td0, .imd):
 *        Sektorlisten je Spur ⇄ @ref DiskMedium.
 *
 * TeleDisk und ImageDisk speichern keine Bitstroeme, sondern je Spur eine Liste von
 * Sektoren in **Spurreihenfolge** (C/H/R/N, Marken-/Fehlerflags, Daten) plus Verfahren
 * (FM/MFM) und Datenrate.  Diese Schicht kennt keine Dateiformate: sie baut aus solchen
 * Listen ueber @ref TrackCodec::buildTrack die Spuren des Mediums und liest sie ueber
 * @ref TrackCodec::parseTrack wieder heraus.  Reihenfolge (Interleave), gemischte
 * Sektorgroessen in einer Spur, geloeschte Datenmarken, absichtliche CRC-Fehler und
 * Sektoren ohne Datenfeld bleiben dabei erhalten — anders als bei `.img`.
 *
 * **Befund zu @ref DiskMedium::rawCompatible (am Code geprueft, Entwurf 38 AP A0):**
 *   - CRC-Fehler (ID oder Daten): `rawCompatible()` ist **false** — die Spur gilt als
 *     nicht `.img`-faehig.  Fuer .td0/.imd ist ein CRC-Fehler aber darstellbar, die
 *     Schranke ist hier also zu streng und taugt nur als Teilprobe.
 *   - geloeschte Datenmarke (DAM F8): `rawCompatible()` **ignoriert** sie (true), obwohl
 *     ein `.img` sie verlieren wuerde.  Fuer .td0/.imd geht sie mit (Flag `deleted`).
 *   - gemischte Sektorgroessen in einer Spur: `rawCompatible()` ignoriert sie (true).
 *     Ob das Zielformat sie kann (IMD nein, TD0 ja), entscheidet der Codec.
 *   - Sektor ohne Datenfeld: kein Datenfeld → `data_crc_ok == false` → **false**.
 *   - Bytes hinter der Daten-CRC (UDOS-Sektorkontrollblock): **false** — und das ist die
 *     Grenze, die auch hier gilt.
 * Deshalb benutzt @ref representable `rawCompatible()` nur als schnellen Positivweg und
 * prueft sonst selbst: erlaubt sind CRC-Fehler, Loeschmarken, fehlende Datenfelder;
 * abgelehnt wird allein ein Nachspann mit Nutzdaten.  Sektorgroessen gehen bis
 * Code 6 (8192 B); @ref toMedium lehnt groessere Codes ab.
 *
 * @see doc/design/38_td0_imd.md §4
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#pragma once
#include "disk_medium.h"
#include "track_codec.h"
#include <cstdint>
#include <string>
#include <vector>

namespace SectorTrackIo {

/// @brief Ein Sektor einer Spur, wie ihn .td0/.imd fuehren.
struct SectorRecord {
    uint8_t c = 0;            ///< Zylinder laut ID-Feld
    uint8_t h = 0;            ///< Kopf laut ID-Feld
    uint8_t r = 1;            ///< Sektornummer laut ID-Feld
    uint8_t n = 0;            ///< Groessen-Code (Bytes = 128 << n); 0..6 (128..8192 B)
    bool deleted    = false;  ///< geloeschte Datenmarke (DAM F8)
    bool data_error = false;  ///< Daten-CRC falsch
    bool id_error   = false;  ///< ID-CRC falsch
    bool no_data    = false;  ///< kein Datenfeld (nur ID-Feld)
    std::vector<uint8_t> data;   ///< `128 << n` Bytes; leer bei @ref no_data

    uint16_t size() const { return static_cast<uint16_t>(128u << (n > 6 ? 6 : n)); }
};

/// @brief Eine Spur: Lage, Verfahren, Datenrate, Sektoren in Spurreihenfolge.
struct TrackRecord {
    uint8_t  cyl = 0;
    uint8_t  head = 0;
    Encoding enc = Encoding::MFM;
    uint16_t rate_kbps = 250;    ///< Kanalrate 250 | 300 | 500 (wie TD0/IMD sie fuehren)
    std::vector<SectorRecord> sectors;   ///< leer = unformatierte Spur
};

/// @brief Kanalrate einer Spur aus Verfahren und Spurlaenge ableiten, wenn die Quelle
///        (.img/.hfe) keine fuehrt: MFM ab 8000 Byte je Umdrehung (8″ DD/HD) 500, sonst 250;
///        FM ab 4000 Byte (8″) 500, sonst 250.  300 kbit/s wird nie geraten.
uint16_t guessRate(Encoding enc, size_t track_bytes);

/**
 * @brief Kann das Medium als Sektorlisten ausgedrueckt werden, ohne Information zu verlieren?
 * @param why bei false: Grund mit Spurangabe
 */
bool representable(const DiskMedium& m, std::string& why);

/**
 * @brief Medium → Spurlisten (nur nicht-leere Spuren, aufsteigend nach Zylinder, Kopf).
 *
 * Liest ueber @ref DiskMedium::peek (laedt nie nach).  Prueft @ref representable.
 * @return false mit Grund in @p err
 */
bool fromMedium(const DiskMedium& m, std::vector<TrackRecord>& out, std::string& err);

/**
 * @brief Spurlisten → Medium (Geometrie = hoechster Zylinder/Kopf + 1).
 *
 * Spuren ohne Sektoren und nicht aufgefuehrte Spuren bleiben unformatiert.  Das Ergebnis
 * ist sauber (keine Dirty-Markierung).  Das Standardverfahren des Mediums ist das der
 * ersten Spur.  Abgelehnt wird ohne Aenderung von @p out: Daten, deren Laenge nicht
 * `128 << n` ist, Groessen-Code > 6, Zylinder/Kopf ausserhalb des Byte-Bereichs, Liste leer.
 */
bool toMedium(const std::vector<TrackRecord>& tracks, DiskMedium& out, std::string& err);

}  // namespace SectorTrackIo
