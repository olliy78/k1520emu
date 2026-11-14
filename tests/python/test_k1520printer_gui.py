"""Die Oberflaeche von K1520 Printer (``app/printertool/``, Entwurf 49 WP5) - headless (offscreen).

Geprueft wird die Verdrahtung UND das Ergebnis: der Selbsttest ueber die Tasten druckt Punkte ins
Blatt-Widget (gleich dem Blatt im Kern), der Kopfmarker bewegt sich, Lampen folgen dem Kern,
die gesperrten Typen sind nicht waehlbar, das PDF enthaelt die Punkte an den richtigen Stellen
(das PDF wird zurueck in ein Bild gerechnet), der Druckdialog laeuft mit Ersatz, ein Telnet-Rechner
druckt Text ueber die serielle Schnittstelle, und die Kuerzeltabelle des Handbuchs stimmt in beiden
Richtungen.
"""

import socket
import time
from pathlib import Path

import pytest

PySide6 = pytest.importorskip("PySide6")

from PySide6.QtGui import QAction                                      # noqa: E402
from PySide6.QtWidgets import QMessageBox                              # noqa: E402

from app.core_binding import k1520 as K                                # noqa: E402
from app.printertool import ausgabe, konfig, typen                     # noqa: E402

BLOCK = 20000          # Takte je Rechenschritt der Tests


# ─── Hilfen ──────────────────────────────────────────────────────────────────

@pytest.fixture
def fenster(qapp, tmp_path):
    """Fenster ohne Auslieferungsvorgabe (die startet einen Server auf Port 5000) und ohne Takt:
    der Test faehrt den Kern selbst (``lauf``) und zieht die Oberflaeche nach (``aktualisieren``)."""
    from app.printertool.fenster import PrinterWindow
    f = PrinterWindow(konfig_daten={}, konfig_pfad=str(tmp_path / "k1520printer.yaml"), autostart=False)
    f.resize(1200, 700)
    f.show()
    yield f
    f.takt.beenden()
    f.close()


def lauf(f, bloecke, schritt=BLOCK, aktualisieren_jeden=1, beobachter=None):
    for i in range(bloecke):
        f.emu.run(schritt)
        if beobachter:
            beobachter()
        if i % aktualisieren_jeden == 0:
            f.aktualisieren()
    f.aktualisieren()


def selbsttest(f):
    """ONLINE aus, Neustart, Grundstellung abwarten, TEST halten, Selbsttest drucken lassen."""
    f.bedienfeld.knopf("online").click()
    f.act_reset.trigger()
    f.takt.stop()                                   # der Neustart startet den Takt: der Test fuehrt selbst
    lauf(f, 250, aktualisieren_jeden=25)
    f.bedienfeld.knopf("test").pressed.emit()
    lauf(f, 500, aktualisieren_jeden=5)
    f.bedienfeld.knopf("test").released.emit()


def kern_blatt(emu):
    breite, hoehe = emu.printer_sheet_size()
    return breite, hoehe, emu.printer_sheet_rows(0, hoehe)


# ─── Aufbau ──────────────────────────────────────────────────────────────────

def test_fenster_hat_blatt_bedienfeld_einstellungen_und_schnittstellen(fenster):
    assert fenster.windowTitle() == "K1520 Printer"
    assert fenster.centralWidget() is fenster.ansicht
    assert fenster.blatt.blatt_breite() == 3000
    b = fenster.bedienfeld
    assert [t.name for t in b.beschreibung.tasten] == ["online", "pv1", "pv2", "test", "kopf", "lpi6", "papier"]
    assert set(b._lampen) == {"Netz", "ONLINE", "FEHLER"}
    assert [x.info.name for x in fenster.seriell.bloecke()] == ["IFSS", "V.24"]
    assert fenster.reiter.tabText(0) == "Drucker" and fenster.reiter.tabText(1) == "Schnittstellen"
    # alle Tasten und Schalter sind Kern-Elemente: die rastenden zeigen den Kernzustand
    assert b.knopf("online").isChecked() and b.knopf("lpi6").isChecked() and b.knopf("papier").isChecked()
    assert not b.knopf("kopf").isChecked()


def test_der_kern_ist_ein_eigener_faden_und_die_oberflaeche_laeuft_ihn_mit(qapp, tmp_path):
    """Mit Takt: Maschine rechnet im eigenen Faden (Tempo unbegrenzt), der Selbsttest druckt ins Widget."""
    from app.printertool.fenster import PrinterWindow
    f = PrinterWindow(konfig_daten={"general": {"speed": 0.0}, "printer": {"online": False}},
                      konfig_pfad=str(tmp_path / "c.yaml"))
    try:
        assert f.takt.isActive()
        # online=False aus der Konfiguration: der Drucker startet offline (Lampe bleibt dunkel)
        t0 = time.monotonic()
        while time.monotonic() - t0 < 1.5:                   # Grundstellung (~9 Mio. Takte)
            qapp.processEvents()
            time.sleep(0.01)
        assert f.emu.printer_lamp(K.DRUCKER_LAMPE_ONLINE) == 0
        f.bedienfeld.knopf("test").pressed.emit()
        t0 = time.monotonic()
        while time.monotonic() - t0 < 30 and f.blatt.punkte_zaehlen() < 1000:
            qapp.processEvents()
            time.sleep(0.01)
        f.bedienfeld.knopf("test").released.emit()
        assert f.blatt.punkte_zaehlen() >= 1000
    finally:
        f.close()


