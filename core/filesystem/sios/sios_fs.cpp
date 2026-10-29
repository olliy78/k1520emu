/**
 * @file sios_fs.cpp
 * @brief SIOS 1526 — siehe sios_fs.h und doc/sios_diskettenformat.md.
 */

#include "core/filesystem/sios/sios_fs.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>

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
        // Aktiv ist nur ein Satz mit HDR1 (die geloeschte Datenmarke F8 ist doppelte Kodierung
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

// ─── Schreiben (Arbeitspaket D4) ─────────────────────────────────────────────
//
// Grundsatz: erst PLANEN und urteilen (`plane`, nur Lesezugriffe), dann ausfuehren — Nutzdaten
// zuerst, der Verwaltungssatz (HDR1 bzw. Verzeichnissektor) zuletzt.  Bricht ein Schreibzugriff
// ab, ist hoechstens ein noch nicht eingetragener Datenbereich beschrieben; die Diskette bleibt
// formal in Ordnung.  Stapel (`insertAll`) sichern wie bei allen Dateisystemen das ganze Medium
// und rollen darueber zurueck.
//
// Bewusst NICHT umgesetzt (nicht belegt, Formatdokument §11): Fehlspurkarte (ERMAP) mit
// Eintraegen, Satzlaenge <> 128 (eine Linux-Datei sagt sie nicht), Fortsetzung des
// Bibliotheksverzeichnisses ueber einen Sektor hinaus, Verdichten (COPY) einer Bibliothek.

namespace {

/// @brief Hoechstzahl benutzter Verzeichnisplaetze einer Bibliothek, die wir selbst belegen.
/// Bis 14 Plaetze gilt NV = 0200H + 8 * Plaetze (DP2 13, SG1 3, Handbuch 9); DP1 traegt bei 15
/// Plaetzen NV = 0300H statt 0278H.  Den 15. Platz zu fuellen hiesse einen unbekannten NV-Wert
/// zu raten, den LBSV vermutlich als naechsten freien Platz nimmt.
constexpr int kBibSlotsMax = 14;

/// @brief ML (Byte 7 des Verzeichniseintrags) fuer ein Objekt von @p sektoren Sektoren.
/// ANNAHME (Formatdokument §7.3/§11 Nr. 6, nicht am Systemcode belegt): ML ist die Speicherlaenge
/// in 256-B-Seiten, und in allen 31 gemessenen Eintraegen gilt len >= 2*ML (22 mal gleich).
/// Gewaehlt wird deshalb ML = len / 2 (abgerundet, mindestens 1) - der groesste Wert, der zu
/// allen Messwerten passt.  Die Phasenstruktur des Objekts wird nicht ausgewertet.
int mlFuer(int sektoren) { return std::max(1, sektoren / 2); }

std::string zahl2(int v) { char b[16]; std::snprintf(b, sizeof b, "%02d", v); return b; }
std::string adr(int linear) {
    char b[16];
    std::snprintf(b, sizeof b, "%02d%03d", linear / kSiosSektoren, linear % kSiosSektoren + 1);
    return b;
}

/// @brief 80 Zeichen Etikettentext -> 128-Byte-Sektor (DKOI + 48 x 00).
std::vector<uint8_t> etikettBytes(const std::string& text) {
    std::vector<uint8_t> d(kSiosSektor, 0x00);
    for (size_t i = 0; i < 80; ++i) {
        uint8_t b = 0x40;
        if (i < text.size() && !SiosFileSystem::dkoiKodieren(text[i], b)) b = 0x40;
        d[i] = b;
    }
    return d;
}

/// @brief HDR1-Text (80 Zeichen) einer neuen Datei: Felder und Lage wie bei PHASELB der
/// Datendisketten (Formatdokument §4.1): Satzlaenge 128, Austauschkennzeichen `E`.
std::string hdr1Text(const std::string& name, int boe_spur, int eoe_spur, int eod_linear) {
    std::string t(80, ' ');
    auto put = [&](size_t pos1, const std::string& w) { t.replace(pos1 - 1, w.size(), w); };
    put(1, "HDR1");
    put(6, name);
    put(23, "00128");
    put(29, zahl2(boe_spur) + "001");
    put(35, zahl2(eoe_spur) + "026");
    put(44, "E");
    put(75, adr(eod_linear));
    return t;
}

}  // namespace

