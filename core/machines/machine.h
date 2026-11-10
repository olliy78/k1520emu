/**
 * @file machine.h
 * @brief Maschinenneutrale Schnittstelle, ueber die die C-ABI eine Maschine bedient.
 *
 * Der Kern kennt mehr als eine K1520-Maschine (A5120, K8915 — `K1520MachineType`
 * in `core/api/k1520_api.h`).  `k1520_api.cpp` spricht ausschliesslich ueber diese
 * Klasse; welche Maschine hinter dem Handle steht, entscheidet allein
 * `k1520_create*`.  Enthalten ist **genau** das, was die C-ABI braucht — nicht
 * mehr.  Was einer Maschine eigen ist (beim A5120 die ZVE2, der DMA-Beobachter,
 * der gehaltene Bus, Savestates), bleibt in der konkreten Klasse und wird von
 * Werkzeugen (`k1520dbg`, `boot_trace`) direkt ueber sie benutzt.
 *
 * @see doc/design/16_k8915.md §7.1
 */

#pragma once
#include "core/bus/k1520_bus.h"
#include "core/cards/gide/gide.h"
#include "core/cards/gun_usb/gun_usb.h"
#include "core/cards/gun_net/gun_net.h"
#include "core/cards/k5020/k5020.h"
#include "core/cards/k6022/k6022.h"
#include "core/cards/raf/raf.h"
#include "core/peripherals/floppy_drive/disk_image.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "core/serial/hub.h"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class K1520Machine {
public:
    virtual ~K1520Machine() = default;

    // ─── Lebenslauf ────────────────────────────────────────────────────────────
    virtual void powerOn() = 0;
    virtual void reset() = 0;
    /** @brief Bis zu @p max_cycles Takte ausfuehren; liefert die verbrauchten Takte. */
    virtual int  run(int max_cycles) = 0;
    /** @brief Anhalten anfordern (wirkt nach dem laufenden Befehl, fadensicher). */
    virtual void stop() = 0;
    /**
     * @brief NMI-Taster: eine /NMI-Flanke an die CPU (fadensicher, zugestellt am
     *        Anfang des naechsten run()).  Vorgabe: keine Wirkung — der A5120 hat
     *        keinen NMI-Taster (die K2526 kennt /NMI nur aus der Q240-Schutzlogik).
     *        K8915: ROM 0066H = Lampen aus + Selbsttest von vorn; unter SCPX liegt
     *        dort RAM — Verhalten wie am Geraet, nicht abgefangen (§3.6, §6.12).
     */
    virtual void nmi() {}

    // ─── Bild ──────────────────────────────────────────────────────────────────
    virtual const uint8_t* framebuffer() const = 0;
    virtual int  fbWidth()  const = 0;
    virtual int  fbHeight() const = 0;
    virtual bool fbDirty()  const = 0;
    virtual void fbClearDirty() = 0;
    virtual void setConsoleMode(bool on) = 0;
    virtual bool consolePoll(int& x, int& y, char& ch) = 0;
    /**
     * @brief Ein Byte des Textbildspeichers (80 × 24, zeilenweise) direkt von der
     *        Bildschirmkarte — NICHT ueber die CPU-Sicht (`memReadDebug`), die beim
     *        K8915 je nach A8H den Bildspeicher ueberdeckt (§8a AP-E4b).
     * @return Rohbyte samt Bit 7 (Attribut/Cursor); 0 ausserhalb des Bildes.
     */
    virtual uint8_t screenChar(int col, int row) const = 0;

    // ─── Tastatur (fadensicher eingereiht, verbraucht in run()) ───────────────
    virtual void keyPress(uint32_t qt_keycode, bool shift, bool ctrl) = 0;
    virtual void keyRelease(uint32_t qt_keycode) = 0;
    /**
     * @brief Tastenwiederholung in Echtzeit statt in Maschinentakten zaehlen.
     *
     * Fuer die Oberflaeche: dort ist der Rechnertakt einstellbar, die Tastatur hat
     * aber ihren eigenen Quarz.  Vorgabe aus (Tests/Werkzeuge bleiben
     * wiederholbar).  Fadensicher.  @see core/peripherals/tasten_uhr.h
     */
    virtual void setKeyRepeatRealtime(bool an) = 0;
    /** @brief Anzeigen der Tastatur; Bitbelegung je Tastaturmodell (0 = keine). */
    virtual uint8_t keyboardLeds() const { return 0; }

    // ─── Anzeigen ausserhalb des Bildes (AP-E4b; Vorgabe = keine, A5120) ───────
    /** @brief Maschinentyp als Wert von `K1520MachineType` (0 = A5120). */
    virtual int machineType() const { return 0; }
    /** @brief Rohbyte des Anzeigefelds (K8915: Latch 61H, aktiv low); 0 = keins. */
    virtual uint8_t panelLamps() const { return 0; }
    /** @brief Fortlaufender Zaehler der Summertoene (die Oberflaeche bildet die Differenz). */
    virtual uint32_t bellCount() const { return 0; }

    // ─── Disketten ─────────────────────────────────────────────────────────────
    virtual bool mountDisk(int drive, const std::string& path,
                           const std::string& format_name, bool write_protect) = 0;
    virtual bool mountDiskImage(int drive, std::unique_ptr<DiskImage> img,
                                bool write_protect) = 0;
    virtual bool createDisk(int drive, const std::string& path,
                            const std::string& format_name, bool write_protect) = 0;
    virtual bool saveDiskAs(int drive, const std::string& path,
                            const std::string& format_name) = 0;
    virtual bool unmountDisk(int drive) = 0;
    virtual bool flushDisks() = 0;

    virtual bool isDiskRawCompatible(int drive) const = 0;
    virtual std::string diskPath(int drive) const = 0;
    virtual std::string diskContainer(int drive) const = 0;
    virtual std::string diskNotice(int drive) const = 0;
    /// Metadaten des Container (.cqm: description|label|comment|interleave|skew|cylinders|crc; .td0: version|timestamp|comment|rate|tracks|sectors|warnings; .imd: ohne rate).
    virtual std::string diskContainerInfo(int, const std::string&) const { return ""; }
    virtual std::string detectedFormatName(int drive) const = 0;
    virtual std::string defaultFormatName(int drive) const = 0;
    virtual std::vector<std::string> compatibleFormats(int drive) const = 0;
    virtual std::string formatDescription(const std::string& format_name) const = 0;
    virtual const FormatCatalog& formatCatalog() const = 0;

    virtual bool isDiskActive(int drive) const = 0;
    virtual bool isDiskWriteProtected(int drive) const = 0;
    virtual bool isDiskLedOn(int drive) const = 0;
    virtual bool isMotorOn(int drive) const = 0;
    virtual bool isHeadLoaded() const = 0;
    virtual void setDiskWriteProtect(int drive, bool wp) = 0;

    // ─── Serielle Schnittstellen nach aussen (Entwurf 19 §8, AP-S5) ────────────
    /**
     * @brief Der `SerialHub` der Maschine (einer je Maschine): Einstellen, Starten,
     *        Status der einstellbaren Schnittstellen.  Index i = Reihenfolge der
     *        Anmeldung = Reihenfolge von `serielleAnschluesse()` = Index der C-ABI.
     *        nullptr = Maschine ohne Schnittstellen.
     */
    virtual k1520::serial::SerialHub* serialHub() { return nullptr; }
    /// Die einstellbaren Schnittstellen (Anschluesse der Karten), in Hub-Reihenfolge.
    virtual std::vector<k1520::serial::SerialAnschluss*> serielleAnschluesse() { return {}; }
    /// Fest verdrahtete, NICHT einstellbare Schnittstellen (Tastatur) als Anzeigename,
    /// z. B. "Tastatur K7637 (X4)" — Quelle fuer `k1520_serial_fixed_name` (Leitsatz 7/8).
    virtual std::vector<std::string> festeSchnittstellen() const { return {}; }

    // ─── Alter Unterbau: DFUE und Drucker als Rueckruf/Einspeisen ───────────────
    // Bleibt fuer Tests und `k1520_serial_set_rx_cb`/`k1520_serial_send` (Entwurf 19 §8).
    // `set…Callback`: Byte, das der Gast SENDET (in seiner Zeichenzeit, ueber den
    // Wandler getaktet); `…Send`: Byte, das von AUSSEN im Empfaenger ankommt.  Belegt
    // ein Transport oder der Rx/Tx-Loop die Schnittstelle, gehen beide ins Leere.
    // A5120: DFUE = DFÜ/V.24 (K8025 A33-A), Drucker = K8025 A32-B.
    // K8915: DFUE = DFÜ/IFSS2 (SIO2-A), Drucker = Drucker/IFSS1 (SIO1-B).
    using SerialCb = std::function<void(uint8_t)>;
    virtual void setDFUECallback(SerialCb cb) = 0;
    virtual void dfueSend(uint8_t byte) = 0;
    virtual void setPrinterCallback(SerialCb) {}
    virtual void printerSend(uint8_t) {}

    // ─── Diagnose ──────────────────────────────────────────────────────────────
    virtual uint8_t memReadDebug(uint16_t addr) = 0;
    virtual void    memWriteDebug(uint16_t addr, uint8_t data) = 0;
    virtual uint8_t ioReadDebug(uint8_t port) = 0;
    virtual std::string lastError() const = 0;

    /**
     * @brief Hat die Maschine einen K1520-Bus fuer Zusatzkarten (RAF, K6022)?  Vorgabe ja;
     *        der P8000 hat keinen (Dekoder nur 00H–3FH, Entwurf 25 §10.1) und liefert false —
     *        dann scheitern @ref installRaf / @ref installK6022 mit Grund.
     */
    virtual bool zusatzkartenSteckbar() const { return true; }

    // ─── RAM-Floppy RAF 128/512/2M (doc/design/22_raf512.md §5.2, AP-R2) ──────
    // In allen drei Maschinen dieselbe Karte auf 88H/89H.  Gesteckt wird NACH dem
    // Anlegen und VOR dem ersten run()/reset() — so braucht die C-ABI keine weitere
    // `k1520_create_*`-Variante je Kombination (EM × RAF × PRG-Variante, §6).  Ein
    // powerOn() davor ist erlaubt: die frische Karte ist im Netz-Ein-Zustand.
    /**
     * @brief Steckt eine RAF der Bauart @p typ auf 88H/89H.
     * @return false (Grund in @ref rafFehler) nach dem ersten run()/reset(), wenn
     *         schon eine RAF steckt oder 88H/89H belegt ist.
     */
    bool installRaf(RAF::Typ typ) {
        raf_fehler_.clear();
        if (!zusatzkartenSteckbar()) {
            raf_fehler_ = "Diese Maschine hat keinen K1520-Steckplatz fuer eine RAF";
            return false;
        }
        if (!bestueckbar_) {
            raf_fehler_ = "RAF nur vor dem ersten Lauf/Reset steckbar";
            return false;
        }
        if (raf_) {
            raf_fehler_ = "Es steckt bereits eine RAF";
            return false;
        }
        RAF::Config c;
        c.typ = typ;
        auto karte = std::make_unique<RAF>(c);
        try {
            karte->attachToBus(systemBus());   // wirft bei belegtem Port
        } catch (const std::exception& e) {
            raf_fehler_ = e.what();
            return false;
        }
        karte->powerOn();
        raf_ = std::move(karte);
        return true;
    }
    /** @brief Die gesteckte RAF (nullptr = keine). */
    RAF*       raf()       { return raf_.get(); }
    const RAF* raf() const { return raf_.get(); }
    /** @brief Grund des letzten gescheiterten @ref installRaf ("" = keiner). */
    const std::string& rafFehler() const { return raf_fehler_; }

    // ─── Lochstreifen: ADA K6022 / SIF1000 (doc/design/23_lochstreifen.md §3, AP-L1) ──
    // In allen drei Maschinen dieselbe Karte, fest auf E0H–E7H (Stanzer E0H–E3H, Leser
    // E4H–E7H).  Gesteckt wird wie die RAF NACH dem Anlegen und VOR dem ersten
    // run()/reset().  Die Maschine haengt beide PIOs in ihre Interruptkette
    // (@ref k6022Einketten), taktet die Karte im Laufweg (@ref k6022Takt) und setzt sie
    // beim systemweiten /RESET zurueck (@ref k6022Reset).  Kein Teil des Save-State (§2).
    /**
     * @brief Steckt die K6022 auf E0H–E7H.
     * @return false (Grund in @ref k6022Fehler) nach dem ersten run()/reset(), wenn
     *         schon eine K6022 steckt oder eines der Tore E0H–E7H belegt ist.
     */
    bool installK6022() {
        k6022_fehler_.clear();
        if (!zusatzkartenSteckbar()) {
            k6022_fehler_ = "Diese Maschine hat keinen K1520-Steckplatz fuer eine K6022";
            return false;
        }
        if (!bestueckbar_) {
            k6022_fehler_ = "K6022 nur vor dem ersten Lauf/Reset steckbar";
            return false;
        }
        if (k6022_) {
            k6022_fehler_ = "Es steckt bereits eine K6022";
            return false;
        }
        // Erst pruefen, dann anmelden: registerIO wirft erst am belegten Tor und liesse
        // die davor angemeldeten auf eine Karte zeigen, die es gleich nicht mehr gibt.
        K1520Bus& bus = systemBus();
        for (int p = K6022_BASIS; p < K6022_BASIS + 8; ++p) {
            if (const BusDevice* d = bus.ioOwner(static_cast<uint8_t>(p))) {
                char buf[96];
                std::snprintf(buf, sizeof buf, "E/A-Tor %02XH belegt (%s)", p, d->deviceName());
                k6022_fehler_ = buf;
                return false;
            }
        }
        auto karte = std::make_unique<K6022>(k6022TaktHz());
        karte->attachToBus(bus);
        k6022_ = std::move(karte);
        k6022Einketten(*k6022_);
        bus.markIntDirty();
        return true;
    }
    /** @brief Die gesteckte K6022 (nullptr = keine). */
    K6022*       k6022()       { return k6022_.get(); }
    const K6022* k6022() const { return k6022_.get(); }
    /** @brief Grund des letzten gescheiterten @ref installK6022 ("" = keiner). */
    const std::string& k6022Fehler() const { return k6022_fehler_; }
    /// Feste Basis der K6022 in allen Maschinen (Entwurf 23 §2).
    static constexpr uint8_t K6022_BASIS = 0xE0;

    // ─── Kassette: AKB K5020 + zwei K5200 (doc/design/29_kassette.md §2, AP K5) ──────
    // Wie die K6022: nach dem Anlegen, VOR dem ersten run()/reset() gesteckt; die Maschine
    // haengt beide PIOs (Daten vor Steuer) hinten an die Interruptkette, taktet die Karte im
    // Laufweg (@ref k5020Takt, Instruktionstakte), schreibt gebundene Baender je run()-Scheibe
    // zurueck (@ref k5020AutoFlush) und setzt sie beim /RESET zurueck (Baender bleiben liegen).
    // Basis je Maschine (@ref k5020Basis): A5120 30H, K8915 D0H, PRG 710 30H.  Kein Save-State.
    /**
     * @brief Steckt die K5020 auf @ref k5020Basis … +7.
     * @return false (Grund in @ref k5020Fehler) nach dem ersten run()/reset(), bei doppeltem
     *         Stecken oder belegtem Tor.
     */
    bool installK5020() {
        k5020_fehler_.clear();
        if (!zusatzkartenSteckbar()) {
            k5020_fehler_ = "Diese Maschine hat keinen K1520-Steckplatz fuer eine K5020";
            return false;
        }
        if (!bestueckbar_) {
            k5020_fehler_ = "K5020 nur vor dem ersten Lauf/Reset steckbar";
            return false;
        }
        if (k5020_) {
            k5020_fehler_ = "Es steckt bereits eine K5020";
            return false;
        }
        K1520Bus& bus = systemBus();
        const int basis = k5020Basis();
        for (int p = basis; p < basis + 8; ++p) {   // erst pruefen, dann anmelden (wie K6022)
            if (const BusDevice* d = bus.ioOwner(static_cast<uint8_t>(p))) {
                char buf[96];
                std::snprintf(buf, sizeof buf, "E/A-Tor %02XH belegt (%s)", p, d->deviceName());
                k5020_fehler_ = buf;
                return false;
            }
        }
        K5020::Config cfg;
        cfg.basis = static_cast<uint8_t>(basis);
        cfg.takt_hz = k6022TaktHz();
        k5020Konfigurieren(cfg);
        auto karte = std::make_unique<K5020>(cfg);
        karte->attachToBus(bus);
        k5020_ = std::move(karte);
        k5020Einketten(*k5020_);
        bus.markIntDirty();
        return true;
    }
    /** @brief Die gesteckte K5020 (nullptr = keine). */
    K5020*       k5020()       { return k5020_.get(); }
    const K5020* k5020() const { return k5020_.get(); }
    /** @brief Grund des letzten gescheiterten @ref installK5020 ("" = keiner). */
    const std::string& k5020Fehler() const { return k5020_fehler_; }
    /**
     * @brief Kassettenlaufwerk @p drive der Maschine (C-ABI `k1520_tape_*`), nullptr = keins.
     *        Vorgabe: die beiden Laufwerke einer gesteckten K5020 (0/1).  Der MC80.2x liefert
     *        das fest eingebaute SK 1200 seiner MBI als Laufwerk 0 (AP M11).
     */
    virtual kassette::KassettenLaufwerk* kassettenLaufwerk(int drive) {
        if (!k5020_ || drive < 0 || drive > 1) return nullptr;
        return &k5020_->laufwerk(drive);
    }

    // ─── GIDE: IDE-Karte mit ATA-Platte und RTC 72421 (doc/design/50_gun_gide.md, AP G4) ──────
    // Wie K5020/K6022: nach dem Anlegen, VOR dem ersten run()/reset() gesteckt.  Die Basis (Jumper
    // ja4..ja7) und das Vorhandensein der RTC stellt der Aufrufer ein (@p cfg); die Karte belegt
    // basis+4 .. basis+F.  Kein Interrupt, kein /WAIT, kein Save-State.  Die Maschine meldet ihr in
    // der Laufschleife die absolute Taktzahl (@ref gideTakt), setzt sie beim /RESET zurueck
    // (@ref gideReset; das Plattenabbild und die RTC bleiben) und beim Netz-Ein (@ref gidePowerOn).
    /**
     * @brief Steckt eine GIDE.
     * @param cfg    Bestueckung/Jumper (Basis, RTC bestueckt)
     * @param rtc    Zeitpolitik der RTC (nur mit cfg.rtc_vorhanden); nullptr = Vorgaben.  Der Takt
     *               (`takt_hz`) wird IMMER auf den Maschinentakt gesetzt — den kennt nur die Maschine.
     * @param abbild Plattenabbild ("" = ohne Medium); Fehler beim Oeffnen = false, Karte bleibt ungesteckt
     * @return false (Grund in @ref gideFehler) nach dem ersten run()/reset(), bei doppeltem Stecken,
     *         ungueltiger Basis, belegtem Tor oder nicht oeffenbarem Abbild.
     */
    bool installGide(const k1520::Gide::Config& cfg, const k1520::Gide::RtcConfig* rtc = nullptr,
                     const std::string& abbild = "") {
        gide_fehler_.clear();
        if (!zusatzkartenSteckbar()) {
            gide_fehler_ = "Diese Maschine hat keinen K1520-Steckplatz fuer eine GIDE";
            return false;
        }
        if (!bestueckbar_) {
            gide_fehler_ = "GIDE nur vor dem ersten Lauf/Reset steckbar";
            return false;
        }
        if (gide_) {
            gide_fehler_ = "Es steckt bereits eine GIDE";
            return false;
        }
        std::unique_ptr<k1520::Gide> karte;
        try {
            karte = std::make_unique<k1520::Gide>(cfg);   // wirft bei Basis mit gesetztem Tiefnibble
        } catch (const std::exception& e) {
            gide_fehler_ = e.what();
            return false;
        }
        K1520Bus& bus = systemBus();
        if (const std::string g = karte->belegtGrund(bus); !g.empty()) {   // erst pruefen, dann anmelden
            gide_fehler_ = g;
            return false;
        }
        if (!abbild.empty()) {
            std::string f;
            if (!karte->platte().abbildOeffnen(abbild, k1520::AtaGeraet::Geometrie(), &f)) {
                gide_fehler_ = f.empty() ? "Abbild nicht oeffenbar: " + abbild : f;
                return false;
            }
        }
        if (cfg.rtc_vorhanden) {
            k1520::Gide::RtcConfig rc = rtc ? *rtc : k1520::Gide::RtcConfig();
            rc.takt_hz = k6022TaktHz();   // Maschinentakt (Name historisch: die virtuelle Funktion gilt je Maschine)
            karte->rtcAnlegen(rc);
        }
        karte->attachToBus(bus);
        karte->powerOn();
        gide_ = std::move(karte);
        return true;
    }
    /** @brief Die gesteckte GIDE (nullptr = keine). */
    k1520::Gide*       gide()       { return gide_.get(); }
    const k1520::Gide* gide() const { return gide_.get(); }
    /** @brief Grund des letzten gescheiterten @ref installGide ("" = keiner). */
    const std::string& gideFehler() const { return gide_fehler_; }

    // ─── GUN-USB: Z80-PIO + VDIP1 mit Wirtsverzeichnis als Stick (doc/design/50_gun_gide.md, AP U2) ──────
    // Wie GIDE/K5020: nach dem Anlegen, VOR dem ersten run()/reset() gesteckt.  Vier Tore ab cfg.basis
    // (GUN fest 84H; frei einstellbar in 4er-Schritten).  Die PIO haengt hinten in der Interruptkette
    // (@ref gunUsbEinketten).  Zeitlos (kein Takt), nicht im Save-State; /RESET setzt PIO und VDIP1 zurueck,
    // der Stick bleibt gesteckt.
    /**
     * @brief Steckt die GUN-USB.
     * @return false (Grund in @ref gunUsbFehler) nach dem ersten run()/reset(), bei doppeltem Stecken,
     *         ungueltiger Basis, belegtem Tor (K8915/PRG 710: 84H..87H gehoeren der ZRE) oder nicht
     *         oeffenbarem Stickordner.
     */
    bool installGunUsb(const k1520::GunUsb::Config& cfg) {
        gun_usb_fehler_.clear();
        if (!zusatzkartenSteckbar()) {
            gun_usb_fehler_ = "Diese Maschine hat keinen K1520-Steckplatz fuer eine GUN-USB";
            return false;
        }
        if (!bestueckbar_) {
            gun_usb_fehler_ = "GUN-USB nur vor dem ersten Lauf/Reset steckbar";
            return false;
        }
        if (gun_usb_) {
            gun_usb_fehler_ = "Es steckt bereits eine GUN-USB";
            return false;
        }
        std::unique_ptr<k1520::GunUsb> karte;
        try {
            karte = std::make_unique<k1520::GunUsb>(cfg);   // wirft bei ungueltiger Basis / nicht oeffenbarem Ordner
        } catch (const std::exception& e) {
            gun_usb_fehler_ = e.what();
            return false;
        }
        K1520Bus& bus = systemBus();
        if (const std::string g = karte->belegtGrund(bus); !g.empty()) {   // erst pruefen, dann anmelden
            gun_usb_fehler_ = g;
            return false;
        }
        karte->attachToBus(bus);
        karte->powerOn();
        gun_usb_ = std::move(karte);
        gunUsbEinketten(*gun_usb_);
        return true;
    }
    /** @brief Die gesteckte GUN-USB (nullptr = keine). */
    k1520::GunUsb*       gunUsb()       { return gun_usb_.get(); }
    const k1520::GunUsb* gunUsb() const { return gun_usb_.get(); }
    /** @brief Grund des letzten gescheiterten @ref installGunUsb ("" = keiner). */
    const std::string& gunUsbFehler() const { return gun_usb_fehler_; }

    // ─── KCNet (GUN-Netz): Z80-PIO + ATmega-Modell + W5100 (doc/design/50_gun_gide.md AP N2, Protokoll 50a) ─────
    // Wie GunUsb: nach dem Anlegen, VOR dem ersten run()/reset(); vier Tore ab cfg.basis (GUN 88H, Netzkarte C0H/28H;
    // frei in 4er-Schritten).  Kommando 03 zaehlt in Maschinenzeit (@ref gunNetTakt).  Nicht im Save-State.
    /**
     * @brief Steckt die KCNet-Karte.  cfg.takt_hz == 0: der Takt der Maschine (@ref k6022TaktHz).
     * @return false (Grund in @ref gunNetFehler) nach dem ersten run()/reset(), bei doppeltem Stecken,
     *         ungueltiger Basis oder belegtem Tor (RAF 88H/89H, ZRE-Tore usw.).
     */
    bool installGunNet(const k1520::GunNet::Config& cfg) {
        gun_net_fehler_.clear();
        if (!zusatzkartenSteckbar()) {
            gun_net_fehler_ = "Diese Maschine hat keinen K1520-Steckplatz fuer eine KCNet-Karte";
            return false;
        }
        if (!bestueckbar_) {
            gun_net_fehler_ = "KCNet nur vor dem ersten Lauf/Reset steckbar";
            return false;
        }
        if (gun_net_) {
            gun_net_fehler_ = "Es steckt bereits eine KCNet-Karte";
            return false;
        }
        std::unique_ptr<k1520::GunNet> karte;
        try {
            karte = std::make_unique<k1520::GunNet>(cfg);   // wirft bei ungueltiger Basis
        } catch (const std::exception& e) {
            gun_net_fehler_ = e.what();
            return false;
        }
        K1520Bus& bus = systemBus();
        if (const std::string g = karte->belegtGrund(bus); !g.empty()) {   // erst pruefen, dann anmelden
            gun_net_fehler_ = g;
            return false;
        }
        karte->setzeTaktHz(cfg.takt_hz ? cfg.takt_hz : k6022TaktHz());
        karte->attachToBus(bus);
        karte->powerOn();
        gun_net_ = std::move(karte);
        gunNetEinketten(*gun_net_);
        return true;
    }
    /** @brief Die gesteckte KCNet-Karte (nullptr = keine). */
    k1520::GunNet*       gunNet()       { return gun_net_.get(); }
    const k1520::GunNet* gunNet() const { return gun_net_.get(); }
    /** @brief Grund des letzten gescheiterten @ref installGunNet ("" = keiner). */
    const std::string& gunNetFehler() const { return gun_net_fehler_; }

