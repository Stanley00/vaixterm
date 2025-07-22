
# Makefile for SDL2 Terminal Emulator
# Supports both native and Buildroot cross-compilation

# Source files
SRCS = src/main.c src/input.c src/osk.c src/rendering.c src/terminal.c src/manualfont.c
TARGET = vaixterm

# Native build defaults
NATIVE_CC = gcc
NATIVE_CFLAGS = -Wall -Wextra -O2 -g `sdl2-config --cflags` -Isrc -Iinclude
NATIVE_LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lSDL2_image -lm

# Cross-compile (Buildroot) defaults (set only if BUILDROOT_HOST_DIR is set)
ifeq ($(strip $(BUILDROOT_HOST_DIR)),)
  # Native build
  CC := $(NATIVE_CC)
  CFLAGS := $(NATIVE_CFLAGS)
  LDFLAGS := $(NATIVE_LDFLAGS)
  BUILD_MODE := native
else
  # Cross build
  TARGET_HOST ?= $(shell basename $(shell find $(BUILDROOT_HOST_DIR)/bin -name '*buildroot*-gcc' -print -quit) -gcc)
  ifeq ($(strip $(TARGET_HOST)),)
	$(error Could not determine TARGET_HOST from $(BUILDROOT_HOST_DIR). Please set it manually.)
  endif
  SYSROOT := $(BUILDROOT_HOST_DIR)/$(TARGET_HOST)/sysroot
  CC := $(BUILDROOT_HOST_DIR)/bin/$(TARGET_HOST)-gcc
  STRIP := $(BUILDROOT_HOST_DIR)/bin/$(TARGET_HOST)-strip
  CFLAGS := -Wall -Wextra -g --sysroot=$(SYSROOT) -I$(SYSROOT)/usr/include/SDL2 -O3
  LDFLAGS := --sysroot=$(SYSROOT) -lSDL2 -lSDL2_ttf -lSDL2_image -lutil -lm
  BUILD_MODE := cross
endif

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(SRCS)
	@echo "--- Building ($(BUILD_MODE)) for $(TARGET_HOST) ---"
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "--- Build complete: $@ ---"

clean:
	@echo "--- Cleaning up ---"
	rm -f $(TARGET)
