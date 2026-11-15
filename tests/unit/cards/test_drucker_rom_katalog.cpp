/**
 * @file test_drucker_rom_katalog.cpp
 * @brief Waechter DruckerRomKatalog.*: Leser von data/drucker_roms.yaml (Entwurf 49 §12, WP9).
 *
 * Schema, genau eine Vorgabe je Typ, Aliase, `daten_von`, Intel-HEX-Pruefsummen/Adressen/Laenge; Negativtests mit
 * manipulierter Datei (Zeile und Eintrag muessen in der Meldung stehen).  Die Herkunft (Bytes == .bin) prueft
 * tests/python/test_drucker_rom_katalog.py, die Druckbild-Waechter sind die Gegenprobe der Bytes.
 */
#include <gtest/gtest.h>

#include <algorithm>

#include <chrono>
#include <chrono>
#include <fstream>
#include <sstream>

#include "core/machines/printer/drucker_rom_katalog.h"
#include "core/util/os_compat.h"

namespace {

std::string dateiText()
{
    std::ifstream f(DRUCKER_ROMS_YAML, std::ios::binary);
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

/// Text mit `von` -> `nach` an der ersten Fundstelle (Fundstelle muss es geben).
std::string ersetze(std::string t, const std::string& von, const std::string& nach)
{
    const size_t p = t.find(von);
    EXPECT_NE(p, std::string::npos) << von;
    if (p != std::string::npos) t.replace(p, von.size(), nach);
    return t;
}

}  // namespace

/**
 * @test DruckerRomKatalog.AusgelieferterKatalogIstInOrdnung
 * @par Kriterium  Katalog laedt; alle sieben Typen haben genau eine Vorgabe; Aliase loesen auf; `daten_von` teilt dieselben
 *                 Daten (kein Duplikat); Laengen je Typ stimmen.  Ladezeit wird gemessen (Ausgabe).
 */
TEST(DruckerRomKatalog, AusgelieferterKatalogIstInOrdnung)
{
    const auto t0 = std::chrono::steady_clock::now();
    const DruckerRomKatalog k = DruckerRomKatalog::laden(DRUCKER_ROMS_YAML);
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    ASSERT_TRUE(k.ok()) << k.fehler();
    std::printf("[ MESSUNG ] %zu Eintraege aus %zu Bytes Text in %.1f ms\n", k.eintraege().size(), dateiText().size(), ms);

    for (const auto& typ : DruckerRomKatalog::typen()) {
        const auto* v = k.vorgabe(typ);
        ASSERT_NE(v, nullptr) << typ;
        int n = 0;
        for (const auto* e : k.fuerTyp(typ)) n += e->vorgabe;
        EXPECT_EQ(n, 1) << typ;
    }
    EXPECT_EQ(k.vorgabe("sd1157")->schluessel, "sd1157-repariert");   // = bisheriges Verhalten
    EXPECT_EQ(k.vorgabe("k6313")->schluessel, "k6313-33538013");

    ASSERT_NE(k.finde("sd1157", "repariert"), nullptr);
    EXPECT_EQ(k.finde("sd1157", "repariert")->schluessel, "sd1157-repariert");
    EXPECT_EQ(k.finde("sd1157", "original")->schluessel, "sd1157-original");
    EXPECT_EQ(k.finde("sd1157", "sd1157-original")->schluessel, "sd1157-original");
    EXPECT_EQ(k.finde("k6316-ifss", "original"), nullptr) << "Alias gilt nur im eigenen Typ";
    EXPECT_EQ(k.finde("sd1157", "gibtsnicht"), nullptr);

    // IFSS-Varianten teilen die Daten des V.24-Eintrags
    ASSERT_NE(k.finde("k6313-ifss", "k6313-ifss-33538013"), nullptr);
    EXPECT_EQ(k.finde("k6313-ifss", "k6313-ifss-33538013")->daten.get(), k.finde("k6313", "k6313-33538013")->daten.get());
    EXPECT_EQ(k.finde("k6313-ifss", "k6313-ifss-30835007")->daten->size(), 16384u);
    EXPECT_EQ(k.finde("k6311-ifss", "k6311-ifss-13325212")->daten->size(), 8192u);
    for (const auto& e : k.eintraege()) {
        ASSERT_NE(e.daten, nullptr) << e.schluessel;
        const auto ok = DruckerRomKatalog::erlaubteGroessen(e.typ);
        EXPECT_NE(std::find(ok.begin(), ok.end(), e.daten->size()), ok.end()) << e.schluessel;
    }
    // Stichprobe: der Vorgabe-Abzug des SD 1157 beginnt mit IM 2 / LD A,2CH (sy00)
    EXPECT_EQ((*k.vorgabe("sd1157")->daten)[0], 0xED);
}

/// Zusammenspiel Suchpfad: Umgebung gesetzt => nur dort suchen, kein stiller Rueckfall; fehlende Datei = klare Meldung.
TEST(DruckerRomKatalog, UmgebungOhneDateiMeldetKlarUndFaelltNichtZurueck)
{
    k1520::os::setEnv("K1520_PRINTER_ROMS", "/gibt/es/nicht/drucker_roms.yaml");
    auto k = DruckerRomKatalog::gemeinsam();
    k1520::os::unsetEnv("K1520_PRINTER_ROMS");
    EXPECT_FALSE(k->ok());
    EXPECT_NE(k->fehler().find("nicht gefunden"), std::string::npos) << k->fehler();
    EXPECT_NE(k->fehler().find("/gibt/es/nicht"), std::string::npos);
    EXPECT_TRUE(DruckerRomKatalog::gemeinsam()->ok()) << "ohne Umgebung wieder der Quellbaum-Katalog";
}

TEST(DruckerRomKatalog, KaputtePruefsummeMeldetZeileUndEintrag)
{
    const std::string t = dateiText();
    // erste Datenzeile des ersten Eintrags: letzte Hexziffer aendern
    const size_t p = t.find("      :10000000");
    ASSERT_NE(p, std::string::npos);
    const size_t ende = t.find('\n', p);
    std::string kaputt = t;
    char& c = kaputt[ende - 1];
    c = c == '0' ? '1' : '0';
    const auto k = DruckerRomKatalog::ausText(kaputt, "x.yaml");
    EXPECT_FALSE(k.ok());
    int zeile = 1;
    for (size_t i = 0; i < p; ++i) zeile += t[i] == '\n';
    EXPECT_NE(k.fehler().find("x.yaml:" + std::to_string(zeile) + ":"), std::string::npos) << k.fehler();
    EXPECT_NE(k.fehler().find("Eintrag 'sd1157-repariert'"), std::string::npos) << k.fehler();
    EXPECT_NE(k.fehler().find("Pruefsumme"), std::string::npos) << k.fehler();
    EXPECT_TRUE(k.eintraege().empty()) << "ein fehlerhafter Katalog liefert keine Eintraege";
}

TEST(DruckerRomKatalog, GefehlteZeileUndFalscheGroesseUndVerweise)
{
    const std::string t = dateiText();
    // eine Datenzeile (die zweite, Adresse 0010H) entfernen -> Adressluecke in der naechsten Zeile
    {
        const size_t a = t.find("      :10001000");
        ASSERT_NE(a, std::string::npos);
        std::string k = t;
        k.erase(a, k.find('\n', a) - a + 1);
        const auto r = DruckerRomKatalog::ausText(k, "x.yaml");
        EXPECT_FALSE(r.ok());
        EXPECT_NE(r.fehler().find("Adresse 0020H, erwartet 0010H"), std::string::npos) << r.fehler();
    }
    // Endsatz des ersten Eintrags entfernen
    {
        const auto r = DruckerRomKatalog::ausText(ersetze(t, "      :00000001FF\n", ""), "x.yaml");
        EXPECT_FALSE(r.ok());
    }
    // groesse passt nicht zu den Daten
    {
        const auto r = DruckerRomKatalog::ausText(ersetze(t, "groesse: 10624", "groesse: 10625"), "x.yaml");
        EXPECT_FALSE(r.ok());
        EXPECT_NE(r.fehler().find("groesse sagt 10625"), std::string::npos) << r.fehler();
        EXPECT_NE(r.fehler().find("Eintrag 'sd1157-repariert'"), std::string::npos) << r.fehler();
    }
    // daten_von ins Leere / zweite Vorgabe / Typ unbekannt / Alias-Kollision
    EXPECT_NE(DruckerRomKatalog::ausText(ersetze(t, "daten_von: k6313-33538013", "daten_von: gibtsnicht"), "x.yaml").fehler().find("daten_von 'gibtsnicht'"), std::string::npos);
    EXPECT_NE(DruckerRomKatalog::ausText(ersetze(t, "vorgabe: false", "vorgabe: true"), "x.yaml").fehler().find("2 Vorgaben"), std::string::npos);
    EXPECT_NE(DruckerRomKatalog::ausText(ersetze(t, "typ: k6316-v24", "typ: k9999"), "x.yaml").fehler().find("typ 'k9999' unbekannt"), std::string::npos);
    EXPECT_NE(DruckerRomKatalog::ausText(ersetze(t, "aliases: [original]", "aliases: [repariert]"), "x.yaml").fehler().find("Alias 'repariert'"), std::string::npos);
    // Laenge je Typ: eine 16-KiB-Fassung am SD 1157 ist falsch
    EXPECT_NE(DruckerRomKatalog::ausText(ersetze(t, "typ: sd1157", "typ: k6316-v24"), "x.yaml").fehler().find("erwartet 10240"), std::string::npos);
    // kein YAML
    EXPECT_FALSE(DruckerRomKatalog::ausText(": : :\n\t-", "x.yaml").ok());
    EXPECT_FALSE(DruckerRomKatalog::ausText("version: 1\neintraege: []\n", "x.yaml").ok());
}
