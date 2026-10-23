/**
 * @file wega_fs.h
 * @brief WEGA-Dateisystem (System III UNIX des P8000) — lesen, schreiben, pruefen.
 *
 * WEGA ist das UNIX des EAW P8000 (Z8001, also **big endian**).  Sein
 * Dateisystem ist das System-III-Dateisystem mit 512-Byte-Bloecken, ohne Magic:
 *
 *   - Block 0 Boot-Block, **Block 1 Superblock** (`struct filsys`),
 *   - Bloecke 2 … s_isize−1 die Inode-Liste (8 Inodes zu 64 Byte je Block,
 *     Inode 1 = Bad-Block-Datei, **Inode 2 = Wurzel**),
 *   - ab s_isize die Datenbloecke bis s_fsize−1.
 *
 * Freispeicher ist eine **verkettete Liste**: der Superblock fuehrt bis zu 50 freie
 * Bloecke (`s_free[]`), `s_free[0]` nennt den Ankerblock, der die naechsten 50 traegt
 * (`struct fblk`).  Freie Inodes stehen zusaetzlich in einem Zwischenspeicher von bis
 * zu 100 Nummern (`s_inode[]`) — der ist NUR ein Zwischenspeicher, massgeblich ist
 * `di_mode == 0` in der Inode-Liste (Kernel `ialloc`).
 *
 * Diese Klasse arbeitet auf einem **Blockgeraet** (@ref WegaBlockDev), nicht direkt
 * auf einem @ref SectorSpace: dieselbe Klasse liest die 720-K-Disketten (Blockraum =
 * die lineare Sektorfolge) und die Partitionen eines Plattenabbilds (@ref WegaPlatte).
 *
 * Namen sind **Pfade relativ zur Wurzel** (`bin/ls`, `etc/passwd`); ein fuehrender
 * Schraegstrich wird ignoriert.  @ref list liefert den ganzen Baum, Verzeichnisse
 * eingeschlossen (Typ `d`).
 *
 * Jede schreibende Operation ist in sich eine **Transaktion**: alle Blockaenderungen
 * landen zuerst in einer Arbeitskopie und gehen erst ans Geraet, wenn die Operation
 * vollstaendig gelungen ist — eine volle Diskette hinterlaesst keine halbe Datei und
 * keine verlorenen Bloecke.
 *
 * @see doc/design/27_wega_dateisystem.md
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#pragma once
#include <algorithm>
#include "core/filesystem/file_system.h"
#include "core/filesystem/sector_space.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace wega {

constexpr uint32_t kBlock     = 512;    ///< BSIZE der WEGA-Disketten (Vorgabe)
constexpr uint32_t kMaxBlock  = 1024;   ///< groesster Block ueber alle Varianten
constexpr int      kMaxNicFree = 100;   
constexpr int      kNicFree   = 50;     ///< NICFREE — Freibloecke im Superblock
constexpr int      kNicInod   = 100;    ///< NICINOD — freie Inodes im Superblock
constexpr int      kInodesProBlock = 8; ///< INOPB
constexpr int      kInodeGroesse   = 64;
constexpr int      kNAddr     = 13;     ///< 10 direkt, einfach, doppelt, dreifach
constexpr int      kNDirekt   = 10;
constexpr int      kDirSiz    = 14;     ///< DIRSIZ
constexpr int      kDirEintrag = 16;
constexpr uint16_t kRootIno   = 2;

// Dateiarten (di_mode & IFMT)
constexpr uint16_t IFMT  = 0170000;
constexpr uint16_t IFDIR = 0040000;
constexpr uint16_t IFCHR = 0020000;
constexpr uint16_t IFBLK = 0060000;
constexpr uint16_t IFREG = 0100000;
constexpr uint16_t IFIFO = 0010000;
constexpr uint16_t IFMPC = 0030000;   ///< System III: gemultiplexte Zeichendatei
constexpr uint16_t IFMPB = 0070000;   ///< System III: gemultiplexte Blockdatei

/**
 * @struct Variante
 * @brief Die Familie des System-III-Dateisystems: gleiche Strukturen, andere Bytefolge und
 *        Blockgroesse.
 *
 *  - **WEGA** (P8000/Z8000, MUTOS 8000): big endian, 512-B-Bloecke, NICFREE 50.
 *  - **MUTOS 1700** (A7100/8086, Xenix-Sitte): little endian, Langworte PDP-11-artig (hoeherwertiges
 *    Wort zuerst), 1024-B-Bloecke, NICFREE 100; Superblock ab `s_ninode` um 200 Byte verschoben.
 */
