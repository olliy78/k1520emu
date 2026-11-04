"""MicroDOS 1526 am A5120: die Dienstprogramme und residenten Kommandos wirklich bedienen (Merkposten microdos.md, Abschnitt
"Bedienung der Programme").  Jeder Fall prueft die im Handbuch (mdosb20.txt, mkey.txt, msysgen.txt) beschriebene WIRKUNG,
nicht nur dass das Programm startet.  Grundlage: die ausgelieferten Disketten disks/a5120_microdos1526/ (Temp-Kopien).
"""
import os
import shutil
import subprocess
import sys

import pytest

from conftest import PROJECT_ROOT, requires_core

pytestmark = requires_core

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf  # noqa: E402

ORDNER = PROJECT_ROOT / "disks" / "a5120_microdos1526"
SERIELL = ORDNER / "a5120_microdos26_k5601_seriell.hfe"
PARALLEL = ORDNER / "a5120_microdos26_k5601_parallel.hfe"
DISKTOOL = PROJECT_ROOT / "build" / "k1520disktool"

QT_RETURN = 0x01000004
RAW = 0x02000000                       # K7637: physische Taste (Funktionstasten) = RAW | Code

braucht_disktool = pytest.mark.skipif(not DISKTOOL.exists(), reason="build/k1520disktool fehlt")


def _disktool(*args):
    env = dict(os.environ, LD_LIBRARY_PATH=str(PROJECT_ROOT / "build"))
    r = subprocess.run([str(DISKTOOL), *map(str, args)], capture_output=True, env=env)
    assert r.returncode == 0, r.stdout.decode(errors="replace") + r.stderr.decode(errors="replace")


def _sitzung(tmp_path, a=SERIELL, b=None, tastatur="k7637", laufwerke=None, dateien=(), kopie_b=True):
    """Temp-Kopie von A: (und B:) einlegen, booten.  ``dateien`` werden vorher mit dem DiskTool auf A: gelegt."""
    tmp_path.mkdir(parents=True, exist_ok=True)
    a_kopie = tmp_path / "a.hfe"
    shutil.copy(a, a_kopie)
    for d in dateien:
        _disktool("put", a_kopie, d)
    s = ablauf.Sitzung(tastatur, str(a_kopie), laufwerke=laufwerke)
    if b is not None:
        b_kopie = tmp_path / "b.hfe"
        shutil.copy(b, b_kopie)
        assert s.emu.mount_disk(1, str(b_kopie), "cpa800", False), s.emu.last_error()
    s.kaltstart_bis_prompt()
    return s


def _ctrl(s, buchstabe):
    """Steuerzeichen (Strg+Buchstabe) an der K7637."""
    code = ord(buchstabe.lower())
    s.emu.key_press(code, False, True)
    s.lauf(150_000)
    s.emu.key_release(code)
    s.lauf(600_000)


def _raw(s, code):
    s.emu.key_press(RAW | code)
    s.lauf(150_000)
    s.emu.key_release(RAW | code)
    s.lauf(600_000)


def _kommando(s, text, takte=30_000_000):
    s.tippe(text + "\r")
    s.lauf(takte)
    return s.bild()


# ── SELDSK20 ─────────────────────────────────────────────────────────────────

def test_seldsk20_schaltet_das_format_von_b_wirklich_um(tmp_path):
    """Falsches Format (16x256x80x2) -> ID-Feld-Fehler ST=56 auf B:, richtiges (5x1024x80x2) -> Verzeichnis lesbar."""
    s = _sitzung(tmp_path, b=PARALLEL)
    assert "MicroDOS.SYS" in s.kommando("D B:")

    def seldsk(format_nr):
        s.tippe("SELDSK20\r")
        assert s.bis("Welches logische")
        s.tippe("B"); s.lauf(3_000_000)
        assert s.bis("physische"); s.tippe("2"); s.lauf(3_000_000)
        assert s.bis("Laufwerkstyp"); s.tippe("3"); s.lauf(3_000_000)
        assert s.bis("Formate sind"); s.tippe(format_nr); s.lauf(3_000_000)
        assert s.bis("Wiederholung"); s.tippe("N")
        assert s.bis_prompt()

    seldsk("7")                                      # 16 x 256 x 80 x 2
    bild = _kommando(s, "D B:")
    assert "Bdos error on B:" in bild and "ST= 56" in bild, bild
    s.tippe("n"); s.lauf(5_000_000)
    assert s.bis_prompt()
    seldsk("8")                                      # zurueck auf 5 x 1024 x 80 x 2
    assert "MicroDOS.SYS" in s.kommando("D B:")


