// Tests von tools/z8000/z8kdis.h (Entwurf 41, AP K3): Kartendatei, Ablaufverfolgung, Marken,
// MUTOS-as-Ausgabe und der Rundlauf Disassemblat -> Assembler -> Bytes.
//
// Der Kernel-Abzug selbst liegt in einem Zip; seine Pruefung steht in
// tests/python/test_z8kdis_kernel.py.  Hier: kleine, selbst gebaute Prueflinge und die
// Vollstaendigkeit ueber den ganzen Befehlssatz.
#include "tools/z8000/z8kdis.h"

#include <gtest/gtest.h>

#include <set>

using namespace z8kdis;

namespace {

std::vector<uint8_t> baue(const std::string& quelle, bool seg = false) {
    z8k::AsmOptions o; o.seg = seg;
    z8k::AsmResult r = z8k::assemble(quelle, o);
    EXPECT_TRUE(r.ok()) << (r.errors.empty() ? "" : r.errors[0].text);
    uint32_t base = 0;
    return r.flat(&base);
}

Analyse analysiere(const std::vector<uint8_t>& bin, const std::string& karte, uint32_t base = 0,
                   Format f = Format::Listing, bool seg = false) {
    Analyse an;
    an.opt.format = f;
    an.opt.segmentiert = seg;
    std::string err;
    EXPECT_TRUE(an.karte.lesen(karte, err)) << err;
    an.laden(bin, base);
    an.analysieren();
    return an;
}

const char* kProgramm =
    "          ORG   %0\n"
    "  START:  LD    R0,#%1234\n"
    "          CALL  SUB\n"
    "          JR    NZ,START\n"
    "          RET\n"
    "  SUB:    INC   R0\n"
    "          DJNZ  R1,SUB\n"
    "          RET\n"
    "  TAB:    DW    %0001,%0002\n";

} // namespace

// ── Karte ────────────────────────────────────────────────────────────────────

TEST(Z8kdisKarte, LiestAlleSchluesselwoerter) {
    Karte k; std::string err;
    ASSERT_TRUE(k.lesen("# Kommentar\n"
                        "seg 0 0 100\ncode 10 20 30   # Rest ignoriert\nlin 40 4F\ndb 50 5F\ndw 60 6F\n"
                        "dwc 70 7F\nstr 80 8F\ntxt 90 9F\npsa 0 7\nvek 3C 23B\n"
                        "name 10 start\nkom 10 Anfang des Ganzen\nik 10 hier\neq PORT %1F\n", err)) << err;
    EXPECT_EQ(k.segs.size(), 1u);
    EXPECT_EQ(k.code.size(), 3u);
    EXPECT_EQ(k.lin.size(), 1u);
    EXPECT_EQ(k.daten.size(), 5u);
    EXPECT_EQ(k.psa[0].second, 7u);
    EXPECT_EQ(k.vek[0].second, 0x23Bu);
    EXPECT_EQ(k.name.at(0x10), "start");
    EXPECT_EQ(k.kom.find(0x10)->second, "Anfang des Ganzen");
    EXPECT_EQ(k.eq[0].second, 0x1Fu);
}

TEST(Z8kdisKarte, FehlerMitZeilennummer) {
    Karte k; std::string err;
    EXPECT_FALSE(k.lesen("code 10\nunsinn 1 2\n", err));
    EXPECT_NE(err.find("Zeile 2"), std::string::npos) << err;
    EXPECT_FALSE(k.lesen("db 20 10\n", err));            // bis < von
    EXPECT_FALSE(k.lesen("code zz\n", err));
}

// ── Ablaufverfolgung ─────────────────────────────────────────────────────────

TEST(Z8kdis, FolgtSprungAufrufUndSchleifenzielen) {
    Analyse an = analysiere(baue(kProgramm), "code 0\nname 0 start\nname C sub\n");
    // Code: LD(4) CALL(4) JR(2) RET(2) INC(2) DJNZ(2) RET(2) = 18 Byte; danach 4 Byte Daten
    EXPECT_EQ(an.stat.befehle, 7u);
    EXPECT_EQ(an.stat.code, 18u);
    EXPECT_EQ(an.stat.unerreicht, 4u);
    std::string t = an.ausgeben();
    EXPECT_NE(t.find("start:"), std::string::npos) << t;
    EXPECT_NE(t.find("sub:"), std::string::npos) << t;
    EXPECT_NE(t.find("CALL sub"), std::string::npos) << t;
    EXPECT_NE(t.find("JR NZ,start"), std::string::npos) << t;
    EXPECT_NE(t.find("DJNZ R1,sub"), std::string::npos) << t;
    EXPECT_NE(t.find("DW %0001,%0002"), std::string::npos) << t;    // Unerreichtes bleibt Wort
}

