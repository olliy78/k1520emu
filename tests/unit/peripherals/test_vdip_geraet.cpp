/**
 * @file test_vdip_geraet.cpp
 * @brief VDIP1/VDAP-Firmware ueber Wirtsverzeichnis (Entwurf 50 AP U1): Sync, Befehle nach dem Ablauf der
 *        UTools (UDIR/UGET/UPUT/USB), Fehlerfaelle und vor allem die Pfad-Sandbox.
 */
#include "core/peripherals/vdip/vdip_geraet.h"
#include "tests/support/temp_path.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;
using k1520::VdipGeraet;
using k1520::VdipStick;

namespace {

std::string cr() { return "\r"; }
std::string by(std::initializer_list<int> l) {
    std::string s;
    for (int v : l) s.push_back(static_cast<char>(v));
    return s;
}
const std::string SCS = by({0x10}), ECS = by({0x11}), IPH = by({0x91}), IPA = by({0x90});
const std::string OP_DIR = by({0x01}), OP_DLF = by({0x07}), OP_WRF = by({0x08}), OP_OPW = by({0x09}),
                  OP_CLF = by({0x0A}), OP_RDF = by({0x0B}), OP_OPR = by({0x0E}), OP_DIRT = by({0x2F}),
                  OP_SEK = by({0x28});
const std::string OK_K = ">\r", OK_L = "D:\\>\r";

std::string num(uint32_t n) { return by({int(n >> 24), int((n >> 16) & 255), int((n >> 8) & 255), int(n & 255)}); }

std::string muster(size_t n) {
    std::string s(n, '\0');
    for (size_t i = 0; i < n; ++i) s[i] = static_cast<char>((i * 131 + 7 + (i >> 8)) & 0xFF);
    return s;
}

std::string lies(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream o;
    o << f.rdbuf();
    return o.str();
}
void schreib(const fs::path& p, const std::string& d) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << d;
}

// Temp-Welt: <tmp>/<wurzel>/stick (der "USB-Stick") und <tmp>/<wurzel>/aussen (Waechter, nie anzufassen)
struct Welt {
    fs::path basis, stick, aussen;
    Welt() {
        const std::string n = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        basis = k1520test::tempPath("k1520_vdip_" + n);
        std::error_code ec;
        fs::remove_all(basis, ec);
        stick = basis / "stick";
        aussen = basis / "AUSSEN";   // GROSS, damit die Fluchtnamen (../AUSSEN/GUARD.TXT) wirklich treffen
        fs::create_directories(stick);
        fs::create_directories(aussen);
        schreib(aussen / "GUARD.TXT", "WAECHTER");
        fs::create_directories(aussen / "SUB");
        schreib(aussen / "SUB" / "X.TXT", "X");
    }
    ~Welt() {
        std::error_code ec;
        fs::remove_all(basis, ec);
    }
    // Schnappschuss von allem ausserhalb des Sticks (Name -> Inhalt/Verzeichnis)
    std::map<std::string, std::string> aussenAbbild() const {
        std::map<std::string, std::string> m;
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(basis, ec); !ec && it != fs::recursive_directory_iterator();
             it.increment(ec)) {
            const fs::path rel = fs::relative(it->path(), basis, ec);
            if (rel.begin()->string() == "stick") continue;
            std::error_code e2;
            const bool d = fs::is_directory(it->path(), e2);
            m[rel.generic_string()] = d ? "<dir>" : lies(it->path());
        }
        return m;
    }
};

struct Gast {
    VdipGeraet g;
    explicit Gast(const VdipGeraet::Config& c = {}) : g(c) {}
    void tx(const std::string& s) { for (unsigned char c : s) g.senden(c); }
    std::string rx() {
        std::string r;
        while (g.datenBereit()) r.push_back(static_cast<char>(g.empfangen()));
        return r;
    }
    std::string cmd(const std::string& s) { tx(s + cr()); return rx(); }
    // Vorbereitung wie UDIR/UGET/UPUT: Banner abholen, SCS, IPH, Stickpruefung
    void scsIph() {
        rx();
        EXPECT_EQ(cmd(SCS), OK_K);
        EXPECT_EQ(cmd(IPH), OK_K);
        EXPECT_EQ(cmd(""), OK_K);
    }
    // UPUT: OPW, WRF in 128er-Bloecken, CLF
    void hochladen(const std::string& name, const std::string& d, const std::string& stempel = "") {
        ASSERT_EQ(cmd(OP_OPW + " " + name + (stempel.empty() ? "" : " " + stempel)), OK_K);
        for (size_t o = 0; o < d.size(); o += 128) {
            const size_t n = std::min<size_t>(128, d.size() - o);
            tx(OP_WRF + " " + num(uint32_t(n)) + cr() + d.substr(o, n));
            ASSERT_EQ(rx(), OK_K) << "Block bei " << o;
        }
        ASSERT_EQ(cmd(OP_CLF + " " + name), OK_K);
    }
    // UGET: DIR name (Groesse), OPR, RDF-Schleife, CLF
    std::string herunter(const std::string& name) {
        const std::string a = cmd(OP_DIR + " " + name);
        EXPECT_GE(a.size(), name.size() + 1 + 4 + 1 + 1 + 2);
        uint32_t groesse = 0;
        const size_t sp = a.find(' ');
        for (int i = 3; i >= 0; --i) groesse = (groesse << 8) | uint8_t(a[sp + 1 + i]);
        EXPECT_EQ(cmd(OP_OPR + " " + name), OK_K);
        std::string d;
        for (uint32_t rest = groesse; rest > 0;) {
            const uint32_t n = std::min<uint32_t>(128, rest);
            const std::string r = cmd(OP_RDF + " " + num(n));
            EXPECT_EQ(r.size(), n + 2u);
            if (r.size() != n + 2u) break;
            EXPECT_EQ(r.substr(n), OK_K);
            d += r.substr(0, n);
            rest -= n;
        }
        EXPECT_EQ(cmd(OP_CLF + " " + name), OK_K);
        return d;
    }
};

}  // namespace

