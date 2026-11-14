/**
 * @file bs600_fs.h
 * @brief BS600/BS610/BS620 (PRG 700/710/710-1, VEB Numerik Karl-Marx-Stadt) - das Dateisystem der
 *        BS600-Systemdisketten.
 *
 * Vollstaendig beschrieben in **`doc/bs600_diskettenformat.md`** (Entwurf 48, AP-B0).  Kurz:
 *
 * * Zylinder 0..39, einseitig, MFM: Spur 0 = 26 x 128 B, Spur 1..39 = 15 x 256 B.
 * * Spur 0: Sektor 1..3 Urlader (`SYL FOR BS6x0` bzw. `BOOTSTRAP FOR BS600`), Sektor 7 Datentraeger-
 *   kennsatz (`VOL1` in EBCDIC, ab Byte 11 `PRG`), Sektor 8..25 Verzeichnis (3 Eintraege zu 42 B
 *   je Sektor = 54 Dateien), Sektor 26 Belegungstabelle (2 B je Spur, Bit 7 = Sektor 1).
 * * Eine Datei ist ein **zusammenhaengender** Lauf von Sektoren ab Spur 1 (ueber Spurgrenzen);
 *   keine Verkettung.  Nutzbar sind Spur 1..37.
 *
 * **Schreiben.**  `write` sucht den ersten ausreichend grossen freien Lauf (wie die Allokation des
 * Originals, `1AD4`), pflegt die Belegungstabelle und traegt die Datei in den ersten freien
 * Verzeichnisplatz ein.  `erase` ueberschreibt den Eintrag mit Blanks und loescht die
 * Belegungsbits.  **Beides ist abgeleitet, nicht am Originalablauf bestaetigt** (B0 hat Loeschen und
 * Reorganisieren im Programmcode nicht gefunden; die Abzuege sind lueckenlos).  Unbekannte Felder
 * (Rest[10] eines Eintrags) bleiben bei Aenderungen unveraendert, die Neuanlage schreibt Blanks.
 *
 * @see doc/bs600_diskettenformat.md
 * @see doc/design/48_bs600.md
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#pragma once
#include "core/filesystem/file_system.h"
#include "core/filesystem/fs_profile.h"
#include "core/filesystem/sector_space.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/// @brief Sektoren je Spur >= 1 (256 B).
inline constexpr int kBs600Sektoren = 15;
/// @brief Sektorgroesse der Datenspuren.
inline constexpr size_t kBs600Sektor = 256;
/// @brief Sektorgroesse der Spur 0.
inline constexpr size_t kBs600Sektor0 = 128;
/// @brief Verzeichnisplaetze (18 Sektoren x 3).
inline constexpr int kBs600Eintraege = 54;
/// @brief Groesse eines Verzeichniseintrags.
inline constexpr size_t kBs600EintragGroesse = 42;
/// @brief Hoechste vergebene Spur (die Allokation des Originals meldet bei Spur 38 `DISK VOLL`).
inline constexpr int kBs600LetzteNutzSpur = 37;

/// @brief Geteilte Handgriffe des Dateisystems, der Pruefung und der Rettung.
namespace bs600 {
/// @brief 1..6 sichtbare ASCII-Zeichen ohne '.', '/', '\\'.
bool nameGueltig(const std::string& n);
/// @brief `TTMMJJ` oder `TT.MM.JJ` -> `TTMMJJ`; "" = nicht lesbar oder unmoeglicher Tag/Monat.
std::string datumNormal(std::string d);
/// @brief 42 Byte nur Blanks, nur 00 oder nur E5 = leerer Verzeichnisplatz.
bool leerEintrag(const std::vector<uint8_t>& p);
/// @brief Bit (Spur 1.., Sektor 1..15) der rohen Belegungstabelle (Sektor 26) setzen/loeschen/lesen.
void tabBit(std::vector<uint8_t>& tab, int spur, int sek, bool belegt);
bool tabBitGesetzt(const std::vector<uint8_t>& tab, int spur, int sek);
}  // namespace bs600

/**
 * @struct Bs600Eintrag
 * @brief Ein belegter Verzeichnisplatz, Felder wie in §5 des Formatdokuments.
 */
