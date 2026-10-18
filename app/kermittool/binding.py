"""k1520Kermit — Bindung an ``libk1520kermit`` (C-ABI: ``kermit/include/k1520kermit.h``).

Reine ctypes-Huelle, **keine Qt-Abhaengigkeit** (die Oberflaeche stellt Rueckrufe
selbst per Signal in den GUI-Faden).  Die Bibliothek wird beim ERSTEN Aufruf
geladen, ihr Pfad kommt aus :mod:`app.paths` (``kermit_library()``,
``K1520_KERMIT_LIB`` hat Vorrang).

Typische Benutzung::

    from app.kermittool.binding import Kermit, KermitError

    with Kermit.telnet_client("127.0.0.1", 5000) as k:
        k.login("kermit", "kermit", 60000)          # nur an einer getty
        k.send("daten.bin", "daten.bin")
        for e in parse_ls(k.remote_dir("")):
            print(e["name"], e["groesse"])
        k.remote_finish()

Fehler werden als :class:`KermitError` (``code`` = ``K1520K_E_*``) gemeldet.
Blockierende Aufrufe geben die GIL frei (``ctypes.CDLL``); ``abort()`` darf aus
jedem Faden kommen.  Rueckrufe laufen im Faden der Operation; Ausnahmen darin
werden verschluckt (ctypes koennte sie nicht ueber die C-Grenze tragen).

Wichtig: die Funktionszeiger der Rueckrufe muessen LEBEN, solange die
Bibliothek sie kennt — :class:`Kermit` haelt sie in ``_cb_refs`` fest.
"""

from __future__ import annotations

import ctypes
import json
import sys
from pathlib import Path
from typing import Callable, Dict, List, Optional

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from app import paths                                        # noqa: E402

# ─── Ergebniscodes (= K1520K_E_* im Header) ──────────────────────────────────

OK = 0
E_TIMEOUT = 1
E_REMOTE = 2
E_ABORTED = 3
E_DISCONNECTED = 4
E_NOT_FOUND = 5
E_READ = 6
E_WRITE = 7
E_PATH = 8
E_NAME_TOO_LONG = 9
E_REFUSED = 10
E_PROTOCOL = 11
E_UNSUPPORTED = 12
E_USAGE = 100
E_LINE = 101
E_LOGIN = 102


class KermitError(RuntimeError):
    """Eine Operation ist gescheitert; ``code`` ist ein ``K1520K_E_*``-Wert."""

    def __init__(self, code: int, text: str):
        super().__init__(text or f"Kermit-Fehler {code}")
        self.code = code
        self.text = text


# ─── Bibliothek ──────────────────────────────────────────────────────────────

# Rueckrufe: (Datei, Bytes, Gesamt, user) bzw. (Text, user).
PROGRESS_CB = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_int64, ctypes.c_int64, ctypes.c_void_p)
EVENT_CB = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_void_p)

_H = ctypes.c_void_p
_CS = ctypes.c_char_p
_I = ctypes.c_int

#: Signaturen aller Funktionen der C-ABI: Name -> (restype, [argtypes]).  Der Waechter
#: ``tests/python/test_kermit_api.py`` vergleicht diese Tabelle mechanisch mit dem
#: Header (Namen UND Typen) und mit den exportierten Symbolen.
SIGNATUREN: Dict[str, tuple] = {
    "k1520k_version": (_CS, []),
    "k1520k_open_telnet_client": (_H, [_CS, _I, _I]),
    "k1520k_open_telnet_server": (_H, [_I]),
    "k1520k_open_rfc2217_client": (_H, [_CS, _I, _I, _I, _I, _I, _I]),
    "k1520k_open_rfc2217_server": (_H, [_I, _I, _I, _I, _I]),
    "k1520k_last_open_error": (_CS, []),
    "k1520k_listen_port": (_I, [_H]),
    "k1520k_accept": (_I, [_H, _I]),
    "k1520k_connected": (_I, [_H]),
    "k1520k_peer": (_CS, [_H]),
    "k1520k_line_notice": (_CS, [_H]),
    "k1520k_login": (_I, [_H, _CS, _CS, _I]),
    "k1520k_set_pruefsumme": (_I, [_H, _I]),
    "k1520k_set_paketlaenge": (_I, [_H, _I]),
    "k1520k_set_namen": (_I, [_H, _I, _I]),
    "k1520k_set_textmodus": (_I, [_H, _I]),
    "k1520k_set_fristen": (_I, [_H, _I, _I]),
    "k1520k_set_max_namenslaenge": (_I, [_H, _I]),
    "k1520k_set_ueberschreiben": (_I, [_H, _I]),
    "k1520k_set_callbacks": (_I, [_H, PROGRESS_CB, EVENT_CB, ctypes.c_void_p]),
    "k1520k_send": (_I, [_H, _CS, _CS]),
    "k1520k_get": (_I, [_H, _CS, _CS]),
    "k1520k_remote_dir": (_I, [_H, _CS, ctypes.c_char_p, _I]),
    "k1520k_remote_cd": (_I, [_H, _CS]),
    "k1520k_remote_delete": (_I, [_H, _CS]),
    "k1520k_remote_finish": (_I, [_H]),
    "k1520k_remote_logout": (_I, [_H]),
    "k1520k_serve": (_I, [_H, _CS, _I]),
    "k1520k_abort": (None, [_H]),
    "k1520k_abort_file": (None, [_H]),
    "k1520k_abort_group": (None, [_H]),
    "k1520k_last_error": (_CS, [_H]),
    "k1520k_result_string": (_CS, [_H]),
    "k1520k_parse_ls": (_CS, [_CS]),
    "k1520k_close": (None, [_H]),
}

