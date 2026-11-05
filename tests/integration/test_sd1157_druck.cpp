/**
 * @file test_sd1157_druck.cpp
 * @brief Der SD 1157 druckt mit dem Original-ROM auf das Blatt im Kern (Entwurf 49, WP3/WP2b).
 *
 * **Selbsttest mit unabhaengigem Orakel.**  Die erwarteten Punkte entstehen NICHT aus einem Lauf des
 * Emulators, sondern aus dem Zeichengenerator `1157zg.rom` und dem Text darin (Offset 0300H, hardware.md §3, §12):
 *
 *   - Glyphe = (Code - 20H) * 8, Byte 0-6 = Spalten, Bit n = Nadel n+1, **0 = feuert**, Byte 7 = Nadel 9
 *     (Spalte c = Bit 6-c);
 *   - Normaldruck: Zeichenzelle 10 Takte, Zeichen n beginnt bei Takt 80 + 10 n, Spalte k bei +k
 *     (hardware.md §10.1/§10.5: `at = dpz + 8`, 10 Takte je Zelle);
 *   - Breitdruck (`ESC [ 1 m`, aus dem Text): Zelle 20 Takte, Spalte k bei +2k aus Spalte[k] UND Spalte[k-1] (low-aktiv:
 *     feuert, wenn eine von beiden feuert; je Zeichen beginnt die Vorspalte bei FFH), danach die Spalte 6 noch einmal bei +14
 *     (Quelle m1d95, `m1dcf`/`m1de2`);
 *   - Schraegdruck (`ESC [ 3 m`): Nadel j (0-7) um 8-j Takte nach rechts geschoert, Nadel 9 nicht (Quelle m1e1b..: Ring mit
 *     einem Takt je Nadelzeile; der absolute Versatz +1 fuer Nadel 8 ist am Lauf gewonnen und an allen 20 Zeichen bestaetigt);
 *   - Zeilenabstand: ein LF = 8 (6 lpi) bzw. 6 (8 lpi) Papierschritte zu 3 Rasterzeilen, vor JEDER Zeile ein LF.
 *
 * Passt das Bild nicht, steht in der Meldung ein ASCII-Abzug (erwartet / gedruckt).  Das Orakel wird nicht dem Emulator
 * angepasst: ein Unterschied ist ein Befund.  Ein Befund war die **Flugzeit** des Kopfes: die Firmware druckt rueckwaerts einen Takt
 * spaeter als vorwaerts; nur mit 0,5 Takt Flugzeit je Richtung (NadeldruckParameter::versatz_vor/_rueck) landen beide Richtungen auf
 * denselben Punkten (Test OhneFlugzeit...).
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/machines/printer/printer_machine.h"
#include "tests/support/sd1157_orakel.h"

namespace {

using namespace sd1157test;

// ─── Lauf ───────────────────────────────────────────────────────────────────
struct Drucker {
    std::unique_ptr<PrinterMachine> m;
    std::map<int, std::set<int>> richtungen;   // Zeile der Nadel 1 -> Fahrtrichtungen, in denen dort Punkte ausgeloest wurden
    int kopf_weg_beim_druck = 0;               // Anschlaege mit Nadeln bei nicht angeschwenktem Kopf

    explicit Drucker(PrinterMachine::Config cfg = {})
    {
        m = std::make_unique<PrinterMachine>(cfg);
        Sd1157Steuerung& st = m->sd1157();
        // Beobachter: jeder Anschlag (FGDI 2B.5 steigt, Nadeln gesetzt)
        m->setBusTrace([this, &st](bool io, bool rd, uint16_t a, uint8_t d) {
            if (io && !rd && (a & 0xFF) == 0xF9 && (d & 0x20) && st.nadeln()) {
                richtungen[st.druck().papierY(0)].insert(st.druck().wagen().richtung());
                if (!st.kopfAngeschwenkt()) ++kopf_weg_beim_druck;
            }
        });
    }
    Sd1157Steuerung& st() { return m->sd1157(); }
    void fahre(uint64_t takte)
    {
        const uint64_t ziel = m->totalCycles() + takte;
        while (m->totalCycles() < ziel) m->run(20000);
    }
    /// TEST-Taste im Offline-Zustand: gedrueckt halten, bis der Teststrom zu Ende ist (hpt3), loslassen, ausdrucken lassen.
    void selbsttest()
    {
        fahre(5'000'000);                                  // Grundstellung
        st().setTasteTest(true);
        bool los = false;
        m->setCpuTraceCallback([&](const Z80& c) {
            if (c.PC == HPT3 && !los) { los = true; st().setTasteTest(false); }
        });
        fahre(40'000'000);                                 // Zeilen, Formularvorschub, Neustart bis in die Grundstellung
        m->setCpuTraceCallback(nullptr);
        ASSERT_TRUE(los) << "der Teststrom kam nicht bis zum Ende";
    }
};

PrinterMachine::Config offline(bool lpi6 = true)
{
    PrinterMachine::Config c;
    c.sd1157.online = false;   // Taste ONLINE offline: nur dann wirken TEST und PV
    c.sd1157.lpi6 = lpi6;
    return c;
}

// erwartete Pixel je Testzeile, y der Zeile i = Rand + (i+1) LF
std::vector<Pixelmenge> orakel(const Drucker& d, const Zg& zg, const std::vector<Zeile>& zeilen, bool lpi6, std::vector<int>* ys = nullptr)
{
    const NadeldruckParameter& p = d.m->sd1157().druck().parameter();
    const int lf = (lpi6 ? 8 : 6) * p.pv_schritt_y;
    std::vector<Pixelmenge> r;
    for (size_t i = 0; i < zeilen.size(); ++i) {
        const int y0 = p.rand_oben_y + static_cast<int>(i + 1) * lf + p.nadel1_y;
        if (ys) ys->push_back(y0);
        r.push_back(orakelZeile(zeilen[i], zg, y0, p.nadel_teilung_y));
    }
    return r;
}

}  // namespace

// ─── Selbsttest ──────────────────────────────────────────────────────────────

TEST(Sd1157Selbsttest, TestzeilenKommenAusDemAbzug)
{
    // Das Orakel liest den Teststrom selbst; ein falsch gelesener Strom machte jeden folgenden Vergleich wertlos.
    const Zg zg = Zg::laden();
    ASSERT_EQ(zg.b.size(), 1024u);
    const auto z = testZeilen(zg);
    ASSERT_EQ(z.size(), 3u);
    ASSERT_EQ(z[0].seg.size(), 3u);
    EXPECT_EQ(z[0].seg[0].text, "SERIENDRUCKER");
    EXPECT_EQ(z[0].seg[0].modus, Modus::Normal);
    EXPECT_EQ(z[0].seg[1].text, " robotron 1157 ");
    EXPECT_EQ(z[0].seg[1].modus, Modus::Breit);
    EXPECT_EQ(z[0].seg[2].text, "Autotestprogramm 1 :");
    EXPECT_EQ(z[0].seg[2].modus, Modus::Schraeg);
    ASSERT_EQ(z[1].seg.size(), 1u);
    EXPECT_EQ(z[1].seg[0].text.size(), 63u);            // 21H..5FH
    EXPECT_EQ(z[1].seg[0].text.front(), '!');
    EXPECT_EQ(z[1].seg[0].text.back(), '_');
    EXPECT_EQ(z[2].seg[0].text.size(), 31u);            // 60H..7EH
    EXPECT_EQ(z[2].seg[0].text.front(), '`');
    EXPECT_EQ(z[2].seg[0].text.back(), '~');
}

TEST(Sd1157Selbsttest, DruckbildEntsprichtDemOrakelPunktFuerPunkt)
{
    const Zg zg = Zg::laden();
    const auto zeilen = testZeilen(zg);
    Drucker d(offline());
    d.selbsttest();
    const Blattmodell& b = d.st().druck().blatt(0);

    std::vector<int> ys;
    const auto erw = orakel(d, zg, zeilen, true, &ys);
    ASSERT_EQ(erw.size(), 3u);
    uint64_t gesamt = 0;
    for (size_t i = 0; i < erw.size(); ++i) {
        const std::string m = vergleiche(b, erw[i], ys[i] - 2, ys[i] + 18);
        EXPECT_EQ(m, "") << "Zeile " << i + 1 << " (y0=" << ys[i] << ")\n" << m;
        gesamt += erw[i].size();
    }
    // Reihenfolge und Abstaende: erste Zeile bei 36 + 24, Abstand 24 (6 lpi = 8 Schritte)
    EXPECT_EQ(ys[0], 60);
    EXPECT_EQ(ys[1] - ys[0], 24);
    EXPECT_EQ(ys[2] - ys[1], 24);
    // Kein Punkt ausserhalb der drei Zeilen: die Summe stimmt, nichts links der Druckspalte 0, nichts verworfen
    EXPECT_EQ(b.punktzahl(), gesamt) << "es sind Punkte ausserhalb der erwarteten Zeilen gedruckt";
    for (int y = 0; y < b.hoehe(); ++y)
        for (int x = 0; x < 2 * DRUCKSPALTE_0; ++x) ASSERT_FALSE(b.punkt(x, y)) << "Punkt im linken Rand x=" << x << " y=" << y;
    EXPECT_EQ(b.ausserhalb(), 0u);
    EXPECT_EQ(d.st().druck().kopf().abgewiesen(), 0u) << "Punkte bei nicht angeschwenktem Kopf";
    EXPECT_EQ(d.kopf_weg_beim_druck, 0);
    // Titelzeile reicht bis Takt 80 + 130 + 300 + 200 + Schraegverschiebung; die beiden anderen Zeilen enden bei 80 + 630 bzw. 80 + 310
    int xmax = 0;
    for (int x = 0; x < b.breite(); ++x)
        for (int y = ys[0]; y < ys[0] + 17; ++y) if (b.punkt(x, y)) xmax = x;
    EXPECT_GE(xmax / 2, 80 + 130 + 300 + 190);
    EXPECT_LT(xmax / 2, 80 + 130 + 300 + 200 + 9);
}

TEST(Sd1157Selbsttest, BeideFahrtrichtungenDruckenUndLiefernDieselbenPunktlagen)
{
    // Die Firmware druckt die Zeilen abwechselnd vor- und rueckwaerts (kuerzester Weg).  Das Orakel kennt keine Richtung:
    // jede Zeile entspricht ihm exakt (vorheriger Test); hier nur, dass BEIDE Richtungen vorkamen, der Vergleich also
    // nicht leer von Rueckwaertsdruck war.
    Drucker d(offline());
    d.selbsttest();
    bool vor = false, rueck = false;
    for (const auto& kv : d.richtungen) {
        vor |= kv.second.count(+1) > 0;
        rueck |= kv.second.count(-1) > 0;
        EXPECT_EQ(kv.second.size(), 1u) << "Zeile y=" << kv.first << ": die Richtung wechselt nicht innerhalb einer Zeile";
    }
    EXPECT_TRUE(vor);
    EXPECT_TRUE(rueck);
    EXPECT_EQ(d.richtungen.size(), 3u);
}

TEST(Sd1157Selbsttest, OhneFlugzeitLiegtDieRueckwaertsZeileEinenTaktRechts)
{
    // Befund WP3: die Firmware gleicht eine Flugzeit von 0,5 Takt aus.  Landen die Punkte in BEIDEN Richtungen gleich weit
    // rechts der Ausloeseposition (keine richtungsabhaengige Flugzeit), liegt jede rueckwaerts gedruckte Zeile um 2 Pixel
    // (1 Takt) rechts des Orakels, jede vorwaerts gedruckte stimmt.
    const Zg zg = Zg::laden();
    const auto zeilen = testZeilen(zg);
    PrinterMachine::Config c = offline();
    c.sd1157.druck.versatz_vor = c.sd1157.druck.versatz_rueck = +1;
    Drucker d(c);
    d.selbsttest();
    const Blattmodell& b = d.st().druck().blatt(0);
    std::vector<int> ys;
    const auto erw = orakel(d, zg, zeilen, true, &ys);
    for (size_t i = 0; i < erw.size(); ++i) {
        const int rich = *d.richtungen.at(ys[i]).begin();
        EXPECT_EQ(vergleiche(b, erw[i], ys[i] - 2, ys[i] + 18, rich > 0 ? 0 : 2), "")
            << "Zeile " << i + 1 << " Richtung " << rich;
        if (rich < 0) EXPECT_NE(vergleiche(b, erw[i], ys[i] - 2, ys[i] + 18, 0), "") << "rueckwaerts ohne Versatz darf nicht passen";
    }
}

TEST(Sd1157Selbsttest, FgdiFolgtDemTaktImpulsInBeidenRichtungenGleichSchnell)
{
    // Frage (Merkposten 4/17): kommt der 1-Takt-Versatz Rueckwaerts/Vorwaerts aus der Takt-/FGDI-Abfolge des EMULATORS?  Gemessen wird,
    // wie viele Maschinentakte zwischen der letzten Positionsaenderung (= Takt-Impuls der Mechanik) und dem Anschlag (FGDI 2B.5) liegen
    // und ob dabei eine weitere Position uebersprungen wird.  Ist die Latenz in beiden Richtungen gleich und klein gegen die Takt-
    // Periode (1365 Maschinentakte), liegt der Punkt in beiden Richtungen auf der Position des Impulses: die Mechanik des Emulators ist
    // richtungssymmetrisch, der Versatz stammt aus dem TRIGGER der Firmware (rueckwaerts eine Zaehlstelle spaeter) und wird nur durch die
    // Flugzeit (oder eine gleichwertige Phasenlage des richtungsdekodierten Takts, [?]) ausgeglichen.
    Drucker d(offline());
    std::map<int, std::vector<uint64_t>> latenz;   // Richtung -> Takte seit der letzten Positionsaenderung
    int pos = d.st().druck().wagen().position();
    uint64_t seit = 0;
    d.m->setBusTrace([&](bool io, bool rd, uint16_t a, uint8_t dd) {
        if (io && !rd && (a & 0xFF) == 0xF9 && (dd & 0x20) && d.st().nadeln())
            latenz[d.st().druck().wagen().richtung()].push_back(d.m->totalCycles() - seit);
    });
    d.fahre(5'000'000);
    d.st().setTasteTest(true);
    bool los = false;
    d.m->setCpuTraceCallback([&](const Z80& c) {
        const int p = d.st().druck().wagen().position();
        if (p != pos) { pos = p; seit = d.m->totalCycles(); }
        if (c.PC == HPT3 && !los) { los = true; d.st().setTasteTest(false); }
    });
    d.fahre(40'000'000);
    d.m->setCpuTraceCallback(nullptr);
    d.m->setBusTrace(nullptr);
    ASSERT_TRUE(los);
    ASSERT_GT(latenz[+1].size(), 100u);
    ASSERT_GT(latenz[-1].size(), 100u);
    double mittel[2] = {0, 0};
    uint64_t maxi[2] = {0, 0};
    int i = 0;
    for (int r : {+1, -1}) {
        for (uint64_t v : latenz[r]) { mittel[i] += static_cast<double>(v); maxi[i] = std::max(maxi[i], v); }
        mittel[i] /= static_cast<double>(latenz[r].size());
        ++i;
    }
    std::printf("FGDI-Latenz nach dem Takt-Impuls: vorwaerts mittel %.0f max %llu, rueckwaerts mittel %.0f max %llu (n = %zu / %zu)\n",
                mittel[0], (unsigned long long)maxi[0], mittel[1], (unsigned long long)maxi[1], latenz[+1].size(), latenz[-1].size());
    EXPECT_LT(maxi[0], 683u) << "vorwaerts: FGDI innerhalb der halben Takt-Periode";
    EXPECT_LT(maxi[1], 683u) << "rueckwaerts: FGDI innerhalb der halben Takt-Periode";
    EXPECT_LT(std::abs(mittel[0] - mittel[1]), 100.0) << "die Latenz ist richtungsunabhaengig";
}

TEST(Sd1157Selbsttest, ZeilenabstandFolgtDerTasteSechsAchtLpi)
{
    const Zg zg = Zg::laden();
    const auto zeilen = testZeilen(zg);
    Drucker d(offline(false));   // 8 lpi: 6 Schritte = 18 Rasterzeilen je LF
    d.selbsttest();
    const Blattmodell& b = d.st().druck().blatt(0);
    std::vector<int> ys;
    const auto erw = orakel(d, zg, zeilen, false, &ys);
    EXPECT_EQ(ys[0], 36 + 18);
    EXPECT_EQ(ys[1] - ys[0], 18);
    uint64_t gesamt = 0;
    for (size_t i = 0; i < erw.size(); ++i) {
        const std::string m = vergleiche(b, erw[i], ys[i] - 1, ys[i] + 17);
        EXPECT_EQ(m, "") << "Zeile " << i + 1 << "\n" << m;
        gesamt += erw[i].size();
    }
    EXPECT_EQ(b.punktzahl(), gesamt);
}

TEST(Sd1157Selbsttest, DasZyklusendeFaehrtDenFormularvorschubBisZurFormularlaenge)
{
    // hardware.md §12 Nr. 4 / §14 Nr. 6: am Zyklusende FF1 = Formularlaenge (BCH.5..4: 00 = 4" = 192, 11 = 12" = 576 Schritte);
    // die Anfangs-PV nach dem Neustart ist netto 0.
    for (const auto& fl : {std::pair<int, int>{3, 576}, std::pair<int, int>{0, 192}, std::pair<int, int>{1, 288}}) {
        PrinterMachine::Config c = offline();
        c.sd1157.formular_bahn1 = static_cast<uint8_t>(fl.first);
        Drucker d(c);
        d.selbsttest();
        EXPECT_EQ(d.st().druck().papier(0).lage(), fl.second) << "Formularlaenge-Code " << fl.first;
    }
}

TEST(Sd1157Selbsttest, NurOfflineStartetDerTestUndTasteTestAlleinNichtsDruckt)
{
    // online: TEST gedrueckt, nichts geschieht (hardware.md §5.4: PV/TEST nur offline)
    Drucker d;
    d.fahre(5'000'000);
    d.st().setTasteTest(true);
    d.fahre(15'000'000);
    EXPECT_EQ(d.st().druck().blatt(0).punktzahl(), 0u);
    EXPECT_EQ(d.st().druck().papier(0).lage(), 0);
    EXPECT_EQ(d.st().druck().kopf().anschlaege(), 0u);
}

// ─── Anfangsvorschub ─────────────────────────────────────────────────────────

TEST(Sd1157Papier, AnfangsvorschubIst56SchritteMitNettolageNull)
{
    // Quelle: pafl1 = 24 vor (1445-1450), Rueckzug 24 + 4 zurueck (`add a,4` bei m24dd), dann 4 vor: 24 + 28 + 4 = 56 Schritte.
    // Die frueher beobachteten 56 sind also die Firmware, kein Nachlauf des Platzhalters.
    Drucker d;
    std::vector<int> folge;   // Lage nach jedem Schritt, einmal je Wert gemerkt
    int letzte = 0;
    d.m->setCpuTraceCallback([&](const Z80&) {
        const int l = d.st().druck().papier(0).lage();
        if (l != letzte) { folge.push_back(l); letzte = l; }
    });
    d.fahre(6'000'000);
    const Papierweg& p = d.st().druck().papier(0);
    EXPECT_EQ(p.schritte(), 56u);
    EXPECT_EQ(p.schritteVor(), 28u);
    EXPECT_EQ(p.schritteRueck(), 28u);
    EXPECT_EQ(p.lage(), 0) << "Nettolage";
    ASSERT_EQ(folge.size(), 56u);
    // 0 -> 24 (vor), 24 -> -4 (zurueck), -4 -> 0 (vor)
    EXPECT_EQ(folge[23], 24);
    EXPECT_EQ(folge[24], 23);
    EXPECT_EQ(folge[51], -4);
    EXPECT_EQ(folge[55], 0);
    int lo = 0, hi = 0;
    for (int l : folge) { lo = std::min(lo, l); hi = std::max(hi, l); }
    EXPECT_EQ(hi, 24);
    EXPECT_EQ(lo, -4);
    // und danach nichts mehr
    d.fahre(4'000'000);
    EXPECT_EQ(p.schritte(), 56u);
}

// ─── Tasten und Lampen ───────────────────────────────────────────────────────

TEST(Sd1157Tasten, Pv1SchiebtPapierNurOfflineUndEinenLfJeKurzemDruck)
{
    Drucker d;                      // Taste ONLINE: online
    d.fahre(5'000'000);
    Sd1157Steuerung& st = d.st();
    ASSERT_EQ(st.druck().papier(0).lage(), 0);
    st.setTastePV1(true);
    d.fahre(300'000);               // ~120 ms: kurz
    st.setTastePV1(false);
    d.fahre(3'000'000);
    EXPECT_EQ(st.druck().papier(0).lage(), 0) << "online wirkt PV1 nicht";

    st.setOnline(false);
    st.setTastePV1(true);
    d.fahre(300'000);
    st.setTastePV1(false);
    d.fahre(3'000'000);
    EXPECT_EQ(st.druck().papier(0).lage(), 8) << "offline: ein LF = 8 Schritte (6 lpi)";

    // Die Taste 6/8 lpi liest die Firmware nur beim Anlauf (sy34: BCH.1 -> za61): erst nach einem Reset gilt 8 lpi = 6 Schritte
    st.setLpi6(false);
    st.setTastePV1(true);
    d.fahre(300'000);
    st.setTastePV1(false);
    d.fahre(3'000'000);
    EXPECT_EQ(st.druck().papier(0).lage(), 16) << "zur Laufzeit geaendert: noch 6 lpi";
    d.m->reset();
    d.fahre(10'000'000);            // Grundstellung nach dem Reset (Anfangs-PV netto 0)
    const int vor = st.druck().papier(0).lage();
    st.setTastePV1(true);
    d.fahre(300'000);
    st.setTastePV1(false);
    d.fahre(3'000'000);
    EXPECT_EQ(st.druck().papier(0).lage() - vor, 6) << "nach dem Reset: 8 lpi = 6 Schritte";
}

TEST(Sd1157Tasten, Pv1LangGehaltenFuehrtZumFormularvorschub)
{
    Drucker d(offline());
    d.fahre(5'000'000);
    d.st().setTastePV1(true);
    d.fahre(8'000'000);             // >> 32 x 26,6 ms (hardware.md §6): FF1 = 12" = 576 Schritte
    d.st().setTastePV1(false);
    d.fahre(4'000'000);
    EXPECT_EQ(d.st().druck().papier(0).lage(), 576);
}

TEST(Sd1157Tasten, OnlineLampeFolgtDemEinschaltzustandFehlerLampeDerHavarie)
{
    {
        Drucker d;
        d.fahre(5'000'000);
        EXPECT_TRUE(d.st().lampeOnline());
        EXPECT_FALSE(d.st().lampeFehler());
    }
    {
        Drucker d(offline());
        d.fahre(5'000'000);
        EXPECT_FALSE(d.st().lampeOnline()) << "offline gestartet: Lampe aus";
        EXPECT_FALSE(d.st().lampeFehler());
    }
    {
        // NMI = Farbbandhavarie + Spannungsausfall (hardware.md §8): technischer Fehler, Fehlerlampe an (/TF = 0)
        Drucker d;
        d.fahre(5'000'000);
        d.m->nmi();
        d.fahre(3'000'000);
        EXPECT_TRUE(d.st().lampeFehler());
        EXPECT_EQ(d.st().portAusgabe(2, 1) & 0x08, 0) << "2B.3 /TF = 0";
    }
}

TEST(Sd1157Tasten, KopfTasteHaeltDenKopfDauerndAnImLeerlaufNichtSonst)
{
    Drucker d;
    d.fahre(5'000'000);
    EXPECT_FALSE(d.st().kopfAngeschwenkt()) << "im Leerlauf ist KAS = 0 (hardware.md §10.3)";
    d.st().setKopf(true);
    EXPECT_TRUE(d.st().kopfAngeschwenkt());
    EXPECT_TRUE(d.st().kopf());
    d.st().setKopf(false);
    EXPECT_FALSE(d.st().kopfAngeschwenkt());
}

TEST(Sd1157Tasten, SechsAchtLpiLesenderBchBit1)
{
    Drucker d;
    d.st().setLpi6(true);
    EXPECT_EQ(d.m->ioReadDebug(0xBC) & 0x02, 0x02);
    d.st().setLpi6(false);
    EXPECT_EQ(d.m->ioReadDebug(0xBC) & 0x02, 0x00);
    EXPECT_FALSE(d.st().lpi6());
}

// ─── Papierende ──────────────────────────────────────────────────────────────

TEST(Sd1157Papierende, MeldetImVorschubUndLaeuftDann82SchritteWeiter)
{
    // hardware.md §10.2 Nr. 5: Papierende (BCH.0 = 0) beim Vorschub: (IX+3CH) = 82 Schritte herunterzaehlen, dann `ape`.
    // /PE faellt bei Lage 100: das Statusbit `zmb pe` (IX+0AH Bit 2) steht mit dem ersten Schritt dort, `ape` (IX+7 Bit 5) 81 Schritte spaeter.
    constexpr uint16_t ZMB = 0x2C5A, MB7 = 0x2C57;
    Drucker d(offline());
    d.st().druck().papier(0).setzeLaenge(100);
    d.fahre(5'000'000);
    int lage_pe = -1, lage_ape = -1;
    d.m->setCpuTraceCallback([&](const Z80&) {
        const int l = d.st().druck().papier(0).lage();
        if (lage_pe < 0 && (d.m->memReadDebug(ZMB) & 0x04)) lage_pe = l;
        if (lage_ape < 0 && (d.m->memReadDebug(MB7) & 0x20)) lage_ape = l;
    });
    d.st().setTastePV1(true);
    d.fahre(8'000'000);
    d.st().setTastePV1(false);
    d.fahre(4'000'000);
    EXPECT_EQ(lage_pe, 100);
    EXPECT_EQ(lage_ape, 100 + 81);
    EXPECT_FALSE(d.st().papierDa());
}

TEST(Sd1157Papierende, BeimEinschaltenMeldetDieStatusanzeigeUndLoeschtSichMitDemPapier)
{
    constexpr uint16_t ZMB = 0x2C5A;
    Drucker d;
    d.st().setPapierDa(false);
    d.m->reset();                    // die Firmware liest /PE beim Anlauf (sy38)
    d.fahre(10'000'000);
    EXPECT_NE(d.m->memReadDebug(ZMB) & 0x04, 0) << "Papierende im Zustandsregister";
    d.st().setPapierDa(true);
    d.fahre(3'000'000);
    EXPECT_EQ(d.m->memReadDebug(ZMB) & 0x04, 0) << "Papier eingelegt: Papierende geloescht";
}
