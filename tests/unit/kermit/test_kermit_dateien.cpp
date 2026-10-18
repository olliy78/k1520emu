/**
 * @file test_kermit_dateien.cpp
 * @brief Ordnerfreigabe der Kermit-Bibliothek (doc/design/36_kermit.md §5 Nr. 4–6,
 *        AP K2): Pfadfallen, Namensumsetzung, Platzgrenze, ls -l erzeugen und
 *        zerlegen (Zeilen von Linux UND System III/WEGA).
 */
#include <gtest/gtest.h>

#include <fstream>
#include <random>
#include <string>

#include "kermit/src/dateien.h"

namespace fs = std::filesystem;
using namespace k1520::kermit;

namespace {

struct TempOrdner {
    fs::path p;
    TempOrdner() {
        std::random_device rd;
        p = fs::temp_directory_path() / ("k1520kermit_d_" + std::to_string(rd()) + std::to_string(rd()));
        fs::create_directories(p);
    }
    ~TempOrdner() {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
};

void schreibe(const fs::path& p, const std::string& s) {
    std::ofstream f(p, std::ios::binary);
    f << s;
}

std::string mitNul() { return std::string("a\0b", 3); }

}  // namespace

// ─── Namen ─────────────────────────────────────────────────────────────────

TEST(KermitNamen, EmpfangKonvertiertWieCKermit) {
    EXPECT_EQ(nameFuerEmpfang("README", Namen::Konvertiert), "readme");
    EXPECT_EQ(nameFuerEmpfang("Make.File", Namen::Konvertiert), "make.file");
    EXPECT_EQ(nameFuerEmpfang("README", Namen::Literal), "README");
}

TEST(KermitNamen, SendenLiteralUndZltor) {
    EXPECT_EQ(nameFuerSenden("readme", Namen::Literal), "readme");
    EXPECT_EQ(nameFuerSenden("dir/a.b.c", Namen::Konvertiert), "A.BXC");
    EXPECT_EQ(nameFuerSenden(".profile", Namen::Konvertiert), "X.PROFILE");
    EXPECT_EQ(nameFuerSenden("x~1", Namen::Konvertiert), "XX1");
}

TEST(KermitNamen, TextEndungen) {
    EXPECT_TRUE(istTextEndung("a.TXT"));
    EXPECT_TRUE(istTextEndung("ckfns.c"));
    EXPECT_FALSE(istTextEndung("kermit"));
    EXPECT_FALSE(istTextEndung("a.bin"));
}

// ─── Pfadfallen ────────────────────────────────────────────────────────────

TEST(KermitFreigabe, PfadfallenWerdenAbgewiesen) {
    TempOrdner t;
    Freigabe f(t.p);
    ASSERT_TRUE(f.gueltig());
    fs::path p;
    std::string fe;
    for (const std::string& boese : {std::string("../../etc/passwd"), std::string("/etc/passwd"),
                                     std::string("C:\\x"), std::string("c:x"), std::string("a\\b"), mitNul(),
                                     std::string("a\x1b"), std::string("CON"), std::string("nul.txt"),
                                     std::string("sub/../../x"), std::string("..")}) {
        fe.clear();
        EXPECT_FALSE(f.aufloesen(boese, p, fe)) << boese;
        EXPECT_FALSE(fe.empty());
    }
    EXPECT_TRUE(f.aufloesen("a/./b/../c", p, fe));
    EXPECT_EQ(p, t.p / "a" / "c");
    EXPECT_TRUE(f.aufloesen("com10", p, fe));   // nur COM1–9 sind reserviert
}

TEST(KermitFreigabe, AusbrechenderSymlink) {
    TempOrdner t, draussen;
    schreibe(draussen.p / "geheim", "x");
    std::error_code ec;
    fs::create_directory_symlink(draussen.p, t.p / "aus", ec);
    if (ec) GTEST_SKIP() << "Symlinks nicht verfuegbar";
    fs::create_symlink(draussen.p / "geheim", t.p / "link", ec);
    fs::create_symlink(draussen.p / "fehlt", t.p / "haengt", ec);   // Ziel fehlt
    fs::create_directory(t.p / "drin");
    fs::create_symlink(t.p / "drin", t.p / "gut", ec);
    Freigabe f(t.p);
    fs::path p;
    std::string fe;
    EXPECT_FALSE(f.aufloesen("aus/geheim", p, fe));
    EXPECT_FALSE(f.aufloesen("aus/neu", p, fe));
    EXPECT_FALSE(f.aufloesen("link", p, fe));
    EXPECT_FALSE(f.aufloesen("haengt", p, fe)) << "Anlegen ueber haengenden Verweis";
    EXPECT_TRUE(f.aufloesen("gut/x", p, fe));
    std::string g;
    EXPECT_EQ(f.anlegen("haengt", true, g, fe), nullptr);
    EXPECT_FALSE(fs::exists(draussen.p / "fehlt"));
    EXPECT_TRUE(f.finde("li*", fe).empty());
}

TEST(KermitFreigabe, WechselnFindenLoeschen) {
    TempOrdner t;
    fs::create_directories(t.p / "sub" / "tief");
    schreibe(t.p / "sub" / "a.c", "1");
    schreibe(t.p / "sub" / "b.c", "22");
    schreibe(t.p / "sub" / "c.h", "333");
    schreibe(t.p / ".versteckt", "v");
    Freigabe f(t.p);
    std::string fe;
    EXPECT_FALSE(f.wechseln("nichtda", fe));
    EXPECT_FALSE(f.wechseln("sub/a.c", fe));
    EXPECT_TRUE(f.wechseln("sub", fe));
    EXPECT_EQ(f.aktuell(), "sub");
    EXPECT_EQ(f.finde("*.c", fe).size(), 2u);
    EXPECT_EQ(f.finde("?.h", fe).size(), 1u);
    EXPECT_TRUE(f.finde("tief", fe).empty());   // Verzeichnis ist keine Datei
    EXPECT_TRUE(f.finde("*.x", fe).empty());
    EXPECT_TRUE(f.wechseln("tief", fe));
    EXPECT_TRUE(f.wechseln("../..", fe));
    EXPECT_EQ(f.aktuell(), "");
    EXPECT_FALSE(f.wechseln("..", fe));
    EXPECT_TRUE(f.wechseln("sub", fe));
    EXPECT_TRUE(f.wechseln("", fe));   // leer = Wurzel
    EXPECT_EQ(f.aktuell(), "");
    std::string m;
    EXPECT_TRUE(f.loeschen("sub/*.c", m));
    EXPECT_NE(m.find("sub/a.c"), std::string::npos);
    EXPECT_FALSE(fs::exists(t.p / "sub" / "a.c"));
    EXPECT_FALSE(f.loeschen("sub/*.c", m));
    Freigabe nur(t.p, true);
    EXPECT_FALSE(nur.loeschen("sub/c.h", m));
    std::string g;
    EXPECT_EQ(nur.anlegen("neu", true, g, fe), nullptr);
}

TEST(KermitFreigabe, AnlegenNamenslaengeUndPlatz) {
    TempOrdner t;
    Freigabe f(t.p);
    f.setzeMaxNamenslaenge(14);
    std::string g, fe;
    EXPECT_EQ(f.anlegen("fuenfzehnzeiche", true, g, fe), nullptr);
    EXPECT_NE(fe.find("laenger als 14"), std::string::npos);
    auto s = f.anlegen("vierzehnzeiche", true, g, fe);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(g, "vierzehnzeiche");
    EXPECT_TRUE(s->schliessen("19880204 12:30:00"));
    EXPECT_EQ(f.anlegen("fehlt/x", true, g, fe), nullptr);
    fs::create_directory(t.p / "dir");
    EXPECT_EQ(f.anlegen("dir", true, g, fe), nullptr);
    EXPECT_EQ(f.anlegen("vierzehnzeiche", false, g, fe), nullptr);
    f.setzePlatzGrenze(5);
    auto v = f.anlegen("voll", true, g, fe);
    ASSERT_NE(v, nullptr);
    EXPECT_TRUE(v->schreiben(reinterpret_cast<const uint8_t*>("abc"), 3));
    EXPECT_FALSE(v->schreiben(reinterpret_cast<const uint8_t*>("defg"), 4));
    v->verwerfen();
    EXPECT_FALSE(fs::exists(t.p / "voll"));
}

TEST(KermitFreigabe, QuelleUndTextQuelle) {
    TempOrdner t;
    schreibe(t.p / "q", "hallo");
    std::string fe;
    auto q = oeffneQuelle(t.p / "q", fe);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->groesse(), 5);
    EXPECT_EQ(q->datum().size(), 17u);
    uint8_t b[16];
    EXPECT_EQ(q->lesen(b, sizeof b), 5);
    EXPECT_EQ(q->lesen(b, sizeof b), 0);
    EXPECT_EQ(oeffneQuelle(t.p / "nicht", fe), nullptr);
    auto tq = textQuelle("xy");
    EXPECT_EQ(tq->lesen(b, 1), 1);
    EXPECT_EQ(tq->lesen(b, 8), 1);
    EXPECT_EQ(tq->lesen(b, 8), 0);
}

