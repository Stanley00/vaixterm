/**
 * @file osk.c
 * @brief Manages the On-Screen Keyboard (OSK) for the terminal.
 *
 * This file handles rendering, layout loading, and dynamic key set management
 * for the OSK. It supports character-based layouts (from files or a default)
 * and special key layouts that can be extended at runtime by loading `.keys` files.
 * The OSK is designed to be context-aware, positioning itself based on the cursor
 * and providing visual feedback for active modifier keys.
 */
#include "osk.h"
#include "terminal_state.h" // For shared data structures
#include "rendering.h"      // For render_one_osk_key
#include "input.h"          // For key sequence definitions
#include "config.h"         // For DEFAULT_KEY_SET_LIST_PATH (now NULL)

#include <stdio.h> // For fprintf, stderr
#include <string.h>
#include <SDL.h>    // For general SDL types (e.g., SDL_Keycode, KMOD_NONE)
#include <SDL_ttf.h> // For general TTF types (e.g., TTF_Font)
#include <stdlib.h> // For malloc, realloc, free
#include <ctype.h>  // For isspace

// Helper struct for rendering modifier indicators
typedef struct {
    const char* text;
    bool active;
} ModIndicator;

// --- Built-in Special Key Definitions for the 'CONTROL' set ---

// The 'action' keys.
const SpecialKey osk_special_set_action_keys[] = {
    {.display_name = "OSK Pos", .type = SK_INTERNAL_CMD, .sequence = NULL, .keycode = 0, .mod = KMOD_NONE, .command = CMD_OSK_TOGGLE_POSITION},
    {.display_name = "Ctrl",  .type = SK_MOD_CTRL,  .sequence = NULL, .keycode = SDLK_LCTRL, .mod = KMOD_NONE, .command = CMD_NONE},
    {.display_name = "Alt",   .type = SK_MOD_ALT,   .sequence = NULL, .keycode = SDLK_LALT, .mod = KMOD_NONE, .command = CMD_NONE},
    {.display_name = "GUI",   .type = SK_MOD_GUI,   .sequence = NULL, .keycode = SDLK_LGUI,  .mod = KMOD_NONE, .command = CMD_NONE},
    {.display_name = "Esc",   .type = SK_SEQUENCE,  .sequence = "\x1b", .keycode = SDLK_ESCAPE, .mod = KMOD_NONE, .command = CMD_NONE},
    {.display_name = "Tab",   .type = SK_SEQUENCE,  .sequence = "\t", .keycode = SDLK_TAB, .mod = KMOD_NONE, .command = CMD_NONE},
    {.display_name = "Enter", .type = SK_SEQUENCE,  .sequence = "\r", .keycode = SDLK_RETURN, .mod = KMOD_NONE, .command = CMD_NONE},
    {.display_name = "Space", .type = SK_SEQUENCE,  .sequence = " ", .keycode = SDLK_SPACE, .mod = KMOD_NONE, .command = CMD_NONE},
    {.display_name = "Bksp",  .type = SK_SEQUENCE, .sequence = "\b", .keycode = SDLK_BACKSPACE, .mod = KMOD_NONE, .command = CMD_NONE},
    {.display_name = "Del",   .type = SK_SEQUENCE, .sequence = "\x1b[3~", .keycode = SDLK_DELETE, .mod = KMOD_NONE, .command = CMD_NONE},
    {.display_name = "Shift", .type = SK_MOD_SHIFT, .sequence = NULL, .keycode = SDLK_LSHIFT, .mod = KMOD_NONE, .command = CMD_NONE},
};

static const char* s_default_layout_content =
    "[default]\n"
    "qwertyuiop\n"
    "asdfghjkl\n"
    "zxcvbnm\n"
    "-=[]\\\\;',./_+{}|:\"<>?\n"
    "`1234567890\n"
    "\n"
    "[SHIFT]\n"
    "QWERTYUIOP\n"
    "ASDFGHJKL\n"
    "ZXCVBNM\n"
    "{ESC}{F1}{F2}{F3}{F4}{F5}{F6}{F7}{F8}{F9}{F10}{F11}{F12}\n"
    "~!@#$%^&*()\n"
    ;

// Tokens are ordered by length (descending) to handle prefixes correctly (e.g., {F1} vs {F10}).
static const LayoutToken s_layout_tokens[] = {
    // Length 7
    {"{ENTER}",   "ENT",   SK_SEQUENCE,  SDLK_RETURN},
    {"{SPACE}",   "Space", SK_SEQUENCE,  SDLK_SPACE},
    {"{SHIFT}",   "Shift", SK_MOD_SHIFT, SDLK_LSHIFT},
    {"{DEFAULT}", "",      SK_STRING,    SDLK_UNKNOWN}, // Special case for {N/A}
    {"{RIGHT}",   "RIGHT", SK_SEQUENCE,  SDLK_RIGHT},
    // Length 6
    {"{PGUP}", "PGUP", SK_SEQUENCE, SDLK_PAGEUP},
    {"{PGDN}", "PGDN", SK_SEQUENCE, SDLK_PAGEDOWN},
    {"{CTRL}", "Ctrl", SK_MOD_CTRL, SDLK_LCTRL},
    {"{LEFT}", "LEFT", SK_SEQUENCE, SDLK_LEFT},
    {"{HOME}", "HOME", SK_SEQUENCE, SDLK_HOME},
    {"{DOWN}", "DOWN", SK_SEQUENCE, SDLK_DOWN},
    // Length 5
    {"{F10}", "F10", SK_SEQUENCE, SDLK_F10},
    {"{F11}", "F11", SK_SEQUENCE, SDLK_F11},
    {"{F12}", "F12", SK_SEQUENCE, SDLK_F12},
    {"{N/A}", "",    SK_STRING,   SDLK_UNKNOWN},
    {"{ESC}", "ESC", SK_SEQUENCE, SDLK_ESCAPE},
    {"{TAB}", "TAB", SK_SEQUENCE, SDLK_TAB},
    {"{END}", "END", SK_SEQUENCE, SDLK_END},
    {"{INS}", "INS", SK_SEQUENCE, SDLK_INSERT},
    {"{DEL}", "DEL", SK_SEQUENCE, SDLK_DELETE},
    {"{ALT}", "Alt", SK_MOD_ALT,  SDLK_LALT},
    {"{GUI}", "GUI", SK_MOD_GUI,  SDLK_LGUI},
    // Length 4
    {"{UP}", "UP", SK_SEQUENCE, SDLK_UP},
    {"{BS}", "BS", SK_SEQUENCE, SDLK_BACKSPACE},
    {"{F1}", "F1", SK_SEQUENCE, SDLK_F1},
    {"{F2}", "F2", SK_SEQUENCE, SDLK_F2},
    {"{F3}", "F3", SK_SEQUENCE, SDLK_F3},
    {"{F4}", "F4", SK_SEQUENCE, SDLK_F4},
    {"{F5}", "F5", SK_SEQUENCE, SDLK_F5},
    {"{F6}", "F6", SK_SEQUENCE, SDLK_F6},
    {"{F7}", "F7", SK_SEQUENCE, SDLK_F7},
    {"{F8}", "F8", SK_SEQUENCE, SDLK_F8},
    {"{F9}", "F9", SK_SEQUENCE, SDLK_F9},
};
static const int s_num_layout_tokens = sizeof(s_layout_tokens) / sizeof(s_layout_tokens[0]);

/**
 * @brief Finds a layout token (e.g., "{ENTER}") at the start of a string.
 * @param str_start The string to search.
 * @return A pointer to the matching LayoutToken, or NULL if not found.
 */
const LayoutToken* osk_find_layout_token(const char* str_start)
{
    for (int t = 0; t < s_num_layout_tokens; ++t) {
        size_t token_len = strlen(s_layout_tokens[t].token);
        if (strncmp(str_start, s_layout_tokens[t].token, token_len) == 0) {
            return &s_layout_tokens[t];
        }
    }
    return NULL;
}

/**
 * @brief Determines the Y-coordinate for the OSK based on the terminal cursor's position.
 * Positions the OSK at the top if the cursor is in the bottom half, and vice-versa.
 * @return The Y-coordinate for the top of the OSK bar.
 */
int get_osk_y_position(const OnScreenKeyboard* osk, const Terminal* term, int win_h, int char_h)
{
    bool cursor_in_bottom_half = (term->cursor_y >= term->rows / 2);

    if (osk->position_mode == OSK_POSITION_SAME) {
        // SAME side as cursor
        if (cursor_in_bottom_half) {
            return win_h - char_h; // OSK at bottom
        } else {
            return 0; // OSK at top
        }
    } else { // OSK_POSITION_OPPOSITE (default)
        // OPPOSITE side of cursor
        if (cursor_in_bottom_half) {
            return 0; // OSK at top
        } else {
            return win_h - char_h; // OSK at bottom
        }
    }
}

/**
 * @brief Invalidates the OSK's render cache.
 * This forces a recalculation of values like fixed key width on the next render pass.
 * It should be called whenever the OSK's layout or content changes.
 */
static void osk_invalidate_render_cache(OnScreenKeyboard* osk)
{
    osk->cached_set_idx = -1;
    osk->cached_mod_mask = -1;
    osk->cached_key_width = -1;
}

