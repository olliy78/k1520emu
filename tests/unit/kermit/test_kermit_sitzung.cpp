/**
 * @file test_kermit_sitzung.cpp
 * @brief Kermit-Sitzung Client <-> Server in-process (doc/design/36_kermit.md,
 *        AP K2): SpeicherLeitungPaar mit virtueller Uhr in zwei Faeden —
 *        Dateien 0 B … 100 KB, Text, Namen, Aushandlung gegen eine Nachbildung
 *        von C-Kermit 4.2 (9 Init-Felder, CAPAS 0), Verlust/Verfaelschung,
 *        stumme Gegenstelle, Abbruch (sofort, Datei, Gruppe), voller
 *        Datentraeger, Pfadfallen, Serverbefehle auf Paketebene.
 */
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "kermit/src/protokoll.h"

namespace fs = std::filesystem;
using namespace k1520::kermit;

namespace {

struct TempOrdner {
    fs::path p;
    TempOrdner() {
        std::random_device rd;
        p = fs::temp_directory_path() / ("k1520kermit_s_" + std::to_string(rd()) + std::to_string(rd()));
        fs::create_directories(p);
    }
    ~TempOrdner() {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
    std::string s() const { return p.u8string(); }
};

void schreibe(const fs::path& p, const std::string& s) {
    std::ofstream f(p, std::ios::binary);
    f << s;
}
std::string lies(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}
std::string zufall(std::size_t n, unsigned saat) {
    std::mt19937 g(saat);
    std::string s(n, '\0');
    for (auto& c : s) c = static_cast<char>(g());
    return s;
}

Einstellungen client() {
    Einstellungen e;
    e.frist_ms = 2000;
    return e;
}
Einstellungen server() {
    Einstellungen e;
    e.frist_ms = 3000;
    return e;
}
/// Nachbildung der Gegenstelle C-Kermit 4.2: 9 Init-Felder, kein CAPAS,
/// QBIN nur 'Y' (ohne Paritaet), Pruefung 1, Paketlaenge 90.
Einstellungen ck42() {
    Einstellungen e = server();
    e.capas_senden = false;
    e.langpakete = false;
    e.attribute = false;
    e.qbin = 'Y';
    e.pruefsumme = 1;
    e.maxl = 90;
    e.frist_ms = 10000;
    return e;
}

/// Server in eigenem Faden an Ende 1; Ende 0 gehoert dem Test.
struct Aufbau {
    SpeicherLeitungPaar leitung;
    TempOrdner wurzel;
    Freigabe freigabe{wurzel.p};
    std::thread faden;
    Ergebnis serverErgebnis;
    std::vector<std::string> serverEreignisse;

    void starte(Einstellungen e = server()) {
        faden = std::thread([this, e] {
            Rueckrufe r;
            r.ereignis = [this](const std::string& t) { serverEreignisse.push_back(t); };
            Sitzung s(leitung.ende(1), e, r);
            serverErgebnis = s.serve(freigabe);
        });
    }
    void ende() {
        if (faden.joinable()) {
            leitung.ende(0).schliessen();
            faden.join();
        }
    }
    ~Aufbau() { ende(); }
};

/// Pakete von Hand (Nachbildung eines C-Kermit-4.2-Clients auf Paketebene).
struct Roh {
    Leitung& l;
    PaketZerleger z;
    int chkt = 1;
    void sende(char typ, int seq, const std::string& d) {
        const std::string r = baueRahmen(Paket{typ, seq, d}, chkt, {});
        l.schreiben(reinterpret_cast<const uint8_t*>(r.data()), r.size());
    }
    bool lies(Paket& p, int64_t frist = 20000) {
        z.setzeChkt(chkt);
        const int64_t ende = l.jetztMs() + frist;
        while (l.jetztMs() < ende) {
            uint8_t b;
            const int n = l.lesen(&b, 1, ende - l.jetztMs());
            if (n < 0) return false;
            if (n == 0) continue;
            if (z.zufuehren(b) == PaketZerleger::Ergebnis::Paket) {
                p = z.paket();
                return true;
            }
        }
        return false;
    }
    /// Quittiert eine Server-Uebertragung (S … B) wie C-Kermit und sammelt die Daten.
    std::string empfangeAlles(const Paket& s, std::string meinS) {
        std::string text;
        const InitParameter d = zerlegeInit(s.daten);
        meinS[7] = static_cast<char>('0' + d.chkt);   // rinit(): spar, dann rpar — spiegelt
        sende('Y', s.seq, meinS);
        chkt = d.chkt;   // C-Kermit spiegelt
        Kodierung k;
        k.rept = d.rept == '~' ? '~' : 0;
        Paket p;
        while (lies(p)) {
            if (p.typ == 'D') text += dekodiereText(k, p.daten);
            sende('Y', p.seq, "");
            if (p.typ == 'B') break;
        }
        chkt = 1;
        return text;
    }
};

}  // namespace

// ─── Leitung ───────────────────────────────────────────────────────────────

TEST(KermitLeitung, VirtuelleUhrSpringtNurWennAlleWarten) {
    SpeicherLeitungPaar p;
    p.setzeBeteiligt(1, false);   // stumme Gegenstelle
    uint8_t b[4];
    const auto t0 = std::chrono::steady_clock::now();
    EXPECT_EQ(p.ende(0).lesen(b, 4, 60000), 0);
    EXPECT_EQ(p.jetzt(), 60000);
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::seconds(5));
}

