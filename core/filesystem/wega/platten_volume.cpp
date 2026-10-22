/**
 * @file platten_volume.cpp
 * @brief Umsetzung von @ref PlattenVolume.
 */

#include "core/filesystem/wega/platten_volume.h"

#include "core/filesystem/fileinfo.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;
using namespace k1520::fileinfo;

namespace {

bool schreibeDatei(const fs::path& ziel, const std::vector<uint8_t>& d) {
    std::ofstream f(ziel, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
    return static_cast<bool>(f);
}

/// @brief Rechte, Besitzer und Zeit eines Eintrags als `.fileinfo` neben @p ziel ablegen.
bool schreibeAngaben(const FileEntry& e, const std::string& voll, const fs::path& ziel) {
    FileAngaben a;
    a.wega = true;
    a.name = voll;
    a.x    = unixAus(e);
    return schreibeFileinfo(fileinfoPfad(ziel), a);
}

}  // namespace

std::unique_ptr<PlattenVolume> PlattenVolume::open(const std::string& pfad, std::string& err,
                                                   bool read_only) {
    auto p = WegaPlatte::open(pfad, err, read_only);
    if (!p) return nullptr;
    std::unique_ptr<PlattenVolume> v(new PlattenVolume);
    v->platte_ = std::move(p);
    return v;
}

std::pair<int, std::string> PlattenVolume::zerlege(const std::string& text) const {
    const size_t s = text.find('/');
    const int v = platte_->volumeFromDir(text.substr(0, s));
    if (v < 0) return {-1, text};
    return {v, s == std::string::npos ? std::string() : text.substr(s + 1)};
}

std::vector<FileEntry> PlattenVolume::list() const {
    std::vector<FileEntry> aus;
    for (int v = 0; v < platte_->volumeCount(); ++v) {
        const std::string dir = platte_->volumeDir(v);
        FileEntry w;
        w.volume     = v;
        w.name       = dir;
        w.type       = "d";
        w.attributes = "drwxr-xr-x";
        aus.push_back(w);
        for (FileEntry e : platte_->fs(v).list()) {
            e.volume = v;
            e.name   = dir + "/" + e.name;
            aus.push_back(std::move(e));
        }
    }
    return aus;
}

bool PlattenVolume::read(const std::string& pfad, std::vector<uint8_t>& out) {
    const auto [v, name] = zerlege(pfad);
    if (v < 0 || name.empty()) return fail("Partition oder Dateiname fehlt (md0/..., md2/...): " + pfad);
    if (!platte_->fs(v).read(name, out)) return fail(platte_->fs(v).lastError());
    return true;
}

bool PlattenVolume::write(const std::string& pfad, const std::vector<uint8_t>& data,
                          const WriteOptions& opt) {
    if (schreibschutz()) return false;
    const auto [v, name] = zerlege(pfad);
    if (v < 0 || name.empty()) return fail("Partition oder Dateiname fehlt (md0/..., md2/...): " + pfad);
    if (!platte_->fs(v).write(name, data, opt)) return fail(platte_->fs(v).lastError());
    return true;
}

bool PlattenVolume::erase(const std::string& pfad) {
    if (schreibschutz()) return false;
    const auto [v, name] = zerlege(pfad);
    if (v < 0 || name.empty()) return fail("Partition oder Dateiname fehlt (md0/..., md2/...): " + pfad);
    if (!platte_->fs(v).erase(name)) return fail(platte_->fs(v).lastError());
    return true;
}

bool PlattenVolume::makeDirectory(const std::string& pfad) {
    if (schreibschutz()) return false;
    const auto [v, name] = zerlege(pfad);
    if (v < 0) return fail("Partition fehlt (md0/..., md2/...): " + pfad);
    if (!platte_->fs(v).makeDirectory(name)) return fail(platte_->fs(v).lastError());
    return true;
}

bool PlattenVolume::setAttributes(const std::string& pfad, const UnixAttrs& a) {
    if (schreibschutz()) return false;
    const auto [v, name] = zerlege(pfad);
    if (v < 0 || name.empty()) return fail("Partition oder Dateiname fehlt (md0/..., md2/...): " + pfad);
    if (!platte_->fs(v).setAttributes(name, a)) return fail(platte_->fs(v).lastError());
    return true;
}

bool PlattenVolume::extractEintrag(const std::string& pfad, const std::string& dest_dir,
                                   bool overwrite, bool dry_run) {
    const auto [v, name] = zerlege(pfad);
    if (v < 0) return fail("Partition fehlt (md0/..., md2/...): " + pfad);
    FileEntry gefunden;
    bool da = false;
    if (name.empty()) {
        gefunden.type = "d";
        gefunden.attributes = "drwxr-xr-x";
        da = true;
    } else {
        for (const FileEntry& e : platte_->fs(v).list())
            if (e.name == name) { gefunden = e; da = true; break; }
    }
    if (!da) return fail("Eintrag nicht gefunden: " + pfad);
    const std::string voll = platte_->volumeDir(v) + (name.empty() ? "" : "/" + name);
    const fs::path ziel = fs::path(dest_dir) / voll;
    std::error_code ec;
    if (gefunden.type == "d") {
        if (dry_run) return true;
        fs::create_directories(ziel, ec);
        if (ec) return fail("Ordner nicht anlegbar: " + ziel.string());
        // Die Partitionswurzel hat keine eigene Inode-Auskunft hier — nur Unterordner.
        if (!name.empty() && !schreibeAngaben(gefunden, voll, ziel))
            return fail("Angabendatei nicht schreibbar: " + fileinfoPfad(ziel).string());
        return true;
    }
    if (gefunden.type != "-") return true;          // Geraete/FIFOs haben keinen Inhalt
    std::vector<uint8_t> d;
    if (!platte_->fs(v).read(name, d)) return fail(platte_->fs(v).lastError());
    if (dry_run) return true;
    if (!overwrite && fs::exists(ziel, ec)) return fail("Zieldatei existiert bereits: " + ziel.string());
    fs::create_directories(ziel.parent_path(), ec);
    if (!schreibeDatei(ziel, d)) return fail("Zieldatei nicht schreibbar: " + ziel.string());
    if (!schreibeAngaben(gefunden, voll, ziel))
        return fail("Angabendatei nicht schreibbar: " + fileinfoPfad(ziel).string());
    return true;
}

bool PlattenVolume::extract(const std::string& pfad, const std::string& dest_dir, bool overwrite) {
    std::error_code ec;
    fs::create_directories(dest_dir, ec);
    if (ec) return fail("Zielordner nicht anlegbar: " + dest_dir);
    std::string p = pfad;
    while (!p.empty() && p.back() == '/') p.pop_back();
    for (const FileEntry& e : list()) {
        const bool treffer = p.empty() || e.name == p
                             || (e.name.size() > p.size() && e.name.compare(0, p.size(), p) == 0
                                 && e.name[p.size()] == '/');
        if (treffer && !extractEintrag(e.name, dest_dir, overwrite)) return false;
    }
    return true;
}

bool PlattenVolume::insert(const std::string& quelle, const std::string& ziel, bool overwrite,
                           const Fortschritt& fortschritt) {
    if (schreibschutz()) return false;
    const auto [v, basis] = zerlege(ziel);
    if (v < 0) return fail("Ziel mit Partition angeben (md0 = /usr, md2 = /): " + ziel);
    std::error_code ec;

    // Eine Datei: Angaben aus dem `.fileinfo`, sonst aus der Linux-Datei.
    auto datei = [&](const fs::path& q, const std::string& zn) -> bool {
        std::ifstream f(q, std::ios::binary);
        if (!f) return fail("Quelle nicht lesbar: " + q.string());
        std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        WriteOptions w;
        w.overwrite = overwrite;
        bool mode_da = false;
        FileAngaben ein;
        if (leseFileinfo(fileinfoPfad(q), ein) && (ein.wega || ein.tar)) {
            if (ein.x.mode_da) {
                w.wega_mode = static_cast<uint16_t>(ein.x.mode & 07777);
                w.wega_mode_gesetzt = true;
                mode_da = true;
            }
            if (ein.x.owner_da) {
                w.wega_uid = ein.x.uid;
                w.wega_gid = ein.x.gid;
                w.wega_owner_gesetzt = true;
            }
            w.wega_mtime = ein.x.mtime;       // 0 = jetzt
        }
        if (!mode_da) {
#ifdef _WIN32
            const bool x = false;             // Windows kennt kein Ausfuehrungsrecht
#else
            std::error_code e2;
            const bool x = (fs::status(q, e2).permissions() & fs::perms::owner_exec) != fs::perms::none;
#endif
            w.wega_mode = x ? 0755 : 0644;
            w.wega_mode_gesetzt = true;
        }
        if (!platte_->fs(v).write(zn, d, w)) return fail(platte_->fs(v).lastError());
        if (fortschritt) fortschritt(q.string(), platte_->volumeDir(v) + "/" + zn);
        return true;
    };

    if (!fs::is_directory(quelle, ec)) {
        if (basis.empty()) return fail("Zieldatei angeben (md<N>/pfad/name): " + ziel);
        return datei(quelle, basis);
    }

    std::vector<std::pair<fs::path, std::string>> ordner;     // fuer die Angaben danach
    ordner.emplace_back(quelle, basis);
    for (const auto& e : fs::recursive_directory_iterator(quelle, ec)) {
        const std::string rel = e.path().lexically_relative(quelle).generic_string();
        const std::string zn = basis.empty() ? rel : basis + "/" + rel;
        if (e.is_directory()) {
            if (!platte_->fs(v).makeDirectory(zn)) return fail(platte_->fs(v).lastError());
            ordner.emplace_back(e.path(), zn);
            continue;
        }
        if (istFileinfo(e.path().filename())) continue;      // Zubehoer, keine Nutzdatei
        if (!datei(e.path(), zn)) return false;
    }
    // Verzeichnisrechte zuletzt: jede Datei darin aendert die mtime des Verzeichnisses.
    for (auto it = ordner.rbegin(); it != ordner.rend(); ++it) {
        if (it->second.empty()) continue;                     // Partitionswurzel
        FileAngaben ein;
        if (!leseFileinfo(fileinfoPfad(it->first), ein) || !(ein.wega || ein.tar)) continue;
        UnixAttrs ua;
        if (ein.x.mode_da)  { ua.set_mode = true; ua.mode = ein.x.mode & 07777; }
        if (ein.x.owner_da) { ua.set_uid = ua.set_gid = true; ua.uid = ein.x.uid; ua.gid = ein.x.gid; }
        if (ein.x.mtime)    { ua.set_mtime = true; ua.mtime = ein.x.mtime; }
        if (!platte_->fs(v).setAttributes(it->second, ua)) return fail(platte_->fs(v).lastError());
    }
    return true;
}

FsCheckReport PlattenVolume::check(FsCheckLevel level) {
    FsCheckReport gesamt;
    gesamt.level = level;
    for (int v = 0; v < platte_->volumeCount(); ++v) {
        FsCheckReport r = platte_->fs(v).check(level, true);
        for (FsFinding& f : r.findings) {
            f.volume = v;
            f.object = platte_->volumeDir(v) + ":" + f.object;
        }
        gesamt.uebernimm(r);
    }
    gesamt.sortieren();
    return gesamt;
}

bool PlattenVolume::flush() {
    if (!platte_->flush()) return fail(platte_->lastError());
    return true;
}
