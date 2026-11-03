/**
 * @file fat_fs.h
 * @brief FAT12 — das MS-DOS-Dateisystem der 5,25"/3,5"-Disketten (PC, A7150/DCP, PC 1750 …).
 *
 * Gelesen und geschrieben wird, was ein PC-DOS oder Windows auf einer Diskette anlegt:
 *
 *   * **Bootsektor mit BPB** (DOS 2.0 ohne, 3.x, 4.x–6.x mit erweitertem BPB, FAT-Kennung
 *     „FAT12   ") — und BPB-lose Disketten der DOS-Fruehzeit (160K/180K/320K/360K), deren
 *     Geometrie nur die Medienkennung im ersten FAT-Byte verraet,
 *   * **FAT12** mit beliebig vielen FAT-Kopien, Wurzelverzeichnis fester Groesse,
 *     **Unterverzeichnisse** (beliebig tief, `.`/`..`), Attribute (R/O, versteckt, System,
 *     Archiv), Datum und Uhrzeit,
 *   * **lange Dateinamen** (VFAT, Windows 95 und neuer) lesend UND schreibend,
 *     samt der NT-Kleinschreibbits,
 *   * Datentraegername (Wurzeleintrag und erweiterter BPB),
 *   * Pruefung und Reparatur (FAT-Kopien, verlorene Cluster, Querverweise, Ketten, Groessen).
 *
 * Das Dateisystem liegt **byte-linear** ueber dem @ref SectorSpace: logischer Sektor n =
 * Byte n·512 der linearen Sektorfolge (Zylinder-major, Kopf-minor — so nummeriert DOS).
 * Die Sektorgroesse der FAT (BPB) ist unabhaengig von der physischen.
 *
 * Namen sind Pfade mit `/` (wie bei WEGA); @ref list liefert den ganzen Baum samt Verzeichnissen
 * (`type == "d"`).  Gross-/Kleinschreibung ist beim Suchen egal, beim Anzeigen gilt der
 * gespeicherte (lange) Name.
 *
 * @see doc/design/44_fat12.md
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#pragma once
#include "core/filesystem/file_system.h"
#include "core/filesystem/fs_profile.h"
#include "core/filesystem/sector_space.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fat {

constexpr uint8_t kAttrRo   = 0x01;
constexpr uint8_t kAttrHid  = 0x02;
constexpr uint8_t kAttrSys  = 0x04;
constexpr uint8_t kAttrVol  = 0x08;
constexpr uint8_t kAttrDir  = 0x10;
constexpr uint8_t kAttrArc  = 0x20;
constexpr uint8_t kAttrLfn  = 0x0F;     ///< R/O+Hid+Sys+Vol = Eintrag fuer einen langen Namen

/// @brief Der BIOS Parameter Block samt den abgeleiteten Lagen.
struct Bpb {
    uint16_t bps    = 512;      ///< Bytes je Sektor
    uint8_t  spc    = 2;        ///< Sektoren je Cluster
    uint16_t rsv    = 1;        ///< reservierte Sektoren (Bootsektor)
    uint8_t  nfats  = 2;
    uint16_t rde    = 112;      ///< Eintraege im Wurzelverzeichnis
    uint32_t total  = 1440;     ///< Sektoren insgesamt
    uint8_t  media  = 0xF9;
    uint16_t spf    = 3;        ///< Sektoren je FAT
    uint16_t spt    = 9;
    uint16_t heads  = 2;
    uint32_t hidden = 0;

    /// @name abgeleitet (@ref berechne)
    /// @{
    uint32_t fat_start  = 1;    ///< erster Sektor der ersten FAT
    uint32_t root_start = 0;
    uint32_t root_secs  = 0;
    uint32_t data_start = 0;
    uint32_t clusters   = 0;    ///< Datencluster (Nummern 2 … clusters+1)
    /// @}

    bool     mit_bpb   = true;  ///< false: BPB-lose Diskette, Werte aus Geometrie/Medienkennung
    bool     erweitert = false; ///< Kennung 0x29 bei Offset 38 (Seriennummer, Name, Typ)
    std::string oem;            ///< Offset 3, 8 Zeichen (getrimmt)

    uint32_t clusterBytes() const { return static_cast<uint32_t>(bps) * spc; }
    /// @brief Fehlerfrei?  Ergaenzt die abgeleiteten Felder; @p warum nennt sonst den Grund.
    bool berechne(std::string* warum);
};

/// @brief Standardlage einer PC-Diskette aus Gesamtgroesse in Bytes; false = unbekannte Groesse.
bool standardBpb(uint64_t bytes, uint16_t heads, uint16_t spt, Bpb& out);

/// @brief Zeitstempel (Ortszeit-Sekunden seit 1970) <-> DOS-Datum/Uhrzeit.
int64_t dosZuEpoche(uint16_t datum, uint16_t zeit);
void    epocheZuDos(int64_t t, uint16_t& datum, uint16_t& zeit);
int64_t jetztOrtszeit();
/// @brief Echte (UTC-)Sekunden -> „Uhr wie sie zeigte" (Ortszeit) und zurueck.  DOS speichert keine Zeitzone.
int64_t utcZuOrtszeit(int64_t utc);
int64_t ortszeitZuUtc(int64_t ort);

/// @brief Pruefsumme des 8.3-Namens fuer die Eintraege langer Namen.
uint8_t kurznamenSumme(const uint8_t* n11);

} // namespace fat

/// @brief Wie eine neue FAT-Diskette angelegt wird.
struct FatFormatOptions {
    std::string oem   = "MSWIN4.1";   ///< Kennung bei Offset 3 (Windows 9x; schaltet die langen Namen ein)
    uint32_t    serial = 0;           ///< 0 = aus der Uhrzeit
    int         lfn    = -1;          ///< vorgemerkt fuer WriteOptions::fat_lfn
};

/**
 * @class FatFileSystem
 * @brief Ein FAT12-Datentraeger auf einem @ref SectorSpace.
 */
