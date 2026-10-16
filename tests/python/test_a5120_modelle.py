"""A5120-Ausfuehrungen in Bindung und Oberflaeche — Entwurf 30, AP A5.

Die Bestueckung selbst pruefen die C++-Waechter (`A5120Ausfuehrung.*`); hier geht es um den
Weg ``k1520_create_a5120_ausfuehrung`` → ``K1520Emulator(ausfuehrung=…)`` bzw.
``machine="a5120-alt"`` und — seit Entwurf 33 N5 — um das Programmprofil ``a5120emu``: drei
Modelle (A5120/A5130/K8924) als VORGABEN, die Optionsfelder (RAM, Tastatur, Lade-ROM,
Erweiterungsmodul, Kassette), die Modellwahl, die Vorgaben uebernimmt (auch beim erneuten
Waehlen), der Fenstertitel und die Konfiguration.
"""

from pathlib import Path

import pytest

from conftest import requires_core

pytestmark = requires_core


def _emu(**kw):
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator(**kw)


# ── Bindung ──────────────────────────────────────────────────────────────────

def test_vorgabe_ist_die_ausfuehrung_neu():
    e = _emu()
    assert e.ausfuehrung == "neu"
    assert e.framebuffer_size() == (640, 288)
    assert e.machine == "a5120"


@pytest.mark.parametrize("kw", [dict(ausfuehrung="alt"), dict(machine="a5120-alt")])
def test_alt_hat_den_kleinen_bildschirm(kw):
    e = _emu(**kw)
    assert e.ausfuehrung == "alt"
    assert e.machine == "a5120-alt"
    assert e.framebuffer_size() == (512, 256)
    assert not e.tape_installed()
    e.mem_write(0xFC00, ord("A"))
    e.mem_write(0xFC00 + 15 * 64, ord("Z"))
    zeilen = e.screen_text().split("\n")
    assert len(zeilen) == 16 and all(len(z) == 64 for z in zeilen)
    assert zeilen[0][0] == "A" and zeilen[15][0] == "Z"
    assert e.keyboard_leds() == 0


def test_kmbg_hat_die_kassettenkarte_fest():
    e = _emu(machine="a5120-kmbg")
    assert e.ausfuehrung == "kmbg"
    assert e.tape_installed()
    # tape=True ist am KMBG ohne Wirkung — kein „steckt bereits“-Fehler.
    e2 = _emu(ausfuehrung="kmbg", tape=True)
    assert e2.tape_installed()


def test_laufwerke_der_ausfuehrung_und_eigene():
    e = _emu(ausfuehrung="alt")
    assert e.drive_default_format(3) == ""          # Vorgabe: Steckplatz 4 frei
    e = _emu(ausfuehrung="alt", drive_types=["K5601", "K5601", "none", "none"])
    assert e.drive_default_format(0) != e.drive_default_format(2)


def test_acht_lehnt_der_kern_mit_grund_ab():
    with pytest.raises(RuntimeError, match="K5602"):
        _emu(ausfuehrung="acht")


def test_falsche_kombinationen():
    with pytest.raises(ValueError):
        _emu(ausfuehrung="alt", em="em256")
    with pytest.raises(ValueError):
        _emu(machine="k8915", ausfuehrung="alt")
    with pytest.raises(ValueError):
        _emu(ausfuehrung="gibtsnicht")
    with pytest.raises(ValueError):
        _emu(machine="a5120-alt", ausfuehrung="kmbg")


def test_ausfuehrung_an_anderen_maschinen_leer():
    assert _emu(machine="k8915").ausfuehrung == ""


# ── Lade-ROM und Tastatur (k1520_create_a5120_konfig, Modell „BCU 880“) ─────────

def test_laderom_und_tastatur_vorgaben():
    e = _emu()
    assert (e.laderom, e.tastatur) == ("v09_2", "k7637")
    e = _emu(ausfuehrung="alt")
    assert (e.laderom, e.tastatur) == ("v09_2", "k7636")
    assert _emu(machine="k8915").laderom == ""


