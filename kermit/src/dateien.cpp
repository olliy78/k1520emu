/**
 * @file dateien.cpp
 * @brief Ordnerfreigabe, Dateiquellen/-senken, Namensumsetzung, ls -l
 *        (siehe dateien.h).
 */
#include "kermit/src/dateien.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>

namespace fs = std::filesystem;

namespace k1520::kermit {

namespace {

const char* const MONATE[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// std::localtime/mktime sind nicht fadenfest; eine Sperre genuegt hier.
std::mutex& zeitSperre() {
    static std::mutex m;
    return m;
}

bool ortszeit(std::time_t t, std::tm& aus) {
    std::lock_guard<std::mutex> lk(zeitSperre());
    const std::tm* p = std::localtime(&t);
    if (!p) return false;
    aus = *p;
    return true;
}

std::time_t ausOrtszeit(std::tm tm) {
    std::lock_guard<std::mutex> lk(zeitSperre());
    tm.tm_isdst = -1;
    return std::mktime(&tm);
}

std::time_t zuTimeT(fs::file_time_type ft) {
    using namespace std::chrono;
    const auto sys = time_point_cast<system_clock::duration>(ft - fs::file_time_type::clock::now() + system_clock::now());
    return system_clock::to_time_t(sys);
}

fs::file_time_type zuFileTime(std::time_t t) {
    using namespace std::chrono;
    const auto sys = system_clock::from_time_t(t);
    return time_point_cast<fs::file_time_type::duration>(sys - system_clock::now() + fs::file_time_type::clock::now());
}

std::string kermitDatum(std::time_t t) {
    std::tm tm{};
    if (!ortszeit(t, tm)) return {};
    char b[32];
    std::snprintf(b, sizeof b, "%04d%02d%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return b;
}

/// "[yy]yymmdd[ hh:mm[:ss]]" → time_t (-1 bei Formfehler).
std::time_t ausKermitDatum(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    if (i != 6 && i != 8) return -1;
    std::tm tm{};
    const std::string d = s.substr(0, i);
    int jahr = std::stoi(d.substr(0, i - 4));
    if (i == 6) jahr += (jahr < 70) ? 2000 : 1900;
    tm.tm_year = jahr - 1900;
    tm.tm_mon = std::stoi(d.substr(i - 4, 2)) - 1;
    tm.tm_mday = std::stoi(d.substr(i - 2, 2));
    int h = 0, m = 0, sek = 0;
    if (i < s.size()) std::sscanf(s.c_str() + i, " %d:%d:%d", &h, &m, &sek);
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = sek;
    return ausOrtszeit(tm);
}

bool hatMuster(const std::string& s) { return s.find_first_of("*?") != std::string::npos; }

bool passt(const char* m, const char* s) {
    if (*m == 0) return *s == 0;
    if (*m == '*') return passt(m + 1, s) || (*s && passt(m, s + 1));
    if (*s && (*m == '?' || *m == *s)) return passt(m + 1, s + 1);
    return false;
}

bool reserviert(const std::string& teil) {
    std::string b = teil.substr(0, teil.find('.'));
    for (auto& c : b) c = static_cast<char>((c >= 'a' && c <= 'z') ? c - 32 : c);
    if (b == "CON" || b == "PRN" || b == "AUX" || b == "NUL") return true;
    if (b.size() == 4 && (b.compare(0, 3, "COM") == 0 || b.compare(0, 3, "LPT") == 0) && b[3] >= '1' && b[3] <= '9')
        return true;
    return false;
}

std::vector<std::string> teile(const std::string& s) {
    std::vector<std::string> v;
    std::size_t a = 0;
    while (a <= s.size()) {
        const std::size_t e = s.find('/', a);
        const std::size_t b = (e == std::string::npos) ? s.size() : e;
        v.push_back(s.substr(a, b - a));
        if (e == std::string::npos) break;
        a = e + 1;
    }
    return v;
}

bool innerhalb(const fs::path& wurzel, const fs::path& p) {
    auto w = wurzel.begin(), q = p.begin();
    for (; w != wurzel.end(); ++w, ++q) {
        if (q == p.end() || *w != *q) {
            // abschliessendes leeres Element ("dir/") der Wurzel zaehlt nicht
            if (w->empty()) continue;
            return false;
        }
    }
    return true;
}

std::string utf8(const fs::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::string rechteText(const fs::file_status& st, char art) {
    std::string r(10, '-');
    r[0] = art;
    const auto pr = st.permissions();
    const fs::perms bits[9] = {fs::perms::owner_read, fs::perms::owner_write, fs::perms::owner_exec,
                               fs::perms::group_read, fs::perms::group_write, fs::perms::group_exec,
                               fs::perms::others_read, fs::perms::others_write, fs::perms::others_exec};
    const char z[3] = {'r', 'w', 'x'};
    for (int i = 0; i < 9; ++i)
        if ((pr & bits[i]) != fs::perms::none) r[static_cast<std::size_t>(i + 1)] = z[i % 3];
    return r;
}

// ─── Quellen/Senken ────────────────────────────────────────────────────────

class DateiQuelleImpl : public DateiQuelle {
public:
    DateiQuelleImpl(const fs::path& p, int64_t g, std::string d) : f_(p, std::ios::binary), g_(g), d_(std::move(d)) {}
    bool offen() const { return f_.is_open(); }
    long lesen(uint8_t* buf, std::size_t n) override {
        if (!f_) return f_.eof() ? 0 : -1;
        f_.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(n));
        const auto k = f_.gcount();
        if (k == 0 && f_.bad()) return -1;
        return static_cast<long>(k);
    }
    int64_t groesse() const override { return g_; }
    std::string datum() const override { return d_; }

private:
    std::ifstream f_;
    int64_t g_;
    std::string d_;
};

class TextQuelle : public DateiQuelle {
public:
    explicit TextQuelle(std::string t) : t_(std::move(t)) {}
    long lesen(uint8_t* buf, std::size_t n) override {
        const std::size_t k = std::min(n, t_.size() - pos_);
        std::copy(t_.begin() + static_cast<std::ptrdiff_t>(pos_), t_.begin() + static_cast<std::ptrdiff_t>(pos_ + k), buf);
        pos_ += k;
        return static_cast<long>(k);
    }
    int64_t groesse() const override { return static_cast<int64_t>(t_.size()); }
    std::string datum() const override { return {}; }

private:
    std::string t_;
    std::size_t pos_ = 0;
};

}  // namespace

class FreigabeSenke : public DateiSenke {
public:
    FreigabeSenke(Freigabe& fg, fs::path p) : fg_(fg), p_(std::move(p)), f_(p_, std::ios::binary | std::ios::trunc) {}
    bool offen() const { return f_.is_open(); }
    bool schreiben(const uint8_t* buf, std::size_t n) override {
        if (fg_.platz_ >= 0) {
            if (static_cast<int64_t>(n) > fg_.platz_) {
                f_.write(reinterpret_cast<const char*>(buf), static_cast<std::streamsize>(fg_.platz_));
                fg_.platz_ = 0;
                return false;   // Datentraeger voll
            }
            fg_.platz_ -= static_cast<int64_t>(n);
        }
        f_.write(reinterpret_cast<const char*>(buf), static_cast<std::streamsize>(n));
        return static_cast<bool>(f_);
    }
    bool schliessen(const std::string& datum) override {
        f_.close();
        if (f_.fail()) return false;
        if (!datum.empty()) {
            const std::time_t t = ausKermitDatum(datum);
            std::error_code ec;
            if (t != static_cast<std::time_t>(-1)) fs::last_write_time(p_, zuFileTime(t), ec);
        }
        return true;
    }
    void verwerfen() override {
        f_.close();
        std::error_code ec;
        fs::remove(p_, ec);
    }

private:
    Freigabe& fg_;
    fs::path p_;
    std::ofstream f_;
};

// ─── Namen ─────────────────────────────────────────────────────────────────

std::string nameFuerEmpfang(const std::string& name, Namen modus) {
    if (modus == Namen::Literal) return name;
    std::string s = name;
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    return s;
}

std::string nameFuerSenden(const std::string& name, Namen modus) {
    if (modus == Namen::Literal) return name;
    // C-Kermit zltor: Pfad weg, Grossschrift, '~' → X, jeder weitere Punkt → X,
    // fuehrender Punkt bekommt ein X davor.
    std::string w = name;
    const std::size_t sl = w.find_last_of('/');
    if (sl != std::string::npos) w = w.substr(sl + 1);
    int punkte = 0;
    for (auto& c : w) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
        else if (c == '~') c = 'X';
        else if (c == '.' && ++punkte > 1) c = 'X';
    }
    if (!w.empty() && w[0] == '.') w = "X" + w;
    return w;
}

bool istTextEndung(const std::string& name) {
    const std::size_t p = name.find_last_of('.');
    if (p == std::string::npos) return false;
    std::string e = name.substr(p + 1);
    for (auto& c : e)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    return e == "txt" || e == "c" || e == "h" || e == "mac" || e == "asm" || e == "doc";
}

std::unique_ptr<DateiQuelle> oeffneQuelle(const fs::path& p, std::string& fehler) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) {
        fehler = "Datei nicht gefunden: " + utf8(p);
        return nullptr;
    }
    const auto g = static_cast<int64_t>(fs::file_size(p, ec));
    std::string d;
    const auto ft = fs::last_write_time(p, ec);
    if (!ec) d = kermitDatum(zuTimeT(ft));
    auto q = std::make_unique<DateiQuelleImpl>(p, ec ? -1 : g, d);
    if (!q->offen()) {
        fehler = "Datei nicht lesbar: " + utf8(p);
        return nullptr;
    }
    return q;
}

