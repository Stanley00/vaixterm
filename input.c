/**
 * @file input.c
 * @brief Handles all user input, including keyboard, game controller, and On-Screen Keyboard (OSK) actions.
 *
 * This file is responsible for mapping raw SDL input events to higher-level TerminalActions
 * and then processing those actions. It also manages the state of the On-Screen Keyboard,
 * translating OSK key selections into appropriate terminal input sequences or internal commands.
 */
#include "input.h"
#include "terminal_state.h"
#include "osk.h" // For OSK_MOD_ constants
#include "config.h" // For controller button mappings

#include <SDL.h>
#include <string.h>
#include <stdio.h> // For debugging printf
#include <stdlib.h> // For malloc, realloc, free
#include <ctype.h> // For isalpha, tolower, toupper
#include <unistd.h> // For write

// --- Forward Declarations for static functions ---
static InternalCommand process_osk_chars_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int pty_fd);
static InternalCommand process_osk_special_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int pty_fd);
static void execute_macro(int pty_fd, const char* macro_string, const Terminal* term, OnScreenKeyboard* osk, bool* ui_updated);
static SDL_Keymod get_combined_modifiers(const OnScreenKeyboard* osk);
static SDL_Keymod get_effective_send_modifiers(const OnScreenKeyboard* osk);
static void clear_one_shot_modifiers(OnScreenKeyboard* osk, bool* needs_render_ptr);
static InternalCommand osk_handle_key_selection(const SpecialKey* key, Terminal* term, OnScreenKeyboard* osk, int pty_fd, bool* ui_updated);


// --- UTF-8 String Helpers ---

/**
 * @brief Counts the number of characters in a UTF-8 encoded string.
 * @param s The null-terminated UTF-8 string.
 * @return The number of characters.
 */
int utf8_strlen(const char* s)
{
    int len = 0;
    if (!s) {
        return 0;
    }
    while (*s) {
        if ((*s & 0xC0) != 0x80) { // It's a start byte or a single-byte char
            len++;
        }
        s++;
    }
    return len;
}

/**
 * @brief Gets the byte offset of the Nth character in a UTF-8 string.
 * @param s The null-terminated UTF-8 string.
 * @param char_index The character index to find.
 * @return The byte offset of the character, or the offset of the null terminator if out of bounds.
 */
int utf8_offset_for_char_index(const char* s, int char_index)
{
    int current_char = 0;
    const char* start = s;
    if (!s) {
        return 0;
    }
    while (*s) {
        if (current_char == char_index) {
            return (int)(s - start);
        }
        if ((*s & 0xC0) != 0x80) {
            current_char++;
        }
        s++;
    }
    return (int)(s - start); // Return end of string if index is out of bounds
}

/**
 * @brief Gets the byte length of a UTF-8 character at a given position.
 * @param s A pointer to the start of the UTF-8 character.
 * @return The number of bytes in the character.
 */
int utf8_char_len(const char* s)
{
    if (!s) {
        return 0;
    }
    unsigned char c = *s;
    if (c < 0x80) {
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        return 4;
    }
    return 1; // Invalid, treat as single byte
}

// --- Input Mapping Definitions ---

// Game Controller Button Mappings
const ControllerButtonMapping controller_button_map[] = {
    {ACTION_BUTTON_UP, ACTION_UP},
    {ACTION_BUTTON_DOWN, ACTION_DOWN},
    {ACTION_BUTTON_LEFT, ACTION_LEFT},
    {ACTION_BUTTON_RIGHT, ACTION_RIGHT},
    {ACTION_BUTTON_SELECT, ACTION_SELECT},
    {ACTION_BUTTON_BACK, ACTION_BACK},
    {ACTION_BUTTON_TOGGLE_OSK, ACTION_TOGGLE_OSK},
    {ACTION_BUTTON_SPACE, ACTION_SPACE},
    {ACTION_BUTTON_TAB, ACTION_TAB},
    {ACTION_BUTTON_ENTER, ACTION_ENTER},
    {ACTION_BUTTON_SCROLL_UP, ACTION_SCROLL_UP},
    {ACTION_BUTTON_SCROLL_DOWN, ACTION_SCROLL_DOWN},
    // Add more mappings as needed
};
const int num_controller_button_mappings = sizeof(controller_button_map) / sizeof(controller_button_map[0]);