def test_seldsk20_und_formatmenues_der_laufwerkstypen(tmp_path):
    """Das Typmenue nennt die fuenf Laufwerkstypen des Handbuchs, die Formatliste haengt vom Typ ab."""
    s = _sitzung(tmp_path)
    s.tippe("SELDSK20\r")
    assert s.bis("Welches logische"); s.tippe("A"); s.lauf(3_000_000)
    assert s.bis("physische"); s.tippe("1"); s.lauf(3_000_000)
    assert s.bis("Laufwerkstyp")
    bild = s.bild()
    for text in ("MFS 1.2", "MFS 1.4", "MFS 1.6", "MF 3200", "MF 6400"):
        assert text in bild, bild
    s.tippe("4"); s.lauf(3_000_000)                  # MF 3200: nur zwei Formate
    assert s.bis("Formate sind")
    bild = s.bild()
    assert "26 *  128 * 77 * 1" in bild and "4 * 1024 * 77 * 1" in bild and "5 - " not in bild, bild


# ── residente Kommandos (mdosb20.txt Abschnitt 4) ───────────────────────────

@braucht_disktool
def test_offset_systemlaufwerk_benutzerbereich_und_eingabeumlenkung(tmp_path):
    cmd = tmp_path / "CMD.SUB"
    cmd.write_bytes(b"; Kommentar\r\nD B:\r\nO\r\n\x1a")
    s = _sitzung(tmp_path, b=PARALLEL, dateien=[cmd])
    # O d:n -- der Offset verschiebt das Verzeichnis; O d:0 stellt es zurueck
    s.kommando("O B:2")
    assert "no files" in s.kommando("D B:")
    s.kommando("O B:0")
    assert "MicroDOS.SYS" in s.kommando("D B:")
    assert "B: 2" not in s.kommando("O") and "A: 0" in s.bild()
    # U n: der Benutzerbereich steht im Prompt, in Bereich 3 ist nichts
    s.tippe("U 3\r"); s.lauf(10_000_000)
    assert s.letzte_zeile() == "3A>", s.bild()
    assert "no files" in _kommando(s, "D", 10_000_000)
    s.tippe("U 0\r"); s.lauf(10_000_000)
    # <datei: Kommandodatei, Kommentarzeilen (;) loesen nichts aus
    bild = s.kommando("<CMD.SUB")
    assert "; Kommentar" in bild and "B:MicroDOS.SYS" in bild and "A: 0" in bild, bild
    assert "?" not in bild.split("<CMD.SUB")[-1], bild
    # ohne Typ gibt es die Datei nicht (der Name ist massgeblich, nicht nur der Stamm)
    assert "CMD?" in s.kommando("<CMD")


@braucht_disktool
def test_systemlaufwerk_s_findet_programme_mit_systemattribut(tmp_path):
    """KTL.COM liegt nur auf B: (mit Systemattribut): erst nach `S B:` startet `KTL` vom aktuellen Laufwerk A: aus."""
    b = tmp_path / "bsys.hfe"
    shutil.copy(PARALLEL, b)
    _disktool("attr", b, "KTL.COM", "--sys")
    a = tmp_path / "a.hfe"
    shutil.copy(SERIELL, a)
    _disktool("rm", a, "KTL.COM")
    s = ablauf.Sitzung("k7637", str(a))
    assert s.emu.mount_disk(1, str(b), "cpa800", False)
    s.kaltstart_bis_prompt()
    assert "KTL?" in _kommando(s, "KTL", 10_000_000)
    s.kommando("S B:")
    s.tippe("KTL\r"); s.lauf(20_000_000)
    s.tippe("a"); s.lauf(5_000_000)                  # KTL endet bei 'a'
    assert s.bis_prompt()
    assert "KTL?" not in s.bild().split("S B:")[-1], s.bild()