struct Variante {
    const char* name;
    uint32_t    block;     ///< BSIZE
    int         nicFree;   ///< NICFREE
    bool        be;        ///< big endian?
    int  inopb()   const { return static_cast<int>(block / 64); }
    int  nIndir()  const { return static_cast<int>(block / 4); }   ///< Adressen je indirektem Block
};
inline const Variante kWegaV  {"wega",  512,  50, true};
inline const Variante kXenixV {"mutos1700", 1024, 100, false};

/// @brief Traegt eine Inode dieser Art Blockadressen (statt einer Geraetenummer)?
inline bool hatBloecke(uint16_t mode) {
    const uint16_t t = mode & IFMT;
    return t == IFREG || t == IFDIR || t == IFIFO;
}

/**
 * @struct Superblock
 * @brief `struct filsys` — Feldlage nach dem Z8000-Compiler (Wortgrenzen, big endian).
 *
 * Offsets (am Abbild w30root1 nachgemessen): s_isize 0, s_fsize 2, s_nfree 6,
 * s_free 8, s_ninode 208, s_inode 210, s_flock 410 … s_ronly 413, s_time 414,
 * s_tfree 418, s_tinode 422, s_m 424, s_n 426, s_fname 428, s_fpack 434, s_mach 440.
 */
struct Superblock {
    uint16_t isize = 0;
    uint32_t fsize = 0;
    int16_t  nfree = 0;
    std::array<uint32_t, kMaxNicFree> free{};
    int16_t  ninode = 0;
    std::array<uint16_t, kNicInod> inode{};
    uint8_t  flock = 0, ilock = 0, fmod = 0, ronly = 0;
    uint32_t time = 0;
    uint32_t tfree = 0;
    uint16_t tinode = 0;
    int16_t  m = 0, n = 0;
    std::string fname, fpack;   ///< je hoechstens 6 Zeichen
    uint8_t  mach = 0;

    /// @brief Rohe Felder, die beim Zurueckschreiben unveraendert bleiben (ab 441).
    std::array<uint8_t, kMaxBlock> roh{};
    const Variante* v = &kWegaV;

    static Superblock aus(const uint8_t* b, const Variante& v = kWegaV);
    void in(uint8_t* b) const;
    /// @brief Zahl der Inodes = (isize − 2) · 8.
    uint32_t inodes() const { return isize >= 2 ? (isize - 2u) * static_cast<uint32_t>(v->inopb()) : 0; }
};

/**
 * @struct Dinode
 * @brief `struct dinode` (64 Byte): Modus, Links, uid, gid, Groesse, 13 × 3 Byte
 *        Blockadressen, Zugriffs-, Aenderungs- und Statuszeit.
 */
struct Dinode {
    uint16_t mode = 0;
    int16_t  nlink = 0;
    int16_t  uid = 0, gid = 0;
    uint32_t size = 0;
    std::array<uint32_t, kNAddr> addr{};
    uint8_t  pad40 = 0;          ///< 40. Adressbyte (unbenutzt, wird erhalten)
    uint32_t atime = 0, mtime = 0, ctime = 0;

    static Dinode aus(const uint8_t* p, const Variante& v = kWegaV);
    void in(uint8_t* p, const Variante& v = kWegaV) const;
};

} // namespace wega

/**
 * @class WegaBlockDev
 * @brief Ein Bereich von 512-Byte-Bloecken — Diskette oder Plattenpartition.
 */
