"""Waechter DruckerRomKatalog.* (Python-Seite): ROM-Katalog der Drucker, gelesen vom Kern (Entwurf 49 §12, WP9, Merkposten drucker.md Nr. 38).

Schema, Pruefsummen, Adressen, Laengen und `daten_von` prueft der Leser selbst (gtest ``DruckerRomKatalog.*``);
hier: die C-ABI-Abfrage, `rom=<Schluessel|Alias>` am Kern, die Fehlermeldung bei fehlendem/manipuliertem Katalog
(Zeile und Eintrag), die ASCII-Kennung im ROM gegen den Namen und die HERKUNFT (jeder Eintrag byteweise gleich seiner
``.bin`` unter doc/EPROMS - nur im Quellbaum, nicht im Paket).  Laeuft ohne Qt und ohne Greaseweazle.
"""

import os
from pathlib import Path

import pytest
import yaml

from conftest import PROJECT_ROOT

from app import paths
from app.core_binding import k1520 as K
from app.printertool import typen

KATALOG = PROJECT_ROOT / "data" / "drucker_roms.yaml"
EPROMS = PROJECT_ROOT / "doc" / "EPROMS"
TYPEN = ("sd1157", "k6316-ifss", "k6316-v24", "k6313", "k6311", "k6313-ifss", "k6311-ifss")
#: C-ABI-Typnummer je Katalogtyp (k1520_create_printer)
TYP_NR = {t: i for i, t in enumerate(TYPEN)}


def _hex(text: str) -> bytes:
    """Intel-HEX-Block -> Bytes (Pruefsumme, lueckenlose Adressen, Endsatz) - unabhaengig vom C++-Leser."""
    out = bytearray()
    ende = False
    for zeile in text.split("\n"):
        zeile = zeile.strip()
        if not zeile:
            continue
        assert not ende and zeile.startswith(":")
        b = bytes.fromhex(zeile[1:])
        assert sum(b) & 0xFF == 0, zeile
        n, adr, art = b[0], (b[1] << 8) | b[2], b[3]
        assert len(b) == n + 5
        if art == 1:
            ende = True
        else:
            assert art == 0 and adr == len(out), zeile
            out += b[4:4 + n]
    assert ende
    return bytes(out)


@pytest.fixture(scope="module")
def roh():
    return yaml.safe_load(KATALOG.read_text(encoding="utf-8"))


@pytest.fixture(scope="module")
def roms():
    r = K.drucker_roms()
    assert r, K.drucker_roms_fehler()
    return r


def _drucker(typ, konfig):
    return K._lib, K._lib.k1520_create_printer(typ, konfig.encode())


def _byte(lib, h, adr):
    return lib.k1520_mem_read(h, adr)


# ── C-ABI: Katalog abfragen ─────────────────────────────────────────────────

def test_DruckerRomKatalog_CAbi_liefert_alle_Eintraege_mit_Feldern(roms, roh):
    assert K._lib.k1520_printer_rom_count() == len(roms) == len(roh["eintraege"])
    assert K.drucker_roms_fehler() == ""
    for r, e in zip(roms, roh["eintraege"]):
        assert (r.schluessel, r.typ, r.titel) == (e["schluessel"], e["typ"], e["titel"])
        assert r.guete == e["guete"] and r.vorgabe == e["vorgabe"] and r.groesse == e["groesse"]
        assert r.schnittstellen == tuple(e["schnittstellen"]) and r.aliases == tuple(e["aliases"])
        assert r.hinweis == e["hinweis"] and r.pgm == e["pgm"] and r.zeichensatz == e["zeichensatz"]
    assert K._lib.k1520_printer_rom_info(0, 99) == b"" and K._lib.k1520_printer_rom_info(9999, 0) == b""


def test_DruckerRomKatalog_genau_eine_Vorgabe_je_Typ_und_Schluessel_eindeutig(roms):
    for t in TYPEN:
        assert len([r for r in typen.roms_fuer_typ(roms, t) if r.vorgabe]) == 1, t
    assert len({r.schluessel for r in roms}) == len(roms)
    assert typen.rom_aufloesen(roms, None, "sd1157").schluessel == "sd1157-repariert"      # = bisheriges Verhalten
    assert typen.rom_aufloesen(roms, "original", "sd1157").schluessel == "sd1157-original"
    assert typen.rom_aufloesen(roms, "repariert", "sd1157").schluessel == "sd1157-repariert"
    assert typen.rom_aufloesen(roms, "gibtsnicht", "k6313").schluessel == "k6313-33538013"
    assert typen.rom_aufloesen(roms, "original", "k6316-ifss").schluessel == "k6316-ifss"  # Alias nur im eigenen Typ... Vorgabe
    assert typen.rom_aufloesen([], "x", "sd1157") is None


