/**
 * @file test_kermit_paket.cpp
 * @brief Paketschicht der Kermit-Bibliothek (doc/design/36_kermit.md, AP K2):
 *        Blockpruefung gegen bekannte Werte und gegen die Rechenwege von
 *        C-Kermit 4.2, Rahmen/Zerleger (normal, Langpaket, Neusynchronisation),
 *        Init-Parameter, Aushandlung, Praefix-Kodierung, Attribute.
 */
#include <gtest/gtest.h>

#include <random>
#include <string>
#include <vector>

#include "kermit/src/paket.h"

using namespace k1520::kermit;

namespace {

const uint8_t* B(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }

// ── Nachbildungen aus ckfns2.c (C-Kermit 4.2), unsigned char, ohne Paritaet ──
unsigned ck_chk2(const std::string& s) {
    unsigned c = 0;
    for (unsigned char ch : s) c += ch;
    return c;
}
unsigned ck_chk1(const std::string& s) {
    const unsigned c = ck_chk2(s);
    return (((c & 0300) >> 6) + c) & 077;
}
unsigned ck_chk3(const std::string& s) {   // Lowry, tabellenlos, 010201
    unsigned long crc = 0;
    for (unsigned char c : s) {
        unsigned q = (crc ^ c) & 017;
        crc = (crc >> 4) ^ (q * 010201);
        q = (crc ^ (c >> 4)) & 017;
        crc = (crc >> 4) ^ (q * 010201);
    }
    return static_cast<unsigned>(crc & 0xFFFF);
}
// spack() aus ckfns2.c (ohne Fuellung/Paritaet)
std::string ck_spack(char typ, int num, const std::string& dat, int bctu) {
    std::string p;
    p += '\x01';
    p += static_cast<char>(dat.size() + bctu + 2 + 32);
    p += static_cast<char>(num + 32);
    p += typ;
    p += dat;
    const std::string kopf = p.substr(1);
    switch (bctu) {
    case 1: p += static_cast<char>(ck_chk1(kopf) + 32); break;
    case 2: {
        const unsigned j = ck_chk2(kopf);
        p += static_cast<char>(((j & 07700) >> 6) + 32);
        p += static_cast<char>((j & 077) + 32);
        break;
    }
    case 3: {
        const unsigned j = ck_chk3(kopf);
        p += static_cast<char>(((j & 0170000) >> 12) + 32);
        p += static_cast<char>(((j & 07700) >> 6) + 32);
        p += static_cast<char>((j & 077) + 32);
        break;
    }
    }
    p += '\r';
    return p;
}

std::vector<Paket> zerlege(const std::string& strom, int chkt, int* fehlerhaft = nullptr) {
    PaketZerleger z;
    z.setzeChkt(chkt);
    std::vector<Paket> v;
    for (unsigned char c : strom) {
        const auto r = z.zufuehren(c);
        if (r == PaketZerleger::Ergebnis::Paket) v.push_back(z.paket());
        if (r == PaketZerleger::Ergebnis::Fehlerhaft && fehlerhaft) ++*fehlerhaft;
    }
    return v;
}

std::vector<uint8_t> zufall(std::size_t n, unsigned saat) {
    std::mt19937 g(saat);
    std::vector<uint8_t> v(n);
    for (auto& b : v) b = static_cast<uint8_t>(g());
    return v;
}

}  // namespace

// ─── Blockpruefung ─────────────────────────────────────────────────────────

TEST(KermitPruefung, Crc16KermitPruefwert) {
    // CRC-16/KERMIT (Katalog-Pruefwert); CCITT-FALSE waere 0x29B1.
    EXPECT_EQ(crc16Kermit(B("123456789"), 9), 0x2189);
    EXPECT_EQ(crc16Kermit(B(""), 0), 0x0000);
    EXPECT_EQ(crc16Kermit(B("A"), 1), 0x538D);
}

TEST(KermitPruefung, KlassischesNak) {
    // "^A# N3" — das klassische NAK fuer Paket 0 mit Pruefsumme Typ 1
    EXPECT_EQ(baueRahmen(Paket{'N', 0, ""}, 1, {}), std::string("\x01# N3\r"));
}