// --- Static OSK Rendering Helpers ---
static void render_key_tape(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                            const Terminal* term, int win_w, int osk_y, int char_w, int char_h,
                            int set_len, int key_area_start_x,
                            OSK_GetDisplayNameFunc get_name_func, OSK_IsKeyToggledFunc is_toggled_func);

static void render_osk_chars(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk, int win_w, int osk_y, int char_w, int char_h);
static void render_osk_special(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk, const Terminal* term, int win_w, int osk_y, int char_w, int char_h);
static int get_modifier_indicators_width(TTF_Font* font, int char_w);
static const char* get_char_display_name(const OnScreenKeyboard* osk, int set_idx, int char_idx);
static bool is_char_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, int set_idx, int char_idx);
static bool is_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, const SpecialKey* key);
static const char* get_special_key_display_name(const OnScreenKeyboard* osk, int set_idx, int char_idx);
static bool is_special_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, int set_idx, int char_idx);
static int s_max_mod_indicator_width = -1;

/**
 * @brief Gets the current OSK modifier bitmask from both one-shot and held modifiers.
 * This function determines the layer to display based ONLY on physically held controller buttons.
 * @param osk The OnScreenKeyboard state.
 * @return The OSK_MOD_ bitmask.
 */
static int get_physical_modifier_mask(const OnScreenKeyboard* osk)
{
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
    return held_mask;
}

/**
 * @brief Gets the effective character row string after handling all layer fallbacks.
 * @param osk The OnScreenKeyboard state.
 * @param set_idx The index of the character set row.
 * @return A pointer to the character string for the set, or NULL if invalid.
 */
const SpecialKeySet* osk_get_effective_row_ptr(const OnScreenKeyboard* osk, int set_idx)
{
    int target_mask = get_physical_modifier_mask(osk);

    // Find the most specific layer that is a subset of the target mask.
    for (int mask_iter = target_mask; mask_iter >= 0; --mask_iter) {
        if ((target_mask & mask_iter) == mask_iter) { // Is mask_iter a subset of target_mask?
            if (osk->char_sets_by_modifier[mask_iter] != NULL && set_idx < osk->num_char_rows_by_modifier[mask_iter]) {
                const SpecialKeySet* row = &osk->char_sets_by_modifier[mask_iter][set_idx];
                // If the row is a {DEFAULT} marker, it means this whole row should fall back to the default layer's row.
                if (row->length == -1) {
                    break; // Stop searching this branch and fall through to the final default lookup.
                }
                return row;
            }
        }
    }

    // Fallback to the default layer's row if no specific one was found or if a {DEFAULT} marker was hit.
    if (osk->char_sets_by_modifier[OSK_MOD_NONE] != NULL && set_idx < osk->num_char_rows_by_modifier[OSK_MOD_NONE]) {
        const SpecialKeySet* row = &osk->char_sets_by_modifier[OSK_MOD_NONE][set_idx];
        if (row->length != -1) {
            return row;
        }
    }

    return NULL;
}
/**
 * @brief Gets the number of character rows for the currently active modifier layer.
 * @param osk The OnScreenKeyboard state.
 * @return The number of rows.
 */
int get_current_num_char_rows(const OnScreenKeyboard* osk)
{
    int target_mask = get_physical_modifier_mask(osk);

    // Find the most specific layer that is a subset of the target mask.
    for (int mask_iter = target_mask; mask_iter >= 0; --mask_iter) {
        if ((target_mask & mask_iter) == mask_iter) { // Is mask_iter a subset of target_mask?
            if (osk->char_sets_by_modifier[mask_iter] != NULL) {
                return osk->num_char_rows_by_modifier[mask_iter];
            }
        }
    }
    // If no specific layer is found, there are no rows.
    return 0;
}

void osk_validate_row_index(OnScreenKeyboard* osk)
{
    bool reset_to_zero = false;

    if (osk->mode == OSK_MODE_CHARS) {
        int current_num_rows = get_current_num_char_rows(osk);
        if (current_num_rows == 0) {
            reset_to_zero = true;
        } else if (osk->set_idx >= current_num_rows) {
            reset_to_zero = true;
        } else {
            const SpecialKeySet* row = osk_get_effective_row_ptr(osk, osk->set_idx);
            if (!row || (row->length > 0 && osk->char_idx >= row->length) || (row->length == 0 && osk->char_idx != 0)) {
                reset_to_zero = true;
            }
        }
    } else { // OSK_MODE_SPECIAL
        int num_special_sets = osk->num_total_special_sets;
        if (num_special_sets == 0) { // Should not happen if CONTROL set is always present
            reset_to_zero = true;
        } else if (osk->set_idx >= num_special_sets) {
            reset_to_zero = true;
        } else {
            // Directly access the special set
            const SpecialKeySet* current_set = &osk->all_special_sets[osk->set_idx];
            if (current_set->length == 0) { // If the current special set is empty
                reset_to_zero = true;
            } else if (osk->char_idx >= current_set->length) { // If char_idx is out of bounds for this set
                reset_to_zero = true;
            }
        }
    }

    if (reset_to_zero) {
        osk->set_idx = 0;
        osk->char_idx = 0;
    }
}
/**
 * @brief Finds the definitive character pointer for a given key, handling all layer fallbacks.
 * This is the core of the OSK's character logic. It will search from the most specific
 * active layer down to the base layer, handling shorter rows and {N/A} sentinels correctly.
 * @param osk The OnScreenKeyboard state.
 * @param set_idx The index of the character set row.
 * @param char_idx The index of the character within the row.
 * @return A pointer to the character within the layout string, or NULL if none found.
 */
const SpecialKey* osk_get_effective_char_ptr(const OnScreenKeyboard* osk, int set_idx, int char_idx)
{
    int target_mask = get_physical_modifier_mask(osk);

    // 1. Get the ultimate fallback key from the [default] layer, if it exists.
    const SpecialKey* default_key = NULL;
    if (osk->char_sets_by_modifier[OSK_MOD_NONE] != NULL && set_idx < osk->num_char_rows_by_modifier[OSK_MOD_NONE]) {
        const SpecialKeySet* default_row = &osk->char_sets_by_modifier[OSK_MOD_NONE][set_idx];
        if (default_row->length != -1 && char_idx >= 0 && char_idx < default_row->length) {
            default_key = &default_row->keys[char_idx];
        }
    }

    // 2. Find the most specific key defined for the target mask.
    for (int mask_iter = target_mask; mask_iter >= 0; --mask_iter) {
        if ((target_mask & mask_iter) == mask_iter) { // Is mask_iter a subset of target_mask?
            if (osk->char_sets_by_modifier[mask_iter] != NULL && set_idx < osk->num_char_rows_by_modifier[mask_iter]) {
                const SpecialKeySet* specific_row = &osk->char_sets_by_modifier[mask_iter][set_idx];

                // If the row is a {DEFAULT} marker, the key is the default key.
                if (specific_row->length == -1) {
                    return default_key;
                }

                // We have a specific row. Check if the key index is valid for it.
                if (char_idx < 0 || char_idx >= specific_row->length) {
                    return NULL;
                }

                // The key exists. Check if it's an {N/A} marker.
                const SpecialKey* specific_key = &specific_row->keys[char_idx];
                bool is_na_key = (specific_key->type == SK_STRING && specific_key->sequence && specific_key->sequence[0] == '\0');
                if (is_na_key) {
                    return default_key;
                }

                // It's a valid, specific key. Use it.
                return specific_key;
            }
        }
    }

    // If no specific layer was found at all, use the default key.
    return default_key;
}

static const char* get_char_display_name(const OnScreenKeyboard* osk, int set_idx, int char_idx)
{
    const SpecialKey* key = osk_get_effective_char_ptr(osk, set_idx, char_idx);
    return key ? key->display_name : "";
}

static bool is_char_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, int set_idx, int char_idx)
{
    const SpecialKey* key = osk_get_effective_char_ptr(osk, set_idx, char_idx);
    return is_key_toggled(term, osk, key);
}

/**
 * @brief Calculates a uniform width for all keys in a set for smooth scrolling.
 * Caches the result to avoid recalculation.
 * @return The calculated fixed width for keys in the current set.
 */
static bool is_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, const SpecialKey* key)
{
    if (!key) {
        return false;
    }
    if (key->type == SK_MOD_CTRL) {
        return osk->mod_ctrl;
    }
    if (key->type == SK_MOD_ALT) {
        return osk->mod_alt;
    }
    if (key->type == SK_MOD_SHIFT) {
        return osk->mod_shift;
    }
    if (key->type == SK_MOD_GUI) {
        return osk->mod_gui;
    }

    if (key->type == SK_INTERNAL_CMD) {
        switch (key->command) {
        case CMD_CURSOR_TOGGLE_VISIBILITY:
            return !term->cursor_visible;
        case CMD_CURSOR_TOGGLE_BLINK:
            return !term->cursor_style_blinking;
        default:
            return false;
        }
    }
    return false;
}