def test_bcu_bestueckung_ueber_die_bindung():
    e = _emu(machine="a5120-alt", laderom="v06", tastatur="k7636")
    assert (e.ausfuehrung, e.laderom, e.tastatur) == ("alt", "v06", "k7636")
    assert e.framebuffer_size() == (512, 256)
    # Satz 1 der BCU-Disketten: K7024 80×24 mit der K7636 („neu“ + tastatur).
    e = _emu(laderom="v06", tastatur="k7636")
    assert (e.ausfuehrung, e.laderom, e.tastatur, e.machine) == ("neu", "v06", "k7636", "a5120")
    assert e.framebuffer_size() == (640, 288)
    e = _emu(laderom="v09_1")
    assert (e.laderom, e.tastatur) == ("v09_1", "k7637")


def test_laderom_und_tastatur_falsch():
    with pytest.raises(ValueError):
        _emu(laderom="v07")
    with pytest.raises(ValueError):
        _emu(tastatur="k7672")
    with pytest.raises(ValueError):
        _emu(laderom="v06", em="em256")
    with pytest.raises(ValueError):
        _emu(machine="k8915", laderom="v06")
    # Seit Entwurf 33 N1 ist die Tastatur frei: K7637 am „alt“ (K7023) haengt an der K8025.
    e = _emu(ausfuehrung="alt", tastatur="k7637")
    assert (e.ausfuehrung, e.tastatur) == ("alt", "k7637")
    assert e.framebuffer_size() == (512, 256)


def test_c_abi_lehnt_unbekannte_werte_mit_grund_ab():
    from app.core_binding import k1520
    lib = k1520._lib
    for args in ((1, 3, 0), (1, 0, 3), (4, 0, 0)):
        h = lib.k1520_create_a5120_konfig(*args, None, None, None, None)
        assert not h, args
        assert lib.k1520_last_init_error()
    h = lib.k1520_create_a5120_konfig(1, 2, 0, None, None, None, None)
    try:
        assert (lib.k1520_a5120_laderom(h), lib.k1520_a5120_tastatur(h)) == (2, 2)
    finally:
        lib.k1520_destroy(h)


def test_bcu_lieferdiskette_bootet_ueber_die_bindung(tmp_path):
    """Die ausgelieferte Startdiskette (disks/a5120_bcu/, nur als Kopie) mit der Bestueckung
    „BCU 880“ (Huelle alt, Lade-ROM V0.6, K7636) und den Laufwerken K5600.10 bis zum Prompt."""
    import shutil
    from conftest import ausgelieferte_diskette
    kopie = tmp_path / "bcu_start.hfe"
    shutil.copyfile(ausgelieferte_diskette("a5120_bcu12_k5600-10_start-syl.hfe"), kopie)
    e = _emu(drive_types=["K5600.10", "K5600.10", "K5600.10", "none"],
             konfig={"ausfuehrung": "alt", "laderom": "v06", "tastatur": "k7636"})
    assert e.laderom == "v06"
    assert e.mount_disk(0, str(kopie), e.drive_default_format(0))
    e.power_on()
    for _ in range(400):
        e.run(100_000)
        if "\nA>" in e.screen_text():
            break
    assert "BCU 880 VERS 1.2" in e.screen_text()
    assert "\nA>" in e.screen_text(), e.screen_text()


# ── Programmprofil: drei Modelle als Vorgaben (Entwurf 33 §3a, N5) ───────────

def _profil():
    from app import profil
    return profil.profil("a5120")


def test_profil_kennt_genau_drei_modelle_mit_titeln():
    p = _profil()
    assert [m[0] for m in p.modelle] == ["a5120", "a5130", "k8924"]
    assert [m[3] for m in p.modelle] == ["A5120", "A5130", "K8924"]
    assert p.standard_modell() == "a5120"
    assert [p.modell_titel(k) for k in ("a5120", "a5130", "k8924")] == \
        ["A5120 Emulator", "A5130 Emulator", "K8924 Emulator"]
    assert p.titel == "A5120 Emulator" and p.programm == "a5120emu"    # Programmname bleibt
    assert not p.gesperrte_modelle and not p.modell_alt
    assert p.modell_em("a5120") is None


