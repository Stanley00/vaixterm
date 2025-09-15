#!/bin/bash

# Build the PortMaster package
set -e

# Ensure we're in the right directory
cd "$(dirname "$0")"

# Create temporary directory for building
BUILD_DIR="./build_portmaster"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Create the package directory structure
PACKAGE_DIR="$BUILD_DIR/vaixterm"
mkdir -p "$PACKAGE_DIR"

# Copy the terminal binary
cp ../vaixterm "$PACKAGE_DIR/terminal"
chmod +x "$PACKAGE_DIR/terminal"

# Copy resource files
mkdir -p "$PACKAGE_DIR/res"
cp -r PortMaster/res/* "$PACKAGE_DIR/res/"

# Copy PortMaster specific files
cp PortMaster/VaixTerm.sh "$PACKAGE_DIR/"
cp PortMaster/vaixterm.gptk "$PACKAGE_DIR/"
chmod +x "$PACKAGE_DIR/VaixTerm.sh"

# Create a zip file for distribution
cd "$BUILD_DIR"
zip -r "../VaixTerm-PortMaster.zip" ./*

# Clean up
cd ..
rm -rf "$BUILD_DIR"

echo "PortMaster package built: VaixTerm-PortMaster.zip"