std::unique_ptr<DateiQuelle> textQuelle(const std::string& text) { return std::make_unique<TextQuelle>(text); }

// ─── Freigabe ──────────────────────────────────────────────────────────────

Freigabe::Freigabe(const fs::path& wurzel, bool nurLesen) : nurLesen_(nurLesen) {
    std::error_code ec;
    wurzel_ = fs::weakly_canonical(fs::absolute(wurzel, ec), ec);
    gueltig_ = !ec && fs::is_directory(wurzel_, ec);
}

namespace {
/// Streng gueltiges UTF-8 (keine Kurzformen, keine Ersatzzeichen, bis U+10FFFF).
bool gueltigesUtf8(const std::string& s) {
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<uint8_t>(s[i]);
        if (c < 0x80) { ++i; continue; }
        std::size_t n = 0;
        uint32_t cp = 0;
        if (c >= 0xC2 && c <= 0xDF) { n = 2; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { n = 3; cp = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { n = 4; cp = c & 0x07; }
        else return false;
        if (i + n > s.size()) return false;
        for (std::size_t k = 1; k < n; ++k) {
            const auto f = static_cast<uint8_t>(s[i + k]);
            if ((f & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (f & 0x3F);
        }
        if ((n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        i += n;
    }
    return true;
}
}  // namespace

bool Freigabe::normalisiere(const std::string& entfernt, std::string& relativ, std::string& fehler) const {
    for (char c : entfernt) {
        const auto u = static_cast<uint8_t>(c);
        if (u < 0x20 || u == 0x7F) {
            fehler = "Steuerzeichen im Namen";
            return false;
        }
    }
    // Namen sind UTF-8 (Umlaute vom PC kommen so an).  Alles andere ist ein beschaedigter Name — etwa
    // der Rest eines abgebrochenen Sendens im Puffer des C-Kermit — und wuerde unter Windows in
    // fs::u8path() scheitern bzw. unter Linux eine Datei mit unlesbarem Namen anlegen.
    if (!gueltigesUtf8(entfernt)) {
        fehler = "Ungueltige Zeichen im Namen (kein UTF-8)";
        return false;
    }
    if (entfernt.find('\\') != std::string::npos) {
        fehler = "Rueckstrich im Namen: " + entfernt;
        return false;
    }
    if (entfernt.size() >= 2 && entfernt[1] == ':' &&
        ((entfernt[0] >= 'A' && entfernt[0] <= 'Z') || (entfernt[0] >= 'a' && entfernt[0] <= 'z'))) {
        fehler = "Laufwerksbuchstabe im Namen: " + entfernt;
        return false;
    }
    if (!entfernt.empty() && entfernt[0] == '/') {
        fehler = "Absoluter Pfad nicht erlaubt: " + entfernt;
        return false;
    }
    std::vector<std::string> st;
    if (!aktuell_.empty()) st = teile(aktuell_);
    for (const auto& t : teile(entfernt)) {
        if (t.empty() || t == ".") continue;
        if (t == "..") {
            if (st.empty()) {
                fehler = "Pfad fuehrt aus der Freigabe: " + entfernt;
                return false;
            }
            st.pop_back();
            continue;
        }
        if (reserviert(t)) {
            fehler = "Reservierter Name: " + t;
            return false;
        }
#ifdef _WIN32
        if (t.back() == '.' || t.back() == ' ') {
            fehler = "Name endet auf Punkt oder Leerzeichen: " + t;
            return false;
        }
#endif
        st.push_back(t);
    }
    relativ.clear();
    for (std::size_t i = 0; i < st.size(); ++i) relativ += (i ? "/" : "") + st[i];
    return true;
}

bool Freigabe::aufloesen(const std::string& entfernt, fs::path& lokal, std::string& fehler) const {
    if (!gueltig_) {
        fehler = "Freigabe ungueltig";
        return false;
    }
    std::string rel;
    if (!normalisiere(entfernt, rel, fehler)) return false;
    fs::path p = wurzel_;
    if (!rel.empty())
        for (const auto& t : teile(rel)) p /= fs::u8path(t);
    std::error_code ec;
    fs::path kan = fs::weakly_canonical(p, ec);
    bool ok = !ec && innerhalb(wurzel_, kan);
    // Ein Verweis mit FEHLENDEM Ziel loest weakly_canonical nicht auf — sonst
    // legte das Schreiben die Datei ausserhalb der Freigabe an.
    fs::path q = p;
    for (int tiefe = 0; ok && tiefe < 16 && fs::is_symlink(fs::symlink_status(q, ec)); ++tiefe) {
        fs::path z = fs::read_symlink(q, ec);
        if (ec) { ok = false; break; }
        if (z.is_relative()) z = q.parent_path() / z;
        kan = fs::weakly_canonical(z, ec);
        ok = !ec && innerhalb(wurzel_, kan);
        q = z;
    }
    if (!ok) {
        fehler = "Pfad fuehrt aus der Freigabe (Verweis): " + entfernt;
        return false;
    }
    lokal = p;
    return true;
}

std::string Freigabe::anzeigeName(const fs::path& p) const {
    std::error_code ec;
    const auto r = fs::relative(p, wurzel_, ec);
    if (ec) return utf8(p.filename());
    const auto u = r.generic_u8string();
    const std::string s(u.begin(), u.end());
    return s.empty() ? std::string(".") : s;
}

bool Freigabe::wechseln(const std::string& pfad, std::string& fehler) {
    if (pfad.empty()) {
        aktuell_.clear();
        return true;
    }
    std::string rel;
    fs::path p;
    if (!normalisiere(pfad, rel, fehler) || !aufloesen(pfad, p, fehler)) return false;
    std::error_code ec;
    if (!fs::is_directory(p, ec)) {
        fehler = "Kein Verzeichnis: " + pfad;
        return false;
    }
    aktuell_ = rel;
    return true;
}

std::vector<fs::path> Freigabe::finde(const std::string& muster, std::string& fehler) const {
    std::vector<fs::path> v;
    const std::size_t sl = muster.find_last_of('/');
    const std::string letzter = (sl == std::string::npos) ? muster : muster.substr(sl + 1);
    std::error_code ec;
    if (!hatMuster(letzter)) {
        fs::path p;
        if (!aufloesen(muster, p, fehler)) return v;
        if (!fs::is_regular_file(p, ec)) {
            fehler = "Datei nicht gefunden: " + muster;
            return v;
        }
        v.push_back(p);
        return v;
    }
    const std::string dir = (sl == std::string::npos) ? std::string() : muster.substr(0, sl + 1);
    fs::path d;
    if (!aufloesen(dir.empty() ? "." : dir, d, fehler)) return v;
    for (const auto& e : fs::directory_iterator(d, ec)) {
        const std::string n = utf8(e.path().filename());
        if (n.empty() || (n[0] == '.' && letzter[0] != '.')) continue;
        if (!passt(letzter.c_str(), n.c_str())) continue;
        fs::path p;
        std::string f;
        if (!aufloesen(dir + n, p, f)) continue;   // ausbrechender Verweis
        if (fs::is_regular_file(p, ec)) v.push_back(p);
    }
    std::sort(v.begin(), v.end());
    if (v.empty()) fehler = "Datei nicht gefunden: " + muster;
    return v;
}

static VerzeichnisEintrag eintragFuer(const fs::path& p, const std::string& name) {
    VerzeichnisEintrag e;
    std::error_code ec;
    const auto sst = fs::symlink_status(p, ec);
    e.art = fs::is_symlink(sst) ? 'l' : fs::is_directory(sst) ? 'd' : '-';
    e.rechte = rechteText(fs::is_symlink(sst) ? fs::status(p, ec) : sst, e.art);
    if (e.art == 'l') e.rechte = "lrwxrwxrwx";
    e.links = (e.art == 'd') ? 2 : 1;
    e.besitzer = "pc";
    e.gruppe = "pc";
    e.groesse = 0;
    if (e.art == '-') e.groesse = static_cast<int64_t>(fs::file_size(p, ec));
    else if (e.art == 'd') e.groesse = 512;
    else e.groesse = static_cast<int64_t>(utf8(fs::read_symlink(p, ec)).size());
    if (ec) e.groesse = 0;
    std::error_code ec2;
    const auto ft = fs::last_write_time(p, ec2);
    std::tm tm{};
    if (!ec2 && ortszeit(zuTimeT(ft), tm)) {
        e.jahr = tm.tm_year + 1900;
        e.monat = tm.tm_mon + 1;
        e.tag = tm.tm_mday;
        e.stunde = tm.tm_hour;
        e.minute = tm.tm_min;
    }
    e.name = name;
    if (e.art == 'l') e.ziel = utf8(fs::read_symlink(p, ec2));
    e.gueltig = true;
    return e;
}

bool Freigabe::liste(const std::string& pfad, std::string& text, std::string& fehler) const {
    std::vector<std::pair<std::string, fs::path>> eintraege;
    const std::size_t sl = pfad.find_last_of('/');
    const std::string letzter = (sl == std::string::npos) ? pfad : pfad.substr(sl + 1);
    std::error_code ec;
    bool verzeichnis = false;
    if (hatMuster(letzter)) {
        const std::string dir = (sl == std::string::npos) ? std::string() : pfad.substr(0, sl + 1);
        fs::path d;
        if (!aufloesen(dir.empty() ? "." : dir, d, fehler)) return false;
        for (const auto& e : fs::directory_iterator(d, ec)) {
            const std::string n = utf8(e.path().filename());
            if (n[0] == '.' && letzter[0] != '.') continue;
            if (passt(letzter.c_str(), n.c_str())) eintraege.push_back({n, e.path()});
        }
        if (eintraege.empty()) {
            fehler = "Nicht gefunden: " + pfad;
            return false;
        }
    } else {
        fs::path p;
        if (!aufloesen(pfad.empty() ? "." : pfad, p, fehler)) return false;
        if (fs::is_directory(p, ec)) {
            verzeichnis = true;
            for (const auto& e : fs::directory_iterator(p, ec)) {
                const std::string n = utf8(e.path().filename());
                if (n.empty() || n[0] == '.') continue;   // wie ls: keine Punktdateien
                eintraege.push_back({n, e.path()});
            }
        } else if (fs::exists(fs::symlink_status(p, ec))) {
            eintraege.push_back({letzter, p});
        } else {
            fehler = "Nicht gefunden: " + pfad;
            return false;
        }
    }
    std::sort(eintraege.begin(), eintraege.end());
    std::string zeilen;
    int64_t bloecke = 0;
    std::tm jetzt{};
    ortszeit(std::time(nullptr), jetzt);
    const int jetztMon = (jetzt.tm_year + 1900) * 12 + jetzt.tm_mon;
    for (const auto& [n, p] : eintraege) {
        const VerzeichnisEintrag e = eintragFuer(p, n);
        bloecke += (e.groesse + 511) / 512;
        const int mon = e.jahr * 12 + (e.monat - 1);
        zeilen += lsZeile(e, e.jahr > 0 && jetztMon - mon < 6 && jetztMon - mon >= 0) + "\n";
    }
    text.clear();
    if (verzeichnis) text = "total " + std::to_string(bloecke) + "\n";
    text += zeilen;
    return true;
}

bool Freigabe::loeschen(const std::string& muster, std::string& meldung) {
    if (nurLesen_) {
        meldung = "Freigabe ist schreibgeschuetzt";
        return false;
    }
    std::string f;
    const auto v = finde(muster, f);
    if (v.empty()) {
        meldung = f;
        return false;
    }
    std::string ok;
    for (const auto& p : v) {
        std::error_code ec;
        if (!fs::remove(p, ec) || ec) {
            meldung = "Kann nicht loeschen: " + anzeigeName(p);
            return false;
        }
        ok += (ok.empty() ? "" : " ") + anzeigeName(p);
    }
    meldung = "Geloescht: " + ok;
    return true;
}

std::unique_ptr<DateiSenke> Freigabe::anlegen(const std::string& name, bool ueberschreiben, std::string& gespeichert,
                                              std::string& fehler) {
    if (nurLesen_) {
        fehler = "Freigabe ist schreibgeschuetzt";
        return nullptr;
    }
    const std::size_t sl = name.find_last_of('/');
    const std::string letzter = (sl == std::string::npos) ? name : name.substr(sl + 1);
    if (letzter.empty()) {
        fehler = "Leerer Dateiname";
        return nullptr;
    }
    if (maxName_ > 0 && static_cast<int>(letzter.size()) > maxName_) {
        fehler = "Dateiname laenger als " + std::to_string(maxName_) + " Zeichen: " + letzter;
        return nullptr;
    }
    fs::path p;
    if (!aufloesen(name, p, fehler)) return nullptr;
    std::error_code ec;
    if (!fs::is_directory(p.parent_path(), ec)) {
        fehler = "Verzeichnis fehlt: " + name;
        return nullptr;
    }
    if (fs::is_directory(p, ec)) {
        fehler = "Ist ein Verzeichnis: " + name;
        return nullptr;
    }
    if (!ueberschreiben && fs::exists(p, ec)) {
        fehler = "Datei existiert bereits: " + name;
        return nullptr;
    }
    auto s = std::make_unique<FreigabeSenke>(*this, p);
    if (!s->offen()) {
        fehler = "Kann Datei nicht anlegen: " + name;
        return nullptr;
    }
    gespeichert = anzeigeName(p);
    return s;
}

// ─── ls -l ─────────────────────────────────────────────────────────────────

std::string lsZeile(const VerzeichnisEintrag& e, bool mitUhrzeit) {
    char datum[32];
    const char* mon = (e.monat >= 1 && e.monat <= 12) ? MONATE[e.monat - 1] : "Jan";
    if (mitUhrzeit) std::snprintf(datum, sizeof datum, "%s %2d %02d:%02d", mon, e.tag, e.stunde, e.minute);
    else std::snprintf(datum, sizeof datum, "%s %2d  %4d", mon, e.tag, e.jahr);
    char b[512];
    std::snprintf(b, sizeof b, "%-10s%2d %-8s %-8s%7lld %s ", e.rechte.c_str(), e.links, e.besitzer.c_str(),
                  e.gruppe.c_str(), static_cast<long long>(e.groesse), datum);
    std::string z = b;
    z += e.name;
    if (e.art == 'l' && !e.ziel.empty()) z += " -> " + e.ziel;
    return z;
}

namespace {
struct Tok {
    std::size_t a, e;
};
bool ziffern(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}
int monatVon(const std::string& s) {
    for (int i = 0; i < 12; ++i)
        if (s == MONATE[i]) return i + 1;
    return 0;
}
bool istZeit(const std::string& s, int& h, int& m) {
    if (s.size() < 4 || s.size() > 5) return false;
    const std::size_t p = s.find(':');
    if (p == std::string::npos || !ziffern(s.substr(0, p)) || !ziffern(s.substr(p + 1)) || s.size() - p - 1 != 2)
        return false;
    h = std::stoi(s.substr(0, p));
    m = std::stoi(s.substr(p + 1));
    return h < 24 && m < 60;
}
bool istIsoDatum(const std::string& s, int& j, int& mo, int& t) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    if (!ziffern(s.substr(0, 4)) || !ziffern(s.substr(5, 2)) || !ziffern(s.substr(8, 2))) return false;
    j = std::stoi(s.substr(0, 4));
    mo = std::stoi(s.substr(5, 2));
    t = std::stoi(s.substr(8, 2));
    return mo >= 1 && mo <= 12 && t >= 1 && t <= 31;
}
bool istModus(const std::string& s) {
    if (s.size() < 10) return false;
    if (std::string("-dlcbpsDn").find(s[0]) == std::string::npos) return false;
    for (std::size_t i = 1; i < 10; ++i)
        if (std::string("rwxsStTlL-").find(s[i]) == std::string::npos) return false;
    return true;
}
}  // namespace

VerzeichnisEintrag zerlegeLs(const std::string& zeileRoh) {
    VerzeichnisEintrag e;
    std::string zeile = zeileRoh;
    while (!zeile.empty() && (zeile.back() == '\r' || zeile.back() == '\n')) zeile.pop_back();
    std::vector<Tok> t;
    for (std::size_t i = 0; i < zeile.size();) {
        while (i < zeile.size() && (zeile[i] == ' ' || zeile[i] == '\t')) ++i;
        if (i >= zeile.size()) break;
        const std::size_t a = i;
        while (i < zeile.size() && zeile[i] != ' ' && zeile[i] != '\t') ++i;
        t.push_back({a, i});
    }
    auto tok = [&](std::size_t k) { return zeile.substr(t[k].a, t[k].e - t[k].a); };
    std::size_t k = 0;
    while (k < t.size() && ziffern(tok(k))) ++k;   // -i / -s-Vorspalten
    if (k >= t.size() || !istModus(tok(k))) return e;
    const std::string modus = tok(k);
    e.rechte = modus.substr(0, 10);
    e.art = modus[0];
    std::string rest = modus.substr(10);
    while (!rest.empty() && (rest[0] == '.' || rest[0] == '+' || rest[0] == '@')) rest.erase(0, 1);
    bool linksDa = false;
    if (ziffern(rest)) {   // System III: "drwxrwxrwx12" (Linkzahl angeklebt)
        e.links = std::stoi(rest);
        linksDa = true;
    } else if (!rest.empty()) {
        return e;
    }
    ++k;
    if (!linksDa) {
        if (k >= t.size() || !ziffern(tok(k))) return e;
        e.links = std::stoi(tok(k));
        ++k;
    }
    const std::size_t ab = k;
    // Datum suchen: "Mon dd HH:MM" / "Mon dd YYYY" / "YYYY-MM-DD HH:MM[:SS[.n]] [+zzzz]"
    std::size_t d = 0, dl = 0;
    for (std::size_t j = ab + 1; j < t.size() && !dl; ++j) {
        int h = 0, m = 0, jj = 0, mo = 0, tg = 0;
        if (const int mon = monatVon(tok(j)); mon && j + 2 < t.size() && ziffern(tok(j + 1)) && tok(j + 1).size() <= 2) {
            const std::string w = tok(j + 2);
            if (istZeit(w, h, m)) {
                e.stunde = h;
                e.minute = m;
            } else if (ziffern(w) && w.size() == 4) {
                e.jahr = std::stoi(w);
            } else {
                continue;
            }
            e.monat = mon;
            e.tag = std::stoi(tok(j + 1));
            d = j;
            dl = 3;
        } else if (istIsoDatum(tok(j), jj, mo, tg)) {
            e.jahr = jj;
            e.monat = mo;
            e.tag = tg;
            d = j;
            dl = 1;
            if (j + 1 < t.size()) {
                std::string w = tok(j + 1);
                const std::string hm = w.substr(0, 5);
                if (istZeit(hm, h, m) && (w.size() == 5 || w[5] == ':')) {
                    e.stunde = h;
                    e.minute = m;
                    dl = 2;
                    if (j + 2 < t.size()) {
                        const std::string z = tok(j + 2);
                        if (z.size() == 5 && (z[0] == '+' || z[0] == '-') && ziffern(z.substr(1))) dl = 3;
                    }
                }
            }
        }
    }
    if (!dl || d < ab + 1) return e;
    std::size_t g = d - 1;   // Groesse bzw. Nebennummer
    std::size_t besEnde = g;
    const std::string gr = tok(g);
    if (ziffern(gr) && g >= ab + 1 && tok(g - 1).back() == ',') {   // Geraet "4,  0"
        e.groesse = -1;
        besEnde = g - 1;
    } else if (gr.find(',') != std::string::npos) {   // Geraet "4,0"
        e.groesse = -1;
    } else if (ziffern(gr)) {
        e.groesse = std::stoll(gr);
    } else {
        return e;
    }
    if (besEnde > ab) e.besitzer = tok(ab);
    if (besEnde > ab + 1) e.gruppe = tok(ab + 1);
    e.datum = zeile.substr(t[d].a, t[d + dl - 1].e - t[d].a);
    std::size_t na = t[d + dl - 1].e;
    if (na < zeile.size() && zeile[na] == ' ') ++na;
    e.name = zeile.substr(na);
    if (e.name.empty()) return e;
    if (e.art == 'l') {
        const std::size_t p = e.name.find(" -> ");
        if (p != std::string::npos) {
            e.ziel = e.name.substr(p + 4);
            e.name = e.name.substr(0, p);
        }
    }
    e.gueltig = true;
    return e;
}

std::vector<VerzeichnisEintrag> zerlegeLsListe(const std::string& text) {
    std::vector<VerzeichnisEintrag> v;
    std::size_t a = 0;
    while (a < text.size()) {
        std::size_t e = text.find('\n', a);
        if (e == std::string::npos) e = text.size();
        auto z = zerlegeLs(text.substr(a, e - a));
        if (z.gueltig) v.push_back(std::move(z));
        a = e + 1;
    }
    return v;
}

}  // namespace k1520::kermit
