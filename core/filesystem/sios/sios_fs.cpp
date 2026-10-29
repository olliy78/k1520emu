/**
 * @file sios_fs.cpp
 * @brief SIOS 1526 (lesend) — siehe sios_fs.h und doc/sios_diskettenformat.md.
 */

#include "core/filesystem/sios/sios_fs.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

constexpr int kErsterEtikettenSektor = 8;    ///< Spur 0: Sektoren 8..26 tragen HDR1/DDR1
constexpr int kVol1Sektor  = 7;
constexpr int kErmapSektor = 5;
/// @brief Nutzbare Spuren laut Handbuch: Spur 0 + 1..37 (die beiden aeusseren sind Reserve).
constexpr int kNutzSpuren = 38;

/// @brief Bibliotheksverzeichnis: der Datenteil beginnt bei AADR 19 (18 Sektoren Verzeichnisbereich).
constexpr int kBibDatenAb = 19;

std::string rtrim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    return s;
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string dkoiText(const std::vector<uint8_t>& d, size_t von, size_t bis) {
    std::string s;
    for (size_t i = von; i < bis && i < d.size(); ++i) s += SiosFileSystem::dkoi(d[i]);
    return s;
}

/// @brief "TTSSS" -> linearer Sektorindex; -1 = keine Adresse (Sektor muss 1..26 sein).
int adresse(const std::string& t) {
    if (t.size() != 5) return -1;
    for (char c : t) if (c < '0' || c > '9') return -1;
    const int tt = (t[0] - '0') * 10 + (t[1] - '0');
    const int ss = (t[2] - '0') * 100 + (t[3] - '0') * 10 + (t[4] - '0');
    if (ss < 1 || ss > kSiosSektoren) return -1;
    return tt * kSiosSektoren + (ss - 1);
}

/// @brief Dezimalfeld mit fuehrenden Blanks/Nullen; -1 = keine Zahl.
int dezimal(const std::string& t) {
    int v = 0;
    bool da = false;
    for (char c : t) {
        if (c == ' ') { if (da) return -1; continue; }
        if (c < '0' || c > '9') return -1;
        v = v * 10 + (c - '0');
        da = true;
    }
    return da ? v : -1;
}

}  // namespace

const char* SiosFileSystem::schreibsperre() {
    return "SIOS: schreibender Zugriff noch nicht unterstuetzt (Arbeitspaket D4) - "
           "die Diskette ist nur lesbar";
}

char SiosFileSystem::dkoi(uint8_t b) {
    // DKOI = EBCDIC mit fuenf Abweichungen (Formatdokument §2).
    switch (b) {
        case 0x40: return ' ';
        case 0x4A: return '[';    // EBCDIC: Cent
        case 0x4F: return '!';    // EBCDIC: senkrechter Strich
        case 0x5A: return ']';    // EBCDIC: !
        case 0x5F: return '^';    // EBCDIC: Negation
        case 0x6A: return '/';    // EBCDIC: unterbrochener Strich
        case 0x4B: return '.';  case 0x4C: return '<';  case 0x4D: return '(';
        case 0x4E: return '+';  case 0x50: return '&';  case 0x5B: return '$';
        case 0x5C: return '*';  case 0x5D: return ')';  case 0x5E: return ';';
        case 0x60: return '-';  case 0x61: return '/';  case 0x6B: return ',';
        case 0x6C: return '%';  case 0x6D: return '_';  case 0x6E: return '>';
        case 0x6F: return '?';  case 0x7A: return ':';  case 0x7B: return '#';
        case 0x7C: return '@';  case 0x7D: return '\''; case 0x7E: return '=';
        case 0x7F: return '"';
        default: break;
    }
    if (b >= 0xC1 && b <= 0xC9) return static_cast<char>('A' + (b - 0xC1));
    if (b >= 0xD1 && b <= 0xD9) return static_cast<char>('J' + (b - 0xD1));
    if (b >= 0xE2 && b <= 0xE9) return static_cast<char>('S' + (b - 0xE2));
    if (b >= 0xF0 && b <= 0xF9) return static_cast<char>('0' + (b - 0xF0));
    return '?';
}

int SiosEtikett::anzahl() const {
    if (boe < 0 || eoe < boe) return 0;
    if (eod >= boe && eod <= eoe + 1) return eod - boe;
    return eoe - boe + 1;
}

// ─── Erkennung / Mounten ─────────────────────────────────────────────────────