// Keyboard Mappings for special actions (not character input)
const KeyMapping key_map[] = {
    {SDLK_F12, ACTION_TOGGLE_OSK},
    {SDLK_ESCAPE, ACTION_BACK}, // Map ESC to back for consistency with OSK
    {SDLK_RETURN, ACTION_ENTER},
    {SDLK_KP_ENTER, ACTION_ENTER},
    {SDLK_BACKSPACE, ACTION_BACK},
    {SDLK_TAB, ACTION_TAB},
    {SDLK_PAGEUP, ACTION_SCROLL_UP},
    {SDLK_PAGEDOWN, ACTION_SCROLL_DOWN},
    // Add more keyboard mappings for actions
};
const int num_key_mappings = sizeof(key_map) / sizeof(key_map[0]);

/**
 * @brief Maps an SDL_GameControllerButton to a TerminalAction.
 * @param button The SDL_GameControllerButton to map.
 * @return The corresponding TerminalAction, or ACTION_NONE if no mapping exists.
 */
TerminalAction map_cbutton_to_action(SDL_GameControllerButton button)
{
    for (int i = 0; i < num_controller_button_mappings; ++i) {
        if (controller_button_map[i].button == button) {
            return controller_button_map[i].action;
        }
    }
    return ACTION_NONE;
}

/**
 * @brief Maps an SDL_KeyboardEvent to a TerminalAction.
 * @param key A pointer to the SDL_KeyboardEvent.
 * @return The corresponding TerminalAction, or ACTION_NONE if no mapping exists.
 */
TerminalAction map_keyboard_to_action(const SDL_KeyboardEvent* key)
{
    for (int i = 0; i < num_key_mappings; ++i) {
        if (key_map[i].sym == key->keysym.sym) {
            return key_map[i].action;
        }
    }
    return ACTION_NONE;
}

/**
 * @brief Sends a key event (e.g., arrow keys, function keys, or modified characters) to the PTY.
 * This function handles the conversion of SDL_Keycode and SDL_Keymod
 * into appropriate ANSI escape sequences or single characters.
 * @param sym The SDL_Keycode of the pressed key.
 * @param mod The SDL_Keymod (modifier state) of the pressed key.
 * @param term The terminal state, used for checking modes like application cursor keys.
 */
