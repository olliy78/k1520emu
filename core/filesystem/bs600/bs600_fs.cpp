/**
 * @file bs600_fs.cpp
 * @brief BS600 - siehe bs600_fs.h und doc/bs600_diskettenformat.md.
 */

#include "core/filesystem/bs600/bs600_fs.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>

namespace {

constexpr int kVol1Sektor    = 7;
constexpr int kVerzSektor0   = 8;     ///< Sektor 8..25 tragen das Verzeichnis
constexpr int kTabellenSektor = 26;
constexpr int kEintraegeJeSektor = 3;

std::string rtrim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    return s;
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string text(const std::vector<uint8_t>& d, size_t von, size_t bis) {
    std::string s;
    for (size_t i = von; i < bis && i < d.size(); ++i) s += static_cast<char>(d[i]);
    return s;
}

/// @brief Vier Hexziffern (Grossbuchstaben) -> Wert; -1 = kein Hexfeld.
int hex4(const std::string& t) {
    if (t.size() != 4) return -1;
    int v = 0;
    for (char c : t) {
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return -1;
        v = v * 16 + d;
    }
    return v;
}

std::string hexText(int v) {
    char b[8];
    std::snprintf(b, sizeof b, "%04X", v & 0xFFFF);
    return b;
}



std::string heute() {
    const std::time_t t = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char b[8];
    std::snprintf(b, sizeof b, "%02d%02d%02d", lt.tm_mday, lt.tm_mon + 1, lt.tm_year % 100);
    return b;
}

}  // namespace

// Geteilte Handgriffe (auch von check/bs600_check.cpp und check/bs600_recover.cpp gebraucht).
namespace bs600 {

/// @brief Der Anwendungsname einer Datei: 1..6 sichtbare ASCII-Zeichen ohne '.', '/', '\\'.
bool nameGueltig(const std::string& n) {
    if (n.empty() || n.size() > 6) return false;
    for (char c : n)
        if (c <= 0x20 || c > 0x7E || c == '.' || c == '/' || c == '\\') return false;
    return true;
}

/// @brief `TTMMJJ` oder `TT.MM.JJ` -> `TTMMJJ`; "" = nicht lesbar.
std::string datumNormal(std::string d) {
    std::string r;
    for (char c : d) if (c != '.') r += c;
    if (r.size() != 6) return {};
    for (char c : r) if (c < '0' || c > '9') return {};
    const int tag = (r[0] - '0') * 10 + (r[1] - '0'), mon = (r[2] - '0') * 10 + (r[3] - '0');
    if (tag < 1 || tag > 31 || mon < 1 || mon > 12) return {};
    return r;
}

bool leerEintrag(const std::vector<uint8_t>& p) {
    for (uint8_t fuell : {uint8_t(0x20), uint8_t(0x00), uint8_t(0xE5)})
        if (std::all_of(p.begin(), p.end(), [&](uint8_t b) { return b == fuell; })) return true;
    return false;
}

/// @brief Bit (Spur, Sektor) in der rohen Belegungstabelle (Sektor 26) setzen/loeschen.
void tabBit(std::vector<uint8_t>& tab, int spur, int sek, bool belegt) {
    const size_t i = static_cast<size_t>(2 * (spur - 1) + (sek - 1) / 8);
    const uint8_t m = static_cast<uint8_t>(0x80 >> ((sek - 1) % 8));
    if (i >= tab.size()) return;
    if (belegt) tab[i] = static_cast<uint8_t>(tab[i] | m); else tab[i] = static_cast<uint8_t>(tab[i] & ~m);
}

bool tabBitGesetzt(const std::vector<uint8_t>& tab, int spur, int sek) {
    const size_t i = static_cast<size_t>(2 * (spur - 1) + (sek - 1) / 8);
    return i < tab.size() && (tab[i] & (0x80 >> ((sek - 1) % 8)));
}

}  // namespace bs600

using bs600::nameGueltig;
using bs600::datumNormal;
using bs600::leerEintrag;
using bs600::tabBit;
using bs600::tabBitGesetzt;

