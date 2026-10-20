/**
 * @file test_kermit_tempo.cpp
 * @brief Adaptive Fristen der Kermit-Sitzung und des Login-Skripts gegen schnelle und langsame
 *        Gegenstellen (doc/design/36_kermit.md §6.5, AP K7).
 *
 * Hintergrund: der Hub des Emulators taktet die Gast-Baudrate in MASCHINENZEIT.  Ein P8000 mit 10x
 * Takt liefert Zeichen zehnmal so schnell wie 9600 Bd, unter Last ein Zehntel davon; die Fristen des
 * Gastes (C-Kermit: TIME 10 s) laufen ebenfalls in Maschinenzeit.  Nachgebildet wird das hier mit
 * der virtuellen Uhr des `SpeicherLeitungPaar` und seiner Zeichenzeit (eine Leitung, die ein Byte
 * nach dem anderen liefert): Gegenstelle 10x schneller (0.104 ms je Zeichen) und 10x langsamer
 * (10.4 ms je Zeichen, dazu Rechenzeit vor jeder Antwort).
 */
#include <gtest/gtest.h>

#include <fstream>
#include <random>
#include <string>
#include <thread>

#include "kermit/src/loginskript.h"
#include "kermit/src/protokoll.h"

namespace fs = std::filesystem;
using namespace k1520::kermit;

