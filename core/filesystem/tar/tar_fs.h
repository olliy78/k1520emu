/**
 * @file tar_fs.h
 * @brief Tar-Disketten — ein `tar`-Strom direkt auf der Diskette, ohne Dateisystem.
 *
 * Die P8000-Lieferdisketten tragen oft KEIN Dateisystem: jemand hat mit `tar` auf die
 * rohe Diskette geschrieben.  Der Datenstrom beginnt bei Sektor 0 (Reihenfolge wie im
 * `.img`) und laeuft ueber die ganze Diskette; mehrere Disketten eines Satzes bilden
 * EINEN Strom (`ariane.tar` = `ariane1` + `ariane2`).
 *
 * Diese Klasse liest den Strom **nur** (put/rm scheitern).  Drei Lagen kommen vor:
 *   - der Strom beginnt mit einem Kopfblock (Diskette 1 bzw. Einzeldiskette);
 *   - die Diskette beginnt MITTEN in einer Datei (Diskette 2..n): das Anfangsstueck
 *     wird als `_fortsetzung_anfang.bin` angeboten, ab dem ersten gueltigen Kopfblock
 *     (512-ausgerichtet) setzt der Strom wieder auf;
 *   - die letzte Datei ist abgeschnitten (`tar.abgeschnitten`): sie bleibt lesbar als
 *     Teilstueck, `FileEntry::damaged` und `declared_size` kennzeichnen sie.
 *
 * Erkennung ohne Magic (V7-tar hat keins): gueltige Kopfpruefsumme, druckbarer Name,
 * plausible Groesse.  Sie steht in der Erkennungsreihenfolge HINTER allen echten
 * Dateisystemen (siehe disk_volume.cpp).
 *
 * @see doc/design/35_cqm.md (Abschnitt tar-Disketten)
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#pragma once
#include "core/filesystem/file_system.h"
#include "core/filesystem/sector_space.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tar {

constexpr uint32_t kBlock = 512;
constexpr const char* kFortsetzungName = "_fortsetzung_anfang.bin";

/// @brief Ein gelesener Kopfblock.
struct Kopf {
    std::string name;       ///< Pfad wie im Archiv (ustar-Praefix, GNU-Langname eingerechnet)
    std::string linkname;
    uint32_t    mode = 0, uid = 0, gid = 0, mtime = 0;
    uint64_t    size = 0;   ///< Groesse laut Kopf
    char        typ = '0';  ///< '0' Datei, '1' Link, '2' Symlink, '3'/'4' Geraet, '5' Verz., '6' FIFO
};

/// @brief Ist @p b (512 Byte) ein gueltiger Kopfblock?  Prueft Summe, Name, Groesse.
bool kopfLesen(const uint8_t* b, Kopf& k, std::string* warum = nullptr);

/// @brief Besteht der Block ganz aus Nullen?
bool nullblock(const uint8_t* b);

/// @brief Besteht @p n Byte ab @p d ganz aus Nullen?
bool alleNull(const uint8_t* d, size_t n);

/**
 * @brief Wie viele Bytes eines Diskettenstroms gehoeren zum `.tar` (Export, `tar-export`)?
 *
 * Regel (doc/design/35_cqm.md, tar-Export):
 *  - Beginnt der Strom mit einem gueltigen Kopfblock, wird die Struktur abgelaufen (Kopf,
 *    Daten auf 512 gerundet, naechster Kopf).  Steht an einer KOPFPOSITION die Endmarke
 *    (zwei Nullbloecke), endet der Export hinter ihr — die zwei Nullbloecke gehoeren dazu.
 *  - Jede andere Lage — kein gueltiger Kopf an der Kopfposition, eine Datei, die ueber das
 *    Stromende reicht, kein Ende vor dem Stromende — ergibt den GANZEN Strom.
 *  - Beginnt der Strom nicht mit einem gueltigen Kopf (Folgeteil eines Satzes), ebenfalls
 *    der ganze Strom: zwei Nullbloecke INNERHALB von Dateidaten sind keine Endmarke, und
 *    Abschneiden zerstoerte das Archiv beim `cat`.
 * @param endmarke  (optional) true, wenn der Export an einer Endmarke endet.
 */
uint64_t exportLaenge(const uint8_t* strom, uint64_t n, bool* endmarke = nullptr);

/// @brief Urteil ueber eine `.tar`-Datei, die auf eine leere tar-Diskette soll.
struct QuellBefund {
    std::string fehler;     ///< nicht leer = abgelehnt (Datei wird nicht geschrieben)
    std::string warnung;    ///< nicht leer = darf geschrieben werden, der Anwender soll es wissen
    bool        mit_kopf = false;   ///< beginnt mit einem gueltigen Kopfblock (sonst Folgeteil)
};