bool SiosFileSystem::dkoiKodieren(char c, uint8_t& out) {
    for (int b = 0x40; b <= 0xF9; ++b) {
        const char d = dkoi(static_cast<uint8_t>(b));
        if (d == '?' && b != 0x6F) continue;        // "nicht belegt", kein echtes Fragezeichen
        if (d == c) { out = static_cast<uint8_t>(b); return true; }
    }
    return false;
}

int SiosFileSystem::letzteNutzSpur() const { return std::min(tracks_, kNutzSpuren) - 1; }

struct SiosFileSystem::Bib {
    SiosEtikett          e;
    std::vector<uint8_t> dir;        ///< Verzeichnissektor (AADR 2)
    int  nd = 0;                     ///< Fuss: erster freier AADR
    int  gesamt = 0;                 ///< Fuss: Groesse der Bibliothek in Sektoren
    int  belegt = 0;                 ///< benutzte Plaetze (geloeschte mitgezaehlt)
    bool nv_passt = false;           ///< NV = 0200H + 8 * belegt (die belegte Regel)
};

struct SiosFileSystem::Belegung {
    std::vector<SiosEtikett> et;                 ///< aktive Etiketten in Sicht der Planung
    std::vector<bool>        spur;               ///< je Spur belegt (0 und die Reserve gelten als belegt)
    bool                     etikett_frei[kSiosSektoren + 1] = {};   ///< Etikettensektoren 8..26
    std::map<std::string, Bib> bibs;             ///< geladene Bibliotheken (Name in Grossbuchstaben)
};

struct SiosFileSystem::Aufgabe {
    bool        objekt = false;
    int         sektoren = 0;
    // physische Datei
    int         boe_spur = 0, spuren = 0, etikett = 0, alt_etikett = 0;   // alt_etikett 0 = keins
    std::string text;                 ///< HDR1 (80 Zeichen)
    // Bibliotheksobjekt
    int         lib_boe = 0;          ///< linearer Index des ersten Bibliothekssektors
    int         aadr = 0;
    std::vector<uint8_t> dir_neu;
};

bool SiosFileSystem::belegungLesen(Belegung& b, std::string& grund) const {
    b.et = etiketten();
    for (const SiosEtikett& e : b.et)
        if (!e.gueltig) {
            grund = "das Etikett in Sektor " + std::to_string(e.sektor) + " ist beschaedigt ("
                  + e.problem + ") - erst die Dateisystempruefung (fsck) ansehen";
            return false;
        }
    // Fehlspurkarte: ihr Eintragsformat ist unbelegt (§3.1/§11 Nr. 1).  Steht dort etwas, kann
    // eine Spur als defekt vermerkt sein, die wir sonst belegen wuerden - also nichts schreiben.
    SectorData er;
    if (!space_.readSector(0, 0, kErmapSektor, er) || er.data.size() != kSiosSektor) {
        grund = "die Fehlspurkarte (Spur 0 Sektor 5) ist nicht lesbar";
        return false;
    }
    for (size_t i = 5; i < 80; ++i)
        if (er.data[i] != 0x40) {
            grund = "die Fehlspurkarte (ERMAP) traegt Eintraege - ihr Format ist nicht belegt, "
                    "deshalb wird auf diese Diskette nicht geschrieben";
            return false;
        }
    b.spur.assign(static_cast<size_t>(tracks_), true);
    for (int t = 1; t <= letzteNutzSpur(); ++t) b.spur[static_cast<size_t>(t)] = false;
    for (const SiosEtikett& e : b.et)
        for (int t = e.boe / kSiosSektoren; t <= e.eoe / kSiosSektoren && t < tracks_; ++t)
            b.spur[static_cast<size_t>(t)] = true;
    for (int s = kErsterEtikettenSektor; s <= kSiosSektoren; ++s) b.etikett_frei[s] = true;
    for (const SiosEtikett& e : b.et) b.etikett_frei[e.sektor] = false;
    return true;
}