TEST(KermitLeitung, VerzoegerungSiebenBitUndSchliessen) {
    SpeicherLeitungPaar p;
    p.setzeVerzoegerung(0, 500);
    p.setzeSiebenBit(0, true);
    p.setzeBeteiligt(0, false);
    const uint8_t x[2] = {0xC1, 0x42};
    EXPECT_TRUE(p.ende(0).schreiben(x, 2));
    uint8_t b[4];
    EXPECT_EQ(p.ende(1).lesen(b, 4, 100), 0);   // noch unterwegs
    EXPECT_EQ(p.ende(1).lesen(b, 4, 1000), 2);
    EXPECT_EQ(b[0], 0x41);
    EXPECT_GE(p.jetzt(), 500);
    p.ende(0).wecken();   // weckt nur Ende 0
    p.ende(1).wecken();
    EXPECT_EQ(p.ende(1).lesen(b, 4, 1000), 0);
    p.ende(0).schliessen();
    EXPECT_EQ(p.ende(1).lesen(b, 4, 1000), -1);
    EXPECT_FALSE(p.ende(1).schreiben(x, 2));
    EXPECT_EQ(p.bloecke(0), 1);
}

TEST(KermitLeitung, EchtzeitBetrieb) {
    SpeicherLeitungPaar p(false);
    uint8_t b[1];
    EXPECT_EQ(p.ende(0).lesen(b, 1, 20), 0);
    EXPECT_GE(p.jetzt(), 15);
    const uint8_t x = 7;
    p.ende(1).schreiben(&x, 1);
    EXPECT_EQ(p.ende(0).lesen(b, 1, 20), 1);
}

// ─── Uebertragungen ────────────────────────────────────────────────────────

class KermitGroessen : public ::testing::TestWithParam<std::size_t> {};

TEST_P(KermitGroessen, SendenUndHolenBinaer) {
    Aufbau a;
    a.starte();
    TempOrdner lokal;
    const std::string inhalt = zufall(GetParam(), static_cast<unsigned>(GetParam()));
    schreibe(lokal.p / "quelle.bin", inhalt);
    int64_t letzte = -1;
    Rueckrufe r;
    r.fortschritt = [&](const std::string&, int64_t b, int64_t g) {
        letzte = b;
        EXPECT_EQ(g, static_cast<int64_t>(inhalt.size()));
    };
    Sitzung c(a.leitung.ende(0), client(), r);
    Ergebnis e = c.senden((lokal.p / "quelle.bin").u8string(), "ziel.bin");
    ASSERT_TRUE(e.ok()) << e.text;
    EXPECT_EQ(lies(a.wurzel.p / "ziel.bin"), inhalt);
    EXPECT_EQ(letzte, static_cast<int64_t>(inhalt.size()));
    TempOrdner hol;
    e = c.holen("ziel.bin", hol.s());
    ASSERT_TRUE(e.ok()) << e.text;
    ASSERT_EQ(e.dateien.size(), 1u);
    EXPECT_EQ(lies(hol.p / "ziel.bin"), inhalt);
    EXPECT_TRUE(c.entferntBeenden().ok());
    a.faden.join();
    EXPECT_TRUE(a.serverErgebnis.ok()) << a.serverErgebnis.text;
}

INSTANTIATE_TEST_SUITE_P(Groessen, KermitGroessen, ::testing::Values(0, 1, 94, 95, 1000, 102400));

TEST(KermitSitzung, LangpaketeWerdenGefahren) {
    Aufbau a;
    a.starte();
    TempOrdner lokal;
    schreibe(lokal.p / "q", zufall(102400, 5));
    Sitzung c(a.leitung.ende(0), client());
    ASSERT_TRUE(c.senden((lokal.p / "q").u8string(), "q").ok());
    EXPECT_LT(a.leitung.bloecke(0), 250);   // mit 94er Paketen waeren es > 1500
}