// ─── ls -l ─────────────────────────────────────────────────────────────────

TEST(KermitLs, SystemIIIZeilen) {
    auto e = zerlegeLs("-rw-r--r-- 1 wega     system     5801 May 30 20:56 tar.1");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.art, '-');
    EXPECT_EQ(e.links, 1);
    EXPECT_EQ(e.besitzer, "wega");
    EXPECT_EQ(e.gruppe, "system");
    EXPECT_EQ(e.groesse, 5801);
    EXPECT_EQ(e.monat, 5);
    EXPECT_EQ(e.tag, 30);
    EXPECT_EQ(e.stunde, 20);
    EXPECT_EQ(e.minute, 56);
    EXPECT_EQ(e.name, "tar.1");

    e = zerlegeLs("drwxrwxrwx12 wega     system      368 Jan 27 13:30 ..");   // Linkzahl angeklebt
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.art, 'd');
    EXPECT_EQ(e.links, 12);
    EXPECT_EQ(e.name, "..");

    e = zerlegeLs("   99   20 -rw-r--r-- 1 wega     system    10156 Jan  5 12:52 calling.dr.z");   // ls -is
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.groesse, 10156);
    EXPECT_EQ(e.tag, 5);
    EXPECT_EQ(e.name, "calling.dr.z");

    e = zerlegeLs("-rwsr-xr-x 1 root     sys        78560 Feb  4  1988 kermit");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.jahr, 1988);
    EXPECT_EQ(e.stunde, -1);
    EXPECT_EQ(e.rechte, "-rwsr-xr-x");

    e = zerlegeLs("-rw-r--r-- 1 wega       5801 May 30 20:56 v7ohnegruppe");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.besitzer, "wega");
    EXPECT_TRUE(e.gruppe.empty());

    e = zerlegeLs("crw--w--w- 1 root     sys       4,  0 Jan  1 12:00 tty0");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.art, 'c');
    EXPECT_EQ(e.groesse, -1);
    EXPECT_EQ(e.gruppe, "sys");
}