class FatFileSystem : public FileSystem {
public:
    /// @brief Traegt die Diskette ein FAT12-Dateisystem?  Aendert nichts.
    static bool looksLikeFat(const SectorSpace& raum, std::string* warum);

    /// @brief Eine vorhandene FAT12-Diskette oeffnen (liest Bootsektor und FAT).
    static std::unique_ptr<FatFileSystem> mount(SectorSpace& raum, const FsProfile& prof,
                                                std::string& warum);

    /**
     * @brief Eine leere FAT12-Diskette anlegen: Bootsektor (Standard-BPB der Geometrie, Kennung
     *        „Non-System disk"), FAT-Kopien, leeres Wurzelverzeichnis.
     * @param label Datentraegername (hoechstens 11 Zeichen, Grossbuchstaben); leer = keiner
     */
    static std::unique_ptr<FatFileSystem> format(SectorSpace& raum, const FsProfile& prof,
                                                 const std::string& label, std::string& err,
                                                 const FatFormatOptions& opt = {});

    /**
     * @brief Standard-BPB fuer die Geometrie des Raums (160K … 2,88M); false = keine bekannte Lage.
     */
    static bool bpbFuerRaum(const SectorSpace& raum, fat::Bpb& out, std::string* warum = nullptr);

    /// @brief Den eingebauten Bootsektor „Non-System disk or disk error" fuer @p bpb bauen (512 B).
    static std::vector<uint8_t> leererBootsektor(const fat::Bpb& bpb, const std::string& label,
                                                 const FatFormatOptions& opt);

