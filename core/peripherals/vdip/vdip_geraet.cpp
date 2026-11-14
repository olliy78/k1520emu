/**
 * @file vdip_geraet.cpp
 * @brief VDIP1/VDAP-Firmware ueber einem Wirtsverzeichnis (siehe vdip_geraet.h, Entwurf 50 Abschnitt 5a).
 */
#include "core/peripherals/vdip/vdip_geraet.h"

#include <algorithm>
#include <chrono>
#include <system_error>

namespace fs = std::filesystem;

namespace k1520 {

namespace {

constexpr uint8_t CR = 0x0D;

bool istBuchstabe(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

// Tage seit 1970-01-01 <-> Datum (proleptisch gregorianisch, nach H. Hinnant)
int64_t tageAusDatum(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

void datumAusTagen(int64_t z, int64_t& y, unsigned& m, unsigned& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += m <= 2;
}

// Gibt es unter diesem Pfad IRGENDETWAS (auch ein Symlink, auch ein toter)?  Nie darauf anlegen/ueberschreiben.
bool belegt(const fs::path& p) {
    std::error_code ec;
    const auto st = fs::symlink_status(p, ec);
    if (st.type() == fs::file_type::not_found) return false;   // (ec ist dann ggf. gesetzt)
    return true;                                               // alles andere, auch bei Fehler: belegt
}

bool nurDigits(const std::string& s) {
    if (s.empty() || s.size() > 10) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    return true;
}

}  // namespace

const char* vdipFehlerText(VdipFehler f, bool kurz) {
    switch (f) {
        case VdipFehler::BadCommand:      return kurz ? "BC" : "Bad Command";
        case VdipFehler::CommandFailed:   return kurz ? "CF" : "Command Failed";
        case VdipFehler::DiskFull:        return kurz ? "DF" : "Disk Full";
        case VdipFehler::Invalid:         return kurz ? "FI" : "Invalid";
        case VdipFehler::ReadOnly:        return kurz ? "RO" : "Read Only";
        case VdipFehler::FileOpen:        return kurz ? "FO" : "File Open";
        case VdipFehler::DirNotEmpty:     return kurz ? "NE" : "Dir Not Empty";
        case VdipFehler::FilenameInvalid: return kurz ? "FN" : "Filename Invalid";
        case VdipFehler::NoDisk:          return kurz ? "ND" : "No Disk";
        case VdipFehler::Ok:              break;
    }
    return "";
}

// ======================================================================= VdipStick

bool VdipStick::einstecken(const fs::path& ordner, bool schreibgeschuetzt, std::string* fehler) {
    ziehen();
    std::error_code ec;
    if (ordner.empty()) {
        if (fehler) *fehler = "kein Ordner angegeben";
        return false;
    }
    fs::path w = fs::canonical(ordner, ec);
    if (ec || !fs::is_directory(w, ec)) {
        if (fehler) *fehler = "Ordner nicht vorhanden: " + ordner.string();
        return false;
    }
    wurzel_ = w;
    cwd_.clear();
    ro_ = schreibgeschuetzt;
    vorhanden_ = true;
    return true;
}

void VdipStick::ziehen() {
    schliesseDatei();
    vorhanden_ = false;
    wurzel_.clear();
    cwd_.clear();
    ro_ = false;
}

bool VdipStick::dateiname83(const std::string& roh, std::string& norm) {
    norm.clear();
    if (roh.empty() || roh.size() > 12) return false;
    const size_t punkt = roh.find('.');
    std::string base = roh.substr(0, punkt);
    std::string ext;
    if (punkt != std::string::npos) {
        ext = roh.substr(punkt + 1);
        if (ext.find('.') != std::string::npos) return false;
    }
    if (base.empty() || base.size() > 8 || ext.size() > 3) return false;
    static const std::string erlaubt = "!#$%&'()-@^_`{}~";
    auto gross = [&](std::string& s) {
        for (char& c : s) {
            const uint8_t u = static_cast<uint8_t>(c);
            if (u >= 'a' && u <= 'z') c = static_cast<char>(u - 32);
            else if (!((u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') ||
                       erlaubt.find(c) != std::string::npos))
                return false;
        }
        return true;
    };
    if (!gross(base) || !gross(ext)) return false;
    // Geraetenamen (unter Windows wuerde "CON.TXT" das Geraet oeffnen, nicht eine Datei)
    static const char* const gesperrt[] = {"CON", "PRN", "AUX", "NUL"};
    for (const char* g : gesperrt) if (base == g) return false;
    if (base.size() == 4 && (base.compare(0, 3, "COM") == 0 || base.compare(0, 3, "LPT") == 0) &&
        base[3] >= '0' && base[3] <= '9')
        return false;
    norm = ext.empty() ? base : base + "." + ext;
    return true;
}

uint32_t VdipStick::zeitZuDos(fs::file_time_type t) {
    using namespace std::chrono;
    const auto nf = fs::file_time_type::clock::now();
    const auto ns = system_clock::now();
    const auto sys = ns + duration_cast<system_clock::duration>(t - nf);
    // auf ganze Sekunden RUNDEN: der Abgleich der beiden Uhren ist nicht auf die Mikrosekunde genau
    const int64_t ms = duration_cast<milliseconds>(sys.time_since_epoch()).count();
    int64_t s = (ms + 500) / 1000;
    if (ms < 0) s = 0;
    int64_t tage = s / 86400;
    const int64_t rest = s % 86400;
    int64_t y; unsigned m, d;
    datumAusTagen(tage, y, m, d);
    unsigned h = static_cast<unsigned>(rest / 3600), mi = static_cast<unsigned>(rest % 3600 / 60),
             sek = static_cast<unsigned>(rest % 60);
    if (y < 1980) { y = 1980; m = 1; d = 1; h = mi = sek = 0; }
    if (y > 2107) { y = 2107; m = 12; d = 31; h = 23; mi = 58; sek = 0; }
    const uint32_t datum = static_cast<uint32_t>(((y - 1980) << 9) | (m << 5) | d);
    const uint32_t zeit = (h << 11) | (mi << 5) | (sek / 2);
    return (datum << 16) | zeit;
}

bool VdipStick::dosZuZeit(uint32_t dos, fs::file_time_type& t) {
    using namespace std::chrono;
    const unsigned datum = dos >> 16, zeit = dos & 0xFFFF;
    const int64_t y = 1980 + static_cast<int64_t>(datum >> 9);
    const unsigned m = (datum >> 5) & 15, d = datum & 31;
    const unsigned h = zeit >> 11, mi = (zeit >> 5) & 63, s2 = zeit & 31;
    if (m < 1 || m > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s2 > 29) return false;
    const int64_t s = tageAusDatum(y, m, d) * 86400 + h * 3600 + mi * 60 + s2 * 2;
    const auto nf = fs::file_time_type::clock::now();
    const auto ns = system_clock::now();
    t = nf + duration_cast<fs::file_time_type::duration>(system_clock::time_point(seconds(s)) - ns);
    return true;
}

bool VdipStick::cwdPfad(fs::path& p) {
    std::error_code ec;
    if (!vorhanden_) return false;
    p = wurzel_;
    auto st = fs::symlink_status(p, ec);
    if (ec || st.type() != fs::file_type::directory) { cwd_.clear(); return false; }
    for (const auto& c : cwd_) {
        p /= c;
        st = fs::symlink_status(p, ec);
        if (ec || st.type() != fs::file_type::directory) { cwd_.clear(); p = wurzel_; return false; }
    }
    return true;
}

bool VdipStick::finde(const fs::path& dir, const std::string& norm, fs::path& pfad, bool& istVerz,
                      std::string* hostName) {
    std::error_code ec;
    fs::directory_iterator it(dir, ec), ende;
    if (ec) return false;
    bool gefunden = false;
    std::string beste;
    for (; !ec && it != ende; it.increment(ec)) {
        std::error_code e2;
        const auto st = it->symlink_status(e2);
        if (e2) continue;
        if (st.type() != fs::file_type::regular && st.type() != fs::file_type::directory) continue;
        const std::string n = it->path().filename().string();
        std::string nn;
        if (!dateiname83(n, nn) || nn != norm) continue;
        // bei Mehrdeutigkeit (a.txt + A.TXT) deterministisch die kleinste Schreibweise
        if (!gefunden || n < beste) {
            gefunden = true;
            beste = n;
            pfad = it->path();
            istVerz = st.type() == fs::file_type::directory;
        }
    }
    if (gefunden && hostName) *hostName = beste;
    return gefunden;
}

VdipFehler VdipStick::liste(std::vector<Eintrag>& aus) {
    aus.clear();
    if (!vorhanden_) return VdipFehler::NoDisk;
    fs::path dir;
    if (!cwdPfad(dir)) return VdipFehler::CommandFailed;
    std::error_code ec;
    fs::directory_iterator it(dir, ec), ende;
    if (ec) return VdipFehler::CommandFailed;
    for (; !ec && it != ende; it.increment(ec)) {
        std::error_code e2;
        const auto st = it->symlink_status(e2);
        if (e2) continue;
        if (st.type() != fs::file_type::regular && st.type() != fs::file_type::directory) continue;
        Eintrag e;
        if (!dateiname83(it->path().filename().string(), e.name)) continue;
        e.verzeichnis = st.type() == fs::file_type::directory;
        if (!e.verzeichnis) {
            const auto sz = fs::file_size(it->path(), e2);
            e.groesse = e2 ? 0 : sz;
        }
        const auto t = fs::last_write_time(it->path(), e2);
        e.dosZeit = e2 ? 0x00210000u : zeitZuDos(t);
        aus.push_back(e);
    }
    std::sort(aus.begin(), aus.end(), [](const Eintrag& a, const Eintrag& b) { return a.name < b.name; });
    // doppelte Schreibweisen (a.txt + A.TXT) nur einmal zeigen
    aus.erase(std::unique(aus.begin(), aus.end(),
                          [](const Eintrag& a, const Eintrag& b) { return a.name == b.name; }),
              aus.end());
    return VdipFehler::Ok;
}

VdipFehler VdipStick::eintrag(const std::string& name, Eintrag& aus) {
    if (!vorhanden_) return VdipFehler::NoDisk;
    std::string norm;
    if (!dateiname83(name, norm)) return VdipFehler::FilenameInvalid;
    fs::path dir, p;
    bool verz = false;
    if (!cwdPfad(dir) || !finde(dir, norm, p, verz)) return VdipFehler::CommandFailed;
    std::error_code ec;
    aus = Eintrag{};
    aus.name = norm;
    aus.verzeichnis = verz;
    if (!verz) {
        const auto sz = fs::file_size(p, ec);
        aus.groesse = ec ? 0 : sz;
    }
    const auto t = fs::last_write_time(p, ec);
    aus.dosZeit = ec ? 0x00210000u : zeitZuDos(t);
    return VdipFehler::Ok;
}

VdipFehler VdipStick::cd(const std::string& arg) {
    if (!vorhanden_) return VdipFehler::NoDisk;
    if (arg == "/") { cwd_.clear(); return VdipFehler::Ok; }
    if (arg == "..") {
        if (cwd_.empty()) return VdipFehler::CommandFailed;   // nie ueber die Wurzel hinaus
        cwd_.pop_back();
        return VdipFehler::Ok;
    }
    std::string norm;
    if (!dateiname83(arg, norm)) return VdipFehler::FilenameInvalid;
    fs::path dir, p;
    bool verz = false;
    std::string host;
    if (!cwdPfad(dir) || !finde(dir, norm, p, verz, &host) || !verz) return VdipFehler::CommandFailed;
    cwd_.push_back(host);
    return VdipFehler::Ok;
}

std::string VdipStick::verzeichnisAnzeige() const {
    std::string s = "\\";
    for (size_t i = 0; i < cwd_.size(); ++i) {
        if (i) s += "\\";
        s += cwd_[i];
    }
    return s;
}

VdipFehler VdipStick::mkd(const std::string& name, bool stempel, uint32_t dos) {
    if (!vorhanden_) return VdipFehler::NoDisk;
    if (ro_) return VdipFehler::ReadOnly;
    std::string norm;
    if (!dateiname83(name, norm)) return VdipFehler::FilenameInvalid;
    fs::path dir, p;
    bool verz = false;
    if (!cwdPfad(dir)) return VdipFehler::CommandFailed;
    if (finde(dir, norm, p, verz)) return VdipFehler::CommandFailed;
    if (belegt(dir / norm)) return VdipFehler::CommandFailed;
    std::error_code ec;
    fs::create_directory(dir / norm, ec);
    if (ec) return VdipFehler::CommandFailed;
    fs::file_time_type t;
    if (stempel && dosZuZeit(dos, t)) fs::last_write_time(dir / norm, t, ec);
    return VdipFehler::Ok;
}

VdipFehler VdipStick::rmd(const std::string& name) {
    if (!vorhanden_) return VdipFehler::NoDisk;
    if (ro_) return VdipFehler::ReadOnly;
    std::string norm;
    if (!dateiname83(name, norm)) return VdipFehler::FilenameInvalid;
    fs::path dir, p;
    bool verz = false;
    if (!cwdPfad(dir) || !finde(dir, norm, p, verz) || !verz) return VdipFehler::CommandFailed;
    std::error_code ec;
    fs::directory_iterator it(p, ec);
    if (ec) return VdipFehler::CommandFailed;
    if (it != fs::directory_iterator()) return VdipFehler::DirNotEmpty;
    fs::remove(p, ec);   // nur ein leeres Verzeichnis, kein remove_all
    return ec ? VdipFehler::CommandFailed : VdipFehler::Ok;
}

VdipFehler VdipStick::loesche(const std::string& name) {
    if (!vorhanden_) return VdipFehler::NoDisk;
    if (ro_) return VdipFehler::ReadOnly;
    std::string norm;
    if (!dateiname83(name, norm)) return VdipFehler::FilenameInvalid;
    fs::path dir, p;
    bool verz = false;
    if (!cwdPfad(dir) || !finde(dir, norm, p, verz) || verz) return VdipFehler::CommandFailed;
    if (norm == offenName_) return VdipFehler::FileOpen;
    std::error_code ec;
    const auto perm = fs::status(p, ec).permissions();
    if (!ec && (perm & fs::perms::owner_write) == fs::perms::none) return VdipFehler::ReadOnly;
    fs::remove(p, ec);
    return ec ? VdipFehler::CommandFailed : VdipFehler::Ok;
}

VdipFehler VdipStick::benenne(const std::string& alt, const std::string& neu) {
    if (!vorhanden_) return VdipFehler::NoDisk;
    if (ro_) return VdipFehler::ReadOnly;
    std::string na, nn;
    if (!dateiname83(alt, na) || !dateiname83(neu, nn)) return VdipFehler::FilenameInvalid;
    fs::path dir, p, q;
    bool verz = false, verz2 = false;
    if (!cwdPfad(dir) || !finde(dir, na, p, verz)) return VdipFehler::CommandFailed;
    if (na == nn) return VdipFehler::Ok;
    if (finde(dir, nn, q, verz2) || belegt(dir / nn)) return VdipFehler::CommandFailed;
    if (!verz && na == offenName_) return VdipFehler::FileOpen;
    std::error_code ec;
    fs::rename(p, dir / nn, ec);
    return ec ? VdipFehler::CommandFailed : VdipFehler::Ok;
}

uint64_t VdipStick::freierPlatz() {
    if (!vorhanden_) return 0;
    std::error_code ec;
    const auto sp = fs::space(wurzel_, ec);
    return ec ? 0 : static_cast<uint64_t>(sp.available);
}

VdipFehler VdipStick::oeffneLesen(const std::string& name) {
    if (!vorhanden_) return VdipFehler::NoDisk;
    if (dateiOffen()) return VdipFehler::FileOpen;
    std::string norm;
    if (!dateiname83(name, norm)) return VdipFehler::FilenameInvalid;
    fs::path dir, p;
    bool verz = false;
    if (!cwdPfad(dir) || !finde(dir, norm, p, verz) || verz) return VdipFehler::CommandFailed;
    datei_.clear();
    datei_.open(p, std::ios::in | std::ios::binary);
    if (!datei_.is_open()) return VdipFehler::CommandFailed;
    offenName_ = norm;
    offenPfad_ = p;
    offenSchreiben_ = false;
    offenStempel_ = false;
    return VdipFehler::Ok;
}

VdipFehler VdipStick::oeffneSchreiben(const std::string& name, bool stempel, uint32_t dos) {
    if (!vorhanden_) return VdipFehler::NoDisk;
    if (ro_) return VdipFehler::ReadOnly;
    if (dateiOffen()) return VdipFehler::FileOpen;
    std::string norm;
    if (!dateiname83(name, norm)) return VdipFehler::FilenameInvalid;
    fs::path dir, p;
    bool verz = false;
    if (!cwdPfad(dir)) return VdipFehler::CommandFailed;
    datei_.clear();
    if (finde(dir, norm, p, verz)) {
        if (verz) return VdipFehler::CommandFailed;
        std::error_code ec;
        const auto perm = fs::status(p, ec).permissions();
        if (!ec && (perm & fs::perms::owner_write) == fs::perms::none) return VdipFehler::ReadOnly;
        datei_.open(p, std::ios::in | std::ios::out | std::ios::binary);
        if (datei_.is_open()) datei_.seekp(0, std::ios::end);   // vorhandene Datei: anhaengen [?]
    } else {
        p = dir / norm;
        if (belegt(p)) return VdipFehler::CommandFailed;   // z. B. ein Symlink dieses Namens
        datei_.open(p, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    }
    if (!datei_.is_open()) return VdipFehler::CommandFailed;
    offenName_ = norm;
    offenPfad_ = p;
    offenSchreiben_ = true;
    offenStempel_ = stempel;
    offenDos_ = dos;
    return VdipFehler::Ok;
}

VdipFehler VdipStick::lies(uint64_t n, std::string& aus) {
    aus.clear();
    if (!zumLesenOffen()) return VdipFehler::CommandFailed;
    datei_.clear();
    const auto cur = datei_.tellg();
    datei_.seekg(0, std::ios::end);
    const auto end = datei_.tellg();
    datei_.seekg(cur);
    if (cur < 0 || end < 0) return VdipFehler::CommandFailed;
    uint64_t rest = static_cast<uint64_t>(end - cur);
    if (n > rest) n = rest;
    std::string puffer(65536, '\0');
    while (n > 0) {
        const size_t teil = static_cast<size_t>(std::min<uint64_t>(n, puffer.size()));
        datei_.read(&puffer[0], static_cast<std::streamsize>(teil));
        const auto got = static_cast<size_t>(datei_.gcount());
        aus.append(puffer, 0, got);
        n -= got;
        if (got < teil) break;
    }
    datei_.clear();
    return VdipFehler::Ok;
}

VdipFehler VdipStick::schreibe(const char* p, size_t n) {
    if (!zumSchreibenOffen()) return VdipFehler::CommandFailed;
    datei_.write(p, static_cast<std::streamsize>(n));
    if (!datei_) {
        datei_.clear();
        return VdipFehler::DiskFull;
    }
    return VdipFehler::Ok;
}

VdipFehler VdipStick::suche(uint64_t pos) {
    if (!dateiOffen()) return VdipFehler::CommandFailed;
    datei_.clear();
    const auto cur = datei_.tellg();
    datei_.seekg(0, std::ios::end);
    const auto end = datei_.tellg();
    if (end < 0 || pos > static_cast<uint64_t>(end)) {
        datei_.seekg(cur);
        return VdipFehler::CommandFailed;
    }
    datei_.seekg(static_cast<std::streamoff>(pos));
    datei_.seekp(static_cast<std::streamoff>(pos));
    return VdipFehler::Ok;
}

VdipFehler VdipStick::schliesseDatei(const std::string& name) {
    std::string norm;
    if (!dateiOffen() || !dateiname83(name, norm) || norm != offenName_) return VdipFehler::CommandFailed;
    schliesseDatei();
    return VdipFehler::Ok;
}

void VdipStick::schliesseDatei() {
    if (!dateiOffen()) return;
    datei_.close();
    datei_.clear();
    if (offenSchreiben_ && offenStempel_) {
        fs::file_time_type t;
        std::error_code ec;
        if (dosZuZeit(offenDos_, t)) fs::last_write_time(offenPfad_, t, ec);
    }
    offenName_.clear();
    offenPfad_.clear();
    offenSchreiben_ = false;
    offenStempel_ = false;
}

// ===================================================================== VdipGeraet

VdipGeraet::VdipGeraet(const Config& c) {
    if (!c.ordner.empty()) stick_.einstecken(c.ordner, c.schreibgeschuetzt);
    reset();
}

bool VdipGeraet::stickEinstecken(const Config& c, std::string* fehler) {
    stick_.ziehen();
    resetZeile();
    if (!stick_.einstecken(c.ordner, c.schreibgeschuetzt, fehler)) return false;
    text("Device Detected P2");
    zeilenEnde();
    status();
    return true;
}

void VdipGeraet::stickZiehen() {
    const bool war = stick_.vorhanden();
    stick_.ziehen();
    resetZeile();
    if (war) {
        text("Device Removed P2");
        zeilenEnde();
        status();
    }
}

void VdipGeraet::reset() {
    aus_.clear();
    scs_ = false;
    iph_ = false;
    stick_.schliesseDatei();
    resetZeile();
    text("Ver03.68VDAPF On-Line:");
    zeilenEnde();
    if (stick_.vorhanden()) {
        text("Device Detected P2");
        zeilenEnde();
    }
    text("No Upgrade");
    zeilenEnde();
    status();
}

void VdipGeraet::resetZeile() {
    phase_ = Phase::Befehl;
    cmd_ = Cmd::Keiner;
    tok_.clear();
    arg_.clear();
    hatStempel_ = false;
    stempelBytes_.clear();
    zahlBytes_.clear();
    zahl_ = 0;
    wrfRest_ = 0;
    wrfPuffer_.clear();
    wrfFehler_ = VdipFehler::Ok;
}

uint8_t VdipGeraet::empfangen() {
    if (aus_.empty()) return 0;
    const uint8_t b = aus_.front();
    aus_.pop_front();
    return b;
}

void VdipGeraet::text(const std::string& s) { aus_.insert(aus_.end(), s.begin(), s.end()); }
void VdipGeraet::roh(const std::string& s) { aus_.insert(aus_.end(), s.begin(), s.end()); }

void VdipGeraet::status() {
    if (stick_.vorhanden()) text(scs_ ? ">" : "D:\\>");
    else text(scs_ ? "ND" : "No Disk");
    zeilenEnde();
}

void VdipGeraet::fehler(VdipFehler f) {
    text(vdipFehlerText(f, scs_));
    zeilenEnde();
}

void VdipGeraet::zahl32(uint32_t v) {
    if (iph_) {
        for (int i = 0; i < 4; ++i) aus_.push_back(static_cast<uint8_t>(v >> (8 * i)));   // LSB zuerst
    } else {
        text(std::to_string(v));
    }
}

VdipGeraet::Cmd VdipGeraet::mnemonic(const std::string& s) {
    static const struct { const char* n; Cmd c; } tab[] = {
        {"E", Cmd::Echo}, {"e", Cmd::EchoKlein}, {"SCS", Cmd::Scs}, {"ECS", Cmd::Ecs}, {"IPA", Cmd::Ipa},
        {"IPH", Cmd::Iph}, {"DIR", Cmd::Dir}, {"DIRT", Cmd::Dirt}, {"CD", Cmd::Cd}, {"RD", Cmd::Rd},
        {"DLD", Cmd::Dld}, {"MKD", Cmd::Mkd}, {"DLF", Cmd::Dlf}, {"WRF", Cmd::Wrf}, {"OPW", Cmd::Opw},
        {"CLF", Cmd::Clf}, {"RDF", Cmd::Rdf}, {"REN", Cmd::Ren}, {"OPR", Cmd::Opr}, {"SEK", Cmd::Sek},
        {"FS", Cmd::Fs}, {"IDD", Cmd::Idd}, {"FWV", Cmd::Fwv}};
    for (const auto& t : tab) if (s == t.n) return t.c;
    return Cmd::Unbekannt;
}

VdipGeraet::Cmd VdipGeraet::opcode(uint8_t b) {
    // gesichert durch die UTools-Quellen: 01 07 08 09 0A 0B 0E 10 11 2F 91; die uebrigen [?] aus der Erinnerung
    switch (b) {
        case 0x01: return Cmd::Dir;
        case 0x02: return Cmd::Cd;
        case 0x04: return Cmd::Rd;
        case 0x05: return Cmd::Dld;
        case 0x06: return Cmd::Mkd;
        case 0x07: return Cmd::Dlf;
        case 0x08: return Cmd::Wrf;
        case 0x09: return Cmd::Opw;
        case 0x0A: return Cmd::Clf;
        case 0x0B: return Cmd::Rdf;
        case 0x0C: return Cmd::Ren;
        case 0x0E: return Cmd::Opr;
        case 0x0F: return Cmd::Idd;
        case 0x10: return Cmd::Scs;
        case 0x11: return Cmd::Ecs;
        case 0x12: return Cmd::Fs;
        case 0x13: return Cmd::Fwv;
        case 0x28: return Cmd::Sek;
        case 0x2F: return Cmd::Dirt;
        case 0x90: return Cmd::Ipa;
        case 0x91: return Cmd::Iph;
        default:   return Cmd::Unbekannt;
    }
}

void VdipGeraet::senden(uint8_t b) {
    switch (phase_) {
    case Phase::Daten:
        datenByte(b);
        return;
    case Phase::Befehl:
        if (tok_.empty()) {
            if (b == CR) { cmd_ = Cmd::Keiner; zeileEnde(); return; }
            if (istBuchstabe(b)) { tok_.push_back(static_cast<char>(b)); return; }
            cmd_ = opcode(b);               // Einzelbyte-Befehl
            phase_ = Phase::Trenner;
            return;
        }
        if (istBuchstabe(b)) {
            if (tok_.size() < 8) tok_.push_back(static_cast<char>(b));
            return;
        }
        cmd_ = mnemonic(tok_);
        tok_.clear();
        befehlEnde(b);
        return;
    case Phase::Trenner:
        befehlEnde(b);
        return;
    case Phase::Name:
        if (b == CR) { zeileEnde(); return; }
        if (b == ' ' && (cmd_ == Cmd::Mkd || cmd_ == Cmd::Opw) && !arg_.empty()) {
            phase_ = Phase::Stempel;       // danach genau 4 rohe Datumsbytes
            stempelBytes_.clear();
            return;
        }
        if (arg_.size() < 64) arg_.push_back(static_cast<char>(b));
        return;
    case Phase::Stempel:
        stempelBytes_.push_back(b);
        if (stempelBytes_.size() == 4) {
            hatStempel_ = true;
            phase_ = Phase::Rest;
        }
        return;
    case Phase::Zahl:
        if (iph_) {
            zahlBytes_.push_back(b);        // genau 4 Bytes, MSB zuerst; ein CR darunter ist Datum
            if (zahlBytes_.size() == 4) {
                zahl_ = (uint32_t(zahlBytes_[0]) << 24) | (uint32_t(zahlBytes_[1]) << 16) |
                        (uint32_t(zahlBytes_[2]) << 8) | zahlBytes_[3];
                phase_ = Phase::Rest;
            }
        } else {
            if (b == CR) { zeileEnde(); return; }
            if (arg_.size() < 64) arg_.push_back(static_cast<char>(b));
        }
        return;
    case Phase::Rest:
        if (b == CR) zeileEnde();
        return;
    }
}

void VdipGeraet::befehlEnde(uint8_t sep) {
    if (sep == CR) { zeileEnde(); return; }
    if (sep != ' ') {               // Muell hinter dem Befehl: Zeile verwerfen, Bad Command
        cmd_ = Cmd::Unbekannt;
        phase_ = Phase::Rest;
        return;
    }
    switch (cmd_) {
        case Cmd::Dir: case Cmd::Dirt: case Cmd::Cd: case Cmd::Rd: case Cmd::Dld: case Cmd::Mkd:
        case Cmd::Dlf: case Cmd::Opw: case Cmd::Clf: case Cmd::Ren: case Cmd::Opr:
            phase_ = Phase::Name;
            break;
        case Cmd::Rdf: case Cmd::Wrf: case Cmd::Sek:
            phase_ = Phase::Zahl;
            break;
        default:
            phase_ = Phase::Rest;
            break;
    }
}

void VdipGeraet::zeileEnde() {
    ausfuehren();
    if (phase_ != Phase::Daten) resetZeile();
}

void VdipGeraet::ausfuehren() {
    // Zahlenparameter (ASCII-Form) fertig auswerten
    auto zahlParam = [&](uint32_t& v) -> bool {
        if (iph_) {
            if (zahlBytes_.size() != 4) return false;
            v = zahl_;
            return true;
        }
        if (!nurDigits(arg_)) return false;
        const unsigned long long x = std::stoull(arg_);
        if (x > 0xFFFFFFFFull) return false;
        v = static_cast<uint32_t>(x);
        return true;
    };
    auto stempel = [&]() -> uint32_t {
        if (stempelBytes_.size() != 4) return 0;
        return (uint32_t(stempelBytes_[0]) << 24) | (uint32_t(stempelBytes_[1]) << 16) |
               (uint32_t(stempelBytes_[2]) << 8) | stempelBytes_[3];
    };

    const Cmd c = cmd_;
    // Befehle ohne Stick: Modus, Echo, Firmwareversion
    switch (c) {
        case Cmd::Keiner: status(); return;
        case Cmd::Echo: text("E"); zeilenEnde(); return;
        case Cmd::EchoKlein: text("e"); zeilenEnde(); return;
        case Cmd::Scs: scs_ = true; status(); return;
        case Cmd::Ecs: scs_ = false; status(); return;
        case Cmd::Ipa: iph_ = false; status(); return;
        case Cmd::Iph: iph_ = true; status(); return;
        case Cmd::Fwv: text("MAIN 03.68VDAPF"); zeilenEnde(); status(); return;
        case Cmd::Unbekannt: fehler(VdipFehler::BadCommand); return;
        default: break;
    }
    if (c == Cmd::Wrf) {
        // Auch bei Fehler die Daten abnehmen (sonst waeren sie ein Befehl); Ergebnis nach den Daten.
        uint32_t n = 0;
        wrfFehler_ = VdipFehler::Ok;
        if (!stick_.vorhanden()) wrfFehler_ = VdipFehler::NoDisk;
        else if (!zahlParam(n)) { wrfFehler_ = VdipFehler::Invalid; n = 0; }
        else if (!stick_.zumSchreibenOffen()) wrfFehler_ = VdipFehler::CommandFailed;
        wrfRest_ = n;
        wrfPuffer_.clear();
        if (n == 0) { ok(wrfFehler_); wrfFehler_ = VdipFehler::Ok; return; }
        phase_ = Phase::Daten;
        return;
    }
    if (!stick_.vorhanden()) {
        if (c == Cmd::Dir || c == Cmd::Dirt) zeilenEnde();   // wie bei vorhandenem Stick: erst CR
        fehler(VdipFehler::NoDisk);
        return;
    }
    switch (c) {
        case Cmd::Dir: {
            zeilenEnde();
            if (!arg_.empty()) {
                VdipStick::Eintrag e;
                VdipFehler f = stick_.eintrag(arg_, e);
                if (f == VdipFehler::Ok && e.verzeichnis) f = VdipFehler::CommandFailed;
                if (f != VdipFehler::Ok) { fehler(f); return; }
                text(e.name + " ");
                zahl32(static_cast<uint32_t>(std::min<uint64_t>(e.groesse, 0xFFFFFFFFull)));
                zeilenEnde();
                status();
                return;
            }
            std::vector<VdipStick::Eintrag> l;
            const VdipFehler f = stick_.liste(l);
            if (f != VdipFehler::Ok) { fehler(f); return; }
            if (!stick_.imWurzelverzeichnis()) {
                text(". DIR"); zeilenEnde();
                text(".. DIR"); zeilenEnde();
            }
            for (const auto& e : l) {
                text(e.verzeichnis ? e.name + " DIR" : e.name);
                zeilenEnde();
            }
            status();
            return;
        }
        case Cmd::Dirt: {
            zeilenEnde();
            VdipStick::Eintrag e;
            const VdipFehler f = stick_.eintrag(arg_, e);
            if (f != VdipFehler::Ok) { fehler(f); return; }
            text(e.name + " ");
            // erstellt (Dword LE) · Zugriffsdatum (Wort) · geaendert (Dword LE); Wirtsdatei kennt nur eine Zeit
            auto le32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) aus_.push_back(uint8_t(v >> (8 * i))); };
            le32(e.dosZeit);
            aus_.push_back(uint8_t(e.dosZeit >> 16));
            aus_.push_back(uint8_t(e.dosZeit >> 24));
            le32(e.dosZeit);
            zeilenEnde();
            status();
            return;
        }
        case Cmd::Cd:
            if (arg_.empty()) { fehler(VdipFehler::CommandFailed); return; }
            ok(stick_.cd(arg_));
            return;
        case Cmd::Rd: {
            VdipFehler f = stick_.oeffneLesen(arg_);
            if (f == VdipFehler::Ok) {
                std::string s;
                f = stick_.lies(~0ull, s);
                stick_.schliesseDatei();
                if (f == VdipFehler::Ok) roh(s);
            }
            ok(f);
            return;
        }
        case Cmd::Dld: ok(stick_.rmd(arg_)); return;
        case Cmd::Mkd: ok(stick_.mkd(arg_, hatStempel_, stempel())); return;
        case Cmd::Dlf: ok(stick_.loesche(arg_)); return;
        case Cmd::Ren: {
            const size_t sp = arg_.find(' ');
            if (sp == std::string::npos) { fehler(VdipFehler::Invalid); return; }
            ok(stick_.benenne(arg_.substr(0, sp), arg_.substr(sp + 1)));
            return;
        }
        case Cmd::Opr: ok(stick_.oeffneLesen(arg_)); return;
        case Cmd::Opw: ok(stick_.oeffneSchreiben(arg_, hatStempel_, stempel())); return;
        case Cmd::Clf: ok(stick_.schliesseDatei(arg_)); return;
        case Cmd::Rdf: {
            uint32_t n = 0;
            if (!zahlParam(n)) { fehler(VdipFehler::Invalid); return; }
            std::string s;
            const VdipFehler f = stick_.lies(n, s);
            if (f != VdipFehler::Ok) { fehler(f); return; }
            roh(s);
            status();
            return;
        }
        case Cmd::Sek: {
            uint32_t n = 0;
            if (!zahlParam(n)) { fehler(VdipFehler::Invalid); return; }
            ok(stick_.suche(n));
            return;
        }
        case Cmd::Fs:
            zahl32(static_cast<uint32_t>(std::min<uint64_t>(stick_.freierPlatz(), 0xFFFFFFFFull)));
            zeilenEnde();
            status();
            return;
        case Cmd::Idd:
            text("K1520EMU HOST DIRECTORY");
            zeilenEnde();
            text(stick_.schreibgeschuetzt() ? "READ ONLY" : "READ WRITE");
            zeilenEnde();
            status();
            return;
        default:
            fehler(VdipFehler::BadCommand);
            return;
    }
}

void VdipGeraet::datenByte(uint8_t b) {
    if (wrfFehler_ == VdipFehler::Ok) {
        wrfPuffer_.push_back(static_cast<char>(b));
        if (wrfPuffer_.size() >= 4096 && wrfRest_ > 1) {
            wrfFehler_ = stick_.schreibe(wrfPuffer_.data(), wrfPuffer_.size());
            wrfPuffer_.clear();
        }
    }
    if (--wrfRest_ == 0) wrfFertig();
}

void VdipGeraet::wrfFertig() {
    if (wrfFehler_ == VdipFehler::Ok && !wrfPuffer_.empty())
        wrfFehler_ = stick_.schreibe(wrfPuffer_.data(), wrfPuffer_.size());
    const VdipFehler f = wrfFehler_;
    resetZeile();
    ok(f);
}

}  // namespace k1520
