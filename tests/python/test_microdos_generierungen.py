"""MicroDOS 1526: MSYSG20 mit abweichender Hardware generieren, booten, benutzen (Merkposten microdos.md, "Generierungen").

Jede Diskette entsteht wie beim Anwender (FORMAT20 -> MSYSG20 auf einer Leerdiskette, ablauf.bauen).  Geprueft wird, dass das
erzeugte System in der passenden Emulatorbestueckung bootet und -- wo der Emulator die Gegenstelle kennt -- dass der Weg
bis nach aussen funktioniert (Drucker ueber die serielle Schnittstelle in eine Datei, Zusatzkanal in der Schleife).
"""
import os
import shutil
import subprocess
import sys
import time

import pytest

from conftest import PROJECT_ROOT, requires_core

pytestmark = requires_core

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf  # noqa: E402

DISKTOOL = PROJECT_ROOT / "build" / "k1520disktool"
braucht_disktool = pytest.mark.skipif(not DISKTOOL.exists(), reason="build/k1520disktool fehlt")

# LD C,4 / LD E,'X' / CALL 5 (Punch) ; LD C,3 / CALL 5 (Reader) ; LD E,A / LD C,2 / CALL 5 (Konsole) ; RET
AUXT = bytes.fromhex("0E041E58CD05000E03CD05005F0E02CD0500C9")


def _bauen(tmp_path, name, **gen):
    return ablauf.bauen(str(tmp_path), tastatur="k7637", probe=False, generierung=gen, dateiname=name + ".hfe")


def _boot(tmp_path, pfad, tastatur="k7637", dateien=(), **kw):
    a = tmp_path / "boot.hfe"
    shutil.copy(pfad, a)
    for d in dateien:
        env = dict(os.environ, LD_LIBRARY_PATH=str(PROJECT_ROOT / "build"))
        r = subprocess.run([str(DISKTOOL), "put", str(a), str(d)], capture_output=True, env=env)
        assert r.returncode == 0, r.stdout + r.stderr
    s = ablauf.Sitzung(tastatur, str(a), **kw)
    s.kaltstart_bis_prompt()
    return s


def _ctrl(s, buchstabe):
    code = ord(buchstabe)
    s.emu.key_press(code, False, True)
    s.lauf(150_000)
    s.emu.key_release(code)
    s.lauf(600_000)


def _seriell(s, tmp_path, schleife=()):
    """Alle Schnittstellen in Dateien (``schleife``: diese Indizes in die Schleife)."""
    from app.core_binding import k1520
    pfade = []
    for i in range(s.emu.serial_count()):
        s.emu.serial_stop(i)
    for i in range(s.emu.serial_count()):
        p = tmp_path / ("ser%d.txt" % i)
        p.write_bytes(b"")
        if i in schleife:
            assert s.emu.serial_configure(i, loop=True)       # Schleife = keine Verbindung, kein Start
        else:
            assert s.emu.serial_configure(i, betriebsart=k1520.SER_DATEI, datei=str(p), loop=False)
            assert s.emu.serial_start(i)
        pfade.append(p)
    return pfade


def _lesen(pfad, mindestens=1, frist=3.0):
    ende = time.time() + frist
    while time.time() < ende:
        if len(pfad.read_bytes()) >= mindestens:
            time.sleep(0.2)
            break
        time.sleep(0.05)
    return pfad.read_bytes()


def _drucken(s):
    _ctrl(s, "p")
    s.lauf(3_000_000)
    s.kommando("D")
    _ctrl(s, "p")
    s.lauf(20_000_000)


# ── Drucker: IFSS / V.24 / PIO ───────────────────────────────────────────────

def test_drucker_v24_generiert_geht_an_dfue_v24_und_nicht_an_den_ifss_kanal(tmp_path):
    pfad = _bauen(tmp_path, "v24", drucker="2")
    s = _boot(tmp_path, pfad)
    ser = _seriell(s, tmp_path)
    _drucken(s)
    daten = _lesen(ser[0], 30)
    assert daten.startswith(b"D\r") and b"MicroDOS.SYS" in daten, daten
    assert not _lesen(ser[2], 1, 0.3) and not ser[1].read_bytes()


def test_drucker_pio_generiert_bootet_aber_pio_hat_keine_gegenstelle(tmp_path):
    """NICHT MODELLIERT: der PIO-Treiber schreibt auf E/A 54H..57H (= PIO A31 der K8025, DIL-Schalter-Register); die Z80PIO
    nimmt die Bytes an, es gibt keinen Druckeranschluss nach aussen.  Das System laeuft, gedruckt wird nichts."""
    pfad = _bauen(tmp_path, "pio", drucker="3", zusatzkanal="3")
    s = _boot(tmp_path, pfad)
    ser = _seriell(s, tmp_path)
    _drucken(s)
    assert all(not _lesen(p, 1, 0.3) for p in ser)
    assert "MicroDOS.SYS" in s.kommando("D")