_lib_cache: Optional[ctypes.CDLL] = None


def find_libk1520kermit() -> Path:
    """Pfad der Kermit-Bibliothek (Aufloesung in :mod:`app.paths`)."""
    return paths.kermit_library()


def lib() -> ctypes.CDLL:
    """Die geladene Bibliothek (beim ersten Aufruf geladen, Signaturen gesetzt)."""
    global _lib_cache
    if _lib_cache is None:
        paths.prepare_library_load()
        dll = ctypes.CDLL(str(find_libk1520kermit()))
        for name, (res, args) in SIGNATUREN.items():
            fn = getattr(dll, name)
            fn.restype = res
            fn.argtypes = args
        _lib_cache = dll
    return _lib_cache


def _b(s: Optional[str]) -> Optional[bytes]:
    return None if s is None else s.encode("utf-8")


def _s(b: Optional[bytes]) -> str:
    return "" if b is None else b.decode("utf-8", errors="replace")


def version() -> str:
    return _s(lib().k1520k_version())


def parse_ls(text: str) -> List[dict]:
    """Zerlegt den ``ls -l``-Text eines Servers in Eintraege (Name, Art, Rechte, Groesse, Datum …)."""
    roh = lib().k1520k_parse_ls(_b(text))
    return json.loads(_s(roh))


# ─── Sitzung ─────────────────────────────────────────────────────────────────

