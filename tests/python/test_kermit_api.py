"""C-ABI des Kermit-Werkzeugs: `kermit/include/k1520kermit.h` ↔ `libk1520kermit.so` ↔ ctypes.

Dieselbe Mechanik wie `test_c_api.py`/`test_disk_c_api.py` und aus demselben Grund (der
C++-Compiler prueft die Python-Seite nicht, ctypes meldet eine falsche Signatur erst beim
Aufruf — meist als Absturz).  Hier geht der Vergleich weiter als bis zu den Namen: aus den
Prototypen des Headers werden **Rueckgabe- und Argumenttypen** gelesen und mit dem verglichen,
was ctypes tatsaechlich auf der geladenen Bibliothek eingestellt hat.

Dazu ein Rundlauf: Python-Client <-> Python-Server in zwei Faeden ueber Loopback (Telnet und
RFC 2217, beide Leitungsrollen) mit echten Dateien — die Bindung soll nicht nur laden,
sondern ein ganzes Protokoll tragen.  Das Protokoll selbst ist in den C++-Tests belegt
(`kermit_*`).
"""

import ctypes
import os
import re
import threading

import pytest

from conftest import PROJECT_ROOT, requires_kermit

pytestmark = requires_kermit

HEADER = PROJECT_ROOT / "kermit" / "include" / "k1520kermit.h"


# ─── Header lesen ────────────────────────────────────────────────────────────

def _header_text() -> str:
    text = HEADER.read_text(encoding="utf-8")
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def header_prototypes() -> dict:
    """Name -> (Rueckgabetyp, [Argumenttypen]) aus den `K1520K_API`-Deklarationen."""
    protos = {}
    for m in re.finditer(r"K1520K_API\s+([^;{]*?)\b(k1520k_[a-z0-9_]+)\s*\(([^)]*)\)\s*;", _header_text()):
        ret = " ".join(m.group(1).split())
        args = []
        arglist = m.group(3).strip()
        if arglist and arglist != "void":
            for a in arglist.split(","):
                a = " ".join(a.split())
                # Parametername abschneiden: der letzte Bezeichner, wenn davor ein Typ steht.
                mm = re.match(r"(.*?[\w*])\s*\b[a-z_][a-z0-9_]*$", a)
                typ = mm.group(1) if mm and mm.group(1).strip() not in ("const", "unsigned") else a
                args.append(typ.replace(" *", "*").strip())
        protos[m.group(2)] = (ret.replace(" *", "*"), args)
    return protos


def header_functions() -> set:
    """ALLE `k1520k_*`-Funktionen des Headers — auch solche ohne `K1520K_API` (das waere ein Fehler)."""
    return set(re.findall(r"\b(k1520k_[a-z0-9_]+)\s*\(", _header_text()))


def header_callbacks() -> dict:
    """Typedef-Name -> Argumenttypen der Rueckruf-Funktionszeiger."""
    out = {}
    for m in re.finditer(r"typedef\s+void\s*\(\s*\*\s*(\w+)\s*\)\s*\(([^)]*)\)\s*;", _header_text()):
        args = []
        for a in m.group(2).split(","):
            a = " ".join(a.split())
            mm = re.match(r"(.*?[\w*])\s*\b[a-z_][a-z0-9_]*$", a)
            args.append((mm.group(1) if mm else a).replace(" *", "*").strip())
        out[m.group(1)] = args
    return out


_C2CTYPES = {
    "int": ctypes.c_int,
    "int64_t": ctypes.c_int64,
    "double": ctypes.c_double,
    "void*": ctypes.c_void_p,
    "K1520Kermit": ctypes.c_void_p,
    "const char*": ctypes.c_char_p,
    "char*": ctypes.c_char_p,
    "void": None,
}


def _passt(c_typ: str, py_typ) -> bool:
    """Passt der ctypes-Typ `py_typ` zum C-Typ `c_typ` des Headers?"""
    callbacks = header_callbacks()
    if c_typ in callbacks:
        if not (isinstance(py_typ, type) and issubclass(py_typ, ctypes._CFuncPtr)):
            return False
        erwartet = [_C2CTYPES[a] for a in callbacks[c_typ]]
        return list(py_typ._argtypes_) == erwartet and py_typ._restype_ is None
    assert c_typ in _C2CTYPES, f"Typ {c_typ!r} im Header unbekannt — Abbildung in diesem Test ergaenzen"
    return py_typ is _C2CTYPES[c_typ]


# ─── Die drei Seiten ─────────────────────────────────────────────────────────

