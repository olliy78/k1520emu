/**
 * @file fileinfo.cpp
 * @brief Umsetzung der `.fileinfo`-Angabendatei, s. fileinfo.h.
 */

#include "core/filesystem/fileinfo.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace k1520::fileinfo {


std::string hex16(uint16_t v) {
    char t[8];
    std::snprintf(t, sizeof t, "%04X", v);
    return t;
}

/// @brief Leerzeichen im Erstellungsvermerk ("V 4.3 ") sind im Beiblatt `_`.
std::string ohneLeer(std::string t) {
    for (char& c : t) if (c == ' ') c = '_';
    return t;
}
std::string mitLeer(std::string t) {
    for (char& c : t) if (c == '_') c = ' ';
    return t;
}


std::string cpmAttrText(const CpmAngabe& a) {
    std::string t;
    if (a.read_only) t += 'R';
    if (a.system)    t += 'S';
    if (a.archived)  t += 'A';
    return t.empty() ? "-" : t;
}


std::string modeOktal(uint16_t m) {
    char t[16];
    std::snprintf(t, sizeof t, "0%06o", static_cast<unsigned>(m));
    return t;
}

UnixAngabe unixAus(const FileEntry& e) {
    UnixAngabe a;
    a.art     = e.type.empty() ? "-" : e.type;
    a.mode    = e.unix_mode;
    a.mode_da = true;
    a.uid     = e.unix_uid;
    a.gid     = e.unix_gid;
    a.owner_da = true;
    a.mtime   = e.unix_mtime;
    a.atime   = e.unix_atime;
    a.ctime   = e.unix_ctime;
    a.nlink   = e.unix_nlink;
    a.inode   = e.unix_inode;
    if (e.type == "c" || e.type == "b")
        a.rdev = std::to_string(e.unix_rdev >> 8) + "," + std::to_string(e.unix_rdev & 0xFF);
    return a;
}

/// @brief Die Zeilen `mode=` … `rdev=` — gemeinsam fuer `.fileinfo` und Beiblatt.
void schreibeUnixFelder(std::ostream& f, const UnixAngabe& a, char trenner) {
    f << "art=" << a.art << trenner
      << "mode=" << modeOktal(a.mode) << trenner
      << "uid=" << a.uid << trenner
      << "gid=" << a.gid << trenner
      << "nlink=" << a.nlink << trenner
      << "inode=" << a.inode << trenner
      << "mtime=" << a.mtime << trenner
      << "atime=" << a.atime << trenner
      << "ctime=" << a.ctime;
    if (!a.rdev.empty()) f << trenner << "rdev=" << a.rdev;
    f << '\n';
}

/// @brief Einen Schluessel dieser Familie in @p a eintragen; false = kein WEGA-Schluessel.
bool leseUnixFeld(UnixAngabe& a, const std::string& k, const std::string& v) {
    auto zahl = [](const std::string& t, int basis) -> unsigned long {
        return std::strtoul(t.c_str(), nullptr, basis);
    };
    if      (k == "art")   a.art = v.empty() ? "-" : v;
    else if (k == "mode")  { a.mode = static_cast<uint16_t>(zahl(v, 8)); a.mode_da = true; }
    else if (k == "uid")   { a.uid = static_cast<int>(zahl(v, 10)); a.owner_da = true; }
    else if (k == "gid")   { a.gid = static_cast<int>(zahl(v, 10)); a.owner_da = true; }
    else if (k == "mtime") a.mtime = static_cast<uint32_t>(zahl(v, 10));
    else if (k == "atime") a.atime = static_cast<uint32_t>(zahl(v, 10));
    else if (k == "ctime") a.ctime = static_cast<uint32_t>(zahl(v, 10));
    else if (k == "nlink") a.nlink = static_cast<int>(zahl(v, 10));
    else if (k == "inode") a.inode = static_cast<uint32_t>(zahl(v, 10));
    else if (k == "rdev")  a.rdev = v;
    else return false;
    return true;
}


/// @brief Ist @p name ein `.fileinfo`?  (Endungsvergleich ohne Ruecksicht auf Gross/Klein)
bool istFileinfo(const fs::path& name) {
    std::string e = name.extension().string();
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e == kFileinfoEndung;
}

