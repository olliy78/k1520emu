/**
 * @file gide_platte.h
 * @brief GidePlatte - ein Festplattenabbild der GIDE-Karte (IDE-Platte am K1520-Bus) mit CP/M-Laufwerken.
 *
 * Die GIDE-Karte (Entwurf 50) liefert nur den IDE-Zugang; WIE ein CP/M-Treiber die Platte aufteilt, legt der
 * **Gast-Treiber** fest.  Zwei Treiber sind bekannt und **untereinander unvertraeglich** (falscher Treiber =
 * Datenverlust); beide lassen sich aus ihren Quellen (`GIDEHP.ASM`, `gidn5004.mac`) genau nachbilden:
 *
 * | | `poppe` (H. Poppe; auch der im CP/A-BIOS eingebaute Treiber) | `kingstener` (DL, `gidn`) |
 * |---|---|---|
 * | IDE-Befehl 91h | nie (Vorgabegeometrie der Platte, 16 Koepfe x 63 Sektoren) | ja: 4 Koepfe x 64 Sektoren |
 * | Laufwerke | bis 8 (Kopf = Laufwerk, SDH = A0H+n), je 8 MB | 1 (8 MB) |
 * | LBA des Hostsektors | `(Spur*16 + Laufwerk)*63 + Hostsektor` (Hostsektor 0..31 je CP/M-Spur) | `Spur*64 + Satz` |
 * | 128-B-Satz im 512-B-Hostsektor | 4 Saetze je Hostsektor (Satz 4*s .. 4*s+3) | **ein** Satz je Hostsektor |
 * | Nutzbytes im Hostsektor | alle 512 | Bytes 384..511 (gelesen wird das LETZTE Viertel, geschrieben 4 mal dasselbe) |
 * | DPB | SPT 128, BSH 6, BLM 63, EXM 3, DSM 3E7H (3E3H am ersten Laufwerk), DRM 3FFH, AL0/AL1 FFH/00H, OFF 0 (2 am ersten) | SPT 64, BSH 5, BLM 1FH, EXM 1, DSM 7FDH, DRM 3FFH, AL0/AL1 FFH/00H, OFF 0 |
 *
 * Jedes Laufwerk wird als **linearer Sektorstrom** herausgeloest (`SectorSpace` im flachen Betrieb) und vom
 * vorhandenen @ref CpmFileSystem bearbeitet - die Dateisystemlogik wird nicht neu geschrieben.  Zurueck in das
 * Abbild geht nur, was sich gegenueber dem Stand beim Laden geaendert hat.
 *
 * ### Erkennung (nur lesend)
 * Das Layout steht nirgends auf der Platte.  Erkannt wird ueber das **Verzeichnis**: ein Poppe-Laufwerk ist
 * "formatiert", wenn sein Verzeichnis (1024 Eintraege) an der berechneten Stelle ausschliesslich plausible
 * Eintraege traegt; ein Kingstener-Abbild, wenn die ersten 256 Saetze plausibel sind UND in jedem Hostsektor die
 * vier Viertel gleich sind.  Passt genau eines, ist das Layout sicher; passen beide oder keines, wird **nicht
 * geraten** (Oeffnen mit `Auto` scheitert mit Begruendung; ausdruecklich gewaehlt wird geoeffnet, aber bei
 * mehrdeutigem Befund nur mit Hinweis).  Geschrieben wird nur an ein Laufwerk, dessen Verzeichnis im
 * gewaehlten Layout plausibel ist; `formatLaufwerk` verweigert, wo schon etwas anderes steht (ausser `erzwingen`).
 *
 * ### Sperre
 * Wie die Karte legt auch das Werkzeug `<abbild>.lock` an (Prozessnummer; Sperre eines toten Prozesses wird
 * uebernommen).  Steht eine fremde Sperre (laufender Emulator), ist das Abbild nur lesbar: Schreiben meldet
 * "Abbild gesperrt".
 *
 * @see doc/design/50_gun_gide.md, doc/merkposten/disktool.md (GIDE-Platten)
 */

