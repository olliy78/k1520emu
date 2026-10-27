"""Waechter fuer z8kdis am echten Abzug (Entwurf 41, AP K3): der Kernel `mutos_1216` (36352 Byte,
Ladeadresse 0) aus dem Ladedisketten-Archiv wird mit der eingecheckten Karte
`tools/mutos8000/kernel/karte.txt` disassembliert; der Rundlauf (Disassemblat -> z8kasm -> Bytes)
muss das Abbild bitgleich zurueckgeben, auch mit allen Code-Vorschlaegen als Einstiegen.

Der Rundlauf gegen den Gast-`as` (Dialekt des MUTOS-as) steht noch aus -- hier prueft nur die eigene
Befehlstabelle (doc/mutos_kernel/as_dialekt.md)."""

import re
import subprocess
import zipfile

import pytest

from conftest import PROJECT_ROOT

ARCHIV = PROJECT_ROOT / "disks" / "a5120_mutos8000" / "a5120_mutos8000_boot_udos.zip"
KARTE = PROJECT_ROOT / "tools" / "mutos8000" / "kernel" / "karte.txt"
Z8KDIS = PROJECT_ROOT / "build" / "z8kdis"

pytestmark = pytest.mark.skipif(not Z8KDIS.exists() or not ARCHIV.exists(),
                                reason="z8kdis nicht gebaut oder Ladedisketten-Archiv fehlt")


@pytest.fixture(scope="module")
def kernel(tmp_path_factory):
    ziel = tmp_path_factory.mktemp("kernel") / "mutos_1216"
    with zipfile.ZipFile(ARCHIV) as z:
        ziel.write_bytes(z.read("dateien/mutos_1216"))
    assert ziel.stat().st_size == 36352
    return ziel


def dis(*args):
    return subprocess.run([str(Z8KDIS), *args], capture_output=True, text=True, timeout=120)


def test_rundlauf_mit_der_eingecheckten_karte(kernel):
    r = dis("--karte", str(KARTE), "--statistik", "--pruefen", str(kernel))
    assert r.returncode == 0, r.stdout + r.stderr
    assert "Rundlauf: 36352 Bytes verglichen, 0 abweichend" in r.stdout
    # Statistik: nur Belegtes in der Karte -> der groesste Teil bleibt unerreicht, aber Code wird gefunden
    m = re.search(r"Code\s+(\d+) Bytes \((\d+) Befehle\)", r.stderr)
    assert m, r.stderr
    assert int(m.group(1)) > 8000 and int(m.group(2)) > 2500
    assert "ungueltig 0" in r.stderr


def test_rundlauf_mit_allen_vorschlaegen_bleibt_bitgleich(kernel, tmp_path):
    v = dis("--karte", str(KARTE), "--vorschlaege", str(kernel))
    assert v.returncode == 0, v.stderr
    zeilen = [z for z in v.stdout.splitlines() if z.startswith("code ")]
    assert len(zeilen) > 100
    karte = tmp_path / "karte_voll.txt"
    karte.write_text(KARTE.read_text() + "\n".join(zeilen) + "\n")
    r = dis("--karte", str(karte), "--pruefen", str(kernel))
    assert r.returncode == 0, r.stdout + r.stderr
    assert "0 abweichend" in r.stdout


def test_as_ausgabe_hat_den_rahmen_von_cc(kernel):
    r = dis("--karte", str(KARTE), "--format", "as", "--modul", "mutos", str(kernel))
    assert r.returncode == 0, r.stderr
    t = r.stdout
    assert t.startswith("mutos module\n$NONSEGMENTED\nconstant\n")
    assert "\nstart:\n\tld\tr0,#%0000\n\tldctl\tpsapseg,r0\n" in t
    assert t.rstrip().endswith("end mutos")
    # keine Mnemonik in Grossbuchstaben im Code (der as erwartet kleingeschriebene Befehle wie cc -S)
    rumpf = t.split("\nE\n", 1)[1]
    assert not re.search(r"^\t[A-Z]{2,}\t", rumpf, re.M)