def test_header_declares_the_expected_api():
    funcs = header_functions()
    assert HEADER.exists()
    assert len(funcs) >= 30, f"nur {len(funcs)} Funktionen im Header gefunden"
    for essential in ("k1520k_open_telnet_client", "k1520k_open_telnet_server", "k1520k_open_rfc2217_client",
                      "k1520k_open_rfc2217_server", "k1520k_listen_port", "k1520k_accept", "k1520k_login",
                      "k1520k_send", "k1520k_get", "k1520k_remote_dir", "k1520k_serve", "k1520k_abort",
                      "k1520k_close", "k1520k_last_error", "k1520k_version", "k1520k_parse_ls"):
        assert essential in funcs


def test_every_function_carries_the_export_macro():
    """Ohne `K1520K_API` exportiert eine MSVC-DLL die Funktion nicht — ctypes fand sie nie."""
    ohne = sorted(header_functions() - set(header_prototypes()))
    assert not ohne, "ohne K1520K_API (oder unlesbarer Prototyp) deklariert: " + ", ".join(ohne)


def test_every_header_function_is_exported_by_the_library():
    from app.kermittool.binding import lib
    dll = lib()
    missing = [f for f in sorted(header_functions()) if not hasattr(dll, f)]
    assert not missing, "im Header deklariert, aber nicht in libk1520kermit exportiert: " + ", ".join(missing)


def test_binding_table_matches_the_header_names_both_ways():
    from app.kermittool.binding import SIGNATUREN
    assert set(SIGNATUREN) == header_functions(), (
        "Bindung und Header weichen ab — nur im Header: "
        f"{sorted(header_functions() - set(SIGNATUREN))}, nur in der Bindung: "
        f"{sorted(set(SIGNATUREN) - header_functions())}")


def test_ctypes_signatures_match_the_header_types():
    """Rueckgabe- und Argumenttypen der geladenen Bibliothek == Prototypen des Headers."""
    from app.kermittool.binding import lib
    dll = lib()
    falsch = []
    for name, (ret, args) in sorted(header_prototypes().items()):
        fn = getattr(dll, name)
        if fn.argtypes is None:
            falsch.append(f"{name}: keine argtypes")
            continue
        if len(fn.argtypes) != len(args):
            falsch.append(f"{name}: {len(fn.argtypes)} Argumente in ctypes, {len(args)} im Header")
            continue
        if not _passt(ret, fn.restype):
            falsch.append(f"{name}: restype {fn.restype} passt nicht zu '{ret}'")
        for i, (c, p) in enumerate(zip(args, fn.argtypes)):
            if not _passt(c, p):
                falsch.append(f"{name}: Argument {i} {p} passt nicht zu '{c}'")
    assert not falsch, "\n".join(falsch)


def test_error_codes_match_the_header_enum():
    import app.kermittool.binding as b
    m = dict(re.findall(r"K1520K_(\w+)\s*=\s*(\d+)", _header_text()))
    aus_header = {"OK": 0, **{k: int(v) for k, v in m.items()}}
    assert aus_header["E_TIMEOUT"] == b.E_TIMEOUT == 1 and aus_header["E_LOGIN"] == b.E_LOGIN
    for name, wert in aus_header.items():
        assert getattr(b, name) == wert, name


def test_version_string():
    """Dieselbe Bauversion wie Kern und DiskTool (doc/ci_pipeline.md §7.2)."""
    from app.kermittool.binding import version
    from app.core_binding.k1520disk import version as disk_version
    from app import version as v
    assert version() == disk_version()
    assert version().startswith(v.lies_basis(PROJECT_ROOT))


def test_paths_resolve_the_library(monkeypatch, tmp_path):
    from app import paths
    p = paths.kermit_library()
    assert p.name.startswith(("libk1520kermit", "k1520kermit"))
    assert "Kermit-Bibliothek:" in paths.describe()
    # K1520_KERMIT_LIB hat Vorrang (Datei) — und K1520_LIB meint weiter nur den Kern.
    fake = tmp_path / p.name
    fake.write_bytes(b"")
    monkeypatch.setenv(paths.ENV_KERMIT_LIB, str(fake))
    assert paths.kermit_library() == fake.resolve()
    assert paths.core_library() != fake.resolve()


# ─── Fachliche Proben ────────────────────────────────────────────────────────

