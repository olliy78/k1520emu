/**
 * @file test_p8000_rtc_zeit.cpp
 * @brief Zeitmodell der P8000-Uhrenkarten in MASCHINENZEIT (Entwurf 37, AP R2b): die Uhr zaehlt Takte, die
 *        Batteriedatei haelt Stand + Wirtszeit, die Startregel max(Stand + Abwesenheit, Wirtszeit) laesst die
 *        Uhr beim Neustart nie zurueckspringen (Schnelllauf-Waechter), von aussen stellen, Jahresversatz,
 *        statische Startzeit, periodisches Speichern, Zwischenstand, Batteriedatei aus R1.
 *        Beide Kartentypen (typisiert); Karte direkt mit kleinem Takt (1 kHz: 1 Uhrsekunde = 1000 Takte).
 */

#include <gtest/gtest.h>
#include "core/cards/p8000/rtc.h"
#include "core/util/zustand.h"
#include "tests/support/temp_path.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

int64_t utc(int j, int mo, int t, int h = 0, int mi = 0, int s = 0) {
    const int64_t y = j - (mo <= 2);
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + t - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (era * 146097 + doe - 719468) * 86400 + h * 3600 + mi * 60 + s;
}

constexpr uint32_t TAKT = 1000;

template <P8000RtcTyp T> struct Typ { static constexpr P8000RtcTyp wert = T; };

template <class TT> struct P8000RtcZeit : ::testing::Test {
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);   ///< Wirtszeit (UTC)
    std::string datei;
    std::unique_ptr<P8000Rtc> r;

    void SetUp() override {
        datei = k1520test::tempPath("k1520_test_rtc_zeit.txt");
        std::remove(datei.c_str());
    }
    void TearDown() override {
        r.reset();
        std::remove(datei.c_str());
    }

    P8000RtcConfig konfig(int jahresversatz = 0, bool mitDatei = true) {
        P8000RtcConfig c;
        c.typ = TT::wert;
        c.utc = true;
        c.jahresversatz = jahresversatz;
        if (mitDatei) c.zustand = datei;
        c.zeitquelle = [this] { return jetzt; };
        c.takt_hz = TAKT;
        return c;
    }
    void bestuecken(int jahresversatz = 0, bool mitDatei = true) {
        std::string f;
        auto neu = P8000Rtc::anlegen(konfig(jahresversatz, mitDatei), f);   // erst bauen, dann den alten loesen
        r = std::move(neu);
    }
    /// @p sek Sekunden MASCHINENZEIT (die Wirtszeit steht); in Schritten von 100 Takten wie ein Befehlslauf
    /// (das Speicherintervall greift an der Schrittgrenze).
    void laufe(int64_t sek, P8000Rtc* k = nullptr) {
        if (!k) k = r.get();
        for (int64_t t = sek * TAKT; t > 0;) { const int n = int(std::min<int64_t>(t, 100)); k->takte(n); t -= n; }
    }
    std::string datei_text() const {
        std::ifstream f(datei);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
    int64_t datei_stand() const {
        const std::string t = datei_text();
        const size_t p = t.find("stand=");
        return p == std::string::npos ? -1 : std::stoll(t.substr(p + 6));
    }
    static std::string text(int64_t stand, int64_t wirt) {
        return "K1520-P8000-RTC 2\nstand=" + std::to_string(stand) + "\nwirt=" + std::to_string(wirt) + "\n";
    }
    void schreibe(const std::string& t) { std::ofstream(datei, std::ios::trunc) << t; }

    /// Gast stellt die Uhr (nur RTC-72421 hat Register; beim U130X ueber `stellen`).
    void gastStellt(int64_t wand) {
        if (TT::wert == P8000RtcTyp::Rtc72421) {
            int j, mo, t;
            tagesDatum(wand, j, mo, t);
            const int sod = int(wand - (wand / 86400) * 86400);
            auto w = [&](uint16_t z, uint16_t e, int v) { r->schreiben(z, uint8_t(v / 10)); r->schreiben(e, uint8_t(v % 10)); };
            r->schreiben(0xFDFF, 0x07);
            w(0xFDE3, 0xFDE1, sod % 60); w(0xFDE7, 0xFDE5, sod / 60 % 60); w(0xFDEB, 0xFDE9, sod / 3600);
            w(0xFDEF, 0xFDED, t); w(0xFDF3, 0xFDF1, mo); w(0xFDF7, 0xFDF5, j % 100);
            r->schreiben(0xFDFF, 0x04);
        } else {
            r->stellen(wand);
        }
    }
    static void tagesDatum(int64_t wand, int& j, int& mo, int& t) {
        int64_t z = wand / 86400 + 719468;
        const int64_t era = z / 146097, doe = z - era * 146097;
        const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
        t = int(doy - (153 * mp + 2) / 5 + 1);
        mo = int(mp < 10 ? mp + 3 : mp - 9);
        j = int(yoe + era * 400 + (mo <= 2));
    }
};

