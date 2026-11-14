"""Waechter DruckerRomKatalog.*: Katalog der Drucker-EPROMs (Entwurf 49 §11.2, Merkposten drucker.md Nr. 31).

Muster ``FormatKatalogSchema.*``: Schema, MD5 gegen die Dateien, genau eine Vorgabe je Typ, Aliase, Aufloesung
(Umgebung -> Installation -> Quellbaum) und der Kernschluessel ``romdatei=``.  Laeuft ohne Qt und ohne Greaseweazle.
"""

import copy
import hashlib
from pathlib import Path

import pytest
import yaml

from conftest import PROJECT_ROOT

from app import paths
from app.printertool import rom_katalog as rk


@pytest.fixture(scope="module")
def daten():
    return yaml.safe_load((PROJECT_ROOT / "data" / "drucker_roms.yaml").read_text(encoding="utf-8"))


@pytest.fixture(scope="module")
def eintraege():
    return rk.laden(PROJECT_ROOT / "data" / "drucker_roms.yaml")


def test_DruckerRomKatalog_Schema_ist_in_ordnung(daten):
    assert rk.pruefen(daten) == []


def test_DruckerRomKatalog_MD5_und_Groesse_gegen_die_Dateien(eintraege):
    ordner = PROJECT_ROOT / "doc" / "EPROMS"
    assert eintraege and rk.dateien_pruefen(eintraege, ordner) == []


def test_DruckerRomKatalog_K6316_Satz_ist_die_Aneinanderreihung_der_Chips(eintraege):
    for schluessel, chips in (("k6316-ifss", [f"ifss/I{i}.BIN" for i in range(1, 6)]),
                              ("k6316-v24", [f"v24/V{i}.BIN" for i in range(1, 6)])):
        e = next(x for x in eintraege if x.schluessel == schluessel)
        roh = b"".join((PROJECT_ROOT / "doc" / "EPROMS" / "K6316" / c).read_bytes() for c in chips)
        assert (PROJECT_ROOT / "doc" / "EPROMS" / e.datei).read_bytes() == roh


def test_DruckerRomKatalog_K6313_Kennung_im_ROM_stimmt_mit_dem_Dateinamen(eintraege):
    # Dateiname x nn F ZS PS; die Firmware traegt F ZS x nn PS als ASCII bei 27F8H (doc/EPROMS/K6313/README.md)
    liste = rk.fuer_typ("k6313", eintraege)
    assert len(liste) >= 1 and rk.vorgabe("k6313", eintraege).schluessel == "k6313-33538013"
    for e in liste:
        n = Path(e.datei).stem
        roh = (PROJECT_ROOT / "doc" / "EPROMS" / e.datei).read_bytes()
        # WP8h: 16-KiB-Abzuege (Plan "lang") tragen die Kennung am Ende des Chips (3FF8H)
        assert len(roh) in (10240, 16384)
        assert roh[len(roh) - 8:].decode("ascii") == n[3] + n[4:6] + n[0] + n[1:3] + n[6:8], e.schluessel
        assert e.guete == "ok" and e.groesse == len(roh)


def test_DruckerRomKatalog_K6311_Kennung_im_ROM_stimmt_mit_dem_Dateinamen(eintraege):
    # wie K6313: Dateiname x nn F ZS PS (ohne Anhang _1), Kennung F ZS x nn PS bei 27F8H; die Hardware ist nur erschlossen [?], die Abzuege sind ok
    liste = rk.fuer_typ("k6311", eintraege)
    assert len(liste) == 2 and rk.vorgabe("k6311", eintraege).schluessel == "k6311-12128062"
    for e in liste:
        n = Path(e.datei).stem.split("_")[0]
        roh = (PROJECT_ROOT / "doc" / "EPROMS" / e.datei).read_bytes()
        assert roh[0x27F8:0x2800].decode("ascii") == n[3] + n[4:6] + n[0] + n[1:3] + n[6:8], e.schluessel
        assert e.guete == "ok" and len(roh) == 10240
        assert "erschlossen" in e.hinweis or "nicht nachgebildet" in e.hinweis


