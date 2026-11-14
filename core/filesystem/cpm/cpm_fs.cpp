/**
 * @file cpm_fs.cpp
 * @brief Umsetzung von @ref CpmFileSystem — Lesepfad.
 *
 * @see doc/design/13_k1520disktool.md §7
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include "core/filesystem/cpm/cpm_fs.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace {

/// @brief Ein logischer CP/M-Extent fasst immer 128 Saetze à 128 B = 16 KB.
constexpr uint32_t kExtentBytes  = 16384;
constexpr uint32_t kRecordBytes  = 128;

/// @brief "NAME    TYP" → "NAME.TYP" (Hochbits sind Attribute und gehoeren nicht zum Namen).
std::string entryName(const uint8_t* p) {
    std::string name, typ;
    for (int i = 0; i < 8;  ++i) name += static_cast<char>(p[1 + i] & 0x7F);
    for (int i = 0; i < 3;  ++i) typ  += static_cast<char>(p[9 + i] & 0x7F);
    while (!name.empty() && name.back() == ' ') name.pop_back();
    while (!typ.empty()  && typ.back()  == ' ') typ.pop_back();
    return typ.empty() ? name : name + "." + typ;
}

/// @brief Grossschreibung fuer den Namensvergleich (CP/M kennt keine Kleinbuchstaben).
std::string upper(const std::string& s) {
    std::string o = s;
    for (char& c : o) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return o;
}

/// @brief "3:NAME.TYP" aufspalten; ohne Praefix ist es Nutzerbereich 0.
void splitUser(const std::string& in, int& user, std::string& name) {
    user = 0;
    name = in;
    const size_t k = in.find(':');
    if (k == std::string::npos || k == 0 || k > 2) return;
    bool nur_ziffern = true;
    for (size_t i = 0; i < k; ++i)
        if (!std::isdigit(static_cast<unsigned char>(in[i]))) nur_ziffern = false;
    if (!nur_ziffern) return;
    user = std::stoi(in.substr(0, k));
    name = in.substr(k + 1);
}

}  // namespace

// ─── Aufsetzen ───────────────────────────────────────────────────────────────

CpmFileSystem::CpmFileSystem(SectorSpace& space, const FsProfile& prof)
    : space_(space), prof_(prof) {}

std::unique_ptr<CpmFileSystem> CpmFileSystem::mount(SectorSpace& space,
                                                    const FsProfile& prof,
                                                    std::string& err) {
    if (prof.type != FsType::Cpm) {
        err = "Profil '" + prof.name + "' ist kein CP/M-Dateisystem";
        return nullptr;
    }

    const int start = space.trackIndexOf(prof.data_cyl, prof.data_head);
    if (start < 0) {
        err = "data_start c" + std::to_string(prof.data_cyl) + "h"
            + std::to_string(prof.data_head) + " liegt nicht im Sektorraum";
        return nullptr;
    }

    std::unique_ptr<CpmFileSystem> fs(new CpmFileSystem(space, prof));

    // Der Datenbereich muss einheitlich sein — CP/M rechnet mit EINER Sektorgroesse
    // je Diskette.  Die gemischte Geometrie einer Bootdiskette liegt vollstaendig
    // VOR data_start (cpa_ds80_5x1024_boot: drei 128-B-Seiten + eine 1024-B-Spur).
    const SectorSpace::TrackRef erste = space.trackAt(static_cast<size_t>(start));
    fs->secs_per_track_ = erste.sectors;
    fs->sector_size_    = erste.sector_size;

    for (size_t i = static_cast<size_t>(start); i < space.trackCount(); ++i) {
        const SectorSpace::TrackRef t = space.trackAt(i);
        if (t.sectors != fs->secs_per_track_ || t.sector_size != fs->sector_size_) {
            err = "Datenbereich ist nicht einheitlich: Spur c" + std::to_string(t.cyl)
                + "h" + std::to_string(t.head) + " hat " + std::to_string(t.sectors)
                + "x" + std::to_string(t.sector_size) + ", erwartet "
                + std::to_string(fs->secs_per_track_) + "x"
                + std::to_string(fs->sector_size_);
            return nullptr;
        }
        fs->data_bytes_ += t.bytes;
    }

    fs->data_start_   = erste.start;
    fs->total_blocks_ = static_cast<uint32_t>(fs->data_bytes_ / prof.block_size);
    if (prof.max_blocks && fs->total_blocks_ > prof.max_blocks) fs->total_blocks_ = prof.max_blocks;
    if (fs->total_blocks_ == 0) {
        err = "Datenbereich ist kleiner als ein Block";
        return nullptr;
    }

    // Ab 256 Bloecken sind die Blockzeiger 16 Bit breit — das ergibt sich aus der
    // Kapazitaet und steht deshalb NICHT im Profil (so haelt es auch CP/M selbst).
    fs->wide_ptr_       = (fs->total_blocks_ > 255);
    fs->ptrs_per_entry_ = fs->wide_ptr_ ? 8 : 16;
    fs->ext_per_entry_  = std::max<uint32_t>(
        1, (fs->ptrs_per_entry_ * prof.block_size) / kExtentBytes);

    const uint32_t dir_bytes = static_cast<uint32_t>(prof.dir_entries) * 32;
    fs->dir_blocks_ = (dir_bytes + prof.block_size - 1) / prof.block_size;
    if (prof.reserved_blocks > fs->dir_blocks_) fs->dir_blocks_ = prof.reserved_blocks;   // AL0/AL1 > Verzeichnis
    if (fs->dir_blocks_ >= fs->total_blocks_) {
        err = "Verzeichnis (" + std::to_string(prof.dir_entries)
            + " Eintraege) passt nicht in den Datenbereich";
        return nullptr;
    }

    fs->buildSkewTable();

    // MicroDOS 1526: der SYL-Lader belegt die ersten acht Verzeichnisplaetze.  Ob er DA ist,
    // entscheidet, ob sie Bootbereich oder (Datendiskette) gewoehnliche Plaetze sind.
    if (prof.dir_boot && prof.boot_header == "microdos1715") {
        // MicroDOS 1715: Platz 0 = Bootkopf, 1…3 = Parametersaetze mit Nutzerbyte 10H (nicht F0).
        std::vector<uint8_t> kopf(32, 0);
        fs->syl_aktiv_ = fs->readAt(0, kopf.data(), kopf.size()) && bootKopf(kopf.data(), kopf.size());
    }
    if (prof.dir_boot && prof.boot_header == "microdos") {
        std::vector<uint8_t> kopf(fs->bootBytes(), 0);
        fs->syl_aktiv_ = fs->readAt(0, kopf.data(), kopf.size()) && sylKopf(kopf.data(), kopf.size());
    }
    return fs;
}

void CpmFileSystem::buildSkewTable() {
    const uint16_t n = secs_per_track_;
    skew_tab_.assign(n, 0);
    if (prof_.skew == 0) {
        for (uint16_t i = 0; i < n; ++i) skew_tab_[i] = static_cast<uint8_t>(i);
        return;
    }
    // Klassische CP/M-Versatztabelle (wie in cpmtools): jeder Schritt springt um
    // `skew` weiter, belegte Plaetze werden uebersprungen.
    std::vector<bool> belegt(n, false);
    uint16_t j = 0;
    for (uint16_t i = 0; i < n; ++i) {
        while (belegt[j]) j = static_cast<uint16_t>((j + 1) % n);
        skew_tab_[i] = static_cast<uint8_t>(j);
        belegt[j] = true;
        j = static_cast<uint16_t>((j + prof_.skew) % n);
    }
}

// ─── Lesen ───────────────────────────────────────────────────────────────────

bool CpmFileSystem::readAt(uint64_t offset, uint8_t* dst, size_t n) const {
    const int start = space_.trackIndexOf(prof_.data_cyl, prof_.data_head);
    if (start < 0) return fail("Datenbereich nicht auffindbar");

    const uint64_t track_bytes = static_cast<uint64_t>(secs_per_track_) * sector_size_;
    SectorData sec;

    while (n > 0) {
        if (offset + 1 > data_bytes_) return fail("Lesen ueber das Dateisystemende hinaus");

        const uint64_t ti    = offset / track_bytes;
        const uint64_t rest  = offset % track_bytes;
        const uint16_t lsec  = static_cast<uint16_t>(rest / sector_size_);
        const size_t   insec = static_cast<size_t>(rest % sector_size_);

        const SectorSpace::TrackRef t =
            space_.trackAt(static_cast<size_t>(start) + static_cast<size_t>(ti));
        const uint8_t id = static_cast<uint8_t>(t.first_id + skew_tab_[lsec]);

        if (!space_.readSector(t.cyl, t.head, id, sec))
            return fail(space_.lastError());
        if (sec.data.size() < sector_size_)
            return fail("Sektor " + std::to_string(id) + " auf Spur "
                        + std::to_string(t.cyl) + "/" + std::to_string(t.head)
                        + " ist zu kurz");

        const size_t chunk = std::min(n, static_cast<size_t>(sector_size_) - insec);
        std::copy(sec.data.begin() + static_cast<long>(insec),
                  sec.data.begin() + static_cast<long>(insec + chunk), dst);
        dst    += chunk;
        offset += chunk;
        n      -= chunk;
    }
    return true;
}

bool CpmFileSystem::readBlock(uint16_t block, std::vector<uint8_t>& out) const {
    if (block >= total_blocks_)
        return fail("Blocknummer " + std::to_string(block) + " liegt hinter dem Datenbereich ("
                    + std::to_string(total_blocks_) + " Bloecke)");
    const size_t alt = out.size();
    out.resize(alt + prof_.block_size);
    return readAt(static_cast<uint64_t>(block) * prof_.block_size, out.data() + alt,
                  prof_.block_size);
}

bool CpmFileSystem::readDirectory(std::vector<uint8_t>& out) const {
    out.assign(static_cast<size_t>(prof_.dir_entries) * 32, 0xE5);
    return readAt(0, out.data(), out.size());
}

// ─── Verzeichnis ─────────────────────────────────────────────────────────────

std::vector<CpmDirEntry> CpmFileSystem::directory() const {
    std::vector<CpmDirEntry> result;
    std::vector<uint8_t> roh;
    if (!readDirectory(roh)) return result;

    for (int i = 0; i < prof_.dir_entries; ++i) {
        const uint8_t* p = roh.data() + static_cast<size_t>(i) * 32;

        CpmDirEntry e;
        e.index = i;
        e.user  = p[0];
        if (e.user == 0xE5) { result.push_back(e); continue; }
        if (bootPlatz(i, p)) { e.user = 0xF0; e.boot = true; result.push_back(e); continue; }

        e.name      = entryName(p);
        e.extent    = (p[12] & 0x1F) + (p[14] & 0x3F) * 32;
        e.records   = p[15];
        e.read_only = (p[ 9] & 0x80) != 0;
        e.system    = (p[10] & 0x80) != 0;
        e.archived  = (p[11] & 0x80) != 0;

        if (wide_ptr_)
            for (int k = 0; k < 8; ++k)
                e.blocks.push_back(static_cast<uint16_t>(p[16 + 2 * k] | (p[17 + 2 * k] << 8)));
        else
            for (int k = 0; k < 16; ++k) e.blocks.push_back(p[16 + k]);

        result.push_back(e);
    }
    return result;
}

bool CpmFileSystem::bootKopf(const uint8_t* p, size_t n) {
    return n >= 2 && (p[0] == 0x02 || p[0] == 0x03) && p[1] == 0xF0;
}

bool CpmFileSystem::osmUrlader(const uint8_t* p, size_t n) {
    // Zwei Fassungen desselben Urladers: 3.0 setzt den Stapel (LD SP,4000H), 2.0 (15.04.1987) hat dort
    // drei Nullbytes und liegt einen Vektor tiefer (LD IY,0C42H).  Gemeinsam: LD A,(FDCONF) dahinter
    // und die Kennung bei 0x80.
    static const uint8_t kAnfang30[10] = {0xFD, 0x21, 0x48, 0x0C, 0x31, 0x00, 0x40, 0x3A, 0x7E, 0x0C};
    static const uint8_t kAnfang20[10] = {0xFD, 0x21, 0x42, 0x0C, 0x00, 0x00, 0x00, 0x3A, 0x7E, 0x0C};
    static const char    kKennung[8] = {'P', '8', '0', '0', '0', 'S', 'Y', 'S'};
    return n >= 0x88
        && (std::equal(kAnfang30, kAnfang30 + 10, p) || std::equal(kAnfang20, kAnfang20 + 10, p))
        && std::equal(kKennung, kKennung + 8, p + 0x80);
}

bool CpmFileSystem::osmUrladerVon(const SectorSpace& raum, uint8_t* fdconf) {
    if (raum.trackCount() == 0) return false;
    const int i = raum.trackIndexOf(0, 0);
    if (i < 0) return false;
    const SectorSpace::TrackRef t = raum.trackAt(static_cast<size_t>(i));
    SectorData s;
    if (!raum.readSector(t.cyl, t.head, t.first_id, s)
        || !osmUrlader(s.data.data(), s.data.size()))
        return false;
    if (fdconf) *fdconf = s.data[kOsmFdconf];
    return true;
}

uint16_t CpmFileSystem::osmTypenFuer(const SectorSpace& raum) {
    if (raum.trackCount() == 0) return 0;
    const SectorSpace::TrackRef t = raum.trackAt(raum.trackCount() - 1);   // eine Datenspur
    const bool    zweiseitig = raum.trackIndexOf(0, 1) >= 0;
    const unsigned zylinder  = static_cast<unsigned>(t.cyl) + 1;
    if (t.sector_size == 256 && t.sectors == 16) {
        if (zweiseitig) return zylinder >= 70 ? (1u << 5) : 0;
        return zylinder >= 70 ? (1u << 4) : ((1u << 2) | (1u << 3));
    }
    if (zweiseitig && zylinder >= 70) {
        if (t.sector_size == 512 && t.sectors == 9) return 1u << 6;
        if (t.sector_size == 1024 && t.sectors == 5) return 1u << 7;
    }
    return 0;
}

bool CpmFileSystem::sylKopf(const uint8_t* p, size_t n) {
    if (n < 0x100 || p[0] != 'S' || p[1] != 'Y' || p[2] != 'L') return false;
    static const char kText[8] = {'M', 'i', 'c', 'r', 'o', 'D', 'O', 'S'};
    return std::search(p, p + 0x100, kText, kText + 8) != p + 0x100;
}

bool CpmFileSystem::microDosSysName(const uint8_t* p) {
    static const char kName[11] = {'M', 'i', 'c', 'r', 'o', 'D', 'O', 'S', 'S', 'Y', 'S'};
    if (p[0] != 0) return false;
    for (int i = 0; i < 11; ++i)
        if ((p[1 + i] & 0x7F) != static_cast<uint8_t>(kName[i])) return false;
    return true;
}

bool CpmFileSystem::istSystemDateiName(const uint8_t* p) const {
    if (prof_.boot_header != "microdos1715") return microDosSysName(p);
    // MicroDOS 1715: gewoehnlicher Name in Grossschrift, die Endung traegt das SYS-Bit (Bit 7 von Byte 10).
    static const char kName[11] = {'M', 'I', 'C', 'R', 'O', 'D', 'O', 'S', 'S', 'Y', 'S'};
    if (p[0] != 0) return false;
    for (int i = 0; i < 11; ++i)
        if ((p[1 + i] & 0x7F) != static_cast<uint8_t>(kName[i])) return false;
    return true;
}

size_t CpmFileSystem::bootBytes() const {
    return prof_.boot_header == "microdos" ? 256 : 128;
}

bool CpmFileSystem::bootKopfPasst(const uint8_t* p, size_t n) const {
    return prof_.boot_header == "microdos" ? sylKopf(p, n) : bootKopf(p, n);
}

bool CpmFileSystem::bootPlatz(int index, const uint8_t* p) const {
    if (!prof_.dir_boot) return false;
    if (prof_.boot_header == "microdos") return syl_aktiv_ && index >= 0 && index < 8;
    if (prof_.boot_header == "microdos1715") return syl_aktiv_ && index >= 0 && index < 4;
    if (index == 0) return bootKopf(p, 32);
    return index >= 1 && index <= 3 && p[0] == 0xF0;
}

bool CpmFileSystem::readBootSlots(std::vector<uint8_t>& out) const {
    out.assign(bootBytes(), 0xE5);
    return readAt(0, out.data(), out.size());
}

bool CpmFileSystem::writeBootSlots(const std::vector<uint8_t>& img) {
    if (!prof_.dir_boot) return fail("Das Profil '" + prof_.name + "' hat keinen Bootbereich im Verzeichnis");
    if (prof_.boot_header == "microdos") {
        // Plaetze 0…7 als Ganzes; jeder muss frei oder schon Lader sein.
        if (img.size() < 256 || !sylKopf(img.data(), img.size()))
            return fail("Das Bootabbild traegt keinen MicroDOS-SYL-Lader (\"SYL\" und der Text "
                        "\"MicroDOS\" in den ersten 256 Byte)");
        std::vector<uint8_t> alt;
        if (!readBootSlots(alt)) return false;
        for (int i = 0; i < 8; ++i) {
            const uint8_t* a = alt.data() + static_cast<size_t>(i) * 32;
            if (a[0] != 0xE5 && !bootPlatz(i, a))
                return fail("Verzeichnisplatz " + std::to_string(i) + " traegt einen Dateieintrag - "
                            "der Lader wuerde ihn ueberschreiben");
        }
        if (!writeAt(0, img.data(), 256)) return false;
        syl_aktiv_ = true;
        return true;
    }
    if (prof_.boot_header == "microdos1715") {
        if (img.size() < 128 || !bootKopf(img.data(), img.size()))
            return fail("Das Bootabbild traegt keinen PC-1715-Bootkopf (Wort F002H/F003H, Bytes 02|03 F0)");
        std::vector<uint8_t> alt;
        if (!readBootSlots(alt)) return false;
        for (int i = 0; i < 4; ++i) {
            const uint8_t* q = alt.data() + static_cast<size_t>(i) * 32;
            if (q[0] != 0xE5 && !bootPlatz(i, q))
                return fail("Verzeichnisplatz " + std::to_string(i) + " traegt einen Dateieintrag - "
                            "der Bootbereich wuerde ihn ueberschreiben");
        }
        if (!writeAt(0, img.data(), 128)) return false;
        syl_aktiv_ = true;
        return true;
    }
    if (img.size() < 128 || !bootKopf(img.data(), img.size()))
        return fail("Das Bootabbild traegt keinen PC-1715-Bootkopf (Wort F002H/F003H, Bytes 02|03 F0)");

    std::vector<uint8_t> alt;
    if (!readBootSlots(alt)) return false;
    // Zielplaetze: 0 immer, 1…3 nur die Parametersaetze (Nutzerbyte F0) des Abbilds.
    std::vector<int> ziele{0};
    for (int i = 1; i <= 3; ++i) if (img[static_cast<size_t>(i) * 32] == 0xF0) ziele.push_back(i);
    for (int i : ziele) {
        const uint8_t* a = alt.data() + static_cast<size_t>(i) * 32;
        if (a[0] != 0xE5 && !bootPlatz(i, a))
            return fail("Verzeichnisplatz " + std::to_string(i) + " traegt einen Dateieintrag - "
                        "der Bootbereich wuerde ihn ueberschreiben");
    }
    for (int i : ziele)
        if (!writeDirEntry(i, img.data() + static_cast<size_t>(i) * 32)) return false;
    return true;
}

bool CpmFileSystem::directoryRaw(std::vector<uint8_t>& out) const {
    return readDirectory(out);
}

int CpmFileSystem::directoryFill() const {
    std::vector<uint8_t> roh;
    if (!readDirectory(roh) || roh.empty()) return -1;
    for (uint8_t b : roh)
        if (b != roh[0]) return -1;
    return roh[0];
}

std::vector<FileEntry> CpmFileSystem::list() const {
    // Extents derselben Datei zusammenfassen: Schluessel ist (Nutzerbereich, Name).
    struct Sammler {
        FileEntry ent;
        int       max_extent = -1;
        uint8_t   rc_of_max  = 0;
    };
    std::map<std::pair<int, std::string>, Sammler> nach_datei;

    for (const CpmDirEntry& d : directory()) {
        if (d.free() || d.user > 15) continue;      // frei oder Passwort-/Etikett-Satz
        auto& s = nach_datei[{d.user, d.name}];
        if (s.ent.name.empty()) {
            s.ent.name       = d.name;
            s.ent.user       = d.user;
            s.ent.hidden     = d.system;
            s.ent.attributes = std::string(d.read_only ? "RO " : "")
                             + (d.system   ? "SYS " : "")
                             + (d.archived ? "ARC"  : "");
            while (!s.ent.attributes.empty() && s.ent.attributes.back() == ' ')
                s.ent.attributes.pop_back();
        }
        if (d.extent > s.max_extent) { s.max_extent = d.extent; s.rc_of_max = d.records; }
    }

    std::vector<FileEntry> out;
    for (auto& [schluessel, s] : nach_datei) {
        // Klassische CP/M-2.2-Groessenrechnung: der hoechste Extent bestimmt die Laenge.
        s.ent.size = static_cast<uint64_t>(s.max_extent) * kExtentBytes
                   + static_cast<uint64_t>(s.rc_of_max)  * kRecordBytes;
        out.push_back(s.ent);
        (void)schluessel;
    }
    std::sort(out.begin(), out.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.user != b.user) return a.user < b.user;
        return a.name < b.name;
    });
    return out;
}

bool CpmFileSystem::firstSector(const std::string& name, FsRecoverOrt& out) const {
    int gesucht_user = 0;
    std::string gesucht_name;
    splitUser(name, gesucht_user, gesucht_name);
    gesucht_name = upper(gesucht_name);

    // Der Anfang der Datei ist der erste Blockzeiger des KLEINSTEN Extents — nicht
    // der des ersten gefundenen Verzeichnisplatzes: die Plaetze einer Datei stehen
    // im Verzeichnis in beliebiger Reihenfolge.
    // KOPIE, kein Zeiger: `directory()` gibt den Vektor als Wert zurueck — ein
    // Zeiger hinein zeigte nach der Schleife ins Leere (und lieferte stumm nichts).
    CpmDirEntry anfang;
    bool gefunden = false;
    for (const CpmDirEntry& d : directory()) {
        if (d.free() || d.user > 15) continue;
        if (d.user != gesucht_user || upper(d.name) != gesucht_name) continue;
        if (!gefunden || d.extent < anfang.extent) { anfang = d; gefunden = true; }
    }
    if (!gefunden) return false;

    uint16_t blk = 0;
    for (uint16_t b : anfang.blocks) if (b != 0) { blk = b; break; }
    if (blk == 0 || blk >= total_blocks_) return false;

    const int start = space_.trackIndexOf(prof_.data_cyl, prof_.data_head);
    if (start < 0) return false;
    const uint64_t track_bytes = static_cast<uint64_t>(secs_per_track_) * sector_size_;
    const uint64_t off = static_cast<uint64_t>(blk) * prof_.block_size;
    const size_t   ti  = static_cast<size_t>(off / track_bytes);
    if (static_cast<size_t>(start) + ti >= space_.trackCount()) return false;

    const SectorSpace::TrackRef t = space_.trackAt(static_cast<size_t>(start) + ti);
    out.cyl  = t.cyl;
    out.head = t.head;
    // Die KENNUNG, nicht der Versatz in der Spur (wie bei den Funden der Rettung):
    // der Diskeditor sucht den Sektor ueber seine ID.
    out.sector = t.first_id + skew_tab_[(off % track_bytes) / sector_size_];
    return true;
}

bool CpmFileSystem::read(const std::string& name, std::vector<uint8_t>& out) {
    int gesucht_user = 0;
    std::string gesucht_name;
    splitUser(name, gesucht_user, gesucht_name);
    gesucht_name = upper(gesucht_name);

    // Alle Extents der Datei einsammeln, nach Extent-Nummer sortiert.
    std::vector<CpmDirEntry> extents;
    for (const CpmDirEntry& d : directory()) {
        if (d.free() || d.user > 15) continue;
        if (d.user != gesucht_user || upper(d.name) != gesucht_name) continue;
        extents.push_back(d);
    }
    if (extents.empty())
        return fail("Datei '" + name + "' steht nicht im Verzeichnis");

    std::sort(extents.begin(), extents.end(),
              [](const CpmDirEntry& a, const CpmDirEntry& b) { return a.extent < b.extent; });

    int      max_extent = extents.back().extent;
    uint8_t  rc_of_max  = extents.back().records;
    const uint64_t laenge = static_cast<uint64_t>(max_extent) * kExtentBytes
                          + static_cast<uint64_t>(rc_of_max)  * kRecordBytes;

    out.clear();
    out.reserve(static_cast<size_t>(laenge));

    for (const CpmDirEntry& d : extents) {
        // Ein Verzeichnisplatz deckt ext_per_entry_ logische Extents ab; seine Bloecke
        // beginnen beim ERSTEN davon.
        const uint64_t start = static_cast<uint64_t>(d.extent & ~(ext_per_entry_ - 1))
                             * kExtentBytes;
        if (out.size() < start) out.resize(static_cast<size_t>(start), 0x00);

        for (uint16_t b : d.blocks) {
            if (b == 0) {                       // Loch (sparse) — CP/M nutzt Block 0 nie
                out.resize(out.size() + prof_.block_size, 0x00);
                continue;
            }
            if (!readBlock(b, out)) return false;
        }
    }

    if (out.size() > laenge) out.resize(static_cast<size_t>(laenge));
    return true;
}

// ─── Schreiben ───────────────────────────────────────────────────────────────

bool CpmFileSystem::writeAt(uint64_t offset, const uint8_t* src, size_t n) {
    const int start = space_.trackIndexOf(prof_.data_cyl, prof_.data_head);
    if (start < 0) return fail("Datenbereich nicht auffindbar");

    const uint64_t track_bytes = static_cast<uint64_t>(secs_per_track_) * sector_size_;
    SectorData sec;

    while (n > 0) {
        if (offset + 1 > data_bytes_) return fail("Schreiben ueber das Dateisystemende hinaus");

        const uint64_t ti    = offset / track_bytes;
        const uint64_t rest  = offset % track_bytes;
        const uint16_t lsec  = static_cast<uint16_t>(rest / sector_size_);
        const size_t   insec = static_cast<size_t>(rest % sector_size_);

        const SectorSpace::TrackRef t =
            space_.trackAt(static_cast<size_t>(start) + static_cast<size_t>(ti));
        const uint8_t id = static_cast<uint8_t>(t.first_id + skew_tab_[lsec]);

        // Teilsektor → lesen, aendern, zurueckschreiben.  Der Nachspann hinter der
        // Daten-CRC bleibt dabei unangetastet (leeres tail-Argument).
        if (!space_.readSector(t.cyl, t.head, id, sec)) return fail(space_.lastError());
        if (sec.data.size() < sector_size_)
            return fail("Sektor " + std::to_string(id) + " auf Spur "
                        + std::to_string(t.cyl) + "/" + std::to_string(t.head)
                        + " ist zu kurz");

        const size_t chunk = std::min(n, static_cast<size_t>(sector_size_) - insec);
        std::copy(src, src + chunk, sec.data.begin() + static_cast<long>(insec));
        if (!space_.writeSector(t.cyl, t.head, id, sec.data)) return fail(space_.lastError());

        src    += chunk;
        offset += chunk;
        n      -= chunk;
    }
    return true;
}

bool CpmFileSystem::writeDirEntry(int slot, const uint8_t* raw32) {
    if (slot < 0 || slot >= prof_.dir_entries) return fail("Verzeichnisplatz ausserhalb");
    return writeAt(static_cast<uint64_t>(slot) * 32, raw32, 32);
}

bool CpmFileSystem::validName(const std::string& name, std::string* why) {
    auto sag = [&](const std::string& m) { if (why) *why = m; return false; };

    const size_t punkt = name.find('.');
    const std::string basis = punkt == std::string::npos ? name : name.substr(0, punkt);
    const std::string typ   = punkt == std::string::npos ? "" : name.substr(punkt + 1);

    if (basis.empty())    return sag("Name ist leer");
    if (basis.size() > 8) return sag("Name '" + basis + "' ist laenger als 8 Zeichen");
    if (typ.size()   > 3) return sag("Typ '" + typ + "' ist laenger als 3 Zeichen");
    if (typ.find('.') != std::string::npos)
        return sag("CP/M kennt nur EINEN Punkt im Namen");

    // CP/M-Trennzeichen (siehe CP/M 2.2 Interface Guide) sind im Namen verboten.
    // Das LEERZEICHEN steht mit in der Liste: CP/M fuellt Name und Typ damit auf,
    // ein Name mit Leerzeichen ist also im Verzeichnis nicht mehr eindeutig
    // wiederzufinden und am CCP nicht eingebbar.
    static const std::string verboten = " <>.,;:=?*[]%|()/\\";
    for (char c : basis + typ) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u > 0x7E) return sag("Name enthaelt ein Sonderzeichen");
        if (verboten.find(c) != std::string::npos)
            return sag(std::string("Zeichen '") + c + "' ist in CP/M-Namen nicht erlaubt");
        if (c >= 'a' && c <= 'z') return sag("CP/M-Namen sind grossgeschrieben");
    }
    return true;
}

std::string CpmFileSystem::toCpmName(const std::string& linux_name) {
    // Nur der Dateiname, ohne Pfad.
    std::string n = linux_name;
    const size_t schraeg = n.find_last_of("/\\");
    if (schraeg != std::string::npos) n = n.substr(schraeg + 1);

    const size_t punkt = n.find_last_of('.');
    std::string basis = punkt == std::string::npos ? n : n.substr(0, punkt);
    std::string typ   = punkt == std::string::npos ? "" : n.substr(punkt + 1);

    auto saeubere = [](std::string s, size_t max) {
        std::string o;
        for (char c : s) {
            const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            static const std::string verboten = " <>.,;:=?*[]%|()/\\";
            if (u < 0x20 || u > 0x7E || verboten.find(u) != std::string::npos) continue;
            o += u;
            if (o.size() == max) break;
        }
        return o;
    };
    basis = saeubere(basis, 8);
    typ   = saeubere(typ, 3);
    if (basis.empty()) basis = "UNNAMED";
    return typ.empty() ? basis : basis + "." + typ;
}

bool CpmFileSystem::erase(const std::string& name) {
    int gesucht_user = 0;
    std::string gesucht_name;
    splitUser(name, gesucht_user, gesucht_name);
    gesucht_name = upper(gesucht_name);

    std::vector<uint8_t> roh;
    if (!readDirectory(roh)) return false;

    bool etwas = false;
    for (int i = 0; i < prof_.dir_entries; ++i) {
        uint8_t* p = roh.data() + static_cast<size_t>(i) * 32;
        if (p[0] == 0xE5 || p[0] > 15 || bootPlatz(i, p)) continue;
        if (p[0] != gesucht_user || upper(entryName(p)) != gesucht_name) continue;

        // CP/M loescht nur das Nutzerbyte — die Blockliste bleibt stehen und wird
        // beim naechsten Aufbau der Belegungskarte einfach nicht mehr gezaehlt.
        p[0] = 0xE5;
        if (!writeDirEntry(i, p)) return false;
        etwas = true;
    }
    if (!etwas) return fail("Datei '" + name + "' steht nicht im Verzeichnis");
    return true;
}

bool CpmFileSystem::setAttributes(const std::string& name, const CpmAttrs& a) {
    int gesucht_user = 0;
    std::string gesucht_name;
    splitUser(name, gesucht_user, gesucht_name);
    gesucht_name = upper(gesucht_name);

    if (a.set_user && (a.user < 0 || a.user > 15))
        return fail("Nutzerbereich muss zwischen 0 und 15 liegen");

    std::vector<uint8_t> roh;
    if (!readDirectory(roh)) return false;

    // Erst schauen, dann schreiben: ein Wechsel des Nutzerbereichs darf keine
    // vorhandene Datei verdecken — im Ziel gaebe es dann zwei Dateien gleichen
    // Namens, und CP/M faende beim Lesen die falsche.
    const bool zieht_um = a.set_user && a.user != gesucht_user;
    bool gefunden = false;
    for (int i = 0; i < prof_.dir_entries; ++i) {
        const uint8_t* p = roh.data() + static_cast<size_t>(i) * 32;
        if (p[0] == 0xE5 || p[0] > 15 || bootPlatz(i, p)) continue;
        if (upper(entryName(p)) != gesucht_name) continue;
        if (p[0] == gesucht_user) { gefunden = true; continue; }
        if (zieht_um && p[0] == a.user)
            return fail("Im Nutzerbereich " + std::to_string(a.user)
                        + " liegt bereits eine Datei '" + gesucht_name + "'");
    }
    if (!gefunden) return fail("Datei '" + name + "' steht nicht im Verzeichnis");

    // Alle Extents tragen die Attributbits erneut — sie muessen alle mitgehen.
    for (int i = 0; i < prof_.dir_entries; ++i) {
        uint8_t* p = roh.data() + static_cast<size_t>(i) * 32;
        if (p[0] == 0xE5 || p[0] > 15 || bootPlatz(i, p)) continue;
        if (p[0] != gesucht_user || upper(entryName(p)) != gesucht_name) continue;

        if (a.set_read_only) p[ 9] = static_cast<uint8_t>((p[ 9] & 0x7F) | (a.read_only ? 0x80 : 0));
        if (a.set_system)    p[10] = static_cast<uint8_t>((p[10] & 0x7F) | (a.system    ? 0x80 : 0));
        if (a.set_archived)  p[11] = static_cast<uint8_t>((p[11] & 0x7F) | (a.archived  ? 0x80 : 0));
        if (a.set_user)      p[0]  = static_cast<uint8_t>(a.user);

        if (!writeDirEntry(i, p)) return false;
    }
    return true;
}

bool CpmFileSystem::write(const std::string& name, const std::vector<uint8_t>& data,
                          const WriteOptions& opt) {
    int ziel_user = 0;
    std::string ziel_name;
    splitUser(name, ziel_user, ziel_name);
    ziel_name = upper(ziel_name);

    std::string warum;
    if (!validName(ziel_name, &warum)) return fail("'" + ziel_name + "': " + warum);
    if (ziel_user < 0 || ziel_user > 15) return fail("Nutzerbereich muss 0..15 sein");

    // Vorhandene gleichnamige Datei?
    bool vorhanden = false;
    for (const CpmDirEntry& d : directory())
        if (!d.free() && d.user == ziel_user && upper(d.name) == ziel_name) vorhanden = true;
    if (vorhanden) {
        if (!opt.overwrite)
            return fail("'" + name + "' existiert bereits - Ueberschreiben nicht erlaubt");
        if (!erase(ziel_user == 0 ? ziel_name : std::to_string(ziel_user) + ":" + ziel_name))
            return false;
    }

    // Auf ganze Saetze auffuellen: Textdateien mit 0x1A (CP/M-Dateiende), sonst 0x00.
    std::vector<uint8_t> inhalt = data;
    const uint8_t fuell = opt.text ? 0x1A : 0x00;
    if (inhalt.size() % kRecordBytes)
        inhalt.resize((inhalt.size() / kRecordBytes + 1) * kRecordBytes, fuell);
    const uint32_t saetze = static_cast<uint32_t>(inhalt.size() / kRecordBytes);
    if (saetze == 0) return fail("leere Dateien kann CP/M nicht ablegen");

    const uint32_t bloecke_noetig =
        static_cast<uint32_t>((inhalt.size() + prof_.block_size - 1) / prof_.block_size);

    // Freie Bloecke sammeln (First Fit, aufsteigend).
    std::vector<bool> karte = allocationMap();
    std::vector<uint16_t> frei;
    for (uint32_t b = dir_blocks_; b < total_blocks_ && frei.size() < bloecke_noetig; ++b)
        if (!karte[b]) frei.push_back(static_cast<uint16_t>(b));
    if (frei.size() < bloecke_noetig) {
        uint32_t gesamt_frei = 0;
        for (uint32_t b = dir_blocks_; b < total_blocks_; ++b) if (!karte[b]) ++gesamt_frei;
        return fail("Diskette voll: '" + ziel_name + "' braucht "
                    + std::to_string(bloecke_noetig) + " Bloecke, frei sind "
                    + std::to_string(gesamt_frei));
    }

    // Freie Verzeichnisplaetze sammeln.
    const uint32_t plaetze_noetig =
        (bloecke_noetig + ptrs_per_entry_ - 1) / ptrs_per_entry_;
    std::vector<int> slots;
    {
        std::vector<uint8_t> roh;
        if (!readDirectory(roh)) return false;
        for (int i = 0; i < prof_.dir_entries && slots.size() < plaetze_noetig; ++i)
            if (roh[static_cast<size_t>(i) * 32] == 0xE5
                && !bootPlatz(i, roh.data() + static_cast<size_t>(i) * 32)) slots.push_back(i);
    }
    if (slots.size() < plaetze_noetig)
        return fail("Verzeichnis voll: '" + ziel_name + "' braucht "
                    + std::to_string(plaetze_noetig) + " Eintraege");

    // ── Daten schreiben ──────────────────────────────────────────────────────
    for (uint32_t i = 0; i < bloecke_noetig; ++i) {
        const size_t ab   = static_cast<size_t>(i) * prof_.block_size;
        const size_t menge= std::min<size_t>(prof_.block_size, inhalt.size() - ab);
        std::vector<uint8_t> block(prof_.block_size, fuell);
        std::copy(inhalt.begin() + static_cast<long>(ab),
                  inhalt.begin() + static_cast<long>(ab + menge), block.begin());
        if (!writeAt(static_cast<uint64_t>(frei[i]) * prof_.block_size,
                     block.data(), block.size()))
            return false;
    }

    // ── Verzeichniseintraege schreiben ───────────────────────────────────────
    uint32_t rest_saetze = saetze;
    for (uint32_t g = 0; g < plaetze_noetig; ++g) {
        uint8_t e[32];
        std::fill(std::begin(e), std::end(e), static_cast<uint8_t>(0x00));

        e[0] = static_cast<uint8_t>(ziel_user);
        const size_t punkt = ziel_name.find('.');
        const std::string basis = punkt == std::string::npos ? ziel_name
                                                             : ziel_name.substr(0, punkt);
        const std::string typ   = punkt == std::string::npos ? "" : ziel_name.substr(punkt + 1);
        for (int i = 0; i < 8; ++i) e[1 + i] = i < (int)basis.size()
                                             ? static_cast<uint8_t>(basis[i]) : ' ';
        for (int i = 0; i < 3; ++i) e[9 + i] = i < (int)typ.size()
                                             ? static_cast<uint8_t>(typ[i]) : ' ';
        // Die drei Attributbits sitzen in den Hochbits des Typs — und zwar in JEDEM
        // Verzeichnisplatz der Datei.
        if (opt.cpm_read_only) e[ 9] |= 0x80;
        if (opt.cpm_system)    e[10] |= 0x80;
        if (opt.cpm_archived)  e[11] |= 0x80;

        // Saetze dieses Verzeichnisplatzes und daraus EX/S2/RC.
        const uint32_t max_hier = 128u * ext_per_entry_;
        const uint32_t hier     = std::min(rest_saetze, max_hier);
        const uint32_t letzter_log_ext = g * ext_per_entry_ + (hier - 1) / 128;
        const uint32_t rc = hier - 128u * ((hier - 1) / 128);

        e[12] = static_cast<uint8_t>(letzter_log_ext & 0x1F);
        e[13] = 0;                                   // S1
        e[14] = static_cast<uint8_t>((letzter_log_ext >> 5) & 0x3F);
        e[15] = static_cast<uint8_t>(rc);

        for (uint32_t k = 0; k < ptrs_per_entry_; ++k) {
            const uint32_t idx = g * ptrs_per_entry_ + k;
            const uint16_t b   = idx < bloecke_noetig ? frei[idx] : 0;
            if (wide_ptr_) {
                e[16 + 2 * k]     = static_cast<uint8_t>(b & 0xFF);
                e[16 + 2 * k + 1] = static_cast<uint8_t>(b >> 8);
            } else {
                e[16 + k] = static_cast<uint8_t>(b);
            }
        }

        if (!writeDirEntry(slots[g], e)) return false;
        rest_saetze -= hier;
    }
    return true;
}

bool CpmFileSystem::readSystemDatei(std::vector<uint8_t>& out) const {
    out.clear();
    std::vector<uint8_t> roh;
    if (!readDirectory(roh)) return false;
    for (int i = 0; i < prof_.dir_entries; ++i) {
        const uint8_t* p = roh.data() + static_cast<size_t>(i) * 32;
        if (bootPlatz(i, p) || !istSystemDateiName(p) || (p[12] & 0x1F) != 0 || p[14] != 0) continue;
        const size_t bytes = static_cast<size_t>(p[15]) * kRecordBytes;
        for (uint32_t k = 0; k < ptrs_per_entry_ && out.size() < bytes; ++k) {
            const uint16_t b = wide_ptr_ ? static_cast<uint16_t>(p[16 + 2 * k] | (p[17 + 2 * k] << 8))
                                         : p[16 + k];
            if (b == 0 || !readBlock(b, out)) return false;
        }
        out.resize(std::min(out.size(), bytes));
        return true;
    }
    return false;
}

std::vector<uint16_t> CpmFileSystem::systemDateiBloecke(uint32_t anzahl) const {
    std::vector<uint16_t> aus;
    if (prof_.boot_header == "microdos1715") {
        for (uint32_t k = 0; k < anzahl; ++k) aus.push_back(static_cast<uint16_t>(dir_blocks_ + k));
        return aus;
    }
    if (prof_.boot_header != "microdos") return aus;
    if (sector_size_ == 128) {
        // Die Eintragung von MSYSG20 (Anhang B der msysgen.txt, am Ladercode belegt: doc/design/13 §25.4).
        if (secs_per_track_ != 26 || prof_.block_size != 1024 || anzahl > 16) return {};
        for (uint32_t k = 0; k < anzahl; ++k) aus.push_back(kMicroDos128Bloecke[k]);
        return aus;
    }
    const int start = space_.trackIndexOf(prof_.data_cyl, prof_.data_head);
    if (start < 0 || secs_per_track_ < 4) return {};
    const uint64_t spur_bytes = static_cast<uint64_t>(secs_per_track_) * sector_size_;
    const uint64_t je_spur    = 4ull * sector_size_;
    for (uint8_t c = 1; c <= 4 && aus.size() < anzahl; ++c) {
        const int ti = space_.trackIndexOf(c, 0);
        if (ti < start) return {};
        const uint64_t ab = static_cast<uint64_t>(ti - start) * spur_bytes;
        if (ab % prof_.block_size || je_spur % prof_.block_size) return {};
        for (uint64_t b = ab / prof_.block_size; b < (ab + je_spur) / prof_.block_size && aus.size() < anzahl; ++b)
            aus.push_back(static_cast<uint16_t>(b));
    }
    return aus;
}

bool CpmFileSystem::writeSystemDatei(const std::vector<uint8_t>& data) {
    if (prof_.boot_header == "microdos1715") return writeSystemDatei1715(data);
    if (prof_.boot_header != "microdos")
        return fail("Das Profil '" + prof_.name + "' hat keine MicroDOS-Systemdatei");
    if (data.empty() || data.size() > kMicroDosSysMax)
        return fail("MicroDOS.SYS muss 1…" + std::to_string(kMicroDosSysMax) + " Byte gross sein");
    if (sector_size_ == 128) return writeSystemDatei128(data);

    // Die Sektoren 1…4 der Zylinder 1…4 (Kopf 0) als Bloecke: das ist, was SYL und INIT lesen.
    const int start = space_.trackIndexOf(prof_.data_cyl, prof_.data_head);
    if (start < 0) return fail("Datenbereich nicht auffindbar");
    const uint64_t spur_bytes = static_cast<uint64_t>(secs_per_track_) * sector_size_;
    const uint64_t je_spur    = 4ull * sector_size_;
    std::vector<uint16_t> bloecke;
    for (uint8_t c = 1; c <= 4 && bloecke.size() * prof_.block_size < data.size(); ++c) {
        const int ti = space_.trackIndexOf(c, 0);
        if (ti < start || secs_per_track_ < 4)
            return fail("Zylinder " + std::to_string(c) + " Kopf 0 mit vier Sektoren fehlt - "
                        "der SYL-Lader findet MicroDOS.SYS dort nicht");
        const uint64_t ab = static_cast<uint64_t>(ti - start) * spur_bytes;
        if (ab % prof_.block_size || je_spur % prof_.block_size)
            return fail("Blockgroesse " + std::to_string(prof_.block_size)
                        + " passt nicht auf die Systemspuren");
        for (uint64_t b = ab / prof_.block_size; b < (ab + je_spur) / prof_.block_size; ++b)
            bloecke.push_back(static_cast<uint16_t>(b));
    }
    const uint32_t noetig = static_cast<uint32_t>(
        (data.size() + prof_.block_size - 1) / prof_.block_size);
    if (bloecke.size() > noetig) bloecke.resize(noetig);
    if (bloecke.size() > ptrs_per_entry_) return fail("MicroDOS.SYS passt nicht in einen Verzeichnisplatz");

    // Eine vorhandene MicroDOS.SYS weicht (nur das Nutzerbyte), dann muessen die Bloecke frei sein.
    std::vector<uint8_t> roh;
    if (!readDirectory(roh)) return false;
    for (int i = 0; i < prof_.dir_entries; ++i) {
        uint8_t* p = roh.data() + static_cast<size_t>(i) * 32;
        if (!bootPlatz(i, p) && microDosSysName(p)) {
            p[0] = 0xE5;
            if (!writeDirEntry(i, p)) return false;
        }
    }
    const std::vector<bool> karte = allocationMap();
    for (uint16_t b : bloecke)
        if (b < dir_blocks_ || b >= total_blocks_ || karte[b])
            return fail("Block " + std::to_string(b) + " (Systemspur) ist belegt - MicroDOS.SYS muss an den "
                        "festen Ort der Sektoren 1-4 von Zylinder 1-4; die Datei dort erst entfernen");

    int slot = -1;
    for (int i = 0; i < prof_.dir_entries && slot < 0; ++i)
        if (roh[static_cast<size_t>(i) * 32] == 0xE5
            && !bootPlatz(i, roh.data() + static_cast<size_t>(i) * 32)) slot = i;
    if (slot < 0) return fail("Verzeichnis voll");

    std::vector<uint8_t> inhalt = data;
    if (inhalt.size() % kRecordBytes) inhalt.resize((inhalt.size() / kRecordBytes + 1) * kRecordBytes, 0);
    for (size_t i = 0; i < bloecke.size(); ++i) {
        std::vector<uint8_t> block(prof_.block_size, 0);
        const size_t ab = i * prof_.block_size;
        if (ab < inhalt.size())
            std::copy(inhalt.begin() + static_cast<long>(ab),
                      inhalt.begin() + static_cast<long>(std::min(inhalt.size(), ab + prof_.block_size)),
                      block.begin());
        if (!writeAt(static_cast<uint64_t>(bloecke[i]) * prof_.block_size, block.data(), block.size()))
            return false;
    }
    uint8_t e[32] = {};
    static const char kName[11] = {'M', 'i', 'c', 'r', 'o', 'D', 'O', 'S', 'S', 'Y', 'S'};
    for (int i = 0; i < 11; ++i) e[1 + i] = static_cast<uint8_t>(kName[i]);
    e[15] = static_cast<uint8_t>(inhalt.size() / kRecordBytes);
    for (size_t k = 0; k < bloecke.size(); ++k) {
        if (wide_ptr_) { e[16 + 2 * k] = bloecke[k] & 0xFF; e[17 + 2 * k] = bloecke[k] >> 8; }
        else e[16 + k] = static_cast<uint8_t>(bloecke[k]);
    }
    return writeDirEntry(slot, e);
}

bool CpmFileSystem::writeSystemDatei128(const std::vector<uint8_t>& data) {
    // 26 x 128 B, Block 1 KB: Lade-ROM V0.6 holt den Lader SYL 0.6 von Zylinder 1 Sektor 1-6, der laedt 128 Sektoren
    // ab Zylinder 1 Sektor 1 nach B900H und springt nach BD00H; der Lader in den Verzeichnisplaetzen 0-7 (V0.9)
    // liest Zylinder 1-5 ganz (5 x 26 Sektoren) an dieselbe Stelle.  Das System liegt also in den linearen Sektoren
    // 26..155, die der Eintrag von MSYSG20 ueber die Bloecke 3-17 und 19 erfasst; Block 18 (Sektor 144-151) gehoert
    // zum geladenen Bereich und muss Nullen tragen (Fuellbytes des Systemabbilds).
    if (secs_per_track_ != 26 || prof_.block_size != 1024 || ptrs_per_entry_ < 16)
        return fail("MicroDOS-Systemdiskette 26x128: erwartet 26 Sektoren je Spur, Block 1 KB, 16 Zeiger je Platz");
    std::vector<uint16_t> bloecke(kMicroDos128Bloecke, kMicroDos128Bloecke + 16);
    const uint32_t noetig = static_cast<uint32_t>((data.size() + prof_.block_size - 1) / prof_.block_size);
    if (bloecke.size() > noetig) bloecke.resize(noetig);

    // Eine vorhandene MicroDOS.SYS weicht (nur das Nutzerbyte); alles andere in 3..19 darf nicht belegt sein.
    std::vector<uint8_t> roh;
    if (!readDirectory(roh)) return false;
    for (int i = 0; i < prof_.dir_entries; ++i) {
        uint8_t* p = roh.data() + static_cast<size_t>(i) * 32;
        if (!bootPlatz(i, p) && microDosSysName(p)) {
            p[0] = 0xE5;
            if (!writeDirEntry(i, p)) return false;
        }
    }
    const std::vector<bool> karte = allocationMap(false);
    for (uint32_t b = 3; b <= 19; ++b)
        if (b >= total_blocks_ || karte[b])
            return fail("Block " + std::to_string(b) + " (Systemspur) ist belegt - MicroDOS.SYS muss an die festen "
                        "Sektoren von Zylinder 1-5; die Datei dort erst entfernen");
    int slot = -1;
    for (int i = 0; i < prof_.dir_entries && slot < 0; ++i)
        if (roh[static_cast<size_t>(i) * 32] == 0xE5
            && !bootPlatz(i, roh.data() + static_cast<size_t>(i) * 32)) slot = i;
    if (slot < 0) return fail("Verzeichnis voll");

    std::vector<uint8_t> inhalt = data;
    if (inhalt.size() % kRecordBytes) inhalt.resize((inhalt.size() / kRecordBytes + 1) * kRecordBytes, 0);
    for (size_t i = 0; i < bloecke.size(); ++i) {
        std::vector<uint8_t> block(prof_.block_size, 0);
        const size_t ab = i * prof_.block_size;
        if (ab < inhalt.size())
            std::copy(inhalt.begin() + static_cast<long>(ab),
                      inhalt.begin() + static_cast<long>(std::min(inhalt.size(), ab + prof_.block_size)),
                      block.begin());
        if (!writeAt(static_cast<uint64_t>(bloecke[i]) * prof_.block_size, block.data(), block.size()))
            return false;
    }
    const std::vector<uint8_t> nullen(prof_.block_size, 0);
    if (!writeAt(static_cast<uint64_t>(kMicroDos128Luecke) * prof_.block_size, nullen.data(), nullen.size()))
        return false;
    uint8_t e[32] = {};
    static const char kName[11] = {'M', 'i', 'c', 'r', 'o', 'D', 'O', 'S', 'S', 'Y', 'S'};
    for (int i = 0; i < 11; ++i) e[1 + i] = static_cast<uint8_t>(kName[i]);
    e[15] = static_cast<uint8_t>(inhalt.size() / kRecordBytes);
    for (size_t k = 0; k < bloecke.size(); ++k) {
        if (wide_ptr_) { e[16 + 2 * k] = bloecke[k] & 0xFF; e[17 + 2 * k] = bloecke[k] >> 8; }
        else e[16 + k] = static_cast<uint8_t>(bloecke[k]);
    }
    return writeDirEntry(slot, e);
}

bool CpmFileSystem::writeSystemDatei1715(const std::vector<uint8_t>& data) {
    if (data.empty() || data.size() > kMicroDosSysMax)
        return fail("MICRODOS.SYS muss 1…" + std::to_string(kMicroDosSysMax) + " Byte gross sein");
    // Der Urlader liest MICRODOS.SYS von den ERSTEN Datenbloecken (gleich hinter dem Verzeichnis); die
    // Datei ist sonst eine gewoehnliche Datei (Nutzer 0, SYS-Bit an der Endung).  Gemessen an der
    // Originaldiskette und an einer von MSYSG15A erzeugten: Bloecke dir_blocks_ ... aufsteigend.
    const uint32_t noetig = static_cast<uint32_t>((data.size() + prof_.block_size - 1) / prof_.block_size);
    if (noetig > ptrs_per_entry_) return fail("MICRODOS.SYS passt nicht in einen Verzeichnisplatz");

    std::vector<uint8_t> roh;
    if (!readDirectory(roh)) return false;
    for (int i = 0; i < prof_.dir_entries; ++i) {
        uint8_t* p = roh.data() + static_cast<size_t>(i) * 32;
        if (!bootPlatz(i, p) && istSystemDateiName(p)) {
            p[0] = 0xE5;
            if (!writeDirEntry(i, p)) return false;
        }
    }
    const std::vector<bool> karte = allocationMap();
    std::vector<uint16_t> bloecke;
    for (uint32_t k = 0; k < noetig; ++k) {
        const uint32_t b = dir_blocks_ + k;
        if (b >= total_blocks_ || karte[b])
            return fail("Block " + std::to_string(b) + " ist belegt - MICRODOS.SYS muss in die ersten Datenbloecke "
                        "gleich hinter dem Verzeichnis (der Urlader liest sie dort); die Datei dort erst entfernen");
        bloecke.push_back(static_cast<uint16_t>(b));
    }
    int slot = -1;
    for (int i = 0; i < prof_.dir_entries && slot < 0; ++i)
        if (roh[static_cast<size_t>(i) * 32] == 0xE5
            && !bootPlatz(i, roh.data() + static_cast<size_t>(i) * 32)) slot = i;
    if (slot < 0) return fail("Verzeichnis voll");

    std::vector<uint8_t> inhalt = data;
    if (inhalt.size() % kRecordBytes) inhalt.resize((inhalt.size() / kRecordBytes + 1) * kRecordBytes, 0);
    for (size_t i = 0; i < bloecke.size(); ++i) {
        std::vector<uint8_t> block(prof_.block_size, 0);
        const size_t ab = i * prof_.block_size;
        if (ab < inhalt.size())
            std::copy(inhalt.begin() + static_cast<long>(ab),
                      inhalt.begin() + static_cast<long>(std::min(inhalt.size(), ab + prof_.block_size)),
                      block.begin());
        if (!writeAt(static_cast<uint64_t>(bloecke[i]) * prof_.block_size, block.data(), block.size()))
            return false;
    }
    uint8_t e[32] = {};
    static const char kName[11] = {'M', 'I', 'C', 'R', 'O', 'D', 'O', 'S', 'S', 'Y', 'S'};
    for (int i = 0; i < 11; ++i) e[1 + i] = static_cast<uint8_t>(kName[i]);
    e[10] |= 0x80;                                  // SYS-Attribut (Bit 7 des zweiten Endungszeichens)
    e[15] = static_cast<uint8_t>(inhalt.size() / kRecordBytes);
    for (size_t k = 0; k < bloecke.size(); ++k) {
        if (wide_ptr_) { e[16 + 2 * k] = bloecke[k] & 0xFF; e[17 + 2 * k] = bloecke[k] >> 8; }
        else e[16 + k] = static_cast<uint8_t>(bloecke[k]);
    }
    return writeDirEntry(slot, e);
}

bool CpmFileSystem::wouldFit(const std::vector<PlannedFile>& files, FitReport& out) const {
    out = FitReport{};

    std::vector<bool> karte = allocationMap();
    const std::vector<CpmDirEntry> verz = directory();

    int plaetze_frei = 0;
    for (const CpmDirEntry& d : verz) if (d.free()) ++plaetze_frei;

    // Ersetzte gleichnamige Dateien geben Bloecke UND Verzeichnisplaetze zurueck.
    for (const PlannedFile& f : files) {
        int user = 0;
        std::string name;
        splitUser(f.name, user, name);
        name = upper(name);
        for (const CpmDirEntry& d : verz) {
            if (d.free() || d.user != user || upper(d.name) != name) continue;
            ++plaetze_frei;
            for (uint16_t b : d.blocks)
                if (b >= dir_blocks_ && b < total_blocks_) karte[b] = false;
        }
    }

    uint32_t frei = 0;
    for (uint32_t b = dir_blocks_; b < total_blocks_; ++b) if (!karte[b]) ++frei;

    uint32_t bloecke = 0;
    uint32_t plaetze = 0;
    for (const PlannedFile& f : files) {
        const uint64_t gerundet =
            ((f.size + kRecordBytes - 1) / kRecordBytes) * kRecordBytes;
        const uint32_t b = static_cast<uint32_t>(
            (gerundet + prof_.block_size - 1) / prof_.block_size);
        bloecke += b;
        plaetze += std::max(1u, (b + ptrs_per_entry_ - 1) / ptrs_per_entry_);
    }

    out.needed     = static_cast<uint64_t>(bloecke) * prof_.block_size;
    out.available  = static_cast<uint64_t>(frei)    * prof_.block_size;
    out.dir_needed = static_cast<int>(plaetze);
    out.dir_free   = plaetze_frei;
    out.fits       = (bloecke <= frei) && (static_cast<int>(plaetze) <= plaetze_frei);

    if (bloecke > frei)
        out.detail = std::to_string(files.size()) + " Dateien brauchen "
                   + std::to_string(bloecke) + " Bloecke (" + std::to_string(out.needed / 1024)
                   + " KB), frei sind " + std::to_string(frei) + " ("
                   + std::to_string(out.available / 1024) + " KB)";
    else if (static_cast<int>(plaetze) > plaetze_frei)
        out.detail = "Verzeichnis reicht nicht: " + std::to_string(plaetze)
                   + " Eintraege noetig, " + std::to_string(plaetze_frei) + " frei";
    return true;
}

bool CpmFileSystem::mkfs() {
    // Ein leeres CP/M-Dateisystem ist schlicht ein Verzeichnis voller 0xE5 — der
    // Datenbereich dahinter bleibt unberuehrt (so macht es auch CP/M selbst).
    const std::vector<uint8_t> leer(static_cast<size_t>(prof_.dir_entries) * 32, 0xE5);
    return writeAt(0, leer.data(), leer.size());
}

// ─── Zustand ─────────────────────────────────────────────────────────────────

std::vector<bool> CpmFileSystem::allocationMap(bool mitSystembereich) const {
    std::vector<bool> karte(total_blocks_, false);
    for (uint32_t b = 0; b < dir_blocks_; ++b) karte[b] = true;   // Verzeichnis
    for (const CpmDirEntry& d : directory()) {
        if (d.free() || d.user > 15) continue;
        for (uint16_t b : d.blocks)
            if (b != 0 && b < total_blocks_) karte[b] = true;
    }
    // MicroDOS-Systemdiskette mit 128-Byte-Sektoren (msysgen Anhang B): das System liegt in den linearen
    // Sektoren 26..155 (Spur 1-5 ganz), also in den Bloecken 3..19 zu je 8 Sektoren.  MSYSG20 traegt
    // aber nur 16 Bloecke ein und laesst Block 18 aus (gemessen an einer vom Gast erzeugten Diskette):
    // ein `put` koennte ihn vergeben und das System zerstoeren.  Darum gilt der ganze Bereich als belegt.
    if (mitSystembereich && prof_.boot_header == "microdos" && sector_size_ == 128)
        for (uint32_t b = 3; b <= 19 && b < total_blocks_; ++b) karte[b] = true;
    return karte;
}

FsInfo CpmFileSystem::info() const {
    FsInfo i;
    const std::vector<bool> karte = allocationMap();
    uint32_t belegt = 0;
    for (bool b : karte) if (b) ++belegt;

    i.total_bytes = static_cast<uint64_t>(total_blocks_ - dir_blocks_) * prof_.block_size;
    i.used_bytes  = static_cast<uint64_t>(belegt - dir_blocks_) * prof_.block_size;
    i.free_bytes  = static_cast<uint64_t>(total_blocks_ - belegt) * prof_.block_size;
    i.files       = static_cast<int>(list().size());
    // Reine Startdiskette: der Bereich hinter den Systemspuren ist fuer das Geraet kein
    // Dateibereich (FsProfile::nur_boot) — „111 KB frei“ waere eine falsche Auskunft.
    if (prof_.nur_boot) i.total_bytes = i.free_bytes = 0;

    // Blockzeiger, die hinter den Datenbereich zeigen, sind ein sicheres Zeichen fuer
    // ein falsch gewaehltes Profil — genau deshalb gehoeren sie in den Bericht.
    int wild = 0;
    for (const CpmDirEntry& d : directory()) {
        if (d.free() || d.user > 15) continue;
        for (uint16_t b : d.blocks) if (b >= total_blocks_) ++wild;
    }
    if (wild)
        i.warnings.push_back(std::to_string(wild) + " Blockzeiger zeigen hinter den "
                             "Datenbereich - vermutlich das falsche Dateisystemprofil");
    return i;
}

// ─── Reparatur (doc/design/15_dateisystempruefung.md §8) ─────────────────────
//
// Gefunden wird ausserhalb, repariert wird innen (E3).  Alle vier Eingriffe fassen
// genau EINEN Verzeichnisplatz an — bei CP/M steht die ganze Verwaltung dort; einen
// Belegungsplan gibt es nicht (er wird aus dem Verzeichnis gerechnet), und damit
// heilt sich die Belegung von selbst, sobald das Verzeichnis stimmt.
// Parametertabelle: `cpm_fs.h` am `repair`-Haken.

bool CpmFileSystem::repair(const FsRepair& r) {
    if (r.gesperrt)
        return fail("Die Reparatur '" + r.kind + "' ist gesperrt: " + r.warum);

    const bool platzsache = r.kind == "cpm.platz.freigeben"
                         || r.kind == "cpm.zeiger.streichen"
                         || r.kind == "cpm.rc.anpassen"
                         || r.kind == "cpm.kreuz.erstem_lassen";
    if (!platzsache) return FileSystem::repair(r);

    std::vector<uint8_t> roh;
    if (!directoryRaw(roh)) return false;
    if (r.a < 0 || static_cast<size_t>(r.a) * 32 + 32 > roh.size())
        return fail("Verzeichnisplatz " + std::to_string(r.a) + " gibt es nicht");
    uint8_t* p = roh.data() + static_cast<size_t>(r.a) * 32;

    if (r.kind == "cpm.platz.freigeben") {
        // NUR das Nutzerbyte.  Die uebrigen 31 Byte bleiben stehen — genau so
        // loescht CP/M selbst, und genau deshalb ist der Eintrag spaeter noch
        // wiederherstellbar (§13).
        p[0] = 0xE5;
        return writeDirEntry(r.a, p);
    }

    // Blockzeiger lesen und schreiben — 8 oder 16 Bit breit, je nach Blockzahl.
    const int n = ptrs_per_entry_;
    auto zeiger = [&](int k) -> uint16_t {
        return wide_ptr_ ? static_cast<uint16_t>(p[16 + 2 * k] | (p[17 + 2 * k] << 8))
                         : p[16 + k];
    };
    auto setze = [&](int k, uint16_t v) {
        if (wide_ptr_) { p[16 + 2 * k] = static_cast<uint8_t>(v & 0xFF);
                         p[17 + 2 * k] = static_cast<uint8_t>(v >> 8); }
        else             p[16 + k] = static_cast<uint8_t>(v);
    };
    // Wie viele 128-B-Saetze traegt ein Block?  Danach richtet sich RC.
    const uint32_t saetze_je_block = std::max<uint32_t>(1u, prof_.block_size / 128);

    if (r.kind == "cpm.zeiger.streichen") {
        int gestrichen = 0;
        for (int k = 0; k < n; ++k) {
            const uint16_t blk = zeiger(k);
            if (blk == 0) continue;
            if (blk < total_blocks_ && blk >= dir_blocks_) continue;
            setze(k, 0);
            ++gestrichen;
        }
        if (!gestrichen) return fail("Dieser Platz traegt keinen unbrauchbaren Blockzeiger "
                                     "mehr");
    } else if (r.kind == "cpm.kreuz.erstem_lassen") {
        int gestrichen = 0;
        for (int k = 0; k < n; ++k)
            if (zeiger(k) == static_cast<uint16_t>(r.b)) { setze(k, 0); ++gestrichen; }
        if (!gestrichen) return fail("Dieser Platz beansprucht Block "
                                     + std::to_string(r.b) + " nicht mehr");
    }

    // Kein LOCH hinterlassen.  Ein genullter Zeiger MITTEN in der Liste ist bei
    // CP/M kein „Block fehlt", sondern verschiebt jeden folgenden Satz an eine
    // Stelle, an der er nie stand — der Extent wuerde falsche Daten ausliefern,
    // ohne dass es jemand merkt (und die Pruefung meldete danach prompt
    // `cpm.block.luecke`).  Deshalb wird ab dem ersten Nullzeiger ABGESCHNITTEN:
    // was davor steht, gehoert der Datei wirklich; was dahinter stand, ist
    // ohnehin nur ueber den gestrichenen Zeiger erreichbar gewesen.  Bei einem
    // unversehrten Eintrag greift die Schleife nicht — dort stehen hinter der
    // ersten Null nur weitere Nullen.
    if (r.kind != "cpm.rc.anpassen") {
        bool abgeschnitten = false;
        for (int k = 0; k < n; ++k) {
            if (abgeschnitten) setze(k, 0);
            else if (zeiger(k) == 0) abgeschnitten = true;
        }
    }

    // RC nachziehen: mehr Saetze, als die verbliebenen Bloecke tragen, gibt es nicht.
    if (r.kind == "cpm.rc.anpassen") {
        if (r.b < 0 || r.b > 128) return fail("RC muss zwischen 0 und 128 liegen");
        p[15] = static_cast<uint8_t>(r.b);
    } else {
        uint32_t belegt = 0;
        for (int k = 0; k < n; ++k) if (zeiger(k) != 0) ++belegt;
        const uint32_t moeglich = std::min<uint32_t>(128u, belegt * saetze_je_block);
        if (p[15] > moeglich) p[15] = static_cast<uint8_t>(moeglich);
    }
    return writeDirEntry(r.a, p);
}