using Typen = ::testing::Types<Typ<P8000RtcTyp::Rtc72421>, Typ<P8000RtcTyp::U130X>>;
TYPED_TEST_SUITE(P8000RtcZeit, Typen);

TYPED_TEST(P8000RtcZeit, ErststartOhneDateiStartetMitDerWirtszeit) {
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), this->jetzt);
}

TYPED_TEST(P8000RtcZeit, MaschinenzeitZaehltUnabhaengigVonDerWirtszeit) {
    this->bestuecken(0, false);
    this->laufe(90);
    EXPECT_EQ(this->r->gastZeit(), this->jetzt + 90);
    this->jetzt += 5000;   // die Wirtsuhr laeuft weiter - die Uhr der Karte nicht mit
    this->laufe(10);
    EXPECT_EQ(this->r->gastZeit(), utc(2026, 10, 22, 14, 35, 46) + 100);
}

TYPED_TEST(P8000RtcZeit, SekundenUndRestTakteZaehlenGenau) {
    this->bestuecken(0, false);
    this->r->takte(999);
    EXPECT_EQ(this->r->gastZeit(), this->jetzt);
    this->r->takte(1);
    EXPECT_EQ(this->r->gastZeit(), this->jetzt + 1);
    this->r->takte(2500);
    EXPECT_EQ(this->r->gastZeit(), this->jetzt + 3);
}

TYPED_TEST(P8000RtcZeit, AngehalteneMaschineLaesstDieUhrStehen) {
    this->bestuecken(0, false);
    this->laufe(10);
    const int64_t z = this->r->gastZeit();
    this->jetzt += 3600;   // Pause: keine Takte
    EXPECT_EQ(this->r->gastZeit(), z);
}

// ── Schnelllauf-Waechter ─────────────────────────────────────────────────────

TYPED_TEST(P8000RtcZeit, SchnelllaufKeinRuecksprungBeimNeustart) {
    const int64_t h0 = this->jetzt;
    this->bestuecken();
    this->laufe(5 * 3600);          // Maschinenzeit 5 h ...
    this->jetzt = h0 + 3600;        // ... in 1 h Wirtszeit (5facher Takt)
    const int64_t stand = this->r->gastZeit();
    ASSERT_EQ(stand, h0 + 5 * 3600);
    this->r.reset();                // Beenden: Batteriedatei
    ASSERT_EQ(this->datei_stand(), stand);
    this->bestuecken();             // Neustart sofort: nie zurueck
    EXPECT_GE(this->r->gastZeit(), stand);
    EXPECT_EQ(this->r->gastZeit(), stand);
    // Die Wirtszeit holt auf: laeuft die Maschine langsamer als der Wirt (100 s Maschinenzeit in 5,5 h
    // Wirtszeit), ueberholt die Wirtszeit die Uhr und gilt beim naechsten Start.
    this->laufe(100);
    this->jetzt = h0 + 3600 + 20000;
    this->r.reset();
    ASSERT_EQ(this->datei_stand(), stand + 100);
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), this->jetzt);
    EXPECT_GT(this->jetzt, stand + 100);
}