void send_key_event(int pty_fd, SDL_Keycode sym, SDL_Keymod mod, const Terminal* term)
{
    bool ctrl_pressed = (mod & KMOD_CTRL) != 0;
    bool alt_pressed = (mod & KMOD_ALT) != 0;
    bool shift_pressed = (mod & KMOD_SHIFT) != 0;

    // --- Priority 1: Ctrl Key Combinations ---
    if (ctrl_pressed) {
        // Handle Ctrl + a-z -> 0x01-0x1A
        if (sym >= SDLK_a && sym <= SDLK_z) {
            char c = (char)(sym - SDLK_a + 1);
            write(pty_fd, &c, 1);
            return;
        }
        // Handle Ctrl + Space -> NUL
        if (sym == SDLK_SPACE) {
            write(pty_fd, "\0", 1);
            return;
        }

        // Data-driven map for other Ctrl combinations
        const struct {
            SDL_Keycode k;
            const char* s;
        } map[] = {
            {SDLK_c, "\x03"}, {SDLK_d, "\x04"}, {SDLK_z, "\x1a"}, {SDLK_l, "\x0c"},
            {SDLK_u, "\x15"}, {SDLK_k, "\x0b"}, {SDLK_w, "\x17"}, {SDLK_a, "\x01"},
            {SDLK_e, "\x05"}, {SDLK_LEFT, "\x1b[1;5D"}, {SDLK_RIGHT, "\x1b[1;5C"},
            {SDLK_UP, "\x1b[1;5A"}, {SDLK_DOWN, "\x1b[1;5B"}
        };
        for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); ++i) {
            if (map[i].k == sym) {
                write(pty_fd, map[i].s, strlen(map[i].s));
                return;
            }
        }
    }

    // --- Priority 2: Alt Key Combinations (Meta key) ---
    if (alt_pressed) {
        // Handle Alt + printable char -> ESC + char
        if ((sym >= SDLK_SPACE && sym <= SDLK_z) || (sym >= SDLK_0 && sym <= SDLK_9)) {
            char c = (char)sym;
            if (shift_pressed && isalpha(c)) {
                c = (char)toupper(c);
            }
            char seq[] = {'\x1b', c};
            write(pty_fd, seq, 2);
            return;
        }
        // Data-driven map for other Alt combinations
        const struct {
            SDL_Keycode k;
            const char* s;
        } map[] = {
            {SDLK_BACKSPACE, "\x1b\x7f"}, {SDLK_f, "\x1b" "f"}, {SDLK_b, "\x1b" "b"}
        };
        for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); ++i) {
            if (map[i].k == sym) {
                write(pty_fd, map[i].s, strlen(map[i].s));
                return;
            }
        }
    }

    // --- Priority 3: Application-Mode Keys (Cursor, Home/End) ---
    if (term && term->application_cursor_keys_mode) {
        const struct {
            SDL_Keycode k;
            const char* s;
        } map[] = {
            {SDLK_UP,    KEY_SEQ_UP_APP},    {SDLK_DOWN,  KEY_SEQ_DOWN_APP},
            {SDLK_RIGHT, KEY_SEQ_RIGHT_APP}, {SDLK_LEFT,  KEY_SEQ_LEFT_APP},
            {SDLK_HOME,  KEY_SEQ_HOME_APP},  {SDLK_END,   KEY_SEQ_END_APP}
        };
        for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); ++i) {
            if (map[i].k == sym) {
                write(pty_fd, map[i].s, strlen(map[i].s));
                return;
            }
        }
    }

    // --- Priority 4: Standard Special Keys ---
    const struct {
        SDL_Keycode k;
        const char* s;
    } map[] = {
        {SDLK_RETURN,      "\r"},       {SDLK_KP_ENTER,    "\r"},
        {SDLK_BACKSPACE,   "\x7f"},     {SDLK_TAB,         "\t"},
        {SDLK_ESCAPE,      "\x1b"},     {SDLK_SPACE,       " "},
        {SDLK_PAGEUP,      KEY_SEQ_PGUP_NORMAL}, {SDLK_PAGEDOWN, KEY_SEQ_PGDN_NORMAL},
        {SDLK_UP,          KEY_SEQ_UP_NORMAL},   {SDLK_DOWN,   KEY_SEQ_DOWN_NORMAL},
        {SDLK_RIGHT,       KEY_SEQ_RIGHT_NORMAL},{SDLK_LEFT,   KEY_SEQ_LEFT_NORMAL},
        {SDLK_HOME,        KEY_SEQ_HOME_NORMAL}, {SDLK_END,    KEY_SEQ_END_NORMAL},
        {SDLK_INSERT,      "\x1b[2~"},  {SDLK_DELETE,      "\x1b[3~"},
        {SDLK_F1,          "\x1bOP"},   {SDLK_F2,          "\x1bOQ"},
        {SDLK_F3,          "\x1bOR"},   {SDLK_F4,          "\x1bOS"},
        {SDLK_F5,          "\x1b[15~"}, {SDLK_F6,          "\x1b[17~"},
        {SDLK_F7,          "\x1b[18~"}, {SDLK_F8,          "\x1b[19~"},
        {SDLK_F9,          "\x1b[20~"}, {SDLK_F10,         "\x1b[21~"},
        {SDLK_F11,         "\x1b[23~"}, {SDLK_F12,         "\x1b[24~"},
        {SDLK_PRINTSCREEN, "\x1b[29~"}, {SDLK_SCROLLLOCK,  "\x1b[31~"},
        {SDLK_PAUSE,       "\x1b[32~"},
    };
    for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); ++i) {
        if (map[i].k == sym) {
            write(pty_fd, map[i].s, strlen(map[i].s));
            return;
        }
    }

    // --- Priority 5: Printable characters (if not handled by SDL_TEXTINPUT) ---
    // This is a fallback for when SDL_TEXTINPUT is not active or for keys that
    // don't generate text events but are still printable. This is essential for
    // the On-Screen Keyboard to function.
    // The previous check `sym <= SDLK_z` (122) was incorrect, as it excluded several
    // common symbols like '{', '|', '}', and '~'. The correct upper bound for
    // standard printable ASCII is SDLK_TILDE (126).
    if (sym >= SDLK_SPACE && sym <= '~') {
        char c = (char)sym;
        if (shift_pressed && isalpha(c)) {
            c = (char)toupper(c);
        }
        write(pty_fd, &c, 1);
    }

}

