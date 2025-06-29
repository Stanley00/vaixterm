#ifndef TERMINAL_STATE_H
#define TERMINAL_STATE_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_gamecontroller.h>
#include <stdbool.h>
#include <stdint.h> // For uint32_t, uint64_t

// --- Constants ---
#define CSI_MAX_PARAMS 16
#define RESPONSE_BUFFER_SIZE 64
#define CURSOR_BLINK_INTERVAL_MS 500 // Milliseconds for cursor blink toggle

#define MOUSE_WHEEL_SCROLL_AMOUNT 3

// Key sequences for handle_key_down
#define KEY_SEQ_UP_NORMAL    "\x1b[A"
#define KEY_SEQ_UP_APP       "\x1bOA"
#define KEY_SEQ_DOWN_NORMAL  "\x1b[B"
#define KEY_SEQ_DOWN_APP     "\x1bOB"
#define KEY_SEQ_RIGHT_NORMAL "\x1b[C"
#define KEY_SEQ_RIGHT_APP    "\x1bOC"
#define KEY_SEQ_LEFT_NORMAL  "\x1b[D"
#define KEY_SEQ_LEFT_APP     "\x1bOD"
#define KEY_SEQ_HOME_NORMAL  "\x1b[1~"
#define KEY_SEQ_HOME_APP     "\x1bOH"
#define KEY_SEQ_END_NORMAL   "\x1b[4~"
#define KEY_SEQ_END_APP      "\x1bOF"
#define KEY_SEQ_PGUP_NORMAL  "\x1b[5~"
#define KEY_SEQ_PGDN_NORMAL  "\x1b[6~"

// --- Data Structures ---

typedef struct {
    uint32_t character; // Unicode codepoint. Can store multi-byte chars.
    SDL_Color fg; // Foreground color
    SDL_Color bg; // Background color
    unsigned char attributes; // Bitfield for text attributes
} Glyph;

// Attribute flags for Glyph.attributes
#define ATTR_BOLD       (1 << 0)
#define ATTR_ITALIC     (1 << 1)
#define ATTR_UNDERLINE  (1 << 2)
#define ATTR_INVERSE    (1 << 3)
#define ATTR_BLINK      (1 << 4)

// --- Glyph Cache ---
#define GLYPH_CACHE_SIZE 4096 // Number of glyphs to cache

typedef struct {
    uint64_t key;
    SDL_Texture* texture;
    int w, h;
} GlyphCacheEntry;

typedef struct {
    GlyphCacheEntry entries[GLYPH_CACHE_SIZE];
} GlyphCache;

// --- OSK Key Cache ---
#define OSK_KEY_CACHE_SIZE 512 // Ample space for all key states

typedef enum {
    OSK_KEY_STATE_NORMAL,
    OSK_KEY_STATE_SELECTED,
    OSK_KEY_STATE_TOGGLED,
    OSK_KEY_STATE_SET_NAME
} OSKKeyState;

// --- OSK Position Mode Enum ---
typedef enum {
    OSK_POSITION_OPPOSITE, // Position automatically on the opposite half of the screen from the cursor
    OSK_POSITION_SAME      // Position automatically on the same half of the screen as the cursor
} OSKPositionMode;

// --- Cursor Style Enum ---
typedef enum {
    CURSOR_STYLE_BLOCK,
    CURSOR_STYLE_UNDERLINE,
    CURSOR_STYLE_BAR
} CursorStyle;

typedef struct {
    uint64_t key;
    SDL_Texture* texture;
    int w, h;
} OSKKeyCacheEntry;

typedef struct {
    OSKKeyCacheEntry entries[OSK_KEY_CACHE_SIZE];
} OSKKeyCache;

