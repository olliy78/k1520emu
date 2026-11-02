"""MicroDOS 1715 am PC 1715: Dienstprogramme, Druckerwege, 1K-Generierung am K7221 (Merkposten microdos1715.md).

Jeder Fall prueft die im Handbuch (mdosb15.txt, msysgen.txt, mkey.txt) beschriebene Wirkung.  Systemdisketten, die abweichende
Hardware brauchen, entstehen wie beim Anwender (FORMAT15 -> MSYSG15A auf einer Leerdiskette) und werden mit dem DiskTool um die
Programme der Originaldiskette ergaenzt.
"""
import os
import shutil
import subprocess
import sys
import time

import pytest

from conftest import PROJECT_ROOT, requires_core

pytestmark = requires_core

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf1715  # noqa: E402

ORIGINAL = PROJECT_ROOT / "disks" / "pc1715_microdos" / "pc1715_microdos_k5601_system.hfe"
DISKTOOL = PROJECT_ROOT / "build" / "k1520disktool"
braucht_disktool = pytest.mark.skipif(not DISKTOOL.exists(), reason="build/k1520disktool fehlt")


def _disktool(*args):
    env = dict(os.environ, LD_LIBRARY_PATH=str(PROJECT_ROOT / "build"))
    r = subprocess.run([str(DISKTOOL), *map(str, args)], capture_output=True, env=env)
    assert r.returncode == 0, r.stdout.decode(errors="replace") + r.stderr.decode(errors="replace")


def _generiere(tmp_path, name, bildschirm="2", drucker="2"):
    """Leerdiskette -> FORMAT15 -> MSYSG15A (auf dem K7222, die Generierung haengt nicht am Bildschirm der Sitzung)."""
    a = tmp_path / "quelle.hfe"
    b = tmp_path / (name + ".hfe")
    shutil.copy(ORIGINAL, a)
    s = ablauf1715.Sitzung(str(a), str(b))
    s.kaltstart_bis_prompt()
    ablauf1715.formatiere(s)
    ablauf1715.generiere(s, bildschirm=bildschirm, drucker=drucker)
    s.lauf(10_000_000)
    s.emu.flush_disks()
    return b


def _programme_auf(pfad, tmp_path, *namen):
    ordner = tmp_path / "prog"
    ordner.mkdir(exist_ok=True)
    _disktool("get", ORIGINAL, *namen, "--to", ordner)
    _disktool("put", pfad, *[ordner / n for n in namen])


def _boot(pfad, tmp_path, modell="pc1715", warte=True):
    a = tmp_path / ("boot_%s.hfe" % modell)
    shutil.copy(pfad, a)
    s = ablauf1715.Sitzung(str(a), modell=modell)
    if warte:
        s.kaltstart_bis_prompt()
    return s


def _dateien_seriell(s, tmp_path):
    from app.core_binding import k1520
    for i in range(s.emu.serial_count()):
        s.emu.serial_stop(i)
    pfade = []
    for i in range(s.emu.serial_count()):
        p = tmp_path / ("ser%d.txt" % i)
        p.write_bytes(b"")
        assert s.emu.serial_configure(i, betriebsart=k1520.SER_DATEI, datei=str(p), loop=False)
        assert s.emu.serial_start(i)
        pfade.append(p)
    return pfade


def _lesen(pfad, mindestens=1, frist=3.0):
    ende = time.time() + frist
    while time.time() < ende:
        if len(pfad.read_bytes()) >= mindestens:
            time.sleep(0.2)
            break
        time.sleep(0.05)
    return pfad.read_bytes()


def _warte_auf(pfad, muster, frist=4.0):
    ende = time.time() + frist
    while time.time() < ende:
        d = pfad.read_bytes()
        if muster in d:
            return d
        time.sleep(0.05)
    return pfad.read_bytes()


def _starten(s, text, takte=20_000_000):
    s.tippe(text + "\r")
    s.lauf(takte)
    return s.bild()


# ── SELDSK15 ─────────────────────────────────────────────────────────────────