TEST(KermitSitzung, TextmodusCrLfAufDerLeitung) {
    Aufbau a;
    a.starte();   // Server binaer: sieht, was auf der Leitung steht
    TempOrdner lokal;
    schreibe(lokal.p / "t.txt", "zeile1\nzeile2\r\nende");
    Einstellungen e = client();
    e.textmodus = true;
    e.attribute = false;   // sonst galte der Typ aus dem A-Paket ("A" bzw. "B8")
    Sitzung c(a.leitung.ende(0), e);
    ASSERT_TRUE(c.senden((lokal.p / "t.txt").u8string(), "").ok());
    EXPECT_EQ(lies(a.wurzel.p / "t.txt"), "zeile1\r\nzeile2\r\r\nende");
    // zurueck im Textmodus: CR LF → LF, das einzelne CR bleibt
    TempOrdner hol;
    ASSERT_TRUE(c.holen("t.txt", hol.s()).ok());
    EXPECT_EQ(lies(hol.p / "t.txt"), "zeile1\nzeile2\r\nende");
}

TEST(KermitSitzung, AttributTextUeberstimmtEmpfaenger) {
    Aufbau a;
    Einstellungen s = server();
    s.textmodus = false;
    a.starte(s);
    TempOrdner lokal;
    schreibe(lokal.p / "a.txt", "x\ny\n\r");   // CR am Dateiende
    Einstellungen e = client();
    e.textmodus = true;
    Sitzung c(a.leitung.ende(0), e);
    ASSERT_TRUE(c.senden((lokal.p / "a.txt").u8string(), "").ok());
    // Attribut '"' = A: der Server wandelt zurueck, obwohl er binaer eingestellt ist
    EXPECT_EQ(lies(a.wurzel.p / "a.txt"), "x\ny\n\r");
}

TEST(KermitSitzung, NamenUndPunktSonderfaelle) {
    Aufbau a;
    a.starte();
    TempOrdner lokal;
    for (const char* n : {"README", ".hidden", "a.b.c", "noext", "Zeta"}) schreibe(lokal.p / n, n);
    Sitzung c(a.leitung.ende(0), client());
    std::vector<SendeAuftrag> g;
    for (const char* n : {"README", ".hidden", "a.b.c", "noext", "Zeta"}) g.push_back({(lokal.p / n).u8string(), ""});
    const Ergebnis e = c.sendenGruppe(g);
    ASSERT_TRUE(e.ok()) << e.text;
    EXPECT_EQ(e.dateien.size(), 5u);
    EXPECT_EQ(lies(a.wurzel.p / "readme"), "README");   // Empfang konvertiert
    EXPECT_EQ(lies(a.wurzel.p / "zeta"), "Zeta");
    EXPECT_EQ(lies(a.wurzel.p / ".hidden"), ".hidden");
    EXPECT_EQ(lies(a.wurzel.p / "a.b.c"), "a.b.c");
    EXPECT_TRUE(fs::exists(a.wurzel.p / "noext"));
}

TEST(KermitSitzung, NamenSendenKonvertiertUndEmpfangLiteral) {
    Aufbau a;
    Einstellungen s = server();
    s.namen_empfang = Namen::Literal;
    a.starte(s);
    TempOrdner lokal;
    schreibe(lokal.p / "a.b.c", "1");
    Einstellungen e = client();
    e.namen_senden = Namen::Konvertiert;
    Sitzung c(a.leitung.ende(0), e);
    ASSERT_TRUE(c.senden((lokal.p / "a.b.c").u8string(), "").ok());
    EXPECT_TRUE(fs::exists(a.wurzel.p / "A.BXC"));
}

TEST(KermitSitzung, NamenslaengeWega) {
    Aufbau a;
    Einstellungen s = server();
    s.max_namenslaenge = 14;
    a.starte(s);
    TempOrdner lokal;
    schreibe(lokal.p / "q", "x");
    Sitzung c(a.leitung.ende(0), client());
    EXPECT_TRUE(c.senden((lokal.p / "q").u8string(), "vierzehnzeiche").ok());
    Ergebnis e = c.senden((lokal.p / "q").u8string(), "fuenfzehnzeiche");
    EXPECT_EQ(e.art, Fehlerart::Gegenstelle);
    EXPECT_NE(e.text.find("laenger als 14"), std::string::npos);
    EXPECT_FALSE(fs::exists(a.wurzel.p / "fuenfzehnzeiche"));
    // Client mit eigener Grenze: Fehlerpaket mit Grund an den Server
    Einstellungen ce = client();
    ce.max_namenslaenge = 14;
    Sitzung c2(a.leitung.ende(0), ce);
    e = c2.senden((lokal.p / "q").u8string(), "fuenfzehnzeiche");
    EXPECT_EQ(e.art, Fehlerart::NameZuLang);
    EXPECT_TRUE(c2.entferntBeenden().ok());
    a.faden.join();
    bool gemeldet = false;
    for (const auto& t : a.serverEreignisse) gemeldet |= t.find("Zielname laenger") != std::string::npos;
    EXPECT_TRUE(gemeldet);
}