#pragma once
#include "core/filesystem/cpm/cpm_fs.h"
#include "core/filesystem/file_system.h"
#include "core/filesystem/fs_profile.h"
#include "core/filesystem/sector_space.h"
#include "core/peripherals/floppy_drive/disk_format.h"
#include "core/util/gzip_datei.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/// @brief Aufteilung der Platte durch den Gast-Treiber.
enum class GideLayout : uint8_t { Auto, Poppe, Kingstener };

/// @brief "auto" | "poppe" | "kingstener".
const char* gideLayoutName(GideLayout l);
/// @brief Name -> Layout (Gross-/Kleinschreibung egal); false bei unbekanntem Namen.
bool gideLayoutVon(const std::string& name, GideLayout& aus);

/// Reine Abbildungen CHS <-> LBA nach den Gast-Treibern (Randfaelle: tests/unit/filesystem/test_gide_platte.cpp).
namespace gide_layout {

constexpr uint32_t kSektorBytes = 512;
constexpr uint32_t kRecordBytes = 128;

// -- poppe --
constexpr unsigned kPoppeKoepfe = 16;          ///< Vorgabegeometrie der Platte (ohne 91h)
constexpr unsigned kPoppeSektoren = 63;
constexpr unsigned kPoppeLaufwerke = 8;        ///< Kopf 0..7 (SDH = A0H + n)
constexpr unsigned kPoppeSpuren = 500;         ///< CP/M-Spuren je Laufwerk
constexpr unsigned kPoppeHostsektoren = 32;    ///< je CP/M-Spur (128 Saetze / 4)
constexpr unsigned kPoppeSystemspuren = 2;     ///< nur am ersten Laufwerk (OFF = 2)
constexpr uint64_t kPoppeMindestSektoren = 500ull * 16 * 63;   ///< 504000

/// @brief LBA eines Poppe-Hostsektors: Laufwerk @p lw (0..7), CP/M-Spur @p spur (0..499), Hostsektor 0..31.
inline uint64_t poppeLba(unsigned lw, unsigned spur, unsigned hostsektor) {
    return (static_cast<uint64_t>(spur) * kPoppeKoepfe + lw) * kPoppeSektoren + hostsektor;
}
/// @brief Umkehrung; false, wenn die LBA in keinem Laufwerk liegt (Kopf >= 8, Hostsektor >= 32, Spur >= 500).
bool poppeVonLba(uint64_t lba, unsigned& lw, unsigned& spur, unsigned& hostsektor);
/// @brief Die Sektor-/Zylinder-/Kopfwerte, die der Treiber ins Task-File schreibt (Sektor 1-basiert).
struct Chs { unsigned zylinder, kopf, sektor; };
inline Chs poppeChs(unsigned lw, unsigned spur, unsigned hostsektor) { return {spur, lw, hostsektor + 1}; }

// -- kingstener --
constexpr unsigned kKingKoepfe = 4;            ///< ueber 91h
constexpr unsigned kKingSektoren = 64;
constexpr unsigned kKingSpuren = 1023;         ///< DSM 7FDH: 2046 Bloecke zu 4 KB = 1023 Spuren zu 64 Saetzen
constexpr uint64_t kKingMindestSektoren = 1023ull * 64;   ///< 65472 (der Treiber verlangt >= 256 Zylinder = 65536)
constexpr unsigned kKingNutzOffset = 384;      ///< letztes Viertel des Hostsektors

/// @brief LBA des Hostsektors fuer Satz @p satz (0..63) der Spur @p spur (0..1022).
inline uint64_t kingLba(unsigned spur, unsigned satz) {
    return static_cast<uint64_t>(spur) * kKingSektoren + satz;
}
bool kingVonLba(uint64_t lba, unsigned& spur, unsigned& satz);
inline Chs kingChs(unsigned spur, unsigned satz) { return {spur / kKingKoepfe, spur % kKingKoepfe, satz + 1}; }

}  // namespace gide_layout