TEST(KermitPruefung, GleichWieCKermit42) {
    std::mt19937 g(7);
    for (int i = 0; i < 300; ++i) {
        std::string s(static_cast<std::size_t>(g() % 120), ' ');
        for (auto& c : s) c = static_cast<char>(32 + g() % 95);
        EXPECT_EQ(static_cast<unsigned>(pruefsumme1(B(s), s.size())), ck_chk1(s));
        EXPECT_EQ(static_cast<unsigned>(pruefsumme2(B(s), s.size())), ck_chk2(s) & 0xFFF);
        EXPECT_EQ(crc16Kermit(B(s), s.size()), ck_chk3(s));
    }
}

TEST(KermitPruefung, RahmenGleichCKermitSpack) {
    std::mt19937 g(11);
    for (int chkt = 1; chkt <= 3; ++chkt) {
        for (int i = 0; i < 50; ++i) {
            std::string d(static_cast<std::size_t>(g() % (91 - chkt)), ' ');
            for (auto& c : d) c = static_cast<char>(33 + g() % 94);
            const int seq = static_cast<int>(g() % 64);
            EXPECT_EQ(baueRahmen(Paket{'D', seq, d}, chkt, {}), ck_spack('D', seq, d, chkt));
        }
    }
    // S immer mit Typ 1, auch wenn eine andere Pruefung laeuft
    EXPECT_EQ(baueRahmen(Paket{'S', 0, "z* @-#Y1~"}, 3, {}), ck_spack('S', 0, "z* @-#Y1~", 1));
}

TEST(KermitPruefung, ZeichenKodierungDerPruefung) {
    const std::string s = "#! D";
    const auto c3 = blockpruefung(3, B(s), s.size());
    ASSERT_EQ(c3.size(), 3u);
    const int crc = crc16Kermit(B(s), s.size());
    EXPECT_EQ(unchar(c3[0]), (crc >> 12) & 0xF);
    EXPECT_EQ(unchar(c3[1]), (crc >> 6) & 0x3F);
    EXPECT_EQ(unchar(c3[2]), crc & 0x3F);
    EXPECT_EQ(blockpruefung(2, B(s), s.size()).size(), 2u);
}

// ─── Rahmen und Zerleger ───────────────────────────────────────────────────

TEST(KermitRahmen, RundUeberAlleLaengenUndPruefungen) {
    for (int chkt = 1; chkt <= 3; ++chkt) {
        for (int n : {0, 1, 2, 50, 88, 89, 90, 91, 92, 95, 200, 500, 997, 1000, 3000}) {
            std::string d(static_cast<std::size_t>(n), 'x');
            for (std::size_t i = 0; i < d.size(); ++i) d[i] = static_cast<char>(33 + (i * 7) % 94);
            const std::string r = baueRahmen(Paket{'D', 42, d}, chkt, {});
            const bool lang = n + 2 + chkt > MAX_NORMAL;
            EXPECT_EQ(r[1] == ' ', lang) << n;
            const auto v = zerlege(r, chkt);
            ASSERT_EQ(v.size(), 1u) << "chkt " << chkt << " n " << n;
            EXPECT_EQ(v[0].typ, 'D');
            EXPECT_EQ(v[0].seq, 42);
            EXPECT_EQ(v[0].daten, d);
        }
    }
}

TEST(KermitRahmen, FuellzeichenUndEol) {
    RahmenParameter rp;
    rp.npad = 3;
    rp.padc = 0x7F;
    rp.eol = '\n';
    const std::string r = baueRahmen(Paket{'Y', 1, ""}, 1, rp);
    EXPECT_EQ(r.substr(0, 3), std::string(3, '\x7F'));
    EXPECT_EQ(r.back(), '\n');
    EXPECT_EQ(zerlege(r, 1).size(), 1u);
}

