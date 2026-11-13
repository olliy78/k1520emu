/**
 * @file ata_geraet.h
 * @brief ATA-/IDE-Festplatte (ein Master-Geraet) als reine Registerschnittstelle — ohne Bus (Entwurf 50, AP G1).
 *
 * Das Geraet verhaelt sich wie ein DOM/CF-Modul am 40poligen IDE-Stecker: Task-File mit 8 Registern
 * (CS0), Alternate Status/Device Control und Drive Address (CS1), 16-Bit-Datenregister.  Die Karte
 * (GIDE, AP G2) setzt die Adressleitungen HA0-HA2 um und kuemmert sich um das Low-/High-Byte-Flipflop.
 *
 * Registerzaehlung wie am Stecker (HA2..HA0):
 *   cs1 = false:  0 Data · 1 Error/Features · 2 SectorCount · 3 SectorNumber · 4 CylLow · 5 CylHigh ·
 *                 6 DriveHead (SDH) · 7 Status/Command
 *   cs1 = true:   6 AltStatus (lesen) / DeviceControl (schreiben) · 7 DriveAddress (lesen);
 *                 alle anderen Adressen liefern 0 und werden ignoriert.
 *
 * Zeit: Ein Befehl macht das Geraet BSY.  BSY endet, sobald der Gast BSY `bsy_abfragen` mal in
 * Status/AltStatus GESEHEN hat (Vorgabe 1: die naechste Abfrage zeigt schon das Ergebnis) ODER
 * `bsy_max_takte` Maschinentakte vergangen sind (`takt()`), je nachdem, was zuerst eintritt.  So ist
 * BSY mindestens eine Abfrage lang sichtbar, ohne dass Gastschleifen haengen koennen.  Danach steht
 * DRQ (Lesen/Identify: Puffer gefuellt; Schreiben: Puffer anfordernd) bzw. das Befehlsende.
 * Zwischen den Sektoren eines Mehrsektorbefehls gibt es KEIN weiteres BSY-Fenster; C4h/C5h
 * (Multiple) verhalten sich wie 20h/30h (DRQ je Sektor), setzen aber vorheriges C6h voraus.
 * Ein Schreibsektor wird beim letzten Datenwort sofort ins Speicherabbild uebernommen.
 *
 * Medium: Rohabbild mit 512-B-Sektoren (`.img`/`.hdf`) oder `.img.gz` (Art folgt den Magic Bytes);
 * ganz im Speicher.  Aenderungen gehen verzoegert zurueck (`takt()`: nach `flush_pause` Takten
 * Schreibpause, bei gzip zusaetzlich `min_abstand_gz_ms` Wirtszeit seit dem letzten Dateischreiben),
 * in einem eigenen Faden; `flush()`, `auswerfen()` und der Destruktor schreiben SOFORT.  roh: nur
 * die geaenderten Sektoren, gzip: das ganze Abbild atomar (`gzip::speichern`).  Gegen Doppelnutzung
 * legt `abbildOeffnen` die Sperrdatei `<abbild>.lock` mit der Prozessnummer an (eine Sperre eines
 * toten Prozesses wird unter POSIX uebernommen).
 *
 * Fehler im Betrieb laufen ueber Status/Error-Register, keine Ausnahmen.
 */
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/util/gzip_datei.h"

namespace k1520 {

class AtaGeraet {
public:
    /// Registeradresse HA2..HA0 (siehe Dateikopf; Bedeutung von 6/7 haengt von cs1 ab).
    enum class Reg : uint8_t {
        Data = 0, ErrorFeatures = 1, SectorCount = 2, SectorNumber = 3,
        CylLow = 4, CylHigh = 5, DriveHead = 6, StatusCommand = 7,
        AltStatusDeviceControl = 6, DriveAddress = 7   ///< nur mit cs1 = true
    };

    // Status-Bits
    static constexpr uint8_t ST_BSY = 0x80, ST_DRDY = 0x40, ST_DSC = 0x10, ST_DRQ = 0x08, ST_ERR = 0x01;
    // Error-Bits
    static constexpr uint8_t ER_AMNF = 0x01, ER_ABRT = 0x04, ER_IDNF = 0x10, ER_DIAG_OK = 0x01;
    static constexpr uint32_t SEKTORBYTES = 512;

    /// Plattengeometrie; 0 = aus der Abbildgroesse ableiten.
    struct Geometrie {
        uint32_t zylinder, koepfe, sektoren;
        Geometrie(uint32_t z = 0, uint32_t k = 0, uint32_t s = 0) : zylinder(z), koepfe(k), sektoren(s) {}
    };

    struct Config {
        unsigned bsy_abfragen = 1;           ///< so viele BSY-Abfragen bis zum Befehlsende
        uint64_t bsy_max_takte = 20000;      ///< ... oder so viele Maschinentakte
        uint64_t flush_pause = 2'000'000;    ///< Schreibpause vor dem Zurueckschreiben (0,5 s @ 4 MHz)
        unsigned min_abstand_gz_ms = 5000;   ///< gzip: Wirtszeit zwischen zwei Dateischreibvorgaengen
    };

    AtaGeraet();
    explicit AtaGeraet(const Config& cfg);
    ~AtaGeraet();
    AtaGeraet(const AtaGeraet&) = delete;
    AtaGeraet& operator=(const AtaGeraet&) = delete;

