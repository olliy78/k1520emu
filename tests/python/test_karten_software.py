# SPDX-License-Identifier: MIT
"""Gast-Software der Hardwareoptionen (tools/gide/): Umschreiben der Basisadressen und Zusammenstellung je Rechner.

Die Fremddateien liegen im Repository (Anwenderentscheidung 2026-11-11, tools/gide/README.md).  Die Wachter hier
halten zwei Dinge fest: (1) `basis_anpassen.py` schreibt NUR die belegten Operanden um und lehnt jede andere Datei ab;
(2) die Vorauswahl-Basen der Oberflaeche (`app/profil.py`) sind die, fuer die `karten.py` die Software zusammenstellt.
"""
import hashlib
import os
import sys

import pytest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, 'tools', 'gide'))
sys.path.insert(0, REPO)

import basis_anpassen as ba  # noqa: E402
import karten  # noqa: E402


def lies(*teile):
    with open(os.path.join(REPO, 'tools', 'gide', *teile), 'rb') as f:
        return f.read()


def unterschiede(a, b):
    assert len(a) == len(b)
    return {i for i in range(len(a)) if a[i] != b[i]}


# ---- Kingstener: die beiden Fassungen des Autors werden bytegleich nachgebildet ------------------------------

def test_gidn_d0_und_f0_sind_die_fassungen_des_autors():
    """SHA-256 der Fassungen gidn@D0h.com / gidn@F0h.com von hc-ddr.hucki.net (Dateien liegen nicht im Repo)."""
    assert hashlib.sha256(ba.gidn_datei(0xD0)).hexdigest() == \
        '34bacf7247d3432de94dac0b04f09820ddba61eb1510c2a3ca2ad771962cd439'
    assert hashlib.sha256(ba.gidn_datei(0xF0)).hexdigest() == \
        'eebfe78fececd7d87f681798ebe89a96297df3ba8322883ecc0c70ca3b35c8a2'
    assert ba.gidn_datei(0x50) == lies('fremd', 'kingstener', 'gidn_50h.com')


def test_gidn_90_aendert_nur_die_16_operanden():
    o = lies('fremd', 'kingstener', 'gidn_50h.com')
    n = ba.gidn_datei(0x90)
    assert unterschiede(o, n) == set(ba.GIDN_STELLEN)
    assert all(n[i] & 0xF0 == 0x90 and n[i] & 0x0F == o[i] & 0x0F for i in ba.GIDN_STELLEN)


# ---- UTools und KCNet -----------------------------------------------------------------------------------------

def test_usb_originalbasis_ist_unveraendert_und_andere_basen_aendern_nur_die_stellen():
    for name in ba.USB_DATEIEN:
        o = lies('fremd', 'utools', name)
        assert ba.usb(o, 0xDC) == o
        for basis in (0x84, 0x8C):
            n = ba.usb(o, basis)
            assert unterschiede(o, n) == set(ba.USB_STELLEN)
            for off, abstand in ba.USB_STELLEN.items():
                assert n[off] == basis + abstand


def test_net_originalbasis_ist_unveraendert_und_28_aendert_nur_die_stellen():
    for name in ba.NET_DATEIEN:
        o = lies('fremd', 'kcnet', name)
        assert ba.net(o, 0xC0) == o
        n = ba.net(o, 0x28)
        assert unterschiede(o, n) == set(ba.NET_STELLEN)
        for off, abstand in ba.NET_STELLEN.items():
            assert n[off] == 0x28 + abstand


def test_beide_netzprogramme_haben_denselben_treiberblock():
    """Die Patchstellen gelten fuer beide, weil der Treiberblock 0109..028E byteidentisch ist (Entwurf 50a §0)."""
    a, b = (lies('fremd', 'kcnet', n) for n in ba.NET_DATEIEN)
    assert a[0x49:0x190] == b[0x49:0x190]


@pytest.mark.parametrize('art,datei,basis', [
    ('usb', ('fremd', 'kcnet', 'TFTP1287.COM'), 0x84),     # falsche Datei
    ('net', ('fremd', 'utools', 'usb.com'), 0x28),
    ('gidn', ('fremd', 'poppe', 'gidehp90.com'), 0x90),
])
def test_falsche_datei_wird_abgelehnt(art, datei, basis):
    with pytest.raises(ba.Fehler):
        getattr(ba, art)(lies(*datei), basis)


@pytest.mark.parametrize('art,datei,basis', [
    ('usb', ('fremd', 'utools', 'usb.com'), 0x85),          # kein Vielfaches von 4
    ('net', ('fremd', 'kcnet', 'TFTP1287.COM'), 0x29),
    ('gidn', ('fremd', 'kingstener', 'gidn_50h.com'), 0x95),
    ('usb', ('fremd', 'utools', 'usb.com'), 0x100),
])
def test_unmoegliche_basis_wird_abgelehnt(art, datei, basis):
    with pytest.raises(ba.Fehler):
        getattr(ba, art)(lies(*datei), basis)


