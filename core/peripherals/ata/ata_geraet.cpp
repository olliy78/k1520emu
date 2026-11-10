/**
 * @file ata_geraet.cpp
 * @brief ATA-/IDE-Festplatte, siehe ata_geraet.h (Entwurf 50, AP G1).
 */
#include "core/peripherals/ata/ata_geraet.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "core/util/os_compat.h"
#if defined(_WIN32)
#  include <fcntl.h>
#  include <io.h>
#  include <sys/stat.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

#if !defined(_WIN32)
#  include <sys/types.h>
#  include <signal.h>
#endif

namespace k1520 {

namespace {
constexpr uint32_t MAX_ZYL = 16383;

void setzeString(uint16_t* w, size_t wort0, size_t nwoerter, const char* text) {
    // ATA-Konvention: je Wort steht das ERSTE Zeichen im hoeheren Byte; mit Leerzeichen aufgefuellt.
    std::string s(text);
    s.resize(nwoerter * 2, ' ');
    for (size_t i = 0; i < nwoerter; ++i)
        w[wort0 + i] = static_cast<uint16_t>((static_cast<uint8_t>(s[2 * i]) << 8) | static_cast<uint8_t>(s[2 * i + 1]));
}

// Sperrdatei EXKLUSIV anlegen (existiert sie schon: false).  fopen("wx") reicht nicht: die
// alte MSVCRT/MinGW-Laufzeit ignoriert das 'x', die Sperre waere dort nie exklusiv.
bool sperrdateiExklusivAnlegen(const std::string& pfad, long pid) {
#if defined(_WIN32)
    const int fd = ::_open(pfad.c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    const int fd = ::open(pfad.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
#endif
    if (fd < 0) return false;
    char b[32];
    const int n = std::snprintf(b, sizeof b, "%ld\n", pid);
#if defined(_WIN32)
    (void)::_write(fd, b, static_cast<unsigned>(n));
    ::_close(fd);
#else
    (void)::write(fd, b, static_cast<size_t>(n));
    ::close(fd);
#endif
    return true;
}

bool prozessLebt(long pid) {
#if defined(_WIN32)
    (void)pid;
    return true;
#else
    if (pid <= 0) return false;
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno != ESRCH;
#endif
}
}  // namespace

AtaGeraet::AtaGeraet() : AtaGeraet(Config{}) {}
AtaGeraet::AtaGeraet(const Config& cfg) : cfg_(cfg) {}
AtaGeraet::~AtaGeraet() { auswerfen(); }

AtaGeraet::Geometrie AtaGeraet::geometrieAbleiten(uint64_t sektoren) {
    Geometrie g;
    if (sektoren < 16ull * 63) {
        g.koepfe = 4;
        g.sektoren = 16;
    } else {
        g.koepfe = 16;
        g.sektoren = 63;
    }
    uint64_t z = sektoren / (static_cast<uint64_t>(g.koepfe) * g.sektoren);
    g.zylinder = static_cast<uint32_t>(std::clamp<uint64_t>(z, 1, MAX_ZYL));
    return g;
}

// ------------------------------------------------------------------ Medium

bool AtaGeraet::sperreAnlegen(std::string* fehler) {
    sperre_ = pfad_ + ".lock";
    for (int versuch = 0; versuch < 2; ++versuch) {
        if (sperrdateiExklusivAnlegen(sperre_, os::processId())) return true;
        // Bestehende Sperre: nur uebernehmen, wenn ihr Prozess nicht mehr lebt.
        long pid = -1;
        if (FILE* f = std::fopen(sperre_.c_str(), "r")) {
            if (std::fscanf(f, "%ld", &pid) != 1) pid = -1;
            std::fclose(f);
        }
        if (versuch == 0 && !prozessLebt(pid)) {
            std::error_code ec;
            std::filesystem::remove(sperre_, ec);
            continue;
        }
        break;
    }
    if (fehler) *fehler = "Abbild ist gesperrt (" + sperre_ + "): schon in Benutzung";
    sperre_.clear();
    return false;
}

void AtaGeraet::sperreLoesen() {
    if (!sperre_.empty()) {
        std::error_code ec;
        std::filesystem::remove(sperre_, ec);
        sperre_.clear();
    }
}

bool AtaGeraet::abbildOeffnen(const std::string& pfad, Geometrie g, std::string* fehler) {
    auswerfen();
    pfad_ = pfad;
    if (!sperreAnlegen(fehler)) {
        pfad_.clear();
        return false;
    }
    std::vector<uint8_t> daten;
    gzip::Art art = gzip::Art::Roh;
    std::string f2;
    if (!gzip::laden(pfad, daten, &art, &f2)) {
        if (fehler) *fehler = "Abbild nicht lesbar: " + pfad + (f2.empty() ? "" : " (" + f2 + ")");
        sperreLoesen();
        pfad_.clear();
        return false;
    }
    if (daten.empty() || daten.size() % SEKTORBYTES != 0) {
        if (fehler) *fehler = "Abbildgroesse ist kein Vielfaches von 512 Byte: " + pfad;
        sperreLoesen();
        pfad_.clear();
        return false;
    }
    img_ = std::move(daten);
    art_ = art;
    gesamt_ = img_.size() / SEKTORBYTES;
    phys_ = geometrieAbleiten(gesamt_);
    if (g.zylinder) phys_.zylinder = g.zylinder;
    if (g.koepfe) phys_.koepfe = std::clamp<uint32_t>(g.koepfe, 1, 16);
    if (g.sektoren) phys_.sektoren = std::clamp<uint32_t>(g.sektoren, 1, 255);
    if (!g.zylinder && (g.koepfe || g.sektoren)) {
        uint64_t z = gesamt_ / (static_cast<uint64_t>(phys_.koepfe) * phys_.sektoren);
        phys_.zylinder = static_cast<uint32_t>(std::clamp<uint64_t>(z, 1, MAX_ZYL));
    }
    akt_ = phys_;
    schmutzig_bits_.assign(gesamt_, false);
    schmutzig_liste_.clear();
    letzt_datei_ = {};
    medium_ = true;
    return true;
}

bool AtaGeraet::abbildAnlegen(const std::string& pfad, uint64_t sektoren, std::string* fehler) {
    if (sektoren == 0) {
        if (fehler) *fehler = "Abbild mit 0 Sektoren";
        return false;
    }
    std::error_code ec;
    if (std::filesystem::exists(pfad, ec)) {
        if (fehler) *fehler = "Datei existiert schon: " + pfad;
        return false;
    }
    const uint64_t bytes = sektoren * SEKTORBYTES;
    if (gzip::artNachEndung(pfad) == gzip::Art::Gzip) {
        std::vector<uint8_t> nullen(bytes, 0);
        return gzip::speichern(pfad, nullen.data(), nullen.size(), gzip::Art::Gzip, gzip::STUFE_VORGABE, fehler);
    }
    std::ofstream o(pfad, std::ios::binary);
    if (!o) {
        if (fehler) *fehler = "Datei nicht anlegbar: " + pfad;
        return false;
    }
    o.seekp(static_cast<std::streamoff>(bytes - 1));
    o.put('\0');
    o.close();
    if (!o) {
        if (fehler) *fehler = "Schreibfehler: " + pfad;
        std::filesystem::remove(pfad, ec);
        return false;
    }
    return true;
}

void AtaGeraet::warteFaden() {
    if (faden_.joinable()) faden_.join();
}

void AtaGeraet::markiereSchmutzig(uint64_t lba) {
    if (lba < schmutzig_bits_.size() && !schmutzig_bits_[lba]) {
        schmutzig_bits_[lba] = true;
        schmutzig_liste_.push_back(lba);
    }
    schreib_takt_ = jetzt_;
}

// Auftrag schnuerren (im Wirtsfaden) und ausfuehren; async = in eigenem Faden.
bool AtaGeraet::schreibeJetzt(bool async) {
    warteFaden();
    if (!medium_ || schmutzig_liste_.empty()) return !schreibfehler_->load();
    struct Auftrag {
        std::string pfad;
        gzip::Art art;
        std::vector<uint8_t> voll;
        std::vector<std::pair<uint64_t, std::vector<uint8_t>>> sektoren;
        std::shared_ptr<std::atomic<bool>> fehler;
    };
    auto a = std::make_shared<Auftrag>();
    a->pfad = pfad_;
    a->art = art_;
    a->fehler = schreibfehler_;
    if (art_ == gzip::Art::Gzip) {
        a->voll = img_;
    } else {
        std::sort(schmutzig_liste_.begin(), schmutzig_liste_.end());
        for (uint64_t l : schmutzig_liste_)
            a->sektoren.emplace_back(l, std::vector<uint8_t>(img_.begin() + l * SEKTORBYTES,
                                                              img_.begin() + (l + 1) * SEKTORBYTES));
    }
    for (uint64_t l : schmutzig_liste_) schmutzig_bits_[l] = false;
    schmutzig_liste_.clear();
    letzt_datei_ = std::chrono::steady_clock::now();
    auto job = [a] {
        bool ok = true;
        if (a->art == gzip::Art::Gzip) {
            ok = gzip::speichern(a->pfad, a->voll.data(), a->voll.size(), gzip::Art::Gzip);
        } else {
            std::fstream f(a->pfad, std::ios::in | std::ios::out | std::ios::binary);
            if (!f) ok = false;
            for (auto& s : a->sektoren) {
                if (!ok) break;
                f.seekp(static_cast<std::streamoff>(s.first * SEKTORBYTES));
                f.write(reinterpret_cast<const char*>(s.second.data()), static_cast<std::streamsize>(s.second.size()));
                if (!f) ok = false;
            }
            if (ok) {
                f.flush();
                ok = static_cast<bool>(f);
            }
        }
        if (!ok) a->fehler->store(true);
    };
    if (async)
        faden_ = std::thread(job);
    else
        job();
    return !schreibfehler_->load();
}

bool AtaGeraet::flush() {
    schreibeJetzt(false);
    return !schreibfehler_->load();
}

void AtaGeraet::autoFlush() {
    if (schmutzig_liste_.empty() || jetzt_ < schreib_takt_ + cfg_.flush_pause) return;
    if (art_ == gzip::Art::Gzip && letzt_datei_ != std::chrono::steady_clock::time_point{} &&
        std::chrono::steady_clock::now() - letzt_datei_ < std::chrono::milliseconds(cfg_.min_abstand_gz_ms))
        return;
    schreibeJetzt(true);
}

void AtaGeraet::auswerfen() {
    if (medium_) schreibeJetzt(false);
    warteFaden();
    sperreLoesen();
    medium_ = false;
    pfad_.clear();
    img_.clear();
    img_.shrink_to_fit();
    schmutzig_bits_.clear();
    schmutzig_liste_.clear();
    gesamt_ = 0;
    phys_ = akt_ = {};
    schreibfehler_->store(false);
    modus_ = Modus::Keiner;
    drq_ = busy_ = false;
}

// ------------------------------------------------------------------ Zeit, Reset

void AtaGeraet::takt(uint64_t t) {
    jetzt_ = t;
    if (busy_ && jetzt_ - bsy_start_ >= cfg_.bsy_max_takte) abschliessen();
    autoFlush();
}

void AtaGeraet::diagnoseWerte(bool sdh_setzen) {
    err_ = ER_DIAG_OK;
    sc_ = 1;
    sn_ = 1;
    cl_ = ch_ = 0;
    if (sdh_setzen) sdh_ = 0xA0;
}

void AtaGeraet::reset() {
    busy_ = drq_ = errbit_ = false;
    srst_ = false;
    modus_ = Modus::Keiner;
    rest_ = pos_ = 0;
    mult_ = 0;
    akt_ = phys_;
    diagnoseWerte(true);
}

// ------------------------------------------------------------------ Status

uint8_t AtaGeraet::status() const {
    if (slaveGewaehlt()) return 0;
    if (busy_ || srst_) return ST_BSY;
    return static_cast<uint8_t>(ST_DRDY | ST_DSC | (drq_ ? ST_DRQ : 0) | (errbit_ ? ST_ERR : 0));
}

uint8_t AtaGeraet::statusLesen() {
    if (!slaveGewaehlt() && busy_ && !srst_) {
        if (bsy_gesehen_ >= cfg_.bsy_abfragen) {
            abschliessen();
        } else {
            ++bsy_gesehen_;
            return ST_BSY;
        }
    }
    return status();
}

// ------------------------------------------------------------------ Registerzugriff

uint16_t AtaGeraet::lese(Reg reg, bool cs1) {
    const unsigned r = static_cast<unsigned>(reg) & 7;
    if (cs1) {
        if (r == 6) return statusLesen();
        if (r == 7) {
            const unsigned sel = slaveGewaehlt() ? 1 : 0;
            return static_cast<uint8_t>(0xC0 | ((~sdh_ & 0x0F) << 2) | ((sel == 1 ? 0 : 1) << 1) | (sel == 0 ? 0 : 1));
        }
        return 0;
    }
    if (r == 7) return statusLesen();
    if (slaveGewaehlt() && r != 6) return 0;
    switch (r) {
        case 0: return datenLesen();
        case 1: return err_;
        case 2: return sc_;
        case 3: return sn_;
        case 4: return cl_;
        case 5: return ch_;
        default: return sdh_;
    }
}

void AtaGeraet::schreibe(Reg reg, uint16_t wert, bool cs1) {
    const unsigned r = static_cast<unsigned>(reg) & 7;
    const uint8_t b = static_cast<uint8_t>(wert);
    if (cs1) {
        if (r != 6) return;
        const bool alt = srst_;
        nien_ = (b & 0x02) != 0;
        srst_ = (b & 0x04) != 0;
        if (srst_ && !alt) {                 // steigende Flanke: Geraet aufgeben, BSY
            busy_ = false;
            drq_ = errbit_ = false;
            modus_ = Modus::Keiner;
            rest_ = pos_ = 0;
        } else if (!srst_ && alt) {          // fallende Flanke: Diagnose laeuft, dann BSY weg
            reset();
            busy_ = true;
            cmd_ = 0xFF;                     // Marke "Soft-Reset": schliesst ohne Befehlswirkung ab
            bsy_gesehen_ = 0;
            bsy_start_ = jetzt_;
        }
        return;
    }
    if (r == 0) {
        datenSchreiben(wert);
        return;
    }
    if (busy_ || srst_) return;              // waehrend BSY sind Task-File-Schreibzugriffe wirkungslos
    switch (r) {
        case 1: features_ = b; break;
        case 2: sc_ = b; break;
        case 3: sn_ = b; break;
        case 4: cl_ = b; break;
        case 5: ch_ = b; break;
        case 6: sdh_ = b; break;
        default:
            if (!slaveGewaehlt()) befehl(b);
            break;
    }
}

// ------------------------------------------------------------------ Befehle

void AtaGeraet::befehl(uint8_t cmd) {
    cmd_ = cmd;
    err_ = 0;
    errbit_ = false;
    drq_ = false;
    modus_ = Modus::Keiner;
    rest_ = pos_ = 0;
    busy_ = true;
    bsy_gesehen_ = 0;
    bsy_start_ = jetzt_;
}

void AtaGeraet::fertig(bool fehler, uint8_t err) {
    busy_ = false;
    drq_ = false;
    modus_ = Modus::Keiner;
    rest_ = pos_ = 0;
    errbit_ = fehler;
    err_ = fehler ? err : 0;
}

bool AtaGeraet::adresse(uint64_t& lba) const {
    if (sdh_ & 0x40) {
        lba = (static_cast<uint64_t>(sdh_ & 0x0F) << 24) | (static_cast<uint64_t>(ch_) << 16) |
              (static_cast<uint64_t>(cl_) << 8) | sn_;
    } else {
        const uint32_t c = (static_cast<uint32_t>(ch_) << 8) | cl_, h = sdh_ & 0x0F, s = sn_;
        if (s == 0 || s > akt_.sektoren || h >= akt_.koepfe || c >= akt_.zylinder) return false;
        lba = (static_cast<uint64_t>(c) * akt_.koepfe + h) * akt_.sektoren + (s - 1);
    }
    return lba < gesamt_;
}

void AtaGeraet::adresseSetzen(uint64_t lba) {
    if (sdh_ & 0x40) {
        sn_ = lba & 0xFF;
        cl_ = (lba >> 8) & 0xFF;
        ch_ = (lba >> 16) & 0xFF;
        sdh_ = static_cast<uint8_t>((sdh_ & 0xF0) | ((lba >> 24) & 0x0F));
    } else if (akt_.sektoren && akt_.koepfe) {
        const uint64_t t = lba / akt_.sektoren;
        sn_ = static_cast<uint8_t>(lba % akt_.sektoren + 1);
        const uint64_t h = t % akt_.koepfe, c = t / akt_.koepfe;
        sdh_ = static_cast<uint8_t>((sdh_ & 0xF0) | h);
        cl_ = c & 0xFF;
        ch_ = (c >> 8) & 0xFF;
    }
}

void AtaGeraet::sektorLaden() {
    const uint8_t* p = img_.data() + lba_cur_ * SEKTORBYTES;
    for (unsigned i = 0; i < 256; ++i) puf_[i] = static_cast<uint16_t>(p[2 * i] | (p[2 * i + 1] << 8));
    pos_ = 0;
    adresseSetzen(lba_cur_);
}

void AtaGeraet::identifyFuellen() {
    std::memset(puf_, 0, sizeof(puf_));
    puf_[0] = 0x0040;                                    // feste Platte
    puf_[1] = static_cast<uint16_t>(phys_.zylinder);
    puf_[3] = static_cast<uint16_t>(phys_.koepfe);
    puf_[4] = static_cast<uint16_t>(phys_.sektoren * SEKTORBYTES);
    puf_[5] = SEKTORBYTES;
    puf_[6] = static_cast<uint16_t>(phys_.sektoren);
    setzeString(puf_, 10, 10, "K1520EMU0001");
    puf_[20] = 3;                                        // Puffertyp
    puf_[21] = 1;                                        // Puffergroesse in Sektoren
    setzeString(puf_, 23, 4, "1.00");
    setzeString(puf_, 27, 20, "K1520EMU IDE");
    puf_[47] = 0x8010;                                   // Multiple: bis 16 Sektoren
    puf_[49] = 0x0200;                                   // LBA unterstuetzt
    puf_[51] = 0x0200;
    puf_[53] = 0x0001;                                   // Worte 54-58 gueltig
    puf_[54] = static_cast<uint16_t>(akt_.zylinder);
    puf_[55] = static_cast<uint16_t>(akt_.koepfe);
    puf_[56] = static_cast<uint16_t>(akt_.sektoren);
    const uint64_t kap = static_cast<uint64_t>(akt_.zylinder) * akt_.koepfe * akt_.sektoren;
    puf_[57] = static_cast<uint16_t>(kap & 0xFFFF);
    puf_[58] = static_cast<uint16_t>((kap >> 16) & 0xFFFF);
    puf_[59] = mult_ ? static_cast<uint16_t>(0x0100 | mult_) : 0;
    const uint64_t lba28 = std::min<uint64_t>(gesamt_, 0x0FFFFFFF);
    puf_[60] = static_cast<uint16_t>(lba28 & 0xFFFF);
    puf_[61] = static_cast<uint16_t>((lba28 >> 16) & 0xFFFF);
    pos_ = 0;
}

void AtaGeraet::abschliessen() {
    if (!busy_) return;
    const uint8_t cmd = cmd_;
    busy_ = false;
    bsy_gesehen_ = 0;
    unsigned n = sc_ ? sc_ : 256;
    uint64_t lba = 0;
    auto abrt = [&] { fertig(true, ER_ABRT); };
    auto idnf = [&] { fertig(true, ER_IDNF); };

    switch (cmd) {
        case 0xFF:                                       // Ende Soft-Reset: Werte stehen schon
            fertig(false, ER_DIAG_OK);
            err_ = ER_DIAG_OK;
            break;
        case 0x90:
            diagnoseWerte(false);
            errbit_ = false;
            break;
        case 0x20: case 0x21: case 0xC4:
            if (!medium_ || (cmd == 0xC4 && mult_ == 0)) { abrt(); break; }
            if (!adresse(lba)) { idnf(); break; }
            rest_ = n;
            lba_cur_ = lba;
            sektorLaden();
            modus_ = Modus::Lesen;
            drq_ = true;
            break;
        case 0x30: case 0x31: case 0xC5:
            if (!medium_ || (cmd == 0xC5 && mult_ == 0)) { abrt(); break; }
            if (!adresse(lba)) { idnf(); break; }
            rest_ = n;
            lba_cur_ = lba;
            pos_ = 0;
            modus_ = Modus::Schreiben;
            drq_ = true;
            break;
        case 0x40: case 0x41:
            if (!medium_) { abrt(); break; }
            if (!adresse(lba) || lba + n > gesamt_) { idnf(); break; }
            adresseSetzen(lba + n - 1);
            sc_ = 0;
            break;
        case 0x91:
            if (sc_ == 0 || ((sdh_ & 0x0F) + 1u) * sc_ == 0) { abrt(); break; }
            akt_.sektoren = sc_;
            akt_.koepfe = (sdh_ & 0x0F) + 1u;
            if (gesamt_) {
                const uint64_t z = gesamt_ / (static_cast<uint64_t>(akt_.koepfe) * akt_.sektoren);
                akt_.zylinder = static_cast<uint32_t>(std::clamp<uint64_t>(z, 1, 65535));
            }
            break;
        case 0xEC:
            identifyFuellen();
            modus_ = Modus::Identify;
            drq_ = true;
            break;
        case 0xC6:
            if (sc_ == 0 || sc_ > 16) { abrt(); break; }
            mult_ = sc_;
            break;
        case 0xEF:                                       // Set Features: tolerant
            break;
        case 0xE5:                                       // Check Power Mode: aktiv
            sc_ = 0xFF;
            break;
        default:
            if ((cmd & 0xF0) == 0x10) {                  // Recalibrate
                cl_ = ch_ = 0;
                sn_ = 1;
                sdh_ &= 0xF0;
            } else if ((cmd & 0xF0) == 0x70) {           // Seek
                if (!medium_) break;
                if (sdh_ & 0x40) {
                    if (!adresse(lba)) idnf();
                } else if ((((static_cast<uint32_t>(ch_) << 8) | cl_) >= akt_.zylinder) ||
                           ((sdh_ & 0x0F) >= akt_.koepfe)) {
                    idnf();
                }
            } else if ((cmd & 0xF8) == 0xE0) {           // Standby/Idle/Sleep ...: tolerant
            } else {
                abrt();
            }
            break;
    }
}

// ------------------------------------------------------------------ Datenregister

uint16_t AtaGeraet::datenLesen() {
    if (!drq_ || busy_ || srst_ || (modus_ != Modus::Lesen && modus_ != Modus::Identify)) return 0;
    const uint16_t w = puf_[pos_++];
    if (pos_ >= 256) {
        pos_ = 0;
        if (modus_ == Modus::Identify) {
            fertig(false, 0);
        } else if (--rest_ > 0) {
            ++lba_cur_;
            if (lba_cur_ >= gesamt_) {
                fertig(true, ER_IDNF);
            } else {
                sektorLaden();
            }
        } else {
            fertig(false, 0);
        }
    }
    return w;
}

void AtaGeraet::datenSchreiben(uint16_t w) {
    if (!drq_ || busy_ || srst_ || modus_ != Modus::Schreiben) return;
    puf_[pos_++] = w;
    if (pos_ < 256) return;
    pos_ = 0;
    uint8_t* p = img_.data() + lba_cur_ * SEKTORBYTES;
    for (unsigned i = 0; i < 256; ++i) {
        p[2 * i] = static_cast<uint8_t>(puf_[i] & 0xFF);
        p[2 * i + 1] = static_cast<uint8_t>(puf_[i] >> 8);
    }
    markiereSchmutzig(lba_cur_);
    adresseSetzen(lba_cur_);
    if (--rest_ > 0) {
        ++lba_cur_;
        if (lba_cur_ >= gesamt_) fertig(true, ER_IDNF);
    } else {
        fertig(false, 0);
    }
}

}  // namespace k1520