TEST(Z8kdis, UnbedingterSprungEndetDenAblauf) {
    // JP weg; dahinter steht Daten-Muell, das nie als Code gelesen werden darf
    Analyse an = analysiere(baue("  ORG 0\n  JP L1\n  DW %FFFF\n  L1: RET\n"), "code 0\n");
    EXPECT_EQ(an.stat.befehle, 2u);
    EXPECT_EQ(an.stat.unerreicht, 2u);
}

TEST(Z8kdis, BedingterSprungLaeuftWeiter) {
    Analyse an = analysiere(baue("  ORG 0\n  JP NZ,L1\n  NOP\n  L1: RET\n"), "code 0\n");
    EXPECT_EQ(an.stat.befehle, 3u);
}

TEST(Z8kdis, ZeigertabelleDwcMachtZielZuCode) {
    // Tabelle mit zwei Funktionszeigern; die Funktionen sind nur darueber erreichbar
    const char* q = "  ORG 0\n  RET\n  F1: INC R0\n  RET\n  F2: INC R1\n  RET\n  T: DW F1,F2\n";
    Analyse an2 = analysiere(baue(q), "code 0\ndwc A D\n");
    EXPECT_EQ(an2.stat.befehle, 5u);                                    // RET, INC, RET, INC, RET
    EXPECT_EQ(an2.stat.unerreicht, 0u);
    std::string t = an2.ausgeben();
    EXPECT_NE(t.find("DW L0002,L0006"), std::string::npos) << t;
}

TEST(Z8kdis, DwOhneCErzeugtKeineMarkeAusKleinenZahlen) {
    // `dw` setzt Namen nur, wo es die Marke ohnehin gibt -- 0000/4000 duerfen keine Marken werden
    Analyse an = analysiere(baue("  ORG 0\n  RET\n  DW %0001,%4000\n"), "code 0\ndw 2 5\n");
    std::string t = an.ausgeben();
    EXPECT_NE(t.find("DW %0001,%4000"), std::string::npos) << t;
    EXPECT_EQ(t.find("L4000"), std::string::npos);
    EXPECT_EQ(t.find("L0001"), std::string::npos);
}

TEST(Z8kdis, PsaUndVektortabelleLiefernEinstiege) {
    // 7 PSA-Eintraege + VI-Kopf + 2 Vektoren; Handler als Code (je ein RET)
    std::string q = "  ORG 0\n";
    for (int i = 0; i < 7; ++i) q += "  DW 0,%4000,0,H0\n";
    q += "  DW 0,%4000\n";                                // 38: VI reserviert + FCW
    q += "  DW %0060,H1,%0061,H2\n";                     // 3C: zwei Vektoren
    q += "  H0: RET\n  H1: NOP\n  RET\n  H2: INC R0\n  RET\n";
    Analyse an = analysiere(baue(q), "psa 0 7\ndw 38 3B\nvek 3C 43\n");
    EXPECT_EQ(an.stat.befehle, 5u);                     // RET, NOP, RET, INC, RET
    EXPECT_EQ(an.stat.unerreicht, 0u);
    EXPECT_EQ(an.stat.daten, 0x44u);
}

TEST(Z8kdis, KommentareUndNamenInDerAusgabe) {
    Analyse an = analysiere(baue(kProgramm), "code 0\nname 0 anfang\nkom 0 Vorspann\nik 4 ruft sub\n");
    std::string t = an.ausgeben();
    EXPECT_NE(t.find("; Vorspann\nanfang:"), std::string::npos) << t;
    EXPECT_NE(t.find("CALL L000C  ; ruft sub"), std::string::npos) << t;
}