bool SiosFileSystem::looksLikeSios(const SectorSpace& space, std::string* why) {
    auto nein = [&](const std::string& w) { if (why) *why = w; return false; };
    if (space.sectorsPerTrack(0, 0) != kSiosSektoren || space.sectorSize(0, 0) != kSiosSektor)
        return nein("Spur 0 ist nicht 26 x 128 B");
    SectorData ermap, vol1;
    if (!space.readSector(0, 0, kErmapSektor, ermap) || ermap.data.size() != kSiosSektor)
        return nein("Spur 0 Sektor 5 nicht lesbar");
    if (dkoiText(ermap.data, 0, 5) != "ERMAP") return nein("Spur 0 Sektor 5 traegt kein ERMAP");
    if (!space.readSector(0, 0, kVol1Sektor, vol1) || vol1.data.size() != kSiosSektor)
        return nein("Spur 0 Sektor 7 nicht lesbar");
    if (dkoiText(vol1.data, 0, 4) != "VOL1") return nein("Spur 0 Sektor 7 traegt kein VOL1");
    return true;
}

std::unique_ptr<SiosFileSystem> SiosFileSystem::mount(SectorSpace& space, const FsProfile& prof,
                                                      std::string& err) {
    std::string warum;
    if (!looksLikeSios(space, &warum)) { err = "keine SIOS-Diskette: " + warum; return nullptr; }
    std::unique_ptr<SiosFileSystem> fs(new SiosFileSystem(space, prof));
    SectorData v;
    space.readSector(0, 0, kVol1Sektor, v);
    fs->label_ = rtrim(dkoiText(v.data, 4, 10));
    int hoechster = 0;
    for (size_t i = 0; i < space.trackCount(); ++i)
        hoechster = std::max<int>(hoechster, space.trackAt(i).cyl + 1);
    fs->tracks_ = hoechster;
    return fs;
}

// ─── Etiketten ───────────────────────────────────────────────────────────────

std::string SiosFileSystem::etikettenText(int sektor) const {
    SectorData s;
    if (!space_.readSector(0, 0, static_cast<uint8_t>(sektor), s) || s.data.size() != kSiosSektor)
        return {};
    return dkoiText(s.data, 0, 80);
}

std::vector<SiosEtikett> SiosFileSystem::etiketten() const {
    std::vector<SiosEtikett> out;
    for (int sek = kErsterEtikettenSektor; sek <= kSiosSektoren; ++sek) {
        SectorData s;
        if (!space_.readSector(0, 0, static_cast<uint8_t>(sek), s) || s.data.size() != kSiosSektor)
            continue;
        // Aktiv ist nur ein Satz mit HDR1 (die gelöschte Datenmarke F8 ist doppelte Kodierung
        // desselben Zustands und wird nicht ausgewertet, Formatdokument §4.4).
        if (dkoiText(s.data, 0, 4) != "HDR1") continue;

        SiosEtikett e;
        e.sektor = sek;
        e.name   = rtrim(dkoiText(s.data, 5, 22));
        const int sl = dezimal(dkoiText(s.data, 22, 27));
        e.satzlaenge = sl < 0 ? 0 : static_cast<uint32_t>(sl);
        e.boe_text = dkoiText(s.data, 28, 33);
        e.eoe_text = dkoiText(s.data, 34, 39);
        e.eod_text = dkoiText(s.data, 74, 79);
        e.schutz      = dkoiText(s.data, 42, 43)[0];
        e.austausch   = dkoiText(s.data, 43, 44)[0];
        e.fortsetzung = dkoiText(s.data, 44, 45)[0];
        e.folge       = rtrim(dkoiText(s.data, 45, 47));
        e.boe = adresse(e.boe_text);
        e.eoe = adresse(e.eoe_text);
        e.eod = adresse(e.eod_text);

        if (e.boe < 0 || e.eoe < 0) {
            e.gueltig = false;
            e.problem = "BOE '" + e.boe_text + "' / EOE '" + e.eoe_text + "' sind keine Adressen";
        } else if (e.boe < kSiosSektoren) {
            e.gueltig = false;
            e.problem = "BOE " + e.boe_text + " liegt in der Etikettenspur 0";
        } else if (e.eoe < e.boe) {
            e.gueltig = false;
            e.problem = "EOE " + e.eoe_text + " liegt vor BOE " + e.boe_text;
        } else if (e.eoe / kSiosSektoren >= tracks_) {
            e.gueltig = false;
            e.problem = "EOE " + e.eoe_text + " liegt hinter der letzten Spur der Diskette";
        } else if (e.eod < 0) {
            e.problem = "EOD '" + e.eod_text + "' ist keine Adresse - die Datei reicht bis EOE";
        } else if (e.eod < e.boe || e.eod > e.eoe + 1) {
            e.problem = "EOD " + e.eod_text + " liegt ausserhalb des Bereichs " + e.boe_text
                      + ".." + e.eoe_text + " - die Datei reicht bis EOE";
            e.eod = -1;
        }
        out.push_back(std::move(e));
    }
    return out;
}