def test_ctrl_s_stoppt_ctrl_q_setzt_die_ausgabe_fort(tmp_path):
    s = _sitzung(tmp_path)
    s.tippe("POWER\r"); s.lauf(20_000_000)
    s.tippe("TYPE MDOSB20.DOC\r"); s.lauf(1_000_000)
    s.emu.key_press(ord("s"), False, True); s.lauf(60_000); s.emu.key_release(ord("s")); s.lauf(2_000_000)
    angehalten = s.bild()
    s.lauf(5_000_000)
    assert s.bild() == angehalten, "^S hat die Ausgabe nicht angehalten"
    s.emu.key_press(ord("q"), False, True); s.lauf(60_000); s.emu.key_release(ord("q")); s.lauf(3_000_000)
    assert s.bild() != angehalten, "^Q setzt die Ausgabe nicht fort"


def test_ctrl_d_background_kommando_im_debugger_mzsid(tmp_path):
    """Handbuch 4.1: aus MZSID heraus ^D, dann ein residentes Kommando, danach zurueck in MZSID."""
    s = _sitzung(tmp_path)
    bild = _kommando(s, "MZSID", 20_000_000)
    assert "MZSID Vers." in bild
    s.lauf(20_000_000)
    _ctrl(s, "d"); s.lauf(5_000_000)
    assert s.bild().rstrip().endswith(">A>"), s.bild()
    s.tippe("D\r"); s.lauf(20_000_000)
    bild = s.bild()
    assert "A:MicroDOS.SYS" in bild and bild.rstrip().endswith(">"), bild


def _datei_von(pfad, name, ordner):
    """Datei per DiskTool von der (geflushten) Diskette holen; ``None``, wenn sie fehlt."""
    ordner.mkdir(exist_ok=True)
    env = dict(os.environ, LD_LIBRARY_PATH=str(PROJECT_ROOT / "build"))
    subprocess.run([str(DISKTOOL), "get", str(pfad), name, "--fs", "microdos_ds80_5x1024_boot", "--to", str(ordner)], capture_output=True, env=env)
    f = ordner / name
    return f.read_bytes() if f.exists() else None


@braucht_disktool
def test_ausgabeumlenkung_bis_ctrl_c_und_bis_zum_leeren_ausgabekommando(tmp_path):
    """GEKLAERT (2026-11-01): `>DATEI` puffert; der Inhalt steht erst nach dem SCHLIESSEN auf der Diskette.  Geschlossen wird
    durch ^C (Warmstart, Handbuch 4.8), durch ein nacktes `>` -- oder durch ^S ^F (s. naechster Test).  Am Prompt allein
    ist ^F kein Close-Kommando: die Zeileneingabe (BDOS 10) kennt es nicht und gibt `^F` aus (Handbuch 4.2: 'nur wenn die
    BDOS-Funktionen benutzt werden' -- es wird nur in der Warteschleife nach ^S ausgewertet, BDOS-Code CA36)."""
    s = _sitzung(tmp_path)
    s.kommando(">OUT.TXT")
    s.kommando("D")
    _ctrl(s, "f"); s.lauf(5_000_000)
    assert "^F" in s.bild()                                    # am Prompt nur ausgegeben, nichts geschlossen
    s.tippe("\r"); s.lauf(5_000_000)                           # die Zeile "^F" verwerfen: ^C wirkt nur am Zeilenanfang
    _ctrl(s, "c"); s.lauf(50_000_000)                          # Warmstart schliesst die Datei
    s.lauf(10_000_000); s.emu.flush_disks()
    inhalt = _datei_von(tmp_path / "a.hfe", "OUT.TXT", tmp_path / "o1")
    assert inhalt is not None and b"A:MicroDOS.SYS" in inhalt, inhalt
    # nacktes `>` beendet die Umlenkung ebenfalls (legt dabei einen Platz ohne Namen an -- Gastverhalten)
    s2 = _sitzung(tmp_path / "zwei")
    s2.kommando(">OUT.TXT"); s2.kommando("D"); s2.kommando(">")
    s2.lauf(10_000_000); s2.emu.flush_disks()
    inhalt = _datei_von(tmp_path / "zwei" / "a.hfe", "OUT.TXT", tmp_path / "o2")
    assert inhalt is not None and b"A:MicroDOS.SYS" in inhalt, inhalt


