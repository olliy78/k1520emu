"""Basiswahl der Steckkarten (app/kartenwahl.py, Profil ``karten_vorauswahl``; Entwurf 50 G6).

Waechter: die VORAUSWAHL von GIDE, USB und Netz ist an JEDER Maschine/jedem Modell untereinander und mit
RAF, Kassette K5020 und Lochstreifen K6022 (und den festen Karten der Maschine, die der Kern kennt)
disjunkt -- der Kern steckt alle zugleich, ohne einen Konflikt zu melden.  Dazu die Besitzerabfrage
(``k1520_io_owner``) und die Kandidatenliste.
"""

import pytest

from conftest import requires_core

pytestmark = requires_core

PROGRAMME = ["a5120", "k8915", "prg710", "pc1715"]
VORAUSWAHL = {            # (gide, usb, net) je Programm/Modell laut Anwenderentscheid
    ("a5120", None): (0x90, 0x84, 0xC0),
    ("k8915", "k8915"): (0x90, 0x84, 0xC0),
    ("k8915", "k8915-g2"): (0x90, 0x8C, 0xC0),
    ("prg710", None): (0x90, 0x8C, 0x28),
    ("pc1715", None): (0x90, 0x84, 0xC0),
}


def _faelle():
    from app import profil
    for name in PROGRAMME:
        p = profil.profil(name)
        for m in [x[0] for x in p.modelle] or [p.standard_modell()]:
            yield p, m


def _id(f):
    return f"{f[0].maschine}-{f[1]}"


@pytest.mark.parametrize("fall", list(_faelle()), ids=_id)
def test_vorauswahl_laut_tabelle(fall):
    p, m = fall
    erwartet = VORAUSWAHL.get((p.maschine, m)) or VORAUSWAHL[(p.maschine, None)]
    assert (p.karten_vorauswahl("gide", m), p.karten_vorauswahl("usb", m),
            p.karten_vorauswahl("net", m)) == erwartet


@pytest.mark.parametrize("fall", list(_faelle()), ids=_id)
@pytest.mark.parametrize("em", [None, "em064", "em256", "em1024"])
def test_vorauswahl_kollidiert_nirgends(fall, em):
    """Alle drei Karten + RAF + Kassette + Lochstreifen zugleich, ohne gemeldeten Konflikt."""
    from app.core_binding.k1520 import K1520Emulator
    p, m = fall
    if em and p.maschine != "a5120":
        pytest.skip("Erweiterungsmodul gibt es nur am A5120")
    kern = dict(p.kern_parameter(m, p.hardware_standard(m), "k7024"))
    extra = {}
    if "konfig" in kern:
        kern["konfig"] = {**kern["konfig"], "kassette": True, **({"em": em} if em else {})}
    else:
        extra["tape"] = True
    emu = K1520Emulator(machine=p.modell_maschine(m), raf="raf2m", ptape=True,
                        gide={"basis": p.karten_vorauswahl("gide", m)},
                        usb={"basis": p.karten_vorauswahl("usb", m)},
                        net={"basis": p.karten_vorauswahl("net", m)}, **extra, **kern)
    assert emu.gide_installed() and emu.usb_installed() and emu.net_installed()
    assert emu.tape_installed() and emu.ptape_installed() and emu.raf_variant


def test_besitzerabfrage_und_kandidatenliste():
    from app import kartenwahl
    from app.core_binding.k1520 import K1520Emulator
    emu = K1520Emulator(machine="k8915", tape=True, raf="raf512", gide={"basis": 0x90})
    assert emu.io_owner(0xD0).startswith("K5020")
    assert emu.io_owner(0x88) == "RAF"
    assert emu.io_owner(0x95) == "GIDE" and emu.io_owner(0x01) == ""
    assert emu.io_owner(999) == ""
    zeilen = {z.basis: z for z in kartenwahl.eintraege(kartenwahl.GIDE, emu.io_owner)}
    assert len(zeilen) == 16
    assert "K5020" in zeilen[0xD0].belegt and zeilen[0xD0].text.startswith("D0H (belegt: K5020")
    assert zeilen[0x90].belegt == ""                        # die eigenen Tore zaehlen nicht
    assert zeilen[0x80].belegt == "RAF"                     # Basis 80H belegt 84H..8FH, darin 88H/89H
    usb = {z.basis: z for z in kartenwahl.eintraege(kartenwahl.USB, emu.io_owner)}
    assert len(usb) == 64 and usb[0x84].text.startswith("84H (GUN)")
    assert usb[0x88].belegt == "RAF" and usb[0xD4].belegt.startswith("K5020")
    net = {z.basis: z for z in kartenwahl.eintraege(kartenwahl.NETZ, emu.io_owner)}
    assert net[0xC0].text.startswith("C0H (K1520-Netzkarte)") and net[0x28].text.startswith("28H (K1520-Netzkarte)")
    assert net[0x88].text.startswith("88H (GUN)")


def test_konflikt_zwischen_gewaehlten_karten():
    from app import kartenwahl as k
    # GIDE 80H belegt 84H..8FH, USB 84H liegt darin; GIDE 90H und USB 84H sind frei voneinander.
    assert k.konflikt_mit_anderen(k.USB, 0x84, {"gide": 0x80}) == "GIDE"
    assert k.konflikt_mit_anderen(k.USB, 0x84, {"gide": 0x90}) == ""
    assert k.konflikt_mit_anderen(k.NETZ, 0xC0, {"usb": 0xC0, "gide": 0x90}) == "USB"
    assert k.konflikt_mit_anderen(k.NETZ, 0xC0, {"net": 0xC0}) == ""          # sich selbst nie
    z = {e.basis: e for e in k.eintraege(k.GIDE, None, {"usb": 0x84})}
    assert z[0x80].belegt == "USB" and "belegt: USB" in z[0x80].text and z[0x90].belegt == ""