def test_parse_ls_returns_entries_as_json():
    from app.kermittool.binding import parse_ls
    text = ("total 3\n"
            "-rw-r--r-- 1 wega     system      379 Oct 18 10:11 passwd\n"
            "drwxr-xr-x 2 kermit   other        64 Oct 18 10:11 sub dir\n"
            "lrwxrwxrwx 1 wega     system        5 Jan  3  1986 l\xe4nk -> ziel\n")
    e = parse_ls(text)
    assert [x["name"] for x in e] == ["passwd", "sub dir", "l\xe4nk"]
    assert e[0]["art"] == "-" and e[0]["groesse"] == 379 and e[0]["besitzer"] == "wega"
    assert e[1]["art"] == "d" and e[2]["ziel"] == "ziel"
    assert parse_ls("") == []


def test_errors_are_exceptions_with_codes(tmp_path):
    from app.kermittool.binding import Kermit, KermitError, E_LINE, E_USAGE, E_NOT_FOUND

    # Nichts lauscht: Port eines gerade geschlossenen Servers.
    with Kermit.telnet_server(0) as s:
        frei = s.listen_port
    with pytest.raises(KermitError) as ei:
        Kermit.telnet_client("127.0.0.1", frei, 1500)
    assert ei.value.code == E_LINE and "Verbinden" in ei.value.text

    with Kermit.telnet_server(0) as s:
        assert s.listen_port > 0
        assert not s.connected
        with pytest.raises(KermitError) as ei:
            s.set_pruefsumme(9)
        assert ei.value.code == E_USAGE
        assert s.tempo() == 1.0                 # Automatik ohne Messung (K7)
        s.set_tempo(10)
        assert s.tempo() == 10.0
        s.set_tempo(0)
        with pytest.raises(KermitError) as ei:
            s.set_tempo(5000)
        assert ei.value.code == E_USAGE
        with pytest.raises(KermitError) as ei:
            s.accept(150)                       # Frist
        assert ei.value.code == E_LINE and "Frist" in ei.value.text

    k = Kermit.telnet_server(0)
    k.close()
    with pytest.raises(KermitError) as ei:      # geschlossen
        k.accept(10)
    assert ei.value.code == E_USAGE
    k.close()                                   # zweites close ist harmlos


def _paar(art: str, server_lauscht: bool):
    """Zwei verbundene Sitzungen: (Protokoll-Client, Protokoll-Server).  `server_lauscht`: die Leitung des
    Protokoll-SERVERS lauscht (sonst verbindet sie und der Protokoll-Client lauscht)."""
    from app.kermittool.binding import Kermit

    def lausche():
        return Kermit.telnet_server(0) if art == "telnet" else Kermit.rfc2217_server(0)

    def verbinde(port):
        return (Kermit.telnet_client("127.0.0.1", port) if art == "telnet"
                else Kermit.rfc2217_client("127.0.0.1", port))

    lauscher = lausche()
    port = lauscher.listen_port
    ergebnis = {}
    t = threading.Thread(target=lambda: ergebnis.update(k=verbinde(port)))
    t.start()
    lauscher.accept(5000)
    t.join()
    return (ergebnis["k"], lauscher) if not server_lauscht else (lauscher, ergebnis["k"])


@pytest.mark.parametrize("art", ["telnet", "rfc2217"])
@pytest.mark.parametrize("server_lauscht", [True, False], ids=["serverLauscht", "serverVerbindet"])
def test_python_client_and_server_round_trip(tmp_path, art, server_lauscht):
    from app.kermittool.binding import Kermit, KermitError, parse_ls, E_NOT_FOUND

    client, server = _paar(art, server_lauscht)
    freigabe = tmp_path / "freigabe"
    freigabe.mkdir()
    lokal = tmp_path / "lokal"
    lokal.mkdir()
    ziel = tmp_path / "ziel"
    ziel.mkdir()
    inhalt = os.urandom(60_000) + b"\r\n\r\0\xff\xff"
    (lokal / "daten.bin").write_bytes(inhalt)

    server_fehler = []

    def serve():
        try:
            server.serve(str(freigabe))
        except KermitError as e:
            server_fehler.append(e)

    t = threading.Thread(target=serve)
    t.start()
    try:
        fortschritt, ereignisse = [], []
        client.set_callbacks(lambda d, b, g: fortschritt.append((d, b, g)), ereignisse.append)
        client.set_pruefsumme(3)
        client.set_paketlaenge(1000)

        assert client.send(str(lokal / "daten.bin"), "daten.bin") == [str(lokal / "daten.bin")]
        assert (freigabe / "daten.bin").read_bytes() == inhalt
        assert fortschritt and fortschritt[-1][1] == len(inhalt) and fortschritt[-1][2] == len(inhalt)
        assert ereignisse, "Ereignisrueckruf wurde nie gerufen"

        eintraege = parse_ls(client.remote_dir(""))
        assert [(e["name"], e["groesse"]) for e in eintraege] == [("daten.bin", len(inhalt))]

        assert client.get("daten.bin", str(ziel)) == ["daten.bin"]
        assert (ziel / "daten.bin").read_bytes() == inhalt

        (freigabe / "unter").mkdir()
        client.remote_cd("unter")
        assert parse_ls(client.remote_dir("")) == []        # leerer Unterordner
        client.remote_cd("..")
        client.remote_delete("daten.bin")
        assert not (freigabe / "daten.bin").exists()

        with pytest.raises(KermitError) as ei:
            client.send(str(lokal / "gibtsnicht.bin"), "x")
        assert ei.value.code == E_NOT_FOUND

        client.set_callbacks(None, None)   # Abschalten: keine Abstuerze beim Weiterarbeiten
        client.remote_finish()
    finally:
        client.abort()
        t.join(30)
        client.close()
        server.close()
    assert not t.is_alive()
    assert server_fehler == []


