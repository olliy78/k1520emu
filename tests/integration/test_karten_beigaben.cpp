/**
 * @file test_karten_beigaben.cpp
 * @brief Die AUSGELIEFERTEN Systemdisketten (disks/) tragen die Gast-Software der Hardwareoptionen
 *        (GIDE, Uhr, USB, Netz; tools/gide/) - und sie laeuft auf der Vorauswahl-Basis der Oberflaeche.
 *
 * Alle Dateien kommen aus dem Repository (tools/gide/fremd, tools/gide/uhr), nichts wird uebersprungen.
 * Je Rechner/Betriebssystem: die Lieferdiskette (TempDisk-artige Kopie) booten, die Karten auf der
 * Vorauswahl-Basis stecken (app/profil.py: GIDE 90H; USB 84H bzw. 8CH; Netz C0H bzw. 28H am PRG 710) und
 *  - GIDE: Treiber starten, Platte benutzen (Datei hin und zurueck, bitgleich), UHR zeigt und stellt die Zeit,
 *  - USB:  USB FWV, UDIR zeigt den Stick, UGET holt eine Stickdatei bitgleich,
 *  - Netz: TFTP/CPMNET melden die Karte (Version, "online").
 * Netz nur Loopback (Richtlinie), kein UDP-Port 69.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/filesystem/disk_volume.h"
#include "core/filesystem/fs_catalog.h"
#include "core/logger.h"
#include "core/machines/a5120/a5120.h"
#include "core/machines/k8915/k8915.h"
#include "core/machines/pc1715/pc1715.h"
#include "core/machines/prg710/prg710.h"
#include "core/peripherals/ata/ata_geraet.h"
#include "tests/support/keyboard.h"
#include "tests/support/machine_run.h"
#include "tests/support/pc1715_input.h"
#include "tests/support/screen.h"
#include "tests/support/temp_path.h"

#ifndef A5120_LIEFERDISKETTEN
#error "A5120_LIEFERDISKETTEN fehlt - k1520_add_test(... DEFS A5120_LIEFERDISKETTEN=\"<repo>/disks\")"
#endif

namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<uint8_t>;
constexpr int64_t T0 = 1794314096;   // 2026-11-10 12:34:56 UTC

// ---------------------------------------------------------------------------------------------
// Lieferdiskette (Kopie) und DiskTool-Zugriff
// ---------------------------------------------------------------------------------------------
class Lieferkopie {
public:
    Lieferkopie(const std::string& rel, const std::string& name) {
        const fs::path quelle = fs::path(A5120_LIEFERDISKETTEN) / rel;
        ok_ = fs::is_regular_file(quelle);
        path_ = (fs::temp_directory_path() / name).string();
        if (ok_) fs::copy_file(quelle, path_, fs::copy_options::overwrite_existing);
    }
    ~Lieferkopie() { std::error_code ec; fs::remove(path_, ec); }
    bool ok() const { return ok_; }
    const std::string& path() const { return path_; }
private:
    bool ok_ = false;
    std::string path_;
};

const FormatCatalog& formate() {
    static FormatCatalog c = [] { std::string f; return FormatCatalog::loadDefault(&f); }();
    return c;
}
const FsCatalog& dateisysteme() {
    static FsCatalog c = [] { std::string f; return FsCatalog::loadDefault(formate(), &f); }();
    return c;
}

/// Datei @p name von der (geschlossenen) Diskette holen; leer, wenn sie fehlt.
Bytes vonDiskette(const std::string& abbild, const std::string& name) {
    std::string err;
    auto vol = DiskVolume::open(abbild, "", formate(), dateisysteme(), err, /*read_only=*/true);
    if (!vol) return {};
    const std::string ziel = k1520test::tempPath("k1520_karten_extract.bin");
    std::error_code ec;
    fs::remove(ziel, ec);
    FileRef r;
    r.name = name;
    if (!vol->extract(r, ziel, TransferOptions{})) return {};
    std::ifstream f(ziel, std::ios::binary);
    Bytes d((std::istreambuf_iterator<char>(f)), {});
    fs::remove(ziel, ec);
    return d;
}