bool SiosFileSystem::sektorLesen(int linear, std::vector<uint8_t>& out) const {
    if (linear < 0) return false;
    const int cyl = linear / kSiosSektoren;
    const int id  = linear % kSiosSektoren + 1;
    if (cyl >= tracks_) return false;
    SectorData s;
    if (!space_.readSector(static_cast<uint8_t>(cyl), 0, static_cast<uint8_t>(id), s)
        || s.data.size() != kSiosSektor)
        return false;
    out = std::move(s.data);
    return true;
}

// ─── Bibliotheken ────────────────────────────────────────────────────────────

bool SiosFileSystem::bibliothek(const SiosEtikett& lib, std::vector<SiosObjekt>& out, int* nd_aus,
                                std::string* warum) const {
    auto nein = [&](const std::string& w) { if (warum) *warum = w; return false; };
    out.clear();
    const int anz = lib.anzahl();
    if (!lib.gueltig || anz < kBibDatenAb) return nein("zu klein fuer eine Bibliothek");
    std::vector<uint8_t> d;
    if (!sektorLesen(lib.boe + 1, d)) return nein("Verzeichnissektor (AADR 2) nicht lesbar");

    // Fuss: Slot 16 (Offset 120..127): +2 = ND (LE).  Ausserhalb des Bereichs -> Ende der Datei.
    int nd = d[122] | (d[123] << 8);
    if (nd < kBibDatenAb || nd > anz + 1) nd = anz + 1;
    if (nd_aus) *nd_aus = nd;

    std::vector<SiosObjekt> roh;
    for (int slot = 0; slot < 15; ++slot) {
        const uint8_t* p = &d[static_cast<size_t>(slot) * 8];
        if (std::all_of(p, p + 8, [](uint8_t b) { return b == 0x03; })) continue;   // unbenutzt
        SiosObjekt o;
        bool druckbar = true;
        for (int i = 0; i < 5; ++i) {
            if (p[i] == 0) break;
            if (p[i] < 0x20 || p[i] > 0x7E) druckbar = false;
            o.name += static_cast<char>(p[i]);
        }
        o.name = rtrim(o.name);
        o.aadr = p[5] | (p[6] << 8);
        o.ml   = p[7];
        // Ein Slot, dessen Adresse ausserhalb des Datenteils liegt, ist kein Eintrag.
        if (!druckbar || o.aadr < kBibDatenAb || o.aadr >= nd) continue;
        roh.push_back(std::move(o));
    }
    // Objektlaenge = Abstand zum naechsten AADR (geloeschte Eintraege behalten Adresse und ML),
    // beim letzten zu ND (§7.3).
    std::vector<int> grenzen;
    for (const SiosObjekt& o : roh) grenzen.push_back(o.aadr);
    grenzen.push_back(nd);
    std::sort(grenzen.begin(), grenzen.end());
    for (SiosObjekt& o : roh) {
        const auto it = std::upper_bound(grenzen.begin(), grenzen.end(), o.aadr);
        o.laenge = it == grenzen.end() ? 0 : *it - o.aadr;
    }
    out = std::move(roh);
    return true;
}

// ─── Verzeichnis ─────────────────────────────────────────────────────────────

std::vector<SiosFileSystem::Eintrag> SiosFileSystem::eintraege() const {
    std::vector<Eintrag> out;
    for (const SiosEtikett& e : etiketten()) {
        Eintrag x;
        FileEntry& fe = x.fe;
        fe.name   = e.name;
        fe.record_len = static_cast<uint16_t>(std::min<uint32_t>(e.satzlaenge, 0xFFFF));
        fe.damaged = !e.gueltig;
        fe.attributes.clear();
        if (e.schutz != ' ')      fe.attributes += e.schutz;
        if (e.austausch != ' ')   fe.attributes += e.austausch;
        if (e.fortsetzung != ' ') fe.attributes += e.fortsetzung;
        fe.extent = "BOE " + e.boe_text + " EOE " + e.eoe_text
                  + (e.eod < 0 ? "" : " EOD " + e.eod_text);
        x.erster = e.gueltig ? e.boe : 0;
        x.anzahl = e.gueltig ? e.anzahl() : 0;
        fe.size  = static_cast<uint64_t>(x.anzahl) * kSiosSektor;

        std::vector<SiosObjekt> objekte;
        const bool lib = e.name.size() > 2 && e.name.compare(e.name.size() - 2, 2, "LB") == 0
                         && e.satzlaenge == kSiosSektor && e.gueltig
                         && bibliothek(e, objekte);
        fe.type = lib ? "B" : "F";
        out.push_back(x);
        if (!lib) continue;
        for (const SiosObjekt& o : objekte) {
            if (o.name.empty()) continue;           // geloescht: Name = Blank
            Eintrag y;
            char kurz[32];
            std::snprintf(kurz, sizeof kurz, "AADR %04X ML %02X", o.aadr, o.ml);
            y.fe.name = e.name + "/" + o.name;
            y.fe.type = "O";
            y.fe.record_len = static_cast<uint16_t>(kSiosSektor);
            y.fe.extent = kurz;
            y.erster = x.erster + (o.aadr - 1);
            y.anzahl = o.laenge;
            y.fe.size = static_cast<uint64_t>(o.laenge) * kSiosSektor;
            out.push_back(std::move(y));
        }
    }
    return out;
}

