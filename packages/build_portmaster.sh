#!/bin/bash

# Build the PortMaster package
set -e

# Ensure we're in the right directory
cd "$(dirname "$0")"

# Create temporary directory for building
BUILD_DIR="./build_portmaster"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Create the port data directory
PACKAGE_DIR="$BUILD_DIR/vaixterm"
mkdir -p "$PACKAGE_DIR/res"

# Copy the terminal binary into the port data directory
cp ../vaixterm "$PACKAGE_DIR/terminal"
chmod +x "$PACKAGE_DIR/terminal"

# Copy resource files into the port data directory
cp -r ../res/* "$PACKAGE_DIR/res/"

# Copy PortMaster specific files to the root of the build directory
cp PortMaster/vaixterm/port.json "$BUILD_DIR/"
cp PortMaster/vaixterm/README.md "$BUILD_DIR/"
cp PortMaster/vaixterm/gameinfo.xml "$BUILD_DIR/"
cp PortMaster/vaixterm/vaixterm.gptk "$PACKAGE_DIR/"
cp PortMaster/vaixterm/screenshot.png "$BUILD_DIR/"
cp PortMaster/VaixTerm.sh "$BUILD_DIR/"

chmod +x "$BUILD_DIR/VaixTerm.sh"

# Create a zip file for distribution
cd "$BUILD_DIR"
zip -r "../VaixTerm-PortMaster.zip" ./*

# Clean up
cd ..
rm -rf "$BUILD_DIR"

echo "PortMaster package built: VaixTerm-PortMaster.zip"