class WegaBlockDev {
public:
    virtual ~WegaBlockDev() = default;
    /// @brief Bloecke im Bereich (obere Grenze fuer s_fsize).
    virtual uint32_t blocks() const = 0;
    /// @brief Familie des Dateisystems auf diesem Geraet (Blockgroesse, Bytefolge).
    virtual const wega::Variante& variante() const { return wega::kWegaV; }
    virtual bool readBlock(uint32_t bn, uint8_t* dst) const = 0;
    virtual bool writeBlock(uint32_t bn, const uint8_t* src) = 0;
    /// @brief Ort eines Blocks fuer den Diskeditor (nur bei Disketten).
    virtual bool ort(uint32_t bn, FsRecoverOrt& out) const { (void)bn; (void)out; return false; }
};

/**
 * @class WegaSpaceDev
 * @brief Blockgeraet ueber einem @ref SectorSpace: Block n = Bytes n·Blockgroesse ab der
 *        Startspur der linearen Sektorfolge (Zylinder, Kopf, Sektor) — so, wie der
 *        Diskettentreiber und das `.img` die Diskette sehen.
 */
class WegaSpaceDev : public WegaBlockDev {
public:
    /// @param cyl,head Spur, ab der Block 0 beginnt.  MUTOS-Disketten tragen auf
    ///        Zylinder 0 einen eigenen 26×128-Bereich, der nicht zum Blockraum gehoert.
    /// @param v        Familie des Dateisystems (Blockgroesse, Bytefolge)
    explicit WegaSpaceDev(SectorSpace& s, uint8_t cyl = 0, uint8_t head = 0,
                          const wega::Variante& v = wega::kWegaV)
        : s_(s), v_(v), start_(std::max<int64_t>(0, s.offsetOf(cyl, head))) {}
    uint32_t blocks() const override {
        return static_cast<uint32_t>((s_.size() - start_) / v_.block);
    }
    const wega::Variante& variante() const override { return v_; }
    bool readBlock(uint32_t bn, uint8_t* dst) const override {
        return s_.read(start_ + static_cast<uint64_t>(bn) * v_.block, dst, v_.block);
    }
    bool writeBlock(uint32_t bn, const uint8_t* src) override {
        return s_.write(start_ + static_cast<uint64_t>(bn) * v_.block, src, v_.block);
    }
    bool ort(uint32_t bn, FsRecoverOrt& out) const override;
private:
    SectorSpace& s_;
    const wega::Variante& v_;
    uint64_t     start_;   ///< Byte-Offset von Block 0 im linearen Raum
};

/**
 * @class Mutos8kDev
 * @brief Blockgeraet der MUTOS-8000-Disketten im **26×128-Format** (8″ FM: M8KCMD, M8KUSR).
 *
 * Der MUTOS-Plattentreiber (`mutos_io`) rechnet 512-B-Bloecke auf 128-B-Sektoren um — und
 * legt sie NICHT hintereinander auf die Spur:
 *   - Ein Block sind 4 aufeinanderfolgende **logische** Sektoren; ab Zylinder @c cyl0 laufen die
 *     logischen Sektoren ueber alle Spuren durch (26 je Spur), ein Block darf also eine
 *     Spurgrenze ueberschreiten (6,5 Bloecke je Spur; 76 Spuren = 494 Bloecke = `s_fsize`).
 *   - Innerhalb der Spur gilt Verschraenkung 2 (erst die 13 ungeraden, dann die 13 geraden IDs):
 *     logisch 0..12 -> ID 1,3,..,25; logisch 13..25 -> ID 2,4,..,26.
 *   - Jede folgende Spur ist um **6 Sektoren** gegen die vorige versetzt (Spurversatz,
 *     die „Schrittweite 6" des Treibers): ID = (Platz + 6·(Zylinder − cyl0)) mod 26 + 1.
 * Belegt durch Konsistenz an CMD/USR (Wurzel, Verzeichnisbaum, 94 % der Inodes lesbar,
 * Freiliste; Entwurf 39 §4).  Rechnet kein Sektor-ID 126 o. ae. — die Zusatzsektoren der
 * M8KROOT-Spuren gehoeren nicht dazu.
 */
