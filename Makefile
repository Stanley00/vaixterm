
# Makefile for SDL2 Terminal Emulator
# Supports both native and Buildroot cross-compilation

# Source files
SRCS = src/main.c src/input.c src/osk.c src/rendering.c src/terminal.c src/manualfont.c \
       src/app_lifecycle.c src/event_handler.c src/font_manager.c src/config_manager.c \
       src/error_handler.c src/cache_manager.c src/dirty_region_tracker.c src/memory_pool.c
TARGET = vaixterm

# Debug flag (set to 1 to enable debug output, 0 to disable)
DEBUG ?= 0

# Detect OS
UNAME_S := $(shell uname -s)

# Native build defaults
NATIVE_CC = gcc
NATIVE_CFLAGS = -Wall -Wextra -O2 \
                -fno-stack-protector -fomit-frame-pointer \
                -fno-ident -fno-unwind-tables \
                -fno-exceptions -fno-rtti \
                -fno-common -fno-strict-aliasing \
                -ffunction-sections -fdata-sections \
                -fno-asynchronous-unwind-tables \
                `sdl2-config --cflags` -Isrc -Iinclude -DDEBUG=$(DEBUG) -DNDEBUG=1 \
                -fvisibility=hidden -fno-stack-check

# OS-specific flags
ifeq ($(UNAME_S),Darwin)
    # macOS
    SDL_CFLAGS := $(shell pkg-config sdl2 --cflags)
    SDL_LIBS := $(shell pkg-config sdl2 --libs) -lSDL2_ttf -lSDL2_image -lm
    
    NATIVE_CFLAGS += $(SDL_CFLAGS)
    NATIVE_LDFLAGS = -Wl,-dead_strip \
                    -Wl,-dead_strip_dylibs \
                    -Wl,-x \
                    -Wl,-S \
                    $(SDL_LIBS) \
                    -L/opt/homebrew/lib \
                    -L/usr/local/lib
else
    # Linux/other
    NATIVE_CFLAGS += -fdata-sections -ffunction-sections
    NATIVE_LDFLAGS = -flto -Wl,--gc-sections -Wl,--as-needed -Wl,-s `sdl2-config --libs` -lSDL2_ttf -lSDL2_image -lm -no-pie
endif

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
  CFLAGS := -Wall -Wextra -O2 -flto -fdata-sections -ffunction-sections \
           -fno-stack-protector -fomit-frame-pointer -fno-ident \
           -fno-unwind-tables -fno-asynchronous-unwind-tables \
           --sysroot=$(SYSROOT) -I$(SYSROOT)/usr/include/SDL2 -Iinclude -DNDEBUG
  LDFLAGS := -flto -Wl,--gc-sections -Wl,--as-needed -Wl,-s \
            --sysroot=$(SYSROOT) -lSDL2 -lSDL2_ttf -lSDL2_image -lutil -lm -no-pie
  BUILD_MODE := cross
endif

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(SRCS)
	@echo "--- Building ($(BUILD_MODE)) for $(UNAME_S) ---"
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "--- Build complete: $@ ---"
	@echo "Stripping debug symbols..."
ifeq ($(UNAME_S),Darwin)
	@-strip -x $@
	@-strip -S -x -r $@
else
	@-$(STRIP) --strip-all --remove-section=.note.gnu.build-id --remove-section=.comment --strip-unneeded $@
endif
	@echo "\nFinal executable size:"
	@ls -lh $@
	@echo "\nSection sizes:"
	@size -m $@ 2>/dev/null || size $@

clean:
	@echo "--- Cleaning up ---"
	rm -f $(TARGET)
	rm -rf $(TARGET).dSYM