protected:
    /** @brief Der Systembus, auf dem Zusatzkarten (RAF) ihre E/A-Tore anmelden. */
    virtual K1520Bus& systemBus() = 0;
    /** @brief Aus run() und reset(): ab hier ist die Bestueckung fest. */
    void bestueckungAbschliessen() { bestueckbar_ = false; }
    /** @brief Noch vor dem ersten run()/reset() (Zusatzkarten einer einzelnen Maschine, AUP). */
    bool bestueckbar() const { return bestueckbar_; }
    /** @brief Netz-Ein der RAF: Inhalt verworfen (ohne Stand-by-Pufferung, §3.4). */
    void rafPowerOn() { if (raf_) raf_->powerOn(); }
    /** @brief Systemweiter /RESET: nur das Latch sperrt, der Inhalt bleibt (§3.4). */
    void rafReset() { if (raf_) raf_->reset(); }

    /** @brief CPU-Takt fuer die Zeichenzeiten der K6022 (alle drei Maschinen: 2,4576 MHz). */
    virtual uint32_t k6022TaktHz() const { return 2'457'600; }
    /**
     * @brief Die PIOs der frisch gesteckten K6022 in die Interruptkette aufnehmen.
     *        Vorgabe: HINTEN anhaengen, Stanzer vor Leser (Entwurf 23 §2).  Eine Maschine
     *        mit eigener Stellung (PRG 710: vor dem Fernschreiber) setzt ihre Kette neu.
     */
    virtual void k6022Einketten(K6022& k) {
        systemBus().appendInterruptChain(&k.pioStanzer());
        systemBus().appendInterruptChain(&k.pioLeser());
    }
    /** @brief Laufweg: Karte fortschreiben; true = Interruptzustand evtl. geaendert. */
    bool k6022Takt(int takte) { return k6022_ && k6022_->clockTick(takte); }
    /** @brief Systemweiter /RESET: PIOs zurueck; Band und Stanzband bleiben. */
    void k6022Reset() { if (k6022_) k6022_->reset(); }
    /** @brief Ende jeder run()-Scheibe: gebundene Stanzdatei nach der Stanzpause
     *         zurueckschreiben (Entwurf 23 §5, wie `Laufwerke::autoFlush`). */
    void k6022AutoFlush() { if (k6022_) k6022_->autoFlush(); }

    /** @brief E/A-Basis der K5020 (A5120/PRG 710: 30H; K8915 ueberschreibt mit D0H). */
    virtual int k5020Basis() const { return 0x30; }
    /** @brief Taktquelle/Laufwerksprofile der Karte festlegen (Vorgabe: Bustakt, zwei K5200
     *         mit 38 cm/s; MC80.30: Takt aus der ASP + LW 1200, AP M7). */
    virtual void k5020Konfigurieren(K5020::Config&) const {}
    /** @brief PIOs der frisch gesteckten K5020 hinten an die Kette (Daten vor Steuer). */
    virtual void k5020Einketten(K5020& k) {
        systemBus().appendInterruptChain(&k.pioDaten());
        systemBus().appendInterruptChain(&k.pioSteuer());
    }
    /** @brief Laufweg: Karte fortschreiben; true = Interruptzustand evtl. geaendert. */
    bool k5020Takt(int takte) { return k5020_ && k5020_->clockTick(takte); }
    /** @brief Systemweiter /RESET: PIOs zurueck; Baender bleiben. */
    void k5020Reset() { if (k5020_) k5020_->reset(); }
    /** @brief Ende jeder run()-Scheibe: gebundene Baender nach der Schreibpause zurueckschreiben. */
    void k5020AutoFlush() { if (k5020_) k5020_->autoFlush(); }

    /** @brief Laufweg, je Befehl: absolute Maschinentakte an die GIDE (Platte, RTC). */
    void gideTakt(uint64_t takte) { if (gide_) gide_->takt(takte); }
    /** @brief Systemweiter /RESET: LH und Platte zurueck; Medium und RTC bleiben. */
    void gideReset() { if (gide_) gide_->reset(); }
    /** @brief Netz-Ein: Zaehler leer (die RTC hat Batterie, das Medium bleibt). */
    void gidePowerOn() { if (gide_) gide_->powerOn(); }

    /** @brief Die PIO der frisch gesteckten GUN-USB hinten an die Interruptkette (PRG 710 baut neu). */
    virtual void gunUsbEinketten(k1520::GunUsb& k) { systemBus().appendInterruptChain(&k.pio()); }
    /** @brief Systemweiter /RESET: PIO und VDIP1 zurueck; der Stick bleibt. */
    void gunUsbReset() { if (gun_usb_) gun_usb_->reset(); }
    /** @brief Netz-Ein: Zaehler leer. */
    void gunUsbPowerOn() { if (gun_usb_) gun_usb_->powerOn(); }

    /** @brief Die PIO der frisch gesteckten KCNet-Karte hinten an die Interruptkette (PRG 710 baut neu). */
    virtual void gunNetEinketten(k1520::GunNet& k) { systemBus().appendInterruptChain(&k.pio()); }
    /** @brief Systemweiter /RESET: PIO und Automat zurueck, W5100 zurueckgesetzt. */
    void gunNetReset() { if (gun_net_) gun_net_->reset(); }
    /** @brief Netz-Ein: Zaehler leer. */
    void gunNetPowerOn() { if (gun_net_) gun_net_->powerOn(); }
    /** @brief Laufweg, je Befehl: absolute Maschinentakte (Kommando 03 in Maschinenzeit). */
    void gunNetTakt(uint64_t takte) { if (gun_net_) gun_net_->takt(takte); }

    /// Erst NACH Bus und Karten der abgeleiteten Klasse zerstoert — unschaedlich, die
    /// RAF fasst den Bus in ihrem Destruktor nicht an.
    std::unique_ptr<RAF> raf_;
    /// Wie raf_: nach Bus und Karten der abgeleiteten Klasse zerstoert; die K6022 fasst
    /// den Bus im Destruktor nicht an (Bus samt Kette sind dann schon fort).
    std::unique_ptr<K6022> k6022_;
    /// Wie k6022_ (K5020 fasst den Bus im Destruktor nicht an).
    std::unique_ptr<K5020> k5020_;
    /// Wie k5020_ (die GIDE fasst den Bus im Destruktor nicht an; der Destruktor der Platte schreibt zurueck).
    std::unique_ptr<k1520::Gide> gide_;
    /// Wie gide_ (die GUN-USB fasst den Bus im Destruktor nicht an).
    std::unique_ptr<k1520::GunUsb> gun_usb_;
    /// Wie gun_usb_ (zuletzt zerstoert: der Destruktor stoppt den Arbeitsfaden des W5100).
    std::unique_ptr<k1520::GunNet> gun_net_;

private:
    bool        bestueckbar_ = true;
    std::string raf_fehler_;
    std::string k6022_fehler_;
    std::string k5020_fehler_;
    std::string gide_fehler_;
    std::string gun_usb_fehler_;
    std::string gun_net_fehler_;
};
