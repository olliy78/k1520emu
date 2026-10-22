/**
 * @file imd_codec.h
 * @brief ImdCodec – ImageDisk-Abbild (`.imd`) ⇄ @ref DiskMedium (Lesen und Schreiben).
 *
 * ImageDisk (Dave Dunfield) fuehrt je Spur eine Sektorliste statt eines Bitstroms:
 * Modus (FM/MFM + Kanalrate), Zylinder, Kopf, Sektorzahl, EINE Sektorgroesse, die
 * Numerierungsfolge in Spurreihenfolge (= Interleave), optionale Zylinder-/Kopf-Maps
 * und je Sektor einen Typ (fehlend / normal / geloescht / Datenfehler, jeweils auch
 * "komprimiert" = alle Bytes gleich).  Aufbau, Typcodes und Modi: Clean-Room-Beschreibung
 * `doc/design/38a_td0_imd_formatbeschreibung.md` Teil 1.
 *
 * Alles laeuft ueber die Zwischenschicht @ref SectorTrackIo: gelesen wird in Spurlisten
 * (@ref SectorTrackIo::TrackRecord) und von dort ueber @ref TrackCodec::buildTrack ins
 * Medium; Reihenfolge, geloeschte Marken, absichtliche Daten-CRC-Fehler und Sektoren
 * ohne Datenfeld bleiben dabei erhalten.
 *
 * Lesen ist **streng**: falsche Kennung, fehlendes 0x1A, unbekannter Modus, Sektorcode
 * > 6, Kopf > 1, Typ > 8 oder eine mitten im Satz abgeschnittene Datei lassen das Laden
 * mit Begruendung scheitern.  Ein Rest von weniger als 5 Bytes hinter dem letzten Satz
 * wird mit Warnung toleriert, ein doppelter Spursatz (gleicher Zylinder/Kopf) ersetzt
 * den frueheren (ebenfalls mit Warnung).  **Sektoren ueber 1024 Byte** (Code 4..6) kennt
 * das Medium noch nicht (@ref TrackCodec) und werden mit Begruendung abgelehnt; die
 * Sektor-ID-Felder von Spuren ohne Sektoren (n = 0) sind bedeutungslos.
 *
 * Schreiben (@ref save, @ref encode): ein Medium, das sich als Sektorliste darstellen
 * laesst (@ref SectorTrackIo::representable).  Abgelehnt, ohne die Zieldatei anzufassen,
 * wird mit Begruendung: Daten hinter der Daten-CRC (UDOS-Kontrollblock), **gemischte
 * Sektorgroessen in einer Spur** (IMD fuehrt nur eine), ein ID-CRC-Fehler (IMD kennt keinen)
 * und ein unformatiertes Medium.  Spuren ohne Sektoren innerhalb der Mediumsgeometrie
 * werden als Satz mit n = 0 geschrieben, damit die Geometrie beim Wiederladen gleich
 * bleibt.  Geschrieben wird zuerst im Speicher, durch den eigenen Leser gegengeprueft,
 * dann in `<datei>.k1520tmp` und umbenannt (nie ein Teilzustand).
 *
 * Kopf: Eine @ref SourceInfo der Quelle ueberlebt als Vorlage — Kopfzeile (Programm,
 * Datum) und Kommentar bleiben bytegleich, und der Modus (Kanalrate!) jeder Spur, den
 * das Medium selbst nicht fuehrt (z. B. 300 kbit/s), wird fuer Spuren gleichen Verfahrens
 * wieder ausgegeben.  Ohne Vorlage wird die Rate aus Verfahren und Spurlaenge abgeleitet
 * (@ref SectorTrackIo::guessRate) und der Kopf lautet
 * `IMD 1.17: TT/MM/JJJJ hh:mm:ss` + Kommentar "Image file created by k1520emu".
 *
 * @see doc/design/38_td0_imd.md, doc/design/38a_td0_imd_formatbeschreibung.md
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#pragma once
#include "disk_medium.h"
#include "sector_track_io.h"
#include <cstdint>
#include <string>
#include <vector>

/**
 * @namespace ImdCodec
 * @brief Laden und Schreiben von ImageDisk-Abbildern.
 */