// ------------------------------------------------------------------------------ Start und Sync

TEST(VdipGeraet, StartmeldungMitStick) {
    Welt w;
    Gast v({w.stick, false});
    EXPECT_TRUE(v.g.datenBereit());
    EXPECT_TRUE(v.g.sendenBereit());
    EXPECT_EQ(v.rx(), "Ver03.68VDAPF On-Line:\rDevice Detected P2\rNo Upgrade\rD:\\>\r");
    EXPECT_FALSE(v.g.datenBereit());
}

TEST(VdipGeraet, StartmeldungOhneStick) {
    Gast v;
    EXPECT_EQ(v.rx(), "Ver03.68VDAPF On-Line:\rNo Upgrade\rNo Disk\r");
    EXPECT_FALSE(v.g.stickVorhanden());
}

TEST(VdipGeraet, SyncWieUtools) {
    Welt w;
    Gast v({w.stick, false});
    // Banner liegt an: die UTools holen ihn ab (synchr: get, solange RXF# aktiv)
    EXPECT_FALSE(v.rx().empty());
    // CR -> irgendeine Zeile mit CR (syn3)
    const std::string a = v.cmd("");
    ASSERT_FALSE(a.empty());
    EXPECT_EQ(a.back(), '\r');
    // E CR -> E CR, ohne Prompt (sonst scheitert das folgende SCS-EXEC)
    EXPECT_EQ(v.cmd("E"), "E\r");
    EXPECT_EQ(v.cmd("e"), "e\r");
    EXPECT_FALSE(v.g.datenBereit());
    // Moduswechsel antwortet genau mit dem Prompt des neuen Modus
    EXPECT_EQ(v.cmd(SCS), ">\r");
    EXPECT_TRUE(v.g.kurzSatz());
    EXPECT_EQ(v.cmd(IPH), ">\r");
    EXPECT_TRUE(v.g.binaer());
    EXPECT_EQ(v.cmd(""), ">\r");
    // Ausstieg wie exit: ECS CR (Antwort bleibt anstehen, naechster Sync holt sie ab)
    EXPECT_EQ(v.cmd(ECS), "D:\\>\r");
    EXPECT_FALSE(v.g.kurzSatz());
}

TEST(VdipGeraet, EchoMitHalbfertigerZeileUndUnbekannt) {
    Welt w;
    Gast v({w.stick, false});
    v.rx();
    EXPECT_EQ(v.cmd("XYZ"), "Bad Command\r");
    EXPECT_EQ(v.cmd(by({0x77})), "Bad Command\r");
    EXPECT_EQ(v.cmd("dir"), "Bad Command\r");   // Mnemonics sind Grossbuchstaben
    EXPECT_EQ(v.cmd("FWV"), "MAIN 03.68VDAPF\rD:\\>\r");
    v.cmd(SCS);
    EXPECT_EQ(v.cmd("XYZ"), "BC\r");
}

TEST(VdipGeraet, ResetSchliesstDateiUndLiefertBanner) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    ASSERT_EQ(v.cmd(OP_OPW + " A.TXT"), OK_K);
    v.g.reset();
    EXPECT_FALSE(v.g.kurzSatz());
    EXPECT_NE(v.rx().find("On-Line:"), std::string::npos);
    EXPECT_EQ(v.cmd("OPR A.TXT"), "D:\\>\r");   // schliessen hat geklappt, Datei war angelegt
    EXPECT_TRUE(fs::exists(w.stick / "A.TXT"));
}

// ------------------------------------------------------------------------------ DIR / DIRT

TEST(VdipGeraet, DirLeerUndGefuellt) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    EXPECT_EQ(v.cmd(OP_DIR), "\r>\r");
    schreib(w.stick / "b.txt", "12345");
    schreib(w.stick / "README", "");
    fs::create_directories(w.stick / "Sub");
    schreib(w.stick / "zuLangerName.txt", "x");   // kein 8.3 -> unsichtbar
    schreib(w.stick / "A B.TXT", "x");            // Leerzeichen -> kein 8.3
    EXPECT_EQ(v.cmd(OP_DIR), "\rB.TXT\rREADME\rSUB DIR\r>\r");
    // Textform im ECS
    v.cmd(ECS);
    EXPECT_EQ(v.cmd("DIR"), "\rB.TXT\rREADME\rSUB DIR\rD:\\>\r");
}

