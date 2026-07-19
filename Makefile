
# Makefile for SDL2 Terminal Emulator
# Supports both native and Buildroot cross-compilation

# Source files
SRCS = src/main.c src/app_lifecycle.c src/event_handler.c src/font_manager.c src/config_manager.c \
       src/terminal.c src/dirty_region_tracker.c src/core/terminal_libvterm.c src/rendering/rendering_core.c src/rendering/glyph_cache.c src/rendering/color_manager.c \
       src/input/input_mapper.c src/input/keyboard_handler.c \
       src/osk/osk_core.c src/osk/osk_renderer.c src/osk/osk_parser.c \
       src/utils/error_codes.c
TARGET = vaixterm

# Vendored libvterm (downloaded by CI or manually into vendor/libvterm/)
VTERM_DIR = vendor/libvterm
VTERM_SRCS = $(VTERM_DIR)/src/vterm.c $(VTERM_DIR)/src/keyboard.c \
             $(VTERM_DIR)/src/mouse.c $(VTERM_DIR)/src/parser.c \
             $(VTERM_DIR)/src/pen.c $(VTERM_DIR)/src/screen.c \
             $(VTERM_DIR)/src/state.c $(VTERM_DIR)/src/unicode.c \
             $(VTERM_DIR)/src/encoding.c

# Debug flag (set to 1 to enable debug output, 0 to disable)
DEBUG ?= 0

# Detect OS
UNAME_S := $(shell uname -s)

# Native build defaults
NATIVE_CC = gcc
NATIVE_CFLAGS = -Wall -Wextra -O2 -g \
                -fno-stack-protector -fomit-frame-pointer \
                -fno-ident -fno-unwind-tables \
                -fno-exceptions \
                -fno-common -fno-strict-aliasing \
                -ffunction-sections -fdata-sections \
                -fno-asynchronous-unwind-tables \
                `sdl2-config --cflags` -Isrc -Iinclude -DDEBUG=$(DEBUG) -DNDEBUG=1 \
                -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
                -fvisibility=hidden -fno-stack-check

# libvterm flags: vendored source if present, else system package
ifneq ($(wildcard $(VTERM_DIR)/src/vterm.c),)
  ALL_SRCS = $(SRCS) $(VTERM_SRCS)
  VTERM_CFLAGS = -I$(VTERM_DIR)/include -I$(VTERM_DIR)/src \
                 -Wno-unused-parameter -Wno-missing-field-initializers \
                 -Wno-sign-compare -Wno-implicit-fallthrough -Wno-old-style-declaration
  VTERM_LDFLAGS =
  VTERM_MODE := vendored
else
  ALL_SRCS = $(SRCS)
  VTERM_CFLAGS = $(shell pkg-config --cflags vterm 2>/dev/null || echo)
  VTERM_LDFLAGS = -lvterm
  VTERM_MODE := system
endif

NATIVE_CFLAGS += $(VTERM_CFLAGS)

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
                    $(SDL_LIBS) $(VTERM_LDFLAGS) \
                    -L/opt/homebrew/lib \
                    -L/usr/local/lib
else
    # Linux/other
    NATIVE_CFLAGS += -fdata-sections -ffunction-sections
    NATIVE_LDFLAGS = -flto -Wl,--gc-sections -Wl,--as-needed `sdl2-config --libs` -lSDL2_ttf -lSDL2_image -lm $(VTERM_LDFLAGS) -no-pie
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
           --sysroot=$(SYSROOT) -I$(SYSROOT)/usr/include/SDL2 -Iinclude -DNDEBUG \
           $(VTERM_CFLAGS)
  LDFLAGS := -flto -Wl,--gc-sections -Wl,--as-needed \
             --sysroot=$(SYSROOT) -lSDL2 -lSDL2_ttf -lSDL2_image -lutil -lm $(VTERM_LDFLAGS) -no-pie
  BUILD_MODE := cross
endif

 .PHONY: all clean test
all: $(TARGET)

# Headless tests (no visible SDL window needed).
test: tests/test_osk tests/test_scrollback tests/test_dirty
	./tests/test_osk
	./tests/test_scrollback
	./tests/test_dirty

tests/test_osk: tests/test_osk.c $(SRCS)
	$(CC) $(CFLAGS) -Iinclude -Isrc -Isrc/osk -Isrc/core -Isrc/rendering -Isrc/utils -Isrc/input \
		tests/test_osk.c src/osk/osk_core.c src/osk/osk_parser.c \
		src/input/input_mapper.c src/input/keyboard_handler.c src/utils/error_codes.c \
		-o $@ $(LDFLAGS) -lvterm -lSDL2_ttf

tests/test_scrollback: tests/test_scrollback.c $(SRCS)
	$(CC) $(CFLAGS) -Iinclude -Isrc -Isrc/osk -Isrc/core -Isrc/rendering -Isrc/utils -Isrc/input \
		tests/test_scrollback.c src/terminal.c src/core/terminal_libvterm.c \
		src/rendering/glyph_cache.c src/config_manager.c src/dirty_region_tracker.c \
		src/utils/error_codes.c \
		-o $@ $(LDFLAGS) -lvterm -lSDL2 -lSDL2_ttf -lSDL2_image

tests/test_dirty: tests/test_dirty.c $(SRCS)
	$(CC) $(CFLAGS) -Iinclude -Isrc -Isrc/osk -Isrc/core -Isrc/rendering -Isrc/utils -Isrc/input \
		tests/test_dirty.c src/terminal.c src/core/terminal_libvterm.c \
		src/rendering/glyph_cache.c src/config_manager.c src/dirty_region_tracker.c \
		src/utils/error_codes.c \
		-o $@ $(LDFLAGS) -lvterm -lSDL2 -lSDL2_ttf -lSDL2_image


$(TARGET): $(ALL_SRCS)
	@echo "--- Building ($(BUILD_MODE), libvterm=$(VTERM_MODE)) for $(UNAME_S) ---"
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "--- Build complete: $@ ---"
ifndef KEEP_DEBUG
	@echo "Stripping debug symbols..."
ifeq ($(UNAME_S),Darwin)
	@-strip -x $@
	@-strip -S -x -r $@
else
	@-strip --strip-all --remove-section=.note.gnu.build-id --remove-section=.comment --strip-unneeded $@ 2>/dev/null || true
endif
endif
	@echo "\nFinal executable size:"
	@ls -lh $@
	@echo "\nSection sizes:"
	@size -m $@ 2>/dev/null || size $@

clean:
	@echo "--- Cleaning up ---"
	rm -f $(TARGET)
	rm -rf $(TARGET).dSYM
