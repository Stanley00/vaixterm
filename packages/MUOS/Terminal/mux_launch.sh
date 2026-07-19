#!/bin/sh
# HELP: VaixTerm Terminal Emulator
# ICON: terminal
# GRID: VaixTerm

. /opt/muos/script/var/func.sh

APP_BIN="terminal"
SETUP_APP "$APP_BIN" ""

SETUP_STAGE_OVERLAY

GAMEDIR="$(GET_VAR "device" "storage/rom/mount")/MUOS/application/VaixTerm"
CONFDIR="$GAMEDIR/conf"

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

mkdir -p "$CONFDIR"

export XDG_DATA_HOME="$CONFDIR"
export SDL_GAMECONTROLLERCONFIG_FILE="/usr/lib/gamecontrollerdb.txt"

cd "$GAMEDIR" || exit

$GPTOKEYB "$APP_BIN" -c "./vaixterm.gptk" &

./$APP_BIN -w "$DISPLAY_WIDTH" -h "$DISPLAY_HEIGHT" -b res/background.png

$ESUDO kill -9 "$(pidof gptokeyb2)"
