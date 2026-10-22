/**
 * @file tar_archiv.h
 * @brief Das editierbare tar-Archiv im Speicher (Entwurf 37, AP T1).
 *
 * Ein @ref tar::TarArchiv haelt die Eintraege eines tar-Stroms als Liste "Kopf + Daten".
 * Jede Aenderung (Hinzufuegen, Loeschen, Ersetzen, Attribute) aendert NUR diese Liste;
 * der Strom wird mit @ref tar::TarArchiv::strom jedes Mal NEU gebaut: V7-Koepfe, Daten
 * auf 512 Byte aufgefuellt, Endmarke (zwei Nullbloecke).  Es wird nie im bestehenden
 * Strom gepatcht — so kann kein halb umgeschriebener Kopf auf der Diskette bleiben.
 *
 * Das Format ist das des P8000-/WEGA-`tar` (V7): kein ustar-Praefix, keine pax-/GNU-
 * Langnamen, Namen hoechstens 100 Zeichen.  Verzeichnisse stehen als Eintrag Typ '5'
 * mit '/' am Namensende.
 *
 * @see core/filesystem/tar/tar_fs.h, doc/design/37_tar_satz_und_platte.md
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#pragma once
#include "core/filesystem/tar/tar_fs.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tar {

/// @brief Hoechstlaenge eines Namens im V7-Kopf (ohne Praefixfeld).
constexpr size_t kNameMax = 100;

/// @brief Pfad bereinigen: kein fuehrendes '/', keine "." und leeren Teile, ".." wird "__".
///        (Dieselbe Regel wie beim Auflisten; @p geaendert meldet, ob etwas umgeschrieben wurde.)
std::string bereinigePfad(const std::string& roh, bool& geaendert);

/// @brief Ein Eintrag des Archivs.
struct ArchivEintrag {
    Kopf                 k;       ///< Name wie im Archiv (Verzeichnisse mit '/' am Ende); size wird beim Bauen gesetzt
    std::vector<uint8_t> daten;   ///< nur Typ '0' (Datei)
    /// @brief Der bereinigte Name (Schluessel fuer Suche und Anzeige).
    std::string schluessel() const;
    /// @brief Traegt der Eintrag Datenbloecke hinter dem Kopf?
    bool mitDaten() const { return k.typ == '0'; }
    /// @brief Platz im Strom: Kopf + aufgefuellte Daten.
    uint64_t belegt() const {
        return kBlock + (mitDaten() ? (daten.size() + kBlock - 1) / kBlock * kBlock : 0);
    }
};

/// @brief Angaben fuer einen neuen Eintrag.
struct NeuAngaben {
    uint32_t mode = 0644, uid = 0, gid = 0;
    uint32_t mtime = 0;        ///< 0 = jetzt
};

class TarArchiv {
public:
    /// @brief Eintraege in Archivreihenfolge (Doppelte bleiben erhalten; der letzte gewinnt).
    std::vector<ArchivEintrag>&       eintraege()       { return e_; }
    const std::vector<ArchivEintrag>& eintraege() const { return e_; }

    /// @brief Index des wirksamen (letzten) Eintrags zu @p name; -1 = nicht vorhanden.
    int finde(const std::string& name) const;

    /// @brief Datei hinzufuegen bzw. (mit @p ersetzen) ihren Eintrag ersetzen.
    bool hinzufuegen(const std::string& name, const std::vector<uint8_t>& daten,
                     const NeuAngaben& a, bool ersetzen, std::string& err);
    /// @brief Verzeichnis samt fehlenden Zwischenverzeichnissen anlegen (`mkdir -p`).
    bool verzeichnis(const std::string& name, const NeuAngaben& a, std::string& err);
    /// @brief Eintrag entfernen; ein Verzeichnis nur, wenn nichts mehr darin liegt.
    bool loeschen(const std::string& name, std::string& err);
    /// @brief Modus/Besitzer/Zeit des wirksamen Eintrags aendern (nicht Gesetztes bleibt).
    bool attribute(const std::string& name, const uint32_t* mode, const uint32_t* uid,
                   const uint32_t* gid, const uint32_t* mtime, std::string& err);

    /// @brief Laenge des Stroms (Koepfe, Daten, Endmarke).
    uint64_t stromLaenge() const;
    /// @brief Den Strom NEU bauen.  false + @p err, wenn ein Eintrag nicht ins V7-Format passt.
    bool strom(std::vector<uint8_t>& out, std::string& err) const;

private:
    std::vector<ArchivEintrag> e_;
};

/// @brief Einen V7-Kopfblock bauen.  false + @p err, wenn Name/Link zu lang oder Felder zu gross.
bool kopfSchreiben(const Kopf& k, uint64_t groesse, uint8_t out[kBlock], std::string& err);

}  // namespace tar