/// @brief Der Pfad des `.fileinfo` zu @p datei — schlicht die Endung angehaengt,
///        damit beide im Ordner nebeneinander stehen (`ACTIVATE` · `ACTIVATE.fileinfo`).
fs::path fileinfoPfad(const fs::path& datei) {
    return fs::path(datei.string() + kFileinfoEndung);
}

/// @brief Ein `.fileinfo` schreiben — dieselben Schluesselwoerter wie das
///        Sammelbeiblatt, nur EINES JE ZEILE.
bool schreibeFileinfo(const fs::path& ziel, const FileAngaben& a) {
    std::ofstream f(ziel, std::ios::binary);
    if (!f) return false;
    f << "# k1520DiskTool - Angaben zu '" << a.name << "'.\n"
         "# Sie stehen NICHT in der Datei selbst.  Beim Einfuegen (`put`) werden sie\n"
         "# wieder uebernommen.\n";
    // Der Satz danach sagt, WAS ohne sie verlorenginge — und das ist bei den beiden
    // Familien sehr Verschiedenes: bei UDOS die Lauffaehigkeit, bei CP/M der
    // Nutzerbereich.  Ein gemeinsamer Satz waere fuer eine der beiden Seiten falsch.
    if (a.tar)
        f << "# Modus, Besitzer und Zeit stammen aus dem tar-Kopf.  Beim Einfuegen auf eine\n"
             "# WEGA-Diskette werden mode (untere 12 Bit), uid, gid und mtime uebernommen.\n"
             "# teilstueck=ist/soll: die Datei war auf dieser Diskette abgeschnitten.\n";
    else if (a.wega)
        f << "# Ohne sie bekommt die Datei beim Einfuegen die Vorgaben (Modus 0644 bzw.\n"
             "# 0755, Besitzer 0/0, Zeit jetzt) - Inhalt und Name gehen nicht verloren.\n"
             "# Zurueckgespielt werden mode (untere 12 Bit), uid, gid und mtime; art,\n"
             "# nlink, inode, atime, ctime und rdev stehen nur zur Auskunft da.\n";
    else if (a.udos)
        f << "# Ohne sie kann das Werkzeug nicht wissen, was fuer eine Datei das ist -\n"
             "# aus einem Programm wuerde eine Binaerdatei, die nicht laeuft.\n";
    else
        f << "# Ohne sie landet die Datei im Nutzerbereich 0 ohne Attribute - das ist\n"
             "# bei CP/M der Normalfall und selten ein Verlust.\n";
    f << "# Loeschen schadet nur dieser einen Datei; die Zeilenform ist dieselbe wie in\n"
         "# udos-dateiangaben.txt bzw. cpm-dateiangaben.txt.\n";
    f << "fs=" << (a.tar ? "tar" : a.wega ? "wega_mutos" : a.udos ? "udos" : "cpm") << '\n';
    f << "name=" << a.name << '\n';
    if (a.wega || a.tar) {
        schreibeUnixFelder(f, a.x, '\n');
        if (!a.teil.empty()) f << "teilstueck=" << a.teil << '\n';
    } else if (a.udos) {
        f << "typ=" << (a.u.typ.empty() ? "-" : a.u.typ) << '\n'
          << "eig=" << (a.u.eigenschaften.empty() ? "-" : a.u.eigenschaften) << '\n'
          << "start=" << hex16(a.u.start) << '\n'
          << "satz=" << a.u.satzlaenge << '\n'
          << "block=" << a.u.blocklaenge << '\n'
          << "rest=" << a.u.rest << '\n'
          << "segment=" << hex16(a.u.ladeadresse) << ':' << a.u.abbildlaenge << '\n'
          << "mem=" << hex16(a.u.speicher_von) << ':' << hex16(a.u.speicher_bis)
          << ':' << hex16(a.u.speicher_kz) << '\n'
          << "zusatz=" << std::hex << std::uppercase << a.u.zusatz
          << std::dec << std::nouppercase << '\n';
        // Nur bei MEHR als einem Segment — sonst sagt die Zeile nichts, was
        // `segment=` nicht schon sagt (`ZLINK` der PC-1715-Diskette hat sechs).
        if (!a.u.segmente.empty()) f << "segs=" << a.u.segmente << '\n';
        f << "erst=" << (a.u.erstellt.empty() ? "-" : ohneLeer(a.u.erstellt)) << '\n'
          << "geaend=" << (a.u.geaendert.empty() ? "-" : ohneLeer(a.u.geaendert)) << '\n';
    } else {
        f << "attr=" << cpmAttrText(a.c) << '\n';
    }
    return static_cast<bool>(f);
}