bool SiosFileSystem::bibLaden(const SiosEtikett& e, Bib& out, std::string& grund) const {
    const std::string was = "'" + e.name + "'";
    if (e.name.size() < 3 || e.name.compare(e.name.size() - 2, 2, "LB") != 0
        || e.satzlaenge != kSiosSektor) {
        grund = was + " ist keine Bibliothek (Name endet nicht auf LB oder Satzlaenge <> 128)";
        return false;
    }
    if (!e.gueltig || e.eoe - e.boe + 1 < kBibDatenAb) {
        grund = was + " ist zu klein fuer eine Bibliothek";
        return false;
    }
    out = Bib{};
    out.e = e;
    if (!sektorLesen(e.boe + 1, out.dir)) {
        grund = "das Verzeichnis (AADR 2) von " + was + " ist nicht lesbar";
        return false;
    }
    const std::vector<uint8_t>& d = out.dir;
    out.nd     = d[122] | (d[123] << 8);
    out.gesamt = d[124] | (d[125] << 8);
    if (d[126] != 0x03 || d[127] != 0x03 || out.gesamt < kBibDatenAb
        || out.gesamt > e.eoe - e.boe + 1 || out.nd < kBibDatenAb || out.nd > out.gesamt + 1) {
        grund = "der Verzeichnisfuss von " + was + " ist nicht plausibel (ND "
              + std::to_string(out.nd) + ", Groesse " + std::to_string(out.gesamt) + ")";
        return false;
    }
    int erster_frei = -1;
    for (int slot = 0; slot < 15; ++slot) {
        const uint8_t* p = &d[static_cast<size_t>(slot) * 8];
        const bool frei = std::all_of(p, p + 8, [](uint8_t x) { return x == 0x03; });
        if (frei) { if (erster_frei < 0) erster_frei = slot; continue; }
        if (erster_frei >= 0) {
            grund = "das Verzeichnis von " + was + " ist nicht lueckenlos belegt (Platz "
                  + std::to_string(slot + 1) + " nach einem freien)";
            return false;
        }
        bool druckbar = true;
        for (int i = 0; i < 5 && p[i] != 0; ++i) if (p[i] < 0x20 || p[i] > 0x7E) druckbar = false;
        const int aadr = p[5] | (p[6] << 8);
        if (!druckbar || aadr < kBibDatenAb || aadr >= out.nd) {
            grund = "Verzeichnisplatz " + std::to_string(slot + 1) + " von " + was
                  + " ist nicht plausibel";
            return false;
        }
    }
    out.belegt = erster_frei < 0 ? 15 : erster_frei;
    out.nv_passt = ((d[120] << 8) | d[121]) == 0x200 + 8 * out.belegt;
    return true;
}