# ─── Selbsttest: Blatt, Kopf, Lampen ────────────────────────────────────────

def test_selbsttest_ueber_die_tasten_druckt_ins_blatt_der_kopfmarker_faehrt_und_das_blatt_waechst(fenster):
    xs, hoehen, lampen = set(), [], []

    def beobachte():
        fenster.aktualisieren()
        xs.add(fenster.blatt.kopf()[0])
        hoehen.append(fenster.blatt.blatt_hoehe())
        lampen.append(fenster.bedienfeld.lampe("ONLINE").an())

    fenster.bedienfeld.knopf("online").click()
    fenster.act_reset.trigger()
    fenster.takt.stop()
    lauf(fenster, 250, aktualisieren_jeden=25)
    h0 = fenster.blatt.blatt_hoehe()
    assert fenster.blatt.punkte_zaehlen() == 0
    fenster.bedienfeld.knopf("test").pressed.emit()
    lauf(fenster, 500, beobachter=beobachte)
    fenster.bedienfeld.knopf("test").released.emit()

    # Punkte im Widget: gleich dem Blatt im Kern (die Teil-Abfrage ab Kopfzeile verliert nichts)
    breite, hoehe, kern = kern_blatt(fenster.emu)
    assert hoehe > h0 + 50
    k = fenster.blatt.kopie()
    assert (k.breite, k.hoehe) == (breite, hoehe)
    assert k.daten == kern
    assert fenster.blatt.punkte_zaehlen() == sum(bin(b).count("1") for b in kern) > 5000
    # Blatt waechst monoton, der Kopfmarker faehrt ueber die Breite des Blatts
    assert hoehen == sorted(hoehen) and hoehen[-1] > hoehen[0]
    assert max(xs) - min(xs) > 1000
    # Lampen: offline gestartet -> ONLINE-Lampe aus, FEHLER aus, Netz an
    assert not any(lampen)
    assert fenster.bedienfeld.lampe("Netz").an() and not fenster.bedienfeld.lampe("FEHLER").an()
    assert "offline" in fenster.st_zustand.text()
    assert "Kopf x" in fenster.st_kopf.text()


def test_blatt_folgen_haelt_die_kopfzeile_im_bild_und_rollen_beendet_das_folgen(fenster):
    b = fenster.blatt
    b.set_zoom(3.0)                       # so gross, dass der Selbsttest (222 Zeilen) die Ansicht ueberragt
    oys = []
    fenster.bedienfeld.knopf("online").click()
    fenster.act_reset.trigger()
    fenster.takt.stop()
    lauf(fenster, 250, aktualisieren_jeden=25)
    fenster.bedienfeld.knopf("test").pressed.emit()
    lauf(fenster, 500, beobachter=lambda: (fenster.aktualisieren(), oys.append(b.ansicht_ursprung()[1])))
    fenster.bedienfeld.knopf("test").released.emit()
    assert b.folgen() and fenster.act_folgen.isChecked()
    _, sh = b.ansicht_masse()
    ox, oy = b.ansicht_ursprung()
    assert oy <= b.kopf()[1] <= oy + sh                       # Kopfzeile im Bild
    # das Blatt hat sich waehrend des Drucks nach oben geschoben: der Ausschnitt wandert nur nach unten
    assert oy > 0 and oys == sorted(oys) and oys[-1] > oys[0]
    # Von Hand rollen beendet "Blatt folgen"
    fenster.ansicht.vbar.setValue(0)
    assert not fenster.act_folgen.isChecked() and not b.folgen()
    assert b.ansicht_ursprung()[1] == 0
    fenster.act_folgen.setChecked(True)
    assert b.ansicht_ursprung()[1] > 0