Bytes dateiLesen(const fs::path& pfad) {
    std::ifstream f(pfad, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(f)), {});
}

/// Eine Datei ohne Textende ^Z im Kopf der Diskettendatei: nur die ersten @p n Bytes vergleichen.
bool beginntMit(const Bytes& datei, const Bytes& soll) {
    return datei.size() >= soll.size() && std::equal(soll.begin(), soll.end(), datei.begin());
}

// ---------------------------------------------------------------------------------------------
// Ein Gast: Bildschirm lesen, tippen, laufen lassen - fuer alle Rechner gleich
// ---------------------------------------------------------------------------------------------
struct Gast {
    virtual ~Gast() = default;
    virtual long long lauf(long long takte) = 0;
    virtual std::vector<std::string> zeilen() = 0;
    virtual void taste(char c) = 0;                 // '\r' = Return
    virtual void kaltstart() = 0;                   // bis zum Prompt "A>"

    std::string bild() {
        std::string s;
        for (const auto& z : zeilen()) s += z + "\n";
        return s;
    }
    std::string letzte() {
        const auto z = zeilen();
        for (auto it = z.rbegin(); it != z.rend(); ++it)
            if (!it->empty()) return *it;
        return {};
    }
    void tippe(const std::string& s) { for (char c : s) taste(c); }
    bool bisText(const std::string& t, long long frist) {
        for (long long d = 0; d < frist; d += lauf(20'000))
            if (bild().find(t) != std::string::npos) return true;
        return bild().find(t) != std::string::npos;
    }
    /// Die letzte Zeile ist `A>` und bleibt es @p ruhig Takte lang.
    bool bisPrompt(long long frist, long long ruhig = 1'000'000) {
        long long still = 0;
        for (long long d = 0; d < frist;) {
            const long long n = lauf(20'000);
            d += n;
            still = (letzte() == "A>") ? still + n : 0;
            if (still >= ruhig) return true;
        }
        return false;
    }
    /// Kommando tippen, Return, bis der NEUE Prompt da ist.  @p zwischen: Text, auf den @p antwort getippt wird.
    bool befehl(const std::string& cmd, long long frist = 400'000'000, const std::string& zwischen = "",
                const std::string& antwort = "") {
        const std::string vorher = bild();
        tippe(cmd);
        taste('\r');
        bool echo = false, beantwortet = zwischen.empty();
        long long still = 0;
        for (long long d = 0; d < frist;) {
            const long long n = lauf(20'000);
            d += n;
            if (!beantwortet && bild().find(zwischen) != std::string::npos) {
                tippe(antwort);
                beantwortet = true;
                still = 0;
                continue;
            }
            const bool prompt = letzte() == "A>";
            echo = echo || !prompt || bild() != vorher;   // Echo schon vor der ersten Pruefung fertig: Bild hat sich geaendert
            still = (beantwortet && echo && prompt) ? still + n : 0;
            if (still >= 600'000) return true;
        }
        return false;
    }
};

/// Zerlegt eine 80-Spalten-Textflaeche in Zeilen (rechts gestutzt).
std::vector<std::string> zeilenVon(const std::string& t, size_t breite) {
    std::vector<std::string> z;
    for (size_t p = 0; p + breite <= t.size(); p += breite) {
        std::string s = t.substr(p, breite);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
        z.push_back(s);
    }
    return z;
}

struct GastA5120 : Gast {
    A5120Machine m;
    long long lauf(long long takte) override {
        long long d = 0;
        while (d < takte) d += m.run(5'000);
        return d;
    }
    std::vector<std::string> zeilen() override { return zeilenVon(k1520test::visibleText(m), 80); }
    void taste(char c) override {
        if (c == '\r') k1520test::typeKey(m, k1520test::QK_RETURN);
        else k1520test::typeString(m, std::string(1, c));
    }
    void kaltstart() override {
        m.powerOn();
        if (bisText("Bitte Uhrzeit eingeben!", 60'000'000)) {   // CP/A fragt die Uhrzeit ab (SCPX nicht)
            lauf(2'000'000);
            tippe("120000");
            taste('\r');
        }
        bisPrompt(150'000'000, 3'000'000);
    }
};

struct GastK8915 : Gast {
    K8915Machine m;
    GastK8915() = default;
    explicit GastK8915(K8915Machine::Config c) : m(c), gen2(c.generation == K8915Machine::Generation::Gen2) {}
    bool gen2 = false;
    long long lauf(long long takte) override {
        long long d = 0;
        while (d < takte) d += m.run(20'000);
        return d;
    }
    std::vector<std::string> zeilen() override {
        std::vector<std::string> z;
        std::istringstream in(k1520test::vramLines(m));
        for (std::string s; std::getline(in, s);) {
            while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
            z.push_back(s);
        }
        return z;
    }
    void taste(char c) override {
        m.keyboard().sendeZeichen(uint8_t(c == '\r' ? 0x0D : c));
        for (int i = 0; i < 4000 && m.keyboard().sendetNoch(); ++i) m.run(20'000);
        lauf(100'000);
    }
    void kaltstart() override {
        m.powerOn();
        if (!gen2) {
            m.zre().bankPoke(0, 0x0000, 0xC3);   // Selbsttest uebergehen (wie test_k8915_scpx.cpp)
            m.zre().bankPoke(0, 0x0005, 0xC3);
        }
        bisText("* Coldstart *  Disk on A: ready", gen2 ? 100'000'000 : 5'000'000);
        if (gen2) lauf(5'000'000);   // Selbsttest der K2521 laeuft noch, die Tastatur nimmt erst danach an
        m.keyboard().sendeZeichen(0x0D);
        bisPrompt(200'000'000, 3'000'000);
    }
};

struct GastPrg : Gast {
    Prg710Machine m;
    explicit GastPrg(Prg710Machine::Config c) : m(c) {}
    long long lauf(long long takte) override {
        long long d = 0;
        while (d < takte) d += m.run(20'000);
        return d;
    }
    std::vector<std::string> zeilen() override {
        std::vector<std::string> z;
        for (int r = 0; r < 24; ++r) {
            std::string s;
            for (int c = 0; c < 80; ++c) {
                const uint8_t ch = uint8_t(m.screenChar(c, r) & 0x7F);
                s += (ch >= 0x20 && ch < 0x7F) ? char(ch) : ' ';
            }
            while (!s.empty() && s.back() == ' ') s.pop_back();
            z.push_back(s);
        }
        return z;
    }
    void taste(char c) override {
        const uint32_t k = (c == '\r') ? 0x01000004u : uint8_t(c);
        m.keyPress(k, false, false);
        lauf(100'000);
        m.keyRelease(k);
        lauf(100'000);
    }
    void kaltstart() override {
        m.powerOn();
        lauf(3'000'000);
        taste('\r');
        bisPrompt(120'000'000, 2'000'000);
    }
};

struct GastPc : Gast {
    Pc1715Machine m;
    bool w = false;
    GastPc() = default;
    explicit GastPc(Pc1715Machine::Config c) : m(c), w(c.variante == Pc1715Machine::Config::Variante::Pc1715W) {}
    long long lauf(long long takte) override {
        k1520test::pc1715::laufe(m, takte);
        return takte;
    }
    std::vector<std::string> zeilen() override {
        std::vector<std::string> z;
        for (int r = 0; r < m.zre().maxZeilen(); ++r) {
            std::string s = k1520test::pc1715::zeile(m, r);
            if (s.find("**CP/A**") == std::string::npos) z.push_back(s);   // CP/A-Statuszeile ist kein Bildinhalt
        }
        return z;
    }
    void taste(char c) override { k1520test::pc1715::tippe(m, std::string(1, c)); }
    void kaltstart() override {
        m.powerOn();
        if (k1520test::pc1715::laufeBisText(m, "Uhrzeit in der Form HH:MM", 40'000'000)) {   // CP/A 1715 fragt, SCP nicht
            tippe("12:00");
            taste('\r');
        }
        if (w) {   // SCP 3.0 fuehrt PROFILE.SUB aus (modcs ...) - erst danach ist der Prompt der echte
            bisText("modcs sc619.zgf[1]", 200'000'000);
            lauf(30'000'000);
        }
        k1520test::pc1715::laufeBisPrompt(m, "A>", 80'000'000);
        lauf(2'000'000);
    }
};

struct Leise : ::testing::Test {
    void SetUp() override { k1520::logging::Logger::instance().setBaseLevel(k1520::logging::Level::WARN); }
};
using KartenBeigaben = Leise;

/// Diagnose: K1520_TEST_ZEIGEN nennt eine Datei, an die der Bildschirm angehaengt wird.
void zeige(const char* was, Gast& g) {
    if (const char* d = std::getenv("K1520_TEST_ZEIGEN")) {
        std::ofstream f(d, std::ios::app);
        f << "=== " << was << " ===\n" << g.bild() << "\n";
    }
}

// ---------------------------------------------------------------------------------------------
// Die drei Pruefabschnitte (gleich fuer alle Rechner, nur die Basen/Treiber unterscheiden sich)
// ---------------------------------------------------------------------------------------------
enum class Treiber { Poppe, Kingstener, Keiner };   // Keiner: nur die Uhr der Karte (SCP 3.0)

k1520::Gide::Config gideCfg() {
    k1520::Gide::Config c;
    c.basis = 0x90;          // Vorauswahl der Oberflaeche auf allen Rechnern
    return c;
}
k1520::Gide::RtcConfig rtcCfg() {
    k1520::Gide::RtcConfig r;
    r.zeitquelle = [] { return T0; };
    return r;
}

/// Gesteckt (vor dem Start): GIDE mit leerem Plattenabbild auf 90H.
template <class M>
std::string gideStecken(M& m, Treiber t) {
    const std::string abbild = k1520test::tempPath(t == Treiber::Poppe ? "k1520_karten_poppe.img" : "k1520_karten_gidn.img");
    std::error_code ec;
    fs::remove(abbild, ec);
    // Poppe rechnet mit 500 x 16 x 63 Sektoren, Kingstener mit 256 x 4 x 64
    if (!k1520::AtaGeraet::abbildAnlegen(abbild, t == Treiber::Poppe ? 504'100 : 65'600)) return "";
    const auto rc = rtcCfg();
    if (!m.installGide(gideCfg(), &rc, abbild)) {
        ADD_FAILURE() << "GIDE: " << m.gideFehler();
        return "";
    }
    return abbild;
}

/// GIDE-Abschnitt: Treiber, Datei GIDE.TXT hin und zurueck (bitgleich), Uhr anzeigen und stellen.
void gidePruefen(Gast& g, Treiber t, bool pip, const std::string& diskette, const std::string& abbild) {
    if (t == Treiber::Keiner) {
        // kein Treiber: nur die Uhr der Karte
    } else if (t == Treiber::Poppe) {
        ASSERT_TRUE(g.befehl("GIDEHP90")) << g.bild();
        zeige("GIDEHP90", g);
        EXPECT_NE(g.bild().find("GIDE-Modul auf Adresse 90H"), std::string::npos) << g.bild();
        EXPECT_NE(g.bild().find("GIDE-Treiber installiert"), std::string::npos) << g.bild();
        // Erstes neu installiertes Laufwerk aus der Meldung ("Neu installierte GIDE-Laufwerke:\nD: E: ...")
        const std::string b = g.bild();
        const size_t p = b.find("Neu installierte GIDE-Laufwerke:");
        ASSERT_NE(p, std::string::npos) << b;
        const size_t q = b.find_first_of("ABCDEFGHIJKLMNOP", p + 32);
        ASSERT_NE(q, std::string::npos) << b;
        const std::string lw(1, b[q]);
        // Verzeichnis des neuen Laufwerks anlegen (Frage mit Y beantworten)
        ASSERT_TRUE(g.befehl("ERADIR " + lw + ":", 400'000'000, "(y/n, CR=n)", "Y\r")) << g.bild();
        EXPECT_NE(g.bild().find("Successful Directory erase"), std::string::npos) << g.bild();
        ASSERT_TRUE(g.befehl("DIR " + lw + ":")) << g.bild();
        ASSERT_TRUE(g.befehl("DIR A:")) << g.bild();
        ASSERT_TRUE(g.befehl("PIP " + lw + ":=A:GIDE.TXT")) << g.bild();
        ASSERT_TRUE(g.befehl("DIR " + lw + ":")) << g.bild();
        EXPECT_NE(g.bild().find(lw + ": GIDE     TXT"), std::string::npos) << g.bild();
        ASSERT_TRUE(g.befehl("PIP A:KOPIE.TXT=" + lw + ":GIDE.TXT")) << g.bild();
    } else {
        ASSERT_TRUE(g.befehl("GIDN90 /F", 400'000'000, "formatieren J/N?", "J")) << g.bild();
        zeige("GIDN90 /F", g);
        EXPECT_NE(g.bild().find("GIDE als Laufwerk N: installiert"), std::string::npos) << g.bild();
        if (pip) {
            ASSERT_TRUE(g.befehl("PIP N:=A:GIDE.TXT")) << g.bild();
            ASSERT_TRUE(g.befehl("DIR N:")) << g.bild();
            EXPECT_NE(g.bild().find("N: GIDE     TXT"), std::string::npos) << g.bild();
            ASSERT_TRUE(g.befehl("PIP A:KOPIE.TXT=N:GIDE.TXT")) << g.bild();
        } else {
            // SCP 1715 hat kein brauchbares PIP: SAVE schreibt eine Seite des Speichers auf N:
            ASSERT_TRUE(g.befehl("SAVE 1 N:SPEICHER.BIN")) << g.bild();
            ASSERT_TRUE(g.befehl("DIR N:")) << g.bild();
            EXPECT_NE(g.bild().find("N: SPEICHER BIN"), std::string::npos) << g.bild();
        }
    }
    // Uhr: die Zeit des Tests (T0 = 10.11.2026 12:34:56 UTC) plus ein paar Sekunden Maschinenzeit
    ASSERT_TRUE(g.befehl("UHR")) << g.bild();
    zeige("UHR", g);
    EXPECT_NE(g.bild().find("10.11.2026 12:3"), std::string::npos) << g.bild();
    ASSERT_TRUE(g.befehl("UHR S 24.12.26 18:30:00")) << g.bild();
    zeige("UHR S", g);
    EXPECT_NE(g.bild().find("24.12.2026 18:30:"), std::string::npos) << g.bild();
    // falsche Basis: Meldung statt Haenger
    ASSERT_TRUE(g.befehl("UHR B0")) << g.bild();
    EXPECT_NE(g.bild().find("Keine Uhr gefunden"), std::string::npos) << g.bild();
    (void)diskette;
    (void)abbild;
}

/// Nach dem Lauf: KOPIE.TXT auf der Diskette ist GIDE.TXT (bitgleich), und die Nutzdaten stehen im Plattenabbild.
void gideNachpruefen(bool pip, const std::string& diskette, const std::string& abbild) {
    if (diskette.empty()) return;
    if (!pip) {   // SAVE: der Verzeichniseintrag steht im Abbild
        const Bytes img = dateiLesen(abbild);
        const std::string name = "SPEICHERBIN";
        EXPECT_NE(std::search(img.begin(), img.end(), name.begin(), name.end()), img.end())
            << "SPEICHER.BIN steht nicht im Plattenabbild";
        return;
    }
    const Bytes soll = vonDiskette(diskette, "GIDE.TXT");
    const Bytes kopie = vonDiskette(diskette, "KOPIE.TXT");
    ASSERT_FALSE(soll.empty());
    EXPECT_EQ(kopie, soll) << "Rueckkopie von der IDE-Platte weicht ab";
    const Bytes img = dateiLesen(abbild);
    ASSERT_GE(soll.size(), 200u);
    const Bytes probe(soll.begin() + 64, soll.begin() + 128);
    EXPECT_NE(std::search(img.begin(), img.end(), probe.begin(), probe.end()), img.end())
        << "GIDE.TXT steht nicht im Plattenabbild";
}

/// USB-Abschnitt (Programme USB/UDIR/UGET mit dem Suffix der Basis).
void usbPruefen(Gast& g, const std::string& suffix) {
    ASSERT_TRUE(g.befehl("USB" + suffix + " FWV", 600'000'000)) << g.bild();
    zeige("USB FWV", g);
    EXPECT_EQ(g.bild().find("Error"), std::string::npos) << g.bild();
    EXPECT_NE(g.bild().find("03.68"), std::string::npos) << g.bild();
    ASSERT_TRUE(g.befehl("UDIR" + suffix, 600'000'000)) << g.bild();
    zeige("UDIR", g);
    EXPECT_NE(g.bild().find("STICK"), std::string::npos) << g.bild();
    ASSERT_TRUE(g.befehl("UGET" + suffix + " STICK.BIN", 600'000'000)) << g.bild();
    zeige("UGET", g);
}

/// Netz-Abschnitt: TFTP und CPMNET erkennen die Karte.
void netzPruefen(Gast& g) {
    ASSERT_TRUE(g.befehl("TFTP", 600'000'000)) << g.bild();
    zeige("TFTP", g);
    EXPECT_NE(g.bild().find("TFTP Client&Server CP/M 2.x"), std::string::npos) << g.bild();
    EXPECT_NE(g.bild().find("Version 1.2"), std::string::npos) << g.bild();
    EXPECT_EQ(g.bild().find("not found"), std::string::npos) << g.bild();
    ASSERT_TRUE(g.befehl("TFTP -h", 600'000'000)) << g.bild();
    EXPECT_NE(g.bild().find("network not configured !"), std::string::npos) << g.bild();
    EXPECT_EQ(g.bild().find("network-cable"), std::string::npos) << g.bild();
    g.tippe("CPMNET");
    g.taste('\r');
    ASSERT_TRUE(g.bisText("NETWORK MENU", 600'000'000)) << g.bild();
    g.taste('I');
    g.lauf(10'000'000);
    zeige("CPMNET I", g);
    EXPECT_NE(g.bild().find("Software 1.2"), std::string::npos) << g.bild();
    EXPECT_NE(g.bild().find("Hardware 1.0"), std::string::npos) << g.bild();
    EXPECT_NE(g.bild().find("online"), std::string::npos) << g.bild();
}

struct Stick {
    fs::path ordner = k1520test::tempPath("k1520_karten_stick");
    Bytes datei;
    Stick() {
        std::error_code ec;
        fs::remove_all(ordner, ec);
        fs::create_directories(ordner);
        datei.resize(2000);
        for (size_t i = 0; i < datei.size(); ++i) datei[i] = uint8_t((i * 13 + 5 + (i >> 7)) & 0x7F) | 0x20;
        std::ofstream f(ordner / "STICK.BIN", std::ios::binary);
        f.write(reinterpret_cast<const char*>(datei.data()), std::streamsize(datei.size()));
    }
    ~Stick() { std::error_code ec; fs::remove_all(ordner, ec); }
};

template <class M>
void usbStecken(M& m, uint8_t basis, const Stick& s) {
    k1520::GunUsb::Config gc;
    gc.basis = basis;
    gc.ordner = s.ordner;
    ASSERT_TRUE(m.installGunUsb(gc)) << m.gunUsbFehler();
}
template <class M>
void netzStecken(M& m, uint8_t basis) {
    k1520::GunNet::Config gc;
    gc.basis = basis;
    gc.richtlinie.aussenErlaubt = false;   // der TEST bleibt auf Loopback
    ASSERT_TRUE(m.installGunNet(gc)) << m.gunNetFehler();
}

/// Eine ganze Pruefung je Rechner: GIDE+Uhr, USB, Netz auf EINER gebooteten Maschine (jede Karte auf ihrer Vorauswahl-Basis).
struct Satz {
    const char* rel;            ///< Diskette unter disks/
    Treiber treiber;
    uint8_t usb, net;           ///< net 0 = keine Netzprogramme auf der Diskette
    const char* usbSuffix = "";
    bool pip = true;            ///< false: kein PIP auf der Diskette (SCP 1715)
};

/// Gemeinsamer Ablauf; @p g ist der (noch nicht gestartete) Gast, @p m seine Maschine.
template <class G, class M>
void alles(G& g, M& m, const Satz& s, const char* name) {
    Lieferkopie kopie(s.rel, name);
    ASSERT_TRUE(kopie.ok()) << "Lieferdiskette fehlt: " << s.rel;
    Stick stick;
    const std::string abbild = gideStecken(m, s.treiber);
    ASSERT_FALSE(abbild.empty());
    ASSERT_NO_FATAL_FAILURE(usbStecken(m, s.usb, stick));
    if (s.net) ASSERT_NO_FATAL_FAILURE(netzStecken(m, s.net));
    ASSERT_TRUE(m.mountDisk(0, kopie.path(), m.defaultFormatName(0), false)) << m.lastError();
    g.kaltstart();
    ASSERT_EQ(g.letzte(), "A>") << g.bild();

    ASSERT_NO_FATAL_FAILURE(gidePruefen(g, s.treiber, s.pip, kopie.path(), abbild));
    ASSERT_NO_FATAL_FAILURE(usbPruefen(g, s.usbSuffix));
    if (s.net) ASSERT_NO_FATAL_FAILURE(netzPruefen(g));
    ASSERT_TRUE(m.flushDisks());
    ASSERT_TRUE(m.gide()->platte().flush());
    // Nachpruefung an der geschlossenen Kopie
    gideNachpruefen(s.pip, s.treiber == Treiber::Keiner ? "" : kopie.path(), abbild);
    const Bytes stickDatei = vonDiskette(kopie.path(), "STICK.BIN");
    EXPECT_TRUE(beginntMit(stickDatei, stick.datei)) << "UGET-Datei weicht ab (" << stickDatei.size() << " Bytes)";
    std::error_code ec;
    fs::remove(abbild, ec);
}

}  // namespace

/// A5120 mit CP/A: Poppe-Treiber (GIDEHP90, ERADIR), UHR, USB 84H, Netz C0H.
TEST_F(KartenBeigaben, A5120_CPA) {
    GastA5120 g;
    alles(g, g.m, {"a5120_cpa/a5120_cpa_k5601_system.hfe", Treiber::Poppe, 0x84, 0xC0}, "k1520_karten_a5120_cpa.hfe");
}

/// A5120 mit SCPX 1.7: Kingstener-Treiber (GIDN90), UHR, USB 84H, Netz C0H.
TEST_F(KartenBeigaben, A5120_SCPX17) {
    GastA5120 g;
    alles(g, g.m, {"a5120_scpx17/a5120_scpx17_k5601_system.hfe", Treiber::Kingstener, 0x84, 0xC0},
          "k1520_karten_a5120_scpx.hfe");
}

/// K8915 V3 mit SCPX 8915: GIDN90, USB 84H, Netz C0H.
TEST_F(KartenBeigaben, K8915_V3_SCPX) {
    GastK8915 g;
    alles(g, g.m, {"k8915_scpx8915/k8915_scpx8915_v24_system.hfe", Treiber::Kingstener, 0x84, 0xC0},
          "k1520_karten_k8915.hfe");
}

/// PRG 710-1 mit SCPX 1.7: GIDN90, USB 8CH (84H gehoert der ZRE), Netz 28H (C0H gehoert dem ATP-Zusatz).
TEST_F(KartenBeigaben, PRG710_1_SCPX) {
    Prg710Machine::Config c;
    c.variante = Prg710Machine::Config::Variante::Prg710_1;
    GastPrg g(c);
    alles(g, g.m, {"prg710-1_scpx17/prg710-1_scpx17_system.hfe", Treiber::Kingstener, 0x8C, 0x28}, "k1520_karten_prg710_1.hfe");
}

/// PRG 710 mit SCPX 1.5: wie der 710-1.
TEST_F(KartenBeigaben, PRG710_SCPX15) {
    Prg710Machine::Config c;
    c.variante = Prg710Machine::Config::Variante::Prg710;
    GastPrg g(c);
    alles(g, g.m, {"prg710_scpx15/prg710_scpx15_system.hfe", Treiber::Poppe, 0x8C, 0x28}, "k1520_karten_prg710.hfe");
}

/// PC 1715 mit CP/A 1715: Poppe-Treiber, USB 84H, Netz C0H.
TEST_F(KartenBeigaben, PC1715_CPA1715) {
    GastPc g;
    alles(g, g.m, {"pc1715_cpa1715/pc1715_cpa1715_system.hfe", Treiber::Poppe, 0x84, 0xC0}, "k1520_karten_pc1715_cpa.hfe");
}

/// PC 1715 mit SCP 1715 V0006 (Platte: Kingstener).
TEST_F(KartenBeigaben, PC1715_SCP_V0006) {
    GastPc g;
    alles(g, g.m, {"pc1715_scp1715_v0006/pc1715_scp1715_v0006_system.hfe", Treiber::Kingstener, 0x84, 0xC0, "", false},
          "k1520_karten_pc1715_v6.hfe");
}

/// PC 1715 mit SCP 1715 V0007.
TEST_F(KartenBeigaben, PC1715_SCP_V0007) {
    GastPc g;
    alles(g, g.m, {"pc1715_scp1715_v0007/pc1715_scp1715_v0007_system.hfe", Treiber::Kingstener, 0x84, 0xC0, "", false},
          "k1520_karten_pc1715_v7.hfe");
}

/// PC 1715W mit SCP 3.0: Uhr und USB (kein IDE-Treiber, Netz passt nicht mehr auf die Diskette).
TEST_F(KartenBeigaben, PC1715W_SCP30) {
    Pc1715Machine::Config c;
    c.variante = Pc1715Machine::Config::Variante::Pc1715W;
    GastPc g(c);
    alles(g, g.m, {"pc1715w_scp30/pc1715w_scp30_system.hfe", Treiber::Keiner, 0x84, 0}, "k1520_karten_pc1715w.hfe");
}

/// K8915 Gen 2 (V2-Geraet, K2521 + 64 KB): USB 8CH (84H gehoert der K2521), GIDE 90H, Netz C0H; USB-Programme mit Suffix 8C.
TEST_F(KartenBeigaben, K8915_Gen2_SCPX) {
    K8915Machine::Config c;
    c.generation = K8915Machine::Generation::Gen2;
    GastK8915 g(c);
    alles(g, g.m, {"k8915_scpx8915/k8915_scpx8915_v24_system.hfe", Treiber::Kingstener, 0x8C, 0xC0, "8C"},
          "k1520_karten_k8915g2.hfe");
}