TEST(Z8kdis, StrukturversatzBekommtKeineMarke) {
    // `LD R1,4(R14)` ist ein Strukturversatz (< 0100), `LD R1,%0200(R2)` eine Tabelle
    Analyse an = analysiere(baue("  ORG 0\n  LD R1,%0004(R14)\n  LD R1,%0200(R2)\n  RET\n  ORG %0200\n  DW 1\n"), "code 0\n");
    std::string t = an.ausgeben();
    EXPECT_NE(t.find("LD R1,%0004(R14)"), std::string::npos) << t;
    EXPECT_NE(t.find("LD R1,L0200(R2)"), std::string::npos) << t;
    EXPECT_EQ(t.find("L0004"), std::string::npos) << t;
}

// ── Rundlauf ─────────────────────────────────────────────────────────────────

TEST(Z8kdis, RundlaufKleinesProgramm) {
    Analyse an = analysiere(baue(kProgramm), "code 0\n");
    std::string bericht;
    EXPECT_EQ(an.pruefen(bericht), 0) << bericht;
}

TEST(Z8kdis, RundlaufMitDatenUndZeichenketten) {
    std::string q = "  ORG 0\n  RET\n  DB 3,'a','b','c'\n  DB 'xyz',0\n  DW %1234,%5678\n  DB %FF\n";
    Analyse an = analysiere(baue(q), "code 0\nstr 2 5\ntxt 6 9\n");
    std::string bericht;
    EXPECT_EQ(an.pruefen(bericht), 0) << bericht;
}

TEST(Z8kdis, NichtKanonischeKodierungWirdDatenUndBleibtBitgleich) {
    // LD R0,@RR1 -- ungerades Registerpaar: der Disassembler zeigt RR1, der Assembler lehnt es ab.
    // Dieselben Bytes muessen trotzdem herauskommen (als Wort), nur eben nicht als Code.
    std::vector<uint8_t> bin = {0x21, 0x10, 0x9E, 0x08};      // LD R0,@R1 (kanonisch, nonseg) + RET
    Analyse an = analysiere(bin, "code 0\n");
    EXPECT_EQ(an.stat.nicht_kanonisch, 0u);
    // segmentiert: 21 10 = LD R0,@RR1 (ungerade Paarnummer)
    Analyse as = analysiere(bin, "code 0\n", 0, Format::Listing, true);
    EXPECT_EQ(as.stat.nicht_kanonisch, 1u);
    EXPECT_NE(as.ausgeben().find("nicht kanonisch kodiert: LD R0,@RR1"), std::string::npos);
    std::string bericht;
    EXPECT_EQ(as.pruefen(bericht), 0) << bericht;
}

// ── Vollstaendigkeit ueber den ganzen Befehlssatz ────────────────────────────

class Z8kdisVoll : public ::testing::TestWithParam<bool> {};

TEST_P(Z8kdisVoll, JedesErsteWortRundlaeuftBitgleich) {
    const bool seg = GetParam();
    uint32_t rnd = 0xC0FFEEu;
    auto next = [&] { rnd = rnd * 1103515245u + 12345u; return uint16_t(rnd >> 8); };
    std::set<int> erwartet, gesehen;
    int befehle = 0;
    for (int batch = 0; batch < 16; ++batch) {
        // 4096 Plaetze zu 16 Byte = 64 KB; Befehl vorn, RET-Fuellung dahinter (stoppt den Ablauf)
        std::vector<uint8_t> bin(0x10000, 0);
        Karte karte;
        karte.segs.push_back({seg ? 0x30000u : 0u, 0, 0x10000});
        for (int slot = 0; slot < 4096; ++slot) {
            uint16_t w[8];
            w[0] = uint16_t(batch * 4096 + slot);
            for (int i = 1; i < 8; ++i) w[i] = next();
            if (slot & 1) w[1] = 0;
            z8k::Decoded d;
            int len = 8;
            if (z8k::decodeWords(w, 8, seg, d)) {
                len = d.nwords;
                erwartet.insert(d.insn->index);
                ++befehle;
                karte.code.push_back((seg ? 0x30000u : 0u) + uint32_t(slot) * 16);
            }
            for (int i = len; i < 8; ++i) w[i] = 0x9E08;               // RET
            for (int i = 0; i < 8; ++i) {
                bin[size_t(slot) * 16 + size_t(i) * 2] = uint8_t(w[i] >> 8);
                bin[size_t(slot) * 16 + size_t(i) * 2 + 1] = uint8_t(w[i]);
            }
        }
        Analyse an;
        an.opt.segmentiert = seg;
        an.karte = karte;
        an.laden(bin, seg ? 0x30000u : 0u);
        an.analysieren();
        for (int i : an.gesehen()) gesehen.insert(i);
        std::string bericht;
        ASSERT_EQ(an.pruefen(bericht), 0) << "Batch " << batch << (seg ? " seg" : " nonseg") << "\n" << bericht;
    }
    // Jede Tabellenzeile, die ein Zufallswort trifft, wurde auch von z8kdis als Code erkannt.
    EXPECT_EQ(gesehen, erwartet);
    EXPECT_GT(befehle, 50000);
    // Die Zufallsstichprobe trifft nicht jede der 438 Zeilen (Zeilen mit eng begrenzten Feldern, ca. 87 %
    // werden getroffen); die Vollstaendigkeit pro Zeile sichert tests/debugtools/test_z8k_asm.cpp.
    EXPECT_GT(int(erwartet.size()), int(z8k::kRowCount) * 8 / 10)
        << erwartet.size() << " von " << z8k::kRowCount << " Zeilen getroffen";
    std::printf("z8kdis %s: %d Befehle, %zu von %zu Tabellenzeilen\n", seg ? "segmentiert" : "nichtsegmentiert",
                befehle, erwartet.size(), size_t(z8k::kRowCount));
}

