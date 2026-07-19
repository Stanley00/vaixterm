/**
 * @file osk_core.c
 * @brief Core On-Screen Keyboard (OSK) functionality implementation.
 *
 * This module implements the core OSK functionality including state management,
 * layout loading, key set management, and position calculations.
 *
 * @author VaixTerm Team
 * @date 2024
 */

#include "osk_core.h"
#include "osk_parser.h"
#include "error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int get_osk_y_position(const OnScreenKeyboard* osk, const Terminal* term, int win_h, int char_h)
{
    if (!osk || !term || win_h <= 0 || char_h <= 0) {
        ERROR_LOG("Invalid parameters: osk=%p, term=%p, win_h=%d, char_h=%d", 
                  (void*)osk, (void*)term, win_h, char_h);
        return 0;
    }

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

int get_current_num_char_rows(const OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return 0;
    }

    int num_char_rows = osk->num_char_rows;
    int effective_num_rows = num_char_rows;

    // If SHIFT is active, use the shifted layout if it exists
    if (osk->modifier_mask & OSK_MOD_SHIFT && osk->shifted_char_sets) {
        effective_num_rows = osk->num_shifted_rows;
    }

    return effective_num_rows;
}

void osk_validate_row_index(OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return;
    }

    int effective_num_rows = get_current_num_char_rows(osk);
    if (osk->current_char_row >= effective_num_rows) {
        DEBUG_LOG("Clamping current_char_row from %d to %d", osk->current_char_row, effective_num_rows - 1);
        osk->current_char_row = effective_num_rows - 1;
    }
    if (osk->current_char_row < 0) {
        DEBUG_LOG("Clamping current_char_row from %d to 0", osk->current_char_row);
        osk->current_char_row = 0;
    }
}

const SpecialKeySet* osk_get_effective_row_ptr(const OnScreenKeyboard* osk, int set_idx)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return NULL;
    }

    int effective_num_rows = get_current_num_char_rows(osk);
    if (set_idx < 0 || set_idx >= effective_num_rows) {
        ERROR_LOG("Invalid set_idx: %d (effective_num_rows=%d)", set_idx, effective_num_rows);
        return NULL;
    }

    const SpecialKeySet* active_char_set = osk->char_sets;
    if (osk->modifier_mask & OSK_MOD_SHIFT && osk->shifted_char_sets) {
        active_char_set = osk->shifted_char_sets;
    }

    return &active_char_set[set_idx];
}

const SpecialKey* osk_get_effective_char_ptr(const OnScreenKeyboard* osk, int set_idx, int char_idx)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return NULL;
    }

    const SpecialKeySet* row = osk_get_effective_row_ptr(osk, set_idx);
    if (!row) {
        return NULL;
    }

    if (char_idx < 0 || char_idx >= row->num_keys) {
        ERROR_LOG("Invalid char_idx: %d (row->num_keys=%d)", char_idx, row->num_keys);
        return NULL;
    }

    return &row->keys[char_idx];
}

int get_physical_modifier_mask(const OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return 0;
    }

    int mask = 0;
    if (osk->mod_ctrl) mask |= OSK_MOD_CTRL;
    if (osk->mod_alt) mask |= OSK_MOD_ALT;
    if (osk->mod_gui) mask |= OSK_MOD_GUI;
    if (osk->mod_shift) mask |= OSK_MOD_SHIFT;
    return mask;
}

void osk_load_layout(OnScreenKeyboard* osk, const char* path)
{
    if (!osk || !path) {
        ERROR_LOG("Invalid parameters: osk=%p, path=%p", (void*)osk, (void*)path);
        return;
    }

    DEBUG_LOG("Loading OSK layout from: %s", path);

    // Free existing layout
    if (osk->char_sets) {
        free_char_layout_rows(osk->char_sets, osk->num_char_rows);
        osk->char_sets = NULL;
        osk->num_char_rows = 0;
    }
    if (osk->shifted_char_sets) {
        free_char_layout_rows(osk->shifted_char_sets, osk->num_shifted_rows);
        osk->shifted_char_sets = NULL;
        osk->num_shifted_rows = 0;
    }

    // Load new layout
    FILE* file = fopen(path, "r");
    if (!file) {
        ERROR_LOG("Failed to open layout file: %s", path);
        return;
    }

    // Read file content
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = malloc(file_size + 1);
    if (!content) {
        ERROR_LOG("Failed to allocate memory for layout content");
        fclose(file);
        return;
    }

    size_t read_size = fread(content, 1, file_size, file);
    content[read_size] = '\0';
    fclose(file);

    // Parse layout
    SpecialKeySet* temp_key_sets_by_modifier[OSK_NUM_MODIFIERS] = {NULL};
    int temp_num_rows[OSK_NUM_MODIFIERS] = {0};
    int temp_capacity[OSK_NUM_MODIFIERS] = {0};

    if (!parse_layout_content(content, temp_key_sets_by_modifier, temp_num_rows, temp_capacity)) {
        ERROR_LOG("Failed to parse layout content");
        free(content);
        return;
    }

    free(content);

    // Flatten layouts into OSK structure
    osk_flatten_layouts(osk, temp_key_sets_by_modifier, temp_num_rows);

    // Validate current row index
    osk_validate_row_index(osk);

    INFO_LOG("Successfully loaded layout: %s", path);
}