TEST(KermitSitzung, ServerbefehleVerzeichnisWechselnLoeschenHilfe) {
    Aufbau a;
    fs::create_directory(a.wurzel.p / "sub");
    schreibe(a.wurzel.p / "eins.txt", std::string(123, 'a'));
    schreibe(a.wurzel.p / "sub" / "zwei", "22");
    a.starte();
    Sitzung c(a.leitung.ende(0), client());
    Ergebnis e = c.entferntVerzeichnis("");
    ASSERT_TRUE(e.ok()) << e.text;
    auto v = zerlegeLsListe(e.ausgabe);
    ASSERT_EQ(v.size(), 2u) << e.ausgabe;
    EXPECT_EQ(v[0].name, "eins.txt");
    EXPECT_EQ(v[0].groesse, 123);
    EXPECT_EQ(v[1].art, 'd');
    e = c.entferntWechseln("sub");
    ASSERT_TRUE(e.ok());
    EXPECT_EQ(e.ausgabe, "/sub");
    TempOrdner hol;
    ASSERT_TRUE(c.holen("zwei", hol.s()).ok());
    EXPECT_EQ(lies(hol.p / "zwei"), "22");
    e = c.entferntWechseln("../..");
    EXPECT_EQ(e.art, Fehlerart::Gegenstelle);
    EXPECT_NE(e.text.find("aus der Freigabe"), std::string::npos);
    EXPECT_TRUE(c.entferntWechseln("").ok());
    e = c.entferntLoeschen("eins.txt");
    ASSERT_TRUE(e.ok());
    EXPECT_NE(e.ausgabe.find("eins.txt"), std::string::npos);
    EXPECT_FALSE(fs::exists(a.wurzel.p / "eins.txt"));
    EXPECT_EQ(c.entferntLoeschen("eins.txt").art, Fehlerart::Gegenstelle);
    EXPECT_EQ(c.entferntVerzeichnis("nichtda").art, Fehlerart::Gegenstelle);
    e = c.entferntHilfe();
    ASSERT_TRUE(e.ok());
    EXPECT_NE(e.ausgabe.find("Ordnerfreigabe"), std::string::npos);
    EXPECT_EQ(c.holen("gibtsnicht", hol.s()).art, Fehlerart::Gegenstelle);
    EXPECT_TRUE(c.entferntAbmelden().ok());
    a.faden.join();
    EXPECT_TRUE(a.serverErgebnis.ok());
}

TEST(KermitSitzung, HolenMitMusterMehrereDateien) {
    Aufbau a;
    for (const char* n : {"a.c", "b.c", "c.h"}) schreibe(a.wurzel.p / n, n);
    a.starte();
    Sitzung c(a.leitung.ende(0), client());
    TempOrdner hol;
    const Ergebnis e = c.holen("*.c", hol.s());
    ASSERT_TRUE(e.ok()) << e.text;
    EXPECT_EQ(e.dateien.size(), 2u);
    EXPECT_TRUE(fs::exists(hol.p / "a.c"));
    EXPECT_FALSE(fs::exists(hol.p / "c.h"));
}

TEST(KermitSitzung, NurLesenWeistSendenAb) {
    SpeicherLeitungPaar l;
    TempOrdner w, lokal;
    Freigabe f(w.p, true);
    std::thread t([&] {
        Sitzung s(l.ende(1), server());
        s.serve(f);
    });
    schreibe(lokal.p / "q", "x");
    Sitzung c(l.ende(0), client());
    const Ergebnis e = c.senden((lokal.p / "q").u8string(), "");
    EXPECT_EQ(e.art, Fehlerart::Gegenstelle);
    EXPECT_NE(e.text.find("schreibgeschuetzt"), std::string::npos);
    l.ende(0).schliessen();
    t.join();
}

// ─── Aushandlung gegen C-Kermit 4.2 ────────────────────────────────────────

struct Kombination {
    char qbin;
    char rept;
    int chkt;
    bool lang;
    bool siebenBit;
    bool erwartetOk;
};
class KermitGegenCk42 : public ::testing::TestWithParam<Kombination> {};