/**
 * @brief Sends a text input event (UTF-8 string) to the PTY.
 * @param text The UTF-8 string to send.
 */
void send_text_input_event(int pty_fd, const char* text)
{
    write(pty_fd, text, strlen(text));
}

/**
 * @brief Sends a mouse wheel event to the PTY.
 * @param y_direction The vertical scroll direction (positive for up, negative for down).
 */
void send_mouse_wheel_event(int pty_fd, int y_direction)
{
    if (y_direction > 0) {
        write(pty_fd, "\x1b[A", 3); // Scroll up
    } else if (y_direction < 0) {
        write(pty_fd, "\x1b[B", 3); // Scroll down
    }
}

/**
 * @brief Handles SDL_KEYDOWN events, sending appropriate sequences to the PTY.
 * This function is for non-character keys or keys that need special ANSI sequences.
 * Printable characters are generally handled by SDL_TEXTINPUT.
 * @param key A pointer to the SDL_KeyboardEvent.
 * @param pty_fd The file descriptor of the PTY.
 * @param term The terminal state.
 */
void handle_key_down(const SDL_KeyboardEvent* key, int pty_fd, Terminal* term)
{
    // If the key is a simple printable character without Ctrl/Alt/Gui modifiers,
    // we let SDL_TEXTINPUT handle it to avoid duplication. This is the fix for
    // the double-character input issue with physical keyboards.
    // We still allow Shift to be handled by send_key_event's fallback logic.
    // The OSK is not affected because it calls send_key_event directly, bypassing this function.
    SDL_Keycode sym = key->keysym.sym;
    SDL_Keymod mod = key->keysym.mod;
    if ((mod & (KMOD_CTRL | KMOD_ALT | KMOD_GUI)) == 0 && sym >= SDLK_SPACE && sym <= SDLK_z) {
        return;
    }

    // Check for Ctrl+Shift+C (copy) and Ctrl+Shift+V (paste)
    // Note: SDL_TEXTINPUT handles regular character input. This is for special keys.
    if ((key->keysym.mod & KMOD_CTRL) && (key->keysym.mod & KMOD_SHIFT)) {
        // No direct copy/paste to PTY. This would typically involve clipboard.
        // For now, we just let it pass if not handled by other logic.
    }

    // Send key event to PTY
    send_key_event(pty_fd, key->keysym.sym, key->keysym.mod, term);
}

/**
 * @brief Processes an action in character mode of the OSK.
 * @param action The TerminalAction to process.
 * @param term The terminal state.
 * @param osk The OnScreenKeyboard state.
 * @param needs_render Pointer to a boolean, set to true if rendering is needed.
 * @param pty_fd The PTY file descriptor.
 * @return An InternalCommand if one is triggered, otherwise CMD_NONE.
 */
static InternalCommand process_osk_chars_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int pty_fd)
{
    (void)term; // Unused

    const SpecialKeySet* row = osk_get_effective_row_ptr(osk, osk->set_idx);
    if (!row) {
        return CMD_NONE;
    }

    int num_rows = get_current_num_char_rows(osk);
    if (num_rows == 0) { // No rows in the current modifier layer
        return CMD_NONE;
    }

    int num_chars = row->length;
    if (num_chars == 0) {
        return CMD_NONE;
    }

    switch (action) {
    case ACTION_UP:
        osk->set_idx = (osk->set_idx == 0) ? (num_rows - 1) : (osk->set_idx - 1);
        osk->char_idx = 0; // Reset index when changing rows
        osk_validate_row_index(osk); // Validate after index change
        *needs_render = true;
        break;
    case ACTION_DOWN:
        osk->set_idx = (osk->set_idx + 1) % num_rows;
        osk->char_idx = 0; // Reset index when changing rows
        osk_validate_row_index(osk); // Validate after index change
        *needs_render = true;
        break;
    case ACTION_LEFT:
        osk->char_idx = (osk->char_idx == 0) ? (num_chars - 1) : (osk->char_idx - 1);
        osk_validate_row_index(osk); // Validate after index change
        *needs_render = true;
        break;
    case ACTION_RIGHT:
        osk->char_idx = (osk->char_idx + 1) % num_chars;
        osk_validate_row_index(osk); // Validate after index change
        *needs_render = true;
        break;
    case ACTION_SELECT: {
        const SpecialKey* key = osk_get_effective_char_ptr(osk, osk->set_idx, osk->char_idx);
        if (key) {
            bool ui_updated = false;
            InternalCommand cmd = osk_handle_key_selection(key, term, osk, pty_fd, &ui_updated);
            if (ui_updated) {
                osk_validate_row_index(osk); // Validate after potential modifier/layout change
                *needs_render = true;
            }
            return cmd;
        }
        break;
    }
    case ACTION_BACK:
        send_key_event(pty_fd, SDLK_BACKSPACE, KMOD_NONE, term);
        // Modifiers are now persistent, do not clear here
        break;
    case ACTION_SPACE:
        send_text_input_event(pty_fd, " ");
        // Modifiers are now persistent, do not clear here
        break;
    case ACTION_TAB:
        send_key_event(pty_fd, SDLK_TAB, KMOD_NONE, term);
        // Modifiers are now persistent, do not clear here
        break;
    case ACTION_ENTER:
        send_key_event(pty_fd, SDLK_RETURN, KMOD_NONE, term);
        // Modifiers are now persistent, do not clear here
        break;
    default:
        break;
    }
    return CMD_NONE;
}

