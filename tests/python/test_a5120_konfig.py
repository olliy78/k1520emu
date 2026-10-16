"""A5120-Einzeloptionen ueber C-ABI und Bindung — Entwurf 33, Paket N4.

Die Maschine selbst pruefen die C++-Waechter (`A5120Optionen.*`, `A5120Bildschirm.*`,
`BootIntegrationBeide.*`); hier geht es um den WEG: Konfigurationstext
(``k1520_create_a5120_text``) <-> ``K1520Emulator(konfig={...})``, die Eigenschaften
``bildschirm``/``bildschirm_schalter`` und die Fehlerfaelle.
"""

import pytest

from conftest import requires_core

pytestmark = requires_core


def _emu(**kw):
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator(**kw)


def _lib():
    from app.core_binding import k1520
    return k1520._lib


# ── Konfigurationstext, Rundlauf ─────────────────────────────────────────────

def test_ohne_konfig_ist_es_die_vorgabemaschine():
    e = _emu(konfig={})
    assert (e.ausfuehrung, e.bildschirm, e.tastatur, e.laderom) == ("neu", "k7024", "k7637", "v09_2")
    assert e.framebuffer_size() == (640, 288)
    assert e.bildschirm_schalter == "k7024"
    assert not e.tape_installed()


@pytest.mark.parametrize("bildschirm,groesse", [("k7024", (640, 288)), ("k7023", (512, 256))])
def test_eine_karte_hat_keinen_schalter(bildschirm, groesse):
    e = _emu(konfig={"bildschirm": bildschirm})
    assert e.bildschirm == bildschirm
    assert e.bildschirm_schalter == bildschirm      # die gesteckte Karte wird gezeigt
    assert e.framebuffer_size() == groesse
    with pytest.raises(ValueError, match="beide"):
        e.bildschirm_schalter = "k7023" if bildschirm == "k7024" else "k7024"


def test_beide_karten_und_schalter_ueber_python():
    e = _emu(konfig={"bildschirm": "beide"})
    assert (e.bildschirm, e.bildschirm_schalter) == ("beide", "k7024")
    assert e.framebuffer_size() == (640, 288)
    e.bildschirm_schalter = "k7023"
    assert e.bildschirm_schalter == "k7023"
    assert e.framebuffer_size() == (512, 256)
    # Zeiger und Groesse stimmen unter der einen Sperre zusammen.
    assert len(e.get_framebuffer()) == 512 * 256
    e.bildschirm_schalter = "K7024"
    assert len(e.get_framebuffer()) == 640 * 288


def test_schalter_anfangsstellung_und_bild_je_karte():
    e = _emu(konfig={"bildschirm": "beide", "schalter": "k7023"})
    assert e.bildschirm_schalter == "k7023"
    assert e.framebuffer_size() == (512, 256)
    # Schreiben in FC00H trifft beide Karten; screen_text folgt der gezeigten Karte.
    e.mem_write(0xFC00, ord("A"))
    zeilen = e.screen_text().split("\n")
    assert len(zeilen) == 16 and all(len(z) == 64 for z in zeilen) and zeilen[0][0] == "A"
    e.bildschirm_schalter = "k7024"
    zeilen = e.screen_text().split("\n")
    assert len(zeilen) == 24 and all(len(z) == 80 for z in zeilen)
    assert zeilen[12][64] == "A"          # FC00H = Zeile 12, Spalte 64 der K7024


def test_einzeloptionen_und_huelle():
    e = _emu(konfig={"ram": "k3525_4", "bildschirm": "k7023", "tastatur": "k7636",
                     "laderom": "v06", "kassette": True})
    assert (e.bildschirm, e.tastatur, e.laderom) == ("k7023", "k7636", "v06")
    assert e.tape_installed()
    # Huelle zuerst, Einzelwerte darueber: alt + K7637 + beide Karten.
    e = _emu(konfig={"tastatur": "k7637", "ausfuehrung": "alt", "bildschirm": "beide"})
    assert (e.ausfuehrung, e.tastatur, e.bildschirm) == ("alt", "k7637", "beide")


def test_laufwerke_und_em():
    e = _emu(konfig={"em": "em256"}, drive_types=["K5601", "none"])
    assert e.framebuffer_size() == (640, 288)
    assert e.drive_default_format(1) == ""


# ── Fehlerfaelle ─────────────────────────────────────────────────────────────

@pytest.mark.parametrize("konfig", [
    {"ram": "k3525_8"}, {"bildschirm": "drei"}, {"schalter": "beide"}, {"tastatur": "k7672"},
    {"laderom": "v07"}, {"kassette": "vielleicht"}, {"em": "em512"}, {"farbe": "gruen"},
])
def test_bindung_prueft_schluessel_und_werte(konfig):
    with pytest.raises(ValueError):
        _emu(konfig=konfig)


@pytest.mark.parametrize("kw", [
    dict(ausfuehrung="alt"), dict(tastatur="k7636"), dict(laderom="v06"), dict(em="em256"),
    dict(tape=True), dict(machine="a5120-alt"), dict(machine="k8915"),
])
def test_konfig_nicht_neben_den_alten_parametern(kw):
    with pytest.raises(ValueError):
        _emu(konfig={"ram": "k3526"}, **kw)


def test_kern_lehnt_unzulaessige_kombinationen_ab():
    # Schalter ohne zwei Karten und EM mit K3525 pruefen erst Kern bzw. Maschine.
    with pytest.raises(RuntimeError, match="beide"):
        _emu(konfig={"schalter": "k7023"})
    with pytest.raises(RuntimeError):
        _emu(konfig={"ram": "k3525_4", "em": "em256"})


def test_c_abi_text_mit_grund_und_toleranz():
    lib = _lib()
    for text in (b"ram=k3525_8", b"bildschirm", b"=k7024", b"ram=", b"farbe=gruen",
                 b"schalter=k7023", b"ausfuehrung=acht"):
        h = lib.k1520_create_a5120_text(text, None, None, None, None)
        assert not h, text
        assert lib.k1520_last_init_error(), text
    # Gross-/Kleinschreibung, Leerzeichen, beide Trenner, leere Teile.
    h = lib.k1520_create_a5120_text(b" RAM = K3525_2 , Bildschirm=Beide ;; ", None, None, None, None)
    try:
        assert h
        assert lib.k1520_a5120_bildschirm(h) == 2
        assert lib.k1520_screen_selected(h) == 0
        assert lib.k1520_screen_select(h, 1) == 0
        assert (lib.k1520_screen_selected(h), lib.k1520_fb_width(h), lib.k1520_fb_height(h)) == (1, 512, 256)
        assert lib.k1520_screen_select(h, 2) == -1
    finally:
        lib.k1520_destroy(h)
    h = lib.k1520_create_a5120_text(None, None, None, None, None)     # NULL = Vorgabe
    try:
        assert h and lib.k1520_a5120_bildschirm(h) == 0
        assert lib.k1520_screen_select(h, 1) == -1                     # kein Umschalter
        assert lib.k1520_screen_selected(h) == 0
    finally:
        lib.k1520_destroy(h)


def test_andere_maschinen_melden_minus_eins():
    e = _emu(machine="k8915")
    lib = _lib()
    assert lib.k1520_a5120_bildschirm(e._handle) == -1
    assert lib.k1520_screen_selected(e._handle) == -1
    assert lib.k1520_screen_select(e._handle, 0) == -1
    assert e.bildschirm == "" and e.bildschirm_schalter == ""
