/**
 * @file dateien.h
 * @brief Ordnerfreigabe (`Freigabe`), Dateiquellen/-senken, Namensumsetzung,
 *        `ls -l`-Listing (Erzeugen und Zerlegen).
 *
 * @details
 * Doc: doc/design/36_kermit.md §4.1, §5 Nr. 4–6 (AP K2).
 *
 * Sicherheit (§5 Nr. 6): ein entfernter Name wird NIE ungeprueft zu einem
 * lokalen Pfad.  `Freigabe::aufloesen` weist ab: NUL und Steuerzeichen,
 * absolute Pfade (`/…`, `\…`), Laufwerksbuchstaben (`C:`), Rueckstriche,
 * Windows-reservierte Namen (CON, PRN, AUX, NUL, COM1–9, LPT1–9 — auf allen
 * Systemen, damit dieselbe Freigabe ueberall gleich urteilt), `..`, das ueber
 * die Wurzel hinausfuehrt, und Symlinks, die aus der Wurzel herausfuehren.
 * `..` INNERHALB der Freigabe ist erlaubt (Navigation `G C ..`).
 *
 * Namen (§5 Nr. 4): gesendet wird literal, empfangen konvertiert wie
 * C-Kermit 4.2 (`zrtol`: Grossbuchstaben → klein, sonst unveraendert).
 * Laengere Zielnamen als `max_namenslaenge` (14 fuer WEGA) werden abgewiesen,
 * nicht gekuerzt.
 *
 * Das Listing (`G D`) erzeugt der Server selbst im Format von `ls -l` unter
 * System III (WEGA), damit Oberflaeche und C-Kermit-Gegenstelle dasselbe
 * lesen; `zerlegeLs` versteht Zeilen von System III UND Linux.
 * Kein POSIX: alles ueber std::filesystem.
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace k1520::kermit {

enum class Namen { Literal, Konvertiert };

/// Name, unter dem eine empfangene Datei abgelegt wird.
std::string nameFuerEmpfang(const std::string& name, Namen modus);
/// Name, unter dem eine Datei gesendet wird (Konvertiert = C-Kermit `zltor`).
std::string nameFuerSenden(const std::string& name, Namen modus);
/// Textendung (.txt .c .h .mac .asm .doc) — fuer eine Automatik nach Endung.
bool istTextEndung(const std::string& name);

// ─── Quellen und Senken ────────────────────────────────────────────────────

class DateiQuelle {
public:
    virtual ~DateiQuelle() = default;
    /// Liest bis n Bytes; 0 = Ende, -1 = Fehler.
    virtual long lesen(uint8_t* buf, std::size_t n) = 0;
    virtual int64_t groesse() const = 0;    ///< -1 unbekannt
    virtual std::string datum() const = 0;  ///< "yyyymmdd hh:mm:ss" oder leer
};

class DateiSenke {
public:
    virtual ~DateiSenke() = default;
    virtual bool schreiben(const uint8_t* buf, std::size_t n) = 0;
    /// Schliesst und uebernimmt die Datei (setzt das Datum, falls bekannt).
    virtual bool schliessen(const std::string& datum) = 0;
    /// Schliesst und loescht die angefangene Datei.
    virtual void verwerfen() = 0;
};

std::unique_ptr<DateiQuelle> oeffneQuelle(const std::filesystem::path& p, std::string& fehler);
std::unique_ptr<DateiQuelle> textQuelle(const std::string& text);

// ─── Freigabe ──────────────────────────────────────────────────────────────

class Freigabe {
public:
    explicit Freigabe(const std::filesystem::path& wurzel, bool nurLesen = false);

    bool gueltig() const { return gueltig_; }
    bool nurLesen() const { return nurLesen_; }
    const std::filesystem::path& wurzel() const { return wurzel_; }
    /// Aktuelles Verzeichnis relativ zur Wurzel ("" = Wurzel, '/'-getrennt).
    const std::string& aktuell() const { return aktuell_; }

    /// Hoechstlaenge eines Namensteils beim Anlegen (0 = keine Grenze).
    void setzeMaxNamenslaenge(int n) { maxName_ = n; }
    int maxNamenslaenge() const { return maxName_; }
    /// Simulierter Datentraeger: hoechstens so viele Bytes duerfen geschrieben
    /// werden (-1 = keine Grenze).  Fuer Tests „Datentraeger voll".
    void setzePlatzGrenze(int64_t bytes) { platz_ = bytes; }
    int64_t platzFrei() const { return platz_; }

    /// Prueft einen entfernten Namen lexikalisch und liefert den relativen,
    /// normalisierten Pfad ('/'-getrennt, "" = Wurzel).
    bool normalisiere(const std::string& entfernt, std::string& relativ, std::string& fehler) const;
    /// Wie `normalisiere`, dazu Symlinkpruefung; liefert den lokalen Pfad.
    bool aufloesen(const std::string& entfernt, std::filesystem::path& lokal, std::string& fehler) const;

    bool wechseln(const std::string& pfad, std::string& fehler);
    /// Dateien zu einem Namen oder Muster (`*`, `?` im letzten Teil).
    std::vector<std::filesystem::path> finde(const std::string& muster, std::string& fehler) const;
    /// `ls -l`-Text (System-III-Format) fuer ein Verzeichnis, eine Datei oder ein Muster.
    bool liste(const std::string& pfad, std::string& text, std::string& fehler) const;
    /// Loescht Dateien (Muster erlaubt); `meldung` = Klartext fuer die Antwort.
    bool loeschen(const std::string& muster, std::string& meldung);
    /// Legt eine Datei zum Schreiben an (Name schon geprueft/umgesetzt).
    std::unique_ptr<DateiSenke> anlegen(const std::string& name, bool ueberschreiben,
                                        std::string& gespeichert, std::string& fehler);

    /// Relativer Anzeigename eines lokalen Pfads in der Freigabe.
    std::string anzeigeName(const std::filesystem::path& p) const;

private:
    friend class FreigabeSenke;
    std::filesystem::path wurzel_;
    bool gueltig_ = false;
    bool nurLesen_ = false;
    std::string aktuell_;
    int maxName_ = 0;
    int64_t platz_ = -1;
};

// ─── ls -l ─────────────────────────────────────────────────────────────────

struct VerzeichnisEintrag {
    bool gueltig = false;
    char art = '-';            ///< '-', 'd', 'l', 'c', 'b', 'p', 's'
    std::string rechte;        ///< 10 Zeichen, z. B. "-rw-r--r--"
    int links = 0;
    std::string besitzer, gruppe;
    int64_t groesse = -1;      ///< -1 bei Geraetedateien
    int jahr = -1, monat = -1, tag = -1, stunde = -1, minute = -1;
    std::string datum;         ///< Datum, wie es in der Zeile stand
    std::string name;
    std::string ziel;          ///< Ziel eines Symlinks
};

/// Zerlegt EINE Zeile von `ls -l` (System III/WEGA, V7, Linux inkl. long-iso,
/// SELinux-Punkt, Geraetenummern, -i/-s-Vorspalten).  "total N" und Leerzeilen
/// liefern gueltig=false.
VerzeichnisEintrag zerlegeLs(const std::string& zeile);
std::vector<VerzeichnisEintrag> zerlegeLsListe(const std::string& text);

/// Eine Zeile im System-III-Format (`%-10s%2d %-8s %-8s%7ld %-12s %s`);
/// `mitUhrzeit`: "Mon dd HH:MM" (juenger als ein halbes Jahr), sonst "Mon dd  YYYY".
std::string lsZeile(const VerzeichnisEintrag& e, bool mitUhrzeit);

}  // namespace k1520::kermit