/**
 * @brief Beurteilt eine `.tar`-Datei fuer @ref DiskVolume::tarSchreiben.
 *
 * Abgelehnt wird: leer, kein Vielfaches von 512, groesser als @p kapazitaet (mit dem
 * Hinweis auf `split -b`).  Gewarnt wird vor allem, was kein V7-tar ist — der P8000-/WEGA-
 * `tar` kennt weder ustar-Kopf noch pax-/GNU-Langnamen noch binaere Groessenfelder.
 * Geprueft werden ALLE Koepfe, die sich vom Anfang her ablaufen lassen; beginnt die Datei
 * nicht mit einem Kopf (Folgeteil), gibt es keine Warnung — dort ist nichts zu sehen.
 */
QuellBefund quelleBeurteilen(const std::vector<uint8_t>& d, uint64_t kapazitaet);

} // namespace tar

/**
 * @class TarQuelle
 * @brief Der Bytestrom, in dem das Archiv liegt (Diskette oder Speicher).
 */
class TarQuelle {
public:
    virtual ~TarQuelle() = default;
    virtual uint64_t size() const = 0;
    virtual bool read(uint64_t off, uint8_t* dst, size_t n) const = 0;
    /// @brief Ort des Sektors, der @p off enthaelt (fuer den Diskeditor); Vorgabe: unbekannt.
    virtual bool ort(uint64_t off, FsRecoverOrt& out) const { (void)off; (void)out; return false; }
    /// @brief Laesst sich in den Strom schreiben?  (Diskette ja, Satz im Speicher nein — T2.)
    virtual bool beschreibbar() const { return false; }
    /// @brief @p n Byte ab @p off in den Strom schreiben (nur wenn @ref beschreibbar).
    virtual bool schreibe(uint64_t off, const uint8_t* src, size_t n) { (void)off; (void)src; (void)n; return false; }
};

/// @brief Strom ueber der linearen Sektorfolge einer Diskette.
class TarSpaceQuelle : public TarQuelle {
public:
    explicit TarSpaceQuelle(SectorSpace& s) : s_(s) {}
    uint64_t size() const override { return s_.size(); }
    bool read(uint64_t off, uint8_t* dst, size_t n) const override { return s_.read(off, dst, n); }
    bool ort(uint64_t off, FsRecoverOrt& out) const override;
    bool beschreibbar() const override { return true; }
    bool schreibe(uint64_t off, const uint8_t* src, size_t n) override { return s_.write(off, src, n); }
private:
    SectorSpace& s_;
};

/// @brief Strom im Speicher — der Mehrdisketten-Satz (die Strome hintereinander).
class TarSpeicherQuelle : public TarQuelle {
public:
    explicit TarSpeicherQuelle(std::vector<uint8_t> d) : d_(std::move(d)) {}
    uint64_t size() const override { return d_.size(); }
    bool read(uint64_t off, uint8_t* dst, size_t n) const override;
private:
    std::vector<uint8_t> d_;
};

namespace tar { class TarArchiv; }

/**
 * @class TarFileSystem
 * @brief Archiv-Dateisystem ueber einem @ref TarQuelle.
 *
 * Lesbar ist jeder tar-Strom.  **Schreibbar** (Entwurf 37, AP T1) ist nur ein ABGESCHLOSSENES
 * Archiv: Kopf bei Sektor 0 UND Endmarke (zwei Nullbloecke), oder die leere tar-Diskette.
 * Jede Aenderung baut den ganzen Strom aus einem @ref tar::TarArchiv neu (V7-Koepfe, kein
 * Patchen) und schreibt ihn ab Byte 0; was vom alten Archiv hinten uebrig bleibt, wird auf
 * Null gesetzt.  Folgeteil, abgeschnittenes und ein Archiv ohne Endmarke bleiben lesbar,
 * nicht schreibbar — @ref nurLesbarGrund sagt warum.  Passt der neue Strom nicht mehr auf
 * den Datentraeger, scheitert die Aenderung mit klarer Meldung (der Satzmodus folgt in T2).
 */
