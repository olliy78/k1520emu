/**
 * @file drucker_rom_katalog.h
 * @brief Katalog der EPROM-Saetze der Nadeldrucker - eine YAML-Datei mit Intel-HEX-Daten (WP9, Entwurf 49 §12).
 *
 * `data/drucker_roms.yaml` traegt Metadaten UND ROM-Bytes (`daten: |`, Intel-HEX, 16 Byte je Zeile, Adresse ab
 * 0000H).  Mehrfacheintraege (IFSS-Varianten) verweisen mit `daten_von: <schluessel>` auf dieselben Daten.
 * Keine MD5: die Pruefsumme jeder HEX-Zeile und das Feld `groesse` sind die Integritaetspruefung.  Ein Fehler in
 * der Datei meldet Zeile und Eintrag; es gibt keinen Rueckfall auf eingebettete Abbilder.
 *
 * Suchpfad (der erste vorhandene gewinnt): `K1520_PRINTER_ROMS` (Datei oder Verzeichnis, ausschliesslich, wenn
 * gesetzt) -> `<Modul>/../share/k1520emu/drucker_roms.yaml` -> `K1520_PRINTER_ROMS_DEFAULT` (CMake, Quellbaum;
 * in Release-Bauten leer) -> `data/drucker_roms.yaml` relativ zum Arbeitsverzeichnis.
 *
 * Schichtung: Maschinenebene (Karten kennen den Katalog nicht, sie bekommen die Bytes in ihrer Config).
 * @see core/util/yaml_lite.h, core/peripherals/floppy_drive/format_catalog.h (Muster)
 */
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace yaml { class Node; }

struct DruckerRomEintrag {
    std::string schluessel;
    std::string typ;          ///< sd1157 | k6316-ifss | k6316-v24 | k6313 | k6313-ifss | k6311 | k6311-ifss
    std::string titel, pgm;
    std::vector<std::string> schnittstellen;   ///< "ifss" | "v24"
    std::string zeichensatz;
    std::string guete;        ///< ok | unvollstaendig | defekt | unbekannt
    std::string hinweis;
    std::string herkunft;     ///< Beleg (Abzug unter doc/EPROMS), nur Anzeige/Wachter
    bool vorgabe = false;
    std::vector<std::string> aliases;
    int zeile = 0;            ///< Zeile des Eintrags in der Datei
    std::shared_ptr<const std::vector<uint8_t>> daten;   ///< geteilt bei `daten_von`
};

class DruckerRomKatalog {
public:
    /// Alle Typschluessel in fester Reihenfolge (Index = Position in der C-ABI-Typnummer 0..6).
    static const std::vector<std::string>& typen();
    /// Erlaubte Laengen der Daten je Typ (Fassungen); leer bei unbekanntem Typ.
    static std::vector<size_t> erlaubteGroessen(const std::string& typ);

    /// Kandidaten der Suche in Rangfolge (nur Pfade, nicht auf Existenz geprueft).
    static std::vector<std::string> searchPaths();
    /// Gemeinsamer Katalog des Prozesses: laedt beim ersten Aufruf und wieder, wenn sich Pfad/Aenderungszeit/Groesse
    /// der Datei geaendert haben (Tests setzen die Umgebung um).  Bei Fehler: `ok() == false`, `fehler()` gefuellt.
    static std::shared_ptr<const DruckerRomKatalog> gemeinsam();
    /// Datei laden (Pfad ohne Suche); Fehler siehe `fehler()`.
    static DruckerRomKatalog laden(const std::string& pfad);
    /// Aus Text laden (Tests); `name` erscheint in Fehlermeldungen.
    static DruckerRomKatalog ausText(const std::string& text, const std::string& name);

    bool ok() const { return fehler_.empty(); }
    /// Mehrzeilige Fehlerbeschreibung (leer bei ok); mit Zeile und Eintrag, ggf. Abhilfe-Hinweis.
    const std::string& fehler() const { return fehler_; }
    const std::string& pfad() const { return pfad_; }
    const std::vector<DruckerRomEintrag>& eintraege() const { return eintraege_; }

    /// Eintrag eines Typs per Schluessel oder Alias; nullptr, wenn es ihn dort nicht gibt.
    const DruckerRomEintrag* finde(const std::string& typ, const std::string& wahl) const;
    /// Vorgabe eines Typs (genau eine je Typ, vom Leser erzwungen); nullptr nur ohne Katalog.
    const DruckerRomEintrag* vorgabe(const std::string& typ) const;
    /// Die Eintraege eines Typs in Dateireihenfolge.
    std::vector<const DruckerRomEintrag*> fuerTyp(const std::string& typ) const;

private:
    static DruckerRomKatalog ausKnoten(const yaml::Node& wurzel, const std::string& name);

    std::vector<DruckerRomEintrag> eintraege_;
    std::vector<long> groessen_;   ///< Feld `groesse` je Eintrag (nur Leser)
    std::string fehler_;
    std::string pfad_;
};