def test_DruckerRomKatalog_daten_von_loest_auf_und_Datei_ist_eine(roh):
    eig = {e["schluessel"]: e for e in roh["eintraege"] if "daten" in e}
    ifss = [e for e in roh["eintraege"] if "daten_von" in e]
    assert len(ifss) == 5
    for e in ifss:
        assert e["daten_von"] in eig and "daten" not in e and e["groesse"] == eig[e["daten_von"]]["groesse"]
    assert not any("md5" in e or "datei" in e for e in roh["eintraege"]), "weder MD5 noch Dateipfad im Katalog"


# ── Herkunft: jeder Eintrag byteweise gleich seiner .bin (nur Quellbaum) ─────────

def test_DruckerRomKatalog_Herkunft_jeder_Eintrag_byteweise_gleich_den_Abzuegen(roh):
    eig = {e["schluessel"]: e for e in roh["eintraege"]}
    n = 0
    for e in roh["eintraege"]:
        if "daten_von" in e:
            continue
        quellen = e["herkunft"] if isinstance(e["herkunft"], list) else [e["herkunft"]]
        soll = b"".join((EPROMS / q).read_bytes() for q in quellen)
        assert _hex(e["daten"]) == soll, e["schluessel"]
        assert len(soll) == e["groesse"]
        n += 1
    assert n == len(eig) - 5


@pytest.mark.parametrize("typ", ["k6313", "k6311", "k6313-ifss", "k6311-ifss"])
def test_DruckerRomKatalog_Kennung_im_ROM_stimmt_mit_dem_Namen(roh, typ):
    # Dateiname x nn F ZS PS; die Firmware traegt F ZS x nn PS als ASCII am Ende des 10-/16-KiB-Abzugs (27F8H/3FF8H, 13325212: 1FF8H)
    eig = {e["schluessel"]: e for e in roh["eintraege"]}
    liste = [e for e in roh["eintraege"] if e["typ"] == typ]
    assert liste
    for e in liste:
        quelle = eig[e.get("daten_von", e["schluessel"])]["herkunft"][0]
        n = Path(quelle).stem.split("_")[0]
        daten = _hex(eig[e.get("daten_von", e["schluessel"])]["daten"])
        assert daten[len(daten) - 8:].decode("ascii") == n[3] + n[4:6] + n[0] + n[1:3] + n[6:8], e["schluessel"]
        assert e["guete"] == "ok" and len(daten) in (8192, 10240, 16384)


# ── Kern: rom=<Schluessel|Alias> ─────────────────────────────────────────────

def test_DruckerRomKatalog_rom_liefert_die_Bytes_des_Eintrags(roh):
    eig = {e["schluessel"]: e for e in roh["eintraege"]}
    for e in roh["eintraege"]:
        daten = _hex(eig[e.get("daten_von", e["schluessel"])]["daten"])
        lib, h = _drucker(TYP_NR[e["typ"]], f"rom={e['schluessel']}")
        assert h, (e["schluessel"], K.drucker_roms_fehler(), lib.k1520_last_init_error())
        try:
            for adr in (0x0000, 0x0801, 0x0D34, 0x1234, 0x1FF8, min(len(daten) - 1, 0x27FF)):
                if adr < len(daten):
                    assert _byte(lib, h, adr) == daten[adr], (e["schluessel"], hex(adr))
            if len(daten) == 8192:                       # K6311-IFSS: fuenfter Sockel FFH
                assert all(_byte(lib, h, a) == 0xFF for a in (0x2000, 0x2400, 0x27FF))
            if len(daten) == 16384:                      # Plan "lang"
                assert _byte(lib, h, 0x3FF8) == daten[0x3FF8] and _byte(lib, h, 0x4000) == 0xFF
        finally:
            lib.k1520_destroy(h)