typedef struct {
    int cols;
    int rows;
    int cursor_x;
    int cursor_y;
    SDL_Color current_fg;
    SDL_Color current_bg;

    // Color palettes
    SDL_Color colors[16];
    SDL_Color xterm_colors[256];
    SDL_Color default_fg;
    SDL_Color cursor_color;
    SDL_Color default_bg;

    unsigned char current_attributes; // Current attributes for new characters
    Glyph *grid;
    Glyph *alt_grid; // The alternate screen buffer

    // Scrollback additions
    int scrollback;      // Number of lines in scrollback buffer
    int total_lines;     // rows + scrollback
    int top_line;        // Index of the top-most line of the logical screen in the circular buffer
    int view_offset;     // How many lines we are scrolled up from the bottom. 0 = not scrolled.
    int history_size;    // Number of lines currently in the scrollback history

    enum {
        STATE_NORMAL,
        STATE_ESCAPE,
        STATE_CSI,
        STATE_OSC,
        STATE_DCS
    } parse_state;
    int csi_params[CSI_MAX_PARAMS];
    int csi_param_count;
    char osc_buffer[256];
    int osc_len;
    char csi_private_marker; // To store '?' for DEC private modes
    char csi_intermediate_chars[4]; // For intermediate CSI bytes like ' '
    int csi_intermediate_count;

    // Saved cursor position
    int saved_cursor_x;
    int saved_cursor_y;

    // Scrolling region
    int scroll_top;    // 1-based top line of the scroll region
    int scroll_bottom; // 1-based bottom line of the scroll region

    // Terminal modes
    CursorStyle cursor_style;
    bool cursor_style_blinking;
    bool application_cursor_keys_mode; // DECCKM: Cursor Key Mode
    bool cursor_visible;               // DECTCEM: Text Cursor Enable Mode (CSI ? 25 h/l)
    bool application_keypad_mode;      // DECNKM: Numeric Keypad Mode
    bool alt_screen_active;            // True if alternate screen is active
    bool autowrap_mode;                // DECAWM: Autowrap mode (CSI ? 7 h/l)
    bool insert_mode;                  // IRM: Insert/Replace Mode (CSI 4 h/l)
    bool origin_mode;                  // DECOM: Origin Mode (CSI ? 6 h/l)

    // VT100 Character Set Support
    // 'B' = US ASCII, '0' = DEC Special Graphics
    char charsets[2]; // G0, G1 character sets.
    int active_charset; // 0 for G0, 1 for G1

    // UTF-8 decoding state
    uint32_t utf8_codepoint;
    int utf8_bytes_to_read;

    // For responding to DSR (Device Status Report)
    char response_buffer[RESPONSE_BUFFER_SIZE]; // Buffer for responses like cursor position report
    int response_len;

    // Saved cursor position for normal screen
    int normal_saved_cursor_x;
    int normal_saved_cursor_y;

    // Performance
    GlyphCache* glyph_cache;

    // Blinking cursor state
    bool cursor_blink_on;
    Uint32 last_blink_toggle_time;

    // Dirty line tracking for render optimization
    bool* dirty_lines;

    // Double buffering
    SDL_Texture* screen_texture;
    bool full_redraw_needed;

    // Background image
    SDL_Texture* background_texture;
} Terminal;

// --- Main Configuration Struct ---
typedef struct {
    int win_w;
    int win_h;
    char* font_path;
    int font_size;
    char* custom_command;
    int scrollback_lines;
    bool force_full_render;
    char* background_image_path;
    char* colorscheme_path;
    int target_fps;
    bool read_only;
    bool no_credit;
    char* osk_layout_path;      // Path to a custom OSK character layout file

    // New: For handling --key-set arguments
    struct KeySetArg {
        char* path;
        bool load_at_startup;
    } *key_sets; // Renamed from key_set_args
    int num_key_sets; // Renamed from num_key_set_args
} Config;

// --- On-Screen Keyboard ---
typedef enum {
    OSK_MODE_CHARS,
    OSK_MODE_SPECIAL
} OSKMode;

// Forward declaration for SpecialKey
typedef struct SpecialKey SpecialKey;

// Struct to manage special key sets
typedef struct {
    char* name;          // Display name of the set (e.g., "ACTION", "NAV", "bash")
    SpecialKey* keys; // Array of SpecialKey structs
    int length;          // Number of keys in this set
    bool is_dynamic;     // True if this set was loaded from a file and needs to be freed
    char* file_path;     // Path to the .keys file if loaded dynamically
} SpecialKeySet;

// Modifier bitmasks for OSK character layers
#define OSK_MOD_NONE  0
#define OSK_MOD_SHIFT (1 << 0)
#define OSK_MOD_CTRL  (1 << 1)
#define OSK_MOD_ALT   (1 << 2)
#define OSK_MOD_GUI   (1 << 3)

