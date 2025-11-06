#!/bin/bash

# Build the MUOS package
set -e

# Ensure we're in the right directory
cd "$(dirname "$0")"

# Create temporary directory for building
BUILD_DIR="./build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Copy base MuOS files
cp -r MUOS/Terminal "$BUILD_DIR/"

# Copy the terminal binary
cp ../vaixterm "$BUILD_DIR/Terminal/terminal"
chmod +x "$BUILD_DIR/Terminal/terminal"

# Copy resource files
mkdir -p "$BUILD_DIR/Terminal/res"
cp -r ../res/* "$BUILD_DIR/Terminal/res/"

chmod +x "$BUILD_DIR/Terminal/mux_launch.sh"

# Create the .muxapp package
cd "$BUILD_DIR"
zip -r "../VaixTerm.muxapp" ./*

# Clean up
cd ..
rm -rf "$BUILD_DIR"

echo "Package built: VaixTerm.muxapp"
