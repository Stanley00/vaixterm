#!/bin/sh
# HELP: VAIX Terminal
# ICON: terminal
# GRID: Terminal

. /opt/muos/script/var/func.sh

echo app >/tmp/act_go

GOV_GO="/tmp/gov_go"
[ -e "$GOV_GO" ] && cat "$GOV_GO" >"$(GET_VAR "device" "cpu/governor")"

SETUP_SDL_ENVIRONMENT

BASE_DIR="$1"
cd "$BASE_DIR" || exit

SET_VAR "system" "foreground_process" "terminal"

./terminal -b res/background.png  --key-set -res/bash.keys --key-set -res/nano.keys --key-set -res/vim.keys --key-set -res/shell.keys --key-set +res/ctrl-shell.keys --key-set -res/vaixterm.keys --osk-layout res/qwerty.kb 

unset SDL_ASSERT SDL_HQ_SCALER SDL_ROTATION SDL_BLITTER_DISABLED