bool SiosFileSystem::plane(Belegung& b, const std::string& rohname, uint64_t bytes,
                           bool ueberschreiben, Aufgabe& a, std::string& grund) const {
    a = Aufgabe{};
    const uint64_t sektoren64 = (bytes + kSiosSektor - 1) / kSiosSektor;
    if (sektoren64 > 100000) { grund = "die Datei ist viel zu gross"; return false; }
    a.sektoren = static_cast<int>(sektoren64);
    const size_t schraeg = rohname.find('/');

    // ── Objekt einer Bibliothek: LIB/NAME ─────────────────────────────────────
    if (schraeg != std::string::npos) {
        const std::string lib = upper(rohname.substr(0, schraeg));
        const std::string obj = upper(rohname.substr(schraeg + 1));
        if (obj.empty() || obj.size() > 5 || obj.find('/') != std::string::npos
            || std::any_of(obj.begin(), obj.end(), [](char c) { return c <= 0x20 || c > 0x7E; })) {
            grund = "Objektname '" + obj + "' ungueltig: 1 bis 5 sichtbare ASCII-Zeichen";
            return false;
        }
        if (a.sektoren < 1) { grund = "ein leeres Objekt laesst sich nicht ablegen"; return false; }
        const SiosEtikett* le = nullptr;
        for (const SiosEtikett& e : b.et) if (upper(e.name) == lib) { le = &e; break; }
        if (!le) {
            grund = "Bibliothek '" + lib + "' nicht gefunden (Objekte lassen sich nur in eine "
                    "vorhandene Bibliothek einfuegen)";
            return false;
        }
        auto it = b.bibs.find(lib);
        if (it == b.bibs.end()) {
            Bib neu;
            if (!bibLaden(*le, neu, grund)) return false;
            it = b.bibs.emplace(lib, std::move(neu)).first;
        }
        Bib& bib = it->second;
        for (int slot = 0; slot < bib.belegt; ++slot) {
            uint8_t* p = &bib.dir[static_cast<size_t>(slot) * 8];
            std::string n;
            for (int i = 0; i < 5 && p[i] != 0; ++i) n += static_cast<char>(p[i]);
            if (upper(rtrim(n)) != obj) continue;
            if (!ueberschreiben) { grund = "'" + lib + "/" + obj + "' existiert bereits"; return false; }
            std::fill(p, p + 5, 0x20);          // geloescht: Name blank, AADR/ML bleiben (Handbuch)
        }
        if (bib.belegt >= kBibSlotsMax) {
            grund = "das Verzeichnis von '" + lib + "' ist voll (" + std::to_string(kBibSlotsMax)
                  + " Plaetze; der 15. wuerde den unbelegten NV-Wert erraten, eine Fortsetzung "
                    "ueber den ersten Sektor hinaus ist unbekannt)";
            return false;
        }
        if (!bib.nv_passt) {
            grund = "das Verzeichnis von '" + lib + "' fuehrt NV nach einer unbekannten Regel - "
                    "es wird nicht veraendert";
            return false;
        }
        const int ml = mlFuer(a.sektoren);
        if (ml > 255) { grund = "das Objekt ist zu gross (ML ist ein Byte)"; return false; }
        if (bib.nd + a.sektoren - 1 > bib.gesamt) {
            grund = "in '" + lib + "' ist nicht genug Platz: " + std::to_string(a.sektoren)
                  + " Sektoren gebraucht, " + std::to_string(bib.gesamt - bib.nd + 1) + " frei "
                    "(geloeschte Objekte geben ihren Platz nicht zurueck)";
            return false;
        }
        uint8_t* p = &bib.dir[static_cast<size_t>(bib.belegt) * 8];
        std::fill(p, p + 5, 0x00);
        std::copy(obj.begin(), obj.end(), p);
        p[5] = static_cast<uint8_t>(bib.nd & 0xFF);
        p[6] = static_cast<uint8_t>(bib.nd >> 8);
        p[7] = static_cast<uint8_t>(ml);
        a.objekt = true;
        a.aadr = bib.nd;
        a.lib_boe = bib.e.boe;
        bib.nd += a.sektoren;
        ++bib.belegt;
        bib.dir[122] = static_cast<uint8_t>(bib.nd & 0xFF);
        bib.dir[123] = static_cast<uint8_t>(bib.nd >> 8);
        bib.dir[120] = static_cast<uint8_t>((0x200 + 8 * bib.belegt) >> 8);
        bib.dir[121] = static_cast<uint8_t>((0x200 + 8 * bib.belegt) & 0xFF);
        a.dir_neu = bib.dir;
        return true;
    }

    // ── physische Datei ───────────────────────────────────────────────────────
    const std::string name = upper(rohname);
    uint8_t dummy = 0;
    if (name.empty() || name.size() > 8
        || std::any_of(name.begin(), name.end(), [&](char c) {
               return c == ' ' || !SiosFileSystem::dkoiKodieren(c, dummy); })) {
        grund = "Dateiname '" + name + "' ungueltig: 1 bis 8 Zeichen aus dem DKOI-Zeichensatz "
                "(Grossbuchstaben, Ziffern, Satzzeichen; kein Blank, kein '/')";
        return false;
    }
    for (size_t i = 0; i < b.et.size(); ++i) {
        if (upper(b.et[i].name) != name) continue;
        if (!ueberschreiben) { grund = "'" + name + "' existiert bereits"; return false; }
        a.alt_etikett = b.et[i].sektor;
        for (int t = b.et[i].boe / kSiosSektoren; t <= b.et[i].eoe / kSiosSektoren; ++t)
            b.spur[static_cast<size_t>(t)] = false;
        b.etikett_frei[b.et[i].sektor] = true;
        b.bibs.erase(name);
        b.et.erase(b.et.begin() + static_cast<long>(i));
        break;
    }
    a.spuren = std::max(1, (a.sektoren + kSiosSektoren - 1) / kSiosSektoren);
    for (int s = kErsterEtikettenSektor; s <= kSiosSektoren; ++s)
        if (b.etikett_frei[s]) { a.etikett = s; break; }
    if (!a.etikett) {
        grund = "Spur 0 ist gefuellt: alle 19 Etikettensaetze sind belegt (Dateien loeschen)";
        return false;
    }
    int lauf = 0, groesste = 0, start = -1;
    for (int t = 1; t <= letzteNutzSpur(); ++t) {
        lauf = b.spur[static_cast<size_t>(t)] ? 0 : lauf + 1;
        groesste = std::max(groesste, lauf);
        if (lauf == a.spuren) { start = t - a.spuren + 1; break; }
    }
    if (start < 0) {
        grund = "kein zusammenhaengender freier Bereich von " + std::to_string(a.spuren)
              + " Spuren (die groesste Luecke hat " + std::to_string(groesste)
              + "; nutzbar sind Spur 1 bis " + std::to_string(letzteNutzSpur()) + ")";
        return false;
    }
    a.boe_spur = start;
    for (int t = start; t < start + a.spuren; ++t) b.spur[static_cast<size_t>(t)] = true;
    b.etikett_frei[a.etikett] = false;
    const int boe_lin = start * kSiosSektoren;
    a.text = hdr1Text(name, start, start + a.spuren - 1, boe_lin + a.sektoren);
    SiosEtikett e;
    e.sektor = a.etikett; e.name = name; e.satzlaenge = kSiosSektor;
    e.boe = boe_lin; e.eoe = (start + a.spuren) * kSiosSektoren - 1; e.eod = boe_lin + a.sektoren;
    b.et.push_back(e);
    return true;
}

