#!/bin/sh
# Installer fuer Raspberry Pi OS (ARM64 / aarch64, Debian Bookworm oder neuer).
#
# Stellt zuerst die System-Bibliotheken bereit, die Qt (PySide6) und die
# Oberflaeche zur Laufzeit brauchen (per apt, nur das FEHLENDE), und ruft dann
# den gewohnten benutzerlokalen Installer install.sh auf.  Alle weiteren
# Argumente gehen unveraendert an install.sh (--prefix, --disks, --uninstall …).
#
#     ./install_raspberrypi.sh [--prefix DIR] [--no-apt | --deps-only | --check-deps]
#
#   --no-apt      keine Systempakete anfassen (nur install.sh)
#   --deps-only   nur die Systempakete bereitstellen, K1520emu nicht installieren
#   --check-deps  nur anzeigen, was installiert ist und was fehlt (kein sudo)
#
# sudo/root wird NUR fuer `apt-get install` gebraucht; K1520emu selbst
# installiert benutzerlokal.  Online-Voraussetzung: apt-Quellen fuer fehlende
# Pakete und (wie bei install.sh) der Download von Python/Qt.

set -eu

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

# Paket oder "a|b": Alternativen in Reihenfolge der Bevorzugung.  Auf Debian 13/
# Ubuntu 24.04 heissen einige Pakete wegen des 64-Bit-time_t-Umbaus …t64.
APT_PAKETE="
libgtk-3-0|libgtk-3-0t64
libasound2|libasound2t64
libnss3
libxss1
libgbm1
libx11-xcb1
libxkbcommon0
libdrm2
libxdamage1
libxcomposite1
libxrandr2
libatk1.0-0|libatk1.0-0t64
libatk-bridge2.0-0|libatk-bridge2.0-0t64
libpango-1.0-0
libcairo2
libxcb-cursor0
libegl1
libgl1
"

MODUS=install
APT=yes
ARGS=
for a in "$@"; do
    case "$a" in
        --no-apt)     APT=no ;;
        --deps-only)  MODUS=deps ;;
        --check-deps) MODUS=check ;;
        *)            ARGS="$ARGS
$a" ;;
    esac
done

meld() { printf '%s\n' "$*"; }
fehler() { printf 'FEHLER: %s\n' "$*" >&2; exit 1; }

if [ "${K1520_RPI_SKIP_ARCH_CHECK:-}" != 1 ]; then
    case "$(uname -m)" in
        aarch64|arm64) ;;
        *) fehler "dieses Paket ist fuer ARM64 (aarch64); dieser Rechner ist $(uname -m)" ;;
    esac
fi

ist_installiert() { dpkg -s "$1" 2>/dev/null | grep -q '^Status: install ok installed'; }
ist_verfuegbar()  { apt-cache show "$1" >/dev/null 2>&1; }

paket_pruefen() {
    # stdout: "ok NAME" | "fehlt NAME" | "unbekannt SPEC"
    _spec=$1
    _rest=$_spec
    while [ -n "$_rest" ]; do
        _p=${_rest%%|*}
        if [ "$_p" = "$_rest" ]; then _rest=; else _rest=${_rest#*|}; fi
        if ist_installiert "$_p"; then echo "ok $_p"; return; fi
    done
    _rest=$_spec
    while [ -n "$_rest" ]; do
        _p=${_rest%%|*}
        if [ "$_p" = "$_rest" ]; then _rest=; else _rest=${_rest#*|}; fi
        if ist_verfuegbar "$_p"; then echo "fehlt $_p"; return; fi
    done
    echo "unbekannt $_spec"
}

if [ "$APT" = yes ]; then
    command -v dpkg >/dev/null 2>&1 && command -v apt-get >/dev/null 2>&1 \
        || fehler "dpkg/apt-get nicht gefunden — kein Debian/Raspberry Pi OS? Mit --no-apt ueberspringen"

    meld "== Systempakete pruefen =="
    FEHLEND=
    UNBEKANNT=
    for spec in $APT_PAKETE; do
        set -- $(paket_pruefen "$spec")
        case "$1" in
            ok)       meld "  [vorhanden] $2" ;;
            fehlt)    meld "  [fehlt]     $2"; FEHLEND="$FEHLEND $2" ;;
            unbekannt) meld "  [?]         $2 (in den apt-Quellen nicht gefunden)"; UNBEKANNT="$UNBEKANNT $2" ;;
        esac
    done

    [ "$MODUS" = check ] && exit 0

    if [ -n "$FEHLEND" ]; then
        if [ "$(id -u)" = 0 ]; then SUDO=
        elif command -v sudo >/dev/null 2>&1; then SUDO=sudo
        else fehler "fehlende Pakete:$FEHLEND — dafuer sind root-Rechte noetig, aber sudo ist nicht vorhanden. Als root ausfuehren oder die Pakete von Hand installieren"
        fi
        meld "== Installiere per apt:$FEHLEND =="
        # shellcheck disable=SC2086
        $SUDO apt-get update
        # shellcheck disable=SC2086
        $SUDO apt-get install -y --no-install-recommends $FEHLEND \
            || fehler "apt-get install schlug fehl"
    else
        meld "Alle benoetigten Systempakete sind vorhanden."
    fi
    [ -z "$UNBEKANNT" ] || meld "Hinweis: nicht in den Quellen gefunden (ggf. unter anderem Namen vorhanden):$UNBEKANNT"
elif [ "$MODUS" != install ]; then
    fehler "--no-apt passt nicht zu --deps-only/--check-deps"
fi

[ "$MODUS" = install ] || exit 0

[ -f "$SELF_DIR/install.sh" ] || fehler "install.sh liegt nicht neben diesem Skript"
IFS='
'
# shellcheck disable=SC2086
set -- $ARGS
unset IFS
exec sh "$SELF_DIR/install.sh" "$@"