namespace {

std::string datumAnzeige(const std::string& d) {
    return datumNormal(d).empty() ? rtrim(d) : d.substr(0, 2) + "." + d.substr(2, 2) + "." + d.substr(4, 2);
}

int linVon(int spur, int sek) { return (spur - 1) * kBs600Sektoren + (sek - 1); }


/// @brief Hoechster Zylinder + 1 des Raums.
int spurenVon(const SectorSpace& space) {
    int h = 0;
    for (size_t i = 0; i < space.trackCount(); ++i) h = std::max<int>(h, space.trackAt(i).cyl + 1);
    return h;
}

/// @brief 42 Byte -> Eintrag (Plausibilitaet gegen @p spuren).
Bs600Eintrag parse(int platz, const std::vector<uint8_t>& p, int spuren) {
    Bs600Eintrag e;
    e.platz = platz;
    e.name = rtrim(text(p, 0, 6));
    e.typ = static_cast<char>(p[6]);
    e.anf_spur = p[7]; e.anf_sek = p[8]; e.end_spur = p[9]; e.end_sek = p[10];
    e.datum = text(p, 11, 17);
    e.signum = rtrim(text(p, 17, 20));
    e.ladeadr = hex4(text(p, 20, 24));
    e.endadr = hex4(text(p, 24, 28));
    e.startadr = hex4(text(p, 28, 32));
    e.rest.assign(p.begin() + 32, p.begin() + 42);
    auto nein = [&](const std::string& w) { e.gueltig = false; e.problem = w; };
    if (e.name.empty()) {
        e.name = "?" + std::to_string(platz);
        nein("Name fehlt");
    } else if (e.anf_spur < 1 || e.anf_spur >= spuren || e.end_spur < 1 || e.end_spur >= spuren
               || e.anf_sek < 1 || e.anf_sek > kBs600Sektoren
               || e.end_sek < 1 || e.end_sek > kBs600Sektoren) {
        nein("Spur/Sektor ausserhalb der Diskette");
    } else if (e.linEnde() < e.linAnfang()) {
        nein("Ende liegt vor dem Anfang");
    }
    return e;
}

/// @brief Wie viele Nutzbytes traegt der Eintrag?  -1 = Adressfelder unbrauchbar.
long nutzbytes(const Bs600Eintrag& e) {
    if (e.ladeadr < 0 || e.endadr < e.ladeadr) return -1;
    return static_cast<long>(e.endadr) - e.ladeadr + 1;
}

/// @brief Lauf-Suche: erster zusammenhaengender freier Lauf von @p n Sektoren in Spur 1..37.
int findeLauf(const std::vector<uint8_t>& belegt, int n) {
    int lauf = 0;
    for (int lin = 0; lin < kBs600LetzteNutzSpur * kBs600Sektoren; ++lin) {
        const int spur = lin / kBs600Sektoren + 1, sek = lin % kBs600Sektoren + 1;
        if (belegt[static_cast<size_t>(spur) * 16 + static_cast<size_t>(sek)]) { lauf = 0; continue; }
        if (++lauf == n) return lin - n + 1;
    }
    return -1;
}

int freieNutzSektoren(const std::vector<uint8_t>& belegt) {
    int n = 0;
    for (int s = 1; s <= kBs600LetzteNutzSpur; ++s)
        for (int k = 1; k <= kBs600Sektoren; ++k)
            if (!belegt[static_cast<size_t>(s) * 16 + static_cast<size_t>(k)]) ++n;
    return n;
}



void lauf(std::vector<uint8_t>& tab, std::vector<uint8_t>* belegt, int lin_von, int n, bool setzen) {
    for (int l = lin_von; l < lin_von + n; ++l) {
        const int spur = l / kBs600Sektoren + 1, sek = l % kBs600Sektoren + 1;
        tabBit(tab, spur, sek, setzen);
        if (belegt) (*belegt)[static_cast<size_t>(spur) * 16 + static_cast<size_t>(sek)] = setzen ? 1 : 0;
    }
}

}  // namespace

/// @brief Zustand fuer Planung und Schreiben: Verzeichnis, rohe Tabelle, Vereinigung der Belegung.
struct Bs600FileSystem::Belegung {
    std::vector<Bs600Eintrag> eintr;
    std::vector<uint8_t>      platz_belegt = std::vector<uint8_t>(kBs600Eintraege, 0);
    std::vector<uint8_t>      tab;       ///< roher Sektor 26 (128 B)
    std::vector<uint8_t>      belegt;    ///< [Spur*16 + Sektor]: Tabelle UND gueltige Eintraege
};

// ─── Erkennung / Mounten ─────────────────────────────────────────────────────

bool Bs600FileSystem::looksLikeBs600(const SectorSpace& space, std::string* why) {
    auto nein = [&](const std::string& w) { if (why) *why = w; return false; };
    if (space.sectorsPerTrack(0, 0) != 26 || space.sectorSize(0, 0) != kBs600Sektor0)
        return nein("Spur 0 ist nicht 26 x 128 B");
    if (space.sectorsPerTrack(1, 0) != kBs600Sektoren || space.sectorSize(1, 0) != kBs600Sektor)
        return nein("Spur 1 ist nicht 15 x 256 B");
    SectorData v;
    if (!space.readSector(0, 0, kVol1Sektor, v) || v.data.size() != kBs600Sektor0)
        return nein("Spur 0 Sektor 7 nicht lesbar");
    if (v.data[0] != 0xE5 || v.data[1] != 0xD6 || v.data[2] != 0xD3 || v.data[3] != 0xF1)
        return nein("Spur 0 Sektor 7 traegt kein VOL1 (EBCDIC)");
    if (text(v.data, 11, 14) != "PRG") return nein("im Kennsatz fehlt 'PRG' (Byte 11..13)");
    const int spuren = spurenVon(space);
    int gesamt = 0, schlecht = 0;
    for (int s = 0; s < kBs600Eintraege / kEintraegeJeSektor; ++s) {
        SectorData d;
        if (!space.readSector(0, 0, static_cast<uint8_t>(kVerzSektor0 + s), d)
            || d.data.size() != kBs600Sektor0)
            return nein("Verzeichnissektor " + std::to_string(kVerzSektor0 + s) + " nicht lesbar");
        for (int k = 0; k < kEintraegeJeSektor; ++k) {
            const std::vector<uint8_t> p(d.data.begin() + static_cast<long>(k * kBs600EintragGroesse),
                                         d.data.begin() + static_cast<long>((k + 1) * kBs600EintragGroesse));
            if (leerEintrag(p)) continue;
            ++gesamt;
            const Bs600Eintrag e = parse(s * kEintraegeJeSektor + k, p, spuren);
            if (!e.gueltig || (e.typ != 'P' && e.typ != 'L')) ++schlecht;
        }
    }
    if (schlecht * 2 > gesamt)
        return nein("das Verzeichnis ist unplausibel (" + std::to_string(schlecht) + " von "
                    + std::to_string(gesamt) + " Eintraegen unsinnig)");
    return true;
}