def test_lampen_folgen_dem_kern_fehlerlampe_und_netz_aus(fenster):
    lauf(fenster, 250, aktualisieren_jeden=50)
    assert fenster.bedienfeld.lampe("ONLINE").an() and fenster.emu.printer_lamp(K.DRUCKER_LAMPE_ONLINE) == 1
    assert fenster.st_zustand.text() == "online"
    fenster.emu.nmi()                                          # Farbbandhavarie
    lauf(fenster, 5)
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_FEHLER) == 1
    assert fenster.bedienfeld.lampe("FEHLER").an() and "FEHLER" in fenster.st_zustand.text()
    # Netz aus: Lampen dunkel, Takt steht, Tasten gesperrt, Blatt bleibt
    punkte = fenster.blatt.punkte_zaehlen()
    fenster.act_power.setChecked(False)
    assert not fenster.takt.isActive() and not fenster.bedienfeld.netz_an()
    assert not any(l.an() for l in fenster.bedienfeld._lampen.values())
    assert not fenster.bedienfeld.knopf("test").isEnabled()
    assert fenster.st_zustand.text() == "Netz aus" and fenster.blatt.punkte_zaehlen() == punkte
    # Netz ein: Neustart, Lampen kommen wieder
    fenster.act_power.setChecked(True)
    fenster.takt.stop()
    lauf(fenster, 250, aktualisieren_jeden=50)
    assert fenster.bedienfeld.lampe("Netz").an() and fenster.bedienfeld.lampe("ONLINE").an()


def test_online_ohne_neustart_zeigt_den_hinweis_zu_test(fenster):
    lauf(fenster, 250, aktualisieren_jeden=50)
    assert not fenster.bedienfeld.warnung.isVisibleTo(fenster.bedienfeld)
    fenster.act_online.setChecked(False)                      # im Betrieb: Lampe bleibt an (Gastverhalten)
    lauf(fenster, 5)
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_ONLINE) == 0
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_ONLINE) == 1
    assert fenster.bedienfeld.warnung.isVisibleTo(fenster.bedienfeld)
    assert "Neustart" in fenster.bedienfeld.warntext.text()
    assert "OFFLINE eingeschaltet" in fenster.bedienfeld.hinweise.text()   # der dauernde Hinweis am TEST


def test_blatt_leeren_und_papier_taste(fenster):
    selbsttest(fenster)
    assert fenster.blatt.punkte_zaehlen() > 0
    fenster.act_blatt_leeren.trigger()
    assert fenster.blatt.punkte_zaehlen() == 0
    assert kern_blatt(fenster.emu)[2] == bytes(len(kern_blatt(fenster.emu)[2]))
    fenster.bedienfeld.knopf("papier").click()
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_PAPIER) == 0


# ─── Typauswahl, Neuanlage ───────────────────────────────────────────────────

def test_typauswahl_sperrt_alle_unfertigen_typen(fenster, monkeypatch):
    e = fenster.einst_drucker
    items = {e.typ.itemData(i): e.typ.model().item(i) for i in range(e.typ.count())}
    assert items["sd1157"].isEnabled() and items["k6316-ifss"].isEnabled() and items["k6316-v24"].isEnabled()
    assert items["k6313"].isEnabled() and items["k6311"].isEnabled()
    assert items["k6313-ifss"].isEnabled() and items["k6311-ifss"].isEnabled()
    gesperrt = {"sd1157-267", "sd1152-257"}
    assert gesperrt <= set(items)
    for s in gesperrt:
        assert not items[s].isEnabled() and items[s].toolTip()
    # Die Wahl eines gesperrten Typs aendert nichts und fragt nicht einmal (conftest laesst jede Frage scheitern)
    emu = fenster.emu
    e.typ.setCurrentIndex(e.typ.findData("sd1152-257"))
    assert fenster.emu is emu and e.typ.currentData() == "sd1157"
    assert "In Vorbereitung" in e.sperrgrund("sd1152-257") and "Nicht vorgesehen" in e.sperrgrund("sd1157-267")


def test_rom_und_schnittstellenwechsel_legen_neu_an_nach_rueckfrage(fenster, monkeypatch):
    e = fenster.einst_drucker
    emu0 = fenster.emu

    def rom(emu):
        return K._lib.k1520_mem_read(emu._handle, 0x0D34)

    assert rom(emu0) == 0x0B                                   # repariert (Vorgabe): DEC BC
    antworten = []
    monkeypatch.setattr(QMessageBox, "question",
                        staticmethod(lambda *a, **k: antworten.append(a[1]) or QMessageBox.No))
    e.rom.setCurrentIndex(e.rom.findData("sd1157-original"))
    assert fenster.emu is emu0 and e.rom.currentData() == "sd1157-repariert" and len(antworten) == 1
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    e.rom.setCurrentIndex(e.rom.findData("sd1157-original"))
    assert fenster.emu is not emu0 and rom(fenster.emu) == 0x2B and fenster._rom == "sd1157-original"
    assert fenster.takt.isActive()
    fenster.takt.stop()
    # Schnittstelle: v24 am Schalterblock -> Neuanlage; der Kern meldet den Aufbau, die Anzeige folgt
    e.schnittstelle.setCurrentIndex(e.schnittstelle.findData("v24"))
    assert fenster._schnittstelle == "v24" and "V.24" in fenster.st_schnittstelle.text()
    assert fenster.emu.serial_count() == 2 and fenster.seriell.emulator is fenster.emu
    fenster.takt.stop()
    assert fenster.konfig_lesen()["printer"]["rom"]["sd1157"] == "sd1157-original"


