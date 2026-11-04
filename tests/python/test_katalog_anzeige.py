"""Katalogfelder in den Auswahllisten (Entwurf 47, Schritt 6, Waechter W6).

Zeile = ``Kapazitaet · Rechner · Titel``; der technische Schluessel steht nur im Tooltip
und in den Daten (itemData) — nie allein als Text.
"""

import pytest

from conftest import requires_disk

from app import katalog_anzeige as ka


# ── (a) reine Hilfsfunktionen ────────────────────────────────────────────────
def test_zeile_verbindet_und_laesst_leere_teile_aus():
    assert ka.zeile("800K", ["PC 1715"], "SCP 1715 Systemdiskette") == \
        "800K · PC 1715 · SCP 1715 Systemdiskette"
    assert ka.zeile("800K", ["A", "B"], "T") == "800K · A, B · T"
    assert ka.zeile("800K", [], "T") == "800K · T"
    assert ka.zeile("", [], "T") == "T"
    assert ka.zeile("800K", ("", "A"), "") == "800K · A"
    assert ka.zeile("", (), "") == ""


def test_tooltip_nennt_den_technischen_namen():
    t = ka.tooltip("scp1715_ds80_5x1024_boot", "Langtext", ["PC 1715"])
    assert "scp1715_ds80_5x1024_boot" in t and "Langtext" in t and "PC 1715" in t


def test_kapazitaet_wird_numerisch_gelesen_und_sortiert():
    assert ka.kapazitaet_kb("800K") == 800
    assert ka.kapazitaet_kb("1,44M") == pytest.approx(1474.56)
    assert ka.kapazitaet_kb("") == float("inf")
    eintraege = [("1,44M", ["B"], "x"), ("160K", ["Z"], "x"), ("800K", ["B"], "b"),
                 ("800K", ["A"], "z"), ("800K", ["B"], "a"), ("", [], "")]
    s = sorted(eintraege, key=lambda e: ka.sortierschluessel(*e))
    assert s == [("160K", ["Z"], "x"), ("800K", ["A"], "z"), ("800K", ["B"], "a"),
                 ("800K", ["B"], "b"), ("1,44M", ["B"], "x"), ("", [], "")]


# ── GUI ──────────────────────────────────────────────────────────────────────
@pytest.fixture(scope="module")
def qt_app():
    pytest.importorskip("PySide6")
    from PySide6.QtWidgets import QApplication
    yield QApplication.instance() or QApplication([])


@requires_disk
def test_neu_dialog_zeigt_kapazitaet_titel_rechner_nie_den_schluessel_allein(qt_app):
    from PySide6.QtCore import Qt
    from app.core_binding.k1520disk import filesystems
    from app.disktool.ui.main_window import neu_eintraege
    from app.disktool.ui.neu_dialog import NeuDialog
    katalog = {f.name: f for f in filesystems()}
    dlg = NeuDialog(neu_eintraege())
    assert dlg.wahl.count() > 10
    for i in range(dlg.wahl.count()):
        name = dlg.wahl.itemData(i)
        text = dlg.wahl.itemText(i)
        f = katalog[name]
        assert text != name, name
        if f.capacity:
            assert text.startswith(f.capacity), (name, text)
        if f.title:
            assert f.title in text, (name, text)
        if f.machines:
            assert ", ".join(f.machines) in text, (name, text)
        assert name in dlg.wahl.itemData(i, Qt.ToolTipRole)
    # sortiert nach Kapazitaet
    kap = [ka.kapazitaet_kb(katalog[dlg.wahl.itemData(i)].capacity)
           for i in range(dlg.wahl.count())]
    assert kap == sorted(kap)


@requires_disk
def test_filter_fuer_rechner_liefert_nur_dessen_profile(qt_app):
    from app.core_binding.k1520disk import filesystems
    from app.disktool.ui.main_window import neu_eintraege
    from app.disktool.ui.neu_dialog import NeuDialog
    katalog = {f.name: f for f in filesystems()}
    dlg = NeuDialog(neu_eintraege())
    alle = {dlg.wahl.itemData(i) for i in range(dlg.wahl.count())}
    assert dlg.rechner_wahl.itemText(0) == "alle"
    i = dlg.rechner_wahl.findData("PC 1715")
    assert i > 0, "PC 1715 muss als Rechner vorkommen"
    dlg.rechner_wahl.setCurrentIndex(i)
    gefiltert = {dlg.wahl.itemData(k) for k in range(dlg.wahl.count())}
    assert gefiltert and gefiltert < alle
    for name in gefiltert:
        assert "PC 1715" in katalog[name].machines, name
    assert not any(not katalog[n].machines for n in gefiltert)
    # zurueck auf „alle": die Profile ohne Rechnerangabe sind wieder da
    dlg.rechner_wahl.setCurrentIndex(0)
    assert {dlg.wahl.itemData(k) for k in range(dlg.wahl.count())} == alle
    assert dlg.dateisystem() in alle


@requires_disk
def test_format_dialog_zeigt_kapazitaet_und_titel(qt_app):
    from PySide6.QtCore import Qt
    from app.core_binding.k1520 import K1520Emulator
    from app.ui.format_dialog import FormatDialog
    emu = K1520Emulator()
    namen = ["cpa800", "ds80_16x256", "ds80_5x1024"]
    namen = [n for n in namen if emu.format_capacity(n) or emu.format_title(n)]
    assert namen, "kein Testformat mit Katalogtitel gefunden"
    dlg = FormatDialog(None, formate=namen, vorauswahl=namen[0],
                       beschreibung=emu.format_description,
                       anzeige=lambda n: (emu.format_capacity(n), emu.format_title(n)))
    for i in range(dlg.liste.count()):
        e = dlg.liste.item(i)
        n = e.data(Qt.UserRole)
        assert e.text() != n
        assert emu.format_capacity(n) in e.text() and emu.format_title(n) in e.text()
        assert n in e.toolTip()