TEST(KermitRahmen, VerfaelschtesByteWirdErkannt) {
    const std::string r = baueRahmen(Paket{'D', 5, "Hallo Welt"}, 3, {});
    for (std::size_t i = 1; i + 1 < r.size(); ++i) {
        std::string f = r;
        f[i] = static_cast<char>(f[i] == 'A' ? 'B' : 'A');
        int fehl = 0;
        const auto v = zerlege(f, 3, &fehl);
        // Entweder als beschaedigt erkannt oder (Laengenbyte) unvollstaendig — nie ein falsches Paket
        for (const auto& p : v) EXPECT_FALSE(p.daten == "Hallo Welt" && p.seq == 5 && f != r) << i;
    }
}

TEST(KermitRahmen, LangpaketKopfpruefung) {
    std::string r = baueRahmen(Paket{'D', 3, std::string(500, 'a')}, 3, {});
    ASSERT_EQ(r[1], ' ');
    r[6] = static_cast<char>(r[6] + 1);   // HCHECK verfaelscht
    int fehl = 0;
    EXPECT_TRUE(zerlege(r, 3, &fehl).empty());
    EXPECT_EQ(fehl, 1);
}

TEST(KermitRahmen, NeusynchronisationUndMuell) {
    const std::string gut = baueRahmen(Paket{'D', 7, "abc"}, 1, {});
    const std::string halb = gut.substr(0, 4);
    const std::string strom = std::string("Muell\r\n") + halb + gut + "\r" + baueRahmen(Paket{'Z', 8, ""}, 1, {});
    const auto v = zerlege(strom, 1);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0].daten, "abc");
    EXPECT_EQ(v[1].typ, 'Z');
}

TEST(KermitRahmen, AbgeschnittenesPaketSofortBeschaedigt) {
    // EOL mitten im Datenteil: kein Warten auf die Frist
    std::string r = baueRahmen(Paket{'D', 1, "abcdefgh"}, 1, {});
    r = r.substr(0, 7) + "\r";
    int fehl = 0;
    EXPECT_TRUE(zerlege(r, 1, &fehl).empty());
    EXPECT_EQ(fehl, 1);
}

TEST(KermitRahmen, NakMitAndererPruefungslaenge) {
    // Der Zerleger erwartet Typ 1, das NAK kommt mit Typ 3 (und umgekehrt)
    EXPECT_EQ(zerlege(baueRahmen(Paket{'N', 9, ""}, 3, {}), 1).size(), 1u);
    EXPECT_EQ(zerlege(baueRahmen(Paket{'N', 9, ""}, 1, {}), 3).size(), 1u);
    // S wird immer mit Typ 1 gelesen
    EXPECT_EQ(zerlege(baueRahmen(Paket{'S', 0, "abc"}, 1, {}), 3).size(), 1u);
}

TEST(KermitRahmen, UngueltigeKoepfe) {
    int fehl = 0;
    EXPECT_TRUE(zerlege(std::string("\x01!") + " D", 3, &fehl).empty());   // LEN zu klein
    EXPECT_GE(fehl, 1);
    fehl = 0;
    EXPECT_TRUE(zerlege(std::string("\x01#") + "\x7F" + "D", 1, &fehl).empty());   // SEQ > 63
    EXPECT_EQ(fehl, 1);
    fehl = 0;
    EXPECT_TRUE(zerlege(std::string("\x01#\r"), 1, &fehl).empty());   // Steuerzeichen als SEQ
    EXPECT_EQ(fehl, 1);
    fehl = 0;
    EXPECT_TRUE(zerlege(std::string("\x01 !D\r"), 1, &fehl).empty());   // Langpaketkopf abgeschnitten
    EXPECT_EQ(fehl, 1);
}

// ─── Init-Parameter ────────────────────────────────────────────────────────

TEST(KermitInit, CKermit42SendInit) {
    // rpar() von C-Kermit 4.2: spsiz 90, URTIME 10, kein Fuellen, EOL CR, '#', 'Y', bctr 1, '~'
    const InitParameter ip = zerlegeInit("z* @-#Y1~");
    EXPECT_EQ(ip.felder, 9);
    EXPECT_EQ(ip.maxl, 90);
    EXPECT_EQ(ip.time, 10);
    EXPECT_EQ(ip.npad, 0);
    EXPECT_EQ(ip.eol, '\r');
    EXPECT_EQ(ip.qctl, '#');
    EXPECT_EQ(ip.qbin, 'Y');
    EXPECT_EQ(ip.chkt, 1);
    EXPECT_EQ(ip.rept, '~');
    EXPECT_EQ(ip.capas, 0);
    EXPECT_EQ(ip.maxlx, 0);
}