/**
 * @brief Processes an action in special key mode of the OSK.
 * @param action The TerminalAction to process.
 * @param term The terminal state.
 * @param osk The OnScreenKeyboard state.
 * @param needs_render Pointer to a boolean, set to true if rendering is needed.
 * @param pty_fd The PTY file descriptor.
 * @return An InternalCommand if one is triggered, otherwise CMD_NONE.
 */
static InternalCommand process_osk_special_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int pty_fd)
{
    (void)term; // Unused

    SpecialKeySet* current_set = &osk->all_special_sets[osk->set_idx];
    int set_len = current_set->length;

    switch (action) {
    case ACTION_UP:
        osk->set_idx = (osk->set_idx == 0) ? (osk->num_total_special_sets - 1) : (osk->set_idx - 1);
        osk->char_idx = 0; // Reset char_idx when changing sets
        osk->show_special_set_name = true;
        osk_validate_row_index(osk); // Validate after index change
        *needs_render = true;
        break;
    case ACTION_DOWN:
        osk->set_idx = (osk->set_idx + 1) % osk->num_total_special_sets;
        osk->char_idx = 0; // Reset char_idx when changing sets
        osk->show_special_set_name = true;
        osk_validate_row_index(osk); // Validate after index change
        *needs_render = true;
        break;
    case ACTION_LEFT:
        osk->char_idx = (osk->char_idx == 0) ? (set_len - 1) : (osk->char_idx - 1);
        if (osk->show_special_set_name) {
            osk->show_special_set_name = false;
        }
        osk_validate_row_index(osk); // Validate after index change
        *needs_render = true;
        break;
    case ACTION_RIGHT:
        osk->char_idx = (osk->char_idx + 1) % set_len;
        if (osk->show_special_set_name) {
            osk->show_special_set_name = false;
        }
        osk_validate_row_index(osk); // Validate after index change
        *needs_render = true;
        break;
    case ACTION_SELECT: {
        if (set_len > 0 && osk->char_idx < set_len) {
            const SpecialKey* key = &current_set->keys[osk->char_idx];

            // Handle key selection using the common helper
            bool ui_updated = false;
            InternalCommand cmd = osk_handle_key_selection(key, term, osk, pty_fd, &ui_updated);
            if (ui_updated) {
                osk_validate_row_index(osk); // Validate after potential modifier/layout change
                *needs_render = true;
            }
            if (cmd != CMD_NONE) {
                return cmd;
            }
        }
        break;
    }
    case ACTION_BACK:
        send_key_event(pty_fd, SDLK_BACKSPACE, KMOD_NONE, term);
        // Modifiers are now persistent, do not clear here
        break;
    case ACTION_SPACE:
        send_text_input_event(pty_fd, " ");
        // Modifiers are now persistent, do not clear here
        break;
    case ACTION_TAB:
        send_key_event(pty_fd, SDLK_TAB, KMOD_NONE, term);
        // Modifiers are now persistent, do not clear here
        break;
    case ACTION_ENTER:
        send_key_event(pty_fd, SDLK_RETURN, KMOD_NONE, term);
        // Modifiers are now persistent, do not clear here
        break;
    default:
        break;
    }
    return CMD_NONE;
}