def _seldsk15(s, einheit, typ, format_nr):
    s.tippe("SELDSK15\r")
    for erwartet, antwort in (("logische Laufwerk", "B"), ("physische", einheit), ("Laufwerkstyp", typ),
                              ("Formate sind", format_nr)):
        assert s.bis(erwartet), s.bild()
        s.tippe(antwort)
        s.lauf(3_000_000)
    assert s.bis("Wiederholung"), s.bild()
    s.tippe("N")
    assert s.bis_prompt(), s.bild()


def test_seldsk15_stellt_b_auf_das_gewaehlte_format(tmp_path):
    """Richtiges Format (7 = 5x1024x80x2) -> Verzeichnis lesbar; falsches (2 = 16x256x40x1) -> ID-Feld-Fehler ST=56."""
    s = ablauf1715.Sitzung(str(ORIGINAL), str(ORIGINAL))
    s.kaltstart_bis_prompt()
    _seldsk15(s, "2", "3", "7")
    assert "MICRODOS.SYS" in s.kommando("D B:")
    _seldsk15(s, "2", "3", "2")
    s.tippe("D B:\r")
    assert s.bis("Bdos error on B:", frist=100_000_000), s.bild()
    assert "ST= 56" in s.bild(), s.bild()


def test_seldsk15_nach_falschem_zugriff_bleibt_b_bis_zum_neustart_kaputt(tmp_path):
    """GEKLAERT (2026-11-01): ein Zugriff mit falschem Format (16x256x40x1 auf einer 5x1024x80x2-Diskette) endet mit ST=56
    ('ID nicht gefunden', BIOS-Marke EEH in F16F, EB3A) und laesst den Kopf von B: NICHT auf Spur 0 zurueck: /TO (PIO-B Bit 7)
    ist danach 1, der MicroDOS-BIOS faehrt bei Fehlern nie auf Spur 0 und merkt sich Spur 0.  Jeder weitere Zugriff liest
    daher im falschen Zylinder -- auch mit dem richtigen Format und nach ^C -- bis zum Neustart.  Gastverhalten (Kopf
    folgt exakt den Schrittimpulsen des BIOS); ohne vorherigen Zugriff ist das Umstellen beliebig oft moeglich."""
    s = ablauf1715.Sitzung(str(ORIGINAL), str(ORIGINAL))
    s.kaltstart_bis_prompt()
    _seldsk15(s, "2", "3", "2")
    _seldsk15(s, "2", "3", "7")                       # nur umstellen: unkritisch
    assert "MICRODOS.SYS" in s.kommando("D B:")
    _seldsk15(s, "2", "3", "2")
    s.tippe("D B:\r")
    assert s.bis("ignore - Y", frist=100_000_000), s.bild()
    s.tippe("N")
    assert s.bis_prompt(), s.bild()
    _seldsk15(s, "2", "3", "7")
    s.tippe("D B:\r")
    assert s.bis("Bdos error on B:", frist=100_000_000), s.bild()      # richtiges Format, trotzdem ST=56
    assert "ST= 56" in s.bild(), s.bild()


