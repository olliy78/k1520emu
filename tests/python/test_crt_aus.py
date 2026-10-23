"""CRT-Gesamtschalter und Bildrate der Anzeige (Ticket #19): Konfiguration und Takt."""
from app.ui.screen_widget import CRTParams


def test_crt_aus_und_bildrate_gehen_durch_die_konfiguration():
    p = CRTParams()
    assert p.crt_on is True and p.bildrate == 30.0      # Vorgabe: Roehre an, 30 Bilder/s
    p.crt_on, p.bildrate = False, 60.0
    q = CRTParams()
    q.update_from_dict(p.to_dict())
    assert q.crt_on is False and q.bildrate == 60.0


def test_alte_konfiguration_ohne_neue_schluessel_bleibt_gueltig():
    q = CRTParams()
    q.update_from_dict({"brightness": 1.0})
    assert q.crt_on is True and q.bildrate == 30.0


def test_die_zeichenflaeche_wechselt_zur_laufzeit_mit_crt_on(qapp):
    """crt_on schaltet zwischen GL-Flaeche (CRT-Shader) und Qt-Flaeche; die alte wird freigegeben,
    Bildzustand und Eingabe bleiben beim ScreenWidget."""
    from app.ui.screen_widget import ScreenWidget, _GlFlaeche, _QtFlaeche
    w = ScreenWidget()
    try:
        assert isinstance(w._flaeche, _GlFlaeche)
        w._fb_bytes = bytes(640 * 288)
        for an, art in ((False, _QtFlaeche), (True, _GlFlaeche), (False, _QtFlaeche)):
            w.params.crt_on = an
            w.parameter_geaendert()
            assert isinstance(w._flaeche, art)
            assert w._pending_upload                       # die neue Flaeche bekommt das Bild neu
        qapp.processEvents()
        assert sum(isinstance(c, (_GlFlaeche, _QtFlaeche)) for c in w.children()) == 1
        assert w.focusPolicy() != 0 and w._flaeche.focusPolicy() == 0   # Fokus bleibt beim ScreenWidget
    finally:
        w.close()
        qapp.processEvents()


def test_ohne_crt_sind_die_nur_shader_regler_ausgegraut(qapp):
    """Kontrast, Kruemmung X/Y, Ecken-Rundung und Vignette wirken nur im CRT-Shader."""
    from PySide6.QtWidgets import QDoubleSpinBox
        # Aufbau wie in den anderen GUI-Tests des Reiters: ueber das Hauptfenster
    from app import profil
    from app.ui.main_window import MainWindow
    w = MainWindow(None, profil=profil.profil("a5120"))
    try:
        sw, screen = w.settings_widget, w.screen_widget
        spins = sw.findChildren(QDoubleSpinBox)             # Reihenfolge wie im CRT-Reiter
        namen = ["brightness", "contrast", "curv_x", "curv_y", "corner", "sx", "sy", "ox", "oy"]
        assert len(spins) == len(namen)
        screen.params.crt_on = False
        sw._apply()
        frei = {n for n, s in zip(namen, spins) if s.isEnabled()}
        assert frei == {"brightness", "sx", "sy", "ox", "oy"}
        screen.params.crt_on = True
        sw._apply()
        assert all(s.isEnabled() for s in spins)
    finally:
        w.close()
        qapp.processEvents()