@braucht_disktool
def test_ctrl_s_ctrl_f_schliesst_die_ausgabedatei_im_programm(tmp_path):
    """^F wirkt nur, wenn die Ausgabe mit ^S angehalten ist: danach ist die Datei zu und das Weitere (TYPE in POWER) fehlt."""
    s = _sitzung(tmp_path)
    s.kommando(">OUT.TXT")
    s.tippe("POWER\r"); s.lauf(20_000_000)
    s.tippe("TYPE MDOSB20.DOC\r"); s.lauf(1_000_000)
    _ctrl(s, "s"); s.lauf(2_000_000)
    _ctrl(s, "f"); s.lauf(3_000_000)
    _ctrl(s, "q"); s.lauf(40_000_000)
    s.emu.flush_disks()
    inhalt = _datei_von(tmp_path / "a.hfe", "OUT.TXT", tmp_path / "o3")
    assert inhalt is not None and 0 < len(inhalt) <= 512 and b"POWER" in inhalt, inhalt


# ── POWER, MKEY20, MKINST20, HARDCOPY, KTL/KTP ──────────────────────────────

def test_power_dir_copy_und_era(tmp_path):
    s = _sitzung(tmp_path, b=PARALLEL)
    s.tippe("POWER\r"); s.lauf(20_000_000)
    assert "POWER 3.03" in s.bild()
    s.tippe("DIR\r"); s.lauf(10_000_000)
    assert "MicroDOS.SYS" in s.bild() and "SELDSK20.COM" in s.bild()
    s.tippe("?\r"); s.lauf(10_000_000)
    for befehl in ("DIR", "COPY", "REN", "ERA", "TYPE", "EXIT"):
        assert befehl in s.bild()
    s.tippe("ERA B:KTP.COM\r"); s.lauf(20_000_000)
    assert "ERASE (Y/N)" in s.bild()
    s.tippe("Y"); s.lauf(20_000_000)
    s.tippe("DIR B:\r"); s.lauf(20_000_000)
    assert "KTP" not in s.bild().split("DIR B:")[-1]
    s.tippe("EXIT\r"); s.lauf(20_000_000)
    assert s.bis_prompt()


def test_mkey20_belegt_funktionstasten_laut_mkey20_datei(tmp_path):
    s = _sitzung(tmp_path / "ohne")
    _raw(s, 0xCA); s.lauf(2_000_000)
    assert s.bild().rstrip().endswith("A>J"), s.bild()       # ohne MKEY20: Standardbelegung
    s = _sitzung(tmp_path / "mit")
    s.kommando("MKEY20")
    _raw(s, 0xCA); _raw(s, 0xCB); _raw(s, 0xB0); s.lauf(3_000_000)
    assert s.bild().rstrip().endswith("A>DBASEPOWERo d:1"), s.bild()   # PF10 / PF11 / M laut MKEY20