std::unique_ptr<Bs600FileSystem> Bs600FileSystem::mount(SectorSpace& space, const FsProfile& prof,
                                                        std::string& err) {
    std::string warum;
    if (!looksLikeBs600(space, &warum)) { err = "keine BS600-Diskette: " + warum; return nullptr; }
    std::unique_ptr<Bs600FileSystem> fs(new Bs600FileSystem(space, prof));
    SectorData v;
    space.readSector(0, 0, kVol1Sektor, v);
    fs->label_ = rtrim(text(v.data, 4, 10));
    fs->tracks_ = spurenVon(space);
    return fs;
}

std::string Bs600FileSystem::bootProblem(const std::vector<uint8_t>& img) {
    if (img.size() > static_cast<size_t>(kBootSektoren) * kBs600Sektor0)
        return "Das Bootabbild ist " + std::to_string(img.size()) + " Byte gross - der Urlader der BS600-Diskette "
               "belegt nur Sektor 1..3 der Spur 0 (384 Byte).";
    if (img.size() < 16 || img[0] != 0x18
        || !(text(img, 2, 10) == "SYL FOR " || text(img, 2, 15) == "BOOTSTRAP FOR"))
        return "Das ist kein BS600-Urlader: der Kopf 'SYL FOR BS6x0' (PRG 710/710-1) bzw. 'BOOTSTRAP FOR BS600' "
               "(PRG 700) ab Byte 2 fehlt.";
    return {};
}

Bs600Urlader Bs600FileSystem::urladerKennung(const std::vector<uint8_t>& img) {
    Bs600Urlader u;
    const bool leer = std::all_of(img.begin(), img.end(),
                                  [](uint8_t b) { return b == 0xE5 || b == 0x00 || b == 0x4E; });
    if (img.empty() || leer) return u;                       // Art::Keiner
    u.art = Bs600Urlader::Art::Unbekannt;
    if (img.size() < 16 || img[0] != 0x18) { u.text = "kein BS600-Urlader (Kopf 18 xx fehlt)"; return u; }
    if (text(img, 2, 10) == "SYL FOR ") {
        u.art = Bs600Urlader::Art::Syl;
        u.name = rtrim(text(img, 10, 16));
        if (u.name == "BS620")      { u.schalter = "710-1"; u.geraet = "PRG 710-1"; }
        else if (u.name == "BS610") { u.schalter = "710";   u.geraet = "PRG 710"; }     // [?] nur Webseite
        else                        { u.geraet = "PRG 710 oder PRG 710-1 (Fassung nicht bestimmbar)"; }
        u.text = "Urlader 'SYL FOR " + u.name + "' (NKM-Lader) - startet am " + u.geraet;
    } else if (text(img, 2, 15) == "BOOTSTRAP FOR") {
        u.art = Bs600Urlader::Art::Bootstrap;
        u.name = rtrim(text(img, 15, 21));
        u.schalter = "700";
        u.geraet = "PRG 700";
        u.text = "Urlader 'BOOTSTRAP FOR " + u.name + "' (3-KB-ROM) - startet am PRG 700, NICHT am PRG 710/710-1";
    } else {
        u.text = "kein BS600-Urlader (weder 'SYL FOR ' noch 'BOOTSTRAP FOR' ab Byte 2)";
    }
    return u;
}

std::string Bs600FileSystem::urladerGegenWunsch(const std::vector<uint8_t>& img, const std::string& prg) {
    if (prg != "700" && prg != "710" && prg != "710-1")
        return "--prg erwartet 700, 710 oder 710-1";
    const Bs600Urlader u = urladerKennung(img);
    if (u.art == Bs600Urlader::Art::Keiner || u.art == Bs600Urlader::Art::Unbekannt)
        return "Das Bootabbild traegt keinen BS600-Urlader: " + (u.text.empty() ? std::string("es ist leer") : u.text);
    if (u.art == Bs600Urlader::Art::Bootstrap) {
        if (prg == "700") return {};
        return "Der Urlader 'BOOTSTRAP FOR " + u.name + "' ist die Bauart des PRG 700 (3-KB-ROM); der NKM-Lader des PRG "
               + prg + " bootet ihn nicht - dort gehoert ein 'SYL FOR BS6x0' hin.";
    }
    if (prg == "700")
        return "Der Urlader 'SYL FOR " + u.name + "' ist fuer den NKM-Lader des PRG 710/710-1; das PRG 700 braucht "
               "'BOOTSTRAP FOR BS600'.";
    if (!u.schalter.empty() && u.schalter != prg)
        return "Der Urlader 'SYL FOR " + u.name + "' ist fuer das " + u.geraet + " gebaut, verlangt ist PRG " + prg + ".";
    return {};
}

Bs600Urlader Bs600FileSystem::urlader() const {
    std::vector<uint8_t> img;
    for (int i = 1; i <= kBootSektoren; ++i) {
        std::vector<uint8_t> s;
        if (!sektorLesen(0, i, s)) break;
        img.insert(img.end(), s.begin(), s.end());
    }
    return urladerKennung(img);
}

// ─── Lesen ───────────────────────────────────────────────────────────────────

bool Bs600FileSystem::sektorLesen(int cyl, int id, std::vector<uint8_t>& out) const {
    SectorData s;
    const size_t soll = cyl == 0 ? kBs600Sektor0 : kBs600Sektor;
    if (!space_.readSector(static_cast<uint8_t>(cyl), 0, static_cast<uint8_t>(id), s) || s.data.size() != soll)
        return false;
    out = std::move(s.data);
    return true;
}