static const char* get_special_key_display_name(const OnScreenKeyboard* osk, int set_idx, int char_idx)
{
    const SpecialKey* key = &osk->all_special_sets[set_idx].keys[char_idx];
    return key->display_name;
}
static bool is_special_key_toggled(const Terminal* term, const OnScreenKeyboard* osk, int set_idx, int char_idx)
{
    const SpecialKey* key = &osk->all_special_sets[set_idx].keys[char_idx];
    return is_key_toggled(term, osk, key);
}

static int calculate_fixed_key_width(TTF_Font* font, OnScreenKeyboard* osk, int set_len, int char_w, OSK_GetDisplayNameFunc get_name_func)
{
    // Invalidate cache if modifier state changes for character mode
    int current_mod_mask = get_physical_modifier_mask(osk);

    if (osk->cached_set_idx == osk->set_idx && osk->cached_mode == osk->mode &&
            (osk->mode == OSK_MODE_SPECIAL || osk->cached_mod_mask == current_mod_mask)) { // Only check mod mask for char mode
        return osk->cached_key_width;
    }

    int max_text_w = 0;
    for (int i = 0; i < set_len; ++i) {
        const char* display_name = get_name_func(osk, osk->set_idx, i);
        int text_w;
        TTF_SizeUTF8(font, display_name, &text_w, NULL);
        max_text_w = SDL_max(max_text_w, text_w);
    }

    const int key_padding_x = char_w;
    const int fixed_key_width = max_text_w + 2 * key_padding_x;

    osk->cached_key_width = fixed_key_width;
    osk->cached_set_idx = osk->set_idx;
    osk->cached_mode = osk->mode;
    osk->cached_mod_mask = current_mod_mask; // Store current mod mask for char mode

    return fixed_key_width;
}

/**
 * @brief Renders a generic "tape" of OSK keys, centered on the selected key.
 */
static void render_key_tape(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk,
                            const Terminal* term, int tape_area_width, int osk_y, int char_w, int char_h,
                            int set_len, int key_area_start_x,
                            OSK_GetDisplayNameFunc get_name_func, OSK_IsKeyToggledFunc is_toggled_func)
{
    if (set_len == 0) {
        return;
    }

    const int key_spacing = char_w / 2;
    const int fixed_key_width = calculate_fixed_key_width(font, osk, set_len, char_w, get_name_func);
    const int selected_idx = osk->char_idx;

    SDL_Rect old_clip;
    SDL_RenderGetClipRect(renderer, &old_clip);
    const int clip_width = tape_area_width - key_area_start_x;
    SDL_Rect key_area_clip = { key_area_start_x, osk_y, clip_width, char_h };
    SDL_RenderSetClipRect(renderer, &key_area_clip);

    // Calculate the center of the clipped drawing area. All positions are relative to this.
    const int tape_center_x = key_area_start_x + clip_width / 2;

    if (selected_idx >= 0 && selected_idx < set_len) {
        const char* name = get_name_func(osk, osk->set_idx, selected_idx);
        const int x = tape_center_x - fixed_key_width / 2;
        const SDL_Rect rect = {x, osk_y, fixed_key_width, char_h};
        const bool toggled = is_toggled_func(term, osk, osk->set_idx, selected_idx);
        render_one_osk_key(renderer, font, osk->key_cache, name, rect, true, toggled, false);
    }

    int right_x = tape_center_x + fixed_key_width / 2 + key_spacing;
    for (int i = selected_idx + 1; i < set_len; ++i) {
        if (right_x >= key_area_start_x + clip_width) {
            break;
        }

        const char* name = get_name_func(osk, osk->set_idx, i);
        const SDL_Rect rect = {right_x, osk_y, fixed_key_width, char_h};
        const bool toggled = is_toggled_func(term, osk, osk->set_idx, i);
        render_one_osk_key(renderer, font, osk->key_cache, name, rect, false, toggled, false);
        right_x += fixed_key_width + key_spacing;
    }

    int left_x = tape_center_x - fixed_key_width / 2 - key_spacing;
    for (int i = selected_idx - 1; i >= 0; --i) {
        const int key_start_x = left_x - fixed_key_width;
        if (key_start_x < key_area_start_x) {
            break;
        }

        const char* name = get_name_func(osk, osk->set_idx, i);
        const SDL_Rect rect = {key_start_x, osk_y, fixed_key_width, char_h};
        const bool toggled = is_toggled_func(term, osk, osk->set_idx, i);
        render_one_osk_key(renderer, font, osk->key_cache, name, rect, false, toggled, false);
        left_x -= (fixed_key_width + key_spacing);
    }

    SDL_RenderSetClipRect(renderer, &old_clip);
}

/**
 * @brief Renders the character key tape of the OSK.
 */
static void render_osk_chars(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk, int available_width, int osk_y, int char_w, int char_h)
{
    const SpecialKeySet* row = osk_get_effective_row_ptr(osk, osk->set_idx);
    if (!row) {
        return;
    }

    int num_chars = row->length;
    if (char_w == 0 || num_chars == 0) {
        return;
    }

    render_key_tape(renderer, font, osk, NULL, available_width, osk_y, char_w, char_h,
                    num_chars, 0,
                    get_char_display_name, is_char_key_toggled);
}

/**
 * @brief Renders the special key tape of the OSK.
 */
static void render_osk_special(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk, const Terminal* term, int available_width, int osk_y, int char_w, int char_h)
{
    SpecialKeySet* current_special_set = &osk->all_special_sets[osk->set_idx];
    const int current_set_len = current_special_set->length;
    const char* set_name = current_special_set->name;

    int key_area_start_x = char_w;
    // Only show the set name if the flag is set.
    if (osk->show_special_set_name && set_name && set_name[0] != '\0') {
        int text_w, text_h;
        TTF_SizeUTF8(font, set_name, &text_w, &text_h);
        SDL_Rect name_rect = {key_area_start_x, osk_y, text_w, char_h};
        render_one_osk_key(renderer, font, osk->key_cache, set_name, name_rect, false, false, true);
        key_area_start_x += text_w + char_w * 2;
    } else {
        key_area_start_x = 0; // Use full width if name is not shown
    }

    render_key_tape(renderer, font, osk, term, available_width, osk_y, char_w, char_h,
                    current_set_len, key_area_start_x,
                    get_special_key_display_name, is_special_key_toggled);
}

/**
 * @brief Calculates the total horizontal space required for modifier indicators.
 * Caches the result.
 * @return The total width in pixels required for the indicators.
 */
static int get_modifier_indicators_width(TTF_Font* font, int char_w)
{
    if (s_max_mod_indicator_width != -1) {
        return s_max_mod_indicator_width;
    }

    if (!font) {
        return 0;
    }

    const char* indicator_names[] = {"G", "S", "A", "^"};
    int total_width = 0;
    int text_w;

    total_width += (char_w / 2);

    for (size_t i = 0; i < sizeof(indicator_names) / sizeof(indicator_names[0]); ++i) {
        TTF_SizeUTF8(font, indicator_names[i], &text_w, NULL);
        total_width += text_w;
        if (i < (sizeof(indicator_names) / sizeof(indicator_names[0])) - 1) {
            total_width += (char_w / 2);
        }
    }
    s_max_mod_indicator_width = total_width;
    return s_max_mod_indicator_width;
}

/**
 * @brief Renders modifier indicators ("^", "A", "S", "G") on the OSK bar if active.
 * The indicators are rendered from right-to-left to ensure they are always
 * anchored to the right side of the screen, regardless of how many are active.
 */
static void render_modifier_indicators(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk, int win_w, int osk_y, int char_w, int char_h)
{
    // Get the active mask from the current layer/set
    int layer_active_mask = OSK_MOD_NONE;
    if (osk->mode == OSK_MODE_CHARS) {
        const SpecialKeySet* row = osk_get_effective_row_ptr(osk, osk->set_idx);
        if (row) {
            layer_active_mask = row->active_mod_mask;
        }
    } else { // OSK_MODE_SPECIAL
        if (osk->set_idx >= 0 && osk->set_idx < osk->num_total_special_sets) {
            const SpecialKeySet* set = &osk->all_special_sets[osk->set_idx];
            layer_active_mask = set->active_mod_mask;
        }
    }

    // Check both internal OSK modifiers and held controller modifiers
    if (!osk->mod_ctrl && !osk->mod_alt && !osk->mod_shift && !osk->mod_gui &&
            !osk->held_ctrl && !osk->held_shift && !osk->held_alt && !osk->held_gui &&
            layer_active_mask == OSK_MOD_NONE) {
        return;
    }

    if (!font) {
        return;
    }

    int held_mask = get_physical_modifier_mask(osk);

    // A layer switch occurs if a layer is defined for the exact combination of held keys.
    // If so, the physical modifier's job is done, and it shouldn't be shown as an indicator.
    bool layer_switch_active =
        (osk->mode == OSK_MODE_CHARS) &&
        (held_mask != OSK_MOD_NONE) &&
        (osk->char_sets_by_modifier[held_mask] != NULL);

    // Show virtual mods, layer mods, and physical mods (if they aren't causing a layer switch).
    const ModIndicator indicators[] = {
        {"G",   osk->mod_gui   || (osk->held_gui   && !layer_switch_active) || (layer_active_mask & OSK_MOD_GUI)},
        {"S",   osk->mod_shift || (osk->held_shift && !layer_switch_active) || (layer_active_mask & OSK_MOD_SHIFT)},
        {"A",   osk->mod_alt   || (osk->held_alt   && !layer_switch_active) || (layer_active_mask & OSK_MOD_ALT)},
        {"^",   osk->mod_ctrl  || (osk->held_ctrl  && !layer_switch_active) || (layer_active_mask & OSK_MOD_CTRL)},
    };
    const char* indicator_names_fixed_order[] = {"G", "S", "A", "^"};

    int current_x_right_edge = win_w - (char_w / 2);

    // Pre-calculate the positions for all possible indicators.
    typedef struct {
        int x;
        int w;
    } IndicatorSlot;
    IndicatorSlot slots[4];

    for (int i = 0; i < 4; ++i) {
        int text_w;
        TTF_SizeUTF8(font, indicator_names_fixed_order[i], &text_w, NULL);
        // Note: The original code had `total_width += text_w;` here, which is incorrect for calculating
        // individual slot positions from the right edge. It should only be used for the total width.
        // The `current_x_right_edge` calculation is correct for positioning.
        current_x_right_edge -= text_w;
        slots[i].x = current_x_right_edge;
        slots[i].w = text_w;

        if (i < 3) {
            current_x_right_edge -= (char_w / 2);
        }
    }

    // Render only the active indicators in their pre-calculated slots.
    for (size_t i = 0; i < sizeof(indicators) / sizeof(indicators[0]); ++i) {
        if (indicators[i].active) {
            SDL_Rect rect = {slots[i].x, osk_y, slots[i].w, char_h};
            render_one_osk_key(renderer, font, osk->key_cache, indicators[i].text, rect, false, true, false);
        }
    }
}

