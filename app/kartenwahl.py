"""
K1520 Emulator - Basiswahl der Steckkarten (GIDE, USB, Netz)
============================================================

Eine Stelle fuer das, was jede Steckkarte der Oberflaeche braucht: die **Kandidatenliste** der
E/A-Basis (das Auswahlfeld bildet die moeglichen Einstellungen der Hardware ab), die **Besitzerabfrage**
(wer belegt die Tore schon?) und die **Vorauswahl** je Maschine.  GIDE (G6) benutzt sie heute, USB (U3)
und Netz (N3) setzen sich mit ihrer :class:`KartenArt` darauf -- siehe ``eintraege`` und
:meth:`app.profil.Programmprofil.karten_vorauswahl`.  Das Modul importiert kein Qt.

**Die Wahrheit liefert der Kern** (``k1520_io_owner`` / ``K1520Emulator.io_owner``): welche Tore die
Maschine selbst und die schon gesteckten Karten belegen, steht nur im Bus; hier gibt es dazu keine
Tabelle.  Gerechnet wird in Python nur zwischen den Karten, die im Dialog GERADE gewaehlt, aber noch nicht
gesteckt sind (``andere``): deren Tore ueberlappen sich gegenseitig.

Die Karte selbst zaehlt nicht als Besitzer ihrer eigenen Tore (die laufende Maschine hat sie ja gesteckt):
``KartenArt.eigener_name`` ist ihr ``deviceName()`` im Kern.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional


def basis_text(basis: int) -> str:
    """``0xD0`` -> ``"D0H"``."""
    return f"{int(basis) & 0xFF:02X}H"


def basis_lesen(art: "KartenArt", wert, vorgabe: int) -> int:
    """Basis aus der Konfiguration (``"84"``/``"84H"``/``"0x84"``, auch die Zahl 84 = Hexziffern 84).

    Ungueltig (nicht lesbar, kein Vielfaches von ``art.schritt``) -> *vorgabe*.
    """
    try:
        if isinstance(wert, bool):
            raise ValueError
        n = int(str(wert).strip().lower().removeprefix("0x").removesuffix("h"), 16)
    except (ValueError, TypeError):
        return vorgabe
    return n if n in art.basen() else vorgabe


@dataclass(frozen=True)
class KartenArt:
    """Eine Steckkarte mit E/A-Basis."""
    schluessel: str                 # "gide" | "usb" | "net" (Konfig- und Profilschluessel)
    titel: str                      # Anzeige in Fehlertexten
    schritt: int                    # Abstand der Basen (GIDE 10H, USB/Netz 4)
    eigener_name: str               # deviceName() der Karte im Kern
    tore_ab: int                    # erstes belegtes Tor = Basis + tore_ab
    tore_zahl: int                  # Zahl der belegten Tore
    #: Hardware-Einstellungen, die einen Namen tragen: ``{Basis: Name}`` (im Auswahlfeld hervorgehoben).
    hervorgehoben: Dict[int, str] = field(default_factory=dict)

    def basen(self) -> List[int]:
        """Alle waehlbaren Basen (Vielfache von :attr:`schritt` bis FFH)."""
        return list(range(0x00, 0x100, self.schritt))

    def tore(self, basis: int) -> List[int]:
        """Die E/A-Tore, die die Karte bei *basis* belegt (innerhalb 00H..FFH)."""
        return [p for p in range(basis + self.tore_ab, basis + self.tore_ab + self.tore_zahl) if p <= 0xFF]


#: IDE-Karte GIDE: Basis = Hochnibble-Jumper, belegt Basis+4 .. Basis+0FH.
GIDE = KartenArt("gide", "GIDE", 0x10, "GIDE", 4, 12)
#: GUN-USB: vier Tore; GUN fest 84H, sonst frei (Vielfache von 4).
USB = KartenArt("usb", "USB", 4, "GUN-USB", 0, 4, {0x84: "GUN"})
#: KCNet-Netzkarte: vier Tore; K1520-Netzkarte per Jumper C0H oder 28H, GUN fest 88H.
NETZ = KartenArt("net", "Netz", 4, "KCNet", 0, 4,
                 {0xC0: "K1520-Netzkarte", 0x28: "K1520-Netzkarte", 0x88: "GUN"})

ARTEN = {a.schluessel: a for a in (GIDE, USB, NETZ)}

#: Der Besitzer-Abfrager: Port -> Name des Geraets am Bus (``""`` = frei), z. B. ``emulator.io_owner``.
Besitzer = Callable[[int], str]


def besitzer_der_tore(art: KartenArt, basis: int, owner: Optional[Besitzer]) -> str:
    """Wer von AUSSEN die Tore von *art* bei *basis* belegt, als ``"Name"`` (``""`` = alle frei).

    Die eigenen Tore der Karte (laufende Maschine) zaehlen nicht.
    """
    if owner is None:
        return ""
    for p in art.tore(basis):
        name = owner(p)
        if name and name != art.eigener_name:
            return name
    return ""


def konflikt_mit_anderen(art: KartenArt, basis: int, andere: Optional[Dict[str, int]]) -> str:
    """Titel der ANDEREN gewaehlten Karte, deren Tore sich mit denen von *art* ueberlappen (``""`` = keine).

    *andere*: ``{Kartenschluessel: Basis}`` der im Dialog gewaehlten (eingeschalteten) Karten, ohne *art* selbst.
    """
    mine = set(art.tore(basis))
    for schl, b in (andere or {}).items():
        if schl == art.schluessel or schl not in ARTEN:
            continue
        if mine & set(ARTEN[schl].tore(b)):
            return ARTEN[schl].titel
    return ""


def grund(art: KartenArt, basis: int, owner: Optional[Besitzer], andere: Optional[Dict[str, int]] = None) -> str:
    """Warum *basis* nicht frei ist (``""`` = frei): Name des Besitzers oder der anderen Karte."""
    return besitzer_der_tore(art, basis, owner) or konflikt_mit_anderen(art, basis, andere)


@dataclass(frozen=True)
class Eintrag:
    basis: int
    text: str
    belegt: str                     # "" = waehlbar


def eintraege(art: KartenArt, owner: Optional[Besitzer], andere: Optional[Dict[str, int]] = None) -> List[Eintrag]:
    """Die Zeilen des Auswahlfelds: ``D0H``, ``88H (GUN)``, ``D0H (belegt: K5020 Daten-PIO)``."""
    zeilen = []
    for b in art.basen():
        text = basis_text(b)
        if b in art.hervorgehoben:
            text += f" ({art.hervorgehoben[b]})"
        g = grund(art, b, owner, andere)
        if g:
            text += f" (belegt: {g})"
        zeilen.append(Eintrag(b, text, g))
    return zeilen