TEST(VdipGeraet, DirNameGroesseLsbUndFehlend) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    schreib(w.stick / "b.txt", muster(0x12345));
    EXPECT_EQ(v.cmd(OP_DIR + " B.TXT"), "\rB.TXT " + by({0x45, 0x23, 0x01, 0x00}) + "\r>\r");
    EXPECT_EQ(v.cmd(OP_DIR + " NIX.TXT"), "\rCF\r");   // UGET: CR, dann CR vor Leerzeichen = Fehler
    schreib(w.stick / "README", "abc");
    EXPECT_EQ(v.cmd(OP_DIR + " README."), "\rREADME " + by({3, 0, 0, 0}) + "\r>\r");   // outnam sendet `NAME.`
    fs::create_directories(w.stick / "SUB");
    EXPECT_EQ(v.cmd(OP_DIR + " SUB"), "\rCF\r");   // Verzeichnis: keine Groesse
    EXPECT_EQ(v.cmd(OP_DIR + " b.txt"), "\rB.TXT " + by({0x45, 0x23, 0x01, 0x00}) + "\r>\r");   // Gross/klein egal
}

TEST(VdipGeraet, DirNameImAsciiModus) {
    Welt w;
    Gast v({w.stick, false});
    v.rx();
    v.cmd(SCS);
    schreib(w.stick / "b.txt", muster(1234));
    EXPECT_EQ(v.cmd(OP_DIR + " B.TXT"), "\rB.TXT 1234\r>\r");   // IPA-Zahlenform [?]
}

TEST(VdipGeraet, DirtDatumUndZeit) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    schreib(w.stick / "D.TXT", "x");
    // 2011-05-03 12:34:56 -> Datum 0x3EA3, Zeit 0x645C, Dword 0x3EA3645C
    fs::file_time_type t;
    ASSERT_TRUE(VdipStick::dosZuZeit(0x3EA3645Cu, t));
    fs::last_write_time(w.stick / "D.TXT", t);
    const std::string e = by({0x5C, 0x64, 0xA3, 0x3E});
    EXPECT_EQ(v.cmd(OP_DIRT + " D.TXT"), "\rD.TXT " + e + by({0xA3, 0x3E}) + e + "\r>\r");
    EXPECT_EQ(v.cmd(OP_DIRT + " NIX.TXT"), "\rCF\r");
}

TEST(VdipGeraet, ZeitUmrechnung) {
    for (uint32_t dos : {0x00210000u, 0x3EA3645Cu, 0x3F0D6000u, 0xFF9FBF7Du}) {
        fs::file_time_type t;
        ASSERT_TRUE(VdipStick::dosZuZeit(dos, t)) << std::hex << dos;
        EXPECT_EQ(VdipStick::zeitZuDos(t), dos) << std::hex << dos;
    }
    fs::file_time_type t;
    EXPECT_FALSE(VdipStick::dosZuZeit(0x00000000u, t));   // Monat 0
    EXPECT_FALSE(VdipStick::dosZuZeit(0x3EA3FFFFu, t));   // Stunde 31
}

// ------------------------------------------------------------------------------ Schreiben und Lesen

TEST(VdipGeraet, SchreibenWieUput_UndPruefungGegenWirtsdatei) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    for (size_t n : {size_t(0), size_t(1), size_t(100), size_t(141) /*letzter Block = 13 = CR*/, size_t(512),
                     size_t(513), size_t(70000)}) {
        const std::string d = muster(n);
        v.hochladen("f" + std::to_string(n) + ".bin", d);
        EXPECT_EQ(lies(w.stick / ("F" + std::to_string(n) + ".BIN")), d) << n;   // in GROSS angelegt
    }
}

TEST(VdipGeraet, LesenWieUget_UndPruefungGegenWirtsdatei) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    for (size_t n : {size_t(0), size_t(1), size_t(100), size_t(141), size_t(512), size_t(513), size_t(70000)}) {
        const std::string d = muster(n);
        const std::string name = "g" + std::to_string(n) + ".dat";
        schreib(w.stick / name, d);
        EXPECT_EQ(v.herunter(name), d) << n;
    }
}

TEST(VdipGeraet, EinGrosserWrfUndRdfInEinemStueck) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    const std::string d = muster(10000);
    ASSERT_EQ(v.cmd(OP_OPW + " BIG.BIN"), OK_K);
    v.tx(OP_WRF + " " + num(10000) + cr() + d);
    EXPECT_EQ(v.rx(), OK_K);
    EXPECT_EQ(v.cmd(OP_WRF + " " + num(0)), OK_K);   // Laenge 0
    EXPECT_EQ(v.cmd(OP_CLF + " BIG.BIN"), OK_K);
    EXPECT_EQ(lies(w.stick / "BIG.BIN"), d);
    ASSERT_EQ(v.cmd(OP_OPR + " BIG.BIN"), OK_K);
    EXPECT_EQ(v.cmd(OP_RDF + " " + num(10000)), d + OK_K);
    // ueber das Ende hinaus: nur der Rest [?]
    EXPECT_EQ(v.cmd(OP_RDF + " " + num(5)), OK_K);
    EXPECT_EQ(v.cmd(OP_CLF + " BIG.BIN"), OK_K);
}

TEST(VdipGeraet, OpwHaengtAnVorhandeneDateiAn) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    schreib(w.stick / "AP.TXT", "alt");
    v.hochladen("AP.TXT", "neu");
    EXPECT_EQ(lies(w.stick / "AP.TXT"), "altneu");
}