def test_DruckerRomKatalog_IfssTypen_Kennung_Groesse_und_Schnittstelle(eintraege):
    # WP8g: K6313 IFSS = dieselben Abzuege wie am V.24 (Kennung bei 27F8H); K6311 IFSS = 13325212 (8 KiB, Kennung bei 1FF8H)
    k13 = rk.fuer_typ("k6313-ifss", eintraege)
    assert [e.schluessel for e in k13][0] == "k6313-ifss-33538013" and rk.vorgabe("k6313-ifss", eintraege).schluessel == "k6313-ifss-33538013"
    v24 = {e.datei: e.md5 for e in rk.fuer_typ("k6313", eintraege)}
    for e in k13:
        n = Path(e.datei).stem
        roh = (PROJECT_ROOT / "doc" / "EPROMS" / e.datei).read_bytes()
        assert roh[len(roh) - 8:].decode("ascii") == n[3] + n[4:6] + n[0] + n[1:3] + n[6:8], e.schluessel
        assert e.guete == "ok" and len(roh) in (10240, 16384) and e.schnittstellen == ("ifss",)
        assert v24.get(e.datei) == e.md5, "dieselbe Datei wie am V.24"
    k11 = rk.fuer_typ("k6311-ifss", eintraege)
    assert [e.schluessel for e in k11] == ["k6311-ifss-13325212"]
    e = k11[0]
    roh = (PROJECT_ROOT / "doc" / "EPROMS" / e.datei).read_bytes()
    n = Path(e.datei).stem
    assert len(roh) == 8192 == e.groesse and e.schnittstellen == ("ifss",)
    assert roh[0x1FF8:0x2000].decode("ascii") == n[3] + n[4:6] + n[0] + n[1:3] + n[6:8]
    assert "erschlossen" in e.hinweis


def test_DruckerRomKatalog_genau_eine_Vorgabe_je_Typ(eintraege):
    for t in rk.TYPEN:
        assert len([e for e in rk.fuer_typ(t, eintraege) if e.vorgabe]) == 1, t
        assert rk.vorgabe(t, eintraege) is not None
    assert rk.vorgabe("sd1157", eintraege).schluessel == "sd1157-repariert"      # = bisheriges Verhalten


def test_DruckerRomKatalog_Aliase_der_alten_Schluessel(eintraege):
    assert rk.aufloesen("repariert", "sd1157", eintraege).schluessel == "sd1157-repariert"
    assert rk.aufloesen("original", "sd1157", eintraege).schluessel == "sd1157-original"
    assert rk.aufloesen("sd1157-original", "sd1157", eintraege).schluessel == "sd1157-original"
    assert rk.aufloesen("unbekannt", "sd1157", eintraege).schluessel == "sd1157-repariert"
    assert rk.aufloesen(None, "k6316-v24", eintraege).schluessel == "k6316-v24"
    assert rk.aufloesen("original", "k6316-ifss", eintraege).schluessel == "k6316-ifss"    # Alias nur im eigenen Typ


@pytest.mark.parametrize("aendern, fragment", [
    (lambda d: d["eintraege"][1].update(schluessel="sd1157-repariert"), "doppelt"),
    (lambda d: d["eintraege"][1].update(vorgabe=True), "2 Vorgaben"),
    (lambda d: d["eintraege"][0].update(vorgabe=False), "0 Vorgaben"),
    (lambda d: d["eintraege"][0].update(md5="XYZ"), "md5"),
    (lambda d: d["eintraege"][0].update(guete="prima"), "guete"),
    (lambda d: d["eintraege"][0].update(typ="k9999"), "typ"),
    (lambda d: d["eintraege"][0].update(datei="../x.rom"), "relativ"),
    (lambda d: d["eintraege"][1].update(aliases=["repariert"]), "Alias"),
    (lambda d: d["eintraege"][0].pop("md5"), "'md5' fehlt"),
    (lambda d: d.update(version=2), "version"),
    (lambda d: d["eintraege"][0].update(groesse="10624"), "falschen Typ"),
])
def test_DruckerRomKatalog_Schemafehler_werden_gemeldet(daten, aendern, fragment):
    kaputt = copy.deepcopy(daten)
    aendern(kaputt)
    meldungen = rk.pruefen(kaputt)
    assert any(fragment in m for m in meldungen), meldungen


