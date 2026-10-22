/**
 * @file td0_codec.h
 * @brief Td0Codec – TeleDisk-Abbild (`.td0`) ⇄ @ref DiskMedium (AP T1: Lesen).
 *
 * TeleDisk (Sydex) fuehrt wie ImageDisk je Spur eine Sektorliste (C/H/R/N, Flags, Daten),
 * nicht den Bitstrom.  Aufbau, CRC-Regeln, Methoden und die Kompression der Kennung `td`
 * ("Advanced Compression", LZSS + adaptives Huffman): Clean-Room-Beschreibung
 * `doc/design/38a_td0_imd_formatbeschreibung.md` Teil 2.
 *
 * Gelesen wird ueber die Zwischenschicht @ref SectorTrackIo (Spurlisten → Medium); Reihenfolge
 * (Interleave), doppelte Sektornummern, geloeschte Marken, Datenfehler (Flag 0x02) und
 * Sektoren ohne Datenfeld bleiben erhalten.  Die ID-Felder (C/H/R/N) stammen aus dem
 * Sektorkopf, nie aus dem Spurkopf.
 *
 * Lesen ist **streng bei der Struktur, nachsichtig bei Sektordaten**:
 *   - harte Fehler (Laden scheitert mit Grund): falsche Kennung, Kopf-CRC, Spur-CRC,
 *     unbekannte Datenrate, Kopf > 1, Sektorgroessen-Code > 6 bei vorhandenem Datenblock,
 *     unbekannte Methode, entpackte Laenge != Sektorgroesse, abgeschnittene Datei
 *     (auch fehlende Endemarke), leere Datei, Datei ohne Spursatz;
 *   - nur Warnung (@ref SourceInfo::warnings): Kommentar-CRC, Sektor-CRC8 (ueber die
 *     entpackten Daten), doppelte Spursaetze (der letzte gilt), Pseudosektoren (Flag 0x40,
 *     Daten ohne ID-Feld: sie kommen NICHT ins Medium), Sektoren mit DOS-Allocation-Flag
 *     (0x10) und Musterlaenge `2*c` mit c > 1 bei Methode 2 (nur aus der Quelle belegt).
 * Die 3 Bytes hinter der Endemarke (0xFF) werden nicht ausgewertet.  Bei `td` wird bis zum
 * Ende des Eingabestroms entpackt; das Padding am Stromende ist ohne Folgen.
 *
 * Schreiben (@ref save / @ref encode) folgt in AP T2 und scheitert bis dahin mit Begruendung.
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
 * @namespace Td0Codec
 * @brief Laden von TeleDisk-Abbildern (Schreiben folgt).
 */
