# Gemeinsame Shell-Bausteine der Paketierung (Bourne-kompatibel, kein Bashismus).
#
# Wird von packaging/build_payload.sh (Bauen) und packaging/install.sh
# (Installieren beim Anwender) eingebunden.  Entwurf: doc/design/13_distribution.md
#
# Einbinden:  . "$(dirname "$0")/lib/common.sh"

# Python-Fassung der Laufzeitumgebung.  Dieselbe beim Schnueren (requirements.lock
# wird dafuer aufgeloest) wie beim Installieren — sonst passen die Wheels nicht.
K1520_PY_VERSION=${K1520_PY_VERSION:-3.14}

# ─── Meldungen ───────────────────────────────────────────────────────────────
# Farben nur, wenn die Ausgabe wirklich ein Terminal ist — in Protokollen und
# Installer-Fenstern stoeren Steuerzeichen.

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_INFO='\033[1;34m'; C_OK='\033[1;32m'; C_WARN='\033[1;33m'
    C_ERR='\033[1;31m';  C_OFF='\033[0m'
else
    C_INFO=''; C_OK=''; C_WARN=''; C_ERR=''; C_OFF=''
fi

info()  { printf "${C_INFO}==>${C_OFF} %s\n" "$*"; }
ok()    { printf "${C_OK}  ok${C_OFF} %s\n" "$*"; }
warn()  { printf "${C_WARN}Warnung:${C_OFF} %s\n" "$*" >&2; }
die()   { printf "${C_ERR}Fehler:${C_OFF} %s\n" "$*" >&2; exit 1; }

# ─── Plattform ───────────────────────────────────────────────────────────────

# ist_windows — laeuft dieses Skript unter Git-Bash/MSYS auf Windows?
#
# Nur fuers SCHNUEREN gedacht (build_payload.sh).  Der Anwender bekommt unter
# Windows ein Installationsprogramm und sieht nie eine .sh; bauen ist dagegen kein
# Anwenderschritt, und die Liste „was gehoert ins Paket" soll es nicht zweimal
# geben — deshalb hier ein Zweig statt eines zweiten Skripts.
ist_windows() {
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*) return 0 ;;
        *) return 1 ;;
    esac
}

# Zieltripel, wie es die uv-Releases benennen.
uv_target() {
    _os=$(uname -s)
    _arch=$(uname -m)
    case "$_arch" in
        x86_64|amd64)  _arch=x86_64 ;;
        aarch64|arm64) _arch=aarch64 ;;
        *) die "nicht unterstuetzte Architektur: $_arch" ;;
    esac
    case "$_os" in
        Linux)  echo "${_arch}-unknown-linux-gnu" ;;
        Darwin) echo "${_arch}-apple-darwin" ;;
        MINGW*|MSYS*|CYGWIN*) echo "${_arch}-pc-windows-msvc" ;;
        *) die "nicht unterstuetztes System: $_os" ;;
    esac
}

# Dateiname der Kernbibliothek auf diesem System.
core_lib_name() {
    case "$(uname -s)" in
        Darwin)               echo "libk1520core.dylib" ;;
        MINGW*|MSYS*|CYGWIN*) echo "k1520core.dll" ;;
        *)                    echo "libk1520core.so" ;;
    esac
}

# disk_lib_name — Bibliothek des k1520DiskTool (Dateiaustausch mit Disketten).
# Bewusst eine ZWEITE Bibliothek: das Werkzeug braucht keinen Z80, der Emulator
# keine Dateisysteme (doc/design/13_k1520disktool.md §2).
disk_lib_name() {
    case "$(uname -s)" in
        Darwin)               echo "libk1520disk.dylib" ;;
        MINGW*|MSYS*|CYGWIN*) echo "k1520disk.dll" ;;
        *)                    echo "libk1520disk.so" ;;
    esac
}

# disk_cli_name — Kommandozeile des k1520DiskTool (unter Windows mit .exe).
disk_cli_name() {
    if ist_windows; then echo "k1520disktool.exe"; else echo "k1520disktool"; fi
}

# kermit_lib_name — Bibliothek des K1520 Kermit (Datenaustausch mit der WEGA/P8000,
# doc/design/36_kermit.md).  Wie das DiskTool eine EIGENE Bibliothek: weder Z80 noch
# Dateisysteme, dafuer Telnet/RFC 2217.
kermit_lib_name() {
    case "$(uname -s)" in
        Darwin)               echo "libk1520kermit.dylib" ;;
        MINGW*|MSYS*|CYGWIN*) echo "k1520kermit.dll" ;;
        *)                    echo "libk1520kermit.so" ;;
    esac
}