@braucht_disktool
def test_mkinst20_erzeugt_mkey20_spr_aus_einer_eingabedatei(tmp_path):
    eingabe = tmp_path / "MYKEY"
    text = (b";\r\nMKINST20\r\n;\r\n&FE\r\n&FF &0D\r\n&C0 &0D\r\n&CA \"HALLO\"\r\n&00\r\n\x1a")
    eingabe.write_bytes(text)
    s = _sitzung(tmp_path, dateien=[eingabe])
    s.tippe("E MKEY20.SPR\r"); s.lauf(5_000_000); s.tippe("Y"); s.lauf(5_000_000)
    s.kommando("<MYKEY", 60_000_000)
    assert "MKEY20  .SPR" in s.kommando("D") or "MKEY20  .SPR" in s.bild()
    s.kommando("MKEY20")
    _raw(s, 0xCA); s.lauf(3_000_000)
    assert s.bild().rstrip().endswith("A>HALLO"), s.bild()


def test_hardcopy_kopiert_die_angegebenen_spuren_von_a_nach_b(tmp_path):
    """Spuren 0-2 von A: nach B: -> das Verzeichnis von B: ist danach das von A: (CMD.SUB statt KTL.COM-Eintrag)."""
    s = _sitzung(tmp_path, b=PARALLEL)
    s.tippe("HARDCOPY\r")
    assert s.bis("Quelle:")
    s.tippe("A"); s.lauf(3_000_000); s.tippe("B"); s.lauf(3_000_000); s.tippe("03"); s.lauf(3_000_000)
    s.tippe("\r")
    assert s.bis("Richtig (J/N)")
    s.tippe("J")
    assert s.bis("Wiederholung (J/N)", frist=400_000_000)
    s.tippe("N")
    assert s.bis_prompt()
    assert "SELDSK20.COM" in s.kommando("D B:")


def test_ktl_zeigt_logische_codes_ktp_nur_den_parallelen_ubus_tastencode(tmp_path):
    # KTL (BIOS-CONIN): 'b' = 62, Return = 0D, beendet mit 'a'
    s = _sitzung(tmp_path)
    s.tippe("KTL\r"); s.lauf(15_000_000)
    s.tippe("b"); s.lauf(2_000_000); s.tippe("\r"); s.lauf(2_000_000); s.tippe("a"); s.lauf(5_000_000)
    zeilen = s.bild().split("\n")
    assert "62" in zeilen and "0D" in zeilen and zeilen[-1] == "A>", s.bild()
    # KTP liest Port 01H/06H (K7636-Tastatur am U-Bus): physische Tastencodes; Return = FF (ET1), Ende bei 'a'
    p = _sitzung(tmp_path / "p", a=PARALLEL, tastatur="k7636")
    p.tippe("KTP\r"); p.lauf(15_000_000)
    p.tippe("b"); p.lauf(2_000_000); p.tippe("\r"); p.lauf(2_000_000); p.tippe("a"); p.lauf(5_000_000)
    zeilen = p.bild().split("\n")
    assert "62" in zeilen and "FF" in zeilen and zeilen[-1] == "A>", p.bild()
    # An der seriellen K7637 gibt KTP nur FF aus (Port 06H ist leer) -- die Taste kommt nie an
    s.tippe("KTP\r"); s.lauf(15_000_000)
    assert set(s.bild().split("\n")[-6:]) == {"FF"}, s.bild()


# ── SELPRT20 / SELAUX20 und die Druckerwege ──────────────────────────────────

def _seriell_in_dateien(s, tmp_path):
    for i in range(s.emu.serial_count()):
        s.emu.serial_stop(i)
    pfade = []
    from app.core_binding import k1520
    for i in range(s.emu.serial_count()):
        p = tmp_path / ("ser%d.txt" % i)
        p.write_bytes(b"")
        assert s.emu.serial_configure(i, betriebsart=k1520.SER_DATEI, datei=str(p), loop=False)
        assert s.emu.serial_start(i)
        pfade.append(p)
    return pfade