/**
 * @brief Gets the combined modifier state from OSK's internal modifiers and held controller buttons.
 * @param osk The OnScreenKeyboard state.
 * @return An SDL_Keymod bitmask representing the active modifiers.
 */
static SDL_Keymod get_combined_modifiers(const OnScreenKeyboard* osk)
{
    SDL_Keymod mod = KMOD_NONE;
    if (osk->mod_shift || osk->held_shift) {
        mod |= KMOD_SHIFT;
    }
    if (osk->mod_ctrl || osk->held_ctrl) {
        mod |= KMOD_CTRL;
    }
    if (osk->mod_alt || osk->held_alt) {
        mod |= KMOD_ALT;
    }
    if (osk->mod_gui || osk->held_gui) {
        mod |= KMOD_GUI;
    }
    return mod;
}

/**
 * @brief Clears one-shot modifiers (Ctrl, Alt, Shift, GUI) in the OSK state.
 * @param osk The OnScreenKeyboard state.
 * @param needs_render Pointer to a boolean, set to true if rendering is needed.
 */
static void clear_one_shot_modifiers(OnScreenKeyboard* osk, bool* needs_render_ptr)
{
    if (osk->mod_ctrl || osk->mod_alt || osk->mod_shift || osk->mod_gui) {
        osk->mod_ctrl = false;
        osk->mod_alt = false;
        osk->mod_shift = false;
        osk->mod_gui = false;
        *needs_render_ptr = true;
    }
}

/**
 * @brief Gets the modifier state for sending to the PTY when an OSK key is selected.
 * This function correctly distinguishes between physical modifiers that cause a layer
 * switch and those that should act as traditional modifiers.
 * @param osk The OnScreenKeyboard state.
 * @return An SDL_Keymod bitmask.
 */
static SDL_Keymod get_effective_send_modifiers(const OnScreenKeyboard* osk)
{
    SDL_Keymod mod = KMOD_NONE;

    // 1. Always include virtual (one-shot) modifiers.
    if (osk->mod_shift) {
        mod |= KMOD_SHIFT;
    }
    if (osk->mod_ctrl) {
        mod |= KMOD_CTRL;
    }
    if (osk->mod_alt) {
        mod |= KMOD_ALT;
    }
    if (osk->mod_gui) {
        mod |= KMOD_GUI;
    }

    // 2. Determine the mask of physically held keys.
    int held_mask = OSK_MOD_NONE;
    if (osk->held_shift) {
        held_mask |= OSK_MOD_SHIFT;
    }
    if (osk->held_ctrl) {
        held_mask |= OSK_MOD_CTRL;
    }
    if (osk->held_alt) {
        held_mask |= OSK_MOD_ALT;
    }
    if (osk->held_gui) {
        held_mask |= OSK_MOD_GUI;
    }

    // 3. Check if the held keys correspond to a defined character layer.
    bool layer_exists_for_held_keys = (held_mask != OSK_MOD_NONE) &&
                                      (osk->char_sets_by_modifier[held_mask] != NULL);

    // 4. Only add physical modifiers to the key event if they did NOT cause a layer switch.
    if (!layer_exists_for_held_keys) {
        if (osk->held_shift) {
            mod |= KMOD_SHIFT;
        }
        if (osk->held_ctrl) {
            mod |= KMOD_CTRL;
        }
        if (osk->held_alt) {
            mod |= KMOD_ALT;
        }
        if (osk->held_gui) {
            mod |= KMOD_GUI;
        }
    }

    return mod;
}

/**
 * @brief Executes a macro string, which can contain literal text and special key tokens.
 * @param pty_fd The PTY file descriptor.
 * @param macro_string The string to parse and execute.
 * @param term The terminal state.
 * @param osk The OnScreenKeyboard state.
 * @param ui_updated Pointer to a boolean, set to true if the OSK UI needs a redraw.
 */