namespace {

constexpr double ZEICHEN_9600_MS = 10000.0 / 9600.0;   // 10 Bit je Zeichen

struct TempOrdner {
    fs::path p;
    TempOrdner() {
        std::random_device rd;
        p = fs::temp_directory_path() / ("k1520kermit_t_" + std::to_string(rd()) + std::to_string(rd()));
        fs::create_directories(p);
    }
    ~TempOrdner() {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
};

std::string zufall(std::size_t n, unsigned saat) {
    std::mt19937 g(saat);
    std::string s(n, '\0');
    for (auto& c : s) c = static_cast<char>(g());
    return s;
}
void schreibe(const fs::path& p, const std::string& s) { std::ofstream(p, std::ios::binary) << s; }
std::string lies(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

/// Gegenstelle wie C-Kermit 4.2 (9 Init-Felder, Pruefung 1, Paketlaenge 94, TIME = frist), die mit
/// dem Tempo @p tempo laeuft: ihre eigene Frist (10 s IHRER Zeit) ist in Wirtszeit 10 s / tempo.
Einstellungen gegenstelle(double tempo) {
    Einstellungen e;
    e.capas_senden = false;
    e.langpakete = false;
    e.attribute = false;
    e.qbin = 'Y';
    e.pruefsumme = 1;
    e.maxl = 94;
    e.frist_fest = true;   // die Gegenstelle rechnet in ihrer eigenen Zeit
    e.frist_ms = static_cast<int>(10000 / tempo);
    return e;
}

/// Server (Gegenstelle) an Ende 1 im eigenen Faden, Client an Ende 0; Leitung mit Zeichenzeit
/// fuer beide Richtungen, Rechenzeit der Gegenstelle als Verzoegerung ihrer Antworten.
struct Anordnung {
    SpeicherLeitungPaar leitung;
    TempOrdner wurzel, ziel;
    Freigabe freigabe{wurzel.p};
    std::thread faden;

    Anordnung(double tempo, int64_t rechenzeitMs) {
        leitung.setzeZeichenzeit(0, ZEICHEN_9600_MS / tempo);
        leitung.setzeZeichenzeit(1, ZEICHEN_9600_MS / tempo);
        leitung.setzeVerzoegerung(1, rechenzeitMs);
        const Einstellungen e = gegenstelle(tempo);
        faden = std::thread([this, e] {
            Sitzung s(leitung.ende(1), e);
            s.serve(freigabe);
            leitung.ende(1).schliessen();   // sonst steht die virtuelle Uhr (Ende nimmt nicht mehr teil)
        });
    }
    ~Anordnung() {
        leitung.ende(0).schliessen();
        if (faden.joinable()) faden.join();
    }
};

Einstellungen clientAuto() {
    Einstellungen e;
    e.pruefsumme = 1;
    return e;   // tempo = 0: Automatik, frist_ms = 10000 (Zeit der Gegenstelle)
}

}  // namespace

TEST(KermitTempo, FristFolgtDerEinstellung) {
    SpeicherLeitungPaar l;
    Einstellungen e;
    Sitzung s(l.ende(0), e);
    EXPECT_EQ(s.aktuelleFrist(), 10000);   // Automatik ohne Messung: Tempo 1
    s.einstellungen().tempo = 0.5;
    EXPECT_EQ(s.aktuelleFrist(), 20000);
    s.einstellungen().tempo = 10;
    EXPECT_EQ(s.aktuelleFrist(), 10000);   // eine schnelle Gegenstelle bekommt nicht weniger Zeit
    s.einstellungen().frist_fest = true;
    EXPECT_EQ(s.aktuelleFrist(), 10000);   // alter Weg: fest in Wirtszeit
    s.einstellungen().frist_fest = false;
    s.einstellungen().tempo = 0;
    s.setzeTempoSchaetzung(0.25);           // z. B. aus dem Login-Text
    EXPECT_DOUBLE_EQ(s.tempo(), 0.25);
    EXPECT_EQ(s.aktuelleFrist(), 40000);
}

// Gegenstelle 10x schneller: die Schaetzung kommt aus der Zeichenrate der Datenpakete.  Die eigene
// Frist bleibt bei 10 s: nach einem Verlust heilt die Gegenstelle mit IHRER Frist (1 s Wirtszeit);
// eine gleich kurze eigene Frist kreuzte die Wiederholungen (Befund: 122 statt 0).
TEST(KermitTempo, SchnelleGegenstelleHeiltSelbst) {
    auto lauf = [](bool fest, double* dauer, double* tempo, int* wdh) {
        Anordnung a(10.0, 0);
        const std::string inhalt = zufall(30000, 7);
        schreibe(a.wurzel.p / "daten.bin", inhalt);
        // Jeder 7. Block der Gegenstelle geht verloren (Ringpuffer, Leitungsstoerung).
        a.leitung.setzeStoerung(1, SpeicherLeitungPaar::verlustJedes(7));
        Einstellungen e = clientAuto();
        if (fest) {
            e.frist_fest = true;
            e.frist_ms = 10000;
        }
        Sitzung c(a.leitung.ende(0), e);
        const int64_t t0 = a.leitung.jetzt();
        const Ergebnis r = c.holen("daten.bin", a.ziel.p.u8string());
        ASSERT_TRUE(r.ok()) << r.text;
        EXPECT_EQ(lies(a.ziel.p / "daten.bin"), inhalt);
        *dauer = static_cast<double>(a.leitung.jetzt() - t0);
        *tempo = c.tempoSchaetzung();
        *wdh = c.statistik().naks_gesendet + c.statistik().wiederholungen;
        c.entferntBeenden();
    };
    double dAuto = 0, dFest = 0, tAuto = 0, tFest = 0;
    int wAuto = 0, wFest = 0;
    lauf(false, &dAuto, &tAuto, &wAuto);
    lauf(true, &dFest, &tFest, &wFest);
    std::fprintf(stderr, "  10x: Automatik %.1f s (Tempo %.1f, %d Wdh.), fest 10 s: %.1f s (%d Wdh.)\n", dAuto / 1000, tAuto,
                 wAuto, dFest / 1000, wFest);
    EXPECT_GT(tAuto, 7.0);
    EXPECT_LT(tAuto, 13.0);
    EXPECT_LE(wAuto, wFest + 2) << "keine gekreuzten Wiederholungen";
    EXPECT_LT(dAuto, dFest * 1.2);
}

// Gegenstelle 10x langsamer (Emulator unter Last) mit 3 s Rechenzeit vor jeder Antwort (fork von
// `ls`, Plattenzugriff): eine feste 2-s-Frist wiederholt staendig ins Leere; die adaptive wartet ab.
TEST(KermitTempo, LangsameGegenstelleKeineUnnoetigenWiederholungen) {
    auto lauf = [](bool fest, int* wdh, double* tempo, bool* ok) {
        Anordnung a(0.1, 3000);
        const std::string inhalt = zufall(3000, 11);
        schreibe(a.wurzel.p / "daten.bin", inhalt);
        Einstellungen e = clientAuto();
        if (fest) {
            e.frist_fest = true;
            e.frist_ms = 2000;
        }
        Sitzung c(a.leitung.ende(0), e);
        const Ergebnis r = c.holen("daten.bin", a.ziel.p.u8string());
        *ok = r.ok() && lies(a.ziel.p / "daten.bin") == inhalt;
        *wdh = c.statistik().wiederholungen + c.statistik().naks_gesendet;
        *tempo = c.tempoSchaetzung();
        if (r.ok()) c.entferntBeenden();
    };
    int wAuto = 0, wFest = 0;
    double tAuto = 0, tFest = 0;
    bool okAuto = false, okFest = false;
    lauf(false, &wAuto, &tAuto, &okAuto);
    lauf(true, &wFest, &tFest, &okFest);
    std::fprintf(stderr, "  0.1x: Automatik %d Wdh. (Tempo %.3f, ok %d), fest 2 s: %d Wdh. (ok %d)\n", wAuto, tAuto, okAuto,
                 wFest, okFest);
    EXPECT_TRUE(okAuto);
    EXPECT_EQ(wAuto, 0) << "keine Wiederholung ins Leere bei einer langsamen, aber heilen Gegenstelle";
    EXPECT_GT(tAuto, 0.07);
    EXPECT_LT(tAuto, 0.13);
    EXPECT_GT(wFest, wAuto) << "Gegenprobe: die feste Frist wiederholt";
}

// TIME der Gegenstelle begrenzt nur den Zuschlag aus der Umlaufzeit (in IHRER Zeit), nie die
// eigene Grundfrist: Antworten kommen hier erst nach 3 s, 4 x Umlaufzeit waeren 12 s.
TEST(KermitTempo, TimeDerGegenstelleBegrenztDenZuschlag) {
    SpeicherLeitungPaar l;
    l.setzeVerzoegerung(1, 3000);
    TempOrdner w;
    Freigabe f(w.p);
    Einstellungen g = gegenstelle(1.0);
    g.frist_ms = 5000;   // TIME 5
    std::thread t([&] {
        Sitzung s(l.ende(1), g);
        s.serve(f);
        l.ende(1).schliessen();
    });
    Einstellungen e;
    e.frist_ms = 4000;
    Sitzung c(l.ende(0), e);
    ASSERT_TRUE(c.entferntVerzeichnis("").ok());
    EXPECT_EQ(c.vereinbarung().time, 5);
    EXPECT_GT(c.umlaufzeit(), 2500.0);
    EXPECT_EQ(c.aktuelleFrist(), 5000);   // ohne TIME: 4 x Umlaufzeit = 12 s
    c.entferntBeenden();
    t.join();
}

// ─── Login-Skript ──────────────────────────────────────────────────────────

namespace {
/// getty + login + Server-Banner als Attrappe; schreibt Zeichen fuer Zeichen.
void gettyAttrappe(Leitung& l, int motdBytes, int* returns) {
    auto sende = [&](const std::string& s) { l.schreiben(reinterpret_cast<const uint8_t*>(s.data()), s.size()); };
    std::string ein;
    int phase = 0;
    uint8_t b[64];
    for (;;) {
        const int n = l.lesen(b, sizeof b, 600000);
        if (n <= 0) return;
        for (int i = 0; i < n; ++i) {
            const char c = static_cast<char>(b[i]);
            if (c != '\r') {
                ein += c;
                continue;
            }
            if (phase == 0 && ein.empty()) {
                ++*returns;
                sende("\r\nWEGA login: ");
            } else if (phase == 0 && ein == "kermit") {
                phase = 1;
                sende("\r\nPassword:");
            } else if (phase == 1 && ein == "kermit") {
                phase = 2;
                sende("\r\n" + std::string(static_cast<std::size_t>(motdBytes), '*') +
                      "\r\nC-Kermit server starting.  Return to your local machine...\r\n");
            }
            ein.clear();
        }
    }
}
}  // namespace

// Gegenstelle 10x langsamer: die Tagesmeldung (3000 Zeichen) braucht 31 s Wirtszeit — laenger als
// die Frist auf den Serverbeginn.  Die Frist ist eine Frist der STILLE und haelt; die Zeichenrate
// liefert das Tempo.
TEST(KermitTempo, LoginWartetAufStilleNichtAufSekunden) {
    SpeicherLeitungPaar l;
    l.setzeZeichenzeit(1, ZEICHEN_9600_MS / 0.1);
    l.setzeZeichenzeit(0, ZEICHEN_9600_MS / 0.1);
    int returns = 0;
    std::thread t([&] { gettyAttrappe(l.ende(1), 3000, &returns); });
    LoginOptionen o;
    o.frist_server_ms = 20000;
    const LoginErgebnis r = loginSkript(l.ende(0), o);
    l.ende(0).schliessen();
    t.join();
    ASSERT_TRUE(r.ok) << r.text;
    EXPECT_EQ(returns, 1) << "RETURN nur bei stiller Leitung wiederholen";
    EXPECT_GT(r.tempo, 0.07);
    EXPECT_LT(r.tempo, 0.13);
}

// Ein ausdrueckliches Tempo teilt die Fristen: eine stumme Gegenstelle mit Tempo 10 gilt nach
// 6 s statt 60 s Wirtszeit als stumm.
TEST(KermitTempo, LoginMitTempoTeiltDieFristen) {
    SpeicherLeitungPaar l;
    l.setzeBeteiligt(1, false);   // stumm
    LoginOptionen o;
    o.tempo = 10;
    o.frist_login_ms = 60000;
    const int64_t t0 = l.jetzt();
    const LoginErgebnis r = loginSkript(l.ende(0), o);
    const int64_t dauer = l.jetzt() - t0;
    EXPECT_FALSE(r.ok);
    EXPECT_GE(dauer, 6000);
    EXPECT_LT(dauer, 7000);
}