class Mutos8kDev : public WegaBlockDev {
public:
    static constexpr int kSektoren = 26;      ///< logische Sektoren je Spur
    static constexpr int kSektorBytes = 128;
    static constexpr int kSpurversatz = 6;    ///< Versatz der Sektor-IDs von Spur zu Spur
    explicit Mutos8kDev(SectorSpace& s, uint8_t cyl0 = 1, uint8_t head = 0);
    uint32_t blocks() const override { return spuren_ * kSektoren / 4; }
    bool readBlock(uint32_t bn, uint8_t* dst) const override;
    bool writeBlock(uint32_t bn, const uint8_t* src) override;
    bool ort(uint32_t bn, FsRecoverOrt& out) const override;
    /// @brief Spur und ID des @p l-ten logischen Sektors (ab cyl0); false = hinter dem Ende.
    bool sektorOrt(uint32_t l, uint8_t& cyl, uint8_t& id) const;
private:
    SectorSpace& s_;
    uint8_t  cyl0_, head_;
    uint32_t spuren_ = 0;   ///< aufeinanderfolgende 26×128-Spuren ab cyl0
};

/**
 * @class WegaFileSystem
 * @brief Ein WEGA-Dateisystem auf einem Blockgeraet.
 */
class WegaFileSystem : public FileSystem {
public:
    /// @brief Vorhandenes Dateisystem einhaengen (prueft die Plausibilitaet).
    static std::unique_ptr<WegaFileSystem> mount(std::unique_ptr<WegaBlockDev> dev,
                                                 std::string& why);
    /**
     * @brief Leeres Dateisystem anlegen — wie `sa.mkfs <bloecke>`.
     *
     * Inode-Liste = bloecke/25 Bloecke (+2), Freiliste in der Verschraenkung
     * @p m / @p n (die 720-K-Disketten tragen 1/72), Inode 1 = leere Bad-Block-Datei,
     * Inode 2 = Wurzel (`040777`, uid/gid 0) — genau so steht es auf den
     * Lieferdisketten.  @p inodes > 0 verlangt mindestens so viele Inodes (aufgerundet auf
     * ganze Inode-Bloecke) statt bloecke/25 — eine bis zum Rand gefuellte Originaldiskette
     * hat oft eine KLEINERE Inode-Liste als diese Vorgabe.
     */
    static std::unique_ptr<WegaFileSystem> format(std::unique_ptr<WegaBlockDev> dev,
                                                  const std::string& label,
                                                  std::string& err,
                                                  uint32_t bloecke = 0,
                                                  int m = 1, int n = 72,
                                                  uint32_t zeit = 0,
                                                  uint32_t inodes = 0);

    /**
     * @brief Traegt das Geraet ein WEGA-Dateisystem?  Ohne Magic — die Probe ist
     *        eine Plausibilitaetspruefung (Groessen passen zum Geraet, Listen im
     *        Wertebereich, Wurzel-Inode 2 ist ein Verzeichnis mit `.` und `..`).
     */
    static bool looksLikeWega(const WegaBlockDev& dev, std::string* why);

    // ── FileSystem ───────────────────────────────────────────────────────────
    std::vector<FileEntry> list() const override;
    bool firstSector(const std::string& name, FsRecoverOrt& out) const override;
    bool read(const std::string& name, std::vector<uint8_t>& out) override;
    bool write(const std::string& name, const std::vector<uint8_t>& data,
               const WriteOptions& opt) override;
    bool erase(const std::string& name) override;
    bool setAttributes(const std::string& name, const UnixAttrs& a) override;
    std::string blockList(const std::string& name) const override;
    using FileSystem::setAttributes;   // die Ueberladungen fuer UDOS/CP/M bleiben sichtbar
    bool makeDirectory(const std::string& name) override;
    bool makeNode(const std::string& name, uint16_t mode, uint16_t rdev) override;
    bool makeLink(const std::string& vorhanden, const std::string& neu) override;
    bool wouldFit(const std::vector<PlannedFile>& files, FitReport& out) const override;
    bool mkfs() override;
    FsInfo info() const override;
    FsCheckReport check(FsCheckLevel level, bool nachladen) const override;

