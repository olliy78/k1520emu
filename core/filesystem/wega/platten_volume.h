/**
 * @file platten_volume.h
 * @brief PlattenVolume — die P8000-Platte als Ganzes, mit virtueller Wurzel `md0` ... `md4`.
 *
 * Fassade ueber @ref WegaPlatte: die Partitionen mit WEGA-Dateisystem erscheinen als oberste
 * Ordner, Pfade lauten `md2/etc/passwd`.  Lesen, Schreiben, Loeschen, `mkdir`, Attribute,
 * Herausholen und Einfuegen laufen ueber dieselbe `.fileinfo`-Logik (@ref fileinfo.h) wie
 * @ref DiskVolume bei WEGA-Disketten; das Format ist `fs=wega`.
 *
 * Schreibschutz ist die Vorgabe; vor dem ersten Zurueckschreiben entsteht `<abbild>~`
 * (@ref WegaPlatte::flush).
 *
 * @see doc/design/37_tar_satz_und_platte.md §1 (Festlegungen 10-13)
 */

#pragma once
#include "core/filesystem/file_system.h"
#include "core/filesystem/wega/wega_platte.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class PlattenVolume {
public:
    /// @brief Platte oeffnen; nullptr + @p err, wenn es keine (WEGA-)Platte ist.
    static std::unique_ptr<PlattenVolume> open(const std::string& pfad, std::string& err,
                                               bool read_only = true);

    /// @brief Wurzel + Partitionen + alle Eintraege; `name` ist der volle Pfad
    ///        (`md2/etc/passwd`), `volume` der Index der Partition.  Die Partitionsordner
    ///        selbst sind Eintraege vom Typ "d" (`md2`).
    std::vector<FileEntry> list() const;

    bool read(const std::string& pfad, std::vector<uint8_t>& out);
    bool write(const std::string& pfad, const std::vector<uint8_t>& data, const WriteOptions& opt);
    bool erase(const std::string& pfad);
    bool makeDirectory(const std::string& pfad);
    bool setAttributes(const std::string& pfad, const UnixAttrs& a);

    /// @brief Ein Eintrag (Datei oder Ordner, NICHT rekursiv) nach @p dest_dir/@p pfad
    ///        mit `.fileinfo` (Ordner: `<ordner>.fileinfo`).  Geraete/FIFOs entfallen still.
    bool extractEintrag(const std::string& pfad, const std::string& dest_dir,
                        bool overwrite = false, bool dry_run = false);
    /// @brief Datei oder Ordner rekursiv; @p pfad leer = die ganze Platte.
    bool extract(const std::string& pfad, const std::string& dest_dir, bool overwrite = false);

    /// @brief Fortschritt von @ref insert: (Quelle, Ziel).
    using Fortschritt = std::function<void(const std::string&, const std::string&)>;
    /**
     * @brief Eine Linux-Datei oder einen Ordner (rekursiv) einfuegen.
     *
     * @p ziel ist der volle Zielpfad (`md2/etc/datei`); bei einem Ordner als Quelle der
     * Zielordner (`md2` = Wurzel der Partition).  Modus/Besitzer/Zeit aus dem `.fileinfo`
     * neben der Quelle (`fs=wega|tar`), sonst 0644 (ausfuehrbar 0755), uid/gid 0, Zeit jetzt.
     */
    bool insert(const std::string& quelle, const std::string& ziel, bool overwrite = false,
                const Fortschritt& fortschritt = nullptr);

    /// @brief Pruefung ueber alle Partitionen; `object` traegt den Partitionsnamen vorn.
    FsCheckReport check(FsCheckLevel level);
    FsInfo info(int v) const { return platte_->fs(v).info(); }

    bool flush();
    bool readOnly() const { return platte_->readOnly(); }
    void setReadOnly(bool ro) { platte_->setReadOnly(ro); }
    void setBackup(bool b) { platte_->setBackup(b); }
    bool dirty() const { return platte_->dirty(); }
    const std::string& path() const { return platte_->path(); }

    WegaPlatte& platte() { return *platte_; }
    const WegaPlatte& platte() const { return *platte_; }
    const std::string& lastError() const { return err_; }
    /// @brief "md2/etc/passwd" -> (Partition, "etc/passwd"); Partition -1 = keine genannt.
    std::pair<int, std::string> zerlege(const std::string& pfad) const;

private:
    PlattenVolume() = default;
    bool fail(const std::string& t) { err_ = t; return false; }
    /// @brief Schreibschutz aktiv?  Setzt dann die Fehlermeldung.
    bool schreibschutz() {
        if (!readOnly()) return false;
        fail("Das Plattenabbild ist schreibgeschuetzt");
        return true;
    }

    std::unique_ptr<WegaPlatte> platte_;
    std::string err_;
};