# kermit_cli_name — Kommandozeile des K1520 Kermit (unter Windows mit .exe).
kermit_cli_name() {
    if ist_windows; then echo "k1520kermit.exe"; else echo "k1520kermit"; fi
}

# kerne — Anzahl der Prozessorkerne fuers parallele Bauen.
# `getconf` gibt es in der Git-Bash nicht; Windows setzt NUMBER_OF_PROCESSORS.
kerne() {
    getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null \
        || echo "${NUMBER_OF_PROCESSORS:-4}"
}

# abs_path <pfad> — Tilde und Relativangaben aufloesen.
#
# Die Tilde muss dabei GESCHUETZT werden: unquotiert dehnt die Shell sie in der
# Musterangabe selbst zu $HOME aus, das Muster passt dann nie, und im Ergebnis
# bliebe ein Verzeichnis namens „~" stehen.
abs_path() {
    _p=$1
    case "$_p" in
        "~")   _p="$HOME" ;;
        "~/"*) _p="$HOME/${_p#"~/"}" ;;
    esac
    case "$_p" in
        /*) ;;
        *)  _p="$(pwd)/$_p" ;;
    esac
    echo "$_p"
}

# Dateiname des Merkmals, an dem eine Installation zu erkennen ist.
INSTALL_MARKER=".k1520emu-installation"

# ist_installation <verzeichnis> — traegt das Verzeichnis unsere Installation?
#
# Das Deinstallieren LOESCHT sein Ziel, und seit das Ziel erfragt wird, kann dort
# alles stehen — im schlimmsten Fall das Heimatverzeichnis.  Geloescht wird
# deshalb nur, was sich ausweisen kann.  Installationen aus der Zeit vor dem
# Merkmal werden am Inventar erkannt, damit ein Update ueber sie hinweg geht.
ist_installation() {
    [ -f "$1/$INSTALL_MARKER" ] && return 0
    [ -f "$1/VERSION" ] && [ -f "$1/app/paths.py" ] && [ -d "$1/share/k1520emu" ]
}

# ersetze_platzhalter <vorlage> <wurzel> [<daten>] — Vorlage ausfuellen und ausgeben.
#
# Ersetzt @ROOT@ durch die Installationswurzel und @DATEN@ durch den vom Anwender
# gewaehlten Datenordner.  @DATEN@ bleibt LEER, wenn er die Vorgabe genommen hat —
# der Starter setzt K1520_DATA dann gar nicht, und app/paths.py loest den
# Dokumentenordner weiter dynamisch auf (folgt also einem spaeteren Umbenennen).
#
# Bewusst ohne `sed`: der Installationspfad kommt seit der Zielabfrage vom
# Anwender und darf jedes Zeichen enthalten — „|" waere dort das Trennzeichen des
# Ausdrucks, „&" und „\" in der Ersetzung wieder etwas anderes.  Je Zeile ein
# Platzhalter genuegt (alle Vorlagen halten sich daran).
ersetze_platzhalter() {
    _vorlage=$1; _wurzel=$2; _daten=${3:-}
    while IFS= read -r _zeile || [ -n "$_zeile" ]; do
        case "$_zeile" in
            *@ROOT@*)  printf '%s%s%s\n' "${_zeile%%@ROOT@*}"  "$_wurzel" "${_zeile#*@ROOT@}" ;;
            *@DATEN@*) printf '%s%s%s\n' "${_zeile%%@DATEN@*}" "$_daten"  "${_zeile#*@DATEN@}" ;;
            *)         printf '%s\n' "$_zeile" ;;
        esac
    done < "$_vorlage"
}

# dokumente_dir — Dokumentenordner des Anwenders ausgeben, sonst mit 1 enden.
#
# Sein Name ist sprachabhaengig (~/Dokumente, ~/Documents, ~/Documenti);
# verbindlich ist XDG_DOCUMENTS_DIR aus ~/.config/user-dirs.dirs, die die
# Desktop-Umgebung selbst pflegt.  Gelesen wird die Datei direkt, nicht ueber das
# Programm `xdg-user-dir` — das fehlt auf schlanken Systemen.
#
# ACHTUNG: dieselbe Regel steht in app/paths.py (`documents_dir`).  Beide
# muessen dasselbe liefern, sonst raeumt `--purge` woanders auf, als der Emulator
# schreibt; Guard: `test_dokumentenordner_shell_und_python_stimmen_ueberein`.
dokumente_dir() {
    if [ -n "${XDG_DOCUMENTS_DIR:-}" ]; then
        echo "$XDG_DOCUMENTS_DIR"
        return 0
    fi
    _ud="${XDG_CONFIG_HOME:-$HOME/.config}/user-dirs.dirs"
    if [ -f "$_ud" ]; then
        _wert=$(sed -n 's/^[[:space:]]*XDG_DOCUMENTS_DIR=//p' "$_ud" | tail -n 1)
        _wert=$(printf '%s' "$_wert" | tr -d "\"'")
        # In der Datei steht „$HOME/Dokumente" — die Variable gehoert zum Format,
        # eine Shell hat sie nie gesehen.  Ersetzt wird sie ohne `sed`, damit ein
        # Sonderzeichen im Heimatpfad nicht zum Ausdruck wird.
        case "$_wert" in
            '$HOME')   _wert=$HOME ;;
            '$HOME/'*) _wert="$HOME/${_wert#\$HOME/}" ;;
        esac
        if [ -n "$_wert" ]; then
            echo "$_wert"
            return 0
        fi
    fi
    for _n in Documents Dokumente; do
        if [ -d "$HOME/$_n" ]; then echo "$HOME/$_n"; return 0; fi
    done
    return 1
}

# benutzerdaten_dir — wohin der Emulator die Anwenderdaten legt (Disketten …).
benutzerdaten_dir() {
    if _docs=$(dokumente_dir); then
        echo "$_docs/K1520emu"
    else
        echo "${XDG_DATA_HOME:-$HOME/.local/share}/K1520emu"
    fi
}

# ist_leer <verzeichnis> — Verzeichnis nicht vorhanden oder ohne Inhalt?
ist_leer() {
    [ -d "$1" ] || return 0
    [ -z "$(ls -A "$1" 2>/dev/null)" ]
}

# ─── Werkzeuge ───────────────────────────────────────────────────────────────

have() { command -v "$1" >/dev/null 2>&1; }

# fetch <url> <ziel> — laedt eine Datei (curl oder wget, je nachdem was da ist).
fetch() {
    if have curl; then
        curl -fsSL --retry 3 --connect-timeout 20 -o "$2" "$1" \
            || die "Download fehlgeschlagen: $1"
    elif have wget; then
        wget -q --tries=3 --timeout=20 -O "$2" "$1" \
            || die "Download fehlgeschlagen: $1"
    else
        die "weder curl noch wget gefunden — eines von beiden wird zum Nachladen gebraucht"
    fi
}

# sha256_of <datei> — Pruefsumme, egal ob sha256sum (Linux) oder shasum (macOS).
sha256_of() {
    if have sha256sum; then sha256sum "$1" | awk '{print $1}'
    elif have shasum;   then shasum -a 256 "$1" | awk '{print $1}'
    else die "kein sha256sum/shasum gefunden — Pruefsummen nicht ueberpruefbar"
    fi
}

# ─── uv ──────────────────────────────────────────────────────────────────────

# uv_pin <pins-datei> <schluessel> — Wert aus packaging/uv_pins.txt lesen.
uv_pin() {
    awk -v key="$2" '$1 == key { print $2; exit }' "$1"
}

# ensure_uv <zielverzeichnis> <pins-datei>
#
# Legt <zielverzeichnis>/uv an und gibt den Pfad aus.  Ist dort schon ein uv,
# wird es benutzt.  Sonst: gepinnte Fassung laden, Pruefsumme gegen die
# MITGELIEFERTE Erwartung halten, entpacken.
#
# Ein systemweit vorhandenes uv wird bewusst NICHT verwendet: die Installation
# soll auf jedem Rechner dieselbe, gepruefte Fassung benutzen und beim
# Deinstallieren restlos verschwinden.
ensure_uv() {
    _dir=$1
    _pins=$2
    _uv="$_dir/uv"
    [ -x "$_uv" ] && { echo "$_uv"; return 0; }

    [ -f "$_pins" ] || die "uv-Pins nicht gefunden: $_pins"
    _ver=$(uv_pin "$_pins" version)
    _target=$(uv_target)
    _want=$(uv_pin "$_pins" "$_target")
    [ -n "$_ver" ] || die "keine uv-Version in $_pins"
    [ -n "$_want" ] || die "keine uv-Pruefsumme fuer $_target in $_pins"

    _tmp=$(mktemp -d) || die "kein temporaeres Verzeichnis anlegbar"
    _archive="$_tmp/uv.tar.gz"
    _url="https://github.com/astral-sh/uv/releases/download/$_ver/uv-$_target.tar.gz"

    info "uv $_ver fuer $_target laden (~15 MB)" >&2
    fetch "$_url" "$_archive"

    _got=$(sha256_of "$_archive")
    [ "$_got" = "$_want" ] || {
        rm -rf "$_tmp"
        die "Pruefsumme von uv stimmt nicht:
  erwartet $_want
  erhalten $_got
Abbruch — die heruntergeladene Datei wird nicht ausgefuehrt."
    }

    ( cd "$_tmp" && tar xzf uv.tar.gz ) || { rm -rf "$_tmp"; die "uv-Archiv nicht entpackbar"; }
    mkdir -p "$_dir"
    # Das Archiv enthaelt ein Verzeichnis uv-<tripel>/ mit uv und uvx.
    cp "$_tmp/uv-$_target/uv" "$_uv" || { rm -rf "$_tmp"; die "uv nicht auffindbar im Archiv"; }
    chmod +x "$_uv"
    rm -rf "$_tmp"

    # Virenscanner und Sicherheitsrichtlinien greifen sich gelegentlich frisch
    # heruntergeladene Binaerdateien — das hier ist die verstaendliche Meldung.
    [ -x "$_uv" ] || die "uv wurde nach dem Entpacken entfernt (Virenscanner? Richtlinie?)"
    echo "$_uv"
}

# ─── Greaseweazle ────────────────────────────────────────────────────────────

# gw_pin <pins-datei> <schluessel> — Wert aus packaging/gw_pins.txt lesen.
gw_pin() {
    awk -v key="$2" '$1 == key { print $2; exit }' "$1"
}

# gw_quelle_url <fassung> — Adresse des Quellarchivs am GitHub-Release.
gw_quelle_url() {
    echo "https://github.com/keirf/greaseweazle/releases/download/v$1/greaseweazle-$1.zip"
}

# ─── Ablageorte und Inventar (Installieren UND Deinstallieren) ───────────────

BINDIR="$HOME/.local/bin"
APPDIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICONDIR="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/apps"
DATADIR=$(benutzerdaten_dir)
CONFDIR="${XDG_CONFIG_HOME:-$HOME/.config}/k1520emu"

# Was der Installer im Zielverzeichnis anlegt — und ausschliesslich das entfernt
# das Deinstallieren wieder.  Die Liste wandert beim Installieren in den Ausweis,
# sodass eine aeltere Installation nach ihrer EIGENEN Liste abgeraeumt wird und
# nicht nach der einer neueren Fassung.  `uninstall.sh` und `lib` sind das
# Deinstallationsskript samt seinem Hilfsmodul.
INVENTAR="bin app share venv python tools .cache-uv VERSION requirements.lock LICENSE LIZENZ.txt uninstall.sh lib"

# frage_jn <frage> <vorgabe j|n> — Ja/Nein am Terminal; ohne Terminal gilt die Vorgabe.
#
# Gelesen wird von /dev/tty, nicht von der Standardeingabe: der Installer kann
# aus einer Pipe laufen.  Rueckgabe 0 = ja, 1 = nein.
frage_jn() {
    _frage=$1; _vorgabe=$2
    if [ -r /dev/tty ] && [ -t 1 ]; then
        if [ "$_vorgabe" = j ]; then _hinweis="[J/n]"; else _hinweis="[j/N]"; fi
        printf "%s %s: " "$_frage" "$_hinweis"
        _antwort=
        IFS= read -r _antwort < /dev/tty || true
        case "$_antwort" in
            j|J|ja|Ja|JA|y|Y|yes|Yes) return 0 ;;
            n|N|nein|Nein|NEIN|no|No) return 1 ;;
        esac
    fi
    [ "$_vorgabe" = j ]
}

# entferne_menue_eintraege <wurzel> — Verweise und Startmenue-Eintraege einer Installation.
#
# Entfernt wird NACH INHALT, nicht nach einer Namensliste: ein Verweis in
# ~/.local/bin, der in <wurzel>/ zeigt, und eine .desktop-Datei, deren Exec-Zeile
# in <wurzel>/ liegt.  Damit verschwinden auch Eintraege, die eine AELTERE
# Fassung angelegt hat und die in der heutigen Liste nicht mehr stehen — und eine
# zweite Installation an anderem Ort bleibt unangetastet.  Gibt die Zahl der
# entfernten Dateien aus.
entferne_menue_eintraege() {
    _w=$1; _n=0; _a5120=no
    [ -n "$_w" ] && [ "$_w" != / ] || { echo 0; return 0; }
    if [ -d "$BINDIR" ]; then
        for _l in "$BINDIR"/*; do
            [ -L "$_l" ] || continue
            case "$(readlink "$_l" 2>/dev/null || true)" in
                "$_w"/*) rm -f "$_l"; _n=$((_n + 1)) ;;
            esac
        done
    fi
    if [ -d "$APPDIR" ]; then
        for _d in "$APPDIR"/*.desktop; do
            [ -f "$_d" ] || continue
            if grep '^Exec=' "$_d" 2>/dev/null | grep -qF "$_w/"; then
                [ "$(basename "$_d")" = a5120emu.desktop ] && _a5120=yes
                rm -f "$_d"; _n=$((_n + 1))
            fi
        done
    fi
    # Das gemeinsame Symbol aller Eintraege (Icon=a5120emu) — erst weg, wenn der
    # Eintrag, der es mitbrachte, weg ist.
    if [ "$_a5120" = yes ]; then rm -f "$ICONDIR/a5120emu.svg"; fi
    if have update-desktop-database && [ -d "$APPDIR" ]; then
        update-desktop-database "$APPDIR" >/dev/null 2>&1 || true
    fi
    echo "$_n"
}

# installation_ueber_verweise — Wurzel einer Installation, auf die ein Verweis in
# ~/.local/bin zeigt (irgendeiner, nicht nur die der bekannten Maschinen).
installation_ueber_verweise() {
    [ -d "$BINDIR" ] || return 1
    for _l in "$BINDIR"/*; do
        [ -L "$_l" ] || continue
        _ziel=$(readlink -f "$_l" 2>/dev/null || true)
        case "$_ziel" in
            */bin/*) _wurzel=${_ziel%/bin/*} ;;
            *) continue ;;
        esac
        if [ -n "$_wurzel" ] && [ -d "$_wurzel" ] && ist_installation "$_wurzel"; then
            echo "$_wurzel"
            return 0
        fi
    done
    return 1
}

# entferne_installation <wurzel> — nur das wegraeumen, was der Installer anlegte.
#
# Ein rekursives Loeschen der Wurzel waere einfacher, naehme dem Anwender aber
# alles mit, was er DANEBEN abgelegt hat — eigene Disketten, Notizen, ein Skript.
# Geloescht wird deshalb nach dem Inventar aus dem Ausweis (eine aeltere
# Installation nach ihrer eigenen Liste, nicht nach der dieser Fassung); bleibt
# danach etwas uebrig, bleibt auch das Verzeichnis stehen und der Anwender
# erfaehrt es.
entferne_installation() {
    _wurzel=$1
    # Nie mit leerer oder Wurzel-Angabe weiterlaufen.
    if [ -z "$_wurzel" ] || [ "$_wurzel" = / ]; then
        warn "keine Installationswurzel angegeben — nichts geloescht"
        return 1
    fi
    _liste=$(sed -n 's/^eintrag //p' "$_wurzel/$INSTALL_MARKER" 2>/dev/null || true)
    [ -n "$_liste" ] || _liste=$INVENTAR

    for _e in $_liste; do
        # Ein Eintrag ist ein NAME, kein Pfad — sonst zeigte ein verfaelschter
        # Ausweis („../..") aus der Installation heraus.
        case "$_e" in
            */*|..|.|"") warn "uebergangen: fragwuerdiger Eintrag im Ausweis ($_e)"; continue ;;
        esac
        rm -rf "${_wurzel:?}/${_e:?}"
    done
    # Protokolle des Emulators — sie entstehen im ARBEITSVERZEICHNIS (seit
    # e6db12c in dessen Unterverzeichnis `logs/`, siehe k1520_api.cpp), also
    # hier, sobald jemand aus der Installation heraus startet.  Die aeltere
    # Schreibweise direkt im Verzeichnis bleibt beruecksichtigt, damit auch eine
    # damit angelegte Installation sauber verschwindet.
    rm -rf "${_wurzel:?}/logs"
    rm -f "$_wurzel"/k1520_*.log
    rm -f "${_wurzel:?}/$INSTALL_MARKER"

    if rmdir "$_wurzel" 2>/dev/null; then
        ok "Verzeichnis geloescht"
    else
        ok "Programmdateien entfernt"
        printf "     %s bleibt stehen — darin liegt, was nicht vom Installer stammt:\n" "$_wurzel"
        ls -A "$_wurzel" 2>/dev/null | sed 's/^/       /'
    fi
}