TEST(KermitLs, LinuxZeilen) {
    auto e = zerlegeLs("-rw-r--r-- 1 olliy olliy 24117 Sep 30  1987 ckfns.c\r");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.groesse, 24117);
    EXPECT_EQ(e.jahr, 1987);
    EXPECT_EQ(e.name, "ckfns.c");

    e = zerlegeLs("lrwxrwxrwx 1 root root 7 Jan  1 12:00 bin -> usr/bin");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.art, 'l');
    EXPECT_EQ(e.name, "bin");
    EXPECT_EQ(e.ziel, "usr/bin");

    e = zerlegeLs("-rw-r--r--. 1 root root 0 Oct 18 02:15 name mit  leerzeichen");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.name, "name mit  leerzeichen");

    e = zerlegeLs("crw-rw---- 1 root tty 4, 0 Oct 18 02:15 tty0");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.groesse, -1);

    e = zerlegeLs("-rw-r--r-- 1 u g 12 2026-10-18 02:15 iso.txt");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.jahr, 2026);
    EXPECT_EQ(e.monat, 10);
    EXPECT_EQ(e.stunde, 2);
    EXPECT_EQ(e.name, "iso.txt");

    e = zerlegeLs("-rw-r--r-- 1 u g 12 2026-10-18 02:15:33.123456789 +0200 full iso");
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.minute, 15);
    EXPECT_EQ(e.name, "full iso");

    e = zerlegeLs("-rw-r--r--+ 1 u Jan 5 Oct 18 02:15 besitzer_heisst_jan");   // Monat als Gruppenname
    ASSERT_TRUE(e.gueltig);
    EXPECT_EQ(e.groesse, 5);
    EXPECT_EQ(e.gruppe, "Jan");
}

