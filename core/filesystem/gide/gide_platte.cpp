/**
 * @file gide_platte.cpp
 * @brief Umsetzung von @ref GidePlatte - die Aufteilung der Platte nach den Quellen der Gast-Treiber.
 *
 * Belege (Quellen nur lokal, ~/Documents/K1520emu/GIDE_Software/):
 *  - Poppe `GIDEHP.ASM`: `rwoper` (Hostsektor = SEKSEC >> 2), `tfsetup` (SNum = Hostsektor+1, CLo/CHi = Spur,
 *    SDH = lwtab[Laufwerk] = A0H+n), `DPBHD0`/`DPBHD`.  Kein 91h, also Vorgabegeometrie 16 x 63.
 *  - Kingstener `gidn5004.mac`: `HDInit` (SC 40h, SDH A3H, 91h), `HdPrep` (Kopf = Spur und 3, Zylinder = Spur / 4,
 *    Sektor = Satz+1 ueber `p_rsectran`), `RdIDE` (4 x INIR (HL) ... in DENSELBEN 128-B-Puffer: das LETZTE Viertel
 *    zaehlt), `WrIDE` (4 x OTIR aus demselben Puffer: derselbe Satz vierfach), `rafdpb`, `format` (E5 + 31 x 00).
 */

#include "core/filesystem/gide/gide_platte.h"

#include "core/util/os_compat.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#if defined(_WIN32)
#  include <fcntl.h>
#  include <io.h>
#  include <sys/stat.h>
#else
#  include <fcntl.h>
#  include <signal.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace gl = gide_layout;

// ---------------------------------------------------------------------------------------------------------------
// Layout-Abbildungen
// ---------------------------------------------------------------------------------------------------------------

const char* gideLayoutName(GideLayout l) {
    switch (l) {
        case GideLayout::Poppe: return "poppe";
        case GideLayout::Kingstener: return "kingstener";
        default: return "auto";
    }
}

bool gideLayoutVon(const std::string& name, GideLayout& aus) {
    std::string n;
    for (char c : name) n += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (n == "auto" || n.empty()) { aus = GideLayout::Auto; return true; }
    if (n == "poppe") { aus = GideLayout::Poppe; return true; }
    if (n == "kingstener" || n == "king" || n == "dl") { aus = GideLayout::Kingstener; return true; }
    return false;
}

bool gide_layout::poppeVonLba(uint64_t lba, unsigned& lw, unsigned& spur, unsigned& hostsektor) {
    hostsektor = static_cast<unsigned>(lba % kPoppeSektoren);
    const uint64_t rest = lba / kPoppeSektoren;
    lw = static_cast<unsigned>(rest % kPoppeKoepfe);
    const uint64_t z = rest / kPoppeKoepfe;
    if (hostsektor >= kPoppeHostsektoren || lw >= kPoppeLaufwerke || z >= kPoppeSpuren) return false;
    spur = static_cast<unsigned>(z);
    return true;
}