class TarFileSystem : public FileSystem {
public:
    static std::unique_ptr<TarFileSystem> mount(std::unique_ptr<TarQuelle> q, std::string& why);
    /// @brief Die LEERE tar-Diskette: der Strom besteht ganz aus Nullen.  Das Dateisystem ist
    ///        leer und bleibt lesend; @ref leer sagt, dass hier noch ein `.tar` hinein darf.
    static std::unique_ptr<TarFileSystem> mountLeer(std::unique_ptr<TarQuelle> q, std::string& why);
    /// @brief Wie @ref mount, nimmt aber auch einen Strom OHNE jeden Kopfblock (Folgeteil
    ///        ohne Dateigrenze): er erscheint ganz als `_fortsetzung_anfang.bin`.  Nur fuer
    ///        eine Diskette, die gerade selbst mit `tar-put` beschrieben wurde.
    static std::unique_ptr<TarFileSystem> mountRoh(std::unique_ptr<TarQuelle> q, std::string& why);
    /// @brief Darf in dieses Archiv geschrieben werden?  (abgeschlossen oder leer, Quelle beschreibbar)
    bool schreibbar() const { return schreibbar_; }
    /// @brief Warum nicht (Klartext mit dem Weg daraus); leer, wenn @ref schreibbar.
    const std::string& nurLesbarGrund() const { return grund_; }
    /// @brief Besteht der Strom ganz aus Nullen?  (Die Probe der Erkennung; @p why nennt sonst die Stelle.)
    static bool istLeer(const TarQuelle& q, std::string* why);
    /// @brief Ist das die leere tar-Diskette (noch nichts geschrieben)?
    bool leer() const { return leer_; }
    /// @brief Traegt der Strom ein tar-Archiv (Anfang oder Fortsetzung)?
    static bool looksLikeTar(const TarQuelle& q, std::string* why);

    std::vector<FileEntry> list() const override;
    bool firstSector(const std::string& name, FsRecoverOrt& out) const override;
    bool read(const std::string& name, std::vector<uint8_t>& out) override;
    /// @brief Datei einfuegen/ersetzen.  Modus/Besitzer/Zeit aus den `wega_*`-Feldern der
    ///        @ref WriteOptions (Vorgabe 0644, uid/gid 0, mtime jetzt).
    bool write(const std::string& name, const std::vector<uint8_t>& data, const WriteOptions& opt) override;
    /// @brief Datei oder LEERES Verzeichnis entfernen.
    bool erase(const std::string& name) override;
    /// @brief Verzeichnis (samt fehlender Zwischenverzeichnisse) anlegen, Modus 0755.
    bool makeDirectory(const std::string& name) override;
    /// @brief Modus, Besitzer, Zeit aendern (`chmod`/`chown`/`touch`).
    bool setAttributes(const std::string& name, const UnixAttrs& a) override;
    bool wouldFit(const std::vector<PlannedFile>& files, FitReport& out) const override;
    bool mkfs() override;
    FsInfo info() const override;
    FsCheckReport check(FsCheckLevel level, bool nachladen) const override;

    /// @brief Beginnt der Strom mitten in einer Datei?  Dann: Laenge des Anfangsstuecks.
    uint64_t fortsetzungAnfang() const;

private:
    explicit TarFileSystem(std::unique_ptr<TarQuelle> q) : q_(std::move(q)) {}
    using FileSystem::setAttributes;     // die UDOS-/CP/M-Fassungen bleiben sichtbar (melden "nicht fuer tar")

    struct Eintrag {
        tar::Kopf k;
        std::string name;        ///< bereinigter Pfad (ohne '/', '.', '..')
        uint64_t    kopf_off = 0;
        uint64_t    daten_off = 0;
        uint64_t    ist = 0;     ///< tatsaechlich vorhandene Datenbytes
        bool        abgeschnitten = false;
        bool        pfad_geaendert = false;   ///< absolut / '..' bereinigt
    };
    enum class Ende { Nullblock, Streamende, Abgeschnitten, Pruefsumme };
    struct Scan {
        bool        ok = false;
        uint64_t    anfang = 0;           ///< Anfangsstueck vor dem ersten Kopf (0 = Strom beginnt mit Kopf)
        std::vector<Eintrag> e;
        Ende        ende = Ende::Streamende;
        uint64_t    ende_off = 0;
        int         doppelt = 0;
    };
    static Scan scanAb(const TarQuelle& q, uint64_t start);
    static Scan scanAlles(const TarQuelle& q);
    /// @brief scanAlles mit den Sonderlagen leer / roh dieses Objekts.
    Scan scan() const;
    /// @brief Wirksamer Eintrag zu @p name (der letzte gleichen Namens).
    const Eintrag* finde(const Scan& s, const std::string& name) const;
    bool lies(const Scan& s, const Eintrag& e, std::vector<uint8_t>& out, int tiefe) const;

    /// @brief Beurteilt nach dem Mounten, ob der Strom ein abgeschlossenes Archiv ist.
    void bewerte();
    /// @brief Das Archivmodell aus dem Strom laden (nur fuer schreibbare Archive).
    bool ladeArchiv(tar::TarArchiv& a) const;
    /// @brief Strom aus @p a neu bauen und ab Byte 0 schreiben (Rest des alten Archivs -> Null).
    bool speichere(const tar::TarArchiv& a);

    std::unique_ptr<TarQuelle> q_;
    bool schreibbar_ = false;
    std::string grund_;
    bool leer_ = false;     ///< Strom ganz Null: leeres Archiv, beschreibbar mit einem .tar
    bool roh_  = false;     ///< Strom ohne Kopfblock als Fortsetzung zulassen
};
