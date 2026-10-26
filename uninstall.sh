#!/usr/bin/env bash
# uninstall.sh - dk2hires vollstaendig zurueckbauen (Gegenstueck zu install.sh).
#
#   ./uninstall.sh [--state-dir DIR] [--dry-run]
#
# Entfernt dsound.dll, dk2hires.ini, dk2hires.log aus dem Spielordner, stellt die alten
# Registry-Werte wieder her (bzw. loescht die von install.sh angelegten) und vergleicht den
# Spielordner mit dem Manifest von vor der Installation.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/dk2hires"
DRY=0
NOPROC=0
while [ $# -gt 0 ]; do
  case "$1" in
    --state-dir) STATE_DIR="$2"; shift 2;;
    --dry-run) DRY=1; shift;;
    --no-process-check) NOPROC=1; shift;;   # nur fuer Tests gegen Kopien, nie fuer die echte Installation
    *) echo "unbekannte Option: $1"; exit 2;;
  esac
done
say() { printf '%s\n' "$*"; }
run() { if [ "$DRY" = 1 ]; then say "  (dry-run) $*"; else "$@"; fi; }
# Laeuft DK2 oder ein Wine-Prozess dieses Prefix? Unter Proton steht in WINEPREFIX "<Prefix>/pfx/".
check_running() {
  local p arg envpfx want="${PREFIX%/}"
  for p in /proc/[0-9]*; do
    [ "${p#/proc/}" = "$$" ] && continue
    while IFS= read -r -d '' arg; do
      case "${arg,,}" in *dkii*.exe) say "FEHLER: Dungeon Keeper 2 laeuft noch (PID ${p#/proc/}). Bitte beenden."; exit 1;; esac
    done < <(cat "$p/cmdline" 2>/dev/null || true)
    envpfx="$( { tr '\0' '\n' < "$p/environ"; } 2>/dev/null | sed -n 's/^WINEPREFIX=//p' || true)"
    envpfx="${envpfx%/}"; envpfx="${envpfx%/pfx}"
    if [ -n "$envpfx" ] && [ "$envpfx" = "$want" ]; then
      say "FEHLER: Im Prefix laeuft noch ein Wine-Prozess (PID ${p#/proc/}). Bitte warten oder beenden."; exit 1
    fi
  done
}

[ -f "$STATE_DIR/state.env" ] || { say "FEHLER: $STATE_DIR/state.env fehlt, nichts zurueckzubauen."; exit 1; }
# shellcheck disable=SC1091
source "$STATE_DIR/state.env"
: "${GAME_DIR:?}" "${PREFIX:?}" "${DLL_SHA:?}"
REG="$PREFIX/user.reg"
REGPY=(python3 -I "$HERE/dk2reg.py")
KEY_OVR='Software\Wine\AppDefaults\DKII-DX.exe\DllOverrides'
KEY_VID='Software\Bullfrog Productions Ltd\Dungeon Keeper II\Configuration\Video'
say "== dk2hires uninstall: Spielordner '$GAME_DIR', Prefix '$PREFIX' (installiert $INSTALLED_AT)"

[ "$NOPROC" = 1 ] || check_running

say "-- Dateien entfernen"
if [ -f "$GAME_DIR/dsound.dll" ]; then
  if [ "$(sha256sum < "$GAME_DIR/dsound.dll" | cut -d' ' -f1)" = "$DLL_SHA" ]; then run rm -f "$GAME_DIR/dsound.dll"
  else say "WARNUNG: dsound.dll im Spielordner ist nicht die installierte Fassung, bleibt liegen."; fi
fi
for f in dk2hires.ini dk2hires.log; do [ -e "$GAME_DIR/$f" ] && run rm -f "$GAME_DIR/$f"; done

say "-- Registry zuruecksetzen"
restore() { # key name oldraw
  if [ -n "$3" ]; then run "${REGPY[@]}" set "$REG" "$1" "$2" "$3"; else run "${REGPY[@]}" delete "$REG" "$1" "$2"; fi
}
restore "$KEY_VID" "Screen Width" "$OLD_W"
restore "$KEY_VID" "Screen Height" "$OLD_H"
restore "$KEY_OVR" "dsound" "$OLD_OVR"

if [ "$DRY" = 0 ]; then
  say "-- Kontrolle"
  say "   Screen Width=$("${REGPY[@]}" get "$REG" "$KEY_VID" "Screen Width" || echo '<fehlt>')  Screen Height=$("${REGPY[@]}" get "$REG" "$KEY_VID" "Screen Height" || echo '<fehlt>')  dsound=$("${REGPY[@]}" get "$REG" "$KEY_OVR" "dsound" || echo '<fehlt>')"
  # Dateien, die das Spiel selbst beschreibt (Log, Spielstaende, Highscores), duerfen sich unterscheiden.
  (cd "$GAME_DIR" && find . -type f -print0 | sort -z | xargs -0 sha256sum) > "$STATE_DIR/manifest-after-uninstall.sha256"
  if diff <(grep -v -E '\./(MESSAGE\.LOG|Data/Save/|Data/Settings/)' "$STATE_DIR/manifest-before.sha256") \
          <(grep -v -E '\./(MESSAGE\.LOG|Data/Save/|Data/Settings/)' "$STATE_DIR/manifest-after-uninstall.sha256") > "$STATE_DIR/manifest.diff"; then
    say "   Spielordner entspricht dem Stand vor der Installation (ohne Log/Spielstaende)."
  else
    say "   ABWEICHUNG zum Stand vor der Installation, siehe $STATE_DIR/manifest.diff"
  fi
  mv "$STATE_DIR/state.env" "$STATE_DIR/state.env.uninstalled-$(date +%Y%m%d-%H%M%S)"
fi
say "== fertig."
