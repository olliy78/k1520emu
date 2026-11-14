#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
karten.py  -  Gast-Software der Hardwareoptionen (GIDE, RTC, USB, Netz) je Rechner/Betriebssystem
================================================================================================

Was auf welche Systemdiskette gehoert, steht HIER (eine Wahrheit); die Bauskripte der Disketten
(tools/cpa_a5120/build.py usw.) rufen :func:`dateien` und legen das Ergebnis in ihr ``soll()``.
Die Software passt zur VORAUSWAHL der Oberflaeche (``app/profil.py``, Handbuch „Zusatzkarten"):

  GIDE  Basis 90H auf allen Rechnern
  USB   84H (A5120 alle Modelle, K8915 V3, PC 1715/1715W) bzw. 8CH (K8915 Gen 2, PRG 710/710-1)
  Netz  C0H (A5120, K8915, PC 1715/1715W) bzw. 28H (PRG 710/710-1)

Woher die Dateien kommen und was sie duerfen: tools/gide/README.md.

Namen auf der Diskette: GIDEHP90.COM + ERADIR.COM (Poppe, CP/A) oder GIDN90.COM (Kingstener, SCPX/SCP),
UHR.COM (eigen), USB.COM/UDIR.COM/UGET.COM/UPUT.COM (UTools), CPMNET.COM + TFTP.COM (KCNet),
dazu GIDE.TXT, USB.TXT, NETZ.TXT.  Wo zwei USB-Basen gelten (K8915), tragen die Programme der
zweiten Basis ein Suffix: USB8C.COM, UDIR8C.COM, UGET8C.COM, UPUT8C.COM.
"""
import os
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)
import basis_anpassen as ba  # noqa: E402

FREMD = os.path.join(HIER, 'fremd')
UHR_COM = os.path.join(HIER, 'uhr', 'uhr.com')


class Rechner:
    def __init__(self, titel, gide, usb, net, gide_basis=0x90, pip=True):
        self.titel = titel
        self.gide = gide                # 'poppe' | 'gidn' | None (kein Treiber; die Uhr der Karte gibt es trotzdem)
        self.usb = usb                  # Liste der Basen; die erste traegt die Namen USB.COM ..., weitere Suffix
        self.net = net                  # None = nicht auf der Diskette (Platz)
        self.gide_basis = gide_basis
        self.pip = pip                  # False: SCP 1715 hat kein brauchbares PIP (nur Hinweis im Text)


#: Schluessel = Systemdiskette (Bauskript).
RECHNER = {
    'a5120_cpa':    Rechner('A5120 mit CP/A', 'poppe', [0x84], 0xC0),
    'a5120_scpx':   Rechner('A5120 mit SCPX 1.7', 'gidn', [0x84], 0xC0),
    'k8915_scpx':   Rechner('K8915 mit SCPX 8915', 'gidn', [0x84, 0x8C], 0xC0),
    'prg710_scpx':  Rechner('PRG 710 mit SCPX', 'poppe', [0x8C], 0x28),
    'prg710_1_scpx': Rechner('PRG 710-1 mit SCPX 1.7', 'gidn', [0x8C], 0x28),
    'pc1715_cpa':   Rechner('PC 1715 mit CP/A 1715', 'poppe', [0x84], 0xC0),
    'pc1715_scp6':  Rechner('PC 1715 mit SCP 1715 V0006', 'gidn', [0x84], 0xC0, pip=False),
    'pc1715_scp7':  Rechner('PC 1715 mit SCP 1715 V0007', 'gidn', [0x84], 0xC0, pip=False),
    # PC 1715W mit SCP 3.0: kein IDE-Treiber bekannt, die Systemdiskette hat nur 38 KB frei -> Uhr und USB (Netz: zu gross)
    'pc1715w_scp3': Rechner('PC 1715W mit SCP 3.0', None, [0x84], None),
}


def _lies(*teile):
    with open(os.path.join(*teile), 'rb') as f:
        return f.read()


def cpm_text(text):
    """LF -> CRLF, mit ^Z abschliessen, auf volle 128-B-Saetze auffuellen (so liefert das DiskTool CP/M-Text)."""
    t = text.replace('\r\n', '\n').replace('\n', '\r\n').encode('ascii') + b'\x1a'
    return t + b'\x1a' * (-len(t) % 128)


def _hex(b):
    return f'{b:02X}H'


def _gide_text(r):
    b = _hex(r.gide_basis)
    kopf = (f'GIDE-Karte (IDE-Platte) und Uhr - {r.titel}\n'
            f'{"=" * 60}\n\n'
            f'Diese Diskette passt zur Vorauswahl des Emulators: GIDE auf Basis {b}\n'
            f'(Einstellungen - Allgemein - GIDE).  Eine andere Basis braucht einen\n'
            f'anderen Treiber, siehe unten.\n\n')
    if r.gide is None:
        platte = ('Fuer dieses Betriebssystem ist kein IDE-Treiber bekannt (die Treiber von Poppe und\n'
                  'Kingstener laufen unter SCP 3.0 nicht); die Platte der GIDE-Karte bleibt ungenutzt.\n')
    elif r.gide == 'poppe':
        platte = (
            f'Treiber: GIDEHP{r.gide_basis:02X}.COM (Heiko Poppe, V0.2) fuer CP/A.\n\n'
            f'  1. Im Emulator die Platte anlegen: Reiter Laufwerke - Neue Platte\n'
            f'     (mindestens 256 MB; der Treiber rechnet mit 500 Zylindern x 16 Koepfen\n'
            f'     x 63 Sektoren = 258 MB, das Abbild ist anfangs fast leer).\n'
            f'  2. GIDEHP{r.gide_basis:02X} starten.  Er meldet "GIDE-Treiber installiert" und unter\n'
            f'     "Neu installierte GIDE-Laufwerke" bis zu acht neue Buchstaben - die nach den\n'
            f'     Laufwerken des BIOS (am A5120 mit CP/A D: bis K:).  Im Folgenden steht D: fuer\n'
            f'     den ersten davon.\n'
            f'  3. Beim ersten Mal das Verzeichnis jedes benutzten neuen Laufwerks anlegen:\n'
            f'        ERADIR D:      (Frage mit Y und Return beantworten)\n'
            f'     Danach DIR D: und DIR A: aufrufen (BDOS stellt das Laufwerk neu ein).\n'
            f'  4. Kopieren wie gewohnt:  PIP D:=A:DATEI.EXT\n\n'
            f'Der Treiber ist nach einem Neustart weg und muss neu gestartet werden.\n'
            f'ACHTUNG: Platten, die mit diesem Treiber angelegt wurden, sind mit dem\n'
            f'Kingstener-Treiber (GIDN) NICHT lesbar und umgekehrt - der falsche\n'
            f'Treiber zerstoert die Platte.\n\n'
            f'Andere Basis: im Ordner karten_software/fremd/poppe der Installation liegen\n'
            f'GIDEHP00 bis GIDEHPF0 fuer jede Basis; die passende auf die Diskette\n'
            f'kopieren (DiskTool).\n')
    else:
        platte = (
            f'Treiber: GIDN{r.gide_basis:02X}.COM (Kingstener, nachladbarer Treiber V0.4,\n'
            f'Laufwerk N:) fuer SCPX/SCP.\n\n'
            f'  1. Im Emulator die Platte anlegen: Reiter Laufwerke - Neue Platte\n'
            f'     (der Treiber nutzt 256 Zylinder x 4 Koepfe x 64 Sektoren = 32 MB;\n'
            f'     ein groesseres Abbild ist erlaubt, der Rest bleibt ungenutzt).\n'
            f'  2. Beim ERSTEN Mal formatieren:   GIDN{r.gide_basis:02X} /F   (Frage mit J beantworten)\n'
            f'     Danach nur noch   GIDN{r.gide_basis:02X}   (installiert das Laufwerk N:).\n'
            + (f'  3. Kopieren wie gewohnt:  PIP N:=A:DATEI.EXT\n\n' if r.pip else
               f'  3. Diese SCP-1715-Diskette hat kein brauchbares PIP (die Fassung verlangt SCP 3).\n'
               f'     Zum Beschreiben von N: dient z. B. SAVE 1 N:TEST.BIN, gelesen wird mit DIR N:\n'
               f'     und TYPE N:DATEI.TXT.\n\n') +
            f'Der Treiber ist nach einem Neustart weg und muss neu gestartet werden.\n'
            f'ACHTUNG: Platten dieses Treibers sind mit dem Poppe-Treiber (CP/A) NICHT\n'
            f'lesbar und umgekehrt.\n\n'
            f'Andere Basis: python3 karten_software/basis_anpassen.py gidn <BB> --out <Ordner>\n'
            f'schreibt GIDN<BB>.COM (BB = Hexzahl, Vielfaches von 10).\n')
    uhr = (f'\nUhr (RTC 72421 auf der GIDE-Karte)\n'
           f'----------------------------------\n'
           f'  UHR                           zeigt TT.MM.JJJJ HH:MM:SS\n'
           f'  UHR S 24.12.26 18:30:00       stellt die Uhr (Jahr auch vierstellig)\n'
           f'  UHR BB ...                    andere Kartenbasis, BB in Hex (Vorgabe 90)\n'
           f'Die Uhr laeuft in Maschinenzeit und wird vom Emulator mit der PC-Uhr (UTC)\n'
           f'versorgt (Einstellungen - Allgemein - GIDE).\n')
    return kopf + platte + uhr


def _usb_text(r):
    erste = r.usb[0]
    s = (f'USB-Stick (GUN/VDIP1) - {r.titel}\n{"=" * 60}\n\n'
         f'Programme der UTools 1.4 (M. Leubner), umgeschrieben auf die Vorauswahl-Basis\n'
         f'{_hex(erste)} des Emulators (Einstellungen - Allgemein - USB-Stick):\n\n'
         f'  USB FWV              Firmware-Version der Karte (Pruefung der Verbindung)\n'
         f'  UDIR                 Verzeichnis des Sticks\n'
         f'  UGET DATEI.EXT       Datei vom Stick auf die Diskette holen\n'
         f'  UPUT DATEI.EXT       Datei von der Diskette auf den Stick schreiben\n'
         f'  USB CD name / MKD name / DLF name / FS   Verzeichnis, Anlegen, Loeschen, Platz\n\n'
         f'Der "Stick" ist ein Ordner auf dem Rechner (im Emulator waehlen); Namen nur 8.3.\n')
    if len(r.usb) > 1:
        s += (f'\nDieser Rechner hat je nach Modell zwei Vorauswahlen: {_hex(r.usb[0])} (K8915 V3) und '
              f'{_hex(r.usb[1])} (K8915 Gen 2).\n'
              f'Die Programme mit dem Suffix {r.usb[1]:02X} (USB{r.usb[1]:02X}.COM, UDIR{r.usb[1]:02X}.COM, '
              f'UGET{r.usb[1]:02X}.COM, UPUT{r.usb[1]:02X}.COM) gehoeren zur Basis {_hex(r.usb[1])}.\n')
    s += (f'\nAndere Basis: python3 karten_software/basis_anpassen.py usb <BB> --out <Ordner>\n'
          f'(BB = Hexzahl, Vielfaches von 4); die vier Programme dann auf die Diskette kopieren.\n')
    return s


def _netz_text(r):
    b = _hex(r.net)
    return (f'Netzkarte (KCNet, W5100) - {r.titel}\n{"=" * 60}\n\n'
            f'Programme: CPMNET.COM (CPMNET 1.5, TCP-Server/Client, Menues) und TFTP.COM (TFTP 1.2)\n'
            f'von Ralf Kaestner.  Sie sind auf die Vorauswahl-Basis {b} des Emulators\n'
            f'eingestellt (Einstellungen - Allgemein - Netzwerk).\n\n'
            f'Erstes Einrichten:\n'
            f'  1. CPMNET starten, Menue T (TCP/IP-Stack): 1 = eigene IP-Adresse, 2 = Netzmaske,\n'
            f'     3 = Gateway von Hand eintragen, dann C (konfigurieren).  Das W5100-Modell des\n'
            f'     Emulators kennt kein DHCP und keinen Broadcast; die Menuepunkte H, D und P\n'
            f'     (DHCP, DNS, Ping) bleiben ohne Wirkung.  Die im Gast eingestellte Adresse\n'
            f'     ignoriert der Emulator - er benutzt die Netzverbindung des Rechners.\n'
            f'  2. Menue I zeigt Firmware-/Hardwareversion und "online".\n'
            f'  3. TFTP GET <IP> DATEI.EXT holt eine Datei von einem TFTP-Server; die Zieladresse\n'
            f'     ist die des Gegenstelle im Netz des Rechners.\n\n'
            f'Das Netzkabel der Karte schaltet die Oberflaeche (Haken "Kabel"); ohne Kabel melden die\n'
            f'Programme "network-cable not connected".  Verbindungen nach aussen gehen ueber das\n'
            f'Netz des Wirtsrechners - Sperren dort (Firewall) gelten auch fuer den Emulator.\n\n'
            f'Andere Basis: python3 karten_software/basis_anpassen.py net <BB> --out <Ordner>\n'
            f'(BB = Hexzahl, Vielfaches von 4).\n')


def dateien(schluessel):
    """{Diskettenname: Bytes} aller Hardwareoptions-Beigaben fuer @p schluessel (siehe RECHNER)."""
    r = RECHNER[schluessel]
    d = {}
    if r.gide == 'poppe':
        d[f'GIDEHP{r.gide_basis:02X}.COM'] = _lies(FREMD, 'poppe', f'gidehp{r.gide_basis:02x}.com')
        d['ERADIR.COM'] = _lies(FREMD, 'poppe', 'eradir.com')
    elif r.gide == 'gidn':
        d[f'GIDN{r.gide_basis:02X}.COM'] = ba.gidn_datei(r.gide_basis)
    d['UHR.COM'] = _lies(UHR_COM)
    d['GIDE.TXT'] = cpm_text(_gide_text(r))
    for i, basis in enumerate(r.usb):
        suffix = '' if i == 0 else f'{basis:02X}'
        for name, daten in ba.usb_satz(basis).items():
            stamm, endung = name.split('.')
            d[f'{stamm}{suffix}.{endung}'] = daten
    d['USB.TXT'] = cpm_text(_usb_text(r))
    if r.net is not None:
        n = ba.net_satz(r.net)
        d['CPMNET.COM'] = n['CPMN1587.COM']
        d['TFTP.COM'] = n['TFTP1287.COM']
        d['NETZ.TXT'] = cpm_text(_netz_text(r))
    return d


# ---------------------------------------------------------------------------------------------------------
# Waechter: tragen die ausgelieferten Disketten genau diese Dateien?   (ctest: cli_karten_beigaben)
# ---------------------------------------------------------------------------------------------------------
#: Schluessel -> Name der Systemdiskette in disks/
DISKETTEN = {
    'a5120_cpa': 'a5120_cpa_k5601_system.hfe',
    'a5120_scpx': 'a5120_scpx17_k5601_system.hfe',
    'k8915_scpx': 'k8915_scpx8915_v24_system.hfe',
    'prg710_scpx': 'prg710_scpx15_system.hfe',
    'prg710_1_scpx': 'prg710-1_scpx17_system.hfe',
    'pc1715_cpa': 'pc1715_cpa1715_system.hfe',
    'pc1715_scp6': 'pc1715_scp1715_v0006_system.hfe',
    'pc1715_scp7': 'pc1715_scp1715_v0007_system.hfe',
    'pc1715w_scp3': 'pc1715w_scp30_system.hfe',
}


def pruefen(tool, ordner):
    """Jede Diskette traegt alle Dateien von :func:`dateien` bytegleich; Rueckgabe = Zahl der Abweichungen."""
    import subprocess
    import tempfile
    sys.path.insert(0, os.path.dirname(HIER))
    import disketten_beigaben as db
    fehler = 0
    for schluessel, name in DISKETTEN.items():
        abbild = db.diskpfad(ordner, name)
        if not os.path.isfile(abbild):
            print(f'FEHLT   {name}')
            fehler += 1
            continue
        vorher = fehler
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run([*tool, 'get', abbild, '*', '--to', tmp], capture_output=True, text=True)
            for n, soll in dateien(schluessel).items():
                pfad = os.path.join(tmp, n)
                if not os.path.isfile(pfad):
                    print(f'OHNE    {name}: {n}')
                    fehler += 1
                elif _lies(pfad) != soll:
                    print(f'ALT     {name}: {n}')
                    fehler += 1
        if fehler == vorher:
            print(f'aktuell {name}')
    return fehler


def main():
    import argparse
    ap = argparse.ArgumentParser(description='Beigaben der Hardwareoptionen auf den Systemdisketten pruefen')
    ap.add_argument('--tool', required=True, help='k1520disktool')
    ap.add_argument('--emulator', action='append', default=[])
    ap.add_argument('--ordner', default=os.path.join(os.path.dirname(os.path.dirname(HIER)), 'disks'))
    ap.add_argument('--check', action='store_true', required=True)
    a = ap.parse_args()
    fehler = pruefen([*a.emulator, a.tool], a.ordner)
    if fehler:
        print('Nachziehen: die Bauskripte der Systemdisketten (tools/cpa_a5120/build.py usw.) mit --tool aufrufen.')
    return 1 if fehler else 0


if __name__ == '__main__':
    sys.exit(main())