TEST_P(KermitGegenCk42, Uebertragung) {
    const Kombination k = GetParam();
    Aufbau a;
    a.leitung.setzeSiebenBit(0, k.siebenBit);
    a.leitung.setzeSiebenBit(1, k.siebenBit);
    a.starte(ck42());
    TempOrdner lokal;
    const std::string inhalt = zufall(3000, 9) + std::string(300, 'r');
    schreibe(lokal.p / "q", inhalt);
    Einstellungen e = client();
    e.qbin = k.qbin;
    e.rept = k.rept;
    e.pruefsumme = k.chkt;
    e.langpakete = k.lang;
    e.max_wiederholungen = 3;
    Sitzung c(a.leitung.ende(0), e);
    const Ergebnis r = c.senden((lokal.p / "q").u8string(), "q");
    EXPECT_EQ(r.ok(), k.erwartetOk) << r.text;
    if (k.erwartetOk) {
        EXPECT_EQ(lies(a.wurzel.p / "q"), inhalt);
        TempOrdner hol;
        ASSERT_TRUE(c.holen("q", hol.s()).ok());
        EXPECT_EQ(lies(hol.p / "q"), inhalt);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Kombinationen, KermitGegenCk42,
    ::testing::Values(
        // 8-Bit-saubere Leitung: alles geht (QBIN Y/Y → roh, & → Praefix)
        Kombination{'Y', '~', 3, true, false, true}, Kombination{'Y', '\0', 1, false, false, true},
        Kombination{'&', '~', 2, true, false, true}, Kombination{'N', '~', 1, true, false, true},
        Kombination{'&', '\0', 3, false, false, true},
        // 7-Bit-Leitung (Terminal loescht Bit 7): nur mit 8-Bit-Praefix
        Kombination{'&', '~', 3, true, true, true}, Kombination{'&', '\0', 1, false, true, true},
        Kombination{'Y', '~', 3, true, true, false}, Kombination{'N', '~', 2, false, true, false}));

TEST(KermitSitzung, Ck42AntwortetMitNeunFeldern) {
    // Befund K1: der WEGA-Server (C-Kermit 4.2) antwortet auf I mit
    // MAXL 94, TIME 10, EOL CR, QCTL #, QBIN Y, CHKT 1, REPT ~ — 9 Felder.
    Aufbau a;
    a.starte();
    Roh r{a.leitung.ende(0)};
    r.sende('I', 0, "~* @-#Y1~");
    Paket p;
    ASSERT_TRUE(r.lies(p));
    EXPECT_EQ(p.typ, 'Y');
    EXPECT_EQ(p.seq, 0);
    EXPECT_EQ(p.daten.size(), 9u) << "keine CAPAS an eine Gegenstelle ohne CAPAS";
    const InitParameter ip = zerlegeInit(p.daten);
    EXPECT_EQ(ip.chkt, 1);   // gespiegelt
    // C-Kermit-Client: G D nach dem I, Paketnummer wieder 0
    r.sende('G', 0, "D");
    ASSERT_TRUE(r.lies(p));
    ASSERT_EQ(p.typ, 'S');
    EXPECT_EQ(zerlegeInit(p.daten).felder, 13);
    const std::string t = r.empfangeAlles(p, "~* @-#Y1~");
    EXPECT_EQ(t.rfind("total ", 0), 0u) << t;
    // G C mit Kennwortfeld (C-Kermit setgen: Verzeichnis + Kennwort)
    fs::create_directory(a.wurzel.p / "sub");
    r.sende('G', 0, kodiere(Kodierung{}, std::string("C") + tochar(3) + "sub" + tochar(4) + "geht"));
    ASSERT_TRUE(r.lies(p));
    EXPECT_EQ(p.typ, 'Y');
    EXPECT_EQ(p.daten, "/sub");
}

TEST(KermitSitzung, ServerWeistHostKermitUndUnbekanntesAb) {
    Aufbau a;
    a.starte();
    Roh r{a.leitung.ende(0)};
    Paket p;
    for (const auto& [typ, daten] : std::vector<std::pair<char, std::string>>{
             {'C', "rm -rf /"}, {'K', "set x"}, {'Q', ""}, {'G', "W"}, {'G', ""}, {'G', "E"}}) {
        r.sende(typ, 0, daten);
        ASSERT_TRUE(r.lies(p)) << typ;
        EXPECT_EQ(p.typ, 'E') << typ << daten;
    }
    // Client-E und verirrte Quittungen werden still uebergangen
    r.sende('E', 0, "weg");
    r.sende('N', 0, "");
    r.sende('Y', 0, "");
    r.sende('G', 0, "F");
    ASSERT_TRUE(r.lies(p));
    EXPECT_EQ(p.typ, 'Y');
    a.faden.join();
}

TEST(KermitSitzung, UnerwartetesPaketImEmpfang) {
    Aufbau a;
    a.starte();
    Roh r{a.leitung.ende(0)};
    r.sende('S', 0, "~* @-#Y1~");
    Paket p;
    ASSERT_TRUE(r.lies(p));
    ASSERT_EQ(p.typ, 'Y');
    r.sende('Q', 1, "");
    ASSERT_TRUE(r.lies(p));
    EXPECT_EQ(p.typ, 'E');
    // Duplikat des S (Quittung verloren): erneute Quittung, kein Fehler
    r.sende('S', 0, "~* @-#Y1~");
    ASSERT_TRUE(r.lies(p));
    EXPECT_EQ(p.typ, 'Y');
    r.sende('S', 0, "~* @-#Y1~");
    ASSERT_TRUE(r.lies(p));
    EXPECT_EQ(p.typ, 'Y');
    r.sende('F', 5, "x");   // falsche Nummer → NAK fuer 1
    ASSERT_TRUE(r.lies(p));
    EXPECT_EQ(p.typ, 'N');
    EXPECT_EQ(p.seq, 1);
}

// ─── Fehlerfaelle ──────────────────────────────────────────────────────────

class KermitVerlust : public ::testing::TestWithParam<int> {};

TEST_P(KermitVerlust, JedesSiebtePaket) {
    Aufbau a;
    a.leitung.setzeStoerung(GetParam(), SpeicherLeitungPaar::verlustJedes(7));
    a.starte();
    TempOrdner lokal, hol;
    const std::string inhalt = zufall(102400, 77);
    schreibe(lokal.p / "q", inhalt);
    Sitzung c(a.leitung.ende(0), client());
    Ergebnis e = c.senden((lokal.p / "q").u8string(), "q");
    ASSERT_TRUE(e.ok()) << e.text;
    EXPECT_EQ(lies(a.wurzel.p / "q"), inhalt);
    e = c.holen("q", hol.s());
    ASSERT_TRUE(e.ok()) << e.text;
    EXPECT_EQ(lies(hol.p / "q"), inhalt);
    e = c.entferntVerzeichnis("");
    ASSERT_TRUE(e.ok()) << e.text;
}

INSTANTIATE_TEST_SUITE_P(Richtungen, KermitVerlust, ::testing::Values(0, 1));

TEST(KermitSitzung, VerfaelschteBytes) {
    Aufbau a;
    auto stoer = [](std::vector<uint8_t>& b, int nr) {
        if (nr % 5 == 3 && b.size() > 6) b[b.size() / 2] ^= 0x01;
        if (nr % 11 == 0 && b.size() > 4) b.resize(b.size() - 3);   // abgeschnitten
        return true;
    };
    a.leitung.setzeStoerung(0, stoer);
    a.leitung.setzeStoerung(1, stoer);
    a.starte();
    TempOrdner lokal, hol;
    const std::string inhalt = zufall(20000, 4);
    schreibe(lokal.p / "q", inhalt);
    Sitzung c(a.leitung.ende(0), client());
    ASSERT_TRUE(c.senden((lokal.p / "q").u8string(), "q").ok());
    EXPECT_EQ(lies(a.wurzel.p / "q"), inhalt);
    ASSERT_TRUE(c.holen("q", hol.s()).ok());
    EXPECT_EQ(lies(hol.p / "q"), inhalt);
}

TEST(KermitSitzung, VerzoegerteLeitung) {
    Aufbau a;
    a.leitung.setzeVerzoegerung(0, 400);
    a.leitung.setzeVerzoegerung(1, 400);
    a.starte();
    TempOrdner lokal;
    schreibe(lokal.p / "q", zufall(5000, 1));
    Sitzung c(a.leitung.ende(0), client());
    ASSERT_TRUE(c.senden((lokal.p / "q").u8string(), "q").ok());
    EXPECT_EQ(lies(a.wurzel.p / "q"), lies(lokal.p / "q"));
}

TEST(KermitSitzung, StummeGegenstelleLaeuftInDieFrist) {
    SpeicherLeitungPaar l;
    l.setzeBeteiligt(1, false);
    TempOrdner lokal;
    schreibe(lokal.p / "q", "x");
    Einstellungen e = client();
    e.max_wiederholungen = 5;
    Sitzung c(l.ende(0), e);
    const auto t0 = std::chrono::steady_clock::now();
    Ergebnis r = c.senden((lokal.p / "q").u8string(), "");
    EXPECT_EQ(r.art, Fehlerart::Zeitueberschreitung);
    EXPECT_EQ(l.jetzt(), 6 * 2000);   // 1 + 5 Wiederholungen je Frist (virtuell)
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::seconds(5));
    EXPECT_EQ(l.bloecke(0), 7);       // S, 5 Wiederholungen, E
    EXPECT_EQ(c.entferntVerzeichnis("").art, Fehlerart::Zeitueberschreitung);
    EXPECT_EQ(c.empfangen(lokal.s()).art, Fehlerart::Zeitueberschreitung);
    Einstellungen ohneI = e;
    ohneI.i_paket = false;
    Sitzung c2(l.ende(0), ohneI);
    EXPECT_EQ(c2.holen("x", lokal.s()).art, Fehlerart::Zeitueberschreitung);
}