bool SiosFileSystem::datenSchreiben(int linear, const std::vector<uint8_t>& daten, int sektoren) {
    for (int i = 0; i < sektoren; ++i) {
        std::vector<uint8_t> blk(kSiosSektor, 0xE5);          // Fuellbyte der Datenspuren
        const size_t von = static_cast<size_t>(i) * kSiosSektor;
        const size_t n = std::min(kSiosSektor, daten.size() - von);
        std::copy(daten.begin() + static_cast<long>(von), daten.begin() + static_cast<long>(von + n),
                  blk.begin());
        const int lin = linear + i;
        if (!space_.writeSector(static_cast<uint8_t>(lin / kSiosSektoren), 0,
                                static_cast<uint8_t>(lin % kSiosSektoren + 1), blk))
            return fail(space_.lastError());
    }
    return true;
}

bool SiosFileSystem::etikettSchreiben(int sektor, const std::vector<uint8_t>& satz, bool aktiv) {
    // Die Datenmarke ist Teil des Formats (§4.4): aktiver Satz FB, freier/geloeschter F8.
    if (!space_.writeSectorDam(0, 0, static_cast<uint8_t>(sektor), satz, !aktiv))
        return fail(space_.lastError());
    return true;
}

bool SiosFileSystem::ausfuehren(const Aufgabe& a, const std::vector<uint8_t>& daten) {
    if (a.objekt) {
        const int lin = a.lib_boe + a.aadr - 1;
        if (!datenSchreiben(lin, daten, a.sektoren)) return false;
        const int dl = a.lib_boe + 1;
        if (!space_.writeSector(static_cast<uint8_t>(dl / kSiosSektoren), 0,
                                static_cast<uint8_t>(dl % kSiosSektoren + 1), a.dir_neu))
            return fail(space_.lastError());
        return true;
    }
    if (!datenSchreiben(a.boe_spur * kSiosSektoren, daten, a.sektoren)) return false;
    if (a.alt_etikett) {
        SectorData s;
        if (!space_.readSector(0, 0, static_cast<uint8_t>(a.alt_etikett), s)) return fail(space_.lastError());
        s.data[0] = 0xC4;                                       // HDR1 -> DDR1 (§4.3)
        if (!etikettSchreiben(a.alt_etikett, s.data, false)) return false;
    }
    return etikettSchreiben(a.etikett, etikettBytes(a.text), true);
}

