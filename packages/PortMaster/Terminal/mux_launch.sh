#!/bin/bash

# Get the directory this script is in
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Set the working directory to the script's directory
cd "$SCRIPT_DIR"

# Set environment variables
export TERM=xterm-256color
export TERMINFO=/usr/share/terminfo

exec ./terminal