TEST(VdipGeraet, OpwMitDatumsstempelEnthaeltCrByte) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    // 2011-08-13 12:00:00: Datum 0x3F0D, Zeit 0x6000; Wire = MSB zuerst: 3F 0D 60 00 (enthaelt 0Dh!)
    v.hochladen("ST.TXT", "abc", by({0x3F, 0x0D, 0x60, 0x00}));
    EXPECT_EQ(lies(w.stick / "ST.TXT"), "abc");
    const std::string e = by({0x00, 0x60, 0x0D, 0x3F});
    EXPECT_EQ(v.cmd(OP_DIRT + " ST.TXT"), "\rST.TXT " + e + by({0x0D, 0x3F}) + e + "\r>\r");
}

TEST(VdipGeraet, SekUndAsciiParameter) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    const std::string d = muster(1000);
    schreib(w.stick / "S.BIN", d);
    ASSERT_EQ(v.cmd(OP_OPR + " S.BIN"), OK_K);
    EXPECT_EQ(v.cmd(OP_SEK + " " + num(500)), OK_K);
    EXPECT_EQ(v.cmd(OP_RDF + " " + num(10)), d.substr(500, 10) + OK_K);
    EXPECT_EQ(v.cmd(OP_SEK + " " + num(1001)), "CF\r");
    EXPECT_EQ(v.cmd(OP_SEK + " " + num(1000)), OK_K);
    EXPECT_EQ(v.cmd(OP_RDF + " " + num(10)), OK_K);
    // IPA: Dezimalzahlen
    ASSERT_EQ(v.cmd(by({0x90})), OK_K);
    EXPECT_EQ(v.cmd(OP_SEK + " 990"), OK_K);
    EXPECT_EQ(v.cmd(OP_RDF + " 10"), d.substr(990, 10) + OK_K);
    EXPECT_EQ(v.cmd(OP_RDF + " abc"), "FI\r");
    // Schreibseite: SEK im zum Schreiben geoeffneten Stueck
    EXPECT_EQ(v.cmd(OP_CLF + " S.BIN"), OK_K);
    ASSERT_EQ(v.cmd(OP_OPW + " S.BIN"), OK_K);
    EXPECT_EQ(v.cmd(OP_SEK + " 2"), OK_K);
    v.tx(OP_WRF + " 3\r" + "xyz");
    EXPECT_EQ(v.rx(), OK_K);
    EXPECT_EQ(v.cmd(OP_CLF + " S.BIN"), OK_K);
    std::string e = d;
    e.replace(2, 3, "xyz");
    EXPECT_EQ(lies(w.stick / "S.BIN"), e);
}

// ------------------------------------------------------------------------------ Verzeichnisse und Dateien

TEST(VdipGeraet, MkdCdCdPunktPunktUndDateiImUnterverzeichnis) {
    Welt w;
    Gast v({w.stick, false});
    v.rx();
    EXPECT_EQ(v.cmd("MKD sub"), OK_L);
    EXPECT_TRUE(fs::is_directory(w.stick / "SUB"));
    EXPECT_EQ(v.cmd("MKD SUB"), "Command Failed\r");
    EXPECT_EQ(v.cmd("CD SUB"), OK_L);
    EXPECT_EQ(v.g.stick().verzeichnisAnzeige(), "\\SUB");
    EXPECT_EQ(v.cmd("DIR"), "\r. DIR\r.. DIR\rD:\\>\r");
    v.cmd(SCS);
    EXPECT_EQ(v.cmd(OP_OPW + " in.txt"), OK_K);
    EXPECT_EQ(v.cmd(OP_CLF + " in.txt"), OK_K);
    EXPECT_TRUE(fs::exists(w.stick / "SUB" / "IN.TXT"));
    EXPECT_FALSE(fs::exists(w.stick / "IN.TXT"));
    EXPECT_EQ(v.cmd(OP_DIR), "\r. DIR\r.. DIR\rIN.TXT\r>\r");
    v.cmd(ECS);
    EXPECT_EQ(v.cmd("CD .."), OK_L);
    EXPECT_EQ(v.g.stick().verzeichnisAnzeige(), "\\");
    EXPECT_EQ(v.cmd("CD .."), "Command Failed\r");   // nie ueber die Wurzel
    EXPECT_EQ(v.cmd("CD SUB"), OK_L);
    EXPECT_EQ(v.cmd("CD /"), OK_L);
    EXPECT_EQ(v.g.stick().verzeichnisAnzeige(), "\\");
    EXPECT_EQ(v.cmd("CD NIX"), "Command Failed\r");
    EXPECT_EQ(v.cmd("CD"), "Command Failed\r");
    EXPECT_EQ(v.cmd("MKD"), "Filename Invalid\r");
}

TEST(VdipGeraet, MkdMitDatum) {
    Welt w;
    Gast v({w.stick, false});
    v.rx();
    v.tx("MKD DD " + by({0x3E, 0xA3, 0x64, 0x5C}) + cr());
    EXPECT_EQ(v.rx(), OK_L);
    v.cmd(SCS);
    v.cmd(IPH);
    const std::string e = by({0x5C, 0x64, 0xA3, 0x3E});
    EXPECT_EQ(v.cmd(OP_DIRT + " DD"), "\rDD " + e + by({0xA3, 0x3E}) + e + "\r>\r");
}

