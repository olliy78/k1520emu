"""Gemeinsame Helfer der Hardware-Testgruppe (Entwurf 40).

Kein Qt: Maschinen werden ueber die Python-Bindung gefahren, die Diskette ueber
``app.gw.PhysicalSession`` (derselbe Weg wie Emulator und DiskTool).
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

FIXTURES = PROJECT_ROOT / "tests" / "fixtures" / "disks"
OUT = PROJECT_ROOT / "out"

QT_RETURN = 0x01000004
TAKTE_TASTE = 300_000       # gedrueckt / losgelassen, wie serial_gast.py (K7637 = 9600 Baud)


def umgebung(name: str, vorgabe: int) -> int:
    return int(os.environ.get(name, vorgabe))


def laufwerk() -> dict:
    """Sitzungsparameter fuer ``PhysicalSession.start`` (K5601 als Vorgabe)."""
    return dict(drive=os.environ.get("K1520_GW_DRIVE", "a"),
                cell_rate_kbps=umgebung("K1520_GW_RATE", 250),
                num_cyls=umgebung("K1520_GW_CYLS", 80),
                num_heads=umgebung("K1520_GW_HEADS", 2))


def gw_programm() -> str:
    """Das ``gw`` des laufenden Interpreters (venv), nicht irgendeines im PATH."""
    p = Path(sys.executable).parent / "gw"
    return str(p) if p.exists() else (shutil.which("gw") or "gw")


def gw(*args: str, timeout: float = 600) -> subprocess.CompletedProcess:
    """``gw`` aufrufen.  Der Adapter darf dabei von KEINER Sitzung gehalten werden."""
    return subprocess.run([gw_programm(), *args], capture_output=True, text=True,
                          timeout=timeout)


def run_until_text(emu, needle: str, max_cycles: int = 60_000_000,
                   chunk: int = 400_000) -> int:
    """Emulieren bis *needle* im Bild steht.  Returns: verbrauchte Takte, negativ = nicht gefunden."""
    done = 0
    while done < max_cycles:
        done += emu.run(chunk)
        if needle in emu.screen_text():
            return max(done, 1)
    return -max(done, 1)


class Fahrer:
    """Bedient eine Maschine wie ein Anwender: tippen, auf Text warten, Bild festhalten.

    Jeder Schritt landet in ``protokoll``; bei einem Fehlschlag ist das die einzige
    Spur, denn die Hardware laesst sich nicht zurueckspulen.
    """

    #: Nenntakt des A5120 (U880 2,4576 MHz) — Bezug fuer :attr:`faktor`.
    NENNTAKT_HZ = 2_457_600

    def __init__(self, emu, name: str, logdir: Path, faktor: float = 0.0):
        """*faktor*: 1.0 = Echtzeit, 10.0 = zehnfach, 0.0 = unbegrenzt (Vorgabe).

        Gedrosselt wird wie in ``app/emulationstakt.py``: Soll-Takte aus der Wanduhr,
        voraus wird geschlafen, ein Rueckstand (die Maschine stand in einer
        blockierenden Spuranforderung) wird hoechstens 0,1 s nachgeholt.
        """
        self.emu = emu
        self.faktor = faktor
        self._bezug_t = time.monotonic()
        self._bezug_n = 0
        self.name = name
        self.protokoll: list[str] = []
        self.logdatei = logdir / f"{name}.txt"

    def run(self, takte: int) -> int:
        """Wie ``emu.run``, aber im eingestellten Tempo."""
        if self.faktor <= 0.0:
            return self.emu.run(takte)
        tempo = self.NENNTAKT_HZ * self.faktor
        scheibe = max(1000, int(tempo * 0.002))
        done = 0
        while done < takte:
            soll = (time.monotonic() - self._bezug_t) * tempo
            if self._bezug_n >= soll:
                time.sleep(min((self._bezug_n - soll) / tempo + 1e-4, 0.02))
                continue
            if soll - self._bezug_n > tempo * 0.1:
                self._bezug_n = int(soll - tempo * 0.1)
            n = self.emu.run(min(scheibe, takte - done))
            done += n
            self._bezug_n += n
        return done

    def _bild(self, wo: str) -> None:
        self.protokoll.append(f"--- {wo} ---\n{self.emu.screen_text()}")
        self.logdatei.parent.mkdir(parents=True, exist_ok=True)
        self.logdatei.write_text("\n".join(self.protokoll), encoding="utf-8")

    def warte(self, text: str, takte: int = 120_000_000, chunk: int = 400_000) -> bool:
        t0 = time.monotonic()
        n = run_until_text(self, text, takte, chunk)
        ok = n > 0
        self._bild(f"warte auf {text!r}: {'ja' if ok else 'NEIN'} "
                   f"({abs(n) / 1e6:.0f} M Takte, {time.monotonic() - t0:.0f} s)")
        return ok

    def taste(self, code: int) -> None:
        self.emu.key_press(code)
        self.run(TAKTE_TASTE)
        self.emu.key_release(code)
        self.run(TAKTE_TASTE)

    def tippe(self, text: str) -> None:
        for z in text:
            self.taste(ord(z))

    def zeile(self, text: str) -> None:
        """Text + ET1 (RETURN)."""
        self.tippe(text)
        self.taste(QT_RETURN)

    def laufen(self, takte: int) -> None:
        done = 0
        while done < takte:
            done += self.run(min(400_000, takte - done))

    def screen_text(self) -> str:
        return self.emu.screen_text()

    def bild(self) -> str:
        return self.emu.screen_text()

    def zeilen(self) -> list[str]:
        return [z.rstrip(" \x00") for z in self.bild().split("\n") if z.strip(" \x00")]


def cpa_bis_prompt(fahrer: Fahrer, uhr: bool = True) -> bool:
    """CP/A-Uhr-Bootdiskette: Uhrzeit eingeben, bis ``A>``."""
    if uhr:
        if not fahrer.warte("Bitte Uhrzeit eingeben", 120_000_000):
            return False
        fahrer.tippe("120000")
        fahrer.taste(QT_RETURN)
    ok = fahrer.warte("A>", 120_000_000)
    fahrer.laufen(2_000_000)
    return ok


def schreibe_abbild(hfe: Path, *, spuren=None, fortschritt=None, nachkontrolle=True) -> float:
    """Ein ``.hfe``-Abbild Spur fuer Spur mit dem Schreibpfad des Projekts auf die Diskette.

    Bewusst NICHT ``gw write``: das schreibt eine zu lange Spur ueber die Index-Naht
    und zerstoert dort Sektoren (gemessen: Spur 0 der CP/A-Diskette -> 23 von 26
    Sektoren).  ``Device.write_track`` streckt auf die gemessene Drehzahl; das ist der
    Weg, den auch der Emulator geht — und damit selbst ein Pruefling.

    Returns:
        Dauer in Sekunden.
    """
    sys.path.insert(0, str(PROJECT_ROOT / "tests" / "python"))
    from gw_fake import HfeDevice
    from app.gw import open_device

    quelle = HfeDevice(Path(hfe))
    p = laufwerk()
    dev = open_device(p["drive"], cell_rate_kbps=p["cell_rate_kbps"])
    t0 = time.monotonic()
    try:
        liste = spuren if spuren is not None else [
            (c, h) for c in range(quelle.num_cyls) for h in range(quelle.num_heads)]
        for i, (c, h) in enumerate(liste):
            zellen, n = quelle.read_track(c, h)
            dev.write_track(c, h, zellen, n)
            if fortschritt:
                fortschritt(i + 1, len(liste), c, h)
        # Nachkontrolle mit dem unabhaengigen Dekoder von gw: ein Schreibausrutscher
        # (gemessen: 1 von 160 Spuren) faellt hier auf und wird einmal wiederholt —
        # das Produkt macht dasselbe ueber das Pruef-Lesen, dieser Aufspieler nicht.
        for _versuch in range(3 if nachkontrolle else 0):
            dev.deselect()
            schlecht = schlechte_spuren(liste)
            if not schlecht:
                break
            for (c, h) in schlecht:
                zellen, n = quelle.read_track(c, h)
                dev.write_track(c, h, zellen, n)
        else:
            if nachkontrolle:
                dev.deselect()
                rest = schlechte_spuren(liste)
                assert not rest, f"Spuren nach drei Versuchen schlecht: {rest}"
    finally:
        dev.deselect()
    return time.monotonic() - t0


def schlechte_spuren(liste) -> list:
    """Spuren aus *liste*, bei denen ``gw`` weniger Sektoren findet als erwartet."""
    import tempfile
    p = laufwerk()
    bis = max(c for c, _ in liste)
    schlecht = []
    with tempfile.TemporaryDirectory() as d:
        r = gw("read", "--drive", p["drive"], "--tracks",
               f"c=0-{bis}:h=0-{p['num_heads'] - 1}", "--retries", "0",
               "--format", "ibm.scan", f"{d}/x.img")
    wanted = set(liste)
    for m in re.finditer(r"T(\d+)\.(\d): IBM MFM \((\d+)/(\d+) sectors\)", r.stdout + r.stderr):
        c, h, g, e = map(int, m.groups())
        if (c, h) in wanted and g < e:
            schlecht.append((c, h))
    return schlecht


def scan_spur(cyl: int, head: int) -> tuple[int, int]:
    """Unabhaengiger Befund: ``gw``s eigener Dekoder zaehlt die Sektoren der Spur.

    Returns:
        ``(gefunden, erwartet)`` aus der Zeile ``T<c>.<h>: IBM MFM (g/e sectors)``;
        ``(0, 0)`` bei einer Spur ohne Marken.  Der Adapter darf nicht von einer
        Sitzung gehalten werden.
    """
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        r = gw("read", "--drive", laufwerk()["drive"], "--tracks", f"c={cyl}:h={head}",
               "--retries", "0", "--format", "ibm.scan", f"{d}/x.img")
    m = re.search(r"\((\d+)/(\d+) sectors\)", r.stdout + r.stderr)
    return (int(m.group(1)), int(m.group(2))) if m else (0, 0)


def format_und_system(f: Fahrer, ziel: str = "B", *, switch_key: str = "",
                      menu_nav=(), format_taste: str = "1", bis_spur=None,
                      cpabcgen: bool = True, takte=None, sync=None) -> dict:
    """CP/A-Anwenderkette am Prompt ``A>``: FORMAT (mit Vergleichs-Lesen), CPABCGEN, DIR.

    Tastenfolge wie ``tests/system/drivers/format_all.py::make_script`` (dieselben
    Tabellen: ``switch_key`` = Geometrie S/T/U/V/W, ``menu_nav`` = Menueseite).
    ``bis_spur=None`` formatiert die ganze Diskette, sonst nur bis zu dieser Spur
    (Smoke wie in der Matrix).  Erwartet FORMAT.COM + CPABCGEN.COM in A:.

    Returns:
        ``formatiert`` (Ende gemeldet, kein ``SPUR DEFEKT``/``Fehler``),
        ``dir_ok`` (``DIR`` zeigt ``@OS``, nur mit ``cpabcgen``) und die Bilder.
    """
    r: dict = {}
    if takte is None:
        # Eine LEERE Spur kostet ~80 M Takte (Vorlese-Index-Timeouts, Schreiben, Verify;
        # an der echten Diskette gemessen: 44 Spuren in 3,6 G) — Budget grosszuegig.
        takte = 20_000_000_000 if bis_spur is None else (bis_spur + 3) * 250_000_000
    f.zeile("FORMAT")
    f.laufen(5_000_000)
    f.taste(QT_RETURN)                    # Funktion 0 = Formatieren
    f.laufen(6_000_000)
    f.zeile(ziel)                         # Laufwerk
    f.laufen(10_000_000)
    f.taste(QT_RETURN)                    # Vergleichs-Lesen = j
    f.laufen(8_000_000)
    if switch_key:
        f.tippe(switch_key)               # Geometrie umschalten
        f.laufen(4_000_000)
    for k in menu_nav:
        f.tippe(k)
        f.laufen(3_000_000)
    f.tippe(format_taste)
    f.laufen(6_000_000)
    f.taste(QT_RETURN)                    # von Spur 0
    f.laufen(5_000_000)
    if bis_spur is None:
        f.taste(QT_RETURN)                # bis letzte Spur
    else:
        f.zeile(str(bis_spur))
    f.laufen(6_000_000)
    f.tippe("j")                          # Warnung bestaetigen
    ende = _warte_auf_formatende(f, takte, sync, r)
    bild = f.bild()
    r["bild_format"] = bild
    r["formatiert"] = ende and "SPUR DEFEKT" not in bild and "Fehler" not in bild
    f.tippe("n")                          # Wiederholung? n
    f.laufen(20_000_000)
    f.tippe("n")                          # Rueckkehr? n  -> CCP
    f.laufen(30_000_000)
    if cpabcgen:
        f.zeile(f"CPABCGEN {ziel}:")
        f.laufen(80_000_000)
        r["bild_cpabcgen"] = f.bild()
    f.zeile(f"DIR {ziel}:")
    f.laufen(120_000_000)
    r["bild_dir"] = f.bild()
    r["dir_ok"] = "@OS" in r["bild_dir"]
    return r


STILLSTAND_TAKTE = 400_000_000     # eine Spur braucht ~3,5 M; 400 M ohne Fortschritt = Haenger


def _warte_auf_formatende(f: Fahrer, takte: int, sync, r: dict) -> bool:
    """Bis ``FORMATIEREN beendet`` — bricht bei STILLSTAND ab und haelt den Zustand fest.

    Gemessen 2026-10-27: zweimal blieb FORMAT an der echten Diskette bei Spur 44 stehen,
    waehrend die Maschine weiterlief; ohne diese Erkennung verstreicht das ganze Budget
    (Minuten) und am Ende steht nur ein Bild ohne Befund.
    """
    t0 = time.monotonic()
    done, letzte, seit = 0, None, 0
    while done < takte:
        done += f.run(2_000_000)
        bild = f.emu.screen_text()
        if "FORMATIEREN beendet" in bild:
            f._bild(f"FORMAT fertig ({done / 1e6:.0f} M Takte, {time.monotonic() - t0:.0f} s)")
            return True
        m = re.findall(r"FORMATIEREN auf Spur\s+(\d+)", bild)
        spur = m[-1] if m else None
        if spur != letzte:
            letzte, seit = spur, done
        elif done - seit > STILLSTAND_TAKTE:
            r["stillstand"] = dict(spur=spur, takte=done,
                                   stats=str(sync.stats) if sync else "",
                                   fehler=sync.last_error if sync else "")
            r["stillstand"].update(stillstand_befund(f))
            f._bild(f"STILLSTAND bei Spur {spur} ({done / 1e6:.0f} M Takte): {r['stillstand']}")
            return False
    f._bild(f"Budget erschoepft ({done / 1e6:.0f} M Takte)")
    return False


def stillstand_befund(f: Fahrer, proben: int = 400, abstand: int = 997) -> dict:
    """Wo steht die Maschine?  PC-Histogramm beider CPUs + RAM-Abzug neben dem Protokoll.

    Ein Save-State geht ueber die C-ABI nur am P8000; das hier reicht, um die
    Schleife im Gastcode (FORMAT.COM / BIOS / ZVE2-Lesekoroutine) zu finden.
    """
    from collections import Counter
    zve1, zve2, sp1 = Counter(), Counter(), Counter()
    for _ in range(proben):
        f.emu.run(abstand)
        a = f.emu.debug_cpu(0)
        b = f.emu.debug_cpu(1)
        if a:
            zve1[a[0]] += 1
            sp1[a[1]] += 1
        if b:
            zve2[b[0]] += 1
    ram = bytes(f.emu.mem_read(i) for i in range(0x10000))
    abzug = f.logdatei.with_suffix(".ram")
    abzug.write_bytes(ram)
    fmt = lambda c: ", ".join(f"{pc:04X}x{n}" for pc, n in c.most_common(12))
    return dict(zve1=fmt(zve1), zve2=fmt(zve2), sp=fmt(sp1), ram=str(abzug))


def k5601_matrix():
    """Die Matrixzeilen fuer das K5601-Standardlaufwerk, aus ``format_all.py``.

    Returns:
        Liste ``(id, switch_key, menu_nav, format_taste)`` — dieselbe Quelle der
        Wahrheit wie die ctest-Matrix, damit neue Formate hier von selbst erscheinen.
    """
    sys.path.insert(0, str(PROJECT_ROOT / "tests" / "system" / "drivers"))
    import format_all as fa
    zeilen = []
    for boot, drive, geo, key in fa.format_matrix():
        if boot != "clock" or drive != "B":
            continue
        switch, tabelle = fa.resolve_table(geo)
        nav, sel, _beschr, _img = tabelle[key]
        zeilen.append((f"{geo or 'K5601'}-{key}", switch or "", tuple(nav), sel))
    return zeilen


def spurbefund(pfad=None, cyls: int = 6) -> dict:
    """``{(cyl, kopf): (gefunden, erwartet)}`` fuer die ersten *cyls* Zylinder.

    ``pfad`` gesetzt: aus einem Abbild (``gw convert``), sonst von der echten Diskette.
    Beide Seiten gehen durch DENSELBEN Dekoder, damit sind sie vergleichbar.
    """
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        spuren = f"c=0-{cyls - 1}:h=0-1"
        if pfad:
            r = gw("convert", "--tracks", spuren, "--format", "ibm.scan", str(pfad), f"{d}/x.img")
        else:
            r = gw("read", "--drive", laufwerk()["drive"], "--tracks", spuren,
                   "--retries", "0", "--format", "ibm.scan", f"{d}/x.img")
    out = {}
    for m in re.finditer(r"T(\d+)\.(\d): IBM MFM \((\d+)/(\d+) sectors\)", r.stdout + r.stderr):
        c, h, g, e = map(int, m.groups())
        out[(c, h)] = (g, e)
    return out


from contextlib import contextmanager  # noqa: E402


@contextmanager
def sitzung(*, schreibbar: bool = False, fuer_emulator: bool = False,
            read_ahead: bool = True):
    """``PhysicalSession`` mit Aufraeumen.  ``close()`` schreibt Ausstehendes zurueck."""
    from app.gw import PhysicalSession
    s = PhysicalSession.start(writable=schreibbar, for_emulator=fuer_emulator,
                              read_ahead=read_ahead, **laufwerk())
    try:
        yield s
    finally:
        s.close()


def diskette_loeschen() -> None:
    """Echte Leerdiskette: alle Spuren ``gw erase`` (kein Flusswechsel, nur Index)."""
    p = laufwerk()
    r = gw("erase", "--drive", p["drive"], "--tracks",
           f"c=0-{p['num_cyls'] - 1}:h=0-{p['num_heads'] - 1}")
    assert r.returncode == 0, r.stderr[-400:]


def volle_abtastung() -> tuple[int, int, str]:
    """Unabhaengiger Befund ueber die GANZE Diskette mit dem Dekoder von ``gw``.

    Returns:
        ``(gefunden, erwartet, ausgabe)``.  Der Adapter darf nicht gehalten werden.
    """
    import tempfile
    p = laufwerk()
    with tempfile.TemporaryDirectory() as d:
        r = gw("read", "--drive", p["drive"], "--tracks",
               f"c=0-{p['num_cyls'] - 1}:h=0-{p['num_heads'] - 1}",
               "--retries", "0", "--format", "ibm.scan", f"{d}/x.img")
    text = r.stdout + r.stderr
    m = re.search(r"Found (\d+) sectors of (\d+)", text)
    return (int(m.group(1)), int(m.group(2)), text) if m else (0, 0, text)