TEST(KermitLs, KeineEintraege) {
    EXPECT_FALSE(zerlegeLs("total 12").gueltig);
    EXPECT_FALSE(zerlegeLs("").gueltig);
    EXPECT_FALSE(zerlegeLs("ls: cannot access x").gueltig);
    EXPECT_FALSE(zerlegeLs("-rw-r--r-- x u g 1 Oct 18 02:15 a").gueltig);
    EXPECT_FALSE(zerlegeLs("-rw-r--r-- 1 u g 1 Oct 18 02:15").gueltig);    // kein Name
    EXPECT_FALSE(zerlegeLs("-rw-r--r-- 1 u g zz Oct 18 02:15 a").gueltig);   // keine Groesse
    EXPECT_FALSE(zerlegeLs("-rw-r--r--xy 1 u g 1 Oct 18 02:15 a").gueltig);
}

TEST(KermitLs, ErzeugtesListingIstLesbar) {
    TempOrdner t;
    schreibe(t.p / "b.txt", std::string(1000, 'x'));
    schreibe(t.p / "a", "");
    schreibe(t.p / ".hidden", "h");
    fs::create_directory(t.p / "dir");
    Freigabe f(t.p);
    std::string text, fe;
    ASSERT_TRUE(f.liste("", text, fe));
    EXPECT_EQ(text.rfind("total ", 0), 0u);
    const auto v = zerlegeLsListe(text);
    ASSERT_EQ(v.size(), 3u);   // ohne Punktdatei, sortiert
    EXPECT_EQ(v[0].name, "a");
    EXPECT_EQ(v[0].groesse, 0);
    EXPECT_EQ(v[1].name, "b.txt");
    EXPECT_EQ(v[1].groesse, 1000);
    EXPECT_EQ(v[2].name, "dir");
    EXPECT_EQ(v[2].art, 'd');
    EXPECT_GT(v[1].stunde, -1);   // frisch: mit Uhrzeit
    ASSERT_TRUE(f.liste("*.txt", text, fe));
    EXPECT_EQ(zerlegeLsListe(text).size(), 1u);
    ASSERT_TRUE(f.liste("b.txt", text, fe));
    EXPECT_EQ(zerlegeLsListe(text).at(0).groesse, 1000);
    EXPECT_FALSE(f.liste("nicht", text, fe));
    EXPECT_FALSE(f.liste("*.q", text, fe));
    EXPECT_FALSE(f.liste("../x", text, fe));
}

TEST(KermitLs, ZeilenformatSystemIII) {
    VerzeichnisEintrag e;
    e.rechte = "-rw-r--r--";
    e.links = 1;
    e.besitzer = "wega";
    e.gruppe = "system";
    e.groesse = 5801;
    e.monat = 5;
    e.tag = 30;
    e.stunde = 20;
    e.minute = 56;
    e.jahr = 1989;
    e.name = "tar.1";
    EXPECT_EQ(lsZeile(e, true), "-rw-r--r-- 1 wega     system     5801 May 30 20:56 tar.1");
    EXPECT_EQ(lsZeile(e, false), "-rw-r--r-- 1 wega     system     5801 May 30  1989 tar.1");
}