def test_konfiguration_rundlauf(fenster, tmp_path):
    from app.printertool.fenster import PrinterWindow
    fenster.act_online.setChecked(False)
    fenster.einst_drucker.seite.setCurrentIndex(fenster.einst_drucker.seite.findData(6))
    fenster.einst_drucker.tempo.setCurrentIndex(fenster.einst_drucker.tempo.findData(5.0))
    d = fenster.konfig_lesen()
    pfad = str(tmp_path / "rund.yaml")
    assert konfig.speichern(d, pfad)
    g = PrinterWindow(konfig_daten=konfig.laden(pfad), konfig_pfad=pfad, autostart=False)
    try:
        assert g.emu.printer_switch_get(K.DRUCKER_SW_ONLINE) == 0
        assert g._formular_zoll == 6 and g._tempo == 5.0 and g.beschreibung.schluessel == "sd1157"
        assert g.blatt._formular_zeilen == 6 * 144
    finally:
        g.takt.beenden()
        g.close()
    # Die mitgelieferte Vorgabe ist lesbar und kennt den Drucker
    assert konfig.standard()["general"]["model"] == "sd1157"
    assert konfig.zoll_waehlbar(7) == 12 and konfig.tempo_waehlbar("x") == 1.0
    assert typen.typ("sd1152-257").schluessel == "sd1157"     # gesperrt/unbekannt -> Vorgabe
    assert typen.typ("k6313").schluessel == "k6313"
    assert typen.typ("k6311").schluessel == "k6311"
    assert typen.typ("k6313-ifss").schnittstelle_fest == "ifss" and typen.typ("k6311-ifss").schnittstelle_fest == "ifss"
    assert typen.typ("k6313").schnittstelle_fest == "v24" and typen.typ("k6311").schnittstelle_fest == "v24"


# ─── Ausgabe: PDF und Druck ──────────────────────────────────────────────────

def _pdf_bild(pfad, zoll_hoch, seite=0, dpi=200):
    """Seite ``seite`` des PDFs als Graubild mit ``dpi`` Punkten je Zoll (QtPdf)."""
    QtPdf = pytest.importorskip("PySide6.QtPdf")
    from PySide6.QtCore import QSize
    from PySide6.QtCore import Qt
    from PySide6.QtGui import QImage, QPainter
    doc = QtPdf.QPdfDocument()
    assert doc.load(str(pfad)) == QtPdf.QPdfDocument.Error.None_
    roh = doc.render(seite, QSize(15 * dpi, int(zoll_hoch * dpi)))
    papier = QImage(roh.size(), QImage.Format_ARGB32)      # die Seite kommt mit durchsichtigem Grund
    papier.fill(Qt.white)
    p = QPainter(papier)
    p.drawImage(0, 0, roh)
    p.end()
    return doc.pageCount(), papier.convertToFormat(QImage.Format_Grayscale8)


