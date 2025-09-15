#!/bin/bash

# Build the MUOS package
set -e

# Ensure we're in the right directory
cd "$(dirname "$0")"

# Create temporary directory for building
BUILD_DIR="./build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Copy the terminal binary
cp ../vaixterm "$BUILD_DIR/terminal"
chmod +x "$BUILD_DIR/terminal"

# Copy resource files
mkdir -p "$BUILD_DIR/res"
cp -r MUOS/Terminal/res/* "$BUILD_DIR/res/"

# Copy launch script
cp MUOS/Terminal/mux_launch.sh "$BUILD_DIR/"
chmod +x "$BUILD_DIR/mux_launch.sh"

# Create the .muxapp package
cd "$BUILD_DIR"
zip -r "../VaixTerm.muxapp" ./*

# Clean up
cd ..
rm -rf "$BUILD_DIR"

echo "Package built: VaixTerm.muxapp"