def test_DruckerRomKatalog_Aufloesung_der_Pfade(tmp_path, monkeypatch):
    monkeypatch.delenv(paths.ENV_PRINTER_ROMS, raising=False)
    monkeypatch.delenv(paths.ENV_PRINTER_EPROMS, raising=False)
    assert paths.printer_roms_file() == (PROJECT_ROOT / "data" / "drucker_roms.yaml").resolve()
    assert paths.printer_eproms_dir() == (PROJECT_ROOT / "doc" / "EPROMS").resolve()
    # Umgebung hat Vorrang (Datei oder Ordner)
    (tmp_path / "drucker_roms.yaml").write_text("version: 1\n", encoding="utf-8")
    monkeypatch.setenv(paths.ENV_PRINTER_ROMS, str(tmp_path))
    assert paths.printer_roms_file() == (tmp_path / "drucker_roms.yaml").resolve()
    monkeypatch.setenv(paths.ENV_PRINTER_EPROMS, str(tmp_path))
    assert paths.printer_eproms_dir() == tmp_path.resolve()
    # Installationslayout vor Quellbaum
    monkeypatch.delenv(paths.ENV_PRINTER_ROMS)
    monkeypatch.delenv(paths.ENV_PRINTER_EPROMS)
    monkeypatch.setenv(paths.ENV_HOME, str(tmp_path))
    (tmp_path / "share" / "k1520emu" / "eproms").mkdir(parents=True)
    (tmp_path / "share" / "k1520emu" / "drucker_roms.yaml").write_text("version: 1\n", encoding="utf-8")
    assert paths.printer_roms_file() == (tmp_path / "share" / "k1520emu" / "drucker_roms.yaml").resolve()
    assert paths.printer_eproms_dir() == (tmp_path / "share" / "k1520emu" / "eproms").resolve()


def test_DruckerRomKatalog_describe_nennt_Katalog_und_Ordner():
    text = paths.describe()
    assert "Drucker-EPROMs:" in text and "drucker_roms.yaml" in text


def test_DruckerRomKatalog_defekter_Katalog_liefert_leere_Liste_und_Fehler(tmp_path):
    p = tmp_path / "x.yaml"
    p.write_text("version: 1\neintraege: []\n", encoding="utf-8")
    assert rk.laden(p) == [] and rk.fehler(p)
    p.write_text(": : :\n\t-", encoding="utf-8")
    assert rk.laden(p) == [] and rk.fehler(p)
    assert rk.laden(tmp_path / "fehlt.yaml") == []


# ── Kern: romdatei= ─────────────────────────────────────────────────────────

def _drucker(typ, konfig):
    from app.core_binding import k1520 as K
    return K._lib, K._lib.k1520_create_printer(typ, konfig.encode())


def _byte(lib, h, adr):
    return lib.k1520_mem_read(h, adr)