/// @brief Ein `.fileinfo` lesen; gibt es keines, kommt `false` (kein Fehler).
bool leseFileinfo(const fs::path& datei, FileAngaben& out) {
    std::ifstream f(datei);
    if (!f) return false;
    auto zahl = [](const std::string& t, int basis) -> unsigned long {
        return std::strtoul(t.c_str(), nullptr, basis);
    };
    FileAngaben a;
    bool etwas = false;
    std::string zeile;
    while (std::getline(f, zeile)) {
        if (!zeile.empty() && zeile.back() == '\r') zeile.pop_back();
        if (zeile.empty() || zeile[0] == '#') continue;
        const size_t g = zeile.find('=');
        if (g == std::string::npos) continue;
        const std::string k = zeile.substr(0, g), v = zeile.substr(g + 1);
        etwas = true;
        if      (k == "fs")   { a.wega = (v == "wega_mutos" || v == "wega");   // "wega": aeltere .fileinfo
                                a.tar  = (v == "tar");
                                a.udos = (v != "cpm" && !a.wega && !a.tar); }
        else if (k == "name")   a.name = v;
        else if (leseUnixFeld(a.x, k, v)) {}
        else if (k == "attr") { if (v != "-") {
                                    a.c.read_only = v.find('R') != std::string::npos;
                                    a.c.system    = v.find('S') != std::string::npos;
                                    a.c.archived  = v.find('A') != std::string::npos; } }
        else if (k == "typ")    a.u.typ = (v == "-") ? "" : v;
        else if (k == "eig")    a.u.eigenschaften = (v == "-") ? "" : v;
        else if (k == "start")  a.u.start = static_cast<uint16_t>(zahl(v, 16));
        else if (k == "satz")   a.u.satzlaenge = static_cast<uint16_t>(zahl(v, 10));
        else if (k == "block") { a.u.blocklaenge = static_cast<uint16_t>(zahl(v, 10));
                                 a.u.blocklaenge_gesetzt = true; }
        else if (k == "rest")  { a.u.rest = static_cast<uint16_t>(zahl(v, 10));
                                 a.u.rest_gesetzt = true; }
        else if (k == "segment") {
            const size_t d = v.find(':');
            a.u.ladeadresse = static_cast<uint16_t>(zahl(v.substr(0, d), 16));
            if (d != std::string::npos)
                a.u.abbildlaenge = static_cast<uint16_t>(zahl(v.substr(d + 1), 10));
        }
        else if (k == "zusatz") a.u.zusatz = static_cast<uint32_t>(zahl(v, 16));
        else if (k == "segs")   a.u.segmente = v;
        else if (k == "erst")   a.u.erstellt = (v == "-") ? "" : mitLeer(v);
        else if (k == "geaend") a.u.geaendert = (v == "-") ? "" : mitLeer(v);
        else if (k == "mem") {
            std::string x = v;
            std::replace(x.begin(), x.end(), ':', ' ');
            std::istringstream m(x);
            std::string p1, p2, p3;
            m >> p1 >> p2 >> p3;
            a.u.speicher_von = static_cast<uint16_t>(zahl(p1, 16));
            a.u.speicher_bis = static_cast<uint16_t>(zahl(p2, 16));
            a.u.speicher_kz  = static_cast<uint16_t>(zahl(p3, 16));
            a.u.mem_gesetzt  = true;
        }
    }
    if (!etwas) return false;
    out = a;
    return true;
}

}  // namespace k1520::fileinfo