def test_abort_ends_a_blocking_serve(tmp_path):
    from app.kermittool.binding import KermitError, E_ABORTED

    client, server = _paar("telnet", True)
    fehler = []
    t = threading.Thread(target=lambda: _serve_sammeln(server, tmp_path, fehler))
    t.start()
    threading.Event().wait(0.3)
    server.abort()
    t.join(10)
    assert not t.is_alive(), "abort() hat serve() nicht beendet"
    assert len(fehler) == 1 and fehler[0].code == E_ABORTED
    client.close()
    server.close()


def test_abort_racing_close_does_not_touch_a_freed_handle(tmp_path):
    """Waechter fuer den SEGFAULT in k1520k_abort: die GUI bricht ab, waehrend der Arbeitsfaden die
    Sitzung schliesst (Use-after-free zwischen `if self._h` und dem Aufruf).  Ohne die Sperre in
    Kermit.close()/abort*() stuerzt der Prozess nach wenigen Runden ab."""
    for _ in range(40):
        client, server = _paar("telnet", True)
        stopp = threading.Event()

        def hammer(k=server):
            while not stopp.is_set():
                k.abort()
                k.abort_file()
                k.abort_group()

        t = threading.Thread(target=hammer)
        t.start()
        client.close()
        server.close()
        stopp.set()
        t.join(10)
        assert not t.is_alive()


def _serve_sammeln(server, wurzel, fehler):
    from app.kermittool.binding import KermitError
    try:
        server.serve(str(wurzel))
    except KermitError as e:
        fehler.append(e)


# ─── Anmeldung auf Abruf (K10) ───────────────────────────────────────────────

class _GettyAttrappe:
    """Spielt getty/login nach (Klartext-TCP; die Telnet-Aushandlung des Clients wird ueberlesen):
    `login:` -> Name -> `Password:` -> Kennwort -> bei richtig der Kermit-Serverbeginn, sonst
    `Login incorrect` und wieder `login:`."""

    def __init__(self, benutzer="kermit", kennwort="kermit"):
        import socket
        self.benutzer, self.kennwort = benutzer, kennwort
        self.lauscher = socket.socket()
        self.lauscher.bind(("127.0.0.1", 0))
        self.lauscher.listen(1)
        self.port = self.lauscher.getsockname()[1]
        self.erhalten = []   # (name, kennwort)-Paare
        self.thread = threading.Thread(target=self._lauf, daemon=True)
        self.thread.start()

    def _zeile(self, c):
        z = b""
        while True:
            b = c.recv(1)
            if not b:
                return None
            if b == b"\xff":          # IAC + 2 Bytes ueberlesen
                c.recv(2)
                continue
            if b == b"\r":
                return z.decode("latin-1")
            if b not in (b"\n", b"\0"):
                z += b

    def _lauf(self):
        c, _ = self.lauscher.accept()
        c.settimeout(20)
        try:
            if self._zeile(c) is None:        # erstes RETURN des Clients
                return
            while True:
                c.sendall(b"\r\nWEGA login: ")
                n = self._zeile(c)
                if n is None:
                    return
                c.sendall(b"\r\nPassword: ")
                k = self._zeile(c)
                if k is None:
                    return
                self.erhalten.append((n, k))
                if (n, k) == (self.benutzer, self.kennwort):
                    break
                c.sendall(b"\r\nLogin incorrect\r\n")
            c.sendall(b"\r\nC-Kermit server starting.  Return to your local machine\r\n")
            while c.recv(100):
                pass
        except OSError:
            pass
        finally:
            c.close()

    def schliessen(self):
        self.lauscher.close()