    /**
     * @brief Ein Bootsektor-Abbild (Code) mit dem BPB DIESER Diskette versehen.
     *
     * Der Code kommt aus @p bild (512 B, z. B. aus `data/bootsektoren/`), die Parameter dieser
     * Diskette (Sektorgroesse, Cluster, FAT-Lage, Gesamtgroesse, Medienkennung, Spuren/Koepfe,
     * Seriennummer, Name) bleiben erhalten: ein Bootsektor einer 720K-Diskette taugt so auch fuer
     * 360K oder 1,44M.  Die Bytes zwischen BPB und Programmanfang (Sprungziel) gehoeren dem
     * Urlader und bleiben, wie sie im Bild stehen.
     *
     * @return false mit @p err, wenn @p bild kein Bootsektor ist (Sprung, Kennung 55AA, BPB-Block)
     */
    static bool bootsektorMitBpb(const std::vector<uint8_t>& bild, const fat::Bpb& dieser,
                                 const std::vector<uint8_t>& aktuell,
                                 std::vector<uint8_t>& out, std::string& err);

    // ─── FileSystem ──────────────────────────────────────────────────────────
    std::vector<FileEntry> list() const override;
    bool read(const std::string& name, std::vector<uint8_t>& out) override;
    bool write(const std::string& name, const std::vector<uint8_t>& data,
               const WriteOptions& opt) override;
    bool erase(const std::string& name) override;
    bool makeDirectory(const std::string& name) override;
    using FileSystem::setAttributes;
    bool setAttributes(const std::string& name, const FatAttrs& a) override;
    bool wouldFit(const std::vector<PlannedFile>& files, FitReport& out) const override;
    bool mkfs() override;
    FsInfo info() const override;
    bool firstSector(const std::string& name, FsRecoverOrt& out) const override;
    std::string blockList(const std::string& name) const override;

    FsCheckReport check(FsCheckLevel level, bool nachladen) const override;
    bool repair(const FsRepair& r) override;

    // ─── FAT-eigen ───────────────────────────────────────────────────────────
    const fat::Bpb& bpb() const { return bpb_; }

    /// @brief Datentraegername (Wurzeleintrag, sonst erweiterter BPB); "" = keiner.
    std::string label() const;
    /// @brief Datentraegername setzen (Wurzeleintrag und, wenn vorhanden, erweiterter BPB).
    bool setLabel(const std::string& name);

    /// @brief Den Bootsektor (512 B ab Byte 0) lesen.
    bool readBootSector(std::vector<uint8_t>& out) const;
    /**
     * @brief Ein Bootsektor-Abbild auf die Diskette schreiben (BPB bleibt, siehe
     *        @ref bootsektorMitBpb).  Das Wurzelverzeichnis und die Dateien bleiben unberuehrt.
     */
    bool writeBootSector(const std::vector<uint8_t>& bild);

    /// @brief Schreibt das Dateisystem lange Namen?  (Entscheidung der Voreinstellung, @ref WriteOptions::fat_lfn)
    bool langeNamenUeblich() const;

    /// @brief Bis zu welchem Byte-Offset des Sektors beginnt der Programmteil des Bootsektors
    ///        (Sprungziel); 0 = kein gueltiger Sprung.
    static int programmAnfang(const std::vector<uint8_t>& sektor);

private:
    FatFileSystem(SectorSpace& raum, const fat::Bpb& bpb);

    /// @brief Ein Verzeichniseintrag im Speicher.
    struct Eintrag {
        uint8_t  name[11] = {};
        uint8_t  attr = 0, nt = 0;
        uint16_t zeit = 0, datum = 0;
        uint16_t cluster = 0;
        uint32_t groesse = 0;
        std::string lang;       ///< langer Name (UTF-8), "" = keiner
        size_t   erster = 0;    ///< Index des ersten Slots (Eintraege langer Namen davor)
        size_t   slot   = 0;    ///< Index des Kurznamen-Slots
        bool     lfn_gueltig = true;
        bool     istDir() const { return (attr & fat::kAttrDir) != 0; }
    };
    /// @brief Ein geladenes Verzeichnis (Bytes + zerlegte Eintraege).
    struct Verz {
        uint16_t cluster = 0;               ///< 0 = Wurzel
        std::vector<uint8_t> bytes;
        std::vector<uint16_t> kette;        ///< belegte Cluster (leer bei der Wurzel)
        std::vector<Eintrag> eintraege;
        bool defekt = false;
    };

