"""H6 — Rundlauf Emulator <-> DiskTool an derselben echten Diskette."""

import pytest

from hw_helfer import Fahrer, cpa_bis_prompt, sitzung

pytestmark = pytest.mark.hw_schreiben


def test_h6_disktool_schreibt_gast_liest_und_aendert_disktool_liest(freigabe, tmp_path, logdir):
    from app.core_binding.k1520 import K1520Emulator
    from app.core_binding.k1520disk import DiskTool

    text = b"HALLO VOM DISKTOOL\r\n"
    (tmp_path / "HALLO.TXT").write_bytes(text)
    with sitzung(schreibbar=True) as s:
        with DiskTool.open_physical(s.sync, read_only=False) as d:
            # Reste frueherer Laeufe weg: steht GAST.TXT schon da, verweigert CP/A das REN
            for alt in [e.side_prefix + e.name for e in d.list()]:
                if alt.endswith(("HALLO.TXT", "GAST.TXT")):
                    d.erase(alt)
            d.insert(tmp_path / "HALLO.TXT", "HALLO.TXT", overwrite=True)
            d.flush()
            assert s.sync.flush(300_000), s.sync.last_error

    with sitzung(schreibbar=True, fuer_emulator=True, read_ahead=False) as s:
        emu = K1520Emulator()
        assert emu.mount_physical(0, s.sync, False), emu.last_error()
        emu.power_on()
        f = Fahrer(emu, "h6", logdir)
        assert cpa_bis_prompt(f), f.bild()
        f.zeile("TYPE HALLO.TXT")
        f.laufen(60_000_000)
        f._bild("nach TYPE")
        assert "HALLO VOM DISKTOOL" in f.bild(), f.bild()      # Gast liest die Datei
        f.zeile("REN GAST.TXT=HALLO.TXT")                      # Gast aendert die Diskette
        f.laufen(80_000_000)
        f.zeile("DIR")
        f.laufen(60_000_000)
        f._bild("nach REN + DIR")
        assert "GAST" in f.bild(), f.bild()
        del emu
        assert s.sync.flush(300_000), s.sync.last_error
        assert s.sync.stats.tracks_defect == 0

    with sitzung() as s:
        with DiskTool.open_physical(s.sync) as d:
            namen = {e.side_prefix + e.name for e in d.list()}
            assert any(n.endswith("GAST.TXT") for n in namen), sorted(namen)
            assert not any(n.endswith("HALLO.TXT") for n in namen)
            ziel = tmp_path / "gast.txt"
            d.extract(next(n for n in namen if n.endswith("GAST.TXT")), ziel)
            zurueck = ziel.read_bytes()
            # CP/M fuellt den letzten 128-Byte-Satz auf (0x1A bzw. 0x00)
            assert zurueck[:len(text)] == text and len(zurueck) % 128 == 0, zurueck
            assert set(zurueck[len(text):]) <= {0x1A, 0x00}, zurueck
            rep = d.check()
            assert not [x for x in rep.findings if x.severity >= 2]