static void execute_macro(int pty_fd, const char* macro_string, const Terminal* term, OnScreenKeyboard* osk, bool* ui_updated)
{
    const char* p = macro_string;
    const char* segment_start = p;
    bool consumed_one_shot = false;

    while (*p) {
        // Handle escaped literal brace
        if (*p == '\\' && *(p + 1) == '{') {
            // Write segment before the escaped brace
            if (p > segment_start) {
                char* segment = strndup(segment_start, p - segment_start);
                if (segment) {
                    send_text_input_event(pty_fd, segment);
                    free(segment);
                }
            }
            // Write the literal brace
            send_text_input_event(pty_fd, "{");
            p += 2; // Move past \{
            segment_start = p;
            continue;
        }

        // Handle token
        if (*p == '{') {
            const LayoutToken* token = osk_find_layout_token(p);
            if (token) {
                // Write segment before the token
                if (p > segment_start) {
                    char* segment = strndup(segment_start, p - segment_start);
                    if (segment) {
                        send_text_input_event(pty_fd, segment);
                        free(segment);
                    }
                }

                // Handle the token's action
                if (token->type == SK_SEQUENCE) {
                    SDL_Keymod mods = get_effective_send_modifiers(osk);
                    send_key_event(pty_fd, token->keycode, mods, term);
                    consumed_one_shot = true;
                } else if (token->type >= SK_MOD_CTRL && token->type <= SK_MOD_GUI) {
                    if (token->type == SK_MOD_CTRL) {
                        osk->mod_ctrl = !osk->mod_ctrl;
                    } else if (token->type == SK_MOD_ALT) {
                        osk->mod_alt = !osk->mod_alt;
                    } else if (token->type == SK_MOD_SHIFT) {
                        osk->mod_shift = !osk->mod_shift;
                    } else if (token->type == SK_MOD_GUI) {
                        osk->mod_gui = !osk->mod_gui;
                    }
                    *ui_updated = true;
                }

                p += strlen(token->token);
                segment_start = p;
                continue;
            }
        }
        p++; // Not a special character, just advance
    }

    if (p > segment_start) {
        send_text_input_event(pty_fd, segment_start);
    }
    if (consumed_one_shot) {
        clear_one_shot_modifiers(osk, ui_updated);
    }
}

/**
 * @brief Handles the action of selecting a key on the OSK, sending input or executing commands.
 * This function is shared between character and special key modes.
 * @param key The SpecialKey being selected.
 * @param term The terminal state.
 * @param osk The OnScreenKeyboard state.
 * @param pty_fd The PTY file descriptor.
 * @param ui_updated Pointer to a boolean, set to true if the OSK UI needs a redraw.
 * @return InternalCommand if an internal command is triggered, CMD_NONE otherwise.
 */
static InternalCommand osk_handle_key_selection(const SpecialKey* key, Terminal* term, OnScreenKeyboard* osk, int pty_fd, bool* ui_updated)
{
    (void)term; // Not used currently, but kept for API consistency
    InternalCommand cmd = CMD_NONE;
    bool is_modifier_key = false;

    switch (key->type) {
    case SK_STRING: // A literal string with no tokens
        if (key->sequence) {
            send_text_input_event(pty_fd, key->sequence);
        }
        break;
    case SK_MACRO: // A string that may contain tokens
        if (key->sequence) {
            execute_macro(pty_fd, key->sequence, term, osk, ui_updated);
        }
        // Macro execution handles its own one-shot clearing.
        break;
    case SK_LOAD_FILE:
        if (key->sequence) {
            osk_add_custom_set(osk, key->sequence);
            *ui_updated = true;
        }
        break;
    case SK_UNLOAD_FILE:
        if (key->sequence) {
            osk_remove_custom_set(osk, key->sequence);
            *ui_updated = true;
        }
        break;
    case SK_SEQUENCE: {
        SDL_Keymod mods = get_effective_send_modifiers(osk) | key->mod;
        send_key_event(pty_fd, key->keycode, mods, term);
        break;
    }
    case SK_MOD_CTRL:
        is_modifier_key = true;
        osk->mod_ctrl = !osk->mod_ctrl;
        *ui_updated = true; // Modifier state changed, UI needs update
        break;
    case SK_MOD_ALT:
        is_modifier_key = true;
        osk->mod_alt = !osk->mod_alt;
        *ui_updated = true;
        break;
    case SK_MOD_SHIFT:
        is_modifier_key = true;
        osk->mod_shift = !osk->mod_shift;
        *ui_updated = true;
        break;
    case SK_MOD_GUI:
        is_modifier_key = true;
        osk->mod_gui = !osk->mod_gui;
        *ui_updated = true;
        break;
    case SK_INTERNAL_CMD:
        cmd = key->command;
        break;
    }

    // Clear one-shot modifiers if a non-modifier key was selected.
    if (!is_modifier_key && key->type != SK_MACRO) {
        clear_one_shot_modifiers(osk, ui_updated);
    }
    return cmd;
}