TEST(KermitInit, RundMitUndOhneCapas) {
    InitParameter a;
    a.maxl = 94;
    a.time = 7;
    a.npad = 2;
    a.padc = 0;
    a.eol = '\n';
    a.qctl = '#';
    a.qbin = '&';
    a.chkt = 3;
    a.rept = '~';
    a.capas = CAPAS_LANGPAKETE | CAPAS_ATTRIBUTE;
    a.windo = 1;
    a.maxlx = 1000;
    const std::string d = initDaten(a, true);
    EXPECT_EQ(d.size(), 13u);
    const InitParameter b = zerlegeInit(d);
    EXPECT_EQ(b.maxl, 94);
    EXPECT_EQ(b.time, 7);
    EXPECT_EQ(b.npad, 2);
    EXPECT_EQ(b.padc, 0);
    EXPECT_EQ(b.eol, '\n');
    EXPECT_EQ(b.qbin, '&');
    EXPECT_EQ(b.chkt, 3);
    EXPECT_EQ(b.capas, CAPAS_LANGPAKETE | CAPAS_ATTRIBUTE);
    EXPECT_EQ(b.windo, 1);
    EXPECT_EQ(b.maxlx, 1000);
    EXPECT_EQ(initDaten(a, false).size(), 9u);
}

TEST(KermitInit, VorgabenBeiFehlendenUndLeerenFeldern) {
    const InitParameter leer = zerlegeInit("");
    EXPECT_EQ(leer.maxl, 80);
    EXPECT_EQ(leer.eol, '\r');
    EXPECT_EQ(leer.qbin, 'N');
    EXPECT_EQ(leer.chkt, 1);
    EXPECT_EQ(leer.rept, ' ');
    const InitParameter sp = zerlegeInit("         ");
    EXPECT_EQ(sp.maxl, 80);
    EXPECT_EQ(sp.qctl, '#');
    EXPECT_EQ(sp.qbin, 'N');
    EXPECT_EQ(sp.chkt, 1);
    // unsinnige Werte
    const InitParameter u = zerlegeInit(std::string("\"* @\x7F#Y9~"));
    EXPECT_EQ(u.maxl, 80);    // < 10
    EXPECT_EQ(u.eol, '\r');   // ausserhalb 2..31
    EXPECT_EQ(u.chkt, 1);
    // CAPAS mit Fortsetzungsbyte: WINDO/MAXLX stehen dahinter
    std::string c = "~* @-#Y3~";
    c += tochar(CAPAS_LANGPAKETE | 1);
    c += tochar(0);
    c += tochar(1);
    c += tochar(10);
    c += tochar(50);
    const InitParameter k = zerlegeInit(c);
    EXPECT_EQ(k.capas & CAPAS_LANGPAKETE, CAPAS_LANGPAKETE);
    EXPECT_EQ(k.windo, 1);
    EXPECT_EQ(k.maxlx, 10 * 95 + 50);
}

TEST(KermitInit, AntwortSpiegeltDiePruefung) {
    InitParameter mein;
    mein.chkt = 3;
    InitParameter dein;
    dein.chkt = 2;
    EXPECT_EQ(antwortParameter(mein, dein).chkt, 2);
    dein.chkt = 1;
    EXPECT_EQ(antwortParameter(mein, dein).chkt, 1);
}

// ─── Aushandlung ───────────────────────────────────────────────────────────

struct QbinFall {
    char mein, dein, erwartet;
};
class KermitQbin : public ::testing::TestWithParam<QbinFall> {};

TEST_P(KermitQbin, Tabelle) {
    InitParameter m, d;
    m.qbin = GetParam().mein;
    d.qbin = GetParam().dein;
    EXPECT_EQ(verhandle(m, d, 1000).kod.qbin, GetParam().erwartet);
    // symmetrisch
    EXPECT_EQ(verhandle(d, m, 1000).kod.qbin, GetParam().erwartet);
}

