#!/bin/bash
# k1520printer - Starter des Druckeremulators (Gegenstueck zu run_a5120emu.sh, run_kermit.sh)
# Eigenes Programm mit eigenem Fenster (app/printertool/): das Blatt statt des Bildschirms,
# Konfiguration k1520printer.yaml.  Aktiviert das venv selbst.

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$PROJECT_DIR/venv"
BUILD_DIR="$PROJECT_DIR/build"

if [ ! -d "$VENV_DIR" ]; then
    echo "ERROR: Virtual environment not found at $VENV_DIR"
    echo ""
    echo "To create it, run:"
    echo "  python3 -m venv venv"
    echo "  source venv/bin/activate"
    echo "  pip install -r requirements.txt"
    exit 1
fi

# Den Interpreter des venv DIREKT aufrufen (siehe run_mc80emu.sh: `activate` traegt einen absoluten Pfad ein).
PY="$VENV_DIR/bin/python3"

export LD_LIBRARY_PATH="$BUILD_DIR:$LD_LIBRARY_PATH"

exec "$PY" "$PROJECT_DIR/app/printertool/main.py" "$@"