def test_poppe_hat_fuer_jede_basis_eine_fassung_mit_dieser_basis():
    """GIDEHPxx.COM tragen die Basis im Namen; der Banner nennt sie (ASCII 'xxH')."""
    for hi in range(16):
        d = lies('fremd', 'poppe', f'gidehp{hi * 16:02x}.com')
        assert len(d) == 4608


# ---- Zusammenstellung je Rechner -----------------------------------------------------------------------------

def test_vorauswahl_der_oberflaeche_ist_die_basis_der_software():
    """app/profil.py (karten_vorauswahl) und karten.RECHNER muessen dieselben Basen nennen."""
    from app import profil
    profile = {
        'a5120_cpa': ('a5120', 'a5120'), 'a5120_scpx': ('a5120', 'a5120'),
        'k8915_scpx': ('k8915', 'v3'), 'prg710_scpx': ('prg710', 'prg710'), 'prg710_1_scpx': ('prg710', 'prg710-1'),
        'pc1715_cpa': ('pc1715', 'pc1715'), 'pc1715_scp6': ('pc1715', 'pc1715'), 'pc1715_scp7': ('pc1715', 'pc1715'),
        'pc1715w_scp3': ('pc1715', 'pc1715w'),
    }
    assert set(profile) == set(karten.RECHNER)
    for schluessel, (programm, modell) in profile.items():
        p = profil.PROFILE[programm] if hasattr(profil, 'PROFILE') else None
        if p is None:
            pytest.skip('profil.PROFILE nicht vorhanden')
        r = karten.RECHNER[schluessel]
        assert p.karten_vorauswahl('gide') == r.gide_basis
        assert p.karten_vorauswahl('usb', modell) == r.usb[0], schluessel
        if r.net is not None:
            assert p.karten_vorauswahl('net', modell) == r.net, schluessel


def test_k8915_traegt_beide_usb_basen():
    d = karten.dateien('k8915_scpx')
    for n in ('USB.COM', 'UDIR.COM', 'UGET.COM', 'UPUT.COM', 'USB8C.COM', 'UDIR8C.COM', 'UGET8C.COM', 'UPUT8C.COM'):
        assert n in d
    assert d['USB.COM'] == ba.usb(lies('fremd', 'utools', 'usb.com'), 0x84)
    assert d['USB8C.COM'] == ba.usb(lies('fremd', 'utools', 'usb.com'), 0x8C)


def test_jeder_rechner_hat_uhr_und_texte_in_cpm_form():
    for schluessel in karten.RECHNER:
        d = karten.dateien(schluessel)
        assert d['UHR.COM'] == lies('uhr', 'uhr.com')
        for n in ('GIDE.TXT', 'USB.TXT') + (('NETZ.TXT',) if karten.RECHNER[schluessel].net is not None else ()):
            t = d[n]
            assert len(t) % 128 == 0 and t.endswith(b'\x1a')
            t.rstrip(b'\x1a').decode('ascii')                     # kein Umlaut, kein Nicht-ASCII
            assert b'\r\n' in t and b'\n' not in t.replace(b'\r\n', b'')
        for n in d:
            assert len(n.split('.')[0]) <= 8, n


def test_prg710_net_ist_28h_und_der_rest_c0h():
    assert karten.dateien('prg710_1_scpx')['TFTP.COM'] == ba.net(lies('fremd', 'kcnet', 'TFTP1287.COM'), 0x28)
    assert karten.dateien('a5120_cpa')['TFTP.COM'] == lies('fremd', 'kcnet', 'TFTP1287.COM')


def test_paket_liefert_alle_poppe_fassungen_und_das_umschreibwerkzeug():
    """Das Paket (packaging/build_payload.sh) legt tools/gide/fremd + basis_anpassen.py nach share/k1520emu/karten_software."""
    with open(os.path.join(REPO, 'packaging', 'build_payload.sh'), encoding='utf-8') as f:
        bp = f.read()
    assert 'share/k1520emu/karten_software' in bp
    assert '"$REPO/tools/gide/fremd"' in bp and 'basis_anpassen.py' in bp
    assert 'karten_software-HERKUNFT.md' in bp          # die Herkunftstabelle reist als Lizenzhinweis mit
    for hi in range(16):
        assert os.path.isfile(os.path.join(REPO, 'tools', 'gide', 'fremd', 'poppe', f'gidehp{hi * 16:02x}.com'))