@braucht_disktool
def test_selprt_ueberschreibt_den_bildschirmtreiber_dieses_systems(tmp_path):
    """GEKLAERT (2026-11-01): SELPRT.COM der Sammlung kopiert nach der Typenradwahl 95 Byte Umsetzungstabelle nach E6FEH
    (LD DE,E6FEH / LDIR).  In dieser MICRODOS.SYS liegt dort Treibercode (Bildschirmtreiber E696ff), keine Tabelle: die CPU
    laeuft danach in die Tabelle (der Eintrag 76H = HALT bei E753) und steht -- 'Drucker bereit (ENTER)' erscheint nie.
    Unabhaengig vom Druckerweg (V.24 wie IFSS); SELPRT und MICRODOS.SYS der Sammlung gehoeren nicht zusammen."""
    ordner = tmp_path / "prog"
    ordner.mkdir()
    _disktool("get", ORIGINAL, "SELPRT.COM", "--to", ordner)
    com = (ordner / "SELPRT.COM").read_bytes()
    assert bytes.fromhex("11FEE6EDB0") in com                        # LD DE,E6FEH ; LDIR
    tabelle1 = com[0x2E4 - 0x100 + 0x5F: 0x2E4 - 0x100 + 0x5F + 16]
    s = ablauf1715.Sitzung(str(ORIGINAL))
    s.kaltstart_bis_prompt()
    ram = bytes(s.emu.mem_read(a) for a in range(0xE6C0, 0xE780))
    assert tabelle1 not in ram                                       # dort liegt keine Tabelle ...
    assert ram[0xE6FE - 0xE6C0] != tabelle1[0]                       # ... sondern Code
    s.tippe("SELPRT\r"); s.lauf(30_000_000)
    s.tippe("1"); s.lauf(30_000_000)
    s.tippe("\r"); s.lauf(80_000_000)
    assert "Drucker bereit" not in s.bild() and "Richtig" not in s.bild()
    nachher = bytes(s.emu.mem_read(a) for a in range(0xE6FE, 0xE6FE + 0x5F))
    assert nachher == com[0x2E4 - 0x100 + 0x5F: 0x2E4 - 0x100 + 2 * 0x5F]   # Tabelle 1 steht jetzt im Treiber


# ── KTP15, OC15, HARDCOPY, POWER, MKEYINST ───────────────────────────────────

def test_ktp15_zeigt_den_physischen_tastencode_und_endet_bei_a(tmp_path):
    s = ablauf1715.Sitzung(str(ORIGINAL))
    s.kaltstart_bis_prompt()
    s.tippe("KTP15\r"); s.lauf(15_000_000)
    s.tippe("b"); s.lauf(3_000_000); s.tippe("\r"); s.lauf(3_000_000); s.tippe("a"); s.lauf(5_000_000)
    zeilen = s.bild().split("\n")
    assert "E062" in zeilen and "E09E" in zeilen and zeilen[-1] == "A>", s.bild()      # b = 62H, Return = 9EH


def test_oc15_verweigert_auf_dieser_systemversion(tmp_path):
    """Gastverhalten: OC15 prueft die Systemkennung und meldet 'System und OC passen nicht zusammen !' (auch auf der
    Originaldiskette); die Grossschreibung ist an dieser Version nicht umschaltbar."""
    s = ablauf1715.Sitzung(str(ORIGINAL))
    s.kaltstart_bis_prompt()
    assert "System und OC passen nicht zusammen" in _starten(s, "OC15")


def test_hardcopy15_fragt_quelle_und_ziel_und_endet_mit_ctrl_c(tmp_path):
    s = ablauf1715.Sitzung(str(ORIGINAL))
    s.kaltstart_bis_prompt()
    assert "Source:" in _starten(s, "HARDCOPY", 10_000_000)
    s.strg("c"); s.lauf(10_000_000)
    assert s.bild().rstrip().endswith("A>"), s.bild()


@braucht_disktool
def test_mkeyinst_erzeugt_mkey_spr_aus_einer_eingabedatei(tmp_path):
    eingabe = tmp_path / "MK15"
    eingabe.write_bytes(b"MKEYINST\r\n&FE\r\n&A0 &0F\r\n&00\r\n\x1a")
    a = tmp_path / "a.hfe"
    shutil.copy(ORIGINAL, a)
    _disktool("put", a, eingabe)
    s = ablauf1715.Sitzung(str(a))
    s.kaltstart_bis_prompt()
    assert "MKEYINST Vers. 2.0" in _starten(s, "<MK15", 40_000_000)
    assert "MKEY    .SPR" in s.kommando("D")


def test_power_listet_das_verzeichnis(tmp_path):
    s = ablauf1715.Sitzung(str(ORIGINAL))
    s.kaltstart_bis_prompt()
    _starten(s, "POWER")
    bild = _starten(s, "DIR", 10_000_000)
    assert "POWER 3.03" not in bild.split("DIR")[-1] and "MICRODOS.SYS" in bild and "SELDSK15" in bild, bild