std::vector<Bs600Eintrag> Bs600FileSystem::eintraege() const {
    std::vector<Bs600Eintrag> out;
    for (int s = 0; s < kBs600Eintraege / kEintraegeJeSektor; ++s) {
        std::vector<uint8_t> d;
        if (!sektorLesen(0, kVerzSektor0 + s, d)) continue;
        for (int k = 0; k < kEintraegeJeSektor; ++k) {
            const std::vector<uint8_t> p(d.begin() + static_cast<long>(k * kBs600EintragGroesse),
                                         d.begin() + static_cast<long>((k + 1) * kBs600EintragGroesse));
            if (leerEintrag(p)) continue;
            out.push_back(parse(s * kEintraegeJeSektor + k, p, tracks_));
        }
    }
    return out;
}

std::vector<uint8_t> Bs600FileSystem::belegungstabelle() const {
    std::vector<uint8_t> raw, out(static_cast<size_t>(tracks_) * 16, 0);
    if (!sektorLesen(0, kTabellenSektor, raw)) return out;
    for (int s = 1; s < tracks_; ++s)
        for (int k = 1; k <= kBs600Sektoren; ++k)
            if (tabBitGesetzt(raw, s, k)) out[static_cast<size_t>(s) * 16 + static_cast<size_t>(k)] = 1;
    return out;
}

bool Bs600FileSystem::belegungLesen(Belegung& b, std::string& grund) const {
    b = Belegung{};
    if (!sektorLesen(0, kTabellenSektor, b.tab)) { grund = "Belegungstabelle (Spur 0 Sektor 26) nicht lesbar"; return false; }
    b.belegt.assign(static_cast<size_t>(std::max(tracks_, 1)) * 16, 0);
    for (int s = 1; s < tracks_; ++s)
        for (int k = 1; k <= kBs600Sektoren; ++k)
            if (tabBitGesetzt(b.tab, s, k)) b.belegt[static_cast<size_t>(s) * 16 + static_cast<size_t>(k)] = 1;
    b.eintr = eintraege();
    for (const Bs600Eintrag& e : b.eintr) {
        b.platz_belegt[static_cast<size_t>(e.platz)] = 1;
        // Auch ein Eintrag, den die Tabelle nicht fuehrt, gehoert zur Belegung (die Tabelle ist nicht
        // unfehlbar): was einer von beiden belegt nennt, wird nie vergeben.
        if (e.gueltig)
            for (int l = e.linAnfang(); l <= e.linEnde(); ++l)
                b.belegt[static_cast<size_t>(l / kBs600Sektoren + 1) * 16 + static_cast<size_t>(l % kBs600Sektoren + 1)] = 1;
    }
    return true;
}

std::vector<FileEntry> Bs600FileSystem::list() const {
    std::vector<FileEntry> out;
    for (const Bs600Eintrag& e : eintraege()) {
        FileEntry f;
        f.name = e.name;
        f.type = std::string(1, e.typ);
        f.date = datumAnzeige(e.datum);
        f.created = e.signum;
        if (e.ladeadr >= 0) f.low_addr = static_cast<uint16_t>(e.ladeadr);
        if (e.endadr >= 0) f.high_addr = static_cast<uint16_t>(e.endadr);
        if (e.startadr >= 0) f.entry_addr = static_cast<uint16_t>(e.startadr);
        f.damaged = !e.gueltig;
        if (e.gueltig) {
            const long soll = nutzbytes(e);
            const long max = static_cast<long>(e.sektoren()) * static_cast<long>(kBs600Sektor);
            if (soll < 0 || soll > max) { f.size = static_cast<uint64_t>(max); f.damaged = true; }
            else f.size = static_cast<uint64_t>(soll);
            char b[32];
            std::snprintf(b, sizeof b, "%02d/%02d-%02d/%02d", e.anf_spur, e.anf_sek, e.end_spur, e.end_sek);
            f.extent = b;
        }
        out.push_back(std::move(f));
    }
    return out;
}

bool Bs600FileSystem::eintragFinden(const std::string& name, Bs600Eintrag& out) const {
    const std::string n = upper(rtrim(name));
    for (const Bs600Eintrag& e : eintraege())
        if (upper(e.name) == n) { out = e; return true; }
    return false;
}

bool Bs600FileSystem::firstSector(const std::string& name, FsRecoverOrt& out) const {
    Bs600Eintrag e;
    if (!eintragFinden(name, e) || !e.gueltig) return false;
    out.cyl = e.anf_spur; out.head = 0; out.sector = e.anf_sek;
    return true;
}

bool Bs600FileSystem::read(const std::string& name, std::vector<uint8_t>& out) {
    out.clear();
    Bs600Eintrag e;
    if (!eintragFinden(name, e)) return fail("Datei nicht gefunden: " + name);
    if (!e.gueltig) return fail("Eintrag '" + e.name + "' ist unbrauchbar: " + e.problem);
    const int n = e.sektoren();
    for (int i = 0; i < n; ++i) {
        const int lin = e.linAnfang() + i;
        std::vector<uint8_t> s;
        if (!sektorLesen(lin / kBs600Sektoren + 1, lin % kBs600Sektoren + 1, s))
            return fail("Sektor " + std::to_string(lin / kBs600Sektoren + 1) + "/"
                        + std::to_string(lin % kBs600Sektoren + 1) + " nicht lesbar");
        out.insert(out.end(), s.begin(), s.end());
    }
    const long soll = nutzbytes(e);
    if (soll >= 0 && static_cast<size_t>(soll) <= out.size()) out.resize(static_cast<size_t>(soll));
    return true;
}