def test_pdf_enthaelt_alle_punkte_an_ihren_stellen(fenster, tmp_path):
    selbsttest(fenster)
    k = fenster.blatt.kopie()
    erwartet = sum(bin(b).count("1") for b in k.daten) / 2      # Punkte = Pixelpaare
    fenster.einst_drucker.seite.setCurrentIndex(fenster.einst_drucker.seite.findData(4))
    pdf = tmp_path / "blatt.pdf"
    assert fenster._pdf_speichern(str(pdf))
    assert pdf.read_bytes().startswith(b"%PDF")
    assert ausgabe.punkte_zaehlen(k) == erwartet                 # Zerlegung der Pixelfolgen in Punkte
    seiten, bild = _pdf_bild(pdf, 4)
    assert seiten == 1 and (bild.width(), bild.height()) == (3000, 800)
    daten = bytes(bild.constBits())[:bild.bytesPerLine() * bild.height()]
    breite = bild.bytesPerLine()
    # Jeder Punkt des Blatts liegt als dunkler Fleck an seiner Stelle (Raster 200 x 144 -> 200 x 200 dpi)
    dunkel = fehlt = 0
    for x, y, _b in ausgabe.punkte(k, 0, k.hoehe):
        px, py = int(x), int((y + 0.5) * 200 / 144)
        dunkel += 1
        if daten[py * breite + px] > 110:
            fehlt += 1
    assert dunkel == erwartet and fehlt <= dunkel * 0.01, (fehlt, dunkel)
    # Und nirgends sonst: Pixel weit weg von jedem Punkt sind weiss (Stichprobe auf einem Gitter)
    nah = {(int(x) // 8, int((y + 0.5) * 200 / 144) // 8) for x, y, _ in ausgabe.punkte(k, 0, k.hoehe)}
    fremd = 0
    for gy in range(0, 800 // 8):
        for gx in range(0, 3000 // 8):
            if any((gx + dx, gy + dy) in nah for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                continue
            if daten[(gy * 8 + 4) * breite + gx * 8 + 4] < 200:
                fremd += 1
    assert fremd == 0


def test_pdf_bricht_an_der_seitenlaenge_um_und_laesst_leere_seiten_weg(fenster, tmp_path):
    selbsttest(fenster)
    k = fenster.blatt.kopie()
    fz = k.formular_zeilen(4)
    assert ausgabe.seiten(k, fz) == [(0, fz)]                    # Selbsttest + Formularvorschub: eine Seite Text
    gross = ausgabe.BlattKopie(k.breite, fz * 2 + 10, k.daten[:k.zb * k.hoehe] + bytes(k.zb * (fz * 2 + 10 - k.hoehe)),
                               k.xdpi, k.ydpi)
    # mit Punkt auf der dritten Seite: drei Seiten, mit leerer zweiter Seite
    zeilen = bytearray(gross.daten)
    zeilen[(fz * 2 + 3) * gross.zb] = 0x80
    gross = ausgabe.BlattKopie(gross.breite, gross.hoehe, bytes(zeilen), gross.xdpi, gross.ydpi)
    assert [s[0] for s in ausgabe.seiten(gross, fz)] == [0, fz * 2]
    n = ausgabe.pdf_speichern(str(tmp_path / "g.pdf"), gross, 4)
    assert n == ausgabe.punkte_zaehlen(gross)


def test_drucken_ueber_den_systemdialog_mit_ersatz(fenster, tmp_path, monkeypatch):
    from PySide6.QtPrintSupport import QPrintDialog, QPrinter
    selbsttest(fenster)
    ziel = tmp_path / "gedruckt.pdf"

    class Ersatzdialog:
        def __init__(self, printer, parent=None):
            printer.setOutputFormat(QPrinter.PdfFormat)
            printer.setOutputFileName(str(ziel))
            self.printer = printer

        def exec(self):
            return QPrintDialog.Accepted

    fenster._druckdialog = Ersatzdialog
    assert fenster._drucken()
    assert ziel.read_bytes().startswith(b"%PDF")
    QtPdf = pytest.importorskip("PySide6.QtPdf")
    from PySide6.QtCore import QSize
    doc = QtPdf.QPdfDocument()
    assert doc.load(str(ziel)) == QtPdf.QPdfDocument.Error.None_
    assert doc.pageCount() >= 1
    seite = doc.render(0, QSize(1200, 900)).convertToFormat(PySide6.QtGui.QImage.Format_Grayscale8)
    dunkel = sum(1 for b in bytes(seite.constBits())[:seite.bytesPerLine() * seite.height()] if b < 128)
    assert dunkel > 200                                           # die Punkte sind da (eingepasst, aber da)

    class Abbruch(Ersatzdialog):
        def exec(self):
            return QPrintDialog.Rejected

    fenster._druckdialog = Abbruch
    ziel.unlink()
    assert not fenster._drucken() and not ziel.exists()


# ─── Serielle Schnittstelle: ein Rechner druckt ueber Telnet ─────────────────

def test_ein_telnet_rechner_druckt_text_ueber_die_ifss(fenster):
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    emu = fenster.emu
    assert emu.serial_configure(0, betriebsart=K.SER_TELNET, rolle=K.SER_SERVER, port=port, loop=False)
    assert emu.serial_start(0)
    gast = socket.create_connection(("127.0.0.1", port), timeout=5)
    gast.setblocking(False)
    empfangen = b""
    gesendet = False
    t0 = time.monotonic()
    try:
        while time.monotonic() - t0 < 60:
            emu.run(50000)
            time.sleep(0.0005)
            try:
                empfangen += gast.recv(100)
            except (BlockingIOError, InterruptedError):
                pass
            if not gesendet and b"\x11" in empfangen:            # DC1: der Drucker nimmt Daten an
                gast.sendall(b"K1520 PRINTER\r\n")
                gesendet = True
            fenster.aktualisieren()
            if gesendet and fenster.blatt.punkte_zaehlen() > 100 and b"\x11" in empfangen[empfangen.index(b"\x11") + 1:]:
                break
        fenster.seriell.aktualisieren()
        assert "IFSS verbunden" in fenster.st_verbindung.text.text()      # Statuszeile: Schnittstelle + Zustand
        assert str(port) in fenster.st_server.text.text()
    finally:
        gast.close()
    assert gesendet, "kein DC1 vom Drucker"
    assert fenster.blatt.punkte_zaehlen() > 100
    # DC3 bei Druckbeginn, DC1 wieder bei leerem Puffer (Flusssteuerung der Firmware)
    assert b"\x13" in empfangen


# ─── Kuerzel, Handbuch ───────────────────────────────────────────────────────

def test_jede_bedienung_ist_eine_qaction_und_jedes_kuerzel_traegt_strg_umschalt(fenster):
    from app.printertool import aktionen
    for n in aktionen.namen():
        assert isinstance(getattr(fenster, f"act_{n}"), QAction)
    fremd = []
    for a in fenster.findChildren(QAction):
        k = a.shortcut().toString()
        if k and k != "F11" and not k.startswith("Ctrl+Shift+"):
            fremd.append(f"{k} ({a.text()})")
    assert fremd == []
    # Menue und Leiste zeigen dieselben Aktionen
    leiste = [a for a in fenster.leiste.actions() if not a.isSeparator()]
    assert fenster.act_pdf in leiste and fenster.act_power in leiste


_TASTEN = {"Strg": "Ctrl", "Umschalt": "Shift", "Entf": "Del", "Eingabe": "Return"}


def _als_kuerzel(deutsch):
    from PySide6.QtGui import QKeySequence
    return QKeySequence("+".join(_TASTEN.get(t, t) for t in deutsch.split("+"))).toString()


def _handbuch_kuerzel():
    from app.printertool.fenster import HANDBUCH
    from app.ui_help import lade_handbuch
    tabelle = lade_handbuch(HANDBUCH).split("## Tastenkürzel", 1)[1].split("\n## ", 1)[0]
    out = {}
    for zeile in tabelle.splitlines():
        if not zeile.startswith("|") or "---" in zeile:
            continue
        sp = [x.strip() for x in zeile.strip("|").split("|")]
        if len(sp) == 2 and sp[0] != "Kürzel":
            out[sp[0]] = sp[1]
    return out


def test_kuerzeltabelle_des_handbuchs_ist_ein_vertrag_in_beide_richtungen(fenster):
    vorhanden = {a.shortcut().toString() for a in fenster.findChildren(QAction) if not a.shortcut().isEmpty()}
    buch = {_als_kuerzel(k) for k in _handbuch_kuerzel()}
    assert [k for k in _handbuch_kuerzel() if _als_kuerzel(k) not in vorhanden] == []
    assert sorted(vorhanden - buch) == []


def test_handbuch_liegt_im_app_baum_und_nennt_die_gastbesonderheiten(fenster):
    from app.printertool.fenster import HANDBUCH
    from app.ui_help import abschnitte, lade_handbuch
    assert HANDBUCH.is_file() and "app" in HANDBUCH.parts
    text = lade_handbuch(HANDBUCH)
    assert text.startswith("# K1520 Printer") and "{{VERSION}}" not in text
    for stichwort in ("DC1/DC3", "TEST wirkt nur nach einem Neustart", "repariert", "Telnet-Server auf Port 5000"):
        assert stichwort in text
    fenster.open_help().close()
    assert "Tastenkürzel" in abschnitte(text)


def test_programm_zeigt_hilfe_und_pfade_ohne_qt():
    import subprocess
    import sys
    from conftest import PROJECT_ROOT
    for arg, erwartet in (("--help", "k1520printer"), ("--paths", "k1520")):
        r = subprocess.run([sys.executable, str(PROJECT_ROOT / "app" / "printertool" / "main.py"), arg],
                           capture_output=True, text=True, timeout=60, cwd=PROJECT_ROOT)
        assert r.returncode == 0, r.stderr
        assert erwartet in r.stdout


# ─── K6316 (WP2a) ────────────────────────────────────────────────────────────

def test_k6316_typwahl_baut_bedienfeld_dil_und_betriebsart(fenster, monkeypatch):
    e = fenster.einst_drucker
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    emu0 = fenster.emu
    e.typ.setCurrentIndex(e.typ.findData("k6316-ifss"))
    fenster.takt.stop()
    assert fenster.emu is not emu0 and fenster.beschreibung.schluessel == "k6316-ifss"
    assert fenster.emu.machine == "k6316-ifss" and "IFSS" in fenster.st_schnittstelle.text()
    b = fenster.bedienfeld
    assert b.beschreibung is fenster.beschreibung
    # Tasten, Schalter, Lampen H1-H5 und alle 16 DIL-Schalter stehen im Bedienfeld
    for n in ("s1", "s2", "s3", "s4", "s5", "kopf", "haube", "papier", "beleg_o", "beleg_u") + tuple(f"s{n}" for n in range(11, 27)):
        assert b.knopf(n) is not None
    for l in ("TEST", "FEHLER", "PAPIER", "BEREIT", "ONLINE"):
        assert b.lampe(l) is not None
    assert e.rom.isHidden() and not e.schnittstelle.isEnabled()     # das Modul folgt dem Typ; ein Satz je Typ = keine EPROM-Wahl
    assert not fenster.act_online.isEnabled()
    # DIL-Schalter und Drehschalter erreichen den Kern; Kern ist Quelle der Wahrheit
    b.knopf("s14").click()
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_DIL_BASIS + 3) == 1 and fenster._dil == 1 << 3
    assert fenster.konfig_lesen()["printer"]["dil"] == 1 << 3
    b.betriebsart.setCurrentIndex(b.betriebsart.findData(1))
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_BETRIEBSART) == 1
    assert fenster.konfig_lesen()["printer"]["betriebsart"] == "schneiden"
    b.knopf("kopf").click()
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_KOPF) == 0
    b.knopf("kopf").click()
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_KOPF) == 1
    # Grundstellung abwarten: DC1 erreicht die Anzeige als H5 (ONLINE)
    lauf(fenster, 3000, schritt=20000, aktualisieren_jeden=100)
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_ONLINE) == 1 and fenster.emu.printer_lamp(K.DRUCKER_LAMPE_H4) == 1
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_FEHLER) == 0


