/**
 * @file vdip_geraet.h
 * @brief VDIP1 (Vinculum, Firmware VDAP 3.68) als Byte-Zustandsmaschine ueber einem Wirtsverzeichnis
 *        als "USB-Stick" — ohne Bus und PIO (Entwurf 50, AP U1; Protokoll: Abschnitt 5a).
 *
 * Schnittstelle wie der parallele FIFO des Chips: der Gast schreibt Bytes (`senden`, TXE# = `sendenBereit`),
 * holt Antworten (`datenBereit` = RXF#, `empfangen`).  Alles wirkt sofort (kein Zeitmodell); die Karte (U2)
 * setzt die Pegel auf PIO-Leitungen um.
 *
 * Befehlssaetze: Vorgabe nach Reset ist ECS (Textbefehle, Prompt `D:\>`, Klartext-Fehler); SCS (Opcodes,
 * Prompt `>`, Kurzfehler) kommt per `10h`.  Opcodes und Mnemonics werden in beiden Modi erkannt.  Der Prompt
 * ist zugleich der Status: Erfolg = Prompt CR, Fehler = Fehlertext CR (ohne Prompt).
 *
 * "Stick" = `VdipStick`: ein Wirtsverzeichnis mit Sandbox.  Nie ausserhalb der Wurzel: Namen strikt 8.3
 * (Zeichensatz eingeschraenkt, keine Geraetenamen), kein `..`/Pfadtrenner/absoluter Pfad ausser dem
 * Befehl `CD ..`/`CD /`, Symlinks und alles ausser regulaeren Dateien/Verzeichnissen sind unsichtbar.
 * Fehler kommen als `VdipFehler`/VDIP-Fehlertext, nie als Ausnahme.
 */
