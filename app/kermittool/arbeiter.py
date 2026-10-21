"""Der Arbeitsfaden — ALLE Aufrufe der Kermit-Bibliothek laufen hier.

Eine Verbindung, Auftraege nacheinander (Warteschlange); die Oberflaeche friert nie
ein.  Die Rueckrufe der Bibliothek kommen in diesem Faden und werden in Qt-Signale
umgesetzt — ein Widget wird aus dem Arbeitsfaden nie angefasst.  Die Empfaenger
verbindet das Fenster mit ``Qt.QueuedConnection``.

Aus dem GUI-Faden darf nur :meth:`Arbeiter.abbrechen` (und die Einstellung der
Flags) auf die Sitzung zugreifen — wie in der C-ABI vorgesehen (``k1520k_abort*``).
"""

from __future__ import annotations

import queue
import time
from typing import Callable, List, Optional, Tuple

from PySide6.QtCore import QObject, QThread, Qt, Signal

from app.kermittool import binding, konfig
from app.kermittool.binding import Kermit, KermitError, parse_ls

#: Fortschrittsmeldungen hoechstens so oft (Sekunden) — auf Loopback kaeme sonst jedes Paket.
FORTSCHRITT_TAKT = 0.05


class _Faden(QThread):
    def __init__(self, arbeiter: "Arbeiter"):
        super().__init__()
        self._a = arbeiter

    def run(self) -> None:                      # noqa: D401
        self._a._schleife()


