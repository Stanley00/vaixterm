#!/bin/sh
# HELP: VAIX Terminal
# ICON: terminal
# GRID: Terminal

. /opt/muos/script/var/func.sh

echo app >/tmp/act_go

SETUP_SDL_ENVIRONMENT

BASE_DIR="$(GET_VAR "device" "storage/rom/mount")/MUOS/application/Terminal"
cd "$BASE_DIR" || exit

SET_VAR "system" "foreground_process" "terminal"

(
        while ! pgrep -f "terminal" >/dev/null; do
                sleep 0.25
        done
        sleep 1
        evemu-event "$(GET_VAR "device" "input/ev1")" --type "$(GET_VAR "device" "input/type/dpad/right")" --code "$(GET_VAR "device" "input/code/dpad/right")" --value 1
        evemu-event "$(GET_VAR "device" "input/ev1")" --type "$(GET_VAR "device" "input/type/dpad/left")" --code "$(GET_VAR "device" "input/code/dpad/left")" --value -1
) &

SDL_ASSERT=always_ignore SDL_GAMECONTROLLERCONFIG=$(grep "muOS-Keys" "/usr/lib/gamecontrollerdb.txt") ./terminal -b res/background.png  --key-set -res/bash.keys --key-set -res/nano.keys --key-set -res/vim.keys --key-set -res/shell.keys --key-set +res/ctrl-shell.keys --key-set -res/vaixterm.keys --osk-layout res/qwerty.kb 

unset SDL_HQ_SCALER SDL_ROTATION SDL_BLITTER_DISABLED