TEST(VdipGeraet, DldUndNichtLeer) {
    Welt w;
    Gast v({w.stick, false});
    v.rx();
    v.cmd("MKD D1");
    schreib(w.stick / "D1" / "F.TXT", "x");
    v.cmd("MKD D2");
    EXPECT_EQ(v.cmd("DLD D1"), "Dir Not Empty\r");
    EXPECT_TRUE(fs::exists(w.stick / "D1" / "F.TXT"));
    EXPECT_EQ(v.cmd("DLD D2"), OK_L);
    EXPECT_FALSE(fs::exists(w.stick / "D2"));
    EXPECT_EQ(v.cmd("DLD D2"), "Command Failed\r");
    EXPECT_EQ(v.cmd("DLD D1/F.TXT"), "Filename Invalid\r");
    schreib(w.stick / "FILE.TXT", "x");
    EXPECT_EQ(v.cmd("DLD FILE.TXT"), "Command Failed\r");   // Datei ist kein Verzeichnis
    EXPECT_TRUE(fs::exists(w.stick / "FILE.TXT"));
}

TEST(VdipGeraet, DlfUndNichtVorhandenUndSchreibgeschuetzteDatei) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    schreib(w.stick / "WEG.TXT", "x");
    EXPECT_EQ(v.cmd(OP_DLF + " WEG.TXT"), OK_K);
    EXPECT_FALSE(fs::exists(w.stick / "WEG.TXT"));
    EXPECT_EQ(v.cmd(OP_DLF + " WEG.TXT"), "CF\r");
    schreib(w.stick / "RO.TXT", "x");
    fs::permissions(w.stick / "RO.TXT", fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write,
                    fs::perm_options::remove);
    EXPECT_EQ(v.cmd(OP_DLF + " RO.TXT"), "RO\r");   // UPUT: RO = Datei schreibgeschuetzt
    EXPECT_TRUE(fs::exists(w.stick / "RO.TXT"));
    EXPECT_EQ(v.cmd(OP_OPW + " RO.TXT"), "RO\r");
    fs::permissions(w.stick / "RO.TXT", fs::perms::owner_write, fs::perm_options::add);
    v.cmd(ECS);
    schreib(w.stick / "T2.TXT", "x");
    EXPECT_EQ(v.cmd("DLF T2.TXT"), OK_L);
    fs::create_directories(w.stick / "ISDIR");
    EXPECT_EQ(v.cmd("DLF ISDIR"), "Command Failed\r");   // Verzeichnisse nur ueber DLD
}

TEST(VdipGeraet, RenUndFsUndIdd) {
    Welt w;
    Gast v({w.stick, false});
    v.rx();
    schreib(w.stick / "alt.txt", "inhalt");
    EXPECT_EQ(v.cmd("REN ALT.TXT NEU.TXT"), OK_L);
    EXPECT_FALSE(fs::exists(w.stick / "alt.txt"));
    EXPECT_EQ(lies(w.stick / "NEU.TXT"), "inhalt");
    schreib(w.stick / "ANDERE.TXT", "a");
    EXPECT_EQ(v.cmd("REN NEU.TXT ANDERE.TXT"), "Command Failed\r");   // Ziel existiert
    EXPECT_EQ(v.cmd("REN NIX.TXT X.TXT"), "Command Failed\r");
    EXPECT_EQ(v.cmd("REN NEU.TXT"), "Invalid\r");
    EXPECT_EQ(v.cmd("REN NEU.TXT ../X"), "Filename Invalid\r");
    EXPECT_EQ(lies(w.stick / "NEU.TXT"), "inhalt");
    const std::string fsr = v.cmd("FS");
    ASSERT_GT(fsr.size(), 6u);
    EXPECT_EQ(fsr.substr(fsr.size() - OK_L.size()), OK_L);
    EXPECT_GT(std::stoull(fsr.substr(0, fsr.find('\r'))), 0ull);
    EXPECT_EQ(v.cmd("IDD").substr(0, 5), "K1520");
}

TEST(VdipGeraet, RdZeigtDatei) {
    Welt w;
    Gast v({w.stick, false});
    v.rx();
    const std::string d = muster(300);
    schreib(w.stick / "R.TXT", d);
    EXPECT_EQ(v.cmd("RD R.TXT"), d + OK_L);
    EXPECT_EQ(v.cmd("RD NIX.TXT"), "Command Failed\r");
    EXPECT_FALSE(v.g.stick().dateiOffen());
}

// ------------------------------------------------------------------------------ Fehlerfaelle

TEST(VdipGeraet, DateiFehltOprUndCloseOhneOffene) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    EXPECT_EQ(v.cmd(OP_OPR + " NIX.TXT"), "CF\r");   // UPUT: FI (3.64) oder CF (3.66)
    EXPECT_EQ(v.cmd(OP_CLF + " NIX.TXT"), "CF\r");
    EXPECT_EQ(v.cmd(OP_RDF + " " + num(1)), "CF\r");
    // WRF ohne offene Datei: die Daten werden abgenommen, das Ergebnis kommt danach, nichts wird als Befehl gelesen
    v.tx(OP_WRF + " " + num(5) + cr() + "DIR" + cr() + "x");
    EXPECT_EQ(v.rx(), "CF\r");
    EXPECT_EQ(v.cmd(""), OK_K);
}