void render_osk(SDL_Renderer* renderer, TTF_Font* font, OnScreenKeyboard* osk, const Terminal* term, int win_w, int win_h, int char_w, int char_h)
{
    // Determine if the OSK should be at the top or bottom of the screen.
    int osk_y = get_osk_y_position(osk, term, win_h, char_h);

    SDL_Rect bg_rect = {0, osk_y, win_w, char_h};
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 220);
    SDL_RenderFillRect(renderer, &bg_rect);

    int indicator_width = get_modifier_indicators_width(font, char_w);
    // The key tape gets the window width minus the space reserved for indicators.
    int tape_render_width = win_w - indicator_width;

    // If the OSK is in special mode, render the special key sets. The held
    // modifiers will still apply to any key presses, but will not change the view.
    // Otherwise, render the character layouts, where held modifiers do change the view.
    if (osk->mode == OSK_MODE_SPECIAL) {
        render_osk_special(renderer, font, osk, term, tape_render_width, osk_y, char_w, char_h);
    } else {
        render_osk_chars(renderer, font, osk, tape_render_width, osk_y, char_w, char_h);
    }

    // Reset clip rect to draw indicators over the full OSK bar width.
    SDL_Rect full_window_clip = {0, 0, win_w, win_h};
    SDL_RenderSetClipRect(renderer, &full_window_clip);

    render_modifier_indicators(renderer, font, osk, win_w, osk_y, char_w, char_h);
}

/**
 * @brief Processes a line from a layout file, replacing special tokens with sentinel bytes.
 * @param input The raw line from the layout file.
 * @return A newly allocated SpecialKeySet with tokens replaced, or NULL on failure.
 */
static SpecialKeySet process_layout_line(const char* input)
{
    SpecialKeySet new_set = { .name = NULL, .keys = NULL, .length = 0, .is_dynamic = true, .file_path = NULL, .active_mod_mask = OSK_MOD_NONE };
    const char* p = input;
    int capacity = 0;

    while (*p) {
        SpecialKey new_key = { .display_name = NULL, .type = SK_STRING, .sequence = NULL, .keycode = SDLK_UNKNOWN, .mod = KMOD_NONE, .command = CMD_NONE };
        bool key_created = false;

        // 1. Try to match a sentinel token like {SHIFT}
        if (*p == '{') {
            for (int t = 0; t < s_num_layout_tokens; ++t) {
                size_t token_len = strlen(s_layout_tokens[t].token);
                if (strncmp(p, s_layout_tokens[t].token, token_len) == 0) {
                    new_key.display_name = strdup(s_layout_tokens[t].display);
                    new_key.type = s_layout_tokens[t].type;
                    new_key.keycode = s_layout_tokens[t].keycode;

                    // Special case for {N/A} and {DEFAULT}, which are SK_STRING
                    // and need an allocated (but empty) sequence.
                    if (new_key.type == SK_STRING) {
                        new_key.sequence = strdup("");
                    }

                    p += token_len;
                    key_created = true;
                    break;
                }
            }
        }

        // 2. If not a token, try to match an escape sequence like \x...
        if (!key_created && *p == '\\' && *(p + 1) != '\0') {
            p++; // Move past the backslash
            if (*p == 'x') {
                // This part is for parsing hex escape sequences, which are typically for control characters.
                // For now, let's just treat it as a literal character if it's not a known escape.
                // The original code had a bug here, it would fall through to literal char.
                // For now, let's keep the existing behavior for \xNN and let it fall through to the literal character logic.
            }
            // Literal character after backslash (e.g., \\, \:)
            int char_len = utf8_char_len(p);
            char* str = malloc(char_len + 1);
            if (!str) {
                fprintf(stderr, "Failed to allocate memory for escaped char string.\n");
                p += char_len;
                continue;
            }
            strncpy(str, p, char_len);
            str[char_len] = '\0';
            new_key.display_name = str;

            // If it's a single ASCII character, treat as SK_SEQUENCE
            if (char_len == 1 && str[0] >= 0x20 && str[0] <= 0x7E) {
                new_key.type = SK_SEQUENCE;
                new_key.keycode = (SDL_Keycode)str[0];
                new_key.mod = KMOD_NONE;
                new_key.sequence = NULL;
            } else {
                // Multi-byte UTF-8 or non-printable ASCII, treat as string
                new_key.type = SK_STRING;
                new_key.sequence = strdup(str);
                new_key.keycode = SDLK_UNKNOWN;
                new_key.mod = KMOD_NONE;
            }
            p += char_len;
            key_created = true;
        }

        // 3. If nothing else matched, it's a literal character.
        if (!key_created) {
            int char_len = utf8_char_len(p);
            char* str = malloc(char_len + 1);
            if (!str) {
                fprintf(stderr, "Failed to allocate memory for literal char string.\n");
                p += char_len;
                continue;
            }
            strncpy(str, p, char_len);
            str[char_len] = '\0';

            new_key.display_name = str;

            // If it's a single ASCII character (printable range), treat as SK_SEQUENCE
            if (char_len == 1 && str[0] >= 0x20 && str[0] <= 0x7E) {
                new_key.type = SK_SEQUENCE;
                new_key.keycode = (SDL_Keycode)str[0];
                new_key.mod = KMOD_NONE;
                new_key.sequence = NULL;
            } else {
                // Multi-byte UTF-8 character or non-printable ASCII, treat as string
                new_key.type = SK_STRING;
                new_key.sequence = strdup(str);
                new_key.keycode = SDLK_UNKNOWN;
                new_key.mod = KMOD_NONE;
            }
            p += char_len;
            key_created = true;
        }

        if (key_created) {
            if (new_set.length >= capacity) {
                capacity = (capacity == 0) ? 8 : capacity * 2;
                SpecialKey* temp = realloc(new_set.keys, sizeof(SpecialKey) * (size_t)capacity);
                if (temp) {
                    new_set.keys = temp;
                } else {
                    fprintf(stderr, "Failed to realloc for new key.\n");
                    key_created = false; // Prevent adding the key
                }
            }
            if (key_created) {
                new_set.keys[new_set.length++] = new_key;
            } else {
                fprintf(stderr, "Failed to realloc for new key.\n");
                free(new_key.display_name);
                free(new_key.sequence); // Safe to call free(NULL)
            }
        }
    }
    return new_set;
}

/**
 * @brief Frees the dynamically allocated contents of a SpecialKeySet (the keys array and its strings).
 * This function also sets the keys pointer to NULL and length to 0.
 * @param set A pointer to the SpecialKeySet to clean.
 */
static void free_special_key_set_contents(SpecialKeySet* set)
{
    if (!set || !set->keys) {
        return;
    }
    for (int i = 0; i < set->length; ++i) {
        free(set->keys[i].display_name);
        // Only free sequence if it was allocated (for SK_STRING, SK_MACRO, etc.)
        if (set->keys[i].type == SK_STRING || set->keys[i].type == SK_MACRO || set->keys[i].type == SK_LOAD_FILE || set->keys[i].type == SK_UNLOAD_FILE) {
            free(set->keys[i].sequence);
        }
    }
    free(set->keys);
    set->keys = NULL;
    set->length = 0;
}

/**
 * @brief Frees an array of SpecialKeySet rows.
 * @param rows The SpecialKeySet* array to free.
 * @param num_rows The number of rows in the layout.
 */
static void free_char_layout_rows(SpecialKeySet* rows, int num_rows)
{
    if (rows) {
        for (int i = 0; i < num_rows; ++i) {
            free_special_key_set_contents(&rows[i]);
        }
        free(rows);
    }
}

