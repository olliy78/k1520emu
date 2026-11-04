"""Groessenangaben und Statuszeilentext des DiskTools (reine Funktionen, kein Qt)."""

from typing import Iterable, Optional


def groesse(n: int) -> str:
    """Bytes als 'N KB' (bis 1023 KB) bzw. 'X,Y MB' (eine Nachkommastelle, Komma)."""
    kb = int(n) // 1024
    if kb < 1024:
        return f"{kb} KB"
    mb = round(int(n) / (1024 * 1024) * 10) / 10
    return f"{mb:.1f}".replace(".", ",") + " MB"


def inhalt_text(dateien: int, volumes: Iterable, mehrseitig: bool,
                tar: Optional[dict] = None) -> str:
    """Statuszeilentext 'N Dateien - X von Y frei'.

    volumes: Objekte mit dir/total/free.  tar: Plan eines tar-Satzes (abbilder,
    strom, kapazitaet) -> 'belegt von' ueber das ganze Medium.
    """
    kopf = f"{dateien} Dateien"
    if tar is not None:
        n = max(1, int(tar["abbilder"]))
        text = f"{groesse(tar['strom'])} von {groesse(tar['kapazitaet'] * n)} belegt"
        if n > 1:
            text += f" ({n} Disketten)"
        return f"{kopf} - {text}"
    teile = []
    for v in volumes:
        wo = f"{v.dir}: " if mehrseitig else ""
        if v.total > 0:
            teile.append(f"{wo}{groesse(v.free)} von {groesse(v.total)} frei")
        else:
            teile.append(f"{wo}{groesse(v.free)} frei")
    return f"{kopf} - " + " · ".join(teile) if teile else kopf