    // ---- Registerzugriff wie am Stecker ----
    uint16_t lese(Reg reg, bool cs1 = false);
    void schreibe(Reg reg, uint16_t wert, bool cs1 = false);

    /// Maschinenzeit (monoton steigende Gesamtzahl der Takte): BSY-Frist und verzoegertes Zurueckschreiben.
    void takt(uint64_t maschinentakte);

    /// Hardware-Reset (/RESET der Karte): wie ein beendeter Soft-Reset, sofort.  Medium bleibt.
    void reset();

    // ---- Medium ----
    /// Abbild einlegen (Sperrdatei, Laden).  @p g mit Nullen = Geometrie aus der Groesse.  false + Text bei Fehler.
    bool abbildOeffnen(const std::string& pfad, Geometrie g = Geometrie(), std::string* fehler = nullptr);
    /// Leeres Abbild (nur Nullen) anlegen; Endung `.gz` = gepackt.  Eine vorhandene Datei wird NICHT ueberschrieben.
    static bool abbildAnlegen(const std::string& pfad, uint64_t sektoren, std::string* fehler = nullptr);
    /// Ausstehendes Schreiben sofort erledigen, Sperre loesen, Medium entfernen.
    void auswerfen();
    /// Ausstehendes Schreiben sofort erledigen und warten.  false, wenn ein Schreiben fehlschlug.
    bool flush();

    bool mediumVorhanden() const { return medium_; }
    const std::string& pfad() const { return pfad_; }
    uint64_t sektorenGesamt() const { return gesamt_; }
    Geometrie geometrie() const { return phys_; }        ///< Vorgabegeometrie (Identify Worte 1/3/6)
    bool schmutzig() const { return !schmutzig_liste_.empty(); }   ///< Aenderungen noch nicht in der Datei?

    /// Abgeleitete Standardgeometrie fuer @p sektoren (16 Koepfe x 63 Sektoren, Zylinder bis 16383;
    /// sehr kleine Abbilder 4 x 16).
    static Geometrie geometrieAbleiten(uint64_t sektoren);

    // ---- Zustand fuer Tests/Debugger ----
    uint8_t status() const;                              ///< Statusbyte OHNE Nebenwirkung
    bool drq() const { return drq_; }
    bool bsy() const { return busy_ || srst_; }
    bool slaveGewaehlt() const { return (sdh_ & 0x10) != 0; }
    /// Task-File ohne Nebenwirkung (Debugger): kein BSY-Zaehler, kein Datenwort.
    struct TaskFile { uint8_t error, features, sektoranzahl, sektornummer, zyl_low, zyl_high, sdh, status, befehl; };
    TaskFile taskFile() const { return {err_, features_, sc_, sn_, cl_, ch_, sdh_, status(), cmd_}; }

private:
    enum class Modus { Keiner, Lesen, Schreiben, Identify };

    uint8_t statusLesen();                               ///< zaehlt BSY-Abfragen
    void befehl(uint8_t cmd);
    void abschliessen();                                 ///< Ende des BSY-Fensters
    void fertig(bool fehler, uint8_t err);               ///< Befehlsende, Statuszuordnung
    void diagnoseWerte(bool sdh_setzen);
    bool adresse(uint64_t& lba) const;                   ///< Task-File -> LBA; false = IDNF
    void adresseSetzen(uint64_t lba);
    void sektorLaden();                                  ///< Lese-Puffer fuellen (lba_cur_)
    void identifyFuellen();
    void datenSchreiben(uint16_t w);
    uint16_t datenLesen();
    void markiereSchmutzig(uint64_t lba);
    void autoFlush();
    bool schreibeJetzt(bool async);
    void warteFaden();
    bool sperreAnlegen(std::string* fehler);
    void sperreLoesen();

    Config cfg_;

    // Task-File
    uint8_t err_ = 0, features_ = 0, sc_ = 1, sn_ = 1, cl_ = 0, ch_ = 0, sdh_ = 0xA0;
    bool srst_ = false, nien_ = false;
    bool busy_ = false, drq_ = false, errbit_ = false;
    uint8_t cmd_ = 0;
    unsigned bsy_gesehen_ = 0;
    uint64_t jetzt_ = 0, bsy_start_ = 0;

    // Datentransfer
    Modus modus_ = Modus::Keiner;
    uint16_t puf_[256] = {};
    unsigned pos_ = 0;
    unsigned rest_ = 0;
    uint64_t lba_cur_ = 0;
    unsigned mult_ = 0;                                  ///< C6h-Blockgroesse (0 = nicht gesetzt)

    // Geometrie
    Geometrie phys_, akt_;                               ///< physisch / nach 91h
    uint64_t gesamt_ = 0;

    // Medium
    bool medium_ = false;
    std::string pfad_;
    gzip::Art art_ = gzip::Art::Roh;
    std::vector<uint8_t> img_;
    std::vector<bool> schmutzig_bits_;
    std::vector<uint64_t> schmutzig_liste_;
    uint64_t schreib_takt_ = 0;                          ///< Maschinenzeit der letzten Aenderung
    std::string sperre_;
    std::thread faden_;
    std::shared_ptr<std::atomic<bool>> schreibfehler_ = std::make_shared<std::atomic<bool>>(false);
    std::chrono::steady_clock::time_point letzt_datei_{};   ///< letzter Dateischreibauftrag (Wirtszeit)
};

}  // namespace k1520