def test_vorgaben_je_modell():
    p = _profil()
    gemeinsam = {"ram": "k3526", "bildschirm": "k7024", "laderom": "v09_2",
                 "erweiterungsmodul": "none", "kassette": "0"}
    a = p.modell_vorgabe("a5120")
    assert {k: a[k] for k in gemeinsam} == gemeinsam
    assert a["tastatur"] == "k7637" and a["laufwerke"] == ["K5601"] * 3 + ["none"]
    b = p.modell_vorgabe("a5130")
    assert b["tastatur"] == "k7636" and b["laufwerke"] == ["K5601"] * 4
    c = p.modell_vorgabe("k8924")
    assert c["tastatur"] == "k7637" and c["laufwerke"] == ["K5601"] * 3 + ["none"]
    assert p.hardware_standard("a5130") == {"ram": "k3526", "tastatur": "k7636",
                                            "laderom": "v09_2", "erweiterungsmodul": "none"}


def test_optionsfelder_und_beschriftungen():
    p = _profil()
    felder = {h[0]: h for h in p.hardware}
    assert list(felder) == ["ram", "tastatur", "laderom", "erweiterungsmodul"]
    assert [a for _w, a in felder["ram"][3]] == \
        ["1x K3525 16KB", "2x K3525 32KB", "4x K3525 64KB", "K3526 64KB"]
    assert [w for w, _a in felder["tastatur"][3]] == ["k7636", "k7637"]
    # Das Lade-ROM „K8924 V0.9a“ fehlt, solange der Kern es nicht kennt.
    assert [w for w, _a in felder["laderom"][3]] == ["v09_2", "v09_1", "v06"]
    assert felder["erweiterungsmodul"][4] == ("a5120",)
    assert p.tape_wahl and not p.tape_fest


def test_kern_parameter_sind_einzeloptionen_in_konfig():
    p = _profil()
    kern = p.kern_parameter("a5130", {})
    assert list(kern) == ["konfig"]
    assert kern["konfig"] == {"ram": "k3526", "bildschirm": "k7024", "tastatur": "k7636",
                              "laderom": "v09_2", "em": "none"}
    hw = {"ram": "k3525_2", "tastatur": "k7636", "laderom": "v06", "erweiterungsmodul": "em256"}
    # Erweiterungsmodul nur mit K3526 ...
    assert p.kern_parameter("a5120", hw)["konfig"]["em"] == "none"
    assert p.modell_em("a5120", hw) is None
    hw["ram"] = "k3526"
    assert p.kern_parameter("a5120", hw)["konfig"]["em"] == "em256"
    assert p.modell_em("a5120", hw) == "em256"
    # ... und nur am A5120.
    assert p.kern_parameter("a5130", hw)["konfig"]["em"] == "none"
    assert p.modell_em("k8924", hw) is None
    assert not p.hardware_wirkt("erweiterungsmodul", "a5130", hw)
    assert p.hardware_wirkt("erweiterungsmodul", "a5120", hw)
    assert not p.hardware_wirkt("erweiterungsmodul", "a5120", {**hw, "ram": "k3525_4"})


@pytest.mark.parametrize("modell", ["a5120", "a5130", "k8924"])
def test_kern_parameter_baut_eine_maschine(modell):
    p = _profil()
    kern = p.kern_parameter(modell, {})
    kern["konfig"]["kassette"] = False
    e = _emu(**kern)
    assert e.tastatur == p.modell_vorgabe(modell)["tastatur"]
    assert e.framebuffer_size() == (640, 288)


def test_unbekannte_modelle_werden_zur_vorgabe_keine_umsetzung():
    p = _profil()
    for alt in (None, "", "a5120.16", "a5120-alt", "a5120-kmbg", "a5120-bcu", "a5120-8", "quatsch"):
        assert p.modell_normalisieren(alt) == "a5120", alt
    assert p.modell_normalisieren(" A5130 ") == "a5130"
    # Andere Programme kennen die A5120-Modelle nicht.
    assert profil_k8915().modell_normalisieren("a5130") == "k8915"