    // ── WEGA-eigene Auskunft ─────────────────────────────────────────────────
    wega::Superblock superblock() const;
    const WegaBlockDev& device() const { return *dev_; }
    /// @brief Inode lesen (1-basiert).
    bool readInode(uint32_t ino, wega::Dinode& out) const;
    /// @brief Inode-Nummer eines Pfads; 0 = nicht gefunden.
    uint32_t lookup(const std::string& path) const;

    /// @brief Zeitquelle fuer neue Zeitstempel (Tests setzen sie fest).
    void setClock(std::function<uint32_t()> uhr) { uhr_ = std::move(uhr); }

    /// @brief `ls -l`-Zeichenkette fuer einen Modus ("drwxr-xr-x").
    static std::string modeText(uint16_t mode);

private:
    explicit WegaFileSystem(std::unique_ptr<WegaBlockDev> dev)
        : dev_(std::move(dev)), v_(&dev_->variante()) {}
    const wega::Variante& V() const { return *v_; }

    class Tx;                    // Arbeitskopie einer schreibenden Operation
    friend class Tx;
    /// Name in ein Verzeichnis eintragen (Elter-mtime/ctime = jetzt, nlink += Delta).
    bool eintragen(Tx& tx, uint32_t dir, const std::string& blatt, uint32_t ino,
                   uint32_t jetzt_, int nlinkDelta);

    std::unique_ptr<WegaBlockDev> dev_;
    const wega::Variante* v_;
    std::function<uint32_t()> uhr_;
    uint32_t jetzt() const;

    // Lesen ueber eine Blockquelle (Geraet oder Arbeitskopie)
    using Leser = std::function<bool(uint32_t, uint8_t*)>;
    Leser geraeteLeser() const;
    static bool inodeLesen(const Leser& l, const wega::Superblock& sb, uint32_t ino,
                           wega::Dinode& out);
    /// @brief Alle Datenbloecke (0 = Loch) in Dateireihenfolge, bis @p groesse.
    static bool dateiBloecke(const wega::Variante& vv, const Leser& l, const wega::Dinode& di, uint32_t fsize,
                             std::vector<uint32_t>& daten, std::vector<uint32_t>* indirekt,
                             std::string& err);
    static bool dateiLesen(const wega::Variante& vv, const Leser& l, const wega::Dinode& di, uint32_t fsize,
                           std::vector<uint8_t>& out, std::string& err);
    struct DirEintrag { uint16_t ino; std::string name; uint32_t slot; };
    static bool verzeichnisLesen(const Leser& l, const wega::Superblock& sb, uint32_t ino,
                                 std::vector<DirEintrag>& out, std::string& err);
    uint32_t suche(const Leser& l, const wega::Superblock& sb, const std::string& path,
                   uint32_t* eltern, std::string* blatt) const;
};

/**
 * @class WegaSpeicherDev
 * @brief Blockgeraet ueber einem Byte-Puffer (Plattenpartition); die Blockzuordnung
 *        rechnet der Aufrufer (@ref WegaPlatte — dort sitzt die Defektspur-Verschiebung).
 */
class WegaSpeicherDev : public WegaBlockDev {
public:
    WegaSpeicherDev(std::vector<uint8_t>& buf, uint32_t bloecke,
                    std::function<int64_t(uint32_t)> offset,
                    std::function<void()> geaendert)
        : buf_(buf), n_(bloecke), off_(std::move(offset)), geaendert_(std::move(geaendert)) {}
    uint32_t blocks() const override { return n_; }
    bool readBlock(uint32_t bn, uint8_t* dst) const override;
    bool writeBlock(uint32_t bn, const uint8_t* src) override;
private:
    std::vector<uint8_t>& buf_;
    uint32_t n_;
    std::function<int64_t(uint32_t)> off_;
    std::function<void()> geaendert_;
};