namespace Td0Codec {

/// @brief Kopf eines Spursatzes, wie die Datei ihn fuehrt (fuer das spaetere Zurueckschreiben).
struct TrackHead {
    uint8_t cyl = 0;
    uint8_t head = 0;       ///< Seite (Bit 0 des Kopfbytes)
    bool    fm = false;     ///< Spur-FM-Bit (Bit 7 des Kopfbytes), OHNE das globale FM-Bit
};

/**
 * @struct SourceInfo
 * @brief Metadaten der geladenen Datei (Kopf + Befund des Ladens).
 */
struct SourceInfo {
    bool     advanced = false;      ///< Kennung `td` (Advanced Compression)
    uint8_t  volume_seq = 0;        ///< Byte 2: Volume-Folgenummer
    uint8_t  check_seq = 0;         ///< Byte 3: Pruefwert der Mehrdisketten-Folge
    uint8_t  version = 0;           ///< Byte 4: Teledisk-Version (0x15 = 1.5)
    uint16_t rate_kbps = 250;       ///< Kanalrate laut Kopf (250 | 300 | 500)
    bool     fm_global = false;     ///< Bit 7 der Datenrate: gesamte Diskette FM
    uint8_t  drive_type = 0;        ///< Byte 6: 1=360K 2=1,2M 3=720K 4=1,44M 5=8" 6=3,5"
    uint8_t  stepping = 0;          ///< Byte 7 ohne Kommentarbit: 0 einfach, 1 doppelt, 2 gerade Spuren
    bool     has_comment = false;   ///< Byte 7 Bit 7
    bool     dos_allocation = false;///< Byte 8 != 0: nur FAT-belegte Sektoren gelesen
    uint8_t  sides = 2;             ///< Byte 9: 1 oder 2 (jeder andere Wert zaehlt als 2)
    std::string comment;            ///< Kommentartext, Zeilen mit '\n' getrennt, ohne abschliessende Zeilenenden
    std::string timestamp;          ///< "TT.MM.JJJJ hh:mm:ss" (leer ohne Kommentarblock)
    uint8_t  stamp[6] = {0,0,0,0,0,0};   ///< Jahr-1900, Monat (0-basiert), Tag, Stunde, Minute, Sekunde wie gelesen
    std::vector<uint8_t> raw_header;     ///< die 12 Kopfbytes wie gelesen (Vorlage fuer das Schreiben)
    std::vector<uint8_t> comment_raw;    ///< Kommentarbytes wie gelesen (ohne Kuerzung)
    std::vector<TrackHead> heads;        ///< je Spursatz (auch leere), eindeutig je Zylinder/Kopf
    size_t tracks          = 0;   ///< Spursaetze (ohne ersetzte Doppelte)
    size_t empty_tracks    = 0;   ///< davon ohne Sektoren
    size_t sectors         = 0;   ///< Sektoren im Medium (ohne Pseudosektoren)
    size_t no_data_sectors = 0;   ///< davon ohne Datenfeld (Flag 0x10 / 0x20)
    size_t data_error_sectors = 0;///< Flag 0x02
    size_t deleted_sectors = 0;   ///< Flag 0x04
    size_t pseudo_sectors  = 0;   ///< Flag 0x40: Daten ohne ID-Feld, nicht uebernommen
    size_t duplicate_ids   = 0;   ///< Sektoren, deren Nummer in der Spur schon vorkam
    size_t crc8_mismatch   = 0;   ///< Sektor-CRC8 passt nicht zu den entpackten Daten
    size_t trailing_bytes  = 0;   ///< Bytes hinter der Endemarke (ohne die 3 Rest-Bytes)
    std::vector<std::string> warnings;
};

/// @brief CRC-16 der Kennung (Poly 0xA097, Start 0, MSB zuerst, kein Endxor).  CRC("123456789") = 0x0FB3.
uint16_t crc16(const uint8_t* data, size_t n);

/**
 * @brief Sieht @p path wie ein TeleDisk-Abbild aus?  (`TD`/`td` UND Kopf-CRC — ein rohes
 *        Sektorimage darf mit "TD" beginnen)
 */
bool isTd0(const std::string& path);

/// @brief Wie @ref isTd0, fuer einen Speicherpuffer (die ersten 12 Bytes genuegen).
bool isTd0(const uint8_t* data, size_t n);

/**
 * @brief Strom der Kennung `td` entpacken (LZSS + adaptives Huffman nach Okumura/LZHUF).
 * @param packed Eingabe (hinter dem 12-Byte-Kopf)
 * @param out    Ausgabe; am Stromende kann Padding stehen (Strom ist nicht markiert)
 * @return false mit Grund, wenn die Ausgabe unplausibel gross wuerde
 */
bool unpack(const uint8_t* packed, size_t n, std::vector<uint8_t>& out, std::string& err);

/// @brief `.td0` vollstaendig in @p out laden.
bool load(const std::string& path, DiskMedium& out, SourceInfo* info, std::string& err);

/// @brief Wie @ref load, aus einem Speicherpuffer (Tests, Archivinhalte).
bool loadBuffer(const std::vector<uint8_t>& datei, DiskMedium& out, SourceInfo* info,
                std::string& err);

/**
 * @brief Datei in Spurlisten zerlegen (ohne Medium) — auch leere Spuren, in Dateireihenfolge,
 *        doppelte Spursaetze bereits ersetzt, Pseudosektoren entfernt.
 */
bool decodeBuffer(const std::vector<uint8_t>& datei,
                  std::vector<SectorTrackIo::TrackRecord>& tracks,
                  SourceInfo* info, std::string& err);

/// @brief Ein Feld der Metadaten als Text (version, timestamp, comment, rate, tracks, sectors,
///        warnings); unbekannter Schluessel = "".
std::string infoField(const SourceInfo& si, const std::string& key);

/// @brief Schreiben folgt in AP T2: scheitert immer mit Begruendung.
bool encode(const DiskMedium& in, const SourceInfo* vorlage, std::vector<uint8_t>& out,
            SourceInfo* geschrieben, std::string& err);

/// @brief Schreiben folgt in AP T2: scheitert immer mit Begruendung, die Zieldatei bleibt unberuehrt.
bool save(const std::string& path, const DiskMedium& in, const SourceInfo* vorlage,
          SourceInfo* geschrieben, std::string& err);

/// @brief Wie oben ohne Vorlage.
bool save(const std::string& path, const DiskMedium& in, std::string& err);

}  // namespace Td0Codec
