"""Archiv eines tar-SATZES (Entwurf 37, AP T5): alle Abbilder + alle Dateien + Metadaten.

Geprueft wird der Rundlauf: Satz anlegen/speichern -> archivieren -> Abbilder aus dem Archiv
zurueckholen -> derselbe Satz (Abbilder bitgleich, Dateien gleich).  Dazu die Regeln drumherum:
ein ungespeicherter Satz wird nicht halb archiviert, ein beschaedigtes Archiv nicht entpackt.
"""

import hashlib
import zipfile

import pytest
import yaml

from conftest import requires_disk

pytestmark = requires_disk

FORMAT = "k5601_ss40_16x256"          # 163840 Nutzbytes je Abbild


def _bytes(n, start=1):
    return bytes((start + i * 7) & 0xFF for i in range(n))


@pytest.fixture
def satz(tmp_path):
    """Gespeicherter Satz aus zwei Abbildern: eine grosse Datei, eine kleine, ein Unterordner."""
    from app.core_binding.k1520disk import DiskTool
    q = tmp_path / "q"
    (q / "unter").mkdir(parents=True)
    gross = _bytes(220000, 9)
    (q / "gross.bin").write_bytes(gross)
    (q / "unter" / "b.txt").write_bytes(b"zweite datei\n")
    with DiskTool.tar_satz_neu(FORMAT, tmp_path / "satz.hfe") as t:
        t.insert(str(q / "gross.bin"), "gross.bin")
        t.insert(str(q / "unter" / "b.txt"), "unter/b.txt")
        r = t.tar_satz_speichern()
    assert len(r["geschrieben"]) == 2
    return r["geschrieben"], gross


def _archiviere(namen, ziel, **kw):
    from app.core_binding.k1520disk import DiskTool
    from app.disktool.archive import create_archive
    with DiskTool.open_tar_satz(namen) as d:
        assert d.tar_satz_dateien == [str(n) for n in namen]
        return create_archive(d, ziel, satz_abbilder=d.tar_satz_dateien, **kw)


def test_archiv_enthaelt_abbilder_dateien_und_metainformationen(satz, tmp_path):
    namen, gross = satz
    ziel = _archiviere(namen, tmp_path / "a.zip", bezeichnung="mein satz")
    with zipfile.ZipFile(ziel) as z:
        n = set(z.namelist())
        # ALLE Abbilder, bitgleich, in nummerierten Namen
        for pfad in namen:
            assert z.read(f"abbilder/{pfad.rsplit('/', 1)[-1]}") == open(pfad, "rb").read()
        # ALLE Dateien mit Ordnerstruktur, dazu die .fileinfo (fs=tar) je Datei
        assert z.read("dateien/gross.bin") == gross
        assert z.read("dateien/unter/b.txt") == b"zweite datei\n"
        info = [x for x in n if x.startswith("dateien/") and x.endswith(".fileinfo")]
        assert info, sorted(n)
        assert any("fs=tar" in z.read(i).decode("utf-8") or "fs: tar" in z.read(i).decode("utf-8")
                   for i in info)
        # Satzangaben: Basisname, Format, Reihenfolge, Pruefsummen
        daten = yaml.safe_load(z.read("diskarchive.yaml").decode("utf-8"))
        assert "image" not in daten
        ts = daten["tar_satz"]
        assert ts["basename"] == "satz.hfe" and ts["format"] == FORMAT and ts["container"] == "hfe"
        assert [b["index"] for b in ts["images"]] == [1, 2]
        assert [b["path"] for b in ts["images"]] == ["abbilder/satz1.hfe", "abbilder/satz2.hfe"]
        for b in ts["images"]:
            assert hashlib.sha256(z.read(b["path"])).hexdigest() == b["sha256"]
        big = next(f for f in daten["files"] if f["name"] == "gross.bin")
        assert big["sha256"] == hashlib.sha256(gross).hexdigest() and big["unix"]
        txt = z.read("mein_satz.txt").decode("utf-8")
    assert "tar-Satz" in txt and "1. satz1.hfe" in txt and "2. satz2.hfe" in txt


def test_rundlauf_archiv_schreiben_lesen_satz_identisch(satz, tmp_path):
    from app.core_binding.k1520disk import DiskTool
    from app.disktool.archive import satz_entpacken
    namen, gross = satz
    ziel = _archiviere(namen, tmp_path / "a.zip")
    neu = satz_entpacken(ziel, tmp_path / "zurueck")
    assert [p.name for p in neu] == ["satz1.hfe", "satz2.hfe"]
    for alt, n in zip(namen, neu):
        assert open(alt, "rb").read() == n.read_bytes()
    with DiskTool.open_tar_satz(neu) as a, DiskTool.open_tar_satz(namen) as b:
        la = sorted((e.name, e.size) for e in a.list())
        assert la == sorted((e.name, e.size) for e in b.list())
        a.extract_all(tmp_path / "ea")
    assert (tmp_path / "ea" / "gross.bin").read_bytes() == gross