namespace ImdCodec {

/// @brief Modus (Byte 0 eines Spursatzes) einer Spur, wie die Datei ihn fuehrt.
struct TrackMode {
    uint8_t cyl  = 0;
    uint8_t head = 0;
    uint8_t mode = 0;   ///< 0..2 FM 500/300/250, 3..5 MFM 500/300/250
};

/**
 * @struct SourceInfo
 * @brief Metadaten der geladenen Datei (Kopf + Befund des Ladens).
 */
struct SourceInfo {
    std::string version;       ///< "1.17" (aus der Kopfzeile, leer wenn nicht lesbar)
    std::string timestamp;     ///< "22/10/2026 12:00:00" (Text der Kopfzeile hinter dem Doppelpunkt)
    std::string comment;       ///< Kommentar ohne Kopfzeile, ohne abschliessende Zeilenenden
    std::vector<uint8_t> raw_header;   ///< Kopfzeile + Kommentar wie gelesen, OHNE die 0x1A (Vorlage fuer @ref save)
    std::vector<TrackMode> modes;      ///< je Spursatz (auch leere), eindeutig je Zylinder/Kopf
    size_t tracks         = 0;   ///< Spursaetze (ohne ersetzte Doppelte)
    size_t empty_tracks   = 0;   ///< davon ohne Sektoren (n = 0)
    size_t sectors        = 0;   ///< Sektoren insgesamt
    size_t trailing_bytes = 0;   ///< Rest hinter dem letzten Satz (< 5 Bytes, toleriert)
    std::vector<std::string> warnings;
};

/// @brief Modus → Verfahren und Kanalrate (kbit/s); false bei Modus > 5.
bool modeToEncoding(uint8_t mode, Encoding& enc, uint16_t& rate_kbps);

/// @brief Verfahren + Kanalrate → Modus (FM: 500→0, 300→1, sonst 2; MFM: 500→3, 300→4, sonst 5).
uint8_t encodingToMode(Encoding enc, uint16_t rate_kbps);

/**
 * @brief Sieht @p path wie ein ImageDisk-Abbild aus?  (Kennung `IMD ` UND ein 0x1A in den
 *        ersten 64 KiB — ein roher Datentraeger beginnt selten so)
 */
bool isImd(const std::string& path);

/// @brief Wie @ref isImd, fuer einen Speicherpuffer (maximal die ersten 64 KiB werden betrachtet).
bool isImd(const uint8_t* data, size_t n);

/// @brief `.imd` vollstaendig in @p out laden.
bool load(const std::string& path, DiskMedium& out, SourceInfo* info, std::string& err);

/// @brief Wie @ref load, aus einem Speicherpuffer (Tests, Archivinhalte).
bool loadBuffer(const std::vector<uint8_t>& datei, DiskMedium& out, SourceInfo* info,
                std::string& err);

/**
 * @brief Datei in Spurlisten zerlegen (ohne Medium) — auch leere Spuren (n = 0), in
 *        Dateireihenfolge, doppelte Spursaetze bereits ersetzt.
 */
bool decodeBuffer(const std::vector<uint8_t>& datei,
                  std::vector<SectorTrackIo::TrackRecord>& tracks,
                  SourceInfo* info, std::string& err);

/// @brief Ein Feld der Metadaten als Text (version, timestamp, comment, tracks, sectors,
///        warnings); unbekannter Schluessel = "".
std::string infoField(const SourceInfo& si, const std::string& key);

/**
 * @brief Medium als ImageDisk-Datei im Speicher aufbauen (nichts wird geschrieben).
 * @param vorlage     Kopf/Modi der Quelldatei (darf nullptr sein)
 * @param out         Ausgabe: die vollstaendige Datei
 * @param geschrieben Ausgabe (optional): Metadaten der erzeugten Datei, wie ein Leser sie sieht
 * @return false mit Begruendung in @p err, wenn das Medium nicht darstellbar ist
 */
bool encode(const DiskMedium& in, const SourceInfo* vorlage, std::vector<uint8_t>& out,
            SourceInfo* geschrieben, std::string& err);

/// @brief Medium als @p path schreiben (erst Nachbardatei, dann umbenennen); siehe @ref encode.
bool save(const std::string& path, const DiskMedium& in, const SourceInfo* vorlage,
          SourceInfo* geschrieben, std::string& err);

/// @brief Wie oben ohne Vorlage (Standardkopf).
bool save(const std::string& path, const DiskMedium& in, std::string& err);

}  // namespace ImdCodec