/**
 * @brief Maps a string modifier name to its OSK_MOD_ bitmask.
 * @param mod_name The string name (e.g., "normal", "shift", "ctrl+alt").
 * @return The corresponding OSK_MOD_ bitmask, or -1 if invalid.
 */
static int get_modifier_mask_from_name_part(char* mod_name_non_const)
{
    int mask = OSK_MOD_NONE;
    char* saveptr;
    char* p;

    // Lowercase the whole string first for case-insensitivity
    for (p = mod_name_non_const; *p; ++p) {
        *p = tolower((unsigned char)*p);
    }

    char* token = strtok_r(mod_name_non_const, "+", &saveptr);
    while (token) {
        // Trim whitespace from token
        while (*token && isspace((unsigned char)*token)) {
            token++;
        }
        p = token + strlen(token) - 1;
        while (p >= token && isspace((unsigned char)*p)) {
            *p-- = '\0';
        }

        if (strcmp(token, "default") == 0 || strcmp(token, "normal") == 0) {
            mask |= OSK_MOD_NONE; // Redundant, but harmless
        } else if (strcmp(token, "shift") == 0) {
            mask |= OSK_MOD_SHIFT;
        } else if (strcmp(token, "ctrl") == 0 || strcmp(token, "ctl") == 0) {
            mask |= OSK_MOD_CTRL;
        } else if (strcmp(token, "alt") == 0) {
            mask |= OSK_MOD_ALT;
        } else if (strcmp(token, "gui") == 0) {
            mask |= OSK_MOD_GUI;
        } else if (strlen(token) > 0) {
            fprintf(stderr, "Warning: Unknown modifier '%s' in layout file. Skipping.\n", token);
            return -1; // Invalid modifier
        }
        token = strtok_r(NULL, "+", &saveptr);
    }
    return mask;
}

/**
 * @brief Parses a section header which can contain show and active modifiers.
 * Format: `[show_mods]` or `[show_mods:active_mods]`.
 * @param section_name The content of the section header (e.g., "ctrl+alt:alt").
 * @param show_mask Pointer to store the mask for when to show the layer.
 * @param active_mask Pointer to store the mask for what modifiers are active for the layer.
 * @return True on success, false on failure.
 */
static bool parse_section_header_masks(char* section_name, int* show_mask, int* active_mask)
{
    *show_mask = OSK_MOD_NONE;
    *active_mask = OSK_MOD_NONE;

    char* active_part = strchr(section_name, ':');
    char* show_part = section_name;

    if (active_part != NULL) {
        *active_part = '\0'; // Split the string
        active_part++;
    }

    int s_mask = get_modifier_mask_from_name_part(show_part);
    if (s_mask == -1) return false;
    *show_mask = s_mask;

    if (active_part != NULL && *active_part != '\0') {
        int a_mask = get_modifier_mask_from_name_part(active_part);
        if (a_mask == -1) return false;
        *active_mask = a_mask;
    } else {
        // If no active part is specified, default to no active modifiers.
        *active_mask = OSK_MOD_NONE;
    }
    return true;
}

/**
 * @brief Parses OSK layout content from a string.
 * @param content The string containing the layout definition.
 * @param temp_key_sets_by_modifier A temporary array to store parsed key sets.
 * @param temp_num_rows A temporary array to store the number of rows for each modifier.
 * @param temp_capacity A temporary array to store the capacity for each modifier's row array.
 * @return True on success, false on failure.
 */
static bool parse_layout_content(const char* content, SpecialKeySet** temp_key_sets_by_modifier, int* temp_num_rows, int* temp_capacity)
{
    char* content_copy = strdup(content);
    if (!content_copy) {
        fprintf(stderr, "Failed to allocate memory for layout content copy.\n");
        return false;
    }

    int current_mask = -1;
    int current_active_mask = OSK_MOD_NONE;
    char* line_saveptr;
    char* line = strtok_r(content_copy, "\n", &line_saveptr);

    while (line) {
        line[strcspn(line, "\r")] = 0;

        char* end = line + strlen(line) - 1;
        while (end >= line && isspace((unsigned char)*end)) {
            *end-- = '\0';
        }
        char* start = line;
        while (*start && isspace((unsigned char)*start)) {
            start++;
        }

        if (start[0] == '#' || start[0] == '\0') {
            line = strtok_r(NULL, "\n", &line_saveptr);
            continue;
        }

        if (start[0] == '[' && start[strlen(start) - 1] == ']') {
            char section_name[64] = {0};
            size_t line_len = strlen(start);
            size_t content_len = line_len - 2;
            if (content_len >= sizeof(section_name)) {
                fprintf(stderr, "Warning: Section name in OSK layout is too long: %s\n", start);
                content_len = sizeof(section_name) - 1;
            }
            strncpy(section_name, start + 1, content_len);
            section_name[content_len] = '\0';

            int show_mask, active_mask;
            if (parse_section_header_masks(section_name, &show_mask, &active_mask)) {
                current_mask = show_mask;
                current_active_mask = active_mask;
            } else {
                current_mask = -1; // Mark as invalid to skip lines until next valid section
                fprintf(stderr, "Warning: Invalid section header '%s' in OSK layout file. Skipping section.\n", start);
            }
        } else if (current_mask != -1) {
            if (temp_num_rows[current_mask] >= temp_capacity[current_mask]) {
                int new_capacity = (temp_capacity[current_mask] == 0) ? 4 : temp_capacity[current_mask] * 2;
                SpecialKeySet* temp = realloc(temp_key_sets_by_modifier[current_mask], sizeof(SpecialKeySet) * (size_t)new_capacity);
                if (!temp) {
                    fprintf(stderr, "OSK layout parser: realloc failed for modifier set rows\n");
                    free(content_copy);
                    return false;
                }
                temp_key_sets_by_modifier[current_mask] = temp;
                temp_capacity[current_mask] = new_capacity;
            }

            if (strcmp(start, "{DEFAULT}") == 0) {
                SpecialKeySet default_marker = { .name = NULL, .keys = NULL, .length = -1, .is_dynamic = false, .file_path = NULL, .active_mod_mask = current_active_mask };
                temp_key_sets_by_modifier[current_mask][temp_num_rows[current_mask]++] = default_marker;
            } else {
                SpecialKeySet processed_row = process_layout_line(start);
                // The active mask is determined by the section, not the line.
                processed_row.active_mod_mask = current_active_mask;
                temp_key_sets_by_modifier[current_mask][temp_num_rows[current_mask]++] = processed_row;
            }
        }

        line = strtok_r(NULL, "\n", &line_saveptr);
    }

    free(content_copy);
    return (temp_key_sets_by_modifier[OSK_MOD_NONE] != NULL);
}

// --- Layout Flattening Logic ---

/**
 * @brief Pre-calculates the final "flattened" layouts for all 16 modifier combinations.
 * This does the heavy lifting at load time so that rendering is fast.
 */
static void osk_flatten_layouts(OnScreenKeyboard* osk, SpecialKeySet** parsed_sets, int* parsed_num_rows)
{
    // This function is now much simpler. It just deep-copies the parsed layouts
    // without any flattening or fallback logic. The fallback will be handled at
    // render/access time.
    for (int mask = 0; mask < 16; ++mask) {
        if (parsed_sets[mask] == NULL) {
            osk->char_sets_by_modifier[mask] = NULL;
            osk->num_char_rows_by_modifier[mask] = 0;
            continue;
        }

        int num_rows = parsed_num_rows[mask];
        osk->num_char_rows_by_modifier[mask] = num_rows;
        osk->char_sets_by_modifier[mask] = malloc(sizeof(SpecialKeySet) * (size_t)num_rows);
        if (!osk->char_sets_by_modifier[mask]) {
            osk->num_char_rows_by_modifier[mask] = 0;
            continue;
        }

        for (int i = 0; i < num_rows; ++i) {
            SpecialKeySet* dest_row = &osk->char_sets_by_modifier[mask][i];
            const SpecialKeySet* src_row = &parsed_sets[mask][i];
            *dest_row = *src_row;
            dest_row->keys = NULL;

            if (src_row->length > 0) {
                dest_row->keys = malloc(sizeof(SpecialKey) * (size_t)src_row->length);
                if (!dest_row->keys) {
                    dest_row->length = 0;
                    continue;
                }
                for (int j = 0; j < src_row->length; ++j) {
                    dest_row->keys[j] = src_row->keys[j];
                    dest_row->keys[j].display_name = strdup(src_row->keys[j].display_name);
                    // Only strdup sequence if it's not NULL (i.e., for SK_STRING, SK_LOAD_FILE, SK_UNLOAD_FILE)
                    if (src_row->keys[j].type == SK_STRING || src_row->keys[j].type == SK_LOAD_FILE || src_row->keys[j].type == SK_UNLOAD_FILE) {
                        dest_row->keys[j].sequence = strdup(src_row->keys[j].sequence);
                    } else {
                        dest_row->keys[j].sequence = NULL; // Ensure SK_SEQUENCE has NULL sequence
                    }
                }
            }
        }
    }
}

/**
 * @brief Loads an OSK character layout, either from a file or by using the default.
 * @param osk The OnScreenKeyboard state to populate.
 * @param path Path to the layout file, or NULL to use the default QWERTY layout.
 */