TEST(KermitSitzung, GetrennteLeitung) {
    SpeicherLeitungPaar l;
    l.ende(1).schliessen();
    TempOrdner lokal;
    schreibe(lokal.p / "q", "x");
    Sitzung c(l.ende(0), client());
    EXPECT_EQ(c.senden((lokal.p / "q").u8string(), "").art, Fehlerart::LeitungGetrennt);
    EXPECT_EQ(c.senden((lokal.p / "fehlt").u8string(), "").art, Fehlerart::DateiNichtGefunden);
    EXPECT_EQ(c.holen("x", (lokal.p / "fehlt").u8string()).art, Fehlerart::PfadAbgewiesen);
    TempOrdner w;
    Freigabe f(w.p);
    EXPECT_EQ(c.serve(f).art, Fehlerart::LeitungGetrennt);
}

TEST(KermitSitzung, AbbruchMittenInDerDatei) {
    Aufbau a;
    a.starte();
    TempOrdner lokal;
    schreibe(lokal.p / "q", zufall(102400, 2));
    Sitzung* cp = nullptr;
    Rueckrufe r;
    r.fortschritt = [&](const std::string&, int64_t b, int64_t) {
        if (b > 20000) cp->abbrechen();
    };
    Sitzung c(a.leitung.ende(0), client(), r);
    cp = &c;
    const Ergebnis e = c.senden((lokal.p / "q").u8string(), "q");
    EXPECT_EQ(e.art, Fehlerart::Abgebrochen);
    // Der Server hat das E-Paket bekommen und die angefangene Datei verworfen
    EXPECT_TRUE(c.entferntBeenden().ok());
    a.faden.join();
    EXPECT_FALSE(fs::exists(a.wurzel.p / "q"));
}