def test_k6311_typwahl_zeigt_tasten_und_lampen_und_den_hinweis_erschlossen(fenster, monkeypatch):
    e = fenster.einst_drucker
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    e.typ.setCurrentIndex(e.typ.findData("k6311"))
    fenster.takt.stop()
    assert fenster.beschreibung.schluessel == "k6311" and fenster.emu.machine == "k6311"
    b = fenster.bedienfeld
    for l in ("POW", "ON LINE", "ERR"):
        assert b.lampe(l) is not None
    assert "PE" not in b._lampen            # das ROM schreibt keine PE-Lampe: nicht angezeigt statt geraten
    for n in ("t1", "t2", "t3", "t4", "papier"):
        assert b.knopf(n) is not None
    assert not fenster.beschreibung.dil     # keine DIL-Schalter (Baud/Format ueber BCH/BDH)
    assert "erschlossen" in b.hinweise.text() and "Punkt fuer Punkt" in b.hinweise.text()
    lauf(fenster, 3000, schritt=20000, aktualisieren_jeden=100)
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_POW) == 1
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_ONLINE) == 1 and fenster.emu.printer_lamp(K.DRUCKER_LAMPE_FEHLER) == 0
    # ROM-Wahl: die zwei K6311-Abzuege stehen im Auswahlfeld, die Vorgabe ist 12128062
    assert fenster.einst_drucker.rom.count() == 2