TYPED_TEST(P8000RtcZeit, AbwesenheitWirdAddiert) {
    const int64_t h0 = this->jetzt;
    this->bestuecken();
    this->laufe(1000);
    this->r.reset();
    EXPECT_EQ(this->datei_stand(), h0 + 1000);
    this->jetzt = h0 + 400;         // 400 s abwesend, Wirtszeit hinter dem Stand
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), h0 + 1000 + 400);
}

TYPED_TEST(P8000RtcZeit, EineVorausgelaufeneUhrHoltAufSobaldDieWirtszeitSieUeberholt) {
    const int64_t h0 = this->jetzt;
    this->schreibe(TestFixture::text(h0 + 100, h0));   // Stand 100 s voraus, gespeichert zur Wirtszeit h0
    this->jetzt = h0 + 3000;
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), h0 + 3100);          // Stand + Abwesenheit (3000) gewinnt gegen h0+3000
    this->r.reset();
    // Wirtsuhr ueberholt: Stand zu klein gegen die Wirtszeit (Datei von Hand auf alt gesetzt)
    this->schreibe(TestFixture::text(h0 - 50000, h0));
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), this->jetzt);
}

TYPED_TEST(P8000RtcZeit, WirtsuhrRueckwaertsZaehltAlsNullAbwesenheit) {
    const int64_t h0 = this->jetzt;
    this->schreibe(TestFixture::text(h0 + 500, h0));
    this->jetzt = h0 - 7200;        // Wirtsuhr zurueckgestellt
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), h0 + 500);
}

TYPED_TEST(P8000RtcZeit, KaputteOderUnbekannteDateiGiltAlsKeineDatei) {
    this->schreibe("Muell\nstand=5\n");
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), this->jetzt);
    this->r.reset();
    this->schreibe("K1520-P8000-RTC 2\nstand=abc\nwirt=1\n");
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), this->jetzt);
}

TYPED_TEST(P8000RtcZeit, BatteriedateiAusR1MitVersatz) {
    this->schreibe("K1520-P8000-RTC 1\nversatz=3600\n");
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), this->jetzt + 3600);
    this->r.reset();
    this->schreibe("K1520-P8000-RTC 1\nversatz=-3600\n");   // nie zurueck: die Wirtszeit gilt
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), this->jetzt);
    this->r.reset();
    EXPECT_NE(this->datei_text().find("K1520-P8000-RTC 2"), std::string::npos);   // beim Beenden migriert
}

// ── Stellen ──────────────────────────────────────────────────────────────────

TYPED_TEST(P8000RtcZeit, GastStelltRueckwaertsUndDasGiltSofortUndWirdGespeichert) {
    this->bestuecken();
    const int64_t ziel = utc(2026, 10, 22, 13, 0, 0);   // 1 h 35 min vor der Wirtszeit
    this->gastStellt(ziel);
    EXPECT_NEAR(double(this->r->gastZeit()), double(ziel), 1.0);
    EXPECT_NEAR(double(this->datei_stand()), double(ziel), 1.0);   // sofort, ohne Beenden
    this->laufe(60);
    EXPECT_NEAR(double(this->r->gastZeit()), double(ziel + 60), 1.0);
}

TYPED_TEST(P8000RtcZeit, StellenVonAussenMitJahresversatz) {
    for (int vs : {0, -28, -40}) {
        this->r.reset();
        std::remove(this->datei.c_str());
        this->bestuecken(vs);
        const int64_t ziel = utc(2026, 12, 31, 23, 59, 30);
        this->r->stellen(ziel);
        EXPECT_EQ(this->r->gastZeit(), utc(2026 + vs, 12, 31, 23, 59, 30)) << vs;   // Gast sieht das versetzte Jahr
        EXPECT_EQ(this->datei_stand(), ziel) << vs;                                  // gespeichert wird unversetzt
        this->laufe(30);
        EXPECT_EQ(this->r->gastZeit(), utc(2027 + vs, 1, 1, 0, 0, 0)) << vs;
    }
}