class Arbeiter(QObject):
    """Hat die Sitzung und den Faden; spricht mit dem Fenster nur ueber Signale."""

    #: (Art, Text): Art = ``getrennt``/``verbindet``/``lauscht``/``verbunden``; Text fuer die Statuszeile.
    zustand = Signal(str, str)
    #: Ereignistext der Bibliothek (Protokollfenster).
    ereignis = Signal(str)
    #: (Datei, Bytes, Gesamt) — Gesamt -1, wenn unbekannt.
    fortschritt = Signal(str, object, object)
    #: Name des Auftrags, der gleich beginnt.
    auftrag_beginnt = Signal(str)
    #: (Name, ok, Fehlercode, Text, Daten) — Daten je nach Auftrag (Verzeichnis: Liste von Eintraegen).
    auftrag_fertig = Signal(str, bool, int, str, object)
    #: Der Fehlertext/Ergebnistext der Gegenstelle zu einem Auftrag (nur wenn vorhanden).
    gegenstelle_meldet = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._q: "queue.Queue[Optional[Tuple[str, Callable]]]" = queue.Queue()
        self._k: Optional[Kermit] = None
        self._einst = konfig.einstellungen_aus(None)
        self._wieder = False
        self._stopp = False
        self._abbruch_gewuenscht = False
        self._beschaeftigt = 0               # Auftraege in Schlange + laufend — NUR im GUI-Faden gezaehlt
        self._letzter_takt = 0.0
        # Gezaehlt wird im GUI-Faden, wenn das Ergebnis dort ANKOMMT: so stimmt `beschaeftigt` in
        # jedem Empfaenger von `auftrag_fertig` (dieser Empfaenger ist der erste verbundene).
        self.auftrag_fertig.connect(self._gezaehlt, Qt.QueuedConnection)
        self._faden = _Faden(self)
        self._faden.start()

    # ── GUI-Faden ───────────────────────────────────────────────────────────

    def _gezaehlt(self, *_ignoriert) -> None:
        self._beschaeftigt = max(0, self._beschaeftigt - 1)

    @property
    def beschaeftigt(self) -> bool:
        return self._beschaeftigt > 0

    def einstellungen_setzen(self, einst: dict) -> None:
        """Neue Uebertragungsvorgaben — wirken ab dem naechsten Auftrag."""
        self._einst = dict(einst)

    def wieder_lauschen_setzen(self, an: bool) -> None:
        self._wieder = bool(an)

    def auftrag(self, name: str, fn: Callable, *args) -> None:
        self._beschaeftigt += 1
        self._q.put((name, lambda: fn(*args)))

    def abbrechen(self, hart: bool = False) -> None:
        """Laufende Operation abbrechen.  ``hart`` = Sitzung und Leitung (``abort``), sonst geordnet (``Z``)."""
        k = self._k
        self._abbruch_gewuenscht = True
        if k is None:
            return
        try:
            if hart:
                k.abort()
            else:
                k.abort_group()
        except Exception:                       # noqa: BLE001 — Sitzung gerade geschlossen
            pass

    def stoppen(self) -> None:
        """Lauschen/Server/Verbindung hart beenden (wartet nicht)."""
        self._stopp = True
        self._wieder = False
        self.abbrechen(hart=True)

    def beenden(self) -> None:
        """Faden beenden (Fenster schliesst)."""
        self.stoppen()
        self._q.put(None)
        self._faden.wait(5000)

    # ── Auftraege (Namen der Methoden = was das Fenster anstoesst) ──────────

    def verbinden(self, p: dict, kennwort: Optional[str]) -> None:
        self._stopp = False
        self.auftrag("verbinden", self._j_verbinden, p, kennwort)

    def verzeichnis(self) -> None:
        self.auftrag("verzeichnis", self._j_dir)

    def wechseln(self, name: str) -> None:
        self.auftrag("wechseln", self._j_cd, name)

    def holen(self, namen: List[str], ziel: str) -> None:
        self.auftrag("holen", self._j_get, list(namen), ziel)

    def senden(self, dateien: List[str]) -> None:
        self.auftrag("senden", self._j_put, list(dateien))

    def loeschen(self, namen: List[str]) -> None:
        self.auftrag("loeschen", self._j_rm, list(namen))

    def gegenstelle_beenden(self) -> None:
        self.auftrag("finish", self._j_finish)

    def trennen(self) -> None:
        self._stopp = True
        self._wieder = False
        self.abbrechen(hart=True)
        self.auftrag("trennen", self._j_trennen)

    def server(self, p: dict, wurzel: str, nur_lesen: bool) -> None:
        self._stopp = False
        self.auftrag("server", self._j_server, p, wurzel, nur_lesen)

    # ── Arbeitsfaden ────────────────────────────────────────────────────────

    def _schleife(self) -> None:
        while True:
            eintrag = self._q.get()
            if eintrag is None:
                break
            name, fn = eintrag
            self._abbruch_gewuenscht = False
            self.auftrag_beginnt.emit(name)
            ok, code, text, daten = True, 0, "", None
            try:
                daten = fn()
            except KermitError as e:
                ok, code, text = False, e.code, e.text
            except Exception as e:               # noqa: BLE001 — nie den Faden verlieren
                ok, code, text = False, -1, f"{type(e).__name__}: {e}"
            if (not ok and code in (binding.E_DISCONNECTED, binding.E_LINE, binding.E_LOGIN)
                    and name not in ("trennen",)):
                self._schliessen()               # eine tote Leitung bleibt nicht offen
            self.auftrag_fertig.emit(name, ok, code, text, daten)

    def _schliessen(self) -> None:
        k, self._k = self._k, None
        if k is not None:
            try:
                k.close()
            except Exception:                    # noqa: BLE001
                pass
        self.zustand.emit("getrennt", "")

    def _anwenden(self, k: Kermit) -> None:
        e = self._einst
        k.set_pruefsumme(e["pruefsumme"])
        k.set_paketlaenge(e["paketlaenge"])
        k.set_namen(e["namen_senden"] == "konvertiert", e["namen_empfang"] == "konvertiert")
        k.set_fristen(e["paket_frist_s"] * 1000, e["wiederholungen"])
        k.set_tempo(e["tempo"])
        k.set_max_namenslaenge(e["max_namen"])
        k.set_ueberschreiben(e["ueberschreiben"])

    def _rueckrufe(self, k: Kermit) -> None:
        def fortschritt(datei: str, b: int, g: int) -> None:
            jetzt = time.monotonic()
            fertig = g > 0 and b >= g
            if fertig or jetzt - self._letzter_takt >= FORTSCHRITT_TAKT:
                self._letzter_takt = jetzt
                self.fortschritt.emit(datei, b, g)

        k.set_callbacks(fortschritt, self.ereignis.emit)

    def _oeffnen(self, p: dict) -> Kermit:
        """Leitung oeffnen bzw. lauschen und annehmen; liefert die verbundene Sitzung."""
        art, lauschen = p["leitung"], p["aufbau"] == "lauschen"
        baud = int(p.get("baud") or 9600)
        if lauschen:
            k = (Kermit.telnet_server(int(p["port"])) if art == "telnet"
                 else Kermit.rfc2217_server(int(p["port"]), baud))
            self._k = k
            self.zustand.emit("lauscht", f"Lauscht auf Port {k.listen_port}")
            self.ereignis.emit(f"Lausche auf Port {k.listen_port} ...")
            try:
                k.accept(86_400_000)             # abbrechbar (stoppen/trennen weckt)
            except KermitError:
                if self._stopp or self._abbruch_gewuenscht:
                    raise KermitError(binding.E_ABORTED, "Lauschen abgebrochen")
                raise
            return k
        self.zustand.emit("verbindet", f"Verbinde mit {p['host']}:{p['port']} ...")
        k = (Kermit.telnet_client(p["host"], int(p["port"]), 5000) if art == "telnet"
             else Kermit.rfc2217_client(p["host"], int(p["port"]), 5000, baud))
        self._k = k
        return k

    def _j_verbinden(self, p: dict, kennwort: Optional[str]) -> str:
        self._schliessen()
        try:
            k = self._oeffnen(p)
            self._anwenden(k)
            self._rueckrufe(k)
            if p.get("login") and p.get("login_name"):
                self.ereignis.emit(f"Anmeldung als {p['login_name']} ...")
                k.login(p["login_name"], kennwort or None, self._einst["login_frist_s"] * 1000)
            peer = k.peer or f"{p.get('host', '')}:{p.get('port', '')}"
            self.zustand.emit("verbunden", f"Verbunden mit {peer}")
            if k.line_notice:
                self.ereignis.emit(k.line_notice)
            return peer
        except BaseException:
            self._schliessen()
            raise

    def _lebendig(self) -> Kermit:
        if self._k is None:
            raise KermitError(binding.E_USAGE, "Keine Verbindung")
        self._anwenden(self._k)
        return self._k

    def _j_dir(self) -> list:
        k = self._lebendig()
        return parse_ls(k.remote_dir(""))

    def _j_cd(self, name: str) -> list:
        k = self._lebendig()
        k.remote_cd(name)
        return parse_ls(k.remote_dir(""))

    def _j_get(self, namen: List[str], ziel: str) -> list:
        k = self._lebendig()
        abgelegt: List[str] = []
        for n in namen:
            k.set_textmodus(konfig.text_modus(self._einst["modus"], n))
            abgelegt += k.get(n, ziel)
            if self._abbruch_gewuenscht:
                break
        return abgelegt

    def _j_put(self, dateien: List[str]) -> list:
        k = self._lebendig()
        gesendet: List[str] = []
        for d in dateien:
            k.set_textmodus(konfig.text_modus(self._einst["modus"], d))
            gesendet += k.send(d, "")
            if self._abbruch_gewuenscht:
                break
        return gesendet

    def _j_rm(self, namen: List[str]) -> list:
        k = self._lebendig()
        meldungen = []
        for n in namen:
            m = k.remote_delete(n)
            if m:
                meldungen.append(m)
                self.gegenstelle_meldet.emit(m)
        return meldungen

    def _j_finish(self) -> None:
        k = self._lebendig()
        k.remote_finish()
        self._schliessen()

    def _j_trennen(self) -> None:
        self._schliessen()

    def _j_server(self, p: dict, wurzel: str, nur_lesen: bool) -> str:
        """Ordner freigeben: lauschen/verbinden, bedienen, bei Bedarf wieder lauschen."""
        letztes = "Server beendet"
        p = dict(p)
        while True:
            self._schliessen()
            try:
                k = self._oeffnen(p)
                self._anwenden(k)
                self._rueckrufe(k)
                sl = p.get("server_login") or {}
                if sl.get("pflicht"):
                    # Wie getty + login der WEGA; serve endet dann nur durch Trennung oder Stopp.
                    k.set_server_login(True, sl.get("name"), sl.get("kennwort"))
                self.zustand.emit("verbunden", f"Verbunden mit {k.peer}")
                self.ereignis.emit(f"Verbunden mit {k.peer} — Freigabe {wurzel}"
                                   + (" (nur lesen)" if nur_lesen else "")
                                   + (" (mit Anmeldung)" if sl.get("pflicht") else ""))
                k.serve(wurzel, nur_lesen)
                letztes = "Gegenstelle hat den Server beendet"
            except KermitError as e:
                if e.code == binding.E_DISCONNECTED:
                    letztes = "Gegenstelle hat die Leitung getrennt"
                elif self._stopp or e.code == binding.E_ABORTED:
                    self._schliessen()
                    return "Server gestoppt"
                else:
                    self._schliessen()
                    raise
            self._schliessen()
            self.ereignis.emit(letztes)
            if self._stopp or not self._wieder:
                return letztes
            self.ereignis.emit("Lausche erneut ...")