/**
 * @brief Processes a TerminalAction, potentially triggering OSK state changes or internal commands.
 * @param action The TerminalAction to process.
 * @param term The terminal state.
 * @param osk The OnScreenKeyboard state.
 * @param needs_render Pointer to a boolean, set to true if rendering is needed.
 * @param pty_fd The PTY file descriptor.
 * @return An InternalCommand if one is triggered, otherwise CMD_NONE.
 */
InternalCommand process_osk_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int pty_fd)
{
    // Delegate to the appropriate handler based on the current OSK mode.
    if (osk->mode == OSK_MODE_SPECIAL) {
        return process_osk_special_action(action, term, osk, needs_render, pty_fd);
    } else { // OSK_MODE_CHARS
        return process_osk_chars_action(action, term, osk, needs_render, pty_fd);
    }
}

/**
 * @brief Processes a terminal action when the OSK is inactive.
 * This translates abstract actions (from a controller) into direct PTY input.
 */
void process_direct_terminal_action(TerminalAction action, Terminal* term, OnScreenKeyboard* osk, bool* needs_render, int pty_fd)
{
    SDL_Keymod mods = get_combined_modifiers(osk);
    bool consumed_one_shot = false;

    switch (action) {
    case ACTION_UP:
        send_key_event(pty_fd, SDLK_UP, mods, term);
        consumed_one_shot = true;
        break;
    case ACTION_DOWN:
        send_key_event(pty_fd, SDLK_DOWN, mods, term);
        consumed_one_shot = true;
        break;
    case ACTION_LEFT:
        send_key_event(pty_fd, SDLK_LEFT, mods, term);
        consumed_one_shot = true;
        break;
    case ACTION_RIGHT:
        send_key_event(pty_fd, SDLK_RIGHT, mods, term);
        consumed_one_shot = true;
        break;
    case ACTION_BACK:
        send_key_event(pty_fd, SDLK_BACKSPACE, mods, term);
        consumed_one_shot = true;
        break;
    case ACTION_SPACE:
        if (mods != KMOD_NONE) {
            send_key_event(pty_fd, SDLK_SPACE, mods, term);
        } else {
            send_text_input_event(pty_fd, " ");
        }
        consumed_one_shot = true;
        break;
    case ACTION_TAB:
        send_key_event(pty_fd, SDLK_TAB, mods, term);
        consumed_one_shot = true;
        break;
    case ACTION_ENTER:
        send_key_event(pty_fd, SDLK_RETURN, mods, term);
        consumed_one_shot = true;
        break;
    default:
        break;
    }

    if (consumed_one_shot) {
        clear_one_shot_modifiers(osk, needs_render);
    }
}

/**
 * @brief Initializes input devices, specifically game controllers.
 * @param osk Pointer to the OnScreenKeyboard state.
 * @param config Pointer to the application configuration.
 */
void init_input_devices(OnScreenKeyboard* osk, const Config* config)
{
    // Initialize SDL Joystick and GameController subsystems
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) failed: %s\n", SDL_GetError());
        return;
    }

    // Check for connected game controllers
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            osk->controller = SDL_GameControllerOpen(i);
            if (osk->controller) {
                fprintf(stderr, "Opened game controller: %s\n", SDL_GameControllerName(osk->controller));
                osk->joystick = SDL_GameControllerGetJoystick(osk->controller); // Get underlying joystick
                break; // Only open the first one found for now
            } else {
                fprintf(stderr, "Could not open game controller %i: %s\n", i, SDL_GetError());
            }
        }
    }

    // Load dynamic key sets specified in config
    if (config && config->key_sets) {
        for (int i = 0; i < config->num_key_sets; ++i) {
            // Always make the set available in the CONTROL menu, regardless of load_at_startup
            osk_make_set_available(osk, config->key_sets[i].path);

            // Only load the set's actual keys if it's marked for startup loading
            if (config->key_sets[i].load_at_startup) {
                osk_add_custom_set(osk, config->key_sets[i].path);
            }
        }
    }
}
