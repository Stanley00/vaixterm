# VaixTerm Configuration File Formats

This document provides a comprehensive guide to the formats for VaixTerm's external configuration files. Properly formatted files allow for deep customization of the terminal's appearance and functionality.

**Table of Contents**
1.  On-Screen Keyboard Layouts (`.kb` files)
2.  Key Sets (`.keys` files)
3.  Color Schemes (`.theme` files)

---

## On-Screen Keyboard Layouts (`.kb` files)

`.kb` files define the primary character layouts for the On-Screen Keyboard (OSK). These files allow for custom key arrangements and character mappings based on active modifier keys. They are distinct from `.keys` files, which define sets of special actions or shortcuts.

### File Structure

A `.kb` file consists of sections, each defining a layer of key rows for a specific modifier combination.

*   **Modifier Sections:** Each section begins with a line enclosed in square brackets `[]`, specifying the modifier combination for that layer.
    *   Examples: `[default]`, `[SHIFT]`, `[CTRL]`, `[ALT]`, `[GUI]`, `[SHIFT+CTRL]`, `[CTRL+ALT+GUI]`.
    *   The `[default]` or `[normal]` section is the base layer when no modifiers are active. It is **required**.
    *   Modifier names are case-insensitive and can be combined with `+`. The order of modifiers in the name does not matter (e.g., `[SHIFT+CTRL]` is the same as `[CTRL+SHIFT]`).