# ── Zusatzkanal: V.24 / IFSS / IFSP ──────────────────────────────────────────

@braucht_disktool
def test_zusatzkanal_v24_sendet_und_empfaengt_in_der_schleife(tmp_path):
    """BDOS 4 (Punch) 'X' und BDOS 3 (Reader) zurueck: in der Schleife der Schnittstelle DFUE/V.24 kommt das X wieder an."""
    pfad = _bauen(tmp_path, "aux_v24", drucker="3", zusatzkanal="3")
    auxt = tmp_path / "AUXT.COM"
    auxt.write_bytes(AUXT)
    s = _boot(tmp_path, pfad, dateien=[auxt])
    _seriell(s, tmp_path, schleife=(0,))
    s.tippe("AUXT\r")
    s.lauf(60_000_000)
    assert s.bild().split("\n")[-2:] == ["X", "A>"], s.bild()


@braucht_disktool
def test_zusatzkanal_v24_sendet_in_eine_datei(tmp_path):
    pfad = _bauen(tmp_path, "aux_v24d", drucker="3", zusatzkanal="3")
    auxt = tmp_path / "AUXT.COM"
    auxt.write_bytes(AUXT)
    s = _boot(tmp_path, pfad, dateien=[auxt])
    ser = _seriell(s, tmp_path)
    s.tippe("AUXT\r")
    s.lauf(60_000_000)
    assert _lesen(ser[0], 1) == b"X"


@braucht_disktool
def test_zusatzkanal_ifsp_alb_k6025_ist_nicht_modelliert(tmp_path):
    """NICHT MODELLIERT: IFSP geht ueber die ALB K6025 (E/A 90H..97H), im Emulator unbelegt -- das X (OUT 95H) verpufft."""
    pfad = _bauen(tmp_path, "aux_ifsp", drucker="1", zusatzkanal="2")
    auxt = tmp_path / "AUXT.COM"
    auxt.write_bytes(AUXT)
    s = _boot(tmp_path, pfad, dateien=[auxt])
    ser = _seriell(s, tmp_path)
    s.tippe("AUXT\r")
    s.lauf(60_000_000)
    assert all(not _lesen(p, 1, 0.3) for p in ser)


@braucht_disktool
def test_zusatzkanal_ifss_sendet_auf_a32_kanal_a_das_ist_mit_der_k7637_die_tastatur(tmp_path):
    """GEKLAERT (2026-11-01): der Zusatzkanal-IFSS-Treiber schreibt auf 5CH = SIO A32 Kanal A (Stecker X4).  Mit der K7637
    haengt dort die Tastatur (A5120 mit K7637-System): das X geht an die Tastatur, nicht nach aussen -- am Geraet genauso.
    Der Drucker (IFSS) liegt auf Kanal B (5EH) und geht."""
    pfad = _bauen(tmp_path, "aux_ifss", drucker="1", zusatzkanal="1")
    auxt = tmp_path / "AUXT.COM"
    auxt.write_bytes(AUXT)
    s = _boot(tmp_path, pfad, dateien=[auxt])
    ser = _seriell(s, tmp_path)
    assert len(ser) == 3                                   # mit der K7637 gibt es keinen vierten Anschluss
    s.tippe("AUXT\r")
    s.lauf(60_000_000)
    assert all(not _lesen(p, 1, 0.3) for p in ser)
    _drucken(s)
    assert _lesen(ser[2], 20).startswith(b"D\r")


@braucht_disktool
def test_zusatzkanal_ifss_mit_k7636_geht_an_den_freien_stecker_x4(tmp_path):
    """Mit der K7636 am U-Bus ist A32 Kanal A frei: die Maschine fuehrt ihn als vierten Anschluss 'IFSS Zusatzkanal' (X4)
    heraus, das X des Zusatzkanals kommt dort an."""
    pfad = ablauf.bauen(str(tmp_path), tastatur="k7637", probe=False, dateiname="aux7636.hfe",
                        generierung=dict(tastatur="k7636", drucker="1", zusatzkanal="1"))
    auxt = tmp_path / "AUXT.COM"
    auxt.write_bytes(AUXT)
    s = _boot(tmp_path, pfad, tastatur="k7636", dateien=[auxt])
    ser = _seriell(s, tmp_path)
    assert len(ser) == 4
    s.tippe("AUXT\r")
    s.lauf(60_000_000)
    assert _lesen(ser[3], 1) == b"X"
    assert all(not _lesen(p, 1, 0.3) for p in ser[:3])