struct Bs600Eintrag {
    int         platz = 0;           ///< 0..53 (Sektor 8 + platz/3, Versatz 42 * (platz%3))
    std::string name;                ///< 6 Zeichen, rechts bereinigt
    char        typ = 'P';           ///< 'P' Programm, 'L' (Bedeutung offen)
    int         anf_spur = 0, anf_sek = 0, end_spur = 0, end_sek = 0;
    std::string datum;               ///< TTMMJJ wie auf der Diskette
    std::string signum;              ///< 3 Zeichen, rechts bereinigt
    int         ladeadr = -1, endadr = -1, startadr = -1;   ///< -1 = kein Hexfeld
    std::vector<uint8_t> rest;       ///< Byte 32..41, unveraendert
    bool        gueltig = true;      ///< Lage plausibel (Spur/Sektor in Grenzen, Anfang <= Ende)
    std::string problem;             ///< Klartext, wenn nicht gueltig

    /// @brief Linearer Sektorindex `(Spur-1)*15 + (Sektor-1)` von Anfang und Ende.
    int linAnfang() const { return (anf_spur - 1) * kBs600Sektoren + (anf_sek - 1); }
    int linEnde()   const { return (end_spur - 1) * kBs600Sektoren + (end_sek - 1); }
    int sektoren()  const { return gueltig ? linEnde() - linAnfang() + 1 : 0; }
};

/**
 * @struct Bs600Urlader
 * @brief Was der Urlader in Sektor 1..3 der Spur 0 ueber sein Zielgeraet verraet (Entwurf 48, AP-B4).
 *
 * Zwei Bauarten (`doc/bs600_diskettenformat.md` §3): `SYL FOR BS6x0` (NKM-Lader des 1-KB-ROM, PRG 710/710-1)
 * und `BOOTSTRAP FOR BS600` (3-KB-ROM, PRG 700).  Die Zuordnung des Namens zum Geraet ist fuer `BS620` = PRG 710-1
 * belegt (Abzug, Tastaturweg K7672); `BS610` = PRG 710 steht nur auf der Webseite [Web] - Annahme.
 * Das Geraet steht NICHT im Rechnerwortschatz des Katalogs (Merkposten formatnamen Nr. 6), deshalb nur als Text.
 */
struct Bs600Urlader {
    enum class Art { Keiner, Syl, Bootstrap, Unbekannt };
    Art         art = Art::Keiner;
    std::string name;       ///< "BS620" (Syl) bzw. "BS600" (Bootstrap); "" sonst
    std::string schalter;   ///< "710-1" | "710" | "700" | "" (Fassung nicht bestimmbar) - Wert fuer `--prg`
    std::string geraet;     ///< Anzeige: "PRG 710-1" ...
    std::string text;       ///< ein Satz fuer `info`/Hinweis ("Urlader SYL FOR BS620 - startet am PRG 710-1")
};

/**
 * @class Bs600FileSystem
 * @brief Eine BS600-Systemdiskette.
 */
class Bs600FileSystem : public FileSystem {
public:
    using FileSystem::setAttributes;

    /// @brief Positivprobe: 26 x 128 B auf Spur 0, `VOL1` (EBCDIC) in Sektor 7 mit `PRG` ab Byte 11
    ///        UND ein plausibles Verzeichnis (hoechstens die Haelfte der Eintraege unsinnig).
    static bool looksLikeBs600(const SectorSpace& space, std::string* why);

    static std::unique_ptr<Bs600FileSystem> mount(SectorSpace& space, const FsProfile& prof,
                                                  std::string& err);

    /// @brief Leere Diskette anlegen (Kennsatz, leeres Verzeichnis, leere Belegungstabelle); die
    ///        Spuren muessen schon formatiert sein.  Sektor 1..6 bleiben unberuehrt (Urlader: boot-put).
    static std::unique_ptr<Bs600FileSystem> format(SectorSpace& space, const FsProfile& prof,
                                                   const std::string& label, std::string& err);

    /// @brief Beurteilt ein Bootabbild (Sektor 1..3): Kopf `18 xx "SYL FOR "` bzw. `"BOOTSTRAP FOR"`
    ///        ab Byte 2, hoechstens 3 x 128 B.  "" = in Ordnung, sonst die Begruendung.
    static std::string bootProblem(const std::vector<uint8_t>& img);