TEST(KermitSitzung, AbbruchDerDateiBeimHolen) {
    Aufbau a;
    schreibe(a.wurzel.p / "gross", zufall(102400, 3));
    schreibe(a.wurzel.p / "gross2", zufall(1000, 3));
    a.starte();
    Sitzung* cp = nullptr;
    Rueckrufe r;
    r.fortschritt = [&](const std::string& d, int64_t b, int64_t) {
        if (d == "gross" && b > 10000) cp->abbrechenDatei();
    };
    Sitzung c(a.leitung.ende(0), client(), r);
    cp = &c;
    TempOrdner hol;
    const Ergebnis e = c.holen("gross*", hol.s());
    EXPECT_EQ(e.art, Fehlerart::Abgebrochen);
    EXPECT_FALSE(fs::exists(hol.p / "gross"));
    EXPECT_TRUE(fs::exists(hol.p / "gross2"));   // X bricht nur die eine Datei ab
}

TEST(KermitSitzung, AbbruchDerGruppeBeimSenden) {
    Aufbau a;
    a.starte();
    TempOrdner lokal;
    std::vector<SendeAuftrag> g;
    for (int i = 0; i < 3; ++i) {
        schreibe(lokal.p / ("d" + std::to_string(i)), zufall(30000, static_cast<unsigned>(i)));
        g.push_back({(lokal.p / ("d" + std::to_string(i))).u8string(), ""});
    }
    Sitzung* cp = nullptr;
    Rueckrufe r;
    r.fortschritt = [&](const std::string&, int64_t b, int64_t) {
        if (b > 5000) cp->abbrechenGruppe();
    };
    Sitzung c(a.leitung.ende(0), client(), r);
    cp = &c;
    const Ergebnis e = c.sendenGruppe(g);
    EXPECT_EQ(e.art, Fehlerart::Abgebrochen);
    EXPECT_TRUE(e.dateien.empty());
    for (int i = 0; i < 3; ++i) EXPECT_FALSE(fs::exists(a.wurzel.p / ("d" + std::to_string(i))));
    EXPECT_TRUE(c.entferntBeenden().ok());   // Sitzung bleibt benutzbar
}

TEST(KermitSitzung, EmpfaengerBrichtGruppeAb) {
    SpeicherLeitungPaar l;
    TempOrdner lokal, ziel;
    std::vector<SendeAuftrag> g;
    for (int i = 0; i < 3; ++i) {
        schreibe(lokal.p / ("d" + std::to_string(i)), zufall(30000, static_cast<unsigned>(i)));
        g.push_back({(lokal.p / ("d" + std::to_string(i))).u8string(), ""});
    }
    Ergebnis senderErg;
    std::thread t([&] {
        Sitzung s(l.ende(1), server());
        senderErg = s.sendenGruppe(g);
    });
    Sitzung* cp = nullptr;
    Rueckrufe r;
    r.fortschritt = [&](const std::string&, int64_t b, int64_t) {
        if (b > 5000) cp->abbrechenGruppe();
    };
    Sitzung c(l.ende(0), client(), r);
    cp = &c;
    const Ergebnis e = c.empfangen(ziel.s());
    t.join();
    EXPECT_EQ(e.art, Fehlerart::Abgebrochen);
    EXPECT_EQ(senderErg.art, Fehlerart::Abgelehnt);
    EXPECT_TRUE(fs::is_empty(ziel.p));
}