/// @brief Ein Laufwerk der Platte in der Auskunft.
struct GideLaufwerkInfo {
    int         index = 0;          ///< 0..7 (Kingstener: 0)
    std::string name;               ///< "lw0" ... - das Praefix der Pfade
    bool        formatiert = false; ///< Verzeichnis im Layout plausibel
    bool        gesperrt = false;   ///< Schreiben verweigert (Layout nicht sicher / Abbild gesperrt / schreibgeschuetzt)
    uint64_t    total = 0, used = 0, free = 0;   ///< Nutzbytes (nur formatiert)
    int         dateien = 0;
    int         systemspuren = 0;
};

/// @brief Befund der Layout-Erkennung.
struct GideErkennung {
    GideLayout layout = GideLayout::Auto;   ///< das benutzte Layout (Auto = keines bestimmbar)
    bool ausdruecklich = false;             ///< vom Anwender vorgegeben
    bool sicher = false;                    ///< eindeutig erkannt (oder vorgegeben UND nicht widersprochen)
    bool mehrdeutig = false;                ///< beide Layouts passen
    bool poppe_plausibel = false;
    bool king_plausibel = false;
    std::vector<int> poppe_laufwerke;       ///< formatierte Poppe-Laufwerke
    std::string text;                       ///< Begruendung in Klartext
};

class GidePlatte {
public:
    ~GidePlatte();
    GidePlatte(const GidePlatte&) = delete;
    GidePlatte& operator=(const GidePlatte&) = delete;

    /// @brief Kann die Datei (nach Groesse und Endung) eine GIDE-Platte sein?  Billig (liest bei `.gz` nur den Rumpf).
    static bool kandidat(const std::string& pfad);

    /**
     * @brief Abbild laden (roh oder `.gz`), Layout erkennen oder vorgeben.
     *
     * Mit @p read_only = false wird die Sperre `<abbild>.lock` angelegt; steht eine fremde, scheitert das
     * Oeffnen ("Abbild gesperrt").  @p layout `Auto`: nur bei eindeutigem Befund, sonst nullptr + Begruendung.
     */
    static std::unique_ptr<GidePlatte> open(const std::string& pfad, std::string& err, bool read_only = true,
                                            GideLayout layout = GideLayout::Auto);

    /// @brief Leeres (nur Nullen) Abbild im Layout anlegen; `.gz` gepackt.  Vorhandene Datei wird NIE ueberschrieben.
    ///        @p sektoren 0 = Mindestgroesse des Layouts.  Die Laufwerke sind danach unformatiert.
    static bool anlegen(const std::string& pfad, GideLayout layout, uint64_t sektoren, std::string& err);

    // -- Auskunft --
    GideLayout layout() const { return layout_; }
    const GideErkennung& erkennung() const { return erk_; }
    int laufwerkAnzahl() const;
    GideLaufwerkInfo laufwerkInfo(int lw) const;
    uint64_t sektorenGesamt() const { return img_.size() / gide_layout::kSektorBytes; }
    const std::string& path() const { return pfad_; }
    const std::string& lastError() const { return err_; }

    // -- Verzeichnis und Dateien (Pfade `lw0/NAME.EXT`, Nutzerbereich `lw0/3:NAME.EXT`) --
    /// @brief Alle Dateien der formatierten Laufwerke; `volume` = Laufwerk, `name` = voller Pfad.
    std::vector<FileEntry> list();
    bool read(const std::string& pfad, std::vector<uint8_t>& out);
    bool write(const std::string& pfad, const std::vector<uint8_t>& data, const WriteOptions& opt);
    bool erase(const std::string& pfad);
    /// @brief Datei(en) nach @p dest_dir/lwN/NAME holen; @p pfad leer = alle; `lw0` = ein ganzes Laufwerk.
    bool extract(const std::string& pfad, const std::string& dest_dir, bool overwrite);
    /// @brief Linux-Datei als @p ziel (`lw0/NAME.EXT`) einfuegen.
    bool insert(const std::string& quelle, const std::string& ziel, bool overwrite);
    /// @brief Standardlaufwerk fuer Pfade ohne `lwN/` (-1 = keines; Kingstener: immer 0).
    void setStandardLaufwerk(int lw) { std_lw_ = lw; }

