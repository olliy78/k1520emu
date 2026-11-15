#!/bin/sh
# K1520 Printer — Emulator der Nadeldrucker (SD 1157, K6316).  @ROOT@ setzt der Installer.
#
# Startet die Oberflaeche aus der Laufzeitumgebung der Installation.  Bibliothek
# (libk1520core) findet das Programm selbst ueber app/paths.py; hier wird nur
# die Wurzel bekanntgegeben.
set -eu

ROOT="@ROOT@"

[ -x "$ROOT/venv/bin/python3" ] || {
    echo "K1520 Printer: Laufzeitumgebung fehlt in $ROOT/venv" >&2
    echo "Neu einrichten:  ./install.sh --prefix '$ROOT'" >&2
    exit 1
}

K1520_HOME="$ROOT"; export K1520_HOME

# Derselbe Datenordner wie beim Emulator-Starter — sonst oeffnete der
# Dateidialog woanders, als der Emulator seine Disketten ablegt.  Leer =
# Vorgabe, dann loest app/paths.py den Dokumentenordner selbst auf.
K1520_DATA="@DATEN@"
[ -n "$K1520_DATA" ] && export K1520_DATA || unset K1520_DATA

# Arbeitsverzeichnis = Benutzerdaten, damit der Dateidialog dort aufgeht, wo die
# Disketten liegen.  Dieselbe Aufloesung wie beim Emulator-Starter.
DATEN=$("$ROOT/venv/bin/python3" -c \
    'import sys; sys.path.insert(0, sys.argv[1]); from app import paths; print(paths.user_data_dir())' \
    "$ROOT" 2>/dev/null) || DATEN=
if [ -n "$DATEN" ] && mkdir -p "$DATEN" 2>/dev/null; then
    cd "$DATEN" || true
fi

exec "$ROOT/venv/bin/python3" "$ROOT/app/printertool/main.py" "$@"