std::vector<FileEntry> SiosFileSystem::list() const {
    std::vector<FileEntry> out;
    for (Eintrag& e : eintraege()) out.push_back(std::move(e.fe));
    return out;
}

bool SiosFileSystem::findEintrag(const std::string& name, Eintrag& out) const {
    const std::string gesucht = upper(name);
    for (Eintrag& e : eintraege()) {
        if (upper(e.fe.name) == gesucht) { out = std::move(e); return true; }
    }
    return false;
}

bool SiosFileSystem::firstSector(const std::string& name, FsRecoverOrt& out) const {
    Eintrag e;
    if (!findEintrag(name, e) || e.anzahl <= 0) return false;
    out.cyl    = e.erster / kSiosSektoren;
    out.head   = 0;
    out.sector = e.erster % kSiosSektoren + 1;
    return true;
}

bool SiosFileSystem::read(const std::string& name, std::vector<uint8_t>& out) {
    Eintrag e;
    if (!findEintrag(name, e)) return fail("Datei nicht gefunden: " + name);
    if (e.fe.damaged) return fail("Das Etikett von '" + name + "' ist beschaedigt (" + e.fe.extent + ")");
    out.clear();
    out.reserve(static_cast<size_t>(e.anzahl) * kSiosSektor);
    for (int i = 0; i < e.anzahl; ++i) {
        std::vector<uint8_t> d;
        if (!sektorLesen(e.erster + i, d)) {
            out.clear();
            const int lin = e.erster + i;
            return fail("Spur " + std::to_string(lin / kSiosSektoren) + " Sektor "
                        + std::to_string(lin % kSiosSektoren + 1) + " von '" + name
                        + "' ist nicht lesbar");
        }
        out.insert(out.end(), d.begin(), d.end());
    }
    return true;
}

// ─── Schreiben: noch nicht (D4) ──────────────────────────────────────────────

bool SiosFileSystem::write(const std::string&, const std::vector<uint8_t>&, const WriteOptions&) {
    return fail(schreibsperre());
}

bool SiosFileSystem::erase(const std::string&) { return fail(schreibsperre()); }

bool SiosFileSystem::mkfs() { return fail(schreibsperre()); }

bool SiosFileSystem::wouldFit(const std::vector<PlannedFile>&, FitReport& out) const {
    out = FitReport{};
    out.fits   = false;
    out.detail = schreibsperre();
    return true;          // die Auskunft "passt nicht" ist ein Ergebnis, kein Fehler
}

FsInfo SiosFileSystem::info() const {
    FsInfo i;
    i.label = label_;
    const int nutz = std::min(tracks_, kNutzSpuren);
    i.total_bytes = nutz > 1 ? static_cast<uint64_t>(nutz - 1) * kSiosSektoren * kSiosSektor : 0;
    // Belegt = die vorab reservierten Bereiche [BOE, EOE] (§4.2), ueberlappungsfrei gezaehlt.
    std::vector<bool> belegt(static_cast<size_t>(std::max(tracks_, 1)) * kSiosSektoren, false);
    int dateien = 0;
    for (const SiosEtikett& e : etiketten()) {
        ++dateien;
        if (!e.gueltig) continue;
        for (int s = e.boe; s <= e.eoe && s < static_cast<int>(belegt.size()); ++s) belegt[static_cast<size_t>(s)] = true;
    }
    uint64_t n = 0;
    for (size_t s = kSiosSektoren; s < belegt.size(); ++s) if (belegt[s]) ++n;
    i.used_bytes = std::min<uint64_t>(n * kSiosSektor, i.total_bytes);
    i.free_bytes = i.total_bytes - i.used_bytes;
    i.files = dateien;
    return i;
}