TEST(VdipGeraet, SchonOffen) {
    Welt w;
    Gast v({w.stick, false});
    v.scsIph();
    schreib(w.stick / "A.TXT", "a");
    schreib(w.stick / "B.TXT", "b");
    ASSERT_EQ(v.cmd(OP_OPW + " A.TXT"), OK_K);
    EXPECT_EQ(v.cmd(OP_OPW + " B.TXT"), "FO\r");   // UPUT: FO = andere Datei zum Schreiben offen
    EXPECT_EQ(v.cmd(OP_OPR + " B.TXT"), "FO\r");
    EXPECT_EQ(v.cmd(OP_DLF + " A.TXT"), "FO\r");
    EXPECT_EQ(v.cmd(OP_CLF + " B.TXT"), "CF\r");   // falsche Datei
    EXPECT_EQ(v.cmd(OP_CLF + " A.TXT"), OK_K);
    EXPECT_EQ(v.cmd(OP_OPR + " B.TXT"), OK_K);
    EXPECT_EQ(v.cmd(OP_RDF + " " + num(1)), "b" + OK_K);
    ASSERT_EQ(v.cmd(OP_CLF + " B.TXT"), OK_K);
    // Lesen geoeffnet: WRF ist kein Schreibzugriff
    ASSERT_EQ(v.cmd(OP_OPR + " A.TXT"), OK_K);
    v.tx(OP_WRF + " " + num(1) + cr() + "z");
    EXPECT_EQ(v.rx(), "CF\r");
    EXPECT_EQ(lies(w.stick / "A.TXT"), "a");
}

TEST(VdipGeraet, OhneStick) {
    Welt w;
    Gast v;
    v.rx();
    EXPECT_EQ(v.cmd(""), "No Disk\r");
    EXPECT_EQ(v.cmd("DIR"), "\rNo Disk\r");
    EXPECT_EQ(v.cmd("CD X"), "No Disk\r");
    EXPECT_EQ(v.cmd("E"), "E\r");
    EXPECT_EQ(v.cmd(SCS), "ND\r");   // UDIR scheitert mit "ND-Error" [?]
    EXPECT_EQ(v.cmd(""), "ND\r");
    EXPECT_EQ(v.cmd(IPH), "ND\r");
    EXPECT_EQ(v.cmd(OP_OPW + " A.TXT"), "ND\r");
    v.tx(OP_WRF + " " + num(2) + cr() + "ab");
    EXPECT_EQ(v.rx(), "ND\r");
    // Stick einstecken / ziehen im Betrieb
    ASSERT_TRUE(v.g.stickEinstecken({w.stick, false}));
    EXPECT_EQ(v.rx(), "Device Detected P2\r>\r");
    EXPECT_EQ(v.cmd(""), ">\r");
    v.g.stickZiehen();
    EXPECT_EQ(v.rx(), "Device Removed P2\rND\r");
    EXPECT_FALSE(v.g.stickEinstecken({w.basis / "gibtsnicht", false}));
}

TEST(VdipGeraet, SchreibgeschuetzterStick) {
    Welt w;
    schreib(w.stick / "L.TXT", "lesen");
    fs::create_directories(w.stick / "D");
    Gast v({w.stick, true});
    v.scsIph();
    EXPECT_EQ(v.cmd(OP_OPW + " N.TXT"), "RO\r");
    EXPECT_EQ(v.cmd(OP_DLF + " L.TXT"), "RO\r");
    EXPECT_EQ(v.cmd("MKD X"), "RO\r");
    EXPECT_EQ(v.cmd("DLD D"), "RO\r");
    EXPECT_EQ(v.cmd("REN L.TXT M.TXT"), "RO\r");
    EXPECT_FALSE(fs::exists(w.stick / "N.TXT"));
    EXPECT_FALSE(fs::exists(w.stick / "X"));
    EXPECT_TRUE(fs::exists(w.stick / "L.TXT"));
    EXPECT_TRUE(fs::exists(w.stick / "D"));
    EXPECT_EQ(v.herunter("L.TXT"), "lesen");   // Lesen geht
}

// ------------------------------------------------------------------------------ Namen