// ─── Schreiben ───────────────────────────────────────────────────────────────

std::vector<uint8_t> Bs600FileSystem::eintragBytes(const Bs600Eintrag& e) const {
    std::vector<uint8_t> p(kBs600EintragGroesse, 0x20);
    for (size_t i = 0; i < 6 && i < e.name.size(); ++i) p[i] = static_cast<uint8_t>(e.name[i]);
    p[6] = static_cast<uint8_t>(e.typ);
    p[7] = static_cast<uint8_t>(e.anf_spur); p[8] = static_cast<uint8_t>(e.anf_sek);
    p[9] = static_cast<uint8_t>(e.end_spur); p[10] = static_cast<uint8_t>(e.end_sek);
    for (size_t i = 0; i < 6 && i < e.datum.size(); ++i) p[11 + i] = static_cast<uint8_t>(e.datum[i]);
    for (size_t i = 0; i < 3 && i < e.signum.size(); ++i) p[17 + i] = static_cast<uint8_t>(e.signum[i]);
    const std::string la = hexText(e.ladeadr < 0 ? 0 : e.ladeadr), en = hexText(e.endadr < 0 ? 0 : e.endadr),
                      st = hexText(e.startadr < 0 ? 0 : e.startadr);
    for (size_t i = 0; i < 4; ++i) {
        p[20 + i] = static_cast<uint8_t>(la[i]);
        p[24 + i] = static_cast<uint8_t>(en[i]);
        p[28 + i] = static_cast<uint8_t>(st[i]);
    }
    if (e.rest.size() == 10) std::copy(e.rest.begin(), e.rest.end(), p.begin() + 32);
    return p;
}

bool Bs600FileSystem::eintragSchreiben(int platz, const std::vector<uint8_t>& satz42) {
    const int sektor = kVerzSektor0 + platz / kEintraegeJeSektor;
    std::vector<uint8_t> d;
    if (!sektorLesen(0, sektor, d)) return fail("Verzeichnissektor " + std::to_string(sektor) + " nicht lesbar");
    std::copy(satz42.begin(), satz42.end(), d.begin() + static_cast<long>((platz % kEintraegeJeSektor) * kBs600EintragGroesse));
    if (!space_.writeSector(0, 0, static_cast<uint8_t>(sektor), d)) return fail(space_.lastError());
    return true;
}

bool Bs600FileSystem::tabelleSchreiben(const std::vector<uint8_t>& sektor128) {
    if (!space_.writeSector(0, 0, kTabellenSektor, sektor128)) return fail(space_.lastError());
    return true;
}

bool Bs600FileSystem::datenSchreiben(int lin, const std::vector<uint8_t>& daten, int sektoren) {
    for (int i = 0; i < sektoren; ++i) {
        std::vector<uint8_t> blk(kBs600Sektor, 0xE5);          // Fuellbyte der Datenspuren
        const size_t von = static_cast<size_t>(i) * kBs600Sektor;
        if (von < daten.size()) {
            const size_t n = std::min(kBs600Sektor, daten.size() - von);
            std::copy(daten.begin() + static_cast<long>(von), daten.begin() + static_cast<long>(von + n), blk.begin());
        }
        const int l = lin + i;
        if (!space_.writeSector(static_cast<uint8_t>(l / kBs600Sektoren + 1), 0,
                                static_cast<uint8_t>(l % kBs600Sektoren + 1), blk))
            return fail(space_.lastError());
    }
    return true;
}

namespace {

/// @brief Planungsergebnis fuer eine Datei.
struct Plan {
    int  platz = -1;
    int  lin = 0, sektoren = 0;
    bool ersetzt = false;
    std::vector<uint8_t> rest;       ///< Rest[10] des ersetzten Eintrags
};

}  // namespace