def test_k6313_typwahl_baut_bedienfeld_und_dil_matrix(fenster, monkeypatch):
    e = fenster.einst_drucker
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    e.typ.setCurrentIndex(e.typ.findData("k6313"))
    fenster.takt.stop()
    assert fenster.beschreibung.schluessel == "k6313" and fenster.emu.machine == "k6313"
    b = fenster.bedienfeld
    for n in ("t1", "t2", "t3", "t4", "papier") + tuple(f"d{p}_{n}" for p in range(5, 19) for n in (1, 2)):
        assert b.knopf(n) is not None
    for l in ("POW", "ON LINE", "ERR", "PE"):
        assert b.lampe(l) is not None
    assert not fenster.act_online.isEnabled()
    # Vorgabe 5-2 und 13-2 ON; ein DIL-Schalter erreicht den Kern und die Konfiguration
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_K6313_DIL_BASIS + 1) == 1
    b.knopf("d14_2").click()
    bit = (14 - 5) * 2 + 1
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_K6313_DIL_BASIS + bit) == 1
    assert fenster.konfig_lesen()["printer"]["dil"] >> bit & 1 == 1
    lauf(fenster, 3000, schritt=20000, aktualisieren_jeden=100)
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_ONLINE) == 1 and fenster.emu.printer_lamp(K.DRUCKER_LAMPE_FEHLER) == 0
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_POW) == 1


@pytest.mark.parametrize("schluessel, abzuege", [("k6313-ifss", 5), ("k6311-ifss", 1)])
def test_ifss_typen_laufen_an_und_haben_ihre_abzuege(fenster, monkeypatch, schluessel, abzuege):
    # WP8g: IFSS-Fassungen von K6313/K6311 (Kern-Typ 5/6); die Kassette steckt fest, die Schnittstelle folgt dem Typ
    e = fenster.einst_drucker
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    e.typ.setCurrentIndex(e.typ.findData(schluessel))
    fenster.takt.stop()
    assert fenster.beschreibung.schluessel == schluessel and fenster.emu.machine == schluessel
    assert fenster._schnittstelle_aktiv() == "ifss"
    assert e.rom.count() == abzuege
    lauf(fenster, 3000, schritt=20000, aktualisieren_jeden=100)
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_POW) == 1
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_ONLINE) == 1 and fenster.emu.printer_lamp(K.DRUCKER_LAMPE_FEHLER) == 0


def test_k6316_selbsttest_druckt_ueber_die_taste_s5(fenster, monkeypatch):
    e = fenster.einst_drucker
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    e.typ.setCurrentIndex(e.typ.findData("k6316-ifss"))
    fenster.takt.stop()
    fenster.bedienfeld.knopf("s5").pressed.emit()             # beim Netz-Ein halten
    fenster.act_reset.trigger()
    fenster.takt.stop()
    lauf(fenster, 3000, schritt=20000, aktualisieren_jeden=200)
    fenster.bedienfeld.knopf("s5").released.emit()
    lauf(fenster, 800, schritt=20000, aktualisieren_jeden=20)
    assert fenster.blatt.punkte_zaehlen() > 1000
    assert fenster.emu.printer_lamp(K.DRUCKER_LAMPE_TEST) == 1
    assert fenster.emu.printer_geometry() == (300, 216, 1)


