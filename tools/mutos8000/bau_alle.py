#!/usr/bin/env python3
"""Alle fertigen MUTOS-8000-Disketten (A5120.16) aus den eingecheckten Archiven bauen.

Quellen (disks/a5120_mutos8000/):  a5120_mutos8000_boot_udos.zip (Ladediskette),
                                   a5120_mutos8000_m8k_root.zip  (Wurzel).
Ziel (Vorgabe disks/a5120_16_MUTOS/), je Laufwerksbestueckung ein Paar Lade- + Wurzeldiskette:

  nur MF3200            boot_udos_mf3200.hfe        (FM, 2AH) + root_fm.hfe  (FM)
  nur K5601             boot_udos_k5601.hfe         (MFM, FFH) + root_mfm.hfe (MFM)
  A: MF3200, B:-D: K5601 boot_udos_mf3200_k5601.hfe (FM, FEH) + root_mfm.hfe (MFM)

Dazu drei Ladedisketten mit Patches (Entwurf 39, 11.2/11.6): boot_udos_k5601_nur_a.hfe und
boot_udos_mf3200_k5601_nur_a.hfe (UDOS meldet nur A: an; MUTOS-Disketten schon beim Einschalten in B:-D:),
boot_udos_k5601_8x512.hfe (zusaetzlich 8 x 512 B und Swap ab Block 352 fuer den DISK-Satz, nur K5601).

Dazu zwei Disketten fuer den DISK-Satz (8 x 512) aus den TD0-Originalen (bau_8x512.py): root_8x512.hfe, cmd_8x512.hfe.

Dazu die Datendisketten cmd (/mnt), usr (/usr), bin (/tmp) (Best-of) und cc1/cc2 (DISK-Satz, /usr) aus den Archiven best_<rolle>.zip, je FM
(<rolle>_fm.hfe) und MFM (<rolle>_mfm.hfe); Entwurf 39 Abschnitt 10.

Vorhandene Disketten werden nur mit --ueberschreiben ersetzt.  Wer eine Datei aendern will, packt das
Archiv aus, tauscht sie unter dateien/ und gibt den Ordner mit --wurzel-quelle/--lade-quelle an.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bau_8x512  # noqa: E402
import bau_aus_archiv  # noqa: E402
import bau_ladediskette  # noqa: E402
import m8k  # noqa: E402

# (Variante, Dateiname im Zielordner)
LADEDISKETTEN = (("mf3200", "a5120_mutos8000_boot_udos_mf3200.hfe"),
                 ("k5601", "a5120_mutos8000_boot_udos_k5601.hfe"),
                 ("mf3200_k5601", "a5120_mutos8000_boot_udos_mf3200_k5601.hfe"),
                 # mit Patches (bauangaben_ladediskette.yaml): UDOS meldet nur A: an / DISK-Satz 8 x 512
                 ("k5601_nur_a", "a5120_mutos8000_boot_udos_k5601_nur_a.hfe"),
                 ("mf3200_k5601_nur_a", "a5120_mutos8000_boot_udos_mf3200_k5601_nur_a.hfe"),
                 ("k5601_8x512", "a5120_mutos8000_boot_udos_k5601_8x512.hfe"))
WURZELN = (("mf3200", "a5120_mutos8000_root_fm.hfe"),
           ("k5601", "a5120_mutos8000_root_mfm.hfe"))


# DISK-Satz 8 x 512 (bau_8x512.py): Wurzel mit der lauffaehigen pwd und die Befehlsdiskette, aus den TD0-Originalen
DISK_SATZ = tuple("a5120_mutos8000_" + n for n in bau_8x512.ZIELE.values())

# (Rolle, Archivname ohne Pfad); je Rolle zwei Disketten: FM und MFM
DATENDISKETTEN = ("cmd", "usr", "bin", "cc1", "cc2", "nroff")


def datendisketten():
    """Dateiname -> (Rolle, Laufwerke) aller Datendisketten."""
    r = {}
    for rolle in DATENDISKETTEN:
        r["a5120_mutos8000_%s_fm.hfe" % rolle] = (rolle, "mf3200")
        r["a5120_mutos8000_%s_mfm.hfe" % rolle] = (rolle, "k5601")
    return r


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ziel-ordner", default=m8k.FERTIG, help="Vorgabe: disks/a5120_16_MUTOS/")
    ap.add_argument("--lade-quelle", default=m8k.disk("boot_udos.zip"), help="Archiv oder Ordner der Ladediskette")
    ap.add_argument("--wurzel-quelle", default=m8k.disk("m8k_root.zip"), help="Archiv oder Ordner der Wurzel")
    ap.add_argument("--daten-quelle", default=None,
                    help="Ordner mit best_<rolle>.zip (Vorgabe disks/a5120_mutos8000/) oder ausgepackte Ordner best_<rolle>/")
    ap.add_argument("--nur-daten", action="store_true", help="nur die zwoelf Datendisketten (cmd, usr, bin, cc1, cc2, nroff; FM und MFM)")
    ap.add_argument("--ueberschreiben", action="store_true", help="vorhandene Disketten ersetzen")
    a = ap.parse_args(argv)

    ordner = os.path.abspath(a.ziel_ordner)
    ziele = [os.path.join(ordner, n) for _, n in ([] if a.nur_daten else LADEDISKETTEN + WURZELN)] + \
            [os.path.join(ordner, n) for n in datendisketten()] + \
            [os.path.join(ordner, n) for n in ([] if a.nur_daten else DISK_SATZ)]
    vorhanden = [z for z in ziele if os.path.exists(z)]
    if vorhanden and not a.ueberschreiben:      # vorab, damit nicht die Haelfte gebaut wird
        raise SystemExit("vorhanden (--ueberschreiben): " + ", ".join(os.path.basename(z) for z in vorhanden))
    for variante, name in ([] if a.nur_daten else LADEDISKETTEN):
        bau_ladediskette.bauen(a.lade_quelle, os.path.join(ordner, name), variante, ueberschreiben=True)
    for laufwerke, name in ([] if a.nur_daten else WURZELN):
        bau_aus_archiv.bauen(a.wurzel_quelle, os.path.join(ordner, name), ueberschreiben=True,
                             laufwerke=laufwerke)
    for name, (rolle, laufwerke) in datendisketten().items():
        quelle = a.daten_quelle or m8k.DISKS
        pfad = os.path.join(quelle, "a5120_mutos8000_best_%s.zip" % rolle)
        if not os.path.exists(pfad):
            pfad = os.path.join(quelle, "best_" + rolle)
        bau_aus_archiv.bauen(pfad, os.path.join(ordner, name), ueberschreiben=True, laufwerke=laufwerke)
    if not a.nur_daten:
        bau_8x512.bauen(ordner, ueberschreiben=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