void osk_load_layout(OnScreenKeyboard* osk, const char* path)
{
    // Free any existing layout first
    for (int i = 0; i < 16; ++i) {
        free_char_layout_rows(osk->char_sets_by_modifier[i], osk->num_char_rows_by_modifier[i]);
        osk->char_sets_by_modifier[i] = NULL;
    }
    memset(osk->num_char_rows_by_modifier, 0, sizeof(osk->num_char_rows_by_modifier));

    // Temporary storage for the raw parsed layout.
    SpecialKeySet* temp_key_sets_by_modifier[16] = {NULL};
    int temp_num_rows[16] = {0};
    int temp_capacity[16] = {0}; // For geometric growth
    bool loaded_successfully = false;

    FILE* file = path ? fopen(path, "r") : NULL;
    if (file) {
        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        fseek(file, 0, SEEK_SET);

        char* content = malloc((size_t)fsize + 1);
        if (content) {
            fread(content, 1, (size_t)fsize, file);
            content[fsize] = 0;
            loaded_successfully = parse_layout_content(content, temp_key_sets_by_modifier, temp_num_rows, temp_capacity);
            free(content);
        }
        fclose(file);
    }

    if (!loaded_successfully) {
        if (path) {
            fprintf(stderr, "Warning: Could not load OSK layout '%s'. Using default.\n", path);
        }
        // Clean up any partially loaded data from a failed file read
        for (int i = 0; i < 16; ++i) {
            free_char_layout_rows(temp_key_sets_by_modifier[i], temp_num_rows[i]);
            temp_key_sets_by_modifier[i] = NULL;
            temp_num_rows[i] = 0;
            temp_capacity[i] = 0;
        }
        loaded_successfully = parse_layout_content(s_default_layout_content, temp_key_sets_by_modifier, temp_num_rows, temp_capacity);
    }

    if (loaded_successfully) {
        osk_flatten_layouts(osk, temp_key_sets_by_modifier, temp_num_rows);
    } else {
        fprintf(stderr, "Error: Could not load or create a default OSK layout.\n");
    }

    for (int i = 0; i < 16; ++i) {
        free_char_layout_rows(temp_key_sets_by_modifier[i], temp_num_rows[i]);
    }

    osk_invalidate_render_cache(osk);
}
/**
 * @brief Parses a single line from a custom key set file.
 * Format: `display-name:value[:modifiers]`. Supports escaping ':'.
 * @param line The string line to parse.
 * @param key A pointer to a SpecialKey struct to populate.
 * @return True on success, false on failure.
 */
static bool parse_key_set_line(char* line, SpecialKey* key)
{
    char display_name_buf[256] = {0};
    char value_buf[256] = {0};
    char mods_buf[256] = {0};

    // This parser manually splits the line into up to 3 parts using ':' as a delimiter.
    // It uses an array of buffers and an index to track which part it's currently parsing.
    // State 0: display_name, State 1: value, State 2: modifiers.
    // A backslash '\' is used to escape the next character.
    char* bufs[] = { display_name_buf, value_buf, mods_buf };
    int buf_idx = 0;
    int char_idx = 0;
    const int max_len = 255;

    for (char* p = line; *p != '\0' && *p != '\n' && *p != '\r'; ++p) {
        if (*p == '\\') {
            p++;
            if (*p == '\0') {
                break;
            }
            if (char_idx < max_len) {
                bufs[buf_idx][char_idx++] = *p;
            }
        } else if (*p == ':' && buf_idx < 2) {
            bufs[buf_idx][char_idx] = '\0';
            buf_idx++;
            char_idx = 0;
        } else {
            if (char_idx < max_len) {
                bufs[buf_idx][char_idx++] = *p;
            }
        }
    }
    bufs[buf_idx][char_idx] = '\0';

    char* display_name_str = display_name_buf;
    char* value_str = value_buf;
    char* mods_str = mods_buf;

    if (strlen(display_name_str) == 0 || strlen(value_str) == 0) {
        return false;
    }

    key->display_name = strdup(display_name_str);
    if (!key->display_name) {
        return false;
    }

    key->sequence = NULL;
    key->keycode = SDLK_UNKNOWN;
    key->mod = KMOD_NONE;
    key->command = CMD_NONE;

    // Handle SK_LOAD_FILE and SK_UNLOAD_FILE types
    if (strcasecmp(value_str, "LOAD_FILE") == 0) {
        key->type = SK_LOAD_FILE;
        key->sequence = strdup(mods_str); // mods_str contains the path/name
        if (!key->sequence) {
            free(key->display_name);
            return false;
        }
        return true;
    } else if (strcasecmp(value_str, "UNLOAD_FILE") == 0) {
        key->type = SK_UNLOAD_FILE;
        key->sequence = strdup(mods_str); // mods_str contains the path/name
        if (!key->sequence) {
            free(key->display_name);
            return false;
        }
        return true;
    }

    // Check if value_str is an internal command
    const struct {
        const char* name;
        InternalCommand cmd;
    } cmd_map[] = {
        {"CMD_FONT_INC", CMD_FONT_INC},
        {"CMD_FONT_DEC", CMD_FONT_DEC},
        {"CMD_CURSOR_TOGGLE_VISIBILITY", CMD_CURSOR_TOGGLE_VISIBILITY},
        {"CMD_CURSOR_TOGGLE_BLINK", CMD_CURSOR_TOGGLE_BLINK},
        {"CMD_CURSOR_CYCLE_STYLE", CMD_CURSOR_CYCLE_STYLE},
        {"CMD_TERMINAL_RESET", CMD_TERMINAL_RESET},
        {"CMD_TERMINAL_CLEAR", CMD_TERMINAL_CLEAR},
        {"CMD_OSK_TOGGLE_POSITION", CMD_OSK_TOGGLE_POSITION},
    };
    for (size_t i = 0; i < sizeof(cmd_map) / sizeof(cmd_map[0]); ++i) {
        if (strcasecmp(value_str, cmd_map[i].name) == 0) {
            key->type = SK_INTERNAL_CMD;
            key->command = cmd_map[i].cmd;
            return true;
        }
    }

    // If value_str is a quoted string, it could be SK_STRING or SK_MACRO.
    size_t value_len = strlen(value_str);
    if (value_len >= 2 && value_str[0] == '"' && value_str[value_len - 1] == '"') {
        value_str[value_len - 1] = '\0'; // Unquote
        char* content = value_str + 1;

        // Scan for unescaped '{' which indicates a token/macro.
        bool has_tokens = false;
        for (const char* p = content; *p; ++p) {
            if (*p == '\\' && *(p + 1) == '{') {
                p++; // Skip the escaped brace, continue scanning
                continue;
            }
            if (*p == '{' && osk_find_layout_token(p)) {
                has_tokens = true;
                break;
            }
        }

        if (has_tokens) {
            key->type = SK_MACRO;
            key->sequence = strdup(content); // Store raw content with escapes
        } else {
            // No tokens, so it's a simple string. We need to unescape `\{` to `{`.
            key->type = SK_STRING;
            char* unescaped_str = malloc(strlen(content) + 1);
            if (unescaped_str) {
                char* writer = unescaped_str;
                for (const char* reader = content; *reader; ++reader) {
                    if (*reader == '\\' && *(reader + 1) == '{') {
                        *writer++ = '{';
                        reader++; // Skip the brace that was just handled
                    } else {
                        *writer++ = *reader;
                    }
                }
                *writer = '\0';
                key->sequence = unescaped_str;
            } else {
                key->sequence = NULL; // Malloc failed
            }
        }

        if (!key->sequence) {
            free(key->display_name);
            return false;
        }
        key->keycode = SDLK_UNKNOWN;
        key->mod = KMOD_NONE;
        return true;
    }

    // Parse modifiers from mods_str
    SDL_Keymod parsed_mod = KMOD_NONE;
    if (mods_str && strlen(mods_str) > 0) {
        char temp_mods[256];
        strncpy(temp_mods, mods_str, sizeof(temp_mods) - 1);
        temp_mods[sizeof(temp_mods) - 1] = '\0';
        char* saveptr;
        char* mod_token = strtok_r(temp_mods, ",", &saveptr);
        while (mod_token) {
            while (isspace((unsigned char)*mod_token)) {
                mod_token++;
            }
            char* end = mod_token + strlen(mod_token) - 1;
            while (end > mod_token && isspace((unsigned char)*end)) {
                end--;
            }
            *(end + 1) = '\0';

            if (strcmp(mod_token, "ctrl") == 0) {
                parsed_mod |= KMOD_CTRL;
            } else if (strcmp(mod_token, "alt") == 0) {
                parsed_mod |= KMOD_ALT;
            } else if (strcmp(mod_token, "gui") == 0 || strcmp(mod_token, "win") == 0 || strcmp(mod_token, "super") == 0) {
                parsed_mod |= KMOD_GUI;
            } else if (strcmp(mod_token, "shift") == 0) {
                parsed_mod |= KMOD_SHIFT;
            } else if (strlen(mod_token) > 0) {
                fprintf(stderr, "Warning: Unknown modifier '%s' in key set file for key '%s'.\n", mod_token, display_name_str);
            }
            mod_token = strtok_r(NULL, ",", &saveptr);
        }
    }
    key->mod = parsed_mod;

    // Determine keycode for SK_SEQUENCE type
    SDL_Keycode keycode;
    if (strcasecmp(value_str, "ESC") == 0) {
        keycode = SDLK_ESCAPE;
    } else if (strcasecmp(value_str, "ENTER") == 0) {
        keycode = SDLK_RETURN;
    } else if (strcasecmp(value_str, "BS") == 0 || strcasecmp(value_str, "BACKSPACE") == 0) {
        keycode = SDLK_BACKSPACE;
    } else if (strcasecmp(value_str, "DEL") == 0 || strcasecmp(value_str, "DELETE") == 0) {
        keycode = SDLK_DELETE;
    } else if (strcasecmp(value_str, "PGUP") == 0 || strcasecmp(value_str, "PAGEUP") == 0) {
        keycode = SDLK_PAGEUP;
    } else if (strcasecmp(value_str, "PGDN") == 0 || strcasecmp(value_str, "PAGEDOWN") == 0) {
        keycode = SDLK_PAGEDOWN;
    } else if (strcasecmp(value_str, "TAB") == 0) {
        keycode = SDLK_TAB;
    } else {
        keycode = SDL_GetKeyFromName(value_str);
    }

    // All other keys are treated as SK_SEQUENCE
    key->type = SK_SEQUENCE;
    key->keycode = keycode;
    key->sequence = NULL; // SK_SEQUENCE does not use sequence field

    return true;
}