bool Bs600FileSystem::write(const std::string& rohname, const std::vector<uint8_t>& data,
                            const WriteOptions& opt) {
    Belegung b;
    std::string grund;
    if (!belegungLesen(b, grund)) return fail("BS600: " + grund);

    const std::string name = upper(rohname);
    if (!nameGueltig(name))
        return fail("BS600: Dateiname '" + rohname + "' ungueltig: 1 bis 6 sichtbare ASCII-Zeichen (kein '.', '/', Blank)");
    if (data.empty()) return fail("BS600: eine leere Datei laesst sich nicht ablegen (mindestens ein Sektor)");
    const int load = opt.bs600_load;
    if (static_cast<size_t>(load) + data.size() > 0x10000)
        return fail("BS600: Ladeadresse " + hexText(load) + "H + " + std::to_string(data.size())
                    + " Byte laufen ueber das Ende des 64-KB-Adressraums");
    char typ = 'P';
    if (!opt.bs600_type.empty()) {
        typ = static_cast<char>(std::toupper(static_cast<unsigned char>(opt.bs600_type[0])));
        if (opt.bs600_type.size() != 1 || (typ != 'P' && typ != 'L'))
            return fail("BS600: Typ '" + opt.bs600_type + "' unbekannt (P oder L)");
    }
    std::string datum = opt.bs600_date.empty() ? heute() : datumNormal(opt.bs600_date);
    if (datum.empty()) return fail("BS600: Datum '" + opt.bs600_date + "' ungueltig (TTMMJJ)");
    std::string signum = opt.bs600_author;
    if (signum.size() > 3) return fail("BS600: Signum '" + signum + "' ist laenger als 3 Zeichen");
    for (char c : signum) if (c < 0x20 || c > 0x7E) return fail("BS600: Signum enthaelt Steuerzeichen");

    Plan pl;
    for (const Bs600Eintrag& e : b.eintr) {
        if (upper(e.name) != name) continue;
        if (!opt.overwrite) return fail("BS600: '" + name + "' existiert bereits");
        pl.platz = e.platz;
        pl.ersetzt = true;
        pl.rest = e.rest;
        if (e.gueltig) lauf(b.tab, &b.belegt, e.linAnfang(), e.sektoren(), false);
        break;
    }
    if (pl.platz < 0) {
        for (int p = 0; p < kBs600Eintraege; ++p)
            if (!b.platz_belegt[static_cast<size_t>(p)]) { pl.platz = p; break; }
        if (pl.platz < 0) return fail("BS600: das Verzeichnis ist voll (" + std::to_string(kBs600Eintraege) + " Dateien)");
    }
    pl.sektoren = static_cast<int>((data.size() + kBs600Sektor - 1) / kBs600Sektor);
    pl.lin = findeLauf(b.belegt, pl.sektoren);
    if (pl.lin < 0) {
        const int frei = freieNutzSektoren(b.belegt);
        return fail(frei >= pl.sektoren
            ? "BS600: DISK VOLL - kein zusammenhaengender freier Bereich von " + std::to_string(pl.sektoren)
              + " Sektoren (" + std::to_string(frei) + " Sektoren frei, aber zerstueckelt: reorganisieren)"
            : "BS600: DISK VOLL - " + std::to_string(pl.sektoren) + " Sektoren gebraucht, "
              + std::to_string(frei) + " frei (nutzbar sind Spur 1 bis " + std::to_string(kBs600LetzteNutzSpur) + ")");
    }

    // Reihenfolge: Daten, Belegungstabelle, Verzeichnis zuletzt - bricht es ab, steht kein Eintrag auf Unfertigem.
    if (!datenSchreiben(pl.lin, data, pl.sektoren)) return false;
    lauf(b.tab, nullptr, pl.lin, pl.sektoren, true);
    if (!tabelleSchreiben(b.tab)) return false;
    Bs600Eintrag e;
    e.name = name; e.typ = typ;
    e.anf_spur = pl.lin / kBs600Sektoren + 1; e.anf_sek = pl.lin % kBs600Sektoren + 1;
    const int ende = pl.lin + pl.sektoren - 1;
    e.end_spur = ende / kBs600Sektoren + 1; e.end_sek = ende % kBs600Sektoren + 1;
    e.datum = datum; e.signum = signum;
    e.ladeadr = load; e.endadr = load + static_cast<int>(data.size()) - 1; e.startadr = opt.bs600_entry;
    e.rest = pl.rest;
    return eintragSchreiben(pl.platz, eintragBytes(e));
}

bool Bs600FileSystem::erase(const std::string& name) {
    Belegung b;
    std::string grund;
    if (!belegungLesen(b, grund)) return fail("BS600: " + grund);
    Bs600Eintrag e;
    if (!eintragFinden(name, e)) return fail("Datei nicht gefunden: " + name);
    // Erst die Tabelle, dann der Eintrag: bricht es dazwischen ab, bleibt eine Datei mit freigegebenen
    // Sektoren (fsck meldet es) statt belegter Sektoren ohne Eigentuemer.
    if (e.gueltig) {
        lauf(b.tab, nullptr, e.linAnfang(), e.sektoren(), false);
        if (!tabelleSchreiben(b.tab)) return false;
    }
    return eintragSchreiben(e.platz, std::vector<uint8_t>(kBs600EintragGroesse, 0x20));
}

bool Bs600FileSystem::rename(const std::string& alt, const std::string& neu) {
    const std::string nn = upper(neu);
    if (!nameGueltig(nn)) return fail("BS600: Dateiname '" + neu + "' ungueltig: 1 bis 6 sichtbare ASCII-Zeichen");
    Bs600Eintrag e;
    if (!eintragFinden(alt, e)) return fail("Datei nicht gefunden: " + alt);
    Bs600Eintrag anderer;
    if (eintragFinden(nn, anderer) && anderer.platz != e.platz) return fail("BS600: '" + nn + "' existiert bereits");
    e.name = nn;
    return eintragSchreiben(e.platz, eintragBytes(e));
}

bool Bs600FileSystem::setAttributes(const std::string& name, const Bs600Attrs& a) {
    Bs600Eintrag e;
    if (!eintragFinden(name, e)) return fail("Datei nicht gefunden: " + name);
    if (!a.type.empty()) {
        const char t = static_cast<char>(std::toupper(static_cast<unsigned char>(a.type[0])));
        if (a.type.size() != 1 || (t != 'P' && t != 'L')) return fail("BS600: Typ '" + a.type + "' unbekannt (P oder L)");
        e.typ = t;
    }
    if (a.set_date) {
        const std::string d = datumNormal(a.date);
        if (d.empty()) return fail("BS600: Datum '" + a.date + "' ungueltig (TTMMJJ)");
        e.datum = d;
    }
    if (a.set_author) {
        if (a.author.size() > 3) return fail("BS600: Signum '" + a.author + "' ist laenger als 3 Zeichen");
        e.signum = a.author;
    }
    if (a.set_load) {
        const long n = nutzbytes(e);
        if (n < 0) return fail("BS600: die Adressfelder von '" + e.name + "' sind unbrauchbar - Ladeadresse nicht aenderbar");
        if (static_cast<long>(a.load) + n > 0x10000)
            return fail("BS600: Ladeadresse " + hexText(a.load) + "H + " + std::to_string(n) + " Byte laufen ueber 64 KB");
        e.ladeadr = a.load;
        e.endadr = a.load + static_cast<int>(n) - 1;
    }
    if (a.set_entry) e.startadr = a.entry;
    return eintragSchreiben(e.platz, eintragBytes(e));
}

