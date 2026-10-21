"""Warum sich an dieser Diskette nichts schreiben lässt — an EINER Stelle.

Eine tar-Diskette (ein Archivstrom, kein Dateisystem) ist nicht nur „schreibgeschützt
geöffnet", sondern **nicht beschreibbar**.  Der Haken „Schreibschutz" lässt sich dort
nicht abwählen; die Oberfläche soll deshalb nicht raten „Schreibschutz aufheben",
sondern den wahren Grund nennen.  (Ein CopyQM-Abbild, ``.cqm``, gehört seit dem
Schreiben im Kern NICHT mehr hierher — es ist wie ein ``.img`` beschreibbar.)
"""

from __future__ import annotations


def sperrgrund(tool) -> str:
    """Begründung, warum die Diskette unaufhebbar gesperrt ist ("" = beschreibbar
    oder nur schreibgeschützt geöffnet)."""
    if tool is None or not tool.read_only_forced:
        return ""
    teile = []
    if tool.ist_tar:
        teile.append("tar-Disketten sind nur lesbar: sie tragen einen Archivstrom und "
                     "kein Dateisystem.")
    if teile:
        return " ".join(teile)
    return "Der Schreibschutz dieser Diskette lässt sich nicht aufheben."


def schutz_text(tool) -> str:
    """Kurztext für die Statuszeile ("" = nichts Besonderes)."""
    if tool is None or not tool.read_only_forced:
        return ""
    if tool.ist_tar:
        return "R/O  nur lesbar (tar-Archiv)"
    return "R/O  nicht beschreibbar"


def ist_platte(tool) -> bool:
    """Ist das geoeffnete Ding ein P8000-Plattenabbild (``PlatteTool``)?"""
    return bool(getattr(tool, "ist_plattenabbild", False))


#: Aktionen, die an einer Platte keinen Sinn haben, samt Begruendung (Entwurf 37, Festlegung
#: 13).  Der Schluessel ist der Name der Aktion (``act_<name>``); die Begruendung erscheint
#: im Tooltip.  Eine Platte ist ein Blockgeraet mit vier WEGA-Partitionen, keine Diskette:
#: ohne Spuren/Sektoren, ohne Boot-Spuren, ohne Laufwerk.
PLATTE_GESPERRT = {
    "diskeditor": "Eine Platte hat keine Spuren und Sektoren im Sinn des Diskeditors — "
                  "sie ist ein Blockgerät mit WEGA-Partitionen.",
    "datei_im_editor": "Der Diskeditor arbeitet nur an Disketten (Spuren und Sektoren).",
    "angaben": "Geometrie, Medium und Erkennung gibt es nur für Disketten; die Platte "
               "ist durch ihren Plattenkopf (PAR-Sektor) festgelegt.",
    "speichern_unter": "Plattenabbilder werden an Ort und Stelle zurückgeschrieben (vorher "
                       "entsteht eine Sicherung „<abbild>~“).  Für eine Arbeitskopie die "
                       "Datei im Dateimanager kopieren.",
    "archivieren": "Das Archiv (.zip mit Abbild) ist für Disketten gedacht.  Den Inhalt "
                   "der Platte holt „Alles extrahieren“.",
    "bootabbild": "Eine Platte hat keine Systemspuren.",
    "reparieren": "Die Dateisystemprüfung und -reparatur arbeiten an Disketten.  Die "
                  "WEGA-Platte prüft man mit fsck im Emulator.",
    "wiederherstellen": "Die Suche nach gelöschten Dateien arbeitet an Disketten.",
    "alles_rein": "Ganze Ordner gehören über „Auf die Platte schreiben“ (Ordner in der "
                  "rechten Hälfte wählen, Zielordner auf der Platte anklicken) oder "
                  "durch Ziehen auf einen Ordner der Platte hinein.",
    "physisch_schreiben": "Der Greaseweazle schreibt Disketten, keine Platten.",
    "gerade_spuren": "Eine Platte hat keine Spuren.",
    "seite1_weg": "Eine Platte hat keine Diskettenseiten.",
    "tar_export": "Das ist keine tar-Diskette.",
    "tar_schreiben": "Das ist keine tar-Diskette.",
    "neu_beschreiben": "Das ist keine physische Diskette.",
}

#: Begruendung fuer das Dateisystem-Menue und das Auswahlfeld im Kopf.
PLATTE_FORMAT_FEST = ("Das Format ist durch die Platte festgelegt: P8000-Platte mit WEGA-"
                      "Dateisystem, Partitionen md0…md4 nach der Standardtabelle des Kerns.")


def dateisystem_anzeige(tool) -> str:
    """Name des Dateisystems in der Form, die man dem Anwender zeigt."""
    if ist_platte(tool):
        return "WEGA (P8000-Platte)"
    if tool.ist_tar:
        return "tar-Diskette, leer" if tool.tar_leer else "tar-Archiv (UNIX)"
    return tool.filesystem