/**
 * @brief Checks if a dynamic key set is currently loaded.
 * @param osk Pointer to the OnScreenKeyboard state.
 * @param name The name of the key set (e.g., "Nav", "Function").
 * @return True if loaded, false otherwise.
 */
static bool is_dynamic_key_set_loaded(const OnScreenKeyboard* osk, const char* name)
{
    for (int i = 0; i < osk->num_loaded_key_sets; ++i) {
        if (strcmp(osk->loaded_key_set_names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Adds a key set name to the list of loaded sets.
 * @param osk Pointer to the OnScreenKeyboard state.
 * @param name The name of the key set to add.
 */
static void add_loaded_key_set_name(OnScreenKeyboard* osk, const char* name)
{
    if (is_dynamic_key_set_loaded(osk, name)) {
        return; // Already loaded
    }
    osk->loaded_key_set_names = realloc(osk->loaded_key_set_names, sizeof(char*) * (size_t)(osk->num_loaded_key_sets + 1));
    if (!osk->loaded_key_set_names) {
        fprintf(stderr, "Error: Failed to reallocate loaded_key_set_names.\n");
        return;
    }
    osk->loaded_key_set_names[osk->num_loaded_key_sets++] = strdup(name);
}

/**
 * @brief Removes a key set name from the list of loaded sets.
 * @param osk Pointer to the OnScreenKeyboard state.
 * @param name The name of the key set to remove.
 */
static void remove_loaded_key_set_name(OnScreenKeyboard* osk, const char* name)
{
    for (int i = 0; i < osk->num_loaded_key_sets; ++i) {
        if (strcmp(osk->loaded_key_set_names[i], name) == 0) {
            free(osk->loaded_key_set_names[i]);
            for (int j = i; j < osk->num_loaded_key_sets - 1; ++j) {
                osk->loaded_key_set_names[j] = osk->loaded_key_set_names[j + 1];
            }
            osk->num_loaded_key_sets--;
            if (osk->num_loaded_key_sets == 0) {
                free(osk->loaded_key_set_names);
                osk->loaded_key_set_names = NULL;
            } else {
                osk->loaded_key_set_names = realloc(osk->loaded_key_set_names, sizeof(char*) * (size_t)osk->num_loaded_key_sets);
            }
            return;
        }
    }
}

/**
 * @brief Finds the "CONTROL" special key set within the OSK's all_special_sets.
 * @param osk Pointer to the OnScreenKeyboard state.
 * @return Pointer to the "CONTROL" SpecialKeySet, or NULL if not found.
 */
static SpecialKeySet* find_control_set(OnScreenKeyboard* osk)
{
    for (int i = 0; i < osk->num_total_special_sets; ++i) {
        if (osk->all_special_sets[i].name && strcmp(osk->all_special_sets[i].name, "CONTROL") == 0) {
            return &osk->all_special_sets[i];
        }
    }
    return NULL;
}

/**
 * @brief Adds a key set to the list of available dynamic sets if it's not already there.
 * @param osk Pointer to the OnScreenKeyboard state.
 * @param path The file path of the key set.
 * @return True if the set was newly added, false if it was already available.
 */
static bool add_to_available_list(OnScreenKeyboard* osk, const char* path)
{
    // Check if it's already available
    for (int i = 0; i < osk->num_available_dynamic_key_sets; ++i) {
        if (strcmp(osk->available_dynamic_key_sets[i].file_path, path) == 0) {
            return false; // Already available, no change
        }
    }

    // Get basename for the set name
    const char* last_slash = strrchr(path, '/');
    const char* basename = last_slash ? last_slash + 1 : path;
    char* set_name = strdup(basename);
    if (!set_name) {
        fprintf(stderr, "Error: Failed to allocate memory for key set name.\n");
        return false;
    }
    char* dot = strrchr(set_name, '.');
    if (dot && strcmp(dot, ".keys") == 0) {
        *dot = '\0';
    }

    // Add to available_dynamic_key_sets
    SpecialKeySet* temp = realloc(osk->available_dynamic_key_sets, sizeof(SpecialKeySet) * (size_t)(osk->num_available_dynamic_key_sets + 1));
    if (!temp) {
        fprintf(stderr, "Error: Failed to reallocate memory for available_dynamic_key_sets.\n");
        free(set_name);
        return false;
    }
    osk->available_dynamic_key_sets = temp;
    osk->available_dynamic_key_sets[osk->num_available_dynamic_key_sets] = (SpecialKeySet) {
        .name = set_name, .file_path = strdup(path), .keys = NULL, .length = 0, .is_dynamic = true, .active_mod_mask = OSK_MOD_NONE
    };
    osk->num_available_dynamic_key_sets++;

    return true; // It was a new addition
}

/**
 * @brief Rebuilds the dynamic part of the "CONTROL" key set based on loaded/unloaded dynamic key sets.
 * This function is called after any load/unload operation.
 */
static void osk_rebuild_control_set_dynamic_keys(OnScreenKeyboard* osk)
{
    SpecialKeySet* control_set = find_control_set(osk);
    if (!control_set) {
        fprintf(stderr, "Error: 'CONTROL' key set not found. Cannot rebuild dynamic keys.\n");
        return;
    }

    // Free all previously allocated keys and their contents in the CONTROL set.
    free_special_key_set_contents(control_set);

    // Allocate space for combined base keys + dynamic key set entries
    const size_t num_action_keys = sizeof(osk_special_set_action_keys) / sizeof(SpecialKey);
    control_set->keys = malloc(sizeof(SpecialKey) * (num_action_keys + (size_t)osk->num_available_dynamic_key_sets));
    if (!control_set->keys) {
        fprintf(stderr, "Error: Failed to allocate memory for CONTROL set keys.\n");
        return;
    }
    control_set->length = 0;

    // Deep copy base keys from osk_special_set_action_keys
    for (size_t i = 0; i < num_action_keys; ++i) {
        control_set->keys[control_set->length] = osk_special_set_action_keys[i];
        control_set->keys[control_set->length].display_name = strdup(osk_special_set_action_keys[i].display_name);
        control_set->keys[control_set->length].sequence = osk_special_set_action_keys[i].sequence ? strdup(osk_special_set_action_keys[i].sequence) : NULL;
        control_set->length++;
    }

    // Add dynamic key set entries (+Name / -Name)
    for (int i = 0; i < osk->num_available_dynamic_key_sets; ++i) {
        SpecialKey new_key;
        const char* setName = osk->available_dynamic_key_sets[i].name;
        const char* setPath = osk->available_dynamic_key_sets[i].file_path;

        if (is_dynamic_key_set_loaded(osk, setName)) {
            // Key set is loaded, create an UNLOAD_FILE key
            char display_name_buf[256];
            snprintf(display_name_buf, sizeof(display_name_buf), "-%s", setName);
            new_key.display_name = strdup(display_name_buf);
            new_key.type = SK_UNLOAD_FILE;
            new_key.sequence = strdup(setName); // Sequence holds the name to unload
        } else {
            // Key set is not loaded, create a LOAD_FILE key
            char display_name_buf[256];
            snprintf(display_name_buf, sizeof(display_name_buf), "+%s", setName);
            new_key.display_name = strdup(display_name_buf);
            new_key.type = SK_LOAD_FILE;
            new_key.sequence = strdup(setPath); // Sequence holds the path to load
        }
        new_key.keycode = SDLK_UNKNOWN;
        new_key.mod = KMOD_NONE;
        new_key.command = CMD_NONE;

        if (new_key.display_name && new_key.sequence) {
            control_set->keys[control_set->length++] = new_key;
        } else {
            fprintf(stderr, "Error: Failed to create dynamic key for set '%s'.\n", setName);
            free(new_key.display_name);
            free(new_key.sequence); // Safe to call free(NULL)
        }
    }
    osk_invalidate_render_cache(osk); // Invalidate cache to force redraw
}

/**
 * @brief Makes a key set available in the CONTROL menu without loading it.
 * @param osk Pointer to the OnScreenKeyboard state.
 * @param path The file path of the key set.
 */
void osk_make_set_available(OnScreenKeyboard* osk, const char* path)
{
    if (add_to_available_list(osk, path)) {
        // If it was newly added, we need to rebuild the control set
        // to show the new [+Name] key.
        osk_rebuild_control_set_dynamic_keys(osk);
    }
}


/**
 * @brief Initializes the OSK with built-in and custom key sets from config.
 */
void osk_init_all_sets(OnScreenKeyboard* osk)
{
    // Create a single, dynamically-managed "CONTROL" set.
    osk->num_total_special_sets = 1;
    osk->all_special_sets = malloc(sizeof(SpecialKeySet) * (size_t)osk->num_total_special_sets);
    if (!osk->all_special_sets) {
        fprintf(stderr, "Error: Failed to allocate memory for OSK special sets.\n");
        return;
    }

    // Initialize the CONTROL set structure. Its keys will be allocated by the rebuild function.
    SpecialKeySet* control_set = &osk->all_special_sets[0];
    control_set->name = "CONTROL"; // This can be a string literal as it won't be freed.
    control_set->keys = NULL;
    control_set->length = 0;
    control_set->is_dynamic = true; // Mark as dynamic so its contents are freed correctly.
    control_set->file_path = NULL;
    control_set->active_mod_mask = OSK_MOD_NONE;

    // The base keys are now combined from the two original arrays.
    // We will copy them into the dynamic `keys` array in the rebuild function.

    // Rebuild the CONTROL set to include the base keys.
    osk_rebuild_control_set_dynamic_keys(osk);
}

/**
 * @brief Adds a custom key set from a file to the OSK's available special sets.
 */
void osk_add_custom_set(OnScreenKeyboard* osk, const char* path)
{
    add_to_available_list(osk, path); // Ensure it's available.

    FILE* file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open key set file '%s'. Skipping.\n", path);
        return;
    }

    SpecialKeySet new_set;
    new_set.name = NULL;
    new_set.keys = NULL;
    new_set.length = 0;
    new_set.is_dynamic = true;
    new_set.file_path = strdup(path); // Store the path
    new_set.active_mod_mask = OSK_MOD_NONE;

    const char* last_slash = strrchr(path, '/');
    const char* basename = last_slash ? last_slash + 1 : path;
    new_set.name = strdup(basename);
    if (new_set.name) {
        char* dot = strrchr(new_set.name, '.');
        if (dot && strcmp(dot, ".keys") == 0) {
            *dot = '\0';
        }
    } else {
        fprintf(stderr, "Error: Failed to allocate memory for custom key set name.\n");
        fclose(file);
        free(new_set.file_path);
        return;
    }

    // Check if a set with this name is already loaded
    // Start search from 1, as the first built-in set (CONTROL) cannot be replaced by a dynamic one.
    for (int i = 1; i < osk->num_total_special_sets; ++i) {
        if (osk->all_special_sets[i].is_dynamic && osk->all_special_sets[i].name && strcmp(osk->all_special_sets[i].name, new_set.name) == 0) {
            fprintf(stderr, "Info: Key set '%s' is already loaded. Skipping.\n", new_set.name);
            free(new_set.name);
            free(new_set.file_path);
            fclose(file);
            return;
        }
    }


    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        new_set.keys = realloc(new_set.keys, sizeof(SpecialKey) * (size_t)(new_set.length + 1));
        if (!new_set.keys) {
            fprintf(stderr, "Error: Failed to reallocate memory for custom key set keys.\n");
            free_special_key_set_contents(&new_set);
            free(new_set.name);
            free(new_set.file_path);
            fclose(file);
            return;
        }
        if (parse_key_set_line(line, &new_set.keys[new_set.length])) {
            new_set.length++;
        }
    }
    fclose(file);

    if (new_set.length > 0) {
        osk->all_special_sets = realloc(osk->all_special_sets, sizeof(SpecialKeySet) * (size_t)(osk->num_total_special_sets + 1));
        if (!osk->all_special_sets) {
            fprintf(stderr, "Error: Failed to reallocate memory for all special sets.\n");
            free_special_key_set_contents(&new_set);
            free(new_set.name);
            free(new_set.file_path);
            return;
        }
        osk->all_special_sets[osk->num_total_special_sets] = new_set;
        osk->num_total_special_sets++;
        add_loaded_key_set_name(osk, new_set.name); // Add to loaded list
        osk_rebuild_control_set_dynamic_keys(osk); // Rebuild CONTROL set
    } else {
        free(new_set.name);
        // Keys would be freed here if any were allocated, but length is 0
        free(new_set.file_path);
    }
}





/**
 * @brief Removes a custom key set by name from the OSK's available special sets.
 */
void osk_remove_custom_set(OnScreenKeyboard* osk, const char* name)
{
    if (!osk || !osk->all_special_sets || !name) {
        return;
    }

    int found_idx = -1;
    // Start search from 1, as the first built-in set (CONTROL) cannot be removed
    for (int i = 1; i < osk->num_total_special_sets; ++i) {
        if (osk->all_special_sets[i].is_dynamic && osk->all_special_sets[i].name && strcmp(osk->all_special_sets[i].name, name) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx != -1) {
        SpecialKeySet* set_to_remove = &osk->all_special_sets[found_idx];

        free_special_key_set_contents(set_to_remove);
        free(set_to_remove->name);
        free(set_to_remove->file_path); // Free the stored file path

        for (int i = found_idx; i < osk->num_total_special_sets - 1; ++i) {
            osk->all_special_sets[i] = osk->all_special_sets[i + 1];
        }

        osk->num_total_special_sets--;
        if (osk->num_total_special_sets > 0) {
            osk->all_special_sets = realloc(osk->all_special_sets, sizeof(SpecialKeySet) * (size_t)osk->num_total_special_sets);
            if (!osk->all_special_sets) {
                fprintf(stderr, "Warning: Failed to reallocate all_special_sets after removal. Memory leak possible.\n");
            }
        } else {
            free(osk->all_special_sets);
            osk->all_special_sets = NULL;
        }

        remove_loaded_key_set_name(osk, name); // Remove from loaded list
        osk_rebuild_control_set_dynamic_keys(osk); // Rebuild CONTROL set

        if (osk->set_idx >= osk->num_total_special_sets) {
            osk->set_idx = osk->num_total_special_sets > 0 ? osk->num_total_special_sets - 1 : 0;
            osk->char_idx = 0;
            osk_invalidate_render_cache(osk); // Invalidate cache to force redraw
        }
    }
}

/**
 * @brief Frees all memory allocated for dynamic key sets.
 */
void osk_free_all_sets(OnScreenKeyboard* osk)
{
    if (!osk) {
        return;
    }

    // Free keys within the CONTROL set if they were dynamically allocated
    if (osk->all_special_sets && osk->num_total_special_sets > 0) {
        SpecialKeySet* control_set = &osk->all_special_sets[0]; // CONTROL is always first
        if (control_set->is_dynamic && control_set->keys) {
            free_special_key_set_contents(control_set);
        }
    }

    // Free dynamically loaded key sets (those added via osk_add_custom_set)
    // Loop starts from 1 because the CONTROL set (index 0) is handled above.
    for (int i = 1; i < osk->num_total_special_sets; ++i) {
        SpecialKeySet* current_set = &osk->all_special_sets[i];
        // All sets added via osk_add_custom_set are marked as dynamic.
        if (current_set->is_dynamic) {
            free_special_key_set_contents(current_set);
            free(current_set->name);
            free(current_set->file_path);
        }
    }
    free(osk->all_special_sets);
    osk->all_special_sets = NULL;
    osk->num_total_special_sets = 0;

    // Free character layout sets (all 16 possible modifier combinations)
    for (int i = 0; i < 16; ++i) {
        free_char_layout_rows(osk->char_sets_by_modifier[i], osk->num_char_rows_by_modifier[i]);
        osk->char_sets_by_modifier[i] = NULL;
    }
    memset(osk->num_char_rows_by_modifier, 0, sizeof(osk->num_char_rows_by_modifier));


    // Free available_dynamic_key_sets
    if (osk->available_dynamic_key_sets) {
        for (int i = 0; i < osk->num_available_dynamic_key_sets; ++i) {
            free(osk->available_dynamic_key_sets[i].name);
            free(osk->available_dynamic_key_sets[i].file_path);
        }
        free(osk->available_dynamic_key_sets);
        osk->available_dynamic_key_sets = NULL;
        osk->num_available_dynamic_key_sets = 0;
    }

    // Free loaded_key_set_names
    if (osk->loaded_key_set_names) {
        for (int i = 0; i < osk->num_loaded_key_sets; ++i) {
            free(osk->loaded_key_set_names[i]);
        }
        free(osk->loaded_key_set_names);
        osk->loaded_key_set_names = NULL;
        osk->num_loaded_key_sets = 0;
    }
}
