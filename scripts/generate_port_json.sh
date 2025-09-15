#!/bin/bash
set -e

# Get version from tag
VERSION=${1:-0.0.0}
CURRENT_DATE=$(date +'%Y-%m-%d')

# Create port.json
cat << EOF > port.json
{
  "name": "VaixTerm",
  "version": "${VERSION}",
  "architecture": "aarch64",
  "runtimes": ["sdl2"],
  "executable": "vaixterm",
  "description": "SDL2-based terminal emulator for RG35XXH",
  "author": "Stanley00",
  "date": "${CURRENT_DATE}",
  "requires": ["sdl2", "sdl2_ttf", "sdl2_image"]
}
EOF