class Kermit:
    """Eine Leitung samt Kermit-Sitzung.  Ein Faden benutzt sie; nur ``abort*`` darf ein anderer rufen."""

    def __init__(self, handle: int):
        self._h = ctypes.c_void_p(handle)
        self._cb_refs: list = []

    # ── Oeffnen ──────────────────────────────────────────────────────────
    @staticmethod
    def _offen(handle) -> "Kermit":
        if not handle:
            raise KermitError(E_LINE, _s(lib().k1520k_last_open_error()))
        return Kermit(handle)

    @classmethod
    def telnet_client(cls, host: str, port: int, frist_ms: int = 5000) -> "Kermit":
        return cls._offen(lib().k1520k_open_telnet_client(_b(host), port, frist_ms))

    @classmethod
    def telnet_server(cls, port: int = 0) -> "Kermit":
        return cls._offen(lib().k1520k_open_telnet_server(port))

    @classmethod
    def rfc2217_client(cls, host: str, port: int, frist_ms: int = 5000, baud: int = 9600,
                       datenbits: int = 8, paritaet: str = "N", stoppbits: int = 1) -> "Kermit":
        return cls._offen(lib().k1520k_open_rfc2217_client(
            _b(host), port, frist_ms, baud, datenbits, ord(paritaet[:1] or "N"), stoppbits))

    @classmethod
    def rfc2217_server(cls, port: int = 0, baud: int = 9600, datenbits: int = 8,
                       paritaet: str = "N", stoppbits: int = 1) -> "Kermit":
        return cls._offen(lib().k1520k_open_rfc2217_server(
            port, baud, datenbits, ord(paritaet[:1] or "N"), stoppbits))

    # ── Lebenszyklus ─────────────────────────────────────────────────────
    def close(self) -> None:
        if self._h:
            lib().k1520k_close(self._h)
            self._h = ctypes.c_void_p(0)
            self._cb_refs = []

    def __enter__(self) -> "Kermit":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def _pruefe(self, rc: int) -> None:
        if rc != OK:
            raise KermitError(rc, self.last_error)

    def _handle(self) -> ctypes.c_void_p:
        if not self._h:
            raise KermitError(E_USAGE, "Sitzung ist geschlossen")
        return self._h

    # ── Leitung ──────────────────────────────────────────────────────────
    @property
    def listen_port(self) -> int:
        """Gebundener Port eines lauschenden Servers (0 sonst)."""
        return lib().k1520k_listen_port(self._handle())

    def accept(self, frist_ms: int = 60000) -> None:
        self._pruefe(lib().k1520k_accept(self._handle(), frist_ms))

    @property
    def connected(self) -> bool:
        return bool(lib().k1520k_connected(self._handle()))

    @property
    def peer(self) -> str:
        return _s(lib().k1520k_peer(self._handle()))

    @property
    def line_notice(self) -> str:
        return _s(lib().k1520k_line_notice(self._handle()))

    def login(self, name: str, kennwort: Optional[str] = None, frist_ms: int = 60000) -> str:
        """Anmeldung an einer getty; liefert den gelesenen Text (Banner)."""
        self._pruefe(lib().k1520k_login(self._handle(), _b(name), _b(kennwort), frist_ms))
        return self.result

    # ── Einstellungen ────────────────────────────────────────────────────
    def set_pruefsumme(self, typ: int) -> None:
        self._pruefe(lib().k1520k_set_pruefsumme(self._handle(), typ))

    def set_paketlaenge(self, laenge: int) -> None:
        self._pruefe(lib().k1520k_set_paketlaenge(self._handle(), laenge))

    def set_namen(self, senden_konvertiert: bool, empfang_konvertiert: bool) -> None:
        self._pruefe(lib().k1520k_set_namen(self._handle(), int(senden_konvertiert), int(empfang_konvertiert)))

    def set_textmodus(self, an: bool) -> None:
        self._pruefe(lib().k1520k_set_textmodus(self._handle(), int(an)))

    def set_fristen(self, paket_ms: int, max_wiederholungen: int = 10) -> None:
        self._pruefe(lib().k1520k_set_fristen(self._handle(), paket_ms, max_wiederholungen))

    def set_max_namenslaenge(self, n: int) -> None:
        self._pruefe(lib().k1520k_set_max_namenslaenge(self._handle(), n))

    def set_ueberschreiben(self, an: bool) -> None:
        self._pruefe(lib().k1520k_set_ueberschreiben(self._handle(), int(an)))

    def set_callbacks(self, fortschritt: Optional[Callable[[str, int, int], None]] = None,
                      ereignis: Optional[Callable[[str], None]] = None) -> None:
        """Rueckrufe setzen (``None`` schaltet ab).  Nicht waehrend einer Operation aufrufen."""
        def _p(datei, b, g, _user):
            try:
                fortschritt(_s(datei), int(b), int(g))        # type: ignore[misc]
            except Exception:
                pass

        def _e(text, _user):
            try:
                ereignis(_s(text))                            # type: ignore[misc]
            except Exception:
                pass

        p = PROGRESS_CB(_p) if fortschritt else ctypes.cast(None, PROGRESS_CB)
        e = EVENT_CB(_e) if ereignis else ctypes.cast(None, EVENT_CB)
        self._pruefe(lib().k1520k_set_callbacks(self._handle(), p, e, None))
        self._cb_refs = [p, e, _p, _e]   # am Leben halten, solange die Bibliothek sie kennt

    # ── Client ───────────────────────────────────────────────────────────
    def send(self, lokal: str, entfernt: str = "") -> List[str]:
        """Sendet eine Datei (``entfernt`` leer = Dateiname); liefert die gesendeten lokalen Dateien."""
        self._pruefe(lib().k1520k_send(self._handle(), _b(lokal), _b(entfernt)))
        return [z for z in self.result.split("\n") if z]

    def get(self, entfernt: str, zielordner: str) -> List[str]:
        """Holt Datei(en) in ``zielordner``; liefert die abgelegten Namen."""
        self._pruefe(lib().k1520k_get(self._handle(), _b(entfernt), _b(zielordner)))
        return [z for z in self.result.split("\n") if z]

    def remote_dir(self, pfad: str = "") -> str:
        """Verzeichnis des Servers als ``ls -l``-Text."""
        self._pruefe(lib().k1520k_remote_dir(self._handle(), _b(pfad), None, 0))
        return self.result

    def remote_cd(self, pfad: str) -> None:
        self._pruefe(lib().k1520k_remote_cd(self._handle(), _b(pfad)))

    def remote_delete(self, name: str) -> str:
        self._pruefe(lib().k1520k_remote_delete(self._handle(), _b(name)))
        return self.result

    def remote_finish(self) -> None:
        self._pruefe(lib().k1520k_remote_finish(self._handle()))

    def remote_logout(self) -> None:
        self._pruefe(lib().k1520k_remote_logout(self._handle()))

    # ── Server ───────────────────────────────────────────────────────────
    def serve(self, wurzel: str, nur_lesen: bool = False) -> None:
        """Serverschleife (blockiert bis G F/G L; Abbruch/Trennung = KermitError mit E_ABORTED/E_DISCONNECTED)."""
        self._pruefe(lib().k1520k_serve(self._handle(), _b(wurzel), int(nur_lesen)))

    # ── Abbruch (jeder Faden) ────────────────────────────────────────────
    def abort(self) -> None:
        if self._h:
            lib().k1520k_abort(self._h)

    def abort_file(self) -> None:
        if self._h:
            lib().k1520k_abort_file(self._h)

    def abort_group(self) -> None:
        if self._h:
            lib().k1520k_abort_group(self._h)

    # ── Auskunft ─────────────────────────────────────────────────────────
    @property
    def last_error(self) -> str:
        return _s(lib().k1520k_last_error(self._handle()))

    @property
    def result(self) -> str:
        return _s(lib().k1520k_result_string(self._handle()))