bool Bs600FileSystem::reorganize(int* bewegt) {
    if (bewegt) *bewegt = 0;
    Belegung b;
    std::string grund;
    if (!belegungLesen(b, grund)) return fail("BS600: " + grund);
    std::vector<Bs600Eintrag> dat;
    for (const Bs600Eintrag& e : b.eintr) {
        if (!e.gueltig)
            return fail("BS600: Eintrag '" + e.name + "' ist unbrauchbar (" + e.problem + ") - Reorganisieren verweigert");
        dat.push_back(e);
    }
    std::sort(dat.begin(), dat.end(), [](const Bs600Eintrag& x, const Bs600Eintrag& y) { return x.linAnfang() < y.linAnfang(); });
    for (size_t i = 1; i < dat.size(); ++i)
        if (dat[i].linAnfang() <= dat[i - 1].linEnde())
            return fail("BS600: die Dateien '" + dat[i - 1].name + "' und '" + dat[i].name
                        + "' ueberlappen - Reorganisieren verweigert (erst pruefen)");
    // Neue Lage rechnen; nur wenn sich etwas aendert, wird gelesen und geschrieben.
    std::vector<int> neu(dat.size());
    int pos = 0, geaendert = 0;
    for (size_t i = 0; i < dat.size(); ++i) {
        neu[i] = pos;
        pos += dat[i].sektoren();
        if (neu[i] != dat[i].linAnfang()) ++geaendert;
    }
    if (pos > kBs600LetzteNutzSpur * kBs600Sektoren) return fail("BS600: die Dateien passen nicht in Spur 1.." + std::to_string(kBs600LetzteNutzSpur));
    // Tabelle: bei einer lueckenlosen Diskette soll sie dennoch den Eintraegen entsprechen.
    std::vector<uint8_t> tab = b.tab;
    for (int s = 1; s < tracks_; ++s)
        for (int k = 1; k <= kBs600Sektoren; ++k) tabBit(tab, s, k, false);
    for (size_t i = 0; i < dat.size(); ++i) lauf(tab, nullptr, neu[i], dat[i].sektoren(), true);
    if (geaendert == 0) {
        if (tab != b.tab && !tabelleSchreiben(tab)) return false;
        return true;
    }
    // Alles lesen, dann schreiben - sonst ueberschriebe eine Verschiebung Sektoren, die noch gebraucht werden.
    std::vector<std::vector<uint8_t>> inhalt(dat.size());
    for (size_t i = 0; i < dat.size(); ++i)
        for (int j = 0; j < dat[i].sektoren(); ++j) {
            const int lin = dat[i].linAnfang() + j;
            std::vector<uint8_t> s;
            if (!sektorLesen(lin / kBs600Sektoren + 1, lin % kBs600Sektoren + 1, s))
                return fail("BS600: Sektor von '" + dat[i].name + "' nicht lesbar");
            inhalt[i].insert(inhalt[i].end(), s.begin(), s.end());
        }
    for (size_t i = 0; i < dat.size(); ++i) {
        if (neu[i] == dat[i].linAnfang()) continue;
        if (!datenSchreiben(neu[i], inhalt[i], dat[i].sektoren())) return false;
    }
    if (!tabelleSchreiben(tab)) return false;
    for (size_t i = 0; i < dat.size(); ++i) {
        if (neu[i] == dat[i].linAnfang()) continue;
        Bs600Eintrag e = dat[i];
        // Die Sektorzahl VOR dem Umsetzen des Anfangs lesen: sektoren() rechnet aus Anfang und Ende, und mit dem neuen
        // Anfang und dem alten Ende kaeme ein falsches (zu grosses) Ende in den Eintrag.
        const int n = e.sektoren();
        e.anf_spur = neu[i] / kBs600Sektoren + 1; e.anf_sek = neu[i] % kBs600Sektoren + 1;
        const int ende = neu[i] + n - 1;
        e.end_spur = ende / kBs600Sektoren + 1; e.end_sek = ende % kBs600Sektoren + 1;
        if (!eintragSchreiben(e.platz, eintragBytes(e))) return false;
    }
    if (bewegt) *bewegt = geaendert;
    return true;
}

// ─── Anlegen ─────────────────────────────────────────────────────────────────

