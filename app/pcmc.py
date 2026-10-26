"""PCMC — die PC-Seite des Datenaustauschs mit dem MC80.30 (Issue #24).

Gegenstelle zum EPROM „mit PCMC-Erweiterung“ (2800H, `doc/EPROMS/MC80/pcmc/`): sie spielt
das Windows-Programm PCMC (KW-WILLISOFT 2007) nach.  Das Protokoll steht in
`PCMC_ROM.ASM` (4800 Bd 8N1 auf der IFSS, ohne Prüfsumme, Handschlag je Byte):

* **Zum MC80 senden** (MC80: `E`): Kopf aus Startadresse (lo, hi) und *Länge − 1* (lo, hi),
  dann die Daten.  Der MC80 antwortet auf JEDES Byte mit demselben Byte (Echo); erst dann
  geht das nächste.
* **Vom MC80 holen** (MC80: `S`): Startadresse (lo, hi) und Endadresse (lo, hi) — ohne
  Echo —, dann sendet der MC80 die Bytes; die Gegenstelle bestätigt jedes mit demselben
  Byte (der MC80 vergleicht und meldet sonst „Uebertragungsfehler“).

Dateien tragen vorn 4 Bytes `Start lo, Start hi, Länge−1 lo, Länge−1 hi` (:func:`kopf_lesen`,
:func:`mit_kopf`), damit eine geholte Datei später an dieselbe Stelle zurückgeht.

Die Leitung ist ein Telnet-Client auf der IFSS des Emulators (Server auf Schleife, Telnet mit
BINARY in beide Richtungen, damit 0DH und FFH unverändert durchgehen): :class:`PcmcLeitung`.
Das Protokoll selbst (:func:`senden`, :func:`holen`) kennt nur ``ein``/``aus`` und ist ohne
Netz prüfbar.
"""

from __future__ import annotations

import socket
import time
from typing import Callable, Optional, Tuple

#: Laenge des Kopfes in der Datei und auf der Leitung.
KOPF = 4

IAC, DONT, DO, WONT, WILL, SB, SE = 255, 254, 253, 252, 251, 250, 240
OPT_BINARY, OPT_SGA = 0, 3


class PcmcFehler(Exception):
    """Übertragung abgebrochen (Zeitüberschreitung, Abbruch, Leitung weg, Echo falsch)."""


# ─── Dateikopf ────────────────────────────────────────────────────────────────

def kopf_lesen(daten: bytes) -> Optional[Tuple[int, int]]:
    """Start- und Endadresse aus dem Kopf einer .bin, oder ``None`` ohne (stimmigen) Kopf.

    Ein Kopf gilt nur, wenn `Länge−1` im Kopf genau zur Dateigröße passt: sonst könnten
    die ersten vier Bytes eines Programms als Adresse missverstanden werden.
    """
    if len(daten) < KOPF + 1:
        return None
    start = daten[0] | daten[1] << 8
    laenge = (daten[2] | daten[3] << 8) + 1
    if len(daten) - KOPF != laenge or start + laenge > 0x10000:
        return None
    return start, start + laenge - 1


def mit_kopf(start: int, daten: bytes) -> bytes:
    """Daten mit dem Adresskopf davor (so speichert die Gegenstelle geholte Bereiche)."""
    if not daten or not 0 <= start <= 0xFFFF or start + len(daten) > 0x10000:
        raise ValueError("Adressbereich passt nicht in den 64-KB-Raum")
    n = len(daten) - 1
    return bytes((start & 0xFF, start >> 8, n & 0xFF, n >> 8)) + daten


# ─── Protokoll ────────────────────────────────────────────────────────────────

Ein = Callable[[float], int]      # liest ein Byte (Frist in Sekunden), wirft PcmcFehler
Aus = Callable[[bytes], None]
Fortschritt = Callable[[int, int], None]      # (fertig, gesamt) in Bytes; False/Ausnahme = Abbruch


def _echo(ein: Ein, aus: Aus, byte: int, frist: float) -> None:
    aus(bytes((byte,)))
    antwort = ein(frist)
    if antwort != byte:
        raise PcmcFehler(f"Übertragungsfehler: {byte:02X}H gesendet, {antwort:02X}H zurück")


def senden(ein: Ein, aus: Aus, start: int, daten: bytes, *, frist: float = 20.0,
           fortschritt: Optional[Fortschritt] = None) -> None:
    """Daten an Adresse *start* in den MC80 schreiben (am MC80 läuft `E`).

    Der erste Echo-Fehler beendet die Übertragung: der MC80 hat dann einen falschen Kopf
    oder falsche Daten übernommen und wartet auf den Rest.
    """
    if not daten or start < 0 or start + len(daten) > 0x10000:
        raise PcmcFehler("Adressbereich passt nicht in den 64-KB-Raum")
    n = len(daten) - 1
    for b in (start & 0xFF, start >> 8, n & 0xFF, n >> 8):
        _echo(ein, aus, b, frist)
    for i, b in enumerate(daten):
        _echo(ein, aus, b, frist)
        if fortschritt:
            fortschritt(i + 1, len(daten))