TEST(VdipStick, Namen83) {
    struct F { const char* in; const char* norm; };   // norm == nullptr: ungueltig
    const F f[] = {
        {"A.TXT", "A.TXT"}, {"a.txt", "A.TXT"}, {"README", "README"}, {"README.", "README"},
        {"ABCDEFGH.XYZ", "ABCDEFGH.XYZ"}, {"A-B_C~1.T$T", "A-B_C~1.T$T"}, {"1.2", "1.2"},
        {"ABCDEFGHI.TXT", nullptr}, {"A.ABCD", nullptr}, {"A..B", nullptr}, {"A.B.C", nullptr},
        {".", nullptr}, {"..", nullptr}, {"...", nullptr}, {".HID", nullptr}, {"", nullptr},
        {"A B", nullptr}, {"A/B", nullptr}, {"A\\B", nullptr}, {"/A", nullptr}, {"\\A", nullptr},
        {"C:A", nullptr}, {"C:\\A", nullptr}, {"A*", nullptr}, {"A?", nullptr}, {"A\"", nullptr},
        {"A|B", nullptr}, {"A<B", nullptr}, {"A>B", nullptr}, {"A+B", nullptr}, {"A,B", nullptr},
        {"A;B", nullptr}, {"A=B", nullptr}, {"A[B", nullptr}, {"CON", nullptr}, {"con.txt", nullptr},
        {"NUL", nullptr}, {"AUX.X", nullptr}, {"PRN", nullptr}, {"COM1", nullptr}, {"LPT9.TXT", nullptr},
        {"COM10", "COM10"}, {"CONX", "CONX"}, {"A\x01", nullptr}, {"\xC4.TXT", nullptr},
    };
    for (const auto& x : f) {
        std::string n;
        const bool ok = VdipStick::dateiname83(x.in, n);
        if (x.norm) {
            EXPECT_TRUE(ok) << x.in;
            EXPECT_EQ(n, x.norm) << x.in;
        } else {
            EXPECT_FALSE(ok) << "soll ungueltig sein: " << x.in;
        }
    }
    std::string n;
    EXPECT_FALSE(VdipStick::dateiname83(std::string("A\0B", 3), n));   // NUL im Namen
}

// ------------------------------------------------------------------------------ Pfadflucht

namespace {

// Namen, die aus dem Stick hinausfuehren oder ungueltig sein sollen
const char* const FLUCHTNAMEN[] = {
    "..", "../AUSSEN", "../AUSSEN/GUARD.TXT", "..\\AUSSEN\\GUARD.TXT", "../GUARD.TXT", "../../X", "..\\..\\X",
    "/etc/passwd", "/GUARD.TXT", "\\GUARD.TXT", "C:\\X.TXT", "C:X.TXT", "AUSSEN/GUARD.TXT",
    "SUB/../../AUSSEN/GUARD.TXT", "A/B.TXT", "....", "NAME.TXT/..", "~", "~/X", "$HOME", "%TEMP%",
    "ABCDEFGHIJ.TXT", "A.TOOLONG", "CON", "NUL.TXT", "COM1", "A\x01.TXT", "\xE4.TXT", ".", "...", "../",
    "./../AUSSEN", "AUSSEN", "GUARD.TXT/..", "..///AUSSEN",
};

// Alle Befehle, die einen Namen annehmen, mit dem Namen als Argument (und als zweiter Name fuer REN)
std::vector<std::string> befehleMit(const std::string& n) {
    return {OP_DIR + " " + n, OP_DIRT + " " + n, OP_OPR + " " + n, OP_OPW + " " + n, OP_DLF + " " + n,
            OP_CLF + " " + n, "CD " + n, "MKD " + n, "DLD " + n, "RD " + n, "REN " + n + " X.TXT",
            "REN GUARD.TXT " + n, "REN " + n + " " + n};
}

}  // namespace

TEST(VdipPfadflucht, KeinZugriffAusserhalbDesSticks) {
    Welt w;
    schreib(w.stick / "GUARD.TXT", "IM-STICK");   // gleicher Name wie der Waechter draussen
    const auto vorher = w.aussenAbbild();
    ASSERT_FALSE(vorher.empty());
    Gast v({w.stick, false});
    v.scsIph();
    for (const char* n : FLUCHTNAMEN) {
        for (const std::string& c : befehleMit(n)) {
            v.tx(c + cr());
            v.rx();
            EXPECT_EQ(w.aussenAbbild(), vorher) << "Befehl: " << c;
            EXPECT_EQ(v.g.stick().verzeichnisAnzeige().find(".."), std::string::npos);
            v.g.stick().cd("/");
            v.g.stick().schliesseDatei();
        }
    }
    // Wurzel des Sticks: nichts Neues ausser dem, was ein gueltiger Name erlaubt; Geschwister unberuehrt
    std::set<std::string> basisNamen;
    for (const auto& e : fs::directory_iterator(w.basis)) basisNamen.insert(e.path().filename().string());
    EXPECT_EQ(basisNamen, (std::set<std::string>{"AUSSEN", "stick"}));
    EXPECT_EQ(lies(w.aussen / "GUARD.TXT"), "WAECHTER");
}

TEST(VdipPfadflucht, CdPunktPunktUndWurzel) {
    Welt w;
    Gast v({w.stick, false});
    v.rx();
    EXPECT_EQ(v.cmd("CD .."), "Command Failed\r");
    EXPECT_EQ(v.cmd("CD ../AUSSEN"), "Filename Invalid\r");
    v.cmd("MKD UP");
    v.cmd("CD UP");
    EXPECT_EQ(v.cmd("CD .."), OK_L);
    EXPECT_EQ(v.cmd("CD .."), "Command Failed\r");
    EXPECT_EQ(v.cmd("CD .."), "Command Failed\r");
    // Schreiben im Wurzelverzeichnis des Sticks, nicht daneben
    v.cmd(SCS);
    ASSERT_EQ(v.cmd(OP_OPW + " WURZEL.TXT"), OK_K);
    v.tx(OP_WRF + " 1\rx");
    v.rx();
    v.cmd(OP_CLF + " WURZEL.TXT");
    EXPECT_TRUE(fs::exists(w.stick / "WURZEL.TXT"));
    EXPECT_FALSE(fs::exists(w.basis / "WURZEL.TXT"));
}