INSTANTIATE_TEST_SUITE_P(Alle, KermitQbin,
                         ::testing::Values(QbinFall{'Y', 'Y', 0}, QbinFall{'Y', 'N', 0}, QbinFall{'N', 'N', 0},
                                           QbinFall{'Y', '&', '&'}, QbinFall{'&', '&', '&'}, QbinFall{'&', 'N', 0},
                                           QbinFall{'&', '%', 0}, QbinFall{'Y', '#', 0},   // kollidiert mit QCTL
                                           QbinFall{'Y', 'A', 0}));                        // kein gueltiges Praefix

TEST(KermitAushandlung, PruefungWiederholungLangpakete) {
    InitParameter m, d;
    m.chkt = 3;
    d.chkt = 3;
    m.rept = d.rept = '~';
    m.capas = d.capas = CAPAS_LANGPAKETE | CAPAS_ATTRIBUTE;
    m.maxlx = 1000;
    d.maxlx = 2000;
    d.maxl = 90;
    Vereinbarung v = verhandle(m, d, 1000);
    EXPECT_EQ(v.chkt, 3);
    EXPECT_EQ(v.kod.rept, '~');
    EXPECT_TRUE(v.langpakete);
    EXPECT_TRUE(v.attribute);
    EXPECT_EQ(v.maxlx, 1000);   // eigene Grenze
    EXPECT_EQ(v.datenBudget(), 1000 - 3 - 7);

    d.chkt = 2;   // uneinig → 1
    EXPECT_EQ(verhandle(m, d, 1000).chkt, 1);
    d.rept = ' ';
    EXPECT_EQ(verhandle(m, d, 1000).kod.rept, 0);
    d.rept = '%';   // verschiedene Zeichen → keine Wiederholung
    EXPECT_EQ(verhandle(m, d, 1000).kod.rept, 0);
    d.capas = 0;   // Gegenstelle wie C-Kermit 4.2
    v = verhandle(m, d, 1000);
    EXPECT_FALSE(v.langpakete);
    EXPECT_FALSE(v.attribute);
    EXPECT_EQ(v.datenBudget(), 90 - 2 - 1);
    d.capas = CAPAS_LANGPAKETE;
    d.maxlx = 0;   // MAXLX fehlt → 500
    EXPECT_EQ(verhandle(m, d, 1000).maxlx, 500);
}

TEST(KermitAushandlung, EolUndFuellungDerGegenstelle) {
    InitParameter m, d;
    d.eol = '\n';
    d.npad = 4;
    d.padc = 0x7F;
    d.time = 13;
    const Vereinbarung v = verhandle(m, d, 94);
    EXPECT_EQ(v.rahmen.eol, '\n');
    EXPECT_EQ(v.rahmen.npad, 4);
    EXPECT_EQ(v.rahmen.padc, 0x7F);
    EXPECT_EQ(v.time, 13);
    EXPECT_FALSE(v.langpakete);
}

// ─── Kodierung ─────────────────────────────────────────────────────────────

TEST(KermitKodierung, Einzelfaelle) {
    Kodierung k;
    EXPECT_EQ(kodiere(k, "\x01"), "#A");
    EXPECT_EQ(kodiere(k, "\x7F"), "#?");
    EXPECT_EQ(kodiere(k, "#"), "##");
    EXPECT_EQ(kodiere(k, "\r\n"), "#M#J");
    EXPECT_EQ(kodiere(k, "&~"), "&~");   // ohne QBIN/REPT keine Sonderzeichen
    EXPECT_EQ(kodiere(k, std::string("\xFF")), std::string("#\xBF"));   // 8 Bit roh
    EXPECT_EQ(kodiere(k, std::string("\x81")), std::string("#\xC1"));
    k.qbin = '&';
    EXPECT_EQ(kodiere(k, std::string("\x81")), "&#A");
    EXPECT_EQ(kodiere(k, std::string("\xC1")), "&A");
    EXPECT_EQ(kodiere(k, "&"), "#&");
    EXPECT_EQ(kodiere(k, std::string("\xA3")), "&##");
    k.rept = '~';
    EXPECT_EQ(kodiere(k, "aaaa"), "~$a");
    EXPECT_EQ(kodiere(k, "aa"), "aa");
    EXPECT_EQ(kodiere(k, "aaa"), "~#a");
    EXPECT_EQ(kodiere(k, "~"), "#~");
    EXPECT_EQ(kodiere(k, std::string(100, 'z')), "~~z~&z");   // 94 + 6
    EXPECT_EQ(kodiere(k, std::string(5, '\0')), "~%#@");
}

TEST(KermitKodierung, DekodiereFehlerhaft) {
    Kodierung k;
    k.qbin = '&';
    k.rept = '~';
    std::vector<uint8_t> v;
    EXPECT_FALSE(dekodiere(k, "#", v));
    EXPECT_FALSE(dekodiere(k, "&", v));
    EXPECT_FALSE(dekodiere(k, "~", v));
    EXPECT_FALSE(dekodiere(k, "~$", v));
    EXPECT_EQ(dekodiereText(k, "ab#"), "ab#");   // Rohtext bei Formfehler
    v.clear();
    EXPECT_TRUE(dekodiere(k, "~ a", v));   // Zaehler 0 gilt als 1 (C-Kermit)
    EXPECT_EQ(v.size(), 1u);
}

class KermitKodierungRund : public ::testing::TestWithParam<std::tuple<char, char, int>> {};

TEST_P(KermitKodierungRund, ZufallUndLaeufe) {
    Kodierung k;
    k.qbin = std::get<0>(GetParam());
    k.rept = std::get<1>(GetParam());
    const std::size_t budget = static_cast<std::size_t>(std::get<2>(GetParam()));
    auto daten = zufall(20000, 3);
    // Laeufe einstreuen (Wiederholung, auch ueber Blockgrenzen)
    for (std::size_t i = 0; i < 3000; ++i) daten[5000 + i] = 0x23;
    for (std::size_t i = 0; i < 200; ++i) daten[9000 + i] = 0xFE;
    std::vector<uint8_t> zurueck;
    std::size_t pos = 0;
    while (pos < daten.size()) {
        std::string feld;
        const std::size_t k1 = kodiereBlock(k, daten.data() + pos, daten.size() - pos, budget, feld);
        ASSERT_GT(k1, 0u);
        ASSERT_LE(feld.size(), budget);
        for (unsigned char c : feld) {
            ASSERT_GE(c, 32u) << "Steuerzeichen im Datenfeld";
            ASSERT_NE(c, 0x7Fu);
            if (k.qbin) ASSERT_LT(c, 0x80u) << "8-Bit-Zeichen trotz QBIN";
        }
        ASSERT_TRUE(dekodiere(k, feld, zurueck));
        pos += k1;
    }
    EXPECT_EQ(zurueck, daten);
}

INSTANTIATE_TEST_SUITE_P(Varianten, KermitKodierungRund,
                         ::testing::Combine(::testing::Values('\0', '&'), ::testing::Values('\0', '~'),
                                            ::testing::Values(5, 87, 990)));

// ─── Attribute ─────────────────────────────────────────────────────────────

TEST(KermitAttribute, Rund) {
    Attribute a;
    a.laenge = 102400;
    a.laengeK = 100;
    a.datum = "20261018 02:15:00";
    a.typ = 'B';
    const std::string d = attributDaten(a);
    EXPECT_NE(d.find("\"\"B8"), std::string::npos);
    const Attribute b = zerlegeAttribute(d);
    EXPECT_EQ(b.laenge, 102400);
    EXPECT_EQ(b.laengeK, 100);
    EXPECT_EQ(b.datum, a.datum);
    EXPECT_EQ(b.typ, 'B');
    a.typ = 'A';
    EXPECT_EQ(zerlegeAttribute(attributDaten(a)).typ, 'A');
}