bool Bs600FileSystem::mkfs() {
    // Kennsatz: Aufbau und Fuellung wie an den Originalabzuegen (Formatdokument §4).
    std::vector<uint8_t> vol(kBs600Sektor0, 0x00);
    const uint8_t kopf[4] = {0xE5, 0xD6, 0xD3, 0xF1};
    std::copy(kopf, kopf + 4, vol.begin());
    std::string name = label_.empty() ? "BS600" : label_;
    name.resize(6, ' ');
    for (size_t i = 0; i < 6; ++i) vol[4 + i] = static_cast<uint8_t>(name[i]);
    vol[10] = 0x20;
    vol[11] = 'P'; vol[12] = 'R'; vol[13] = 'G';
    const std::string d = heute();
    for (size_t i = 0; i < 6; ++i) vol[14 + i] = static_cast<uint8_t>(d[i]);
    std::fill(vol.begin() + 20, vol.begin() + 37, 0x40);
    std::fill(vol.begin() + 37, vol.begin() + 40, 0x20);          // Signum des Besitzers: leer
    std::fill(vol.begin() + 40, vol.begin() + 67, 0x20);
    std::fill(vol.begin() + 67, vol.begin() + 79, 0x40);
    vol[70] = 0xD4; vol[75] = 0xF1; vol[79] = 0x20;
    if (!space_.writeSector(0, 0, kVol1Sektor, vol)) return fail(space_.lastError());
    for (int s = 0; s < kBs600Eintraege / kEintraegeJeSektor; ++s)
        if (!space_.writeSector(0, 0, static_cast<uint8_t>(kVerzSektor0 + s), std::vector<uint8_t>(kBs600Sektor0, 0x20)))
            return fail(space_.lastError());
    return tabelleSchreiben(std::vector<uint8_t>(kBs600Sektor0, 0x00));
}

std::unique_ptr<Bs600FileSystem> Bs600FileSystem::format(SectorSpace& space, const FsProfile& prof,
                                                         const std::string& label, std::string& err) {
    if (space.sectorsPerTrack(0, 0) != 26 || space.sectorSize(0, 0) != kBs600Sektor0
        || space.sectorsPerTrack(1, 0) != kBs600Sektoren || space.sectorSize(1, 0) != kBs600Sektor) {
        err = "BS600: Geometrie ist nicht 26 x 128 B (Spur 0) + 15 x 256 B";
        return nullptr;
    }
    const std::string name = upper(label);
    if (name.size() > 6 || std::any_of(name.begin(), name.end(), [](char c) { return c < 0x20 || c > 0x7E; })) {
        err = "BS600: Datentraegername '" + label + "' ungueltig: hoechstens 6 sichtbare ASCII-Zeichen";
        return nullptr;
    }
    std::unique_ptr<Bs600FileSystem> fs(new Bs600FileSystem(space, prof));
    fs->label_ = rtrim(name);
    fs->tracks_ = spurenVon(space);
    if (!fs->mkfs()) { err = fs->lastError(); return nullptr; }
    return fs;
}

// ─── Platzpruefung, Zustand ──────────────────────────────────────────────────

bool Bs600FileSystem::wouldFit(const std::vector<PlannedFile>& files, FitReport& out) const {
    out = FitReport{};
    Belegung b;
    std::string grund;
    if (!belegungLesen(b, grund)) { out.detail = "BS600: " + grund; return true; }
    out.available = static_cast<uint64_t>(freieNutzSektoren(b.belegt)) * kBs600Sektor;
    int frei_plaetze = 0;
    for (uint8_t p : b.platz_belegt) if (!p) ++frei_plaetze;
    out.dir_free = frei_plaetze;
    for (const PlannedFile& f : files) {
        const std::string name = upper(f.name);
        if (!nameGueltig(name)) { out.detail = "'" + f.name + "': Name ungueltig (1 bis 6 Zeichen, kein '.')"; return true; }
        if (f.size == 0) { out.detail = "'" + f.name + "': eine leere Datei laesst sich nicht ablegen"; return true; }
        // Im Stapel ersetzt der Ordner den Bestand (insertAll setzt overwrite).
        bool ersetzt = false;
        for (const Bs600Eintrag& e : b.eintr)
            if (upper(e.name) == name) {
                ersetzt = true;
                if (e.gueltig) lauf(b.tab, &b.belegt, e.linAnfang(), e.sektoren(), false);
                ++out.dir_free;       // der Platz wird wiederverwendet
                break;
            }
        if (!ersetzt) {
            if (out.dir_free - out.dir_needed <= 0) { out.detail = "das Verzeichnis ist voll (54 Dateien)"; return true; }
            ++out.dir_needed;
        }
        const int n = static_cast<int>((f.size + kBs600Sektor - 1) / kBs600Sektor);
        const int lin = findeLauf(b.belegt, n);
        if (lin < 0) {
            out.detail = "'" + f.name + "': kein zusammenhaengender freier Bereich von " + std::to_string(n) + " Sektoren ("
                       + std::to_string(freieNutzSektoren(b.belegt)) + " frei; Dateien liegen unfragmentiert - ggf. reorganisieren)";
            return true;
        }
        lauf(b.tab, &b.belegt, lin, n, true);
        out.needed += static_cast<uint64_t>(n) * kBs600Sektor;
    }
    out.fits = true;
    return true;
}

FsInfo Bs600FileSystem::info() const {
    FsInfo i;
    i.label = label_;
    Belegung b;
    std::string grund;
    i.total_bytes = static_cast<uint64_t>(kBs600LetzteNutzSpur) * kBs600Sektoren * kBs600Sektor;
    if (!belegungLesen(b, grund)) { i.warnings.push_back(grund); i.free_bytes = i.total_bytes; return i; }
    i.free_bytes = static_cast<uint64_t>(freieNutzSektoren(b.belegt)) * kBs600Sektor;
    i.used_bytes = i.total_bytes - i.free_bytes;
    i.files = static_cast<int>(b.eintr.size());
    for (const Bs600Eintrag& e : b.eintr)
        if (!e.gueltig) i.warnings.push_back("Eintrag '" + e.name + "': " + e.problem);
    return i;
}