TEST(VdipPfadflucht, SymlinksNachAussen) {
    Welt w;
    std::error_code ec;
    fs::create_directory_symlink(w.aussen, w.stick / "LINK", ec);
    if (ec) GTEST_SKIP() << "Symlinks nicht moeglich: " << ec.message();
    fs::create_symlink(w.aussen / "GUARD.TXT", w.stick / "LNK.TXT", ec);
    ASSERT_FALSE(ec);
    fs::create_symlink(w.aussen / "NEU.TXT", w.stick / "DANGLE.TXT", ec);   // toter Link nach draussen
    ASSERT_FALSE(ec);
    fs::create_symlink(w.aussen / "SUB", w.stick / "SUBLINK", ec);
    ASSERT_FALSE(ec);
    schreib(w.stick / "OK.TXT", "ok");
    const auto vorher = w.aussenAbbild();
    Gast v({w.stick, false});
    v.scsIph();
    // Symlinks sind unsichtbar
    EXPECT_EQ(v.cmd(OP_DIR), "\rOK.TXT\r>\r");
    // und nicht benutzbar
    EXPECT_EQ(v.cmd("CD LINK"), "CF\r");
    EXPECT_EQ(v.cmd("CD SUBLINK"), "CF\r");
    EXPECT_EQ(v.cmd(OP_OPR + " LNK.TXT"), "CF\r");
    EXPECT_EQ(v.cmd(OP_OPW + " LNK.TXT"), "CF\r");      // sonst wuerde der Waechter ueberschrieben
    EXPECT_EQ(v.cmd(OP_OPW + " DANGLE.TXT"), "CF\r");   // sonst entstuende aussen/neu.txt
    EXPECT_EQ(v.cmd(OP_DLF + " LNK.TXT"), "CF\r");
    EXPECT_EQ(v.cmd("DLD LINK"), "CF\r");
    EXPECT_EQ(v.cmd("MKD LINK"), "CF\r");
    EXPECT_EQ(v.cmd("REN OK.TXT LNK.TXT"), "CF\r");
    EXPECT_EQ(v.cmd("REN LNK.TXT Y.TXT"), "CF\r");
    EXPECT_EQ(v.cmd(OP_DIR + " LNK.TXT"), "\rCF\r");
    EXPECT_EQ(w.aussenAbbild(), vorher);
    EXPECT_FALSE(fs::exists(w.aussen / "NEU.TXT"));
    EXPECT_EQ(lies(w.aussen / "GUARD.TXT"), "WAECHTER");
    EXPECT_TRUE(fs::is_symlink(w.stick / "LNK.TXT"));
}

TEST(VdipPfadflucht, VerzeichnisWirdNachCdDurchSymlinkErsetzt) {
    Welt w;
    Gast v({w.stick, false});
    v.rx();
    ASSERT_EQ(v.cmd("MKD T"), OK_L);
    ASSERT_EQ(v.cmd("CD T"), OK_L);
    // Waehrend die Firmware in T steht, wird T gegen einen Link nach draussen getauscht (TOCTOU)
    std::error_code ec;
    fs::remove(w.stick / "T", ec);
    fs::create_directory_symlink(w.aussen, w.stick / "T", ec);
    if (ec) GTEST_SKIP() << "Symlinks nicht moeglich";
    const auto vorher = w.aussenAbbild();
    EXPECT_EQ(v.cmd("DIR"), "\rCommand Failed\r");
    v.cmd(SCS);
    // die Firmware ist an die Wurzel zurueckgefallen: geschrieben wird dort, nie durch den Link
    EXPECT_EQ(v.cmd(OP_OPW + " EVIL.TXT"), OK_K);
    EXPECT_EQ(v.cmd(OP_CLF + " EVIL.TXT"), OK_K);
    EXPECT_TRUE(v.g.stick().imWurzelverzeichnis());
    EXPECT_TRUE(fs::exists(w.stick / "EVIL.TXT"));
    EXPECT_EQ(w.aussenAbbild(), vorher);
}

TEST(VdipPfadflucht, WurzelSelbstAlsSymlinkWirdAufgeloest) {
    Welt w;
    std::error_code ec;
    fs::create_directory_symlink(w.stick, w.basis / "viaLink", ec);
    if (ec) GTEST_SKIP() << "Symlinks nicht moeglich";
    Gast v({w.basis / "viaLink", false});
    ASSERT_TRUE(v.g.stickVorhanden());
    v.rx();
    v.cmd("MKD X");
    EXPECT_TRUE(fs::is_directory(w.stick / "X"));
}

TEST(VdipPfadflucht, TestKannRotWerden) {
    // Selbstpruefung der Waechter-Methode: veraendert man etwas ausserhalb, muss das Abbild abweichen.
    Welt w;
    const auto vorher = w.aussenAbbild();
    schreib(w.aussen / "GUARD.TXT", "VERAENDERT");
    EXPECT_NE(w.aussenAbbild(), vorher);
    schreib(w.aussen / "NEU.TXT", "x");
    schreib(w.aussen / "GUARD.TXT", "WAECHTER");
    EXPECT_NE(w.aussenAbbild(), vorher);
}