def test_DruckerRomKatalog_ohne_rom_gilt_die_Vorgabe_und_Aliase_laufen():
    lib, h = _drucker(0, "")
    assert h and _byte(lib, h, 0x0D34) == 0x0B                  # sd1157-repariert (Vorgabe)
    lib.k1520_destroy(h)
    for alias, soll in (("original", 0x2B), ("sd1157-original", 0x2B), ("repariert", 0x0B)):
        lib, h = _drucker(0, f"rom={alias}")
        assert h and _byte(lib, h, 0x0D34) == soll, alias
        lib.k1520_destroy(h)
    # K6311 IFSS ohne rom=: Vorgabe 13325212 (8 KiB, Kennung bei 1FF8H)
    lib, h = _drucker(6, "")
    assert h and bytes(_byte(lib, h, 0x1FF8 + i) for i in range(8)) == b"25213312"
    lib.k1520_destroy(h)


def test_DruckerRomKatalog_rom_unbekannt_und_romdatei_entfallen_klare_Meldung(tmp_path):
    assert not K._lib.k1520_create_printer(0, b"rom=gibtsnicht")
    m = K._lib.k1520_last_init_error().decode()
    assert "gibtsnicht" in m and "sd1157-repariert" in m and "sd1157-original" in m
    assert not K._lib.k1520_create_printer(3, b"rom=k6316-v24")          # Schluessel eines anderen Typs
    assert "unbekannt" in K._lib.k1520_last_init_error().decode()
    assert not K._lib.k1520_create_printer(0, f"romdatei={tmp_path / 'x.rom'}".encode())
    m = K._lib.k1520_last_init_error().decode()
    assert "romdatei" in m and "rom=" in m


# ── fehlender / manipulierter Katalog: klare Meldung, kein Rueckfall ────────────

def _mit_katalog(monkeypatch, pfad):
    monkeypatch.setenv("K1520_PRINTER_ROMS", str(pfad))


def test_DruckerRomKatalog_fehlender_Katalog_meldet_klar_und_laeuft_nicht_mit_eingebettetem_Abbild(tmp_path, monkeypatch):
    _mit_katalog(monkeypatch, tmp_path / "gibtsnicht.yaml")
    assert K._lib.k1520_printer_rom_count() == -1 and K.drucker_roms() == []
    assert "nicht gefunden" in K.drucker_roms_fehler() and "gibtsnicht.yaml" in K.drucker_roms_fehler()
    assert not K._lib.k1520_create_printer(0, b"")
    assert "nicht gefunden" in K._lib.k1520_last_init_error().decode()
    assert not K._lib.k1520_create_printer(0, b"rom=original")
    monkeypatch.delenv("K1520_PRINTER_ROMS")
    assert K._lib.k1520_printer_rom_count() > 0                         # wieder der Quellbaum-Katalog


def test_DruckerRomKatalog_manipulierte_Datei_meldet_Zeile_und_Eintrag(tmp_path, monkeypatch):
    text = KATALOG.read_text(encoding="utf-8").split("\n")
    i = next(n for n, z in enumerate(text) if z.startswith("      :10000000"))
    z = text[i]
    text[i] = z[:-1] + ("0" if z[-1] != "0" else "1")                   # Pruefsumme kaputt
    p = tmp_path / "drucker_roms.yaml"
    p.write_text("\n".join(text), encoding="utf-8")
    _mit_katalog(monkeypatch, p)
    assert K._lib.k1520_printer_rom_count() == -1
    m = K.drucker_roms_fehler()
    assert f"{p}:{i + 1}:" in m and "Eintrag 'sd1157-repariert'" in m and "Pruefsumme" in m, m
    assert not K._lib.k1520_create_printer(0, b"")
    assert f":{i + 1}:" in K._lib.k1520_last_init_error().decode()
    # eine geaenderte Datenzeile MIT richtiger Summe wird akzeptiert (Texteditor-Anwendung): letztes Datenbyte der Zeile aendern
    b = bytearray(bytes.fromhex(z.strip()[1:]))
    b[4] ^= 0xFF
    b[-1] = (-sum(b[:-1])) & 0xFF
    text[i] = "      :" + b.hex().upper()
    p.write_text("\n".join(text), encoding="utf-8")
    assert K._lib.k1520_printer_rom_count() > 0
    lib, h = _drucker(0, "")
    assert h and _byte(lib, h, 0x0000) == 0xED ^ 0xFF
    lib.k1520_destroy(h)


def test_DruckerRomKatalog_describe_nennt_den_Weg_des_Katalogs():
    text = paths.describe()
    assert "Drucker-ROMs:" in text and "drucker_roms.yaml" in text
    assert not hasattr(paths, "printer_eproms_dir") and not hasattr(paths, "ENV_PRINTER_EPROMS")
