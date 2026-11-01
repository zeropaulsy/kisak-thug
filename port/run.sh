#!/bin/sh
# Runs the Linux build against a game install: port/run.sh "<dir holding Data/>" [game args]
DIR="$(dirname "$(readlink -f "$0")")"
GAME="$1"; shift
[ -d "$GAME/Data" ] || { echo "No Data/ folder in: $GAME" >&2; exit 1; }
export KISAK_GAME_DIR="$GAME"
exec "${KISAK_BIN:-$DIR/../build-linux/KisakThug}" "$@"