bool SiosFileSystem::write(const std::string& name, const std::vector<uint8_t>& data,
                           const WriteOptions& opt) {
    Belegung b;
    std::string grund;
    if (!belegungLesen(b, grund)) return fail("SIOS: " + grund);
    Aufgabe a;
    if (!plane(b, name, data.size(), opt.overwrite, a, grund)) return fail("SIOS: " + grund);
    return ausfuehren(a, data);
}

bool SiosFileSystem::erase(const std::string& rohname) {
    const size_t schraeg = rohname.find('/');
    if (schraeg != std::string::npos) {
        const std::string lib = upper(rohname.substr(0, schraeg));
        const std::string obj = upper(rohname.substr(schraeg + 1));
        for (const SiosEtikett& e : etiketten()) {
            if (upper(e.name) != lib) continue;
            Bib bib;
            std::string grund;
            if (!bibLaden(e, bib, grund)) return fail("SIOS: " + grund);
            for (int slot = 0; slot < bib.belegt; ++slot) {
                uint8_t* p = &bib.dir[static_cast<size_t>(slot) * 8];
                std::string n;
                for (int i = 0; i < 5 && p[i] != 0; ++i) n += static_cast<char>(p[i]);
                if (obj.empty() || upper(rtrim(n)) != obj) continue;
                // Geloescht = Name blank; AADR und ML bleiben stehen, der Datenteil auch
                // (Handbuch "DELET", Formatdokument §7.3).  Der Platz kommt erst mit COPY zurueck.
                std::fill(p, p + 5, 0x20);
                const int dl = e.boe + 1;
                if (!space_.writeSector(static_cast<uint8_t>(dl / kSiosSektoren), 0,
                                        static_cast<uint8_t>(dl % kSiosSektoren + 1), bib.dir))
                    return fail(space_.lastError());
                return true;
            }
            return fail("Datei nicht gefunden: " + rohname);
        }
        return fail("Datei nicht gefunden: " + rohname);
    }
    const std::string name = upper(rohname);
    for (const SiosEtikett& e : etiketten()) {
        if (upper(e.name) != name) continue;
        SectorData s;
        if (!space_.readSector(0, 0, static_cast<uint8_t>(e.sektor), s)) return fail(space_.lastError());
        s.data[0] = 0xC4;                      // HDR1 -> DDR1; der Rest des Satzes bleibt (SG1 DATAnn)
        return etikettSchreiben(e.sektor, s.data, false);
    }
    return fail("Datei nicht gefunden: " + rohname);
}