#pragma once
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace k1520 {

/// Fehlerarten der Firmware (ECS-Klartext / SCS-Kuerzel, siehe `vdipFehlerText`).
enum class VdipFehler {
    Ok,
    BadCommand,       ///< BC
    CommandFailed,    ///< CF
    DiskFull,         ///< DF
    Invalid,          ///< FI
    ReadOnly,         ///< RO
    FileOpen,         ///< FO
    DirNotEmpty,      ///< NE
    FilenameInvalid,  ///< FN
    NoDisk            ///< ND
};

const char* vdipFehlerText(VdipFehler f, bool kurz);

/// Ein Wirtsverzeichnis als USB-Stick (FAT-Sicht: 8.3-Namen, 32-Bit-Groessen, DOS-Zeitstempel).
class VdipStick {
public:
    struct Eintrag {
        std::string name;          ///< GROSS, `NAME` oder `NAME.EXT`
        bool verzeichnis = false;
        uint64_t groesse = 0;
        uint32_t dosZeit = 0;      ///< DOS-Dword: Datum << 16 | Zeit (UTC)
    };

    VdipStick() = default;
    ~VdipStick() { schliesseDatei(); }
    VdipStick(const VdipStick&) = delete;
    VdipStick& operator=(const VdipStick&) = delete;

    /// Wurzel setzen (muss ein vorhandenes Verzeichnis sein, wird aufgeloest); false + Text bei Fehler.
    bool einstecken(const std::filesystem::path& ordner, bool schreibgeschuetzt, std::string* fehler = nullptr);
    void ziehen();
    bool vorhanden() const { return vorhanden_; }
    bool schreibgeschuetzt() const { return ro_; }

    /// Name strikt 8.3 pruefen und GROSS normalisieren (`name.` -> `NAME`).  false = ungueltig.
    static bool dateiname83(const std::string& roh, std::string& norm);
    /// DOS-Zeitstempel <-> Wirtszeit (UTC, 2-s-Raster).
    static uint32_t zeitZuDos(std::filesystem::file_time_type t);
    static bool dosZuZeit(uint32_t dos, std::filesystem::file_time_type& t);

    // Verzeichnis
    VdipFehler liste(std::vector<Eintrag>& aus);          ///< ohne `.`/`..`, sortiert
    VdipFehler eintrag(const std::string& name, Eintrag& aus);
    VdipFehler cd(const std::string& arg);                ///< Name, `..` oder `/`
    bool imWurzelverzeichnis() const { return cwd_.empty(); }
    std::string verzeichnisAnzeige() const;               ///< `\` oder `\A\B`
    VdipFehler mkd(const std::string& name, bool stempel, uint32_t dos);
    VdipFehler rmd(const std::string& name);
    VdipFehler loesche(const std::string& name);
    VdipFehler benenne(const std::string& alt, const std::string& neu);
    uint64_t freierPlatz();

    // Datei (hoechstens eine offen)
    VdipFehler oeffneLesen(const std::string& name);
    VdipFehler oeffneSchreiben(const std::string& name, bool stempel, uint32_t dos);
    VdipFehler lies(uint64_t n, std::string& aus);        ///< liefert hoechstens den Rest der Datei
    VdipFehler schreibe(const char* p, size_t n);
    VdipFehler suche(uint64_t pos);
    VdipFehler schliesseDatei(const std::string& name);   ///< nur die offene Datei dieses Namens
    void schliesseDatei();
    bool dateiOffen() const { return !offenName_.empty(); }
    bool zumSchreibenOffen() const { return dateiOffen() && offenSchreiben_; }
    bool zumLesenOffen() const { return dateiOffen() && !offenSchreiben_; }

private:
    /// Pfad des aktuellen Verzeichnisses; prueft jede Komponente erneut (kein Symlink, Verzeichnis).
    bool cwdPfad(std::filesystem::path& p);
    /// Vorhandenen Eintrag (Datei/Verzeichnis, kein Symlink) ohne Gross/Klein-Unterschied suchen.
    bool finde(const std::filesystem::path& dir, const std::string& norm, std::filesystem::path& pfad,
               bool& istVerz, std::string* hostName = nullptr);

    std::filesystem::path wurzel_;
    std::vector<std::string> cwd_;   ///< Hostnamen der Komponenten unterhalb der Wurzel
    bool vorhanden_ = false;
    bool ro_ = false;

    std::fstream datei_;
    std::string offenName_;          ///< normalisierter Name der offenen Datei (leer = keine)
    std::filesystem::path offenPfad_;
    bool offenSchreiben_ = false;
    bool offenStempel_ = false;
    uint32_t offenDos_ = 0;
};

/// Die VDAP-Firmware.
class VdipGeraet {
public:
    struct Config {
        std::filesystem::path ordner;   ///< Wirtsverzeichnis; leer = kein Stick
        bool schreibgeschuetzt = false;
    };

    VdipGeraet() : VdipGeraet(Config{}) {}
    explicit VdipGeraet(const Config& c);

    /// Firmware-Neustart: Zustand zurueck, Eingangs-FIFO leer, Startmeldung in die Ausgabe.
    void reset();

    // host -> VDIP
    void senden(uint8_t b);
    bool sendenBereit() const { return true; }     ///< TXE# = low: Eingabe wird sofort verarbeitet
    // VDIP -> host
    bool datenBereit() const { return !aus_.empty(); }   ///< RXF# = low
    uint8_t empfangen();
    size_t datenAnzahl() const { return aus_.size(); }

    /// Stick wechseln (Meldung `Device Detected P2` / `Device Removed P2` in die Ausgabe).
    bool stickEinstecken(const Config& c, std::string* fehler = nullptr);
    void stickZiehen();
    bool stickVorhanden() const { return stick_.vorhanden(); }
    VdipStick& stick() { return stick_; }

    // Zustand (Diagnose/Tests)
    bool kurzSatz() const { return scs_; }
    bool binaer() const { return iph_; }

private:
    enum class Cmd {
        Keiner, Echo, EchoKlein, Scs, Ecs, Ipa, Iph, Dir, Dirt, Cd, Rd, Dld, Mkd, Dlf, Wrf, Opw, Clf,
        Rdf, Ren, Opr, Sek, Fs, Idd, Fwv, Unbekannt
    };
    enum class Phase { Befehl, Trenner, Name, Stempel, Zahl, Rest, Daten };

    static Cmd mnemonic(const std::string& s);
    static Cmd opcode(uint8_t b);

    void befehlEnde(uint8_t sep);  ///< Befehl erkannt, `sep` = folgendes Byte (' ' oder CR)
    void resetZeile();
    void zeileEnde();              ///< CR erreicht: Befehl ausfuehren
    void ausfuehren();
    void datenByte(uint8_t b);
    void wrfFertig();

    void text(const std::string& s);
    void roh(const std::string& s);
    void zeilenEnde() { aus_.push_back(0x0D); }
    void status();
    void fehler(VdipFehler f);
    void ok(VdipFehler f) { if (f == VdipFehler::Ok) status(); else fehler(f); }
    void zahl32(uint32_t v);
    void startMeldung(bool mitBanner);

    VdipStick stick_;
    std::deque<uint8_t> aus_;
    bool scs_ = false;    ///< Kurzsatz aktiv
    bool iph_ = false;    ///< Zahlen binaer
    // Befehlszeile
    Phase phase_ = Phase::Befehl;
    Cmd cmd_ = Cmd::Keiner;
    std::string tok_;
    std::string arg_;
    bool hatStempel_ = false;
    std::vector<uint8_t> stempelBytes_;
    std::vector<uint8_t> zahlBytes_;
    uint32_t zahl_ = 0;
    // WRF-Daten
    uint64_t wrfRest_ = 0;
    std::string wrfPuffer_;
    VdipFehler wrfFehler_ = VdipFehler::Ok;
};

}  // namespace k1520