def _anmelde_client(getty):
    from app.kermittool.binding import Kermit
    return Kermit.telnet_client("127.0.0.1", getty.port, 5000)


def test_login_on_demand_asks_again_after_rejection():
    from app.kermittool import binding as b
    getty = _GettyAttrappe()
    k = _anmelde_client(getty)
    fragen = []

    def frage(versuch, gelesen):
        fragen.append((versuch, gelesen))
        return ("kermit", "falsch") if versuch == 0 else ("kermit", "kermit")

    try:
        text = k.login_ask(frage, 20000)
        assert k.login_kind == b.LOGIN_OK
        assert "server starting" in text
        assert [f[0] for f in fragen] == [0, 1]
        assert "login:" in fragen[0][1] and "Login incorrect" in fragen[1][1]
        assert getty.erhalten == [("kermit", "falsch"), ("kermit", "kermit")]
    finally:
        k.close()
        getty.schliessen()


def test_login_on_demand_without_answer_is_an_abort():
    from app.kermittool import binding as b
    getty = _GettyAttrappe()
    k = _anmelde_client(getty)
    try:
        with pytest.raises(b.KermitError) as ei:
            k.login_ask(lambda v, g: None, 20000)      # der Anwender bricht ab
        assert ei.value.code == b.E_LOGIN
        assert k.login_kind == b.LOGIN_ABBRUCH
        assert getty.erhalten == []
    finally:
        k.close()
        getty.schliessen()


def test_login_kind_tells_a_rejection_from_a_timeout():
    from app.kermittool import binding as b
    getty = _GettyAttrappe()
    k = _anmelde_client(getty)
    try:
        with pytest.raises(b.KermitError):
            k.login("kermit", "falsch", 3000)            # Anmeldung mit vorab gegebenen Daten
        assert k.login_kind == b.LOGIN_ABGELEHNT
    finally:
        k.close()
        getty.schliessen()

    # Stumme Gegenstelle: Frist, nicht Ablehnung.
    import socket
    stumm = socket.socket()
    stumm.bind(("127.0.0.1", 0))
    stumm.listen(1)
    k = b.Kermit.telnet_client("127.0.0.1", stumm.getsockname()[1], 5000)
    try:
        with pytest.raises(b.KermitError):
            k.login_ask(lambda v, g: ("kermit", "kermit"), 600)
        assert k.login_kind == b.LOGIN_FRIST
    finally:
        k.close()
        stumm.close()


# ─── Server mit Anmeldezwang (K12) ───────────────────────────────────────────

def test_server_login_enforces_the_login_and_returns_to_login_after_finish(tmp_path):
    from app.kermittool.binding import Kermit, KermitError, E_DISCONNECTED, E_LOGIN

    client, server = _paar("telnet", True)
    freigabe = tmp_path / "freigabe"
    freigabe.mkdir()
    (freigabe / "da.txt").write_bytes(b"da")
    ziel = tmp_path / "ziel"
    ziel.mkdir()
    server.set_server_login(True, "olli", "geheim")
    fehler = []
    t = threading.Thread(target=lambda: _serve_sammeln(server, freigabe, fehler))
    t.start()
    try:
        client.login("olli", "geheim", 30000)
        assert client.get("da.txt", str(ziel)) == ["da.txt"]
        client.remote_finish()                   # der Server meldet sich wieder mit login:
        assert t.is_alive(), "mit Anmeldezwang endet serve() nach FINISH nicht"
        with pytest.raises(KermitError) as ei:   # falsches Kennwort
            client.login("olli", "falsch", 3000)
        assert ei.value.code == E_LOGIN
        client.login("olli", "geheim", 30000)    # derselbe Server, naechste Anmeldung
        assert client.get("da.txt", str(ziel)) == ["da.txt"]
    finally:
        client.close()
        t.join(30)
        server.close()
    assert not t.is_alive()
    assert len(fehler) == 1 and fehler[0].code == E_DISCONNECTED


def test_server_login_defaults_are_off_and_keep_names_when_none(tmp_path):
    from app.kermittool.binding import Kermit

    client, server = _paar("telnet", True)
    try:
        server.set_server_login(False)           # Aufruf ohne Namen/Kennwort ist erlaubt
        server.set_server_login(True, None, None)
    finally:
        client.close()
        server.close()