TEST(KermitAttribute, UnbekannteUndKaputte) {
    // unbekanntes Attribut '.' wird uebergangen, abgeschnittenes Ende beendet
    const std::string d = std::string(".") + tochar(3) + "xyz" + "1" + tochar(2) + "42" + "#" + tochar(20) + "kurz";
    const Attribute a = zerlegeAttribute(d);
    EXPECT_EQ(a.laenge, 42);
    EXPECT_TRUE(a.datum.empty());
    EXPECT_EQ(zerlegeAttribute(std::string("1") + tochar(2) + "4x").laenge, -1);
    EXPECT_EQ(zerlegeAttribute(std::string("\"") + tochar(1) + "I").typ, 'B');
}

// ─── Befund K7: C-Kermit 4.2 rechnet die Blockpruefung ueber `char` ─────────
// ckfns2.c: chk2() summiert `*pkt` (char), chk1() faltet diese Summe, chk3() nimmt `c = *s++` in ein
// unsigned int.  Bei vorzeichenbehaftetem char (Z8000-cc?) zaehlt ein Byte >= 0x80 als b - 256.
// Nachbildung beider Rechenwege; Ergebnis: Typ 1 und Typ 3 sind UNBERUEHRT (Typ 1 haengt nur an den
// unteren 8 Bit der Summe, Typ 3 nur an den Halbbytes), Typ 2 weicht um 256 x (Anzahl hoher Bytes)
// mod 4096 ab.  Welche Fassung auf WEGA laeuft, misst P8000WegaKermit.PruefsummenUnd8Bit.
namespace {
unsigned ck42Summe(const std::string& s, bool vorzeichen) {
    uint16_t chk = 0;   // `unsigned int` auf dem Z8000: 16 Bit
    for (unsigned char c : s) chk = static_cast<uint16_t>(chk + (vorzeichen ? static_cast<int>(static_cast<signed char>(c)) : c));
    return chk;
}
int ck42Chk1(const std::string& s, bool v) {
    const int chk = static_cast<int16_t>(ck42Summe(s, v));
    return ((((chk & 0300) >> 6) + chk) & 077);
}
int ck42Chk2(const std::string& s, bool v) { return static_cast<int>(ck42Summe(s, v) & 07777); }
unsigned ck42Chk3(const std::string& s, bool v) {
    uint32_t crc = 0;
    for (unsigned char b : s) {
        const unsigned c = v ? static_cast<unsigned>(static_cast<int>(static_cast<signed char>(b))) & 0xFFFF : b;
        unsigned q = (crc ^ c) & 017;
        crc = (crc >> 4) ^ (q * 010201);
        q = (crc ^ (c >> 4)) & 017;
        crc = (crc >> 4) ^ (q * 010201);
    }
    return crc & 0xFFFF;
}
}  // namespace

TEST(KermitPaket, Ck42VorzeichenbehaftetesCharTrifftNurTyp2) {
    std::string paket = "~!D";   // LEN SEQ TYPE + 8-Bit-Rohdaten (ohne QBIN)
    for (int i = 0xA0; i < 0xFF; ++i) paket += static_cast<char>(i);
    const auto* b = reinterpret_cast<const uint8_t*>(paket.data());
    for (bool v : {false, true}) {
        EXPECT_EQ(ck42Chk1(paket, v), pruefsumme1(b, paket.size())) << "Typ 1, vorzeichen=" << v;
        EXPECT_EQ(ck42Chk3(paket, v), crc16Kermit(b, paket.size())) << "Typ 3, vorzeichen=" << v;
    }
    EXPECT_EQ(ck42Chk2(paket, false), pruefsumme2(b, paket.size()));
    EXPECT_NE(ck42Chk2(paket, true), pruefsumme2(b, paket.size())) << "Typ 2 weicht bei signed char ab";
    // Mit QBIN '&' stehen nur druckbare 7-Bit-Zeichen im Paket: alle drei Typen stimmen.
    const std::string sieben = "~!D&!&\"&#abc";
    const auto* s7 = reinterpret_cast<const uint8_t*>(sieben.data());
    EXPECT_EQ(ck42Chk2(sieben, true), pruefsumme2(s7, sieben.size()));
}