def _lesen(pfad, mindestens=1, frist=3.0):
    """Der E/A-Faden der Schnittstelle schreibt in Wirtszeit: kurz auf die Datei warten (nie ueber Maschinentakte)."""
    import time
    ende = time.time() + frist
    while time.time() < ende:
        d = pfad.read_bytes()
        if len(d) >= mindestens:
            time.sleep(0.2)
            return pfad.read_bytes()
        time.sleep(0.05)
    return pfad.read_bytes()


@pytest.mark.parametrize("schnittstelle,kanal", [("1", 2), ("2", 0)], ids=["IFSS->Drucker_X3", "V24->DFUE_V24_X6"])
def test_selprt20_druckertest_geht_an_die_gewaehlte_schnittstelle(tmp_path, schnittstelle, kanal):
    """Der Druckertest sendet die Codes 20H..7FH (96 Zeichen) an die Schnittstelle, die SELPRT20 vorher einstellt."""
    s = _sitzung(tmp_path)
    ser = _seriell_in_dateien(s, tmp_path)
    s.tippe("SELPRT20\r")
    assert s.bis("Interface benutzen Sie")
    s.tippe(schnittstelle); s.lauf(5_000_000)
    assert s.bis("Welches Typenrad"); s.tippe("1")
    assert s.bis("Drucker bereit (ENTER)"); s.tippe("\r")
    assert s.bis("Richtig (J/N)", frist=200_000_000)
    s.tippe("J")
    assert s.bis_prompt()
    daten = _lesen(ser[kanal], 96)
    assert len(daten) == 96 and daten.startswith(b" !\"#$%&'()*+,-./0123456789:;<=>?@ABC"), daten
    assert all(not p.read_bytes() for i, p in enumerate(ser) if i != kanal)


def test_selprt20_pio_druckertest_laeuft_durch_aber_pio_ist_nicht_nach_aussen_modelliert(tmp_path):
    """PIO-Drucker: der Treiber schreibt auf E/A 54H..57H = die PIO A31 der K8025 (DIL-Schalter), die Z80PIO nimmt die
    96 Zeichen an, nach aussen geht nichts -- keine Druckerschnittstelle, keine Datei."""
    s = _sitzung(tmp_path)
    ser = _seriell_in_dateien(s, tmp_path)
    s.tippe("SELPRT20\r")
    assert s.bis("Interface benutzen Sie")
    s.tippe("3"); s.lauf(5_000_000)
    assert s.bis("Welches Typenrad"); s.tippe("1")
    assert s.bis("Drucker bereit (ENTER)"); s.tippe("\r")
    assert s.bis("Richtig (J/N)", frist=200_000_000)
    s.lauf(20_000_000)
    assert all(not _lesen(p, 1, 0.5) for p in ser)


def test_selaux20_menue_nennt_die_drei_zusatzkanaele(tmp_path):
    s = _sitzung(tmp_path)
    s.tippe("SELAUX20\r")
    assert s.bis("Geben Sie die entsprechende Ziffer an")
    bild = s.bild()
    assert "IFSS (ASS K 8025)" in bild and "IFSP (ALB K 6025)" in bild and "V.24 (ASS K 8025)" in bild
    s.tippe("3"); s.lauf(3_000_000)
    assert s.bis("Wiederholung"); s.tippe("N")
    assert s.bis_prompt()


@pytest.mark.parametrize("ctrl_p", [True])
def test_ctrl_p_schaltet_den_drucker_zu_und_wieder_ab(tmp_path, ctrl_p):
    s = _sitzung(tmp_path)
    ser = _seriell_in_dateien(s, tmp_path)
    _ctrl(s, "p"); s.lauf(3_000_000)
    s.kommando("D")
    _ctrl(s, "p"); s.lauf(3_000_000)
    s.kommando("D")
    s.lauf(20_000_000)
    daten = _lesen(ser[2], 30)                          # Standardgenerierung: Drucker IFSS = "Drucker" X3
    assert daten.startswith(b"D\r") and daten.count(b"MicroDOS.SYS") == 1, daten
