"""H5 — k1520DiskTool live an der echten Diskette (nicht als Abbild).

Setzt eine formatierte Diskette voraus (nach H4 oder H3.2).  Beide Wege: Python-API
und Kommandozeile ``--physical``.
"""

import os
import subprocess
import sys

import pytest

from hw_helfer import PROJECT_ROOT, laufwerk, sitzung

pytestmark = pytest.mark.hw_schreiben

# CP/M speichert in 128-Byte-Saetzen: Groessen als Vielfache von 128, sonst kommt die
# Datei mit Fuellbytes zurueck und der Bytevergleich luegt.
DATEIEN = {
    "KLEIN.TXT": b"K1520 HW KLEIN\r\n" * 3,
    "MITTEL.DAT": bytes((i * 7 + 3) & 0xFF for i in range(3072)),
    "GROSS.DAT": bytes((i * 31 + i // 251) & 0xFF for i in range(40_960)),
}


def _namen(d):
    return {e.side_prefix + e.name for e in d.list()}


def _ohne_befund(d):
    rep = d.check()
    schwer = [x for x in rep.findings if x.severity >= 2]
    return rep, schwer


def test_h5_1_info_und_fsck_vor_der_aenderung(freigabe):
    from app.core_binding.k1520disk import DiskTool
    with sitzung() as s:
        with DiskTool.open_physical(s.sync) as d:
            print(f"\nerkannt: {d.format} / {d.filesystem}")
            assert d.filesystem
            rep, schwer = _ohne_befund(d)
            assert not schwer, [f.text for f in schwer]


def test_h5_2_aendern_und_h5_3_frisch_einhaengen(freigabe, tmp_path):
    from app.core_binding.k1520disk import DiskTool
    for n, b in DATEIEN.items():
        (tmp_path / n).write_bytes(b)

    with sitzung(schreibbar=True) as s:
        with DiskTool.open_physical(s.sync, read_only=False) as d:
            vorher = _namen(d)
            for n in DATEIEN:
                d.insert(tmp_path / n, n, overwrite=True)
            d.erase("KLEIN.TXT")
            d.flush()
            assert s.sync.flush(300_000), s.sync.last_error
        st = s.sync.stats
        print(f"\n{st.writes_done} Spuren geschrieben, {st.verifies_done} geprueft")
        assert st.writes_done > 0 and st.verifies_done >= st.writes_done
        assert st.tracks_defect == 0 and st.verify_failed == 0

    # frisch: neue Sitzung, nichts im Speicher
    with sitzung() as s:
        with DiskTool.open_physical(s.sync) as d:
            namen = _namen(d)
            assert "KLEIN.TXT" not in namen
            for n in ("MITTEL.DAT", "GROSS.DAT"):
                treffer = [x for x in namen if x.endswith(n)]
                assert treffer, f"{n} fehlt: {sorted(namen)}"
                ziel = tmp_path / f"zurueck_{n}"
                d.extract(treffer[0], ziel)
                assert ziel.read_bytes() == DATEIEN[n], f"{n} veraendert"
            for v in vorher:                       # kein Nebenschaden
                assert any(x.endswith(v.split("/")[-1]) for x in namen), v
            rep, schwer = _ohne_befund(d)
            assert not schwer, [f.text for f in schwer]


def test_h5_5_kommandozeile_physical(freigabe, tmp_path):
    p = laufwerk()
    basis = [sys.executable, str(PROJECT_ROOT / "app/disktool/main.py"), "--physical",
             "--drive", p["drive"], "--rate", str(p["cell_rate_kbps"])]
    env = dict(os.environ, LD_LIBRARY_PATH=str(PROJECT_ROOT / "build"))
    r = subprocess.run(basis + ["ls"], capture_output=True, text=True, env=env, timeout=900)
    assert r.returncode == 0, r.stderr[-500:]
    assert "MITTEL.DAT" in r.stdout
    r = subprocess.run(basis + ["check"], capture_output=True, text=True, env=env, timeout=900)
    assert r.returncode == 0, r.stdout[-500:] + r.stderr[-500:]