bool gide_layout::kingVonLba(uint64_t lba, unsigned& spur, unsigned& satz) {
    if (lba >= kKingMindestSektoren) return false;
    spur = static_cast<unsigned>(lba / kKingSektoren);
    satz = static_cast<unsigned>(lba % kKingSektoren);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Sperrdatei (wie die Karte: AtaGeraet) - Prozessnummer, Sperre eines toten Prozesses wird uebernommen
// ---------------------------------------------------------------------------------------------------------------

namespace {

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

/// Sperre lebt (Datei vorhanden UND Prozess lebt)?
bool sperreLebt(const std::string& sperre) {
    std::error_code ec;
    if (!fs::exists(sperre, ec)) return false;
    long pid = -1;
    if (FILE* f = std::fopen(sperre.c_str(), "r")) {
        if (std::fscanf(f, "%ld", &pid) != 1) pid = -1;
        std::fclose(f);
    }
    return prozessLebt(pid);
}

}  // namespace

bool GidePlatte::sperreAnlegen(std::string& err) {
    const std::string s = pfad_ + ".lock";
    for (int versuch = 0; versuch < 2; ++versuch) {
        if (sperrdateiExklusivAnlegen(s, k1520::os::processId())) { sperre_ = s; return true; }
        if (versuch == 0 && !sperreLebt(s)) {
            std::error_code ec;
            fs::remove(s, ec);
            continue;
        }
        break;
    }
    err = "Abbild gesperrt (" + s + "): schon in Benutzung (laufender Emulator oder anderes Werkzeug)";
    return false;
}

void GidePlatte::sperreLoesen() {
    if (!sperre_.empty()) {
        std::error_code ec;
        fs::remove(sperre_, ec);
        sperre_.clear();
    }
}

GidePlatte::~GidePlatte() { sperreLoesen(); }

// ---------------------------------------------------------------------------------------------------------------
// Verzeichnis-Plausibilitaet
// ---------------------------------------------------------------------------------------------------------------

bool GidePlatte::verzeichnisPlausibel(const uint8_t* d, size_t eintraege, unsigned dsm_bloecke, int* belegt) {
    int b = 0;
    for (size_t i = 0; i < eintraege; ++i) {
        const uint8_t* e = d + i * 32;
        if (e[0] == 0xE5) continue;     // frei - Rest beliebig (E5 + 31 x E5 bzw. E5 + 31 x 00)
        if (e[0] > 15) return false;    // Nutzerbereich 0..15
        for (int k = 1; k <= 11; ++k) {
            const uint8_t c = e[k] & 0x7F;   // Hochbits = R/O, SYS, ARC
            if (c < 0x20 || c > 0x7E) return false;
        }
        if (e[1] == ' ') return false;       // leerer Name
        if (e[12] > 31 || e[13] != 0 || e[14] > 0x3F || e[15] > 0x80) return false;
        for (int k = 0; k < 8; ++k) {
            const unsigned p = e[16 + 2 * k] | (e[17 + 2 * k] << 8);
            if (p >= dsm_bloecke) return false;
        }
        ++b;
    }
    if (belegt) *belegt = b;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Laden, Erkennen
// ---------------------------------------------------------------------------------------------------------------

bool GidePlatte::kandidat(const std::string& pfad) {
    std::error_code ec;
    if (!fs::is_regular_file(pfad, ec)) return false;
    uint64_t n = 0;
    const auto art = k1520::gzip::artVon(pfad);
    if (!art) return false;
    if (*art == k1520::gzip::Art::Gzip) {
        const auto g = k1520::gzip::inhaltsGroesse(pfad);
        if (!g) return false;
        n = *g;
    } else {
        n = static_cast<uint64_t>(fs::file_size(pfad, ec));
    }
    return n >= gl::kKingMindestSektoren * gl::kSektorBytes && n % gl::kSektorBytes == 0;
}

uint64_t GidePlatte::stromBytes(GideLayout l) const {
    return l == GideLayout::Poppe ? uint64_t(gl::kPoppeSpuren) * gl::kPoppeHostsektoren * gl::kSektorBytes
                                  : uint64_t(1024) * gl::kKingSektoren * gl::kRecordBytes;
}

bool GidePlatte::lwVorhanden(GideLayout l, int lw) const {
    const uint64_t n = sektorenGesamt();
    if (l == GideLayout::Poppe)
        return lw >= 0 && lw < int(gl::kPoppeLaufwerke) &&
               gl::poppeLba(unsigned(lw), gl::kPoppeSpuren - 1, gl::kPoppeHostsektoren - 1) < n;
    if (l == GideLayout::Kingstener) return lw == 0 && n >= gl::kKingMindestSektoren;
    return false;
}

bool GidePlatte::kingViertelGleich(uint64_t lba) const {
    const uint8_t* s = img_.data() + lba * gl::kSektorBytes;
    return std::memcmp(s, s + 128, 128) == 0 && std::memcmp(s, s + 256, 128) == 0 && std::memcmp(s, s + 384, 128) == 0;
}

bool GidePlatte::laufwerkPlausibel(GideLayout l, int lw, int* belegt) const {
    if (!lwVorhanden(l, lw)) return false;
    std::vector<uint8_t> dir(1024 * 32);
    if (l == GideLayout::Poppe) {
        const unsigned st = lw == 0 ? gl::kPoppeSystemspuren : 0;
        for (unsigned t = 0; t < 2; ++t) {
            const uint64_t lba = gl::poppeLba(unsigned(lw), st + t, 0);
            std::memcpy(dir.data() + t * 16384, img_.data() + lba * gl::kSektorBytes, 16384);
        }
        return verzeichnisPlausibel(dir.data(), 1024, 1000, belegt);
    }
    // Kingstener: Satz r = letztes Viertel des Hostsektors r; im Verzeichnis (256 Saetze) muessen die Viertel gleich sein
    for (uint64_t r = 0; r < 256; ++r) {
        if (!kingViertelGleich(r)) return false;
        std::memcpy(dir.data() + r * 128, img_.data() + r * gl::kSektorBytes + gl::kKingNutzOffset, 128);
    }
    return verzeichnisPlausibel(dir.data(), 1024, 2046, belegt);
}

void GidePlatte::erkennen(GideLayout vorgabe) {
    GideErkennung e;
    e.ausdruecklich = vorgabe != GideLayout::Auto;
    formatiert_.assign(gl::kPoppeLaufwerke, 0);
    for (int lw = 0; lw < int(gl::kPoppeLaufwerke); ++lw)
        if (laufwerkPlausibel(GideLayout::Poppe, lw, nullptr)) e.poppe_laufwerke.push_back(lw);
    e.poppe_plausibel = !e.poppe_laufwerke.empty();
    e.king_plausibel = laufwerkPlausibel(GideLayout::Kingstener, 0, nullptr);
    e.mehrdeutig = e.poppe_plausibel && e.king_plausibel;

    GideLayout l = vorgabe;
    if (vorgabe == GideLayout::Auto) {
        if (e.mehrdeutig)
            e.text = "Layout nicht eindeutig: das Abbild passt zu Poppe (Laufwerk(e) " +
                     std::to_string(e.poppe_laufwerke.front()) + "...) UND zu Kingstener; es wird nichts geraten - "
                     "mit --gide-layout poppe|kingstener ausdruecklich waehlen (Schreiben nur dann).";
        else if (e.poppe_plausibel) { l = GideLayout::Poppe; e.sicher = true; }
        else if (e.king_plausibel) { l = GideLayout::Kingstener; e.sicher = true; }
        else
            e.text = "Kein CP/M-Verzeichnis im Layout Poppe (Laufwerke 0-7) oder Kingstener gefunden: die Platte ist "
                     "unformatiert oder gehoert zu einem anderen Treiber.  Mit --gide-layout poppe|kingstener "
                     "ausdruecklich waehlen und das Laufwerk formatieren.";
    } else {
        const bool eigenes = vorgabe == GideLayout::Poppe ? e.poppe_plausibel : e.king_plausibel;
        const bool fremdes = vorgabe == GideLayout::Poppe ? e.king_plausibel : e.poppe_plausibel;
        e.sicher = eigenes && !fremdes;
        if (e.mehrdeutig) e.text = "Hinweis: das Abbild passt zu beiden Layouts; gewaehlt ist " + std::string(gideLayoutName(l)) + ".";
        else if (!eigenes && fremdes)
            e.text = "Achtung: das Abbild sieht nach dem anderen Layout aus (" +
                     std::string(vorgabe == GideLayout::Poppe ? "kingstener" : "poppe") +
                     "); ein falscher Treiber vernichtet Daten.";
        else if (!eigenes) e.text = "Im gewaehlten Layout steht noch kein Verzeichnis (unformatiert).";
    }
    e.layout = l;
    layout_ = l;
    if (l == GideLayout::Poppe)
        for (int lw : e.poppe_laufwerke) formatiert_[size_t(lw)] = 1;
    else if (l == GideLayout::Kingstener && e.king_plausibel)
        formatiert_[0] = 1;
    erk_ = e;
}

bool GidePlatte::laden(const std::string& pfad, std::string& err) {
    if (!k1520::gzip::laden(pfad, img_, &art_, &err)) return false;
    if (img_.size() % gl::kSektorBytes != 0) { err = "Abbildgroesse ist kein Vielfaches von 512 Byte"; return false; }
    if (img_.size() < gl::kKingMindestSektoren * gl::kSektorBytes) {
        err = "Abbild zu klein fuer eine GIDE-Platte (mindestens " + std::to_string(gl::kKingMindestSektoren) + " Sektoren)";
        return false;
    }
    return true;
}

std::unique_ptr<GidePlatte> GidePlatte::open(const std::string& pfad, std::string& err, bool read_only,
                                             GideLayout layout) {
    std::unique_ptr<GidePlatte> p(new GidePlatte);
    p->pfad_ = pfad;
    if (!p->laden(pfad, err)) return nullptr;
    p->backup_ = p->art_ == k1520::gzip::Art::Gzip;
    p->lws_.resize(gl::kPoppeLaufwerke);
    p->erkennen(layout);
    if (p->layout_ == GideLayout::Auto) { err = p->erk_.text; return nullptr; }
    p->fremd_gesperrt_ = sperreLebt(pfad + ".lock");
    if (!read_only) {
        if (!p->sperreAnlegen(err)) return nullptr;
        p->read_only_ = false;
    }
    return p;
}

bool GidePlatte::setReadOnly(bool ro) {
    if (ro) {
        read_only_ = true;
        sperreLoesen();
        return true;
    }
    if (!read_only_) return true;
    std::string e;
    if (!sperreAnlegen(e)) { fremd_gesperrt_ = true; return fail(e); }
    fremd_gesperrt_ = false;
    read_only_ = false;
    return true;
}

bool GidePlatte::anlegen(const std::string& pfad, GideLayout layout, uint64_t sektoren, std::string& err) {
    if (layout == GideLayout::Auto) { err = "Zum Anlegen muss das Layout ausdruecklich gewaehlt werden (poppe|kingstener)"; return false; }
    const uint64_t mind = layout == GideLayout::Poppe ? gl::kPoppeMindestSektoren : 65536;
    if (sektoren == 0) sektoren = mind;
    if (sektoren < (layout == GideLayout::Poppe ? gl::kPoppeMindestSektoren : gl::kKingMindestSektoren)) {
        err = "Abbild zu klein fuer das Layout " + std::string(gideLayoutName(layout));
        return false;
    }
    std::error_code ec;
    if (fs::exists(pfad, ec)) { err = "Datei existiert bereits (wird nie ueberschrieben): " + pfad; return false; }
    const uint64_t bytes = sektoren * gl::kSektorBytes;
    if (k1520::gzip::artNachEndung(pfad) == k1520::gzip::Art::Gzip)
        return k1520::gzip::nullenSpeichern(pfad, bytes, k1520::gzip::STUFE_VORGABE, &err);
    {
        std::ofstream f(pfad, std::ios::binary);
        if (!f) { err = "Kann Datei nicht anlegen: " + pfad; return false; }
    }
    fs::resize_file(pfad, bytes, ec);   // sparse
    if (ec) { err = "Kann Datei nicht dehnen: " + ec.message(); return false; }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Laufwerke
// ---------------------------------------------------------------------------------------------------------------

int GidePlatte::laufwerkAnzahl() const {
    int n = 0;
    while (lwVorhanden(layout_, n)) ++n;
    return n;
}

void GidePlatte::formatFuer(GideLayout l, int lw, DiskFormat& fmt, FsProfile& prof) {
    TrackFormat t;
    t.head_first = 0;
    t.cyl_first = 0;
    t.first_sector_id = 1;
    prof = FsProfile();
    prof.type = FsType::Cpm;
    prof.dir_entries = 1024;
    prof.skew = 0;
    if (l == GideLayout::Poppe) {
        // 500 CP/M-Spuren zu 32 Hostsektoren; als 250 Zylinder x 2 Koepfe (Zylinder sind 8 Bit).
        t.cyl_last = 249; t.head_last = 1; t.secs_per_track = 32; t.bytes_per_sec = 512;
        prof.name = "gide_poppe_lw" + std::to_string(lw);
        prof.description = "GIDE-Platte nach Poppe, Laufwerk " + std::to_string(lw);
        prof.block_size = 8192;
        prof.reserved_blocks = 8;                   // AL0 = FFH
        if (lw == 0) { prof.data_cyl = 1; prof.data_head = 0; }   // OFF = 2 Systemspuren
    } else {
        // 1024 Spuren zu 64 Saetzen; als 128 Zylinder x 8 Koepfe (Zylinder sind 8 Bit, numCylinders() = cyl_last+1).
        t.cyl_last = 127; t.head_last = 7; t.secs_per_track = 64; t.bytes_per_sec = 128;
        prof.name = "gide_kingstener_lw0";
        prof.description = "GIDE-Platte nach Kingstener (gidn)";
        prof.block_size = 4096;
        prof.max_blocks = 2046;                     // DSM = 7FDH
    }
    fmt = DiskFormat();
    fmt.name = prof.name;
    fmt.tracks.push_back(t);
    prof.format = fmt.name;
}

void GidePlatte::stromLaden(GideLayout l, int lw, std::vector<uint8_t>& strom) const {
    strom.assign(size_t(stromBytes(l)), 0);
    const uint64_t n = sektorenGesamt();
    if (l == GideLayout::Poppe) {
        for (unsigned t = 0; t < gl::kPoppeSpuren; ++t)
            std::memcpy(strom.data() + size_t(t) * 16384,
                        img_.data() + gl::poppeLba(unsigned(lw), t, 0) * gl::kSektorBytes, 16384);
    } else {
        for (uint64_t r = 0; r < n && r < 65536; ++r)
            std::memcpy(strom.data() + r * 128, img_.data() + r * gl::kSektorBytes + gl::kKingNutzOffset, 128);
    }
}

GidePlatte::Laufwerk* GidePlatte::oeffne(int lw) {
    if (lw < 0 || lw >= int(lws_.size()) || !lwVorhanden(layout_, lw)) {
        fail("Laufwerk " + std::to_string(lw) + " gibt es in diesem Abbild nicht");
        return nullptr;
    }
    if (!lws_[size_t(lw)]) {
        auto l = std::make_unique<Laufwerk>();
        l->index = lw;
        stromLaden(layout_, lw, l->strom);
        l->orig = l->strom;
        formatFuer(layout_, lw, l->fmt, l->prof);
        l->raum.reset(new SectorSpace(l->strom, l->fmt));
        std::string e;
        l->fs = CpmFileSystem::mount(*l->raum, l->prof, e);
        if (!l->fs) { fail("Laufwerk " + std::to_string(lw) + ": " + e); return nullptr; }
        lws_[size_t(lw)] = std::move(l);
    }
    return lws_[size_t(lw)].get();
}

GideLaufwerkInfo GidePlatte::laufwerkInfo(int lw) const {
    GideLaufwerkInfo i;
    i.index = lw;
    i.name = "lw" + std::to_string(lw);
    i.systemspuren = (layout_ == GideLayout::Poppe && lw == 0) ? int(gl::kPoppeSystemspuren) : 0;
    i.formatiert = lw >= 0 && lw < int(formatiert_.size()) && formatiert_[size_t(lw)];
    i.gesperrt = read_only_ || fremd_gesperrt_ || !i.formatiert;
    if (i.formatiert) {
        auto* self = const_cast<GidePlatte*>(this);
        if (Laufwerk* l = self->oeffne(lw)) {
            const FsInfo f = l->fs->info();
            i.total = f.total_bytes; i.used = f.used_bytes; i.free = f.free_bytes; i.dateien = f.files;
        }
    }
    return i;
}

bool GidePlatte::schreibErlaubt(int lw) {
    if (read_only_) return fail("Das Plattenabbild ist schreibgeschuetzt");
    if (fremd_gesperrt_) return fail("Abbild gesperrt: ein laufender Emulator oder ein anderes Werkzeug benutzt es");
    if (layout_ == GideLayout::Auto) return fail("Layout unbekannt - nicht geschrieben");
    if (lw < 0 || lw >= int(formatiert_.size()) || !formatiert_[size_t(lw)])
        return fail("Laufwerk " + std::to_string(lw) + " ist nicht formatiert (Verzeichnis nicht erkannt) - nicht geschrieben");
    return true;
}

bool GidePlatte::zerlege(const std::string& pfad, int& lw, std::string& name) {
    lw = -1;
    name = pfad;
    if (pfad.size() >= 3 && pfad[0] == 'l' && pfad[1] == 'w' && std::isdigit(static_cast<unsigned char>(pfad[2]))) {
        size_t i = 2;
        int n = 0;
        while (i < pfad.size() && std::isdigit(static_cast<unsigned char>(pfad[i]))) n = n * 10 + (pfad[i++] - '0');
        if (i == pfad.size() || pfad[i] == '/') {
            lw = n;
            name = i < pfad.size() ? pfad.substr(i + 1) : std::string();
            return true;
        }
    }
    if (layout_ == GideLayout::Kingstener) lw = 0;
    else if (std_lw_ >= 0) lw = std_lw_;
    if (lw < 0) return fail("Laufwerk fehlt: Pfad als lw0/NAME.EXT angeben (oder --laufwerk): " + pfad);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Dateien
// ---------------------------------------------------------------------------------------------------------------

std::vector<FileEntry> GidePlatte::list() {
    std::vector<FileEntry> aus;
    for (int lw = 0; lw < laufwerkAnzahl(); ++lw) {
        if (!formatiert_[size_t(lw)]) continue;
        Laufwerk* l = oeffne(lw);
        if (!l) continue;
        for (FileEntry e : l->fs->list()) {
            e.volume = lw;
            e.name = "lw" + std::to_string(lw) + "/" + e.qualifiedName();   // Nutzerbereich als "3:NAME.EXT"
            aus.push_back(std::move(e));
        }
    }
    return aus;
}

bool GidePlatte::read(const std::string& pfad, std::vector<uint8_t>& out) {
    int lw; std::string name;
    if (!zerlege(pfad, lw, name)) return false;
    if (name.empty()) return fail("Dateiname fehlt: " + pfad);
    if (lw >= int(formatiert_.size()) || lw < 0 || !formatiert_[size_t(lw)]) return fail("Laufwerk " + std::to_string(lw) + " ist nicht formatiert");
    Laufwerk* l = oeffne(lw);
    if (!l) return false;
    if (!l->fs->read(name, out)) return fail(l->fs->lastError());
    return true;
}

bool GidePlatte::write(const std::string& pfad, const std::vector<uint8_t>& data, const WriteOptions& opt) {
    int lw; std::string name;
    if (!zerlege(pfad, lw, name)) return false;
    if (name.empty()) return fail("Dateiname fehlt: " + pfad);
    if (!schreibErlaubt(lw)) return false;
    Laufwerk* l = oeffne(lw);
    if (!l) return false;
    if (!l->fs->write(name, data, opt)) return fail(l->fs->lastError());
    return true;
}

bool GidePlatte::erase(const std::string& pfad) {
    int lw; std::string name;
    if (!zerlege(pfad, lw, name)) return false;
    if (name.empty()) return fail("Dateiname fehlt: " + pfad);
    if (!schreibErlaubt(lw)) return false;
    Laufwerk* l = oeffne(lw);
    if (!l) return false;
    if (!l->fs->erase(name)) return fail(l->fs->lastError());
    return true;
}

bool GidePlatte::extract(const std::string& pfad, const std::string& dest_dir, bool overwrite) {
    std::error_code ec;
    int lw = -1;
    std::string name;
    if (!pfad.empty() && !zerlege(pfad, lw, name)) return false;
    int n = 0;
    for (const FileEntry& e : list()) {
        const std::string pre = "lw" + std::to_string(e.volume) + "/";
        const std::string rest = e.name.substr(pre.size());                  // "NAME.EXT" oder "3:NAME.EXT"
        const std::string schlicht = e.user != 0 ? rest.substr(rest.find(':') + 1) : rest;   // Dateiname ohne Nutzer
        if (!pfad.empty()) {
            if (e.volume != lw) continue;
            if (!name.empty() && rest != name) continue;
        }
        std::vector<uint8_t> d;
        if (!read(e.name, d)) return false;
        fs::path ziel = fs::path(dest_dir) / ("lw" + std::to_string(e.volume));
        if (e.user != 0) ziel /= "u" + std::to_string(e.user);
        fs::create_directories(ziel, ec);
        ziel /= schlicht;
        if (fs::exists(ziel, ec) && !overwrite) return fail("Zieldatei existiert: " + ziel.string());
        std::ofstream f(ziel, std::ios::binary);
        if (!f) return fail("Kann nicht schreiben: " + ziel.string());
        f.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
        ++n;
    }
    if (n == 0 && !pfad.empty()) return fail("Nichts gefunden: " + pfad);
    return true;
}

bool GidePlatte::insert(const std::string& quelle, const std::string& ziel, bool overwrite) {
    std::ifstream f(quelle, std::ios::binary);
    if (!f) return fail("Kann nicht lesen: " + quelle);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    WriteOptions o;
    o.overwrite = overwrite;
    return write(ziel, d, o);
}

// ---------------------------------------------------------------------------------------------------------------
// Pruefung, Formatieren
// ---------------------------------------------------------------------------------------------------------------

FsCheckReport GidePlatte::check(FsCheckLevel level) {
    FsCheckReport rep;
    rep.level = level;
    FsFindings f(rep);
    rep.schritt("gide.schritt.layout", "Layout der Platte (Erkennung)");
    if (erk_.mehrdeutig)
        f.add("gide.layout.mehrdeutig", FsSeverity::Warnung, FsLayer::Erkennung, "Platte",
              "Das Abbild passt zu den Layouts Poppe UND Kingstener; ein falscher Treiber vernichtet Daten");
    rep.schrittEnde();
    for (int lw = 0; lw < laufwerkAnzahl(); ++lw) {
        if (!formatiert_[size_t(lw)]) continue;
        Laufwerk* l = oeffne(lw);
        if (!l) continue;
        FsCheckReport r = l->fs->check(level, true);
        for (FsFinding& x : r.findings) {
            x.volume = lw;
            x.object = "lw" + std::to_string(lw) + (x.object.empty() ? "" : "/" + x.object);
            x.cyl = x.head = x.sector_index = -1;     // Ort in der synthetischen Geometrie ist nichts wert
            x.repairs.clear();                        // Reparatur nicht angeboten (Gast-Treiber ist massgeblich)
        }
        rep.uebernimm(r);
    }
    if (layout_ == GideLayout::Kingstener) {
        rep.schritt("gide.schritt.viertel", "Hostsektoren: vier gleiche Viertel (Kingstener schreibt jeden Satz vierfach)");
        // Schnell: nur das Verzeichnis (256 Saetze); Voll: alle Saetze bis zum Ende des Datenbereichs.
        const uint64_t bis = level == FsCheckLevel::Voll ? gl::kKingMindestSektoren : 256;
        int ungleich = 0;
        uint64_t erster = 0;
        for (uint64_t r = 0; r < bis && r < sektorenGesamt(); ++r) {
            if (kingViertelGleich(r)) continue;
            if (!ungleich) erster = r;
            ++ungleich;
        }
        if (ungleich)
            f.add("gide.kingstener.viertel", FsSeverity::Warnung, FsLayer::Medium, "Hostsektor " + std::to_string(erster),
                  std::to_string(ungleich) + " Hostsektoren mit ungleichen Vierteln (ab Sektor " + std::to_string(erster) +
                  "); der Treiber liest das letzte Viertel - fremd beschrieben (anderer Treiber?)");
        rep.schrittEnde();
    }
    rep.sortieren();
    return rep;
}

bool GidePlatte::formatLaufwerk(int lw, bool erzwingen) {
    if (!erk_.ausdruecklich && !erk_.sicher) return fail("Formatieren nur bei ausdruecklich gewaehltem oder eindeutig erkanntem Layout");
    if (read_only_) return fail("Das Plattenabbild ist schreibgeschuetzt");
    if (fremd_gesperrt_) return fail("Abbild gesperrt: ein laufender Emulator oder ein anderes Werkzeug benutzt es");
    if (!lwVorhanden(layout_, lw)) return fail("Laufwerk " + std::to_string(lw) + " gibt es in diesem Abbild nicht");
    if (!erzwingen) {
        // Sieht die Platte nach dem ANDEREN Layout aus, ist der Bereich vermutlich dessen Nutzdaten/Systemspur -
        // auch wenn er gerade Nullen traegt (Poppe-Laufwerk 0 hat dort seine Systemspuren und Laufwerk 1..7 ihre Verzeichnisse).
        if (layout_ == GideLayout::Kingstener ? erk_.poppe_plausibel : erk_.king_plausibel)
            return fail("Die Platte ist nach dem anderen Layout formatiert (" +
                        std::string(layout_ == GideLayout::Kingstener ? "poppe" : "kingstener") +
                        ") - Formatieren im Layout " + gideLayoutName(layout_) + " vernichtet Daten (nur mit erzwingen)");
        // Verzeichnisbereich ansehen: nur Nullen oder ein LEERES Verzeichnis darf ueberschrieben werden.
        std::vector<uint8_t> dir(1024 * 32);
        if (layout_ == GideLayout::Poppe) {
            const unsigned st = lw == 0 ? gl::kPoppeSystemspuren : 0;
            for (unsigned t = 0; t < 2; ++t)
                std::memcpy(dir.data() + t * 16384, img_.data() + gl::poppeLba(unsigned(lw), st + t, 0) * gl::kSektorBytes, 16384);
        } else {
            for (uint64_t r = 0; r < 256; ++r)
                std::memcpy(dir.data() + r * 128, img_.data() + r * gl::kSektorBytes + gl::kKingNutzOffset, 128);
        }
        const bool nullen = std::all_of(dir.begin(), dir.end(), [](uint8_t b) { return b == 0; });
        int belegt = 0;
        const bool plaus = verzeichnisPlausibel(dir.data(), 1024, layout_ == GideLayout::Poppe ? 1000 : 2046, &belegt);
        if (!nullen && !(plaus && belegt == 0))
            return fail(plaus ? "Laufwerk " + std::to_string(lw) + " enthaelt Dateien - Formatieren loescht sie (nur mit erzwingen)"
                              : "Der Verzeichnisbereich von Laufwerk " + std::to_string(lw) +
                                    " enthaelt Daten, die kein leeres Verzeichnis dieses Layouts sind (anderer Treiber?) - nur mit erzwingen");
    }
    Laufwerk* l = oeffne(lw);
    if (!l) return false;
    if (!l->fs->mkfs()) return fail(l->fs->lastError());
    formatiert_[size_t(lw)] = 1;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Zurueckschreiben
// ---------------------------------------------------------------------------------------------------------------

bool GidePlatte::dirty() const {
    if (!schmutzig_.empty()) return true;
    for (const auto& l : lws_)
        if (l && l->raum->flatRevision() != l->gespeichert_rev) return true;
    return false;
}

bool GidePlatte::stromZurueck(Laufwerk& l) {
    if (l.raum->flatRevision() == l.gespeichert_rev) return true;
    const uint64_t n = sektorenGesamt();
    const int lw = l.index;
    if (layout_ == GideLayout::Poppe) {
        for (unsigned t = 0; t < gl::kPoppeSpuren; ++t)
            for (unsigned s = 0; s < gl::kPoppeHostsektoren; ++s) {
                const size_t o = (size_t(t) * gl::kPoppeHostsektoren + s) * gl::kSektorBytes;
                if (std::memcmp(l.strom.data() + o, l.orig.data() + o, gl::kSektorBytes) == 0) continue;
                const uint64_t lba = gl::poppeLba(unsigned(lw), t, s);
                if (lba >= n) return fail("Sektor ausserhalb des Abbilds");
                std::memcpy(img_.data() + lba * gl::kSektorBytes, l.strom.data() + o, gl::kSektorBytes);
                schmutzig_.push_back(lba);
            }
    } else {
        for (uint64_t r = 0; r < 65536 && r < n; ++r) {
            const size_t o = size_t(r) * gl::kRecordBytes;
            if (std::memcmp(l.strom.data() + o, l.orig.data() + o, gl::kRecordBytes) == 0) continue;
            // Der Treiber schreibt denselben 128-B-Satz vier Mal in den Hostsektor (OTIR aus demselben Puffer).
            uint8_t* s = img_.data() + r * gl::kSektorBytes;
            for (int q = 0; q < 4; ++q) std::memcpy(s + q * 128, l.strom.data() + o, 128);
            schmutzig_.push_back(r);
        }
    }
    l.orig = l.strom;
    l.gespeichert_rev = l.raum->flatRevision();
    return true;
}

bool GidePlatte::flush() {
    if (!dirty()) return true;
    if (read_only_) return fail("Das Plattenabbild ist schreibgeschuetzt");
    if (fremd_gesperrt_) return fail("Abbild gesperrt: ein laufender Emulator oder ein anderes Werkzeug benutzt es");
    for (auto& l : lws_)
        if (l && !stromZurueck(*l)) return false;
    std::sort(schmutzig_.begin(), schmutzig_.end());
    schmutzig_.erase(std::unique(schmutzig_.begin(), schmutzig_.end()), schmutzig_.end());

    std::error_code ec;
    if (backup_ && !backup_getan_ && fs::exists(pfad_, ec)) {
        fs::copy_file(pfad_, pfad_ + "~", fs::copy_options::overwrite_existing, ec);
        if (ec) return fail("Sicherungskopie " + pfad_ + "~ nicht anlegbar: " + ec.message());
        backup_getan_ = true;
    }
    if (art_ == k1520::gzip::Art::Gzip) {
        if (!k1520::gzip::speichern(pfad_, img_.data(), img_.size(), art_, k1520::gzip::STUFE_VORGABE, &err_)) return false;
    } else {
        // Roh: nur die geaenderten Sektoren, an Ort und Stelle (die Datei kann sparse sein und gross).
        std::fstream f(pfad_, std::ios::in | std::ios::out | std::ios::binary);
        if (!f) return fail("Kann Abbild nicht zum Schreiben oeffnen: " + pfad_);
        for (uint64_t lba : schmutzig_) {
            f.seekp(static_cast<std::streamoff>(lba * gl::kSektorBytes));
            f.write(reinterpret_cast<const char*>(img_.data() + lba * gl::kSektorBytes), gl::kSektorBytes);
        }
        f.flush();
        if (!f) return fail("Schreibfehler im Abbild: " + pfad_);
    }
    schmutzig_.clear();
    return true;
}