typedef struct {
    bool active;
    OSKMode mode;
    int set_idx; // Index of the current character set (row)
    OSKPositionMode position_mode; // Control for OSK positioning
    int char_idx; // Index of the selected character within the current set (column)

    // Character layouts for different modifier combinations
    // Indexed by a bitmask: [GUI][ALT][CTRL][SHIFT]
    // Each layer is an array of SpecialKeySet structs (one per row).
    SpecialKeySet* char_sets_by_modifier[16];
    int num_char_rows_by_modifier[16];
    SDL_GameController* controller;
    SDL_Joystick* joystick; // Fallback for unmapped controllers
    // For special key mode modifiers (these are for the OSK's internal state, not held controller buttons)
    bool mod_ctrl;
    bool mod_alt;
    bool mod_shift;
    bool mod_gui;
    // For held controller modifiers (these reflect physical button presses)
    bool held_ctrl;
    bool held_shift;
    bool held_alt;
    bool held_gui;
    bool held_back;  // For exit combo
    bool held_start; // For exit combo
    OSKKeyCache* key_cache;
    // Pointers to all available special key sets (static and dynamic)
    SpecialKeySet* all_special_sets;
    int num_total_special_sets;
    // OSK render optimization
    int cached_key_width;
    int cached_set_idx;
    OSKMode cached_mode;
    int cached_mod_mask; // New: Store current modifier mask for char mode caching
    bool show_special_set_name; // For special mode: show set name only on switch

    // For managing dynamic key sets from key-set.list
    SpecialKeySet* available_dynamic_key_sets; // List of all key sets defined in key-set.list
    int num_available_dynamic_key_sets;
    char** loaded_key_set_names; // Names of currently loaded dynamic key sets
    int num_loaded_key_sets;
} OnScreenKeyboard;

// --- Special Keys for OSK ---
typedef enum {
    SK_SEQUENCE,
    SK_STRING,      // For sending a literal string
    SK_MOD_CTRL,
    SK_MOD_ALT,
    SK_MOD_SHIFT,
    SK_MOD_GUI,
    SK_INTERNAL_CMD, // For terminal-internal commands
    SK_LOAD_FILE,    // Load a key set from a file (sequence field holds path)
    SK_UNLOAD_FILE   // Unload a key set by name (sequence field holds name)
} SpecialKeyType;

typedef enum {
    CMD_NONE,
    CMD_FONT_INC,
    CMD_FONT_DEC,
    CMD_CURSOR_TOGGLE_VISIBILITY,
    CMD_CURSOR_TOGGLE_BLINK,
    CMD_CURSOR_CYCLE_STYLE,
    CMD_TERMINAL_RESET,
    CMD_TERMINAL_CLEAR,
    CMD_OSK_TOGGLE_POSITION
} InternalCommand;

typedef struct SpecialKey {
    char* display_name;
    SpecialKeyType type;
    char* sequence; // For SK_STRING, SK_SEQUENCE, SK_LOAD_FILE (path), SK_UNLOAD_FILE (name)
    SDL_Keycode keycode;
    SDL_Keymod mod;
    InternalCommand command; // Only used for SK_INTERNAL_CMD type
} SpecialKey;

// Special Key Sets for OSK (declared extern, defined in osk.c)
extern SpecialKeySet osk_built_in_special_sets[]; // No longer const
extern const int osk_num_built_in_special_sets;

// --- Input Actions ---
// An abstract representation of user actions, independent of input device.
typedef enum {
    ACTION_NONE,

    // D-Pad / Stick
    ACTION_UP,
    ACTION_DOWN,
    ACTION_LEFT,
    ACTION_RIGHT,

    // Face Buttons (mapped to their primary function)
    ACTION_SELECT,      // A button: Selects/types in OSK
    ACTION_BACK,        // B button: Backspace/Cancel
    ACTION_SPACE,       // Y button: Inserts a space
    ACTION_TAB,         // Controller 'Back' button or equivalent: Inserts a tab

    // Scrolling & OSK Actions
    ACTION_SCROLL_UP,       // L-Shoulder: Scrolls up terminal view
    ACTION_SCROLL_DOWN,     // R-Shoulder: Scrolls down terminal view

    // Center Buttons
    ACTION_TOGGLE_OSK,  // F12 or Controller X button
    ACTION_ENTER,       // Start button
    // ACTION_EXIT,        // Guide button: Quit application (Now handled by combo)
} TerminalAction;

// --- Input Mapping Configuration ---
// This section defines how physical device inputs are mapped to abstract TerminalActions.

// 1. Game Controller Mapping (for devices with known layouts, e.g., Xbox/PlayStation)
typedef struct {
    SDL_GameControllerButton button;
    TerminalAction action;
} ControllerButtonMapping;

// 2. Keyboard Mapping (for non-character keys that trigger actions)
typedef struct {
    SDL_Keycode sym;
    TerminalAction action;
} KeyMapping;

// --- Button Repeat Handling ---
typedef struct {
    bool is_held;
    TerminalAction action;
    Uint32 next_repeat_time;
} ButtonRepeatState;

#endif // TERMINAL_STATE_H