def test_entpacken_ueberschreibt_nicht_und_prueft(satz, tmp_path):
    from app.disktool.archive import satz_entpacken
    namen, _ = satz
    ziel = _archiviere(namen, tmp_path / "a.zip")
    ordner = tmp_path / "z"
    satz_entpacken(ziel, ordner)
    with pytest.raises(ValueError, match="Schon vorhanden"):
        satz_entpacken(ziel, ordner)
    assert len(satz_entpacken(ziel, ordner, ueberschreiben=True)) == 2
    # beschaedigtes Abbild im Archiv: Pruefsumme schlaegt an, es wird NICHTS geschrieben
    kaputt = tmp_path / "k.zip"
    with zipfile.ZipFile(ziel) as zi, zipfile.ZipFile(kaputt, "w") as zo:
        for i in zi.infolist():
            d = zi.read(i.filename)
            if i.filename == "abbilder/satz2.hfe":
                d = d[:-1] + bytes([d[-1] ^ 1])
            zo.writestr(i.filename, d)
    with pytest.raises(ValueError, match="Pruefsumme"):
        satz_entpacken(kaputt, tmp_path / "leer")
    assert not (tmp_path / "leer").exists()
    # Archiv einer gewoehnlichen Diskette ist kein Satzarchiv
    einfach = tmp_path / "e.zip"
    with zipfile.ZipFile(einfach, "w") as z:
        z.writestr("diskarchive.yaml", "diskarchive: 1\n")
    with pytest.raises(ValueError, match="keinen tar-Satz"):
        satz_entpacken(einfach, tmp_path / "x")


def test_ungespeicherter_satz_wird_nicht_halb_archiviert(satz, tmp_path):
    from app.core_binding.k1520disk import DiskTool
    from app.disktool.archive import create_archive
    namen, _ = satz
    with DiskTool.open_tar_satz(namen) as d:
        d.set_read_only(False)
        d.mkdir("neu")
        assert d.dirty and d.tar_satz_dateien == []
        with pytest.raises(ValueError, match="erst speichern"):
            create_archive(d, tmp_path / "x.zip", satz_abbilder=list(namen))
        assert not (tmp_path / "x.zip").exists()
        d.tar_satz_speichern()
        assert d.tar_satz_dateien == [str(n) for n in namen]


def test_cli_satz_archivieren_und_entpacken(satz, tmp_path, capsys):
    from app.disktool import archive
    namen, _ = satz
    ziel = tmp_path / "cli.zip"
    assert archive.main([namen[0], str(ziel), "--satz", namen[1]]) == 0
    with zipfile.ZipFile(ziel) as z:
        assert {"abbilder/satz1.hfe", "abbilder/satz2.hfe"} <= set(z.namelist())
    assert archive.main([str(ziel), "--satz-entpacken", str(tmp_path / "o")]) == 0
    assert (tmp_path / "o" / "satz2.hfe").is_file()
    assert archive.main([str(ziel), "--satz-entpacken", str(tmp_path / "o")]) == 1   # vorhanden
    assert "Schon vorhanden" in capsys.readouterr().err


# ─── Oberflaeche ─────────────────────────────────────────────────────────────

@pytest.fixture(scope="module")
def qt_app():
    pytest.importorskip("PySide6")
    from PySide6.QtWidgets import QApplication
    yield QApplication.instance() or QApplication([])


@pytest.fixture
def window(qt_app):
    from app.disktool.ui.main_window import MainWindow
    w = MainWindow()
    yield w
    w._close_tool()


def test_gui_archiviert_gespeicherten_satz_ohne_rueckfrage(window, satz, tmp_path, monkeypatch):
    namen, gross = satz
    assert window.open_image(namen[0], satz=namen)
    assert window.act_archivieren.isEnabled()                # nicht gesperrt (Entwurf 37, T5)
    monkeypatch.setattr(window, "_frage_satz_vor_archiv",
                        lambda: (_ for _ in ()).throw(AssertionError("unerwartete Rueckfrage")))
    ziel = tmp_path / "g.zip"
    assert window.archive(ziel, bezeichnung="gui")
    with zipfile.ZipFile(ziel) as z:
        assert z.read("dateien/gross.bin") == gross
        assert "abbilder/satz1.hfe" in z.namelist() and "abbilder/satz2.hfe" in z.namelist()


def test_gui_geaenderter_satz_wird_nach_rueckfrage_gespeichert(window, satz, tmp_path, monkeypatch):
    namen, _ = satz
    assert window.open_image(namen[0], satz=namen)
    window.act_schreibschutz.setChecked(False)
    assert window.tar_mkdir("neuer/ordner")
    assert window.tool.dirty
    fragen = []
    monkeypatch.setattr(window, "_frage_satz_vor_archiv", lambda: fragen.append(1) or False)
    assert not window.archive(tmp_path / "nein.zip")          # abgelehnt: nichts passiert
    assert fragen and not (tmp_path / "nein.zip").exists() and window.tool.dirty
    monkeypatch.setattr(window, "_frage_satz_vor_archiv", lambda: True)
    # das Speichern fragt wie immer (Plan-Dialog); "" = der vorgesehene Basisname
    monkeypatch.setattr(window, "_satz_speichern_fragen", lambda *a, **k: "")
    ziel = tmp_path / "ja.zip"
    assert window.archive(ziel)
    assert not window.tool.dirty
    with zipfile.ZipFile(ziel) as z:
        assert any(n.startswith("dateien/neuer/ordner") for n in z.namelist())
        daten = yaml.safe_load(z.read("diskarchive.yaml").decode("utf-8"))
        # die Abbilder im Archiv sind die eben gespeicherten Dateien
        for b in daten["tar_satz"]["images"]:
            n = b["path"].rsplit("/", 1)[-1]
            assert z.read(b["path"]) == open(tmp_path / n, "rb").read()