INSTANTIATE_TEST_SUITE_P(Modi, Z8kdisVoll, ::testing::Values(false, true),
                         [](const auto& p) { return p.param ? "segmentiert" : "nichtsegmentiert"; });

// ── MUTOS-as ─────────────────────────────────────────────────────────────────

TEST(Z8kdisAs, DialektNachCcS) {
    const char* q =
        "  ORG 0\n"
        "  S:  LD R0,#%0000\n"
        "      LDCTL PSAP,R0\n"
        "      CALL SUB\n"
        "      JP NZ,S\n"
        "      JP Z,S\n"
        "      PUSH @R15,#%0001\n"
        "      RET\n"
        "  SUB: INC R0,#2\n"
        "      RET\n"
        "  TAB: DW %0001,%0203\n"
        "      DB %41,%42\n";
    Analyse an = analysiere(baue(q), "code 0\nname 0 start\nname 18 sub\ndw 1C 1F\ndb 20 21\n", 0, Format::As);
    std::string t = an.ausgeben();
    // Rahmen wie `cc -S`
    EXPECT_EQ(t.rfind("mutos module\n$NONSEGMENTED\nconstant\n", 0), 0u) << t;
    EXPECT_NE(t.find("\tBV\t:= bval\n"), std::string::npos);
    EXPECT_NE(t.find("G\t_mutos\tPR\nE\n"), std::string::npos);
    EXPECT_NE(t.find("end mutos\n"), t.npos);
    // Befehle: klein, Tabulator, Marken unveraendert, nichtsegmentiertes Steuerregister PSAPOFF
    EXPECT_NE(t.find("\tcall\tsub\n"), std::string::npos) << t;
    EXPECT_NE(t.find("\tldctl\tpsapoff,r0\n"), std::string::npos) << t;
    EXPECT_NE(t.find("\tjp\tne,start\n"), std::string::npos) << t;      // NZ -> ne
    EXPECT_NE(t.find("\tjp\teq,start\n"), std::string::npos) << t;      // Z  -> eq
    EXPECT_NE(t.find("\tpush\t@r15,#%0001\n"), std::string::npos) << t;
    EXPECT_NE(t.find("\tinc\tr0,#2\n"), std::string::npos) << t;
    // Daten: je Wert eine Anweisung, durch ; verbunden
    EXPECT_NE(t.find("\tWV %0001;WV %0203\n"), std::string::npos) << t;
    EXPECT_NE(t.find("\tBV %41;BV %42\n"), std::string::npos) << t;
    // Hexzahlen behalten ihre Grossschreibung
    EXPECT_EQ(t.find("%ed"), std::string::npos);
    // Keine Grossbuchstaben-Mnemonik
    EXPECT_EQ(t.find("\tCALL"), std::string::npos);
}
