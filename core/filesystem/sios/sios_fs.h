/**
 * @file sios_fs.h
 * @brief SIOS 1526 — das Dateisystem der IBM-Etikettendisketten des A5120/A5130/K8924.
 *
 * Vollstaendig beschrieben in **`doc/sios_diskettenformat.md`** (Arbeitspaket D1).  Kurz:
 *
 * * 26 x 128 B MFM, einseitig, natuerliche Sektorfolge; **Spur 0 ist die Etikettenspur**
 *   (EBCDIC/DKOI): Sektor 5 `ERMAP`, Sektor 7 `VOL1` (Datentraegername), Sektoren 8..26
 *   `HDR1` (aktive Datei) bzw. `DDR1`/leer.
 * * Eine **physische Datei** ist ein zusammenhaengender Bereich ganzer Spuren
 *   `[BOE, EOE]`; gueltig sind die Sektoren `BOE .. EOD-1`.  Keine Verkettung, keine Bitmap.
 * * Eine **Bibliothek** (Name endet auf `LB`) traegt in ihrem zweiten Sektor ein Verzeichnis
 *   ihrer Objekte (8-Byte-Eintraege, Fuss mit ND).  Sie erscheinen als `PHASELB/INIT`.
 *
 * **Schreiben (Arbeitspaket D4).**  Physische Dateien: freier zusammenhaengender Spurbereich
 * (erste passende Luecke ab Spur 1, hoechstens bis Spur 37), HDR1 in den ersten freien
 * Etikettensektor, dessen Datenmarke dabei F8 -> FB wird; Loeschen = HDR1 -> DDR1 und Marke F8.
 * Bibliotheksobjekte (`PHASELB/NAME`): nur ANHAENGEN hinter ND und als geloescht kennzeichnen
 * (Name blank, AADR/ML bleiben); Annahmen und Ablehnungsgruende stehen in `sios_fs.cpp` und in
 * `doc/merkposten/disktool.md`.  Alles Unbelegte (ERMAP-Fehlspuren, Verzeichnisfortsetzung,
 * Satzlaenge <> 128, NV bei 15 Eintraegen) wird abgelehnt statt geraten.
 *
 * @see doc/sios_diskettenformat.md
 * @see doc/design/43_sios1526.md
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#pragma once
#include "core/filesystem/file_system.h"
#include "core/filesystem/fs_profile.h"
#include "core/filesystem/sector_space.h"

#include <memory>
#include <string>
#include <vector>

/// @brief Sektoren je Spur einer SIOS-Diskette (fest, 128 B/Sektor).
inline constexpr int kSiosSektoren = 26;
/// @brief Sektorgroesse (fest).
inline constexpr size_t kSiosSektor = 128;

/**
 * @struct SiosEtikett
 * @brief Ein HDR1-Dateikennsatz (Spur 0, Sektor 8..26), Felder wie in §4.1 des Formatdokuments.
 *
 * Alle Adressen sind zusaetzlich als **linearer Sektorindex** (`Spur * 26 + Sektor - 1`)
 * gefuehrt; -1 = Feld nicht auswertbar.
 */
struct SiosEtikett {
    int         sektor = 0;          ///< Etikettensektor 8..26
    std::string name;                ///< Pos. 6-22, rechts bereinigt
    uint32_t    satzlaenge = 0;      ///< Pos. 23-27
    std::string boe_text, eoe_text, eod_text;   ///< "TTSSS" wie im Etikett
    int         boe = -1, eoe = -1, eod = -1;   ///< linearer Sektorindex
    char        schutz = ' ';        ///< Pos. 43 (`P`)
    char        austausch = ' ';     ///< Pos. 44 (`E`)
    char        fortsetzung = ' ';   ///< Pos. 45 (`C` = auf der Folgediskette fortgesetzt)
    std::string folge;               ///< Pos. 46-47
    bool        gueltig = true;      ///< false = ein Adressfeld ist keine Zahl / Bereich unsinnig
    std::string problem;             ///< Klartext, wenn nicht gueltig

    /// @brief Zahl der Datensektoren: `BOE .. EOD-1`; ohne brauchbares EOD bis EOE.
    int anzahl() const;
};

/**
 * @struct SiosObjekt
 * @brief Ein Eintrag im Verzeichnis einer Bibliothek (§7.2).
 */
struct SiosObjekt {
    std::string name;       ///< bis 5 Zeichen ASCII; leer = geloescht (AADR/ML bleiben stehen)
    int         aadr = 0;   ///< 1-basierte Sektornummer in der Bibliothek
    int         ml   = 0;   ///< Byte 7 des Eintrags
    int         laenge = 0; ///< Sektoren: Abstand zum naechsten AADR bzw. zu ND
};