def profil_k8915():
    from app import profil
    return profil.profil("k8915")


def test_hardware_normalisieren_prueft_jede_option_einzeln():
    p = _profil()
    n = p.hardware_normalisieren({"ram": "k3525_4", "tastatur": "quatsch", "laderom": "v06"}, "a5130")
    assert n == {"ram": "k3525_4", "tastatur": "k7636", "laderom": "v06", "erweiterungsmodul": "none"}
    assert p.hardware_normalisieren({}, "k8924")["tastatur"] == "k7637"


# ── Oberflaeche ───────────────────────────────────────────────────────────────

@pytest.fixture
def konfig_ordner(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    from app import config_io
    ordner = Path(config_io.default_config_dir())
    ordner.mkdir(parents=True, exist_ok=True)
    return ordner


def _fenster(qapp):
    from app import profil
    from app.ui.main_window import MainWindow
    w = MainWindow(None, profil=profil.profil("a5120"))
    w.show()
    qapp.processEvents()
    return w


def _zu(w, qapp):
    w.close()
    qapp.processEvents()


def _waehle(w, qapp, modell):
    """Wie der Anwender: Eintrag im Modellfeld waehlen (`activated` meldet auch Gleiches)."""
    combo = w.settings_widget.model_combo
    i = combo.findData(modell)
    assert i >= 0
    combo.setCurrentIndex(i)
    combo.activated.emit(i)
    qapp.processEvents()


def _option(w, schluessel, wert):
    box = w.settings_widget.hardware_combos[schluessel]
    box.setCurrentIndex(box.findData(wert))


def test_modellfeld_zeigt_genau_die_drei_modelle(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        combo = w.settings_widget.model_combo
        assert [combo.itemText(i) for i in range(combo.count())] == ["A5120", "A5130", "K8924"]
        assert set(w.settings_widget.hardware_combos) == {"ram", "tastatur", "laderom", "erweiterungsmodul"}
        ram = w.settings_widget.hardware_combos["ram"]
        assert [ram.itemText(i) for i in range(ram.count())] == \
            ["1x K3525 16KB", "2x K3525 32KB", "4x K3525 64KB", "K3526 64KB"]
        assert w.windowTitle() == "A5120 Emulator"
        assert w.emulator.tastatur == "k7637"
    finally:
        _zu(w, qapp)


def test_modellwahl_uebernimmt_vorgaben_laufwerke_und_titel(qapp, konfig_ordner):
    from app import config_io
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        _waehle(w, qapp, "a5130")
        assert w._model == "a5130" and w.windowTitle() == "A5130 Emulator"
        assert w.emulator.tastatur == "k7636"
        assert sw.hardware_value()["tastatur"] == "k7636"
        assert list(w._drive_types) == ["K5601"] * 4
        assert w.emulator.drive_default_format(3) != ""         # vierter Schacht bestueckt

        _waehle(w, qapp, "k8924")
        assert w.windowTitle() == "K8924 Emulator"
        assert w.emulator.tastatur == "k7637"
        assert list(w._drive_types) == ["K5601"] * 3 + ["none"]
        assert w.emulator.drive_default_format(3) == ""

        w._autosave_now()
        cfg = config_io.load_config(str(konfig_ordner / "a5120emu.yaml"))
        assert cfg["general"]["model"] == "k8924"
    finally:
        _zu(w, qapp)

    # Neustart: Modell, Titel und Maschine kommen wieder.
    w = _fenster(qapp)
    try:
        assert w._model == "k8924" and w.windowTitle() == "K8924 Emulator"
    finally:
        _zu(w, qapp)


def test_erneutes_waehlen_desselben_modells_setzt_die_vorgaben_zurueck(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _option(w, "ram", "k3525_2")
        _option(w, "tastatur", "k7636")
        qapp.processEvents()
        assert w._hardware["ram"] == "k3525_2" and w.emulator.tastatur == "k7636"
        assert "ram=k3525_2" in w.emulator._konfig_text
        _waehle(w, qapp, "a5120")                  # gleicher Eintrag wie vorher
        assert w._hardware["ram"] == "k3526" and w._hardware["tastatur"] == "k7637"
        assert w.emulator.tastatur == "k7637" and "ram=k3526" in w.emulator._konfig_text
        assert w.settings_widget.hardware_value()["ram"] == "k3526"
    finally:
        _zu(w, qapp)


def test_modellwechsel_ueberschreibt_geaenderte_optionen_und_kassette(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _option(w, "laderom", "v09_1")
        w.settings_widget.tape_box.setChecked(True)
        qapp.processEvents()
        assert w._tape and w.emulator.tape_installed() and w.emulator.laderom == "v09_1"
        _waehle(w, qapp, "a5130")
        assert w.emulator.laderom == "v09_2"
        assert not w._tape and not w.emulator.tape_installed()
        assert not w.settings_widget.tape_value()
    finally:
        _zu(w, qapp)


def test_optionen_wirken_im_kern_und_kassette(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _option(w, "ram", "k3525_4")
        qapp.processEvents()
        assert "ram=k3525_4" in w.emulator._konfig_text
        w.settings_widget.tape_box.setChecked(True)
        qapp.processEvents()
        assert w.emulator.tape_installed() and "ram=k3525_4" in w.emulator._konfig_text
    finally:
        _zu(w, qapp)


def test_erweiterungsmodul_nur_am_a5120_und_mit_k3526(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        em = sw.hardware_combos["erweiterungsmodul"]
        assert em.isEnabled()
        _option(w, "erweiterungsmodul", "em256")
        qapp.processEvents()
        assert w.emulator.em_variant() == "em256" and w.status_widget.em_sichtbar()
        _option(w, "ram", "k3525_4")                 # ohne K3526: Feld grau, kein EM
        qapp.processEvents()
        assert not em.isEnabled()
        assert w.emulator.em_variant() == ""
        _option(w, "ram", "k3526")
        qapp.processEvents()
        assert em.isEnabled() and w.emulator.em_variant() == "em256"
        for modell in ("a5130", "k8924"):
            _waehle(w, qapp, modell)
            assert not em.isEnabled(), modell
            assert w.emulator.em_variant() == ""
    finally:
        _zu(w, qapp)


def test_konfiguration_alte_modellschluessel_sind_die_vorgabe(qapp, konfig_ordner):
    (konfig_ordner / "a5120emu.yaml").write_text(
        "version: 1\ngeneral: {model: a5120-kmbg, ram: k3525_2, tastatur: quatsch}\n",
        encoding="utf-8")
    w = _fenster(qapp)
    try:
        assert w._model == "a5120" and w.windowTitle() == "A5120 Emulator"
        # Gueltige Einzelwerte bleiben, ungueltige werden zur Vorgabe.
        assert w._hardware["ram"] == "k3525_2" and w._hardware["tastatur"] == "k7637"
        assert "ram=k3525_2" in w.emulator._konfig_text
    finally:
        _zu(w, qapp)


def test_konfiguration_ohne_modell_startet_den_a5120(qapp, konfig_ordner):
    (konfig_ordner / "a5120emu.yaml").write_text(
        "version: 1\ngeneral: {}\n", encoding="utf-8")
    w = _fenster(qapp)
    try:
        assert w._model == "a5120" and w.emulator.tastatur == "k7637"
    finally:
        _zu(w, qapp)


def test_konfiguration_fehlende_optionen_gelten_nach_modellvorgabe(qapp, konfig_ordner):
    (konfig_ordner / "a5120emu.yaml").write_text(
        "version: 1\ngeneral: {model: a5130}\n", encoding="utf-8")
    w = _fenster(qapp)
    try:
        assert w._model == "a5130" and w._hardware["tastatur"] == "k7636"
        assert w.emulator.tastatur == "k7636"
    finally:
        _zu(w, qapp)


def test_ausgelieferte_vorgabe_traegt_modell_aber_keine_optionen():
    from app import config_io, profil
    p = profil.profil("a5120")
    allg = config_io.standard_konfiguration(p)["general"]
    assert allg["model"] == "a5120"
    assert not (set(allg) & {"ram", "tastatur", "laderom", "erweiterungsmodul", "bildschirm"})
