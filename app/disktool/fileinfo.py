"""Die Angabendatei (`.fileinfo`) schreiben — ohne Qt, ohne Kern.

Eine Linux-Datei trägt nur Bytes.  Was daneben in einem `<datei>.fileinfo` steht,
sind die Angaben, die das Dateisystem der Diskette führt und das Wirtsystem nicht:
bei UDOS Typ, Eigenschaften, Satzlänge, ENTRY, Segmente und die Speicheranforderung
LOW/HIGH/STACK, bei CP/M Nutzerbereich und Attribute, bei tar und WEGA (``fs=tar``/``fs=wega``)
Dateiart, Modus, Besitzer und Zeit (`doc/bug_disktool_Programmdatei.md` §2.1,
Entwurf 37 Festlegung 8).

**Gelesen** wird diese Form ausschliesslich im Kern (`leseFileinfo` in
`core/filesystem/disk_volume.cpp`) — es gibt genau einen Leser.  Geschrieben wird
sie an drei Stellen: dort beim Extrahieren, hier vom Eingabedialog der Oberfläche
und von der Kommandozeile der physischen Diskette.  Deshalb steht das Schreiben
hier und nicht im Dialog: `app/disktool/physical_cli.py` läuft **ohne Qt**.
"""

from __future__ import annotations

import time
from pathlib import Path

#: Reihenfolge der UNIX-Schluessel, wie sie der Kern schreibt (`schreibeUnixFelder` in
#: `core/filesystem/fileinfo.cpp`); `mode` zaehlt davon nur mit den unteren 12 Bit.
UNIX_SCHLUESSEL = ("art", "mode", "uid", "gid", "nlink", "inode", "mtime", "atime", "ctime",
                   "rdev", "teilstueck")

#: Familien, deren `.fileinfo` Modus/Besitzer/Zeit traegt.
UNIX_FAMILIEN = ("tar", "wega")


def unix_vorgaben(ordner: bool = False, jetzt=None) -> dict:
    """Die Angaben, die der Kern beim Einfuegen OHNE `.fileinfo` selbst nimmt:
    Modus 0644 (Ordner 0755), Besitzer 0/0, Zeit jetzt.  Schluessel und Zahlenform wie im
    Kern (`mode=` als achtstellige Oktalzahl mit fuehrender 0 samt Dateiart-Bits)."""
    t = int(time.time() if jetzt is None else jetzt)
    if ordner:
        art, mode = "d", 0o040755
    else:
        art, mode = "-", 0o100644
    return {"art": art, "mode": f"0{mode:06o}", "uid": 0, "gid": 0, "mtime": t}


_KOPF = {
    "tar": ("# Modus, Besitzer und Zeit stammen aus dem tar-Kopf.  Beim Einfuegen auf eine",
            "# WEGA-Diskette oder in ein tar-Archiv werden mode (untere 12 Bit), uid, gid und",
            "# mtime uebernommen.  teilstueck=ist/soll: die Datei war abgeschnitten."),
    "wega": ("# Ohne sie bekommt die Datei beim Einfuegen die Vorgaben (Modus 0644 bzw.",
             "# 0755, Besitzer 0/0, Zeit jetzt) — Inhalt und Name gehen nicht verloren.",
             "# Zurueckgespielt werden mode (untere 12 Bit), uid, gid und mtime; art,",
             "# nlink, inode, atime, ctime und rdev stehen nur zur Auskunft da."),
}


def schreibe_fileinfo(ziel, angaben: dict, name: str) -> Path:
    """Eine Angabendatei schreiben — eine Zeile ``schlüssel=wert`` je Angabe.

    Args:
        ziel: Pfad der zu schreibenden Datei (üblich: ``<datei>.fileinfo``).
        angaben: Schlüssel/Wert, wie sie der Kern liest — ``typ``, ``eig``,
            ``start``, ``satz``, ``block``, ``segment``, ``segs``, ``mem``,
            ``zusatz``, ``erst``, ``geaend`` bzw. bei CP/M ``attr``.  Leere Werte
            werden weggelassen; unbekannte Schlüssel überliest der Leser.
        name: der **echte** Name auf der Diskette — bei CP/M mit Nutzerbereich
            (``3:SYSTEM.COM``), für den im Linux-Dateinamen ein Unterstrich steht.
    """
    p = Path(ziel)
    zeilen = [
        f"# k1520DiskTool — Angaben zu '{name}'.",
        "# Von Hand eingegeben (die Datei brachte keine mit).  Beim Einfuegen",
        "# (`put`) werden sie wieder uebernommen.",
        *_KOPF.get(str(angaben.get("fs", "")), ()),
        # `fs=` in der ersten Sachzeile: ein .fileinfo einer CP/M-Datei darf nicht
        # stillschweigend als UDOS-Angabe gelesen werden.
        f"fs={angaben.get('fs', 'udos')}",
        f"name={name}",
    ]
    # UNIX-Familien: die Schluessel in der Reihenfolge des Kerns, dann der Rest.
    reihe = list(angaben)
    if str(angaben.get("fs", "")) in UNIX_FAMILIEN:
        reihe = ([k for k in UNIX_SCHLUESSEL if k in angaben]
                 + [k for k in angaben if k not in UNIX_SCHLUESSEL])
    for k in reihe:
        v = angaben[k]
        if k in ("fs", "name") or v in ("", None):
            continue
        zeilen.append(f"{k}={v}")
    p.write_text("\n".join(zeilen) + "\n", encoding="utf-8")
    return p
