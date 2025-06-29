# VaixTerm: A Robust SDL2 Terminal for Handheld & Embedded Systems

VaixTerm is a lightweight, high-performance terminal emulator built using SDL2, primarily designed for **handheld gaming devices** and embedded systems. It provides a full-featured command-line interface optimized for environments where traditional keyboard input is limited, leveraging game controller input and a highly customizable on-screen keyboard.

## Main Features

VaixTerm combines the power of a standard terminal with features tailored for controller-based use:

- **Full Terminal Functionality:** Enjoy a complete command-line experience with support for ANSI/VT100 escape codes, 256-color and True Color palettes, UTF-8, and a scrollback buffer.
- **Controller-First Navigation:** All terminal functions, including scrolling and special key input, are mapped to a standard game controller.
- **Customizable On-Screen Keyboard:** A powerful OSK allows for text input and execution of complex commands. Its layout is fully configurable, and you can create dynamic sets of keys for your favorite shortcuts.
- **Theming and Customization:** Change the look and feel of your terminal with custom color schemes, fonts, and background images.

## Key Features & Design Principles

*   **Optimized for Handheld & Controller Input:** Engineered with a primary focus on game controller navigation and input, providing a natural and efficient interface for devices without physical keyboards.
*   **Extensible On-Screen Keyboard (OSK):** Features a highly configurable OSK with custom character layouts (`.kb` files) and dynamic key sets (`.keys` files) for shortcuts and internal commands, adapting to diverse workflows.
*   **Lightweight SDL2 Core:** Built on SDL2 for efficient rendering and minimal resource consumption, making it suitable for embedded and resource-constrained environments.
*   **Comprehensive Terminal Emulation:** Supports standard ANSI/VT100 escape codes, 256-color, True Color, and robust UTF-8 character rendering, including custom drawing for box-drawing and Braille characters.
*   **File-Based Configuration:** Appearance and behavior are fully customizable via external `.theme` (color scheme), `.kb` (OSK layout), and `.keys` (key set) files, allowing for easy sharing and management of configurations.

## Getting Started

Compile VaixTerm effortlessly with the provided `Makefile`.

### VaixTerm Features

For a full list of command-line features and options, simply run `vaixterm --help`. The output is as follows:

```
vaixterm - A simple, modern terminal emulator mainly for game handheld.

Usage: ./vaixterm [options]

Options:
  -w, --width <pixels>       Set window width (default: 640)
  -h, --height <pixels>      Set window height (default: 480)
  -f, --font <path>          Set font path (default: res/Martian.ttf)
  -s, --size <points>        Set font size (default: 12)
  -l, --scrollback <lines>   Set scrollback lines (default: 1000)
  -e, --exec <command>       Execute command instead of default shell.
  -b, --background <path>    Set background image (optional).
  -cs, --colorscheme <path>  Set colorscheme (optional).
  --fps <value>              Set framerate cap (default: 30 fps).
  --read-only                Run in read-only mode (input disabled).
  --no-credit                Start shell directly, skip credits.
  --force-full-render        Force a full re-render on every frame.
  --key-set [-|+]<path>      Add key set ('-': available, '+': load).
  --osk-layout <path>        Use a custom OSK layout file.
```