#!/usr/bin/env python3
"""Alle fertigen MUTOS-8000-Disketten (A5120.16) aus den eingecheckten Archiven bauen.

Quellen (disks/a5120_mutos8000/):  a5120_mutos8000_boot_udos.zip (Ladediskette),
                                   a5120_mutos8000_m8k_root.zip  (Wurzel).
Ziel (Vorgabe disks/a5120_16_MUTOS/), je Laufwerksbestueckung ein Paar Lade- + Wurzeldiskette:

  nur MF3200            boot_udos_mf3200.hfe        (FM, 2AH) + root.hfe     (FM)
  nur K5601             boot_udos_k5601.hfe         (MFM, 3FH) + root_mfm.hfe (MFM)
  A: MF3200, B:/C: K5601 boot_udos_mf3200_k5601.hfe (FM, 3EH) + root_mfm.hfe (MFM)

Vorhandene Disketten werden nur mit --ueberschreiben ersetzt.  Wer eine Datei aendern will, packt das
Archiv aus, tauscht sie unter dateien/ und gibt den Ordner mit --wurzel-quelle/--lade-quelle an.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bau_aus_archiv  # noqa: E402
import bau_ladediskette  # noqa: E402
import m8k  # noqa: E402

# (Variante, Dateiname im Zielordner)
LADEDISKETTEN = (("mf3200", "a5120_mutos8000_boot_udos_mf3200.hfe"),
                 ("k5601", "a5120_mutos8000_boot_udos_k5601.hfe"),
                 ("mf3200_k5601", "a5120_mutos8000_boot_udos_mf3200_k5601.hfe"))
WURZELN = (("mf3200", "a5120_mutos8000_root.hfe"),
           ("k5601", "a5120_mutos8000_root_mfm.hfe"))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ziel-ordner", default=m8k.FERTIG, help="Vorgabe: disks/a5120_16_MUTOS/")
    ap.add_argument("--lade-quelle", default=m8k.disk("boot_udos.zip"), help="Archiv oder Ordner der Ladediskette")
    ap.add_argument("--wurzel-quelle", default=m8k.disk("m8k_root.zip"), help="Archiv oder Ordner der Wurzel")
    ap.add_argument("--ueberschreiben", action="store_true", help="vorhandene Disketten ersetzen")
    a = ap.parse_args(argv)

    ordner = os.path.abspath(a.ziel_ordner)
    ziele = [os.path.join(ordner, n) for _, n in LADEDISKETTEN + WURZELN]
    vorhanden = [z for z in ziele if os.path.exists(z)]
    if vorhanden and not a.ueberschreiben:      # vorab, damit nicht die Haelfte gebaut wird
        raise SystemExit("vorhanden (--ueberschreiben): " + ", ".join(os.path.basename(z) for z in vorhanden))
    for variante, name in LADEDISKETTEN:
        bau_ladediskette.bauen(a.lade_quelle, os.path.join(ordner, name), variante, ueberschreiben=True)
    for laufwerke, name in WURZELN:
        bau_aus_archiv.bauen(a.wurzel_quelle, os.path.join(ordner, name), ueberschreiben=True,
                             laufwerke=laufwerke)
    return 0


if __name__ == "__main__":
    sys.exit(main())