    /// @brief Zahl der Sektoren des Bootabbilds (Sektor 1..3 der Spur 0).
    static constexpr int kBootSektoren = 3;
    /// Fuellung eines Datensektors wie von FORMAT unter BS620: C4H, dann 255 x 5EH.
    static std::vector<uint8_t> datenFuellung();
    /// Datum (TTMMJJ) und Besitzer (hoechstens 5 Zeichen, Byte 37..41) des Kennsatzes setzen; leer = unveraendert.
    bool kennsatzSetzen(const std::string& datum, const std::string& besitzer, std::string& err);

    /// @brief Urlader eines Bootabbilds (Byteband Sektor 1..3) einordnen; `art == Keiner` = kein Urlader.
    static Bs600Urlader urladerKennung(const std::vector<uint8_t>& img);

    /// @brief Passt der Urlader zum verlangten Geraet (`--prg 700|710|710-1`)?  "" = ja, sonst die Begruendung.
    static std::string urladerGegenWunsch(const std::vector<uint8_t>& img, const std::string& prg);

    /// @brief Der Urlader dieser Diskette (liest Sektor 1..3 der Spur 0).
    Bs600Urlader urlader() const;

    std::vector<FileEntry> list() const override;
    bool firstSector(const std::string& name, FsRecoverOrt& out) const override;
    bool read(const std::string& name, std::vector<uint8_t>& out) override;
    bool write(const std::string& name, const std::vector<uint8_t>& data,
               const WriteOptions& opt) override;
    bool erase(const std::string& name) override;
    bool wouldFit(const std::vector<PlannedFile>& files, FitReport& out) const override;
    bool mkfs() override;
    FsInfo info() const override;
    bool setAttributes(const std::string& name, const Bs600Attrs& attrs) override;

    // ─── Pruefung und Rettung (Entwurf 48, AP-B5) ────────────────────────────
    //
    // Befundkennungen `bs600.*` sind ein VERTRAG (check/bs600_check.cpp, Tabelle im Kopf der Datei).
    // Die Pruefung schreibt nie; Reparaturen laufen nur ueber @ref repair.
    FsCheckReport check(FsCheckLevel level, bool nachladen) const override;
    bool repair(const FsRepair& r) override;
    FsRecoverReport recoverScan(FsRecoverLevel level, bool nachladen) const override;
    bool recoverRead(const FsRecoverFind& f, std::vector<uint8_t>& out) const override;
    bool recoverRestore(const FsRecoverFind& f, const std::string& name) override;

    /// @brief Eine Datei umbenennen (Inhalt und Lage bleiben).
    bool rename(const std::string& alt, const std::string& neu);

    /**
     * @brief Luecken schliessen: alle Dateien lueckenlos ab Spur 1/Sektor 1 neu legen
     *        (in der Reihenfolge ihrer bisherigen Lage), Belegungstabelle neu rechnen.
     * @param bewegt  Zahl der Dateien, deren Lage sich geaendert hat (0 = war schon lueckenlos)
     */
    bool reorganize(int* bewegt = nullptr);

    // ─── Innenansicht (Diagnose, Tests, Pruefung) ────────────────────────────

    /// @brief Datentraegername (VOL1 Byte 4..9, rechts bereinigt).
    const std::string& volumeName() const { return label_; }
    /// @brief Alle belegten Verzeichnisplaetze in Platzreihenfolge.
    std::vector<Bs600Eintrag> eintraege() const;
    /// @brief Belegungstabelle (Sektor 26) als Bitfeld `[Spur 1..39][Sektor 1..15]`, Index `Spur*16 + Sektor`.
    std::vector<uint8_t> belegungstabelle() const;
    /// @brief Zylinder der Diskette (Spur 0 mitgezaehlt).
    int spuren() const { return tracks_; }

private:
    Bs600FileSystem(SectorSpace& space, const FsProfile& prof) : space_(space), prof_(prof) {}

    struct Belegung;
    bool sektorLesen(int cyl, int id, std::vector<uint8_t>& out) const;
    bool belegungLesen(Belegung& b, std::string& grund) const;
    bool eintragSchreiben(int platz, const std::vector<uint8_t>& satz42);
    bool tabelleSchreiben(const std::vector<uint8_t>& frei_bits);
    bool datenSchreiben(int lin, const std::vector<uint8_t>& daten, int sektoren);
    bool eintragFinden(const std::string& name, Bs600Eintrag& out) const;
    std::vector<uint8_t> eintragBytes(const Bs600Eintrag& e) const;

    SectorSpace& space_;
    FsProfile    prof_;
    std::string  label_;
    int          tracks_ = 0;
};
