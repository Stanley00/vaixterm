#!/bin/bash

# Source the controls.txt from PortMaster directory
if [ -f "/opt/system/Tools/PortMaster/controls.txt" ]; then
    . "/opt/system/Tools/PortMaster/controls.txt"
else
    . "./controls.txt"
fi

# Set up environment variables
export TERM=xterm-256color
export TERMINFO=/usr/share/terminfo

# Set up paths
PORTDIR="/$directory/ports/vaixterm"
cd "$PORTDIR"

# Launch the terminal with gamepad controls
$GPTOKEYB "terminal" -c "./vaixterm.gptk" &

# Run the terminal
./terminal

# Clean up on exit
killall gptokeyb
"$ESUDO" systemctl restart oga_events &
printf "\033c" > /dev/tty1