TYPED_TEST(P8000RtcZeit, JahresversatzMachtAusDem29ZumNicht29) {
    this->bestuecken(-1);
    this->r->stellen(utc(2028, 2, 29, 12, 0, 0));
    EXPECT_EQ(this->r->gastZeit(), utc(2027, 2, 28, 12, 0, 0));
}

// ── Statische Startzeit ──────────────────────────────────────────────────────

TYPED_TEST(P8000RtcZeit, StatischeStartzeitIstDerStartwertEinerNeuenKarteBitgleich) {
    const int64_t h0 = this->jetzt;
    const std::vector<std::string> inhalte = {
        "",                                                  // keine Datei
        TestFixture::text(h0 + 9000, h0 - 100),              // vorausgelaufen
        TestFixture::text(h0 - 90000, h0 - 100),             // lange her
        TestFixture::text(h0 + 50, h0 + 10000),              // Wirtsuhr zurueckgestellt
        "K1520-P8000-RTC 1\nversatz=1234\n",                 // R1
        "Muell",
    };
    for (const auto& in : inhalte)
        for (int vs : {0, -28, -40}) {
            this->r.reset();   // der Destruktor schreibt die Datei - vor dem Praeparieren
            std::remove(this->datei.c_str());
            if (!in.empty()) this->schreibe(in);
            const int64_t st = P8000Rtc::startzeit(this->datei, vs, true, this->konfig().zeitquelle);
            this->bestuecken(vs);
            EXPECT_EQ(this->r->gastZeit(), st) << in << " vs=" << vs;
        }
    // ohne Dateipfad
    EXPECT_EQ(P8000Rtc::startzeit("", 0, true, this->konfig().zeitquelle), h0);
}

TYPED_TEST(P8000RtcZeit, StatischesStellenDerDateiWirktBeimNaechstenStart) {
    const int64_t ziel = this->jetzt + 7200;
    ASSERT_TRUE(P8000Rtc::stellenDatei(this->datei, ziel, this->konfig().zeitquelle));
    EXPECT_EQ(this->datei_stand(), ziel);
    EXPECT_EQ(P8000Rtc::startzeit(this->datei, 0, true, this->konfig().zeitquelle), ziel);
    this->bestuecken();
    EXPECT_EQ(this->r->gastZeit(), ziel);
    EXPECT_FALSE(P8000Rtc::stellenDatei("", ziel));
    EXPECT_FALSE(P8000Rtc::stellenDatei("/gibt/es/nicht/rtc.txt", ziel));
}

// ── Speichern ────────────────────────────────────────────────────────────────

TYPED_TEST(P8000RtcZeit, SpeichertJeMinuteMaschinenzeitNichtJeBefehl) {
    const int64_t h0 = this->jetzt;
    this->bestuecken();
    EXPECT_FALSE(std::filesystem::exists(this->datei));   // beim Anlegen noch nicht
    for (int i = 0; i < 59 * 1000; i += 10) this->r->takte(10);
    EXPECT_FALSE(std::filesystem::exists(this->datei));   // 59 s: nichts geschrieben
    this->laufe(2);
    ASSERT_TRUE(std::filesystem::exists(this->datei));
    EXPECT_EQ(this->datei_stand(), h0 + 60);              // geschrieben bei 60 s
    this->laufe(30);
    EXPECT_EQ(this->datei_stand(), h0 + 60);              // dazwischen nicht
    this->laufe(30);
    EXPECT_EQ(this->datei_stand(), h0 + 120);
}

TYPED_TEST(P8000RtcZeit, ZerstoerenSchreibtDenStand) {
    this->bestuecken();
    this->laufe(17);
    this->r.reset();
    EXPECT_EQ(this->datei_stand(), this->jetzt + 17);
    EXPECT_NE(this->datei_text().find("wirt=" + std::to_string(this->jetzt)), std::string::npos);
}

// ── Zwischenstand ────────────────────────────────────────────────────────────