TEST(KermitSitzung, VollerDatentraeger) {
    Aufbau a;
    a.freigabe.setzePlatzGrenze(50000);
    a.starte();
    TempOrdner lokal;
    schreibe(lokal.p / "q", zufall(102400, 8));
    // ohne Attribute: Schreibfehler mitten in der Datei → E-Paket
    Einstellungen e = client();
    e.attribute = false;
    Sitzung c(a.leitung.ende(0), e);
    Ergebnis r = c.senden((lokal.p / "q").u8string(), "q");
    EXPECT_EQ(r.art, Fehlerart::Gegenstelle);
    EXPECT_NE(r.text.find("Datentraeger"), std::string::npos);
    EXPECT_FALSE(fs::exists(a.wurzel.p / "q"));
    // mit Attributen: Ablehnung schon am A-Paket (Laenge > Platz)
    a.freigabe.setzePlatzGrenze(50000);
    Sitzung c2(a.leitung.ende(0), client());
    r = c2.senden((lokal.p / "q").u8string(), "q");
    EXPECT_EQ(r.art, Fehlerart::Abgelehnt);
    EXPECT_FALSE(fs::exists(a.wurzel.p / "q"));
    EXPECT_TRUE(c2.entferntBeenden().ok());
}

TEST(KermitSitzung, PfadfallenUeberDasProtokoll) {
    TempOrdner draussen;
    schreibe(draussen.p / "geheim", "x");
    Aufbau a;
    std::error_code ec;
    fs::create_directory_symlink(draussen.p, a.wurzel.p / "aus", ec);
    const bool symlinks = !ec;
    a.starte();
    TempOrdner lokal, hol;
    schreibe(lokal.p / "q", "boese");
    Sitzung c(a.leitung.ende(0), client());
    for (const std::string& n : {std::string("../../etc/passwd"), std::string("/etc/passwd"), std::string("C:\\x"),
                                 std::string("a\0b", 3)}) {
        EXPECT_EQ(c.senden((lokal.p / "q").u8string(), n).art, Fehlerart::Gegenstelle) << n;
        EXPECT_EQ(c.holen(n, hol.s()).art, Fehlerart::Gegenstelle) << n;
    }
    if (symlinks) {
        EXPECT_EQ(c.holen("aus/geheim", hol.s()).art, Fehlerart::Gegenstelle);
        EXPECT_EQ(c.senden((lokal.p / "q").u8string(), "aus/neu").art, Fehlerart::Gegenstelle);
        EXPECT_FALSE(fs::exists(draussen.p / "neu"));
    }
    EXPECT_TRUE(fs::is_empty(hol.p));
}

TEST(KermitSitzung, HolenEmpfaengtBoeseNamenNicht) {
    // Ein boesartiger Server schickt "../x": der Client weist ihn ab
    SpeicherLeitungPaar l;
    TempOrdner ziel;
    std::thread t([&] {
        Roh r{l.ende(1)};
        Paket p;
        while (r.lies(p) && p.typ != 'R') {
            if (p.typ == 'I') r.sende('Y', 0, "~* @-#Y1~");
        }
        r.sende('S', 0, "~* @-#Y1~");
        r.lies(p);   // Y
        r.sende('F', 1, "../x");
        r.lies(p);   // E
        EXPECT_EQ(p.typ, 'E');
    });
    Sitzung c(l.ende(0), client());
    EXPECT_EQ(c.holen("x", ziel.s()).art, Fehlerart::PfadAbgewiesen);
    t.join();
    EXPECT_FALSE(fs::exists(ziel.p.parent_path() / "x"));
}

TEST(KermitSitzung, ServerAbbruchVonAussen) {
    Aufbau a;
    std::atomic<Sitzung*> sp{nullptr};
    std::atomic<bool> fertig{false};
    std::thread t([&] {
        Sitzung s(a.leitung.ende(1), server());
        sp = &s;
        a.serverErgebnis = s.serve(a.freigabe);
        fertig = true;
    });
    // serve() setzt den Schalter beim Start zurueck: so lange abbrechen, bis es endet
    while (!fertig) {
        if (Sitzung* s = sp.load()) s->abbrechen();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    t.join();
    EXPECT_EQ(a.serverErgebnis.art, Fehlerart::Abgebrochen);
}

TEST(KermitSitzung, FehlerTexte) {
    for (int i = 0; i <= static_cast<int>(Fehlerart::NichtUnterstuetzt); ++i)
        EXPECT_STRNE(fehlerText(static_cast<Fehlerart>(i)), "?");
}
