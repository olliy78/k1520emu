"""Motor und Index beim Schreiben einer physischen Diskette — ohne Hardware, ohne Paket.

Issue #13: beim Formatieren meldete der Adapter „kein Indexpuls", die Spur wurde nicht
geschrieben; lief nebenher das Hintergrundlesen, ging es besser.  Der Schreibpfad muss
den Motor selbst sicher anfahren und bei fehlendem Index neu anfahren.  Das
``greaseweazle``-Paket wird nicht gebraucht: der Adapter ist ein Ersatz, der
Schreibvorgang selbst (``_write_track``) wird ersetzt.
"""

from __future__ import annotations

import pytest

from app.gw import device as dev


class _Bus:
    value = "ibm"


class _Laufwerk:
    unit_id = 0
    bus = _Bus()


class _Adapter:
    def __init__(self):
        self.befehle = []

    def drive_motor(self, unit, an):
        self.befehle.append(("motor", an))

    def set_bus_type(self, v):
        self.befehle.append(("bus", v))

    def drive_select(self, u):
        self.befehle.append(("select", u))

    def drive_deselect(self):
        self.befehle.append(("deselect",))


def _geraet():
    usb = _Adapter()
    d = dev.Device(usb, _Laufwerk())
    pausen = []
    d._schlafen = pausen.append
    return d, usb, pausen


def test_motoranlauf_wartet_und_beginnt_den_nachlauf_neu():
    d, usb, pausen = _geraet()
    d._letzte_arbeit = 1.0                       # uralter Wert von vor dem Abschalten
    d.motor(True)
    assert pausen == [dev._ANLAUF_S]
    assert d._letzte_arbeit > 1.0                # sonst liefe er bei der ersten Pause aus


def test_fehlender_index_beim_schreiben_faehrt_den_motor_neu_an(monkeypatch):
    d, usb, pausen = _geraet()
    versuche = []

    def roh(cyl, head, cells, bitcells):
        versuche.append(1)
        if len(versuche) < 3:
            raise RuntimeError("GetFluxStatus: No Index")

    monkeypatch.setattr(d, "_write_track", roh)
    d.write_track(5, 0, b"", 0)

    assert len(versuche) == 3
    # je Fehlschlag: Motor aus, dann an, dann die lange Anlaufzeit abwarten
    assert usb.befehle == [("motor", False), ("motor", True)] * 2
    assert pausen.count(dev._ANLAUF_NACH_FEHLER_S) == 2


def test_ein_dauerhaft_fehlender_index_wird_nach_den_versuchen_gemeldet(monkeypatch):
    d, usb, pausen = _geraet()
    versuche = []

    def roh(cyl, head, cells, bitcells):
        versuche.append(1)
        raise RuntimeError("GetFluxStatus: No Index")

    monkeypatch.setattr(d, "_write_track", roh)
    with pytest.raises(dev.LaufwerkMeldet):      # mit Deutung, nicht endlos
        d.write_track(5, 0, b"", 0)
    assert len(versuche) == dev._INDEX_WIEDERHOLUNGEN + 1


def test_andere_fehler_werden_nicht_wiederholt(monkeypatch):
    d, usb, pausen = _geraet()
    versuche = []

    def roh(cyl, head, cells, bitcells):
        versuche.append(1)
        raise RuntimeError("GetFluxStatus: Write Protected")

    monkeypatch.setattr(d, "_write_track", roh)
    with pytest.raises(dev.LaufwerkMeldet):
        d.write_track(5, 0, b"", 0)
    assert len(versuche) == 1
    assert usb.befehle == []


def test_lesen_wiederholt_bei_fehlendem_index_ebenfalls(monkeypatch):
    d, usb, pausen = _geraet()
    versuche = []

    def roh(cyl, head):
        versuche.append(1)
        if len(versuche) == 1:
            raise RuntimeError("GetFluxStatus: No Index")
        return b"x", 8

    monkeypatch.setattr(d, "_read_track", roh)
    assert d.read_track(1, 0) == (b"x", 8)
    assert len(versuche) == 2