    // Rohzugriff
    uint64_t sektorOffset(uint32_t sektor) const { return static_cast<uint64_t>(sektor) * bpb_.bps; }
    uint64_t clusterOffset(uint16_t c) const;
    bool lesen(uint64_t off, uint8_t* dst, size_t n) const;
    bool schreiben(uint64_t off, const uint8_t* src, size_t n);

    // FAT
    bool ladeFat();
    bool schreibeFat();
    uint16_t fatWert(uint32_t n) const { return n < fat_.size() ? fat_[n] : 0xFFF; }
    static bool istEnde(uint16_t v) { return v >= 0xFF8; }
    static bool istDefekt(uint16_t v) { return v == 0xFF7; }
    bool gueltigerCluster(uint32_t c) const { return c >= 2 && c < bpb_.clusters + 2; }
    /// @brief Kette ab @p start; bricht bei Schleife/wildem Zeiger ab (@p heil = false).
    std::vector<uint16_t> kette(uint16_t start, bool* heil = nullptr, std::string* grund = nullptr) const;
    uint32_t freiCluster() const;
    /// @brief @p n freie Cluster finden (kleinste zuerst); false, wenn es nicht reicht.
    bool findeFrei(uint32_t n, std::vector<uint16_t>& out) const;
    void verketten(const std::vector<uint16_t>& c);
    void freigeben(uint16_t start);

    // Verzeichnisse
    bool ladeVerz(uint16_t cluster, Verz& v) const;
    bool speichereVerz(Verz& v, const std::vector<uint8_t>& alt);
    static void zerlege(Verz& v);
    /// @brief Pfad aufloesen: Verzeichnis (Wurzel = Cluster 0) und Eintragsindex; @p letztes false = nur Elternverz.
    bool sucheEintrag(const std::string& pfad, Verz& eltern, int& index, std::string& blatt, bool& elternGefunden) const;
    static std::vector<std::string> pfadTeile(const std::string& pfad);
    bool findeVerz(const std::vector<std::string>& teile, size_t n, Verz& v, std::string& fehler) const;
    int  findeName(const Verz& v, const std::string& name) const;
    /// @brief @p anzahl freie aufeinanderfolgende Slots; waechst ein Unterverzeichnis bei Bedarf.
    bool slotsBeschaffen(Verz& v, size_t anzahl, size_t& erster);
    bool trageEin(Verz& v, const std::string& langName, const uint8_t kurz[11], uint8_t nt,
                  uint8_t attr, uint16_t cluster, uint32_t groesse, int64_t mtime, bool lfn);
    bool legeNameAn(const Verz& v, const std::string& name, bool lfn, uint8_t kurz[11], uint8_t& nt, bool& brauchtLfn,
                    std::string& err) const;
    bool loescheEintrag(Verz& v, size_t index);

    void listeRekursiv(uint16_t cluster, const std::string& prefix, std::vector<FileEntry>& out,
                       std::vector<uint16_t>& besucht, int tiefe) const;
    FileEntry zuFileEntry(const Eintrag& e, const std::string& pfad) const;
    bool pruefeName(const std::string& teil, std::string& err) const;

    SectorSpace& raum_;
    fat::Bpb     bpb_;
    std::vector<uint16_t> fat_;          ///< entschluesselte FAT (Index = Clusternummer)
    std::vector<uint8_t>  fat_roh_;      ///< zuletzt geschriebene/gelesene Bytes (nur Aenderungen schreiben)

    friend struct FatPruefer;
};
