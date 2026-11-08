#!/bin/sh
# ─────────────────────────────────────────────────────────────────────────────
# K1520-Emulator — Deinstallation (Linux/macOS)
# ─────────────────────────────────────────────────────────────────────────────
#
# Entfernt die Startmenue-Eintraege (.desktop-Dateien) und die Verweise in
# ~/.local/bin, die zu DIESER Installation gehoeren, und fragt dann, ob der
# Programmordner geloescht werden soll.  Das Skript liegt in der Installation
# (<ziel>/uninstall.sh, im Startmenue als „K1520emu deinstallieren") und im
# Paket neben install.sh; `install.sh --uninstall` ruft es auf.
#
#     ./uninstall.sh                  # fragt, ob der Programmordner geloescht wird
#     ./uninstall.sh -y               # ohne Rueckfrage: Eintraege UND Programmordner
#     ./uninstall.sh --keep-folder    # nur Eintraege entfernen, Ordner behalten
#     ./uninstall.sh --purge          # zusaetzlich Disketten und Konfiguration loeschen
#     ./uninstall.sh --prefix DIR     # eine bestimmte Installation (aus dem Paket heraus)
#
# Geloescht wird nur, was der Installer selbst angelegt hat (Inventar im Ausweis);
# Eigenes im selben Ordner und die Arbeitsdisketten bleiben stehen.
set -eu

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$SELF_DIR/lib/common.sh"

PREFIX=
ASSUME_YES=no
KEEP_FOLDER=no
PURGE=no

usage() {
    cat <<EOF
K1520-Emulator — Deinstallation

  --prefix DIR     diese Installation entfernen (ohne Angabe: die, in der das Skript
                   liegt, sonst die, auf die ~/.local/bin zeigt)
  -y, --yes        nicht fragen: Eintraege und Programmordner entfernen
  --keep-folder    Startmenue-Eintraege und Verweise entfernen, den Ordner behalten
  --purge          zusaetzlich Disketten und Konfiguration loeschen
  -h, --help       diese Hilfe
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)      PREFIX=$2; shift 2 ;;
        --prefix=*)    PREFIX=${1#*=}; shift ;;
        -y|--yes)      ASSUME_YES=yes; shift ;;
        --keep-folder) KEEP_FOLDER=yes; shift ;;
        --purge)       PURGE=yes; shift ;;
        -h|--help)     usage; exit 0 ;;
        *) die "unbekannte Option: $1  (--help zeigt die Liste)" ;;
    esac
done

# Welche Installation?  Vorrang: ausdruecklich genannt, dann die, in der dieses
# Skript liegt, dann die, auf die ein Verweis in ~/.local/bin zeigt.
if [ -z "$PREFIX" ]; then
    if ist_installation "$SELF_DIR" && [ -f "$SELF_DIR/uninstall.sh" ]; then
        PREFIX=$SELF_DIR
    elif _gefunden=$(installation_ueber_verweise); then
        PREFIX=$_gefunden
    else
        die "keine Installation gefunden — bitte mit --prefix DIR angeben"
    fi
fi
PREFIX=$(abs_path "$PREFIX")

# Dieselben Riegel wie beim Installieren: geloescht wird nur, was sich ausweist.
case "$PREFIX" in
    /|"$HOME"|"$HOME"/) die "„$PREFIX\" darf kein Installationsverzeichnis sein" ;;
esac

info "Installation entfernen: $PREFIX"

# 1. Eintraege und Verweise — immer, auch wenn der Ordner bleibt.
_anzahl=$(entferne_menue_eintraege "$PREFIX")
ok "Startmenue-Eintraege und Verweise entfernt ($_anzahl)"

# 2. Der Programmordner — gefragt wird, weil der Anwender hier vielleicht noch
# Eigenes abgelegt hat (es bleibt ohnehin stehen) oder die Programme behalten will.
if [ ! -d "$PREFIX" ]; then
    warn "nichts zu loeschen — $PREFIX gibt es nicht"
elif ! ist_installation "$PREFIX"; then
    warn "$PREFIX sieht nicht nach einer Installation aus — es bleibt unangetastet"
else
    _loeschen=yes
    if [ "$KEEP_FOLDER" = yes ]; then
        _loeschen=no
    elif [ "$ASSUME_YES" = no ] && [ -r /dev/tty ] && [ -t 1 ]; then
        printf "\n"
        if ! frage_jn "Programmordner $PREFIX loeschen?" j; then _loeschen=no; fi
    fi
    if [ "$_loeschen" = yes ]; then
        entferne_installation "$PREFIX"
    else
        printf "     Der Programmordner bleibt: %s\n" "$PREFIX"
    fi
fi

if [ "$PURGE" = yes ]; then
    # Beide Ziele kommen aus common.sh und sind nie leer (Rueckfall auf ~/.local/share).
    rm -rf "${DATADIR:?}" "${CONFDIR:?}"
    ok "Disketten und Konfiguration geloescht"
else
    printf "     Behalten: %s (Disketten), %s (Konfiguration)\n" "$DATADIR" "$CONFDIR"
    printf "     Mit --purge loeschen.\n"
fi

# Aus dem Startmenue gestartet schliesst sich das Terminalfenster sofort — dann
# bliebe das Ergebnis ungelesen.
if [ "${K1520_UNINSTALL_WARTEN:-}" = 1 ] && [ -r /dev/tty ]; then
    printf "\nEnter zum Schliessen "
    IFS= read -r _ < /dev/tty || true
fi
exit 0
