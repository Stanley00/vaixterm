# Makefile for cross-compiling the SDL2 Terminal Emulator with a Buildroot toolchain.
#
# This Makefile is designed to be used with a pre-built Buildroot SDK or
# the output directory of a Buildroot build.
#
# Usage:
#   1. Set the BUILDROOT_HOST_DIR environment variable to point to the 'host'
#      directory within your Buildroot output directory.
#      Example:
#      $ export BUILDROOT_HOST_DIR=/path/to/buildroot/output/host
#
#   2. Run make with this file:
#      $ make -f Makefile.buildroot
#
#   3. To install the binary to the Buildroot target directory:
#      $ make -f Makefile.buildroot install
#

# --- Configuration ---

BUILDROOT_HOST_DIR = aarch64-buildroot-linux-gnu_sdk-buildroot

# The final executable name
TARGET = terminal

# Source files
SOURCES = main.c terminal.c rendering.c osk.c input.c manualfont.c


# --- Buildroot Toolchain Setup ---

# Check if the Buildroot host directory is provided.
ifeq ($(strip $(BUILDROOT_HOST_DIR)),)
    $(error BUILDROOT_HOST_DIR is not set. Please export it to point to your Buildroot output's host directory.)
endif

# Automatically determine the target tuple (e.g., arm-buildroot-linux-gnueabihf)
# This looks for the first compiler-like name in the bin directory.
TARGET_HOST ?= $(shell basename $(shell find $(BUILDROOT_HOST_DIR)/bin -name '*buildroot*-gcc' -print -quit) -gcc)

ifeq ($(strip $(TARGET_HOST)),)
    $(error Could not determine TARGET_HOST from $(BUILDROOT_HOST_DIR). Please set it manually.)
endif

# Define paths based on the Buildroot directory structure
SYSROOT = $(BUILDROOT_HOST_DIR)/$(TARGET_HOST)/sysroot
CC = $(BUILDROOT_HOST_DIR)/bin/$(TARGET_HOST)-gcc
STRIP = $(BUILDROOT_HOST_DIR)/bin/$(TARGET_HOST)-strip

# --- Compiler and Linker Flags ---

# CFLAGS:
# -Wall -Wextra: Enable all warnings.
# -g: Include debugging symbols.
# --sysroot: Point the compiler to the target's root filesystem for headers and libraries.
# -I...: Explicitly add the SDL2 include path within the sysroot.
CFLAGS = -Wall -Wextra -g --sysroot=$(SYSROOT) -I$(SYSROOT)/usr/include/SDL2 -O3

# LDFLAGS:
# --sysroot: Point the linker to the target's root filesystem for shared libraries.
LDFLAGS = --sysroot=$(SYSROOT)

# LIBS:
# The required libraries. The linker will find them within the sysroot.
LIBS = -lSDL2 -lSDL2_ttf -lSDL2_image -lutil -lm

# --- Build Rules ---

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(SOURCES)
	@echo "--- Cross-compiling for $(TARGET_HOST) ---"
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS) $(LIBS)
	@echo "--- Build complete: $(TARGET) ---"

.PHONY: clean
clean:
	@echo "--- Cleaning up ---"
	rm -f $(TARGET)
	rm -f $(DATA_FILES)