void osk_make_set_available(OnScreenKeyboard* osk, const char* path)
{
    if (!osk || !path) {
        ERROR_LOG("Invalid parameters: osk=%p, path=%p", (void*)osk, (void*)path);
        return;
    }

    add_to_available_list(osk, path);
}

void osk_init_all_sets(OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return;
    }

    DEBUG_LOG("Initializing all OSK key sets");

    // Initialize built-in control set
    if (!osk->control_set.keys) {
        static const SpecialKey osk_special_set_action_keys[] = {
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

        osk->control_set.keys = malloc(sizeof(osk_special_set_action_keys));
        if (osk->control_set.keys) {
            memcpy(osk->control_set.keys, osk_special_set_action_keys, sizeof(osk_special_set_action_keys));
            osk->control_set.num_keys = sizeof(osk_special_set_action_keys) / sizeof(osk_special_set_action_keys[0]);
            for (int i = 0; i < osk->control_set.num_keys; i++) {
                if (osk->control_set.keys[i].display_name)
                    osk->control_set.keys[i].display_name = strdup(osk->control_set.keys[i].display_name);
                if (osk->control_set.keys[i].sequence)
                    osk->control_set.keys[i].sequence = strdup(osk->control_set.keys[i].sequence);
            }
        }
    }

    // Load available key sets
    for (int i = 0; i < osk->num_available_sets; i++) {
        if (osk->available_sets[i]) {
            osk_add_custom_set(osk, osk->available_sets[i]);
        }
    }

    INFO_LOG("OSK key sets initialized");
}

void osk_add_custom_set(OnScreenKeyboard* osk, const char* path)
{
    if (!osk || !path) {
        ERROR_LOG("Invalid parameters: osk=%p, path=%p", (void*)osk, (void*)path);
        return;
    }

    DEBUG_LOG("Adding custom OSK set from: %s", path);

    FILE* file = fopen(path, "r");
    if (!file) {
        ERROR_LOG("Failed to open key set file: %s", path);
        return;
    }

    // Parse key set file
    char line[256];
    SpecialKey key;
    bool found_keys = false;

    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') continue;

        if (parse_key_set_line(line, &key)) {
            osk_rebuild_control_set_dynamic_keys(osk);
            free(key.display_name);
            free(key.sequence);
            found_keys = true;
        }
    }

    fclose(file);

    if (found_keys) {
        INFO_LOG("Successfully added custom key set: %s", path);
    } else {
        WARN_LOG("No valid keys found in: %s", path);
    }
}

void osk_remove_custom_set(OnScreenKeyboard* osk, const char* name)
{
    if (!osk || !name) {
        ERROR_LOG("Invalid parameters: osk=%p, name=%p", (void*)osk, (void*)name);
        return;
    }

    DEBUG_LOG("Removing custom OSK set: %s", name);

    // Remove from loaded sets
    remove_loaded_key_set_name(osk, name);

    // Rebuild control set without this set
    osk_rebuild_control_set_dynamic_keys(osk);

    INFO_LOG("Removed custom key set: %s", name);
}

void osk_free_all_sets(OnScreenKeyboard* osk)
{
    if (!osk) {
        ERROR_LOG("Invalid parameter: osk=%p", (void*)osk);
        return;
    }

    DEBUG_LOG("Freeing all OSK sets");

    // Free character layouts
    if (osk->char_sets) {
        free_char_layout_rows(osk->char_sets, osk->num_char_rows);
        osk->char_sets = NULL;
        osk->num_char_rows = 0;
    }
    if (osk->shifted_char_sets) {
        free_char_layout_rows(osk->shifted_char_sets, osk->num_shifted_rows);
        osk->shifted_char_sets = NULL;
        osk->num_shifted_rows = 0;
    }

    // Free control set
    if (osk->control_set.keys) {
        free_special_key_set_contents(&osk->control_set);
        osk->control_set.keys = NULL;
        osk->control_set.num_keys = 0;
    }

    // Free available sets list
    if (osk->available_sets) {
        for (int i = 0; i < osk->num_available_sets; i++) {
            free(osk->available_sets[i]);
        }
        free(osk->available_sets);
        osk->available_sets = NULL;
        osk->num_available_sets = 0;
    }

    // Free loaded sets list
    if (osk->loaded_key_set_names) {
        for (int i = 0; i < osk->num_loaded_key_sets; i++) {
            free(osk->loaded_key_set_names[i]);
        }
        free(osk->loaded_key_set_names);
        osk->loaded_key_set_names = NULL;
        osk->num_loaded_key_sets = 0;
    }

    INFO_LOG("All OSK sets freed");
}