TYPED_TEST(P8000RtcZeit, ZwischenstandStelltDenStandWiederHerUndSchreibtDieDatei) {
    const int64_t h0 = this->jetzt;
    this->bestuecken(0, false);
    this->laufe(777);
    std::vector<uint8_t> b;
    this->r->serialize(b);
    const int64_t gemerkt = this->r->gastZeit();
    // eine andere Karte, spaeter gestartet, mit Batteriedatei
    this->jetzt = h0 + 100000;
    this->bestuecken();
    ASSERT_EQ(this->r->gastZeit(), this->jetzt);
    const uint8_t* p = b.data();
    ASSERT_TRUE(this->r->deserialize(p, b.data() + b.size()));
    EXPECT_EQ(this->r->gastZeit(), gemerkt);              // wie er war, konsistent zur geladenen Kernzeit
    EXPECT_EQ(this->datei_stand(), gemerkt);              // und die Batteriedatei folgt
    this->laufe(5);
    EXPECT_EQ(this->r->gastZeit(), gemerkt + 5);
}

TYPED_TEST(P8000RtcZeit, ZwischenstandHaeltDenRestTakt) {
    this->bestuecken(0, false);
    this->r->takte(700);
    std::vector<uint8_t> b;
    this->r->serialize(b);
    this->bestuecken(0, false);
    const uint8_t* p = b.data();
    ASSERT_TRUE(this->r->deserialize(p, b.data() + b.size()));
    this->r->takte(299);
    EXPECT_EQ(this->r->gastZeit(), this->jetzt);
    this->r->takte(1);
    EXPECT_EQ(this->r->gastZeit(), this->jetzt + 1);
}

// ── Staende aus R1/R2 (Versatz) ──────────────────────────────────────────────

TEST(P8000RtcZeitAlt, Zwischenstand72421AusR1LaedtAlsWirtszeitPlusVersatz) {
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
    P8000RtcConfig c;
    c.typ = P8000RtcTyp::Rtc72421; c.utc = true; c.takt_hz = TAKT;
    c.zeitquelle = [&] { return jetzt; };
    std::string f;
    auto r = P8000Rtc::anlegen(c, f);
    std::vector<uint8_t> b;
    b.push_back(1);   // R1: Version 1, Versatz zuerst
    auto a = k1520::ZAr::schreiber(b);
    int64_t vs = 7200;
    uint8_t d = 0, e = 0, ff = 0x04;
    bool h = false, g = false;
    uint64_t ph = 0;
    uint8_t zeit[13] = {};
    a.num(vs); a.num(d); a.num(e); a.num(ff); a.flag(h); a.flag(g); a.num(ph);
    a.raw(zeit, sizeof zeit);
    const uint8_t* p = b.data();
    ASSERT_TRUE(r->deserialize(p, b.data() + b.size()));
    EXPECT_EQ(r->gastZeit(), jetzt + 7200);
}

TEST(P8000RtcZeitAlt, ZwischenstandU130AusR2LaedtAlsWirtszeitPlusVersatz) {
    int64_t jetzt = utc(2026, 10, 22, 14, 35, 46);
    P8000RtcConfig c;
    c.typ = P8000RtcTyp::U130X; c.utc = true; c.takt_hz = TAKT;
    c.zeitquelle = [&] { return jetzt; };
    std::string f;
    auto r = P8000Rtc::anlegen(c, f);
    std::vector<uint8_t> b;
    b.push_back(0x81);   // R2: Versatz, dann Rahmenwerte
    auto a = k1520::ZAr::schreiber(b);
    int64_t vs = -600, fr = 0;
    uint8_t z = 0, t = 0, mo = 1, dz = 0, de = 1, jz = 0, je = 0, sd = 0, mz = 0, me = 0;
    uint64_t tot = 0, db = 0, sb = 0;
    a.num(vs); a.num(fr); a.num(z); a.num(t); a.num(tot); a.num(db); a.num(sb);
    a.num(mo); a.num(dz); a.num(de); a.num(jz); a.num(je); a.num(sd); a.num(mz); a.num(me);
    const uint8_t* p = b.data();
    ASSERT_TRUE(r->deserialize(p, b.data() + b.size()));
    EXPECT_EQ(r->gastZeit(), jetzt - 600);
}

}  // namespace