*   **Key Definition Rows:** Following a modifier section, each line represents a row of keys. Keys within a row are defined by a sequence of characters or special tokens.

    *   **Literal Characters:** Any character not part of a special token is treated as a literal character to be displayed and sent. UTF-8 characters are fully supported.
    *   **Escaped Characters:** A backslash `\` can be used to escape the next character, allowing literal interpretation of characters that might otherwise be part of a special token (e.g., `\{` to display `{`).

### Special Tokens

Special tokens, enclosed in curly braces `{}`, represent common special keys or OSK modifier toggles.

*   **Input Keys:**
    *   `{ENTER}`: Enter key (`\r`)
    *   `{SPACE}`: Space key (` `)
    *   `{TAB}`: Tab key (`\t`)
    *   `{BS}`: Backspace key (sends `\x7f`)
    *   `{DEL}`: Delete key (sends `\x1b[3~`)
    *   `{ESC}`: Escape key (`\x1b`)
*   **Navigation & Editing Keys:**
    *   `{UP}`, `{DOWN}`, `{LEFT}`, `{RIGHT}`: Arrow keys
    *   `{HOME}`, `{END}`, `{PGUP}`, `{PGDN}`, `{INS}`: Navigation/editing keys
*   **Function Keys:**
    *   `{F1}` through `{F12}`: Function keys
*   **OSK Modifiers:**
    *   `{SHIFT}`: Toggles the OSK's one-shot Shift modifier.
    *   `{CTRL}`: Toggles the OSK's one-shot Ctrl modifier.
    *   `{ALT}`: Toggles the OSK's one-shot Alt modifier.
    *   `{GUI}`: Toggles the OSK's one-shot GUI (Windows/Command) modifier.
*   **Layout Control:**
    *   `{N/A}` or `{DEFAULT}`: This token acts as a placeholder.
        *   If a key position in a specific modifier layer (e.g., `[SHIFT]`) contains `{N/A}`, the OSK will fall back to the key defined at the same position in the `[default]` layer.
        *   If an entire row is just `"{DEFAULT}"`, that entire row will be inherited from the corresponding row in the `[default]` layer.

**Note:** Some tokens have a shorter display name on the OSK button. For example, `{ENTER}` is displayed as `ENT`, `{SPACE}` is `Space`, and `{SHIFT}` is `Shift`.

### Guidelines & Best Practices

*   **Always Define a `[default]` Layer:** A `.kb` file is not valid without a `[default]` (or `[normal]`) section, as it serves as the fallback for all other layers.
*   **Use Fallbacks to Reduce Duplication:** Leverage `{N/A}` and `{DEFAULT}` extensively to avoid redefining keys that are common across layers. This makes your layout files cleaner and easier to maintain.
*   **Maintain Consistent Row Lengths:** While not strictly required, keeping the number of keys in corresponding rows across different layers the same will result in a more visually stable and predictable user experience when switching layers.

### Example `.kb` File

```
[default]
`1234567890-=
qwertyuiop[]\
asdfghjkl;'
zxcvbnm,./
{SHIFT}{SPACE}{ENTER}{BS}
{CTRL}{ALT}{GUI}{ESC}{TAB}
{UP}{DOWN}{LEFT}{RIGHT}{HOME}{END}{PGUP}{PGDN}{INS}{DEL}

[SHIFT]
~!@#$%^&*()_+
QWERTYUIOP{}|
ASDFGHJKL:"
ZXCVBNM<>?
{SHIFT}{SPACE}{ENTER}{BS}

[CTRL]
{CTRL}{ALT}{GUI}{ESC}{TAB}
```

---

## Key Sets (`.keys` files)

`.keys` files define collections of special keys that appear in the "Special" mode of the OSK. These sets act as shortcuts for sending complex strings, executing commands, or controlling the terminal application itself. These sets can be dynamically loaded and unloaded via the OSK's "CONTROL" menu.

### File Structure

Each line in a `.keys` file defines a single key using the format:
`DISPLAY_NAME:VALUE[:EXTRA_DATA]`

*   **`DISPLAY_NAME`**: The text string that will be displayed on the key button in the UI (e.g., `Enter`, `Font+`, `+Bash`). This can contain spaces or special characters. To include a literal colon `:`, escape it with a backslash `\:`.
*   **`VALUE`**: Specifies the action the key performs. This can be one of several types:

    *   **1. Literal String**
        To send an exact string to the terminal, enclose it in double quotes.
        *   Example: `"ls -la --color=auto"`

    *   **2. Special Key Name**
        A case-insensitive name for a common key, which sends the corresponding key event. The parser uses `SDL_GetKeyFromName`, so any standard SDL key name is valid.
        *   Common aliases: `ESC`, `ENTER`, `BS` or `BACKSPACE`, `DEL` or `DELETE`, `PGUP`, `PAGEUP`, `PGDN`, `PAGEDOWN`, `TAB`.
        *   Other valid names: `A`, `F1`, `Left`, `Right`, `Up`, `Down`, `Home`, `End`, `Insert`, `Space`.

    *   **3. Internal Command**
        A predefined command that controls the vaixTerm application itself. These do not send input to the terminal.
        *   `CMD_FONT_INC`: Increase font size.
        *   `CMD_FONT_DEC`: Decrease font size.
        *   `CMD_CURSOR_TOGGLE_VISIBILITY`: Toggle cursor visibility.
        *   `CMD_CURSOR_TOGGLE_BLINK`: Toggle cursor blinking.
        *   `CMD_CURSOR_CYCLE_STYLE`: Cycle through cursor styles (block, underline, bar).
        *   `CMD_TERMINAL_RESET`: Reset the terminal state.
        *   `CMD_TERMINAL_CLEAR`: Clear the visible terminal screen.
        *   `CMD_OSK_TOGGLE_POSITION`: Toggles the OSK auto-positioning logic. It switches between placing the OSK on the opposite half of the screen from the cursor (default), and placing it on the same half.

    *   **4. Dynamic Loading**
        These values are used to dynamically load or unload other `.keys` files from the OSK.
        *   `LOAD_FILE`: The `EXTRA_DATA` field must contain the path to the `.keys` file to load.
        *   `UNLOAD_FILE`: The `EXTRA_DATA` field must contain the *name* of the key set to unload (typically the filename without the `.keys` extension).

*   **`EXTRA_DATA` (Optional)**: The meaning of this field depends on the `VALUE`.
    *   If `VALUE` is a **Special Key Name**, this field can be a comma-separated list of modifiers to apply: `ctrl`, `alt`, `shift`, `gui` (or `win`, `super`).
    *   If `VALUE` is `LOAD_FILE`, this field is the **file path**.
    *   If `VALUE` is `UNLOAD_FILE`, this field is the **set name**.

### Guidelines & Best Practices

*   **Create Modular Sets:** Break down common workflows into separate `.keys` files (e.g., `git.keys`, `docker.keys`, `ssh.keys`). This makes your configuration clean and allows you to load only the tools you need.
*   **Leverage Dynamic Loading:** Create a primary `.keys` file that contains `LOAD_FILE` entries for your other modular sets. This allows you to build a menu system within the OSK.

### Example `.keys` File

```bash
# --- Common Commands ---
# Send a literal string, including a newline to execute it.
List Files:"ls -l --color=auto"
# Send a string without a newline, allowing further input.
Change Dir:"cd "
# Send a simple command.
Exit Shell:"exit"

# --- Special Keys with Modifiers ---
# Send Ctrl+C sequence.
Interrupt:C:ctrl
# Send Shift+Tab.
Back-Tab:Tab:shift

# --- Internal App Commands ---
Font+:CMD_FONT_INC
Font-:CMD_FONT_DEC
Toggle Cursor:CMD_CURSOR_TOGGLE_VISIBILITY

# --- Dynamic Loading Example (in a file named 'main.keys') ---
# This key will load the 'git_shortcuts.keys' file.
Load Git:LOAD_FILE:./keys/git_shortcuts.keys
# This key will unload the 'docker' key set.
Unload Docker:UNLOAD_FILE:docker
```

---

## Color Schemes (`.theme` files)

`.theme` files define the color palette for the terminal, including default foreground/background colors and the 16 ANSI colors.

### File Structure

A `.theme` file is a simple key-value store. Each line defines a color using a `NAME=HEX_COLOR` format. Lines starting with `#` are treated as comments and are ignored.

*   **`NAME`**: The identifier for the color.
    *   `default_fg`: Default text color.
    *   `default_bg`: Default background color.
    *   `color0` to `color15`: The 16 ANSI colors.
        *   `color0` to `color7` are the standard 8 colors (Black, Red, Green, Yellow, Blue, Magenta, Cyan, White).
        *   `color8` to `color15` are their "bright" or "high-intensity" counterparts.
*   **`HEX_COLOR`**: A 6-digit hexadecimal color code in `#RRGGBB` format (e.g., `#d8d8d8`). This format specifies the Red, Green, and Blue components. The alpha (opacity) component is optional with default value to fully opaque (255) internally.

### Guidelines & Best Practices

*   **Partial Themes are Allowed:** You do not need to specify all 18 colors. Any color not defined in the `.theme` file will fall back to VaixTerm's internal default value. This allows you to create themes that only change a few specific colors.
*   **Use Comments:** Use `#` to add comments to your theme file, making it easier to remember which color is which.

### Example `.theme` File

```
# Tango Colorscheme
background = #2e3436
foreground = #c0c0c0  # Modified for better contrast
color0  = #2e3436
color1  = #cc0000
color2  = #4e9a06
color3  = #c4a000
color4  = #3465a4
color5  = #75507b
color6  = #06989a
color7  = #d3d7cf
color8  = #555753
color9  = #ef2929
color10 = #8ae234
color11 = #fce94f
color12 = #729fcf
color13 = #ad7fa8
color14 = #34e2e2
color15 = #eeeeec
```