bool SiosFileSystem::spur0Anlegen(bool bootbereich) {
    if (bootbereich)
        for (int s = 1; s <= 4; ++s)
            if (!etikettSchreiben(s, std::vector<uint8_t>(kSiosSektor, 0x40), true)) return false;
    std::string ermap(80, ' ');
    ermap.replace(0, 5, "ERMAP");
    std::string vol(80, ' ');
    vol.replace(0, 4, "VOL1");
    vol.replace(4, label_.size(), label_);
    vol[70] = 'M';
    vol.replace(76, 2, "01");
    vol[79] = 'W';
    if (!etikettSchreiben(5, etikettBytes(ermap), true)) return false;
    if (!etikettSchreiben(6, etikettBytes(""), true)) return false;
    if (!etikettSchreiben(7, etikettBytes(vol), true)) return false;
    for (int s = kErsterEtikettenSektor; s <= kSiosSektoren; ++s)
        if (!etikettSchreiben(s, etikettBytes("D"), false)) return false;
    return true;
}

bool SiosFileSystem::mkfs() { return spur0Anlegen(false); }

std::unique_ptr<SiosFileSystem> SiosFileSystem::format(SectorSpace& space, const FsProfile& prof,
                                                       const std::string& label, std::string& err) {
    if (space.sectorsPerTrack(0, 0) != kSiosSektoren || space.sectorSize(0, 0) != kSiosSektor) {
        err = "SIOS: Spur 0 ist nicht 26 x 128 B";
        return nullptr;
    }
    const std::string name = upper(label);
    uint8_t dummy = 0;
    if (name.size() > 6 || std::any_of(name.begin(), name.end(), [&](char c) {
            return !SiosFileSystem::dkoiKodieren(c, dummy); })) {
        err = "SIOS: Datentraegername '" + label
            + "' ungueltig: hoechstens 6 Zeichen aus dem DKOI-Zeichensatz";
        return nullptr;
    }
    std::unique_ptr<SiosFileSystem> fs(new SiosFileSystem(space, prof));
    fs->label_ = name;
    int hoechster = 0;
    for (size_t i = 0; i < space.trackCount(); ++i)
        hoechster = std::max<int>(hoechster, space.trackAt(i).cyl + 1);
    fs->tracks_ = hoechster;
    if (!fs->spur0Anlegen(true)) { err = fs->lastError(); return nullptr; }
    return fs;
}

bool SiosFileSystem::wouldFit(const std::vector<PlannedFile>& files, FitReport& out) const {
    out = FitReport{};
    Belegung b;
    std::string grund;
    if (!belegungLesen(b, grund)) { out.detail = "SIOS: " + grund; return true; }
    int freie_spuren = 0, freie_etiketten = 0;
    for (int t = 1; t <= letzteNutzSpur(); ++t) if (!b.spur[static_cast<size_t>(t)]) ++freie_spuren;
    for (int s = kErsterEtikettenSektor; s <= kSiosSektoren; ++s) if (b.etikett_frei[s]) ++freie_etiketten;
    out.available = static_cast<uint64_t>(freie_spuren) * kSiosSektoren * kSiosSektor;
    out.dir_free  = freie_etiketten;
    for (const PlannedFile& f : files) {
        Aufgabe a;
        // Im Stapel ersetzt der Ordner den Bestand (insertAll setzt overwrite).
        if (!plane(b, f.name, f.size, true, a, grund)) {
            out.detail = "'" + f.name + "': " + grund;
            return true;                         // die Auskunft "passt nicht" ist ein Ergebnis
        }
        if (!a.objekt) {
            out.needed += static_cast<uint64_t>(a.spuren) * kSiosSektoren * kSiosSektor;
            ++out.dir_needed;
        } else {
            out.needed += static_cast<uint64_t>(a.sektoren) * kSiosSektor;
        }
    }
    out.fits = true;
    return true;
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
