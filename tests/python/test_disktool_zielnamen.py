"""Zielnamen des Auszugs (DiskVolume::zielNamen) auf allen Wegen.

CP/M-Disketten aus WEGA-Uebertragungen tragen Namen mit '/' (`/Z/HOFFM`,
`GW/ZSID.DOC`).  Der Auszug darf weder den Zielordner verlassen noch zwei Dateien
auf denselben Namen legen.  Geprueft werden die Oberflaeche (`extract_refs`, auch der
Weg des Ziehens aus der Diskettenliste), die Kommandozeile (`get` mit und ohne
Muster) und der Eintrag der C-ABI (`Entry.target_name`).
"""

import shutil
import subprocess
from pathlib import Path

import pytest

from conftest import PROJECT_ROOT, requires_disk

pytestmark = requires_disk

pytest.importorskip("PySide6")

# (Platzhalter beim Anlegen, Name auf der Diskette)
NAMEN = [("P1", "/Z/HOFFM"), ("P2", "/TMP/X"), ("P3", "A/B"),
         ("P4", "A_B"), ("P5", ".HID"), ("P6", "OK.TXT")]
ERWARTET = {"_Z_HOFFM", "_TMP_X", "A_B", "A_B~1", "_.HID", "OK.TXT"}


def _feld(name: str) -> bytes:
    nm, _, ex = name.partition(".") if not name.startswith(".") else (name, "", "")
    return (nm.ljust(8) + ex.ljust(3)).encode("latin-1")


@pytest.fixture
def abbild(fixture_disks, tmp_path) -> Path:
    """CP/M-Abbild mit Verzeichnisnamen, die `insert` (zu Recht) ablehnt."""
    from app.core_binding.k1520disk import DiskTool
    pfad = tmp_path / "schraeg.img"
    shutil.copy(fixture_disks / "cpa_cpa780_k5601_noclock.img", pfad)
    quelle = tmp_path / "inhalt.bin"
    quelle.write_bytes(b"x" * 300)
    with DiskTool.open(pfad, read_only=False) as d:
        for platzhalter, _ in NAMEN:
            d.insert(quelle, platzhalter)
        d.flush()
    roh = bytearray(pfad.read_bytes())
    for platzhalter, name in NAMEN:
        alt = b"\x00" + _feld(platzhalter)
        pos = bytes(roh).find(alt)
        assert pos >= 0, platzhalter
        roh[pos + 1:pos + 12] = _feld(name)
    pfad.write_bytes(bytes(roh))
    return pfad


def _nur_ziel(wurzel: Path, ziel: Path) -> set:
    """Alles ausserhalb des Zielordners muss unberuehrt sein; liefert die Namen darin."""
    assert {p.name for p in wurzel.iterdir() if p != ziel and p.name != ziel.name} <= {
        "schraeg.img", "inhalt.bin", "schraeg.img~"}, "etwas ausserhalb des Zielordners"
    dateien = set()
    for p in ziel.rglob("*"):
        assert p.is_file() and p.parent == ziel, f"Unterordner/Ausbruch: {p}"
        dateien.add(p.name)
    return dateien


def test_target_name_der_c_abi(abbild):
    from app.core_binding.k1520disk import DiskTool
    with DiskTool.open(abbild) as d:
        ziele = {e.name: e.target_name for e in d.list_names()}
    assert ziele["/Z/HOFFM"] == "_Z_HOFFM"
    assert ziele["A_B"] == "A_B", "wer nicht umgeschrieben wird, behaelt seinen Namen"
    assert ziele["A/B"] == "A_B~1"
    assert ziele["OK.TXT"] == "OK.TXT"


def test_extract_refs_waehlt_die_namen_des_kerns(abbild, tmp_path):
    from PySide6.QtWidgets import QApplication
    app = QApplication.instance() or QApplication([])           # noqa: F841
    from app.disktool.ui.main_window import MainWindow
    w = MainWindow()
    try:
        assert w.open_image(abbild)
        ziel = tmp_path / "gui" / "ziel"
        refs = [e.ref for e in w.tool.list_names()]
        assert w.extract_refs(refs, ziel)
        assert _nur_ziel(tmp_path / "gui", ziel) >= ERWARTET
        # Zwei Dateien duerfen nicht auf 'HOFFM'/'B' verkuerzt einander ueberschreiben.
        assert not (ziel / "HOFFM").exists() and not (ziel / "B").exists()
    finally:
        w._close_tool()


CLI = PROJECT_ROOT / "build" / "k1520disktool"


@pytest.mark.skipif(not CLI.exists(), reason="k1520disktool nicht gebaut")
@pytest.mark.parametrize("muster", [["*"], []])
def test_cli_get_bleibt_im_zielordner(abbild, tmp_path, muster):
    wurzel = tmp_path / ("cli_" + ("m" if muster else "o"))
    ziel = wurzel / "ziel"
    wurzel.mkdir()
    r = subprocess.run([str(CLI), "get", str(abbild), *muster, "--to", str(ziel)],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    assert [p.name for p in wurzel.iterdir()] == ["ziel"], "etwas ausserhalb des Zielordners"
    assert ERWARTET <= _nur_ziel(wurzel, ziel)
    assert "kollidiert" in r.stderr, "die Umbenennung gehoert ins Protokoll"
