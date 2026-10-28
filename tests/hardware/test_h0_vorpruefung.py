"""H0 — Vorpruefung: Adapter, Drehzahl, Zellzahl.  Lesend."""

import pytest

pytestmark = pytest.mark.hw_lesen


def test_adapter_laufwerk_und_drehzahl(vorpruefung):
    v = vorpruefung
    print(f"\nGreaseweazle {v['adapter'].model}, Firmware {v['adapter'].firmware}; "
          f"Umlauf {v['umdrehung_ms']:.1f} ms, {v['zellen']} Zellen, "
          f"erste Spur in {v['lesedauer']:.2f} s")
    assert abs(v["umdrehung_ms"] - 200.0) < 6.0, "Drehzahl ausserhalb 300 U/min +-3 %"
    assert v["zellen"] > 80_000, "viel zu wenige Zellen — falsche Zellrate?"