# ── Drucker: V.24 / IFSS ─────────────────────────────────────────────────────

@braucht_disktool
def test_drucker_v24_ctrl_p_und_k6311sel_senden_an_die_schnittstelle_drucker(tmp_path):
    """MSYSG15A Drucker 1 (V.24, DTR-Protokoll): ^P schickt die Konsolenausgabe an 'Drucker' X4 (SIO0-A); K6311SEL sendet
    die Steuerfolge fuer Zeichensatz 2 und Breite 1/10 Zoll (ESC R 2, ESC [ 0 SP K)."""
    pfad = _generiere(tmp_path, "v24", drucker="1")
    _programme_auf(pfad, tmp_path, "K6311SEL.COM")
    s = _boot(pfad, tmp_path)
    ser = _dateien_seriell(s, tmp_path)
    s.strg("p"); s.lauf(3_000_000)
    s.kommando("D")
    s.strg("p"); s.lauf(3_000_000)
    daten = _lesen(ser[0], 25)
    assert daten.startswith(b"D\r") and b"MICRODOS.SYS" in daten, daten
    assert not ser[1].read_bytes()
    s.tippe("K6311SEL\r")
    assert s.bis("Zeichensatz aus")
    s.tippe("2\r")
    assert s.bis("Zeichenbreite aus")
    s.tippe("1\r")
    assert s.bis_prompt()
    assert b"\x1bR\x02\x1b[0 K" in _warte_auf(ser[0], b"\x1bR\x02\x1b[0 K")


@braucht_disktool
def test_drucker_ifss_am_pc1715_ist_nicht_modelliert(tmp_path):
    """NICHT MODELLIERT: der IFSS-Druckertreiber spricht die Zusatzkarte (2 x IFSS / 2 x V.24, E/A 10H..17H, Entwurf 21:
    'keine Zusatzkarten').  Er liest 14H/16H (unbelegt = FFh = bereit) und schreibt 14H; es kommt nichts an."""
    pfad = _generiere(tmp_path, "ifss", drucker="2")
    s = _boot(pfad, tmp_path)
    ser = _dateien_seriell(s, tmp_path)
    s.strg("p"); s.lauf(3_000_000)
    s.kommando("D")
    s.strg("p"); s.lauf(10_000_000)
    assert all(not _lesen(p, 1, 0.3) for p in ser)
    assert "MICRODOS.SYS" in s.kommando("D")


# ── K7221 (64 x 16) mit 1K-Generierung ───────────────────────────────────────

def test_1k_generierung_laeuft_am_k7221_ohne_umbruch_bei_spalte_64(tmp_path):
    """MSYSG15A Bildschirm 1 (1K): Banner und Menues stehen im 64-Spalten-Bild ungebrochen; die 2K-Originaldiskette bricht
    dort bei Spalte 64 um ('Ver/s 2.6')."""
    pfad = _generiere(tmp_path, "k1k", bildschirm="1")
    s = _boot(pfad, tmp_path, modell="pc1715-k7221", warte=False)
    assert s.bis("MicroDOS Vers 2.6", frist=400_000_000), s.bild()
    zeilen = s.bild().split("\n")
    assert any(z.strip() == "48K MicroDOS Vers 2.6" for z in zeilen), s.bild()
    assert not any(z.rstrip().endswith("MicroDOS Ver") for z in zeilen), s.bild()
    s.lauf(150_000_000)
    s.tippe("D\r")
    s.lauf(30_000_000)
    assert "A:MICRODOS.SYS  S" in s.bild()
    # zum Vergleich die 2K-Diskette: gebrochener Banner
    o = _boot(ORIGINAL, tmp_path, modell="pc1715-k7221", warte=False)
    assert o.bis("MicroDOS Ver", frist=400_000_000)
    assert any(z.rstrip().endswith("MicroDOS Ver") for z in o.bild().split("\n")), o.bild()   # "...Ver" / "s 2.6"
