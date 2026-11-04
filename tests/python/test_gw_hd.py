"""HD-Disketten am Greaseweazle (Entwurf 45, AP H2) -- ohne Hardware.

Rate und Drehzahl der DISKETTE sind getrennt von der Drehzahl des LAUFWERKS: ein fest
auf 300 U/min gestelltes TEAC FD-55GFR liest eine 1,2M-Diskette (500 kbit/s bei
360 U/min) mit 416,7 kbit/s.  Das Abbild bleibt in der Zeitbasis der Diskette.
"""

from __future__ import annotations

import struct

import pytest

from gw_fake import HfeDevice


def test_pll_takt_folgt_der_diskette_und_dem_laufwerk():
    from app.gw.device import Device

    gleich = Device(None, None, cell_rate_kbps=250)
    assert gleich.pll_takt() == pytest.approx(2e-6)
    # Alter Weg (blosse Zahl) bleibt bitgleich.
    assert Device(None, None, cell_rate_kbps=500).pll_takt() == pytest.approx(1e-6)
    # 1,2M im 300-U/min-Laufwerk: Zellen um 360/300 gedehnt.
    teac = Device(None, None, cell_rate_kbps=500, disk_rpm=360, drive_rpm=300)
    assert teac.pll_takt() == pytest.approx(1.2e-6)
    # Zellen je Umdrehung des Laufwerks (200 ms) = Zellen der Diskette (166 667).
    assert 0.2 / teac.pll_takt() == pytest.approx(500 * 2000 * 60 / 360, rel=1e-6)


def test_aufzeichnungen_tragen_drei_angaben():
    from app.gw import AUFZEICHNUNGEN, RATEN, aufzeichnung

    assert [a.schluessel for a in AUFZEICHNUNGEN] == [k for k, _ in RATEN]
    teac = aufzeichnung("417")
    assert (teac.rate, teac.disk_rpm, teac.drive_rpm) == (500, 360, 300)
    assert "TEAC" in teac.text
    # Die alten Eintraege behalten ihre Zahl als Schluessel.
    assert aufzeichnung(250).rate == 250
    assert aufzeichnung(500).rate == 500 and aufzeichnung(500).disk_rpm == 360


@pytest.fixture(scope="module")
def qt_app(qapp):
    return qapp


def test_dialog_bietet_die_eintraege_und_liefert_drei_angaben(qt_app):
    pytest.importorskip("PySide6")
    from app.ui.physical_disk import PhysicalDiskDialog

    d = PhysicalDiskDialog()
    texte = [d._rate.itemText(i) for i in range(d._rate.count())]
    assert any("417 kbit/s" in t and "TEAC FD-55GFR" in t for t in texte)
    assert any("500 kbit/s" in t and "IBM-PC" in t for t in texte)
    assert not any("und HD" in t for t in texte)
    d._rate.setCurrentIndex(d._rate.findText(next(t for t in texte if "TEAC" in t)))
    w = d.auswahl()
    assert (w["cell_rate_kbps"], w["rpm"], w["drive_rpm"]) == (500, 360, 300)
    # Vorgabe: 250/300/300 wie bisher.
    w0 = PhysicalDiskDialog().auswahl()
    assert (w0["cell_rate_kbps"], w0["rpm"], w0["drive_rpm"]) == (250, 300, 300)


def test_emulator_bietet_kein_5_25_hd(qt_app):
    pytest.importorskip("PySide6")
    from app.ui.physical_disk import PhysicalDiskDialog

    d = PhysicalDiskDialog(fuer_emulator=True)
    texte = [d._rate.itemText(i) for i in range(d._rate.count())]
    assert len(texte) == 2 and not any("HD" in t for t in texte)


def _hd_hfe(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    pfad = tmp_path / "hd.hfe"
    with DiskTool.create(pfad, "dos_ds80_15x512_hd", "HDPROBE") as d:
        d.flush()
    return pfad


def test_save_as_einer_hd_sitzung_traegt_kopf_500(tmp_path, monkeypatch, capsys):
    """Das gesicherte Abbild einer 500/360-Sitzung ist eine HD-HFE (Kopf 500)."""
    from app.disktool import physical_cli
    from app.gw import PhysicalSession, Sync, TrackWorker

    quelle = _hd_hfe(tmp_path)
    assert struct.unpack_from("<H", quelle.read_bytes(), 12)[0] == 500
    geraet = HfeDevice(quelle)
    gesehen = {}

    def start(*, drive="a", cell_rate_kbps=250, num_cyls=80, num_heads=2,
              writable=False, read_ahead=True, for_emulator=False, rpm=300,
              verify_writes=True, double_step=False, drive_rpm=0):
        gesehen.update(rate=cell_rate_kbps, rpm=rpm, drive_rpm=drive_rpm)
        sync = Sync(num_cyls=geraet.num_cyls, num_heads=geraet.num_heads,
                    cell_rate_kbps=cell_rate_kbps, rpm=rpm, writable=writable,
                    read_ahead=read_ahead, verify_writes=verify_writes)
        worker = TrackWorker(sync, geraet)
        worker.start()
        return PhysicalSession(sync, worker, geraet, drive=drive, writable=writable,
                               cell_rate_kbps=cell_rate_kbps,
                               num_cyls=geraet.num_cyls, num_heads=geraet.num_heads,
                               rpm=rpm, drive_rpm=drive_rpm)

    monkeypatch.setattr(PhysicalSession, "start", staticmethod(start))
    ziel = tmp_path / "sicherung.hfe"
    rc = physical_cli.main(["--physical", "--rate", "500", "--rpm", "360",
                            "--drive-rpm", "300", "--cyls", str(geraet.num_cyls),
                            "save-as", str(ziel), "-q"])
    assert rc == 0, capsys.readouterr().err
    assert gesehen == {"rate": 500, "rpm": 360, "drive_rpm": 300}
    kopf = ziel.read_bytes()
    assert struct.unpack_from("<H", kopf, 12)[0] == 500
    # gleiche Groesse wie die Vorlage (nicht doppelt wie bei Kopf 250/300)
    assert abs(ziel.stat().st_size - quelle.stat().st_size) < 0.02 * quelle.stat().st_size
