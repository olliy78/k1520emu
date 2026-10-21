/**
 * @file fileinfo.h
 * @brief Die `.fileinfo`-Angabendatei (je Datei eine, `schluessel=wert` je Zeile).
 *
 * Gemeinsame Einheit von @ref DiskVolume (Disketten) und @ref PlattenVolume (P8000-Platte):
 * Lesen, Schreiben und die Abbildung UNIX-Inode <-> Zeilen.  Das Format ist ein Vertrag
 * (`fs=udos|cpm|wega|tar`) und bleibt bitgleich.
 *
 * @see doc/design/13_k1520disktool.md §2.1, doc/design/37_tar_satz_und_platte.md
 */

#pragma once
#include "core/filesystem/file_system.h"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>

namespace k1520::fileinfo {

constexpr const char* kFileinfoEndung = ".fileinfo";

struct UdosAngabe {
    std::string typ, eigenschaften, erstellt, geaendert;
    uint16_t    start = 0, satzlaenge = 0, ladeadresse = 0, abbildlaenge = 0;
    uint16_t    speicher_von = 0, speicher_bis = 0, speicher_kz = 0;
    uint16_t    blocklaenge = 0, rest = 0;
    uint32_t    zusatz = 0;
    /// @brief ALLE Speichersegmente, mit Komma getrennt (`4400+0041,8442+0026`).
    ///        Steht nur bei mehr als einem Segment in der Zeile — sonst genuegt `segment=`.
    std::string segmente;
    /// @name Steht der Wert im Beiblatt?  (0 ist ueberall eine gueltige ANGABE)
    /// @{
    bool        blocklaenge_gesetzt = false;
    bool        rest_gesetzt        = false;
    bool        mem_gesetzt         = false;
    /// @}
};

struct CpmAngabe {
    std::string name;                ///< der ECHTE CP/M-Name, ggf. "3:NAME.TYP"
    bool read_only = false, system = false, archived = false;
};

struct UnixAngabe {
    std::string art = "-";      ///< "-" Datei · "d" Verzeichnis · "c"/"b" Geraet · "p" FIFO
    uint16_t    mode = 0;       ///< di_mode samt Dateiart
    bool        mode_da = false;
    int         uid = 0, gid = 0;
    bool        owner_da = false;
    uint32_t    mtime = 0, atime = 0, ctime = 0;
    int         nlink = 0;
    uint32_t    inode = 0;
    std::string rdev;           ///< "major,minor" nur bei Geraetedateien
};

/**
 * @struct FileAngaben
 * @brief Die Vereinigung von @ref UdosAngabe und @ref CpmAngabe samt Herkunft.
 *
 * `fs=` steht in der ersten Zeile und ist kein Zierat: ein `.fileinfo` einer
 * CP/M-Datei darf nicht stillschweigend als UDOS-Angabe gelesen werden.  Passt es
 * nicht zum Ziel, ist das eine Meldung und keine Vorgabe.
 */
struct FileAngaben {
    bool        udos = false;   ///< `fs=udos` (ZDOS/NDOS) statt `fs=cpm`
    bool        wega = false;   ///< `fs=wega` (UNIX-Inode); schliesst @ref udos aus
    bool        tar  = false;   ///< `fs=tar` (UNIX-Angaben aus einem tar-Kopf); schliesst @ref udos aus
    std::string teil;           ///< tar: "ist/soll", wenn die Datei abgeschnitten ist
    std::string name;           ///< der ECHTE Name auf der Diskette ("3:SYSTEM.COM")
    UdosAngabe  u;
    CpmAngabe   c;
    UnixAngabe  x;
};

std::string hex16(uint16_t v);
/// @brief Leerzeichen im Erstellungsvermerk ("V 4.3 ") sind im Beiblatt `_`.
std::string ohneLeer(std::string t);
std::string mitLeer(std::string t);
std::string cpmAttrText(const CpmAngabe& a);
std::string modeOktal(uint16_t m);
/// @brief Die UNIX-Angaben eines Verzeichniseintrags (WEGA/tar).
UnixAngabe unixAus(const FileEntry& e);
/// @brief Die Zeilen `art=` ... `rdev=` — gemeinsam fuer `.fileinfo` und Beiblatt.
void schreibeUnixFelder(std::ostream& f, const UnixAngabe& a, char trenner);
/// @brief Einen Schluessel dieser Familie in @p a eintragen; false = kein WEGA-Schluessel.
bool leseUnixFeld(UnixAngabe& a, const std::string& k, const std::string& v);
/// @brief Ist @p name ein `.fileinfo`?  (Endungsvergleich ohne Ruecksicht auf Gross/Klein)
bool istFileinfo(const std::filesystem::path& name);
/// @brief Pfad des `.fileinfo` zu @p datei — die Endung schlicht angehaengt.
std::filesystem::path fileinfoPfad(const std::filesystem::path& datei);
/// @brief Ein `.fileinfo` schreiben.
bool schreibeFileinfo(const std::filesystem::path& ziel, const FileAngaben& a);
/// @brief Ein `.fileinfo` lesen; gibt es keines, kommt `false` (kein Fehler).
bool leseFileinfo(const std::filesystem::path& datei, FileAngaben& out);

}  // namespace k1520::fileinfo
