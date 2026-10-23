"""Waechter fuer tools/mutos8000/ (Entwurf 39, 4.7.6): die Auswertung von M8KROOT bleibt lauffaehig
und liefert den dokumentierten Befund.  Nur Standardbibliothek, kein DiskTool, wenige Sekunden."""

import subprocess
import sys

from conftest import PROJECT_ROOT

TOOLS = PROJECT_ROOT / "tools" / "mutos8000"


def lauf(skript, *args):
    r = subprocess.run([sys.executable, "-I", str(TOOLS / skript), *args],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    return r.stdout


def test_m8kroot_befund_stimmt_mit_dem_entwurf_ueberein():
    out = lauf("m8kroot_lesen.py")
    assert "isize 15 fsize 250" in out
    assert "inodes belegt 45" in out
    assert "root mode 40777" in out
    assert "/etc/passwd" in out and "VOLL" in out


def test_skripte_zeigen_ihre_hilfe():
    for s in ("m8kroot_lesen.py", "wurzel_quellbaum.py", "wurzel_pruefen.py", "bau_wurzel.py"):
        assert "usage" in lauf(s, "--help")