def test_sd1157_formularlaenge_geht_an_den_kern_schalter(fenster):
    fenster.einst_drucker.seite.setCurrentIndex(fenster.einst_drucker.seite.findData(6))
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_FORMULAR1) == 1          # 6" = Code 1 (BCH.5..4)
    fenster.einst_drucker.seite.setCurrentIndex(fenster.einst_drucker.seite.findData(12))
    assert fenster.emu.printer_switch_get(K.DRUCKER_SW_FORMULAR1) == 3


def test_k6316_vorsteckbeleg_kasten_einlegen_entnehmen(fenster, monkeypatch):
    # SD 1157: kein Vorsteckschacht, der Kasten ist verborgen
    assert fenster.dock_beleg.isHidden()
    e = fenster.einst_drucker
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    e.typ.setCurrentIndex(e.typ.findData("k6316-ifss"))
    fenster.takt.stop()
    assert not fenster.dock_beleg.isHidden()
    k = fenster.beleg
    fenster.aktualisieren()
    assert k.einlegen.isEnabled() and not k.entnehmen.isEnabled() and "leer" in k.zustand.text()
    k.laenge.setValue(120)
    k.breite.setValue(90)
    k.einlegen.click()
    st = fenster.emu.printer_beleg_state()
    assert st["eingelegt"] == 1 and st["breite_px"] > 0
    assert not k.einlegen.isEnabled() and k.entnehmen.isEnabled()
    assert "Lichtschranke" in k.zustand.text() or "Schacht" in k.zustand.text()
    assert not k.ansicht.blatt._kopf_sichtbar           # Beleg liegt nicht unter der Druckzeile
    k.entnehmen.click()
    assert fenster.emu.printer_beleg_state()["eingelegt"] == 0
    assert k.einlegen.isEnabled() and "leer" in k.zustand.text()


# ─── EPROM-Katalog in der Oberflaeche (WP8b, Entwurf 49 §11.2) ───────────────

def test_eprom_feld_nur_bei_mehreren_eintraegen_vorgabe_markiert_und_hinweis(fenster):
    e = fenster.einst_drucker
    assert not e.rom.isHidden() and e.rom.count() == 2                  # SD 1157: repariert + original
    assert e.rom.currentData() == "sd1157-repariert" and "(Vorgabe)" in e.rom.currentText()
    assert "Güte: ok" in e.rom_text.text() and "Byte 0D34H" in e.rom_text.text()
    # K6316: ein Satz je Typ -> kein Auswahlfeld
    fenster._neu_anlegen(beschreibung=typen.typ("k6316-v24"))
    assert e.rom.isHidden() and e.rom.count() == 1
    fenster.takt.stop()
    fenster._neu_anlegen(beschreibung=typen.typ("sd1157"))
    assert not e.rom.isHidden()
    fenster.takt.stop()


def test_alte_konfiguration_rom_text_und_aliase_laufen_weiter(qapp, tmp_path):
    from app.printertool.fenster import PrinterWindow
    for wahl in ("original", {"sd1157": "original"}, {"sd1157": "sd1157-original"}):
        f = PrinterWindow(konfig_daten={"printer": {"rom": wahl}}, konfig_pfad=str(tmp_path / "c.yaml"), autostart=False)
        try:
            assert f._rom == "sd1157-original"
            assert K._lib.k1520_mem_read(f.emu._handle, 0x0D34) == 0x2B
            assert f.konfig_lesen()["printer"]["rom"] == {"sd1157": "sd1157-original"}
        finally:
            f.takt.beenden()
            f.close()
    f = PrinterWindow(konfig_daten={"printer": {"rom": "gibtsnicht"}}, konfig_pfad=str(tmp_path / "c.yaml"), autostart=False)
    try:
        assert f._rom == "sd1157-repariert"                              # Unbekanntes -> Vorgabe
    finally:
        f.takt.beenden()
        f.close()


def test_ohne_katalog_startet_der_drucker_nicht_mit_klarer_meldung(qapp, tmp_path, monkeypatch):
    """WP9: kein Rueckfall auf ein eingebettetes Abbild - der Kern meldet den fehlenden Katalog (main.py gibt die Meldung aus)."""
    from app.printertool.fenster import PrinterWindow
    monkeypatch.setenv("K1520_PRINTER_ROMS", str(tmp_path / "gibtsnicht.yaml"))
    with pytest.raises(RuntimeError, match="drucker_roms.yaml nicht gefunden"):
        PrinterWindow(konfig_daten={}, konfig_pfad=str(tmp_path / "c.yaml"), autostart=False)