/**
 * @class SiosFileSystem
 * @brief Eine SIOS-1526-Diskette (lesend).
 */
class SiosFileSystem : public FileSystem {
public:
    /// @brief Positivprobe der Erkennung: 26 x 128 B, `VOL1` in Sektor 7 UND `ERMAP` in Sektor 5.
    static bool looksLikeSios(const SectorSpace& space, std::string* why);

    static std::unique_ptr<SiosFileSystem> mount(SectorSpace& space, const FsProfile& prof,
                                                 std::string& err);

    /// @brief Leere Datendiskette anlegen (Spur 0 nach Formatdokument §10); die Datenspuren
    ///        muessen schon als 26 x 128 B (E5) formatiert sein.  @p label <= 6 Zeichen DKOI.
    static std::unique_ptr<SiosFileSystem> format(SectorSpace& space, const FsProfile& prof,
                                                  const std::string& label, std::string& err);

    std::vector<FileEntry> list() const override;
    bool firstSector(const std::string& name, FsRecoverOrt& out) const override;
    bool read(const std::string& name, std::vector<uint8_t>& out) override;
    bool write(const std::string& name, const std::vector<uint8_t>& data,
               const WriteOptions& opt) override;
    bool erase(const std::string& name) override;
    bool wouldFit(const std::vector<PlannedFile>& files, FitReport& out) const override;
    bool mkfs() override;
    FsInfo info() const override;
    /// @brief Schnellpruefung der Etiketten (`core/filesystem/check/sios_check.cpp`).
    FsCheckReport check(FsCheckLevel level, bool nachladen) const override;

    // ─── Innenansicht (Diagnose, Tests) ──────────────────────────────────────

    /// @brief Datentraegername (VOL1 Pos. 5-10).
    const std::string& volumeName() const { return label_; }
    /// @brief Alle aktiven HDR1-Etiketten in Sektorreihenfolge.
    std::vector<SiosEtikett> etiketten() const;
    /// @brief Das Verzeichnis einer Bibliothek; false = keine lesbare Bibliothek.
    bool bibliothek(const SiosEtikett& lib, std::vector<SiosObjekt>& out, int* nd = nullptr,
                    std::string* warum = nullptr) const;
    /// @brief Etikettensektor der Spur 0 als Text (80 Zeichen, DKOI-dekodiert); leer = nicht lesbar.
    std::string etikettenText(int sektor) const;

    /// @brief DKOI-Zeichen (EBCDIC mit den fuenf Abweichungen, §2) -> ASCII.
    static char dkoi(uint8_t b);

    /// @brief Zylinder der Diskette (Spur 0 mitgezaehlt).
    int spuren() const { return tracks_; }

    /// @brief ASCII -> DKOI; false = Zeichen nicht darstellbar.
    static bool dkoiKodieren(char c, uint8_t& out);

    /// @brief Hoechste nutzbare Spur (Spur 0 + 1..37; die beiden aeusseren sind Reserve, §1).
    int letzteNutzSpur() const;

private:
    SiosFileSystem(SectorSpace& space, const FsProfile& prof) : space_(space), prof_(prof) {}

    // ─── Schreiben ───────────────────────────────────────────────────────────
    struct Bib;                 ///< eine geladene Bibliothek (Verzeichnissektor + Fuss)
    struct Belegung;            ///< Zustand der Diskette fuer die Planung
    struct Aufgabe;             ///< das Ergebnis der Planung einer Datei
    bool belegungLesen(Belegung& b, std::string& grund) const;
    bool bibLaden(const SiosEtikett& e, Bib& out, std::string& grund) const;
    bool plane(Belegung& b, const std::string& name, uint64_t bytes, bool ueberschreiben,
               Aufgabe& a, std::string& grund) const;
    bool ausfuehren(const Aufgabe& a, const std::vector<uint8_t>& daten);
    bool datenSchreiben(int linear_erster, const std::vector<uint8_t>& daten, int sektoren);
    bool etikettSchreiben(int sektor, const std::vector<uint8_t>& text80, bool aktiv);
    bool spur0Anlegen(bool bootbereich);

    /// @brief Ein Etikett bzw. Bibliotheksobjekt samt der FileEntry-Sicht.
    struct Eintrag {
        FileEntry fe;
        int       erster = 0;    ///< linearer Sektorindex des ersten Datensektors
        int       anzahl = 0;    ///< Zahl der Datensektoren
    };
    std::vector<Eintrag> eintraege() const;
    bool sektorLesen(int linear, std::vector<uint8_t>& out) const;
    bool findEintrag(const std::string& name, Eintrag& out) const;

    SectorSpace& space_;
    FsProfile    prof_;
    std::string  label_;
    int          tracks_ = 0;
};