def test_DruckerRomKatalog_romdatei_liefert_dieselben_Bytes_wie_das_eingebettete_Abbild(eintraege):
    ordner = PROJECT_ROOT / "doc" / "EPROMS"
    # SD 1157: Datei == eingebettet (Vorgabe repariert / rom=original)
    for schluessel, alt in (("sd1157-repariert", "rom=repariert"), ("sd1157-original", "rom=original")):
        e = next(x for x in eintraege if x.schluessel == schluessel)
        lib, h_alt = _drucker(0, alt)
        _, h_neu = _drucker(0, f"romdatei={ordner / e.datei}")
        assert h_alt and h_neu
        roh = (ordner / e.datei).read_bytes()
        for adr in (0x0000, 0x0D34, 0x1234, 0x297F):
            assert _byte(lib, h_neu, adr) == _byte(lib, h_alt, adr) == roh[adr]
        lib.k1520_destroy(h_alt)
        lib.k1520_destroy(h_neu)
    # K6313: Vorgabe-Abzug aus Datei == eingebettet
    e = rk.vorgabe("k6313", eintraege)
    lib, h_alt = _drucker(3, "")
    _, h_neu = _drucker(3, f"romdatei={ordner / e.datei}")
    assert h_alt and h_neu
    roh = (ordner / e.datei).read_bytes()
    for adr in (0x0000, 0x0801, 0x1234, 0x27F8, 0x27FF):
        assert _byte(lib, h_neu, adr) == _byte(lib, h_alt, adr) == roh[adr]
    lib.k1520_destroy(h_alt)
    lib.k1520_destroy(h_neu)
    # K6311: Vorgabe-Abzug aus Datei == eingebettet (Typ 4)
    e = rk.vorgabe("k6311", eintraege)
    lib, h_alt = _drucker(4, "")
    _, h_neu = _drucker(4, f"romdatei={ordner / e.datei}")
    assert h_alt and h_neu
    roh = (ordner / e.datei).read_bytes()
    for adr in (0x0000, 0x0801, 0x1234, 0x27F7, 0x27FF):
        assert _byte(lib, h_neu, adr) == _byte(lib, h_alt, adr) == roh[adr]
    lib.k1520_destroy(h_alt)
    lib.k1520_destroy(h_neu)
    # IFSS (WP8g, Typ 5/6): K6313-Vorgabe aus Datei == eingebettet; K6311: 8-KiB-Abzug wird mit FFH aufgefuellt
    e = rk.vorgabe("k6313-ifss", eintraege)
    lib, h_alt = _drucker(5, "")
    _, h_neu = _drucker(5, f"romdatei={ordner / e.datei}")
    assert h_alt and h_neu
    roh = (ordner / e.datei).read_bytes()
    for adr in (0x0000, 0x0801, 0x1234, 0x27F8, 0x27FF):
        assert _byte(lib, h_neu, adr) == _byte(lib, h_alt, adr) == roh[adr]
    lib.k1520_destroy(h_alt)
    lib.k1520_destroy(h_neu)
    e = rk.vorgabe("k6311-ifss", eintraege)
    lib, h = _drucker(6, f"romdatei={ordner / e.datei}")
    assert h
    roh = (ordner / e.datei).read_bytes()
    assert len(roh) == 8192
    for adr in (0x0000, 0x0801, 0x1D08, 0x1FF8, 0x1FFF):
        assert _byte(lib, h, adr) == roh[adr]
    assert all(_byte(lib, h, adr) == 0xFF for adr in (0x2000, 0x2400, 0x27FF)), "fuenfter Sockel: FFH"
    lib.k1520_destroy(h)
    # WP8h: 16-KiB-Abzug (Plan "lang"): ROM bis 3FFFH, RAM-Fenster F000H statt 3800H, Typ 3 und 5
    e = next(x for x in eintraege if x.schluessel == "k6313-30835007")
    roh = (ordner / e.datei).read_bytes()
    assert len(roh) == 16384
    for typ in (3, 5):
        lib, h = _drucker(typ, f"romdatei={ordner / e.datei}")
        assert h
        for adr in (0x0000, 0x0801, 0x2C90, 0x3FF8, 0x3FFF):
            assert _byte(lib, h, adr) == roh[adr], hex(adr)
        assert _byte(lib, h, 0x4000) == 0xFF
        lib.k1520_destroy(h)
    # K6316: Satz aus Datei == eingebettet
    for typ, schluessel in ((1, "k6316-ifss"), (2, "k6316-v24")):
        e = next(x for x in eintraege if x.schluessel == schluessel)
        roh = (ordner / e.datei).read_bytes()
        lib, h_alt = _drucker(typ, "")
        _, h_neu = _drucker(typ, f"romdatei={ordner / e.datei}")
        assert h_alt and h_neu
        for adr in (0x0000, 0x0801, 0x1234, 0x27FF):
            assert _byte(lib, h_neu, adr) == _byte(lib, h_alt, adr) == roh[adr]
        lib.k1520_destroy(h_alt)
        lib.k1520_destroy(h_neu)


def test_DruckerRomKatalog_romdatei_hat_Vorrang_und_laeuft_wirklich_aus_der_Datei(tmp_path):
    ordner = PROJECT_ROOT / "doc" / "EPROMS"
    roh = bytearray((ordner / "SD1157" / "1157_repariert.rom").read_bytes())
    roh[0x0D34] = 0x77
    p = tmp_path / "eigen.rom"
    p.write_bytes(bytes(roh))
    lib, h = _drucker(0, f"rom=original,romdatei={p}")
    assert h and _byte(lib, h, 0x0D34) == 0x77
    lib.k1520_destroy(h)


@pytest.mark.parametrize("typ, groesse, fragment", [(0, 100, "100 Bytes, erwartet 10624"),
                                                    (1, 10624, "10624 Bytes, erwartet 10240"),
                                                    (2, 0, "nicht lesbar oder leer"),
                                                    (3, 100, "100 Bytes, erwartet 10240"),
                                                    (5, 8192, "8192 Bytes, erwartet 10240"),    # K6313: nur der K6311 darf 8 KiB
                                                    (6, 100, "100 Bytes, erwartet 10240")])
def test_DruckerRomKatalog_romdatei_falsche_Laenge_klare_Meldung(tmp_path, typ, groesse, fragment):
    from app.core_binding import k1520 as K
    p = tmp_path / "falsch.rom"
    p.write_bytes(b"\xff" * groesse)
    assert not K._lib.k1520_create_printer(typ, f"romdatei={p}".encode())
    assert fragment in K._lib.k1520_last_init_error().decode()
    assert not K._lib.k1520_create_printer(typ, f"romdatei={tmp_path / 'fehlt.rom'}".encode())
    assert "nicht lesbar" in K._lib.k1520_last_init_error().decode()
