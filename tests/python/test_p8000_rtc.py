"""Uhrenkarte des P8000 in Maschinenzeit (Entwurf 37, AP R2b): C-ABI `k1520_rtc_*` und Python-Bindung.

Die Rechnung (Startregel, Jahresversatz) lebt im Kern; hier wird nur geprueft, dass die Bindung sie
unveraendert durchreicht -- ohne Maschine (statische Funktionen) und an einer laufenden Maschine.
"""

import calendar
import time

import pytest

from conftest import requires_core

pytestmark = requires_core

from app.core_binding.k1520 import K1520Emulator  # noqa: E402

ZIEL = calendar.timegm((2030, 1, 1, 12, 0, 0))   # Zukunft: die Startregel (nie zurueck) laesst sie gelten


def _jahr(t: int) -> int:
    return time.gmtime(t).tm_year


def _stand(pfad) -> int:
    for z in pfad.read_text().splitlines():
        if z.startswith("stand="):
            return int(z[6:])
    raise AssertionError(pfad.read_text())


def _p8000(typ, datei, versatz=0):
    return K1520Emulator(machine="p8000", p8000={
        "karte16": "1", "rtc": typ, "rtc_utc": 1, "rtc_jahresversatz": versatz, "rtc_zustand": str(datei)})


def test_startzeit_ohne_datei_ist_die_wirtszeit_und_stellen_datei_wirkt(tmp_path):
    datei = tmp_path / "rtc.txt"
    jetzt = calendar.timegm(time.gmtime())
    assert abs(K1520Emulator.rtc_startzeit(str(datei), 0, True) - jetzt) <= 5
    assert abs(K1520Emulator.rtc_startzeit("", 0, True) - jetzt) <= 5
    assert K1520Emulator.rtc_stellen_datei(str(datei), ZIEL)
    assert _stand(datei) == ZIEL
    assert K1520Emulator.rtc_startzeit(str(datei), 0, True) == ZIEL
    # gastsichtig: der Jahresversatz wirkt auf die Anzeige, nicht auf die Datei
    assert _jahr(K1520Emulator.rtc_startzeit(str(datei), -28, True)) == 2002
    assert _jahr(K1520Emulator.rtc_startzeit(str(datei), -40, True)) == 1990
    assert _stand(datei) == ZIEL
    assert not K1520Emulator.rtc_stellen_datei(str(tmp_path / "gibt" / "es" / "nicht.txt"), ZIEL)


@pytest.mark.parametrize("typ", ["rtc72421", "u130x"])
def test_uhr_zaehlt_in_maschinenzeit_und_wird_beim_beenden_gespeichert(tmp_path, typ):
    datei = tmp_path / "rtc.txt"
    emu = _p8000(typ, datei)
    assert K1520Emulator.rtc_startzeit(str(datei), 0, True) <= calendar.timegm(time.gmtime()) + 5
    assert emu.rtc_stellen(ZIEL)
    assert emu.rtc_zeit() == ZIEL
    assert _stand(datei) == ZIEL                    # beim Stellen sofort geschrieben
    emu.power_on()
    done = 0
    while done < 8_000_000:                         # 2 s Maschinenzeit (4 MHz)
        done += emu.run(100_000)
    z = emu.rtc_zeit()
    assert z in (ZIEL + 2, ZIEL + 3), z
    del emu                                         # Zerstoeren schreibt den Stand
    assert _stand(datei) == z
    # Neustart sofort: nie zurueck, der Stand bleibt; die statische Rechnung stimmt mit der Karte ueberein
    st = K1520Emulator.rtc_startzeit(str(datei), 0, True)
    assert st == z
    emu2 = _p8000(typ, datei)
    assert emu2.rtc_zeit() == st


def test_stellen_von_aussen_mit_jahresversatz_gastsichtig(tmp_path):
    for versatz in (-28, -40):
        datei = tmp_path / f"rtc{-versatz}.txt"
        emu = _p8000("rtc72421", datei, versatz)
        assert emu.rtc_stellen(ZIEL)
        assert _jahr(emu.rtc_zeit()) == 2030 + versatz
        assert _stand(datei) == ZIEL                # gespeichert wird unversetzt
        assert emu.rtc_zeit() == K1520Emulator.rtc_startzeit(str(datei), versatz, True)


def test_ohne_karte_und_an_anderen_maschinen_meldet_minus_eins():
    emu = K1520Emulator(machine="p8000", p8000={"karte16": "1"})
    assert emu.rtc_zeit() == -1 and not emu.rtc_stellen(ZIEL)
    a5120 = K1520Emulator(machine="a5120")
    assert a5120.rtc_zeit() == -1 and not a5120.rtc_stellen(ZIEL)