    // -- Pruefung, Formatieren --
    FsCheckReport check(FsCheckLevel level);
    /// @brief Verzeichnis eines Laufwerks leeren (0xE5), wie ERADIR/quickformat.  Nur bei ausdruecklich gewaehltem
    ///        Layout und schreibbarem Abbild; wo nicht bloss Nullen oder ein leeres Verzeichnis stehen, nur mit @p erzwingen.
    bool formatLaufwerk(int lw, bool erzwingen = false);

    // -- Bindung an die Datei --
    bool readOnly() const { return read_only_; }
    /// @brief Schreibschutz aendern; Aufheben legt die Sperre an (false + Text, wenn gesperrt / Layout unsicher).
    bool setReadOnly(bool ro);
    void setBackup(bool b) { backup_ = b; }
    bool dirty() const;
    /// @brief Anderungen in das Abbild und die Datei zurueckschreiben (gz: ganz, atomar; roh: nur geaenderte Sektoren).
    bool flush();
    /// @brief Fremde Sperre vorhanden (ein Emulator hat das Abbild geladen)?
    bool fremdGesperrt() const { return fremd_gesperrt_; }

    /// @brief Das Abbild selbst (Tests): Bytes des Hostsektors @p lba.
    const std::vector<uint8_t>& abbild() const { return img_; }

    /// @brief Auf Plausibilitaet pruefen: 32-Byte-Eintraege (Verzeichnis eines CP/M-Laufwerks).
    ///        @p dsm_bloecke = Blockzahl (Zeiger duerfen nicht darueber liegen).  belegt = Anzahl belegter Eintraege.
    static bool verzeichnisPlausibel(const uint8_t* d, size_t eintraege, unsigned dsm_bloecke, int* belegt = nullptr);

private:
    GidePlatte() = default;

    struct Laufwerk {
        int index = 0;
        std::vector<uint8_t> strom, orig;
        DiskFormat fmt;
        FsProfile prof;
        std::unique_ptr<SectorSpace> raum;
        std::unique_ptr<CpmFileSystem> fs;
        uint64_t gespeichert_rev = 0;
    };

    bool fail(const std::string& t) { err_ = t; return false; }
    bool laden(const std::string& pfad, std::string& err);
    void erkennen(GideLayout vorgabe);
    bool laufwerkPlausibel(GideLayout l, int lw, int* belegt) const;
    bool kingViertelGleich(uint64_t lba) const;
    uint64_t stromBytes(GideLayout l) const;
    bool lwVorhanden(GideLayout l, int lw) const;
    void stromLaden(GideLayout l, int lw, std::vector<uint8_t>& strom) const;
    bool stromZurueck(Laufwerk& l);   ///< geaenderte Teile in img_ eintragen + schmutzig_ vermerken
    Laufwerk* oeffne(int lw);
    bool schreibErlaubt(int lw);
    bool zerlege(const std::string& pfad, int& lw, std::string& name);
    bool sperreAnlegen(std::string& err);
    void sperreLoesen();
    static void formatFuer(GideLayout l, int lw, DiskFormat& fmt, FsProfile& prof);

    std::string pfad_;
    std::vector<uint8_t> img_;
    k1520::gzip::Art art_ = k1520::gzip::Art::Roh;
    GideLayout layout_ = GideLayout::Auto;
    GideErkennung erk_;
    std::vector<std::unique_ptr<Laufwerk>> lws_;   ///< Index = Laufwerk, nullptr = noch nicht geladen
    std::vector<char> formatiert_;                  ///< je Laufwerk: Verzeichnis plausibel (nach formatLaufwerk wahr)
    std::vector<uint64_t> schmutzig_;               ///< geaenderte LBAs (roh: nur diese werden geschrieben)
    bool read_only_ = true;
    bool backup_ = true, backup_getan_ = false;
    bool fremd_gesperrt_ = false;
    std::string sperre_;
    int std_lw_ = -1;
    std::string err_;
};