def holen(ein: Ein, aus: Aus, start: int, ende: int, *, frist: float = 20.0,
          fortschritt: Optional[Fortschritt] = None) -> bytes:
    """Den Bereich *start*…*ende* (einschließlich) aus dem MC80 lesen (am MC80 läuft `S`)."""
    if not 0 <= start <= ende <= 0xFFFF:
        raise PcmcFehler("Adressbereich ungültig")
    aus(bytes((start & 0xFF, start >> 8, ende & 0xFF, ende >> 8)))
    anzahl = ende - start + 1
    erg = bytearray()
    for i in range(anzahl):
        b = ein(frist)
        erg.append(b)
        aus(bytes((b,)))
        if fortschritt:
            fortschritt(i + 1, anzahl)
    return bytes(erg)


# ─── Leitung ──────────────────────────────────────────────────────────────────

class PcmcLeitung:
    """Telnet-Client auf der IFSS des Emulators (Server, Schleife).  Nicht fadensicher:
    :meth:`lesen`/:meth:`schreiben` gehören dem Arbeitsfaden der Übertragung."""

    def __init__(self, host: str, port: int, abbruch: Optional[Callable[[], bool]] = None):
        self._abbruch = abbruch or (lambda: False)
        self._sock = socket.create_connection((host, port), timeout=3)
        self._sock.settimeout(0.05)
        self._puffer = bytearray()          # dekodierte Nutzbytes
        self._zst = 0                       # 0 Daten, 1 nach IAC, 2 nach Verb, 3 SB-Daten, 4 SB-IAC
        self._verb = 0
        # BINARY in beide Richtungen: sonst wird 0DH als 0DH 00 uebertragen.
        self._roh(bytes((IAC, WILL, OPT_BINARY, IAC, DO, OPT_BINARY)))

    def _roh(self, daten: bytes) -> None:
        try:
            self._sock.sendall(daten)
        except OSError as e:
            raise PcmcFehler(f"Leitung zum Emulator unterbrochen ({e})") from e

    def _verarbeite(self, roh: bytes) -> None:
        for b in roh:
            if self._zst == 0:
                if b == IAC:
                    self._zst = 1
                else:
                    self._puffer.append(b)
            elif self._zst == 1:
                if b == IAC:
                    self._puffer.append(IAC)
                    self._zst = 0
                elif b in (WILL, WONT, DO, DONT):
                    self._verb, self._zst = b, 2
                elif b == SB:
                    self._zst = 3
                else:
                    self._zst = 0
            elif self._zst == 2:
                self._zst = 0
                # BINARY und SGA haben wir selbst angeboten bzw. sind harmlos; alles andere
                # (ECHO, COM-PORT-Option …) lehnen wir ab.
                if b not in (OPT_BINARY, OPT_SGA):
                    if self._verb == DO:
                        self._roh(bytes((IAC, WONT, b)))
                    elif self._verb == WILL:
                        self._roh(bytes((IAC, DONT, b)))
            elif self._zst == 3:
                self._zst = 4 if b == IAC else 3
            else:   # 4: SB … IAC
                self._zst = 0 if b == SE else 3

    def _einlesen(self) -> None:
        try:
            roh = self._sock.recv(4096)
        except socket.timeout:
            return
        except OSError as e:
            raise PcmcFehler(f"Leitung zum Emulator unterbrochen ({e})") from e
        if not roh:
            raise PcmcFehler("Der Emulator hat die Leitung geschlossen")
        self._verarbeite(roh)

    def leeren(self) -> None:
        """Alles Vorhandene verwerfen (Reste einer früheren Übertragung)."""
        self._einlesen()
        self._puffer.clear()

    def lesen(self, frist: float) -> int:
        ende = time.monotonic() + frist
        while not self._puffer:
            if self._abbruch():
                raise PcmcFehler("Abgebrochen")
            if time.monotonic() > ende:
                raise PcmcFehler("Keine Antwort vom MC80 (Zeitüberschreitung) — läuft am MC80 "
                                 "das Programm PCMC mit E bzw. S?")
            self._einlesen()
        return self._puffer.pop(0)

    def schreiben(self, daten: bytes) -> None:
        if self._abbruch():
            raise PcmcFehler("Abgebrochen")
        # Nutzbytes: 0xFF verdoppeln (Telnet).
        self._roh(daten.replace(b"\xff", b"\xff\xff"))

    def schliessen(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass
